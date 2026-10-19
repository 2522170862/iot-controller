#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../MqttConfiguration.cpp"
#include "../../BleProvisioningProtocol.cpp"
#include "../../MqttProvisioningCoordinator.cpp"
#include "../../MqttConfigurationStore.cpp"

class FakeMqtt : public IMqttConfigurationConnection {
 public:
  MqttConfiguration value = defaultMqttConfiguration();
  MqttConfigurationAttempt state = MqttConfigurationAttempt::kConnected;
  int applied = 0;
  MqttConfiguration currentConfiguration() const override { return value; }
  void configure(const MqttConfiguration& next, uint32_t) override {
    value = next; ++applied; state = MqttConfigurationAttempt::kConnecting;
  }
  MqttConfigurationAttempt configurationAttempt() const override { return state; }
  const char* configurationFailure() const override { return "authentication_failed"; }
};
class FakeStore : public IMqttConfigurationStore {
 public:
  bool succeeds = true;
  int saves = 0;
  MqttConfiguration saved = {};
  bool load(MqttConfiguration*) override { return false; }
  bool save(const MqttConfiguration& value) override { ++saves; saved=value; return succeeds; }
};

BleProvisioningRequest request(const char* extra = "") {
  char json[1024];
  snprintf(json, sizeof(json), "{\"id\":\"1234abcd\",\"cmd\":\"configure_mqtt\",\"server\":\"new.example.com\",\"port\":1884,\"username\":\"u\",\"password\":\"p\"%s}", extra);
  BleProvisioningRequest result;
  assert(BleProvisioningProtocol::parseRequest(json, &result) == BleProtocolResult::kOk);
  assert(result.kind == BleProvisioningKind::kMqtt);
  return result;
}

