#include "LanderUdp.h"
#include "Config.h"

bool LanderUdp::begin() {
  Serial.println("connecting to lander");

  _ethernetReady = false;
  _connected = false;
  _ethernetTimeoutReported = false;
  _connectTimeoutReported = false;
  _ethernetTimer = 0;
  _connectTimer = 0;
  _ethernetRetryTimer = ETHERNET_BEGIN_RETRY_MS;
  _statusRetryTimer = LANDER_STATUS_RETRY_MS;

  beginEthernet();
  return true;
}

void LanderUdp::update() {
  updateEthernet();
  if (!_ethernetReady) return;

  if (!_connected) {
    if (_statusRetryTimer >= LANDER_STATUS_RETRY_MS) {
      _statusRetryTimer = 0;
      requestStatus();
    }

    if (!_connectTimeoutReported && _connectTimer >= LANDER_CONNECT_TIMEOUT_MS) {
      Serial.println("lander response timeout; still retrying");
      _connectTimeoutReported = true;
    }
  }

  int packetSize = _udp.parsePacket();
  if (!packetSize) return;

  int length = _udp.readBytesUntil('\r', _rxBuffer, sizeof(_rxBuffer) - 1);
  _rxBuffer[length] = '\0';
  handlePacket((size_t)length);
}

bool LanderUdp::send(const char *command) {
  if (command == nullptr || command[0] == '\0' || !_ethernetReady) return false;

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

bool LanderUdp::connected() const {
  return _connected;
}

void LanderUdp::setReceiveCallback(ReceiveCallback callback) {
  _receiveCallback = callback;
}

void LanderUdp::setTransmitCallback(TransmitCallback callback) {
  _transmitCallback = callback;
}

void LanderUdp::beginEthernet() {
  Serial.println("calling Ethernet.begin");
  Ethernet.begin(SURFACE_MAC, SURFACE_IP);
  _ethernetRetryTimer = 0;

  Serial.print("Surface IP address: ");
  Serial.println(Ethernet.localIP());
}

void LanderUdp::updateEthernet() {
  if (_ethernetReady) return;

  if (Ethernet.linkStatus() == LinkON) {
    _udp.begin(LANDER_LOCAL_PORT);
    _ethernetReady = true;
    _connectTimer = 0;
    _statusRetryTimer = LANDER_STATUS_RETRY_MS;
    Serial.println("connected to lander");
    return;
  }

  if (!_ethernetTimeoutReported && _ethernetTimer >= ETHERNET_LINK_TIMEOUT_MS) {
    Serial.println("Ethernet link timeout; retrying Ethernet.begin");
    _ethernetTimeoutReported = true;
  }

  if (_ethernetRetryTimer >= ETHERNET_BEGIN_RETRY_MS) {
    beginEthernet();
  }
}

void LanderUdp::handlePacket(size_t length) {
  if (length == 0) return;

  if (!_connected) {
    _connected = true;
    Serial.println("lander replied to UDP status probe");
  }

  if (_rxBuffer[0] == '?' && length > 1) {
    _status = _rxBuffer[1] - '0';
  } else if (_rxBuffer[0] == '$') {
    sendTime(now());
  }

  if (_receiveCallback) {
    _receiveCallback(_rxBuffer, length);
  }
}
