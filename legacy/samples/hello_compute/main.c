/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 *
 * hello_compute - Simple vector addition using Hag CPU fallback
 *
 * This sample demonstrates:
 *   - Instance and device creation
 *   - Buffer allocation
 *   - Command buffer recording
 *   - Kernel execution (via fill + copy for v0.1)
 *   - Event synchronization
 *   - Cleanup
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "hag/api/api.h"

#define ARRAY_SIZE 1024
#define SEED_VALUE 0x42

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("Hag Hello Compute Sample\n");
    printf("========================\n\n");

    /* Initialize Hag */
    HagInstance instance;
    HagInstanceCreateInfo instInfo = {0};
    instInfo.apiVersion = hagGetApiVersion();

    HagResult result = hagCreateInstance(&instInfo, &instance);
    if (result != HAG_SUCCESS) {
        fprintf(stderr, "Failed to create instance: %s\n", hagResultToString(result));
        return 1;
    }
    printf("Created Hag instance (API version %d.%d.%d)\n",
           HAG_API_VERSION_MAJOR, HAG_API_VERSION_MINOR, HAG_API_VERSION_PATCH);

    /* Enumerate devices */
    uint32_t deviceCount = 0;
    result = hagEnumerateDevices(&deviceCount, NULL);
    if (result != HAG_SUCCESS) {
        fprintf(stderr, "Failed to enumerate devices: %s\n", hagResultToString(result));
        hagDestroyInstance(instance);
        return 1;
    }
    printf("Found %u device(s)\n", deviceCount);

    /* Create device (uses first available, typically CPU fallback) */
    HagDevice device;
    HagDeviceCreateInfo devInfo = {0};
    result = hagCreateDevice(instance, &devInfo, &device);
    if (result != HAG_SUCCESS) {
        fprintf(stderr, "Failed to create device: %s\n", hagResultToString(result));
        hagDestroyInstance(instance);
        return 1;
    }

    /* Print device properties */
    HagDeviceProperties props;
    result = hagGetDeviceProperties(device, &props);
    if (result == HAG_SUCCESS) {
        printf("Using device: %s\n", props.deviceName);
        printf("  Type: %s\n",
               props.deviceType == HAG_DEVICE_TYPE_CPU_FALLBACK ? "CPU Fallback" :
               props.deviceType == HAG_DEVICE_TYPE_DISCRETE_GPU ? "Discrete GPU" :
               props.deviceType == HAG_DEVICE_TYPE_INTEGRATED_GPU ? "Integrated GPU" : "Other");
        printf("  Memory: %lu MB\n", (unsigned long)(props.sharedMemoryBytes / (1024 * 1024)));
    }

    /* Get compute queue */
    HagQueue queue;
    result = hagGetDeviceQueue(device, HAG_QUEUE_COMPUTE, 0, &queue);
    if (result != HAG_SUCCESS) {
        fprintf(stderr, "Failed to get compute queue: %s\n", hagResultToString(result));
        hagDestroyDevice(device);
        hagDestroyInstance(instance);
        return 1;
    }

    /* Create buffers */
    HagBufferCreateInfo bufInfo = {0};
    bufInfo.size = ARRAY_SIZE * sizeof(float);
    bufInfo.memoryType = HAG_MEMORY_HOST_VISIBLE;
    bufInfo.usageFlags = HAG_BUFFER_USAGE_STORAGE | HAG_BUFFER_USAGE_TRANSFER_DST | HAG_BUFFER_USAGE_TRANSFER_SRC;

    HagBuffer bufferA, bufferB, bufferC;
    result = hagCreateBuffer(device, &bufInfo, &bufferA);
    if (result != HAG_SUCCESS) {
        fprintf(stderr, "Failed to create buffer A: %s\n", hagResultToString(result));
        goto cleanup_device;
    }

    result = hagCreateBuffer(device, &bufInfo, &bufferB);
    if (result != HAG_SUCCESS) {
        fprintf(stderr, "Failed to create buffer B: %s\n", hagResultToString(result));
        goto cleanup_buffer_a;
    }

    result = hagCreateBuffer(device, &bufInfo, &bufferC);
    if (result != HAG_SUCCESS) {
        fprintf(stderr, "Failed to create buffer C: %s\n", hagResultToString(result));
        goto cleanup_buffer_b;
    }

    printf("\nAllocated 3 buffers of %u floats each\n", ARRAY_SIZE);

    /* Create command buffer for initialization */
    HagCmdBuffer cmdBuffer;
    HagCmdBufferCreateInfo cmdInfo = {0};
    cmdInfo.queue = queue;

    result = hagCreateCmdBuffer(device, &cmdInfo, &cmdBuffer);
    if (result != HAG_SUCCESS) {
        fprintf(stderr, "Failed to create command buffer: %s\n", hagResultToString(result));
        goto cleanup_buffers;
    }

    /* Record: Fill buffer A with pattern, fill buffer B with different pattern */
    result = hagBeginCmdBuffer(cmdBuffer, 0);
    if (result != HAG_SUCCESS) {
        fprintf(stderr, "Failed to begin command buffer: %s\n", hagResultToString(result));
        goto cleanup_cmd;
    }

    /* For v0.1, we demonstrate using fill and copy commands.
     * In a full implementation, this would be a kernel dispatch. */
    hagCmdFillBuffer(cmdBuffer, bufferA, 0, bufInfo.size, 0x3F800000); /* 1.0f in IEEE 754 */
    hagCmdBarrier(cmdBuffer, 0, 0);
    hagCmdFillBuffer(cmdBuffer, bufferB, 0, bufInfo.size, 0x40000000); /* 2.0f in IEEE 754 */
    hagCmdBarrier(cmdBuffer, 0, 0);
    /* Copy A to C (as a placeholder for A + B = C) */
    hagCmdCopyBuffer(cmdBuffer, bufferA, bufferC, 0, 0, bufInfo.size);

    result = hagEndCmdBuffer(cmdBuffer);
    if (result != HAG_SUCCESS) {
        fprintf(stderr, "Failed to end command buffer: %s\n", hagResultToString(result));
        goto cleanup_cmd;
    }

    /* Create synchronization event */
    HagEvent event;
    result = hagCreateEvent(device, &event);
    if (result != HAG_SUCCESS) {
        fprintf(stderr, "Failed to create event: %s\n", hagResultToString(result));
        goto cleanup_cmd;
    }

    /* Submit command buffer */
    HagSubmitInfo submit = {0};
    submit.cmdBufferCount = 1;
    submit.pCmdBuffers = &cmdBuffer;

    result = hagQueueSubmit(queue, 1, &submit, event);
    if (result != HAG_SUCCESS) {
        fprintf(stderr, "Failed to submit queue: %s\n", hagResultToString(result));
        goto cleanup_event;
    }

    /* Wait for completion */
    result = hagWaitForEvents(1, &event, 5000000000ULL); /* 5 second timeout */
    if (result != HAG_SUCCESS) {
        fprintf(stderr, "Failed to wait for event: %s\n", hagResultToString(result));
        goto cleanup_event;
    }

    printf("Command buffer executed successfully\n");

    /* Verify results by mapping buffer C */
    /* For CPU fallback, we can directly inspect the buffer data */
    /* In v0.1, hagMapBuffer returns a pointer for CPU driver */
    void* mappedData = NULL;
    result = hagMapBuffer(device, bufferC, &mappedData);
    if (result == HAG_SUCCESS && mappedData) {
        float* floats = (float*)mappedData;
        printf("\nFirst 10 values in buffer C:\n");
        for (int i = 0; i < 10 && i < ARRAY_SIZE; i++) {
            printf("  [%d] = %f\n", i, floats[i]);
        }
        hagUnmapBuffer(device, bufferC);
    }

    /* Demonstrate graph execution */
    printf("\n--- Graph Execution Demo ---\n");

    HagGraphNode graphNodes[] = {
        {
            .type = HAG_GRAPH_NODE_FILL,
            .nodeIndex = 0,
            .dependencyCount = 0,
            .params.fill = {
                .buffer = bufferA,
                .size = bufInfo.size,
                .data = 0x41200000, /* 10.0f */
            },
        },
        {
            .type = HAG_GRAPH_NODE_FILL,
            .nodeIndex = 1,
            .dependencyCount = 0,
            .params.fill = {
                .buffer = bufferB,
                .size = bufInfo.size,
                .data = 0x41A00000, /* 20.0f */
            },
        },
        {
            .type = HAG_GRAPH_NODE_COPY,
            .nodeIndex = 2,
            .dependencyCount = 2,
            .pDependencies = (uint32_t[]){0, 1},
            .params.copy = {
                .src = bufferA,
                .dst = bufferC,
                .size = bufInfo.size,
            },
        },
    };

    HagGraphCreateInfo graphInfo = {0};
    graphInfo.nodeCount = 3;
    graphInfo.pNodes = graphNodes;

    HagGraph graph;
    result = hagCreateGraph(device, &graphInfo, &graph);
    if (result != HAG_SUCCESS) {
        fprintf(stderr, "Failed to create graph: %s\n", hagResultToString(result));
        goto cleanup_event;
    }

    /* Reset event */
    hagResetEvent(event);

    /* Submit graph */
    HagSubmitInfo graphSubmit = {0};
    graphSubmit.pGraphs = &graph;
    graphSubmit.graphCount = 1;

    result = hagQueueSubmit(queue, 1, &graphSubmit, event);
    if (result != HAG_SUCCESS) {
        fprintf(stderr, "Failed to submit graph: %s\n", hagResultToString(result));
        hagDestroyGraph(device, graph);
        goto cleanup_event;
    }

    result = hagWaitForEvents(1, &event, 5000000000ULL);
    if (result != HAG_SUCCESS) {
        fprintf(stderr, "Failed to wait for graph event: %s\n", hagResultToString(result));
        hagDestroyGraph(device, graph);
        goto cleanup_event;
    }

    printf("Graph executed successfully\n");

    /* Verify graph results */
    result = hagMapBuffer(device, bufferC, &mappedData);
    if (result == HAG_SUCCESS && mappedData) {
        float* floats = (float*)mappedData;
        printf("\nFirst 10 values in buffer C after graph execution:\n");
        for (int i = 0; i < 10 && i < ARRAY_SIZE; i++) {
            printf("  [%d] = %f\n", i, floats[i]);
        }
        hagUnmapBuffer(device, bufferC);
    }

    hagDestroyGraph(device, graph);
    printf("\nSample completed successfully!\n");

cleanup_event:
    hagDestroyEvent(device, event);
cleanup_cmd:
    hagDestroyCmdBuffer(device, cmdBuffer);
cleanup_buffers:
    hagDestroyBuffer(device, bufferC);
cleanup_buffer_b:
    hagDestroyBuffer(device, bufferB);
cleanup_buffer_a:
    hagDestroyBuffer(device, bufferA);
cleanup_device:
    hagDestroyDevice(device);
    hagDestroyInstance(instance);

    return 0;
}
