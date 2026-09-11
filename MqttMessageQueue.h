#pragma once

#include <stddef.h>

struct MqttMessage {
  static constexpr size_t kTopicCapacity = 40;
  static constexpr size_t kPayloadCapacity = 385;
  char topic[kTopicCapacity];
  char payload[kPayloadCapacity];
  bool isReply;
};

class MqttMessageQueue {
 public:
  static constexpr size_t kCapacity = 16;

  bool push(const MqttMessage& message);
  bool pushReply(const MqttMessage& message);
  bool pop(MqttMessage* message);
  size_t size() const;

 private:
  MqttMessage messages_[kCapacity] = {};
  size_t count_ = 0;

  void eraseAt(size_t index);
};
