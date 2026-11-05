<!-- Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
SPDX-License-Identifier: MIT -->

> **Archived design document.** This describes the unsupported C prototype,
> not the current Rust library or its security guarantees. See the
> [current README](../../README.md) and [security policy](../../SECURITY.md).

# Hag Memory Model

## Document Information
- **Version**: 0.1.0
- **Status**: Draft
- **Date**: 2025-05-27

## 1. Goals

1. **Predictable**: Developers know exactly where their data lives
2. **Efficient**: Minimize copies, maximize bandwidth
3. **Flexible**: Support various hardware capabilities (discrete, integrated, unified)
4. **Safe**: Prevent use-after-free, double-free, leaks
5. **Scalable**: Handle models from MBs to 100s of GBs

## 2. Memory Types

### 2.1 Abstract Memory Types

Hag exposes four logical memory types:

| Type | Location | CPU Visible | GPU Optimal | Use Case |
|------|----------|-------------|-------------|----------|
| `HAG_MEMORY_DEVICE_LOCAL` | VRAM | No | Yes | Model weights, activations |
| `HAG_MEMORY_HOST_VISIBLE` | Shared/GTT | Yes, uncached | No | Staging, uploads |
| `HAG_MEMORY_HOST_CACHED` | Shared/GTT | Yes, cached | No | Readback, debugging |
| `HAG_MEMORY_DEVICE_HOST_VISIBLE` | VRAM (ReBAR) | Yes | Yes | Zero-copy when available |

### 2.2 Hardware Mapping

| Hardware | Device Local | Host Visible | Host Cached | Device Host Visible |
|----------|--------------|--------------|-------------|---------------------|
| AMD Discrete | VRAM | GTT (uncached) | GTT (cached) | ReBAR/SAM |
| AMD APU | System RAM | System RAM | System RAM | N/A |
| Intel Discrete | VRAM | System RAM | System RAM | ReBAR |
| Intel Integrated | System RAM | System RAM | System RAM | N/A |
| NVIDIA Discrete | VRAM | Sysmem pinned | Sysmem pinned | ReBAR |
| Apple Silicon | Unified | Unified | Unified | Unified |

## 3. Address Spaces

### 3.1 Virtual Address Space

Each device has a unified virtual address space:
```
0x0000_0000_0000_0000
    ┌─────────────────┐
    │  Reserved       │
    ├─────────────────┤
    │  Host-visible   │  Mapped to process address space
    │  (BAR / GTT)    │
    ├─────────────────┤
    │  Device-local   │  VRAM, GPU-only
    │  (VRAM)         │
    ├─────────────────┤
    │  Sparse regions │  On-demand mapped pages
    ├─────────────────┤
    │  Peer memory    │  Other GPUs in system
    └─────────────────┘
0xFFFF_FFFF_FFFF_FFFF
```

### 3.2 Memory Handles

Memory is referenced by handles, not raw pointers:
- `HagBuffer` - Buffer object (typed, sized)
- `HagMemory` - Raw memory allocation (driver-level)
- `HagTensor` - View into buffer with shape information

## 4. Allocation Strategy

### 4.1 Buffer Object Model

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│  HagTensor   │────→│  HagBuffer   │────→│  HagMemory   │
│  (shape,     │     │  (size,      │     │  (physical   │
│   dtype)     │     │   usage)     │     │   pages)     │
└──────────────┘     └──────────────┘     └──────────────┘
        │                                     │
        └─────────────── optional ────────────┘
                  (user-provided memory)
```

### 4.2 Allocation Paths

**Automatic (Recommended):**
```c
HagBufferCreateInfo info = {
    .size = 1024 * 1024,
    .memoryType = HAG_MEMORY_DEVICE_LOCAL,
    .usageFlags = HAG_BUFFER_USAGE_STORAGE | HAG_BUFFER_USAGE_TRANSFER_DST
};
hagCreateBuffer(device, &info, &buffer);  // Allocates memory automatically
```

**Manual (Advanced):**
```c
// Allocate memory separately
hagAllocateMemory(device, size, HAG_MEMORY_DEVICE_LOCAL, &memory);
// Create buffer backed by that memory
hagCreateBuffer(device, &info, &buffer);
hagBindBufferMemory(device, buffer, memory, 0);
```

### 4.3 Sub-allocation

For small allocations, use sub-allocation from larger heaps:

```
┌──────────────────────────────┐ Heap (256 MB)
│ ┌────┐ ┌────┐ ┌────────┐    │
│ │Buf1│ │Buf2│ │  Buf3  │ ...│
│ └────┘ └────┘ └────────┘    │
└──────────────────────────────┘
```

Benefits:
- Fewer system calls
- Better memory locality
- Reduced fragmentation

## 5. Memory Pools

### 5.1 Pool Types

| Pool | Size | Strategy | Use |
|------|------|----------|-----|
| Small | < 1 MB | Slab allocator | Activations, gradients |
| Medium | 1-256 MB | Buddy allocator | Attention weights, embeddings |
| Large | > 256 MB | Direct | Model weights, KV cache |
| Staging | Fixed | Ring buffer | Host <-> device transfers |

### 5.2 Pool Lifecycle

```
Device Created
    ↓
