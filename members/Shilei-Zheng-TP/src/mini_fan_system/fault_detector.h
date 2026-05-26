#ifndef FAULT_DETECTOR_H
#define FAULT_DETECTOR_H

#include "system_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 異常値への張り付きが FAULT_MS 継続したら true を返します。 */
bool isAbnormalReading(int value, SystemContext *ctx, unsigned long now);

#ifdef __cplusplus
}
#endif

#endif
