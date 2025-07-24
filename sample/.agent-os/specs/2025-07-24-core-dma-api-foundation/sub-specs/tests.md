# Tests Specification

This is the tests coverage details for the spec detailed in @.agent-os/specs/2025-07-24-core-dma-api-foundation/spec.md

> Created: 2025-07-24
> Version: 1.0.0

## Test Coverage

### Unit Tests

**Enhanced DMA H2D Functions**
- Test successful H2D transfer with valid parameters
- Test H2D transfer with null source pointer (should return EDMA_INVALID_PARAM)
- Test H2D transfer with null destination pointer (should return EDMA_INVALID_PARAM)
- Test H2D transfer with zero size (should return EDMA_INVALID_PARAM)
- Test H2D transfer with misaligned memory (should handle gracefully)
- Test H2D transfer with various buffer sizes (64 bytes to 16MB)

**Enhanced DMA D2H Functions**
- Test successful D2H transfer with valid parameters
- Test D2H transfer with null source pointer (should return EDMA_INVALID_PARAM)
- Test D2H transfer with null destination pointer (should return EDMA_INVALID_PARAM)
- Test D2H transfer with zero size (should return EDMA_INVALID_PARAM)
- Test D2H transfer with invalid device address (should return EDMA_MEMORY_ERROR)
- Test D2H transfer with various buffer sizes (64 bytes to 16MB)

**Enhanced DMA D2D Functions**
- Test successful D2D transfer between DRAM regions
- Test successful D2D transfer between SRAM regions  
- Test successful D2D transfer from DRAM to SRAM
- Test successful D2D transfer from SRAM to DRAM
- Test D2D transfer with overlapping memory regions (should return error)
- Test D2D transfer with invalid source address (should return EDMA_MEMORY_ERROR)
- Test D2D transfer with invalid destination address (should return EDMA_MEMORY_ERROR)

**Error Handling Framework**
- Test error code enumeration completeness
- Test error message generation for each error code
- Test error recovery guidance system
- Test parameter validation logic for all APIs

### Integration Tests

**Memory Management Integration**
- Test H2D transfer with hl-thunk allocated host memory
- Test D2H transfer with hl-thunk allocated device memory
- Test D2D transfer with mixed DRAM/SRAM allocations from hl-thunk
- Test transfer operations with memory allocated by different methods
- Test alignment compatibility with hl-thunk memory allocators

**Hardware Integration**
- Test transfers on real HabanaLabs hardware (if available)
- Test device handle validation with multiple devices
- Test queue selection and coordination with hl-thunk
- Test concurrent access to different devices
- Test proper cleanup after transfer completion

**Performance Baseline**
- Measure H2D transfer throughput for various sizes
- Measure D2H transfer throughput for various sizes  
- Measure D2D transfer throughput for various sizes
- Verify 80% of theoretical hardware bandwidth for large transfers
- Test transfer latency for small buffers (<1KB)

### Mocking Requirements

**Hardware Mocking**
- Mock hl-thunk device handles for unit tests without hardware
- Mock memory allocation functions for failure scenario testing
- Mock queue operations for testing queue selection logic
- Mock hardware errors for error handling validation

**Memory System Mocking**
- Mock memory alignment failures for validation testing
- Mock device memory allocation failures
- Mock host memory allocation failures
- Mock memory mapping operations for edge case testing

## Test Environment Setup

### Hardware Requirements
- HabanaLabs device (Goya, Gaudi, Gaudi2, or Gaudi3) for integration tests
- Sufficient host memory for large buffer tests (minimum 1GB)
- Multiple device setup for concurrent testing (optional)

### Software Requirements
- CMocka testing framework installed
- hl-thunk library with development headers
- GCC compiler with debugging symbols enabled
- Valgrind for memory leak detection
- Hardware drivers properly loaded (habanalabs kernel module)

### Test Data
- Various buffer sizes: 64B, 1KB, 64KB, 1MB, 16MB
- Test patterns: sequential data, random data, zero-filled buffers
- Alignment variations: aligned, misaligned by 1 byte, misaligned by various offsets
- Device memory types: DRAM only, SRAM only, mixed DRAM/SRAM

## Continuous Testing Strategy

### Automated Test Execution
- Run unit tests on every code change
- Run integration tests on hardware when available
- Performance regression tests weekly
- Memory leak detection tests with every build

### Test Coverage Goals
- 100% function coverage for all DMA APIs
- 90% branch coverage for error handling paths  
- 100% error code path coverage
- Performance benchmarks within 5% of baseline