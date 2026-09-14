#include "ConnectionStatusIndicators.h"

ConnectionStatusIndicators::ConnectionStatusIndicators(
    uint8_t wifiPin, uint8_t mqttPin, PinModeFunction pinModeFunction,
    DigitalWriteFunction digitalWriteFunction)
    : wifiPin_(wifiPin),
      mqttPin_(mqttPin),
      pinModeFunction_(pinModeFunction),
      digitalWriteFunction_(digitalWriteFunction) {}

void ConnectionStatusIndicators::begin() {
  pinModeFunction_(wifiPin_, OUTPUT);
  pinModeFunction_(mqttPin_, OUTPUT);
  update(false, false);
}

void ConnectionStatusIndicators::update(bool wifiConnected, bool mqttConnected) {
  digitalWriteFunction_(wifiPin_, wifiConnected ? HIGH : LOW);
  digitalWriteFunction_(mqttPin_, mqttConnected ? HIGH : LOW);
}
