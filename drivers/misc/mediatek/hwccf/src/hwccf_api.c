// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <hwccf.h>
#include <v0_hwccf.h>

#include <hwccf_provider.h>
#include <hwccf_dbg.h>
#include <linux/module.h>
#include <linux/regmap.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/mfd/syscon.h>
#include <linux/platform_device.h>
#include <linux/device.h>

// Default HWCCF Timeout For _hwccf_voter_ctrl(...)
#define MTK_WAIT_GHWV_PREPARE_CNT     10000
#define MTK_WAIT_GHWV_PREPARE_US      1
#define MTK_WAIT_GHWV_VOTE_CNT        200
#define MTK_WAIT_GHWV_VOTE_US         1
// 300ms to 10ms
#define MTK_WAIT_GHWV_DONE_CNT        10000
#define MTK_WAIT_GHWV_DONE_US         1
// HWCCF Timeout For hwccf_cg_voter_ctrl(...)
#define MTK_WAIT_GHWV_SET_CHK_CNT     2000
#define MTK_WAIT_GHWV_SET_CHK_US      1

// Default HWCCF Timeout For hwccf_dfs_voter_ctrl(...)
#define MTK_WAIT_GHWV_MUX_PREPARE_CNT    100000
#define MTK_WAIT_GHWV_MUX_PREPARE_US     1
#define MTK_WAIT_GHWV_MUX_VOTE_CNT       200
#define MTK_WAIT_GHWV_MUX_VOTE_US        1
#define MTK_WAIT_GHWV_MUX_DONE_CNT       300000
#define MTK_WAIT_GHWV_MUX_DONE_US        1

// For IRQ Voter
#define MTK_WAIT_GHWV_IRQ_PREPARE_CNT 10000
#define MTK_WAIT_GHWV_IRQ_PREPARE_US  1
#define MTK_WAIT_GHWV_IRQ_VOTE_CNT    1250
#define MTK_WAIT_GHWV_IRQ_VOTE_US     2

#define MTK_WAIT_GHWV_MTCMOS_DONE_CNT 10000
#define MTK_WAIT_GHWV_MTCMOS_DONE_US 1

// 10ms
#define MTK_WAIT_GHWV_IRQ_DONE_CNT    10000
#define MTK_WAIT_GHWV_IRQ_DONE_US     1

static bool _inited;
static struct regmap *regmaps[MAX_HWCCF];
static int regmap_count = 0;
static DEFINE_SPINLOCK(hwccf_lock);
static DEFINE_SPINLOCK(hwccf_irq_lock);
struct hwccf_ops *hwccf_ops;

/*usage*/
//#define regmap_read_poll_timeout_atomic(map, addr, val, cond, sleep_us, timeout_us)

/* clock voter support */
static int _v0_hwccf_voter_ctrl(struct regmap *regmap, uint32_t setclr_ofs, uint32_t vote_val,
					  uint32_t done_ofs, uint32_t done_ack_msk)
{
	unsigned long flags;
	bool is_set = V0_IS_SET_FROM_VOTER_ADDR(setclr_ofs);
	uint32_t en_ofs = setclr_ofs;
	int ret = 0;
	uint32_t val;
	uint32_t global_en_ofs = 0, setclr_sta_ofs = 0;

	// Check args
	if (!vote_val || !done_ofs || !done_ack_msk) {
		HWCCF_ERR("bad args\n");
		return -HWV_EINVAL;
	}
	spin_lock_irqsave(&hwccf_lock, flags);
#ifdef HWCCF_TEST_MODE
	hwccf_read(regmap, setclr_ofs);
	goto skip;
#endif
	// Check repeat vote
	val = hwccf_read(regmap, en_ofs);
	if (is_set ? IS_MASK_SET(val, vote_val) : IS_MASK_CLR(val, vote_val)) {
		HWCCF_WARN("already %s, [%x]=%x{%x}\n",
			is_set ? "set" : "clr", setclr_ofs, vote_val, val);
		goto skip;
	}

	// check mtcmos/pll/backup
	switch (setclr_ofs & 0xfff) {
		case V0_CCF_MTCMOS0_SET_OFS:
			setclr_sta_ofs = V0_XPU_MTCMOS0_SET_STA;
			global_en_ofs = V0_CCF_MTCMOS_EN_0;
			break;
		case V0_CCF_MTCMOS0_CLR_OFS:
			setclr_sta_ofs = V0_XPU_MTCMOS0_CLR_STA;
			global_en_ofs = V0_CCF_MTCMOS_EN_0;
			break;
		case V0_CCF_MTCMOS1_SET_OFS:
			setclr_sta_ofs = V0_XPU_MTCMOS1_SET_STA;
			global_en_ofs = V0_CCF_MTCMOS_EN_1;
			break;
		case V0_CCF_MTCMOS1_CLR_OFS:
			setclr_sta_ofs = V0_XPU_MTCMOS1_CLR_STA;
			global_en_ofs = V0_CCF_MTCMOS_EN_1;
			break;
		default:
			setclr_sta_ofs = 0;
			break;
	}

	// Pre-Polling done
	ret = regmap_read_poll_timeout_atomic(regmap, done_ofs, val, IS_MASK_SET(val, done_ack_msk),
	    MTK_WAIT_GHWV_PREPARE_US, MTK_WAIT_GHWV_PREPARE_CNT);
	if (ret) {
		HWCCF_ERR("%s timeout\n", is_set ? "prepare" : "unprepare");
		ret = -HWV_PREPARE_TIMEOUT;
		goto ERR;
	}

	// XPU Voting & polling HW_CCF_XPU$_{CG/MTCMOS/PLL/BACKUP}_SET
	hwccf_write(regmap, setclr_ofs, vote_val);

	ret = regmap_read_poll_timeout_atomic(regmap, en_ofs, val,
		is_set ? IS_MASK_SET(val, vote_val) : IS_MASK_CLR(val, vote_val),
		MTK_WAIT_GHWV_VOTE_US, MTK_WAIT_GHWV_VOTE_CNT);
	if (ret) {
		HWCCF_ERR("%s timeout\n", is_set ? "vote" : "unvote");
		ret = -HWV_VOTE_TIMEOUT;
		goto ERR;
	}

	if (global_en_ofs != 0) {
		if (is_set) {
			ret = regmap_read_poll_timeout_atomic(regmap, global_en_ofs, val,
				IS_MASK_SET(val, vote_val),
				MTK_WAIT_GHWV_DONE_US, MTK_WAIT_GHWV_DONE_CNT);
			if (ret) {
				HWCCF_ERR("%s timeout\n", is_set ? "g_vote" : "g_unvote");
				ret = -HWV_VOTE_TIMEOUT;
				goto ERR;
			}
		} else {
			/* delay 100us for stable status */
			udelay(100);
		}
	}

	if (setclr_sta_ofs != 0) {
		ret = regmap_read_poll_timeout_atomic(regmap, setclr_sta_ofs, val, IS_MASK_CLR(val, done_ack_msk),
		    MTK_WAIT_GHWV_DONE_US, MTK_WAIT_GHWV_DONE_CNT);
		if (ret) {
			HWCCF_ERR("%s timeout\n", is_set ? "set_sta" : "clr_sta");
			ret = -HWV_SET_TIMEOUT;
			goto ERR;
		}
	}

	// Polling done
	ret = regmap_read_poll_timeout_atomic(regmap, done_ofs, val,
		IS_MASK_SET(val, done_ack_msk),
		MTK_WAIT_GHWV_DONE_US, MTK_WAIT_GHWV_DONE_CNT);
	if (ret) {
		HWCCF_ERR("%s timeout\n", is_set ? "set" : "clr");
		ret = -HWV_SET_TIMEOUT;
		goto ERR;
	}

skip:
	spin_unlock_irqrestore(&hwccf_lock, flags);
#ifdef HWCCF_TEST_MODE
	return 0;
#else
	return ret;
#endif

ERR:
	HWCCF_ERR("%s: vote[%x]=%x{%x}, done[%x]=%x{%x}\n", __func__,
		setclr_ofs, vote_val, setclr_ofs ? hwccf_read(regmap, en_ofs) : 0,
		done_ofs, done_ack_msk, done_ofs ? hwccf_read(regmap, done_ofs) : 0);
	spin_unlock_irqrestore(&hwccf_lock, flags);
	return ret;
}

