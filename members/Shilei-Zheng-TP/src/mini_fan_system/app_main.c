/*
 * app_main.c — Mini扇風機システムのアプリケーション層（C 言語実装）
 *
 * このファイルが、Arduino の setup() / loop() に対応する「本物のロジック」を
 * 持っている C ファイルです。
 *
 * 全体の流れ：
 *   mini_fan_system.ino (C++)
 *     └ setup() → app_setup() を呼ぶだけ
 *     └ loop()  → app_loop()  を呼ぶだけ
 *
 *   app_main.c (このファイル / C 言語)
 *     └ app_setup() : ピン初期化 + コンテキスト初期化 + 初期出力設定
 *     └ app_loop()  : 入力読取 → 状態遷移 → 出力更新 → デバッグ出力
 *
 * C 言語研修の主役はこのファイルおよびこのファイルから呼ばれる各 .c モジュールです。
 */

#include "app_main.h"

#include <stdbool.h>

#include "button_input.h"
#include "c_port.h"          /* Arduino API を C から呼ぶための薄いラッパー */
#include "debug_output.h"
#include "fan_config.h"
#include "fault_detector.h"
#include "led_output.h"
#include "motor_output.h"
#include "pot_speed.h"
#include "state_machine.h"
#include "system_context.h"

/*
 * システム全体で共有するコンテキストです（旧 .ino の g_ctx を移動）。
 * グローバル変数は通常避けたいところですが、Arduino の setup/loop モデル上、
 * 1 つだけ「現在の状態」を保持するハブを置くのが分かりやすいため、ここに集約します。
 */
static SystemContext g_ctx;

/* ===========================================================================
 * app_setup() — 起動時 1 回だけ実行される初期化処理
 *
 * 詳細設計書 §2「setup()」の実装：
 *   1. 入出力ピンの設定
 *   2. シリアル通信の開始
 *   3. SystemContext の初期化（C 言語では明示的に初期化関数を呼ぶ）
 *   4. currentState を OFF に固定（要件3-1③）
 *   5. OFF 状態に対応する初期 LED / モーター出力を確定
 * =========================================================================== */
void app_setup(void) {
  /* --- 1. ピンモードを設定する ------------------------------------------ */
  hw_pin_mode_input_pullup(PIN_BUTTON);  /* ボタンは内蔵プルアップで使う */
  hw_pin_mode_output(PIN_LED_GREEN);
  hw_pin_mode_output(PIN_LED_RED);
  hw_pin_mode_output(PIN_MOTOR);
  /* PIN_DHT11 は未配線（A02 設計のみ）のため pinMode 設定しません */

  /* --- 2. デバッグ用シリアル通信を開始する ------------------------------ */
  hw_serial_begin(9600);

  /* --- 3. SystemContext を初期化する ------------------------------------ */
  /*  C 言語では構造体メンバの一括初期化ができないため、専用の初期化関数を使います。 */
  systemContextInit(&g_ctx);

  /* --- 4. 起動直後は必ず OFF にする（要件3-1③） ------------------------ */
  /*  グローバル初期値でも STATE_OFF ですが、setup() 内で再度保証することで
   *  「電源 ON 時は必ず OFF」を明示します。                                  */
  g_ctx.currentState = STATE_OFF;

  /* --- 5. OFF 状態に対応する初期出力を確定する -------------------------- */
  hw_digital_write(PIN_LED_RED,   HW_HIGH);  /* 赤 LED 点灯（停止中） */
  hw_digital_write(PIN_LED_GREEN, HW_LOW);   /* 緑 LED 消灯           */
  hw_analog_write(PIN_MOTOR, 0);              /* モーター完全停止     */
}

/* ===========================================================================
 * app_loop() — 毎ループ実行されるメイン処理
 *
 * 詳細設計書 §2「loop()」の実装：
 *   1. 現在時刻 now を 1 回だけ取得（ループ内で統一して使う）
 *   2. ボタン押下エッジの確認（デバウンス + 起動ガード込み）
 *   3. 運転中なら つまみ値の読取・PWM 変換・異常判定
 *   4. 状態遷移（OFF / RUNNING / FAULT）
 *   5. 出力更新（モーター・LED）
 *   6. デバッグ出力（500ms 周期）
 * =========================================================================== */
void app_loop(void) {
  /* --- 1. 現在時刻 ------------------------------------------------------- */
  const unsigned long now = hw_millis();

  /* --- 2. ボタン押下エッジ ----------------------------------------------- */
  /*  buttonReady ガードにより、起動時に押しっぱなしでも誤検知しません。      */
  const bool pressed = readButtonEdge(&g_ctx, now);

  bool abnormal = false;

  /* --- 3. 運転中のみ、つまみ読取と異常判定 ------------------------------ */
  if (g_ctx.currentState == STATE_RUNNING) {
    g_ctx.potValue   = hw_analog_read(PIN_POT);
    g_ctx.motorSpeed = readPotSpeed(g_ctx.potValue, g_ctx.motorSpeed);
    abnormal         = isAbnormalReading(g_ctx.potValue, &g_ctx, now);
  } else {
    /* OFF / FAULT 中は異常判定を行いません（誤遷移防止） */
    abnormal = false;
  }

  /* --- 4. 状態遷移の更新 ------------------------------------------------- */
  /*  updateState() は遷移が起きたかどうかを bool で返し、
   *  旧状態を引数経由で受け取れる設計にしています。                          */
  int oldState = g_ctx.currentState;
  const bool changed = updateState(&g_ctx, pressed, abnormal, &oldState);

  /* 遷移があったときだけログを出力（ログ過多を防止） */
  if (changed) {
    logStateTransition(oldState, g_ctx.currentState);
  }

  /* --- 5. 出力系を更新 --------------------------------------------------- */
  updateMotor(&g_ctx);   /* 状態と速度に応じてモーター PWM を出力 */
  updateLED(&g_ctx, now); /* 状態に応じて LED を点灯・点滅       */

  /* --- 6. 周期サマリログ（500ms 周期） ----------------------------------- */
  logMainStatus(&g_ctx, now);
}
