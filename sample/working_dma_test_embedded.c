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

// Logging macros
#define __STRINGIFY(x_) #x_
#define STRINGIFY(x_) __STRINGIFY(x_)
#define LOG(level_, format_, ...) \
	printf("[" level_ "] %s:" STRINGIFY(__LINE__) ": " format_ "\n", __func__, ##__VA_ARGS__)
#define W(format_, ...) LOG("WRN", format_, ##__VA_ARGS__)

struct config_iterator;

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


//** hlthunk_tests **/

// SPDX-License-Identifier: MIT

/*
 * Copyright 2019 HabanaLabs, Ltd.
 * All Rights Reserved.
 */


/* SPDX-License-Identifier: MIT
 *
 * Copyright 2019 HabanaLabs, Ltd.
 * All Rights Reserved.
 *
 */

#ifndef HLTHUNK_H
#define HLTHUNK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "drm/habanalabs_accel.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define hlthunk_public  __attribute__((visibility("default")))

#define HLTHUNK_MAX_MINOR		256
#define HLTHUNK_DEV_NAME_PRIMARY	"/dev/accel/accel%d"
#define HLTHUNK_DEV_NAME_CONTROL	"/dev/accel/accel_controlD%d"

#define HLTHUNK_BUSY_ENGINES_MASK_SIZE	4

enum hlthunk_node_type {
	HLTHUNK_NODE_PRIMARY,
	HLTHUNK_NODE_CONTROL,
	HLTHUNK_NODE_MAX
};

enum hlthunk_device_name {
	HLTHUNK_DEVICE_GOYA = 0,
	HLTHUNK_DEVICE_GRECO = 1,
	HLTHUNK_DEVICE_GAUDI = 2,
	HLTHUNK_DEVICE_INVALID = 3,
	HLTHUNK_DEVICE_DONT_CARE = 4,
	HLTHUNK_DEVICE_GAUDI2 = 5,
	HLTHUNK_DEVICE_GAUDI_HL2000M = 6,
	HLTHUNK_DEVICE_GAUDI3 = 7,
	HLTHUNK_DEVICE_GAUDI2B = 8,
	HLTHUNK_DEVICE_GAUDI2C = 10,
	HLTHUNK_DEVICE_GAUDI2D = 11,
	HLTHUNK_DEVICE_GAUDI3D = 12,
	HLTHUNK_DEVICE_MAX
};

enum hlthunk_event_record_id {
	HLTHUNK_OPEN_DEV,
	HLTHUNK_CS_TIMEOUT,
	HLTHUNK_RAZWI_EVENT,
	HLTHUNK_UNDEFINED_OPCODE,
	HLTHUNK_HW_ERR_OPCODE,
	HLTHUNK_FW_ERR_OPCODE,
	HLTHUNK_ENGINE_EVENT,
};

/**
 * struct hlthunk_hw_ip_info - hardware information on various IPs in the ASIC
 * @sram_base_address: The first SRAM physical base address that is free to be
 *                     used by the user.
 * @dram_base_address: The first DRAM virtual or physical base address that is
 *                     free to be used by the user.
 * @dram_size: The DRAM size that is available to the user.
 * @sram_size: The SRAM size that is available to the user.
 * @num_of_events: The number of events that can be received from the f/w. This
 *                 is needed so the user can what is the size of the h/w events
 *                 array he needs to pass to the kernel when he wants to fetch
 *                 the event counters.
 * @device_id: PCI device ID of the ASIC.
 * @cpld_version: CPLD version on the board.
 * @psoc_pci_pll_nr: PCI PLL NR value. Needed by the profiler in some ASICs.
 * @psoc_pci_pll_nf: PCI PLL NF value. Needed by the profiler in some ASICs.
 * @psoc_pci_pll_od: PCI PLL OD value. Needed by the profiler in some ASICs.
 * @psoc_pci_pll_div_factor: PCI PLL DIV factor value. Needed by the profiler
 *                           in some ASICs.
 * @tpc_enabled_mask: Bit-mask that represents which TPCs are enabled. Relevant
 *                    for Goya/Gaudi only.
 * @dram_enabled: Whether the DRAM is enabled.
 * @cpucp_version: The CPUCP f/w version.
 * @module_id: Module ID of the ASIC for mezzanine cards in servers
 *             (From OCP spec).
 * @card_name: The card name as passed by the f/w.
 * @decoder_enabled_mask: Bit-mask that represents which decoders are enabled.
 * @mme_master_slave_mode: Indicate whether the MME is working in master/slave
 *                         configuration. Relevant for Gaudi2 and later.
 * @tpc_enabled_mask_ext: Bit-mask that represents which TPCs are enabled.
 *                        Relevant for Gaudi2 and later.
 * @dram_page_size: The DRAM physical page size.
 * @first_available_interrupt_id: The first available interrupt ID for the user
 *                                to be used when it works with user interrupts.
 *                                Relevant for Gaudi2 and later.
 * @edma_enabled_mask: Bit-mask that represents which EDMAs are enabled.
 *                     Relevant for Gaudi2 and later.
 * @server_type: Server type that the Gaudi ASIC is currently installed in.
 *               The value is according to enum hl_server_type
 * @pdma_user_owned_ch_mask: Bit-mask that represents which PDMA channels are
 *                     enabled and can be used by the user.
 *                     Relevant for Gaudi3 and later.
 * @number_of_user_interrupts: The number of interrupts that are available to the userspace
 *                             application to use. Relevant for Gaudi2 and later.
 * @device_mem_alloc_default_page_size: default page size used in device memory allocation.
 * @nic_ports_mask: Bit-mask that represents the nic ports that are enabled.
 * @interposer_version: Interposer version.
 * @substrate_version: Substrate version.
 * @nic_ports_external_mask: Bit mask that represents the nic ports that are external - used for
 *                           scale-out and are exposed to Linux as network devices.
 * @mme_enabled_mask: Bit-mask that represents which MMEs are enabled.
 * @odp_supported: true if ODP is supported, otherwise false.
 * @security_enabled: true if security is enabled on device.
 * @revision_id: PCI revision ID of the ASIC.
 * @tpc_interrupt_id: interrupt id for TPC to use in order to raise events toward host.
 * @engine_core_interrupt_reg_addr: interrupt register address for engine core to use
 *                                  in order to raise events toward FW.
 * @rotator_enabled_mask: Bit-mask that represents which rotators are enabled.
 * @sched_arc_enabled_mask: Bit-mask that represents which arc sched are enabled.
 * @reserved_dram_size: DRAM size reserved for driver and firmware.
 */
struct hlthunk_hw_ip_info {
	uint64_t sram_base_address;
	uint64_t dram_base_address;
	uint64_t dram_size;
	uint32_t sram_size;
	uint32_t num_of_events;
	uint32_t device_id;
	uint32_t cpld_version;
	uint32_t psoc_pci_pll_nr;
	uint32_t psoc_pci_pll_nf;
	uint32_t psoc_pci_pll_od;
	uint32_t psoc_pci_pll_div_factor;
	uint16_t tpc_enabled_mask;
	uint8_t dram_enabled;
	uint8_t cpucp_version[HL_INFO_VERSION_MAX_LEN];
	uint32_t module_id;
	uint8_t card_name[HL_INFO_CARD_NAME_MAX_LEN];
	uint32_t decoder_enabled_mask;
	uint8_t mme_master_slave_mode;
	uint64_t tpc_enabled_mask_ext;
	uint64_t dram_page_size;
	uint16_t first_available_interrupt_id;
	uint32_t edma_enabled_mask;
	uint16_t server_type;
	uint64_t pdma_user_owned_ch_mask;
	uint16_t number_of_user_interrupts;
	uint64_t device_mem_alloc_default_page_size;
	uint64_t nic_ports_mask;
	uint8_t interposer_version;
	uint8_t substrate_version;
	uint64_t nic_ports_external_mask;
	uint32_t mme_enabled_mask;
	uint8_t odp_supported;
	uint8_t security_enabled;
	uint8_t revision_id;
	uint16_t tpc_interrupt_id;
	uint64_t engine_core_interrupt_reg_addr;
	uint32_t rotator_enabled_mask;
	uint32_t sched_arc_enabled_mask;
	uint64_t reserved_dram_size;
};

struct hlthunk_dram_usage_info {
	uint64_t dram_free_mem;
	uint64_t ctx_dram_mem;
};

struct hlthunk_mac_addr {
	uint8_t addr[ETH_ALEN];
};

struct hlthunk_mac_addr_info {
	struct hlthunk_mac_addr array[HL_INFO_MAC_ADDR_MAX_NUM];
	uint64_t mask[2];
};

struct hlthunk_engines_idle_info {
	uint32_t is_idle;
	uint32_t pad;
	uint64_t mask[HLTHUNK_BUSY_ENGINES_MASK_SIZE];
};

struct hlthunk_pll_frequency_info {
	uint16_t output[HL_PLL_NUM_OUTPUTS];
};

struct hlthunk_reset_count_info {
	uint32_t hard_reset_count;
	uint32_t soft_reset_count;
};

struct hlthunk_time_sync_info {
	uint64_t device_time;
	uint64_t host_time;
	uint64_t tsc_time;
};

struct hlthunk_sync_manager_info {
	uint32_t first_available_sync_object;
	uint32_t first_available_monitor;
	uint32_t first_available_cq;
	uint32_t reserved;
};

struct hlthunk_pci_counters_info {
	uint64_t rx_throughput;
	uint64_t tx_throughput;
	uint32_t replay_cnt;
};

/* clk_throttling_reason masks */
#define HLTHUNK_CLK_THROTTLE_POWER	(1 << HL_CLK_THROTTLE_TYPE_POWER)
#define HLTHUNK_CLK_THROTTLE_THERMAL	(1 << HL_CLK_THROTTLE_TYPE_THERMAL)

struct hlthunk_clk_throttle_info {
	uint32_t clk_throttle_reason_bitmask;
	uint64_t clk_throttle_start_timestamp_us[HL_CLK_THROTTLE_TYPE_MAX];
	uint64_t clk_throttle_duration_ns[HL_CLK_THROTTLE_TYPE_MAX];
};

struct hlthunk_energy_info {
	uint64_t total_energy_consumption;
};

struct hlthunk_power_info {
	uint64_t power;
};

struct hlthunk_cs_in {
	void *chunks_restore;
	void *chunks_execute;
	uint32_t num_chunks_restore;
	uint32_t num_chunks_execute;
	uint32_t flags;
};

struct hlthunk_cs_out {
	uint64_t seq;
	uint32_t status;
	uint16_t sob_count_before_submission;
};

struct hlthunk_signal_in {
	void *chunks_restore;
	uint32_t num_chunks_restore;
	uint32_t queue_index;
	uint32_t flags;
};

struct hlthunk_signal_out {
	uint64_t seq;
	uint32_t status;
	uint32_t sob_base_addr_offset;
	uint16_t sob_count_before_submission;
};

struct hlthunk_sig_res_in {
	uint32_t queue_index;
	uint32_t count;
};

struct reserve_sig_handle {
	uint32_t id;
	uint32_t sob_base_addr_offset;
	uint32_t count;
};

struct hlthunk_sig_res_out {
	struct reserve_sig_handle handle;
	uint32_t status;
};

struct hlthunk_wait_for_signal_data {
	union {
		uint64_t *signal_seq_arr;
		uint64_t encaps_signal_seq;
	};
	uint32_t signal_seq_nr; /* value of 1 is currently supported */
	uint32_t queue_index;
	uint32_t flags; /* currently unused */
	uint32_t collective_engine_id;
	uint32_t encaps_signal_offset;
};

struct hlthunk_wait_in {
	void *chunks_restore;
	uint32_t num_chunks_restore;
	uint64_t *hlthunk_wait_for_signal;
	uint32_t num_wait_for_signal; /* value of 1 is currently supported */
	uint32_t flags;
};

struct hlthunk_wait_out {
	uint64_t seq;
	uint32_t status;
};

enum wq_types {
	WQ_WRITE = 1,
	WQ_RENDEZVOUS_READ = 2,
	WQ_RENDEZVOUS_WRITE = 3
};

/**
 * struct hlthunk_requester_conn_ctx - set up a requester connection context
 * @dst_ip_addr: Destination IP address in native endianness
 * @dst_conn_id: Destination connection ID
 * @last_index: Index of last entry [2..(2^22)-1]. NOTE: relevant for Gaudi1 (only)
 * @dst_mac_addr: Destination MAC address
 * @priority: Connection priority [0..3]
 * @timer_granularity: Timer granularity [0..127]
 * @swq_granularity: SWQ granularity [0 for 32B or 1 for 64B]
 * @wq_type: Work queue type [1..3]
 * @cq_number: Completion queue number
 * @wq_remote_log_size: Remote Work queue log size [2^QPC] Rendezvous
 * @congestion_wnd: Congestion-Window size
 * @mtu: Max Transmit Unit
 * @congestion_en: Enable/disable Congestion-Control
 * @encap_en: used as boolean; indicates if this QP has encapsulation support
 * @encap_id: Encapsulation-id; valid only if 'encap_en' is set
 * @loopback: used as boolean; indicates if this QP used for loopback mode.
 * @wq_size: Max number of elements in the work queue. NOTE: relevant for Gaudi2 (or higher)
 * @coll_lag_idx: (Gaudi3 and above) The index of the specific NIC within the LAG. Note that the
 *                advanced flag must be enabled in case it's being set.
 * @coll_last_in_lag: (Gaudi3 and above) Is the specific NIC the last one within the collective LAG.
 *                    Note that the advanced flag must be enabled in case it's being set.
 * @compression_en: Enable compression
 * @remote_key: Remote-key to be used to generate on outgoing packets
 * @sack_en: (Gaudi3 and above) Enable Selective Acknowlegment (SACK)
 */
struct hlthunk_requester_conn_ctx {
	uint32_t deprecated;
	uint32_t dst_ip_addr;
	uint32_t dst_conn_id;
	uint32_t deprecated1;
	uint32_t last_index;
	uint8_t dst_mac_addr[ETH_ALEN];
	uint8_t deprecated2;
	uint8_t priority;
	uint8_t deprecated3;
	uint8_t timer_granularity;
	uint8_t swq_granularity;
	uint8_t wq_type;
	uint8_t deprecated4;
	uint8_t cq_number;
	uint8_t wq_remote_log_size;
	uint32_t congestion_wnd;
	uint16_t mtu;
	uint8_t congestion_en;
	uint8_t encap_en;
	uint8_t encap_id;
	uint8_t loopback;
	uint32_t wq_size;
	uint8_t coll_lag_idx;
	uint8_t coll_last_in_lag;
	uint8_t compression_en;
	uint32_t remote_key;
	uint8_t sack_en;
};

struct hlthunk_requester_conn_ctx_out {
	uint64_t swq_mem_handle;
	uint64_t rwq_mem_handle;
};

/**
 * struct hlthunk_responder_conn_ctx - set up a responder connection context
 * @dst_ip_addr: Destination IP address in native endianness
 * @dst_conn_id: Destination connection ID
 * @port: NIC port ID
 * @conn_id: Connection ID
 * @dst_mac_addr: Destination MAC address
 * @priority: Connection priority [0..3]
 * @wq_peer_granularity: Work queue granularity
 * @cq_number: Completion queue number
 * @conn_peer: Connection peer
 * @rdv: used as boolean; indicates if this QP is RDV (WRITE or READ)
 * @loopback: used as boolean; indicates if this QP used for loopback mode.
 * @wq_peer_size: size of the peer Work queue
 * @encap_en: used as boolean; indicates if this QP has encapsulation support
 * @encap_id: Encapsulation-id; valid only if 'encap_en' is set
 * @local_key: Local-key to be used to validate against incoming packets
 * @sack_en: (Gaudi3 and above) Enable Selective Acknowlegment (SACK)
 */
struct hlthunk_responder_conn_ctx {
	uint32_t deprecated;
	uint32_t dst_ip_addr;
	uint32_t dst_conn_id;
	uint8_t dst_mac_addr[ETH_ALEN];
	uint8_t priority;
	uint8_t deprecated1;
	uint8_t deprecated2;
	uint8_t wq_peer_granularity;
	uint8_t cq_number;
	uint8_t deprecated3;
	uint32_t conn_peer;
	uint8_t rdv;
	uint8_t loopback;
	uint32_t wq_peer_size;
	uint8_t encap_en;
	uint8_t encap_id;
	uint32_t local_key;
	uint8_t sack_en;
};

struct hlthunk_nic_wq_arr_set_in {
	uint32_t port;
	uint64_t addr;
	uint32_t num_of_wqs;
	uint32_t num_of_wq_entries;
	uint32_t type;
	enum hl_nic_mem_id mem_id;
	uint8_t swq_granularity;
};

struct hlthunk_nic_wq_arr_set_out {
	uint64_t handle;
};

struct hlthunk_nic_user_cq_id_alloc_in {
	uint32_t port;
};

struct hlthunk_nic_user_cq_id_alloc_out {
	uint32_t id;
};

struct hlthunk_nic_user_cq_id_set_in {
	uint32_t port;
	uint32_t num_of_cqes;
	uint32_t id;
};

struct hlthunk_nic_user_cq_id_set_out {
	uint64_t mem_handle;
	uint64_t pi_handle;
	uint64_t regs_handle;
	uint32_t regs_offset;
};

struct hlthunk_nic_user_cq_id_unset_in {
	uint32_t port;
	uint64_t id;
};

struct hlthunk_nic_alloc_coll_conn_in {
	uint8_t is_scale_out;
};

struct hlthunk_encap_cfg {
	uintptr_t tnl_hdr_ptr;
	uint32_t tnl_hdr_size;
	uint32_t port;
	uint32_t id;
	uint32_t src_ipv4_addr;
	enum hl_nic_encap_type encap_type;
	union {
		uint16_t udp_dst_port;
		uint16_t ip_proto;
	};
};

struct hlthunk_open_stats_info {
	uint64_t open_counter;
	uint64_t last_open_period_ms;
	uint8_t is_compute_ctx_active;
	uint8_t compute_ctx_in_release;
	uint8_t compute_ctx_has_mapped_resources;
};

struct hlthunk_hw_asic_status {
	struct hlthunk_clk_throttle_info throttle;
	struct hlthunk_open_stats_info open_stats;
	struct hlthunk_power_info power;
	enum hl_device_status status;
	uint64_t timestamp_sec;
	uint8_t valid;
};

/**
 * struct hlthunk_nic_user_set_app_params_in -Allow the user application to set general parameters
 *                                        regarding the RDMA nic operation. These parameters stay
 *                                        in effect until the application releases the device.
 * @advanced: A boolean that indicates whether this WQ should support advanced operations, such as
 *            RDV, QMan, WTD, etc.
 * @bp_offs: Offsets in NIC memory to signal a back pressure. Note that the advanced flag
 *              must be enabled in case it's being set.
 * @fna_mask_size: Determines the width of the completion value.
 * @fna_fifo_offs: SRAM/DCCM addresses provided to the HW by the user when FnA completion is
 *              configured in the SRAM/DDCM.
 */
struct hlthunk_nic_user_set_app_params_in {
	__u8 advanced;
	__u32 bp_offs[HL_NIC_USER_BP_OFFS_MAX];
	__u8 deprecated;
	__u8 fna_mask_size;
	__u32 fna_fifo_offs[HL_NIC_FNA_CMPL_ADDR_NUM];
};

struct hlthunk_nic_user_get_app_params_out {
	uint32_t max_num_of_qps;
	uint32_t num_allocated_qps;
	uint32_t max_allocated_qp_idx;
	uint32_t max_cq_size;
	uint8_t advanced;
	uint8_t max_num_of_cqs;
	uint8_t max_num_of_db_fifos;
	uint8_t max_num_of_encaps;
	uint32_t speed;
	uint32_t max_num_of_coll_qps;
	uint32_t coll_qps_offset;
	uint32_t base_coll_qp_idx;
	uint32_t base_scale_out_coll_qp_idx;
	uint32_t max_num_of_scale_out_coll_qps;
};

struct hlthunk_wait_multi_cs_in {
	uint64_t *seq;
	uint64_t timeout_us;
	uint32_t seq_len;
};

struct hlthunk_wait_multi_cs_out {
	uint32_t status;
	uint32_t seq_set;
	uint8_t completed;
};

struct hlthunk_nic_eq_poll_out {
	/* holds HL_NIC_EQ_POLL_STATUS_* */
	uint32_t poll_status;
	uint32_t idx;
	uint32_t ev_data;
	/* holds HL_NIC_EQ_EVENT_TYPE_* */
	uint8_t ev_type;
};

/**
 * struct hlthunk_get_habana_link_state_in - get PCS link state of Habana link internal port.
 * @port: the port to probe.
 */
struct hlthunk_get_habana_link_state_in {
	uint32_t port;
	uint32_t pad;
};

/**
 * struct hlthunk_get_habana_link_state_out - PCS link state of the probed Habana link internal
 * port.
 * @up: state of the PCS link.
 */
struct hlthunk_get_habana_link_state_out {
	uint8_t up;
	uint8_t pad[7];
};

/**
 * struct hlthunk_get_habana_link_stat_in - info for getting a Habana link internal port statistics.
 * @port: the port to probe.
 */
struct hlthunk_get_habana_link_stat_in {
	uint32_t port;
};

/**
 * struct hlthunk_get_habana_link_stat_out - info for result of getting a Habana link internal port
 * statistics.
 * @str_buf: buffer allocated by the user to be filled with the counters names. Recommended size is
 *           HABANA_LINK_STR_LEN * HABANA_LINK_CNT_MAX_NUM.
 * @val_buf: buffer allocated by the user to be filled with the counters values. Recommended size is
 *           sizeof(__u64) * HABANA_LINK_CNT_MAX_NUM.
 * @num_of_stat: the actual number of counters that were retrieved.
 */
struct hlthunk_get_habana_link_stat_out {
	uint8_t *str_buf;
	uint64_t *val_buf;
	uint32_t num_of_stat;
};

/**
 * struct hlthunk_nic_get_ports_masks_out - ports mask info.
 * @ports_mask: Mask of available ports.
 * @ext_ports_mask: Mask of external ports (subset of ports_mask).
 */
struct hlthunk_nic_get_ports_masks_out {
	uint64_t ports_mask;
	uint64_t ext_ports_mask;
};

/* DRAM replaced rows related data structures */
#define DRAM_ROW_REPLACE_MAX	32

enum hlthunk_dram_row_replace_cause {
	HLTHUNK_ROW_REPLACE_CAUSE_DOUBLE_ECC_ERR,
	HLTHUNK_ROW_REPLACE_CAUSE_MULTI_SINGLE_ECC_ERR,
};

struct hlthunk_dram_row_info {
	uint8_t dram_idx;
	uint8_t pc;
	uint8_t sid;
	uint8_t bank_idx;
	uint16_t row_addr;
	uint8_t replaced_row_cause; /* enum hlthunk_dram_row_replace_cause */
	uint8_t pad;
};

/*
 * struct hlthunk_dram_replaced_rows_info -
 * @num_replaced_rows: number of replaced rows.
 * @replaced_rows: replaced rows info.
 */
struct hlthunk_dram_replaced_rows_info {
	uint16_t num_replaced_rows;
	uint8_t pad[6];
	struct hlthunk_dram_row_info replaced_rows[DRAM_ROW_REPLACE_MAX];
};

/**
 * struct hlthunk_event_record_open_dev_time - timestamp of last time device was opened and
 *                                             CS timeout or razwi error occurred.
 * @timestamp: timestamp of device open.
 */
struct hlthunk_event_record_open_dev_time {
	int64_t timestamp;
};

/**
 * struct hlthunk_event_record_cs_timeout - last CS timeout information.
 * @timestamp: timestamp when last CS timeout event occurred.
 * @seq: sequence number of last CS timeout event.
 */
struct hlthunk_event_record_cs_timeout {
	int64_t timestamp;
	uint64_t seq;
};

/**
 * struct hlthunk_nic_user_ccq_set_in - info for cc-q creation
 * @port: the port associated with this ccq
 * @num_of_entries: number of entries in the CCQ buffer
 */
struct hlthunk_nic_user_ccq_set_in {
	uint32_t port;
	uint32_t num_of_entries;
};

/**
 * struct hlthunk_nic_user_ccq_set_out - info for mapping ccq
 * @ccq_handle: handle for mapping NIC CCQ buffer
 * @ccq_pi_handle: handle for mapping NIC CCQ pi
 */
struct hlthunk_nic_user_ccq_set_out {
	uint64_t ccq_handle;
	uint64_t ccq_pi_handle;
};

/**
 * struct hlthunk_nic_user_db_fifo_set_in - ioctl in param
 * @port: nic port id
 * @id: nic db-fifo id
 * @base_sob_addr: base address of Sync Object
 * @num_sobs: number of Sobs should be used by NIC
 * @mode: represents desired mode of operation for provided FIFO, according to hl_nic_db_fifo_type
 * @dir_dup_ports_mask: ports for which the hw should duplicate the direct patcher descriptor
 */
struct hlthunk_nic_user_db_fifo_set_in {
	uint32_t port;
	uint32_t id;
	uint32_t base_sob_addr;
	uint32_t num_sobs;
	uint8_t mode;
	uint8_t dir_dup_ports_mask;
};

/**
 * struct hlthunk_nic_user_db_fifo_set_out - ioctl out param
 * @ci_handle: handle of db-fifo consumer index memory buffer
 * @regs_handle: handle of db-fifo registers base-address
 * @fifo_size: Fifo size allocated by the driver
 * @fifo_bp_thresh: fifo back pressure threshold programmed by the driver
 * @regs_offset: offset to the db-fifo registers
 */
struct hlthunk_nic_user_db_fifo_set_out {
	uint64_t ci_handle;
	uint64_t regs_handle;
	uint32_t fifo_size;
	uint32_t fifo_bp_thresh;
	uint32_t regs_offset;
};

/**
 * struct hlthunk_event_record_razwi_event - razwi information.
 * @timestamp: timestamp of razwi.
 * @addr: address which accessing it caused razwi.
 * @engine_id: engine id of the razwi initiator, if it was initiated by engine that does not
 *             have engine id it will be set to HL_RAZWI_NA_ENG_ID. If there are several possible
 *             engines which caused the razwi, it will hold all of them.
 * @num_of_possible_engines: contains number of possible engine ids. In some asics, razwi indication
 *                           might be common for several engines and there is no way to get the
 *                           exact engine. In this way, engine_id array will be filled with all
 *                           possible engines caused this razwi. Also, there might be possibility
 *                           in gaudi, where we don't indication on specific engine, in that case
 *                           the value of this parameter will be zero.
 * @flags: bitmask for additional data: HL_RAZWI_READ - razwi caused by read operation
 *                                      HL_RAZWI_WRITE - razwi caused by write operation
 *                                      HL_RAZWI_LBW - razwi caused by lbw fabric transaction
 *                                      HL_RAZWI_HBW - razwi caused by hbw fabric transaction
 *                                      HL_RAZWI_RR - razwi caused by range register
 *                                      HL_RAZWI_ADDR_DEC - razwi caused by address decode error
 *         Note: this data is not supported by all asics, in that case the relevant bits will not
 *               be set.
 */
struct hlthunk_event_record_razwi_event {
	int64_t timestamp;
	uint64_t addr;
	uint16_t engine_id[HL_RAZWI_MAX_NUM_OF_ENGINES_PER_RTR];
	uint16_t num_of_possible_engines;
	uint8_t flags;
};

/**
 * struct hlthunk_event_record_undefined_opcode - info about last undefined opcode error
 * @timestamp: timestamp of the undefined opcode error
 * @cb_addr_streams: CB addresses (per stream) that are currently exists in the PQ
 *                   entiers. In case all streams array entries are
 *                   filled with values, it means the execution was in Lower-CP.
 * @cq_addr: the address of the current handled command buffer
 * @cq_size: the size of the current handled command buffer
 * @cb_addr_streams_len: num of streams - actual len of cb_addr_streams array.
 *                       should be equal to 1 incase of undefined opcode
 *                       in Upper-CP (specific stream) and equal to 4 incase
 *                       of undefined opcode in Lower-CP.
 * @engine_id: engine-id that the error occurred on
 * @stream_id: the stream id the error occurred on. In case the stream equals to
 *             MAX_QMAN_STREAMS_INFO it means the error occurred on a Lower-CP.
 */
struct hlthunk_event_record_undefined_opcode {
	int64_t  timestamp;
	uint64_t cb_addr_streams[MAX_QMAN_STREAMS_INFO][OPCODE_INFO_MAX_ADDR_SIZE];
	uint64_t cq_addr;
	uint32_t cq_size;
	uint32_t cb_addr_streams_len;
	uint32_t engine_id;
	uint32_t stream_id;
};

/**
 * struct hlthunk_event_record_critical_hw_err - info about the HW error that occurred
 * @timestamp: The time of the error occurrence.
 * @event_id: The device event generated by this error.
 */
struct hlthunk_event_record_critical_hw_err {
	int64_t timestamp;
	uint16_t event_id;
};

/**
 * struct hlthunk_event_record_critical_fw_err - info about the FW error that occurred
 * @timestamp: timestamp of the undefined opcode error
 * @err_type: The type of error being reported.
 * @reported_err_id: The reported error ID in-case of FW_REPORTED_ERR.
 */
struct hlthunk_event_record_critical_fw_err {
	int64_t timestamp;
	enum hl_info_fw_err_type err_type;
	uint16_t reported_err_id;
};

/**
 * struct hlthunk_event_record_engine_err - info about the engine reported an error
 * @timestamp: timestamp of the engine error.
 * @engine_id: engine id who reported the error.
 * @error_count: Amount of errors reported.
 */
struct  hlthunk_event_record_engine_err {
	int64_t timestamp;
	uint16_t engine_id;
	uint16_t error_count;
};

#define SEC_PCR_DATA_BUF_SZ	256
#define SEC_PCR_QUOTE_BUF_SZ	510	/* (512 - 2) 2 bytes used for size */
#define SEC_SIGNATURE_BUF_SZ	255	/* (256 - 1) 1 byte used for size */
#define SEC_PUB_DATA_BUF_SZ	510	/* (512 - 2) 2 bytes used for size */
#define SEC_CERTIFICATE_BUF_SZ	2046	/* (2048 - 2) 2 bytes used for size */

/*
 * struct hlthunk_sec_attest_info - attestation report of the boot
 * @nonce: number only used once. random number provided by host. this also passed to the quote
 *         command as a qualifying data.
 * @pcr_quote_len: length of the attestation quote data (bytes)
 * @pub_data_len: length of the public data (bytes)
 * @certificate_len: length of the certificate (bytes)
 * @pcr_num_reg: number of PCR registers in the pcr_data array
 * @pcr_reg_len: length of each PCR register in the pcr_data array (bytes)
 * @quote_sig_len: length of the attestation report signature (bytes)
 * @pcr_data: raw values of the PCR registers
 * @pcr_quote: attestation report data structure
 * @quote_sig: signature structure of the attestation report
 * @public_data: public key for the signed attestation
 *		 (outPublic + name + qualifiedName)
 * @certificate: certificate for the attestation signing key
 */
struct hlthunk_sec_attest_info {
	uint32_t nonce;
	uint16_t pcr_quote_len;
	uint16_t pub_data_len;
	uint16_t certificate_len;
	uint8_t pcr_num_reg;
	uint8_t pcr_reg_len;
	uint8_t quote_sig_len;
	uint8_t pcr_data[SEC_PCR_DATA_BUF_SZ];
	uint8_t pcr_quote[SEC_PCR_QUOTE_BUF_SZ];
	uint8_t quote_sig[SEC_SIGNATURE_BUF_SZ];
	uint8_t public_data[SEC_PUB_DATA_BUF_SZ];
	uint8_t certificate[SEC_CERTIFICATE_BUF_SZ];
};

/*
 * struct hlthunk_dev_info_signed - device information signed by a secured device.
 * @nonce: number only used once. random number provided by host. this also passed to the quote
 *         command as a qualifying data.
 * @pub_data_len: length of the public data (bytes)
 * @certificate_len: length of the certificate (bytes)
 * @info_sig_len: length of the attestation signature (bytes)
 * @public_data: public key info signed info data (outPublic + name + qualifiedName)
 * @certificate: certificate for the signing key
 * @info_sig: signature of the info + nonce data.
 * @dev_info_len: length of the device info (bytes)
 * @dev_info: device info (provided as an array of bytes).
 */
struct hlthunk_dev_info_signed {
	uint32_t nonce;
	uint16_t pub_data_len;
	uint16_t certificate_len;
	uint8_t info_sig_len;
	uint8_t public_data[SEC_PUB_DATA_BUF_SZ];
	uint8_t certificate[SEC_CERTIFICATE_BUF_SZ];
	uint8_t info_sig[SEC_SIGNATURE_BUF_SZ];
	uint16_t dev_info_len;
	uint8_t dev_info[SEC_DEV_INFO_BUF_SZ];
};

/**
 * struct hlthunk_user_mapping - user mapping information.
 * @dev_va: device virtual address.
 * @size: virtual address mapping size.
 */
struct hlthunk_user_mapping {
	uint64_t dev_va;
	uint64_t size;
};
/*
 * struct hl_page_fault_info - page fault information.
 * @timestamp: timestamp of page fault.
 * @addr: address which accessing it caused page fault.
 * @engine_id: engine id which caused the page fault, supported only in gaudi3.
 * @num_of_mappings: actual number of user mappings, filled by the driver. Even if error is
 *                   returned, this parameter will be filled by driver, unless driver did not had
 *                   the chance to fill it, in that case it will be equal to 0xFFFFFFFF.
 * @mappings_buf: holds all user mappings
 */
struct hlthunk_page_fault_info {
	int64_t timestamp;
	uint64_t addr;
	uint16_t engine_id;
	uint32_t num_of_mappings;
	struct hlthunk_user_mapping *mappings_buf;

};

struct hlthunk_functions_pointers {
	/*
	 * Functions that will be wrapped with profiler code to enable
	 * profiling
	 */
	int (*fp_hlthunk_command_submission)(int fd, struct hlthunk_cs_in *in,
						struct hlthunk_cs_out *out);
	int (*fp_hlthunk_open)(enum hlthunk_device_name device_name,
				const char *busid);
	int (*fp_hlthunk_close)(int fd);
	int (*fp_hlthunk_profiler_start)(int fd);
	int (*fp_hlthunk_profiler_stop)(int fd);
	int (*fp_hlthunk_profiler_get_trace)(int fd, void *buffer,
						uint64_t *size,
						uint64_t *num_entries);

	/* Function for the profiler to use */
	uint64_t (*fp_hlthunk_device_memory_alloc)(int fd, uint64_t size, uint64_t page_size,
						bool contiguous, bool shared);
	int (*fp_hlthunk_device_memory_free)(int fd, uint64_t handle);
	uint64_t (*fp_hlthunk_device_memory_map)(int fd, uint64_t handle,
							uint64_t hint_addr);
	uint64_t (*fp_hlthunk_host_memory_map)(int fd, void *host_virt_addr,
						uint64_t hint_addr,
						uint64_t host_size);
	int (*fp_hlthunk_memory_unmap)(int fd, uint64_t device_virt_addr);
	int (*fp_hlthunk_debug)(int fd, struct hl_debug_args *debug);
	int (*fp_hlthunk_request_command_buffer)(int fd, uint32_t cb_size,
							uint64_t *cb_handle);
	int (*fp_hlthunk_destroy_command_buffer)(int fd, uint64_t cb_handle);
	int (*fp_hlthunk_wait_for_cs)(int fd, uint64_t seq,
					uint64_t timeout_us, uint32_t *status);
	enum hlthunk_device_name (*fp_hlthunk_get_device_name_from_fd)(int fd);
	int (*fp_hlthunk_get_pci_bus_id_from_fd)(int fd, char *pci_bus_id,
							int len);
	int (*fp_hlthunk_get_device_index_from_pci_bus_id)(const char *busid);
	void* (*fp_hlthunk_malloc)(size_t size);
	void (*fp_hlthunk_free)(void *pt);
	int (*fp_hlthunk_signal_submission)(int fd,
					struct hlthunk_signal_in *in,
					struct hlthunk_signal_out *out);
	int (*fp_hlthunk_wait_for_signal)(int fd, struct hlthunk_wait_in *in,
					struct hlthunk_wait_out *out);
	int (*fp_hlthunk_get_time_sync_info)(int fd,
					struct hlthunk_time_sync_info *info);
	int (*fp_hlthunk_get_cs_counters_info)(int fd,
					struct hl_info_cs_counters *info);
	void (*fp_hlthunk_profiler_destroy)(void);
	int (*fp_hlthunk_request_mapped_command_buffer)(int fd,
					uint32_t cb_size, uint64_t *cb_handle);
	int (*fp_hlthunk_wait_for_collective_sig)(int fd,
					struct hlthunk_wait_in *in,
					struct hlthunk_wait_out *out);
	int (*fp_hlthunk_deprecated_func1)(int fd, uint64_t seq,
					uint64_t timeout_us, uint32_t *status,
					uint64_t *timestamp);
	int (*fp_hlthunk_get_cb_usage_count)(int fd, uint64_t cb_handle,
						uint32_t *usage_cnt);
	int (*fp_hlthunk_staged_command_submission)(int fd, uint64_t sequence,
						struct hlthunk_cs_in *in,
						struct hlthunk_cs_out *out);
	int (*fp_hlthunk_get_hw_block)(int fd, uint64_t block_address,
						uint32_t *block_size,
						uint64_t *handle);
	int (*fp_hlthunk_wait_for_interrupt)(int fd, void *addr,
					uint64_t target_value,
					uint32_t interrupt_id,
					uint64_t timeout_us,
					uint32_t *status);
	int (*fp_hlthunk_device_memory_export_dmabuf_fd)(int fd,
							uint64_t handle,
							uint64_t size,
							uint32_t flags);
	int (*fp_hlthunk_command_submission_timeout)(int fd,
						struct hlthunk_cs_in *in,
						struct hlthunk_cs_out *out,
						uint32_t timeout);
	int (*fp_hlthunk_signal_submission_timeout)(int fd,
					struct hlthunk_signal_in *in,
					struct hlthunk_signal_out *out,
					uint32_t timeout);
	int (*fp_hlthunk_wait_for_signal_timeout)(int fd,
					struct hlthunk_wait_in *in,
					struct hlthunk_wait_out *out,
					uint32_t timeout);
	int (*fp_hlthunk_wait_for_collective_sig_timeout)(int fd,
					struct hlthunk_wait_in *in,
					struct hlthunk_wait_out *out,
					uint32_t timeout);
	int (*fp_hlthunk_staged_cs_timeout)(int fd,
					uint64_t sequence,
					struct hlthunk_cs_in *in,
					struct hlthunk_cs_out *out,
					uint32_t timeout);
	int (*fp_hlthunk_reserve_signals)(int fd,
					struct hlthunk_sig_res_in *in,
					struct hlthunk_sig_res_out *out);
	int (*fp_hlthunk_unreserve_signals)(int fd,
					struct reserve_sig_handle *handle,
					uint32_t *status);
	int (*fp_hlthunk_staged_cs_encaps_signals)(int fd,
					uint64_t sequence,
					struct hlthunk_cs_in *in,
					struct hlthunk_cs_out *out);
	int (*fp_hlthunk_wait_for_reserved_encaps_signals)(int fd,
					struct hlthunk_wait_in *in,
					struct hlthunk_wait_out *out);
	int (*fp_hlthunk_wait_for_collective_reserved_encap_sig)(int fd,
					struct hlthunk_wait_in *in,
					struct hlthunk_wait_out *out);
	uint64_t (*fp_hlthunk_host_memory_map_flags)(int fd, void *host_virt_addr,
						uint64_t hint_addr,
						uint64_t host_size,
						uint32_t flags);
	int (*fp_get_dram_replaced_rows_info)(int fd,
					struct hlthunk_dram_replaced_rows_info *info);
	int (*fp_get_dram_pending_rows_info)(int fd, uint32_t *out);
	int (*fp_DEPRECATED1)(int fd, void *addr, uint64_t target_value, uint32_t engine_id,
				uint64_t timeout_us, uint32_t *status, uint64_t *timestamp);
	int (*fp_hlthunk_wait_for_interrupt_by_handle)(int fd,
					uint64_t cq_counters_handle,
					uint64_t cq_counters_offset,
					uint64_t target_value,
					uint32_t interrupt_id,
					uint64_t timeout_us,
					uint32_t *status);
	int (*fp_hlthunk_get_mapped_cb_device_va_by_handle)(int fd,
						uint64_t cb_handle,
						uint64_t *device_va);
	int (*fp_hlthunk_get_pll_frequency)(int fd, uint32_t index,
					struct hlthunk_pll_frequency_info *frequency);
	int (*fp_hlthunk_deprecated_func2)(int fd, uint32_t interrupt_id,
							uint64_t cq_counters_handle,
							uint64_t cq_counters_offset,
							uint64_t target_value,
							uint64_t timestamp_handle,
							uint64_t timestamp_offset);
	int (*fp_hlthunk_deprecated_func3)(int fd, uint32_t num_elements, uint64_t *handle);
	int (*fp_hlthunk_device_mapped_memory_export_dmabuf_fd)(int fd,
							uint64_t addr,
							uint64_t size,
							uint64_t offset,
							uint32_t flags);
	int (*fp_hlthunk_get_time_sync_per_die_info)(int fd,
					uint32_t die_index,
					struct hlthunk_time_sync_info *info);
	int (*fp_hlthunk_get_hw_ip_info)(int fd,
					struct hlthunk_hw_ip_info *hw_ip);
};

struct hlthunk_debugfs {
	int addr_fd;
	int data_fd;
	int clk_gate_fd;
	char clk_gate_val[32];
};

/**
 * This function opens the habanalabs device according to specified busid, or
 * according to the device name, if busid is NULL. If busid is specified but
 * the device can't be opened, the function fails.
 * @param device_name name of the device that the user wants to open
 * @param busid pci address of the device on the host pci bus
 * @return file descriptor handle or negative value in case of error
 */
hlthunk_public int hlthunk_open(enum hlthunk_device_name device_name,
				const char *busid);

/**
 * This function opens the habanalabs device according to a specified module id.
 * This API is relevant only for ASICs that support this property
 * @param module_id a number representing the module_id in the host machine
 * @return file descriptor handle or negative value in case of error
 */
hlthunk_public int hlthunk_open_by_module_id(uint32_t module_id);

/**
 * This function opens the habanalabs control device according to specified
 * busid, or according to the requested device id number, if busid is NULL. If
 * busid is specified but the device can't be opened, the function fails.
 * @param dev_id the dev_id number of the control device as appears in /dev/.
 * The control devices appear like this:
 * /dev/accel/accel_controlD0
 * /dev/accel/accel_controlD1
 * ...
 * So the dev_id represents the number that appear at the name of the node.
 * Note it is different from the minor number
 * @param busid pci address of the device on the host pci bus
 * @return file descriptor handle or negative value in case of error
 */
hlthunk_public int hlthunk_open_control(int dev_id, const char *busid);

/**
 * This function opens the habanalabs control device according to specified busid,
 * or according to the device name, if busid is NULL. If busid is specified but
 * the device can't be opened, the function fails.
 * @param device_name name of the device that the user wants to open
 * @param busid pci address of the device on the host pci bus
 * @return file descriptor handle or negative value in case of error
 */
hlthunk_public int hlthunk_open_control_by_name(enum hlthunk_device_name device_name,
					const char *busid);

/**
 * This function opens the habanalabs control device according to a specified module id.
 * This API is relevant only for ASICs that support this property
 * @param module_id a number representing the module_id in the host machine
 * @return file descriptor handle or negative value in case of error
 */
hlthunk_public int hlthunk_open_control_by_module_id(uint32_t module_id);

/**
 * This function opens the habanalabs control device according to a specified bus id.
 * @param busid null-terminated string of the device PCI bus ID in the following
 * format: [domain]:[bus]:[device].[function] where domain, bus, device, and
 * function are all hexadecimal values.
 * @return file descriptor handle or negative value in case of error
 */
hlthunk_public int hlthunk_open_control_by_bus_id(const char *busid);

/**
 * This function closes an open file descriptor
 * @param fd file descriptor handle
 * @return the return value of the close() syscall
 */
hlthunk_public int hlthunk_close(int fd);

/**
 * This function retrieves the PCI device ID of a specific device through the
 * INFO IOCTL call
 * @param fd file descriptor handle of habanalabs main or control device
 * @return PCI device ID of the acquired device
 */
hlthunk_public uint32_t hlthunk_get_device_id_from_fd(int fd);

/**
 * This function returns the matching device name (ASIC type) of a specific
 * device through the INFO IOCTL call
 * @param fd file descriptor handle of habanalabs main or control device
 * @return enumeration value that represents the ASIC type of the acquired
 * device
 */
hlthunk_public enum hlthunk_device_name hlthunk_get_device_name_from_fd(int fd);

/**
 * This function retrieves the PCI bus ID of a specific device
 * @param fd file descriptor handle of habanalabs main device
 * @param pci_bus_id null-terminated string for the device in the following
 * format: [domain]:[bus]:[device].[function] where domain, bus, device, and
 * function are all hexadecimal values. pci_bus_id should be large enough to
 * store 13 characters including the NULL-terminator.
 * @param len maximum length of string to store in pci_bus_id
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_pci_bus_id_from_fd(int fd, char *pci_bus_id,
							int len);

/**
 * This function retrieves the device index of a specific device by its PCI bus ID
 * @param busid null-terminated string of the device PCI bus ID in the following
 * format: [domain]:[bus]:[device].[function] where domain, bus, device, and
 * function are all hexadecimal values.
 * @return device index for success, negative value for failure
 */
hlthunk_public int hlthunk_get_device_index_from_pci_bus_id(const char *busid);

/**
 * This function retrieves the device index of a specific device by its module ID
 * @param module_id a number representing the module_id in the host machine
 * @return device index for success, negative value for failure
 */
hlthunk_public int hlthunk_get_device_index_from_module_id(uint32_t module_id);

/**
 * This function retrieves H/W IP information for a specific device
 * @param fd file descriptor handle of habanalabs main device
 * @param hw_ip info pointer to H/W IP information structure
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_hw_ip_info(int fd,
					struct hlthunk_hw_ip_info *hw_ip);

/**
 * This function retrieves statistics info on device open operations
 * @param fd file descriptor handle of habanalabs main or control device
 * @param open_stats info pointer to open stats structure
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_open_stats(int fd,
				struct hlthunk_open_stats_info *open_stats);

/**
 * This function retrieves aggregated asic status including general status,
 * open_stas, throttling and power info.
 * @param fd file descriptor handle of habanalabs main or control device
 * @param hw_asic_status info pointer to hw asic status structure
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_hw_asic_status(int fd,
				struct hlthunk_hw_asic_status *hw_asic_status);

/**
 * This function retrieves DRAM usage information for a specific device
 * @param fd file descriptor handle of habanalabs main device
 * @param dram_usage pointer to DRAM usage information structure
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_dram_usage(int fd,
				struct hlthunk_dram_usage_info *dram_usage);

/**
 * This function retrieves the status of a specific device
 * @param fd file descriptor handle of habanalabs main device
 * @return enum value that represents the device's status in case it was acquired,
 * otherwise - returns Linux error code.
 */
hlthunk_public int hlthunk_get_device_status_info(int fd);

/**
 * This function retrieves the status of a specific device with an option to stall any upcoming
 * soft resets. It is useful to ensure that the status will remain the same for a certain amount
 * of time after polling.
 * @param fd file descriptor handle of habanalabs main device
 * @param soft_reset_stall whether to stall any upcoming soft reset after the polling
 * @return enum value that represents the device's status in case it was acquired,
 * otherwise - returns Linux error code.
 */
hlthunk_public int hlthunk_get_device_status_info_telemetry(int fd, bool soft_reset_stall);

/**
 * This function checks whether a specific device is idle
 * @param fd file descriptor handle of habanalabs main device
 * @return true if the acquired device is idle, false otherwise
 */
hlthunk_public bool hlthunk_is_device_idle(int fd);

/**
 * This function retrieves a busy engines bitmask of a specific device
 * @param fd file descriptor handle of habanalabs main device
 * @param info pointer to engines idle information structure
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_busy_engines_mask(int fd,
					struct hlthunk_engines_idle_info *info);

/**
 * This function retrieves MAC addresses information for a specific device
 * @param fd file descriptor handle of habanalabs main device
 * @param info pointer to MAC addresses information structure
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_mac_addr_info(int fd, struct hlthunk_mac_addr_info *info);

/**
 * This function retrieves the PLL frequency of a specific device.
 * @param fd file descriptor handle of habanalabs device
 * @param index the index of the specified PLL
 * @param frequency pointer to frequency structure to store the frequency in MHz
 * in each of the available outputs. if a certain output is not available a 0
 * value will be set
 * @return 0 upon success or negative value in case of error
 */
hlthunk_public int hlthunk_get_pll_frequency(int fd, uint32_t index,
				struct hlthunk_pll_frequency_info *frequency);

/**
 * This function retrieves the device utilization as percentage in the last
 * Xms period.
 * @param fd file descriptor handle of habanalabs main device
 * @param period_ms the period value in ms. Valid values are 100-1000, with
 * resolution of 100.
 * @rate pointer to uint32_t to store the utilization rate as percentage.
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_device_utilization(int fd, uint32_t period_ms,
						uint32_t *rate);

/**
 * This function retrieves the h/w events array
 * @param fd file descriptor handle of habanalabs main device
 * @param aggregate whether to retrieve from last reset or from loading of the
 * driver (aggregate mode)
 * @hw_events_arr_size size of hw_events_arr, in bytes
 * @hw_events_arr pointer to array of uint32_t to store the result.
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_hw_events_arr(int fd, bool aggregate,
			uint32_t hw_events_arr_size, uint32_t *hw_events_arr);

/**
 * This function retrieves the ASIC current and maximum clock rate, in MHz
 * @param fd file descriptor handle of habanalabs main device
 * @param cur_clk_mhz pointer to memory that will be filled by the function
 * with the current clock rate in MHz
 * @param max_clk_mhz pointer to memory that will be filled by the function
 * with the maximum clock rate in MHz
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_clk_rate(int fd, uint32_t *cur_clk_mhz,
					uint32_t *max_clk_mhz);

/**
 * This function retrieves the number of times the device had been hard or soft
 * reset since the last time the driver was loaded
 * @param fd file descriptor handle of habanalabs main device
 * @param info pointer to memory that will be filled by the function
 * with the current reset counts.
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_reset_count_info(int fd,
					struct hlthunk_reset_count_info *info);

/**
 * This function retrieves the device's time alongside the host's time
 * for synchronization
 * @param fd file descriptor handle of habanalabs main device
 * @param info pointer to memory that will be filled by the function with the
 * device's and host's times
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_time_sync_info(int fd,
					struct hlthunk_time_sync_info *info);

/**
 * This function retrieves the device's sync manager information
 * @param fd file descriptor handle of habanalabs main device
 * @dcore_id dcore id to fetch relevant sm info from
 * @param info pointer to memory that will be filled by the function with the
 * sync manager information
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_sync_manager_info(int fd, int dcore_id,
					struct hlthunk_sync_manager_info *info);

/**
 * This function retrieves the device's cs counters information
 * @param fd file descriptor handle of habanalabs main device
 * @param info pointer to memory that will be filled by the function with the
 * cs counters
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_cs_counters_info(int fd,
					struct hl_info_cs_counters *info);

/**
 * This function retrieves the device's pci counters information
 * @param fd file descriptor handle of habanalabs main device
 * @param info pointer to memory that will be filled by the function with the
 * pci counters information
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_pci_counters_info(int fd,
					struct hlthunk_pci_counters_info *info);

/**
 * This function retrieves the device's clock throttling information
 * @param fd file descriptor handle of habanalabs main device
 * @param info pointer to memory that will be filled by the function with the
 * clock throttling information
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_clk_throttle_info(int fd,
					struct hlthunk_clk_throttle_info *info);

/**
 * This function retrieves the device's total energy consumption
 * in millijoules (mJ), since the driver was loaded.
 * @param fd file descriptor handle of habanalabs main device
 * @param info pointer to memory, where to fill the total energy consumption
 * info.
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_total_energy_consumption_info(int fd,
				struct hlthunk_energy_info *info);

/**
 * This function retrieves the current device's power in milliwatts (mW)
 * @param fd file descriptor handle of habanalabs main device
 * @param info pointer to memory, where to fill the power info
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_power_info(int fd,
				struct hlthunk_power_info *info);

/**
 * This function retrieves miscellaneous information of a specific device
 * @param fd file descriptor handle of habanalabs main device
 * @param info pointer to device information structure
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_info(int fd, struct hl_info_args *info);

/**
 * This function creates a command buffer for a specific device
 * @param fd file descriptor handle of habanalabs main device
 * @param cb_size size of command buffer
 * @param cb_handle pointer to uint64_t to store the command buffer handle
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_request_command_buffer(int fd, uint32_t cb_size,
							uint64_t *cb_handle);
/**
 * This function creates a command buffer for a specific device and maps it to
 * the device's MMU
 * @param fd file descriptor handle of habanalabs main device
 * @param cb_size size of command buffer
 * @param cb_handle pointer to uint64_t to store the command buffer handle
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_request_mapped_command_buffer(int fd,
					uint32_t cb_size, uint64_t *cb_handle);

/**
 * This function destroys a command buffer for a specific device
 * @param fd file descriptor handle of habanalabs main device
 * @param cb_handle handle of the command buffer
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_destroy_command_buffer(int fd, uint64_t cb_handle);

/**
 * This function retrieves the usage count of a CB for a specific device
 * @param fd file descriptor handle of habanalabs main device
 * @param cb_handle handle of the CB
 * @param usage_cnt pointer to uint32_t to store the usage count of the CB
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_cb_usage_count(int fd, uint64_t cb_handle,
						uint32_t *usage_cnt);

/**
 * This function submits a set of jobs to a specific device
 * @param fd file descriptor handle of habanalabs main device
 * @param in pointer to command submission input structure
 * @param out pointer to command submission output structure
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_command_submission(int fd, struct hlthunk_cs_in *in,
						struct hlthunk_cs_out *out);

/**
 * This function submits a set of jobs to a specific device with a timeout
 * @param fd file descriptor handle of habanalabs main device
 * @param in pointer to command submission input structure
 * @param out pointer to command submission output structure
 * @param timeout duration in seconds
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_command_submission_timeout(int fd,
						struct hlthunk_cs_in *in,
						struct hlthunk_cs_out *out,
						uint32_t timeout);

/**
 * This function submits a set of jobs to a specific device as part of a
 * staged submission
 * @param fd file descriptor handle of habanalabs main device
 * @sequence sequence number of this staged submission obtained from the
 *           first CS submitted
 * @param in pointer to command submission input structure
 * @param out pointer to command submission output structure
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_staged_command_submission(int fd,
						uint64_t sequence,
						struct hlthunk_cs_in *in,
						struct hlthunk_cs_out *out);

/**
 * This function submits a set of jobs to a specific device as part of a
 * staged submission with a timeout
 * @param fd file descriptor handle of habanalabs main device
 * @sequence number of this staged submission obtained from the
 *           first CS submitted
 * @param in pointer to command submission input structure
 * @param out pointer to command submission output structure
 * @param timeout duration in seconds
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_staged_command_submission_timeout(int fd,
						uint64_t sequence,
						struct hlthunk_cs_in *in,
						struct hlthunk_cs_out *out,
						uint32_t timeout);

/**
 * This function waits until a command submission of a specific device has
 * finished executing
 * @param fd file descriptor handle of habanalabs main device
 * @param seq sequence number of command submission
 * @param timeout_us absolute timeout to wait in microseconds. If the timeout
 * value is 0, the driver won't sleep at all. It will check the status of the
 * CS and return immediately
 * @param status pointer to uint32_t to store the wait status
 * @return negative error code on error, otherwise non-negative CS status
 */
hlthunk_public int hlthunk_wait_for_cs(int fd, uint64_t seq,
					uint64_t timeout_us, uint32_t *status);

/**
 * This function waits until a command submission of a specific device has
 * finished executing
 * @param fd file descriptor handle of habanalabs main device
 * @param seq sequence number of command submission
 * @param timeout_us absolute timeout to wait in microseconds. If the timeout
 * value is 0, the driver won't sleep at all. It will check the status of the
 * CS and return immediately
 * @param status pointer to uint32_t to store the wait status
 * @param timestamp nanoseconds timestamp recorded once cs is completed
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_wait_for_cs_with_timestamp(int fd, uint64_t seq,
					uint64_t timeout_us, uint32_t *status,
					uint64_t *timestamp);

/**
 * This function waits until at least one command submission from a sequence
 * of command submissions of a specific device has finished executing
 * @param fd file descriptor handle of habanalabs main device
 * @param in pointer to a wait for multi CS input structure
 * @param out pointer to a wait for multi CS output structure
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_wait_for_multi_cs(int fd,
					struct hlthunk_wait_multi_cs_in *in,
					struct hlthunk_wait_multi_cs_out *out);

/**
 * This function waits until at least one command submission from a sequence
 * of command submissions of a specific device has finished executing
 * @param fd file descriptor handle of habanalabs main device
 * @param in pointer to a wait for multi CS input structure
 * @param out pointer to a wait for multi CS output structure
 * @param timestamp nanoseconds timestamp of the first CS to be completed. set
 * to zero if the timestamp cannot be determined.
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_wait_for_multi_cs_with_timestamp(int fd,
					struct hlthunk_wait_multi_cs_in *in,
					struct hlthunk_wait_multi_cs_out *out,
					uint64_t *timestamp);

/**
 * This function waits until an interrupt occurs and target value is greater or
 * equal than the content of a given user address
 * @param fd file descriptor handle of habanalabs main device
 * @param addr user address for target value comparison
 * @param target_value target value for comparison
 * @param interrupt_id interrupt id to wait for, set to all 1s in order to
 * register to all user interrupts
 * @param timeout_us absolute timeout to wait in microseconds. If the timeout
 * value is 0, the driver won't sleep at all. It will perform the comparison
 * without waiting for the interrupt to expire and will return immediately
 * @param status pointer to uint32_t to store the wait status
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_wait_for_interrupt(int fd, void *addr,
					uint64_t target_value,
					uint32_t interrupt_id,
					uint64_t timeout_us,
					uint32_t *status);

/**
 * This function waits until an interrupt occurs and target value is greater or
 * equal than the content of a given user address
 * @param fd file descriptor handle of habanalabs main device
 * @param cq_counters_handle cb handle of the cq counters
 * @param cq_counters_offset offset from the cq_counters_handle
 * @param target_value target value for comparison
 * @param interrupt_id interrupt id to wait for, set to all 1s in order to
 * register to all user interrupts
 * @param timeout_us absolute timeout to wait in microseconds. If the timeout
 * value is 0, the driver won't sleep at all. It will perform the comparison
 * without waiting for the interrupt to expire and will return immediately
 * @param status pointer to uint32_t to store the wait status
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_wait_for_interrupt_by_handle(int fd,
					uint64_t cq_counters_handle,
					uint64_t cq_counters_offset,
					uint64_t target_value,
					uint32_t interrupt_id,
					uint64_t timeout_us,
					uint32_t *status);

/**
 * This wait registers to get a timestamp when
 * an interrupt occurs and target value is greater or equal
 * than the content of a given user address.
 * timestamp 0 means that the interrupt didn't occur yet (target wasn't reached).
 * @param fd file descriptor handle of habanalabs main device
 * @param addr user address for target value comparison
 * @param target_value target value for comparison
 * @param interrupt_id interrupt id to wait for, set to all 1s in order to
 * register to all user interrupts
 * @param timeout_us absolute timeout to wait in microseconds. If the timeout
 * value is 0, the driver won't sleep at all. It will perform the comparison
 * without waiting for the interrupt to expire and will return immediately
 * @param status pointer to uint32_t to store the wait status
 * @param timestamp system timestamp in nanoseconds at time of interrupt.
 * 0, the interrupt wasn't triggered yet (CQ target wasn't reached).
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_wait_for_interrupt_with_timestamp(int fd,
					void *addr,
					uint64_t target_value,
					uint32_t interrupt_id,
					uint64_t timeout_us,
					uint32_t *status,
					uint64_t *timestamp);

/**
 * This wait registers to get a timestamp when
 * an interrupt occurs and target value is greater or equal
 * than the content of a given user address.
 * timestamp 0 means that the interrupt didn't occur yet (target wasn't reached).
 * @param fd file descriptor handle of habanalabs main device
 * @param cq_counters_handle cb handle of the cq counters
 * @param cq_counters_offset offset from the cq_counters_handle
 * @param target_value target value for comparison
 * @param interrupt_id interrupt id to wait for, set to all 1s in order to
 * register to all user interrupts
 * @param timeout_us absolute timeout to wait in microseconds. If the timeout
 * value is 0, the driver won't sleep at all. It will perform the comparison
 * without waiting for the interrupt to expire and will return immediately
 * @param status pointer to uint32_t to store the wait status
 * @param timestamp system timestamp in nanoseconds at time of interrupt.
 * 0, the interrupt wasn't triggered yet (CQ target wasn't reached).
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_wait_for_interrupt_by_handle_with_timestamp(int fd,
					uint64_t cq_counters_handle,
					uint64_t cq_counters_offset,
					uint64_t target_value,
					uint32_t interrupt_id,
					uint64_t timeout_us,
					uint32_t *status,
					uint64_t *timestamp);

/**
 * This function submits a job of a signal CS to a specific device
 * @param fd file descriptor handle of habanalabs main device
 * @param in pointer to a signal command submission input structure
 * @param out pointer to a  signal command submission output structure
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_signal_submission(int fd,
					struct hlthunk_signal_in *in,
					struct hlthunk_signal_out *out);

/**
 * This function submits a job of a signal CS to a specific device with timeout
 * @param fd file descriptor handle of habanalabs main device
 * @param in pointer to a signal command submission input structure
 * @param out pointer to a  signal command submission output structure
 * @param timeout duration in seconds
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_signal_submission_timeout(int fd,
					struct hlthunk_signal_in *in,
					struct hlthunk_signal_out *out,
					uint32_t timeout);

/**
 * This function submits a job of a  wait CS to a specific device
 * @param fd file descriptor handle of habanalabs main device
 * @param in pointer to a wait command submission input structure
 * @param out pointer to a wait command submission output structure
 * @return 0 for success, negative value for failure. ULLONG_MAX is returned if
 * the given signal CS was already completed. Undefined behavior if the given
 * seq is not of a  signal CS
 */
hlthunk_public int hlthunk_wait_for_signal(int fd,
					struct hlthunk_wait_in *in,
					struct hlthunk_wait_out *out);

/**
 * This function submits a job of a  wait CS to a specific device with a timeout
 * @param fd file descriptor handle of habanalabs main device
 * @param in pointer to a wait command submission input structure
 * @param out pointer to a wait command submission output structure
 * @param timeout duration in seconds
 * @return 0 for success, negative value for failure. ULLONG_MAX is returned if
 * the given signal CS was already completed. Undefined behavior if the given
 * seq is not of a  signal CS
 */
hlthunk_public int hlthunk_wait_for_signal_timeout(int fd,
					struct hlthunk_wait_in *in,
					struct hlthunk_wait_out *out,
					uint32_t timeout);

/**
 * This function submits a job of a  wait CS to a specific device
 * @param fd file descriptor handle of habanalabs main device
 * @param in pointer to a wait command submission input structure
 * @param out pointer to a wait command submission output structure
 * @return 0 for success, negative value for failure. ULLONG_MAX is returned if
 * the given signal CS was already completed. Undefined behavior if the given
 * seq is not of a  signal CS
 */
hlthunk_public int hlthunk_wait_for_collective_signal(int fd,
					struct hlthunk_wait_in *in,
					struct hlthunk_wait_out *out);

/**
 * This function submits a job of a  wait CS to a specific device with a timeout
 * @param fd file descriptor handle of habanalabs main device
 * @param in pointer to a wait command submission input structure
 * @param out pointer to a wait command submission output structure
 * @param timeout duration in seconds
 * @return 0 for success, negative value for failure. ULLONG_MAX is returned if
 * the given signal CS was already completed. Undefined behavior if the given
 * seq is not of a  signal CS
 */
hlthunk_public int hlthunk_wait_for_collective_signal_timeout(int fd,
					struct hlthunk_wait_in *in,
					struct hlthunk_wait_out *out,
					uint32_t timeout);

/**
 * This function set supported device memory allocation page orders
 * @param fd file descriptor of the device on which to perform the query
 * @param page_order_bitmask bitmask of supported allocation page orders
 * @return 0 on success, otherwise non 0 error code.
 *
 * note that on ASICs that does not support multiple page sizes of device memory the
 * function will set page_order_bitmask to be 0.
 */
hlthunk_public int hlthunk_get_dev_memalloc_page_orders(int fd, uint64_t *page_order_bitmask);

/**
 * This function allocates DRAM memory on the device
 * @param fd file descriptor of the device on which to allocate the memory
 * @param size how much memory to allocate
 * @param page_size what page size to use in the allocation. 0 means using the default size.
 * @param contiguous whether the memory area will be physically contiguous
 * @param shared whether this memory can be shared with other user processes
 * on the device
 * @return opaque handle representing the memory allocation. 0 is returned
 * upon failure
 */
hlthunk_public uint64_t hlthunk_device_memory_alloc(int fd, uint64_t size, uint64_t page_size,
						bool contiguous, bool shared);

/**
 * This function frees DRAM memory that was allocated on the device using
 * hlthunk_device_memory_alloc
 * @param fd file descriptor of the device that this memory belongs to
 * @param handle the opaque handle that represents this memory
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_device_memory_free(int fd, uint64_t handle);

/**
 * This function asks the driver to map a previously allocated DRAM memory
 * to the device's MMU and to allocate for it a VA in the device address space
 * @param fd file descriptor of the device that this memory belongs to
 * @param handle the opaque handle that represents this memory
 * @param hint_addr the user can request from the driver that the VA will be
 * a specific address. The driver doesn't have to comply to this request but
 * will take it under consideration
 * @return VA in the device address space. 0 is returned upon failure
 */
hlthunk_public uint64_t hlthunk_device_memory_map(int fd, uint64_t handle,
							uint64_t hint_addr);

/**
 * This function asks the driver to map a previously allocated host memory
 * to the device's MMU and to allocate for it a VA in the device address space
 * @param fd file descriptor of the device that this memory will be mapped to
 * @param host_virt_addr the user's VA of memory area on the host
 * @param hint_addr the user can request from the driver that the device VA will
 * be a specific address. The driver doesn't have to comply to this request but
 * will take it under consideration
 * @param host_size the size of the memory area
 * @return VA in the device address space. 0 is returned upon failure
 */
hlthunk_public uint64_t hlthunk_host_memory_map(int fd, void *host_virt_addr,
						uint64_t hint_addr,
						uint64_t host_size);

/**
 * This function asks the driver to map a previously allocated host memory
 * to the device's MMU and to allocate for it a VA in the device address space.
 * In this variation the function allows to specify custom flags for memory map.
 * @param fd file descriptor of the device that this memory will be mapped to
 * @param host_virt_addr the user's VA of memory area on the host
 * @param hint_addr the user can request from the driver that the device VA will
 * be a specific address. The driver doesn't have to comply to this request but
 * will take it under consideration
 * @param host_size the size of the memory area
 * @param flags custom flags for the memory map
 * @return VA in the device address space. 0 is returned upon failure
 */
hlthunk_public uint64_t hlthunk_host_memory_map_flags(int fd, void *host_virt_addr,
						uint64_t hint_addr,
						uint64_t host_size,
						uint32_t flags);

/**
 * This function unmaps a mapping in the device's MMU that was previously done
 * using either hlthunk_device_memory_map or hlthunk_host_memory_map
 * @param fd file descriptor of the device that contains the mapping
 * @param device_virt_addr the VA in the device address space representing
 * the device or host memory area
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_memory_unmap(int fd, uint64_t device_virt_addr);

/**
 * This function asks the driver to create a DMA-BUF object that will represent
 * an existing memory allocation inside the device memory. The function will
 * return a FD that will represent that DMA-BUF object. The application should
 * pass that FD to the importer driver. This function shall be used only for
 * Gaudi1
 * @param fd file descriptor of the device that this memory belongs to
 * @param addr physical address inside the device's DRAM.
 * @param size holds the size of the memory that the user wants to create a
 *             dma-buf that will describe it.
 * @param flags DMA-BUF file/FD flags. For now this parameter is not used.
 * @return file descriptor (positive value). negative value for failure
 */
hlthunk_public int hlthunk_device_memory_export_dmabuf_fd(int fd,
							uint64_t addr,
							uint64_t size,
							uint32_t flags);

/**
 * This function does the same as hlthunk_device_memory_export_dmabuf_fd but
 * with different interface for the application. this function shall be used
 * only for Gaudi2 and later ASICs
 * @param fd file descriptor of the device that this memory belongs to
 * @param addr a device virtual address that represents the start address of
 *             a mapped DRAM memory area inside the device. the address must
 *             be the same as was received from the driver during a previous
 *             HL_MEM_OP_MAP operation.
 * @param size holds the size of the memory that the user wants to create a
 *             dma-buf that will describe it.
 * @param offset an offset inside of the memory area describe by addr. The
 *               offset represents the start address of that the exported
 *               dma-buf object describes.
 * @param flags DMA-BUF file/FD flags. For now this parameter is not used.
 * @return file descriptor (positive value). negative value for failure
 */
hlthunk_public int hlthunk_device_mapped_memory_export_dmabuf_fd(int fd,
							uint64_t addr,
							uint64_t size,
							uint64_t offset,
							uint32_t flags);

/**
 * This function retrieves a HW block handle according to a given address
 * @param fd file descriptor of the device on which to allocate the memory
 * @param block_address HW block address (configuration space only)
 * @param block_size pointer to HW block size. The driver fills this value and
 * the user needs to pass it when calling mmap
 * @param handle pointer to handle that needs to be passed to mmap in order to
 * map the HW block to a VA in the process VA range
 * @return 0 on success or negative value in case of error
 */
hlthunk_public int hlthunk_get_hw_block(int fd, uint64_t block_address,
				uint32_t *block_size, uint64_t *handle);

/**
 * This function enables and retrieves debug traces of a specific device
 * @param fd file descriptor handle of habanalabs main device
 * @param debug pointer to debug parameters structure
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_debug(int fd, struct hl_debug_args *debug);

/**
 * This function allocates connection ID for a specific port of a specific
 * device
 * @param fd file descriptor handle of habanalabs main device
 * @param port port number
 * @param conn_id pointer to uint32_t to store the connection ID
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_alloc_conn(int fd, uint32_t port, uint32_t *conn_id);

/**
 * This function allocates collective connection ID for a specific device
 * @param fd file descriptor handle of habanalabs main device
 * @param in pointer to alloc collective connection input structure
 * @param conn_id pointer to uint32_t to store the collective connection ID
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_alloc_coll_conn(int fd, struct hlthunk_nic_alloc_coll_conn_in *in,
						uint32_t *conn_id);

/**
 * This function sets up a requester connection context for a specific port of a
 * specific device
 * @param fd file descriptor handle of habanalabs main device
 * @param port port number
 * @param conn_id connection ID
 * @param in pointer to requester context input structure
 * @param out pointer to requester context output structure
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_set_requester_conn_ctx(int fd, uint32_t port,
				uint32_t conn_id,
				const struct hlthunk_requester_conn_ctx *in,
				struct hlthunk_requester_conn_ctx_out *out);

/**
 * This function sets up a responder connection context for a specific port of a
 * specific device
 * @param fd file descriptor handle of habanalabs main device
 * @param port port number
 * @param conn_id connection ID
 * @param ctx pointer to responder context structure
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_set_responder_conn_ctx(int fd, uint32_t port,
				uint32_t conn_id,
				const struct hlthunk_responder_conn_ctx *ctx);

/**
 * This function destroys connection ID for a specific port of a specific device
 * @param fd file descriptor handle of habanalabs main device
 * @param port port number
 * @param conn_id connection ID
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_destroy_conn(int fd, uint32_t port,
					uint32_t conn_id);

/**
 * This function creates a NIC CQ on a specific device
 * @param fd file descriptor of the device with the NIC CQ to create
 * @param num_of_entries number of entries in the CQ buffer
 * @param handle pointer to the returned handle of NIC CQ buffer
 * @return 0 on success or negative value in case of error
 */
hlthunk_public int hlthunk_nic_cq_create(int fd, uint32_t num_of_entries,
					uint64_t *handle);

/**
 * This function destroys a NIC CQ on a specific device
 * @param fd file descriptor of the device with NIC CQ to destroy
 * @param handle identifier handle of NIC CQ
 * @return 0 on success or negative value in case of error
 */
hlthunk_public int hlthunk_nic_cq_destroy(int fd, uint64_t handle);

/**
 * This function waits on the NIC CQ until there are available CQEs
 * @param fd file descriptor of the device with the NIC CQ to wait on
 * @param handle identifier handle of NIC CQ
 * @param status current status of the NIC CQ
 * @param pi pointer to the returned producer index to return, meaning the first
 * available CQE
 * @param num_of_cqes pointer to the returned number of available CQEs to
 * consume starting from the CQE at pi
 * @param timeout_us timeout for waiting on the CQ in microseconds
 * @return 0 on success or negative value in case of error
 */
hlthunk_public int hlthunk_nic_cq_wait(int fd, uint64_t handle,
					uint32_t *status, uint32_t *pi,
					uint32_t *num_of_cqes,
					uint64_t timeout_us);

/**
 * This function updates the number of consumed CQEs by the user
 * @param fd file descriptor of the device with the NIC CQ to update
 * @param handle identifier handle of NIC CQ
 * @param num_of_consumed_entries number of consumed CQEs by the user
 * @return 0 on success or negative value in case of error
 */
hlthunk_public int hlthunk_nic_cq_update_consumed_entries(int fd,
					uint64_t handle,
					uint32_t num_of_consumed_entries);

/**
 * This function sets a NIC Congestion Control Completion Queue on a specific device
 * @param fd file descriptor of the device with the NIC CCQ to create
 * @param in input info for ccq creation
 * @param out output info for ccq mapping
 * @return 0 on success or negative value in case of error
 */
hlthunk_public int hlthunk_nic_user_ccq_set(int fd, struct hlthunk_nic_user_ccq_set_in *in,
		struct hlthunk_nic_user_ccq_set_out *out);

/**
 * This function unsets a NIC CCQ on a specific device
 * @param fd file descriptor of the device with NIC CCQ to destroy
 * @param port the port associated with this ccq
 * @return 0 on success or negative value in case of error
 */
hlthunk_public int hlthunk_nic_user_ccq_unset(int fd, uint32_t port);

/**
 * This function sets a NIC WQ array on a specific device
 * @param fd file descriptor of the device with the NIC WQ array to set
 * @param in pointer to WQ array set input structure
 * @param out pointer to WQ array set output structure
 * @return 0 on success or negative value in case of error
 */
hlthunk_public int hlthunk_nic_wq_arr_set(int fd,
					struct hlthunk_nic_wq_arr_set_in *in,
					struct hlthunk_nic_wq_arr_set_out *out);

/**
 * This function unsets a NIC WQ array on a specific device
 * @param fd file descriptor of the device with the NIC WQ array to unset
 * @param port the NIC port to unset the WQ array on
 * @param type sender/receiver
 * @return 0 on success or negative value in case of error
 */
hlthunk_public int hlthunk_nic_wq_arr_unset(int fd, uint32_t port,
					uint32_t type);

/**
 * This function sets a NIC user CQ on a specific device
 * @param fd file descriptor of the device with the NIC user CQ to set
 * @param port the NIC port to set the user CQ on
 * @param addr the base virtual address of the user CQ to set
 * @param num_of_cqes number of CQ entries for each user CQ
 * @return 0 on success or negative value in case of error
 */
hlthunk_public int hlthunk_nic_user_cq_set(int fd, uint32_t port, uint64_t addr,
					uint32_t num_of_cqes);

/**
 * This function unsets a NIC user CQ on a specific device
 * @param fd file descriptor of the device with the NIC user CQ to unset
 * @param port the NIC port to unset the user CQ on
 * @return 0 on success or negative value in case of error
 */
hlthunk_public int hlthunk_nic_user_cq_unset(int fd, uint32_t port);

/**
 * This function updates the user CQ consumer index of the buffer
 * @param fd file descriptor of the device with the NIC user CQ to update
 * @param num_of_consumed_entries number of consumed CQEs by the user
 * @return 0 on success or negative value in case of error
 */
hlthunk_public int hlthunk_nic_user_cq_update_ci(int fd, uint32_t port, uint32_t ci);

/**
 * This function allocates a NIC user CQ on a specific device
 * @param fd file descriptor of the device with the NIC user CQ to allocate
 * @param in pointer to user CQ alloc input structure
 * @param out pointer to user CQ alloc output structure
 * @return 0 on success or negative value in case of error
 */
hlthunk_public int hlthunk_nic_user_cq_id_alloc(int fd, struct hlthunk_nic_user_cq_id_alloc_in *in,
						struct hlthunk_nic_user_cq_id_alloc_out *out);

/**
 * This function sets a NIC user CQ on a specific device
 * @param fd file descriptor of the device with the NIC user CQ to set
 * @param in pointer to user CQ set input structure
 * @param out pointer to user CQ set output structure
 * @return 0 on success or negative value in case of error
 */
hlthunk_public int hlthunk_nic_user_cq_id_set(int fd, struct hlthunk_nic_user_cq_id_set_in *in,
						struct hlthunk_nic_user_cq_id_set_out *out);

/**
 * This function unsets a NIC user CQ on a specific device
 * @param fd file descriptor of the device with the NIC user CQ to unset
 * @param in pointer to user CQ unset input structure
 * @return 0 on success or negative value in case of error
 */
hlthunk_public int hlthunk_nic_user_cq_id_unset(int fd, struct hlthunk_nic_user_cq_id_unset_in *in);

/**
 * This function checks if the port its going to use has advanced features.
 * @param fd file descriptor of the device used by the application
 * @param port the port this application is associated with
 * @return 0 application and port support advanced features
 *  non-zero application and/or port do not support advanced features.
 */
hlthunk_public int hlthunk_nic_user_set_app_params(
		int fd, uint32_t port,
		const struct hlthunk_nic_user_set_app_params_in *in);

/**
 * This function retrieve port resources and configurations.
 * @param fd file descriptor of the device that is used by the application
 * @param port the port for which to get the params
 * @param out holds the nic's retrieved resources and configurations.
 * @return 0 application and port support advanced features
 *  non-zero application and/or port do not support advanced features.
 */
hlthunk_public int hlthunk_nic_user_get_app_params(
		int fd, uint32_t port,
		struct hlthunk_nic_user_get_app_params_out *out);

/**
 * This function reads a single event from the driver-maintained event queue
 * associated with the given fd.
 * @param fd file descriptor of the device that is used by the application
 * @param port the port for which to get the event from
 * @param out holds the nic's retrieved event.
 * @return 0 successfully accessed the associated event-queue and retrieved
 *  a valid entry, negative value in case of an error or if the queue is empty.
 */
hlthunk_public int hlthunk_nic_eq_poll(
				int fd, uint32_t port,
				struct hlthunk_nic_eq_poll_out *out);

/**
 * This function allocates doorbell fifo ID
 * @param fd file descriptor of the device
 * @param port NIC port we are working with
 * @param id allocated db fifo ID
 * @return 0 successfully allocated db fifo ID
 *  non-zero failed to allocate db fifo ID
 */
hlthunk_public int
hlthunk_nic_alloc_user_db_fifo(int fd, uint32_t port, uint32_t *id);

/**
 * This function configures doorbell fifo
 * @param fd file descriptor of the device
 * @param in struct parameter with the needed info for setting db fifo
 * @param out struct parameter with the needed info for mapping db fifo
 * @return 0 successfully configured db fifo
 *  non-zero failed to configure db fifo
 */
hlthunk_public int
hlthunk_nic_user_db_fifo_set(int fd, struct hlthunk_nic_user_db_fifo_set_in *in,
		struct hlthunk_nic_user_db_fifo_set_out *out);

/**
 * This function destroys doorbell fifo.
 * @param fd file descriptor of the device.
 * @param port NIC port we are working with.
 * @param id allocated db fifo ID
 * @return 0 successfully destroyed db fifo
 *  non-zero failed to destroy db fifo
 */
hlthunk_public int
hlthunk_nic_user_db_fifo_unset(int fd, uint32_t port, uint32_t id);

/**
 * This function retrieves the internal port PCS link up/down status
 * @param fd file descriptor of the device with the Habana link port to probe
 * @param in struct parameter with the needed info for getting the link state
 * @param out struct parameter with info of the result of getting the link state
 * @return 0 on success or negative value in case of error
 */
hlthunk_public int hlthunk_get_habana_link_state(int fd,
						struct hlthunk_get_habana_link_state_in *in,
						struct hlthunk_get_habana_link_state_out *out);

/**
 * This function retrieves the internal port statistics
 * @param fd file descriptor of the device with the Habana link port to probe
 * @param in struct parameter with the needed info for getting the statistics
 * @param out struct parameter with info of the result of getting the statistics
 * @return 0 on success or negative value in case of error
 */
hlthunk_public int hlthunk_get_habana_link_statistics(int fd,
						struct hlthunk_get_habana_link_stat_in *in,
						struct hlthunk_get_habana_link_stat_out *out);

/**
 * This function allocates NIC encapsulation ID.
 * @param fd file descriptor of the device that is used by the application.
 * @param port NIC port for which to get the params.
 * @param encap_id holds allocated encapsulation ID.
 * @return 0 if success. Non-zero for any error.
 */
hlthunk_public int hlthunk_nic_user_encap_alloc(
		int fd, uint32_t port, uint32_t *encap_id);

/**
 * This function configures NIC HW to start encapsulation.
 * @param fd file descriptor of the device that is used by the application.
 * @param encap_cfg holds encapsulation parameters.
 * @return 0 if success. Non-zero for any error.
 */
hlthunk_public int hlthunk_nic_user_encap_set(
		int fd, const struct hlthunk_encap_cfg *encap_cfg);

/**
 * This function de-allocates ID and stops encapsulation.
 * @param fd file descriptor of the device that is used by the application.
 * @param port NIC port for which to get the params.
 * @param encap_id holds encapsulation ID to be de-allocated.
 * @return 0 if success. Non-zero for any error.
 */
hlthunk_public int hlthunk_nic_user_encap_unset(
		int fd, uint32_t port, uint32_t encap_id);

/**
 * This function retrieves QPC dump for a given QP of a given NIC port.
 * @param fd file descriptor handle of habanalabs main device.
 * @param port NIC port which holds that QP.
 * @param qpn QP number.
 * @param req true for requester dump, false for responder dump.
 * @param buf buffer to hold the dump.
 * @param buf_size size of the buffer to hold the dump.
 * @return 0 if success. Non-zero for any error.
 */
hlthunk_public int hlthunk_nic_dump_qp(int fd, uint32_t port, uint32_t qpn, uint32_t req,
					char *buf, uint32_t buf_size);

/**
 * This function retrieves the NIC ports enabled ports masks. This function is common for all ASICs.
 * @param fd file descriptor handle of habanalabs main device.
 * @param mask returned masks.
 * @return 0 if success. Non-zero for any error.
 */
hlthunk_public int hlthunk_nic_get_enabled_ports_mask(int fd, uint64_t *mask);

/**
 * This function retrieves the NIC ports and external ports masks. This function shall be used
 * only for Gaudi2 and later ASICs.
 * @param fd file descriptor handle of habanalabs main device.
 * @param out struct parameter with returned masks.
 * @return 0 if success. Non-zero for any error.
 */
hlthunk_public int hlthunk_nic_get_ports_masks(int fd, struct hlthunk_nic_get_ports_masks_out *out);

/**
 * This function retrieves information of recorded events.
 * @param fd file descriptor of the device that is used by the application.
 * @param event_id event id to retrieve its data.
 * @param buf buffer that holds retrieved data of requested event id.
 * @return 0 if success. Non-zero for any error. In case no data was available, function will
 *  return 0 and the timestamp will not be changed. This can happen in the following
 *  events: HLTHUNK_RAZWI_EVENT, HLTHUNK_UNDEFINED_OPCODE, HLTHUNK_HW_ERR_OPCODE and
 *  HLTHUNK_FW_ERR_OPCODE.
 */
hlthunk_public int hlthunk_get_event_record(int fd,
		enum hlthunk_event_record_id event_id, void *buf);

hlthunk_public void *hlthunk_malloc(size_t size);
hlthunk_public void hlthunk_free(void *pt);

/**
 * This function returns a pointer to a char array containing the version. The
 * char array should be freed using hlthunk_free
 * @return valid pointer on success or null in case of error
 */
hlthunk_public char *hlthunk_get_version(void);

/* Functions for hash table implementation */

hlthunk_public void *hlthunk_hash_create(void);
hlthunk_public int hlthunk_hash_destroy(void *t);
hlthunk_public int hlthunk_hash_lookup(void *t, unsigned long key,
					void **value);
hlthunk_public int hlthunk_hash_insert(void *t, unsigned long key, void *value);
hlthunk_public int hlthunk_hash_delete(void *t, unsigned long key);
hlthunk_public int hlthunk_hash_next(void *t, unsigned long *key, void **value);
hlthunk_public int hlthunk_hash_first(void *t, unsigned long *key,
					void **value);


/**
 * This function starts an API controlled profiling session on a device
 * @param fd file descriptor of the device to start profiling on
 * @return 0 on success or negative value in case of error
 */
hlthunk_public int hlthunk_profiler_start(int fd);

/**
 * This function stops an API controlled profiling session on a device
 * @param fd file descriptor of the device to stop profiling on
 * @return 0 on success or negative value in case of error
 */
hlthunk_public int hlthunk_profiler_stop(int fd);

/**
 * This function retrieves the profiler trace created in an API controlled
 * profiling session. This function should be called first with buffer = null
 * in order to get the trace size, allocate the buffer according to this size
 * and call again with the buffer allocated
 * @param fd file descriptor of the device to get the trace from
 * @param buffer a buffer to copy to trace to, if buffer = null then only
 * retrieves the trace size and amount of entries.
 * The ruturned buffer is built in the following format:
 * [synTraceEvent enries][chars][size_t num][size_t version]
 * num: Amount of synTraceEvent entries
 * version: Synprof parser version
 * @param size out param for the amount of bytes copied to buffer (or the trace
 * size if buffer = null)
 * @param num_entries pointer to the returned number of entries in the trace
 * buffer
 * @return 0 on success or negative value in case of error
 */
hlthunk_public int hlthunk_profiler_get_trace(int fd, void *buffer,
					uint64_t *size, uint64_t *num_entries);

/**
 * This function destroys profiler instance if it existed
 * As long as env var HABANA_PROFILE=1
 * or HABANA_PROFILE=<template_name>, the profiler will reinitialize
 * on the next hlthunk_open call
 */
hlthunk_public void hlthunk_profiler_destroy(void);

/**
 * This function opens the debug file system of the habanalabs device already
 * opened via the hlthunk_open routine.
 * @param fd The file descriptor of the device as returned from the call to
 * the hlthunk_open routine.
 * @param debugfs The debug-fs information filled by the open routine.
 * @return 0 upon success or negative value in case of error
 */
hlthunk_public int hlthunk_debugfs_open(int fd,
					struct hlthunk_debugfs *debugfs);

/**
 * Using debugfs, this function returns the 32bit value read from the
 * device address space at the specified address.
 * @param debugfs Pointer to the device debug-fs information.
 * @param full_address The 64 bit address in the device address space to read
 * the from
 * @param val The vslue read from the given address
 * @return 0 upon success or negative value in case of error
 */
hlthunk_public int hlthunk_debugfs_read(struct hlthunk_debugfs *debugfs,
					uint64_t full_address, uint32_t *val);

/**
 * Using debugfs, this function writes the 32bit value to the specified address
 * in the device address space
 * @param debugfs Pointer to the device debug-fs information.
 * @param full_address The address in the device address space to writ the value
 * to.
 * @param val The vale to write.
 * @return 0 upon success or negative value in case of error
 */
hlthunk_public int hlthunk_debugfs_write(struct hlthunk_debugfs *debugfs,
					 uint64_t full_address, uint32_t val);

/**
 * This function closes the open debug file system of the habanalabs device.
 * @param debugfs Pointer to the debug-fs information as filled by the
 * hlthunk_debugfs_open routine
 * @return 0 upon success or negative value in case of error
 */
hlthunk_public int hlthunk_debugfs_close(struct hlthunk_debugfs *debugfs);

/**
 * This function reserves a number of signals for a certain stream.
 * @param fd device descriptor
 * @param in operation inputs which contains the queue idx and the signals count
 * @param out output of the operation which is the handle and the status.
 * @return 0 upon success or negative value in case of error
 */
hlthunk_public int hlthunk_reserve_encaps_signals(int fd,
					struct hlthunk_sig_res_in *in,
					struct hlthunk_sig_res_out *out);

/**
 * This function releases the signals that were reserved in
 * hlthunk_reserve_encaps_signals
 * @param handle reservation handle obtained from hlthunk_reserve_encaps_signals
 * @param status as output of the operation, which is 0 in case driver succeeded
 * to unreserve, negative value otherwise.
 * @return 0 upon success or negative value in case of error
 */
hlthunk_public int hlthunk_unreserve_encaps_signals(int fd,
					struct reserve_sig_handle *handle,
					uint32_t *status);

/**
 * This function submits a set of jobs to a specific device as the first part
 * of a staged submission, which includes a set of signal cmds to a
 * reserved signals, which were reserved in hlthunk_reserve_encaps_signals.
 * @param fd file descriptor handle of habanalabs main device
 * @handle_id reserved signals handle id, obtained from
 *		hlthunk_reserve_encaps_signals.
 * @param in pointer to command submission input structure
 * @param out pointer to command submission output structure
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_staged_command_submission_encaps_signals(int fd,
					uint64_t handle_id,
					struct hlthunk_cs_in *in,
					struct hlthunk_cs_out *out);

/**
 * This function submits a job of a wait CS to a specific device
 * This will wait for a number of signals which were resereved before calling
 * this function. it can wait for the whole reserved signals or
 * just to a specific offset whithin that range.
 * @param fd file descriptor handle of habanalabs main device
 * @param in pointer to a wait command submission input structure
 * @param out pointer to a wait command submission output structure
 * @return 0 for success, negative value for failure. ULLONG_MAX is returned if
 * the given signal CS was already completed. Undefined behavior if the given
 * seq is not of a  signal CS
 */
hlthunk_public int hlthunk_wait_for_reserved_encaps_signals(int fd,
					struct hlthunk_wait_in *in,
					struct hlthunk_wait_out *out);

/**
 * This function submits a job of a collective wait CS to a specific device
 * This will wait for a number of signals which were resereved before calling
 * this function. it can wait for the whole reserved signals or
 * just for a specific offset whithin that range.
 * @param fd file descriptor handle of habanalabs main device
 * @param in pointer to a wait command submission input structure
 * @param out pointer to a wait command submission output structure
 * @return 0 for success, negative value for failure. ULLONG_MAX is returned if
 * the given signal CS was already completed. Undefined behavior if the given
 * seq is not of a  signal CS
 */
hlthunk_public int hlthunk_wait_for_reserved_encaps_collective_signals(int fd,
					struct hlthunk_wait_in *in,
					struct hlthunk_wait_out *out);

/**
 * This function retrieves the dram replaced rows info.
 * @param fd file descriptor handle of habanalabs main device
 * @param info pointer to memory, where to fill the replaced rows info
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_dram_replaced_rows_info(int fd,
			struct hlthunk_dram_replaced_rows_info *info);

/**
 * This function retrieves the dram pending rows number.
 * @param fd file descriptor handle of habanalabs main device
 * @param out pointer to memory, where to fill the pending rows number
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_dram_pending_rows_info(int fd, uint32_t *out);

/**
 * This function retrieves attestation report of the boot.
 * @param fd file descriptor handle of habanalabs main device
 * @param nonce number used to generate secured attestation report
 * @param info pointer to attestation report info output buffer
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_sec_attest_info(int fd, uint32_t nonce,
					struct hlthunk_sec_attest_info *info);

/**
 * This function retrieves page fault information.
 * @param fd file descriptor handle of habanalabs main device
 * @param pf holds all page fault info, including user mappings
 * @param num_of_allocated_mappings array size of user buffer holding user mappings
 * @return 0 for success, a negative value for failure. In case no data was available, function will
 * return 0 and the timestamp will not be changed.
 */
hlthunk_public int hlthunk_get_page_fault_info(int fd, struct hlthunk_page_fault_info *pf,
						uint32_t num_of_allocated_mappings);

/**
 * This function retrieve the device va of a command buffer previously allocated
 * in host kernel memory.
 * Note that the buffer should be created with CB_TYPE_KERNEL_MAPPED flag.
 * @param fd file descriptor handle of habanalabs main device
 * @param cb_handle command buffer handle
 * @param device_va device va address of the allocated cb
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_mapped_cb_device_va_by_handle(int fd,
							uint64_t cb_handle,
							uint64_t *device_va);

/**
 * This function flush all PCI HBW writes
 * @param fd file descriptor handle of habanalabs main device
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_cs_flush_pci_hbw_writes(int fd);

/**
 * This function registers a timestamp event of a specific user interrupt id.
 * when interrupt occurred the driver will compare the CQ pi value with the
 * target value, and if CQ reached that value it'll write the timestamp
 * into the specified timestamp offset.
 * - If the timestamp record is already registered for interrupt, that has not
 * been expired yet, the driver will unregister the node and will register it on the new interrupt.
 * - As part of the call, the timestamp handle will be set atomically to TS_NOT_EXP_VAL.
 * The value is then overridden only after the cq counter reaches its target value.
 * users can wait until the timestamp entry is different than TS_NOT_EXP_VAL
 * to conclude that the target value has been reached.
 * - When there are other pending events waiting for the selected interrupt ID,
 * the caller must ensure that the CQ entry (cq_counters_handle + cq_counters_offset)
 * is not registered on other interrupt ID.
 * In other words, each CQ entry can be set at most by a single interrupt ID.
 * Note that if the counter current value has already reach the target value, the call
 * will not wait for the interrupt to set the timestamp, and instead set the
 * timestamp value immediately.
 *
 * @param fd file descriptor handle of habanalabs main device
 * @param interrupt_id interrupt id to register for
 * @param cq_counters_handle a cb which have the CQs counters value
 * @param cq_counters_offset offset in the CQs counters cb
 * @param target_value target value for comparison
 * @param timestamps_handle buffer handle of timestamps
 * @param timestamps_offset offset in the timestamps buffer
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_register_timestamp_interrupt(int fd, uint32_t interrupt_id,
					uint64_t cq_counters_handle,
					uint64_t cq_counters_offset,
					uint64_t target_value,
					uint64_t timestamps_handle,
					uint64_t timestamps_offset);

/**
 * This function allocate buffer in host kernel memory for timestamps events pool.
 * the driver will allocate enough space to store all timestamps data needed
 * by the driver to register/unregister timestamp events
 * This is needed due to a requirement, that driver cannot fail on out-of-memory
 * at event registration phase.
 * Note that each element has the size of uint64_t.
 * The memory will be freed when the user closes the file descriptor(ctx close)
 * @param fd file descriptor handle of habanalabs main device
 * @param elements_num number of timestamps elements, each element is uint64_t size.
 * @param handle buffer handle output
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_allocate_timestamp_elements(int fd,
					uint32_t elements_num,
					uint64_t *handle);

/**
 * This function creates a notifier object that allows the user
 * to receive async notification events, from the Kernel driver.
 * @param fd file descriptor handle of habanalabs main device
 * @return notifier handle for success, a negative value for failure
 */
hlthunk_public int hlthunk_notifier_create(int fd);

/**
 * This function releases a notifier object. it shall be invoked
 * when the user is no longer needs to receive notification events
 * from the Kernel driver.
 * @param fd file descriptor handle of habanalabs main device
 * @param handle of the notifier object
 * @return 0 for success, a negative value for failure
 */
hlthunk_public int hlthunk_notifier_release(int fd, int handle);

/**
 * This function receives a notification event, that raises by the
 * Kernel driver. The function may block until an event is
 * received, or timeout expired. The function returns a bitmap value
 * that indicates, which event has occurred. Each function invocation
 * retrieves a new bitmap value, that indicates the last occurred events.
 * @param fd file descriptor handle of habanalabs main device
 * @param handle of the notifier object
 * @param notifier_events bitmap pointer. Each bit indicates a specific event.
 * @param notifier_cnt pointer to uint64_t, stores the notifier count.
 *  zero - indicates no notification - timeout expired.
 *  number greater from zero - indicates the notifications count since the last read.
 * @param flags of function's operations. Not used for now.
 * @param timeout in milliseconds. If the timeout value is 0, the function
 *  will return immediately, even if no notification was received.
 * @return 0 for success, a negative value for failure
 */
hlthunk_public int hlthunk_notifier_recv(int fd, int handle, uint64_t *notifier_events,
						uint64_t *notifier_cnt, uint32_t flags,
						uint32_t timeout);

/**
 * This function retrieves string containing engines status.
 * @param fd file descriptor handle of habanalabs main device
 * @param status_buf buffer for retrieved string
 * @param status_buf_size buffer size
 * @param actual_size actual size of data
 * @return 0 for success, a negative value for failure
 */
hlthunk_public int hlthunk_get_engine_status(int fd, char *status_buf, uint32_t status_buf_size,
						int *actual_size);

/**
 * This function set a run mode for all the cores that are supplied in core_ids array
 * @param fd file descriptor handle of habanalabs main device
 * @param core_ids a pointer to an array of cores numbers
 * @param num_cores actual number of core numbers in the core_ids array
 * @param mode the mode to be set, HL_ENGINE_CORE_RUN/HALT values.
 * @return 0 for success, a negative value for failure
 */
hlthunk_public int hlthunk_engine_cores_set_mode(int fd, const uint32_t *core_ids,
				uint32_t num_cores, uint32_t mode);

/**
 * This function applies a command for all the engines that are supplied in engine_ids array
 * @param fd file descriptor handle of habanalabs main device
 * @param engine_ids a pointer to an array of engine numbers
 * @param num_engines actual number of engine numbers in the engine_ids array
 * @param command the command to be set
 * @return 0 for success, a negative value for failure
 */
hlthunk_public int hlthunk_engines_command(int fd, const uint32_t *engine_ids,
				uint32_t num_engines, enum hl_engine_command command);

/**
 * This function returns the number of devices for the given device name.
 * @param device_name name of the device to count
 * @return number of devices for success, negative value for failure
 */
hlthunk_public int hlthunk_get_device_count(enum hlthunk_device_name device_name);

/**
 * This function reads a memory block from the device. The function is experimental which
 * means, it can be deprecated without providing backward compatibility and/or alternative
 * solution. Please take that into account when using this function.
 * @param fd file descriptor handle of habanalabs main device
 * @param buff buffer for retrieved the read memory
 * @param 64-bits block_address the device starting address to read from
 * @param block_size the request to read. max size - 16KB
 * @param flags indicates flags option for the read block opeartion. Not in used
 * @return 0 for success, a negative value for failure
 */
hlthunk_public int hlthunk_device_memory_read_block_experimental(int fd, uint32_t *buff,
				uint64_t block_address, uint32_t block_size, uint32_t flags);

hlthunk_public int hlthunk_deprecated_func1(int fd, uint64_t seq,
					uint64_t timeout_us, uint32_t *status,
					uint64_t *timestamp);

hlthunk_public int hlthunk_deprecated_func2(int fd, uint32_t interrupt_id,
						uint64_t cq_counters_handle,
						uint64_t cq_counters_offset,
						uint64_t target_value,
						uint64_t timestamp_handle,
						uint64_t timestamp_offset);

hlthunk_public int hlthunk_deprecated_func3(int fd, uint32_t num_elements, uint64_t *handle);

/**
 * This function used to send a generic request asking FW for some information.
 * The driver will verify the opcode of the command and send it to fw then copy
 * the output to the specified buffer.
 * @param fd file descriptor handle of habanalabs main device
 * @param buff buffer to be sent to the FW.
 *	Note that this buffer may serve for both input/output buffer, for cases
 *	where the command has also input data to be passed to fw.
 * @param buff_size size of the buffer.
 * @sub_opcode generic request sub-opcode.
 * @return 0 for success, a negative value for failure.
 */
hlthunk_public int hlthunk_fw_send_generic_request(int fd, void *buff, uint32_t buff_size,
							uint32_t sub_opcode);

/**
 * This function retrieves the signed device information.
 * @param fd file descriptor handle of habanalabs main device
 * @param nonce number used to generate signed device info report
 * @param info pointer to the signed device info output buffer
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_dev_info_signed(int fd, uint32_t nonce,
						struct hlthunk_dev_info_signed *info);
/**
 * This function retrieves the device's time alongside the host's time
 * on specified die for synchronization
 * @param fd file descriptor handle of habanalabs main device
 * @param die_index index for requested die must be assigned
 * @param info pointer to memory that will be filled by the function with the
 * device's and host's times
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_get_time_sync_per_die_info(int fd, uint32_t die_index,
					struct hlthunk_time_sync_info *info);

/**
 * This function reports the user’s memory consumption in bytes alongside timestamp in seconds.
 * @param fd file descriptor handle of habanalabs main device
 * @param used_mem_bytes device used memory in bytes
 * @param timestamp_sec time stamp in seconds
 * @return 0 for success, negative value for failure
 */
hlthunk_public int hlthunk_set_memory_consumption(int fd, uint64_t used_mem_bytes,
						  uint64_t timestamp_sec);

#ifdef __cplusplus
}   //extern "C"
#endif

