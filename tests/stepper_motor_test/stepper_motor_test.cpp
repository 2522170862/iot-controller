#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

#include "StepperMotor.h"

namespace {
struct WriteEvent {
  uint8_t pin;
  uint8_t value;
};

std::vector<WriteEvent> writes;
}

void pinMode(uint8_t, uint8_t) {}

void digitalWrite(uint8_t pin, uint8_t value) {
  writes.push_back({pin, value});
}

static std::array<uint8_t, 4> lastOutputs() {
  assert(writes.size() >= 4);
  const size_t offset = writes.size() - 4;
  return {writes[offset].value, writes[offset + 1].value,
          writes[offset + 2].value, writes[offset + 3].value};
}

static void testForwardHalfStepSequence() {
  StepperMotor motor(39, 40, 41, 42);
  motor.begin();
  writes.clear();

  motor.moveSteps(2, 1000);
  motor.update(0);
  assert((lastOutputs() == std::array<uint8_t, 4>{HIGH, LOW, LOW, LOW}));
  motor.update(1000);
  assert((lastOutputs() == std::array<uint8_t, 4>{HIGH, HIGH, LOW, LOW}));
  assert(!motor.isBusy());
}

static void testReverseHalfStepSequence() {
  StepperMotor motor(39, 40, 41, 42);
  motor.begin();
  writes.clear();

  motor.moveSteps(-2, 1000);
  motor.update(0);
  assert((lastOutputs() == std::array<uint8_t, 4>{LOW, LOW, LOW, HIGH}));
  motor.update(1000);
  assert((lastOutputs() == std::array<uint8_t, 4>{LOW, LOW, HIGH, HIGH}));
  assert(!motor.isBusy());
}

static void testDegreesUse4096HalfStepsPerRevolution() {
  StepperMotor motor(39, 40, 41, 42);
  motor.begin();
  motor.moveDegrees(90.0f, 1000);

  for (uint32_t step = 0; step < 1024; ++step) {
    motor.update(step * 1000);
  }

  assert(!motor.isBusy());
}

static void testStopReleasesAllCoils() {
  StepperMotor motor(39, 40, 41, 42);
  motor.begin();
  motor.moveSteps(10, 1000);
  motor.update(0);
  writes.clear();

  motor.stop();

  assert((lastOutputs() == std::array<uint8_t, 4>{LOW, LOW, LOW, LOW}));
  assert(!motor.isBusy());
}

int main() {
  testForwardHalfStepSequence();
  testReverseHalfStepSequence();
  testDegreesUse4096HalfStepsPerRevolution();
  testStopReleasesAllCoils();
  return 0;
}
