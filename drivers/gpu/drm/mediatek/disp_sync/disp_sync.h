/* SPDX-License-Identifier: GPL-2.0*/

/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/irq.h>
#include <linux/component.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/of_graph.h>
#include <linux/types.h>

#include <uapi/drm/mediatek_drm.h>

#define MTK_TIMELINE_PLANE_INPUT_LAYER_COUNT 25
#define MTK_INVALID_FENCE_FD (-1)


enum MTK_TIMELINE_ENUM {
	MTK_TIMELINE_INPUT_TIMELINE_ID = MTK_TIMELINE_PLANE_INPUT_LAYER_COUNT,
	MTK_TIMELINE_PRIMARY_PRESENT_TIMELINE_ID,
	MTK_TIMELINE_SECONDARY_PRESENT_TIMELINE_ID,
	MTK_TIMELINE_THIRD_PRESENT_TIMELINE_ID,
	MTK_TIMELINE_FOURTH_PRESENT_TIMELINE_ID,
	MTK_TIMELINE_FIFTH_PRESENT_TIMELINE_ID,
	MTK_TIMELINE_COUNT,
};

enum MTK_FENCE_TYPE {
	MTK_PRESENT_FENCE = 1,
	MTK_LAYER_RELEASE_FENCE = 2,
};

enum MTK_SESSION_TYPE {
	MTK_SESSION_V_PRIMARY = 1,
	MTK_SESSION_V_DYNAMIC_INTERNAL = 2,
	MTK_SESSION_V_DYNAMIC_EX1 = 3,
	MTK_SESSION_V_DYNAMIC_EX2 = 4,
	MTK_SESSION_V_DYNAMIC_EX3 = 5,
};

enum BUFFER_STATE { create, insert, reg_configed, reg_updated, read_done };

struct mtk_fence_buf_info {
	struct list_head list;
	unsigned int idx;
	int fence;
	struct ion_client *client;
	struct ion_handle *hnd;
	unsigned long mva;
	unsigned int size;
	unsigned int mva_offset;
	enum BUFFER_STATE buf_state;
	unsigned int set_input_ticket;
	unsigned int trigger_ticket; /* we can't update trigger_ticket_end,*/
	/*because can't gurantee ticket being updated before cmdq callback*/

	unsigned int release_ticket;
	unsigned int enable;
	unsigned long long ts_create;
	unsigned long long ts_period_keep;
	unsigned int seq;
	unsigned int layer_type;
};

struct mtk_fence_sync_info {
	unsigned int inited;
	struct mutex mutex_lock;
	unsigned int layer_id;
	unsigned int fence_idx;
	unsigned int timeline_idx;
	unsigned int inc;
	unsigned int cur_idx;
	struct sw_sync_timeline *timeline;
	struct list_head buf_list;
};


struct FENCE_LAYER_INFO {
	unsigned int layer;
	unsigned int layer_en;
	unsigned int fmt;
	unsigned long addr;
	unsigned long vaddr;
	unsigned int src_x;
	unsigned int src_y;
	unsigned int src_w;
	unsigned int src_h;
	unsigned int src_pitch;
	unsigned int dst_x;
	unsigned int dst_y;
	unsigned int dst_w;
	unsigned int dst_h; /* clip region */
	unsigned int keyEn;
	unsigned int key;
	unsigned int aen;
	unsigned char alpha;

	unsigned int isDirty;

	unsigned int buff_idx;
	unsigned int security;
};


struct mtk_fence_info {
	unsigned int inited;
	struct mutex sync_lock;
	unsigned int layer_id;
	unsigned int fence_idx;
	unsigned int timeline_idx;
	unsigned int fence_fd;
	unsigned int inc;
	unsigned int cur_idx;
	struct sync_timeline *timeline;
	struct list_head buf_list;
	struct FENCE_LAYER_INFO cached_config;
};


struct mtk_fence_session_sync_info {
	unsigned int session_id;
	struct mtk_fence_info session_layer_info[MTK_TIMELINE_COUNT];
};

struct disp_sync_vblank_reply {
	unsigned int sequence;
	long tval_sec;
	long tval_usec;
};

struct mtk_disp_sync {
	struct cdev cdev;
	struct clk *clk;
	void __iomem *regs;
	resource_size_t regs_pa;
	atomic_t vblank_irq;
	wait_queue_head_t vblank_irq_wq;
	ktime_t vblank_time;
};
