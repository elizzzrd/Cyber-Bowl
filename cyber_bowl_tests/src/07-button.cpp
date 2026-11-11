// #include <Arduino.h>

// constexpr uint8_t BUTTON_PIN = 2;
// constexpr uint8_t LED_PIN = 5;

// bool previousState = HIGH;

// void setup() {
//   Serial.begin(115200);
//   delay(1500);

//   pinMode(BUTTON_PIN, INPUT_PULLUP);
//   pinMode(LED_PIN, OUTPUT);

//   digitalWrite(LED_PIN, LOW);

//   Serial.println();
//   Serial.println("Cyber Bowl: button and LED test");
// }

// void loop() {
//   bool currentState = digitalRead(BUTTON_PIN);

//   if (previousState == HIGH && currentState == LOW) {
//     digitalWrite(LED_PIN, HIGH);
//     Serial.println("Button pressed: LED ON");
//   }

//   if (previousState == LOW && currentState == HIGH) {
//     digitalWrite(LED_PIN, LOW);
//     Serial.println("Button released: LED OFF");
//   }

//   previousState = currentState;

//   delay(20);
// }