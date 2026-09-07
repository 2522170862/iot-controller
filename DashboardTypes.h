#pragma once

struct EnvironmentData {
  float temperatureC;
  float humidityPercent;
  float pressureHpa;
  float lightLux;
  float microphonePercent;
  uint16_t joystickX;
  uint16_t joystickY;
  int16_t encoderDelta;
  const char* rfidCard;
};

struct NetworkStatus {
  bool connected;
  const char* ssid;
  const char* localIp;
  const char* mqttHost;
};
