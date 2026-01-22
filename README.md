# Design Policy and System Architecture of Sensor-based Alarm System

## 1. System Overview

温度センサ値に基づいて LED の状態を制御し、
ユーザに状態を可視化・通知する。

構成は以下の3層からなる。
それぞれの層は責務に基づいて分離されており、
疎結合で拡張性・保守性の高い構成を目指している。

* **Frontend**： センサ温度および LED 状態の表示・警告通知などの UI
* **Interface**： REST API による通信および外部インターフェース
* **Backend**： 判断ロジックおよびハードウェア制御
---

## 2. Frontend

### 2.1 Role of Frontend

* センサ温度（`sensorTemp`）の表示
* LED 状態（`ledStatus`）の表示
* 警告状態の通知（視覚的なアラート表示）

### 2.2 Reason for Static Page (HTML + CSS + JavaScript)

UI は、状態の表示のみを目的としており、
複雑な状態管理や画面遷移を必要としないため、

* ビルド環境が不要
* 軽量で構成が単純
* 組込み環境で扱いやすい

といった利点を持つ Static Page を採用した。

### 2.3 Why not React

React は高機能なフレームワークであるが、
今回の要件に対しては過剰であり、
依存関係や環境構築の複雑化を避けるため採用していない。

---

## 3. Interface

### 3.1 API Design

| Method | Endpoint     | Description             |
| ------ | ------------ | ----------------------- |
| POST   | `sensorTemp` | センサ温度を更新する      |
| GET    | `sensorTemp` | 現在のセンサ温度を取得する |
| GET    | `ledStatus`  | 現在のLED状態を取得する   |

### 3.2 Reason for Using REST API

* Frontend と Backend を疎結合にできる
* curl 等の外部ツールからも操作可能
* 将来的なクライアント追加が容易

### 3.3 Why Logic Is Hidden

API が扱うのは入力と出力のみとし、
内部の判断ロジックは外部に公開していない。

ロジックが実装詳細であり、
将来変更しても API 仕様に影響を与えないようにするためである。

* Input： `sensorTemp`
* Output： `sensorTemp`, `ledStatus`


---

## 4. Backend

Backend は責務に基づき、以下の2層に分割している。
* **Backend1 (SW Logic Layer)**
* **Backend2 (HW Control Layer)**

判断ロジックとハードウェア依存処理を分離し、
テスト性および移植性の向上を図っている。

### 4.1 Backend1 (SW Logic Layer)

Backend1 は、
ハードウェアに依存しない判断ロジックを担当する。

* `sensorTemp` の保持
* `sensorTemp` に基づく `ledStatus` の決定
* 判断ロジック（`ledStatusChange()`）

ハードウェアに依存せず、
ボード無しでも単体テストが可能である。

### 4.2 Backend2 (HW Control Layer)

Backend2 は、
実際のデバイス制御を担当する。

* `ledStatus` に基づく LED 制御
* AM62x EVM 固有の処理

LED 制御は Linux の sysfs を経由して実装しており、
実装を差し替えることで別ボードへの対応も可能である。

### 4.3 Definition of ledStatus

| ledStatus | State     | LED Behavior |
| --------- | --------- | ------------ |
| 0         | Safe      | OFF          |
| 1         | Caution   | Blink Slow   |
| 2         | Danger    | Blink Fast   |
| 3         | Immediate | ON           |

### 4.4 Public / Private Design

public 関数は API から呼ばれる窓口として最小限の責務に留め、
実際の判断・状態変更は private に集約している。

これにより、責務が明確になり、
実装変更の影響範囲を限定できる。

### 4.5 Web Server Library

Web サーバには cpp-httplib を使用している。

* ヘッダ1つで使用可能
* 依存関係が少ない
* 組込み環境に適している

といった理由から選定した。

---

## 5. Benefits of This Architecture

* UI / API / Backend の分離
* Backend 内での SW Logic / HW Control 分離

を行うことで、

* テスト性
* 移植性
* 保守性

の向上を実現している。

---