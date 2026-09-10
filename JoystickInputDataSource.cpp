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

  data->joystickX = static_cast<uint16_t>(analogReadFunction_(kXPin));
  data->joystickY = static_cast<uint16_t>(analogReadFunction_(kYPin));
  data->joystickPressed = false;
}
