/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 *
 * AMDGPU Driver Skeleton
 *
 * This is the initial skeleton for AMD GPU support via the amdgpu kernel driver.
 * Full implementation requires libdrm-dev and Mesa knowledge.
 */

#include "hag/driver/amdgpu/amdgpu_driver.h"

#if defined(HAG_HAS_LIBDRM)

#include "hag/core/instance.h"
#include "hag/utils/platform.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

/* DRM/amdgpu headers */
#include <xf86drm.h>
#include <amdgpu_drm.h>

/* ============================================================================
 * AMDGPU Driver Operations
 * ============================================================================ */

static HagResult amdgpuDriverProbe(int drmFd, uint32_t vendorId, uint32_t deviceId, int* pScore) {
    (void)deviceId;
    
    if (vendorId == HAG_VENDOR_AMD) {
        /* Verify amdgpu driver by checking DRM version */
        drmVersionPtr ver = drmGetVersion(drmFd);
        if (ver) {
            int isAmdgpu = (strcmp(ver->name, "amdgpu") == 0);
            drmFreeVersion(ver);
            if (isAmdgpu) {
                *pScore = HAG_DRIVER_SCORE_BEST;
                return HAG_SUCCESS;
            }
        }
    }
    
    *pScore = HAG_DRIVER_SCORE_NONE;
    return HAG_SUCCESS;
}

static HagResult amdgpuDriverCreate(int drmFd, const HagAllocationCallbacks* pAlloc, void** ppDriverData) {
    (void)pAlloc;
    
    AmdGpuDriverData* data = (AmdGpuDriverData*)calloc(1, sizeof(AmdGpuDriverData));
    if (!data) return HAG_ERROR_OUT_OF_MEMORY;
    
    data->drmFd = drmFd;
    
    /* Query GPU info via amdgpu ioctl */
    struct drm_amdgpu_info request;
    memset(&request, 0, sizeof(request));
    
    /* Get device info */
    request.query = AMDGPU_INFO_DEV_INFO;
    struct drm_amdgpu_info_device devInfo;
    memset(&devInfo, 0, sizeof(devInfo));
    request.return_pointer = (uint64_t)(uintptr_t)&devInfo;
    request.return_size = sizeof(devInfo);
    
    int ret = drmCommandWriteRead(drmFd, DRM_AMDGPU_INFO, &request, sizeof(request));
    if (ret == 0) {
        data->family = devInfo.family;
        data->chipClass = devInfo.external_rev;
        data->computeUnits = devInfo.cu_active_number;
        data->clockFrequency = (uint32_t)(devInfo.max_engine_clock / 1000); /* Convert from kHz to MHz */
        
        /* Query memory info */
        struct drm_amdgpu_memory_info memInfo;
        memset(&memInfo, 0, sizeof(memInfo));
        request.query = AMDGPU_INFO_MEMORY;
        request.return_pointer = (uint64_t)(uintptr_t)&memInfo;
        request.return_size = sizeof(memInfo);
        int memRet = drmCommandWriteRead(drmFd, DRM_AMDGPU_INFO, &request, sizeof(request));
        if (memRet == 0) {
            data->vramSize = memInfo.vram.total_heap_size;
            data->gttSize = memInfo.gtt.total_heap_size;
        }
        
        /* Determine wavefront size and features based on family */
        switch (data->family) {
            case AMDGPU_FAMILY_NV:
            case AMDGPU_FAMILY_VGH:
            case AMDGPU_FAMILY_GC_10_3_6:
            case AMDGPU_FAMILY_GC_10_3_7:
                data->waveFrontSize = 32;
                data->hasWave32 = 1;
                data->hasRayTracing = 1;
                break;
            case AMDGPU_FAMILY_GC_11_0_0:
            case AMDGPU_FAMILY_GC_11_0_1:
            case AMDGPU_FAMILY_GC_11_5_0:
                data->waveFrontSize = 32;
                data->hasWave32 = 1;
                data->hasRayTracing = 1;
                data->hasMatrixCores = 1; /* WMMA/DPAS on RDNA3+ */
                break;
            default:
                data->waveFrontSize = 64;
                data->hasWave32 = 0;
                break;
        }
    } else {
        /* Fallback: use basic info from PCI */
        data->family = 0;
        data->waveFrontSize = 64;
    }
    
    /* Get firmware versions */
    request.query = AMDGPU_INFO_FW_VERSION;
    request.query_fw.fw_type = AMDGPU_INFO_FW_GFX_ME;
    uint32_t meVersion = 0;
    request.return_pointer = (uint64_t)(uintptr_t)&meVersion;
    request.return_size = sizeof(meVersion);
    drmCommandWriteRead(drmFd, DRM_AMDGPU_INFO, &request, sizeof(request));
    data->meFirmwareVersion = meVersion;
    
    *ppDriverData = data;
    return HAG_SUCCESS;
}