int main() {
  {
    FakeMqtt mqtt; FakeStore store;
    snprintf(mqtt.value.pubTopic,sizeof(mqtt.value.pubTopic),"existing/out");
    snprintf(mqtt.value.subTopic,sizeof(mqtt.value.subTopic),"existing/in");
    mqtt.value.subQos = 1;
    MqttProvisioningCoordinator coordinator(&mqtt,&store);
    auto change = request(",\"subQos\":0");
    assert(coordinator.start(change,100));
    assert(!coordinator.start(change,101));
    assert(strcmp(mqtt.value.pubTopic,"existing/out")==0);
    assert(strcmp(mqtt.value.subTopic,"existing/in")==0);
    assert(mqtt.value.subQos==0);
    assert(store.saves==0);
    MqttProvisioningEvent event;
    assert(coordinator.takeEvent(&event));
    assert(event.kind==MqttProvisioningEventKind::kConnecting);
    mqtt.state=MqttConfigurationAttempt::kConnected;
    coordinator.poll(200);
    assert(store.saves==1 && !coordinator.active());
    assert(strcmp(store.saved.server,"new.example.com")==0);
    assert(coordinator.takeEvent(&event));
    assert(event.kind==MqttProvisioningEventKind::kConnected);
    assert(strcmp(event.id,"1234abcd")==0);
  }
  for (int mode=0;mode<3;++mode) {
    FakeMqtt mqtt; FakeStore store; auto before=mqtt.value;
    MqttProvisioningCoordinator coordinator(&mqtt,&store);
    assert(coordinator.start(request(),100));
    MqttProvisioningEvent event;
    assert(coordinator.takeEvent(&event));
    if (mode==0) mqtt.state=MqttConfigurationAttempt::kFailed;
    if (mode==1) { mqtt.state=MqttConfigurationAttempt::kConnected; store.succeeds=false; }
    coordinator.poll(mode==2 ? 15100 : 200);
    assert(!coordinator.active());
    assert(mqtt.applied==2);
    assert(strcmp(mqtt.value.server,before.server)==0);
    assert(coordinator.takeEvent(&event));
    assert(event.kind==MqttProvisioningEventKind::kFailed);
    assert(strcmp(event.reason,mode==0?"authentication_failed":mode==1?"storage_failed":"timeout")==0);
  }
  {
    FakeMqtt mqtt; FakeStore store; auto before=mqtt.value;
    MqttProvisioningCoordinator coordinator(&mqtt,&store);
    assert(coordinator.start(request(),100));
    MqttProvisioningEvent event; assert(coordinator.takeEvent(&event));
    mqtt.state=MqttConfigurationAttempt::kConnected;
    coordinator.poll(15100);
    assert(store.saves==0);
    assert(!coordinator.active() && mqtt.applied==2);
    assert(strcmp(mqtt.value.server,before.server)==0);
    assert(coordinator.takeEvent(&event));
    assert(event.kind==MqttProvisioningEventKind::kFailed && strcmp(event.reason,"timeout")==0);
  }
  {
    const char* invalid[] = {",\"subTopic\":null",",\"pubTopic\":\"\"",",\"subQos\":2",",\"subQos\":null",",\"pubTopic\":\"a/#\"",",\"subTopic\":\"a/#/b\""};
    for (auto fields:invalid) {
      char json[512];
      snprintf(json,sizeof(json),"{\"id\":\"1234abcd\",\"cmd\":\"configure_mqtt\",\"server\":\"host\",\"port\":1883,\"username\":\"\",\"password\":\"\"%s}",fields);
      BleProvisioningRequest output;
      assert(BleProvisioningProtocol::parseRequest(json,&output)!=BleProtocolResult::kOk);
      assert(strcmp(output.id,"1234abcd")==0);
    }
    auto valid=request(",\"subTopic\":\"a/+/b/#\",\"pubTopic\":\"a/out\"");
    assert(valid.mqtt.hasSubTopic && valid.mqtt.hasPubTopic && !valid.mqtt.hasSubQos);
  }
  {
    BleProvisioningRequest wifi;
    assert(BleProvisioningProtocol::parseRequest("{\"id\":\"1234abcd\",\"cmd\":\"configure_wifi\",\"ssid\":\"Lab\",\"password\":\"12345678\"}",&wifi)==BleProtocolResult::kOk);
    assert(wifi.kind==BleProvisioningKind::kWifi && strcmp(wifi.ssid,"Lab")==0);
    const char* json="{\"id\":\"1234abcd\",\"cmd\":\"configure_mqtt\",\"server\":\"new.example.com\",\"port\":1883,\"username\":\"u\",\"password\":\"p\"}\n";
    BleProvisioningFrameAssembler assembler;
    for(size_t offset=0;offset<strlen(json);offset+=20) {
      auto length = strlen(json)-offset; if(length>20) length=20;
      auto result=assembler.append(reinterpret_cast<const uint8_t*>(json+offset),length,&wifi);
      assert(result == (offset+length==strlen(json)? BleProtocolResult::kOk:BleProtocolResult::kNeedMore));
    }
    assert(wifi.kind==BleProvisioningKind::kMqtt);
  }
  puts("PASS: MQTT parsing, optional fields, success, failure, persistence gate, timeout and Wi-Fi compatibility");
  {
    MqttConfigurationStore store;
    MqttConfiguration loaded;
    assert(!store.load(&loaded));
    auto original=defaultMqttConfiguration();
    assert(store.save(original));
    assert(store.load(&loaded));
    assert(strcmp(loaded.server,original.server)==0);
    auto changed=original;
    snprintf(changed.server,sizeof(changed.server),"changed.example.com");
    Preferences::failKey="active";
    assert(!store.save(changed));
    assert(store.load(&loaded));
    assert(strcmp(loaded.server,original.server)==0);
    Preferences::failKey="";
    assert(store.save(changed));
    assert(store.load(&loaded));
    assert(strcmp(loaded.server,"changed.example.com")==0);
    // Corruption of the active record falls back to the previous valid slot.
    Preferences::values["cfg1"]={0,1,2};
    assert(store.load(&loaded));
    assert(strcmp(loaded.server,original.server)==0);
    puts("PASS: NVS boot loading, atomic activation, failed-save retention and corrupt-slot fallback");
  }
}