#endif /* HLTHUNK_H */

/* SPDX-License-Identifier: MIT
 *
 * Copyright 2019-2022 HabanaLabs, Ltd.
 * All Rights Reserved.
 *
 */

#ifndef HLTHUNK_TESTS_H
#define HLTHUNK_TESTS_H

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

#ifndef HLTESTS_LIB_MODE
int hltests_run_group_tests(const char *group_name,
				const struct CMUnitTest * const tests,
				const size_t num_tests,
				CMFixtureFunction group_setup,
				CMFixtureFunction group_teardown);
#endif

int hltests_control_dev_open(const char *busid);
int hltests_control_dev_close(int fd);
int hltests_open(const char *busid);
int hltests_close(int fd);

void *hltests_cb_mmap(int fd, size_t len, off_t offset);
int hltests_cb_munmap(void *addr, size_t length);

uint32_t hltests_debugfs_read(int addr_fd, int data_fd, uint64_t full_address);
void hltests_debugfs_write(int addr_fd, int data_fd, uint64_t full_address,
				uint32_t val);
uint64_t hltests_debugfs_read64(int addr_fd, int data_fd, uint64_t full_address);
void hltests_debugfs_write64(int addr_fd, int data_fd, uint64_t full_address,
				uint64_t val);
struct hltests_memory *
hltests_allocate_host_mem_nomap(uint64_t size, enum hltests_huge huge);
int gaudi_free_host_mem_nounmap(struct hltests_memory *mem,
					enum hltests_huge huge);
