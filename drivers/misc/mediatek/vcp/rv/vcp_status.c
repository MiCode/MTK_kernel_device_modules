// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chia-Mao Hung <chia-mao.hung@mediatek.com>
 */
#include <linux/module.h>
#include "vcp_status.h"
#include "vcp.h"

struct vcp_status_fp *vcp_fp;
struct mtk_ipi_device *vcp_ipidev_ex;
struct vcp_mminfra_on_off_st *vcp_mminfra_cb_ptr;
mminfra_dump_ptr mminfra_debug_dump;
EXPORT_SYMBOL_GPL(mminfra_debug_dump);

int pwclkcnt;
EXPORT_SYMBOL_GPL(pwclkcnt);
bool is_suspending;
EXPORT_SYMBOL_GPL(is_suspending);
bool vcp_ao;
EXPORT_SYMBOL_GPL(vcp_ao);

static int __init mtk_vcp_status_init(void)
{
	pwclkcnt = 0;
	return 0;
}

int mmup_enable_count(void)
{
	if (vcp_ao)
		return pwclkcnt;

	return ((is_suspending) ? 0 : pwclkcnt);
}
EXPORT_SYMBOL_GPL(mmup_enable_count);

void vcp_set_fp(struct vcp_status_fp *fp)
{
	if (!fp)
		return;
	vcp_fp = fp;
}
EXPORT_SYMBOL_GPL(vcp_set_fp);

void vcp_set_ipidev(struct mtk_ipi_device *ipidev)
{
	if (!ipidev)
		return;
	vcp_ipidev_ex = ipidev;
}
EXPORT_SYMBOL_GPL(vcp_set_ipidev);

struct mtk_ipi_device *vcp_get_ipidev(void)
{
	return vcp_ipidev_ex;
}
EXPORT_SYMBOL_GPL(vcp_get_ipidev);

phys_addr_t vcp_get_reserve_mem_phys_ex(enum vcp_reserve_mem_id_t id)
{
	if (!vcp_fp || !vcp_fp->vcp_get_reserve_mem_phys)
		return 0;
	return vcp_fp->vcp_get_reserve_mem_phys(id);
}
EXPORT_SYMBOL_GPL(vcp_get_reserve_mem_phys_ex);

phys_addr_t vcp_get_reserve_mem_virt_ex(enum vcp_reserve_mem_id_t id)
{
	if (!vcp_fp || !vcp_fp->vcp_get_reserve_mem_virt)
		return 0;
	return vcp_fp->vcp_get_reserve_mem_virt(id);
}
EXPORT_SYMBOL_GPL(vcp_get_reserve_mem_virt_ex);

int vcp_register_feature_ex(enum feature_id id)
{
	if (!vcp_fp || !vcp_fp->vcp_register_feature)
		return -1;
	return vcp_fp->vcp_register_feature(id);
}
EXPORT_SYMBOL_GPL(vcp_register_feature_ex);

int vcp_deregister_feature_ex(enum feature_id id)
{
	if (!vcp_fp || !vcp_fp->vcp_deregister_feature)
		return -1;
	return vcp_fp->vcp_deregister_feature(id);
}
EXPORT_SYMBOL_GPL(vcp_deregister_feature_ex);

unsigned int is_vcp_ready_ex(enum vcp_core_id id)
{
	if (!vcp_fp || !vcp_fp->is_vcp_ready)
		return 0;
	return vcp_fp->is_vcp_ready(id);
}
EXPORT_SYMBOL_GPL(is_vcp_ready_ex);

unsigned int is_vcp_suspending_ex(void)
{
	if (!vcp_fp || !vcp_fp->is_vcp_suspending)
		return 0;
	return vcp_fp->is_vcp_suspending();
}
EXPORT_SYMBOL_GPL(is_vcp_suspending_ex);

void vcp_A_register_notify_ex(struct notifier_block *nb)
{
	if (!vcp_fp || !vcp_fp->vcp_A_register_notify)
		return;
	vcp_fp->vcp_A_register_notify(nb);
}
EXPORT_SYMBOL_GPL(vcp_A_register_notify_ex);

void vcp_A_unregister_notify_ex(struct notifier_block *nb)
{
	if (!vcp_fp || !vcp_fp->vcp_A_unregister_notify)
		return;
	vcp_fp->vcp_A_unregister_notify(nb);
}
EXPORT_SYMBOL_GPL(vcp_A_unregister_notify_ex);

unsigned int vcp_cmd_ex(enum vcp_cmd_id id, char *user)
{
	if (!vcp_fp || !vcp_fp->vcp_cmd)
		return 0;
	return vcp_fp->vcp_cmd(id, user);
}
EXPORT_SYMBOL_GPL(vcp_cmd_ex);

void vcp_set_mminfra_cb(struct vcp_mminfra_on_off_st *str_ptr)
{
	if (!str_ptr)
		return;
	vcp_mminfra_cb_ptr = str_ptr;
}
EXPORT_SYMBOL_GPL(vcp_set_mminfra_cb);

int vcp_register_mminfra_cb_ex(mminfra_pwr_ptr fpt_on, mminfra_pwr_ptr fpt_off,
	mminfra_dump_ptr mminfra_dump_func)
{
	if(!vcp_mminfra_cb_ptr)
		return -1;
	vcp_mminfra_cb_ptr->mminfra_on = fpt_on;
	vcp_mminfra_cb_ptr->mminfra_off = fpt_off;
	mminfra_debug_dump = mminfra_dump_func;
	return 0;
}
EXPORT_SYMBOL_GPL(vcp_register_mminfra_cb_ex);

static void __exit mtk_vcp_status_exit(void)
{
}
module_init(mtk_vcp_status_init);
module_exit(mtk_vcp_status_exit);
MODULE_LICENSE("GPL v2");
