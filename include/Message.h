#pragma once

#include <Arduino.h>

enum class MessageDirection {
  FromLander,
  ToLander,
  FromConsole,
  FromTelemetry,
  ToTelemetry,
  System
};

const char *directionName(MessageDirection direction);
