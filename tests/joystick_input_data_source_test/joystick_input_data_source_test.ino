#include <assert.h>

#include "../../JoystickInputDataSource.cpp"

namespace {
uint16_t fakeAnalogRead(uint8_t pin) {
  return pin == JoystickInputDataSource::kXPin ? 1234 : 3456;
}

int fakeDigitalRead(uint8_t pin) {
  return pin == JoystickInputDataSource::kButtonPin ? LOW : HIGH;
}
}  // namespace

void setup() {
  JoystickInputDataSource source(fakeAnalogRead, fakeDigitalRead);
  EnvironmentData data = {};

  source.readInto(&data);

  assert(data.joystickX == 1234);
  assert(data.joystickY == 3456);
  assert(data.joystickPressed);
}

void loop() {}
