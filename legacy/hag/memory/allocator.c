/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#include "hag/memory/allocator.h"
#include <stdlib.h>

/* C99-compatible alignment for ordinary scalar objects. This does not admit
 * extended alignment (for example SIMD or GPU buffer alignment). */
typedef union HagScalarAlignment {
    long double floating;
    long long integer;
    void* pointer;
} HagScalarAlignment;

typedef struct HagAlignmentProbe {
    char prefix;
    HagScalarAlignment value;
} HagAlignmentProbe;

#define HAG_ALLOC_ALIGNMENT offsetof(HagAlignmentProbe, value)

static int hagAlignSize(size_t size, size_t alignment, size_t* result) {
    size_t remainder;
    size_t padding;
    if (alignment == 0) return -1;
    remainder = size % alignment;
    padding = remainder == 0 ? 0 : alignment - remainder;
    if (size > SIZE_MAX - padding) return -1;
    *result = size + padding;
    return 0;
}

/* ============================================================================
 * Slab Allocator Implementation
 * ============================================================================ */

typedef struct SlabBlock SlabBlock;
struct SlabBlock {
    SlabBlock*  next;
    char        data[];
};

typedef struct SlabFreeNode {
    struct SlabFreeNode* next;
} SlabFreeNode;

static int hagSlabLayout(size_t objectSize, size_t objectsPerBlock,
                         size_t* stride, size_t* headerSize, size_t* blockSize) {
    if (objectSize < sizeof(SlabFreeNode) || objectsPerBlock == 0) return -1;
    if (hagAlignSize(objectSize, HAG_ALLOC_ALIGNMENT, stride) != 0 ||
        hagAlignSize(sizeof(SlabBlock), HAG_ALLOC_ALIGNMENT, headerSize) != 0) return -1;
    if (objectsPerBlock > (SIZE_MAX - *headerSize) / *stride) return -1;
    *blockSize = *headerSize + *stride * objectsPerBlock;
    /* Keep all offsets representable for host pointer arithmetic. */
    if (*blockSize > PTRDIFF_MAX) return -1;
    return 0;
}

int hagSlabInit(HagSlabAllocator* allocator, size_t objectSize, size_t objectsPerBlock) {
    size_t stride, headerSize, blockSize;
    if (!allocator) return -1;
    *allocator = (HagSlabAllocator){0};
    if (hagSlabLayout(objectSize, objectsPerBlock, &stride, &headerSize, &blockSize) != 0)
        return -1;

    allocator->objectSize = stride;
    allocator->alignment = HAG_ALLOC_ALIGNMENT;
    allocator->objectsPerBlock = objectsPerBlock;
    allocator->blocks = NULL;
    allocator->freeList = NULL;
    allocator->allocatedCount = 0;
    allocator->freeCount = 0;
    return 0;
}

static int hagSlabGrow(HagSlabAllocator* allocator) {
    size_t stride, headerSize, blockSize;
    if (hagSlabLayout(allocator->objectSize, allocator->objectsPerBlock,
                      &stride, &headerSize, &blockSize) != 0) return -1;
    if (allocator->freeCount > SIZE_MAX - allocator->allocatedCount ||
        allocator->objectsPerBlock >
            SIZE_MAX - allocator->allocatedCount - allocator->freeCount) return -1;
    SlabBlock* block = (SlabBlock*)malloc(blockSize);
    if (!block) return -1;

    block->next = allocator->blocks;
    allocator->blocks = block;

    char* objects = (char*)block + headerSize;
    for (size_t i = 0; i < allocator->objectsPerBlock; ++i) {
        SlabFreeNode* node = (SlabFreeNode*)(objects + i * stride);
        node->next = allocator->freeList;
        allocator->freeList = node;
        allocator->freeCount++;
    }
    return 0;
}

void hagSlabDestroy(HagSlabAllocator* allocator) {
    if (!allocator) return;

    SlabBlock* block = allocator->blocks;
    while (block) {
        SlabBlock* next = block->next;
        free(block);
        block = next;
    }
    *allocator = (HagSlabAllocator){0};
}

void* hagSlabAlloc(HagSlabAllocator* allocator) {
    if (!allocator || allocator->objectSize < sizeof(SlabFreeNode) ||
        allocator->alignment != HAG_ALLOC_ALIGNMENT || allocator->objectsPerBlock == 0 ||
        allocator->objectSize % HAG_ALLOC_ALIGNMENT != 0 ||
        allocator->allocatedCount == SIZE_MAX) return NULL;

    if (!allocator->freeList) {
        if (hagSlabGrow(allocator) != 0) return NULL;
    }
    if (allocator->freeCount == 0) return NULL;

    SlabFreeNode* node = allocator->freeList;
    allocator->freeList = node->next;
    allocator->freeCount--;
    allocator->allocatedCount++;
    return node;
}