int hltests_map_host_mem(int fd, struct hltests_memory *mem);
int hltests_unmap_host_mem(int fd, struct hltests_memory *mem);
void *hltests_allocate_host_mem(int fd, uint64_t size, enum hltests_huge huge);
void *hltests_allocate_host_mem_aligned(int fd, uint64_t size,
				enum hltests_huge huge, uint64_t align);
void *hltests_allocate_host_mem_aligned_flags(int fd, uint64_t size,
			enum hltests_huge huge, uint64_t align, uint32_t flags);
void *gaudi_allocate_device_mem(int fd, uint64_t size, uint64_t page_size,
				  enum hltests_contiguous contiguous);
const struct hltests_memory *
gaudi_allocate_device_mem_ret_mem(int fd, uint64_t size, uint64_t page_size,
				    enum hltests_contiguous contiguous);
int gaudi_allocate_device_mem_mix_page_size(int fd, uint64_t size,
		uint64_t *device_addr_arr, int device_addr_arr_size, int *actual_arr_size);
int gaudi_free_device_mem_mix_page_size(int fd, uint64_t *device_addr_arr, int size);

int gaudi_free_host_mem(int fd, void *vaddr);
int gaudi_free_device_mem(int fd, void *vaddr);
uint64_t gaudi_get_device_va_for_host_ptr(int fd, void *vaddr);
const struct hltests_memory *hltests_get_mem_for_device_va(int fd, void *device_va);
uint64_t hltests_get_device_handle_for_device_va(int fd, void *device_va);

