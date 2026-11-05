<!-- Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
SPDX-License-Identifier: MIT -->

> **Archived design document.** This describes the unsupported C prototype,
> not the current Rust library or its security guarantees. See the
> [current README](../../README.md) and [security policy](../../SECURITY.md).

# Hag Platform Support Plan

## Document Information
- **Version**: 0.1.0
- **Status**: Draft
- **Date**: 2025-05-27

## 1. Platform Tiers

Hag uses a tiered support model similar to Rust and Firefox:

| Tier | Description | CI | Guarantees |
|------|-------------|----|-----------|
| **Tier 1** | Fully supported, primary development target | Every commit | All tests pass, SDK published |
| **Tier 2** | Supported, maintained by community/core team | Nightly | Builds pass, basic tests pass |
| **Tier 3** | Experimental, best effort | Weekly | May break, community patches |
| **Tier 4** | Planned, no current support | None | Roadmap only |

## 2. Tier 1: Linux x86_64

### 2.1 Distributions
- **Ubuntu 24.04 LTS** (primary development)
- **Ubuntu 22.04 LTS** (minimum supported)
- **Fedora 40**
- **Debian 12**

### 2.2 Compilers
- GCC 12+
- Clang 16+

### 2.3 Kernels
- Linux 6.8+ (primary)
- Linux 6.1+ (minimum for amdgpu)
- Linux 5.15+ (minimum for i915)

### 2.4 Windowing
- No window system required (headless compute)
- X11 and Wayland do not affect functionality
- Works on servers without graphics stack

### 2.5 Drivers
- Mesa 24.0+ (primary)
- amdgpu kernel driver
- i915/xe kernel driver
- nouveau kernel driver (experimental)

### 2.6 Required Packages
```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake ninja-build \
    libdrm-dev libelf-dev pkg-config

# Fedora
sudo dnf install gcc cmake ninja-build \
    libdrm-devel elfutils-libelf-devel pkgconfig
```

## 3. Tier 2: Linux aarch64

### 3.1 Hardware
- NVIDIA Jetson (Orin, Xavier)
- Raspberry Pi 5 (with PCIe GPU)
- ARM servers with GPU (e.g., NVIDIA Grace Hopper)

### 3.2 Considerations
- Different page sizes (4K, 16K, 64K)
- Memory model differences
- Less mature GPU drivers

### 3.3 Testing
- Cross-compilation from x86_64
- Native testing on Jetson Orin

## 4. Tier 3: Windows

### 4.1 Challenges
- No DRM/KMS on Windows
- WDDM is closed-source
- Mesa on Windows is incomplete
- Different syscall and memory model

### 4.2 Strategy
**Phase 1: WSL2 (Immediate)**
- Use Linux drivers inside WSL2
- Same code path as native Linux
- Good for development, not deployment

**Phase 2: Native Windows (Medium Term)**
Options:
1. **Vulkan Compute Backend**: Use Vulkan as the driver backend on Windows
   - All GPUs supported via vendor Vulkan drivers
   - We wanted to avoid Vulkan, but it's pragmatic for Windows
   - Implementation: `drv_vulkan` driver module

2. **DirectX 12 Backend**: Use D3D12 compute shaders
   - Native Windows API
   - Good performance
   - More work to implement
   - Implementation: `drv_d3d12` driver module

3. **Mesa on Windows**: Port Mesa drivers to Windows
   - Long-term, aligns with open-source philosophy
   - Significant effort
   - Unclear timeline

**Decision**: Implement Vulkan compute backend first. It's the fastest path to functional Windows support. D3D12 can be added later for better performance.

### 4.3 Build System
- Visual Studio 2022 (primary)
- MinGW-w64 (alternative)
- CMake with MSVC toolchain

### 4.4 Required Changes
- Abstract DRM calls behind platform interface
- Implement Windows-specific memory allocation (VirtualAlloc)
- Threading via Windows threads (or use pthreads-win32)
- Dynamic loading via LoadLibrary
- Path handling (backslash vs forward slash)

## 5. Tier 3: Android

### 5.1 Challenges
- Bionic libc (not glibc)
- Android permissions for GPU access
- APK packaging
- Gralloc instead of GEM

### 5.2 Strategy
- Use DRM/Gralloc for memory management
- Mesa drivers available on Android
- NDK build or CMake with Android toolchain
- Target Android 12+ (API 31)

### 5.3 Hardware Targets
- Qualcomm Snapdragon (Adreno GPU via Freedreno)
- Samsung Exynos (Mali via Panfrost)
- Google Tensor (Mali via Panfrost)

### 5.4 Build
```bash
cmake -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-31
```

## 6. Tier 4: macOS / iOS

### 6.1 Challenges
- No DRM/KMS on macOS
- Apple Silicon uses Metal, not traditional GPU drivers
- Closed ecosystem
- Asahi Linux exists but is Linux-only

### 6.2 Strategy
**Option 1: Metal Backend**
- Use Metal Performance Shaders for compute
- Native Apple API
- Best performance on Apple Silicon
- Implementation: `drv_metal` driver module
- Requires Objective-C++ or Metal-cpp wrapper

**Option 2: MoltenVK / Vulkan Portability**
- Use Vulkan via MoltenVK
- Then use our Vulkan backend (same as Windows)
- Extra layer of indirection
- May not support all compute features

**Decision**: Implement Metal backend directly. Better performance and fewer layers.

