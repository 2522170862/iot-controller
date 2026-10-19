#include "BleProvisioningService.h"
#include "DashboardView.h"
#include "ConnectionStatusIndicators.h"
#include "DcMotor.h"
#include "JoystickInputDataSource.h"
#include "MicrophoneInputDataSource.h"
#include "MqttMessageProtocol.h"
#include "MqttService.h"
#include "MqttConfigurationStore.h"
#include "MqttProvisioningCoordinator.h"
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
#include "WifiCredentialStore.h"
#include "WifiCredentials.h"
#include "WifiProvisioningCoordinator.h"

DashboardView dashboard;
JoystickInputDataSource joystickDataSource;
MicrophoneInputDataSource microphoneInputDataSource;
RfidReader rfidReader(PeripheralPins::kRfidSs, PeripheralPins::kRfidReset);
RelayController relayController(PeripheralPins::kRelay1, PeripheralPins::kRelay2,
                                pinMode, digitalWrite);
RotaryEncoderInputDataSource rotaryEncoderDataSource;
Rs485EnvironmentDataSource dataSource;
WiFiDataSource wifiDataSource;
WifiCredentialStore wifiCredentialStore;
WifiProvisioningCoordinator wifiProvisioningCoordinator(&wifiDataSource,
                                                         &wifiCredentialStore);
BleProvisioningService bleProvisioningService;
ServoMotor servoMotor(PeripheralPins::kServoSignal);
RgbLedMatrix rgbLedMatrix(PeripheralPins::kRgbData);
MqttService mqttService;
MqttConfigurationStore mqttConfigurationStore;
MqttProvisioningCoordinator mqttProvisioningCoordinator(&mqttService, &mqttConfigurationStore);
DcMotor dcMotor(PeripheralPins::kDcMotorIn1, PeripheralPins::kDcMotorIn2);
ConnectionStatusIndicators connectionIndicators(PeripheralPins::kWifiIndicator,
                                                 PeripheralPins::kMqttIndicator,
                                                 PeripheralPins::kBleIndicator);

