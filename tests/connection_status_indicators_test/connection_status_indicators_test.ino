#include <assert.h>

#include "../../ConnectionStatusIndicators.cpp"

namespace {
uint8_t modes[64] = {};
uint8_t levels[64] = {};

void fakePinMode(uint8_t pin, uint8_t mode) { modes[pin] = mode; }
void fakeDigitalWrite(uint8_t pin, uint8_t level) { levels[pin] = level; }
}  // namespace

void setup() {
  ConnectionStatusIndicators indicators(3, 43, 44, fakePinMode,
                                         fakeDigitalWrite);

  indicators.begin();
  assert(modes[3] == OUTPUT && modes[43] == OUTPUT && modes[44] == OUTPUT);
  assert(levels[3] == LOW && levels[43] == LOW && levels[44] == LOW);

  indicators.update(0, false, true, false, false);
  assert(levels[3] == LOW);
  indicators.update(250, false, true, false, false);
  assert(levels[3] == HIGH);
  indicators.update(500, false, true, false, false);
  assert(levels[3] == LOW);

  indicators.update(500, true, false, true, true);
  assert(levels[3] == HIGH && levels[43] == HIGH && levels[44] == HIGH);

  indicators.update(750, false, false, false, false);
  assert(levels[3] == LOW && levels[43] == LOW && levels[44] == LOW);
}

void loop() {}
