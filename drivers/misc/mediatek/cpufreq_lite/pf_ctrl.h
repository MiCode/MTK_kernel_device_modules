// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#define MT_PREFETCH_SMC_ACT_SET		(1<<0UL)
#define MT_PREFETCH_SMC_ACT_CLR		(1<<1UL)
#define MT_PREFETCH_SMC_ACT_GET		(1<<2UL)
#define MT_PREFETCH_SMC_MAGIC		(0xDA000000)
#define CPUECTLR_EL1_PREFETCH_MASK	(0x1U << 21)
#define MS_TO_NS			(1000000L)
#define PF_MIN_INTERVAL			(300)   // 0.3s
#define PF_MAX_INTERVAL			(100000) // 100s
#define COREL_NUM			4
#define PF_IPC_CIRC_BUF_SIZE		256

struct pf_info {
	u64 pf_ts[COREL_NUM], pf_off_total_time;
	bool pf_set[COREL_NUM], pf_get[COREL_NUM];
};
struct pf_work_struct {
	struct work_struct work;
	int pf_type;
	int cpu;
};
enum pf_work_type {
	PF_DISABLE,
	PF_ENABLE,
};

struct pf_ipc_record {
	unsigned long long cycle, inst;
	unsigned int ipc;
	bool pf_dis;
};

struct pf_ipc_buf {
	struct pf_ipc_record *buf;
	int head;
	int tail;
};

int mtk_pf_ctrl_init(void);
void mtk_pf_ctrl_exit(void);
bool mtk_get_pf_ctrl_enable(void);
int mtk_set_pf_ctrl_enable(bool enable);
