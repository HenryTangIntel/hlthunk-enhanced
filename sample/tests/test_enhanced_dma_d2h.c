#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../enhanced_dma.h"
#include "../enhanced_dma_internal.h"

/* Mock device handle for testing */
static void* mock_device_handle = (void*)0x12345678;

/* Test buffer sizes */
#define TEST_BUFFER_SIZE_SMALL  64
#define TEST_BUFFER_SIZE_MEDIUM 1024
#define TEST_BUFFER_SIZE_LARGE  (1024 * 1024)

/* Mock device memory addresses */
#define MOCK_DEVICE_DRAM_ADDR   0x40000000UL
#define MOCK_DEVICE_SRAM_ADDR   0x50000000UL

/*
 * Test fixture setup - allocate test buffers
 */
static int setup_d2h_tests(void **state) {
    void *host_buffer = malloc(TEST_BUFFER_SIZE_LARGE);
    if (!host_buffer) {
        return -1;
    }
    
    /* Initialize buffer with zeros for receiving data */
    memset(host_buffer, 0x00, TEST_BUFFER_SIZE_LARGE);
    
    *state = host_buffer;
    return 0;
}

/*
 * Test fixture teardown - free test buffers
 */
static int teardown_d2h_tests(void **state) {
    free(*state);
    return 0;
}

/*
 * Test enhanced_dma_sync_d2h with valid parameters (DRAM source)
 */
static void test_d2h_valid_parameters_dram(void **state) {
    void *host_buffer = *state;
    enhanced_dma_device_ptr_t device_addr = MOCK_DEVICE_DRAM_ADDR;
    
    /* This test will fail initially since function is not implemented */
    int result = enhanced_dma_sync_d2h(mock_device_handle, 
                                       device_addr,
                                       host_buffer,
                                       TEST_BUFFER_SIZE_SMALL,
                                       EDMA_FLAG_DEFAULT);
    
    /* Expected behavior: function should succeed with valid parameters */
    assert_int_equal(result, EDMA_SUCCESS);
}

/*
 * Test enhanced_dma_sync_d2h with valid parameters (SRAM source)
 */
static void test_d2h_valid_parameters_sram(void **state) {
    void *host_buffer = *state;
    enhanced_dma_device_ptr_t device_addr = MOCK_DEVICE_SRAM_ADDR;
    
    int result = enhanced_dma_sync_d2h(mock_device_handle, 
                                       device_addr,
                                       host_buffer,
                                       TEST_BUFFER_SIZE_SMALL,
                                       EDMA_FLAG_DEFAULT);
    
    /* Expected behavior: function should succeed with SRAM source */
    assert_int_equal(result, EDMA_SUCCESS);
}

/*
 * Test enhanced_dma_sync_d2h with NULL device handle
 */
static void test_d2h_null_device_handle(void **state) {
    void *host_buffer = *state;
    enhanced_dma_device_ptr_t device_addr = MOCK_DEVICE_DRAM_ADDR;
    
    int result = enhanced_dma_sync_d2h(NULL,
                                       device_addr,
                                       host_buffer,
                                       TEST_BUFFER_SIZE_SMALL,
                                       EDMA_FLAG_DEFAULT);
    
    /* Expected behavior: should return EDMA_INVALID_PARAM for NULL handle */
    assert_int_equal(result, EDMA_INVALID_PARAM);
}

/*
 * Test enhanced_dma_sync_d2h with NULL host pointer
 */
static void test_d2h_null_host_pointer(void **state) {
    (void) state; /* unused */
    enhanced_dma_device_ptr_t device_addr = MOCK_DEVICE_DRAM_ADDR;
    
    int result = enhanced_dma_sync_d2h(mock_device_handle,
                                       device_addr,
                                       NULL,
                                       TEST_BUFFER_SIZE_SMALL,
                                       EDMA_FLAG_DEFAULT);
    
    /* Expected behavior: should return EDMA_INVALID_PARAM for NULL host pointer */
    assert_int_equal(result, EDMA_INVALID_PARAM);
}

/*
 * Test enhanced_dma_sync_d2h with zero size
 */
