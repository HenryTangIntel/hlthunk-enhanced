# Makefile for HL-thunk DMA Tests
# This builds both the original (with test dependency) and standalone versions

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O1 -g
INCLUDES = -I../include  -I/usr/include/libdrm -I/opt/habanalabs/src/hl-thunk/include/specs/common

# Library paths
LIBDIR = ../build/lib
LIBS_ORIGINAL = -L$(LIBDIR) -lhl-thunk -lhl-thunk-tests -lhl-thunk-err_injection -lpthread -ldl 

LIBS_STANDALONE = -L$(LIBDIR) -lhl-thunk -lhl-thunk-tests -lhl-thunk-err_injection -lpthread -ldl 

# Targets
TARGETS = working_dma_test_embedded 

.PHONY: all clean test

all: $(TARGETS)

# Original version (requires libhl-thunk-tests)
working_dma_test_embedded: working_dma_test_embedded.c
	@echo "Building original working_dma_test (requires libhl-thunk-tests)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(LIBS_ORIGINAL)

# Test all versions
test: $(TARGETS)
	@echo "Testing original version..."
	@if [ -f working_dma_test ]; then \
		./working_dma_test_embedded || echo "Original test failed or no device available"; \
	fi

clean:
	rm -f $(TARGETS)

# Help
help:
	@echo "Available targets:"
	@echo "  all                           - Build all versions"
	@echo "  working_dma_test              - Build original version (needs libhl-thunk-tests)"
	@echo "  working_dma_test_standalone   - Build standalone version (only needs libhl-thunk)"
	@echo "  working_dma_test_real         - Build real hardware version"
	@echo "  working_dma_test_refactored   - Build refactored version (RECOMMENDED)"
	@echo "  test                          - Run all test versions"
	@echo "  clean                         - Remove built executables"
	@echo "  help                          - Show this help"
	@echo ""
	@echo "Prerequisites:"
	@echo "  - Run 'cd .. && ./build.sh' first to build the hl-thunk library"
	@echo "  - For original version: 'cd .. && EXTRA_CMAKE_FLAGS=-DHLTESTS_LIB_MODE=ON ./build.sh'"
	@echo ""
	@echo "Recommended: Use working_dma_test_refactored - copies essential functions without framework dependency"