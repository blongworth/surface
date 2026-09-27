#pragma once

#include <Arduino.h>

// Drives the low-voltage shutdown handshake with the lander: sends the OFF
// command, resends it on a timer until the lander ACKs, then waits for the
// lander to report it has finished powering down. A DONE is accepted even if
// the ACK was lost, and if DONE never arrives the OFF is resent. Once the
// lander confirms, the sequence latches in the ShutDown state: no further OFF
// commands are sent and the shutdown cannot be re-triggered until
// clearShutdown() is called from a manual restart.
class ShutdownSequence {
public:
  typedef void (*SendCallback)(const char *command);
  typedef void (*CompleteCallback)();
  typedef void (*EventCallback)(const char *message);

  void setSendCallback(SendCallback callback);
  void setCompleteCallback(CompleteCallback callback);
  void setEventCallback(EventCallback callback);

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
  EventCallback _eventCallback = nullptr;

  void sendOff();
  void event(const char *message);
};
