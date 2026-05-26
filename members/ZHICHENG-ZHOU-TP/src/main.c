// Arduino Code (Timer Fan)
// タイマーで自動停止する扇風機制御

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef uint8_t byte;

enum {
  LOW = 0,
  HIGH = 1,
  INPUT_PULLUP = 2,
  OUTPUT = 3
};

// C環境でコンパイル可能にするための最小HALスタブ
static unsigned long fakeMillis = 0;

static void pinMode(byte pin, int mode) {
  (void)pin;
  (void)mode;
}

static void digitalWrite(byte pin, int value) {
  (void)pin;
  (void)value;
}

static int digitalRead(byte pin) {
  (void)pin;
  return HIGH;
}

static unsigned long millis(void) {
  fakeMillis += 10;
  return fakeMillis;
}

static void delay(unsigned long ms) {
  fakeMillis += ms;
}

static void logText(const char* text) {
  printf("%s\n", text);
}

static void logLabelU32(const char* label, unsigned long value) {
  printf("%s%lu\n", label, value);
}

// ピン定義
const byte PIN_START_BUTTON = 2;
const byte PIN_STOP_BUTTON = 3;
const byte PIN_LED_STATUS = 6;
const byte PIN_MOTOR_CTRL = 9;

// 状態定義
const byte STATE_IDLE = 0; // 待機中
const byte STATE_RUNNING = 1; // 動作中
const byte STATE_FINISHED = 2; // 停止完了
const byte STATE_ERROR = 3; // 異常停止

// 時間定義
const unsigned long DEBOUNCE_DELAY_MS = 50;
const unsigned long TIMER_TICK_MS = 1000;
const unsigned long FINISH_BLINK_INTERVAL = 500;
const unsigned long ERROR_BLINK_INTERVAL = 200;
const unsigned long LONG_PRESS_MS = 2000;
const unsigned long DOUBLE_CLICK_MS = 1000; // ダブルクリック判定時間

// 妥当範囲
const unsigned int MAX_REMAINING_SECONDS = 1800; // 30分

// グローバル変数
byte currentState = STATE_IDLE;

unsigned long lastTickMillis = 0;
unsigned long lastBlinkMillis = 0;
unsigned long lastButtonMillis = 0;
unsigned long lastErrorBlinkMillis = 0;
unsigned long stopPressStartMillis = 0;
unsigned long lastStartEventMillis = 0;

byte selectedMinutes = 10;
unsigned int remainingSeconds = 0;
bool startButtonEvent = false;
bool stopButtonEvent = false;
bool ledState = false;
bool stopLongPressHandled = false;
bool startArmed = false; // ダブルクリック待ちフラグ

// 前回のボタン状態（INPUT_PULLUP なので未押下=HIGH、押下=LOW）
byte prevStartRaw = HIGH;
byte prevStopRaw = HIGH;

// ========== 関数宣言 ==========
void readButtons(unsigned long now);
void handleTimeSetting();
void handleStopReset();
void handleErrorState(unsigned long now);
void handleFinishedState(unsigned long now);
void handleIdleState(unsigned long now);
void runTimerControl(unsigned long now);
void factoryResetByLongPress(unsigned long now);
void blinkOnFinish(unsigned long now);
void updateOutputs();

// ========== setup ==========
void setup() {
  pinMode(PIN_START_BUTTON, INPUT_PULLUP);
  pinMode(PIN_STOP_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED_STATUS, OUTPUT);
  pinMode(PIN_MOTOR_CTRL, OUTPUT);

  digitalWrite(PIN_MOTOR_CTRL, LOW);
  digitalWrite(PIN_LED_STATUS, LOW);

  currentState = STATE_IDLE;
  selectedMinutes = 10;
  remainingSeconds = 0;
  ledState = false;
  startArmed = false;

  unsigned long now = millis();
  lastTickMillis = now;
  lastBlinkMillis = now;
  lastButtonMillis = now;
  lastErrorBlinkMillis = now;
  stopPressStartMillis = 0;
  lastStartEventMillis = 0;

  logText("[BOOT] Timer fan controller started.");
  logText("[INFO] Idle: 1x START=change(10/20/30), 2x START within 1s=run");

  updateOutputs();
}

