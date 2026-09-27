#include "Battery.h"
#include "Config.h"
#include <InternalTemperature.h>
#include <Wire.h>

bool Battery::begin() {
  InternalTemperature.begin(TEMPERATURE_MAX_ACCURACY);
  Wire.begin();

  _ready = _ina260.begin();
  _retryTimer = 0;
  if (!_ready) {
    event("INA260 not found; battery monitoring and low-voltage shutdown disabled until it responds");
    return false;
  }

  event("INA260 initialized");
  _sampleTimer = 0;
  _reportTimer = 0;
  return true;
}

void Battery::update() {
  if (!_ready) {
    if (_retryTimer >= BATTERY_RETRY_MS) {
      _retryTimer = 0;
      if (connect()) {
        event("INA260 found; battery monitoring enabled");
      }
    }
    return;
  }

  if (_sampleTimer >= BATTERY_SAMPLE_INTERVAL_MS) {
    _sampleTimer = 0;
    sample();
  }

  if (_reportTimer >= BATTERY_REPORT_INTERVAL_MS) {
    _reportTimer = 0;
    report();
  }
}

bool Battery::isReady() const {
  return _ready;
}

void Battery::setReadingCallback(ReadingCallback callback) {
  _readingCallback = callback;
}

void Battery::setLowVoltageCallback(LowVoltageCallback callback) {
  _lowVoltageCallback = callback;
}

void Battery::setEventCallback(EventCallback callback) {
  _eventCallback = callback;
}

// Adafruit_INA260::begin() allocates on every call, so only call it once the
// chip acknowledges its address; otherwise each retry would leak heap.
bool Battery::connect() {
  Wire.beginTransmission(INA260_I2CADDR_DEFAULT);
  if (Wire.endTransmission() != 0) return false;

  _ready = _ina260.begin();
  if (_ready) {
    _sampleTimer = 0;
    _reportTimer = 0;
  }
  return _ready;
}

void Battery::sample() {
  float voltage = _ina260.readBusVoltage() / 1000.0f; // mV -> V
  float current = _ina260.readCurrent() / 1000.0f;     // mA -> A

  // A failed I2C read comes back as 0xFFFFFFFF (~5e6 V); one such sample would
  // swamp the average and hide a real low voltage.
  if (isnan(voltage) || voltage < 0.0f || voltage > BATTERY_MAX_VALID_VOLTAGE) {
    _invalidCount++;
    return;
  }

  _voltageSum += voltage;
  _currentSum += current;
  _sampleCount++;
}

void Battery::report() {
  if (_invalidCount > 0) {
    char line[64];
    snprintf(line, sizeof(line), "INA260 read failed; dropped %u of %u samples",
             _invalidCount, _invalidCount + _sampleCount);
    event(line);
    _invalidCount = 0;
  }

  if (_sampleCount == 0) return;

  float avgVoltage = _voltageSum / _sampleCount;
  float avgCurrent = _currentSum / _sampleCount;

  _voltageSum = 0;
  _currentSum = 0;
  _sampleCount = 0;

  float temperatureC = InternalTemperature.readTemperatureC();

  if (_readingCallback) {
    _readingCallback(avgVoltage, avgCurrent, temperatureC);
  }

  // Require a sustained low reading so a brief load sag doesn't latch a
  // lander shutdown.
  if (avgVoltage >= BATTERY_LOW_VOLTAGE_THRESHOLD) {
    _lowReports = 0;
    return;
  }

  if (_lowReports < BATTERY_LOW_VOLTAGE_REPORTS) _lowReports++;
  if (_lowReports >= BATTERY_LOW_VOLTAGE_REPORTS && _lowVoltageCallback) {
    _lowVoltageCallback();
  }
}

void Battery::event(const char *message) {
  if (_eventCallback) {
    _eventCallback(message);
  } else {
    Serial.println(message);
  }
}
