/*
 * main.c -- AVR C implementation for "temperature sensor fan" (ATmega328P)
 * - F_CPU = 16MHz
 * - Timer1 CTC generates 1ms tick (millis)
 * - PWM on OC0A (PD6 / Arduino D6)
 * - DHT11 on PD2 (D2)
 *
 * Build (avr-gcc + avrdude): see README in this folder.
 */

#define F_CPU 16000000UL

#include <avr/io.h>               // AVR 入出力レジスタ定義
#include <avr/interrupt.h>       // 割り込み関連定義
#include <util/delay.h>          // _delay_ms/_delay_us 用
#include <stdint.h>              // 固定長整数型
#include <stdbool.h>             // bool 型
#include <string.h>              // 文字列操作（必要に応じて）

// ----- Pin mapping (Arduino Uno)
#define PIN_DHT_DDR   DDRD   // DHT 用 DDR（データ方向レジスタ）
#define PIN_DHT_PORT  PORTD  // DHT 用 PORT レジスタ
#define PIN_DHT_PIN   PIND   // DHT 用 PIN レジスタ（入力読み取り）
#define PIN_DHT_BIT   PD2    // DHT DATA ピン（Arduino D2）

#define PIN_POWER_BIT PD3   // 電源ボタン入力ビット（D3）
#define PIN_WEAK_BIT  PD4   // 弱ボタン入力ビット（D4）
#define PIN_STRONG_BIT PD5  // 強ボタン入力ビット（D5）

/* LCD を使わない設定に変更 */

// MOSFET gate -> D6 (PD6) (OC0A)
#define MOSFET_DDR DDRD    // MOSFET 用 DDR
#define MOSFET_PORT PORTD  // MOSFET 用 PORT
#define MOSFET_BIT PD6     // MOSFET Gate が接続されるピン（D6 / OC0A）

// LEDs: D13 (PB5), A0->PC0, A1->PC1
#define LEDW_PORT PORTB  // 白LED (D13) は PB5
#define LEDW_DDR DDRB
#define LEDW_BIT PB5
#define LEDG_PORT PORTC  // 緑LED (A0) は PC0
#define LEDG_DDR DDRC
#define LEDG_BIT PC0
#define LEDR_PORT PORTC  // 赤LED (A1) は PC1
#define LEDR_DDR DDRC
#define LEDR_BIT PC1

// Constants
enum { STATE_IDLE=0, STATE_WEAK=1, STATE_STRONG=2 };
const uint16_t SENSOR_INTERVAL = 2000; // センサー読み取り間隔（ms）
const uint16_t SENSOR_TIMEOUT = 4000; // センサー有効タイムアウト（ms）
const uint16_t LED_INTERVAL = 500;    // LED 更新間隔（ms）
const int FILTER_WINDOW_N = 3;        // 移動平均窓サイズ
const int SENSOR_FAIL_THRESHOLD = 3;  // 失敗連続回数でフォールバック
const uint8_t PWM_WEAK = 153;         // 弱風の PWM 値（約60%）
const uint8_t PWM_STRONG = 255;       // 強風の PWM 値（100%）
const int TEMP_OFF_THRESHOLD = 19;    // OFF 判定閾値
const int TEMP_WEAK_THRESHOLD = 20;   // WEAK 判定閾値
const int TEMP_STRONG_THRESHOLD = 25; // STRONG 判定閾値
const int HYSTERESIS = 1;             // ヒステリシス幅（℃）

// Globals
volatile uint32_t g_millis = 0;

volatile uint32_t lastMillis_Sensor = 0;     // 最後にセンサーを読んだ時刻
volatile uint32_t lastMillis_LED = 0;        // LED 更新の最終時刻
volatile uint32_t lastDebouncePower = 0;     // 電源ボタン用デバウンス時刻
volatile uint32_t lastDebounceWeak = 0;      // 弱ボタン用デバウンス時刻
volatile uint32_t lastDebounceStrong = 0;    // 強ボタン用デバウンス時刻

