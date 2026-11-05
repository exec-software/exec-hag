<!-- Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
SPDX-License-Identifier: MIT -->

> **Archived design document.** This describes the unsupported C prototype,
> not the current Rust library or its security guarantees. See the
> [current README](../../README.md) and [security policy](../../SECURITY.md).

# Hag Compute Model

## Document Information
- **Version**: 0.1.0
- **Status**: Draft
- **Date**: 2025-05-27

## 1. Goals

1. **Explicit**: Developer controls when work happens
2. **Asynchronous**: GPU work overlaps with CPU work
3. **Batched**: Multiple operations submitted together
4. **Fused**: Common patterns combined into single kernel
5. **Optimized**: Hardware-specific tuning without API changes

## 2. Execution Model

### 2.1 Queues

Compute work is submitted to queues. Each device has one or more queues.

```
Device
├── Compute Queue 0  ← Primary compute work
├── Compute Queue 1  ← Async compute (background tasks)
├── Transfer Queue 0 ← Host <-> device copies
└── Transfer Queue 1 ← Peer-to-peer copies
```

Queue types:
- `HAG_QUEUE_COMPUTE` - Shader execution
- `HAG_QUEUE_TRANSFER` - Memory copy/fill
- `HAG_QUEUE_SPARSE` - Sparse memory operations

### 2.2 Command Buffers

Work is recorded into command buffers, then submitted:

```
┌─────────────────┐
│  Begin CmdBuf   │
├─────────────────┤
│  Bind Kernel    │
├─────────────────┤
│  Bind Tensor 0  │
│  Bind Tensor 1  │
│  Bind Tensor 2  │
├─────────────────┤
│  Dispatch       │
├─────────────────┤
│  Barrier        │
├─────────────────┤
│  Bind Kernel    │
│  Bind Tensors   │
│  Dispatch       │
├─────────────────┤
│  End CmdBuf     │
└─────────────────┘
         │
         ▼
    ┌─────────┐
    │ Submit  │ ──→ Queue
    └─────────┘
         │
         ▼
    ┌─────────┐
    │  Fence  │
    └─────────┘
```

### 2.3 Command Buffer Lifecycle

```
Initial ──→ Recording ──→ Executable ──→ Pending ──→ Executable
              │                │              │            │
              │                │              └──→ Invalid (if one-time)
              │                │
              └──→ Reset ──────┘
```

States:
- **Initial**: Allocated, no commands
- **Recording**: Commands being recorded
- **Executable**: Ready to submit
- **Pending**: Submitted to queue, executing
- **Invalid**: Error occurred or one-time buffer completed

## 3. Kernel Model

### 3.1 Kernel Definition

A kernel is a compiled GPU program with:
- Entry point function
- Uniform/constant arguments
- Tensor bindings (read, write, read-write)
- Workgroup dimensions

### 3.2 Work Hierarchy

```
Grid (Dispatch)
├── Workgroup (0,0,0)
│   ├── Wave/Warp (0)
│   │   ├── Thread (0,0,0)
│   │   ├── Thread (1,0,0)
│   │   └── ... (waveSize threads)
│   ├── Wave/Warp (1)
│   └── ...
├── Workgroup (1,0,0)
└── ... (dispatchX * dispatchY * dispatchZ workgroups)
```

### 3.3 Dispatch Dimensions

```c
// 1D dispatch
typedef struct HagDispatch1D {
    uint32_t groupCountX;
    uint32_t groupSizeX;  // Optional: 0 = driver default
} HagDispatch1D;

// 2D dispatch
typedef struct HagDispatch2D {
    uint32_t groupCountX, groupCountY;
    uint32_t groupSizeX, groupSizeY;
} HagDispatch2D;

// 3D dispatch
typedef struct HagDispatch3D {
    uint32_t groupCountX, groupCountY, groupCountZ;
    uint32_t groupSizeX, groupSizeY, groupSizeZ;
} HagDispatch3D;
```

### 3.4 Automatic Workgroup Size