Pools allocated (pre-sized based on device properties)
    ↓
Buffers allocated from pools
    ↓
Buffers freed (returned to pool)
    ↓
Device Destroyed
Pools destroyed
```

### 5.3 Memory Pressure

When memory is exhausted:
1. Try to reclaim unused pool memory
2. Evict least-recently-used allocations (if evictable)
3. Return `HAG_ERROR_OUT_OF_MEMORY`

## 6. Transfers

### 6.1 Transfer Types

| Transfer | API | Async? | Notes |
|----------|-----|--------|-------|
| Host → Device | `hagCmdCopyBuffer` | Yes | Via staging buffer or direct if unified |
| Device → Host | `hagCmdCopyBuffer` | Yes | Via cached staging |
| Device → Device | `hagCmdCopyBuffer` | Yes | P2P if supported, else via host |
| Fill | `hagCmdFillBuffer` | Yes | GPU-side memset |

### 6.2 Zero-Copy Paths

**Unified Memory (Apple Silicon, ReBAR):**
```
Host Pointer ←→ Same Physical Memory ←→ GPU Pointer
No copy needed!
```

**Direct Mapping:**
```c
hagMapBuffer(device, buffer, &ptr);  // May be slow for device-local
// ... use ptr ...
hagUnmapBuffer(device, buffer);
```

### 6.3 Staging Strategy

For discrete GPUs without ReBAR:
```
Host Data
    ↓ copy (memcpy)
Host Staging Buffer (pinned)
    ↓ copy (DMA)
Device Buffer (VRAM)
```

## 7. Sparse Memory

### 7.1 Use Case

Large models that don't fit contiguously:
- 70B parameter model = 140 GB FP16
- May exceed single allocation limits
- Can map/unmap pages as needed

### 7.2 API

```c
// Create sparse buffer
HagBufferCreateInfo info = {
    .size = 140ULL * 1024 * 1024 * 1024,  // 140 GB
    .memoryType = HAG_MEMORY_DEVICE_LOCAL,
    .usageFlags = HAG_BUFFER_USAGE_SPARSE | HAG_BUFFER_USAGE_STORAGE
};
hagCreateBuffer(device, &info, &buffer);  // No physical memory yet

// Bind memory regions
HagSparseMemoryBind binds[] = {
    { .offset = 0, .size = 4ULL * 1024 * 1024 * 1024, .memory = mem0 },
    { .offset = 4ULL * 1024 * 1024 * 1024, .size = 4ULL * 1024 * 1024 * 1024, .memory = mem1 },
};
hagBindSparseBufferMemory(device, buffer, 2, binds);

