#include "MqttMessageQueue.h"

bool MqttMessageQueue::push(const MqttMessage& message) {
  if (count_ == kCapacity) {
    return false;
  }

  messages_[count_++] = message;
  return true;
}

bool MqttMessageQueue::pushReply(const MqttMessage& message) {
  if (count_ == kCapacity) {
    for (size_t index = 0; index < count_; ++index) {
      if (!messages_[index].isReply) {
        eraseAt(index);
        break;
      }
    }
  }

  return push(message);
}

bool MqttMessageQueue::pop(MqttMessage* message) {
  if (message == nullptr || count_ == 0) {
    return false;
  }

  *message = messages_[0];
  eraseAt(0);
  return true;
}

size_t MqttMessageQueue::size() const { return count_; }

void MqttMessageQueue::eraseAt(size_t index) {
  for (size_t current = index + 1; current < count_; ++current) {
    messages_[current - 1] = messages_[current];
  }
  --count_;
}
