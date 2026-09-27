#include <Arduino.h>
#include <TimeLib.h>
#include <MTP_Teensy.h>
#include <Flasher.h>

#include "Battery.h"
#include "Clock.h"
#include "Config.h"
#include "Console.h"
#include "LanderUdp.h"
#include "Logger.h"
#include "Message.h"
#include "ShutdownSequence.h"
#include "Telemetry.h"

const char firmwareVersion[] = FIRMWARE_VERSION;
const char compileTime[] = "Compiled on " __DATE__ " " __TIME__;

Logger logger;
LanderUdp lander;
Console console;
Telemetry telemetry;
Battery battery;
ShutdownSequence shutdownSequence;
Flasher flasher(LED_PIN, LED_NORMAL_ON_MS, LED_NORMAL_OFF_MS);

static bool sdRegistered = false;
static bool sdWasHealthy = false;

static time_t getTeensyTime() {
  return Teensy3Clock.get();
}

static void recordCommunication(MessageDirection direction, const char *data, size_t length) {
  if (data == nullptr) return;

  Serial.print(directionName(direction));
  Serial.print(": ");
  Serial.write((const uint8_t *)data, length);
  Serial.println();

  logger.log(direction, data, length);
}

static void recordLine(MessageDirection direction, const char *line) {
  recordCommunication(direction, line, strlen(line));
}

// Adapter: ShutdownSequence's send callback returns void.
static void sendLanderCommand(const char *command) {
  lander.send(command);
}

// Local console/telemetry commands. Anything not listed here is relayed to the
// lander, so avoid names that are valid lander commands.
struct LocalCommand {
  const char *name;
  const char *help;
  void (*run)();
};

static void printHelp();

static void restartLogging() {
  // Opening logs is what "running" means here, so this also clears a
  // latched low-voltage shutdown.
  const bool wasShutDown = shutdownSequence.isShutDown();
  shutdownSequence.clearShutdown();
  logger.rotateNow();
  recordLine(MessageDirection::System, wasShutDown
                                           ? "manual restart after low voltage shutdown"
                                           : "opening new log files");
}

static void closeLogging() {
  recordLine(MessageDirection::System, "closing log file");
  logger.close();
}

static void requestLanderStatus() { lander.requestStatus(); }
static void syncLanderTime() { lander.sendTime(now()); }
static void resetMtp() { MTP.send_DeviceResetEvent(); }

static const LocalCommand localCommands[] = {
  {"help", "show this help", printHelp},
  {"restart", "open new SD log files, clear low voltage shutdown", restartLogging},
  {"start-log", "alias for restart", restartLogging},
  {"close-log", "close current SD log files", closeLogging},
  {"status", "request lander status", requestLanderStatus},
  {"time-sync", "send current surface time to lander", syncLanderTime},
  {"mtp-reset", "send MTP device reset event", resetMtp},
};

static void printHelp() {
  Serial.println("Local commands:");
  for (const LocalCommand &command : localCommands) {
    Serial.printf("  %-14s %s\n", command.name, command.help);
  }
  Serial.println("Any other line is relayed directly to the lander.");
}

static void executeCommand(const char *line) {
  if (line == nullptr || line[0] == '\0') return;

  for (const LocalCommand &command : localCommands) {
    if (strcmp(line, command.name) == 0) {
      command.run();
      return;
    }
  }

  lander.send(line);
}

static void handleLanderReceive(const char *data, size_t length) {
  recordCommunication(MessageDirection::FromLander, data, length);

  telemetry.rememberLanderLine(data, length);
  shutdownSequence.handleLanderLine(data);

  if (length > 1 && data[0] == '?') {
    Serial.print("Lander status: ");
    Serial.println(lander.status());
  }
}

static void handleLanderTransmit(const char *data, size_t length) {
  recordCommunication(MessageDirection::ToLander, data, length);
}

static void handleLanderEvent(const char *message) {
  recordLine(MessageDirection::System, message);
}

static void handleConsoleCommand(const char *line) {
  recordLine(MessageDirection::FromConsole, line);
  executeCommand(line);
}

static void handleTelemetryReceive(const char *data) {
  recordLine(MessageDirection::FromTelemetry, data);
  executeCommand(data);
}

