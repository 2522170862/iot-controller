#pragma once

#include <stdint.h>

#include "DashboardTypes.h"

class Rs485EnvironmentDataSource {
 public:
  void begin();
  void poll(uint32_t nowMs);
  const EnvironmentData& readEnvironment() const;

 private:
  static constexpr int8_t kRxPin = 18;
  static constexpr int8_t kTxPin = 17;
  static constexpr uint32_t kBaudRate = 9600;
  static constexpr uint32_t kPollIntervalMs = 1000;
  static constexpr uint32_t kResponseTimeoutMs = 150;

  EnvironmentData environment_{};
  uint8_t response_[19]{};
  uint8_t responseLength_ = 0;
  uint32_t lastRequestMs_ = 0;
  uint32_t requestStartedMs_ = 0;
  bool requestInFlight_ = false;

  void startReadRequest(uint32_t nowMs);
  void resetResponse();
};
