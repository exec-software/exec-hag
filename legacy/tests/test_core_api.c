/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#include "unity.h"
#include "hag/api/api.h"
#include <stdlib.h>
#include <string.h>

void test_instance_create_destroy(void) {
    HagInstance instance;
    HagInstanceCreateInfo info = {0};
    
    HagResult result = hagCreateInstance(&info, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(instance);
    
    hagDestroyInstance(instance);
}

void test_instance_version_check(void) {
    HagInstance instance;
    HagInstanceCreateInfo info = {0};
    
    /* Request current version - should succeed */
    info.apiVersion = hagGetApiVersion();
    HagResult result = hagCreateInstance(&info, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    hagDestroyInstance(instance);
    
    /* Request version 0 (unspecified) - should succeed */
    info.apiVersion = 0;
    result = hagCreateInstance(&info, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    hagDestroyInstance(instance);
    
    /* Request impossibly high version - should fail */
    info.apiVersion = 0xFFFFFFFF;
    result = hagCreateInstance(&info, &instance);
    TEST_ASSERT_NOT_EQUAL(HAG_SUCCESS, result);
}

void test_enumerate_devices(void) {
    uint32_t count = 0;
    HagResult result = hagEnumerateDevices(&count, NULL);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Should have at least CPU fallback device */
    TEST_ASSERT_GREATER_THAN(0, count);
}

void test_device_create_destroy(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Get device count */
    uint32_t count = 0;
    result = hagEnumerateDevices(&count, NULL);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    TEST_ASSERT_GREATER_THAN(0, count);
    
    /* For v0.1, we don't have proper physical device handles yet,
     * so device creation uses a simplified path.
     * We'll test with a NULL physicalDevice which should pick the first one. */
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    devInfo.physicalDevice = NULL; /* Use first available */
    
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(device);
    
    hagDestroyDevice(device);
    hagDestroyInstance(instance);
}

void test_device_properties(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagDeviceProperties props;
    result = hagGetDeviceProperties(device, &props);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Verify properties are sensible */
    TEST_ASSERT_NOT_EQUAL(0, props.apiVersion);
    TEST_ASSERT_NOT_EQUAL(0, strlen(props.deviceName));
    
    hagDestroyDevice(device);
    hagDestroyInstance(instance);
}

void test_device_wait_idle(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    result = hagDeviceWaitIdle(device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    hagDestroyDevice(device);
    hagDestroyInstance(instance);
}

void test_event_create_wait(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagEvent event;
    result = hagCreateEvent(device, &event);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(event);
    
    /* Event should be unsignaled initially */
    result = hagGetEventStatus(event);
    TEST_ASSERT_NOT_EQUAL(HAG_SUCCESS, result); /* Should be timeout/unsignaled */
    
    /* Reset should succeed */
    result = hagResetEvent(event);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Wait with 0 timeout should timeout immediately */
    result = hagWaitForEvents(1, &event, 0);
    TEST_ASSERT_NOT_EQUAL(HAG_SUCCESS, result);
    
    hagDestroyEvent(device, event);
    hagDestroyDevice(device);
    hagDestroyInstance(instance);
}

void test_physical_device_enumeration(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Query count */
    uint32_t count = 0;
    result = hagEnumeratePhysicalDevices(instance, &count, NULL);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    /* Should have at least CPU fallback, possibly GPU */
    TEST_ASSERT_GREATER_THAN(0, count);
    
    /* Enumerate devices */
    HagPhysicalDevice* devices = (HagPhysicalDevice*)malloc(sizeof(HagPhysicalDevice) * count);
    result = hagEnumeratePhysicalDevices(instance, &count, devices);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Get properties of first device */
    HagDeviceProperties props;
    result = hagGetPhysicalDeviceProperties(devices[0], &props);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    TEST_ASSERT_NOT_EQUAL(0, props.apiVersion);
    TEST_ASSERT_NOT_EQUAL(0, strlen(props.deviceName));
    
    /* Create device with explicit physical device */
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    devInfo.physicalDevice = devices[0];
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(device);
    
    hagDestroyDevice(device);
    free(devices);
    hagDestroyInstance(instance);
}

void test_physical_device_invalid_handle(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Try to create device with invalid physical device pointer */
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    /* Create a dummy pointer that doesn't belong to this instance */
    int dummy = 0;
    devInfo.physicalDevice = (HagPhysicalDevice)&dummy;
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_NOT_EQUAL(HAG_SUCCESS, result);
    
    hagDestroyInstance(instance);
}
