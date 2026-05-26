#ifndef BUTTON_INPUT_H
#define BUTTON_INPUT_H

#include "system_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 有効な「押下エッジ」が確定した瞬間だけ true を返します。
 * 押しっぱなし中は true を連発しません。
 */
bool readButtonEdge(SystemContext *ctx, unsigned long now);

#ifdef __cplusplus
}
#endif

#endif
