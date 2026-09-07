#pragma once

#include <stdint.h>

#include "DashboardTypes.h"

class WiFiDataSource {
 public:
  WiFiDataSource();

  void begin();
  void poll(uint32_t nowMs);
  NetworkStatus readNetwork() const;

 private:
  static constexpr uint32_t kReconnectIntervalMs = 10000;

  bool connected_;
  uint32_t lastAttemptMs_;
  char localIp_[16];

  void startConnection(uint32_t nowMs);
  void setLocalIp(const char* value);
};
