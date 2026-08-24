#pragma once

#include <TimeLib.h>

// Sanity bounds for the RTC-derived time. Bump RTC_MIN_VALID_EPOCH forward
// periodically as firmware ages, so a stale/incorrect RTC value (e.g. a dead
// backup battery or a bad manual set) doesn't pass as valid.
#define RTC_MIN_VALID_EPOCH 1704067200UL // 2024-01-01T00:00:00Z
#define RTC_MAX_VALID_EPOCH 2051222400UL // 2035-01-01T00:00:00Z

// True only if TimeLib has synced with the RTC and the resulting time falls
// within a plausible range. Catches both an unset RTC (sync provider returns
// 0) and one set to an implausible value.
inline bool rtcTimeIsValid() {
  if (timeStatus() != timeSet) return false;
  time_t current = now();
  return current >= (time_t)RTC_MIN_VALID_EPOCH && current < (time_t)RTC_MAX_VALID_EPOCH;
}
