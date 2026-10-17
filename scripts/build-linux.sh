#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
cd "$project_dir"
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON "$@"
cmake --build build-linux --parallel 4
ctest --test-dir build-linux --output-on-failure
desktop-file-validate dist/linux/org.nestview.viewer.desktop
cpack --config build-linux/CPackConfig.cmake -B release/linux
