#include <MFRC522.h>
#include <SPI.h>

namespace {
constexpr uint8_t kSckPin = 12;
constexpr uint8_t kMosiPin = 11;
constexpr uint8_t kMisoPin = 13;
constexpr uint8_t kSsPin = 14;
constexpr uint8_t kResetPin = 15;
constexpr uint8_t kLcdCsPin = 8;
constexpr uint32_t kTestIntervalMs = 500;
constexpr uint32_t kDiagnosticSpiHz = 250000;

MFRC522 reader(kSsPin, kResetPin);
uint32_t lastTestMs = 0;

byte readRegisterSlow(byte address) {
  SPI.beginTransaction(SPISettings(kDiagnosticSpiHz, MSBFIRST, SPI_MODE0));
  digitalWrite(kSsPin, LOW);
  SPI.transfer(0x80 | ((address << 1) & 0x7E));
  const byte value = SPI.transfer(0x00);
  digitalWrite(kSsPin, HIGH);
  SPI.endTransaction();
  return value;
}

void writeRegisterSlow(byte address, byte value) {
  SPI.beginTransaction(SPISettings(kDiagnosticSpiHz, MSBFIRST, SPI_MODE0));
  digitalWrite(kSsPin, LOW);
  SPI.transfer((address << 1) & 0x7E);
  SPI.transfer(value);
  digitalWrite(kSsPin, HIGH);
  SPI.endTransaction();
}

void printHexByte(byte value) {
  if (value < 0x10) {
    Serial.print('0');
  }
  Serial.print(value, HEX);
}

void configureTjdzReader() {
  reader.PCD_WriteRegister(MFRC522::TModeReg, 0x8D);
  reader.PCD_WriteRegister(MFRC522::TPrescalerReg, 0x3E);
  reader.PCD_WriteRegister(MFRC522::TReloadRegH, 0x00);
  reader.PCD_WriteRegister(MFRC522::TReloadRegL, 30);
  reader.PCD_WriteRegister(MFRC522::TxASKReg, 0x40);
  reader.PCD_WriteRegister(MFRC522::ModeReg, 0x3D);
  reader.PCD_WriteRegister(MFRC522::RFCfgReg, 0x7F);
  delay(10);
  reader.PCD_AntennaOn();
}

void printReaderRegisters() {
  Serial.print("VersionReg=0x");
  printHexByte(reader.PCD_ReadRegister(MFRC522::VersionReg));
  Serial.print(" TxControlReg=0x");
  printHexByte(reader.PCD_ReadRegister(MFRC522::TxControlReg));
  Serial.print(" RFCfgReg=0x");
  printHexByte(reader.PCD_ReadRegister(MFRC522::RFCfgReg));
  Serial.println();
}

void runSlowSpiWriteTest() {
  constexpr byte kVersionAddress = 0x37;
  constexpr byte kReloadLowAddress = 0x2D;
  constexpr byte kTestValue = 0x5A;

  const byte version = readRegisterSlow(kVersionAddress);
  const byte originalValue = readRegisterSlow(kReloadLowAddress);
  writeRegisterSlow(kReloadLowAddress, kTestValue);
  const byte readBack = readRegisterSlow(kReloadLowAddress);
  writeRegisterSlow(kReloadLowAddress, originalValue);

  Serial.print("Slow SPI self-test: version=0x");
  printHexByte(version);
  Serial.print(" write 0x");
  printHexByte(kTestValue);
  Serial.print(" read 0x");
  printHexByte(readBack);
  Serial.println(readBack == kTestValue ? " PASS" : " FAIL");
}

void printUid() {
  Serial.print("RFID UID: ");
  for (byte index = 0; index < reader.uid.size; ++index) {
    if (index != 0) {
      Serial.print(':');
    }
    printHexByte(reader.uid.uidByte[index]);
  }
  Serial.println();
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(kSsPin, OUTPUT);
  digitalWrite(kSsPin, HIGH);
  pinMode(kLcdCsPin, OUTPUT);
  digitalWrite(kLcdCsPin, HIGH);
  SPI.begin(kSckPin, kMisoPin, kMosiPin, kSsPin);
  runSlowSpiWriteTest();
  reader.PCD_Init();
  configureTjdzReader();

  Serial.println("RC522 standalone diagnostic started");
  Serial.println("Place a 13.56MHz ISO14443A card on the antenna");
  printReaderRegisters();
}

void loop() {
  const uint32_t nowMs = millis();
  if (nowMs - lastTestMs < kTestIntervalMs) {
    return;
  }
  lastTestMs = nowMs;

  byte atqa[2] = {};
  byte atqaSize = sizeof(atqa);
  const MFRC522::StatusCode status = reader.PICC_WakeupA(atqa, &atqaSize);
  if (status == MFRC522::STATUS_TIMEOUT) {
    Serial.println("WUPA: no card response");
    return;
  }
  if (status != MFRC522::STATUS_OK && status != MFRC522::STATUS_COLLISION) {
    Serial.print("WUPA error: ");
    Serial.println(MFRC522::GetStatusCodeName(status));
    return;
  }

  Serial.print("Card RF response, ATQA=");
  printHexByte(atqa[0]);
  Serial.print(' ');
  printHexByte(atqa[1]);
  Serial.println();

  if (reader.PICC_ReadCardSerial()) {
    printUid();
    reader.PICC_HaltA();
    reader.PCD_StopCrypto1();
  } else {
    Serial.println("Card responded, UID selection failed");
  }
}
