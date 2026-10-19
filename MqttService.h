#pragma once

#include <PubSubClient.h>
#include <WiFi.h>

#include "MqttConnectionState.h"
#include "MqttMessageQueue.h"
#include "MqttConfiguration.h"

class MqttService : public IMqttConfigurationConnection {
 public:
  void begin();
  void poll(uint32_t nowMs, bool wifiConnected);
  bool takeIncoming(MqttMessage* message);
  bool enqueue(const MqttMessage& message);
  bool enqueueReply(const MqttMessage& message);
  bool connected();
  MqttConfiguration currentConfiguration() const override;
  void configure(const MqttConfiguration& value, uint32_t nowMs) override;
  MqttConfigurationAttempt configurationAttempt() const override;
  const char* configurationFailure() const override;
  const char* publishTopic() const { return configuration_.pubTopic; }
  const char* server() const { return configuration_.server; }

 private:
  static constexpr size_t kIncomingCapacity = 8;
  static MqttService* activeService_;
  WiFiClient wifiClient_;
  PubSubClient client_{wifiClient_};
  MqttConnectionState connectionState_;
  MqttMessageQueue incoming_;
  MqttMessageQueue outgoing_;
  MqttConfiguration configuration_;
  MqttConfigurationAttempt attempt_ = MqttConfigurationAttempt::kConnecting;
  bool subscribed_ = false;
  const char* failure_ = "connection_failed";

  static void onMessage(char* topic, uint8_t* payload, unsigned int length);
  void copyIncoming(const char* topic, const uint8_t* payload, size_t length);
  void connectAndSubscribe(uint32_t nowMs);
  void reportConnectionTransition(const char* disconnectReason = nullptr);
  void logConnectFailure();
  void publishOne();
};
