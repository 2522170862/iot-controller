#include "DashboardView.h"

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ChineseFont16.h"

namespace {
constexpr uint16_t kBackground = 0x0862;
constexpr uint16_t kPanel = 0x10C4;
constexpr uint16_t kHeader = 0x18E7;
constexpr uint16_t kBorder = 0x3B4D;
constexpr uint16_t kText = ST77XX_WHITE;
constexpr uint16_t kMuted = 0xBDF7;
constexpr uint16_t kAccent = 0x3DFF;
constexpr uint16_t kWarm = 0xFD20;
constexpr uint16_t kOnline = 0x3666;
constexpr uint16_t kOffline = 0xF986;
constexpr uint16_t kTrack = 0x294A;
constexpr int16_t kPageX = 7;
constexpr int16_t kPageWidth = DashboardView::overviewContentWidth();

void printRightAligned(Adafruit_ST7789& display, const char* text,
                       int16_t rightX, int16_t baselineY,
                       const GFXfont* font, uint16_t color) {
  int16_t boundsX = 0;
  int16_t boundsY = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  display.setFont(font);
  display.setTextSize(1);
  display.getTextBounds(text, 0, baselineY, &boundsX, &boundsY, &width,
                        &height);
  display.setTextColor(color);
  display.setCursor(rightX - boundsX - static_cast<int16_t>(width),
                    baselineY);
  display.print(text);
}

void printCentered(Adafruit_ST7789& display, const char* text,
                   int16_t centerX, int16_t baselineY,
                   const GFXfont* font, uint16_t color) {
  int16_t boundsX = 0;
  int16_t boundsY = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  display.setFont(font);
  display.setTextSize(1);
  display.getTextBounds(text, 0, baselineY, &boundsX, &boundsY, &width,
                        &height);
  display.setTextColor(color);
  display.setCursor(centerX - boundsX - static_cast<int16_t>(width) / 2,
                    baselineY);
  display.print(text);
}

void drawChineseText(Adafruit_ST7789& display, int16_t x, int16_t y,
                     const char* text, uint16_t color) {
  while (text != nullptr && *text != '\0') {
    const uint16_t codepoint = ChineseFont16::nextCodepoint(text);
    const uint8_t* bitmap = ChineseFont16::glyph(codepoint);
    if (bitmap != nullptr) {
      display.drawBitmap(x, y, bitmap, ChineseFont16::kWidth,
                         ChineseFont16::kHeight, color);
    }
    x += ChineseFont16::kWidth;
  }
}
}  // namespace

DashboardView::DashboardView()
    : display_(&SPI, DashboardConfig::kLcdCs, DashboardConfig::kLcdDc,
               DashboardConfig::kLcdRst),
      drawnEventType_(DashboardEventType::kNone),
      pendingRfidEvent_(false),
      pageDrawn_(false),
      networkBadgeDrawn_(false),
      displayedNetworkConnected_(false),
      lastRenderMs_(0) {}

void DashboardView::begin() {
  SPI.begin(DashboardConfig::kLcdSclk, DashboardConfig::kSpiMiso,
            DashboardConfig::kLcdMosi, DashboardConfig::kLcdCs);
  display_.init(DashboardConfig::kWidth, DashboardConfig::kHeight);
  display_.setRotation(DashboardConfig::kRotation);
  display_.setSPISpeed(40000000);
  display_.setTextWrap(false);
}

void DashboardView::drawStaticLayout() { pageDrawn_ = false; }

void DashboardView::drawHeader(const NetworkStatus& network) {
  display_.fillRect(0, 0, 240, 34, kHeader);
  display_.setFont(nullptr);
  display_.setTextSize(1);
  display_.setTextColor(kText);
  display_.setCursor(10, 13);
  display_.print("WiFi");
  drawChineseText(display_, 38, 8, "状态", kText);
  drawNetworkBadge(network);
}

