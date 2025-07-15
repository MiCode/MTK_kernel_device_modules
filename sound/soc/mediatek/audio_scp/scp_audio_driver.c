// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/workqueue.h>
#include "adsp_feature_define.h"
#include "adsp_helper.h"
#include "audio_mbox.h"

#include "scp_audio_driver.h"
#include "scp_audio_logger.h"
#include "scp_audio_ipi.h"
#include "scp_audio_fs.h"
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
#include "scp_ipi_pin.h"
#endif

static struct scp_audio_priv scp_audio;
static int _scp_audio_clock_control(bool lock, bool wait);

static bool get_audio_clock_status(void)
{
	unsigned long spin_flags;
	bool lock;

	spin_lock_irqsave(&scp_audio.lock, spin_flags);
	lock = scp_audio.clock_lock;
	spin_unlock_irqrestore(&scp_audio.lock, spin_flags);

	return lock;
}

static void set_audio_clock_status(bool lock)
{
	unsigned long spin_flags;

	spin_lock_irqsave(&scp_audio.lock, spin_flags);
	scp_audio.clock_lock = lock;
	spin_unlock_irqrestore(&scp_audio.lock, spin_flags);
}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
/* SCP reboot */
static int scp_audio_ctrl_event_receive(
	struct notifier_block *this,
	unsigned long event,
	void *ptr)
{
	switch (event) {
	case SCP_EVENT_STOP:
		pr_info("%s(), SCP_EVENT_STOP\n", __func__);
		reset_adsp_logger_status();
		set_audio_clock_status(false);
		break;
	case SCP_EVENT_READY:
		pr_info("%s(), SCP_EVENT_READY\n", __func__);
		set_audio_clock_status(false);
		wake_up(&scp_audio.waitq);
		scp_audio_logger_init_message();
		if (is_adsp_feature_in_active()) {
			pr_info("%s(), sync lock status with SCP\n", __func__);
			_scp_audio_clock_control(true, false);
		}
		break;
	default:
		pr_info("%s(), event %lu err", __func__, event);
	}
	return 0;
}

static struct notifier_block scp_audio_ctrl_notifier = {
	.notifier_call = scp_audio_ctrl_event_receive,
};
#endif

static int _scp_audio_clock_control(bool lock, bool wait)
{
	int ret = 0;
	int value;
	ktime_t start = ktime_get();
	s64 us = ktime_to_us(start);
	bool clock_status = get_audio_clock_status();

	if (!scp_audio.inited)
		return -EINVAL;

	value = lock ? 0x55 : 0x66;
	if (!clock_status & lock || clock_status & !lock) {

		if (wait && !wait_event_timeout(scp_audio.waitq, is_scp_audio_ready(),
		    msecs_to_jiffies(2000))) {
			ret = -ENODEV;
			goto ERROR;
		}

		if (!get_audio_clock_status() && !lock) {
			ret = -EAGAIN;
			goto ERROR;
		}

		ret = scp_push_message(SCP_AUDIO_IPI_CLOCK_LOCK, &value, sizeof(value), 2000, 0);
		if (ret != SCP_IPI_DONE) {
			ret = -EPIPE;
			goto ERROR;
		}
		pr_info("%s, notify lock(%d)\n", __func__, lock);
		set_audio_clock_status(lock);
	}
	return 0;

ERROR:
	pr_warn("%s() fail, lock(%d) ret(%d), start at [%lld.%lld]",
		__func__, lock, ret, (us / USEC_PER_SEC), (us % USEC_PER_SEC));
	return ret;
}

static int scp_audio_lock_clk_source(void)
{
	return _scp_audio_clock_control(true, true);
}

static int scp_audio_unlock_clk_source(void)
{
	return _scp_audio_clock_control(false, false);
}

