// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
// for thermal node
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
// end
#include "apusys_secure.h"
#include "aputop_rpmsg.h"
#include "apu_top.h"
#include "aputop_log.h"
#include "mt6993_apupwr.h"
#include "mt6993_apupwr_prot.h"
#include "mt6993_apupwr_ce.h"

#define LOCAL_DBG	(1)
#define RPC_ALIVE_DBG	(0)
#define SMC_APUSYS_PWR_DUMP	(0)

static uint32_t mbox_data;

static struct apu_power apupw = {
	.env = MP,
};

#define _DOMAIN(_idx, _phy_addr, _size) \
	apupw.reg_name[_idx] = #_idx; \
	apupw.phy_addr[_idx] = _phy_addr; \
	apupw.remap_size[_idx] = _size;

static void init_reg_base(void)
{
	int idx;

	_DOMAIN(apu_rcx,		0x19020000, 0x1000)
	_DOMAIN(apu_are,		0x19040000, 0x11000)
	_DOMAIN(apu_vcore,		0x19004800, 0x4000)
	_DOMAIN(apu_md32_mbox,		0x4c2c0000, 0x2000)
//	_DOMAIN(apu_briske_del_sram,	0x19056000, 0x2000)
//	_DOMAIN(apu_briske_del,		0x19058000, 0x1000)
	_DOMAIN(apu_rpc,		0x19052000, 0x1000)
	_DOMAIN(apu_pcu,		0x19053000, 0x1000)
	_DOMAIN(apu_ao_ctl,		0x19038000, 0x1000)
	_DOMAIN(apu_acc,		0x1905b000, 0x3000)
	_DOMAIN(apu_pll,		0x19054000, 0x3000)
	_DOMAIN(apu_top_rpc_lite,	0x1905c000, 0x1000)
	_DOMAIN(apu_intc_cfg,		0x19250000, 0x1000)
	_DOMAIN(apu_ctrlsys_cfg,	0x19419000, 0x1000)
	_DOMAIN(mvpu_top_config,	0x1912b000, 0x1000)
	_DOMAIN(apu_dla_0_config,	0x19200000, 0x1000)
	_DOMAIN(apu_dla_1_config,	0x19210000, 0x1000)
	_DOMAIN(apu_dla_2_config,	0x19220000, 0x1000)
	_DOMAIN(apu_dla_3_config,	0x19230000, 0x1000)
	_DOMAIN(tinydla_top_config,	0x19242000, 0x1000)
	_DOMAIN(sys_vlp,		0x1c000000, 0x1000)
	_DOMAIN(sys_spm,		0x1c004000, 0x1000)

	for (idx = 0 ; idx < APUPW_MAX_REGS ; idx++) {
		apupw.regs[idx] = ioremap(
				apupw.phy_addr[idx],
				apupw.remap_size[idx]);

		pr_info("%s %s (0x%08x, 0x%p, 0x%08x)\n",
				__func__,
				apupw.reg_name[idx],
				apupw.phy_addr[idx],
				apupw.regs[idx],
				apupw.remap_size[idx]);
	}
}

#if APUPW_DUMP_FROM_APMCU
static void aputop_dump_reg(enum apupw_reg idx, uint32_t offset, uint32_t size)
{
	char buf[32];
	int ret = 0;

	pr_info("%s %d/0x%x/%u begin\n", __func__, idx, offset, size);
	/* prepare pa address */
	memset(buf, 0, sizeof(buf));
	ret = snprintf(buf, 32, "phys 0x%08lx: ",
			(ulong)(apupw.phy_addr[idx]) + offset);
	/* dump content with pa as prefix */
	if (ret)
		print_hex_dump(KERN_ERR, buf, DUMP_PREFIX_OFFSET, 16, 4,
			       apupw.regs[idx] + offset, size, true);
}
#endif

static uint32_t apusys_pwr_smc_call(struct device *dev, uint32_t smc_id,
		uint32_t a2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL, smc_id,
			a2, 0, 0, 0, 0, 0, &res);
	if (((int) res.a0) < 0)
		dev_info(dev, "%s: smc call %d return error(%lu)\n",
				__func__,
				smc_id, res.a0);

	return res.a0;
}

/*
 * sub_func id :
 *      0 - DRV_STAT_SYNC_REG
 *      1 - MBRAIN_DATA_SYNC_0_REG
 *      2 - MBRAIN_DATA_SYNC_1_REG
 */
