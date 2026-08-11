#pragma once

#include <Arduino.h>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <TimeLib.h>
#include "Config.h"

class LanderUdp {
public:
  typedef void (*ReceiveCallback)(const char *data, size_t length);
  typedef void (*TransmitCallback)(const char *data, size_t length);

  bool begin();
  void update();

  bool send(const char *command);
  bool sendLine(const char *command);
  void requestStatus();
  void sendTime(time_t timestamp);

  int status() const;
  void setReceiveCallback(ReceiveCallback callback);
  void setTransmitCallback(TransmitCallback callback);

private:
  EthernetUDP _udp;
  char _rxBuffer[UDP_BUFFER_SIZE] = {0};
  int _status = -1;
  ReceiveCallback _receiveCallback = nullptr;
  TransmitCallback _transmitCallback = nullptr;

  void handlePacket(size_t length);
};
