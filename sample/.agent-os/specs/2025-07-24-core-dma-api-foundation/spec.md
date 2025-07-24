# Spec Requirements Document

> Spec: Core DMA API Foundation
> Created: 2025-07-24
> Status: Planning

## Overview

Implement the foundational synchronous DMA transfer APIs for h2d, d2h, and d2d operations with comprehensive error handling and memory management integration. This phase establishes the core building blocks for all future asynchronous and multi-channel enhancements.

## User Stories

### Hardware Integration Engineer - Basic DMA Operations

As a HabanaLabs hardware integration engineer, I want to perform reliable synchronous DMA transfers between host and device memory, so that I can validate hardware functionality and debug data movement issues without complex async coordination.

**Workflow:** The engineer allocates host and device memory buffers, calls sync transfer functions with clear error reporting, and can immediately verify transfer completion and data integrity. All operations block until completion, providing predictable timing for hardware debugging scenarios.

### ML Platform Developer - Memory Management Integration

As an ML platform developer, I want DMA functions that integrate seamlessly with existing HabanaLabs memory allocators, so that I can build higher-level APIs without worrying about buffer compatibility or memory alignment issues.

**Workflow:** The developer uses existing hl-thunk memory allocation functions, passes the resulting pointers directly to DMA transfer functions, and receives detailed error information if incompatible memory types or alignment issues occur.

## Spec Scope

1. **Synchronous H2D Transfer API** - Function to transfer data from host memory to device DRAM/SRAM with blocking operation until completion
2. **Synchronous D2H Transfer API** - Function to transfer data from device DRAM/SRAM to host memory with blocking operation until completion  
3. **Synchronous D2D Transfer API** - Function to transfer data between device memory regions (DRAM↔DRAM, SRAM↔SRAM, DRAM↔SRAM) with blocking operation until completion
4. **Error Handling Framework** - Comprehensive error codes, detailed error messages, and recovery guidance for all failure modes
5. **Memory Management Integration** - Validation and integration with existing hl-thunk memory allocation and addressing functions

## Out of Scope

- Asynchronous operations or callback mechanisms
- Multi-channel DMA utilization  
- Performance optimization beyond basic functionality
- Transfer batching or queuing systems
- Advanced debugging or profiling features

## Expected Deliverable

1. **Functional Sync DMA APIs** - All three transfer types (h2d, d2h, d2d) work reliably with various buffer sizes and memory types
2. **Comprehensive Error Handling** - Clear error codes and messages for all failure scenarios including invalid parameters, hardware errors, and memory issues
3. **Integration Validation** - Seamless operation with existing hl-thunk memory allocation functions and device management APIs

## Spec Documentation

- **Tasks:** @.agent-os/specs/2025-07-24-core-dma-api-foundation/tasks.md
- **Technical Specification:** @.agent-os/specs/2025-07-24-core-dma-api-foundation/sub-specs/technical-spec.md
- **Tests Specification:** @.agent-os/specs/2025-07-24-core-dma-api-foundation/sub-specs/tests.md