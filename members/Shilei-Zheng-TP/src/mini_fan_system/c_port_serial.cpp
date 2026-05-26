/*
 * c_port_serial.cpp — Serial 通信のみを担当する C++ ラッパー
 *
 * このファイルが「本プロジェクトで C++ がどうしても必要な唯一の理由」です。
 *
 *   Arduino の `Serial` は HardwareSerial（C++ クラス）のインスタンスで、
 *   `Serial.begin()` / `Serial.print()` などはメソッド呼び出し（C++ 構文）。
 *   C 言語にはクラスもメソッドも存在しないため、C ファイルから直接呼ぶことは
 *   不可能。そのため、この薄い C++ シムを 1 つだけ残し、外側（C 側）からは
 *   `hw_serial_xxx()` という普通の C 関数として見えるようにする。
 *
 * mini_fan_system.ino と合わせて C++ ファイルは 2 つだが、合計でも 30 行未満。
 * 業務ロジックは全て C ファイルに集約済み。
 */

#include <Arduino.h>

#include "c_port.h"

/*
 * extern "C" で囲むことで、C++ コンパイラに「これらの関数はマングルせず、
 * C と同じシンボル名で出力してくれ」と指示する。
 * これにより c_port.h を通じて C ファイルから呼べるようになる。
 */
extern "C" {

void hw_serial_begin(unsigned long baud) {
  Serial.begin(baud);
}

void hw_serial_print_text(const char *text) {
  Serial.print(text);
}

void hw_serial_print_int(int value) {
  Serial.print(value);
}

void hw_serial_println_int(int value) {
  Serial.println(value);
}

}  // extern "C"
