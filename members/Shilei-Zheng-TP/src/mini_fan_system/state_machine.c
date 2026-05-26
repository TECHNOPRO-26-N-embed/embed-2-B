#include "state_machine.h"

bool updateState(SystemContext *ctx, bool pressed, bool abnormal, int *oldState) {
  if (ctx == 0 || oldState == 0) {
    return false;
  }

  *oldState = ctx->currentState;

  if (ctx->currentState == STATE_OFF) {
    if (pressed) {
      ctx->currentState = STATE_RUNNING;
    }
  } else if (ctx->currentState == STATE_RUNNING) {
    /* 異常を優先して安全側へ遷移 */
    if (abnormal) {
      ctx->currentState = STATE_FAULT;
      ctx->motorSpeed = 0;
    } else if (pressed) {
      ctx->currentState = STATE_OFF;
      ctx->motorSpeed = 0;

      /* 手動停止時に異常タイマー持ち越しを防ぐ */
      ctx->abnormalStartMillis = 0;
      ctx->abnormalCount = 0;
    }
  } else if (ctx->currentState == STATE_FAULT) {
    if (pressed) {
      ctx->currentState = STATE_OFF;
      ctx->motorSpeed = 0;
      ctx->abnormalCount = 0;
      ctx->abnormalStartMillis = 0;
    }
  }

  return (*oldState != ctx->currentState);
}
