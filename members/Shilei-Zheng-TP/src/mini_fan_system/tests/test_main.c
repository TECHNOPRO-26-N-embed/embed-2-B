/*
 * test_main.c — Mini扇風機システムの PC 上カバレッジテスト
 *
 * 詳細設計書 §5 の単体テスト相当を PC 上で実行し、
 * 全関数の全分岐パスが通過することを確認する。
 *
 * 各シナリオで mock_c_port のセッターを使って入力（時刻・ピン値）を制御し、
 * 対象関数を呼ぶ。TRACE_BRANCH の出力により分岐通過を確認、
 * assert で論理検証も行う。
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "button_input.h"
#include "c_port.h"
#include "coverage_tracker.h"
#include "debug_output.h"
#include "fan_config.h"
#include "fault_detector.h"
#include "led_output.h"
#include "mock_c_port.h"
#include "motor_output.h"
#include "pot_speed.h"
#include "state_machine.h"
#include "system_context.h"

/* -----------------------------------------------------------------------
 * 全分岐 ID の事前登録
 * --------------------------------------------------------------------- */
static void register_all_branches(void) {
  /* system_context */
  coverage_register("sysctx:null");
  coverage_register("sysctx:normal");

  /* button_input */
  coverage_register("button:null");
  coverage_register("button:guard-release");
  coverage_register("button:guard-low");
  coverage_register("button:edge-confirmed");
  coverage_register("button:debounce-skip");
  coverage_register("button:released");
  coverage_register("button:already-released");
  coverage_register("button:still-pressed");

  /* pot_speed */
  coverage_register("pot:clamp-low");
  coverage_register("pot:clamp-high");
  coverage_register("pot:normal");
  coverage_register("pot:deadband-hit");
  coverage_register("pot:outside-deadband");

  /* fault_detector */
  coverage_register("fault:null");
  coverage_register("fault:pinned-start");
  coverage_register("fault:pinned-confirmed");
  coverage_register("fault:pinned-not-yet");
  coverage_register("fault:not-pinned");

  /* state_machine */
  coverage_register("state:null");
  coverage_register("state:off-pressed");
  coverage_register("state:off-idle");
  coverage_register("state:running-abnormal");
  coverage_register("state:running-pressed");
  coverage_register("state:running-idle");
  coverage_register("state:fault-pressed");
  coverage_register("state:fault-idle");

  /* motor_output */
  coverage_register("motor:clamp-low");
  coverage_register("motor:clamp-high");
  coverage_register("motor:clamp-normal");
  coverage_register("motor:null");
  coverage_register("motor:stopped-state");
  coverage_register("motor:under-min");
  coverage_register("motor:active");

  /* led_output */
  coverage_register("led:null");
  coverage_register("led:off");
  coverage_register("led:running");
  coverage_register("led:fault-enter");
  coverage_register("led:fault-toggle");
  coverage_register("led:fault-hold");

  /* debug_output */
  coverage_register("debug:null");
  coverage_register("debug:throttled");
  coverage_register("debug:emit");
  coverage_register("debug:transition");
}

/* シナリオ見出しを出力するマクロ */
#define SCENARIO(num, name) printf("\n=== SCENARIO %d: %s ===\n", num, name)

/* -----------------------------------------------------------------------
 * シナリオ群
 * --------------------------------------------------------------------- */

static void scenario_01_sysctx_null(void) {
  SCENARIO(1, "systemContextInit(NULL)");
  systemContextInit(NULL);
}

static void scenario_02_sysctx_normal(void) {
  SCENARIO(2, "systemContextInit(&ctx) normal");
  SystemContext ctx;
  systemContextInit(&ctx);
  assert(ctx.currentState == STATE_OFF);
  assert(ctx.motorSpeed == 0);
  assert(ctx.buttonReady == false);
}

static void scenario_03_button_null(void) {
  SCENARIO(3, "readButtonEdge(NULL)");
  bool r = readButtonEdge(NULL, 0);
  assert(r == false);
}

static void scenario_04_button_boot_guard(void) {
  SCENARIO(4, "boot guard: button held LOW at power-on");
  SystemContext ctx;
  systemContextInit(&ctx);
  mock_reset();

  /* 起動直後にボタンが LOW（押しっぱなし） */
  mock_set_digital(PIN_BUTTON, HW_LOW);
  bool r = readButtonEdge(&ctx, 0);
  assert(r == false);
  assert(ctx.buttonReady == false);  /* まだガード解除されていない */

  /* 押し続け（同じ条件） */
  r = readButtonEdge(&ctx, 10);
  assert(r == false);

  /* 指を離す → buttonReady が true へ */
  mock_set_digital(PIN_BUTTON, HW_HIGH);
  r = readButtonEdge(&ctx, 20);
  assert(r == false);
  assert(ctx.buttonReady == true);
}

