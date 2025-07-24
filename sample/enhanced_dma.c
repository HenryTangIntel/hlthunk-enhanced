/**
 * @file enhanced_dma.c
 * @brief Implementation of Enhanced DMA Functions Library
 * 
 * This file contains the implementation of synchronous and asynchronous DMA
 * operations for HabanaLabs AI accelerators with comprehensive error handling
 * and performance optimization.
 * 
 * @author Enhanced DMA Functions Library
 * @date 2025-07-24
 * @version 1.0.0
 */

#include "enhanced_dma.h"
#include "enhanced_dma_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <pthread.h>

/**
 * @defgroup InternalState Internal Library State
 * @brief Global state management for the enhanced DMA library
 * @{
 */

/** Thread-local storage for error context */
static __thread enhanced_dma_error_context_t g_error_context = {
    .error_message = {0},
    .last_error_code = EDMA_SUCCESS,
    .function_name = NULL,
    .line_number = 0
};

/** Library initialization flag */
static bool g_library_initialized = false;

/** Global library mutex for thread safety */
static pthread_mutex_t g_library_mutex = PTHREAD_MUTEX_INITIALIZER;

/** @} */

/**
 * @defgroup InternalImplementations Internal Helper Functions
 * @brief Private implementation functions
 * @{
 */

/**
 * @brief Initialize internal library state
 */
int enhanced_dma_internal_init(void) {
    pthread_mutex_lock(&g_library_mutex);
    
    if (g_library_initialized) {
        pthread_mutex_unlock(&g_library_mutex);
        return EDMA_SUCCESS;
    }
    
    /* Initialize error context */
    memset(&g_error_context, 0, sizeof(g_error_context));
    
    g_library_initialized = true;
    pthread_mutex_unlock(&g_library_mutex);
    
    return EDMA_SUCCESS;
}

/**
 * @brief Cleanup internal library state
 */
void enhanced_dma_internal_cleanup(void) {
    pthread_mutex_lock(&g_library_mutex);
    g_library_initialized = false;
    pthread_mutex_unlock(&g_library_mutex);
}

/**
 * @brief Set detailed error information with thread-local storage
 */
void enhanced_dma_set_error(int error_code, const char *function, int line,
                           const char *format, ...) {
    g_error_context.last_error_code = error_code;
    g_error_context.function_name = function;
    g_error_context.line_number = line;
    
    va_list args;
    va_start(args, format);
    vsnprintf(g_error_context.error_message, EDMA_MAX_ERROR_MSG_LEN, format, args);
    va_end(args);
}

/**
 * @brief Get thread-local error context
 */
enhanced_dma_error_context_t* enhanced_dma_get_error_context(void) {
    return &g_error_context;
}

/**
 * @brief Validate device handle (basic implementation)
 */
bool enhanced_dma_validate_device_handle(enhanced_dma_device_handle_t device_handle) {
    /* Basic validation - in real implementation this would check against
     * actual device registry and hl-thunk integration */
    return (device_handle != NULL);
}

/**
 * @brief Check if pointer is host memory (basic implementation)
 */
bool enhanced_dma_is_host_memory(const void *ptr) {
    /* Basic validation - in real implementation this would check memory mapping */
    return (ptr != NULL);
}

/**
 * @brief Check if address is device memory (basic implementation)
 */
bool enhanced_dma_is_device_memory(enhanced_dma_device_handle_t device_handle,
                                   enhanced_dma_device_ptr_t device_addr) {
    (void)device_handle; /* unused in basic implementation */
    
    /* Basic validation - device addresses should be non-zero and in valid ranges
     * In real implementation, this would check against actual device memory maps */
    return (device_addr != 0);
}

/**
 * @brief Check memory alignment
 */
bool enhanced_dma_check_alignment(uint64_t ptr, size_t alignment) {
    return ((ptr % alignment) == 0);
}

/**
 * @brief Validate transfer parameters for H2D operation
 */
int enhanced_dma_validate_h2d_params(enhanced_dma_device_handle_t device_handle,
                                      enhanced_dma_host_ptr_t host_src,
                                      enhanced_dma_device_ptr_t device_dst,
                                      enhanced_dma_size_t size,
                                      enhanced_dma_flags_t flags) {
    (void)flags; /* flags validation would be added here */
    
    /* Validate device handle */
    EDMA_CHECK_NULL(device_handle, "device_handle");
    
    if (!enhanced_dma_validate_device_handle(device_handle)) {
        EDMA_RETURN_ERROR(EDMA_INVALID_PARAM, "Invalid device handle");
    }
    
    /* Validate host source pointer */
    EDMA_CHECK_NULL(host_src, "host_src");
    
    if (!enhanced_dma_is_host_memory(host_src)) {
        EDMA_RETURN_ERROR(EDMA_MEMORY_ERROR, "Invalid host memory pointer");
    }
    
    /* Validate device destination address */
    if (!enhanced_dma_is_device_memory(device_handle, device_dst)) {
        EDMA_RETURN_ERROR(EDMA_MEMORY_ERROR, "Invalid device memory address: 0x%lx", device_dst);
    }
    
    /* Validate transfer size */
    EDMA_CHECK_SIZE(size);
    
    return EDMA_SUCCESS;
}

