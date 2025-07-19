#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "include/uapi/hlthunk.h"
#include "include/uapi/hlthunk_tests.h"

int test_basic_connectivity() {
    printf("Testing basic HL-thunk connectivity...\n");
    
    // Try to get device count
    int device_count = hlthunk_get_device_count();
    printf("Device count: %d\n", device_count);
    
    if (device_count <= 0) {
        printf("No Habana devices found or driver not loaded\n");
        return -1;
    }
    
    return 0;
}

int test_dma_functionality() {
    struct hltests_state *tests_state;
    void *state;
    int rc;
    
    printf("Testing DMA functionality...\n");
    
    // Initialize the test framework
    rc = hltests_init();
    if (rc) {
        printf("Failed to initialize hlthunk tests library: %d\n", rc);
        return rc;
    }
    
    // Setup test state
    rc = hltests_setup(&state);
    if (rc) {
        printf("Failed to run setup phase of hlthunk tests: %d\n", rc);
        hltests_fini();
        return rc;
    }
    
    tests_state = (struct hltests_state *) state;
    
    printf("Test setup successful. Running DMA tests...\n");
    printf("Device ID: %d\n", tests_state->device_id);
    
    // Test H2D (Host to Device) - test_host_dram_perf
    printf("\nTesting Host-to-Device (H2D) DMA performance...\n");
    rc = test_host_dram_perf(&state);
    if (rc == 0) {
        printf("H2D test PASSED\n");
        printf("HOST->DRAM             %7.2lf GB/Sec\n",
            tests_state->perf_outcomes[RESULTS_DMA_PERF_HOST2DRAM]);
    } else {
        printf("H2D test FAILED with code: %d\n", rc);
    }
    
    // Test D2H (Device to Host) - test_dram_host_perf  
    printf("\nTesting Device-to-Host (D2H) DMA performance...\n");
    rc = test_dram_host_perf(&state);
    if (rc == 0) {
        printf("D2H test PASSED\n");
        printf("DRAM->HOST             %7.2lf GB/Sec\n",
            tests_state->perf_outcomes[RESULTS_DMA_PERF_DRAM2HOST]);
    } else {
        printf("D2H test FAILED with code: %d\n", rc);
    }
    
    // Test D2D (Device to Device) - test_dram_dram_single_ch_perf
    printf("\nTesting Device-to-Device (D2D) DMA performance...\n");
    rc = test_dram_dram_single_ch_perf(&state);
    if (rc == 0) {
        printf("D2D test PASSED\n");
        printf("DRAM->DRAM             %7.2lf GB/Sec\n",
            tests_state->perf_outcomes[RESULTS_DMA_PERF_DRAM2DRAM]);
    } else {
        printf("D2D test FAILED with code: %d\n", rc);
    }
    
    // Cleanup
    hltests_teardown(&state);
    hltests_fini();
    
    return 0;
}

int main(int argc, char *argv[]) {
    int rc;
    
    printf("HL-thunk DMA Direction Test\n");
    printf("===========================\n\n");
    
    // Test basic connectivity first
    rc = test_basic_connectivity();
    if (rc != 0) {
        printf("Basic connectivity test failed. Exiting.\n");
        return rc;
    }
    
    // Test DMA functionality
    rc = test_dma_functionality();
    if (rc != 0) {
        printf("DMA functionality test failed.\n");
        return rc;
    }
    
    printf("\nAll tests completed!\n");
    return 0;
}