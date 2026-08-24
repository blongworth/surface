#include "Logger.h"
#include "Config.h"
#include "Clock.h"

bool Logger::begin() {
  if (!SD.begin(SD_CHIP_SELECT)) {
    Serial.println("Card failed, or not present");
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
}

void Logger::rotateNow() {
  openNewFiles();
}

void Logger::close() {
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
  return (bool)_landerFile || (bool)_eventFile;
}

void Logger::logLine(MessageDirection direction, const char *line) {
  if (line == nullptr) return;
  log(direction, line, strlen(line));
}

void Logger::log(MessageDirection direction, const char *data, size_t length) {
  if (data == nullptr) return;

  char ts[25];
  timestamp(ts, sizeof(ts));

  if (direction == MessageDirection::FromLander) {
    if (!_landerFile) return;

    // High-rate lander data file: ISO8601 payload
    _landerFile.print(ts);
    _landerFile.print(' ');
    _landerFile.write((const uint8_t *)data, length);
    _landerFile.println();
    return;
  }

  if (!_eventFile) return;

  // Event file: ISO8601 direction payload
  _eventFile.print(ts);
  _eventFile.print(' ');
  _eventFile.print(directionName(direction));
  _eventFile.print(' ');
  _eventFile.write((const uint8_t *)data, length);
  _eventFile.println();
}

void Logger::openNewFiles() {
  close();

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
    writeHeader(_landerFile, "# surface-lander-log-v1", "# fields: iso8601 payload");
  }

  _eventFile = SD.open(_eventFilename, FILE_WRITE);
  if (!_eventFile) {
    Serial.print("Could not create event log: ");
    Serial.println(_eventFilename);
  } else {
    Serial.print("New event log: ");
    Serial.println(_eventFilename);
    writeHeader(_eventFile, "# surface-event-log-v1", "# fields: iso8601 direction payload");
  }

  setNextRotation();
  _flushTimer = 0;
}

void Logger::writeHeader(File &file, const char *header, const char *fields) {
  file.println(header);
  file.println(fields);
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
