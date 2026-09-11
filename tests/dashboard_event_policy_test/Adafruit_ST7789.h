#pragma once

#include <stdint.h>

#include "SPI.h"

constexpr uint16_t ST77XX_WHITE = 0xFFFF;

class Adafruit_ST7789 {
 public:
  Adafruit_ST7789(SPIClass*, int8_t, int8_t, int8_t) {}
  void init(uint16_t, uint16_t) {}
  void setRotation(uint8_t) {}
  void setSPISpeed(uint32_t) {}
  void setTextWrap(bool) {}
  void fillRect(int16_t, int16_t, int16_t, int16_t, uint16_t) {}
  void fillRoundRect(int16_t, int16_t, int16_t, int16_t, int16_t,
                     uint16_t) {}
  void drawRoundRect(int16_t, int16_t, int16_t, int16_t, int16_t,
                     uint16_t) {}
  void fillScreen(uint16_t) {}
  void fillCircle(int16_t, int16_t, int16_t, uint16_t) {}
  void setTextColor(uint16_t) {}
  void setTextSize(uint8_t) {}
  void setCursor(int16_t, int16_t) {}
  void print(const char*) {}
};