// ========== loop ==========
void loop() {
  unsigned long now = millis();

  readButtons(now);
  factoryResetByLongPress(now);
  handleStopReset(); // STOPボタン処理（優先）

  // 状態ごとの処理
  switch (currentState) {
    case STATE_IDLE:
    handleIdleState(now);
    break;
    case STATE_RUNNING:
    runTimerControl(now);
    break;
    case STATE_FINISHED:
    handleFinishedState(now);
    break;
    case STATE_ERROR:
    handleErrorState(now);
    break;
  }
}

// ========== ボタン読み取り + チャタリング対策 ==========
void readButtons(unsigned long now) {
  // イベントをクリア（毎ループで上書き）
  startButtonEvent = false;
  stopButtonEvent = false;

  byte startRaw = digitalRead(PIN_START_BUTTON);
  byte stopRaw = digitalRead(PIN_STOP_BUTTON);

  // 両方同時押し → STOP優先
  if (startRaw == LOW && stopRaw == LOW) {
    if (now - lastButtonMillis >= DEBOUNCE_DELAY_MS) {
    stopButtonEvent = true;
    lastButtonMillis = now;
    prevStartRaw = startRaw;
    prevStopRaw = stopRaw;
  }
  return;
}

// 状態変化がない場合は何もしない
  bool changed = (startRaw != prevStartRaw) || (stopRaw != prevStopRaw);
  if (!changed) {
    return;
  }

  // チャタリング防止
  if (now - lastButtonMillis < DEBOUNCE_DELAY_MS) {
    return;
  }

  // STARTボタン：HIGH→LOW の変化を検出
  if (prevStartRaw == HIGH && startRaw == LOW) {
    startButtonEvent = true;
  }
  // STOPボタン：HIGH→LOW の変化を検出
  if (prevStopRaw == HIGH && stopRaw == LOW) {
    stopButtonEvent = true;
  }

  prevStartRaw = startRaw;
  prevStopRaw = stopRaw;
  lastButtonMillis = now;
}

// ========== 時間設定（10 → 20 → 30 → 10） ==========
void handleTimeSetting() {
  if (currentState != STATE_IDLE) {
    return;
  }

  if (selectedMinutes == 10) {
    selectedMinutes = 20;
  } else if (selectedMinutes == 20) {
    selectedMinutes = 30;
  } else {
    selectedMinutes = 10;
  }

  logLabelU32("[SET] selectedMinutes=", selectedMinutes);
  updateOutputs();
}

// ========== STOPボタン処理（強制停止） ==========
void handleStopReset() {
  if (!stopButtonEvent) {
    return;
  }

  stopButtonEvent = false; // イベントを消費

  // どの状態でも IDLE に戻す
  if (currentState != STATE_IDLE) {
    currentState = STATE_IDLE;
    remainingSeconds = 0;
    startArmed = false;
    ledState = false;
    logText("[STOP] Motor stopped, state -> IDLE");
    updateOutputs();
  }
}

// ========== アイドル状態の処理（時間設定 + 開始） ==========
void handleIdleState(unsigned long now) {
  if (startButtonEvent) {
    if (startArmed && (now - lastStartEventMillis <= DOUBLE_CLICK_MS)) {
    // ダブルクリック：運転開始
      remainingSeconds = (unsigned int)selectedMinutes * 60;
      currentState = STATE_RUNNING;
      lastTickMillis = now;
      startArmed = false;
      logLabelU32("[STATE] RUNNING start, remainingSeconds=", remainingSeconds);
      updateOutputs();
    } else {
    // シングルクリック：時間設定変更
      handleTimeSetting();
      startArmed = true;
      lastStartEventMillis = now;
    }
    startButtonEvent = false;
  }

  // 1秒経過でダブルクリック待ち解除
  if (startArmed && (now - lastStartEventMillis > DOUBLE_CLICK_MS)) {
    startArmed = false;
  }
}

