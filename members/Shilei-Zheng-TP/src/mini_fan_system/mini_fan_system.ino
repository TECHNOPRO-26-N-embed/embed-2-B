/*
 * mini_fan_system.ino — Mini扇風機システムのエントリポイント（最小 C++ シム）
 *
 * Arduino IDE は内部で setup() / loop() を C++ リンケージで呼び出すため、
 * このファイルだけは C++ として残す必要があります。
 *
 * ただし、ここでは「ロジックを一切持たない」設計とし、
 * すべての実処理を app_main.c（C 言語）の app_setup() / app_loop() に委譲します。
 *
 * 本プロジェクトの C 言語研修では、このファイルは見るだけで OK で、
 * 編集する対象は app_main.c および各機能別 .c ファイルです。
 */

#include "app_main.h"  /* extern "C" で app_setup / app_loop を宣言 */

void setup() {
  app_setup();
}

void loop() {
  app_loop();
}
