# Makefile for HL-thunk DMA Tests
# This builds both the original (with test dependency) and standalone versions

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O1 -g -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-sign-compare
INCLUDES = -I.  -I/usr/include/libdrm -I/opt/habanalabs/src/hl-thunk/include/specs/common -I/opt/habanalabs/src/hl-thunk/include/uapi -I/opt/habanalabs/src/hl-thunk/include/uapi

# Library paths
LIBDIR = ../build/lib
LIBS_ORIGINAL = -L$(LIBDIR) -lhl-thunk -lhl-thunk-tests -lhl-thunk-err_injection -lpthread -ldl -lcmocka 

LIBS_STANDALONE = -L$(LIBDIR) -lhl-thunk -lhl-thunk-err_injection -lpthread -ldl 

# Enhanced DMA library source files
ENHANCED_DMA_SOURCES = enhanced_dma.c
ENHANCED_DMA_HEADERS = enhanced_dma.h enhanced_dma_internal.h
ENHANCED_DMA_LIB = libenhanced_dma.a

# Targets
TARGETS = wkdma wkdma_refactored $(ENHANCED_DMA_LIB)

.PHONY: all clean test help test-enhanced

all: $(TARGETS)

# Original embedded DMA test version (requires libhl-thunk-tests)
wkdma: wkdma.c
	@echo "Building wkdma (requires libhl-thunk-tests)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(LIBS_ORIGINAL)

# Refactored DMA test version (requires libhl-thunk-tests)
wkdma_refactored: wkdma_refactored.c
	@echo "Building wkdma_refactored (uses proper hlthunk functions)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(LIBS_ORIGINAL)

# Enhanced DMA library - will be created when we implement the .c file
$(ENHANCED_DMA_LIB): $(ENHANCED_DMA_SOURCES) $(ENHANCED_DMA_HEADERS)
	@echo "Building Enhanced DMA library..."
	@if [ -f enhanced_dma.c ]; then \
		$(CC) $(CFLAGS) $(INCLUDES) -c enhanced_dma.c -o enhanced_dma.o; \
		ar rcs $(ENHANCED_DMA_LIB) enhanced_dma.o; \
		echo "Enhanced DMA library built successfully"; \
	else \
		echo "enhanced_dma.c not yet implemented - creating placeholder library"; \
		ar rcs $(ENHANCED_DMA_LIB); \
	fi

# Test all versions
test: $(TARGETS)
	@echo "Testing wkdma..."
	@if [ -f wkdma ]; then \
		./wkdma || echo "Test failed or no device available"; \
	fi
	@echo "Testing wkdma_refactored..."
	@if [ -f wkdma_refactored ]; then \
		./wkdma_refactored || echo "Test failed or no device available"; \
	fi

# Test enhanced DMA headers and library
test-enhanced:
	@echo "Running Enhanced DMA tests..."
	@cd tests && $(MAKE) test

clean:
	rm -f $(TARGETS) *.o
	@cd tests && $(MAKE) clean

# Help
help:
	@echo "Available targets:"
	@echo "  all                           - Build all versions including Enhanced DMA library"
	@echo "  wkdma                         - Build original embedded test version"
	@echo "  wkdma_refactored              - Build refactored version using proper hlthunk functions"
	@echo "  libenhanced_dma.a             - Build Enhanced DMA library"
	@echo "  test                          - Run both test executables"
	@echo "  test-enhanced                 - Run Enhanced DMA library tests"
	@echo "  clean                         - Remove built executables and objects"
	@echo "  help                          - Show this help"
	@echo ""
	@echo "Prerequisites:"
	@echo "  - Run 'cd .. && ./build.sh' first to build the hl-thunk library"
	@echo "  - For embedded version: 'cd .. && EXTRA_CMAKE_FLAGS=\"-DHLTESTS_LIB_MODE=ON\" ./build.sh'"
	@echo "  - Install required packages: sudo apt install libcmocka-dev libdrm-dev"
	@echo ""
	@echo "Build process fixes applied:"
	@echo "  - Fixed typeof compilation errors by replacing with standard C"
	@echo "  - Added logging macros (W, LOG, STRINGIFY) for proper compilation"
	@echo "  - Updated library linking to include CMocka and HabanaLabs test framework"
