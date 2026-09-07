#pragma once

#include <stdint.h>

#include "DashboardTypes.h"

class MockDataSource {
 public:
  EnvironmentData readEnvironment(uint32_t nowMs) const;
  NetworkStatus readNetwork() const;

 private:
  static float triangleWave(uint32_t nowMs, uint32_t periodMs);
};
