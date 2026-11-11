#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p .native-build

compiler_flags=(
    -std=c++17 -Wall -Wextra -Werror -g
    -fsanitize=address,undefined
    -Itest/native -Iinclude -Isrc
)
ui_sources=(
    src/ui.cpp
    src/ui_menu.cpp
    src/ui_edit.cpp
    src/ui_render.cpp
    src/ui_font.cpp
)
controller_sources=(
    src/settings.cpp
    src/config_store.cpp
    src/button.cpp
    src/feeding_controller.cpp
)

# Controller, persistence, buttons and UI with simulated equipment.
g++ "${compiler_flags[@]}" "${controller_sources[@]}" "${ui_sources[@]}" \
    test/native/fakes.cpp test/native/test_main.cpp -o .native-build/tests
ASAN_OPTIONS=detect_leaks=0 .native-build/tests

# The real sensor module with queued HX711 and ADC samples.
g++ "${compiler_flags[@]}" -DCYBER_TEST_REAL_SENSORS \
    "${controller_sources[@]}" "${ui_sources[@]}" src/sensors.cpp \
    test/native/fakes.cpp test/native/test_sensors.cpp -o .native-build/sensor-tests
ASAN_OPTIONS=detect_leaks=0 .native-build/sensor-tests

# Current and legacy configuration records, without UI or actuator dependencies.
g++ "${compiler_flags[@]}" src/settings.cpp src/config_store.cpp \
    test/native/test_config_store.cpp -o .native-build/config-tests
ASAN_OPTIONS=detect_leaks=0 .native-build/config-tests