static void test_d2h_zero_size(void **state) {
    void *host_buffer = *state;
    enhanced_dma_device_ptr_t device_addr = MOCK_DEVICE_DRAM_ADDR;
    
    int result = enhanced_dma_sync_d2h(mock_device_handle,
                                       device_addr,
                                       host_buffer,
                                       0,
                                       EDMA_FLAG_DEFAULT);
    
    /* Expected behavior: should return EDMA_INVALID_PARAM for zero size */
    assert_int_equal(result, EDMA_INVALID_PARAM);
}

/*
 * Test enhanced_dma_sync_d2h with size too large
 */
static void test_d2h_size_too_large(void **state) {
    void *host_buffer = *state;
    enhanced_dma_device_ptr_t device_addr = MOCK_DEVICE_DRAM_ADDR;
    enhanced_dma_size_t huge_size = EDMA_MAX_TRANSFER_SIZE + 1;
    
    int result = enhanced_dma_sync_d2h(mock_device_handle,
                                       device_addr,
                                       host_buffer,
                                       huge_size,
                                       EDMA_FLAG_DEFAULT);
    
    /* Expected behavior: should return EDMA_INVALID_PARAM for oversized transfer */
    assert_int_equal(result, EDMA_INVALID_PARAM);
}

/*
 * Test enhanced_dma_sync_d2h with invalid device address
 */
static void test_d2h_invalid_device_address(void **state) {
    void *host_buffer = *state;
    enhanced_dma_device_ptr_t invalid_addr = 0x0; /* Invalid device address */
    
    int result = enhanced_dma_sync_d2h(mock_device_handle,
                                       invalid_addr,
                                       host_buffer,
                                       TEST_BUFFER_SIZE_SMALL,
                                       EDMA_FLAG_DEFAULT);
    
    /* Expected behavior: should return EDMA_MEMORY_ERROR for invalid device address */
    assert_int_equal(result, EDMA_MEMORY_ERROR);
}

/*
 * Test enhanced_dma_sync_d2h with different buffer sizes
 */
static void test_d2h_various_sizes(void **state) {
    void *host_buffer = *state;
    enhanced_dma_device_ptr_t device_addr = MOCK_DEVICE_DRAM_ADDR;
    
    /* Test small transfer */
    int result = enhanced_dma_sync_d2h(mock_device_handle,
                                       device_addr,
                                       host_buffer,
                                       TEST_BUFFER_SIZE_SMALL,
                                       EDMA_FLAG_DEFAULT);
    assert_int_equal(result, EDMA_SUCCESS);
    
    /* Test medium transfer */
    result = enhanced_dma_sync_d2h(mock_device_handle,
                                   device_addr,
                                   host_buffer,
                                   TEST_BUFFER_SIZE_MEDIUM,
                                   EDMA_FLAG_DEFAULT);
    assert_int_equal(result, EDMA_SUCCESS);
    
    /* Test large transfer */
    result = enhanced_dma_sync_d2h(mock_device_handle,
                                   device_addr,
                                   host_buffer,
                                   TEST_BUFFER_SIZE_LARGE,
                                   EDMA_FLAG_DEFAULT);
    assert_int_equal(result, EDMA_SUCCESS);
}

/*
 * Test enhanced_dma_sync_d2h with validation flag
 */
static void test_d2h_with_validation_flag(void **state) {
    void *host_buffer = *state;
    enhanced_dma_device_ptr_t device_addr = MOCK_DEVICE_DRAM_ADDR;
    
    int result = enhanced_dma_sync_d2h(mock_device_handle,
                                       device_addr,
                                       host_buffer,
                                       TEST_BUFFER_SIZE_SMALL,
                                       EDMA_FLAG_VALIDATE);
    
    /* Expected behavior: should succeed with validation flag */
    assert_int_equal(result, EDMA_SUCCESS);
}

/*
 * Test enhanced_dma_sync_d2h with performance logging flag
 */
static void test_d2h_with_performance_flag(void **state) {
    void *host_buffer = *state;
    enhanced_dma_device_ptr_t device_addr = MOCK_DEVICE_DRAM_ADDR;
    
    int result = enhanced_dma_sync_d2h(mock_device_handle,
                                       device_addr,
                                       host_buffer,
                                       TEST_BUFFER_SIZE_SMALL,
                                       EDMA_FLAG_LOG_PERF);
    
    /* Expected behavior: should succeed with performance logging flag */
    assert_int_equal(result, EDMA_SUCCESS);
}

/*
 * Test enhanced_dma_sync_d2h with combined flags
 */
