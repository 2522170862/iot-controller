#pragma once

#include <stdint.h>

struct MqttConfiguration {
  char server[128] = {};
  uint16_t port = 1883;
  char username[64] = {};
  char password[128] = {};
  char subTopic[40] = {};
  char pubTopic[40] = {};
  uint8_t subQos = 0;
};

bool validMqttConfiguration(const MqttConfiguration& value);
MqttConfiguration defaultMqttConfiguration();

class IMqttConfigurationStore {
 public:
  virtual ~IMqttConfigurationStore() = default;
  virtual bool load(MqttConfiguration* output) = 0;
  virtual bool save(const MqttConfiguration& value) = 0;
};

enum class MqttConfigurationAttempt : uint8_t { kConnecting, kConnected, kFailed };

class IMqttConfigurationConnection {
 public:
  virtual ~IMqttConfigurationConnection() = default;
  virtual MqttConfiguration currentConfiguration() const = 0;
  virtual void configure(const MqttConfiguration& value, uint32_t nowMs) = 0;
  virtual MqttConfigurationAttempt configurationAttempt() const = 0;
  virtual const char* configurationFailure() const = 0;
};
