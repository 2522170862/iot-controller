#include "DashboardView.h"

#include <stdio.h>

namespace {
constexpr uint16_t kBackground = 0x0862;
constexpr uint16_t kPanel = 0x10C4;
constexpr uint16_t kHeader = 0x18E7;
constexpr uint16_t kBorder = 0x296A;
constexpr uint16_t kText = ST77XX_WHITE;
constexpr uint16_t kMuted = 0x9CF3;
constexpr uint16_t kAccent = 0x3DFF;
constexpr uint16_t kOnline = 0x3666;
constexpr uint16_t kOffline = 0xF986;
constexpr uint32_t kPageIntervalMs = 4000;
constexpr int16_t kFirstRowY = 48;
constexpr int16_t kRowHeight = 29;
}  // namespace

DashboardView::DashboardView()
    : display_(&SPI, DashboardConfig::kLcdCs, DashboardConfig::kLcdDc,
               DashboardConfig::kLcdRst),
      currentPage_(0),
      lastPageSwitchMs_(0),
      pageDrawn_(false) {}

void DashboardView::begin() {
  SPI.begin(DashboardConfig::kLcdSclk, -1, DashboardConfig::kLcdMosi,
            DashboardConfig::kLcdCs);
  display_.init(DashboardConfig::kWidth, DashboardConfig::kHeight);
  display_.setRotation(DashboardConfig::kRotation);
  display_.setSPISpeed(40000000);
  display_.setTextWrap(false);
}

void DashboardView::drawStaticLayout() {
  pageDrawn_ = false;
}

void DashboardView::drawHeader(const NetworkStatus& network) {
  display_.fillRect(0, 0, 240, 40, kHeader);
  display_.setTextColor(kText);
  display_.setTextSize(2);
  display_.setCursor(10, 12);
  display_.print(currentPage_ == 0 ? "IOT STATUS" : "INPUT / RFID");

  display_.fillRoundRect(176, 7, 58, 26, 6,
                         network.connected ? kOnline : kOffline);
  display_.setTextSize(1);
  display_.setCursor(185, 16);
  display_.print(network.connected ? "WIFI OK" : "OFFLINE");
}

void DashboardView::drawOverviewPage() {
  display_.fillScreen(kBackground);
  for (uint8_t row = 0; row < 9; ++row) {
    const int16_t y = kFirstRowY + row * kRowHeight;
    display_.fillRoundRect(7, y, 226, 24, 4, kPanel);
    display_.drawRoundRect(7, y, 226, 24, 4, kBorder);
  }
}

void DashboardView::drawInputPage() {
  display_.fillScreen(kBackground);
  for (uint8_t row = 0; row < 5; ++row) {
    const int16_t y = 58 + row * 48;
    display_.fillRoundRect(10, y, 220, 38, 6, kPanel);
    display_.drawRoundRect(10, y, 220, 38, 6, kBorder);
  }
  display_.setTextColor(kMuted);
  display_.setTextSize(1);
  display_.setCursor(52, 304);
  display_.print("RS485 sensor data");
}

void DashboardView::drawRow(int16_t y, const char* label, const char* value,
                            uint16_t valueColor) {
  display_.fillRect(14, y + 5, 212, 14, kPanel);
  display_.setTextSize(1);
  display_.setTextColor(kMuted);
  display_.setCursor(14, y + 8);
  display_.print(label);
  display_.setTextColor(valueColor);
  display_.setCursor(92, y + 8);
  display_.print(value);
}

void DashboardView::updateOverview(const EnvironmentData& data,
                                   const NetworkStatus& network) {
  char value[48];
  drawRow(kFirstRowY + 0 * kRowHeight, "DEVICE", "ESP32-S3-01", kText);
  drawRow(kFirstRowY + 1 * kRowHeight, "SSID",
          network.ssid == nullptr ? "-" : network.ssid, kText);
  drawRow(kFirstRowY + 2 * kRowHeight, "LOCAL IP",
          network.localIp == nullptr ? "-" : network.localIp, kText);
  drawRow(kFirstRowY + 3 * kRowHeight, "SERVER IP",
          network.mqttHost == nullptr ? "NOT SET" : network.mqttHost, kText);
  snprintf(value, sizeof(value), "%.1f C", data.temperatureC);
  drawRow(kFirstRowY + 4 * kRowHeight, "TEMP", value, kAccent);
  snprintf(value, sizeof(value), "%.1f %%", data.humidityPercent);
  drawRow(kFirstRowY + 5 * kRowHeight, "HUMIDITY", value, kAccent);
  snprintf(value, sizeof(value), "%.1f hPa", data.pressureHpa);
  drawRow(kFirstRowY + 6 * kRowHeight, "PRESSURE", value, kAccent);
  snprintf(value, sizeof(value), "%.0f lux", data.lightLux);
  drawRow(kFirstRowY + 7 * kRowHeight, "LIGHT", value, kAccent);
  snprintf(value, sizeof(value), "%.0f m", data.altitudeM);
  drawRow(kFirstRowY + 8 * kRowHeight, "ALTITUDE", value, kAccent);
}

void DashboardView::updateInputs(const EnvironmentData& data) {
  char value[32];
  snprintf(value, sizeof(value), "%u", data.joystickX);
  drawRow(58, "JOYSTICK X", value, kAccent);
  snprintf(value, sizeof(value), "%u", data.joystickY);
  drawRow(106, "JOYSTICK Y", value, kAccent);
  snprintf(value, sizeof(value), "%d", data.encoderDelta);
  drawRow(154, "ENCODER", value, kAccent);
  drawRow(202, "RFID CARD", data.rfidCard == nullptr ? "NO CARD" : data.rfidCard,
          kText);
  drawRow(250, "SENSOR", data.sensorConnected ? "ONLINE" : "OFFLINE",
          data.sensorConnected ? kOnline : kOffline);
}

void DashboardView::update(const EnvironmentData& environment,
                           const NetworkStatus& network, uint32_t nowMs) {
  if (pageDrawn_ && nowMs - lastPageSwitchMs_ >= kPageIntervalMs) {
    currentPage_ = (currentPage_ + 1) % 2;
    lastPageSwitchMs_ = nowMs;
    pageDrawn_ = false;
  }

  if (!pageDrawn_) {
    if (currentPage_ == 0) {
      drawOverviewPage();
    } else {
      drawInputPage();
    }
    drawHeader(network);
    pageDrawn_ = true;
  }

  if (currentPage_ == 0) {
    updateOverview(environment, network);
  } else {
    updateInputs(environment);
  }
}
