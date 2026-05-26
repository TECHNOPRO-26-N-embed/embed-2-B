#ifndef C_PORT_H
#define C_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Arduino API を C 言語モジュールから呼ぶための薄いラッパー層。
 * C ファイルはこのヘッダだけを使い、Arduino.h へ直接依存しません。
 */

#define HW_LOW   0
#define HW_HIGH  1

void hw_pin_mode_input_pullup(int pin);
void hw_pin_mode_output(int pin);

int hw_digital_read(int pin);
void hw_digital_write(int pin, int level);

int hw_analog_read(int pin);
void hw_analog_write(int pin, int value);

unsigned long hw_millis(void);

void hw_serial_begin(unsigned long baud);
void hw_serial_print_text(const char *text);
void hw_serial_print_int(int value);
void hw_serial_println_int(int value);

#ifdef __cplusplus
}
#endif

#endif
