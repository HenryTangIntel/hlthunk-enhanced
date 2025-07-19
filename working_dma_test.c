#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include "include/uapi/hlthunk.h"
#include "include/uapi/hlthunk_tests.h"

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

struct test_result test_host_to_device(int fd) {
    struct test_result result = {0};
    void *host_ptr = NULL;
    uint64_t device_va = 0, device_addr = 0;
    struct timespec start, end;
    double time_sec, bandwidth;
    int rc;
    
    printf("  Allocating host memory...\n");
    host_ptr = hltests_allocate_host_mem(fd, TEST_SIZE, NOT_HUGE_MAP);
    if (!host_ptr) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "Failed to allocate host memory");
        return result;
    }
    
    // Fill with test pattern
    memset(host_ptr, 0xAA, TEST_SIZE);
    
    printf("  Getting device VA for host pointer...\n");
    device_va = hltests_get_device_va_for_host_ptr(fd, host_ptr);
    if (device_va == 0) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "Failed to get device VA for host pointer");
        goto cleanup;
    }
    
    printf("  Allocating device memory...\n");
    device_addr = hltests_allocate_device_mem(fd, 0, TEST_SIZE, 0, false);
    if (device_addr == 0) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "Failed to allocate device memory");
        goto cleanup;
    }
    
    printf("  Performing H2D DMA transfer...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    rc = hltests_dma_transfer(fd, 
                             hltests_get_dma_down_qid(fd, STREAM0),  // queue index
                             EB_FALSE,  // eb
                             MB_TRUE,   // mb
                             device_va,    // src address (host)
                             device_addr,  // dst address (device)
                             TEST_SIZE,
                             DMA_DIR_HOST_TO_DRAM);  // direction
    
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
    hltests_free_device_mem(fd, device_addr);
cleanup:
    hltests_free_host_mem(fd, host_ptr);
    return result;
}

struct test_result test_device_to_host(int fd) {
    struct test_result result = {0};
    void *host_ptr = NULL;
    uint64_t device_va = 0, device_addr = 0;
    struct timespec start, end;
    double time_sec, bandwidth;
    int rc;
    
    printf("  Allocating host memory...\n");
    host_ptr = hltests_allocate_host_mem(fd, TEST_SIZE, NOT_HUGE_MAP);
    if (!host_ptr) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "Failed to allocate host memory");
        return result;
    }
    
    // Clear host memory
    memset(host_ptr, 0x00, TEST_SIZE);
    
    printf("  Getting device VA for host pointer...\n");
    device_va = hltests_get_device_va_for_host_ptr(fd, host_ptr);
    if (device_va == 0) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "Failed to get device VA for host pointer");
        goto cleanup;
    }
    
    printf("  Allocating and initializing device memory...\n");
    device_addr = hltests_allocate_device_mem(fd, 0, TEST_SIZE, 0, false);
    if (device_addr == 0) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "Failed to allocate device memory");
        goto cleanup;
    }
    
    // Fill device memory with pattern (using memset operation)
    rc = hltests_fill_device_mem(fd, device_addr, TEST_SIZE, 0xBB);
    if (rc != 0) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "Failed to initialize device memory");
        goto cleanup_device;
    }
    
    printf("  Performing D2H DMA transfer...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    rc = hltests_dma_transfer(fd,
                             hltests_get_dma_up_qid(fd, STREAM0),  // queue index  
                             EB_FALSE, // eb
                             MB_TRUE,  // mb
                             device_addr,  // src address (device)
                             device_va,    // dst address (host)
                             TEST_SIZE,
                             DMA_DIR_DRAM_TO_HOST);  // direction
    
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
    hltests_free_device_mem(fd, device_addr);
cleanup:
    hltests_free_host_mem(fd, host_ptr);
    return result;
}

struct test_result test_device_to_device(int fd) {
    struct test_result result = {0};
    uint64_t device_addr1 = 0, device_addr2 = 0;
    struct timespec start, end;
    double time_sec, bandwidth;
    int rc;
    
