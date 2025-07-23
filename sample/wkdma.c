#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>


// Logging macros
#define __STRINGIFY(x_) #x_
#define STRINGIFY(x_) __STRINGIFY(x_)
#define LOG(level_, format_, ...) \
	printf("[" level_ "] %s:" STRINGIFY(__LINE__) ": " format_ "\n", __func__, ##__VA_ARGS__)
#define W(format_, ...) LOG("WRN", format_, ##__VA_ARGS__)

struct config_iterator;

// Forward declarations  
uint64_t gaudi_get_device_va_for_host_ptr(int fd, void *vaddr);
void hltests_build_memalloc_page_size_array(int fd, uint64_t **page_arr, uint8_t *page_arr_size);
void hltests_destroy_memalloc_page_size_array(uint64_t *page_size_arr);

// ASIC-specific functions  
int gaudi_init(void);
void hltests_fini(void);

// System and module functions
int gaudi_free_host_mem(int fd, void *ptr);
int hltests_completion_db_init(int fd, void *completion_db);
void hltests_completion_db_teardown(int fd, void *completion_db);
int hltests_mme_dma_init(int fd);
int hltests_arc_init(int fd, void *arc_db);
void hltests_arc_fini(int fd, void *arc_db);
void hltests_teardown(void *state);

/**
 * struct config_limit -  config limits
 * @start: the first valid configuration value.
 * @end: the last valid configuration value.
 */
struct config_limit {
	size_t start;
	size_t end;
};

/**
 * config_iterator_init() - initializes the config iterator.
 * @limits: an array of limits, with the upper and lower limit of each config.
 * @limits_len: the length of the limits array.
 * Returns: a pointer to the config iterator.
 *
 * LIFETIME: `limits` is stored inside the iterator, so it must outlive the iterator.
 */
struct config_iterator *config_iterator_init(const struct config_limit *limits, size_t limits_len);

/**
 * config_iterator_destroy() - destroys the config iterator.
 * @config_iterator: the config iterator to destroy.
 */
void config_iterator_destroy(struct config_iterator *config_iterator);

/**
 * config_iterator_first() - Returns the current state of the config without advancing the state.
 * @config_iterator: the config iterator to get config from.
 * Returns: a pointer to the current configuration, NULL if reached the end of the iterator.
 */
const size_t *config_iterator_current(struct config_iterator *config_iterator);

/**
 * config_iterator_next() - Advances the state and returns the new state of the config.
 * @config_iterator: the config iterator advance.
 * Returns: a pointer to the current configuration, NULL if reached the end of the iterator.
 */
const size_t *config_iterator_next(struct config_iterator *config_iterator);


#include "../include/uapi/hlthunk.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <time.h>

/**
 * struct config_iterator -  config iterator data structure.
 * @limit: an array of limits of the config iterator.
 * @configs: current state of the configurations.
 * @configs_len: the lengths of the configurations array.
 * @is_exhausted: is the iterator exhausted.
 */
struct config_iterator {
	const struct config_limit *limits;
	size_t *configs;
	size_t configs_len;
	bool is_exhausted;
};

struct config_iterator *config_iterator_init(const struct config_limit *limits, size_t limits_len)
{
	/* Validate limits */
	for (size_t i = 0; i < limits_len; i++) {
		if (limits[i].end < limits[i].start) {
			W("Config at index %zu doesn't satisfy end >= start, end: %zu, start: %zu",
			  i, limits[i].end, limits[i].start);
			return NULL;
		}
	}

	struct config_iterator *iter = calloc(1, sizeof(struct config_iterator));

	if (!iter)
		return NULL;

	iter->configs = calloc(limits_len, sizeof(*iter->configs));
	if (!iter->configs)
		goto configs_alloc_failed;

	iter->limits = limits;
	iter->configs_len = limits_len;
	iter->is_exhausted = false;

	for (size_t i = 0; i < limits_len; i++)
		iter->configs[i] = iter->limits[i].start;

	return iter;

configs_alloc_failed:
	free(iter);
	return NULL;
}

void config_iterator_destroy(struct config_iterator *iter)
{
	free(iter->configs);
	free(iter);
}

const size_t *config_iterator_current(struct config_iterator *iter)
{
	if (iter->is_exhausted)
		return NULL;

	return iter->configs;
}

const size_t *config_iterator_next(struct config_iterator *iter)
{
	for (size_t i = 0; i < iter->configs_len; i++) {
		iter->configs[i]++;

		if (iter->configs[i] <= iter->limits[i].end)
			return iter->configs;

		iter->configs[i] = iter->limits[i].start;
	}

	iter->is_exhausted = true;

	return NULL;
}


/* SPDX-License-Identifier: MIT
 *
 * Copyright 2019-2022 HabanaLabs, Ltd.
 * All Rights Reserved.
 *
 */

#include "hlthunk.h"
#include "khash.h"
#include "pci_ids.h"

#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifndef HLTESTS_LIB_MODE
/* These includes are necessary for cmocka */
#include <setjmp.h>
#include <stdarg.h>

#include <cmocka.h>
#endif /* ndef HLTESTS_LIB_MODE */

#define __STRINGIFY(x_) #x_
#define STRINGIFY(x_) __STRINGIFY(x_)

#define LOG(level_, format_, ...) \
	printf("[" level_ "] %s:" STRINGIFY(__LINE__) ": " format_ "\n", __func__, ##__VA_ARGS__)

#define D(format_, ...)                                     \
	do {                                                \
		if (hltests_get_verbose_enabled())          \
			LOG("DBG", format_, ##__VA_ARGS__); \
	} while (0)
#define I(format_, ...) LOG("INF", format_, ##__VA_ARGS__)
#define W(format_, ...) LOG("WRN", format_, ##__VA_ARGS__)
#define E(format_, ...) LOG("ERR", format_ " (errno: %s)", ##__VA_ARGS__, strerror(errno))

/**
 * read_poll_timeout - Periodically poll an address until a condition is met or
 *                     a timeout occurs.
 * @op: Accessor function (takes @args as its arguments)
 * @val: Variable to read the value into
 * @cond: Break condition (usually involving @val)
 * @sleep_us: Maximum time to sleep between reads in us (0
 *            tight-loops).
 * @timeout_us: Timeout in us, 0 means never timeout
 * @sleep_before_read: if it is true, sleep @sleep_us before read.
 * @args: arguments for @op poll
 *
 * Returns 0 on success and -ETIMEDOUT upon a timeout. In either
 * case, the last read value at @args is stored in @val.
 */
#define read_poll_timeout(op, val, cond, sleep_us, timeout_us, sleep_before_read, args...) \
	({                                                                                 \
		uint64_t __timeout_us = (timeout_us);                                      \
		unsigned long __sleep_us = (sleep_us);                                     \
		struct timeval __end_time, __current_time,                                 \
			__timeout = { .tv_usec = __timeout_us };                           \
		gettimeofday(&__end_time, NULL);                                           \
		timeradd(&__end_time, &__timeout, &__end_time);                            \
		if (sleep_before_read && __sleep_us)                                       \
			usleep(__sleep_us);                                                \
		for (;;) {                                                                 \
			(val) = op(args);                                                  \
			if (cond)                                                          \
				break;                                                     \
			gettimeofday(&__current_time, NULL);                               \
			if (__timeout_us && timercmp(&__current_time, &__end_time, >=)) {  \
				(val) = op(args);                                          \
				break;                                                     \
			}                                                                  \
			if (__sleep_us)                                                    \
				usleep(__sleep_us);                                        \
		}                                                                          \
		(cond) ? 0 : -ETIMEDOUT;                                                   \
	})

#ifdef HLTESTS_LIB_MODE

#define VOID int
#define END_TEST return 0
#define END_TEST_FUNC(a) return (a)
#define EXIT_FROM_TEST return 0
#define CALL_HELPER_FUNC(func) \
		do { int _rc = (func); if (_rc) return _rc; } while (0)

#define fail() return -1
#define skip() return 0

#define fail_msg(fmt, ...) do { printf(fmt, ##__VA_ARGS__); return -1; } while (0)
#define print_message(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define print_error(fmt, ...) printf(fmt, ##__VA_ARGS__)

#define assert_null(p) if (p) return -1
#define assert_non_null(p) if (!(p)) return -1

#define assert_int_equal(a, b) if ((a) != (b)) return -1
#define assert_int_not_equal(a, b) if ((a) == (b)) return -1

#define assert_ptr_not_equal(p, v) if ((p) == (v)) return -1
#define assert_ptr_equal(p, v) if ((p) != (v)) return -1

#define assert_in_range(a, min, max) if (!((a) >= (min) && (a) <= (max))) return -1
#define assert_not_in_range(a, min, max) if ((a) >= (min) && (a) <= (max)) return -1

#define assert_true(a) if (!(a)) return -1
#define assert_false(a) if (a) return -1

#define fail_ret_ptr() return NULL

#define fail_msg_ret_ptr(fmt, ...) printf(fmt, ##__VA_ARGS__); return NULL

#define assert_null_ret_ptr(p) if (p) return NULL
#define assert_non_null_ret_ptr(p) if (!(p)) return NULL

#define assert_int_equal_ret_ptr(a, b) if ((a) != (b)) return NULL
#define assert_int_not_equal_ret_ptr(a, b) if ((a) == (b)) return NULL

#define assert_ptr_not_equal_ret_ptr(p, v) if ((p) == (v)) return NULL

#define assert_in_range_ret_ptr(a, min, max) \
		if (!((a) >= (min) && (a) <= (max))) \
			return NULL

#define assert_not_in_range_ret_ptr(a, min, max) \
		if ((a) >= (min) && (a) <= (max)) \
			return NULL

#define assert_true_ret_ptr(a) if (!(a)) return NULL
#define assert_false_ret_ptr(a) if (a) return NULL

#define assert_return_code(rc, error) fail_msg("%d < 0, errno(%d): %s", rc, error, strerror(error))

#define ALLOC_2D_ARR_RET_PTR(arr, r, c)                               \
	do {                                                          \
		int __i;                                              \
		(arr) = malloc(r * sizeof(*(arr)));                   \
		assert_non_null_ret_ptr((arr));                       \
		for (__i = 0; __i < (r); __i++) {                     \
			(arr)[__i] = malloc((c) * sizeof(**(arr)));   \
			assert_non_null_ret_ptr(arr[__i]);            \
			memset((arr)[__i], 0, (c) * sizeof(**(arr))); \
		}                                                     \
	} while (0)

#else /* HLTESTS_LIB_MODE */

#define VOID void
#define END_TEST
#define END_TEST_FUNC(a) (a)
#define EXIT_FROM_TEST return
#define CALL_HELPER_FUNC(func) (func)

#define fail_ret_ptr() fail()

#define fail_msg_ret_ptr(fmt, ...) fail_msg(fmt, ...)

#define assert_null_ret_ptr(p) assert_null((p))
#define assert_non_null_ret_ptr(p) assert_non_null((p))

#define assert_int_equal_ret_ptr(a, b) assert_int_equal((a), (b))
#define assert_int_not_equal_ret_ptr(a, b) assert_int_not_equal((a), (b))

#define assert_ptr_not_equal_ret_ptr(p, v) assert_ptr_not_equal((p), (v))

#define assert_in_range_ret_ptr(a, min, max) assert_in_range((a), (min), (max))

#define assert_not_in_range_ret_ptr(a, min, max) \
			assert_not_in_range((a), (min), (max))

#define assert_true_ret_ptr(a) assert_true((a))
#define assert_false_ret_ptr(a) assert_false((a))

#define ALLOC_2D_ARR_RET_PTR(arr, r, c) ALLOC_2D_ARR(arr, r, c)

#endif /* HLTESTS_LIB_MODE */

#define NUM_OF_SRAM_AFA_ADDRS		2
#define AFA_MAX_NUM_OF_CMPL_REGS	2

#ifdef HLTHUNK_TESTS_SANITIZER
#undef skip
#define skip() EXIT_FROM_TEST
#endif /* HLTHUNK_TESTS_SANITIZER */

#define MSEC_PER_SEC			1000L
#define USEC_PER_MSEC			1000L
#define NSEC_PER_USEC			1000L
#define NSEC_PER_MSEC			1000000L
#define USEC_PER_SEC			1000000L
#define NSEC_PER_SEC			1000000000L

#define ARRAY_SIZE(arr)			(sizeof(arr) / sizeof((arr)[0]))
#define BIT(nr)				((1UL) << (nr))
#define BIT_ULL(nr)			((1ULL) << (nr))
#define lower_16_bits(n)			((uint16_t) (n))
#define lower_32_bits(n)			((uint32_t) (n))
#define upper_32_bits(n)			((uint32_t) (((n) >> 16) >> 16))

#ifndef BITS_PER_BYTE
#define BITS_PER_BYTE	8
#endif /* BITS_PER_BYTE */

#ifndef BITS_PER_LONG
#define BITS_PER_LONG	(BITS_PER_BYTE * sizeof(unsigned long))
#endif /* BITS_PER_LONG */

#define CONCATENATE(a, b) __CONCAT(a, b)

#define GENMASK(h, l) \
	(((~0UL) - (1UL << (l)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

#define roundup(x, y) (((x) + (y) - 1) / (y) * (y))

#define rounddown(x, y) ((x) - ((x) % (y)))

#define __bf_shf(x) (__builtin_ffsll(x) - 1)

#define FIELD_PREP(_mask, _val) (((_val) << __bf_shf(_mask)) & (_mask))

#define print_and_flush(...)	\
	do { \
		printf(__VA_ARGS__); \
		fflush(stdout); \
	} while (0)

#define print_with_ts_and_flush(f_, ...)	\
do {	\
	time_t now = time(NULL);	\
	char *time = asctime(gmtime(&now));	\
	time[strlen(time)-1] = '\0'; /* remove \n */	\
	printf("%s ", time);	\
	printf((f_), ##__VA_ARGS__);	\
	fflush(stdout);	\
} while (0)

#define WAIT_FOR_CS_DEFAULT_TIMEOUT		5000000 /* 5 sec */
#define WAIT_FOR_CS_DEFAULT_TIMEOUT_NON_LEGACY	30000000 /* 30 sec */

#define RESET_WAIT_TIMEOUT_SEC			60 /* 60 sec */
#define PLDM_RESET_WAIT_TIMEOUT_SEC		600 /* 600 sec */

#define EVENTS_WAIT_TIMEOUT_SEC			5 /* 5 sec */
#define PLDM_EVENTS_WAIT_TIMEOUT_SEC		40 /* 40 sec */

#define HLTHUNK_MAX_DCORES			4

/* device bitmasks */
#define HLTEST_DEVICE_MASK_INVALID		0
#define HLTEST_DEVICE_MASK_GOYA			BIT(0)
#define HLTEST_DEVICE_MASK_GRECO		BIT(1)
#define HLTEST_DEVICE_MASK_GAUDI		BIT(2)
#define HLTEST_DEVICE_MASK_GAUDI_HL2000M	BIT(3)
#define HLTEST_DEVICE_MASK_GAUDI2		BIT(4)
#define HLTEST_DEVICE_MASK_GAUDI3		BIT(5)
#define HLTEST_DEVICE_MASK_GAUDI2B		BIT(6)
#define HLTEST_DEVICE_MASK_GAUDI2C		BIT(8)
#define HLTEST_DEVICE_MASK_GAUDI2D		BIT(9)
#define HLTEST_DEVICE_MASK_GAUDI3D		BIT(10)
#define HLTEST_DEVICE_MASK_DONT_CARE		GENMASK(10, 0)

#define HLTEST_DEVICE_MASK_GAUDI_ALL	\
		(HLTEST_DEVICE_MASK_GAUDI | HLTEST_DEVICE_MASK_GAUDI_HL2000M)

#define HLTEST_DEVICE_MASK_GAUDI2_ALL	\
		(HLTEST_DEVICE_MASK_GAUDI2 | HLTEST_DEVICE_MASK_GAUDI2B | \
		HLTEST_DEVICE_MASK_GAUDI2C | HLTEST_DEVICE_MASK_GAUDI2D)

#define HLTEST_DEVICE_MASK_GAUDI3_ALL	\
		(HLTEST_DEVICE_MASK_GAUDI3 | HLTEST_DEVICE_MASK_GAUDI3D)

#define HLTEST_DEVICE_MASK_GAUDI_FAMILY	\
		(HLTEST_DEVICE_MASK_GAUDI_ALL | HLTEST_DEVICE_MASK_GAUDI2_ALL | \
		HLTEST_DEVICE_MASK_GAUDI3)

#define SZ_128				0x00000080

#define SZ_1K				0x00000400
#define SZ_2K				0x00000800
#define SZ_4K				0x00001000
#define SZ_8K				0x00002000
#define SZ_16K				0x00004000
#define SZ_32K				0x00008000
#define SZ_64K				0x00010000
#define SZ_128K				0x00020000
#define SZ_256K				0x00040000
#define SZ_512K				0x00080000

#define SZ_1M				0x00100000
#define SZ_2M				0x00200000
#define SZ_4M				0x00400000
#define SZ_8M				0x00800000
#define SZ_16M				0x01000000
#define SZ_32M				0x02000000
#define SZ_64M				0x04000000
#define SZ_128M				0x08000000
#define SZ_256M				0x10000000
#define SZ_512M				0x20000000

#define SZ_1G				0x40000000
#define SZ_2G				0x80000000

#define SZ_4G				0x100000000ULL
#define SZ_8G				0x200000000ULL
#define SZ_16G				0x400000000ULL
#define SZ_32G				0x800000000ULL
#define SZ_128G				0x2000000000ULL

#define SHIFT_1MB			20
#define SHIFT_32MB			25
#define SHIFT_1GB			30

#define TIME_1_MIN_IN_USEC		60000000

#define ALLOC_2D_ARR(arr, r, c)                                       \
	do {                                                          \
		int __i;                                              \
		(arr) = malloc(r * sizeof(*(arr)));                   \
		assert_non_null((arr));                               \
		for (__i = 0; __i < (r); __i++) {                     \
			(arr)[__i] = malloc((c) * sizeof(**(arr)));   \
			assert_non_null(arr[__i]);                    \
			memset((arr)[__i], 0, (c) * sizeof(**(arr))); \
		}                                                     \
	} while (0)

#define FREE_2D_ARR(arr, r)                       \
	do {                                      \
		typeof(r) __i;                    \
		for (__i = 0; __i < (r); __i++) { \
			free((arr)[__i]);         \
		}                                 \
		free((arr));                      \
	} while (0)

#define PAGE_SHIFT_4KB			12
#define PAGE_SHIFT_2MB			21
#define PAGE_SHIFT_16MB			24

#define DIV_ROUND_DOWN_ULL(ll, d) ({ div((ll), (d)).quot; })

#define DIV_ROUND_UP_ULL(ll, d) \
	DIV_ROUND_DOWN_ULL((unsigned long long) (ll) + (d) - 1, (d))

#define MAX(x, y) ((x > y) ? (x) : (y))

#define __builtin_clzg(_x) __builtin_clzll((unsigned long long)(_x))

/* Last bit set */
#define LBS(_n)			(_n ? (sizeof(_n) * 8 - __builtin_clzg(_n)) : 0)
/* log of base 2 for unsigned 32bit or 64bit values */
#define HL_LOG2(_n)		(LBS(_n) - 1)

#define DMA_TEST_INC_SRAM(func_name, state, size) \
	void func_name(void **state) { hltests_dma_sram_test(state, size); }
#define DMA_TEST_INC_DRAM(func_name, state, size) \
	void func_name(void **state) { hltests_dma_dram_test(state, size); }
#define DMA_TEST_INC_DRAM_FRAG(func_name, state, size) \
	void func_name(void **state) \
	{ hltests_dma_dram_frag_mem_test(state, size); }
#define DMA_TEST_INC_DRAM_HIGH(func_name, state, size) \
	void func_name(void **state) \
	{ hltests_dma_dram_high_mem_test(state, size); }
#define READ32(full_address) \
		hltests_debugfs_read(tests_state->debugfs.addr_fd, \
			tests_state->debugfs.data32_fd, full_address)
#define WRITE32(full_address, val) \
		hltests_debugfs_write(tests_state->debugfs.addr_fd, \
			tests_state->debugfs.data32_fd, full_address, val)
#define READ64(full_address) \
		hltests_debugfs_read64(tests_state->debugfs.addr_fd, \
			tests_state->debugfs.data64_fd, full_address)
#define WRITE64(full_address, val) \
		hltests_debugfs_write64(tests_state->debugfs.addr_fd, \
			tests_state->debugfs.data64_fd, full_address, val)

#define PLDM_MAX_DMA_SIZE_FOR_TESTING SZ_1M

/* opcode 0 is undefined for all asics (including for pqm in gaudi3) */
#define PACKET_UNDEF_OPCODE 0x0

#define ALIGN_UP(addr, size)	(((addr) + ((size) - 1)) & ~((size) - 1))
#define ALIGN_DOWN(addr, size)  ((addr) & ~((size) - 1))
#define IS_8B_ALIGNED(addr)	(((addr) & 0x7) == 0)

#define IS_POWER_OF_TWO(x) ((x) > 0 && (((x) & ((x) - 1)) == 0))

#define ROUND_UP(x, y) (((x) + (y) - 1) / (y) * (y))							\

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define IS_ALIGNED(x, a)		(((x) & ((a) - 1)) == 0)

#define CQ_SIZE_LOG_2			3
#define CQ_SIZE				(1 << CQ_SIZE_LOG_2) /* 8 bytes, always */

#define DEFAULT_COLL_LAG_SIZE		3

#define CAP_ARC_FW_LOAD_NON_MASK		0x00000000
#define CAP_ARC_FW_LOAD_SCHED_MASK		BIT_ULL(1)
#define CAP_ARC_FW_LOAD_TPC_MASK		BIT_ULL(2)
#define CAP_ARC_FW_LOAD_MME_MASK		BIT_ULL(3)
#define CAP_ARC_FW_LOAD_EDMA_MASK		BIT_ULL(4)
#define CAP_ARC_FW_LOAD_PDMA_MASK		BIT_ULL(5)
#define CAP_ARC_FW_LOAD_ROT_MASK		BIT_ULL(6)
#define CAP_ARC_FW_LOAD_NIC_MASK		BIT_ULL(7)
#define CAP_MME_DMA_MASK			BIT_ULL(8)

#define CAP_ARC_FW_LOAD_PDMA_ONLY \
	(CAP_ARC_FW_LOAD_SCHED_MASK | CAP_ARC_FW_LOAD_PDMA_MASK)

#define ARC_BIN_PATH_MAX_LENGTH		256
#define ARC_HINT_48BIT_MASK		((1ull << 48ull) - 1ull)
#define ARC_ADDR_OFFSET_MASK		((1ull << 28ull) - 1ull)

KHASH_MAP_INIT_INT(ptr, void*)
KHASH_MAP_INIT_INT64(ptr64, void*)
KHASH_MAP_INIT_INT64(mapping, size_t)

#define PQM_PI_CI_IN_MEM_Q_SIZE		SZ_32M
#define PQM_TIMEOUT_USEC		10000
#define PLDM_PQM_TIMEOUT_USEC		3000000

#define ARC_BUILD_ADDR(region, addr)	\
	((uint32_t) (((region) << 28) | ((addr) & ARC_ADDR_OFFSET_MASK)))

/* This is used by gaudi3 only and it is (NUM_OF_PDMA_GRP * NUM_OF_PDMA_CH_PER_GRP) */
#define MAX_PDMA_CH_NUM		24

#define MAX_NUM_OF_MME	8

#define MAX_NUM_OF_BP_OFFSETS		16

#define TEST_TDR_DEADLOCK_TIMEOUT_SEC	5
#define MME_DMA_SIZE SZ_1M

/* There are no userspace equivalent for the SPEED and WIDTH kernel macros. Hence add them
 * explicitly till it is added to IB core userspace.
 */
#define HLTEST_IBV_SPEED_SDR	1
#define HLTEST_IBV_SPEED_DDR	2
#define HLTEST_IBV_SPEED_QDR	4
#define HLTEST_IBV_SPEED_FDR10	8
#define HLTEST_IBV_SPEED_FDR	16
#define HLTEST_IBV_SPEED_EDR	32
#define HLTEST_IBV_SPEED_HDR	64
#define HLTEST_IBV_SPEED_NDR	128

enum hltest_ib_port_width {
	HLTEST_IBV_WIDTH_1X	= 1,
	HLTEST_IBV_WIDTH_2X	= 16,
	HLTEST_IBV_WIDTH_4X	= 2,
	HLTEST_IBV_WIDTH_8X	= 4,
	HLTEST_IBV_WIDTH_12X	= 8
};

/* Must not be changed, otherwise the no mmu mode will fail to work on GOYA.
 * This structure is relevant only for Goya, Gaudi3 and above.
 * In other ASICs, we don't need the user to hint us about the direction.
 */
enum hltests_dma_direction {
	DMA_DIR_HOST_TO_DRAM,
	DMA_DIR_HOST_TO_SRAM,
	DMA_DIR_DRAM_TO_SRAM,
	DMA_DIR_SRAM_TO_DRAM,
	DMA_DIR_SRAM_TO_HOST,
	DMA_DIR_DRAM_TO_HOST,
	DMA_DIR_DRAM_TO_DRAM,
	DMA_DIR_SRAM_TO_SRAM,
	DMA_DIR_ENUM_MAX
};

enum hltests_endian_swap {
	ENDIAN_SWAP_NONE,
	ENDIAN_SWAP_16,
	ENDIAN_SWAP_32,
	ENDIAN_SWAP_64
};

/* Should be removed when a more appropriate enum is defined in habanalabs.h */
enum hltests_dcore_separation_mode {
	DCORE_MODE_FULL_CHIP,
	DCORE_MODE_HALF_CHIP,
	DCORE_MODE_ENUM_MAX
};

enum hltests_eb {
	EB_FALSE = 0,
	EB_TRUE
};

enum hltests_mb {
	MB_FALSE = 0,
	MB_TRUE
};

enum hltests_mon_mode {
	SOB_GREATER_OR_EQUAL = 0,
	SOB_EQUAL,
};

enum hl_tests_write_to_sob_mod {
	SOB_SET = 0,
	SOB_ADD,
	SOB_INC,
	SOB_DEC
};

enum sync_mng_base {
	SYNC_MNG_BASE_ES,
	SYNC_MNG_BASE_WS,
};

enum hl_tests_size_desc {
	ENTRY_SIZE_16B = 0,
	ENTRY_SIZE_32B
};

enum hl_tests_load_dst {
	DST_PREDICATES = 0,
	DST_SCALARS
};

enum hl_tests_predicates_map {
	PMAP_NON_CONSECUTIVE = 0,
	PMAP_CONSECUTIVE
};

enum hl_tests_exe_type {
	ETYPE_ALL_OR_LOWER_RF = 0,
	ETYPE_UPPER_RF
};

enum hl_tests_mon_wr_num {
	WR_NUM_1_WRITE = 0,
	WR_NUM_2_WRITES,
	WR_NUM_3_WRITES,
	WR_NUM_4_WRITES,
};

enum hltests_stream_id {
	STREAM0 = 0,
	STREAM1,
	STREAM2,
	STREAM3,
	NUM_OF_STREAMS
};

enum hltests_cb_type {
	CB_TYPE_USER = 0,
	CB_TYPE_KERNEL,
	CB_TYPE_KERNEL_MAPPED
};

#define INTERNAL	CB_TYPE_USER
#define EXTERNAL	CB_TYPE_KERNEL

enum hltests_destroy_cb {
	DESTROY_CB_FALSE = 0,
	DESTROY_CB_TRUE
};

enum hltests_huge {
	NOT_HUGE_MAP = 0,
	HUGE_MAP
};

enum hltests_contiguous {
	NOT_CONTIGUOUS = 0,
	CONTIGUOUS
};

// Forward declaration after enum hltests_contiguous definition
void *hltests_allocate_host_mem_aligned(int fd, uint64_t size, enum hltests_huge is_huge, uint64_t alignment);
void *gaudi_allocate_host_mem_aligned_flags(int fd, uint64_t size, enum hltests_huge huge, uint64_t align, uint32_t flags);

enum hltests_test_results {
	RESULTS_DMA_PERF_HOST2DRAM,
	RESULTS_DMA_PERF_HOST2SRAM,
	RESULTS_DMA_PERF_DRAM2SRAM_SINGLE_CH,
	RESULTS_DMA_PERF_SRAM2DRAM_SINGLE_CH,
	RESULTS_DMA_PERF_DRAM2DRAM_SINGLE_CH,
	RESULTS_DMA_PERF_DRAM2SRAM_MULTI_CH,
	RESULTS_DMA_PERF_SRAM2DRAM_MULTI_CH,
	RESULTS_DMA_PERF_DRAM2DRAM_MULTI_CH,
	RESULTS_DMA_PERF_SRAM_DRAM_BIDIR_FULL_CH,
	RESULTS_DMA_PERF_DRAM2SRAM_5_CH,
	RESULTS_DMA_PERF_SRAM2HOST,
	RESULTS_DMA_PERF_DRAM2HOST,
	RESULTS_DMA_PERF_HOST_SRAM_BIDIR,
	RESULTS_DMA_PERF_HOST_DRAM_BIDIR,
	RESULTS_DMA_PERF_ALL2ALL_SUPER_STRESS,
	RESULTS_NIC_E2E,
	RESULTS_DMA_PERF_DRAM2DRAM_SINGLE_CH_MME_DMA,
	RESULTS_DMA_PERF_DRAM2DRAM_MULTI_CH_MME_DMA,
	RESULTS_DMA_PERF_HOST2DRAM_MULTI_CH,
	RESULTS_DMA_PERF_DRAM2HOST_MULTI_CH,
	RESULTS_MAX
};

enum hltests_random {
	NOT_RANDOM = 0,
	RANDOM
};

enum range_type {
	HOST_ADDR,
	DRAM_ADDR
};

enum dup_group_id {
	ENG_GROUP_MME_COMPUTE = 0,
	ENG_GROUP_TPC_COMPUTE = 1,
	ENG_GROUP_EDMA_COMPUTE = 2,
	ENG_GROUP_EDMA_NETWORK_REDUCTION = 3,
	ENG_GROUP_EDMA_NETWORK_LOOPBACK = 4,
	ENG_GROUP_EDMA_NETWORK_MEMSET = 5,
	ENG_GROUP_TPC_MEDIA = 6,
	ENG_GROUP_RTR_MEDIA = 7,
	ENG_GROUP_PDMA_TX_CMD = 8,
	ENG_GROUP_PDMA_TX_DATA = 9,
	ENG_GROUP_PDMA_RX = 10,
	ENG_GROUP_NIC_RECEIVE_SCALE_UP = 11,
	ENG_GROUP_NIC_RECEIVE_SCALE_OUT = 12,
	ENG_GROUP_NIC_SEND_SCALE_UP = 13,
	ENG_GROUP_NIC_SEND_SCALE_OUT = 14,
	ENG_GROUP_NUMBER_OF_GROUPS,
	ENG_GROUP_INVALID
};

enum arc_type {
	ARC_TYPE_SCHED,
	ARC_TYPE_ENGINE,
	ARC_TYPE_NUM,
};

enum arc_type_mask {
	ARC_TYPE_SCHED_MASK = BIT(ARC_TYPE_SCHED),
	ARC_TYPE_ENGINE_MASK = BIT(ARC_TYPE_ENGINE),
	ARC_TYPE_ALL_MASK = ARC_TYPE_SCHED_MASK | ARC_TYPE_ENGINE_MASK,
};

enum err_trigger {
	TRIG_QM_ERR,
	TRIG_DMA_ERR,
	TRIG_SM_ERR,
	TRIG_MME_DMA_ERR,
	RAZWI_TYPE_LBW_RR,
	RAZWI_TYPE_HBW_RR,
	RAZWI_TYPE_ADDR_DEC,
	RAZWI_TYPE_NUM,
	TRIG_NUM
};

struct hltests_debugfs {
	int addr_fd;
	int data32_fd;
	int data64_fd;
	int clk_gate_fd;
	char clk_gate_val[32];
};

struct hltests_module_params_info {
	uint64_t tpc_mask;
	uint64_t nic_ports_mask;
	uint32_t gaudi_huge_page_optimization;
	uint32_t timeout_locked;
	uint32_t reset_on_lockup;
	uint32_t pldm;
	uint32_t mmu_enable;
	uint32_t clock_gating;
	uint32_t mme_enable;
	uint32_t dram_enable;
	uint32_t cpu_enable;
	uint32_t reset_pcilink;
	uint32_t config_pll;
	uint32_t cpu_queues_enable;
	uint32_t fw_loading;
	uint32_t heartbeat;
	uint32_t axi_drain;
	uint32_t security_enable;
	uint32_t sram_scrambler_enable;
	uint32_t dram_scrambler_enable;
	uint32_t cache_enabled;
	uint32_t hbm_ecc_enable;
	uint32_t compatibility_mode;
	uint32_t hard_reset_on_fw_events;
	uint32_t decoder_mask;
	uint32_t rotator_mask;
	uint32_t dram_page_scrub;
	uint32_t clock_gating_ext;
	uint32_t fw_loading_ext;
	uint32_t nic_lanes_per_port;
};

struct hltests_cb {
	void *ptr;
	uint64_t cb_handle;
	uint32_t cb_size;
	enum hltests_cb_type cb_type;
};

struct hltests_cq_queue {
	struct hltests_cb cq_cb;
	uint64_t cq_device_va;
	uint32_t *data;
	uint32_t length;
	uint32_t pi;
	uint32_t ci;
	uint16_t log2_size;
	uint16_t cqid;
	uint16_t intid;
};

struct hltests_completion_db {
	struct hltests_cq_queue cqq;
	struct hltests_cb cq_cb;
	pthread_mutex_t db_lock;
	pthread_mutex_t wait_lock;
	uint64_t next_seq;
	uint64_t data_device_va;
	uint64_t submission_cnt;
	uint64_t curr_cnt;
	uint32_t max_num_of_cs;
	uint16_t cqid;
	uint16_t intid;
	uint16_t sync_sob;
	uint16_t sync_sob_val;
	uint16_t pre_cq_ready_active_sob;
	bool cq_ready;
	size_t idx;

	struct hltests_completion_db_e {
		bool job_completed;
		uint64_t seq;
		uint32_t target_value;
		uint16_t counter_mon;
		uint16_t queue_mon;
		uint16_t sob[2];
		uint16_t payload;
		uint16_t expected_sync_sob_val;
		uint16_t expected_active_sob_val;
		uint8_t  active_sob;
	} *cqs;
};

struct hltests_mmapped_buff {
	void *ptr;
	uint64_t lbw_addr;
	size_t size;
};

struct hltests_mmapped_reg {
	uint32_t *ptr;
	uint32_t lbw_addr;
};

struct hltests_mmapped_resource {
	struct hltests_mmapped_buff buff;
	bool is_available;
};

struct arc_fw_info {
	pthread_mutex_t submission_lock;
	uint32_t arc_id;
	uint8_t *dccm_host_addr;
	uint8_t *acp_host_addr;
	uint8_t *cq_ptr;
	uint32_t hbm_offset;
	uint32_t dccm_size;
	uint32_t acp_size;
	uint32_t cq_size;
	uint32_t ccb_size;
	bool enabled;
};

struct sm_global_counters {
	uint32_t reserved_sobs;
	uint32_t reserved_mons;
	uint32_t reserved_cqs;
	uint32_t reserved_interrupts;
};

struct arc_log_buf {
	char *ptr;
	uint32_t ci;
};

struct hltests_arc_db {
	struct arc_fw_info *fw_info;
	struct arc_log_buf *arc_log_bufs;
	void *host_mem;
	void *cq_host_mem;
	void *dram_on_host_mem;
	uint64_t device_mem_handle;
	uint64_t device_mem_size;
	uint64_t host_device_va;
	uint64_t dram_device_va;
	uint64_t cq_device_va;
	uint32_t host_mem_size;
	uint32_t dram_mem_size;
	uint32_t host_mem_reserved_size;
	uint32_t device_mem_reserved_size;
	uint32_t arc_count;
	pthread_t logs_thread;
};

struct arc_asic_fw_load_params {
	uint64_t sched_arc_image_size;
	uint64_t eng_arc_image_size;
	uint32_t sched_arc_first_idx;
	uint32_t sched_arc_last_idx;
	uint32_t eng_arc_first_idx;
	uint32_t eng_arc_last_idx;
	uint32_t arc_image_hbm_size;
};

struct arc_load_img_data {
	void *img_host_ptr;
	uint64_t img_host_va;
};

struct arc_load_data {
	struct arc_load_img_data sched_img_data;
	struct arc_load_img_data eng_img_data;
	int fd;
};

struct iterate_arcs_ctx {
	void (*fn)(struct iterate_arcs_ctx *ctx, uint32_t cpu_id);
	struct arc_asic_fw_load_params *load_param;
	struct hltests_arc_db *arc_db;
	void *data;
	int rc;
};

struct pdma_ch_info {
	pthread_mutex_t pi_ci_lock;
	uint64_t submission_q_handle;
	uint32_t submission_q_size;
	uint8_t *submission_q;
	uint32_t ch_block_size;
	uint8_t *ch_host_addr;
	uint32_t pi_shadow;
	uint32_t ci_shadow;
	bool is_down;
	bool enabled;
	uint8_t prio;
};

struct hltests_pdma_db {
	struct pdma_ch_info *ch_info;
	uint64_t user_en_ch_mask;
	uint16_t pdma_grp_ch_max;
	uint16_t pdma_grp_max;
	uint16_t pdma_ch_max;
	uint16_t ch_qid_lut[MAX_PDMA_CH_NUM];
};

struct hltests_mme_dma_info {
	uint16_t sob_id[MAX_NUM_OF_MME];
	uint16_t mon_id[MAX_NUM_OF_MME];
	bool enable;
};

/* enum hltests_recovery_status - recovery status information.
 * @RECOVERY_STATUS_NOT_REQUIRED: no recovery is required.
 * @RECOVERY_STATUS_REQUIRED: recovery is required due to event with a device reset indication.
 * @RECOVERY_STATUS_FAILED: recovery has failed.
 */
enum hltests_recovery_status {
	RECOVERY_STATUS_NOT_REQUIRED,
	RECOVERY_STATUS_REQUIRED,
	RECOVERY_STATUS_FAILED
};

struct hltests_listener_thread_params {
	pthread_t thread_id;
	pthread_barrier_t barrier;
	pthread_cond_t cond;
	pthread_mutex_t lock;
	uint64_t notifier_events;
	enum hltests_recovery_status recovery_status;
	int handle;
	pid_t listener_tid; /* SW-159137 TODO: field can be removed when issue is sovled */
};

struct hltests_device {
	const struct hltests_asic_funcs *asic_funcs;

	khash_t(ptr64) * mem_table_host;

	pthread_mutex_t mem_table_host_lock;

	khash_t(ptr64) * mem_table_device;

	pthread_mutex_t mem_table_device_lock;

	khash_t(ptr64) * cb_table;
	struct ibv_device *ibdev;
	uint32_t *hl_to_ib_port_map;

	pthread_mutex_t cb_table_lock;

	khash_t(mapping) * mmap_table;
	pthread_mutex_t mmap_table_lock;

	void *priv;
	int fd;
	int refcnt;
	enum hl_pci_ids device_id;
	bool sim_dram_on_host;
	uint32_t vm;
	int32_t vm_fd;
	struct hltests_module_params_info module_params;
	struct hltests_arc_db arc_db;
	struct hltests_pdma_db pdma_db;
	struct sm_global_counters counters;
	struct sm_global_counters cq_db_counters;
	struct hltests_mme_dma_info mme_dma_info;
	struct hltests_completion_db completion_db;

	void *nic_test_ctx;
};

// ASIC function declarations after struct hltests_device definition
void goya_tests_set_asic_funcs(struct hltests_device *hdev);
void gaudi_tests_set_asic_funcs(struct hltests_device *hdev);
void gaudi2_tests_set_asic_funcs(struct hltests_device *hdev);
void gaudi3_tests_set_asic_funcs(struct hltests_device *hdev);

struct hltests_state {
	struct hlthunk_mac_addr mac_addrs[HL_INFO_MAC_ADDR_MAX_NUM];
	double perf_outcomes[RESULTS_MAX];
	struct hltests_debugfs debugfs;
	struct ibv_device **dev_list;
	uint64_t nic_ports_mask;
	int fd;
	int imp_fd;
	bool mme;
	bool lkd_security;
	enum hlthunk_device_name asic_type;
	struct hltests_listener_thread_params listener_thread_params;
	struct hlthunk_hw_ip_info hw_ip;
	void *priv;
};

// Function declarations after struct hltests_state definition
int hltests_teardown_user_engines(struct hltests_state *tests_state);

struct hltests_pkt_info {
	uint32_t qid; /* currently, reserved only for Gaudi3's PDMA/ARC */
	enum hltests_eb eb;
	enum hltests_mb mb;
	uint8_t pred;
	union {
		struct {
			uint32_t value;
			uint16_t reg_addr;
		} wreg32;
		struct {
			uint8_t priority;
			bool release;
		} arb_point;
		struct {
			uint64_t address;
			uint32_t value;
		} msg_long;
		struct {
			uint8_t base;
			uint16_t address;
			uint32_t value;
		} msg_short;
		struct {
			uint16_t address;
			enum hltests_mon_mode mon_mode;
			uint16_t sob_val;
			uint16_t sob_id;
			uint8_t base;
		} arm_monitor;
		struct {
			uint16_t address;
			uint8_t wr_num;
			uint8_t msb_sob_id;
			uint8_t sm_data_config;
			bool long_mode;
			bool cq_enable;
			bool lbw_enable;
			bool long_high_group;
			bool auto_zero;
			uint8_t base;
		} config_monitor;
		struct {
			uint8_t long_mode;
			uint8_t zero_sob_counter;
			uint16_t sob_id;
			uint64_t value;
			enum hl_tests_write_to_sob_mod mode;
			enum sync_mng_base base;
		} write_to_sob;
		struct {
			uint8_t dec_val;
			uint8_t gate_val;
			uint8_t fence_id;
		} fence;
		struct {
			uint64_t src_addr;
			uint64_t dst_addr;
			uint64_t comp_wr_addr;
			uint32_t comp_wr_data;
			uint32_t size;
			enum hltests_dma_direction dma_dir;
			enum hltests_endian_swap endian_swap;
			bool memset;
		} dma;
		struct {
			uint64_t src_addr;
			uint32_t size;
			uint8_t upper_cp;
		} cp_dma;
		struct {
			uint64_t table_addr;
			uint64_t index_addr;
			enum hl_tests_size_desc size_desc;
		} cb_list;
		struct {
			uint64_t src_addr;
			uint8_t load;
			uint8_t exe;
			enum hl_tests_load_dst load_dst;
			enum hl_tests_predicates_map pred_map;
			enum hl_tests_exe_type exe_type;
		} load_and_exe;
	};
};

struct hltests_monitor {
	uint32_t qid; /* currently, reserved only for Gaudi3's PDMA/ARC */
	union {
		uint64_t mon_address;
		uint8_t cq_id;
	};
	uint64_t sob_val;
	uint32_t mon_payload;
	bool avoid_arm_mon;
	uint16_t sob_id;
	uint16_t mon_id;
	uint8_t long_mode;
	uint8_t cq_enable;
	uint8_t sm_data_config;
	bool auto_zero;
	enum hl_tests_mon_wr_num num_writes;
	enum hltests_mon_mode mon_mode;
};

struct hltests_monitor_and_fence {
	uint64_t mon_address;
	uint64_t sob_val;
	uint32_t mon_payload;
	bool cmdq_fence;
	bool dec_fence; /* decrement the fence once it reaches the gate value */
	uint16_t sob_id;
	uint16_t mon_id;
	uint8_t queue_id;
	uint8_t long_mode;
	enum hl_tests_mon_wr_num num_writes;
	enum hltests_mon_mode mon_mode;
};

enum hltests_arb {
	ARB_PRIORITY = 0,
	ARB_WRR
};

struct hltests_arb_info {
	enum hltests_arb arb;
	union {
		uint32_t weight[NUM_OF_STREAMS];
		uint32_t priority[NUM_OF_STREAMS];
	};
	uint32_t arb_mst_quiet_val;
};

enum hltests_async_event_id {
	FIX_POWER_ENV_S,
	FIX_POWER_ENV_E,
};

struct hltests_cq_config {
	int fd;
	uint32_t qid; /* currently, reserved only for Gaudi3's PDMA/ARC */
	uint64_t cq_address;
	uint16_t cq_size_log2;
	uint16_t cq_id;
	uint16_t interrupt_id;
	uint8_t inc_mode;
};

struct hltests_cs_chunk {
	void *cb_ptr;
	uint32_t cb_size;
	uint32_t queue_index;
};

struct hltests_direct_cq_write {
	uint64_t value;
	uint32_t cq_id;
	uint32_t qid;
};

struct hltests_sched_arc_cmd_params {
	union {
		struct {
			uint32_t padding_cnt_in_bytes;
		} nop;
		struct {
			uint32_t fence_id;
			uint32_t target;
		} fence_wait;
		struct {
			uint32_t comp_group_index;
			uint32_t target_value;
			uint32_t rel_so_set;
		} alloc_barrier;
		struct {
			uint32_t num_engine_group_type;
			uint8_t *engine_group_type;
		} dispatch_barrier;
		struct {
			uint32_t engine_group_type;
			uint32_t size;
			uint32_t engine_cpu_id;
			uint64_t addr;
		} dispatch_static_ecb_list;
	};
};

/**
 * struct monitor_dma_test - manage monitoring of (long) DMA operation
 * @tid: monitoring thread ID.
 * @cond: condition variable to signal the monitoring thread.
 * @mutex: mutex of the cond variable.
 * @qid: DMA queue ID to be monitored.
 * @fd: file descriptor
 * @poll_interval_sec: interval in seconds to poll the DMA status.
 */
struct monitor_dma_test {
	pthread_t tid;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	uint32_t qid;
	int fd;
	int poll_interval_sec;
};

/* the size of the arc log cyclic buffer should be power of 2 */
#define ARC_LOG_BUF_SIZE_LOG2 20
#define ARC_LOG_BUF_SIZE (1 << ARC_LOG_BUF_SIZE_LOG2)
#define ARC_LOG_BUF_NEXT_CI(ci) (((ci) + 1) & (~ARC_LOG_BUF_SIZE))

struct hltests_asic_funcs {
	uint32_t (*add_arb_en_pkt)(void *buffer, uint32_t buf_off,
			struct hltests_pkt_info *pkt_info,
			struct hltests_arb_info *arb_info,
			uint32_t queue_id, bool enable);
	uint32_t (*add_cq_config_pkt)(void *buffer, uint32_t buf_off,
			struct hltests_cq_config *cq_config);
	uint32_t (*add_pdma_ch_bw_config_pkt)(int fd, void *buffer,
			uint32_t buf_off, int qid, bool set_lbw);
	uint32_t (*add_monitor_and_fence)(int fd,
			enum hltests_dcore_separation_mode dcore_sep_mode,
			void *buffer, uint32_t buf_off,
			struct hltests_monitor_and_fence *mon_and_fence);
	uint32_t (*add_monitor)(void *buffer, uint32_t buf_off,
					struct hltests_monitor *mon);
	uint64_t (*get_fence_addr)(int fd, uint32_t qid, bool cmdq_fence);
	uint32_t (*add_nop_pkt)(void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);
	uint32_t (*add_undef_opcode_pkt)(void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);
	uint32_t (*add_msg_barrier_pkt)(void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);
	uint32_t (*add_wreg32_pkt)(void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);
	uint32_t (*add_arb_point_pkt)(void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);
	uint32_t (*add_msg_long_pkt)(void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);
	uint32_t (*add_msg_short_pkt)(void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);
	uint32_t (*add_arm_monitor_pkt)(void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);
	uint32_t (*add_write_to_sob_pkt)(void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);
	uint32_t (*add_fence_pkt)(void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);
	uint32_t (*add_dma_pkt)(void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);
	uint32_t (*add_cp_dma_pkt)(void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);
	uint32_t (*add_cb_list_pkt)(void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);
	uint32_t (*add_load_and_exe_pkt)(void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);
	uint32_t (*get_dma_down_qid)(int fd,
			enum hltests_dcore_separation_mode dcore_sep_mode,
			enum hltests_stream_id stream);
	uint32_t (*get_dma_up_qid)(int fd,
			enum hltests_dcore_separation_mode dcore_sep_mode,
			enum hltests_stream_id stream);
	uint32_t (*get_ddma_qid)(int fd,
			enum hltests_dcore_separation_mode dcore_sep_mode,
			int dma_ch,
			enum hltests_stream_id stream);
	uint8_t (*get_ddma_cnt)(int fd,
			enum hltests_dcore_separation_mode dcore_sep_mode);
	uint32_t (*get_pdma_qid)(int fd, uint16_t dma_idx);
	uint8_t (*get_pdma_ch_cnt)(int fd,
			enum hltests_dcore_separation_mode dcore_sep_mode);
	uint32_t (*get_tpc_qid)(int fd,
			enum hltests_dcore_separation_mode dcore_sep_mode,
			uint8_t tpc_id, enum hltests_stream_id stream);
	uint32_t (*get_mme_id)(int fd, uint32_t qid);
	uint32_t (*get_mme_qid)(
			enum hltests_dcore_separation_mode dcore_sep_mode,
			uint8_t mme_id,	enum hltests_stream_id stream);
	uint8_t (*get_tpc_cnt)(int fd,
			enum hltests_dcore_separation_mode dcore_sep_mode);
	uint8_t (*get_mme_cnt)(int fd,
			enum hltests_dcore_separation_mode dcore_sep_mode,
			bool master_slave_mode);
	uint32_t (*get_nic_qid)(enum hltests_dcore_separation_mode dcore_sep_mode,
			uint8_t nic_id,	enum hltests_stream_id stream);
	uint16_t (*get_first_avail_sob)(int fd);
	uint16_t (*get_first_avail_mon)(int fd);
	uint16_t (*get_first_avail_cq)(int fd);
	uint64_t (*get_sob_base_addr)(int fd);
	uint64_t (*get_sob_lbw_offset)(int fd, uint32_t sob_idx);
	uint32_t (*get_sob_value)(int fd, uint32_t sob_idx);
	int (*set_sob_value)(int fd, uint32_t sob_idx, uint32_t value);
	uint64_t (*get_lbw_base_addr)(int fd);
	uint64_t (*get_any_mappable_hw_block_base_addr)(int fd);
	uint16_t (*get_cache_line_size)(void);
	int (*asic_priv_init)(struct hltests_device *hdev);
	void (*asic_priv_fini)(struct hltests_device *hdev);
	int (*dram_pool_alloc)(struct hltests_device *hdev, uint64_t size,
				uint64_t *return_addr);
	void (*dram_pool_free)(struct hltests_device *hdev, uint64_t addr,
				uint64_t size);
	int (*va_pool_alloc)(struct hltests_device *hdev, uint64_t size, uint64_t *return_addr);
	void (*va_pool_free)(struct hltests_device *hdev, uint64_t addr, uint64_t size);
	int (*submit_cs)(int fd, struct hltests_cs_chunk *restore_arr,
				uint32_t restore_arr_size,
				struct hltests_cs_chunk *execute_arr,
				uint32_t execute_arr_size,
				uint32_t flags, uint32_t timeout,
				uint64_t *seq);
	int (*wait_for_cs)(int fd, uint64_t seq, uint64_t timeout_us);
	int (*wait_for_cs_until_not_busy)(int fd, uint64_t seq);
	int (*get_max_pll_idx)(void);
	const char *(*stringify_pll_idx)(uint32_t pll_idx);
	const char *(*stringify_pll_type)(uint32_t pll_idx, uint8_t type_idx);
	uint64_t (*get_dram_va_hint_mask)(void);
	uint64_t (*get_dram_va_reserved_addr_start)(void);
	uint32_t (*get_sob_id)(uint32_t base_addr_off);
	uint16_t (*get_mon_cnt_per_dcore)(void);
	int (*get_stream_master_qid_arr)(uint32_t **qid_arr);
	uint32_t (*add_sched_arc_nop_cmd)(void *buffer, uint32_t buf_off,
					struct hltests_sched_arc_cmd_params *params);
	uint32_t (*add_sched_arc_dispatch_static_ecb_list)(void *buffer, uint32_t buf_off,
					struct hltests_sched_arc_cmd_params *params);
	uint32_t (*get_arc_cb_suffix_size)(void);
	uint32_t (*arc_get_max_cpuid)(void);
	uint64_t (*arc_get_va_range_host_start)(void);
	uint64_t (*arc_get_va_range_dram_start)(void);
	void (*arc_get_sched_cpuid_range)(int *sched_first, int *sched_last);
	void (*arc_get_engine_cpuid_range)(int *engine_first, int *engine_last);
	int (*asic_load_fw_to_arcs)(struct iterate_arcs_ctx *ctx);
	void (*set_arc_asic_fw_load_params)(struct arc_asic_fw_load_params *load_param);
	int (*arc_set_asic_model)(int fd, uint32_t cpu_id, struct hltests_arc_db *arc_db);
	int (*arc_set_regions)(int fd, uint32_t cpu_id,	struct hltests_arc_db *arc_db);
	int (*arc_set_config)(int fd, uint32_t cpu_id, struct hltests_arc_db *arc_db,
				uint16_t run_arc_done_sob);
	int (*arc_map_lbw_blocks)(int fd, uint32_t cpu_id, struct hltests_arc_db *arc_db);
	uint32_t (*arc_get_num_schedulers)(void);
	int (*wait_arc_run_done)(int fd, struct hltests_arc_db *arc_db, uint16_t arc_run_done_sob);
	int (*arc_activate)(int fd, uint32_t cpu_id, struct hltests_arc_db *arc_db);
	void (*arc_unmap_lbw_blocks)(int fd, uint32_t cpu_id, struct hltests_arc_db *arc_db);
	int (*arc_set_enabled_cores)(int fd, struct hltests_arc_db *arc_db);
	int (*arc_configure_scheduler_streams)(int fd, struct hltests_arc_db *arc_db);
	uint32_t (*pdma_get_max_ch_id)(int fd);
	int (*pdma_map_lbw_blocks)(int fd, struct hltests_pdma_db *pdma_db);
	void (*pdma_unmap_lbw_blocks)(int fd, struct hltests_pdma_db *pdma_db);
	int (*pdma_config_ch_blocks)(int fd, struct hltests_pdma_db *pdma_db);
	int (*arc_get_cpu_id_eng_group)(int fd, uint32_t queue_idx,
					uint32_t *cpu_id, uint32_t *eng_group);
	uint64_t (*get_tc_base_addr)(uint32_t core_id);
	int (*get_async_event_id)(enum hltests_async_event_id hltests_event_id,
					uint32_t *asic_event_id);
	uint32_t (*get_cq_patch_size)(uint32_t qid);
	uint32_t (*get_max_pkt_size)(int fd, bool mb, bool eb, uint32_t qid);
	uint64_t (*add_direct_write_cq_pkt)(int fd, void *buffer, uint32_t buf_off,
					struct hltests_direct_cq_write *direct_cq_write);
	void (*monitor_dma_test_progress)(struct monitor_dma_test *params);
	uint16_t (*cq_db_get_available_sob)(int fd);
	uint16_t (*cq_db_get_available_mon)(int fd);
	uint16_t (*cq_db_get_available_cq)(int fd);
	uint32_t (*add_cq_db_set_cq_queue_pkt)(void *buffer, uint32_t buf_off,
					struct hltests_cq_config *cq_config, uint16_t sob_id);
	int (*mme_dma_init)(int fd);
	uint32_t (*prepare_mme_dma_req)(int fd, void *cb, uint32_t cb_size, uint64_t src,
			uint64_t dst, uint8_t mme_idx, uint64_t size);
	uint32_t (*get_mme_dma_cb_size)(int fd, int num_of_lindma_pkts, uint32_t size);
	uint64_t (*get_razwi_addr)(enum err_trigger);
	uint64_t (*get_pb_secured_addr)(void);
	int (*edp_get_engines_list)(int fd, uint32_t **engine_ids, uint32_t *engine_ids_size);
	uint64_t (*get_fw_mem_addr)(uint64_t size);
	int (*debug_completion)(int fd, uint32_t dbe_sob_idx, uint32_t sync_sob_idx,
					uint32_t expected_dbe_sob, uint32_t expected_sync_sob);
	uint64_t (*get_tpc_intr_cause_reg)(uint32_t tpc_idx);
	uint16_t (*qid_to_eid)(uint16_t qid);
	size_t (*get_coll_resources_by_type)(int fd, struct hltests_mmapped_resource res[],
					     size_t res_len, uint32_t resource_type);
	struct hltests_nic_asic_funcs *nic_funcs;
};

struct hltests_memory {
	uint64_t device_handle;
	void *host_ptr;
	uint64_t device_virt_addr;
	uint64_t size;
	bool is_huge;
	bool is_host;
	bool is_pool;
};

// Forward declaration after struct definitions
const struct hltests_memory *gaudi_allocate_device_mem_ret_mem(int fd, uint64_t size, uint64_t page_size, enum hltests_contiguous contiguous);

struct hltest_host_meminfo {
	uint64_t mem_total;
	uint64_t mem_free;
	uint64_t mem_available;
	uint64_t page_size;
	uint64_t hugepage_total;
	uint64_t hugepage_free;
	uint64_t hugepage_size;
};

struct mem_pool {
	pthread_mutex_t lock;
	uint64_t start;
	uint32_t page_size;
	uint32_t pool_npages;
	uint8_t *pool;
};

extern char asic_names[HLTHUNK_DEVICE_MAX][20];

void hltests_parser(int argc, const char **argv, const char * const*usage,
				unsigned long expected_device_mask
#ifndef HLTESTS_LIB_MODE
			, const struct CMUnitTest * const tests, int num_tests
#endif
			);

const char *hltests_get_parser_pciaddr(void);
void hltests_override_parser_pciaddr(const char *pciaddr);
uint32_t hltests_get_parser_enable_arc_log(void);
const char *hltests_get_config_filename(void);
int hltests_get_parser_run_disabled_tests(void);
int hltests_get_verbose_enabled(void);
uint32_t hltests_get_cur_seed(void);
const char *hltests_get_build_path(void);
int hltests_set_build_path(const char *path);
int hltests_get_parser_nic_port(void);
int hltests_get_parser_events_listener(void);
uint32_t hltests_get_parser_negative_tests(void);
uint32_t hltests_get_parser_mini_suite(void);
struct hltests_device *get_hdev_from_fd(int fd);
bool hltests_is_legacy_mode_enabled(int fd);
bool hltests_is_simulator(int fd);
bool hltests_is_goya(int fd);
bool hltests_is_gaudi(int fd);
bool hltests_is_gaudi2(int fd);
bool hltests_is_gaudi3(int fd);
bool hltests_is_gaudi_family(int fd);
bool hltests_is_pldm(int fd);
bool hltests_is_device_idle_and_operational(int fd);

int hltests_run_group_tests(const char *group_name,
				const struct CMUnitTest * const tests,
				const size_t num_tests,
				CMFixtureFunction group_setup,
				CMFixtureFunction group_teardown);

uint32_t rand_u32(void);

/*
 * Initialize Mersenne Twister with given seed value.
 */
void seed(uint32_t seed_value);

#include <assert.h>
#include <byteswap.h>
#include <errno.h>
#include <errno.h>
#include <fcntl.h>
#include <immintrin.h>
#include <inttypes.h>
#include <linux/mman.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <syscall.h>
#include <time.h>
#include <unistd.h>

#include <infiniband/hbldv.h>

#ifndef MAP_HUGE_2MB
	#define MAP_HUGE_2MB    (21 << MAP_HUGE_SHIFT)
#endif

#define FRAG_MEM_MULT 3

#define BUILD_PATH_MAX_LENGTH	256

#define PSOC_FREQ_GHZ		0.05

#ifndef HLTESTS_LIB_MODE
struct hltests_thread_params {
	const char *group_name;
	const struct CMUnitTest *tests;
	size_t num_tests;
	CMFixtureFunction group_setup;
	CMFixtureFunction group_teardown;
};
#endif

static pthread_mutex_t table_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_spinlock_t rand_lock;
static khash_t(ptr) * dev_table;

static long asic_mask_for_testing = HLTEST_DEVICE_MASK_DONT_CARE;

#ifndef HLTESTS_LIB_MODE
static pthread_barrier_t barrier;

static int num_devices = 1;
#endif

static const char *parser_pciaddr;

char asic_names[HLTHUNK_DEVICE_MAX][20] = {
	[HLTHUNK_DEVICE_INVALID] = "Invalid",
	[HLTHUNK_DEVICE_GOYA] = "Goya",
	[HLTHUNK_DEVICE_GAUDI] = "Gaudi",
	[HLTHUNK_DEVICE_GAUDI_HL2000M] = "Gaudi_HL2000M",
	[HLTHUNK_DEVICE_GRECO] = "Greco",
	[HLTHUNK_DEVICE_GAUDI2] = "Gaudi2",
	[HLTHUNK_DEVICE_GAUDI2B] = "Gaudi2B",
	[HLTHUNK_DEVICE_GAUDI2C] = "Gaudi2C",
	[HLTHUNK_DEVICE_GAUDI2D] = "Gaudi2D",
	[HLTHUNK_DEVICE_GAUDI3] = "Gaudi3",
	[HLTHUNK_DEVICE_GAUDI3D] = "Gaudi3D",
	[HLTHUNK_DEVICE_DONT_CARE] = "Don't care"
};

/* translate device name (enum) to device mask */
unsigned long device_enum_to_device_mask[HLTHUNK_DEVICE_MAX] = {
	[HLTHUNK_DEVICE_INVALID] = HLTEST_DEVICE_MASK_INVALID,
	[HLTHUNK_DEVICE_GOYA] = HLTEST_DEVICE_MASK_GOYA,
	[HLTHUNK_DEVICE_GAUDI] = HLTEST_DEVICE_MASK_GAUDI,
	[HLTHUNK_DEVICE_GAUDI_HL2000M] = HLTEST_DEVICE_MASK_GAUDI_HL2000M,
	[HLTHUNK_DEVICE_GRECO] = HLTEST_DEVICE_MASK_GRECO,
	[HLTHUNK_DEVICE_GAUDI2] = HLTEST_DEVICE_MASK_GAUDI2,
	[HLTHUNK_DEVICE_GAUDI2B] = HLTEST_DEVICE_MASK_GAUDI2B,
	[HLTHUNK_DEVICE_GAUDI2C] = HLTEST_DEVICE_MASK_GAUDI2C,
	[HLTHUNK_DEVICE_GAUDI2D] = HLTEST_DEVICE_MASK_GAUDI2D,
	[HLTHUNK_DEVICE_GAUDI3] = HLTEST_DEVICE_MASK_GAUDI3,
	[HLTHUNK_DEVICE_DONT_CARE] = HLTEST_DEVICE_MASK_DONT_CARE,
	[HLTHUNK_DEVICE_GAUDI3D] = HLTEST_DEVICE_MASK_GAUDI3D,
};

static struct hltests_module_params_info default_module_params = {
	.gaudi_huge_page_optimization = 1,
	.timeout_locked = 5,
	.reset_on_lockup = 1,
	.pldm = 0,
	.mmu_enable = 1,
	.clock_gating = 1,
	.mme_enable = 1,
	.tpc_mask = 0x3FF,
	.nic_ports_mask = 0,
	.dram_enable = 1,
	.cpu_enable = 1,
	.reset_pcilink = 0,
	.config_pll = 0,
	.cpu_queues_enable = 1,
	.fw_loading = 0x3,
	.heartbeat = 1,
	.axi_drain = 1,
	.security_enable = 1,
	.sram_scrambler_enable = 1,
	.dram_scrambler_enable = 1,
	.cache_enabled = 0,
	.hbm_ecc_enable = 1,
	.compatibility_mode = 0,
	.hard_reset_on_fw_events = 1,
	.decoder_mask = 0,
	.rotator_mask = 0,
	.fw_loading_ext = 0
};

struct hltests_device *get_hdev_from_fd(int fd)
{
	struct hltests_device *hdev;
	khint_t k;

	pthread_mutex_lock(&table_lock);

	k = kh_get(ptr, dev_table, fd);
	if (k == kh_end(dev_table)) {
		pthread_mutex_unlock(&table_lock);
		return NULL;
	}

	hdev = kh_val(dev_table, k);

	pthread_mutex_unlock(&table_lock);

	return hdev;
}

double get_timediff_usec(struct timespec *begin, struct timespec *end)
{
	return (end->tv_nsec - begin->tv_nsec) / 1000.0 +
						1000000.0 * (end->tv_sec  - begin->tv_sec);
}

double get_timediff_sec(struct timespec *begin, struct timespec *end)
{
	return (end->tv_nsec - begin->tv_nsec) / 1000000000.0 +
						(end->tv_sec  - begin->tv_sec);
}

static int create_mem_maps(struct hltests_device *hdev)
{
	int rc;
	bool mem_table_host_lock = false;
	bool mem_table_device_lock = false;
	bool mmap_table_lock = false;
	bool cb_table_lock = false;

	hdev->mem_table_host = kh_init(ptr64);
	if (!hdev->mem_table_host) {
		rc = -ENOMEM;
		goto cleanup;
	}

	hdev->mem_table_device = kh_init(ptr64);
	if (!hdev->mem_table_device) {
		rc = -ENOMEM;
		goto cleanup;
	}

	hdev->mmap_table = kh_init(mapping);
	if (!hdev->mmap_table) {
		rc = -ENOMEM;
		goto cleanup;
	}

	hdev->cb_table = kh_init(ptr64);
	if (!hdev->cb_table) {
		rc = -ENOMEM;
		goto cleanup;
	}

	rc = pthread_mutex_init(&hdev->mem_table_host_lock, NULL);
	if (rc)
		goto cleanup;
	else
		mem_table_host_lock = true;

	rc = pthread_mutex_init(&hdev->mem_table_device_lock, NULL);
	if (rc)
		goto cleanup;
	else
		mem_table_device_lock = true;

	rc = pthread_mutex_init(&hdev->mmap_table_lock, NULL);
	if (rc)
		goto cleanup;
	else
		mmap_table_lock = true;

	rc = pthread_mutex_init(&hdev->cb_table_lock, NULL);
	if (rc)
		goto cleanup;
	else
		cb_table_lock = true;

	return 0;

cleanup:
	if (cb_table_lock)
		pthread_mutex_destroy(&hdev->cb_table_lock);
	if (mmap_table_lock)
		pthread_mutex_destroy(&hdev->mmap_table_lock);
	if (mem_table_device_lock)
		pthread_mutex_destroy(&hdev->mem_table_device_lock);
	if (mem_table_host_lock)
		pthread_mutex_destroy(&hdev->mem_table_host_lock);
	if (hdev->cb_table)
		kh_destroy(ptr64, hdev->cb_table);
	if (hdev->mmap_table)
		kh_destroy(mapping, hdev->mmap_table);
	if (hdev->mem_table_device)
		kh_destroy(ptr64, hdev->mem_table_device);
	if (hdev->mem_table_host)
		kh_destroy(ptr64, hdev->mem_table_host);
	return rc;
}

static void destroy_mem_maps(struct hltests_device *hdev)
{
	kh_destroy(mapping, hdev->mmap_table);
	kh_destroy(ptr64, hdev->mem_table_host);
	kh_destroy(ptr64, hdev->mem_table_device);
	kh_destroy(ptr64, hdev->cb_table);
	pthread_mutex_destroy(&hdev->mmap_table_lock);
	pthread_mutex_destroy(&hdev->mem_table_host_lock);
	pthread_mutex_destroy(&hdev->mem_table_device_lock);
	pthread_mutex_destroy(&hdev->cb_table_lock);
}


bool hltests_is_dma_dir_from_dram(enum hltests_dma_direction dir)
{
	return dir == DMA_DIR_DRAM_TO_SRAM ||
		dir == DMA_DIR_DRAM_TO_HOST ||
		dir == DMA_DIR_DRAM_TO_DRAM;
}


static void *hltests_thread_start(void *args)
{
	struct hltests_thread_params *params =
			(struct hltests_thread_params *) args;
	int rc;

	/*
	 * PTHREAD_BARRIER_SERIAL_THREAD is returned to one unspecified thread
	 * and zero is returned to each of the remaining threads.
	 */
	rc = pthread_barrier_wait(&barrier);
	if (rc && rc != PTHREAD_BARRIER_SERIAL_THREAD)
		return NULL;

	rc = _cmocka_run_group_tests(params->group_name,
				params->tests, params->num_tests,
				params->group_setup, params->group_teardown);
	if (rc)
		return NULL;

	return args;
}



static bool hltests_is_asic_type_valid(enum hlthunk_device_name actual_asic_type)
{
	unsigned long actual_asic_mask;

	actual_asic_mask = device_enum_to_device_mask[actual_asic_type];
	if (!(asic_mask_for_testing & actual_asic_mask)) {
		printf("Expected device mask %#lx but detected device %s (%#lx)\n",
				asic_mask_for_testing,
				asic_names[actual_asic_type],
				actual_asic_mask);
		return false;
	}

	return true;
}



int hltests_control_dev_close(int fd)
{
	struct hltests_device *hdev;
	khint_t k;

	pthread_mutex_lock(&table_lock);

	k = kh_get(ptr, dev_table, fd);
	if (k == kh_end(dev_table)) {
		pthread_mutex_unlock(&table_lock);
		return -ENODEV;
	}

	hdev = kh_val(dev_table, k);

	if (--hdev->refcnt) {
		pthread_mutex_unlock(&table_lock);
		return 0;
	}

	hlthunk_close(hdev->fd);

	kh_del(ptr, dev_table, k);
	pthread_mutex_unlock(&table_lock);

	hlthunk_free(hdev);

	return 0;
}


int wait_until_device_idle(int fd, uint32_t timeout_sec)
{
	uint64_t elapsed_usec = 0, timeout_usec = timeout_sec * USEC_PER_SEC;
	uint32_t sleep_usec = USEC_PER_MSEC;
	int rc;

	while (elapsed_usec < timeout_usec) {
		if (hlthunk_is_device_idle(fd))
			break;

		rc = usleep(sleep_usec);
		if (rc)
			return -errno;

		elapsed_usec += sleep_usec;
	}

	return elapsed_usec < timeout_usec ? 0 : -ETIMEDOUT;
}

int wait_until_device_not_in_reset(int fd)
{
	unsigned int elapsed_sec = 0, sleep_sec = 1, timeout_sec;
	int device_status;

	timeout_sec = hltests_is_pldm(fd) ? PLDM_RESET_WAIT_TIMEOUT_SEC : RESET_WAIT_TIMEOUT_SEC;

	while (elapsed_sec < timeout_sec) {
		device_status = hlthunk_get_device_status_info(fd);
		if (device_status < 0)
			return device_status;

		if (device_status != HL_DEVICE_STATUS_IN_RESET &&
				device_status != HL_DEVICE_STATUS_IN_RESET_AFTER_DEVICE_RELEASE)
			break;

		sleep(sleep_sec);
		elapsed_sec += sleep_sec;
	}

	return elapsed_sec < timeout_sec ? 0 : -ETIMEDOUT;
}

int hltests_open(const char *busid)
{
	enum hlthunk_device_name actual_asic_type;
	struct hltests_device *hdev;
	int ctrl_fd, fd, rc;
	khint_t k;

	if (!asic_mask_for_testing) {
		printf("Expecting invalid ASIC!!!\n");
		printf("Something is very wrong, exiting...\n");
		return -EINVAL;
	}

	pthread_mutex_lock(&table_lock);

	/* Open control device first in order to compare against asic_mask_for_testing */
	rc = ctrl_fd = hlthunk_open_control_by_name(HLTHUNK_DEVICE_DONT_CARE, busid);
	if (ctrl_fd < 0)
		goto out;

	actual_asic_type = hlthunk_get_device_name_from_fd(ctrl_fd);
	if (!hltests_is_asic_type_valid(actual_asic_type)) {
		rc = -EINVAL;
		hlthunk_close(ctrl_fd);
		pthread_mutex_unlock(&table_lock);
		exit(0);
	}

	/* If device is in reset, wait until reset process is done */
	rc = wait_until_device_not_in_reset(ctrl_fd);
	hlthunk_close(ctrl_fd);
	if (rc)
		goto out;

	rc = fd = hlthunk_open(HLTHUNK_DEVICE_DONT_CARE, busid);
	if (fd < 0)
		goto out;

	k = kh_get(ptr, dev_table, fd);
	if (k != kh_end(dev_table)) {
		/* found, just incr refcnt */
		hdev = kh_val(dev_table, k);
		hdev->refcnt++;
		goto out;
	}

	/* not found, create new device */
	hdev = hlthunk_malloc(sizeof(struct hltests_device));
	if (!hdev) {
		rc = -ENOMEM;
		goto close_device;
	}

	hdev->fd = fd;
	hdev->refcnt = 1;
	hdev->device_id = hlthunk_get_device_id_from_fd(fd);

	k = kh_put(ptr, dev_table, fd, &rc);
	kh_val(dev_table, k) = hdev;

	switch (actual_asic_type) {
	case HLTHUNK_DEVICE_GOYA:
		goya_tests_set_asic_funcs(hdev);
		break;
	case HLTHUNK_DEVICE_GAUDI:
	case HLTHUNK_DEVICE_GAUDI_HL2000M:
		gaudi_tests_set_asic_funcs(hdev);
		break;
	case HLTHUNK_DEVICE_GAUDI2:
	case HLTHUNK_DEVICE_GAUDI2B:
	case HLTHUNK_DEVICE_GAUDI2C:
	case HLTHUNK_DEVICE_GAUDI2D:
		gaudi2_tests_set_asic_funcs(hdev);
		break;

	case HLTHUNK_DEVICE_GAUDI3:
	case HLTHUNK_DEVICE_GAUDI3D:
		gaudi3_tests_set_asic_funcs(hdev);
		break;
	default:
		printf("Invalid device type 0x%x\n", hdev->device_id);
		rc = -ENXIO;
		goto remove_device;
	}

	memset(&hdev->module_params, 0, sizeof(hdev->module_params));
	rc = hltests_get_module_params_info(fd, &hdev->module_params);
	if (rc) {
		printf("Failed to retrieve values of module parameters\n");
		goto remove_device;
	}

	rc = hdev->asic_funcs->asic_priv_init(hdev);
	if (rc)
		goto remove_device;

	rc = create_mem_maps(hdev);
	if (rc)
		goto destroy_asic_priv;

	pthread_mutex_unlock(&table_lock);

	return fd;

destroy_asic_priv:
	hdev->asic_funcs->asic_priv_fini(hdev);
remove_device:
	kh_del(ptr, dev_table, k);
close_device:
	hlthunk_close(fd);
out:
	pthread_mutex_unlock(&table_lock);
	return rc;
}

int hltests_close(int fd)
{
	struct hltests_device *hdev;
	khint_t k;

	pthread_mutex_lock(&table_lock);

	k = kh_get(ptr, dev_table, fd);
	if (k == kh_end(dev_table)) {
		pthread_mutex_unlock(&table_lock);
		return -ENODEV;
	}

	hdev = kh_val(dev_table, k);

	if (--hdev->refcnt) {
		pthread_mutex_unlock(&table_lock);
		return 0;
	}

	hdev->asic_funcs->asic_priv_fini(hdev);

	destroy_mem_maps(hdev);

	hlthunk_close(hdev->fd);

	kh_del(ptr, dev_table, k);
	pthread_mutex_unlock(&table_lock);

	hlthunk_free(hdev);

	return 0;
}





int hltests_reset_events(struct hltests_state *tests_state)
{
	struct hltests_listener_thread_params *params = &tests_state->listener_thread_params;
	int rc;

	rc = pthread_mutex_lock(&params->lock);
	if (rc)
		return rc;

	params->notifier_events = 0;
	return pthread_mutex_unlock(&params->lock);
}

static void hltests_set_recovery_status(struct hltests_state *tests_state,
					enum hltests_recovery_status recovery_status)
{
	struct hltests_listener_thread_params *params = &tests_state->listener_thread_params;

	params->recovery_status = recovery_status;
}

static void hltests_print_cs_timeout_event_info(int fd)
{
	struct hlthunk_event_record_cs_timeout cs_timeout;
	int rc;

	rc = hlthunk_get_event_record(fd, HLTHUNK_CS_TIMEOUT, &cs_timeout);
	if (rc) {
		printf("Failed to retrieve information for a CS_TIMEOUT event (%d)\n", rc);
		return;
	}

	printf("timestamp: %"PRId64"\n", cs_timeout.timestamp);
	printf("seq: %"PRIu64"\n", cs_timeout.seq);
}

static void hltests_handle_notifier_events(int fd, uint64_t notifier_events, uint64_t notifier_cnt)
{
	printf("[events listener] Received a notification: events %#"PRIx64", cnt: %"PRIu64"\n",
		notifier_events, notifier_cnt);

	if (notifier_events & HL_NOTIFIER_EVENT_CS_TIMEOUT) {
		printf("[events listener] CS_TIMEOUT\n");
		hltests_print_cs_timeout_event_info(fd);
	}

	/* TODO: use INFO IOCTL to get debug information about the events */
}




static int hltests_signal_waiting_threads(struct hltests_state *tests_state,
						uint64_t notifier_events)
{
	struct hltests_listener_thread_params *params = &tests_state->listener_thread_params;
	int rc;

	rc = pthread_mutex_lock(&params->lock);
	if (rc)
		return rc;

	params->notifier_events |= notifier_events;

	rc = pthread_cond_broadcast(&params->cond);
	if (rc) {
		pthread_mutex_unlock(&params->lock);
		return rc;
	}

	return pthread_mutex_unlock(&params->lock);
}



static void *hltests_listener_thread_func(void *args)
{
	struct hltests_state *tests_state = args;
	struct hltests_listener_thread_params *params = &tests_state->listener_thread_params;
	uint64_t notifier_events, notifier_cnt;
	int rc;
	params->listener_tid = syscall(SYS_gettid);

	/* PTHREAD_BARRIER_SERIAL_THREAD is returned to one unspecified thread and zero is returned
	 * to each of the remaining threads.
	 */
	rc = pthread_barrier_wait(&params->barrier);
	if (rc && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
		printf("Failed on barrier wait from events listener thread (%d)\n", rc);
		return NULL;
	}

	while (true) {
		/* hlthunk_notifier_recv() calls internally to poll() which is a cancellation point,
		 * so pthread_cancel() will be able to terminate the thread when needed.
		 */
		rc = hlthunk_notifier_recv(tests_state->fd, params->handle, &notifier_events,
						&notifier_cnt, 0, INT_MAX);
		if (rc) {
			printf("Failed to receive a notification event (%d)\n", rc);
			return NULL;
		}

		/* TODO: a debug print to be removed when SW-159137 is solved */
		if (notifier_cnt != 1)
			printf("num of events is %lu\n", notifier_cnt);

		/* TODO: a debug print to be removed when SW-159137 is solved */
		if (notifier_events == 0)
			printf("returned with no events\n");

		/* No notification */
		if (!notifier_cnt)
			continue;

		/* Do not handle events during negative testing */
		if (!hltests_get_parser_negative_tests())
			hltests_handle_notifier_events(tests_state->fd, notifier_events,
										notifier_cnt);

		if (notifier_events & HL_NOTIFIER_EVENT_DEVICE_RESET)
			hltests_set_recovery_status(tests_state, RECOVERY_STATUS_REQUIRED);

		rc = hltests_signal_waiting_threads(tests_state, notifier_events);
		if (rc) {
			printf("Failed to signal about a notification event (%d)\n", rc);
			return NULL;
		}
	}

	return args;
}



static int hltests_start_listener_thread(struct hltests_state *tests_state)
{
	struct hltests_listener_thread_params *params = &tests_state->listener_thread_params;
	int rc, fd = tests_state->fd;

	if (!hltests_get_parser_events_listener())
		return 0;

	memset(params, 0, sizeof(*params));

	rc = pthread_barrier_init(&params->barrier, NULL, 2);
	if (rc) {
		printf("Failed to initialize barrier for listener thread (%d)\n", rc);
		return rc;
	}

	rc = pthread_cond_init(&params->cond, NULL);
	if (rc) {
		printf("Failed to initialize condition variable for listener thread (%d)\n", rc);
		goto destroy_barrier;
	}

	rc = pthread_mutex_init(&params->lock, NULL);
	if (rc) {
		printf("Failed to initialize mutex for listener thread (%d)\n", rc);
		goto destroy_cond;
	}

	params->handle = hlthunk_notifier_create(fd);
	if (params->handle < 0) {
		printf("Failed to create a notifier object (%d)\n", params->handle);
		rc = params->handle;
		goto destroy_mutex;
	}

	rc = pthread_create(&params->thread_id, NULL, hltests_listener_thread_func, tests_state);
	if (rc) {
		printf("Failed to create listener thread (%d)\n", rc);
		goto release_notifier;
	}

	/* PTHREAD_BARRIER_SERIAL_THREAD is returned to one unspecified thread and zero is returned
	 * to each of the remaining threads.
	 */
	rc = pthread_barrier_wait(&params->barrier);
	if (rc && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
		printf("Failed on barrier wait while starting listener thread (%d)\n", rc);
		goto cancel_pthread;
	}

	return 0;

cancel_pthread:
	if (pthread_cancel(params->thread_id) == 0)
		pthread_join(params->thread_id, NULL);
release_notifier:
	hlthunk_notifier_release(fd, params->handle);
destroy_mutex:
	pthread_mutex_destroy(&params->lock);
destroy_cond:
	pthread_cond_destroy(&params->cond);
destroy_barrier:
	pthread_barrier_destroy(&params->barrier);
	return rc;
}





static int hltests_stop_listener_thread(struct hltests_state *tests_state)
{
	struct hltests_listener_thread_params *params = &tests_state->listener_thread_params;
	int fd = tests_state->fd;
	int rc;

	if (!hltests_get_parser_events_listener())
		return 0;

	rc = pthread_cancel(params->thread_id);
	if (rc)
		printf("failed to cancel listener thread (%d)\n", rc);
	else
		pthread_join(params->thread_id, NULL);

	rc |= hlthunk_notifier_release(fd, params->handle);
	if (rc)
		printf("failed to release eventfd (%d)\n", rc);

	pthread_mutex_destroy(&params->lock);
	pthread_cond_destroy(&params->cond);
	pthread_barrier_destroy(&params->barrier);
	return rc;
}

static struct hltests_state *hltests_alloc_state(void)
{
	struct hltests_state *tests_state;

	tests_state = hlthunk_malloc(sizeof(*tests_state));
	if (!tests_state)
		goto out;

	tests_state->fd = -1;
	tests_state->imp_fd = -1;
	tests_state->asic_type = HLTHUNK_DEVICE_MAX;
	tests_state->debugfs.addr_fd = -1;
	tests_state->debugfs.data32_fd = -1;
	tests_state->debugfs.data64_fd = -1;
	tests_state->debugfs.clk_gate_fd = -1;

out:
	return tests_state;
}


static int gaudi_setup_common(void **state)
{
	struct hltests_state *tests_state;
	struct hltests_device *hdev;
	int rc, fd;

	tests_state = hltests_alloc_state();
	if (!tests_state)
		return -ENOMEM;

	fd = tests_state->fd = hltests_open(parser_pciaddr);
	if (fd < 0) {
		printf("Failed to open device %d\n", fd);
		rc = fd;
		goto free_state;
	}


	hdev = get_hdev_from_fd(fd);
	if (!hdev) {
		printf("Failed to get hdev from file descriptor %d\n", fd);
		rc = -ENODEV;
		goto close_imp_fd;
	}

	tests_state->mme = !!hdev->module_params.mme_enable;
	tests_state->lkd_security = !!hdev->module_params.security_enable;

	rc = hltests_setup_user_engines(tests_state);
	if (rc)
		goto close_imp_fd;

	rc = hltests_start_listener_thread(tests_state);
	if (rc)
		goto teardown_user_engines;

	rc = hlthunk_get_hw_ip_info(tests_state->fd, &tests_state->hw_ip);
	if (rc)
		goto stop_listener;

	*state = tests_state;

	return 0;

stop_listener:
	hltests_stop_listener_thread(tests_state);

teardown_user_engines:
	hltests_teardown_user_engines(tests_state);

close_imp_fd:
	if (tests_state->imp_fd >= 0)
		close(tests_state->imp_fd);
close_fd:
	if (hltests_close(fd))
		printf("Problem in closing FD, ignoring...\n");
free_state:
	hlthunk_free(tests_state);

	return rc;
}

int hltests_teardown_user_engines(struct hltests_state *tests_state)
{
	int fd = tests_state->fd;
	struct hltests_device *hdev = get_hdev_from_fd(fd);

	if (hltests_is_legacy_mode_enabled(fd))
		return 0;

	hdev = get_hdev_from_fd(fd);
	if (!hdev) {
		printf("Failed to get hdev from file descriptor %d\n", fd);
		return -ENODEV;
	}

	hltests_arc_fini(fd, &hdev->arc_db);
	hltests_completion_db_teardown(fd, &hdev->completion_db);
	hltests_pdma_fini(fd, &hdev->pdma_db);
	hlthunk_free(hdev->arc_db.fw_info);

	return 0;
}

static int gaudi_teardown_common(void **state)
{
	struct hltests_state *tests_state = (struct hltests_state *) *state;
	int rc = 0;

	if (!tests_state)
		return -EINVAL;

	hltests_stop_listener_thread(tests_state);

	hltests_teardown_user_engines(tests_state);

	if (tests_state->imp_fd >= 0)
		close(tests_state->imp_fd);

	if (hltests_close(tests_state->fd))
		printf("Problem in closing FD, ignoring...\n");

	if (tests_state->priv)
		hlthunk_free(tests_state->priv);

	hlthunk_free(*state);

	return rc;
}

int gaudi_setup(void **state)
{
	struct hltests_state *tests_state;
	char pci_bus_id[13];
	int rc, device_idx;

	rc = gaudi_setup_common(state);
	if (rc)
		return rc;

	if (!can_open_debugfs(false))
		return 0;

	/* in case debugfs can be opened- open it */

	tests_state = *state;

	rc = hlthunk_get_pci_bus_id_from_fd(tests_state->fd, pci_bus_id,
						sizeof(pci_bus_id));
	if (rc)
		return rc;

	device_idx = hlthunk_get_device_index_from_pci_bus_id(pci_bus_id);
	if (device_idx < 0)
		return -ENODEV;

	return 0;
}

int gaudi_teardown(void **state)
{
	struct hltests_state *tests_state;

	if (can_open_debugfs(false)) {
		tests_state = (struct hltests_state *) *state;

		if (!tests_state)
			return -EINVAL;

	}

	return gaudi_teardown_common(state);
}

static int hltests_ioctl(int fd, unsigned long request, void *arg)
{
	int ret;

	do {
		ret = ioctl(fd, request, arg);
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN));

	return ret;
}

int hltests_get_module_params_info(int fd,
				struct hltests_module_params_info *info)
{
	struct hl_info_args args;
	struct hl_info_module_params hl_info;
	int rc;

	if (!info)
		return -EINVAL;

	memset(&args, 0, sizeof(args));
	memset(&hl_info, 0, sizeof(hl_info));

	args.op = HL_INFO_MODULE_PARAMS;
	args.return_pointer = (__u64) (uintptr_t) &hl_info;
	args.return_size = sizeof(hl_info);

	rc = hltests_ioctl(fd, DRM_IOCTL_HL_INFO, &args);
	if (rc) {
		*info = default_module_params;
		goto out;
	}

	info->gaudi_huge_page_optimization =
			hl_info.gaudi_huge_page_optimization;
	info->timeout_locked = hl_info.timeout_locked;
	info->reset_on_lockup = hl_info.reset_on_lockup;
	info->pldm = hl_info.pldm;
	info->mmu_enable = hl_info.mmu_enable;
	info->clock_gating = hl_info.clock_gating;
	info->mme_enable = hl_info.mme_enable;
	info->tpc_mask = hl_info.tpc_mask;
	info->nic_ports_mask = hl_info.nic_ports_mask;
	info->dram_enable = hl_info.dram_enable;
	info->cpu_enable = hl_info.cpu_enable;
	info->reset_pcilink = hl_info.reset_pcilink;
	info->config_pll = hl_info.config_pll;
	info->cpu_queues_enable = hl_info.cpu_queues_enable;
	info->fw_loading = hl_info.fw_loading;
	info->fw_loading_ext = hl_info.fw_loading_ext;
	info->heartbeat = hl_info.heartbeat;
	info->axi_drain = hl_info.axi_drain;
	info->security_enable = hl_info.security_enable;
	info->sram_scrambler_enable = hl_info.sram_scrambler_enable;
	info->dram_scrambler_enable = hl_info.dram_scrambler_enable;
	info->hbm_ecc_enable = hl_info.hbm_ecc_enable;
	info->compatibility_mode = hl_info.compatibility_mode;
	info->hard_reset_on_fw_events = hl_info.hard_reset_on_fw_events;
	info->decoder_mask = hl_info.decoder_mask;
	info->rotator_mask = hl_info.rotator_mask;
	info->dram_page_scrub = hl_info.dram_page_scrub;
	info->clock_gating_ext = hl_info.clock_gating_ext;
	info->cache_enabled = hl_info.cache_enabled;
	info->nic_lanes_per_port = hl_info.nic_lanes_per_port;
out:
	return 0;
}

static void *allocate_huge_mem(uint64_t size)
{
#if defined(__powerpc__)
	int mmapFlags = MAP_SHARED | MAP_ANONYMOUS;
#else
	int mmapFlags = MAP_HUGE_2MB | MAP_HUGETLB | MAP_SHARED | MAP_ANONYMOUS;
#endif
	int prot = PROT_READ | PROT_WRITE;
	void *vaddr;

	vaddr = mmap(0, size, prot, mmapFlags, -1, 0);

	if (vaddr == MAP_FAILED) {
		printf("Failed to allocate %lu host memory with huge pages\n",
			size);
		return NULL;
	}

	return vaddr;
}

/**
 * This function allocates memory on the host
 * @param size how much memory to allocate
 * @param huge whether to use huge pages for the memory allocation
 * @return pointer to the struct hltests_memory generated.
 * NULL is returned upon failure.
 */
struct hltests_memory *
hltests_allocate_host_mem_nomap(uint64_t size, enum hltests_huge huge)
{
	struct hltests_memory *mem;

	mem = hlthunk_malloc(sizeof(struct hltests_memory));
	if (!mem)
		return NULL;

	mem->is_host = true;
	mem->is_huge = huge;
	mem->size = size;

	if (mem->is_huge) {
		mem->host_ptr = allocate_huge_mem(size);

		/* Failed to allocate huge memory, fall-back to regular memory */
		if (!mem->host_ptr) {
			mem->is_huge = false;
			mem->host_ptr = malloc(size);
		}
	} else {
		mem->host_ptr = malloc(size);
	}

	if (!mem->host_ptr) {
		printf("Failed to allocate %lu bytes of host memory\n", size);
		goto free_mem_struct;
	}

	return mem;

free_mem_struct:
	hlthunk_free(mem);
	return NULL;
}

/**
 * This function frees host memory allocation which were done using
 * hltests_allocate_host_mem_nomap
 * @param mem pointer to the hltests_memory structure
 * @param huge whether huge pages were used for the memory allocation
 * @return 0 for success, negative value for failure
 */
int gaudi_free_host_mem_nounmap(struct hltests_memory *mem,
					enum hltests_huge huge)
{
	/* contract: device_virt_addr must be released by this stage */
	assert_null(mem->device_virt_addr);

	if (mem->is_huge)
		munmap(mem->host_ptr, mem->size);
	else
		free(mem->host_ptr);

	hlthunk_free(mem);

	return 0;
}

/**
 * This function maps the host memory previously allocated by
 * hltests_allocate_host_mem_nomap to the device virtual address space.
 * @param fd file descriptor of the device to which the function will map
 *           the memory
 * @param mem pointer to the hltests_memory structure
 * @return 0 for success, negative value for failure
 */
int hltests_map_host_mem(int fd, struct hltests_memory *mem)
{
	mem->device_virt_addr = hlthunk_host_memory_map(fd, mem->host_ptr, 0,
							mem->size);
	if (!mem->device_virt_addr) {
		printf("Failed to map host memory to device\n");
		return -1;
	}

	return 0;
}

/**
 * This function unmaps the host memory previously mapped by
 * hltests_map_host_mem.
 * @param mem pointer to the hltests_memory structure
 * @param fd file descriptor of the device to which the memory was mapped
 * @return 0 for success, negative value for failure
 */
int hltests_unmap_host_mem(int fd, struct hltests_memory *mem)
{
	int rc = hlthunk_memory_unmap(fd, mem->device_virt_addr);

	mem->device_virt_addr = 0;
	return rc;
}

/**
 * This function allocates memory on the host, aligned as specified, and will
 * map it to the device virtual address space
 * @param fd file descriptor of the device to which the function will map
 *           the memory
 * @param size how much memory to allocate
 * @param huge whether to use huge pages for the memory allocation
 * @param align desired alignment in bytes, 0 meaning unaligned
 * @return pointer to the host memory. NULL is returned upon failure
 */
void *gaudi_allocate_host_mem_aligned(int fd, uint64_t size,
				enum hltests_huge huge, uint64_t align)
{
	return gaudi_allocate_host_mem_aligned_flags(fd, size, huge, align, 0);

}

/**
 * This function allocates memory on the host, aligned as specified, and will
 * map it to the device virtual address space, allowing to pass custom memory
 * map flags alongside.
 * @param fd file descriptor of the device to which the function will map
 *           the memory
 * @param size how much memory to allocate
 * @param huge whether to use huge pages for the memory allocation
 * @param align desired alignment in bytes, 0 meaning unaligned
 * @param flags memory map flags
 * @return pointer to the host memory. NULL is returned upon failure
 */
void *gaudi_allocate_host_mem_aligned_flags(int fd, uint64_t size,
    
			enum hltests_huge huge, uint64_t align, uint32_t flags)
{
	struct hltests_device *hdev;
	struct hltests_memory *mem;
	khint_t k;
	int rc;

	hdev = get_hdev_from_fd(fd);
	if (!hdev)
		return NULL;

	mem = hlthunk_malloc(sizeof(struct hltests_memory));
	if (!mem)
		return NULL;

	mem->is_host = true;
	mem->is_huge = huge;
	mem->size = size;

	if (mem->is_huge) {
		mem->host_ptr = allocate_huge_mem(size);

		/* Failed to allocate huge memory, fall-back to regular memory */
		if (!mem->host_ptr) {
			mem->is_huge = false;
			mem->host_ptr = align ? aligned_alloc(align, size) : malloc(size);
		}
	} else {
		mem->host_ptr = align ? aligned_alloc(align, size) : malloc(size);
	}

	if (!mem->host_ptr) {
		printf("Failed to allocate %lu bytes of host memory\n", size);
		goto free_mem_struct;
	}

	mem->device_virt_addr = hlthunk_host_memory_map_flags(fd, mem->host_ptr, 0, size,
									flags);

	if (!mem->device_virt_addr) {
		printf("Failed to map host memory to device\n");
		goto free_allocation;
	}

	pthread_mutex_lock(&hdev->mem_table_host_lock);

	k = kh_put(ptr64, hdev->mem_table_host, (uintptr_t) mem->host_ptr, &rc);
	kh_val(hdev->mem_table_host, k) = mem;

	pthread_mutex_unlock(&hdev->mem_table_host_lock);

	return (void *) mem->host_ptr;

free_allocation:
	if (mem->is_huge)
		munmap(mem->host_ptr, size);
	else
		free(mem->host_ptr);
free_mem_struct:
	hlthunk_free(mem);
	return NULL;
}

/**
 * This function allocates memory on the host and will map it to the device
 * virtual address space
 * @param fd file descriptor of the device to which the function will map
 *           the memory
 * @param size how much memory to allocate
 * @param huge whether to use huge pages for the memory allocation
 * @return pointer to the host memory. NULL is returned upon failure
 */
void *gaudi_allocate_host_mem(int fd, uint64_t size, enum hltests_huge huge)
{
	return gaudi_allocate_host_mem_aligned(fd, size, huge, 0);
}



static int allocate_device_mem(struct hltests_device *hdev, struct hltests_memory *mem,
			       size_t page_size, enum hltests_contiguous contiguous)
{
	mem->device_handle =
		hlthunk_device_memory_alloc(hdev->fd, mem->size, page_size, contiguous, false);
	if (!mem->device_handle) {
		E("Failed to allocate %#lx bytes of device memory!", mem->size);
		goto error_memory_alloc;
	}

	mem->device_virt_addr = hlthunk_device_memory_map(hdev->fd, mem->device_handle, 0);
	if (!mem->device_virt_addr) {
		E("Failed to map device memory!");
		goto error_memory_map;
	}

	/* Memory access must go through DMA */
	mem->host_ptr = NULL;

	return 0;

error_memory_map:
	hlthunk_device_memory_free(hdev->fd, mem->device_handle);

error_memory_alloc:
	return -1;
}

/**
 * This function allocates DRAM memory on the device and will map it to
 * the device virtual address space
 * @param fd file descriptor of the device to which the function will map
 *           the memory
 * @param size how much memory to allocate
 * @param page_size what page size to use. 0 means use default page size
 * @param contiguous whether the memory area will be physically contiguous
 * @return pointer to the device memory. This pointer can NOT be dereferenced
 * directly from the host. NULL is returned upon failure
 */
void *gaudi_allocate_device_mem(int fd, uint64_t size, uint64_t page_size,
				  enum hltests_contiguous contiguous)
{
	const struct hltests_memory *mem =
		gaudi_allocate_device_mem_ret_mem(fd, size, page_size, contiguous);

	if (!mem)
		return NULL;

	static_assert(sizeof(void *) >= sizeof(mem->device_virt_addr),
		      "The cast to `void *` can be fatal on 31bit systems, whoever decided to return a `void *` clearly wasn't thinking straight.");
	return (void *)mem->device_virt_addr;
}

/**
 * gaudi_allocate_device_mem_ret_mem() - Allocates a memory buffer on the device.
 *
 * Returns a structure containing all the information regarding the buffer.
 * If the device supports it, the buffer will be memory mapped into process memory and exposed via
 * &struct hltests_memory->host_ptr.
 *
 * @param fd file descriptor of the device to which the function will map
 *           the memory
 * @param size how much memory to allocate
 * @param page_size what page size to use. 0 means use default page size
 * @param contiguous whether the memory area will be physically contiguous
 * @return pointer to the device memory structure, NULL on failure.
 */
const struct hltests_memory *
gaudi_allocate_device_mem_ret_mem(int fd, uint64_t size, uint64_t page_size,
				    enum hltests_contiguous contiguous)
{
	const struct hltests_asic_funcs *asic;
	struct hltests_device *hdev;
	struct hltests_memory *mem;
	khint_t k;
	int rc;

	hdev = get_hdev_from_fd(fd);
	if (!hdev)
		return NULL;

	asic = hdev->asic_funcs;

	mem = hlthunk_malloc(sizeof(struct hltests_memory));
	if (!mem)
		return NULL;

	mem->is_host = false;
	mem->is_pool = false;
	mem->size = size;

	if (!asic->dram_pool_alloc(hdev, size, &mem->device_virt_addr)) {
		mem->is_pool = true;
		rc = 0;
	} else {
		rc = allocate_device_mem(hdev, mem, page_size, contiguous);
	}

	if (rc)
		goto error_allocate_memory;

	pthread_mutex_lock(&hdev->mem_table_device_lock);

	k = kh_put(ptr64, hdev->mem_table_device, mem->device_virt_addr, &rc);
	kh_val(hdev->mem_table_device, k) = mem;

	pthread_mutex_unlock(&hdev->mem_table_device_lock);

	return mem;

error_allocate_memory:
	hlthunk_free(mem);
	return NULL;
}



/*
 * This function frees device memory allocation array
 * @param fd file descriptor of the device to which the function will map the memory
 * @param device_addr_arr device memory allocation array
 * @param size the size of the array
 * @return 0 for success, negative value for failure
 */
int gaudi_free_device_mem_mix_page_size(int fd, uint64_t *device_addr_arr, int size)
{
	int i, rc = 0;

	for (i = 0 ; i < size && !rc ; i++)
		rc = gaudi_free_device_mem(fd, (void *)device_addr_arr[i]);

	return rc;
}

/*
 * This function frees host memory allocation which were done using
 * hltests_allocate_host_mem
 * @param fd file descriptor of the device that the host memory is mapped to
 * @param vaddr host pointer that points to the memory area
 * @return 0 for success, negative value for failure
 */
int gaudi_free_host_mem(int fd, void *vaddr)
{
	struct hltests_device *hdev;
	struct hltests_memory *mem;
	khint_t k;
	int rc;

	hdev = get_hdev_from_fd(fd);
	if (!hdev)
		return -ENODEV;

	pthread_mutex_lock(&hdev->mem_table_host_lock);

	k = kh_get(ptr64, hdev->mem_table_host, (uintptr_t) vaddr);
	if (k == kh_end(hdev->mem_table_host)) {
		pthread_mutex_unlock(&hdev->mem_table_host_lock);
		return -EINVAL;
	}

	mem = kh_val(hdev->mem_table_host, k);
	kh_del(ptr64, hdev->mem_table_host, k);

	pthread_mutex_unlock(&hdev->mem_table_host_lock);

	rc = hlthunk_memory_unmap(fd, mem->device_virt_addr);

	if (rc) {
		printf("Failed to unmap host memory\n");
		return rc;
	}

	if (mem->is_huge)
		munmap(mem->host_ptr, mem->size);
	else
		free(mem->host_ptr);

	hlthunk_free(mem);

	return 0;
}

static void sim_free_device_mem_on_host(int fd, struct hltests_memory *mem)
{
	if (hlthunk_memory_unmap(fd, mem->device_virt_addr))
		E("Sim failed to unmap host memory from device!");

	hlthunk_free(mem->host_ptr);
}

static void free_device_mem(struct hltests_device *hdev, struct hltests_memory *mem)
{
	if (hlthunk_memory_unmap(hdev->fd, mem->device_virt_addr))
		E("Failed to unmap device memory!");

	if (hlthunk_device_memory_free(hdev->fd, mem->device_handle))
		E("Failed to free device memory!");
}

/**
 * This function frees device memory allocation which were done using
 * gaudi_allocate_device_mem
 * @param fd file descriptor of the device that this memory belongs to
 * @param vaddr device VA that points to the memory area
 * @return 0 for success, negative value for failure
 */
int gaudi_free_device_mem(int fd, void *vaddr)
{
	const struct hltests_asic_funcs *asic;
	struct hltests_device *hdev;
	struct hltests_memory *mem;
	khint_t k;

	hdev = get_hdev_from_fd(fd);
	if (!hdev)
		return -ENODEV;

	asic = hdev->asic_funcs;

	pthread_mutex_lock(&hdev->mem_table_device_lock);

	k = kh_get(ptr64, hdev->mem_table_device, (uintptr_t)vaddr);
	if (k == kh_end(hdev->mem_table_device)) {
		pthread_mutex_unlock(&hdev->mem_table_device_lock);
		return -EINVAL;
	}

	mem = kh_val(hdev->mem_table_device, k);
	kh_del(ptr64, hdev->mem_table_device, k);

	pthread_mutex_unlock(&hdev->mem_table_device_lock);

	if (mem->is_pool) {
		asic->dram_pool_free(hdev, mem->device_virt_addr, mem->size);
	} else if (hdev->sim_dram_on_host) {
		sim_free_device_mem_on_host(fd, mem);
	} else {
		free_device_mem(hdev, mem);
	}

	hlthunk_free(mem);

	return 0;
}

/**
 * This function retrieves the device VA for a host memory area that was mapped
 * to the device
 * @param fd file descriptor of the device that the host memory is mapped to
 * @param vaddr host pointer that points to the memory area
 * @return virtual address in the device VA space representing this host memory
 * area. 0 for failure
 */


/**
 * This function retrieves the device memory block by virtual address in the
 * device address space
 * @param fd file descriptor of the device that the host memory is mapped to
 * @param device_va virtual address in the device VA space
 * @return struct containing memory information (handle, pva, dva, pointer). NULL for failure
 */
const struct hltests_memory *hltests_get_mem_for_device_va(int fd, void *device_va)
{
	const struct hltests_memory *mem;
	struct hltests_device *hdev;
	khint_t k;

	hdev = get_hdev_from_fd(fd);
	if (!hdev)
		return 0;

	pthread_mutex_lock(&hdev->mem_table_device_lock);

	k = kh_get(ptr64, hdev->mem_table_device, (uintptr_t) device_va);
	if (k == kh_end(hdev->mem_table_device)) {
		pthread_mutex_unlock(&hdev->mem_table_device_lock);
		errno = EINVAL;
		return NULL;
	}

	mem = kh_val(hdev->mem_table_device, k);

	pthread_mutex_unlock(&hdev->mem_table_device_lock);

	return mem;
}

/**
 * This function retrieves the device memory handle by virtual address in the
 * device address space
 * @param fd file descriptor of the device that the host memory is mapped to
 * @param device_va virtual address in the device VA space
 * @return opaque handle representing the device memory allocation. 0 for
 * failure
 */
uint64_t hltests_get_device_handle_for_device_va(int fd, void *device_va)
{
	const struct hltests_memory *mem = hltests_get_mem_for_device_va(fd, device_va);

	if (!mem)
		return 0;

	return mem->device_handle;
}


uint64_t gaudi_get_device_va_for_host_ptr(int fd, void *vaddr)
{
	struct hltests_device *hdev;
	struct hltests_memory *mem;
	khint_t k;

	hdev = get_hdev_from_fd(fd);
	if (!hdev)
		return 0;

	pthread_mutex_lock(&hdev->mem_table_host_lock);

	k = kh_get(ptr64, hdev->mem_table_host, (uintptr_t) vaddr);
	if (k == kh_end(hdev->mem_table_host)) {
		pthread_mutex_unlock(&hdev->mem_table_host_lock);
		return 0;
	}

	mem = kh_val(hdev->mem_table_host, k);

	pthread_mutex_unlock(&hdev->mem_table_host_lock);

	return mem->device_virt_addr;
}

/**
 * This function creates a command buffer for a specific device. It also
 * supports creating internal command buffer, which is basically a block of
 * memory on the host which is DMA'd into the device memory
 * @param fd file descriptor of the device
 * @param cb_size the size of the command buffer
 * @param cb_type the type of the command buffer
 * @cb_internal_sram_address the address in the sram that the internal CB will
 *                           be executed from by the CS. If this parameter is
 *                           0, the CB will be located on the host
 * @return virtual address of the CB in the user process VA space, or NULL for
 *         failure
 */
void *hltests_create_cb(int fd, uint32_t cb_size, enum hltests_cb_type cb_type,
			uint64_t cb_internal_sram_address)
{
	struct hltests_device *hdev;
	struct hltests_cb *cb;
	uint64_t align;
	uint32_t suffix_size = 0;
	int rc;
	khint_t k;

	hdev = get_hdev_from_fd(fd);
	if (!hdev)
		return NULL;

	cb = hlthunk_malloc(sizeof(*cb));
	if (!cb)
		return NULL;

	if (hltests_is_legacy_mode_enabled(fd)) {
		cb->cb_size = cb_size;
	} else {
		suffix_size = hdev->asic_funcs->get_arc_cb_suffix_size();
		cb->cb_size = ALIGN_UP((cb_size + suffix_size), 64);
	}

	cb->cb_type = cb_type;

	align = hltests_is_legacy_mode_enabled(fd) ? 0 : 8;

	/* For external queues, request the kernel driver to allocate a CB only
	 * if the ASIC is Goya/Gaudi.
	 */
	if (cb->cb_type == CB_TYPE_KERNEL && !(hltests_is_goya(fd) || hltests_is_gaudi(fd)))
		cb->cb_type = CB_TYPE_USER;

	switch (cb->cb_type) {
	case CB_TYPE_USER:
		cb->ptr = gaudi_allocate_host_mem_aligned(fd, cb->cb_size, NOT_HUGE_MAP, align);
		if (!cb->ptr)
			goto free_cb;

		if (cb_internal_sram_address)
			cb->cb_handle = cb_internal_sram_address;
		else
			cb->cb_handle =
				gaudi_get_device_va_for_host_ptr(fd, cb->ptr);

		break;

	case CB_TYPE_KERNEL:
	case CB_TYPE_KERNEL_MAPPED:
		if (cb->cb_type == CB_TYPE_KERNEL)
			rc = hlthunk_request_command_buffer(fd, cb->cb_size,
								&cb->cb_handle);
		else
			rc = hlthunk_request_mapped_command_buffer(fd,
						cb->cb_size, &cb->cb_handle);
		if (rc)
			goto free_cb;

		cb->ptr = hltests_mmap(fd, cb->cb_size, cb->cb_handle);
		if (cb->ptr == MAP_FAILED)
			goto destroy_cb;

		break;

	default:
		printf("Invalid CB type %d\n", cb->cb_type);
		goto free_cb;
	}

	pthread_mutex_lock(&hdev->cb_table_lock);

	k = kh_put(ptr64, hdev->cb_table, (uint64_t) (uintptr_t) cb->ptr, &rc);
	kh_val(hdev->cb_table, k) = cb;

	pthread_mutex_unlock(&hdev->cb_table_lock);

	return cb->ptr;

destroy_cb:
	hlthunk_destroy_command_buffer(fd, cb->cb_handle);
free_cb:
	hlthunk_free(cb);
	return NULL;
}

int hltests_destroy_cb(int fd, void *ptr)
{
	struct hltests_device *hdev;
	struct hltests_cb *cb;
	khint_t k;

	hdev = get_hdev_from_fd(fd);
	if (!hdev)
		return -ENODEV;

	pthread_mutex_lock(&hdev->cb_table_lock);

	k = kh_get(ptr64, hdev->cb_table, (uint64_t) (uintptr_t) ptr);
	if (k == kh_end(hdev->cb_table)) {
		pthread_mutex_unlock(&hdev->cb_table_lock);
		return -EINVAL;
	}

	cb = kh_val(hdev->cb_table, k);
	kh_del(ptr64, hdev->cb_table, k);

	pthread_mutex_unlock(&hdev->cb_table_lock);

	if (cb->cb_type == CB_TYPE_KERNEL ||
			cb->cb_type == CB_TYPE_KERNEL_MAPPED) {
		hltests_munmap(fd, cb->ptr, cb->cb_size);
		hlthunk_destroy_command_buffer(fd, cb->cb_handle);
	} else {
		gaudi_free_host_mem(fd, cb->ptr);
	}

	hlthunk_free(cb);

	return 0;
}


/* cs with one cb is very common so write special helper for this */
int hltests_submit_cb(int fd,
		void *cb_ptr,
		uint32_t cb_size,
		uint32_t queue_index,
		uint32_t flags,
		uint64_t *seq)
{
	struct hltests_cs_chunk execute_arr;
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	execute_arr.cb_ptr = cb_ptr;
	execute_arr.cb_size = cb_size;
	execute_arr.queue_index = queue_index;
	return asic->submit_cs(fd, NULL, 0, &execute_arr, 1, flags, 0, seq);
}

int hltests_submit_cs(int fd,
		struct hltests_cs_chunk *restore_arr,
		uint32_t restore_arr_size,
		struct hltests_cs_chunk *execute_arr,
		uint32_t execute_arr_size,
		uint32_t flags,
		uint64_t *seq)
{
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	return asic->submit_cs(fd, restore_arr, restore_arr_size, execute_arr,
					execute_arr_size, flags, 0, seq);
}

int hltests_submit_cs_timeout(int fd,
		struct hltests_cs_chunk *restore_arr,
		uint32_t restore_arr_size,
		struct hltests_cs_chunk *execute_arr,
		uint32_t execute_arr_size,
		uint32_t flags,
		uint32_t timeout_sec,
		uint64_t *seq)
{
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	return asic->submit_cs(fd, restore_arr, restore_arr_size, execute_arr,
					execute_arr_size, flags, timeout_sec, seq);
}

static int fill_cs_chunks(struct hltests_device *hdev,
			struct hl_cs_chunk *submit_arr,
			struct hltests_cs_chunk *chunks_arr,
			uint32_t num_chunks)
{
	int i, rc;

	for (i = 0 ; i < num_chunks ; i++) {
		rc = hltests_fill_cs_chunk(hdev, &submit_arr[i],
				chunks_arr[i].cb_ptr,
				chunks_arr[i].cb_size,
				chunks_arr[i].queue_index);
		if (rc)
			return rc;
	}

	return 0;
}

int hltests_submit_staged_cs(int fd,
		struct hltests_cs_chunk *restore_arr,
		uint32_t restore_arr_size,
		struct hltests_cs_chunk *execute_arr,
		uint32_t execute_arr_size,
		uint32_t flags,
		uint64_t staged_cs_seq,
		uint64_t *seq)
{
	struct hltests_device *hdev;
	struct hl_cs_chunk *chunks_restore = NULL, *chunks_execute = NULL;
	struct hlthunk_cs_in cs_in;
	struct hlthunk_cs_out cs_out;
	uint32_t size;
	int rc = 0;

	if (!(flags & HL_CS_FLAGS_STAGED_SUBMISSION)) {
		printf("Staged submission flags are not set");
		return -EINVAL;
	}

	hdev = get_hdev_from_fd(fd);
	if (!hdev)
		return -ENODEV;

	if (!restore_arr_size && !execute_arr_size)
		return 0;

	if (restore_arr_size && restore_arr) {
		size = restore_arr_size * sizeof(*chunks_restore);
		chunks_restore = hlthunk_malloc(size);
		if (!chunks_restore) {
			rc = -ENOMEM;
			goto out;
		}

		rc = fill_cs_chunks(hdev, chunks_restore, restore_arr,
				restore_arr_size);
		if (rc)
			goto free_chunks_restore;
	}

	if (execute_arr_size && execute_arr) {
		size = execute_arr_size * sizeof(*chunks_execute);
		chunks_execute = hlthunk_malloc(size);
		if (!chunks_execute) {
			rc = -ENOMEM;
			goto free_chunks_restore;
		}

		rc = fill_cs_chunks(hdev, chunks_execute, execute_arr,
				execute_arr_size);
		if (rc)
			goto free_chunks_execute;
	}

	memset(&cs_in, 0, sizeof(cs_in));
	cs_in.chunks_restore = chunks_restore;
	cs_in.chunks_execute = chunks_execute;
	cs_in.num_chunks_restore = restore_arr_size;
	cs_in.num_chunks_execute = execute_arr_size;
	cs_in.flags = flags;

	memset(&cs_out, 0, sizeof(cs_out));

	if (flags & HL_CS_FLAGS_ENCAP_SIGNALS)
		rc = hlthunk_staged_command_submission_encaps_signals(fd,
						staged_cs_seq,
						&cs_in, &cs_out);
	else
		rc = hlthunk_staged_command_submission(fd, staged_cs_seq,
						&cs_in, &cs_out);
	if (rc)
		goto free_chunks_execute;

	if (cs_out.status != HL_CS_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto free_chunks_execute;
	}

	*seq = cs_out.seq;

free_chunks_execute:
	hlthunk_free(chunks_execute);
free_chunks_restore:
	hlthunk_free(chunks_restore);
out:
	return rc;
}

int hltests_wait_for_legacy_cs(int fd, uint64_t seq, uint64_t timeout_us)
{
	uint32_t status;
	int rc;

	rc = hlthunk_wait_for_cs(fd, seq, timeout_us, &status);
	if (rc && errno != ETIMEDOUT && errno != EIO)
		return rc;

	return status;
}

int hltests_wait_for_legacy_cs_until_not_busy(int fd, uint64_t seq)
{
	int status;

	do {
		status = hltests_wait_for_legacy_cs(fd, seq,
					WAIT_FOR_CS_DEFAULT_TIMEOUT);
	} while (status == HL_WAIT_CS_STATUS_BUSY);

	return status;
}

int hltests_wait_for_cs(int fd, uint64_t seq, uint64_t timeout_us)
{
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	return asic->wait_for_cs(fd, seq, timeout_us);
}

int hltests_wait_for_cs_until_not_busy(int fd, uint64_t seq)
{
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	return asic->wait_for_cs_until_not_busy(fd, seq);
}

int hltests_wait_for_interrupt(int fd, void *addr, uint32_t target_value,
				uint32_t interrupt_id, uint64_t timeout_us)
{
	uint32_t status;
	int rc;

	rc = hlthunk_wait_for_interrupt(fd, addr, target_value, interrupt_id,
					timeout_us, &status);
	if (rc && errno != ETIMEDOUT && errno != EIO)
		return rc;

	return status;
}

int hltests_wait_for_interrupt_until_not_busy(int fd, void *addr,
				uint32_t target_value, uint32_t interrupt_id)
{
	int status;

	do {
		status = hltests_wait_for_interrupt(fd, addr, target_value,
				interrupt_id, WAIT_FOR_CS_DEFAULT_TIMEOUT);
	} while (status == HL_WAIT_CS_STATUS_BUSY);

	return status;
}

int hltests_wait_for_interrupt_by_handle(int fd, uint64_t cq_counters_handle,
				uint64_t cq_counters_offset, uint32_t target_value,
				uint32_t interrupt_id, uint64_t timeout_us)
{
	uint32_t status;
	int rc;

	rc = hlthunk_wait_for_interrupt_by_handle(fd, cq_counters_handle, cq_counters_offset,
					target_value, interrupt_id,
					timeout_us, &status);
	if (rc && errno != ETIMEDOUT && errno != EIO)
		return rc;

	return status;
}

int hltests_wait_for_interrupt_by_handle_until_not_busy(int fd, uint64_t cq_counters_handle,
				uint64_t cq_counters_offset,
				uint32_t target_value, uint32_t interrupt_id)
{
	int status;

	do {
		status = hltests_wait_for_interrupt_by_handle(fd, cq_counters_handle,
				cq_counters_offset,
				target_value,
				interrupt_id, WAIT_FOR_CS_DEFAULT_TIMEOUT);
	} while (status == HL_WAIT_CS_STATUS_BUSY);

	return status;
}

/**
 * This function submits a single command buffer for a specific queue, and
 * waits for it.
 * @param fd file descriptor of the device
 * @param cb_ptr a pointer to the command buffer
 * @param cb_size the size of the command buffer
 * @param queue_index the allocated queue for the command submission
 * @param destroy_cb true if CB should be destroyed, false otherwise
 * @param expected_val expected status of current CS (e.g., COMPLETED, BUSY, etc.)
 * @return -1 on failure, 0 on success
 */
int hltests_submit_and_wait_cs(int fd, void *cb_ptr, uint32_t cb_size,
				uint32_t queue_index,
				enum hltests_destroy_cb destroy_cb,
				int expected_val)
{
	struct hltests_cs_chunk execute_arr[1];
	uint64_t seq = 0;
	int rc;

	execute_arr[0].cb_ptr = cb_ptr;
	execute_arr[0].cb_size = cb_size;
	execute_arr[0].queue_index = queue_index;

	rc = hltests_submit_cs(fd, NULL, 0, execute_arr, 1, 0, &seq);
	assert_int_equal(rc, 0);

	rc = hltests_wait_for_cs_until_not_busy(fd, seq);
	if (rc)
		assert_int_equal(rc, expected_val);

	if (destroy_cb) {
		rc = hltests_destroy_cb(fd, cb_ptr);
		assert_int_equal(rc, 0);
	}

	return 0;
}




/**
 * This function submits a single command buffer for a specific queue, and
 * waits for it.
 * @param fd file descriptor of the device
 * @param cb_ptr a pointer to the command buffer
 * @param cb_size the size of the command buffer
 * @param queue_index the allocated queue for the command submission
 * @param destroy_cb true if CB should be destroyed, false otherwise
 * @return -1 on failure, 0 on success
 */
int hltests_submit_and_wait_legacy_cs(int fd, void *cb_ptr, uint32_t cb_size,
				uint32_t queue_index,
				enum hltests_destroy_cb destroy_cb,
				int expected_val)
{
	struct hltests_cs_chunk execute_arr[1];
	uint64_t seq = 0;
	int rc;

	execute_arr[0].cb_ptr = cb_ptr;
	execute_arr[0].cb_size = cb_size;
	execute_arr[0].queue_index = queue_index;

	rc = hltests_submit_legacy_cs(fd, NULL, 0, execute_arr, 1, 0, 0, &seq);
	assert_int_equal(rc, 0);

	rc = hltests_wait_for_legacy_cs_until_not_busy(fd, seq);
	assert_int_equal(rc, expected_val);

	if (destroy_cb) {
		rc = hltests_destroy_cb(fd, cb_ptr);
		assert_int_equal(rc, 0);
	}

	return 0;
}





uint32_t gaudi_get_dma_down_qid(int fd, enum hltests_stream_id stream)
{
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_dma_down_qid(fd, DCORE_MODE_FULL_CHIP, stream);
}

uint32_t gaudi_get_dma_up_qid(int fd, enum hltests_stream_id stream)
{
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_dma_up_qid(fd, DCORE_MODE_FULL_CHIP, stream);
}






int gaudi_dma_transfer(int fd, uint32_t queue_index, enum hltests_eb eb,
				enum hltests_mb mb,
				uint64_t src_addr, uint64_t dst_addr,
				uint32_t size,
				enum hltests_dma_direction dma_dir)
{
	struct hltests_cs_chunk execute_arr;
	struct hltests_pkt_info pkt_info;
	uint32_t offset = 0;
	uint64_t seq = 0;
	void *ptr;
	int rc;

	ptr = hltests_create_cb(fd, 0x1000, EXTERNAL, 0);
	assert_non_null(ptr);

	memset(&pkt_info, 0, sizeof(pkt_info));
	pkt_info.qid = queue_index;
	pkt_info.eb = eb;
	pkt_info.mb = mb;
	pkt_info.dma.src_addr = src_addr;
	pkt_info.dma.dst_addr = dst_addr;
	pkt_info.dma.size = size;
	pkt_info.dma.dma_dir = dma_dir;
	offset = hltests_add_dma_pkt(fd, ptr, offset, &pkt_info);

	execute_arr.cb_ptr = ptr;
	execute_arr.cb_size = offset;
	execute_arr.queue_index = queue_index;

	rc = hltests_submit_cs(fd, NULL, 0, &execute_arr, 1, 0, &seq);
	if (rc)
		return rc;

	rc = hltests_wait_for_cs_until_not_busy(fd, seq);
	if (rc != HL_WAIT_CS_STATUS_COMPLETED)
		return rc;

	rc = hltests_destroy_cb(fd, ptr);
	if (rc)
		return rc;

	return 0;
}


int gaudi_dma_transfer_legacy(int fd, uint32_t queue_index,
				enum hltests_eb eb, enum hltests_mb mb,
				uint64_t src_addr, uint64_t dst_addr,
				uint32_t size,
				enum hltests_dma_direction dma_dir)
{
	uint32_t offset = 0;
	void *ptr;
	struct hltests_pkt_info pkt_info;

	ptr = hltests_create_cb(fd, 0x1000, EXTERNAL, 0);
	assert_non_null(ptr);

	memset(&pkt_info, 0, sizeof(pkt_info));
	pkt_info.eb = eb;
	pkt_info.mb = mb;
	pkt_info.dma.src_addr = src_addr;
	pkt_info.dma.dst_addr = dst_addr;
	pkt_info.dma.size = size;
	pkt_info.dma.dma_dir = dma_dir;
	offset = hltests_add_dma_pkt(fd, ptr, offset, &pkt_info);

	return hltests_submit_and_wait_legacy_cs(fd, ptr, offset, queue_index,
				DESTROY_CB_TRUE, HL_WAIT_CS_STATUS_COMPLETED);
}





/**
 * This test allocates device memory until all the memory was allocated.
 * @param state contains the open file descriptor.
 * @param page_size page size to use (0 means use default page size).
 * @param contiguous indicates if the allocated device memory should be
 *        contiguous or not.
 * @param mix_alloc if true use mixed allocations, otherwise use single page size.
 *
 * Note:
 * - When test is running with mixed allocation, each iteration different page size
 *   is chosen (round robin). in this case chunk size equals the page size.
 *   In this case it is expected that we will not be able to allocate the whole memory
 *   (because of memory fragmentation)
 * - Otherwise (not mixed allocation) the test is affected by the page size:
 *     # When the device dram_page_size is a power of 2 the allocated chunks are 0.5GB
 *       (because the driver reserves the first 0.5GB and we have multiples of 1GB of
 *       memory).
 *     # When the device dram_page_size is not  a power of 2, the allocated memory
 *       chunks will have the same size as the dram_page_size. This may leave some
 *       memory residues which will not be used by the test.
 *    In the above cases we expect to allocate the whole memory.
 */
VOID gaudi_allocate_device_mem_until_full(void **state, uint32_t page_size,
					enum hltests_contiguous contigouos, bool mix_alloc)
{
	uint64_t total_size, num_of_chunks, i, j, page_order_bitmask, *page_size_arr, total_alloc;
	struct hltests_state *tests_state = (struct hltests_state *) *state;
	uint32_t chunk_size, used_page_size;
	struct hlthunk_hw_ip_info *hw_ip = &tests_state->hw_ip;
	int rc, fd = tests_state->fd;
	uint8_t page_size_arr_size;
	void **device_addr;
	bool error = false;

	if (!hw_ip->dram_enabled) {
		printf("DRAM is disabled so skipping test\n");
		skip();
	}

	/* initialize to avoid compiler warnings */
	page_size_arr = NULL;
	page_size_arr_size = 0;

	/*
	 * store explicitly what page size we are using for calculations
	 * (as page_size=0 means default)
	 */
	used_page_size = page_size ? page_size : hw_ip->device_mem_alloc_default_page_size;

	if (mix_alloc) {
		/* check whether mixed allocation is supported */
		rc = hlthunk_get_dev_memalloc_page_orders(fd, &page_order_bitmask);
		assert_int_equal(rc, 0);

		if (!page_order_bitmask) {
			if (hltests_is_gaudi3(fd)) {
				fail();
			} else {
				printf("Multiple page sizes is not supported\n");
				skip();
			}
		}
	}

	total_size = hltests_get_total_avail_device_mem(fd);
	if (mix_alloc) {
		CALL_HELPER_FUNC(hltests_build_page_size_array(&page_size_arr, &page_size_arr_size,
					page_order_bitmask));
		/* we set check size to the minimal page size so we'll "give a chance" to
		 * maximum number of allocations
		 */
		chunk_size = page_size_arr[0];
	} else if ((hw_ip->dram_page_size == 0) || IS_POWER_OF_TWO(used_page_size)) {
		chunk_size = hltests_is_simulator(fd) ? SZ_32M : SZ_512M;
		/* handle devices with dram_page_size > 32M */
		chunk_size = (chunk_size > used_page_size) ? chunk_size : used_page_size;
	} else {
		chunk_size = used_page_size;
	}

	num_of_chunks = total_size / chunk_size;
	assert_int_not_equal(num_of_chunks, 0);

	device_addr = hlthunk_malloc(num_of_chunks * sizeof(void *));
	assert_non_null(device_addr);

	total_alloc = 0;
	for (i = 0 ; i < num_of_chunks ; i++) {
		/* fix mixed allocation page size is modified each time */
		if (mix_alloc) {
			chunk_size = page_size_arr[i % page_size_arr_size];
			page_size = chunk_size;
		}
		device_addr[i] = gaudi_allocate_device_mem(fd, chunk_size, page_size, contigouos);
		if (!device_addr[i])
			break;
		total_alloc += chunk_size;
	}

	/* failure criteria */
	if (mix_alloc) {
		/*
		 * we expect to be able to allocate to at least number of highest order
		 * pages that can fill the memory
		 */
		uint64_t min_chunks = total_size / page_size_arr[page_size_arr_size - 1];

		if (i < min_chunks)
			error = true;
	} else if (i < num_of_chunks) {
		error = true;
	}

	if (error || mix_alloc)
		printf("Was able to allocate %luMB out of %luMB\n",
				total_alloc / SZ_1M, total_size / SZ_1M);


	for (j = 0 ; j < i ; j++) {
		rc = gaudi_free_device_mem(fd, device_addr[j]);
		assert_int_equal(rc, 0);
	}

	hlthunk_free(device_addr);
	if (mix_alloc)
		hltests_destroy_page_size_array(page_size_arr);

	if (error)
		fail();

	END_TEST;
}

//* end of hlthunk_tests */




int gaudi_init(void)
{
	int rc;

	rc = pthread_spin_init(&rand_lock, PTHREAD_PROCESS_PRIVATE);
	if (rc) {
		printf("Failed to initialize number randomizer lock [rc %d]\n",
			rc);
		return rc;
	}

	seed(time(NULL));

	dev_table = kh_init(ptr);
	if (!dev_table) {
		rc = -ENOMEM;
		printf("Failed to initialize device table [rc %d]\n", rc);
		goto free_spinlock;
	}

	return 0;

free_spinlock:
	pthread_spin_destroy(&rand_lock);

	return rc;
}

void gaudi_fini(void)
{
	if (!dev_table)
		return;

	kh_destroy(ptr, dev_table);
	pthread_spin_destroy(&rand_lock);
}





#define TEST_SIZE_KB  64  // 64KB test size
#define TEST_SIZE     (TEST_SIZE_KB * 1024)

struct test_result {
    int success;
    double bandwidth_gbps;
    char error_msg[256];
};

void print_test_result(const char* test_name, struct test_result* result) {
    printf("%s: ", test_name);
    if (result->success) {
        printf("PASSED - %.2f GB/s\n", result->bandwidth_gbps);
    } else {
        printf("FAILED - %s\n", result->error_msg);
    }
}

struct test_result test_host_to_device(int fd, void *host_ptr) {
    struct test_result result = {0};
    uint64_t device_va = 0;
    void *device_addr = NULL;
    struct timespec start, end;
    double time_sec, bandwidth;
    int rc;


    printf("  Getting device VA for host pointer...\n");
    device_va = gaudi_get_device_va_for_host_ptr(fd, host_ptr);
    if (device_va == 0) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "Failed to get device VA for host pointer");
        goto cleanup;
    }

    printf("  Allocating device memory...\n");
    device_addr = gaudi_allocate_device_mem(fd, TEST_SIZE, 0, false);
    if (!device_addr) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "Failed to allocate device memory");
        goto cleanup;
    }

    printf("  Performing H2D DMA transfer...\n");

    uint32_t qid = gaudi_get_dma_down_qid(fd, STREAM0);

    clock_gettime(CLOCK_MONOTONIC, &start);

    rc = gaudi_dma_transfer(fd,
                             qid,
                             EB_FALSE,
                             MB_TRUE,
                             device_va,
                             (uint64_t)device_addr,
                             TEST_SIZE,
                             DMA_DIR_HOST_TO_DRAM);

    clock_gettime(CLOCK_MONOTONIC, &end);

    if (rc != 0) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "DMA transfer failed with rc=%d", rc);
        goto cleanup_device;
    }

    time_sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    bandwidth = (TEST_SIZE / (1024.0 * 1024.0 * 1024.0)) / time_sec;

    result.success = 1;
    result.bandwidth_gbps = bandwidth;

cleanup_device:
    gaudi_free_device_mem(fd, device_addr);
cleanup:
    gaudi_free_host_mem(fd, host_ptr);
    return result;
}

