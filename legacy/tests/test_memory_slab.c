/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#include "unity.h"
#include "hag/memory/allocator.h"

void test_memory_slab_init_destroy(void) {
    HagSlabAllocator allocator;
    int result = hagSlabInit(&allocator, 64, 16);
    TEST_ASSERT_EQUAL(0, result);
    hagSlabDestroy(&allocator);
}

void test_memory_slab_invalid_size(void) {
    HagSlabAllocator allocator;
    int result = hagSlabInit(&allocator, 2, 16);
    TEST_ASSERT_NOT_EQUAL(0, result);
}

void test_memory_arena_init_destroy(void) {
    HagArenaAllocator arena;
    int result = hagArenaInit(&arena, 1024);
    TEST_ASSERT_EQUAL(0, result);
    hagArenaDestroy(&arena);
}
