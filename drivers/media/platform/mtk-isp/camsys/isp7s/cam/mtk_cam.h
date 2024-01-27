/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_H
#define __MTK_CAM_H

#include <linux/list.h>
#include <linux/of.h>
#include <linux/rpmsg.h>
#include <media/media-device.h>
#include <media/media-request.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>

#include "imgsensor-user.h"

#include "mtk_cam-hsf-def.h"
#include "mtk_cam-ipi.h"
#include "mtk_cam-larb.h"
#include "mtk_cam-pool.h"
#include "mtk_cam-raw.h"
#include "mtk_cam-request.h"
#include "mtk_cam-seninf-drv.h"
#include "mtk_cam-seninf-if.h"
#include "mtk_camera-v4l2-controls.h"

#define CCD_READY 1


#define SENSOR_FMT_MASK			0xFFFF

struct mtk_cam_debug_fs;
struct mtk_cam_request;
struct mtk_raw_pipeline;
struct mtk_sv_pipeline;
struct mtk_mraw_pipeline;

struct mtk_cam_device;
struct mtk_rpmsg_device;

#define MAX_PIPES_PER_STREAM 5

struct mtk_cam_ctx {
	struct mtk_cam_device *cam;
	unsigned int stream_id;

	/* v4l2 related */
	unsigned int enabled_node_cnt;
	unsigned int streaming_node_cnt;
	int has_raw_subdev;

	struct media_pipeline pipeline;
	struct v4l2_subdev *sensor;
	//struct v4l2_subdev *prev_sensor;
	struct v4l2_subdev *seninf;
	//struct v4l2_subdev *prev_seninf;
	struct v4l2_subdev *pipe_subdevs[MAX_PIPES_PER_STREAM];

	/* rpmsg related */
	struct rpmsg_channel_info rpmsg_channel;
	struct mtk_rpmsg_device *rpmsg_dev;
	struct work_struct session_work;
	bool session_created;
	struct completion session_complete;

	struct task_struct *sensor_worker_task;
	struct kthread_worker sensor_worker;
	struct workqueue_struct *composer_wq;
	struct workqueue_struct *frame_done_wq;

	struct mtk_cam_device_buf cq_buffer;
	struct mtk_cam_device_buf ipi_buffer;

	struct mtk_cam_pool	cq_pool;
	struct mtk_cam_pool	ipi_pool;

	/* TODO:
	 * life-cycle of work buffer during switch
	 * e.g., PDI/camsv's imgo
	 */

	atomic_t streaming;
	int used_pipe;

	struct mtk_raw_pipeline *pipe;
	//struct mtk_camsv_pipeline *sv_pipe[MAX_SV_PIPES_PER_STREAM];
	//struct mtk_mraw_pipeline *mraw_pipe[MAX_MRAW_PIPES_PER_STREAM];

	/* list for struct mtk_cam_job */
	struct list_head	running_jobs;
};

struct mtk_cam_v4l2_pipelines {
	int num_raw;
	struct mtk_raw_pipeline *raw;

	int num_camsv;
	struct mtk_camsv_pipeline *camsv;

	int num_mraw;
	struct mtk_mraw_pipeline *mraw;
};

struct mtk_cam_engines {
	int num_seninf_devices;

	int num_raw_devices;
	int num_camsv_devices;
	int num_mraw_devices;
	int num_larb_devices;

	/* raw */
	struct device **raw_devs;
	struct device **yuv_devs;

	/* camsv */
	struct device **sv_devs;

	/* mraw */
	struct device **mraw_devs;

	/* larb */
	struct device **larb_devs;
};

struct mtk_cam_device {
	struct device *dev;
	void __iomem *base;

	struct v4l2_device v4l2_dev;
	struct v4l2_async_notifier notifier;
	struct media_device media_dev;

	atomic_t initialize_cnt;

	//TODO: for real SCP
	//struct device *smem_dev;
	//struct platform_device *scp_pdev; /* only for scp case? */
	phandle rproc_phandle;
	struct rproc *rproc_handle;

	phandle rproc_ccu_phandle;
	struct rproc *rproc_ccu_handle;

	struct mtk_cam_v4l2_pipelines	pipelines;
	struct mtk_cam_engines		engines;

	struct mutex queue_lock;

