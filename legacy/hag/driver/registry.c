/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#include "hag/driver/driver_interface.h"
#include "hag/core/instance.h"
#include "hag/utils/platform.h"
#include <string.h>

#define HAG_MAX_REGISTERED_DRIVERS 16

static struct {
    HagDriverRegistration*  entries[HAG_MAX_REGISTERED_DRIVERS];
    uint32_t                count;
    HagMutex                mutex;
    int                     initialized;
} g_driverRegistry = {0};

void hagDriverRegister(const HagDriverRegistration* registration) {
    if (!registration || !registration->ops) return;
    
    if (!g_driverRegistry.initialized) {
        g_driverRegistry.mutex = hagPlatformMutexCreate();
        g_driverRegistry.initialized = 1;
    }
    
    hagPlatformMutexLock(g_driverRegistry.mutex);
    
    if (g_driverRegistry.count < HAG_MAX_REGISTERED_DRIVERS) {
        /* Store pointer - registration must persist! */
        g_driverRegistry.entries[g_driverRegistry.count++] = (HagDriverRegistration*)registration;
    }
    
    hagPlatformMutexUnlock(g_driverRegistry.mutex);
}

const HagDriverRegistration* hagDriverFind(int drmFd, uint32_t vendorId, uint32_t deviceId) {
    if (!g_driverRegistry.initialized) return NULL;
    
    hagPlatformMutexLock(g_driverRegistry.mutex);
    
    const HagDriverRegistration* best = NULL;
    int bestScore = HAG_DRIVER_SCORE_NONE;
    
    for (uint32_t i = 0; i < g_driverRegistry.count; i++) {
        HagDriverRegistration* reg = g_driverRegistry.entries[i];
        if (!reg || !reg->ops || !reg->ops->probe) continue;
        
        int score = HAG_DRIVER_SCORE_NONE;
        HagResult r = reg->ops->probe(drmFd, vendorId, deviceId, &score);
        if (r == HAG_SUCCESS && score > bestScore) {
            bestScore = score;
            best = reg;
        }
    }
    
    hagPlatformMutexUnlock(g_driverRegistry.mutex);
    return best;
}

/* Get all registered drivers for enumeration */
uint32_t hagDriverGetAll(const HagDriverRegistration*** ppRegistrations) {
    if (!g_driverRegistry.initialized || !ppRegistrations) return 0;
    *ppRegistrations = (const HagDriverRegistration**)g_driverRegistry.entries;
    return g_driverRegistry.count;
}