/* clock voter support */
static int _v1_hwccf_voter_ctrl(struct regmap *regmap, uint32_t setclr_ofs, uint32_t vote_val,
					  uint32_t done_ofs, uint32_t done_ack_msk)
{
	unsigned long flags;
	bool is_set = IS_SET_FROM_VOTER_ADDR(setclr_ofs);
	uint32_t en_ofs = setclr_ofs + (is_set ? 0x8 : 0x4);
	int ret = 0;
	uint32_t val;

	// Check args
	if (!setclr_ofs || !vote_val || !done_ofs || !done_ack_msk) {
		HWCCF_ERR("bad args\n");
		return -HWV_EINVAL;
	}
	spin_lock_irqsave(&hwccf_lock, flags);
#ifdef HWCCF_TEST_MODE
	hwccf_read(regmap, setclr_ofs);
	goto skip;
#endif
	// Check repeat vote
	val = hwccf_read(regmap, en_ofs);
	if (is_set ? IS_MASK_SET(val, vote_val) : IS_MASK_CLR(val, vote_val)) {
		HWCCF_WARN("already %s, [%x]=%x{%x}\n",
			is_set ? "set" : "clr", setclr_ofs, vote_val, val);
		goto skip;
	}

	// Pre-Polling done
	ret = regmap_read_poll_timeout_atomic(regmap, done_ofs, val, IS_MASK_SET(val, done_ack_msk),
	    MTK_WAIT_GHWV_PREPARE_US, MTK_WAIT_GHWV_PREPARE_CNT);
	if (ret) {
		HWCCF_ERR("%s timeout\n", is_set ? "prepare" : "unprepare");
		ret = -HWV_PREPARE_TIMEOUT;
		goto ERR;
	}

	// XPU Voting & polling HW_CCF_XPU$_{CG/MTCMOS/PLL/BACKUP}_SET
	hwccf_write(regmap, setclr_ofs, vote_val);

	ret = regmap_read_poll_timeout_atomic(regmap, en_ofs, val,
		is_set ? IS_MASK_SET(val, vote_val) : IS_MASK_CLR(val, vote_val),
		MTK_WAIT_GHWV_VOTE_US, MTK_WAIT_GHWV_VOTE_CNT);
	if (ret) {
		HWCCF_ERR("%s timeout\n", is_set ? "vote" : "unvote");
		ret = -HWV_VOTE_TIMEOUT;
		goto ERR;
	}

	// Polling done
	ret = regmap_read_poll_timeout_atomic(regmap, done_ofs, val,
		IS_MASK_SET(val, done_ack_msk),
		MTK_WAIT_GHWV_VOTE_US, MTK_WAIT_GHWV_DONE_CNT);
	if (ret) {
		HWCCF_ERR("%s timeout\n", is_set ? "set" : "clr");
		ret = -HWV_SET_TIMEOUT;
		goto ERR;
	}

skip:
	spin_unlock_irqrestore(&hwccf_lock, flags);
#ifdef HWCCF_TEST_MODE
	return 0;
#else
	return ret;
#endif

ERR:
	HWCCF_ERR("%s: vote[%x]=%x{%x}, done[%x]=%x{%x}\n", __func__,
		setclr_ofs, vote_val, setclr_ofs ? hwccf_read(regmap, en_ofs) : 0,
		done_ofs, done_ack_msk, done_ofs ? hwccf_read(regmap, done_ofs) : 0);
	spin_unlock_irqrestore(&hwccf_lock, flags);
	return ret;
}

int v0_hwccf_voter_ctrl(enum HWCCF_TYPE hwccf_type, uint32_t resource_id, enum HWCCF_OP hwccf_op, uint32_t vote_bit)
{
	int ret = 0;
	uint32_t setclr_ofs = 0x0;
	uint32_t done_ofs = 0x0;
	struct regmap *map;

	map = ((hwccf_type == AP_HWCCF) ? regmaps[AP_HWCCF]:regmaps[MM_HWCCF]);


	if (resource_id <= HW_CCF_CG_GRP_14) {
		setclr_ofs = ((hwccf_op == HWCCF_VOTE) ? V0_CG_SET_OFS(resource_id) : V0_CG_CLR_OFS(resource_id));
		done_ofs = V0_CG_DONE_OFS(resource_id);
	} else if((resource_id > HW_CCF_CG_GRP_14) && (resource_id <= HW_CCF_CG_GRP_29)
														&& (hwccf_type == MM_HWCCF)) {
		setclr_ofs = ((hwccf_op == HWCCF_VOTE) ? V0_CG_SET_OFS(resource_id) : V0_CG_CLR_OFS(resource_id));
		done_ofs = V0_CG_DONE_OFS(resource_id);
	} else {
		ret = -HWV_WRONG_ID;
		goto ERR;
	}
	// common hwccf voting function
	ret = _v0_hwccf_voter_ctrl(map, setclr_ofs, BIT(vote_bit), done_ofs, BIT(vote_bit));
	if (ret)
		goto ERR;
	return ret;

ERR:
	HWCCF_ERR("HWCCF vote fail, error code: %x\n", abs(ret));
	configASSERT(0);
	return ret;
}


