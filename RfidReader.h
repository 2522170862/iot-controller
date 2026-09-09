#pragma once

#include <Arduino.h>
#include <MFRC522.h>

class RfidReader {
 public:
  RfidReader(uint8_t ssPin, uint8_t resetPin);

  void begin();
  bool poll();
  bool hasCard() const;
  const char* cardUid() const;

 private:
  static constexpr size_t kUidBufferSize = 32;
  static constexpr uint32_t kDiagnosticIntervalMs = 1000;

  MFRC522 reader_;
  char uid_[kUidBufferSize] = {};
  bool hasCard_ = false;
  uint32_t lastDiagnosticMs_ = 0;

  void configureTjdzCompatibleReader();
  void printSearchStatus(MFRC522::StatusCode status);
  void storeUid();
};
