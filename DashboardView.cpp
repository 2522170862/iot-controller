#include "DashboardView.h"

#include <math.h>
#include <stdio.h>

namespace {
constexpr uint16_t kBackground = 0x0862;
constexpr uint16_t kPanel = 0x10C4;
constexpr uint16_t kHeader = 0x18E7;
constexpr uint16_t kBorder = 0x296A;
constexpr uint16_t kText = ST77XX_WHITE;
constexpr uint16_t kMuted = 0x9CF3;
constexpr uint16_t kAccent = 0x3DFF;
constexpr uint16_t kWarm = 0xFD20;
constexpr uint16_t kGaugeInactive = 0x3186;
constexpr uint16_t kOnline = 0x3666;
constexpr uint16_t kOffline = 0xF986;
}  // namespace

DashboardView::DashboardView()
    : display_(&SPI, DashboardConfig::kLcdCs, DashboardConfig::kLcdDc,
               DashboardConfig::kLcdRst),
      drawnEventType_(DashboardEventType::kNone),
      pendingRfidEvent_(false),
      pageDrawn_(false) {}

void DashboardView::begin() {
  SPI.begin(DashboardConfig::kLcdSclk, DashboardConfig::kSpiMiso,
            DashboardConfig::kLcdMosi,
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
  display_.print(drawnEventType_ == DashboardEventType::kNone
                     ? "ENVIRONMENT"
                     : "EVENT DATA");

  display_.fillRoundRect(176, 7, 58, 26, 6,
                         network.connected ? kOnline : kOffline);
  display_.setTextSize(1);
  display_.setCursor(185, 16);
  display_.print(network.connected ? "WIFI OK" : "OFFLINE");
}

void DashboardView::drawOverviewPage() {
  display_.fillScreen(kBackground);

  display_.fillRoundRect(7, 48, 130, 88, 7, kPanel);
  display_.drawRoundRect(7, 48, 130, 88, 7, kBorder);
  display_.setTextSize(1);
  display_.setTextColor(kMuted);
  display_.setCursor(16, 58);
  display_.print("TEMPERATURE");

  display_.fillRoundRect(143, 48, 90, 88, 7, kPanel);
  display_.drawRoundRect(143, 48, 90, 88, 7, kBorder);
  display_.setCursor(157, 58);
  display_.print("HUMIDITY");

  drawMetricCard(7, 143, "PRESSURE");
  drawMetricCard(123, 143, "LIGHT");
  drawMetricCard(7, 213, "ALTITUDE");
  drawMetricCard(123, 213, "RS485");

  display_.fillRoundRect(7, 283, 226, 30, 6, kPanel);
  display_.drawRoundRect(7, 283, 226, 30, 6, kBorder);
}

void DashboardView::drawMetricCard(int16_t x, int16_t y,
                                   const char* label) {
  display_.fillRoundRect(x, y, 110, 63, 7, kPanel);
  display_.drawRoundRect(x, y, 110, 63, 7, kBorder);
  display_.setTextSize(1);
  display_.setTextColor(kMuted);
  display_.setCursor(x + 9, y + 9);
  display_.print(label);
}

void DashboardView::drawCardValue(int16_t x, int16_t y, const char* value,
                                  uint16_t color) {
  display_.fillRect(x, y, 94, 24, kPanel);
  display_.setTextSize(2);
  display_.setTextColor(color);
  display_.setCursor(x, y + 4);
  display_.print(value);
}

void DashboardView::drawHumidityGauge(float humidityPercent) {
  constexpr int16_t kCenterX = 188;
  constexpr int16_t kCenterY = 101;
  constexpr int16_t kRadius = 25;
  constexpr uint8_t kSegmentCount = 20;
  const uint8_t activeSegments = gaugeSegmentsForPercent(humidityPercent);

  display_.fillCircle(kCenterX, kCenterY, 32, kPanel);
  for (uint8_t segment = 0; segment < kSegmentCount; ++segment) {
    const float angle = (-225.0f + segment * (270.0f / 19.0f)) *
                        3.14159265f / 180.0f;
    const int16_t x = kCenterX + static_cast<int16_t>(cosf(angle) * kRadius);
    const int16_t y = kCenterY + static_cast<int16_t>(sinf(angle) * kRadius);
    display_.fillCircle(x, y, 3,
                        segment < activeSegments ? kAccent : kGaugeInactive);
  }

  char value[12];
  snprintf(value, sizeof(value), "%.0f%%", humidityPercent);
  display_.fillRect(166, 92, 45, 20, kPanel);
  display_.setTextSize(2);
  display_.setTextColor(kText);
  display_.setCursor(168, 95);
  display_.print(value);
}

void DashboardView::drawEventPage(DashboardEventType type) {
  display_.fillScreen(kBackground);
  const char* title = "EVENT";
  if (type == DashboardEventType::kJoystick) title = "JOYSTICK";
  else if (type == DashboardEventType::kMicrophone) title = "MICROPHONE";
  else if (type == DashboardEventType::kEncoder) title = "ENCODER";
  else if (type == DashboardEventType::kRfid) title = "RFID CARD";

  display_.setTextColor(kAccent);
  display_.setTextSize(2);
  display_.setCursor(12, 58);
  display_.print(title);
  for (uint8_t row = 0; row < 4; ++row) {
    const int16_t y = 96 + row * 48;
    display_.fillRoundRect(10, y, 220, 38, 6, kPanel);
    display_.drawRoundRect(10, y, 220, 38, 6, kBorder);
  }
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
  snprintf(value, sizeof(value), "%.1f C", data.temperatureC);
  display_.fillRect(15, 78, 114, 42, kPanel);
  display_.setTextSize(3);
  display_.setTextColor(kWarm);
  display_.setCursor(16, 85);
  display_.print(value);
  drawHumidityGauge(data.humidityPercent);

  snprintf(value, sizeof(value), "%.1f hPa", data.pressureHpa);
  drawCardValue(16, 171, value, kAccent);
  snprintf(value, sizeof(value), "%.0f lux", data.lightLux);
  drawCardValue(132, 171, value, kAccent);
  snprintf(value, sizeof(value), "%.0f m", data.altitudeM);
  drawCardValue(16, 241, value, kAccent);
  drawCardValue(132, 241, data.sensorConnected ? "LIVE" : "OFFLINE",
                data.sensorConnected ? kOnline : kOffline);

  display_.fillRect(14, 290, 212, 15, kPanel);
  display_.setTextSize(1);
  display_.setTextColor(network.connected ? kOnline : kOffline);
  display_.setCursor(14, 294);
  display_.print(network.connected ? "WIFI" : "OFF");
  display_.setTextColor(kText);
  display_.setCursor(51, 294);
  char ssid[11];
  snprintf(ssid, sizeof(ssid), "%.10s",
           network.ssid == nullptr ? "-" : network.ssid);
  display_.print(ssid);
  display_.setCursor(124, 294);
  display_.print(network.localIp == nullptr ? "-" : network.localIp);
}

void DashboardView::updateEvent(const EnvironmentData& data,
                                DashboardEventType type) {
  char value[32];
  if (type == DashboardEventType::kJoystick) {
    snprintf(value, sizeof(value), "%u", data.joystickX);
    drawRow(96, "X", value, kAccent);
    snprintf(value, sizeof(value), "%u", data.joystickY);
    drawRow(144, "Y", value, kAccent);
  } else if (type == DashboardEventType::kMicrophone) {
    snprintf(value, sizeof(value), "%.0f %%", data.microphonePercent);
    drawRow(96, "LEVEL", value, kAccent);
  } else if (type == DashboardEventType::kEncoder) {
    snprintf(value, sizeof(value), "%ld",
             static_cast<long>(data.encoderPosition));
    drawRow(96, "POSITION", value, kAccent);
    drawRow(144, "BUTTON", data.encoderPressed ? "PRESSED" : "RELEASED",
            kText);
  } else if (type == DashboardEventType::kRfid) {
    drawRow(96, "CARD UID", data.rfidCard == nullptr ? "NO CARD" : data.rfidCard,
            kText);
  }
}

void DashboardView::update(const EnvironmentData& environment,
                           const NetworkStatus& network, uint32_t nowMs) {
  eventPolicy_.detect(environment, pendingRfidEvent_, nowMs);
  pendingRfidEvent_ = false;
  const DashboardEventType eventType = eventPolicy_.type(nowMs);
  if (eventType != drawnEventType_) {
    drawnEventType_ = eventType;
    pageDrawn_ = false;
  }

  if (!pageDrawn_) {
    if (drawnEventType_ == DashboardEventType::kNone) {
      drawOverviewPage();
    } else {
      drawEventPage(drawnEventType_);
    }
    drawHeader(network);
    pageDrawn_ = true;
  }

  if (drawnEventType_ == DashboardEventType::kNone) {
    updateOverview(environment, network);
  } else {
    updateEvent(environment, drawnEventType_);
  }
}

void DashboardView::notifyRfidReceived() { pendingRfidEvent_ = true; }
