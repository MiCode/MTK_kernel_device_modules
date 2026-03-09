// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#include <linux/pm_qos.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/export.h>
#include <linux/proc_fs.h>
#define CREATE_TRACE_POINTS
#include "mtk-pm-qos-trace.h"
#include "helio-dvfsrc-opp-mt6771.h"

static DEFINE_SPINLOCK(mtk_pm_qos_lock);

static struct mtk_pm_qos_object null_pm_qos;


static BLOCKING_NOTIFIER_HEAD(memory_bandwidth_notifier);
static struct mtk_pm_qos_constraints memory_bw_constraints = {
#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT8788)
	.req_list = LIST_HEAD_INIT(memory_bw_constraints.req_list),
#endif
	.list = PLIST_HEAD_INIT(memory_bw_constraints.list),
	.target_value = MTK_PM_QOS_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.type = MTK_PM_QOS_SUM,
#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT8788)
	.qos_lock = __MUTEX_INITIALIZER(memory_bw_constraints.qos_lock),
#endif
	.notifiers = &memory_bandwidth_notifier,
};
static struct mtk_pm_qos_object memory_bandwidth_pm_qos = {
	.constraints = &memory_bw_constraints,
	.name = "memory_bandwidth",
};

#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT8788)
static BLOCKING_NOTIFIER_HEAD(disp_freq_notifier);
static struct mtk_pm_qos_constraints disp_freq_constraints = {
	.req_list = LIST_HEAD_INIT(disp_freq_constraints.req_list),
	.list = PLIST_HEAD_INIT(disp_freq_constraints.list),
	.target_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.type = MTK_PM_QOS_MAX,
	.qos_lock = __MUTEX_INITIALIZER(disp_freq_constraints.qos_lock),
	.notifiers = &disp_freq_notifier,
};
static struct mtk_pm_qos_object disp_freq_pm_qos = {
	.constraints = &disp_freq_constraints,
	.name = "disp_freq",
};

static BLOCKING_NOTIFIER_HEAD(mdp_freq_notifier);
static struct mtk_pm_qos_constraints mdp_freq_constraints = {
	.req_list = LIST_HEAD_INIT(mdp_freq_constraints.req_list),
	.list = PLIST_HEAD_INIT(mdp_freq_constraints.list),
	.target_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.type = MTK_PM_QOS_MAX,
	.qos_lock = __MUTEX_INITIALIZER(mdp_freq_constraints.qos_lock),
	.notifiers = &mdp_freq_notifier,
};
static struct mtk_pm_qos_object mdp_freq_pm_qos = {
	.constraints = &mdp_freq_constraints,
	.name = "mdp_freq",
};

static BLOCKING_NOTIFIER_HEAD(vdec_freq_notifier);
static struct mtk_pm_qos_constraints vdec_freq_constraints = {
	.req_list = LIST_HEAD_INIT(vdec_freq_constraints.req_list),
	.list = PLIST_HEAD_INIT(vdec_freq_constraints.list),
	.target_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.type = MTK_PM_QOS_MAX,
	.qos_lock = __MUTEX_INITIALIZER(vdec_freq_constraints.qos_lock),
	.notifiers = &vdec_freq_notifier,
};
static struct mtk_pm_qos_object vdec_freq_pm_qos = {
	.constraints = &vdec_freq_constraints,
	.name = "vdec_freq",
};

static BLOCKING_NOTIFIER_HEAD(venc_freq_notifier);
static struct mtk_pm_qos_constraints venc_freq_constraints = {
	.req_list = LIST_HEAD_INIT(venc_freq_constraints.req_list),
	.list = PLIST_HEAD_INIT(venc_freq_constraints.list),
	.target_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.type = MTK_PM_QOS_MAX,
	.qos_lock = __MUTEX_INITIALIZER(venc_freq_constraints.qos_lock),
	.notifiers = &venc_freq_notifier,
};
static struct mtk_pm_qos_object venc_freq_pm_qos = {
	.constraints = &venc_freq_constraints,
	.name = "venc_freq",
};

static BLOCKING_NOTIFIER_HEAD(img_freq_notifier);
static struct mtk_pm_qos_constraints img_freq_constraints = {
	.req_list = LIST_HEAD_INIT(img_freq_constraints.req_list),
	.list = PLIST_HEAD_INIT(img_freq_constraints.list),
	.target_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.type = MTK_PM_QOS_MAX,
	.qos_lock = __MUTEX_INITIALIZER(img_freq_constraints.qos_lock),
	.notifiers = &img_freq_notifier,
};
static struct mtk_pm_qos_object img_freq_pm_qos = {
	.constraints = &img_freq_constraints,
	.name = "img_freq",
};

static BLOCKING_NOTIFIER_HEAD(cam_freq_notifier);
static struct mtk_pm_qos_constraints cam_freq_constraints = {
	.req_list = LIST_HEAD_INIT(cam_freq_constraints.req_list),
	.list = PLIST_HEAD_INIT(cam_freq_constraints.list),
	.target_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.type = MTK_PM_QOS_MAX,
	.qos_lock = __MUTEX_INITIALIZER(cam_freq_constraints.qos_lock),
	.notifiers = &cam_freq_notifier,
};
static struct mtk_pm_qos_object cam_freq_pm_qos = {
	.constraints = &cam_freq_constraints,
	.name = "cam_freq",
};

