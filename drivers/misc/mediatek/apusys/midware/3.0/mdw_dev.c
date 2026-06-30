// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/of.h>
#include <linux/of_device.h>

#include "mdw_cmn.h"
#include "mdw_cmd.h"
#include "mdw_cb_appendix.h"

struct mdw_device *g_mdev;

static void mdw_dev_put_cmd_func(struct work_struct *wk)
{
	struct mdw_device *mdev =
		container_of(wk, struct mdw_device, c_wk);
	struct mdw_cmd *c = NULL, *tmp = NULL;

	mutex_lock(&mdev->c_mtx);
	list_for_each_entry_safe(c, tmp, &mdev->d_cmds, d_node) {
		list_del(&c->d_node);
		c->put_ref(c);
	}
	mutex_unlock(&mdev->c_mtx);
}

int mdw_dev_init(struct device *dev, struct mdw_device *mdev)
{
	int ret = 0;

	mdw_drv_info("mdw dev init type(%d-%u)\n",
		mdev->driver_type, mdev->mdw_ver);

	mdev->allocator = apu_sysmem_create_allocator(0xffffffff);
	if (!mdev->allocator) {
		mdw_drv_err("create allocator failed\n");
		ret = -ENOMEM;
	}

	/* mem related */
	mutex_init(&mdev->mctl_mtx);
	INIT_LIST_HEAD(&mdev->maps);
	hash_init(mdev->u_mem_hash);
	/* cmd realted */
	INIT_LIST_HEAD(&mdev->d_cmds);
	mutex_init(&mdev->c_mtx);
	INIT_WORK(&mdev->c_wk, &mdw_dev_put_cmd_func);
	mdev->base_fence_ctx = dma_fence_context_alloc(MDW_FENCE_MAX_RINGS);
	mdev->num_fence_ctx = MDW_FENCE_MAX_RINGS;
	mutex_init(&mdev->f_mtx);
	mutex_init(&mdev->dtime_mtx);
	mutex_init(&mdev->power_mtx);
	mdw_cb_appendix_init();

	mdev->plat_funcs = (struct mdw_plat_func *)of_device_get_match_data(dev);
	if (!mdev->plat_funcs) {
		mdw_drv_err("of_device_get_match_data fail\n");
		return -EINVAL;
	}

	if (mdev->plat_funcs) {
		ret = mdev->plat_funcs->late_init(mdev);
		if (ret) {
			mdw_drv_err("late init failed(%d)\n", ret);
			goto free_allocator;
		}
	}

	g_mdev = mdev;

	goto out;

free_allocator:
	apu_sysmem_delete_allocator(mdev->allocator);
	mdev->allocator = NULL;
out:
	return ret;
}

void mdw_dev_deinit(struct mdw_device *mdev)
{
	if (mdev->plat_funcs) {
		mdev->plat_funcs->late_deinit(mdev);
		mdev->plat_funcs = NULL;
	}

	mdw_cb_appendix_deinit();

	if (mdev->allocator)
		apu_sysmem_delete_allocator(mdev->allocator);

	g_mdev = NULL;
}

void mdw_dev_session_create(struct mdw_fpriv *mpriv)
{
	struct mdw_device *mdev = mpriv->mdev;
	uint32_t i = 0;

	for (i = 0; i < MDW_DEV_MAX; i++) {
		if (mdev->adevs[i])
			mdev->adevs[i]->send_cmd(APUSYS_CMD_SESSION_CREATE, mpriv, mdev->adevs[i]);
	}
}

void mdw_dev_session_delete(struct mdw_fpriv *mpriv)
{
	struct mdw_device *mdev = mpriv->mdev;
	uint32_t i = 0;

	for (i = 0; i < MDW_DEV_MAX; i++) {
		if (mdev->adevs[i])
			mdev->adevs[i]->send_cmd(APUSYS_CMD_SESSION_DELETE, mpriv, mdev->adevs[i]);
	}
}

int mdw_dev_validation(struct mdw_fpriv *mpriv, uint32_t dtype,
	struct mdw_cmd *cmd, struct apusys_cmdbuf *cbs, uint32_t num)
{
	struct apusys_device *adev = NULL;
	struct apusys_cmd_valid_handle v_hnd;
	int ret = 0;

	if (dtype >= MDW_DEV_MAX)
		return -ENODEV;

	memset(&v_hnd, 0, sizeof(v_hnd));
	v_hnd.cmdbufs = cbs;
	v_hnd.num_cmdbufs = num;
	v_hnd.session = mpriv;
	v_hnd.cmd = cmd;
	adev = mpriv->mdev->adevs[dtype];
	if (adev) {
		mutex_lock(&mpriv->mdev->mctl_mtx);
		ret = adev->send_cmd(APUSYS_CMD_VALIDATE, &v_hnd, adev);
		mutex_unlock(&mpriv->mdev->mctl_mtx);
	}

	return ret;
}

int apusys_register_device(struct apusys_device *adev)
{
	uint32_t type = 0;

	if (!mdw_dev) {
		pr_info("[apusys] apu mdw not ready\n");
		return -ENODEV;
	}

	if (!mdw_dev->plat_funcs) {
		pr_info("[apusys] apu mdw not inited\n");
		return -ENODEV;
	}

	type = adev->dev_type;
	if (type >= MDW_DEV_MAX) {
		pr_info("[apusys] invalid dev(%u)\n", type);
		return -EINVAL;
	}

	if (!mdw_dev->adevs[type])
		mdw_dev->adevs[type] = adev;

	return mdw_dev->plat_funcs->register_device(adev);
}

int apusys_unregister_device(struct apusys_device *adev)
{
	if (!mdw_dev) {
		pr_info("[apusys] apu mdw not ready\n");
		return -ENODEV;
	}

	if (!mdw_dev->plat_funcs) {
		pr_info("[apusys] apu mdw not inited\n");
		return -ENODEV;
	}

	return mdw_dev->plat_funcs->unregister_device(adev);
}
