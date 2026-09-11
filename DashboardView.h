#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

#include "DashboardTypes.h"
#include "DashboardEventPolicy.h"
#include "PeripheralPins.h"

namespace DashboardConfig {
constexpr int8_t kLcdSclk = PeripheralPins::kSpiSck;
constexpr int8_t kLcdMosi = PeripheralPins::kSpiMosi;
constexpr int8_t kSpiMiso = PeripheralPins::kSpiMiso;
constexpr int8_t kLcdRst = PeripheralPins::kLcdReset;
constexpr int8_t kLcdDc = PeripheralPins::kLcdDc;
constexpr int8_t kLcdCs = PeripheralPins::kLcdCs;
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
  void notifyRfidReceived();

  static constexpr uint8_t gaugeSegmentsForPercent(float percent) {
    return percent <= 0.0f
               ? 0
               : (percent >= 100.0f
                      ? 20
                      : static_cast<uint8_t>(percent * 0.2f + 0.5f));
  }

 private:
  Adafruit_ST7789 display_;
  DashboardEventPolicy eventPolicy_;
  DashboardEventType drawnEventType_;
  bool pendingRfidEvent_;
  bool pageDrawn_;

  void drawHeader(const NetworkStatus& network);
  void drawOverviewPage();
  void drawEventPage(DashboardEventType type);
  void drawMetricCard(int16_t x, int16_t y, const char* label);
  void drawHumidityGauge(float humidityPercent);
  void drawCardValue(int16_t x, int16_t y, const char* value,
                     uint16_t color);
  void drawRow(int16_t y, const char* label, const char* value,
               uint16_t valueColor);
  void updateOverview(const EnvironmentData& environment,
                      const NetworkStatus& network);
  void updateEvent(const EnvironmentData& environment,
                   DashboardEventType type);
};
