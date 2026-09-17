#pragma once

#include <Arduino.h>

#include "DashboardTypes.h"
#include "PeripheralPins.h"

class RotaryEncoderInputDataSource {
 public:
  using DigitalReadFunction = int (*)(uint8_t pin);
  using PinModeFunction = void (*)(uint8_t pin, uint8_t mode);

  static constexpr uint8_t kAPin = PeripheralPins::kEncoderA;
  static constexpr uint8_t kBPin = PeripheralPins::kEncoderB;
  static constexpr uint8_t kButtonPin = PeripheralPins::kEncoderKey;

  explicit RotaryEncoderInputDataSource(
      DigitalReadFunction digitalReadFunction = digitalRead,
      PinModeFunction pinModeFunction = pinMode);

  void begin();
  void poll();
  void readInto(EnvironmentData* data) const;

 private:
  DigitalReadFunction digitalReadFunction_;
  PinModeFunction pinModeFunction_;
  uint8_t previousState_ = 0;
  int8_t transitionAccumulator_ = 0;
  int32_t position_ = 0;

  uint8_t readState() const;
};
