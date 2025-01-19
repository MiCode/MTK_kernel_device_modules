/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 ARM Ltd.
 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef _TRUSTY_PRIVATE_H
#define _TRUSTY_PRIVATE_H

#include <linux/types.h>

struct trusty_work {
	struct trusty_state *s;
	unsigned int cpu;
	struct task_struct *nop_thread;
	wait_queue_head_t nop_event_wait;
	int signaled;
};

struct trusty_call_ops {
	const struct trusty_transport_desc *desc;
	u32 (*trusty_std_call)(struct device *dev, unsigned long fid,
			       unsigned long a0, unsigned long a1,
			       unsigned long a2);
	u32 (*trusty_fast_call)(struct device *dev, unsigned long fid,
				unsigned long a0, unsigned long a1,
				unsigned long a2);
};

struct trusty_mem_ops {
	const struct trusty_transport_desc *desc;
	int (*trusty_share_memory)(struct device *dev, u64 *id,
				   struct scatterlist *sglist,
				   unsigned int nents, pgprot_t pgprot, u64 tag);
	int (*trusty_lend_memory)(struct device *dev, u64 *id,
				  struct scatterlist *sglist,
				  unsigned int nents, pgprot_t pgprot, u64 tag);
	int (*trusty_reclaim_memory)(struct device *dev, u64 id,
				     struct scatterlist *sglist,
				     unsigned int nents);
};

struct trusty_state {
	struct mutex smc_lock;
	struct atomic_notifier_head notifier;
	struct completion cpu_idle_completion;
	char *version_str;
	u32 api_version;
	bool trusty_panicked;
	struct device *dev;
	struct hlist_node cpuhp_node;
	struct trusty_work __percpu *nop_works;
	struct list_head nop_queue;
	spinlock_t nop_lock; /* protects nop_queue */
	struct device_dma_parameters dma_parms;
	struct trusty_sched_share_state *trusty_sched_share_state;
#ifndef CONFIG_TRUSTY_FFA_TRANSPORT
	void *ffa_tx;
	void *ffa_rx;
	u16 ffa_local_id;
	u16 ffa_remote_id;
	struct mutex share_memory_msg_lock; /* protects share_memory_msg */
#endif
	const struct trusty_call_ops *call_ops;
	const struct trusty_mem_ops *mem_ops;
	struct trusty_ffa_state *ffa;
};

struct trusty_ffa_state {
	struct device *dev; /* ffa device */
#ifdef CONFIG_TRUSTY_FFA_TRANSPORT
	const struct ffa_ops *ops;
#else
	const struct ffa_dev_ops *ops;
#endif
	struct mutex share_memory_msg_lock; /* protects share_memory_msg */
};

struct trusty_transport_desc {
	const char *name;
	int (*setup)(struct device *dev);
	void (*cleanup)(struct device *dev);
};

int trusty_init_api_version(struct trusty_state *s, struct device *dev,
			    u32 (*trusty_fast_call)(struct device *dev,
						    unsigned long fid,
						    unsigned long a0,
						    unsigned long a1,
						    unsigned long a2));

#ifdef CONFIG_TRUSTY_SMC_TRANSPORT
extern const struct trusty_transport_desc trusty_smc_transport;
#endif
#ifdef CONFIG_TRUSTY_FFA_TRANSPORT
extern const struct trusty_transport_desc trusty_ffa_transport;
#endif

#endif /* _TRUSTY_PRIVATE_H */
