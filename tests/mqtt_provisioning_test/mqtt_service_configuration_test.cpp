#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../MqttConfiguration.cpp"
#include "../../MqttConfigurationStore.cpp"
#include "../../MqttMessageQueue.cpp"
#include "../../MqttService.cpp"

int main() {
  MqttService service;
  service.begin();
  MqttMessage old = {};
  snprintf(old.topic,sizeof(old.topic),"old/topic");
  snprintf(old.payload,sizeof(old.payload),"old-payload");
  assert(service.enqueue(old));
  auto candidate=service.currentConfiguration();
  snprintf(candidate.server,sizeof(candidate.server),"new.example.com");
  snprintf(candidate.pubTopic,sizeof(candidate.pubTopic),"new/out");
  snprintf(candidate.subTopic,sizeof(candidate.subTopic),"new/in/+");
  snprintf(candidate.username,sizeof(candidate.username),"new-user");
  candidate.port=1884;candidate.subQos=1;
  service.configure(candidate,100);
  service.poll(100,true);
  assert(service.connected());
  assert(service.configurationAttempt()==MqttConfigurationAttempt::kConnected);
  assert(PubSubClient::server=="new.example.com" && PubSubClient::port==1884);
  assert(PubSubClient::username=="new-user");
  assert(PubSubClient::subscribedTopic=="new/in/+" && PubSubClient::qos==1);
  assert(PubSubClient::publishedTopics.empty());
  MqttMessage fresh={};
  snprintf(fresh.topic,sizeof(fresh.topic),"%s",service.publishTopic());
  snprintf(fresh.payload,sizeof(fresh.payload),"fresh");
  assert(service.enqueue(fresh));service.poll(200,true);
  assert(PubSubClient::publishedTopics.size()==1 && PubSubClient::publishedTopics[0]=="new/out");
  assert(PubSubClient::publishedPayloads[0]=="fresh");
  PubSubClient::connectSucceeds=false;
  service.configure(candidate,300);service.poll(300,true);
  assert(service.configurationAttempt()==MqttConfigurationAttempt::kFailed);
  assert(strcmp(service.configurationFailure(),"authentication_failed")==0);
  PubSubClient::connectSucceeds=true;PubSubClient::subscribeSucceeds=false;
  service.configure(candidate,400);service.poll(400,true);
  assert(service.configurationAttempt()==MqttConfigurationAttempt::kFailed);
  assert(strcmp(service.configurationFailure(),"subscribe_failed")==0);
  assert(!service.connected());
  service.poll(450,false);
  assert(!service.connected());
  puts("PASS: runtime broker, authentication, subscriptions, publishing, queue isolation and connection failures");
}