/**
 * @brief Perform the actual H2D DMA transfer (mock implementation)
 */
int enhanced_dma_perform_h2d_transfer(enhanced_dma_device_handle_t device_handle,
                                       enhanced_dma_host_ptr_t host_src,
                                       enhanced_dma_device_ptr_t device_dst,
                                       enhanced_dma_size_t size,
                                       enhanced_dma_flags_t flags) {
    (void)device_handle;
    (void)host_src;
    (void)device_dst;
    (void)size;
    (void)flags;
    
    /* This is a mock implementation. In the real implementation, this would:
     * 1. Get appropriate DMA queue from hl-thunk
     * 2. Set up DMA transfer using hl-thunk APIs
     * 3. Wait for completion
     * 4. Handle any hardware errors
     * 
     * For now, we simulate a successful transfer.
     */
    
    /* Simulate some basic validation */
    if (size == 0) {
        EDMA_RETURN_ERROR(EDMA_INVALID_PARAM, "Transfer size cannot be zero");
    }
    
    /* Simulate successful transfer */
    return EDMA_SUCCESS;
}

/** @} */

/**
 * @defgroup PublicAPIs Public API Implementations
 * @brief Implementation of public DMA APIs
 * @{
 */

/**
 * @brief Synchronous Host-to-Device (H2D) DMA transfer
 */
int enhanced_dma_sync_h2d(enhanced_dma_device_handle_t device_handle,
                          enhanced_dma_host_ptr_t host_src,
                          enhanced_dma_device_ptr_t device_dst,
                          enhanced_dma_size_t size,
                          enhanced_dma_flags_t flags) {
    
    /* Initialize library if needed */
    if (!g_library_initialized) {
        int init_result = enhanced_dma_internal_init();
        if (init_result != EDMA_SUCCESS) {
            return init_result;
        }
    }
    
    /* Clear any previous error state */
    memset(&g_error_context, 0, sizeof(g_error_context));
    
    /* Validate all parameters */
    int validation_result = enhanced_dma_validate_h2d_params(device_handle, host_src, 
                                                             device_dst, size, flags);
    if (validation_result != EDMA_SUCCESS) {
        return validation_result;
    }
    
    /* Perform the actual transfer */
    int transfer_result = enhanced_dma_perform_h2d_transfer(device_handle, host_src,
                                                           device_dst, size, flags);
    if (transfer_result != EDMA_SUCCESS) {
        return transfer_result;
    }
    
    return EDMA_SUCCESS;
}

/**
 * @brief Synchronous Device-to-Host (D2H) DMA transfer
 * @note Not yet implemented - will be added in Task 3
 */
int enhanced_dma_sync_d2h(enhanced_dma_device_handle_t device_handle,
                          enhanced_dma_device_ptr_t device_src,
                          enhanced_dma_host_ptr_t host_dst,
                          enhanced_dma_size_t size,
                          enhanced_dma_flags_t flags) {
    (void)device_handle;
    (void)device_src;
    (void)host_dst;
    (void)size;
    (void)flags;
    
    EDMA_RETURN_ERROR(EDMA_HARDWARE_ERROR, "D2H transfer not yet implemented");
}

/**
 * @brief Synchronous Device-to-Device (D2D) DMA transfer
 * @note Not yet implemented - will be added in Task 4
 */
int enhanced_dma_sync_d2d(enhanced_dma_device_handle_t device_handle,
                          enhanced_dma_device_ptr_t device_src,
                          enhanced_dma_device_ptr_t device_dst,
                          enhanced_dma_size_t size,
                          enhanced_dma_flags_t flags) {
    (void)device_handle;
    (void)device_src;
    (void)device_dst;
    (void)size;
    (void)flags;
    
    EDMA_RETURN_ERROR(EDMA_HARDWARE_ERROR, "D2D transfer not yet implemented");
}

/**
 * @brief Get human-readable error message for error code
 */
const char* enhanced_dma_get_error_string(int error_code) {
    switch (error_code) {
        case EDMA_SUCCESS:
            return "Success";
        case EDMA_INVALID_PARAM:
            return "Invalid parameter";
        case EDMA_MEMORY_ERROR:
            return "Memory error";
        case EDMA_HARDWARE_ERROR:
            return "Hardware error";
        case EDMA_TIMEOUT:
            return "Transfer timeout";
        default:
            return "Unknown error";
    }
}

/**
 * @brief Get last error message with additional context
 */
const char* enhanced_dma_get_last_error(void) {
    if (g_error_context.error_message[0] != '\0') {
        return g_error_context.error_message;
    }
    return enhanced_dma_get_error_string(g_error_context.last_error_code);
}

/** @} */