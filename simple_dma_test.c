#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "include/uapi/hlthunk.h"

int test_basic_connectivity() {
    printf("Testing basic HL-thunk connectivity...\n");
    
    // Try to get device count for different device types
    int goya_count = hlthunk_get_device_count(HLTHUNK_DEVICE_GOYA);
    int gaudi_count = hlthunk_get_device_count(HLTHUNK_DEVICE_GAUDI);
    int gaudi2_count = hlthunk_get_device_count(HLTHUNK_DEVICE_GAUDI2);
    int gaudi3_count = hlthunk_get_device_count(HLTHUNK_DEVICE_GAUDI3);
    
    printf("Goya devices: %d\n", goya_count);
    printf("Gaudi devices: %d\n", gaudi_count);
    printf("Gaudi2 devices: %d\n", gaudi2_count);
    printf("Gaudi3 devices: %d\n", gaudi3_count);
    
    int total_devices = goya_count + gaudi_count + gaudi2_count + gaudi3_count;
    if (total_devices <= 0) {
        printf("No Habana devices found or driver not loaded\n");
        return -1;
    }
    
    printf("Total Habana devices found: %d\n", total_devices);
    return 0;
}

int test_device_open() {
    int fd, i;
    
    printf("\nTesting device open...\n");
    
    // Try to open a device
    for (i = 0; i < 10; i++) {
        fd = hlthunk_open(HLTHUNK_DEVICE_DONT_CARE, NULL);
        if (fd >= 0) {
            printf("Successfully opened device, fd: %d\n", fd);
            
            // Get device info
            struct hlthunk_hw_ip_info hw_ip;
            int rc = hlthunk_get_hw_ip_info(fd, &hw_ip);
            if (rc == 0) {
                printf("Device type: %d\n", hw_ip.device_id);
                printf("SRAM size: %llu MB\n", hw_ip.sram_size / (1024 * 1024));
                printf("DRAM size: %llu MB\n", hw_ip.dram_size / (1024 * 1024));
            }
            
            hlthunk_close(fd);
            return 0;
        }
    }
    
    printf("Failed to open any device\n");
    return -1;
}

int main(int argc, char *argv[]) {
    int rc;
    
    printf("Simple HL-thunk Connectivity Test\n");
    printf("==================================\n\n");
    
    // Test basic connectivity
    rc = test_basic_connectivity();
    if (rc != 0) {
        printf("Basic connectivity test failed.\n");
        // Don't exit - this might fail if no hardware is present
    }
    
    // Test device open 
    rc = test_device_open();
    if (rc != 0) {
        printf("Device open test failed.\n");
        // This is expected if no hardware is present
    }
    
    printf("\nConnectivity test completed!\n");
    printf("Note: Tests may fail if no Habana hardware is present in the system.\n");
    return 0;
}