// SPDX-License-Identifier: GPL-2.0
/*
 * mtk_freq_qos.h - Freq QoS debug Driver
 *
 * Copyright (c) 2023 MediaTek Inc.
 * Chung-kai Yang <Chung-kai.Yang@mediatek.com>
 */

#include <linux/kallsyms.h>
#include <linux/pm_qos.h>
#include <linux/types.h>
#include <linux/list.h>

struct mtk_freq_qos_data {
	struct freq_qos_request *req;
	struct hlist_node req_node;
	unsigned long addr;
	char caller_info[KSYM_SYMBOL_LEN];
	s32 min_value;
	s32 max_value;
	unsigned long last_update;
	unsigned int cpu;
};

struct mtk_freq_qos_req {
	struct mtk_freq_qos_data *last_min_req;
	struct mtk_freq_qos_data *min_dominant;
	struct mtk_freq_qos_data *last_max_req;
	struct mtk_freq_qos_data *max_dominant;
	s32 min_value;
	s32 max_value;
	struct notifier_block nb_min;
	struct notifier_block nb_max;
	unsigned int policy_idx;
};

struct control_mapping {
	int master;
	int policy_idx;
};

enum mtk_freq_qos_record_type {
	FREQ_QOS_ADD,
	FREQ_QOS_UPDATE,
	FREQ_QOS_REMOVE,
};

struct mtk_freq_qos_record {
	char caller_info[KSYM_SYMBOL_LEN];
	s32 min_value;
	s32 max_value;
	unsigned long ts;
	unsigned int cpu;
	unsigned int type;
};

struct mtk_freq_qos_circ_buf {
	struct mtk_freq_qos_record *buf;
	int head;
	int tail;
};

void mtk_freq_qos_add_request(void *data, struct freq_constraints *qos,
	struct freq_qos_request *req, enum freq_qos_req_type type,
	int value, int ret);
void mtk_freq_qos_update_request(void *data, struct freq_qos_request *req, int value);
void mtk_freq_qos_remove_request(void *data, struct freq_qos_request *req);
void dump_list(void);
int mtk_freq_qos_init(void);

#define FREQ_QOS_HT_SHIFT_BIT    6 /* 64 */
#define FREQ_QOS_HT_SZ           BIT(FREQ_QOS_HT_SHIFT_BIT)
#define MAX_NR_POLICY            8
#define FREQ_QOS_CIRC_BUF_SIZE   64
