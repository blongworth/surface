#pragma once

#include <Arduino.h>

// Drives the low-voltage shutdown handshake with the lander: sends the OFF
// command, resends it on a timer until the lander ACKs, then waits for the
// lander to report it has finished powering down.
class ShutdownSequence {
public:
  typedef void (*SendCallback)(const char *command);
  typedef void (*CompleteCallback)();

  void setSendCallback(SendCallback callback);
  void setCompleteCallback(CompleteCallback callback);

  void start();
  void update();
  void handleLanderLine(const char *line);

  bool isActive() const;

private:
  enum class State { Idle, WaitingForAck, WaitingForDone };

  State _state = State::Idle;
  elapsedMillis _retryTimer;
  SendCallback _sendCallback = nullptr;
  CompleteCallback _completeCallback = nullptr;

  void sendOff();
};