// ========== 運転中のタイマー制御 ==========
void runTimerControl(unsigned long now) {
// 異常値チェック
  if (remainingSeconds > MAX_REMAINING_SECONDS) {
    currentState = STATE_ERROR;
    logText("[ERROR] remainingSeconds out of range");
    updateOutputs();
    return;
  }

// 1秒ごとに残り時間を減らす
  if (now - lastTickMillis >= TIMER_TICK_MS) {
   lastTickMillis = now;
  if (remainingSeconds > 0) {
    remainingSeconds--;
    logLabelU32("[TIMER] remainingSeconds=", remainingSeconds);
  }
}

// 残り時間が0になったら終了
  if (remainingSeconds == 0) {
    currentState = STATE_FINISHED;
    lastBlinkMillis = now;
    ledState = false;
    logText("[STATE] FINISHED (auto stop)");
    updateOutputs();
  }
}

// ========== 終了後のLED点滅処理 ==========
void handleFinishedState(unsigned long now) {
  blinkOnFinish(now);

  // STARTボタンでアイドルに戻る
  if (startButtonEvent) {
    startButtonEvent = false;
    currentState = STATE_IDLE;
    remainingSeconds = 0;
    startArmed = false;
    logText("[STATE] IDLE (ack finish)");
    updateOutputs();
  }
}

// ========== エラー状態処理 ==========
void handleErrorState(unsigned long now) {
  digitalWrite(PIN_MOTOR_CTRL, LOW);

  if (now - lastErrorBlinkMillis >= ERROR_BLINK_INTERVAL) {
    lastErrorBlinkMillis = now;
    ledState = !ledState;
    digitalWrite(PIN_LED_STATUS, ledState ? HIGH : LOW);
  }

  // STARTボタンでエラーから復帰
  if (startButtonEvent) {
    startButtonEvent = false;
    currentState = STATE_IDLE;
    remainingSeconds = 0;
    startArmed = false;
    logText("[STATE] IDLE (recover from error)");
    updateOutputs();
  }
}

// ========== 終了後のLED点滅 ==========
void blinkOnFinish(unsigned long now) {
  if (now - lastBlinkMillis >= FINISH_BLINK_INTERVAL) {
    lastBlinkMillis = now;
    ledState = !ledState;
    digitalWrite(PIN_LED_STATUS, ledState ? HIGH : LOW);
  }
}

// ========== 長押しで工場出荷状態にリセット（長押し2秒） ==========
void factoryResetByLongPress(unsigned long now) {
  byte stopRaw = digitalRead(PIN_STOP_BUTTON);

  if (stopRaw == LOW) {
    if (stopPressStartMillis == 0) {
      stopPressStartMillis = now;
    }
  } else {
    stopPressStartMillis = 0;
    stopLongPressHandled = false;
  }

  if (!stopLongPressHandled && (stopPressStartMillis != 0) && (now - stopPressStartMillis >= LONG_PRESS_MS)) {
    selectedMinutes = 10;
    stopLongPressHandled = true;
    stopPressStartMillis = 0;

    // フィードバック：LEDを2回点滅
    for (int i = 0; i < 2; i++) {
      digitalWrite(PIN_LED_STATUS, HIGH);
      delay(120);
      digitalWrite(PIN_LED_STATUS, LOW);
      delay(120);
    }

    logText("[FACTORY RESET] selectedMinutes=10");
    updateOutputs();
  }
}

// ========== 出力更新（モータ＋LED） ==========
void updateOutputs() {
  switch (currentState) {
    case STATE_IDLE:
    digitalWrite(PIN_MOTOR_CTRL, LOW);
    digitalWrite(PIN_LED_STATUS, LOW);
    break;
    case STATE_RUNNING:
    digitalWrite(PIN_MOTOR_CTRL, HIGH);
    digitalWrite(PIN_LED_STATUS, HIGH);
    break;
    case STATE_FINISHED:
    digitalWrite(PIN_MOTOR_CTRL, LOW);
    // LEDは blinkOnFinish で制御するのでここでは何もしない
    break;
    case STATE_ERROR:
    digitalWrite(PIN_MOTOR_CTRL, LOW);
    // LEDは handleErrorState で点滅制御
    break;
  }
}