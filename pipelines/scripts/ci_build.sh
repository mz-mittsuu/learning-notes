#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"
BUILD_TYPE="${2:-Debug}"

# coverage計測のためのフラグをCMakeに渡す
# - GCC/Clang系なら --coverage が簡単（内部で -fprofile-arcs -ftest-coverage 等）
mkdir -p "$BUILD_DIR"

cmake -S . -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DENABLE_TESTS=ON \
  -DENABLE_COVERAGE=ON \
  -DCMAKE_C_FLAGS="--coverage" \
  -DCMAKE_CXX_FLAGS="--coverage" \
  -DCMAKE_EXE_LINKER_FLAGS="--coverage"

cmake --build "$BUILD_DIR" -j "$(nproc)"
