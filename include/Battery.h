#pragma once

#include <Arduino.h>
#include <Adafruit_INA260.h>

// Samples bus voltage/current from an INA260 at BATTERY_SAMPLE_INTERVAL_MS
// and reports the average, plus the Teensy's internal temperature, over the
// trailing BATTERY_REPORT_INTERVAL_MS window. Implausible samples (I2C read
// failures) are dropped, low voltage must persist for
// BATTERY_LOW_VOLTAGE_REPORTS consecutive reports before the low-voltage
// callback fires, and a missing INA260 is retried every BATTERY_RETRY_MS.
class Battery {
public:
  typedef void (*ReadingCallback)(float voltage, float current, float temperatureC);
  typedef void (*LowVoltageCallback)();
  typedef void (*EventCallback)(const char *message);

  bool begin();
  void update();
  bool isReady() const;

  void setReadingCallback(ReadingCallback callback);
  void setLowVoltageCallback(LowVoltageCallback callback);
  void setEventCallback(EventCallback callback);

private:
  Adafruit_INA260 _ina260;
  bool _ready = false;

  elapsedMillis _sampleTimer;
  elapsedMillis _reportTimer;
  elapsedMillis _retryTimer;
  float _voltageSum = 0;
  float _currentSum = 0;
  uint16_t _sampleCount = 0;
  uint16_t _invalidCount = 0;
  uint8_t _lowReports = 0;

  ReadingCallback _readingCallback = nullptr;
  LowVoltageCallback _lowVoltageCallback = nullptr;
  EventCallback _eventCallback = nullptr;

  bool connect();
  void sample();
  void report();
  void event(const char *message);
};
