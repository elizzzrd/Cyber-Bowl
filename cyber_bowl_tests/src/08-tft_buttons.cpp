// #include <Arduino.h>
// #include <SPI.h>
// #include <Adafruit_GFX.h>
// #include <Adafruit_ST7789.h>

// // TFT-дисплей
// constexpr uint8_t TFT_SCLK = 10;
// constexpr uint8_t TFT_MOSI = 11;
// constexpr uint8_t TFT_DC = 12;
// constexpr uint8_t TFT_RST = 13;
// constexpr uint8_t TFT_CS = 14;
// constexpr uint8_t TFT_BL = 6;

// // Кнопки
// constexpr uint8_t BUTTON_UP = 17;
// constexpr uint8_t BUTTON_DOWN = 18;
// constexpr uint8_t BUTTON_OK = 21;
// constexpr uint8_t BUTTON_BACK = 20;

// constexpr uint8_t MENU_ITEMS_COUNT = 4;

// const char* menuItems[MENU_ITEMS_COUNT] = {
//   "Feed now",
//   "Schedule",
//   "Water",
//   "Settings"
// };

// Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

// uint8_t selectedItem = 0;

// struct Button {
//   uint8_t pin;
//   bool lastReading;
//   bool stableState;
//   unsigned long lastChangeTime;
// };

// Button upButton   = {BUTTON_UP, HIGH, HIGH, 0};
// Button downButton = {BUTTON_DOWN, HIGH, HIGH, 0};
// Button okButton   = {BUTTON_OK, HIGH, HIGH, 0};
// Button backButton = {BUTTON_BACK, HIGH, HIGH, 0};

// bool wasPressed(Button& button) {
//   bool reading = digitalRead(button.pin);

//   if (reading != button.lastReading) {
//     button.lastChangeTime = millis();
//   }

//   if (millis() - button.lastChangeTime > 30) {
//     if (reading != button.stableState) {
//       button.stableState = reading;

//       if (button.stableState == LOW) {
//         button.lastReading = reading;
//         return true;
//       }
//     }
//   }

//   button.lastReading = reading;
//   return false;
// }

// void drawMenu() {
//   tft.fillScreen(ST77XX_BLACK);

//   tft.setTextColor(ST77XX_WHITE);
//   tft.setTextSize(2);
//   tft.setCursor(18, 15);
//   tft.println("Cyber Bowl");

//   tft.setTextSize(1);
//   tft.setTextColor(ST77XX_CYAN);
//   tft.setCursor(18, 45);
//   tft.println("Local control menu");

//   for (uint8_t i = 0; i < MENU_ITEMS_COUNT; i++) {
//     int y = 70 + i * 38;
//     bool isSelected = (i == selectedItem);

//     if (isSelected) {
//       tft.fillRoundRect(15, y, 290, 30, 5, ST77XX_BLUE);
//       tft.setTextColor(ST77XX_WHITE);
//     } else {
//       tft.drawRoundRect(15, y, 290, 30, 5, ST77XX_ORANGE);
//       tft.setTextColor(ST77XX_WHITE);
//     }

//     tft.setTextSize(2);
//     tft.setCursor(30, y + 7);
//     tft.print(menuItems[i]);
//   }

//   tft.setTextSize(1);
//   tft.setTextColor(ST77XX_YELLOW);
//   tft.setCursor(18, 220);
//   tft.println("UP/DOWN: select  OK: enter");
// }

// void setup() {
//   Serial.begin(115200);
//   delay(1500);

//   pinMode(BUTTON_UP, INPUT_PULLUP);
//   pinMode(BUTTON_DOWN, INPUT_PULLUP);
//   pinMode(BUTTON_OK, INPUT_PULLUP);
//   pinMode(BUTTON_BACK, INPUT_PULLUP);

//   pinMode(TFT_BL, OUTPUT);
//   digitalWrite(TFT_BL, HIGH);

//   SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

//   tft.init(240, 320);
//   tft.invertDisplay(false);
//   tft.setRotation(1);

//   drawMenu();

//   Serial.println("Cyber Bowl: TFT buttons test");
// }

// void loop() {
//   if (wasPressed(upButton)) {
//     selectedItem = (selectedItem + MENU_ITEMS_COUNT - 1) % MENU_ITEMS_COUNT;
//     drawMenu();
//     Serial.println("UP");
//   }

//   if (wasPressed(downButton)) {
//     selectedItem = (selectedItem + 1) % MENU_ITEMS_COUNT;
//     drawMenu();
//     Serial.println("DOWN");
//   }

//   if (wasPressed(okButton)) {
//     Serial.printf("OK: %s\n", menuItems[selectedItem]);
//   }

//   if (wasPressed(backButton)) {
//     Serial.println("BACK");
//   }
// }