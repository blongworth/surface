#include "Message.h"

const char *directionName(MessageDirection direction) {
  switch (direction) {
    case MessageDirection::FromLander: return "RX_LANDER";
    case MessageDirection::ToLander: return "TX_LANDER";
    case MessageDirection::FromConsole: return "RX_CONSOLE";
    case MessageDirection::FromTelemetry: return "RX_TELEMETRY";
    case MessageDirection::ToTelemetry: return "TX_TELEMETRY";
    case MessageDirection::System: return "SYSTEM";
  }
  return "UNKNOWN";
}