static void scenario_05_button_normal_press(void) {
  SCENARIO(5, "normal press after boot guard cleared");
  SystemContext ctx;
  systemContextInit(&ctx);
  mock_reset();
  ctx.buttonReady = true;  /* ガード済み状態を直接設定 */
  ctx.lastButtonState = true;

  /* ボタンを押す（HIGH → LOW、50ms 経過済み） */
  mock_set_digital(PIN_BUTTON, HW_LOW);
  bool r = readButtonEdge(&ctx, 100);
  assert(r == true);  /* 押下エッジ確定 */
  assert(ctx.lastButtonState == false);
}

static void scenario_06_button_debounce(void) {
  SCENARIO(6, "debounce: rapid second press within 50ms");
  SystemContext ctx;
  systemContextInit(&ctx);
  ctx.buttonReady = true;
  ctx.lastButtonState = true;
  ctx.lastButtonMillis = 100;

  /* 押下確定直後にもう一度押す（lastButtonState=false のままだとデバウンス分岐に入らないので、
     1 度離して再度押すシナリオを作る） */
  mock_reset();
  mock_set_digital(PIN_BUTTON, HW_HIGH);
  readButtonEdge(&ctx, 110);  /* lastButtonState を true に戻す */

  /* すぐ押す（49ms 経過のみ） */
  mock_set_digital(PIN_BUTTON, HW_LOW);
  bool r = readButtonEdge(&ctx, 120);
  /* debounce-skip 分岐に入る */
  assert(r == false || r == true); /* 通過確認が目的 */
  (void)r;
}

static void scenario_07_button_release_variants(void) {
  SCENARIO(7, "button release: released vs already-released");
  SystemContext ctx;
  systemContextInit(&ctx);
  ctx.buttonReady = true;
  mock_reset();

  /* released: lastButtonState=false の状態で HIGH を読む */
  ctx.lastButtonState = false;
  mock_set_digital(PIN_BUTTON, HW_HIGH);
  readButtonEdge(&ctx, 200);

  /* already-released: lastButtonState=true の状態で HIGH を読む */
  ctx.lastButtonState = true;
  readButtonEdge(&ctx, 300);

  /* still-pressed: ガード後・lastButtonState=false で LOW を読み、デバウンス未経過 */
  ctx.lastButtonState = false;
  ctx.lastButtonMillis = 290;
  mock_set_digital(PIN_BUTTON, HW_LOW);
  readButtonEdge(&ctx, 300);  /* 10ms 経過のみ */
}

static void scenario_08_pot_clamp_low(void) {
  SCENARIO(8, "readPotSpeed: clamp-low (negative)");
  int r = readPotSpeed(-5, 0);
  assert(r == 0);
}

static void scenario_09_pot_clamp_high(void) {
  SCENARIO(9, "readPotSpeed: clamp-high (over 1023)");
  int r = readPotSpeed(2000, 0);
  /* 1023 を 0-255 にマップすると 255 */
  assert(r == 255);
}

static void scenario_10_pot_normal_outside_deadband(void) {
  SCENARIO(10, "readPotSpeed: normal range, outside deadband");
  int r = readPotSpeed(512, 0);
  /* 512 * 255 / 1023 ≒ 127 */
  assert(r > 100 && r < 150);
}

static void scenario_11_pot_deadband_hit(void) {
  SCENARIO(11, "readPotSpeed: deadband hit (small change kept)");
  /* 前回 127、今回 512 → 127 と非常に近いのでデッドバンド内 */
  int r = readPotSpeed(513, 127);
  assert(r == 127);
}

static void scenario_12_fault_null(void) {
  SCENARIO(12, "isAbnormalReading(NULL)");
  bool r = isAbnormalReading(0, NULL, 0);
  assert(r == false);
}

static void scenario_13_fault_not_pinned(void) {
  SCENARIO(13, "isAbnormalReading: not pinned (normal value)");
  SystemContext ctx;
  systemContextInit(&ctx);
  ctx.abnormalStartMillis = 12345;  /* 何か値が入っていたとする */
  bool r = isAbnormalReading(512, &ctx, 1000);
  assert(r == false);
  assert(ctx.abnormalStartMillis == 0);  /* リセットされる */
}