namespace {
constexpr uint32_t kRefreshIntervalMs = 100;
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
uint16_t lastJoystickX = 0;
uint16_t lastJoystickY = 0;

void makeHash(char hash[9]) { MqttMessageProtocol::generateHash(++mqttSequence, millis(), hash); }

void enqueueJson(const char* json, bool reply = false) {
  MqttMessage message = {};
  snprintf(message.topic, sizeof(message.topic), "%s", mqttService.publishTopic());
  snprintf(message.payload, sizeof(message.payload), "%s", json);
  message.isReply = reply;
  if (reply) mqttService.enqueueReply(message); else mqttService.enqueue(message);
}

void publishEnvironment(const EnvironmentData& data) {
  char hash[9] = {};
  makeHash(hash);
  char json[385] = {};
  snprintf(json, sizeof(json), "{\"hash\":\"%s\",\"type\":\"telemetry\",\"module\":\"environment\",\"temperature\":%.2f,\"humidity\":%.2f,\"pressure\":%.2f,\"light\":%.2f,\"altitude\":%.2f,\"microphone\":%.0f,\"joystickX\":%u,\"joystickY\":%u,\"encoderPosition\":%ld,\"encoderPressed\":%s,\"rfid\":\"%s\",\"online\":%s}", hash, data.temperatureC, data.humidityPercent, data.pressureHpa, data.lightLux, data.altitudeM, data.microphonePercent, data.joystickX, data.joystickY, static_cast<long>(data.encoderPosition), data.encoderPressed ? "true" : "false", data.rfidCard == nullptr ? "" : data.rfidCard, data.sensorConnected ? "true" : "false");
  enqueueJson(json);
}

void publishEncoder(const EnvironmentData& data) {
  char hash[9] = {}; makeHash(hash);
  char json[180] = {};
  snprintf(json, sizeof(json), "{\"hash\":\"%s\",\"type\":\"telemetry\",\"module\":\"encoder\",\"position\":%ld,\"pressed\":%s}", hash, static_cast<long>(data.encoderPosition), data.encoderPressed ? "true" : "false");
  enqueueJson(json);
}

void publishJoystick(const EnvironmentData& data) {
  char hash[9] = {}; makeHash(hash);
  char json[180] = {};
  snprintf(json, sizeof(json), "{\"hash\":\"%s\",\"type\":\"telemetry\",\"module\":\"joystick\",\"x\":%u,\"y\":%u,\"pressed\":%s}", hash, data.joystickX, data.joystickY, data.joystickPressed ? "true" : "false");
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
 
  dashboard.begin();
  dashboard.drawStaticLayout();

  connectionIndicators.begin();
  relayController.begin();
  pinMode(PeripheralPins::kLcdCs, OUTPUT);
  digitalWrite(PeripheralPins::kLcdCs, HIGH);
  pinMode(PeripheralPins::kRfidSs, OUTPUT);
  digitalWrite(PeripheralPins::kRfidSs, HIGH);

  rfidReader.begin();
  joystickDataSource.begin();
  microphoneInputDataSource.begin();
  rotaryEncoderDataSource.begin();
  dataSource.begin();
  WifiCredentialsValue savedCredentials = {};
  WifiCredentialsValue fallbackCredentials = {};
  snprintf(fallbackCredentials.ssid, sizeof(fallbackCredentials.ssid), "%s",
           WifiCredentials::kSsid);
  snprintf(fallbackCredentials.password,
           sizeof(fallbackCredentials.password), "%s",
           WifiCredentials::kPassword);
  const bool savedCredentialsPresent =
      wifiCredentialStore.load(&savedCredentials);
  const WifiCredentialsValue bootCredentials =
      WifiCredentialPolicy::selectBootCredentials(
          savedCredentialsPresent, savedCredentials, fallbackCredentials);
  wifiDataSource.begin(bootCredentials);
  bleProvisioningService.begin();
  mqttService.begin();
  stepperMotor.begin();
  dcMotor.begin();
  rgbLedMatrix.begin();
  Serial.println("WS2812B matrix shows a centered red Yi character");

  EnvironmentData environment = dataSource.readEnvironment();
  joystickDataSource.readInto(&environment);
  lastJoystickX = environment.joystickX;
  lastJoystickY = environment.joystickY;
  microphoneInputDataSource.readInto(&environment);
  environment.rfidCard = rfidReader.cardUid();
  rotaryEncoderDataSource.readInto(&environment);
  lastEncoderPosition = environment.encoderPosition;
  lastEncoderPressed = environment.encoderPressed;
  NetworkStatus initialNetwork = wifiDataSource.readNetwork();
  initialNetwork.mqttHost = mqttService.server();
  dashboard.update(environment, initialNetwork, 0);

  Serial.println("ST7789 RS485 dashboard started");
   Serial.begin(115200);
  if (servoMotor.begin()) {
    Serial.println("Servo startup: 90 degrees, then return to 0 degrees");
  }
}

void loop() {
  servoMotor.update(millis());
  static uint32_t lastUpdateMs = 0;
  const uint32_t nowMs = millis();
  dataSource.poll(nowMs);
  wifiDataSource.poll(nowMs);
  bleProvisioningService.poll(nowMs);

  BleWifiRequest provisioningRequest = {};
  while (bleProvisioningService.takeRequest(&provisioningRequest)) {
    const bool mqttRequest = provisioningRequest.kind == BleProvisioningKind::kMqtt;
    const char* failureEvent = mqttRequest ? "mqtt_failed" : "wifi_failed";
    if (wifiProvisioningCoordinator.active() || mqttProvisioningCoordinator.active()) {
      bleProvisioningService.enqueueStatus(provisioningRequest.id, failureEvent, "busy");
    } else if (mqttRequest && !wifiDataSource.readNetwork().connected) {
      bleProvisioningService.enqueueStatus(provisioningRequest.id, failureEvent, "wifi_offline");
    } else if (mqttRequest) {
      if (!mqttProvisioningCoordinator.start(provisioningRequest, nowMs))
        bleProvisioningService.enqueueStatus(provisioningRequest.id, failureEvent, "invalid_mqtt");
    } else if (!wifiProvisioningCoordinator.start(provisioningRequest, nowMs)) {
      bleProvisioningService.enqueueStatus(provisioningRequest.id, failureEvent, "busy");
    }
  }
  wifiProvisioningCoordinator.poll(nowMs);
  WifiProvisioningEvent provisioningEvent = {};
  while (wifiProvisioningCoordinator.takeEvent(&provisioningEvent)) {
    bleProvisioningService.enqueueEvent(provisioningEvent);
  }

  NetworkStatus networkStatus = wifiDataSource.readNetwork();
  mqttService.poll(nowMs, networkStatus.connected);
  mqttProvisioningCoordinator.poll(millis());
  MqttProvisioningEvent mqttEvent;
  while (mqttProvisioningCoordinator.takeEvent(&mqttEvent)) {
    const char* name = mqttEvent.kind == MqttProvisioningEventKind::kConnecting ? "mqtt_connecting" :
        mqttEvent.kind == MqttProvisioningEventKind::kConnected ? "mqtt_connected" : "mqtt_failed";
    bleProvisioningService.enqueueStatus(mqttEvent.id, name, mqttEvent.reason);
  }
  networkStatus.mqttHost = mqttService.server();
  connectionIndicators.update(
      nowMs, networkStatus.connected, wifiProvisioningCoordinator.active(),
      mqttService.connected(), bleProvisioningService.connected());
  microphoneInputDataSource.poll(micros());
  rotaryEncoderDataSource.poll();
  stepperMotor.update(micros());

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
    if (MqttTelemetrySchedule::joystickChanged(environment.joystickX, environment.joystickY, lastJoystickX, lastJoystickY)) {
      publishJoystick(environment);
      lastJoystickX = environment.joystickX;
      lastJoystickY = environment.joystickY;
    }
    dashboard.update(environment, networkStatus, nowMs);
  }
}