static BLOCKING_NOTIFIER_HEAD(dpe_freq_notifier);
static struct mtk_pm_qos_constraints dpe_freq_constraints = {
	.req_list = LIST_HEAD_INIT(dpe_freq_constraints.req_list),
	.list = PLIST_HEAD_INIT(dpe_freq_constraints.list),
	.target_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.type = MTK_PM_QOS_MAX,
	.qos_lock = __MUTEX_INITIALIZER(dpe_freq_constraints.qos_lock),
	.notifiers = &dpe_freq_notifier,
};
static struct mtk_pm_qos_object dpe_freq_pm_qos = {
	.constraints = &dpe_freq_constraints,
	.name = "dpe_freq",
};

static BLOCKING_NOTIFIER_HEAD(cpu_memory_bandwidth_notifier);
static struct mtk_pm_qos_constraints cpu_memory_bw_constraints = {
	.req_list = LIST_HEAD_INIT(cpu_memory_bw_constraints.req_list),
	.list = PLIST_HEAD_INIT(cpu_memory_bw_constraints.list),
	.target_value = MTK_PM_QOS_CPU_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_CPU_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_CPU_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.type = MTK_PM_QOS_SUM,
	.qos_lock = __MUTEX_INITIALIZER(cpu_memory_bw_constraints.qos_lock),
	.notifiers = &cpu_memory_bandwidth_notifier,
};
static struct mtk_pm_qos_object cpu_memory_bandwidth_pm_qos = {
	.constraints = &cpu_memory_bw_constraints,
	.name = "cpu_memory_bandwidth",
};


static BLOCKING_NOTIFIER_HEAD(gpu_memory_bandwidth_notifier);
static struct mtk_pm_qos_constraints gpu_memory_bw_constraints = {
	.req_list = LIST_HEAD_INIT(gpu_memory_bw_constraints.req_list),
	.list = PLIST_HEAD_INIT(gpu_memory_bw_constraints.list),
	.target_value = MTK_PM_QOS_GPU_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_GPU_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_GPU_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.type = MTK_PM_QOS_SUM,
	.qos_lock = __MUTEX_INITIALIZER(gpu_memory_bw_constraints.qos_lock),
	.notifiers = &gpu_memory_bandwidth_notifier,
};
static struct mtk_pm_qos_object gpu_memory_bandwidth_pm_qos = {
	.constraints = &gpu_memory_bw_constraints,
	.name = "gpu_memory_bandwidth",
};


static BLOCKING_NOTIFIER_HEAD(mm_memory_bandwidth_notifier);
static struct mtk_pm_qos_constraints mm_memory_bw_constraints = {
	.req_list = LIST_HEAD_INIT(mm_memory_bw_constraints.req_list),
	.list = PLIST_HEAD_INIT(mm_memory_bw_constraints.list),
	.target_value = MTK_PM_QOS_MM_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_MM_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_MM_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.type = MTK_PM_QOS_SUM,
	.qos_lock = __MUTEX_INITIALIZER(mm_memory_bw_constraints.qos_lock),
	.notifiers = &mm_memory_bandwidth_notifier,
};
static struct mtk_pm_qos_object mm_memory_bandwidth_pm_qos = {
	.constraints = &mm_memory_bw_constraints,
	.name = "mm_memory_bandwidth",
};


static BLOCKING_NOTIFIER_HEAD(md_peri_memory_bandwidth_notifier);
static struct mtk_pm_qos_constraints md_peri_memory_bw_constraints = {
	.req_list = LIST_HEAD_INIT(md_peri_memory_bw_constraints.req_list),
	.list = PLIST_HEAD_INIT(md_peri_memory_bw_constraints.list),
	.target_value = MTK_PM_QOS_MD_PERI_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_MD_PERI_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_MD_PERI_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.type = MTK_PM_QOS_SUM,
	.qos_lock = __MUTEX_INITIALIZER(md_peri_memory_bw_constraints.qos_lock),
	.notifiers = &md_peri_memory_bandwidth_notifier,
};
static struct mtk_pm_qos_object md_peri_memory_bandwidth_pm_qos = {
	.constraints = &md_peri_memory_bw_constraints,
	.name = "md_peri_memory_bandwidth",
};

static BLOCKING_NOTIFIER_HEAD(other_memory_bandwidth_notifier);
static struct mtk_pm_qos_constraints other_memory_bw_constraints = {
	.req_list = LIST_HEAD_INIT(other_memory_bw_constraints.req_list),
	.list = PLIST_HEAD_INIT(other_memory_bw_constraints.list),
	.target_value = MTK_PM_QOS_OTHER_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_OTHER_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_OTHER_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.type = MTK_PM_QOS_SUM,
	.qos_lock = __MUTEX_INITIALIZER(other_memory_bw_constraints.qos_lock),
	.notifiers = &other_memory_bandwidth_notifier,
};
static struct mtk_pm_qos_object other_memory_bandwidth_pm_qos = {
	.constraints = &other_memory_bw_constraints,
	.name = "other_memory_bandwidth",
};

