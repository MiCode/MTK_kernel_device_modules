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
#include <mt-plat/mtk-vmm-avs-mt6993.h>
#include <soc/mediatek/mmdvfs_v3.h>
#include <mt-plat/mtk-vmm-notifier.h>
#include <linux/of_address.h>
#include "clk-mtk.h"

#define IS_CVFS_OPP(lvl)		(((lvl) >= OPP_LEVEL_1) && ((lvl) <= OPP_LEVEL_4))
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
#define HW_CCF_IPE_SEL_VOTER_BIT			(16)
#define VMM_DBG_EN_BIT						(18)
#define VMM_DBG_MUX0_BIT			(19)
#define VMM_DBG_MUX1_BIT			(20)
#define VMM_DBG_MUX2_BIT			(21)
#define VMM_DBG_MUX3_BIT			(22)

#define GET_SEL_COUNT(value, sel) (((value) >> ((sel) * 8)) & SEL_MASK)

#define ENCODE_USER_VOTE(value, sel, newCount) \
	((value) = ((value) & ~(SEL_MASK << ((sel) * 8))) | ((newCount) << ((sel) * 8)))

#define CHECK_CVFS_VOTE_OVERFLOW(vote, currVal) \
	(((vote) > 0 && (currVal) == SEL_MASK) ? CVFS_POSITIVE_OVERFLOW : \
	(((vote) < 0 && (currVal) == 0) ? CVFS_NEGATIVE_OVERFLOW : 0))

#define CHECK_CVFS_VOTE_TOGGLE(vote, currVal) \
	(((vote) > 0 && (currVal) == 0) ? CVFS_ENABLE_TOGGLE : \
	(((vote) < 0 && (currVal) == 1) ? CVFS_DISABLE_TOGGLE : 0))
#define NAME_REG(NAME) NAME##_REG
#define NAME_VAL(NAME) NAME##_VAL
#define VMM_WRITE_REG_BY_NAME(NAME) writel_relaxed(NAME_VAL(NAME).Raw, NAME_REG(NAME))

#define MUX_PARSE_VOTE(curr_val, new_val)  (((new_val) & (~(curr_val))) << VMM_DBG_MUX0_BIT)
#define MUX_PARSE_UNVOTE(curr_val, new_val) (((~(new_val)) & (curr_val)) << VMM_DBG_MUX0_BIT)

#define STEP_TO_MARGIN(step)		(VMM_ONE_STEP_MARGIN * (step))
#define ISP_GUARDBAND_MARGIN_MICROVOLT	STEP_TO_MARGIN(2)
#define ISP_AGING_MARGIN_MICROVOLT		STEP_TO_MARGIN(1)
#define ISP_TEMP_PHASE1_MARGIN_STEP		(5)
#define ISP_TEMP_PHASE1_MARGIN_MICROVOLT	STEP_TO_MARGIN(ISP_TEMP_PHASE1_MARGIN_STEP)
#define ISP_CONST_MARGIN	(ISP_GUARDBAND_MARGIN_MICROVOLT + ISP_AGING_MARGIN_MICROVOLT + \
							ISP_TEMP_PHASE1_MARGIN_MICROVOLT)
#define VDE_GUARDBAND_MARGIN_MICROVOLT	STEP_TO_MARGIN(2)
#define VDE_AGING_MARGIN_MICROVOLT		STEP_TO_MARGIN(2)
#define VDE_CONST_MARGIN	(VDE_GUARDBAND_MARGIN_MICROVOLT + VDE_AGING_MARGIN_MICROVOLT)
#define VMM_ROUNDUP(x, y)			((((x) + (y - 1)) / y) * y)
#define DBG_VMM_DUMP_EFUSE_VAL		(520)

enum temp_zone_idx {
	TEMP_ZONE_1 = 0,
	TEMP_ZONE_2,
	TEMP_ZONE_3,
	TEMP_ZONE_4,
	TEMP_ZONE_MAX,
};

enum OPP_LEVELS {
	OPP_LEVEL_0 = 0, // 0.575(aov)
	OPP_LEVEL_1, // 0.575
	OPP_LEVEL_2, // 0.600
	OPP_LEVEL_3, // 0.650
	OPP_LEVEL_4, // 0.700
	OPP_LEVEL_5, // 0.950
	OPP_LEVEL_TOTAL
};

enum AVS_SUBSYS {
	VMM_AVS_ISP,
	VMM_AVS_VDE,
};

struct vmm_regs_t {
	void __iomem *vmm_efuse_va;
	void __iomem *vmm_cvfs_va;
} vmm_regs;

struct mutex ctrl_mutex;
static int vmm_user_counter;

struct mutex cvfs_mutex;
typedef struct {
	uint32_t sumSEL;
	uint32_t user_count[VMM_CVFS_USR_MAX];
} CVFSCounter;

CVFSCounter cvfsCNT;

static const unsigned int isp_temperature_margin[TEMP_ZONE_MAX][OPP_LEVEL_TOTAL] = {
	{0, 5, 5, 5, 5, 0},	// zone 1: no vb, please use signed off
	{0, 5, 5, 5, 5, 0},	// zone 2
	{0, 3, 3, 3, 3, 0},	// zone 3
	{0, 5, 5, 5, 5, 0},	// zone 4
};

static const unsigned int isp_mssv_margin[OPP_LEVEL_TOTAL] = {
	0,
	STEP_TO_MARGIN(0),
	STEP_TO_MARGIN(1),
	STEP_TO_MARGIN(0),
	STEP_TO_MARGIN(1),
	0,
};

static const unsigned int vde_temperature_margin[TEMP_ZONE_MAX][OPP_LEVEL_TOTAL] = {
	{0, 9, 6, 6, 6, 0},	// zone 1: no vb, please use signed off
	{0, 9, 6, 6, 6, 0},	// zone 2
	{0, 7, 4, 4, 4, 0},	// zone 3
	{0, 9, 6, 6, 6, 0},	// zone 4
};


static const unsigned int vde_mssv_margin[OPP_LEVEL_TOTAL] = {
	0,
	STEP_TO_MARGIN(4),
	STEP_TO_MARGIN(4),
	STEP_TO_MARGIN(4),
	STEP_TO_MARGIN(4),
	0,
};

static const unsigned int isp_cvfs_floor_margin[OPP_LEVEL_TOTAL] = {
	0, 14, 18, 21, 24, 0,  // zone 2
};

static const unsigned int vde_cvfs_floor_margin[OPP_LEVEL_TOTAL] = {
	0, 12, 14, 12, 16, 0,	// zone 2
};

static const unsigned int isp_avs20_floor_margin[OPP_LEVEL_TOTAL] = {
	0, 14, 18, 21, 24, 0  // zone 2
};

static const unsigned int vde_avs20_floor_margin[OPP_LEVEL_TOTAL] = {
	0, 9, 11, 9, 13, 0  // zone 2
};

static unsigned int cross_cvfs_floor_margin[OPP_LEVEL_TOTAL] = {
	0, 0, 0, 0, 0, 0
};

static unsigned int cross_avs20_floor_margin[OPP_LEVEL_TOTAL] = {
	0, 0, 0, 0, 0, 0
};

bool vmm_debug_dump;
static void vmm_update_isp_avs_info(bool enable_avs);
static void vmm_update_vde_avs_info(bool enable_avs);
static void vmm_update_isp_cross_vde_avs_info(bool enable_avs);
static void vmm_update_cvfs_wa_avs_info(bool enable_avs);
static void vmm_update_aging_degrade_info(bool enable_avs);

static unsigned int vmm_cal_avs_phase1(unsigned int OPP, unsigned int efuse_bin, enum AVS_SUBSYS mode);
static unsigned int vmm_cal_cvfs_floor_phase1(unsigned int OPP, enum AVS_SUBSYS mode);
static unsigned int vmm_cal_cross_avs_phase1(unsigned int OPP);
static unsigned int vmm_cal_cross_cvfs_floor_phase1(unsigned int OPP);
static unsigned int vmm_cal_cross_avs20_phase1(unsigned int OPP);
static void vmm_compare_cross_floor_phase1(bool enable_avs);

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
		if (vmm_debug_dump) {
			ISP_LOGI("SUM cvfs: 0x%08x", cnts->sumSEL);
			ISP_LOGI("CAMSYS:0x%08x,IMGSYS:0x%08x,PDA:0x%08x,SENINF:0x%08x,UISP:0x%08x,VDE:0x%08x",
				cnts->user_count[VMM_CVFS_USR_CAMSYS],
				cnts->user_count[VMM_CVFS_USR_IMGSYS],
				cnts->user_count[VMM_CVFS_USR_PDA],
				cnts->user_count[VMM_CVFS_USR_SENINF],
				cnts->user_count[VMM_CVFS_USR_UISP],
				cnts->user_count[VMM_CVFS_USR_VDE]);
		}
		break;
	}

	return ret;
}

