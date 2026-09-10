#include "RfidReader.h"

#include <stdio.h>

RfidReader::RfidReader(uint8_t ssPin, uint8_t resetPin)
    : reader_(ssPin, resetPin) {}

void RfidReader::begin() {
  reader_.PCD_Init();
  delay(50);
  Serial.println("RC522 RFID reader started");
  const byte firmwareVersion =
      reader_.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print("RC522 firmware version: 0x");
  if (firmwareVersion < 0x10) {
    Serial.print('0');
  }
  Serial.println(firmwareVersion, HEX);

  if (firmwareVersion == 0x00 || firmwareVersion == 0xFF) {
    Serial.println("RC522 communication failed: check SPI wiring");
  } else {
    Serial.println("Place a card near the reader...");
  }
}

bool RfidReader::poll() {
  if (!reader_.PICC_IsNewCardPresent()) {
    return false;
  }

  if (!reader_.PICC_ReadCardSerial()) {
    return false;
  }

  storeUid();
  hasCard_ = true;

  const MFRC522::PICC_Type cardType =
      reader_.PICC_GetType(reader_.uid.sak);
  Serial.print("RFID card type: ");
  Serial.println(reader_.PICC_GetTypeName(cardType));

  reader_.PICC_HaltA();
  reader_.PCD_StopCrypto1();
  return true;
}

bool RfidReader::hasCard() const { return hasCard_; }

const char* RfidReader::cardUid() const { return hasCard_ ? uid_ : nullptr; }

void RfidReader::storeUid() {
  size_t offset = 0;
  uid_[0] = '\0';

  for (byte index = 0; index < reader_.uid.size && offset < sizeof(uid_);
       ++index) {
    const int written = snprintf(uid_ + offset, sizeof(uid_) - offset,
                                 index == 0 ? "%02X" : ":%02X",
                                 reader_.uid.uidByte[index]);
    if (written < 0 || static_cast<size_t>(written) >= sizeof(uid_) - offset) {
      uid_[sizeof(uid_) - 1] = '\0';
      break;
    }
    offset += static_cast<size_t>(written);
  }
}
