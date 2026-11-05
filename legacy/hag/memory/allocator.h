/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#ifndef HAG_MEMORY_ALLOCATOR_H
#define HAG_MEMORY_ALLOCATOR_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Archived host allocator interfaces. Slab and arena allocations support the
 * ordinary scalar alignment of long double, long long and object pointers.
 * Extended/SIMD/GPU alignment is unsupported. Buddy and pool APIs are retained
 * for source compatibility but explicitly refuse all allocation requests.
 *
 * All fields are implementation-owned; callers must not modify them. Serialize
 * all access to an allocator and its allocations. Init accepts uninitialized
 * or destroyed storage, never a live allocator: Destroy before reinitializing.
 * Failed Init leaves zeroed, destroy-safe state. Destroy invalidates all live
 * allocations, clears the state, and may be repeated. This limited repair does
 * not make the archived legacy project buildable or supported as a runtime.
 */

/* ============================================================================
 * Slab Allocator
 * ============================================================================ */

typedef struct HagSlabAllocator {
    size_t  objectSize;     /* Aligned slot stride, at least the requested size */
    size_t  alignment;
    size_t  objectsPerBlock;
    void*   blocks;         /* Linked list of memory blocks */
    void*   freeList;       /* Linked list of free objects */
    size_t  allocatedCount;
    size_t  freeCount;
} HagSlabAllocator;

int hagSlabInit(HagSlabAllocator* allocator, size_t objectSize, size_t objectsPerBlock);
void hagSlabDestroy(HagSlabAllocator* allocator);
void* hagSlabAlloc(HagSlabAllocator* allocator);
/* ptr must be NULL or an exact live allocation from this allocator, freed once.
 * Foreign/interior pointers, double free and use after free are caller errors. */
void hagSlabFree(HagSlabAllocator* allocator, void* ptr);

/* ============================================================================
 * Buddy Allocator
 * ============================================================================ */

typedef struct HagBuddyAllocator HagBuddyAllocator;

/* Unsupported: Create/Alloc return NULL, GetBlockSize returns 0; Destroy/Free
 * are no-ops. No backing storage or usable handle is created. */
HagBuddyAllocator* hagBuddyCreate(size_t minBlockSize, size_t maxBlockSize);
void hagBuddyDestroy(HagBuddyAllocator* allocator);
void* hagBuddyAlloc(HagBuddyAllocator* allocator, size_t size);
void hagBuddyFree(HagBuddyAllocator* allocator, void* ptr);
size_t hagBuddyGetBlockSize(HagBuddyAllocator* allocator, void* ptr);

/* ============================================================================
 * Arena Allocator
 * ============================================================================ */

typedef struct HagArenaAllocator {
    char*   buffer;
    size_t  capacity;
    size_t  offset;
    size_t  alignment;
} HagArenaAllocator;

int hagArenaInit(HagArenaAllocator* arena, size_t capacity);
void hagArenaDestroy(HagArenaAllocator* arena);
void* hagArenaAlloc(HagArenaAllocator* arena, size_t size);
/* Reset invalidates every previous arena allocation without freeing storage. */
void hagArenaReset(HagArenaAllocator* arena);
size_t hagArenaGetOffset(HagArenaAllocator* arena);

/* ============================================================================
 * Pool Allocator (Higher-level)
 * ============================================================================ */

typedef struct HagPool HagPool;

/* Unsupported: Create/Alloc return NULL and Destroy/Free are no-ops. */
HagPool* hagPoolCreate(size_t smallObjectMax, size_t mediumObjectMax);
void hagPoolDestroy(HagPool* pool);
void* hagPoolAlloc(HagPool* pool, size_t size, size_t alignment);
void hagPoolFree(HagPool* pool, void* ptr);

#ifdef __cplusplus
}
#endif

#endif /* HAG_MEMORY_ALLOCATOR_H */
