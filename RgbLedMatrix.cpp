#include "RgbLedMatrix.h"

RgbLedMatrix::RgbLedMatrix(uint8_t dataPin)
    : pixels_(kPixelCount, dataPin, NEO_GRB + NEO_KHZ800) {}

void RgbLedMatrix::begin() {
  pixels_.begin();
  showCenteredYi();
}

void RgbLedMatrix::fillWhite(uint8_t brightness) {
  pixels_.setBrightness(limitedBrightness(brightness));
  pixels_.fill(pixels_.Color(255, 255, 255));
  pixels_.show();
}

void RgbLedMatrix::fillColor(uint8_t red, uint8_t green, uint8_t blue,
                              uint8_t brightness) {
  pixels_.setBrightness(limitedBrightness(brightness));
  pixels_.fill(pixels_.Color(red, green, blue));
  pixels_.show();
}

void RgbLedMatrix::showCenteredYi() {
  pixels_.setBrightness(kMaximumBrightness);
  const uint32_t dimWhite = pixels_.Color(20, 20, 20);
  const uint32_t red = pixels_.Color(255, 0, 0);

  for (uint16_t pixelIndex = 0; pixelIndex < kPixelCount; ++pixelIndex) {
    pixels_.setPixelColor(
        pixelIndex, isCenteredYiPixel(pixelIndex) ? red : dimWhite);
  }
  pixels_.show();
}

void RgbLedMatrix::clear() {
  pixels_.clear();
  pixels_.show();
}
