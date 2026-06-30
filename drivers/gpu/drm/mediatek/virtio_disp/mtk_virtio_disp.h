/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _MTK_VIRTIO_DISP_H_
#define _MTK_VIRTIO_DISP_H_

#ifndef MAX_PANEL_NAME_LEN
#define MAX_PANEL_NAME_LEN 64
#endif

#define MAX_VIRT_CRTC	5
#define MAX_OVL_COMP	32

enum virtio_disp_req_id {
	VIRTIO_DISP_CMD_GET_PANEL = 0,
	VIRTIO_DISP_CMD_UPDATE_PANEL = 1,
	VIRTIO_DISP_CRTC_ENABLE = 2,
	VIRTIO_DISP_CMD_HOTPLUG_STATUS,
	VIRTIO_DISP_CMD_CHECK_INDEX,
	VIRTIO_DRM_VBUF_MAP,
	VIRTIO_DRM_VBUF_UNMAP,
	VIRTIO_DISP_CMD_GET_CRTC_INFO,
};

struct virtio_disp_req_crtc {
	uint32_t output_comp_id;
	uint32_t enable;
};

struct virtio_disp_req_panel {
	uint32_t output_comp_id;
};

struct virtio_drm_mem_entry {
	uint64_t addr;
	uint32_t length;
	uint32_t padding;
};

struct virtio_disp_req_vbuf {
	uint32_t num_ents;
	void *buf_entries;
	uint32_t buf_size;
	uint32_t id;
};

union virtio_disp_req_param {
	struct virtio_disp_req_panel panel;
	struct virtio_disp_req_crtc crtc;
#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
	struct virtio_disp_req_vbuf vbuf;
#endif
	uint32_t event;
};

struct virtio_disp_req {
	uint32_t id;
	enum virtio_disp_req_id cmd;
	union virtio_disp_req_param param;
};

struct virtio_disp_panel {
	uint32_t id;
	uint32_t width;
	uint32_t height;
	uint32_t hfp;
	uint32_t hsa;
	uint32_t hbp;
	uint32_t vfp;
	uint32_t vsa;
	uint32_t vbp;
	uint32_t vrefresh;
	uint32_t width_mm;
	uint32_t height_mm;
	uint32_t degree;
	char panel_name[MAX_PANEL_NAME_LEN];
};

struct virtio_disp_rsp_crtc {
	uint32_t crtc_id;
	uint32_t crtc_obj_id;
	uint32_t enable;
};

struct virtio_disp_an_crtc_path_data {
	uint32_t host_crtc_id;
	uint32_t is_shared_device;
	uint32_t dual_ovl_enable;
	uint32_t ovl_path[MAX_OVL_COMP];
	uint32_t ovl_path_len;
	uint32_t dual_ovl_path[MAX_OVL_COMP];
	uint32_t dual_ovl_path_len;
	uint32_t output_comp;
};

struct virtio_disp_rsp_crtc_path_info {
	uint32_t crtc_nr;
	struct virtio_disp_an_crtc_path_data crtc_path_data[MAX_VIRT_CRTC];
};

struct virtio_disp_rsp_vbuf {
	uint32_t idr;
	uint64_t dma_addr;
};

union virtio_disp_rsp_param {
	struct virtio_disp_panel panel;
	struct virtio_disp_rsp_crtc crtc;
#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
	struct virtio_disp_rsp_vbuf vbuf;
#endif
	struct virtio_disp_rsp_crtc_path_info path_info;
};

struct virtio_disp_rsp {
	uint32_t rc;
	union virtio_disp_rsp_param param;
};


/**
 * Request, Response, and Event protocol for virtio-display device.
 */
struct virtio_disp_event {
	uint32_t id;
	uint32_t type;
	uint32_t cmd_id;
	struct virtio_disp_rsp rsp;
	uint32_t event;
};

typedef void (*mtk_virt_hotplug_cb)(unsigned int event);

/**
 * Command for virtio-disp device.
 */
struct virtio_disp_cmd {
	struct virtio_disp_req req;
	struct virtio_disp_rsp rsp;
	struct completion done;
	mtk_virt_hotplug_cb cb;
};
#if IS_ENABLED(CONFIG_MTK_VIRTIO_DISP)
struct virtio_disp_cmd *virtio_disp_cmd_create(void);
void virtio_disp_cmd_destroy(struct virtio_disp_cmd *cmd);
int virtio_disp_cmd_submit(struct virtio_disp_cmd *cmd);
#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
dma_addr_t mtk_drm_vbuffer_map(struct sg_table *sgt, uint32_t *idr);
void mtk_drm_vbuffer_unmap(uint32_t idr);
#endif
#endif

#endif
