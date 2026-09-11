#pragma once

#include <PubSubClient.h>
#include <WiFi.h>

#include "MqttConnectionState.h"
#include "MqttMessageQueue.h"

class MqttService {
 public:
  void begin();
  void poll(uint32_t nowMs, bool wifiConnected);
  bool takeIncoming(MqttMessage* message);
  bool enqueue(const MqttMessage& message);
  bool enqueueReply(const MqttMessage& message);
  bool connected();

 private:
  static constexpr size_t kIncomingCapacity = 8;
  static MqttService* activeService_;
  WiFiClient wifiClient_;
  PubSubClient client_{wifiClient_};
  MqttConnectionState connectionState_;
  MqttMessageQueue incoming_;
  MqttMessageQueue outgoing_;

  static void onMessage(char* topic, uint8_t* payload, unsigned int length);
  void copyIncoming(const char* topic, const uint8_t* payload, size_t length);
  void connectAndSubscribe(uint32_t nowMs);
  void publishOne();
};
