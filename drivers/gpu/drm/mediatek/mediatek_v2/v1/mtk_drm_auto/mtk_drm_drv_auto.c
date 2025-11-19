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

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_log.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_plane.h"
#include "mtk_drm_gem.h"

#include "mtk_drm_ddp_comp_auto.h"
#include "mtk_drm_drv_auto.h"
#include "mtk_drm_crtc_auto.h"
#include "mtk_drm_mmp.h"
#include "mtk_virtio_disp.h"

struct mtk_crtc_path_data *mtk_disp_crtc_path_data[MAX_CRTC] = {NULL};

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_HOST) || IS_ENABLED(MTK_DRM_MEDIATEK_AUTO_AN_ONLY)
#define OVLSYS_PATH_DATA_PROP_NAME "ovlsys-path-data"
#define DUAL_OVLSYS_PATH_DATA_PROP_NAME "dual-ovlsys-path-data"
#define PRIMARY_LAYER_SWAP_PROP_NAME "is-primary-layer-swap"
#define OUTPUT_COMP_PROP_NAME "output-comp"
#define DSC_COMP_PROP_NAME "dsc-comp"
#define EXDMA_USAGE_PROP_NAME "exdma-usage"
#define INIT_IN_LK_PROP_NAME "init-in-lk"
#define AN_CRTC_ID_PROP_NAME "an-crtc-id"
#define GUEST_EXCLUSIVE_PROP_NAME "is-guest-exclusive-device"
#define DUAL_PIPE_PROP_NAME "is-dual-pipe"

#if IS_ENABLED(CONFIG_DRM_PATH_CONFIG_FROM_DTS)
static void mtk_drm_path_update_crtc_prop(struct device_node *node, u32 crtc_id,
					  struct mtk_crtc_path_data *crtc_path_data)
{
	u32 prop_val = 0;

	if (!of_property_read_u32(node, PRIMARY_LAYER_SWAP_PROP_NAME, &prop_val)) {
		crtc_path_data->is_primary_layer_swap = prop_val ? true : false;
		DDPMSG("%s crtc-%d primary_layer_swap %d\n", __func__, crtc_id, prop_val);
	}

	if (!of_property_read_u32(node, GUEST_EXCLUSIVE_PROP_NAME, &prop_val)) {
		crtc_path_data->is_guest_exclusive_device = prop_val ? true : false;
		DDPMSG("%s crtc-%d is_guest_exclusive_device %d\n", __func__, crtc_id, prop_val);
	}

	if (!of_property_read_u32(node, DUAL_PIPE_PROP_NAME, &prop_val)) {
		crtc_path_data->is_dual_pipe = prop_val ? true : false;
		DDPMSG("%s crtc-%d is_dual_pipe %d\n", __func__, crtc_id, prop_val);
	}
}

static void mtk_drm_path_update_output_comp(struct device_node *node, u32 crtc_id,
					    struct mtk_crtc_path_data *crtc_path_data)
{
	u32 output_comp = 0;
	int ret = 0;
	enum mtk_ddp_comp_id output_comp_id = 0;
	int i = 0, ddp_mode = DDP_MAJOR, ddp_path = DDP_FIRST_PATH;
	enum mtk_ddp_comp_id comp_id;

	ret = of_property_read_u32(node, OUTPUT_COMP_PROP_NAME, &output_comp);
	if (ret < 0) {
		DDPMSG("[E]%s crtc-%d has no property %s\n",
		       __func__, crtc_id, OUTPUT_COMP_PROP_NAME);
		return;
	}

	output_comp_id = mtk_ddp_comp_get_map_id(output_comp);
	if (!mtk_ddp_comp_is_output_by_id(output_comp_id)) {
		DDPMSG("[E]%s crtc-%d invalid output comp %s\n",
		       __func__, crtc_id, mtk_dump_comp_str_id(output_comp_id));
		return;
	}