int vmm_enable_cvfs(enum VMM_CVFS_USR_ID user_id, enum VMM_CVFS_SEL_ID vmm_cvfs_sel_id)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_MTK_HWCCF)
	int hwccf_ret = 0;
#endif

	switch(vmm_cvfs_sel_id) {
	case VMM_CVFS_CAM_SEL:
	case VMM_CVFS_IMG_SEL:
	case VMM_CVFS_IPE_SEL:
		mutex_lock(&cvfs_mutex);
		ret = update_cvfs_table(&cvfsCNT, user_id, vmm_cvfs_sel_id, (int8_t)1);
		mutex_unlock(&cvfs_mutex);
		if (ret == CVFS_ENABLE_TOGGLE) {
			if (vmm_debug_dump)
				ISP_LOGI("do cvfs_enable, with %d", vmm_cvfs_sel_id);
		#if IS_ENABLED(CONFIG_MTK_HWCCF)
			hwccf_ret = hwccf_irq_voter_ctrl(
				MM_HWCCF, HW_CCF_BACKUP_GRP_0, HWCCF_VOTE,
				vmm_cvfs_sel_id == VMM_CVFS_CAM_SEL ? HW_CCF_CAM_SEL_VOTER_BIT :
				(vmm_cvfs_sel_id == VMM_CVFS_IPE_SEL ? HW_CCF_IPE_SEL_VOTER_BIT :
				HW_CCF_IMG_SEL_VOTER_BIT));
			if (hwccf_ret) {
				ISP_LOGE("HWCCF cvfs unvoter failed, ret: %d", hwccf_ret);
				clkchk_external_dump();
			}
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
#if IS_ENABLED(CONFIG_MTK_HWCCF)
	int hwccf_ret = 0;
#endif

	switch(vmm_cvfs_sel_id) {
	case VMM_CVFS_CAM_SEL:
	case VMM_CVFS_IMG_SEL:
	case VMM_CVFS_IPE_SEL:
		mutex_lock(&cvfs_mutex);
		ret = update_cvfs_table(&cvfsCNT, user_id, vmm_cvfs_sel_id, (int8_t)-1);
		mutex_unlock(&cvfs_mutex);
		if (ret == CVFS_DISABLE_TOGGLE) {
			if (vmm_debug_dump)
				ISP_LOGI("do cvfs_disable, with %d", vmm_cvfs_sel_id);
		#if IS_ENABLED(CONFIG_MTK_HWCCF)
			hwccf_ret = hwccf_irq_voter_ctrl(
				MM_HWCCF, HW_CCF_BACKUP_GRP_0, HWCCF_UNVOTE,
				vmm_cvfs_sel_id == VMM_CVFS_CAM_SEL ? HW_CCF_CAM_SEL_VOTER_BIT :
				(vmm_cvfs_sel_id == VMM_CVFS_IPE_SEL ? HW_CCF_IPE_SEL_VOTER_BIT :
				HW_CCF_IMG_SEL_VOTER_BIT));
			if (hwccf_ret) {
				ISP_LOGE("HWCCF cvfs unvoter failed, ret: %d", hwccf_ret);
				clkchk_external_dump();
			}
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
	if (vmm_debug_dump)
		ISP_LOGI("vmm_cnt: %d", vmm_user_counter);

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
	int vote_val;
	int ret;

	ISP_LOGI("[%s][%d] vmm mtk_vmm_ctrl_dbg_use[%u]", __func__, __LINE__, enable);

	vote_val = MUX_PARSE_VOTE(0, enable);
	ISP_LOGI("vote: 0x%0x", vote_val);
	ret = hwccf_irq_multi_voter_ctrl(MM_HWCCF, HW_CCF_BACKUP_GRP_0, HWCCF_VOTE,
					vote_val | BIT(VMM_DBG_EN_BIT));
	if (ret) {
		ISP_LOGE("HWCCF_voter_ctrl fail, ret: %d", ret);
		clkchk_external_dump();
	}

	vote_val = MUX_PARSE_UNVOTE(enable, 0);
	ISP_LOGI("unvote: 0x%0x", vote_val);
	ret = hwccf_irq_multi_voter_ctrl(MM_HWCCF, HW_CCF_BACKUP_GRP_0, HWCCF_UNVOTE,
					vote_val | BIT(VMM_DBG_EN_BIT));
	if (ret) {
		ISP_LOGE("HWCCF_voter_ctrl fail, ret: %d", ret);
		clkchk_external_dump();
	}

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_vmm_ctrl_dbg_use);

static bool vmm_check_efuse_valid(void)
{
	ISP_LOGI("EFUSE_ISP_VB_VER: %d", EFUSE_ISP_VB_VERSION);
	ISP_LOGI("EFUSE_ISP_VMIN_REG: %d", EFUSE_ISP_VMIN_REG);
	ISP_LOGI("EFUSE_VDE_VMIN_REG: %d", EFUSE_VDE_VMIN_REG);
	if (EFUSE_ISP_VB_VERSION == 0 || EFUSE_ISP_VMIN_REG < 4 || EFUSE_VDE_VMIN_REG < 4) {
		ISP_LOGI("vmin efuse check fail! disable avs flow!");
		return false;
	}

	if ((EFUSE_IMG_DMIN_OP6570 <= 1) ||
		(EFUSE_IMG_DMIN_OP5760 <= 1) ||
		(EFUSE_IPE_DMIN_OP6570 <= 1) ||
		(EFUSE_IPE_DMIN_OP5760 <= 1) ||
		(EFUSE_CAM_DMIN_OP6570 <= 1) ||
		(EFUSE_CAM_DMIN_OP5760 <= 1)) {
		ISP_LOGI("dmin efuse check fail! disable avs flow!");
		return false;
	}

	return true;
}

static void vmm_update_cvfs_table(void)
{
	/* get efuse */
	bool enable_avs = false;

	/* check efuse valid */
	enable_avs = vmm_check_efuse_valid();
	vmm_compare_cross_floor_phase1(enable_avs);
	vmm_update_isp_avs_info(enable_avs);
	vmm_update_vde_avs_info(enable_avs);
	vmm_update_isp_cross_vde_avs_info(enable_avs);
	vmm_update_cvfs_wa_avs_info(enable_avs);
	vmm_update_aging_degrade_info(enable_avs);
}

int mtk_vmm_ctrl(struct cb_params *cb_para)
{
	int ret = 0;

	mutex_lock(&ctrl_mutex);
	/* TODO: save cg_status, PIC: Eric Chien */
	if (vmm_debug_dump)
		ISP_LOGI("vmm_api, %d, %s", cb_para->onoff, cb_para->name);
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

	vmm_regs.vmm_efuse_va = ioremap(0x10165A00, 0x200);
	vmm_regs.vmm_cvfs_va = ioremap(0x31AC4000, 0x1000);

	ISP_LOGI("register mtk_vmm for hwccf api");
	register_mtk_clk_external_api_cb(CLK_REQUEST_VMM_CB, &mtk_vmm_ctrl, NULL);

	vmm_debug_dump = false;
	vmm_locked_buck_ctrl(true);
	vmm_locked_buck_ctrl(false);
	vmm_update_cvfs_table();

	if (vmm_regs.vmm_efuse_va) {
		iounmap(vmm_regs.vmm_efuse_va);
		vmm_regs.vmm_efuse_va = 0L;
	}
	if (vmm_regs.vmm_cvfs_va) {
		iounmap(vmm_regs.vmm_cvfs_va);
		vmm_regs.vmm_cvfs_va = 0L;
	}

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

static void vmm_update_isp_avs_info(bool enable_avs)
{
	/* vmin */
	AVS_PHASE1_VMIN_1_VAL.Bits.AVS_PHASE1_OPP0 = enable_avs ?
		vmm_cal_avs_phase1(OPP_LEVEL_1, ISP_575_VBIN_VAL, VMM_AVS_ISP) : SIGNED_OFF_575V_NORM;
	AVS_PHASE1_VMIN_1_VAL.Bits.AVS_PHASE1_OPP1 = enable_avs ?
		vmm_cal_avs_phase1(OPP_LEVEL_2, ISP_600_VBIN_VAL, VMM_AVS_ISP) : SIGNED_OFF_600V_NORM;
	AVS_PHASE1_VMIN_1_VAL.Bits.AVS_PHASE1_OPP2 = enable_avs ?
		vmm_cal_avs_phase1(OPP_LEVEL_3, ISP_650_VBIN_VAL, VMM_AVS_ISP) : SIGNED_OFF_650V_NORM;
	AVS_PHASE1_VMIN_1_VAL.Bits.AVS_PHASE1_OPP3 = enable_avs ?
		vmm_cal_avs_phase1(OPP_LEVEL_4, ISP_700_VBIN_VAL, VMM_AVS_ISP) : SIGNED_OFF_700V_NORM;
	VMM_WRITE_REG_BY_NAME(AVS_PHASE1_VMIN_1);

	/* temp margin */
	AVS_MARGIN_TEMP_OPP0_1_VAL.Bits.AVS_MARGIN_TEMP_OPP0_Z1 = enable_avs ?
		(SIGNED_OFF_575V_NORM-AVS_PHASE1_VMIN_1_VAL.Bits.AVS_PHASE1_OPP0)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP0_1_VAL.Bits.AVS_MARGIN_TEMP_OPP0_Z2 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP0_1_VAL.Bits.AVS_MARGIN_TEMP_OPP0_Z3 = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP0_1_VAL.Bits.AVS_MARGIN_TEMP_OPP0_Z4 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP0_1);

	AVS_MARGIN_TEMP_OPP0_2_VAL.Bits.AVS_MARGIN_TEMP_OPP0_Z5 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP0_2_VAL.Bits.AVS_MARGIN_TEMP_OPP0_Z6 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP0_2);

	AVS_MARGIN_TEMP_OPP1_1_VAL.Bits.AVS_MARGIN_TEMP_OPP1_Z1 = enable_avs ?
		(SIGNED_OFF_600V_NORM-AVS_PHASE1_VMIN_1_VAL.Bits.AVS_PHASE1_OPP1)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP1_1_VAL.Bits.AVS_MARGIN_TEMP_OPP1_Z2 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP1_1_VAL.Bits.AVS_MARGIN_TEMP_OPP1_Z3 = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP1_1_VAL.Bits.AVS_MARGIN_TEMP_OPP1_Z4 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP1_1);

	AVS_MARGIN_TEMP_OPP1_2_VAL.Bits.AVS_MARGIN_TEMP_OPP1_Z5 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP1_2_VAL.Bits.AVS_MARGIN_TEMP_OPP1_Z6 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP1_2);

	AVS_MARGIN_TEMP_OPP2_1_VAL.Bits.AVS_MARGIN_TEMP_OPP2_Z1 = enable_avs ?
		(SIGNED_OFF_650V_NORM-AVS_PHASE1_VMIN_1_VAL.Bits.AVS_PHASE1_OPP2)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP2_1_VAL.Bits.AVS_MARGIN_TEMP_OPP2_Z2 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP2_1_VAL.Bits.AVS_MARGIN_TEMP_OPP2_Z3 = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP2_1_VAL.Bits.AVS_MARGIN_TEMP_OPP2_Z4 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP2_1);

	AVS_MARGIN_TEMP_OPP2_2_VAL.Bits.AVS_MARGIN_TEMP_OPP2_Z5 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP2_2_VAL.Bits.AVS_MARGIN_TEMP_OPP2_Z6 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP2_2);

	AVS_MARGIN_TEMP_OPP3_1_VAL.Bits.AVS_MARGIN_TEMP_OPP3_Z1 = enable_avs ?
		(SIGNED_OFF_700V_NORM-AVS_PHASE1_VMIN_1_VAL.Bits.AVS_PHASE1_OPP3)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP3_1_VAL.Bits.AVS_MARGIN_TEMP_OPP3_Z2 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP3_1_VAL.Bits.AVS_MARGIN_TEMP_OPP3_Z3 = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP3_1_VAL.Bits.AVS_MARGIN_TEMP_OPP3_Z4 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP3_1);

	AVS_MARGIN_TEMP_OPP3_2_VAL.Bits.AVS_MARGIN_TEMP_OPP3_Z5 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP3_2_VAL.Bits.AVS_MARGIN_TEMP_OPP3_Z6 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP3_2);

	/* lower bound vmin */
	AVS_PHASE1_VMIN_1_partial_VAL.Bits.AVS_PHASE1_OPP0_partial = enable_avs ?
		vmm_cal_cvfs_floor_phase1(OPP_LEVEL_1, VMM_AVS_ISP) : SIGNED_OFF_575V_NORM;
	AVS_PHASE1_VMIN_1_partial_VAL.Bits.AVS_PHASE1_OPP1_partial = enable_avs ?
		vmm_cal_cvfs_floor_phase1(OPP_LEVEL_2, VMM_AVS_ISP) : SIGNED_OFF_600V_NORM;
	AVS_PHASE1_VMIN_1_partial_VAL.Bits.AVS_PHASE1_OPP2_partial = enable_avs ?
		vmm_cal_cvfs_floor_phase1(OPP_LEVEL_3, VMM_AVS_ISP) : SIGNED_OFF_650V_NORM;
	AVS_PHASE1_VMIN_1_partial_VAL.Bits.AVS_PHASE1_OPP3_partial = enable_avs ?
		vmm_cal_cvfs_floor_phase1(OPP_LEVEL_4, VMM_AVS_ISP) : SIGNED_OFF_700V_NORM;
	VMM_WRITE_REG_BY_NAME(AVS_PHASE1_VMIN_1_partial);

	/* lower bound temp */
	AVS_MARGIN_TEMP_OPP0_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP0_Z1_partial = enable_avs ?
		(SIGNED_OFF_575V_NORM-AVS_PHASE1_VMIN_1_partial_VAL.Bits.AVS_PHASE1_OPP0_partial)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP0_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP0_Z2_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP0_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP0_Z3_partial = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP0_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP0_Z4_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP0_partial_1);

	AVS_MARGIN_TEMP_OPP0_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP0_Z5_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP0_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP0_Z6_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP0_partial_2);

	AVS_MARGIN_TEMP_OPP1_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP1_Z1_partial = enable_avs ?
		(SIGNED_OFF_600V_NORM-AVS_PHASE1_VMIN_1_partial_VAL.Bits.AVS_PHASE1_OPP1_partial)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP1_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP1_Z2_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP1_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP1_Z3_partial = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP1_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP1_Z4_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP1_partial_1);

	AVS_MARGIN_TEMP_OPP1_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP1_Z5_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP1_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP1_Z6_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP1_partial_2);

	AVS_MARGIN_TEMP_OPP2_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP2_Z1_partial = enable_avs ?
		(SIGNED_OFF_650V_NORM-AVS_PHASE1_VMIN_1_partial_VAL.Bits.AVS_PHASE1_OPP2_partial)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP2_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP2_Z2_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP2_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP2_Z3_partial = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP2_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP2_Z4_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP2_partial_1);

	AVS_MARGIN_TEMP_OPP2_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP2_Z5_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP2_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP2_Z6_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP2_partial_2);

	AVS_MARGIN_TEMP_OPP3_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP3_Z1_partial = enable_avs ?
		(SIGNED_OFF_700V_NORM-AVS_PHASE1_VMIN_1_partial_VAL.Bits.AVS_PHASE1_OPP3_partial)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP3_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP3_Z2_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP3_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP3_Z3_partial = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP3_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP3_Z4_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP3_partial_1);

	AVS_MARGIN_TEMP_OPP3_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP3_Z5_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP3_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP3_Z6_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP3_partial_2);
}

