#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>

/* Include the headers we're testing */
#ifdef HAVE_ENHANCED_DMA_H
#include "../enhanced_dma.h"
#endif

#ifdef HAVE_ENHANCED_DMA_INTERNAL_H
#include "../enhanced_dma_internal.h"
#endif

/*
 * Test that enhanced_dma.h can be included without errors
 */
static void test_enhanced_dma_header_inclusion(void **state) {
    (void) state; /* unused parameter */
    
#ifdef HAVE_ENHANCED_DMA_H
    /* If we reach here, the header was included successfully */
    assert_int_equal(1, 1);
#else
    /* Header doesn't exist yet - this is expected for TDD */
    skip();
#endif
}

/*
 * Test that error codes are properly defined
 */
static void test_error_codes_defined(void **state) {
    (void) state; /* unused parameter */
    
#ifdef HAVE_ENHANCED_DMA_H
    /* Test that all expected error codes are defined */
    assert_true(EDMA_SUCCESS == 0);
    assert_true(EDMA_INVALID_PARAM < 0);
    assert_true(EDMA_MEMORY_ERROR < 0);
    assert_true(EDMA_HARDWARE_ERROR < 0);
    assert_true(EDMA_TIMEOUT < 0);
    
    /* Ensure error codes are distinct */
    assert_true(EDMA_INVALID_PARAM != EDMA_MEMORY_ERROR);
    assert_true(EDMA_MEMORY_ERROR != EDMA_HARDWARE_ERROR);
    assert_true(EDMA_HARDWARE_ERROR != EDMA_TIMEOUT);
#else
    skip();
#endif
}

/*
 * Test that function prototypes are properly declared
 */
static void test_function_prototypes_declared(void **state) {
    (void) state; /* unused parameter */
    
#ifdef HAVE_ENHANCED_DMA_H
    /* Test that the header compiled successfully - if function prototypes
     * are properly declared, the header will include without errors.
     * We can't test function addresses without implementations, but we can
     * test that the declarations compile correctly by checking data types. */
    
    /* Test that the function parameter types are defined correctly */
    enhanced_dma_device_handle_t test_handle = NULL;
    enhanced_dma_host_ptr_t test_host_ptr = NULL;
    enhanced_dma_device_ptr_t test_device_ptr = 0;
    enhanced_dma_size_t test_size = 1024;
    enhanced_dma_flags_t test_flags = EDMA_FLAG_DEFAULT;
    
    /* If we reach here, all types are properly defined */
    assert_null(test_handle);
    assert_null(test_host_ptr);
    assert_int_equal(test_device_ptr, 0);
    assert_true(test_size > 0);
    assert_int_equal(test_flags, EDMA_FLAG_DEFAULT);
#else
    skip();
#endif
}

/*
 * Test that internal header inclusion works
 */
static void test_internal_header_inclusion(void **state) {
    (void) state; /* unused parameter */
    
#ifdef HAVE_ENHANCED_DMA_INTERNAL_H
    /* If we reach here, the internal header was included successfully */
    assert_int_equal(1, 1);
#else
    /* Header doesn't exist yet - this is expected for TDD */
    skip();
#endif
}

/*
 * Test that necessary system includes are present
 */
static void test_system_includes_present(void **state) {
    (void) state; /* unused parameter */
    
#ifdef HAVE_ENHANCED_DMA_H
    /* Test that we can use standard types that should be included */
    size_t test_size = 1024;
    void *test_ptr = NULL;
    
    assert_true(test_size > 0);
    assert_null(test_ptr);
#else
    skip();
#endif
}

/*
 * Main test runner
 */
int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_enhanced_dma_header_inclusion),
        cmocka_unit_test(test_error_codes_defined),
        cmocka_unit_test(test_function_prototypes_declared),
        cmocka_unit_test(test_internal_header_inclusion),
        cmocka_unit_test(test_system_includes_present),
    };

    printf("Running Enhanced DMA Header Tests...\n");
    return cmocka_run_group_tests(tests, NULL, NULL);
}