// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/kconfig.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/dma-buf.h>

#include <slbc.h>

unsigned int slbc_enable;
unsigned int slbc_all_cache_mode;

EXPORT_SYMBOL_GPL(slbc_enable);
EXPORT_SYMBOL_GPL(slbc_all_cache_mode);


struct slbc_common_ops *common_ops;

/* need to modify enum slbc_uid */
char *slbc_uid_str[UID_MAX + 1] = {
	"UID_ZERO",
	"UID_MM_VENC",
	"UID_MM_DISP",
	"UID_MM_MDP",
	"UID_MM_VDEC",
	"UID_AI_MDLA",
	"UID_AI_ISP",
	"UID_GPU",
	"UID_HIFI3",
	"UID_CPU",
	"UID_AOV",
	"UID_SH_P2",
	"UID_SH_APU",
	"UID_MML",
	"UID_DSC_IDLE",
	"UID_AINR",
	"UID_TEST_BUFFER",
	"UID_TEST_CACHE",
	"UID_TEST_ACP",
	"UID_DISP",
	"UID_AOV_DC",
	"UID_AOV_APU",
	"UID_AISR_APU",
	"UID_AISR_MML",
	"UID_SH_P1",
	"UID_SMT",
	"UID_APU",
	"UID_AOD",
	"UID_BIF",
	"UID_MM_VENC_SL",
	"UID_SENSOR",
	"UID_MM_VENC_FHD",
	"UID_MAX",
};
EXPORT_SYMBOL_GPL(slbc_uid_str);

/* need to modify enum slc_ach_uid */
char *slc_ach_uid_str[ID_MAX + 1] = {
	"ID_PD",
	"ID_CPU",
	"ID_GPU",
	"ID_GPU_W",
	"ID_OVL_R",
	"ID_VDEC_FRAME",
	"ID_VDEC_UBE",
	"ID_SMMU",
	"ID_MD",
	"ID_ADSP",
	"ID_MAX",
};
EXPORT_SYMBOL_GPL(slc_ach_uid_str);

/* bit count */
int popcount(unsigned int x)
{
	x = x - ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
	x = (x + (x >> 4)) & 0x0F0F0F0F;
	x = x + (x >> 8);
	x = x + (x >> 16);

	/* pr_info("popcount %d\n", x & 0x0000003F); */

	return x & 0x0000003F;
}
EXPORT_SYMBOL_GPL(popcount);

u32 slbc_sram_read(u32 offset)
{
	if (common_ops && common_ops->slbc_sram_read)
		return common_ops->slbc_sram_read(offset);
	else
		return 0;
}
EXPORT_SYMBOL_GPL(slbc_sram_read);

void slbc_sram_write(u32 offset, u32 val)
{
	if (common_ops && common_ops->slbc_sram_write)
		return common_ops->slbc_sram_write(offset, val);
	else
		return;
}
EXPORT_SYMBOL_GPL(slbc_sram_write);

int slbc_status(struct slbc_data *d)
{
	if (common_ops && common_ops->slbc_status)
		return common_ops->slbc_status(d);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_status);

int slbc_request(struct slbc_data *d)
{
	if (common_ops && common_ops->slbc_request)
		return common_ops->slbc_request(d);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_request);

int slbc_release(struct slbc_data *d)
{
	if (common_ops && common_ops->slbc_release)
		return common_ops->slbc_release(d);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_release);

int slbc_power_on(struct slbc_data *d)
{
	if (common_ops && common_ops->slbc_power_on)
		return common_ops->slbc_power_on(d);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_power_on);

int slbc_power_off(struct slbc_data *d)
{
	if (common_ops && common_ops->slbc_power_off)
		return common_ops->slbc_power_off(d);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_power_off);

int slbc_secure_on(struct slbc_data *d)
{
	if (common_ops && common_ops->slbc_secure_on)
		return common_ops->slbc_secure_on(d);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_secure_on);

int slbc_secure_off(struct slbc_data *d)
{
	if (common_ops && common_ops->slbc_secure_off)
		return common_ops->slbc_secure_off(d);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_secure_off);

int slbc_register_activate_ops(struct slbc_ops *ops)
{
	if (common_ops && common_ops->slbc_register_activate_ops)
		return common_ops->slbc_register_activate_ops(ops);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_register_activate_ops);

