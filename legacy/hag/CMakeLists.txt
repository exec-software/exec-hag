# Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
# SPDX-License-Identifier: MIT

# Hag library build

set(HAG_SOURCES
    core/api_utils.c
    core/instance.c
    core/api_impl.c
    core/tensor.c
    core/graph.c
    core/profiler.c
    driver/registry.c
    driver/cpu/cpu_driver.c
    memory/allocator.c
    op/tensor_ops.c
    utils/platform_linux.c
)

if(HAG_DRIVER_AMDGPU AND DRM_FOUND)
    list(APPEND HAG_SOURCES driver/amdgpu/amdgpu_driver.c)
endif()

set(HAG_PUBLIC_HEADERS
    api/api.h
)

# Core library
add_library(hag_core OBJECT ${HAG_SOURCES})
target_include_directories(hag_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/hag/api>
    PRIVATE
        ${DRM_INCLUDE_DIRS}
)
target_link_libraries(hag_core
    PRIVATE
        ${DRM_LIBRARIES}
        ${ELF_LIBRARY}
        m
        pthread
        dl
)
target_compile_definitions(hag_core
    PRIVATE
        HAG_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        HAG_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        HAG_VERSION_PATCH=${PROJECT_VERSION_PATCH}
        $<$<BOOL:${DRM_FOUND}>:HAG_HAS_LIBDRM>
)

# Shared library
if(HAG_BUILD_SHARED)
    add_library(hag SHARED $<TARGET_OBJECTS:hag_core>)
    target_include_directories(hag
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/hag/api>
        PRIVATE
            ${DRM_INCLUDE_DIRS}
    )
    target_link_libraries(hag
        PRIVATE
            ${DRM_LIBRARIES}
            ${ELF_LIBRARY}
            m
            pthread
            dl
    )
    target_compile_definitions(hag
        PRIVATE
            HAG_BUILD_DLL
    )
    set_target_properties(hag PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
endif()

# Static library
add_library(hag_static STATIC $<TARGET_OBJECTS:hag_core>)
target_include_directories(hag_static
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/hag/api>
    PRIVATE
        ${DRM_INCLUDE_DIRS}
)
target_link_libraries(hag_static
    PRIVATE
        ${DRM_LIBRARIES}
        ${ELF_LIBRARY}
        m
        pthread
        dl
)

# Drivers (will be added as they are implemented)
# if(HAG_DRIVER_CPU)
#     add_subdirectory(driver/cpu)
# endif()
# if(HAG_DRIVER_AMDGPU)
#     add_subdirectory(driver/amdgpu)
# endif()
# if(HAG_DRIVER_INTEL)
#     add_subdirectory(driver/intel)
# endif()