void DashboardView::drawNetworkBadge(const NetworkStatus& network) {
  display_.fillRect(170, 2, 68, 29, kHeader);
  display_.fillRoundRect(174, 4, 60, 24, 6,
                         network.connected ? kOnline : kOffline);
  drawChineseText(display_, network.connected ? 188 : 180, 8,
                  network.connected ? "连接" : "未连接", kText);
  displayedNetworkConnected_ = network.connected;
  networkBadgeDrawn_ = true;
}

void DashboardView::drawOverviewPage() {
  display_.fillScreen(kBackground);
  drawMetricCard(7, 40, 130, 62, "温度");
  drawMetricCard(143, 40, 90, 62, "湿度");
  drawMetricCard(kPageX, 108, DashboardView::pressureCardWidth(), 48,
                 "气压");
  drawMetricCard(7, 162, 110, 48, "光照");
  drawMetricCard(123, 162, 110, 48, "海拔");
  drawMetricCard(kPageX, 216, kPageWidth, 42, "麦克风");
  display_.fillRoundRect(kPageX, 264, kPageWidth, 49, 6, kPanel);
  display_.drawRoundRect(kPageX, 264, kPageWidth, 49, 6, kBorder);
}

void DashboardView::drawMetricCard(int16_t x, int16_t y, int16_t width,
                                   int16_t height, const char* label) {
  display_.fillRoundRect(x, y, width, height, 7, kPanel);
  display_.drawRoundRect(x, y, width, height, 7, kBorder);
  drawChineseText(display_, x + 9, y + 7, label, kMuted);
}

void DashboardView::drawHumidityGauge(float humidityPercent) {
  const float safeHumidity =
      !isfinite(humidityPercent)
          ? 0.0f
          : (humidityPercent < 0.0f
                 ? 0.0f
                 : (humidityPercent > 100.0f ? 100.0f : humidityPercent));
  // Keep the changing value below the 16-pixel label (y=47..62).
  display_.fillRect(151, 65, 74, 32, kPanel);
  char value[12];
  snprintf(value, sizeof(value), "%.0f%%", safeHumidity);
  printCentered(display_, value, 188, 87, &FreeSansBold12pt7b, kText);
  display_.fillRoundRect(157, 93, 62, 4, 2, kTrack);
  const int16_t filled = static_cast<int16_t>(safeHumidity * 62.0f / 100.0f);
  if (filled > 0) display_.fillRect(157, 93, filled, 4, kAccent);
}

void DashboardView::drawProgressBar(int16_t x, int16_t y, int16_t width,
                                    uint8_t percent, uint16_t color) {
  const uint8_t limited = percent > 100 ? 100 : percent;
  display_.fillRoundRect(x, y, width, 12, 5, kTrack);
  const int16_t filled = static_cast<int16_t>(width * limited / 100);
  if (filled >= 10) {
    display_.fillRoundRect(x, y, filled, 12, 5, color);
  } else if (filled > 0) {
    display_.fillRect(x, y, filled, 12, color);
  }
}

void DashboardView::drawEventPage(DashboardEventType type) {
  display_.fillScreen(kBackground);
  const char* title = "事件";
  if (type == DashboardEventType::kJoystick) title = "摇杆";
  else if (type == DashboardEventType::kEncoder) title = "编码器";
  else if (type == DashboardEventType::kRfid) title = "射频卡";

  drawChineseText(display_, 12, 52, title, kAccent);

  if (type == DashboardEventType::kJoystick) {
    drawMetricCard(10, 84, 220, 78, "横轴");
    drawMetricCard(10, 174, 220, 78, "纵轴");
  } else if (type == DashboardEventType::kEncoder) {
    drawMetricCard(10, 84, 220, 116, "位置");
    drawMetricCard(10, 210, 220, 54, "按键");
  } else if (type == DashboardEventType::kRfid) {
    drawMetricCard(10, 94, 220, 130, "卡号");
  }
}