static void amdgpuDriverDestroy(void* pDriverData) {
    if (!pDriverData) return;
    AmdGpuDriverData* data = (AmdGpuDriverData*)pDriverData;
    
    if (data->drmFd >= 0) {
        close(data->drmFd);
    }
    free(data);
}

static HagResult amdgpuDriverGetDeviceProperties(void* pDriverData, HagDeviceProperties* pProps) {
    if (!pDriverData || !pProps) return HAG_ERROR_INVALID_ARGUMENT;
    
    AmdGpuDriverData* data = (AmdGpuDriverData*)pDriverData;
    memset(pProps, 0, sizeof(*pProps));
    
    pProps->apiVersion = hagGetApiVersion();
    pProps->driverVersion = 1;
    pProps->vendorId = HAG_VENDOR_AMD;
    pProps->deviceType = HAG_DEVICE_TYPE_DISCRETE_GPU;
    
    /* Device name based on family */
    switch (data->family) {
        case AMDGPU_FAMILY_AI:
            strncpy(pProps->deviceName, "AMD Vega", sizeof(pProps->deviceName) - 1);
            break;
        case AMDGPU_FAMILY_NV:
            strncpy(pProps->deviceName, "AMD Navi/RDNA", sizeof(pProps->deviceName) - 1);
            break;
        case AMDGPU_FAMILY_GC_11_0_0:
        case AMDGPU_FAMILY_GC_11_0_1:
            strncpy(pProps->deviceName, "AMD RDNA3", sizeof(pProps->deviceName) - 1);
            break;
        default:
            snprintf(pProps->deviceName, sizeof(pProps->deviceName), "AMD GPU (family %u)", data->family);
            break;
    }
    
    pProps->deviceLocalMemoryBytes = data->vramSize;
    pProps->sharedMemoryBytes = data->gttSize;
    pProps->computeUnits = data->computeUnits;
    pProps->maxWorkGroupSize[0] = 1024;
    pProps->maxWorkGroupSize[1] = 1024;
    pProps->maxWorkGroupSize[2] = 1024;
    pProps->maxTensorDimensions = 8;
    pProps->warpSize = data->waveFrontSize;
    pProps->clockFrequencyMHz = data->clockFrequency;
    pProps->supportsTensorCores = data->hasMatrixCores;
    pProps->supportsRayTracing = data->hasRayTracing;
    pProps->supportsFp16Atomic = 1;
    pProps->supportsBf16 = data->family >= AMDGPU_FAMILY_NV;
    pProps->supportsFp8 = data->family >= AMDGPU_FAMILY_GC_11_0_0;
    pProps->supportsInt8DotAcc = 1;
    
    return HAG_SUCCESS;
}

/* ============================================================================
 * Memory Operations (Stubs - full implementation requires GEM/BO management)
 * ============================================================================ */

static HagResult amdgpuDriverAllocMemory(void* pDriverData, uint64_t size, uint32_t memoryType, HagDriverMemory* pMemory) {
    (void)pDriverData;
    (void)size;
    (void)memoryType;
    (void)pMemory;
    return HAG_ERROR_NOT_SUPPORTED;
}

static void amdgpuDriverFreeMemory(void* pDriverData, HagDriverMemory memory) {
    (void)pDriverData;
    (void)memory;
}

