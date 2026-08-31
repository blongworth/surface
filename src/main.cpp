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

const char compileTime[] = "Compiled on " __DATE__ " " __TIME__;

Logger logger;
LanderUdp lander;
Console console;
Telemetry telemetry;
Battery battery;
ShutdownSequence shutdownSequence;
Flasher flasher(LED_PIN, 100, 1900);

static bool sdReady = false;

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

static void sendLanderCommand(const char *command) {
  lander.sendLine(command);
}

static void executeCommand(const char *line) {
  if (line == nullptr || line[0] == '\0') return;

  // Yes: a simple and practical rule is to handle known local commands here and
  // relay everything else to the lander. Keep local command names documented and
  // avoid choosing names that are valid lander commands.
  if (strcmp(line, "help") == 0) {
    console.printHelp();
  } else if (strcmp(line, "rotate-log") == 0 || strcmp(line, "start-log") == 0) {
    recordLine(MessageDirection::System, "opening new log files");
    logger.rotateNow();
  } else if (strcmp(line, "close-log") == 0) {
    recordLine(MessageDirection::System, "closing log file");
    logger.close();
  } else if (strcmp(line, "status") == 0) {
    lander.requestStatus();
  } else if (strcmp(line, "time-sync") == 0) {
    lander.sendTime(now());
  } else if (strcmp(line, "mtp-reset") == 0) {
    MTP.send_DeviceResetEvent();
  } else {
    sendLanderCommand(line);
  }
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

static void handleBatteryLow() {
  if (shutdownSequence.isActive()) return;

  recordLine(MessageDirection::System, "battery voltage below threshold; sending OFF to lander");
  shutdownSequence.start();
}

static void handleShutdownComplete() {
  recordLine(MessageDirection::System, "low voltage shutdown confirmed by lander; closing log files");
  logger.close();
}

void setup() {
  console.begin();
  Serial.println();
  Serial.print("Surface Controller ");
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

  sdReady = logger.begin();
  if (sdReady) {
    MTP.addFilesystem(SD, "SD Card");
  }

  lander.setReceiveCallback(handleLanderReceive);
  lander.setTransmitCallback(handleLanderTransmit);
  lander.begin();

  telemetry.setReceiveCallback(handleTelemetryReceive);
  telemetry.setTransmitCallback(handleTelemetryTransmit);
  telemetry.begin();

  battery.setReadingCallback(handleBatteryReading);
  battery.setLowVoltageCallback(handleBatteryLow);
  battery.begin();

  shutdownSequence.setSendCallback(sendLanderCommand);
  shutdownSequence.setCompleteCallback(handleShutdownComplete);

  console.setLineCallback(handleConsoleCommand);
  flasher.begin();

  recordLine(MessageDirection::System, "startup complete");
  lander.sendTime(now());
  lander.requestStatus();
  console.printHelp();
}

void loop() {
  console.update();
  lander.update();
  logger.update();
  telemetry.update();
  battery.update();
  shutdownSequence.update();
  flasher.run();

  // Let a host browse the SD card when the log is explicitly closed.
  if (sdReady && !logger.isOpen()) {
    MTP.loop();
  }
}