int currentState = STATE_IDLE;        // 現在状態（IDLE/WEAK/STRONG）
float sensorTemp = 0.0f;              // 生の温度値
bool sensorValid = false;             // センサー値が有効か
float filteredTemp = 0.0f;            // フィルタ後の温度
float filterBuffer[FILTER_WINDOW_N];  // 移動平均バッファ
int filterIndex = 0;                  // バッファ書込み位置
int filterCount = 0;                  // バッファ内の有効要素数
int sensorFailCount = 0;              // センサー失敗連続回数
uint32_t lastValidMillis = 0;         // 最後に有効値を受け取った時刻
bool modeManual = false;              // 手動モードか（true）
uint8_t motorPWM = 0;                 // 現在の PWM 値

// Forward
void timer1_init_ms(void);                   // 1ms タイマ初期化
uint32_t millis(void);                       // ミリ秒時刻取得
void pwm_init(void);                         // PWM 初期化（OC0A）
void dht_set_output_low(void);               // DHT 用出力駆動開始
void dht_set_input(void);                    // DHT を入力に戻す
int dht_read_temp(float *out);               // DHT11 から温度を読む
void readButtons(void);                      // ボタン読み取り（デバウンス）
void readSensor(void);                       // センサー周期制御と読み取り
void autoControl(void);                      // 自動制御ロジック
void updateOutput(int state);                // PWM/LED の出力更新
float movingAverageFilter(float v);          // 移動平均フィルタ

ISR(TIMER1_COMPA_vect) { g_millis++; } // Timer1 割り込み: 1ms 毎にカウント

uint32_t millis(void) { return g_millis; } // ミリ秒カウンタを返す

void timer1_init_ms(void) {
    cli(); // 割り込み禁止
    // CTC モード、プリスケーラ 64 -> 16MHz/64 = 250kHz、OCR1A=249 で 1ms
    TCCR1B = (1<<WGM12) | (1<<CS11) | (1<<CS10); // CTC, clk/64
    OCR1A = 249; // 比較一致値（1ms 毎）
    TIMSK1 = (1<<OCIE1A); // コンパレータ A 割り込みを有効化
    sei(); // 割り込み許可
}

void pwm_init(void) {
    // Timer0 を Fast PWM、OC0A を非反転出力に設定（OC0A = PD6）
    TCCR0A = (1<<COM0A1) | (1<<WGM01) | (1<<WGM00); // 非反転, Fast PWM
    TCCR0B = (1<<CS01) | (1<<CS00); // クロック分周 64
    MOSFET_DDR |= (1<<MOSFET_BIT); // PD6 を出力に設定
    OCR0A = 0; // 初期デューティ 0
}

// DHT11 helpers
void dht_set_output_low(void) {
    PIN_DHT_PORT &= ~(1<<PIN_DHT_BIT); // 出力を LOW にセット
    PIN_DHT_DDR |= (1<<PIN_DHT_BIT);   // ピンを出力モードにする
}
void dht_set_input(void) {
    PIN_DHT_DDR &= ~(1<<PIN_DHT_BIT);  // ピンを入力モードに戻す
    PIN_DHT_PORT &= ~(1<<PIN_DHT_BIT); // プルアップを無効にする
}

// Wait for pin value with timeout (us). returns 0 on timeout, 1 if matched
static int wait_pin_level(uint8_t bit, uint8_t level, uint32_t timeout_us) {
    // 指定ビットが指定レベルになるまで待機（タイムアウトはマイクロ秒）
    while (((PIN_DHT_PIN >> bit) & 1) != level) {
        if (timeout_us == 0) return 0; // タイムアウト
        _delay_us(1);
        timeout_us--;
    }
    return 1; // 指定レベルを検出
}