static int _v1_hwccf_voter_ctrl_wrapper(enum HWCCF_TYPE hwccf_type, uint32_t resource_id, enum HWCCF_OP hwccf_op, uint32_t vote_val)
{
	int ret = 0;
	uint32_t setclr_ofs = 0x0;
	uint32_t done_ofs = 0x0;
	struct regmap *map;

	map = ((hwccf_type == AP_HWCCF) ? regmaps[AP_HWCCF]:regmaps[MM_HWCCF]);


	if (resource_id <= HW_CCF_CG_GRP_29) {
		setclr_ofs = ((hwccf_op == HWCCF_VOTE) ? CG_SET_OFS(resource_id) : CG_CLR_OFS(resource_id));
		done_ofs = CG_DONE_OFS(resource_id);
	} else if((resource_id > HW_CCF_CG_GRP_29) && (resource_id <= HW_CCF_CG_GRP_51)
														&& (hwccf_type == MM_HWCCF)) {
		setclr_ofs = ((hwccf_op == HWCCF_VOTE) ? CG_SET_OFS(resource_id) : CG_CLR_OFS(resource_id));
		done_ofs = CG_DONE_OFS(resource_id);
	} else if (resource_id == HW_CCF_MTCMOS_GRP_0) {
		setclr_ofs = ((hwccf_op == HWCCF_VOTE) ? MTCMOS0_SET_OFS : MTCMOS0_CLR_OFS);
		done_ofs = MTCMOS0_DONE_OFS;
	} else if (resource_id == HW_CCF_MTCMOS_GRP_1) {
		setclr_ofs = ((hwccf_op == HWCCF_VOTE) ? MTCMOS1_SET_OFS : MTCMOS1_CLR_OFS);
		done_ofs = MTCMOS1_DONE_OFS;
	} else if (resource_id == HW_CCF_MUX_PWR_GRP_0) {
		setclr_ofs = ((hwccf_op == HWCCF_VOTE) ? MUX_PWR_SET_OFS : MUX_PWR_CLR_OFS);
		done_ofs = MUX_PWR_DONE_OFS;
	} else {
		ret = -HWV_WRONG_ID;
		goto ERR;
	}
	// common hwccf voting function
	ret = _v1_hwccf_voter_ctrl(map, setclr_ofs, vote_val, done_ofs, vote_val);
	if (ret)
		goto ERR;
	return ret;

ERR:
	HWCCF_ERR("HWCCF vote fail, error code: %x\n", abs(ret));
	configASSERT(0);
	return ret;
}

int v1_hwccf_voter_ctrl(enum HWCCF_TYPE hwccf_type, uint32_t resource_id, enum HWCCF_OP hwccf_op, uint32_t vote_bit)
{
    return _v1_hwccf_voter_ctrl_wrapper( hwccf_type, resource_id, hwccf_op, BIT(vote_bit));
}

int v1_hwccf_multi_voter_ctrl(enum HWCCF_TYPE hwccf_type, uint32_t resource_id, enum HWCCF_OP hwccf_op, uint32_t vote_val)
{
    return _v1_hwccf_voter_ctrl_wrapper( hwccf_type, resource_id, hwccf_op, vote_val);
}

int v0_raw_hwccf_voter_ctrl(struct cb_params *params)
{
	int ret = 0;

	// common hwccf voting function
	ret = _v0_hwccf_voter_ctrl(params->regmap, V0_CCF_XPU(HWV_XPU_0) + params->setclr_ofs,
			BIT(params->vote_bit), params->done_ofs, BIT(params->vote_bit));
	if (ret)
		goto ERR;
	return ret;
#ifdef CLK_RES_IS_CG
	//readx_poll_timeout_us(CG);
#endif

ERR:
	HWCCF_ERR("HWCCF vote fail, error code: %x\n", abs(ret));
	configASSERT(0);
	return ret;
}

int v1_raw_hwccf_voter_ctrl(struct cb_params *params)
{
	int ret = 0;

	// common hwccf voting function
	ret = _v1_hwccf_voter_ctrl(params->regmap, CCF_XPU(HWV_XPU_0) + params->setclr_ofs,
			BIT(params->vote_bit), params->done_ofs, BIT(params->vote_bit));

	if (ret)
		goto ERR;
	return ret;
#ifdef CLK_RES_IS_CG
	//readx_poll_timeout_us(CG);
#endif

ERR:
	HWCCF_ERR("HWCCF vote fail, error code: %x\n", abs(ret));
	configASSERT(0);
	return ret;
}

static int _v1_hwccf_is_enabled(struct regmap *regmap, uint32_t setclr_ofs, uint32_t vote_val,
		uint32_t done_ofs, uint32_t done_ack_msk)
{
	bool is_set = IS_SET_FROM_VOTER_ADDR(setclr_ofs);
	uint32_t en_ofs = setclr_ofs + (is_set ? 0x8 : 0x4);
	uint32_t all_status_ofs = 0;
	uint32_t val;

	// Check args
	if (!setclr_ofs || !vote_val || !done_ofs || !done_ack_msk) {
		HWCCF_ERR("bad args\n");
		return 0;
	}

	// XPU en_ofs replaced by ALL_XXX due to autolink
	switch (en_ofs & 0xFFF) {
		case 0x708:
			en_ofs = CCF_ALL_MTCMOS_EN_0;
			all_status_ofs = CCF_ALL_MTCMOS_STA_0;
			break;
		case 0x714:
			en_ofs = CCF_ALL_MTCMOS_EN_1;
			all_status_ofs = CCF_ALL_MTCMOS_STA_1;
			break;
		case 0x608:
			en_ofs = CCF_ALL_MUX_EN;
			all_status_ofs = CCF_ALL_MUX_STA;
			break;
		default:
			break;
	}

	val = hwccf_read(regmap, en_ofs);
	if (IS_MASK_SET(val, vote_val)) {

		// Polling done
		val = (all_status_ofs ? hwccf_read(regmap, all_status_ofs) : hwccf_read(regmap, done_ofs));
		return (all_status_ofs ? IS_MASK_SET(~val, done_ack_msk) : IS_MASK_SET(val, done_ack_msk));
	}
	return 0;
}

int v1_raw_hwccf_is_enabled(struct cb_params *params)
{
	return _v1_hwccf_is_enabled(params->regmap, CCF_XPU(HWV_XPU_0) + params->setclr_ofs, BIT(params->vote_bit), params->done_ofs, BIT(params->vote_bit));
}

