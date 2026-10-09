#include "ServoMotor.h"

static_assert(ServoMotor::safePulseWidthUs(500) == 700);
static_assert(ServoMotor::safePulseWidthUs(700) == 700);
static_assert(ServoMotor::safePulseWidthUs(1500) == 1500);
static_assert(ServoMotor::safePulseWidthUs(2050) == 2050);
static_assert(ServoMotor::safePulseWidthUs(2100) == 2050);
static_assert(ServoMotor::safePulseWidthUs(2300) == 2050);

static_assert(ServoMotor::pulseWidthUsForAngle(-20) == 700);
static_assert(ServoMotor::pulseWidthUsForAngle(0) == 700);
static_assert(ServoMotor::pulseWidthUsForAngle(10) == 700);
static_assert(ServoMotor::pulseWidthUsForAngle(90) == 1500);
static_assert(ServoMotor::pulseWidthUsForAngle(130) == 1800);
static_assert(ServoMotor::pulseWidthUsForAngle(160) == 2025);
static_assert(ServoMotor::pulseWidthUsForAngle(170) == 2050);
static_assert(ServoMotor::pulseWidthUsForAngle(180) == 2050);
static_assert(ServoMotor::pulseWidthUsForAngle(220) == 2050);

int main() { return 0; }
