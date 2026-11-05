/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#ifndef HAG_PROFILER_H
#define HAG_PROFILER_H

#include "hag/api/api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Profiling scope types */
typedef enum HagProfilerScopeType {
    HAG_PROFILER_SCOPE_HOST = 0,
    HAG_PROFILER_SCOPE_DEVICE = 1,
    HAG_PROFILER_SCOPE_COPY = 2,
} HagProfilerScopeType;

/* Profiler handle */
typedef struct HagProfiler_T* HagProfiler;

/* Create/destroy profiler */
HAG_API HagResult hagProfilerCreate(HagDevice device, HagProfiler* pProfiler);
HAG_API void hagProfilerDestroy(HagProfiler profiler);

/* Begin/end profiling scope */
HAG_API void hagProfilerBegin(HagProfiler profiler, const char* name, HagProfilerScopeType type);
HAG_API void hagProfilerEnd(HagProfiler profiler);

/* Get results */
HAG_API uint32_t hagProfilerGetEntryCount(HagProfiler profiler);
HAG_API HagResult hagProfilerGetEntry(HagProfiler profiler, uint32_t index, 
    const char** ppName, uint64_t* pStartNs, uint64_t* pEndNs, HagProfilerScopeType* pType);

/* Export to Chrome trace format */
HAG_API HagResult hagProfilerExportChromeTrace(HagProfiler profiler, const char* filePath);

#ifdef __cplusplus
}
#endif

#endif /* HAG_PROFILER_H */
