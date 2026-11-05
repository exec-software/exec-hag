# Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
# SPDX-License-Identifier: MIT

# CMakeLists.txt for CLI tools

# hag-info: Device enumeration and info tool
add_executable(hag-info
    hag-info/main.c
)

target_include_directories(hag-info
    PRIVATE
        ${CMAKE_SOURCE_DIR}/hag
        ${CMAKE_SOURCE_DIR}/hag/api
)

target_link_libraries(hag-info
    PRIVATE
        hag_static
        ${DRM_LIBRARIES}
        ${ELF_LIBRARY}
        m
        pthread
        dl
)

# hag-bench: Benchmark suite (future)
# add_executable(hag-bench
#     hag-bench/main.c
# )
