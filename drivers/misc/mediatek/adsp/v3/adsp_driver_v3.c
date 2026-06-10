// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include "adsp_mbox.h"
#include "adsp_platform.h"
#include "adsp_platform_driver.h"
#include "adsp_core.h"
#include "adsp_clk_v3.h"
#include "adsp_driver_v3.h"
#include "adsp_qos.h"
#include "adsp_ipic.h"
#include "mtk-afe-external.h"
#include "adsp_dbg_dump.h"

const struct adspsys_description mt6993_adspsys_desc = {
	.platform_name = "mt6993",
	.version = 3,
	.semaphore_ways = 3,
	.semaphore_ctrl = 2,
	.semaphore_retry = 5000,
	.axibus_idle_val = 0x0,
	.mtcmos_ao_ctrl = 0,
	.slc_bw = 100,
	.slc_dma_size = 2,
};

const struct adsp_core_description mt6993_adsp_c0_desc = {
	.id = 0,
	.name = "adsp_0",
	.sharedmems = {
		[ADSP_SHAREDMEM_TIMESYNC] = {0xB00, 0x0018},
	},
	.ops = {
		.initialize = adsp_core0_init,
		.after_bootup = adsp_after_bootup,
	}
};

const struct adsp_core_description mt6993_adsp_c1_desc = {
	.id = 1,
	.name = "adsp_1",
	.sharedmems = {},
	.ops = {
		.initialize = adsp_core1_init,
		.after_bootup = adsp_after_bootup,
	}
};

static const struct of_device_id adspsys_of_ids[] = {
	{ .compatible = "mediatek,mt6993-adspsys", .data = &mt6993_adspsys_desc},
	{}
};

static const struct of_device_id adsp_core_of_ids[] = {
	{ .compatible = "mediatek,mt6993-adsp_core_0", .data = &mt6993_adsp_c0_desc},
	{ .compatible = "mediatek,mt6993-adsp_core_1", .data = &mt6993_adsp_c1_desc},
	{}
};

static const struct of_device_id adsp_qos_scene_of_ids[] = {
	{ .compatible = "mediatek,mt6993-audio-dsp-hrt-bw"},
	{},
};

static int adspsys_drv_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;
	struct adspsys_priv *adspsys;

	/* create private data */
	adspsys = devm_kzalloc(dev, sizeof(*adspsys), GFP_KERNEL);
	if (!adspsys) {
		dev_err(dev, "create adspsys instance fail!");
		return -ENOMEM;
	}

	match = of_match_node(adspsys_of_ids, dev->of_node);
	if (!match) {
		dev_err(dev, "no match for this dev node");
		return -ENODEV;
	}

	adspsys->desc = (struct adspsys_description *)match->data;
	adspsys->dev = dev;

	/* get resource from platform_device */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cfg");
	if (!res) {
		dev_err(dev, "no cfg resource found!");
		return -ENODEV;
	}
	adspsys->cfg = devm_ioremap_resource(dev, res);
	adspsys->cfg_size = resource_size(res);

	/* for dump */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cfg2");
	if (!res) {
		dev_err(dev, "no cfg2 resource found!");
		return -ENODEV;
	}
	adspsys->cfg2 = devm_ioremap_resource(dev, res);
	adspsys->cfg2_size = resource_size(res);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cfg3");
	if (!res)
		dev_err(dev, "no cfg3 resource found!");
	else {
		adspsys->cfg3 = devm_ioremap_resource(dev, res);
		adspsys->cfg3_size = resource_size(res);
	}

	/* property read from dts*/
	READ_U32_PROPERTY_DEFAULT(dev->of_node, "core-num",
				  &adspsys->num_cores, 0);
	READ_U32_PROPERTY_DEFAULT(dev->of_node, "slp-prot-ctrl",
				  &adspsys->slp_prot_ctrl, 0);
	READ_U32_PROPERTY_DEFAULT(dev->of_node, "sram-sleep-mode-ctrl",
				  &adspsys->sram_sleep_mode_ctrl, 0);
	READ_U32_PROPERTY_DEFAULT(dev->of_node, "system-l2sram",
				  &adspsys->system_l2sram, 0);
