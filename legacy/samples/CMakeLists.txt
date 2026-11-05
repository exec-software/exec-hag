# Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
# SPDX-License-Identifier: MIT

# CMakeLists.txt for samples

# hello_compute: Simple vector addition using Hag CPU fallback
add_executable(hello_compute
    hello_compute/main.c
)

target_include_directories(hello_compute
    PRIVATE
        ${CMAKE_SOURCE_DIR}/hag
        ${CMAKE_SOURCE_DIR}/hag/api
)

target_link_libraries(hello_compute
    PRIVATE
        hag_static
        ${DRM_LIBRARIES}
        ${ELF_LIBRARY}
        m
        pthread
        dl
)

# simple_gemm: Matrix multiplication (future)
# add_executable(simple_gemm
#     simple_gemm/main.c
# )
