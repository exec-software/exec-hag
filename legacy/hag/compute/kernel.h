/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#ifndef HAG_COMPUTE_KERNEL_H
#define HAG_COMPUTE_KERNEL_H

#include "hag/api/api.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal compute kernel definitions and built-in operations.
 */

/* Built-in operation types */
typedef enum HagOpType {
    HAG_OP_NONE = 0,
    HAG_OP_ADD,
    HAG_OP_MUL,
    HAG_OP_MATMUL,
    HAG_OP_CONV2D,
    HAG_OP_ATTENTION,
    HAG_OP_SOFTMAX,
    HAG_OP_LAYERNORM,
    HAG_OP_RELU,
    HAG_OP_GELU,
    HAG_OP_SILU,
    HAG_OP_QUANTIZE,
    HAG_OP_DEQUANTIZE,
    HAG_OP_COUNT,
} HagOpType;

/* Kernel variant for hardware-specific optimizations */
typedef uint32_t HagKernelVariant;
#define HAG_KERNEL_VARIANT_DEFAULT 0

/* Kernel registry */
typedef struct HagKernelRegistry HagKernelRegistry;

HagKernelRegistry* hagKernelRegistryCreate(HagDevice device);
void hagKernelRegistryDestroy(HagKernelRegistry* registry);

HagResult hagKernelRegistryGetBuiltin(
    HagKernelRegistry* registry,
    HagOpType op,
    HagDType dtype,
    HagKernelVariant variant,
    HagKernel* pKernel);

/* Graph optimization */
typedef struct HagGraphOptimizer HagGraphOptimizer;

HagGraphOptimizer* hagGraphOptimizerCreate(HagDevice device);
void hagGraphOptimizerDestroy(HagGraphOptimizer* optimizer);

HagResult hagGraphOptimizerRun(
    HagGraphOptimizer* optimizer,
    HagGraph graph,
    uint32_t* pNodeCount,
    HagGraphNode** ppOptimizedNodes);

#ifdef __cplusplus
}
#endif

#endif /* HAG_COMPUTE_KERNEL_H */
