#pragma once

#include <Arduino.h>
#include <stdint.h>

class DcMotor {
 public:
  static constexpr uint8_t kStartupDemoSpeedPercent = 100;

  using PinModeFunction = void (*)(uint8_t, uint8_t);
  using DigitalWriteFunction = void (*)(uint8_t, uint8_t);
  using AnalogWriteFunction = void (*)(uint8_t, int);

  DcMotor(uint8_t in1Pin, uint8_t in2Pin,
          PinModeFunction pinModeFunction = pinMode,
          DigitalWriteFunction digitalWriteFunction = digitalWrite,
          AnalogWriteFunction analogWriteFunction = analogWrite);

  void begin();
  void forward(uint8_t speedPercent);
  void reverse(uint8_t speedPercent);
  void stop();
  void startStartupDemo(uint32_t nowMs);
  void update(uint32_t nowMs);
  bool demoActive() const { return demoState_ != DemoState::kIdle; }

  static constexpr uint8_t pwmForPercent(uint16_t speedPercent) {
    return speedPercent >= 100
               ? 255
               : static_cast<uint8_t>((speedPercent * 255U + 50U) / 100U);
  }

 private:
  enum class DemoState { kIdle, kForward, kPause, kReverse };

  void applyForward(uint8_t speedPercent);
  void applyReverse(uint8_t speedPercent);
  void applyStop();

  uint8_t in1Pin_;
  uint8_t in2Pin_;
  PinModeFunction pinModeFunction_;
  DigitalWriteFunction digitalWriteFunction_;
  AnalogWriteFunction analogWriteFunction_;
  DemoState demoState_ = DemoState::kIdle;
  uint32_t demoStateStartedMs_ = 0;
};
