#ifndef POT_SPEED_H
#define POT_SPEED_H

#include "fan_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * つまみの生値（0-1023）を PWM（0-255）へ変換します。
 * さらにデッドバンドを適用し、微小ノイズによる揺れを抑えます。
 */
int readPotSpeed(int potRaw, int previousMotorSpeed);

#ifdef __cplusplus
}
#endif

#endif
