/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#include "hag/driver/cpu/cpu_driver.h"
#include "hag/driver/cpu/commands.h"
#include "hag/core/instance.h"
#include "hag/utils/platform.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ============================================================================
 * CPU Driver Internal Structures
 * ============================================================================ */

typedef struct CpuBuffer {
    uint64_t    size;
    uint32_t    usage;
    void*       data;
} CpuBuffer;

typedef struct CpuMemory {
    uint64_t    size;
    uint32_t    type;
    void*       data;
    int         mapped;
} CpuMemory;

typedef struct CpuQueue {
    HagQueueType    type;
} CpuQueue;

typedef struct CpuFence {
    int         signaled;
    HagMutex    mutex;
} CpuFence;

typedef struct CpuKernel {
    char        name[64];
    void*       funcPtr;
    void*       userData;
} CpuKernel;

typedef struct CpuDriverData {
    HagAllocationCallbacks    allocator;
    uint32_t                  nextBufferId;
    uint32_t                  nextKernelId;
} CpuDriverData;

/* ============================================================================
 * Built-in CPU Kernels
 * ============================================================================ */

typedef void (*CpuKernelFunc)(uint32_t tid, uint32_t gid, void* userData);

/* Union for safe function pointer <-> object pointer conversion */
union CpuFuncPtrConverter {
    void*           ptr;
    CpuKernelFunc   func;
};

static void cpuKernelAddF32(uint32_t tid, uint32_t gid, void* userData) {
    (void)gid;
    typedef struct { float* a; float* b; float* c; uint64_t n; } Args;
    Args* args = (Args*)userData;
    if (tid < args->n) {
        args->c[tid] = args->a[tid] + args->b[tid];
    }
}

/* ============================================================================
 * CPU Driver Operations
 * ============================================================================ */

static HagResult cpuDriverProbe(int drmFd, uint32_t vendorId, uint32_t deviceId, int* pScore) {
    (void)vendorId;
    (void)deviceId;
    
    if (drmFd < 0) {
        *pScore = HAG_DRIVER_SCORE_BEST;
        return HAG_SUCCESS;
    }
    
    /* Don't claim real GPU devices */
    *pScore = HAG_DRIVER_SCORE_NONE;
    return HAG_SUCCESS;
}

static HagResult cpuDriverCreate(int drmFd, const HagAllocationCallbacks* pAlloc, void** ppDriverData) {
    (void)drmFd;
    
    CpuDriverData* data = (CpuDriverData*)hagAllocate(pAlloc, sizeof(CpuDriverData), sizeof(void*));
    if (!data) return HAG_ERROR_OUT_OF_MEMORY;
    
    memset(data, 0, sizeof(*data));
    if (pAlloc) {
        data->allocator = *pAlloc;
    }
    
    *ppDriverData = data;
    return HAG_SUCCESS;
}

static void cpuDriverDestroy(void* pDriverData) {
    if (!pDriverData) return;
    CpuDriverData* data = (CpuDriverData*)pDriverData;
    hagFree(&data->allocator, data);
}

static HagResult cpuDriverGetDeviceProperties(void* pDriverData, HagDeviceProperties* pProps) {
    (void)pDriverData;
    if (!pProps) return HAG_ERROR_INVALID_ARGUMENT;
    
    memset(pProps, 0, sizeof(*pProps));
    pProps->apiVersion = hagGetApiVersion();
    pProps->driverVersion = 1;
    pProps->vendorId = HAG_VENDOR_UNKNOWN;
    pProps->deviceType = HAG_DEVICE_TYPE_CPU_FALLBACK;
    strncpy(pProps->deviceName, "Hag CPU Fallback", sizeof(pProps->deviceName) - 1);
    pProps->deviceLocalMemoryBytes = 0;
    pProps->sharedMemoryBytes = (uint64_t)1024 * 1024 * 1024;
    pProps->computeUnits = 1;
    pProps->maxWorkGroupSize[0] = 1;
    pProps->maxWorkGroupSize[1] = 1;
    pProps->maxWorkGroupSize[2] = 1;
    pProps->maxTensorDimensions = 8;
    pProps->warpSize = 1;
    pProps->clockFrequencyMHz = 0;
    
    return HAG_SUCCESS;
}

/* ============================================================================
 * Memory Operations
 * ============================================================================ */