static void vmm_update_vde_avs_info(bool enable_avs)
{
	/* vmin */
	AVS_PHASE1_VMIN_2_VAL.Bits.AVS_PHASE1_OPP4 = enable_avs ?
		vmm_cal_avs_phase1(OPP_LEVEL_1, VDE_575_VBIN_VAL, VMM_AVS_VDE) : SIGNED_OFF_575V_NORM;
	AVS_PHASE1_VMIN_2_VAL.Bits.AVS_PHASE1_OPP5 = enable_avs ?
		vmm_cal_avs_phase1(OPP_LEVEL_2, VDE_600_VBIN_VAL, VMM_AVS_VDE) : SIGNED_OFF_600V_NORM;
	AVS_PHASE1_VMIN_2_VAL.Bits.AVS_PHASE1_OPP6 = enable_avs ?
		vmm_cal_avs_phase1(OPP_LEVEL_3, VDE_650_VBIN_VAL, VMM_AVS_VDE) : SIGNED_OFF_650V_NORM;
	AVS_PHASE1_VMIN_2_VAL.Bits.AVS_PHASE1_OPP7 = enable_avs ?
		vmm_cal_avs_phase1(OPP_LEVEL_4, VDE_700_VBIN_VAL, VMM_AVS_VDE) : SIGNED_OFF_700V_NORM;
	VMM_WRITE_REG_BY_NAME(AVS_PHASE1_VMIN_2);

	/* temp margin */
	AVS_MARGIN_TEMP_OPP4_1_VAL.Bits.AVS_MARGIN_TEMP_OPP4_Z1 = enable_avs ?
		(SIGNED_OFF_575V_NORM-AVS_PHASE1_VMIN_2_VAL.Bits.AVS_PHASE1_OPP4)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP4_1_VAL.Bits.AVS_MARGIN_TEMP_OPP4_Z2 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP4_1_VAL.Bits.AVS_MARGIN_TEMP_OPP4_Z3 = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP4_1_VAL.Bits.AVS_MARGIN_TEMP_OPP4_Z4 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP4_1);

	AVS_MARGIN_TEMP_OPP4_2_VAL.Bits.AVS_MARGIN_TEMP_OPP4_Z5 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP4_2_VAL.Bits.AVS_MARGIN_TEMP_OPP4_Z6 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP4_2);

	AVS_MARGIN_TEMP_OPP5_1_VAL.Bits.AVS_MARGIN_TEMP_OPP5_Z1 = enable_avs ?
		(SIGNED_OFF_600V_NORM-AVS_PHASE1_VMIN_2_VAL.Bits.AVS_PHASE1_OPP5)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP5_1_VAL.Bits.AVS_MARGIN_TEMP_OPP5_Z2 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP5_1_VAL.Bits.AVS_MARGIN_TEMP_OPP5_Z3 = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP5_1_VAL.Bits.AVS_MARGIN_TEMP_OPP5_Z4 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP5_1);

	AVS_MARGIN_TEMP_OPP5_2_VAL.Bits.AVS_MARGIN_TEMP_OPP5_Z5 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP5_2_VAL.Bits.AVS_MARGIN_TEMP_OPP5_Z6 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP5_2);

	AVS_MARGIN_TEMP_OPP6_1_VAL.Bits.AVS_MARGIN_TEMP_OPP6_Z1 = enable_avs ?
		(SIGNED_OFF_650V_NORM-AVS_PHASE1_VMIN_2_VAL.Bits.AVS_PHASE1_OPP6)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP6_1_VAL.Bits.AVS_MARGIN_TEMP_OPP6_Z2 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP6_1_VAL.Bits.AVS_MARGIN_TEMP_OPP6_Z3 = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP6_1_VAL.Bits.AVS_MARGIN_TEMP_OPP6_Z4 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP6_1);

	AVS_MARGIN_TEMP_OPP6_2_VAL.Bits.AVS_MARGIN_TEMP_OPP6_Z5 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP6_2_VAL.Bits.AVS_MARGIN_TEMP_OPP6_Z6 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP6_2);

	AVS_MARGIN_TEMP_OPP7_1_VAL.Bits.AVS_MARGIN_TEMP_OPP7_Z1 = enable_avs ?
		(SIGNED_OFF_700V_NORM-AVS_PHASE1_VMIN_2_VAL.Bits.AVS_PHASE1_OPP7)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP7_1_VAL.Bits.AVS_MARGIN_TEMP_OPP7_Z2 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP7_1_VAL.Bits.AVS_MARGIN_TEMP_OPP7_Z3 = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP7_1_VAL.Bits.AVS_MARGIN_TEMP_OPP7_Z4 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP7_1);

	AVS_MARGIN_TEMP_OPP7_2_VAL.Bits.AVS_MARGIN_TEMP_OPP7_Z5 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP7_2_VAL.Bits.AVS_MARGIN_TEMP_OPP7_Z6 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP7_2);

	/* lower bound vmin */
	AVS_PHASE1_VMIN_2_partial_VAL.Bits.AVS_PHASE1_OPP4_partial = enable_avs ?
		vmm_cal_cvfs_floor_phase1(OPP_LEVEL_1, VMM_AVS_VDE) : SIGNED_OFF_575V_NORM;
	AVS_PHASE1_VMIN_2_partial_VAL.Bits.AVS_PHASE1_OPP5_partial = enable_avs ?
		vmm_cal_cvfs_floor_phase1(OPP_LEVEL_2, VMM_AVS_VDE) : SIGNED_OFF_600V_NORM;
	AVS_PHASE1_VMIN_2_partial_VAL.Bits.AVS_PHASE1_OPP6_partial = enable_avs ?
		vmm_cal_cvfs_floor_phase1(OPP_LEVEL_3, VMM_AVS_VDE) : SIGNED_OFF_650V_NORM;
	AVS_PHASE1_VMIN_2_partial_VAL.Bits.AVS_PHASE1_OPP7_partial = enable_avs ?
		vmm_cal_cvfs_floor_phase1(OPP_LEVEL_4, VMM_AVS_VDE) : SIGNED_OFF_700V_NORM;
	VMM_WRITE_REG_BY_NAME(AVS_PHASE1_VMIN_2_partial);

	/* lower bound temp */
	AVS_MARGIN_TEMP_OPP4_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP4_Z1_partial = enable_avs ?
		(SIGNED_OFF_575V_NORM-AVS_PHASE1_VMIN_2_partial_VAL.Bits.AVS_PHASE1_OPP4_partial)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP4_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP4_Z2_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP4_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP4_Z3_partial = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP4_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP4_Z4_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP4_partial_1);

	AVS_MARGIN_TEMP_OPP4_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP4_Z5_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP4_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP4_Z6_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP4_partial_2);

	AVS_MARGIN_TEMP_OPP5_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP5_Z1_partial = enable_avs ?
		(SIGNED_OFF_600V_NORM-AVS_PHASE1_VMIN_2_partial_VAL.Bits.AVS_PHASE1_OPP5_partial)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP5_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP5_Z2_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP5_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP5_Z3_partial = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP5_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP5_Z4_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP5_partial_1);

	AVS_MARGIN_TEMP_OPP5_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP5_Z5_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP5_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP5_Z6_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP5_partial_2);

	AVS_MARGIN_TEMP_OPP6_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP6_Z1_partial = enable_avs ?
		(SIGNED_OFF_650V_NORM-AVS_PHASE1_VMIN_2_partial_VAL.Bits.AVS_PHASE1_OPP6_partial)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP6_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP6_Z2_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP6_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP6_Z3_partial = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP6_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP6_Z4_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP6_partial_1);

	AVS_MARGIN_TEMP_OPP6_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP6_Z5_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP6_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP6_Z6_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP6_partial_2);

	AVS_MARGIN_TEMP_OPP7_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP7_Z1_partial = enable_avs ?
		(SIGNED_OFF_700V_NORM-AVS_PHASE1_VMIN_2_partial_VAL.Bits.AVS_PHASE1_OPP7_partial)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP7_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP7_Z2_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP7_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP7_Z3_partial = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP7_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP7_Z4_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP7_partial_1);

	AVS_MARGIN_TEMP_OPP7_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP7_Z5_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP7_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP7_Z6_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP7_partial_2);
}
static void vmm_update_isp_cross_vde_avs_info(bool enable_avs)
{
	/* vmin */
	AVS_PHASE1_VMIN_3_VAL.Bits.AVS_PHASE1_OPP8 = enable_avs ?
		vmm_cal_cross_avs_phase1(OPP_LEVEL_1) : SIGNED_OFF_575V_NORM;
	AVS_PHASE1_VMIN_3_VAL.Bits.AVS_PHASE1_OPP9 = enable_avs ?
		vmm_cal_cross_avs_phase1(OPP_LEVEL_2) : SIGNED_OFF_600V_NORM;
	AVS_PHASE1_VMIN_3_VAL.Bits.AVS_PHASE1_OPP10 = enable_avs ?
		vmm_cal_cross_avs_phase1(OPP_LEVEL_3) : SIGNED_OFF_650V_NORM;
	AVS_PHASE1_VMIN_3_VAL.Bits.AVS_PHASE1_OPP11 = enable_avs ?
		vmm_cal_cross_avs_phase1(OPP_LEVEL_4) : SIGNED_OFF_700V_NORM;
	VMM_WRITE_REG_BY_NAME(AVS_PHASE1_VMIN_3);

	/* temp margin */
	AVS_MARGIN_TEMP_OPP8_1_VAL.Bits.AVS_MARGIN_TEMP_OPP8_Z1 = enable_avs ?
		(SIGNED_OFF_575V_NORM-AVS_PHASE1_VMIN_3_VAL.Bits.AVS_PHASE1_OPP8)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP8_1_VAL.Bits.AVS_MARGIN_TEMP_OPP8_Z2 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP8_1_VAL.Bits.AVS_MARGIN_TEMP_OPP8_Z3 = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP8_1_VAL.Bits.AVS_MARGIN_TEMP_OPP8_Z4 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP8_1);

	AVS_MARGIN_TEMP_OPP8_2_VAL.Bits.AVS_MARGIN_TEMP_OPP8_Z5 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP8_2_VAL.Bits.AVS_MARGIN_TEMP_OPP8_Z6 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP8_2);

	AVS_MARGIN_TEMP_OPP9_1_VAL.Bits.AVS_MARGIN_TEMP_OPP9_Z1 = enable_avs ?
		(SIGNED_OFF_600V_NORM-AVS_PHASE1_VMIN_3_VAL.Bits.AVS_PHASE1_OPP9)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP9_1_VAL.Bits.AVS_MARGIN_TEMP_OPP9_Z2 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP9_1_VAL.Bits.AVS_MARGIN_TEMP_OPP9_Z3 = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP9_1_VAL.Bits.AVS_MARGIN_TEMP_OPP9_Z4 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP9_1);

	AVS_MARGIN_TEMP_OPP9_2_VAL.Bits.AVS_MARGIN_TEMP_OPP9_Z5 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP9_2_VAL.Bits.AVS_MARGIN_TEMP_OPP9_Z6 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP9_2);

	AVS_MARGIN_TEMP_OPP10_1_VAL.Bits.AVS_MARGIN_TEMP_OPP10_Z1 = enable_avs ?
		(SIGNED_OFF_650V_NORM-AVS_PHASE1_VMIN_3_VAL.Bits.AVS_PHASE1_OPP10)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP10_1_VAL.Bits.AVS_MARGIN_TEMP_OPP10_Z2 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP10_1_VAL.Bits.AVS_MARGIN_TEMP_OPP10_Z3 = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP10_1_VAL.Bits.AVS_MARGIN_TEMP_OPP10_Z4 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP10_1);

	AVS_MARGIN_TEMP_OPP10_2_VAL.Bits.AVS_MARGIN_TEMP_OPP10_Z5 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP10_2_VAL.Bits.AVS_MARGIN_TEMP_OPP10_Z6 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP10_2);

	AVS_MARGIN_TEMP_OPP11_1_VAL.Bits.AVS_MARGIN_TEMP_OPP11_Z1 = enable_avs ?
		(SIGNED_OFF_700V_NORM-AVS_PHASE1_VMIN_3_VAL.Bits.AVS_PHASE1_OPP11)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP11_1_VAL.Bits.AVS_MARGIN_TEMP_OPP11_Z2 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP11_1_VAL.Bits.AVS_MARGIN_TEMP_OPP11_Z3 = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP11_1_VAL.Bits.AVS_MARGIN_TEMP_OPP11_Z4 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP11_1);

	AVS_MARGIN_TEMP_OPP11_2_VAL.Bits.AVS_MARGIN_TEMP_OPP11_Z5 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP11_2_VAL.Bits.AVS_MARGIN_TEMP_OPP11_Z6 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP11_2);

	/* lower bound vmin */
	AVS_PHASE1_VMIN_3_partial_VAL.Bits.AVS_PHASE1_OPP8_partial = enable_avs ?
		vmm_cal_cross_cvfs_floor_phase1(OPP_LEVEL_1) : SIGNED_OFF_575V_NORM;
	AVS_PHASE1_VMIN_3_partial_VAL.Bits.AVS_PHASE1_OPP9_partial = enable_avs ?
		vmm_cal_cross_cvfs_floor_phase1(OPP_LEVEL_2) : SIGNED_OFF_600V_NORM;
	AVS_PHASE1_VMIN_3_partial_VAL.Bits.AVS_PHASE1_OPP10_partial = enable_avs ?
		vmm_cal_cross_cvfs_floor_phase1(OPP_LEVEL_3) : SIGNED_OFF_650V_NORM;
	AVS_PHASE1_VMIN_3_partial_VAL.Bits.AVS_PHASE1_OPP11_partial = enable_avs ?
		vmm_cal_cross_cvfs_floor_phase1(OPP_LEVEL_4) : SIGNED_OFF_700V_NORM;
	VMM_WRITE_REG_BY_NAME(AVS_PHASE1_VMIN_3_partial);

	/* lower bound temp margin */
	AVS_MARGIN_TEMP_OPP8_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP8_Z1_partial = enable_avs ?
		(SIGNED_OFF_575V_NORM-AVS_PHASE1_VMIN_3_partial_VAL.Bits.AVS_PHASE1_OPP8_partial)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP8_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP8_Z2_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP8_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP8_Z3_partial = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP8_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP8_Z4_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP8_partial_1);

	AVS_MARGIN_TEMP_OPP8_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP8_Z5_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP8_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP8_Z6_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP8_partial_2);

	AVS_MARGIN_TEMP_OPP9_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP9_Z1_partial = enable_avs ?
		(SIGNED_OFF_600V_NORM-AVS_PHASE1_VMIN_3_partial_VAL.Bits.AVS_PHASE1_OPP9_partial)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP9_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP9_Z2_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP9_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP9_Z3_partial = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP9_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP9_Z4_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP9_partial_1);

	AVS_MARGIN_TEMP_OPP9_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP9_Z5_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP9_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP9_Z6_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP9_partial_2);

	AVS_MARGIN_TEMP_OPP10_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP10_Z1_partial = enable_avs ?
		(SIGNED_OFF_650V_NORM-AVS_PHASE1_VMIN_3_partial_VAL.Bits.AVS_PHASE1_OPP10_partial)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP10_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP10_Z2_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP10_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP10_Z3_partial = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP10_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP10_Z4_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP10_partial_1);

	AVS_MARGIN_TEMP_OPP10_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP10_Z5_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP10_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP10_Z6_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP10_partial_2);

	AVS_MARGIN_TEMP_OPP11_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP11_Z1_partial = enable_avs ?
		(SIGNED_OFF_700V_NORM-AVS_PHASE1_VMIN_3_partial_VAL.Bits.AVS_PHASE1_OPP11_partial)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP11_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP11_Z2_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP11_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP11_Z3_partial = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP11_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP11_Z4_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP11_partial_1);

	AVS_MARGIN_TEMP_OPP11_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP11_Z5_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP11_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP11_Z6_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP11_partial_2);
}
static void vmm_update_cvfs_wa_avs_info(bool enable_avs)
{
	/* vmin */
	AVS_PHASE1_VMIN_4_VAL.Bits.AVS_PHASE1_OPP12 = enable_avs ?
		vmm_cal_cross_avs20_phase1(OPP_LEVEL_1) : SIGNED_OFF_575V_NORM;
	AVS_PHASE1_VMIN_4_VAL.Bits.AVS_PHASE1_OPP13 = enable_avs ?
		vmm_cal_cross_avs20_phase1(OPP_LEVEL_2) : SIGNED_OFF_600V_NORM;
	AVS_PHASE1_VMIN_4_VAL.Bits.AVS_PHASE1_OPP14 = enable_avs ?
		vmm_cal_cross_avs20_phase1(OPP_LEVEL_3) : SIGNED_OFF_650V_NORM;
	AVS_PHASE1_VMIN_4_VAL.Bits.AVS_PHASE1_OPP15 = enable_avs ?
		vmm_cal_cross_avs20_phase1(OPP_LEVEL_4) : SIGNED_OFF_700V_NORM;
	VMM_WRITE_REG_BY_NAME(AVS_PHASE1_VMIN_4);

	/* temp margin */
	AVS_MARGIN_TEMP_OPP12_1_VAL.Bits.AVS_MARGIN_TEMP_OPP12_Z1 = enable_avs ?
		(SIGNED_OFF_575V_NORM-AVS_PHASE1_VMIN_4_VAL.Bits.AVS_PHASE1_OPP12)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP12_1_VAL.Bits.AVS_MARGIN_TEMP_OPP12_Z2 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP12_1_VAL.Bits.AVS_MARGIN_TEMP_OPP12_Z3 = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP12_1_VAL.Bits.AVS_MARGIN_TEMP_OPP12_Z4 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP12_1);

	AVS_MARGIN_TEMP_OPP12_2_VAL.Bits.AVS_MARGIN_TEMP_OPP12_Z5 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP12_2_VAL.Bits.AVS_MARGIN_TEMP_OPP12_Z6 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP12_2);

	AVS_MARGIN_TEMP_OPP13_1_VAL.Bits.AVS_MARGIN_TEMP_OPP13_Z1 = enable_avs ?
		(SIGNED_OFF_600V_NORM-AVS_PHASE1_VMIN_4_VAL.Bits.AVS_PHASE1_OPP13)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP13_1_VAL.Bits.AVS_MARGIN_TEMP_OPP13_Z2 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP13_1_VAL.Bits.AVS_MARGIN_TEMP_OPP13_Z3 = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP13_1_VAL.Bits.AVS_MARGIN_TEMP_OPP13_Z4 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP13_1);

	AVS_MARGIN_TEMP_OPP13_2_VAL.Bits.AVS_MARGIN_TEMP_OPP13_Z5 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP13_2_VAL.Bits.AVS_MARGIN_TEMP_OPP13_Z6 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP13_2);

	AVS_MARGIN_TEMP_OPP14_1_VAL.Bits.AVS_MARGIN_TEMP_OPP14_Z1 = enable_avs ?
		(SIGNED_OFF_650V_NORM-AVS_PHASE1_VMIN_4_VAL.Bits.AVS_PHASE1_OPP14)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP14_1_VAL.Bits.AVS_MARGIN_TEMP_OPP14_Z2 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP14_1_VAL.Bits.AVS_MARGIN_TEMP_OPP14_Z3 = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP14_1_VAL.Bits.AVS_MARGIN_TEMP_OPP14_Z4 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP14_1);

	AVS_MARGIN_TEMP_OPP14_2_VAL.Bits.AVS_MARGIN_TEMP_OPP14_Z5 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP14_2_VAL.Bits.AVS_MARGIN_TEMP_OPP14_Z6 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP14_2);

	AVS_MARGIN_TEMP_OPP15_1_VAL.Bits.AVS_MARGIN_TEMP_OPP15_Z1 = enable_avs ?
		(SIGNED_OFF_700V_NORM-AVS_PHASE1_VMIN_4_VAL.Bits.AVS_PHASE1_OPP15)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP15_1_VAL.Bits.AVS_MARGIN_TEMP_OPP15_Z2 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP15_1_VAL.Bits.AVS_MARGIN_TEMP_OPP15_Z3 = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP15_1_VAL.Bits.AVS_MARGIN_TEMP_OPP15_Z4 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP15_1);

	AVS_MARGIN_TEMP_OPP15_2_VAL.Bits.AVS_MARGIN_TEMP_OPP15_Z5 = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP15_2_VAL.Bits.AVS_MARGIN_TEMP_OPP15_Z6 = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP15_2);

	/* lower bound vmin */
	AVS_PHASE1_VMIN_4_partial_VAL.Bits.AVS_PHASE1_OPP12_partial = enable_avs ?
		vmm_cal_cross_avs20_phase1(OPP_LEVEL_1) : SIGNED_OFF_575V_NORM;
	AVS_PHASE1_VMIN_4_partial_VAL.Bits.AVS_PHASE1_OPP13_partial = enable_avs ?
		vmm_cal_cross_avs20_phase1(OPP_LEVEL_2) : SIGNED_OFF_600V_NORM;
	AVS_PHASE1_VMIN_4_partial_VAL.Bits.AVS_PHASE1_OPP14_partial = enable_avs ?
		vmm_cal_cross_avs20_phase1(OPP_LEVEL_3) : SIGNED_OFF_650V_NORM;
	AVS_PHASE1_VMIN_4_partial_VAL.Bits.AVS_PHASE1_OPP15_partial = enable_avs ?
		vmm_cal_cross_avs20_phase1(OPP_LEVEL_4) : SIGNED_OFF_700V_NORM;
	VMM_WRITE_REG_BY_NAME(AVS_PHASE1_VMIN_4_partial);

	/* lower bound temp */
	AVS_MARGIN_TEMP_OPP12_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP12_Z1_partial = enable_avs ?
		(SIGNED_OFF_575V_NORM-AVS_PHASE1_VMIN_4_partial_VAL.Bits.AVS_PHASE1_OPP12_partial)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP12_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP12_Z2_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP12_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP12_Z3_partial = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP12_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP12_Z4_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP12_partial_1);

	AVS_MARGIN_TEMP_OPP12_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP12_Z5_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP12_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP12_Z6_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP12_partial_2);

	AVS_MARGIN_TEMP_OPP13_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP13_Z1_partial = enable_avs ?
		(SIGNED_OFF_600V_NORM-AVS_PHASE1_VMIN_4_partial_VAL.Bits.AVS_PHASE1_OPP13_partial)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP13_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP13_Z2_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP13_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP13_Z3_partial = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP13_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP13_Z4_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP13_partial_1);

	AVS_MARGIN_TEMP_OPP13_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP13_Z5_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP13_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP13_Z6_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP13_partial_2);

	AVS_MARGIN_TEMP_OPP14_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP14_Z1_partial = enable_avs ?
		(SIGNED_OFF_650V_NORM-AVS_PHASE1_VMIN_4_partial_VAL.Bits.AVS_PHASE1_OPP14_partial)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP14_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP14_Z2_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP14_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP14_Z3_partial = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP14_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP14_Z4_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP14_partial_1);

	AVS_MARGIN_TEMP_OPP14_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP14_Z5_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP14_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP14_Z6_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP14_partial_2);

	AVS_MARGIN_TEMP_OPP15_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP15_Z1_partial = enable_avs ?
		(SIGNED_OFF_700V_NORM-AVS_PHASE1_VMIN_4_partial_VAL.Bits.AVS_PHASE1_OPP15_partial)+16 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP15_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP15_Z2_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP15_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP15_Z3_partial = enable_avs ? 2 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP15_partial_1_VAL.Bits.AVS_MARGIN_TEMP_OPP15_Z4_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP15_partial_1);

	AVS_MARGIN_TEMP_OPP15_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP15_Z5_partial = enable_avs ? 0 : FORCE_ZERO;
	AVS_MARGIN_TEMP_OPP15_partial_2_VAL.Bits.AVS_MARGIN_TEMP_OPP15_Z6_partial = enable_avs ? 0 : FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_TEMP_OPP15_partial_2);
}

