#include "ServoMotor.h"

static_assert(ServoMotor::pulseWidthUsForAngle(-20) == 600);
static_assert(ServoMotor::pulseWidthUsForAngle(0) == 600);
static_assert(ServoMotor::pulseWidthUsForAngle(90) == 1500);
static_assert(ServoMotor::pulseWidthUsForAngle(180) == 2400);
static_assert(ServoMotor::pulseWidthUsForAngle(220) == 2400);

int main() { return 0; }
