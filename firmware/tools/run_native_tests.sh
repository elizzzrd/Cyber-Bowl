#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p .native-build
g++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -Itest/native -Iinclude src/settings.cpp src/config_store.cpp src/button.cpp \
    src/feeding_controller.cpp src/ui.cpp src/ui_font.cpp \
    test/native/fakes.cpp test/native/test_main.cpp -o .native-build/tests
ASAN_OPTIONS=detect_leaks=0 .native-build/tests
g++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -DCYBER_TEST_REAL_SENSORS -Itest/native -Iinclude \
    src/settings.cpp src/config_store.cpp src/button.cpp src/sensors.cpp \
    src/feeding_controller.cpp src/ui.cpp src/ui_font.cpp \
    test/native/fakes.cpp test/native/test_sensors.cpp -o .native-build/sensor-tests
ASAN_OPTIONS=detect_leaks=0 .native-build/sensor-tests