static HagResult cpuDriverAllocMemory(void* pDriverData, uint64_t size, uint32_t memoryType, HagDriverMemory* pMemory) {
    (void)pDriverData;
    (void)memoryType;
    if (!pMemory || size == 0) return HAG_ERROR_INVALID_ARGUMENT;
    
    CpuMemory* mem = (CpuMemory*)malloc(sizeof(CpuMemory));
    if (!mem) return HAG_ERROR_OUT_OF_MEMORY;
    
    mem->size = size;
    mem->type = memoryType;
    mem->mapped = 0;
    mem->data = hagPlatformAllocAligned((size_t)size, 64);
    
    if (!mem->data) {
        free(mem);
        return HAG_ERROR_OUT_OF_MEMORY;
    }
    
    *pMemory = (HagDriverMemory)mem;
    return HAG_SUCCESS;
}

static void cpuDriverFreeMemory(void* pDriverData, HagDriverMemory memory) {
    (void)pDriverData;
    if (!memory) return;
    
    CpuMemory* mem = (CpuMemory*)memory;
    if (mem->data) hagPlatformFree(mem->data);
    free(mem);
}

static HagResult cpuDriverMapMemory(void* pDriverData, HagDriverMemory memory, void** ppData) {
    (void)pDriverData;
    if (!memory || !ppData) return HAG_ERROR_INVALID_ARGUMENT;
    
    CpuMemory* mem = (CpuMemory*)memory;
    *ppData = mem->data;
    mem->mapped = 1;
    return HAG_SUCCESS;
}

static void cpuDriverUnmapMemory(void* pDriverData, HagDriverMemory memory) {
    (void)pDriverData;
    if (!memory) return;
    ((CpuMemory*)memory)->mapped = 0;
}

static HagResult cpuDriverFlushMemory(void* pDriverData, HagDriverMemory memory, uint64_t offset, uint64_t size) {
    (void)pDriverData; (void)memory; (void)offset; (void)size;
    return HAG_SUCCESS;
}

static HagResult cpuDriverInvalidateMemory(void* pDriverData, HagDriverMemory memory, uint64_t offset, uint64_t size) {
    (void)pDriverData; (void)memory; (void)offset; (void)size;
    return HAG_SUCCESS;
}

/* ============================================================================
 * Buffer Operations
 * ============================================================================ */

static HagResult cpuDriverCreateBuffer(void* pDriverData, uint64_t size, uint32_t usage, HagDriverBuffer* pBuffer) {
    (void)pDriverData;
    if (!pBuffer || size == 0) return HAG_ERROR_INVALID_ARGUMENT;
    
    CpuBuffer* buf = (CpuBuffer*)malloc(sizeof(CpuBuffer));
    if (!buf) return HAG_ERROR_OUT_OF_MEMORY;
    
    buf->size = size;
    buf->usage = usage;
    buf->data = hagPlatformAllocAligned((size_t)size, 64);
    
    if (!buf->data) {
        free(buf);
        return HAG_ERROR_OUT_OF_MEMORY;
    }
    
    *pBuffer = (HagDriverBuffer)buf;
    return HAG_SUCCESS;
}

static void cpuDriverDestroyBuffer(void* pDriverData, HagDriverBuffer buffer) {
    (void)pDriverData;
    if (!buffer) return;
    
    CpuBuffer* buf = (CpuBuffer*)buffer;
    if (buf->data) hagPlatformFree(buf->data);
    free(buf);
}

static HagResult cpuDriverBindBufferMemory(void* pDriverData, HagDriverBuffer buffer, HagDriverMemory memory, uint64_t offset) {
    (void)pDriverData; (void)buffer; (void)memory; (void)offset;
    return HAG_SUCCESS;
}

static HagResult cpuDriverGetBufferMemoryRequirements(void* pDriverData, HagDriverBuffer buffer, uint64_t* pSize, uint32_t* pAlignment) {
    (void)pDriverData;
    if (!buffer) return HAG_ERROR_INVALID_ARGUMENT;
    
    CpuBuffer* buf = (CpuBuffer*)buffer;
    if (pSize) *pSize = buf->size;
    if (pAlignment) *pAlignment = 64;
    return HAG_SUCCESS;
}

static HagResult cpuDriverMapBuffer(void* pDriverData, HagDriverBuffer buffer, void** ppData) {
    (void)pDriverData;
    if (!buffer || !ppData) return HAG_ERROR_INVALID_ARGUMENT;
    
    CpuBuffer* buf = (CpuBuffer*)buffer;
    *ppData = buf->data;
    return HAG_SUCCESS;
}

static void cpuDriverUnmapBuffer(void* pDriverData, HagDriverBuffer buffer) {
    (void)pDriverData;
    (void)buffer;
    /* No-op for CPU driver */
}

