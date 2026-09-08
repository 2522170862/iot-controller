#pragma once

#include <Arduino.h>

#include "DashboardTypes.h"

class JoystickInputDataSource {
 public:
  using AnalogReadFunction = uint16_t (*)(uint8_t pin);
  using DigitalReadFunction = int (*)(uint8_t pin);

  static constexpr int8_t kXPin = 1;
  static constexpr int8_t kYPin = 2;
  static constexpr int8_t kButtonPin = 5;

  JoystickInputDataSource(AnalogReadFunction analogReadFunction = analogRead,
                          DigitalReadFunction digitalReadFunction = digitalRead);

  void begin();
  void readInto(EnvironmentData* data) const;

 private:
  AnalogReadFunction analogReadFunction_;
  DigitalReadFunction digitalReadFunction_;
};