int v1_hwccf_is_enabled(enum HWCCF_TYPE hwccf_type, uint32_t resource_id, enum HWCCF_OP hwccf_op, uint32_t vote_bit)
{
	uint32_t setclr_ofs = 0x0;
	uint32_t done_ofs = 0x0;
	struct regmap *map;

	map = ((hwccf_type == AP_HWCCF) ? regmaps[AP_HWCCF]:regmaps[MM_HWCCF]);

	if (resource_id <= HW_CCF_CG_GRP_29) {
		setclr_ofs = ((hwccf_op == HWCCF_VOTE) ? CG_SET_OFS(resource_id) : CG_CLR_OFS(resource_id));
		done_ofs = CG_DONE_OFS(resource_id);
	} else if((resource_id > HW_CCF_CG_GRP_29) && (resource_id <= HW_CCF_CG_GRP_51)
													&& (hwccf_type == MM_HWCCF)) {
		setclr_ofs = ((hwccf_op == HWCCF_VOTE) ? CG_SET_OFS(resource_id) : CG_CLR_OFS(resource_id));
		done_ofs = CG_DONE_OFS(resource_id);
	} else if (resource_id == HW_CCF_MTCMOS_GRP_0) {
		setclr_ofs = ((hwccf_op == HWCCF_VOTE) ? MTCMOS0_SET_OFS : MTCMOS0_CLR_OFS);
		done_ofs = MTCMOS0_DONE_OFS;
	} else if (resource_id == HW_CCF_MTCMOS_GRP_1) {
		setclr_ofs = ((hwccf_op == HWCCF_VOTE) ? MTCMOS1_SET_OFS : MTCMOS1_CLR_OFS);
		done_ofs = MTCMOS1_DONE_OFS;
	} else if (resource_id == HW_CCF_MUX_PWR_GRP_0) {
		setclr_ofs = ((hwccf_op == HWCCF_VOTE) ? MUX_PWR_SET_OFS : MUX_PWR_CLR_OFS);
		done_ofs = MUX_PWR_DONE_OFS;
	} else {
		HWCCF_ERR("-HWV_WRONG_ID\n");
		return 0;
	}

	return _v1_hwccf_is_enabled(map, setclr_ofs, BIT(vote_bit), done_ofs, BIT(vote_bit));
}

static int _v0_hwccf_irq_voter_nowait(struct regmap *regmap, uint32_t setclr_ofs, uint32_t vote_val,
						uint32_t done_ofs, uint32_t done_ack_msk)
{
	unsigned long flags;
	bool is_set = V0_IS_SET_FROM_VOTER_ADDR(setclr_ofs);
	uint32_t en_ofs = setclr_ofs;
	int ret = 0;
	uint32_t val;

	// Check args
	if (!setclr_ofs || !vote_val || !done_ofs || !done_ack_msk) {
		HWCCF_ERR("bad args\n");
		return -HWV_EINVAL;
	}

	// Profling start
	HWCCF_PROFILE_DECLARE(nowait);
	HWCCF_PROFILE_RESET(nowait);
	HWCCF_PROFILE_START(nowait);

	spin_lock_irqsave(&hwccf_irq_lock, flags);
	// Check repeat vote
	val = hwccf_read(regmap, en_ofs);
	if (is_set ? IS_MASK_SET(val, vote_val) : IS_MASK_CLR(val, vote_val)) {
		HWCCF_ERR("vote repeat\n");
		HWCCF_ERR("%x = %x, %x = %x, %d\n", setclr_ofs, vote_val, en_ofs, val, is_set);
		goto skip;
	}

	// Pre-Polling done
	ret = regmap_read_poll_timeout_atomic(regmap, done_ofs, val, IS_MASK_SET(val, done_ack_msk),
		MTK_WAIT_GHWV_IRQ_PREPARE_US, MTK_WAIT_GHWV_IRQ_PREPARE_CNT);

	if (ret) {
		HWCCF_ERR("%s timeout\n", is_set ? "prepare" : "unprepare");
		ret = -HWV_PREPARE_TIMEOUT;
		goto ERR;
	}

	// XPU Voting & polling HW_CCF_XPU$_{CG/MTCMOS/PLL/BACKUP}_SET
	hwccf_write(regmap, setclr_ofs, vote_val);
	ret = regmap_read_poll_timeout_atomic(regmap, en_ofs, val,
		is_set ? IS_MASK_SET(val, vote_val) : IS_MASK_CLR(val, vote_val),
		MTK_WAIT_GHWV_IRQ_VOTE_US, MTK_WAIT_GHWV_IRQ_VOTE_CNT);

	if (ret) {
		HWCCF_ERR("%s timeout\n", is_set ? "vote" : "unvote");
		ret = -HWV_VOTE_TIMEOUT;
		goto ERR;
	}

skip:
	spin_unlock_irqrestore(&hwccf_irq_lock, flags);
	// Profling End
	HWCCF_PROFILE_END(nowait);
	//HWCCF_PROFILE_PRINT(nowait);
	return ret;

ERR:
	HWCCF_ERR("%s: vote[%x]=%x{%x}, done[%x]=%x{%x}\n", __func__,
		setclr_ofs, vote_val,     setclr_ofs ? hwccf_read(regmap, en_ofs) : 0,
		done_ofs, done_ack_msk, done_ofs ? hwccf_read(regmap, done_ofs) : 0);
	spin_unlock_irqrestore(&hwccf_irq_lock, flags);
	HWCCF_PROFILE_END(nowait);
	HWCCF_PROFILE_PRINT(nowait);
	return ret;
}

static int _v1_hwccf_irq_voter_nowait(struct regmap *regmap, uint32_t setclr_ofs, uint32_t vote_val,
						uint32_t done_ofs, uint32_t done_ack_msk, uint32_t sta_ofs)
{
	bool is_set = IS_SET_FROM_VOTER_ADDR(setclr_ofs);
	uint32_t en_ofs = setclr_ofs + (is_set ? 0x8 : 0x4);
	int ret = 0;
	uint32_t val;

	// Check args
	if (!setclr_ofs || !vote_val || !done_ofs || !done_ack_msk) {
		HWCCF_ERR("bad args\n");
		return -HWV_EINVAL;
	}

	// Profling start
	HWCCF_PROFILE_DECLARE(nowait);
	HWCCF_PROFILE_RESET(nowait);
	HWCCF_PROFILE_START(nowait);

	// Check repeat vote
	val = hwccf_read(regmap, en_ofs);
	if (is_set ? IS_MASK_SET(val, vote_val) : IS_MASK_CLR(val, vote_val)) {
		HWCCF_ERR("vote repeat\n");
		HWCCF_ERR("%x = %x, %x = %x, %d\n", setclr_ofs, vote_val, en_ofs, val, is_set);
		goto skip;
	}

	// Pre-Polling sta
	ret = regmap_read_poll_timeout_atomic(regmap, sta_ofs, val,
		IS_MASK_CLR(val, done_ack_msk),
		MTK_WAIT_GHWV_IRQ_PREPARE_US, MTK_WAIT_GHWV_IRQ_PREPARE_CNT);

	if (ret) {
		HWCCF_ERR("%s polling sta timeout\n", is_set ? "prepare" : "unprepare");
		ret = -HWV_PREPARE_TIMEOUT;
		goto ERR;
	}

	// XPU Voting & polling HW_CCF_XPU$_{CG/MTCMOS/PLL/BACKUP}_SET
	hwccf_write(regmap, setclr_ofs, vote_val);
	ret = regmap_read_poll_timeout_atomic(regmap, en_ofs, val,
		is_set ? IS_MASK_SET(val, vote_val) : IS_MASK_CLR(val, vote_val),
		MTK_WAIT_GHWV_IRQ_VOTE_US, MTK_WAIT_GHWV_IRQ_VOTE_CNT);

	if (ret) {
		HWCCF_ERR("%s timeout\n", is_set ? "vote" : "unvote");
		ret = -HWV_VOTE_TIMEOUT;
		goto ERR;
	}

skip:
	// Profling End
	HWCCF_PROFILE_END(nowait);
	HWCCF_PROFILE_PRINT(nowait);
	return ret;

ERR:
	HWCCF_ERR("%s: vote[%x]=%x{%x}, done[%x]=%x{%x}\n", __func__,
		setclr_ofs, vote_val,     setclr_ofs ? hwccf_read(regmap, en_ofs) : 0,
		done_ofs, done_ack_msk, done_ofs ? hwccf_read(regmap, done_ofs) : 0);
	HWCCF_PROFILE_END(nowait);
	HWCCF_PROFILE_PRINT(nowait);
	return ret;
}


