// #include <Arduino.h>
// #include <HX711.h>

// constexpr uint8_t HX711_DOUT_PIN = 41;
// constexpr uint8_t HX711_SCK_PIN = 42;

// // Начальный калибровочный коэффициент
// float calibration_factor = 194.0f;

// HX711 scale;

// void printMenu() {
//   Serial.println("\n--- Команды управления ---");
//   Serial.println(" 't' или 'T' — Сбросить вес в ноль (Tare)");
//   Serial.println(" '+' или 'a' — Увеличить делитель калибровки на 10");
//   Serial.println(" '-' или 'z' — Уменьшить делитель калибровки на 10");
//   Serial.println("---------------------------\n");
// }

// void setup() {
//   Serial.begin(115200);
//   delay(1500);

//   Serial.println("\n--- HX711 Load Cell Interactive Test ---");

//   scale.begin(HX711_DOUT_PIN, HX711_SCK_PIN);

//   Serial.print("Проверка подключения HX711...");
//   if (!scale.is_ready()) {
//     Serial.println(" Ошибка! Модуль HX711 не найден. Проверьте контакты DOUT и SCK.");
//     while (true) {
//       delay(1000);
//     }
//   }
//   Serial.println(" ОК!");

//   scale.set_scale(calibration_factor);

//   Serial.println("Освободите платформу от груза. Выполняется установка нуля...");
//   scale.tare(20);
//   Serial.println("Ноль установлен!");

//   printMenu();
// }

// void loop() {
//   // 1. Проверка пользовательского ввода через Serial
//   if (Serial.available()) {
//     char ch = Serial.read();
    
//     if (ch == 't' || ch == 'T') {
//       Serial.println("\n[Сброс в ноль...]");
//       scale.tare(20);
//       Serial.println("[Ноль установлен]");
//     } 
//     else if (ch == '+' || ch == 'a') {
//       calibration_factor += 1.0f;
//       scale.set_scale(calibration_factor);
//       Serial.printf("\n[Калибровочный коэффициент увеличен: %.1f]\n", calibration_factor);
//     } 
//     else if (ch == '-' || ch == 'z') {
//       calibration_factor -= 1.0f;
//       scale.set_scale(calibration_factor);
//       Serial.printf("\n[Калибровочный коэффициент уменьшен: %.1f]\n", calibration_factor);
//     }
//   }

//   // 2. Чтение данных
//   if (scale.is_ready()) {
//     long raw_value = scale.read_average(3);  // Сырое значение АЦП
//     float grams = scale.get_units(5);        // Расчитанный вес в граммах

//     // Отсечка шума около нуля
//     if (fabs(grams) < 0.5f) {
//       grams = 0.0f;
//     }

//     Serial.printf("RAW: %9ld | Вес: %7.1f г | Калибровка: %.1f\n", raw_value, grams, calibration_factor);
//   } else {
//     Serial.println("HX711: Не готов к чтению (Not Ready)");
//   }

//   delay(500);
// }