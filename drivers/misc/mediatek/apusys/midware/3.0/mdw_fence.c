// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mdw.h"
#include "mdw_cmn.h"
//--------------------------------------------
static uint64_t mdw_fence_ctx_alloc(struct mdw_device *mdev)
{
	uint64_t idx = 0, ctx = 0;

	mutex_lock(&mdev->f_mtx);
	idx = find_first_zero_bit(mdev->fence_ctx_mask, mdev->num_fence_ctx);
	if (idx >= mdev->num_fence_ctx) {
		ctx = dma_fence_context_alloc(1);
		mdw_drv_warn("no free fence ctx(%llu), alloc ctx(%llu)\n", idx, ctx);
	} else {
		set_bit(idx, mdev->fence_ctx_mask);
		ctx = mdev->base_fence_ctx + idx;
	}
	mutex_unlock(&mdev->f_mtx);
	mdw_cmd_debug("alloc fence ctx(%llu) idx(%llu) base(%llu)\n",
		ctx, idx, mdev->base_fence_ctx);

	return ctx;
}

static void mdw_fence_ctx_free(struct mdw_device *mdev, uint64_t ctx)
{
	int idx = 0;

	idx = ctx - mdev->base_fence_ctx;
	if (idx < 0 || idx >= mdev->num_fence_ctx) {
		mdw_cmd_debug("out of range ctx(%llu/%llu)\n", ctx, mdev->base_fence_ctx);
		return;
	}

	mutex_lock(&mdev->f_mtx);
	if (!test_bit(idx, mdev->fence_ctx_mask))
		mdw_drv_warn("ctx state conflict(%d)\n", idx);
	else
		clear_bit(idx, mdev->fence_ctx_mask);
	mutex_unlock(&mdev->f_mtx);
}

static const char *mdw_fence_get_driver_name(struct dma_fence *fence)
{
	return "apu_mdw";
}

static const char *mdw_fence_get_timeline_name(struct dma_fence *fence)
{
	struct mdw_fence *f =
		container_of(fence, struct mdw_fence, base_fence);

	return f->name;
}

static bool mdw_fence_enable_signaling(struct dma_fence *fence)
{
	return true;
}

static void mdw_fence_release(struct dma_fence *fence)
{
	struct mdw_fence *mf =
		container_of(fence, struct mdw_fence, base_fence);

	mdw_flw_debug("fence release, fence(%s/%llu-%llu)\n",
		mf->name, mf->base_fence.context, mf->base_fence.seqno);
	mdw_fence_ctx_free(mf->mdev, mf->base_fence.context);
	kfree(mf);
}

const struct dma_fence_ops mdw_fence_ops = {
	.get_driver_name =  mdw_fence_get_driver_name,
	.get_timeline_name =  mdw_fence_get_timeline_name,
	.enable_signaling =  mdw_fence_enable_signaling,
	.wait = dma_fence_default_wait,
	.release =  mdw_fence_release,
};

//--------------------------------------------
int mdw_fence_init(struct mdw_cmd *c)
{
	int ret = 0;
	struct mdw_device *mdev = c->mpriv->mdev;

	c->fence = kzalloc(sizeof(*c->fence), GFP_KERNEL);
	if (!c->fence)
		return -ENOMEM;

	if (snprintf(c->fence->name, sizeof(c->fence->name), "0x%llx:%s", c->inference_id, c->comm) <= 0)
		mdw_drv_warn("set fance name fail\n");
	c->fence->mdev = c->mpriv->mdev;
	spin_lock_init(&c->fence->lock);
	dma_fence_init(&c->fence->base_fence, &mdw_fence_ops,
		&c->fence->lock, mdw_fence_ctx_alloc(mdev),
		atomic_add_return(1, &c->mpriv->exec_seqno));

	mdw_flw_debug("fence init, c(0x%llx) fence(%s/%llu-%llu)\n",
		(uint64_t)c, c->fence->name, c->fence->base_fence.context,
		c->fence->base_fence.seqno);

	return ret;
}

void mdw_fence_delete(struct mdw_cmd *c)
{
	struct dma_fence *f = &c->fence->base_fence;

	dma_fence_put(f);
}