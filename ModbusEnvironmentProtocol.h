#pragma once

#include <stdint.h>

#include "DashboardTypes.h"

namespace ModbusEnvironmentProtocol {
constexpr uint8_t kSlaveAddress = 0x01;
constexpr uint8_t kReadHoldingRegisters = 0x03;
constexpr uint16_t kFirstRegister = 0x0065;
constexpr uint16_t kRegisterCount = 0x0007;
constexpr uint8_t kReadRequestLength = 8;
constexpr uint8_t kReadResponseLength = 19;

void buildReadAllRequest(uint8_t request[kReadRequestLength]);
bool parseReadAllResponse(const uint8_t* response, uint8_t responseLength,
                          EnvironmentData* environment);
}  // namespace ModbusEnvironmentProtocol
