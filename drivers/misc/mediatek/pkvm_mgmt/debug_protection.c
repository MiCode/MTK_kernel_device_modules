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

static struct page *debug_page;

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

static ssize_t mgmt_smmu_protection_fops_write(struct file *file, const char __user *buffer,
			  size_t count, loff_t *data)
{
	long cmd;
	int ret;

	cmd = fops_write(file, buffer, count, data);
	if (cmd) {
		ret = pkvm_el2_mod_call(hyp_pmm_debug_hypmmu_hcall, ENABLE_SMMU_PROTECTION);
		if (ret == 0)
			protection_status[SMMU_PROTECTION] = true;
	} else {
		ret = pkvm_el2_mod_call(hyp_pmm_debug_hypmmu_hcall, DISABLE_SMMU_PROTECTION);
		if (ret == 0)
			protection_status[SMMU_PROTECTION] = false;
	}

	return count;
}

#define DEFINE_PROC_WRITE_FOPS(name)			\
	static const struct proc_ops name = {			\
		.proc_read		= seq_read,		\
		.proc_write		= name ## _write,	\
	}

DEFINE_PROC_WRITE_FOPS(mgmt_cpu_protection_fops);
DEFINE_PROC_WRITE_FOPS(mgmt_gpu_protection_fops);
DEFINE_PROC_WRITE_FOPS(mgmt_inframpu_protection_fops);
DEFINE_PROC_WRITE_FOPS(mgmt_smmu_protection_fops);

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
	} else
		seq_printf(s, "SMMU : %d\n", protection_status[SMMU_PROTECTION]);


	pkvm_el2_mod_call(hyp_pmm_debug_hypmmu_hcall, DUMP_PROTECTION_STATUS);

	return 0;
}

static int mgmt_dump_protection_open(struct inode *inode, struct file *file)
{
	return single_open(file, mgmt_dump_show, inode->i_private);
}

static void get_avg_max_min_time(u32 start_idx, u64 *avg_time, u64 *max_time, u64 *min_time)
{
	void *pgtbl_virt = page_to_virt(debug_page);
	int i, use_idx = 1;
	u64 total_time = 0;

	*max_time = ((u64 *)pgtbl_virt)[start_idx];
	*min_time = ((u64 *)pgtbl_virt)[start_idx];
	for (i = start_idx; i < (start_idx + MAX_RECORD_TIMES); i++) {
		if(((u64 *)pgtbl_virt)[i] > *max_time)
			*max_time = ((u64 *)pgtbl_virt)[i];

		if(((u64 *)pgtbl_virt)[i] < *min_time)
			*min_time = ((u64 *)pgtbl_virt)[i];

		total_time += ((u64 *)pgtbl_virt)[i];
		if (((u64 *)pgtbl_virt)[i])
			use_idx++;
	}
	*avg_time = total_time / use_idx;
}

static void show_pgtbl(char *name, u32 start_idx, struct seq_file *s)
{
	void *pgtbl_virt;
	u64 avg_time, max_time, min_time;
	int i, j;

	pgtbl_virt = page_to_virt(debug_page);

	get_avg_max_min_time(start_idx, &avg_time, &max_time, &min_time);

	seq_printf(s, "%s pgtbl time:\n", name);
	seq_printf(s, " - avg time : %llu us\n", avg_time);
	seq_printf(s, " - max time : %llu us\n", max_time);
	if (min_time)
		seq_printf(s, " - min time : %llu us\n", min_time);
	else
		seq_puts(s, " - min time : < 1 us\n");
	seq_puts(s, "\n");
	seq_printf(s, "%s pgtbl:", name);
	seq_puts(s, "\n");
	for (i = start_idx, j = 1; i < (start_idx + MAX_RECORD_TIMES); i++,j++) {
		if (j < 10)
			seq_printf(s, " %d: %llu us,\t", j, (((u64 *)pgtbl_virt)[i]));
		else
			seq_printf(s, "%d: %llu us,\t", j, (((u64 *)pgtbl_virt)[i]));
		if (j % ONE_LINE_LEN == 0)
			seq_puts(s, "\n");
	}
	seq_puts(s, "----------------------------------------------------------------------");
	seq_puts(s, "----------------------------------------------------------------------");
	seq_puts(s, "-----------------\n\n");
}

static int mgmt_cpu_perf_show(struct seq_file *s, void *v)
{
	if (!s)
		return -EINVAL;

	show_pgtbl("CPU", CPU_PGTBL_BASE_OFFSET, s);

	return 0;
}

static int mgmt_cpu_perf_pgtbl_open(struct inode *inode, struct file *file)
{
	return single_open(file, mgmt_cpu_perf_show, inode->i_private);
}

static int mgmt_gpu_perf_show(struct seq_file *s, void *v)
{
	if (!s)
		return -EINVAL;

	show_pgtbl("GPU", GPU_PGTBL_BASE_OFFSET, s);

	return 0;
}

static int mgmt_gpu_perf_pgtbl_open(struct inode *inode, struct file *file)
{
	return single_open(file, mgmt_gpu_perf_show, inode->i_private);
}

static int mgmt_iommu_perf_show(struct seq_file *s, void *v)
{
	if (!s)
		return -EINVAL;

	show_pgtbl("IOMMU", IOMMU_PGTBL_BASE_OFFSET, s);

	return 0;
}

static int mgmt_iommu_perf_pgtbl_open(struct inode *inode, struct file *file)
{
	return single_open(file, mgmt_iommu_perf_show, inode->i_private);
}

