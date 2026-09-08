#include "StepperMotor.h"

#include <math.h>

namespace {
constexpr uint8_t kHalfStepSequence[8][4] = {
    {HIGH, LOW, LOW, LOW},  {HIGH, HIGH, LOW, LOW},
    {LOW, HIGH, LOW, LOW},  {LOW, HIGH, HIGH, LOW},
    {LOW, LOW, HIGH, LOW},  {LOW, LOW, HIGH, HIGH},
    {LOW, LOW, LOW, HIGH},  {HIGH, LOW, LOW, HIGH},
};
}

StepperMotor::StepperMotor(uint8_t in1Pin, uint8_t in2Pin, uint8_t in3Pin,
                           uint8_t in4Pin)
    : pins_{in1Pin, in2Pin, in3Pin, in4Pin} {}

void StepperMotor::begin() {
  for (uint8_t pin : pins_) {
    pinMode(pin, OUTPUT);
  }
  release();
}

void StepperMotor::moveSteps(int32_t steps, uint32_t stepIntervalUs) {
  if (steps == 0) {
    remainingSteps_ = 0;
    firstStepPending_ = false;
    return;
  }

  direction_ = steps > 0 ? 1 : -1;
  remainingSteps_ = steps > 0 ? steps : -steps;
  stepIntervalUs_ = stepIntervalUs == 0 ? 1 : stepIntervalUs;
  phaseIndex_ = direction_ > 0 ? 0 : 7;
  firstStepPending_ = true;
}

void StepperMotor::moveDegrees(float degrees, uint32_t stepIntervalUs) {
  const int32_t steps = static_cast<int32_t>(
      lroundf(degrees * kHalfStepsPerRevolution / 360.0f));
  moveSteps(steps, stepIntervalUs);
}

void StepperMotor::update(uint32_t nowUs) {
  if (remainingSteps_ == 0) {
    return;
  }

  if (!firstStepPending_ &&
      static_cast<int32_t>(nowUs - nextStepAtUs_) < 0) {
    return;
  }

  writePhase(phaseIndex_);
  --remainingSteps_;
  firstStepPending_ = false;
  nextStepAtUs_ = nowUs + stepIntervalUs_;

  if (direction_ > 0) {
    phaseIndex_ = (phaseIndex_ + 1) % 8;
  } else {
    phaseIndex_ = (phaseIndex_ + 7) % 8;
  }
}

void StepperMotor::stop(bool releaseCoils) {
  remainingSteps_ = 0;
  firstStepPending_ = false;
  if (releaseCoils) {
    release();
  }
}

void StepperMotor::release() {
  for (uint8_t pin : pins_) {
    digitalWrite(pin, LOW);
  }
}

bool StepperMotor::isBusy() const { return remainingSteps_ > 0; }

void StepperMotor::writePhase(uint8_t phaseIndex) {
  for (uint8_t index = 0; index < 4; ++index) {
    digitalWrite(pins_[index], kHalfStepSequence[phaseIndex][index]);
  }
}

