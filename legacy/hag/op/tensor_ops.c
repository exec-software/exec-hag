/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#include "hag/op/tensor_ops.h"
#include "hag/core/tensor.h"
#include "hag/core/buffer.h"
#include "hag/core/device.h"
#include "hag/core/instance.h"
#include <string.h>
#include <math.h>

/* ============================================================================
 * Helper: Map tensor buffer and get data pointer
 * ============================================================================ */

static HagResult mapTensorData(HagDevice device, HagTensor tensor, void** ppData) {
    if (!tensor || !ppData) return HAG_ERROR_INVALID_ARGUMENT;
    
    HagTensorInternal_T* t = (HagTensorInternal_T*)tensor;
    if (t->magic != HAG_TENSOR_MAGIC) return HAG_ERROR_INVALID_ARGUMENT;
    
    HagBuffer buffer;
    uint64_t offset;
    HagResult r = hagGetTensorBuffer(tensor, &buffer, &offset);
    if (r != HAG_SUCCESS) return r;
    
    void* base = NULL;
    r = hagMapBuffer(device, buffer, &base);
    if (r != HAG_SUCCESS) return r;
    
    *ppData = (char*)base + offset;
    return HAG_SUCCESS;
}

static void unmapTensorData(HagDevice device, HagTensor tensor) {
    if (!tensor) return;
    
    HagBuffer buffer;
    uint64_t offset;
    if (hagGetTensorBuffer(tensor, &buffer, &offset) == HAG_SUCCESS) {
        hagUnmapBuffer(device, buffer);
    }
}

/* ============================================================================
 * Element-wise Operations
 * ============================================================================ */

HAG_API HagResult hagOpAdd(HagDevice device, HagTensor a, HagTensor b, HagTensor out) {
    if (!device || !a || !b || !out) return HAG_ERROR_INVALID_ARGUMENT;
    
    HagDType dtype = hagGetTensorDType(a);
    if (dtype != hagGetTensorDType(b) || dtype != hagGetTensorDType(out)) {
        return HAG_ERROR_MISMATCHED_DTYPES;
    }
    
    uint32_t dimsA, dimsB, dimsOut;
    uint64_t shapeA[8], shapeB[8], shapeOut[8];
    hagGetTensorShape(a, &dimsA, shapeA);
    hagGetTensorShape(b, &dimsB, shapeB);
    hagGetTensorShape(out, &dimsOut, shapeOut);
    
    if (dimsA != dimsB || dimsA != dimsOut) return HAG_ERROR_INVALID_DIMENSIONS;
    
    uint64_t count = hagTensorGetElementCount(dimsA, shapeA);
    uint64_t countB = hagTensorGetElementCount(dimsB, shapeB);
    uint64_t countOut = hagTensorGetElementCount(dimsOut, shapeOut);
    if (count != countB || count != countOut) return HAG_ERROR_INVALID_DIMENSIONS;
    
    void *ptrA = NULL, *ptrB = NULL, *ptrOut = NULL;
    HagResult r = mapTensorData(device, a, &ptrA);
    if (r != HAG_SUCCESS) goto cleanup;
    r = mapTensorData(device, b, &ptrB);
    if (r != HAG_SUCCESS) goto cleanup;
    r = mapTensorData(device, out, &ptrOut);
    if (r != HAG_SUCCESS) goto cleanup;
    
    switch (dtype) {
        case HAG_DTYPE_FLOAT32: {
            float* fa = (float*)ptrA;
            float* fb = (float*)ptrB;
            float* fo = (float*)ptrOut;
            for (uint64_t i = 0; i < count; i++) {
                fo[i] = fa[i] + fb[i];
            }
            break;
        }
        case HAG_DTYPE_FLOAT16:
        case HAG_DTYPE_BFLOAT16:
            /* TODO: Implement FP16/BF16 element-wise ops */
            r = HAG_ERROR_NOT_SUPPORTED;
            break;
        case HAG_DTYPE_INT32: {
            int32_t* ia = (int32_t*)ptrA;
            int32_t* ib = (int32_t*)ptrB;
            int32_t* io = (int32_t*)ptrOut;
            for (uint64_t i = 0; i < count; i++) {
                io[i] = ia[i] + ib[i];
            }
            break;
        }
        default:
            r = HAG_ERROR_NOT_SUPPORTED;
            break;
    }
    
cleanup:
    unmapTensorData(device, a);
    unmapTensorData(device, b);
    unmapTensorData(device, out);
    return r;
}