void *hltests_create_cb(int fd, uint32_t cb_size, enum hltests_cb_type cb_type,
			uint64_t cb_internal_sram_address);
int hltests_destroy_cb(int fd, void *ptr);
uint32_t hltests_add_packet_to_cb(void *ptr, uint32_t offset, void *pkt,
					uint32_t pkt_size);
int hltests_get_cb_usage_count(int fd, void *ptr, uint32_t *usage_cnt);

int hltests_fill_cs_chunk(struct hltests_device *hdev,
				struct hl_cs_chunk *chunk,
				void *cb_ptr,
				uint32_t cb_size,
				uint32_t queue_index);
int hltests_submit_cb(int fd,
				void *cb_ptr,
				uint32_t cb_size,
				uint32_t queue_index,
				uint32_t flags,
				uint64_t *seq);
int hltests_submit_cs(int fd, struct hltests_cs_chunk *restore_arr,
				uint32_t restore_arr_size,
				struct hltests_cs_chunk *execute_arr,
				uint32_t execute_arr_size,
				uint32_t flags,
				uint64_t *seq);
int hltests_submit_cs_timeout(int fd, struct hltests_cs_chunk *restore_arr,
				uint32_t restore_arr_size,
				struct hltests_cs_chunk *execute_arr,
				uint32_t execute_arr_size,
				uint32_t flags,
				uint32_t timeout_sec,
				uint64_t *seq);
int hltests_submit_legacy_cs(int fd, struct hltests_cs_chunk *restore_arr,
				uint32_t restore_arr_size,
				struct hltests_cs_chunk *execute_arr,
				uint32_t execute_arr_size,
				uint32_t flags, uint32_t timeout,
				uint64_t *seq);
int hltests_submit_staged_cs(int fd, struct hltests_cs_chunk *restore_arr,
				uint32_t restore_arr_size,
				struct hltests_cs_chunk *execute_arr,
				uint32_t execute_arr_size,
				uint32_t flags,
				uint64_t staged_cs_seq,
				uint64_t *seq);
int hltests_sched_arc_send_cb(int fd, struct hltests_cs_chunk *hltests_chunk);
int hltests_sched_arc_submit_cs(int fd, struct hltests_cs_chunk *arr,
				uint32_t arr_size, uint32_t sched_id,
				uint64_t *seq);
int hltests_wait_for_cs(int fd, uint64_t seq, uint64_t timeout_us);
int hltests_wait_for_legacy_cs(int fd, uint64_t seq, uint64_t timeout_us);
int hltests_wait_for_legacy_cs_until_not_busy(int fd, uint64_t seq);
int hltests_wait_for_cs_until_not_busy(int fd, uint64_t seq);
int hltests_wait_for_interrupt(int fd, void *addr, uint32_t target_value,
				uint32_t interrupt_id, uint64_t timeout_us);
int hltests_wait_for_interrupt_until_not_busy(int fd, void *addr,
				uint32_t target_value, uint32_t interrupt_id);
int hltests_wait_for_interrupt_by_handle(int fd, uint64_t cq_counters_handle,
				uint64_t cq_counters_offset, uint32_t target_value,
				uint32_t interrupt_id, uint64_t timeout_us);
int hltests_wait_for_interrupt_by_handle_until_not_busy(int fd, uint64_t cq_counters_handle,
				uint64_t cq_counters_offset, uint32_t target_value,
				uint32_t interrupt_id);

int hltests_submit_and_wait_cs(int fd, void *cb_ptr, uint32_t cb_size,
				uint32_t queue_index,
				enum hltests_destroy_cb destroy_cb,
				int expected_val);

int hltests_submit_and_wait_legacy_cs(int fd, void *cb_ptr, uint32_t cb_size,
				uint32_t queue_index,
				enum hltests_destroy_cb destroy_cb,
				int expected_val);

int hltests_control_dev_setup(void **state);
int hltests_control_dev_teardown(void **state);
int hltests_setup(void **state);
int hltests_setup_user_engines(struct hltests_state *tests_state);
int hltests_teardown(void **state);
int hltests_teardown_user_engines(struct hltests_state *tests_state);
int hltests_root_debug_setup(void **state);
int hltests_root_debug_teardown(void **state);

int hltests_get_module_params_info(int fd,
				struct hltests_module_params_info *info);

void hltests_set_rand_seed(uint32_t val);
uint32_t hltests_rand_u32(void);
bool hltests_rand_flip_coin(void);
void hltests_fill_rand_values(void *ptr, uint64_t size);
void hltests_fill_addr_chunk(void *ptr, uint32_t size, uint64_t addr, uint64_t chunk);
void hltests_fill_seq_values(void *ptr, uint32_t size);
void hltests_endian_swap_values(void *ptr, uint32_t size,
				enum hltests_endian_swap endian_swap);

int hltests_mem_compare_with_stop(void *ptr1, void *ptr2, uint64_t size, bool
			stop_on_err, uint64_t *offset);
int hltests_mem_compare(void *ptr1, void *ptr2, uint64_t size);
int hltests_mem_compare_offset(void *ptr1, void *ptr2, uint64_t size, uint64_t *offset);

int gaudi_dma_transfer(int fd, uint32_t queue_index, enum hltests_eb eb,
				enum hltests_mb mb,
				uint64_t src_addr, uint64_t dst_addr,
				uint32_t size,
				enum hltests_dma_direction dma_dir);

int hltests_zero_dram_memory(int fd, uint64_t dst_addr, uint32_t size);

int gaudi_dma_transfer_legacy(int fd, uint32_t queue_index,
				enum hltests_eb eb, enum hltests_mb mb,
				uint64_t src_addr, uint64_t dst_addr,
				uint32_t size,
				enum hltests_dma_direction dma_dir);

VOID hltests_dma_sram_test(void **state, uint64_t size);
VOID hltests_dma_dram_test(void **state, uint64_t size);
VOID hltests_dma_test_flags(void **state, bool is_ddr, uint64_t size, uint64_t page_size,
				uint32_t flags);

VOID hltests_dma_dram_frag_mem_test(void **state, uint64_t size);

VOID hltests_dma_dram_high_mem_test(void **state, uint64_t size);

VOID hltests_tdr_deadlock_test(void **state, bool recovery);

int hltests_mmu_hint_address(int fd, uint64_t page_size, uint64_t ref_addr,
			     enum range_type type, bool page_aligned);

int hltests_ensure_device_operational(void **state);

VOID test_sm_pingpong_common_cp(void **state, bool is_tpc,
				bool common_cb_in_host, uint8_t tpc_id);

void hltests_clear_sobs(int fd, uint16_t num_of_sobs);
int hltests_clear_sobs_offset(int fd, uint16_t num_of_sobs, uint16_t offset);

void *hltests_map_hw_block(int fd, uint64_t block_addr, uint32_t *block_size);
int hltests_unmap_hw_block(int fd, void *host_addr, uint32_t block_size);

int hltests_read_lbw_mem(int fd, void *dst, void *src, uint32_t size);
int hltests_write_lbw_mem(int fd, void *dst, void *src, uint32_t size);
int hltests_read_lbw_reg(int fd, void *src, uint32_t *value);
int hltests_write_lbw_reg(int fd, void *dst, uint32_t value);

/* Generic memory addresses pool */
void *hltests_mem_pool_init(uint64_t start_addr, uint64_t size, uint8_t order);
void hltests_mem_pool_fini(void *data);
int hltests_mem_pool_alloc(void *data, uint64_t size, uint64_t *addr);
void hltests_mem_pool_free(void *data, uint64_t addr, uint64_t size);

/* ASIC functions */
uint32_t hltests_add_nop_pkt(int fd, void *buffer, uint32_t buf_off,
				struct hltests_pkt_info *pkt_info);

uint32_t hltests_add_msg_barrier_pkt(int fd, void *buffer, uint32_t buf_off,
				struct hltests_pkt_info *pkt_info);

uint32_t hltests_add_wreg32_pkt(int fd, void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);

uint32_t hltests_add_arb_point_pkt(int fd, void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);

uint32_t hltests_add_msg_long_pkt(int fd, void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);

uint32_t hltests_add_msg_short_pkt(int fd, void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);

uint32_t hltests_add_arm_monitor_pkt(int fd, void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);

uint32_t hltests_add_write_to_sob_pkt(int fd, void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);

uint32_t hltests_add_fence_pkt(int fd, void *buffer, uint32_t buf_off,
				struct hltests_pkt_info *pkt_info);

uint32_t hltests_add_dma_pkt(int fd, void *buffer, uint32_t buf_off,
				struct hltests_pkt_info *pkt_info);

uint32_t hltests_add_cp_dma_pkt(int fd, void *buffer, uint32_t buf_off,
				struct hltests_pkt_info *pkt_info);

uint32_t hltests_add_cb_list_pkt(int fd, void *buffer, uint32_t buf_off,
				struct hltests_pkt_info *pkt_info);

uint32_t hltests_add_load_and_exe_pkt(int fd, void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info);

uint32_t hltests_add_monitor_and_fence(int fd, void *buffer, uint32_t buf_off,
		struct hltests_monitor_and_fence *mon_and_fence_info);
uint32_t hltests_add_monitor(int fd, void *buffer, uint32_t buf_off,
		struct hltests_monitor *mon_info);
uint64_t hltests_get_fence_addr(int fd, uint32_t qid, bool cmdq_fence);
uint32_t hltests_add_arb_en_pkt(int fd, void *buffer, uint32_t buf_off,
		struct hltests_pkt_info *pkt_info,
		struct hltests_arb_info *arb_info,
		uint32_t queue_id, bool enable);
uint32_t hltests_add_cq_config_pkt(int fd, void *buffer, uint32_t buf_off,
		struct hltests_cq_config *cq_config);
uint32_t hltests_add_pdma_ch_bw_config_pkt(int fd, void *buffer, uint32_t buf_off,
		int qid, bool set_lbw);
uint32_t gaudi_get_dma_down_qid(int fd, enum hltests_stream_id stream);
uint32_t hltests_get_dma_up_qid(int fd, enum hltests_stream_id stream);
uint32_t hltests_get_ddma_qid(int fd, int dma_ch,
					enum hltests_stream_id stream);
uint32_t hltests_get_pdma_qid(int fd, int dma_idx);
uint8_t hltests_get_ddma_cnt(int fd);
uint8_t hltests_get_pdma_ch_cnt(int fd);
uint32_t hltests_pdma_config_ch_blocks(int fd);
uint32_t hltests_get_tpc_qid(int fd, uint8_t tpc_id,
				enum hltests_stream_id stream);
uint32_t hltests_get_mme_id(int fd, uint32_t qid);
uint32_t hltests_get_mme_qid(int fd, uint8_t mme_id, enum hltests_stream_id stream);
uint32_t hltests_get_nic_qid(int fd, uint8_t nic_id, enum hltests_stream_id stream);
uint8_t hltests_get_tpc_cnt(int fd);
uint8_t hltests_get_mme_cnt(int fd, bool master_slave_mode);
uint16_t hltests_get_first_avail_sob(int fd);
uint16_t hltests_get_first_avail_mon(int fd);
uint16_t hltests_get_first_avail_cq(int fd);
uint16_t hltests_get_first_avail_interrupt(int fd);
uint64_t hltests_get_sob_base_addr(int fd);
uint64_t hltests_get_sob_lbw_offset(int fd, uint32_t sob_idx);
uint64_t hltests_get_lbw_base_addr(int fd);
uint64_t hltests_get_any_mappable_hw_block_base_addr(int fd);
uint16_t hltests_get_cache_line_size(int fd);
uint16_t hltests_get_monitors_cnt_per_dcore(int fd);
int hltests_get_stream_master_qid_arr(int fd, uint32_t **qid_arr);

uint32_t hltests_add_sched_arc_nop_cmd(int fd, void *buffer, uint32_t buf_off,
					struct hltests_sched_arc_cmd_params *params);
uint32_t hltests_add_sched_arc_dispatch_static_ecb_list(int fd, void *buffer, uint32_t buf_off,
					struct hltests_sched_arc_cmd_params *params);
uint32_t hltests_add_direct_write_cq_pkt(int fd, void *buffer, uint32_t buf_off,
						struct hltests_direct_cq_write *pkt_info);
void hltests_monitor_dma_start(struct monitor_dma_test *params, int fd, uint32_t qid,
				int poll_interval_sec);
void hltests_monitor_dma_stop(struct monitor_dma_test *params);

void goya_tests_set_asic_funcs(struct hltests_device *hdev);
void gaudi_tests_set_asic_funcs(struct hltests_device *hdev);
void gaudi2_tests_set_asic_funcs(struct hltests_device *hdev);
void gaudi3_tests_set_asic_funcs(struct hltests_device *hdev);

double get_timediff_sec(struct timespec *begin, struct timespec *end);
double get_timediff_usec(struct timespec *begin, struct timespec *end);
double get_bw_gigabyte_per_sec(uint64_t bytes, struct timespec *begin,
							struct timespec *end);
double get_bw_gigabit_from_timesync(uint64_t total_size, struct hlthunk_time_sync_info *begin,
					struct hlthunk_time_sync_info *end);

uint32_t next_pow2(uint32_t v);

int hltests_get_max_pll_idx(int fd);
const char *hltests_stringify_pll_idx(int fd, uint32_t pll_idx);
const char *hltests_stringify_pll_type(int fd, uint32_t pll_idx,
				uint8_t type_idx);

void *gaudi_allocate_host_mem_aligned_flags(int fd, uint64_t size,
    
			enum hltests_huge huge, uint64_t align, uint32_t flags);

int hltests_device_memory_export_dmabuf_fd(int fd, void *device_addr, uint64_t size,
						uint64_t offset);

uint64_t hltests_get_tc_base_addr(int fd, uint32_t core_id);

int hltest_get_host_meminfo(struct hltest_host_meminfo *res);

int hltests_get_async_event_id(int fd, enum hltests_async_event_id hltests_event_id,
					uint32_t *asic_event_id);

uint32_t hltests_get_cq_patch_size(int fd, uint32_t qid);
uint32_t hltests_get_max_pkt_size(int fd, bool mb, bool eb, uint32_t qid);
uint64_t hltests_get_total_avail_device_mem(int fd);

int hltests_reset_events(struct hltests_state *tests_state);
int hltests_wait_for_events(struct hltests_state *tests_state, uint32_t timeout_sec,
				uint64_t expected_events, uint64_t *received_events);
int hltests_teardown_and_setup(struct hltests_state *tests_state);

void *hltests_mmap(int fd, size_t length, off_t offset);
int hltests_munmap(int fd, void *addr, size_t length);

int hltests_init(void);
void hltests_fini(void);
bool hltests_is_dma_dir_from_dram(enum hltests_dma_direction dir);

int wait_until_device_idle(int fd, uint32_t timeout_sec);
int wait_until_device_not_in_reset(int fd);
int hltests_edp_get_engines_list(int fd, uint32_t **engine_ids, uint32_t *engine_ids_size);

/* ARCs */
void *hltests_arc_get_log_buf_ci_addr(uint32_t cpu_id,
				struct hltests_arc_db *arc_db);
void *hltests_arc_get_asic_model_addr(uint32_t cpu_id,
				struct hltests_arc_db *arc_db);
void *hltests_arc_get_canary_addr(uint32_t cpu_id,
				struct hltests_arc_db *arc_db);

int hltests_arc_add_arc_config(int fd, struct hltests_arc_db *arc_db, void *cfg,
		size_t config_sz, void *cb, uint32_t cb_size, uint32_t cpu_id, uint64_t base,
		uint64_t config_addr_offset, uint64_t config_size_offset, uint64_t arc_pci_reg,
		uint32_t qid);
int hltests_arc_init(int fd, struct hltests_arc_db *arc_db);
void hltests_arc_fini(int fd, struct hltests_arc_db *arc_db);
int hltests_completion_db_init(int fd, struct hltests_completion_db *cq_db);
int hltests_completion_db_teardown(int fd, struct hltests_completion_db *cq_db);
void hltests_set_capabilities_mask(const uint64_t new_mask);
uint64_t hltests_get_capabilities_mask(void);
int hltests_asic_iterate_arcs(struct iterate_arcs_ctx *ctx, enum arc_type_mask arc_type_mask);
uint32_t hltests_arc_get_submitter_id(int fd, uint32_t scheduler_id);

/* MME DMA*/
int gaudi3_mme_dma(int fd, uint64_t src, uint64_t dst, uint8_t mme_idx, uint64_t size);
int hltests_mme_dma_init(int fd);
int hltests_is_mme_dma_enabled(int fd);
uint32_t hltests_prepare_mme_dma_req(int fd, void *cb, uint32_t cb_size, uint64_t src,
					uint64_t dst, uint8_t mme_idx, uint64_t size);
uint32_t hltests_get_mme_dma_cb_size(int fd, int num_of_lindma_pkts, uint32_t size);
bool can_open_debugfs(bool dbg_print);

/* MMU multi-page support */
VOID hltests_build_page_size_array(uint64_t **page_size_arr, uint8_t *arr_size, uint64_t bitmask);
void hltests_destroy_page_size_array(uint64_t *page_size_arr);
VOID hltests_build_memalloc_page_size_array(int fd, uint64_t **page_size_arr,
						uint8_t *page_size_arr_size);
void hltests_destroy_memalloc_page_size_array(uint64_t *page_size_arr);

/* Completion */
int hltests_submit_job(int fd, struct hltests_cs_chunk *arr, uint32_t arr_size,
					uint64_t *seq, pthread_mutex_t *lock,
					int (*submitter)(int, struct hltests_cs_chunk *));
int hltests_wait_for_job(int fd, uint64_t seq, int64_t timeout_us);
uint32_t hltests_add_cq_db_set_cq_queue_pkt(int fd, void *buffer, uint32_t buf_off,
					struct hltests_cq_config *cq_config, uint16_t sob_id);
uint64_t hltests_get_razwi_addr(int fd, enum err_trigger type);
uint64_t hltests_get_fw_mem_addr(int fd, uint64_t size);
uint64_t hltests_get_tpc_intr_cause_reg(int fd, uint64_t tpc_idx);
int verbose_printf(const char *format, ...);

/* Tests */
VOID test_hbm_read_temperature(void **state);
VOID test_hbm_read_interrupts(void **state);
VOID test_print_asic_rev(void **state);
VOID test_read_every_4KB_registers_block(void **state);
VOID test_read_through_pci(void **state);
VOID test_nic_read_interrupts(void **state);
VOID test_nic_e2e_lpbk(void **state);
VOID test_nic_bw_pfc_recv(void **state);
VOID test_nic_bw_pfc_send(void **state);
VOID test_nic_e2e(void **state);
VOID test_nic_ping_pong_latency(void **state);
VOID test_nic_bw(void **state);
VOID test_nic_bidir_bw(void **state);
VOID test_tpc_corner_with_inbound_pci_hbw(void **state);
VOID rate_limiter_init(struct hltests_state *tests_state);
VOID activate_all2all_dma_channels(void **state, uint64_t *dram_addr,
					uint32_t dma_size,
					struct hlthunk_hw_ip_info *hw_ip);
VOID test_dma_all2all_stress(void **state);
VOID test_dma_all2all_stress_minimum_host_memory(void **state);
VOID activate_super_stress_dma_channels(void **state,
					struct hlthunk_hw_ip_info *hw_ip,
					int num_of_iterations);
VOID test_dma_all2all_super_stress(void **state);
VOID test_cpucp_msg_stress(void **state);
VOID test_cpucp_eq_stress(void **state);
VOID test_mme_basic_conv(void **state);
VOID test_conn_alloc_destroy(void **state);
VOID test_conn_set_context(void **state);
VOID test_gaudi_dma_all2all(void **state);
VOID test_strided_dma(void **state);
VOID test_threads(void **state, uint32_t num_of_threads, void *(*func)(void *),
			int (*pre_task)(void *), int (*post_task)(void *));
VOID test_wq_threads(void **state);
VOID test_conn_threads(void **state, uint32_t num_of_threads);
VOID test_conn_1_thread(void **state);
VOID test_conn_8_threads(void **state);
VOID test_conn_512_threads(void **state);
VOID test_conn_1023_threads(void **state);
VOID test_print_ips_macs(void **state);
VOID test_nic_disabled_qm(void **state);
VOID test_qman_write_to_protected_register(void **state, bool is_tpc);
VOID test_goya_debugfs_sram_read_write(void **state);
VOID test_write_to_cfg_space(void **state);
VOID test_tpc_qman_write_to_protected_register(void **state);
VOID test_mme_qman_write_to_protected_register(void **state);
VOID test_write_to_mmTPC_PLL_CLK_RLX_0_from_qman(void **state);
VOID test_dma_4_queues_goya(void **state);
VOID test_axi_drain_functionality(void **state);
VOID test_fence_cnt_cleanup_on_ctx_switch(void **state);
VOID inc_sobs(int fd, uint16_t first_sob, uint16_t num_of_sobs);
VOID test_deny_access_to_secured_sobjs(void **state);
VOID test_deny_mon_access_to_secured_area(void **state);
VOID test_deny_access_to_secured_monitors(void **state);
VOID test_tdr_deadlock(void **state);
VOID test_tdr_deadlock_and_recovery(void **state);
VOID test_endless_memory_ioctl(void **state);
VOID test_dma_custom(void **state);
VOID test_transfer_bigger_than_alloc(void **state);
VOID test_map_custom(void **state);
VOID test_loop_map_work_unmap(void **state);
VOID test_duplicate_file_descriptor(void **state);
VOID test_page_miss(void **state);
VOID test_register_security(void **state);
VOID test_scan_with_sm(void **state);
VOID test_recover_from_page_fault(void **state);
VOID test_open_by_busid(void **state);
VOID test_open_twice(void **state);
VOID test_open_by_module_id(void **state);
VOID test_open_control_by_module_id(void **state);
VOID test_open_close_without_ioctl(void **state);
VOID test_close_without_releasing_debug(void **state);
VOID test_open_and_print_pci_bdf(void **state);
VOID test_sm(void **state, int engine_qid, bool is_wait);
VOID test_sm_pingpong_upper_cp(void **state, bool is_tpc,
				bool upper_cb_in_host, uint8_t engine_id);