static BLOCKING_NOTIFIER_HEAD(mm0_bandwidth_limiter_notifier);
static struct mtk_pm_qos_constraints mm0_bw_limiter_constraints = {
	.req_list = LIST_HEAD_INIT(mm0_bw_limiter_constraints.req_list),
	.list = PLIST_HEAD_INIT(mm0_bw_limiter_constraints.list),
	.target_value = MTK_PM_QOS_MM_BANDWIDTH_LIMITER_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_MM_BANDWIDTH_LIMITER_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_MM_BANDWIDTH_LIMITER_DEFAULT_VALUE,
	.type = MTK_PM_QOS_SUM,
	.qos_lock = __MUTEX_INITIALIZER(mm0_bw_limiter_constraints.qos_lock),
	.notifiers = &mm0_bandwidth_limiter_notifier,
};
static struct mtk_pm_qos_object mm0_bandwidth_limiter_pm_qos = {
	.constraints = &mm0_bw_limiter_constraints,
	.name = "mm0_bandwidth_limiter",
};

static BLOCKING_NOTIFIER_HEAD(mm1_bandwidth_limiter_notifier);
static struct mtk_pm_qos_constraints mm1_bw_limiter_constraints = {
	.req_list = LIST_HEAD_INIT(mm1_bw_limiter_constraints.req_list),
	.list = PLIST_HEAD_INIT(mm1_bw_limiter_constraints.list),
	.target_value = MTK_PM_QOS_MM_BANDWIDTH_LIMITER_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_MM_BANDWIDTH_LIMITER_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_MM_BANDWIDTH_LIMITER_DEFAULT_VALUE,
	.type = MTK_PM_QOS_SUM,
	.qos_lock = __MUTEX_INITIALIZER(mm1_bw_limiter_constraints.qos_lock),
	.notifiers = &mm1_bandwidth_limiter_notifier,
};
static struct mtk_pm_qos_object mm1_bandwidth_limiter_pm_qos = {
	.constraints = &mm1_bw_limiter_constraints,
	.name = "mm1_bandwidth_limiter",
};

static BLOCKING_NOTIFIER_HEAD(emi_opp_notifier);
static struct mtk_pm_qos_constraints emi_opp_constraints = {
	.req_list = LIST_HEAD_INIT(emi_opp_constraints.req_list),
	.list = PLIST_HEAD_INIT(emi_opp_constraints.list),
	.target_value = MTK_PM_QOS_EMI_OPP_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_EMI_OPP_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_EMI_OPP_DEFAULT_VALUE,
	.type = MTK_PM_QOS_MIN,
	.qos_lock = __MUTEX_INITIALIZER(emi_opp_constraints.qos_lock),
	.notifiers = &emi_opp_notifier,
};
static struct mtk_pm_qos_object emi_opp_pm_qos = {
	.constraints = &emi_opp_constraints,
	.name = "emi_opp",
};

static BLOCKING_NOTIFIER_HEAD(ddr_opp_notifier);
static struct mtk_pm_qos_constraints ddr_opp_constraints = {
	.req_list = LIST_HEAD_INIT(ddr_opp_constraints.req_list),
	.list = PLIST_HEAD_INIT(ddr_opp_constraints.list),
	.target_value = MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE,
	.type = MTK_PM_QOS_MIN,
	.qos_lock = __MUTEX_INITIALIZER(ddr_opp_constraints.qos_lock),
	.notifiers = &ddr_opp_notifier,
};
static struct mtk_pm_qos_object ddr_opp_pm_qos = {
	.constraints = &ddr_opp_constraints,
	.name = "ddr_opp",
};

static BLOCKING_NOTIFIER_HEAD(vcore_opp_notifier);
static struct mtk_pm_qos_constraints vcore_opp_constraints = {
	.req_list = LIST_HEAD_INIT(vcore_opp_constraints.req_list),
	.list = PLIST_HEAD_INIT(vcore_opp_constraints.list),
	.target_value = MTK_PM_QOS_VCORE_OPP_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_VCORE_OPP_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_VCORE_OPP_DEFAULT_VALUE,
	.type = MTK_PM_QOS_MIN,
	.qos_lock = __MUTEX_INITIALIZER(vcore_opp_constraints.qos_lock),
	.notifiers = &vcore_opp_notifier,
};
static struct mtk_pm_qos_object vcore_opp_pm_qos = {
	.constraints = &vcore_opp_constraints,
	.name = "vcore_opp",
};

static BLOCKING_NOTIFIER_HEAD(vcore_dvfs_fixed_opp_notifier);
static struct mtk_pm_qos_constraints vcore_dvfs_fixed_opp_constraints = {
	.req_list = LIST_HEAD_INIT(vcore_dvfs_fixed_opp_constraints.req_list),
	.list = PLIST_HEAD_INIT(vcore_dvfs_fixed_opp_constraints.list),
	.target_value = MTK_PM_QOS_VCORE_DVFS_FIXED_OPP_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_VCORE_DVFS_FIXED_OPP_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_VCORE_DVFS_FIXED_OPP_DEFAULT_VALUE,
	.type = MTK_PM_QOS_MIN,
	.qos_lock =
		__MUTEX_INITIALIZER(vcore_dvfs_fixed_opp_constraints.qos_lock),
	.notifiers = &vcore_dvfs_fixed_opp_notifier,
};
static struct mtk_pm_qos_object vcore_dvfs_fixed_opp_pm_qos = {
	.constraints = &vcore_dvfs_fixed_opp_constraints,
	.name = "vcore_dvfs_fixed_opp",
};

