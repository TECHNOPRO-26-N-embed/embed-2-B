#ifndef FAN_CONFIG_H
#define FAN_CONFIG_H

/*
 * C 言語版向けの共通定数定義。
 * すべてマクロ化しておくことで、.c / .cpp の両方から同じ値を参照できます。
 */

/* ------------------------------
 * ピン定義
 * ------------------------------ */
#define PIN_BUTTON      2  /* タクトスイッチ（INPUT_PULLUP: 押下で LOW） */
#define PIN_LED_GREEN   5  /* 緑 LED（運転中） */
#define PIN_LED_RED     6  /* 赤 LED（停止・異常） */
#define PIN_MOTOR       9  /* モーター PWM 出力 */
#define PIN_POT         14 /* A0 を数値化（UNO では A0=14） */
#define PIN_DHT11       7  /* 将来拡張用（A02） */

/* ------------------------------
 * 状態定義
 * ------------------------------ */
#define STATE_OFF       0
#define STATE_RUNNING   1
#define STATE_FAULT     2

/* ------------------------------
 * 動作定数
 * ------------------------------ */
#define DEBOUNCE_MS     50UL
#define BLINK_MS        250UL
#define DEBUG_MS        500UL
#define FAULT_MS        1000UL

#define MOTOR_MIN_PWM   45
#define POT_DEADBAND    3

#endif
