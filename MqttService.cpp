#include "MqttService.h"

#include <string.h>

#include "MqttCredentials.h"

MqttService* MqttService::activeService_ = nullptr;

void MqttService::begin() {
  activeService_ = this;
  client_.setServer(MqttCredentials::kServer, MqttCredentials::kPort);
  client_.setBufferSize(512);
  client_.setCallback(onMessage);
}

void MqttService::poll(uint32_t nowMs, bool wifiConnected) {
  if (!wifiConnected) {
    if (client_.connected()) client_.disconnect();
    connectionState_.reset();
    return;
  }
  if (!client_.connected() && connectionState_.shouldAttempt(nowMs, true)) connectAndSubscribe(nowMs);
  if (client_.connected()) {
    client_.loop();
    publishOne();
  }
}

bool MqttService::takeIncoming(MqttMessage* message) { return incoming_.pop(message); }
bool MqttService::enqueue(const MqttMessage& message) { return outgoing_.push(message); }
bool MqttService::enqueueReply(const MqttMessage& message) { return outgoing_.pushReply(message); }
bool MqttService::connected() { return client_.connected(); }

void MqttService::onMessage(char* topic, uint8_t* payload, unsigned int length) {
  if (activeService_ != nullptr) activeService_->copyIncoming(topic, payload, length);
}

void MqttService::copyIncoming(const char* topic, const uint8_t* payload, size_t length) {
  if (incoming_.size() >= kIncomingCapacity || topic == nullptr || payload == nullptr ||
      strlen(topic) >= MqttMessage::kTopicCapacity || length >= MqttMessage::kPayloadCapacity) return;
  MqttMessage message = {};
  snprintf(message.topic, sizeof(message.topic), "%s", topic);
  memcpy(message.payload, payload, length);
  message.payload[length] = '\0';
  incoming_.push(message);
}

void MqttService::connectAndSubscribe(uint32_t nowMs) {
  connectionState_.recordAttempt(nowMs);
  if (client_.connect(MqttCredentials::kClientId, MqttCredentials::kUserName, MqttCredentials::kPassword))
    client_.subscribe(MqttCredentials::kSubTopic, MqttCredentials::kSubQos);
}

void MqttService::publishOne() {
  MqttMessage message = {};
  if (!outgoing_.pop(&message)) return;
  if (!client_.publish(message.topic, reinterpret_cast<const uint8_t*>(message.payload), strlen(message.payload))) {
    if (message.isReply) outgoing_.pushReply(message); else outgoing_.push(message);
  }
}
