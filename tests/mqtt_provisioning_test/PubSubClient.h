#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>
#include "WiFi.h"
#define MQTT_CONNECT_BAD_CREDENTIALS 4
#define MQTT_CONNECT_UNAUTHORIZED 5

// Fake network boundary. MqttService itself, its queues and persistence are real.
class PubSubClient {
 public:
  inline static std::string server, username, password, clientId, subscribedTopic;
  inline static uint16_t port=0;
  inline static uint8_t qos=0;
  inline static bool connectSucceeds=true, subscribeSucceeds=true;
  inline static std::vector<std::string> publishedTopics, publishedPayloads;
  explicit PubSubClient(WiFiClient&) {}
  void setServer(const char* host, uint16_t number) { server=host;port=number; }
  void setBufferSize(size_t) {}
  void setSocketTimeout(uint16_t) {}
  void setCallback(void (*)(char*,uint8_t*,unsigned int)) {}
  bool connected() { return online; }
  void disconnect() { online=false; }
  bool loop() { return online; }
  int state() { return connectSucceeds ? 0 : MQTT_CONNECT_BAD_CREDENTIALS; }
  bool connect(const char* id,const char* user,const char* pass) {
    clientId=id?id:"";username=user?user:"";password=pass?pass:"";
    online=connectSucceeds;return online;
  }
  bool subscribe(const char* topic, uint8_t value) { subscribedTopic=topic;qos=value;return subscribeSucceeds; }
  bool publish(const char* topic,const uint8_t* payload,size_t length) {
    if(!online) return false;
    publishedTopics.emplace_back(topic);publishedPayloads.emplace_back(reinterpret_cast<const char*>(payload),length);return true;
  }
 private:
  bool online=false;
};
