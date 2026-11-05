# Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
# SPDX-License-Identifier: MIT

# CMakeLists.txt for tests

# Unity Test Framework (header-only, will be in libs/unity/)
set(UNITY_SOURCES
    ${CMAKE_SOURCE_DIR}/libs/unity/unity.c
)

set(TEST_SOURCES
    test_main.c
    test_utils_alloc.c
    test_api_basics.c
    test_memory_slab.c
    test_core_api.c
    test_commands.c
    test_tensor.c
    test_graph.c
    test_tensor_ops.c
    test_profiler.c
)

add_executable(hag_tests
    ${UNITY_SOURCES}
    ${TEST_SOURCES}
)

target_include_directories(hag_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/hag
        ${CMAKE_SOURCE_DIR}/hag/api
        ${CMAKE_SOURCE_DIR}/libs/unity
)

# Suppress missing-prototypes warnings in test files
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(${TEST_SOURCES} PROPERTIES
        COMPILE_OPTIONS "-Wno-missing-prototypes"
    )
endif()

target_link_libraries(hag_tests
    PRIVATE
        hag_static
        ${DRM_LIBRARIES}
        ${ELF_LIBRARY}
        m
        pthread
        dl
)

add_test(NAME hag_unit_tests COMMAND hag_tests)
