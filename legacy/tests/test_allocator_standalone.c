/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

/* Standalone host regression for allocator.c only. It does not build or
 * qualify the archived legacy runtime, its drivers, or GPU execution.
 * Example from the repository root (choose a private output directory):
 *   cc -std=c99 -Wall -Wextra -Wpedantic -Werror -I legacy \
 *      legacy/hag/memory/allocator.c legacy/tests/test_allocator_standalone.c \
 *      -o /path/to/private-directory/allocator-regression
 * The same source may be run with AddressSanitizer/UndefinedBehaviorSanitizer.
 */

#include "hag/memory/allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void checkSlabCleared(const HagSlabAllocator* slab) {
    CHECK(slab->objectSize == 0);
    CHECK(slab->alignment == 0);
    CHECK(slab->objectsPerBlock == 0);
    CHECK(slab->blocks == NULL);
    CHECK(slab->freeList == NULL);
    CHECK(slab->allocatedCount == 0);
    CHECK(slab->freeCount == 0);
}

static void checkArenaCleared(const HagArenaAllocator* arena) {
    CHECK(arena->buffer == NULL);
    CHECK(arena->capacity == 0);
    CHECK(arena->offset == 0);
    CHECK(arena->alignment == 0);
}

static void testSlabAlignmentGrowthAndReuse(void) {
    HagSlabAllocator slab;
    unsigned char* objects[5];
    size_t objectSize = sizeof(void*) + 1;
    size_t i, j, k;
    CHECK(hagSlabInit(&slab, objectSize, 2) == 0);
    CHECK(slab.objectSize >= objectSize);
    CHECK(slab.objectSize % slab.alignment == 0);
    for (i = 0; i < 5; ++i) {
        objects[i] = (unsigned char*)hagSlabAlloc(&slab);
        CHECK(objects[i] != NULL);
        for (j = 0; j < i; ++j) CHECK(objects[i] != objects[j]);
        memset(objects[i], (int)(0x30 + i), objectSize);
    }
    CHECK(slab.allocatedCount == 5);
    CHECK(slab.freeCount == 1);
    for (i = 0; i < 5; ++i) {
        for (k = 0; k < objectSize; ++k) CHECK(objects[i][k] == 0x30 + i);
    }
    hagSlabFree(&slab, objects[1]);
    CHECK(slab.allocatedCount == 4);
    CHECK(slab.freeCount == 2);
    CHECK(hagSlabAlloc(&slab) == objects[1]);
    for (i = 0; i < 5; ++i) hagSlabFree(&slab, objects[i]);
    CHECK(slab.allocatedCount == 0);
    CHECK(slab.freeCount == 6);
    hagSlabDestroy(&slab);
    checkSlabCleared(&slab);
    CHECK(hagSlabAlloc(&slab) == NULL);
    hagSlabDestroy(&slab);

    /* Typed stores allow UBSan to check payload-start and stride alignment. */
    CHECK(hagSlabInit(&slab, sizeof(long double), 2) == 0);
    long double* first = (long double*)hagSlabAlloc(&slab);
    long double* second = (long double*)hagSlabAlloc(&slab);
    CHECK(first != NULL && second != NULL && first != second);
    *first = 1.25L;
    *second = 2.5L;
    CHECK(*first == 1.25L && *second == 2.5L);
    hagSlabFree(&slab, first);
    hagSlabFree(&slab, second);
    hagSlabDestroy(&slab);
}

static void testSlabRefusesInvalidGeometryBeforeAllocation(void) {
    HagSlabAllocator slab;
    const size_t invalid[][2] = {
        {0, 1}, {sizeof(void*) - 1, 1}, {sizeof(void*), 0},
        {SIZE_MAX, 1}, {sizeof(void*), SIZE_MAX},
        {(size_t)PTRDIFF_MAX, 2}
    };
    size_t i;
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        /* Init must not inspect uninitialized prior fields. */
        memset(&slab, 0xa5, sizeof(slab));
        CHECK(hagSlabInit(&slab, invalid[i][0], invalid[i][1]) == -1);
        checkSlabCleared(&slab);
        CHECK(hagSlabAlloc(&slab) == NULL);
        hagSlabDestroy(&slab);
        hagSlabDestroy(&slab);
    }
    CHECK(hagSlabInit(NULL, 16, 2) == -1);
    CHECK(hagSlabAlloc(NULL) == NULL);
    hagSlabDestroy(NULL);
    hagSlabFree(NULL, NULL);
}

