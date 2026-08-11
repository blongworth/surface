#ifndef GSM_COMM_H
#define GSM_COMM_H

#include <Arduino.h>

// Default protocol delimiters - can be overridden by defining before including this header
#ifndef GSM_START_DELIMITER
#define GSM_START_DELIMITER '<'
#endif

#ifndef GSM_END_DELIMITER
#define GSM_END_DELIMITER '>'
#endif

class gsmCom {
  public:
    // Function pointer type for response callback
    typedef void (*ResponseCallback)(const char* response);
    
    // Constructor that takes a Stream reference
    gsmCom(Stream &serialStream);
    
    // Send a request and wait for response non-blockingly
    // Request will be wrapped with start and end delimiters
    void sendCommand(const char* command);
    
    // Process any incoming data (call this in your loop)
    void update();
    
    // Set timeout for responses (in milliseconds)
    void setTimeout(unsigned long timeout);
    
    // Set the callback function for when responses are received
    void setResponseCallback(ResponseCallback callback);
    
    // Check if currently waiting for a response
    bool isWaiting() const;
    
  private:
    Stream* _modem;
    unsigned long _lastCommandTime;
    bool _waitingForResponse;
    unsigned long _responseTimeout;
    ResponseCallback _callback;
    
    // Buffer for incoming data
    static const int MAX_BUFFER_SIZE = 128;
    char _responseBuffer[MAX_BUFFER_SIZE];
    int _bufferIndex;
    
    // Process a complete response
    void processResponse();
    
    // Flag to track if we're within a message
    bool _inMessage;
};

#endif
