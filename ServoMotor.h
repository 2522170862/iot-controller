#pragma once

#include <stdint.h>

class ServoMotor {
 public:
  explicit ServoMotor(uint8_t signalPin);

  bool begin();
  bool setAngle(int angle);
  void update(uint32_t nowMs);
  int angle() const;

  // ZP10S measured travel is approximately 270 degrees over 500-2500 us.
  // Keep MQTT commands limited to 180 physical degrees.
  static constexpr uint16_t kMinimumPulseWidthUs = 500;
  static constexpr int kPhysicalTravelDegrees = 270;
  static constexpr uint16_t kPhysicalPulseSpanUs = 2000;
  static constexpr uint16_t kMaximumPulseWidthUs = 1833;
  static constexpr uint16_t kCenterPulseWidthUs = 1166;  // Commanded 90 degrees.
  static constexpr int kMinimumAngle = 0;
  static constexpr int kMaximumAngle = 180;

  static constexpr uint16_t safePulseWidthUs(uint16_t pulseWidthUs) {
    return pulseWidthUs < kMinimumPulseWidthUs
               ? kMinimumPulseWidthUs
               : (pulseWidthUs > kMaximumPulseWidthUs
                      ? kMaximumPulseWidthUs
                      : pulseWidthUs);
  }

  static constexpr uint16_t pulseWidthUsForAngle(int angle) {
    const int limitedAngle =
        angle < kMinimumAngle
            ? kMinimumAngle
            : (angle > kMaximumAngle ? kMaximumAngle : angle);
    return static_cast<uint16_t>(
        kMinimumPulseWidthUs +
        static_cast<uint32_t>(limitedAngle) * kPhysicalPulseSpanUs /
            kPhysicalTravelDegrees);
  }

 private:
  static constexpr uint32_t kPwmFrequencyHz = 50;
  static constexpr uint8_t kPwmResolutionBits = 14;
  static constexpr uint32_t kPwmMaximumDuty = (1UL << kPwmResolutionBits) - 1;
  static constexpr uint32_t kPwmPeriodUs = 1000000UL / kPwmFrequencyHz;
  static constexpr int32_t kPulseStepUs = 21;
  static constexpr uint16_t kStepDelayMs = 20;

  uint8_t signalPin_;
  int angle_ = -1;  // Unknown until the first explicit position command.
  bool attached_ = false;
  bool hasCommandedPosition_ = false;
  int targetAngle_ = 0;
  int32_t currentPulseWidthUs_ = kCenterPulseWidthUs;
  uint32_t lastStepMs_ = 0;
  bool startupReturnPending_ = false;
  uint32_t startupCenterAtMs_ = 0;
  static constexpr uint32_t kStartupCenterHoldMs = 1000;

  static constexpr uint32_t dutyForPulseWidth(uint16_t pulseWidthUs) {
    return static_cast<uint32_t>(safePulseWidthUs(pulseWidthUs)) *
           kPwmMaximumDuty /
           kPwmPeriodUs;
  }
};
