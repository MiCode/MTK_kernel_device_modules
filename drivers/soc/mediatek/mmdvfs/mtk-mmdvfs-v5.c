// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <soc/mediatek/mmdvfs_public.h>
#include <soc/mediatek/smi.h>
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

#include "mtk-mmdvfs-v5-memory.h"

static struct mmdvfs_data *mmdvfs_data;

static int log_level;

static bool mmup_ena;
#define MMDVFS_HFRP_FEATURE_ID (mmup_ena ? MMDVFS_MMUP_FEATURE_ID : MMDVFS_VCP_FEATURE_ID)

static phys_addr_t mmdvfs_mmup_iova;
static phys_addr_t mmdvfs_mmup_pa;
static void *mmdvfs_mmup_va;

static phys_addr_t mmdvfs_vcp_iova;
static phys_addr_t mmdvfs_vcp_pa;
static void *mmdvfs_vcp_va;

static bool mmdvfs_mmup_sram;
static void __iomem *mmdvfs_mmup_sram_va;

static bool mmdvfs_mmup_cb_ready;
static u64 mmup_cb_tick[VCP_EVENT_RESUME + 1];
static u64 pm_cb_tick[2];
static DEFINE_MUTEX(mmdvfs_mmup_cb_mutex);
static DEFINE_MUTEX(mmdvfs_mmup_ipi_mutex);

static int vcp_power;
static DEFINE_MUTEX(mmdvfs_vcp_pwr_mutex);

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

int mmdvfs_dump_dvfsrc_rg(void)
{
	if(mmdvfs_data->ops->dvfsrc_rg_dump)
		return mmdvfs_data->ops->dvfsrc_rg_dump();

	MMDVFS_ERR("get dvfsrc_rg_dump ops failed");

	return -EPERM;
}
EXPORT_SYMBOL_GPL(mmdvfs_dump_dvfsrc_rg);

int mmdvfs_dump_dvfsrc_record(void)
{
	if(mmdvfs_data->ops->dvfsrc_record_dump)
		return mmdvfs_data->ops->dvfsrc_record_dump();

	MMDVFS_ERR("get dvfsrc_record_dump ops failed");

	return -EPERM;
}
EXPORT_SYMBOL_GPL(mmdvfs_dump_dvfsrc_record);

int mmdvfs_force_step(const u8 idx, const s8 opp)
{
	int i;
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
			mmdvfs_data->ops->dfs_vote_by_xpu(user_id, level);
		mutex_unlock(&mmdvfs_data->mux[mux_id].lock);
	}

	return 0;
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

static int mmdvfs_hfrp_ipi_send(const u8 func, const u8 idx, const u8 opp, u32 *data, const bool vcp)
{
	const u32 feature_id = vcp ? MMDVFS_VCP_FEATURE_ID : MMDVFS_MMUP_FEATURE_ID;
	struct mtk_ipi_device *vcp_ipi_dev;
	struct mmdvfs_ipi_data slot = {func, idx, opp,
		(vcp ? mmdvfs_vcp_iova : mmdvfs_mmup_iova) >> 32, (u32)(vcp ? mmdvfs_vcp_iova : mmdvfs_mmup_iova)};
	int gen, ret = 0, retry = 0;
	static u8 times;
	u32 val;

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
	val = readl(MEM_IPI_SYNC_FUNC(vcp));
	writel(0, MEM_IPI_SYNC_DATA(vcp));
	writel(val | (1 << func), MEM_IPI_SYNC_FUNC(vcp));
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

	ret = mtk_ipi_send(vcp_ipi_dev, vcp ? IPI_OUT_MMDVFS_VCP : IPI_OUT_MMDVFS_MMUP,
		IPI_SEND_WAIT, &slot, PIN_OUT_SIZE_MMDVFS, IPI_TIMEOUT_MS);
	if (ret != IPI_ACTION_DONE)
		goto ipi_lock_end;

	retry = 0;
	while (!(readl(MEM_IPI_SYNC_DATA(vcp)) & (1 << func))) {
		//temp code, need remove
		if (!retry)
			break;

		if (++retry > 1000000) {
			ret = IPI_COMPL_TIMEOUT;
			break;
		}
		if (!is_vcp_ready_ex(feature_id)) {
			ret = -ETIMEDOUT;
			break;
		}
		udelay(1);
	}

	if (!ret)
		writel(val & ~readl(MEM_IPI_SYNC_DATA(vcp)), MEM_IPI_SYNC_FUNC(vcp));
	else if (gen == vcp_cmd_ex(feature_id, VCP_GET_GEN, "mmdvfs_task")) {
		if (!times)
			vcp_cmd_ex(feature_id, VCP_SET_HALT, "mmdvfs_task");
		times += 1;
	}

ipi_lock_end:
	val = readl(MEM_IPI_SYNC_FUNC(vcp));
	mutex_unlock(&mmdvfs_mmup_ipi_mutex);

ipi_send_end:
	if (ret || (log_level & (1 << log_ipi))) {
		MMDVFS_ERR(
			"ret:%d retry:%d vcp:%d feature_id:%u ready:%d cb_ready:%d slot:%#llx vcp_power:%d unfinish func:%#x",
			ret, retry, vcp, feature_id, is_vcp_ready_ex(feature_id), mmdvfs_mmup_cb_ready,
			*(u64 *)&slot, vcp_power, val);
		MMDVFS_ERR("sync_data:%#x ts_pm_suspend:%llu ts_pm_resume:%llu",
			readl(MEM_IPI_SYNC_DATA(vcp)), pm_cb_tick[0], pm_cb_tick[1]);
		MMDVFS_ERR("ts_mmup_suspend:%llu ts_mmup_resume:%llu ts_mmup_ready:%llu ts_mmup_stop:%llu",
			mmup_cb_tick[VCP_EVENT_SUSPEND], mmup_cb_tick[VCP_EVENT_RESUME],
			mmup_cb_tick[VCP_EVENT_READY], mmup_cb_tick[VCP_EVENT_STOP]);
	}

	return ret;
}

