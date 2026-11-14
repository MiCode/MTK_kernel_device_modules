/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _SHARE_MEMORY_H_
#define _SHARE_MEMORY_H_

#include "scp.h"

enum share_mem_payload_type {
	SHARE_MEM_DATA_PAYLOAD_TYPE,
	SHARE_MEM_SUPER_DATA_PAYLOAD_TYPE,
	SHARE_MEM_LIST_PAYLOAD_TYPE,
	SHARE_MEM_DEBUG_PAYLOAD_TYPE,
	SHARE_MEM_CUSTOM_W_PAYLOAD_TYPE,
	SHARE_MEM_CUSTOM_R_PAYLOAD_TYPE,
	MAX_SHARE_MEM_PAYLOAD_TYPE,
};

struct share_mem_data {
	uint8_t sensor_type;
	uint8_t action;
	uint8_t accurancy;
	uint8_t padding[1];
	int64_t timestamp;
	int32_t value[6] __aligned(4);
} __packed __aligned(4);

struct share_mem_super_data {
	uint8_t sensor_type;
	uint8_t action;
	uint8_t accurancy;
	uint8_t padding[1];
	int64_t timestamp;
	int32_t value[16] __aligned(4);
} __packed __aligned(4);

struct share_mem_debug {
	uint8_t sensor_type;
	uint8_t padding[3];
	uint32_t written;
	uint8_t buffer[4032] __aligned(4); //2048+1024+512+256+128+64
} __packed __aligned(4);

struct share_mem_info {
	uint8_t sensor_type;
	uint8_t padding[3];
	uint32_t gain;
	uint8_t name[16];
	uint8_t vendor[16];
} __packed __aligned(4);

struct share_mem_cmd {
	uint8_t command;
	uint8_t tx_len;
	uint8_t rx_len;
	uint8_t padding[1];
	int32_t data[15] __aligned(4);
} __packed __aligned(4);

struct share_mem_base {
	uint32_t rp;
	uint32_t wp;
	uint32_t buffer_size;
	uint32_t item_size;
	uint8_t data[] __aligned(4);
} __packed __aligned(4);

struct share_mem {
	struct share_mem_base *base;

	struct mutex lock;
	uint32_t item_size;

	uint32_t write_position;
	uint32_t last_write_position;

	bool buffer_full_detect;
	uint8_t buffer_full_cmd;
	uint32_t buffer_full_written;
	uint32_t buffer_full_threshold;

	char *name;
};

struct share_mem_notify {
	uint8_t sequence;
	uint8_t sensor_type;
	uint8_t notify_cmd;
};

struct share_mem_config {
	uint8_t payload_type;
	struct share_mem_base *base;
	uint32_t buffer_size;
};

int share_mem_seek(struct share_mem *shm, uint32_t write_position);
int share_mem_read_reset(struct share_mem *shm);
int share_mem_write_reset(struct share_mem *shm);
int share_mem_read(struct share_mem *shm, void *buf, uint32_t count);
int share_mem_write(struct share_mem *shm, void *buf, uint32_t count);
int share_mem_flush(struct share_mem *shm, struct share_mem_notify *notify);
int share_mem_init(struct share_mem *shm, struct share_mem_config *cfg);
int share_mem_config(void);
void share_mem_config_handler_register(uint8_t payload_type,
	int (*f)(struct share_mem_config *cfg, void *private_data),
	void *private_data);
void share_mem_config_handler_unregister(uint8_t payload_type);

#define SHARE_BUFFER_CHN_BITS   (4)
#define SHARE_BUFFER_CMD_BITS   (4)

enum share_buffer_mem_chn {
	SHARE_BUFFER_COMMON_CHN,
	SHARE_BUFFER_CUST_CMD_CHN,
	MAX_SHARE_BUFFER_CHN,
};
static_assert(MAX_SHARE_BUFFER_CHN < (1 << SHARE_BUFFER_CHN_BITS));

enum share_buffer_common_command {
	SHARE_BUFFER_LIST_CMD,
	SHARE_BUFFER_DEBUG_CMD,
	SHARE_BUFFER_CUSTOM_DATA_CMD,
	MAX_SHARE_BUFFER_COMMON_CMD,
};
static_assert(MAX_SHARE_BUFFER_COMMON_CMD < (1 << SHARE_BUFFER_CMD_BITS));

extern struct share_buffer_comm *common_sbc;

struct share_buffer {
	uint32_t total_size;
	uint32_t buffer_size;
	uint32_t *head_magic;
	void *buffer;
	uint32_t *tail_magic;
	bool inited;
};

struct share_buffer_header {
	uint8_t sequence;
	uint8_t sensor_type;
	uint8_t channel : SHARE_BUFFER_CHN_BITS, command : SHARE_BUFFER_CMD_BITS;
	uint8_t sub_command;
	uint32_t tx_len;
	uint32_t rx_len;
} __packed __aligned(4);

struct share_buffer_mem {
	uint32_t crc;
	struct share_buffer_header header;
	uint32_t length;
	uint8_t data[] __aligned(4);
} __packed __aligned(4);

struct share_buffer_comm {
	struct mutex lock;
	struct completion done;
	uint8_t channel;
	uint8_t max_cmd;
	phys_addr_t tx_addr;
	uint32_t tx_size;
	phys_addr_t rx_addr;
	uint32_t rx_size;
	struct share_buffer sb_tx;
	struct share_buffer sb_rx;
};

static inline bool share_buffer_enabled(void)
{
	return (get_scp_dram_region_manage() == 1) ? true : false;
}

void share_buffer_init(struct share_buffer *sb, phys_addr_t addr, uint32_t size);
int share_buffer_write(struct share_buffer *sb,
		uint32_t offset, void *data, uint32_t length);
int share_buffer_read(struct share_buffer *sb,
		uint32_t offset, void *data, uint32_t length);

int share_buffer_comm_with(struct share_buffer_comm *sbc,
		int sensor_type, uint8_t command, uint8_t sub_command,
		void *tx_buf, uint32_t tx_len,
		void *rx_buf, uint32_t rx_len);
struct share_buffer_comm *share_buffer_comm_get(uint8_t channel);
int share_buffer_comm_init(struct share_buffer_comm *sbc);
int share_buffer_comm_plat_init(void);
void share_buffer_comm_plat_exit(void);

#endif
