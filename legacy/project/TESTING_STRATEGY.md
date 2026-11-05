<!-- Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
SPDX-License-Identifier: MIT -->

> **Archived design document.** This describes the unsupported C prototype,
> not the current Rust library or its security guarantees. See the
> [current README](../../README.md) and [security policy](../../SECURITY.md).

# Hag Testing Strategy

## Document Information
- **Version**: 0.1.0
- **Status**: Draft
- **Date**: 2025-05-27

## 1. Philosophy

Testing is not an afterthought. Every component must be testable, and every feature must have tests before merging.

**Core Principles:**
1. **Test at every level**: Unit, integration, system, performance
2. **Hardware independence**: Tests must run on CI without GPUs
3. **Determinism**: Tests produce the same results every run
4. **Speed**: Full test suite completes in < 5 minutes
5. **Coverage**: Aim for > 90% line coverage in core modules

## 2. Test Pyramid

```
         /\
        /  \      End-to-End Tests (1%)
       /----\     Full model inference
      /      \    
     /--------\   Integration Tests (10%)
    /          \  Multi-component workflows
   /------------\ 
  /              \Unit Tests (89%)
 /----------------\Individual functions
```

## 3. Test Framework

### 3.1 Unity (C Unit Testing)

[Unity](https://github.com/ThrowTheSwitch/Unity) by ThrowTheSwitch.

This was a design proposal. The checked-in `libs/unity` files are Execute
Software's minimal compatibility placeholders, not a vendored Unity release.

**Why Unity:**
- Single C file + header (drops into `libs/`)
- No dynamic allocation
- Works on embedded/constrainted environments
- Simple assertions
- Generates JUnit XML for CI

**Integration:**
```c
/* tests/test_memory.c */
#include "unity.h"
#include "hag/memory/allocator.h"

void setUp(void) { /* before each test */ }
void tearDown(void) { /* after each test */ }

void test_slab_allocator_basic(void) {
    HagSlabAllocator alloc;
    hagSlabInit(&alloc, 64, 16);
    
    void* p1 = hagSlabAlloc(&alloc);
    TEST_ASSERT_NOT_NULL(p1);
    
    void* p2 = hagSlabAlloc(&alloc);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_NOT_EQUAL(p1, p2);
    
    hagSlabFree(&alloc, p1);
    hagSlabFree(&alloc, p2);
    hagSlabDestroy(&alloc);
}
```

### 3.2 Mock Framework

Custom mock system for HagDriverInterface:
- Records call arguments
- Returns configurable results
- Simulates async behavior

```c
/* tests/mock/mock_driver.h */
typedef struct MockDriver {
    HagDriver base;
    int createDeviceCallCount;
    HagResult nextCreateDeviceResult;
} MockDriver;

MockDriver* mockDriverCreate(void);
void mockDriverSetResult(MockDriver* mock, const char* func, HagResult result);
```

## 4. Unit Tests

### 4.1 What to Test

| Module | Test Focus | Target Coverage |
|--------|-----------|----------------|
| `hag/utils/` | Allocators, strings, math | 95% |
| `hag/memory/` | Pool allocators, buffer management | 95% |
| `hag/core/` | Context, device, queue state machines | 90% |
| `hag/api/` | Parameter validation, handle tracking | 90% |
| `hag/compute/` | Graph building, kernel dispatch | 85% |
| `hag/driver/cpu/` | CPU kernel execution, threading | 90% |

### 4.2 Test File Organization

```
tests/
├── test_main.c           # Unity main, test runner
├── test_utils_alloc.c    # Allocator tests
├── test_utils_math.c     # Math utility tests
├── test_memory_pool.c    # Memory pool tests
├── test_memory_buffer.c  # Buffer tests
├── test_api_instance.c   # Instance API tests
├── test_api_device.c     # Device API tests
├── test_api_tensor.c     # Tensor API tests
├── test_core_context.c   # Context tests
├── test_core_scheduler.c # Scheduler tests
├── test_compute_graph.c  # Graph tests
├── test_driver_cpu.c     # CPU driver tests
└── mock/
    ├── mock_driver.c
    └── mock_driver.h
```

## 5. Integration Tests

### 5.1 Component Interactions

Test multiple components together:
- Instance → Device → Queue → Buffer → Kernel → Submit
- Graph recording → optimization → execution
- Multi-device enumeration

### 5.2 Test Scenarios

```c
void test_full_inference_pipeline_cpu(void) {
    /* 1. Create instance */
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    TEST_ASSERT_EQUAL(HAG_SUCCESS, hagCreateInstance(&instInfo, &instance));
    
    /* 2. Create device (CPU) */
    HagDevice device;
    /* ... find CPU device ... */
    TEST_ASSERT_EQUAL(HAG_SUCCESS, hagCreateDevice(instance, &devInfo, &device));
    
    /* 3. Allocate buffers */
    HagBuffer input, weights, output;
    /* ... create buffers ... */
    
    /* 4. Create tensors */
    HagTensor inputTensor, weightTensor, outputTensor;
    /* ... create tensors ... */
    
    /* 5. Load kernel */
    HagKernel kernel;
    /* ... load matmul kernel ... */
    
    /* 6. Record commands */
    HagCmdBuffer cmd;
    hagCreateCmdBuffer(device, &cmdInfo, &cmd);
    hagBeginCmdBuffer(cmd, 0);
    hagCmdBindKernel(cmd, kernel);
    hagCmdBindTensor(cmd, 0, inputTensor);
    hagCmdBindTensor(cmd, 1, weightTensor);
    hagCmdBindTensor(cmd, 2, outputTensor);
    hagCmdDispatch(cmd, 1, 1, 1);
    hagEndCmdBuffer(cmd);
    
    /* 7. Submit and wait */
    HagEvent fence;
    hagCreateEvent(device, &fence);
    HagSubmitInfo submit = {0};
    submit.cmdBufferCount = 1;
    submit.pCmdBuffers = &cmd;
    hagQueueSubmit(queue, 1, &submit, fence);
    hagWaitForEvents(1, &fence, UINT64_MAX);
    
    /* 8. Verify results */
    /* ... map output and check values ... */
    
    /* 9. Cleanup */
    /* ... destroy everything ... */
}
```

## 6. System Tests

### 6.1 End-to-End Workloads

Run complete ML workloads:
- MNIST classification
- Simple GPT-2 inference (small model)
- LLaMA 7B quantized inference

### 6.2 Hardware Tests

Tests that require actual GPU hardware. Run manually or in dedicated CI runners:

```
tests/hardware/
├── test_amd_gpu_basic.c      # AMDGPU smoke tests
├── test_intel_gpu_basic.c    # Intel GPU smoke tests
├── test_memory_bandwidth.c   # Bandwidth benchmarks
└── test_kernel_performance.c # Kernel launch overhead
```

These are NOT run in standard CI but in nightly/weekly runs.

## 7. Performance Tests

### 7.1 Microbenchmarks

Measure specific operations:
- Kernel launch overhead (μs)
- Memory allocation latency
- Host-to-device copy bandwidth
- Device-to-device copy bandwidth
- Tensor shape manipulation cost

### 7.2 Macrobenchmarks

Full operation benchmarks:
- FP16 GEMM (various sizes)
- Attention forward pass
- Convolution forward pass
- Element-wise fused operations

### 7.3 Comparison Benchmarks

Compare against baselines:
- Same operation in CUDA (NVIDIA hardware)
- Same operation in ROCm (AMD hardware)
- Same operation in oneAPI (Intel hardware)

## 8. Correctness Tests

### 8.1 Numerical Accuracy

Verify output matches reference implementation:
- Element-wise: vs CPU reference
- GEMM: vs BLAS reference
- Conv: vs naive CPU implementation
- Full model: vs PyTorch CPU output

### 8.2 Tolerance Levels

| Operation | FP32 Tolerance | FP16 Tolerance |
|-----------|---------------|----------------|
| Element-wise | 1e-6 | 1e-3 |
| GEMM | 1e-4 | 1e-2 |
| Conv | 1e-4 | 1e-2 |
| Attention | 1e-4 | 1e-2 |
| Full model | 1e-3 | 1e-1 |

## 9. Conformance Tests

### 9.1 API Contract Tests

Verify API behavior matches specification:
- Invalid arguments return correct error codes
- NULL pointers handled gracefully
- Double-free is prevented
- Out-of-bounds access is caught
- Handle uniqueness is guaranteed

### 9.2 State Machine Tests

Verify lifecycle state machines:
- Command buffer: Initial → Recording → Executable → Pending → Executable/Invalid
- Event: Unsignaled → Signaled → Reset
- Device: Created → Idle → Lost → Destroyed

## 10. Stress Tests

### 10.1 Long-Running Tests

Run for extended periods:
- 1000 sequential kernel launches
- 10000 memory allocations/deallocations
- 24-hour inference loop

### 10.2 Resource Exhaustion

Test graceful failure:
- Allocate until OOM
- Create handles until limit
- Submit work until queue full

### 10.3 Concurrency

Test thread safety:
- Multiple threads creating/destroying resources
- Multiple threads submitting to same queue
- Multiple threads using same allocator

## 11. Fuzzing

### 11.1 API Fuzzing

Use libFuzzer or AFL to fuzz API calls:
- Random sequences of API calls
- Random parameter values
- Random buffer sizes

### 11.2 Shader Fuzzing

Fuzz SPIR-V inputs:
- Random SPIR-V binaries
- Mutate valid SPIR-V
- Verify compiler doesn't crash

## 12. Test Infrastructure

### 12.1 Test Data

Small test models stored in `tests/fixtures/`:
```
tests/fixtures/
├── mnist/
│   ├── model.bin
│   └── input.bin
├── gpt2_tiny/
│   ├── weights_q4_0.bin
│   └── config.json
└── kernels/
    ├── add.spv
    ├── matmul.spv
    └── conv2d.spv
```

### 12.2 CI Configuration

```yaml
# .github/workflows/test.yml
jobs:
  unit-tests:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - name: Build
        run: |
          cmake -B build -S . -DHAG_BUILD_TESTS=ON
          cmake --build build
      - name: Unit Tests
        run: ctest --test-dir build -R unit --output-on-failure
      - name: Integration Tests
        run: ctest --test-dir build -R integration --output-on-failure
      - name: Coverage
        run: |
          cmake -B build -S . -DHAG_BUILD_TESTS=ON -DCMAKE_C_FLAGS="--coverage"
          cmake --build build
          ctest --test-dir build
          gcovr --xml coverage.xml
      - uses: codecov/codecov-action@v4
        with: { files: coverage.xml }
```

## 13. Test Quality Metrics

| Metric | Target | Measurement |
|--------|--------|-------------|
| Line Coverage | > 90% | gcov + gcovr |
| Branch Coverage | > 80% | gcov + gcovr |
| Test Suite Time | < 5 min | CI timer |
| Flaky Tests | 0 | Track in CI |
| Open Bugs | < 10 | Issue tracker |

## 14. Open Questions

1. Should we use CMocka instead of Unity?
2. Do we need property-based testing (QuickCheck style)?
3. How do we test GPU reset recovery?
4. Should we integrate with sanitizer builds in CI?
5. Do we need dedicated performance regression CI?
