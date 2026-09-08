#include "DashboardView.h"
#include "MockDataSource.h"
#include "WiFiDataSource.h"

DashboardView dashboard;
MockDataSource dataSource;
WiFiDataSource wifiDataSource;

namespace {
constexpr uint32_t kRefreshIntervalMs = 500;
}

void setup() {
  Serial.begin(115200);
  dashboard.begin();
  dashboard.drawStaticLayout();
  wifiDataSource.begin();
  dashboard.update(dataSource.readEnvironment(0), wifiDataSource.readNetwork(), 0);

  Serial.println("ST7789 virtual dashboard started");
}

void loop() {
  static uint32_t lastUpdateMs = 0;
  const uint32_t nowMs = millis();
  wifiDataSource.poll(nowMs);

  if (nowMs - lastUpdateMs >= kRefreshIntervalMs) {
    lastUpdateMs = nowMs;
    dashboard.update(dataSource.readEnvironment(nowMs),
                     wifiDataSource.readNetwork(), nowMs);
  }



  
}
