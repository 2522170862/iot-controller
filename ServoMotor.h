#pragma once

#include <stdint.h>

class ServoMotor {
 public:
  explicit ServoMotor(uint8_t signalPin);

  bool begin();
  bool setAngle(int angle);
  int angle() const;

  static constexpr uint16_t kMinimumPulseWidthUs = 700;
  // Limit the upper endpoint for this MG90S; 2100 us still overruns it.
  static constexpr uint16_t kMaximumPulseWidthUs = 2050;
  static constexpr uint16_t kCenterPulseWidthUs = 1500;
  static constexpr int kMinimumAngle = 10;
  static constexpr int kMaximumAngle = 170;

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
    // Keep the existing angle mapping until the safe upper pulse limit.
    if (limitedAngle <= 90) {
      return static_cast<uint16_t>(
          kMinimumPulseWidthUs +
          (limitedAngle - kMinimumAngle) *
              (kCenterPulseWidthUs - kMinimumPulseWidthUs) /
              (90 - kMinimumAngle));
    }
    const uint16_t nominalPulseWidthUs = static_cast<uint16_t>(
        kCenterPulseWidthUs +
        (limitedAngle - 90) *
            (2100 - kCenterPulseWidthUs) /
            (kMaximumAngle - 90));
    return safePulseWidthUs(nominalPulseWidthUs);
  }

 private:
  static constexpr uint32_t kPwmFrequencyHz = 50;
  static constexpr uint8_t kPwmResolutionBits = 14;
  static constexpr uint32_t kPwmMaximumDuty = (1UL << kPwmResolutionBits) - 1;
  static constexpr uint32_t kPwmPeriodUs = 1000000UL / kPwmFrequencyHz;
  static constexpr uint16_t kPulseStepUs = 21;
  static constexpr uint16_t kStepDelayMs = 20;

  uint8_t signalPin_;
  int angle_ = -1;  // Unknown until the first explicit position command.
  bool attached_ = false;
  bool hasCommandedPosition_ = false;

  static constexpr uint32_t dutyForPulseWidth(uint16_t pulseWidthUs) {
    return static_cast<uint32_t>(safePulseWidthUs(pulseWidthUs)) *
           kPwmMaximumDuty /
           kPwmPeriodUs;
  }
};