/* ============================================================================
 * Queue & Execution
 * ============================================================================ */

static HagResult cpuDriverCreateQueue(void* pDriverData, HagQueueType type, uint32_t* pQueueFamilyIndex, HagDriverQueue* pQueue) {
    (void)pDriverData;
    if (!pQueue) return HAG_ERROR_INVALID_ARGUMENT;
    
    CpuQueue* queue = (CpuQueue*)malloc(sizeof(CpuQueue));
    if (!queue) return HAG_ERROR_OUT_OF_MEMORY;
    
    queue->type = type;
    
    if (pQueueFamilyIndex) *pQueueFamilyIndex = 0;
    *pQueue = (HagDriverQueue)queue;
    return HAG_SUCCESS;
}

static void cpuDriverDestroyQueue(void* pDriverData, HagDriverQueue queue) {
    (void)pDriverData;
    free((CpuQueue*)queue);
}

static HagResult cpuExecuteCommands(CpuCmdBuffer* cmdBuf) {
    CpuExecutionContext ctx = {0};
    
    for (uint32_t i = 0; i < cmdBuf->commandCount; i++) {
        CpuCmd* cmd = &cmdBuf->commands[i];
        switch (cmd->type) {
            case CPU_CMD_BIND_KERNEL:
                ctx.currentKernel = cmd->params.bindKernel.kernelFunc;
                ctx.currentUserData = cmd->params.bindKernel.userData;
                break;
            case CPU_CMD_DISPATCH: {
                if (!ctx.currentKernel) break;
                union CpuFuncPtrConverter converter;
                converter.ptr = ctx.currentKernel;
                CpuKernelFunc func = converter.func;
                uint32_t total = cmd->params.dispatch.groupsX * 
                                 cmd->params.dispatch.groupsY * 
                                 cmd->params.dispatch.groupsZ;
                for (uint32_t tid = 0; tid < total; tid++) {
                    func(tid, 0, ctx.currentUserData);
                }
                break;
            }
            case CPU_CMD_COPY_BUFFER: {
                CpuBuffer* s = (CpuBuffer*)cmd->params.copyBuffer.src;
                CpuBuffer* d = (CpuBuffer*)cmd->params.copyBuffer.dst;
                memcpy((char*)d->data + cmd->params.copyBuffer.dstOffset,
                       (char*)s->data + cmd->params.copyBuffer.srcOffset,
                       (size_t)cmd->params.copyBuffer.size);
                break;
            }
            case CPU_CMD_FILL_BUFFER: {
                CpuBuffer* b = (CpuBuffer*)cmd->params.fillBuffer.buffer;
                memset((char*)b->data + cmd->params.fillBuffer.offset,
                       (int)cmd->params.fillBuffer.data,
                       (size_t)cmd->params.fillBuffer.size);
                break;
            }
            case CPU_CMD_BARRIER:
                /* No-op on CPU */
                break;
        }
    }
    return HAG_SUCCESS;
}

static HagResult cpuDriverSubmit(void* pDriverData, HagDriverQueue queue, uint32_t cmdBufferCount, void** ppCmdBuffers, HagDriverFence fence) {
    (void)pDriverData;
    (void)queue;
    
    for (uint32_t i = 0; i < cmdBufferCount; i++) {
        CpuCmdBuffer* cmdBuf = (CpuCmdBuffer*)ppCmdBuffers[i];
        if (!cmdBuf || !cmdBuf->executable) continue;
        cpuExecuteCommands(cmdBuf);
    }
    
    if (fence) {
        CpuFence* f = (CpuFence*)fence;
        hagPlatformMutexLock(f->mutex);
        f->signaled = 1;
        hagPlatformMutexUnlock(f->mutex);
    }
    
    return HAG_SUCCESS;
}

static HagResult cpuDriverWaitQueueIdle(void* pDriverData, HagDriverQueue queue) {
    (void)pDriverData; (void)queue;
    return HAG_SUCCESS;
}

/* ============================================================================
 * Command Buffer
 * ============================================================================ */