static int scp_audio_drv_probe(struct platform_device *pdev)
{
	int ret = -1;
	struct scp_audio_priv *pdata = &scp_audio;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	of_property_read_u64(pdev->dev.of_node, "feature-control-bits", &pdata->feature_set);

	pdata->clock_lock = false;
	spin_lock_init(&pdata->lock);
	pdata->workq = alloc_workqueue("scp_audio_wq", WORK_CPU_UNBOUND | WQ_HIGHPRI, 0);
	init_waitqueue_head(&pdata->waitq);
	ret = init_adsp_feature_control(SCP_A_ID, pdata->feature_set, 1100,
		   pdata->workq,
		   scp_audio_unlock_clk_source,
		   scp_audio_lock_clk_source);
	if (ret)
		pr_info("%s, init adsp feature control fail (ret=%d)\n", __func__, ret);
	scp_A_register_notify(&scp_audio_ctrl_notifier);

	/* register misc driver for ioctl and debug */
	ret = misc_register(&scp_audio_fs_mdev);
	if (ret) {
		pr_info("%s, cannot register misc device\n", __func__);
		goto EXIT;
	}

	ret = device_create_file(scp_audio_fs_mdev.this_device, &dev_attr_audio_ipi_test);
	if (ret) {
		pr_info("%s, cannot create dev_attr_audio_ipi_test\n", __func__);
		goto EXIT;
	}

	ret = audio_mbox_init(pdev);
	if (ret)
		pr_info("%s, audio mbox init fail, %d\n", __func__, ret);

	/* audio reserved memory */
	pr_notice("%s() adsp_reserve_memory_ioremap\n", __func__);
	ret = adsp_mem_device_probe(pdev);
	if (ret) {
		pr_notice("%s(), memory probe fail, %d\n", __func__, ret);
		goto EXIT;
	}

	/* scp audio logger */
	pr_notice("%s() scp_audio_logger\n", __func__);
	ret = scp_audio_logger_init(pdev);
	if (ret) {
		pr_info("%s, init scp audio logger fail\n", __func__);
		goto EXIT;
	}

	ret = scp_audio_debug_cmds_init();
	if (ret) {
		pr_info("%s, init scp audio dbg fail\n", __func__);
		goto EXIT;
	}

#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct dentry *file = NULL;

	file = debugfs_create_file("audiodsp0", S_IFREG | 0644, NULL, NULL, &scp_audio_debug_ops);
	if (!file) {
		pr_info("%s, create debug ops fail!\n", __func__);
		ret = -1;
		goto EXIT;
	}
#endif

	/* adsp bus probe */
	ret = adsp_qos_probe(pdev);
	if (ret) {
		pr_warn("%s(), qos probe fail, %d\n", __func__, ret);
		goto EXIT;
	}

	pdata->inited = true;
#else
	ret = 0;
#endif /* CONFIG_MTK_TINYSYS_SCP_SUPPORT */

EXIT:
	pr_info("%s, done ret:%d", __func__, ret);
	return ret;
}
static const struct of_device_id scp_qos_scene_of_ids[] = {
	{ .compatible = "mediatek,mt6858-audio-dsp-hrt-bw"},
	{},
};

static int scp_qos_scene_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;

	match = of_match_node(scp_qos_scene_of_ids, dev->of_node);
	if (match)
		adsp_set_scene_bw(pdev);
	else
		pr_info("%s() no qos scene supported\n", __func__);

	pr_info("%s, done", __func__);
	return 0;
}

static void scp_qos_scene_remove(struct platform_device *pdev)
{
	pr_info("%s, remove", __func__);
}

static struct platform_driver scp_qos_scene_driver = {
	.probe = scp_qos_scene_probe,
	.remove = scp_qos_scene_remove,
	.driver = {
		.name = "audio-dsp-in-scp-hrt-bw",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = scp_qos_scene_of_ids,
#endif
	},
};

static const struct of_device_id scp_audio_core_dt_match[] = {
	{ .compatible = "mediatek,scp-audio-core", },
	{},
};
MODULE_DEVICE_TABLE(of, scp_audio_core_dt_match);

static struct platform_driver scp_audio_core_driver = {
	.driver = {
		   .name = "scp-audio-core",
		   .owner = THIS_MODULE,
		   .of_match_table = scp_audio_core_dt_match,
	},
	.probe = scp_audio_drv_probe,
};

static struct platform_driver * const drivers[] = {
	&scp_audio_core_driver,
	&scp_qos_scene_driver,
};

static int __init scp_audio_driver_init(void)
{
	int ret = platform_register_drivers(drivers, ARRAY_SIZE(drivers));
	return ret;
}

static void __exit scp_audio_driver_exit(void)
{
	platform_unregister_drivers(drivers, ARRAY_SIZE(drivers));
}

module_init(scp_audio_driver_init);
module_exit(scp_audio_driver_exit);

MODULE_DESCRIPTION("Mediatek common driver for scp audio core");
MODULE_LICENSE("GPL");
