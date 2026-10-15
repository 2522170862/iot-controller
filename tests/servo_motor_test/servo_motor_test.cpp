#include "ServoMotor.h"

static_assert(ServoMotor::safePulseWidthUs(0) == 500);
static_assert(ServoMotor::safePulseWidthUs(500) == 500);
static_assert(ServoMotor::safePulseWidthUs(700) == 700);
static_assert(ServoMotor::safePulseWidthUs(1500) == 1500);
static_assert(ServoMotor::safePulseWidthUs(2050) == 1833);
static_assert(ServoMotor::safePulseWidthUs(2500) == 1833);
static_assert(ServoMotor::safePulseWidthUs(3000) == 1833);

static_assert(ServoMotor::pulseWidthUsForAngle(-20) == 500);
static_assert(ServoMotor::pulseWidthUsForAngle(0) == 500);
static_assert(ServoMotor::pulseWidthUsForAngle(45) == 833);
static_assert(ServoMotor::pulseWidthUsForAngle(90) == 1166);
static_assert(ServoMotor::pulseWidthUsForAngle(135) == 1500);
static_assert(ServoMotor::pulseWidthUsForAngle(180) == 1833);
static_assert(ServoMotor::pulseWidthUsForAngle(220) == 1833);

int main() { return 0; }
