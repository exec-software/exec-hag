/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#ifndef HAG_UTILS_PLATFORM_H
#define HAG_UTILS_PLATFORM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Platform abstraction for Linux (primary) and future platforms */

/* Threads */
typedef struct HagThread_T* HagThread;
typedef struct HagMutex_T* HagMutex;

typedef void (*HagThreadFunc)(void* arg);

HagThread hagPlatformThreadCreate(HagThreadFunc func, void* arg);
void hagPlatformThreadJoin(HagThread thread);
void hagPlatformThreadYield(void);

HagMutex hagPlatformMutexCreate(void);
void hagPlatformMutexLock(HagMutex mutex);
void hagPlatformMutexUnlock(HagMutex mutex);
void hagPlatformMutexDestroy(HagMutex mutex);

/* Time */
uint64_t hagPlatformTimeNanos(void);
void hagPlatformSleepNanos(uint64_t ns);

/* Filesystem */
int hagPlatformFileExists(const char* path);
int64_t hagPlatformFileSize(const char* path);
int hagPlatformFileRead(const char* path, void* buffer, size_t size);
int hagPlatformFileWrite(const char* path, const void* buffer, size_t size);
int hagPlatformCreateDirectory(const char* path);

/* Memory */
void* hagPlatformAlloc(size_t size);
void* hagPlatformAllocAligned(size_t size, size_t alignment);
void hagPlatformFree(void* ptr);

/* Dynamic loading */
typedef struct HagModule_T* HagModule;
HagModule hagPlatformModuleOpen(const char* path);
void hagPlatformModuleClose(HagModule module);
void* hagPlatformModuleSymbol(HagModule module, const char* name);

/* DRM device enumeration */
typedef struct HagDrmDeviceInfo {
    char path[64];
    uint32_t vendorId;
    uint32_t deviceId;
    uint32_t subsystemVendorId;
    uint32_t subsystemDeviceId;
} HagDrmDeviceInfo;

int hagPlatformEnumerateDrmDevices(HagDrmDeviceInfo* pDevices, int maxDevices);

#ifdef __cplusplus
}
#endif

#endif /* HAG_UTILS_PLATFORM_H */
