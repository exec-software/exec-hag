/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "unity.h"
#include "hag/api/api.h"
#include "hag/api/profiler.h"
#include "hag/utils/platform.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void test_profiler_create_destroy(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagProfiler profiler;
    result = hagProfilerCreate(device, &profiler);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(profiler);
    
    TEST_ASSERT_EQUAL(0, hagProfilerGetEntryCount(profiler));
    
    hagProfilerDestroy(profiler);
    hagDestroyDevice(device);
    hagDestroyInstance(instance);
}

void test_profiler_basic_timing(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagProfiler profiler;
    result = hagProfilerCreate(device, &profiler);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Record a scope */
    hagProfilerBegin(profiler, "test_scope", HAG_PROFILER_SCOPE_HOST);
    hagPlatformSleepNanos(1000000); /* 1ms */
    hagProfilerEnd(profiler);
    
    TEST_ASSERT_EQUAL(1, hagProfilerGetEntryCount(profiler));
    
    const char* name;
    uint64_t start, end;
    HagProfilerScopeType type;
    result = hagProfilerGetEntry(profiler, 0, &name, &start, &end, &type);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    TEST_ASSERT_EQUAL_STRING("test_scope", name);
    TEST_ASSERT_EQUAL(HAG_PROFILER_SCOPE_HOST, type);
    TEST_ASSERT_EQUAL(1, end > start);
    
    hagProfilerDestroy(profiler);
    hagDestroyDevice(device);
    hagDestroyInstance(instance);
}

void test_profiler_multiple_entries(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagProfiler profiler;
    result = hagProfilerCreate(device, &profiler);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "scope_%d", i);
        hagProfilerBegin(profiler, name, HAG_PROFILER_SCOPE_DEVICE);
        hagPlatformSleepNanos(100000); /* 0.1ms */
        hagProfilerEnd(profiler);
    }
    
    TEST_ASSERT_EQUAL(5, hagProfilerGetEntryCount(profiler));
    
    for (int i = 0; i < 5; i++) {
        const char* name;
        result = hagProfilerGetEntry(profiler, i, &name, NULL, NULL, NULL);
        TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
        char expected[32];
        snprintf(expected, sizeof(expected), "scope_%d", i);
        TEST_ASSERT_EQUAL_STRING(expected, name);
    }
    
    hagProfilerDestroy(profiler);
    hagDestroyDevice(device);
    hagDestroyInstance(instance);
}

void test_profiler_export(void) {
    HagInstance instance;
    HagDevice device;
    HagProfiler profiler;
    HagInstanceCreateInfo instInfo = {0};
    HagDeviceCreateInfo devInfo = {0};
    char directoryTemplate[] = "/tmp/hag-profiler-XXXXXX";
    char tracePath[sizeof(directoryTemplate) + sizeof("/trace.json")] = {0};
    char buf[256] = {0};
    char* directory = NULL;
    FILE* fp = NULL;
    int ioOk = 0;
    int cleanupOk = 1;
    int pathLength;

    /* Archived test source: the profiler implementation is absent, so this
     * path hardening does not provide standalone executable coverage. */
    HagResult result = hagCreateInstance(&instInfo, &instance);
    if (result != HAG_SUCCESS) goto assertions;

    result = hagCreateDevice(instance, &devInfo, &device);
    if (result != HAG_SUCCESS) goto cleanup_instance;

    result = hagProfilerCreate(device, &profiler);
    if (result != HAG_SUCCESS) goto cleanup_device;

    hagProfilerBegin(profiler, "export_test", HAG_PROFILER_SCOPE_COPY);
    hagPlatformSleepNanos(500000); /* 0.5ms */
    hagProfilerEnd(profiler);

    /* The filename-only exporter may reopen its path. A private 0700 directory
     * prevents other users from replacing the target with a symlink. */
    directory = mkdtemp(directoryTemplate);
    if (!directory) goto cleanup_trace;
    pathLength = snprintf(tracePath, sizeof(tracePath), "%s/trace.json", directory);
    if (pathLength < 0 || (size_t)pathLength >= sizeof(tracePath)) {
        tracePath[0] = '\0';
        goto cleanup_trace;
    }
    result = hagProfilerExportChromeTrace(profiler, tracePath);
    if (result != HAG_SUCCESS) goto cleanup_trace;

    fp = fopen(tracePath, "r");
    if (!fp) goto cleanup_trace;
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    buf[n] = '\0';
    ioOk = !ferror(fp);

cleanup_trace:
    /* Unity assertions can longjmp, so release files and sessions first. */
    if (fp && fclose(fp) != 0) cleanupOk = 0;
    if (directory) {
        if (tracePath[0] != '\0' && unlink(tracePath) != 0 && errno != ENOENT)
            cleanupOk = 0;
        if (rmdir(directory) != 0) cleanupOk = 0;
    }
    hagProfilerDestroy(profiler);
cleanup_device:
    hagDestroyDevice(device);
cleanup_instance:
    hagDestroyInstance(instance);
assertions:
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    TEST_ASSERT_EQUAL(1, ioOk);
    TEST_ASSERT_EQUAL(1, cleanupOk);
    TEST_ASSERT_NOT_NULL(strstr(buf, "export_test"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "copy"));
}
