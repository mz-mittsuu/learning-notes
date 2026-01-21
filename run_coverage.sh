#!/usr/bin/env bash
set -euo pipefail

cmake -S . -B build -DENABLE_COVERAGE=ON
cmake --build build -j
ctest --test-dir build --output-on-failure

gcovr -r . build \
  --exclude 'build/.*' \
  --html-details -o build/coverage.html \
  --print-summary \
  --fail-under-line 80