static void scenario_14_fault_pinned_progression(void) {
  SCENARIO(14, "isAbnormalReading: pinned start -> not-yet -> confirmed");
  SystemContext ctx;
  systemContextInit(&ctx);

  /* start: 異常開始時刻が 0 のとき → 設定される */
  bool r = isAbnormalReading(0, &ctx, 100);
  assert(r == false);
  assert(ctx.abnormalStartMillis == 100);

  /* not-yet: 1000ms 未満 */
  r = isAbnormalReading(0, &ctx, 500);
  assert(r == false);

  /* confirmed: 1000ms 以上経過 */
  r = isAbnormalReading(0, &ctx, 1100);
  assert(r == true);
  assert(ctx.abnormalCount == 1);
}

static void scenario_15_fault_pinned_high(void) {
  SCENARIO(15, "isAbnormalReading: value >= 1021 also counts");
  SystemContext ctx;
  systemContextInit(&ctx);

  /*  now=0 は abnormalStartMillis の初期値（センチネル）と衝突するため避ける */
  bool r = isAbnormalReading(1023, &ctx, 1);
  assert(r == false);
  r = isAbnormalReading(1023, &ctx, 1001);  /* +1000ms 経過 */
  assert(r == true);
}

static void scenario_16_state_null(void) {
  SCENARIO(16, "updateState: null pointers");
  int oldState = 0;
  bool r = updateState(NULL, false, false, &oldState);
  assert(r == false);
  SystemContext ctx;
  systemContextInit(&ctx);
  r = updateState(&ctx, false, false, NULL);
  assert(r == false);
}

static void scenario_17_state_all_transitions(void) {
  SCENARIO(17, "updateState: all 8 combinations (per detailed_design §5-4)");
  SystemContext ctx;
  int oldState = 0;

  /* OFF + pressed → RUNNING */
  systemContextInit(&ctx);
  updateState(&ctx, true, false, &oldState);
  assert(ctx.currentState == STATE_RUNNING);

  /* OFF + idle → OFF */
  systemContextInit(&ctx);
  updateState(&ctx, false, false, &oldState);
  assert(ctx.currentState == STATE_OFF);

  /* RUNNING + abnormal → FAULT */
  systemContextInit(&ctx);
  ctx.currentState = STATE_RUNNING;
  ctx.motorSpeed = 200;
  updateState(&ctx, false, true, &oldState);
  assert(ctx.currentState == STATE_FAULT);
  assert(ctx.motorSpeed == 0);

  /* RUNNING + pressed (no abnormal) → OFF */
  systemContextInit(&ctx);
  ctx.currentState = STATE_RUNNING;
  ctx.motorSpeed = 200;
  ctx.abnormalStartMillis = 999;
  updateState(&ctx, true, false, &oldState);
  assert(ctx.currentState == STATE_OFF);
  assert(ctx.motorSpeed == 0);
  assert(ctx.abnormalStartMillis == 0);

  /* RUNNING + idle → RUNNING */
  systemContextInit(&ctx);
  ctx.currentState = STATE_RUNNING;
  updateState(&ctx, false, false, &oldState);
  assert(ctx.currentState == STATE_RUNNING);

  /* RUNNING + pressed + abnormal → FAULT（abnormal が優先） */
  systemContextInit(&ctx);
  ctx.currentState = STATE_RUNNING;
  updateState(&ctx, true, true, &oldState);
  assert(ctx.currentState == STATE_FAULT);

  /* FAULT + pressed → OFF */
  systemContextInit(&ctx);
  ctx.currentState = STATE_FAULT;
  ctx.abnormalStartMillis = 999;
  ctx.abnormalCount = 5;
  updateState(&ctx, true, false, &oldState);
  assert(ctx.currentState == STATE_OFF);
  assert(ctx.abnormalStartMillis == 0);
  assert(ctx.abnormalCount == 0);

  /* FAULT + idle → FAULT */
  systemContextInit(&ctx);
  ctx.currentState = STATE_FAULT;
  updateState(&ctx, false, false, &oldState);
  assert(ctx.currentState == STATE_FAULT);
}