static void vmm_update_aging_degrade_info(bool enable_avs)
{
	AVS_MARGIN_AGING_1_VAL.Bits.AVS_MARGIN_AGING_OPP0 = FORCE_ZERO;
	AVS_MARGIN_AGING_1_VAL.Bits.AVS_MARGIN_AGING_OPP1 = FORCE_ZERO;
	AVS_MARGIN_AGING_1_VAL.Bits.AVS_MARGIN_AGING_OPP2 = FORCE_ZERO;
	AVS_MARGIN_AGING_1_VAL.Bits.AVS_MARGIN_AGING_OPP3 = FORCE_ZERO;
	AVS_MARGIN_AGING_1_VAL.Bits.AVS_MARGIN_AGING_OPP4 = FORCE_ZERO;
	AVS_MARGIN_AGING_1_VAL.Bits.AVS_MARGIN_AGING_OPP5 = FORCE_ZERO;
	AVS_MARGIN_AGING_1_VAL.Bits.AVS_MARGIN_AGING_OPP6 = FORCE_ZERO;
	AVS_MARGIN_AGING_1_VAL.Bits.AVS_MARGIN_AGING_OPP7 = FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_AGING_1);

	AVS_MARGIN_AGING_2_VAL.Bits.AVS_MARGIN_AGING_OPP8 = FORCE_ZERO;
	AVS_MARGIN_AGING_2_VAL.Bits.AVS_MARGIN_AGING_OPP9 = FORCE_ZERO;
	AVS_MARGIN_AGING_2_VAL.Bits.AVS_MARGIN_AGING_OPP10 = FORCE_ZERO;
	AVS_MARGIN_AGING_2_VAL.Bits.AVS_MARGIN_AGING_OPP11 = FORCE_ZERO;
	AVS_MARGIN_AGING_2_VAL.Bits.AVS_MARGIN_AGING_OPP12 = FORCE_ZERO;
	AVS_MARGIN_AGING_2_VAL.Bits.AVS_MARGIN_AGING_OPP13 = FORCE_ZERO;
	AVS_MARGIN_AGING_2_VAL.Bits.AVS_MARGIN_AGING_OPP14 = FORCE_ZERO;
	AVS_MARGIN_AGING_2_VAL.Bits.AVS_MARGIN_AGING_OPP15 = FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_AGING_2);

	AVS_MARGIN_AGING_1_partial_VAL.Bits.AVS_MARGIN_AGING_OPP0_partial = FORCE_ZERO;
	AVS_MARGIN_AGING_1_partial_VAL.Bits.AVS_MARGIN_AGING_OPP1_partial = FORCE_ZERO;
	AVS_MARGIN_AGING_1_partial_VAL.Bits.AVS_MARGIN_AGING_OPP2_partial = FORCE_ZERO;
	AVS_MARGIN_AGING_1_partial_VAL.Bits.AVS_MARGIN_AGING_OPP3_partial = FORCE_ZERO;
	AVS_MARGIN_AGING_1_partial_VAL.Bits.AVS_MARGIN_AGING_OPP4_partial = FORCE_ZERO;
	AVS_MARGIN_AGING_1_partial_VAL.Bits.AVS_MARGIN_AGING_OPP5_partial = FORCE_ZERO;
	AVS_MARGIN_AGING_1_partial_VAL.Bits.AVS_MARGIN_AGING_OPP6_partial = FORCE_ZERO;
	AVS_MARGIN_AGING_1_partial_VAL.Bits.AVS_MARGIN_AGING_OPP7_partial = FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_AGING_1_partial);

	AVS_MARGIN_AGING_2_partial_VAL.Bits.AVS_MARGIN_AGING_OPP8_partial = FORCE_ZERO;
	AVS_MARGIN_AGING_2_partial_VAL.Bits.AVS_MARGIN_AGING_OPP9_partial = FORCE_ZERO;
	AVS_MARGIN_AGING_2_partial_VAL.Bits.AVS_MARGIN_AGING_OPP10_partial = FORCE_ZERO;
	AVS_MARGIN_AGING_2_partial_VAL.Bits.AVS_MARGIN_AGING_OPP11_partial = FORCE_ZERO;
	AVS_MARGIN_AGING_2_partial_VAL.Bits.AVS_MARGIN_AGING_OPP12_partial = FORCE_ZERO;
	AVS_MARGIN_AGING_2_partial_VAL.Bits.AVS_MARGIN_AGING_OPP13_partial = FORCE_ZERO;
	AVS_MARGIN_AGING_2_partial_VAL.Bits.AVS_MARGIN_AGING_OPP14_partial = FORCE_ZERO;
	AVS_MARGIN_AGING_2_partial_VAL.Bits.AVS_MARGIN_AGING_OPP15_partial = FORCE_ZERO;
	VMM_WRITE_REG_BY_NAME(AVS_MARGIN_AGING_2_partial);
}

