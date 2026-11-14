// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/regulator/consumer.h>
#include <linux/seq_file.h>
#include <linux/spinlock_types.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>

#include <clk-fmeter.h>
#include <mt-plat/mrdump.h>
#include <mtk-mmdebug-vcp.h>
#include <mtk-mmdvfs-v5.h>
#include <mtk-smi-dbg.h>
#include <mtk-vmm-notifier.h>
#include <soc/mediatek/mmdvfs_public.h>

#include "mtk-mmdvfs-debug.h"
#include "mtk-mmdvfs-v5-memory.h"
#include "mtk-mmdvfs-ftrace.h"

#define mmdvfs_seq_print(file, fmt, args...)		\
({							\
	if (file)					\
		seq_printf(file, fmt"\n", ##args);	\
	else						\
		MMDVFS_DBG(fmt, ##args);		\
})

//static DEFINE_SPINLOCK(lock);

#define OPP_NAG	(-1)

struct workqueue_struct *workq;
struct work_struct work;

struct regulator *reg_vcore;
struct regulator *reg_vmm;
struct regulator *reg_vdisp;

static u32 mux_base_pa;
void __iomem *mux_base;

static u16 mux_count;
static u16 *mux_offset;

static u8 fmeter_count;
static u8 *fmeter_id;
static u8 *fmeter_type;

static int dpsw_thr;
static bool met_freerun;
#if IS_ENABLED(CONFIG_MTK_MMDVFS_VCP)
static bool ftrace_v5_ena;
#endif
static bool mmdvfs_pm_suspend;

static u8 user_count;
static struct mmdvfs_debug_user *user;
static u8 step_count, *step_idx;

static u32 dconfig_vote_step, dconfig_force_step;

int mmdvfs_debug_v5_force_vcore(const u32 val)
{
#if IS_ENABLED(CONFIG_MTK_MMDVFS_VCP)
	return mmdvfs_force_vcore_notify(val);
#else
	return 0;
#endif
}
EXPORT_SYMBOL_GPL(mmdvfs_debug_v5_force_vcore);

int mmdvfs_debug_force_step(const u8 idx, const s8 opp)
{
	int ret, last;

	if (idx >= step_count || mmdvfs_pm_suspend) {
		MMDVFS_ERR("invalid idx:%hhu opp:%hhd pm_suspend:%d", idx, opp, mmdvfs_pm_suspend);
		return -EINVAL;
	}

	last = user[step_idx[idx]].force_opp;

	mtk_mmdvfs_enable_vcp(true, user[step_idx[idx]].id);

	if ((user[step_idx[idx]].rc == 1) && dpsw_thr && opp >= 0 && opp < dpsw_thr &&
		(last < 0 || last >= dpsw_thr))
		mtk_vmm_ctrl_dbg_use(true);

	ret = mmdvfs_force_step(idx, opp);
	if (!ret) {
		user[step_idx[idx]].force_opp = opp;
		mmdvfs_record_cmd_user(step_idx[idx], opp, MAX_LEVEL);
	}

	if ((user[step_idx[idx]].rc == 1) && dpsw_thr && (opp < 0 || opp >= dpsw_thr) &&
		last >= 0 && last < dpsw_thr)
		mtk_vmm_ctrl_dbg_use(false);

	mtk_mmdvfs_enable_vcp(false, user[step_idx[idx]].id);

	return ret;
}
EXPORT_SYMBOL_GPL(mmdvfs_debug_force_step);

static int mmdvfs_debug_vote_impl(const u8 idx, const s8 opp)
{
	int last, ret;

	if (mmdvfs_pm_suspend) {
		MMDVFS_ERR("idx:%hhu opp:%hhd pm_suspend:%d", idx, opp, mmdvfs_pm_suspend);
		return -EINVAL;
	}

	last = user[idx].vote_opp;

	mtk_mmdvfs_enable_vcp(true, user[idx].id);

	if ((user[idx].rc == 1) && dpsw_thr && opp >= 0 && opp < dpsw_thr &&
		(last < 0 || last >= dpsw_thr))
		mtk_vmm_ctrl_dbg_use(true);

	ret = clk_set_rate(user[idx].clk, mmdvfs_user_get_freq_by_opp(user[idx].id, opp));
	if (!ret) {
		user[idx].vote_opp = opp;
		mmdvfs_record_cmd_user(idx, MAX_LEVEL, opp);
	}

	if ((user[idx].rc == 1) && dpsw_thr && (opp < 0 || opp >= dpsw_thr) &&
		last >= 0 && last < dpsw_thr)
		mtk_vmm_ctrl_dbg_use(false);

	mtk_mmdvfs_enable_vcp(false, user[idx].id);

	return ret;
}

int mmdvfs_debug_vote_step(const u8 idx, const s8 opp)
{
	if (idx >= step_count) {
		MMDVFS_ERR("invalid idx:%hhu opp:%hhd", idx, opp);
		return -EINVAL;
	}

	return mmdvfs_debug_vote_impl(step_idx[idx], opp);
}
EXPORT_SYMBOL_GPL(mmdvfs_debug_vote_step);

static int mmdvfs_debug_v5_set_force_step(const char *val, const struct kernel_param *kp)
{
	int idx = 0, opp = 0, ret;

	ret = sscanf(val, "%d %d", &idx, &opp);
	if (ret != 2) {
		MMDVFS_DBG("failed:%d idx:%d opp:%d", ret, idx, opp);
		return -EINVAL;
	}

	return mmdvfs_debug_force_step(idx, opp);
}

static int mmdvfs_debug_v5_set_vote_step(const char *val, const struct kernel_param *kp)
{
	int idx = 0, opp = 0, ret;

	ret = sscanf(val, "%d %d", &idx, &opp);
	if (ret != 2) {
		MMDVFS_DBG("failed:%d idx:%d opp:%d", ret, idx, opp);
		return -EINVAL;
	}

	return mmdvfs_debug_vote_step(idx, opp);
}

static int mmdvfs_debug_v5_ap_set_rate(const char *val, const struct kernel_param *kp)
{
	int idx = 0, opp = 0, ret;

	ret = sscanf(val, "%d %d", &idx, &opp);
	if (ret != 2 || idx >= user_count) {
		MMDVFS_DBG("failed:%d idx:%d opp:%d", ret, idx, opp);
		return -EINVAL;
	}

	return mmdvfs_debug_vote_impl(idx, opp);
}

static int mmdvfs_debug_freerun(const char *val, const struct kernel_param *kp)
{
	uint8_t idx = 0;
	int i, ret = 0;

	ret = sscanf(val, "%hhu", &idx);
	if (ret != 1) {
		MMDVFS_DBG("failed:%d idx:%hhu", ret, idx);
		return -EINVAL;
	}

	for (i = 0; i < user_count; i++)
		if (user && user[i].rc == idx) {
			user[i].vote_opp = OPP_NAG;
			mmdvfs_record_cmd_user(i, MAX_LEVEL, OPP_NAG);
			ret = clk_set_rate(user[i].clk, mmdvfs_user_get_freq_by_opp(user[i].id, OPP_NAG));
		}

	return ret;
}

static struct kernel_param_ops mmdvfs_debug_freerun_ops = {
	.set = mmdvfs_debug_freerun,
};
module_param_cb(freerun, &mmdvfs_debug_freerun_ops, NULL, 0644);
MODULE_PARM_DESC(freerun, "freerun by rc id");

#ifndef CONFIG_64BIT
static inline u64 readq(const void __iomem *addr)
{
	u32 low, high;

	low = readl(addr);
	high = readl(addr + 4);

	return ((u64)high << 32) | low;
}
#endif

static void mmdvfs_debug_work(struct work_struct *work)
{
	if (!IS_ERR_OR_NULL(reg_vcore))
		MMDVFS_DBG("vcore enabled:%d voltage:%d",
			regulator_is_enabled(reg_vcore), regulator_get_voltage(reg_vcore));

	if (!IS_ERR_OR_NULL(reg_vmm))
		MMDVFS_DBG("vmm enabled:%d voltage:%d",
			regulator_is_enabled(reg_vmm), regulator_get_voltage(reg_vmm));

	if (!IS_ERR_OR_NULL(reg_vdisp))
		MMDVFS_DBG("vdisp enabled:%d voltage:%d",
			regulator_is_enabled(reg_vdisp), regulator_get_voltage(reg_vdisp));
}

static int mmdvfs_debug_dump_volt_freq(struct seq_file *file)
{
	u32 val;
	int i;

	if (!work_pending(&work))
		queue_work(workq, &work);
	else
		MMDVFS_DBG("mmdvfs_debug_work fail : cannot dump regulator");

	for (i = 0; i < mux_count; i++)
		mmdvfs_seq_print(file, "mux:%d val:%#x", i, readl(mux_base + mux_offset[i]));

	for (i = 0; i < fmeter_count; i++) {
		val = mt_get_fmeter_freq(fmeter_id[i], fmeter_type[i]);
		mmdvfs_seq_print(file, "fmeter:%d id:%hhu type:%hhu freq:%u", i, fmeter_id[i], fmeter_type[i], val);
	}

	// mmdvfs_dump_dvfsrc_rg();

	return 0;
}

#define MEM_PRINT(sec, val, file, name, idx_name, lvl_name) \
	do { \
		if (!sec && !MEM_DEC_USEC(val)) \
			continue; \
		mmdvfs_seq_print(file, "[%6u.%06u] " name ":%2d " idx_name ":%2u " lvl_name ":%2u", \
			sec, MEM_DEC_USEC(val), i, MEM_DEC_IDX(val), MEM_DEC_LVL(val)); \
	} while (0)

#define MEM_PRINT_LOOP(NUM, IDX, SEC, VAL, file, desc, ...) \
	do { \
		mmdvfs_seq_print(file, desc); \
		for (i = 0; i < NUM; i++) { \
			int j, k = readl(IDX(i)) % MEM_REC_CNT; \
			for (j = k; j < MEM_REC_CNT; j++) \
				MEM_PRINT(readl(SEC(i, j)), readl(VAL(i, j)), file, ##__VA_ARGS__); \
			for (j = 0; j < k; j++) \
				MEM_PRINT(readl(SEC(i, j)), readl(VAL(i, j)), file, ##__VA_ARGS__); \
		} \
	} while (0)

#define MAX_REC_CLK_SIZE	(80)
#define REC_CLK_IDX(x)		(clk_snapshot[x])
#define REC_CLK_SEC(x, y)	(clk_snapshot[SRAM_CLK_CNT + MEM_OBJ_CNT * (MEM_REC_CNT * x + y) + 0])
#define REC_CLK_VAL(x, y)	(clk_snapshot[SRAM_CLK_CNT + MEM_OBJ_CNT * (MEM_REC_CNT * x + y) + 1])
static u32 *clk_snapshot;
static int mmdvfs_debug_v5_record_snapshot(void)
{
	int ret = 0;
	bool mmup_cb_ready;

	mtk_mmdvfs_enable_vcp(true, user ? user[0].id : 0);
	mmdvfs_mmup_cb_mutex_lock();
	mmup_cb_ready = mmdvfs_mmup_cb_ready_get();

	if (!mmup_cb_ready || !unlikely(SRAM_BASE)) {
		MMDVFS_ERR("mmup_cb_ready:%d SRAM_BASE:%d", mmup_cb_ready, SRAM_BASE ? true : false);
		goto record_snapshot_end;
	}

	if (clk_snapshot) {
		MMDVFS_DBG("clk snapshot already set");
		goto record_snapshot_end;
	}

	clk_snapshot = kmalloc((MAX_REC_CLK_SIZE + SRAM_CLK_CNT) * sizeof(*clk_snapshot) , GFP_KERNEL);
	if (!clk_snapshot) {
		MMDVFS_ERR("failed to allocate memory");
		ret = -ENOMEM;
		goto record_snapshot_end;
	}

	// clk: vcore, vmm, vdisp, cam, hop
	memcpy_fromio(clk_snapshot, SRAM_CLK_IDX(0), SRAM_CLK_CNT * sizeof(*clk_snapshot));
	memcpy_fromio(clk_snapshot + SRAM_CLK_CNT, SRAM_CLK_SEC(0, 0), MAX_REC_CLK_SIZE * sizeof(*clk_snapshot));

record_snapshot_end:
	mmdvfs_mmup_cb_mutex_unlock();
	mtk_mmdvfs_enable_vcp(false, user ? user[0].id : 0);
	return ret;
}

static int mmdvfs_debug_v5_status_dump(struct seq_file *file)
{
	int i, ret;

	ret = mmdvfs_debug_dump_volt_freq(file);

	if (DRAM_VCP_BASE) {
		MEM_PRINT_LOOP(DRAM_USR_NUM, DRAM_USR_IDX, DRAM_USR_SEC, DRAM_USR_VAL,
			file, "lvl history of usr from ap and vcp", "usr", "pwr", "lvl");

		MEM_PRINT_LOOP(DRAM_CMD_NUM, DRAM_CMD_IDX, DRAM_CMD_SEC, DRAM_CMD_VAL,
			file, "opp history of cmd from ap and vcp", "cmd", "force", "vote");
	}

	if (user) {
		mmdvfs_seq_print(file, "opp status of cmd from ap");
		for (i = 0; i < user_count; i++)
			mmdvfs_seq_print(file, "usr:%hhu name:%8s mux:%d pwr:%hhu force:%hhu vote:%hhu",
				user[i].id, user[i].name, i, user[i].rc, user[i].force_opp, user[i].vote_opp);
	}

	mtk_mmdvfs_enable_vcp(true, user ? user[0].id : 0);
	mmdvfs_mmup_cb_mutex_lock();
	ret = mmdvfs_mmup_cb_ready_get();
	if (!ret || !unlikely(SRAM_BASE)) {
		mmdvfs_mmup_cb_mutex_unlock();
		mtk_mmdvfs_enable_vcp(false, user ? user[0].id : 0);
		mmdvfs_seq_print(file, "mmup_cb_ready:%d SRAM_BASE:%d", ret, SRAM_BASE ? true : false);
		return 0;
	}

	// xpc: mmpc, cpc, dpc
	MEM_PRINT_LOOP(SRAM_XPC_CNT, SRAM_XPC_IDX, SRAM_XPC_SEC, SRAM_XPC_VAL,
		file, "lvl history of xpc from mmup", "xpc", "", "lvl");
	// irq: vcore dvs, vmm dvs, vdisp dvs, vmm dfs, vdisp dfs
	MEM_PRINT_LOOP(SRAM_IRQ_CNT, SRAM_IRQ_IDX, SRAM_IRQ_SEC, SRAM_IRQ_VAL,
		file, "lvl history of irq from mmup", "irq", "", "lvl");
	// pwr: vcore, vmm, vdisp
	MEM_PRINT_LOOP(SRAM_PWR_CNT, SRAM_PWR_IDX, SRAM_PWR_SEC, SRAM_PWR_VAL,
		file, "lvl history of pwr from mmup", "pwr", "", "lvl");
	// ccpro: vcore vmm
	MEM_PRINT_LOOP(SRAM_CCPRO_CNT, SRAM_CCPRO_IDX, SRAM_CCPRO_SEC, SRAM_CCPRO_VAL,
		file, "lvl history of ccpro from mmup", "ccpro", "", "lvl");
	// clk: vcore, vmm, vdisp, cam, hop
	if (clk_snapshot) {
		mmdvfs_seq_print(file,  "lvl history of clk from snapshot");
		for (i = 0; i < SRAM_CLK_CNT; i++) {
			u32 j, k = REC_CLK_IDX(i) % MEM_REC_CNT;

			for (j = k; j < MEM_REC_CNT; j++)
				MEM_PRINT(REC_CLK_SEC(i, j), REC_CLK_VAL(i, j), file, "clk", "", "lvl");
			for (j = 0; j < k; j++)
				MEM_PRINT(REC_CLK_SEC(i, j), REC_CLK_VAL(i, j), file, "clk", "", "lvl");
		}
		kfree(clk_snapshot);
		clk_snapshot = NULL;
	}
	MEM_PRINT_LOOP(SRAM_CLK_CNT, SRAM_CLK_IDX, SRAM_CLK_SEC, SRAM_CLK_VAL,
		file, "lvl history of clk from mmup", "clk", "", "lvl");
	// ceil: vcore, vmm, vdisp
	MEM_PRINT_LOOP(SRAM_CEIL_CNT, SRAM_CEIL_IDX, SRAM_CEIL_SEC, SRAM_CEIL_VAL,
		file, "lvl history of ceil from mmup", "ceil", "", "lvl");

	// prof
	mmdvfs_seq_print(file, "ns profile status from mmup");
	for (i = 0; i < SRAM_PROF_CNT; i++) {
		u32 sec = readl(SRAM_PROF_SEC(i));
		u32 val = readl(SRAM_PROF_VAL(i));

		if (!sec && !MEM_DEC_USEC(val))
			continue;
		mmdvfs_seq_print(file, "[%6u.] prof:%2d idx:%2u lvl:%2u dur:%u",
			sec, i, MEM_DEC_IDX(val), MEM_DEC_LVL(val), MEM_DEC_USEC(val));
	}

	mmdvfs_mmup_cb_mutex_unlock();
	mtk_mmdvfs_enable_vcp(false, user ? user[0].id : 0);

	return 0;
}

static int mmdvfs_debug_mmdebug_cb(struct notifier_block *nb, unsigned long action, void *data)
{
	return mmdvfs_debug_status_dump(NULL);
}

static int mmdvfs_debug_smi_cb(struct notifier_block *nb, unsigned long action, void *data)
{
	return mmdvfs_debug_dump_volt_freq(NULL);
}

struct notifier_block mmdebug_nb = {mmdvfs_debug_mmdebug_cb};
struct notifier_block smi_dbg_nb = {mmdvfs_debug_smi_cb};

static int mmdvfs_debug_show(struct seq_file *file, void *data)
{
	return mmdvfs_debug_status_dump(file);
}

static int mmdvfs_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmdvfs_debug_show, inode->i_private);
}

static const struct proc_ops mmdvfs_debug_fops = {
	.proc_open = mmdvfs_debug_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static void mmdvfs_encode_mbrain_data(struct seq_file *file, struct mmdvfs_res_mbrain_debug_ops *ops,
	struct mmdvfs_opp_record *rec, unsigned int record_num, const char *type)
{
	unsigned int data_size = 0;
	unsigned char *ptr = NULL, *addr = NULL;
	struct mmdvfs_res_mbrain_header header;
	u32 i, j;

	if (ops && ops->get_length && ops->get_data) {
		data_size = ops->get_length();
		seq_printf(file, "data_size:%u\n", data_size);

		if (data_size > 0) {
			ptr = kmalloc(data_size, GFP_KERNEL);
			if (!ptr)
				return;

			ops->get_data(ptr, data_size);

			memcpy(&header, ptr, sizeof(header));
			addr = ptr + sizeof(header);
			memcpy(rec, addr, sizeof(*rec) * record_num);

			kfree(ptr);
			ptr = NULL;

			seq_printf(file, "header mbrain_module:%u, version:%u ",
				header.mbrain_module, header.version);
			seq_printf(file, "data_offset:%u index_data_length:%u\n",
				header.data_offset, header.index_data_length);

			for (i = 0; i < record_num; i++)
				for (j = 0; j < MAX_OPP; j++)
					seq_printf(file, "%s:%u opp:%u total_time:%llu\n",
						type, i, j, rec[i].opp_duration[j]);
		}
	}
}

static int mmdvfs_mbrain_test(struct seq_file *file, void *data)
{
	struct mmdvfs_res_mbrain_debug_ops *ops = NULL;
	struct mmdvfs_opp_record rec_pwr[MMDVFS_OPP_RECORD_NUM];
	struct mmdvfs_opp_record rec_usr[MMDVFS_USER_OPP_RECORD_NUM];

	// get power & cam data
	ops = get_mmdvfs_mbrain_dbg_ops();
	mmdvfs_encode_mbrain_data(file, ops, rec_pwr, MMDVFS_OPP_RECORD_NUM, "pwr");

	//get usr data
	ops = get_mmdvfs_mbrain_usr_dbg_ops();
	mmdvfs_encode_mbrain_data(file, ops, rec_usr, MMDVFS_USER_OPP_RECORD_NUM, "usr");

	return 0;
}

static int mmdvfs_mbrain_test_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmdvfs_mbrain_test, inode->i_private);
}

static const struct proc_ops mmdvfs_mbrain_test_fops = {
	.proc_open = mmdvfs_mbrain_test_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static struct mmdvfs_res_mbrain_header mmdvfs_mbrain_header;

static unsigned int mmdvfs_debug_mbrain_pwr_get_length(void)
{
	mmdvfs_mbrain_header.mbrain_module = MMDVFS_RES_DATA_MODULE_ID;
	mmdvfs_mbrain_header.version = MMDVFS_RES_DATA_VERSION;
	mmdvfs_mbrain_header.data_offset = sizeof(struct mmdvfs_res_mbrain_header);
	mmdvfs_mbrain_header.index_data_length = mmdvfs_mbrain_header.data_offset +
		sizeof(struct mmdvfs_opp_record) * MMDVFS_OPP_RECORD_NUM;

	MMDVFS_DBG("length:%u", mmdvfs_mbrain_header.index_data_length);

	return mmdvfs_mbrain_header.index_data_length;
}

static unsigned int mmdvfs_debug_mbrain_usr_get_length(void)
{
	mmdvfs_mbrain_header.mbrain_module = MMDVFS_RES_USR_DATA_MODULE_ID;
	mmdvfs_mbrain_header.version = MMDVFS_RES_DATA_VERSION;
	mmdvfs_mbrain_header.data_offset = sizeof(struct mmdvfs_res_mbrain_header);
	mmdvfs_mbrain_header.index_data_length = mmdvfs_mbrain_header.data_offset +
		sizeof(struct mmdvfs_opp_record) * MMDVFS_USER_OPP_RECORD_NUM;

	MMDVFS_DBG("length:%u", mmdvfs_mbrain_header.index_data_length);

	return mmdvfs_mbrain_header.index_data_length;
}

static void *mmdvfs_debug_memcpy(void *dest, void *src, uint64_t size)
{
	memcpy(dest, src, size);
	dest += size;
	return dest;
}

static int mmdvfs_debug_mbrain_pwr_get_data(void *address, uint32_t size)
{
	struct mmdvfs_opp_record record[MMDVFS_OPP_RECORD_NUM];
	uint64_t ns, sec, usec, us, total;
	int i, j, ret;
	u32 val;
	s8 opp;

	ns = sched_clock();
	us = ns / 1000;

	// sram
	mtk_mmdvfs_enable_vcp(true, user ? user[0].id : 0);
	mmdvfs_mmup_cb_mutex_lock();
	ret = mmdvfs_mmup_cb_ready_get();
	if (!ret || !unlikely(SRAM_BASE)) {
		mmdvfs_mmup_cb_mutex_unlock();
		mtk_mmdvfs_enable_vcp(false, user ? user[0].id : 0);
		MMDVFS_ERR("mmup_cb_ready:%d SRAM_BASE:%d", ret, SRAM_BASE ? true : false);
		return 0;
	}

	for (i = 0; i < MMDVFS_OPP_RECORD_NUM; i++) {
		total = 0;
		j = (readl(SRAM_CLK_IDX(i)) - 1 + MEM_REC_CNT) % MEM_REC_CNT;
		val = readl(SRAM_CLK_VAL(i, j));
		sec = readl(SRAM_CLK_SEC(i, j));
		usec = MEM_DEC_USEC(val);
		opp = mmdvfs_get_level_to_opp((i == MMDVFS_ALONE_CAM) ? MMDVFS_POWER_1 : i, MEM_DEC_LVL(val));

		for (j = 0; j < ARRAY_SIZE(record[i].opp_duration); j++) {
			record[i].opp_duration[j] = readq(SRAM_PWR_TOTAL(i, j));
			if (j == opp && (sec || usec) && opp < MAX_OPP)
				record[i].opp_duration[j] += (us - (sec * 1000000 + usec)) / 1000;
		}
	}

	mmdvfs_mmup_cb_mutex_unlock();
	mtk_mmdvfs_enable_vcp(false, user ? user[0].id : 0);

	mmdvfs_debug_mbrain_pwr_get_length();
	address = mmdvfs_debug_memcpy(address, &mmdvfs_mbrain_header,
		sizeof(struct mmdvfs_res_mbrain_header));
	address = mmdvfs_debug_memcpy(address, &record,
		sizeof(struct mmdvfs_opp_record) * MMDVFS_OPP_RECORD_NUM);

	return 0;
}

static int mmdvfs_debug_mbrain_usr_get_data(void *address, uint32_t size)
{
	struct mmdvfs_opp_record record[MMDVFS_USER_OPP_RECORD_NUM];
	uint64_t ns, sec, usec, us, total;
	int i, j, ret, usr;
	u32 val;
	s8 opp;

	ns = sched_clock();
	us = ns / 1000;

	// dram, sw users
	if (DRAM_VCP_BASE) {
		for (i = 0; i < MMDVFS_USER_13; i++) {
			total = 0;
			j = (readl(DRAM_USR_IDX(i)) - 1 + MEM_REC_CNT) % MEM_REC_CNT;
			val = readl(DRAM_USR_VAL(i, j));
			sec = readl(DRAM_USR_SEC(i, j));
			usec = MEM_DEC_USEC(val);
			opp = mmdvfs_get_level_to_opp(mmdvfs_user_get_rc(i), MEM_DEC_LVL(val));

			for (j = 0; j < ARRAY_SIZE(record[i].opp_duration); j++) {
				record[i].opp_duration[j] = readq(DRAM_USR_TOTAL(i, j));
				if (j == opp && (sec || usec) && opp < MAX_OPP)
					record[i].opp_duration[j] += (us - (sec * 1000000 + usec)) / 1000;
			}
		}
	}

	// sram, hw users
	mtk_mmdvfs_enable_vcp(true, user ? user[0].id : 0);
	mmdvfs_mmup_cb_mutex_lock();
	ret = mmdvfs_mmup_cb_ready_get();
	if (!ret || !unlikely(SRAM_BASE)) {
		mmdvfs_mmup_cb_mutex_unlock();
		mtk_mmdvfs_enable_vcp(false, user ? user[0].id : 0);
		MMDVFS_ERR("mmup_cb_ready:%d SRAM_BASE:%d", ret, SRAM_BASE ? true : false);
		return 0;
	}

	for (i = MMDVFS_USER_13; i < MMDVFS_USER_OPP_RECORD_NUM; i++) {
		total = 0;
		usr = i - MMDVFS_USER_13;
		j = (readl(SRAM_XPC_IDX(usr)) - 1 + MEM_REC_CNT) % MEM_REC_CNT;
		val = readl(SRAM_XPC_VAL(usr, j));
		sec = readl(SRAM_XPC_SEC(usr, j));
		usec = MEM_DEC_USEC(val);
		opp = mmdvfs_get_level_to_opp(mmdvfs_user_get_rc(i), MEM_DEC_LVL(val));

		for (j = 0; j < ARRAY_SIZE(record[i].opp_duration); j++) {
			record[i].opp_duration[j] = readq(SRAM_USR_TOTAL(usr, j));
			if (j == opp && (sec || usec) && opp < MAX_OPP)
				record[i].opp_duration[j] += (us - (sec * 1000000 + usec)) / 1000;
		}
	}

	mmdvfs_mmup_cb_mutex_unlock();
	mtk_mmdvfs_enable_vcp(false, user ? user[0].id : 0);

	mmdvfs_debug_mbrain_usr_get_length();
	address = mmdvfs_debug_memcpy(address, &mmdvfs_mbrain_header,
		sizeof(struct mmdvfs_res_mbrain_header));
	address = mmdvfs_debug_memcpy(address, &record,
		sizeof(struct mmdvfs_opp_record) * MMDVFS_USER_OPP_RECORD_NUM);

	return 0;
}

static struct mmdvfs_res_mbrain_debug_ops mmdvfs_mbrain_ops = {
	.get_length = mmdvfs_debug_mbrain_pwr_get_length,
	.get_data = mmdvfs_debug_mbrain_pwr_get_data,
};

static struct mmdvfs_res_mbrain_debug_ops mmdvfs_mbrain_usr_ops = {
	.get_length = mmdvfs_debug_mbrain_usr_get_length,
	.get_data = mmdvfs_debug_mbrain_usr_get_data,
};

static struct mmdvfs_res_mbrain_debug_ops *mmdvfs_debug_v5_mbrain_pwr_get(void)
{
	if (!SRAM_BASE)
		return NULL;

	return &mmdvfs_mbrain_ops;
}

static struct mmdvfs_res_mbrain_debug_ops *mmdvfs_debug_v5_mbrain_usr_get(void)
{
	if (!SRAM_BASE || !DRAM_VCP_BASE)
		return NULL;

	return &mmdvfs_mbrain_usr_ops;
}

#if IS_ENABLED(CONFIG_MTK_MMDVFS_VCP)
static int mmdvfs_v5_dbg_ftrace_thread(void *data)
{
	int i, j, ret;

	if (!SRAM_BASE) {
		MMDVFS_DBG("mmdvfs_v5 SRAM_BASE not ready");
		ftrace_v5_ena = false;
		return 0;
	}

	MMDVFS_DBG("mmdvfs_v5 SRAM_BASE ready");
	ftrace_v5_ena = true;
	while (!kthread_should_stop()) {
		//sram
		mtk_mmdvfs_enable_vcp(true, user ? user[0].id : 0);
		mmdvfs_mmup_cb_mutex_lock();
		ret = mmdvfs_mmup_cb_ready_get();
		if (!ret || !unlikely(SRAM_BASE)) {
			mmdvfs_mmup_cb_mutex_unlock();
			mtk_mmdvfs_enable_vcp(false, user ? user[0].id : 0);
			MMDVFS_ERR("mmup_cb_ready:%d SRAM_BASE:%d", ret, SRAM_BASE ? true : false);
			continue;
		}

		// power opp
		for (i = 0; i < SRAM_PWR_CNT; i++) {
			j = (readl(SRAM_PWR_IDX(i)) - 1 + MEM_REC_CNT) % MEM_REC_CNT;
			ftrace_pwr_opp_v3(i, MEM_DEC_LVL(readl(SRAM_PWR_VAL(i, j))));
		}

		//sram, hw users
		for (i = MMDVFS_USER_13; i < MMDVFS_USER_OPP_RECORD_NUM; i++) {
			j = (readl(SRAM_XPC_IDX(i - MMDVFS_USER_13)) - 1 + MEM_REC_CNT) % MEM_REC_CNT;
			ftrace_user_opp_v3(i, MEM_DEC_LVL(readl(SRAM_XPC_VAL(i - MMDVFS_USER_13, j))));
		}

		mmdvfs_mmup_cb_mutex_unlock();
		mtk_mmdvfs_enable_vcp(false, user ? user[0].id : 0);

		//dram, sw users
		for (i = 0; i < MMDVFS_USER_13; i++) {
			j = (readl(DRAM_USR_IDX(i)) - 1 + MEM_REC_CNT) % MEM_REC_CNT;
			ftrace_user_opp_v3(i, MEM_DEC_LVL(readl(DRAM_USR_VAL(i, j))));
		}

		msleep(1);
	}

	ftrace_v5_ena = false;
	MMDVFS_DBG("kthread mmdvfs-dbg-ftrace-v5 end");
	return 0;
}
#endif

static int mmdvfs_debug_v5_set_ftrace(const char *val,
	const struct kernel_param *kp)
{
#if IS_ENABLED(CONFIG_MTK_MMDVFS_VCP)
	static struct task_struct *kthr_v5;
#endif
	u32 ver = 0, ena = 0;
	int ret;

	ret = sscanf(val, "%u %u", &ver, &ena);
	if (ret != 2) {
		MMDVFS_DBG("failed:%d ver:%hhu ena:%hhu", ret, ver, ena);
		return -EINVAL;
	}

	if (!met_freerun) {
		MMDVFS_DBG("mmdvfs met not freerun");
		return 0;
	}

#if IS_ENABLED(CONFIG_MTK_MMDVFS_VCP)
	if (ena) {
		if (ftrace_v5_ena)
			MMDVFS_DBG("kthread mmdvfs-dbg-ftrace-v5 already created");
		else {
			kthr_v5 = kthread_run(
				mmdvfs_v5_dbg_ftrace_thread, NULL, "mmdvfs-dbg-ftrace-v5");
			if (IS_ERR(kthr_v5))
				MMDVFS_DBG("create kthread mmdvfs-dbg-ftrace-v5 failed");
		}
	} else {
		if (ftrace_v5_ena) {
			ret = kthread_stop(kthr_v5);
			if (!ret) {
				MMDVFS_DBG("stop kthread mmdvfs-dbg-ftrace-v5");
				ftrace_v5_ena = false;
			}
		}
	}
#endif

	return 0;
}

static struct mmdvfs_debug_ops mmdvfs_debug_v5_ops = {
	.force_step_fp = mmdvfs_debug_v5_set_force_step,
	.vote_step_fp = mmdvfs_debug_v5_set_vote_step,
	.status_dump_fp = mmdvfs_debug_v5_status_dump,
	.record_snapshot_fp = mmdvfs_debug_v5_record_snapshot,
	.force_vcore_fp = mmdvfs_debug_v5_force_vcore,
	.mmdvfs_mbrain_fp = mmdvfs_debug_v5_mbrain_pwr_get,
	.mmdvfs_mbrain_usr_fp = mmdvfs_debug_v5_mbrain_usr_get,
	.ap_set_rate_fp = mmdvfs_debug_v5_ap_set_rate,
	.mmdvfs_ftrace_fp = mmdvfs_debug_v5_set_ftrace,
};

static int mmdvfs_debug_pm_notifier(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	int i;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		MMDVFS_DBG("PM_SUSPEND_PREPARE in");
		for (i = 0; i < user_count; i++)
			if (unlikely(user[i].force_opp != OPP_NAG)) {
				MMDVFS_DBG("user i:%d id:%hhu name:%8s force:%hhd not release before suspend",
					i, user[i].id, user[i].name, user[i].force_opp);
				mmdvfs_debug_force_step(user[i].rc, OPP_NAG);
			}
		for (i = 0; i < user_count; i++)
			if (unlikely(user[i].vote_opp != OPP_NAG)) {
				MMDVFS_DBG("user i:%d id:%hhu name:%8s vote:%hhd not release before suspend",
					i, user[i].id, user[i].name, user[i].vote_opp);
				mmdvfs_debug_vote_impl(i, OPP_NAG);
			}
		mmdvfs_pm_suspend = true;
		break;
	case PM_POST_SUSPEND:
		MMDVFS_DBG("PM_POST_SUSPEND in");
		mmdvfs_pm_suspend = false;
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block mmdvfs_debug_pm_notifier_block = {
	.notifier_call = mmdvfs_debug_pm_notifier,
	.priority = 0,
};

static inline void mmdvfs_debug_parse_proc(void)
{
	struct proc_dir_entry *dir, *proc;

	dir = proc_mkdir("mmdvfs", NULL);
	if (IS_ERR_OR_NULL(dir))
		MMDVFS_DBG("proc_mkdir failed:%ld", PTR_ERR(dir));

	proc = proc_create("mmdvfs_opp", 0440, dir, &mmdvfs_debug_fops);
	if (IS_ERR_OR_NULL(proc))
		MMDVFS_DBG("proc_create failed:%ld", PTR_ERR(proc));

	proc = proc_create("mmdvfs_mbrain_test", 0440, dir, &mmdvfs_mbrain_test_fops);
	if (IS_ERR_OR_NULL(proc))
		MMDVFS_DBG("proc_create failed:%ld", PTR_ERR(proc));
}

static inline void mmdvfs_debug_parse_regulator(struct device *dev)
{
	struct regulator *reg;

	reg = devm_regulator_get(dev, "vcore");
	if (IS_ERR_OR_NULL(reg))
		MMDVFS_DBG("devm_regulator_get vcore failed:%ld", PTR_ERR(reg));
	else
		reg_vcore = reg;

	reg = devm_regulator_get(dev, "vmm");
	if (IS_ERR_OR_NULL(reg))
		MMDVFS_DBG("devm_regulator_get vmm failed:%ld", PTR_ERR(reg));
	else
		reg_vmm = reg;

	reg = devm_regulator_get(dev, "vdisp");
	if (IS_ERR_OR_NULL(reg))
		MMDVFS_DBG("devm_regulator_get vdisp failed:%ld", PTR_ERR(reg));
	else
		reg_vdisp = reg;
}

static inline int mmdvfs_debug_parse_mux(struct device *dev)
{
	int ret;

	ret = of_property_read_u32(dev->of_node, "mux-base", &mux_base_pa);
	if (ret) {
		MMDVFS_DBG("read_u32 mux-base failed:%d", ret);
		return ret;
	}
	mux_base = ioremap(mux_base_pa, 0x1000);

	ret = of_property_count_u16_elems(dev->of_node, "mux-offset");
	if (ret <= 0) {
		MMDVFS_DBG("count_u8_elems mux-offset failed:%d", ret);
		return ret;
	}
	mux_count = ret;

	mux_offset = kcalloc(mux_count, sizeof(*mux_offset), GFP_KERNEL);
	if (!mux_offset)
		return -ENOMEM;
	ret = of_property_read_u16_array(dev->of_node, "mux-offset", mux_offset, mux_count);

	return ret;
}

static inline int mmdvfs_debug_parse_fmeter(struct device *dev)
{
	int ret;

	ret = of_property_count_u8_elems(dev->of_node, "fmeter-id");
	if (ret <= 0) {
		MMDVFS_DBG("count_u8_elems fmeter-id failed:%d", ret);
		return ret;
	}
	fmeter_count = ret;

	fmeter_id = kcalloc(fmeter_count, sizeof(*fmeter_id), GFP_KERNEL);
	if (!fmeter_id)
		return -ENOMEM;
	ret = of_property_read_u8_array(dev->of_node, "fmeter-id", fmeter_id, fmeter_count);

	fmeter_type = kcalloc(fmeter_count, sizeof(*fmeter_type), GFP_KERNEL);
	if (!fmeter_type)
		return -ENOMEM;
	ret = of_property_read_u8_array(dev->of_node, "fmeter-type", fmeter_type, fmeter_count);

	return ret;
}

static inline int mmdvfs_debug_parse_user(struct device *dev, struct mmdvfs_debug_user **_user, u8 *count)
{
	struct mmdvfs_debug_user *user;
	struct property *prop;
	const char *name;
	struct clk *clk;
	int i = 0, ret;

	ret = of_property_count_strings(dev->of_node, "clock-names");
	if (ret <= 0) {
		MMDVFS_DBG("count_strings clock-names failed:%d", ret);
		return ret;
	}
	*count = ret;

	user = kcalloc(*count, sizeof(*user), GFP_KERNEL);
	if (!user)
		return -ENOMEM;

	*_user = user;
	of_property_for_each_string(dev->of_node, "clock-names", prop, name) {
		struct of_phandle_args spec;

		ret = of_parse_phandle_with_args(dev->of_node, "clocks", "#clock-cells", i, &spec);
		if (!ret)
			user[i].id = spec.args[0];

		clk = devm_clk_get(dev, name);
		if (!IS_ERR_OR_NULL(clk))
			user[i].clk = clk;

		user[i].rc = mmdvfs_user_get_rc(user[i].id);
		user[i].name = name;
		user[i].force_opp = OPP_NAG;
		user[i].vote_opp = OPP_NAG;

		MMDVFS_DBG("user:%p count:%hhu i:%2d id:%2hhu rc:%hhu name:%8s clk:%p force:%hhd vote:%hhd",
			*_user, *count, i, user[i].id, user[i].rc, user[i].name, user[i].clk, user[i].force_opp, user[i].vote_opp);

		i += 1;
	}

	return ret;
}

static inline int mmdvfs_debug_parse_dconfig(struct device *dev)
{
	dconfig_vote_step = 0xff;
	dconfig_force_step = 0xff;

	of_property_read_u32(dev->of_node, "vote-step", &dconfig_vote_step);
	of_property_read_u32(dev->of_node, "force-step", &dconfig_force_step);

	MMDVFS_DBG("dconfig_vote_step:%#x dconfig_force_step:%#x",
		dconfig_vote_step, dconfig_force_step);

	return 0;
}

static inline int mmdvfs_debug_set_dconfig(void)
{
	int i, retry = 0;

	while (!step_count) {
		if (++retry > 100) {
			MMDVFS_DBG("step_count not ready");
			return -ETIMEDOUT;
		}
		ssleep(1);
	}

	if (dconfig_vote_step != 0xff)
		for (i = 0; i < step_count; i++) {
			MMDVFS_DBG("set dconfig_vote_step:%#x", dconfig_vote_step);
			mmdvfs_debug_vote_step(dconfig_vote_step >> 4 & 0xf, dconfig_vote_step & 0xf);
			dconfig_vote_step = dconfig_vote_step >> 8;
		}

	if (dconfig_force_step != 0xff)
		for (i = 0; i < step_count; i++) {
			MMDVFS_DBG("set dconfig_force_step:%#x", dconfig_force_step);
			mmdvfs_debug_force_step(dconfig_force_step >> 4 & 0xf, dconfig_force_step & 0xf);
			dconfig_force_step = dconfig_force_step >> 8;
		}

	return 0;
}

static int mmdvfs_debug_kthread(void *data)
{
	phys_addr_t pa = 0ULL;
	unsigned long va;
	int ret, retry = 0;

	while (!mmdvfs_mmup_cb_ready_get()) {
		if (++retry > 100) {
			MMDVFS_DBG("mmdvfs_v5 init not ready");
			return -ETIMEDOUT;
		}
		ssleep(1);
	}

	va = (unsigned long)(unsigned long *)mmdvfs_get_mmup_base(&pa);
	if (va && pa) {
		ret = mrdump_mini_add_extra_file(va, pa, PAGE_SIZE, "MMDVFS_OPP");
		if (ret)
			MMDVFS_DBG("failed:%d va:%#lx pa:%pa", ret, va, &pa);
	} else
		MMDVFS_DBG("get_mmup_base failed va:%#lx pa:%pa", va, &pa);

	va = (unsigned long)(unsigned long *)mmdvfs_get_vcp_base(&pa);
	if (va && pa) {
		ret = mrdump_mini_add_extra_file(va, pa, PAGE_SIZE, "MMDVFS_OPP_VCP");
		if (ret)
			MMDVFS_DBG("failed:%d va:%#lx pa:%pa", ret, va, &pa);
	} else
		MMDVFS_DBG("get_vcp_base failed va:%#lx pa:%pa", va, &pa);

	mmdvfs_debug_set_dconfig();

	return 0;
}

static int mmdvfs_debug_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct task_struct *task;
	int ret;

	mmdvfs_debug_parse_proc();
	mmdvfs_debug_parse_regulator(dev);
	mmdvfs_debug_parse_mux(dev);
	mmdvfs_debug_parse_fmeter(dev);
	mmdvfs_debug_parse_dconfig(dev);

	met_freerun = of_property_read_bool(dev->of_node, "mediatek,met-freerun");
	of_property_read_s32(dev->of_node, "mediatek,dpsw-thres", &dpsw_thr);

	MMDVFS_DBG("mux_base:%#x mux_count:%hu fmeter_count:%hhu met_freerun:%d",
		mux_base_pa, mux_count, fmeter_count, met_freerun);

	ret = register_pm_notifier(&mmdvfs_debug_pm_notifier_block);
	if (ret) {
		MMDVFS_ERR("failed:%d", ret);
		return ret;
	}

	mmdvfs_debug_ops_set(&mmdvfs_debug_v5_ops);

	workq = create_singlethread_workqueue("mmdvfs-debug-workq");
	INIT_WORK(&work, mmdvfs_debug_work);

	mtk_mmdebug_status_dump_register_notifier(&mmdebug_nb);
	mtk_smi_dbg_register_notifier(&smi_dbg_nb);

	task = kthread_run(mmdvfs_debug_kthread, NULL, "mmdvfs-debug-kthread");
	if (IS_ERR(task))
		MMDVFS_DBG("kthread_run failed:%ld", PTR_ERR(task));

	return 0;
}

static void mmdvfs_debug_remove(struct platform_device *pdev)
{
}

static const struct of_device_id of_mmdvfs_debug[] = {
	{
		.compatible = "mediatek,mmdvfs-debug",
	},
	{}
};

static struct platform_driver mmdvfs_debug_drv = {
	.probe = mmdvfs_debug_probe,
	.remove = mmdvfs_debug_remove,
	.driver = {
		.name = "mtk-mmdvfs-debug",
		.of_match_table = of_mmdvfs_debug,
	},
};

static int mmdvfs_debug_user_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	u32 freerun;
	int i, ret;

	mmdvfs_debug_parse_user(dev, &user, &user_count);

	ret = of_property_count_u8_elems(dev->of_node, "mediatek,step-idx");
	if (ret <= 0) {
		MMDVFS_DBG("count_u8_elems step-idx failed:%d", ret);
		return ret;
	}

	step_count = ret;

	step_idx = kcalloc(step_count, sizeof(*step_idx), GFP_KERNEL);
	if (!step_idx)
		return -ENOMEM;

	ret = of_property_read_u8_array(dev->of_node, "mediatek,step-idx", step_idx, step_count);
	ret = of_property_read_u32(dev->of_node, "mediatek,clk-freerun", &freerun);
	MMDVFS_DBG("step_count:%hhu freerun:%#x", step_count, freerun);

	for (i = 0; i < user_count; i++) {
		uint8_t opp;

		if (freerun & (1U << i))
			continue;

		opp = user[i].rc ? 1 : 3;
		ret = clk_set_rate(user[i].clk, mmdvfs_user_get_freq_by_opp(user[i].id, opp));
		user[i].vote_opp = opp;
		mmdvfs_record_cmd_user(i, MAX_LEVEL, opp);
	}

	return ret;
}

static void mmdvfs_debug_user_remove(struct platform_device *pdev)
{
}

static const struct of_device_id of_mmdvfs_debug_user[] = {
	{
		.compatible = "mediatek,mmdvfs-debug-user",
	},
	{}
};

static struct platform_driver mmdvfs_debug_user_drv = {
	.probe = mmdvfs_debug_user_probe,
	.remove = mmdvfs_debug_user_remove,
	.driver = {
		.name = "mtk-mmdvfs-debug-user",
		.of_match_table = of_mmdvfs_debug_user,
	},
};

static struct platform_driver * const mmdvfs_debug_drvs[] = {
	&mmdvfs_debug_drv,
	&mmdvfs_debug_user_drv,
};

static int __init mmdvfs_debug_init(void)
{
	int ret;

	ret = platform_register_drivers(mmdvfs_debug_drvs, ARRAY_SIZE(mmdvfs_debug_drvs));
	if (ret)
		MMDVFS_DBG("failed:%d", ret);

	return ret;
}

static void __exit mmdvfs_debug_exit(void)
{
	platform_unregister_drivers(mmdvfs_debug_drvs, ARRAY_SIZE(mmdvfs_debug_drvs));
}

module_init(mmdvfs_debug_init);
module_exit(mmdvfs_debug_exit);
MODULE_DESCRIPTION("MMDVFS Debug V5 Driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL");

