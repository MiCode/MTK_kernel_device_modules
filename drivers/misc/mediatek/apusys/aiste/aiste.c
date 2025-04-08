// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#endif
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#define NONE DEL_NONE
#include <linux/scmi_protocol.h>
#undef NONE
#include <linux/regmap.h>
#include <linux/sched/cputime.h>
#include <linux/sched.h>
#include <kernel/sched/sched.h>
#include <linux/types.h>
#include <uapi/linux/sched/types.h>
#include "apusys_core.h"
#include "tinysys-scmi.h"

#define AI_SYS_TUNE_DNAME "ai_sys_tune"
#define AI_SYS_TUNE_F_UCLAMP_MIN "uclamp_min"
#define AI_SYS_TUNE_F_UCLAMP_MAX "uclamp_max"
struct proc_dir_entry *ai_sys_tune_root, *f_uclamp_min, *f_uclamp_max;
static u32 g_uclamp_min = 0, g_uclamp_max = 1024;

enum {
AISTE_INIT,
AISTE_PERFORMANCE_L1_ON,
AISTE_PERFORMANCE_L2_ON,
AISTE_PERFORMANCE_L3_ON,
AISTE_PERFORMANCE_OFF,
AISTE_MAX,
};

enum {
AISTE_CREATE_CMD,
AISTE_PER_L1_CMD,
AISTE_PER_L2_CMD,
AISTE_PER_L3_CMD,
AISTE_DELETE_CMD,
AISTE_CMD_MAX,
};

enum AISTE_SCHED_MODE {
AISTE_SCHED_MODE_NONE,
AISTE_SCHED_MODE_PERFORMANCE,
AISTE_SCHED_MODE_SUSTAINABLE,
AISTE_SCHED_MODE_LOW_POWER,
};

static unsigned int g_create_cmd_count;
static unsigned int g_per_l1_count;
static unsigned int g_per_l2_count;
static unsigned int g_per_l3_count;
static unsigned int g_delete_cmd_count;

static unsigned int g_aiste_addr;
static unsigned int g_aiste_size;
static void __iomem *g_aiste_buf_addr;

static struct scmi_tinysys_info_st *tinfo;
static int feature_id;

void aiste_scmi_init(void)
{
	int err;

	pr_info("%s:addr=0x%x,size=0x%x\n", __func__, g_aiste_addr, g_aiste_size);

	if ((g_aiste_addr > 0) && (g_aiste_size > 0)) {
		if (!tinfo) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
			tinfo = get_scmi_tinysys_info();
#endif
			if ((IS_ERR_OR_NULL(tinfo)) || (IS_ERR_OR_NULL(tinfo->ph))) {
				pr_info("%s: tinfo or tinfo->ph is wrong!!\n", __func__);
				tinfo = NULL;
				} else {
					err = of_property_read_u32(tinfo->sdev->dev.of_node,
						"scmi-aiste", &feature_id);
					if (err) {
						pr_info("get scmi-aiste fail\n");
						return;
					}

					pr_info("%s: get scmi_smi succeed id=%d!!\n",
						__func__, feature_id);

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
					err = scmi_tinysys_common_set(tinfo->ph, feature_id,
					AISTE_INIT, g_aiste_addr, g_aiste_size, 0, 0);
#endif
					if (err)
						pr_info("%s: call scmi_tinysys_common_set err=%d\n",
						__func__, err);
				}
			}
		}
	pr_info("%s++\n", __func__);
}

void aiste_scmi_set(unsigned int performance_level)
{
	int err;

	if ((performance_level > AISTE_INIT) && (performance_level < AISTE_MAX)) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
		err = scmi_tinysys_common_set(tinfo->ph, feature_id, performance_level, 0, 0, 0, 0);
#endif
		if (err)
			pr_info("%s: call scmi_tinysys_common_set err=%d\n",__func__, err);
		}
	pr_info("%s++\n", __func__);
}