static unsigned int vmm_cal_avs_phase1(unsigned int OPP, unsigned int efuse_bin, enum AVS_SUBSYS mode)
{
	unsigned int result_vol = 0;
	unsigned int vmm_sign = 0;
	unsigned int vmm_sign_norm = 0;

	switch (OPP) {
	case OPP_LEVEL_1:
		result_vol = ATE_575V;
		vmm_sign = SIGNED_OFF_575V;
		vmm_sign_norm = SIGNED_OFF_575V_NORM;
		break;
	case OPP_LEVEL_2:
		result_vol = ATE_600V;
		vmm_sign = SIGNED_OFF_600V;
		vmm_sign_norm = SIGNED_OFF_600V_NORM;
		break;
	case OPP_LEVEL_3:
		result_vol = ATE_650V;
		vmm_sign = SIGNED_OFF_650V;
		vmm_sign_norm = SIGNED_OFF_650V_NORM;
		break;
	case OPP_LEVEL_4:
		result_vol = ATE_700V;
		vmm_sign = SIGNED_OFF_700V;
		vmm_sign_norm = SIGNED_OFF_700V_NORM;
		break;
	default:
		return 0;
	}

	if (efuse_bin >= ATE_BASE_BIN)
		result_vol = result_vol - (efuse_bin-ATE_BASE_BIN) * VMM_ONE_STEP_MARGIN;

	switch (mode) {
	case VMM_AVS_ISP:
		result_vol = result_vol + ISP_CONST_MARGIN + isp_mssv_margin[OPP];
		result_vol = result_vol < (vmm_sign_norm-isp_cvfs_floor_margin[OPP]) ?
								(vmm_sign_norm-isp_cvfs_floor_margin[OPP]) : result_vol;
		break;
	case VMM_AVS_VDE:
		result_vol = result_vol + VDE_CONST_MARGIN + vde_mssv_margin[OPP]
						+ STEP_TO_MARGIN(vde_temperature_margin[TEMP_ZONE_2][OPP]);
		result_vol = result_vol < (vmm_sign_norm-vde_cvfs_floor_margin[OPP]) ?
								(vmm_sign_norm-vde_cvfs_floor_margin[OPP]) : result_vol;
		break;
	default:
		return 0;
	}

	result_vol = (result_vol > vmm_sign) ? vmm_sign: result_vol;
	result_vol = VMM_ROUNDUP(result_vol, VMM_ONE_STEP_MARGIN);

	return (result_vol/VMM_ONE_STEP_MARGIN);
}

