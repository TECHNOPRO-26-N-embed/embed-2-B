/*
  温度センサーつき扇風機 - Arduino スケッチ
  詳細設計書に基づく実装（DHT11, ボタン, PWM, LED）
*/

#include <DHT.h>

// ----- ピン定義 -----
const int PIN_DHT11      = 2;   // DHT11 DATA
const int PIN_POWER_SW   = 3;   // 電源スイッチ
const int PIN_BTN_WEAK   = 4;   // 弱ボタン
const int PIN_BTN_STRONG = 5;   // 強ボタン
const int PIN_MOSFET     = 6;   // トランジスタのベース入力（PWM 出力）
const int PIN_LED_WHITE  = 13;  // 白 LED
const int PIN_LED_GREEN  = A0;  // 緑 LED
const int PIN_LED_RED    = A1;  // 赤 LED

// ----- 定数（main.c と合わせる） -----
const unsigned long DEBOUNCE_DELAY       = 50UL;
const unsigned long LONG_PRESS_MS        = 200UL;
const unsigned long SENSOR_INTERVAL      = 2000UL;
const unsigned long SENSOR_TIMEOUT       = 4000UL;
const unsigned long LED_INTERVAL         = 1000UL;
const int FILTER_WINDOW_N                = 3;
const int SENSOR_FAIL_THRESHOLD          = 3;
const int PWM_WEAK                       = 153; // 約60%
const int PWM_STRONG                     = 255; // 100%
const int TEMP_OFF_THRESHOLD             = 19;
const int TEMP_WEAK_THRESHOLD            = 20;
const int TEMP_STRONG_THRESHOLD          = 25;
const int HYSTERESIS                     = 1;
const unsigned long DEBUG_INTERVAL = 1000UL; // デバッグ出力間隔(ms)

// ----- 状態 -----
const int STATE_IDLE   = 0;
const int STATE_WEAK   = 1;
const int STATE_STRONG = 2;

// ----- グローバル（main.c と同名・同動作を目指す） -----
int currentState = STATE_IDLE;

unsigned long lastMillis_LED     = 0;
unsigned long lastMillis_Sensor  = 0;
unsigned long lastDebounceWeak   = 0;
unsigned long lastDebounceStrong = 0;
unsigned long lastDebouncePower  = 0;
unsigned long pressStartWeak = 0;
unsigned long pressStartStrong = 0;
unsigned long pressStartPower = 0;
bool weakLongHandled = false;
bool strongLongHandled = false;
bool powerLongHandled = false;

float sensorTemp = 0.0;
bool sensorValid = false;
float filteredTemp = 0.0;
float filterBuffer[FILTER_WINDOW_N];
int filterIndex = 0;
int filterCount = 0;
int sensorFailCount = 0;
unsigned long lastValidMillis = 0;
bool modeManual = false;
int motorPWM = 0;
bool buttonStateWeak = false;
bool buttonStateStrong = false;
bool powerState = false;
unsigned long lastDebugMillis = 0;
int prevState = STATE_IDLE; // 状態変化検出用

// DHT 初期化
#define DHTTYPE DHT11
DHT dht(PIN_DHT11, DHTTYPE);

// forward
void readButtons();
void handlePowerOn();
void readSensor();
void autoControl();
void updateOutput(int state);
void updateLED();
float movingAverageFilter(float v);

void setup() {
  // シリアル
  Serial.begin(9600);

  // ピン設定
  pinMode(PIN_POWER_SW, INPUT_PULLUP);
  pinMode(PIN_BTN_WEAK, INPUT_PULLUP);
  pinMode(PIN_BTN_STRONG, INPUT_PULLUP);
  pinMode(PIN_MOSFET, OUTPUT);
  pinMode(PIN_LED_WHITE, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);

  // ライブラリ初期化
  dht.begin();


  // 変数初期化
  currentState = STATE_IDLE;
  sensorValid = false;
  lastMillis_Sensor = 0;
  filterIndex = 0;
  filterCount = 0;
  for (int i = 0; i < FILTER_WINDOW_N; ++i) filterBuffer[i] = 0.0;
  sensorFailCount = 0;
  lastValidMillis = 0;
  lastDebounceWeak = lastDebounceStrong = lastDebouncePower = millis();
  // 起動時にボタン状態を読み取り、誤検出を防ぐ
  powerState = (digitalRead(PIN_POWER_SW) == LOW);
  buttonStateWeak = (digitalRead(PIN_BTN_WEAK) == LOW);
  buttonStateStrong = (digitalRead(PIN_BTN_STRONG) == LOW);
  // 初期 PWM を明示的に 0 に設定
  motorPWM = 0;
  analogWrite(PIN_MOSFET, motorPWM);
  // 起動時は手動モードに即遷移しない（ボタンはエッジで検出する）
  modeManual = false;
  currentState = STATE_IDLE;

  // 起動表示（白 LED を短く点灯）
  digitalWrite(PIN_LED_WHITE, HIGH);
  delay(600);
  digitalWrite(PIN_LED_WHITE, LOW);
  delay(600);
}

