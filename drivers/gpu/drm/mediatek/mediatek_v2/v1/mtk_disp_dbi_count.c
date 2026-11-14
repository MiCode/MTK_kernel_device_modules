// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/ratelimit.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/dma-buf.h>
#include <linux/dma-direction.h>
#include <linux/dma-heap.h>


#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_drv.h"
#include "mtk_fence.h"
#include "mtk_sync.h"



#include "mtk_disp_dbi_count.h"

#define DBI_SPIN_LOCK(lock, name, line, flag)                        \
	do {                                                         \
		DDPINFO("DBI_SPIN_LOCK:%s[%d] +\n", name, line);     \
		spin_lock_irqsave(lock, flag);                       \
	} while (0)

#define DBI_SPIN_UNLOCK(lock, name, line, flag)                                \
	do {                                                                   \
		DDPINFO("DBI_SPIN_UNLOCK:%s[%d] +\n", name, line);             \
		spin_unlock_irqrestore(lock, flag);                            \
	} while (0)

#define log_en (1)

#define DBI_COUNT_INFO(fmt, arg...) do { \
			if (log_en) \
				DDPINFO("[DBI_COUNT]%s:" fmt, __func__, ##arg); \
		} while (0)

#define DBI_COUNT_MSG(fmt, arg...) do { \
			if (log_en) \
				DDPMSG("[DBI_COUNT]%s:" fmt, __func__, ##arg); \
		} while (0)

struct dbi_count_block_info {
	uint32_t block_h;
	uint32_t block_v;
	uint32_t channel;
};

static int dmabuf_to_iova(struct drm_device *dev, struct mtk_dbi_dma_buf *dma)
{
	int err;
	struct mtk_drm_private *priv = dev->dev_private;

	dma->attach = dma_buf_attach(dma->dmabuf, priv->dma_dev);
	if (IS_ERR_OR_NULL(dma->attach)) {
		err = PTR_ERR(dma->attach);
		DDPMSG("%s attach fail buf %p dev %p err %d",
			__func__, dma->dmabuf, priv->dma_dev, err);
		goto err;
	}

	dma->attach->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;
	dma->sgt = dma_buf_map_attachment_unlocked(dma->attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(dma->sgt)) {
		err = PTR_ERR(dma->sgt);
		DDPMSG("%s map failed err %d attach %p dev %p",
			__func__, err, dma->attach, priv->dma_dev);
		goto err_detach;
	}

	dma->iova = sg_dma_address(dma->sgt->sgl);
	if (!dma->iova) {
		DDPMSG("%s iova map fail dev %p", __func__, priv->dma_dev);
		err = -ENOMEM;
		goto err_detach;
	}

	return 0;

err_detach:
	dma_buf_detach(dma->dmabuf, dma->attach);
	dma->sgt = NULL;
err:
	dma->attach = NULL;
	return err;
}

void mtk_crtc_release_dbi_count_fence_by_idx(
	struct drm_crtc *crtc, int session_id, unsigned int fence_idx)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;

	if (fence_idx && fence_idx != -1) {
		DDPINFO("output fence_idx:%d\n", fence_idx);
		mtk_release_fence(session_id,
			mtk_fence_get_dbi_count_timeline_id(), fence_idx);
	}
}