static int _v0_hwccf_irq_voter_wait_done(struct regmap *regmap, uint32_t setclr_ofs, uint32_t vote_val,
							uint32_t done_ofs, uint32_t done_ack_msk)
{
	bool is_set = V0_IS_SET_FROM_VOTER_ADDR(setclr_ofs);
	uint32_t en_ofs = setclr_ofs;
	uint32_t setclr_sta_ofs = 0, global_en_ofs = 0;
	int ret = 0;
	uint32_t val;

	switch (setclr_ofs & 0xFFF) {
		case 0x218:
			setclr_sta_ofs = V0_XPU_MTCMOS0_SET_STA;
			global_en_ofs = V0_CCF_MTCMOS_EN_0;
			break;
		case 0x21C:
			setclr_sta_ofs = V0_XPU_MTCMOS0_CLR_STA;
			global_en_ofs = V0_CCF_MTCMOS_EN_0;
			break;
		case 0x220:
			setclr_sta_ofs = V0_XPU_MTCMOS1_SET_STA;
			global_en_ofs = V0_CCF_MTCMOS_EN_1;
			break;
		case 0x224:
			setclr_sta_ofs = V0_XPU_MTCMOS1_CLR_STA;
			global_en_ofs = V0_CCF_MTCMOS_EN_1;
			break;
		case 0x210:
			setclr_sta_ofs = V0_PLL_SET_STA;
			global_en_ofs = V0_CCF_PLL_EN;
			break;
		case 0x214:
			setclr_sta_ofs = V0_PLL_CLR_STA;
			global_en_ofs = V0_CCF_PLL_EN;
			break;
		case 0x230:
			setclr_sta_ofs = V0_CCF_BACKUP1_SET_STA;
			global_en_ofs = V0_CCF_BACKUP1_EN;
			break;
		case 0x234:
			setclr_sta_ofs = V0_CCF_BACKUP1_CLR_STA;
			global_en_ofs = V0_CCF_BACKUP1_EN;
			break;
		case 0x238:
			setclr_sta_ofs = V0_CCF_BACKUP2_SET_STA;
			global_en_ofs = V0_CCF_BACKUP2_EN;
			break;
		case 0x23C:
			setclr_sta_ofs = V0_CCF_BACKUP2_CLR_STA;
			global_en_ofs = V0_CCF_BACKUP2_EN;
			break;
		default:
			break;
	}

	// Profling start
	HWCCF_PROFILE_DECLARE(wait_done);
	HWCCF_PROFILE_RESET(wait_done);
	HWCCF_PROFILE_START(wait_done);


	if (global_en_ofs != 0) {
		if (is_set) {
			ret = regmap_read_poll_timeout_atomic(regmap, global_en_ofs, val,
				IS_MASK_SET(val, vote_val),
				MTK_WAIT_GHWV_DONE_US, MTK_WAIT_GHWV_DONE_CNT);
			if (ret) {
				HWCCF_ERR("%s timeout\n", is_set ? "g_vote" : "g_unvote");
				ret = -HWV_VOTE_TIMEOUT;
				goto ERR;
			}
		} else {
			/* delay 100us for stable status */
			udelay(100);
		}
	}

	if (setclr_sta_ofs) {
		// Polling set/clr_sta
		ret = regmap_read_poll_timeout_atomic(regmap, setclr_sta_ofs, val,
			IS_MASK_CLR(val, vote_val),
			MTK_WAIT_GHWV_IRQ_DONE_US, MTK_WAIT_GHWV_IRQ_DONE_CNT);
		if (ret) {
			HWCCF_ERR("%s timeout\n", is_set ? "set_sta" : "clr_sta");
			ret = -HWV_SET_TIMEOUT;
			goto ERR;
		}
	}

	// Polling done
	ret = regmap_read_poll_timeout_atomic(regmap, done_ofs, val,
		IS_MASK_SET(val, done_ack_msk),
		MTK_WAIT_GHWV_IRQ_DONE_US, MTK_WAIT_GHWV_IRQ_DONE_CNT);
	if (ret) {
		HWCCF_ERR("%s timeout\n", is_set ? "set" : "clr");
		ret = -HWV_SET_TIMEOUT;
		goto ERR;
	}

	// Profling End
	HWCCF_PROFILE_END(wait_done);
	//HWCCF_PROFILE_PRINT(wait_done);
	return ret;

ERR:
	HWCCF_ERR("%s: vote[%x]=%x{%x}, done[%x]=%x{%x}\n", __func__,
		setclr_ofs, vote_val,     setclr_ofs ? hwccf_read(regmap, en_ofs) : 0,
		done_ofs, done_ack_msk, done_ofs ? hwccf_read(regmap, done_ofs) : 0);
	HWCCF_PROFILE_END(wait_done);
	HWCCF_PROFILE_PRINT(wait_done);
	return ret;
}


static int _v1_hwccf_irq_voter_wait_done(struct regmap *regmap, uint32_t setclr_ofs, uint32_t vote_val,
		uint32_t done_ofs, uint32_t done_ack_msk, uint32_t sta_ofs, uint32_t all_en_ofs)
{
	bool is_set = IS_SET_FROM_VOTER_ADDR(setclr_ofs);
	uint32_t en_ofs = setclr_ofs + (is_set ? 0x8 : 0x4);
	int ret = 0;
	uint32_t val;
	int i;

	// Profling start
	HWCCF_PROFILE_DECLARE(wait_done);
	HWCCF_PROFILE_RESET(wait_done);
	HWCCF_PROFILE_START(wait_done);

	// Polling all_en
	if (is_set) {
		for (i = 0; i <= MTK_WAIT_GHWV_MTCMOS_DONE_CNT; i++) {
			val = hwccf_read(regmap, all_en_ofs);
			udelay(MTK_WAIT_GHWV_MTCMOS_DONE_US);

			if (IS_MASK_SET(val, done_ack_msk))
				break;

			if (i == MTK_WAIT_GHWV_MTCMOS_DONE_CNT) {
				HWCCF_ERR("%s polling all_en timeout\n", is_set ? "set" : "clr");
				goto ERR;
			}
		}
	} else {
		udelay(100);
	}


	// Polling sta
	ret = regmap_read_poll_timeout_atomic(regmap, sta_ofs, val,
		IS_MASK_CLR(val, done_ack_msk),
		MTK_WAIT_GHWV_IRQ_DONE_US, MTK_WAIT_GHWV_IRQ_DONE_CNT);

	if (ret) {
		HWCCF_ERR("%s polling sta timeout\n", is_set ? "set" : "clr");
		ret = -HWV_SET_TIMEOUT;
		goto ERR;
	}

	// Profling End
	HWCCF_PROFILE_END(wait_done);
	HWCCF_PROFILE_PRINT(wait_done);
	return ret;

ERR:
	HWCCF_ERR("%s: vote[%x]=%x{%x}, done[%x]=%x{%x}\n", __func__,
		setclr_ofs, vote_val,     setclr_ofs ? hwccf_read(regmap, en_ofs) : 0,
		done_ofs, done_ack_msk, done_ofs ? hwccf_read(regmap, done_ofs) : 0);
	HWCCF_PROFILE_END(wait_done);
	HWCCF_PROFILE_PRINT(wait_done);
	return ret;
}

