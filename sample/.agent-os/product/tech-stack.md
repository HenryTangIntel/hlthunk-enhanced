# Technical Stack

> Last Updated: 2025-01-24
> Version: 1.0.0

## Core Technologies

### Application Framework
- **Framework:** C11 Standard Library
- **Version:** C11 with GNU extensions
- **Language:** C11

### Database
- **Primary:** n/a
- **Version:** n/a
- **ORM:** n/a

## Development Stack

### Build System
- **Framework:** CMake
- **Version:** 3.0.1+
- **Build Tool:** GNU Make / Ninja

### Import Strategy
- **Strategy:** Static/Dynamic linking
- **Package Manager:** System package manager (apt/yum/dnf)
- **Dependency Management:** CMake find_package and pkg-config

### Testing Framework
- **Framework:** CMocka
- **Version:** Latest stable
- **Coverage:** gcov/lcov

### Documentation
- **Library:** Doxygen
- **Version:** Latest
- **Format:** API documentation with examples

## Hardware Integration

### Target Platforms
- **Primary:** Linux x86_64
- **Kernel:** Linux 5.15+
- **Driver Interface:** DRM/accel subsystem

### HabanaLabs Support
- **Devices:** Goya, Gaudi, Gaudi2, Gaudi3
- **Driver:** habanalabs kernel module
- **Interface:** /dev/accel/accel* device nodes

## Infrastructure

### Development Environment
- **Platform:** Linux development machines
- **Service:** Local development
- **Debugging:** GDB, Valgrind, AddressSanitizer

### Code Repository
- **Provider:** Git
- **Service:** Local repository
- **Access:** Direct filesystem access

### Build Pipeline
- **Platform:** Local builds
- **Trigger:** Manual make/cmake invocation
- **Tests:** Manual test execution

## Dependencies

### System Libraries
- **libc:** GNU C Library (glibc)
- **pthread:** POSIX threads library
- **libdrm:** Direct Rendering Manager library

### HabanaLabs Libraries
- **hl-thunk:** HabanaLabs userspace thunk library
- **habanalabs-driver:** Kernel driver interfaces

### Development Tools
- **Compiler:** GCC 7.0+ or Clang 10+
- **Debugger:** GDB with hardware debugging support
- **Memory Tools:** Valgrind, AddressSanitizer
- **Static Analysis:** cppcheck, clang-static-analyzer

## Performance Optimization

### Compiler Optimizations
- **Flags:** -O2 for development, -O3 for production
- **Features:** Auto-vectorization, loop unrolling
- **Profile-Guided:** Optional PGO support

### Memory Management
- **Alignment:** Hardware-specific alignment requirements
- **Allocation:** Custom allocators for DMA-coherent memory
- **Caching:** CPU cache optimization for data structures

### Hardware Features
- **DMA Channels:** Multi-channel parallel transfers
- **Memory Types:** Host RAM, Device DRAM, Device SRAM
- **Coherency:** Hardware cache coherency management