static int mgmt_inframpu_perf_show(struct seq_file *s, void *v)
{
	if (!s)
		return -EINVAL;

	show_pgtbl("Infra-MPU", IMPU_PGTBL_BASE_OFFSET, s);

	return 0;
}

static int mgmt_inframpu_perf_pgtbl_open(struct inode *inode, struct file *file)
{
	return single_open(file, mgmt_inframpu_perf_show, inode->i_private);
}

static int mgmt_smmu_perf_show(struct seq_file *s, void *v)
{
	if (!s)
		return -EINVAL;

	show_pgtbl("SMMU", SMMU_PGTBL_BASE_OFFSET, s);

	return 0;
}

static int mgmt_smmu_perf_pgtbl_open(struct inode *inode, struct file *file)
{
	return single_open(file, mgmt_smmu_perf_show, inode->i_private);
}

static int mgmt_all_perf_show(struct seq_file *s, void *v)
{
	if (!s)
		return -EINVAL;

	show_pgtbl("CPU", CPU_PGTBL_BASE_OFFSET, s);
	if (is_loading_hypmmu()) {
		show_pgtbl("GPU", GPU_PGTBL_BASE_OFFSET, s);
		show_pgtbl("IOMMU", IOMMU_PGTBL_BASE_OFFSET, s);
		show_pgtbl("Infra-MPU", IMPU_PGTBL_BASE_OFFSET, s);
	} else
		show_pgtbl("SMMU", SMMU_PGTBL_BASE_OFFSET, s);

	return 0;
}

static int mgmt_all_perf_pgtbl_open(struct inode *inode, struct file *file)
{
	return single_open(file, mgmt_all_perf_show, inode->i_private);
}

static int mgmt_reset_perf_show(struct seq_file *s, void *v)
{
	if (!s)
		return -EINVAL;

	pkvm_el2_mod_call(hyp_pmm_debug_hypmmu_hcall, RESET_PGTBL_TIME, PGTBL_PAGE_ORDER);
	seq_puts(s, "reset all pgtbl time\n");

	return 0;
}

static int mgmt_reset_perf_pgtbl_open(struct inode *inode, struct file *file)
{
	return single_open(file, mgmt_reset_perf_show, inode->i_private);
}

/* Define proc_ops: *_proc_show function will be called when file is opened */
#define DEFINE_PROC_OPEN_RO(name)           \
	static const struct proc_ops name = {			\
		.proc_open		= name ## _open,	\
		.proc_read		= seq_read,		\
	}

DEFINE_PROC_OPEN_RO(mgmt_dump_protection);
DEFINE_PROC_OPEN_RO(mgmt_cpu_perf_pgtbl);
DEFINE_PROC_OPEN_RO(mgmt_gpu_perf_pgtbl);
DEFINE_PROC_OPEN_RO(mgmt_iommu_perf_pgtbl);
DEFINE_PROC_OPEN_RO(mgmt_inframpu_perf_pgtbl);
DEFINE_PROC_OPEN_RO(mgmt_smmu_perf_pgtbl);
DEFINE_PROC_OPEN_RO(mgmt_all_perf_pgtbl);
DEFINE_PROC_OPEN_RO(mgmt_reset_perf_pgtbl);

static int mgmt_create_debug_entry(void)
{
	struct proc_dir_entry *root_dir, *debug_root, *perf_root;

	root_dir = proc_mkdir("pkvm_mgmt", NULL);
	debug_root = proc_mkdir("debug", root_dir);
	proc_create_data("cpu", 0664, debug_root, &mgmt_cpu_protection_fops, NULL);
	if (is_loading_hypmmu()) {
		proc_create_data("gpu", 0664, debug_root, &mgmt_gpu_protection_fops, NULL);
		proc_create_data("infra-mpu", 0664, debug_root, &mgmt_inframpu_protection_fops, NULL);
	} else
		proc_create_data("smmu", 0664, debug_root, &mgmt_smmu_protection_fops, NULL);
	proc_create_data("dump_protection_status", 0664, debug_root, &mgmt_dump_protection, NULL);

	perf_root = proc_mkdir("pgtbl_time", root_dir);
	proc_create_data("cpu", 0664, perf_root, &mgmt_cpu_perf_pgtbl, NULL);
	if (is_loading_hypmmu()) {
		proc_create_data("gpu", 0664, perf_root, &mgmt_gpu_perf_pgtbl, NULL);
		proc_create_data("iommu", 0664, perf_root, &mgmt_iommu_perf_pgtbl, NULL);
		proc_create_data("infra-mpu", 0664, perf_root, &mgmt_inframpu_perf_pgtbl, NULL);
	} else
		proc_create_data("smmu", 0664, perf_root, &mgmt_smmu_perf_pgtbl, NULL);
	proc_create_data("all", 0664, perf_root, &mgmt_all_perf_pgtbl, NULL);
	proc_create_data("reset", 0664, perf_root, &mgmt_reset_perf_pgtbl, NULL);

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
	int i, ret;

	for(i = 0; i < TOTAL_PROTECTION; i++)
		protection_status[i] = true;

	setup_hvc_call();

	mgmt_create_debug_entry();

	debug_page = alloc_pages(GFP_KERNEL | __GFP_ZERO, PGTBL_PAGE_ORDER);
	ret = pkvm_el2_mod_call(hyp_pmm_debug_hypmmu_hcall, INIT_DEBUG_PAGE,
			page_to_phys(debug_page) >> PAGE_SHIFT, PGTBL_PAGE_ORDER);
	if (ret != 0)
		pr_info("INIT_DEBUG_PAGE fail\n");

	return 0;
}
