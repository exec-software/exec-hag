/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#ifndef HAG_DRIVER_INTERFACE_H
#define HAG_DRIVER_INTERFACE_H

#include "hag/api/api.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal driver interface - NOT part of public API.
 * All drivers implement this interface to provide hardware abstraction.
 */

/* Opaque driver types */
typedef struct HagDriverMemory_T*   HagDriverMemory;
typedef struct HagDriverBuffer_T*   HagDriverBuffer;
typedef struct HagDriverQueue_T*    HagDriverQueue;
typedef struct HagDriverKernel_T*   HagDriverKernel;
typedef struct HagDriverFence_T*    HagDriverFence;

/* Driver capability score - higher is better match */
#define HAG_DRIVER_SCORE_NONE 0
#define HAG_DRIVER_SCORE_LOW  10
#define HAG_DRIVER_SCORE_OK   50
#define HAG_DRIVER_SCORE_GOOD 80
#define HAG_DRIVER_SCORE_BEST 100

typedef struct HagDriverOps {
    /* Lifecycle */
    HagResult (*probe)(int drmFd, uint32_t vendorId, uint32_t deviceId, int* pScore);
    HagResult (*create)(int drmFd, const HagAllocationCallbacks* pAlloc, void** ppDriverData);
    void      (*destroy)(void* pDriverData);

    /* Device info */
    HagResult (*getDeviceProperties)(void* pDriverData, HagDeviceProperties* pProps);

    /* Memory (raw allocations) */
    HagResult (*allocMemory)(void* pDriverData, uint64_t size, uint32_t memoryType, HagDriverMemory* pMemory);
    void      (*freeMemory)(void* pDriverData, HagDriverMemory memory);
    HagResult (*mapMemory)(void* pDriverData, HagDriverMemory memory, void** ppData);
    void      (*unmapMemory)(void* pDriverData, HagDriverMemory memory);
    HagResult (*flushMemory)(void* pDriverData, HagDriverMemory memory, uint64_t offset, uint64_t size);
    HagResult (*invalidateMemory)(void* pDriverData, HagDriverMemory memory, uint64_t offset, uint64_t size);

    /* Buffers (typed objects backed by memory) */
    HagResult (*createBuffer)(void* pDriverData, uint64_t size, uint32_t usage, HagDriverBuffer* pBuffer);
    void      (*destroyBuffer)(void* pDriverData, HagDriverBuffer buffer);
    HagResult (*bindBufferMemory)(void* pDriverData, HagDriverBuffer buffer, HagDriverMemory memory, uint64_t offset);
    HagResult (*getBufferMemoryRequirements)(void* pDriverData, HagDriverBuffer buffer, uint64_t* pSize, uint32_t* pAlignment);
    HagResult (*mapBuffer)(void* pDriverData, HagDriverBuffer buffer, void** ppData);
    void      (*unmapBuffer)(void* pDriverData, HagDriverBuffer buffer);

    /* Queue & execution */
    HagResult (*createQueue)(void* pDriverData, HagQueueType type, uint32_t* pQueueFamilyIndex, HagDriverQueue* pQueue);
    void      (*destroyQueue)(void* pDriverData, HagDriverQueue queue);
    HagResult (*submit)(void* pDriverData, HagDriverQueue queue, uint32_t cmdBufferCount, void** ppCmdBuffers, HagDriverFence fence);
    HagResult (*waitQueueIdle)(void* pDriverData, HagDriverQueue queue);

    /* Command buffer building (driver-specific) */
    HagResult (*createCmdBuffer)(void* pDriverData, HagDriverQueue queue, void** ppCmdBuffer);
    void      (*destroyCmdBuffer)(void* pDriverData, void* pCmdBuffer);
    HagResult (*beginCmdBuffer)(void* pDriverData, void* pCmdBuffer, uint32_t flags);
    HagResult (*endCmdBuffer)(void* pDriverData, void* pCmdBuffer);
    HagResult (*resetCmdBuffer)(void* pDriverData, void* pCmdBuffer, uint32_t flags);

    /* Commands */
    HagResult (*cmdBindKernel)(void* pDriverData, void* pCmdBuffer, HagDriverKernel kernel);
    HagResult (*cmdBindBuffer)(void* pDriverData, void* pCmdBuffer, uint32_t binding, HagDriverBuffer buffer, uint64_t offset);
    HagResult (*cmdDispatch)(void* pDriverData, void* pCmdBuffer, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ);
    HagResult (*cmdCopyBuffer)(void* pDriverData, void* pCmdBuffer, HagDriverBuffer src, HagDriverBuffer dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size);
    HagResult (*cmdFillBuffer)(void* pDriverData, void* pCmdBuffer, HagDriverBuffer buffer, uint64_t offset, uint64_t size, uint32_t data);
    HagResult (*cmdBarrier)(void* pDriverData, void* pCmdBuffer, uint32_t srcStage, uint32_t dstStage);

    /* Kernels */
    HagResult (*createKernel)(void* pDriverData, const void* pCode, size_t codeSize, HagKernelLanguage lang, HagDriverKernel* pKernel);
    void      (*destroyKernel)(void* pDriverData, HagDriverKernel kernel);
    HagResult (*getKernelWorkGroupSize)(void* pDriverData, HagDriverKernel kernel, uint32_t* pSize);

    /* Synchronization */
    HagResult (*createFence)(void* pDriverData, HagDriverFence* pFence);
    void      (*destroyFence)(void* pDriverData, HagDriverFence fence);
    HagResult (*resetFence)(void* pDriverData, HagDriverFence fence);
    HagResult (*waitFences)(void* pDriverData, uint32_t count, HagDriverFence* pFences, uint64_t timeout);

    /* Profiling */
    HagResult (*getTimestampFrequency)(void* pDriverData, uint64_t* pFrequency);
} HagDriverOps;

/* Driver registration */
typedef struct HagDriverRegistration {
    const char*     name;
    const HagDriverOps* ops;
    uint32_t        priority;
} HagDriverRegistration;

void hagDriverRegister(const HagDriverRegistration* registration);
const HagDriverRegistration* hagDriverFind(int drmFd, uint32_t vendorId, uint32_t deviceId);

/* Get all registered drivers */
uint32_t hagDriverGetAll(const HagDriverRegistration*** ppRegistrations);

#ifdef __cplusplus
}
#endif

#endif /* HAG_DRIVER_INTERFACE_H */
