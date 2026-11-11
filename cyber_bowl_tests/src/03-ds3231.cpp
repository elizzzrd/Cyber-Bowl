// #include <Arduino.h>
// #include <Wire.h>           // работа с I^2C
// #include <RTClib.h>         // библиотека для работы с часами

// constexpr uint8_t SDA_PIN = 8;      // SDA - линия данных I^2C
// constexpr uint8_t SCL_PIN = 9;      // SCL - линия тактового сигнала I^2C

// // При первом запуске true: модуль получит время компиляции.
// // После успешной установки времени поменяем на false.
// constexpr bool SET_RTC_TIME = true;


// // Создаём объект rtc — программное представление модуля DS3231.
// // Через него далее вызываем rtc.begin(), rtc.now() и rtc.adjust()
// RTC_DS3231 rtc;

// void scanI2C() {
//   bool found = false;

//   Serial.println("I2C scan:");

//   for (uint8_t address = 1; address < 127; address++) {
//     Wire.beginTransmission(address);

//     if (Wire.endTransmission() == 0) {
//       Serial.printf("  Device found at 0x%02X\n", address);
//       found = true;
//     }
//   }

//   if (!found) {
//     Serial.println("  No I2C devices found");
//   }
// }

// void setup() {
//   Serial.begin(115200);
//   delay(1500);

//   Serial.println();
//   Serial.println("Cyber Bowl: DS3231 test");

//   Wire.begin(SDA_PIN, SCL_PIN);
//   scanI2C();

//   if (!rtc.begin()) {
//     Serial.println("DS3231 not found");
//     while (true) {
//       delay(1000);
//     }
//   }

//   if (rtc.lostPower()) {
//     Serial.println("DS3231 lost power: time must be set");
//   }

//   if (SET_RTC_TIME) {
//     rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
//     Serial.println("Time set from compilation time");
//   }
// }

// void loop() {
//   DateTime now = rtc.now();

//   Serial.printf(
//     "%04d-%02d-%02d %02d:%02d:%02d\n",
//     now.year(), now.month(), now.day(),
//     now.hour(), now.minute(), now.second()
//   );

//   delay(1000);
// }