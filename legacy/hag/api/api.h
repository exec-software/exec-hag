/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 */

#ifndef HAG_API_H
#define HAG_API_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Hag API Version 0.1.0 (Pre-alpha)
 *
 * This is the primary public API header for the Hag AI Compute Runtime.
 * All applications consuming Hag link against this interface.
 */

#define HAG_API_VERSION_MAJOR 0
#define HAG_API_VERSION_MINOR 1
#define HAG_API_VERSION_PATCH 0

/* ============================================================================
 * Platform Abstraction & Macros
 * ============================================================================ */

#if defined(_WIN32)
  #ifdef HAG_BUILD_DLL
    #define HAG_API __declspec(dllexport)
  #else
    #define HAG_API __declspec(dllimport)
  #endif
#else
  #if __GNUC__ >= 4
    #define HAG_API __attribute__((visibility("default")))
  #else
    #define HAG_API
  #endif
#endif

#define HAG_INLINE static inline

/* ============================================================================
 * Opaque Handles
 * ============================================================================ */

typedef struct HagInstance_T*      HagInstance;
typedef struct HagPhysicalDevice_T* HagPhysicalDevice;
typedef struct HagDevice_T*        HagDevice;
typedef struct HagQueue_T*         HagQueue;
typedef struct HagBuffer_T*        HagBuffer;
typedef struct HagTensor_T*        HagTensor;
typedef struct HagKernel_T*        HagKernel;
typedef struct HagGraph_T*         HagGraph;
typedef struct HagEvent_T*         HagEvent;
typedef struct HagSemaphore_T*     HagSemaphore;
typedef struct HagAllocator_T*     HagAllocator;

/* ============================================================================
 * Result Codes
 * ============================================================================ */

typedef enum HagResult {
    HAG_SUCCESS = 0,
    HAG_ERROR_UNKNOWN = -1,
    HAG_ERROR_OUT_OF_MEMORY = -2,
    HAG_ERROR_INVALID_ARGUMENT = -3,
    HAG_ERROR_DEVICE_LOST = -4,
    HAG_ERROR_NOT_SUPPORTED = -5,
    HAG_ERROR_TIMEOUT = -6,
    HAG_ERROR_DRIVER_ERROR = -7,
    HAG_ERROR_SHADER_COMPILE = -8,
    HAG_ERROR_INVALID_DIMENSIONS = -9,
    HAG_ERROR_MISMATCHED_DTYPES = -10,
} HagResult;

/* ============================================================================
 * Data Types (AI-First)
 * ============================================================================ */

typedef enum HagDType {
    HAG_DTYPE_FLOAT32 = 0,
    HAG_DTYPE_FLOAT16 = 1,
    HAG_DTYPE_BFLOAT16 = 2,
    HAG_DTYPE_INT32 = 3,
    HAG_DTYPE_INT16 = 4,
    HAG_DTYPE_INT8 = 5,
    HAG_DTYPE_UINT8 = 6,
    HAG_DTYPE_INT4 = 7,     /* Packed int4 */
    HAG_DTYPE_UINT4 = 8,    /* Packed uint4 */
    HAG_DTYPE_FP8_E4M3 = 9, /* NVIDIA/ARM FP8 format */
    HAG_DTYPE_FP8_E5M2 = 10,/* Alternative FP8 format */
    HAG_DTYPE_BOOL = 11,
} HagDType;

/* ============================================================================
 * Device Discovery & Enumeration
 * ============================================================================ */

typedef enum HagDeviceType {
    HAG_DEVICE_TYPE_DISCRETE_GPU = 0,
    HAG_DEVICE_TYPE_INTEGRATED_GPU = 1,
    HAG_DEVICE_TYPE_ACCELERATOR = 2,  /* AI-specific silicon (TPU, NPU) */
    HAG_DEVICE_TYPE_CPU_FALLBACK = 3, /* CPU fallback for testing */
} HagDeviceType;