void mtk_crtc_dbi_count_cfg(struct mtk_drm_crtc *mtk_crtc, struct mtk_crtc_state *crtc_state)
{
	unsigned int slice_num = crtc_state->prop_val[CRTC_PROP_DBI_COUNT_SLICE_NUM];
	unsigned int slice_size = crtc_state->prop_val[CRTC_PROP_DBI_COUNT_SLICE_SIZE];
	unsigned int block_h = crtc_state->prop_val[CRTC_PROP_DBI_COUNT_BLOCK_H];
	unsigned int block_v = crtc_state->prop_val[CRTC_PROP_DBI_COUNT_BLOCK_V];
	unsigned int enable = crtc_state->prop_val[CRTC_PROP_DBI_COUNT_ENABLE];
	unsigned int fence = crtc_state->prop_val[CRTC_PROP_DBI_COUNT_FENCE_IDX];
	unsigned int out = crtc_state->prop_val[CRTC_PROP_OUTPUT_ENABLE];
	int session_id = 0;
	struct mtk_dbi_timer *dbi_timer;
	int sec;
	unsigned int flags;

	if (!mtk_crtc->dbi_data.support)
		return;

	session_id = mtk_get_session_id(&mtk_crtc->base);
	if (enable) {
		if (atomic_read(&mtk_crtc->dbi_data.disable_finish) == 1) {
			/* enable again*/
			if (mtk_crtc->dbi_data.slice_idx >= mtk_crtc->dbi_data.slice_num) {
				mtk_crtc->dbi_data.slice_idx = 0;
				mtk_crtc->dbi_data.slice_num = slice_num;
				mtk_crtc->dbi_data.slice_size = slice_size;
			}

			if (mtk_crtc->dbi_data.fence_unreleased) {
				mtk_crtc_release_dbi_count_fence_by_idx(&mtk_crtc->base,
					session_id, mtk_crtc->dbi_data.fence_idx);
				mtk_crtc->dbi_data.fence_unreleased = 0;
			}

			mtk_crtc->dbi_data.fence_idx = fence;
			mtk_crtc->dbi_data.fence_unreleased = 1;
			atomic_set(&mtk_crtc->dbi_data.disable_finish, 0);
		}

		if (mtk_crtc->dbi_data.slice_idx >= mtk_crtc->dbi_data.slice_num){
			if(mtk_crtc->dbi_data.fence_unreleased) {
				cmdq_pkt_write(crtc_state->cmdq_handle, mtk_crtc->gce_obj.base,
					mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_RELEASE_DBI_COUNT_FENCE),
					1, ~0);
				cmdq_pkt_write(crtc_state->cmdq_handle, mtk_crtc->gce_obj.base,
					mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_CURRENT_DBI_COUNT_FENCE),
					mtk_crtc->dbi_data.fence_idx, ~0);
				mtk_crtc->dbi_data.fence_unreleased = 0;
			}
		} else
			mtk_crtc->dbi_data.slice_idx++;
		DBI_COUNT_INFO("%d %d\n", mtk_crtc->dbi_data.slice_idx, mtk_crtc->dbi_data.slice_num);
	} else {
		if (mtk_crtc->dbi_data.fence_unreleased) {
			mtk_crtc_release_dbi_count_fence_by_idx(&mtk_crtc->base,
				session_id, mtk_crtc->dbi_data.fence_idx);
			mtk_crtc->dbi_data.fence_unreleased = 0;
		}
		atomic_set(&mtk_crtc->dbi_data.disable_finish, 1);
	}

	dbi_timer = &mtk_crtc->dbi_data.dbi_timer;
	DBI_SPIN_LOCK(&dbi_timer->lock, __func__, __LINE__, flags);
	if (dbi_timer->active) {
		sec = dbi_timer->sec;
		mod_timer(&dbi_timer->base, jiffies + msecs_to_jiffies(sec*1000));
	}
	DBI_SPIN_UNLOCK(&dbi_timer->lock, __func__, __LINE__, flags);

	atomic_set(&mtk_crtc->dbi_data.new_frame_arrival, 1);
	wake_up_all(&mtk_crtc->dbi_data.new_frame_wq);

}

void mtk_crtc_dbi_count_release_fence(struct mtk_drm_crtc *mtk_crtc)
{
	unsigned int release;
	unsigned int fence;
	struct drm_crtc *crtc = &mtk_crtc->base;
	int session_id = 0;
	struct mtk_dbi_event *dbi_event;
	unsigned int flags;

	session_id = mtk_get_session_id(crtc);

	if(!mtk_crtc->dbi_data.support)
		return;

	dbi_event = &mtk_crtc->dbi_data.dbi_event;

	release = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_RELEASE_DBI_COUNT_FENCE);
	fence = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_CURRENT_DBI_COUNT_FENCE);
	if (release) {
		DBI_COUNT_INFO("release : %d, fence:%d", release, fence);
		mtk_crtc_release_dbi_count_fence_by_idx(crtc, session_id, fence);
		*(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_RELEASE_DBI_COUNT_FENCE) = 0;
		*(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_CURRENT_DBI_COUNT_FENCE) = 0;

		DBI_SPIN_LOCK(&dbi_event->lock, __func__, __LINE__, flags);
		dbi_event->event |= (1<<DBI_COUNT_DONE);
		wake_up_all(&dbi_event->event_wq);
		DBI_SPIN_UNLOCK(&dbi_event->lock, __func__, __LINE__, flags);
	}
}

