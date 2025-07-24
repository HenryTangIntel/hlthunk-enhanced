/**
 * @file enhanced_dma.h
 * @brief Enhanced DMA Functions Library for HabanaLabs AI Accelerators
 * 
 * This library provides synchronous and asynchronous DMA operations for
 * HabanaLabs AI accelerators (Goya, Gaudi, Gaudi2, Gaudi3) with comprehensive
 * error handling and multi-channel support.
 * 
 * @author Enhanced DMA Functions Library
 * @date 2025-07-24
 * @version 1.0.0
 */

#ifndef ENHANCED_DMA_H
#define ENHANCED_DMA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ErrorCodes Enhanced DMA Error Codes
 * @brief Comprehensive error codes for all DMA operations
 * @{
 */

/** Success - operation completed successfully */
#define EDMA_SUCCESS            (0)

/** Invalid parameter provided to function */
#define EDMA_INVALID_PARAM      (-1)

/** Memory allocation, alignment, or access error */
#define EDMA_MEMORY_ERROR       (-2)

/** Hardware or driver error */
#define EDMA_HARDWARE_ERROR     (-3)

/** Transfer timeout (reserved for future use) */
#define EDMA_TIMEOUT            (-4)

/** @} */

/**
 * @defgroup DataTypes Enhanced DMA Data Types
 * @brief Core data types used throughout the library
 * @{
 */

/** Handle to HabanaLabs device - opaque pointer */
typedef void* enhanced_dma_device_handle_t;

/** Host memory pointer type */
typedef void* enhanced_dma_host_ptr_t;

/** Device memory pointer type (virtual address) */
typedef uint64_t enhanced_dma_device_ptr_t;

/** Transfer size type */
typedef size_t enhanced_dma_size_t;

/** Transfer flags for controlling operation behavior */
typedef uint32_t enhanced_dma_flags_t;

/** @} */

/**
 * @defgroup TransferFlags Enhanced DMA Transfer Flags
 * @brief Flags to control DMA transfer behavior
 * @{
 */

/** Default transfer behavior */
#define EDMA_FLAG_DEFAULT       (0x0)

/** Enable transfer validation (data integrity checking) */
#define EDMA_FLAG_VALIDATE      (0x1)

/** Enable performance logging for this transfer */
#define EDMA_FLAG_LOG_PERF      (0x2)

/** Skip parameter validation (use with caution) */
#define EDMA_FLAG_SKIP_VALIDATION   (0x4)

/** @} */

/**
 * @defgroup SyncTransferAPIs Synchronous Transfer APIs
 * @brief Blocking DMA transfer functions
 * @{
 */

/**
 * @brief Synchronous Host-to-Device (H2D) DMA transfer
 * 
 * Transfers data from host memory to device DRAM or SRAM. This function
 * blocks until the transfer is complete or an error occurs.
 * 
 * @param device_handle Handle to the target HabanaLabs device
 * @param host_src Source buffer in host memory
 * @param device_dst Destination address in device memory
 * @param size Number of bytes to transfer
 * @param flags Transfer control flags (see EDMA_FLAG_*)
 * 
 * @return EDMA_SUCCESS on success, negative error code on failure
 * 
 * @note Both source and destination should be properly aligned for optimal performance
 * @note The device handle must be valid and the device must be accessible
 */
int enhanced_dma_sync_h2d(enhanced_dma_device_handle_t device_handle,
                          enhanced_dma_host_ptr_t host_src,
                          enhanced_dma_device_ptr_t device_dst,
                          enhanced_dma_size_t size,
                          enhanced_dma_flags_t flags);

/**
 * @brief Synchronous Device-to-Host (D2H) DMA transfer
 * 
 * Transfers data from device DRAM or SRAM to host memory. This function
 * blocks until the transfer is complete or an error occurs.
 * 
 * @param device_handle Handle to the target HabanaLabs device
 * @param device_src Source address in device memory
 * @param host_dst Destination buffer in host memory
 * @param size Number of bytes to transfer
 * @param flags Transfer control flags (see EDMA_FLAG_*)
 * 
 * @return EDMA_SUCCESS on success, negative error code on failure
 * 
 * @note Both source and destination should be properly aligned for optimal performance
 * @note The device handle must be valid and the device must be accessible
 */
int enhanced_dma_sync_d2h(enhanced_dma_device_handle_t device_handle,
                          enhanced_dma_device_ptr_t device_src,
                          enhanced_dma_host_ptr_t host_dst,
                          enhanced_dma_size_t size,
                          enhanced_dma_flags_t flags);

/**
 * @brief Synchronous Device-to-Device (D2D) DMA transfer
 * 
 * Transfers data between device memory regions (DRAM to DRAM, SRAM to SRAM,
 * or DRAM to SRAM). This function blocks until the transfer is complete
 * or an error occurs.
 * 
 * @param device_handle Handle to the target HabanaLabs device
 * @param device_src Source address in device memory
 * @param device_dst Destination address in device memory
 * @param size Number of bytes to transfer
 * @param flags Transfer control flags (see EDMA_FLAG_*)
 * 
 * @return EDMA_SUCCESS on success, negative error code on failure
 * 
 * @note Source and destination regions must not overlap
 * @note Both addresses should be properly aligned for optimal performance
 * @note Cross-memory-type transfers (DRAM↔SRAM) are supported
 */
int enhanced_dma_sync_d2d(enhanced_dma_device_handle_t device_handle,
                          enhanced_dma_device_ptr_t device_src,
                          enhanced_dma_device_ptr_t device_dst,
                          enhanced_dma_size_t size,
                          enhanced_dma_flags_t flags);

/** @} */

/**
 * @defgroup UtilityAPIs Utility and Helper APIs
 * @brief Additional functions for error handling and diagnostics
 * @{
 */

/**
 * @brief Get human-readable error message for error code
 * 
 * @param error_code Error code returned by DMA functions
 * @return Static string describing the error, or "Unknown error" for invalid codes
 */
const char* enhanced_dma_get_error_string(int error_code);

/**
 * @brief Get last error message with additional context
 * 
 * @return Static string with detailed error information from last operation
 * 
 * @note This function is thread-local safe
 */
const char* enhanced_dma_get_last_error(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ENHANCED_DMA_H */