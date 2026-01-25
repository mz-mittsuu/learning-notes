#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"
OUT_DIR="${2:-coverage}"

mkdir -p "$OUT_DIR/html"

# Azure DevOps は Cobertura 形式が相性良いので gcovr で cobertura xml を作る
# -r . : リポジトリルートを基準にする
# BUILD_DIR を object directory として指定
gcovr -r . "$BUILD_DIR" \
  --cobertura-pretty -o "$OUT_DIR/coverage.xml" \
  --html-details -o "$OUT_DIR/html/index.html" \
  --exclude '.*(CMakeFiles|_deps).*' \
  --print-summary