static unsigned int vmm_cal_cvfs_floor_phase1(unsigned int OPP, enum AVS_SUBSYS mode)
{
	unsigned int result_vol = 0;

	switch (OPP) {
	case OPP_LEVEL_1:
		result_vol = SIGNED_OFF_575V_NORM;
		break;
	case OPP_LEVEL_2:
		result_vol = SIGNED_OFF_600V_NORM;
		break;
	case OPP_LEVEL_3:
		result_vol = SIGNED_OFF_650V_NORM;
		break;
	case OPP_LEVEL_4:
		result_vol = SIGNED_OFF_700V_NORM;
		break;
	default:
		return 0;
	}

	switch (mode) {
	case VMM_AVS_ISP:
		result_vol = result_vol - isp_cvfs_floor_margin[OPP];
		break;
	case VMM_AVS_VDE:
		result_vol = result_vol - vde_cvfs_floor_margin[OPP];
		break;
	default:
		return 0;
	}

	return result_vol;
}

static unsigned int vmm_cal_cross_avs_phase1(unsigned int OPP)
{
	unsigned int result_vol = 0;
	unsigned int efuse_isp = 0;
	unsigned int efuse_vde = 0;

	switch (OPP) {
	case OPP_LEVEL_1:
		efuse_isp = ISP_575_VBIN_VAL;
		efuse_vde = VDE_575_VBIN_VAL;
		break;
	case OPP_LEVEL_2:
		efuse_isp = ISP_600_VBIN_VAL;
		efuse_vde = VDE_600_VBIN_VAL;
		break;
	case OPP_LEVEL_3:
		efuse_isp = ISP_650_VBIN_VAL;
		efuse_vde = VDE_650_VBIN_VAL;
		break;
	case OPP_LEVEL_4:
		efuse_isp = ISP_700_VBIN_VAL;
		efuse_vde = VDE_700_VBIN_VAL;
		break;
	default:
		return 0;
	}

	result_vol = (vmm_cal_avs_phase1(OPP, efuse_isp, VMM_AVS_ISP) >
				 vmm_cal_avs_phase1(OPP, efuse_vde, VMM_AVS_VDE)) ?
				 vmm_cal_avs_phase1(OPP, efuse_isp, VMM_AVS_ISP) :
				 vmm_cal_avs_phase1(OPP, efuse_vde, VMM_AVS_VDE);

	return result_vol;
}