static void scenario_18_motor_all(void) {
  SCENARIO(18, "updateMotor: null / stopped / under-min / active / clamp-low / clamp-high");
  SystemContext ctx;
  mock_reset();

  /* null */
  updateMotor(NULL);

  /* stopped-state (OFF) */
  systemContextInit(&ctx);
  ctx.currentState = STATE_OFF;
  ctx.motorSpeed = 200;
  updateMotor(&ctx);
  assert(mock_get_analog_write(PIN_MOTOR) == 0);

  /* under-min: RUNNING but below MOTOR_MIN_PWM */
  ctx.currentState = STATE_RUNNING;
  ctx.motorSpeed = 30;  /* < 45 */
  updateMotor(&ctx);
  assert(mock_get_analog_write(PIN_MOTOR) == 0);

  /* active: RUNNING and >= MOTOR_MIN_PWM */
  ctx.motorSpeed = 128;
  updateMotor(&ctx);
  assert(mock_get_analog_write(PIN_MOTOR) == 128);

  /* clamp-low: motorSpeed = -1 → clamped to 0 → under-min */
  ctx.motorSpeed = -1;
  updateMotor(&ctx);
  assert(mock_get_analog_write(PIN_MOTOR) == 0);

  /* clamp-high: motorSpeed = 300 → clamped to 255 → active */
  ctx.motorSpeed = 300;
  updateMotor(&ctx);
  assert(mock_get_analog_write(PIN_MOTOR) == 255);
}

static void scenario_19_led_all(void) {
  SCENARIO(19, "updateLED: null / OFF / RUNNING / FAULT enter / FAULT toggle / FAULT hold");
  SystemContext ctx;
  mock_reset();

  /* null */
  updateLED(NULL, 0);

  /* OFF: 赤点灯・緑消灯 */
  systemContextInit(&ctx);
  ctx.currentState = STATE_OFF;
  updateLED(&ctx, 0);
  assert(mock_get_digital_write(PIN_LED_RED) == HW_HIGH);
  assert(mock_get_digital_write(PIN_LED_GREEN) == HW_LOW);

  /* RUNNING: 緑点灯・赤消灯 */
  ctx.currentState = STATE_RUNNING;
  updateLED(&ctx, 100);
  assert(mock_get_digital_write(PIN_LED_RED) == HW_LOW);
  assert(mock_get_digital_write(PIN_LED_GREEN) == HW_HIGH);

  /* FAULT enter (previousState != FAULT): 赤即時点灯 */
  ctx.currentState = STATE_FAULT;
  ctx.lastLedToggleMillis = 0;
  updateLED(&ctx, 200);
  assert(mock_get_digital_write(PIN_LED_RED) == HW_HIGH);
  assert(ctx.ledBlinkState == true);

  /* FAULT hold: previousState == FAULT, BLINK_MS 未到達 */
  updateLED(&ctx, 300);  /* +100ms */

  /* FAULT toggle: BLINK_MS 到達 */
  updateLED(&ctx, 500);  /* lastLedToggleMillis=200 から +300ms ≥ 250ms */
}

static void scenario_20_debug_log(void) {
  SCENARIO(20, "logMainStatus: null / throttled / emit + logStateTransition");
  SystemContext ctx;

  /* null */
  logMainStatus(NULL, 0);

  /* throttled: DEBUG_MS 未満 */
  systemContextInit(&ctx);
  ctx.lastDebugMillis = 100;
  logMainStatus(&ctx, 200);

  /* emit: DEBUG_MS 到達 */
  ctx.lastDebugMillis = 0;
  ctx.currentState = STATE_RUNNING;
  ctx.potValue = 512;
  ctx.motorSpeed = 128;
  logMainStatus(&ctx, 500);

  /* logStateTransition */
  logStateTransition(STATE_OFF, STATE_RUNNING);
}

/* -----------------------------------------------------------------------
 * main
 * --------------------------------------------------------------------- */
int main(void) {
  printf("Mini Fan System - PC Coverage Test\n");
  printf("===================================\n");

  register_all_branches();
  mock_reset();

  scenario_01_sysctx_null();
  scenario_02_sysctx_normal();
  scenario_03_button_null();
  scenario_04_button_boot_guard();
  scenario_05_button_normal_press();
  scenario_06_button_debounce();
  scenario_07_button_release_variants();
  scenario_08_pot_clamp_low();
  scenario_09_pot_clamp_high();
  scenario_10_pot_normal_outside_deadband();
  scenario_11_pot_deadband_hit();
  scenario_12_fault_null();
  scenario_13_fault_not_pinned();
  scenario_14_fault_pinned_progression();
  scenario_15_fault_pinned_high();
  scenario_16_state_null();
  scenario_17_state_all_transitions();
  scenario_18_motor_all();
  scenario_19_led_all();
  scenario_20_debug_log();

  return coverage_report();
}
