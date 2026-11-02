# Digimin

距離センサーで弾く、オープンな電子楽器。片手で音程、もう片手で音量を、2 つの ToF 距離センサーで読み取り、Raspberry Pi Pico 2 W で音を作ってジャックから出す。設定はスマホのブラウザから変えられる。

An open electronic instrument played with two time-of-flight distance sensors — one hand for pitch, the other for volume. A Raspberry Pi Pico 2 W synthesizes the sound and drives a line-level jack; settings are edited from a phone browser over the board's own Wi-Fi.

## 特徴

- 音程: VL53L1X（約21回/秒）、音量: VL53L4CD（約100回/秒）。どちらのセンサーをどちらに付けても起動時に自動判別
- 波形（サイン・三角・ノコギリ）の混ぜ合わせ、ハーモニー、ビブラート、オーバードライブ、レスリー、コーラス、ディレイ、リバーブ
- 組み込みプリセット 10 種類、名前付きプリセット 7 つ、起動時の設定
- 本体が Wi-Fi アクセスポイント「Digimin-XXXX」になり、つなぐと操作画面が開く（日本語 / English）
- 音は PWM（約96kHz）＋ RC フィルタ 2 段で出力。外付けの DAC は不要

## 必要なもの

| 部品 | 数 |
|---|---|
| Raspberry Pi Pico 2 W | 1 |
| VL53L1X モジュール（音程） | 1 |
| VL53L4CD モジュール（音量） | 1 |
| 可変抵抗 10kΩ B カーブ | 1 |
| 抵抗 1kΩ ×2、3.3kΩ ×1 / コンデンサ 10nF ×2、10µF ×1 | |
| 3.5mm ステレオジャック | 1 |

## ピン表

| 物理ピン | GP | つなぐ先 |
|---|---|---|
| 21 | GP16 | 音量センサー XSHUT |
| 22 | GP17 | 音程センサー XSHUT |
| 24 | GP18 | 音量センサー SDA（I2C1） |
| 25 | GP19 | 音量センサー SCL（I2C1） |
| 26 | GP20 | 音程センサー SDA（I2C0） |
| 27 | GP21 | 音程センサー SCL（I2C0） |
| 29 | GP22 | 音の PWM 出力 → RC フィルタ → ジャック |
| 34 | GP28 | つまみのワイパー |
| 36 | 3V3(OUT) | センサー VIN ×2、つまみの片端 |
| 33 | AGND | つまみのもう片端 |
| 3, 8, 13, 18, 23, 28, 38 | GND | センサー GND ×2、RC フィルタ・ジャックの GND |

センサーの GPIO1 は未接続。

RC フィルタの定数・初期値など、詳しい仕様は [theremin_pico2w/SPEC.md](theremin_pico2w/SPEC.md) を参照。

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
2. 本体のつまみを上げて Wi-Fi をオンにし、スマホで Wi-Fi「Digimin-XXXX」につなぐと操作画面が開く（開かなければ http://192.168.42.1/ ）
3. 音程センサーに手を近づけると高く、遠ざけると低くなる。音量センサーに手を近づけると大きくなる
4. 本体のつまみで Wi-Fi を切り替える。上げるとオン、右いっぱいでパスワードなしでオン、絞り切るとオフ。Wi-Fi を出している間は小さなノイズが乗るので、音色を決めたら絞り切って演奏する

## ライセンス

MIT License（[LICENSE](LICENSE)）
