// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <soc/mediatek/mmdvfs_public.h>
#include <soc/mediatek/smi.h>
#include <mtk-vmm-notifier.h>
#include <linux/pm_domain.h>
#include <linux/rpmsg.h>
#include <linux/rpmsg/mtk_rpmsg.h>
#include <linux/iommu.h>
#include <linux/workqueue.h>

#include "clk-fmeter.h"
#include "clk-mtk.h"

#include "vcp_status.h"

#include "mtk-smmu-v3.h"

#include "mtk-mmdebug-vcp.h"
#include "mtk-mmdvfs-v5.h"

#include "mtk-mmdvfs-debug.h"
#include "mtk-mmdvfs-v5-memory.h"

#define OPP_NAG	(-1)

static struct mmdvfs_data *mmdvfs_data;

static int log_level;

static u8 vcore_level_count;
static u8 *vcore_level;

static bool mmup_ena;
#define MMDVFS_HFRP_FEATURE_ID (mmup_ena ? MMDVFS_MMUP_FEATURE_ID : MMDVFS_VCP_FEATURE_ID)

static u8 step_idx;
static int dpsw_thr;

static struct mmdvfs_debug_user *user; // {mmup_force_clock, vcp_set_rate}

static phys_addr_t mmdvfs_mmup_iova;
static phys_addr_t mmdvfs_mmup_pa;
static void *mmdvfs_mmup_va;
uint64_t mmup_ipi_ack_data;

static phys_addr_t mmdvfs_vcp_iova;
static phys_addr_t mmdvfs_vcp_pa;
static void *mmdvfs_vcp_va;
uint64_t vcp_ipi_ack_data;

static bool mmdvfs_mmup_sram;
static void __iomem *mmdvfs_mmup_sram_va;

static bool mmdvfs_mmup_cb_ready;
static u64 mmup_cb_tick[VCP_EVENT_RESUME + 1];
static u64 pm_cb_tick[2];
static DEFINE_MUTEX(mmdvfs_mmup_cb_mutex);
static DEFINE_MUTEX(mmdvfs_mmup_ipi_mutex);

static int vcp_power;
static DEFINE_MUTEX(mmdvfs_vcp_pwr_mutex);

static s8 vcore_force_val = OPP_NAG;
static s8 vcore_force_opp = OPP_NAG;

static u32 dconfig_force_clk, dconfig_force_clk_rc;

int mmdvfs_get_version(void)
{
	return MMDVFS_VER_V5;
}
EXPORT_SYMBOL_GPL(mmdvfs_get_version);

bool mmdvfs_get_mmup_enable(void)
{
	return mmup_ena;
}
EXPORT_SYMBOL_GPL(mmdvfs_get_mmup_enable);

void *mmdvfs_get_mmup_base(phys_addr_t *pa)
{
	if (pa)
		*pa = mmdvfs_mmup_pa;
	return mmdvfs_mmup_va;
}
EXPORT_SYMBOL_GPL(mmdvfs_get_mmup_base);

void *mmdvfs_get_vcp_base(phys_addr_t *pa)
{
	if (pa)
		*pa = mmdvfs_vcp_pa;
	return mmdvfs_vcp_va;
}
EXPORT_SYMBOL_GPL(mmdvfs_get_vcp_base);

bool mmdvfs_get_mmup_sram_enable(void)
{
	return mmdvfs_mmup_sram;
}
EXPORT_SYMBOL_GPL(mmdvfs_get_mmup_sram_enable);

void __iomem *mmdvfs_get_mmup_sram(void)
{
	return mmdvfs_mmup_sram_va;
}
EXPORT_SYMBOL_GPL(mmdvfs_get_mmup_sram);

inline bool mmdvfs_mmup_cb_ready_get(void)
{
	return mmdvfs_mmup_cb_ready;
}
EXPORT_SYMBOL_GPL(mmdvfs_mmup_cb_ready_get);

inline void mmdvfs_mmup_cb_mutex_lock(void)
{
	mutex_lock(&mmdvfs_mmup_cb_mutex);
}
EXPORT_SYMBOL_GPL(mmdvfs_mmup_cb_mutex_lock);

inline void mmdvfs_mmup_cb_mutex_unlock(void)
{
	mutex_unlock(&mmdvfs_mmup_cb_mutex);
}
EXPORT_SYMBOL_GPL(mmdvfs_mmup_cb_mutex_unlock);

inline u8 mmdvfs_user_get_rc(const u8 idx)
{
	return mmdvfs_data->mux[mmdvfs_data->user[idx].mux].rc;
}
EXPORT_SYMBOL_GPL(mmdvfs_user_get_rc);

inline u64 mmdvfs_user_get_freq_by_opp(const u8 idx, const s8 opp)
{
	u8 mux, lvl;

	if (unlikely(!mmdvfs_data || idx >= mmdvfs_data->user_num))
		return 0;

	mux = mmdvfs_data->user[idx].mux;
	lvl = OPP2LEVEL(mmdvfs_data->mux[mux].rc, opp);

	MMDVFS_DBG("idx:%hhd opp:%hhd mux:%hhd lvl:%hhd freq:%llu",
		idx, opp, mux, lvl, mmdvfs_data->mux[mux].freq[lvl]);

	return mmdvfs_data->mux[mux].freq[lvl];
}
EXPORT_SYMBOL_GPL(mmdvfs_user_get_freq_by_opp);

