#pragma once

#include <Arduino.h>

// Drives the low-voltage shutdown handshake with the lander: sends the OFF
// command, resends it on a timer until the lander ACKs, then waits for the
// lander to report it has finished powering down. Once the lander confirms,
// the sequence latches in the ShutDown state: no further OFF commands are
// sent and the shutdown cannot be re-triggered until clearShutdown() is
// called from a manual restart.
class ShutdownSequence {
public:
  typedef void (*SendCallback)(const char *command);
  typedef void (*CompleteCallback)();

  void setSendCallback(SendCallback callback);
  void setCompleteCallback(CompleteCallback callback);

  void start();
  void update();
  void handleLanderLine(const char *line);
  void clearShutdown();

  bool isActive() const;
  bool isShutDown() const;

private:
  enum class State { Idle, WaitingForAck, WaitingForDone, ShutDown };

  State _state = State::Idle;
  elapsedMillis _retryTimer;
  SendCallback _sendCallback = nullptr;
  CompleteCallback _completeCallback = nullptr;

  void sendOff();
};