void loop() {
  unsigned long now = millis();

  // シリアル入力処理: 's' を受け取ると緊急停止
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 's' || c == 'S') {
      motorPWM = 0;
      // PWM を 0 にし、さらにデジタルで強制 LOW にしてベースを確実に落とす
      analogWrite(PIN_MOSFET, 0);
      pinMode(PIN_MOSFET, OUTPUT);
      digitalWrite(PIN_MOSFET, LOW);
      currentState = STATE_IDLE;
      modeManual = false;
      Serial.println("EMERGENCY STOP: motor PWM=0, ベースを強制LOW");
    }
  }

  readButtons();

  // センサー周期に従って読取
  if (now - lastMillis_Sensor >= SENSOR_INTERVAL) {
    readSensor();
  }

  // センサー有効性タイムアウト
  if (sensorValid && now - lastValidMillis > SENSOR_TIMEOUT) {
    sensorValid = false;
    Serial.println("Sensor timeout: invalid");
  }

  if (!modeManual) {
    autoControl();
  }

  updateOutput(currentState);
  updateLED();
  // 状態変化の検出とログ
  if (currentState != prevState) {
    Serial.print("STATE CHANGE: "); Serial.print(prevState); Serial.print(" -> "); Serial.println(currentState);
    prevState = currentState;
  }
  // 定期的にデバッグ出力（ボタン生値・状態・PWM）
  if (now - lastDebugMillis >= DEBUG_INTERVAL) {
    lastDebugMillis = now;
    Serial.print("DBG ms="); Serial.print(now);
    Serial.print(" power="); Serial.print(digitalRead(PIN_POWER_SW));
    Serial.print(" weak="); Serial.print(digitalRead(PIN_BTN_WEAK));
    Serial.print(" strong="); Serial.print(digitalRead(PIN_BTN_STRONG));
    Serial.print(" sensorValid="); Serial.print(sensorValid);
      Serial.print(" temp=");
      if (sensorValid) Serial.print(filteredTemp); else Serial.print("NaN");
    Serial.print(" state="); Serial.print(currentState);
    Serial.print(" pwm="); Serial.println(motorPWM);
  }
}

// ボタン読み取り（main.c のロジックに合わせる）
void readButtons() {
  unsigned long now = millis();

  // 電源スイッチ（INPUT_PULLUP：押下で LOW と想定）
  int rawPower = digitalRead(PIN_POWER_SW);
  bool powerPressed = (rawPower == LOW);
  if (powerPressed != powerState) {
    if (now - lastDebouncePower >= DEBOUNCE_DELAY) {
      powerState = powerPressed;
      lastDebouncePower = now;
      if (powerState) {
        // 押された開始時刻を記録（長押し判定開始）
        pressStartPower = now;
        powerLongHandled = false;
      } else {
        // リリース: 長押しフラグをリセット
        pressStartPower = 0;
        powerLongHandled = false;
        // リリース時に他に長押しがない場合は自動に戻す
        // modeManual は下で再評価
        Serial.println("Power released");
      }
    }
  }

  // 弱ボタン
  int rawWeak = digitalRead(PIN_BTN_WEAK);
  bool pressedWeak = (rawWeak == LOW);
  if (pressedWeak != buttonStateWeak) {
    if (now - lastDebounceWeak >= DEBOUNCE_DELAY) {
      buttonStateWeak = pressedWeak;
      lastDebounceWeak = now;
      if (buttonStateWeak) {
        // 押された開始時刻を記録（長押し判定開始）
        pressStartWeak = now;
        weakLongHandled = false;
      } else {
        // リリース
        pressStartWeak = 0;
        weakLongHandled = false;
        Serial.println("Weak released");
      }
    }
  }

  // 強ボタン
  int rawStrong = digitalRead(PIN_BTN_STRONG);
  bool pressedStrong = (rawStrong == LOW);
  if (pressedStrong != buttonStateStrong) {
    if (now - lastDebounceStrong >= DEBOUNCE_DELAY) {
      buttonStateStrong = pressedStrong;
      lastDebounceStrong = now;
      if (buttonStateStrong) {
        // 押された開始時刻を記録（長押し判定開始）
        pressStartStrong = now;
        strongLongHandled = false;
      } else {
        // リリース
        pressStartStrong = 0;
        strongLongHandled = false;
        Serial.println("Strong released");
      }
    }
  }

  // 長押し判定処理（押下後に LONG_PRESS_MS を越えたら確定動作）
  if (buttonStateWeak && !weakLongHandled && pressStartWeak > 0 && now - pressStartWeak >= LONG_PRESS_MS) {
    weakLongHandled = true;
    // 押下による手動弱風確定
    modeManual = true;
    currentState = STATE_WEAK;
    Serial.println("Button WEAK long-press: manual WEAK");
  }
  if (buttonStateStrong && !strongLongHandled && pressStartStrong > 0 && now - pressStartStrong >= LONG_PRESS_MS) {
    strongLongHandled = true;
    // 押下による手動強風確定
    modeManual = true;
    currentState = STATE_STRONG;
    Serial.println("Button STRONG long-press: manual STRONG");
  }
  if (powerState && !powerLongHandled && pressStartPower > 0 && now - pressStartPower >= LONG_PRESS_MS) {
    powerLongHandled = true;
    // 電源長押しで手動弱風
    modeManual = true;
    currentState = STATE_WEAK;
    Serial.println("Power long-press: manual WEAK");
  }

  // 手動モード判定: 長押しで確定したものがある間は手動モード
  modeManual = powerLongHandled || weakLongHandled || strongLongHandled;
}