VOID test_sm_tpc(void **state);
VOID test_sm_mme(void **state);
VOID test_sm_pingpong_tpc_upper_cp_from_device(void **state);
VOID test_sm_pingpong_mme_upper_cp_from_device(void **state);
VOID test_sm_pingpong_tpc_upper_cp_from_host(void **state);
VOID test_sm_pingpong_mme_upper_cp_from_host(void **state);
VOID test_sm_pingpong_tpc_common_cp_from_device(void **state);
VOID test_sm_pingpong_mme_common_cp_from_device(void **state);
VOID test_sm_pingpong_tpc_common_cp_from_host(void **state);
VOID test_sm_pingpong_mme_common_cp_from_host(void **state);
VOID test_sm_sob_cleanup_on_ctx_switch(void **state);
VOID test_sm_monitor_set_device_mem(void **state);
VOID test_sm_monitor_set_host_mem(void **state);
VOID test_signal_wait(void **state);
VOID test_signal_wait_parallel(void **state);
VOID test_signal_collective_wait_parallel(void **state);
VOID test_signal_wait_dma(void **state);
VOID test_sm_long_mode(void **state);
VOID test_signal_collective_wait_dma(void **state);
VOID test_print_hw_ip_info(void **state);
VOID test_get_hw_idle_info(void **state);
VOID test_print_dram_usage_info_no_stop(void **state);
VOID test_print_device_utilization_no_stop(void **state);
VOID test_get_clk_rate(void **state);
VOID test_get_reset_count(void **state);
VOID test_get_time_sync_info(void **state);
VOID test_print_hlthunk_version(void **state);
VOID test_get_cs_drop_statistics(void **state);
VOID test_get_pci_counters(void **state);
VOID test_get_clk_throttling_reason(void **state);
VOID test_get_total_energy_consumption(void **state);
VOID test_get_events_counters(void **state);
VOID test_get_events_counters_aggregate(void **state);
VOID test_get_pci_bdf(void **state);
VOID test_get_pll_info(void **state);
VOID test_get_hw_asic_status(void **state);
VOID test_get_nic_ports_mask(void **state);
VOID test_get_nic_link_state(void **state);
VOID test_print_nic_statistics(void **state);
VOID test_event_record(void **state);
VOID test_get_dev_memalloc_page_orders(void **state);
VOID test_engine_status(void **state);
VOID test_get_mac_addresses(void **state);
VOID test_get_sec_attest_info(void **state);
VOID test_page_fault_info(void **state);
VOID test_get_device_count(void **state);
VOID test_dma_entire_sram_random(void **state);

VOID test_dmabuf_basic(void **state);
VOID test_dmabuf_multiple_threads_non_shared_memory(void **state);
VOID test_dmabuf_multiple_threads_shared_memory(void **state);
VOID test_dmabuf_non_zero_offset(void **state);
VOID test_dmabuf_entire_dram(void **state);

VOID test_host_sram_perf(void **state);
VOID test_sram_host_perf(void **state);
VOID test_host_dram_perf(void **state);
VOID test_dram_host_perf(void **state);
VOID test_sram_dram_single_ch_perf(void **state);
VOID test_sram_dram_single_ch_mme_dma_perf(void **state);
VOID test_dram_sram_single_ch_perf(void **state);
VOID test_dram_sram_single_ch_mme_dma_perf(void **state);
VOID test_dram_dram_single_ch_perf(void **state);
VOID test_dram_dram_single_ch_mme_dma_perf(void **state);
VOID test_sram_dram_multi_ch_perf(void **state);
VOID test_sram_dram_multi_ch_mme_dma_perf(void **state);
VOID test_dram_sram_multi_ch_perf(void **state);
VOID test_dram_sram_multi_ch_mme_dma_perf(void **state);
VOID test_dram_dram_multi_ch_perf(void **state);
VOID test_dram_dram_multi_ch_mme_dma_perf(void **state);
VOID test_sram_dram_bidirectional_full_multi_ch_perf(void **state);
VOID test_sram_dram_bidirectional_full_multi_ch_mme_dma_perf(void **state);
VOID test_dram_sram_5ch_perf(void **state);
VOID test_host_sram_bidirectional_perf(void **state);
VOID test_host_dram_bidirectional_perf(void **state);
VOID test_host_dram_multi_ch_perf(void **state);
VOID test_dram_host_multi_ch_perf(void **state);

VOID test_map_bigger_than_4GB(void **state);
VOID gaudi_allocate_device_mem_until_full(void **state, uint32_t page_size,
					enum hltests_contiguous contigouos, bool mix_alloc);
VOID test_alloc_device_mem_until_full(void **state);
VOID test_alloc_device_mem_until_full_contiguous(void **state);
VOID test_submit_after_unmap(void **state);
VOID test_submit_and_close(void **state);
VOID test_hint_addresses(void **state);
VOID test_dmmu_hint_address(void **state);
VOID test_pmmu_hint_address(void **state, bool is_huge);
VOID test_pmmu_hint_address_regular_page(void **state);
VOID test_pmmu_hint_address_huge_page(void **state);
VOID test_dma_threads(void **state, uint32_t num_of_threads);
VOID test_dma_8_threads(void **state);
VOID test_dma_64_threads(void **state);
VOID test_dma_512_threads(void **state);
VOID dma_4_queues(void **state, bool sram_only);
VOID test_dma_4_queues(void **state);
VOID test_dma_4_queues_sram_only(void **state);
VOID test_dma_endian_swap(void **state, bool dma_up_swap,
					enum hltests_endian_swap endian_swap);
VOID test_dma_down_endian_swap_16(void **state);
VOID test_dma_up_endian_swap_16(void **state);
VOID test_dma_down_endian_swap_32(void **state);
VOID test_dma_up_endian_swap_32(void **state);
VOID test_dma_down_endian_swap_64(void **state);
VOID test_dma_up_endian_swap_64(void **state);
VOID test_lbw_scan(void **state);
VOID test_debug_mode(void **state);
VOID hltest_bench_host_map_expected(struct hltests_state *tests_state,
					uint64_t n_allocs, uint64_t alloc_size,
					enum hltests_huge huge,
					uint64_t n_maps, uint64_t n_unmaps,
					enum hltests_random random,
					uint32_t n_iter,
					bool disabled_test,
					bool validate_exp,
					const char *test_name);
VOID test_bench_mappings_custom(void **state);
VOID submit_cs_nop(void **state, int num_of_pqe,
				uint16_t wait_after_submit_cnt);
VOID test_cs_nop(void **state);
VOID test_cs_nop_16PQE(void **state);
VOID test_cs_nop_32PQE(void **state);
VOID test_cs_nop_48PQE(void **state);
VOID test_cs_nop_64PQE(void **state);
VOID test_and_measure_wait_after_submit_cs_nop(void **state);
VOID test_and_measure_wait_after_64_submit_cs_nop(void **state);
VOID test_and_measure_wait_after_256_submit_cs_nop(void **state);
VOID test_cs_msg_long(void **state);
VOID test_cs_msg_long_2000(void **state);
VOID test_cs_two_streams_with_fence(void **state);
VOID hltests_cs_two_streams_arb_point(int fd,
					     struct hltests_arb_info *arb_info,
					     uint64_t *host_data_va,
					     uint64_t *device_data_addr,
					     uint16_t sob_id,
					     uint16_t *mon_id,
					     uint32_t dma_size,
					     bool ds_direction);
VOID test_cs_two_streams_with_arb(void **state);
VOID test_cs_two_streams_with_priority_arb(void **state);
VOID test_cs_two_streams_with_wrr_arb(void **state);
VOID test_cs_cq_wrap_around(void **state);
VOID test_cs_load_predicates(void **state, bool is_consecutive_map);
VOID test_cs_load_pred_non_consecutive_map(void **state);
VOID test_cs_load_pred_consecutive_map(void **state);
VOID load_scalars_and_exe_4_rfs(int fd, uint64_t scalar_buf_sram_addr,
					uint64_t cb_sram_addr,
					uint64_t msg_long_dst_sram_addr,
					uint64_t host_data_device_va,
					bool is_separate_exe);
VOID test_cs_load_scalars_exe_4_rfs(void **state);
VOID load_scalars_and_exe_2_rfs(int fd, uint64_t scalar_buf_sram_addr,
					uint64_t cb_sram_addr, uint16_t sob0,
					uint16_t mon0, bool is_upper_rfs,
					bool is_separate_exe);
VOID test_cs_load_scalars_exe_2_rfs(void **state, bool is_upper_rfs);
VOID test_cs_load_scalars_exe_lower_2_rfs(void **state);
VOID test_cs_load_scalars_exe_upper_2_rfs(void **state);
VOID test_cs_cb_list(void **state);
VOID test_cs_cb_list_with_parallel_pqe(void **state);
VOID test_cs_drop(void **state);
VOID test_wait_for_cs_with_timestamp(void **state);
VOID test_staged_submission_256_threads(void **state);
VOID test_wait_for_interrupt(void **state);
VOID dma_entire_dram_random(void **state, uint64_t zone_size,
			uint64_t dma_size);
VOID test_dma_entire_dram_random_256KB(void **state);
VOID test_dma_entire_dram_random_512KB(void **state);
VOID test_dma_entire_dram_random_1MB(void **state);
VOID test_dma_entire_dram_random_2MB(void **state);
VOID test_cb_mmap(void **state);
VOID test_cb_unaligned_size(void **state);
VOID test_cb_small_unaligned_odd_size(void **state);
VOID test_cb_unaligned_odd_size(void **state);
VOID test_cb_skip_unmap(void **state);
VOID test_cb_skip_unmap_and_destroy(void **state);
VOID test_cb_skip_unmap_and_redundant_destroy(void **state);
VOID test_cb_unalign(void **state);
VOID test_common_cb_unalign(void **state);
VOID test_cb_kernel_mapped(void **state);
VOID test_error_injection_endless_command(void **state);
VOID test_error_injection_non_fatal_event(void **state);
VOID test_error_injection_fatal_event(void **state);
VOID test_error_injection_heartbeat(void **state);
VOID test_error_injection_thermal_event(void **state);
VOID test_dma_all2all_dram2sram(void **state);
VOID test_dma_all2all_dram2dram(void **state);
VOID test_dma_all2all_sram2sram(void **state);
VOID test_axi_drain_functionality_gaudi2(void **state);
VOID test_debugfs_dmmu_low_addresses(void **state);
VOID test_debugfs_dmmu_high_addresses(void **state);
VOID test_debugfs_read_write_host(void **state);
VOID test_debugfs_read_write_host64(void **state);
VOID test_debugfs_read_write_sram(void **state);
VOID test_debugfs_read_write_dram(void **state);
VOID test_debugfs_read_write_sram64(void **state);
VOID test_debugfs_read_write_dram64(void **state);
VOID test_debugfs_read_write_dram_full_range(void **state);
VOID test_cbc_single_range(void **state);
VOID test_cbc_multiple_ranges(void **state);
VOID test_coalescing_same_interrupt(void **state);
VOID test_coalescing_different_interrupts(void **state);
VOID test_pmmu_page_fault_sm_initiator(void **state);
VOID test_pmmu_page_fault_tpc_initiator(void **state);
VOID test_pmmu_page_fault_mme_initiator(void **state);
VOID test_pmmu_page_fault_pdma_initiator(void **state);
VOID test_hmmu_page_fault_pdma_initiator(void **state);
VOID test_odp_page_in_flow_basic(void **state);
VOID test_odp_full_cycle_basic(void **state);
#endif /* HLTHUNK_TESTS_H */

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
static pthread_mutex_t debugfs_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_spinlock_t rand_lock;
static khash_t(ptr) * dev_table;

static long asic_mask_for_testing = HLTEST_DEVICE_MASK_DONT_CARE;

#ifndef HLTESTS_LIB_MODE
static pthread_barrier_t barrier;

static int run_disabled_tests;
static int num_devices = 1;
#endif

static int verbose_enabled;
static const char *parser_pciaddr;
static const char *config_filename;
static int nic_port;
static int legacy_mode_enabled = 1;
static uint32_t cur_seed;
static char build_path_str[BUILD_PATH_MAX_LENGTH];
static const char *build_path;
static uint64_t hltests_capabilities_mask;
static int enable_arc_log;
static int events_listener = 1;
static uint32_t negative_tests;
static uint32_t run_mini_suite;

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

bool hltests_is_dma_dir_from_dram(enum hltests_dma_direction dir)
{
	return dir == DMA_DIR_DRAM_TO_SRAM ||
		dir == DMA_DIR_DRAM_TO_HOST ||
		dir == DMA_DIR_DRAM_TO_DRAM;
}

#ifndef HLTESTS_LIB_MODE
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

int hltests_run_group_tests(const char *group_name,
				const struct CMUnitTest * const tests,
				const size_t num_tests,
				CMFixtureFunction group_setup,
				CMFixtureFunction group_teardown)
{
	struct tm ts_units_curr[num_devices], ts_units_prev[num_devices];
	struct hltests_thread_params *thread_params = NULL;
	time_t ts_curr[num_devices], ts_prev[num_devices];
	char formatted_ts[SZ_128], device_no[SZ_128];
	uint32_t i, num_threads = num_devices;
	pthread_t *thread_ids = NULL;
	int rc, diff_sec;
	void *retval;

	rc = pthread_barrier_init(&barrier, NULL, num_threads);
	if (rc) {
		printf("Failed to initialize pthread barrier [rc %d]\n", rc);
		return rc;
	}

	rc = gaudi_init();
	if (rc) {
		printf("Failed to initialize tests library [rc %d]\n", rc);
		goto out;
	}

	/* Allocate arrays for threads management */
	thread_ids = (pthread_t *) hlthunk_malloc(num_threads *
							sizeof(*thread_ids));
	if (!thread_ids) {
		printf("Failed to allocate memory for thread identifiers\n");
		rc = -ENOMEM;
		goto out;
	}

	thread_params = (struct hltests_thread_params *)
			hlthunk_malloc(num_threads * sizeof(*thread_params));
	if (!thread_params) {
		printf("Failed to allocate memory for thread parameters\n");
		rc = -ENOMEM;
		goto out;
	}

	/* Create and execute threads */
	for (i = 0 ; i < num_threads ; i++) {
		if (time(&ts_curr[i]) == -1) {
			perror("'time()' failed");
			goto out;
		}

		localtime_r(&ts_curr[i], &ts_units_curr[i]);
		strftime(formatted_ts, sizeof(formatted_ts), "%T", &ts_units_curr[i]);
		sprintf(device_no, num_threads > 1 ? "[device no. %d]" : "", i);
		printf("\n[%s]%s timestamp #1: %s\n", group_name, device_no, formatted_ts);

		thread_params[i].group_name = group_name;
		thread_params[i].tests = tests;
		thread_params[i].num_tests = num_tests;
		thread_params[i].group_setup = group_setup;
		thread_params[i].group_teardown = group_teardown;

		rc = pthread_create(&thread_ids[i], NULL, hltests_thread_start,
					&thread_params[i]);
		if (rc) {
			printf("Failed to create thread %d\n", i);
			goto out;
		}
	}

	/* Wait for the termination of the threads */
	for (i = 0 ; i < num_threads ; i++) {
		rc = pthread_join(thread_ids[i], &retval);

		/* Save timestamps previously sampled */
		ts_prev[i] = ts_curr[i];
		ts_units_prev[i] = ts_units_curr[i];

		if (time(&ts_curr[i]) == -1) {
			perror("'time()' failed");
			goto out;
		}

		localtime_r(&ts_curr[i], &ts_units_curr[i]);
		strftime(formatted_ts, sizeof(formatted_ts), "%T", &ts_units_curr[i]);
		sprintf(device_no, num_threads > 1 ? "[device no. %d]" : "", i);
		printf("\n[%s]%s timestamp #2: %s\n", group_name, device_no, formatted_ts);

		/* Set relative H/M/S, later to be formatted using strftime */
		diff_sec = (int)difftime(ts_curr[i], ts_prev[i]);
		ts_units_curr[i].tm_hour = diff_sec / 3600;
		diff_sec %= 3600; /* reduced whole hours */
		ts_units_curr[i].tm_min = diff_sec / 60;
		diff_sec %= 60; /* reduced whole minutes */
		ts_units_curr[i].tm_sec = diff_sec;

		strftime(formatted_ts, sizeof(formatted_ts), "%T", &ts_units_curr[i]);
		printf("[%s]%s runtime: %s\n\n", group_name, device_no, formatted_ts);

		if (rc) {
			printf("Failed to join with thread %d\n", i);
			goto out;
		}

		if (!retval || retval == PTHREAD_CANCELED) {
			printf("Thread %d has failed\n", i);
			rc = -1;
			goto out;
		}
	}

out:
	/* Cleanup */
	hlthunk_free(thread_params);
	hlthunk_free(thread_ids);
	hltests_fini();
	pthread_barrier_destroy(&barrier);

	return rc;
}
#endif

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

int hltests_control_dev_open(const char *busid)
{
	enum hlthunk_device_name actual_asic_type;
	struct hltests_device *hdev;
	int fd, rc;
	khint_t k;

	if (!asic_mask_for_testing) {
		printf("Expecting invalid ASIC!!!\n");
		printf("Something is very wrong, exiting...\n");
		rc = -EINVAL;
		goto out;
	}

	pthread_mutex_lock(&table_lock);

	rc = fd = hlthunk_open_control(0, busid);
	if (fd < 0)
		goto out;

	actual_asic_type = hlthunk_get_device_name_from_fd(fd);
	if (!hltests_is_asic_type_valid(actual_asic_type)) {
		rc = -EINVAL;
		hlthunk_close(fd);
		pthread_mutex_unlock(&table_lock);
		exit(0);
	}

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

	k = kh_put(ptr, dev_table, fd, &rc);
	kh_val(dev_table, k) = hdev;

	hdev->device_id = hlthunk_get_device_id_from_fd(fd);

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

	pthread_mutex_unlock(&table_lock);
	return fd;

remove_device:
	kh_del(ptr, dev_table, k);
	hlthunk_free(hdev);
close_device:
	hlthunk_close(fd);
out:
	pthread_mutex_unlock(&table_lock);
	return rc;
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

void *hltests_mmap(int fd, size_t length, off_t offset)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	void *ptr;
	int rc;
	khint_t k;

	ptr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
	if (ptr == MAP_FAILED)
		return MAP_FAILED;

	pthread_mutex_lock(&hdev->mmap_table_lock);

	k = kh_put(mapping, hdev->mmap_table, (uintptr_t)ptr, &rc);
	if (rc < 0) {
		pthread_mutex_unlock(&hdev->mmap_table_lock);
		munmap(ptr, length);
		errno = ENOMEM;
		return MAP_FAILED;
	}
	kh_val(hdev->mmap_table, k) = length;

	pthread_mutex_unlock(&hdev->mmap_table_lock);

	return ptr;
}

int hltests_munmap(int fd, void *addr, size_t length)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	khint_t k;

	pthread_mutex_lock(&hdev->mmap_table_lock);

	k = kh_get(mapping, hdev->mmap_table, (uint64_t)(uintptr_t)addr);
	if (k == kh_end(hdev->mmap_table)) {
		pthread_mutex_unlock(&hdev->mmap_table_lock);
		errno = EINVAL;
		return -1;
	}

	if (length != kh_val(hdev->mmap_table, k)) {
		pthread_mutex_unlock(&hdev->mmap_table_lock);
		errno = ENOENT;
		return -1;
	}

	kh_del(mapping, hdev->mmap_table, k);

	pthread_mutex_unlock(&hdev->mmap_table_lock);

	return munmap(addr, length);
}

static int debugfs_open(struct hltests_state *tests_state, int device_idx)
{
	int parent_device_fd, debugfs_addr_fd, debugfs_data32_fd, clk_gate_fd, debugfs_data64_fd;
	char parent_device[16], clk_gate_str[16] = "0";
	char path[PATH_MAX];
	ssize_t size;

	snprintf(path, PATH_MAX, "/sys/class/accel/accel%d/device/parent_device", device_idx);
	parent_device_fd = open(path, O_RDONLY);
	if (parent_device_fd == -1) {
		printf("Failed to open sysfs parent_device\n");
		return -EPERM;
	}

	size = read(parent_device_fd, parent_device, sizeof(parent_device));
	if (size <= 0) {
		close(parent_device_fd);
		printf("Failed to read from sysfs parent_device, rc %zd\n", size);
		return -errno;
	}

	parent_device[strcspn(parent_device, "\n")] = '\0'; /* remove trailing newline character */
	close(parent_device_fd);

	snprintf(path, PATH_MAX, "/sys/kernel/debug/accel/%s/addr", parent_device);

	debugfs_addr_fd = open(path, O_WRONLY);

	if (debugfs_addr_fd == -1) {
		printf("Failed to open debugfs_addr_fd (forgot sudo ?)\n");
		return -EPERM;
	}

	snprintf(path, PATH_MAX, "/sys/kernel/debug/accel/%s/data32", parent_device);

	debugfs_data32_fd = open(path, O_RDWR);

	if (debugfs_data32_fd == -1) {
		close(debugfs_addr_fd);
		printf("Failed to open debugfs_data_fd (forgot sudo ?)\n");
		return -EPERM;
	}

	snprintf(path, PATH_MAX, "/sys/kernel/debug/accel/%s/data64", parent_device);

	debugfs_data64_fd = open(path, O_RDWR);

	if (debugfs_data64_fd == -1) {
		close(debugfs_data32_fd);
		close(debugfs_addr_fd);
		printf("Failed to open debugfs_data64_fd (forgot sudo ?)\n");
		return -EPERM;
	}

	snprintf(path, PATH_MAX, "/sys/kernel/debug/accel/%s/clk_gate", parent_device);

	clk_gate_fd = open(path, O_RDWR);

	if (clk_gate_fd == -1) {
		close(debugfs_addr_fd);
		close(debugfs_data64_fd);
		close(debugfs_data32_fd);
		printf("Failed to open clk_gate_fd (forgot sudo ?)\n");
		return -EPERM;
	}

	tests_state->debugfs.addr_fd = debugfs_addr_fd;
	tests_state->debugfs.data32_fd = debugfs_data32_fd;
	tests_state->debugfs.data64_fd = debugfs_data64_fd;
	tests_state->debugfs.clk_gate_fd = clk_gate_fd;

	size = pread(tests_state->debugfs.clk_gate_fd,
			tests_state->debugfs.clk_gate_val,
			sizeof(tests_state->debugfs.clk_gate_val), 0);
	if (size < 0)
		printf("Failed to read debugfs clk gate fd [rc %zd]\n", size);

	size = write(tests_state->debugfs.clk_gate_fd, clk_gate_str,
			strlen(clk_gate_str) + 1);
	if (size < 0)
		printf("Failed to write debugfs clk gate [rc %zd]\n", size);

	return 0;
}

static int debugfs_close(struct hltests_state *tests_state)
{
	ssize_t size;

	if ((tests_state->debugfs.addr_fd == -1) ||
		(tests_state->debugfs.data32_fd == -1) ||
		(tests_state->debugfs.data64_fd == -1) ||
		(tests_state->debugfs.clk_gate_fd == -1))
		return -EFAULT;

	size = write(tests_state->debugfs.clk_gate_fd,
			tests_state->debugfs.clk_gate_val,
			strlen(tests_state->debugfs.clk_gate_val) + 1);
	if (size < 0)
		printf("Failed to write debugfs clk gate [rc %zd]\n", size);

	close(tests_state->debugfs.clk_gate_fd);
	close(tests_state->debugfs.addr_fd);
	close(tests_state->debugfs.data32_fd);
	close(tests_state->debugfs.data64_fd);
	tests_state->debugfs.clk_gate_fd = -1;
	tests_state->debugfs.addr_fd = -1;
	tests_state->debugfs.data32_fd = -1;
	tests_state->debugfs.data64_fd = -1;

	return 0;
}

uint32_t hltests_debugfs_read(int addr_fd, int data_fd, uint64_t full_address)
{
	char addr_str[64] = "", value[64] = "";
	ssize_t size;

	sprintf(addr_str, "0x%lx", full_address);

	pthread_mutex_lock(&debugfs_lock);

	size = write(addr_fd, addr_str, strlen(addr_str) + 1);
	if (size < 0)
		printf("Failed to write to debugfs address fd [rc %zd]\n",
				size);

	size = pread(data_fd, value, sizeof(value), 0);
	if (size < 0)
		printf("Failed to read from debugfs data fd [rc %zd]\n", size);

	pthread_mutex_unlock(&debugfs_lock);

	return strtoul(value, NULL, 16);
}

void hltests_debugfs_write(int addr_fd, int data_fd, uint64_t full_address,
				uint32_t val)
{
	char addr_str[64] = "", val_str[64] = "";
	ssize_t size;

	sprintf(addr_str, "0x%lx", full_address);
	sprintf(val_str, "0x%x", val);

	pthread_mutex_lock(&debugfs_lock);

	size = write(addr_fd, addr_str, strlen(addr_str) + 1);
	if (size < 0)
		printf("Failed to write to debugfs address [rc %zd]\n", size);

	size = write(data_fd, val_str, strlen(val_str) + 1);
	if (size < 0)
		printf("Failed to write to debugfs data [rc %zd]\n", size);

	pthread_mutex_unlock(&debugfs_lock);
}

uint64_t hltests_debugfs_read64(int addr_fd, int data_fd, uint64_t full_address)
{
	char addr_str[64] = "", value[64] = "";
	ssize_t size;

	sprintf(addr_str, "0x%lx", full_address);

	pthread_mutex_lock(&debugfs_lock);

	size = write(addr_fd, addr_str, strlen(addr_str) + 1);
	if (size < 0)
		printf("Failed to write64 to debugfs address fd [rc %zd]\n",
				size);

	size = pread(data_fd, value, sizeof(value), 0);
	if (size < 0)
		printf("Failed to read from debugfs data fd [rc %zd]\n", size);

	pthread_mutex_unlock(&debugfs_lock);

	return strtoul(value, NULL, 16);
}

void hltests_debugfs_write64(int addr_fd, int data_fd, uint64_t full_address,
				uint64_t val)
{
	char addr_str[64] = "", val_str[64] = "";
	ssize_t size;

	sprintf(addr_str, "0x%lx", full_address);
	sprintf(val_str, "0x%lx", val);

	pthread_mutex_lock(&debugfs_lock);

	size = write(addr_fd, addr_str, strlen(addr_str) + 1);
	if (size < 0)
		printf("Failed to write to debugfs address fd [rc %zd]\n",
				size);

	size = write(data_fd, val_str, strlen(val_str) + 1);
	if (size < 0)
		printf("Failed to write to debugfs data fd [rc %zd]\n", size);

	pthread_mutex_unlock(&debugfs_lock);
}

static bool hltests_is_importer_exists(void)
{
	if (!access("/dev/hli", F_OK))
		return true;
	return false;
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

int hltests_control_dev_setup(void **state)
{
	struct hltests_state *tests_state;
	struct hltests_device *hdev;
	int rc, fd;

	tests_state = hltests_alloc_state();
	if (!tests_state)
		return -ENOMEM;

	fd = tests_state->fd = hltests_control_dev_open(parser_pciaddr);
	if (fd < 0) {
		printf("Failed to open device %d\n", fd);
		rc = fd;
		goto free_state;
	}

	rc = hlthunk_get_hw_ip_info(fd, &tests_state->hw_ip);
	assert_int_equal(rc, 0);

	hdev = get_hdev_from_fd(fd);
	if (!hdev) {
		printf("Failed to get hdev from file descriptor %d\n", fd);
		rc = -ENODEV;
		goto close_fd;
	}

	*state = tests_state;

	return 0;

close_fd:
	if (hltests_close(fd))
		printf("Problem in closing FD, ignoring...\n");
free_state:
	hlthunk_free(tests_state);

	return rc;
}

int hltests_control_dev_teardown(void **state)
{
	struct hltests_state *tests_state = (struct hltests_state *) *state;

	if (!tests_state)
		return -EINVAL;

	if (hltests_control_dev_close(tests_state->fd))
		printf("Problem in closing FD, ignoring...\n");

	hlthunk_free(*state);

	return 0;
}

static void hltests_pdma_memory_free(int fd, struct hltests_pdma_db *pdma_db)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	const struct hltests_asic_funcs *asic = hdev->asic_funcs;
	uint32_t pdma_ch_cnt = asic->pdma_get_max_ch_id(fd);
	struct pdma_ch_info *ch_info = pdma_db->ch_info;
	int i;

	for (i = 0 ; i < pdma_ch_cnt ; i++)
		gaudi_free_host_mem(fd, ch_info[i].submission_q);

	hlthunk_free(pdma_db->ch_info);
}

static int hltests_pdma_memory_allocate(int fd, struct hltests_pdma_db *pdma_db)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	const struct hltests_asic_funcs *asic = hdev->asic_funcs;
	uint32_t pdma_ch_cnt = asic->pdma_get_max_ch_id(fd);
	struct pdma_ch_info *ch_info;
	int i, ch_idx, rc = -ENOMEM;

	assert_int_equal(!IS_POWER_OF_TWO(PQM_PI_CI_IN_MEM_Q_SIZE), 0);
	assert_in_range(PQM_PI_CI_IN_MEM_Q_SIZE, 0, SZ_1G);

	memset(pdma_db, 0, sizeof(*pdma_db));

	pdma_db->ch_info = hlthunk_malloc(sizeof(struct pdma_ch_info) * pdma_ch_cnt);
	if (!pdma_db->ch_info)
		return rc;

	for (ch_idx = 0 ; ch_idx < pdma_ch_cnt ; ch_idx++) {
		ch_info = &pdma_db->ch_info[ch_idx];
		ch_info->submission_q_size = PQM_PI_CI_IN_MEM_Q_SIZE;
		ch_info->submission_q = hltests_allocate_host_mem_aligned(fd,
				ALIGN_UP(ch_info->submission_q_size, 8), NOT_HUGE_MAP, 8);
		if (!ch_info->submission_q)
			goto free_pqm_ch_q_mem;

		/* The low part of the pointer to the base of submission queue for PI/CI
		 * in memory mode. This address is expected to be 8B aligned.
		 */
		assert_int_equal(!IS_8B_ALIGNED((uintptr_t)ch_info->submission_q), 0);

		ch_info->submission_q_handle =
				gaudi_get_device_va_for_host_ptr(fd, ch_info->submission_q);
		assert_non_null(ch_info->submission_q_handle);

		/* Submission queue of PI/CI in memory mode is expected to be 8B aligned */
		assert_int_equal(!IS_8B_ALIGNED(ch_info->submission_q_handle), 0);
	}

	return 0;

free_pqm_ch_q_mem:
	ch_info = pdma_db->ch_info;
	for (i = 0 ; i < ch_idx ; i++)
		gaudi_free_host_mem(fd, ch_info[i].submission_q);

	hlthunk_free(ch_info);

	return rc;
}