static BLOCKING_NOTIFIER_HEAD(scp_vcore_req_notifier);
static struct mtk_pm_qos_constraints scp_vcore_req_constraints = {
	.req_list = LIST_HEAD_INIT(scp_vcore_req_constraints.req_list),
	.list = PLIST_HEAD_INIT(scp_vcore_req_constraints.list),
	.target_value = MTK_PM_QOS_SCP_VCORE_REQUEST_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_SCP_VCORE_REQUEST_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_SCP_VCORE_REQUEST_DEFAULT_VALUE,
	.type = MTK_PM_QOS_MAX,
	.qos_lock = __MUTEX_INITIALIZER(scp_vcore_req_constraints.qos_lock),
	.notifiers = &scp_vcore_req_notifier,
};
static struct mtk_pm_qos_object scp_vcore_req_pm_qos = {
	.constraints = &scp_vcore_req_constraints,
	.name = "scp_vcore_req",
};

static BLOCKING_NOTIFIER_HEAD(power_model_ddr_req_notifier);
static struct mtk_pm_qos_constraints power_model_ddr_req_constraints = {
	.req_list = LIST_HEAD_INIT(power_model_ddr_req_constraints.req_list),
	.list = PLIST_HEAD_INIT(power_model_ddr_req_constraints.list),
	.target_value = MTK_PM_QOS_POWER_MODEL_DDR_REQUEST_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_POWER_MODEL_DDR_REQUEST_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_POWER_MODEL_DDR_REQUEST_DEFAULT_VALUE,
	.type = MTK_PM_QOS_MAX,
	.qos_lock =
		__MUTEX_INITIALIZER(power_model_ddr_req_constraints.qos_lock),
	.notifiers = &power_model_ddr_req_notifier,
};
static struct mtk_pm_qos_object power_model_ddr_req_pm_qos = {
	.constraints = &power_model_ddr_req_constraints,
	.name = "power_model_ddr_req",
};

static BLOCKING_NOTIFIER_HEAD(power_model_vcore_req_notifier);
static struct mtk_pm_qos_constraints power_model_vcore_req_constraints = {
	.req_list = LIST_HEAD_INIT(power_model_vcore_req_constraints.req_list),
	.list = PLIST_HEAD_INIT(power_model_vcore_req_constraints.list),
	.target_value = MTK_PM_QOS_POWER_MODEL_VCORE_REQUEST_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_POWER_MODEL_VCORE_REQUEST_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_POWER_MODEL_VCORE_REQUEST_DEFAULT_VALUE,
	.type = MTK_PM_QOS_MAX,
	.qos_lock =
		__MUTEX_INITIALIZER(power_model_vcore_req_constraints.qos_lock),
	.notifiers = &power_model_vcore_req_notifier,
};
static struct mtk_pm_qos_object power_model_vcore_req_pm_qos = {
	.constraints = &power_model_vcore_req_constraints,
	.name = "power_model_vcore_req",
};

static BLOCKING_NOTIFIER_HEAD(vcore_dvfs_force_opp_notifier);
static struct mtk_pm_qos_constraints vcore_dvfs_force_opp_constraints = {
	.req_list = LIST_HEAD_INIT(vcore_dvfs_force_opp_constraints.req_list),
	.list = PLIST_HEAD_INIT(vcore_dvfs_force_opp_constraints.list),
	.target_value = MTK_PM_QOS_VCORE_DVFS_FORCE_OPP_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_VCORE_DVFS_FORCE_OPP_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_VCORE_DVFS_FORCE_OPP_DEFAULT_VALUE,
	.type = MTK_PM_QOS_MIN,
	.qos_lock =
		__MUTEX_INITIALIZER(vcore_dvfs_force_opp_constraints.qos_lock),
	.notifiers = &vcore_dvfs_force_opp_notifier,
};
static struct mtk_pm_qos_object vcore_dvfs_force_opp_pm_qos = {
	.constraints = &vcore_dvfs_force_opp_constraints,
	.name = "vcore_dvfs_force_opp",
};

static BLOCKING_NOTIFIER_HEAD(isp_hrt_bandwidth_notifier);
static struct mtk_pm_qos_constraints isp_hrt_bw_constraints = {
	.req_list = LIST_HEAD_INIT(isp_hrt_bw_constraints.req_list),
	.list = PLIST_HEAD_INIT(isp_hrt_bw_constraints.list),
	.target_value = MTK_PM_QOS_ISP_HRT_BANDWIDTH_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_ISP_HRT_BANDWIDTH_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_ISP_HRT_BANDWIDTH_DEFAULT_VALUE,
	.type = MTK_PM_QOS_SUM,
	.qos_lock = __MUTEX_INITIALIZER(isp_hrt_bw_constraints.qos_lock),
	.notifiers = &isp_hrt_bandwidth_notifier,
};
static struct mtk_pm_qos_object isp_hrt_bandwidth_pm_qos = {
	.constraints = &isp_hrt_bw_constraints,
	.name = "isp_hrt_bandwidth",
};

