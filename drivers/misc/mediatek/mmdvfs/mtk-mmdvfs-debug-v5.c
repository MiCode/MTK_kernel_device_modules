// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

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

int mmdvfs_debug_force_step(const u8 pwr_idx, const s8 opp)
{
	// TODO
	MMDVFS_DBG("pwr_idx:%hhu opp:%hhd", pwr_idx, opp);
	return 0;
}
EXPORT_SYMBOL_GPL(mmdvfs_debug_force_step);

int mmdvfs_debug_vote_step(const u8 pwr_idx, const s8 opp)
{
	// TODO
	MMDVFS_DBG("pwr_idx:%hhu opp:%hhd", pwr_idx, opp);
	return 0;
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

	return 0;
}

static int mmdvfs_debug_v5_status_dump(struct seq_file *file)
{
	u32 val;
	int i, j, k, ret;

	ret = mmdvfs_debug_dump_volt_freq(file);

	// TODO : dram, reg

	mmdvfs_mmup_cb_mutex_lock();
	ret = mmdvfs_mmup_cb_ready_get();
	if (!ret || !unlikely(SRAM_BASE)) {
		mmdvfs_mmup_cb_mutex_unlock();
		mmdvfs_seq_print(file, "mmup_cb_ready:%d mmup_sram:%#lx", ret, (unsigned long)(void *)SRAM_BASE);
		return 0;
	}
	mtk_mmdvfs_enable_vcp(true, 0); // TODO : user_idx

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

	// vcore, vmm, vdisp, ceil
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

	mtk_mmdvfs_enable_vcp(false, 0); // TODO : user_idx
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
	struct proc_dir_entry *dir, *proc;
	struct regulator *reg;
	struct task_struct *task;
	int ret;

	// proc
	dir = proc_mkdir("mmdvfs", NULL);
	if (IS_ERR_OR_NULL(dir))
		MMDVFS_DBG("proc_mkdir failed:%ld", PTR_ERR(dir));

	proc = proc_create("mmdvfs_opp", 0440, dir, &mmdvfs_debug_fops);
	if (IS_ERR_OR_NULL(proc))
		MMDVFS_DBG("proc_create failed:%ld", PTR_ERR(proc));

	// regulator
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

	// mux
	ret = of_property_read_u32(dev->of_node, "mux-base", &mux_base_pa);
	if (!ret) {
		mux_base = ioremap(mux_base_pa, 0x1000);

		ret = of_property_count_u16_elems(dev->of_node, "mux-offset");
		if (ret > 0) {
			mux_count = ret;
			mux_offset = kcalloc(mux_count, sizeof(*mux_offset), GFP_KERNEL);
			if (mux_offset)
				ret = of_property_read_u16_array(dev->of_node, "mux-offset", mux_offset, mux_count);
		} else
			MMDVFS_DBG("count_u8_elems mux-offset failed:%d", ret);
	} else
		MMDVFS_DBG("read_u32 mux-base failed:%d", ret);

	// fmeter
	ret = of_property_count_u8_elems(dev->of_node, "fmeter-id");
	if (ret > 0) {
		fmeter_count = ret;
		fmeter_id = kcalloc(fmeter_count, sizeof(*fmeter_id), GFP_KERNEL);
		if (fmeter_id)
			ret = of_property_read_u8_array(dev->of_node, "fmeter-id", fmeter_id, fmeter_count);

		fmeter_type = kcalloc(fmeter_count, sizeof(*fmeter_type), GFP_KERNEL);
		if (fmeter_type)
			ret = of_property_read_u8_array(dev->of_node, "fmeter-type", fmeter_type, fmeter_count);
	} else
		MMDVFS_DBG("count_u8_elems fmeter-id failed:%d", ret);

	met_freerun = of_property_read_bool(dev->of_node, "mediatek,met-freerun");

	mmdvfs_debug_ops_set(&mmdvfs_debug_v5_ops);

	MMDVFS_DBG("mux_base:%pa mux_count:%hu fmeter_count:%hhu met_freerun:%d",
		&mux_base_pa, mux_count, fmeter_count, met_freerun);

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

