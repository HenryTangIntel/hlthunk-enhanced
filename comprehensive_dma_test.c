#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "include/uapi/hlthunk.h"

#define TEST_SIZE_KB  64  // 64KB test size
#define TEST_SIZE     (TEST_SIZE_KB * 1024)

struct test_result {
    int success;
    double bandwidth_gbps;
    char error_msg[256];
};

void print_test_result(const char* test_name, struct test_result* result) {
    printf("%s: ", test_name);
    if (result->success) {
        printf("PASSED - %.2f GB/s\n", result->bandwidth_gbps);
    } else {
        printf("FAILED - %s\n", result->error_msg);
    }
}

struct test_result test_host_to_device(int fd, struct hlthunk_hw_ip_info* hw_ip) {
    struct test_result result = {0};
    void *host_ptr = NULL;
    uint64_t device_va = 0, device_addr = 0;
    struct timespec start, end;
    double time_sec, bandwidth;
    int rc;
    
    printf("  Allocating host memory...\n");
    host_ptr = hlthunk_malloc(TEST_SIZE);
    if (!host_ptr) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "Failed to allocate host memory");
        return result;
    }
    
    // Fill with test pattern
    memset(host_ptr, 0xAA, TEST_SIZE);
    
    printf("  Getting device VA for host pointer...\n");
    device_va = hlthunk_get_device_va_for_host_ptr(fd, host_ptr);
    if (device_va == ULLONG_MAX) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "Failed to get device VA for host pointer");
        goto cleanup;
    }
    
    printf("  Allocating device memory...\n");
    device_addr = hlthunk_device_memory_alloc(fd, TEST_SIZE, 0, false, false);
    if (device_addr == ULLONG_MAX) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "Failed to allocate device memory");
        goto cleanup;
    }
    
    printf("  Performing H2D DMA transfer...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    rc = hlthunk_dma_transfer(fd, 
                             0,  // queue index
                             true,  // is_memset
                             false, // is_external_cb
                             device_va,    // src address (host)
                             device_addr,  // dst address (device)
                             TEST_SIZE,
                             false);  // is_sram
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    if (rc != 0) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "DMA transfer failed with rc=%d", rc);
        goto cleanup_device;
    }
    
    // Calculate bandwidth
    time_sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    bandwidth = (TEST_SIZE / (1024.0 * 1024.0 * 1024.0)) / time_sec;
    
    result.success = 1;
    result.bandwidth_gbps = bandwidth;
    
cleanup_device:
    hlthunk_device_memory_free(fd, device_addr, TEST_SIZE);
cleanup:
    hlthunk_free(host_ptr);
    return result;
}

struct test_result test_device_to_host(int fd, struct hlthunk_hw_ip_info* hw_ip) {
    struct test_result result = {0};
    void *host_ptr = NULL;
    uint64_t device_va = 0, device_addr = 0;
    struct timespec start, end;
    double time_sec, bandwidth;
    int rc;
    
    printf("  Allocating host memory...\n");
    host_ptr = hlthunk_malloc(TEST_SIZE);
    if (!host_ptr) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "Failed to allocate host memory");
        return result;
    }
    
    // Clear host memory
    memset(host_ptr, 0x00, TEST_SIZE);
    
    printf("  Getting device VA for host pointer...\n");
    device_va = hlthunk_get_device_va_for_host_ptr(fd, host_ptr);
    if (device_va == ULLONG_MAX) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "Failed to get device VA for host pointer");
        goto cleanup;
    }
    
    printf("  Allocating and initializing device memory...\n");
    device_addr = hlthunk_device_memory_alloc(fd, TEST_SIZE, 0, false, false);
    if (device_addr == ULLONG_MAX) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "Failed to allocate device memory");
        goto cleanup;
    }
    
    // Fill device memory with pattern (using memset operation)
    rc = hlthunk_memset(fd, device_addr, 0xBB, TEST_SIZE, false);
    if (rc != 0) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "Failed to initialize device memory");
        goto cleanup_device;
    }
    
    printf("  Performing D2H DMA transfer...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    rc = hlthunk_dma_transfer(fd,
                             0,  // queue index  
                             false, // is_memset
                             false, // is_external_cb
                             device_addr,  // src address (device)
                             device_va,    // dst address (host)
                             TEST_SIZE,
                             false);  // is_sram
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    if (rc != 0) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "DMA transfer failed with rc=%d", rc);
        goto cleanup_device;
    }
    
    // Calculate bandwidth
    time_sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    bandwidth = (TEST_SIZE / (1024.0 * 1024.0 * 1024.0)) / time_sec;
    
    result.success = 1;
    result.bandwidth_gbps = bandwidth;
    
