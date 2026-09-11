#include <type_traits>

#include "ModuleCommandDispatcher.h"

class DcMotor;

static_assert(std::is_constructible<
              ModuleCommandDispatcher, RelayController*, RgbLedMatrix*,
              ServoMotor*, StepperMotor*, DcMotor*>::value);

int main() { return 0; }
