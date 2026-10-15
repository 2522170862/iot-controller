#include "ServoMotor.h"

#include <Arduino.h>

ServoMotor::ServoMotor(uint8_t signalPin) : signalPin_(signalPin) {}

bool ServoMotor::begin() {
  pinMode(signalPin_, OUTPUT);
  digitalWrite(signalPin_, LOW);
  return true;
}

bool ServoMotor::setAngle(int angle) {
  // An explicit command takes priority over an unfinished startup sequence.
  startupReturnPending_ = false;
  if (!attached_) {
    attached_ = ledcAttach(signalPin_, kPwmFrequencyHz, kPwmResolutionBits);
    if (!attached_) {
      Serial.println("Servo PWM initialization failed");
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
    currentPulseWidthUs_ = target;
    targetAngle_ = targetAngle;
    lastStepMs_ = millis();
    hasCommandedPosition_ = true;
    return true;
  }

  targetAngle_ = targetAngle;
  return true;
}

void ServoMotor::update(uint32_t nowMs) {
  if (startupReturnPending_) {
    if (static_cast<uint32_t>(nowMs - startupCenterAtMs_) <
        kStartupCenterHoldMs) return;
    startupReturnPending_ = false;
    targetAngle_ = 0;
    lastStepMs_ = nowMs;
    Serial.println("Servo startup: returning to 0 degrees");
  }
  const int32_t targetPulseWidthUs = pulseWidthUsForAngle(targetAngle_);
  if (!attached_ || !hasCommandedPosition_ ||
      currentPulseWidthUs_ == targetPulseWidthUs ||
      static_cast<uint32_t>(nowMs - lastStepMs_) < kStepDelayMs) return;
  const int32_t direction = targetPulseWidthUs > currentPulseWidthUs_ ? 1 : -1;
  int32_t nextPulseWidthUs = currentPulseWidthUs_ + direction * kPulseStepUs;
  if ((direction > 0 && nextPulseWidthUs > targetPulseWidthUs) ||
      (direction < 0 && nextPulseWidthUs < targetPulseWidthUs)) {
    nextPulseWidthUs = targetPulseWidthUs;
  }
  if (!ledcWrite(signalPin_, dutyForPulseWidth(
                                static_cast<uint16_t>(nextPulseWidthUs)))) {
    Serial.println("Servo PWM write failed");
    lastStepMs_ = nowMs;
    return;
  }
  currentPulseWidthUs_ = nextPulseWidthUs;
  angle_ = currentPulseWidthUs_ == targetPulseWidthUs
               ? targetAngle_
               : static_cast<int>((currentPulseWidthUs_ - kMinimumPulseWidthUs) *
                                  kPhysicalTravelDegrees / kPhysicalPulseSpanUs);
  lastStepMs_ = nowMs;
}

int ServoMotor::angle() const { return angle_; }
