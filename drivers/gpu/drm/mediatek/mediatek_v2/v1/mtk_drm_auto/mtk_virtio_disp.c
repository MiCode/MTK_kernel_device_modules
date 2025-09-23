// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <drm/drm_crtc.h>
#include <drm/drm_vblank.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_plane.h>
#include <linux/videodev2.h>

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_HOST)
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_log.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_plane.h"
#include "mtk_drm_gem.h"

#include "mtk_drm_ddp_comp_auto.h"
#include "mtk_drm_crtc_auto.h"
#include "mtk_drm_mmp.h"
#include "mtk_virtio_disp.h"

#if IS_ENABLED(CONFIG_VHOST_CMDQ_DMA_MAP)
struct drm_vbuf {
	int id;
	struct sg_table *sgt;
	struct list_head list;
};

static LIST_HEAD(vbuf_list);
static DEFINE_IDR(vbuf_idr);
static DEFINE_MUTEX(vbuf_lock);
#endif

static struct cmdq_client *g_cmdq_client[CLIENT_TYPE_MAX * 10];
static unsigned int cmdq_client_num;

struct virtio_disp_rsp_crtc_path_info mt6991_mtk_an_crtc_path_info;

static bool mtk_drm_crtc_layer_off(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle)
{
	unsigned int i, j;
	struct mtk_ddp_comp *comp;
	bool is_share_dev = false;

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		if (mtk_ddp_comp_is_rdma(comp) &&
			(mtk_ddp_comp_is_share_comp(comp) ||
			mtk_ddp_comp_is_virt(comp))) {
			DDPMSG("%s crtc%d layer off %s\n",
				__func__, drm_crtc_index(&mtk_crtc->base),
				mtk_dump_comp_str(comp));
			is_share_dev = true;
			mtk_ddp_comp_stop(comp, handle);
		}
	}

	return is_share_dev;
}

void mtk_drm_crtc_vhost_layer_off(void)
{
	u32 i;
	struct mtk_ddp_comp *comp;
	struct mtk_drm_private *priv;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct cmdq_pkt *cmdq_handle = NULL;
	bool is_share_dev = false;
	struct drm_device *drm_dev = mtk_drm_get_dev();

	if (IS_ERR_OR_NULL(drm_dev)) {
		DDPMSG("%s invalid drm dev\n", __func__);
		return;
	}

	priv = drm_dev->dev_private;
	if (IS_ERR_OR_NULL(priv)) {
		DDPMSG("%s, invalid priv\n", __func__);
		return;
	}

	DDPMSG("%s+\n", __func__);

	for (i = 0; i < MAX_CRTC; i++) {
		if (!priv->crtc[i])
			continue;

		mtk_crtc = to_mtk_crtc(priv->crtc[i]);
		if (!mtk_crtc)
			continue;

		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
		if (!mtk_crtc->enabled) {
			DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
			continue;
		}
		mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
			mtk_crtc->gce_obj.client[CLIENT_CFG]);
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_FIRST_PATH, 0);
		is_share_dev = mtk_drm_crtc_layer_off(mtk_crtc, cmdq_handle);

		if (cmdq_handle) {
			if (is_share_dev)
				cmdq_pkt_flush(cmdq_handle);
			cmdq_pkt_destroy(cmdq_handle);
		}
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
	}

	DDPMSG("%s-\n", __func__);
}
EXPORT_SYMBOL(mtk_drm_crtc_vhost_layer_off);

bool mtk_drm_wake_state(uint32_t thread_id)
{
	struct mtk_drm_private *priv;
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_device *drm_dev = mtk_drm_get_dev();

	if (IS_ERR_OR_NULL(drm_dev)) {
		DDPMSG("%s invalid drm dev\n", __func__);
		return;
	}

	priv = drm_dev->dev_private;
	if (IS_ERR_OR_NULL(priv)) {
		DDPMSG("%s, invalid priv\n", __func__);
		return;
	}

	if (thread_id < MAX_CMDQ_THREAD) {
		mtk_crtc = priv->crtc_thread_map[thread_id];
		if (mtk_crtc)
			return mtk_crtc->enabled;
	}

	return true;
}
EXPORT_SYMBOL(mtk_drm_wake_state);

