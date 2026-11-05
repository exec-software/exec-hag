<!-- Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
SPDX-License-Identifier: MIT -->

> **Archived design document.** This describes the unsupported C prototype,
> not the current Rust library or its security guarantees. See the
> [current README](../../README.md) and [security policy](../../SECURITY.md).

# Hag Build System Design

## Document Information
- **Version**: 0.1.0
- **Status**: Draft
- **Date**: 2025-05-27

## 1. Goals

1. **Simple**: One command to build everything
2. **Portable**: Works on Linux now, Windows/macOS later
3. **Flexible**: Debug, release, sanitizers, profiling
4. **Reproducible**: Pin tool versions, lock dependencies
5. **SDK-Friendly**: Produces redistributable SDK in `sdk/`

## 2. Tool Choice: CMake + Ninja

**Why CMake:**
- Industry standard for C/C++ projects
- Excellent IDE support
- Cross-platform
- Package discovery (Find modules)

**Why Ninja:**
- Fast incremental builds
- Used by Chrome, LLVM, Mesa
- CMake generates Ninja by default when available

**Minimum Versions:**
- CMake 3.20
- Ninja 1.10
- GCC 12 / Clang 16

## 3. Directory Layout

```
exec-crypt-hag/
├── CMakeLists.txt              # Root configuration
├── cmake/
│   ├── HagConfig.cmake.in      # For find_package support
│   ├── FindDRM.cmake           # Find libdrm
│   ├── FindOpenBLAS.cmake      # Find BLAS for CPU driver
│   ├── CompilerWarnings.cmake  # Warning flags per compiler
│   ├── Sanitizers.cmake        # ASan/TSan/UBSan options
│   └── Version.cmake           # Version extraction
├── hag/
│   ├── CMakeLists.txt          # Core library build
│   ├── api/                    # Public headers (no build needed)
│   ├── core/
│   │   └── CMakeLists.txt
│   ├── driver/
│   │   ├── CMakeLists.txt
│   │   ├── cpu/
│   │   │   └── CMakeLists.txt
│   │   ├── amdgpu/
│   │   │   └── CMakeLists.txt
│   │   └── intel/
│   │       └── CMakeLists.txt
│   ├── memory/
│   │   └── CMakeLists.txt
│   ├── compute/
│   │   └── CMakeLists.txt
│   └── utils/
│       └── CMakeLists.txt
├── libs/
│   ├── CMakeLists.txt          # Third-party builds
│   ├── unity/                  # Test framework (header-only)
│   ├── spirv-tools/            # SPIR-V tools
│   └── openblas/               # Optional, system preferred
├── tests/
│   ├── CMakeLists.txt
│   └── unit/
│       └── CMakeLists.txt
├── tools/
│   ├── CMakeLists.txt
│   ├── hag-info/               # Device enumeration tool
│   └── hag-bench/              # Benchmark suite
└── sdk/                        # Build output (gitignored)
```

## 4. Build Types

### 4.1 Standard Types
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake -B build -S . -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

### 4.2 Custom Configurations

**Sanitizer Build:**
```bash
cmake -B build -S . -DHAG_SANITIZE=address
cmake -B build -S . -DHAG_SANITIZE=thread
cmake -B build -S . -DHAG_SANITIZE=undefined
```

**Profile Build:**
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=RelWithDebInfo -DHAG_ENABLE_PROFILER=ON
```

**SDK Build:**
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DHAG_BUILD_SDK=ON
# Outputs to sdk/ directory
```

## 5. Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `HAG_BUILD_TESTS` | `ON` | Build test suite |
| `HAG_BUILD_TOOLS` | `ON` | Build CLI tools |
| `HAG_BUILD_DOCS` | `ON` | Build man pages |
| `HAG_BUILD_SDK` | `OFF` | Package SDK artifacts |
| `HAG_BUILD_SHARED` | `ON` | Build shared library (also static) |
| `HAG_ENABLE_ASAN` | `OFF` | Enable AddressSanitizer |
| `HAG_ENABLE_TSAN` | `OFF` | Enable ThreadSanitizer |
| `HAG_ENABLE_UBSAN` | `OFF` | Enable UndefinedBehaviorSanitizer |
| `HAG_ENABLE_PROFILER` | `OFF` | Enable built-in profiler |
| `HAG_DRIVER_CPU` | `ON` | Build CPU fallback driver |
| `HAG_DRIVER_AMDGPU` | `ON` | Build AMD GPU driver |
| `HAG_DRIVER_INTEL` | `ON` | Build Intel GPU driver |
| `HAG_DRIVER_NOUVEAU` | `OFF` | Build Nouveau driver (experimental) |
| `HAG_USE_SYSTEM_OPENBLAS` | `ON` | Use system OpenBLAS (else build from source) |
| `HAG_USE_SYSTEM_SPIRV_TOOLS` | `ON` | Use system SPIRV-Tools |

