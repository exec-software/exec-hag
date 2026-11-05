<!-- Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
SPDX-License-Identifier: MIT -->

> **Archived design document.** This describes the unsupported C prototype,
> not the current Rust library or its security guarantees. See the
> [current README](../../README.md) and [security policy](../../SECURITY.md).

# Hag Security Model

## Document Information
- **Version**: 0.1.0
- **Status**: Draft
- **Date**: 2025-05-27

## 1. Threat Model

### 1.1 Scope

Hag operates in user space and interacts with GPU hardware via kernel drivers. We consider:
- **In scope**: API misuse, memory safety, kernel driver interaction, shader validation
- **Out of scope**: Physical attacks, OS kernel vulnerabilities, hardware flaws

### 1.2 Threat Actors

| Actor | Capability | Motivation | Risk |
|-------|-----------|-----------|------|
| Malicious Application | User-space code | Escape sandbox, steal data | High |
| Compromised ML Model | Shader code via model | GPU exploit, data exfil | Medium |
| Local User | Same machine | Read other users' GPU data | Medium |
| Network Attacker | Remote | Limited (no network stack) | Low |

### 1.3 Assets

| Asset | Value | Protection |
|-------|-------|-----------|
| GPU Memory | Model weights, activations | Process isolation |
| Host Memory | Staging buffers, caches | ASan, guard pages |
| Kernel Code | Shader binaries | Validation, sandbox |
| Device State | GPU registers, contexts | DRM permissions |

## 2. Design Principles

1. **Principle of Least Privilege**: No root required, runs as regular user
2. **Defense in Depth**: Multiple layers of protection
3. **Fail Secure**: Errors result in denial of service, not privilege escalation
4. **Explicit Trust**: No implicit trust of user-provided shaders
5. **Auditability**: All security-relevant events are loggable

## 3. Process Isolation

### 3.1 DRM Render Nodes

Hag uses DRM render nodes (`/dev/dri/renderD*`), not primary nodes:
- Render nodes: no display control, no privileged operations
- Each process has independent GPU context
- GPU memory isolated between processes (hardware-enforced)

### 3.2 No Elevated Privileges

```
┌─────────────────┐
│   Application   │  User process
├─────────────────┤
│   Hag Library   │  User process
├─────────────────┤
│  libdrm, ioctl  │  Kernel interface
├─────────────────┤
│  amdgpu/i915    │  Kernel driver (ring 0)
└─────────────────┘
```

At no point does Hag require root, setuid, or capabilities.

### 3.3 Seccomp (Future)

Consider seccomp-bpf to restrict syscalls:
- Allow: `ioctl` (DRM only), `mmap`, `read`, `write`
- Deny: `open` (except known paths), `socket`, `execve`

## 4. Memory Safety

### 4.1 Buffer Bounds

Debug builds validate all buffer accesses:
```c
#ifdef HAG_ENABLE_VALIDATION
    if (offset + size > buffer->size) {
        hagLogError("Buffer overflow: %zu > %zu", offset + size, buffer->size);
        return HAG_ERROR_INVALID_ARGUMENT;
    }
#endif
```

### 4.2 Use-After-Free

Handles are validated before use:
```c
typedef struct HagHandleEntry {
    uint32_t magic;      // HAG_HANDLE_MAGIC
    uint32_t generation; // Incremented on reuse
    void* object;
} HagHandleEntry;

HagResult hagHandleResolve(HagContext* ctx, uint64_t handle, void** ppObject) {
    uint32_t index = handle >> 32;
    uint32_t gen = handle & 0xFFFFFFFF;
    
    if (index >= ctx->handleCapacity) return HAG_ERROR_INVALID_ARGUMENT;
    if (ctx->handles[index].magic != HAG_HANDLE_MAGIC) return HAG_ERROR_INVALID_ARGUMENT;
    if (ctx->handles[index].generation != gen) return HAG_ERROR_INVALID_ARGUMENT;
    
    *ppObject = ctx->handles[index].object;
    return HAG_SUCCESS;
}
```