static void plat_get_up_drv_data(struct aputop_func_param *aputop)
{
	int sub_func = 0;

	sub_func = aputop->param1;

	if (sub_func == 0) {
		mbox_data = apu_readl(
				apupw.regs[apu_md32_mbox] + DRV_STAT_SYNC_REG);
	} else if (sub_func == 1) {
		mbox_data = apu_readl(
				apupw.regs[apu_md32_mbox] + MBRAIN_DATA_SYNC_0_REG);
	} else if (sub_func == 2) {
		mbox_data = apu_readl(
				apupw.regs[apu_md32_mbox] + MBRAIN_DATA_SYNC_1_REG);
	} else {
		pr_info("%s#%d invalid sub_func : %d\n",
				__func__, __LINE__, sub_func);
	}
}

static void aputop_dump_pwr_reg(struct device *dev)
{
#if SMC_APUSYS_PWR_DUMP
	// dump reg in ATF log
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_DUMP,
			SMC_PWR_DUMP_ALL);
#endif
	// dump reg in AEE db
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_REGDUMP, 0);
}

#if APUPW_DUMP_FROM_APMCU
static void aputop_dump_pll_data(void)
{
	// need to 1-1 in order mapping with array in __apu_pll_init func
	uint32_t pll_base_arr[] = {MNOC_PLL_BASE, UP_PLL_BASE};
	uint32_t pll_offset_arr[] = {
				PLL1CPLL_FHCTL_HP_EN, PLL1CPLL_FHCTL_RST_CON,
				PLL1CPLL_FHCTL_CLK_CON, PLL1CPLL_FHCTL0_CFG,
				PLL1C_PLL1_CON1, PLL1CPLL_FHCTL0_DDS};
	int base_arr_size = ARRAY_SIZE(pll_base_arr);
	int offset_arr_size = ARRAY_SIZE(pll_offset_arr);
	int pll_idx;
	int ofs_idx;
	char buf[256];
	int ret = 0;

	for (pll_idx = 0 ; pll_idx < base_arr_size ; pll_idx++) {
		memset(buf, 0, sizeof(buf));
		for (ofs_idx = 0 ; ofs_idx < offset_arr_size ; ofs_idx++) {
			ret = snprintf(buf + strlen(buf),
					sizeof(buf) - strlen(buf),
					" 0x%08x",
					apu_readl(apupw.regs[apu_pll] +
						pll_base_arr[pll_idx] +
						pll_offset_arr[ofs_idx]));
			if (ret <= 0)
				break;
		}

		if (ret <= 0)
			break;
		pr_info("%s pll_base:0x%08x = %s\n", __func__,
				apupw.phy_addr[apu_pll] + pll_base_arr[pll_idx],
				buf);
	}
}
#endif

