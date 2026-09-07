#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

#include "DashboardTypes.h"

namespace DashboardConfig {
constexpr int8_t kLcdSclk = 12;
constexpr int8_t kLcdMosi = 11;
constexpr int8_t kLcdRst = 10;
constexpr int8_t kLcdDc = 9;
constexpr int8_t kLcdCs = 8;
constexpr uint16_t kWidth = 240;
constexpr uint16_t kHeight = 320;
constexpr uint8_t kRotation = 0;
}  // namespace DashboardConfig

class DashboardView {
 public:
  DashboardView();

  void begin();
  void drawStaticLayout();
  void update(const EnvironmentData& environment,
              const NetworkStatus& network, uint32_t nowMs);

 private:
  Adafruit_ST7789 display_;
  uint8_t currentPage_;
  uint32_t lastPageSwitchMs_;
  bool pageDrawn_;

  void drawHeader(const NetworkStatus& network);
  void drawOverviewPage();
  void drawInputPage();
  void drawRow(int16_t y, const char* label, const char* value,
               uint16_t valueColor);
  void updateOverview(const EnvironmentData& environment,
                      const NetworkStatus& network);
  void updateInputs(const EnvironmentData& environment);
};
