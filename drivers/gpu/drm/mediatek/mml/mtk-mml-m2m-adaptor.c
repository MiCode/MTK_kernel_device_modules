// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Iris-SC Yang <iris-sc.yang@mediatek.com>
 */

#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>

#include "mtk-mml-m2m-adaptor.h"
#include "mtk-mml-adaptor.h"

int m2m_max_cache_task = 4;
module_param(m2m_max_cache_task, int, 0644);

int m2m_max_cache_cfg = 2;
module_param(m2m_max_cache_cfg, int, 0644);

struct mml_v4l2_dev {
	struct v4l2_device v4l2_dev;
	struct video_device *m2m_vdev;
	struct v4l2_m2m_dev *m2m_dev;
	struct mutex m2m_mutex;
};

struct mml_m2m_ctx {
	struct mml_ctx ctx;
};

static int mml_m2m_querycap(struct file *file, void *fh,
			    struct v4l2_capability *cap)
{
	strscpy(cap->driver, MML_M2M_MODULE_NAME, sizeof(cap->driver));
	strscpy(cap->card, MML_M2M_DEVICE_NAME, sizeof(cap->card));
	return 0;
}

static int mml_m2m_enum_fmt_mplane(struct file *file, void *fh,
				   struct v4l2_fmtdesc *f)
{
	return 0;
}

static int mml_m2m_g_fmt_mplane(struct file *file, void *fh,
				struct v4l2_format *f)
{
	return 0;
}

static int mml_m2m_s_fmt_mplane(struct file *file, void *fh,
				struct v4l2_format *f)
{
	return 0;
}

static int mml_m2m_try_fmt_mplane(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	return 0;
}

static int mml_m2m_g_selection(struct file *file, void *fh,
			       struct v4l2_selection *s)
{
	return 0;
}

static int mml_m2m_s_selection(struct file *file, void *fh,
			       struct v4l2_selection *s)
{
	return 0;
}

static const struct v4l2_ioctl_ops mml_m2m_ioctl_ops = {
	.vidioc_querycap		= mml_m2m_querycap,
	.vidioc_enum_fmt_vid_cap	= mml_m2m_enum_fmt_mplane,
	.vidioc_enum_fmt_vid_out	= mml_m2m_enum_fmt_mplane,
	.vidioc_g_fmt_vid_cap_mplane	= mml_m2m_g_fmt_mplane,
	.vidioc_g_fmt_vid_out_mplane	= mml_m2m_g_fmt_mplane,
	.vidioc_s_fmt_vid_cap_mplane	= mml_m2m_s_fmt_mplane,
	.vidioc_s_fmt_vid_out_mplane	= mml_m2m_s_fmt_mplane,
	.vidioc_try_fmt_vid_cap_mplane	= mml_m2m_try_fmt_mplane,
	.vidioc_try_fmt_vid_out_mplane	= mml_m2m_try_fmt_mplane,
	.vidioc_reqbufs			= v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf		= v4l2_m2m_ioctl_querybuf,
	.vidioc_qbuf			= v4l2_m2m_ioctl_qbuf,
	.vidioc_expbuf			= v4l2_m2m_ioctl_expbuf,
	.vidioc_dqbuf			= v4l2_m2m_ioctl_dqbuf,
	.vidioc_create_bufs		= v4l2_m2m_ioctl_create_bufs,
	/* .vidioc_prepare_buf		= v4l2_m2m_ioctl_prepare_buf, */
	.vidioc_streamon		= v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff		= v4l2_m2m_ioctl_streamoff,
	.vidioc_g_selection		= mml_m2m_g_selection,
	.vidioc_s_selection		= mml_m2m_s_selection,
	.vidioc_subscribe_event		= v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event	= v4l2_event_unsubscribe,
};

static int mml_m2m_open(struct file *file)
{
	return 0;
}

static int mml_m2m_release(struct file *file)
{
	return 0;
}