typedef enum HagVendorId {
    HAG_VENDOR_AMD = 0x1002,
    HAG_VENDOR_NVIDIA = 0x10DE,
    HAG_VENDOR_INTEL = 0x8086,
    HAG_VENDOR_QUALCOMM = 0x5143,
    HAG_VENDOR_ARM = 0x13B5,
    HAG_VENDOR_APPLE = 0x106B,
    HAG_VENDOR_UNKNOWN = 0x0000,
} HagVendorId;

typedef struct HagDeviceProperties {
    uint32_t        apiVersion;
    uint32_t        driverVersion;
    HagVendorId     vendorId;
    HagDeviceType   deviceType;
    char            deviceName[256];
    uint64_t        deviceLocalMemoryBytes;
    uint64_t        sharedMemoryBytes;
    uint32_t        computeUnits;
    uint32_t        maxWorkGroupSize[3];
    uint32_t        maxTensorDimensions;
    uint32_t        warpSize;
    uint32_t        clockFrequencyMHz;
    /* Capabilities */
    uint32_t        supportsTensorCores : 1;
    uint32_t        supportsRayTracing  : 1;
    uint32_t        supportsFp16Atomic  : 1;
    uint32_t        supportsBf16        : 1;
    uint32_t        supportsFp8         : 1;
    uint32_t        supportsInt8DotAcc  : 1;
    uint32_t        reserved            : 26;
} HagDeviceProperties;

HAG_API HagResult hagEnumerateDevices(
    uint32_t*           pDeviceCount,
    HagDevice*          pDevices);

HAG_API HagResult hagEnumeratePhysicalDevices(
    HagInstance         instance,
    uint32_t*           pDeviceCount,
    HagPhysicalDevice*  pPhysicalDevices);

HAG_API HagResult hagGetPhysicalDeviceProperties(
    HagPhysicalDevice   physicalDevice,
    HagDeviceProperties* pProperties);

HAG_API HagResult hagGetDeviceProperties(
    HagDevice           device,
    HagDeviceProperties* pProperties);

/* ============================================================================
 * Instance & Context
 * ============================================================================ */

typedef struct HagInstanceCreateInfo {
    uint32_t    apiVersion;
    const char* applicationName;
    uint32_t    applicationVersion;
    uint32_t    flags;              /* Reserved */
} HagInstanceCreateInfo;

HAG_API HagResult hagCreateInstance(
    const HagInstanceCreateInfo* pCreateInfo,
    HagInstance* pInstance);

HAG_API void hagDestroyInstance(
    HagInstance instance);

/* ============================================================================
 * Device & Queue Management
 * ============================================================================ */

typedef enum HagQueueType {
    HAG_QUEUE_COMPUTE = 0,
    HAG_QUEUE_TRANSFER = 1,
    HAG_QUEUE_SPARSE = 2,  /* For sparse memory operations */
} HagQueueType;

typedef struct HagDeviceCreateInfo {
    HagPhysicalDevice physicalDevice;
    uint32_t        queueFamilyCount;
    const uint32_t* pQueueFamilyIndices;
    const uint32_t* pQueueCounts;
    uint32_t        flags;
} HagDeviceCreateInfo;

HAG_API HagResult hagCreateDevice(
    HagInstance                 instance,
    const HagDeviceCreateInfo*  pCreateInfo,
    HagDevice*                  pDevice);

HAG_API void hagDestroyDevice(
    HagDevice device);

HAG_API HagResult hagGetDeviceQueue(
    HagDevice       device,
    HagQueueType    queueType,
    uint32_t        queueIndex,
    HagQueue*       pQueue);

/* ============================================================================
 * Memory & Buffer Management
 * ============================================================================ */

typedef enum HagMemoryType {
    HAG_MEMORY_DEVICE_LOCAL = 0,     /* VRAM, fastest for compute */
    HAG_MEMORY_HOST_VISIBLE = 1,     /* Mappable, coherent if possible */
    HAG_MEMORY_HOST_CACHED = 2,      /* Mappable, cached reads */
    HAG_MEMORY_DEVICE_HOST_VISIBLE = 3, /* ReBAR/SAM memory */
} HagMemoryType;