static HagResult cpuDriverCreateCmdBuffer(void* pDriverData, HagDriverQueue queue, void** ppCmdBuffer) {
    (void)pDriverData; (void)queue;
    if (!ppCmdBuffer) return HAG_ERROR_INVALID_ARGUMENT;
    
    CpuCmdBuffer* cmd = (CpuCmdBuffer*)calloc(1, sizeof(CpuCmdBuffer));
    if (!cmd) return HAG_ERROR_OUT_OF_MEMORY;
    
    cmd->commandCapacity = 64;
    cmd->commands = (CpuCmd*)calloc(cmd->commandCapacity, sizeof(CpuCmd));
    if (!cmd->commands) {
        free(cmd);
        return HAG_ERROR_OUT_OF_MEMORY;
    }
    
    *ppCmdBuffer = cmd;
    return HAG_SUCCESS;
}

static void cpuDriverDestroyCmdBuffer(void* pDriverData, void* pCmdBuffer) {
    (void)pDriverData;
    if (!pCmdBuffer) return;
    CpuCmdBuffer* cmd = (CpuCmdBuffer*)pCmdBuffer;
    free(cmd->commands);
    free(cmd);
}

static HagResult cpuDriverBeginCmdBuffer(void* pDriverData, void* pCmdBuffer, uint32_t flags) {
    (void)pDriverData;
    if (!pCmdBuffer) return HAG_ERROR_INVALID_ARGUMENT;
    
    CpuCmdBuffer* cmd = (CpuCmdBuffer*)pCmdBuffer;
    cmd->recording = 1;
    cmd->executable = 0;
    cmd->flags = flags;
    cmd->commandCount = 0;
    return HAG_SUCCESS;
}

static HagResult cpuDriverEndCmdBuffer(void* pDriverData, void* pCmdBuffer) {
    (void)pDriverData;
    if (!pCmdBuffer) return HAG_ERROR_INVALID_ARGUMENT;
    
    CpuCmdBuffer* cmd = (CpuCmdBuffer*)pCmdBuffer;
    cmd->recording = 0;
    cmd->executable = 1;
    return HAG_SUCCESS;
}

static HagResult cpuDriverResetCmdBuffer(void* pDriverData, void* pCmdBuffer, uint32_t flags) {
    (void)pDriverData; (void)flags;
    if (!pCmdBuffer) return HAG_ERROR_INVALID_ARGUMENT;
    
    CpuCmdBuffer* cmd = (CpuCmdBuffer*)pCmdBuffer;
    cmd->recording = 0;
    cmd->executable = 0;
    cmd->commandCount = 0;
    return HAG_SUCCESS;
}

/* ============================================================================
 * Commands
 * ============================================================================ */

static HagResult cpuDriverAppendCmd(CpuCmdBuffer* cmdBuf, CpuCmd* newCmd) {
    if (!cmdBuf->recording) return HAG_ERROR_INVALID_ARGUMENT;
    if (cmdBuf->commandCount >= cmdBuf->commandCapacity) {
        uint32_t newCap = cmdBuf->commandCapacity * 2;
        CpuCmd* newCmds = (CpuCmd*)realloc(cmdBuf->commands, newCap * sizeof(CpuCmd));
        if (!newCmds) return HAG_ERROR_OUT_OF_MEMORY;
        cmdBuf->commands = newCmds;
        cmdBuf->commandCapacity = newCap;
    }
    cmdBuf->commands[cmdBuf->commandCount++] = *newCmd;
    return HAG_SUCCESS;
}

static HagResult cpuDriverCmdBindKernel(void* pDriverData, void* pCmdBuffer, HagDriverKernel kernel) {
    (void)pDriverData;
    if (!pCmdBuffer || !kernel) return HAG_ERROR_INVALID_ARGUMENT;
    
    CpuKernel* k = (CpuKernel*)kernel;
    CpuCmd cmd = {0};
    cmd.type = CPU_CMD_BIND_KERNEL;
    cmd.params.bindKernel.kernelFunc = k->funcPtr;
    cmd.params.bindKernel.userData = k->userData;
    return cpuDriverAppendCmd((CpuCmdBuffer*)pCmdBuffer, &cmd);
}

static HagResult cpuDriverCmdBindBuffer(void* pDriverData, void* pCmdBuffer, uint32_t binding, HagDriverBuffer buffer, uint64_t offset) {
    (void)pDriverData; (void)pCmdBuffer; (void)binding; (void)buffer; (void)offset;
    /* CPU driver doesn't need explicit buffer binding in commands */
    return HAG_SUCCESS;
}

static HagResult cpuDriverCmdDispatch(void* pDriverData, void* pCmdBuffer, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) {
    (void)pDriverData;
    if (!pCmdBuffer) return HAG_ERROR_INVALID_ARGUMENT;
    
    CpuCmd cmd = {0};
    cmd.type = CPU_CMD_DISPATCH;
    cmd.params.dispatch.groupsX = groupsX;
    cmd.params.dispatch.groupsY = groupsY;
    cmd.params.dispatch.groupsZ = groupsZ;
    return cpuDriverAppendCmd((CpuCmdBuffer*)pCmdBuffer, &cmd);
}

