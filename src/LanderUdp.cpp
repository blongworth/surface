#include "LanderUdp.h"
#include "Config.h"

bool LanderUdp::begin() {
  Ethernet.begin(SURFACE_MAC, SURFACE_IP);
  _udp.begin(LANDER_LOCAL_PORT);

  Serial.print("Surface IP address: ");
  Serial.println(Ethernet.localIP());
  return true;
}

void LanderUdp::update() {
  int packetSize = _udp.parsePacket();
  if (!packetSize) return;

  int length = _udp.readBytesUntil('\r', _rxBuffer, sizeof(_rxBuffer) - 1);
  _rxBuffer[length] = '\0';
  handlePacket((size_t)length);
}

bool LanderUdp::send(const char *command) {
  if (command == nullptr || command[0] == '\0') return false;

  _udp.beginPacket(LANDER_IP, LANDER_REMOTE_PORT);
  _udp.print(command);
  _udp.print('\r');
  bool ok = _udp.endPacket() == 1;

  if (_transmitCallback) {
    _transmitCallback(command, strlen(command));
  }
  return ok;
}

bool LanderUdp::sendLine(const char *command) {
  return send(command);
}

void LanderUdp::requestStatus() {
  send("?");
}

void LanderUdp::sendTime(time_t timestamp) {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%s%lu", TIME_HEADER, (unsigned long)timestamp);
  send(buffer);
}

int LanderUdp::status() const {
  return _status;
}

void LanderUdp::setReceiveCallback(ReceiveCallback callback) {
  _receiveCallback = callback;
}

void LanderUdp::setTransmitCallback(TransmitCallback callback) {
  _transmitCallback = callback;
}

void LanderUdp::handlePacket(size_t length) {
  if (length == 0) return;

  if (_rxBuffer[0] == '?' && length > 1) {
    _status = _rxBuffer[1] - '0';
  } else if (_rxBuffer[0] == '$') {
    sendTime(now());
  }

  if (_receiveCallback) {
    _receiveCallback(_rxBuffer, length);
  }
}
