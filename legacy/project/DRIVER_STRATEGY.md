<!-- Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
SPDX-License-Identifier: MIT -->

> **Archived design document.** This describes the unsupported C prototype,
> not the current Rust library or its security guarantees. See the
> [current README](../../README.md) and [security policy](../../SECURITY.md).

# Hag Driver Strategy

## Document Information
- **Version**: 0.1.0
- **Status**: Draft
- **Date**: 2025-05-27

## 1. Philosophy

Hag's driver strategy is built on a core principle: **maximize open-source driver coverage to enable development, testing, and deployment without proprietary dependencies**.

This means:
1. Mesa drivers are the primary target on Linux
2. Proprietary drivers are wrapped only when necessary
3. The driver interface is internal and can evolve
4. Each driver is self-contained with minimal shared state

## 2. Why Mesa?

### 2.1 Openness
- Full source code available (gitlab.freedesktop.org/mesa/mesa)
- No NDAs required for driver development
- Community-maintained and audited

### 2.2 Hardware Coverage
| Vendor | Mesa Driver | Kernel Driver | Hardware |
|--------|-------------|---------------|----------|
| AMD | RadeonSI/RADV | amdgpu | RX 400+ |
| Intel | Iris/ANV | i915/xe | Gen8+ |
| NVIDIA | Nouveau/NVK | nouveau | Most GPUs |
| ARM | Panfrost/Lima | panfrost | Mali |
| Qualcomm | Freedreno | msm | Adreno |
| Broadcom | V3D/VC4 | v3d | Raspberry Pi |
| Apple | Asahi | - | M1/M2/M3 |

### 2.3 Development Velocity
- Can test on any system without vendor SDKs
- Easy CI with software rendering (llvmpipe, softpipe)
- Debug symbols and source available

## 3. Driver Architecture

### 3.1 Driver Interface

All drivers implement a common interface. This is an INTERNAL interface, not public API.

```c
/* hag/driver/driver_interface.h */
#ifndef HAG_DRIVER_INTERFACE_H
#define HAG_DRIVER_INTERFACE_H

typedef struct HagDriver HagDriver;

typedef struct HagDriverOps {
    /* Lifecycle */
    HagResult (*probe)(int drmFd, uint32_t vendorId, uint32_t deviceId, int* pScore);
    HagResult (*create)(int drmFd, const HagAllocationCallbacks* pAlloc, HagDriver** ppDriver);
    void      (*destroy)(HagDriver* pDriver);

    /* Device Info */
    HagResult (*getDeviceProperties)(HagDriver* pDriver, HagDeviceProperties* pProps);

    /* Memory */
    HagResult (*allocDeviceMemory)(HagDriver* pDriver, uint64_t size, uint32_t type, HagDriverMemory* pMemory);
    void      (*freeDeviceMemory)(HagDriver* pDriver, HagDriverMemory memory);
    HagResult (*mapMemory)(HagDriver* pDriver, HagDriverMemory memory, void** ppData);
    void      (*unmapMemory)(HagDriver* pDriver, HagDriverMemory memory);

    /* Buffers */
    HagResult (*createBuffer)(HagDriver* pDriver, uint64_t size, uint32_t usage, HagDriverBuffer* pBuffer);
    void      (*destroyBuffer)(HagDriver* pDriver, HagDriverBuffer buffer);
    HagResult (*bindBufferMemory)(HagDriver* pDriver, HagDriverBuffer buffer, HagDriverMemory memory, uint64_t offset);

    /* Execution */
    HagResult (*createQueue)(HagDriver* pDriver, HagQueueType type, HagDriverQueue* pQueue);
    void      (*destroyQueue)(HagDriver* pDriver, HagDriverQueue queue);
    HagResult (*submit)(HagDriver* pDriver, HagDriverQueue queue, const HagDriverSubmitInfo* pSubmit);
    HagResult (*waitQueueIdle)(HagDriver* pDriver, HagDriverQueue queue);

    /* Kernels */
    HagResult (*createKernel)(HagDriver* pDriver, const void* pCode, size_t codeSize, HagKernelLanguage lang, HagDriverKernel* pKernel);
    void      (*destroyKernel)(HagDriver* pDriver, HagDriverKernel kernel);
    HagResult (*getKernelWorkGroupSize)(HagDriver* pDriver, HagDriverKernel kernel, uint32_t* pSize);

    /* Synchronization */
    HagResult (*createFence)(HagDriver* pDriver, HagDriverFence* pFence);
    void      (*destroyFence)(HagDriver* pDriver, HagDriverFence fence);
    HagResult (*resetFence)(HagDriver* pDriver, HagDriverFence fence);
    HagResult (*waitFences)(HagDriver* pDriver, uint32_t count, HagDriverFence* pFences, uint64_t timeout);
} HagDriverOps;

struct HagDriver {
    const HagDriverOps* ops;
    void* driverData;  /* Driver-specific context */
};

#endif
```

### 3.2 Driver Loading

Drivers are discovered at runtime:

```
1. Enumerate /dev/dri/renderD128, renderD129, ...
2. For each, query PCI vendor/device ID
3. Ask each registered driver: "can you handle this?"
   - Driver returns a score (0 = cannot handle, higher = better match)
4. Select highest-scoring driver
5. Initialize driver with DRM fd
```

### 3.3 Driver Registration

Drivers register themselves via constructor functions:

```c
/* In drv_amdgpu.c */
static const HagDriverOps g_amdgpuOps = { ... };

HAG_CONSTRUCTOR static void register_amdgpu(void) {
    hagDriverRegister(&g_amdgpuOps);
}
```

## 4. Driver Implementations

### 4.1 CPU Fallback Driver (`drv_cpu`)

**Purpose**: Enable testing without any GPU hardware.

**Approach**:
- No DRM required
- Implements HagDriverInterface via pthreads
- Supports basic element-wise and BLAS operations
- Useful for CI, debugging, and correctness verification

**Implementation Notes**:
- Use OpenBLAS or BLIS for GEMM (third-party in `libs/`)
- Simple loop-based kernels for element-wise ops
- No actual command buffers; execute immediately or via thread pool

### 4.2 AMDGPU Driver (`drv_amdgpu`)

**Purpose**: Primary GPU target. AMD has the most open GPU stack.

**Components**:
1. **DRM Interface**: Direct `amdgpu` ioctls
2. **Memory Manager**: GEM BO allocation, VRAM vs GTT
3. **Command Stream**: PM4 command buffers
4. **Shader Compiler**: SPIR-V → LLVM AMDGPU → binary

**DRM ioctls Used**:
- `DRM_IOCTL_AMDGPU_CTX` - GPU context
- `DRM_IOCTL_AMDGPU_BO_ALLOC` - Buffer object allocation
- `DRM_IOCTL_AMDGPU_BO_CPU_MAP` - CPU mapping
- `DRM_IOCTL_AMDGPU_CS` - Command submission
- `DRM_IOCTL_AMDGPU_WAIT_CS` - Wait for completion
- `DRM_IOCTL_AMDGPU_GEM_METADATA` - Buffer metadata
- `DRM_IOCTL_AMDGPU_GEM_CREATE` - GEM creation

**Command Submission Flow**:
```
1. Build PM4 packet stream
   - WRITE_DATA (set shader registers)
   - DISPATCH_DIRECT or DISPATCH_INDIRECT
   - EVENT_WRITE_EOP (fence)
2. Submit via amdgpu_cs_submit ioctl
3. Signal fence on completion
```

**Shader Pipeline**:
```
HGSL source (optional)
    ↓
SPIR-V (standard IR)
    ↓
Mesa's aco_compiler OR LLVM AMDGPU backend
    ↓
AMD GPU native binary (GCN/RDNA ISA)
    ↓
Upload to GPU via PM4 packets
```

### 4.3 Intel Driver (`drv_intel`)

**Purpose**: Intel Arc and integrated graphics.

**Components**:
1. **DRM Interface**: `i915` or `xe` ioctls
2. **Memory Manager**: GEM on Intel
3. **Command Stream**: Batch buffers (ring buffer)
4. **Shader Compiler**: SPIR-V → Intel GEN compiler

**DRM ioctls Used**:
- `DRM_IOCTL_I915_GEM_CREATE`
- `DRM_IOCTL_I915_GEM_SET_DOMAIN`
- `DRM_IOCTL_I915_GEM_MMAP`
- `DRM_IOCTL_I915_GEM_EXECBUFFER2`
- `DRM_IOCTL_I915_GEM_WAIT`

**Note**: Intel's `xe` driver is the modern replacement for `i915`. We should target `xe` for new hardware.

### 4.4 Nouveau Driver (`drv_nouveau`)

**Purpose**: Open-source NVIDIA support.

**Challenges**:
- Nouveau lacks reclocking → poor performance
- No firmware for newer GPUs (TU116+ signed firmware)
- Limited compute support

**Approach**:
- Use nouveau DRM for memory and submission
- SPIR-V → nouveau compiler (NVC0)
- Accept limited performance; position as "works but slow"

**Alternative**: NVK (new Vulkan driver) might expose compute. Evaluate using NVK's internal NAK compiler.

### 4.5 Virtual Driver (`drv_virtio`)

**Purpose**: CI/testing without real GPU.

**Approach**:
- VirtIO GPU driver in Mesa
- Command submission via virtio protocol
- Host can be llvmpipe or real GPU

## 5. Shader Compilation Strategy

### 5.1 Intermediate Representation: SPIR-V

SPIR-V is the lingua franca:
- Khronos standard
- Supported by Mesa (Vulkan drivers can ingest it)
- Can be generated from multiple languages
- Stable binary format

### 5.2 Compilation Pipeline