	unsigned int max_stream_num;
	unsigned int streaming_ctx;
	unsigned int streaming_pipe;
	struct mtk_cam_ctx *ctxs;

	/* request related */
	struct list_head pending_job_list;
	spinlock_t pending_job_lock;
	struct list_head running_job_list;
	unsigned int running_job_count;
	spinlock_t running_job_lock;

	//struct mtk_cam_debug_fs *debug_fs;
	//struct workqueue_struct *debug_wq;
	//struct workqueue_struct *debug_exception_wq;
	//wait_queue_head_t debug_exception_waitq;
};

struct device *mtk_cam_root_dev(void);

int mtk_cam_set_dev_raw(struct device *dev, int idx,
			struct device *raw, struct device *yuv);
int mtk_cam_set_dev_sv(struct device *dev, int idx, struct device *sv);
int mtk_cam_set_dev_mraw(struct device *dev, int idx, struct device *mraw);
 /* special case: larb dev is push back into array */
int mtk_cam_set_dev_larb(struct device *dev, struct device *larb);
struct device *mtk_cam_get_larb(struct device *dev, int larb_id);

static inline struct mtk_cam_request *
to_mtk_cam_req(struct media_request *__req)
{
	return container_of(__req, struct mtk_cam_request, req);
}

static inline void
mtk_cam_pad_fmt_enable(struct v4l2_mbus_framefmt *framefmt, bool enable)
{
	if (enable)
		framefmt->flags |= V4L2_MBUS_FRAMEFMT_PAD_ENABLE;
	else
		framefmt->flags &= ~V4L2_MBUS_FRAMEFMT_PAD_ENABLE;
}

static inline bool
mtk_cam_is_pad_fmt_enable(struct v4l2_mbus_framefmt *framefmt)
{
	return framefmt->flags & V4L2_MBUS_FRAMEFMT_PAD_ENABLE;
}

struct mtk_cam_ctx *mtk_cam_find_ctx(struct mtk_cam_device *cam,
				     struct media_entity *entity);
struct mtk_cam_ctx *mtk_cam_start_ctx(struct mtk_cam_device *cam,
				      struct mtk_cam_video_device *node);
void mtk_cam_stop_ctx(struct mtk_cam_ctx *ctx, struct media_entity *entity);
int mtk_cam_ctx_all_nodes_streaming(struct mtk_cam_ctx *ctx);
int mtk_cam_ctx_all_nodes_idle(struct mtk_cam_ctx *ctx);
int mtk_cam_ctx_stream_on(struct mtk_cam_ctx *ctx);
int mtk_cam_ctx_stream_off(struct mtk_cam_ctx *ctx);
int isp_composer_create_session(struct mtk_cam_ctx *ctx);
void isp_composer_destroy_session(struct mtk_cam_ctx *ctx);

int mtk_cam_call_seninf_set_pixelmode(struct mtk_cam_ctx *ctx,
				      struct v4l2_subdev *sd,
				      int pad_id, int pixel_mode);

void mtk_cam_dev_req_enqueue(struct mtk_cam_device *cam,
			     struct mtk_cam_request *req);
//void mtk_cam_dev_req_cleanup(struct mtk_cam_ctx *ctx, int pipe_id, int buf_state);
//void mtk_cam_dev_req_clean_pending(struct mtk_cam_device *cam, int pipe_id,
//				   int buf_state);

void mtk_cam_dev_req_try_queue(struct mtk_cam_device *cam);

int mtk_cam_link_validate(struct v4l2_subdev *sd,
			  struct media_link *link,
			  struct v4l2_subdev_format *source_fmt,
			  struct v4l2_subdev_format *sink_fmt);
int mtk_cam_seninf_link_validate(struct v4l2_subdev *sd,
			  struct media_link *link,
			  struct v4l2_subdev_format *source_fmt,
			  struct v4l2_subdev_format *sink_fmt);
int mtk_cam_sv_link_validate(struct v4l2_subdev *sd,
			  struct media_link *link,
			  struct v4l2_subdev_format *source_fmt,
			  struct v4l2_subdev_format *sink_fmt);
int mtk_cam_mraw_link_validate(struct v4l2_subdev *sd,
			  struct media_link *link,
			  struct v4l2_subdev_format *source_fmt,
			  struct v4l2_subdev_format *sink_fmt);

#endif /*__MTK_CAM_H*/
