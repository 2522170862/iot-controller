#pragma once

#include <stdint.h>

namespace PeripheralPins {
constexpr int8_t kJoystickX = 1;
constexpr int8_t kJoystickY = 2;
constexpr int8_t kRelay1 = 4;
constexpr int8_t kRelay2 = 5;
constexpr int8_t kEncoderA = 6;
constexpr int8_t kEncoderB = 7;
constexpr int8_t kMicrophone = 8;
constexpr int8_t kLcdCs = 9;
constexpr int8_t kLcdDc = 10;
constexpr int8_t kLcdReset = -1;  // ST7789 RST is wired to ESP32-S3 EN.
constexpr int8_t kSpiMosi = 11;
constexpr int8_t kSpiSck = 12;
constexpr int8_t kSpiMiso = 13;
constexpr int8_t kRfidSs = 14;
constexpr int8_t kRfidReset = 15;
constexpr int8_t kServoSignal = 16;
constexpr int8_t kRs485Tx = 17;
constexpr int8_t kRs485Rx = 18;
constexpr int8_t kRgbData = 21;
constexpr int8_t kEncoderKey = 38;
constexpr int8_t kStepperIn1 = 39;
constexpr int8_t kStepperIn2 = 40;
constexpr int8_t kStepperIn3 = 41;
constexpr int8_t kStepperIn4 = 42;
constexpr int8_t kDcMotorIn1 = 47;
constexpr int8_t kDcMotorIn2 = 48;
}  // namespace PeripheralPins