#if IS_ENABLED(CONFIG_MTK_SLBC)
	READ_U32_PROPERTY_DEFAULT(dev->of_node, "adsp-slc-enable",
				  &adspsys->slc_enable, 0);
#endif
	ret = adsp_clk_probe(pdev, &adspsys->clk_ops);
	if (ret) {
		dev_err(dev ,"clk probe fail, %d", ret);
		return ret;
	}

	ret = adsp_mem_device_probe(pdev);
	if (ret) {
		dev_err(dev ,"memory probe fail, %d", ret);
		return ret;
	}

	ret = adsp_mbox_probe(pdev);
	if (ret) {
		dev_err(dev ,"mbox probe fail, %d", ret);
		return ret;
	}
#ifndef CFG_FPGA
	/* adsp bus probe */
	if (adsp_qos_probe(pdev))
		dev_warn(dev ,"qos probe fail %d, continue", ret);

	/* adsp ipic probe */
	if (adsp_ipic_probe(pdev))
		dev_warn(dev ,"ipic probe fail %d, continue", ret);
#endif
	/* register as syscore_device, not to be turned off when suspend */
	dev_pm_syscore_device(dev, true);

	adsp_hardware_init(adspsys);

	register_adspsys(adspsys);

	switch_adsp_power(true);

	dev_info(dev, "%s success done", __func__);
	return 0;
}

static void adspsys_drv_remove(struct platform_device *pdev)
{
}

static int adsp_core_drv_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct adsp_priv *pdata;
	const struct adsp_core_description *desc;
	const struct of_device_id *match;
	struct of_phandle_args spec;
	u64 system_info[2];

	/* create private data */
	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "create pdata instance fail!");
		return -ENOMEM;
	}

	match = of_match_node(adsp_core_of_ids, dev->of_node);
	if (!match) {
		dev_err(dev, "no match for this dev node");
		return -ENODEV;
	}

	desc = (struct adsp_core_description *)match->data;

	pdata->id = desc->id;
	pdata->name = desc->name;
	pdata->ops = &desc->ops;
	pdata->mapping_table = desc->sharedmems;

	pdata->dev = dev;
	init_completion(&pdata->done);

	/* Since the RV only has L2 SRAM and no TCM present,
	   we will map the ITCM to the same region as L2 SRAM */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sram");
	if (!res) {
		dev_err(dev, "no sram resource found!");
		return -ENODEV;
	}
	pdata->itcm = devm_ioremap_resource(dev, res);
	pdata->itcm_size = resource_size(res);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "share");
	if (!res) {
		dev_err(dev, "no share resource found!");
		return -ENODEV;
	}
	pdata->dtcm = devm_ioremap_resource(dev, res);
	pdata->dtcm_size = resource_size(res);

	/* Let has_system_l2sram() return true */
	pdata->l2sram = pdata->dtcm;
	pdata->l2sram_size = pdata->dtcm_size;

	pdata->irq[ADSP_IRQ_IPC_ID].seq = platform_get_irq_byname(pdev, "sysirq");
	pdata->irq[ADSP_IRQ_IPC_ID].clear_irq = adsp_mt_clr_sysirq;
	pdata->irq[ADSP_IRQ_WDT_ID].seq = platform_get_irq_byname(pdev, "wdt");
	pdata->irq[ADSP_IRQ_WDT_ID].clear_irq = adsp_mt_disable_wdt;
	pdata->irq[ADSP_IRQ_AUDIO_ID].seq = platform_get_irq_byname(pdev, "audioirq");
	pdata->irq[ADSP_IRQ_AUDIO_ID].clear_irq = adsp_mt_clr_auidoirq;

	of_property_read_u64_array(dev->of_node, "system", system_info, 2);
	pdata->sysram_phys = (phys_addr_t)system_info[0];
	pdata->sysram_size = (size_t)system_info[1];

	if (pdata->sysram_phys == 0 || pdata->sysram_size == 0)
		return -ENODEV;
	pdata->sysram = ioremap_wc(pdata->sysram_phys, pdata->sysram_size);

	READ_U64_PROPERTY_DEFAULT(dev->of_node, "feature-control-bits",
				  &pdata->feature_set, 0xF);

	READ_U32_PROPERTY_DEFAULT(dev->of_node, "adsp-mbrain-enable",
				  &pdata->mbrain_enable, 0);

	/* mailbox channel parsing */
	if (of_parse_phandle_with_args(dev->of_node, "mboxes",
				       "#mbox-cells", 0, &spec)) {
		dev_dbg(dev, "%s: can't parse \"mboxes\" property\n", __func__);
		return -ENODEV;
	}
	pdata->send_mbox = get_adsp_mbox_pin_send(spec.args[0]);

	if (of_parse_phandle_with_args(dev->of_node, "mboxes",
				       "#mbox-cells", 1, &spec)) {
		dev_dbg(dev, "%s: can't parse \"mboxes\" property\n", __func__);
		return -ENODEV;
	}
	pdata->recv_mbox = get_adsp_mbox_pin_recv(spec.args[0]);

	/* add to adsp_core list */
	register_adsp_core(pdata);

	pr_info("%s, id:%d success\n", __func__, pdata->id);
	return ret;
}

