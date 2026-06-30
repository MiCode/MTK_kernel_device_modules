// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include "mtk-afe-external.h"
#include "mtk-adspscp-external.h"
#include <linux/module.h>
#include <linux/errno.h>

static RAW_NOTIFIER_HEAD(afe_mem_init_noitify_chain);
static ATOMIC_NOTIFIER_HEAD(semaphore_noitify_chain);
static RAW_NOTIFIER_HEAD(vp_audio_noitify_chain);
static RAW_NOTIFIER_HEAD(vow_ipi_send_noitify_chain);
static RAW_NOTIFIER_HEAD(ultra_afe_hw_free_noitify_chain);
void* (*afe_get_power_scenario)(void) = NULL;

/* memory allocate */
int register_afe_allocate_mem_notifier(struct notifier_block *nb)
{
	int status;

	status = raw_notifier_chain_register(&afe_mem_init_noitify_chain, nb);
	return status;
}
EXPORT_SYMBOL_GPL(register_afe_allocate_mem_notifier);

int unregister_afe_allocate_mem_notifier(struct notifier_block *nb)
{
	int status;

	status = raw_notifier_chain_unregister(&afe_mem_init_noitify_chain, nb);
	return status;
}
EXPORT_SYMBOL_GPL(unregister_afe_allocate_mem_notifier);

int notify_allocate_mem(unsigned long module, void *v)
{
	return raw_notifier_call_chain(&afe_mem_init_noitify_chain, module, v);
}
EXPORT_SYMBOL_GPL(notify_allocate_mem);

/* vp_audio_message */
int register_vp_audio_notifier(struct notifier_block *nb)
{
	int status;

	status = raw_notifier_chain_register(&vp_audio_noitify_chain, nb);
	return status;
}
EXPORT_SYMBOL_GPL(register_vp_audio_notifier);

int unregister_vp_audio_notifier(struct notifier_block *nb)
{
	int status;

	status = raw_notifier_chain_unregister(&vp_audio_noitify_chain, nb);
	return status;
}
EXPORT_SYMBOL_GPL(unregister_vp_audio_notifier);

int notify_vb_audio_control(unsigned long module, void *v)
{
	return raw_notifier_call_chain(&vp_audio_noitify_chain, module, v);
}
EXPORT_SYMBOL_GPL(notify_vb_audio_control);

/* semaphore control */
int register_3way_semaphore_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&semaphore_noitify_chain, nb);
}
EXPORT_SYMBOL_GPL(register_3way_semaphore_notifier);

int unregister_3way_semaphore_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&semaphore_noitify_chain, nb);
}
EXPORT_SYMBOL_GPL(unregister_3way_semaphore_notifier);

int notify_3way_semaphore_control(unsigned long module, void *v)
{
	return atomic_notifier_call_chain(&semaphore_noitify_chain, module, v);
}
EXPORT_SYMBOL_GPL(notify_3way_semaphore_control);

/* vow send ipi */
int register_vow_ipi_send_notifier(struct notifier_block *nb)
{
	int status;

	status = raw_notifier_chain_register(&vow_ipi_send_noitify_chain, nb);
	return status;
}
EXPORT_SYMBOL_GPL(register_vow_ipi_send_notifier);

int unregister_vow_ipi_send_notifier(struct notifier_block *nb)
{
	int status;

	status = raw_notifier_chain_unregister(&vow_ipi_send_noitify_chain, nb);
	return status;
}
EXPORT_SYMBOL_GPL(unregister_vow_ipi_send_notifier);

int notify_vow_ipi_send(unsigned long module, void *v)
{
	return raw_notifier_call_chain(&vow_ipi_send_noitify_chain, module, v);
}
EXPORT_SYMBOL_GPL(notify_vow_ipi_send);

/* ultrasound register notify for AFE hw free */
int register_ultra_afe_hw_free_notifier(struct notifier_block *nb)
{
	int status;

	status = raw_notifier_chain_register(&ultra_afe_hw_free_noitify_chain, nb);
	return status;
}
EXPORT_SYMBOL_GPL(register_ultra_afe_hw_free_notifier);

