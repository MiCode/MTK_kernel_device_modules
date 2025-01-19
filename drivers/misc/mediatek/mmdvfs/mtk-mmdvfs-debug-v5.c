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

static u8 user_count;
struct mmdvfs_debug_user {
	u8 idx;
	const char *name;
	struct clk *clk;
	s8 force_opp;
	s8 vote_opp;
};
static struct mmdvfs_debug_user *user;

int mmdvfs_debug_force_step(const u8 idx, const s8 opp)
{
	int ret;

	if (idx >= user_count) {
		MMDVFS_ERR("invalide idx:%hhu opp:%hhd", idx, opp);
		return -EINVAL;
	}

	mtk_mmdvfs_enable_vcp(true, user[idx].idx);
	ret = mmdvfs_force_step(idx, opp);
	if (!ret)
		user[idx].force_opp = opp;
	mtk_mmdvfs_enable_vcp(false, user[idx].idx);

	return ret;
}
EXPORT_SYMBOL_GPL(mmdvfs_debug_force_step);

int mmdvfs_debug_vote_step(const u8 idx, const s8 opp)
{
	int ret;

	if (idx >= user_count) {
		MMDVFS_ERR("invalide idx:%hhu opp:%hhd", idx, opp);
		return -EINVAL;
	}

	mtk_mmdvfs_enable_vcp(true, user[idx].idx);
	ret = clk_set_rate(user[idx].clk, mmdvfs_user_get_freq_by_opp(user[idx].idx, opp));
	if (!ret)
		user[idx].vote_opp = opp;
	mtk_mmdvfs_enable_vcp(false, user[idx].idx);

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

static int mmdvfs_debug_dump_volt_freq(struct seq_file *file)
{
	u32 val;
	int i;

	if (!IS_ERR_OR_NULL(reg_vcore))
		mmdvfs_seq_print(file, "vcore enabled:%d voltage:%d",
			regulator_is_enabled(reg_vcore), regulator_get_voltage(reg_vcore));

	if (!IS_ERR_OR_NULL(reg_vmm))
		mmdvfs_seq_print(file, "vmm enabled:%d voltage:%d",
			regulator_is_enabled(reg_vmm), regulator_get_voltage(reg_vmm));

	if (!IS_ERR_OR_NULL(reg_vdisp))
		mmdvfs_seq_print(file, "vdisp enabled:%d voltage:%d",
			regulator_is_enabled(reg_vdisp), regulator_get_voltage(reg_vdisp));

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
	ret = mmdvfs_dump_dvfsrc_record();

	// TODO : dram, reg

	mmdvfs_mmup_cb_mutex_lock();
	ret = mmdvfs_mmup_cb_ready_get();
	if (!ret || !unlikely(SRAM_BASE)) {
		mmdvfs_mmup_cb_mutex_unlock();
		mmdvfs_seq_print(file, "mmup_cb_ready:%d mmup_sram:%#lx", ret, (unsigned long)(void *)SRAM_BASE);
		return 0;
	}
	mtk_mmdvfs_enable_vcp(true, user ? user[0].idx : 0);

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

	mtk_mmdvfs_enable_vcp(false, user ? user[0].idx : 0);
	mmdvfs_mmup_cb_mutex_unlock();

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
};

static int mmdvfs_debug_pm_notifier(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	int i;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		MMDVFS_DBG("PM_SUSPEND_PREPARE in");
		for (i = 0; i < user_count; i++) {
			if (unlikely(user[i].force_opp != OPP_NAG || user[i].vote_opp != OPP_NAG))
				MMDVFS_DBG("user i:%d idx:%hhu name:%16s force:%hhd vote:%hhd not release at suspend",
					i, user[i].idx, user[i].name, user[i].force_opp, user[i].vote_opp);
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

static inline int mmdvfs_debug_parse_user(struct device *dev)
{
	struct property *prop;
	const char *name;
	struct clk *clk;
	int i = 0, ret;

	ret = of_property_count_strings(dev->of_node, "clock-names");
	if (ret <= 0) {
		MMDVFS_DBG("count_strings clock-names failed:%d", ret);
		return ret;
	}
	user_count = ret;

	user = kcalloc(user_count, sizeof(*user), GFP_KERNEL);
	if (!user)
		return -ENOMEM;

	of_property_for_each_string(dev->of_node, "clock-names", prop, name) {
		struct of_phandle_args spec;

		ret = of_parse_phandle_with_args(dev->of_node, "clocks", "#clock-cells", i, &spec);
		if (!ret)
			user[i].idx = spec.args[0];

		clk = devm_clk_get(dev, name);
		if (!IS_ERR_OR_NULL(clk))
			user[i].clk = clk;

		user[i].name = name;
		user[i].force_opp = OPP_NAG;
		user[i].vote_opp = OPP_NAG;

		MMDVFS_DBG("user i:%d idx:%hhu name:%16s clk:%p force:%hhd vote:%hhd",
			i, user[i].idx, user[i].name, user[i].clk, user[i].force_opp, user[i].vote_opp);

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

	mmdvfs_debug_parse_proc();
	mmdvfs_debug_parse_regulator(dev);
	mmdvfs_debug_parse_mux(dev);
	mmdvfs_debug_parse_fmeter(dev);
	mmdvfs_debug_parse_user(dev);

	met_freerun = of_property_read_bool(dev->of_node, "mediatek,met-freerun");

	MMDVFS_DBG("mux_base:%pa mux_count:%hu fmeter_count:%hhu user_count:%hhu met_freerun:%d",
		&mux_base_pa, mux_count, fmeter_count, user_count, met_freerun);

	register_pm_notifier(&mmdvfs_debug_pm_notifier_block);
	mmdvfs_debug_ops_set(&mmdvfs_debug_v5_ops);

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
		.compatible = "mediatek,mmdvfs-debug-v5",
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

static int __init mmdvfs_debug_init(void)
{
	int ret;

	ret = platform_driver_register(&mmdvfs_debug_drv);
	if (ret)
		MMDVFS_DBG("failed:%d", ret);

	return ret;
}

static void __exit mmdvfs_debug_exit(void)
{
	platform_driver_unregister(&mmdvfs_debug_drv);
}

module_init(mmdvfs_debug_init);
module_exit(mmdvfs_debug_exit);
MODULE_DESCRIPTION("MMDVFS Debug V5 Driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL");

