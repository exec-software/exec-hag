<!-- Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
SPDX-License-Identifier: MIT -->

> **Archived design document.** This describes the unsupported C prototype,
> not the current Rust library or its security guarantees. See the
> [current README](../../README.md) and [security policy](../../SECURITY.md).

# Hag System Architecture

## Document Information
- **Version**: 0.1.0
- **Status**: Draft
- **Author**: Execute Software Architecture Team
- **Date**: 2025-05-27

## 1. Executive Summary

Hag is a bare-metal GPU compute API designed specifically for AI/ML workloads. Unlike graphics-oriented APIs (Vulkan, OpenGL) or vendor-locked solutions (CUDA, ROCm), Hag provides a unified, open-source compute layer optimized for tensor operations, memory patterns, and kernel dispatch typical in modern machine learning.

The architecture follows a layered design:
```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                         │
│         (LLM Inference Engines, Training Frameworks)         │
├─────────────────────────────────────────────────────────────┤
│                      Hag Public API                          │
│              hag/api/api.h  -  Stable Interface              │
├─────────────────────────────────────────────────────────────┤
│                    Hag Core Runtime                          │
│    Context Management │ Memory Pools │ Scheduling │ Graphs   │
├─────────────────────────────────────────────────────────────┤
│              Hardware Abstraction Layer (HAL)                │
│         Vendor-agnostic interface to GPU capabilities        │
├─────────────────────────────────────────────────────────────┤
│                   Driver Implementations                     │
│     Mesa DRM/KMS │ NVIDIA (open) │ Intel │ Apple │ QCOM    │
├─────────────────────────────────────────────────────────────┤
│                     Kernel Space                             │
│           drm_ioctl │ amdgpu │ i915 │ nouveau │ panfrost   │
├─────────────────────────────────────────────────────────────┤
│                     Hardware                                 │
│          AMD GPU │ Intel Arc │ NVIDIA │ Apple Silicon       │
└─────────────────────────────────────────────────────────────┘
```

## 2. Design Principles

### 2.1 AI-First Design
Every API decision prioritizes ML workload patterns:
- **Tensor-centric**: Native tensor types with automatic shape tracking
- **Kernel graphs**: Record and replay entire inference graphs with one submit
- **Quantization-aware**: First-class support for INT8, INT4, FP8, BF16
- **Attention-optimized**: Special primitives for transformer attention patterns

### 2.2 Vendor Neutrality
No vendor-specific extensions in core API. All hardware-specific optimizations live in driver implementations.

### 2.3 Zero-Copy Where Possible
Minimize host/device transfers through:
- Unified memory when available (Resizable BAR, Apple Unified Memory)
- Peer-to-peer transfers between devices
- Sparse memory for large models

### 2.4 Predictable Performance
- Explicit synchronization (no hidden stalls)
- Pre-compiled kernels for common operations
- Deterministic memory allocation

## 3. Component Architecture

### 3.1 Public API Layer (`hag/api/`)
The stable C99 interface that all consumers link against.

**Key Characteristics:**
- C99 for maximum ABI compatibility
- Opaque handles to prevent ABI breakage
- Extensible via `pNext` chain pattern (inspired by Vulkan but simplified)
- Versioned for backward compatibility

**Modules:**
- `api.h` - Core API (devices, queues, synchronization)
- `tensor.h` - Tensor operations and shapes
- `kernel.h` - Kernel loading and compilation
- `memory.h` - Memory allocation and management
- `graph.h` - Execution graphs
- `math.h` - Built-in math primitives (GEMM, conv, attention)

### 3.2 Core Runtime (`hag/core/`)
The engine that orchestrates API calls into actual GPU work.

**Responsibilities:**
- Instance and device lifecycle management
- Queue scheduling and load balancing
- Memory pool management
- Graph optimization and execution
- Error handling and recovery

**Subsystems:**
- **Context Manager**: Tracks active instances, devices, and resources
- **Scheduler**: Work submission, queue selection, async execution
- **Memory Manager**: Pool allocators, sub-allocation, defragmentation
- **Graph Engine**: Graph recording, optimization, replay
- **Profiler**: Timestamp queries, performance counters, trace export

### 3.3 Hardware Abstraction Layer (`hag/driver/`)
Translates Hag operations into vendor-specific commands.

**Design:**
- Driver interface is internal (not public API)
- Each driver is a dynamically loadable module
- Common infrastructure shared across drivers

**Driver Interface:**
```c
typedef struct HagDriverInterface {
    HagResult (*createDevice)(...);
    HagResult (*destroyDevice)(...);
    HagResult (*allocateMemory)(...);
    HagResult (*freeMemory)(...);
    HagResult (*createKernel)(...);
    HagResult (*dispatchKernel)(...);
    HagResult (*submitQueue)(...);
    HagResult (*waitIdle)(...);
    /* ... etc ... */
} HagDriverInterface;
```