struct test_result test_device_to_host(int fd, void *host_ptr) {
    struct test_result result = {0};
    uint64_t device_va = 0;
    void *device_addr = NULL;
    struct timespec start, end;
    double time_sec, bandwidth;
    int rc;



    printf("  Getting device VA for host pointer...\n");
    device_va = gaudi_get_device_va_for_host_ptr(fd, host_ptr);
    if (device_va == 0) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "Failed to get device VA for host pointer");
        goto cleanup;
    }

    printf("  Allocating and initializing device memory...\n");
    device_addr = gaudi_allocate_device_mem(fd, TEST_SIZE, 0, false);
    if (!device_addr) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "Failed to allocate device memory");
        goto cleanup;
    }

    printf("  Performing D2H DMA transfer...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    rc = gaudi_dma_transfer(fd,
                             gaudi_get_dma_up_qid(fd, STREAM0),
                             EB_FALSE,
                             MB_TRUE,
                             (uint64_t)device_addr,
                             device_va,
                             TEST_SIZE,
                             DMA_DIR_DRAM_TO_HOST);

    clock_gettime(CLOCK_MONOTONIC, &end);

    if (rc != 0) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "DMA transfer failed with rc=%d", rc);
        goto cleanup_device;
    }

    time_sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    bandwidth = (TEST_SIZE / (1024.0 * 1024.0 * 1024.0)) / time_sec;

    result.success = 1;
    result.bandwidth_gbps = bandwidth;

