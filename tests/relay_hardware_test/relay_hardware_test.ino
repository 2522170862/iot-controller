#include <Arduino.h>

#include "../../PeripheralPins.h"
#include "../../RelayController.cpp"

RelayController relays(PeripheralPins::kRelay1, PeripheralPins::kRelay2,
                       pinMode, digitalWrite);

void printState() {
  Serial.print("Relay 1: ");
  Serial.println(relays.isOn(1) ? "ON" : "OFF");
  Serial.print("Relay 2: ");
  Serial.println(relays.isOn(2) ? "ON" : "OFF");
}

void setup() {
  Serial.begin(115200);
  relays.begin();

  Serial.println();
  Serial.println("2-channel relay test ready");
  Serial.println("Jumpers must be Low + Com (low-level trigger).");
  Serial.println("Send 1: toggle relay 1 | 2: toggle relay 2 | 0: all off | r: state");
  printState();
}

void loop() {
  if (!Serial.available()) {
    return;
  }

  const char command = static_cast<char>(Serial.read());
  if (command == '1') {
    relays.setChannel(1, !relays.isOn(1));
    Serial.println("Relay 1 toggled");
  } else if (command == '2') {
    relays.setChannel(2, !relays.isOn(2));
    Serial.println("Relay 2 toggled");
  } else if (command == '0') {
    relays.allOff();
    Serial.println("Both relays off");
  } else if (command != '\r' && command != '\n' && command != 'r' && command != 'R') {
    Serial.println("Unknown command: use 1, 2, 0, or r");
  }

  if (command == '1' || command == '2' || command == '0' || command == 'r' || command == 'R') {
    printState();
  }
}
