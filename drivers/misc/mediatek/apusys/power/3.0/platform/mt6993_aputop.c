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
#include <linux/thermal.h>
#include <linux/pm.h>
// for thermal node & apu_sw_power throttle
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/timer.h>
// end
#include "apusys_secure.h"
#include "aputop_rpmsg.h"
#include "apu_top.h"
#include "aputop_log.h"
#include "mt6993_apupwr.h"
#include "mt6993_apupwr_prot.h"
#include "mt6993_apupwr_ce.h"
#include "aputop_cdev.h"

#define LOCAL_DBG	(0)
#define NEED_CHK	(0)
#define RPC_ALIVE_DBG	(0)
#define SMC_APUSYS_PWR_DUMP	(0)
#define TIMER_RDY	(0)
#define CLIENT_NUM	(6)
#define SW_THROTTLE_PT_THERMAL	(0)
#define SW_THROTTLE_SYSFS	(1)
#define SW_THROTTLE_LIMIT_HAL	(2)
static uint32_t mbox_data;

static struct apu_power apupw = {
	.env = MP,
};

static int global_upper_limit = USER_MAX_OPP_VAL - 1;
static int global_lower_limit = USER_MIN_OPP_VAL + 1;
static struct mutex lock;
static int sys_request_id = 5; // for sysfs input
static int limit_debug_request_id = 4; // for Limit_HAL cmd input
static int first_dump;

struct client_work {
	int lower_limit;
	int upper_limit;
	int request_id;
	struct list_head list;
};

static LIST_HEAD(client_list);

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

#if TIMER_RDY
static struct timer_list limit_timer;
static int limit_timer_active;
static int last_upper_limit = -1;
static int last_lower_limit = -1;
static int last_upper_request_id = -1;
static int last_lower_request_id = -1;

static void limit_timer_callback(struct timer_list *t)
{
	if (global_upper_limit == last_upper_limit &&
	    global_lower_limit == last_lower_limit) {
		pr_info("Frequency limit has been active for more than 100ms:\n"
			"\t\tupper_limit=%d (from user %d), lower_limit=%d (from user %d)\n",
			global_upper_limit, last_upper_request_id,
			global_lower_limit, last_lower_request_id);
		mod_timer(&limit_timer, jiffies + msecs_to_jiffies(100));
	} else {
		limit_timer_active = 0;
	}
}
#endif

/* for apu_sw_throttle update upper/lower bounds */
static int mt6993_update_bounds(void)
{
	int new_upper_limit = USER_MAX_OPP_VAL - 1;
	int new_lower_limit = USER_MIN_OPP_VAL + 1;
	int irregular_limits[CLIENT_NUM];
	int irregular_count = 0;
	int skip = 0;
	struct client_work *cw;

	list_for_each_entry(cw, &client_list, list) {
		if (cw->upper_limit > new_upper_limit)
			new_upper_limit = cw->upper_limit;
		if (cw->lower_limit < new_lower_limit)
			new_lower_limit = cw->lower_limit;
	}

	/* Skip irregular lower_limit and reset new lower_limits */
	if (new_lower_limit < new_upper_limit) {
		pr_info("%s: lower_limit %d cannot be greater than upper_limit %d, Error.\n",
				__func__, new_lower_limit, new_upper_limit);
		list_for_each_entry(cw, &client_list, list) {
			if (cw->lower_limit == new_lower_limit) {
				irregular_limits[irregular_count++] = new_lower_limit;
				pr_info("%s: skip modify lower_limit here.\n", __func__);
			}
		}

		new_upper_limit = USER_MAX_OPP_VAL - 1;
		new_lower_limit = USER_MIN_OPP_VAL + 1;
		list_for_each_entry(cw, &client_list, list) {
			skip = 0;
			for (int i = 0; i < irregular_count; i++) {
				if (cw->lower_limit == irregular_limits[i]) {
					skip = 1;
					break;
				}
			}
			if (skip)
				continue;
			if (cw->upper_limit > new_upper_limit)
				new_upper_limit = cw->upper_limit;
			if (cw->lower_limit < new_lower_limit)
				new_lower_limit = cw->lower_limit;
		}
#if LOCAL_DBG
		pr_info("%s: Now new_upper_limit = %d, new_lower_limit = %d\n",
				__func__, new_upper_limit, new_lower_limit);
#endif
	}

	if (new_upper_limit == global_upper_limit && new_lower_limit == global_lower_limit) {
#if LOCAL_DBG
		pr_info("%s: bounds not changed.\n", __func__);
#endif
		return -EAGAIN;
	}

#if TIMER_RDY
	// Reset the timer
	if (limit_timer_active)
		del_timer(&limit_timer);

	if (new_upper_limit > global_lower_limit) {
		last_upper_limit = new_upper_limit;
		last_upper_request_id = cw->request_id;
	}

	if (new_lower_limit < global_upper_limit) {
		last_lower_limit = new_lower_limit;
		last_lower_request_id = cw->request_id;
	}

	limit_timer_active = 1;
	mod_timer(&limit_timer, jiffies + msecs_to_jiffies(100));
#endif

	global_upper_limit = new_upper_limit;
	global_lower_limit = new_lower_limit;

	return 0;
}