If workgroup size is 0, the driver selects optimal size based on:
- Kernel register pressure
- GPU architecture
- Tensor sizes

```c
// Let driver decide
hagCmdDispatch(cmd, groupsX, groupsY, groupsZ);  // groupSize = 0 → auto
```

## 4. Execution Graphs

### 4.1 Why Graphs?

For inference, the same operations run repeatedly:
- Recording command buffers every iteration wastes CPU time
- Graphs record once, replay many times
- Graphs enable cross-kernel optimizations (fusion, aliasing)

### 4.2 Graph Structure

```
┌─────────────────────────────────────────┐
│              Graph                      │
│  ┌─────┐     ┌─────┐     ┌─────┐      │
│  │Node0│────→│Node1│────→│Node2│      │
│  └─────┘     └──┬──┘     └─────┘      │
│                 │                       │
│                 ↓                       │
│              ┌─────┐                    │
│              │Node3│                    │
│              └─────┘                    │
└─────────────────────────────────────────┘
```

Each node is:
- Kernel dispatch
- Memory copy
- Barrier
- Conditional (future)

### 4.3 Graph Recording

```c
// Build graph
HagGraphNode nodes[] = {
    {
        .type = HAG_GRAPH_NODE_KERNEL,
        .nodeIndex = 0,
        .dependencyCount = 0,
        .params.kernel = {
            .kernel = matmulKernel,
            .groupCountX = M / 64, .groupCountY = N / 64,
        }
    },
    {
        .type = HAG_GRAPH_NODE_KERNEL,
        .nodeIndex = 1,
        .dependencyCount = 1,
        .pDependencies = (uint32_t[]){0},
        .params.kernel = {
            .kernel = addBiasKernel,
            .groupCountX = N / 256,
        }
    },
};

HagGraphCreateInfo info = {
    .nodeCount = 2,
    .pNodes = nodes,
};
hagCreateGraph(device, &info, &graph);

// Execute
HagSubmitInfo submit = {.pGraphs = &graph, .graphCount = 1};
hagQueueSubmit(queue, 1, &submit, fence);
```

### 4.4 Graph Optimization

When creating a graph, Hag optimizes:
1. **Kernel Fusion**: Merge compatible nodes
   ```
   matmul → add_bias → relu  →  fused_matmul_add_bias_relu
   ```
2. **Barrier Elimination**: Remove unnecessary barriers
3. **Memory Aliasing**: Reuse buffers for temporaries
4. **Workgroup Tuning**: Adjust sizes for entire graph
5. **Precompile**: Compile all kernels upfront

### 4.5 Graph Update

For dynamic shapes or weights:
```c
// Update tensor binding in graph
hagUpdateGraphTensor(graph, nodeIndex, binding, newTensor);

// Update without re-recording entire graph
```

## 5. Built-in Kernels

### 5.1 Math Primitives

Hag provides optimized built-in kernels:

| Kernel | Description | Variants |
|--------|-------------|----------|
| `hag_op_add` | Element-wise addition | F32, F16, BF16, I32 |
| `hag_op_mul` | Element-wise multiply | F32, F16, BF16, I32 |
| `hag_op_matmul` | Matrix multiply | F32, F16, BF16, INT8 |
| `hag_op_conv2d` | 2D convolution | F32, F16, BF16 |
| `hag_op_attention` | Multi-head attention | F32, F16, BF16 |
| `hag_op_softmax` | Softmax | F32, F16 |
| `hag_op_layernorm` | Layer normalization | F32, F16 |
| `hag_op_relu` | ReLU activation | All float |
| `hag_op_gelu` | GELU activation | F32, F16 |
| `hag_op_silu` | SiLU activation | F32, F16 |
| `hag_op_quantize` | Quantize to INT8/INT4 | F32→INT8, F16→INT4 |
| `hag_op_dequantize` | Dequantize | INT8→F32, INT4→F16 |

### 5.2 Kernel Selection

