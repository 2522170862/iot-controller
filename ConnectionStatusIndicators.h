#pragma once

#include <Arduino.h>
#include <stdint.h>

class ConnectionStatusIndicators {
 public:
  using PinModeFunction = void (*)(uint8_t, uint8_t);
  using DigitalWriteFunction = void (*)(uint8_t, uint8_t);

  ConnectionStatusIndicators(uint8_t wifiPin, uint8_t mqttPin,
                             uint8_t blePin,
                             PinModeFunction pinModeFunction = pinMode,
                             DigitalWriteFunction digitalWriteFunction = digitalWrite);

  void begin();
  void update(uint32_t nowMs, bool wifiConnected, bool wifiValidating,
              bool mqttConnected, bool bleConnected);

 private:
  static constexpr uint32_t kWifiBlinkIntervalMs = 250;

  uint8_t wifiPin_;
  uint8_t mqttPin_;
  uint8_t blePin_;
  PinModeFunction pinModeFunction_;
  DigitalWriteFunction digitalWriteFunction_;
};
