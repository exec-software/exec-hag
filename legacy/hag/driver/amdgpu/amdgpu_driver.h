/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#ifndef HAG_DRIVER_AMDGPU_AMDGPU_DRIVER_H
#define HAG_DRIVER_AMDGPU_AMDGPU_DRIVER_H

#include "hag/driver/driver_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/* AMDGPU driver operations - available when libdrm is present */
#if defined(HAG_HAS_LIBDRM)

extern const HagDriverOps g_hagAmdgpuDriverOps;

/* AMDGPU chip families for feature detection */
typedef enum AmdGpuFamily {
    AMDGPU_FAMILY_SI = 110,     /* Southern Islands */
    AMDGPU_FAMILY_CI = 120,     /* Sea Islands */
    AMDGPU_FAMILY_KV = 125,     /* Kaveri */
    AMDGPU_FAMILY_VI = 130,     /* Volcanic Islands */
    AMDGPU_FAMILY_CZ = 135,     /* Carrizo */
    AMDGPU_FAMILY_AI = 141,     /* Arctic Islands (Vega) */
    AMDGPU_FAMILY_RV = 142,     /* Raven */
    AMDGPU_FAMILY_NV = 143,     /* Navi */
    AMDGPU_FAMILY_VGH = 144,    /* Van Gogh */
    AMDGPU_FAMILY_GC_10_3_6 = 145,
    AMDGPU_FAMILY_GC_10_3_7 = 146,
    AMDGPU_FAMILY_GC_11_0_0 = 148, /* RDNA3 */
    AMDGPU_FAMILY_GC_11_0_1 = 149,
    AMDGPU_FAMILY_GC_11_5_0 = 150,
} AmdGpuFamily;

/* Internal AMDGPU driver data structure */
typedef struct AmdGpuDriverData {
    int                     drmFd;
    uint32_t                family;
    uint32_t                chipClass;
    uint32_t                gfxVersion;
    uint32_t                meFirmwareVersion;
    uint32_t                pfpFirmwareVersion;
    uint32_t                ceFirmwareVersion;
    uint64_t                vramSize;
    uint64_t                gttSize;
    uint32_t                computeUnits;
    uint32_t                waveFrontSize;
    uint32_t                maxWorkGroupSize;
    uint32_t                clockFrequency;
    int                     hasWave32;
    int                     hasRayTracing;
    int                     hasMatrixCores;
} AmdGpuDriverData;

#endif /* HAG_HAS_LIBDRM */

#ifdef __cplusplus
}
#endif

#endif /* HAG_DRIVER_AMDGPU_AMDGPU_DRIVER_H */