static inline void mmdvfs_mmup_sram_init(void)
{
	static bool sram_init;

	if (unlikely(!sram_init) && mmdvfs_mmup_sram) {
		mmdvfs_mmup_sram_va = vcp_get_sram_virt_ex() + readl(MEM_SRAM_OFFSET);
		sram_init = true;
		MMDVFS_DBG("sram_init:%d virt:%#lx offset:%#x va:%#lx",
			sram_init, (unsigned long)(void *)vcp_get_sram_virt_ex(),
			readl(MEM_SRAM_OFFSET), (unsigned long)(void *)mmdvfs_mmup_sram_va);
	}
}

static int mmdvfs_mmup_notifier_callback(struct notifier_block *nb, unsigned long action, void *data)
{
	switch (action) {
	case VCP_EVENT_READY:
		MMDVFS_DBG("VCP_EVENT_READY in");
		mmup_cb_tick[VCP_EVENT_READY] = sched_clock();
		mmdvfs_hfrp_ipi_send(FUNC_MMDVFS_INIT, 0, 0, NULL, false);
		mmdvfs_mmup_sram_init();
		mutex_lock(&mmdvfs_mmup_cb_mutex);
		mmdvfs_mmup_cb_ready = true;
		mutex_unlock(&mmdvfs_mmup_cb_mutex);
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
		mmdvfs_hfrp_ipi_send(FUNC_MMDVFS_SUSPEND, 0, 0, NULL, false);
		mutex_lock(&mmdvfs_mmup_cb_mutex);
		mmdvfs_mmup_cb_ready = false;
		mutex_unlock(&mmdvfs_mmup_cb_mutex);
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block mmdvfs_mmup_notifier = {
	.notifier_call	= mmdvfs_mmup_notifier_callback,
};

static int mmdvfs_vcp_init(void)
{
	static struct mtk_ipi_device *vcp_ipi_dev;
	struct iommu_domain *domain;
	int retry = 0;

	mmup_ena = is_mmup_enable_ex();
	MMDVFS_DBG("mmup_ena:%d", mmup_ena);

	/*while (!mmdebug_is_init_done()) {
		if (++retry > 100) {
			MMDVFS_ERR("mmdebug is not ready yet");
			return -ETIMEDOUT;
		}
		ssleep(1);
	}*/

	retry = 0;
	while (!is_vcp_ready_ex(MMDVFS_HFRP_FEATURE_ID)) {
		if (++retry > VCP_SYNC_TIMEOUT_MS) {
			MMDVFS_ERR("VCP not ready");
			return -ETIMEDOUT;
		}
		mdelay(1);
	}

	retry = 0;
	while (!(vcp_ipi_dev = vcp_get_ipidev(MMDVFS_HFRP_FEATURE_ID))) {
		if (++retry > 100) {
			MMDVFS_ERR("cannot get vcp ipidev");
			return -ETIMEDOUT;
		}
		ssleep(1);
	}

	mmdvfs_mmup_iova = vcp_get_reserve_mem_phys_ex(MMDVFS_MMUP_MEM_ID);
	if (smmu_v3_enabled())
		domain = iommu_get_domain_for_dev(
			mtk_smmu_get_shared_device(&vcp_ipi_dev->mrpdev->pdev->dev));
	else
		domain = iommu_get_domain_for_dev(&vcp_ipi_dev->mrpdev->pdev->dev);
	if (domain)
		mmdvfs_mmup_pa = iommu_iova_to_phys(domain, mmdvfs_mmup_iova);
	mmdvfs_mmup_va = (void *)vcp_get_reserve_mem_virt_ex(MMDVFS_MMUP_MEM_ID);

	mmdvfs_vcp_iova = vcp_get_reserve_mem_phys_ex(MMDVFS_VCP_MEM_ID);
	if (smmu_v3_enabled())
		domain = iommu_get_domain_for_dev(
			mtk_smmu_get_shared_device(&vcp_ipi_dev->mrpdev->pdev->dev));
	else
		domain = iommu_get_domain_for_dev(&vcp_ipi_dev->mrpdev->pdev->dev);
	if (domain)
		mmdvfs_vcp_pa = iommu_iova_to_phys(domain, mmdvfs_vcp_iova);
	mmdvfs_vcp_va = (void *)vcp_get_reserve_mem_virt_ex(MMDVFS_VCP_MEM_ID);

	MMDVFS_DBG("mmup: iova:%pa pa:%pa va:%#lx vcp: iova:%pa pa:%pa va:%#lx",
		&mmdvfs_mmup_iova, &mmdvfs_mmup_pa, (unsigned long)mmdvfs_mmup_va,
		&mmdvfs_vcp_iova, &mmdvfs_vcp_pa, (unsigned long)mmdvfs_vcp_va);

	vcp_A_register_notify_ex(MMDVFS_HFRP_FEATURE_ID, &mmdvfs_mmup_notifier);

	return 0;
}

static int mmdvfs_set_rate(struct clk_hw *hw, unsigned long rate, unsigned long parent_rate)
{
	struct mmdvfs_user *user = container_of(hw, typeof(*user), clk_hw);
	struct mmdvfs_mux *mux = &mmdvfs_data->mux[user->mux];
	u8 level = 0;
	int i;

	for (i = 0; i < mmdvfs_data->rc[mux->rc].level_num; i++)
		if (rate <= mux->freq[i])
			break;

	level = (i == mmdvfs_data->rc[mux->rc].level_num) ? (i - 1) : i;

	mutex_lock(&mux->lock);
	if(mmdvfs_data->ops->dfs_vote_by_xpu)
		mmdvfs_data->ops->dfs_vote_by_xpu(user->id, level);
	mutex_unlock(&mux->lock);

	MMDVFS_DBG("user_id:%d user_name:%s user_mux:%d user_xpu:%d user_level:%d rate:%lu level:%d",
		user->id, user->name, user->mux, user->xpu, user->level, rate, level);

	return 0;
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

static int mmdvfs_parse_mmdvfs_mux(struct device_node *node, struct mmdvfs_data *mmdvfs_data)
{
	u8 mmdvfs_clk_num;
	int i, ret;

	mmdvfs_clk_num = of_property_count_strings(node, "mediatek,mmdvfs-mux-names");
	if (mmdvfs_clk_num != mmdvfs_data->mux_num) {
		MMDVFS_ERR("mmdvfs_clk_num:%d mux_num:%d not aligned",
			mmdvfs_clk_num, mmdvfs_data->mux_num);
		return -EINVAL;
	}

	for (i = 0; i < mmdvfs_clk_num; i++) {
		struct device_node *table, *level = NULL;
		struct mmdvfs_mux *mux;
		phandle handle;
		u32 mux_id;
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

		ret = of_property_read_u32_index(node, "mediatek,mmdvfs-opp-table", i, &handle);
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

int mmdvfs_mux_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	int ret;

	mmdvfs_data = mmdvfs_get_mmdvfs_data(dev);
	if(!mmdvfs_data)
		return -EINVAL;

	mmdvfs_mmup_sram = of_property_read_bool(node, "mediatek,mmup-sram");

	ret = mmdvfs_parse_mmdvfs_mux(node, mmdvfs_data);
	ret = mmdvfs_parse_mmdvfs_clk(node, mmdvfs_data);
	ret = mmdvfs_get_rc_base(mmdvfs_data);

	ret = mmdvfs_vcp_init();
	register_pm_notifier(&mmdvfs_pm_notifier_block);

	return ret;
}
EXPORT_SYMBOL(mmdvfs_mux_probe);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek MMDVFS");
MODULE_AUTHOR("MediaTek Inc.");