static int _v0_hwccf_irq_voter_ctrl(struct regmap *regmap, uint32_t setclr_ofs, uint32_t vote_val,
                         uint32_t done_ofs)
{
	int ret = 0;

	// Voting hwccf irq voter without waiting hwccf done bit
	ret = _v0_hwccf_irq_voter_nowait(regmap, setclr_ofs, vote_val, done_ofs, vote_val);

	if (ret)
		goto ERR;

	// Polling hwccf done bit
	ret = _v0_hwccf_irq_voter_wait_done(regmap, setclr_ofs, vote_val, done_ofs, vote_val);

	if (ret)
		goto ERR;

	return ret;

ERR:
	configASSERT(0);
	return ret;
}

static int _v1_hwccf_irq_voter_ctrl(struct regmap *regmap, uint32_t setclr_ofs, uint32_t vote_val,
			uint32_t done_ofs, uint32_t sta_ofs, uint32_t all_en_ofs)
{
	int ret = 0;
	unsigned long flags;
	//CCF_XPU(HWV_XPU_0)

	spin_lock_irqsave(&hwccf_irq_lock, flags);
	// Voting hwccf irq voter without waiting hwccf done bit
	ret = _v1_hwccf_irq_voter_nowait(regmap, setclr_ofs, vote_val, done_ofs, vote_val, sta_ofs);

	if (ret)
		goto ERR;

	// Polling hwccf done bit
	ret = _v1_hwccf_irq_voter_wait_done(regmap, setclr_ofs, vote_val, done_ofs, vote_val, sta_ofs, all_en_ofs);

	if (ret)
		goto ERR;

	spin_unlock_irqrestore(&hwccf_irq_lock, flags);

	return ret;

ERR:
	spin_unlock_irqrestore(&hwccf_irq_lock, flags);
	configASSERT(0);
	return ret;
}

int _v0_hwccf_irq_voter_ctrl_wrapper(enum HWCCF_TYPE hwccf_type, uint32_t resource_id, enum HWCCF_OP hwccf_op, uint32_t vote_val)
{
	int ret = 0;
	uint32_t setclr_ofs = 0x0;
	uint32_t done_ofs = 0x0;
	struct regmap *map;

	map = ((hwccf_type == AP_HWCCF) ? regmaps[AP_HWCCF]:regmaps[MM_HWCCF]);


	if (resource_id == HW_CCF_MTCMOS_GRP_0) {
		setclr_ofs = ((hwccf_op == HWCCF_VOTE) ? V0_MTCMOS0_SET_OFS : V0_MTCMOS0_CLR_OFS);
		done_ofs = V0_MTCMOS0_DONE_OFS;
	} else if (resource_id == HW_CCF_MTCMOS_GRP_1) {
		setclr_ofs = ((hwccf_op == HWCCF_VOTE) ? V0_MTCMOS1_SET_OFS : V0_MTCMOS1_CLR_OFS);
		done_ofs = V0_MTCMOS1_DONE_OFS;
	} else if (resource_id == HW_CCF_BACKUP_GRP_1) {
		setclr_ofs = ((hwccf_op == HWCCF_VOTE) ? V0_XPU_B1_SET : V0_XPU_B1_CLR);
		done_ofs = V0_XPU_B1_DONE;
	} else if (resource_id == HW_CCF_BACKUP_GRP_2) {
		setclr_ofs = ((hwccf_op == HWCCF_VOTE) ? V0_XPU_B2_SET : V0_XPU_B2_CLR);
		done_ofs = V0_XPU_B2_DONE;
	} else if (resource_id == HW_CCF_PLL) {
		setclr_ofs = ((hwccf_op == HWCCF_VOTE) ? V0_XPU_PLL_SET : V0_XPU_PLL_CLR);

		done_ofs = V0_CCF_PLL_DONE;
	} else {
		ret = -HWV_WRONG_ID;
		goto ERR;
	}

	// common hwccf voting function
	ret = _v0_hwccf_irq_voter_ctrl(map, setclr_ofs, vote_val, done_ofs);
	if (ret)
		goto ERR;
	return ret;

ERR:
	HWCCF_ERR("HWCCF IRQ vote fail, error code: %x\n", abs(ret));
	configASSERT(0);
	return ret;
}

int v0_hwccf_irq_voter_ctrl(enum HWCCF_TYPE hwccf_type, uint32_t resource_id, enum HWCCF_OP hwccf_op, uint32_t vote_bit)
{
    return _v0_hwccf_irq_voter_ctrl_wrapper( hwccf_type, resource_id, hwccf_op, BIT(vote_bit));
}

int v0_hwccf_irq_multi_voter_ctrl(enum HWCCF_TYPE hwccf_type, uint32_t resource_id, enum HWCCF_OP hwccf_op, uint32_t vote_val)
{
    return _v0_hwccf_irq_voter_ctrl_wrapper( hwccf_type, resource_id, hwccf_op, vote_val);
}

int _v1_hwccf_irq_voter_ctrl_wrapper(enum HWCCF_TYPE hwccf_type, uint32_t resource_id, enum HWCCF_OP hwccf_op, uint32_t vote_val)
{
	int ret = 0;
	uint32_t setclr_ofs = 0x0;
	uint32_t done_ofs = 0x0;
	uint32_t sta_ofs = 0x0;
	uint32_t all_en_ofs = 0x0;
	struct regmap *map;

	map = ((hwccf_type == AP_HWCCF) ? regmaps[AP_HWCCF]:regmaps[MM_HWCCF]);

	if (resource_id == HW_CCF_BACKUP_GRP_0) {
		setclr_ofs = ((hwccf_op == HWCCF_VOTE) ? XPU_B0_SET : XPU_B0_CLR);
		done_ofs = XPU_B0_DONE;
		sta_ofs = XPU_B0_STA;
		all_en_ofs = XPU_B0_ALL_EN;
	} else if (resource_id == HW_CCF_BACKUP_GRP_1) {
		setclr_ofs = ((hwccf_op == HWCCF_VOTE) ? XPU_B1_SET : XPU_B1_CLR);
		done_ofs = XPU_B1_DONE;
		sta_ofs = XPU_B1_STA;
		all_en_ofs = XPU_B1_ALL_EN;
	} else if (resource_id == HW_CCF_BACKUP_GRP_2) {
		setclr_ofs = ((hwccf_op == HWCCF_VOTE) ? XPU_B2_SET : XPU_B2_CLR);
		done_ofs = XPU_B2_DONE;
		sta_ofs = XPU_B2_STA;
		all_en_ofs = XPU_B2_ALL_EN;
	} else {
		ret = -HWV_WRONG_ID;
		goto ERR;
	}
	// common hwccf voting function
	ret = _v1_hwccf_irq_voter_ctrl(map, setclr_ofs, vote_val, done_ofs, sta_ofs, all_en_ofs);
	if (ret)
		goto ERR;
	return ret;

ERR:
	HWCCF_ERR("HWCCF IRQ vote fail, error code: %x\n", abs(ret));
	configASSERT(0);
	return ret;
}