void DashboardView::updateOverview(const EnvironmentData& data,
                                   const NetworkStatus& network) {
  char value[48];
  if (!isfinite(data.temperatureC)) snprintf(value, sizeof(value), "--.-");
  else if (data.temperatureC > 999.0f) snprintf(value, sizeof(value), "HIGH");
  else if (data.temperatureC < -99.0f) snprintf(value, sizeof(value), "LOW");
  else snprintf(value, sizeof(value), "%.1f", data.temperatureC);
  display_.fillRect(15, 65, 114, 32, kPanel);
  printRightAligned(display_, value, 103, 94, &FreeSansBold12pt7b, kWarm);
  display_.setFont(&FreeSans9pt7b);
  display_.setTextColor(kMuted);
  display_.setCursor(109, 93);
  display_.print("C");
  drawHumidityGauge(data.humidityPercent);

  if (!isfinite(data.pressureHpa)) snprintf(value, sizeof(value), "--.-");
  else if (data.pressureHpa > 9999.9f) snprintf(value, sizeof(value), "HIGH");
  else if (data.pressureHpa < -999.9f) snprintf(value, sizeof(value), "LOW");
  else snprintf(value, sizeof(value), "%.1f", data.pressureHpa);
  display_.fillRect(15, 131, 210, 21, kPanel);
  printRightAligned(display_, value, 154, 149, &FreeSansBold12pt7b,
                    kAccent);
  display_.setFont(&FreeSans9pt7b);
  display_.setTextColor(kMuted);
  display_.setCursor(166, 149);
  display_.print("hPa");

  if (!isfinite(data.lightLux)) snprintf(value, sizeof(value), "--");
  else if (data.lightLux > 99999.0f) snprintf(value, sizeof(value), "HIGH");
  else if (data.lightLux < 0.0f) snprintf(value, sizeof(value), "LOW");
  else snprintf(value, sizeof(value), "%.0f", data.lightLux);
  display_.fillRect(15, 185, 94, 22, kPanel);
  printRightAligned(display_, value, 78, 203, &FreeSansBold12pt7b, kAccent);
  display_.setFont(&FreeSans9pt7b);
  display_.setTextColor(kMuted);
  display_.setCursor(84, 203);
  display_.print("lux");

  if (!isfinite(data.altitudeM)) snprintf(value, sizeof(value), "--");
  else if (data.altitudeM > 9999.0f) snprintf(value, sizeof(value), "0");
  else if (data.altitudeM < -999.0f) snprintf(value, sizeof(value), "LOW");
  else snprintf(value, sizeof(value), "%.0f", data.altitudeM);
  display_.fillRect(131, 185, 94, 22, kPanel);
  printRightAligned(display_, value, 199, 203, &FreeSansBold12pt7b,
                    kAccent);
  display_.setFont(&FreeSans9pt7b);
  display_.setTextColor(kMuted);
  display_.setCursor(207, 203);
  display_.print("m");

  const float microphonePercent =
      !isfinite(data.microphonePercent)
          ? 0.0f
          : (data.microphonePercent < 0.0f
                 ? 0.0f
                 : (data.microphonePercent > 100.0f ? 100.0f
                                                    : data.microphonePercent));
  snprintf(value, sizeof(value), "%.0f%%", microphonePercent);
  // Right-aligned text can extend left of x=96 when changing from 100% to 9%.
  // Clear the entire value field before drawing the new number.
  display_.fillRect(66, 230, 74, 24, kPanel);
  printRightAligned(display_, value, 136, 250, &FreeSansBold12pt7b, kAccent);
  drawProgressBar(146, 239, 72, static_cast<uint8_t>(microphonePercent),
                  kAccent);

  display_.fillRect(14, 268, 212, 39, kPanel);
  drawChineseText(display_, 14, 266, "设备编号", kText);
  display_.setFont(nullptr);
  display_.setTextSize(1);
  display_.setTextColor(kText);
  display_.setCursor(88, 270);
  display_.print("NO.001");
  drawChineseText(display_, 14, 290, "服务器", kText);
  display_.setCursor(66, 294);
  char mqttServer[32];
  snprintf(mqttServer, sizeof(mqttServer), "%s",
           network.mqttHost == nullptr ? "-" : network.mqttHost);
  display_.print(mqttServer);
}

