#pragma once

#include <stdint.h>

struct EnvironmentData {
  float temperatureC;
  float humidityPercent;
  float pressureHpa;
  float lightLux;
  float altitudeM;
  float microphonePercent;
  uint16_t joystickX;
  uint16_t joystickY;
  int16_t encoderDelta;
  const char* rfidCard;
  bool sensorConnected;
  bool joystickPressed;
  int32_t encoderPosition;
  bool encoderPressed;
};

struct NetworkStatus {
  bool connected;
  const char* ssid;
  const char* localIp;
  const char* mqttHost;
};
