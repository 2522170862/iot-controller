#include "../../BleProvisioningProtocol.cpp"
#include "../../MqttConfiguration.cpp"
#include "../../BleProvisioningService.cpp"

BleProvisioningService service;

void setup() {
  service.begin();
  service.poll(millis());
}

void loop() {}
