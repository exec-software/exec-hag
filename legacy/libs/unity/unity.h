/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 *
 * Unity Test Framework - placeholder
 * Download the actual Unity from: https://github.com/ThrowTheSwitch/Unity
 * and replace these files.
 */

#ifndef UNITY_H
#define UNITY_H

#include <stdio.h>
#include <string.h>
#include <setjmp.h>

/* Minimal Unity stub for compilation */
#define UNITY_BEGIN() unity_begin()
#define UNITY_END() unity_end()
#define RUN_TEST(func) run_test(func, #func)

#define TEST_ASSERT_EQUAL(expected, actual) \
    if ((expected) != (actual)) { \
        printf("FAIL: %s:%d: Values not equal\n", __FILE__, __LINE__); \
        longjmp(unity_jmp_buf, 1); \
    }

#define TEST_ASSERT_NOT_EQUAL(expected, actual) \
    if ((expected) == (actual)) { \
        printf("FAIL: %s:%d: Expected not equal\n", __FILE__, __LINE__); \
        longjmp(unity_jmp_buf, 1); \
    }

#define TEST_ASSERT_NOT_NULL(pointer) \
    if ((pointer) == NULL) { \
        printf("FAIL: %s:%d: Expected non-NULL\n", __FILE__, __LINE__); \
        longjmp(unity_jmp_buf, 1); \
    }

#define TEST_ASSERT_NULL(pointer) \
    if ((pointer) != NULL) { \
        printf("FAIL: %s:%d: Expected NULL\n", __FILE__, __LINE__); \
        longjmp(unity_jmp_buf, 1); \
    }

#define TEST_ASSERT_EQUAL_STRING(expected, actual) \
    if (strcmp((expected), (actual)) != 0) { \
        printf("FAIL: %s:%d: Expected \"%s\", got \"%s\"\n", __FILE__, __LINE__, (expected), (actual)); \
        longjmp(unity_jmp_buf, 1); \
    }

#define TEST_ASSERT_GREATER_THAN(threshold, actual) \
    if ((actual) <= (threshold)) { \
        printf("FAIL: %s:%d: Expected greater than %d\n", __FILE__, __LINE__, (int)(threshold)); \
        longjmp(unity_jmp_buf, 1); \
    }

extern jmp_buf unity_jmp_buf;

static inline int unity_begin(void) { return 0; }
static inline int unity_end(void) { return 0; }

static inline void run_test(void (*func)(void), const char* name) {
    printf("RUNNING: %s ... ", name);
    if (setjmp(unity_jmp_buf) == 0) {
        func();
        printf("PASS\n");
    } else {
        printf("FAIL\n");
    }
}

#endif /* UNITY_H */
