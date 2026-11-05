<!-- Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
SPDX-License-Identifier: MIT -->

> **Archived design document.** This describes the unsupported C prototype,
> not the current Rust library or its security guarantees. See the
> [current README](../../README.md) and [security policy](../../SECURITY.md).

# Hag API Design Rationale

## Document Information
- **Version**: 0.1.0
- **Status**: Draft
- **Date**: 2025-05-27

## 1. Design Goals

The Hag API is designed to balance several competing priorities:

1. **Explicitness**: Like Vulkan, the developer controls when things happen
2. **Ergonomics**: Less verbose than Vulkan for common AI tasks
3. **Performance**: Zero-cost abstractions, no hidden work
4. **Stability**: C99 API with opaque handles for ABI compatibility
5. **Portability**: Works across GPU vendors without code changes

## 2. Comparison with Existing APIs

### 2.1 vs CUDA

| Aspect | CUDA | Hag |
|--------|------|-----|
| Vendor lock | NVIDIA only | Any GPU |
| Memory model | Unified (implicit) | Explicit types |
| Synchronization | Implicit | Explicit |
| Kernel language | CUDA C | SPIR-V / HGSL |
| Error handling | Exceptions / status | Result codes |
| Graphs | CUDA Graphs | Hag Graphs |
| Open source | No | Yes |

**Hag's approach**: Keep CUDA's programming model familiarity but make vendor-neutral and explicit.

### 2.2 vs Vulkan

| Aspect | Vulkan | Hag |
|--------|--------|-----|
| Primary use | Graphics | AI Compute |
| Verbosity | Very high | Medium |
| Tensor support | None | Native |
| Quantization | None | Built-in |
| Kernel fusion | Manual | Automatic (graphs) |
| Command buffers | Complex lifecycle | Simplified |
| Descriptor sets | Explicit binding | Simplified tensor binding |

**Hag's approach**: Take Vulkan's explicitness and correctness, but remove graphics-specific complexity and add AI-native abstractions.

### 2.3 vs ROCm/HIP

| Aspect | HIP | Hag |
|--------|-----|-----|
| Hardware | AMD only (mostly) | Any GPU |
| CUDA compat | Translation layer | Native API |
| Maturity | Growing | New |
| Open source | Yes | Yes |

**Hag's approach**: Be to all GPUs what HIP wants to be to AMD - but with first-class multi-vendor support from day one.

### 2.4 vs OpenCL

| Aspect | OpenCL | Hag |
|--------|--------|-----|
| Standard | Khronos | Independent |
| Adoption | Declining | New |
| Kernel language | OpenCL C | SPIR-V |
| AI focus | None | Primary |
| Graphs | No | Yes |

**Hag's approach**: Learn from OpenCL's mistakes (too general, poor driver support) and be focused and opinionated.

## 3. API Philosophy

### 3.1 Explicit Resource Management

All resources are explicitly created and destroyed:
```c
HagBuffer buffer;
hagCreateBuffer(device, &info, &buffer);
/* ... use buffer ... */
hagDestroyBuffer(device, buffer);
```

No garbage collection, no reference counting (except internally where needed).

### 3.2 Opaque Handles

All API objects are opaque pointers:
```c
typedef struct HagBuffer_T* HagBuffer;
```

