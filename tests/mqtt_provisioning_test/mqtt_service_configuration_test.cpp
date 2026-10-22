#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <Esp.h>
#include "../../MqttConfiguration.cpp"
#include "../../MqttConfigurationStore.cpp"
#include "../../MqttMessageQueue.cpp"
#include "../../MqttService.cpp"

int main() {
  // Keep every MAC byte, including leading zeroes and bytes discarded by the
  // previous four-digit suffix. getEfuseMac() stores the first MAC byte lowest.
  const struct { uint64_t mac; const char* name; } identityCases[] = {
    {0x11223344C398ULL, "lot-98c344332211"},
    {0xAABB3344C398ULL, "lot-98c34433bbaa"},
    {0x112233440007ULL, "lot-070044332211"},
    {0x000000000001ULL, "lot-010000000000"},
    {0x000000000000ULL, "lot-000000000000"},
    {0xFFFFFFFFFFFFULL, "lot-ffffffffffff"},
  };
  for (const auto& identity : identityCases) {
    char name[DeviceIdentity::kNameCapacity] = {};
    DeviceIdentity::formatName(identity.mac, name);
    if (strcmp(name, identity.name) != 0) {
      fprintf(stderr, "FAIL: full-MAC identity expected %s, got %s\n", identity.name, name);
      return 1;
    }
  }
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
  snprintf(candidate.password,sizeof(candidate.password),"new&pass");
  candidate.port=1884;candidate.subQos=1;
  service.configure(candidate,100);
  Serial.output.clear();
  service.poll(100,true);
  if (Serial.output.find("MQTT login: server=new.example.com:1884, username=[new-user], password=[new&pass]\n") == std::string::npos) {
    fputs("FAIL: connection log must show the actual runtime username and password\n", stderr);
    return 1;
  }
  assert(service.connected());
  assert(service.configurationAttempt()==MqttConfigurationAttempt::kConnected);
  assert(PubSubClient::server=="new.example.com" && PubSubClient::port==1884);
  assert(PubSubClient::username=="new-user");
  assert(PubSubClient::password=="new&pass");
  assert(PubSubClient::clientId=="lot-98c344332211");
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
  PubSubClient::subscribeSucceeds=true;
  service.configure(candidate,500);service.poll(500,true);
  assert(service.connected());
  assert(PubSubClient::clientId=="lot-98c344332211");
  // A changed MAC read cannot change an already-running device's identity.
  EspClass::mac=0x112233440007ULL;
  service.configure(candidate,600);service.poll(600,true);
  assert(PubSubClient::clientId=="lot-98c344332211");
  EspClass::mac=0x11223344C398ULL;
  MqttService rebooted;
  rebooted.begin();rebooted.configure(candidate,700);rebooted.poll(700,true);
  assert(PubSubClient::clientId=="lot-98c344332211");
  // A second device uses its own suffix, preserving leading zeroes.
  EspClass::mac=0x112233440007ULL;
  MqttService secondDevice;
  secondDevice.begin();secondDevice.configure(candidate,800);secondDevice.poll(800,true);
  assert(PubSubClient::clientId=="lot-070044332211");
  snprintf(candidate.username,sizeof(candidate.username),"other-user");
  snprintf(candidate.password,sizeof(candidate.password),"other&pass");
  Serial.output.clear();
  secondDevice.configure(candidate,900);secondDevice.poll(900,true);
  assert(Serial.output.find("username=[other-user], password=[other&pass]")!=std::string::npos);
  candidate.username[0]='\0'; // Existing anonymous-login behavior sends no password.
  Serial.output.clear();
  secondDevice.configure(candidate,1000);secondDevice.poll(1000,true);
  assert(Serial.output.find("username=[], password=[]")!=std::string::npos);
  assert(Serial.output.find("other&pass")==std::string::npos);
  puts("PASS: MQTT login log follows current credentials and anonymous-login arguments");
  puts("PASS: runtime broker, authentication, subscriptions, publishing, queue isolation and connection failures");
  puts("PASS: full 12-digit MAC identity, leading zeroes, distinct MACs and stable reconnect/reboot");
}
