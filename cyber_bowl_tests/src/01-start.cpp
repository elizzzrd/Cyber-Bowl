// #include <Arduino.h>

// void setup() {
//   Serial.begin(115200);
//   delay(1500);

//   Serial.println();
//   Serial.println("Cyber Bowl: ESP32-S3 board check");

//   Serial.printf("Chip: %s, revision %d, cores %d\n",
//                 ESP.getChipModel(),
//                 ESP.getChipRevision(),
//                 ESP.getChipCores());

//   Serial.printf("Flash: %u MB\n",
//                 (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)));

//   Serial.printf("PSRAM found: %s\n", psramFound() ? "yes" : "no");
//   Serial.printf("PSRAM: %u MB\n",
//                 (unsigned)(ESP.getPsramSize() / (1024 * 1024)));

//   Serial.printf("Free heap: %u bytes\n", (unsigned)ESP.getFreeHeap());
// }

// void loop() {
//   Serial.printf("Uptime: %lu s\n", millis() / 1000);
//   delay(1000);
// }
