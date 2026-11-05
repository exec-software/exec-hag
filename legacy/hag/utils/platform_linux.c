/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#define _GNU_SOURCE

#include "hag/utils/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Linux-specific headers */
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <pthread.h>
#include <time.h>
#include <dlfcn.h>
#include <stdint.h>

/* DRM headers - optional, we fallback to sysfs if unavailable */
#if __has_include(<xf86drm.h>)
  #include <xf86drm.h>
  #define HAG_HAS_LIBDRM 1
#else
  #define HAG_HAS_LIBDRM 0
#endif

/* Threads */
typedef struct HagThread_T {
    pthread_t handle;
} HagThread_T;

typedef struct HagThreadStart {
    HagThreadFunc func;
    void* arg;
} HagThreadStart;

typedef struct HagMutex_T {
    pthread_mutex_t handle;
} HagMutex_T;

static void* hagThreadMain(void* opaque) {
    HagThreadStart* start = (HagThreadStart*)opaque;
    HagThreadFunc func = start->func;
    void* arg = start->arg;
    /* Successful pthread_create transfers this context to the new thread.
     * Free it before the callback, which may itself call pthread_exit. */
    hagPlatformFree(start);
    func(arg);
    return NULL;
}

HagThread hagPlatformThreadCreate(HagThreadFunc func, void* arg) {
    if (!func) return NULL;
    HagThread_T* thread = (HagThread_T*)hagPlatformAlloc(sizeof(HagThread_T));
    if (!thread) return NULL;

    HagThreadStart* start = (HagThreadStart*)hagPlatformAlloc(sizeof(HagThreadStart));
    if (!start) {
        hagPlatformFree(thread);
        return NULL;
    }
    start->func = func;
    start->arg = arg;
    if (pthread_create(&thread->handle, NULL, hagThreadMain, start) != 0) {
        /* A failed creation has no child to consume the startup context. */
        hagPlatformFree(start);
        hagPlatformFree(thread);
        return NULL;
    }
    return thread;
}

void hagPlatformThreadJoin(HagThread thread) {
    if (!thread) return;
    HagThread_T* t = (HagThread_T*)thread;
    /* Only a successful join establishes retirement. A failed/self join must
     * not destroy the handle needed by its owning thread to join later. */
    if (pthread_join(t->handle, NULL) == 0) hagPlatformFree(t);
}

void hagPlatformThreadYield(void) {
    sched_yield();
}

HagMutex hagPlatformMutexCreate(void) {
    HagMutex_T* mutex = (HagMutex_T*)hagPlatformAlloc(sizeof(HagMutex_T));
    if (!mutex) return NULL;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    if (pthread_mutex_init(&mutex->handle, &attr) != 0) {
        hagPlatformFree(mutex);
        return NULL;
    }
    pthread_mutexattr_destroy(&attr);
    return mutex;
}

void hagPlatformMutexLock(HagMutex mutex) {
    if (mutex) pthread_mutex_lock(&((HagMutex_T*)mutex)->handle);
}

void hagPlatformMutexUnlock(HagMutex mutex) {
    if (mutex) pthread_mutex_unlock(&((HagMutex_T*)mutex)->handle);
}

void hagPlatformMutexDestroy(HagMutex mutex) {
    if (!mutex) return;
    HagMutex_T* m = (HagMutex_T*)mutex;
    pthread_mutex_destroy(&m->handle);
    hagPlatformFree(m);
}

/* Time */
uint64_t hagPlatformTimeNanos(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

void hagPlatformSleepNanos(uint64_t ns) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ns / 1000000000ULL);
    ts.tv_nsec = (long)(ns % 1000000000ULL);
    nanosleep(&ts, NULL);
}

/* Filesystem */
int hagPlatformFileExists(const char* path) {
    return access(path, F_OK) == 0;
}

int64_t hagPlatformFileSize(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (int64_t)st.st_size;
}

int hagPlatformFileRead(const char* path, void* buffer, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    ssize_t total = 0;
    while ((size_t)total < size) {
        ssize_t n = read(fd, (char*)buffer + total, size - (size_t)total);
        if (n < 0) {
            close(fd);
            return -1;
        }
        if (n == 0) break;
        total += n;
    }
    close(fd);
    return (int)total;
}

