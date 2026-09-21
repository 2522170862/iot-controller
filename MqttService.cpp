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
    reportConnectionTransition("Wi-Fi offline");
    connectionState_.reset();
    return;
  }
  if (!client_.connected() && connectionState_.shouldAttempt(nowMs, true)) connectAndSubscribe(nowMs);
  if (client_.connected()) {
    client_.loop();
    if (client_.connected()) publishOne();
  }
  reportConnectionTransition();
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
  if (!client_.connect(MqttCredentials::kClientId, MqttCredentials::kUserName,
                       MqttCredentials::kPassword)) {
    logConnectFailure();
    return;
  }

  reportConnectionTransition();
  if (client_.subscribe(MqttCredentials::kSubTopic, MqttCredentials::kSubQos)) {
    Serial.print("MQTT subscribed: ");
    Serial.println(MqttCredentials::kSubTopic);
  } else {
    Serial.print("MQTT subscribe failed: ");
    Serial.println(MqttCredentials::kSubTopic);
  }
}

void MqttService::reportConnectionTransition(const char* disconnectReason) {
  const MqttConnectionEvent event =
      connectionState_.observeConnection(client_.connected());
  if (event == MqttConnectionEvent::kConnected) {
    Serial.print("MQTT connected: ");
    Serial.print(MqttCredentials::kServer);
    Serial.print(':');
    Serial.println(MqttCredentials::kPort);
    return;
  }
  if (event != MqttConnectionEvent::kDisconnected) return;

  if (disconnectReason != nullptr) {
    Serial.print("MQTT disconnected: ");
    Serial.println(disconnectReason);
    return;
  }
  Serial.print("MQTT disconnected, state: ");
  Serial.println(client_.state());
}

void MqttService::logConnectFailure() {
  Serial.print("MQTT connect failed, state: ");
  Serial.print(client_.state());
  Serial.print(", retry in ");
  Serial.print(MqttConnectionState::kReconnectIntervalMs);
  Serial.println(" ms");
}

void MqttService::publishOne() {
  MqttMessage message = {};
  if (!outgoing_.pop(&message)) return;
  if (!client_.publish(message.topic, reinterpret_cast<const uint8_t*>(message.payload), strlen(message.payload))) {
    if (message.isReply) outgoing_.pushReply(message); else outgoing_.push(message);
  }
}
