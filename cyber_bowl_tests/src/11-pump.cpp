// #include <Arduino.h>

// constexpr uint8_t PUMP_PIN = 4;

// // PWM для XY-MOS
// constexpr uint8_t PWM_CHANNEL = 0;
// constexpr uint16_t PWM_FREQUENCY = 1000; // 1 кГц
// constexpr uint8_t PWM_RESOLUTION = 8;    // 0...255

// uint8_t currentPower = 0;

// bool timedRun = false;
// uint32_t stopTime = 0;

// void setPumpPower(uint8_t percent)
// {
//     percent = constrain(percent, 0, 100);

//     currentPower = percent;

//     const uint8_t pwmValue = map(percent, 0, 100, 0, 255);
//     ledcWrite(PWM_CHANNEL, pwmValue);

//     Serial.print("Pump power: ");
//     Serial.print(percent);
//     Serial.println(" %");
// }

// void runFor(uint8_t percent, uint32_t durationMs)
// {
//     setPumpPower(percent);

//     timedRun = true;
//     stopTime = millis() + durationMs;

//     Serial.print("Timed run: ");
//     Serial.print(durationMs / 1000);
//     Serial.println(" seconds");
// }

// void printMenu()
// {
//     Serial.println();
//     Serial.println("=== Kamoer pump test ===");
//     Serial.println("0 - OFF");
//     Serial.println("1 - 25%");
//     Serial.println("2 - 50%");
//     Serial.println("3 - 75%");
//     Serial.println("4 - 100%");
//     Serial.println("t - 100% for 3 seconds");
//     //всSerial.println("c - 100% for 10 seconds (calibration)");
//     Serial.println("s - show current power");
//     Serial.println("h - show menu");
//     Serial.println();
// }

// // назад // вверх // вниз // ОКя

// constexpr float PUMP_MS_PER_ML = 952.0f;

// void dispenseWater(float milliliters)
// {
//     if (milliliters <= 0) {
//         return;
//     }

//     const uint32_t durationMs =
//         static_cast<uint32_t>(milliliters * PUMP_MS_PER_ML);

//     Serial.print("Dispensing ");
//     Serial.print(milliliters, 1);
//     Serial.print(" ml, duration ");
//     Serial.print(durationMs);
//     Serial.println(" ms");

//     setPumpPower(100);

//     timedRun = true;
//     stopTime = millis() + durationMs;
// }




// void setup()
// {
//     Serial.begin(115200);
//     delay(1000);

//     ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
//     ledcAttachPin(PUMP_PIN, PWM_CHANNEL);

//     setPumpPower(0);
//     printMenu();
// }

// void loop()
// {
//     // Останавливаем помпу после завершения таймера
//     if (timedRun && static_cast<int32_t>(millis() - stopTime) >= 0) {
//         timedRun = false;
//         setPumpPower(0);
//         Serial.println("Timed run completed");
//     }

//     if (!Serial.available()) {
//         return;
//     }

//     const char command = Serial.read();

//     switch (command) {
//         case '0':
//             timedRun = false;
//             setPumpPower(0);
//             break;

//         case '1':
//             timedRun = false;
//             setPumpPower(25);
//             break;

//         case '2':
//             timedRun = false;
//             setPumpPower(50);
//             break;

//         case '3':
//             timedRun = false;
//             setPumpPower(75);
//             break;

//         case '4':
//             timedRun = false;
//             setPumpPower(100);
//             break;

//         case 't':
//             runFor(100, 3000);
//             break;



//         case 's':
//             Serial.print("Current power: ");
//             Serial.print(currentPower);
//             Serial.println(" %");
//             break;

//         case 'h':
//             printMenu();
//             break;
//         case 'a':
//             dispenseWater(10);
//             break;

//         case 'b':
//             dispenseWater(25);
//             break;

//         case 'c':
//             dispenseWater(50);
//             break;

//         case 'd':
//             dispenseWater(100);
//             break;

//         case '\r':
//         case '\n':
//             break;

//         default:
//             Serial.println("Unknown command. Press h for help.");
//             break;
//     }
// }