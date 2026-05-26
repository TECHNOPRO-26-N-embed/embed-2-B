#include "system_context.h"

void systemContextInit(SystemContext *ctx) {
  if (ctx == 0) {
    return;
  }

  ctx->currentState = STATE_OFF;

  ctx->potValue = 0;
  ctx->motorSpeed = 0;

  ctx->lastButtonMillis = 0;
  ctx->lastButtonState = true;
  ctx->buttonReady = false;

  ctx->abnormalStartMillis = 0;
  ctx->abnormalCount = 0;

  ctx->lastLedToggleMillis = 0;
  ctx->ledBlinkState = false;

  ctx->lastDebugMillis = 0;
}