static HagResult cpuDriverCmdCopyBuffer(void* pDriverData, void* pCmdBuffer, HagDriverBuffer src, HagDriverBuffer dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size) {
    (void)pDriverData;
    if (!pCmdBuffer || !src || !dst) return HAG_ERROR_INVALID_ARGUMENT;
    
    CpuCmd cmd = {0};
    cmd.type = CPU_CMD_COPY_BUFFER;
    cmd.params.copyBuffer.src = src;
    cmd.params.copyBuffer.dst = dst;
    cmd.params.copyBuffer.srcOffset = srcOffset;
    cmd.params.copyBuffer.dstOffset = dstOffset;
    cmd.params.copyBuffer.size = size;
    return cpuDriverAppendCmd((CpuCmdBuffer*)pCmdBuffer, &cmd);
}

static HagResult cpuDriverCmdFillBuffer(void* pDriverData, void* pCmdBuffer, HagDriverBuffer buffer, uint64_t offset, uint64_t size, uint32_t data) {
    (void)pDriverData;
    (void)pCmdBuffer;
    
    if (!buffer) return HAG_ERROR_INVALID_ARGUMENT;
    
    CpuBuffer* buf = (CpuBuffer*)buffer;
    uint8_t* ptr = (uint8_t*)buf->data + offset;
    
    /* Fill byte-by-byte for pattern support */
    /* In v0.1, we treat data as a byte pattern for memset */
    /* For proper multi-byte patterns, we'd need a loop */
    if (size > 0) {
        memset(ptr, (int)(data & 0xFF), (size_t)size);
    }
    return HAG_SUCCESS;
}

static HagResult cpuDriverCmdBarrier(void* pDriverData, void* pCmdBuffer, uint32_t srcStage, uint32_t dstStage) {
    (void)pDriverData; (void)srcStage; (void)dstStage;
    if (!pCmdBuffer) return HAG_ERROR_INVALID_ARGUMENT;
    
    CpuCmd cmd = {0};
    cmd.type = CPU_CMD_BARRIER;
    return cpuDriverAppendCmd((CpuCmdBuffer*)pCmdBuffer, &cmd);
}

/* ============================================================================
 * Kernels
 * ============================================================================ */

static HagResult cpuDriverCreateKernel(void* pDriverData, const void* pCode, size_t codeSize, HagKernelLanguage lang, HagDriverKernel* pKernel) {
    (void)pDriverData; (void)codeSize;
    if (!pKernel) return HAG_ERROR_INVALID_ARGUMENT;
    
    CpuKernel* k = (CpuKernel*)calloc(1, sizeof(CpuKernel));
    if (!k) return HAG_ERROR_OUT_OF_MEMORY;
    
    /* For CPU driver, we support built-in kernels by name */
    if (lang == HAG_KERNEL_LANGUAGE_HGSL && pCode) {
        const char* name = (const char*)pCode;
        strncpy(k->name, name, sizeof(k->name) - 1);
        
        if (strcmp(name, "add_f32") == 0) {
            union CpuFuncPtrConverter converter;
            converter.func = cpuKernelAddF32;
            k->funcPtr = converter.ptr;
        } else {
            k->funcPtr = NULL;
        }
    }
    
    *pKernel = (HagDriverKernel)k;
    return HAG_SUCCESS;
}

static void cpuDriverDestroyKernel(void* pDriverData, HagDriverKernel kernel) {
    (void)pDriverData;
    free((CpuKernel*)kernel);
}

static HagResult cpuDriverGetKernelWorkGroupSize(void* pDriverData, HagDriverKernel kernel, uint32_t* pSize) {
    (void)pDriverData; (void)kernel;
    if (!pSize) return HAG_ERROR_INVALID_ARGUMENT;
    *pSize = 1;
    return HAG_SUCCESS;
}

/* ============================================================================
 * Synchronization
 * ============================================================================ */

static HagResult cpuDriverCreateFence(void* pDriverData, HagDriverFence* pFence) {
    (void)pDriverData;
    if (!pFence) return HAG_ERROR_INVALID_ARGUMENT;
    
    CpuFence* fence = (CpuFence*)calloc(1, sizeof(CpuFence));
    if (!fence) return HAG_ERROR_OUT_OF_MEMORY;
    
    fence->mutex = hagPlatformMutexCreate();
    *pFence = (HagDriverFence)fence;
    return HAG_SUCCESS;
}