static int mtk_drm_crtc_enable_full(struct drm_device *dev, struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_atomic_state *state;
	struct drm_crtc_state *crtc_state;
	struct drm_crtc *crtc;
	struct drm_modeset_acquire_ctx ctx;
	struct drm_display_mode *mode;
	struct drm_connector *connector;
	struct drm_connector_list_iter conn_iter;
	struct drm_encoder *encoder;
	struct drm_connector_state *conn_state;
	int ret;
	bool found = false;

	if (mtk_crtc->enabled) {
		DDPMSG(" %s skip\n", __func__);
		return;
	}

	crtc = &mtk_crtc->base;

	drm_modeset_acquire_init(&ctx, 0);
	state = drm_atomic_state_alloc(crtc->dev);
	if (!state)
		return -ENOMEM;

	state->acquire_ctx = &ctx;

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		drm_connector_for_each_possible_encoder(connector, encoder) {
			if (encoder->possible_crtcs & drm_crtc_mask(crtc)) {
				found = true;
				break;
			}
		}
		if (found)
			break;
	}
	drm_connector_list_iter_end(&conn_iter);
	if (!found) {
		ret = -EINVAL;
		goto out;
	}

	mutex_lock(&dev->mode_config.mutex);
	connector->funcs->fill_modes(connector, dev->mode_config.max_width,
						dev->mode_config.max_height);
	mutex_unlock(&dev->mode_config.mutex);

	conn_state = drm_atomic_get_connector_state(state, connector);

	ret = drm_atomic_set_crtc_for_connector(conn_state, crtc);

	mode = list_first_entry(&connector->modes, typeof(*mode), head);
	drm_mode_debug_printmodeline(mode);
	drm_mode_set_crtcinfo(mode, 0);

	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	if (IS_ERR(crtc_state)) {
		ret = PTR_ERR(crtc_state);
		goto out;
	}
	crtc_state->active = true;
	crtc_state->mode_changed = true;

	ret = drm_atomic_set_mode_for_crtc(crtc_state, mode);
	if (ret)
		DDPMSG("%s L%d: crtc%d ret %d\n", __func__, __LINE__, drm_crtc_index(crtc), ret);

	ret = drm_atomic_commit(state);
 out:
	drm_atomic_state_put(state);
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);
	DDPMSG("%s crtc%d ret %d\n", __func__, drm_crtc_index(crtc), ret);
	return ret;
}

static int mtk_drm_crtc_disable_full(struct drm_device *dev, struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_crtc *crtc;
	struct drm_atomic_state *state;
	struct drm_crtc_state *crtc_state;
	struct drm_modeset_acquire_ctx ctx;
	struct drm_connector_state *conn_state;
	struct drm_connector *conn;
	struct drm_plane_state *plane_state;
	struct drm_plane *plane;
	int ret, i;

	if (!mtk_crtc->enabled)
		return 0;

	crtc = &mtk_crtc->base;

	drm_modeset_acquire_init(&ctx, 0);
	state = drm_atomic_state_alloc(crtc->dev);
	if (!state)
		return -ENOMEM;

	state->acquire_ctx = &ctx;

	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	if (IS_ERR(crtc_state)) {
		ret = PTR_ERR(crtc_state);
		goto out;
	}

	crtc_state->active = false;

	ret = drm_atomic_set_mode_prop_for_crtc(crtc_state, NULL);
	if (ret < 0)
		goto out;

	ret = drm_atomic_add_affected_planes(state, crtc);
	if (ret < 0)
		goto out;

	ret = drm_atomic_add_affected_connectors(state, crtc);
	if (ret < 0)
		goto out;

	for_each_new_connector_in_state(state, conn, conn_state, i) {
		ret = drm_atomic_set_crtc_for_connector(conn_state, NULL);
		if (ret < 0)
			goto out;
	}

	for_each_new_plane_in_state(state, plane, plane_state, i) {
		ret = drm_atomic_set_crtc_for_plane(plane_state, NULL);
		if (ret < 0)
			goto out;

		drm_atomic_set_fb_for_plane(plane_state, NULL);
	}

	ret = drm_atomic_commit(state);
 out:
	drm_atomic_state_put(state);
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);

	DDPMSG("%s crtc%d ret %d\n", __func__, drm_crtc_index(crtc), ret);
	return ret;
}

int mtk_drm_get_crtc_index(struct virtio_disp_req_crtc *req_crtc,
			   struct virtio_disp_rsp_crtc *rsp_crtc)
{
	struct mtk_drm_crtc *mtk_crtc;
	int ret;

