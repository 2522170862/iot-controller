#include "ModbusEnvironmentProtocol.h"

namespace {
uint16_t calculateCrc(const uint8_t* bytes, uint8_t length) {
  uint16_t crc = 0xFFFF;
  for (uint8_t index = 0; index < length; ++index) {
    crc ^= bytes[index];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x0001) == 0 ? crc >> 1 : (crc >> 1) ^ 0xA001;
    }
  }
  return crc;
}

uint16_t readUint16(const uint8_t* bytes) {
  return static_cast<uint16_t>(bytes[0]) << 8 | bytes[1];
}

uint32_t readUint32(const uint8_t* bytes) {
  return static_cast<uint32_t>(bytes[0]) << 24 |
         static_cast<uint32_t>(bytes[1]) << 16 |
         static_cast<uint32_t>(bytes[2]) << 8 | bytes[3];
}
}  // namespace

void ModbusEnvironmentProtocol::buildReadAllRequest(
    uint8_t request[kReadRequestLength]) {
  request[0] = kSlaveAddress;
  request[1] = kReadHoldingRegisters;
  request[2] = static_cast<uint8_t>(kFirstRegister >> 8);
  request[3] = static_cast<uint8_t>(kFirstRegister);
  request[4] = static_cast<uint8_t>(kRegisterCount >> 8);
  request[5] = static_cast<uint8_t>(kRegisterCount);

  const uint16_t crc = calculateCrc(request, 6);
  request[6] = static_cast<uint8_t>(crc);
  request[7] = static_cast<uint8_t>(crc >> 8);
}

bool ModbusEnvironmentProtocol::parseReadAllResponse(
    const uint8_t* response, uint8_t responseLength,
    EnvironmentData* environment) {
  if (response == nullptr || environment == nullptr ||
      responseLength != kReadResponseLength || response[0] != kSlaveAddress ||
      response[1] != kReadHoldingRegisters || response[2] != 0x0E) {
    return false;
  }

  const uint16_t receivedCrc = static_cast<uint16_t>(response[17]) |
                               static_cast<uint16_t>(response[18]) << 8;
  if (calculateCrc(response, 17) != receivedCrc) {
    return false;
  }

  environment->lightLux = readUint32(response + 3) / 100.0f;
  environment->temperatureC =
      static_cast<int16_t>(readUint16(response + 7)) / 100.0f;
  environment->pressureHpa = readUint32(response + 9) / 10000.0f;
  environment->humidityPercent = readUint16(response + 13) / 100.0f;
  environment->altitudeM = readUint16(response + 15);
  return true;
}
