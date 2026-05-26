#ifndef MOTOR_OUTPUT_H
#define MOTOR_OUTPUT_H

#include "system_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 状態と目標速度に応じてモーター PWM 出力を更新します。 */
void updateMotor(const SystemContext *ctx);

#ifdef __cplusplus
}
#endif

#endif