### 4.3 Address Sanitizer

ASan catches:
- Buffer overflows
- Use-after-free
- Memory leaks
- Stack buffer overflows

Enabled via: `-DHAG_ENABLE_ASAN=ON`

### 4.4 Thread Sanitizer

TSan catches:
- Data races
- Lock order inversions
- Thread leaks

Enabled via: `-DHAG_ENABLE_TSAN=ON`

## 5. Shader Safety

### 5.1 SPIR-V Validation

All user-provided SPIR-V is validated before compilation:
```c
// libs/SPIRV-Tools
spv_context_t ctx = spvContextCreate(SPV_ENV_VULKAN_1_3);
spv_diagnostic_t diagnostic = NULL;

spv_result_t result = spvValidate(ctx, &binary, &diagnostic);
if (result != SPV_SUCCESS) {
    hagLogError("Invalid SPIR-V: %s", diagnostic->error);
    return HAG_ERROR_SHADER_COMPILE;
}
```

Validation checks:
- Valid SPIR-V structure
- No undefined instructions
- Capabilities declared
- No infinite loops
- No out-of-bounds literal indices

### 5.2 Capability Restrictions

Only allow safe SPIR-V capabilities:
```c
static const spv_capability_t g_allowedCapabilities[] = {
    SpvCapabilityShader,
    SpvCapabilityFloat16,
    SpvCapabilityInt8,
    SpvCapabilityInt16,
    SpvCapabilityStorageBuffer16BitAccess,
    SpvCapabilityGroupNonUniform,
    SpvCapabilityGroupNonUniformArithmetic,
    SpvCapabilityVariablePointers,
    SpvCapabilityVariablePointersStorageBuffer,
    // Explicitly denied:
    // SpvCapabilityKernel,          // OpenCL kernels (different model)
    // SpvCapabilityAddresses,       // Physical addressing
    // SpvCapabilityLinker,          // Dynamic linking
};
```

### 5.3 No JIT Compilation from Untrusted Sources

Default mode:
- Pre-compiled kernels only
- No runtime compilation of user-provided source code
- Kernel cache is read-only after initial population

Opt-in unsafe mode (requires explicit flag):
```c
HagInstanceCreateInfo info = {
    .flags = HAG_INSTANCE_CREATE_ALLOW_JIT_COMPILATION,
};
```

## 6. Denial of Service Protection

### 6.1 Timeouts

All GPU operations have timeouts:
```c
// Default: 30 seconds
#define HAG_DEFAULT_TIMEOUT_NS (30ULL * 1000000000)

HagResult hagWaitForEvents(uint32_t count, HagEvent* events, uint64_t timeoutNs);
```

If timeout expires:
1. Return `HAG_ERROR_TIMEOUT`
2. Attempt GPU reset
3. If reset fails, mark device as lost

### 6.2 Watchdog

Background thread monitors GPU health:
```c
void* watchdogThread(void* arg) {
    while (running) {
        sleep(1);
        for each active context {
            if (context->lastProgress + WATCHDOG_TIMEOUT < now) {
                hagLogWarning("GPU watchdog timeout, resetting");
                hagResetDevice(context->device);
            }
        }
    }
}
```

### 6.3 Resource Limits

Prevent resource exhaustion:
```c
typedef struct HagResourceLimits {
    uint64_t maxMemoryAllocation;
    uint32_t maxBufferCount;
    uint32_t maxKernelCount;
    uint32_t maxQueueCount;
    uint32_t maxPendingSubmissions;
} HagResourceLimits;
```

## 7. Information Disclosure

### 7.1 GPU Memory Zeroing

