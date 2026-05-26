#ifndef DEBUG_OUTPUT_H
#define DEBUG_OUTPUT_H

#include "system_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 周期サマリログ（DEBUG_MS で出力間隔を制限） */
void logMainStatus(SystemContext *ctx, unsigned long now);

/* 状態遷移ログ（原因調査時に有効） */
void logStateTransition(int oldState, int newState);

#ifdef __cplusplus
}
#endif

#endif