void mtk_crtc_dbi_count_init(struct mtk_drm_crtc *mtk_crtc)
{
	spin_lock_init(&mtk_crtc->dbi_data.dbi_timer.lock);

	atomic_set(&mtk_crtc->dbi_data.new_frame_arrival, 1);
	mtk_crtc->dbi_data.support = 1;
	mtk_crtc->dbi_data.dbi_timer.mtk_crtc = mtk_crtc;
	init_waitqueue_head(&mtk_crtc->dbi_data.dbi_event.event_wq);
	spin_lock_init(&mtk_crtc->dbi_data.dbi_event.lock);

	init_waitqueue_head(&mtk_crtc->dbi_data.new_frame_wq);
	init_waitqueue_head(&mtk_crtc->dbi_data.disable_finish_wq);

}

int mtk_dbi_count_wait_event(struct drm_crtc *crtc, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int ret = 0;
	unsigned int *event = data;
	struct mtk_dbi_event *dbi_event = &mtk_crtc->dbi_data.dbi_event;
	unsigned int flags_event;

	wait_event_interruptible_timeout(dbi_event->event_wq,
		dbi_event->event, msecs_to_jiffies(10000));

	DBI_SPIN_LOCK(&dbi_event->lock, __func__, __LINE__, flags_event);
	if (dbi_event->event) {
		*event = dbi_event->event;
		dbi_event->event = 0;
		DBI_SPIN_UNLOCK(&dbi_event->lock, __func__, __LINE__, flags_event);
		return ret;
	}
	DBI_SPIN_UNLOCK(&dbi_event->lock, __func__, __LINE__, flags_event);

	return ret;
}

int mtk_dbi_count_clear_event(struct drm_crtc *crtc, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int ret = 0;
	struct mtk_dbi_event *dbi_event = &mtk_crtc->dbi_data.dbi_event;
	unsigned int flags_event;

	DBI_SPIN_LOCK(&dbi_event->lock, __func__, __LINE__, flags_event);
	dbi_event->event = 0;
	DBI_SPIN_UNLOCK(&dbi_event->lock, __func__, __LINE__, flags_event);

	return ret;
}


int mtk_dbi_count_wait_new_frame(struct drm_crtc *crtc, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int ret = 0;
	unsigned int *event = data;

	wait_event_interruptible_timeout(mtk_crtc->dbi_data.new_frame_wq,
		atomic_read(&mtk_crtc->dbi_data.new_frame_arrival), msecs_to_jiffies(10000));
	*event = atomic_read(&mtk_crtc->dbi_data.new_frame_arrival);

	return 0;
}



void mtk_dbi_count_timer_callback( struct timer_list *timer)
{
	struct mtk_dbi_timer *dbi_timer;
	struct mtk_drm_crtc *mtk_crtc;
	unsigned int flags;
	struct mtk_dbi_event *dbi_event;
	unsigned int flags_event;

	dbi_timer = to_dbi_timer(timer);
	mtk_crtc = dbi_timer->mtk_crtc;
	dbi_event = &mtk_crtc->dbi_data.dbi_event;

	DBI_COUNT_INFO("%s +++\n",__func__);

	DBI_SPIN_LOCK(&dbi_timer->lock, __func__, __LINE__, flags);
	dbi_timer->active = 0;
	DBI_SPIN_UNLOCK(&dbi_timer->lock, __func__, __LINE__, flags);

	DBI_SPIN_LOCK(&dbi_event->lock, __func__, __LINE__, flags_event);
	dbi_event->event |= (1<<DBI_COUNT_IDLE_TIMER_TRIGGER);
	wake_up_all(&mtk_crtc->dbi_data.dbi_event.event_wq);
	DBI_SPIN_UNLOCK(&dbi_event->lock, __func__, __LINE__, flags_event);

}

