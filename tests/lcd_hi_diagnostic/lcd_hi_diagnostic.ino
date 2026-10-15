#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

namespace {
constexpr int8_t kLcdCs = 9;
constexpr int8_t kLcdDc = 10;
constexpr int8_t kLcdMosi = 11;
constexpr int8_t kLcdSck = 12;
constexpr int8_t kSpiMiso = 13;
constexpr int8_t kLcdReset = 15;

constexpr uint16_t kLcdWidth = 240;
constexpr uint16_t kLcdHeight = 320;
constexpr uint32_t kSpiFrequencyHz = 1000000;

Adafruit_ST7789 lcd(&SPI, kLcdCs, kLcdDc, kLcdReset);

void checkpoint(uint8_t number, const char* message) {
  Serial.print("[CHECK ");
  Serial.print(number);
  Serial.print("] ");
  Serial.println(message);
  Serial.flush();
}

void resetLcd() {
  checkpoint(2, "Driving LCD RST HIGH");
  pinMode(kLcdReset, OUTPUT);
  digitalWrite(kLcdReset, HIGH);
  delay(20);

  checkpoint(3, "Driving LCD RST LOW");
  digitalWrite(kLcdReset, LOW);
  delay(20);

  checkpoint(4, "Releasing LCD RST HIGH");
  digitalWrite(kLcdReset, HIGH);
  delay(150);
}
}  // namespace

void setup() {
  Serial.begin(115200);

  // Do not wait forever when USB CDC is not enabled.
  const uint32_t serialWaitStartedMs = millis();
  while (!Serial && millis() - serialWaitStartedMs < 3000) {
    delay(10);
  }

  checkpoint(1, "ESP32-S3 setup started");
  Serial.println("Expected LCD wiring:");
  Serial.println("  CS=GPIO9, DC=GPIO10, MOSI/SDA=GPIO11");
  Serial.println("  SCK/SCL=GPIO12, RST/RES=GPIO15");
  Serial.println("  VCC=3.3V, GND=GND");
  Serial.flush();

  pinMode(kLcdCs, OUTPUT);
  digitalWrite(kLcdCs, HIGH);
  pinMode(kLcdDc, OUTPUT);
  digitalWrite(kLcdDc, LOW);

  resetLcd();

  checkpoint(5, "Calling SPI.begin");
  SPI.begin(kLcdSck, kSpiMiso, kLcdMosi, kLcdCs);
  checkpoint(6, "SPI.begin returned");

  // Set a deliberately low clock before and after initialization so long
  // jumper wires are less likely to corrupt commands.
  lcd.setSPISpeed(kSpiFrequencyHz);
  checkpoint(7, "Calling ST7789 init(240, 320)");
  lcd.init(kLcdWidth, kLcdHeight);
  checkpoint(8, "ST7789 init returned");

  lcd.setSPISpeed(kSpiFrequencyHz);
  lcd.setRotation(0);
  checkpoint(9, "Rotation set; clearing screen");
  lcd.fillScreen(ST77XX_BLACK);
  checkpoint(10, "Screen clear returned");

  lcd.setTextWrap(false);
  lcd.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  lcd.setTextSize(8);
  lcd.setCursor(72, 120);
  checkpoint(11, "Drawing HI");
  lcd.print("HI");
  checkpoint(12, "HI draw returned; entering heartbeat loop");
}

void loop() {
  static uint32_t lastHeartbeatMs = 0;
  static uint32_t heartbeat = 0;
  const uint32_t nowMs = millis();

  if (nowMs - lastHeartbeatMs < 1000) {
    return;
  }

  lastHeartbeatMs = nowMs;
  ++heartbeat;
  Serial.print("[HEARTBEAT] Program running for ");
  Serial.print(heartbeat);
  Serial.println(" seconds");
  Serial.flush();
}
