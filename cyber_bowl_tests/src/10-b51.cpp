// #include <Arduino.h>

// // Датчик подключен сигнальным пином S к GPIO 1
// constexpr uint8_t WATER_SENSOR_PIN = 1; 

// void setup() {
//   Serial.begin(115200);
//   delay(1500);

//   Serial.println();
//   Serial.println("Cyber Bowl: Water Level Sensor Test (B51)");

//   // Настраиваем пин на вход. На ESP32 для аналогового чтения pinMode 
//   // строго говоря необязателен, но для явности кода лучше указать.
//   pinMode(WATER_SENSOR_PIN, INPUT);
// }

// void loop() {
//   // Считываем аналоговое значение (на ESP32-S3 разрешение ADC по умолчанию 12 бит, от 0 до 4095)
//   int rawValue = analogRead(WATER_SENSOR_PIN);

//   // Выводим сырые данные в Монитор порта
//   Serial.print("Raw ADC Value: ");
//   Serial.print(rawValue);

//   // Примерная интерпретация значений (откалибруйте под свою воду):
//   if (rawValue < 500) {
//     Serial.println(" -> Сухо (Датчик на воздухе)");
//   } else if (rawValue >= 500 && rawValue < 1500) {
//     Serial.println(" -> Низкий уровень (Датчик слегка погружен)");
//   } else if (rawValue >= 1500 && rawValue < 2800) {
//     Serial.println(" -> Средний уровень");
//   } else {
//     Serial.println(" -> Высокий уровень / Полное погружение");
//   }

//   delay(500); // Опрос два раза в секунду
// }
