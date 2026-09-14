#pragma once

#include <Arduino.h>
#include <stdint.h>

class ConnectionStatusIndicators {
 public:
  using PinModeFunction = void (*)(uint8_t, uint8_t);
  using DigitalWriteFunction = void (*)(uint8_t, uint8_t);

  ConnectionStatusIndicators(uint8_t wifiPin, uint8_t mqttPin,
                             PinModeFunction pinModeFunction = pinMode,
                             DigitalWriteFunction digitalWriteFunction = digitalWrite);

  void begin();
  void update(bool wifiConnected, bool mqttConnected);

 private:
  uint8_t wifiPin_;
  uint8_t mqttPin_;
  PinModeFunction pinModeFunction_;
  DigitalWriteFunction digitalWriteFunction_;
};
