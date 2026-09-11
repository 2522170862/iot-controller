#pragma once

#include <stdint.h>

#include "DashboardTypes.h"

enum class DashboardEventType {
  kNone,
  kJoystick,
  kMicrophone,
  kEncoder,
  kRfid,
};

class DashboardEventPolicy {
 public:
  static constexpr uint32_t kDisplayDurationMs = 3000;
  static constexpr uint16_t kJoystickChangeThreshold = 250;
  static constexpr float kMicrophoneTriggerPercent = 60.0f;

  constexpr DashboardEventType detect(const EnvironmentData& data,
                                      bool rfidReceived, uint32_t nowMs) {
    if (!initialized_) {
      remember(data);
      initialized_ = true;
      if (rfidReceived) trigger(DashboardEventType::kRfid, nowMs);
      return rfidReceived ? DashboardEventType::kRfid
                          : DashboardEventType::kNone;
    }

    DashboardEventType detected = DashboardEventType::kNone;
    if (rfidReceived) {
      detected = DashboardEventType::kRfid;
    } else if (data.encoderPosition != previousEncoderPosition_ ||
               data.encoderPressed != previousEncoderPressed_) {
      detected = DashboardEventType::kEncoder;
    } else if (difference(data.joystickX, previousJoystickX_) >=
                   kJoystickChangeThreshold ||
               difference(data.joystickY, previousJoystickY_) >=
                   kJoystickChangeThreshold) {
      detected = DashboardEventType::kJoystick;
    } else if (previousMicrophonePercent_ <= kMicrophoneTriggerPercent &&
               data.microphonePercent > kMicrophoneTriggerPercent) {
      detected = DashboardEventType::kMicrophone;
    }

    remember(data);
    if (detected != DashboardEventType::kNone) trigger(detected, nowMs);
    return detected;
  }

  constexpr bool active(uint32_t nowMs) const {
    return activeType_ != DashboardEventType::kNone &&
           nowMs - eventStartedMs_ < kDisplayDurationMs;
  }

  constexpr DashboardEventType type(uint32_t nowMs) const {
    return active(nowMs) ? activeType_ : DashboardEventType::kNone;
  }

 private:
  bool initialized_ = false;
  DashboardEventType activeType_ = DashboardEventType::kNone;
  uint32_t eventStartedMs_ = 0;
  uint16_t previousJoystickX_ = 0;
  uint16_t previousJoystickY_ = 0;
  float previousMicrophonePercent_ = 0;
  int32_t previousEncoderPosition_ = 0;
  bool previousEncoderPressed_ = false;

  static constexpr uint16_t difference(uint16_t first, uint16_t second) {
    return first >= second ? first - second : second - first;
  }

  constexpr void trigger(DashboardEventType type, uint32_t nowMs) {
    activeType_ = type;
    eventStartedMs_ = nowMs;
  }

  constexpr void remember(const EnvironmentData& data) {
    previousJoystickX_ = data.joystickX;
    previousJoystickY_ = data.joystickY;
    previousMicrophonePercent_ = data.microphonePercent;
    previousEncoderPosition_ = data.encoderPosition;
    previousEncoderPressed_ = data.encoderPressed;
  }
};