// Read DHT11 temperature only, returns 1 on success
int dht_read_temp(float *out) {
    uint8_t data[5] = {0};
    // Start 信号を送る（DHT11 プロトコル）
    dht_set_output_low();
    _delay_ms(20); // マスタが 18ms 以上 LOW を維持
    dht_set_input();
    _delay_us(30); // リリースしてレスポンスを待つ
    // センサの応答: LOW 約80us, HIGH 約80us を待つ
    if (!wait_pin_level(PIN_DHT_BIT, 0, 100)) return 0; // 応答 LOW が来ない
    if (!wait_pin_level(PIN_DHT_BIT, 1, 100)) return 0; // 応答 HIGH が来ない
    if (!wait_pin_level(PIN_DHT_BIT, 0, 100)) return 0; // データ送信開始 LOW
    // 40 ビットを受信
    for (int i = 0; i < 40; ++i) {
        if (!wait_pin_level(PIN_DHT_BIT, 1, 100)) return 0; // ビット開始の HIGH を待つ
        uint16_t len = 0;
        while (((PIN_DHT_PIN >> PIN_DHT_BIT) & 1)) { // HIGH の長さを測定
            _delay_us(1);
            len++;
            if (len > 200) return 0; // タイムアウト
        }
        // HIGH の長さで 0/1 を判定（経験的閾値）
        int bitval = (len > 40) ? 1 : 0;
        data[i/8] <<= 1;
        data[i/8] |= bitval;
    }
    // checksum
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) return 0;
    // DHT11: data[2] = temp integer
    *out = (float)data[2];
    return 1;
}

/* LCD は未使用：関数群を削除 */

// ボタン読み取り（デバウンス処理）
void readButtons(void) {
    uint32_t now = millis(); // 現在時刻（ms）を取得
    // 電源スイッチ (PD3) - 押下で LOW になる（内部プルアップ使用）
    bool rawPower = !((PIND >> PIN_POWER_BIT) & 1); // 押下: true
    static bool powerState = false; // 前回の安定状態を保存
    if (rawPower != powerState) { // 状態に変化があればデバウンス判定
        if (now - lastDebouncePower >= 50) { // 50ms のデバウンス期間を満たす
            powerState = rawPower; // 新しい安定状態に更新
            lastDebouncePower = now; // デバウンス時刻を記録
            if (powerState) { // 押された -> 手動モード、弱風に遷移
                modeManual = true;
                currentState = STATE_WEAK;
            } else { // 離された -> 自動モード、待機状態
                modeManual = false;
                currentState = STATE_IDLE;
            }
        }
    }

    // 弱 (PD4)
    bool rawWeak = !((PIND >> PIN_WEAK_BIT) & 1); // 押下で true
    static bool weakState = false; // 前回状態
    if (rawWeak != weakState) {
        if (now - lastDebounceWeak >= 50) { // デバウンス判定
            weakState = rawWeak; // 更新
            lastDebounceWeak = now;
            if (weakState) { // 押下されたら手動弱風
                modeManual = true; currentState = STATE_WEAK;
            }
        }
    }

    // 強 (PD5)
    bool rawStrong = !((PIND >> PIN_STRONG_BIT) & 1); // 押下で true
    static bool strongState = false; // 前回状態
    if (rawStrong != strongState) {
        if (now - lastDebounceStrong >= 50) {
            strongState = rawStrong;
            lastDebounceStrong = now;
            if (strongState) { // 押下されたら手動強風
                modeManual = true; currentState = STATE_STRONG;
            }
        }
    }
}

// センサー読み取り（間隔制御 + DHT 読み出し）
void readSensor(void) {
    uint32_t now = millis();
    if (now - lastMillis_Sensor < SENSOR_INTERVAL) return; // 間隔未満なら抜ける
    lastMillis_Sensor = now; // 最終読み取り時刻を更新
    float t;
    if (dht_read_temp(&t)) { // 読み取り成功
        sensorTemp = t; // 生値保存
        sensorValid = true; // 有効フラグ
        lastValidMillis = now; // 最終有効時刻
        sensorFailCount = 0; // 失敗カウンタをリセット
        filteredTemp = movingAverageFilter(sensorTemp); // フィルタ適用
    } else { // 読み取り失敗時の扱い
        sensorFailCount++;
        if (sensorFailCount >= SENSOR_FAIL_THRESHOLD) { // 連続失敗が閾値を超えたらフォールバック
            sensorValid = false;
            currentState = STATE_WEAK; // センサー無しは安全側で弱風
            motorPWM = PWM_WEAK; // PWM を弱に設定（実際は updateOutput で反映）
        }
    }
}