typedef struct HagBufferCreateInfo {
    uint64_t        size;
    HagMemoryType   memoryType;
    uint32_t        usageFlags;      /* HAG_BUFFER_USAGE_* flags */
} HagBufferCreateInfo;

#define HAG_BUFFER_USAGE_TRANSFER_SRC   0x0001
#define HAG_BUFFER_USAGE_TRANSFER_DST   0x0002
#define HAG_BUFFER_USAGE_STORAGE        0x0004
#define HAG_BUFFER_USAGE_UNIFORM        0x0008
#define HAG_BUFFER_USAGE_TENSOR         0x0010
#define HAG_BUFFER_USAGE_GRAPH          0x0020

HAG_API HagResult hagCreateBuffer(
    HagDevice                   device,
    const HagBufferCreateInfo*  pCreateInfo,
    HagBuffer*                  pBuffer);

HAG_API void hagDestroyBuffer(
    HagDevice   device,
    HagBuffer   buffer);

HAG_API HagResult hagMapBuffer(
    HagDevice   device,
    HagBuffer   buffer,
    void**      ppData);

HAG_API void hagUnmapBuffer(
    HagDevice   device,
    HagBuffer   buffer);

HAG_API HagResult hagFlushBuffer(
    HagDevice   device,
    HagBuffer   buffer,
    uint64_t    offset,
    uint64_t    size);

HAG_API HagResult hagInvalidateBuffer(
    HagDevice   device,
    HagBuffer   buffer,
    uint64_t    offset,
    uint64_t    size);

/* ============================================================================
 * Tensor Abstraction
 * ============================================================================ */

typedef struct HagTensorCreateInfo {
    uint32_t    dimensions;
    const uint64_t* pShape;
    HagDType    dtype;
    HagBuffer   buffer;          /* Optional: use own allocation */
    uint64_t    offset;          /* Offset into buffer */
    uint32_t    flags;
} HagTensorCreateInfo;

HAG_API HagResult hagCreateTensor(
    HagDevice                   device,
    const HagTensorCreateInfo*  pCreateInfo,
    HagTensor*                  pTensor);

HAG_API void hagDestroyTensor(
    HagDevice   device,
    HagTensor   tensor);

HAG_API HagResult hagGetTensorShape(
    HagTensor   tensor,
    uint32_t*   pDimensions,
    uint64_t*   pShape);

HAG_API HagDType hagGetTensorDType(
    HagTensor   tensor);

HAG_API HagResult hagGetTensorBuffer(
    HagTensor   tensor,
    HagBuffer*  pBuffer,
    uint64_t*   pOffset);

/* ============================================================================
 * Kernel Management
 * ============================================================================ */

typedef enum HagKernelLanguage {
    HAG_KERNEL_LANGUAGE_HGSL = 0,    /* Hag GPU Shading Language (SPIR-V based) */
    HAG_KERNEL_LANGUAGE_SPIRV = 1,   /* Raw SPIR-V binary */
    HAG_KERNEL_LANGUAGE_OPENCL = 2,  /* OpenCL C source (for fallback) */
} HagKernelLanguage;

typedef struct HagKernelCreateInfo {
    HagKernelLanguage   language;
    const void*         pCode;
    size_t              codeSize;
    const char*         entryPoint;
    uint32_t            flags;
} HagKernelCreateInfo;

HAG_API HagResult hagCreateKernel(
    HagDevice                   device,
    const HagKernelCreateInfo*  pCreateInfo,
    HagKernel*                  pKernel);

HAG_API void hagDestroyKernel(
    HagDevice   device,
    HagKernel   kernel);

/* ============================================================================
 * Command Recording & Submission
 * ============================================================================ */

typedef struct HagCmdBuffer_T* HagCmdBuffer;

