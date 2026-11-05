/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#ifndef HAG_OPS_TENSOR_OPS_H
#define HAG_OPS_TENSOR_OPS_H

#include "hag/api/api.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * High-level tensor operations for Hag.
 * These are convenience functions that execute common AI operations.
 * In v0.1, they execute on the CPU fallback. In the future, they will
 * dispatch to optimized GPU kernels.
 */

/* Element-wise operations */
HAG_API HagResult hagOpAdd(HagDevice device, HagTensor a, HagTensor b, HagTensor out);
HAG_API HagResult hagOpMul(HagDevice device, HagTensor a, HagTensor b, HagTensor out);
HAG_API HagResult hagOpFill(HagDevice device, HagTensor tensor, float value);

/* Matrix multiplication: out = A * B */
/* A: [M, K], B: [K, N], out: [M, N] */
HAG_API HagResult hagOpMatmul(HagDevice device, HagTensor a, HagTensor b, HagTensor out);

/* Softmax along last dimension */
HAG_API HagResult hagOpSoftmax(HagDevice device, HagTensor input, HagTensor output);

/* Layer normalization */
HAG_API HagResult hagOpLayerNorm(HagDevice device, HagTensor input, HagTensor gamma, HagTensor beta, HagTensor output);

#ifdef __cplusplus
}
#endif

#endif /* HAG_OPS_TENSOR_OPS_H */