static BLOCKING_NOTIFIER_HEAD(apu_memory_bandwidth_notifier);
static struct mtk_pm_qos_constraints apu_memory_bw_constraints = {
	.req_list = LIST_HEAD_INIT(apu_memory_bw_constraints.req_list),
	.list = PLIST_HEAD_INIT(apu_memory_bw_constraints.list),
	.target_value = MTK_PM_QOS_APU_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_APU_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_APU_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.type = MTK_PM_QOS_SUM,
	.qos_lock = __MUTEX_INITIALIZER(apu_memory_bw_constraints.qos_lock),
	.notifiers = &apu_memory_bandwidth_notifier,
};
static struct mtk_pm_qos_object apu_memory_bandwidth_pm_qos = {
	.constraints = &apu_memory_bw_constraints,
	.name = "apu_memory_bandwidth",
};
static BLOCKING_NOTIFIER_HEAD(vvpu_opp_notifier);
static struct mtk_pm_qos_constraints vvpu_opp_constraints = {
	.req_list = LIST_HEAD_INIT(vvpu_opp_constraints.req_list),
	.list = PLIST_HEAD_INIT(vvpu_opp_constraints.list),
	.target_value = MTK_PM_QOS_VVPU_OPP_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_VVPU_OPP_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_VVPU_OPP_DEFAULT_VALUE,
	.type = MTK_PM_QOS_MIN,
	.qos_lock = __MUTEX_INITIALIZER(vvpu_opp_constraints.qos_lock),
	.notifiers = &vvpu_opp_notifier,
};
static struct mtk_pm_qos_object vvpu_opp_pm_qos = {
	.constraints = &vvpu_opp_constraints,
	.name = "vvpu_opp",
};
#endif
static struct mtk_pm_qos_object *mtk_pm_qos_array[] = {
	[MTK_PM_QOS_RESERVED]		=	&null_pm_qos,
	// [MTK_PM_QOS_CPU_DMA_LATENCY]	=	&cpu_dma_pm_qos,
	// [MTK_PM_QOS_NETWORK_LATENCY]	=	&network_lat_pm_qos,
	// [MTK_PM_QOS_NETWORK_THROUGHPUT]	=	&network_throughput_pm_qos,
	[MTK_PM_QOS_MEMORY_BANDWIDTH]	=	&memory_bandwidth_pm_qos,
#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT8788)
	[MTK_PM_QOS_CPU_MEMORY_BANDWIDTH]	=	&cpu_memory_bandwidth_pm_qos,
	[MTK_PM_QOS_GPU_MEMORY_BANDWIDTH]	=	&gpu_memory_bandwidth_pm_qos,
	[MTK_PM_QOS_MM_MEMORY_BANDWIDTH]	=	&mm_memory_bandwidth_pm_qos,
	[MTK_PM_QOS_MD_PERI_MEMORY_BANDWIDTH]=	&md_peri_memory_bandwidth_pm_qos,
	[MTK_PM_QOS_OTHER_MEMORY_BANDWIDTH]	=	&other_memory_bandwidth_pm_qos,
	[MTK_PM_QOS_MM0_BANDWIDTH_LIMITER]	=	&mm0_bandwidth_limiter_pm_qos,
	[MTK_PM_QOS_MM1_BANDWIDTH_LIMITER]	=	&mm1_bandwidth_limiter_pm_qos,
	[MTK_PM_QOS_DDR_OPP]		=	&ddr_opp_pm_qos,
	[MTK_PM_QOS_EMI_OPP]		=	&emi_opp_pm_qos,
	[MTK_PM_QOS_VCORE_OPP]		=	&vcore_opp_pm_qos,
	[MTK_PM_QOS_VCORE_DVFS_FIXED_OPP]	=	&vcore_dvfs_fixed_opp_pm_qos,
	[MTK_PM_QOS_SCP_VCORE_REQUEST]	=	&scp_vcore_req_pm_qos,
	[MTK_PM_QOS_POWER_MODEL_DDR_REQUEST]=	&power_model_ddr_req_pm_qos,
	[MTK_PM_QOS_POWER_MODEL_VCORE_REQUEST]=	&power_model_vcore_req_pm_qos,
	[MTK_PM_QOS_VCORE_DVFS_FORCE_OPP]	=	&vcore_dvfs_force_opp_pm_qos,
	[MTK_PM_QOS_DISP_FREQ]		=	&disp_freq_pm_qos,
	[MTK_PM_QOS_MDP_FREQ]		=	&mdp_freq_pm_qos,
	[MTK_PM_QOS_VDEC_FREQ]		=	&vdec_freq_pm_qos,
	[MTK_PM_QOS_VENC_FREQ]		=	&venc_freq_pm_qos,
	[MTK_PM_QOS_IMG_FREQ]		=	&img_freq_pm_qos,
	[MTK_PM_QOS_CAM_FREQ]		=	&cam_freq_pm_qos,
	[MTK_PM_QOS_DPE_FREQ]		=	&dpe_freq_pm_qos,
	[MTK_PM_QOS_ISP_HRT_BANDWIDTH]	=	&isp_hrt_bandwidth_pm_qos,
	[MTK_PM_QOS_APU_MEMORY_BANDWIDTH]	=	&apu_memory_bandwidth_pm_qos,
	[MTK_PM_QOS_VVPU_OPP]		=	&vvpu_opp_pm_qos,
#endif
};