cleanup_device:
    gaudi_free_device_mem(fd, device_addr);
cleanup:
    gaudi_free_host_mem(fd, host_ptr);
    return result;
}

struct test_result test_device_to_device(int fd) {
    struct test_result result = {0};
    void *device_addr1 = NULL, *device_addr2 = NULL;
    struct timespec start, end;
    double time_sec, bandwidth;
    int rc;

    printf("  Allocating source device memory...\n");
    device_addr1 = gaudi_allocate_device_mem(fd, TEST_SIZE, 0, false);
    if (!device_addr1) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "Failed to allocate source device memory");
        return result;
    }

    printf("  Allocating destination device memory...\n");
    device_addr2 = gaudi_allocate_device_mem(fd, TEST_SIZE, 0, false);
    if (!device_addr2) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "Failed to allocate destination device memory");
        goto cleanup_src;
    }

    printf("  Performing D2D DMA transfer...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    rc = gaudi_dma_transfer(fd,
                             gaudi_get_dma_down_qid(fd, STREAM0),
                             EB_FALSE,
                             MB_TRUE,
                             (uint64_t)device_addr1,
                             (uint64_t)device_addr2,
                             TEST_SIZE,
                             DMA_DIR_DRAM_TO_DRAM);

    clock_gettime(CLOCK_MONOTONIC, &end);

    if (rc != 0) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "DMA transfer failed with rc=%d", rc);
        goto cleanup_both;
    }

    time_sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    bandwidth = (TEST_SIZE / (1024.0 * 1024.0 * 1024.0)) / time_sec;

    result.success = 1;
    result.bandwidth_gbps = bandwidth;

