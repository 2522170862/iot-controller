#include "JoystickInputDataSource.h"

#include <Arduino.h>

JoystickInputDataSource::JoystickInputDataSource(
    AnalogReadFunction analogReadFunction)
    : analogReadFunction_(analogReadFunction) {}

void JoystickInputDataSource::begin() {
  analogReadResolution(12);
  analogSetPinAttenuation(kXPin, ADC_11db);
  analogSetPinAttenuation(kYPin, ADC_11db);
}

void JoystickInputDataSource::readInto(EnvironmentData* data) const {
  if (data == nullptr) {
    return;
  }

  data->joystickX = readFilteredAxis(kXPin);
  data->joystickY = readFilteredAxis(kYPin);
  data->joystickPressed = false;
}

uint16_t JoystickInputDataSource::readFilteredAxis(uint8_t pin) const {
  uint32_t total = 0;
  for (uint8_t sample = 0; sample < kSamplesPerRead; ++sample) {
    total += analogReadFunction_(pin);
  }
  return static_cast<uint16_t>(total / kSamplesPerRead);
}
