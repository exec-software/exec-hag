/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#include "unity.h"
#include "hag/api/api.h"

void test_tensor_create_destroy(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Create a 2D tensor: [3, 4] of float32 */
    uint64_t shape[] = {3, 4};
    HagTensorCreateInfo tensorInfo = {0};
    tensorInfo.dimensions = 2;
    tensorInfo.pShape = shape;
    tensorInfo.dtype = HAG_DTYPE_FLOAT32;
    tensorInfo.buffer = NULL; /* Let Hag allocate */
    tensorInfo.offset = 0;
    
    HagTensor tensor;
    result = hagCreateTensor(device, &tensorInfo, &tensor);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(tensor);
    
    /* Verify properties */
    uint32_t dims = 0;
    uint64_t outShape[8] = {0};
    result = hagGetTensorShape(tensor, &dims, outShape);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    TEST_ASSERT_EQUAL(2, dims);
    TEST_ASSERT_EQUAL(3, outShape[0]);
    TEST_ASSERT_EQUAL(4, outShape[1]);
    
    HagDType dtype = hagGetTensorDType(tensor);
    TEST_ASSERT_EQUAL(HAG_DTYPE_FLOAT32, dtype);
    
    /* Verify buffer association */
    HagBuffer buffer;
    uint64_t offset;
    result = hagGetTensorBuffer(tensor, &buffer, &offset);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_EQUAL(0, offset);
    
    hagDestroyTensor(device, tensor);
    hagDestroyDevice(device);
    hagDestroyInstance(instance);
}

void test_tensor_invalid_dimensions(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Zero dimensions should fail */
    HagTensorCreateInfo tensorInfo = {0};
    tensorInfo.dimensions = 0;
    tensorInfo.pShape = (uint64_t[]){1};
    tensorInfo.dtype = HAG_DTYPE_FLOAT32;
    
    HagTensor tensor;
    result = hagCreateTensor(device, &tensorInfo, &tensor);
    TEST_ASSERT_NOT_EQUAL(HAG_SUCCESS, result);
    
    /* Zero-sized dimension should fail */
    tensorInfo.dimensions = 2;
    tensorInfo.pShape = (uint64_t[]){3, 0};
    result = hagCreateTensor(device, &tensorInfo, &tensor);
    TEST_ASSERT_NOT_EQUAL(HAG_SUCCESS, result);
    
    /* Too many dimensions should fail */
    tensorInfo.dimensions = 100;
    tensorInfo.pShape = (uint64_t[]){1, 1, 1, 1, 1, 1, 1, 1, 1};
    result = hagCreateTensor(device, &tensorInfo, &tensor);
    TEST_ASSERT_NOT_EQUAL(HAG_SUCCESS, result);
    
    hagDestroyDevice(device);
    hagDestroyInstance(instance);
}

void test_tensor_with_external_buffer(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Create external buffer */
    HagBufferCreateInfo bufInfo = {0};
    bufInfo.size = 1024;
    bufInfo.memoryType = HAG_MEMORY_HOST_VISIBLE;
    bufInfo.usageFlags = HAG_BUFFER_USAGE_STORAGE;
    
    HagBuffer buffer;
    result = hagCreateBuffer(device, &bufInfo, &buffer);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Create tensor using external buffer */
    uint64_t shape[] = {16, 16}; /* 256 floats = 1024 bytes */
    HagTensorCreateInfo tensorInfo = {0};
    tensorInfo.dimensions = 2;
    tensorInfo.pShape = shape;
    tensorInfo.dtype = HAG_DTYPE_FLOAT32;
    tensorInfo.buffer = buffer;
    tensorInfo.offset = 0;
    
    HagTensor tensor;
    result = hagCreateTensor(device, &tensorInfo, &tensor);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Verify it uses the external buffer */
    HagBuffer outBuffer;
    uint64_t offset;
    result = hagGetTensorBuffer(tensor, &outBuffer, &offset);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    TEST_ASSERT_EQUAL(buffer, outBuffer);
    
    hagDestroyTensor(device, tensor);
    hagDestroyBuffer(device, buffer);
    hagDestroyDevice(device);
    hagDestroyInstance(instance);
}

void test_tensor_different_dtypes(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    uint64_t shape[] = {10};
    HagDType dtypes[] = {
        HAG_DTYPE_FLOAT32,
        HAG_DTYPE_FLOAT16,
        HAG_DTYPE_BFLOAT16,
        HAG_DTYPE_INT32,
        HAG_DTYPE_INT8,
        HAG_DTYPE_UINT8,
    };
    
    for (size_t i = 0; i < sizeof(dtypes)/sizeof(dtypes[0]); i++) {
        HagTensorCreateInfo tensorInfo = {0};
        tensorInfo.dimensions = 1;
        tensorInfo.pShape = shape;
        tensorInfo.dtype = dtypes[i];
        
        HagTensor tensor;
        result = hagCreateTensor(device, &tensorInfo, &tensor);
        TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
        
        HagDType actualDtype = hagGetTensorDType(tensor);
        TEST_ASSERT_EQUAL(dtypes[i], actualDtype);
        
        hagDestroyTensor(device, tensor);
    }
    
    hagDestroyDevice(device);
    hagDestroyInstance(instance);
}
