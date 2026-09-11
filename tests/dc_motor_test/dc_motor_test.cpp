#include <assert.h>

#include "DcMotor.h"

namespace {
uint8_t levels[64] = {};
int pwmValues[64] = {};

void fakePinMode(uint8_t, uint8_t) {}
void fakeDigitalWrite(uint8_t pin, uint8_t level) { levels[pin] = level; }
void fakeAnalogWrite(uint8_t pin, int value) { pwmValues[pin] = value; }
}  // namespace

int main() {
  static_assert(DcMotor::pwmForPercent(0) == 0);
  static_assert(DcMotor::pwmForPercent(50) == 128);
  static_assert(DcMotor::pwmForPercent(100) == 255);
  static_assert(DcMotor::pwmForPercent(150) == 255);
  static_assert(DcMotor::kStartupDemoSpeedPercent == 100);

  DcMotor motor(47, 48, fakePinMode, fakeDigitalWrite, fakeAnalogWrite);
  motor.begin();
  assert(pwmValues[47] == 0 && pwmValues[48] == 0);

  motor.forward(40);
  assert(pwmValues[47] == DcMotor::pwmForPercent(40));
  assert(pwmValues[48] == 0);

  motor.reverse(60);
  assert(pwmValues[47] == 0);
  assert(pwmValues[48] == DcMotor::pwmForPercent(60));

  motor.startStartupDemo(1000);
  motor.update(1000);
  assert(motor.demoActive());
  assert(pwmValues[47] ==
         DcMotor::pwmForPercent(DcMotor::kStartupDemoSpeedPercent));
  assert(pwmValues[48] == 0);

  motor.update(3000);
  assert(pwmValues[47] == 0 && pwmValues[48] == 0);
  motor.update(3500);
  assert(pwmValues[47] == 0);
  assert(pwmValues[48] ==
         DcMotor::pwmForPercent(DcMotor::kStartupDemoSpeedPercent));
  motor.update(5500);
  assert(!motor.demoActive());
  assert(pwmValues[47] == 0 && pwmValues[48] == 0);
}
