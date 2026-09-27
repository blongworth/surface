#include "ShutdownSequence.h"
#include "Config.h"

// True if line is expected, ignoring surrounding whitespace (e.g. a stray
// '\n' from a CRLF-terminated lander message).
static bool lineMatches(const char *line, const char *expected) {
  while (isspace((unsigned char)*line)) line++;
  size_t n = strlen(expected);
  if (strncmp(line, expected, n) != 0) return false;
  for (line += n; *line; line++) {
    if (!isspace((unsigned char)*line)) return false;
  }
  return true;
}

void ShutdownSequence::setSendCallback(SendCallback callback) {
  _sendCallback = callback;
}

void ShutdownSequence::setCompleteCallback(CompleteCallback callback) {
  _completeCallback = callback;
}

void ShutdownSequence::setEventCallback(EventCallback callback) {
  _eventCallback = callback;
}

void ShutdownSequence::start() {
  if (_state != State::Idle) return;

  _state = State::WaitingForAck;
  sendOff();
}

void ShutdownSequence::update() {
  if (_state == State::WaitingForAck && _retryTimer >= LANDER_OFF_RETRY_MS) {
    sendOff();
  } else if (_state == State::WaitingForDone && _retryTimer >= LANDER_OFF_DONE_TIMEOUT_MS) {
    event("lander did not confirm shutdown; resending OFF");
    _state = State::WaitingForAck;
    sendOff();
  }
}

void ShutdownSequence::handleLanderLine(const char *line) {
  if (line == nullptr) return;

  if (_state == State::WaitingForAck && lineMatches(line, LANDER_OFF_ACK)) {
    _state = State::WaitingForDone;
    _retryTimer = 0;
    return;
  }

  // Accept DONE while still waiting for ACK too: over UDP the ACK may have
  // been lost even though the lander powered down.
  if ((_state == State::WaitingForAck || _state == State::WaitingForDone) &&
      lineMatches(line, LANDER_OFF_DONE)) {
    _state = State::ShutDown;
    if (_completeCallback) {
      _completeCallback();
    }
  }
}

void ShutdownSequence::clearShutdown() {
  _state = State::Idle;
}

bool ShutdownSequence::isActive() const {
  return _state != State::Idle;
}

bool ShutdownSequence::isShutDown() const {
  return _state == State::ShutDown;
}

void ShutdownSequence::sendOff() {
  _retryTimer = 0;
  if (_sendCallback) {
    _sendCallback(LANDER_POWER_OFF_COMMAND);
  }
}

void ShutdownSequence::event(const char *message) {
  if (_eventCallback) {
    _eventCallback(message);
  }
}