inline s8 mmdvfs_get_level_to_opp(const u8 rc, const s8 lvl)
{
	return OPP2LEVEL(rc, lvl);
}
EXPORT_SYMBOL_GPL(mmdvfs_get_level_to_opp);

int mmdvfs_dump_dvfsrc_rg(void)
{
	if(mmdvfs_data && mmdvfs_data->ops && mmdvfs_data->ops->dvfsrc_rg_dump)
		return mmdvfs_data->ops->dvfsrc_rg_dump();

	MMDVFS_ERR("get dvfsrc_rg_dump ops failed");

	return -EPERM;
}
EXPORT_SYMBOL_GPL(mmdvfs_dump_dvfsrc_rg);

int mmdvfs_dump_dvfsrc_record(void)
{
	if(mmdvfs_data && mmdvfs_data->ops && mmdvfs_data->ops->dvfsrc_record_dump)
		return mmdvfs_data->ops->dvfsrc_record_dump();

	MMDVFS_ERR("get dvfsrc_record_dump ops failed");

	return -EPERM;
}
EXPORT_SYMBOL_GPL(mmdvfs_dump_dvfsrc_record);

int mmdvfs_force_vcore_notify(const u32 val)
{
	int ret = 0;
	s8 opp;

	if (!vcore_level_count || (val >= vcore_level_count && val != 0xFF))
		return 0;

	//TODO: refactor from calling force_step to calling vcore ceil flow
	//release force_step at max vcore level
	//0:MMDVFS_PWR_VCORE
	opp = (val == 0xFF) ? OPP_NAG : OPP2LEVEL(0, vcore_level[0]);
	vcore_force_val = opp;

	if (!mmdvfs_mmup_cb_ready)
		return 0;

	ret = mmdvfs_force_step(0, opp);
	if (ret)
		MMDVFS_DBG("ret:%d force_vcore_level:%u final_vcore_opp:%hhd", ret, val, opp);
	else
		vcore_force_opp = opp;

	return ret;
}
EXPORT_SYMBOL_GPL(mmdvfs_force_vcore_notify);