int v1_hwccf_irq_voter_ctrl(enum HWCCF_TYPE hwccf_type, uint32_t resource_id, enum HWCCF_OP hwccf_op, uint32_t vote_bit)
{
    return _v1_hwccf_irq_voter_ctrl_wrapper( hwccf_type, resource_id, hwccf_op, BIT(vote_bit));
}

int v1_hwccf_irq_multi_voter_ctrl(enum HWCCF_TYPE hwccf_type, uint32_t resource_id, enum HWCCF_OP hwccf_op, uint32_t vote_val)
{
    return _v1_hwccf_irq_voter_ctrl_wrapper( hwccf_type, resource_id, hwccf_op, vote_val);
}

void v1_hwccf_freeze(int is_MASK_XPC, struct regmap *regmap)
{
	unsigned long flags;
	int ret = 0;
	uint32_t val;
	uint32_t irq_fsm = (hw_ccf_to_up_int_b_0 |
					   hw_ccf_to_up_int_b_1 |
					   hw_ccf_to_up_int_b_2 |
					   hw_ccf_to_up_int_b_3);

	spin_lock_irqsave(&hwccf_lock, flags);
	hwccf_update_bit(regmap, HWCCF_REG_EN6, block_voting, block_voting);

	// Polling FSM to idle
	ret = regmap_read_poll_timeout_atomic(regmap, CCF_STATUS, val, IS_MASK_SET(val, hwccf_idle),
	    MTK_WAIT_GHWV_PREPARE_US, MTK_WAIT_GHWV_PREPARE_CNT);

	if (ret) {
		HWCCF_ERR("hwccf_idle timeout\n");
		ret = -HWV_PREPARE_TIMEOUT;
		goto ERR;
	}

	if (is_MASK_XPC) {
		hwccf_write(regmap, CCF_MASK_HW_MTCMOS_REQ_0, u32_all_MASK);
		hwccf_write(regmap, CCF_MASK_HW_IRQ_REQ_0, u32_all_MASK);
	}

	// Polling no irq
	ret = regmap_read_poll_timeout_atomic(regmap, CCF_OUTPUT_STATUS, val, IS_MASK_SET(val, irq_fsm),
	    MTK_WAIT_GHWV_PREPARE_US, MTK_WAIT_GHWV_PREPARE_CNT);

	if (ret) {
		HWCCF_ERR("HWCCF Freeze timeout\n");
		ret = -HWV_PREPARE_TIMEOUT;
		goto ERR;
	}
	spin_unlock_irqrestore(&hwccf_lock, flags);
	return;

ERR:
	spin_unlock_irqrestore(&hwccf_lock, flags);
	HWCCF_ERR("HWCCF freeze fail, error code: %x\n", abs(ret));
	configASSERT(0);
	return;

}

void v1_hwccf_unfreeze(int is_MASK_XPC, struct regmap *regmap)
{
	if (is_MASK_XPC) {
		hwccf_write(regmap, CCF_MASK_HW_MTCMOS_REQ_0, 0);
		hwccf_write(regmap, CCF_MASK_HW_IRQ_REQ_0, 0);
	}

	hwccf_update_bit(regmap, HWCCF_REG_EN6, block_voting, 0);
}

static int hwccf_drv_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	const struct of_device_id *match;
	const struct hwccf_match_data *match_data;
	int ret = 0;

	/* get match compatible node */
	match = of_match_node(pdev->dev.driver->of_match_table, node);
	if (!match) {
		HWCCF_ERR("No matching HWCCF found\n");
		return 0;
	}
	match_data = (const struct hwccf_match_data *)match->data;


	/* get regmap info */
	if (regmap_count >= MAX_HWCCF) {
		HWCCF_ERR("Too many regmaps\n");
		return -ENOMEM;
	}

	regmaps[regmap_count] = syscon_node_to_regmap(node);
	if ((regmaps[regmap_count] == NULL) || IS_ERR(regmaps[regmap_count])) {
		HWCCF_ERR("Failed to get regmap for node %s\n", node->name);
		configASSERT(0);
		return PTR_ERR(regmaps[regmap_count]);
	}

	regmap_count++;

	HWCCF_LOG("regmap node %s\n", node->name);

	/* check current regmap info == required match_data */
	if (regmap_count == match_data->required_regmaps) {

		/* set platform ops */
		hwccf_ops = match_data->ops;

		ret = register_mtk_clk_external_api_cb(CLK_REQUEST_RAW_HWCCF_VOTER_CB, hwccf_ops->raw_hwccf_voter_ctrl,
												"register_raw_hwccf_voter_ctrl fail");
		if (ret) {
			HWCCF_ERR("Failed to register raw_hwccf_voter_ctrl callback\n");
		}

		ret = register_mtk_clk_external_api_cb(CLK_REQUEST_RAW_HWCCF_IS_ENABLED_CB, hwccf_ops->raw_hwccf_is_enabled,
												"register_raw_hwccf_is_enabled fail");
		if (ret) {
			HWCCF_ERR("Failed to register raw_hwccf_is_enabled callback\n");
		}

		HWCCF_LOG("hwccf init done\n");
		_inited = true;
	}

	ret = hwccf_proc_dbg_register();

	if (ret)
		HWCCF_ERR("Failed to register proc/hwccf_dbg\n");

	return 0;
}

int hwccf_voter_ctrl(enum HWCCF_TYPE hwccf_type, uint32_t resource_id, enum HWCCF_OP hwccf_op, uint32_t vote_bit)
{
	if (!_inited) {
		HWCCF_ERR("_inited FAILED!\n");
		return -HWV_EINVAL;
	}
	if (hwccf_ops && hwccf_ops->hwccf_voter_ctrl) {
		return hwccf_ops->hwccf_voter_ctrl(hwccf_type, resource_id, hwccf_op, vote_bit);
	}
    return -EINVAL;
}
EXPORT_SYMBOL(hwccf_voter_ctrl);

