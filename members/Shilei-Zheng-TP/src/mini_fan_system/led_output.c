#include "led_output.h"

#include "c_port.h"

void updateLED(SystemContext *ctx, unsigned long now) {
  static int previousState = STATE_OFF;

  if (ctx == 0) {
    return;
  }

  if (ctx->currentState == STATE_OFF) {
    hw_digital_write(PIN_LED_RED, HW_HIGH);
    hw_digital_write(PIN_LED_GREEN, HW_LOW);
    ctx->ledBlinkState = false;
  } else if (ctx->currentState == STATE_RUNNING) {
    hw_digital_write(PIN_LED_RED, HW_LOW);
    hw_digital_write(PIN_LED_GREEN, HW_HIGH);
    ctx->ledBlinkState = false;
  } else if (ctx->currentState == STATE_FAULT) {
    hw_digital_write(PIN_LED_GREEN, HW_LOW);

    /*
     * FAULT へ入った直後は赤 LED を即時点灯し、
     * その後 BLINK_MS 周期で点滅させます。
     */
    if (previousState != STATE_FAULT) {
      ctx->ledBlinkState = true;
      hw_digital_write(PIN_LED_RED, HW_HIGH);
      ctx->lastLedToggleMillis = now;
    } else if (now - ctx->lastLedToggleMillis >= BLINK_MS) {
      ctx->ledBlinkState = !ctx->ledBlinkState;
      hw_digital_write(PIN_LED_RED, ctx->ledBlinkState ? HW_HIGH : HW_LOW);
      ctx->lastLedToggleMillis = now;
    }
  }

  previousState = ctx->currentState;
}