#if NEED_CHK
static void mt6993_verify_bounds(void)
{
	struct client_work *cw;
	int expected_lower_limit = USER_MIN_OPP_VAL + 1;
	int expected_upper_limit = USER_MAX_OPP_VAL - 1;
	int irregular_limits[CLIENT_NUM];
	int irregular_count = 0;
	int skip;

	list_for_each_entry(cw, &client_list, list) {
		if (cw->lower_limit < expected_lower_limit)
			expected_lower_limit = cw->lower_limit;
		if (cw->upper_limit > expected_upper_limit)
			expected_upper_limit = cw->upper_limit;
	}

	if (expected_lower_limit < expected_upper_limit) {
		list_for_each_entry(cw, &client_list, list) {
			if (cw->lower_limit == expected_lower_limit)
				irregular_limits[irregular_count++] = expected_lower_limit;
		}

		expected_lower_limit = USER_MIN_OPP_VAL + 1;
		expected_upper_limit = USER_MAX_OPP_VAL - 1;

		list_for_each_entry(cw, &client_list, list) {
			skip = 0;
			for (int i = 0; i < irregular_count; i++) {
				if (cw->lower_limit == irregular_limits[i]) {
					skip = 1;
					break;
				}
			}
			if (skip)
				continue;
			if (cw->lower_limit < expected_lower_limit)
				expected_lower_limit = cw->lower_limit;
			if (cw->upper_limit > expected_upper_limit)
				expected_upper_limit = cw->upper_limit;
		}
	}

	if (expected_lower_limit != global_lower_limit || expected_upper_limit != global_upper_limit) {
		pr_info("%s: Limit inconsistency detected: expected lower %d, upper %d, but got lower %d, upper %d\n",
				__func__, expected_lower_limit, expected_upper_limit,
				global_lower_limit, global_upper_limit);
	}

}
#endif

/* maintain nodes & judge final upper_limit & lower_limit to APU */
int mt6993_set_freq_limit(int upper_limit, int lower_limit, int *request_id, int calltype)
{
	struct client_work *cw;
	bool found = false;
	int ret;
	int type = calltype;

	// mapping user opp to real opp
	if (type == SW_THROTTLE_SYSFS) { // sysfs node
		upper_limit = upper_limit + THERMAL_OPP_OFS;
		lower_limit = lower_limit + THERMAL_OPP_OFS;
	} else if (type == SW_THROTTLE_PT_THERMAL) // thermal/PT
		upper_limit = upper_limit + THERMAL_OPP_OFS;
	// type = 2 -> Limit HAL cmd -> do not shift.

	// real opp range is from 0 to 15
	if ((lower_limit > USER_MIN_OPP_VAL || lower_limit < USER_MAX_OPP_VAL) ||
		(upper_limit > USER_MIN_OPP_VAL || upper_limit < USER_MAX_OPP_VAL)) {
#if LOCAL_DBG
		pr_info("%s: Error, limits out of range: lower_limit (%d), upper_limit (%d)\n",
				__func__, lower_limit, upper_limit);
#endif
		if (lower_limit > USER_MIN_OPP_VAL) {
			lower_limit = USER_MIN_OPP_VAL;
#if LOCAL_DBG
			pr_info("%s: setting lower_limit to %d\n",
				__func__, lower_limit);
#endif
			}
		else
			return -ERANGE;
	}

	if (lower_limit < upper_limit) {
#if LOCAL_DBG
		pr_info("%s: Error, upper_limit (%d) cannot be bigger than lower_limit (%d)\n",
				__func__, lower_limit, upper_limit);
#endif
		return -EINVAL;
	}

	mutex_lock(&lock);
	list_for_each_entry(cw, &client_list, list) {
		if (cw->request_id == *request_id) {
			found = true;
			break;
		}
	}

	if (found) {
		/* update existing nodes values */
		cw->lower_limit = lower_limit;
		cw->upper_limit = upper_limit;
#if LOCAL_DBG
		pr_info("%s: updated existing node, and request_id is %d\n",
				__func__, *request_id);
#endif
		} else {
			cw = kmalloc(sizeof(*cw), GFP_KERNEL);
		if (!cw) {
			mutex_unlock(&lock);
			return -ENOMEM;
		}
		cw->lower_limit = lower_limit;
		cw->upper_limit = upper_limit;
		/* record id number */
		cw->request_id = *request_id;
		list_add(&cw->list, &client_list);
#if LOCAL_DBG
		pr_info("%s: added new node, and request_id is %d\n", __func__, cw->request_id);
#endif
	}

	ret = mt6993_update_bounds();
	if (ret == 0 && type == SW_THROTTLE_PT_THERMAL) {
#if LOCAL_DBG
		pr_info("%s: input from apu_sw_throttle, detected bounds changed, sending to apu\n", __func__);
#endif
		mt6993_aputop_opp_limit(global_upper_limit, global_lower_limit, 1);
	} else if (ret == 0 && type == SW_THROTTLE_SYSFS) {
#if LOCAL_DBG
		pr_info("%s: input from sysfs, detected bounds changed, sending to apu\n", __func__);
#endif
		mt6993_aputop_opp_limit(global_upper_limit, global_lower_limit, 2);
	} else if (ret == 0 && type == SW_THROTTLE_LIMIT_HAL) {
#if LOCAL_DBG
		pr_info("%s: input from limit hal cmd, detected bounds changed, sending to apu\n", __func__);
#endif
		mt6993_aputop_opp_limit(global_upper_limit, global_lower_limit, 2);
	} else {
#if LOCAL_DBG
		pr_info("%s: detected bounds not changed, skip sending to apu\n", __func__);
#endif
		mutex_unlock(&lock);
		return -EINVAL;
	}

#if NEED_CHK
	mt6993_verify_bounds();
#endif
	mutex_unlock(&lock);

	return ret;
}

