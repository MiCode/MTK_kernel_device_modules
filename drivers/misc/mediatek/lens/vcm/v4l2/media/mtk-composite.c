// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": %s: " fmt, __func__

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/of_graph.h>

#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-common.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include "mtk-composite.h"

/******************************************************************************
 * Definition
 *****************************************************************************/

#define MTK_COMPOSITE_NAME "mtk-composite-v4l2-2"

static int
fl_async_bound(struct v4l2_async_notifier *notifier,
		 struct v4l2_subdev *subdev,
		 struct v4l2_async_subdev *asd)
{
	struct mtk_composite_v4l2_device *pfdev =
			container_of(notifier->v4l2_dev,
			struct mtk_composite_v4l2_device, v4l2_dev);

	pr_info("%s v4l2 subdev entity name:%s\n", __func__,
		subdev->entity.name);

	/* add for media_create_ancillary_link */
	notifier->sd = &pfdev->nf_sd;
	INIT_LIST_HEAD(&notifier->sd->entity.links);
	notifier->sd->entity.graph_obj.mdev = pfdev->v4l2_dev.mdev;
	pr_info("%s %d\n", __func__, __LINE__);

	return 0;
}

static int fl_probe_complete(struct mtk_composite_v4l2_device *vpfe)
{
	int err;
	struct v4l2_subdev *sd;

	pr_info("%s\n", __func__);

	err = v4l2_device_register_subdev_nodes(&vpfe->v4l2_dev);
	if (err) {
		pr_info("Unable to v4l2_device_register_subdev_nodes\n");
		goto probe_out;
	}

	list_for_each_entry(sd, &vpfe->v4l2_dev.subdevs, list) {
		if (!(sd->flags & V4L2_SUBDEV_FL_HAS_DEVNODE))
			continue;

#if defined(CONFIG_MEDIA_CONTROLLER)
		pr_info("%s v4l2:%s\n", __func__, sd->entity.name);
#endif
	}

	pr_debug("%s -\n", __func__);
	return 0;

probe_out:
	v4l2_device_unregister(&vpfe->v4l2_dev);
	return err;
}

static int fl_async_complete(struct v4l2_async_notifier *notifier)
{
	struct mtk_composite_v4l2_device *pfdev =
		container_of(notifier->v4l2_dev,
			struct mtk_composite_v4l2_device, v4l2_dev);

	pr_info("%s %d\n", __func__, __LINE__);
	return fl_probe_complete(pfdev);
}

static void mtk_composite_unregister_entities(
		struct mtk_composite_v4l2_device *isp)
{
	media_device_unregister(isp->v4l2_dev.mdev);
	v4l2_device_unregister(&isp->v4l2_dev);
}

static const struct v4l2_async_notifier_operations fl_async_notify_ops = {
	.bound = fl_async_bound,
	.complete = fl_async_complete,
};

static int mtk_composite_probe(struct platform_device *dev)
{
	struct mtk_composite_v4l2_device *pfdev;
	int rc = 0;

	pr_info("lens v4l2 probe\n");
	pfdev = devm_kzalloc(&dev->dev, sizeof(*pfdev), GFP_KERNEL);
	if (!pfdev) {
		pr_info("could not allocate memory\n");
		return -ENOMEM;
	}

	pfdev->vdev = video_device_alloc();
	if (WARN_ON(!pfdev->vdev)) {
		rc = -ENOMEM;
		pr_info("failed to allocate video_device\n");
		goto vdec_end;
	}

#if defined(CONFIG_MEDIA_CONTROLLER)
	pfdev->v4l2_dev.mdev = kzalloc(sizeof(struct media_device),
		GFP_KERNEL);
	if (!pfdev->v4l2_dev.mdev) {
		rc = -ENOMEM;
		pr_info("failed to allocate  media_device\n");
		goto mdev_end;
	}
	strscpy(pfdev->v4l2_dev.mdev->model, "mtk_camera_af",
			sizeof(pfdev->v4l2_dev.mdev->model));
	pfdev->v4l2_dev.mdev->dev = &(dev->dev);

	media_device_init(pfdev->v4l2_dev.mdev);

	rc = media_device_register(pfdev->v4l2_dev.mdev);
	if (WARN_ON(rc < 0)) {
		pr_info("failed to register media_device");
		goto mdev_end;
	}

	/* Initialize mutex */
	mutex_init(&pfdev->v4l2_dev.mdev->graph_mutex);
	rc = media_entity_pads_init(&pfdev->vdev->entity, 0, NULL);

	if (WARN_ON(rc < 0)) {
		pr_info("media_entity_pads init failed\n");
		goto mdev_end;
	}
	pfdev->vdev->entity.function = MEDIA_ENT_F_IO_V4L;
#endif

	rc = v4l2_device_register(&dev->dev, &pfdev->v4l2_dev);
	if (rc) {
		pr_info("Unable to register v4l2 device.\n");
		goto mdev_end;
	}
	platform_set_drvdata(dev, pfdev);

	v4l2_async_nf_init(&pfdev->notifier);

	rc = v4l2_async_nf_parse_fwnode_endpoints
		(&dev->dev, &pfdev->notifier, sizeof(struct v4l2_async_subdev), NULL);
	if (rc < 0) {
		pr_info("no lens endpoint\n");
		goto mdev_end;
	}
	pr_debug("asd %p %p %p\n", pfdev->asd[0], pfdev->asd[1],
		pfdev->asd[2]);

	pfdev->notifier.ops = &fl_async_notify_ops;

	rc = v4l2_async_nf_register(&pfdev->v4l2_dev, &pfdev->notifier);
	if (rc) {
		pr_info("Error registering async notifier\n");
		rc = -EINVAL;
		goto mdev_end;
	}

	pr_info("%s: Probe done.\n", __func__);
	return 0;

mdev_end:
	kfree_sensitive(pfdev->vdev);
vdec_end:
	kfree(pfdev);

	return rc;
}

static int mtk_composite_remove(struct platform_device *dev)
{
	struct mtk_composite_v4l2_device *isp = platform_get_drvdata(dev);

	v4l2_async_nf_unregister(&isp->notifier);
	mtk_composite_unregister_entities(isp);

	return 0;
}

static void mtk_composite_shutdown(struct platform_device *dev)
{

}

#ifdef CONFIG_OF
static const struct of_device_id mtk_composite_of_match[] = {
	{.compatible = "mediatek,mtk_composite_v4l2_2"},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_composite_of_match);

#else
static struct platform_device mtk_composite_platform_device[] = {
	{
		.name = MTK_COMPOSITE_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, mtk_composite_platform_device);
#endif

static struct platform_driver mtk_composite_platform_driver = {
	.probe = mtk_composite_probe,
	.remove = mtk_composite_remove,
	.shutdown = mtk_composite_shutdown,
	.driver = {
		   .name = MTK_COMPOSITE_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = mtk_composite_of_match,
#endif
	},
};

static int __init mtk_composite_init(void)
{
	int ret;

	pr_debug("Init start\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&mtk_composite_platform_device);
	if (ret) {
		pr_info("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&mtk_composite_platform_driver);
	if (ret) {
		pr_info("Failed to register platform driver\n");
		return ret;
	}

	pr_debug("Init done\n");

	return 0;
}

static void __exit mtk_composite_exit(void)
{
	platform_driver_unregister(&mtk_composite_platform_driver);
#ifndef CONFIG_OF
	platform_device_unregister(&mtk_composite_platform_device);
#endif
}

late_initcall(mtk_composite_init);
module_exit(mtk_composite_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MTK V4L2 Composite Driver");