	struct mtk_drm_private *priv;
	struct drm_crtc *crtc;
	struct cmdq_pkt *cmdq_handle = NULL;
	struct drm_device *drm_dev = mtk_drm_get_dev();

	unsigned int comp_id;

	if (IS_ERR_OR_NULL(drm_dev)) {
		DDPMSG("%s invalid drm dev\n", __func__);
		return;
	}

	comp_id = req_crtc->output_comp_id;

	priv = drm_dev->dev_private;

	mtk_crtc = mtk_drm_get_crtc_by_output(comp_id);
	if (!mtk_crtc) {
		DDPMSG("%s invalid comp_id %d\n", __func__, comp_id);
		return -EINVAL;
	}

	rsp_crtc->crtc_id = drm_crtc_index(&mtk_crtc->base);
	rsp_crtc->crtc_obj_id = mtk_crtc->base.base.id;

	DDPMSG("%s crtc-%d %s obj id %d\n",
		__func__, rsp_crtc->crtc_id,
		mtk_dump_comp_str_id(comp_id),
		rsp_crtc->crtc_obj_id);

	return rsp_crtc->crtc_id;
}
EXPORT_SYMBOL(mtk_drm_get_crtc_index);

int mtk_drm_crtc_enable_full_by_output(unsigned int comp_id, bool enable)
{
	struct mtk_drm_crtc *mtk_crtc;
	int ret;

	struct mtk_drm_private *priv;
	struct drm_crtc *crtc;
	struct cmdq_pkt *cmdq_handle = NULL;
	struct drm_device *drm_dev = mtk_drm_get_dev();

	if (IS_ERR_OR_NULL(drm_dev)) {
		DDPMSG("%s invalid drm dev\n", __func__);
		return;
	}

	priv = drm_dev->dev_private;

	mtk_crtc = mtk_drm_get_crtc_by_output(comp_id);
	if (!mtk_crtc) {
		DDPMSG("%s invalid comp_id %d\n", __func__, comp_id);
		return -EINVAL;
	}

	if (enable)
		ret = mtk_drm_crtc_enable_full(drm_dev, mtk_crtc);
	else
		ret = mtk_drm_crtc_disable_full(drm_dev, mtk_crtc);
	if (ret < 0)
		return ret;

	return drm_crtc_index(&mtk_crtc->base);
}
EXPORT_SYMBOL(mtk_drm_crtc_enable_full_by_output);

int mtk_drm_get_panel_timing(unsigned int comp_id,
	struct virtio_disp_rsp_panel *panel,
	mtk_virt_hotplug_cb cb)
{
	struct drm_device *dev;
	struct drm_connector *connector;
	struct drm_connector_list_iter conn_iter;
	struct drm_encoder *encoder;
	struct drm_display_mode *mode;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_ddp_comp *comp;
	bool found = false;
	char *panel_name = NULL;
	struct drm_device *drm_dev = mtk_drm_get_dev();

	if (IS_ERR_OR_NULL(drm_dev)) {
		DDPMSG("%s invalid drm dev\n", __func__);
		return;
	}
	dev = drm_dev;

	mtk_crtc = mtk_drm_get_crtc_by_output(comp_id);
	if (!mtk_crtc) {
		DDPMSG("%s invalid comp_id %d\n", __func__, comp_id);
		return -EINVAL;
	}
	if (cb)
		mtk_crtc->cb = cb;
	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (!comp)
		return -EINVAL;

	crtc = &mtk_crtc->base;
	DDPMSG("%s crtc%d L:%d\n", __func__, drm_crtc_index(crtc), __LINE__);

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		drm_connector_for_each_possible_encoder(connector, encoder) {
			if (encoder->possible_crtcs & drm_crtc_mask(crtc)) {
				found = true;
				break;
			}
		}
		if (found)
			break;
	}
	drm_connector_list_iter_end(&conn_iter);
	if (!found) {
		DDPMSG("%s crtc%d L:%d no connector\n", __func__, drm_crtc_index(crtc), __LINE__);
		return -EINVAL;
	}
	mutex_lock(&dev->mode_config.mutex);
	connector->funcs->fill_modes(connector, dev->mode_config.max_width,
		dev->mode_config.max_height);
	mutex_unlock(&dev->mode_config.mutex);
	mode = list_first_entry(&connector->modes, typeof(*mode), head);
	if (!mode) {
		DDPMSG("%s crtc%d L:%d no mode\n", __func__, drm_crtc_index(crtc), __LINE__);
		return -EINVAL;
	}
	if (panel != NULL) {
		panel->width = mode->hdisplay;
		panel->height = mode->vdisplay;
		panel->vrefresh = drm_mode_vrefresh(mode);
		panel->hfp = mode->hsync_start - mode->hdisplay;
		panel->hsa = mode->hsync_end - mode->hsync_start;
		panel->hbp = mode->htotal - mode->hsync_end;
		panel->vfp = mode->vsync_start - mode->vdisplay;
		panel->vsa = mode->vsync_end - mode->vsync_start;
		panel->vbp = mode->vtotal - mode->vsync_end;

		if (mtk_ddp_comp_get_type(comp->id) == MTK_DSI) {
			struct mtk_panel_ext *panel_ext = NULL;

			mtk_ddp_comp_io_cmd(comp, NULL, REQ_PANEL_EXT, &panel_ext);
			if (!panel_ext || !panel_ext->params)
				return -EINVAL;
			panel->degree = panel_ext->params->lcm_degree;
			mtk_ddp_comp_io_cmd(comp, NULL, GET_PANEL_NAME,
				    &panel_name);
			strscpy_pad(panel->panel_name, panel_name,
				MAX_PANEL_NAME_LEN - 1);
		} else {
			mtk_ddp_comp_io_cmd(comp, NULL, GET_PANEL_NAME,
							panel->panel_name);
		}

		DDPMSG("%s crtc%d L:%d %ux%u@%u degree:%u panel_name:%s\n", __func__,
			drm_crtc_index(crtc), __LINE__,
			panel->width, panel->height,
			panel->vrefresh, panel->degree,
			panel->panel_name);
	}

	return 0;
}
EXPORT_SYMBOL(mtk_drm_get_panel_timing);

