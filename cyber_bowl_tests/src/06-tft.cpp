// #include <Arduino.h>
// #include <SPI.h>
// #include <Adafruit_GFX.h>
// #include <Adafruit_ST7789.h>

// constexpr uint8_t TFT_SCLK = 10;
// constexpr uint8_t TFT_MOSI = 11;
// constexpr uint8_t TFT_DC = 12;
// constexpr uint8_t TFT_RST = 13;
// constexpr uint8_t TFT_CS = 14;
// constexpr uint8_t TFT_BL = 6;

// Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

// void setup() {
//   Serial.begin(115200);
//   delay(1500);

//   Serial.println("Cyber Bowl: ST7789 display test");

//   pinMode(TFT_BL, OUTPUT);
//   digitalWrite(TFT_BL, HIGH);

//   SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

//   // Физическое разрешение контроллера: 240 × 320.
//   tft.init(240, 320);
//   tft.invertDisplay(false);

//   // Поворот 1 делает экран альбомным: 320 × 240.
//   tft.setRotation(1);

//   tft.fillScreen(ST77XX_BLACK);

//   tft.setTextWrap(false);
//   tft.setTextColor(ST77XX_WHITE);
//   tft.setTextSize(2);
//   tft.setCursor(20, 20);
//   tft.println("Cyber Bowl");

//   tft.setTextSize(1);
//   tft.setCursor(20, 55);
//   tft.println("ESP32-S3 + ST7789V");

//   tft.fillRect(20, 90, 80, 50, ST77XX_RED);
//   tft.fillRect(120, 90, 80, 50, ST77XX_GREEN);
//   tft.fillRect(220, 90, 80, 50, ST77XX_BLUE);

//   tft.drawRect(20, 165, 280, 45, ST77XX_YELLOW);

//   tft.setTextColor(ST77XX_YELLOW);
//   tft.setTextSize(2);
//   tft.setCursor(35, 178);
//   tft.println("Display works");

//   Serial.println("Image sent to display");
// }

// void loop() {
//   delay(1000);
// }