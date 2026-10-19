#include "MqttService.h"

#include <string.h>

#include "MqttCredentials.h"
#include "MqttConfigurationStore.h"

MqttService* MqttService::activeService_ = nullptr;

void MqttService::begin() {
  activeService_ = this;
  MqttConfigurationStore store;
  if (!store.load(&configuration_)) configuration_ = defaultMqttConfiguration();
  client_.setServer(configuration_.server, configuration_.port);
  client_.setBufferSize(512);
  client_.setSocketTimeout(2);
  wifiClient_.setConnectionTimeout(2000);
  client_.setCallback(onMessage);
}

MqttConfiguration MqttService::currentConfiguration() const { return configuration_; }
MqttConfigurationAttempt MqttService::configurationAttempt() const { return attempt_; }
const char* MqttService::configurationFailure() const { return failure_; }

void MqttService::configure(const MqttConfiguration& value, uint32_t) {
  client_.disconnect();
  wifiClient_.stop();
  reportConnectionTransition("configuration changed");
  incoming_.clear();
  outgoing_.clear();
  configuration_ = value;
  client_.setServer(configuration_.server, configuration_.port);
  connectionState_.reset();
  subscribed_ = false;
  attempt_ = MqttConfigurationAttempt::kConnecting;
  failure_ = "connection_failed";
}

void MqttService::poll(uint32_t nowMs, bool wifiConnected) {
  if (!wifiConnected) {
    if (client_.connected()) client_.disconnect();
    wifiClient_.stop();
    subscribed_ = false;
    attempt_ = MqttConfigurationAttempt::kConnecting;
    reportConnectionTransition("Wi-Fi offline");
    connectionState_.reset();
    return;
  }
  if (!client_.connected()) {
    subscribed_ = false;
    if (connectionState_.shouldAttempt(nowMs, true)) connectAndSubscribe(nowMs);
  }
  if (client_.connected()) {
    client_.loop();
    if (client_.connected() && subscribed_) publishOne();
    if (!client_.connected()) { subscribed_ = false; attempt_ = MqttConfigurationAttempt::kConnecting; }
  }
  reportConnectionTransition();
}

bool MqttService::takeIncoming(MqttMessage* message) { return incoming_.pop(message); }
bool MqttService::enqueue(const MqttMessage& message) { return outgoing_.push(message); }
bool MqttService::enqueueReply(const MqttMessage& message) { return outgoing_.pushReply(message); }
bool MqttService::connected() { return client_.connected() && subscribed_; }

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
  if (!client_.connect(MqttCredentials::kClientId,
                       configuration_.username[0] ? configuration_.username : nullptr,
                       configuration_.username[0] ? configuration_.password : nullptr)) {
    attempt_ = MqttConfigurationAttempt::kFailed;
    failure_ = (client_.state() == MQTT_CONNECT_BAD_CREDENTIALS || client_.state() == MQTT_CONNECT_UNAUTHORIZED)
        ? "authentication_failed" : "connection_failed";
    logConnectFailure();
    return;
  }

  reportConnectionTransition();
  if (client_.subscribe(configuration_.subTopic, configuration_.subQos)) {
    subscribed_ = true;
    attempt_ = MqttConfigurationAttempt::kConnected;
    Serial.print("MQTT subscribed: ");
    Serial.println(configuration_.subTopic);
  } else {
    Serial.print("MQTT subscribe failed: ");
    Serial.println(configuration_.subTopic);
    failure_ = "subscribe_failed";
    attempt_ = MqttConfigurationAttempt::kFailed;
    client_.disconnect();
  }
}

void MqttService::reportConnectionTransition(const char* disconnectReason) {
  const MqttConnectionEvent event =
      connectionState_.observeConnection(client_.connected());
  if (event == MqttConnectionEvent::kConnected) {
    Serial.print("MQTT connected: ");
    Serial.print(configuration_.server);
    Serial.print(':');
    Serial.println(configuration_.port);
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
