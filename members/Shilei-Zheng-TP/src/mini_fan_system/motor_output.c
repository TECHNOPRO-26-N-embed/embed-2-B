#include "motor_output.h"

#include "c_port.h"

static int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

void updateMotor(const SystemContext *ctx) {
  int pwm;

  if (ctx == 0) {
    return;
  }

  if (ctx->currentState == STATE_OFF || ctx->currentState == STATE_FAULT) {
    hw_analog_write(PIN_MOTOR, 0);
    return;
  }

  pwm = clampInt(ctx->motorSpeed, 0, 255);

  if (pwm < MOTOR_MIN_PWM) {
    hw_analog_write(PIN_MOTOR, 0);
  } else {
    hw_analog_write(PIN_MOTOR, pwm);
  }
}