int hltests_pdma_init(int fd, struct hltests_pdma_db *pdma_db)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	struct hlthunk_hw_ip_info hw_ip;
	int rc;

	rc = hlthunk_get_hw_ip_info(fd, &hw_ip);
	assert_int_equal(rc, 0);

	if (!hw_ip.pdma_user_owned_ch_mask)
		return 0;

	rc = hltests_pdma_memory_allocate(fd, pdma_db);
	if (rc)
		return rc;

	/* map user-owned channels to user's address space */
	rc = hdev->asic_funcs->pdma_map_lbw_blocks(fd, pdma_db);
	if (rc) {
		printf("Failed to map LBW blocks for PDMA\n");
		goto err_free_pdma_memory;
	}

	rc = hdev->asic_funcs->pdma_config_ch_blocks(fd, pdma_db);
	if (rc) {
		printf("Failed to configure PDMA channels/s\n");
		goto err_unmap_lbw_blocks;
	}

	return 0;

err_unmap_lbw_blocks:
	hdev->asic_funcs->pdma_unmap_lbw_blocks(fd, pdma_db);
err_free_pdma_memory:
	hltests_pdma_memory_free(fd, pdma_db);

	return rc;
}

void hltests_pdma_fini(int fd, struct hltests_pdma_db *pdma_db)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);

	if (!pdma_db->user_en_ch_mask)
		return;

	hdev->asic_funcs->pdma_unmap_lbw_blocks(fd, pdma_db);
	hltests_pdma_memory_free(fd, pdma_db);
}

uint64_t hltests_get_total_avail_device_mem(int fd)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	struct hlthunk_hw_ip_info hw_ip;
	uint64_t dram_size, num_pages;
	int rc;

	rc = hlthunk_get_hw_ip_info(fd, &hw_ip);
	assert_int_equal(rc, 0);

	dram_size = hw_ip.dram_size;

	/*
	 * When we add dram reserved areas smaller than 32MB tests suite such
	 * as entire dram starts to fails. the reason is the hw_ip.dram_size
	 * takes into consideration those reserved areas and such tests
	 * will try to allocate the whole available memory in pages of
	 * default page size(meaning the total available memory rounded up
	 * according to the default page size). in such scenario the memory
	 * pool will have less room than the rounded up size to be allocated
	 * and then the tests will fail.
	 * To solve this we need to round down the total available size
	 * according to the default page size.
	 */
	dram_size = rounddown(dram_size, hw_ip.device_mem_alloc_default_page_size);

	if (!hltests_is_legacy_mode_enabled(fd)) {
		/*
		 * In ARC mode we allocate device memory for loading the FW
		 * In this test we should subtract this size from the entire dram size
		 * and test only the remaining part.
		 */
		num_pages = DIV_ROUND_UP(hdev->arc_db.device_mem_size,
						hw_ip.device_mem_alloc_default_page_size);
		dram_size -= (num_pages * hw_ip.device_mem_alloc_default_page_size);
	}

	return dram_size;
}

int hltests_setup_user_engines(struct hltests_state *tests_state)
{
	int rc, fd = tests_state->fd, verbose = hltests_get_verbose_enabled();
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	time_t pdma_ts, arcdb_ts, mme_dma_ts, arc_init_ts, end;

	if (hltests_is_legacy_mode_enabled(fd))
		return 0;

	memset(&hdev->arc_db, 0, sizeof(struct hltests_arc_db));

	hdev->arc_db.fw_info =
		hlthunk_malloc(sizeof(struct arc_fw_info) *
				hdev->asic_funcs->arc_get_max_cpuid());
	if (!hdev->arc_db.fw_info)
		return -ENOMEM;

	if (verbose) {
		time(&pdma_ts);
		printf("Init PDMA...");
	}

	rc = hltests_pdma_init(fd, &hdev->pdma_db);
	if (rc) {
		printf("\nFailed to allocate PDMA resources\n");
		goto free_fw_info;
	}

	if (verbose) {
		time(&arcdb_ts);
		printf("\t\t%.f seconds\n", difftime(arcdb_ts, pdma_ts));
		printf("Init ARC CQ DB...");
	}

	rc = hltests_completion_db_init(fd, &hdev->completion_db);
	if (rc) {
		printf("\nFailed to configure completion DB\n");
		goto pdma_fini;
	}

	if (verbose) {
		time(&mme_dma_ts);
		printf("\t%.f seconds\n", difftime(mme_dma_ts, arcdb_ts));
		printf("Init MME DMA...");
	}

	rc = hltests_mme_dma_init(fd);
	if (rc) {
		printf("\nFailed to init MME DMA\n");
		goto arc_cq_db_teardown;
	}



	if (verbose) {
		time(&arc_init_ts);
		printf("\t\t%.f seconds\n", difftime(arc_init_ts, mme_dma_ts));
		printf("Initializing ARCs...\n");
	}

	rc = hltests_arc_init(fd, &hdev->arc_db);
	if (rc) {
		printf("Failed to allocate arc memories\n");
		goto arc_cq_db_teardown;
	}

	if (verbose) {
		time(&end);
		printf("Initializing ARCs took \t%.f seconds\n", difftime(end, arc_init_ts));
	}

	return 0;

arc_cq_db_teardown:
	hltests_completion_db_teardown(fd, &hdev->completion_db);
pdma_fini:
	hltests_pdma_fini(fd, &hdev->pdma_db);
free_fw_info:
	hlthunk_free(hdev->arc_db.fw_info);

	return rc;
}

/**
 * This function waits until expected events are received and retrieve the whole received events
 * @tests_state pointer to the hltests_state structure
 * @timeout_sec timeout duration in seconds. 0 means using the default timeout value.
 * @expected_events mask of events to wait for
 * @received_events optional pointer to uint64_t to store a mask of the received events
 * @return 0 for success, negative value for failure
 */
int hltests_wait_for_events(struct hltests_state *tests_state, uint32_t timeout_sec,
				uint64_t expected_events, uint64_t *received_events)
{
	struct hltests_listener_thread_params *params = &tests_state->listener_thread_params;
	uint64_t events_to_wait_for = expected_events;
	struct timespec ts;
	int rc;

	if (!timeout_sec)
		timeout_sec = hltests_is_pldm(tests_state->fd) ?
				PLDM_EVENTS_WAIT_TIMEOUT_SEC :
				EVENTS_WAIT_TIMEOUT_SEC;

	if (received_events)
		*received_events = 0;

	rc = pthread_mutex_lock(&params->lock);
	if (rc)
		return rc;

	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += timeout_sec;

	while (events_to_wait_for) {
		/* Store all received events, either if they are part of the expected mask or not */
		if (received_events)
			*received_events |= params->notifier_events;

		/* Clear the received events from the mask of events that we wait for */
		events_to_wait_for &= ~params->notifier_events;
		params->notifier_events = 0;
		if (!events_to_wait_for)
			break;

		rc = pthread_cond_timedwait(&params->cond, &params->lock, &ts);
		if (rc) {
			pthread_mutex_unlock(&params->lock);
			/* SW-159137 TODO: this 'if' can be removed when issue is solved */
			if (rc == ETIMEDOUT && received_events && *received_events == 0) {
				char buf[128];
				int fdinfo_fd, n;

				printf("timeout, notifier events are 0x%lx\n",
							params->notifier_events);
				sprintf(buf, "/proc/%u/fdinfo/%u", params->listener_tid,
							params->handle);
				fdinfo_fd = open(buf, O_RDONLY);
				if (fdinfo_fd < 0)
					return rc;
				memset(buf, 0, sizeof(buf));
				n = read(fdinfo_fd, buf, sizeof(buf));
				buf[127] = '\0';
				printf("eventfd fdinfo (first %d bytes):\n%s\n", n, buf);
				close(fdinfo_fd);
			}
			return rc;
		}
	}

	return pthread_mutex_unlock(&params->lock);
}

static enum hltests_recovery_status hltests_get_recovery_status(struct hltests_state *tests_state)
{
	struct hltests_listener_thread_params *params = &tests_state->listener_thread_params;

	return params->recovery_status;
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

	if (hltests_is_importer_exists()) {
		tests_state->imp_fd = open("/dev/hli", O_RDWR | O_CLOEXEC, 0);
		if (tests_state->imp_fd < 0) {
			printf("Failed to open importer %d\n",
							tests_state->imp_fd);
			rc = tests_state->imp_fd;
			goto close_fd;
		}
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

	return debugfs_open(tests_state, device_idx);
}

int gaudi_teardown(void **state)
{
	struct hltests_state *tests_state;

	if (can_open_debugfs(false)) {
		tests_state = (struct hltests_state *) *state;

		if (!tests_state)
			return -EINVAL;

		debugfs_close(tests_state);
	}

	return gaudi_teardown_common(state);
}

int hltests_root_debug_setup(void **state)
{
	const char *pciaddr = hltests_get_parser_pciaddr();
	struct hltests_state *tests_state;
	int device_idx = 0, control_fd;

	tests_state = hltests_alloc_state();
	if (!tests_state)
		return -ENOMEM;

	*state = tests_state;

	if (!asic_mask_for_testing) {
		printf("Expecting invalid ASIC!!!\n");
		printf("Something is very wrong, exiting...\n");
		return -EINVAL;
	}

	if (pciaddr) {
		device_idx = hlthunk_get_device_index_from_pci_bus_id(pciaddr);
		if (device_idx < 0) {
			printf("No device for the given PCI address %s\n",
				pciaddr);
			return -EINVAL;
		}
	}

	control_fd = hlthunk_open_control(device_idx, pciaddr);
	if (control_fd < 0)
		return control_fd;

	tests_state->asic_type = hlthunk_get_device_name_from_fd(control_fd);
	hlthunk_close(control_fd);

	if (!hltests_is_asic_type_valid(tests_state->asic_type)) {
		hlthunk_free(*state);
		exit(0);
	}

	return debugfs_open(tests_state, device_idx);
}

int hltests_root_debug_teardown(void **state)
{
	struct hltests_state *tests_state = (struct hltests_state *) *state;
	int rc;

	if (!tests_state)
		return -EINVAL;

	rc = debugfs_close(tests_state);

	hlthunk_free(*state);

	return rc;
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

static int sim_allocate_device_mem_on_host(int fd, struct hltests_memory *mem)
{
	mem->host_ptr = hlthunk_malloc(mem->size);
	if (!mem->host_ptr) {
		E("Sim failed to allocate %#lx bytes of host memory!", mem->size);
		goto error_memory_alloc;
	}

	mem->device_virt_addr = hlthunk_host_memory_map(fd, mem->host_ptr, 0, mem->size);
	if (!mem->device_virt_addr) {
		E("Sim failed to map host memory to device!");
		goto error_memory_map;
	}

	mem->device_handle = 0;

	return 0;

error_memory_map:
	free(mem->host_ptr);

error_memory_alloc:
	return -1;
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
		      "The cast to `void *` can be fatal on 32bit systems, whoever decided to return a `void *` clearly wasn't thinking straight.");
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
	} else if (hdev->sim_dram_on_host) {
		rc = sim_allocate_device_mem_on_host(fd, mem);
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
 * This function allocates DRAM memory on the device, using all supported page sizes and
 * will map it to the device virtual address space. The input 'size' will be divided into several
 * allocations. Each allocation will be stored inside "device_addr_arr" array's entry.
 * @param fd file descriptor of the device to which the function will map the memory
 * @param size how much memory to allocate
 * @param device_addr_arr output array, each array entry stores an allocated device address
 * @param device_addr_arr_size the size of "device_addr_arr" array
 * @param actual_arr_size the actual size of "device_addr_arr" array
 * @return 0 for success
 */
int gaudi_allocate_device_mem_mix_page_size(int fd, uint64_t size,
			uint64_t *device_addr_arr, int device_addr_arr_size, int *actual_arr_size)
{
	uint64_t page_size, size_allocated = 0, *page_arr = NULL, end_device_addr = 0;
	int i = 0, next_page_size, arr_idx = 0, rc = 0;
	uint8_t page_arr_size = 0;
	void *device_addr = NULL;

	hltests_build_memalloc_page_size_array(fd, &page_arr, &page_arr_size);

	if (!page_arr || !page_arr_size)
		return -EIO;

	while (size_allocated < size) {
		for (i = 0 ; i < page_arr_size ; i++) {
			page_size = page_arr[i % page_arr_size];
			next_page_size = page_arr[(i + 1) % page_arr_size];

			if (size_allocated + page_size > size)
				page_size = page_arr[0];

			do {
				/* allocate page_size bytes */
				device_addr = gaudi_allocate_device_mem(fd,
					page_size, page_size, CONTIGUOUS);

				if (!device_addr) {
					printf("Failed to allocate %ld bytes of device memory\n",
							page_size);
					rc = -ENOMEM;
					goto exit_alloc;
				}

				device_addr_arr[arr_idx] = (uint64_t)device_addr;
				arr_idx++;

				/* verify contiguous address.
				 * Do the check starting form arr_idx > 1.
				 */
				if (arr_idx > 1
					&& end_device_addr != (uint64_t)device_addr) {
					printf("Failed to alloc contiguous device memory addr\n");
					rc = -ENOMEM;
					goto exit_alloc;
				}

				size_allocated += page_size;
				end_device_addr = (uint64_t)device_addr + page_size;

				if (next_page_size < page_size)
					break;

				/* run as long as the next addr isn't aligned to next_page size */
			} while (size_allocated + page_size < size &&
					!IS_ALIGNED(end_device_addr, next_page_size));

			if (size_allocated == size)
				break;
		}
	}

exit_alloc:
	/* free the array in case of failure */
	if (rc)
		gaudi_free_device_mem_mix_page_size(fd, device_addr_arr, arr_idx);

	hlthunk_free(page_arr);
	*actual_arr_size = arr_idx;
	return rc;
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

uint32_t hltests_add_packet_to_cb(void *ptr, uint32_t offset, void *pkt,
					uint32_t pkt_size)
{
	memcpy((uint8_t *) ptr + offset, pkt, pkt_size);

	return offset + pkt_size;
}

int hltests_get_cb_usage_count(int fd, void *ptr, uint32_t *usage_cnt)
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

	pthread_mutex_unlock(&hdev->cb_table_lock);

	if (cb->cb_type == CB_TYPE_USER)
		return -EINVAL;

	return hlthunk_get_cb_usage_count(fd, cb->cb_handle, usage_cnt);
}

int hltests_fill_cs_chunk(struct hltests_device *hdev,
			struct hl_cs_chunk *chunk, void *cb_ptr,
			uint32_t cb_size, uint32_t queue_index)
{
	struct hltests_cb *cb = NULL;
	khint_t k;

	pthread_mutex_lock(&hdev->cb_table_lock);

	k = kh_get(ptr64, hdev->cb_table, (uint64_t) (uintptr_t) cb_ptr);
	if (k == kh_end(hdev->cb_table)) {
		pthread_mutex_unlock(&hdev->cb_table_lock);

		/* Can't find matching handle so treat this as address */
		chunk->cb_handle = (__u64) cb_ptr;
		goto out;
	}

	cb = kh_val(hdev->cb_table, k);

	pthread_mutex_unlock(&hdev->cb_table_lock);

	chunk->cb_handle = cb->cb_handle;

out:
	chunk->queue_index = queue_index;
	chunk->cb_size = cb_size;
	if (!cb || cb->cb_type == CB_TYPE_USER)
		chunk->cs_chunk_flags |= HL_CS_CHUNK_FLAGS_USER_ALLOC_CB;

	return 0;
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

int hltests_submit_legacy_cs(int fd,
		struct hltests_cs_chunk *restore_arr,
		uint32_t restore_arr_size,
		struct hltests_cs_chunk *execute_arr,
		uint32_t execute_arr_size,
		uint32_t flags,
		uint32_t timeout,
		uint64_t *seq)
{
	struct hltests_device *hdev;
	struct hl_cs_chunk *chunks_restore = NULL, *chunks_execute = NULL;
	struct hlthunk_cs_in cs_in;
	struct hlthunk_cs_out cs_out;
	uint32_t size;
	int rc = 0;

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
	if (timeout)
		rc = hlthunk_command_submission_timeout(fd, &cs_in, &cs_out,
								timeout);
	else
		rc = hlthunk_command_submission(fd, &cs_in, &cs_out);
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

uint32_t hltests_add_nop_pkt(int fd, void *buffer, uint32_t buf_off,
				struct hltests_pkt_info *pkt_info)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->add_nop_pkt(buffer, buf_off, pkt_info);
}

uint32_t hltests_add_msg_barrier_pkt(int fd, void *buffer, uint32_t buf_off,
				struct hltests_pkt_info *pkt_info)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->add_msg_barrier_pkt(buffer, buf_off, pkt_info);
}

uint32_t hltests_add_wreg32_pkt(int fd, void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->add_wreg32_pkt(buffer, buf_off, pkt_info);
}

uint32_t hltests_add_arb_point_pkt(int fd, void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->add_arb_point_pkt(buffer, buf_off, pkt_info);
}

uint32_t hltests_add_msg_long_pkt(int fd, void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->add_msg_long_pkt(buffer, buf_off, pkt_info);
}

uint32_t hltests_add_msg_short_pkt(int fd, void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->add_msg_short_pkt(buffer, buf_off, pkt_info);
}

uint32_t hltests_add_arm_monitor_pkt(int fd, void *buffer,
					uint32_t buf_off,
					struct hltests_pkt_info *pkt_info)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->add_arm_monitor_pkt(buffer, buf_off, pkt_info);
}

uint32_t hltests_add_write_to_sob_pkt(int fd, void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	assert_non_null(asic->add_write_to_sob_pkt);
	return asic->add_write_to_sob_pkt(buffer, buf_off, pkt_info);
}

uint32_t hltests_add_fence_pkt(int fd, void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->add_fence_pkt(buffer, buf_off, pkt_info);
}

uint32_t hltests_add_dma_pkt(int fd, void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->add_dma_pkt(buffer, buf_off, pkt_info);
}

uint32_t hltests_add_cp_dma_pkt(int fd, void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->add_cp_dma_pkt(buffer, buf_off, pkt_info);
}

uint32_t hltests_add_cb_list_pkt(int fd, void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->add_cb_list_pkt(buffer, buf_off, pkt_info);
}

uint32_t hltests_add_load_and_exe_pkt(int fd, void *buffer, uint32_t buf_off,
					struct hltests_pkt_info *pkt_info)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->add_load_and_exe_pkt(buffer, buf_off, pkt_info);
}

uint32_t hltests_add_monitor_and_fence(int fd, void *buffer, uint32_t buf_off,
		struct hltests_monitor_and_fence *mon_and_fence_info)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->add_monitor_and_fence(fd, DCORE_MODE_FULL_CHIP, buffer,
					buf_off, mon_and_fence_info);
}

uint32_t hltests_add_monitor(int fd, void *buffer, uint32_t buf_off,
		struct hltests_monitor *mon_info)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->add_monitor(buffer, buf_off, mon_info);
}

uint64_t hltests_get_fence_addr(int fd, uint32_t qid, bool cmdq_fence)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_fence_addr(fd, qid, cmdq_fence);
}

uint32_t hltests_add_arb_en_pkt(int fd, void *buffer, uint32_t buf_off,
		struct hltests_pkt_info *pkt_info,
		struct hltests_arb_info *arb_info,
		uint32_t queue_id, bool enable)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->add_arb_en_pkt(buffer, buf_off, pkt_info,
			arb_info, queue_id, enable);
}

uint32_t hltests_add_cq_config_pkt(int fd, void *buffer, uint32_t buf_off,
		struct hltests_cq_config *cq_config)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->add_cq_config_pkt(buffer, buf_off, cq_config);
}

uint32_t hltests_add_pdma_ch_bw_config_pkt(int fd, void *buffer, uint32_t buf_off,
		int qid, bool set_lbw)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->add_pdma_ch_bw_config_pkt(fd, buffer, buf_off, qid, set_lbw);
}

uint32_t gaudi_get_dma_down_qid(int fd, enum hltests_stream_id stream)
{
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_dma_down_qid(fd, DCORE_MODE_FULL_CHIP, stream);
}

uint32_t hltests_get_dma_up_qid(int fd, enum hltests_stream_id stream)
{
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_dma_up_qid(fd, DCORE_MODE_FULL_CHIP, stream);
}

uint32_t hltests_get_ddma_qid(int fd, int dma_ch, enum hltests_stream_id stream)
{
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_ddma_qid(fd, DCORE_MODE_FULL_CHIP, dma_ch, stream);
}

uint8_t hltests_get_ddma_cnt(int fd)
{
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_ddma_cnt(fd, DCORE_MODE_FULL_CHIP);
}

uint32_t hltests_get_pdma_qid(int fd, int dma_idx)
{
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_pdma_qid(fd, dma_idx);
}

uint8_t hltests_get_pdma_ch_cnt(int fd)
{
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_pdma_ch_cnt(fd, DCORE_MODE_FULL_CHIP);
}

uint32_t hltests_pdma_config_ch_blocks(int fd)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	return asic->pdma_config_ch_blocks(fd, &hdev->pdma_db);
}

uint32_t hltests_get_tpc_qid(int fd, uint8_t tpc_id,
				enum hltests_stream_id stream)
{
	const struct hltests_asic_funcs *asic = get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_tpc_qid(fd, DCORE_MODE_FULL_CHIP, tpc_id, stream);
}

uint32_t hltests_get_mme_id(int fd, uint32_t qid)
{
	const struct hltests_asic_funcs *asic = get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_mme_id(fd, qid);
}

uint32_t hltests_get_mme_qid(int fd, uint8_t mme_id,
				enum hltests_stream_id stream)
{
	const struct hltests_asic_funcs *asic = get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_mme_qid(DCORE_MODE_FULL_CHIP, mme_id, stream);
}

uint32_t hltests_get_nic_qid(int fd, uint8_t nic_id,
				enum hltests_stream_id stream)
{
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_nic_qid(DCORE_MODE_FULL_CHIP, nic_id, stream);
}

uint8_t hltests_get_tpc_cnt(int fd)
{
	const struct hltests_asic_funcs *asic = get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_tpc_cnt(fd, DCORE_MODE_FULL_CHIP);
}

uint8_t hltests_get_mme_cnt(int fd, bool master_slave_mode)
{
	const struct hltests_asic_funcs *asic = get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_mme_cnt(fd, DCORE_MODE_FULL_CHIP, master_slave_mode);
}

uint16_t hltests_get_first_avail_sob(int fd)
{
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_first_avail_sob(fd);
}

uint16_t hltests_get_first_avail_mon(int fd)
{
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_first_avail_mon(fd);
}

uint16_t hltests_get_first_avail_cq(int fd)
{
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_first_avail_cq(fd);
}

uint16_t hltests_get_first_avail_interrupt(int fd)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	struct hlthunk_hw_ip_info hw_ip = {};

	hlthunk_get_hw_ip_info(fd, &hw_ip);

	return hw_ip.first_available_interrupt_id + hdev->counters.reserved_interrupts;
}

uint64_t hltests_get_sob_base_addr(int fd)
{
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	assert_non_null(asic->get_sob_base_addr);
	return asic->get_sob_base_addr(fd);
}

uint64_t hltests_get_sob_lbw_offset(int fd, uint32_t sob_idx)
{
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	assert_non_null(asic->get_sob_lbw_offset);
	return asic->get_sob_lbw_offset(fd, sob_idx);
}

uint64_t hltests_get_lbw_base_addr(int fd)
{
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	assert_non_null(asic->get_lbw_base_addr);
	return asic->get_lbw_base_addr(fd);
}

uint64_t hltests_get_any_mappable_hw_block_base_addr(int fd)
{
	const struct hltests_asic_funcs *asic = get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_any_mappable_hw_block_base_addr(fd);
}

uint16_t hltests_get_monitors_cnt_per_dcore(int fd)
{
	const struct hltests_asic_funcs *asic = get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_mon_cnt_per_dcore();
}

int hltests_get_stream_master_qid_arr(int fd, uint32_t **qid_arr)
{
	const struct hltests_asic_funcs *asic = get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_stream_master_qid_arr(qid_arr);
}

void hltests_set_rand_seed(uint32_t val)
{
	cur_seed = val;
	seed(val);
}

uint32_t hltests_rand_u32(void)
{
	uint32_t val;

	pthread_spin_lock(&rand_lock);
	val = rand_u32();
	pthread_spin_unlock(&rand_lock);

	return val;
}

bool hltests_rand_flip_coin(void)
{
	return hltests_rand_u32() & 1;
}

void hltests_fill_rand_values(void *ptr, uint64_t size)
{
	uint64_t rounddown_aligned_size, remainder, i;
	uint32_t *p = ptr, val;

	rounddown_aligned_size = size & ~(sizeof(uint32_t) - 1);
	remainder = size - rounddown_aligned_size;

	for (i = 0 ; i < rounddown_aligned_size ; i += sizeof(uint32_t), p++)
		*p = hltests_rand_u32();

	if (!remainder)
		return;

	val = hltests_rand_u32();
	for (i = 0 ; i < remainder ; i++) {
		((uint8_t *) p)[i] = (uint8_t) (val & 0xff);
		val >>= 8;
	}
}

void hltests_fill_addr_chunk(void *ptr, uint32_t size, uint64_t addr, uint64_t chunk)
{
	uint64_t *p, chunk_template;
	uint32_t i, num_addr;

	p = ptr;
	num_addr = size / sizeof(uint64_t);
	chunk_template = (chunk & 0xffff) << 48;
	/* bits 48-63 holds the chunk index, bits 0-47 the address */
	for (i = 0 ; i < num_addr ; i++, p++) {
		*p = chunk_template | (addr & 0xffffffffffff);
		addr += sizeof(uint64_t);
	}
}

void hltests_fill_seq_values(void *ptr, uint32_t size)
{
	uint32_t i, *p = ptr, rounddown_aligned_size, remainder, val;

	rounddown_aligned_size = size & ~(sizeof(uint32_t) - 1);
	remainder = size - rounddown_aligned_size;

	for (i = 0 ; i < rounddown_aligned_size ; i += sizeof(uint32_t), p++)
		*p = i / 4;

	if (!remainder)
		return;

	val = i / 4;
	for (i = 0 ; i < remainder ; i++) {
		((uint8_t *) p)[i] = (uint8_t) (val & 0xff);
		val >>= 8;
	}
}

static void hltests_endian_swap_16_values(void *ptr, uint32_t size)
{
	uint32_t i, rounddown_aligned_size, remainder;
	uint16_t *p = ptr;

	rounddown_aligned_size = size & ~(sizeof(uint16_t) - 1);
	remainder = size - rounddown_aligned_size;

	for (i = 0 ; i < rounddown_aligned_size ; i += sizeof(uint16_t), p++)
		*p = bswap_16(*p);

	if (!remainder)
		return;

	/* There can be a remainder of only one byte */
	*((uint8_t *) p) = 0;
}

static void hltests_endian_swap_32_values(void *ptr, uint32_t size)
{
	uint32_t i, rounddown_aligned_size, remainder;
	uint32_t *p = ptr, tmp;

	rounddown_aligned_size = size & ~(sizeof(uint32_t) - 1);
	remainder = size - rounddown_aligned_size;

	for (i = 0 ; i < rounddown_aligned_size ; i += sizeof(uint32_t), p++)
		*p = bswap_32(*p);

	if (!remainder)
		return;

	tmp = 0;
	for (i = 0 ; i < remainder ; i++)
		tmp |= ((uint32_t) ((uint8_t *) p)[i]) << (i * 8);

	tmp = bswap_32(tmp);
	for (i = 0 ; i < remainder ; i++)
		((uint8_t *) p)[i] = ((uint8_t *) &tmp)[i];
}

static void hltests_endian_swap_64_values(void *ptr, uint32_t size)
{
	uint32_t i, rounddown_aligned_size, remainder;
	uint64_t *p = ptr, tmp;

	rounddown_aligned_size = size & ~(sizeof(uint64_t) - 1);
	remainder = size - rounddown_aligned_size;

	for (i = 0 ; i < rounddown_aligned_size ; i += sizeof(uint64_t), p++)
		*p = bswap_64(*p);

	if (!remainder)
		return;

	tmp = 0;
	for (i = 0 ; i < remainder ; i++)
		tmp |= ((uint64_t) ((uint8_t *) p)[i]) << (i * 8);

	tmp = bswap_64(tmp);
	for (i = 0 ; i < remainder ; i++)
		((uint8_t *) p)[i] = ((uint8_t *) &tmp)[i];
}

static uint64_t hltests_get_dram_va_hint_mask(int fd)
{
	const struct hltests_asic_funcs *asic =
				get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_dram_va_hint_mask();
}

void hltests_endian_swap_values(void *ptr, uint32_t size,
				enum hltests_endian_swap endian_swap)
{
	switch (endian_swap) {
	case ENDIAN_SWAP_16:
		hltests_endian_swap_16_values(ptr, size);
		break;
	case ENDIAN_SWAP_32:
		hltests_endian_swap_32_values(ptr, size);
		break;
	case ENDIAN_SWAP_64:
		hltests_endian_swap_64_values(ptr, size);
		break;
	default:
		break;
	}
}

int hltests_mem_compare_with_stop(void *ptr1, void *ptr2, uint64_t size,
					bool stop_on_err, uint64_t *offset)
{
	uint64_t *p1 = (uint64_t *) ptr1, *p2 = (uint64_t *) ptr2;
	uint64_t err_cnt = 0, rounddown_aligned_size, remainder, i = 0;
	uint64_t mismatch_off = ULONG_MAX;

	rounddown_aligned_size = size & ~(sizeof(uint64_t) - 1);
	remainder = size - rounddown_aligned_size;

	while (i < rounddown_aligned_size) {
		if (*p1 != *p2) {
			if (mismatch_off == ULONG_MAX)
				mismatch_off = (uint64_t)p1 - (uint64_t)ptr1;
			printf("[%p]: 0x%"PRIx64" <--> [%p]: 0x%"PRIx64"\n",
				p1, *p1, p2, *p2);
			err_cnt++;
		}

		i += sizeof(uint64_t);
		p1++;
		p2++;

		if (stop_on_err && err_cnt >= 10)
			break;
	}

	if (!remainder)
		goto ret_err_cnt;

	for (i = 0 ; i < remainder ; i++) {
		if (((uint8_t *) p1)[i] != ((uint8_t *) p2)[i]) {
			if (mismatch_off == ULONG_MAX)
				mismatch_off = (uint64_t)p1 - (uint64_t)ptr1;
			printf("[%p]: 0x%hhx <--> [%p]: 0x%hhx\n",
				(uint8_t *) p1 + i, ((uint8_t *) p1)[i],
				(uint8_t *) p2 + i, ((uint8_t *) p2)[i]);
			err_cnt++;
		}
	}

ret_err_cnt:
	if (offset)
		*offset = mismatch_off;
	return err_cnt;
}

int hltests_mem_compare(void *ptr1, void *ptr2, uint64_t size)
{
	return hltests_mem_compare_with_stop(ptr1, ptr2, size, true, NULL);
}