    printf("  Allocating source device memory...\n");
    device_addr1 = hltests_allocate_device_mem(fd, 0, TEST_SIZE, 0, false);
    if (device_addr1 == 0) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "Failed to allocate source device memory");
        return result;
    }
    
    printf("  Allocating destination device memory...\n");
    device_addr2 = hltests_allocate_device_mem(fd, 0, TEST_SIZE, 0, false);
    if (device_addr2 == 0) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "Failed to allocate destination device memory");
        goto cleanup_src;
    }
    
    printf("  Initializing source device memory...\n");
    rc = hltests_fill_device_mem(fd, device_addr1, TEST_SIZE, 0xCC);
    if (rc != 0) {
        snprintf(result.error_msg, sizeof(result.error_msg), 
                "Failed to initialize source device memory");
        goto cleanup_both;
    }
    
    printf("  Performing D2D DMA transfer...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    rc = hltests_dma_transfer(fd,
                             hltests_get_dma_down_qid(fd, STREAM0),  // queue index
                             EB_FALSE, // eb
                             MB_TRUE,  // mb
                             device_addr1,  // src address (device)
                             device_addr2,  // dst address (device)
                             TEST_SIZE,
                             DMA_DIR_DRAM_TO_DRAM);  // direction
    
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
    hltests_free_device_mem(fd, device_addr2);
cleanup_src:
    hltests_free_device_mem(fd, device_addr1);
    return result;
}

int main(int argc, char *argv[]) {
    struct hltests_state *tests_state;
    void *state;
    int fd, rc;
    struct test_result h2d_result, d2h_result, d2d_result;
    
    printf("Working HL-thunk DMA Test\n");
    printf("=========================\n\n");
    
    // Parse arguments (if any)
    hltests_parser(argc, (const char **)argv, NULL, HLTEST_DEVICE_MASK_DONT_CARE);
    
    printf("Initializing test framework...\n");
    rc = hltests_init();
    if (rc) {
        printf("Failed to initialize hlthunk tests library: %d\n", rc);
        return -1;
    }
    
    printf("Setting up test state...\n");
    rc = hltests_setup(&state);
    if (rc) {
        printf("Failed to run setup phase of hlthunk tests: %d\n", rc);
        hltests_fini();
        return -1;
    }
    
    tests_state = (struct hltests_state *) state;
    fd = tests_state->fd;
    
    printf("Test setup successful.\n");
    printf("Device opened with fd: %d\n\n", fd);
    
    // Test H2D (Host to Device)
    printf("Testing Host-to-Device (H2D) DMA (%d KB)...\n", TEST_SIZE_KB);
    h2d_result = test_host_to_device(fd);
    print_test_result("H2D DMA", &h2d_result);
    printf("\n");
    
    // Test D2H (Device to Host)
    printf("Testing Device-to-Host (D2H) DMA (%d KB)...\n", TEST_SIZE_KB);
    d2h_result = test_device_to_host(fd);
    print_test_result("D2H DMA", &d2h_result);
    printf("\n");
    
    // Test D2D (Device to Device)
    printf("Testing Device-to-Device (D2D) DMA (%d KB)...\n", TEST_SIZE_KB);
    d2d_result = test_device_to_device(fd);
    print_test_result("D2D DMA", &d2d_result);
    printf("\n");
    
    // Summary
    printf("=== SUMMARY ===\n");
    print_test_result("H2D (Host-to-Device)", &h2d_result);
    print_test_result("D2H (Device-to-Host)", &d2h_result);
    print_test_result("D2D (Device-to-Device)", &d2d_result);
    
    int tests_passed = h2d_result.success + d2h_result.success + d2d_result.success;
    printf("\nTotal: %d/3 tests passed\n", tests_passed);
    
    // Cleanup
    hltests_teardown(&state);
    hltests_fini();
    
    return (tests_passed == 3) ? 0 : -1;
}