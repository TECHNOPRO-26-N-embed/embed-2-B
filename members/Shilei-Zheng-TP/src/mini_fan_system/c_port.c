/*
 * c_port.c — Arduino API（GPIO / アナログ / 時刻）の C 言語ラッパー
 *
 * このファイルの役割：
 *   pinMode / digitalRead / digitalWrite / analogRead / analogWrite / millis
 *   といった Arduino の基本 API を、C 言語のモジュールから安全に呼べるように
 *   薄くラップする。
 *
 * 「なぜ Arduino.h を include しないのか？」
 *   Arduino.h は HardwareSerial（C++ クラス）など、C コンパイラで処理できない
 *   宣言を多数含むため、C ファイルからは直接読めない。
 *   代わりに、必要な関数だけを extern で前方宣言する。
 *
 *   実は pinMode / digitalRead 等は Arduino core 内部で C 言語（.c ファイル）
 *   として実装されているため、こうして名前を宣言すれば C から直接呼べる。
 *
 * リスク（README にも記載）：
 *   Arduino core 側で関数のシグネチャ（引数や型）が変わると、リンクが壊れる。
 *   現行の Arduino AVR core では長年シグネチャが変わっていないため、
 *   実用上の問題はほぼ起きない。
 *   ただし Arduino IDE を大幅にアップグレードした場合は要確認。
 *
 * Serial 関連の関数は HardwareSerial（C++ クラス）に依存するため、
 * このファイルには含めない（c_port_serial.cpp 側で処理する）。
 */

#include <stdint.h>

#include "c_port.h"

/* ------------------------------------------------------------------
 * Arduino core 内の C 関数を前方宣言
 * （cores/arduino/wiring_digital.c / wiring.c / wiring_analog.c 由来）
 * ------------------------------------------------------------------ */
extern void          pinMode(uint8_t pin, uint8_t mode);
extern int           digitalRead(uint8_t pin);
extern void          digitalWrite(uint8_t pin, uint8_t val);
extern int           analogRead(uint8_t pin);
extern void          analogWrite(uint8_t pin, int val);
extern unsigned long millis(void);

/* ------------------------------------------------------------------
 * Arduino のピンモード・レベル定数の値を、Arduino.h と一致するように
 * ローカル定義する（Arduino.h を include できないため）。
 *
 *   OUTPUT       = 0x1
 *   INPUT_PULLUP = 0x2
 *   LOW          = 0x0
 *   HIGH         = 0x1
 * ------------------------------------------------------------------ */
#define ARD_OUTPUT        0x1
#define ARD_INPUT_PULLUP  0x2
#define ARD_LOW           0x0
#define ARD_HIGH          0x1

/* ------------------------------------------------------------------
 * ピンモード設定
 * ------------------------------------------------------------------ */
void hw_pin_mode_input_pullup(int pin) {
  /* 内蔵プルアップを有効化したデジタル入力にする */
  pinMode((uint8_t)pin, ARD_INPUT_PULLUP);
}

void hw_pin_mode_output(int pin) {
  /* デジタル出力（HIGH / LOW を出せる）にする */
  pinMode((uint8_t)pin, ARD_OUTPUT);
}

/* ------------------------------------------------------------------
 * デジタル入出力
 * ------------------------------------------------------------------ */
int hw_digital_read(int pin) {
  /* HIGH / LOW を 1 / 0 として返す */
  return digitalRead((uint8_t)pin);
}

void hw_digital_write(int pin, int level) {
  /* 0 を LOW、それ以外を HIGH として書き込む */
  digitalWrite((uint8_t)pin, (level == 0) ? ARD_LOW : ARD_HIGH);
}

/* ------------------------------------------------------------------
 * アナログ入出力
 * ------------------------------------------------------------------ */
int hw_analog_read(int pin) {
  /* 10bit ADC 結果（0〜1023）を int で返す */
  return analogRead((uint8_t)pin);
}

void hw_analog_write(int pin, int value) {
  /* PWM デューティ（0〜255）を出力する */
  analogWrite((uint8_t)pin, value);
}

/* ------------------------------------------------------------------
 * 経過時刻
 * ------------------------------------------------------------------ */
unsigned long hw_millis(void) {
  /* 起動からの経過ミリ秒（unsigned long でラップアラウンドあり） */
  return millis();
}
