// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/regulator/consumer.h>
#include <linux/seq_file.h>
#include <linux/spinlock_types.h>
#include <linux/workqueue.h>

#include <clk-fmeter.h>
#include <mt-plat/mrdump.h>
#include <mtk-mmdebug-vcp.h>
#include <mtk-mmdvfs-v5.h>
#include <mtk-smi-dbg.h>
#include <soc/mediatek/mmdvfs_public.h>

#include "mtk-mmdvfs-debug.h"
#include "mtk-mmdvfs-v5-memory.h"

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

static bool met_freerun;

struct mmdvfs_debug_user {
	u8 id;
	u8 rc;
	const char *name;
	struct clk *clk;
	s8 force_opp;
	s8 vote_opp;
};

static u8 user_count;
static struct mmdvfs_debug_user *user;
static u8 step_count, *step_idx;

int mmdvfs_debug_v5_force_vcore(const u32 val)
{
#if IS_ENABLED(CONFIG_MTK_MMDVFS_VCP)
	//return mmdvfs_force_vcore_notify(val);
	return 0;
#else
	return 0;
#endif
}
EXPORT_SYMBOL_GPL(mmdvfs_debug_v5_force_vcore);

int mmdvfs_debug_force_step(const u8 idx, const s8 opp)
{
	int ret;

	if (idx >= step_count) {
		MMDVFS_ERR("invalide idx:%hhu opp:%hhd", idx, opp);
		return -EINVAL;
	}

	mtk_mmdvfs_enable_vcp(true, user[step_idx[idx]].id);
	ret = mmdvfs_force_step(idx, opp);
	if (!ret)
		user[step_idx[idx]].force_opp = opp;
	mtk_mmdvfs_enable_vcp(false, user[step_idx[idx]].id);

	return ret;
}
EXPORT_SYMBOL_GPL(mmdvfs_debug_force_step);

int mmdvfs_debug_vote_step(const u8 idx, const s8 opp)
{
	int ret;

	if (idx >= step_count) {
		MMDVFS_ERR("invalide idx:%hhu opp:%hhd", idx, opp);
		return -EINVAL;
	}

	mtk_mmdvfs_enable_vcp(true, user[step_idx[idx]].id);
	ret = clk_set_rate(user[step_idx[idx]].clk, mmdvfs_user_get_freq_by_opp(user[step_idx[idx]].id, opp));
	if (!ret)
		user[step_idx[idx]].vote_opp = opp;
	mtk_mmdvfs_enable_vcp(false, user[step_idx[idx]].id);

	return ret;
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

	mtk_mmdvfs_enable_vcp(true, user[idx].id);
	ret = clk_set_rate(user[idx].clk, mmdvfs_user_get_freq_by_opp(user[idx].id, opp));
	if (!ret)
		user[idx].vote_opp = opp;
	mtk_mmdvfs_enable_vcp(false, user[idx].id);

	return ret;
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
			ret = clk_set_rate(user[i].clk, mmdvfs_user_get_freq_by_opp(user[i].id, OPP_NAG));
		}

	return ret;
}

static struct kernel_param_ops mmdvfs_debug_freerun_ops = {
	.set = mmdvfs_debug_freerun,
};
module_param_cb(freerun, &mmdvfs_debug_freerun_ops, NULL, 0644);
MODULE_PARM_DESC(freerun, "freerun by rc id");

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
		mmdvfs_seq_print(file, "mux:%d [%#x]=%#x",
			i, mux_base_pa + mux_offset[i], readl(mux_base + mux_offset[i]));

	for (i = 0; i < fmeter_count; i++) {
		val = mt_get_fmeter_freq(fmeter_id[i], fmeter_type[i]);
		mmdvfs_seq_print(file, "fmeter:%d id:%hhu type:%hhu freq:%u", i, fmeter_id[i], fmeter_type[i], val);
	}

	mmdvfs_dump_dvfsrc_rg();

	return 0;
}