HAG_API HagResult hagOpMul(HagDevice device, HagTensor a, HagTensor b, HagTensor out) {
    if (!device || !a || !b || !out) return HAG_ERROR_INVALID_ARGUMENT;
    
    HagDType dtype = hagGetTensorDType(a);
    if (dtype != hagGetTensorDType(b) || dtype != hagGetTensorDType(out)) {
        return HAG_ERROR_MISMATCHED_DTYPES;
    }
    
    uint32_t dimsA, dimsB, dimsOut;
    uint64_t shapeA[8], shapeB[8], shapeOut[8];
    hagGetTensorShape(a, &dimsA, shapeA);
    hagGetTensorShape(b, &dimsB, shapeB);
    hagGetTensorShape(out, &dimsOut, shapeOut);
    
    if (dimsA != dimsB || dimsA != dimsOut) return HAG_ERROR_INVALID_DIMENSIONS;
    
    uint64_t count = hagTensorGetElementCount(dimsA, shapeA);
    uint64_t countB = hagTensorGetElementCount(dimsB, shapeB);
    uint64_t countOut = hagTensorGetElementCount(dimsOut, shapeOut);
    if (count != countB || count != countOut) return HAG_ERROR_INVALID_DIMENSIONS;
    
    void *ptrA = NULL, *ptrB = NULL, *ptrOut = NULL;
    HagResult r = mapTensorData(device, a, &ptrA);
    if (r != HAG_SUCCESS) goto cleanup;
    r = mapTensorData(device, b, &ptrB);
    if (r != HAG_SUCCESS) goto cleanup;
    r = mapTensorData(device, out, &ptrOut);
    if (r != HAG_SUCCESS) goto cleanup;
    
    switch (dtype) {
        case HAG_DTYPE_FLOAT32: {
            float* fa = (float*)ptrA;
            float* fb = (float*)ptrB;
            float* fo = (float*)ptrOut;
            for (uint64_t i = 0; i < count; i++) {
                fo[i] = fa[i] * fb[i];
            }
            break;
        }
        default:
            r = HAG_ERROR_NOT_SUPPORTED;
            break;
    }
    
cleanup:
    unmapTensorData(device, a);
    unmapTensorData(device, b);
    unmapTensorData(device, out);
    return r;
}

HAG_API HagResult hagOpFill(HagDevice device, HagTensor tensor, float value) {
    if (!device || !tensor) return HAG_ERROR_INVALID_ARGUMENT;
    
    HagDType dtype = hagGetTensorDType(tensor);
    uint32_t dims;
    uint64_t shape[8];
    hagGetTensorShape(tensor, &dims, shape);
    uint64_t count = hagTensorGetElementCount(dims, shape);
    
    void* ptr = NULL;
    HagResult r = mapTensorData(device, tensor, &ptr);
    if (r != HAG_SUCCESS) return r;
    
    switch (dtype) {
        case HAG_DTYPE_FLOAT32: {
            float* f = (float*)ptr;
            for (uint64_t i = 0; i < count; i++) {
                f[i] = value;
            }
            break;
        }
        case HAG_DTYPE_INT32: {
            int32_t* i32 = (int32_t*)ptr;
            int32_t v = (int32_t)value;
            for (uint64_t i = 0; i < count; i++) {
                i32[i] = v;
            }
            break;
        }
        default:
            r = HAG_ERROR_NOT_SUPPORTED;
            break;
    }
    
    unmapTensorData(device, tensor);
    return r;
}

/* ============================================================================
 * Matrix Multiplication (Naive CPU Implementation)
 * ============================================================================ */

