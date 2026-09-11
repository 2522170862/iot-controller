#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

class RgbLedMatrix {
 public:
  static constexpr uint16_t kPixelCount = 64;
  static constexpr uint8_t kMaximumBrightness = 25;

  explicit RgbLedMatrix(uint8_t dataPin);

  void begin();
  void fillWhite(uint8_t brightness = kMaximumBrightness);
  void fillColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness);
  void showCenteredYi();
  void clear();

  static constexpr uint8_t limitedBrightness(uint8_t brightness) {
    return brightness > kMaximumBrightness ? kMaximumBrightness : brightness;
  }

  static constexpr bool isCenteredYiPixel(uint16_t pixelIndex) {
    const uint8_t row = pixelIndex / 8;
    const uint8_t column = pixelIndex % 8;
    return (row == 3 || row == 4) && column >= 1 && column <= 6;
  }

 private:
  Adafruit_NeoPixel pixels_;
};
