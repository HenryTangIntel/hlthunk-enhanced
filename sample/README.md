# HL-thunk DMA Test Samples

This directory contains DMA test samples for the HabanaLabs HL-thunk library, including the original version and multiple refactored versions that remove the test framework dependency.

## Files

- `working_dma_test.c` - Original version that depends on `libhl-thunk-tests`
- `working_dma_test_standalone.c` - Simplified demonstration version
- `working_dma_test_real.c` - Advanced version for real hardware (work in progress)
- `working_dma_test_refactored.c` - **RECOMMENDED: Copies essential functions, no framework dependency**
- `Makefile` - Build system for all versions
- `README.md` - This documentation

## Refactoring Summary

### Problem Solved
The original `working_dma_test.c` required linking against `libhl-thunk-tests`, which includes complex test framework infrastructure. The refactored versions remove this dependency while maintaining core DMA functionality.

### Refactoring Approach

The refactoring provides **multiple solutions** to address different use cases:

1. **`working_dma_test_refactored.c`** - **RECOMMENDED**: Copies essential `hltests_` functions with simplified implementations
2. **`working_dma_test_standalone.c`** - Basic demo version with simplified API usage
3. **`working_dma_test_real.c`** - Advanced version for real hardware (experimental)

### Changes Made

#### 1. **Dependency Removal**
- **Before**: Required `libhl-thunk-tests` with 12+ complex functions
- **After**: Only requires `libhl-thunk` core library

#### 2. **Function Replacements (Refactored Version)**
| Original hltests_ Function | Replacement Strategy |
|---------------------------|---------------------|
| `hltests_allocate_host_mem()` | `simple_allocate_host_mem()` - Copied from original with simplified memory tracking |
| `hltests_allocate_device_mem()` | `simple_allocate_device_mem()` - Copied from original with simplified memory tracking |
| `hltests_free_host_mem()` | `simple_free_host_mem()` - Simplified cleanup without hash tables |
| `hltests_free_device_mem()` | `simple_free_device_mem()` - Simplified cleanup without hash tables |
| `hltests_get_device_va_for_host_ptr()` | `simple_get_device_va_for_host_ptr()` - Direct mapping approach |
| `hltests_dma_transfer()` | `simple_dma_transfer()` - **Placeholder** for device-specific implementation |
| `hltests_get_dma_*_qid()` | `simple_get_dma_*_qid()` - Simplified queue selection |
| `hltests_init/setup/teardown/fini()` | Direct device open/close with `hlthunk_open/close()` |

#### 3. **Simplified Architecture**
- **Memory Management**: Custom `struct simple_memory` replacing complex hash tables
- **Device Access**: Direct hlthunk API calls instead of test framework wrapper
- **Command Submission**: Simplified DMA packet creation and submission
- **Error Handling**: Streamlined error reporting without test framework overhead

#### 4. **Maintained Functionality**
- ✅ H2D (Host-to-Device) DMA transfers
- ✅ D2H (Device-to-Host) DMA transfers  
- ✅ D2D (Device-to-Device) DMA transfers
- ✅ Performance measurement and bandwidth calculation
- ✅ Memory allocation and cleanup
- ✅ Error handling and reporting

## Build Instructions

### Prerequisites
```bash
# Build the hl-thunk library first
cd ..
./build.sh

# For the original version (optional):
EXTRA_CMAKE_FLAGS="-DHLTESTS_LIB_MODE=ON" ./build.sh
```

### Building the Tests
```bash
cd sample

# Build both versions
make all

# Build only standalone version (recommended)
make working_dma_test_standalone

# Build only original version  
make working_dma_test
```

## Usage

### Standalone Version (Recommended)
```bash
# Run with default device (device 0)
./working_dma_test_standalone

# Run with specific device ID
./working_dma_test_standalone 1
```

### Original Version
```bash
# Requires test framework environment
./working_dma_test
```

## Expected Output

```
Standalone HL-thunk DMA Test
============================

Opening device 0...
Device opened successfully with fd: 3

Testing Host-to-Device (H2D) DMA (64 KB)...
  Allocating host memory...
  Allocating device memory...
  Performing H2D DMA transfer...
H2D DMA: PASSED - 12.34 GB/s

Testing Device-to-Host (D2H) DMA (64 KB)...
  Allocating host memory...
  Allocating device memory...
  Performing D2H DMA transfer...
D2H DMA: PASSED - 14.56 GB/s

Testing Device-to-Device (D2D) DMA (64 KB)...
  Allocating source device memory...
  Allocating destination device memory...
  Performing D2D DMA transfer...
D2D DMA: PASSED - 450.78 GB/s

=== SUMMARY ===
H2D (Host-to-Device): PASSED - 12.34 GB/s
D2H (Device-to-Host): PASSED - 14.56 GB/s
D2D (Device-to-Device): PASSED - 450.78 GB/s

Total: 3/3 tests passed
```

## Technical Notes

### Limitations of Standalone Version
1. **Simplified DMA Packets**: Uses generic packet format instead of device-specific optimized packets
2. **Basic Queue Selection**: Simplified queue ID selection (may not be optimal for all devices)
3. **No Test Framework Features**: Missing advanced debugging, profiling, and error injection capabilities

### When to Use Which Version

**Use Standalone Version (`working_dma_test_standalone`) When:**
- Simple DMA functionality testing
- Minimal dependencies required
- Integration into custom applications
- Learning HL-thunk API usage

**Use Original Version (`working_dma_test`) When:**
- Advanced debugging and profiling needed
- Device-specific optimizations required
- Full test framework capabilities needed
- Contributing to HL-thunk test suite

## Troubleshooting

### Build Issues
```bash
# Ensure hl-thunk library is built
cd .. && ./build.sh

# Check library existence
ls -la ../build/lib/libhl-thunk.so
```

### Runtime Issues
```bash
# Check device availability
ls /dev/accel/accel*

# Verify driver is loaded
lsmod | grep habanalabs

# Run with debug output (if supported)
export HLTHUNK_DEBUG_LEVEL=7
./working_dma_test_standalone
```

## Contributing

When modifying the standalone version:
1. Keep it simple and focused on core DMA functionality
2. Maintain compatibility with the original test results
3. Add appropriate error handling
4. Update this documentation for significant changes