#!/usr/bin/env bash
set -euo pipefail

# CI agent に必要なツールを入れる
# - cmake/ninja/build-essential: ビルド
# - python3/pip: gcovrを入れるため
# - gcovr: カバレッジレポート生成（pipでもaptでもOK）
sudo apt-get update
sudo apt-get install -y \
  cmake ninja-build build-essential pkg-config \
  python3 python3-pip \
  lcov

python3 -m pip install --upgrade pip
python3 -m pip install gcovr