cleanup_device:
    hlthunk_device_memory_free(fd, device_addr, TEST_SIZE);
cleanup:
    hlthunk_free(host_ptr);
    return result;
}

struct test_result test_device_to_device(int fd, struct hlthunk_hw_ip_info* hw_ip) {
    struct test_result result = {0};
    uint64_t device_addr1 = 0, device_addr2 = 0;
    struct timespec start, end;
    double time_sec, bandwidth;
    int rc;
    
    printf("  Allocating source device memory...\n");
    device_addr1 = hlthunk_device_memory_alloc(fd, TEST_SIZE, 0, false, false);
    if (device_addr1 == ULLONG_MAX) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "Failed to allocate source device memory");
        return result;
    }
    
    printf("  Allocating destination device memory...\n");
    device_addr2 = hlthunk_device_memory_alloc(fd, TEST_SIZE, 0, false, false);
    if (device_addr2 == ULLONG_MAX) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "Failed to allocate destination device memory");
        goto cleanup_src;
    }
    
    printf("  Initializing source device memory...\n");
    rc = hlthunk_memset(fd, device_addr1, 0xCC, TEST_SIZE, false);
    if (rc != 0) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "Failed to initialize source device memory");
        goto cleanup_both;
    }
    
    printf("  Performing D2D DMA transfer...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    rc = hlthunk_dma_transfer(fd,
                             0,  // queue index
                             false, // is_memset  
                             false, // is_external_cb
                             device_addr1,  // src address (device)
                             device_addr2,  // dst address (device)
                             TEST_SIZE,
                             false);  // is_sram
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    if (rc != 0) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "DMA transfer failed with rc=%d", rc);
        goto cleanup_both;
    }
    
    // Calculate bandwidth
    time_sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    bandwidth = (TEST_SIZE / (1024.0 * 1024.0 * 1024.0)) / time_sec;
    
    result.success = 1;
    result.bandwidth_gbps = bandwidth;
    
cleanup_both:
    hlthunk_device_memory_free(fd, device_addr2, TEST_SIZE);
cleanup_src:
    hlthunk_device_memory_free(fd, device_addr1, TEST_SIZE);
    return result;
}

int main(int argc, char *argv[]) {
    int fd, rc;
    struct hlthunk_hw_ip_info hw_ip;
    struct test_result h2d_result, d2h_result, d2d_result;
    
    printf("Comprehensive HL-thunk DMA Test\n");
    printf("===============================\n\n");
    
    printf("Opening Habana device...\n");
    fd = hlthunk_open(HLTHUNK_DEVICE_DONT_CARE, NULL);
    if (fd < 0) {
        printf("Failed to open device: %s\n", strerror(errno));
        return -1;
    }
    
    printf("Getting hardware information...\n");
    rc = hlthunk_get_hw_ip_info(fd, &hw_ip);
    if (rc != 0) {
        printf("Failed to get hardware info: %d\n", rc);
        hlthunk_close(fd);
        return -1;
    }
    
    printf("Device: Type=%d, SRAM=%u MB, DRAM=%lu MB\n\n", 
           hw_ip.device_id, 
           hw_ip.sram_size / (1024 * 1024),
           hw_ip.dram_size / (1024 * 1024));
    
    // Test H2D (Host to Device)
    printf("Testing Host-to-Device (H2D) DMA (%d KB)...\n", TEST_SIZE_KB);
    h2d_result = test_host_to_device(fd, &hw_ip);
    print_test_result("H2D DMA", &h2d_result);
    printf("\n");
    
    // Test D2H (Device to Host)
    printf("Testing Device-to-Host (D2H) DMA (%d KB)...\n", TEST_SIZE_KB);
    d2h_result = test_device_to_host(fd, &hw_ip);
    print_test_result("D2H DMA", &d2h_result);
    printf("\n");
    
    // Test D2D (Device to Device)
    printf("Testing Device-to-Device (D2D) DMA (%d KB)...\n", TEST_SIZE_KB);
    d2d_result = test_device_to_device(fd, &hw_ip);
    print_test_result("D2D DMA", &d2d_result);
    printf("\n");
    
    // Summary
    printf("=== SUMMARY ===\n");
    print_test_result("H2D (Host-to-Device)", &h2d_result);
    print_test_result("D2H (Device-to-Host)", &d2h_result);
    print_test_result("D2D (Device-to-Device)", &d2d_result);
    
    int tests_passed = h2d_result.success + d2h_result.success + d2d_result.success;
    printf("\nTotal: %d/3 tests passed\n", tests_passed);
    
    hlthunk_close(fd);
    return (tests_passed == 3) ? 0 : -1;
}