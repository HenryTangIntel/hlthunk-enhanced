/**
 * @file enhanced_dma_internal.h
 * @brief Internal definitions and structures for Enhanced DMA Functions Library
 * 
 * This header contains internal data structures, helper functions, and 
 * implementation details that are not exposed in the public API.
 * 
 * @author Enhanced DMA Functions Library
 * @date 2025-07-24
 * @version 1.0.0
 */

#ifndef ENHANCED_DMA_INTERNAL_H
#define ENHANCED_DMA_INTERNAL_H

#include "enhanced_dma.h"
#include <stdbool.h>
#include <pthread.h>

/* HabanaLabs thunk library includes - these will be added when we integrate */
/* #include <hlthunk.h> */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup InternalConstants Internal Constants and Limits
 * @brief Constants used internally by the library
 * @{
 */

/** Maximum error message length */
#define EDMA_MAX_ERROR_MSG_LEN      (256)

/** Minimum transfer size in bytes */
#define EDMA_MIN_TRANSFER_SIZE      (1)

/** Maximum transfer size in bytes (16MB) */
#define EDMA_MAX_TRANSFER_SIZE      (16 * 1024 * 1024)

/** Required memory alignment in bytes */
#define EDMA_MEMORY_ALIGNMENT       (64)

/** Maximum number of supported devices */
#define EDMA_MAX_DEVICES            (8)

/** @} */

/**
 * @defgroup InternalStructures Internal Data Structures
 * @brief Private data structures used by the implementation
 * @{
 */

/**
 * @brief Internal device context structure
 * 
 * Contains device-specific information and state for DMA operations.
 * This structure is opaque to users and accessed via device handle.
 */
typedef struct {
    /** HabanaLabs device handle (will integrate with hl-thunk) */
    void *hl_device_handle;
    
    /** Device ID for identification */
    uint32_t device_id;
    
    /** Device type (Goya, Gaudi, etc.) */
    uint32_t device_type;
    
    /** Number of available DMA channels */
    uint32_t num_dma_channels;
    
    /** Device memory information */
    struct {
        uint64_t dram_base_addr;    /**< DRAM base address */
        uint64_t dram_size;         /**< DRAM size in bytes */
        uint64_t sram_base_addr;    /**< SRAM base address */
        uint64_t sram_size;         /**< SRAM size in bytes */
    } memory_info;
    
    /** Transfer statistics */
    struct {
        uint64_t total_transfers;   /**< Total number of transfers */
        uint64_t total_bytes;       /**< Total bytes transferred */
        uint64_t error_count;       /**< Number of errors */
    } stats;
    
    /** Thread safety mutex */
    pthread_mutex_t device_mutex;
    
    /** Device initialization state */
    bool is_initialized;
    
} enhanced_dma_device_context_t;

/**
 * @brief Transfer validation parameters
 * 
 * Structure used internally for parameter validation and transfer setup.
 */
typedef struct {
    enhanced_dma_device_handle_t device_handle;    /**< Device handle */
    void *src_ptr;                                 /**< Source pointer */
    void *dst_ptr;                                 /**< Destination pointer */
    enhanced_dma_size_t size;                      /**< Transfer size */
    enhanced_dma_flags_t flags;                    /**< Transfer flags */
    
    /** Validation results */
    bool src_is_host;                              /**< Source is host memory */
    bool dst_is_host;                              /**< Destination is host memory */
    bool src_is_device;                            /**< Source is device memory */
    bool dst_is_device;                            /**< Destination is device memory */
    bool src_aligned;                              /**< Source is properly aligned */
    bool dst_aligned;                              /**< Destination is properly aligned */
    
} enhanced_dma_transfer_params_t;

/**
 * @brief Error context for detailed error reporting
 * 
 * Thread-local structure to store detailed error information.
 */
typedef struct {
    char error_message[EDMA_MAX_ERROR_MSG_LEN];    /**< Detailed error message */
    int last_error_code;                           /**< Last error code */
    const char *function_name;                     /**< Function where error occurred */
    int line_number;                               /**< Line number where error occurred */
} enhanced_dma_error_context_t;

/** @} */

/**
 * @defgroup InternalAPIs Internal Helper APIs
 * @brief Private functions used by the implementation
 * @{
 */

/**
 * @brief Initialize internal library state
 * 
 * @return EDMA_SUCCESS on success, error code on failure
 */
int enhanced_dma_internal_init(void);

/**
 * @brief Cleanup internal library state
 */
void enhanced_dma_internal_cleanup(void);

/**
 * @brief Validate device handle
 * 
 * @param device_handle Device handle to validate
 * @return true if valid, false otherwise
 */
bool enhanced_dma_validate_device_handle(enhanced_dma_device_handle_t device_handle);

/**
 * @brief Validate transfer parameters
 * 
 * @param params Transfer parameters to validate
 * @return EDMA_SUCCESS if valid, error code otherwise
 */
int enhanced_dma_validate_transfer_params(enhanced_dma_transfer_params_t *params);

/**
 * @brief Check if pointer is host memory
 * 
 * @param ptr Pointer to check
 * @return true if host memory, false otherwise
 */
bool enhanced_dma_is_host_memory(const void *ptr);

/**
 * @brief Check if address is device memory
 * 
 * @param device_handle Device handle
 * @param device_addr Device address to check
 * @return true if valid device memory, false otherwise
 */
bool enhanced_dma_is_device_memory(enhanced_dma_device_handle_t device_handle,
                                   enhanced_dma_device_ptr_t device_addr);

/**
 * @brief Check memory alignment
 * 
 * @param ptr Pointer or address to check
 * @param alignment Required alignment in bytes
 * @return true if properly aligned, false otherwise
 */
bool enhanced_dma_check_alignment(uint64_t ptr, size_t alignment);

/**
 * @brief Set detailed error information
 * 
 * @param error_code Error code
 * @param function Function name where error occurred
 * @param line Line number where error occurred
 * @param format Printf-style format string
 * @param ... Format arguments
 */
void enhanced_dma_set_error(int error_code, const char *function, int line,
                           const char *format, ...);

/**
 * @brief Get thread-local error context
 * 
 * @return Pointer to error context for current thread
 */
enhanced_dma_error_context_t* enhanced_dma_get_error_context(void);

/**
 * @brief Convert device handle to internal context
 * 
 * @param device_handle Public device handle
 * @return Internal device context, or NULL if invalid
 */
enhanced_dma_device_context_t* enhanced_dma_get_device_context(
    enhanced_dma_device_handle_t device_handle);

/** @} */

/**
 * @defgroup InternalMacros Internal Helper Macros
 * @brief Utility macros for internal implementation
 * @{
 */

/** Set error with automatic function name and line number */
#define EDMA_SET_ERROR(code, fmt, ...) \
    enhanced_dma_set_error(code, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)

/** Return error code and set error message */
#define EDMA_RETURN_ERROR(code, fmt, ...) \
    do { \
        EDMA_SET_ERROR(code, fmt, ##__VA_ARGS__); \
        return code; \
    } while(0)

/** Validate parameter and return error if invalid */
#define EDMA_VALIDATE_PARAM(condition, fmt, ...) \
    do { \
        if (!(condition)) { \
            EDMA_RETURN_ERROR(EDMA_INVALID_PARAM, fmt, ##__VA_ARGS__); \
        } \
    } while(0)

/** Check for null pointer */
#define EDMA_CHECK_NULL(ptr, name) \
    EDMA_VALIDATE_PARAM((ptr) != NULL, "%s cannot be NULL", name)

/** Check transfer size bounds */
#define EDMA_CHECK_SIZE(size) \
    EDMA_VALIDATE_PARAM((size) >= EDMA_MIN_TRANSFER_SIZE && \
                        (size) <= EDMA_MAX_TRANSFER_SIZE, \
                        "Transfer size %zu is out of bounds [%d, %d]", \
                        (size_t)(size), EDMA_MIN_TRANSFER_SIZE, EDMA_MAX_TRANSFER_SIZE)

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ENHANCED_DMA_INTERNAL_H */