Benefits:
- ABI stability (struct can change without breaking clients)
- Type safety (can't pass wrong handle type)
- Encapsulation (clients can't manipulate internals)

### 3.3 Result Codes

All functions that can fail return `HagResult`:
```c
HagResult hagCreateBuffer(HagDevice device, const HagBufferCreateInfo* pInfo, HagBuffer* pBuffer);
```

Pattern: output parameters are last, result is return value.

### 3.4 CreateInfo Structures

Object creation uses descriptor structures:
```c
typedef struct HagBufferCreateInfo {
    uint64_t        size;
    HagMemoryType   memoryType;
    uint32_t        usageFlags;
} HagBufferCreateInfo;
```

Benefits:
- Extensible (add fields without breaking ABI)
- Self-documenting
- Can be stack-allocated

### 3.5 C99 Base

The API is C99 for maximum compatibility:
- No C++ features in public headers
- No VLA (variable length arrays)
- Standard integer types (`stdint.h`)
- No complex numbers

Future language bindings (Python, Rust, Go) will wrap this C API.

## 4. API Conventions

### 4.1 Naming

| Prefix | Usage |
|--------|-------|
| `hag` | All public API functions |
| `Hag` | All public types, enums, structs |
| `HAG_` | All macros, constants |
| `hag_` | Internal functions (not in public headers) |

### 4.2 Parameter Naming

| Suffix | Meaning |
|--------|---------|
| `Count` | Number of elements in array |
| `p` | Pointer (often output) |
| `pp` | Pointer-to-pointer |
| `Info` | Create/descriptor structure |
| `Flags` | Bitmask |

### 4.3 Error Handling

```c
HagResult result = hagSomeFunction(...);
if (result != HAG_SUCCESS) {
    // Handle error
    fprintf(stderr, "Error: %s\n", hagResultToString(result));
    // Potentially clean up and exit
}
```

### 4.4 Versioning

```c
#define HAG_API_VERSION_MAJOR 0
#define HAG_API_VERSION_MINOR 1
#define HAG_API_VERSION_PATCH 0

// Packed version: major << 22 | minor << 12 | patch
uint32_t hagGetApiVersion(void);  // Returns 0x00001000 for 0.1.0
```

## 5. Key API Decisions

### 5.1 Why Tensors as First-Class Objects?

AI workloads think in tensors, not buffers:
```c
// Vulkan-style (what we avoid):
hagBindBuffer(cmd, 0, buffer, offset);  // Just bytes
hagCmdDispatch(cmd, ...);               // Kernel interprets shape

// Hag-style (what we do):
hagCmdBindTensor(cmd, 0, tensor);       // Shape, dtype, strides included
hagCmdDispatch(cmd, ...);               // Kernel knows tensor layout
```

### 5.2 Why Explicit Memory Types?

CUDA's unified memory hides performance cliffs. Hag makes it explicit:
```c
HAG_MEMORY_DEVICE_LOCAL     // Fastest for GPU, CPU can't touch directly
HAG_MEMORY_HOST_VISIBLE     // CPU can map, slower for GPU
HAG_MEMORY_DEVICE_HOST_VISIBLE // Best of both when ReBAR available
```

### 5.3 Why Graphs?

AI inference is repetitive. Graphs allow:
- One-time recording, many replays
- Cross-kernel optimization (fusion)
- Reduced CPU overhead
- Better scheduling

### 5.4 Why SPIR-V Instead of Custom IR?

SPIR-V is:
- Industry standard (Khronos)
- Tooling ecosystem exists
- Can be generated from multiple languages
- Hardware vendors already support it

## 6. API Layers

```
┌─────────────────────────────────────────┐
│  Layer 3: Framework Integration         │
│  (PyTorch backend, llama.cpp backend) │
├─────────────────────────────────────────┤
│  Layer 2: Convenience Helpers         │
│  (hag/op/tensor_ops.h, hag/op/nn.h)     │
├─────────────────────────────────────────┤
│  Layer 1: Core Public API               │
│  (hag/api/api.h)                        │
├─────────────────────────────────────────┤
│  Layer 0: Driver Interface              │
│  (Internal, not public)                 │
└─────────────────────────────────────────┘
```

Most applications use Layer 1 or 2.
Framework integrations use Layer 2.
Advanced users may use Layer 1 directly.

## 7. Future API Extensions

### 7.1 v0.2 (Planned)
- Multi-device synchronization
- External memory / semaphore import
- Subgroups / warp-level primitives

### 7.2 v1.0 (Planned)
- Stable ABI guarantee
- Extensions mechanism
- Debug utilities

### 7.3 v2.0 (Future)
- Ray tracing primitives (for NeRF, etc.)
- Video decode/encode integration
- FPGA support

## 8. Open Questions

1. Should we provide a C++ RAII wrapper in `hag/api/api.hpp`?
2. Should we have a flat C API or object-oriented style?
3. How do we handle API deprecation?
4. Should we version individual functions or the whole API?
5. Do we need async / callback-based APIs?

## 9. References

- [Vulkan API Design](https://www.khronos.org/assets/uploads/developers/library/2016-vulkan-devday-uk/1-Vulkan-API-Overview.pdf)
- [CUDA Runtime API](https://docs.nvidia.com/cuda/cuda-runtime-api/)
- [HIP API](https://rocm.docs.amd.com/projects/HIP/en/latest/doxygen/html/index.html)
- [OpenCL API](https://www.khronos.org/registry/OpenCL/specs/3.0-unified/html/OpenCL_API.html)
- [Apple Metal API](https://developer.apple.com/documentation/metal)