// Unbind when no longer needed
hagUnbindSparseBufferMemory(device, buffer, offset, size);
```

### 7.3 Page Size

| Hardware | Page Size | Notes |
|----------|-----------|-------|
| AMD GCN | 4 KB / 64 KB | 64KB for textures, 4KB for buffers |
| AMD RDNA | 4 KB | Uniform |
| Intel | 4 KB | Standard |
| NVIDIA | 4 KB / 64 KB | Varies by generation |

## 8. Tensor Memory Layouts

### 8.1 Layout Types

```c
typedef enum HagTensorLayout {
    HAG_TENSOR_LAYOUT_LINEAR = 0,      // Row-major, contiguous
    HAG_TENSOR_LAYOUT_NCHW = 1,        // Channels-first
    HAG_TENSOR_LAYOUT_NHWC = 2,        // Channels-last
    HAG_TENSOR_LAYOUT_BLOCKED = 3,     // Tiled/blocked for cache
    HAG_TENSOR_LAYOUT_COMPRESSED = 4,  // Quantized/compressed
} HagTensorLayout;
```

### 8.2 Automatic Layout Selection

Hag selects optimal layout based on:
- Operation type (conv prefers NHWC, linear prefers blocked)
- Hardware vendor (Intel prefers NCHW, AMD prefers NHWC)
- Data type (quantized uses packed layouts)

### 8.3 Layout Conversion

```c
hagCmdConvertTensorLayout(cmd, srcTensor, HAG_TENSOR_LAYOUT_BLOCKED, dstTensor);
```

## 9. Memory Aliasing

### 9.1 Purpose

Reuse memory for non-overlapping tensors:
- Activations in inference (forward only, no backward)
- KV cache in attention (grows but can reuse)
- Temporary buffers in fused kernels

### 9.2 API

```c
// Create aliased tensor (no new memory)
HagTensorCreateInfo info = {
    .dimensions = 2,
    .pShape = (uint64_t[]){1024, 1024},
    .dtype = HAG_DTYPE_FLOAT16,
    .buffer = existingBuffer,
    .offset = 0,  // Can offset into buffer
};
hagCreateTensor(device, &info, &aliasedTensor);
```

### 9.3 Graph-Level Aliasing

The graph optimizer automatically aliases tensors:
```
Graph: A → B → C
If B is not used after C starts, C can reuse B's memory
```

## 10. Quantization & Compression

### 10.1 In-Memory Formats

| Format | Bits/Weight | Relative Size | Accuracy |
|--------|-------------|---------------|----------|
| FP32 | 32 | 1.0x | Baseline |
| FP16 | 16 | 0.5x | Good |
| BF16 | 16 | 0.5x | Good |
| FP8 | 8 | 0.25x | Moderate |
| INT8 | 8 | 0.25x | Good (with calibration) |
| INT4 | 4 | 0.125x | Moderate (GGML Q4) |
| INT4_G128 | 4.125 | ~0.13x | Good (per-group scales) |

### 10.2 Block Quantization (GGML Compatible)

```c
typedef struct HagQ4_0Block {
    float   scale;          // Block scale
    uint8_t qs[32/2];       // 32 weights, 4 bits each, packed
} HagQ4_0Block;             // Total: 2 + 16 = 18 bytes for 32 weights
```

### 10.3 Dequantization Kernels

Dequantize on-the-fly in compute kernels:
```
Load Q4_0 block
Extract scale
Unpack weights
Dequantize: float = (weight - 8) * scale  // symmetric
Use in computation
```

## 11. Synchronization

### 11.1 Memory Coherency

| Transfer | Coherency | Action Required |
|----------|-----------|----------------|
| Host write → GPU read | Explicit | `hagFlushBuffer` or barrier |
| GPU write → Host read | Explicit | `hagInvalidateBuffer` or barrier |
| GPU write → GPU read | Explicit | `hagCmdBarrier` in command buffer |
| Unified memory | Automatic | None (hardware coherency) |

### 11.2 Barrier Types

```c
HAG_BARRIER_TYPE_HOST = 0,      // Host <-> device
HAG_BARRIER_TYPE_DEVICE = 1,    // Device <-> device
HAG_BARRIER_TYPE_UNIFIED = 2,   // Unified memory (may be no-op)
```

## 12. Memory Limits & Validation

### 12.1 Limits

| Limit | Query | Typical Value |
|-------|-------|---------------|
| Max allocation size | `deviceLocalMemoryBytes` | 8-48 GB |
| Max buffer size | Device property | 2-4 GB (discrete), unlimited (unified) |
| Alignment | `minMemoryAlignment` | 256 B - 4 KB |
| Address bits | `addressBits` | 32 or 64 |

### 12.2 Validation

Debug builds validate:
- Buffer bounds on access
- Memory type compatibility with usage
- Alignment requirements
- Overlapping aliasing (when unsafe)
- Use-after-free detection (via magic numbers)

## 13. Open Questions

1. Should we support memory oversubscription (swapping to host)?
2. How do we handle NUMA systems with multiple GPU sockets?
3. Do we need a dedicated "scratch" memory type for temporaries?
4. Should tensor layouts be explicit or automatic?
5. How do we profile memory usage effectively?
6. Do we need memory pooling at the driver level or only runtime?

## 14. References

- [Vulkan Memory Model](https://docs.vulkan.org/spec/latest/chapters/memory.html)
- [CUDA Unified Memory](https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#um-overview)
- [ROCm Memory Management](https://rocm.docs.amd.com/projects/HIP/en/latest/how_to_guides/memory.html)
- [Linux DRM Memory](https://dri.freedesktop.org/docs/drm/gpu/drm-mm.html)
- [GGML Quantization](https://github.com/ggerganov/ggml/blob/master/docs/quantization.md)
