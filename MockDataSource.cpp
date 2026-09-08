#include "MockDataSource.h"

float MockDataSource::triangleWave(uint32_t nowMs, uint32_t periodMs) {
  const uint32_t phaseMs = nowMs % periodMs;
  const float halfPeriodMs = periodMs / 2.0f;

  if (phaseMs <= halfPeriodMs) {
    return phaseMs / halfPeriodMs;
  }

  return (periodMs - phaseMs) / halfPeriodMs;
}

EnvironmentData MockDataSource::readEnvironment(uint32_t nowMs) const {
  const float temperatureWave = triangleWave(nowMs, 10000);
  const float humidityWave = triangleWave(nowMs + 3000, 14000);
  const float pressureWave = triangleWave(nowMs + 1200, 18000);
  const float lightWave = triangleWave(nowMs + 500, 8000);
  const float microphoneWave = triangleWave(nowMs + 1700, 3200);
  const float joystickXWave = triangleWave(nowMs, 6000);
  const float joystickYWave = triangleWave(nowMs + 2200, 7000);
  const float encoderWave = triangleWave(nowMs, 12000);

  return {
      23.0f + temperatureWave * 4.0f,
      50.0f + humidityWave * 16.0f,
      998.0f + pressureWave * 20.0f,
      80.0f + lightWave * 720.0f,
      340.0f,
      8.0f + microphoneWave * 72.0f,
      static_cast<uint16_t>(joystickXWave * 4095.0f),
      static_cast<uint16_t>(joystickYWave * 4095.0f),
      static_cast<int16_t>(encoderWave * 200.0f - 100.0f),
      "A1 B2 C3 D4",
      true,
      false,
  };
}

NetworkStatus MockDataSource::readNetwork() const {
  return {
      true,
      "Demo-WiFi",
      "192.168.1.88",
      "192.168.1.100",
  };
}
