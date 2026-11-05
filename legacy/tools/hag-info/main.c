/*
 * MIT License
 * Copyright (C) 2008 - 2026 Execute Software.  All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>

#include "hag/api/api.h"
#include "hag/utils/platform.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("Hag Device Enumeration Tool v%d.%d.%d\n",
           HAG_API_VERSION_MAJOR,
           HAG_API_VERSION_MINOR,
           HAG_API_VERSION_PATCH);
    printf("=====================================\n\n");

    /* Enumerate DRM devices */
    HagDrmDeviceInfo devices[16];
    int count = hagPlatformEnumerateDrmDevices(devices, 16);

    if (count == 0) {
        printf("No DRM render nodes found.\n");
        printf("Make sure you have GPU drivers installed and /dev/dri/renderD* exists.\n");
        return 0;
    }

    printf("Found %d DRM device(s):\n\n", count);

    for (int i = 0; i < count; i++) {
        const HagDrmDeviceInfo* dev = &devices[i];
        printf("Device %d:\n", i);
        printf("  Path:     %s\n", dev->path);
        printf("  Vendor:   0x%04X", dev->vendorId);

        /* Pretty-print known vendors */
        switch (dev->vendorId) {
            case 0x1002: printf(" (AMD)"); break;
            case 0x10DE: printf(" (NVIDIA)"); break;
            case 0x8086: printf(" (Intel)"); break;
            case 0x5143: printf(" (Qualcomm)"); break;
            case 0x13B5: printf(" (ARM)"); break;
            case 0x106B: printf(" (Apple)"); break;
            default: break;
        }
        printf("\n");
        printf("  Device:   0x%04X\n", dev->deviceId);
        printf("\n");
    }

    printf("Note: Full Hag device enumeration requires the Hag runtime.\n");
    printf("This tool shows raw DRM devices only.\n");

    return 0;
}
