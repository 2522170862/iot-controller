#include <assert.h>

#include "../../MockDataSource.cpp"

void setup() {
  MockDataSource source;

  const EnvironmentData start = source.readEnvironment(0);
  const EnvironmentData later = source.readEnvironment(2500);
  const NetworkStatus network = source.readNetwork();

  assert(start.temperatureC >= 20.0f && start.temperatureC <= 30.0f);
  assert(start.humidityPercent >= 40.0f && start.humidityPercent <= 70.0f);
  assert(start.pressureHpa >= 980.0f && start.pressureHpa <= 1040.0f);
  assert(start.lightLux >= 0.0f && start.lightLux <= 1000.0f);
  assert(start.microphonePercent >= 0.0f && start.microphonePercent <= 100.0f);
  assert(start.joystickX >= 0 && start.joystickX <= 4095);
  assert(start.joystickY >= 0 && start.joystickY <= 4095);
  assert(start.encoderDelta >= -100 && start.encoderDelta <= 100);
  assert(start.rfidCard != nullptr && start.rfidCard[0] != '\0');
  assert(start.temperatureC != later.temperatureC);
  assert(start.joystickX != later.joystickX);
  assert(network.connected);
}

void loop() {}
