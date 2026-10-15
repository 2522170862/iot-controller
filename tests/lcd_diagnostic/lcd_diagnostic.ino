#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

namespace {
constexpr int8_t kLcdCs = 9;
constexpr int8_t kLcdDc = 10;
constexpr int8_t kLcdMosi = 11;
constexpr int8_t kLcdSck = 12;
constexpr int8_t kSpiMiso = 13;
constexpr int8_t kRfidCs = 14;
constexpr int8_t kLcdReset = -1;  // LCD RST is connected to ESP32-S3 EN.

constexpr uint16_t kWidth = 240;
constexpr uint16_t kHeight = 320;
constexpr uint32_t kInitialSpiHz = 10000000;
constexpr uint32_t kCounterIntervalMs = 1000;

Adafruit_ST7789 lcd(&SPI, kLcdCs, kLcdDc, kLcdReset);
uint32_t spiHz = kInitialSpiHz;
uint32_t counter = 0;
uint32_t lastCounterMs = 0;

void printStage(uint8_t stage, const char* message) {
  Serial.print("[LCD TEST ");
  Serial.print(stage);
  Serial.print("] ");
  Serial.println(message);
}

void drawTextScreen(const char* title, uint16_t color) {
  lcd.fillScreen(ST77XX_BLACK);
  lcd.drawRect(4, 4, kWidth - 8, kHeight - 8, color);
  lcd.setTextWrap(false);
  lcd.setTextColor(color);
  lcd.setTextSize(3);
  lcd.setCursor(22, 60);
  lcd.println(title);

  lcd.setTextColor(ST77XX_WHITE);
  lcd.setTextSize(2);
  lcd.setCursor(22, 120);
  lcd.print("SPI: ");
  lcd.print(spiHz / 1000000UL);
  lcd.println(" MHz");
  lcd.setCursor(22, 155);
  lcd.println("LCD 240x320");
  lcd.setCursor(22, 190);
  lcd.println("If visible:");
  lcd.setCursor(22, 220);
  lcd.println("SPI works");
}

void runColorTest() {
  printStage(4, "RED screen");
  lcd.fillScreen(ST77XX_RED);
  delay(800);

  printStage(5, "GREEN screen");
  lcd.fillScreen(ST77XX_GREEN);
  delay(800);

  printStage(6, "BLUE screen");
  lcd.fillScreen(ST77XX_BLUE);
  delay(800);

  printStage(7, "WHITE screen");
  lcd.fillScreen(ST77XX_WHITE);
  delay(800);

  printStage(8, "BLACK screen");
  lcd.fillScreen(ST77XX_BLACK);
  delay(500);
}

void drawGraphicsTest() {
  printStage(9, "Drawing text and graphics");
  drawTextScreen("LCD OK", ST77XX_GREEN);
  lcd.fillCircle(60, 275, 18, ST77XX_RED);
  lcd.fillCircle(120, 275, 18, ST77XX_GREEN);
  lcd.fillCircle(180, 275, 18, ST77XX_BLUE);
}

void initializeDisplay() {
  printStage(2, "Initializing ST7789");
  lcd.init(kWidth, kHeight);
  lcd.setRotation(0);
  lcd.setSPISpeed(spiHz);
  lcd.setTextWrap(false);
  printStage(3, "ST7789 initialization returned");
}

void setSpiSpeed(uint32_t newSpeedHz) {
  spiHz = newSpeedHz;
  lcd.setSPISpeed(spiHz);
  Serial.print("SPI speed changed to ");
  Serial.print(spiHz / 1000000UL);
  Serial.println(" MHz");
  drawTextScreen("SPEED TEST", ST77XX_YELLOW);
}

void printCommands() {
  Serial.println();
  Serial.println("Serial commands:");
  Serial.println("  1 = use 10 MHz SPI");
  Serial.println("  2 = use 20 MHz SPI");
  Serial.println("  4 = use 40 MHz SPI");
  Serial.println("  c = repeat color test");
  Serial.println("  g = redraw graphics test");
  Serial.println("  r = reinitialize LCD");
  Serial.println("  h = show these commands");
}

void handleSerialCommand(char command) {
  switch (command) {
    case '1':
      setSpiSpeed(10000000);
      break;
    case '2':
      setSpiSpeed(20000000);
      break;
    case '4':
      setSpiSpeed(40000000);
      break;
    case 'c':
    case 'C':
      runColorTest();
      drawGraphicsTest();
      break;
    case 'g':
    case 'G':
      drawGraphicsTest();
      break;
    case 'r':
    case 'R':
      initializeDisplay();
      runColorTest();
      drawGraphicsTest();
      break;
    case 'h':
    case 'H':
      printCommands();
      break;
    case '\r':
    case '\n':
      break;
    default:
      Serial.print("Unknown command: ");
      Serial.println(command);
      printCommands();
      break;
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);
  printStage(1, "ESP32-S3 started");

  // Keep both SPI devices deselected before starting the shared bus.
  pinMode(kLcdCs, OUTPUT);
  digitalWrite(kLcdCs, HIGH);
  pinMode(kRfidCs, OUTPUT);
  digitalWrite(kRfidCs, HIGH);

  Serial.println("Expected wiring:");
  Serial.println("  CS=GPIO9, DC=GPIO10, MOSI=GPIO11");
  Serial.println("  SCK=GPIO12, MISO=GPIO13, RST=EN");
  Serial.println("Disconnect RC522 and all motors during this test.");

  SPI.begin(kLcdSck, kSpiMiso, kLcdMosi, kLcdCs);
  initializeDisplay();
  runColorTest();
  drawGraphicsTest();
  printStage(10, "Continuous refresh test started");
  printCommands();
}

void loop() {
  while (Serial.available() > 0) {
    handleSerialCommand(static_cast<char>(Serial.read()));
  }

  const uint32_t nowMs = millis();
  if (nowMs - lastCounterMs < kCounterIntervalMs) {
    return;
  }

  lastCounterMs = nowMs;
  ++counter;

  lcd.fillRect(20, 15, 205, 28, ST77XX_BLACK);
  lcd.setTextColor(ST77XX_CYAN);
  lcd.setTextSize(2);
  lcd.setCursor(20, 20);
  lcd.print("RUN ");
  lcd.print(counter);
  lcd.print(" s");

  Serial.print("LCD refresh counter: ");
  Serial.print(counter);
  Serial.print(" s, SPI=");
  Serial.print(spiHz / 1000000UL);
  Serial.println(" MHz");
}