HAG_API HagResult hagOpMatmul(HagDevice device, HagTensor a, HagTensor b, HagTensor out) {
    if (!device || !a || !b || !out) return HAG_ERROR_INVALID_ARGUMENT;
    
    HagDType dtype = hagGetTensorDType(a);
    if (dtype != hagGetTensorDType(b) || dtype != hagGetTensorDType(out)) {
        return HAG_ERROR_MISMATCHED_DTYPES;
    }
    
    /* Expect 2D tensors: A[M,K], B[K,N], out[M,N] */
    uint32_t dimsA, dimsB, dimsOut;
    uint64_t shapeA[8], shapeB[8], shapeOut[8];
    hagGetTensorShape(a, &dimsA, shapeA);
    hagGetTensorShape(b, &dimsB, shapeB);
    hagGetTensorShape(out, &dimsOut, shapeOut);
    
    if (dimsA != 2 || dimsB != 2 || dimsOut != 2) return HAG_ERROR_INVALID_DIMENSIONS;
    
    uint64_t M = shapeA[0];
    uint64_t K = shapeA[1];
    uint64_t Kb = shapeB[0];
    uint64_t N = shapeB[1];
    
    if (K != Kb) return HAG_ERROR_INVALID_DIMENSIONS;
    if (shapeOut[0] != M || shapeOut[1] != N) return HAG_ERROR_INVALID_DIMENSIONS;
    
    void *ptrA = NULL, *ptrB = NULL, *ptrOut = NULL;
    HagResult r = mapTensorData(device, a, &ptrA);
    if (r != HAG_SUCCESS) goto cleanup;
    r = mapTensorData(device, b, &ptrB);
    if (r != HAG_SUCCESS) goto cleanup;
    r = mapTensorData(device, out, &ptrOut);
    if (r != HAG_SUCCESS) goto cleanup;
    
    switch (dtype) {
        case HAG_DTYPE_FLOAT32: {
            float* fa = (float*)ptrA;
            float* fb = (float*)ptrB;
            float* fo = (float*)ptrOut;
            
            /* Initialize output to zero */
            memset(fo, 0, M * N * sizeof(float));
            
            /* Naive triple-loop GEMM */
            for (uint64_t m = 0; m < M; m++) {
                for (uint64_t k = 0; k < K; k++) {
                    float aVal = fa[m * K + k];
                    for (uint64_t n = 0; n < N; n++) {
                        fo[m * N + n] += aVal * fb[k * N + n];
                    }
                }
            }
            break;
        }
        default:
            r = HAG_ERROR_NOT_SUPPORTED;
            break;
    }
    
cleanup:
    unmapTensorData(device, a);
    unmapTensorData(device, b);
    unmapTensorData(device, out);
    return r;
}

/* ============================================================================
 * Softmax (along last dimension)
 * ============================================================================ */

HAG_API HagResult hagOpSoftmax(HagDevice device, HagTensor input, HagTensor output) {
    if (!device || !input || !output) return HAG_ERROR_INVALID_ARGUMENT;
    
    HagDType dtype = hagGetTensorDType(input);
    if (dtype != hagGetTensorDType(output)) {
        return HAG_ERROR_MISMATCHED_DTYPES;
    }
    
    uint32_t dims;
    uint64_t shape[8];
    hagGetTensorShape(input, &dims, shape);
    
    if (dims < 1) return HAG_ERROR_INVALID_DIMENSIONS;
    
    uint64_t lastDim = shape[dims - 1];
    uint64_t batches = 1;
    for (uint32_t i = 0; i < dims - 1; i++) {
        batches *= shape[i];
    }
    
    void *ptrIn = NULL, *ptrOut = NULL;
    HagResult r = mapTensorData(device, input, &ptrIn);
    if (r != HAG_SUCCESS) goto cleanup;
    r = mapTensorData(device, output, &ptrOut);
    if (r != HAG_SUCCESS) goto cleanup;
    
    switch (dtype) {
        case HAG_DTYPE_FLOAT32: {
            float* fin = (float*)ptrIn;
            float* fout = (float*)ptrOut;
            
            for (uint64_t b = 0; b < batches; b++) {
                float* inRow = fin + b * lastDim;
                float* outRow = fout + b * lastDim;
                
                /* Find max for numerical stability */
                float maxVal = inRow[0];
                for (uint64_t i = 1; i < lastDim; i++) {
                    if (inRow[i] > maxVal) maxVal = inRow[i];
                }
                
                /* Compute exponentials and sum */
                float sum = 0.0f;
                for (uint64_t i = 0; i < lastDim; i++) {
                    float e = expf(inRow[i] - maxVal);
                    outRow[i] = e;
                    sum += e;
                }
                
                /* Normalize */
                float invSum = 1.0f / sum;
                for (uint64_t i = 0; i < lastDim; i++) {
                    outRow[i] *= invSum;
                }
            }
            break;
        }
        default:
            r = HAG_ERROR_NOT_SUPPORTED;
            break;
    }
    
cleanup:
    unmapTensorData(device, input);
    unmapTensorData(device, output);
    return r;
}