static unsigned int vmm_cal_cross_cvfs_floor_phase1(unsigned int OPP)
{
	unsigned int result_vol = 0;

	switch (OPP) {
	case OPP_LEVEL_1:
		result_vol = SIGNED_OFF_575V_NORM;
		break;
	case OPP_LEVEL_2:
		result_vol = SIGNED_OFF_600V_NORM;
		break;
	case OPP_LEVEL_3:
		result_vol = SIGNED_OFF_650V_NORM;
		break;
	case OPP_LEVEL_4:
		result_vol = SIGNED_OFF_700V_NORM;
		break;
	default:
		return 0;
	}

	result_vol = result_vol - cross_cvfs_floor_margin[OPP];

	return result_vol;
}

static unsigned int vmm_cal_cross_avs20_phase1(unsigned int OPP)
{
	unsigned int result_vol = 0;
	unsigned int vmm_sign = 0;

	switch (OPP) {
	case OPP_LEVEL_1:
		vmm_sign = SIGNED_OFF_575V_NORM;
		break;
	case OPP_LEVEL_2:
		vmm_sign = SIGNED_OFF_600V_NORM;
		break;
	case OPP_LEVEL_3:
		vmm_sign = SIGNED_OFF_650V_NORM;
		break;
	case OPP_LEVEL_4:
		vmm_sign = SIGNED_OFF_700V_NORM;
		break;
	default:
		return 0;
	}

	result_vol = vmm_cal_cross_avs_phase1(OPP) > (vmm_sign - cross_avs20_floor_margin[OPP]) ?
		vmm_cal_cross_avs_phase1(OPP) : (vmm_sign - cross_avs20_floor_margin[OPP]);

	return result_vol;
}

static void vmm_compare_cross_floor_phase1(bool enable_avs)
{
	if (enable_avs == false)
		return;

	for (int i = OPP_LEVEL_0; i <= OPP_LEVEL_5; i++) {
		cross_avs20_floor_margin[i] = isp_avs20_floor_margin[i] > vde_avs20_floor_margin[i] ?
			 vde_avs20_floor_margin[i] : isp_avs20_floor_margin[i];

		cross_cvfs_floor_margin[i] = isp_cvfs_floor_margin[i] > vde_cvfs_floor_margin[i] ?
			 vde_cvfs_floor_margin[i] : isp_cvfs_floor_margin[i];
	}
}