/* unlocked internal variant */
static inline int mtk_pm_qos_get_value(struct mtk_pm_qos_constraints *c)
{
	struct plist_node *node;
	int total_value = 0;

	if (plist_head_empty(&c->list))
		return c->no_constraint_value;

	switch (c->type) {
	case MTK_PM_QOS_MIN:
		return plist_first(&c->list)->prio;

	case MTK_PM_QOS_MAX:
		return plist_last(&c->list)->prio;

	case MTK_PM_QOS_SUM:
		plist_for_each(node, &c->list)
			total_value += node->prio;

		return total_value;

	default:
		/* runtime check for not using enum */
		WARN(1, "Unknown PM QoS type in %s\n", __func__);
		return PM_QOS_DEFAULT_VALUE;
	}
}

s32 mtk_pm_qos_read_value(struct mtk_pm_qos_constraints *c)
{
	return READ_ONCE(c->target_value);
}

static void mtk_pm_qos_set_value(struct mtk_pm_qos_constraints *c, s32 value)
{
	WRITE_ONCE(c->target_value, value);
}


#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT8788)
void pm_qos_trace_dbg_show_request(int pm_qos_class)
{
	struct mtk_pm_qos_constraints *c;
	struct mtk_pm_qos_request *req;
	unsigned long flags;
	struct list_head *l;

	if (pm_qos_class < MTK_PM_QOS_RESERVED
		|| pm_qos_class >= MTK_PM_QOS_NUM_CLASSES)
		return;

	c = mtk_pm_qos_array[pm_qos_class]->constraints;

	spin_lock_irqsave(&mtk_pm_qos_lock, flags);
	/* dump owner information*/
	list_for_each(l, &c->req_list) {
		req = list_entry(l, struct mtk_pm_qos_request, list_node);
#if IS_DVFSRC_PM_QOS_TRACE_ENABLED
	trace_mtk_pm_qos_update_request(req->pm_qos_class,
					req->node.prio, req->owner);
#endif
	}
	spin_unlock_irqrestore(&mtk_pm_qos_lock, flags);
}

// static inline int mtk_pm_qos_get_value(struct mtk_pm_qos_constraints *c);
#endif
static int mtk_pm_qos_dbg_show_requests(struct seq_file *s, void *unused)
{
	struct mtk_pm_qos_object *qos = (struct mtk_pm_qos_object *)s->private;
	struct mtk_pm_qos_constraints *c;
	struct mtk_pm_qos_request *req;
	char *type;
	unsigned long flags;
	int tot_reqs = 0;
	int active_reqs = 0;
#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT8788)
	struct list_head *l;
#endif

	if (IS_ERR_OR_NULL(qos)) {
		pr_err("%s: bad qos param!\n", __func__);
		return -EINVAL;
	}
	c = qos->constraints;
	if (IS_ERR_OR_NULL(c)) {
		pr_err("%s: Bad constraints on qos?\n", __func__);
		return -EINVAL;
	}

	/* Lock to ensure we have a snapshot */
	spin_lock_irqsave(&mtk_pm_qos_lock, flags);
	if (plist_head_empty(&c->list)) {
		seq_puts(s, "Empty!\n");
		goto out;
	}

	switch (c->type) {
	case MTK_PM_QOS_MIN:
		type = "Minimum";
		break;
	case MTK_PM_QOS_MAX:
		type = "Maximum";
		break;
	case MTK_PM_QOS_SUM:
		type = "Sum";
		break;
	default:
		type = "Unknown";
	}

	plist_for_each_entry(req, &c->list, node) {
		char *state = "Default";

		if ((req->node).prio != c->default_value) {
			active_reqs++;
			state = "Active";
		}
		tot_reqs++;
		seq_printf(s, "%d: %d: %s\n", tot_reqs,
			(req->node).prio, state);
	}

	seq_printf(s, "Type=%s, Value=%d, Requests: active=%d / total=%d\n",
		type, mtk_pm_qos_get_value(c), active_reqs, tot_reqs);

#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT8788)
	list_for_each(l, &c->req_list) {
		req = list_entry(l, struct mtk_pm_qos_request, list_node);

		seq_printf(s, "%s: %d\n", req->owner, req->node.prio);
	}
#endif
out:
	spin_unlock_irqrestore(&mtk_pm_qos_lock, flags);
	return 0;
}

static int mtk_pm_qos_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_pm_qos_dbg_show_requests,
			inode->i_private);
}

