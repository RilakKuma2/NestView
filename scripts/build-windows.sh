#!/usr/bin/env bash
# Run from an MSYS2 UCRT64 terminal. See docs/BUILDING.md for packages.
set -euo pipefail
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
cd "$project_dir"
cmake -S . -B build-windows -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON "$@"
cmake --build build-windows --parallel 4
ctest --test-dir build-windows --output-on-failure
python scripts/package-windows.py
