#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../MqttMessageQueue.cpp"

namespace {

MqttMessage telemetry(uint8_t index) {
  MqttMessage message = {};
  snprintf(message.topic, sizeof(message.topic), "LoTC2S/");
  snprintf(message.payload, sizeof(message.payload), "telemetry-%u", index);
  message.isReply = false;
  return message;
}

MqttMessage reply() {
  MqttMessage message = {};
  snprintf(message.topic, sizeof(message.topic), "LoTC2S/");
  snprintf(message.payload, sizeof(message.payload), "reply");
  message.isReply = true;
  return message;
}

}  // namespace

void setup() {
  MqttMessageQueue queue;
  for (uint8_t index = 1; index <= MqttMessageQueue::kCapacity; ++index) {
    assert(queue.push(telemetry(index)));
  }

  assert(queue.pushReply(reply()));

  MqttMessage item = {};
  assert(queue.pop(&item));
  assert(strcmp(item.payload, "telemetry-2") == 0);

  for (uint8_t index = 3; index <= MqttMessageQueue::kCapacity; ++index) {
    assert(queue.pop(&item));
    char expected[20] = {};
    snprintf(expected, sizeof(expected), "telemetry-%u", index);
    assert(strcmp(item.payload, expected) == 0);
  }
  assert(queue.pop(&item));
  assert(strcmp(item.payload, "reply") == 0);
  assert(!queue.pop(&item));
}

void loop() {}
