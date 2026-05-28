#include "fault_detector.h"

#include "tests/branch_trace.h"

bool isAbnormalReading(int value, SystemContext *ctx, unsigned long now) {
  bool isPinned;

  if (ctx == 0) {
    TRACE_BRANCH("fault:null");
    return false;
  }

  /* ADC ノイズを考慮して近傍値も異常候補に含める */
  isPinned = (value <= 2 || value >= 1021);

  if (isPinned) {
    if (ctx->abnormalStartMillis == 0) {
      TRACE_BRANCH("fault:pinned-start");
      ctx->abnormalStartMillis = now;
    }

    if (now - ctx->abnormalStartMillis >= FAULT_MS) {
      TRACE_BRANCH("fault:pinned-confirmed");
      ctx->abnormalCount++;
      return true;
    }

    TRACE_BRANCH("fault:pinned-not-yet");
    return false;
  }

  /* 正常値へ戻ったら再計測に備えてリセット */
  TRACE_BRANCH("fault:not-pinned");
  ctx->abnormalStartMillis = 0;
  return false;
}
