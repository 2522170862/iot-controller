#pragma once

#include <stdint.h>

// Only the hardware MAC read is replaced; identity formatting stays real.
class EspClass {
 public:
  inline static uint64_t mac = 0x11223344C398ULL;
  uint64_t getEfuseMac() const { return mac; }
};

inline EspClass ESP;
