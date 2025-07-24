#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <stdint.h>

#include "../include/uapi/hlthunk.h"
#include "../include/uapi/hlthunk_tests.h"

#define TEST_SIZE_KB    (64)
#define TEST_SIZE       (TEST_SIZE_KB * 1024)

// Logging macros
#define __STRINGIFY(x_) #x_
#define STRINGIFY(x_) __STRINGIFY(x_)
#define LOG(level_, format_, ...) \
    printf("[" level_ "] %s:" STRINGIFY(__LINE__) ": " format_ "\n", __func__, ##__VA_ARGS__)
#define I(format_, ...) LOG("INF", format_, ##__VA_ARGS__)
#define W(format_, ...) LOG("WRN", format_, ##__VA_ARGS__)
#define ERR(format_, ...) LOG("ERR", format_, ##__VA_ARGS__)

struct test_result {
    int success;
    double duration_ms;
    double bandwidth_mbps;
};

static void print_test_result(const char *test_name, const struct test_result *result)
{
    printf("  %s: %s", test_name, result->success ? "PASS" : "FAIL");
    if (result->success) {
        printf(" (%.2f ms, %.2f MB/s)", result->duration_ms, result->bandwidth_mbps);
    }
    printf("\n");
}

static double calculate_bandwidth_mbps(uint32_t size_bytes, double duration_ms)
{
    if (duration_ms <= 0.0) return 0.0;
    return (size_bytes / (1024.0 * 1024.0)) / (duration_ms / 1000.0);
}

static struct test_result test_host_to_device(int fd, void *host_ptr)
{
    struct test_result result = {0};
    struct timespec start_time, end_time;
    void *device_addr;
    uint64_t host_device_va;
    int rc;

    I("Allocating device memory (%u bytes)", TEST_SIZE);
    device_addr = hltests_allocate_device_mem(fd, TEST_SIZE, 0, NOT_CONTIGUOUS);
    if (!device_addr) {
        ERR("Failed to allocate device memory");
        return result;
    }

    I("Getting device VA for host pointer");
    host_device_va = hltests_get_device_va_for_host_ptr(fd, host_ptr);
    if (!host_device_va) {
        ERR("Failed to get device VA for host pointer");
        hltests_free_device_mem(fd, device_addr);
        return result;
    }

    I("Starting H2D DMA transfer");
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    rc = hltests_dma_transfer(fd, 
                              hltests_get_dma_down_qid(fd, STREAM0),
                              EB_FALSE, MB_TRUE, 
                              host_device_va, (uint64_t)device_addr, TEST_SIZE,
                              DMA_DIR_HOST_TO_DRAM);

    clock_gettime(CLOCK_MONOTONIC, &end_time);

    if (rc) {
        ERR("H2D DMA transfer failed: %d", rc);
        hltests_free_device_mem(fd, device_addr);
        return result;
    }

    double duration_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                        (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;

    result.success = 1;
    result.duration_ms = duration_ms;
    result.bandwidth_mbps = calculate_bandwidth_mbps(TEST_SIZE, duration_ms);

    I("H2D DMA completed successfully");
    hltests_free_device_mem(fd, device_addr);
    return result;
}

static struct test_result test_device_to_host(int fd, void *host_ptr)
{
    struct test_result result = {0};
    struct timespec start_time, end_time;
    void *device_addr;
    uint64_t host_device_va;
    int rc;

    I("Allocating device memory (%u bytes)", TEST_SIZE);
    device_addr = hltests_allocate_device_mem(fd, TEST_SIZE, 0, NOT_CONTIGUOUS);
    if (!device_addr) {
        ERR("Failed to allocate device memory");
        return result;
    }

    I("Getting device VA for host pointer");
    host_device_va = hltests_get_device_va_for_host_ptr(fd, host_ptr);
    if (!host_device_va) {
        ERR("Failed to get device VA for host pointer");
        hltests_free_device_mem(fd, device_addr);
        return result;
    }

    I("Starting D2H DMA transfer");
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    rc = hltests_dma_transfer(fd,
                              hltests_get_dma_up_qid(fd, STREAM0),
                              EB_FALSE, MB_TRUE,
                              (uint64_t)device_addr, host_device_va, TEST_SIZE,
                              DMA_DIR_DRAM_TO_HOST);

    clock_gettime(CLOCK_MONOTONIC, &end_time);

    if (rc) {
        ERR("D2H DMA transfer failed: %d", rc);
        hltests_free_device_mem(fd, device_addr);
        return result;
    }

    double duration_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                        (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;

    result.success = 1;
    result.duration_ms = duration_ms;
    result.bandwidth_mbps = calculate_bandwidth_mbps(TEST_SIZE, duration_ms);

    I("D2H DMA completed successfully");
    hltests_free_device_mem(fd, device_addr);
    return result;
}

static struct test_result test_device_to_device(int fd)
{
    struct test_result result = {0};
    struct timespec start_time, end_time;
    void *device_addr1, *device_addr2;
    int rc;

    I("Allocating source device memory (%u bytes)", TEST_SIZE);
    device_addr1 = hltests_allocate_device_mem(fd, TEST_SIZE, 0, NOT_CONTIGUOUS);
    if (!device_addr1) {
        ERR("Failed to allocate source device memory");
        return result;
    }

    I("Allocating destination device memory (%u bytes)", TEST_SIZE);
    device_addr2 = hltests_allocate_device_mem(fd, TEST_SIZE, 0, NOT_CONTIGUOUS);
    if (!device_addr2) {
        ERR("Failed to allocate destination device memory");
        hltests_free_device_mem(fd, device_addr1);
        return result;
    }

    I("Starting D2D DMA transfer");
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    rc = hltests_dma_transfer(fd,
                              hltests_get_ddma_qid(fd, STREAM0, 0),
                              EB_FALSE, MB_TRUE,
                              (uint64_t)device_addr1, (uint64_t)device_addr2, TEST_SIZE,
                              DMA_DIR_DRAM_TO_DRAM);

    clock_gettime(CLOCK_MONOTONIC, &end_time);

    if (rc) {
        ERR("D2D DMA transfer failed: %d", rc);
        hltests_free_device_mem(fd, device_addr2);
        hltests_free_device_mem(fd, device_addr1);
        return result;
    }

    double duration_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                        (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;

    result.success = 1;
    result.duration_ms = duration_ms;
    result.bandwidth_mbps = calculate_bandwidth_mbps(TEST_SIZE, duration_ms);

    I("D2D DMA completed successfully");
    hltests_free_device_mem(fd, device_addr2);
    hltests_free_device_mem(fd, device_addr1);
    return result;
}

int main(void)
{
    struct hltests_state *tests_state;
    void *state;
    int fd, rc;
    struct test_result h2d_result, d2h_result, d2d_result;
    void *host_ptr;

    printf("Refactored HL-thunk DMA Test\n");
    printf("============================\n\n");

    I("Initializing test framework");
    rc = hltests_init();
    if (rc) {
        ERR("Failed to initialize hlthunk tests library: %d", rc);
        return -1;
    }

    I("Setting up test state");
    rc = hltests_setup(&state);
    if (rc) {
        ERR("Failed to run setup phase of hlthunk tests: %d", rc);
        hltests_fini();
        return -1;
    }

    tests_state = (struct hltests_state *) state;
    fd = tests_state->fd;

    I("Test setup successful. Device opened with fd: %d", fd);

    I("Allocating host memory (%u bytes)", TEST_SIZE);
    host_ptr = hltests_allocate_host_mem(fd, TEST_SIZE, NOT_HUGE_MAP);
    if (!host_ptr) {
        ERR("Failed to allocate host memory");
        hltests_teardown(&state);
        hltests_fini();
        return -1;
    }

    // Initialize host memory with test pattern
    memset(host_ptr, 0xAA, TEST_SIZE);

    // Test H2D (Host to Device)
    printf("\nTesting Host-to-Device (H2D) DMA (%d KB)...\n", TEST_SIZE_KB);
    h2d_result = test_host_to_device(fd, host_ptr);
    print_test_result("H2D DMA", &h2d_result);

    // Test D2H (Device to Host)  
    printf("\nTesting Device-to-Host (D2H) DMA (%d KB)...\n", TEST_SIZE_KB);
    d2h_result = test_device_to_host(fd, host_ptr);
    print_test_result("D2H DMA", &d2h_result);

    // Test D2D (Device to Device)
    printf("\nTesting Device-to-Device (D2D) DMA (%d KB)...\n", TEST_SIZE_KB);
    d2d_result = test_device_to_device(fd);
    print_test_result("D2D DMA", &d2d_result);

    // Summary
    printf("\n=== SUMMARY ===\n");
    print_test_result("H2D (Host-to-Device)", &h2d_result);
    print_test_result("D2H (Device-to-Host)", &d2h_result);
    print_test_result("D2D (Device-to-Device)", &d2d_result);

    int tests_passed = h2d_result.success + d2h_result.success + d2d_result.success;
    printf("\nTotal: %d/3 tests passed\n", tests_passed);

    // Cleanup
    I("Cleaning up resources");
    hltests_free_host_mem(fd, host_ptr);
    hltests_teardown(&state);
    hltests_fini();

    return (tests_passed == 3) ? 0 : -1;
}