	DDPMSG("%s crtc-%d output_comp %d %s\n", __func__, crtc_id, output_comp,
		mtk_dump_comp_str_id(output_comp_id));

	for_each_comp_id_target_mode_path_in_disp_path_data(comp_id, crtc_path_data, i, ddp_mode,
							    ddp_path) {
		if (mtk_ddp_comp_is_output_by_id(comp_id)) {
			DDPMSG("%s crtc-%d update output comp %s -> %s\n",
			       __func__, crtc_id, mtk_dump_comp_str_id(comp_id),
			       mtk_dump_comp_str_id(output_comp_id));
			crtc_path_data->path[ddp_mode][ddp_path][i] = output_comp_id;
		}
	}
}

static void mtk_drm_path_update_dsc_comp(struct device_node *node, u32 crtc_id,
					 struct mtk_crtc_path_data *crtc_path_data)
{
	u32 dsc_comp = 0;
	int ret = 0;
	enum mtk_ddp_comp_id dsc_comp_id = 0;
	int i = 0, ddp_mode = DDP_MAJOR, ddp_path = DDP_FIRST_PATH;
	enum mtk_ddp_comp_id comp_id;

	ret = of_property_read_u32(node, DSC_COMP_PROP_NAME, &dsc_comp);
	if (ret < 0)
		return;

	dsc_comp_id = mtk_ddp_comp_get_map_id(dsc_comp);
	if (mtk_ddp_comp_get_type(dsc_comp_id) != MTK_DISP_DSC) {
		DDPMSG("[E]%s crtc-%d invalid dsc comp %s\n",
		       __func__, crtc_id, mtk_dump_comp_str_id(dsc_comp_id));
		return;
	}

	DDPMSG("%s crtc-%d dsc_comp %d %s\n",
		__func__, crtc_id, dsc_comp_id,
		mtk_dump_comp_str_id(dsc_comp_id));

	for_each_comp_id_target_mode_path_in_disp_path_data(comp_id, crtc_path_data, i, ddp_mode,
							    ddp_path) {
		if (mtk_ddp_comp_is_comp_out_cb_by_id(comp_id)) {
			DDPMSG("%s crtc-%d replace %s -> %s\n",
			       __func__, crtc_id,
			       mtk_dump_comp_str_id(comp_id),
			       mtk_dump_comp_str_id(dsc_comp_id));
			crtc_path_data->path[ddp_mode][ddp_path][i] = dsc_comp_id;
		}
	}
}

static void mtk_drm_path_update_exdma_comp_type(struct device_node *node, u32 crtc_id,
						struct mtk_crtc_path_data *crtc_path_data)
{
	char *prop_name = EXDMA_USAGE_PROP_NAME;

	int exdma_count = 0;
	/*
	 * PHY_COMP 0, yocto layer
	 * VIRT_COMP 1, android layer
	 * SHARE_COMP 3, yocto and android
	 */
	u32 *exdma_type = NULL;

	exdma_count = of_property_count_u32_elems(node, prop_name);
	if (exdma_count < 0) {
		DDPMSG("[E]%s crtc-%d has no property %s\n", __func__, crtc_id, prop_name);
		return;
	}
	DDPMSG("%s crtc-%d prop %s count %d\n", __func__, crtc_id, prop_name, exdma_count);

	exdma_type = kcalloc(exdma_count, sizeof(*exdma_type), GFP_KERNEL);
	if (!exdma_type) {
		DDPMSG("[E]%s crtc-%d property %s kcalloc fail\n", __func__, crtc_id, prop_name);
		return;
	}

	if (of_property_read_u32_array(node, prop_name, exdma_type, exdma_count) < 0) {
		DDPMSG("[E]%s crtc-%d read property %s fail\n", __func__, crtc_id, prop_name);
		kfree(exdma_type);
		return;
	}

	crtc_path_data->exdma_type = exdma_type;
	crtc_path_data->exdma_count = exdma_count;
}