static int mt6993_client_input_show(struct seq_file *m, void *v)
{
	struct client_work *cw;

	if (!first_dump) {
		mt6993_request_opp_table();
		first_dump = 1;
	}

	mutex_lock(&lock);

	if (list_empty(&client_list)) {
		seq_puts(m, "Client list is empty\n");
	} else {
		list_for_each_entry(cw, &client_list, list) {
			if(cw->request_id == 5)
				seq_printf(m, "%d,%d\n",
					opp_level_pll_freq[cw->upper_limit], opp_level_pll_freq[cw->lower_limit]);
		}
	}

	mutex_unlock(&lock);

	return 0;
}

static void mt6993_prepare_freq_input(int upper_limit, int lower_limit, int *opp_max, int *opp_min)
{
	int tmp_opp_min = -1;
	int tmp_opp_max = -1;

	/* if opp table is not dump, request opp tbl first */
	if (!first_dump) {
		mt6993_request_opp_table();
		first_dump = 1;
	}

	for (int i = OPP_TABLE_SIZE-1; i >= 0; i--) {
		int freq = opp_level_pll_freq[i];

		if (freq >= lower_limit) {
			tmp_opp_min = i;
			break;
		}
	}

	for (int i = 0; i < OPP_TABLE_SIZE; i++) {
		int freq = opp_level_pll_freq[i];

		if (freq <= upper_limit) {
			tmp_opp_max = i;
			break;
		}
	}

	if (upper_limit == lower_limit)
		tmp_opp_min = tmp_opp_max;

	if (lower_limit < opp_level_pll_freq[OPP_TABLE_SIZE-1])
		tmp_opp_min = 15 - THERMAL_OPP_OFS; // set to opp15

	if (upper_limit > opp_level_pll_freq[0])
		tmp_opp_max = USER_MAX_OPP_VAL; // set to opp0

	if (tmp_opp_min < tmp_opp_max) {
		pr_info("%s: opp_max=%d, opp_min=%d\n , lower limit cannot be greater than upper limit!\n",
				__func__, tmp_opp_max, tmp_opp_min);
	} else {
		pr_info("%s: opp_max=%d, opp_min=%d\n", __func__, tmp_opp_max, tmp_opp_min);
		*opp_max = tmp_opp_max;
		*opp_min = tmp_opp_min;
	}

}

static ssize_t mt6993_handle_client_input(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	int lower_limit, upper_limit;
	int ret;
	int opp_max, opp_min;
	char *input, *token;

	input = kzalloc(count + 1, GFP_KERNEL);
	if (!input)
		return -ENOMEM;

	if (copy_from_user(input, buf, count)) {
		kfree(input);
		return -EFAULT;
	}

	token = strsep(&input, " ");
	if (!token) {
		ret = -EINVAL;
		goto out;
	}

	ret = kstrtoint(token, 0, &upper_limit);
	if (ret)
		goto out;

	token = strsep(&input, " ");
	if (!token) {
		ret = -EINVAL;
		goto out;
	}

	ret = kstrtoint(token, 0, &lower_limit);
	if (ret)
		goto out;

	token = strsep(&input, " ");
	if (token && *token != '\0' && *token != '\n') {
		ret = -EINVAL;
		goto out;
	}

	if (lower_limit > upper_limit) {
		pr_debug("Upper limit is smaller than lower limit. Adjusting values.\n");
		ret = -EINVAL;
		goto out;
	}

	mt6993_prepare_freq_input(upper_limit, lower_limit, &opp_max, &opp_min);
	ret = mt6993_set_freq_limit(opp_max, opp_min, &sys_request_id, SW_THROTTLE_SYSFS);
	if (ret)
		goto out;

	if (sys_request_id != -1)
		pr_info("Generated request_id: %d\n", sys_request_id);

	ret = count;

out:
	kfree(input);
	return ret;
}