static void cpuDriverDestroyFence(void* pDriverData, HagDriverFence fence) {
    (void)pDriverData;
    if (!fence) return;
    CpuFence* f = (CpuFence*)fence;
    hagPlatformMutexDestroy(f->mutex);
    free(f);
}

static HagResult cpuDriverResetFence(void* pDriverData, HagDriverFence fence) {
    (void)pDriverData;
    if (!fence) return HAG_ERROR_INVALID_ARGUMENT;
    
    CpuFence* f = (CpuFence*)fence;
    hagPlatformMutexLock(f->mutex);
    f->signaled = 0;
    hagPlatformMutexUnlock(f->mutex);
    return HAG_SUCCESS;
}

static HagResult cpuDriverWaitFences(void* pDriverData, uint32_t count, HagDriverFence* pFences, uint64_t timeout) {
    (void)pDriverData; (void)timeout;
    if (!pFences && count > 0) return HAG_ERROR_INVALID_ARGUMENT;
    
    for (uint32_t i = 0; i < count; i++) {
        CpuFence* f = (CpuFence*)pFences[i];
        if (!f) continue;
        
        int signaled = 0;
        while (!signaled) {
            hagPlatformMutexLock(f->mutex);
            signaled = f->signaled;
            hagPlatformMutexUnlock(f->mutex);
            if (!signaled) hagPlatformSleepNanos(1000);
        }
    }
    return HAG_SUCCESS;
}

static HagResult cpuDriverGetTimestampFrequency(void* pDriverData, uint64_t* pFrequency) {
    (void)pDriverData;
    if (!pFrequency) return HAG_ERROR_INVALID_ARGUMENT;
    *pFrequency = 1000000000ULL;
    return HAG_SUCCESS;
}

/* ============================================================================
 * Driver Ops Table
 * ============================================================================ */

const HagDriverOps g_hagCpuDriverOps = {
    .probe = cpuDriverProbe,
    .create = cpuDriverCreate,
    .destroy = cpuDriverDestroy,
    .getDeviceProperties = cpuDriverGetDeviceProperties,
    .allocMemory = cpuDriverAllocMemory,
    .freeMemory = cpuDriverFreeMemory,
    .mapMemory = cpuDriverMapMemory,
    .unmapMemory = cpuDriverUnmapMemory,
    .flushMemory = cpuDriverFlushMemory,
    .invalidateMemory = cpuDriverInvalidateMemory,
    .createBuffer = cpuDriverCreateBuffer,
    .destroyBuffer = cpuDriverDestroyBuffer,
    .bindBufferMemory = cpuDriverBindBufferMemory,
    .getBufferMemoryRequirements = cpuDriverGetBufferMemoryRequirements,
    .mapBuffer = cpuDriverMapBuffer,
    .unmapBuffer = cpuDriverUnmapBuffer,
    .createQueue = cpuDriverCreateQueue,
    .destroyQueue = cpuDriverDestroyQueue,
    .submit = cpuDriverSubmit,
    .waitQueueIdle = cpuDriverWaitQueueIdle,
    .createCmdBuffer = cpuDriverCreateCmdBuffer,
    .destroyCmdBuffer = cpuDriverDestroyCmdBuffer,
    .beginCmdBuffer = cpuDriverBeginCmdBuffer,
    .endCmdBuffer = cpuDriverEndCmdBuffer,
    .resetCmdBuffer = cpuDriverResetCmdBuffer,
    .cmdBindKernel = cpuDriverCmdBindKernel,
    .cmdBindBuffer = cpuDriverCmdBindBuffer,
    .cmdDispatch = cpuDriverCmdDispatch,
    .cmdCopyBuffer = cpuDriverCmdCopyBuffer,
    .cmdFillBuffer = cpuDriverCmdFillBuffer,
    .cmdBarrier = cpuDriverCmdBarrier,
    .createKernel = cpuDriverCreateKernel,
    .destroyKernel = cpuDriverDestroyKernel,
    .getKernelWorkGroupSize = cpuDriverGetKernelWorkGroupSize,
    .createFence = cpuDriverCreateFence,
    .destroyFence = cpuDriverDestroyFence,
    .resetFence = cpuDriverResetFence,
    .waitFences = cpuDriverWaitFences,
    .getTimestampFrequency = cpuDriverGetTimestampFrequency,
};