static int mtk_drm_path_update_ovlsys_data_impl(struct mtk_drm_private *private,
						struct device_node *node, u32 crtc_id,
						struct mtk_crtc_path_data *crtc_path_data,
						char *prop_name)
{
	enum mtk_ddp_comp_id *ovl_path = NULL;
	int ovl_path_len = 0, ret = 0;
	u32 *prop_values = NULL;
	bool is_dual_ovl = false;
	u32 *exdma_type = NULL;
	u32 exdma_count = 0;
	int i = 0, j = 0;
	enum mtk_ddp_comp_id comp_id;

	if (strstr(prop_name, "dual"))
		is_dual_ovl = true;

	ovl_path_len = of_property_count_u32_elems(node, prop_name);
	if (ovl_path_len < 0) {
		DDPMSG("[E]%s crtc-%d has no property %s\n", __func__, crtc_id, prop_name);
		return -EINVAL;
	}
	DDPMSG("%s crtc-%d ovl_path_len %d is_dual_ovl %d\n",
		__func__, crtc_id, ovl_path_len, is_dual_ovl);

	ovl_path = kcalloc(ovl_path_len, sizeof(enum mtk_ddp_comp_id), GFP_KERNEL);
	if (!ovl_path) {
		DDPMSG("[E]%s crtc-%d data path kcalloc fail\n", __func__, crtc_id);
		return -EINVAL;
	}

	prop_values = kcalloc(ovl_path_len, sizeof(*prop_values), GFP_KERNEL);
	if (!prop_values) {
		DDPMSG("[E]%s crtc-%d prop_values kcalloc fail\n", __func__, crtc_id);
		kfree(ovl_path);
		return -EINVAL;
	}

	if (of_property_read_u32_array(node, prop_name, prop_values, ovl_path_len) < 0) {
		DDPMSG("[E]%s crtc-%d read property %s fail\n", __func__, crtc_id, prop_name);
		kfree(prop_values);
		kfree(ovl_path);
		return -EINVAL;
	}

	for (i = 0; i < ovl_path_len; i++) {
		enum mtk_ddp_comp_id id = mtk_ddp_comp_get_map_id(prop_values[i]);

		if (id == DDP_COMPONENT_ID_MAX) {
			DDPMSG("[E]%s crtc-%d cannot find comp:%d, use default path\n",
				__func__, crtc_id, prop_values[i]);
			ret = -EINVAL;
			break;
		}

		ovl_path[i] = id;
	}
	if (ret) {
		kfree(prop_values);
		kfree(ovl_path);
		return -EINVAL;
	}

	kfree(prop_values);

	if (is_dual_ovl) {
		crtc_path_data->dual_ovl_path[0] = ovl_path;
		crtc_path_data->dual_ovl_path_len[0] = ovl_path_len;
	} else {
		crtc_path_data->ovl_path[0][0] = ovl_path;
		crtc_path_data->ovl_path_len[0][0] = ovl_path_len;
	}

	exdma_type = crtc_path_data->exdma_type;
	exdma_count = crtc_path_data->exdma_count;

	if (!exdma_type || !exdma_count) {
		DDPMSG("[E]%s crtc-%d invalid exdma type %p count %d\n",
			__func__, crtc_id, exdma_type, exdma_count);
		return -EINVAL;
	}

	for (i = 0, j = 0; i < ovl_path_len; i++) {
		comp_id = ovl_path[i];
		if (mtk_ddp_comp_is_rdma_by_id(comp_id) && j < exdma_count) {
			DDPMSG("%s crtc-%d update [%d] comp %s type to %d\n",
			   __func__, crtc_id, j, mtk_dump_comp_str_id(comp_id), exdma_type[j]);
			mtk_ddp_comp_init_type(private, comp_id, exdma_type[j]);
			j++;
		}
	}

	return 0;
}

