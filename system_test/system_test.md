以下は、いまの構成（HostPCビルド→scp配布→EVM上systemd常駐→ブラウザでdashboard.html、tempは別センサープログラムからPOST）に対する**システムテスト観点**の整理です。
※単体テスト済み前提なので、「結合〜実機上で破綻しやすい所」を厚めにしています。

---

## 1. 導入・配布・起動（運用フロー）

* **HostPCでビルドしたbinがEVMで実行できること**（依存ライブラリ/実行権限/配置パス）
* **scp転送後に起動できること**（転送失敗・古いbin残存・差し替え）
* **systemd serviceとして起動/停止/再起動できること**

  * `start/stop/restart/status` が想定どおり
  * **自動起動（boot後）**で立ち上がる
  * **異常終了時の再起動ポリシー**（Restart=always等）どおり復帰する
* **ログが追えること**（journalctlで必要情報が出る：起動、API受付、LED更新失敗など）

---

## 2. 初期状態（既定値）

* 起動直後の状態が仕様どおり

  * `temp` 初期値 **20**
  * `ledStatus` 初期値 **0（OFF）**
* dashboard.html を開いたときの表示が初期値と一致（GET /sensor/temp, GET /sensor/led）

---

## 3. REST APIの機能（機能結合）

### POST /sensor/{temp}

* POSTで受けたtempが **Alarm Controllerに反映**される
* 反映後、**LEDモード判定→LED Driver→sysfs反映**まで一連で動く
* 連続POST（センサが周期送信する想定）で取りこぼしや破綻がない

### GET /sensor/temp

* 最新のtempが返る（直前POSTの値が返る）
* POSTが来ていない間は初期値 or 前回値（仕様に合わせて）

### GET /sensor/led

* LED状態（0〜3）が返る（判定ロジックの結果と一致）

---

## 4. 閾値・境界値（温度→LED）

**境界が最重要**（システムでの取り違えが起きやすい）

* 39/40、59/60、79/80 を重点（期待：0/1/2/3に切り替わる）

  * 例：39→0、40→1、59→1、60→2、79→2、80→3
* “急激に上下する”入力でも、期待する状態に追従する（例：78→80→79→60→59）

---

## 5. LED実機挙動（sysfs反映＋目視/計測）

* `setOff()`：trigger=none, brightness=0 が反映され、**消灯**する
* `blinkSlow()`：trigger=timer, delay_on=200, delay_off=800 で **遅点滅**
* `blinkFast()`：trigger=timer, delay_on=100, delay_off=100 で **速点滅**
* `setOn()`：trigger=none, brightness=1 で **点灯**
* LED状態変更時に **以前のtriggerが残って変な点滅にならない**（遷移テスト：fast→on→slow→off など）

---

## 6. 異常系（入力・通信・ファイルI/O）

### API入力の異常

* tempが数値でない / 空 / 桁あふれ / 負値 / 極端に大きい値
* URI形式が違う、メソッド違い（GETにPOSTしてしまう等）
* 想定外でも **プロセスが落ちない**、HTTPステータスが妥当（400/404/405など）

### ネットワーク異常

* dashboard閲覧中にLAN断 → 復帰後に再取得できる
* API連打・同時アクセス（複数ブラウザ）でも応答が固まらない

### sysfs書き込み異常

* `/sys/class/leds/...` パス不正、権限不足、ファイルが存在しない場合
* 書き込み失敗時に **false返却・ログ出力**・サービス継続（落ちない）

---

## 7. 同時実行・整合性（地味に壊れがち）

* POST（センサ）とGET（UI）が同時に来た時に

  * 返すtemp/ledが破綻しない（中途半端な値・取り違え）
  * ledStatusと実LED挙動が乖離しない
* 短時間にPOST連打（例：1秒に10回）でも安定

---

## 8. 性能・応答性（体感品質）

* GET/POSTの応答が許容時間内（例：数百ms以内など、目標があればそれに合わせる）
* 常駐状態でCPU/メモリが過大にならない（数分〜数時間の長時間稼働）

---

## 9. UI観点（dashboard.html）

* 表示内容がAPIの値と一致（temp/led）
* 更新周期や再読込時に破綻しない（キャッシュ・表示遅延）
* APIエラー時の表示（通信失敗時に固まる/誤表示しない）

---

## 10. リカバリ・長時間（実運用）

