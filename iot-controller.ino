#include "DashboardView.h"
#include "DcMotor.h"
#include "JoystickInputDataSource.h"
#include "MicrophoneInputDataSource.h"
#include "MqttMessageProtocol.h"
#include "MqttService.h"
#include "MqttTelemetrySchedule.h"
#include "ModuleCommandDispatcher.h"
#include "PeripheralPins.h"
#include "RelayController.h"
#include "RfidReader.h"
#include "RgbLedMatrix.h"
#include "RotaryEncoderInputDataSource.h"
#include "Rs485EnvironmentDataSource.h"
#include "ServoMotor.h"
#include "StepperMotor.h"
#include "WiFiDataSource.h"

DashboardView dashboard;
JoystickInputDataSource joystickDataSource;
MicrophoneInputDataSource microphoneInputDataSource;
RfidReader rfidReader(PeripheralPins::kRfidSs, PeripheralPins::kRfidReset);
RelayController relayController(PeripheralPins::kRelay1, PeripheralPins::kRelay2,
                                pinMode, digitalWrite);
RotaryEncoderInputDataSource rotaryEncoderDataSource;
Rs485EnvironmentDataSource dataSource;
WiFiDataSource wifiDataSource;
ServoMotor servoMotor(PeripheralPins::kServoSignal);
RgbLedMatrix rgbLedMatrix(PeripheralPins::kRgbData);
MqttService mqttService;
DcMotor dcMotor(PeripheralPins::kDcMotorIn1, PeripheralPins::kDcMotorIn2);

namespace {
constexpr uint32_t kRefreshIntervalMs = 500;
}

StepperMotor stepperMotor(
    PeripheralPins::kStepperIn1, PeripheralPins::kStepperIn2,
    PeripheralPins::kStepperIn3, PeripheralPins::kStepperIn4);
ModuleCommandDispatcher commandDispatcher(&relayController, &rgbLedMatrix,
                                          &servoMotor, &stepperMotor,
                                          &dcMotor);

namespace {
uint32_t mqttSequence = 0;
uint32_t lastEnvironmentTelemetryMs = 0;
int32_t lastEncoderPosition = 0;
bool lastEncoderPressed = false;

void makeHash(char hash[9]) { MqttMessageProtocol::generateHash(++mqttSequence, millis(), hash); }

void enqueueJson(const char* json, bool reply = false) {
  MqttMessage message = {};
  snprintf(message.topic, sizeof(message.topic), "LoTC2S/");
  snprintf(message.payload, sizeof(message.payload), "%s", json);
  message.isReply = reply;
  if (reply) mqttService.enqueueReply(message); else mqttService.enqueue(message);
}

void publishEnvironment(const EnvironmentData& data) {
  char hash[9] = {};
  makeHash(hash);
  char json[385] = {};
  snprintf(json, sizeof(json), "{\"hash\":\"%s\",\"type\":\"telemetry\",\"module\":\"environment\",\"temperature\":%.2f,\"humidity\":%.2f,\"pressure\":%.2f,\"light\":%.2f,\"altitude\":%.2f,\"microphone\":%.0f,\"online\":%s}", hash, data.temperatureC, data.humidityPercent, data.pressureHpa, data.lightLux, data.altitudeM, data.microphonePercent, data.sensorConnected ? "true" : "false");
  enqueueJson(json);
}

void publishEncoder(const EnvironmentData& data) {
  char hash[9] = {}; makeHash(hash);
  char json[180] = {};
  snprintf(json, sizeof(json), "{\"hash\":\"%s\",\"type\":\"telemetry\",\"module\":\"encoder\",\"position\":%ld,\"pressed\":%s}", hash, static_cast<long>(data.encoderPosition), data.encoderPressed ? "true" : "false");
  enqueueJson(json);
}

void publishRfid(const char* uid) {
  char hash[9] = {}; makeHash(hash);
  char json[180] = {};
  snprintf(json, sizeof(json), "{\"hash\":\"%s\",\"type\":\"telemetry\",\"module\":\"rfid\",\"uid\":\"%s\"}", hash, uid == nullptr ? "" : uid);
  enqueueJson(json);
}

void processCommand(const MqttMessage& message, const EnvironmentData& data) {
  MqttCommand command = {};
  const MqttParseResult parsed = MqttMessageProtocol::parseCommand(message.payload, &command);
  if (command.hash[0] == '\0') makeHash(command.hash);
  const CommandExecutionResult result = parsed == MqttParseResult::kOk ? commandDispatcher.dispatch(command) : CommandExecutionResult{false, "invalid_command"};
  char json[300] = {};
  if (command.kind == MqttCommandKind::kInputGet && result.ok) {
    snprintf(json, sizeof(json), "{\"hash\":\"%s\",\"type\":\"reply\",\"ok\":true,\"module\":\"input\",\"x\":%u,\"y\":%u,\"joystickPressed\":%s,\"encoderPosition\":%ld,\"encoderPressed\":%s}", command.hash, data.joystickX, data.joystickY, data.joystickPressed ? "true" : "false", static_cast<long>(data.encoderPosition), data.encoderPressed ? "true" : "false");
  } else {
    snprintf(json, sizeof(json), "{\"hash\":\"%s\",\"type\":\"reply\",\"ok\":%s,\"state\":\"%s\"}", command.hash, result.ok ? "true" : "false", result.stateOrError);
  }
  enqueueJson(json, true);
}
}

void setup() {
  Serial.begin(115200);
  relayController.begin();
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
  mqttService.begin();
  stepperMotor.begin();
  dcMotor.begin();
  dcMotor.startStartupDemo(millis());
  Serial.println("DC motor startup demo: forward, stop, reverse, stop");
  rgbLedMatrix.begin();
  Serial.println("WS2812B matrix shows a centered red Yi character");
  if (servoMotor.begin(90)) {
    Serial.println("MG90S moved to 90 degrees");
  }
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
  mqttService.poll(nowMs, wifiDataSource.readNetwork().connected);
  microphoneInputDataSource.poll(micros());
  rotaryEncoderDataSource.poll();
  stepperMotor.update(micros());
  dcMotor.update(nowMs);

  if (rfidReader.poll()) {
    dashboard.notifyRfidReceived();
    Serial.print("RFID card UID: ");
    Serial.println(rfidReader.cardUid());
    publishRfid(rfidReader.cardUid());
  }

  if (nowMs - lastUpdateMs >= kRefreshIntervalMs) {
    lastUpdateMs = nowMs;
    EnvironmentData environment = dataSource.readEnvironment();
    joystickDataSource.readInto(&environment);
    microphoneInputDataSource.readInto(&environment);
    environment.rfidCard = rfidReader.cardUid();
    rotaryEncoderDataSource.readInto(&environment);
    MqttMessage message = {};
    while (mqttService.takeIncoming(&message)) processCommand(message, environment);
    if (MqttTelemetrySchedule::environmentDue(nowMs, lastEnvironmentTelemetryMs)) {
      publishEnvironment(environment);
      lastEnvironmentTelemetryMs = nowMs;
    }
    if (MqttTelemetrySchedule::encoderChanged(environment.encoderPosition, environment.encoderPressed, lastEncoderPosition, lastEncoderPressed)) {
      publishEncoder(environment);
      lastEncoderPosition = environment.encoderPosition;
      lastEncoderPressed = environment.encoderPressed;
    }
    dashboard.update(environment, wifiDataSource.readNetwork(), nowMs);
  }
}
