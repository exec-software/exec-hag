/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#include "unity.h"
#include "hag/memory/allocator.h"
#include "hag/utils/platform.h"

void test_utils_alloc_platform_basic(void) {
    void* ptr = hagPlatformAlloc(128);
    TEST_ASSERT_NOT_NULL(ptr);
    hagPlatformFree(ptr);
}

void test_utils_alloc_aligned(void) {
    void* ptr = hagPlatformAllocAligned(256, 64);
    TEST_ASSERT_NOT_NULL(ptr);
    TEST_ASSERT_EQUAL(0, ((uintptr_t)ptr) % 64);
    hagPlatformFree(ptr);
}

void test_utils_alloc_zero_size(void) {
    void* ptr = hagPlatformAlloc(0);
    /* Implementation-defined: may return NULL or valid pointer */
    hagPlatformFree(ptr);
}