static const struct proc_ops mtk_pm_qos_proc_fops = {
	.proc_open	= mtk_pm_qos_dbg_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

/**
 * mtk_pm_qos_update_target - manages the constraints list and calls the notifiers
 *  if needed
 * @c: constraints data struct
 * @node: request to add to the list, to update or to remove
 * @action: action to take on the constraints list
 * @value: value of the request to add or update
 *
 * This function returns 1 if the aggregated constraint value has changed, 0
 *  otherwise.
 */
int mtk_pm_qos_update_target(struct mtk_pm_qos_constraints *c, struct plist_node *node,
			enum pm_qos_req_action action, int value)
{
	unsigned long flags;
	int prev_value, curr_value, new_value;
	int ret;

	mutex_lock(&c->qos_lock);

	spin_lock_irqsave(&mtk_pm_qos_lock, flags);
	prev_value = mtk_pm_qos_get_value(c);
	if (value == PM_QOS_DEFAULT_VALUE)
		new_value = c->default_value;
	else
		new_value = value;

	switch (action) {
	case PM_QOS_REMOVE_REQ:
		mtk_pm_qos_plist_del(node, &c->list);
		break;
	case PM_QOS_UPDATE_REQ:
		/*
		 * to change the list, we atomically remove, reinit
		 * with new value and add, then see if the extremal
		 * changed
		 */
		mtk_pm_qos_plist_del(node, &c->list);
		fallthrough;
	case PM_QOS_ADD_REQ:
		plist_node_init(node, new_value);
		mtk_pm_qos_plist_add(node, &c->list);
		break;
	default:
		/* no action */
		break;
	}
	curr_value = mtk_pm_qos_get_value(c);
	mtk_pm_qos_set_value(c, curr_value);
	spin_unlock_irqrestore(&mtk_pm_qos_lock, flags);

	if (prev_value != curr_value) {
		ret = 1;
		if (c->notifiers)
			blocking_notifier_call_chain(c->notifiers,
						(unsigned long)curr_value,
						NULL);
	} else {
		ret = 0;
	}

	mutex_unlock(&c->qos_lock);

	return ret;
}



/**
 * mtk_pm_qos_request - returns current system wide qos expectation
 * @pm_qos_class: identification of which qos value is requested
 *
 * This function returns the current target value.
 */
int mtk_pm_qos_request(int pm_qos_class)
{
	if (pm_qos_class >= MTK_PM_QOS_NUM_CLASSES) {
		pr_err("%s:_%d_: unknown class =%d\n", __func__, __LINE__, pm_qos_class);
		return PM_QOS_DEFAULT_VALUE;
	}

	return mtk_pm_qos_read_value(mtk_pm_qos_array[pm_qos_class]->constraints);
}
EXPORT_SYMBOL(mtk_pm_qos_request);

/**
 * plist_add - add @node to @head
 *
 * @node:	&struct plist_node pointer
 * @head:	&struct plist_head pointer
 */
void mtk_pm_qos_plist_add(struct plist_node *node, struct plist_head *head)
{
	struct plist_node *first, *iter, *prev = NULL;
	struct list_head *node_next = &head->node_list;

	WARN_ON(!plist_node_empty(node));
	WARN_ON(!list_empty(&node->prio_list));

	if (plist_head_empty(head))
		goto ins_node;

	first = iter = plist_first(head);

	do {
		if (node->prio < iter->prio) {
			node_next = &iter->node_list;
			break;
		}

		prev = iter;
		iter = list_entry(iter->prio_list.next,
				struct plist_node, prio_list);
	} while (iter != first);

	if (!prev || prev->prio != node->prio)
		list_add_tail(&node->prio_list, &iter->prio_list);
ins_node:
	list_add_tail(&node->node_list, node_next);

}

/**
 * plist_del - Remove a @node from plist.
 *
 * @node:	&struct plist_node pointer - entry to be removed
 * @head:	&struct plist_head pointer - list head
 */
void mtk_pm_qos_plist_del(struct plist_node *node, struct plist_head *head)
{
	if (!list_empty(&node->prio_list)) {
		if (node->node_list.next != &head->node_list) {
			struct plist_node *next;

			next = list_entry(node->node_list.next,
					struct plist_node, node_list);

			/* add the next plist_node into prio_list */
			if (list_empty(&next->prio_list))
				list_add(&next->prio_list, &node->prio_list);
		}
		list_del_init(&node->prio_list);
	}

	list_del_init(&node->node_list);
}

int mtk_pm_qos_request_active(struct mtk_pm_qos_request *req)
{
	return req->pm_qos_class != 0;
}
EXPORT_SYMBOL(mtk_pm_qos_request_active);

static void __pm_qos_update_request(struct mtk_pm_qos_request *req,
			s32 new_value)
{
#if IS_DVFSRC_PM_QOS_TRACE_ENABLED
	trace_mtk_pm_qos_update_request(req->pm_qos_class,
					new_value, req->owner);
#endif
	if (new_value != req->node.prio)
		mtk_pm_qos_update_target(
			mtk_pm_qos_array[req->pm_qos_class]->constraints,
			&req->node, PM_QOS_UPDATE_REQ, new_value);
}

/**
 * mtk_pm_qos_add_request - inserts new qos request into the list
 * @req: pointer to a preallocated handle
 * @pm_qos_class: identifies which list of qos request to use
 * @value: defines the qos request
 *
 * This function inserts a new entry in the pm_qos_class list of requested qos
 * performance characteristics.  It recomputes the aggregate QoS expectations
 * for the pm_qos_class of parameters and initializes the mtk_pm_qos_request
 * handle.  Caller needs to save this handle for later use in updates and
 * removal.
 */

void mtk_pm_qos_add_request(struct mtk_pm_qos_request *req,
			int pm_qos_class, s32 value)
{
	char owner[20] = {0};
	int n;

	if (!req) /*guard against callers passing in null */
		return;

	if (pm_qos_class >= MTK_PM_QOS_NUM_CLASSES) {
		pr_err("%s: unknown class =%d\n", __func__, pm_qos_class);
		return;
	}

	n = snprintf(owner, sizeof(owner) - 1, "%pS",
		__builtin_return_address(0));

	if (n < 0)
		strscpy(owner, "unknown", sizeof(owner) - 1);

	if (mtk_pm_qos_request_active(req)) {
		pr_info("%s called for already added request\n", __func__);
		return;
	}
#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT8788)
	strscpy(req->owner, owner, sizeof(req->owner) - 1);
#endif
	req->pm_qos_class = pm_qos_class;
#if IS_DVFSRC_PM_QOS_TRACE_ENABLED
	trace_mtk_pm_qos_add_request(pm_qos_class, value, req->owner);
#endif
	mtk_pm_qos_update_target(mtk_pm_qos_array[pm_qos_class]->constraints,
				&req->node, PM_QOS_ADD_REQ, value);

}
EXPORT_SYMBOL(mtk_pm_qos_add_request);

/**
 * mtk_pm_qos_update_request - modifies an existing qos request
 * @req : handle to list element holding a pm_qos request to use
 * @value: defines the qos request
 *
 * Updates an existing qos request for the pm_qos_class of parameters along
 * with updating the target pm_qos_class value.
 *
 * Attempts are made to make this code callable on hot code paths.
 */
void mtk_pm_qos_update_request(struct mtk_pm_qos_request *req,
			s32 new_value)
{
	if (!req) /*guard against callers passing in null */
		return;

	if (!mtk_pm_qos_request_active(req)) {
		pr_err("%s: called for unknown object\n", __func__);
		return;
	}

	// cancel_delayed_work_sync(&req->work);
	__pm_qos_update_request(req, new_value);
}
EXPORT_SYMBOL(mtk_pm_qos_update_request);

/**
 * mtk_pm_qos_remove_request - modifies an existing qos request
 * @req: handle to request list element
 *
 * Will remove pm qos request from the list of constraints and
 * recompute the current target value for the pm_qos_class.  Call this
 * on slow code paths.
 */
void mtk_pm_qos_remove_request(struct mtk_pm_qos_request *req)
{
	if (!req) /*guard against callers passing in null */
		return;
		/* silent return to keep pcm code cleaner */

	if (!mtk_pm_qos_request_active(req)) {
		pr_err("%s:  called for unknown object\n", __func__);
		return;
	}

#if IS_DVFSRC_PM_QOS_TRACE_ENABLED
	trace_mtk_pm_qos_remove_request(req->pm_qos_class, PM_QOS_DEFAULT_VALUE,
					req->owner);
#endif
	mtk_pm_qos_update_target(mtk_pm_qos_array[req->pm_qos_class]->constraints,
			&req->node, PM_QOS_REMOVE_REQ,
			PM_QOS_DEFAULT_VALUE);
	memset(req, 0, sizeof(*req));
}
EXPORT_SYMBOL(mtk_pm_qos_remove_request);

/**
 * mtk_pm_qos_add_notifier - sets notification entry for changes to target value
 * @pm_qos_class: identifies which qos target changes should be notified.
 * @notifier: notifier block managed by caller.
 *
 * will register the notifier into a notification chain that gets called
 * upon changes to the pm_qos_class target value.
 */
int mtk_pm_qos_add_notifier(int pm_qos_class, struct notifier_block *notifier)
{
	int retval;

	retval = blocking_notifier_chain_register(
			mtk_pm_qos_array[pm_qos_class]->constraints->notifiers,
			notifier);

	return retval;
}
EXPORT_SYMBOL(mtk_pm_qos_add_notifier);

/**
 * mtk_pm_qos_remove_notifier - deletes notification entry from chain.
 * @pm_qos_class: identifies which qos target changes are notified.
 * @notifier: notifier block to be removed.
 *
 * will remove the notifier from the notification chain that gets called
 * upon changes to the pm_qos_class target value.
 */
int mtk_pm_qos_remove_notifier(int pm_qos_class, struct notifier_block *notifier)
{
	int retval;

	retval = blocking_notifier_chain_unregister(
			mtk_pm_qos_array[pm_qos_class]->constraints->notifiers,
			notifier);

	return retval;
}
EXPORT_SYMBOL(mtk_pm_qos_remove_notifier);

/* debug procfs for PM QoS classes*/
static int register_mtk_pm_qos_procfs(struct mtk_pm_qos_object *qos,
	struct proc_dir_entry *d)
{
	proc_create_data(qos->name, 0444, d,
		&mtk_pm_qos_proc_fops, (void *)qos);

	return 0;
}

static int __init mtk_pm_qos_power_init(void)
{
	int i;
	struct proc_dir_entry *proc_root = NULL;

	proc_root = proc_mkdir("mtk_pm_qos", NULL);
	if (!proc_root)
		return -1;

	for (i = MTK_PM_QOS_MEMORY_BANDWIDTH; i < MTK_PM_QOS_NUM_CLASSES; i++)
		register_mtk_pm_qos_procfs(mtk_pm_qos_array[i], proc_root);

	return 0;
}

static void __exit mtk_pm_qos_power_exit(void)
{
}

module_init(mtk_pm_qos_power_init)
module_exit(mtk_pm_qos_power_exit)

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Helio dvfsrc driver");