int hagPlatformFileWrite(const char* path, const void* buffer, size_t size) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;

    ssize_t total = 0;
    while ((size_t)total < size) {
        ssize_t n = write(fd, (const char*)buffer + total, size - (size_t)total);
        if (n < 0) {
            close(fd);
            return -1;
        }
        total += n;
    }
    close(fd);
    return (int)total;
}

int hagPlatformCreateDirectory(const char* path) {
    return mkdir(path, 0755);
}

/* Memory */
void* hagPlatformAlloc(size_t size) {
    return malloc(size);
}

void* hagPlatformAllocAligned(size_t size, size_t alignment) {
    void* ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) return NULL;
    return ptr;
}

void hagPlatformFree(void* ptr) {
    free(ptr);
}

/* Dynamic loading */
typedef struct HagModule_T {
    void* handle;
} HagModule_T;

HagModule hagPlatformModuleOpen(const char* path) {
    void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) return NULL;
    HagModule_T* mod = (HagModule_T*)hagPlatformAlloc(sizeof(HagModule_T));
    if (!mod) {
        dlclose(handle);
        return NULL;
    }
    mod->handle = handle;
    return mod;
}

void hagPlatformModuleClose(HagModule module) {
    if (!module) return;
    HagModule_T* mod = (HagModule_T*)module;
    dlclose(mod->handle);
    hagPlatformFree(mod);
}

void* hagPlatformModuleSymbol(HagModule module, const char* name) {
    if (!module) return NULL;
    return dlsym(((HagModule_T*)module)->handle, name);
}

/* DRM device enumeration */
int hagPlatformEnumerateDrmDevices(HagDrmDeviceInfo* pDevices, int maxDevices) {
    DIR* dir = opendir("/dev/dri");
    if (!dir) return 0;

    int count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && count < maxDevices) {
        if (strncmp(entry->d_name, "renderD", 7) != 0) continue;

        /* A path must fit both the open call and the exported device record. */
        char path[sizeof(pDevices[0].path)];
        int pathLength = snprintf(path, sizeof(path), "/dev/dri/%s", entry->d_name);
        if (pathLength < 0 || (size_t)pathLength >= sizeof(path)) continue;

        int fd = open(path, O_RDWR | O_CLOEXEC);
        if (fd < 0) continue;

        /* Query sysfs for PCI info - works without libdrm */
        HagDrmDeviceInfo* info = &pDevices[count++];
        memcpy(info->path, path, (size_t)pathLength + 1);
        info->vendorId = 0;
        info->deviceId = 0;
        info->subsystemVendorId = 0;
        info->subsystemDeviceId = 0;

        int minor = atoi(entry->d_name + 7);
        char sysfsPath[256];
        snprintf(sysfsPath, sizeof(sysfsPath),
                 "/sys/class/drm/renderD%d/device/vendor", minor);

        FILE* fp = fopen(sysfsPath, "r");
        if (fp) {
            unsigned int vendor = 0;
            if (fscanf(fp, "%x", &vendor) == 1) {
                info->vendorId = vendor;
            }
            fclose(fp);
        }

        snprintf(sysfsPath, sizeof(sysfsPath),
                 "/sys/class/drm/renderD%d/device/device", minor);
        fp = fopen(sysfsPath, "r");
        if (fp) {
            unsigned int device = 0;
            if (fscanf(fp, "%x", &device) == 1) {
                info->deviceId = device;
            }
            fclose(fp);
        }

        snprintf(sysfsPath, sizeof(sysfsPath),
                 "/sys/class/drm/renderD%d/device/subsystem_vendor", minor);
        fp = fopen(sysfsPath, "r");
        if (fp) {
            unsigned int sv = 0;
            if (fscanf(fp, "%x", &sv) == 1) {
                info->subsystemVendorId = sv;
            }
            fclose(fp);
        }

        snprintf(sysfsPath, sizeof(sysfsPath),
                 "/sys/class/drm/renderD%d/device/subsystem_device", minor);
        fp = fopen(sysfsPath, "r");
        if (fp) {
            unsigned int sd = 0;
            if (fscanf(fp, "%x", &sd) == 1) {
                info->subsystemDeviceId = sd;
            }
            fclose(fp);
        }

#if HAG_HAS_LIBDRM
        drmVersionPtr ver = drmGetVersion(fd);
        if (ver) {
            drmFreeVersion(ver);
        }
#endif
        close(fd);
    }
    closedir(dir);
    return count;
}
