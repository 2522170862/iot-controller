#include "DashboardView.h"
#include "JoystickInputDataSource.h"
#include "Rs485EnvironmentDataSource.h"
#include "WiFiDataSource.h"

DashboardView dashboard;
JoystickInputDataSource joystickDataSource;
Rs485EnvironmentDataSource dataSource;
WiFiDataSource wifiDataSource;

namespace {
constexpr uint32_t kRefreshIntervalMs = 500;
}

void setup() {
  Serial.begin(115200);
  dashboard.begin();
  dashboard.drawStaticLayout();
  joystickDataSource.begin();
  dataSource.begin();
  wifiDataSource.begin();
  EnvironmentData environment = dataSource.readEnvironment();
  joystickDataSource.readInto(&environment);
  dashboard.update(environment, wifiDataSource.readNetwork(), 0);

  Serial.println("ST7789 RS485 dashboard started");
}

void loop() {
  static uint32_t lastUpdateMs = 0;
  const uint32_t nowMs = millis();
  dataSource.poll(nowMs);
  wifiDataSource.poll(nowMs);

  if (nowMs - lastUpdateMs >= kRefreshIntervalMs) {
    lastUpdateMs = nowMs;
    EnvironmentData environment = dataSource.readEnvironment();
    joystickDataSource.readInto(&environment);
    dashboard.update(environment, wifiDataSource.readNetwork(), nowMs);
  }



  
}
