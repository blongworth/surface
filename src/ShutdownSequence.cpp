#include "ShutdownSequence.h"
#include "Config.h"

void ShutdownSequence::setSendCallback(SendCallback callback) {
  _sendCallback = callback;
}

void ShutdownSequence::setCompleteCallback(CompleteCallback callback) {
  _completeCallback = callback;
}

void ShutdownSequence::start() {
  if (_state != State::Idle) return;

  _state = State::WaitingForAck;
  sendOff();
}

void ShutdownSequence::update() {
  if (_state != State::WaitingForAck) return;

  if (_retryTimer >= LANDER_OFF_RETRY_MS) {
    sendOff();
  }
}

void ShutdownSequence::handleLanderLine(const char *line) {
  if (line == nullptr) return;

  if (_state == State::WaitingForAck && strcmp(line, LANDER_OFF_ACK) == 0) {
    _state = State::WaitingForDone;
    return;
  }

  if (_state == State::WaitingForDone && strcmp(line, LANDER_OFF_DONE) == 0) {
    _state = State::Idle;
    if (_completeCallback) {
      _completeCallback();
    }
  }
}

bool ShutdownSequence::isActive() const {
  return _state != State::Idle;
}

void ShutdownSequence::sendOff() {
  _retryTimer = 0;
  if (_sendCallback) {
    _sendCallback(LANDER_POWER_OFF_COMMAND);
  }
}
