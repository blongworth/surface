#include "Logger.h"
#include "Config.h"
#include "Clock.h"

bool Logger::begin() {
  if (!SD.begin(SD_CHIP_SELECT)) {
    Serial.println("Card failed, or not present");
    // Logging was asked for even though the card is not usable; the periodic
    // fault service will warn and retry until a card shows up.
    _wanted = true;
    markFault("SD card missing or failed to initialize");
    return false;
  }

  Serial.println("SD card initialized");
  openNewFiles();
  return isOpen();
}

void Logger::update() {
  if (_nextRotation != 0 && now() >= _nextRotation) {
    rotateNow();
  }

  if (_flushTimer >= LOG_FLUSH_INTERVAL_MS) {
    if (_landerFile) _landerFile.flush();
    if (_eventFile) _eventFile.flush();
    _flushTimer = 0;
  }

  serviceFault();
}

void Logger::rotateNow() {
  openNewFiles();
}

void Logger::close() {
  // Disarm rotation so a closed logger stays closed until something
  // explicitly reopens the files.
  _nextRotation = 0;

  // A deliberate close is not a fault.
  _wanted = false;
  _healthy = false;
  _faultReason = nullptr;

  if (_landerFile) {
    _landerFile.flush();
    _landerFile.close();
  }

  if (_eventFile) {
    _eventFile.flush();
    _eventFile.close();
  }
}

bool Logger::isOpen() {
  return (bool)_landerFile && (bool)_eventFile;
}

bool Logger::isHealthy() {
  return _healthy;
}

bool Logger::isLoggingWanted() {
  return _wanted;
}

void Logger::logLine(MessageDirection direction, const char *line) {
  if (line == nullptr) return;
  log(direction, line, strlen(line));
}

void Logger::log(MessageDirection direction, const char *data, size_t length) {
  if (data == nullptr) return;

  char ts[25];
  timestamp(ts, sizeof(ts));

  // The File wrapper returns 0 from write() once the card is gone but does not
  // set a write error, so compare byte counts to catch a card pulled mid-run.
  size_t written = 0;
  size_t expected = 0;

  if (direction == MessageDirection::FromLander) {
    if (!_landerFile) return;

    // High-rate lander data file: ISO8601 payload
    written += _landerFile.print(ts);
    written += _landerFile.print(' ');
    written += _landerFile.write((const uint8_t *)data, length);
    written += _landerFile.println();
    expected = strlen(ts) + 1 + length + 2;
  } else {
    if (!_eventFile) return;

    const char *name = directionName(direction);

    // Event file: ISO8601 direction payload
    written += _eventFile.print(ts);
    written += _eventFile.print(' ');
    written += _eventFile.print(name);
    written += _eventFile.print(' ');
    written += _eventFile.write((const uint8_t *)data, length);
    written += _eventFile.println();
    expected = strlen(ts) + 1 + strlen(name) + 1 + length + 2;
  }

  if (written != expected) {
    markFault("SD write failed");
  }
}

void Logger::openNewFiles() {
  close();

  _wanted = true;

  snprintf(_landerFilename, sizeof(_landerFilename),
           "surface_%04d-%02d-%02d-%02d-%02d_lander.log",
           year(), month(), day(), hour(), minute());
  snprintf(_eventFilename, sizeof(_eventFilename),
           "surface_%04d-%02d-%02d-%02d-%02d_events.log",
           year(), month(), day(), hour(), minute());

  _landerFile = SD.open(_landerFilename, FILE_WRITE);
  if (!_landerFile) {
    Serial.print("Could not create lander log: ");
    Serial.println(_landerFilename);
  } else {
    Serial.print("New lander log: ");
    Serial.println(_landerFilename);
  }

  _eventFile = SD.open(_eventFilename, FILE_WRITE);
  if (!_eventFile) {
    Serial.print("Could not create event log: ");
    Serial.println(_eventFilename);
  } else {
    Serial.print("New event log: ");
    Serial.println(_eventFilename);
  }

  // Both files must be open: half-open logging is a fault, not a degraded
  // success.
  if (!isOpen()) {
    markFault("could not create log files");
    return;
  }

  _healthy = true;

  writeHeader(_landerFile, "# surface-lander-log-v1", "# fields: iso8601 payload");
  writeHeader(_eventFile, "# surface-event-log-v1", "# fields: iso8601 direction payload");

  setNextRotation();
  _flushTimer = 0;
}

void Logger::writeHeader(File &file, const char *header, const char *fields) {
  size_t written = file.println(header);
  written += file.println(fields);

  if (written != strlen(header) + 2 + strlen(fields) + 2) {
    markFault("SD write failed");
  }
}

void Logger::markFault(const char *reason) {
  _healthy = false;
  _faultReason = reason;

  // Drop the handles so later writes short-circuit on the !_file guard
  // instead of repeating the failure.
  if (_landerFile) _landerFile.close();
  if (_eventFile) _eventFile.close();

  // Start the interval now so the first warning lands a full interval after
  // the fault rather than immediately.
  _faultTimer = 0;
}

void Logger::serviceFault() {
  if (!_wanted || _healthy) return;
  if (_faultTimer < SD_FAULT_WARN_INTERVAL_MS) return;
  _faultTimer = 0;

  // Straight to Serial: the normal record path writes through log(), which is
  // exactly what is broken here.
  Serial.print("WARNING: ");
  Serial.print(_faultReason != nullptr ? _faultReason : "SD logging unavailable");
  Serial.println("; logging is stopped");

  // The Teensy 4.1 SDIO socket is hot-swappable, so retry on the same tick.
  if (!SD.mediaPresent()) return;

  if (!SD.begin(SD_CHIP_SELECT)) {
    Serial.println("SD card present but initialization failed");
    return;
  }

  Serial.println("SD card detected; restarting logging");
  openNewFiles();
}

void Logger::timestamp(char *buffer, size_t bufferSize) {
  snprintf(buffer, bufferSize, "%04d-%02d-%02dT%02d:%02d:%02dZ",
           year(), month(), day(), hour(), minute(), second());
}

void Logger::setNextRotation() {
  time_t current = now();
  const time_t interval = LOG_ROTATE_HOURS * SECS_PER_HOUR;

  if (!rtcTimeIsValid()) {
    // RTC not set (or set to an implausible value); fall back to a relative
    // interval instead of wall-clock alignment.
    _nextRotation = current + interval;
    return;
  }

  _nextRotation = current - (current % interval) + interval;
}
