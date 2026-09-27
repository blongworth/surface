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

  updateConnection();

  // Drain a bounded number of packets so a burst doesn't back up the stack
  // but also can't starve the rest of the loop.
  for (int i = 0; i < LANDER_MAX_PACKETS_PER_UPDATE && _udp.parsePacket() >= 0; i++) {
    readPacket();
  }
}

void LanderUdp::updateConnection() {
  if (_connected && _lastPacketTimer >= LANDER_LINK_LOST_MS) {
    _connected = false;
    _connectTimer = 0;
    _connectTimeoutReported = false;
    _statusRetryTimer = LANDER_STATUS_RETRY_MS;
    event("no packets from lander; probing");
  }

  if (_connected) return;

  if (_statusRetryTimer >= LANDER_STATUS_RETRY_MS) {
    _statusRetryTimer = 0;
    requestStatus();
  }

  if (!_connectTimeoutReported && _connectTimer >= LANDER_CONNECT_TIMEOUT_MS) {
    event("lander response timeout; still retrying");
    _connectTimeoutReported = true;
  }
}

// Reads a whole datagram (no Stream timeouts) and hands each CR/LF-delimited
// line to handleLine().
void LanderUdp::readPacket() {
  int packetSize = (int)_udp.size();
  int length = _udp.read(_rxBuffer, sizeof(_rxBuffer) - 1);
  if (length <= 0) return;
  _rxBuffer[length] = '\0';

  if (packetSize > length) {
    char line[64];
    snprintf(line, sizeof(line), "lander packet truncated: %d of %d bytes kept", length, packetSize);
    event(line);
  }

  char *start = _rxBuffer;
  for (char *p = _rxBuffer; p <= _rxBuffer + length; p++) {
    if (*p == '\r' || *p == '\n' || *p == '\0') {
      *p = '\0';
      if (p > start) handleLine(start, p - start);
      start = p + 1;
    }
  }
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

void LanderUdp::setEventCallback(EventCallback callback) {
  _eventCallback = callback;
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

void LanderUdp::handleLine(char *line, size_t length) {
  _lastPacketTimer = 0;

  // Sync the lander clock as soon as it is reachable; a send from setup()
  // would be dropped because the link is not up yet.
  const bool justConnected = !_connected;
  if (justConnected) {
    _connected = true;
    event("lander connected");
    sendTime(now());
  }

  if (line[0] == '?' && length > 1) {
    _status = line[1] - '0';
  } else if (line[0] == '$' && !justConnected) {
    sendTime(now());
  }

  if (_receiveCallback) {
    _receiveCallback(line, length);
  }
}

void LanderUdp::event(const char *message) {
  if (_eventCallback) {
    _eventCallback(message);
  } else {
    Serial.println(message);
  }
}