void aiste_set_uclamp(struct task_struct *p, enum AISTE_SCHED_MODE sched_mode)
{
	// unsigned int cur_min = 0, cur_max = 0;
	struct sched_attr attr = {};

	if (sched_mode == AISTE_SCHED_MODE_PERFORMANCE) {
		attr.sched_policy = -1;
		attr.sched_flags =
			SCHED_FLAG_KEEP_ALL |
			SCHED_FLAG_UTIL_CLAMP |
			SCHED_FLAG_RESET_ON_FORK;
		if (p->policy == SCHED_FIFO || p->policy == SCHED_RR)
			attr.sched_priority = p->rt_priority;
		attr.sched_util_max = 1024;
		attr.sched_util_min = 512;
		if (sched_setattr_nocheck(p, &attr) != 0)
			pr_info("%s: set uclamp fail\n", __func__);
	} else if (sched_mode == AISTE_SCHED_MODE_SUSTAINABLE) {
		attr.sched_policy = -1;
		attr.sched_flags =
			SCHED_FLAG_KEEP_ALL |
			SCHED_FLAG_UTIL_CLAMP |
			SCHED_FLAG_RESET_ON_FORK;
		if (p->policy == SCHED_FIFO || p->policy == SCHED_RR)
			attr.sched_priority = p->rt_priority;
		attr.sched_util_max = 256;//attr.sched_util_max = g_uclamp_max;
		attr.sched_util_min = 0;//attr.sched_util_min = g_uclamp_min;
		if (sched_setattr_nocheck(p, &attr) != 0)
			pr_info("ai_system_tuning: set %d uclamp fail\n", p->pid);
	}

	// cur_min = uclamp_eff_value(p, UCLAMP_MIN);
	// cur_max = uclamp_eff_value(p, UCLAMP_MAX);
	pr_info("ai_system_tuning: pid=%d cur_min=%d cur_max=%d\n", p->pid, attr.sched_util_min, attr.sched_util_max);
}

void ai_system_tuning(unsigned int cmd_type, unsigned int cmd_id, pid_t pid)
{
	struct task_struct *p = find_task_by_vpid(pid);

	if (cmd_type < AISTE_CMD_MAX) {
		if (cmd_type == AISTE_PER_L1_CMD) {
			g_per_l1_count = g_per_l1_count + 1;
			// pr_info("%s: perf++\n", __func__);
		} else if (cmd_type == AISTE_PER_L2_CMD) {
			g_per_l2_count = g_per_l2_count + 1;
			// pr_info("%s: sustain++\n", __func__);
		} else if (cmd_type == AISTE_PER_L3_CMD) {
			g_per_l3_count = g_per_l3_count + 1;
			// pr_info("%s: low power++\n", __func__);
		} else if (cmd_type == AISTE_DELETE_CMD) {
			g_per_l1_count = 0;
			g_per_l2_count = 0;
			g_per_l3_count = 0;
			// pr_info("%s: delete cmd\n", __func__);
		}
		//recode the counter, reduce sent ipi,if conter > 1 (no need to send ipi again)
		if (g_per_l1_count) {
			aiste_scmi_set(AISTE_PERFORMANCE_L1_ON);
			aiste_set_uclamp(p, AISTE_SCHED_MODE_PERFORMANCE);
		} else if (g_per_l2_count) {
			aiste_scmi_set(AISTE_PERFORMANCE_L2_ON);
			aiste_set_uclamp(p, AISTE_SCHED_MODE_SUSTAINABLE);
		} else if (g_per_l3_count) {
			aiste_scmi_set(AISTE_PERFORMANCE_L3_ON);
		} else {
			aiste_scmi_set(AISTE_PERFORMANCE_OFF);
		}

		pr_info("perf/sus/low = %u/%u/%u, pid=%d\n", g_per_l1_count, g_per_l2_count, g_per_l3_count, pid);
	}
}

