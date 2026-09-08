#include "DashboardView.h"
#include "JoystickInputDataSource.h"
#include "Rs485EnvironmentDataSource.h"
#include "StepperMotor.h"
#include "WiFiDataSource.h"

DashboardView dashboard;
JoystickInputDataSource joystickDataSource;
Rs485EnvironmentDataSource dataSource;
WiFiDataSource wifiDataSource;

namespace {
constexpr uint32_t kRefreshIntervalMs = 500;
constexpr uint8_t kStepperIn1Pin = 39;
constexpr uint8_t kStepperIn2Pin = 40;
constexpr uint8_t kStepperIn3Pin = 41;
constexpr uint8_t kStepperIn4Pin = 42;
constexpr uint32_t kStepperMoveIntervalMs = 10000;
constexpr float kStepperMoveDegrees = 20.0f;
constexpr float kStepperTargetDegrees = 60.0f;
}

StepperMotor stepperMotor(kStepperIn1Pin, kStepperIn2Pin, kStepperIn3Pin,
                          kStepperIn4Pin);
uint32_t nextStepperMoveMs = 0;
float stepperMovedDegrees = 0.0f;

void setup() {
  Serial.begin(115200);
  dashboard.begin();
  dashboard.drawStaticLayout();
  joystickDataSource.begin();
  dataSource.begin();
  wifiDataSource.begin();
  stepperMotor.begin();
  nextStepperMoveMs = millis() + kStepperMoveIntervalMs;
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
  stepperMotor.update(micros());

  if (stepperMovedDegrees < kStepperTargetDegrees &&
      static_cast<int32_t>(nowMs - nextStepperMoveMs) >= 0 &&
      !stepperMotor.isBusy()) {
    stepperMotor.moveDegrees(kStepperMoveDegrees);
    stepperMovedDegrees += kStepperMoveDegrees;
    nextStepperMoveMs += kStepperMoveIntervalMs;
  }

  if (nowMs - lastUpdateMs >= kRefreshIntervalMs) {
    lastUpdateMs = nowMs;
    EnvironmentData environment = dataSource.readEnvironment();
    joystickDataSource.readInto(&environment);
    dashboard.update(environment, wifiDataSource.readNetwork(), nowMs);
  }
}
