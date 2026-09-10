#include "ServoMotor.h"

#include <Arduino.h>

ServoMotor::ServoMotor(uint8_t signalPin) : signalPin_(signalPin) {}

bool ServoMotor::begin(int initialAngle) {
  attached_ = ledcAttach(signalPin_, kPwmFrequencyHz, kPwmResolutionBits);
  if (!attached_) {
    Serial.println("MG90S PWM initialization failed");
    return false;
  }

  return setAngle(initialAngle);
}

bool ServoMotor::setAngle(int angle) {
  if (!attached_) {
    return false;
  }

  angle_ = angle < 0 ? 0 : (angle > 180 ? 180 : angle);
  const uint32_t duty = dutyForPulseWidth(pulseWidthUsForAngle(angle_));
  return ledcWrite(signalPin_, duty);
}

int ServoMotor::angle() const { return angle_; }