int mtk_dbi_count_create_timer(struct drm_crtc *crtc, void *data, bool need_lock, bool update_sec)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int ret = 0;
	unsigned int sec;
	struct timer_list *timer = &mtk_crtc->dbi_data.dbi_timer.base;
	struct mtk_dbi_timer *dbi_timer =  &mtk_crtc->dbi_data.dbi_timer;
	unsigned int flags;
	struct mtk_dbi_event *dbi_event = &mtk_crtc->dbi_data.dbi_event;
	unsigned int flags_event;

	DBI_COUNT_INFO("+++\n");

	if (need_lock)
		DBI_SPIN_LOCK(&dbi_timer->lock, __func__, __LINE__, flags);
	if (dbi_timer->enable)
		del_timer(timer);
	if (update_sec) {
		sec = *(unsigned int *)data;
		dbi_timer->sec = sec;
	}
	dbi_timer->active = 1;
	dbi_timer->enable = 1;
	dbi_timer->suspend = 0;

	DBI_SPIN_LOCK(&dbi_event->lock, __func__, __LINE__, flags_event);
	dbi_event->event &= ~(1<<DBI_COUNT_IDLE_TIMER_TRIGGER);
	DBI_SPIN_UNLOCK(&dbi_event->lock, __func__, __LINE__, flags_event);

	timer_setup(timer, mtk_dbi_count_timer_callback, 0);
	mod_timer(timer, jiffies + msecs_to_jiffies((mtk_crtc->dbi_data.dbi_timer.sec)*1000));
	if (need_lock)
		DBI_SPIN_UNLOCK(&dbi_timer->lock, __func__, __LINE__, flags);
	return ret;
}

int mtk_dbi_count_delete_timer(struct drm_crtc *crtc, bool need_lock, bool mark_suspend)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int ret = 0;
	struct timer_list *timer = &mtk_crtc->dbi_data.dbi_timer.base;
	struct mtk_dbi_timer *dbi_timer =  &mtk_crtc->dbi_data.dbi_timer;
	unsigned int flags;

	DBI_COUNT_INFO(" +++\n");

	if (need_lock)
		DBI_SPIN_LOCK(&dbi_timer->lock, __func__, __LINE__, flags);
	if (mtk_crtc->dbi_data.dbi_timer.enable) {
		del_timer(timer);
		dbi_timer->active =  0;
		dbi_timer->enable = 0;
		if(mark_suspend)
			dbi_timer->suspend = 1;
	}
	if (!mark_suspend)
		dbi_timer->suspend = 0;
	if (need_lock)
		DBI_SPIN_UNLOCK(&dbi_timer->lock, __func__, __LINE__, flags);

	return ret;
}

int mtk_dbi_count_timer_disable(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int ret = 0;
	struct mtk_dbi_timer *dbi_timer = &mtk_crtc->dbi_data.dbi_timer;
	unsigned int flags;
	unsigned int crtc_id = drm_crtc_index(crtc);
	struct mtk_crtc_state *mtk_state = to_mtk_crtc_state(crtc->state);

	if (mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE])
		return ret;

	if (crtc_id)
		return ret;

	DBI_COUNT_INFO("+++\n");

	DBI_SPIN_LOCK(&dbi_timer->lock, __func__, __LINE__, flags);
	mtk_dbi_count_delete_timer(crtc, false, true);
	DBI_SPIN_UNLOCK(&dbi_timer->lock, __func__, __LINE__, flags);

	return ret;
}


int mtk_dbi_count_timer_enable(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int ret = 0;
	struct mtk_dbi_timer *dbi_timer = &mtk_crtc->dbi_data.dbi_timer;
	unsigned int flags;
	struct mtk_crtc_state *mtk_state = to_mtk_crtc_state(crtc->state);
	unsigned int crtc_id = drm_crtc_index(crtc);

	if (mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE])
		return ret;

	if (crtc_id)
		return ret;

	DBI_COUNT_INFO("+++\n");

	DBI_SPIN_LOCK(&dbi_timer->lock, __func__, __LINE__, flags);
	if (mtk_crtc->dbi_data.dbi_timer.suspend)
		mtk_dbi_count_create_timer(crtc, NULL, false, false);
	DBI_SPIN_UNLOCK(&dbi_timer->lock, __func__, __LINE__, flags);

	return ret;
}



