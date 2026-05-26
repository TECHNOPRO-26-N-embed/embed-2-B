#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "system_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ボタン入力と異常判定の結果から currentState を更新します。
 * 状態が変化したときだけ true を返します。
 */
bool updateState(SystemContext *ctx, bool pressed, bool abnormal, int *oldState);

#ifdef __cplusplus
}
#endif

#endif
