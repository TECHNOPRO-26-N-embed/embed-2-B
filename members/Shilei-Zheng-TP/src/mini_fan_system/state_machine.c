#include "state_machine.h"

#include "tests/branch_trace.h"

bool updateState(SystemContext *ctx, bool pressed, bool abnormal, int *oldState) {
  if (ctx == 0 || oldState == 0) {
    TRACE_BRANCH("state:null");
    return false;
  }

  *oldState = ctx->currentState;

  if (ctx->currentState == STATE_OFF) {
    if (pressed) {
      TRACE_BRANCH("state:off-pressed");
      ctx->currentState = STATE_RUNNING;
    } else {
      TRACE_BRANCH("state:off-idle");
    }
  } else if (ctx->currentState == STATE_RUNNING) {
    /* 異常を優先して安全側へ遷移 */
    if (abnormal) {
      TRACE_BRANCH("state:running-abnormal");
      ctx->currentState = STATE_FAULT;
      ctx->motorSpeed = 0;
    } else if (pressed) {
      TRACE_BRANCH("state:running-pressed");
      ctx->currentState = STATE_OFF;
      ctx->motorSpeed = 0;

      /* 手動停止時に異常タイマー持ち越しを防ぐ */
      ctx->abnormalStartMillis = 0;
      ctx->abnormalCount = 0;
    } else {
      TRACE_BRANCH("state:running-idle");
    }
  } else if (ctx->currentState == STATE_FAULT) {
    if (pressed) {
      TRACE_BRANCH("state:fault-pressed");
      ctx->currentState = STATE_OFF;
      ctx->motorSpeed = 0;
      ctx->abnormalCount = 0;
      ctx->abnormalStartMillis = 0;
    } else {
      TRACE_BRANCH("state:fault-idle");
    }
  }

  return (*oldState != ctx->currentState);
}
