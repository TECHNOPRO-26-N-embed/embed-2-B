#include "debug_output.h"

#include "c_port.h"

void logMainStatus(SystemContext *ctx, unsigned long now) {
  if (ctx == 0) {
    return;
  }

  if (now - ctx->lastDebugMillis < DEBUG_MS) {
    return;
  }

  hw_serial_print_text("state:");
  hw_serial_print_int(ctx->currentState);
  hw_serial_print_text(" pot:");
  hw_serial_print_int(ctx->potValue);
  hw_serial_print_text(" pwm:");
  hw_serial_println_int(ctx->motorSpeed);

  ctx->lastDebugMillis = now;
}

void logStateTransition(int oldState, int newState) {
  hw_serial_print_text("[TR] ");
  hw_serial_print_int(oldState);
  hw_serial_print_text(" -> ");
  hw_serial_println_int(newState);
}
