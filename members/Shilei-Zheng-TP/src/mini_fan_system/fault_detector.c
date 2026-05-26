#include "fault_detector.h"

bool isAbnormalReading(int value, SystemContext *ctx, unsigned long now) {
  bool isPinned;

  if (ctx == 0) {
    return false;
  }

  /* ADC ノイズを考慮して近傍値も異常候補に含める */
  isPinned = (value <= 2 || value >= 1021);

  if (isPinned) {
    if (ctx->abnormalStartMillis == 0) {
      ctx->abnormalStartMillis = now;
    }

    if (now - ctx->abnormalStartMillis >= FAULT_MS) {
      ctx->abnormalCount++;
      return true;
    }

    return false;
  }

  /* 正常値へ戻ったら再計測に備えてリセット */
  ctx->abnormalStartMillis = 0;
  return false;
}
