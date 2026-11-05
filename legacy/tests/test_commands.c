/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#include "unity.h"
#include "hag/api/api.h"

void test_cmd_buffer_lifecycle(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagQueue queue;
    result = hagGetDeviceQueue(device, HAG_QUEUE_COMPUTE, 0, &queue);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Create command buffer */
    HagCmdBuffer cmdBuffer;
    HagCmdBufferCreateInfo cmdInfo = {0};
    cmdInfo.queue = queue;
    result = hagCreateCmdBuffer(device, &cmdInfo, &cmdBuffer);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(cmdBuffer);
    
    /* Begin recording */
    result = hagBeginCmdBuffer(cmdBuffer, 0);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Record some commands (these should not crash) */
    HagBufferCreateInfo bufInfo = {0};
    bufInfo.size = 1024;
    bufInfo.memoryType = HAG_MEMORY_HOST_VISIBLE;
    bufInfo.usageFlags = HAG_BUFFER_USAGE_STORAGE | HAG_BUFFER_USAGE_TRANSFER_DST;
    
    HagBuffer buffer;
    result = hagCreateBuffer(device, &bufInfo, &buffer);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(buffer);
    
    hagCmdFillBuffer(cmdBuffer, buffer, 0, 1024, 0xAB);
    hagCmdBarrier(cmdBuffer, 0, 0);
    
    /* End recording */
    result = hagEndCmdBuffer(cmdBuffer);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Submit */
    HagSubmitInfo submit = {0};
    submit.cmdBufferCount = 1;
    submit.pCmdBuffers = &cmdBuffer;
    
    HagEvent event;
    result = hagCreateEvent(device, &event);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    result = hagQueueSubmit(queue, 1, &submit, event);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Wait for completion */
    result = hagWaitForEvents(1, &event, 1000000000ULL); /* 1 second timeout */
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Reset and reuse */
    result = hagResetCmdBuffer(cmdBuffer, 0);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    result = hagBeginCmdBuffer(cmdBuffer, 0);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagBuffer buffer2;
    result = hagCreateBuffer(device, &bufInfo, &buffer2);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    hagCmdCopyBuffer(cmdBuffer, buffer, buffer2, 0, 0, 1024);
    
    result = hagEndCmdBuffer(cmdBuffer);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    result = hagResetEvent(event);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    result = hagQueueSubmit(queue, 1, &submit, event);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    result = hagWaitForEvents(1, &event, 1000000000ULL);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Cleanup */
    hagDestroyEvent(device, event);
    hagDestroyBuffer(device, buffer);
    hagDestroyBuffer(device, buffer2);
    hagDestroyCmdBuffer(device, cmdBuffer);
    hagDestroyDevice(device);
    hagDestroyInstance(instance);
}

void test_kernel_lifecycle(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Create a kernel by name (CPU driver built-in) */
    HagKernelCreateInfo kinfo = {0};
    kinfo.language = HAG_KERNEL_LANGUAGE_HGSL;
    kinfo.pCode = "add_f32";
    kinfo.codeSize = 8;
    kinfo.entryPoint = "main";
    
    HagKernel kernel;
    result = hagCreateKernel(device, &kinfo, &kernel);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(kernel);
    
    hagDestroyKernel(device, kernel);
    hagDestroyDevice(device);
    hagDestroyInstance(instance);
}