/* ============================================================================
 * Layer Normalization
 * ============================================================================ */

HAG_API HagResult hagOpLayerNorm(HagDevice device, HagTensor input, HagTensor gamma, HagTensor beta, HagTensor output) {
    if (!device || !input || !gamma || !beta || !output) return HAG_ERROR_INVALID_ARGUMENT;
    
    HagDType dtype = hagGetTensorDType(input);
    if (dtype != hagGetTensorDType(output)) {
        return HAG_ERROR_MISMATCHED_DTYPES;
    }
    
    uint32_t dims;
    uint64_t shape[8];
    hagGetTensorShape(input, &dims, shape);
    
    if (dims < 1) return HAG_ERROR_INVALID_DIMENSIONS;
    
    uint64_t lastDim = shape[dims - 1];
    uint64_t batches = 1;
    for (uint32_t i = 0; i < dims - 1; i++) {
        batches *= shape[i];
    }
    
    /* Validate gamma and beta shapes */
    uint32_t gDims;
    uint64_t gShape[8];
    hagGetTensorShape(gamma, &gDims, gShape);
    if (gDims != 1 || gShape[0] != lastDim) return HAG_ERROR_INVALID_DIMENSIONS;
    
    uint32_t bDims;
    uint64_t bShape[8];
    hagGetTensorShape(beta, &bDims, bShape);
    if (bDims != 1 || bShape[0] != lastDim) return HAG_ERROR_INVALID_DIMENSIONS;
    
    void *ptrIn = NULL, *ptrOut = NULL, *ptrGamma = NULL, *ptrBeta = NULL;
    HagResult r = mapTensorData(device, input, &ptrIn);
    if (r != HAG_SUCCESS) goto cleanup;
    r = mapTensorData(device, output, &ptrOut);
    if (r != HAG_SUCCESS) goto cleanup;
    r = mapTensorData(device, gamma, &ptrGamma);
    if (r != HAG_SUCCESS) goto cleanup;
    r = mapTensorData(device, beta, &ptrBeta);
    if (r != HAG_SUCCESS) goto cleanup;
    
    switch (dtype) {
        case HAG_DTYPE_FLOAT32: {
            float* fin = (float*)ptrIn;
            float* fout = (float*)ptrOut;
            float* fg = (float*)ptrGamma;
            float* fb = (float*)ptrBeta;
            
            for (uint64_t b = 0; b < batches; b++) {
                float* inRow = fin + b * lastDim;
                float* outRow = fout + b * lastDim;
                
                /* Compute mean */
                float mean = 0.0f;
                for (uint64_t i = 0; i < lastDim; i++) {
                    mean += inRow[i];
                }
                mean /= (float)lastDim;
                
                /* Compute variance */
                float var = 0.0f;
                for (uint64_t i = 0; i < lastDim; i++) {
                    float diff = inRow[i] - mean;
                    var += diff * diff;
                }
                var /= (float)lastDim;
                
                /* Normalize and apply scale/shift */
                float invStd = 1.0f / sqrtf(var + 1e-5f);
                for (uint64_t i = 0; i < lastDim; i++) {
                    float normalized = (inRow[i] - mean) * invStd;
                    outRow[i] = normalized * fg[i] + fb[i];
                }
            }
            break;
        }
        default:
            r = HAG_ERROR_NOT_SUPPORTED;
            break;
    }
    
cleanup:
    unmapTensorData(device, input);
    unmapTensorData(device, output);
    unmapTensorData(device, gamma);
    unmapTensorData(device, beta);
    return r;
}
