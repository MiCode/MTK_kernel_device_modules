/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#ifndef __SCP_AUDIO_DRIVER_H__
#define __SCP_AUDIO_DRIVER_H__

#include <linux/cdev.h>
struct scp_audio_priv {
	u64 feature_set;
	bool inited;
	bool clock_lock;

	struct workqueue_struct *workq;
	struct wait_queue_head waitq;
};

extern struct device_attribute dev_attr_audio_ipi_test;
extern int adsp_qos_probe(struct platform_device *pdev);
extern void adsp_set_scene_bw(struct platform_device *pdev);
extern int adsp_mem_device_probe(struct platform_device *pdev);

#endif  /* __SCP_AUDIO_DRIVER_H__ */
