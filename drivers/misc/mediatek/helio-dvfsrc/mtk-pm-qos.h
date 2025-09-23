/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef _MTK_PM_QOS_H
#define _MTK_PM_QOS_H
/* interface for the pm_qos_power infrastructure of the linux kernel.
 *
 * Mark Gross <mgross@linux.intel.com>
 */
#include <linux/plist.h>
#include <linux/notifier.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/cpumask.h>
#include <linux/interrupt.h>
#include <linux/pm_qos.h>


enum {
	MTK_PM_QOS_RESERVED = 0,
	// PM_QOS_CPU_DMA_LATENCY,
	// PM_QOS_NETWORK_LATENCY,
	// PM_QOS_NETWORK_THROUGHPUT,
	MTK_PM_QOS_MEMORY_BANDWIDTH,

	MTK_PM_QOS_CPU_MEMORY_BANDWIDTH,
	MTK_PM_QOS_GPU_MEMORY_BANDWIDTH,
	MTK_PM_QOS_MM_MEMORY_BANDWIDTH,
	MTK_PM_QOS_MD_PERI_MEMORY_BANDWIDTH,
	MTK_PM_QOS_OTHER_MEMORY_BANDWIDTH,
	MTK_PM_QOS_MM0_BANDWIDTH_LIMITER,
	MTK_PM_QOS_MM1_BANDWIDTH_LIMITER,

	MTK_PM_QOS_DDR_OPP,
	MTK_PM_QOS_EMI_OPP,
	MTK_PM_QOS_VCORE_OPP,
	MTK_PM_QOS_VCORE_DVFS_FIXED_OPP,
	MTK_PM_QOS_SCP_VCORE_REQUEST,
	MTK_PM_QOS_POWER_MODEL_DDR_REQUEST,
	MTK_PM_QOS_POWER_MODEL_VCORE_REQUEST,
	MTK_PM_QOS_VCORE_DVFS_FORCE_OPP,

	MTK_PM_QOS_DISP_FREQ,
	MTK_PM_QOS_MDP_FREQ,
	MTK_PM_QOS_VDEC_FREQ,
	MTK_PM_QOS_VENC_FREQ,
	MTK_PM_QOS_IMG_FREQ,
	MTK_PM_QOS_CAM_FREQ,
	MTK_PM_QOS_DPE_FREQ,
	MTK_PM_QOS_ISP_HRT_BANDWIDTH,
	MTK_PM_QOS_APU_MEMORY_BANDWIDTH,
	MTK_PM_QOS_VVPU_OPP,
	/* insert new class ID */
	MTK_PM_QOS_NUM_CLASSES,
};

// #define MTK_PM_QOS_CPU_DMA_LAT_DEFAULT_VALUE	(2000 * USEC_PER_SEC)
// #define MTK_PM_QOS_NETWORK_LAT_DEFAULT_VALUE	(2000 * USEC_PER_SEC)
// #define MTK_PM_QOS_NETWORK_THROUGHPUT_DEFAULT_VALUE	0
#define PM_QOS_DEFAULT_VALUE	(-1)
#define MTK_PM_QOS_MEMORY_BANDWIDTH_DEFAULT_VALUE	0
#define MTK_PM_QOS_CPU_MEMORY_BANDWIDTH_DEFAULT_VALUE	0
#define MTK_PM_QOS_GPU_MEMORY_BANDWIDTH_DEFAULT_VALUE	0
#define MTK_PM_QOS_MM_MEMORY_BANDWIDTH_DEFAULT_VALUE	0
#define MTK_PM_QOS_MD_PERI_MEMORY_BANDWIDTH_DEFAULT_VALUE	0
#define MTK_PM_QOS_OTHER_MEMORY_BANDWIDTH_DEFAULT_VALUE	0
#define MTK_PM_QOS_MM_BANDWIDTH_LIMITER_DEFAULT_VALUE	0
#define MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE			16
#define MTK_PM_QOS_EMI_OPP_DEFAULT_VALUE	16
#define MTK_PM_QOS_VCORE_OPP_DEFAULT_VALUE	16
#define MTK_PM_QOS_VCORE_DVFS_FIXED_OPP_DEFAULT_VALUE	16
#define MTK_PM_QOS_SCP_VCORE_REQUEST_DEFAULT_VALUE		0
#define MTK_PM_QOS_POWER_MODEL_DDR_REQUEST_DEFAULT_VALUE	0
#define MTK_PM_QOS_POWER_MODEL_VCORE_REQUEST_DEFAULT_VALUE	0
#define MTK_PM_QOS_VCORE_DVFS_FORCE_OPP_DEFAULT_VALUE	32
#define MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE		0
#define MTK_PM_QOS_ISP_HRT_BANDWIDTH_DEFAULT_VALUE         0
#define MTK_PM_QOS_APU_MEMORY_BANDWIDTH_DEFAULT_VALUE      0
#define MTK_PM_QOS_VVPU_OPP_DEFAULT_VALUE			3
// #define MTK_PM_QOS_FLAG_REMOTE_WAKEUP	(1 << 1)

struct mtk_pm_qos_object {
	struct mtk_pm_qos_constraints *constraints;
	// struct mutex qos_lock;
	struct list_head req_list;
	char *name;
};

struct mtk_pm_qos_request {
	struct list_head list_node;
	struct plist_node node;
	int pm_qos_class;
	char owner[20];
};

enum mtk_pm_qos_type {
	MTK_PM_QOS_UNITIALIZED,
	MTK_PM_QOS_MAX,		/* return the largest value */
	MTK_PM_QOS_MIN,		/* return the smallest value */
	MTK_PM_QOS_SUM		/* return the sum */
};

/*
 * Note: The lockless read path depends on the CPU accessing target_value
 * or effective_flags atomically.  Atomic access is only guaranteed on all CPU
 * types linux supports for 32 bit quantites
 */
struct mtk_pm_qos_constraints {
	struct list_head req_list;
	struct plist_head list;
	s32 target_value;	/* Do not change to 64 bit */
	s32 target_per_cpu[NR_CPUS];
	s32 default_value;
	s32 no_constraint_value;
	enum mtk_pm_qos_type type;
	struct mutex qos_lock;
	struct blocking_notifier_head *notifiers;
};

struct mtk_pm_qos_flags {
	struct list_head list;
	s32 effective_flags;	/* Do not change to 64 bit */
};

int mtk_pm_qos_update_target(struct mtk_pm_qos_constraints *c, struct plist_node *node,
			enum pm_qos_req_action action, int value);

void mtk_pm_qos_add_request(struct mtk_pm_qos_request *req, int pm_qos_class,
		s32 value);
void mtk_pm_qos_update_request(struct mtk_pm_qos_request *req,
		s32 new_value);
void mtk_pm_qos_remove_request(struct mtk_pm_qos_request *req);

int mtk_pm_qos_request(int pm_qos_class);
int mtk_pm_qos_add_notifier(int pm_qos_class, struct notifier_block *notifier);
int mtk_pm_qos_remove_notifier(int pm_qos_class, struct notifier_block *notifier);
int mtk_pm_qos_request_active(struct mtk_pm_qos_request *req);
s32 mtk_pm_qos_read_value(struct mtk_pm_qos_constraints *c);

void mtk_pm_qos_plist_add(struct plist_node *node, struct plist_head *head);
void mtk_pm_qos_plist_del(struct plist_node *node, struct plist_head *head);

#endif
