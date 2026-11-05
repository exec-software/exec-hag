/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

/* Unity test runner main */
#include "unity.h"

/* Test suites declared in other files */
extern void test_api_version(void);
extern void test_result_to_string(void);
extern void test_dtype_sizes(void);
extern void test_slab_allocator_basic(void);
extern void test_slab_allocator_growth(void);
extern void test_arena_allocator_basic(void);
extern void test_arena_allocator_exhaustion(void);
extern void test_instance_create_destroy(void);
extern void test_instance_version_check(void);
extern void test_enumerate_devices(void);
extern void test_device_create_destroy(void);
extern void test_device_properties(void);
extern void test_device_wait_idle(void);
extern void test_event_create_wait(void);
extern void test_physical_device_enumeration(void);
extern void test_physical_device_invalid_handle(void);
extern void test_cmd_buffer_lifecycle(void);
extern void test_kernel_lifecycle(void);
extern void test_tensor_create_destroy(void);
extern void test_tensor_invalid_dimensions(void);
extern void test_tensor_with_external_buffer(void);
extern void test_tensor_different_dtypes(void);
extern void test_graph_create_destroy(void);
extern void test_graph_cycle_detection(void);
extern void test_graph_invalid_node_index(void);
extern void test_op_fill_float32(void);
extern void test_op_add_float32(void);
extern void test_op_matmul_float32(void);
extern void test_op_softmax_float32(void);
extern void test_profiler_create_destroy(void);
extern void test_profiler_basic_timing(void);
extern void test_profiler_multiple_entries(void);
extern void test_profiler_export(void);

void setUp(void) { }
void tearDown(void) { }

int main(void) {
    UNITY_BEGIN();

    /* API basics */
    RUN_TEST(test_api_version);
    RUN_TEST(test_result_to_string);
    RUN_TEST(test_dtype_sizes);

    /* Memory allocators */
    RUN_TEST(test_slab_allocator_basic);
    RUN_TEST(test_slab_allocator_growth);
    RUN_TEST(test_arena_allocator_basic);
    RUN_TEST(test_arena_allocator_exhaustion);

    /* Core API */
    RUN_TEST(test_instance_create_destroy);
    RUN_TEST(test_instance_version_check);
    RUN_TEST(test_enumerate_devices);
    RUN_TEST(test_device_create_destroy);
    RUN_TEST(test_device_properties);
    RUN_TEST(test_device_wait_idle);
    RUN_TEST(test_event_create_wait);
    RUN_TEST(test_physical_device_enumeration);
    RUN_TEST(test_physical_device_invalid_handle);
    RUN_TEST(test_cmd_buffer_lifecycle);
    RUN_TEST(test_kernel_lifecycle);
    RUN_TEST(test_tensor_create_destroy);
    RUN_TEST(test_tensor_invalid_dimensions);
    RUN_TEST(test_tensor_with_external_buffer);
    RUN_TEST(test_tensor_different_dtypes);
    RUN_TEST(test_graph_create_destroy);
    RUN_TEST(test_graph_cycle_detection);
    RUN_TEST(test_graph_invalid_node_index);
    RUN_TEST(test_op_fill_float32);
    RUN_TEST(test_op_add_float32);
    RUN_TEST(test_op_matmul_float32);
    RUN_TEST(test_op_softmax_float32);
    RUN_TEST(test_profiler_create_destroy);
    RUN_TEST(test_profiler_basic_timing);
    RUN_TEST(test_profiler_multiple_entries);
    RUN_TEST(test_profiler_export);

    return UNITY_END();
}