static int __apu_wake_rpc_rcx(struct device *dev)
{
	int ret = 0, val = 0;

	if (log_lvl)
		dev_info(dev, "%s before wakeup RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			 __func__,
			 (u32)(apupw.phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			 readl(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

	/* Change used RV SMC call to wake up RPC */
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_PWR_CTRL, 1);

	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL), 50, 10000);

	if (ret) {
		pr_info("%s polling RPC RDY timeout, ret %d\n", __func__, ret);
		/* show powerack info */
		dev_info(dev, "%s RCX APU_RPC_PWR_ACK 0x%x = 0x%x\n",
					 __func__,
					 (u32)(apupw.phy_addr[apu_rpc] + APU_RPC_PWR_ACK),
					 readl(apupw.regs[apu_rpc] + APU_RPC_PWR_ACK));
		goto out;
	}

	/*  show this once per 500ms */
	apu_info_ratelimited(dev, "%s after wakeup RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			     __func__,
			     (u32)(apupw.phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			     readl(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

	/* clear vcore/rcx cgs */
	apu_writel(0xFFFFFFFF, apupw.regs[apu_intc_cfg] + 0x8);
	apu_writel(0xFFFFFFFF, apupw.regs[apu_ctrlsys_cfg] + 0x8);
out:
	return ret;
}

static int mt6993_apu_top_on(struct device *dev)
{
	int ret = 0;

	if (apupw.env < MP)
		return 0;

	if (log_lvl)
		pr_info("%s +\n", __func__);

	ret = __apu_wake_rpc_rcx(dev);

	if (ret) {
		pr_info("%s fail to wakeup RPC, ret %d\n", __func__, ret);
		aputop_dump_pwr_reg(dev);
#if APUPW_DUMP_FROM_APMCU
		aputop_dump_pll_data();
#endif
		if (ret == -EIO)
			apupw_aee_warn("APUSYS_POWER",
					"APUSYS_POWER_RPC_CFG_ERR");
		else
			apupw_aee_warn("APUSYS_POWER",
					"APUSYS_POWER_WAKEUP_FAIL");
		return -1;
	}

	if (log_lvl)
		pr_info("%s -\n", __func__);

	return 0;
}

#if APMCU_REQ_RPC_SLEEP
// backup solution : send request for RPC sleep from APMCU
static int __apu_sleep_rpc_rcx(struct device *dev)
{
	// REG_WAKEUP_CLR
	pr_info("%s step1. set REG_WAKEUP_CLR\n", __func__);
	apu_setl((BIT(10) | BIT(14) | BIT(18)), apupw.regs[apu_rpc] + APU_RPC_TOP_CON);
	apu_setl((BIT(1) | BIT(2) | BIT(6) | BIT(7) | BIT(24)), apupw.regs[apu_rpc] + APU_RPC_TOP_CON);
	udelay(10);
	// mask RPC IRQ and bypass WFI
	pr_info("%s step2. mask RPC IRQ and bypass WFI\n", __func__);
	apu_setl(1 << 7, apupw.regs[apu_rpc] + APU_RPC_TOP_SEL);
	udelay(10);
	pr_info("%s step3. raise up sleep request.\n", __func__);
	apu_writel(1, apupw.regs[apu_rpc] + APU_RPC_TOP_CON);
	udelay(100);
	dev_info(dev, "%s RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			readl(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

	return 0;
}
#endif

static int mt6993_apu_top_off(struct device *dev)
{
	int ret = 0, val = 0;
	int rpc_timeout_val = 500000; // 500 ms

	if (apupw.env >= MP)
		return 0;

	if (log_lvl)
		pr_info("%s +\n", __func__);
#if APMCU_REQ_RPC_SLEEP
	__apu_sleep_rpc_rcx(dev);
#endif
	// blocking until sleep success or timeout, delay 50 us per round
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL) == 0x0, 50, rpc_timeout_val);

	if (ret) {
		pr_info("%s polling PWR RDY timeout\n", __func__);
	} else {
		ret = readl_relaxed_poll_timeout_atomic(
				(apupw.regs[apu_rpc] + APU_RPC_STATUS),
				val, (val & 0x1UL) == 0x1, 50, 10000);
		if (ret)
			pr_info("%s polling PWR STATUS timeout\n", __func__);
	}

	if (ret) {
		pr_info(
		"%s timeout to wait RPC sleep (val:%d), ret %d\n", __func__, rpc_timeout_val, ret);
		aputop_dump_pwr_reg(dev);
#if APUPW_DUMP_FROM_APMCU
		aputop_dump_pll_data();
#endif
		apupw_aee_warn("APUSYS_POWER", "APUSYS_POWER_SLEEP_TIMEOUT");
		return -1;
	}

	if (log_lvl)
		pr_info("%s -\n", __func__);

	return 0;
}

static int opp_proc_show(struct seq_file *m, void *v)
{
	int i;

	request_opp_table();
	seq_puts(m, "APU Support Frequency points(Unit is KHZ):\n");
	for (i = 0; i < ARRAY_SIZE(opp_level_pll_freq); i++) {
		if (opp_level_pll_freq[i] == 0)
			continue; /* cause bin 0.9v was set as opp0 */
		else if (opp_level_pll_freq[i] > 1000000)
			seq_printf(m, "%d\n", opp_level_pll_freq[i]);
		else
			seq_printf(m, " %d\n", opp_level_pll_freq[i]);
	}

	return 0;
}

static int opp_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, opp_proc_show, NULL);
}