static void handleTelemetryTransmit(const char *data) {
  recordLine(MessageDirection::ToTelemetry, data);
}

static void handleBatteryReading(float voltage, float current, float temperatureC) {
  char line[64];
  snprintf(line, sizeof(line), "battery voltage=%.2fV current=%.3fA temp=%.1fC", voltage, current, temperatureC);
  recordLine(MessageDirection::System, line);
}

static void handleBatteryEvent(const char *message) {
  recordLine(MessageDirection::System, message);
}

static void handleBatteryLow() {
  // Latched: once the lander has confirmed shutdown, stay off until the
  // operator issues a manual restart.
  if (shutdownSequence.isActive()) return;

  recordLine(MessageDirection::System, "battery voltage below threshold; sending OFF to lander");
  shutdownSequence.start();
}

static void handleShutdownEvent(const char *message) {
  recordLine(MessageDirection::System, message);
}

static void handleShutdownComplete() {
  recordLine(MessageDirection::System, "low voltage shutdown confirmed by lander; closing log files");
  logger.close();
}

// Fast flash when logging was asked for but the SD card can't be written, the
// battery monitor is missing, or a low-voltage shutdown is in progress or
// latched. A deliberate close-log is not an error.
static void updateStatusLed() {
  const bool sdFault = logger.isLoggingWanted() && !logger.isHealthy();
  const bool error = sdFault || !battery.isReady() || shutdownSequence.isActive();
  if (error) {
    flasher.update(LED_ERROR_ON_MS, LED_ERROR_OFF_MS);
  } else {
    flasher.update(LED_NORMAL_ON_MS, LED_NORMAL_OFF_MS);
  }
}

void setup() {
  // Solid on while starting up; loop() takes over with the status pattern.
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  console.begin();
  Serial.println();
  Serial.print("Surface Controller v");
  Serial.print(firmwareVersion);
  Serial.print(" ");
  Serial.println(compileTime);

  if (CrashReport) {
    Serial.print(CrashReport);
    delay(5000);
  }

  MTP.begin();

  setSyncProvider(getTeensyTime);
  if (!rtcTimeIsValid()) {
    Serial.println("Unable to sync with the RTC, or RTC time is implausible");
  } else {
    Serial.println("RTC has set the system time");
  }

  logger.begin();

  battery.setReadingCallback(handleBatteryReading);
  battery.setLowVoltageCallback(handleBatteryLow);
  battery.setEventCallback(handleBatteryEvent);
  battery.begin();

  lander.setReceiveCallback(handleLanderReceive);
  lander.setTransmitCallback(handleLanderTransmit);
  lander.setEventCallback(handleLanderEvent);
  lander.begin();

  telemetry.setReceiveCallback(handleTelemetryReceive);
  telemetry.setTransmitCallback(handleTelemetryTransmit);
  telemetry.begin();

  shutdownSequence.setSendCallback(sendLanderCommand);
  shutdownSequence.setCompleteCallback(handleShutdownComplete);
  shutdownSequence.setEventCallback(handleShutdownEvent);

  console.setLineCallback(handleConsoleCommand);

  // Time sync and status probe go out once the link is up and the lander
  // replies; see LanderUdp.
  recordLine(MessageDirection::System, "startup complete");
  printHelp();

  flasher.begin();
}

void loop() {
  console.update();
  lander.update();
  logger.update();
  telemetry.update();
  battery.update();
  shutdownSequence.update();
  updateStatusLed();
  flasher.run();

  // Register the card with MTP on the first healthy tick, whether that is at
  // boot or after a card is hot-inserted later. On later recoveries (e.g. a
  // card swap re-ran SD.begin()) tell the host to re-read the card.
  const bool sdHealthy = logger.isHealthy();
  if (sdHealthy && !sdWasHealthy) {
    if (!sdRegistered) {
      MTP.addFilesystem(SD, "SD Card");
      sdRegistered = true;
    }
    MTP.send_DeviceResetEvent();
  }
  sdWasHealthy = sdHealthy;

  // Let a host browse the SD card only after an explicit close-log. An SD
  // fault also leaves the files closed, but the logger may re-init the card
  // at any moment, so MTP stays off then.
  if (sdRegistered && !logger.isLoggingWanted()) {
    MTP.loop();
  }
}
