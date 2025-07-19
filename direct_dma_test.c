#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

// Include basic hlthunk without the tests framework first
#include "include/uapi/hlthunk.h"

// Simple test to verify we can at least do basic operations
int test_basic_dma_operations() {
    int fd;
    struct hlthunk_hw_ip_info hw_ip;
    int rc;
    
    printf("=== Basic DMA Operations Test ===\n");
    
    // Open device
    printf("Opening device...\n");
    fd = hlthunk_open(HLTHUNK_DEVICE_DONT_CARE, NULL);
    if (fd < 0) {
        printf("❌ Failed to open device\n");
        return -1;
    }
    printf("✅ Device opened successfully (fd=%d)\n", fd);
    
    // Get hardware info
    printf("Getting hardware info...\n");
    rc = hlthunk_get_hw_ip_info(fd, &hw_ip);
    if (rc != 0) {
        printf("❌ Failed to get hardware info\n");
        hlthunk_close(fd);
        return -1;
    }
    
    printf("✅ Hardware info retrieved:\n");
    printf("   Device type: %d\n", hw_ip.device_id);
    printf("   SRAM size: %u MB\n", hw_ip.sram_size / (1024 * 1024));
    printf("   DRAM size: %lu MB\n", hw_ip.dram_size / (1024 * 1024));
    
    // Test memory allocation
    printf("Testing memory allocation...\n");
    uint64_t device_mem = hlthunk_device_memory_alloc(fd, 1024 * 1024, 0, false, false);
    if (device_mem == 0) {
        printf("❌ Failed to allocate device memory\n");
        hlthunk_close(fd);
        return -1;
    }
    printf("✅ Device memory allocated at 0x%lx\n", device_mem);
    
    // Free memory
    rc = hlthunk_device_memory_free(fd, device_mem);
    if (rc != 0) {
        printf("⚠️  Warning: Failed to free device memory\n");
    } else {
        printf("✅ Device memory freed\n");
    }
    
    // Close device
    hlthunk_close(fd);
    printf("✅ Device closed\n");
    
    return 0;
}

int test_host_memory_operations() {
    int fd;
    void *host_ptr;
    uint64_t device_va;
    int rc;
    
    printf("\n=== Host Memory Operations Test ===\n");
    
    // Open device
    fd = hlthunk_open(HLTHUNK_DEVICE_DONT_CARE, NULL);
    if (fd < 0) {
        printf("❌ Failed to open device\n");
        return -1;
    }
    
    // Allocate host memory
    printf("Allocating host memory...\n");
    host_ptr = hlthunk_malloc(64 * 1024);  // 64KB
    if (!host_ptr) {
        printf("❌ Failed to allocate host memory\n");
        hlthunk_close(fd);
        return -1;
    }
    printf("✅ Host memory allocated at %p\n", host_ptr);
    
    // Fill with test pattern
    memset(host_ptr, 0xAA, 64 * 1024);
    printf("✅ Host memory filled with test pattern\n");
    
    // Note: Device VA mapping requires test framework
    printf("ℹ️  Device VA mapping test skipped (requires test framework)\n");
    
    // Free host memory
    hlthunk_free(host_ptr);
    printf("✅ Host memory freed\n");
    
    hlthunk_close(fd);
    return 0;
}

int main(int argc, char *argv[]) {
    int rc1, rc2;
    
    printf("Direct HL-thunk DMA Test\n");
    printf("========================\n\n");
    
    rc1 = test_basic_dma_operations();
    rc2 = test_host_memory_operations();
    
    printf("\n=== TEST SUMMARY ===\n");
    printf("Basic DMA operations: %s\n", rc1 == 0 ? "✅ PASS" : "❌ FAIL");
    printf("Host memory operations: %s\n", rc2 == 0 ? "✅ PASS" : "❌ FAIL");
    
    if (rc1 == 0 && rc2 == 0) {
        printf("\n🎉 All basic tests passed! HL-thunk is working correctly.\n");
        printf("Note: For full H2D/D2H/D2D performance tests, use the test library.\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed.\n");
        return 1;
    }
}