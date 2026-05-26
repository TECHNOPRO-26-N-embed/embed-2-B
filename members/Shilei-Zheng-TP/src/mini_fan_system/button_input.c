#include "button_input.h"

#include "c_port.h"

bool readButtonEdge(SystemContext *ctx, unsigned long now) {
  int raw;

  if (ctx == 0) {
    return false;
  }

  raw = hw_digital_read(PIN_BUTTON);

  /*
   * 起動ガード:
   * 電源投入後、最初に一度 HIGH（ボタンを離した状態）を確認するまで
   * 押下入力を受け付けません。
   */
  if (!ctx->buttonReady) {
    if (raw == HW_HIGH) {
      ctx->buttonReady = true;
      ctx->lastButtonState = true;
    }
    return false;
  }

  /* 押下エッジ検出: HIGH -> LOW */
  if (raw == HW_LOW && ctx->lastButtonState) {
    /* デバウンス判定 */
    if (now - ctx->lastButtonMillis >= DEBOUNCE_MS) {
      ctx->lastButtonState = false;
      ctx->lastButtonMillis = now;
      return true;
    }
  }

  /* 離されたら次の押下受付準備 */
  if (raw == HW_HIGH) {
    ctx->lastButtonState = true;
  }

  return false;
}