```
┌─────────────┐     ┌──────────────┐     ┌─────────────────┐
│   HGSL      │     │   SPIR-V     │     │  Native ISA     │
│  (future)   │────→│  (binary)    │────→│  (AMD/Intel/    │
│             │     │              │     │   NVIDIA)       │
└─────────────┘     └──────────────┘     └─────────────────┘
                           ↑
                    ┌──────┴──────┐
                    │ SPIR-V Tools │
                    │  (validate,  │
                    │  optimize)   │
                    └─────────────┘
```

For Phase 1-2, we will:
1. Hand-write SPIR-V for common kernels
2. Use `libs/SPIRV-Tools` for validation
3. Use Mesa's `spirv_to_nir` for compilation to native

### 5.3 Kernel Cache

Compiled kernels are cached on disk:
```
~/.cache/hag/kernels/
├── amdgpu/
│   └── <hash>.bin
├── intel/
│   └── <hash>.bin
└── index.db
```

Cache key = SHA256(SPIR-V binary + driver version + GPU variant)

## 6. Mesa Integration

### 6.1 Build-Time Dependency

Mesa is NOT a runtime dependency. We use Mesa source/code patterns as reference, but Hag is a standalone project.

However, we MAY link against:
- `libdrm` (small, stable, already installed)
- `libelf` (for shader binaries)

### 6.2 Code Borrowing

We can borrow Mesa code (MIT license) for:
- GPU family detection tables
- Register definitions
- Command packet builders
- SPIR-V to NIR conversion

All borrowed code must be:
- Clearly marked with original copyright
- Placed in `hag/driver/mesa_compat/`
- Tracked in `thirdparty.md`

## 7. Proprietary Driver Strategy

### 7.1 NVIDIA Proprietary

**Option A**: Do not support directly. Users can use Nouveau (slow) or stick with CUDA.

**Option B**: Create `drv_nvidia_proprietary` that uses:
- `libcuda.so` via `dlopen` (avoids linking dependency)
- Translates Hag calls to CUDA driver API calls
- This is a compatibility layer, not a true driver

**Decision**: Implement Option B in Phase 5. It provides value to users stuck with NVIDIA proprietary drivers while keeping our core open.

### 7.2 AMD Proprietary (amdgpu-pro)

Not needed. Mesa amdgpu is superior and fully open.

### 7.3 Intel Proprietary

Not applicable. Intel uses Mesa exclusively on Linux.

## 8. Platform Driver Notes

### 8.1 Linux (Primary)
- Direct DRM/KMS
- Mesa drivers
- No window system required (headless compute)

### 8.2 Windows (Future)
**Challenges**:
- Mesa on Windows exists but is incomplete
- WDDM driver model is closed
- DirectX 12 could be a backend

**Options**:
1. Use WSL2 + Linux drivers (short term)
2. Port Mesa to Windows (long term, hard)
3. Use D3D12 compute as a backend (similar to Zink)
4. Use Vulkan compute as a backend (via Zink or directly)

**Decision**: Target Vulkan compute backend for Windows. This gives us access to all GPUs via vendor drivers. Not ideal (we wanted to avoid Vulkan), but pragmatic for Windows.

### 8.3 Android (Future)
- DRM/Gralloc for memory
- Mesa drivers available
- Similar to Linux but with Android-specific memory management

### 8.4 macOS/iOS (Future)
- No DRM on macOS
- Apple Silicon uses Metal
- Options:
  1. Use Metal Performance Shaders
  2. Port Asahi driver (Linux-only today)
  3. Use MoltenVK → Vulkan → Hag (too many layers)

**Decision**: Metal backend for Apple platforms. Similar to D3D12 on Windows, it's a pragmatic concession.

## 9. Driver Testing Matrix

| Driver | CI Target | Hardware Test | Priority |
|--------|-----------|---------------|----------|
| drv_cpu | Always | Always | P0 |
| drv_virtio | Always | N/A | P1 |
| drv_amdgpu | Mesa Docker image | RX 6000/7000 | P0 |
| drv_intel | Mesa Docker image | Arc A750 | P0 |
| drv_nouveau | Mesa Docker image | GTX 1060 | P2 |
| drv_nvidia_proprietary | N/A | RTX 3060 | P3 |
| drv_d3d12 | Windows CI | N/A | P3 |
| drv_metal | macOS CI | M1/M2 | P3 |

## 10. Open Questions

1. Should we use Mesa's `libgallium` directly, or reimplement?
2. Can we leverage `rusticl` (OpenCL on Mesa) as a shortcut?
3. What's the exact ioctl interface for Intel Xe? (Still evolving)
4. How do we handle GPU resets gracefully?
5. Should drivers be shared libraries or statically linked?

## 11. References

- [Mesa Documentation](https://docs.mesa3d.org/)
- [AMDGPU Kernel Driver](https://dri.freedesktop.org/docs/drm/gpu/amdgpu.html)
- [Intel Xe Driver](https://dri.freedesktop.org/docs/drm/gpu/rfc/xe.html)
- [Nouveau Wiki](https://nouveau.freedesktop.org/)
- [DRM Memory Management](https://dri.freedesktop.org/docs/drm/gpu/drm-mm.html)
- [SPIR-V Specification](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html)
