// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Nick.wen <nick.wen@mediatek.com>
 */

#include <linux/io.h>
#include <linux/poll.h>
#include <linux/iopoll.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/notifier.h>
#include <linux/timekeeping.h>
#include <linux/remoteproc/mtk_ccu.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <soc/mediatek/mmdvfs_v3.h>

#include <mt-plat/mtk-vmm-notifier.h>


#define ISP_LOGI(fmt, args...) \
	pr_notice("%s(): " fmt "\n", \
		__func__, ##args)
#define ISP_LOGE(fmt, args...) \
	pr_notice("error %s(),%d: " fmt "\n", \
		__func__, __LINE__, ##args)
#define ISP_LOGF(fmt, args...) \
	pr_notice("fatal %s(),%d: " fmt "\n", \
		__func__, __LINE__, ##args)

// Time setting
#define POLL_DELAY_US				(1)
#define TIMEOUT_500US				(500)
#define TIMEOUT_1000US				(1000)
#define TIMEOUT_100000US			(100000)

// HWCCF setting
#define HW_CCF_AP_VOTER_BIT			(0)
#define MM_HW_CCF_BASE				(0x31B00000)
#define HW_CCF_BACKUP2_DONE			(MM_HW_CCF_BASE + 0x144C)
#define HW_CCF_XPU0_BACKUP2_SET		(MM_HW_CCF_BASE + 0x238)
#define HW_CCF_BACKUP2_SET_STATUS	(MM_HW_CCF_BASE + 0x148C)
#define HW_CCF_XPU0_BACKUP2_CLR		(MM_HW_CCF_BASE + 0x23C)
#define HW_CCF_BACKUP2_CLR_STATUS	(MM_HW_CCF_BASE + 0x1490)
#define HW_CCF_BACKUP2_ENABLE		(MM_HW_CCF_BASE+0x1440)
#define HW_CCF_BACKUP2_STATUS		(MM_HW_CCF_BASE+0x1444)
#define HW_CCF_BACKUP2_STATUS_DBG	(MM_HW_CCF_BASE+0x1448)

enum POWER_DOMAIN_ID {
	PD_ISP_TRAW,	/*0*/
	PD_ISP_DIP,
	PD_ISP_MAIN,
	PD_ISP_VCORE,
	PD_ISP_WPE_EIS,
	PD_ISP_WPE_TNR,	/*5*/
	PD_ISP_WPE_LITE,
	PD_VDE0,
	PD_VDE1,
	PD_CAM_MRAW,
	PD_CAM_RAWA,	/*10*/
	PD_CAM_RAWB,
	PD_CAM_RAWC,
	PD_CAM_RMSA,
	PD_CAM_RMSB,
	PD_CAM_RMSC,	/*15*/
	PD_CAM_MAIN,
	PD_CAM_VCORE,
	PD_NUM			/*18*/
};

struct vmm_notifier_data {
	struct notifier_block notifier;
	u32 pd_id;
	struct clk *clk_avs;
	const char *avs_name;
	void __iomem *base;
	unsigned long timestamp;
};

static struct vmm_notifier_data global_data[PD_NUM];
struct mutex ctrl_mutex;
static int vmm_user_counter;

static void vmm_notifier_timeout_debug_dump(void)
{
	ISP_LOGI("[%s]: set(0x%x),clr(0x%x),en(0x%x),st(0x%x),std(0x%x),done(0x%x),set_s(0x%x),clr_s(0x%x)\n",
		__func__,
		readl_relaxed(ioremap(HW_CCF_XPU0_BACKUP2_SET, 4)),
		readl_relaxed(ioremap(HW_CCF_XPU0_BACKUP2_CLR, 4)),
		readl_relaxed(ioremap(HW_CCF_BACKUP2_ENABLE, 4)),
		readl_relaxed(ioremap(HW_CCF_BACKUP2_STATUS, 4)),
		readl_relaxed(ioremap(HW_CCF_BACKUP2_STATUS_DBG, 4)),
		readl_relaxed(ioremap(HW_CCF_BACKUP2_DONE, 4)),
		readl_relaxed(ioremap(HW_CCF_BACKUP2_SET_STATUS, 4)),
		readl_relaxed(ioremap(HW_CCF_BACKUP2_CLR_STATUS, 4)));
}

static void vmm_locked_hwccf_ctrl(bool enable)
{
	void __iomem *hwccf_done = 0;
	void __iomem *ctrl_reg = 0;
	void __iomem *hwccf_ctrl_status = 0;
	unsigned int val = 0;
	int tmp = 0;

	hwccf_done = ioremap(HW_CCF_BACKUP2_DONE, 4);
	ctrl_reg = (enable) ? ioremap(HW_CCF_XPU0_BACKUP2_SET, 4) : ioremap(HW_CCF_XPU0_BACKUP2_CLR, 4);
	hwccf_ctrl_status = (enable) ? ioremap(HW_CCF_BACKUP2_SET_STATUS, 4) : ioremap(HW_CCF_BACKUP2_CLR_STATUS, 4);
	val = (enable) ? BIT(HW_CCF_AP_VOTER_BIT) : 0;

	// polling done
	if (readl_poll_timeout_atomic
		(hwccf_done, tmp, (tmp & BIT(HW_CCF_AP_VOTER_BIT)) == BIT(HW_CCF_AP_VOTER_BIT),
		POLL_DELAY_US, TIMEOUT_1000US) < 0)
		vmm_notifier_timeout_debug_dump();

	// set/clr
	writel_relaxed(BIT(HW_CCF_AP_VOTER_BIT), ctrl_reg);

	// polling
	if (readl_poll_timeout_atomic
		(ctrl_reg, tmp, (tmp & BIT(HW_CCF_AP_VOTER_BIT)) == val, POLL_DELAY_US, TIMEOUT_1000US) < 0)
		vmm_notifier_timeout_debug_dump();

	// polling done
	if (readl_poll_timeout_atomic
		(hwccf_done, tmp, (tmp & BIT(HW_CCF_AP_VOTER_BIT)) == BIT(HW_CCF_AP_VOTER_BIT),
		POLL_DELAY_US, TIMEOUT_1000US) < 0)
		vmm_notifier_timeout_debug_dump();

	// wait for current done
	if (readl_poll_timeout_atomic
		(hwccf_ctrl_status, tmp, (tmp & BIT(HW_CCF_AP_VOTER_BIT)) == 0, POLL_DELAY_US, TIMEOUT_1000US) < 0)
		vmm_notifier_timeout_debug_dump();
}

static int vmm_locked_buck_ctrl(bool enable)
{
	int pre_cnt = vmm_user_counter;

	if (enable)
		vmm_user_counter++;
	else
		vmm_user_counter--;

	if (pre_cnt == 0 && vmm_user_counter == 1)
		vmm_locked_hwccf_ctrl(true);
	else if (pre_cnt == 1 && vmm_user_counter == 0)
		vmm_locked_hwccf_ctrl(false);

	return 0;
}

static int mtk_camera_pd_callback(struct notifier_block *nb,
		unsigned long flags, void *data)
{
	int ret = 0;
	struct vmm_notifier_data *priv;

	mutex_lock(&ctrl_mutex);
	priv = container_of(nb, struct vmm_notifier_data, notifier);

	if (flags == GENPD_NOTIFY_PRE_ON)
		ret = vmm_locked_buck_ctrl(true);
	else if (flags == GENPD_NOTIFY_OFF)
		ret = vmm_locked_buck_ctrl(false);

	mutex_unlock(&ctrl_mutex);

	return ret;
}
static int vmm_notifier_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	u32 pd_id;
	struct vmm_notifier_data *data;

	ret = of_property_read_u32(dev->of_node, "pd-id", &pd_id);
	if (ret) {
		ISP_LOGE("vmm property read fail(%d)", ret);
		return -ENODEV;
	}

	if (pd_id >= PD_NUM) {
		ISP_LOGE("vmm invalid pd_id in dts(%u)", pd_id);
		return -ENODEV;
	}
	data = &global_data[pd_id];

	ISP_LOGI("[%s][%d] pd_id[%d] start\n", __func__, __LINE__, pd_id);

	vmm_locked_buck_ctrl(true);
	pm_runtime_enable(dev);
	data->notifier.notifier_call = mtk_camera_pd_callback;
	data->pd_id = pd_id;
	ret = dev_pm_genpd_add_notifier(dev, &data->notifier);
	if (ret)
		ISP_LOGE("vmm gen pd add notifier fail(%d)\n", ret);
	ISP_LOGI("[%s][%d] pd_id[%d] end\n", __func__, __LINE__, pd_id);
	return 0;
}

static const struct of_device_id of_vmm_notifier_match_tbl[] = {
	{
		.compatible = "mediatek,vmm_notifier_mt6991",
	},
	{}
};

static struct platform_driver drv_vmm_notifier = {
	.probe = vmm_notifier_probe,
	.driver = {
		.name = "mtk-vmm-notifier-mt6991",
		.of_match_table = of_vmm_notifier_match_tbl,
	},
};

static int __init mtk_vmm_notifier_init(void)
{
	s32 status;

	mutex_init(&ctrl_mutex);
	ISP_LOGI("[%s][%d] start\n", __func__, __LINE__);
	vmm_locked_buck_ctrl(true);
	status = platform_driver_register(&drv_vmm_notifier);
	if (status) {
		pr_notice("Failed to register VMM dbg driver(%d)\n", status);
		return -ENODEV;
	}
	vmm_locked_buck_ctrl(false);
	ISP_LOGI("[%s][%d] end\n", __func__, __LINE__);

	return 0;
}

static void __exit mtk_vmm_notifier_exit(void)
{
	platform_driver_unregister(&drv_vmm_notifier);
}

int vmm_isp_ctrl_notify(int openIsp)
{
	return 0;
}
EXPORT_SYMBOL_GPL(vmm_isp_ctrl_notify);

int mtk_vmm_notify_ut_ctrl(const char *val, const struct kernel_param *kp)
{
	int ctrl;
	bool st;
	int ret;

	ret = sscanf(val, "%u ", &ctrl);
	ISP_LOGI("[%s][%d] ctrl[%u]\n", __func__, __LINE__, ctrl);
	st = (ctrl == 1);
	vmm_locked_buck_ctrl(st);

	return 0;
}

static const struct kernel_param_ops vmm_notify_ut_ctrl_ops = {
	.set = mtk_vmm_notify_ut_ctrl,
};

module_param_cb(vmm_notify_ut_ctrl, &vmm_notify_ut_ctrl_ops, NULL, 0644);
MODULE_PARM_DESC(vmm_notify_ut_ctrl, "vmm_notify_ut_ctrl");

module_init(mtk_vmm_notifier_init);
module_exit(mtk_vmm_notifier_exit);
MODULE_DESCRIPTION("MTK VMM notifier driver");
MODULE_AUTHOR("Nick Wen <nick.wen@mediatek.com>");
MODULE_SOFTDEP("pre:mtk-scpsys");
MODULE_SOFTDEP("pre:mtk-scpsys-mt6991");
MODULE_SOFTDEP("pre:mtk-scpsys.ko");
MODULE_SOFTDEP("pre:mtk-scpsys-mt6991.ko");
MODULE_LICENSE("GPL");
