#pragma once

#include <Arduino.h>
#include <Adafruit_INA260.h>

// Samples bus voltage/current from an INA260 at BATTERY_SAMPLE_INTERVAL_MS
// and reports the average over the trailing BATTERY_REPORT_INTERVAL_MS window.
class Battery {
public:
  typedef void (*ReadingCallback)(float voltage, float current);
  typedef void (*LowVoltageCallback)();

  bool begin();
  void update();

  void setReadingCallback(ReadingCallback callback);
  void setLowVoltageCallback(LowVoltageCallback callback);

private:
  Adafruit_INA260 _ina260;
  bool _ready = false;

  elapsedMillis _sampleTimer;
  elapsedMillis _reportTimer;
  float _voltageSum = 0;
  float _currentSum = 0;
  uint16_t _sampleCount = 0;

  ReadingCallback _readingCallback = nullptr;
  LowVoltageCallback _lowVoltageCallback = nullptr;

  void sample();
  void report();
};
