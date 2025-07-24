# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

This is the Enhanced DMA Functions library for HabanaLabs AI accelerators - a comprehensive C library providing synchronous and asynchronous DMA operations for h2d, d2h, and d2d transfers with multi-channel support and robust error handling.

## Agent OS Documentation

### Product Context
- **Mission & Vision:** @.agent-os/product/mission.md
- **Technical Architecture:** @.agent-os/product/tech-stack.md
- **Development Roadmap:** @.agent-os/product/roadmap.md
- **Decision History:** @.agent-os/product/decisions.md

### Development Standards
- **Code Style:** @~/.agent-os/standards/code-style.md
- **Best Practices:** @~/.agent-os/standards/best-practices.md

### Project Management
- **Active Specs:** @.agent-os/specs/
- **Spec Planning:** Use `@~/.agent-os/instructions/create-spec.md`
- **Tasks Execution:** Use `@~/.agent-os/instructions/execute-tasks.md`

## Workflow Instructions

When asked to work on this codebase:

1. **First**, check @.agent-os/product/roadmap.md for current priorities
2. **Then**, follow the appropriate instruction file:
   - For new features: @.agent-os/instructions/create-spec.md
   - For tasks execution: @.agent-os/instructions/execute-tasks.md
3. **Always**, adhere to the standards in the files listed above

## Important Notes

- Product-specific files in `.agent-os/product/` override any global standards
- User's specific instructions override (or amend) instructions found in `.agent-os/specs/...`
- Always adhere to established patterns, code style, and best practices documented above.

## Build System

This project uses a Makefile-based build system:

### Primary Build Commands
- `make` - Standard build (Debug mode by default)
- `make clean` - Clean build artifacts
- `make test` - Run test suite (when implemented)

### Build Configuration
- Build directory: Current directory with object files
- Compiler: GCC with C11 standard
- Dependencies: HabanaLabs thunk library headers

## DMA Operations

The library provides three main DMA types:

### H2D (Host-to-Device)
- Transfer data from host memory to device DRAM/SRAM
- Synchronous and asynchronous variants available
- Multi-channel support for improved performance

### D2H (Device-to-Host) 
- Transfer data from device DRAM/SRAM to host memory
- Non-blocking operations with callback support
- Automatic error handling and recovery

### D2D (Device-to-Device)
- Transfer data within device memory (DRAM↔DRAM, SRAM↔SRAM, DRAM↔SRAM)
- Direct device memory operations
- Optimized for ML workload patterns

## Development Guidelines

### Code Style
- Follow Linux kernel coding style with minor modifications
- Use C11 standard features
- Comprehensive error handling for all operations

### Memory Management
- Integration with existing HabanaLabs allocators
- Support for DMA-coherent memory allocation
- Automatic cleanup and resource management

### Testing
- Unit tests using CMocka framework
- Performance benchmarks for all transfer types
- Hardware-specific validation tests