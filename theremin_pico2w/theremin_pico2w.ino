/*
  Digimin theremin (Pico 2 W 版) — Raspberry Pi Pico 2 W + VL53L4CD（または VL53L1X）x2、音声は PWM 出力、ブラウザで操作

  演奏の仕様は Pico 2 版（theremin_pico_pwm/）と同じ。設定値をブラウザから変えられるようにしたもの。
  Pico 2 W が Wi-Fi のアクセスポイントになる。Wi-Fi の名前は「Digimin-XXXX」（XXXX はチップ固有 ID の
  末尾 4 桁で、ボードごとに違う。複数台あっても名前がかぶらない）。スマホをその Wi-Fi につなぐと
  操作画面が開く（開かなければ http://192.168.42.1/）。パスワードは初期状態ではなし。画面の「Wi-Fi」で
  設定できる（保存すると再起動）。名前は画面の見出しとシリアル出力に出る。

  ブラウザで操作できるもの
    音色: サイン波 / 三角波 / ノコギリ波の比率（合計は常に 100%。全部 0 で無音）
    音程: 最低音、オクターブ数、グライド
    距離: 音程の手の範囲、最低音のまま鳴らす幅、音量の手の範囲（＋今の距離の表示）
    音量: 音量カーブの幅、払ったときの余韻、払ったと判定する基準
    エフェクト: ハーモニー（量・音程）、ビブラート（深さ・速さ）、オーバードライブ（歪み・音色）、レスリー（量・スロー/ファスト）、
                コーラス（量・速さ・深さ）、ディレイ（量・時間・繰り返し）、リバーブ（量・広さ・響きの明るさ）
    組み込みプリセット: 10 種類（クラシック・テルミン、SF映画、ボーカル風、シンセリード、ジャズオルガン、
                ロックオルガン、ギター風リード、ツインリード、ストリングス風、アンビエント）。音色とエフェクトだけを読み込む
    起動時の設定: 今の設定を保存しておくと、電源を入れたときにそれで起動する（保存先は 0 番の枠）
    プリセット: 7 個まで名前をつけて保存（本体のフラッシュに残る。保存先は 1〜7 番の枠）
    画面の言語: 日本語 / 英語（端末の言語で自動選択、右上のボタンで切り替え。選んだ言語はブラウザに記憶）
  本体のつまみ（GP28）で Wi-Fi を切り替える。絞り切ると Wi-Fi オフ、上げるとオン（パスワードを設定して
  いればパスワードあり）、右いっぱいにするとパスワードなしでオン（保存したパスワードは消えない）。
  Wi-Fi の電波を出している間は、約 6 回/秒の小さな「ブツッ」が音に乗る（電源の揺れ。実測）ので、
  演奏するときはつまみを絞り切って Wi-Fi を切る。

  Core0: Wi-Fi / ウェブサーバー / LED。
  Core1: PWM の 1 周期ごとの割り込み（約 96kHz、最優先）で波形を計算 (GP22)。メインループで
         ToF センサー 2 台と可変抵抗を処理する。
         音の割り込みを Core0 に置くと、Wi-Fi の処理が割り込みを止める区間で最大 70µs（7 周期）
         遅れ、ブラウザの更新のたびに「チチッ」と鳴った（実測）。Core1 には Wi-Fi の処理が来ないので
         こちらに置く。割り込みの中で使う計算はすべて RAM 上（exp2f は使わない）。
  ※ プリセットの保存はフラッシュへの書き込みで、その間（数十 ms）は音が止まる。

  音の流れ: 波形（3 波形のミックス。ハーモニーは同じ音色の 2 声目を決まった音程差で重ねる。
  ビブラートは両方の音程に）→ 音量 → オーバードライブ → レスリー → コーラス
  → ディレイ → リバーブ（Freeverb 型）→ 出力。
  オーバードライブは音量のあとなので、本物のオルガン（エクスプレッションペダル → レスリーのアンプ）と
  同じく、音量を下げると歪みも減る。
  レスリー: 800Hz で高音（ホーン）と低音（ドラム）に分け、ホーンは短い遅延を揺らして音程を揺らし
  （ドップラー効果）音量も揺らす。ドラムは音量だけ揺らす。スロー / ファスト を切り替えると、回転の
  慣性でホーンは約 0.8 秒、ドラムは約 3.5 秒かけて速度が変わる。
  ディレイとリバーブは音を切っても余韻が残る。量を 0 にするとオフ（処理は回し続けるので、オンにした
  瞬間に古い音が鳴ることはない）。バッファはディレイ最長 1 秒（int16, 約 190KB）、リバーブ 約 110KB。

  配線（物理ピン: GP16 = 21, GP17 = 22, GP18 = 24, GP19 = 25, GP20 = 26, GP21 = 27, GP22 = 29, GP28 = 34）
    GP22 -[1kΩ]--+--[1kΩ]--+--[10µF]--+--> AUX (Tip/Ring 両方)
                 |         |          |
              [10nF]    [10nF]     [3.3kΩ]
                 |         |          |
                GND       GND        GND

    距離センサー x2（VL53L1X / VL53L4CD のどちらでも可、起動時にチップ ID で自動判別）
                    VIN -> 3V3(OUT),  GND -> GND,  GPIO1 は未接続
                    音程側: SDA -> GP20, SCL -> GP21（I2C0）,       XSHUT -> GP17
                    音量側: SDA -> GP18, SCL -> GP19（I2C1 で専用）, XSHUT -> GP16
                    音量側を GP20 / GP21 に並列にしても動く。音量側は起動時に I2C1 → I2C0 の順に探す

    可変抵抗（サイン波, 10kΩ B カーブ）  両端 -> 3V3(OUT) と AGND,  ワイパー -> GP28
    基板上の LED（Wi-Fi チップ経由）: 音が出る状態（どれかの比率が 0 でない）のとき点灯
    ※ Pico 2 W は GP23 / GP24 / GP25 / GP29 を Wi-Fi チップに使うので、配線には使わない
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include <Wire.h>
#include <VL53L1X.h>

struct Tof;  // Arduino の自動プロトタイプ生成用の前方宣言
struct Params;

// ---- Wi-Fi ----
const char *WIFI_SSID_BASE = "Digimin";  // 実際の名前は "Digimin-XXXX"（XXXX はチップ固有 ID の末尾 4 桁）
String wifiSsid;
String wifiPass;            // 空 = パスワードなし。フラッシュに保存（WifiCfg）
enum WifiState : uint8_t { WIFI_ST_OFF, WIFI_ST_PASS, WIFI_ST_OPEN };  // つまみの位置で決まる
WifiState wifiState = WIFI_ST_OFF;  // いま動いている状態（PASS でもパスワード未設定ならパスワードなし）
uint32_t rebootAtMs = 0;    // 0 以外: この時刻に再起動する（パスワード変更後）

// ---- ピン ----
const uint8_t PIN_AUDIO = 22;    // PWM はどのピンでも出せる
const uint8_t PIN_TOF_SDA = 20;  // I2C0（Wire）の SDA。音程センサー（音量センサーを並べてもよい）
const uint8_t PIN_TOF_SCL = 21;  // I2C0（Wire）の SCL
TwoWire &tofBus = Wire;
const uint8_t PIN_VOL_SDA = 18;  // I2C1（Wire1）の SDA。音量センサー専用（つながっていなければ I2C0 を探す）
const uint8_t PIN_VOL_SCL = 19;  // I2C1（Wire1）の SCL
TwoWire &volBus = Wire1;         // GP18 / GP19 は I2C1 なので Wire ではなく Wire1
const uint8_t PIN_XSHUT_PITCH = 17;  // 音程と音量はここを入れ替えるだけで役割が入れ替わる
const uint8_t PIN_XSHUT_VOL = 16;
const uint8_t PIN_KNOB = 28;  // ADC2
const uint8_t PIN_SMPS_MODE = 65;  // Wi-Fi チップの GPIO1 = 電源回路のモード切り替え（HIGH で PWM モード）

// ---- ブラウザから変えられる設定 ----
struct Params {
  float mix[3];           // 音色の比率（サイン, 三角, ノコギリ）0〜1。合計で割って 100% にそろえる
  float lowHz;            // 最低音（MAX_CM の位置）
  float octaves;          // 音域（オクターブ数）
  float glideMs;          // グライド（1 次フィルタ 2 段の合計の時定数）
  float minCm, maxCm;     // 音程の手の範囲（minCm で最高音、maxCm で最低音）
  float holdCm;           // maxCm からこの幅だけは最低音のまま鳴らす。それより遠い = 無音
  float volMinCm, volMaxCm;  // 音量の手の範囲（volMinCm より近いと最大、volMaxCm で無音）
  float volDbRange;       // 音量カーブの幅（dB）
  float releaseAbruptMs;  // 音量側の手を一気に払ったときの余韻
  float releaseCutMs;     // それ以外で無音にするとき
  float volAbruptDb;      // 範囲外になる直前の音量がこれより大きければ「一気に払った」
  // ---- エフェクト（ここから下は v2 で追加）----
  float vibCents;         // ビブラートの深さ（セント）。0 = オフ
  float vibRateHz;        // ビブラートの速さ
  float delayMix;         // ディレイの量 0〜1。0 = オフ
  float delayMs;          // ディレイの時間
  float delayFeedback;    // ディレイの繰り返し 0〜0.9
  float reverbMix;        // リバーブの量 0〜1。0 = オフ
  float reverbSize;       // リバーブの広さ 0〜1
  float reverbDamp;       // リバーブの高域の吸収 0〜1（大きいほど暗い響き）
  // ---- v3 で追加 ----
  float chorusMix;        // コーラスの量 0〜1。0 = オフ
  float chorusRateHz;     // コーラスの揺れの速さ
  float chorusDepthMs;    // コーラスの揺れの深さ（遅れ時間の振れ幅）
  // ---- v4 で追加 ----
  float odDrive;          // オーバードライブの歪み 0〜1。0 = オフ
  float odTone;           // オーバードライブの音色 0〜1（大きいほど明るい）
  float leslieMix;        // レスリーの量 0〜1。0 = オフ
  float leslieFast;       // レスリーの速度 0 = スロー（コラール）、1 = ファスト（トレモロ）
  // ---- v6 で追加 ----
  float harmonyMix;       // ハーモニーの量 0〜1。0 = オフ。1 で主音と同じ大きさ
  float harmonySemis;     // ハーモニーの音程差（半音）。-12〜+12
  // ---- v7 で追加 ----
  float pitchBeta;        // 音程の手の追従の速さ（One Euro フィルタの beta）。大きいほど動きに速く追従
  float volBeta;          // 音量の手の追従の速さ
};


const Params DEFAULT_PARAMS = {
  {0.0f, 1.0f, 0.0f}, 261.63f, 3.0f, 30.0f,  // 三角波 100%
  5.0f, 60.0f, 5.0f, 10.0f, 50.0f, 40.0f,
  5.0f, 5.0f, -30.0f,
  0.0f, 5.5f,           // ビブラート: オフ、5.5Hz
  0.0f, 350.0f, 0.35f,  // ディレイ: オフ、350ms、繰り返し 35%
  0.0f, 0.7f, 0.4f,     // リバーブ: オフ、広さ 70%、吸収 40%
  0.0f, 0.8f, 3.0f,     // コーラス: オフ、0.8Hz、深さ 3ms
  0.0f, 0.6f,           // オーバードライブ: オフ、音色 60%
  0.0f, 0.0f,           // レスリー: オフ、スロー
  0.0f, 4.0f,           // ハーモニー: オフ、長 3 度上
  0.5f, 0.8f,           // 追従の速さ: 音程 0.5、音量 0.8
};

// Core0（ウェブ）で書き、Core1 と割り込みで読む。float 1 個ずつの読み書きはアトミック
Params prm = DEFAULT_PARAMS;

// ---- 固定のパラメータ ----
const float JUMP_CM = 10.0f;   // 1回の測定でこれ以上動いたら飛び値の疑い
const float AMP_MS = 32.0f;    // 音量センサーの追従
const float SAW_GAIN = 1.0f;
const uint16_t PWM_TOP = 1562;  // 周期 1563 カウント → 150MHz / 1563 ≈ 95969Hz
const float OUT_LEVEL = 0.9f;   // フルスケールに対する最大振幅

const int TABLE_BITS = 10;
const int TABLE_SIZE = 1 << TABLE_BITS;
float sineTable[TABLE_SIZE + 1];  // +1 は線形補間用のガード

// Core1 → 割り込みの受け渡し
volatile float targetHz = 261.63f;   // 表示用
volatile float targetOct = 8.0313f;  // log2(目標周波数)
volatile float targetAmp = 0.0f;
volatile float targetMix[3] = {0.0f, 0.0f, 0.0f};
volatile bool ledWanted = false;     // LED は Wi-Fi チップ経由なので Core0 のループで点ける

// 設定から計算する係数（applyParams で更新）
float sampleRate;
volatile float glideK, ampK, releaseAbruptK, releaseCutK, hzToInc;
volatile float releaseK = 0.0f;  // いま使う無音への絞り方（Core1 で切り替え、割り込みが使う）

// エフェクト（applyParams で更新し、割り込みが使う）
volatile float vibDepthOct = 0.0f;   // ビブラートの深さ（オクターブ）
volatile uint32_t vibInc8 = 0;       // ビブラートの位相増分（8 サンプルごと）
volatile uint32_t delaySamples = 1;
volatile float delayMixV = 0.0f, delayFbV = 0.0f;
volatile float revMixV = 0.0f, revFbV = 0.84f, revDampV = 0.16f;
volatile float chMixV = 0.0f, chDepthSamp = 0.0f;
volatile uint32_t chInc = 0;         // コーラスの揺れの位相増分（1 サンプルごと）

// コーラス: 中心 CHORUS_BASE_MS の遅れを chorusDepthMs だけ揺らす。最長 CHORUS_MAX_MS ぶん
const float CHORUS_BASE_MS = 12.0f;
const int CH_MAX = 2400;             // 96kHz で 25ms
float chBuf[CH_MAX];
volatile float chBaseSamp = 1000.0f;

// オーバードライブ
volatile float odGain = 1.0f, odNorm = 1.0f, odToneK = 0.5f;
volatile bool odOn = false;

// レスリー
const float LESLIE_XOVER_HZ = 800.0f;            // ホーンとドラムの境目
const float HORN_SLOW_HZ = 0.8f, HORN_FAST_HZ = 6.8f;
const float DRUM_SLOW_HZ = 0.67f, DRUM_FAST_HZ = 5.9f;
const float HORN_ACCEL_S = 0.8f, DRUM_ACCEL_S = 3.5f;  // 速度が切り替わるまでの時定数（回転の慣性）
const float HORN_DOPPLER_MS = 0.4f;              // ホーンの半径ぶんの遅れの揺れ（ドップラー効果）
const float HORN_AM = 0.5f, DRUM_AM = 0.3f;      // 音量の揺れの深さ
const int LS_MAX = 256;                          // ホーンの遅延バッファ（96kHz で約 2.6ms）
float lsBuf[LS_MAX];
volatile float lsMixV = 0.0f;
volatile float harmRatio = 1.0f, harmMixV = 0.0f;  // ハーモニーの周波数比と量
volatile float hornTargetInc = 0.0f, drumTargetInc = 0.0f;  // 位相増分（1 サンプルあたり、2^32 単位）
float lsXoverK, hornAccelK, drumAccelK, lsBaseSamp, lsDopplerSamp;

const uint32_t DELAY_MAX = 96000;    // ディレイの最長（約 1 秒）
int16_t delayBuf[DELAY_MAX];
const float DELAY_TONE_K = 0.25f;    // 繰り返すたびに高域を少し落とす（テープエコー風）

// リバーブ（Freeverb 型: コムフィルター 8 本 + オールパス 4 本）。長さは 44.1kHz 用の値を実際の
// サンプルレートに合わせて伸ばす
const int REV_NCOMB = 8, REV_NAP = 4;
const int REV_COMB_44K[REV_NCOMB] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
const int REV_AP_44K[REV_NAP] = {556, 441, 341, 225};
const int REV_BUF_MAX = 28000;       // 96kHz で 8 本 + 4 本ぶん（約 27400）
float revBuf[REV_BUF_MAX];
int revCombLen[REV_NCOMB], revCombOfs[REV_NCOMB];
int revApLen[REV_NAP], revApOfs[REV_NAP];

// 表示用の測定値
volatile float liveCm[2] = {1e6f, 1e6f};  // 音程, 音量
volatile float knobPos = 0.0f;
// 診断用: 音の割り込みの間隔（本来は約 10.4µs）。最大値と、20µs を超えた回数
volatile uint32_t isrGapMaxUs = 0, isrGapLate = 0;
volatile uint32_t isrDurMaxUs = 0;  // 診断用: 割り込み 1 回の処理時間の最大（µs）

// ================= Core0: 音源 =================

// 2^x の高速近似（整数部は指数ビットに直接入れ、小数部は 4 次多項式。相対誤差 ~4e-6）
// x は log2(周波数) なので常に正。floorf はフラッシュ上のライブラリ関数で、Wi-Fi の処理と重なると
// 読み込み待ちで割り込みが遅れることがあるので、整数への切り捨てで代用する
// 割り込みから呼ぶ小さな関数は必ずその場に展開させる（2 か所以上から呼ぶとコンパイラが別関数に
// 切り出してフラッシュに置くことがあり、Wi-Fi のフラッシュ読み込みと重なると割り込みが 2 倍以上遅くなった）
#define ISR_INLINE static inline __attribute__((always_inline))

ISR_INLINE float fastExp2(float x) {
  int32_t xi = (int32_t)x;
  float f = x - (float)xi;
  float p = 1.0f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * 0.0096181f)));
  union { float f; int32_t i; } u;
  u.f = p;
  u.i += xi << 23;
  return u.f;
}

// 平方根。sqrtf はフラッシュ上のライブラリ関数になることがあるので、FPU の命令を直接使う
ISR_INLINE float fastSqrt(float x) {
  float r;
  asm("vsqrt.f32 %0, %1" : "=t"(r) : "t"(x));
  return r;
}

// ノコギリ波の段差を前後 1 サンプルぶん丸める補正項（t: 0..1 の位相, dt: 1 サンプルの位相増分）
ISR_INLINE float polyBlep(float t, float dt) {
  if (t < dt) {
    t /= dt;
    return t + t - t * t - 1.0f;
  }
  if (t > 1.0f - dt) {
    t = (t - 1.0f) / dt;
    return t * t + t + t + 1.0f;
  }
  return 0.0f;
}

uint pwmSlice;
volatile bool coreReady = false;  // Core0 の準備（係数・プリセット読み込み）が終わったら true

// PWM の 1 周期ごとに呼ばれ、次の周期のデューティを決める
void __not_in_flash_func(audioISR)() {
  static uint32_t phase = 0, phaseInc = 0;
  static uint32_t phaseH = 0, phaseIncH = 0;  // ハーモニー（2 声目）
  static float oct1 = 8.0313f, oct2 = 8.0313f;
  static float amp = 0.0f;
  static float qerr = 0.0f;
  static float mix[3] = {0.0f, 0.0f, 0.0f};
  static uint8_t div = 0;

  pwm_clear_irq(pwmSlice);
  uint32_t isrStart = timer_hw->timerawl;
  {
    static uint32_t lastUs = 0;
    uint32_t now = isrStart;
    uint32_t gap = now - lastUs;
    lastUs = now;
    if (gap > isrGapMaxUs) isrGapMaxUs = gap;
    if (gap > 20) isrGapLate++;
  }

  // グライド（2 段）と周波数→位相増分の変換は 8 サンプルに 1 回（約 12kHz）で十分
  if ((div++ & 7) == 0) {
    static uint32_t vibPhase = 0;
    float tOct = targetOct;
    float k = glideK;
    oct1 += (tOct - oct1) * k;
    oct2 += (oct1 - oct2) * k;
    // ビブラート: 音程をオクターブ単位で正弦波状に揺らす
    vibPhase += vibInc8;
    float vib = vibDepthOct * sineTable[vibPhase >> (32 - TABLE_BITS)];
    phaseInc = (uint32_t)(fastExp2(oct2 + vib) * hzToInc);
    phaseIncH = (uint32_t)((float)phaseInc * harmRatio);
  }
  float tAmp = targetAmp;
  amp += (tAmp - amp) * (tAmp > 0.0f ? ampK : releaseK);
  phase += phaseInc;
  for (int i = 0; i < 3; i++) mix[i] += (targetMix[i] - mix[i]) * ampK;

  uint32_t idx = phase >> (32 - TABLE_BITS);
  float frac = (phase & ((1u << (32 - TABLE_BITS)) - 1)) * (1.0f / (1u << (32 - TABLE_BITS)));
  float s0 = sineTable[idx];
  float sine = s0 + (sineTable[idx + 1] - s0) * frac;

  float t = phase * (1.0f / 4294967296.0f);
  float dt = phaseInc * (1.0f / 4294967296.0f);
  float tq = t + 0.25f;                        // 三角波はサイン波と山谷が重なる位相にする
  if (tq >= 1.0f) tq -= 1.0f;
  float tri = 1.0f - 4.0f * fabsf(tq - 0.5f);
  float t2 = t + 0.5f;                         // ノコギリ波は基本波の位相をサイン波に合わせる
  if (t2 >= 1.0f) t2 -= 1.0f;
  float saw = (2.0f * t2 - 1.0f - polyBlep(t2, dt)) * SAW_GAIN;

  float s = mix[0] * sine + mix[1] * tri + mix[2] * saw;

  // ---- ハーモニー: 同じ音色の 2 声目（量が 0 のときは計算しない）----
  float hm = harmMixV;
  if (hm > 0.0f) {
    phaseH += phaseIncH;
    uint32_t hi = phaseH >> (32 - TABLE_BITS);
    float hfr = (phaseH & ((1u << (32 - TABLE_BITS)) - 1)) * (1.0f / (1u << (32 - TABLE_BITS)));
    float h0 = sineTable[hi];
    float hsine = h0 + (sineTable[hi + 1] - h0) * hfr;
    float th = phaseH * (1.0f / 4294967296.0f);
    float dth = phaseIncH * (1.0f / 4294967296.0f);
    float thq = th + 0.25f;
    if (thq >= 1.0f) thq -= 1.0f;
    float htri = 1.0f - 4.0f * fabsf(thq - 0.5f);
    float th2 = th + 0.5f;
    if (th2 >= 1.0f) th2 -= 1.0f;
    float hsaw = (2.0f * th2 - 1.0f - polyBlep(th2, dth)) * SAW_GAIN;
    float sh = mix[0] * hsine + mix[1] * htri + mix[2] * hsaw;
    s = s * (1.0f - 0.5f * hm) + sh * 0.5f * hm;  // 量 100% で 主音 50% + 2 声目 50%
  }
  float dry = s * amp;

  // ---- オーバードライブ: なめらかに頭打ちする歪み（x/√(1+x²)）+ 音色のローパス ----
  if (odOn) {
    static float odLp = 0.0f;
    float xg = dry * odGain;
    float y = xg / fastSqrt(1.0f + xg * xg) * odNorm;  // 入力 1 のとき出力もほぼ 1 になるよう補正
    odLp += (y - odLp) * odToneK;
    dry = odLp;
  }

  // ---- レスリー（量が 0 でも回転と書き込みは続ける）----
  {
    static float xlp = 0.0f;
    static float hornInc = 0.0f, drumInc = 0.0f;
    static uint32_t hornPh = 0, drumPh = 0;
    static int lsPos = 0;
    if ((div & 7) == 1) {  // 回転速度の変化（慣性）は 8 サンプルに 1 回
      hornInc += (hornTargetInc - hornInc) * hornAccelK;
      drumInc += (drumTargetInc - drumInc) * drumAccelK;
    }
    hornPh += (uint32_t)hornInc;
    drumPh += (uint32_t)drumInc;
    xlp += (dry - xlp) * lsXoverK;
    float lo = xlp, hi = dry - xlp;
    // ホーン: 遅れを揺らして音程を揺らす（ドップラー）。音量の揺れは 90° ずらす（近づくとき大きい）
    lsBuf[lsPos] = hi;
    float hs = sineTable[hornPh >> (32 - TABLE_BITS)];
    float hc = sineTable[(hornPh + 0x40000000u) >> (32 - TABLE_BITS)];
    float d = lsBaseSamp + lsDopplerSamp * hs;
    int di = (int)d;
    float df = d - (float)di;
    int r0 = lsPos - di;
    if (r0 < 0) r0 += LS_MAX;
    int r1 = r0 - 1;
    if (r1 < 0) r1 += LS_MAX;
    float horn = (lsBuf[r0] + (lsBuf[r1] - lsBuf[r0]) * df) * (1.0f - HORN_AM * 0.5f * (1.0f - hc));
    lsPos = lsPos + 1 >= LS_MAX ? 0 : lsPos + 1;
    // ドラム: 音量だけ揺らす
    float ds = sineTable[drumPh >> (32 - TABLE_BITS)];
    float drum = lo * (1.0f - DRUM_AM * 0.5f * (1.0f - ds));
    float lm = lsMixV;
    dry = dry * (1.0f - lm) + (horn + drum) * lm;
  }

  // ---- コーラス: 揺らし方を逆向きにした 2 声を重ねる（量が 0 でも書き込みは続ける）----
  static int chPos = 0;
  static uint32_t chPhase = 0;
  chBuf[chPos] = dry;
  chPhase += chInc;
  float chWet = 0.0f;
  for (int v = 0; v < 2; v++) {
    float mod = sineTable[(chPhase + (v ? 0x80000000u : 0u)) >> (32 - TABLE_BITS)];
    float d = chBaseSamp + chDepthSamp * mod;   // 遅れ（サンプル、小数あり）
    int di = (int)d;
    float df = d - (float)di;
    int r0 = chPos - di;
    if (r0 < 0) r0 += CH_MAX;
    int r1 = r0 - 1;
    if (r1 < 0) r1 += CH_MAX;
    chWet += chBuf[r0] + (chBuf[r1] - chBuf[r0]) * df;  // 線形補間
  }
  chPos = chPos + 1 >= CH_MAX ? 0 : chPos + 1;
  float cm = chMixV;
  dry = dry * (1.0f - 0.5f * cm) + chWet * 0.25f * cm;  // 量 100% で 原音 50% + 揺れた音 50%

  // ---- ディレイ（量が 0 でも回し続け、オンにした瞬間に古い音が出ないようにする）----
  static uint32_t dpos = 0;
  static float dlp = 0.0f;
  uint32_t dlen = delaySamples;
  uint32_t rp = dpos >= dlen ? dpos - dlen : dpos + DELAY_MAX - dlen;
  float dOut = delayBuf[rp] * (1.0f / 16384.0f);
  dlp += (dOut - dlp) * DELAY_TONE_K;
  float dw = dry + dlp * delayFbV;
  if (dw > 1.99f) dw = 1.99f;
  if (dw < -1.99f) dw = -1.99f;
  delayBuf[dpos] = (int16_t)(dw * 16384.0f);
  dpos = dpos + 1 >= DELAY_MAX ? 0 : dpos + 1;
  float wetIn = dry + dOut * delayMixV;

  // ---- リバーブ（Freeverb 型）----
  static int cpos[REV_NCOMB] = {0}, apos[REV_NAP] = {0};
  static float cstore[REV_NCOMB] = {0};
  float rin = wetIn * 0.015f;
  float fb = revFbV, damp = revDampV, damp1 = 1.0f - damp;
  float racc = 0.0f;
  for (int c = 0; c < REV_NCOMB; c++) {
    float *b = revBuf + revCombOfs[c];
    int ps = cpos[c];
    float o = b[ps];
    cstore[c] = o * damp1 + cstore[c] * damp;
    b[ps] = rin + cstore[c] * fb;
    cpos[c] = ps + 1 >= revCombLen[c] ? 0 : ps + 1;
    racc += o;
  }
  for (int a = 0; a < REV_NAP; a++) {
    float *b = revBuf + revApOfs[a];
    int ps = apos[a];
    float bo = b[ps];
    b[ps] = racc + bo * 0.5f;
    racc = bo - racc;
    apos[a] = ps + 1 >= revApLen[a] ? 0 : ps + 1;
  }
  // 響きは入力の 1.2〜2.5 倍に膨らむ（96kHz で実測相当のシミュレーション）。元の音に上乗せすると
  // 量 15% あたりから出力の上限を超えてガリガリと割れたので、量に応じて元の音を下げながら混ぜる
  float rm = revMixV;
  float out = wetIn * (1.0f - 0.4f * rm) + racc * 1.8f * rm;

  // リミッター: ±0.6 までは素通し、それを超えたらなめらかに ±1 へ近づける（角のある頭打ちにしない）
  {
    const float T = 0.6f;
    float ax = fabsf(out);
    if (ax > T) {
      float u = (ax - T) * (1.0f / (1.0f - T));
      float y = T + (1.0f - T) * u / (1.0f + u);
      out = out > 0.0f ? y : -y;
    }
  }

  // 0..PWM_TOP に変換し、前回の量子化誤差を足し込む（1 次ノイズシェーピング）
  float x = (out * OUT_LEVEL * 0.5f + 0.5f) * PWM_TOP + qerr;
  int32_t q = (int32_t)(x + 0.5f);
  if (q < 0) q = 0;
  if (q > PWM_TOP) q = PWM_TOP;
  qerr = x - q;
  pwm_set_gpio_level(PIN_AUDIO, q);  // 次の周期の頭で反映される
  uint32_t dur = timer_hw->timerawl - isrStart;
  if (dur > isrDurMaxUs) isrDurMaxUs = dur;
}

// 設定の範囲を整え、係数を計算し直す
void applyParams() {
  Params &p = prm;
  for (int i = 0; i < 3; i++) p.mix[i] = constrain(p.mix[i], 0.0f, 1.0f);
  p.lowHz = constrain(p.lowHz, 30.0f, 2000.0f);
  p.octaves = constrain(p.octaves, 0.5f, 5.0f);
  p.glideMs = constrain(p.glideMs, 1.0f, 300.0f);
  p.minCm = constrain(p.minCm, 1.0f, 50.0f);
  p.maxCm = constrain(p.maxCm, p.minCm + 5.0f, 120.0f);
  p.holdCm = constrain(p.holdCm, 0.0f, 30.0f);
  p.volMinCm = constrain(p.volMinCm, 1.0f, 50.0f);
  p.volMaxCm = constrain(p.volMaxCm, p.volMinCm + 5.0f, 120.0f);
  p.volDbRange = constrain(p.volDbRange, 10.0f, 80.0f);
  p.releaseAbruptMs = constrain(p.releaseAbruptMs, 1.0f, 2000.0f);
  p.releaseCutMs = constrain(p.releaseCutMs, 1.0f, 500.0f);
  p.volAbruptDb = constrain(p.volAbruptDb, -60.0f, -3.0f);
  p.vibCents = constrain(p.vibCents, 0.0f, 200.0f);
  p.vibRateHz = constrain(p.vibRateHz, 0.5f, 15.0f);
  p.delayMix = constrain(p.delayMix, 0.0f, 1.0f);
  p.delayMs = constrain(p.delayMs, 20.0f, 1000.0f);
  p.delayFeedback = constrain(p.delayFeedback, 0.0f, 0.9f);
  p.reverbMix = constrain(p.reverbMix, 0.0f, 1.0f);
  p.reverbSize = constrain(p.reverbSize, 0.0f, 1.0f);
  p.reverbDamp = constrain(p.reverbDamp, 0.0f, 1.0f);
  p.chorusMix = constrain(p.chorusMix, 0.0f, 1.0f);
  p.chorusRateHz = constrain(p.chorusRateHz, 0.05f, 5.0f);
  p.chorusDepthMs = constrain(p.chorusDepthMs, 0.0f, 10.0f);
  p.odDrive = constrain(p.odDrive, 0.0f, 1.0f);
  p.odTone = constrain(p.odTone, 0.0f, 1.0f);
  p.leslieMix = constrain(p.leslieMix, 0.0f, 1.0f);
  p.leslieFast = p.leslieFast >= 0.5f ? 1.0f : 0.0f;
  p.harmonyMix = constrain(p.harmonyMix, 0.0f, 1.0f);
  p.harmonySemis = constrain(roundf(p.harmonySemis), -12.0f, 12.0f);
  p.pitchBeta = constrain(p.pitchBeta, 0.0f, 3.0f);
  p.volBeta = constrain(p.volBeta, 0.0f, 3.0f);

  // グライドは 8 サンプルに 1 回更新するので、その周期で係数を作る（1 段あたり glideMs / 2）
  glideK = 1.0f - expf(-1000.0f * 8.0f / (p.glideMs * 0.5f * sampleRate));
  releaseAbruptK = 1.0f - expf(-1000.0f / (p.releaseAbruptMs * sampleRate));
  releaseCutK = 1.0f - expf(-1000.0f / (p.releaseCutMs * sampleRate));

  vibDepthOct = p.vibCents / 1200.0f;
  vibInc8 = (uint32_t)((double)p.vibRateHz * 8.0 / sampleRate * 4294967296.0);
  uint32_t ds = (uint32_t)(p.delayMs * sampleRate / 1000.0f);
  delaySamples = ds < 1 ? 1 : (ds > DELAY_MAX - 1 ? DELAY_MAX - 1 : ds);
  delayMixV = p.delayMix;
  delayFbV = p.delayFeedback;
  // 量 50% を超えると響きが勝ちすぎて使えなかったので、スライダーの 100% を以前の 50% にあてる
  revMixV = 0.5f * p.reverbMix;
  revFbV = 0.7f + 0.28f * p.reverbSize;   // Freeverb と同じ割り当て
  revDampV = 0.4f * p.reverbDamp;
  chMixV = p.chorusMix;
  chBaseSamp = CHORUS_BASE_MS * sampleRate / 1000.0f;
  chDepthSamp = p.chorusDepthMs * sampleRate / 1000.0f;
  chInc = (uint32_t)((double)p.chorusRateHz / sampleRate * 4294967296.0);

  // オーバードライブ: 歪み 0〜1 を入力の倍率 1〜16 に。音色 0〜1 をローパス 1.5k〜9kHz に
  float g = 1.0f + 15.0f * p.odDrive;
  odGain = g;
  odNorm = sqrtf(1.0f + g * g) / g;
  float fc = 1500.0f + 7500.0f * p.odTone;
  odToneK = 1.0f - expf(-2.0f * PI * fc / sampleRate);
  odOn = p.odDrive > 0.001f;

  // レスリー
  lsMixV = p.leslieMix;
  float hz = p.leslieFast > 0.5f ? HORN_FAST_HZ : HORN_SLOW_HZ;
  float dz = p.leslieFast > 0.5f ? DRUM_FAST_HZ : DRUM_SLOW_HZ;
  hornTargetInc = (float)((double)hz / sampleRate * 4294967296.0);
  drumTargetInc = (float)((double)dz / sampleRate * 4294967296.0);

  harmRatio = powf(2.0f, p.harmonySemis / 12.0f);
  harmMixV = p.harmonyMix;
}

// リバーブのバッファの割り付けと、サンプルレートで決まる係数（サンプルレートが決まってから 1 回だけ）
void reverbInit() {
  lsXoverK = 1.0f - expf(-2.0f * PI * LESLIE_XOVER_HZ / sampleRate);
  hornAccelK = 1.0f - expf(-8.0f / (HORN_ACCEL_S * sampleRate));  // 8 サンプルに 1 回更新するぶん
  drumAccelK = 1.0f - expf(-8.0f / (DRUM_ACCEL_S * sampleRate));
  lsBaseSamp = 1.0f * sampleRate / 1000.0f;                         // 中心の遅れ 1ms
  lsDopplerSamp = HORN_DOPPLER_MS * sampleRate / 1000.0f;
  float scale = sampleRate / 44100.0f;
  int ofs = 0;
  for (int c = 0; c < REV_NCOMB; c++) {
    revCombLen[c] = (int)(REV_COMB_44K[c] * scale);
    revCombOfs[c] = ofs;
    ofs += revCombLen[c];
  }
  for (int a = 0; a < REV_NAP; a++) {
    revApLen[a] = (int)(REV_AP_44K[a] * scale);
    revApOfs[a] = ofs;
    ofs += revApLen[a];
  }
  if (ofs > REV_BUF_MAX) Serial.println("reverb buffer too small");
}

// ---- プリセット（EEPROM エミュレーション = フラッシュ）----
const int N_PRESETS = 8;
const uint32_t PRESET_MAGIC_V1 = 0x44474d31;  // "DGM1"（エフェクト追加前）
const uint32_t PRESET_MAGIC_V2 = 0x44474d32;  // "DGM2"（コーラス追加前）
const uint32_t PRESET_MAGIC_V3 = 0x44474d33;  // "DGM3"（オーバードライブ・レスリー追加前）
const uint32_t PRESET_MAGIC_V4 = 0x44474d34;  // "DGM4"（リバーブの量の目盛りを変える前）
const uint32_t PRESET_MAGIC_V5 = 0x44474d35;  // "DGM5"（ハーモニー追加前）
const uint32_t PRESET_MAGIC_V6 = 0x44474d36;  // "DGM6"（追従の速さ追加前）
const uint32_t PRESET_MAGIC = 0x44474d37;     // "DGM7"
struct PresetSlot {
  char name[24];
  uint8_t used;
  Params p;
};
// Wi-Fi のパスワードはプリセットのうしろに置く
const uint32_t WIFI_MAGIC = 0x44475750;  // "DGWP"
struct WifiCfg {
  uint32_t magic;
  char pass[64];  // WPA2 は 8〜63 文字。空 = パスワードなし
};
const int WIFI_CFG_ADDR = 4 + sizeof(PresetSlot) * N_PRESETS;
const int EEPROM_SIZE = WIFI_CFG_ADDR + sizeof(WifiCfg);

int slotAddr(int i) { return 4 + i * (int)sizeof(PresetSlot); }

void presetsInit() {
  EEPROM.begin(EEPROM_SIZE);
  uint32_t magic;
  EEPROM.get(0, magic);
  // 古い形式のプリセットを読み替える。設定の項目は常に末尾に追加しているので、
  // 古い形式の長さぶんをそのまま写し、増えた項目は初期値（エフェクトはオフ）で埋める
  int oldSize = magic == PRESET_MAGIC_V1 ? (int)offsetof(Params, vibCents)
              : magic == PRESET_MAGIC_V2 ? (int)offsetof(Params, chorusMix)
              : magic == PRESET_MAGIC_V3 ? (int)offsetof(Params, odDrive)
              : magic == PRESET_MAGIC_V4 ? (int)offsetof(Params, harmonyMix)
              : magic == PRESET_MAGIC_V5 ? (int)offsetof(Params, harmonyMix)
              : magic == PRESET_MAGIC_V6 ? (int)offsetof(Params, pitchBeta) : 0;
  if (oldSize > 0) {
    const int nameUsedSize = (int)offsetof(PresetSlot, p);  // name[24] + used + 詰め物
    const int oldSlotSize = nameUsedSize + oldSize;
    PresetSlot slots[N_PRESETS];
    for (int i = 0; i < N_PRESETS; i++) {
      int addr = 4 + i * oldSlotSize;
      memset(&slots[i], 0, sizeof(PresetSlot));
      for (int k = 0; k < nameUsedSize; k++) ((uint8_t *)&slots[i])[k] = EEPROM.read(addr + k);
      slots[i].p = DEFAULT_PARAMS;
      for (int k = 0; k < oldSize; k++) ((uint8_t *)&slots[i].p)[k] = EEPROM.read(addr + nameUsedSize + k);
      // v4 以前はリバーブの量の目盛りが半分だったので、同じ響きになるよう 2 倍にする
      if (magic != PRESET_MAGIC_V5 && magic != PRESET_MAGIC_V6) slots[i].p.reverbMix = min(1.0f, slots[i].p.reverbMix * 2.0f);
    }
    for (int i = 0; i < N_PRESETS; i++) EEPROM.put(slotAddr(i), slots[i]);
    EEPROM.put(0, PRESET_MAGIC);
    EEPROM.commit();
  } else if (magic != PRESET_MAGIC) {  // 初回: 全スロットを空にする
    PresetSlot empty;
    memset(&empty, 0, sizeof(empty));
    for (int i = 0; i < N_PRESETS; i++) EEPROM.put(slotAddr(i), empty);
    EEPROM.put(0, PRESET_MAGIC);
    EEPROM.commit();
  }
}

void wifiCfgLoad() {
  WifiCfg c;
  EEPROM.get(WIFI_CFG_ADDR, c);
  c.pass[sizeof(c.pass) - 1] = 0;
  wifiPass = c.magic == WIFI_MAGIC ? String(c.pass) : String("");
}

// 空か 8〜63 文字の表示できる ASCII 文字だけを受け付ける
bool wifiPassValid(const String &p) {
  if (p.length() != 0 && (p.length() < 8 || p.length() > 63)) return false;
  for (unsigned i = 0; i < p.length(); i++)
    if (p[i] < 0x20 || p[i] > 0x7E) return false;
  return true;
}

void wifiCfgSave(const String &p) {
  WifiCfg c;
  memset(&c, 0, sizeof(c));
  c.magic = WIFI_MAGIC;
  strncpy(c.pass, p.c_str(), sizeof(c.pass) - 1);
  EEPROM.put(WIFI_CFG_ADDR, c);
  EEPROM.commit();
}

bool presetLoad(int i) {
  PresetSlot s;
  EEPROM.get(slotAddr(i), s);
  if (!s.used) return false;
  prm = s.p;
  applyParams();
  return true;
}

void presetSave(int i, const String &name) {
  PresetSlot s;
  memset(&s, 0, sizeof(s));
  name.substring(0, sizeof(s.name) - 1).toCharArray(s.name, sizeof(s.name));
  s.used = 1;
  s.p = prm;
  EEPROM.put(slotAddr(i), s);
  EEPROM.commit();  // フラッシュ書き込み中（数十 ms）は音が止まる
}

// 組み込みプリセット: 音色とエフェクトだけを書き換える（音程・距離・音量の設定はそのまま）。
// 全部のエフェクトの値を持たせて、前に使っていたエフェクトが残らないようにする
struct Builtin {
  const char *id;
  const char *label;
  float mix[3];                             // サイン, 三角, ノコギリ
  float vibCents, vibRateHz;
  float odDrive, odTone;
  float leslieMix, leslieFast;
  float chorusMix, chorusRateHz, chorusDepthMs;
  float delayMix, delayMs, delayFeedback;
  float reverbMix, reverbSize, reverbDamp;
  float harmonyMix, harmonySemis;
};

const Builtin BUILTINS[] = {
  //  id              名前                   サイン 三角 ノコギリ  ビブラート  歪み 音色   レスリー  コーラス         ディレイ           リバーブ         ハーモニー
  {"classic",  "クラシック・テルミン", {0.85f, 0.15f, 0.0f}, 28, 5.5f, 0.0f, 0.6f, 0, 0, 0.0f, 0.8f, 3.0f, 0.0f, 350, 0.35f, 0.32f, 0.75f, 0.45f, 0.0f, 4},
  {"scifi",    "SF映画",               {0.6f, 0.4f, 0.0f}, 45, 6.8f, 0.0f, 0.6f, 0, 0, 0.0f, 0.8f, 3.0f, 0.35f, 420, 0.5f, 0.35f, 0.85f, 0.35f, 0.0f, 4},
  {"voice",    "ボーカル風",           {0.35f, 0.65f, 0.0f}, 16, 5.0f, 0.0f, 0.6f, 0, 0, 0.35f, 0.6f, 3.0f, 0.0f, 350, 0.35f, 0.25f, 0.7f, 0.5f, 0.0f, 4},
  {"synth",    "シンセリード",         {0.0f, 0.2f, 0.8f}, 8, 5.5f, 0.15f, 0.7f, 0, 0, 0.25f, 0.9f, 2.5f, 0.2f, 300, 0.25f, 0.15f, 0.6f, 0.5f, 0.0f, 4},
  {"jazzorgan", "ジャズオルガン",      {0.6f, 0.3f, 0.1f}, 0, 5.5f, 0.3f, 0.55f, 1, 0, 0.0f, 0.8f, 3.0f, 0.0f, 350, 0.35f, 0.20f, 0.6f, 0.5f, 0.0f, 4},
  {"rockorgan", "ロックオルガン",      {0.5f, 0.3f, 0.2f}, 0, 5.5f, 0.6f, 0.65f, 1, 1, 0.0f, 0.8f, 3.0f, 0.0f, 350, 0.35f, 0.18f, 0.6f, 0.5f, 0.0f, 4},
  {"guitar",   "ギター風リード",       {0.0f, 0.25f, 0.75f}, 10, 5.0f, 0.75f, 0.5f, 0, 0, 0.0f, 0.8f, 3.0f, 0.25f, 380, 0.35f, 0.20f, 0.65f, 0.5f, 0.0f, 4},
  {"twinlead", "ツインリード",         {0.0f, 0.25f, 0.75f}, 10, 5.0f, 0.75f, 0.5f, 0, 0, 0.0f, 0.8f, 3.0f, 0.25f, 380, 0.35f, 0.20f, 0.65f, 0.5f, 0.8f, 4},
  {"strings",  "ストリングス風",       {0.0f, 0.5f, 0.5f}, 10, 5.0f, 0.0f, 0.6f, 0, 0, 0.6f, 0.4f, 5.0f, 0.0f, 350, 0.35f, 0.45f, 0.85f, 0.45f, 0.0f, 4},
  {"ambient",  "アンビエント",         {1.0f, 0.0f, 0.0f}, 6, 4.5f, 0.0f, 0.6f, 0, 0, 0.2f, 0.3f, 4.0f, 0.45f, 600, 0.6f, 0.50f, 0.9f, 0.3f, 0.0f, 4},
};
const int N_BUILTINS = sizeof(BUILTINS) / sizeof(BUILTINS[0]);

bool applyBuiltin(const String &id) {
  for (int i = 0; i < N_BUILTINS; i++) {
    const Builtin &b = BUILTINS[i];
    if (id != b.id) continue;
    Params &p = prm;
    for (int k = 0; k < 3; k++) p.mix[k] = b.mix[k];
    p.vibCents = b.vibCents; p.vibRateHz = b.vibRateHz;
    p.odDrive = b.odDrive; p.odTone = b.odTone;
    p.leslieMix = b.leslieMix; p.leslieFast = b.leslieFast;
    p.chorusMix = b.chorusMix; p.chorusRateHz = b.chorusRateHz; p.chorusDepthMs = b.chorusDepthMs;
    p.delayMix = b.delayMix; p.delayMs = b.delayMs; p.delayFeedback = b.delayFeedback;
    p.reverbMix = b.reverbMix; p.reverbSize = b.reverbSize; p.reverbDamp = b.reverbDamp;
    p.harmonyMix = b.harmonyMix; p.harmonySemis = b.harmonySemis;
    applyParams();
    return true;
  }
  return false;
}

String builtinsJson() {
  String j = "[";
  for (int i = 0; i < N_BUILTINS; i++) {
    if (i) j += ",";
    j += "{\"id\":\"" + String(BUILTINS[i].id) + "\",\"label\":\"" + String(BUILTINS[i].label) + "\"}";
  }
  return j + "]";
}

void presetDelete(int i) {
  PresetSlot s;
  memset(&s, 0, sizeof(s));
  EEPROM.put(slotAddr(i), s);
  EEPROM.commit();
}

// ---- ウェブ ----
WebServer server(80);
DNSServer dns;

extern const char INDEX_HTML[];

String jsonEscape(const char *s) {
  String o;
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') o += '\\';
    o += *s;
  }
  return o;
}

String stateJson() {
  const Params &p = prm;
  String j = "{";
  j += "\"mix\":[" + String(p.mix[0], 3) + "," + String(p.mix[1], 3) + "," + String(p.mix[2], 3) + "],";
  j += "\"lowHz\":" + String(p.lowHz, 2) + ",\"octaves\":" + String(p.octaves, 2) +
       ",\"glideMs\":" + String(p.glideMs, 1) + ",";
  j += "\"minCm\":" + String(p.minCm, 1) + ",\"maxCm\":" + String(p.maxCm, 1) +
       ",\"holdCm\":" + String(p.holdCm, 1) + ",";
  j += "\"volMinCm\":" + String(p.volMinCm, 1) + ",\"volMaxCm\":" + String(p.volMaxCm, 1) +
       ",\"volDbRange\":" + String(p.volDbRange, 1) + ",";
  j += "\"releaseAbruptMs\":" + String(p.releaseAbruptMs, 1) + ",\"releaseCutMs\":" + String(p.releaseCutMs, 1) +
       ",\"volAbruptDb\":" + String(p.volAbruptDb, 1) + ",";
  j += "\"vibCents\":" + String(p.vibCents, 1) + ",\"vibRateHz\":" + String(p.vibRateHz, 2) + ",";
  j += "\"delayMix\":" + String(p.delayMix, 3) + ",\"delayMs\":" + String(p.delayMs, 0) +
       ",\"delayFeedback\":" + String(p.delayFeedback, 3) + ",";
  j += "\"reverbMix\":" + String(p.reverbMix, 3) + ",\"reverbSize\":" + String(p.reverbSize, 3) +
       ",\"reverbDamp\":" + String(p.reverbDamp, 3) + ",";
  j += "\"chorusMix\":" + String(p.chorusMix, 3) + ",\"chorusRateHz\":" + String(p.chorusRateHz, 2) +
       ",\"chorusDepthMs\":" + String(p.chorusDepthMs, 2) + ",";
  j += "\"odDrive\":" + String(p.odDrive, 3) + ",\"odTone\":" + String(p.odTone, 3) +
       ",\"leslieMix\":" + String(p.leslieMix, 3) + ",\"leslieFast\":" + String(p.leslieFast, 0) + ",";
  j += "\"harmonyMix\":" + String(p.harmonyMix, 3) + ",\"harmonySemis\":" + String(p.harmonySemis, 0) + ",";
  j += "\"pitchBeta\":" + String(p.pitchBeta, 2) + ",\"volBeta\":" + String(p.volBeta, 2) + ",";
  float pc = liveCm[0], vc = liveCm[1];
  j += "\"live\":{\"pitchCm\":" + (pc < 1000 ? String(pc, 1) : String("null")) +
       ",\"volCm\":" + (vc < 1000 ? String(vc, 1) : String("null")) +
       ",\"hz\":" + String((float)targetHz, 1) + ",\"amp\":" + String((float)targetAmp, 2) +
       ",\"pot\":" + String((float)knobPos, 2) + "},";
  j += "\"wifiPass\":" + String(wifiPass.length() > 0 ? "true" : "false") +
       ",\"wifiOpen\":" + String(wifiState == WIFI_ST_OPEN ? "true" : "false") + ",";
  j += "\"ssid\":\"" + wifiSsid + "\"";
  j += "}";
  return j;
}

String presetsJson() {
  String j = "[";
  for (int i = 0; i < N_PRESETS; i++) {
    PresetSlot s;
    EEPROM.get(slotAddr(i), s);
    if (i) j += ",";
    j += "{\"slot\":" + String(i) + ",\"used\":" + String(s.used ? "true" : "false") +
         ",\"name\":\"" + (s.used ? jsonEscape(s.name) : String("")) + "\"}";
  }
  return j + "]";
}

void handleSet() {
  Params &p = prm;
  struct { const char *key; float *v; } keys[] = {
    {"sine", &p.mix[0]}, {"tri", &p.mix[1]}, {"saw", &p.mix[2]},
    {"lowHz", &p.lowHz}, {"octaves", &p.octaves}, {"glideMs", &p.glideMs},
    {"minCm", &p.minCm}, {"maxCm", &p.maxCm}, {"holdCm", &p.holdCm},
    {"volMinCm", &p.volMinCm}, {"volMaxCm", &p.volMaxCm}, {"volDbRange", &p.volDbRange},
    {"releaseAbruptMs", &p.releaseAbruptMs}, {"releaseCutMs", &p.releaseCutMs}, {"volAbruptDb", &p.volAbruptDb},
    {"vibCents", &p.vibCents}, {"vibRateHz", &p.vibRateHz},
    {"delayMix", &p.delayMix}, {"delayMs", &p.delayMs}, {"delayFeedback", &p.delayFeedback},
    {"reverbMix", &p.reverbMix}, {"reverbSize", &p.reverbSize}, {"reverbDamp", &p.reverbDamp},
    {"chorusMix", &p.chorusMix}, {"chorusRateHz", &p.chorusRateHz}, {"chorusDepthMs", &p.chorusDepthMs},
    {"odDrive", &p.odDrive}, {"odTone", &p.odTone}, {"leslieMix", &p.leslieMix}, {"leslieFast", &p.leslieFast},
    {"harmonyMix", &p.harmonyMix}, {"harmonySemis", &p.harmonySemis},
    {"pitchBeta", &p.pitchBeta}, {"volBeta", &p.volBeta},
  };
  for (auto &k : keys) {
    if (server.hasArg(k.key)) *k.v = server.arg(k.key).toFloat();
  }
  applyParams();
  server.send(200, "application/json", stateJson());
}

int slotArg() {
  if (!server.hasArg("slot")) return -1;
  int i = server.arg("slot").toInt();
  return (i >= 0 && i < N_PRESETS) ? i : -1;
}

// 操作画面の URL を登録する（起動時に 1 回。Wi-Fi のオン / オフとは別）
void webInit() {
  pico_unique_board_id_t uid;
  pico_get_unique_board_id(&uid);
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%02X%02X", uid.id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES - 2],
           uid.id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES - 1]);
  wifiSsid = String(WIFI_SSID_BASE) + "-" + suffix;
  server.on("/", []() { server.send(200, "text/html; charset=utf-8", INDEX_HTML); });
  server.on("/api/state", []() { server.send(200, "application/json", stateJson()); });
  server.on("/api/set", handleSet);
  server.on("/api/reset", []() {
    prm = DEFAULT_PARAMS;
    applyParams();
    server.send(200, "application/json", stateJson());
  });
  server.on("/api/builtins", []() { server.send(200, "application/json", builtinsJson()); });
  server.on("/api/builtin", []() {
    if (!server.hasArg("id") || !applyBuiltin(server.arg("id"))) { server.send(404, "text/plain", "unknown"); return; }
    server.send(200, "application/json", stateJson());
  });
  server.on("/api/presets", []() { server.send(200, "application/json", presetsJson()); });
  server.on("/api/preset/save", []() {
    int i = slotArg();
    if (i < 0) { server.send(400, "text/plain", "bad slot"); return; }
    presetSave(i, server.hasArg("name") ? server.arg("name") : String("Preset ") + (i + 1));
    server.send(200, "application/json", presetsJson());
  });
  server.on("/api/preset/load", []() {
    int i = slotArg();
    if (i < 0 || !presetLoad(i)) { server.send(404, "text/plain", "empty"); return; }
    server.send(200, "application/json", stateJson());
  });
  server.on("/api/preset/delete", []() {
    int i = slotArg();
    if (i < 0) { server.send(400, "text/plain", "bad slot"); return; }
    presetDelete(i);
    server.send(200, "application/json", presetsJson());
  });
  // パスワードの変更（POST。保存して 1.5 秒後に再起動し、新しいパスワードで Wi-Fi を立ち上げ直す）
  server.on("/api/wifi/pass", HTTP_POST, []() {
    String p = server.arg("pass");
    if (!wifiPassValid(p)) { server.send(400, "text/plain", "bad password"); return; }
    wifiCfgSave(p);
    wifiPass = p;
    server.send(200, "application/json", "{\"ok\":true}");
    rebootAtMs = millis() + 1500;
  });
  server.onNotFound([]() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/");
    server.send(302, "text/plain", "");
  });
}

// Wi-Fi をつまみの状態に合わせる（Core0）。切り替えのたびにアクセスポイントを立ち上げ直す
void wifiApply(WifiState st) {
  if (wifiState != WIFI_ST_OFF) {
    dns.stop();
    WiFi.disconnectAP();
  }
  wifiState = st;
  if (st == WIFI_ST_OFF) {
    Serial.println("Wi-Fi off (knob down)");
    return;
  }
  static bool serverStarted = false;
  bool usePass = wifiPass.length() > 0 && st == WIFI_ST_PASS;
  WiFi.mode(WIFI_AP);
  IPAddress apIp(192, 168, 42, 1);  // 決めておかないとライブラリの既定（192.168.4.1）になる
  WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0));
  WiFi.softAP(wifiSsid.c_str(), usePass ? wifiPass.c_str() : nullptr);
  IPAddress ip = WiFi.softAPIP();
  dns.start(53, "*", ip);  // どのアドレスを開いても操作画面に来るようにする（キャプティブポータル）
  if (!serverStarted) {
    server.begin();
    serverStarted = true;
  }
  Serial.print("Wi-Fi AP \"");
  Serial.print(wifiSsid);
  Serial.print("\" started, http://");
  Serial.print(ip);
  Serial.println(usePass ? "  (password set)" : st == WIFI_ST_OPEN ? "  (no password: knob full up)" : "  (no password)");
}

// つまみの位置から Wi-Fi の状態を決める。境目でパタパタしないようにヒステリシスをつける
// （オフ ↔ オン: 3% / 6%、パスワードあり ↔ なし: 85% / 90%）
WifiState knobWifiState(WifiState cur, float pot) {
  if (pot < 0.03f) return WIFI_ST_OFF;
  if (cur == WIFI_ST_OFF && pot <= 0.06f) return WIFI_ST_OFF;
  if (pot >= 0.90f) return WIFI_ST_OPEN;
  if (cur == WIFI_ST_OPEN && pot >= 0.85f) return WIFI_ST_OPEN;
  return WIFI_ST_PASS;
}

// つまみを回している途中で何度も立ち上げ直さないよう、同じ状態が 0.7 秒続いてから切り替える
void wifiKnobPoll() {
  static WifiState want = WIFI_ST_OFF;
  static uint32_t sinceMs = 0;
  WifiState st = knobWifiState(wifiState, knobPos);
  uint32_t nowMs = millis();
  if (st != want) {
    want = st;
    sinceMs = nowMs;
  }
  if (want != wifiState && nowMs - sinceMs >= 700) wifiApply(want);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("theremin_pico2w start");

  for (int i = 0; i <= TABLE_SIZE; i++) {
    sineTable[i] = sinf(2.0f * PI * i / TABLE_SIZE);
  }
  sampleRate = (float)clock_get_hz(clk_sys) / (PWM_TOP + 1);
  ampK = 1.0f - expf(-1000.0f / (AMP_MS * sampleRate));
  hzToInc = 4294967296.0f / sampleRate;

  reverbInit();
  presetsInit();
  wifiCfgLoad();
  analogReadResolution(12);
  knobPos = analogRead(PIN_KNOB) / 4095.0f;  // 起動時の Wi-Fi はすぐにつまみの位置で決める（Core1 が動く前）
  if (!presetLoad(0)) applyParams();  // 起動時の設定（0 番の枠）が保存してあれば読み込む
  releaseK = releaseCutK;
  targetOct = log2f(prm.lowHz);

  coreReady = true;  // ここまでの準備ができたら Core1 が音の割り込みを始める


  pinMode(LED_BUILTIN, OUTPUT);
  webInit();
  wifiApply(knobWifiState(WIFI_ST_OFF, knobPos));
  // 電源回路（SMPS）を省電力モードから PWM モードに固定する。Wi-Fi チップの GPIO1 につながっている
  // （arduino-pico ではピン 65）。省電力モードは 3.3V の揺れが大きく、PWM の音声出力は 3.3V そのものが
  // 振幅になるので、揺れがそのままノイズとして乗る
  pinMode(PIN_SMPS_MODE, OUTPUT);
  digitalWrite(PIN_SMPS_MODE, HIGH);
}

void loop() {
  static bool ledNow = false;
  wifiKnobPoll();
  if (wifiState != WIFI_ST_OFF) {
    dns.processNextRequest();
    server.handleClient();
  }
  bool w = ledWanted;
  if (w != ledNow) {  // LED は Wi-Fi チップ経由なので、変わったときだけ書く
    ledNow = w;
    digitalWrite(LED_BUILTIN, w ? HIGH : LOW);
  }

  printReport();  // 診断出力（Core1 では出さない。理由は printReport の説明）

  if (rebootAtMs && (int32_t)(millis() - rebootAtMs) >= 0) rp2040.reboot();

}

// ================= Core1: センサー =================

const uint32_t TOF_PERIOD_MS = 25;    // VL53L1X の測定時間 兼 測定周期（VL53L4CD は L4CD_PERIOD_MS）
const uint8_t TOF_ROI = 8;            // 受光エリア 8x8（視野 約20°）
const uint8_t TOF_ADDR_PITCH = 0x30;  // 音程側は起動時にこのアドレスへ変更（音量側は 0x29 のまま。古い配線で同じバスに並んでもぶつからない）

// 飛び値除去 + 取りこぼし耐性
struct HandFilter {
  uint8_t missLimit;  // 範囲外がこの回数だけ続いたら「手なし」とみなす
  float lastCm;       // 最後に採用した距離（-1 = なし）
  float pendingCm;    // 保留中の飛び値（-1 = なし）
  uint8_t missCount;
  uint32_t rejects;

  // 1: 採用（*out に距離） / 0: 保留（何もしない） / -1: 手がなくなった
  int8_t update(float cm, float maxCm, float *out) {
    if (cm > maxCm) {
      if (missCount < 255) missCount++;
      if (missCount >= missLimit) {
        lastCm = pendingCm = -1;  // 次に手を入れたときは即採用
        return -1;
      }
      return 0;
    }
    missCount = 0;
    if (lastCm < 0 || fabsf(cm - lastCm) <= JUMP_CM ||
        (pendingCm >= 0 && fabsf(cm - pendingCm) <= JUMP_CM)) {
      lastCm = cm;
      pendingCm = -1;
      *out = cm;
      return 1;
    }
    pendingCm = cm;
    rejects++;
    return 0;
  }
};

// 何回続いたら「手なし」とするかはセンサーで変える（tofInitAll で設定）。
// VL53L1X（約 30 回/秒、受光エリアを絞れる）: 1 回。範囲内での取りこぼしがほぼない。
// VL53L4CD（約 100 回/秒、視野 18° 固定）: 4 回 = 40ms。遠い手では背景の反射が混ざって
// ときどき 1 回だけ背景の距離が出るので、1 回で切ると音がブツブツ切れる
const uint8_t MISS_LIMIT_L1X = 1;
const uint8_t MISS_LIMIT_L4CD = 4;
HandFilter pitchFilter = {MISS_LIMIT_L1X, -1, -1, 0, 0};
HandFilter volFilter = {MISS_LIMIT_L1X, -1, -1, 0, 0};
uint32_t pitchCuts = 0;  // 音程側で「手なし」になった回数（診断用）

bool pitchOn = false;
float volGain = 0.0f;

// 手ぶれ・測定のばらつき対策（One Euro フィルタ。音程側・音量側の両方）。手が止まっているときは強くならし（遅れ大）、
// 速く動かすほどならしを弱める（遅れ小）。カットオフ = HAND_SMOOTH_MIN_HZ + beta × (速さ(cm/s) − HAND_SMOOTH_DEAD_CMS)
// beta はブラウザの「追従の速さ」（pitchBeta / volBeta）。例: 0.5 なら 50cm/s で 21Hz
const float HAND_SMOOTH_MIN_HZ = 1.0f;   // 止まっているときのカットオフ
const float HAND_SMOOTH_D_HZ = 1.0f;     // 速さの推定に使うローパス
// 速さの推定にもノイズが乗る（遠い手で 5cm/s 前後）。これ以下の速さは「止まっている」とみなし、ならしを弱めない
const float HAND_SMOOTH_DEAD_CMS = 10.0f;
const bool HAND_SMOOTH_ON = true;        // false でフィルタを通さない（センサーの値をそのまま使う。比較用）
struct OneEuro {
  bool init;
  float x, dx;
  uint32_t lastUs;
  float lpK(float fc, float dt) { return 1.0f - expf(-2.0f * 3.14159265f * fc * dt); }
  float update(float v, float beta) {
    if (!HAND_SMOOTH_ON) return x = v;
    uint32_t nowUs = micros();
    if (!init) {
      init = true;
      x = v;
      dx = 0;
      lastUs = nowUs;
      return v;
    }
    float dt = constrain((nowUs - lastUs) * 1e-6f, 0.001f, 0.1f);
    lastUs = nowUs;
    dx += ((v - x) / dt - dx) * lpK(HAND_SMOOTH_D_HZ, dt);
    x += (v - x) * lpK(HAND_SMOOTH_MIN_HZ + beta * fmaxf(fabsf(dx) - HAND_SMOOTH_DEAD_CMS, 0.0f), dt);
    return x;
  }
};
OneEuro pitchSmooth = {false, 0, 0, 0};
OneEuro volSmooth = {false, 0, 0, 0};

// 診断用: 音程側の距離のばらつき（50 回分の標準偏差、mm）
volatile float pitchJitterMm = 0.0f;
void trackJitter(float cm) {
  static float sum = 0, sum2 = 0;
  static int n = 0;
  sum += cm;
  sum2 += cm * cm;
  if (++n == 50) {
    float m = sum / n;
    pitchJitterMm = sqrtf(fmaxf(sum2 / n - m * m, 0.0f)) * 10.0f;
    sum = sum2 = 0;
    n = 0;
  }
}

void handlePitch(float cm) {
  const Params &p = prm;
  float d;
  int8_t r = pitchFilter.update(cm, p.maxCm + p.holdCm, &d);
  if (r > 0) {
    trackJitter(d);
    d = pitchSmooth.update(d, p.pitchBeta);
    float t = (constrain(d, p.minCm, p.maxCm) - p.minCm) / (p.maxCm - p.minCm);  // 0=近い 1=遠い
    float hz = p.lowHz * powf(2.0f, p.octaves * (1.0f - t));
    targetHz = hz;
    targetOct = log2f(hz);
    pitchOn = true;
  } else if (r < 0) {
    if (pitchOn) pitchCuts++;
    pitchOn = false;  // 周波数は維持したまま絞る
    pitchSmooth.init = false;  // 次に手を入れたときは即その位置から
    releaseK = releaseCutK;
  }
}

void handleVolume(float cm) {
  const Params &p = prm;
  float d;
  int8_t r = volFilter.update(cm, p.volMaxCm, &d);
  if (r > 0) {
    d = volSmooth.update(d, p.volBeta);
    // u: 近さ（volMinCm で 1、volMaxCm で 0）。dB で直線にしてから、u=0 でちょうど 0 になるよう底上げ分を引く
    float u = (p.volMaxCm - constrain(d, p.volMinCm, p.volMaxCm)) / (p.volMaxCm - p.volMinCm);
    float floorG = powf(10.0f, -p.volDbRange / 20.0f);
    float g = powf(10.0f, -p.volDbRange * (1.0f - u) / 20.0f);
    volGain = (g - floorG) / (1.0f - floorG);
  } else if (r < 0) {
    // 直前の音量が大きいまま範囲外になった = 一気に払った → 余韻。徐々に遠ざけた → すぐ切る。
    // 判定は手が外れた瞬間の 1 回だけ（手がない間も測定のたびにここへ来るため）
    if (volGain > 0.0f) {
      float abruptGain = powf(10.0f, p.volAbruptDb / 20.0f);
      releaseK = volGain > abruptGain ? releaseAbruptK : releaseCutK;
    }
    volGain = 0.0f;  // 手がなければ無音
    volSmooth.init = false;
  }
}

// ---- ToF センサー（VL53L1X / VL53L4CD 自動判別） ----
// 起動直後のアドレス 0x29 でチップ ID（レジスタ 0x010F）を読んで種類を決める。
// VL53L4CD は測定時間 10ms で連続測定（約 100 回/秒）、VL53L1X は Short モード 20ms。

const uint16_t ID_VL53L1X = 0xEACC;
const uint16_t ID_VL53L4CD = 0xEBAA;
const uint32_t L4CD_PERIOD_MS = 10;  // VL53L4CD の測定時間（10〜200ms。最短）。20ms にしても遠い手のばらつきは減らなかった

struct Tof {
  VL53L1X dev;
  bool ok;
  uint16_t id;
  uint8_t addr;  // VL53L4CD の 7 ビットアドレス
  uint32_t periodMs;
  uint32_t lastMs;
  uint32_t reads, valid;
  float cm;
  uint8_t status;
  uint8_t failStep;  // 初期化で失敗した段階（診断用）
  uint16_t sigKcps, sigmaMm, rawMm;  // VL53L4CD の信号の強さ・ばらつき推定・状態によらない距離（診断用）
  TwoWire *bus;  // このセンサーがつながっている I2C
};

Tof tofPitch, tofVol;
bool rawLog = false;

// ---- I2C の読み書き（16 ビットのレジスタ番号。失敗したら数回やり直す） ----
// 読み書きの相手は i2c が指す I2C。センサーを触る前に i2c = t.bus にする（センサーの処理は Core1 だけなので競合しない）
TwoWire *i2c = &tofBus;
// 電源を入れた直後の最初の通信が失敗することがあるのでやり直す。
// ST の VL53L4CD ライブラリは失敗すると永久にやり直し続け、そこで止まってしまうので使わない
bool i2cWrite(uint8_t addr, uint16_t reg, const uint8_t *buf, uint8_t n) {
  for (int tryN = 0; tryN < 3; tryN++) {
    i2c->beginTransmission(addr);
    i2c->write((uint8_t)(reg >> 8));
    i2c->write((uint8_t)(reg & 0xFF));
    i2c->write(buf, n);
    if (i2c->endTransmission() == 0) return true;
  }
  return false;
}

bool i2cRead(uint8_t addr, uint16_t reg, uint8_t *buf, uint8_t n) {
  for (int tryN = 0; tryN < 3; tryN++) {
    i2c->beginTransmission(addr);
    i2c->write((uint8_t)(reg >> 8));
    i2c->write((uint8_t)(reg & 0xFF));
    if (i2c->endTransmission() != 0) continue;
    if (i2c->requestFrom(addr, n) != n) continue;
    for (int i = 0; i < n; i++) buf[i] = i2c->read();
    return true;
  }
  return false;
}

bool i2cWrite8(uint8_t addr, uint16_t reg, uint8_t v) { return i2cWrite(addr, reg, &v, 1); }
bool i2cWrite16(uint8_t addr, uint16_t reg, uint16_t v) {
  uint8_t b[2] = {(uint8_t)(v >> 8), (uint8_t)v};
  return i2cWrite(addr, reg, b, 2);
}
bool i2cWrite32(uint8_t addr, uint16_t reg, uint32_t v) {
  uint8_t b[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v};
  return i2cWrite(addr, reg, b, 4);
}
// 16 ビットのレジスタを読む（失敗したら 0）
uint16_t i2cRead16(uint8_t addr, uint16_t reg) {
  uint8_t b[2];
  if (!i2cRead(addr, reg, b, 2)) return 0;
  return (uint16_t)b[0] << 8 | b[1];
}

// ---- VL53L4CD（ST の ULD ドライバ 1.0 の手順をそのまま移したもの） ----

const uint8_t L4CD_DEFAULT_CONFIG[] = {  // レジスタ 0x2D〜0x87 の初期値
  0x12, 0x00, 0x00, 0x11, 0x02, 0x00, 0x02, 0x08, 0x00, 0x08, 0x10, 0x01, 0x01, 0x00, 0x00, 0x00,
  0x00, 0xff, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x0b, 0x00, 0x00, 0x02, 0x14, 0x21,
  0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x00, 0x00, 0x38, 0xff, 0x01, 0x00, 0x08, 0x00,
  0x00, 0x01, 0xcc, 0x07, 0x01, 0xf1, 0x05, 0x00, 0xa0, 0x00, 0x80, 0x08, 0x38, 0x00, 0x00, 0x00,
  0x00, 0x0f, 0x89, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x07, 0x05, 0x06, 0x06, 0x00,
  0x00, 0x02, 0xc7, 0xff, 0x9B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
};

// 測定結果ができていれば true
bool l4DataReady(uint8_t addr, bool *ready) {
  uint8_t mux, st;
  if (!i2cRead(addr, 0x0030, &mux, 1) || !i2cRead(addr, 0x0031, &st, 1)) return false;
  uint8_t pol = (mux & 0x10) ? 0 : 1;
  *ready = (st & 1) == pol;
  return true;
}

bool l4WaitReady(uint8_t addr, uint32_t timeoutMs) {
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    bool r;
    if (l4DataReady(addr, &r) && r) return true;
    delay(1);
  }
  return false;
}

// 測定時間（ms）を設定し、連続測定にする
bool l4SetTiming(uint8_t addr, uint32_t budgetMs) {
  uint16_t osc = i2cRead16(addr, 0x0006);
  if (osc == 0) return false;
  uint32_t macroUs = (2304u * (0x40000000u / osc)) >> 6;
  uint32_t budgetUs = (budgetMs * 1000u - 2500u) << 12;  // 連続測定のときは 2.5ms 引く
  if (!i2cWrite32(addr, 0x006C, 0)) return false;  // 測定の間隔 0 = 連続測定
  const uint32_t mul[2] = {16, 12};
  const uint16_t reg[2] = {0x005E, 0x0061};  // RANGE_CONFIG_A / B
  for (int k = 0; k < 2; k++) {
    uint32_t tmp = (macroUs * mul[k]) >> 6;
    uint32_t ls = (budgetUs + (tmp >> 1)) / tmp - 1;
    uint16_t ms = 0;
    while (ls & 0xFFFFFF00u) {
      ls >>= 1;
      ms++;
    }
    if (!i2cWrite16(addr, reg[k], (uint16_t)(ms << 8) + (ls & 0xFF))) return false;
  }
  return true;
}

// アドレス 0x29 で起動したばかりの VL53L4CD を初期化して、newAddr で連続測定を始める
bool startL4CD(Tof &t, uint8_t newAddr) {
  t.addr = 0x29;
  if (newAddr != 0x29) {
    i2cWrite8(0x29, 0x0001, newAddr);  // 書き込み後は新しいアドレスで返事をするので、成否は新アドレスで確かめる
    if (i2cRead16(newAddr, 0x010F) != ID_VL53L4CD) { t.failStep = 1; return false; }
    t.addr = newAddr;
  }
  uint8_t a = t.addr, v = 0;
  uint32_t t0 = millis();
  while (!(i2cRead(a, 0x00E5, &v, 1) && v == 3)) {  // ファームウェアの起動待ち
    if (millis() - t0 > 200) { t.failStep = 2; return false; }
    delay(1);
  }
  for (uint8_t r = 0x2D; r <= 0x87; r++)
    if (!i2cWrite8(a, r, L4CD_DEFAULT_CONFIG[r - 0x2D])) { t.failStep = 3; return false; }
  // VHV（温度補正）を 1 回走らせる
  if (!i2cWrite8(a, 0x0087, 0x40) || !l4WaitReady(a, 200)) { t.failStep = 4; return false; }
  i2cWrite8(a, 0x0086, 0x01);  // 割り込みクリア
  i2cWrite8(a, 0x0087, 0x00);  // 停止
  i2cWrite8(a, 0x0008, 0x09);  // VHV_CONFIG__TIMEOUT_MACROP_LOOP_BOUND
  i2cWrite8(a, 0x000B, 0x00);
  i2cWrite16(a, 0x0024, 0x0500);
  if (!l4SetTiming(a, L4CD_PERIOD_MS)) { t.failStep = 5; return false; }
  // 連続測定を開始（最初の結果を 1 回捨てる）
  if (!i2cWrite8(a, 0x0087, 0x21) || !l4WaitReady(a, 200)) { t.failStep = 6; return false; }
  i2cWrite8(a, 0x0086, 0x01);
  t.periodMs = L4CD_PERIOD_MS;
  return true;
}

// 測定結果を読む。range_status は ST のドライバと同じ番号に直す（0 = 正常）
bool l4Read(uint8_t addr, uint8_t *status, uint16_t *mm, uint16_t *sigKcps, uint16_t *sigmaMm) {
  static const uint8_t statusMap[24] = {255, 255, 255, 5, 2, 4, 1, 7, 3, 0, 255, 255,
                                        9, 13, 255, 255, 255, 255, 10, 6, 255, 255, 11, 12};
  uint8_t b[15];  // 0x89〜0x97
  if (!i2cRead(addr, 0x0089, b, sizeof(b))) return false;
  uint8_t raw = b[0] & 0x1F;
  uint8_t st = raw < 24 ? statusMap[raw] : raw;
  uint16_t spads = ((uint16_t)b[0x8C - 0x89] << 8 | b[0x8D - 0x89]) / 256;
  if (spads == 0) st = 255;
  *status = st;
  *mm = (uint16_t)b[0x96 - 0x89] << 8 | b[0x97 - 0x89];
  *sigKcps = ((uint16_t)b[0x8E - 0x89] << 8 | b[0x8F - 0x89]) * 8;
  *sigmaMm = ((uint16_t)b[0x92 - 0x89] << 8 | b[0x93 - 0x89]) / 4;
  i2cWrite8(addr, 0x0086, 0x01);  // 割り込みクリア（次の測定結果を受け付ける）
  return true;
}

// init() は内部でソフトリセットをかけ、I2C アドレスも 0x29 に戻る。アドレス変更は必ず init() のあと
bool startL1X(Tof &t, uint8_t newAddr) {
  t.dev.setBus(t.bus);
  t.dev.setAddress(0x29);
  t.dev.setTimeout(100);
  if (!t.dev.init()) return false;
  if (newAddr != 0x29) t.dev.setAddress(newAddr);
  t.dev.setDistanceMode(VL53L1X::Short);
  t.dev.setROISize(TOF_ROI, TOF_ROI);
  t.dev.setMeasurementTimingBudget(TOF_PERIOD_MS * 1000);
  t.dev.startContinuous(TOF_PERIOD_MS);
  t.periodMs = TOF_PERIOD_MS;
  return true;
}

bool tofStart(Tof &t, uint8_t newAddr) {
  i2c = t.bus;
  t.id = i2cRead16(0x29, 0x010F);
  if (t.id == ID_VL53L4CD) return startL4CD(t, newAddr);
  if (t.id == ID_VL53L1X) return startL1X(t, newAddr);
  return false;
}

// I2C バスの固着を解く（SCL を 9 回叩いてから STOP）
void i2cRecover(TwoWire &w, uint8_t sda, uint8_t scl) {
  w.end();
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);
  for (int i = 0; i < 9; i++) {
    digitalWrite(scl, LOW);
    pinMode(scl, OUTPUT);
    delayMicroseconds(10);
    pinMode(scl, INPUT_PULLUP);
    delayMicroseconds(10);
  }
  digitalWrite(sda, LOW);
  pinMode(sda, OUTPUT);
  delayMicroseconds(10);
  pinMode(sda, INPUT_PULLUP);
  delayMicroseconds(10);
  w.setSDA(sda);
  w.setSCL(scl);
  w.begin();
  w.setClock(100000);
  w.setTimeout(25, true);
}

bool i2cAck(uint8_t addr) {
  i2c->beginTransmission(addr);
  i2c->write((uint8_t)0x00);  // arduino-pico の 0 バイト送信はソフト実装で当てにならないので 1 バイト送る
  i2c->write((uint8_t)0x01);
  return i2c->endTransmission() == 0;
}

bool i2cAckOn(TwoWire &w, uint8_t addr) {
  i2c = &w;
  return i2cAck(addr);
}

// 起動したセンサーが 0x29 で返事をするまで待つ。音量側は I2C1（専用）と I2C0（共有）の両方を見て、
// 返事があった方を返す（どちらからも返事がなければ I2C0。初期化が失敗して FAIL と表示される）
TwoWire *waitBoot(bool tryVolBus, uint32_t timeoutMs) {
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    TwoWire *found = (tryVolBus && i2cAckOn(volBus, 0x29)) ? &volBus
                     : i2cAckOn(tofBus, 0x29)                  ? &tofBus
                                                               : nullptr;
    if (found) {
      delay(20);
      return found;
    }
    delay(2);
  }
  return &tofBus;
}

// 2 台ともリセットして起動し直す。音程側は I2C0 で TOF_ADDR_PITCH に変える。
// 音量側は I2C1（なければ I2C0 で音程側と共有）の 0x29
void tofInitAll() {
  digitalWrite(PIN_XSHUT_PITCH, LOW);
  digitalWrite(PIN_XSHUT_VOL, LOW);
  i2cRecover(volBus, PIN_VOL_SDA, PIN_VOL_SCL);
  i2cRecover(tofBus, PIN_TOF_SDA, PIN_TOF_SCL);
  uint32_t t0 = millis();
  delay(10);
  while ((i2cAckOn(tofBus, 0x29) || i2cAckOn(tofBus, TOF_ADDR_PITCH) || i2cAckOn(volBus, 0x29)) &&
         millis() - t0 < 200)
    delay(5);

  digitalWrite(PIN_XSHUT_PITCH, HIGH);
  tofPitch.bus = &tofBus;
  waitBoot(false, 300);
  tofPitch.ok = tofStart(tofPitch, TOF_ADDR_PITCH);

  digitalWrite(PIN_XSHUT_VOL, HIGH);
  tofVol.bus = waitBoot(true, 300);
  tofVol.ok = tofStart(tofVol, 0x29);

  pitchFilter.missLimit = tofPitch.id == ID_VL53L4CD ? MISS_LIMIT_L4CD : MISS_LIMIT_L1X;
  volFilter.missLimit = tofVol.id == ID_VL53L4CD ? MISS_LIMIT_L4CD : MISS_LIMIT_L1X;
  tofPitch.lastMs = tofVol.lastMs = millis();
}

// 測定完了していれば読む。読めたら距離（無効なら 1e6）を返し、読めなければ負の値を返す
float tofPoll(Tof &t, uint32_t nowMs) {
  if (nowMs - t.lastMs < t.periodMs - 2) return -1.0f;
  i2c = t.bus;
  float cm = 1e6f;
  if (t.id == ID_VL53L4CD) {
    bool ready;
    if (!l4DataReady(t.addr, &ready) || !ready) return -1.0f;
    uint16_t mm;
    if (!l4Read(t.addr, &t.status, &mm, &t.sigKcps, &t.sigmaMm)) return -1.0f;
    t.rawMm = mm;
    // 0 = 正常。1（ばらつき大）と 2（反射が弱い）は警告扱いで距離は使える（遠い手でこうなる）
    if (t.status <= 2) cm = mm / 10.0f;
  } else {
    if (!t.dev.dataReady()) return -1.0f;
    t.dev.read(false);
    t.status = t.dev.ranging_data.range_status;
    // WrapTargetFail も採用する（遠い手では背景の反射が混ざってこの状態になるが、距離は手に追従している）
    VL53L1X::RangeStatus st = t.dev.ranging_data.range_status;
    if (st == VL53L1X::RangeValid || st == VL53L1X::WrapTargetFail) cm = t.dev.ranging_data.range_mm / 10.0f;
  }
  t.lastMs = nowMs;
  t.reads++;
  if (cm < 1000) t.valid++;
  t.cm = cm;
  return cm;
}

// ブラウザで作った比率を合計 100% にそろえて使う（全部 0 なら無音）。本体のつまみもここで読む
// （位置は knobPos に入れ、Wi-Fi の切り替えは Core0 の wifiKnobPoll が行う）
void updateMix() {
  static float pot = -1.0f;
  float v = constrain(analogRead(PIN_KNOB) / 4095.0f, 0.0f, 1.0f);
  pot = pot < 0 ? v : pot + (v - pot) * 0.2f;  // ADC のノイズをならす
  knobPos = pot;

  float sum = prm.mix[0] + prm.mix[1] + prm.mix[2];
  float scale = sum > 0.02f ? 1.0f / sum : 0.0f;
  for (int i = 0; i < 3; i++) targetMix[i] = prm.mix[i] * scale;
  ledWanted = scale > 0.0f;
}

// 音の PWM と割り込みを始める。Core1 から呼ぶので、割り込みは Core1 で処理される
void audioStart() {
  gpio_set_function(PIN_AUDIO, GPIO_FUNC_PWM);
  pwmSlice = pwm_gpio_to_slice_num(PIN_AUDIO);
  pwm_config c = pwm_get_default_config();
  pwm_config_set_clkdiv(&c, 1.0f);
  pwm_config_set_wrap(&c, PWM_TOP);
  pwm_init(pwmSlice, &c, false);
  pwm_set_gpio_level(PIN_AUDIO, PWM_TOP / 2);
  pwm_clear_irq(pwmSlice);
  pwm_set_irq_enabled(pwmSlice, true);
  irq_set_exclusive_handler(PWM_DEFAULT_IRQ_NUM(), audioISR);
  irq_set_priority(PWM_DEFAULT_IRQ_NUM(), PICO_HIGHEST_IRQ_PRIORITY);
  irq_set_enabled(PWM_DEFAULT_IRQ_NUM(), true);
  pwm_set_enabled(pwmSlice, true);
  Serial.print("PWM audio started, sample rate = ");
  Serial.println(sampleRate, 1);
}

void setup1() {
  // 浮動小数点の非正規化数を 0 として扱う（リバーブの余韻が消えかけの極小値で計算が遅くならないように）
  {
    uint32_t fpscr;
    asm volatile("vmrs %0, fpscr" : "=r"(fpscr));
    fpscr |= (1u << 24);  // FZ ビット
    asm volatile("vmsr fpscr, %0" : : "r"(fpscr));
  }
  while (!coreReady) delay(1);
  audioStart();
  analogReadResolution(12);
  pinMode(PIN_XSHUT_PITCH, OUTPUT);
  pinMode(PIN_XSHUT_VOL, OUTPUT);
  tofInitAll();
}

void loop1() {
  static uint32_t lastMixMs = 0;
  static uint32_t lastRetryMs = 0;
  uint32_t nowMs = millis();

  if (nowMs - lastMixMs >= 10) {
    lastMixMs = nowMs;
    updateMix();
  }

  // 診断: シリアルで 'r' を送ると音程センサーの生の値を毎回出す（もう一度 'r' で止める）
  while (Serial.available()) {
    if (Serial.read() == 'r') rawLog = !rawLog;
  }

  if (!tofPitch.ok || !tofVol.ok) {
    if (nowMs - lastRetryMs >= 1000) {
      lastRetryMs = nowMs;
      tofInitAll();
    }
  } else {
    float cm = tofPoll(tofPitch, nowMs);
    if (cm >= 0) {
      liveCm[0] = cm;
      handlePitch(cm);
      if (rawLog) {
        Serial.printf("R %lu %.1f raw=%u st=%u sig=%u sigma=%u smooth=%.1f hz=%.1f on=%d\n", (unsigned long)nowMs,
                      cm < 1000 ? cm : -1.0f, tofPitch.rawMm, tofPitch.status, tofPitch.sigKcps, tofPitch.sigmaMm, pitchSmooth.x,
                      (float)targetHz, pitchOn);
      }
    }
    cm = tofPoll(tofVol, nowMs);
    if (cm >= 0) {
      liveCm[1] = cm;
      handleVolume(cm);
      if (rawLog) {
        Serial.printf("V %lu %.1f raw=%u st=%u sig=%u sigma=%u\n", (unsigned long)nowMs, cm < 1000 ? cm : -1.0f,
                      tofVol.rawMm, tofVol.status, tofVol.sigKcps, tofVol.sigmaMm);
      }
    }
    targetAmp = pitchOn ? volGain : 0.0f;
    if (nowMs - tofPitch.lastMs >= 500) tofPitch.ok = false;
    if (nowMs - tofVol.lastMs >= 500) tofVol.ok = false;
  }

}


// 診断出力（0.5秒ごと）。Core0 の loop から呼ぶ。シリアル出力（USB）は割り込みを短時間止めるので、
// 音の割り込みがある Core1 では出さない（Core1 で出すと音の割り込みが最大 50µs 遅れた。実測）
void printReport() {
  static uint32_t lastReportMs = 0;
  uint32_t nowMs = millis();
  if (nowMs - lastReportMs >= 500) {
    lastReportMs = nowMs;
    Tof *ts[2] = {&tofPitch, &tofVol};
    for (int i = 0; i < 2; i++) {
      Serial.print(i == 0 ? "P(" : "  V(");
      Serial.print(ts[i]->ok ? "OK" : "FAIL");
      Serial.print(ts[i]->id == ID_VL53L4CD ? ",4CD" : ts[i]->id == ID_VL53L1X ? ",L1X" : ",?");
      Serial.print(ts[i]->bus == &Wire ? ",I2C0" : ",I2C1");
      if (!ts[i]->ok) { Serial.print(",step"); Serial.print(ts[i]->failStep); }
      if (ts[i]->id != ID_VL53L4CD && ts[i]->id != ID_VL53L1X) Serial.print(ts[i]->id, HEX);
      Serial.print("):");
      Serial.print(ts[i]->reads);
      Serial.print("/");
      Serial.print(ts[i]->valid);
      Serial.print(" ");
      if (ts[i]->cm < 1000) {
        Serial.print(ts[i]->cm, 1);
        Serial.print("cm");
      } else {
        Serial.print("-- st=");
        Serial.print(ts[i]->status);
      }
    }
    Serial.print("  cut=");
    Serial.print(pitchCuts);
    Serial.print(" jit=");
    Serial.print(pitchJitterMm, 1);
    Serial.print("mm");
    Serial.print("  mix=");
    for (int i = 0; i < 3; i++) {
      Serial.print((int)(targetMix[i] * 100.0f + 0.5f));
      Serial.print(i < 2 ? "/" : "%");
    }
    Serial.print("  pot=");
    Serial.print((int)(knobPos * 100.0f + 0.5f));
    Serial.print("%");
    Serial.print("  amp=");
    Serial.print(targetAmp, 2);
    Serial.print("  hz=");
    Serial.print(targetHz, 1);
    Serial.print("  ");
    Serial.print(wifiSsid);
    if (wifiState == WIFI_ST_OFF) {
      Serial.print(" wifi=off");
    } else {
      Serial.print(wifiState == WIFI_ST_OPEN ? " wifi=open" : " wifi=on");
      Serial.print(" clients=");
      Serial.print(WiFi.softAPgetStationNum());
    }
    Serial.print("  isrGapMax=");
    Serial.print(isrGapMaxUs);
    Serial.print("us late=");
    Serial.print(isrGapLate);
    Serial.print(" isrDur=");
    Serial.print(isrDurMaxUs);
    Serial.println("us");
    isrGapMaxUs = 0;
    isrGapLate = 0;
    isrDurMaxUs = 0;
  }
}

// ================= 操作画面 =================

const char INDEX_HTML[] = R"HTML(<!doctype html>
<html lang="ja"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Digimin</title>
<style>
:root{--bg:#f5f5f2;--card:#fff;--fg:#1d1d1b;--sub:#6b6b66;--acc:#c2410c;--line:#e2e2dc}
@media (prefers-color-scheme:dark){:root{--bg:#161614;--card:#22221f;--fg:#ededE8;--sub:#9a9a92;--acc:#fb923c;--line:#34342f}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,-apple-system,sans-serif}
header{padding:16px 16px 4px;display:flex;align-items:flex-start;gap:8px;max-width:640px;margin:0 auto}
header div{flex:1}h1{margin:0;font-size:22px}header p{margin:2px 0 0;color:var(--sub);font-size:13px}
main{padding:8px 12px 40px;max-width:640px;margin:0 auto}
section{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:12px 14px;margin:10px 0}
h2{font-size:15px;margin:0 0 8px}h3.sub{font-size:13px;margin:12px 0 0;color:var(--sub);font-weight:600}
.row{display:grid;grid-template-columns:7.5em 1fr 4.2em;gap:8px;align-items:center;margin:8px 0}
.row label{font-size:14px}.row output{text-align:right;font-variant-numeric:tabular-nums;color:var(--sub);font-size:13px}
input[type=range]{width:100%;accent-color:var(--acc)}
select,input[type=text]{font:inherit;padding:6px 8px;border:1px solid var(--line);border-radius:8px;background:var(--bg);color:var(--fg)}
.live{display:grid;grid-template-columns:repeat(2,1fr);gap:8px}
.live div{background:var(--bg);border-radius:8px;padding:8px 10px}
.live b{display:block;font-size:20px;font-variant-numeric:tabular-nums}.live span{color:var(--sub);font-size:12px}
.bar{height:6px;background:var(--line);border-radius:3px;margin-top:6px;overflow:hidden}.bar i{display:block;height:100%;background:var(--acc);width:0}
.presets{display:grid;gap:6px}.builtins{display:grid;grid-template-columns:repeat(auto-fill,minmax(8.5em,1fr));gap:6px}
.builtins button{padding:9px 6px}.preset{display:flex;gap:6px;align-items:center}
.preset .name{flex:1;font-size:14px}section.slim{padding:6px 14px}section.slim .name{font-size:12px;color:var(--sub)}section.slim button{font-size:12px;padding:4px 8px}.preset .empty{color:var(--sub)}
button{font:inherit;font-size:13px;padding:6px 10px;border-radius:8px;border:1px solid var(--line);background:var(--bg);color:var(--fg)}
button.acc{background:var(--acc);border-color:var(--acc);color:#fff}
button:disabled{opacity:.35;cursor:not-allowed}
.note{color:var(--sub);font-size:12px;margin:4px 0 0}
.dlg{position:fixed;inset:0;background:rgba(0,0,0,.45);display:flex;align-items:center;justify-content:center;padding:16px;z-index:10}
.dlg[hidden]{display:none}.dlg .box{background:var(--card);border-radius:12px;padding:16px;max-width:360px;width:100%}
.dlg p{margin:0 0 12px}.dlg p.note{margin:-4px 0 12px}.dlg input{width:100%;margin:0 0 12px}.dlg .btns{display:flex;gap:8px;justify-content:flex-end}
#msg{position:fixed;left:50%;bottom:16px;transform:translateX(-50%);background:var(--fg);color:var(--bg);padding:8px 14px;border-radius:8px;font-size:13px;opacity:0;transition:opacity .2s}
</style></head><body>
<header><div><h1>Digimin</h1><p><span data-i18n="subtitle"></span> <span id="ssid"></span></p></div><button id="langBtn" onclick="toggleLang()"></button></header>
<main>
<section class="slim"><div class="preset"><span class="name">Wi-Fi · <span id="wifiState">--</span></span>
 <button onclick="wifiSave()" data-i18n="wifiChange"></button></div></section>

<section><h2 data-i18n="secLive"></h2>
<div class="live">
 <div><span data-i18n="livePitchHand"></span><b id="lp">--</b><div class="bar"><i id="bp"></i></div></div>
 <div><span data-i18n="liveVolHand"></span><b id="lv">--</b><div class="bar"><i id="bv"></i></div></div>
 <div><span data-i18n="livePitch"></span><b id="lh">--</b></div>
 <div><span data-i18n="liveVol"></span><b id="la">--</b></div>
</div></section>

<section><h2 data-i18n="secTone"></h2>
<div class="row"><label data-i18n="sine"></label><input type="range" id="sine" min="0" max="100" step="1"><output></output></div>
<div class="row"><label data-i18n="tri"></label><input type="range" id="tri" min="0" max="100" step="1"><output></output></div>
<div class="row"><label data-i18n="saw"></label><input type="range" id="saw" min="0" max="100" step="1"><output></output></div>
<p class="note" id="knobNote"></p>
</section>

<section><h2 data-i18n="secPitch"></h2>
<div class="row"><label data-i18n="lowNote"></label><select id="lowNote"></select><output></output></div>
<div class="row"><label data-i18n="octaves"></label><input type="range" id="octaves" min="1" max="4" step="0.5"><output></output></div>
<div class="row"><label data-i18n="glide"></label><input type="range" id="glideMs" min="2" max="150" step="1"><output></output></div>
<div class="row"><label data-i18n="follow"></label><input type="range" id="pitchBeta" min="0" max="2" step="0.1"><output></output></div>
<p class="note" data-i18n="followNote"></p>
</section>

<section><h2 data-i18n="secVol"></h2>
<div class="row"><label data-i18n="volDbRange"></label><input type="range" id="volDbRange" min="10" max="60" step="1"><output></output></div>
<div class="row"><label data-i18n="releaseAbrupt"></label><input type="range" id="releaseAbruptMs" min="1" max="500" step="1"><output></output></div>
<div class="row"><label data-i18n="volAbruptDb"></label><input type="range" id="volAbruptDb" min="-50" max="-5" step="1"><output></output></div>
<div class="row"><label data-i18n="follow"></label><input type="range" id="volBeta" min="0" max="2" step="0.1"><output></output></div>
<p class="note" data-i18n="volNote"></p>
</section>

<section><h2 data-i18n="secDist"></h2>
<div class="row"><label data-i18n="minCm"></label><input type="range" id="minCm" min="2" max="30" step="1"><output></output></div>
<div class="row"><label data-i18n="maxCm"></label><input type="range" id="maxCm" min="20" max="100" step="1"><output></output></div>
<div class="row"><label data-i18n="holdCm"></label><input type="range" id="holdCm" min="0" max="20" step="1"><output></output></div>
<div class="row"><label data-i18n="volMinCm"></label><input type="range" id="volMinCm" min="2" max="30" step="1"><output></output></div>
<div class="row"><label data-i18n="volMaxCm"></label><input type="range" id="volMaxCm" min="20" max="100" step="1"><output></output></div>
<p class="note" data-i18n="distNote"></p>
</section>

<section><h2 data-i18n="secFx"></h2>
<h3 class="sub" data-i18n="harmony"></h3>
<div class="row"><label data-i18n="amount"></label><input type="range" id="harmonyMix" min="0" max="100" step="1"><output></output></div>
<div class="row"><label data-i18n="interval"></label><select id="harmonySemis"></select><output></output></div>
<h3 class="sub" data-i18n="vibrato"></h3>
<div class="row"><label data-i18n="depth"></label><input type="range" id="vibCents" min="0" max="100" step="1"><output></output></div>
<div class="row"><label data-i18n="rate"></label><input type="range" id="vibRateHz" min="2" max="10" step="0.1"><output></output></div>
<h3 class="sub" data-i18n="overdrive"></h3>
<div class="row"><label data-i18n="drive"></label><input type="range" id="odDrive" min="0" max="100" step="1"><output></output></div>
<div class="row"><label data-i18n="odTone"></label><input type="range" id="odTone" min="0" max="100" step="1"><output></output></div>
<h3 class="sub" data-i18n="leslie"></h3>
<div class="row"><label data-i18n="amount"></label><input type="range" id="leslieMix" min="0" max="100" step="1"><output></output></div>
<div class="row"><label data-i18n="speed"></label><button id="leslieBtn" onclick="toggleLeslie()"></button><output></output></div>
<h3 class="sub" data-i18n="chorus"></h3>
<div class="row"><label data-i18n="amount"></label><input type="range" id="chorusMix" min="0" max="100" step="1"><output></output></div>
<div class="row"><label data-i18n="rate"></label><input type="range" id="chorusRateHz" min="0.1" max="3" step="0.05"><output></output></div>
<div class="row"><label data-i18n="depth"></label><input type="range" id="chorusDepthMs" min="0.5" max="8" step="0.1"><output></output></div>
<h3 class="sub" data-i18n="delay"></h3>
<div class="row"><label data-i18n="amount"></label><input type="range" id="delayMix" min="0" max="100" step="1"><output></output></div>
<div class="row"><label data-i18n="time"></label><input type="range" id="delayMs" min="50" max="1000" step="10"><output></output></div>
<div class="row"><label data-i18n="feedback"></label><input type="range" id="delayFeedback" min="0" max="90" step="1"><output></output></div>
<h3 class="sub" data-i18n="reverb"></h3>
<div class="row"><label data-i18n="amount"></label><input type="range" id="reverbMix" min="0" max="100" step="1"><output></output></div>
<div class="row"><label data-i18n="size"></label><input type="range" id="reverbSize" min="0" max="100" step="1"><output></output></div>
<div class="row"><label data-i18n="damp"></label><input type="range" id="reverbDamp" min="0" max="100" step="1"><output></output></div>
<p class="note" data-i18n="fxNote"></p>
</section>

<section><h2 data-i18n="secBuiltin"></h2>
<div class="builtins" id="builtins"></div>
<p class="note" data-i18n="builtinNote"></p>
</section>

<section><h2 data-i18n="secPresets"></h2>
<p style="margin:0 0 10px"><button onclick="resetAll()" data-i18n="reset"></button></p>
<h3 class="sub" data-i18n="secBoot"></h3>
<div class="preset"><span class="name" id="bootName">--</span>
 <button onclick="bootLoad()" id="bootLoadBtn" data-i18n="load"></button>
 <button class="acc" onclick="bootSave()" data-i18n="saveCurrent"></button>
 <button onclick="bootClear()" id="bootClearBtn" data-i18n="clear"></button></div>
<p class="note" data-i18n="bootNote"></p>
<h3 class="sub" data-i18n="namedPresets"></h3>
<div class="presets" id="presets"></div>
<p class="note" data-i18n="presetsNote"></p>
</section>
</main>
<div id="msg"></div>
<div id="dlg" class="dlg" hidden><div class="box"><p id="dlgMsg"></p><input id="dlgIn" type="text"><p class="note" id="dlgNote" hidden></p>
<div class="btns"><button id="dlgCancel"></button><button id="dlgOk" class="acc"></button></div></div></div>
<script>
// ---- 言語 ----
const I18N={
ja:{subtitle:'Digimin の操作パネル',langBtn:'English',
 secLive:'いまの状態',livePitchHand:'音程の手',liveVolHand:'音量の手',livePitch:'音程',liveVol:'音量',silent:'無音',
 secTone:'音色',sine:'サイン波',tri:'三角波',saw:'ノコギリ波',
 toneNote:'比率で混ぜます（合計は常に 100%）。',
 secPitch:'音程',lowNote:'最低音',octaves:'音域',glide:'グライド',follow:'追従の速さ',
 followNote:'手を止めたときの音程の揺れ（センサーのばらつき）をならします。上げると動きに速く追従し、下げると揺れにくくなります（その分もたつく）。',
 secDist:'距離',minCm:'音程 いちばん近い',maxCm:'音程 いちばん遠い',holdCm:'最低音のまま',volMinCm:'音量 最大の距離',volMaxCm:'音量 無音の距離',
 distNote:'距離センサーは VL53L1X と VL53L4CD のどちらでも使えます（自動で見分けます）。遠いほど値がばらつき、特に VL53L4CD は 50cm を超えると音程が揺れやすくなります。',
 secVol:'音量',volDbRange:'音量カーブの幅',releaseAbrupt:'払ったときの余韻',volAbruptDb:'払った判定',
 volNote:'音量の手を一気に払ったときだけ余韻を残し、ゆっくり遠ざけたときや音程の手を離したときはすぐ切ります。',
 secFx:'エフェクト',harmony:'ハーモニー',interval:'音程',
 iv_m12:'1オクターブ下',iv_m7:'5度下',iv_m5:'4度下',iv_3:'短3度上',iv_4:'長3度上',iv_5:'4度上',iv_7:'5度上',iv_12:'1オクターブ上',
 vibrato:'ビブラート',depth:'深さ',rate:'速さ',overdrive:'オーバードライブ',drive:'歪み',odTone:'音色',
 leslie:'レスリー',amount:'量',speed:'速度',slow:'スロー',fast:'ファスト',slowDesc:'ゆったり',fastDesc:'速いうねり',
 chorus:'コーラス',delay:'ディレイ',time:'時間',feedback:'繰り返し',reverb:'リバーブ',size:'広さ',damp:'響きの暗さ',
 fxNote:'深さ・量を 0 にするとオフです。ディレイとリバーブは音を切っても余韻が残ります。',off:'オフ',
 secBoot:'起動時の設定',load:'読込',saveCurrent:'今の設定を保存',clear:'消去',bootSaved:'保存済み',bootUnsaved:'未保存（初期値で起動）',
 bootNote:'電源を入れたときは、ここに保存した設定で起動します。未保存のときは初期値で起動します。消去しても今の設定は変わりません（すぐ初期値にするには「初期値に戻す」）。',
 wifiOn:'パスワードあり',wifiOff:'パスワードなし（誰でもつなげます）',wifiBypass:'つまみが右いっぱいなので、パスワードなしです',
 wifiChange:'パスワードを変更',wifiPh:'新しいパスワード（空欄でなし）',wifiBad:'8〜63 文字の英数字・記号にしてください（空欄でパスワードなし）',
 wifiConfirm:'保存すると本体が再起動します。新しい設定で Wi-Fi につなぎ直してください。',wifiConfirmOff:'パスワードをなしにします。保存すると本体が再起動します。',
 wifiDone:'保存しました。再起動します…',
 wifiNote:'8〜63 文字。空欄で保存するとパスワードなしになります。忘れたときは、本体のつまみを右いっぱいに回すとパスワードなしになります（保存したパスワードは消えません）。演奏中はつまみを絞り切って Wi-Fi を切ると、Wi-Fi の小さなノイズが消えます。',
 secBuiltin:'組み込みプリセット',builtinNote:'音色とエフェクトだけを変えます（音程・距離・音量の設定はそのまま）。気に入ったらプリセットに保存してください。',
 secPresets:'プリセット',namedPresets:'名前付きプリセット',save:'保存',delete:'削除',empty:'（空き）',presetsNote:'保存の瞬間は音が一瞬止まります。',reset:'初期値に戻す',
 loaded:'読み込みました',saved:'保存しました',bootSavedToast:'起動時の設定を保存しました',builtinLoaded:'{0} を読み込みました',resetDone:'初期値に戻しました',
 ok:'OK',cancel:'キャンセル',presetName:'プリセットの名前',confirmDelete:'削除しますか？',
 confirmBootClear:'起動時の設定を消去しますか？（次の起動から初期値で起動します）',
 confirmReset:'設定を初期値に戻しますか？（保存したプリセットは消えません）',
 b_classic:'クラシック・テルミン',b_scifi:'SF映画',b_voice:'ボーカル風',b_synth:'シンセリード',b_jazzorgan:'ジャズオルガン',
 b_rockorgan:'ロックオルガン',b_guitar:'ギター風リード',b_twinlead:'ツインリード',b_strings:'ストリングス風',b_ambient:'アンビエント'},
en:{subtitle:'Digimin control panel',langBtn:'日本語',
 secLive:'Now',livePitchHand:'Pitch hand',liveVolHand:'Volume hand',livePitch:'Pitch',liveVol:'Volume',silent:'Silent',
 secTone:'Tone',sine:'Sine',tri:'Triangle',saw:'Sawtooth',
 toneNote:'Mixed by ratio (always totals 100%).',
 secPitch:'Pitch',lowNote:'Lowest note',octaves:'Range',glide:'Glide',follow:'Tracking speed',
 followNote:'Smooths out sensor jitter when your hand is still. Higher follows movement faster; lower is steadier but feels more sluggish.',
 secDist:'Distance',minCm:'Pitch: nearest',maxCm:'Pitch: farthest',holdCm:'Hold lowest note',volMinCm:'Volume: loudest at',volMaxCm:'Volume: silent at',
 distNote:'Either VL53L1X or VL53L4CD sensors work (detected automatically). Readings get noisier with distance; with a VL53L4CD the pitch tends to waver beyond about 50 cm.',
 secVol:'Volume',volDbRange:'Curve range',releaseAbrupt:'Tail on swipe',volAbruptDb:'Swipe threshold',
 volNote:'A tail is left only when you swipe the volume hand away quickly. Moving it away slowly or lifting the pitch hand cuts the sound at once.',
 secFx:'Effects',harmony:'Harmony',interval:'Interval',
 iv_m12:'Octave below',iv_m7:'5th below',iv_m5:'4th below',iv_3:'Minor 3rd above',iv_4:'Major 3rd above',iv_5:'4th above',iv_7:'5th above',iv_12:'Octave above',
 vibrato:'Vibrato',depth:'Depth',rate:'Rate',overdrive:'Overdrive',drive:'Drive',odTone:'Tone',
 leslie:'Leslie',amount:'Amount',speed:'Speed',slow:'Slow',fast:'Fast',slowDesc:'Chorale',fastDesc:'Tremolo',
 chorus:'Chorus',delay:'Delay',time:'Time',feedback:'Feedback',reverb:'Reverb',size:'Size',damp:'Darkness',
 fxNote:'Set depth or amount to 0 to turn an effect off. Delay and reverb tails continue after the sound stops.',off:'Off',
 secBoot:'Startup settings',load:'Load',saveCurrent:'Save current',clear:'Clear',bootSaved:'Saved',bootUnsaved:'Not saved (starts with defaults)',
 bootNote:'The unit starts with the settings saved here. If nothing is saved, it starts with the defaults. Clearing does not change the current settings (use “Reset to defaults” for that).',
 wifiOn:'Password set',wifiOff:'No password (anyone can connect)',wifiBypass:'The knob is full up, so there is no password',
 wifiChange:'Change password',wifiPh:'New password (empty = none)',wifiBad:'Use 8–63 letters, digits or symbols (empty = no password)',
 wifiConfirm:'The unit will restart after saving. Reconnect to the Wi-Fi with the new settings.',wifiConfirmOff:'Remove the password? The unit will restart after saving.',
 wifiDone:'Saved. Restarting…',
 wifiNote:'8–63 characters. Save it empty for no password. If you forget it, turn the knob on the unit all the way up to run without a password (the saved one is kept). Turn the knob all the way down while playing to switch Wi-Fi off and remove its small noise.',
 secBuiltin:'Built-in presets',builtinNote:'Changes only the tone and effects (pitch, distance and volume settings stay). Save it to a preset if you like it.',
 secPresets:'Presets',namedPresets:'Named presets',save:'Save',delete:'Delete',empty:'(empty)',presetsNote:'The sound stops for a moment while saving.',reset:'Reset to defaults',
 loaded:'Loaded',saved:'Saved',bootSavedToast:'Startup settings saved',builtinLoaded:'Loaded {0}',resetDone:'Reset to defaults',
 ok:'OK',cancel:'Cancel',presetName:'Preset name',confirmDelete:'Delete this preset?',
 confirmBootClear:'Clear the startup settings? (The unit will start with the defaults next time.)',
 confirmReset:'Reset all settings to the defaults? (Saved presets are kept.)',
 b_classic:'Classic Theremin',b_scifi:'Sci-Fi Movie',b_voice:'Voice',b_synth:'Synth Lead',b_jazzorgan:'Jazz Organ',
 b_rockorgan:'Rock Organ',b_guitar:'Guitar Lead',b_twinlead:'Twin Lead',b_strings:'Strings',b_ambient:'Ambient'}};
let lang='ja';
try{lang=localStorage.getItem('digiminLang')||''}catch(e){lang=''}
if(!I18N[lang])lang=(navigator.language||'').toLowerCase().startsWith('ja')?'ja':'en';
function t(k,a){let v=(I18N[lang][k]!==undefined?I18N[lang][k]:I18N.ja[k])||k;if(a!==undefined)v=v.replace('{0}',a);return v}
function applyLang(){document.documentElement.lang=lang;
 document.querySelectorAll('[data-i18n]').forEach(e=>e.textContent=t(e.dataset.i18n));
 document.getElementById('langBtn').textContent=t('langBtn');
 renderIntervals();renderBuiltins();if(presetList)renderPresets(presetList);if(st)fill(st)}
function toggleLang(){lang=lang==='ja'?'en':'ja';try{localStorage.setItem('digiminLang',lang)}catch(e){}applyLang()}

// ---- 表示 ----
const NOTES=['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
const sel=document.getElementById('lowNote');
for(let m=24;m<=72;m++){const o=document.createElement('option');o.value=m;o.textContent=NOTES[m%12]+(Math.floor(m/12)-1);sel.appendChild(o)}
const hzToMidi=h=>Math.round(69+12*Math.log2(h/440)), midiToHz=m=>440*Math.pow(2,(m-69)/12);
const noteName=h=>{const m=Math.round(69+12*Math.log2(h/440));return NOTES[((m%12)+12)%12]+(Math.floor(m/12)-1)};
const offOr=(v,s)=>v==0?t('off'):s;
const fmt={sine:v=>v+'%',tri:v=>v+'%',saw:v=>v+'%',octaves:v=>v+' oct',glideMs:v=>v+' ms',
 minCm:v=>v+' cm',maxCm:v=>v+' cm',holdCm:v=>v+' cm',volMinCm:v=>v+' cm',volMaxCm:v=>v+' cm',
 volDbRange:v=>v+' dB',releaseAbruptMs:v=>v+' ms',volAbruptDb:v=>v+' dB',
 vibCents:v=>offOr(v,v+' c'),vibRateHz:v=>(+v).toFixed(1)+' Hz',delayMix:v=>offOr(v,v+'%'),delayMs:v=>v+' ms',delayFeedback:v=>v+'%',
 reverbMix:v=>offOr(v,v+'%'),reverbSize:v=>v+'%',reverbDamp:v=>v+'%',
 chorusMix:v=>offOr(v,v+'%'),chorusRateHz:v=>(+v).toFixed(2)+' Hz',chorusDepthMs:v=>(+v).toFixed(1)+' ms',
 odDrive:v=>offOr(v,v+'%'),odTone:v=>v+'%',leslieMix:v=>offOr(v,v+'%'),harmonyMix:v=>offOr(v,v+'%'),
 pitchBeta:v=>(+v).toFixed(1),volBeta:v=>(+v).toFixed(1)};
// 画面では 0〜100、本体には 0〜1 で送る項目
const keyOf={sine:1,tri:1,saw:1,delayMix:1,delayFeedback:1,reverbMix:1,reverbSize:1,reverbDamp:1,chorusMix:1,
 odDrive:1,odTone:1,leslieMix:1,harmonyMix:1};
// ハーモニーの音程の選択肢（半音）
const INTERVALS=[-12,-7,-5,3,4,5,7,12];
const hsel=document.getElementById('harmonySemis');
function renderIntervals(){const v=hsel.value;hsel.innerHTML='';
 INTERVALS.forEach(n=>{const o=document.createElement('option');o.value=n;o.textContent=t('iv_'+(n<0?'m'+(-n):n));hsel.appendChild(o)});
 if(v!=='')hsel.value=v}
hsel.addEventListener('change',()=>{hsel.dataset.touched=Date.now();queue('harmonySemis',+hsel.value);
 hsel.nextElementSibling.textContent=(+hsel.value>0?'+':'')+hsel.value})
let st=null, pending={}, timer=null, presetList=null, builtinList=[];
// 確認・名前の入力は画面内のダイアログで行う。Wi-Fi につないだときに自動で開く簡易ブラウザ（iPhone など）は
// ブラウザ標準の確認・入力ダイアログを表示せず、常にキャンセル扱いにするため
function dialog(msg,def,note){return new Promise(res=>{
 const d=document.getElementById('dlg'),inp=document.getElementById('dlgIn'),ok=document.getElementById('dlgOk'),cn=document.getElementById('dlgCancel');
 document.getElementById('dlgMsg').textContent=msg;
 const nt=document.getElementById('dlgNote');nt.hidden=!note;nt.textContent=note||'';ok.textContent=t('ok');cn.textContent=t('cancel');
 const ask=def!==undefined;inp.hidden=!ask;if(ask)inp.value=def;
 const done=v=>{d.hidden=true;ok.onclick=cn.onclick=inp.onkeydown=null;res(v)};
 ok.onclick=()=>done(ask?inp.value:true);cn.onclick=()=>done(ask?null:false);
 inp.onkeydown=e=>{if(e.key==='Enter')ok.onclick()};
 d.hidden=false;if(ask)setTimeout(()=>inp.focus(),50)})}
// 空き枠に保存するときの名前の初期値: スマホの時計から「20261023-1405」（年月日-時分）を作る
function stampName(){const d=new Date(),z=n=>String(n).padStart(2,'0');
 return d.getFullYear()+z(d.getMonth()+1)+z(d.getDate())+'-'+z(d.getHours())+z(d.getMinutes())}
function toast(m){const e=document.getElementById('msg');e.textContent=m;e.style.opacity=1;setTimeout(()=>e.style.opacity=0,1200)}
function send(){timer=null;const q=new URLSearchParams(pending);pending={};fetch('/api/set?'+q).then(r=>r.json()).then(s=>{st=s;}).catch(()=>{})}
function queue(k,v){pending[k]=v;if(!timer)timer=setTimeout(send,60)}
document.querySelectorAll('input[type=range]').forEach(el=>{
 el.addEventListener('input',()=>{const v=+el.value;el.nextElementSibling.textContent=fmt[el.id](v);
  queue(el.id,(el.id in keyOf)?v/100:v);el.dataset.touched=Date.now()});
});
sel.addEventListener('change',()=>{const h=midiToHz(+sel.value);sel.nextElementSibling.textContent=h.toFixed(1)+' Hz';queue('lowHz',h)});
function fill(s){
 const vals={sine:Math.round(s.mix[0]*100),tri:Math.round(s.mix[1]*100),saw:Math.round(s.mix[2]*100),
  octaves:s.octaves,glideMs:Math.round(s.glideMs),minCm:Math.round(s.minCm),maxCm:Math.round(s.maxCm),holdCm:Math.round(s.holdCm),
  volMinCm:Math.round(s.volMinCm),volMaxCm:Math.round(s.volMaxCm),volDbRange:Math.round(s.volDbRange),
  releaseAbruptMs:Math.round(s.releaseAbruptMs),volAbruptDb:Math.round(s.volAbruptDb),
  vibCents:Math.round(s.vibCents),vibRateHz:s.vibRateHz,delayMix:Math.round(s.delayMix*100),delayMs:Math.round(s.delayMs),
  delayFeedback:Math.round(s.delayFeedback*100),reverbMix:Math.round(s.reverbMix*100),reverbSize:Math.round(s.reverbSize*100),
  reverbDamp:Math.round(s.reverbDamp*100),
  chorusMix:Math.round(s.chorusMix*100),chorusRateHz:s.chorusRateHz,chorusDepthMs:s.chorusDepthMs,
  odDrive:Math.round(s.odDrive*100),odTone:Math.round(s.odTone*100),leslieMix:Math.round(s.leslieMix*100),
  harmonyMix:Math.round(s.harmonyMix*100),pitchBeta:s.pitchBeta,volBeta:s.volBeta};
 for(const k in vals){const el=document.getElementById(k);if(!el)continue;
  if(el.dataset.touched&&Date.now()-el.dataset.touched<1500)continue;  // 動かしている最中は上書きしない
  el.value=vals[k];el.nextElementSibling.textContent=fmt[k](vals[k])}
 if(!(hsel.dataset.touched&&Date.now()-hsel.dataset.touched<1500)){
  hsel.value=Math.round(s.harmonySemis);hsel.nextElementSibling.textContent=(s.harmonySemis>0?'+':'')+Math.round(s.harmonySemis)}
 const fast=s.leslieFast>0.5, lb=document.getElementById('leslieBtn');
 lb.textContent=t(fast?'fast':'slow');lb.className=fast?'acc':'';
 lb.nextElementSibling.textContent=t(fast?'fastDesc':'slowDesc');
 sel.value=hzToMidi(s.lowHz);sel.nextElementSibling.textContent=s.lowHz.toFixed(1)+' Hz';
 if(s.ssid)document.getElementById('ssid').textContent='· '+s.ssid;
 document.getElementById('wifiState').textContent=s.wifiOpen?t('wifiBypass'):s.wifiPass?t('wifiOn'):t('wifiOff');
 const L=s.live;
 const kn=document.getElementById('knobNote');
 kn.textContent=t('toneNote');
 document.getElementById('lp').textContent=L.pitchCm==null?'--':L.pitchCm.toFixed(1)+' cm';
 document.getElementById('lv').textContent=L.volCm==null?'--':L.volCm.toFixed(1)+' cm';
 document.getElementById('bp').style.width=L.pitchCm==null?'0':Math.max(0,Math.min(100,100*(1-(L.pitchCm-s.minCm)/(s.maxCm+s.holdCm-s.minCm))))+'%';
 document.getElementById('bv').style.width=L.volCm==null?'0':Math.max(0,Math.min(100,100*(1-(L.volCm-s.volMinCm)/(s.volMaxCm-s.volMinCm))))+'%';
 document.getElementById('lh').textContent=L.amp>0?(L.hz.toFixed(0)+' Hz '+noteName(L.hz)):'--';
 document.getElementById('la').textContent=L.amp>0?(L.amp>0.0001?(20*Math.log10(L.amp)).toFixed(0)+' dB':'--'):t('silent');
}
// 状態の取得は 0.5 秒ごと。画面が見えていないときは止める（Wi-Fi の送信が電源を揺らし、音のノイズになるため）
function poll(){if(document.hidden){setTimeout(poll,1000);return}
 fetch('/api/state').then(r=>r.json()).then(s=>{st=s;fill(s)}).catch(()=>{}).finally(()=>setTimeout(poll,500))}
// 0 番の枠は「起動時の設定」、1〜7 番の枠をプリセット 1〜7 として表示する
function reloaded(s){document.querySelectorAll('input[type=range],select').forEach(e=>delete e.dataset.touched);st=s;fill(s)}
function renderBoot(b){const n=document.getElementById('bootName');
 n.textContent=t(b.used?'bootSaved':'bootUnsaved');n.className='name'+(b.used?'':' empty');
 document.getElementById('bootLoadBtn').disabled=!b.used;document.getElementById('bootClearBtn').disabled=!b.used}
// ページ上には入力欄を置かない（キャプティブポータルの画面が入力欄にフォーカスしてキーボードが出るため）。
// ボタンを押したときだけダイアログで入力する
async function wifiSave(){const p=await dialog(t('wifiPh'),'',t('wifiNote'));if(p===null)return;
 if(p.length&&(p.length<8||p.length>63||!/^[\x20-\x7e]*$/.test(p))){toast(t('wifiBad'));return}
 if(!await dialog(t(p.length?'wifiConfirm':'wifiConfirmOff')))return;
 fetch('/api/wifi/pass',{method:'POST',body:new URLSearchParams({pass:p})}).then(r=>{if(!r.ok)throw 0;toast(t('wifiDone'))}).catch(()=>toast(t('wifiBad')))}
function bootSave(){fetch('/api/preset/save?slot=0&name=startup').then(r=>r.json()).then(l=>{renderPresets(l);toast(t('bootSavedToast'))})}
function bootLoad(){fetch('/api/preset/load?slot=0').then(r=>r.json()).then(s=>{reloaded(s);toast(t('loaded'))})}
async function bootClear(){if(!await dialog(t('confirmBootClear')))return;
 fetch('/api/preset/delete?slot=0').then(r=>r.json()).then(renderPresets)}
function renderPresets(list){presetList=list;const box=document.getElementById('presets');box.innerHTML='';
 renderBoot(list[0]);
 list.slice(1).forEach(p=>{const d=document.createElement('div');d.className='preset';
  const nm=document.createElement('span');nm.className='name'+(p.used?'':' empty');nm.textContent=p.slot+'. '+(p.used?p.name:t('empty'));
  const b1=document.createElement('button');b1.textContent=t('load');b1.disabled=!p.used;
  b1.onclick=()=>fetch('/api/preset/load?slot='+p.slot).then(r=>r.json()).then(s=>{reloaded(s);toast(t('loaded'))});
  const b2=document.createElement('button');b2.textContent=t('save');b2.className='acc';
  b2.onclick=async()=>{const n=await dialog(t('presetName'),p.used?p.name:stampName());if(n===null)return;
   fetch('/api/preset/save?slot='+p.slot+'&name='+encodeURIComponent(n)).then(r=>r.json()).then(l=>{renderPresets(l);toast(t('saved'))})};
  const b3=document.createElement('button');b3.textContent=t('delete');b3.disabled=!p.used;
  b3.onclick=async()=>{if(!await dialog(t('confirmDelete')))return;fetch('/api/preset/delete?slot='+p.slot).then(r=>r.json()).then(renderPresets)};
  d.append(nm,b1,b2,b3);box.appendChild(d)});}
function toggleLeslie(){const fast=!(st&&st.leslieFast>0.5);
 fetch('/api/set?leslieFast='+(fast?1:0)).then(r=>r.json()).then(s=>{st=s;fill(s)})}
// 組み込みプリセットの名前は id で翻訳する（本体から来る名前は翻訳がないときの予備）
const builtinName=b=>I18N[lang]['b_'+b.id]||b.label;
function renderBuiltins(){const box=document.getElementById('builtins');box.innerHTML='';
 builtinList.forEach(b=>{const btn=document.createElement('button');btn.textContent=builtinName(b);
  btn.onclick=()=>fetch('/api/builtin?id='+b.id).then(r=>r.json()).then(s=>{reloaded(s);toast(t('builtinLoaded',builtinName(b)))});
  box.appendChild(btn)})}
async function resetAll(){if(!await dialog(t('confirmReset')))return;
 fetch('/api/reset').then(r=>r.json()).then(s=>{reloaded(s);toast(t('resetDone'))})}
applyLang();
fetch('/api/builtins').then(r=>r.json()).then(l=>{builtinList=l;renderBuiltins()});
fetch('/api/presets').then(r=>r.json()).then(renderPresets);
poll();
</script></body></html>)HTML";