void DashboardView::updateEvent(const EnvironmentData& data,
                                DashboardEventType type) {
  char value[40];
  if (type == DashboardEventType::kJoystick) {
    const uint8_t xPercent = static_cast<uint8_t>(
        data.joystickX >= 4095 ? 100 : data.joystickX * 100UL / 4095UL);
    const uint8_t yPercent = static_cast<uint8_t>(
        data.joystickY >= 4095 ? 100 : data.joystickY * 100UL / 4095UL);
    snprintf(value, sizeof(value), "%u", data.joystickX);
    display_.fillRect(22, 112, 196, 42, kPanel);
    display_.setFont(&FreeSansBold12pt7b);
    display_.setTextSize(1);
    display_.setTextColor(kText);
    display_.setCursor(22, 137);
    display_.print(value);
    drawProgressBar(22, 144, 190, xPercent, kAccent);

    snprintf(value, sizeof(value), "%u", data.joystickY);
    display_.fillRect(22, 202, 196, 42, kPanel);
    display_.setFont(&FreeSansBold12pt7b);
    display_.setCursor(22, 227);
    display_.print(value);
    drawProgressBar(22, 234, 190, yPercent, kAccent);
  } else if (type == DashboardEventType::kEncoder) {
    snprintf(value, sizeof(value), "%ld",
             static_cast<long>(data.encoderPosition));
    display_.fillRect(24, 122, 192, 62, kPanel);
    display_.setFont(&FreeSansBold12pt7b);
    display_.setTextSize(1);
    display_.setTextColor(kAccent);
    printRightAligned(display_, value, 207, 166, &FreeSansBold12pt7b,
                      kAccent);

    display_.fillRoundRect(95, 222, 120, 32, 8,
                           data.encoderPressed ? kOnline : kTrack);
    drawChineseText(display_, 139, 230,
                    data.encoderPressed ? "按下" : "松开", kText);
  } else if (type == DashboardEventType::kRfid) {
    const char* uid = data.rfidCard == nullptr ? "NO CARD" : data.rfidCard;
    char boundedUid[29];
    snprintf(boundedUid, sizeof(boundedUid), "%.28s", uid);
    display_.fillRect(18, 128, 204, 72, kPanel);
    constexpr size_t kCharactersPerLine = 14;
    if (strlen(boundedUid) <= kCharactersPerLine) {
      printCentered(display_, boundedUid, 120, 174, &FreeSansBold12pt7b,
                    kText);
    } else {
      char firstLine[kCharactersPerLine + 1] = {};
      char secondLine[kCharactersPerLine + 1] = {};
      memcpy(firstLine, boundedUid, kCharactersPerLine);
      snprintf(secondLine, sizeof(secondLine), "%.14s",
               boundedUid + kCharactersPerLine);
      printCentered(display_, firstLine, 120, 158, &FreeSansBold12pt7b,
                    kText);
      printCentered(display_, secondLine, 120, 190, &FreeSansBold12pt7b,
                    kText);
    }
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

  if (pageDrawn_ && networkBadgeRefreshDue(
                        networkBadgeDrawn_, displayedNetworkConnected_,
                        network.connected)) {
    drawNetworkBadge(network);
  }

  if (!renderDue(nowMs, lastRenderMs_, pageDrawn_)) return;

  if (!pageDrawn_) {
    if (drawnEventType_ == DashboardEventType::kNone) drawOverviewPage();
    else drawEventPage(drawnEventType_);
    drawHeader(network);
    pageDrawn_ = true;
  }

  if (drawnEventType_ == DashboardEventType::kNone) {
    updateOverview(environment, network);
  } else {
    updateEvent(environment, drawnEventType_);
  }
  lastRenderMs_ = nowMs;
}

void DashboardView::notifyRfidReceived() { pendingRfidEvent_ = true; }
