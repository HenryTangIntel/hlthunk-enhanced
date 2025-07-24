# Technical Specification

This is the technical specification for the spec detailed in @.agent-os/specs/2025-07-24-core-dma-api-foundation/spec.md

> Created: 2025-07-24
> Version: 1.0.0

## Technical Requirements

- **Function Signatures:** C11-compliant function definitions with clear parameter types and return values
- **Error Handling:** Integer return codes (0 = success, negative = error) with detailed error code enumeration
- **Memory Alignment:** Support for hardware-required memory alignment constraints (typically 64-byte aligned)
- **Buffer Validation:** Parameter validation for null pointers, invalid sizes, and memory region compatibility
- **Thread Safety:** Basic thread safety for concurrent access to different transfer operations
- **Hardware Integration:** Direct integration with existing hl-thunk device management and queue selection APIs
- **Performance Baseline:** Achieve at least 80% of raw hardware bandwidth for large transfers (>1MB)

## Approach Options

**Option A: Wrapper Around Existing hl-thunk APIs**
- Pros: Faster development, leverages proven code, maintains compatibility
- Cons: Limited error handling improvement, constrained by existing API limitations

**Option B: Direct Kernel Interface Implementation** 
- Pros: Maximum control, optimal performance, custom error handling
- Cons: Complex development, potential compatibility issues, higher maintenance burden

**Option C: Enhanced Wrapper with Validation Layer** (Selected)
- Pros: Improved error handling, parameter validation, maintains hl-thunk compatibility, extensible for future features
- Cons: Slight performance overhead from validation layer

**Rationale:** Option C provides the best balance of development speed, reliability, and future extensibility. The validation layer adds valuable error checking while maintaining compatibility with existing hl-thunk infrastructure. This approach allows for incremental enhancement while building on proven hardware interfaces.

## External Dependencies

- **hl-thunk library** - Core HabanaLabs userspace library for device communication
- **Justification:** Required for all device operations, memory management, and hardware queue access

- **CMocka** - Unit testing framework for comprehensive test coverage
- **Justification:** Needed for reliable testing of DMA operations and error conditions without hardware dependency

- **Standard C Libraries (libc, pthread)** - Basic system functionality and thread synchronization
- **Justification:** Required for standard C operations, error handling, and basic thread safety

## API Design

### Function Naming Convention
- `enhanced_dma_sync_h2d()` - Host to device synchronous transfer
- `enhanced_dma_sync_d2h()` - Device to host synchronous transfer  
- `enhanced_dma_sync_d2d()` - Device to device synchronous transfer

### Common Parameters
- `device_handle` - hl-thunk device handle for target device
- `src_ptr` - Source buffer pointer (host or device virtual address)
- `dst_ptr` - Destination buffer pointer (host or device virtual address)
- `size` - Transfer size in bytes
- `flags` - Optional transfer flags for validation control

### Error Code System
- `EDMA_SUCCESS (0)` - Transfer completed successfully
- `EDMA_INVALID_PARAM (-1)` - Invalid parameter provided
- `EDMA_MEMORY_ERROR (-2)` - Memory allocation or alignment error
- `EDMA_HARDWARE_ERROR (-3)` - Hardware or driver error
- `EDMA_TIMEOUT (-4)` - Transfer timeout (for future use)

## Implementation Strategy

### Phase 1.1: API Structure Setup
- Define header files with function prototypes and error codes
- Create basic validation framework for parameter checking
- Establish integration points with hl-thunk APIs

### Phase 1.2: H2D Implementation
- Implement synchronous host-to-device transfer
- Add comprehensive parameter validation
- Integrate with hl-thunk memory management

### Phase 1.3: D2H Implementation  
- Implement synchronous device-to-host transfer
- Reuse validation framework from H2D
- Add device memory address validation

### Phase 1.4: D2D Implementation
- Implement synchronous device-to-device transfer
- Handle DRAM/SRAM address space differences
- Validate memory region compatibility

### Phase 1.5: Error Handling Polish
- Comprehensive error message system
- Error recovery guidance
- Integration testing with various failure scenarios