int hltests_mem_compare_offset(void *ptr1, void *ptr2, uint64_t size, uint64_t *offset)
{
	return hltests_mem_compare_with_stop(ptr1, ptr2, size, true, offset);
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

int hltests_zero_dram_memory(int fd, uint64_t dst_addr, uint32_t size)
{
	uint64_t host_src_addr;
	void *src_ptr;
	int rc;

	src_ptr = hltests_allocate_host_mem(fd, size, HUGE_MAP);
	assert_non_null(src_ptr);

	memset(src_ptr, 0, size);
	host_src_addr = gaudi_get_device_va_for_host_ptr(fd, src_ptr);

	/* TODO: for gaudi3 we might want to use the memset option of pdma */
	rc = gaudi_dma_transfer(fd, gaudi_get_dma_down_qid(fd, STREAM0),
				EB_FALSE, MB_TRUE, host_src_addr, dst_addr, size,
					DMA_DIR_HOST_TO_DRAM);
	assert_int_equal(rc, 0);

	gaudi_free_host_mem(fd, src_ptr);

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

VOID hltests_dma_dram_frag_mem_test(void **state, uint64_t size)
{
	struct hltests_state *tests_state = (struct hltests_state *) *state;
	uint32_t i, frag_arr_size, page_num, rand;
	struct hlthunk_hw_ip_info *hw_ip;
	int rc, fd = tests_state->fd;
	uint64_t used_page_size;
	void **frag_arr;

	/* Create fragmented device physical memory.
	 * Allocate FRAG_MEM_MULT times more memory in advance and free randomly
	 * the amount of memory required for the test inside this area to create
	 * fragmentation.
	 */

	if (hltests_is_pldm(fd) && size > PLDM_MAX_DMA_SIZE_FOR_TESTING)
		skip();

	if (hltests_is_simulator(fd) && size > SZ_512M)
		skip();

	hw_ip = &tests_state->hw_ip;

	if (!hw_ip->dram_enabled) {
		printf("DRAM is disabled so skipping test\n");
		skip();
	}

	if (hltests_is_simulator(fd) &&
			(FRAG_MEM_MULT * size) > hw_ip->dram_size) {
		printf(
			"SIM's DRAM (%lu[B]) is smaller than required allocation (%lu[B]) so skipping test\n",
			hw_ip->dram_size, FRAG_MEM_MULT * size);
		skip();
	}

	/*
	 * since in this test the device memory is allocated using the default page size
	 * we need to use it to calc number of pages
	 */
	used_page_size = hw_ip->device_mem_alloc_default_page_size;
	if (size < used_page_size) {
		printf("page size %#lx is greater than memory chunk %#lx\n", used_page_size, size);
		skip();
	}
	page_num = size / used_page_size;
	assert_int_not_equal(page_num, 0);
	frag_arr_size = page_num * FRAG_MEM_MULT;
	frag_arr = hlthunk_malloc(frag_arr_size * sizeof(*frag_arr));
	assert_non_null(frag_arr);

	for (i = 0; i < frag_arr_size; i++) {
		frag_arr[i] = gaudi_allocate_device_mem(fd, used_page_size, 0, NOT_CONTIGUOUS);
		assert_non_null(frag_arr[i]);
	}

	i = 0;
	while (i < page_num) {
		rand = hltests_rand_u32() % frag_arr_size;
		while (!frag_arr[rand])
			rand = (rand + 1) % frag_arr_size;
		rc = gaudi_free_device_mem(fd, frag_arr[rand]);
		assert_int_equal(rc, 0);
		frag_arr[rand] = NULL;
		i++;
	}

	hltests_dma_dram_test(state, size);

	for (i = 0; i < frag_arr_size; i++) {
		if (!frag_arr[i])
			continue;
		rc = gaudi_free_device_mem(fd, frag_arr[i]);
		assert_int_equal(rc, 0);
	}
	hlthunk_free(frag_arr);

	END_TEST;
}

VOID hltests_dma_dram_high_mem_test(void **state, uint64_t size)
{
	void *device_addr;
	uint64_t alloc_size;
	struct hltests_state *tests_state = (struct hltests_state *) *state;
	struct hlthunk_hw_ip_info *hw_ip = &tests_state->hw_ip;
	int rc, fd = tests_state->fd;

	/* Allocate half size of device memory so that test allocation
	 * will begin from high memory address
	 */

	if (hltests_is_pldm(fd) && size > PLDM_MAX_DMA_SIZE_FOR_TESTING)
		skip();

	if (hltests_is_simulator(fd) && size > SZ_512M)
		skip();

	if (!hw_ip->dram_enabled) {
		printf("DRAM is disabled so skipping test\n");
		skip();
	}

	alloc_size = hw_ip->dram_size / 2;

	if (hltests_is_simulator(fd) && size > alloc_size) {
		printf(
			"SIM's DRAM (%lu[B]) is smaller than required allocation (%lu[B]) so skipping test\n",
			alloc_size, size);
		skip();
	}

	device_addr = gaudi_allocate_device_mem(fd, alloc_size, 0, NOT_CONTIGUOUS);
	assert_non_null(device_addr);

	hltests_dma_dram_test(state, size);

	rc = gaudi_free_device_mem(fd, device_addr);
	assert_int_equal(rc, 0);

	END_TEST;
}

VOID hltests_dma_test_flags(void **state, bool is_ddr, uint64_t size,
				uint64_t page_size, uint32_t flags)
{
	struct hltests_state *tests_state = (struct hltests_state *) *state;
	struct hlthunk_hw_ip_info *hw_ip = &tests_state->hw_ip;
	void *device_addr, *src_ptr, *dst_ptr;
	uint64_t host_src_addr, host_dst_addr;
	uint32_t dma_dir_down, dma_dir_up;
	bool is_huge = !!((size > SZ_32K) && (size < SZ_1G));
	int rc, fd = tests_state->fd;

	if (hltests_is_pldm(fd) && (size > PLDM_MAX_DMA_SIZE_FOR_TESTING))
		skip();

	/* Sanity and memory allocation */
	if (is_ddr) {
		if (hltests_is_simulator(fd) && size > hw_ip->dram_size) {
			printf(
				"SIM's DRAM (%lu[B]) is smaller than required allocation (%lu[B]) so skipping test\n",
				hw_ip->dram_size, size);
			skip();
		}

		if (!hw_ip->dram_enabled) {
			if (hltests_is_gaudi2(fd)) {
				printf("DRAM disabled, using DRAM on host\n");
			} else {
				printf("DRAM is disabled so skipping test\n");
				skip();
			}
		} else {
			assert_in_range(size, 1, hw_ip->dram_size);
		}

		device_addr = gaudi_allocate_device_mem(fd, size, page_size, NOT_CONTIGUOUS);
		assert_non_null(device_addr);

		dma_dir_down = DMA_DIR_HOST_TO_DRAM;
		dma_dir_up = DMA_DIR_DRAM_TO_HOST;
	} else {
		if (size > hw_ip->sram_size)
			skip();
		device_addr = (void *) (uintptr_t) hw_ip->sram_base_address;

		dma_dir_down = DMA_DIR_HOST_TO_SRAM;
		dma_dir_up = DMA_DIR_SRAM_TO_HOST;
	}

	src_ptr = hltests_allocate_host_mem_aligned_flags(fd, size, is_huge, 0,
								flags);
	assert_non_null(src_ptr);
	hltests_fill_rand_values(src_ptr, size);
	host_src_addr = gaudi_get_device_va_for_host_ptr(fd, src_ptr);

	dst_ptr = hltests_allocate_host_mem_aligned_flags(fd, size, is_huge, 0,
								flags);
	assert_non_null(dst_ptr);
	memset(dst_ptr, 0, size);
	host_dst_addr = gaudi_get_device_va_for_host_ptr(fd, dst_ptr);

	/* DMA: host->device */
	rc = gaudi_dma_transfer(fd, gaudi_get_dma_down_qid(fd, STREAM0),
			EB_FALSE, MB_TRUE, host_src_addr,
			(uint64_t) (uintptr_t) device_addr,
			size, dma_dir_down);
	assert_int_equal(rc, 0);

	/* DMA: device->host */
	rc = gaudi_dma_transfer(fd, hltests_get_dma_up_qid(fd, STREAM0),
			EB_FALSE, MB_TRUE, (uint64_t) (uintptr_t) device_addr,
			host_dst_addr, size, dma_dir_up);
	assert_int_equal(rc, 0);

	/* Compare host memories */
	rc = hltests_mem_compare(src_ptr, dst_ptr, size);
	assert_int_equal(rc, 0);

	/* Cleanup */
	rc = gaudi_free_host_mem(fd, dst_ptr);
	assert_int_equal(rc, 0);
	rc = gaudi_free_host_mem(fd, src_ptr);
	assert_int_equal(rc, 0);

	if (is_ddr) {
		rc = gaudi_free_device_mem(fd, device_addr);
		assert_int_equal(rc, 0);
	}

	END_TEST;
}

VOID hltests_dma_sram_test(void **state, uint64_t size)
{
	END_TEST_FUNC(hltests_dma_test_flags(state, false, size, 0, 0));
}

VOID hltests_dma_dram_test(void **state, uint64_t size)
{
	struct hltests_state *tests_state = (struct hltests_state *) *state;
	uint8_t i, page_size_arr_size;
	int fd = tests_state->fd;
	uint64_t *page_size_arr;

	CALL_HELPER_FUNC(hltests_build_memalloc_page_size_array(fd, &page_size_arr,
									&page_size_arr_size));

	if (!page_size_arr) {
		CALL_HELPER_FUNC(hltests_dma_test_flags(state, true, size, 0, 0));
		EXIT_FROM_TEST;
	}

	for (i = 0; i < page_size_arr_size; i++) {
		verbose_printf("page size: %#lx\n", page_size_arr[i]);
		CALL_HELPER_FUNC(hltests_dma_test_flags(state, true, size, page_size_arr[i], 0));
	}

	hltests_destroy_memalloc_page_size_array(page_size_arr);

	END_TEST;
}

VOID hltests_tdr_deadlock_test(void **state, bool recovery)
{
	struct hltests_state *tests_state = (struct hltests_state *) *state;
	struct hltests_cs_chunk execute_arr;
	struct hltests_pkt_info pkt_info;
	uint64_t seq, expected_events;
	int fd = tests_state->fd, rc;
	uint32_t cb_size;
	void *cb_ptr;

	if (!hltests_is_legacy_mode_enabled(fd)) {
		printf("Test is irrelevant when running in ARC mode, skipping\n");
		skip();
	}

	cb_ptr = hltests_create_cb(fd, SZ_4K, EXTERNAL, 0);
	assert_non_null(cb_ptr);
	cb_size = 0;

	memset(&pkt_info, 0, sizeof(pkt_info));
	pkt_info.eb = EB_FALSE;
	pkt_info.mb = MB_FALSE;
	pkt_info.fence.dec_val = 1;
	pkt_info.fence.gate_val = 1;
	pkt_info.fence.fence_id = 0;
	cb_size = hltests_add_fence_pkt(fd, cb_ptr, cb_size, &pkt_info);

	execute_arr.cb_ptr = cb_ptr;
	execute_arr.cb_size = cb_size;
	execute_arr.queue_index = gaudi_get_dma_down_qid(fd, STREAM0);

	rc = hltests_submit_cs_timeout(fd, NULL, 0, &execute_arr, 1, 0x0,
					TEST_TDR_DEADLOCK_TIMEOUT_SEC, &seq);
	assert_int_equal(rc, 0);

	rc = hltests_wait_for_cs_until_not_busy(fd, seq);
	assert_true(rc == HL_WAIT_CS_STATUS_TIMEDOUT || rc == HL_WAIT_CS_STATUS_ABORTED);

	/* Verify that the expected events are received */
	expected_events = HL_NOTIFIER_EVENT_CS_TIMEOUT | HL_NOTIFIER_EVENT_DEVICE_RESET;
	rc = hltests_wait_for_events(tests_state, 0, expected_events, NULL);
	assert_int_equal(rc, 0);

	/* Cleanup */
	rc = hltests_destroy_cb(fd, cb_ptr);
	assert_int_equal(rc, 0);

	if (!recovery)
		EXIT_FROM_TEST;

	/* Recovery */
	rc = hltests_teardown_and_setup(tests_state);
	assert_int_equal(rc, 0);

	/* Sanity check after the recovery */
	fd = tests_state->fd;
	assert_true(hlthunk_is_device_idle(fd));

	END_TEST;
}

VOID hltests_build_page_size_array(uint64_t **page_size_arr, uint8_t *arr_size, uint64_t bitmask)
{
	int i, set_idx, elem = __builtin_popcountll(bitmask);

	/* the function  should not be called with empty bitmask */
	assert_int_not_equal(elem, 0);

	*page_size_arr = hlthunk_malloc(elem * sizeof(uint64_t));
	assert_non_null(*page_size_arr);

	for (i = 0; i < elem; i++) {
		set_idx = __builtin_ffsll(bitmask) - 1;
		(*page_size_arr)[i] = (1ULL << set_idx);
		bitmask &= ~(*page_size_arr)[i];
	}

	*arr_size = elem;

	END_TEST;
}

void hltests_destroy_page_size_array(uint64_t *page_size_arr)
{
	hlthunk_free(page_size_arr);
}

VOID hltests_build_memalloc_page_size_array(int fd, uint64_t **page_size_arr,
						uint8_t *page_size_arr_size)
{
	uint64_t page_order_bitmask;
	int rc;

	/* if multi page size not supported return NULL in array */
	if (!hltests_is_gaudi3(fd)) {
		*page_size_arr = NULL;
		EXIT_FROM_TEST;
	}

	rc = hlthunk_get_dev_memalloc_page_orders(fd, &page_order_bitmask);
	assert_int_equal(rc, 0);

	END_TEST_FUNC(hltests_build_page_size_array(page_size_arr, page_size_arr_size,
							page_order_bitmask));
}

/* the function nesting redundancy is to preserve the alloc-free mirror */
void hltests_destroy_memalloc_page_size_array(uint64_t *page_size_arr)
{
	hltests_destroy_page_size_array(page_size_arr);
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

int hltests_mmu_hint_address(int fd, uint64_t page_size, uint64_t ref_addr,
			     enum range_type type, bool page_aligned)
{
	uint64_t hint_addr, device_map_addr, device_map_addr_late,
		 device_handle = 0, buf_size = SZ_16K, page_off = 0;
	void *host_ptr = NULL;
	bool is_huge = false;
	int rc = 0, ret;

	/* By default, set hint address to be page aligned */
	hint_addr = (ref_addr & ~hltests_get_dram_va_hint_mask(fd)) +
			ROUND_UP(ref_addr & hltests_get_dram_va_hint_mask(fd),
					page_size);
	if (!page_aligned) {
		/* Set hint address to be non page aligned */
		hint_addr += page_size - 1;
	}

	if (type == HOST_ADDR) {
		is_huge = (page_size == SZ_2M);

		if (is_huge)
			host_ptr = allocate_huge_mem(buf_size);
		else
			host_ptr = hlthunk_malloc(buf_size);

		assert_non_null(host_ptr);

		/* In case of a regular page size, we expect a mapped address
		 * with the same offset as the host address, hence we need to
		 * save it for future comparison with the hint address.
		 */
		page_off = ((uint64_t) host_ptr) & (page_size - 1);
		device_map_addr = hlthunk_host_memory_map(fd, host_ptr,
							  hint_addr, buf_size);
	} else {
		device_handle = hlthunk_device_memory_alloc(fd, buf_size, 0,
							    NOT_CONTIGUOUS,
							    false);
		assert_non_null(device_handle);

		device_map_addr = hlthunk_device_memory_map(fd, device_handle,
							    hint_addr);
	}

	/* The expected behavior is that if hint address is page aligned, it
	 * should be equal to the device mapped address - otherwise not.
	 */
	if ((device_map_addr == hint_addr + page_off) ^ page_aligned) {
		printf("Unexpected result: MMU type %s, page_aligned %u, "
		       "page_off 0x%lx, is_huge %u, ref_addr 0x%lx, "
		       "page_size 0x%lx, hint address 0x%lx, "
		       "device_map_addr 0x%lx\n",
		       type == HOST_ADDR ? "PMMU" : "DMMU", page_aligned,
		       page_off, is_huge, ref_addr, page_size, hint_addr,
		       device_map_addr);
		rc = -1;
	}

	if (type == HOST_ADDR) {
		/* Now when the address is in use, using it as hint will not
		 * work. Validate we get a failure when hint is forced.
		 */
		device_map_addr_late = hlthunk_host_memory_map_flags(
			fd, host_ptr, hint_addr, buf_size, HL_MEM_FORCE_HINT);
		assert_null(device_map_addr_late);
	}

	ret = hlthunk_memory_unmap(fd, device_map_addr);
	assert_int_equal(ret, 0);

	if (type == HOST_ADDR) {
		if (is_huge)
			munmap(host_ptr, buf_size);
		else
			hlthunk_free(host_ptr);
	} else {
		ret = hlthunk_device_memory_free(fd, device_handle);
		assert_int_equal(ret, 0);
	}

	return rc;
}

int hltests_teardown_and_setup(struct hltests_state *tests_state)
{
	int rc, fd = tests_state->fd;
	char pci_bus_id[13];

	rc = hlthunk_get_pci_bus_id_from_fd(fd, pci_bus_id, sizeof(pci_bus_id));
	if (rc) {
		printf("Failed to get the PCI bus ID of the device while recovering (%d)\n", rc);
		goto set_recovery_status_failed;
	}

	rc = hltests_stop_listener_thread(tests_state);
	if (rc)
		printf("Failed to stop listener thread while recovering (%d)\n", rc);

	rc = hltests_teardown_user_engines(tests_state);
	if (rc)
		printf("Failed to tear down user engines while recovering (%d)\n", rc);

	rc = hltests_close(fd);
	if (rc)
		printf("Failed to close the device while recovering (%d)\n", rc);

	fd = tests_state->fd = hltests_open(pci_bus_id);
	if (fd < 0) {
		printf("Failed to reopen the device while recovering (%d)\n", fd);
		rc = fd;
		goto set_recovery_status_failed;
	}

	rc = hltests_setup_user_engines(tests_state);
	if (rc) {
		printf("Failed to setup user engines while recovering (%d)\n", rc);
		goto close_fd;
	}

	/* Events might be received right after the listener thread starts, so clear the recovery
	 * status before starting it and check it again afterwards.
	 */
	hltests_set_recovery_status(tests_state, RECOVERY_STATUS_NOT_REQUIRED);

	rc = hltests_start_listener_thread(tests_state);
	if (rc) {
		printf("Failed to start the listener thread while recovering (%d)\n", rc);
		goto teardown_user_engines;
	}

	if (hltests_get_recovery_status(tests_state) != RECOVERY_STATUS_NOT_REQUIRED) {
		printf("Recovery is still required after performing recovery operations\n");
		rc = -EIO;
		goto teardown_user_engines;
	}

	return 0;

teardown_user_engines:
	hltests_teardown_user_engines(tests_state);
close_fd:
	hltests_close(fd);
set_recovery_status_failed:
	hltests_set_recovery_status(tests_state, RECOVERY_STATUS_FAILED);
	return rc;
}

bool hltests_is_device_idle_and_operational(int fd)
{
	return (hlthunk_is_device_idle(fd) &&
			hlthunk_get_device_status_info(fd) == HL_DEVICE_STATUS_OPERATIONAL);
}

static void hltests_destroy_all_mappings(int fd)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	void *addr;
	size_t size;
	khint_t k;

	khash_t(mapping) * table = hdev->mmap_table;

	pthread_mutex_lock(&hdev->mmap_table_lock);

	for (k = kh_begin(table) ; k != kh_end(table) ; ++k) {
		if (!kh_exist(table, k))
			continue;

		addr = (void *)kh_key(table, k);
		size = kh_val(table, k);

		kh_del(mapping, table, k);
		munmap(addr, size);
	}

	pthread_mutex_unlock(&hdev->mmap_table_lock);
}

static void hltests_destroy_all_cbs(int fd)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	struct hltests_cb *cb;
	khint_t k;

	khash_t(ptr64) * table = hdev->cb_table;

	pthread_mutex_lock(&hdev->cb_table_lock);

	for (k = kh_begin(table) ; k != kh_end(table) ; ++k) {
		if (!kh_exist(table, k))
			continue;

		cb = kh_val(table, k);
		kh_del(ptr64, table, k);

		if (cb->cb_type == CB_TYPE_KERNEL || cb->cb_type == CB_TYPE_KERNEL_MAPPED) {
			hltests_munmap(fd, cb->ptr, cb->cb_size);
			hlthunk_destroy_command_buffer(fd, cb->cb_handle);
		} else {
			gaudi_free_host_mem(fd, cb->ptr);
		}

		hlthunk_free(cb);
	}

	pthread_mutex_unlock(&hdev->cb_table_lock);
}

static void hltests_free_all_device_mem(int fd)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	struct hltests_memory *mem;
	khint_t k;

	khash_t(ptr64) * table = hdev->mem_table_device;

	pthread_mutex_lock(&hdev->mem_table_device_lock);

	for (k = kh_begin(table) ; k != kh_end(table) ; ++k) {
		if (!kh_exist(table, k))
			continue;

		mem = kh_val(table, k);
		kh_del(ptr64, table, k);

		if (mem->is_pool) {
			hdev->asic_funcs->dram_pool_free(hdev, mem->device_virt_addr, mem->size);
		} else {
			hlthunk_memory_unmap(fd, mem->device_virt_addr);

			if (!hdev->sim_dram_on_host)
				hlthunk_device_memory_free(fd, mem->device_handle);
			else
				free(mem->host_ptr);
		}

		hlthunk_free(mem);
	}

	pthread_mutex_unlock(&hdev->mem_table_device_lock);
}

static void hltests_free_all_host_mem(int fd)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	struct hltests_memory *mem;
	khint_t k;

	khash_t(ptr64) * table = hdev->mem_table_host;

	pthread_mutex_lock(&hdev->mem_table_host_lock);

	for (k = kh_begin(table) ; k != kh_end(table) ; ++k) {
		if (!kh_exist(table, k))
			continue;

		mem = kh_val(table, k);
		kh_del(ptr64, table, k);

		hlthunk_memory_unmap(fd, mem->device_virt_addr);

		if (mem->is_huge)
			munmap(mem->host_ptr, mem->size);
		else
			free(mem->host_ptr);

		hlthunk_free(mem);
	}

	pthread_mutex_unlock(&hdev->mem_table_host_lock);
}

static int hltests_recover(struct hltests_state *tests_state)
{
	int fd = tests_state->fd;

	/* Cleanup */
	hltests_destroy_all_cbs(fd);
	hltests_destroy_all_mappings(fd);
	hltests_free_all_device_mem(fd);
	hltests_free_all_host_mem(fd);

	return hltests_teardown_and_setup(tests_state);
}

int hltests_ensure_device_operational(void **state)
{
	struct hltests_state *tests_state = (struct hltests_state *) *state;
	struct hltests_device *hdev;
	uint32_t timeout_locked, i;
	bool need_recover = false;
	int fd = -1, rc;

	/* Check if need to release and reopen the device due to a previous error */
	if (hltests_get_recovery_status(tests_state) == RECOVERY_STATUS_REQUIRED) {
		need_recover = true;
		rc = hltests_recover(tests_state);
		if (rc) {
			printf("Failed to recover from error (%d)\n", rc);
			fd = tests_state->fd;
			goto err_exit;
		}
	}

	fd = tests_state->fd;

	if (hltests_is_device_idle_and_operational(fd))
		return 0;

	hdev = get_hdev_from_fd(fd);
	if (!hdev) {
		printf("Failed to get hdev from file descriptor %d\n", fd);
		goto err_exit;
	}

	timeout_locked = hdev->module_params.timeout_locked;
	if (timeout_locked > 1000)
		timeout_locked = 1000;

	for (i = 0 ; i <= timeout_locked ; i++) {
		sleep(1);
		if (hltests_is_device_idle_and_operational(fd))
			return 0;
	}

err_exit:
	/* If we got here it means that something is broken */
	printf("ERROR! device broken, status = 0x%x, (%s recovery) stop running tests\n",
		hlthunk_get_device_status_info(fd), need_recover ? "after" : "did not need");

	/* The current thread terminates itself, which leads to a full test-suite closure */
	pthread_exit(PTHREAD_CANCELED);
}

void *hltests_mem_pool_init(uint64_t start_addr, uint64_t size, uint8_t order)
{
	struct mem_pool *mem_pool;
	uint64_t page_size;
	int rc;

	if (!(order >= PAGE_SHIFT_4KB && order <= PAGE_SHIFT_16MB))
		return NULL;

	page_size = 1ull << order;

	if (size < page_size) {
		printf("pool size should be at least one order size\n");
		return NULL;
	}

	mem_pool = calloc(1, sizeof(struct mem_pool));
	if (!mem_pool)
		return NULL;

	mem_pool->start = start_addr;
	mem_pool->page_size = page_size;
	mem_pool->pool_npages = (size + (page_size - 1)) >> order;
	mem_pool->pool = calloc(mem_pool->pool_npages, 1);
	if (!mem_pool->pool)
		goto free_struct;

	rc = pthread_mutex_init(&mem_pool->lock, NULL);
	if (rc)
		goto free_pool;

	return mem_pool;

free_pool:
	free(mem_pool->pool);
free_struct:
	free(mem_pool);

	return NULL;
}

void hltests_mem_pool_fini(void *data)
{
	struct mem_pool *mem_pool = (struct mem_pool *) data;

	pthread_mutex_destroy(&mem_pool->lock);
	free(mem_pool->pool);
	free(mem_pool);
}

int hltests_mem_pool_alloc(void *data, uint64_t size, uint64_t *addr)
{
	struct mem_pool *mem_pool = (struct mem_pool *) data;
	uint32_t needed_npages, curr_npages, i, j, k;
	bool found = false;

	needed_npages = (size + mem_pool->page_size - 1) / mem_pool->page_size;

	pthread_mutex_lock(&mem_pool->lock);

	for (i = 0 ; i < mem_pool->pool_npages ; i++) {
		for (j = i, curr_npages = 0 ; j < mem_pool->pool_npages ; j++) {
			if (mem_pool->pool[j]) {
				i = j;
				break;
			}

			curr_npages++;

			if (curr_npages == needed_npages) {
				for (k = i ; k <= j ; k++)
					mem_pool->pool[k] = 1;

				found = true;
				break;
			}
		}

		if (found) {
			/* cast to avoid int overflow */
			*addr = mem_pool->start +
					((uint64_t) i) * mem_pool->page_size;
			break;
		}

	}

	pthread_mutex_unlock(&mem_pool->lock);

	return found ? 0 : -ENOMEM;
}

void hltests_mem_pool_free(void *data, uint64_t addr, uint64_t size)
{
	struct mem_pool *mem_pool = (struct mem_pool *) data;
	uint32_t start_page, npages, i;

	start_page = (addr - mem_pool->start) / mem_pool->page_size;
	npages = (size + mem_pool->page_size - 1) / mem_pool->page_size;

	pthread_mutex_lock(&mem_pool->lock);

	for (i = start_page ; i < (start_page + npages) ; i++)
		mem_pool->pool[i] = 0;

	pthread_mutex_unlock(&mem_pool->lock);
}


#if 0

void hltests_parser(int argc, const char **argv, const char * const *usage,
			unsigned long expected_device_mask
#ifndef HLTESTS_LIB_MODE
			, const struct CMUnitTest * const tests, int num_tests
#endif
			)
{
	struct argparse argparse;
#ifndef HLTESTS_LIB_MODE
	const char *test = NULL;
	int list = 0;
	int i;
#endif
	int prof = 0;

	struct argparse_option options[] = {
		OPT_HELP(),
		OPT_GROUP("Basic options"),
#ifndef HLTESTS_LIB_MODE
		OPT_BOOLEAN('l', "list", &list, "list tests"),
		OPT_BOOLEAN('d', "disabled", &run_disabled_tests, "run disabled tests"),
		OPT_STRING('s', "test", &test, "name of specific test to run"),
		OPT_INTEGER('n', "ndevices", &num_devices, "number of devices"),
#endif
		OPT_BOOLEAN('v', "verbose", &verbose_enabled, "enable verbose"),
		OPT_STRING('p', "pciaddr", &parser_pciaddr, "pci address of device"),
		OPT_STRING('c', "config", &config_filename, "config filename for test(s)"),
		OPT_BOOLEAN('f', "prof", &prof, "enable profiling for test(s)"),
		OPT_INTEGER('i', "interface", &nic_port, "NIC interface/port"),
		OPT_INTEGER('m', "mode", &legacy_mode_enabled, "Legacy mode enabled"),
		OPT_STRING('b', "build_path", &build_path, "Path to build directory"),
		OPT_BOOLEAN('a', "arc-log", &enable_arc_log, "enable logging for arcs"),
		OPT_INTEGER('e', "events-listener", &events_listener,
				"enable driver-events listener thread"),
		OPT_BOOLEAN('x', "neg", &negative_tests, "enable negative tests"),
		OPT_BOOLEAN('g', "mini-suite", &run_mini_suite, "run minimized tests suite"),
		OPT_END(),
	};

	argparse_init(&argparse, options, usage, 0);
	argparse_describe(&argparse, "\nRun tests using hl-thunk", NULL);
	argc = argparse_parse(&argparse, argc, argv);

#ifndef HLTESTS_LIB_MODE
	if (list) {
		printf("\nList of tests:");
		printf("\n-----------------\n\n");
		for (i = 0 ; i < num_tests ; i++)
			printf("%s\n", tests[i].name);
		printf("\n");
		exit(0);
	}

	if (test)
		cmocka_set_test_filter(test);
#endif

	if (prof)
		putenv("HABANA_PROFILE=1");

	asic_mask_for_testing = expected_device_mask;

#ifndef HLTESTS_LIB_MODE
	/*
	 * TODO:
	 * Remove when providing multiple PCI bus addresses is supported.
	 */
	if (num_devices > 1 &&  parser_pciaddr) {
		printf(
			"The '--pciaddr' and '--ndevices' options cannot coexist\n");
		exit(-1);
	}
#endif
}

#endif

uint32_t hltests_get_parser_enable_arc_log(void)
{
	return enable_arc_log;
}

const char *hltests_get_parser_pciaddr(void)
{
	return parser_pciaddr;
}

void hltests_override_parser_pciaddr(const char *pciaddr)
{
	parser_pciaddr = pciaddr;
}

const char *hltests_get_config_filename(void)
{
	return config_filename;
}

int hltests_get_parser_run_disabled_tests(void)
{
#ifndef HLTESTS_LIB_MODE
	return run_disabled_tests;
#else
	return 1;
#endif
}

int hltests_get_verbose_enabled(void)
{
	return verbose_enabled;
}

uint32_t hltests_get_cur_seed(void)
{
	return cur_seed;
}

const char *hltests_get_build_path(void)
{
	return build_path;
}

int hltests_set_build_path(const char *path)
{
	int ret = snprintf(build_path_str, BUILD_PATH_MAX_LENGTH, "%s", path);

	if (ret < 0 || ret >= BUILD_PATH_MAX_LENGTH)
		return -1;

	build_path = build_path_str;

	return 0;
}

int hltests_get_parser_nic_port(void)
{
	return nic_port;
}

int hltests_get_parser_events_listener(void)
{
	return events_listener;
}

uint32_t hltests_get_parser_negative_tests(void)
{
	return negative_tests;
}

uint32_t hltests_get_parser_mini_suite(void)
{
	return run_mini_suite;
}

bool hltests_is_legacy_mode_enabled(int fd)
{
	if (hltests_is_goya(fd) || hltests_is_gaudi(fd))
		return true;

	if (hltests_is_gaudi3(fd))
		return false;

	return !!legacy_mode_enabled;
}

bool hltests_is_simulator(int fd)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);

	if (hdev->device_id == PCI_IDS_GOYA_SIMULATOR ||
		hdev->device_id == PCI_IDS_GAUDI_SIMULATOR ||
		hdev->device_id == PCI_IDS_GAUDI_HL2000M_SIMULATOR ||
		hdev->device_id == PCI_IDS_GAUDI2_SIMULATOR ||
		hdev->device_id == PCI_IDS_GAUDI2_ARC_SIMULATOR ||
		hdev->device_id == PCI_IDS_GAUDI2B_SIMULATOR ||
		hdev->device_id == PCI_IDS_GAUDI2B_ARC_SIMULATOR ||
		hdev->device_id == PCI_IDS_GAUDI3_SIMULATOR ||
		hdev->device_id == PCI_IDS_GAUDI3_ARC_SIMULATOR ||
		hdev->device_id == PCI_IDS_GAUDI3_HL_338_SIMULATOR ||
		hdev->device_id == PCI_IDS_GAUDI3_HL_338_ARC_SIMULATOR)
		return true;

	return false;
}

