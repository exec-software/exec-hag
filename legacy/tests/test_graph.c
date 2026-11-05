/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#include "unity.h"
#include "hag/api/api.h"

void test_graph_create_destroy(void) {
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
    
    /* Create a simple graph: fill buffer -> copy buffer */
    HagBufferCreateInfo bufInfo = {0};
    bufInfo.size = 256;
    bufInfo.memoryType = HAG_MEMORY_HOST_VISIBLE;
    bufInfo.usageFlags = HAG_BUFFER_USAGE_STORAGE | HAG_BUFFER_USAGE_TRANSFER_DST | HAG_BUFFER_USAGE_TRANSFER_SRC;
    
    HagBuffer bufferA;
    result = hagCreateBuffer(device, &bufInfo, &bufferA);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagBuffer bufferB;
    result = hagCreateBuffer(device, &bufInfo, &bufferB);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagGraphNode nodes[] = {
        {
            .type = HAG_GRAPH_NODE_FILL,
            .nodeIndex = 0,
            .dependencyCount = 0,
            .params.fill = {
                .buffer = bufferA,
                .size = 256,
                .data = 0x42,
            },
        },
        {
            .type = HAG_GRAPH_NODE_COPY,
            .nodeIndex = 1,
            .dependencyCount = 1,
            .pDependencies = (uint32_t[]){0},
            .params.copy = {
                .src = bufferA,
                .dst = bufferB,
                .size = 256,
            },
        },
    };
    
    HagGraphCreateInfo graphInfo = {0};
    graphInfo.nodeCount = 2;
    graphInfo.pNodes = nodes;
    
    HagGraph graph;
    result = hagCreateGraph(device, &graphInfo, &graph);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(graph);
    
    /* Execute graph */
    HagEvent event;
    result = hagCreateEvent(device, &event);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagSubmitInfo submit = {0};
    submit.pGraphs = &graph;
    submit.graphCount = 1;
    
    result = hagQueueSubmit(queue, 1, &submit, event);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    result = hagWaitForEvents(1, &event, 1000000000ULL);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    hagDestroyEvent(device, event);
    hagDestroyGraph(device, graph);
    hagDestroyBuffer(device, bufferA);
    hagDestroyBuffer(device, bufferB);
    hagDestroyDevice(device);
    hagDestroyInstance(instance);
}

void test_graph_cycle_detection(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagBufferCreateInfo bufInfo = {0};
    bufInfo.size = 64;
    bufInfo.memoryType = HAG_MEMORY_HOST_VISIBLE;
    bufInfo.usageFlags = HAG_BUFFER_USAGE_STORAGE;
    
    HagBuffer buffer;
    result = hagCreateBuffer(device, &bufInfo, &buffer);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Create graph with cycle: 0 -> 1 -> 2 -> 0 */
    HagGraphNode nodes[] = {
        {
            .type = HAG_GRAPH_NODE_FILL,
            .nodeIndex = 0,
            .dependencyCount = 1,
            .pDependencies = (uint32_t[]){2},
            .params.fill = {
                .buffer = buffer,
                .size = 64,
                .data = 0x01,
            },
        },
        {
            .type = HAG_GRAPH_NODE_FILL,
            .nodeIndex = 1,
            .dependencyCount = 1,
            .pDependencies = (uint32_t[]){0},
            .params.fill = {
                .buffer = buffer,
                .size = 64,
                .data = 0x02,
            },
        },
        {
            .type = HAG_GRAPH_NODE_FILL,
            .nodeIndex = 2,
            .dependencyCount = 1,
            .pDependencies = (uint32_t[]){1},
            .params.fill = {
                .buffer = buffer,
                .size = 64,
                .data = 0x03,
            },
        },
    };
    
    HagGraphCreateInfo graphInfo = {0};
    graphInfo.nodeCount = 3;
    graphInfo.pNodes = nodes;
    
    HagGraph graph;
    result = hagCreateGraph(device, &graphInfo, &graph);
    TEST_ASSERT_NOT_EQUAL(HAG_SUCCESS, result);
    
    hagDestroyBuffer(device, buffer);
    hagDestroyDevice(device);
    hagDestroyInstance(instance);
}

void test_graph_invalid_node_index(void) {
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    HagResult result = hagCreateInstance(&instInfo, &instance);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    result = hagCreateDevice(instance, &devInfo, &device);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    HagBufferCreateInfo bufInfo = {0};
    bufInfo.size = 64;
    bufInfo.memoryType = HAG_MEMORY_HOST_VISIBLE;
    bufInfo.usageFlags = HAG_BUFFER_USAGE_STORAGE;
    
    HagBuffer buffer;
    result = hagCreateBuffer(device, &bufInfo, &buffer);
    TEST_ASSERT_EQUAL(HAG_SUCCESS, result);
    
    /* Dependency points to non-existent node */
    HagGraphNode nodes[] = {
        {
            .type = HAG_GRAPH_NODE_FILL,
            .nodeIndex = 0,
            .dependencyCount = 1,
            .pDependencies = (uint32_t[]){99},
            .params.fill = {
                .buffer = buffer,
                .size = 64,
                .data = 0x01,
            },
        },
    };
    
    HagGraphCreateInfo graphInfo = {0};
    graphInfo.nodeCount = 1;
    graphInfo.pNodes = nodes;
    
    HagGraph graph;
    result = hagCreateGraph(device, &graphInfo, &graph);
    TEST_ASSERT_NOT_EQUAL(HAG_SUCCESS, result);
    
    hagDestroyBuffer(device, buffer);
    hagDestroyDevice(device);
    hagDestroyInstance(instance);
}
