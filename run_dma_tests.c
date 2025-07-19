#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "include/uapi/hlthunk.h"
#include "include/uapi/hlthunk_tests.h"

int main(int argc, char *argv[]) {
    struct hltests_state *tests_state;
    void *state;
    int rc;
    
    printf("HL-thunk DMA Performance Tests\n");
    printf("==============================\n\n");
    
    // Initialize the test framework
    printf("Initializing test framework...\n");
    rc = hltests_init();
    if (rc) {
        printf("Failed to initialize hlthunk tests library: %d\n", rc);
        return rc;
    }
    
    // Setup test state
    printf("Setting up test state...\n");
    rc = hltests_setup(&state);
    if (rc) {
        printf("Failed to run setup phase of hlthunk tests: %d\n", rc);
        hltests_fini();
        return rc;
    }
    
    tests_state = (struct hltests_state *) state;
    printf("Test setup successful. Device fd: %d\n\n", tests_state->fd);
    
    // Run H2D (Host-to-Device) DMA performance test
    printf("=== H2D (Host-to-Device) DMA Test ===\n");
    printf("Running host-to-DRAM performance test...\n");
    test_host_dram_perf(&state);
    if (tests_state->perf_outcomes[RESULTS_DMA_PERF_HOST2DRAM] > 0) {
        printf("✅ H2D Test PASSED: %.2f GB/s\n", 
               tests_state->perf_outcomes[RESULTS_DMA_PERF_HOST2DRAM]);
    } else {
        printf("❌ H2D Test FAILED\n");
    }
    printf("\n");
    
    // Run D2H (Device-to-Host) DMA performance test  
    printf("=== D2H (Device-to-Host) DMA Test ===\n");
    printf("Running DRAM-to-host performance test...\n");
    test_dram_host_perf(&state);
    if (tests_state->perf_outcomes[RESULTS_DMA_PERF_DRAM2HOST] > 0) {
        printf("✅ D2H Test PASSED: %.2f GB/s\n", 
               tests_state->perf_outcomes[RESULTS_DMA_PERF_DRAM2HOST]);
    } else {
        printf("❌ D2H Test FAILED\n");
    }
    printf("\n");
    
    // Run D2D (Device-to-Device) DMA performance test
    printf("=== D2D (Device-to-Device) DMA Test ===\n");
    printf("Running DRAM-to-DRAM single channel performance test...\n");
    test_dram_dram_single_ch_perf(&state);
    if (tests_state->perf_outcomes[RESULTS_DMA_PERF_DRAM2DRAM_SINGLE_CH] > 0) {
        printf("✅ D2D Test PASSED: %.2f GB/s\n", 
               tests_state->perf_outcomes[RESULTS_DMA_PERF_DRAM2DRAM_SINGLE_CH]);
    } else {
        printf("❌ D2D Test FAILED\n");
    }
    printf("\n");
    
    // Summary
    printf("=== TEST SUMMARY ===\n");
    double h2d_perf = tests_state->perf_outcomes[RESULTS_DMA_PERF_HOST2DRAM];
    double d2h_perf = tests_state->perf_outcomes[RESULTS_DMA_PERF_DRAM2HOST]; 
    double d2d_perf = tests_state->perf_outcomes[RESULTS_DMA_PERF_DRAM2DRAM_SINGLE_CH];
    
    printf("H2D (Host→Device): %s %.2f GB/s\n", 
           h2d_perf > 0 ? "✅ PASS" : "❌ FAIL", h2d_perf);
    printf("D2H (Device→Host): %s %.2f GB/s\n", 
           d2h_perf > 0 ? "✅ PASS" : "❌ FAIL", d2h_perf);
    printf("D2D (Device→Device): %s %.2f GB/s\n", 
           d2d_perf > 0 ? "✅ PASS" : "❌ FAIL", d2d_perf);
    
    int passed_tests = (h2d_perf > 0) + (d2h_perf > 0) + (d2d_perf > 0);
    printf("\nResult: %d/3 tests passed\n", passed_tests);
    
    // Cleanup
    hltests_teardown(&state);
    hltests_fini();
    
    return (passed_tests == 3) ? 0 : 1;
}