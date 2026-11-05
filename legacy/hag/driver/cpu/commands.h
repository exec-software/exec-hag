/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#ifndef HAG_DRIVER_CPU_COMMANDS_H
#define HAG_DRIVER_CPU_COMMANDS_H

#include "hag/driver/cpu/cpu_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Internal CPU driver command types */
typedef enum CpuCmdType {
    CPU_CMD_BIND_KERNEL = 0,
    CPU_CMD_DISPATCH,
    CPU_CMD_COPY_BUFFER,
    CPU_CMD_FILL_BUFFER,
    CPU_CMD_BARRIER,
} CpuCmdType;

typedef struct CpuCmd {
    CpuCmdType  type;
    union {
        struct {
            void*       kernelFunc;
            void*       userData;
        } bindKernel;
        struct {
            uint32_t    groupsX;
            uint32_t    groupsY;
            uint32_t    groupsZ;
        } dispatch;
        struct {
            HagDriverBuffer src;
            HagDriverBuffer dst;
            uint64_t    srcOffset;
            uint64_t    dstOffset;
            uint64_t    size;
        } copyBuffer;
        struct {
            HagDriverBuffer buffer;
            uint64_t    offset;
            uint64_t    size;
            uint32_t    data;
        } fillBuffer;
    } params;
} CpuCmd;

typedef struct CpuCmdBuffer {
    uint32_t    flags;
    int         recording;
    int         executable;
    CpuCmd*     commands;
    uint32_t    commandCount;
    uint32_t    commandCapacity;
} CpuCmdBuffer;

/* Execution context during submit */
typedef struct CpuExecutionContext {
    void*       currentKernel;
    void*       currentUserData;
} CpuExecutionContext;

#ifdef __cplusplus
}
#endif

#endif /* HAG_DRIVER_CPU_COMMANDS_H */
