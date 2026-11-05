<!-- Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
SPDX-License-Identifier: MIT -->

> **Archived design document.** This describes the unsupported C prototype,
> not the current Rust library or its security guarantees. See the
> [current README](../../README.md) and [security policy](../../SECURITY.md).

# Hag Development Roadmap

## Document Information
- **Version**: 0.1.0
- **Status**: Draft
- **Date**: 2025-05-27

## Overview

This roadmap outlines the phased development of Hag from initial planning through production-ready v1.0. Each phase has clear deliverables, success criteria, and estimated timelines.

## Phase 0: Foundation (Months 1-3)
**Goal**: Establish build system, core infrastructure, and Linux driver foundations.

### Deliverables
- [ ] Build system (CMake-based, cross-platform ready)
- [ ] Core type system and result codes
- [ ] Memory allocators (host-side)
- [ ] Linux platform abstraction
- [ ] DRM render node discovery and basic ioctl wrappers
- [ ] Logging and tracing infrastructure
- [ ] Unit test framework integration
- [ ] CI/CD pipeline (GitHub Actions or similar)

### Technical Tasks
1. **Build System**
   - CMake 3.20+ based
   - Support Debug/Release/RelWithDebInfo
   - Option for static vs shared library
   - pkg-config file generation
   - SDK packaging target

2. **Platform Layer**
   - `hag/utils/linux_platform.c` - syscalls, threading, file I/O
   - DRM render node enumeration (`/dev/dri/renderD128+`)
   - PCI device ID mapping to vendor/driver

3. **Memory Management**
   - Slab allocator for small objects
   - Arena allocator for command buffers
   - Custom allocator support (HagAllocationCallbacks)

4. **Testing Infrastructure**
   - Unity Test Framework (single C header) in `libs/`
   - Mock driver for unit tests
   - CI runners with Mesa software rendering (virgl or llvmpipe)

### Success Criteria
- Builds on Ubuntu 22.04/24.04, Fedora 40+
- Unit test suite runs in CI
- Can enumerate DRM render nodes
- Zero compiler warnings with `-Wall -Wextra -Werror`

## Phase 1: Core API & CPU Fallback (Months 4-6)
**Goal**: Implement public API with a CPU fallback driver to enable testing without GPU hardware.

### Deliverables
- [ ] Full `hag/api/api.h` implementation
- [ ] CPU fallback driver (`drv_cpu`)
- [ ] Buffer management (host memory backed)
- [ ] Basic tensor operations
- [ ] Command buffer recording and submission
- [ ] Synchronization primitives (events, semaphores)
- [ ] Basic documentation in `man/`

### Technical Tasks
1. **API Implementation**
   - Instance creation and enumeration
   - Device abstraction (even CPU is a "device")
   - Queue management
   - Buffer create/destroy/map/unmap

2. **CPU Driver**
   - Implements HagDriverInterface
   - Executes kernels via pthread pool
   - Supports basic element-wise ops and GEMM (BLAS)
   - Useful for correctness testing and CI

3. **Tensor Basics**
   - Shape tracking
   - Stride calculation
   - Basic element-wise kernels (add, mul, etc.)

4. **Command Recording**
   - In-memory command buffer representation
   - Simple sequential execution for CPU driver

### Success Criteria
- All API functions return correct error codes
- CPU driver can run simple neural network (MNIST-level)
- API documentation complete for implemented functions
- Test coverage > 80% for core module

## Phase 2: Mesa AMD Driver (Months 7-9)
**Goal**: First real GPU driver targeting AMD GPUs via Mesa/amdgpu.

### Deliverables
- [ ] AMDGPU kernel driver interface
- [ ] User-space command submission (AMD GPU IBs)
- [ ] Memory allocation via amdgpu GEM
- [ ] Shader compilation pipeline (HGSL → SPIR-V → AMD ISA)
- [ ] Basic compute kernel execution on AMD GPU
- [ ] Performance benchmarking vs ROCm

### Technical Tasks
1. **AMDGPU Driver (`drv_amdgpu`)**
   - Open `/dev/dri/renderD*` for AMD devices
   - Implement DRM amdgpu ioctls:
     - `DRM_IOCTL_AMDGPU_CTX`
     - `DRM_IOCTL_AMDGPU_BO_ALLOC`
     - `DRM_IOCTL_AMDGPU_BO_CPU_MAP`
     - `DRM_IOCTL_AMDGPU_CS`
   - Parse AMD GPU info (families, chip classes)

2. **Shader Compilation**
   - Integrate SPIR-V tools (libs/SPIRV-Tools)
   - Use Mesa's aco_compiler or LLVM AMDGPU backend
   - Kernel cache on disk (~/.cache/hag/kernels/)

3. **Memory Management**
   - VRAM allocation via GEM
   - GTT (shared) allocation for host-visible buffers
   - ReBAR detection and utilization

4. **Command Submission**
   - Build AMD PM4 command buffers
   - Submit via amdgpu CS ioctl
   - Handle fences and synchronization

### Success Criteria
- Can execute element-wise kernels on AMD RX 6000/7000 series
- GEMM performance within 50% of rocBLAS
- No hangs or GPU resets during testing
- Memory allocations succeed up to VRAM capacity

## Phase 3: Intel Driver & Multi-Vendor (Months 10-12)
**Goal**: Add Intel Arc/integrated support and refine multi-vendor abstraction.

