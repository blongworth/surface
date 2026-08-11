#include "Telemetry.h"

#if ENABLE_TELEMETRY
static Telemetry *activeTelemetry = nullptr;
#endif

void Telemetry::begin() {
#if ENABLE_TELEMETRY
  pinMode(MIFI_POWER_PIN, OUTPUT);
  pinMode(MIFI_WAKE_PIN, OUTPUT);
  pinMode(GPS_POWER_PIN, OUTPUT);
  pinMode(TELEMETRY_POWER_PIN, OUTPUT);
  digitalWrite(MIFI_POWER_PIN, LOW);
  digitalWrite(MIFI_WAKE_PIN, LOW);
  digitalWrite(GPS_POWER_PIN, LOW);
  digitalWrite(TELEMETRY_POWER_PIN, LOW);

  TELEMETRY_SERIAL.begin(TELEMETRY_BAUD);
  activeTelemetry = this;
  _sendIntervalTimer = TELEMETRY_SEND_INTERVAL_MS; // allow a session as soon as data exists

  _com.setResponseCallback([](const char *response) {
    if (activeTelemetry) {
      activeTelemetry->handleResponse(response);
    }
  });
#endif
}

void Telemetry::update() {
#if ENABLE_TELEMETRY
  _com.update();
  updateStateMachine();
#endif
}

void Telemetry::rememberLanderLine(const char *data, size_t length) {
#if ENABLE_TELEMETRY
  if (data == nullptr || length == 0) return;

  size_t copyLength = min(length, (size_t)TELEMETRY_LINE_BUFFER_SIZE - 1);
  memcpy(_recentLanderLines[_recentLanderNext], data, copyLength);
  _recentLanderLines[_recentLanderNext][copyLength] = '\0';

  _recentLanderNext = (_recentLanderNext + 1) % TELEMETRY_LINES_TO_SEND;
  if (_recentLanderCount < TELEMETRY_LINES_TO_SEND) {
    _recentLanderCount++;
  }
#else
  (void)data;
  (void)length;
#endif
}

void Telemetry::setReceiveCallback(ReceiveCallback callback) {
  _receiveCallback = callback;
}

void Telemetry::setTransmitCallback(TransmitCallback callback) {
  _transmitCallback = callback;
}

bool Telemetry::send(const char *data) {
#if ENABLE_TELEMETRY
  if (data == nullptr || _com.isWaiting()) return false;
  _com.sendCommand(data);
  if (_transmitCallback) {
    _transmitCallback(data);
  }
  return true;
#else
  (void)data;
  return false;
#endif
}

bool Telemetry::isBusy() const {
#if ENABLE_TELEMETRY
  return _com.isWaiting() || _state != State::Off;
#else
  return false;
#endif
}

bool Telemetry::isEnabled() const {
#if ENABLE_TELEMETRY
  return true;
#else
  return false;
#endif
}

#if ENABLE_TELEMETRY
void Telemetry::clearSerial() {
  while (TELEMETRY_SERIAL.available() > 0) {
    TELEMETRY_SERIAL.read();
  }
  TELEMETRY_SERIAL.flush();
}

void Telemetry::startSession() {
  clearSerial();
  _connected = false;
  _batchActive = false;
  _waitingForDataAck = false;
  _lastSendOk = false;
  _retryCount = 0;
  _sessionTimer = 0;
  _pollTimer = TELEMETRY_CONNECT_POLL_MS;
  _shutdownSent = false;
  _state = State::Connecting;
}

void Telemetry::shutdownSession() {
  if (_state == State::ShuttingDown || _state == State::Off) return;

  _connected = false;
  _batchActive = false;
  _waitingForDataAck = false;
  _retryCount = 0;
  _state = State::ShuttingDown;
  _shutdownSent = false;

  if (!_com.isWaiting()) {
    _shutdownSent = send(TELEMETRY_SHUTDOWN_COMMAND);
  }
}

void Telemetry::updateStateMachine() {
  if (_state == State::Off) {
    if (_recentLanderCount > 0 && _sendIntervalTimer >= TELEMETRY_SEND_INTERVAL_MS) {
      _sendIntervalTimer = 0;
      startSession();
    }
    return;
  }

  if (_sessionTimer >= TELEMETRY_SESSION_TIMEOUT_MS) {
    shutdownSession();
  }

  switch (_state) {
    case State::Off:
      break;
    case State::Connecting:
      updateConnecting();
      break;
    case State::SendingBatch:
      updateSender();
      break;
    case State::ShuttingDown:
      if (!_com.isWaiting()) {
        if (!_shutdownSent) {
          _shutdownSent = send(TELEMETRY_SHUTDOWN_COMMAND);
        } else {
          _state = State::Off;
        }
      }
      break;
  }
}

void Telemetry::updateConnecting() {
  if (_connected) {
    startBatch();
    _state = State::SendingBatch;
    return;
  }

  // This mimics the original use of the module: repeatedly send "^" until the
  // module reports connected, or until the session timeout shuts it down.
  if (!_com.isWaiting() && _pollTimer >= TELEMETRY_CONNECT_POLL_MS) {
    _pollTimer = 0;
    send(TELEMETRY_CONNECT_COMMAND);
  }
}

void Telemetry::updateSender() {
  if (!_batchActive) {
    shutdownSession();
    return;
  }

  if (_com.isWaiting()) return;

  if (_waitingForDataAck) {
    if (_lastSendOk) {
      _batchIndex++;
      _retryCount = 0;
    } else if (_retryCount < TELEMETRY_MAX_SEND_RETRIES) {
      _retryCount++;
    } else {
      _batchIndex++;
      _retryCount = 0;
    }
    _waitingForDataAck = false;
  }

  if (_batchIndex >= _batchCount) {
    _batchActive = false;
    shutdownSession();
    return;
  }

  sendCurrentBatchLine();
}

void Telemetry::startBatch() {
  if (_recentLanderCount == 0) {
    _batchActive = false;
    return;
  }

  _batchActive = true;
  _batchIndex = 0;
  _batchCount = _recentLanderCount;
  _retryCount = 0;
  _waitingForDataAck = false;
  _lastSendOk = false;
}

void Telemetry::sendCurrentBatchLine() {
  size_t idx = recentLineIndex(_batchIndex);
  char packet[TELEMETRY_LINE_BUFFER_SIZE + 8];
  snprintf(packet, sizeof(packet), "DATA:%s", _recentLanderLines[idx]);

  if (send(packet)) {
    _waitingForDataAck = true;
    _lastSendOk = false;
  }
}

void Telemetry::handleResponse(const char *response) {
  if (response == nullptr || response[0] == '\0') return;

  if (response[0] == '0') {
    _connected = false;
    _lastSendOk = false;
  } else if (response[0] == '1') {
    _connected = true;
    _lastSendOk = false;
  } else if (response[0] == 'D') {
    _lastSendOk = (response[1] == 'a');
  } else if (response[0] == 'C') {
    // Original module used C-prefixed command responses. Forward the command
    // content to the application if present, otherwise forward the raw response.
    if (_receiveCallback) {
      _receiveCallback(response[1] ? response + 1 : response);
    }
  } else {
    if (_receiveCallback) {
      _receiveCallback(response);
    }
  }
}

size_t Telemetry::recentLineIndex(size_t chronologicalOffset) const {
  size_t oldest = 0;
  if (_recentLanderCount == TELEMETRY_LINES_TO_SEND) {
    oldest = _recentLanderNext;
  }
  return (oldest + chronologicalOffset) % TELEMETRY_LINES_TO_SEND;
}
#endif
