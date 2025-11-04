// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/kvm.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>

#include <asm/kvm_pkvm_module.h>
#include <pkvm_mgmt/pkvm_mgmt.h>
#include "pkvm_mgmt_host.h"
#include "debug_protection.h"

static int hyp_pmm_debug_hypmmu_hcall;

static bool protection_status[TOTAL_PROTECTION];

static long fops_write(struct file *file, const char __user *buffer,
			  size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;
	long cmd;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return INVALID_COMMAND;
	desc[len] = '\0';
	if (kstrtol(desc, 10, &cmd) != 0)
		return INVALID_COMMAND;

	return cmd;
}

static ssize_t mgmt_cpu_protection_fops_write(struct file *file, const char __user *buffer,
			  size_t count, loff_t *data)
{
	long cmd;
	int ret;

	cmd = fops_write(file, buffer, count, data);
	if (cmd) {
		ret = pkvm_el2_mod_call(hyp_pmm_debug_hypmmu_hcall, ENABLE_CPU_PROTECTION);
		if (ret == 0)
			protection_status[CPU_PROTECTION] = true;
	} else {
		ret = pkvm_el2_mod_call(hyp_pmm_debug_hypmmu_hcall, DISABLE_CPU_PROTECTION);
		if (ret == 0)
			protection_status[CPU_PROTECTION] = false;
	}

	return count;
}

static ssize_t mgmt_gpu_protection_fops_write(struct file *file, const char __user *buffer,
			  size_t count, loff_t *data)
{
	long cmd;
	int ret;

	cmd = fops_write(file, buffer, count, data);
	if (cmd) {
		ret = pkvm_el2_mod_call(hyp_pmm_debug_hypmmu_hcall, ENABLE_GPU_PROTECTION);
		if (ret == 0)
			protection_status[GPU_PROTECTION] = true;
	} else {
		ret = pkvm_el2_mod_call(hyp_pmm_debug_hypmmu_hcall, DISABLE_GPU_PROTECTION);
		if (ret == 0)
			protection_status[GPU_PROTECTION] = false;
	}

	return count;
}

static ssize_t mgmt_inframpu_protection_fops_write(struct file *file, const char __user *buffer,
			  size_t count, loff_t *data)
{
	long cmd;
	int ret;

	cmd = fops_write(file, buffer, count, data);
	if (cmd) {
		ret = pkvm_el2_mod_call(hyp_pmm_debug_hypmmu_hcall, ENABLE_INFRA_MPU_PROTECTION);
		if (ret == 0)
			protection_status[INFRA_MPU_PROTECTION] = true;
	} else {
		ret = pkvm_el2_mod_call(hyp_pmm_debug_hypmmu_hcall, DISABLE_INFRA_MPU_PROTECTION);
		if (ret == 0)
			protection_status[INFRA_MPU_PROTECTION] = false;
	}

	return count;
}

/* Define proc_ops: *_proc_show function will be called when file is opened */
#define DEFINE_PROC_FOPS_RO(name)			\
	static const struct proc_ops name = {			\
		.proc_read		= seq_read,		\
		.proc_write		= name ## _write,	\
	}

DEFINE_PROC_FOPS_RO(mgmt_cpu_protection_fops);
DEFINE_PROC_FOPS_RO(mgmt_gpu_protection_fops);
DEFINE_PROC_FOPS_RO(mgmt_inframpu_protection_fops);

static bool is_loading_hypmmu(void)
{
	struct device_node *node = NULL;

	node = of_find_node_by_name(NULL, "pkvm");
	if (!node)
		goto out;

	node = of_find_node_by_name(node, "hypmmu");
	if (!node)
		goto out;

	return true;
out:
	return false;
}

static int mgmt_dump_show(struct seq_file *s, void *v)
{
	if (!s)
		return -EINVAL;

	seq_puts(s, "Protection Status:\n");
	seq_printf(s, "CPU       : %d\n", protection_status[CPU_PROTECTION]);
	if (is_loading_hypmmu()) {
		seq_printf(s, "GPU       : %d\n", protection_status[GPU_PROTECTION]);
		seq_printf(s, "INFRA-MPU : %d\n", protection_status[INFRA_MPU_PROTECTION]);
	}

	pkvm_el2_mod_call(hyp_pmm_debug_hypmmu_hcall, DUMP_PROTECTION_STATUS);

	return 0;
}

static int mgmt_dump_protection_open(struct inode *inode, struct file *file)
{
	return single_open(file, mgmt_dump_show, inode->i_private);
}

/* Define proc_ops: *_proc_show function will be called when file is opened */
#define DEFINE_PROC_OPEN_RO(name)           \
	static const struct proc_ops name = {			\
		.proc_open		= name ## _open,	\
		.proc_read		= seq_read,		\
	}

DEFINE_PROC_OPEN_RO(mgmt_dump_protection);

static int mgmt_create_debug_entry(void)
{
	struct proc_dir_entry *root_dir, *debug_root;

	root_dir = proc_mkdir("pkvm_mgmt", NULL);
	debug_root = proc_mkdir("debug", root_dir);
	proc_create_data("cpu", 0664, debug_root, &mgmt_cpu_protection_fops, NULL);
	proc_create_data("gpu", 0664, debug_root, &mgmt_gpu_protection_fops, NULL);
	proc_create_data("infra-mpu", 0664, debug_root, &mgmt_inframpu_protection_fops, NULL);
	proc_create_data("dump_protection_status", 0664, debug_root, &mgmt_dump_protection, NULL);

	return 0;
}

static int setup_hvc_call(void)
{
	struct arm_smccc_res res;

	hyp_pmm_debug_hypmmu_hcall = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(hyp_pmm_debug_hypmmu), mod_token);

	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
			  SMC_ID_MTK_PKVM_PMM_DEBUG_HYPMMU,
			  hyp_pmm_debug_hypmmu_hcall , 0, 0, 0, 0, &res);

	return 0;
}

int debug_protection_init(void)
{
	int i;

	for(i = 0; i < TOTAL_PROTECTION; i++)
		protection_status[i] = true;

	setup_hvc_call();

	mgmt_create_debug_entry();

	return 0;
}