int unregister_ultra_afe_hw_free_notifier(struct notifier_block *nb)
{
	int status;

	status = raw_notifier_chain_unregister(&ultra_afe_hw_free_noitify_chain, nb);
	return status;
}
EXPORT_SYMBOL_GPL(unregister_ultra_afe_hw_free_notifier);

int notify_ultra_afe_hw_free(unsigned long module, void *v)
{
	return raw_notifier_call_chain(&ultra_afe_hw_free_noitify_chain, module, v);
}
EXPORT_SYMBOL_GPL(notify_ultra_afe_hw_free);

void *audio_get_power_scenario(void)
{
	if (afe_get_power_scenario)
		return afe_get_power_scenario();
	return NULL;
}
EXPORT_SYMBOL_GPL(audio_get_power_scenario);

void register_get_power_scene_callback(void* (*callback)(void))
{
	afe_get_power_scenario = callback;
}
EXPORT_SYMBOL_GPL(register_get_power_scene_callback);

static struct scp_system_callback_op scp_sys_op = {0};
int scp_system_cb_init(struct scp_system_callback_op *sys_callback)
{
	if (sys_callback) {
		scp_sys_op.scp_awake_lock_cb = sys_callback->scp_awake_lock_cb;
		scp_sys_op.scp_awake_unlock_cb = sys_callback->scp_awake_unlock_cb;
		scp_sys_op.scp_clr_spm_reg_cb = sys_callback->scp_clr_spm_reg_cb;
		scp_sys_op.is_scp_ready_cb = sys_callback->is_scp_ready_cb;
		scp_sys_op.scp_A_register_notify_cb = sys_callback->scp_A_register_notify_cb;
		return 0;
	} else
		return -EPERM;
}
EXPORT_SYMBOL_GPL(scp_system_cb_init);


int scp_awake_lock_wrap(void *scp_id)
{
	int ret;

	if (scp_sys_op.scp_awake_lock_cb != NULL) {
		ret = scp_sys_op.scp_awake_lock_cb(scp_id);
		return ret;
	} else
		return -EPERM;
}
EXPORT_SYMBOL_GPL(scp_awake_lock_wrap);

int scp_awake_unlock_wrap(void *scp_id)
{
	int ret;

	if (scp_sys_op.scp_awake_unlock_cb != NULL) {
		ret = scp_sys_op.scp_awake_unlock_cb(scp_id);
		return ret;
	} else
		return -EPERM;
}
EXPORT_SYMBOL_GPL(scp_awake_unlock_wrap);

int scp_clr_spm_reg_cb_wrap(void *__unused)
{
	int ret;

	if (scp_sys_op.scp_clr_spm_reg_cb != NULL) {
		ret = scp_sys_op.scp_clr_spm_reg_cb(NULL);
		return ret;
	} else
		return -EPERM;
}
EXPORT_SYMBOL_GPL(scp_clr_spm_reg_cb_wrap);

unsigned int is_scp_ready_wrap(int id)
{
	unsigned int ret;

	if (scp_sys_op.is_scp_ready_cb != NULL) {
		ret = scp_sys_op.is_scp_ready_cb(id);
		return ret;
	} else
		return 0;
}
EXPORT_SYMBOL_GPL(is_scp_ready_wrap);

void scp_A_register_notify_wrap(struct notifier_block *nb)
{
	if (scp_sys_op.scp_A_register_notify_cb != NULL)
		scp_sys_op.scp_A_register_notify_cb(nb);
}
EXPORT_SYMBOL_GPL(scp_A_register_notify_wrap);

struct scp_system_callback_op *get_scp_system_op(void)
{
	return &scp_sys_op;
}
EXPORT_SYMBOL_GPL(get_scp_system_op);

MODULE_SOFTDEP("post: mediatek-drm");

MODULE_DESCRIPTION("Mediatek afe external");
MODULE_AUTHOR("Shane Chien <shane.chien@mediatek.com>");
MODULE_LICENSE("GPL v2");
