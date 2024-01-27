// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chia-Mao Hung <chia-mao.hung@mediatek.com>
 */
#include <linux/module.h>
#include "vcp_status.h"
#include "vcp.h"

struct vcp_status_fp *vcp_fp;
mminfra_dump_ptr mminfra_debug_dump;
EXPORT_SYMBOL_GPL(mminfra_debug_dump);

int pwclkcnt;
EXPORT_SYMBOL_GPL(pwclkcnt);
bool is_suspending;
EXPORT_SYMBOL_GPL(is_suspending);

static int __init mtk_vcp_status_init(void)
{
	pwclkcnt = 0;
	return 0;
}

int mmup_enable_count(void)
{
	return pwclkcnt;
}
EXPORT_SYMBOL_GPL(mmup_enable_count);

void vcp_set_fp(struct vcp_status_fp *fp)
{
	if (!fp)
		return;
	vcp_fp = fp;
}
EXPORT_SYMBOL_GPL(vcp_set_fp);

struct mtk_ipi_device *vcp_get_ipidev(enum feature_id id)
{
	if (!vcp_fp || !vcp_fp->get_ipidev)
		return NULL;
	return vcp_fp->get_ipidev(id);
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

phys_addr_t vcp_get_reserve_mem_size_ex(enum vcp_reserve_mem_id_t id)
{
	if (!vcp_fp || !vcp_fp->vcp_get_reserve_mem_size)
		return 0;
	return vcp_fp->vcp_get_reserve_mem_size(id);
}
EXPORT_SYMBOL_GPL(vcp_get_reserve_mem_size_ex);

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

unsigned int is_vcp_ready_ex(enum feature_id id)
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

unsigned int is_vcp_ao_ex(void)
{
	if (!vcp_fp || !vcp_fp->is_vcp_ao)
		return 0;
	return vcp_fp->is_vcp_ao();
}
EXPORT_SYMBOL_GPL(is_vcp_ao_ex);

void vcp_A_register_notify_ex(enum feature_id id, struct notifier_block *nb)
{
	if (!vcp_fp || !vcp_fp->vcp_A_register_notify)
		return;
	vcp_fp->vcp_A_register_notify(id, nb);
}
EXPORT_SYMBOL_GPL(vcp_A_register_notify_ex);

void vcp_A_unregister_notify_ex(enum feature_id id, struct notifier_block *nb)
{
	if (!vcp_fp || !vcp_fp->vcp_A_unregister_notify)
		return;
	vcp_fp->vcp_A_unregister_notify(id, nb);
}
EXPORT_SYMBOL_GPL(vcp_A_unregister_notify_ex);

unsigned int vcp_cmd_ex(enum feature_id id, enum vcp_cmd_id cmd_id, char *user)
{
	if (!vcp_fp || !vcp_fp->vcp_cmd)
		return 0;
	return vcp_fp->vcp_cmd(id, cmd_id, user);
}
EXPORT_SYMBOL_GPL(vcp_cmd_ex);

int vcp_register_mminfra_cb_ex(mminfra_pwr_ptr fpt_on, mminfra_pwr_ptr fpt_off,
	mminfra_dump_ptr mminfra_dump_func)
{
	return 0;
}
EXPORT_SYMBOL_GPL(vcp_register_mminfra_cb_ex);

static void __exit mtk_vcp_status_exit(void)
{
}
module_init(mtk_vcp_status_init);
module_exit(mtk_vcp_status_exit);
MODULE_LICENSE("GPL");
