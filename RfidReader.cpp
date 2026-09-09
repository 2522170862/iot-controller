#include "RfidReader.h"

#include <stdio.h>

RfidReader::RfidReader(uint8_t ssPin, uint8_t resetPin)
    : reader_(ssPin, resetPin) {}

void RfidReader::begin() {
  reader_.PCD_Init();
  configureTjdzCompatibleReader();
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
  }
}

bool RfidReader::poll() {
  byte atqa[2] = {};
  byte atqaSize = sizeof(atqa);
  const MFRC522::StatusCode requestStatus =
      reader_.PICC_RequestA(atqa, &atqaSize);
  if (requestStatus != MFRC522::STATUS_OK) {
    printSearchStatus(requestStatus);
    return false;
  }

  if (!reader_.PICC_ReadCardSerial()) {
    Serial.println("RFID card detected, but UID read failed");
    return false;
  }

  storeUid();
  hasCard_ = true;
  reader_.PICC_HaltA();
  reader_.PCD_StopCrypto1();
  return true;
}

bool RfidReader::hasCard() const { return hasCard_; }

const char* RfidReader::cardUid() const { return hasCard_ ? uid_ : nullptr; }

void RfidReader::configureTjdzCompatibleReader() {
  // The TJDZ module's reference program uses these values for its
  // RC522-compatible controller instead of the Arduino library defaults.
  reader_.PCD_WriteRegister(MFRC522::TModeReg, 0x8D);
  reader_.PCD_WriteRegister(MFRC522::TPrescalerReg, 0x3E);
  reader_.PCD_WriteRegister(MFRC522::TReloadRegH, 0x00);
  reader_.PCD_WriteRegister(MFRC522::TReloadRegL, 30);
  reader_.PCD_WriteRegister(MFRC522::TxASKReg, 0x40);
  reader_.PCD_WriteRegister(MFRC522::ModeReg, 0x3D);
  reader_.PCD_WriteRegister(MFRC522::RFCfgReg, 0x7F);
  delay(10);
  reader_.PCD_AntennaOn();
}

void RfidReader::printSearchStatus(MFRC522::StatusCode status) {
  const uint32_t nowMs = millis();
  if (nowMs - lastDiagnosticMs_ < kDiagnosticIntervalMs) {
    return;
  }

  lastDiagnosticMs_ = nowMs;
  Serial.print("RFID search: ");
  if (status == MFRC522::STATUS_TIMEOUT) {
    Serial.println("no ISO14443A card detected");
  } else {
    Serial.println(MFRC522::GetStatusCodeName(status));
  }
}

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
