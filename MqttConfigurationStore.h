#pragma once

#include "MqttConfiguration.h"

class MqttConfigurationStore : public IMqttConfigurationStore {
 public:
  bool load(MqttConfiguration* output) override;
  bool save(const MqttConfiguration& value) override;
};