// 自動制御ロジック（フィルタ済み温度を参照）
void autoControl(void) {
    if (!sensorValid) return; // センサー無効なら何もしない
    float t = filteredTemp; // フィルタ後の温度
    // 強風からの戻り判定：ヒステリシスを適用
    if (currentState == STATE_STRONG) {
        if (t < (TEMP_STRONG_THRESHOLD - HYSTERESIS)) {
            if (t >= TEMP_WEAK_THRESHOLD) currentState = STATE_WEAK; else currentState = STATE_IDLE;
        }
        return; // 強風時の戻り判定のみ行って終了
    }
    // 通常の閾値判定
    if (t >= TEMP_STRONG_THRESHOLD) currentState = STATE_STRONG;
    else if (t >= TEMP_WEAK_THRESHOLD) currentState = STATE_WEAK;
    else if (t <= TEMP_OFF_THRESHOLD) currentState = STATE_IDLE;
}

// 出力更新 (PWM と LED)：状態に応じて出力をセット
void updateOutput(int state) {
    uint8_t newPWM = 0;
    switch (state) {
        case STATE_IDLE:
            newPWM = 0; // モーター停止
            LEDW_PORT &= ~(1<<LEDW_BIT); // 白 LED 消灯
            LEDG_PORT &= ~(1<<LEDG_BIT); // 緑 LED 消灯
            LEDR_PORT &= ~(1<<LEDR_BIT); // 赤 LED 消灯
            break;
        case STATE_WEAK:
            newPWM = PWM_WEAK; // 弱風 PWM
            LEDW_PORT |= (1<<LEDW_BIT);  // 白 LED 点灯
            LEDG_PORT |= (1<<LEDG_BIT);  // 緑 LED 点灯
            LEDR_PORT &= ~(1<<LEDR_BIT); // 赤 LED 消灯
            break;
        case STATE_STRONG:
            newPWM = PWM_STRONG; // 強風 PWM
            LEDW_PORT |= (1<<LEDW_BIT);  // 白 LED 点灯
            LEDG_PORT &= ~(1<<LEDG_BIT); // 緑 LED 消灯
            LEDR_PORT |= (1<<LEDR_BIT);  // 赤 LED 点灯
            break;
    }
    if (newPWM != motorPWM) { // 値が変わったら PWM を更新
        motorPWM = newPWM;
        OCR0A = motorPWM; // ハードウェア PWM レジスタに書き込み
    }
}

// 移動平均フィルタ（窓幅 FILTER_WINDOW_N）
float movingAverageFilter(float v) {
    filterBuffer[filterIndex] = v; // 新しい値をバッファに格納
    filterIndex = (filterIndex + 1) % FILTER_WINDOW_N; // 書込み位置を進める
    if (filterCount < FILTER_WINDOW_N) filterCount++; // 初期充填中はカウントを増やす
    float sum = 0.0f;
    for (int i = 0; i < filterCount; ++i) sum += filterBuffer[i]; // 合計を計算
    return sum / filterCount; // 平均を返す
}

int main(void) {
    // IO 方向設定
    // ボタン用ピンを入力にし、内部プルアップを有効化
    DDRD &= ~((1<<PIN_DHT_BIT)|(1<<PIN_POWER_BIT)|(1<<PIN_WEAK_BIT)|(1<<PIN_STRONG_BIT)); // DHT + ボタンを入力に
    PORTD |= (1<<PIN_POWER_BIT)|(1<<PIN_WEAK_BIT)|(1<<PIN_STRONG_BIT); // ボタン入力に対して内部プルアップを有効
    // LED を出力に設定
    LEDW_DDR |= (1<<LEDW_BIT);
    LEDG_DDR |= (1<<LEDG_BIT);
    LEDR_DDR |= (1<<LEDR_BIT);
    LEDW_PORT &= ~(1<<LEDW_BIT); // LED を消灯
    LEDG_PORT &= ~(1<<LEDG_BIT);
    LEDR_PORT &= ~(1<<LEDR_BIT);

    // サブシステム初期化
    timer1_init_ms(); // millis() 用タイマ初期化
    pwm_init(); // PWM 出力初期化 (OC0A)

    // メインループ
    while (1) {
        readButtons();                 // ボタン読み取り
        readSensor();                  // センサー周期処理
        if (!modeManual) autoControl();// 自動制御（手動でなければ）
        updateOutput(currentState);    // 出力更新（PWM/LED）
        // LCD は未使用のためここでは何もしない
        (void)0;
    }
    return 0;
}
