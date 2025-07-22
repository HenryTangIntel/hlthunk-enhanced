# DMA Test Debugging Guide

## Overview
The enhanced DMA test program now includes comprehensive debugging capabilities to help diagnose issues with HabanaLabs devices and DMA operations.

## Quick Start Debugging

### 1. Enable Debug Logging
```bash
# Set debug level (1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=TRACE)
export HLTHUNK_DEBUG_LEVEL=4
./working_dma_test_embedded
```

### 2. Common Issues and Solutions

#### Issue: "Failed to open device"
```bash
# Check if device nodes exist
ls -la /dev/accel/accel*
ls -la /dev/hlX

# Check if driver is loaded
lsmod | grep habanalabs

# Check device permissions
sudo chmod 666 /dev/accel/accel0  # or appropriate device
```

#### Issue: "Device status check failed"
- Device may not be initialized
- Check dmesg for kernel driver errors
- Verify device is not in use by another process

#### Issue: "Failed to allocate memory"
- Check available system memory: `free -h`
- For huge pages: `cat /proc/meminfo | grep Huge`
- Try without huge pages (program will fallback automatically)

#### Issue: "DMA transfer failed"
- Check address alignment (should be 8-byte aligned)
- Verify source/destination addresses are valid
- Check if device memory ranges are correct

## Debug Output Interpretation

### Debug Levels
- **ERROR (1)**: Critical failures that prevent operation
- **WARN (2)**: Warnings about potentially problematic conditions
- **INFO (3)**: General information about test progress
- **DEBUG (4)**: Detailed function entry/exit and parameter info
- **TRACE (5)**: Very detailed execution traces

### Sample Debug Output
```
[INF] main:1154 Memory tracking system initialized successfully
[INF] debug_print_device_info:178 === DEVICE INFO ===
[INF] debug_print_device_info:179 Device ID: 0x1001
[INF] debug_print_device_info:180 Device type: Gaudi
[INF] debug_print_device_info:185 SRAM base: 0x10000000, size: 0x2000000
[INF] debug_print_device_info:186 DRAM base: 0x0, size: 0x100000000
[DBG] hltests_allocate_host_mem:380 Allocating host memory: size=0x10000, huge=0
[DBG] hltests_allocate_host_mem:398 Host memory allocated: ptr=0x7f8b4c000000, size=0x10000, huge=0
```

## Advanced Debugging Techniques

### 1. Memory Tracking
The program tracks all memory allocations. Enable DEBUG level to see:
- Memory allocation/deallocation
- Device virtual address mappings
- Memory leak detection at program exit

### 2. DMA Validation
All DMA transfers are validated before execution:
- Parameter validation (size, alignment)
- Address range validation against device memory maps
- Queue ID validation

### 3. Device Status Monitoring
```c
// Check device status
int debug_check_device_status(int fd);

// Print device information  
int debug_print_device_info(int fd);

// Print memory contents for debugging
void debug_print_memory_info(void *ptr, uint64_t size, const char *type);
```

## Environment Variables

### HLTHUNK_DEBUG_LEVEL
Controls debug output verbosity:
```bash
export HLTHUNK_DEBUG_LEVEL=5  # Maximum verbosity
export HLTHUNK_DEBUG_LEVEL=1  # Errors only
```

### HL_DEBUG (if supported by driver)
```bash
export HL_DEBUG=1  # Enable driver debug output
```

## Common Debugging Commands

### Check Device Status
```bash
# List devices
ls -la /dev/accel/ /dev/hl*

# Check driver status
lsmod | grep habanalabs
dmesg | grep habana | tail -20

# Check device info
sudo cat /sys/class/accel/accel0/device/vendor
sudo cat /sys/class/accel/accel0/device/device
```

### Monitor DMA Operations
```bash
# Enable maximum debug and trace DMA
export HLTHUNK_DEBUG_LEVEL=5
./working_dma_test_embedded 2>&1 | grep -E "(DMA|dma|transfer)"
```

### Memory Debugging
```bash
# Check for memory leaks
export HLTHUNK_DEBUG_LEVEL=4
./working_dma_test_embedded 2>&1 | grep -E "(allocat|free|leak)"
```

## Error Code Reference

### hlthunk Error Codes
- `-ENODEV` (-19): Device not found or not accessible
- `-EINVAL` (-22): Invalid parameter
- `-ENOMEM` (-12): Out of memory
- `-EBUSY` (-16): Device busy
- `-EFAULT` (-14): Bad address

### DMA Transfer Status Codes
- `HL_WAIT_CS_STATUS_COMPLETED` (0): Success
- `HL_WAIT_CS_STATUS_BUSY` (1): Still in progress
- `HL_WAIT_CS_STATUS_TIMEDOUT` (2): Timeout
- `HL_WAIT_CS_STATUS_ABORTED` (3): Aborted

## Building with Debug Symbols

```bash
# Build with debug symbols for GDB
gcc -g -O0 -DDEBUG working_dma_test_embedded.c -o working_dma_test_embedded -lhlthunk -lpthread

# Run with GDB
gdb ./working_dma_test_embedded
(gdb) set environment HLTHUNK_DEBUG_LEVEL=5
(gdb) run
```

## Troubleshooting Steps

1. **Basic connectivity**: Can you open the device?
2. **Device status**: Is the device properly initialized?
3. **Memory allocation**: Can you allocate host/device memory?
4. **Memory mapping**: Can host memory be mapped to device?
5. **DMA setup**: Are queue IDs and addresses correct?
6. **Command buffer**: Can you create and submit command buffers?
7. **DMA execution**: Does the DMA transfer complete successfully?

## Getting Help

If issues persist:
1. Capture full debug output: `HLTHUNK_DEBUG_LEVEL=5 ./working_dma_test_embedded > debug.log 2>&1`
2. Check kernel logs: `dmesg | grep habana > kernel.log`
3. Include device information and error messages in bug reports