#include "Console.h"

void Console::begin() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial && millis() < 3000) {
    // Wait briefly for USB serial, but do not block forever in the field.
  }
}

void Console::update() {
  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\r' || c == '\n') {
      finishLine();
      continue;
    }

    if (c == '\b' || c == 127) {
      if (_index > 0) _index--;
      _buffer[_index] = '\0';
      continue;
    }

    if (_index < sizeof(_buffer) - 1) {
      _buffer[_index++] = c;
      _buffer[_index] = '\0';
    }
  }
}

void Console::setLineCallback(LineCallback callback) {
  _lineCallback = callback;
}

void Console::finishLine() {
  if (_index == 0) return;
  _buffer[_index] = '\0';

  if (_lineCallback) {
    _lineCallback(_buffer);
  }

  _index = 0;
  _buffer[0] = '\0';
}