* 数時間〜1日動かしてもLED制御が崩れない（タイマー点滅含む）
* systemd再起動、EVM再起動後に初期値（temp=20, led=0）から正常に立ち上がる

---

必要なら、この観点をそのまま **xlsxの「テストケース」列に落とし込む形**で、
「観点 → テスト項目ID → 前提 → 手順（API例含む） → 期待結果（HTTP/LED目視/ファイル内容）」まで書ける粒度に展開して出します。


---

```python
#!/usr/bin/env python3
# /system_test/system_test.py
"""
AM62x EVM + Device API + LED(sysfs) のシステムテスト（ホストPC側で実行想定）

狙い：
- REST API（POST/GET）が正しく動く
- temp -> ledStatus 判定が正しい（境界値）
- ledStatus -> sysfs（trigger/brightness/delay_*）が正しく反映される
- systemd サービスの起動/再起動で復帰できる

前提：
- EVMに Device API が systemd サービス化されている
- テストPCからEVMへ SSH できる（root想定）
- dashboard.html 自体のE2Eはここでは「APIの値が整合する」まで（UI自動操作は別枠）

使い方例：
  python3 -m pip install -U pytest requests paramiko
  pytest -q system_test.py

環境変数（必要に応じて上書き）：
  EVM_HOST=192.168.10.2
  EVM_USER=root
  EVM_PASS=  # 鍵運用なら空でOK
  API_BASE=http://192.168.10.2:8080   # main.cpp が待ち受けるURLに合わせて変更
  SYSTEMD_SERVICE=device-api.service  # 実際のユニット名に変更
  LED_SYSFS_DIR=/sys/class/leds/am62-sk:green:heartbeat
"""

import os
import time
import json
import threading
from dataclasses import dataclass
from typing import Optional, Tuple, Dict

import pytest
import requests

# SSH は paramiko を使用（scpは不要。sysfs確認とsystemctl操作に使う）
try:
    import paramiko
except ImportError:
    paramiko = None


# ----------------------------
# 設定
# ----------------------------
@dataclass(frozen=True)
class Config:
    evm_host: str
    evm_user: str
    evm_pass: str
    api_base: str
    systemd_service: str
    led_sysfs_dir: str
    request_timeout: float = 3.0
    settle_sec: float = 0.25  # POST後にLED反映を待つ時間（必要なら調整）


def load_config() -> Config:
    evm_host = os.getenv("EVM_HOST", "192.168.10.2")
    evm_user = os.getenv("EVM_USER", "root")
    evm_pass = os.getenv("EVM_PASS", "")
    api_base = os.getenv("API_BASE", f"http://{evm_host}:8080")
    systemd_service = os.getenv("SYSTEMD_SERVICE", "device-api.service")
    led_sysfs_dir = os.getenv("LED_SYSFS_DIR", "/sys/class/leds/am62-sk:green:heartbeat")

    return Config(
        evm_host=evm_host,
        evm_user=evm_user,
        evm_pass=evm_pass,
        api_base=api_base.rstrip("/"),
        systemd_service=systemd_service,
        led_sysfs_dir=led_sysfs_dir.rstrip("/"),
    )


CFG = load_config()


# ----------------------------
# REST API クライアント
# ----------------------------
class DeviceApiClient:
    def __init__(self, base_url: str, timeout: float):
        self.base = base_url
        self.timeout = timeout

    def post_temp(self, temp: int) -> requests.Response:
        # 仕様：POST /sensor/{temp}
        url = f"{self.base}/sensor/{temp}"
        return requests.post(url, timeout=self.timeout)

    def get_temp(self) -> Tuple[int, requests.Response]:
        # 仕様：GET /sensor/temp
        url = f"{self.base}/sensor/temp"
        r = requests.get(url, timeout=self.timeout)
        # 返り値形式は実装に依存するので柔軟に読む
        return parse_int_from_response(r), r

    def get_led(self) -> Tuple[int, requests.Response]:
        # 仕様：GET /sensor/led
        url = f"{self.base}/sensor/led"
        r = requests.get(url, timeout=self.timeout)
        return parse_int_from_response(r), r


def parse_int_from_response(r: requests.Response) -> int:
    """
    main.cppの実装が
      - plain text: "20"
      - json: {"temp": 20} / {"led": 2}
      - json: 20
    など何でも来ても、最初に見つかった int を返す。
    """
    ct = (r.headers.get("Content-Type") or "").lower()
    text = r.text.strip()

    # まず JSON を試す
    if "application/json" in ct or (text.startswith("{") or text.startswith("[")):
        try:
            obj = r.json()
            if isinstance(obj, int):
                return obj
            if isinstance(obj, dict):
                for k in ("temp", "led", "value", "status"):
                    if k in obj and isinstance(obj[k], int):
                        return obj[k]
                # dict内のintを総当り
                for v in obj.values():
                    if isinstance(v, int):
                        return v
            if isinstance(obj, list):
                for v in obj:
                    if isinstance(v, int):
                        return v
        except Exception:
            pass

    # plain text から数字抽出
    # 例: "temp=20" でも拾う
    import re

    m = re.search(r"-?\d+", text)
    if not m:
        raise AssertionError(f"Cannot parse int from response. status={r.status_code}, body={text[:200]}")
    return int(m.group(0))


# ----------------------------
# SSH ユーティリティ（sysfs / systemd 操作用）
# ----------------------------
class SshClient:
    def __init__(self, host: str, user: str, password: str):
        if paramiko is None:
            raise RuntimeError("paramiko が未インストールです: pip install paramiko")
        self.host = host
        self.user = user
        self.password = password
        self._cli: Optional[paramiko.SSHClient] = None

    def __enter__(self):
        cli = paramiko.SSHClient()
        cli.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        cli.connect(
            hostname=self.host,
            username=self.user,
            password=self.password if self.password else None,
            look_for_keys=True,
            allow_agent=True,
            timeout=5,
        )
        self._cli = cli
        return self

    def __exit__(self, exc_type, exc, tb):
        if self._cli:
            self._cli.close()

    def exec(self, cmd: str, check: bool = True) -> Tuple[int, str, str]:
        assert self._cli is not None
        stdin, stdout, stderr = self._cli.exec_command(cmd)
        out = stdout.read().decode(errors="ignore")
        err = stderr.read().decode(errors="ignore")
        code = stdout.channel.recv_exit_status()
        if check and code != 0:
            raise AssertionError(f"SSH cmd failed ({code}): {cmd}\n--- stdout ---\n{out}\n--- stderr ---\n{err}")
        return code, out, err

    def read_file(self, path: str) -> str:
        # busyboxでもOKな読み方
        _, out, _ = self.exec(f"cat {shell_quote(path)}", check=True)
        return out.strip()

    def systemctl(self, action: str, service: str, check: bool = True) -> Tuple[int, str, str]:
        return self.exec(f"systemctl {action} {shell_quote(service)}", check=check)


def shell_quote(s: str) -> str:
    return "'" + s.replace("'", "'\"'\"'") + "'"


# ----------------------------
# 期待値（温度->ledStatus, ledStatus->sysfs）
# ----------------------------
def expected_led_status_from_temp(temp: int) -> int:
    if temp <= 39:
        return 0
    if temp <= 59:
        return 1
    if temp <= 79:
        return 2
    return 3


def expected_sysfs_for_led_status(led: int) -> Dict[str, str]:
    """
    LedControl.cpp の実装（提示の値）を期待値として固定。
    実装が変わるならここを変える。
    """
    if led == 0:
        return {"trigger": "none", "brightness": "0"}
    if led == 1:
        return {"trigger": "timer", "delay_on": "200", "delay_off": "800"}
    if led == 2:
        return {"trigger": "timer", "delay_on": "100", "delay_off": "100"}
    if led == 3:
        return {"trigger": "none", "brightness": "1"}
    raise ValueError(f"invalid led={led}")


def read_led_sysfs_snapshot(ssh: SshClient, led_dir: str) -> Dict[str, str]:
    """
    sysfsの関係ファイルを読み取ってスナップショット化。
    trigger は現在の選択肢が [] で囲まれることがあるので正規化。
    """
    def norm_trigger(v: str) -> str:
        # 例: "none timer [heartbeat]" みたいな形式もあり得るので、[] 内を優先して返す
        if "[" in v and "]" in v:
            import re
            m = re.search(r"\[(.*?)\]", v)
            if m:
                return m.group(1)
        # 空白区切りなら先頭を返す…ではなく、完全一致が難しいのでそのまま
        return v.split()[0] if v else v

    snap: Dict[str, str] = {}
    # trigger は必須
    trig = ssh.read_file(f"{led_dir}/trigger")
    snap["trigger"] = norm_trigger(trig)

    # brightness / delay はトリガに応じて存在/意味が変わるので、読めるものだけ読む
    for name in ("brightness", "delay_on", "delay_off"):
        code, _, _ = ssh.exec(f"test -f {shell_quote(led_dir + '/' + name)}", check=False)
        if code == 0:
            snap[name] = ssh.read_file(f"{led_dir}/{name}")

    return snap


def assert_sysfs_matches_expected(snap: Dict[str, str], exp: Dict[str, str]):
    # trigger は必ず比較
    assert snap.get("trigger") == exp.get("trigger"), f"trigger mismatch: got={snap.get('trigger')} exp={exp.get('trigger')}"
    # brightness or delay_* は exp に書いたキーだけ比較
    for k, v in exp.items():
        if k == "trigger":
            continue
        assert snap.get(k) == v, f"{k} mismatch: got={snap.get(k)} exp={v}"


# ----------------------------
# pytest fixtures
# ----------------------------
@pytest.fixture(scope="session")
def api() -> DeviceApiClient:
    return DeviceApiClient(CFG.api_base, CFG.request_timeout)


@pytest.fixture(scope="session")
def ssh() -> SshClient:
    with SshClient(CFG.evm_host, CFG.evm_user, CFG.evm_pass) as c:
        yield c


@pytest.fixture(autouse=True)
def ensure_service_running(ssh: SshClient):
    """
    テスト前にサービスが動いている前提にする。
    もし unit 名や起動方法が違うならここを変える。
    """
    # is-active が非0でも落とさず、必要なら start
    code, out, _ = ssh.systemctl("is-active", CFG.systemd_service, check=False)
    if code != 0:
        ssh.systemctl("start", CFG.systemd_service, check=True)
        time.sleep(0.5)
    yield


# ----------------------------
# テスト：疎通・初期値
# ----------------------------
def test_api_health_basic(api: DeviceApiClient):
    # どちらかが通れば最低限OK、両方通るのが理想
    temp, r1 = api.get_temp()
    led, r2 = api.get_led()
    assert r1.status_code == 200
    assert r2.status_code == 200
    assert isinstance(temp, int)
    assert led in (0, 1, 2, 3)


def test_initial_values_after_restart(ssh: SshClient, api: DeviceApiClient):
    """
    仕様：temp初期値=20, led初期値=0
    ※実装上「再起動で初期化される」想定。違うならこのテストは調整。
    """
    ssh.systemctl("restart", CFG.systemd_service, check=True)
    time.sleep(0.8)

    temp, _ = api.get_temp()
    led, _ = api.get_led()

    assert temp == 20, f"temp initial expected 20, got {temp}"
    assert led == 0, f"led initial expected 0, got {led}"


# ----------------------------
# テスト：温度→LED判定（境界値）
# ----------------------------
@pytest.mark.parametrize(
    "temp, expected_led",
    [
        (0, 0),
        (20, 0),
        (39, 0),
        (40, 1),
        (59, 1),
        (60, 2),
        (79, 2),
        (80, 3),
        (120, 3),
    ],
)
def test_temp_to_led_status_via_api(api: DeviceApiClient, temp: int, expected_led: int):
    r = api.post_temp(temp)
    assert r.status_code in (200, 204), f"POST status unexpected: {r.status_code} body={r.text[:200]}"
    time.sleep(CFG.settle_sec)

    led, _ = api.get_led()
    assert led == expected_led, f"temp={temp}: expected led={expected_led}, got {led}"


# ----------------------------
# テスト：LED→sysfs反映（実機I/O）
# ----------------------------
@pytest.mark.parametrize("temp", [20, 40, 60, 80])
def test_led_sysfs_reflects_expected_mode(ssh: SshClient, api: DeviceApiClient, temp: int):
    exp_led = expected_led_status_from_temp(temp)

    r = api.post_temp(temp)
    assert r.status_code in (200, 204)
    time.sleep(CFG.settle_sec)

    # API上の値
    led, _ = api.get_led()
    assert led == exp_led

    # sysfsの値
    snap = read_led_sysfs_snapshot(ssh, CFG.led_sysfs_dir)
    exp_sysfs = expected_sysfs_for_led_status(exp_led)
    assert_sysfs_matches_expected(snap, exp_sysfs)


# ----------------------------
# テスト：状態遷移（fast→on→slow→off など）
# ----------------------------
def test_led_state_transitions_do_not_break(ssh: SshClient, api: DeviceApiClient):
    sequence = [80, 79, 60, 59, 40, 39, 80, 20]  # 3->2->2->1->1->0->3->0 を跨ぐ
    for temp in sequence:
        r = api.post_temp(temp)
        assert r.status_code in (200, 204)
        time.sleep(CFG.settle_sec)

        exp_led = expected_led_status_from_temp(temp)
        led, _ = api.get_led()
        assert led == exp_led

        snap = read_led_sysfs_snapshot(ssh, CFG.led_sysfs_dir)
        exp_sysfs = expected_sysfs_for_led_status(exp_led)
        assert_sysfs_matches_expected(snap, exp_sysfs)


# ----------------------------
# テスト：異常入力（落ちない・妥当なHTTP）
# ----------------------------
@pytest.mark.parametrize("bad_path", ["", "abc", "12.3", "-1", "999999999999999999999"])
def test_post_temp_bad_input_returns_4xx_or_handles(api: DeviceApiClient, bad_path: str):
    url = f"{CFG.api_base}/sensor/{bad_path}"
    r = requests.post(url, timeout=CFG.request_timeout)
    # 実装次第で 400/404/422 あたり。200で丸める実装もあり得るので許容するならここを調整。
    assert r.status_code in (200, 204, 400, 404, 405, 409, 422), f"unexpected status={r.status_code}, body={r.text[:200]}"

    # 少なくとも GET が死んでないこと
    temp, _ = api.get_temp()
    led, _ = api.get_led()
    assert isinstance(temp, int)
    assert led in (0, 1, 2, 3)


# ----------------------------
# テスト：同時アクセス（軽い並行）
# ----------------------------
def test_concurrent_get_while_posting(api: DeviceApiClient):
    stop = threading.Event()
    errors = []

    def getter():
        while not stop.is_set():
            try:
                _t, r1 = api.get_temp()
                _l, r2 = api.get_led()
                if r1.status_code != 200 or r2.status_code != 200:
                    errors.append(("status", r1.status_code, r2.status_code))
            except Exception as e:
                errors.append(("exc", repr(e)))

    th = threading.Thread(target=getter, daemon=True)
    th.start()

    try:
        for temp in [20, 40, 60, 80, 39, 59, 79]:
            r = api.post_temp(temp)
            assert r.status_code in (200, 204)
            time.sleep(0.05)
    finally:
        stop.set()
        th.join(timeout=2.0)

    assert not errors, f"concurrent errors: {errors[:5]}"


# ----------------------------
# テスト：systemd再起動・復帰
# ----------------------------
def test_service_restart_recovery(ssh: SshClient, api: DeviceApiClient):
    # 変更しておいて…
    r = api.post_temp(80)
    assert r.status_code in (200, 204)
    time.sleep(CFG.settle_sec)
    led, _ = api.get_led()
    assert led == 3

    # restart で初期値に戻る仕様なら戻る、保持仕様なら保持。
    ssh.systemctl("restart", CFG.systemd_service, check=True)
    time.sleep(0.8)

    temp, _ = api.get_temp()
    led2, _ = api.get_led()

    # ここは「どっちが仕様か」で調整ポイント
    # 現状のあなたの説明（初期値がある）から、restartで初期化される想定で書いておく
    assert temp == 20, f"expected temp reset to 20 after restart, got {temp}"
    assert led2 == 0, f"expected led reset to 0 after restart, got {led2}"


# ----------------------------
# 補助：テスト実行時に設定を表示（デバッグ用）
# ----------------------------
def test__print_config():
    # -q だと出ない。必要なら pytest -s で確認。
    print("CFG:", json.dumps(CFG.__dict__, ensure_ascii=False, indent=2))
```

この `system_test.py` は、**main.cpp（APIの返し方やポート）とsystemdユニット名**さえ分かれば「ほぼ完成」にしてあります。
あなたの実装に合わせて、まずはここだけ直すのが最短です：

* `API_BASE`（ポート・パス）
* `SYSTEMD_SERVICE`（ユニット名）
* もしGETの返却形式が特殊なら `parse_int_from_response()` を微調整
* LEDのsysfsパスが違う場合は `LED_SYSFS_DIR`

必要なら、あなたの `main.cpp` の **エンドポイント実装の返却形式**（text/json）だけ貼ってくれれば、`parse_int_from_response()` をピン止めして「完全に一致」させます。
