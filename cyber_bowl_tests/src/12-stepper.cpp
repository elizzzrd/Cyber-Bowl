#include <Arduino.h>

constexpr uint8_t STEP_PIN   = 38;
constexpr uint8_t DIR_PIN    = 39;
constexpr uint8_t ENABLE_PIN = 40;

constexpr uint32_t STEPS_PER_REVOLUTION = 200;
constexpr uint32_t TEST_REVOLUTIONS = 3;

// Начальная скорость разгона: 250 шагов/с.
constexpr uint32_t START_PERIOD_US = 4000;

uint32_t target_period_us = 4000;

void make_step(uint32_t period_us)
{
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(5);

    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(period_us - 5);
}

void move_steps(uint32_t steps, bool direction)
{
    constexpr uint32_t RAMP_STEPS = 50;

    digitalWrite(DIR_PIN, direction);
    delayMicroseconds(20);

    // ENABLE активен низким уровнем.
    digitalWrite(ENABLE_PIN, LOW);
    delay(20);

    for (uint32_t i = 0; i < steps; ++i) {
        const uint32_t remaining = steps - 1 - i;

        // Расстояние до ближайшего края движения.
        uint32_t ramp_position =
            (i < remaining) ? i : remaining;

        if (ramp_position > RAMP_STEPS) {
            ramp_position = RAMP_STEPS;
        }

        // Плавно уменьшаем период — двигатель ускоряется.
        const uint32_t period_us =
            START_PERIOD_US -
            (START_PERIOD_US - target_period_us) *
            ramp_position / RAMP_STEPS;

        make_step(period_us);
    }

    // После движения снимаем ток с обмоток.
    digitalWrite(ENABLE_PIN, HIGH);
}

void print_menu()
{
    Serial.println();
    Serial.println("Commands:");
    Serial.println("1 - speed 250 steps/s  (75 rpm)");
    Serial.println("2 - speed 400 steps/s (120 rpm)");
    Serial.println("3 - speed 600 steps/s (180 rpm)");
    Serial.println("4 - speed 800 steps/s (240 rpm)");
    Serial.println("f - 3 revolutions forward");
    Serial.println("b - 3 revolutions backward");
    Serial.println("h - hold shaft for 3 seconds");
    Serial.println();
}

void setup()
{
    Serial.begin(115200);

    pinMode(STEP_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);
    pinMode(ENABLE_PIN, OUTPUT);

    digitalWrite(STEP_PIN, LOW);
    digitalWrite(DIR_PIN, LOW);
    digitalWrite(ENABLE_PIN, HIGH);

    delay(1000);

    Serial.println("A4988 extended test");
    print_menu();
}

void loop()
{
    if (!Serial.available()) {
        return;
    }

    const char command = Serial.read();

    switch (command) {
        case '1':
            target_period_us = 4000;
            Serial.println("Speed: 250 steps/s");
            break;

        case '2':
            target_period_us = 2500;
            Serial.println("Speed: 400 steps/s");
            break;

        case '3':
            target_period_us = 1667;
            Serial.println("Speed: 600 steps/s");
            break;

        case '4':
            target_period_us = 1250;
            Serial.println("Speed: 800 steps/s");
            break;

        case 'f':
            Serial.println("Forward...");
            move_steps(
                STEPS_PER_REVOLUTION * TEST_REVOLUTIONS,
                true
            );
            Serial.println("Done");
            break;

        case 'b':
            Serial.println("Backward...");
            move_steps(
                STEPS_PER_REVOLUTION * TEST_REVOLUTIONS,
                false
            );
            Serial.println("Done");
            break;

        case 'h':
            Serial.println("Holding shaft for 3 seconds...");
            digitalWrite(ENABLE_PIN, LOW);
            delay(3000);
            digitalWrite(ENABLE_PIN, HIGH);
            Serial.println("Driver disabled");
            break;

        case '\n':
        case '\r':
            break;

        default:
            Serial.println("Unknown command");
            print_menu();
            break;
    }
}