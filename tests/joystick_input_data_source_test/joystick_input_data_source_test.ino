#include <assert.h>

#include "../../JoystickInputDataSource.cpp"

namespace {
uint8_t xSampleIndex = 0;
uint8_t ySampleIndex = 0;

uint16_t fakeAnalogRead(uint8_t pin) {
  if (pin == JoystickInputDataSource::kXPin) {
    return (xSampleIndex++ % 2 == 0) ? 1000 : 1020;
  }
  return (ySampleIndex++ % 2 == 0) ? 2000 : 2040;
}

}  // namespace

void setup() {
  JoystickInputDataSource source(fakeAnalogRead);
  EnvironmentData data = {};

  source.readInto(&data);

  assert(data.joystickX == 1010);
  assert(data.joystickY == 2020);
  assert(!data.joystickPressed);
}

void loop() {}
