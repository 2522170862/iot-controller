#include "MqttConfiguration.h"

#include <stdio.h>
#include <string.h>

#include "MqttCredentials.h"

namespace {
bool validTopic(const char* text, size_t capacity, bool subscription) {
  const size_t length = strnlen(text, capacity);
  if (length == 0 || length >= capacity) return false;
  for (size_t i = 0; i < length; ++i) {
    if (text[i] != '+' && text[i] != '#') continue;
    if (!subscription || (i != 0 && text[i - 1] != '/')) return false;
    if (text[i] == '#' && i != length - 1) return false;
    if (text[i] == '+' && i + 1 < length && text[i + 1] != '/') return false;
  }
  return true;
}
}  // namespace

bool validMqttConfiguration(const MqttConfiguration& value) {
  const size_t length = strnlen(value.server, sizeof(value.server));
  if (length == 0 || length >= sizeof(value.server) || value.port == 0 ||
      strnlen(value.username, sizeof(value.username)) >= sizeof(value.username) ||
      strnlen(value.password, sizeof(value.password)) >= sizeof(value.password) ||
      value.subQos > 1) return false;
  for (size_t i = 0; i < length; ++i) {
    const char c = value.server[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '-')) return false;
  }
  return validTopic(value.subTopic, sizeof(value.subTopic), true) &&
         validTopic(value.pubTopic, sizeof(value.pubTopic), false);
}

MqttConfiguration defaultMqttConfiguration() {
  MqttConfiguration value;
  snprintf(value.server, sizeof(value.server), "%s", MqttCredentials::kServer);
  value.port = MqttCredentials::kPort;
  snprintf(value.username, sizeof(value.username), "%s", MqttCredentials::kUserName);
  snprintf(value.password, sizeof(value.password), "%s", MqttCredentials::kPassword);
  snprintf(value.subTopic, sizeof(value.subTopic), "%s", MqttCredentials::kSubTopic);
  snprintf(value.pubTopic, sizeof(value.pubTopic), "%s", MqttCredentials::kPubTopic);
  value.subQos = MqttCredentials::kSubQos;
  return value;
}
