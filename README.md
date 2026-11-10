# Digimin

距離センサーで弾く、オープンな電子楽器。片手で音程、もう片手で音量を、2 つの ToF 距離センサーで読み取り、Raspberry Pi Pico 2 W で音を作って DAC から出す。設定はスマホのブラウザから変えられる。

An open electronic instrument played with two time-of-flight distance sensors — one hand for pitch, the other for volume. A Raspberry Pi Pico 2 W synthesizes the sound and sends it to an I2S DAC (PCM5102A); settings are edited from a phone browser over the board's own Wi-Fi.

## 特徴

- 音程: VL53L1X（約21回/秒）、音量: VL53L4CD（約100回/秒）。どちらのセンサーをどちらに付けても起動時に自動判別
- 波形（サイン・三角・ノコギリ）の混ぜ合わせ、ハーモニー、ビブラート、オーバードライブ、レスリー、コーラス、ディレイ、リバーブ
- 組み込みプリセット 10 種類、名前付きプリセット 7 つ、起動時の設定
- 本体が Wi-Fi アクセスポイント「Digimin-XXXX」になり、つなぐと操作画面が開く（日本語 / English）
- 音は PCM5102A（I2S DAC、96kHz、16bit）から出力。Wi-Fi はスイッチでオン / オフ

## 必要なもの

| 部品 | 数 |
|---|---|
| Raspberry Pi Pico 2 W | 1 |
| VL53L1X モジュール（音程） | 1 |
| VL53L4CD モジュール（音量） | 1 |
| PCM5102A DAC モジュール（紫色の基板、3.5mm ジャック付き） | 1 |
| トグルスイッチ ON-OFF-ON（センターオフ、3 ポジション） | 1 |

## ピン表

| 物理ピン | GP | つなぐ先 |
|---|---|---|
| 1 | GP0 | DAC LCK |
| 2 | GP1 | DAC BCK |
| 3 | GND | DAC SCK |
| 4 | GP2 | DAC DIN |
| 7 | GP5 | スイッチの片側（Wi-Fi オフ） |
| 8 | GND | スイッチの中央 |
| 12 | GP9 | スイッチの反対側（Wi-Fi パスワードなし） |
| 15 | GP11 | 音程センサー XSHUT |
| 16 | GP12 | 音程センサー SDA（I2C0） |
| 17 | GP13 | 音程センサー SCL（I2C0） |
| 18 | GND | 音程センサー GND |
| 23 | GND | 音量センサー GND |
| 24 | GP18 | 音量センサー SDA（I2C1） |
| 25 | GP19 | 音量センサー SCL（I2C1） |
| 26 | GP20 | 音量センサー XSHUT |
| 36 | 3V3(OUT) | センサー VIN ×2 |
| 38 | GND | DAC GND |
| 39 | VSYS | DAC VIN |

- センサーの GPIO1 は未接続。
- DAC モジュール裏のジャンパーは、H1（FLT）、H2（DEMP）、H4（FMT）を L、H3（XSMT）を H にする。H3 が L だとミュートされて音が出ない。
- 音は DAC モジュールの 3.5mm ジャック（または LROUT / ROUT / AGND の端子）から出す。左右は同じ音。

詳しい仕様は [theremin_pico2w/SPEC.md](theremin_pico2w/SPEC.md) を参照。

## ファームの書き込み

[arduino-pico](https://github.com/earlephilhower/arduino-pico)（Raspberry Pi Pico/RP2040/RP2350、6.2.0 で確認）と、Pololu の [VL53L1X](https://github.com/pololu/vl53l1x-arduino) ライブラリを使う。VL53L4CD のドライバはスケッチに含まれている。

```bash
arduino-cli core install rp2040:rp2040 --additional-urls https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
arduino-cli lib install VL53L1X
arduino-cli compile --fqbn rp2040:rp2040:rpipico2w theremin_pico2w
arduino-cli upload --fqbn rp2040:rp2040:rpipico2w -p <ポート> theremin_pico2w
```

Arduino IDE なら、ボードに「Raspberry Pi Pico 2W」を選んで `theremin_pico2w/theremin_pico2w.ino` を開いて書き込む。

## 使い方

1. USB（モバイルバッテリーでも可）で電源を入れる
2. スイッチを中央にして Wi-Fi をオンにし、スマホで Wi-Fi「Digimin-XXXX」につなぐと操作画面が開く（開かなければ http://192.168.42.1/ ）
3. 音程センサーに手を近づけると高く、遠ざけると低くなる。音量センサーに手を近づけると大きくなる
4. スイッチで Wi-Fi を切り替える。中央でオン（パスワードを設定していればパスワードあり）、片側でオフ、反対側でパスワードなしでオン。パスワードを忘れたときは、パスワードなしの位置にすればつなげる（保存したパスワードは消えない）

## 旧版（PWM 版）

DAC を使わず、Pico の PWM 出力（約96kHz）と RC フィルタ 2 段で音を出す最初の版。Wi-Fi はつまみ（10kΩ B カーブ）で切り替える。センサーのピンも DAC 版と違う。部品と配線は [SPEC.md](theremin_pico2w/SPEC.md) の PWM 版の節を参照。

- ファームは同じファイル。`-DDIGIMIN_DAC=0` を付けてビルドする（Arduino IDE なら、スケッチの `#define DIGIMIN_DAC 1` を `0` に書き換える）

```bash
arduino-cli compile --fqbn rp2040:rp2040:rpipico2w --build-property "compiler.cpp.extra_flags=-DDIGIMIN_DAC=0" theremin_pico2w
```

- Wi-Fi の電波を出している間は、3.3V の揺れが PWM の振幅に出て、小さなノイズが乗る。演奏するときはつまみを絞り切って Wi-Fi を切る
- RC フィルタ・ジャック・つまみ用の基板（KiCad、JLCPCB 向けの製造データ付き）は [theremin_pico2w/kicad/](theremin_pico2w/kicad/)

## ライセンス

MIT License（[LICENSE](LICENSE)）