static void test_d2h_with_combined_flags(void **state) {
    void *host_buffer = *state;
    enhanced_dma_device_ptr_t device_addr = MOCK_DEVICE_DRAM_ADDR;
    enhanced_dma_flags_t combined_flags = EDMA_FLAG_VALIDATE | EDMA_FLAG_LOG_PERF;
    
    int result = enhanced_dma_sync_d2h(mock_device_handle,
                                       device_addr,
                                       host_buffer,
                                       TEST_BUFFER_SIZE_SMALL,
                                       combined_flags);
    
    /* Expected behavior: should succeed with combined flags */
    assert_int_equal(result, EDMA_SUCCESS);
}

/*
 * Test enhanced_dma_sync_d2h with skip validation flag
 */
static void test_d2h_with_skip_validation_flag(void **state) {
    void *host_buffer = *state;
    enhanced_dma_device_ptr_t device_addr = MOCK_DEVICE_DRAM_ADDR;
    
    int result = enhanced_dma_sync_d2h(mock_device_handle,
                                       device_addr,
                                       host_buffer,
                                       TEST_BUFFER_SIZE_SMALL,
                                       EDMA_FLAG_SKIP_VALIDATION);
    
    /* Expected behavior: should succeed with skip validation flag */
    assert_int_equal(result, EDMA_SUCCESS);
}

/*
 * Test enhanced_dma_sync_d2h with device memory regions (boundary testing)
 */
static void test_d2h_memory_boundary_conditions(void **state) {
    void *host_buffer = *state;
    
    /* Test minimum valid device address */
    enhanced_dma_device_ptr_t min_addr = 0x1;
    int result = enhanced_dma_sync_d2h(mock_device_handle,
                                       min_addr,
                                       host_buffer,
                                       TEST_BUFFER_SIZE_SMALL,
                                       EDMA_FLAG_DEFAULT);
    assert_int_equal(result, EDMA_SUCCESS);
    
    /* Test high device address */
    enhanced_dma_device_ptr_t high_addr = 0xFFFFFFFFUL;
    result = enhanced_dma_sync_d2h(mock_device_handle,
                                   high_addr,
                                   host_buffer,
                                   TEST_BUFFER_SIZE_SMALL,
                                   EDMA_FLAG_DEFAULT);
    assert_int_equal(result, EDMA_SUCCESS);
}

/*
 * Main test runner for D2H tests
 */
int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_d2h_valid_parameters_dram, 
                                        setup_d2h_tests, teardown_d2h_tests),
        cmocka_unit_test_setup_teardown(test_d2h_valid_parameters_sram,
                                        setup_d2h_tests, teardown_d2h_tests),
        cmocka_unit_test_setup_teardown(test_d2h_null_device_handle,
                                        setup_d2h_tests, teardown_d2h_tests),
        cmocka_unit_test_setup_teardown(test_d2h_null_host_pointer,
                                        setup_d2h_tests, teardown_d2h_tests),
        cmocka_unit_test_setup_teardown(test_d2h_zero_size,
                                        setup_d2h_tests, teardown_d2h_tests),
        cmocka_unit_test_setup_teardown(test_d2h_size_too_large,
                                        setup_d2h_tests, teardown_d2h_tests),
        cmocka_unit_test_setup_teardown(test_d2h_invalid_device_address,
                                        setup_d2h_tests, teardown_d2h_tests),
        cmocka_unit_test_setup_teardown(test_d2h_various_sizes,
                                        setup_d2h_tests, teardown_d2h_tests),
        cmocka_unit_test_setup_teardown(test_d2h_with_validation_flag,
                                        setup_d2h_tests, teardown_d2h_tests),
        cmocka_unit_test_setup_teardown(test_d2h_with_performance_flag,
                                        setup_d2h_tests, teardown_d2h_tests),
        cmocka_unit_test_setup_teardown(test_d2h_with_combined_flags,
                                        setup_d2h_tests, teardown_d2h_tests),
        cmocka_unit_test_setup_teardown(test_d2h_with_skip_validation_flag,
                                        setup_d2h_tests, teardown_d2h_tests),
        cmocka_unit_test_setup_teardown(test_d2h_memory_boundary_conditions,
                                        setup_d2h_tests, teardown_d2h_tests),
    };

    printf("Running Enhanced DMA D2H Transfer Tests...\n");
    return cmocka_run_group_tests(tests, NULL, NULL);
}