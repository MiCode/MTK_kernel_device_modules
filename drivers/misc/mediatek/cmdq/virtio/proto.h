/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __VIRTIO_MTK_PROTO_H__
#define __VIRTIO_MTK_PROTO_H__

#include <linux/types.h>
#include <linux/notifier.h>
#include <linux/mailbox/mtk-cmdq-mailbox-ext.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#define RESPONSE_DATA_SIZE	8

#define MAX_CMDQ_IOV_SIZE (35)

#define util_time_to_us(start, end, duration)	\
{	\
	u64 _duration = end - start;	\
	do_div(_duration, 1000);	\
	duration = (s32)_duration;	\
}

enum {
	REQ_MBOX,
	REQ_IPI,
	REQ_CLK,
};

enum {
	MCUPM_MBOX,
	SSPM_MBOX,
};

enum {
	MBOX_OPS_READ,
	MBOX_OPS_WRITE,
	MBOX_OPS_GET_IRQSTATE,
};

enum {
	IPI_OPS_SEND,
	IPI_OPS_SEND_COMP,
};

enum {
	CLK_OPS_PREPARE,
	CLK_OPS_UNPREPARE,
	CLK_OPS_ENABLE,
	CLK_OPS_DISABLE,
	CLK_OPS_SET_RATE,
	CLK_OPS_SET_PARENT,
};

enum {
	CMDQ_OPS_FLUSH,
	CMDQ_OPS_WAIT_COMPLETE,
	CMDQ_OPS_DESTROY,
	CMDQ_OPS_CHAN_STOP,
	CMDQ_OPS_MBOX_ENABLE,
	CMDQ_OPS_MBOX_DISABLE,
#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
	CMDQ_OPS_ALLOC_BUF,
#endif
};

enum {
	VCORE_DVFS_TYPE_UNKNOWN,
	VCORE_DVFS_TYPE_ADD_REQUEST,
	VCORE_DVFS_TYPE_UPDATE_REQUEST,
	VCORE_DVFS_TYPE_REMOVE_REQUEST,
};

struct mbox_req {
	uint8_t mbox_type;
	uint8_t ops;
	uint8_t mbox_id;
	uint8_t slot;
	uint8_t pin_index;
};

struct ipi_req {
	uint8_t mbox_type;
	uint8_t ipi_id;
	uint8_t ops;
	uint8_t opt;
	uint32_t timeout_ms;
};

#define CLK_NAME_MAX_LEN 64

struct clk_req {
	char name[CLK_NAME_MAX_LEN];
	char parent_name[CLK_NAME_MAX_LEN];
	uint8_t ops;
	uint64_t arg1;
	uint64_t arg2;
};

struct mtk_request {
	uint8_t type;
	union {
		struct mbox_req mbox_req;
		struct ipi_req ipi_req;
		struct clk_req clk_req;
	};
};

struct mtk_response {
	int32_t ret;
	uint32_t payload_len;
	uint64_t payload[RESPONSE_DATA_SIZE];
};

struct cmdq_flush_request {
	uint32_t thread_id;
	uint64_t key;
	uint32_t avail_buf_size;
	uint32_t cmd_buf_size;
	uint32_t buf_size;
	uint32_t priority;
	uint32_t hwid;
	bool loop;
	uint iov_len;
	uint64_t cmd_buf_paddrs[MAX_CMDQ_IOV_SIZE];
	uint64_t cmd_buf_iovaddrs[MAX_CMDQ_IOV_SIZE];
	uint64_t cmd_buf_ids[MAX_CMDQ_IOV_SIZE];
	struct iovec iov[MAX_CMDQ_IOV_SIZE];
};

#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
struct cmdq_alloc_buf_request {
	uint64_t key;
	void *data_buf;
	uint32_t data_size;
	uint32_t hwid;
};
#endif

struct cmdq_wait_complete_request {
	uint64_t key;
	uint32_t hwid;
};

