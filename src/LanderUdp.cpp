#include "LanderUdp.h"
#include "Config.h"
#include "Clock.h"

using qindesign::network::Ethernet;

bool LanderUdp::begin() {
  _ethernetStarted = false;
  _ethernetReady = false;
  _connected = false;
  _ethernetTimeoutReported = false;
  _connectTimeoutReported = false;
  _ethernetTimer = 0;
  _connectTimer = 0;
  _statusRetryTimer = LANDER_STATUS_RETRY_MS;

  startEthernet();
  return _ethernetStarted;
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
  if (!rtcTimeIsValid()) {
    Serial.println("RTC not set or implausible; refusing to send time to lander");
    return;
  }

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

// QNEthernet's static-IP begin() returns without waiting for a link, so the
// rest of the firmware keeps running with the cable unplugged.
void LanderUdp::startEthernet() {
  _ethernetRetryTimer = 0;
  _ethernetStarted = Ethernet.begin(SURFACE_IP, SURFACE_NETMASK, SURFACE_GATEWAY);
  if (!_ethernetStarted) {
    Serial.println("Ethernet.begin failed; retrying");
    return;
  }

  Serial.print("Surface IP address: ");
  Serial.println(Ethernet.localIP());
}

void LanderUdp::updateEthernet() {
  if (_ethernetReady) return;

  if (!_ethernetStarted) {
    if (_ethernetRetryTimer >= ETHERNET_BEGIN_RETRY_MS) {
      startEthernet();
    }
    return;
  }

  if (Ethernet.linkState()) {
    if (!_udp.begin(LANDER_LOCAL_PORT)) {
      Serial.println("UDP socket failed to open");
      return;
    }
    _ethernetReady = true;
    _connectTimer = 0;
    _statusRetryTimer = LANDER_STATUS_RETRY_MS;
    Serial.println("Ethernet link up; probing lander");
    return;
  }

  if (!_ethernetTimeoutReported && _ethernetTimer >= ETHERNET_LINK_TIMEOUT_MS) {
    Serial.println("Ethernet link timeout; still waiting for link");
    _ethernetTimeoutReported = true;
  }
}

void LanderUdp::handlePacket(size_t length) {
  if (length == 0) return;

  // Sync the lander clock as soon as it is reachable; a send from setup()
  // would be dropped because the link is not up yet.
  const bool justConnected = !_connected;
  if (justConnected) {
    _connected = true;
    Serial.println("lander replied to UDP status probe");
    sendTime(now());
  }

  if (_rxBuffer[0] == '?' && length > 1) {
    _status = _rxBuffer[1] - '0';
  } else if (_rxBuffer[0] == '$' && !justConnected) {
    sendTime(now());
  }

  if (_receiveCallback) {
    _receiveCallback(_rxBuffer, length);
  }
}
