#include "ServoMotor.h"

#include <Arduino.h>

ServoMotor::ServoMotor(uint8_t signalPin) : signalPin_(signalPin) {}

bool ServoMotor::begin() {
  // An idle low signal sends no position command during startup.
  pinMode(signalPin_, OUTPUT);
  digitalWrite(signalPin_, LOW);
  return true;
}

bool ServoMotor::setAngle(int angle) {
  if (!attached_) {
    attached_ = ledcAttach(signalPin_, kPwmFrequencyHz, kPwmResolutionBits);
    if (!attached_) {
      Serial.println("MG90S PWM initialization failed");
      return false;
    }
  }

  const int targetAngle =
      angle < kMinimumAngle
          ? kMinimumAngle
          : (angle > kMaximumAngle ? kMaximumAngle : angle);
  const int32_t target = pulseWidthUsForAngle(targetAngle);

  // Physical position is unknown after reboot. Apply only the requested
  // target on the first command; later commands can move smoothly.
  if (!hasCommandedPosition_) {
    if (!ledcWrite(signalPin_, dutyForPulseWidth(target))) return false;
    angle_ = targetAngle;
    hasCommandedPosition_ = true;
    return true;
  }

  int32_t currentPulseWidthUs = pulseWidthUsForAngle(angle_);
  const int32_t direction = target >= currentPulseWidthUs ? 1 : -1;

  if (!ledcWrite(signalPin_, dutyForPulseWidth(
                                  static_cast<uint16_t>(currentPulseWidthUs)))) {
    return false;
  }

  while (currentPulseWidthUs != target) {
    int32_t nextPulseWidthUs =
        currentPulseWidthUs + direction * kPulseStepUs;
    if ((direction > 0 && nextPulseWidthUs > target) ||
        (direction < 0 && nextPulseWidthUs < target)) {
      nextPulseWidthUs = target;
    }

    if (!ledcWrite(signalPin_, dutyForPulseWidth(
         static_cast<uint16_t>(nextPulseWidthUs)))) {
      return false;
    }

    currentPulseWidthUs = nextPulseWidthUs;
    delay(kStepDelayMs);
  }

  angle_ = targetAngle;
  return true;
}

int ServoMotor::angle() const { return angle_; }