void readSensor() {
  unsigned long now = millis();
  lastMillis_Sensor = now;
  float t = dht.readTemperature();
  if (!isnan(t)) {
    sensorTemp = t;
    sensorValid = true;
    lastValidMillis = now;
    sensorFailCount = 0;
    filteredTemp = movingAverageFilter(sensorTemp);
    Serial.print("Temp: "); Serial.print(sensorTemp); Serial.print(" => filt: "); Serial.println(filteredTemp);
  } else {
    sensorFailCount++;
    Serial.println("DHT read failed");
    if (sensorFailCount >= SENSOR_FAIL_THRESHOLD) {
      sensorValid = false;
      Serial.println("Sensor fails exceeded");
      // 自動モード時のみフォールバックして弱動作にする
      if (!modeManual) {
        currentState = STATE_WEAK;
        motorPWM = PWM_WEAK;
        analogWrite(PIN_MOSFET, motorPWM);
        Serial.println("fallback to WEAK (auto mode)");
      } else {
        Serial.println("manual mode: ignoring fallback");
      }
    }
  }
}

void autoControl() {
  if (!sensorValid) return;
  float t = filteredTemp;

  // 強モードからの降下判定（ヒステリシス適用）
  if (currentState == STATE_STRONG) {
    if (t < (TEMP_STRONG_THRESHOLD - HYSTERESIS)) {
      if (t >= TEMP_WEAK_THRESHOLD) currentState = STATE_WEAK; else currentState = STATE_IDLE;
    }
    return;
  }

  if (t >= TEMP_STRONG_THRESHOLD) {
    currentState = STATE_STRONG;
  } else if (t >= TEMP_WEAK_THRESHOLD) {
    currentState = STATE_WEAK;
  } else if (t <= TEMP_OFF_THRESHOLD) {
    currentState = STATE_IDLE;
  }
}

void updateOutput(int state) {
  int newPWM = 0;
  switch (state) {
    case STATE_IDLE:
      newPWM = 0;
      digitalWrite(PIN_LED_WHITE, LOW);
      digitalWrite(PIN_LED_GREEN, LOW);
      digitalWrite(PIN_LED_RED, LOW);
      break;
    case STATE_WEAK:
      newPWM = PWM_WEAK;
      digitalWrite(PIN_LED_WHITE, HIGH);
      digitalWrite(PIN_LED_GREEN, HIGH);
      digitalWrite(PIN_LED_RED, LOW);
      break;
    case STATE_STRONG:
      newPWM = PWM_STRONG;
      digitalWrite(PIN_LED_WHITE, HIGH);
      digitalWrite(PIN_LED_GREEN, LOW);
      digitalWrite(PIN_LED_RED, HIGH);
      break;
  }
  // clamp
  if (newPWM < 0) newPWM = 0; if (newPWM > 255) newPWM = 255;
  if (newPWM != motorPWM) {
    motorPWM = newPWM;
    if (motorPWM == 0) {
      // PWM 0 のときはアナログ PWM を解除して強制 LOW
      analogWrite(PIN_MOSFET, 0);
      pinMode(PIN_MOSFET, OUTPUT);
      digitalWrite(PIN_MOSFET, LOW);
    } else {
      pinMode(PIN_MOSFET, OUTPUT);
      analogWrite(PIN_MOSFET, motorPWM);
    }
    Serial.print("Set PWM: "); Serial.println(motorPWM);
  }
}

void updateLED() {
  unsigned long now = millis();
  // 手動モードなら白LEDを常時点灯
  if (modeManual) {
    digitalWrite(PIN_LED_WHITE, HIGH);
  } else {
    // 自動モードでは1秒間隔で白LEDを点滅
    if (now - lastMillis_LED >= LED_INTERVAL) {
      lastMillis_LED = now;
      int v = digitalRead(PIN_LED_WHITE);
      digitalWrite(PIN_LED_WHITE, !v);
    }
  }
}

float movingAverageFilter(float v) {
  filterBuffer[filterIndex] = v;
  filterIndex = (filterIndex + 1) % FILTER_WINDOW_N;
  if (filterCount < FILTER_WINDOW_N) filterCount++;
  float sum = 0.0;
  for (int i = 0; i < filterCount; ++i) sum += filterBuffer[i];
  return sum / filterCount;
}
