#pragma once

#include <Arduino.h>
#include <SD.h>
#include <TimeLib.h>
#include "Message.h"

class Logger {
public:
  bool begin();
  void update();
  void rotateNow();
  void close();
  bool isOpen();

  void log(MessageDirection direction, const char *data, size_t length);
  void logLine(MessageDirection direction, const char *line);

private:
  File _landerFile;
  File _eventFile;
  char _landerFilename[64] = {0};
  char _eventFilename[64] = {0};
  time_t _nextRotation = 0;
  elapsedMillis _flushTimer;

  void openNewFiles();
  void writeHeader(File &file, const char *header, const char *fields);
  void timestamp(char *buffer, size_t bufferSize);
  void setNextRotation();
};