static const struct proc_ops opp_proc_ops = {
	.proc_open    = opp_proc_open,
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

static struct proc_dir_entry *apudvfs_dir;

static int mt6993_apu_top_pb(struct platform_device *pdev)
{
	int ret = 0, val = 0;

	pr_info("%s +%d\n", __func__, apupw.env);

	init_reg_base();

	if (apupw.env < MP) {
		mt6993_power_init(pdev, &apupw); // remove lk2 init flow, init apu from here now

#if PRELOAD_ACE_FW
		ret = mt6993_load_ce_bin();
		if (ret != 0)
			pr_info("%s load fw error\n", __func__);
#else
		pr_info("%s bypass load fw in aputop\n", __func__);
#endif
		ret = mt6993_all_on(pdev);
	} else {
		mt6993_apu_top_on(&pdev->dev);

		ret = readl_relaxed_poll_timeout_atomic(
				(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
				val, (val & 0x1UL), 50, 10000);
	}

	mt6993_init_remote_data_sync(apupw.regs[apu_md32_mbox]);

	apudvfs_dir = proc_mkdir("apudvfs", NULL);
	if (!apudvfs_dir)
		return -ENOMEM;

	if (!proc_create("apu_opp_table", 0, apudvfs_dir, &opp_proc_ops)) {
		remove_proc_entry("apudvfs", NULL);
		return -ENOMEM;
	}

	return ret;
}

static int mt6993_apu_top_rm(struct platform_device *pdev)
{
	int idx;

	pr_info("%s +\n", __func__);
	if (apupw.env < MP)
		mt6993_all_off(pdev);
	for (idx = 0; idx < APUPW_MAX_REGS; idx++)
		iounmap(apupw.regs[idx]);
	pr_info("%s -\n", __func__);

	return 0;
}

static int mt6993_apu_top_suspend(struct device *dev)
{
	return 0;
}

static int mt6993_apu_top_resume(struct device *dev)
{
	return 0;
}

static int mt6993_apu_top_func_return_val(int func_id, char *buf)
{
	if (func_id == APUTOP_FUNC_GET_UP_DATA) {
		return snprintf(buf, 64,
				"func_id:%d, aputop_func_return_val:0x%08x\n",
				func_id, mbox_data);
	} else {
		pr_info("%s func_id %d, NOT supported\n", __func__, func_id);
	}

	return 0;
}

static int mt6993_apu_top_func(struct platform_device *pdev,
		enum aputop_func_id func_id, struct aputop_func_param *aputop)
{
	pr_info("%s func_id : %d\n", __func__, aputop->func_id);

	switch (aputop->func_id) {
#if APMCU_REQ_RPC_SLEEP
	case APUTOP_FUNC_PWR_OFF:
		mt6993_apu_top_off(&pdev->dev);
		break;
	case APUTOP_FUNC_PWR_ON:
		mt6993_all_on(pdev);
		break;
#endif
	case APUTOP_FUNC_OPP_LIMIT_HAL:
		mt6993_aputop_opp_limit(aputop, OPP_LIMIT_HAL);
		break;
	case APUTOP_FUNC_OPP_LIMIT_DBG:
		mt6993_aputop_opp_limit(aputop, OPP_LIMIT_DEBUG);
		break;
	case APUTOP_FUNC_DUMP_REG:
		aputop_dump_pwr_reg(&pdev->dev);
		break;
	case APUTOP_FUNC_DRV_CFG:
	case APUTOP_FUNC_FEATURE_OPTION_0:
	case APUTOP_FUNC_FEATURE_OPTION_1:
		mt6993_drv_cfg_remote_sync(aputop);
		break;
#if APUPW_DUMP_FROM_APMCU
	case APUTOP_FUNC_ARE_DUMP1:
	case APUTOP_FUNC_ARE_DUMP2:
		aputop_dump_reg(apu_are, 0x0, 0x20); // are sram init config
		aputop_dump_reg(apu_are, 0x10b8, 0x200); // are sram
		aputop_dump_reg(apu_are, 0x10000, 0x20); // are hw
		aputop_dump_reg(apu_are, 0x10110, 0x80); // are hw
		break;
#endif
	case APUTOP_FUNC_GET_UP_DATA:
		plat_get_up_drv_data(aputop);
		break;
	default:
		pr_info("%s invalid func_id : %d\n", __func__, aputop->func_id);
		return -EINVAL;
	}

	return 0;
}

/* call by mt6993_pwr_func.c */
void mt6993_apu_dump_rpc_status(enum t_acx_id id, struct rpc_status_dump *dump)
{
}

const struct apupwr_plat_data mt6993_plat_data = {
	.plat_name = "mt6993_apupwr",
	.plat_aputop_on = mt6993_apu_top_on,
	.plat_aputop_off = mt6993_apu_top_off,
	.plat_aputop_pb = mt6993_apu_top_pb,
	.plat_aputop_rm = mt6993_apu_top_rm,
	.plat_aputop_suspend = mt6993_apu_top_suspend,
	.plat_aputop_resume = mt6993_apu_top_resume,
	.plat_aputop_func = mt6993_apu_top_func,
	.plat_aputop_func_return_val = mt6993_apu_top_func_return_val,
#if IS_ENABLED(CONFIG_DEBUG_FS)
	.plat_aputop_dbg_open = mt6993_apu_top_dbg_open,
	.plat_aputop_dbg_write = mt6993_apu_top_dbg_write,
#endif
	.plat_rpmsg_callback = mt6993_apu_top_rpmsg_cb,
	.bypass_pwr_on = 0,
	.bypass_pwr_off = 0,
};
