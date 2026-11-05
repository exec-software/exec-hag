# Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
# SPDX-License-Identifier: MIT

# Compatibility test helpers for the archived prototype

# Unity-compatible test helpers
# These are Execute Software's minimal Unity-compatible placeholders, not
# a downloaded or vendored copy of the upstream Unity test framework.
# Upstream reference: https://github.com/ThrowTheSwitch/Unity
# License of these placeholders: MIT

add_library(unity STATIC
    unity/unity.c
)

target_include_directories(unity
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/unity
)

set_target_properties(unity PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