void mtk_vmm_dump_cvfs_reg(void)
{
	vmm_regs.vmm_efuse_va = ioremap(0x10165A00, 0x200);
	vmm_regs.vmm_cvfs_va = ioremap(0x31AC4000, 0x1000);

	ISP_LOGI("AVS_PHASE1_OPP0~3: 0x%x", readl(AVS_PHASE1_VMIN_1_REG));
	ISP_LOGI("AVS_PARTIAL_OPP0~3: 0x%x", readl(AVS_PHASE1_VMIN_1_partial_REG));
	ISP_LOGI("AVS_TEMP_OPP0~3: 0x%x, 0x%x, 0x%x, 0x%x",
		readl(AVS_MARGIN_TEMP_OPP0_1_REG),
		readl(AVS_MARGIN_TEMP_OPP1_1_REG),
		readl(AVS_MARGIN_TEMP_OPP2_1_REG),
		readl(AVS_MARGIN_TEMP_OPP3_1_REG));
	ISP_LOGI("AVS_PARTIAL_TEMP_OPP0~3: 0x%x, 0x%x, 0x%x, 0x%x",
		readl(AVS_MARGIN_TEMP_OPP0_partial_1_REG),
		readl(AVS_MARGIN_TEMP_OPP1_partial_1_REG),
		readl(AVS_MARGIN_TEMP_OPP2_partial_1_REG),
		readl(AVS_MARGIN_TEMP_OPP3_partial_1_REG));

	ISP_LOGI("AVS_PHASE1_OPP4~7: 0x%x", readl(AVS_PHASE1_VMIN_2_REG));
	ISP_LOGI("AVS_PARTIAL_OPP4~7: 0x%x", readl(AVS_PHASE1_VMIN_2_partial_REG));
	ISP_LOGI("AVS_TEMP_OPP4~7: 0x%x, 0x%x, 0x%x, 0x%x",
		readl(AVS_MARGIN_TEMP_OPP4_1_REG),
		readl(AVS_MARGIN_TEMP_OPP5_1_REG),
		readl(AVS_MARGIN_TEMP_OPP6_1_REG),
		readl(AVS_MARGIN_TEMP_OPP7_1_REG));
	ISP_LOGI("AVS_PARTIAL_TEMP_OPP4~7: 0x%x, 0x%x, 0x%x, 0x%x",
		readl(AVS_MARGIN_TEMP_OPP4_partial_1_REG),
		readl(AVS_MARGIN_TEMP_OPP5_partial_1_REG),
		readl(AVS_MARGIN_TEMP_OPP6_partial_1_REG),
		readl(AVS_MARGIN_TEMP_OPP7_partial_1_REG));

	ISP_LOGI("AVS_PHASE1_OPP8~11: 0x%x", readl(AVS_PHASE1_VMIN_3_REG));
	ISP_LOGI("AVS_PARTIAL_OPP8~11: 0x%x", readl(AVS_PHASE1_VMIN_3_partial_REG));
	ISP_LOGI("AVS_TEMP_OPP8~11: 0x%x, 0x%x, 0x%x, 0x%x",
		readl(AVS_MARGIN_TEMP_OPP8_1_REG),
		readl(AVS_MARGIN_TEMP_OPP9_1_REG),
		readl(AVS_MARGIN_TEMP_OPP10_1_REG),
		readl(AVS_MARGIN_TEMP_OPP11_1_REG));
	ISP_LOGI("AVS_PARTIAL_TEMP_OPP8~11: 0x%x, 0x%x, 0x%x, 0x%x",
		readl(AVS_MARGIN_TEMP_OPP8_partial_1_REG),
		readl(AVS_MARGIN_TEMP_OPP9_partial_1_REG),
		readl(AVS_MARGIN_TEMP_OPP10_partial_1_REG),
		readl(AVS_MARGIN_TEMP_OPP11_partial_1_REG));

	ISP_LOGI("AVS_PHASE1_OPP12~15: 0x%x", readl(AVS_PHASE1_VMIN_4_REG));
	ISP_LOGI("AVS_PARTIAL_OPP12~15: 0x%x", readl(AVS_PHASE1_VMIN_4_partial_REG));
	ISP_LOGI("AVS_TEMP_OPP12~15: 0x%x, 0x%x, 0x%x, 0x%x",
		readl(AVS_MARGIN_TEMP_OPP12_1_REG),
		readl(AVS_MARGIN_TEMP_OPP13_1_REG),
		readl(AVS_MARGIN_TEMP_OPP14_1_REG),
		readl(AVS_MARGIN_TEMP_OPP15_1_REG));
	ISP_LOGI("AVS_PARTIAL_TEMP_OPP12~15: 0x%x, 0x%x, 0x%x, 0x%x",
		readl(AVS_MARGIN_TEMP_OPP12_partial_1_REG),
		readl(AVS_MARGIN_TEMP_OPP13_partial_1_REG),
		readl(AVS_MARGIN_TEMP_OPP14_partial_1_REG),
		readl(AVS_MARGIN_TEMP_OPP15_partial_1_REG));

	if (vmm_regs.vmm_efuse_va) {
		iounmap(vmm_regs.vmm_efuse_va);
		vmm_regs.vmm_efuse_va = 0L;
	}
	if (vmm_regs.vmm_cvfs_va) {
		iounmap(vmm_regs.vmm_cvfs_va);
		vmm_regs.vmm_cvfs_va = 0L;
	}
}

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

int mtk_vmm_dbg_ctrl(const char *val, const struct kernel_param *kp)
{
	int enable;
	int vote_val;
	int ret;

	ret = kstrtouint(val, 0, &enable);
	if (ret)
		return ret;
	ISP_LOGI("[%s][%d] vmm adb cmd[%u]", __func__, __LINE__, enable);

	if (enable == DBG_VMM_DUMP_EFUSE_VAL) {
		mtk_vmm_dump_cvfs_reg();

		return 0;
	}

#if IS_ENABLED(CONFIG_MTK_HWCCF)
	vote_val = MUX_PARSE_VOTE(0, enable);
	ISP_LOGI("vote: 0x%0x", vote_val);
	ret = hwccf_irq_multi_voter_ctrl(MM_HWCCF, HW_CCF_BACKUP_GRP_0, HWCCF_VOTE,
							vote_val | BIT(VMM_DBG_EN_BIT));
	if (ret) {
		ISP_LOGE("HWCCF_voter_ctrl fail, ret: %d", ret);
		clkchk_external_dump();
	}

	vote_val = MUX_PARSE_UNVOTE(enable, 0);
	ISP_LOGI("unvote: 0x%0x", vote_val);
	ret = hwccf_irq_multi_voter_ctrl(MM_HWCCF, HW_CCF_BACKUP_GRP_0, HWCCF_UNVOTE,
							vote_val | BIT(VMM_DBG_EN_BIT));
	if (ret) {
		ISP_LOGE("HWCCF_voter_ctrl fail, ret: %d", ret);
		clkchk_external_dump();
	}
#endif

	if (enable == VMM_DBG_DISABLE_AVS) {
		vmm_regs.vmm_efuse_va = ioremap(0x10165A00, 0x200);
		vmm_regs.vmm_cvfs_va = ioremap(0x31AC4000, 0x1000);

		vmm_update_isp_avs_info(false);
		vmm_update_vde_avs_info(false);
		vmm_update_isp_cross_vde_avs_info(false);
		vmm_update_cvfs_wa_avs_info(false);
		vmm_update_aging_degrade_info(false);

		if (vmm_regs.vmm_efuse_va) {
			iounmap(vmm_regs.vmm_efuse_va);
			vmm_regs.vmm_efuse_va = 0L;
		}
		if (vmm_regs.vmm_cvfs_va) {
			iounmap(vmm_regs.vmm_cvfs_va);
			vmm_regs.vmm_cvfs_va = 0L;
		}
	}

	return 0;
}

static const struct kernel_param_ops vmm_dbg_ctrl_ops = {
	.set = mtk_vmm_dbg_ctrl,
};

int mtk_vmm_dump_debug_ctrl(const char *val, const struct kernel_param *kp)
{
	unsigned int enable;
	int ret;

	ret = kstrtouint(val, 0, &enable);
	if (ret)
		return ret;
	ISP_LOGI("[%s][%d] dump debug en[%u]", __func__, __LINE__, enable);

	switch (enable) {
	case 0:
		ISP_LOGI("vmm_dump_debug_ctrl: disable");
		vmm_debug_dump = false;
		break;
	case 1:
		ISP_LOGI("vmm_dump_debug_ctrl: enable");
		vmm_debug_dump = true;
		break;
	default:
		break;
	}

	return 0;
}

static const struct kernel_param_ops vmm_dump_debug_ctrl_ops = {
	.set = mtk_vmm_dump_debug_ctrl,
};

module_param_cb(vmm_notify_ut_ctrl, &vmm_notify_ut_ctrl_ops, NULL, 0644);
MODULE_PARM_DESC(vmm_notify_ut_ctrl, "vmm_notify_ut_ctrl");

module_param_cb(vmm_ccf_ut_ctrl, &vmm_ccf_ut_ctrl_ops, NULL, 0644);
MODULE_PARM_DESC(vmm_ccf_ut_ctrl, "vmm_ccf_ut_ctrl");

module_param_cb(vmm_cvfs_ut_ctrl, &vmm_cvfs_ut_ctrl_ops, NULL, 0644);
MODULE_PARM_DESC(vmm_cvfs_ut_ctrl, "vmm_cvfs_ut_ctrl");

module_param_cb(vmm_dbg_ctrl, &vmm_dbg_ctrl_ops, NULL, 0644);
MODULE_PARM_DESC(vmm_dbg_ctrl, "vmm_dbg_ctrl");

module_param_cb(vmm_dump_debug_ctrl, &vmm_dump_debug_ctrl_ops, NULL, 0644);
MODULE_PARM_DESC(vmm_dump_debug_ctrl, "vmm_dump_debug_ctrl");

module_init(mtk_vmm_notifier_init);
module_exit(mtk_vmm_notifier_exit);
MODULE_DESCRIPTION("MTK VMM notifier driver");
MODULE_AUTHOR("Eric Chien <eric.chien@mediatek.com>");
MODULE_SOFTDEP("pre:mtk-scpsys");
MODULE_SOFTDEP("pre:mtk-scpsys-mt6991");
MODULE_SOFTDEP("pre:mtk-scpsys.ko");
MODULE_SOFTDEP("pre:mtk-scpsys-mt6991.ko");
MODULE_LICENSE("GPL");
