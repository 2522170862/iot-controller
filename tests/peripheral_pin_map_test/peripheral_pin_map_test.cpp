#include "../../PeripheralPins.h"

#include <array>
#include <cstddef>
#include <cstdint>

constexpr std::array<int8_t, 25> kUsedPins = {
    PeripheralPins::kJoystickX,    PeripheralPins::kJoystickY,
    PeripheralPins::kRelay1,      PeripheralPins::kRelay2,
    PeripheralPins::kEncoderA,    PeripheralPins::kEncoderB,
    PeripheralPins::kMicrophone,  PeripheralPins::kLcdCs,
    PeripheralPins::kLcdDc,       PeripheralPins::kSpiMosi,
    PeripheralPins::kSpiSck,      PeripheralPins::kSpiMiso,
    PeripheralPins::kRfidSs,      PeripheralPins::kRfidReset,
    PeripheralPins::kServoSignal, PeripheralPins::kRs485Tx,
    PeripheralPins::kRs485Rx,     PeripheralPins::kRgbData,
    PeripheralPins::kEncoderKey,  PeripheralPins::kStepperIn1,
    PeripheralPins::kStepperIn2,  PeripheralPins::kStepperIn3,
    PeripheralPins::kStepperIn4,  PeripheralPins::kDcMotorIn1,
    PeripheralPins::kDcMotorIn2,
};

constexpr bool pinsAreUnique() {
  for (std::size_t left = 0; left < kUsedPins.size(); ++left) {
    for (std::size_t right = left + 1; right < kUsedPins.size(); ++right) {
      if (kUsedPins[left] == kUsedPins[right]) {
        return false;
      }
    }
  }
  return true;
}

static_assert(PeripheralPins::kLcdReset == -1);
static_assert(PeripheralPins::kLcdCs == 9);
static_assert(PeripheralPins::kLcdDc == 10);
static_assert(PeripheralPins::kMicrophone == 8);
static_assert(PeripheralPins::kRelay1 == 4);
static_assert(PeripheralPins::kRelay2 == 5);
static_assert(pinsAreUnique(), "Two peripherals use the same GPIO");

int main() { return 0; }
