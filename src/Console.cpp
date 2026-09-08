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

void Console::printHelp() {
  Serial.println("Local commands:");
  Serial.println("  help           show this help");
  Serial.println("  restart        open new SD log files, clear low voltage shutdown");
  Serial.println("  start-log      alias for restart");
  Serial.println("  close-log      close current SD log files");
  Serial.println("  status         request lander status");
  Serial.println("  time-sync      send current surface time to lander");
  Serial.println("  mtp-reset      send MTP device reset event");
  Serial.println("Any other line is relayed directly to the lander.");
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
