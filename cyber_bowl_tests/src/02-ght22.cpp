// #include <Arduino.h>
// #include <DHT.h>

// constexpr uint8_t DHT_PIN = 5;
// constexpr uint8_t DHT_TYPE = DHT22;

// DHT dht(DHT_PIN, DHT_TYPE);

// void setup() {
//   Serial.begin(115200);
//   delay(1500);

//   Serial.println();
//   Serial.println("Cyber Bowl: DHT22 test");

//   dht.begin();

//   // DHT22 нужно время после включения питания.
//   delay(2000);
// }

// void loop() {
//   float humidity = dht.readHumidity();
//   float temperature = dht.readTemperature();

//   if (isnan(humidity) || isnan(temperature)) {
//     Serial.println("DHT22 read error");
//   } else {
//     Serial.printf(
//       "Temperature: %.1f C | Humidity: %.1f %%\n",
//       temperature,
//       humidity
//     );
//   }

//   // DHT22 нельзя опрашивать слишком часто.
//   delay(2500);
// }