static int mtk_drm_path_update_ovlsys_data(struct mtk_drm_private *private,
	struct device_node *node, u32 crtc_id, struct mtk_crtc_path_data *crtc_path_data)
{
	int ret = 0;

	mtk_drm_path_update_exdma_comp_type(node, crtc_id, crtc_path_data);

	ret = mtk_drm_path_update_ovlsys_data_impl(private, node, crtc_id,
						   crtc_path_data,
						   OVLSYS_PATH_DATA_PROP_NAME);
	if (ret < 0) {
		DDPMSG("[E]%s crtc-%d update primary ovlsys data fail\n", __func__, crtc_id);
		return ret;
	}

	if (crtc_path_data->is_dual_pipe) {
		ret = mtk_drm_path_update_ovlsys_data_impl(private, node, crtc_id,
							   crtc_path_data,
							   DUAL_OVLSYS_PATH_DATA_PROP_NAME);
		if (ret < 0) {
			DDPMSG("[E]%s crtc-%d update dual ovlsys data fail\n", __func__, crtc_id);
			return ret;
		}
	}

	return 0;
}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_HOST)
static void mtk_drm_path_update_an_ovlsys_data_impl(struct mtk_drm_private *private,
					u32 crtc_id, u32 an_crtc_id,
					struct mtk_crtc_path_data *crtc_path_data,
					struct virtio_disp_an_crtc_path_data *an_crtc_path_data,
					bool dual_ovl)
{
	int i = 0, j = 0;
	enum mtk_ddp_comp_id comp_id, *ovl_path, *an_ovl_path;
	u32 ovl_path_len;

	if (dual_ovl) {
		ovl_path = crtc_path_data->dual_ovl_path[0];
		ovl_path_len = crtc_path_data->dual_ovl_path_len[0];

		an_ovl_path = an_crtc_path_data->dual_ovl_path;
	} else {
		ovl_path = crtc_path_data->ovl_path[0][0];
		ovl_path_len = crtc_path_data->ovl_path_len[0][0];

		an_ovl_path = an_crtc_path_data->ovl_path;
	}

	/* exdma + blender */
	for (i = 0, j = 0; i < ovl_path_len && j < MAX_OVL_COMP; i += 2) {
		comp_id = ovl_path[i];

		/* update android exdma + blender */
		if (mtk_ddp_comp_is_rdma_by_id(comp_id) && mtk_ddp_comp_is_virt(private, comp_id)) {
			an_ovl_path[j] = ovl_path[i];
			an_ovl_path[j + 1] = ovl_path[i + 1];

			DDPMSG("%s crtc-%d %d [%d] update an comp %s %s\n",
			       __func__, crtc_id, an_crtc_id, j, mtk_dump_comp_str_id(comp_id),
			       mtk_dump_comp_str_id(ovl_path[i + 1]));

			j += 2;
		}
	}

	DDPMSG("%s crtc-%d an-crtc-%d an_ovl_path_len %d dual_ovl %d\n",
		__func__, crtc_id, an_crtc_id, j, dual_ovl);

	if (dual_ovl)
		an_crtc_path_data->dual_ovl_path_len = j;
	else
		an_crtc_path_data->ovl_path_len = j;
}

static void mtk_drm_path_update_an_ovlsys_data(struct mtk_drm_private *private,
					u32 crtc_id, u32 an_crtc_id,
					struct mtk_crtc_path_data *crtc_path_data,
					struct virtio_disp_an_crtc_path_data *an_crtc_path_data)
{
	mtk_drm_path_update_an_ovlsys_data_impl(private, crtc_id, an_crtc_id, crtc_path_data,
						an_crtc_path_data, false);

	if (crtc_path_data->is_dual_pipe)
		mtk_drm_path_update_an_ovlsys_data_impl(private, crtc_id, an_crtc_id, crtc_path_data,
							an_crtc_path_data, true);
}