**Planned Drivers:**
| Driver | Target | Status | Priority |
|--------|--------|--------|----------|
| `drv_amdgpu` | AMD GPUs via amdgpu kernel driver | Planned | P0 |
| `drv_intel` | Intel GPUs via i915/xe driver | Planned | P0 |
| `drv_nouveau` | NVIDIA via nouveau (open) | Planned | P1 |
| `drv_panfrost` | ARM Mali GPUs | Planned | P2 |
| `drv_virtio` | Virtual GPU (testing/CI) | Planned | P1 |
| `drv_cpu` | CPU fallback for testing | Planned | P0 |

### 3.4 Memory Subsystem (`hag/memory/`)
Optimized memory management for AI workloads.

**Features:**
- **Device-local heaps**: VRAM management with pooling
- **Host-visible heaps**: Zero-copy staging when possible
- **Unified heaps**: Single address space (Apple Silicon, ReBAR)
- **Sparse allocations**: Map/unmap pages for large models
- **Tensor layouts**: Automatic selection of optimal memory layouts (NCHW, NHWC, blocked)

**Allocation Strategy:**
```
Small allocations (< 1MB): Slab allocator
Medium allocations (1MB - 256MB): Buddy allocator
Large allocations (> 256MB): Direct allocation + sparse support
```

### 3.5 Compute Subsystem (`hag/compute/`)
Kernel execution and dispatch.

**Features:**
- **HGSL Compiler**: Hag GPU Shading Language → SPIR-V → native ISA
- **Kernel cache**: Persistent compiled kernel cache on disk
- **Wavefront tuning**: Automatic workgroup size selection per GPU
- **Multi-dispatch**: Batch multiple kernels in single submit
- **Indirect dispatch**: GPU-generated work dimensions

### 3.6 Utility Layer (`hag/utils/`)
Shared infrastructure.

**Components:**
- Thread pool for async operations
- Lock-free queues for producer/consumer patterns
- Math utilities (quantization, dequantization)
- Logging and tracing infrastructure
- Platform abstraction (Linux syscalls, file I/O, threading)

## 4. Data Flow

### 4.1 Inference Flow
```
1. Application creates tensors for model weights + inputs
2. Weights are loaded from disk → host memory → device memory
3. Application records an execution graph (or uses pre-built)
4. Graph is optimized (kernel fusion, memory aliasing)
5. Graph is submitted to compute queue
6. Application waits on fence or polls event
7. Results are read back (or used in-place for next iteration)
```

### 4.2 Training Flow
```
1. Application sets up forward graph
2. Application sets up backward graph
3. Optimizer state tensors allocated
4. Forward + backward executed per batch
5. Optimizer kernel updates weights
6. Gradients zeroed for next iteration
```

## 5. Multi-Device Support

### 5.1 Single-Host Multi-GPU
- Peer-to-peer memory transfers where supported
- Automatic tensor sharding for model parallelism
- Ring-allreduce for data parallelism

### 5.2 Multi-Node (Future)
- NCCL-like collectives built on RDMA
- Out of scope for v1.0, architecture supports extension

## 6. Security Model

- No elevated privileges required (runs as regular user via DRM render nodes)
- Sandboxed kernel execution (GPU memory isolated between processes)
- No JIT compilation from untrusted sources in default mode
- Optional IOMMU for additional isolation

## 7. Performance Targets

### 7.1 Baseline (v1.0)
- Within 80% of native vendor API (CUDA/ROCm) on same hardware
- Sub-10 microsecond kernel launch overhead
- Support for models up to 70B parameters (with sufficient VRAM)

### 7.2 Stretch Goals
- Within 90% of native API through driver-specific optimizations
- Sub-5 microsecond kernel launch
- 100B+ parameter models with sparse memory

## 8. Open Questions

1. Should we support Vulkan as a fallback driver for broader hardware support?
2. How do we handle NVIDIA's closed-source driver stack on Linux (proprietary blob)?
3. Should we implement a Triton-like kernel DSL or stick to SPIR-V?
4. What's our strategy for Windows drivers (Mesa on Windows via Zink/Clover)?
5. Do we need a Python binding layer, or is C API sufficient for framework integration?

## 9. Related Work & Inspiration

- **CUDA**: Programming model and memory semantics
- **Vulkan**: Synchronization primitives, explicit resource management
- **ROCm/HIP**: Open-source GPU compute philosophy
- **WebGPU**: Modern API design, explicitness
- **oneAPI**: Cross-vendor abstraction approach
- **Mesa Gallium**: Driver architecture inspiration

## 10. Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1.0 | 2025-05-27 | Execute Software | Initial architecture draft |
