#include <assert.h>

#include "../../ConnectionStatusIndicators.cpp"

namespace {
uint8_t modes[64] = {};
uint8_t levels[64] = {};

void fakePinMode(uint8_t pin, uint8_t mode) { modes[pin] = mode; }
void fakeDigitalWrite(uint8_t pin, uint8_t level) { levels[pin] = level; }
}  // namespace

void setup() {
  ConnectionStatusIndicators indicators(3, 37, fakePinMode, fakeDigitalWrite);

  indicators.begin();
  assert(modes[3] == OUTPUT && modes[37] == OUTPUT);
  assert(levels[3] == LOW && levels[37] == LOW);

  indicators.update(true, false);
  assert(levels[3] == HIGH && levels[37] == LOW);

  indicators.update(true, true);
  assert(levels[3] == HIGH && levels[37] == HIGH);

  indicators.update(false, false);
  assert(levels[3] == LOW && levels[37] == LOW);
}

void loop() {}
