#include "GSMCom.h"

gsmCom::gsmCom(Stream &serialStream) :
  _modem(&serialStream),
  _lastCommandTime(0),
  _waitingForResponse(false),
  _responseTimeout(120000),
  _callback(NULL),
  _bufferIndex(0),
  _inMessage(false) {
  
  // Initialize buffer
  memset(_responseBuffer, 0, MAX_BUFFER_SIZE);
}

void gsmCom::sendCommand(const char* command) {
  // Check if we're already waiting for a response
  if (_waitingForResponse) {
    return;
  }
  // Send the command with start and end delimiters
  _modem->print(GSM_START_DELIMITER);
  _modem->print(command);
  _modem->print(GSM_END_DELIMITER);
  
  // Mark that we're now waiting for a response
  _waitingForResponse = true;
  _lastCommandTime = millis();
  
  // Reset the buffer and message state
  memset(_responseBuffer, 0, MAX_BUFFER_SIZE);
  _bufferIndex = 0;
  _inMessage = false;
}

void gsmCom::update() {
  // Only check if we're waiting for a response
  if (!_waitingForResponse) {
    return;
  }
  
  // Check for timeout
  if (millis() - _lastCommandTime > _responseTimeout) {
    _waitingForResponse = false;
    _inMessage = false;
    _bufferIndex = 0;
    strcpy(_responseBuffer, "0");  // Set message content to "0" on timeout
    // processResponse(); #don't process response on timeout
    return;
  }
  
  // Read available data (non-blocking)
  while (_modem->available() > 0) {
    char inChar = _modem->read();
    
    // Check for start of message
    if (inChar == GSM_START_DELIMITER) {
      _inMessage = true;
      _bufferIndex = 0;
      continue;
    }
    
    // Check for end of message
    if (inChar == GSM_END_DELIMITER) {
      if (_inMessage && _bufferIndex > 0) {
        // Null-terminate the string
        _responseBuffer[_bufferIndex] = '\0';
        
        // Process the received message
        processResponse();
        
        // Reset for next message
        _waitingForResponse = false;
        _inMessage = false;
        _bufferIndex = 0;
        return;
      }
      _inMessage = false;
      continue;
    }
    
    // Add character to buffer if we're inside a message
    if (_inMessage && _bufferIndex < MAX_BUFFER_SIZE - 1) {
      _responseBuffer[_bufferIndex++] = inChar;
    }
  }
}

void gsmCom::setTimeout(unsigned long timeout) {
  _responseTimeout = timeout;
}

void gsmCom::setResponseCallback(ResponseCallback callback) {
  _callback = callback;
}

bool gsmCom::isWaiting() const {
  return _waitingForResponse;
}

void gsmCom::processResponse() {
  // If a callback is registered, call it with the response
  if (_callback != NULL) {
    _callback(_responseBuffer);
  }
}
