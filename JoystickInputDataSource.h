#pragma once

#include <Arduino.h>

#include "DashboardTypes.h"
#include "PeripheralPins.h"

class JoystickInputDataSource {
 public:
  using AnalogReadFunction = uint16_t (*)(uint8_t pin);
  static constexpr int8_t kXPin = PeripheralPins::kJoystickX;
  static constexpr int8_t kYPin = PeripheralPins::kJoystickY;
  static constexpr uint8_t kSamplesPerRead = 16;

  explicit JoystickInputDataSource(
      AnalogReadFunction analogReadFunction = analogRead);

  void begin();
  void readInto(EnvironmentData* data) const;

 private:
  AnalogReadFunction analogReadFunction_;

  uint16_t readFilteredAxis(uint8_t pin) const;
};
