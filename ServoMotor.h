#pragma once

#include <stdint.h>

class ServoMotor {
 public:
  explicit ServoMotor(uint8_t signalPin);

  bool begin(int initialAngle = 90);
  bool setAngle(int angle);
  int angle() const;

  static constexpr uint16_t pulseWidthUsForAngle(int angle) {
    const int limitedAngle = angle < 0 ? 0 : (angle > 180 ? 180 : angle);
    return static_cast<uint16_t>(600 + limitedAngle * 10);
  }

 private:
  static constexpr uint32_t kPwmFrequencyHz = 50;
  static constexpr uint8_t kPwmResolutionBits = 14;
  static constexpr uint32_t kPwmMaximumDuty = (1UL << kPwmResolutionBits) - 1;
  static constexpr uint32_t kPwmPeriodUs = 1000000UL / kPwmFrequencyHz;

  uint8_t signalPin_;
  int angle_ = 90;
  bool attached_ = false;

  static constexpr uint32_t dutyForPulseWidth(uint16_t pulseWidthUs) {
    return static_cast<uint32_t>(pulseWidthUs) * kPwmMaximumDuty /
           kPwmPeriodUs;
  }
};
