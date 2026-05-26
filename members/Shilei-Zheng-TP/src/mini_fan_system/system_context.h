#ifndef SYSTEM_CONTEXT_H
#define SYSTEM_CONTEXT_H

#include <stdbool.h>
#include <stdint.h>

#include "fan_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 実行中に変化する値を 1 つにまとめた構造体です。
 * C 言語ではメンバ初期化できないため、初期値は systemContextInit() で設定します。
 */
typedef struct SystemContext {
  int currentState;

  int potValue;
  int motorSpeed;

  unsigned long lastButtonMillis;
  bool lastButtonState;
  bool buttonReady;

  unsigned long abnormalStartMillis;
  int abnormalCount;

  unsigned long lastLedToggleMillis;
  bool ledBlinkState;

  unsigned long lastDebugMillis;
} SystemContext;

/* 構造体の初期値をまとめて設定します。 */
void systemContextInit(SystemContext *ctx);

#ifdef __cplusplus
}
#endif

#endif