void mtk_drm_crtc_record_client(struct cmdq_client *client)
{
	g_cmdq_client[cmdq_client_num] = client;
	cmdq_client_num++;
	DDPMSG("%s cmdq_client_num:%d\n", __func__, cmdq_client_num);
}

#if IS_ENABLED(CONFIG_VHOST_CMDQ)
void mtk_drm_crtc_init_client(void)
{
	int i = 0;

	/* set client for hypervisor */
	for (i = 0; i < cmdq_client_num; i++) {
		if (g_cmdq_client[i] != NULL) {
			DDPMSG("set cmdq client: %d, cmdq_client_num %d\n",
				i, cmdq_client_num);
			vhost_cmdq_set_client((void *)g_cmdq_client[i], 0);
		}
	}
}
EXPORT_SYMBOL(mtk_drm_crtc_init_client);
#endif

int mtk_drm_path_get_an_crtc_path_data(struct virtio_disp_rsp_crtc_path_info *path_info)
{
	int i, j;
	enum mtk_ddp_comp_id *an_ovl_path = NULL, an_output_comp;
	struct virtio_disp_an_crtc_path_data *an_crtc_path_data;
	int an_ovl_path_len, an_dual_ovl_path_len;

	if (path_info == NULL) {
		DDPMSG("[E]%s invalid input param!!!\n", __func__);
		return -EINVAL;

	}

	memcpy((void *)path_info, (void *)&mt6991_mtk_an_crtc_path_info,
		sizeof(struct virtio_disp_rsp_crtc_path_info));

	for (i = 0; i < MAX_VIRT_CRTC; i++) {
		an_crtc_path_data = &path_info->crtc_path_data[i];
		an_ovl_path = an_crtc_path_data->ovl_path;
		an_ovl_path_len = an_crtc_path_data->ovl_path_len;
		an_output_comp = an_crtc_path_data->output_comp;
		an_dual_ovl_path_len = an_crtc_path_data->dual_ovl_path_len;

		if (!an_ovl_path_len)
			continue;

		for (j = 0; j < an_ovl_path_len; j++)
			DDPMSG("%s crtc-%d an-crtc-%d %s\n",
				__func__, i, an_crtc_path_data->host_crtc_id,
				mtk_dump_comp_str_id(an_ovl_path[j]));

		if (an_crtc_path_data->dual_ovl_enable)
			an_ovl_path = an_crtc_path_data->dual_ovl_path;
		for (j = 0; j < an_dual_ovl_path_len; j++)
			DDPMSG("%s crtc-%d an-crtc-%d dual %s\n",
				__func__, i, an_crtc_path_data->host_crtc_id,
				mtk_dump_comp_str_id(an_ovl_path[j]));

		DDPMSG("%s crtc-%d output_comp %s\n",
			__func__, i,
			mtk_dump_comp_str_id(an_output_comp));
	}

	DDPMSG("%s an crtc_nr %d\n", __func__, path_info->crtc_nr);

	return 0;
}
EXPORT_SYMBOL(mtk_drm_path_get_an_crtc_path_data);

