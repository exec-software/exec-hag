/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#include "unity.h"
#include "hag/api/api.h"
#include "hag/op/tensor_ops.h"
#include <math.h>

void test_op_fill_float32(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    uint64_t shape[] = {4, 4};
    HagTensorCreateInfo tensorInfo = {0};
    tensorInfo.dimensions = 2;
    tensorInfo.pShape = shape;
    tensorInfo.dtype = HAG_DTYPE_FLOAT32;
    
    HagTensor tensor;
    result = hagCreateTensor(device, &tensorInfo, &tensor);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    result = hagOpFill(device, tensor, 3.14f);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Verify by mapping */
    HagBuffer buffer;
    uint64_t offset;
    hagGetTensorBuffer(tensor, &buffer, &offset);
    
    void* data;
    result = hagMapBuffer(device, buffer, &data);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    float* floats = (float*)((char*)data + offset);
    for (int i = 0; i < 16; i++) {
        TEST_ASSERT_EQUAL(1, fabsf(floats[i] - 3.14f) < 0.001f);
    }
    
    hagUnmapBuffer(device, buffer);
    hagDestroyTensor(device, tensor);
    hagDestroyDevice(device);
    hagDestroyInstance(instance);
}

void test_op_add_float32(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    uint64_t shape[] = {4};
    HagTensorCreateInfo tensorInfo = {0};
    tensorInfo.dimensions = 1;
    tensorInfo.pShape = shape;
    tensorInfo.dtype = HAG_DTYPE_FLOAT32;
    
    HagTensor a, b, out;
    result = hagCreateTensor(device, &tensorInfo, &a);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    result = hagCreateTensor(device, &tensorInfo, &b);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    result = hagCreateTensor(device, &tensorInfo, &out);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    hagOpFill(device, a, 1.0f);
    hagOpFill(device, b, 2.0f);
    
    result = hagOpAdd(device, a, b, out);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Verify */
    HagBuffer buffer;
    uint64_t offset;
    hagGetTensorBuffer(out, &buffer, &offset);
    
    void* data;
    hagMapBuffer(device, buffer, &data);
    float* floats = (float*)((char*)data + offset);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL(1, fabsf(floats[i] - 3.0f) < 0.001f);
    }
    hagUnmapBuffer(device, buffer);
    
    hagDestroyTensor(device, a);
    hagDestroyTensor(device, b);
    hagDestroyTensor(device, out);
    hagDestroyDevice(device);
    hagDestroyInstance(instance);
}

void test_op_matmul_float32(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* A: [2, 3], B: [3, 2], out: [2, 2] */
    HagTensor a, b, out;
    
    uint64_t shapeA[] = {2, 3};
    HagTensorCreateInfo infoA = {0};
    infoA.dimensions = 2;
    infoA.pShape = shapeA;
    infoA.dtype = HAG_DTYPE_FLOAT32;
    result = hagCreateTensor(device, &infoA, &a);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    uint64_t shapeB[] = {3, 2};
    HagTensorCreateInfo infoB = {0};
    infoB.dimensions = 2;
    infoB.pShape = shapeB;
    infoB.dtype = HAG_DTYPE_FLOAT32;
    result = hagCreateTensor(device, &infoB, &b);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    uint64_t shapeOut[] = {2, 2};
    HagTensorCreateInfo infoOut = {0};
    infoOut.dimensions = 2;
    infoOut.pShape = shapeOut;
    infoOut.dtype = HAG_DTYPE_FLOAT32;
    result = hagCreateTensor(device, &infoOut, &out);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Fill A and B with known values */
    hagOpFill(device, a, 1.0f);
    hagOpFill(device, b, 2.0f);
    
    /* out = A * B: each element should be sum of 3 products = 3 * (1*2) = 6 */
    result = hagOpMatmul(device, a, b, out);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Verify */
    HagBuffer buffer;
    uint64_t offset;
    hagGetTensorBuffer(out, &buffer, &offset);
    
    void* data;
    hagMapBuffer(device, buffer, &data);
    float* floats = (float*)((char*)data + offset);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL(1, fabsf(floats[i] - 6.0f) < 0.001f);
    }
    hagUnmapBuffer(device, buffer);
    
    hagDestroyTensor(device, a);
    hagDestroyTensor(device, b);
    hagDestroyTensor(device, out);
    hagDestroyDevice(device);
    hagDestroyInstance(instance);
}

void test_op_softmax_float32(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    uint64_t shape[] = {2, 4};
    HagTensorCreateInfo tensorInfo = {0};
    tensorInfo.dimensions = 2;
    tensorInfo.pShape = shape;
    tensorInfo.dtype = HAG_DTYPE_FLOAT32;
    
    HagTensor input, output;
    result = hagCreateTensor(device, &tensorInfo, &input);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    result = hagCreateTensor(device, &tensorInfo, &output);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Fill input with zeros - softmax should give uniform distribution */
    hagOpFill(device, input, 0.0f);
    
    result = hagOpSoftmax(device, input, output);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Verify: each row should sum to 1.0, each element = 0.25 */
    HagBuffer buffer;
    uint64_t offset;
    hagGetTensorBuffer(output, &buffer, &offset);
    
    void* data;
    hagMapBuffer(device, buffer, &data);
    float* floats = (float*)((char*)data + offset);
    
    for (int row = 0; row < 2; row++) {
        float sum = 0.0f;
        for (int i = 0; i < 4; i++) {
            sum += floats[row * 4 + i];
            TEST_ASSERT_EQUAL(1, fabsf(floats[row * 4 + i] - 0.25f) < 0.001f);
        }
        TEST_ASSERT_EQUAL(1, fabsf(sum - 1.0f) < 0.001f);
    }
    
    hagUnmapBuffer(device, buffer);
    hagDestroyTensor(device, input);
    hagDestroyTensor(device, output);
    hagDestroyDevice(device);
    hagDestroyInstance(instance);
}