int slbc_activate_status(struct slbc_data *d)
{
	if (common_ops && common_ops->slbc_activate_status)
		return common_ops->slbc_activate_status(d);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_activate_status);

void slbc_update_mm_bw(unsigned int bw)
{
	if (common_ops && common_ops->slbc_update_mm_bw)
		return common_ops->slbc_update_mm_bw(bw);
	else
		return;
}
EXPORT_SYMBOL_GPL(slbc_update_mm_bw);

void slbc_update_mic_num(unsigned int num)
{
	if (common_ops && common_ops->slbc_update_mic_num)
		return common_ops->slbc_update_mic_num(num);
	else
		return;
}
EXPORT_SYMBOL_GPL(slbc_update_mic_num);

int slbc_gid_val(enum slc_ach_uid uid)
{
	if (common_ops && common_ops->slbc_gid_val)
		return common_ops->slbc_gid_val(uid);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_gid_val);

int slbc_gid_request(enum slc_ach_uid uid, int *gid, struct slbc_gid_data *d)
{
	if (common_ops && common_ops->slbc_gid_request)
		return common_ops->slbc_gid_request(uid, gid, d);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_gid_request);

int slbc_gid_release(enum slc_ach_uid uid, int gid)
{
	if (common_ops && common_ops->slbc_gid_release)
		return common_ops->slbc_gid_release(uid, gid);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_gid_release);

int slbc_roi_update(enum slc_ach_uid uid, int gid, struct slbc_gid_data *d)
{
	if (common_ops && common_ops->slbc_roi_update)
		return common_ops->slbc_roi_update(uid, gid, d);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_roi_update);

int slbc_validate(enum slc_ach_uid uid, int gid)
{
	if (common_ops && common_ops->slbc_validate)
		return common_ops->slbc_validate(uid, gid);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_validate);

int slbc_invalidate(enum slc_ach_uid uid, int gid)
{
	if (common_ops && common_ops->slbc_invalidate)
		return common_ops->slbc_invalidate(uid, gid);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_invalidate);

int slbc_read_invalidate(enum slc_ach_uid uid, int gid, int enable)
{
	if (common_ops && common_ops->slbc_read_invalidate)
		return common_ops->slbc_read_invalidate(uid, gid, enable);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_read_invalidate);

int slbc_force_cache(enum slc_ach_uid uid, unsigned int size)
{
	if (common_ops && common_ops->slbc_force_cache)
		return common_ops->slbc_force_cache(uid, size);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_force_cache);

int slbc_ceil(enum slc_ach_uid uid, unsigned int ceil)
{
	if (common_ops && common_ops->slbc_ceil)
		return common_ops->slbc_ceil(uid, ceil);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_ceil);

int slbc_window(unsigned int window)
{
	if (common_ops && common_ops->slbc_window)
		return common_ops->slbc_window(window);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_window);

int slbc_get_cache_size(enum slc_ach_uid uid)
{
	if (common_ops && common_ops->slbc_get_cache_size)
		return common_ops->slbc_get_cache_size(uid);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_get_cache_size);

int slbc_get_cache_hit_rate(enum slc_ach_uid uid)
{
	if (common_ops && common_ops->slbc_get_cache_hit_rate)
		return common_ops->slbc_get_cache_hit_rate(uid);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_get_cache_hit_rate);

int slbc_get_cache_hit_bw(enum slc_ach_uid uid)
{
	if (common_ops && common_ops->slbc_get_cache_hit_bw)
		return common_ops->slbc_get_cache_hit_bw(uid);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_get_cache_hit_bw);

int slbc_get_cache_usage(int *cpu, int *gpu, int *other)
{
	if (common_ops && common_ops->slbc_get_cache_usage)
		return common_ops->slbc_get_cache_usage(cpu, gpu, other);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_get_cache_usage);

void slbc_register_common_ops(struct slbc_common_ops *ops)
{
	common_ops = ops;
}
EXPORT_SYMBOL_GPL(slbc_register_common_ops);

void slbc_unregister_common_ops(struct slbc_common_ops *ops)
{
	common_ops = NULL;
}
EXPORT_SYMBOL_GPL(slbc_unregister_common_ops);

int __init slbc_common_module_init(void)
{
	return 0;
}

late_initcall(slbc_common_module_init);

MODULE_DESCRIPTION("SLBC Driver common v0.1");
MODULE_LICENSE("GPL");
