#pragma once

#include <Arduino.h>

#include "DashboardTypes.h"
#include "PeripheralPins.h"

class RotaryEncoderInputDataSource {
 public:
  using DigitalReadFunction = int (*)(uint8_t pin);

  static constexpr uint8_t kAPin = PeripheralPins::kEncoderA;
  static constexpr uint8_t kBPin = PeripheralPins::kEncoderB;
  static constexpr uint8_t kButtonPin = PeripheralPins::kEncoderKey;

  explicit RotaryEncoderInputDataSource(
      DigitalReadFunction digitalReadFunction = digitalRead);

  void begin();
  void poll();
  void readInto(EnvironmentData* data) const;

 private:
  DigitalReadFunction digitalReadFunction_;
  uint8_t previousState_ = 0;
  int8_t transitionAccumulator_ = 0;
  int32_t position_ = 0;

  uint8_t readState() const;
};