static HagResult amdgpuDriverMapMemory(void* pDriverData, HagDriverMemory memory, void** ppData) {
    (void)pDriverData;
    (void)memory;
    (void)ppData;
    return HAG_ERROR_NOT_SUPPORTED;
}

static void amdgpuDriverUnmapMemory(void* pDriverData, HagDriverMemory memory) {
    (void)pDriverData;
    (void)memory;
}

static HagResult amdgpuDriverFlushMemory(void* pDriverData, HagDriverMemory memory, uint64_t offset, uint64_t size) {
    (void)pDriverData;
    (void)memory;
    (void)offset;
    (void)size;
    return HAG_SUCCESS;
}

static HagResult amdgpuDriverInvalidateMemory(void* pDriverData, HagDriverMemory memory, uint64_t offset, uint64_t size) {
    (void)pDriverData;
    (void)memory;
    (void)offset;
    (void)size;
    return HAG_SUCCESS;
}

/* ============================================================================
 * Buffer Operations (Stubs)
 * ============================================================================ */

static HagResult amdgpuDriverCreateBuffer(void* pDriverData, uint64_t size, uint32_t usage, HagDriverBuffer* pBuffer) {
    (void)pDriverData;
    (void)size;
    (void)usage;
    (void)pBuffer;
    return HAG_ERROR_NOT_SUPPORTED;
}

static void amdgpuDriverDestroyBuffer(void* pDriverData, HagDriverBuffer buffer) {
    (void)pDriverData;
    (void)buffer;
}

static HagResult amdgpuDriverBindBufferMemory(void* pDriverData, HagDriverBuffer buffer, HagDriverMemory memory, uint64_t offset) {
    (void)pDriverData;
    (void)buffer;
    (void)memory;
    (void)offset;
    return HAG_ERROR_NOT_SUPPORTED;
}

static HagResult amdgpuDriverGetBufferMemoryRequirements(void* pDriverData, HagDriverBuffer buffer, uint64_t* pSize, uint32_t* pAlignment) {
    (void)pDriverData;
    (void)buffer;
    (void)pSize;
    (void)pAlignment;
    return HAG_ERROR_NOT_SUPPORTED;
}

static HagResult amdgpuDriverMapBuffer(void* pDriverData, HagDriverBuffer buffer, void** ppData) {
    (void)pDriverData;
    (void)buffer;
    (void)ppData;
    return HAG_ERROR_NOT_SUPPORTED;
}

static void amdgpuDriverUnmapBuffer(void* pDriverData, HagDriverBuffer buffer) {
    (void)pDriverData;
    (void)buffer;
}

/* ============================================================================
 * Queue & Execution (Stubs)
 * ============================================================================ */

static HagResult amdgpuDriverCreateQueue(void* pDriverData, HagQueueType type, uint32_t* pQueueFamilyIndex, HagDriverQueue* pQueue) {
    (void)pDriverData;
    (void)type;
    (void)pQueueFamilyIndex;
    (void)pQueue;
    return HAG_ERROR_NOT_SUPPORTED;
}

static void amdgpuDriverDestroyQueue(void* pDriverData, HagDriverQueue queue) {
    (void)pDriverData;
    (void)queue;
}

static HagResult amdgpuDriverSubmit(void* pDriverData, HagDriverQueue queue, uint32_t cmdBufferCount, void** ppCmdBuffers, HagDriverFence fence) {
    (void)pDriverData;
    (void)queue;
    (void)cmdBufferCount;
    (void)ppCmdBuffers;
    (void)fence;
    return HAG_ERROR_NOT_SUPPORTED;
}

static HagResult amdgpuDriverWaitQueueIdle(void* pDriverData, HagDriverQueue queue) {
    (void)pDriverData;
    (void)queue;
    return HAG_SUCCESS;
}

/* ============================================================================
 * Command Buffer (Stubs)
 * ============================================================================ */

static HagResult amdgpuDriverCreateCmdBuffer(void* pDriverData, HagDriverQueue queue, void** ppCmdBuffer) {
    (void)pDriverData;
    (void)queue;
    (void)ppCmdBuffer;
    return HAG_ERROR_NOT_SUPPORTED;
}

