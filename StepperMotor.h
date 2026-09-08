#pragma once

#include <Arduino.h>

class StepperMotor {
 public:
  static constexpr int32_t kHalfStepsPerRevolution = 4096;
  static constexpr uint32_t kDefaultStepIntervalUs = 2000;

  StepperMotor(uint8_t in1Pin, uint8_t in2Pin, uint8_t in3Pin,
               uint8_t in4Pin);

  void begin();
  void moveSteps(int32_t steps,
                 uint32_t stepIntervalUs = kDefaultStepIntervalUs);
  void moveDegrees(float degrees,
                   uint32_t stepIntervalUs = kDefaultStepIntervalUs);
  void update(uint32_t nowUs);
  void stop(bool releaseCoils = true);
  void release();
  bool isBusy() const;

 private:
  void writePhase(uint8_t phaseIndex);

  uint8_t pins_[4];
  int32_t remainingSteps_ = 0;
  int8_t direction_ = 1;
  uint8_t phaseIndex_ = 0;
  uint32_t stepIntervalUs_ = kDefaultStepIntervalUs;
  uint32_t nextStepAtUs_ = 0;
  bool firstStepPending_ = false;
};

