#include <assert.h>
#include <math.h>

#include "../../ModbusEnvironmentProtocol.cpp"

void setup() {
  uint8_t request[ModbusEnvironmentProtocol::kReadRequestLength] = {0};
  ModbusEnvironmentProtocol::buildReadAllRequest(request);

  const uint8_t expected[] = {0x01, 0x03, 0x00, 0x65,
                              0x00, 0x07, 0x14, 0x17};
  for (uint8_t index = 0; index < sizeof(expected); ++index) {
    assert(request[index] == expected[index]);
  }

  const uint8_t response[] = {
      0x01, 0x03, 0x0E, 0x00, 0x00, 0x02, 0x58, 0x0A, 0xAA, 0x00,
      0x99, 0x6D, 0x65, 0x14, 0x73, 0x00, 0x40, 0x23, 0x53,
  };
  EnvironmentData environment = {};
  assert(ModbusEnvironmentProtocol::parseReadAllResponse(
      response, sizeof(response), &environment));
  assert(fabsf(environment.lightLux - 6.0f) < 0.001f);
  assert(fabsf(environment.temperatureC - 27.30f) < 0.001f);
  assert(fabsf(environment.pressureHpa - 1005.5013f) < 0.001f);
  assert(fabsf(environment.humidityPercent - 52.35f) < 0.001f);
  assert(fabsf(environment.altitudeM - 64.0f) < 0.001f);

  uint8_t corruptResponse[sizeof(response)];
  memcpy(corruptResponse, response, sizeof(response));
  corruptResponse[18] ^= 0x01;
  assert(!ModbusEnvironmentProtocol::parseReadAllResponse(
      corruptResponse, sizeof(corruptResponse), &environment));
}

void loop() {}
