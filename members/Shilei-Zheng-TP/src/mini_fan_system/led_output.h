#ifndef LED_OUTPUT_H
#define LED_OUTPUT_H

#include "system_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 現在状態に応じて LED の点灯・点滅を更新します。 */
void updateLED(SystemContext *ctx, unsigned long now);

#ifdef __cplusplus
}
#endif

#endif