static void adsp_core_drv_remove(struct platform_device *pdev)
{
}

static int adsp_qos_scene_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;

	match = of_match_node(adsp_qos_scene_of_ids, dev->of_node);
	if (match)
		adsp_set_scene_bw(pdev);
	else
		pr_info("%s() no qos scene supported\n", __func__);

	return 0;
}

static void adsp_qos_scene_remove(struct platform_device *pdev)
{
}

static struct platform_driver adspsys_driver = {
	.probe = adspsys_drv_probe,
	.remove = adspsys_drv_remove,
	.driver = {
		.name = "adspsys",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = adspsys_of_ids,
#endif
	},
};

static struct platform_driver adsp_core0_driver = {
	.probe = adsp_core_drv_probe,
	.remove = adsp_core_drv_remove,
	.driver = {
		.name = "adsp-core0",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = adsp_core_of_ids,
#endif
	},
};

static struct platform_driver adsp_core1_driver = {
	.probe = adsp_core_drv_probe,
	.remove = adsp_core_drv_remove,
	.driver = {
		.name = "adsp-core1",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = adsp_core_of_ids,
#endif
	},
};

static struct platform_driver adsp_qos_scene_driver = {
	.probe = adsp_qos_scene_probe,
	.remove = adsp_qos_scene_remove,
	.driver = {
		.name = "audio-dsp-hrt-bw",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = adsp_qos_scene_of_ids,
#endif
	},
};

static struct platform_driver * const drivers[] = {
	&adspsys_driver,
	&adsp_core0_driver,
	&adsp_core1_driver,
	&adsp_qos_scene_driver,
};

int notify_adsp_semaphore_event(struct notifier_block *nb,
				unsigned long event, void *v)
{
	int status = NOTIFY_DONE;

	if (event == NOTIFIER_ADSP_3WAY_SEMAPHORE_GET) {
		status = (get_adsp_semaphore(SEMA_AUDIOREG) == ADSP_OK) ?
			 NOTIFY_STOP : NOTIFY_BAD;
	} else if (event == NOTIFIER_ADSP_3WAY_SEMAPHORE_RELEASE) {
		release_adsp_semaphore(SEMA_AUDIOREG);
		status = NOTIFY_STOP;
	}

	return status;
}

static struct notifier_block adsp_semaphore_init_notifier = {
	.notifier_call = notify_adsp_semaphore_event,
};

/*
 * driver initialization entry point
 */
static int __init platform_adsp_init(void)
{
	int ret = 0;

	ret = platform_register_drivers(drivers, ARRAY_SIZE(drivers));
	if (ret)
		return ret;

	register_3way_semaphore_notifier(&adsp_semaphore_init_notifier);
	adsp_system_bootup();
	return 0;
}

static void __exit platform_adsp_exit(void)
{
	unregister_3way_semaphore_notifier(&adsp_semaphore_init_notifier);
	platform_unregister_drivers(drivers, ARRAY_SIZE(drivers));
	pr_info("[ADSP] platform-adsp Exit.\n");
}

module_init(platform_adsp_init);
module_exit(platform_adsp_exit);

MODULE_AUTHOR("Chien-Wei Hsu <Chien-Wei.Hsu@mediatek.com>");
MODULE_DESCRIPTION("MTK AUDIO DSP PLATFORM Device Driver");
MODULE_LICENSE("GPL v2");
