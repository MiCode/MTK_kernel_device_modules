// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

/**
 * @file    gpueb_debug.c
 * @brief   Debug mechanism for GPU-DVFS
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#include "gpueb_helper.h"
#include "ghpm_debug.h"
#include "gpueb_ipi.h"
#include "ghpm_wrapper.h"
#include "gpueb_common.h"

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */

#if GHPM_TEST
static void gpuhre_read_backup(void);
static void gpuhre_write_random(void);
static void gpuhre_check_restore(void);

static void gpuhre_read_backup(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(
		MTK_SIP_KERNEL_GPUEB_CONTROL,  /* a0 */
		GPUHRE_SMP_OP_READ_BACKUP,     /* a1 */
		0, 0, 0, 0, 0, 0, &res);

	gpueb_pr_info(GHPM_TAG, "done");
}

static void gpuhre_write_random(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(
		MTK_SIP_KERNEL_GPUEB_CONTROL,  /* a0 */
		GPUHRE_SMP_OP_WRITE_RANDOM,    /* a1 */
		0, 0, 0, 0, 0, 0, &res);
	gpueb_pr_info(GHPM_TAG, "done");
}

static void gpuhre_check_restore(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(
		MTK_SIP_KERNEL_GPUEB_CONTROL,  /* a0 */
		GPUHRE_SMP_OP_CHECK_RESTORE,   /* a1 */
		0, 0, 0, 0, 0, 0, &res);
	gpueb_pr_info(GHPM_TAG, "done");
}

#if defined(CONFIG_PROC_FS)
static int ghpm_ctrl_test_proc_show(struct seq_file *m, void *v)
{
	int ret = 0;

	seq_puts(m, "GHPM TEST\n");

	ret = ghpm_ctrl(GHPM_OFF, MFG1_OFF);
	if (ret)
		gpueb_pr_err(GHPM_TAG, "ghpm_ctrl off failed, ret=%d", ret);
	else
		gpueb_pr_info(GHPM_TAG, "ghpm_ctrl off done");

	ret = wait_gpueb(SUSPEND_POWER_OFF);
	if (ret)
		gpueb_pr_err(GHPM_TAG, "wait_gpueb suspend failed, ret=%d", ret);
	else
		gpueb_pr_info(GHPM_TAG, "wait_gpueb suspend done");

	ret = ghpm_ctrl(GHPM_ON, MFG1_OFF);
	if (ret)
		gpueb_pr_err(GHPM_TAG, "ghpm_ctrl on failed, ret=%d", ret);
	else
		gpueb_pr_info(GHPM_TAG, "ghpm_ctrl on done");

	ret = wait_gpueb(SUSPEND_POWER_ON);
	if (ret)
		gpueb_pr_err(GHPM_TAG, "wait_gpueb resume failed, ret=%d", ret);
	else
		gpueb_pr_info(GHPM_TAG, "wait_gpueb resume done");

	return 0;
}

static ssize_t ghpm_ctrl_test_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = 0;
	char buf[64];
	unsigned int len = 0;
	int state, vcore_off_allow;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = -1;
		goto done;
	}
	buf[len] = '\0';

	if (sscanf(buf, "%d %d", &state, &vcore_off_allow) == 2) {
		ret = ghpm_ctrl(state, vcore_off_allow);
		if (ret)
			gpueb_pr_err(GHPM_TAG, "ghpm_ctrl failed, ret=%d", ret);
		else
			gpueb_pr_info(GHPM_TAG, "ghpm_ctrl done");

		ret = wait_gpueb(state);
		if (ret)
			gpueb_pr_err(GHPM_TAG, "wait_gpueb failed, ret=%d", ret);
		else
			gpueb_pr_info(GHPM_TAG, "wait_gpueb done");
	}

done:
	return (ret < 0) ? ret : count;
}

static int gpuhre_read_backup_proc_show(struct seq_file *m, void *v)
{
	gpueb_pr_info(GHPM_TAG, "read reg");
	gpuhre_read_backup();
	return 0;
}

static int gpuhre_write_random_proc_show(struct seq_file *m, void *v)
{
	gpueb_pr_info(GHPM_TAG, "write random value");
	gpuhre_write_random();
	return 0;
}

static int gpuhre_check_restore_proc_show(struct seq_file *m, void *v)
{
	gpueb_pr_info(GHPM_TAG, "check restore");
	gpuhre_check_restore();
	return 0;
}

/* PROCFS : initialization */
PROC_FOPS_RW(ghpm_ctrl_test);
PROC_FOPS_RO(gpuhre_read_backup);
PROC_FOPS_RO(gpuhre_write_random);
PROC_FOPS_RO(gpuhre_check_restore);

static int ghpm_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i = 0;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};

	const struct pentry default_entries[] = {
#if GHPM_TEST
		PROC_ENTRY(ghpm_ctrl_test),
		PROC_ENTRY(gpuhre_read_backup),
		PROC_ENTRY(gpuhre_write_random),
		PROC_ENTRY(gpuhre_check_restore)
#endif
	};

	dir = proc_mkdir("ghpm", NULL);
	if (!dir) {
		gpueb_pr_info(GHPM_TAG, "fail to create /proc/ghpm (ENOMEM)");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(default_entries); i++) {
		if (!proc_create(default_entries[i].name, 0660,
			dir, default_entries[i].fops))
			gpueb_pr_info(GHPM_TAG, "fail to create /proc/ghpm/%s", default_entries[i].name);
	}

	return 0;
}
#endif /* CONFIG_PROC_FS */
#endif /* GHPM_TEST*/

void ghpm_debug_init(struct platform_device *pdev)
{
#if !GHPM_TEST
	gpueb_pr_debug(GHPM_TAG, "GHPM_TEST is not enabled");
	return;
#elif defined(CONFIG_PROC_FS)
	int ret = 0;

	gpueb_pr_info(GHPM_TAG, "ghpm_create_procfs");
	ret = ghpm_create_procfs();
	if (ret)
		gpueb_pr_info(GHPM_TAG, "fail to create procfs (%d)", ret);
#else
	gpueb_pr_info(GHPM_TAG, "CONFIG_PROC_FS is not enabled");
	return;
#endif
}