Hag selects the best implementation based on:
- GPU vendor and architecture
- Tensor shapes and data types
- Memory layout
- Quantization format

```c
// Automatic selection
hagOpMatmul(queue, A, B, C, HAG_DTYPE_FLOAT16);  // Uses best kernel

// Explicit selection (advanced)
hagOpMatmulEx(queue, A, B, C, HAG_KERNEL_VARIANT_AMD_GFX1030_WMMA);
```

## 6. Synchronization

### 6.1 Explicit Synchronization

Hag uses explicit synchronization like Vulkan, not implicit like CUDA:

```c
// Within command buffer
hagCmdBarrier(cmd, HAG_PIPELINE_STAGE_COMPUTE, HAG_PIPELINE_STAGE_COMPUTE);

// Between submissions
HagSemaphore sem;
hagCreateSemaphore(device, &semInfo, &sem);

// Submit 1 signals
HagSubmitInfo submit1 = {
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &sem,
};
hagQueueSubmit(queue, 1, &submit1, NULL);

// Submit 2 waits
HagSubmitInfo submit2 = {
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &sem,
};
hagQueueSubmit(queue, 1, &submit2, fence);
```

### 6.2 Pipeline Stages

```c
typedef enum HagPipelineStage {
    HAG_PIPELINE_STAGE_TOP = 0x0001,
    HAG_PIPELINE_STAGE_TRANSFER = 0x0002,
    HAG_PIPELINE_STAGE_COMPUTE = 0x0004,
    HAG_PIPELINE_STAGE_BOTTOM = 0x0008,
} HagPipelineStage;
```

### 6.3 Memory Barriers

```c
// Ensure writes to buffer are visible to subsequent reads
hagCmdBarrierBuffer(cmd,
    HAG_PIPELINE_STAGE_COMPUTE,      // After compute writes
    HAG_PIPELINE_STAGE_COMPUTE,      // Before compute reads
    buffer,                          // Buffer to barrier
    HAG_ACCESS_WRITE,                // Previous access
    HAG_ACCESS_READ);                // Next access
```

### 6.4 Timeline Semaphores

For CPU-GPU coordination and multi-queue work:

```c
HagSemaphoreCreateInfo semInfo = {
    .type = HAG_SEMAPHORE_TIMELINE,
    .initialValue = 0,
};
hagCreateSemaphore(device, &semInfo, &timeline);

// GPU signals value 5
hagQueueSubmit(queue, 1, &(HagSubmitInfo){
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &timeline,
    .pSignalValues = (uint64_t[]){5},
}, NULL);

// CPU waits for value 5
hagWaitSemaphores(device, 1, &timeline, (uint64_t[]){5}, UINT64_MAX);
```

## 7. Kernel Language

### 7.1 HGSL (Hag GPU Shading Language)

Future language inspired by CUDA/HLSL but targeting SPIR-V:

```hlsl
// Example HGSL kernel (future)
kernel void matmul_fp16(
    tensor<half> A : binding(0),
    tensor<half> B : binding(1),
    tensor<half> C : binding(2),
    uniform uint M : push_constant,
    uniform uint N : push_constant,
    uniform uint K : push_constant
)
{
    uint row = global_id.x;
    uint col = global_id.y;
    
    half sum = 0;
    for (uint k = 0; k < K; k++) {
        sum += A[row * K + k] * B[k * N + col];
    }
    C[row * N + col] = sum;
}
```

### 7.2 Phase 1: Raw SPIR-V

For initial implementation, kernels are hand-written SPIR-V:
- Assembly or generated via `spriv-as`
- Stored in `tests/fixtures/kernels/`
- Loaded at runtime via `hagCreateKernel`

### 7.3 Phase 2: High-Level Language

Later, add HGSL compiler:
```
HGSL source
    ↓
HGSL Frontend (parser, type checker)
    ↓
Hag IR (custom SSA IR)
    ↓
SPIR-V Backend
    ↓
SPIR-V binary
```

## 8. Indirect Dispatch

### 8.1 Use Case