static void mtk_drm_path_update_an_output_comp(u32 crtc_id, u32 an_crtc_id,
					struct mtk_crtc_path_data *crtc_path_data,
					struct virtio_disp_an_crtc_path_data *an_crtc_path_data)
{
	enum mtk_ddp_comp_id *ddp_path_data, an_output_comp = 0, comp_id = 0;
	int i = 0;
	u32 ddp_path_len;

	ddp_path_data = crtc_path_data->path[0][0];
	ddp_path_len = crtc_path_data->path_len[0][0];

	for (i = 0; i < ddp_path_len; i++) {
		comp_id = ddp_path_data[i];

		if (mtk_ddp_comp_is_output_by_id(comp_id)) {
			an_output_comp = mtk_ddp_comp_get_virt_output_comp(comp_id);
			break;
		}
	}

	an_crtc_path_data->output_comp = an_output_comp;

	DDPMSG("%s crtc-%d an-crtc-%d output comp %s %s\n",
		__func__, crtc_id, an_crtc_id,
		mtk_dump_comp_str_id(comp_id),
		mtk_dump_comp_str_id(an_output_comp));
}

static void mtk_drm_path_update_an_crtc_prop(u32 crtc_id, u32 an_crtc_id,
					struct mtk_crtc_path_data *crtc_path_data,
					struct virtio_disp_an_crtc_path_data *an_crtc_path_data)
{
	an_crtc_path_data->host_crtc_id = crtc_id;
	an_crtc_path_data->is_shared_device = !crtc_path_data->is_guest_exclusive_device;
	an_crtc_path_data->dual_ovl_enable = crtc_path_data->is_dual_pipe;

	DDPMSG("%s crtc-%d an-crtc-%d share %d dual_ovl %d\n",
		__func__, crtc_id, an_crtc_id,
		an_crtc_path_data->is_shared_device,
		an_crtc_path_data->dual_ovl_enable);
}

static void mtk_drm_path_update_an_path_data(struct mtk_drm_private *private,
						struct device_node *node, u32 crtc_id,
					     struct mtk_crtc_path_data *crtc_path_data,
					     struct virtio_disp_rsp_crtc_path_info *an_crtc_path_info)
{
	int ret = 0;
	u32 an_crtc_id = 0;
	struct virtio_disp_an_crtc_path_data *an_crtc_path_data;

	ret = of_property_read_u32(node, AN_CRTC_ID_PROP_NAME, &an_crtc_id);
	if (ret < 0)
		return;

	if (an_crtc_id >= MAX_VIRT_CRTC) {
		DDPMSG("%s crtc-%d invalid an-crtc-%d crtc_nr %d\n",
			__func__, crtc_id, an_crtc_id, an_crtc_path_info->crtc_nr);
		return;
	}

	an_crtc_path_data = &an_crtc_path_info->crtc_path_data[an_crtc_id];

	mtk_drm_path_update_an_ovlsys_data(private, crtc_id, an_crtc_id, crtc_path_data, an_crtc_path_data);

	mtk_drm_path_update_an_output_comp(crtc_id, an_crtc_id, crtc_path_data, an_crtc_path_data);

	mtk_drm_path_update_an_crtc_prop(crtc_id, an_crtc_id, crtc_path_data, an_crtc_path_data);

	an_crtc_path_info->crtc_nr++;

	DDPMSG("%s crtc-%d an-crtc-%d is_shared_device %d crtc_nr %d\n",
		__func__, crtc_id, an_crtc_id,
		an_crtc_path_data->is_shared_device,
		an_crtc_path_info->crtc_nr);
}
#endif
#endif

#endif