struct cmdq_destroy_request {
	uint64_t key;
	uint32_t hwid;
};

struct cmdq_chan_stop_request {
	uint32_t thread_id;
	uint32_t hwid;
};

struct cmdq_mbox_enable_request {
	uint32_t thread_id;
	uint32_t hwid;
};

struct cmdq_mbox_disable_request {
	uint32_t thread_id;
	uint32_t hwid;
};

struct cmdq_task_complete_event {
	uint64_t key;
	uint64_t exec_time;
	int32_t result;
	bool wfe;
	uint16_t event;
	size_t off;
};

struct cmdq_request {
	uint8_t type;
	union {
		struct cmdq_flush_request flush_req;
		struct cmdq_wait_complete_request wait_req;
		struct cmdq_destroy_request destroy_req;
		struct cmdq_chan_stop_request chan_stop_req;
		struct cmdq_mbox_enable_request mbox_enable_req;
		struct cmdq_mbox_disable_request mbox_disable_req;
#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
		struct cmdq_alloc_buf_request buf_req;
#endif
	};
};

struct cmdq_response {
	int32_t ret;
	u64 init_time;
	u64 copy_time;
	u64 handle_time;
#if IS_ENABLED(CONFIG_VHOST_CMDQ_DMA_MAP)
	u64 iova;
	u64 pa;
#endif
};

struct client_record {
	uint64_t key;
	struct cmdq_pkt *pkt;
	struct list_head list_entry;
	cmdq_async_flush_cb cb;
	void *data;
	struct completion done;
	int host_result;  // host cmdq exec result.
	struct work_struct err_cb_work;
	bool user_err_dump;
};

struct cmdq_util_platform_fp;
void virtio_cmdq_util_set_fp(struct cmdq_util_platform_fp *cust_cmdq_platform);

#define VIRTIO_CMDQ_IRQ_TRACE_ID 0x00CCBBAA

extern int virtio_cmdq_trace;
#define VIRTIO_CMDQ_DRV_TRACE_FORCE_BEGIN_TID(tid, fmt, args...) \
	virtio_cmdq_print_trace("B|%d|" fmt "\n", tid, ##args) \

#define VIRTIO_CMDQ_DRV_TRACE_FORCE_END_TID(tid, fmt, args...) \
	virtio_cmdq_print_trace("E|%d|" fmt "\n", tid, ##args) \

#define virtio_cmdq_trace_begin(fmt, args...) do { \
	if (virtio_cmdq_trace) { \
		preempt_disable(); \
		VIRTIO_CMDQ_DRV_TRACE_FORCE_BEGIN_TID(VIRTIO_CMDQ_IRQ_TRACE_ID, fmt, ##args); \
		preempt_enable(); \
	} \
} while (0)

#define virtio_cmdq_trace_end(fmt, args...) do { \
	if (virtio_cmdq_trace) { \
		preempt_disable(); \
		VIRTIO_CMDQ_DRV_TRACE_FORCE_END_TID(VIRTIO_CMDQ_IRQ_TRACE_ID, fmt, ##args); \
		preempt_enable(); \
	} \
} while (0)

#if IS_ENABLED(CONFIG_CMDQ_MBRAIN)
#define CMDQ_INFO_LENGTH 1024
struct cmdq_mbrain_latency_data {
	struct work_struct cmdq_mb_notify;
	char mbrain[CMDQ_INFO_LENGTH];
};

enum cmdq_mb_event {
	CMDQ_LATENCY_TO_MB = 0,
	MAX_EVENT_TO_MB,
};

int virtio_cmdq_mb_register(struct notifier_block *nb);
int virtio_cmdq_mb_unregister(struct notifier_block *nb);
void virtio_cmdq_mb_record(struct cmdq_pkt *pkt);
#endif

u32 *virtio_cmdq_pkt_get_perf_ret(struct cmdq_pkt *pkt);
void virtio_cmdq_print_trace(char *fmt, ...);
#endif