Work dimensions computed by GPU:
- Dynamic shape inference
- Variable-length sequences
- Sparse operations

### 8.2 API

```c
// Buffer contains {groupCountX, groupCountY, groupCountZ}
HagBuffer indirectBuffer;
// ... fill indirectBuffer ...

hagCmdDispatchIndirect(cmd, kernel, indirectBuffer, 0);
```

### 8.3 Multi-Draw Indirect (Future)

Dispatch multiple kernels with one call:
```c
hagCmdDispatchIndirectMulti(cmd, indirectBuffer, count, stride);
```

## 9. Cooperative Groups (Future)

### 9.1 Synchronization Within Kernel

```hlsl
// Future HGSL
void kernel reduce(...) {
    local half scratch[256];
    
    uint tid = local_id.x;
    scratch[tid] = input[global_id.x];
    
    workgroup_barrier();  // Synchronize within workgroup
    
    // Tree reduction
    for (uint stride = 128; stride > 0; stride >>= 1) {
        if (tid < stride) {
            scratch[tid] += scratch[tid + stride];
        }
        workgroup_barrier();
    }
    
    if (tid == 0) output[group_id.x] = scratch[0];
}
```

## 10. Performance Considerations

### 10.1 Occupancy

Maximize GPU utilization:
- Low register pressure → more waves in flight
- Shared memory usage → fewer workgroups per CU
- Wavefront size awareness (64 on AMD, 32 on NVIDIA)

### 10.2 Memory Coalescing

Threads should access consecutive memory:
```
Good:  thread 0 reads addr+0, thread 1 reads addr+4, ...
Bad:   thread 0 reads addr+0, thread 1 reads addr+1024, ...
```

### 10.3 Tensor Core / Matrix Core Usage

On supported hardware, use hardware-accelerated matrix multiply:
- AMD: WMMA (RDNA3+) or MFMA (CDNA)
- Intel: DPAS (Xe)
- NVIDIA: WMMA / Tensor Cores

## 11. Profiling & Debugging

### 11.1 Timestamp Queries

```c
HagProfiler profiler;
hagProfilerCreate(device, &profiler);

hagProfilerBegin(profiler, cmd, "matmul_layer_0");
hagCmdDispatch(cmd, ...);
hagProfilerEnd(profiler, cmd, "matmul_layer_0");

uint64_t nanos;
hagProfilerGetResult(profiler, "matmul_layer_0", &nanos);
```

### 11.2 Debug Markers

```c
hagCmdDebugMarkerBegin(cmd, "Attention Block 0");
// ... attention kernels ...
hagCmdDebugMarkerEnd(cmd);
```

Exported to:
- RenderDoc (if debugging)
- Chrome trace format
- Custom profiler UI

## 12. Error Handling

### 12.1 GPU Hangs

If GPU hangs:
1. Detect via fence timeout
2. Attempt GPU reset (amdgpu: `amdgpu_reset`)
3. Return `HAG_ERROR_DEVICE_LOST`
4. Application must recreate device

### 12.2 Validation Layer

Debug builds include validation:
- Buffer bounds checking
- Tensor shape mismatch detection
- Synchronization hazard detection
- Memory leak tracking

## 13. Open Questions

1. Should we support persistent kernels (run until stopped)?
2. How do we handle workgroup-level reductions efficiently?
3. Should graphs support conditional execution?
4. Do we need a task graph API (like Taskflow) on top?
5. How do we handle subgroups/warps in a vendor-neutral way?
6. Should we support inline SPIR-V assembly?

## 14. References

- [CUDA Programming Guide](https://docs.nvidia.com/cuda/cuda-c-programming-guide/)
- [Vulkan Command Buffers](https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html)
- [ROCm Kernel Language](https://rocm.docs.amd.com/projects/HIP/en/latest/reference/kernel_language.html)
- [OpenCL Execution Model](https://www.khronos.org/opencl/)
- [SPIR-V Execution and Memory Model](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html#_execution_and_memory_model)
