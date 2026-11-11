// #include <Arduino.h>

// constexpr uint8_t KY032_OUT_PIN = 7;

// void setup() {
//   Serial.begin(115200);
//   delay(1500);

//   Serial.println();
//   Serial.println("Cyber Bowl: KY-032 test");

//   pinMode(KY032_OUT_PIN, INPUT);
// }

// void loop() {
//   int rawValue = digitalRead(KY032_OUT_PIN);

//   // У большинства KY-032 LOW означает обнаруженное препятствие.
//   bool obstacleDetected = (rawValue == LOW);

//   Serial.printf(
//     "OUT = %d | %s\n",
//     rawValue,
//     obstacleDetected ? "obstacle detected" : "clear"
//   );

//   delay(250);
// }