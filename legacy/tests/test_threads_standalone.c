/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

/* Standalone Linux host regression for the thread wrapper only. No DRM device
 * enumeration or GPU function is called. This does not build or qualify the
 * archived runtime. Link platform_linux.c with -pthread -ldl and libdrm's
 * pkg-config flags when its optional headers are installed, plus:
 *   -Wl,--wrap=malloc,--wrap=free,--wrap=pthread_create,--wrap=pthread_join
 * Linker wrappers inject failures without resource exhaustion.
 */

#include "hag/utils/platform.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

void* __real_malloc(size_t size);
void __real_free(void* ptr);
int __real_pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                          void* (*start)(void*), void* arg);
int __real_pthread_join(pthread_t thread, void** result);

void* __wrap_malloc(size_t size);
void __wrap_free(void* ptr);
int __wrap_pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                          void* (*start)(void*), void* arg);
int __wrap_pthread_join(pthread_t thread, void** result);

/* Parent allocation precedes pthread_create; child cleanup precedes successful
 * pthread_join. No counter is accessed by both threads without that ordering.
 * The forced join-error case does not inspect counters until the real join. */
static size_t allocationAttempts;
static size_t liveAllocations;
static size_t failAllocationAt;
static unsigned createAttempts;
static int failCreate;
static int failJoin;

void* __wrap_malloc(size_t size) {
    void* ptr;
    allocationAttempts++;
    if (failAllocationAt == allocationAttempts) return NULL;
    ptr = __real_malloc(size);
    if (ptr) liveAllocations++;
    return ptr;
}

void __wrap_free(void* ptr) {
    if (ptr) {
        CHECK(liveAllocations != 0);
        liveAllocations--;
    }
    __real_free(ptr);
}

int __wrap_pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                          void* (*start)(void*), void* arg) {
    createAttempts++;
    if (failCreate) return EAGAIN;
    return __real_pthread_create(thread, attr, start, arg);
}

int __wrap_pthread_join(pthread_t thread, void** result) {
    if (failJoin) return EDEADLK;
    return __real_pthread_join(thread, result);
}

static void resetFailures(void) {
    CHECK(liveAllocations == 0);
    allocationAttempts = 0;
    failAllocationAt = 0;
    createAttempts = 0;
    failCreate = 0;
    failJoin = 0;
}

static void callback(void* arg) {
    int* value = (int*)arg;
    *value += 1;
}

static void exitingCallback(void* arg) {
    callback(arg);
    pthread_exit(NULL);
}

int main(void) {
    int value = 0;
    HagThread thread;
    size_t failure;

    resetFailures();
    CHECK(hagPlatformThreadCreate(NULL, &value) == NULL);
    CHECK(allocationAttempts == 0 && createAttempts == 0);
    hagPlatformThreadJoin(NULL);

    for (failure = 1; failure <= 2; ++failure) {
        resetFailures();
        failAllocationAt = failure;
        CHECK(hagPlatformThreadCreate(callback, &value) == NULL);
        CHECK(liveAllocations == 0 && createAttempts == 0 && value == 0);
    }

    resetFailures();
    failCreate = 1;
    CHECK(hagPlatformThreadCreate(callback, &value) == NULL);
    CHECK(liveAllocations == 0 && createAttempts == 1 && value == 0);

    resetFailures();
    thread = hagPlatformThreadCreate(callback, &value);
    CHECK(thread != NULL);
    /* An unsuccessful join must leave the owner able to retry retirement. */
    failJoin = 1;
    hagPlatformThreadJoin(thread);
    failJoin = 0;
    hagPlatformThreadJoin(thread);
    CHECK(value == 1 && liveAllocations == 0 && allocationAttempts == 2);

    resetFailures();
    thread = hagPlatformThreadCreate(exitingCallback, &value);
    CHECK(thread != NULL);
    hagPlatformThreadJoin(thread);
    CHECK(value == 2 && liveAllocations == 0 && allocationAttempts == 2);
    puts("standalone legacy thread regression passed");
    return EXIT_SUCCESS;
}
