#pragma once

#include <stdint.h>

class MqttTelemetrySchedule {
 public:
  static constexpr uint32_t kEnvironmentIntervalMs = 20000;
  static constexpr uint16_t kJoystickChangeThreshold = 20;
  static bool environmentDue(uint32_t nowMs, uint32_t lastMs) {
    return nowMs - lastMs >= kEnvironmentIntervalMs;
  }
  static bool encoderChanged(int32_t currentPosition, bool currentPressed,
                             int32_t previousPosition, bool previousPressed) {
    return currentPosition != previousPosition || currentPressed != previousPressed;
  }
  static constexpr bool joystickChanged(uint16_t currentX, uint16_t currentY,
                                        uint16_t previousX,
                                        uint16_t previousY) {
    return difference(currentX, previousX) > kJoystickChangeThreshold ||
           difference(currentY, previousY) > kJoystickChangeThreshold;
  }

 private:
  static constexpr uint16_t difference(uint16_t first, uint16_t second) {
    return first >= second ? first - second : second - first;
  }
};