int mtk_drm_path_data_update(struct mtk_drm_private *private)
{
	int i, ret = 0;
	char crtc_node_name[100];
	struct device_node *crtc_node;
	struct mtk_crtc_path_data *crtc_path_data;
	struct virtio_disp_rsp_crtc_path_info *an_crtc_path_info;
	struct device_node *node = private->mmsys_dev->of_node;

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
	return 0;
#endif

#if IS_ENABLED(CONFIG_DRM_PATH_CONFIG_FROM_DTS)
	for (i = 0; i < MAX_CRTC; i++) {
		crtc_path_data = mtk_disp_crtc_path_data[i];

		/* writeback crtc, default enable*/
		if (i == 2) {
			an_crtc_path_info->crtc_nr++;
			crtc_path_data->is_path_enable = true;
			continue;
		}

		ret = snprintf(crtc_node_name, sizeof(crtc_node_name) - 1, "crtc-%d", i);
		if (ret < 0) {
			DDPMSG("[E]%s crtc-%d snprintf failed\n", __func__, i);
			crtc_path_data->is_path_enable = false;
			continue;
		}

		crtc_node = of_get_child_by_name(node, crtc_node_name);
		if (!crtc_node) {
			DDPMSG("%s crtc-%d %s disable\n", __func__, i, crtc_node_name);
			crtc_path_data->is_path_enable = false;
			continue;
		}

		crtc_path_data->is_path_enable = true;

		mtk_drm_path_update_crtc_prop(crtc_node, i, crtc_path_data);

		ret = mtk_drm_path_update_ovlsys_data(private, crtc_node, i, crtc_path_data);
		if (ret < 0)
			DDPMSG("[E]%s crtc-%d update ovlsys data failed\n", __func__, i);

		mtk_drm_path_update_dsc_comp(crtc_node, i, crtc_path_data);

		mtk_drm_path_update_output_comp(crtc_node, i, crtc_path_data);

		mtk_drm_path_update_an_path_data(private, crtc_node, i, crtc_path_data, an_crtc_path_info);
	}

	return ret;
#else
	for (i = 0; i < MAX_CRTC; i++) {
		switch (i) {
		case 0:
			mtk_disp_crtc_path_data[i] =
				(struct mtk_crtc_path_data *)private->data->main_path_data;
			break;
		case 1:
			mtk_disp_crtc_path_data[i] =
				(struct mtk_crtc_path_data *)private->data->ext_path_data;
			break;
		case 2:
			mtk_disp_crtc_path_data[i] =
				(struct mtk_crtc_path_data *)private->data->third_path_data;
			break;
		case 3:
			mtk_disp_crtc_path_data[i] =
				(struct mtk_crtc_path_data *)private->data->fourth_path_data_discrete;
			break;
		case 4:
			mtk_disp_crtc_path_data[i] =
				(struct mtk_crtc_path_data *)private->data->fifth_path_data;
			break;
		case 5:
			mtk_disp_crtc_path_data[i] =
				(struct mtk_crtc_path_data *)private->data->sixth_path_data;
			break;
		case 6:
			mtk_disp_crtc_path_data[i] =
				(struct mtk_crtc_path_data *)private->data->seventh_path_data;
			break;
		default:
			break;
		}
	}
	return ret;
#endif
}

int mtk_drm_path_crtc_create(struct drm_device *drm)
{
	int i = 0, ret = 0;
	struct mtk_crtc_path_data *crtc_path_data;

	for (i = 0; i < MAX_CRTC; i++) {
		crtc_path_data = mtk_disp_crtc_path_data[i];
		if (!crtc_path_data) {
			DDPMSG("%s path-%d data invalid\n", __func__, i);
			continue;
		}

#if IS_ENABLED(CONFIG_VHOST_DISP) || IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
		if (!crtc_path_data->is_path_enable) {
			DDPMSG("%s path-%d data disabled\n", __func__, i);
			continue;
		}
#endif

		DDPMSG("%s CRTC%d MAX_CRTC %d\n", __func__, i, MAX_CRTC);
		ret = mtk_drm_crtc_create(drm, crtc_path_data);
		if (ret < 0) {
			DDPMSG("%s path-%d crtc create fail\n", __func__, i);
			break;
		}
	}
	return ret;
}
