#include "pot_speed.h"

#include <stdlib.h>

#include "tests/branch_trace.h"

int readPotSpeed(int potRaw, int previousMotorSpeed) {
  long mapped;

  if (potRaw < 0) {
    TRACE_BRANCH("pot:clamp-low");
    potRaw = 0;
  } else if (potRaw > 1023) {
    TRACE_BRANCH("pot:clamp-high");
    potRaw = 1023;
  } else {
    TRACE_BRANCH("pot:normal");
  }

  /*
   * Arduino の map 相当:
   * 0-1023 を 0-255 に線形変換します。
   */
  mapped = ((long)potRaw * 255L) / 1023L;

  /* 小さな揺れはノイズとして無視 */
  if (abs((int)mapped - previousMotorSpeed) < POT_DEADBAND) {
    TRACE_BRANCH("pot:deadband-hit");
    return previousMotorSpeed;
  }

  TRACE_BRANCH("pot:outside-deadband");
  return (int)mapped;
}
