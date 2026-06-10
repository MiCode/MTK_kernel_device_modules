// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>
#include <linux/arm-smccc.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <pkvm_mgmt/pkvm_mgmt.h>

#include "pkvm_seninf_host.h"
#include "pkvm_seninf_ioctl.h"

static int seninf_checkpipe_hvc;
static int seninf_free_hvc;

static long seninf_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	int ret;

	pr_info(PFX "%s: cmd = %d\n", __func__, cmd);

	switch(cmd) {
	case IOCTL_ID_PKVM_SENINF_IS_ENABLED:
		ret = 0;
		break;
	case IOCTL_ID_PKVM_SENINF_CHECKPIPE:
		pkvm_el2_mod_call(seninf_checkpipe_hvc);
		ret = 0;
		break;
	case IOCTL_ID_PKVM_SENINF_FREE:
		pkvm_el2_mod_call(seninf_free_hvc);
		ret = 0;
		break;
	default:
		pr_info(PFX "%s: no such ioctl cmd\n", __func__);
		ret = -1;
		break;
	}

	return ret;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = seninf_ioctl,
};

static struct miscdevice seninf_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = PKVM_SENINF_DEV_NAME,
	.fops = &fops
};

static int seninf_hvc_register(unsigned long token)
{
	seninf_checkpipe_hvc = pkvm_register_el2_mod_call(kvm_nvhe_sym(seninf_hyp_checkpipe), token);
	seninf_free_hvc = pkvm_register_el2_mod_call(kvm_nvhe_sym(seninf_hyp_free), token);

	return 0;
}

static int __init seninf_nvhe_init(void)
{
	unsigned long token;
	int ret;

	if (!is_protected_kvm_enabled()) {
		pr_info(PFX "%s: skip to init pkvm seninf module\n", __func__);
		return 0;
	}

	ret = pkvm_load_el2_module(kvm_nvhe_sym(seninf_hyp_init), &token);
	if (ret) {
		pr_info(PFX "%s: failed to load pkvm seninf module, ret %d\n", __func__, ret);
		return ret;
	}

	ret = seninf_hvc_register(token);
	if (ret) {
		pr_info(PFX "%s: failed to register seninf hvc, ret %d\n", __func__, ret);
		return ret;
	}

	ret = misc_register(&seninf_dev);
	if (ret) {
		pr_info(PFX "%s: failed to register seninf dev, ret %d\n", __func__, ret);
		return ret;
	}

	pr_info(PFX "%s: success to load pkvm seninf module\n", __func__);
	return 0;
}
module_init(seninf_nvhe_init);
MODULE_LICENSE("GPL");