static void testArenaOverflowAlignmentAndReset(void) {
    HagArenaAllocator arena;
    CHECK(hagArenaInit(&arena, 64) == 0);
    unsigned char* first = (unsigned char*)hagArenaAlloc(&arena, 16);
    CHECK(first != NULL);
    memset(first, 0x5a, 16);
    CHECK(hagArenaGetOffset(&arena) == 16);
    /* The historical alignedOffset + size check wrapped to zero here. */
    CHECK(hagArenaAlloc(&arena, SIZE_MAX - 15) == NULL);
    CHECK(hagArenaAlloc(&arena, SIZE_MAX) == NULL);
    CHECK(hagArenaAlloc(&arena, 0) == NULL);
    CHECK(hagArenaGetOffset(&arena) == 16);
    size_t i;
    for (i = 0; i < 16; ++i) CHECK(first[i] == 0x5a);
    CHECK(hagArenaAlloc(&arena, 48) != NULL);
    CHECK(hagArenaGetOffset(&arena) == 64);
    CHECK(hagArenaAlloc(&arena, 1) == NULL);
    CHECK(hagArenaGetOffset(&arena) == 64);
    hagArenaReset(&arena);
    CHECK(hagArenaGetOffset(&arena) == 0);
    CHECK(hagArenaAlloc(&arena, 16) == first);
    hagArenaDestroy(&arena);
    checkArenaCleared(&arena);
    CHECK(hagArenaAlloc(&arena, 1) == NULL);
    hagArenaDestroy(&arena);

    CHECK(hagArenaInit(&arena, 4 * sizeof(long double) + 1) == 0);
    CHECK(hagArenaAlloc(&arena, 1) != NULL);
    long double* aligned = (long double*)hagArenaAlloc(&arena, sizeof(long double));
    CHECK(aligned != NULL);
    *aligned = 3.75L;
    CHECK(*aligned == 3.75L);
    hagArenaDestroy(&arena);
}

static void testArenaFailedInitializationIsDestroySafe(void) {
    HagArenaAllocator arena;
    const size_t invalid[] = {0, SIZE_MAX};
    size_t i;
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        memset(&arena, 0xa5, sizeof(arena));
        CHECK(hagArenaInit(&arena, invalid[i]) == -1);
        checkArenaCleared(&arena);
        CHECK(hagArenaAlloc(&arena, 1) == NULL);
        hagArenaReset(&arena);
        hagArenaDestroy(&arena);
        hagArenaDestroy(&arena);
    }
    CHECK(hagArenaInit(NULL, 64) == -1);
    CHECK(hagArenaAlloc(NULL, 1) == NULL);
    CHECK(hagArenaGetOffset(NULL) == 0);
    hagArenaReset(NULL);
    hagArenaDestroy(NULL);
}

static void testUnsupportedAllocatorsNeverReturnStorage(void) {
    int callerOwned = 42;
    CHECK(hagBuddyCreate(16, 1024) == NULL);
    CHECK(hagBuddyCreate(SIZE_MAX, SIZE_MAX) == NULL);
    CHECK(hagBuddyAlloc(NULL, 16) == NULL);
    CHECK(hagBuddyGetBlockSize(NULL, &callerOwned) == 0);
    hagBuddyFree(NULL, &callerOwned);
    hagBuddyDestroy(NULL);
    CHECK(hagPoolCreate(16, 1024) == NULL);
    CHECK(hagPoolCreate(16, SIZE_MAX) == NULL);
    CHECK(hagPoolAlloc(NULL, 16, 16) == NULL);
    hagPoolFree(NULL, &callerOwned);
    hagPoolDestroy(NULL);
    CHECK(callerOwned == 42);
}

int main(void) {
    testSlabAlignmentGrowthAndReuse();
    testSlabRefusesInvalidGeometryBeforeAllocation();
    testArenaOverflowAlignmentAndReset();
    testArenaFailedInitializationIsDestroySafe();
    testUnsupportedAllocatorsNeverReturnStorage();
    puts("standalone legacy allocator regression passed");
    return EXIT_SUCCESS;
}