static int mt6993_client_input_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt6993_client_input_show, NULL);
}

static const struct proc_ops client_input_ops = {
	.proc_open    = mt6993_client_input_open,
	.proc_read    = seq_read,
	.proc_write   = mt6993_handle_client_input,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

void mt6993_activate_apu_cooling_device(struct platform_device *pdev)
{
	struct apu_cooling_device *apu_cdev = (struct apu_cooling_device *)platform_get_drvdata(pdev);

	if (apu_cdev->status == APUCDEV_NOT_READY){
		apu_cdev->target_state = APU_COOLING_UNLIMITED_STATE;
		apu_cdev->unlimite_state = APU_COOLING_UNLIMITED_STATE;
		apu_cdev->max_state = APU_COOLING_MAX_STATE;
		apu_cdev->status = APUCDEV_READY;
		thermal_cooling_device_update(apu_cdev->cdev);
		pr_info("%s: %s ready.\n", __func__, apu_cdev->name);
	}
}

static int mt6993_opp_proc_show(struct seq_file *m, void *v)
{
	int i;

	mt6993_request_opp_table();
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
	return single_open(file, mt6993_opp_proc_show, NULL);
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

	mt6993_activate_apu_cooling_device(pdev);

	mt6993_init_remote_data_sync(apupw.regs[apu_md32_mbox]);
	// init lock
	mutex_init(&lock);
	// init apudvfs proc
	apudvfs_dir = proc_mkdir("apudvfs", NULL);
	if (!apudvfs_dir)
		return -ENOMEM;

	if (!proc_create("apu_opp_table", 0, apudvfs_dir, &opp_proc_ops)) {
		remove_proc_entry("apudvfs", NULL);
		return -ENOMEM;
	}

	if (!proc_create("user_limit", 0644, apudvfs_dir, &client_input_ops)) {
		remove_proc_entry("apudvfs", NULL);
		return -ENOMEM;
	}

#if TIMER_RDY
	timer_setup(&limit_timer, limit_timer_callback, 0);
#endif

	return ret;
}

static int mt6993_apu_top_rm(struct platform_device *pdev)
{
	int idx;
	struct client_work *cw, *tmp;

	pr_info("%s +\n", __func__);
	if (apupw.env < MP)
		mt6993_all_off(pdev);
	for (idx = 0; idx < APUPW_MAX_REGS; idx++)
		iounmap(apupw.regs[idx]);
	pr_info("%s -\n", __func__);

	mutex_lock(&lock);
	list_for_each_entry_safe(cw, tmp, &client_list, list) {
		list_del(&cw->list);
		kfree(cw);
	}
	mutex_unlock(&lock);
	mutex_destroy(&lock);
	// rm client input and apudvfs opp table
	remove_proc_entry("user_limit", apudvfs_dir);
	remove_proc_entry("apu_opp_table", apudvfs_dir);
	remove_proc_entry("apudvfs", NULL);

#if TIMER_RDY
	del_timer(&limit_timer);
#endif

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
	int dla_max, dla_min;
	int request_id = -1;

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
		dla_max = aputop->param3;
		dla_min = aputop->param4;
		mt6993_set_freq_limit(dla_max, dla_min, &limit_debug_request_id, SW_THROTTLE_LIMIT_HAL);
		break;
	case APUTOP_FUNC_OPP_LIMIT_DBG:
		dla_max = aputop->param3;
		dla_min = aputop->param4;
		mt6993_set_freq_limit(dla_max, dla_min, &limit_debug_request_id, SW_THROTTLE_LIMIT_HAL);
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
	/* throttle from thermal & PT, calltype = 0 */
	case APUTOP_FUNC_APU_THROTTLE:
		dla_max = aputop->param1;
		dla_min = USER_MIN_OPP_VAL;
		request_id = aputop->param3;
		mt6993_set_freq_limit(dla_max, dla_min, &request_id, SW_THROTTLE_PT_THERMAL);
		aputop->param3 = request_id;
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
