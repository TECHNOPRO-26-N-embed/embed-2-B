#ifndef MOCK_C_PORT_H
#define MOCK_C_PORT_H

/*
 * mock_c_port.h — PC 上で Arduino API を再現するためのテスト用補助 API
 *
 * c_port.h で定義された hw_* 関数群を PC で実装し、
 * かつテストドライバ側から「現在時刻」「ピンの読み取り値」を
 * 自由に設定できるようにする。
 */

#define MOCK_MAX_PINS 32

/* 全状態を初期値に戻す（各シナリオ冒頭で呼ぶ） */
void mock_reset(void);

/* hw_millis() が返す値を設定 */
void mock_set_millis(unsigned long ms);

/* hw_digital_read(pin) が返す値を設定（HW_HIGH / HW_LOW） */
void mock_set_digital(int pin, int level);

/* hw_analog_read(pin) が返す値を設定（0〜1023） */
void mock_set_analog(int pin, int value);

/* 直近に書き込まれた値の取得（出力検証用） */
int mock_get_digital_write(int pin);
int mock_get_analog_write(int pin);

#endif /* MOCK_C_PORT_H */