cleanup_both:
    gaudi_free_device_mem(fd, device_addr2);
cleanup_src:
    gaudi_free_device_mem(fd, device_addr1);
    return result;
}



int main(void) {
    struct hltests_state *tests_state;
    void *state;
    int fd, rc;
    struct test_result h2d_result, d2h_result, d2d_result;

    printf("Self-Contained HL-thunk DMA Test\n");
    printf("=================================\n\n");

    printf("Initializing test framework...\n");
    rc = gaudi_init();
    if (rc) {
        printf("Failed to initialize hlthunk tests library: %d\n", rc);
        return -1;
    }

    printf("Setting up test state...\n");
    rc = gaudi_setup(&state);
    if (rc) {
        printf("Failed to run setup phase of hlthunk tests: %d\n", rc);
        gaudi_fini();
        return -1;
    }

    tests_state = (struct hltests_state *) state;
    fd = tests_state->fd;

    printf("Test setup successful. Device opened with fd: %d\n\n", fd);

    // Test H2D (Host to Device)
    printf("Testing Host-to-Device (H2D) DMA (%d KB)...\n", TEST_SIZE_KB);

    
    printf("  Allocating host memory...\n");
    void *host_ptr = gaudi_allocate_host_mem(fd, TEST_SIZE, NOT_HUGE_MAP);
    if (!host_ptr) {
        printf("Failed to allocate host memory");
        return -1;
    }

    memset(host_ptr, 0xAA, TEST_SIZE);

    h2d_result = test_host_to_device(fd, host_ptr);
    print_test_result("H2D DMA", &h2d_result);
    printf("\n");

    // Test D2H (Device to Host)
    printf("Testing Device-to-Host (D2H) DMA (%d KB)...\n", TEST_SIZE_KB);

    printf("  Allocating host memory...\n");
    void *host_ptr1 = gaudi_allocate_host_mem(fd, TEST_SIZE, NOT_HUGE_MAP);
    if (!host_ptr1) {
        printf("Failed to allocate host memory");
        return -1;
    }

    memset(host_ptr1, 0x00, TEST_SIZE);
    d2h_result = test_device_to_host(fd, host_ptr);
    print_test_result("D2H DMA", &d2h_result);
    int rec =  memcmp(host_ptr1, host_ptr, TEST_SIZE);
    printf("Memory comparison result: %s\n", rec == 0 ? "MATCH" : "MISMATCH");
    printf("\n");

    // Free the host memory used for D2H
    gaudi_free_host_mem(fd, host_ptr1);   
    gaudi_free_host_mem(fd, host_ptr);

    // Test D2D (Device to Device)
    printf("Testing Device-to-Device (D2D) DMA (%d KB)...\n", TEST_SIZE_KB);
    d2d_result = test_device_to_device(fd);
    print_test_result("D2D DMA", &d2d_result);
    printf("\n");

    // Summary
    printf("=== SUMMARY ===\n");
    print_test_result("H2D (Host-to-Device)", &h2d_result);
    print_test_result("D2H (Device-to-Host)", &d2h_result);
    print_test_result("D2D (Device-to-Device)", &d2d_result);

    int tests_passed = h2d_result.success + d2h_result.success + d2d_result.success;
    printf("\nTotal: %d/3 tests passed\n", tests_passed);

    // Cleanup
    hltests_teardown(&state);
    gaudi_fini();

    return (tests_passed == 3) ? 0 : -1;
}