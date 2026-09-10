#include "DashboardView.h"
#include "JoystickInputDataSource.h"
#include "MicrophoneInputDataSource.h"
#include "PeripheralPins.h"
#include "RfidReader.h"
#include "RotaryEncoderInputDataSource.h"
#include "Rs485EnvironmentDataSource.h"
#include "StepperMotor.h"
#include "WiFiDataSource.h"

DashboardView dashboard;
JoystickInputDataSource joystickDataSource;
MicrophoneInputDataSource microphoneInputDataSource;
RfidReader rfidReader(PeripheralPins::kRfidSs, PeripheralPins::kRfidReset);
RotaryEncoderInputDataSource rotaryEncoderDataSource;
Rs485EnvironmentDataSource dataSource;
WiFiDataSource wifiDataSource;

namespace {
constexpr uint32_t kRefreshIntervalMs = 500;
constexpr uint32_t kStepperMoveIntervalMs = 10000;
constexpr float kStepperMoveDegrees = 20.0f;
constexpr float kStepperTargetDegrees = 60.0f;
}

StepperMotor stepperMotor(
    PeripheralPins::kStepperIn1, PeripheralPins::kStepperIn2,
    PeripheralPins::kStepperIn3, PeripheralPins::kStepperIn4);
uint32_t nextStepperMoveMs = 0;
float stepperMovedDegrees = 0.0f;

void setup() {
  Serial.begin(115200);
  pinMode(PeripheralPins::kLcdCs, OUTPUT);
  digitalWrite(PeripheralPins::kLcdCs, HIGH);
  pinMode(PeripheralPins::kRfidSs, OUTPUT);
  digitalWrite(PeripheralPins::kRfidSs, HIGH);
  dashboard.begin();
  dashboard.drawStaticLayout();
  rfidReader.begin();
  joystickDataSource.begin();
  microphoneInputDataSource.begin();
  rotaryEncoderDataSource.begin();
  dataSource.begin();
  wifiDataSource.begin();
  stepperMotor.begin();
  nextStepperMoveMs = millis() + kStepperMoveIntervalMs;
  EnvironmentData environment = dataSource.readEnvironment();
  joystickDataSource.readInto(&environment);
  microphoneInputDataSource.readInto(&environment);
  environment.rfidCard = rfidReader.cardUid();
  rotaryEncoderDataSource.readInto(&environment);
  dashboard.update(environment, wifiDataSource.readNetwork(), 0);

  Serial.println("ST7789 RS485 dashboard started");
}

void loop() {
  static uint32_t lastUpdateMs = 0;
  const uint32_t nowMs = millis();
  dataSource.poll(nowMs);
  wifiDataSource.poll(nowMs);
  microphoneInputDataSource.poll(micros());
  rotaryEncoderDataSource.poll();
  stepperMotor.update(micros());

  if (rfidReader.poll()) {
    Serial.print("RFID card UID: ");
    Serial.println(rfidReader.cardUid());
  }

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
    microphoneInputDataSource.readInto(&environment);
    environment.rfidCard = rfidReader.cardUid();
    rotaryEncoderDataSource.readInto(&environment);
    dashboard.update(environment, wifiDataSource.readNetwork(), nowMs);
  }
}
