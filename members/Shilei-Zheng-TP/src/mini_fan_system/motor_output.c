#include "motor_output.h"

#include "c_port.h"
#include "tests/branch_trace.h"

static int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) {
    TRACE_BRANCH("motor:clamp-low");
    return minValue;
  }
  if (value > maxValue) {
    TRACE_BRANCH("motor:clamp-high");
    return maxValue;
  }
  TRACE_BRANCH("motor:clamp-normal");
  return value;
}

void updateMotor(const SystemContext *ctx) {
  int pwm;

  if (ctx == 0) {
    TRACE_BRANCH("motor:null");
    return;
  }

  if (ctx->currentState == STATE_OFF || ctx->currentState == STATE_FAULT) {
    TRACE_BRANCH("motor:stopped-state");
    hw_analog_write(PIN_MOTOR, 0);
    return;
  }

  pwm = clampInt(ctx->motorSpeed, 0, 255);

  if (pwm < MOTOR_MIN_PWM) {
    TRACE_BRANCH("motor:under-min");
    hw_analog_write(PIN_MOTOR, 0);
  } else {
    TRACE_BRANCH("motor:active");
    hw_analog_write(PIN_MOTOR, pwm);
  }
}
