#include <assert.h>

#include "../../RotaryEncoderInputDataSource.cpp"

namespace {
uint8_t state = 0b11;
bool buttonPressed = false;
uint8_t pinModes[64] = {};

void fakePinMode(uint8_t pin, uint8_t mode) {
  pinModes[pin] = mode;
}

int fakeDigitalRead(uint8_t pin) {
  if (pin == RotaryEncoderInputDataSource::kButtonPin) {
    return buttonPressed ? LOW : HIGH;
  }

  if (pin == RotaryEncoderInputDataSource::kAPin) {
    return (state & 0b10) == 0 ? LOW : HIGH;
  }

  return (state & 0b01) == 0 ? LOW : HIGH;
}

void rotateClockwise(RotaryEncoderInputDataSource* source) {
  const uint8_t states[] = {0b01, 0b00, 0b10, 0b11};
  for (uint8_t nextState : states) {
    state = nextState;
    source->poll();
  }
}

void rotateCounterclockwise(RotaryEncoderInputDataSource* source) {
  const uint8_t states[] = {0b10, 0b00, 0b01, 0b11};
  for (uint8_t nextState : states) {
    state = nextState;
    source->poll();
  }
}
}  // namespace

void setup() {
  RotaryEncoderInputDataSource source(fakeDigitalRead, fakePinMode);
  source.begin();

  assert(pinModes[RotaryEncoderInputDataSource::kAPin] == INPUT_PULLUP);
  assert(pinModes[RotaryEncoderInputDataSource::kBPin] == INPUT_PULLUP);
  assert(pinModes[RotaryEncoderInputDataSource::kButtonPin] == INPUT_PULLUP);

  rotateClockwise(&source);
  rotateClockwise(&source);
  rotateCounterclockwise(&source);
  buttonPressed = true;

  EnvironmentData data = {};
  source.readInto(&data);

  assert(data.encoderPosition == 1);
  assert(data.encoderPressed);
}

void loop() {}