static void amdgpuDriverDestroyCmdBuffer(void* pDriverData, void* pCmdBuffer) {
    (void)pDriverData;
    (void)pCmdBuffer;
}

static HagResult amdgpuDriverBeginCmdBuffer(void* pDriverData, void* pCmdBuffer, uint32_t flags) {
    (void)pDriverData;
    (void)pCmdBuffer;
    (void)flags;
    return HAG_ERROR_NOT_SUPPORTED;
}

static HagResult amdgpuDriverEndCmdBuffer(void* pDriverData, void* pCmdBuffer) {
    (void)pDriverData;
    (void)pCmdBuffer;
    return HAG_ERROR_NOT_SUPPORTED;
}

static HagResult amdgpuDriverResetCmdBuffer(void* pDriverData, void* pCmdBuffer, uint32_t flags) {
    (void)pDriverData;
    (void)pCmdBuffer;
    (void)flags;
    return HAG_ERROR_NOT_SUPPORTED;
}

/* ============================================================================
 * Commands (Stubs)
 * ============================================================================ */

static HagResult amdgpuDriverCmdBindKernel(void* pDriverData, void* pCmdBuffer, HagDriverKernel kernel) {
    (void)pDriverData;
    (void)pCmdBuffer;
    (void)kernel;
    return HAG_ERROR_NOT_SUPPORTED;
}

static HagResult amdgpuDriverCmdBindBuffer(void* pDriverData, void* pCmdBuffer, uint32_t binding, HagDriverBuffer buffer, uint64_t offset) {
    (void)pDriverData;
    (void)pCmdBuffer;
    (void)binding;
    (void)buffer;
    (void)offset;
    return HAG_ERROR_NOT_SUPPORTED;
}

static HagResult amdgpuDriverCmdDispatch(void* pDriverData, void* pCmdBuffer, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) {
    (void)pDriverData;
    (void)pCmdBuffer;
    (void)groupsX;
    (void)groupsY;
    (void)groupsZ;
    return HAG_ERROR_NOT_SUPPORTED;
}

static HagResult amdgpuDriverCmdCopyBuffer(void* pDriverData, void* pCmdBuffer, HagDriverBuffer src, HagDriverBuffer dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size) {
    (void)pDriverData;
    (void)pCmdBuffer;
    (void)src;
    (void)dst;
    (void)srcOffset;
    (void)dstOffset;
    (void)size;
    return HAG_ERROR_NOT_SUPPORTED;
}

static HagResult amdgpuDriverCmdFillBuffer(void* pDriverData, void* pCmdBuffer, HagDriverBuffer buffer, uint64_t offset, uint64_t size, uint32_t data) {
    (void)pDriverData;
    (void)pCmdBuffer;
    (void)buffer;
    (void)offset;
    (void)size;
    (void)data;
    return HAG_ERROR_NOT_SUPPORTED;
}

static HagResult amdgpuDriverCmdBarrier(void* pDriverData, void* pCmdBuffer, uint32_t srcStage, uint32_t dstStage) {
    (void)pDriverData;
    (void)pCmdBuffer;
    (void)srcStage;
    (void)dstStage;
    return HAG_ERROR_NOT_SUPPORTED;
}

/* ============================================================================
 * Kernels (Stubs)
 * ============================================================================ */

static HagResult amdgpuDriverCreateKernel(void* pDriverData, const void* pCode, size_t codeSize, HagKernelLanguage lang, HagDriverKernel* pKernel) {
    (void)pDriverData;
    (void)pCode;
    (void)codeSize;
    (void)lang;
    (void)pKernel;
    return HAG_ERROR_NOT_SUPPORTED;
}

static void amdgpuDriverDestroyKernel(void* pDriverData, HagDriverKernel kernel) {
    (void)pDriverData;
    (void)kernel;
}

static HagResult amdgpuDriverGetKernelWorkGroupSize(void* pDriverData, HagDriverKernel kernel, uint32_t* pSize) {
    (void)pDriverData;
    (void)kernel;
    (void)pSize;
    return HAG_ERROR_NOT_SUPPORTED;
}

/* ============================================================================
 * Synchronization (Stubs)
 * ============================================================================ */

