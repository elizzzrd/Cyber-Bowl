#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$(mktemp -d)"
trap 'rm -rf "$build_dir"' EXIT
g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$project_dir/include" -I"$project_dir/test/native/mocks" \
  "$project_dir/test/native/test_camera.cpp" "$project_dir/src/form_codec.cpp" \
  "$project_dir/src/config_validation.cpp" "$project_dir/src/config.cpp" -o "$build_dir/test_camera"
ASAN_OPTIONS=detect_leaks=0 "$build_dir/test_camera"
