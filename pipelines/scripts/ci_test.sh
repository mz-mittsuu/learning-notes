#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"

# gtestをJUnit形式で吐くとAzureが結果をUIで出せる
# 例: --gtest_output=xml:... を各テストに渡す
# CTestを使うなら、テスト定義側で環境変数や引数を設定するのが楽
cd "$BUILD_DIR"

# CTest経由の例（ctest登録されている想定）
# 失敗時も結果アップロードしたいので、上位のPublishTestResultsは succeededOrFailed()
ctest --output-on-failure

# もし ctest を使わずに直接実行するなら例：
# ./unit_tests --gtest_output=xml:../test-results-unit.xml