int hwccf_multi_voter_ctrl(enum HWCCF_TYPE hwccf_type, uint32_t resource_id, enum HWCCF_OP hwccf_op, uint32_t vote_val)
{
	if (!_inited) {
		HWCCF_ERR("_inited FAILED!\n");
		return -HWV_EINVAL;
	}
	if (hwccf_ops && hwccf_ops->hwccf_multi_voter_ctrl) {
		return hwccf_ops->hwccf_multi_voter_ctrl(hwccf_type, resource_id, hwccf_op, vote_val);
	}
    return -EINVAL;
}
EXPORT_SYMBOL(hwccf_multi_voter_ctrl);

int raw_hwccf_voter_ctrl(struct cb_params *params)
{
	if (!_inited) {
		HWCCF_ERR("_inited FAILED!\n");
		return -HWV_EINVAL;
	}
	if (hwccf_ops && hwccf_ops->raw_hwccf_voter_ctrl) {
		return hwccf_ops->raw_hwccf_voter_ctrl(params);
	}
	return -EINVAL;
}
EXPORT_SYMBOL(raw_hwccf_voter_ctrl);

void hwccf_freeze(int is_MASK_XPC, struct regmap *regmap)
{
	if (!_inited) {
		HWCCF_ERR("_inited FAILED!\n");
		return;
	}
	if (hwccf_ops && hwccf_ops->hwccf_freeze) {
		return hwccf_ops->hwccf_freeze(is_MASK_XPC, regmap);
	}
	return;
}
EXPORT_SYMBOL(hwccf_freeze);


void hwccf_unfreeze(int is_MASK_XPC, struct regmap *regmap)
{
	if (!_inited) {
		HWCCF_ERR("_inited FAILED!\n");
		return;
	}
	if (hwccf_ops && hwccf_ops->hwccf_unfreeze) {
		return hwccf_ops->hwccf_unfreeze(is_MASK_XPC, regmap);
	}
	return;
}
EXPORT_SYMBOL(hwccf_unfreeze);

int hwccf_irq_voter_ctrl(enum HWCCF_TYPE hwccf_type, uint32_t resource_id, enum HWCCF_OP hwccf_op, uint32_t vote_bit)
{
	if (!_inited) {
		HWCCF_ERR("_inited FAILED!\n");
		return -HWV_EINVAL;
	}
	if (hwccf_ops && hwccf_ops->hwccf_irq_voter_ctrl) {
		return hwccf_ops->hwccf_irq_voter_ctrl(hwccf_type, resource_id, hwccf_op, vote_bit);
	}
	return -EINVAL;
}
EXPORT_SYMBOL(hwccf_irq_voter_ctrl);

int hwccf_irq_multi_voter_ctrl(enum HWCCF_TYPE hwccf_type, uint32_t resource_id, enum HWCCF_OP hwccf_op, uint32_t vote_val)
{
	if (!_inited) {
		HWCCF_ERR("_inited FAILED!\n");
		return -HWV_EINVAL;
	}
	if (hwccf_ops && hwccf_ops->hwccf_irq_multi_voter_ctrl) {
		return hwccf_ops->hwccf_irq_multi_voter_ctrl(hwccf_type, resource_id, hwccf_op, vote_val);
	}
	return -EINVAL;
}
EXPORT_SYMBOL(hwccf_irq_multi_voter_ctrl);

int hwccf_is_enabled(enum HWCCF_TYPE hwccf_type, uint32_t resource_id, enum HWCCF_OP hwccf_op, uint32_t vote_bit)
{
	if (!_inited) {
		HWCCF_ERR("_inited FAILED!\n");
		return 0;
	}
	if (hwccf_ops && hwccf_ops->hwccf_is_enabled) {
		return hwccf_ops->hwccf_is_enabled(hwccf_type, resource_id, hwccf_op, vote_bit);
	}

	HWCCF_ERR("ops NULL\n");

	return 0;
}
EXPORT_SYMBOL(hwccf_is_enabled);

int raw_hwccf_is_enabled(struct cb_params *params)
{
	if (!_inited) {
		HWCCF_ERR("_inited FAILED!\n");
		return 0;
	}
	if (hwccf_ops && hwccf_ops->raw_hwccf_is_enabled) {
		return hwccf_ops->raw_hwccf_is_enabled(params);
	}

	HWCCF_ERR("ops NULL\n");

	return 0;
}
EXPORT_SYMBOL(raw_hwccf_is_enabled);

uint32_t hwccf_read_wrapper(enum HWCCF_TYPE hwccf_type, uint32_t ofs)
{
	struct regmap *map;
	int ret;
	uint32_t rval;

	map = ((hwccf_type == AP_HWCCF) ? regmaps[AP_HWCCF]:regmaps[MM_HWCCF]);

	ret = regmap_read(map, ofs, &rval);

	if (ret)
		return -HWV_READ_FAIL;
	else
		return rval;
}

static struct hwccf_ops v0_hwccf_ops = {
	.hwccf_voter_ctrl = v0_hwccf_voter_ctrl,
	.raw_hwccf_voter_ctrl = v0_raw_hwccf_voter_ctrl,
	.hwccf_irq_voter_ctrl = v0_hwccf_irq_voter_ctrl,
	.hwccf_irq_multi_voter_ctrl = v0_hwccf_irq_multi_voter_ctrl,
};

static struct hwccf_ops v1_hwccf_ops = {
	.hwccf_voter_ctrl = v1_hwccf_voter_ctrl,
	.hwccf_multi_voter_ctrl = v1_hwccf_multi_voter_ctrl,
	.raw_hwccf_voter_ctrl = v1_raw_hwccf_voter_ctrl,
	.hwccf_is_enabled = v1_hwccf_is_enabled,
	.raw_hwccf_is_enabled = v1_raw_hwccf_is_enabled,
	.hwccf_freeze = v1_hwccf_freeze,
	.hwccf_unfreeze = v1_hwccf_unfreeze,
	.hwccf_irq_voter_ctrl = v1_hwccf_irq_voter_ctrl,
	.hwccf_irq_multi_voter_ctrl = v1_hwccf_irq_multi_voter_ctrl,
};

static const struct hwccf_match_data v0_data = {
	.required_regmaps = 2,
	.ops = &v0_hwccf_ops,
};

static const struct hwccf_match_data v1_hwv_data = {
	.required_regmaps = 2,
	.ops = &v1_hwccf_ops,
};

static const struct of_device_id hwccf_of_match[] = {
	{ .compatible = "mediatek,mt6991-hwv", .data = &v0_data },
	{ .compatible = "mediatek,mt6991-mm_hwv", .data = &v0_data },
	{ .compatible = "mediatek,mt6993-ap_hwv", .data = &v1_hwv_data },
	{ .compatible = "mediatek,mt6993-mm_hwv", .data = &v1_hwv_data },
	{ /* sentinel */ }
};

static struct platform_driver hwccf_driver = {
	.probe = hwccf_drv_probe,
	.driver = {
		.name = "hwccf",
		.of_match_table = hwccf_of_match,
	},
};
module_platform_driver(hwccf_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek HWCCF Driver");
MODULE_AUTHOR("Kuan-Hsin Lee <kuan-hsin.lee@mediatek.com>");
