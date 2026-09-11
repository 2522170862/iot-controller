#pragma once

#include <stdint.h>

class MqttTelemetrySchedule {
 public:
  static constexpr uint32_t kEnvironmentIntervalMs = 20000;
  static bool environmentDue(uint32_t nowMs, uint32_t lastMs) {
    return nowMs - lastMs >= kEnvironmentIntervalMs;
  }
  static bool encoderChanged(int32_t currentPosition, bool currentPressed,
                             int32_t previousPosition, bool previousPressed) {
    return currentPosition != previousPosition || currentPressed != previousPressed;
  }
};
