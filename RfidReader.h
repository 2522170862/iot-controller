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

  MFRC522 reader_;
  char uid_[kUidBufferSize] = {};
  bool hasCard_ = false;

  void storeUid();
};
