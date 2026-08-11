#pragma once

#include <Arduino.h>
#include "Config.h"

class Console {
public:
  typedef void (*LineCallback)(const char *line);

  void begin();
  void update();
  void setLineCallback(LineCallback callback);
  void printHelp();

private:
  char _buffer[SERIAL_COMMAND_BUFFER_SIZE] = {0};
  size_t _index = 0;
  LineCallback _lineCallback = nullptr;

  void finishLine();
};