#if IS_ENABLED(CONFIG_VHOST_CMDQ_DMA_MAP)
static int add_vbuf(struct sg_table *sgt, int *id)
{
	struct drm_vbuf *new_vbuf;
	int ret;

	new_vbuf = kzalloc(sizeof(*new_vbuf), GFP_KERNEL);
	if (!new_vbuf)
		return -ENOMEM;

	new_vbuf->sgt = sgt;
	INIT_LIST_HEAD(&new_vbuf->list);

	mutex_lock(&vbuf_lock);
	ret = idr_alloc(&vbuf_idr, new_vbuf, 1, 0, GFP_KERNEL);
	mutex_unlock(&vbuf_lock);
	if (ret < 0) {
		ret = -ENOMEM;
		kfree(new_vbuf);
		return ret;
	}

	new_vbuf->id = ret;
	*id = ret;

	list_add_tail(&new_vbuf->list, &vbuf_list);
	return 0;
}

int remove_vbuf_by_id(uint32_t id)
{
	struct drm_vbuf *vbuf;
	int ret;
	struct drm_device *drm_dev = mtk_drm_get_dev();

	mutex_lock(&vbuf_lock);
	vbuf = idr_find(&vbuf_idr, id);
	if (vbuf) {
		idr_remove(&vbuf_idr, id);
		list_del(&vbuf->list);
		mtk_gem_prime_unmap_vbuf(drm_dev, vbuf->sgt);
		sg_free_table(vbuf->sgt);
		kfree(vbuf->sgt);
		kfree(vbuf);
		mutex_unlock(&vbuf_lock);
		DDPMSG("vbuf with id %d removed\n", id);
		return 0;
	}
	mutex_unlock(&vbuf_lock);
	DDPMSG("[ERR] vbuf with id %d not found\n", id);
	return -EINVAL;
}
EXPORT_SYMBOL(remove_vbuf_by_id);


int mtk_drm_setup_vbuf_map(struct virtio_disp_req_vbuf *vbuf, struct virtio_disp_rsp_vbuf *rsp_vbuf)
{
	struct sg_table *sgt;
	struct scatterlist *sgl;
	phys_addr_t paddr;
	dma_addr_t iova;
	struct page *page;
	struct virtio_drm_mem_entry *entry;
	int i, ret;
	uint32_t nents;
	struct drm_device *drm_dev = mtk_drm_get_dev();

	if (IS_ERR_OR_NULL(drm_dev)) {
		DDPMSG("%s invalid drm dev\n", __func__);
		return -EFAULT;
	}

	/*alloc sgt*/
	sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (sgt == NULL) {
		ret = -ENOMEM;
		DDPMSG("fail to create sg table.\n");
		return ret;
	}

	nents = vbuf->num_ents;
	ret = sg_alloc_table(sgt, nents, GFP_KERNEL);
	if (ret) {
		ret = -ENOMEM;
		kfree(sgt);
		DDPMSG("fail to alloc sg table.\n");
		return ret;
	}

	/*rebuild sgt*/
	entry = (struct virtio_drm_mem_entry *)(vbuf->buf_entries);
	/*DDPMSG("nents:%d, entry:%p, size:%d\n", nents, entry, vbuf->buf_size);*/
	sgl = sgt->sgl;
	for (i = 0; i < nents; i++) {
		paddr = entry[i].addr;
		page = phys_to_page(paddr);
		sg_set_page(sgl, page, (entry[i].length), 0);
		sgl = sg_next(sgl);
	}

	/*map sgt*/
	ret = mtk_gem_prime_map_vbuf(drm_dev, sgt);
	if (ret) {
		sg_free_table(sgt);
		kfree(sgt);
		DDPMSG("[ERR] map sg table fail.\n");
		return ret;
	}

	/*insert vbuf to vbuf list*/
	ret = add_vbuf(sgt, &(rsp_vbuf->idr));

	/*get dma_addr*/
	rsp_vbuf->dma_addr = sg_dma_address(sgt->sgl);

	return ret;

}
EXPORT_SYMBOL(mtk_drm_setup_vbuf_map);
#endif
#endif