void hagSlabFree(HagSlabAllocator* allocator, void* ptr) {
    if (!allocator || !ptr || allocator->allocatedCount == 0 ||
        allocator->freeCount == SIZE_MAX) return;

    SlabFreeNode* node = (SlabFreeNode*)ptr;
    node->next = allocator->freeList;
    allocator->freeList = node;
    allocator->freeCount++;
    allocator->allocatedCount--;
}

/* ============================================================================
 * Arena Allocator Implementation
 * ============================================================================ */

int hagArenaInit(HagArenaAllocator* arena, size_t capacity) {
    if (!arena) return -1;
    *arena = (HagArenaAllocator){0};
    if (capacity == 0 || capacity > PTRDIFF_MAX) return -1;

    arena->buffer = (char*)malloc(capacity);
    if (!arena->buffer) return -1;

    arena->capacity = capacity;
    arena->offset = 0;
    arena->alignment = HAG_ALLOC_ALIGNMENT;
    return 0;
}

void hagArenaDestroy(HagArenaAllocator* arena) {
    if (!arena) return;
    free(arena->buffer);
    *arena = (HagArenaAllocator){0};
}

void* hagArenaAlloc(HagArenaAllocator* arena, size_t size) {
    size_t alignedOffset;
    if (!arena || !arena->buffer || size == 0 ||
        arena->alignment != HAG_ALLOC_ALIGNMENT || arena->offset > arena->capacity)
        return NULL;

    if (hagAlignSize(arena->offset, arena->alignment, &alignedOffset) != 0 ||
        alignedOffset > arena->capacity || size > arena->capacity - alignedOffset)
        return NULL;

    void* ptr = arena->buffer + alignedOffset;
    arena->offset = alignedOffset + size;
    return ptr;
}

void hagArenaReset(HagArenaAllocator* arena) {
    if (!arena) return;
    arena->offset = 0;
}

size_t hagArenaGetOffset(HagArenaAllocator* arena) {
    return arena ? arena->offset : 0;
}

/* ============================================================================
 * Buddy Allocator: explicitly unsupported in this archived implementation
 * ============================================================================ */

struct HagBuddyAllocator {
    size_t minBlockSize;
    size_t maxBlockSize;
    void*  memory;
};

HagBuddyAllocator* hagBuddyCreate(size_t minBlockSize, size_t maxBlockSize) {
    /* The historical stub returned the same storage for every allocation.
     * Refuse construction until splitting, ownership and coalescing exist. */
    (void)minBlockSize;
    (void)maxBlockSize;
    return NULL;
}

void hagBuddyDestroy(HagBuddyAllocator* allocator) {
    (void)allocator;
}

void* hagBuddyAlloc(HagBuddyAllocator* allocator, size_t size) {
    (void)allocator;
    (void)size;
    return NULL;
}

void hagBuddyFree(HagBuddyAllocator* allocator, void* ptr) {
    (void)allocator;
    (void)ptr;
}

size_t hagBuddyGetBlockSize(HagBuddyAllocator* allocator, void* ptr) {
    (void)allocator;
    (void)ptr;
    return 0;
}

/* ============================================================================
 * Pool Allocator: explicitly unsupported in this archived implementation
 * ============================================================================ */

struct HagPool {
    HagSlabAllocator    smallSlab;
    HagBuddyAllocator*  buddy;
    size_t              smallObjectMax;
    size_t              mediumObjectMax;
};

HagPool* hagPoolCreate(size_t smallObjectMax, size_t mediumObjectMax) {
    /* A pool needs supported backing allocators and allocation-source tracking.
     * Its old free(ptr) path could free a pointer inside a slab or buddy block. */
    (void)smallObjectMax;
    (void)mediumObjectMax;
    return NULL;
}

void hagPoolDestroy(HagPool* pool) {
    (void)pool;
}

void* hagPoolAlloc(HagPool* pool, size_t size, size_t alignment) {
    (void)pool;
    (void)size;
    (void)alignment;
    return NULL;
}

void hagPoolFree(HagPool* pool, void* ptr) {
    (void)pool;
    (void)ptr;
}