typedef struct HagCmdBufferCreateInfo {
    HagQueue    queue;
    uint32_t    flags;
} HagCmdBufferCreateInfo;

HAG_API HagResult hagCreateCmdBuffer(
    HagDevice                       device,
    const HagCmdBufferCreateInfo*   pCreateInfo,
    HagCmdBuffer*                   pCmdBuffer);

HAG_API void hagDestroyCmdBuffer(
    HagDevice       device,
    HagCmdBuffer    cmdBuffer);

HAG_API HagResult hagBeginCmdBuffer(
    HagCmdBuffer    cmdBuffer,
    uint32_t        flags);

HAG_API HagResult hagEndCmdBuffer(
    HagCmdBuffer    cmdBuffer);

HAG_API HagResult hagResetCmdBuffer(
    HagCmdBuffer    cmdBuffer,
    uint32_t        flags);

/* --- Command Recording --- */

HAG_API void hagCmdBindKernel(
    HagCmdBuffer    cmdBuffer,
    HagKernel       kernel);

HAG_API void hagCmdBindTensor(
    HagCmdBuffer    cmdBuffer,
    uint32_t        binding,
    HagTensor       tensor);

HAG_API void hagCmdDispatch(
    HagCmdBuffer    cmdBuffer,
    uint32_t        groupsX,
    uint32_t        groupsY,
    uint32_t        groupsZ);

HAG_API void hagCmdDispatchIndirect(
    HagCmdBuffer    cmdBuffer,
    HagBuffer       buffer,
    uint64_t        offset);

HAG_API void hagCmdCopyBuffer(
    HagCmdBuffer    cmdBuffer,
    HagBuffer       src,
    HagBuffer       dst,
    uint64_t        srcOffset,
    uint64_t        dstOffset,
    uint64_t        size);

HAG_API void hagCmdCopyBufferToTensor(
    HagCmdBuffer    cmdBuffer,
    HagBuffer       src,
    HagTensor       dst);

HAG_API void hagCmdCopyTensorToBuffer(
    HagCmdBuffer    cmdBuffer,
    HagTensor       src,
    HagBuffer       dst);

HAG_API void hagCmdFillBuffer(
    HagCmdBuffer    cmdBuffer,
    HagBuffer       buffer,
    uint64_t        offset,
    uint64_t        size,
    uint32_t        data);

HAG_API void hagCmdBarrier(
    HagCmdBuffer    cmdBuffer,
    uint32_t        srcStage,
    uint32_t        dstStage);

/* ============================================================================
 * Execution Graphs (AI-Optimized)
 * ============================================================================ */

typedef enum HagGraphNodeType {
    HAG_GRAPH_NODE_KERNEL = 0,
    HAG_GRAPH_NODE_COPY = 1,
    HAG_GRAPH_NODE_BARRIER = 2,
    HAG_GRAPH_NODE_CONDITIONAL = 3,
    HAG_GRAPH_NODE_FILL = 4,
} HagGraphNodeType;

typedef struct HagGraphNode {
    HagGraphNodeType    type;
    uint32_t            nodeIndex;
    uint32_t            dependencyCount;
    const uint32_t*     pDependencies;
    union {
        struct {
            HagKernel   kernel;
            uint32_t    groupCountX;
            uint32_t    groupCountY;
            uint32_t    groupCountZ;
        } kernel;
        struct {
            HagBuffer   src;
            HagBuffer   dst;
            uint64_t    size;
        } copy;
        struct {
            uint32_t    srcStage;
            uint32_t    dstStage;
        } barrier;
        struct {
            HagBuffer   buffer;
            uint64_t    offset;
            uint64_t    size;
            uint32_t    data;
        } fill;
    } params;
} HagGraphNode;

typedef struct HagGraphCreateInfo {
    uint32_t            nodeCount;
    const HagGraphNode* pNodes;
    uint32_t            flags;
} HagGraphCreateInfo;

