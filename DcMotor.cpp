#include "DcMotor.h"

namespace {
constexpr uint32_t kDemoRunDurationMs = 2000;
constexpr uint32_t kDemoPauseDurationMs = 500;
}  // namespace

DcMotor::DcMotor(uint8_t in1Pin, uint8_t in2Pin,
                 PinModeFunction pinModeFunction,
                 DigitalWriteFunction digitalWriteFunction,
                 AnalogWriteFunction analogWriteFunction)
    : in1Pin_(in1Pin),
      in2Pin_(in2Pin),
      pinModeFunction_(pinModeFunction),
      digitalWriteFunction_(digitalWriteFunction),
      analogWriteFunction_(analogWriteFunction) {}

void DcMotor::begin() {
  pinModeFunction_(in1Pin_, OUTPUT);
  pinModeFunction_(in2Pin_, OUTPUT);
  applyStop();
}

void DcMotor::forward(uint8_t speedPercent) {
  demoState_ = DemoState::kIdle;
  applyForward(speedPercent);
}

void DcMotor::reverse(uint8_t speedPercent) {
  demoState_ = DemoState::kIdle;
  applyReverse(speedPercent);
}

void DcMotor::stop() {
  demoState_ = DemoState::kIdle;
  applyStop();
}

void DcMotor::startStartupDemo(uint32_t nowMs) {
  demoState_ = DemoState::kForward;
  demoStateStartedMs_ = nowMs;
  applyForward(kStartupDemoSpeedPercent);
}

void DcMotor::update(uint32_t nowMs) {
  const uint32_t elapsedMs = nowMs - demoStateStartedMs_;
  switch (demoState_) {
    case DemoState::kForward:
      if (elapsedMs >= kDemoRunDurationMs) {
        applyStop();
        demoState_ = DemoState::kPause;
        demoStateStartedMs_ = nowMs;
      }
      break;
    case DemoState::kPause:
      if (elapsedMs >= kDemoPauseDurationMs) {
        applyReverse(kStartupDemoSpeedPercent);
        demoState_ = DemoState::kReverse;
        demoStateStartedMs_ = nowMs;
      }
      break;
    case DemoState::kReverse:
      if (elapsedMs >= kDemoRunDurationMs) {
        applyStop();
        demoState_ = DemoState::kIdle;
      }
      break;
    case DemoState::kIdle:
      break;
  }
}

void DcMotor::applyForward(uint8_t speedPercent) {
  analogWriteFunction_(in2Pin_, 0);
  analogWriteFunction_(in1Pin_, pwmForPercent(speedPercent));
}

void DcMotor::applyReverse(uint8_t speedPercent) {
  analogWriteFunction_(in1Pin_, 0);
  analogWriteFunction_(in2Pin_, pwmForPercent(speedPercent));
}

void DcMotor::applyStop() {
  analogWriteFunction_(in1Pin_, 0);
  analogWriteFunction_(in2Pin_, 0);
  digitalWriteFunction_(in1Pin_, LOW);
  digitalWriteFunction_(in2Pin_, LOW);
}
