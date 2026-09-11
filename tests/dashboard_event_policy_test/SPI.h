#pragma once

#include <stdint.h>

class SPIClass {
 public:
  void begin(int8_t, int8_t, int8_t, int8_t) {}
};

extern SPIClass SPI;