### 6.3 Build System
- Xcode project generation via CMake
- Metal shaders compiled at build time
- Universal binaries (x86_64 + arm64)

### 6.4 Hardware Targets
- Mac Studio (M1 Max, M2 Ultra)
- MacBook Pro (M1 Pro, M3 Max)
- iPhone 15 Pro (A17 Pro - future)

## 7. Tier 4: Embedded / Edge

### 7.1 Raspberry Pi
- Pi 4/5 with PCIe GPU
- Native driver for Broadcom V3D (limited compute)
- External GPU via USB/PCIe more practical

### 7.2 Edge AI Devices
- NVIDIA Jetson (Orin Nano, Xavier NX)
- Qualcomm RB3 Gen 2
- Intel NUC with Arc GPU

## 8. Platform Abstraction Layer

### 8.1 Interface

All platform-specific code lives behind `hag/utils/platform.h`:

```c
typedef struct HagPlatformOps {
    /* Filesystem */
    int (*fileExists)(const char* path);
    int64_t (*fileSize)(const char* path);
    int (*fileRead)(const char* path, void* buffer, size_t size);
    int (*fileWrite)(const char* path, const void* buffer, size_t size);
    int (*createDirectory)(const char* path);
    
    /* Memory */
    void* (*alloc)(size_t size);
    void* (*allocAligned)(size_t size, size_t alignment);
    void (*free)(void* ptr);
    void* (*mapFile)(const char* path, size_t* pSize);
    void (*unmapFile)(void* ptr, size_t size);
    
    /* Threads */
    HagThread (*threadCreate)(HagThreadFunc func, void* arg);
    void (*threadJoin)(HagThread thread);
    void (*threadYield)(void);
    HagMutex (*mutexCreate)(void);
    void (*mutexLock)(HagMutex mutex);
    void (*mutexUnlock)(HagMutex mutex);
    void (*mutexDestroy)(HagMutex mutex);
    
    /* Time */
    uint64_t (*timeNanos)(void);
    void (*sleepNanos)(uint64_t ns);
    
    /* Dynamic Loading */
    HagModule (*moduleOpen)(const char* path);
    void (*moduleClose)(HagModule module);
    void* (*moduleSymbol)(HagModule module, const char* name);
    
    /* DRM/GPU */
    int (*enumerateDrmDevices)(HagDrmDeviceInfo* pDevices, int maxDevices);
} HagPlatformOps;
```

### 8.2 Implementations

| Platform | File | Notes |
|----------|------|-------|
| Linux | `hag/utils/platform_linux.c` | Primary implementation |
| Windows | `hag/utils/platform_windows.c` | WIP |
| Android | `hag/utils/platform_android.c` | Extends Linux |
| macOS | `hag/utils/platform_macos.c` | Uses pthreads + Mach |

## 9. Cross-Compilation Matrix

| Host | Target | Status | Notes |
|------|--------|--------|-------|
| Linux x86_64 | Linux x86_64 | Supported | Native |
| Linux x86_64 | Linux aarch64 | Planned | Cross-compile |
| Linux x86_64 | Android arm64 | Planned | Android NDK |
| Linux x86_64 | Windows x86_64 | Planned | MinGW-w64 |
| macOS arm64 | macOS arm64 | Planned | Native |
| macOS arm64 | iOS arm64 | Future | Xcode |
| Windows x86_64 | Windows x86_64 | Future | Native MSVC |

## 10. Container Support

### 10.1 Docker
```dockerfile
FROM ubuntu:24.04
RUN apt-get update && apt-get install -y \
    build-essential cmake ninja-build \
    libdrm-dev libelf-dev
WORKDIR /hag
COPY . .
RUN cmake -B build -G Ninja -S . && cmake --build build
```

### 10.2 GPU Passthrough
- Docker `--device /dev/dri/renderD128`
- Kubernetes device plugin for GPUs
- Podman with `--group-add keep-groups`

## 11. CI/CD Platform Matrix

| Platform | CI Service | Frequency | Notes |
|----------|-----------|-----------|-------|
| Ubuntu 24.04 x86_64 | GitHub Actions | Every commit | Primary |
| Ubuntu 22.04 x86_64 | GitHub Actions | Every commit | Compatibility |
| Fedora 40 x86_64 | GitHub Actions | Nightly | Compatibility |
| Ubuntu 24.04 aarch64 | GitHub Actions | Nightly | QEMU emulation |
| Windows 11 x86_64 | GitHub Actions | Nightly | Vulkan backend |
| macOS 14 arm64 | GitHub Actions | Weekly | Metal backend |

## 12. Open Questions

1. Should we support FreeBSD? (Has DRM, smaller market)
2. What's our ChromeOS strategy? (Android apps run there)
3. Do we need QNX support for automotive? (future)
4. How do we handle different Linux libc implementations (musl)?
5. Should we provide prebuilt binaries or source-only?

## 13. References

- [Rust Platform Support](https://doc.rust-lang.org/nightly/rustc/platform-support.html)
- [Mesa Platform Matrix](https://docs.mesa3d.org/systems.html)
- [DRM Render Nodes](https://dri.freedesktop.org/docs/drm/gpu/drm-render-nodes.html)
- [Android NDK](https://developer.android.com/ndk)
- [Windows WSL GPU](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gpu-compute)
- [Metal Shading Language](https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf)