static int mtk_aiste_probe(struct platform_device *pdev)
{

	int ret;
	struct device_node *aiste_node = pdev->dev.of_node;

	g_create_cmd_count = 0;
	g_per_l1_count = 0;
	g_per_l2_count = 0;
	g_per_l3_count = 0;
	g_delete_cmd_count = 0;

	dev_info(&pdev->dev, "%s probed\n", __func__);

	ret = of_property_read_u32(aiste_node, "aiste_addr", &g_aiste_addr);
	if (ret) {
		g_aiste_addr = 0;
		pr_info("%s: get aiste_addr fail\n", __func__);
	}

	ret = of_property_read_u32(aiste_node, "aiste_size", &g_aiste_size);
	if (ret) {
		g_aiste_size = 0;
		pr_info("%s: get aiste_size fail\n", __func__);
	}

	if ((g_aiste_addr > 0) && (g_aiste_size > 0)) {
		g_aiste_buf_addr = ioremap_wc((phys_addr_t)g_aiste_addr,
			g_aiste_size);
		pr_info("aiste addr=0x%x, size=0x%x, buf_addr=0x%lx, %s\n",
			g_aiste_addr, g_aiste_size,
			(unsigned long)g_aiste_buf_addr, __func__);
	}

	return 0;
}

static const struct of_device_id mtk_aiste_of_match[] = {
	{ .compatible = "mediatek,aiste"},
	{},
};

static struct platform_driver mtk_aiste_driver = {
	.probe = mtk_aiste_probe,
	.driver	= {
		.name = "mtk-aiste",
		.of_match_table = mtk_aiste_of_match,
	},
};

static ssize_t ai_sys_tune_uclamp_min_write(struct file *file, const char __user *user_buf, size_t count, loff_t *pos)
{
	int ret;

	ret = kstrtou32_from_user(user_buf, count, 0, &g_uclamp_min);
	if (ret)
		return ret;

	return count;
}

static const struct proc_ops ai_sys_tune_uclamp_min_fops = {
	.proc_read = seq_read,
	.proc_write = ai_sys_tune_uclamp_min_write,
};

static ssize_t ai_sys_tune_uclamp_max_write(struct file *file, const char __user *user_buf, size_t count, loff_t *pos)
{
	int ret;

	ret = kstrtou32_from_user(user_buf, count, 0, &g_uclamp_max);
	if (ret)
		return ret;

	return count;
}

static const struct proc_ops ai_sys_tune_uclamp_max_fops = {
	.proc_read = seq_read,
	.proc_write = ai_sys_tune_uclamp_max_write,
};

int mtk_aiste_procfs_init(void)
{
	ai_sys_tune_root = proc_mkdir(AI_SYS_TUNE_DNAME, NULL);
	if (IS_ERR_OR_NULL(ai_sys_tune_root)) {
		pr_info("%s: failed to create debug dir %s\n",
			__func__, AI_SYS_TUNE_DNAME);
		return -EINVAL;
	}

	f_uclamp_min = proc_create(AI_SYS_TUNE_F_UCLAMP_MIN, 0644, ai_sys_tune_root,
					   &ai_sys_tune_uclamp_min_fops);
	if (IS_ERR_OR_NULL(f_uclamp_min)) {
		pr_info("%s: failed to create debug node %s\n",
			__func__, AI_SYS_TUNE_F_UCLAMP_MIN);
		proc_remove(ai_sys_tune_root);
	}
	f_uclamp_max = proc_create(AI_SYS_TUNE_F_UCLAMP_MAX, 0644, ai_sys_tune_root,
					   &ai_sys_tune_uclamp_max_fops);
	if (IS_ERR_OR_NULL(f_uclamp_max)) {
		pr_info("%s: failed to create debug node %s\n",
			__func__, AI_SYS_TUNE_F_UCLAMP_MAX);
		proc_remove(f_uclamp_min);
		proc_remove(ai_sys_tune_root);
	}
	return 0;
}

void mtk_aiste_procfs_remove(void)
{
	proc_remove(f_uclamp_max);
	proc_remove(f_uclamp_min);
	proc_remove(ai_sys_tune_root);
}

int aiste_init(struct apusys_core_info *info)
{
	int ret = 0;

	ret = platform_driver_register(&mtk_aiste_driver);
	if (ret < 0)
		pr_info("%s Failed to register mtk_aiste_driver, ret %d\n", __func__, ret);
	else
		pr_info("%s Success to register mtk_aiste_driver, ret %d\n", __func__, ret);

	mtk_aiste_procfs_init();
	aiste_scmi_init();
	return 0;
}

void aiste_exit(void)
{
	mtk_aiste_procfs_remove();
	platform_driver_unregister(&mtk_aiste_driver);
}
