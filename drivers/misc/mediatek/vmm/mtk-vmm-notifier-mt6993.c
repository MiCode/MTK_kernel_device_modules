// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Eric Chien <eric.chien@mediatek.com>
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
#include <linux/delay.h>
#include <linux/clk.h>
#include <soc/mediatek/mmdvfs_v3.h>

#include <mt-plat/mtk-vmm-notifier.h>

#include "clk-mtk.h"
#if IS_ENABLED(CONFIG_MTK_HWCCF)
#include "hwccf_provider.h"
#include "hwccf_provider_data.h"
#include "clkchk.h"
#endif

#define ISP_LOGI(fmt, args...) \
	pr_notice("%s(): " fmt "\n", \
		__func__, ##args)
#define ISP_LOGE(fmt, args...) \
	pr_notice("error %s(),%d: " fmt "\n", \
		__func__, __LINE__, ##args)
#define ISP_LOGF(fmt, args...) \
	pr_notice("fatal %s(),%d: " fmt "\n", \
		__func__, __LINE__, ##args)

#define SEL_MASK								(0xff)
// Time setting
#define POLL_DELAY_US						(1)
#define TIMEOUT_500US						(500)
#define TIMEOUT_1000US						(1000)
#define TIMEOUT_100000US					(100000)

// HWCCF setting
#define VMM_BUCK_SWITCH						(10)
#define HW_CCF_AP_VOTER_BIT					(11)
#define HW_CCF_CAM_SEL_VOTER_BIT			(14)
#define HW_CCF_IMG_SEL_VOTER_BIT			(15)
#define VMM_DBG_EN_BIT						(18)
#define VMM_DBG_FORCE_BUCK_ON_BIT			(19)
#define VMM_DBG_FORCE_BUCK_OFF_BIT			(20)

#define GET_SEL_COUNT(value, sel) (((value) >> ((sel) * 8)) & SEL_MASK)

#define ENCODE_USER_VOTE(value, sel, newCount) \
	((value) = ((value) & ~(SEL_MASK << ((sel) * 8))) | ((newCount) << ((sel) * 8)))

#define CHECK_CVFS_VOTE_OVERFLOW(vote, currVal) \
	(((vote) > 0 && (currVal) == SEL_MASK) ? CVFS_POSITIVE_OVERFLOW : \
	(((vote) < 0 && (currVal) == 0) ? CVFS_NEGATIVE_OVERFLOW : 0))

#define CHECK_CVFS_VOTE_TOGGLE(vote, currVal) \
	(((vote) > 0 && (currVal) == 0) ? CVFS_ENABLE_TOGGLE : \
	(((vote) < 0 && (currVal) == 1) ? CVFS_DISABLE_TOGGLE : 0))

struct mutex ctrl_mutex;
static int vmm_user_counter;

struct mutex cvfs_mutex;
typedef struct {
	uint32_t sumSEL;
	uint32_t user_count[VMM_CVFS_USR_MAX];
} CVFSCounter;

CVFSCounter cvfsCNT;

int update_cvfs_table(CVFSCounter *cnts, enum VMM_CVFS_USR_ID usrID, enum VMM_CVFS_SEL_ID selID, int8_t vote)
{
	int ret = 0;
	uint8_t currVal = 0;

	if (cnts == NULL) {
		ret = CVFS_NULL_POINTER;
		goto updateEnd;
	}

	if (usrID < VMM_CVFS_USR_START || usrID >= VMM_CVFS_USR_MAX ||
		selID < VMM_CVFS_SEL_ID_START || selID >= VMM_CVFS_SEL_ID_END) {
		ret = CVFS_INVALID_PARAM;
		goto updateEnd;
	}

	/* update UsrTable */
	currVal = GET_SEL_COUNT(cnts->user_count[usrID], selID);
	ret = CHECK_CVFS_VOTE_OVERFLOW(vote, currVal);
	if (ret != 0)
		goto updateEnd;

	ENCODE_USER_VOTE(cnts->user_count[usrID], selID, currVal+vote);

	/* update SumTable */
	currVal = GET_SEL_COUNT(cnts->sumSEL, selID);
	ret = CHECK_CVFS_VOTE_OVERFLOW(vote, currVal);
	if (ret != 0)
		goto updateEnd;

	ENCODE_USER_VOTE(cnts->sumSEL, selID, currVal+vote);
	ret = CHECK_CVFS_VOTE_TOGGLE(vote, currVal);

updateEnd:
	switch (ret) {
	case CVFS_NULL_POINTER:
		ISP_LOGE("cvfs_update null pointer");
		break;
	case CVFS_INVALID_PARAM:
		ISP_LOGE("cvfs_update invalid usrID: %d", usrID);
		break;
	case CVFS_POSITIVE_OVERFLOW:
	case CVFS_NEGATIVE_OVERFLOW:
		ISP_LOGE("cvfs_update overflow, vote:%d", vote);
		break;
	default:
		ISP_LOGI("SUM cvfs: 0x%08x", cnts->sumSEL);
		ISP_LOGI("CAMSYS:0x%08x, IMGSYS: 0x%08x, PDA: 0x%08x, SENINF: 0x%08x, UISP: 0x%08x, VDE: 0x%08x",
			cnts->user_count[VMM_CVFS_USR_CAMSYS],
			cnts->user_count[VMM_CVFS_USR_IMGSYS],
			cnts->user_count[VMM_CVFS_USR_PDA],
			cnts->user_count[VMM_CVFS_USR_SENINF],
			cnts->user_count[VMM_CVFS_USR_UISP],
			cnts->user_count[VMM_CVFS_USR_VDE]);
		break;
	}

	return ret;
}

int vmm_enable_cvfs(enum VMM_CVFS_USR_ID user_id, enum VMM_CVFS_SEL_ID vmm_cvfs_sel_id)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_CONFIG_VMM_SUPPORT_CCF)
#if IS_ENABLED(CONFIG_MTK_HWCCF)
	int hwccf_ret = 0;
#endif
#endif

	switch(vmm_cvfs_sel_id) {
	case VMM_CVFS_CAM_SEL:
	case VMM_CVFS_IMG_SEL:
	case VMM_CVFS_IPE_SEL:
		mutex_lock(&cvfs_mutex);
		ret = update_cvfs_table(&cvfsCNT, user_id, vmm_cvfs_sel_id, (int8_t)1);
		mutex_unlock(&cvfs_mutex);
		if (ret == CVFS_ENABLE_TOGGLE) {
			ISP_LOGI("do cvfs_enable, with %d", vmm_cvfs_sel_id);
		/* TODO: enable cvfs */
#if IS_ENABLED(CONFIG_VMM_SUPPORT_CCF)
#if IS_ENABLED(CONFIG_MTK_HWCCF)
			hwccf_ret = hwccf_irq_voter_ctrl(MM_HWCCF, HW_CCF_BACKUP_GRP_0, HWCCF_VOTE,
				vmm_cvfs_sel_id == VMM_CVFS_CAM_SEL ?
				HW_CCF_CAM_SEL_VOTER_BIT : HW_CCF_IMG_SEL_VOTER_BIT);
			if (hwccf_ret) {
				ISP_LOGE("HWCCF cvfs unvoter failed, ret: %d", hwccf_ret);
				clkchk_external_dump();
			}
#endif
#endif
		} else if (ret == CVFS_DISABLE_TOGGLE) {
			ISP_LOGE("vmm_enable should NOT disable %d", vmm_cvfs_sel_id);
			ret = CVFS_UPDATE_EXCEPTION;
		} else if (ret != CVFS_UPDATE_SUCCESS) {
			ISP_LOGE("vmm_enable failed, ret: %d", ret);
			vmm_cvfs_dump();  // temp use
		}
		break;
	case VMM_CVFS_VDE_SEL:
		ISP_LOGE("ap not support to vote VDE_SEL");
		break;
	default:
		ISP_LOGE("[user:%d] [sel:%d] unknown sel id", user_id, vmm_cvfs_sel_id);
		ret = CVFS_INVALID_PARAM;
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(vmm_enable_cvfs);

int vmm_disable_cvfs(enum VMM_CVFS_USR_ID user_id, enum VMM_CVFS_SEL_ID vmm_cvfs_sel_id)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_VMM_SUPPORT_CCF)
#if IS_ENABLED(CONFIG_MTK_HWCCF)
	int hwccf_ret = 0;
#endif
#endif

	switch(vmm_cvfs_sel_id) {
	case VMM_CVFS_CAM_SEL:
	case VMM_CVFS_IMG_SEL:
	case VMM_CVFS_IPE_SEL:
		mutex_lock(&cvfs_mutex);
		ret = update_cvfs_table(&cvfsCNT, user_id, vmm_cvfs_sel_id, (int8_t)-1);
		mutex_unlock(&cvfs_mutex);
		if (ret == CVFS_DISABLE_TOGGLE) {
			ISP_LOGI("do cvfs_disable, with %d", vmm_cvfs_sel_id);
		/* TODO: disable cvfs */
#if IS_ENABLED(CONFIG_VMM_SUPPORT_CCF)
#if IS_ENABLED(CONFIG_MTK_HWCCF)
			hwccf_ret = hwccf_irq_voter_ctrl(MM_HWCCF, HW_CCF_BACKUP_GRP_0, HWCCF_UNVOTE,
				vmm_cvfs_sel_id == VMM_CVFS_CAM_SEL ?
				HW_CCF_CAM_SEL_VOTER_BIT : HW_CCF_IMG_SEL_VOTER_BIT);
			if (hwccf_ret) {
				ISP_LOGE("HWCCF cvfs unvoter failed, ret: %d", hwccf_ret);
				clkchk_external_dump();
			}
#endif
#endif
		} else if (ret == CVFS_ENABLE_TOGGLE) {
			ISP_LOGE("vmm_disable should NOT enable %d", vmm_cvfs_sel_id);
			ret = CVFS_UPDATE_EXCEPTION;
		} else if (ret != CVFS_UPDATE_SUCCESS) {
			ISP_LOGE("vmm_disable failed, ret: %d", ret);
			vmm_cvfs_dump();  // temp use
		}
		break;
	case VMM_CVFS_VDE_SEL:
		ISP_LOGE("ap not support to unvote VDE_SEL");
		break;
	default:
		ISP_LOGE("[user:%d] [sel:%d] unknown sel id", user_id, vmm_cvfs_sel_id);
		ret = CVFS_INVALID_PARAM;
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(vmm_disable_cvfs);

int vmm_cvfs_dump(void)
{
	ISP_LOGI("vmm_cvfs_dump");

	return 0;
}
EXPORT_SYMBOL_GPL(vmm_cvfs_dump);

static int vmm_locked_buck_ctrl(bool enable)
{
	int ret = 0;
	int pre_cnt = vmm_user_counter;

	vmm_user_counter += (enable ? 1 : -1);

	// Check if we're transitioning from 0 to 1 (enabling) or from 1 to 0 (disabling)
	if ((pre_cnt == 0 && vmm_user_counter == 1) || (pre_cnt == 1 && vmm_user_counter == 0)) {
		bool is_activating = (pre_cnt == 0 && vmm_user_counter == 1);

#if IS_ENABLED(CONFIG_MTK_HWCCF)
		ret = hwccf_irq_multi_voter_ctrl(MM_HWCCF, HW_CCF_BACKUP_GRP_0,
						 is_activating ? HWCCF_VOTE : HWCCF_UNVOTE,
						 BIT(HW_CCF_AP_VOTER_BIT)|BIT(VMM_BUCK_SWITCH));
		if (ret) {
			ISP_LOGE("HWCCF_voter_ctrl fail, ret: %d", ret);
			clkchk_external_dump();
		}
#endif
	}

	return ret;
}

int mtk_vmm_ctrl_dbg_use(bool enable)
{
	int ret = 0;

	mutex_lock(&ctrl_mutex);
	ISP_LOGI("dbg_use en: %u\n", enable);
	ret = vmm_locked_buck_ctrl(enable);
	mutex_unlock(&ctrl_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_vmm_ctrl_dbg_use);

int mtk_vmm_ctrl(struct cb_params *cb_para)
{
	int ret = 0;

	mutex_lock(&ctrl_mutex);
	/* TODO: save cg_status, PIC: Eric Chien */
	ret = vmm_locked_buck_ctrl(cb_para->onoff ? true : false);
	mutex_unlock(&ctrl_mutex);

	return ret;
}

static int vmm_notifier_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	u32 pd_id;

	ret = of_property_read_u32(dev->of_node, "pd-id", &pd_id);
	if (ret) {
		ISP_LOGE("vmm property read fail(%d)", ret);
		return -ENODEV;
	}

	ISP_LOGI("register mtk_vmm for hwccf api");
	register_mtk_clk_external_api_cb(CLK_REQUEST_VMM_CB, &mtk_vmm_ctrl, NULL);

	vmm_locked_buck_ctrl(true);
	vmm_locked_buck_ctrl(false);

	return 0;
}

static const struct of_device_id of_vmm_notifier_match_tbl[] = {
	{
		.compatible = "mediatek,vmm_notifier_mt6993",
	},
	{}
};

static struct platform_driver drv_vmm_notifier = {
	.probe = vmm_notifier_probe,
	.driver = {
		.name = "mtk-vmm-notifier-mt6993",
		.of_match_table = of_vmm_notifier_match_tbl,
	},
};

static int __init mtk_vmm_notifier_init(void)
{
	s32 status;

	mutex_init(&ctrl_mutex);
	mutex_init(&cvfs_mutex);

	ISP_LOGI("[%s][%d] start\n", __func__, __LINE__);
	vmm_user_counter = 0;
	memset(&cvfsCNT, 0, sizeof(cvfsCNT));

	mutex_lock(&ctrl_mutex);
	// vmm_locked_buck_ctrl(true);
	mutex_unlock(&ctrl_mutex);
	status = platform_driver_register(&drv_vmm_notifier);
	if (status) {
		pr_notice("Failed to register VMM dbg driver(%d)\n", status);
		return -ENODEV;
	}

	mutex_lock(&ctrl_mutex);
	// vmm_locked_buck_ctrl(false);
	ISP_LOGI("[%s][%d] end, vmm_user_counter=%d\n", __func__, __LINE__, vmm_user_counter);
	mutex_unlock(&ctrl_mutex);

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
	unsigned int enable;
	unsigned int vote_bit;
	int ret;

	ret = sscanf(val, "%u %u", &enable, &vote_bit);
	if (ret <= 0) {
		ISP_LOGI("sscanf ret is wrong %d\n", ret);
		return 0;
	}
	ISP_LOGI("[%s][%d] en[%u] vote_bit[%u]\n", __func__, __LINE__, enable, vote_bit);

#if IS_ENABLED(CONFIG_MTK_HWCCF)
	ret = hwccf_irq_multi_voter_ctrl(MM_HWCCF, HW_CCF_BACKUP_GRP_0,
						(enable > 0) ? HWCCF_VOTE : HWCCF_UNVOTE,
						BIT(vote_bit+HW_CCF_AP_VOTER_BIT)|BIT(VMM_BUCK_SWITCH));
	if (ret)
		ISP_LOGE("HWCCF_voter_ctrl fail, ret: %d", ret);
#endif

	return 0;
}

static const struct kernel_param_ops vmm_notify_ut_ctrl_ops = {
	.set = mtk_vmm_notify_ut_ctrl,
};

int mtk_vmm_ccf_ut_ctrl(const char *val, const struct kernel_param *kp)
{
	unsigned int enable;
	unsigned int vote_bit;
	struct cb_params cb_para;
	int ret;

	ret = sscanf(val, "%u %u", &enable, &vote_bit);
	if (ret <= 0) {
		ISP_LOGI("sscanf ret is wrong %d\n", ret);
		return 0;
	}
	ISP_LOGI("[%s][%d] en[%u] vote_bit[%u]\n", __func__, __LINE__, enable, vote_bit);
	cb_para.onoff = enable;
	mtk_vmm_ctrl(&cb_para);

	return 0;
}

static const struct kernel_param_ops vmm_ccf_ut_ctrl_ops = {
	.set = mtk_vmm_ccf_ut_ctrl,
};

int mtk_vmm_cvfs_ut_ctrl(const char *val, const struct kernel_param *kp)
{
	unsigned int enable;
	unsigned int user_id;
	unsigned int vmm_cvfs_sel_id;
	int ret;

	ret = sscanf(val, "%u %u %u", &enable, &user_id, &vmm_cvfs_sel_id);
	if (ret <= 0) {
		ISP_LOGI("sscanf ret is wrong %d\n", ret);
		return 0;
	}
	ISP_LOGI("[%s][%d] en[%u] usr_id[%u] sel[%u]",
		__func__, __LINE__, enable, user_id, vmm_cvfs_sel_id);
	switch (enable) {
	case 0:
		vmm_disable_cvfs(user_id, vmm_cvfs_sel_id);
		break;
	case 1:
		vmm_enable_cvfs(user_id, vmm_cvfs_sel_id);
		break;
	default:
		break;
	}

	return 0;
}

static const struct kernel_param_ops vmm_cvfs_ut_ctrl_ops = {
	.set = mtk_vmm_cvfs_ut_ctrl,
};

int mtk_vmm_force_buck_ctrl(const char *val, const struct kernel_param *kp)
{
	unsigned int enable;
	int ret;

	ret = kstrtouint(val, 0, &enable);
	if (ret)
		return ret;
	ISP_LOGI("[%s][%d] force buck en[%u]", __func__, __LINE__, enable);

#if IS_ENABLED(CONFIG_MTK_HWCCF)
	switch (enable) {
	case 0:
		ret = hwccf_irq_voter_ctrl(MM_HWCCF, HW_CCF_BACKUP_GRP_0, HWCCF_VOTE,
							VMM_DBG_FORCE_BUCK_OFF_BIT);
		if (ret)
			ISP_LOGE("HWCCF_voter_ctrl fail, ret: %d", ret);
		ret = hwccf_irq_voter_ctrl(MM_HWCCF, HW_CCF_BACKUP_GRP_0, HWCCF_UNVOTE,
							VMM_DBG_FORCE_BUCK_OFF_BIT);
		if (ret)
			ISP_LOGE("HWCCF_voter_ctrl fail, ret: %d", ret);
		break;
	case 1:
		ret = hwccf_irq_voter_ctrl(MM_HWCCF, HW_CCF_BACKUP_GRP_0, HWCCF_VOTE,
							VMM_DBG_FORCE_BUCK_ON_BIT);
		if (ret)
			ISP_LOGE("HWCCF_voter_ctrl fail, ret: %d", ret);
		ret = hwccf_irq_voter_ctrl(MM_HWCCF, HW_CCF_BACKUP_GRP_0, HWCCF_UNVOTE,
							VMM_DBG_FORCE_BUCK_ON_BIT);
		if (ret)
			ISP_LOGE("HWCCF_voter_ctrl fail, ret: %d", ret);
		break;
	case 8:
		ret = hwccf_irq_voter_ctrl(MM_HWCCF, HW_CCF_BACKUP_GRP_0, HWCCF_VOTE,
							VMM_DBG_EN_BIT);
		if (ret)
			ISP_LOGE("HWCCF_voter_ctrl fail, ret: %d", ret);
		break;
	case 9:
		ret = hwccf_irq_voter_ctrl(MM_HWCCF, HW_CCF_BACKUP_GRP_0, HWCCF_UNVOTE,
							VMM_DBG_EN_BIT);
		if (ret)
			ISP_LOGE("HWCCF_voter_ctrl fail, ret: %d", ret);
		break;
	default:
		break;
	}
#endif
	return 0;
}

static const struct kernel_param_ops vmm_force_buck_ctrl_ops = {
	.set = mtk_vmm_force_buck_ctrl,
};

module_param_cb(vmm_notify_ut_ctrl, &vmm_notify_ut_ctrl_ops, NULL, 0644);
MODULE_PARM_DESC(vmm_notify_ut_ctrl, "vmm_notify_ut_ctrl");

module_param_cb(vmm_ccf_ut_ctrl, &vmm_ccf_ut_ctrl_ops, NULL, 0644);
MODULE_PARM_DESC(vmm_ccf_ut_ctrl, "vmm_ccf_ut_ctrl");

module_param_cb(vmm_cvfs_ut_ctrl, &vmm_cvfs_ut_ctrl_ops, NULL, 0644);
MODULE_PARM_DESC(vmm_cvfs_ut_ctrl, "vmm_cvfs_ut_ctrl");

module_param_cb(vmm_force_buck_ctrl, &vmm_force_buck_ctrl_ops, NULL, 0644);
MODULE_PARM_DESC(vmm_force_buck_ctrl, "vmm_force_buck_ctrl");

module_init(mtk_vmm_notifier_init);
module_exit(mtk_vmm_notifier_exit);
MODULE_DESCRIPTION("MTK VMM notifier driver");
MODULE_AUTHOR("Eric Chien <eric.chien@mediatek.com>");
MODULE_SOFTDEP("pre:mtk-scpsys");
MODULE_SOFTDEP("pre:mtk-scpsys-mt6991");
MODULE_SOFTDEP("pre:mtk-scpsys.ko");
MODULE_SOFTDEP("pre:mtk-scpsys-mt6991.ko");
MODULE_LICENSE("GPL");