### Deliverables
- [ ] Intel Xe driver support (`drv_intel`)
   - i915/xe kernel driver interface
   - Intel GPU command submission
- [ ] Unified shader compilation (common SPIR-V path)
- [ ] Multi-device enumeration and selection
- [ ] Peer-to-peer detection (even if not yet implemented)
- [ ] Performance parity with Level Zero on Intel

### Technical Tasks
1. **Intel Driver**
   - Xe/DRM ioctl interface
   - Intel GPU context creation
   - Memory allocation via GEM
   - Command streamer programming

2. **Shader Pipeline Refinement**
   - Single SPIR-V → native compilation path
   - Driver-specific optimizations post-SPIR-V
   - Kernel specialization constants support

3. **Multi-Device**
   - Enumerate all GPUs in system
   - Device selection API improvements
   - Vendor-specific capability queries

### Success Criteria
- Works on Intel Arc A-Series and integrated Xe graphics
- Multi-device systems show all GPUs
- Same kernel binary works on AMD and Intel (SPIR-V portability)

## Phase 4: Advanced Features (Months 13-15)
**Goal**: Execution graphs, advanced memory, and performance optimizations.

### Deliverables
- [ ] Execution graph recording and optimization
- [ ] Kernel fusion engine
- [ ] Sparse memory support
- [ ] Quantization kernels (INT8, INT4, FP8)
- [ ] Attention primitives
- [ ] Memory aliasing / optimization
- [ ] Profiler with timeline export

### Technical Tasks
1. **Graph Engine**
   - DAG representation of kernel execution
   - Topological sort for execution order
   - Kernel fusion: merge element-wise ops
   - Memory aliasing: reuse buffers when safe

2. **Quantization**
   - Q4_0, Q4_1, Q5_0, Q5_1, Q8_0 formats (llama.cpp compatible)
   - Dequantization kernels
   - INT8 GEMM via DP4A

3. **Attention Kernels**
   - FlashAttention-style algorithm
   - Split-K for large attention
   - Causal masking support

4. **Profiler**
   - GPU timestamp queries
   - Host-side event tracing
   - Chrome trace format export

### Success Criteria
- llama.cpp can be ported to run on Hag backend
- Quantized inference works with 4-bit weights
- Profiler produces usable Chrome traces
- Graph execution faster than individual submits

## Phase 5: NVIDIA & Platform Expansion (Months 16-18)
**Goal**: NVIDIA support and preparation for non-Linux platforms.

### Deliverables
- [ ] Nouveau driver support (open-source NVIDIA)
- [ ] Proprietary NVIDIA driver wrapper (if feasible)
- [ ] Windows architecture planning
- [ ] Android architecture planning
- [ ] Python bindings (ctypes/CFFI based)

### Technical Tasks
1. **NVIDIA Support**
   - Nouveau: DRM-based, limited to what nouveau exposes
   - Investigate NVK (new open-source Vulkan driver) compute path
   - Consider CUDA-compatible wrapper layer

2. **Platform Planning**
   - Windows: WDDM model, Mesa on Windows challenges
   - Android: DRM/Gralloc integration
   - macOS/iOS: MoltenVK-style translation or Metal driver

3. **Language Bindings**
   - Python C extension module
   - Integration with PyTorch (custom backend)

### Success Criteria
- Can run on NVIDIA hardware via open-source path
- Windows build compiles (even if drivers incomplete)
- Python bindings functional for simple examples

## Phase 6: Production Hardening (Months 19-24)
**Goal**: Stability, performance, ecosystem.

### Deliverables
- [ ] v1.0 release
- [ ] Stable ABI guarantee
- [ ] Comprehensive test suite (> 90% coverage)
- [ ] Performance benchmarks published
- [ ] Integration with at least one major inference engine
- [ ] Security audit
- [ ] Complete documentation

### Success Criteria
- Passes continuous stress testing (24+ hours)
- API frozen for v1.0 compatibility
- Documented performance within 80% of vendor APIs
- Community contributions accepted

## Milestone Summary

| Phase | Timeline | Key Milestone |
|-------|----------|---------------|
| 0 | M1-M3 | Build + CI + CPU test framework |
| 1 | M4-M6 | CPU fallback runs MNIST |
| 2 | M7-M9 | AMD GPU executes kernels |
| 3 | M10-M12 | Intel + multi-vendor |
| 4 | M13-M15 | Graphs + quantization + attention |
| 5 | M16-M18 | NVIDIA + Windows prep |
| 6 | M19-M24 | v1.0 production release |

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| AMDGPU docs incomplete | High | Use Mesa source as reference; reverse engineer where needed |
| Nouveau too limited | Medium | Focus on open AMD/Intel first; NVIDIA via proprietary wrap later |
| SPIR-V compilation complex | Medium | Start with pre-baked kernels; build compiler incrementally |
| Memory model bugs | High | Extensive testing; use AddressSanitizer; fuzzing |
| Performance gap too large | High | Profile early; optimize hot paths; use assembly kernels |
| Team bandwidth | High | Open source early; build community; clear contribution guidelines |

## Open Questions

1. Should Phase 2 start with RADV (Mesa Vulkan) as a shortcut to AMD support?
2. What's our stance on NVIDIA proprietary driver? Wrap it or skip?
3. Do we need a dedicated compiler team, or can we leverage existing projects?
4. Should we target MLIR as an intermediate representation?
