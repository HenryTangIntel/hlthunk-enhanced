# Test Makefile for Enhanced DMA Library
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
LDFLAGS = -lcmocka

# Source directories
SRC_DIR = ..
TEST_DIR = .

# Test targets  
TEST_HEADERS = test_enhanced_dma_headers
TESTS = $(TEST_HEADERS)

# Check if headers exist and set compile flags accordingly
ENHANCED_DMA_H = $(SRC_DIR)/enhanced_dma.h
ENHANCED_DMA_INTERNAL_H = $(SRC_DIR)/enhanced_dma_internal.h

ifneq (,$(wildcard $(ENHANCED_DMA_H)))
    CFLAGS += -DHAVE_ENHANCED_DMA_H
endif

ifneq (,$(wildcard $(ENHANCED_DMA_INTERNAL_H)))
    CFLAGS += -DHAVE_ENHANCED_DMA_INTERNAL_H
endif

.PHONY: all clean test $(TESTS)

all: $(TESTS)

# Build individual tests
$(TEST_HEADERS): $(TEST_HEADERS).c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Run all tests
test: $(TESTS)
	@echo "Running all tests..."
	@for test in $(TESTS); do \
		echo "Running $$test..."; \
		./$$test || exit 1; \
	done
	@echo "All tests completed successfully!"

# Run individual test
test-headers: $(TEST_HEADERS)
	./$(TEST_HEADERS)

clean:
	rm -f $(TESTS)

# Check dependencies
check-deps:
	@echo "Checking for CMocka..."
	@pkg-config --exists cmocka || (echo "CMocka not found. Install with: apt-get install libcmocka-dev" && exit 1)
	@echo "CMocka found!"