HAG_API HagResult hagCreateGraph(
    HagDevice                   device,
    const HagGraphCreateInfo*   pCreateInfo,
    HagGraph*                   pGraph);

HAG_API void hagDestroyGraph(
    HagDevice   device,
    HagGraph    graph);

HAG_API HagResult hagUpdateGraphTensor(
    HagGraph    graph,
    uint32_t    nodeIndex,
    uint32_t    binding,
    HagTensor   newTensor);

/* ============================================================================
 * Submission & Synchronization
 * ============================================================================ */

typedef struct HagSubmitInfo {
    uint32_t        waitSemaphoreCount;
    HagSemaphore*   pWaitSemaphores;
    const uint64_t* pWaitValues;         /* For timeline semaphores */
    uint32_t        cmdBufferCount;
    HagCmdBuffer*   pCmdBuffers;
    HagGraph*       pGraphs;             /* Optional graph execution */
    uint32_t        graphCount;          /* Number of graphs to execute */
    uint32_t        signalSemaphoreCount;
    HagSemaphore*   pSignalSemaphores;
    const uint64_t* pSignalValues;
} HagSubmitInfo;

HAG_API HagResult hagQueueSubmit(
    HagQueue            queue,
    uint32_t            submitCount,
    const HagSubmitInfo* pSubmits,
    HagEvent            fence);

HAG_API HagResult hagQueueWaitIdle(
    HagQueue queue);

HAG_API HagResult hagDeviceWaitIdle(
    HagDevice device);

/* --- Events & Semaphores --- */

HAG_API HagResult hagCreateEvent(
    HagDevice   device,
    HagEvent*   pEvent);

HAG_API void hagDestroyEvent(
    HagDevice   device,
    HagEvent    event);

HAG_API HagResult hagGetEventStatus(
    HagEvent    event);

HAG_API HagResult hagResetEvent(
    HagEvent    event);

HAG_API HagResult hagWaitForEvents(
    uint32_t    eventCount,
    HagEvent*   pEvents,
    uint64_t    timeoutNs);

typedef enum HagSemaphoreType {
    HAG_SEMAPHORE_BINARY = 0,
    HAG_SEMAPHORE_TIMELINE = 1,
} HagSemaphoreType;

typedef struct HagSemaphoreCreateInfo {
    HagSemaphoreType    type;
    uint64_t            initialValue;
} HagSemaphoreCreateInfo;

HAG_API HagResult hagCreateSemaphore(
    HagDevice                       device,
    const HagSemaphoreCreateInfo*   pCreateInfo,
    HagSemaphore*                   pSemaphore);

HAG_API void hagDestroySemaphore(
    HagDevice       device,
    HagSemaphore    semaphore);

HAG_API HagResult hagSignalSemaphore(
    HagDevice       device,
    HagSemaphore    semaphore,
    uint64_t        value);

HAG_API HagResult hagWaitSemaphores(
    HagDevice       device,
    uint32_t        semaphoreCount,
    HagSemaphore*   pSemaphores,
    const uint64_t* pValues,
    uint64_t        timeoutNs);

/* ============================================================================
 * Allocator Interface
 * ============================================================================ */

typedef struct HagAllocationCallbacks {
    void*       pUserData;
    void*       (*pfnAllocate)(void* pUserData, size_t size, size_t alignment);
    void        (*pfnFree)(void* pUserData, void* pMemory);
    void*       (*pfnReallocate)(void* pUserData, void* pOriginal, size_t size, size_t alignment);
} HagAllocationCallbacks;

HAG_API void hagSetDefaultAllocator(
    const HagAllocationCallbacks* pAllocator);

/* ============================================================================
 * Utilities
 * ============================================================================ */

HAG_API const char* hagResultToString(
    HagResult result);

HAG_API uint32_t hagGetApiVersion(void);

HAG_API size_t hagGetDTypeSize(
    HagDType dtype);

#ifdef __cplusplus
}
#endif

#endif /* HAG_API_H */
