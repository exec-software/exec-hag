# Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.20)
project(Hag
    VERSION 0.1.0
    DESCRIPTION "Hag AI Compute Runtime"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# =============================================================================
# Options
# =============================================================================
option(HAG_BUILD_TESTS "Build test suite" ON)
option(HAG_BUILD_TOOLS "Build CLI tools" ON)
option(HAG_BUILD_DOCS "Build documentation" OFF)
option(HAG_BUILD_SDK "Package SDK artifacts" OFF)
option(HAG_BUILD_SHARED "Build shared library" ON)
option(HAG_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(HAG_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(HAG_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(HAG_ENABLE_PROFILER "Enable built-in profiler" OFF)
option(HAG_DRIVER_CPU "Build CPU fallback driver" ON)
option(HAG_DRIVER_AMDGPU "Build AMD GPU driver" ON)
option(HAG_DRIVER_INTEL "Build Intel GPU driver" ON)
option(HAG_DRIVER_NOUVEAU "Build Nouveau driver (experimental)" OFF)
option(HAG_USE_SYSTEM_OPENBLAS "Use system OpenBLAS" ON)
option(HAG_USE_SYSTEM_SPIRV_TOOLS "Use system SPIRV-Tools" ON)

# =============================================================================
# Compiler Flags
# =============================================================================
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wcast-align
        -Wshadow
        -Wwrite-strings
        -fPIC
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-O0 -g3)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -DNDEBUG)
    elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(-O2 -g)
    endif()
endif()

# Sanitizers
if(HAG_ENABLE_ASAN AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()
if(HAG_ENABLE_TSAN AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()
if(HAG_ENABLE_UBSAN AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
endif()

# Symbol visibility
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)

# =============================================================================
# Find Dependencies
# =============================================================================
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(DRM libdrm)
endif()

if(NOT DRM_FOUND)
    message(WARNING "libdrm not found. GPU drivers will be disabled. Install libdrm-dev for full GPU support.")
    set(HAG_DRIVER_AMDGPU OFF)
    set(HAG_DRIVER_INTEL OFF)
    set(HAG_DRIVER_NOUVEAU OFF)
endif()

find_library(ELF_LIBRARY elf)
if(NOT ELF_LIBRARY)
    set(ELF_LIBRARY "")
endif()

# =============================================================================
# Testing
# =============================================================================
if(HAG_BUILD_TESTS)
    enable_testing()
endif()

# =============================================================================
# Subdirectories
# =============================================================================
add_subdirectory(hag)

if(HAG_BUILD_TESTS)
    add_subdirectory(libs)
    add_subdirectory(tests)
endif()

if(HAG_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# Samples are built by default when tools are enabled
option(HAG_BUILD_SAMPLES "Build sample applications" ON)
if(HAG_BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

# =============================================================================
# SDK Packaging
# =============================================================================
if(HAG_BUILD_SDK)
    include(GNUInstallDirs)
    install(TARGETS hag
        EXPORT HagTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(DIRECTORY hag/api/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hag
        FILES_MATCHING PATTERN "*.h"
    )
    install(EXPORT HagTargets
        FILE HagTargets.cmake
        NAMESPACE Hag::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Hag
    )
endif()
