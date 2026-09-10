#include "RgbLedMatrix.h"

static_assert(RgbLedMatrix::limitedBrightness(0) == 0);
static_assert(RgbLedMatrix::limitedBrightness(20) == 20);
static_assert(RgbLedMatrix::limitedBrightness(25) == 25);
static_assert(RgbLedMatrix::limitedBrightness(255) == 25);
static_assert(!RgbLedMatrix::isCenteredYiPixel(0));
static_assert(!RgbLedMatrix::isCenteredYiPixel(3 * 8));
static_assert(RgbLedMatrix::isCenteredYiPixel(3 * 8 + 1));
static_assert(RgbLedMatrix::isCenteredYiPixel(3 * 8 + 6));
static_assert(!RgbLedMatrix::isCenteredYiPixel(3 * 8 + 7));
static_assert(RgbLedMatrix::isCenteredYiPixel(4 * 8 + 1));
static_assert(RgbLedMatrix::isCenteredYiPixel(4 * 8 + 6));
static_assert(!RgbLedMatrix::isCenteredYiPixel(5 * 8 + 3));

int main() { return 0; }
