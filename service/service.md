* **今は**：sshで入って手で `./sensoralarm` を起動（たぶんssh切ると止まる／起動手順が人依存）
* **service化すると**：EVM起動時に **自動起動**、落ちたら **自動再起動**、ログも `journalctl` に残る

…という “製品っぽい運用” に寄せる作業です。AM62x(EVM)側でやります。

---

## まず整理（今の構成に合わせた前提）

* HostPCでビルドした `sensoralarm` のbin と `build/staticpage` を **scpでEVMの `~/workspace` 的な場所**に配置
* EVMに `ssh root@192.168.10.2` で入る
* `dashboard.html` はHostPCのブラウザで見る（＝EVMがHTTPでページ/APIを出してる、もしくは何らかの経路で配信されてる）

service化の対象は基本 **EVM上で動くアプリ（sensoralarm）** です。

---

## 1) EVM側に配置場所を固定する（例）

手でscpする場所が毎回ブレるとserviceが困るので、例えばこう固定します。

* 実行ファイル：`/opt/sensoralarm/sensoralarm`
* 静的ファイル：`/opt/sensoralarm/staticpage/`

```bash
# EVM側(root)
mkdir -p /opt/sensoralarm/staticpage
# HostPCからは例:
# scp sensoralarm root@192.168.10.2:/opt/sensoralarm/
# scp -r build/staticpage/* root@192.168.10.2:/opt/sensoralarm/staticpage/
chmod +x /opt/sensoralarm/sensoralarm
```

---

## 2) systemdのユニットファイルを作る

`/etc/systemd/system/sensoralarm.service` を作成します。

```ini
[Unit]
Description=SensorAlarm application
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
WorkingDirectory=/opt/sensoralarm
ExecStart=/opt/sensoralarm/sensoralarm
Restart=on-failure
RestartSec=1

# もし環境変数が要るならここに足す
# Environment=PORT=8080
# Environment=LOG_LEVEL=info

# 標準出力/標準エラーはjournalctlに集約される
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

> **ポイント**
>
> * `Restart=on-failure`：落ちたら自動復帰
> * `After/Wants=network-online.target`：ネットワークが上がってから起動（IP絡むなら大事）
> * `WorkingDirectory`：相対パスや設定ファイル参照があるなら必須級

---

## 3) 有効化して起動

```bash
systemctl daemon-reload
systemctl enable sensoralarm.service
systemctl start sensoralarm.service
systemctl status sensoralarm.service --no-pager
```

ログ確認：

```bash
journalctl -u sensoralarm.service -f
```

止める／再起動：

```bash
systemctl stop sensoralarm.service
systemctl restart sensoralarm.service
```

---

## 4) dashboard.html（staticpage）の扱いは2パターン

### A. sensoralarm自身がHTTPで配信している場合

* そのままOK。service化で常駐・自動起動になるだけ。

### B. 静的ページは別HTTPサーバで配信したい場合（おすすめ）

EVM上で `nginx` や `lighttpd` を動かして `/opt/sensoralarm/staticpage` を配信。

* HostPCのブラウザで `http://192.168.10.2/.../dashboard.html` みたいに見る形に整理しやすい
* フロント（静的）とバックエンド（sensoralarm）の責務が分かれて運用が楽

---

## 5) つまずきやすい所チェック

* `ExecStart` のパスが違う（scp先とズレる）
* 実行権限 `chmod +x` 忘れ
* アプリが “端末前提” で動いていて、daemon化すると落ちる（カレントディレクトリ依存、相対パス依存など）
* rootで動かす必要が本当にあるか（sysfs操作等が理由なら、将来的には専用ユーザ＋権限付与も検討）

---

もし `sensoralarm` が **「どのポートで待つ」「設定ファイルを読む」「staticpageの場所を参照する」** みたいな仕様なら、`ExecStart` に引数を付けた版のservice例もそのまま作れるよ。
今の起動コマンド（ssh後に手で打ってるやつ）をそのまま貼ってくれたら、それに完全一致する `sensoralarm.service` を書き起こす。
