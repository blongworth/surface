#pragma once

#include <Arduino.h>
#include <QNEthernet.h>
#include <TimeLib.h>
#include "Config.h"

class LanderUdp {
public:
  typedef void (*ReceiveCallback)(const char *data, size_t length);
  typedef void (*TransmitCallback)(const char *data, size_t length);
  typedef void (*EventCallback)(const char *message);

  bool begin();
  void update();

  bool send(const char *command);
  bool sendLine(const char *command);
  void requestStatus();
  void sendTime(time_t timestamp);

  int status() const;
  bool connected() const;
  void setReceiveCallback(ReceiveCallback callback);
  void setTransmitCallback(TransmitCallback callback);
  void setEventCallback(EventCallback callback);

private:
  qindesign::network::EthernetUDP _udp;
  char _rxBuffer[UDP_BUFFER_SIZE] = {0};
  int _status = -1;
  bool _ethernetStarted = false;
  bool _ethernetReady = false;
  bool _connected = false;
  bool _ethernetTimeoutReported = false;
  bool _connectTimeoutReported = false;
  elapsedMillis _ethernetRetryTimer;
  elapsedMillis _ethernetTimer;
  elapsedMillis _statusRetryTimer;
  elapsedMillis _connectTimer;
  elapsedMillis _lastPacketTimer;
  ReceiveCallback _receiveCallback = nullptr;
  TransmitCallback _transmitCallback = nullptr;
  EventCallback _eventCallback = nullptr;

  void startEthernet();
  void updateEthernet();
  void updateConnection();
  void readPacket();
  void handleLine(char *line, size_t length);
  void event(const char *message);
};
