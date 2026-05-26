#include "pot_speed.h"

#include <stdlib.h>

int readPotSpeed(int potRaw, int previousMotorSpeed) {
  long mapped;

  if (potRaw < 0) {
    potRaw = 0;
  } else if (potRaw > 1023) {
    potRaw = 1023;
  }

  /*
   * Arduino の map 相当:
   * 0-1023 を 0-255 に線形変換します。
   */
  mapped = ((long)potRaw * 255L) / 1023L;

  /* 小さな揺れはノイズとして無視 */
  if (abs((int)mapped - previousMotorSpeed) < POT_DEADBAND) {
    return previousMotorSpeed;
  }

  return (int)mapped;
}
