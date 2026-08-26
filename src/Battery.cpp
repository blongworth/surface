#include "Battery.h"
#include "Config.h"

bool Battery::begin() {
  _ready = _ina260.begin();
  if (!_ready) {
    Serial.println("INA260 not found");
    return false;
  }

  Serial.println("INA260 initialized");
  _sampleTimer = 0;
  _reportTimer = 0;
  return true;
}

void Battery::update() {
  if (!_ready) return;

  if (_sampleTimer >= BATTERY_SAMPLE_INTERVAL_MS) {
    _sampleTimer = 0;
    sample();
  }

  if (_reportTimer >= BATTERY_REPORT_INTERVAL_MS) {
    _reportTimer = 0;
    report();
  }
}

void Battery::setReadingCallback(ReadingCallback callback) {
  _readingCallback = callback;
}

void Battery::setLowVoltageCallback(LowVoltageCallback callback) {
  _lowVoltageCallback = callback;
}

void Battery::sample() {
  float voltage = _ina260.readBusVoltage() / 1000.0f; // mV -> V
  float current = _ina260.readCurrent() / 1000.0f;     // mA -> A

  _voltageSum += voltage;
  _currentSum += current;
  _sampleCount++;
}

void Battery::report() {
  if (_sampleCount == 0) return;

  float avgVoltage = _voltageSum / _sampleCount;
  float avgCurrent = _currentSum / _sampleCount;

  _voltageSum = 0;
  _currentSum = 0;
  _sampleCount = 0;

  if (_readingCallback) {
    _readingCallback(avgVoltage, avgCurrent);
  }

  if (avgVoltage < BATTERY_LOW_VOLTAGE_THRESHOLD && _lowVoltageCallback) {
    _lowVoltageCallback();
  }
}
