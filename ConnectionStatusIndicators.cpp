#include "ConnectionStatusIndicators.h"

ConnectionStatusIndicators::ConnectionStatusIndicators(
    uint8_t wifiPin, uint8_t mqttPin, uint8_t blePin,
    PinModeFunction pinModeFunction, DigitalWriteFunction digitalWriteFunction)
    : wifiPin_(wifiPin),
      mqttPin_(mqttPin),
      blePin_(blePin),
      pinModeFunction_(pinModeFunction),
      digitalWriteFunction_(digitalWriteFunction) {}

void ConnectionStatusIndicators::begin() {
  pinModeFunction_(wifiPin_, OUTPUT);
  pinModeFunction_(mqttPin_, OUTPUT);
  pinModeFunction_(blePin_, OUTPUT);
  update(0, false, false, false, false);
}

void ConnectionStatusIndicators::update(uint32_t nowMs, bool wifiConnected,
                                        bool wifiValidating,
                                        bool mqttConnected,
                                        bool bleConnected) {
  const bool wifiLit =
      wifiValidating
          ? ((nowMs / kWifiBlinkIntervalMs) % 2U) != 0
          : wifiConnected;
  digitalWriteFunction_(wifiPin_, wifiLit ? HIGH : LOW);
  digitalWriteFunction_(mqttPin_, mqttConnected ? HIGH : LOW);
  digitalWriteFunction_(blePin_, bleConnected ? HIGH : LOW);
}