Allocated GPU memory may contain data from previous process:
```c
HagResult hagCreateBuffer(...) {
    // ... allocate ...
    #ifdef HAG_SECURE_ZERO
        // Zero memory on allocation (slower, secure)
        hagCmdFillBuffer(cmd, buffer, 0, size, 0);
        hagQueueSubmit(queue, ...);
        hagQueueWaitIdle(queue);
    #endif
}
```

Default: No zeroing (performance)
Opt-in: `HAG_BUFFER_CREATE_ZERO_MEMORY` flag

### 7.2 Kernel Cache Isolation

Kernel cache is per-user:
```
~/.cache/hag/kernels/
├── user_a/   (0700 permissions)
└── user_b/   (0700 permissions)
```

### 7.3 Error Messages

Error messages reveal minimal information:
```c
// Good
return HAG_ERROR_INVALID_ARGUMENT;  // Generic

// Bad (in production)
// return HAG_ERROR_INVALID_ARGUMENT;
// hagLogDebug("Buffer %p overflow at offset %zu", buffer, offset);  // Only in debug
```

## 8. Secure Defaults

### 8.1 Default Flags

All create functions default to safe settings:
```c
// Default: no unsafe features
HagInstanceCreateInfo info = {0};  // All flags zero = safe

// Must explicitly opt into:
// - JIT compilation
// - Untrusted shaders
// - Shared contexts
// - Debug markers in production
```

### 8.2 Debug vs Release

| Feature | Debug | Release |
|---------|-------|---------|
| Validation layers | On | Off |
| Bounds checking | On | Off |
| ASan/TSan | Available | Not used |
| Detailed errors | Yes | Generic codes |
| Debug markers | Yes | Stripped |
| Memory zeroing | Optional | Off |

## 9. Audit & Logging

### 9.1 Security Events

Logged events:
- Device creation/destruction
- Kernel loading (hash of binary)
- Memory allocation sizes
- Timeout/reset events
- Validation failures

### 9.2 Log Levels

```c
typedef enum HagLogLevel {
    HAG_LOG_LEVEL_ERROR = 0,
    HAG_LOG_LEVEL_WARNING = 1,
    HAG_LOG_LEVEL_INFO = 2,
    HAG_LOG_LEVEL_DEBUG = 3,
    HAG_LOG_LEVEL_TRACE = 4,
} HagLogLevel;
```

Default in release: `HAG_LOG_LEVEL_WARNING`

## 10. Vulnerability Response

### 10.1 Reporting

Security issues reported to: `security@executesoftware.example`
- PGP key available
- 48-hour acknowledgment
- 90-day disclosure timeline

### 10.2 Response Process

1. Receive report
2. Verify vulnerability
3. Develop fix
4. Test fix
5. Release security advisory
6. Credit reporter (with permission)

### 10.3 Security Releases

- CVE assignment for qualifying issues
- Patch releases (0.1.x) for security fixes
- No breaking changes in security patches

## 11. Compliance

### 11.1 Standards

- Follows CII Best Practices
- OpenSSF Scorecard monitoring
- Reproducible builds where possible

### 11.2 Dependencies

All dependencies tracked in `thirdparty.md`:
- License compatibility
- Security advisories monitoring
- Pin to specific versions

## 12. Open Questions

1. Should we implement a capability-based security model?
2. How do we handle side-channel attacks (power analysis, timing)?
3. Do we need formal verification for critical paths?
4. Should we support confidential computing (AMD SEV-SNP, Intel TDX)?
5. How do we sandbox user-provided kernels effectively?
6. Do we need a bug bounty program?

## 13. References

- [DRM Security](https://dri.freedesktop.org/docs/drm/gpu/security.html)
- [Vulkan Validation Layers](https://github.com/KhronosGroup/Vulkan-ValidationLayers)
- [OpenSSF Scorecard](https://github.com/ossf/scorecard)
- [CII Best Practices](https://bestpractices.coreinfrastructure.org/)
- [Rust Secure Code Guidelines](https://secure-code.rust-lang.org/)
