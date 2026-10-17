#!/bin/bash
set -euo pipefail
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_DIR"
PREFIX="$(brew --prefix)"
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$PREFIX;$PREFIX/opt/libarchive" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
  -DPHOTO_UPDATE_REPOSITORY="${NESTVIEW_UPDATE_REPOSITORY:-RilakKuma2/NestView}" \
  -DBUILD_TESTS=ON
cmake --build build --parallel 6
ctest --test-dir build --output-on-failure
