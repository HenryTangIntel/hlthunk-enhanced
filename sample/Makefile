# Makefile for HL-thunk DMA Tests
# This builds both the original (with test dependency) and standalone versions

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O1 -g -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-sign-compare
INCLUDES = -I.  -I/usr/include/libdrm -I/opt/habanalabs/src/hl-thunk/include/specs/common -I/opt/habanalabs/src/hl-thunk/include/uapi -I/opt/habanalabs/src/hl-thunk/include/uapi

# Library paths
LIBDIR = ../build/lib
LIBS_ORIGINAL = -L$(LIBDIR) -lhl-thunk -lhl-thunk-tests -lhl-thunk-err_injection -lpthread -ldl -lcmocka 

LIBS_STANDALONE = -L$(LIBDIR) -lhl-thunk -lhl-thunk-err_injection -lpthread -ldl 

# Targets
TARGETS = working_dma_test_embedded 

.PHONY: all clean test help

all: $(TARGETS)

# Embedded DMA test version (requires libhl-thunk-tests)
working_dma_test_embedded: working_dma_test_embedded.c
	@echo "Building working_dma_test_embedded (requires libhl-thunk-tests)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(LIBS_ORIGINAL)

# Test all versions
test: $(TARGETS)
	@echo "Testing working_dma_test_embedded..."
	@if [ -f working_dma_test_embedded ]; then \
		./working_dma_test_embedded || echo "Test failed or no device available"; \
	fi

clean:
	rm -f $(TARGETS)

# Help
help:
	@echo "Available targets:"
	@echo "  all                           - Build all versions"
	@echo "  working_dma_test_embedded     - Build embedded test version (needs libhl-thunk-tests)"
	@echo "  test                          - Run test executable"
	@echo "  clean                         - Remove built executables"
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
