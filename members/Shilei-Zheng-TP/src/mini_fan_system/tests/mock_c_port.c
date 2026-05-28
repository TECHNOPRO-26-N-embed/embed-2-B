/*
 * mock_c_port.c — c_port.h の hw_* 関数を PC 上で実装するモック
 *
 * Arduino.h は使わず、stdlib のみで実装する。
 * テストドライバ（test_main.c）から mock_set_* で内部状態を操作できる。
 */

#include "c_port.h"          /* 本番と同じヘッダを使う */
#include "mock_c_port.h"

#include <stdio.h>

static unsigned long s_millis = 0;
static int s_digital_read [MOCK_MAX_PINS];
static int s_analog_read  [MOCK_MAX_PINS];
static int s_digital_write[MOCK_MAX_PINS];
static int s_analog_write [MOCK_MAX_PINS];

/* -------------------- テスト用補助 API -------------------- */

void mock_reset(void) {
  int i;
  s_millis = 0;
  for (i = 0; i < MOCK_MAX_PINS; i++) {
    s_digital_read [i] = HW_HIGH;  /* INPUT_PULLUP の既定は HIGH */
    s_analog_read  [i] = 0;
    s_digital_write[i] = -1;
    s_analog_write [i] = -1;
  }
}

void mock_set_millis(unsigned long ms) {
  s_millis = ms;
}

void mock_set_digital(int pin, int level) {
  if (pin < 0 || pin >= MOCK_MAX_PINS) return;
  s_digital_read[pin] = level;
}

void mock_set_analog(int pin, int value) {
  if (pin < 0 || pin >= MOCK_MAX_PINS) return;
  s_analog_read[pin] = value;
}

int mock_get_digital_write(int pin) {
  if (pin < 0 || pin >= MOCK_MAX_PINS) return -1;
  return s_digital_write[pin];
}

int mock_get_analog_write(int pin) {
  if (pin < 0 || pin >= MOCK_MAX_PINS) return -1;
  return s_analog_write[pin];
}

/* -------------------- c_port.h の実装 -------------------- */

void hw_pin_mode_input_pullup(int pin) {
  (void)pin;
  /* PC モックでは何もしない */
}

void hw_pin_mode_output(int pin) {
  (void)pin;
}

int hw_digital_read(int pin) {
  if (pin < 0 || pin >= MOCK_MAX_PINS) return HW_HIGH;
  return s_digital_read[pin];
}

void hw_digital_write(int pin, int level) {
  if (pin < 0 || pin >= MOCK_MAX_PINS) return;
  s_digital_write[pin] = level;
}

int hw_analog_read(int pin) {
  if (pin < 0 || pin >= MOCK_MAX_PINS) return 0;
  return s_analog_read[pin];
}

void hw_analog_write(int pin, int value) {
  if (pin < 0 || pin >= MOCK_MAX_PINS) return;
  s_analog_write[pin] = value;
}

unsigned long hw_millis(void) {
  return s_millis;
}

void hw_serial_begin(unsigned long baud) {
  (void)baud;
}

void hw_serial_print_text(const char *text) {
  if (text) fputs(text, stdout);
}

void hw_serial_print_int(int value) {
  printf("%d", value);
}

void hw_serial_println_int(int value) {
  printf("%d\n", value);
}
