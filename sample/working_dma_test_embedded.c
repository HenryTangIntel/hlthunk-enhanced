#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <assert.h>
#include <stdio.h>

struct config_iterator;

/**
 * struct config_limit -  config limits
 * @start: the first valid configuration value.
 * @end: the last valid configuration value.
 */
struct config_limit {
	size_t start;
	size_t end;
};

/**
 * config_iterator_init() - initializes the config iterator.
 * @limits: an array of limits, with the upper and lower limit of each config.
 * @limits_len: the length of the limits array.
 * Returns: a pointer to the config iterator.
 *
 * LIFETIME: `limits` is stored inside the iterator, so it must outlive the iterator.
 */
struct config_iterator *config_iterator_init(const struct config_limit *limits, size_t limits_len);

/**
 * config_iterator_destroy() - destroys the config iterator.
 * @config_iterator: the config iterator to destroy.
 */
void config_iterator_destroy(struct config_iterator *config_iterator);

/**
 * config_iterator_first() - Returns the current state of the config without advancing the state.
 * @config_iterator: the config iterator to get config from.
 * Returns: a pointer to the current configuration, NULL if reached the end of the iterator.
 */
const size_t *config_iterator_current(struct config_iterator *config_iterator);

/**
 * config_iterator_next() - Advances the state and returns the new state of the config.
 * @config_iterator: the config iterator advance.
 * Returns: a pointer to the current configuration, NULL if reached the end of the iterator.
 */
const size_t *config_iterator_next(struct config_iterator *config_iterator);


#include "../include/uapi/hlthunk.h"
#include "../include/uapi/hlthunk_tests.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <time.h>

/**
 * struct config_iterator -  config iterator data structure.
 * @limit: an array of limits of the config iterator.
 * @configs: current state of the configurations.
 * @configs_len: the lengths of the configurations array.
 * @is_exhausted: is the iterator exhausted.
 */
struct config_iterator {
	const struct config_limit *limits;
	size_t *configs;
	size_t configs_len;
	bool is_exhausted;
};

struct config_iterator *config_iterator_init(const struct config_limit *limits, size_t limits_len)
{
	/* Validate limits */
	for (size_t i = 0; i < limits_len; i++) {
		if (limits[i].end < limits[i].start) {
			W("Config at index %zu doesn't satisfy end >= start, end: %zu, start: %zu",
			  i, limits[i].end, limits[i].start);
			return NULL;
		}
	}

	struct config_iterator *iter = calloc(1, sizeof(struct config_iterator));

	if (!iter)
		return NULL;

	iter->configs = calloc(limits_len, sizeof(*iter->configs));
	if (!iter->configs)
		goto configs_alloc_failed;

	iter->limits = limits;
	iter->configs_len = limits_len;
	iter->is_exhausted = false;

	for (size_t i = 0; i < limits_len; i++)
		iter->configs[i] = iter->limits[i].start;

	return iter;

configs_alloc_failed:
	free(iter);
	return NULL;
}

void config_iterator_destroy(struct config_iterator *iter)
{
	free(iter->configs);
	free(iter);
}

const size_t *config_iterator_current(struct config_iterator *iter)
{
	if (iter->is_exhausted)
		return NULL;

	return iter->configs;
}

const size_t *config_iterator_next(struct config_iterator *iter)
{
	for (size_t i = 0; i < iter->configs_len; i++) {
		iter->configs[i]++;

		if (iter->configs[i] <= iter->limits[i].end)
			return iter->configs;

		iter->configs[i] = iter->limits[i].start;
	}

	iter->is_exhausted = true;

	return NULL;
}



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
    uint64_t device_va = 0;
    void *device_addr = NULL;
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

    memset(host_ptr, 0xAA, TEST_SIZE);

    printf("  Getting device VA for host pointer...\n");
    device_va = hltests_get_device_va_for_host_ptr(fd, host_ptr);
    if (device_va == 0) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "Failed to get device VA for host pointer");
        goto cleanup;
    }

    printf("  Allocating device memory...\n");
    device_addr = hltests_allocate_device_mem(fd, TEST_SIZE, 0, false);
    if (!device_addr) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "Failed to allocate device memory");
        goto cleanup;
    }

    printf("  Performing H2D DMA transfer...\n");

    uint32_t qid = hltests_get_dma_down_qid(fd, STREAM0);

    clock_gettime(CLOCK_MONOTONIC, &start);

    rc = hltests_dma_transfer(fd,
                             qid,
                             EB_FALSE,
                             MB_TRUE,
                             device_va,
                             (uint64_t)device_addr,
                             TEST_SIZE,
                             DMA_DIR_HOST_TO_DRAM);

    clock_gettime(CLOCK_MONOTONIC, &end);

    if (rc != 0) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "DMA transfer failed with rc=%d", rc);
        goto cleanup_device;
    }

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
    uint64_t device_va = 0;
    void *device_addr = NULL;
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

    memset(host_ptr, 0x00, TEST_SIZE);

    printf("  Getting device VA for host pointer...\n");
    device_va = hltests_get_device_va_for_host_ptr(fd, host_ptr);
    if (device_va == 0) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "Failed to get device VA for host pointer");
        goto cleanup;
    }

    printf("  Allocating and initializing device memory...\n");
    device_addr = hltests_allocate_device_mem(fd, TEST_SIZE, 0, false);
    if (!device_addr) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "Failed to allocate device memory");
        goto cleanup;
    }

    printf("  Performing D2H DMA transfer...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    rc = hltests_dma_transfer(fd,
                             hltests_get_dma_up_qid(fd, STREAM0),
                             EB_FALSE,
                             MB_TRUE,
                             (uint64_t)device_addr,
                             device_va,
                             TEST_SIZE,
                             DMA_DIR_DRAM_TO_HOST);

    clock_gettime(CLOCK_MONOTONIC, &end);

    if (rc != 0) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "DMA transfer failed with rc=%d", rc);
        goto cleanup_device;
    }

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
    void *device_addr1 = NULL, *device_addr2 = NULL;
    struct timespec start, end;
    double time_sec, bandwidth;
    int rc;

    printf("  Allocating source device memory...\n");
    device_addr1 = hltests_allocate_device_mem(fd, TEST_SIZE, 0, false);
    if (!device_addr1) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "Failed to allocate source device memory");
        return result;
    }

    printf("  Allocating destination device memory...\n");
    device_addr2 = hltests_allocate_device_mem(fd, TEST_SIZE, 0, false);
    if (!device_addr2) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "Failed to allocate destination device memory");
        goto cleanup_src;
    }

    printf("  Performing D2D DMA transfer...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    rc = hltests_dma_transfer(fd,
                             hltests_get_dma_down_qid(fd, STREAM0),
                             EB_FALSE,
                             MB_TRUE,
                             (uint64_t)device_addr1,
                             (uint64_t)device_addr2,
                             TEST_SIZE,
                             DMA_DIR_DRAM_TO_DRAM);

    clock_gettime(CLOCK_MONOTONIC, &end);

    if (rc != 0) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "DMA transfer failed with rc=%d", rc);
        goto cleanup_both;
    }

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

int main(void) {
    struct hltests_state *tests_state;
    void *state;
    int fd, rc;
    struct test_result h2d_result, d2h_result, d2d_result;

    printf("Self-Contained HL-thunk DMA Test\n");
    printf("=================================\n\n");

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

    printf("Test setup successful. Device opened with fd: %d\n\n", fd);

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