static int mmdvfs_debug_v5_status_dump(struct seq_file *file)
{
	u32 val;
	int i, j, k, ret;

	ret = mmdvfs_debug_dump_volt_freq(file);

	if (DRAM_VCP_BASE) {
		// user vote history
		for (i = 0; i < DRAM_USR_NUM_MAX; i++) {
			k = readl(DRAM_USR_IDX(i)) % DRAM_REC_CNT;
			for (j = k; j < DRAM_REC_CNT; j++) {
				val = readl(DRAM_USR_VAL(i, j));
				if (!readl(DRAM_USR_SEC(i, j)) && !DRAM_DEC_USR_USEC(val))
					continue;
				mmdvfs_seq_print(file, "[%5u.%3u] user:%u pwr:%u lvl:%u", readl(DRAM_USR_SEC(i, j)),
					DRAM_DEC_USR_USEC(val), i, DRAM_DEC_USR_PWR(val), DRAM_DEC_USR_LVL(val));
			}
			for (j = 0; j < k; j++) {
				val = readl(DRAM_USR_VAL(i, j));
				if (!readl(DRAM_USR_SEC(i, j)) && !DRAM_DEC_USR_USEC(val))
					continue;
				mmdvfs_seq_print(file, "[%5u.%3u] user:%u pwr:%u lvl:%u", readl(DRAM_USR_SEC(i, j)),
					DRAM_DEC_USR_USEC(val), i, DRAM_DEC_USR_PWR(val), DRAM_DEC_USR_LVL(val));
			}
		}
	}

	mtk_mmdvfs_enable_vcp(true, user ? user[0].id : 0);
	mmdvfs_mmup_cb_mutex_lock();
	ret = mmdvfs_mmup_cb_ready_get();
	if (!ret || !unlikely(SRAM_BASE)) {
		mmdvfs_mmup_cb_mutex_unlock();
		mtk_mmdvfs_enable_vcp(false, user ? user[0].id : 0);
		mmdvfs_seq_print(file, "mmup_cb_ready:%d mmup_sram:%#lx", ret, (unsigned long)(void *)SRAM_BASE);
		return 0;
	}

	mmdvfs_seq_print(file, "mmup_sram:%#lx", (unsigned long)(void *)SRAM_BASE);

	// vcore dvs, vmm dvs, vdisp dvs, vmm dfs, vdisp dfs
	for (i = 0; i < SRAM_IRQ_CNT; i++) {
		k = readl(SRAM_IRQ_IDX(i)) % SRAM_REC_CNT;
		for (j = k; j < SRAM_REC_CNT; j++) {
			val = readl(SRAM_IRQ_VAL(i, j));
			mmdvfs_seq_print(file, "[%5u.%3u] (%d, %d) irq:%u lvl:%u", readl(SRAM_IRQ_SEC(i, j)),
				SRAM_DEC_USEC(val), i, j, SRAM_DEC_IDX(val), SRAM_DEC_LVL(val));
		}
		for (j = 0; j < k; j++) {
			val = readl(SRAM_IRQ_VAL(i, j));
			mmdvfs_seq_print(file, "[%5u.%3u] (%d, %d) irq:%u lvl:%u", readl(SRAM_IRQ_SEC(i, j)),
				SRAM_DEC_USEC(val), i, j, SRAM_DEC_IDX(val), SRAM_DEC_LVL(val));
		}
	}

	// vcore, vmm, vdisp
	for (i = 0; i < SRAM_PWR_CNT; i++) {
		k = readl(SRAM_PWR_IDX(i)) % SRAM_REC_CNT;
		for (j = k; j < SRAM_REC_CNT; j++) {
			val = readl(SRAM_PWR_VAL(i, j));
			mmdvfs_seq_print(file, "[%5u.%3u] (%d, %d) pwr:%u lvl:%u", readl(SRAM_PWR_SEC(i, j)),
				SRAM_DEC_USEC(val), i, j, SRAM_DEC_IDX(val), SRAM_DEC_LVL(val));
		}
		for (j = 0; j < k; j++) {
			val = readl(SRAM_PWR_VAL(i, j));
			mmdvfs_seq_print(file, "[%5u.%3u] (%d, %d) pwr:%u lvl:%u", readl(SRAM_PWR_SEC(i, j)),
				SRAM_DEC_USEC(val), i, j, SRAM_DEC_IDX(val), SRAM_DEC_LVL(val));
		}
	}

	// vcore, vmm, vdisp, cam, hop
	for (i = 0; i < SRAM_CLK_CNT; i++) {
		k = readl(SRAM_CLK_IDX(i)) % SRAM_REC_CNT;
		for (j = k; j < SRAM_REC_CNT; j++) {
			val = readl(SRAM_CLK_VAL(i, j));
			mmdvfs_seq_print(file, "[%5u.%3u] (%d, %d) clk:%u lvl:%u", readl(SRAM_CLK_SEC(i, j)),
				SRAM_DEC_USEC(val), i, j, SRAM_DEC_IDX(val), SRAM_DEC_LVL(val));
		}
		for (j = 0; j < k; j++) {
			val = readl(SRAM_CLK_VAL(i, j));
			mmdvfs_seq_print(file, "[%5u.%3u] (%d, %d) clk:%u lvl:%u", readl(SRAM_CLK_SEC(i, j)),
				SRAM_DEC_USEC(val), i, j, SRAM_DEC_IDX(val), SRAM_DEC_LVL(val));
		}
	}

	// vcore, vmm, vdisp
	for (i = 0; i < SRAM_CEIL_CNT; i++) {
		k = readl(SRAM_CEIL_IDX(i)) % SRAM_REC_CNT;
		for (j = k; j < SRAM_REC_CNT; j++) {
			val = readl(SRAM_CEIL_VAL(i, j));
			mmdvfs_seq_print(file, "[%5u.%3u] (%d, %d) idx:%u ceil:%u", readl(SRAM_CEIL_SEC(i, j)),
				SRAM_DEC_USEC(val), i, j, SRAM_DEC_IDX(val), SRAM_DEC_LVL(val));
		}
		for (j = 0; j < k; j++) {
			val = readl(SRAM_CEIL_VAL(i, j));
			mmdvfs_seq_print(file, "[%5u.%3u] (%d, %d) idx:%u ceil:%u", readl(SRAM_CEIL_SEC(i, j)),
				SRAM_DEC_USEC(val), i, j, SRAM_DEC_IDX(val), SRAM_DEC_LVL(val));
		}
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

static struct mmdvfs_debug_ops mmdvfs_debug_v5_ops = {
	.force_step_fp = mmdvfs_debug_v5_set_force_step,
	.vote_step_fp = mmdvfs_debug_v5_set_vote_step,
	.status_dump_fp = mmdvfs_debug_v5_status_dump,
	.force_vcore_fp = mmdvfs_debug_v5_force_vcore,
	.ap_set_rate_fp = mmdvfs_debug_v5_ap_set_rate,
};

static int mmdvfs_debug_pm_notifier(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	int i;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		MMDVFS_DBG("PM_SUSPEND_PREPARE in");
		for (i = 0; i < step_count; i++) {
			if (unlikely(user[i].force_opp != OPP_NAG || user[i].vote_opp != OPP_NAG))
				MMDVFS_DBG("user i:%d id:%hhu name:%16s force:%hhd vote:%hhd not release at suspend",
					i, user[i].id, user[i].name, user[i].force_opp, user[i].vote_opp);
			if (unlikely(user[i].force_opp != OPP_NAG))
				mmdvfs_debug_force_step(i, OPP_NAG);
			if (unlikely(user[i].vote_opp != OPP_NAG))
				mmdvfs_debug_vote_step(i, OPP_NAG);
		}
		break;
	case PM_POST_SUSPEND:
		MMDVFS_DBG("PM_POST_SUSPEND in");
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

		MMDVFS_DBG("user:%p count:%hhu i:%2d id:%2hhu rc:%hhu name:%16s clk:%p force:%hhd vote:%hhd",
			*_user, *count, i, user[i].id, user[i].rc, user[i].name, user[i].clk, user[i].force_opp, user[i].vote_opp);

		i += 1;
	}

	return ret;
}

static int mmdvfs_debug_kthread(void *data)
{
	phys_addr_t pa = 0ULL;
	unsigned long va;
	int ret;

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

	met_freerun = of_property_read_bool(dev->of_node, "mediatek,met-freerun");

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

	//mtk_mmdebug_status_dump_register_notifier(&mmdebug_nb);
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

