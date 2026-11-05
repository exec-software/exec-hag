/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#include "unity.h"
#include "hag/api/api.h"
#include "hag/memory/allocator.h"

/* ============================================================================
 * API Basics Tests
 * ============================================================================ */

void test_api_version(void) {
    uint32_t version = hagGetApiVersion();
    TEST_ASSERT_NOT_EQUAL(0, version);

    uint32_t major = (version >> 22) & 0x3FF;
    uint32_t minor = (version >> 12) & 0x3FF;
    uint32_t patch = version & 0xFFF;

    TEST_ASSERT_EQUAL(HAG_API_VERSION_MAJOR, major);
    TEST_ASSERT_EQUAL(HAG_API_VERSION_MINOR, minor);
    TEST_ASSERT_EQUAL(HAG_API_VERSION_PATCH, patch);
}

void test_result_to_string(void) {
    TEST_ASSERT_EQUAL_STRING("HAG_SUCCESS", hagResultToString(HAG_SUCCESS));
    TEST_ASSERT_EQUAL_STRING("HAG_ERROR_OUT_OF_MEMORY", hagResultToString(HAG_ERROR_OUT_OF_MEMORY));
    TEST_ASSERT_EQUAL_STRING("HAG_ERROR_INVALID_ARGUMENT", hagResultToString(HAG_ERROR_INVALID_ARGUMENT));
    TEST_ASSERT_EQUAL_STRING("HAG_ERROR_UNKNOWN_CODE", hagResultToString((HagResult)-999));
}

void test_dtype_sizes(void) {
    TEST_ASSERT_EQUAL(4, hagGetDTypeSize(HAG_DTYPE_FLOAT32));
    TEST_ASSERT_EQUAL(2, hagGetDTypeSize(HAG_DTYPE_FLOAT16));
    TEST_ASSERT_EQUAL(2, hagGetDTypeSize(HAG_DTYPE_BFLOAT16));
    TEST_ASSERT_EQUAL(4, hagGetDTypeSize(HAG_DTYPE_INT32));
    TEST_ASSERT_EQUAL(2, hagGetDTypeSize(HAG_DTYPE_INT16));
    TEST_ASSERT_EQUAL(1, hagGetDTypeSize(HAG_DTYPE_INT8));
    TEST_ASSERT_EQUAL(1, hagGetDTypeSize(HAG_DTYPE_UINT8));
    TEST_ASSERT_EQUAL(1, hagGetDTypeSize(HAG_DTYPE_FP8_E4M3));
    TEST_ASSERT_EQUAL(1, hagGetDTypeSize(HAG_DTYPE_FP8_E5M2));
    TEST_ASSERT_EQUAL(1, hagGetDTypeSize(HAG_DTYPE_BOOL));
}

/* ============================================================================
 * Slab Allocator Tests
 * ============================================================================ */

void test_slab_allocator_basic(void) {
    HagSlabAllocator allocator;
    int result = hagSlabInit(&allocator, 64, 16);
    TEST_ASSERT_EQUAL(0, result);

    void* p1 = hagSlabAlloc(&allocator);
    TEST_ASSERT_NOT_NULL(p1);

    void* p2 = hagSlabAlloc(&allocator);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_NOT_EQUAL(p1, p2);

    hagSlabFree(&allocator, p1);
    hagSlabFree(&allocator, p2);
    hagSlabDestroy(&allocator);
}

void test_slab_allocator_growth(void) {
    HagSlabAllocator allocator;
    hagSlabInit(&allocator, 32, 4);

    /* Allocate more than initial block size to trigger growth */
    void* ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = hagSlabAlloc(&allocator);
        TEST_ASSERT_NOT_NULL(ptrs[i]);
    }

    for (int i = 0; i < 10; i++) {
        hagSlabFree(&allocator, ptrs[i]);
    }
    hagSlabDestroy(&allocator);
}

/* ============================================================================
 * Arena Allocator Tests
 * ============================================================================ */

void test_arena_allocator_basic(void) {
    HagArenaAllocator arena;
    int result = hagArenaInit(&arena, 4096);
    TEST_ASSERT_EQUAL(0, result);

    void* p1 = hagArenaAlloc(&arena, 128);
    TEST_ASSERT_NOT_NULL(p1);

    void* p2 = hagArenaAlloc(&arena, 256);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_NOT_EQUAL(p1, p2);

    size_t offset = hagArenaGetOffset(&arena);
    TEST_ASSERT_GREATER_THAN(0, offset);

    hagArenaReset(&arena);
    TEST_ASSERT_EQUAL(0, hagArenaGetOffset(&arena));

    hagArenaDestroy(&arena);
}

void test_arena_allocator_exhaustion(void) {
    HagArenaAllocator arena;
    hagArenaInit(&arena, 64);

    void* p1 = hagArenaAlloc(&arena, 32);
    TEST_ASSERT_NOT_NULL(p1);

    void* p2 = hagArenaAlloc(&arena, 32);
    TEST_ASSERT_NOT_NULL(p2);

    /* Should fail - arena exhausted */
    void* p3 = hagArenaAlloc(&arena, 32);
    TEST_ASSERT_NULL(p3);

    hagArenaDestroy(&arena);
}