static const struct v4l2_file_operations mml_m2m_fops = {
	.owner		= THIS_MODULE,
	.poll		= v4l2_m2m_fop_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= v4l2_m2m_fop_mmap,
	.open		= mml_m2m_open,
	.release	= mml_m2m_release,
};

static void mml_m2m_device_run(void *priv)
{
}

static const struct v4l2_m2m_ops mml_m2m_ops = {
	.device_run	= mml_m2m_device_run,
};

static int mml_m2m_device_register(struct device *dev, struct mml_v4l2_dev *v4l2_dev)
{
	struct mml_dev *mml = dev_get_drvdata(dev);
	struct video_device *vdev;
	int ret;

	vdev = video_device_alloc();
	if (!vdev) {
		dev_err(dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto err_video_alloc;
	}
	vdev->device_caps = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
	vdev->fops = &mml_m2m_fops;
	vdev->ioctl_ops = &mml_m2m_ioctl_ops;
	vdev->release = video_device_release;
	vdev->lock = &v4l2_dev->m2m_mutex;
	vdev->vfl_dir = VFL_DIR_M2M;
	vdev->v4l2_dev = &v4l2_dev->v4l2_dev;
	snprintf(vdev->name, sizeof(vdev->name), "%s:m2m", MML_M2M_MODULE_NAME);
	video_set_drvdata(vdev, mml);
	v4l2_dev->m2m_vdev = vdev;

	v4l2_dev->m2m_dev = v4l2_m2m_init(&mml_m2m_ops);
	if (IS_ERR(v4l2_dev->m2m_dev)) {
		dev_err(dev, "Failed to initialize v4l2-m2m device\n");
		ret = PTR_ERR(v4l2_dev->m2m_dev);
		goto err_m2m_init;
	}

	ret = video_register_device(vdev, VFL_TYPE_VIDEO, -1);
	if (ret) {
		dev_err(dev, "Failed to register video device\n");
		goto err_register;
	}

	v4l2_info(&v4l2_dev->v4l2_dev, "Driver registered as /dev/video%d",
		  vdev->num);
	return 0;

err_register:
	v4l2_m2m_release(v4l2_dev->m2m_dev);
err_m2m_init:
	video_device_release(vdev);
err_video_alloc:
	return ret;
}

static void mml_m2m_device_unregister(struct mml_v4l2_dev *v4l2_dev)
{
	video_unregister_device(v4l2_dev->m2m_vdev);
	v4l2_m2m_release(v4l2_dev->m2m_dev);
}

struct mml_v4l2_dev *mml_v4l2_dev_create(struct device *dev)
{
	struct mml_v4l2_dev *v4l2_dev;
	int ret;

	v4l2_dev = devm_kzalloc(dev, sizeof(*v4l2_dev), GFP_KERNEL);
	if (!v4l2_dev)
		return ERR_PTR(-ENOMEM);

	mutex_init(&v4l2_dev->m2m_mutex);

	ret = v4l2_device_register(dev, &v4l2_dev->v4l2_dev);
	if (ret) {
		dev_err(dev, "Failed to register v4l2 device\n");
		goto err_free;
	}

	ret = mml_m2m_device_register(dev, v4l2_dev);
	if (ret) {
		v4l2_err(&v4l2_dev->v4l2_dev, "Failed to register m2m device\n");
		goto err_unregister;
	}

	return v4l2_dev;

err_unregister:
	v4l2_device_unregister(&v4l2_dev->v4l2_dev);
err_free:
	devm_kfree(dev, v4l2_dev);
	return ERR_PTR(ret);
}

void mml_v4l2_dev_destroy(struct device *dev, struct mml_v4l2_dev *v4l2_dev)
{
	if (IS_ERR_OR_NULL(v4l2_dev))
		return;

	mml_m2m_device_unregister(v4l2_dev);
	v4l2_device_unregister(&v4l2_dev->v4l2_dev);
	devm_kfree(dev, v4l2_dev);
}

