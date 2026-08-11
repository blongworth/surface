#pragma once

#include <Arduino.h>
#include "Config.h"

#if ENABLE_TELEMETRY
#include <GSMCom.h>
#endif

class Telemetry {
public:
  typedef void (*ReceiveCallback)(const char *data);
  typedef void (*TransmitCallback)(const char *data);

  void begin();
  void update();
  void rememberLanderLine(const char *data, size_t length);
  void setReceiveCallback(ReceiveCallback callback);
  void setTransmitCallback(TransmitCallback callback);
  bool send(const char *data);
  bool isBusy() const;
  bool isEnabled() const;

private:
#if ENABLE_TELEMETRY
  enum class State {
    Off,
    Connecting,
    SendingBatch,
    ShuttingDown
  };

  gsmCom _com{TELEMETRY_SERIAL};
  State _state = State::Off;

  char _recentLanderLines[TELEMETRY_LINES_TO_SEND][TELEMETRY_LINE_BUFFER_SIZE] = {{0}};
  size_t _recentLanderCount = 0;
  size_t _recentLanderNext = 0;

  bool _batchActive = false;
  size_t _batchIndex = 0;
  size_t _batchCount = 0;
  uint8_t _retryCount = 0;
  bool _waitingForDataAck = false;
  bool _lastSendOk = false;

  bool _connected = false;
  bool _shutdownSent = false;
  elapsedMillis _sendIntervalTimer;
  elapsedMillis _sessionTimer;
  elapsedMillis _pollTimer;

  void clearSerial();
  void startSession();
  void shutdownSession();
  void updateStateMachine();
  void updateConnecting();
  void updateSender();
  void startBatch();
  void sendCurrentBatchLine();
  void handleResponse(const char *response);
  size_t recentLineIndex(size_t chronologicalOffset) const;
#endif

  ReceiveCallback _receiveCallback = nullptr;
  TransmitCallback _transmitCallback = nullptr;
};
