// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/acpi.h>
#include <linux/component.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/gfp.h>
#include <linux/i2c.h>
#include <linux/kdev_t.h>
#include <linux/property.h>
#include <linux/slab.h>

#include <drm/drm_accel.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include <drm/drm_file.h>
#include <drm/drm_modes.h>
#include <drm/drm_print.h>
#include <drm/drm_property.h>
#include <drm/drm_sysfs.h>

#include "drm_internal.h"
#include "drm_crtc_internal.h"

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_log.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_plane.h"
#include "mtk_drm_gem.h"

#include "mtk_drm_ddp_comp_auto.h"
#include "mtk_drm_crtc_auto.h"

static const struct device_type mtk_drm_sysfs_device = {
	.name = "mtk_drm",
};

static bool mtk_drm_get_encoder_enable_status(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_ddp_comp *output_comp = NULL;
	bool encoder_enabled = false;
	int ret = -EINVAL;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (!output_comp) {
		DDPMSG("%s invalid output_comp\n", __func__);
		goto exit;
	}

	ret = mtk_ddp_comp_io_cmd(output_comp, NULL, GET_ENABLE_READY_STATUS, &encoder_enabled);
	if (ret) {
		DDPMSG("%s invalid output_comp\n", __func__);
		goto exit;
	}

exit:
	if (!ret)
		DDPMSG("%s crtc%d output_comp %s encoder_enabled %d\n",
			__func__, drm_crtc_index(&mtk_crtc->base),
			mtk_dump_comp_str(output_comp), encoder_enabled);

	return encoder_enabled;
}

static ssize_t runtime_status_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct mtk_drm_private *private = NULL;
	const struct dev_pm_ops *pm = NULL;
	struct device_driver *driver = NULL;
	struct device *mmsys_dev = NULL;

	if (!device) {
		DDPMSG("%s invalid device\n", __func__);
		goto store_exit;
	}

	private = dev_get_drvdata(device);
	if (!private) {
		DDPMSG("%s invalid private\n", __func__);
		goto store_exit;
	}

	mmsys_dev = private->mmsys_dev;
	if (!mmsys_dev) {
		DDPMSG("%s invalid mmsys_dev\n", __func__);
		goto store_exit;
	}

	DDPMSG("%s device %s priave device %s\n",
		__func__, dev_name(device), dev_name(mmsys_dev));

	driver = mmsys_dev->driver;
	if (!driver) {
		DDPMSG("%s invalid device driver\n", __func__);
		goto store_exit;
	}
	DDPMSG("%s driver 0x%p\n", __func__, driver);

	pm = driver->pm;
	if (!driver->pm) {
		DDPMSG("%s invalid device driver pm\n", __func__);
		goto store_exit;
	}
	DDPMSG("%s pm 0x%p\n", __func__, pm);

	DDPMSG("%s suspend 0x%p resume 0x%p\n",
		__func__, pm->suspend, pm->resume);

	if (sysfs_streq(buf, "suspend") && pm->suspend) {
		DDPMSG("%s %s driver 0x%p pm %p\n", __func__, dev_name(device), driver, pm);
		pm->suspend(device);
	} else if (sysfs_streq(buf, "resume") && pm->resume) {
		pm->resume(device);
	} else {
		DDPMSG("%s %s invalid cmd\n", __func__, dev_name(device));
	}

store_exit:
	return count;
}

static ssize_t runtime_status_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct mtk_drm_private *private = dev_get_drvdata(device);
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	int i;
	bool encoder_status = false, encoder_enabled = false;

	DDPMSG("%s +\n", __func__);

	/* check each CRTC */
	for (i = 0; i < MAX_CRTC; i++) {
		crtc = private->crtc[i];
		if (!crtc)
			continue;
		mtk_crtc = to_mtk_crtc(crtc);
		if (!mtk_crtc)
			continue;

		if (mtk_crtc->is_guest_exclusive_device)
			continue;

		encoder_enabled = mtk_drm_get_encoder_enable_status(mtk_crtc);

		if (i == 0)
			encoder_status = encoder_enabled;
		else if (mtk_crtc->enabled) /* resume need all encoder enabled */
			encoder_status &= encoder_enabled;
		else /* suspend need all encoder disabled */
			encoder_status |= encoder_enabled;

		DDPMSG("%s crtc%d encoder_enabled %d encoder_status %d\n",
			__func__, i, encoder_enabled, encoder_status);
	}

	return sysfs_emit(buf, "%s\n", encoder_status ? "active" : "suspended");
}

static DEVICE_ATTR_RW(runtime_status);

static struct attribute *mtk_drm_dev_attrs[] = {
	&dev_attr_runtime_status.attr,
	NULL
};

static const struct attribute_group mtk_drm_dev_group = {
	.attrs = mtk_drm_dev_attrs,
};

static const struct attribute_group *mtk_drm_dev_groups[] = {
	&mtk_drm_dev_group,
	NULL
};

static void mtk_drm_sysfs_release(struct device *dev)
{
	kfree(dev);
}

int mtk_drm_sysfs_node_add(struct drm_device *dev)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct device *kdev;
	int ret;

	DDPMSG("%s +\n", __func__);

	if (private->kdev)
		return 0;

	kdev = kzalloc(sizeof(*kdev), GFP_KERNEL);
	if (!kdev)
		return -ENOMEM;

	DDPMSG("%s %s parent %s\n",
		__func__,
		dev_name(dev->dev),
		dev->primary ? dev_name(dev->primary->kdev) : "no parent");

	kdev->type = &mtk_drm_sysfs_device;
	kdev->parent = dev->primary ? dev->primary->kdev : NULL;
	kdev->groups = mtk_drm_dev_groups;
	kdev->release = mtk_drm_sysfs_release;
	dev_set_drvdata(kdev, private);

	ret = dev_set_name(kdev, "mtk_drm");
	if (ret) {
		DDPMSG("%s set dev name %s fail!\n", __func__, dev_name(dev->dev));
		goto err_free;
	}

	ret = drm_class_device_register(kdev);
	if (ret) {
		DDPMSG("%s %s class register fail ret %d!\n",
			__func__, dev_name(dev->dev), ret);
		goto err_free;
	}

	DDPMSG("%s %s %s parent %s\n",
		__func__, dev_name(kdev), dev_name(dev->dev), dev_name(kdev->parent));

	private->kdev = kdev;

	DDPMSG("%s -\n", __func__);

	return 0;

err_free:
	DDPMSG("%s -----------\n", __func__);
	put_device(kdev);
	kfree(kdev);
	return ret;
}