static HagResult amdgpuDriverCreateFence(void* pDriverData, HagDriverFence* pFence) {
    (void)pDriverData;
    (void)pFence;
    return HAG_ERROR_NOT_SUPPORTED;
}

static void amdgpuDriverDestroyFence(void* pDriverData, HagDriverFence fence) {
    (void)pDriverData;
    (void)fence;
}

static HagResult amdgpuDriverResetFence(void* pDriverData, HagDriverFence fence) {
    (void)pDriverData;
    (void)fence;
    return HAG_ERROR_NOT_SUPPORTED;
}

static HagResult amdgpuDriverWaitFences(void* pDriverData, uint32_t count, HagDriverFence* pFences, uint64_t timeout) {
    (void)pDriverData;
    (void)count;
    (void)pFences;
    (void)timeout;
    return HAG_ERROR_NOT_SUPPORTED;
}

static HagResult amdgpuDriverGetTimestampFrequency(void* pDriverData, uint64_t* pFrequency) {
    (void)pDriverData;
    if (!pFrequency) return HAG_ERROR_INVALID_ARGUMENT;
    *pFrequency = 1000000000ULL;
    return HAG_SUCCESS;
}

/* ============================================================================
 * Driver Ops Table
 * ============================================================================ */

const HagDriverOps g_hagAmdgpuDriverOps = {
    .probe = amdgpuDriverProbe,
    .create = amdgpuDriverCreate,
    .destroy = amdgpuDriverDestroy,
    .getDeviceProperties = amdgpuDriverGetDeviceProperties,
    .allocMemory = amdgpuDriverAllocMemory,
    .freeMemory = amdgpuDriverFreeMemory,
    .mapMemory = amdgpuDriverMapMemory,
    .unmapMemory = amdgpuDriverUnmapMemory,
    .flushMemory = amdgpuDriverFlushMemory,
    .invalidateMemory = amdgpuDriverInvalidateMemory,
    .createBuffer = amdgpuDriverCreateBuffer,
    .destroyBuffer = amdgpuDriverDestroyBuffer,
    .bindBufferMemory = amdgpuDriverBindBufferMemory,
    .getBufferMemoryRequirements = amdgpuDriverGetBufferMemoryRequirements,
    .mapBuffer = amdgpuDriverMapBuffer,
    .unmapBuffer = amdgpuDriverUnmapBuffer,
    .createQueue = amdgpuDriverCreateQueue,
    .destroyQueue = amdgpuDriverDestroyQueue,
    .submit = amdgpuDriverSubmit,
    .waitQueueIdle = amdgpuDriverWaitQueueIdle,
    .createCmdBuffer = amdgpuDriverCreateCmdBuffer,
    .destroyCmdBuffer = amdgpuDriverDestroyCmdBuffer,
    .beginCmdBuffer = amdgpuDriverBeginCmdBuffer,
    .endCmdBuffer = amdgpuDriverEndCmdBuffer,
    .resetCmdBuffer = amdgpuDriverResetCmdBuffer,
    .cmdBindKernel = amdgpuDriverCmdBindKernel,
    .cmdBindBuffer = amdgpuDriverCmdBindBuffer,
    .cmdDispatch = amdgpuDriverCmdDispatch,
    .cmdCopyBuffer = amdgpuDriverCmdCopyBuffer,
    .cmdFillBuffer = amdgpuDriverCmdFillBuffer,
    .cmdBarrier = amdgpuDriverCmdBarrier,
    .createKernel = amdgpuDriverCreateKernel,
    .destroyKernel = amdgpuDriverDestroyKernel,
    .getKernelWorkGroupSize = amdgpuDriverGetKernelWorkGroupSize,
    .createFence = amdgpuDriverCreateFence,
    .destroyFence = amdgpuDriverDestroyFence,
    .resetFence = amdgpuDriverResetFence,
    .waitFences = amdgpuDriverWaitFences,
    .getTimestampFrequency = amdgpuDriverGetTimestampFrequency,
};

#else /* !HAG_HAS_LIBDRM */

/* Stub when libdrm is not available */
const HagDriverOps g_hagAmdgpuDriverOps = {0};

#endif /* HAG_HAS_LIBDRM */