## 6. Core Library Build

### 6.1 Target: `hag`

The main library is built as:
- `libhag.so` (shared)
- `libhag.a` (static)

**Sources:**
- `hag/core/*.c`
- `hag/driver/*.c`
- `hag/memory/*.c`
- `hag/compute/*.c`
- `hag/utils/*.c`
- Enabled driver subdirectories

**Include Directories:**
- `hag/` (private headers)
- `hag/api/` (public headers)
- `libs/` (third-party)

**Compile Definitions:**
- `HAG_BUILD_DLL` (when building shared)
- `HAG_VERSION_MAJOR=0`
- `HAG_VERSION_MINOR=1`
- `HAG_VERSION_PATCH=0`

**Link Dependencies:**
- `libdrm` (always)
- `libelf` (for shader loading)
- `libm` (math)
- `libpthread` (threading)
- `libdl` (dynamic loading)
- `OpenBLAS` (optional, for CPU driver)

### 6.2 Symbol Visibility

Default symbol visibility is `hidden`. Only symbols marked with `HAG_API` are exported.

```cmake
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)
```

## 7. Test Build

### 7.1 Framework: Unity

[Unity Test Framework](https://github.com/ThrowTheSwitch/Unity) - a proposed C test framework with source and header files.

The archived design proposed upstream Unity. The checked-in `libs/unity/unity.c`
and `libs/unity/unity.h` are Execute Software's minimal placeholders, not that
upstream implementation or a complete test framework.

### 7.2 Test Organization

```
tests/
├── test_main.c           # Unity main + test runner
├── test_api.c            # API contract tests
├── test_memory.c         # Memory allocator tests
├── test_tensor.c         # Tensor operation tests
├── test_driver_cpu.c     # CPU driver tests
└── fixtures/
    └── models/           # Small test models
```

### 7.3 Running Tests

```bash
cmake -B build -S . -DHAG_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### 7.4 Mock Driver

A mock driver is built into the test suite for testing API layer without real hardware:
- `tests/mock/mock_driver.c`
- Records all calls for verification
- Simulates device with configurable properties

## 8. SDK Packaging

When `HAG_BUILD_SDK=ON`:

```bash
cmake --build build --target sdk
```

Produces in `sdk/`:
```
sdk/
├── include/
│   └── hag/
│       └── api.h
├── lib/
│   ├── libhag.so -> libhag.so.0
│   ├── libhag.so.0 -> libhag.so.0.1.0
│   ├── libhag.so.0.1.0
│   └── libhag.a
├── bin/
│   ├── hag-info
│   └── hag-bench
├── share/
│   ├── doc/hag/README.md
│   ├── man/man3/hag.3
│   └── pkgconfig/hag.pc
└── samples/
    ├── hello_compute/
    ├── simple_gemm/
    └── mnist_inference/
```

## 9. CI/CD Pipeline

### 9.1 GitHub Actions Workflow

```yaml
# .github/workflows/ci.yml
name: CI
on: [push, pull_request]
jobs:
  linux-gcc:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - name: Install dependencies
        run: sudo apt-get install -y cmake ninja-build libdrm-dev libelf-dev
      - name: Configure
        run: cmake -B build -G Ninja -S . -DCMAKE_BUILD_TYPE=Release
      - name: Build
        run: cmake --build build
      - name: Test
        run: ctest --test-dir build --output-on-failure

  linux-clang-sanitizer:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - name: Install dependencies
        run: sudo apt-get install -y cmake ninja-build libdrm-dev libelf-dev clang
      - name: Configure
        run: cmake -B build -G Ninja -S . -DCMAKE_C_COMPILER=clang -DHAG_ENABLE_ASAN=ON
      - name: Build
        run: cmake --build build
      - name: Test
        run: ctest --test-dir build --output-on-failure
```

### 9.2 Pre-Commit Hooks

```bash
# .pre-commit-config.yaml (for local development)
repos:
  - repo: local
    hooks:
      - id: clang-format
        name: Format C code
        entry: clang-format -i
        language: system
        files: \.(c|h)$
      - id: cmake-format
        name: Format CMake
        entry: cmake-format -i
        language: system
        files: CMakeLists\.txt$
```

## 10. Cross-Compilation

### 10.1 Android

```bash
cmake -B build-android \
  -S . \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-30
```

### 10.2 Windows (MinGW)

```bash
cmake -B build-windows \
  -S . \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-toolchain.cmake
```

## 11. Open Questions

1. Should we use `vcpkg` or `conan` for dependency management?
2. Do we need a `configure` script wrapper for autotools compatibility?
3. Should the SDK include debug symbols in a separate package?
4. How do we handle kernel cache directory permissions?
5. Should we support `pkg-config` generation?

## 12. Initial CMakeLists.txt

See `hag/CMakeLists.txt` and root `CMakeLists.txt` for initial implementation.