int mmdvfs_force_step(const u8 idx, const s8 opp)
{
	int i, ret = 0;
	u8 level, user_id, mux_id;

	if (unlikely(!mmdvfs_data || idx >= mmdvfs_data->rc_num))
		return -EINVAL;

	level = OPP2LEVEL(idx, opp);
	level = (opp < 0) ? 0 : mmdvfs_data->rc[idx].level_num + level; //transfer to force level

	for (i = 0; i < mmdvfs_data->rc[idx].force_user_num; i++) {
		user_id = mmdvfs_data->rc[idx].force_user[i];
		mux_id = mmdvfs_data->user[user_id].mux;

		mutex_lock(&mmdvfs_data->mux[mux_id].lock);
		if(mmdvfs_data->ops->dfs_vote_by_xpu)
			ret = mmdvfs_data->ops->dfs_vote_by_xpu(user_id, level);
		mutex_unlock(&mmdvfs_data->mux[mux_id].lock);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(mmdvfs_force_step);

int mtk_mmdvfs_enable_vcp(const bool enable, const u8 idx)
{
	int ret = 0;

	if(!mmdvfs_data || idx >= mmdvfs_data->user_num)
		return -EINVAL;

	if (is_vcp_suspending_ex())
		return -EBUSY;

	mutex_lock(&mmdvfs_vcp_pwr_mutex);
	if (enable) {
		if (!vcp_power) {
			ret = vcp_register_feature_ex(MMDVFS_HFRP_FEATURE_ID);
			if (ret)
				goto enable_vcp_end;
		}
		vcp_power += 1;
		mmdvfs_data->user[idx].vcp_power += 1;
	} else {
		if (!mmdvfs_data->user[idx].vcp_power || !vcp_power) {
			ret = -EINVAL;
			goto enable_vcp_end;
		}
		if (vcp_power == 1) {
			mutex_lock(&mmdvfs_mmup_ipi_mutex);
			mutex_unlock(&mmdvfs_mmup_ipi_mutex);
			ret = vcp_deregister_feature_ex(MMDVFS_HFRP_FEATURE_ID);
			if (ret)
				goto enable_vcp_end;
		}
		mmdvfs_data->user[idx].vcp_power -= 1;
		vcp_power -= 1;
	}

enable_vcp_end:
	if (ret || (log_level & (1 << log_pwr)))
		MMDVFS_ERR("ret:%d enable:%d vcp_power:%d idx:%hhu usage:%d",
			ret, enable, vcp_power, idx, mmdvfs_data->user[idx].vcp_power);
	mutex_unlock(&mmdvfs_vcp_pwr_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_enable_vcp);

static inline void mmdvfs_check_vcp_power(void)
{
	int i;

	for(i = 0; i < mmdvfs_data->user_num; i++) {
		if (mmdvfs_data->user[i].vcp_power)
			MMDVFS_DBG("i:%d usage:%d not disable",
				i, mmdvfs_data->user[i].vcp_power);
	}
}

static int mmdvfs_pm_notifier(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		MMDVFS_DBG("PM_SUSPEND_PREPARE in");
		pm_cb_tick[0] = sched_clock();
		mmdvfs_check_vcp_power();
		break;
	case PM_POST_SUSPEND:
		MMDVFS_DBG("PM_POST_SUSPEND in");
		pm_cb_tick[1] = sched_clock();
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block mmdvfs_pm_notifier_block = {
	.notifier_call = mmdvfs_pm_notifier,
	.priority = 1,  //digit larger means higher priority
};

int mmdvfs_hfrp_ipi_send(const u8 func, const u8 idx, const u8 opp, u32 *data, const bool vcp)
{
	const u32 feature_id = vcp ? MMDVFS_VCP_FEATURE_ID : MMDVFS_MMUP_FEATURE_ID;
	struct mtk_ipi_device *vcp_ipi_dev;
	struct mmdvfs_ipi_data slot = {func, idx, opp,
		(vcp ? mmdvfs_vcp_iova : mmdvfs_mmup_iova) >> 32, (u32)(vcp ? mmdvfs_vcp_iova : mmdvfs_mmup_iova)};
	int gen, ret = 0, retry = 0;
	static u8 times;

	while (!is_vcp_ready_ex(feature_id) || !mmdvfs_mmup_cb_ready) {
		if (!mmdvfs_mmup_cb_ready && func == FUNC_MMDVFS_INIT)
			break;
		if (++retry > VCP_SYNC_TIMEOUT_MS) {
			ret = -ETIMEDOUT;
			goto ipi_send_end;
		}
		mdelay(1);
	}

	mutex_lock(&mmdvfs_mmup_ipi_mutex);
	gen = vcp_cmd_ex(feature_id, VCP_GET_GEN, "mmdvfs_task");

	mutex_lock(&mmdvfs_mmup_cb_mutex);
	if (!mmdvfs_mmup_cb_ready && func != FUNC_MMDVFS_INIT) {
		mutex_unlock(&mmdvfs_mmup_cb_mutex);
		ret = -ETIMEDOUT;
		goto ipi_lock_end;
	}
	mutex_unlock(&mmdvfs_mmup_cb_mutex);

	vcp_ipi_dev = vcp_get_ipidev(feature_id);
	if (!vcp_ipi_dev)
		goto ipi_lock_end;

	ret = mtk_ipi_send_compl(vcp_ipi_dev, vcp ? IPI_OUT_MMDVFS_VCP : IPI_OUT_MMDVFS_MMUP,
		IPI_SEND_WAIT, &slot, PIN_OUT_SIZE_MMDVFS, IPI_TIMEOUT_MS);
	MMDVFS_DBG("mtk_ipi_send_compl ret:%d", ret);  // need to remove after done

	if ((ret != IPI_ACTION_DONE) && (gen == vcp_cmd_ex(feature_id, VCP_GET_GEN, "mmdvfs_task"))) {
		if (!times)
			vcp_cmd_ex(feature_id, VCP_SET_HALT, "mmdvfs_task");
		times += 1;
	}

ipi_lock_end:
	mutex_unlock(&mmdvfs_mmup_ipi_mutex);

ipi_send_end:
	if (ret || (log_level & (1 << log_ipi))) {
		MMDVFS_ERR(
			"ret:%d retry:%d vcp:%d feature_id:%u ready:%d cb_ready:%d slot:%#llx vcp_power:%d unfinish func:%d",
			ret, retry, vcp, feature_id, is_vcp_ready_ex(feature_id), mmdvfs_mmup_cb_ready,
			*(u64 *)&slot, vcp_power, func);
		MMDVFS_ERR("ts_pm_suspend:%llu ts_pm_resume:%llu",
			pm_cb_tick[0], pm_cb_tick[1]);
		MMDVFS_ERR("ts_mmup_suspend:%llu ts_mmup_resume:%llu ts_mmup_ready:%llu ts_mmup_stop:%llu",
			mmup_cb_tick[VCP_EVENT_SUSPEND], mmup_cb_tick[VCP_EVENT_RESUME],
			mmup_cb_tick[VCP_EVENT_READY], mmup_cb_tick[VCP_EVENT_STOP]);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(mmdvfs_hfrp_ipi_send);

static int mmdvfs_vcp_set_rate(const char *val, const struct kernel_param *kp)
{
	int idx = 0, opp = 0, ret, last;

	ret = sscanf(val, "%d %d", &idx, &opp);
	if (ret != 2 || idx >= mmdvfs_data->mux_num) {
		MMDVFS_DBG("failed:%d idx:%d opp:%d mux_num:%d", ret, idx, opp, mmdvfs_data->mux_num);
		return -EINVAL;
	}

	last = user[idx].vote_opp;

	mtk_mmdvfs_enable_vcp(true, idx + step_idx);

	if ((mmdvfs_data->mux[idx].rc == 1) && dpsw_thr && opp >= 0 && opp < dpsw_thr &&
		(last < 0 || last >= dpsw_thr))
		mtk_vmm_ctrl_dbg_use(true);

	ret = mmdvfs_hfrp_ipi_send(FUNC_MMDVFS_VCP_SET_RATE, idx, opp, NULL, true);
	if(!ret) {
		user[idx].vote_opp = opp;
		mmdvfs_record_cmd_user(DRAM_CMD_VCP_IDX + idx, MAX_LEVEL, opp);
	}

	if ((mmdvfs_data->mux[idx].rc == 1) && dpsw_thr && (opp < 0 || opp >= dpsw_thr) &&
		last >= 0 && last < dpsw_thr)
		mtk_vmm_ctrl_dbg_use(false);

	mtk_mmdvfs_enable_vcp(false, idx + step_idx);

	return ret;
}

static const struct kernel_param_ops mmdvfs_vcp_set_rate_ops = {
	.set = mmdvfs_vcp_set_rate,
};
module_param_cb(vcp_set_rate, &mmdvfs_vcp_set_rate_ops, NULL, 0644);
MODULE_PARM_DESC(vcp_set_rate, "set rate from dummy vcp user by ipi");

static int mmdvfs_force_clock_impl(const int idx, const int opp, const int all)
{
	int ret = 0;
	u8 lvl;

	mtk_mmdvfs_enable_vcp(true, idx + step_idx);

	lvl = opp < 0 ? MAX_LEVEL : OPP2LEVEL(mmdvfs_data->mux[idx].rc, opp);
	if (!all) // mux
		ret = mmdvfs_hfrp_ipi_send(FUNC_MMDVFS_FORCE_CLOCK, idx, lvl, NULL, false);
	else // rc
		ret = mmdvfs_hfrp_ipi_send(FUNC_MMDVFS_FORCE_CLOCK_RC, mmdvfs_data->mux[idx].rc, lvl, NULL, false);

	if (!ret) {
		user[idx].force_opp = opp;
		mmdvfs_record_cmd_user(DRAM_CMD_VCP_IDX + idx, opp, MAX_LEVEL);
	}

	mtk_mmdvfs_enable_vcp(false, idx + step_idx);
	return ret;
}

static int mmdvfs_force_clock(const char *val, const struct kernel_param *kp)
{
	int all = 0, idx = 0, opp = 0, ret = 0;

	ret = sscanf(val, "%d %d %d", &idx, &opp, &all);
	if (ret != 3 || idx >= mmdvfs_data->mux_num) {
		MMDVFS_DBG("failed:%d idx:%d opp:%d all:%d", ret, idx, opp, all);
		return -EINVAL;
	}

	return mmdvfs_force_clock_impl(idx, opp, all);
}

static const struct kernel_param_ops mmdvfs_force_clock_ops = {
	.set = mmdvfs_force_clock,
};
module_param_cb(force_clock, &mmdvfs_force_clock_ops, NULL, 0644);
MODULE_PARM_DESC(force_clock, "force clock by ipi");

static inline void mmdvfs_mmup_sram_init(void)
{
	static bool sram_init;

	if (unlikely(!sram_init) && mmdvfs_mmup_sram) {
		if(mmdvfs_data && mmdvfs_data->ops && mmdvfs_data->ops->get_mmup_sram_offset) {
			mmdvfs_mmup_sram_va = vcp_get_sram_virt_ex() + mmdvfs_data->ops->get_mmup_sram_offset();
			if (DRAM_VCP_BASE)
				writel(mmdvfs_data->ops->get_mmup_sram_offset(), DRAM_SRAM_OFFSET);
		}
		sram_init = true;
		MMDVFS_DBG("sram_init:%d virt:%#lx offset:%#x va:%#lx",
			sram_init, (unsigned long)(void *)vcp_get_sram_virt_ex(),
			mmdvfs_data && mmdvfs_data->ops && mmdvfs_data->ops->get_mmup_sram_offset ?
				mmdvfs_data->ops->get_mmup_sram_offset() : 0,
			(unsigned long)(void *)mmdvfs_mmup_sram_va);
	}
}

static int mmdvfs_mmup_notifier_callback(struct notifier_block *nb, unsigned long action, void *data)
{
	switch (action) {
	case VCP_EVENT_READY:
		MMDVFS_DBG("VCP_EVENT_READY in");
		mmup_cb_tick[VCP_EVENT_READY] = sched_clock();
		mmdvfs_mmup_sram_init();
		mutex_lock(&mmdvfs_mmup_cb_mutex);
		mmdvfs_mmup_cb_ready = true;
		mutex_unlock(&mmdvfs_mmup_cb_mutex);
		if (vcore_force_val != OPP_NAG) {
			mmdvfs_force_step(0, vcore_force_val);
			vcore_force_opp = vcore_force_val;
		}
		break;
	case VCP_EVENT_RESUME:
		MMDVFS_DBG("VCP_EVENT_RESUME in");
		mmup_cb_tick[VCP_EVENT_RESUME] = sched_clock();
		break;
	case VCP_EVENT_STOP:
		MMDVFS_DBG("VCP_EVENT_STOP in");
		mmup_cb_tick[VCP_EVENT_STOP] = sched_clock();
		mutex_lock(&mmdvfs_mmup_cb_mutex);
		mmdvfs_mmup_cb_ready = false;
		mutex_unlock(&mmdvfs_mmup_cb_mutex);
		break;
	case VCP_EVENT_SUSPEND:
		MMDVFS_DBG("VCP_EVENT_SUSPEND in");
		mmup_cb_tick[VCP_EVENT_SUSPEND] = sched_clock();
		mutex_lock(&mmdvfs_mmup_cb_mutex);
		mmdvfs_mmup_cb_ready = false;
		mutex_unlock(&mmdvfs_mmup_cb_mutex);
		if (vcore_force_opp != OPP_NAG)
			mmdvfs_force_step(0, OPP_NAG);
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block mmdvfs_mmup_notifier = {
	.notifier_call	= mmdvfs_mmup_notifier_callback,
};

static int mmdvfs_vcp_notifier_callback(struct notifier_block *nb, unsigned long action, void *data)
{
	switch (action) {
	case VCP_EVENT_READY:
		MMDVFS_DBG("VCP_EVENT_READY in");
		mmdvfs_hfrp_ipi_send(FUNC_MMDVFS_INIT, 0, 0, NULL, true);
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block mmdvfs_vcp_notifier = {
	.notifier_call	= mmdvfs_vcp_notifier_callback,
};

static int mmdvfs_vcp_init(void)
{
	static struct mtk_ipi_device *vcp_ipi_dev, *mmup_ipi_dev;
	struct iommu_domain *domain;
	int retry = 0, ret = 0;

	while (!mmdebug_is_init_done()) {
		if (++retry > 100) {
			MMDVFS_ERR("mmdebug is not ready yet");
			return -ETIMEDOUT;
		}
		ssleep(1);
	}

	//vcp
	retry = 0;
	while (!is_vcp_ready_ex(MMDVFS_VCP_FEATURE_ID)) {
		if (++retry > VCP_SYNC_TIMEOUT_MS) {
			MMDVFS_ERR("VCP not ready");
			return -ETIMEDOUT;
		}
		mdelay(1);
	}

	retry = 0;
	while (!(vcp_ipi_dev = vcp_get_ipidev(MMDVFS_VCP_FEATURE_ID))) {
		if (++retry > 100) {
			MMDVFS_ERR("cannot get vcp ipidev");
			return -ETIMEDOUT;
		}
		ssleep(1);
	}

	mmdvfs_vcp_iova = vcp_get_reserve_mem_phys_ex(MMDVFS_VCP_MEM_ID);
	if (smmu_v3_enabled())
		domain = iommu_get_domain_for_dev(
			mtk_smmu_get_shared_device(&vcp_ipi_dev->mrpdev->pdev->dev));
	else
		domain = iommu_get_domain_for_dev(&vcp_ipi_dev->mrpdev->pdev->dev);
	if (domain)
		mmdvfs_vcp_pa = iommu_iova_to_phys(domain, mmdvfs_vcp_iova);
	mmdvfs_vcp_va = (void *)vcp_get_reserve_mem_virt_ex(MMDVFS_VCP_MEM_ID);

	MMDVFS_DBG("vcp: iova:%pa pa:%pa va:%#lx",
		&mmdvfs_vcp_iova, &mmdvfs_vcp_pa, (unsigned long)mmdvfs_vcp_va);

	ret = mtk_ipi_register(vcp_ipi_dev, IPI_OUT_MMDVFS_VCP,
		NULL, NULL, &vcp_ipi_ack_data);
	if (ret)
		MMDVFS_ERR("mtk_ipi_register failed:%d ipi_id:%d", ret, IPI_OUT_MMDVFS_VCP);


	mmup_ena = is_mmup_enable_ex();
	MMDVFS_DBG("mmup_ena:%d", mmup_ena);
	if (mmup_ena) {
		retry = 0;
		while (!is_vcp_ready_ex(MMDVFS_MMUP_FEATURE_ID)) {
			if (++retry > VCP_SYNC_TIMEOUT_MS) {
				MMDVFS_ERR("MMUP not ready");
				return -ETIMEDOUT;
			}
			mdelay(1);
		}

		retry = 0;
		while (!(mmup_ipi_dev = vcp_get_ipidev(MMDVFS_MMUP_FEATURE_ID))) {
			if (++retry > 100) {
				MMDVFS_ERR("cannot get mmup ipidev");
				return -ETIMEDOUT;
			}
			ssleep(1);
		}

		mmdvfs_mmup_iova = vcp_get_reserve_mem_phys_ex(MMDVFS_MMUP_MEM_ID);
		if (smmu_v3_enabled())
			domain = iommu_get_domain_for_dev(
				mtk_smmu_get_shared_device(&mmup_ipi_dev->mrpdev->pdev->dev));
		else
			domain = iommu_get_domain_for_dev(&mmup_ipi_dev->mrpdev->pdev->dev);
		if (domain)
			mmdvfs_mmup_pa = iommu_iova_to_phys(domain, mmdvfs_mmup_iova);
		mmdvfs_mmup_va = (void *)vcp_get_reserve_mem_virt_ex(MMDVFS_MMUP_MEM_ID);

		MMDVFS_DBG("mmup: iova:%pa pa:%pa va:%#lx",
			&mmdvfs_mmup_iova, &mmdvfs_mmup_pa, (unsigned long)mmdvfs_mmup_va);

		ret = mtk_ipi_register(mmup_ipi_dev, IPI_OUT_MMDVFS_MMUP,
			NULL, NULL, &mmup_ipi_ack_data);
		if (ret)
			MMDVFS_ERR("mtk_ipi_register failed:%d ipi_id:%d", ret, IPI_OUT_MMDVFS_MMUP);
	}

	if (mmup_ena) {
		vcp_A_register_notify_ex(MMDVFS_MMUP_FEATURE_ID, &mmdvfs_mmup_notifier);
		vcp_A_register_notify_ex(MMDVFS_VCP_FEATURE_ID, &mmdvfs_vcp_notifier);
	} else
		vcp_A_register_notify_ex(MMDVFS_VCP_FEATURE_ID, &mmdvfs_mmup_notifier);

	return 0;
}

void mmdvfs_record_cmd_user(const u8 usr, const u8 idx, const u8 lvl)
{
	u64 ns = sched_clock(), sec = ns / 1000000000, usec = (ns / 1000) % 1000000;
	u32 cnt;

	if (!DRAM_VCP_BASE || usr >= DRAM_CMD_NUM)
		return;

	cnt = readl(DRAM_CMD_IDX(usr)) % MEM_REC_CNT;
	writel(sec, DRAM_CMD_SEC(usr, cnt));
	writel(MEM_ENC_VAL(idx, lvl, usec), DRAM_CMD_VAL(usr, cnt));
	writel((cnt + 1) % MEM_REC_CNT, DRAM_CMD_IDX(usr));
}
EXPORT_SYMBOL_GPL(mmdvfs_record_cmd_user);

#ifndef CONFIG_64BIT
static inline u64 readq(const void __iomem *addr)
{
	u32 low, high;

	low = readl(addr);
	high = readl(addr + 4);

	return ((u64)high << 32) | low;
}

static inline void writeq(u64 value, void __iomem *addr)
{
	writel((u32)value, addr);
	writel((u32)(value >> 32), addr + 4);
}
#endif

static void mmdvfs_record_mbrain_data(const u8 user, const u32 rc, const u8 lvl,
	const u64 sec, const u64 usec, const u64 us)
{
	static struct mmdvfs_record_opp rec[MMDVFS_USER_OPP_RECORD_NUM];
	u64 total = 0;
	u32 val = 0;
	int i;

	if (!rec[user].sec && !rec[user].usec) {
		i = (readl(DRAM_USR_IDX(user)) - 1 + MEM_REC_CNT) % MEM_REC_CNT;
		val = readl(DRAM_USR_VAL(user, i));
		rec[user].sec = readl(DRAM_USR_SEC(user, i));
		rec[user].usec = MEM_DEC_USEC(val);
		rec[user].opp = OPP2LEVEL(rc, MEM_DEC_LVL(val));
	}

	if (rec[user].sec || rec[user].usec) {
		total = readq(DRAM_USR_TOTAL(user, rec[user].opp));
		total += (us - (rec[user].sec * 1000000 + rec[user].usec)) / 1000;
		writeq(total, DRAM_USR_TOTAL(user, rec[user].opp));
	}

	rec[user].sec = sec;
	rec[user].usec = usec;
	rec[user].opp = OPP2LEVEL(rc, lvl);
}

static inline void mmdvfs_record_user(const u8 usr, const u8 idx, const u8 lvl)
{
	u64 ns = sched_clock(), sec = ns / 1000000000, usec = (ns / 1000) % 1000000, us = ns / 1000;
	u32 cnt;

	if (!DRAM_VCP_BASE || usr >= DRAM_USR_NUM)
		return;

	cnt = readl(DRAM_USR_IDX(usr)) % MEM_REC_CNT;
	writel(sec, DRAM_USR_SEC(usr, cnt));
	writel(MEM_ENC_VAL(idx, lvl, usec), DRAM_USR_VAL(usr, cnt));
	writel((cnt + 1) % MEM_REC_CNT, DRAM_USR_IDX(usr));

	//only record real user
	mmdvfs_record_mbrain_data(usr, idx, lvl, sec, usec, us);
}

static int mmdvfs_set_rate(struct clk_hw *hw, unsigned long rate, unsigned long parent_rate)
{
	struct mmdvfs_user *user = container_of(hw, typeof(*user), clk_hw);
	struct mmdvfs_mux *mux = &mmdvfs_data->mux[user->mux];
	u8 level = 0;
	int i, ret = 0;

	for (i = 0; i < mmdvfs_data->rc[mux->rc].level_num; i++)
		if (rate <= mux->freq[i])
			break;

	level = (i == mmdvfs_data->rc[mux->rc].level_num) ? (i - 1) : i;

	mmdvfs_record_user(user->id, mux->rc, level);

	mutex_lock(&mux->lock);
	if(mmdvfs_data && mmdvfs_data->ops && mmdvfs_data->ops->dfs_vote_by_xpu)
		ret = mmdvfs_data->ops->dfs_vote_by_xpu(user->id, level);
	mutex_unlock(&mux->lock);

	//MMDVFS_DBG("user_id:%d user_name:%s user_mux:%d user_xpu:%d user_level:%d rate:%lu level:%d",
	//	user->id, user->name, user->mux, user->xpu, user->level, rate, level);

	return ret;
}

static long mmdvfs_round_rate(struct clk_hw *hw, unsigned long rate,
	unsigned long *parent_rate)
{
	struct mmdvfs_user *user = container_of(hw, typeof(*user), clk_hw);

	/*MMDVFS_DBG("user_id:%d user_name:%s user_mux:%d user_xpu:%d user_level:%d rate:%lu",
		user->id, user->name, user->mux, user->xpu, user->level, rate);*/

	return rate;
}

static unsigned long mmdvfs_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct mmdvfs_user *user = container_of(hw, typeof(*user), clk_hw);

	/*MMDVFS_DBG("user_id:%d user_name:%s user_mux:%d user_xpu:%d user_level:%d parent_rate:%lu",
		user->id, user->name, user->mux, user->xpu, user->level, parent_rate);*/

	return user->level < MAX_LEVEL ? mmdvfs_data->mux[user->mux].freq[user->level] : 0;
}

static const struct clk_ops mmdvfs_req_ops = {
	.set_rate	= mmdvfs_set_rate,
	.round_rate	= mmdvfs_round_rate,
	.recalc_rate	= mmdvfs_recalc_rate,
};

module_param(log_level, uint, 0644);
MODULE_PARM_DESC(log_level, "mmdvfs log level");

static struct mmdvfs_data *mmdvfs_get_mmdvfs_data(struct device *dev)
{
	struct mmdvfs_data *mmdvfs_data;

	mmdvfs_data = (struct mmdvfs_data *) of_device_get_match_data(dev);
	if (!mmdvfs_data) {
		MMDVFS_ERR("get mmdvfs_data failed");
		return NULL;
	}

	return mmdvfs_data;
}

struct tag_chipid {
	u32 size;
	u32 hw_code;
	u32 hw_subcode;
	u32 hw_ver;
	u32 sw_ver;
};

static int mmdvfs_get_chipid(void)
{
	struct device_node *node = of_find_node_by_path("/chosen");
	struct tag_chipid *chip_id = NULL;
	int len;

	if (!node)
		node = of_find_node_by_path("/chosen@0");
	if (!node) {
		MMDVFS_ERR("%s not found in device tree", "/chosen");
		return -ENODEV;
	}

	chip_id = (struct tag_chipid *)of_get_property(node, "atag,chipid", &len);
	if (!chip_id) {
		MMDVFS_ERR("%s not found in chosen", "atag,chipid");
		return -ENODEV;
	}

	return chip_id->sw_ver;
}

static int mmdvfs_parse_mmdvfs_mux(struct device_node *node, struct mmdvfs_data *mmdvfs_data)
{
	u8 mmdvfs_clk_num;
	int sw_ver, i, ret;

	mmdvfs_clk_num = of_property_count_strings(node, "mediatek,mmdvfs-mux-names");
	if (mmdvfs_clk_num != mmdvfs_data->mux_num) {
		MMDVFS_ERR("mmdvfs_clk_num:%d mux_num:%d not aligned",
			mmdvfs_clk_num, mmdvfs_data->mux_num);
		return -EINVAL;
	}

	sw_ver = mmdvfs_get_chipid();
	for (i = 0; i < mmdvfs_clk_num; i++) {
		struct device_node *table, *level = NULL;
		struct mmdvfs_mux *mux;
		phandle handle = 0;
		u32 mux_id = mmdvfs_data->mux_num;
		u64 freq;
		u8 idx = 0;

		ret = of_property_read_u32_index(node, "mediatek,mmdvfs-muxs", i, &mux_id);
		if (ret || mux_id >= mmdvfs_data->mux_num) {
			MMDVFS_ERR("parse %s i:%d mux_id:%d mmdvfs_mux_num:%d failed:%d",
				"clocks", i, mux_id, mmdvfs_data->mux_num, ret);
			return ret;
		}
		mux = &mmdvfs_data->mux[mux_id];

		of_property_read_string_index(node, "mediatek,mmdvfs-mux-names", i, &mux->name);

		ret = of_property_read_u32_index(node, sw_ver == 0x0001 ?
			"mediatek,mmdvfs-opp-table-b0" : "mediatek,mmdvfs-opp-table", i, &handle);
		if (ret) {
			MMDVFS_ERR("failed:%d i:%d handle:%u", ret, i, handle);
			return ret;
		}

		table = of_find_node_by_phandle(handle);
		if (!table)
			return -EINVAL;
		do {
			level = of_get_next_available_child(table, level);
			if (level) {
				ret = of_property_read_u64(level, "opp-hz", &freq);
				if (ret) {
					MMDVFS_ERR("failed:%d i:%d freq:%llu", ret, i, freq);
					return ret;
				}
				mux->freq[idx] = freq;
				idx += 1;
			}
		} while (level);
		of_node_put(table);

		if (idx != mmdvfs_data->rc[mux->rc].level_num) {
			MMDVFS_ERR("idx:%d level_num:%d not aligned",
				idx, mmdvfs_data->rc[mux->rc].level_num);
			//return -EINVAL;
		}

		mutex_init(&mux->lock);
		MMDVFS_DBG("i:%d mux_id:%d mux_name:%s mux_rc:%d mux_pos:%d mux_freq[0]:%llu",
			i, mux_id, mux->name, mux->rc, mux->pos, mux->freq[0]);
	}

	return 0;
}

static int mmdvfs_parse_mmdvfs_clk(struct device_node *node, struct mmdvfs_data *mmdvfs_data)
{
	struct clk_onecell_data *clk_data;
	struct clk *clk;
	int i, ret;

	clk_data = mtk_alloc_clk_data(mmdvfs_data->user_num);
	if (!clk_data) {
		MMDVFS_ERR("allocate clk_data failed num:%hhu", mmdvfs_data->user_num);
		return -ENOMEM;
	}

	for (i = 0; i < mmdvfs_data->user_num; i++) {
		struct clk_init_data init = {};

		init.name = mmdvfs_data->user[i].name;
		init.ops = &mmdvfs_req_ops;
		mmdvfs_data->user[i].clk_hw.init = &init;

		clk = clk_register(NULL, &mmdvfs_data->user[i].clk_hw);
		if (IS_ERR_OR_NULL(clk))
			MMDVFS_ERR("i:%d clk:%s register failed:%d",
				i, mmdvfs_data->user[i].name, PTR_ERR_OR_ZERO(clk));
		else
			clk_data->clks[i] = clk;
	}

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (ret) {
		MMDVFS_ERR("add clk provider failed:%d", ret);
		mtk_free_clk_data(clk_data);
		return ret;
	}

	MMDVFS_DBG("add clk provider pass");

	return 0;
}

static int mmdvfs_parse_vcore_level(struct device_node *node)
{
	int ret = 0;

	ret = of_property_count_u8_elems(node, "mediatek,vcore-level");
	if (ret > 0) {
		vcore_level_count = ret;
		vcore_level = kcalloc(vcore_level_count, sizeof(*vcore_level), GFP_KERNEL);
		if (!vcore_level) {
			MMDVFS_DBG("vcore-level alloc failed:%d", ret);
		} else {
			ret = of_property_read_u8_array(node,
				"mediatek,vcore-level", vcore_level, vcore_level_count);
			if (ret)
				MMDVFS_DBG("read_array vcore-level failed:%d", ret);
		}
	}

	return 0;
}

static int mmdvfs_get_rc_base(struct mmdvfs_data *mmdvfs_data)
{
	int i;
	struct mmdvfs_rc *rc;

	for (i = 0; i < mmdvfs_data->rc_num; i++) {
		rc = &mmdvfs_data->rc[i];
		rc->rc_base = ioremap(rc->pa, 0x100000);
	}

	MMDVFS_DBG("pass");

	return 0;
}

static inline int mmdvfs_debug_parse_user(struct mmdvfs_debug_user **_user, u8 count)
{
	struct mmdvfs_debug_user *user;
	int i;

	user = kcalloc(count, sizeof(*user), GFP_KERNEL);
	if (!user)
		return -ENOMEM;

	*_user = user;
	for (i = 0; i < count; i++) {
		user[i].id = i;
		user[i].rc = mmdvfs_data->mux[i].rc;
		user[i].force_opp = OPP_NAG;
		user[i].vote_opp = OPP_NAG;
	}

	return 0;
}

static inline int mmdvfs_debug_parse_dconfig(struct device_node *node)
{
	dconfig_force_clk = 0xff;
	dconfig_force_clk_rc = 0xff;

	of_property_read_u32(node, "force-single-clk", &dconfig_force_clk);
	of_property_read_u32(node, "force-rc-clk", &dconfig_force_clk_rc);

	MMDVFS_DBG("dconfig_force_clk:%#x dconfig_force_clk_rc:%#x",
		dconfig_force_clk, dconfig_force_clk_rc);

	return 0;
}

static inline int mmdvfs_debug_set_dconfig(void)
{
	if (dconfig_force_clk != 0xff) {
		MMDVFS_DBG("set dconfig_force_clk:%#x", dconfig_force_clk);
		mmdvfs_force_clock_impl(dconfig_force_clk >> 4 & 0xf, dconfig_force_clk & 0xf, false);
	}

	if (dconfig_force_clk_rc != 0xff) {
		MMDVFS_DBG("set dconfig_force_clk_rc:%#x", dconfig_force_clk_rc);
		mmdvfs_force_clock_impl(dconfig_force_clk_rc >> 4 & 0xf, dconfig_force_clk_rc & 0xf, true);
	}

	return 0;
}

static int mmdvfs_init_kthread(void *data)
{
	int retry = 0;

	while (!mmdvfs_mmup_cb_ready_get()) {
		if (++retry > 100) {
			MMDVFS_DBG("mmdvfs_v5 init not ready");
			return -ETIMEDOUT;
		}
		ssleep(1);
	}

	mmdvfs_debug_set_dconfig();

	return 0;
}

int mmdvfs_mux_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	struct task_struct *task;
	int ret;

	mmdvfs_data = mmdvfs_get_mmdvfs_data(dev);
	if(!mmdvfs_data)
		return -EINVAL;

	mmdvfs_mmup_sram = of_property_read_bool(node, "mediatek,mmup-sram");
	of_property_read_s32(node, "mediatek,dpsw-thres", &dpsw_thr);

	ret = mmdvfs_parse_mmdvfs_mux(node, mmdvfs_data);
	ret = mmdvfs_parse_mmdvfs_clk(node, mmdvfs_data);
	ret = mmdvfs_parse_vcore_level(node);
	ret = mmdvfs_debug_parse_dconfig(node);

	ret = of_property_read_u8(node, "mediatek,step-idx", &step_idx);
	mmdvfs_debug_parse_user(&user, mmdvfs_data->mux_num);

	ret = mmdvfs_get_rc_base(mmdvfs_data);

	ret = mmdvfs_vcp_init();
	ret = register_pm_notifier(&mmdvfs_pm_notifier_block);
	if (ret)
		MMDVFS_ERR("failed:%d", ret);

	task = kthread_run(mmdvfs_init_kthread, NULL, "mmdvfs-init-kthread");
	if (IS_ERR(task))
		MMDVFS_DBG("kthread_run failed:%ld", PTR_ERR(task));

	return ret;
}
EXPORT_SYMBOL_GPL(mmdvfs_mux_probe);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek MMDVFS");
MODULE_AUTHOR("MediaTek Inc.");

