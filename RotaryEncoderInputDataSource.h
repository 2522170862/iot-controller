#pragma once

#include <Arduino.h>

#include "DashboardTypes.h"

class RotaryEncoderInputDataSource {
 public:
  using DigitalReadFunction = int (*)(uint8_t pin);

  static constexpr uint8_t kAPin = 6;
  static constexpr uint8_t kBPin = 7;
  static constexpr uint8_t kButtonPin = 38;

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