bool hltests_is_goya(int fd)
{
	return (hlthunk_get_device_name_from_fd(fd) == HLTHUNK_DEVICE_GOYA);
}

bool hltests_is_gaudi(int fd)
{
	enum hlthunk_device_name device;

	device = hlthunk_get_device_name_from_fd(fd);
	if ((device == HLTHUNK_DEVICE_GAUDI) ||
			(device == HLTHUNK_DEVICE_GAUDI_HL2000M))
		return true;

	return false;
}

bool hltests_is_gaudi2(int fd)
{
	enum hlthunk_device_name device;

	device = hlthunk_get_device_name_from_fd(fd);
	if ((device == HLTHUNK_DEVICE_GAUDI2) ||
			(device == HLTHUNK_DEVICE_GAUDI2B) || (device == HLTHUNK_DEVICE_GAUDI2C) ||
			(device == HLTHUNK_DEVICE_GAUDI2D))
		return true;

	return false;
}

bool hltests_is_gaudi3(int fd)
{
	enum hlthunk_device_name device;

	device = hlthunk_get_device_name_from_fd(fd);
	if (device == HLTHUNK_DEVICE_GAUDI3 || device == HLTHUNK_DEVICE_GAUDI3D)
		return true;

	return false;
}

bool hltests_is_gaudi_family(int fd)
{
	return (hltests_is_gaudi(fd) || hltests_is_gaudi2(fd) || hltests_is_gaudi3(fd));
}

bool hltests_is_pldm(int fd)
{
	struct hltests_module_params_info module_params;
	int rc;

	rc = hltests_get_module_params_info(fd, &module_params);
	assert_int_equal(rc, 0);

	return !!module_params.pldm;
}

VOID test_sm_pingpong_common_cp(void **state, bool is_tpc,
				bool common_cb_in_host, uint8_t engine_id)
{
	struct hltests_state *tests_state = (struct hltests_state *) *state;
	void *host_src, *host_dst, *engine_common_cb, *engine_upper_cb = NULL, *restore_cb,
			*dmadown_cb, *dmaup_cb, *dram_ptr = NULL;
	uint64_t seq = 0, host_src_device_va, host_dst_device_va, device_data_address = 0x0,
			engine_common_cb_address = 0x0, engine_common_cb_device_va = 0x0,
			engine_upper_cb_address = 0x0, engine_upper_cb_device_va = 0x0;
	uint32_t engine_qid, dma_size, engine_common_cb_size, engine_upper_cb_size = 0,
			restore_cb_size, dmadown_cb_size, dmaup_cb_size,
			dma_dir_down = DMA_DIR_HOST_TO_SRAM, dma_dir_up = DMA_DIR_SRAM_TO_HOST;
	struct hltests_cs_chunk restore_arr[1], execute_arr[3];
	struct hltests_monitor_and_fence mon_and_fence_info;
	uint16_t sob[2], mon[2], dma_down_qid, dma_up_qid;
	struct hltests_pkt_info pkt_info;
	struct hlthunk_hw_ip_info *hw_ip = &tests_state->hw_ip;
	int rc, fd = tests_state->fd;

	/* Test Description:
	 * - First DMA QMAN transfers data from host to device and then signals SOB0.
	 * - Engine QMAN processes CP_DMA packet and transfers internal CB to CMDQ.
	 * - Engine CMDQ fences on SOB0, processes NOP packet, and then signals SOB8.
	 * - Second DMA QMAN fences on SOB1 and then transfers data from device to host.
	 * - Setup CB is used to clear SOB0/1 and to DMA the internal CBs to device.
	 *
	 * NOTE:
	 * The engine's common CB can be located on the host, depending on the "common_cb_in_host"
	 * flag.
	 */

	/* Check conditions if CB is in the host */
	if (common_cb_in_host) {
		/* This test can't run on Goya */
		if (hltests_is_goya(fd)) {
			printf("Test is skipped. Goya's common CP can't be in host\n");
			skip();
		}
	}

	if (is_tpc)
		engine_qid = hltests_get_tpc_qid(fd, engine_id, STREAM0);
	else
		engine_qid = hltests_get_mme_qid(fd, engine_id, STREAM0);

	dma_down_qid = gaudi_get_dma_down_qid(fd, STREAM0);
	dma_up_qid = hltests_get_dma_up_qid(fd, STREAM0);

	/* Device addresses for data and engine's CB */
	if (hw_ip->sram_size) {
		device_data_address = hw_ip->sram_base_address + 0x1000;
		engine_upper_cb_address = hw_ip->sram_base_address + 0x2000;
		engine_common_cb_address = hw_ip->sram_base_address + 0x3000;
	} else if (hw_ip->dram_enabled) {
		dram_ptr = gaudi_allocate_device_mem(fd, 0x3000, 0, CONTIGUOUS);
		assert_non_null(dram_ptr);
		device_data_address = (uint64_t) (uintptr_t) dram_ptr;
		engine_upper_cb_address = device_data_address + 0x1000;
		engine_common_cb_address = device_data_address + 0x2000;
		dma_dir_down = DMA_DIR_HOST_TO_DRAM;
		dma_dir_up = DMA_DIR_DRAM_TO_HOST;
	} else {
		printf("No device memory is available so skipping test\n");
		skip();
	}

	dma_size = 4;

	/* Allocate two buffers on the host for data transfers */
	host_src = hltests_allocate_host_mem(fd, dma_size, NOT_HUGE_MAP);
	assert_non_null(host_src);
	hltests_fill_rand_values(host_src, dma_size);
	host_src_device_va = gaudi_get_device_va_for_host_ptr(fd, host_src);

	host_dst = hltests_allocate_host_mem(fd, dma_size, NOT_HUGE_MAP);
	assert_non_null(host_dst);
	memset(host_dst, 0, dma_size);
	host_dst_device_va = gaudi_get_device_va_for_host_ptr(fd, host_dst);

	sob[0] = hltests_get_first_avail_sob(fd);
	sob[1] = hltests_get_first_avail_sob(fd) + 1;
	mon[0] = hltests_get_first_avail_mon(fd);
	mon[1] = hltests_get_first_avail_mon(fd) + 1;

	/* Allocate memory on host for the common CB. Either the ASIC will fetch it directly
	 * from host, or we will download it to the device and run it from there.
	 */
	if (hltests_is_legacy_mode_enabled(fd)) {
		engine_common_cb = hltests_allocate_host_mem(fd, 0x1000, NOT_HUGE_MAP);
		assert_non_null(engine_common_cb);
		memset(engine_common_cb, 0, 0x1000);
		engine_common_cb_device_va = gaudi_get_device_va_for_host_ptr(fd,
								engine_common_cb);
	} else {
		engine_common_cb = hltests_create_cb(fd, 0x1000, EXTERNAL, 0);
	}

	engine_common_cb_size = 0;
	memset(&mon_and_fence_info, 0, sizeof(mon_and_fence_info));
	mon_and_fence_info.queue_id = engine_qid;
	mon_and_fence_info.cmdq_fence = true;
	mon_and_fence_info.sob_id = sob[0];
	mon_and_fence_info.mon_id = mon[0];
	mon_and_fence_info.mon_address = 0;
	mon_and_fence_info.sob_val = 1;
	mon_and_fence_info.dec_fence = true;
	mon_and_fence_info.mon_payload = 1;
	mon_and_fence_info.mon_mode = SOB_EQUAL;
	engine_common_cb_size = hltests_add_monitor_and_fence(fd,
							engine_common_cb,
							engine_common_cb_size,
							&mon_and_fence_info);

	memset(&pkt_info, 0, sizeof(pkt_info));
	pkt_info.qid = engine_qid;
	pkt_info.eb = EB_FALSE;
	pkt_info.mb = MB_TRUE;
	engine_common_cb_size = hltests_add_nop_pkt(fd, engine_common_cb,
							engine_common_cb_size,
							&pkt_info);
	memset(&pkt_info, 0, sizeof(pkt_info));
	pkt_info.qid = engine_qid;
	pkt_info.eb = EB_FALSE;
	pkt_info.mb = MB_FALSE;
	pkt_info.write_to_sob.sob_id = sob[0] + 1;
	pkt_info.write_to_sob.value = 1;
	pkt_info.write_to_sob.mode = SOB_ADD;
	engine_common_cb_size = hltests_add_write_to_sob_pkt(fd,
							engine_common_cb,
							engine_common_cb_size,
							&pkt_info);

	/* Upper CB for engine: CP_DMA */
	if (hltests_is_legacy_mode_enabled(fd)) {
		engine_upper_cb = hltests_create_cb(fd, SZ_4K, INTERNAL, engine_upper_cb_address);
		assert_non_null(engine_upper_cb);
		engine_upper_cb_device_va = gaudi_get_device_va_for_host_ptr(fd, engine_upper_cb);
		engine_upper_cb_size = 0;

		pkt_info.eb = EB_FALSE;
		pkt_info.mb = MB_FALSE;
		if (common_cb_in_host)
			pkt_info.cp_dma.src_addr = engine_common_cb_device_va;
		else
			pkt_info.cp_dma.src_addr = engine_common_cb_address;
		pkt_info.cp_dma.size = engine_common_cb_size;
		engine_upper_cb_size = hltests_add_cp_dma_pkt(fd, engine_upper_cb,
							engine_upper_cb_size,
							&pkt_info);
	}

	hltests_clear_sobs(fd, 2);

	/* Setup CB: DMA the internal CBs to SRAM */
	restore_cb =  hltests_create_cb(fd, SZ_4K, EXTERNAL, 0);
	assert_non_null(restore_cb);
	restore_cb_size = 0;

	if (!common_cb_in_host) {
		memset(&pkt_info, 0, sizeof(pkt_info));
		pkt_info.qid = dma_down_qid;
		pkt_info.eb = EB_FALSE;
		pkt_info.mb = MB_TRUE;
		pkt_info.dma.src_addr = engine_common_cb_device_va;
		pkt_info.dma.dst_addr = engine_common_cb_address;
		pkt_info.dma.size = engine_common_cb_size;
		pkt_info.dma.dma_dir = dma_dir_down;
		restore_cb_size = hltests_add_dma_pkt(fd, restore_cb,
						restore_cb_size, &pkt_info);
	}

	if (hltests_is_legacy_mode_enabled(fd)) {
		memset(&pkt_info, 0, sizeof(pkt_info));
		pkt_info.eb = EB_FALSE;
		pkt_info.mb = MB_TRUE;
		pkt_info.dma.src_addr = engine_upper_cb_device_va;
		pkt_info.dma.dst_addr = engine_upper_cb_address;
		pkt_info.dma.size = engine_upper_cb_size;
		pkt_info.dma.dma_dir = dma_dir_down;
		restore_cb_size = hltests_add_dma_pkt(fd, restore_cb, restore_cb_size,
							&pkt_info);
	}

	/* CB for first DMA QMAN:
	 * Transfer data from host to SRAM + signal SOB0.
	 */
	dmadown_cb = hltests_create_cb(fd, SZ_4K, EXTERNAL, 0);
	assert_non_null(dmadown_cb);
	dmadown_cb_size = 0;

	memset(&pkt_info, 0, sizeof(pkt_info));
	pkt_info.qid = dma_down_qid;
	pkt_info.eb = EB_FALSE;
	pkt_info.mb = MB_FALSE;
	pkt_info.dma.src_addr = host_src_device_va;
	pkt_info.dma.dst_addr = device_data_address;
	pkt_info.dma.size = dma_size;
	pkt_info.dma.dma_dir = dma_dir_down;
	dmadown_cb_size = hltests_add_dma_pkt(fd, dmadown_cb, dmadown_cb_size,
						&pkt_info);
	memset(&pkt_info, 0, sizeof(pkt_info));
	pkt_info.qid = dma_down_qid;
	pkt_info.eb = EB_TRUE;
	pkt_info.mb = MB_FALSE;
	pkt_info.write_to_sob.sob_id = sob[0];
	pkt_info.write_to_sob.value = 1;
	pkt_info.write_to_sob.mode = SOB_ADD;
	dmadown_cb_size = hltests_add_write_to_sob_pkt(fd, dmadown_cb,
						dmadown_cb_size, &pkt_info);

	/* CB for second DMA QMAN:
	 * Fence on SOB1 + transfer data from SRAM to host.
	 */
	dmaup_cb = hltests_create_cb(fd, SZ_4K, EXTERNAL, 0);
	assert_non_null(dmaup_cb);
	dmaup_cb_size = 0;
	memset(&mon_and_fence_info, 0, sizeof(mon_and_fence_info));
	mon_and_fence_info.queue_id = dma_up_qid;
	mon_and_fence_info.cmdq_fence = false;
	mon_and_fence_info.sob_id = sob[0] + 1;
	mon_and_fence_info.mon_id = mon[0] + 1;
	mon_and_fence_info.mon_address = 0;
	mon_and_fence_info.sob_val = 1;
	mon_and_fence_info.dec_fence = true;
	mon_and_fence_info.mon_payload = 1;
	mon_and_fence_info.mon_mode = SOB_EQUAL;
	dmaup_cb_size = hltests_add_monitor_and_fence(fd, dmaup_cb,
				dmaup_cb_size, &mon_and_fence_info);

	memset(&pkt_info, 0, sizeof(pkt_info));
	pkt_info.qid = dma_up_qid;
	pkt_info.eb = EB_FALSE;
	pkt_info.mb = MB_TRUE;
	pkt_info.dma.src_addr = device_data_address;
	pkt_info.dma.dst_addr = host_dst_device_va;
	pkt_info.dma.size = dma_size;
	pkt_info.dma.dma_dir = dma_dir_up;
	dmaup_cb_size = hltests_add_dma_pkt(fd, dmaup_cb, dmaup_cb_size,
								&pkt_info);

	/* Submit CS and wait for completion */
	restore_arr[0].cb_ptr = restore_cb;
	restore_arr[0].cb_size = restore_cb_size;
	restore_arr[0].queue_index = dma_down_qid;

	execute_arr[0].cb_ptr = dmadown_cb;
	execute_arr[0].cb_size = dmadown_cb_size;
	execute_arr[0].queue_index = dma_down_qid;

	execute_arr[1].cb_ptr = hltests_is_legacy_mode_enabled(fd) ?
					engine_upper_cb : engine_common_cb;
	execute_arr[1].cb_size = hltests_is_legacy_mode_enabled(fd) ?
					engine_upper_cb_size : engine_common_cb_size;
	execute_arr[1].queue_index = engine_qid;

	execute_arr[2].cb_ptr = dmaup_cb;
	execute_arr[2].cb_size = dmaup_cb_size;
	execute_arr[2].queue_index = dma_up_qid;

	rc = hltests_submit_cs(fd, restore_arr, restore_cb_size ? 1 : 0,
				execute_arr, 3, HL_CS_FLAGS_FORCE_RESTORE, &seq);
	assert_int_equal(rc, 0);

	rc = hltests_wait_for_cs_until_not_busy(fd, seq);
	assert_int_equal(rc, HL_WAIT_CS_STATUS_COMPLETED);

	/* Compare host memories */
	rc = hltests_mem_compare(host_src, host_dst, dma_size);
	assert_int_equal(rc, 0);

	/* Cleanup */
	if (hltests_is_legacy_mode_enabled(fd)) {
		rc = hltests_destroy_cb(fd, engine_upper_cb);
		assert_int_equal(rc, 0);
	}

	rc = hltests_destroy_cb(fd, restore_cb);
	assert_int_equal(rc, 0);
	rc = hltests_destroy_cb(fd, dmadown_cb);
	assert_int_equal(rc, 0);
	rc = hltests_destroy_cb(fd, dmaup_cb);
	assert_int_equal(rc, 0);

	if (hltests_is_legacy_mode_enabled(fd))
		rc = gaudi_free_host_mem(fd, engine_common_cb);
	else
		rc = hltests_destroy_cb(fd, engine_common_cb);
	assert_int_equal(rc, 0);

	rc = gaudi_free_host_mem(fd, host_dst);
	assert_int_equal(rc, 0);
	rc = gaudi_free_host_mem(fd, host_src);
	assert_int_equal(rc, 0);

	if (dram_ptr) {
		rc = gaudi_free_device_mem(fd, dram_ptr);
		assert_int_equal(rc, 0);
	}

	END_TEST;
}

int hltests_clear_sobs_offset(int fd, uint16_t num_of_sobs, uint16_t offset)
{
	const struct hltests_asic_funcs *asic = get_hdev_from_fd(fd)->asic_funcs;
	uint16_t first_sob = hltests_get_first_avail_sob(fd);
	uint32_t cb_offset = 0, i, dma_qid;
	struct hltests_pkt_info pkt_info;
	int rc;
	void *cb;

	if (asic->get_sob_value) {
		assert_non_null(asic->set_sob_value);

		for (i = 0; i < num_of_sobs; i++) {
			rc = asic->set_sob_value(fd, first_sob + offset + i, 0);
			assert_int_equal(rc, 0);
		}

		/* Flush writes */
		asic->get_sob_value(fd, first_sob + offset + num_of_sobs);

		return 0;
	}

	/* Use control block approach if we can't directly read/write SOBs */
	cb = hltests_create_cb(fd, HL_MAX_CB_SIZE, EXTERNAL, 0);
	assert_non_null(cb);

	dma_qid = gaudi_get_dma_down_qid(fd, STREAM0);

	memset(&pkt_info, 0, sizeof(pkt_info));
	pkt_info.qid = dma_qid;
	pkt_info.eb = EB_FALSE;
	pkt_info.mb = MB_FALSE;
	pkt_info.write_to_sob.value = 0;
	pkt_info.write_to_sob.mode = SOB_SET;
	for (i = first_sob ; i < (first_sob + offset + num_of_sobs - 1) ; i++) {
		pkt_info.write_to_sob.sob_id = i;
		cb_offset = hltests_add_write_to_sob_pkt(fd, cb, cb_offset, &pkt_info);
	}
	/* Message Barrier should be true only in the last packet */
	pkt_info.write_to_sob.sob_id = i;
	pkt_info.mb = MB_TRUE;
	cb_offset = hltests_add_write_to_sob_pkt(fd, cb, cb_offset, &pkt_info);

	hltests_submit_and_wait_cs(fd, cb, cb_offset, dma_qid,
			DESTROY_CB_TRUE, HL_WAIT_CS_STATUS_COMPLETED);

	return 0;
}

void hltests_clear_sobs(int fd, uint16_t num_of_sobs)
{
	hltests_clear_sobs_offset(fd, num_of_sobs, 0);
}

void *hltests_map_hw_block(int fd, uint64_t block_addr, uint32_t *block_size)
{
	uint64_t handle;
	void *ptr;
	int rc;

	if (hltests_is_simulator(fd)) {
		*block_size = 0;
		return (void *) block_addr;
	}

	rc = hlthunk_get_hw_block(fd, block_addr, block_size, &handle);
	if (rc) {
		printf(
			"Failed to retrieve a HW block handle [block_address 0x%"PRIx64", rc %d]\n",
			block_addr, rc);
		return NULL;
	}

	ptr = hltests_mmap(fd, *block_size, handle);
	if (ptr == MAP_FAILED) {
		printf(
			"Failed to mmap a HW block handle [block_address 0x%"PRIx64"]\n",
			block_addr);
		ptr = NULL;
	}

	return ptr;
}

int hltests_unmap_hw_block(int fd, void *host_addr, uint32_t block_size)
{
	if (hltests_is_simulator(fd))
		return 0;

	return hltests_munmap(fd, host_addr, block_size);
}

static int hltests_sim_read_from_lbw_mem(int fd, void *dst, uint64_t src,
						uint32_t size)
{
	struct hl_debug_params_mem_access mem_access;
	struct hl_debug_args debug;

	memset(&mem_access, 0, sizeof(mem_access));
	mem_access.cfg_address = src;
	mem_access.user_address = (uint64_t) (uintptr_t) dst;
	mem_access.size = size;

	memset(&debug, 0, sizeof(debug));
	debug.input_ptr = (uint64_t) (uintptr_t) &mem_access;
	debug.input_size = sizeof(mem_access);
	debug.op = HL_DEBUG_OP_READMEM;

	return hlthunk_debug(fd, &debug);
}

static int hltests_sim_write_to_lbw_mem(int fd, uint64_t dst, void *src,
					uint32_t size)
{
	struct hl_debug_params_mem_access mem_access;
	struct hl_debug_args debug;

	memset(&mem_access, 0, sizeof(mem_access));
	mem_access.cfg_address = dst;
	mem_access.user_address = (uint64_t) (uintptr_t) src;
	mem_access.size = size;

	memset(&debug, 0, sizeof(debug));
	debug.input_ptr = (uint64_t) (uintptr_t) &mem_access;
	debug.input_size = sizeof(mem_access);
	debug.op = HL_DEBUG_OP_MEMCPY;

	return hlthunk_debug(fd, &debug);
}

int hltests_read_lbw_mem(int fd, void *dst, void *src, uint32_t size)
{
	int num_of_regs, i;
	uint32_t *d, *s;

	/* LBW access must be aligned to 32 bits*/
	if (size % sizeof(uint32_t) != 0)
		return -EINVAL;

	if (hltests_is_simulator(fd))
		return hltests_sim_read_from_lbw_mem(fd, dst,
				(uint64_t) (uintptr_t) src, size);

	num_of_regs = size / sizeof(uint32_t);
	d = dst;
	s = src;

	for (i = 0; i < num_of_regs; i++, d++, s++)
		*d = *s;

	return 0;
}

int hltests_write_lbw_mem(int fd, void *dst, void *src, uint32_t size)
{
	int num_of_regs, i;
	uint32_t *d, *s;

	/* LBW access must be aligned to 32 bits*/
	if (size % sizeof(uint32_t) != 0)
		return -EINVAL;

	if (hltests_is_simulator(fd))
		return hltests_sim_write_to_lbw_mem(fd,
					(uint64_t) (uintptr_t) dst, src, size);

	_mm_sfence();
	num_of_regs = size / sizeof(uint32_t);
	d = dst;
	s = src;

	for (i = 0; i < num_of_regs; i++, d++, s++)
		*d = *s;

	return 0;
}

int hltests_read_lbw_reg(int fd, void *src, uint32_t *value)
{
	return hltests_read_lbw_mem(fd, value, src, sizeof(*value));
}

int hltests_write_lbw_reg(int fd, void *dst, uint32_t value)
{
	return hltests_write_lbw_mem(fd, dst, &value, sizeof(value));
}

uint32_t next_pow2(uint32_t v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;

	return v;
}

double get_bw_gigabyte_per_sec(uint64_t bytes, struct timespec *begin,
							struct timespec *end)
{
	/*
	 * calculation conforms to GB definition:
	 * 1 GB = 1000000000 bytes (= 1000^3 B = 10^9 B)
	 */
	return ((double)(bytes) / get_timediff_sec(begin, end)) /
						(1000 * 1000 * 1000);
}

double get_bw_gigabit_from_timesync(uint64_t total_size, struct hlthunk_time_sync_info *begin,
					struct hlthunk_time_sync_info *end)
{
		double timediff_dev = (end->device_time - begin->device_time) / PSOC_FREQ_GHZ;

		/* in case of wraparound return the result to a positive value. */
		if (timediff_dev < 0)
			timediff_dev *= -1;
		return total_size * 8 / timediff_dev;
}

int hltests_get_max_pll_idx(int fd)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_max_pll_idx();
}

const char *hltests_stringify_pll_idx(int fd, uint32_t pll_idx)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->stringify_pll_idx(pll_idx);
}

const char *hltests_stringify_pll_type(int fd, uint32_t pll_idx,
				uint8_t type_idx)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->stringify_pll_type(pll_idx, type_idx);
}

int hltests_device_memory_export_dmabuf_fd(int fd, void *device_addr, uint64_t size,
						uint64_t offset)
{
	uint64_t addr = (uint64_t) (uintptr_t) device_addr;
	struct hltests_device *hdev;

	hdev = get_hdev_from_fd(fd);
	assert_non_null(hdev);

	if (hltests_is_gaudi(fd)) {
		assert_int_equal(offset, 0);
		return hlthunk_device_memory_export_dmabuf_fd(fd, addr, size, 0);
	}

	return hlthunk_device_mapped_memory_export_dmabuf_fd(fd, addr, size, offset,
								O_RDWR | O_CLOEXEC);
}

int hltest_get_host_meminfo(struct hltest_host_meminfo *res)
{
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int n_fields = 0;

	fp = fopen("/proc/meminfo", "r");
	if (!fp)
		return -1;
	memset(res, 0, sizeof(*res));

	while ((read = getline(&line, &len, fp)) != -1) {
		if (sscanf(line, "MemTotal: %lu", &res->mem_total) == 1)
			n_fields++;
		if (sscanf(line, "MemFree: %lu", &res->mem_free) == 1)
			n_fields++;
		if (sscanf(line, "MemAvailable: %lu", &res->mem_available) == 1)
			n_fields++;
		if (sscanf(line, "HugePages_Total: %lu",
			&res->hugepage_total) == 1)
			n_fields++;
		if (sscanf(line, "HugePages_Free: %lu", &res->hugepage_free) ==
		    1)
			n_fields++;
		if (sscanf(line, "Hugepagesize: %lu", &res->hugepage_size) == 1)
			n_fields++;
	}
	if (n_fields != 6)
		return -1;
	res->mem_total *= 1024;
	res->mem_free *= 1024;
	res->mem_available *= 1024;
	res->hugepage_size *= 1024;
	res->page_size = getpagesize();
	if (line)
		free(line);
	fclose(fp);
	return 0;
}

uint16_t hltests_get_cache_line_size(int fd)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_cache_line_size();
}

uint64_t hltests_get_tc_base_addr(int fd, uint32_t core_id)
{
	const struct hltests_asic_funcs *asic = get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_tc_base_addr(core_id);
}

int hltests_get_async_event_id(int fd, enum hltests_async_event_id hl_tests_event_id,
				uint32_t *asic_event_id)
{
	const struct hltests_asic_funcs *asic = get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_async_event_id(hl_tests_event_id, asic_event_id);
}

uint32_t hltests_get_cq_patch_size(int fd, uint32_t qid)
{
	const struct hltests_asic_funcs *asic = get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_cq_patch_size(qid);
}

uint32_t hltests_get_max_pkt_size(int fd, bool mb, bool eb, uint32_t qid)
{
	const struct hltests_asic_funcs *asic = get_hdev_from_fd(fd)->asic_funcs;

	return asic->get_max_pkt_size(fd, mb, eb, qid);
}

uint32_t hltests_add_direct_write_cq_pkt(int fd, void *buffer, uint32_t buf_off,
					struct hltests_direct_cq_write *pkt_info)
{
	const struct hltests_asic_funcs *asic =
			get_hdev_from_fd(fd)->asic_funcs;

	return asic->add_direct_write_cq_pkt(fd, buffer, buf_off, pkt_info);
}

static void *hltests_monitor_dma_thread(void *data)
{
	const struct hltests_asic_funcs *asic;
	struct monitor_dma_test *params;

	params = (struct monitor_dma_test *)data;
	asic = get_hdev_from_fd(params->fd)->asic_funcs;

	asic->monitor_dma_test_progress(params);

	return data;
}

void hltests_monitor_dma_start(struct monitor_dma_test *params, int fd, uint32_t qid,
				int poll_interval_sec)
{
	const struct hltests_asic_funcs *asic = get_hdev_from_fd(fd)->asic_funcs;
	int rc;

	if (!asic->monitor_dma_test_progress)
		return;

	memset(params, 0, sizeof(*params));
	params->fd = fd;
	params->qid = qid;
	params->poll_interval_sec = poll_interval_sec;
	pthread_cond_init(&params->cond, NULL);
	pthread_mutex_init(&params->mutex, NULL);

	rc = pthread_create(&params->tid, NULL, hltests_monitor_dma_thread, params);
	if (rc)
		printf("DMA monitor pthread_create error: %d\n", rc);
}

void hltests_monitor_dma_stop(struct monitor_dma_test *params)
{
	const struct hltests_asic_funcs *asic = get_hdev_from_fd(params->fd)->asic_funcs;
	int rc;

	if (!asic->monitor_dma_test_progress)
		return;

	/* signal, by means of condition variable, to the mon thread to stop  */
	pthread_mutex_lock(&params->mutex);
	pthread_cond_signal(&params->cond);
	pthread_mutex_unlock(&params->mutex);

	pthread_mutex_destroy(&params->mutex);
	pthread_cond_destroy(&params->cond);

	rc = pthread_join(params->tid, NULL);
	if (rc)
		printf("DMA monitor thread join error: %d\n", rc);
}

void hltests_set_capabilities_mask(const uint64_t new_mask)
{
	hltests_capabilities_mask = new_mask;
}

uint64_t hltests_get_capabilities_mask(void)
{
	return hltests_capabilities_mask;
}

int hltests_mme_dma_init(int fd)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	const struct hltests_asic_funcs *asic = hdev->asic_funcs;

	if (asic->mme_dma_init)
		return asic->mme_dma_init(fd);
	return 0;
}

int hltests_is_mme_dma_enabled(int fd)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);

	return hdev->mme_dma_info.enable;
}

uint32_t hltests_get_mme_dma_cb_size(int fd, int num_of_lindma_pkts, uint32_t size)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	const struct hltests_asic_funcs *asic = hdev->asic_funcs;

	return asic->get_mme_dma_cb_size(fd, num_of_lindma_pkts, size);
}

uint32_t hltests_prepare_mme_dma_req(int fd, void *cb, uint32_t cb_size, uint64_t src,
					uint64_t dst, uint8_t mme_idx, uint64_t size)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	const struct hltests_asic_funcs *asic = hdev->asic_funcs;

	return asic->prepare_mme_dma_req(fd, cb, cb_size, src, dst, mme_idx, size);
}

bool can_open_debugfs(bool dbg_print)
{
	if (!access("/sys/kernel/debug", R_OK))
		return true;

	if (dbg_print)
		printf("Cannot access debugfs, maybe need to run with sudo?\n");

	return false;
}

uint32_t hltests_add_cq_db_set_cq_queue_pkt(int fd, void *buffer, uint32_t buf_off,
					struct hltests_cq_config *cq_config, uint16_t sob_id)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	const struct hltests_asic_funcs *asic = hdev->asic_funcs;

	return asic->add_cq_db_set_cq_queue_pkt(buffer, buf_off, cq_config, sob_id);
}

uint64_t hltests_get_tpc_intr_cause_reg(int fd, uint64_t tpc_idx)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	const struct hltests_asic_funcs *asic = hdev->asic_funcs;

	return asic->get_tpc_intr_cause_reg(tpc_idx);
}

uint64_t hltests_get_razwi_addr(int fd, enum err_trigger type)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	const struct hltests_asic_funcs *asic = hdev->asic_funcs;

	return asic->get_razwi_addr(type);
}

int hltests_edp_get_engines_list(int fd, uint32_t **engine_ids, uint32_t *engine_ids_size)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	const struct hltests_asic_funcs *asic = hdev->asic_funcs;

	return asic->edp_get_engines_list(fd, engine_ids, engine_ids_size);
}

uint64_t hltests_get_fw_mem_addr(int fd, uint64_t size)
{
	struct hltests_device *hdev = get_hdev_from_fd(fd);
	const struct hltests_asic_funcs *asic = hdev->asic_funcs;

	return asic->get_fw_mem_addr(size);
}

int verbose_printf(const char *format, ...)
{
	va_list args;
	int ret;

	if (!hltests_get_verbose_enabled())
		return 0;

	va_start(args, format);
	ret = printf(format, args);
	va_end(args);

	return ret;
}






//* end of hlthunk_tests */





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
                             hltests_get_dma_up_qid(fd, STREAM0),
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