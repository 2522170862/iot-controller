#pragma once

#include <stdint.h>

#define OUTPUT 0x03
#define LOW 0x0
#define HIGH 0x1

inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
inline void analogWrite(uint8_t, int) {}