int mtk_drm_crtc_get_count_fence_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_crtc *crtc;
	struct drm_mtk_fence *args = data;
	struct mtk_drm_private *private;
	struct fence_data fence;
	unsigned int fence_idx;
	struct mtk_fence_info *l_info = NULL;
	int tl, idx;

	crtc = drm_crtc_find(dev, file_priv, args->crtc_id);
	if (!crtc) {
		DDPMSG("Unknown CRTC ID %d\n", args->crtc_id);
		ret = -ENOENT;
		return ret;
	}

	idx = drm_crtc_index(crtc);
	if (!crtc->dev) {
		DDPMSG("%s:%d dev is null\n", __func__, __LINE__);
		ret = -EFAULT;
		return ret;
	}
	if (!crtc->dev->dev_private) {
		DDPMSG("%s:%d dev private is null\n", __func__, __LINE__);
		ret = -EFAULT;
		return ret;
	}
	private = crtc->dev->dev_private;
	fence_idx = atomic_read(&private->crtc_dbi_count[idx]);
	tl = mtk_fence_get_dbi_count_timeline_id();
	l_info = mtk_fence_get_layer_info(mtk_get_session_id(crtc), tl);
	if (!l_info) {
		DDPMSG("%s:%d layer_info is null\n", __func__, __LINE__);
		ret = -EFAULT;
		return ret;
	}

	/* create fence */
	fence.fence = MTK_INVALID_FENCE_FD;
	fence.value = ++fence_idx;
	atomic_inc(&private->crtc_dbi_count[idx]);
	ret = mtk_sync_fence_create(l_info->timeline, &fence);
	if (ret) {
		DDPMSG("%d,L%d create Fence Object failed!\n",
			  MTK_SESSION_DEV(mtk_get_session_id(crtc)), tl);
		ret = -EFAULT;
	}

	args->fence_fd = fence.fence;
	args->fence_idx = fence.value;

	return ret;
}

struct dbi_count_block_info mtk_dbi_count_get_block_info(uint32_t block_h, uint32_t block_v)
{
	struct dbi_count_block_info  ret = { 0 };

	if((block_h == 2) && (block_v == 1)) {
		ret.block_h = 2;
		ret.block_v = 1;
		ret.channel = 4;
		return ret;
	} else if((block_h == 2) && (block_v == 1)){
		ret.block_h = 2;
		ret.block_v = 1;
		ret.channel = 3;
		return ret;
	} else if((block_h == 2) && (block_v == 1)){
		ret.block_h = 2;
		ret.block_v = 2;
		ret.channel = 3;
		return ret;
	}
	return ret;
}

void mtk_dbi_count_hrt_cal(uint32_t en, uint32_t slice_size, uint32_t slice_num,
	uint32_t block_h, uint32_t block_v, int *oddmr_hrt)
{
	uint32_t panel_width = 1440;
	uint32_t panel_height = 3200;
	struct dbi_count_block_info block;
	uint32_t slice_width;
	uint32_t slice_height;
	uint32_t hrt;
	uint32_t layer_size;

	block = mtk_dbi_count_get_block_info(block_h, block_v);
	slice_width = panel_width / block.block_h * block.channel * 2;
	slice_height = (panel_height/block.block_v + slice_num - 1)/slice_num;
	hrt = slice_width * slice_height * 2;
	layer_size = panel_width * panel_height * 4;
	hrt = (400 * hrt)/layer_size;

	if (en)
		*oddmr_hrt += hrt;
	DBI_COUNT_INFO("en:%d, total:%d, hrt:%d\n", en, *oddmr_hrt, hrt);
}

int mtk_dbi_count_wait_disable_finish(struct drm_crtc *crtc, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned int *event = data;

	DBI_COUNT_INFO("+++\n");

	wait_event_interruptible_timeout(mtk_crtc->dbi_data.disable_finish_wq,
		atomic_read(&mtk_crtc->dbi_data.disable_finish), msecs_to_jiffies(10000));
	*event = atomic_read(&mtk_crtc->dbi_data.disable_finish);

	return 0;
}

int mtk_dbi_count_check_buffer(struct drm_crtc *crtc, void *data)
{
	unsigned int *event = data;

	DBI_COUNT_INFO("+++\n");
	* event = 1;

	return 0;
}

int mtk_dbi_count_load_buffer(struct drm_crtc *crtc,void *data)
{
	int fd = *(int *)data;
	struct mtk_dbi_dma_buf buf;
	int ret;

	DBI_COUNT_INFO("+++\n");
	buf.dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(buf.dmabuf)) {
		DDPMSG("%s: fail to get dma_buf by fd : %d\n", __func__, fd);
		return -1;
	}

	ret = dmabuf_to_iova(crtc->dev, &buf);
	if (ret < 0) {
		DDPMSG("%s: fail to dmabuf_to_iova : %d\n", __func__, ret);
		return -1;
	}
	return 0;
}




