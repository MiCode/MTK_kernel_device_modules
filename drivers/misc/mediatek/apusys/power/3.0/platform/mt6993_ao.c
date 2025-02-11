// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
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
#include <linux/random.h>
#include "apusys_secure.h"
#include "aputop_rpmsg.h"
#include "apu_top.h"
#include "mt6993_apupwr.h"
#include "mt6993_apupwr_prot.h"

#define LOCAL_DBG	(1)
#define RPC_ALIVE_DBG	(0)

#define FPGA_PRG_ENV		(0)
#define TEST_DUMMY_REG_RW	(1)

#define APU_AOC_INIT		(0)
#define APU_DREQ_INIT		(0)
#define APU_RPC_INIT		(0)
#define APU_RPCLITE_INIT	(0)
#define APU_PCU_INIT		(0)
#define APU_PLL_INIT		(0)
#define APU_ACC_INIT		(0)
#define APU_ARE_INIT		(0)
#define APU_BUCK_OFF		(0)
#define APU_ON_BY_CE_FW		(0)
#define CFG_APU_TOP_ARDCM_EN	(0)
#define CFG_APU_DEV_ARDCM_EN	(0) // always disabled in this platform !!

static struct apu_power *papw;
#if APU_ARE_INIT
static int vcore_are_idx __maybe_unused;
static int apu_ao_26m_are_idx;
static int apu_ao_are_idx __maybe_unused;
static int apu_pwr_are_idx __maybe_unused;
#endif
#if APU_PCU_INIT
static uint32_t vapu_pmic_slave_id = BUCK_VAPU_PMIC_ID;
static uint32_t vapu_en_set_offset = BUCK_VAPU_PMIC_REG_EN_SET_ADDR;
static uint32_t vapu_en_clr_offset = BUCK_VAPU_PMIC_REG_EN_CLR_ADDR;
static uint32_t vapu_en_shift = BUCK_VAPU_PMIC_REG_EN_SHIFT;
static uint32_t vapu_vosel_offset = BUCK_VAPU_PMIC_REG_VOSEL_ADDR;

static uint32_t vsram_apu_pmic_slave_id = LDO_VSRAM_PMIC_ID;
static uint32_t vsram_apu_en_shift = LDO_VSRAM_PMIC_REG_EN_SHIFT;
static uint32_t vsram_apu_en_lp_set_offset = LDO_VSRAM_PMIC_REG_EN_LP_SET_ADDR;
static uint32_t vsram_apu_en_lp_clr_offset = LDO_VSRAM_PMIC_REG_EN_LP_CLR_ADDR;
static uint32_t vsram_apu_en_lp_shift = LDO_VSRAM_PMIC_REG_EN_LP_SHIFT;
#endif

#if APU_ARE_INIT
#define DBG_ARE_ENTRY_SIZE	100
static ulong dbg_are_entry_reg[DBG_ARE_ENTRY_SIZE];
static ulong dbg_are_entry_data[DBG_ARE_ENTRY_SIZE];
static int dbg_are_entry_cnt;
#endif

#if (APU_ARE_INIT && (APU_PLL_INIT || APU_ACC_INIT || APU_RPCLITE_INIT))
static void _apu_w_are(int entry, ulong reg, ulong data)
{
	ulong are_entry_addr;
#if LOCAL_DBG
	ulong are_entry_addr_phy;

	dbg_are_entry_reg[dbg_are_entry_cnt] = reg;
	dbg_are_entry_data[dbg_are_entry_cnt] = data;
	dbg_are_entry_cnt++;
#endif

	// *((UINT32P)(APU_RCX_ARE_SRAM_BASE + 4 * entry_idx)) = (ostd_en << 31) | (reg >> 2)
	reg = (reg >> 2); // default not support OSTD in here

	/* (address of entry) = register */
	are_entry_addr = (ulong)papw->regs[apu_are] + 4 * ARE_ENTRY(entry);
#if LOCAL_DBG
	are_entry_addr_phy = (ulong)papw->phy_addr[apu_are] + 4 * ARE_ENTRY(entry);
#endif
	apu_writel(reg, (void __iomem *)are_entry_addr);
	apu_writel(data, (void __iomem *)(are_entry_addr + 4));
#if LOCAL_DBG
	pr_info("%s %d,%d 0x%08x : 0x%08x (0x%08x)\n",
		__func__, entry, ARE_ENTRY(entry),
		(uint32_t)are_entry_addr_phy, (uint32_t)reg, (uint32_t)(reg << 2));
	pr_info("%s %d,%d 0x%08x : 0x%08x\n",
		__func__, entry, ARE_ENTRY(entry),
		(uint32_t)(are_entry_addr_phy + 4), (uint32_t)data);
#endif
}
#endif

static void aputop_dump_reg(enum apupw_reg idx, uint32_t offset, uint32_t size)
{
	char buf[32];
	int ret = 0;

	// reg dump for RPC
	memset(buf, 0, sizeof(buf));
	ret = snprintf(buf, 32, "phys 0x%08x: ",
		       (u32)(papw->phy_addr[idx]) + offset);
	if (ret)
		print_hex_dump(KERN_ERR, buf, DUMP_PREFIX_OFFSET, 16, 4,
			papw->regs[idx] + offset, size, true);
}

#if APU_PLL_INIT
static void get_pll_pcw(uint32_t clk_rate, uint32_t *r1, uint32_t *r2)
{
	unsigned int fvco = clk_rate;
	unsigned int pcw_val;
	unsigned int postdiv_val = 1;
	unsigned int postdiv_reg = 0;

	while (fvco <= 1500) {
		postdiv_val = postdiv_val << 1;
		postdiv_reg = postdiv_reg + 1;
		fvco = fvco << 1;
	}

	pcw_val = (fvco * 1 << 14) / 26;
	if (postdiv_reg == 0) { //Fvco * 2 with post_divider = 2
		pcw_val = pcw_val * 2;
		postdiv_val = postdiv_val << 1;
		postdiv_reg = postdiv_reg + 1;
	} //Post divider is 1 is not available

	*r1 = postdiv_reg;
	*r2 = pcw_val;
}
#endif

#if APU_ACC_INIT
#if 0
static void __apu_engine_acc_on(void)
{
	// need to 1-1 in order mapping to these two array
	uint32_t eng_acc[] = {MDLA_ACC_BASE, MVPU_ACC_BASE};
	int eng_acc_arr_size = ARRAY_SIZE(eng_acc);
	ulong addr = 0;
	uint32_t val = 0;
	int ret = 0, acc_idx;

	for (acc_idx = 0; acc_idx < eng_acc_arr_size; acc_idx++) {
		addr = (ulong)papw->regs[apu_acc] + eng_acc[acc_idx] + APU_ACC_AUTO_CTRL_SET0;
		/* TINFO="[pllon/off]Step2: auto enable acc_idx clock" */
		apu_setl(1 << 8, (void __iomem *)addr);
		addr = (ulong)papw->regs[apu_acc] + eng_acc[acc_idx] + APU_ACC_AUTO_STATUS0;
		ret = readl_relaxed_poll_timeout_atomic((void *)addr, val,
							(val & (0x1UL << 6)), 50, 10000);
		if (ret)
			pr_info("%s %d wait hacc-%d on fail, ret = %d\n",
			       __func__, __LINE__, acc_idx, ret);
	}
}
#endif
#endif

#if APU_PLL_INIT
static void __apu_pll_init(void)
{
	// need to 1-1 in order mapping to these two array
	uint32_t pll_b[] = {MNOC_PLL_BASE, UP_PLL_BASE,
					MVPU_PLL_BASE, MDLA_PLL_BASE};
	int32_t pll_freq_out[] = {800, 800, 800, 800}; // MHz
	uint32_t pcw_val, posdiv_val;
	int pll_arr_size = ARRAY_SIZE(pll_b);
	int pll_i;

	// Step4. Initial PLL setting
	pr_info("PLL init %s %d --\n", __func__, __LINE__);

	for (pll_i = 0 ; pll_i < pll_arr_size ; pll_i++) {
		// PCW value always from hopping function: ofs 0x300
		_apu_w_are(apu_ao_26m_are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + pll_b[pll_i] + PLL1CPLL_FHCTL_HP_EN,
			   0x1 << 0);
		// Hopping function reset release: ofs 0x30C
		_apu_w_are(apu_ao_26m_are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + pll_b[pll_i] + PLL1CPLL_FHCTL_RST_CON,
			   0x1 << 0);
		// Hopping function clock enable: ofs 0x308
		_apu_w_are(apu_ao_26m_are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + pll_b[pll_i] + PLL1CPLL_FHCTL_CLK_CON,
			   0x1 << 0);
		// Hopping function enable: ofs 0x314
		_apu_w_are(apu_ao_26m_are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + pll_b[pll_i] + PLL1CPLL_FHCTL0_CFG,
			   (0x1 << 0) | (0x1 << 2));
		posdiv_val = 0;
		pcw_val = 0;
		get_pll_pcw(pll_freq_out[pll_i], &posdiv_val, &pcw_val);
		// POSTDIV: ofs 0x20C , [26:24] RG_PLL_POSDIV
		// 3'b000: /1 , 3'b001: /2 , 3'b010: /4
		// 3'b011: /8 , 3'b100: /16
		_apu_w_are(apu_ao_26m_are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + pll_b[pll_i] + PLL1C_PLL1_CON1,
			   ((0x1 << 31) | (posdiv_val << 24) | pcw_val));

		// PCW register: ofs 0x31C
		// [31] FHCTL0_PLL_TGL_ORG
		// [21:0] FHCTL0_PLL_ORG set to PCW value
		_apu_w_are(apu_ao_26m_are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + pll_b[pll_i] + PLL1CPLL_FHCTL0_DDS,
			   ((0x1 << 31) | pcw_val));
	}
}
#endif

#if APU_ACC_INIT
/* Cost 18 ARE entries, ARDCM(8) + ACC(4+6=10) */
static void __apu_acc_init(void)
{
	uint32_t top[] = {MNOC_ACC_BASE, UP_ACC_BASE};
	uint32_t eng[] = {MVPU_ACC_BASE, MDLA_ACC_BASE};
	int top_acc_arr_size = ARRAY_SIZE(top);
	int eng_acc_arr_size = ARRAY_SIZE(eng);
	int acc_idx = 0;
	int *are_idx = NULL;

	// Step6. Initial ACC setting (@ACC)
	for (acc_idx = 0 ; acc_idx < top_acc_arr_size ; acc_idx++) {
		if (acc_idx == 0) {
			/*
			 * although mnoc cfg in apupw are since s5 idle may turn mnoc off
			 * but we still can use ao_26m ARE to retore it in coldboot
			 * bcz s5 won't happen in coldboot
			 */
			//are_idx = &apu_pwr_are_idx;
			are_idx = &apu_ao_26m_are_idx;
		} else {
			are_idx = &apu_ao_26m_are_idx;
		}
#if CFG_APU_TOP_ARDCM_EN
		// DCM_EN/DBC_EN
		_apu_w_are((*are_idx)++,
			   (ulong)papw->phy_addr[apu_acc] + top[acc_idx]  + APU_ARDCM_CTRL1,
			   0x00001006);
		// APB_DCM_EN/APB_DBC_EN/APB_IDLE_FSEL_UPD_EN
		_apu_w_are((*are_idx)++,
			   (ulong)papw->phy_addr[apu_acc] + top[acc_idx]  + APU_ARDCM_CTRL0,
			   0x00000016);
		// IDLE_FSEL/DBC_CNT
		_apu_w_are((*are_idx)++,
			   (ulong)papw->phy_addr[apu_acc] + top[acc_idx]  + APU_ARDCM_CTRL1,
			   0x07F0F006);
		// APB_LOAD_TOG
		_apu_w_are((*are_idx)++,
			   (ulong)papw->phy_addr[apu_acc] + top[acc_idx]  + APU_ARDCM_CTRL0,
			   0x00000036);
#endif
		// CGEN_SOC
		_apu_w_are((*are_idx)++,
			   (ulong)papw->phy_addr[apu_acc] + top[acc_idx]  + APU_ACC_CONFG_CLR0,
			   0x00000004);
		// HW_CTRL_EN
		_apu_w_are((*are_idx)++,
			   (ulong)papw->phy_addr[apu_acc] + top[acc_idx] + APU_ACC_CONFG_SET0,
			   0x00008000);
	}

	for (acc_idx = 0 ; acc_idx < eng_acc_arr_size ; acc_idx++) {
#if CFG_APU_DEV_ARDCM_EN
		// DCM_EN/DBC_EN
		_apu_w_are(apu_ao_26m_are_idx++,
			   (ulong)papw->phy_addr[apu_acc] + eng[acc_idx]  + APU_ARDCM_CTRL1,
			   0x00001006);
		// APB_DCM_EN/APB_DBC_EN/APB_IDLE_FSEL_UPD_EN
		_apu_w_are(apu_ao_26m_are_idx++,
			   (ulong)papw->phy_addr[apu_acc] + eng[acc_idx]  + APU_ARDCM_CTRL0,
			   0x00000016);
		// IDLE_FSEL/DBC_CNT
		_apu_w_are(apu_ao_26m_are_idx++,
			   (ulong)papw->phy_addr[apu_acc] + eng[acc_idx]  + APU_ARDCM_CTRL1,
			   0x07F0F006);
		// APB_LOAD_TOG
		_apu_w_are(apu_ao_26m_are_idx++,
			   (ulong)papw->phy_addr[apu_acc] + eng[acc_idx]  + APU_ARDCM_CTRL0,
			   0x00000036);
#endif
		// CGEN_SOC
		_apu_w_are(apu_ao_26m_are_idx++,
			   (ulong)papw->phy_addr[apu_acc] + eng[acc_idx]  + APU_ACC_CONFG_CLR0,
			   0x00000004);
		// HW_CTRL_EN
		_apu_w_are(apu_ao_26m_are_idx++,
			   (ulong)papw->phy_addr[apu_acc] + eng[acc_idx] + APU_ACC_CONFG_SET0,
			   0x00008000);
		// CLK_REQ_SW_EN
		_apu_w_are(apu_ao_26m_are_idx++,
			   (ulong)papw->phy_addr[apu_acc] + eng[acc_idx] + APU_ACC_AUTO_CTRL_SET0,
			   0x00000100);
	}
}
#endif

#if APU_BUCK_OFF
static void buck_off_by_pcu(uint32_t ofs, uint32_t shift, uint32_t slv_id)
{
#if APU_PCU_INIT
	uint32_t cmd_op_w = 0x7;
	uint32_t pmif_id = 0x1;
	int retry = 10;

	pr_info("%s cmd1:0x%08x cmd2:0x%08x\n",
			__func__,
			(ofs << 16) | (0x1U << shift),
			(slv_id << 5) | (pmif_id << 3) | cmd_op_w);

	apu_setl(0x00000004,
			papw->regs[apu_pcu] + APU_PCUTOP_CTRL_SET);
	apu_writel((ofs << 16) | (0x1U << shift),
			papw->regs[apu_pcu] + APU_PCU_PMIC_TAR_BUF1);
	apu_writel((slv_id << 5) | (pmif_id << 3) | cmd_op_w,
			papw->regs[apu_pcu] + APU_PCU_PMIC_TAR_BUF2);
	apu_writel(0x00000001,
			papw->regs[apu_pcu] + APU_PCU_PMIC_CMD);

	while ((apu_readl(papw->regs[apu_pcu] + APU_PCU_PMIC_IRQ) & 0x1) == 0) {
		udelay(10);
		if (--retry < 0) {
			pr_info("%s wait APU_PCU_PMIC_IRQ timeout ! 0x%08x\n",
				__func__,
				apu_readl(papw->regs[apu_pcu] + APU_PCU_PMIC_IRQ));
		}
	}

	/* clear PCU irq status */
	apu_writel(0x1, papw->regs[apu_pcu] + APU_PCU_PMIC_IRQ);
#endif
}

static void trigger_buck_off(void)
{
#if APU_PCU_INIT
	// vapu buck off
	buck_off_by_pcu(vapu_en_clr_offset, vapu_en_shift, vapu_pmic_slave_id);
#endif
}

static void __apu_buck_off_cfg(void)
{
	pr_info("%s %d ++\n", __func__, __LINE__);

	trigger_buck_off();

	apu_setl(1 << 6, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);
	apu_setl(1 << 7, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);
	apu_clearl(1 << 6, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);
	apu_clearl(1 << 7, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);

	// Step12. After APUsys is finished, update the following register to 1,
	//     ARE will use this information to ensure the SRAM in ARE is
	//     trusted or not
	//     apusys_initial_done

	pr_info("%s %d --\n", __func__, __LINE__);
}
#endif

/*
 * low 32-bit data for PMIC control
 *	APU_PCU_PMIC_TAR_BUF1 (or APU_PCU_BUCK_ON_DAT0_L)
 *	[31:16] offset to update
 *	[15:00] data to update
 *
 * high 32-bit data for PMIC control
 *	APU_PCU_PMIC_TAR_BUF2 (or APU_PCU_BUCK_ON_DAT0_H)
 *	[2:0] cmd_op, read:0x3 , write:0x7
 *	[4:3]: pmifid, 0: SPI, 1: SPMI
 *	[7:5]: slvid
 */
#if APU_PCU_INIT
static void __apu_pcu_init(void)
{
	uint32_t cmd_op_w = 0x7;
	uint32_t pmif_id = 0x1;

	if (papw->env == FPGA)
		return;

	// auto buck enable
	apu_writel((0x1 << 3), papw->regs[apu_pcu] + APU_PCUTOP_CTRL_SET);

	/*
	 * Step1. enable cmd operation in auto buck on/off flow
	 * [0]: enable auto ON cmd0 (clear vsram ldo LP mode),
	 * [1]: enable auto ON cmd1 (set vapu voltage to 0.75v)
	 * [2]: enable auto ON cmd2 (turn vapu buck ON),
	 * [4]: enable auto OFF cmd0 (turn vapu buck OFF),
	 * [5]: enable auto OFF cmd1 (set vsram ldo LP mode),
	 */
	apu_writel(0x37,  papw->regs[apu_pcu] + APU_PCU_BUCK_STEP_SEL);

	// Step2. fill-in auto ON cmd0
	apu_writel((vsram_apu_en_lp_clr_offset << 16) | (0x1 << vsram_apu_en_lp_shift),
		papw->regs[apu_pcu] + APU_PCU_BUCK_ON_DAT0_L);
	apu_writel((vsram_apu_pmic_slave_id << 5) | (pmif_id << 3) | cmd_op_w,
		papw->regs[apu_pcu] + APU_PCU_BUCK_ON_DAT0_H);

	// Step3. fill-in auto ON cmd1
	apu_writel((vapu_vosel_offset << 16) | (750000 / 5000),
		papw->regs[apu_pcu] + APU_PCU_BUCK_ON_DAT1_L);
	apu_writel((vapu_pmic_slave_id << 5) | (pmif_id << 3) | cmd_op_w,
		papw->regs[apu_pcu] + APU_PCU_BUCK_ON_DAT1_H);

	// Step4. fill-in auto ON cmd2
	apu_writel((vapu_en_set_offset << 16) | (0x1U << vapu_en_shift),
		papw->regs[apu_pcu] + APU_PCU_BUCK_ON_DAT2_L);
	apu_writel((vapu_pmic_slave_id << 5) | (pmif_id << 3) | cmd_op_w,
		papw->regs[apu_pcu] + APU_PCU_BUCK_ON_DAT2_H);

	// Step5. fill-in auto OFF cmd0
	apu_writel((vapu_en_clr_offset << 16) | (0x1U << vapu_en_shift),
		papw->regs[apu_pcu] + APU_PCU_BUCK_OFF_DAT0_L);
	apu_writel((vapu_pmic_slave_id << 5) | (pmif_id << 3) | cmd_op_w,
		papw->regs[apu_pcu] + APU_PCU_BUCK_OFF_DAT0_H);

	// Step6. fill-in auto OFF cmd1
	apu_writel((vsram_apu_en_lp_set_offset << 16) |
		(0x1U << vsram_apu_en_lp_shift) | (0x1U << vsram_apu_en_shift),
		papw->regs[apu_pcu] + APU_PCU_BUCK_OFF_DAT1_L);
	apu_writel((vsram_apu_pmic_slave_id << 5) | (pmif_id << 3) | cmd_op_w,
		papw->regs[apu_pcu] + APU_PCU_BUCK_OFF_DAT1_H);

	// Step7. fill-in settle time for auto ON/OFF cmd
	apu_writel(0x1,  papw->regs[apu_pcu] + APU_PCU_BUCK_ON_SLE0);
	apu_writel(0xC8,  papw->regs[apu_pcu] + APU_PCU_BUCK_ON_SLE2);
	apu_writel(0x1,  papw->regs[apu_pcu] + APU_PCU_BUCK_OFF_SLE1);

	pr_info("PCU init %s %d %d--\n", __func__, __LINE__, pmif_id);
}
#endif

#if APU_RPCLITE_INIT
static void __apu_rpclite_init(void)
{
	uint32_t reg_data = 0x0;
	uint32_t reg_addr_phy = 0x0;

	uint32_t sleep_type_offset[] = {
		0x200, // SW_TYPE0, mvputop (no sram in mvpu core, only in top)
		0x20C, // SW_TYPE3, mdla0
		0x210, // SW_TYPE4, mdla1
		0x214, // SW_TYPE5, mdla2
		0x218, // SW_TYPE6, mdla3
	};
	int ofs_arr_size1 = sizeof(sleep_type_offset) / sizeof(uint32_t);
	int ofs_idx1;

#if APU_DREQ_INIT
	uint32_t mvpu_dreq_ctrl_offset[] = {
		0x1C4, // PWR_CON_S1
		0x1C8, // PWR_CON_S2
	};
	int ofs_arr_size2 = sizeof(mvpu_dreq_ctrl_offset) / sizeof(uint32_t);
	int ofs_idx2;

	uint32_t dla_dreq_ctrl_offset[] = {
		0x1CC, // PWR_CON_S3
		0x1D0, // PWR_CON_S4
		0x1D4, // PWR_CON_S5
		0x1D8, // PWR_CON_S6
	};
	int ofs_arr_size3 = sizeof(dla_dreq_ctrl_offset) / sizeof(uint32_t);
	int ofs_idx3;
#endif

	pr_info("%s %d ++\n", __func__, __LINE__);

	/* all rpclite config should be initialized through ARE */
	reg_data =
		BIT(4)		// VTOP_AFC_ENA, set 1 to enable pwr notify BD
		| BIT(15)	// FSM_CTRL_SEL, set 1 to enable mvpu_top auto on
		| 0x00ff0000	// PWR_RDY_IRQ_EN
		| 0xff000000;	// PWR_OFF_IRQ_EN

#if FPGA_PRG_ENV
	reg_addr_phy = 0x1905c000;
#else
	reg_addr_phy = (ulong)papw->phy_addr[apu_top_rpc_lite];
#endif

	_apu_w_are(apu_ao_26m_are_idx++,
		reg_addr_phy + APU_RPC_TOP_SEL,
		reg_data);

	// update for sleep_type
	for (ofs_idx1 = 0; (ofs_idx1 < ofs_arr_size1); ofs_idx1++) {
		// Memory setting
		_apu_w_are(apu_ao_26m_are_idx++,
			reg_addr_phy + sleep_type_offset[ofs_idx1],
			0x6);
	}

#if APU_DREQ_INIT
	// update for mvpu_dreq_ctrl
	for (ofs_idx2 = 0; (ofs_idx2 < ofs_arr_size2); ofs_idx2++) {
		// Memory setting
		_apu_w_are(apu_ao_26m_are_idx++,
				reg_addr_phy + mvpu_dreq_ctrl_offset[ofs_idx2],
				(0x471a | BIT(6) | BIT(7) | BIT(16)));
	}

	// update for dla_dreq_ctrl
	for (ofs_idx3 = 0; (ofs_idx3 < ofs_arr_size3); ofs_idx3++) {
		// Memory setting
		_apu_w_are(apu_ao_26m_are_idx++,
				reg_addr_phy + dla_dreq_ctrl_offset[ofs_idx3],
				(0x471a | BIT(6) | BIT(7) | BIT(16) | BIT(17)));
	}
#endif
	pr_info("%s %d ++\n", __func__, __LINE__);
}
#endif

#if APU_DREQ_INIT
static void __apu_dreq_init(void)
{
	pr_info("DREQ init %s %d ++\n", __func__, __LINE__);

	/* DREQ init */
	// auto_mode reset
	apu_setl(BIT(9), papw->regs[apu_rpc] + APU_RPC_HW_CON_1);

	pr_info("DREQ init %s %d --\n", __func__, __LINE__);
}
#endif

#if APU_RPC_INIT
static void __apu_rpc_init(void)
{
	pr_info("RPC init %s %d 0x%08x ++\n", __func__, __LINE__, apu_readl(papw->regs[apu_rpc] + APU_RPC_TOP_SEL));

	/* RPC config */
	apu_writel(apu_readl(papw->regs[apu_rpc] + APU_RPC_TOP_SEL)
		| BIT(0)  // DPSW_AOC_SEL (but should be DREQ)
#if APU_ON_BY_CE_FW
		| BIT(10) // CE_ENABLE, set 1 to enable CE to ctl APU wakeup or sleep flow
#endif
		| BIT(12) // APU_CLK_SEL, set 1 to enable auto clk on/off func from apupll path
		| BIT(14) // BUCK_SEL, set 1 to enable auto buck on/off func
		| BIT(15) // RCX_AO_ARE_SEL, set 1 to enable ARE func (in mt6993: APU_AO_26M)
		| BIT(27) // ACX_AO_ARE_SEL, set 1 to enable ARE func (in mt6993: APU_AO)
		| BIT(28) // SRAM_AOC_SEL, set 1 to enable SRAM AOC func
		| BIT(29) // PLL_AOC_SEL, set 1 to enable PLL AOC func
		| BIT(30) // DREQ_AM_SEL, set 1 to enable RPC control DREQ auto mode
		| BIT(31) // SOC_CLK_SEL, set 1 to enable auto clk on/off func from SOC path (LPOSC)
		, papw->regs[apu_rpc] + APU_RPC_TOP_SEL);

	apu_writel(apu_readl(papw->regs[apu_rpc] + APU_RPC_TOP_SEL_1)
		| BIT(15) // IPS_AOC_SEL, set 1 to enable IPS AOC func
		| BIT(20) // BUCK_PROT_SEL, set 1 to not masking
		, papw->regs[apu_rpc] + APU_RPC_TOP_SEL_1);

	// SRAM_AOC_LHENB, set 1 for LHENB assert
	apu_setl(BIT(4), papw->regs[apu_rpc] + APU_RPC_HW_CON);

	// AOVBUS_SLE_SEL set 1
	apu_setl(BIT(2), papw->regs[apu_rpc] + APU_RPC_TOP_SEL_2);

	// memory types (sleep or PD type), RPC: APU TCM set sleep type
	apu_writel(0x12, papw->regs[apu_rpc] + 0x0200);

	pr_info("RPC init %s %d --\n", __func__, __LINE__);
}
#endif

#if APU_ARE_INIT
static int __apu_are_init(struct device *dev)
{
//	uint32_t entry = 0;
	uint32_t apu_are_cfg = 0x0;

	pr_info("ARE init %s %d ++\n", __func__, __LINE__);

	/*  clean sram entry for safety */
//	for (entry = 0; entry < 7; entry++)
//		apu_writel(0x0, papw->regs[apu_are] + (4 * entry));

	apu_are_cfg =  BIT(23) // ARE_APU_PWR_EN
			| BIT(22) // ARE_APU_AO_EN
			| BIT(21) // ARE_APU_AO_26M_EN
			| BIT(20); // ARE_VCORE_EN

	apu_setl(apu_are_cfg, papw->regs[apu_are]); // apu_are_sram
	apu_setl(apu_are_cfg, papw->regs[apu_are] + 0x10000); // apu_are_hw

	apu_writel(0x0, papw->regs[apu_are] + (0x4 * 1)); // axi_sideband

	/*
	 * [13:0] start entry
	 * [29:16] entry num
	 */
	// vcore_are
	apu_writel(0x0, papw->regs[apu_are] + (0x4 * 4));

	// apu_ao_26m_are
	apu_ao_26m_are_idx *= 2; // idx * {reg,data} pair
	apu_writel(ARE_ENTRY_BEGIN | (apu_ao_26m_are_idx << 16),
			papw->regs[apu_are] + (0x4 * 5));

	// apu_ao_are
	apu_writel(0x0, papw->regs[apu_are] + (0x4 * 6));

	// apu_pwr_are
	apu_writel(0x0, papw->regs[apu_are] + (0x4 * 7));

	/* Disable sMMU by set HW flag 10 */
//	apu_setl(0x1<<10, papw->regs[apu_are] + 0x105D4);

	pr_info("ARE init %s %d --\n", __func__, __LINE__);

	return 0;
}
#endif

#if APU_ACC_INIT
static void mtk_clk_acc_get_rate(void)
{
	int32_t output = 0, i = 0, j = 0;
	uint32_t tempValue = 0;
	bool timeout = false;
	//uint32_t phy_confg_set;
	//uint32_t phy_fm_confg_set, phy_fm_confg_clr, phy_fm_sel, phy_fm_cnt;
	//ulong confg_set;
	ulong fm_confg_set, fm_confg_clr, fm_sel, fm_cnt;
	uint32_t loop_ref = 0;  // 0 for Max freq  ~ 1074MHz
	int32_t retry = 30;

	uint32_t acc_base_arr[] = {MNOC_ACC_BASE, UP_ACC_BASE};
	uint32_t acc_offset_arr[] = {
				APU_ACC_CONFG_SET0, APU_ACC_FM_SEL, APU_ACC_FM_CONFG_SET,
				APU_ACC_FM_CONFG_CLR, APU_ACC_FM_CNT};

	for (j = 0 ; j < (sizeof(acc_base_arr) / sizeof(uint32_t)) ; j++) {
		//confg_set = (ulong)papw->regs[apu_acc] + acc_base_arr[j] + acc_offset_arr[0];
		fm_sel = (ulong)papw->regs[apu_acc] + acc_base_arr[j] + acc_offset_arr[1];
		fm_confg_set = (ulong)papw->regs[apu_acc] + acc_base_arr[j] + acc_offset_arr[2];
		fm_confg_clr = (ulong)papw->regs[apu_acc] + acc_base_arr[j] + acc_offset_arr[3];
		fm_cnt = (ulong)papw->regs[apu_acc] + acc_base_arr[j] + acc_offset_arr[4];

		/* reset */
		apu_writel(0x0, (void __iomem *)fm_sel);
		apu_writel(apu_readl((void __iomem *)fm_sel), (void __iomem *)fm_sel);
		apu_writel(apu_readl((void __iomem *)fm_sel) | (loop_ref << 16),
			(void __iomem *)fm_sel);
		apu_writel(BIT(0), (void __iomem *)fm_confg_set);
		apu_writel(BIT(1), (void __iomem *)fm_confg_set);

		/* wait frequency meter finish */
		while (!(apu_readl((void __iomem *)fm_confg_set) & BIT(4))) {
			udelay(10);
			i++;
			if (i > retry) {
				timeout = true;
				pr_info("%s acc error, fm_sel = 0x%08x, fm_confg_set = 0x%08x\n",
					__func__,
					apu_readl((void __iomem *)fm_sel),
					apu_readl((void __iomem *)fm_confg_set));
				break;
			}
		}

		if ((!timeout) &&
			!(apu_readl((void __iomem *)fm_confg_set) & BIT(5))) {
			tempValue = apu_readl((void __iomem *)fm_cnt);
			tempValue = tempValue & 0xFFFFF;
			output = tempValue * 16384 / ((loop_ref + 1) * 1000);  //KHz
		} else {
			output = 0;
		}

		pr_info("%s: acc%d clk rate : %d\n", __func__, j, output);

		apu_writel(BIT(4), (void __iomem *)fm_confg_clr);
		apu_writel(BIT(1), (void __iomem *)fm_confg_clr);
		apu_writel(BIT(0), (void __iomem *)fm_confg_clr);
	}
}
#endif

static void wakeup_rpc_by_mbox(void)
{
	void *mbox11 = ioremap(0x4c2b0000, 0x100);

	apu_writel(0x1, mbox11 + 0x80);
	pr_info("%s readback mbox11 data: 0x%08x\n",
			__func__, apu_readl(mbox11 + 0x80));
#if APMCU_REQ_RPC_SLEEP
	udelay(200);

	apu_writel(0x0, mbox11 + 0x80);
	pr_info("%s readback mbox11 data: 0x%08x\n",
			__func__, apu_readl(mbox11 + 0x80));
#endif
	iounmap(mbox11);
}

static int __apu_wake_rpc_rcx(struct device *dev)
{
	int ret = 0, val = 0;

	dev_info(dev, "%s Before wakeup RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			readl(papw->regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

	/* TINFO="Enable AFC enable" */
	apu_setl(0x1 << 16, papw->regs[apu_rpc] + APU_RPC_TOP_SEL_1);

#if 0
	/* wakeup RPC by rpc inner register */
	apu_writel(0x00000100, papw->regs[apu_rpc] + APU_RPC_TOP_CON);
#else
	/* wakeup RPC by mbox */
	wakeup_rpc_by_mbox();
#endif

	/* clear wakeup signal */
	apu_setl((BIT(10) | BIT(14) | BIT(18)), papw->regs[apu_rpc] + APU_RPC_TOP_CON);

	ret = readl_relaxed_poll_timeout_atomic(
			(papw->regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL), 50, 10000);
	if (ret) {
		pr_info("%s polling RPC RDY timeout, val = 0x%x, ret %d\n", __func__, val, ret);
		goto out;
	}

	dev_info(dev, "%s After wakeup RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			readl(papw->regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

#if 0
	/* polling FSM @RPC-lite to ensure RPC is in on/off stage */
	ret |= readl_relaxed_poll_timeout_atomic(
			(papw->regs[apu_rpc] + APU_RPC_STATUS_1),
			val, (val & (0x1 << 13)), 50, 10000);
	if (ret) {
		pr_info("%s polling ARE FSM timeout, ret %d\n", __func__, ret);
		goto out;
	}

	ret |= readl_relaxed_poll_timeout_atomic(
			(papw->regs[apu_rpc] + APU_RPC_PWR_ACK),
			val, (val & 0x3UL), 50, 10000);
	if (ret) {
		pr_info("%s power chain(vcore[1] rcx[0]) fail, ret %d\n", __func__, ret);
		goto out;
	}
#endif

	/* clear vcore/rcx cgs */
	apu_writel(0xFFFFFFFF, papw->regs[apu_vcore] + APUSYS_VCORE_CG_CLR);
	apu_writel(0xFFFFFFFF, papw->regs[apu_intc_cfg] + 0x8);
	apu_writel(0xFFFFFFFF, papw->regs[apu_ctrlsys_cfg] + 0x8);
out:
	return ret;
}

#if TEST_DUMMY_REG_RW
static int test_apu_dummy_reg_access(int byass_engine)
{
#if APUPW_DUMP_FROM_APMCU
	int arr_idx;
	uint32_t reg_data_before = 0x0;
	uint32_t reg_data_after = 0x0;
	uint32_t test_code = 0xdeadbeaf;
	int engine_begin_arr_idx = 6;
	uint32_t dev_test_reg_arr[] = {
		/*--- top ---*/
		0x19419334,
		0x1925029c,
		0x19090500,
		/*--- apu tcm ---*/
		0x19411000,
		0x19413000,
		0x19415000,
		/*--- engines ---*/
		0x1912b314,
		0x19122818,
		0x19125818,
		0x192000f0,
		0x192100f0,
		0x192200f0,
		0x192300f0,
		0x19243100,
	};
	int reg_arr_size = sizeof(dev_test_reg_arr) / sizeof(uint32_t);
	void *reg_addr;

	for (arr_idx = 0 ; arr_idx < reg_arr_size ; arr_idx++) {

		if (byass_engine && arr_idx == engine_begin_arr_idx)
			break;

		reg_addr = ioremap(dev_test_reg_arr[arr_idx], 0x8);
		reg_data_before = apu_readl(reg_addr);
		apu_writel(test_code, reg_addr);
		reg_data_after = apu_readl(reg_addr);
		apu_writel(reg_data_before, reg_addr);

		if (reg_data_after != test_code) {
			pr_info("%s FAIL arr_idx:%d spare_reg: 0x%08x , data: before = 0x%08x , after = 0x%08x\n",
					__func__, arr_idx,
					dev_test_reg_arr[arr_idx],
					reg_data_before, reg_data_after);
		} else {
			pr_info("%s PASS arr_idx:%d spare_reg: 0x%08x , data: before = 0x%08x , after = 0x%08x\n",
					__func__, arr_idx,
					dev_test_reg_arr[arr_idx],
					reg_data_before, reg_data_after);
		}
		iounmap(reg_addr);
	}
#endif
	return 0;
}
#endif

int apu_rpclite_pwr_on(int eng_idx)
{
	int ret = 0;
	uint32_t val = 0;
	uint32_t pwr_rdy_idx = 0;
	uint32_t cg_clr_bitmask = 0x0;
	void __iomem *cg_clr_base_addr = 0x0;

	/*
	 * engine_id_remap
	 * [15:0] for mvpu top(hw voter)
	 * [16] mvpu0
	 * [17] mvpu1
	 * [21:18]mdla3~mdla0
	 * [22]tdla
	 */
	switch(eng_idx){
	case 0 :
		// MVPU TOP
		eng_idx = 0;
		pwr_rdy_idx = 0;
		cg_clr_bitmask = 0x0000ffff;
		cg_clr_base_addr = papw->regs[mvpu_top_config];
		break;
	case 1 :
		// MVPU0
		eng_idx = 16;
		pwr_rdy_idx = 1;
		cg_clr_bitmask = 0x00ffffff;
		cg_clr_base_addr = papw->regs[mvpu_top_config];
		break;
	case 2 :
		// MVPU1
		eng_idx = 17;
		pwr_rdy_idx = 2;
		cg_clr_bitmask = 0xff00ffff;
		cg_clr_base_addr = papw->regs[mvpu_top_config];
		break;
	case 3 :
		// MDLA0
		eng_idx = 18;
		pwr_rdy_idx = 3;
		cg_clr_bitmask = 0xffffffff;
		cg_clr_base_addr = papw->regs[apu_dla_0_config];
		break;
	case 4 :
		// MDLA1
		eng_idx = 19;
		pwr_rdy_idx = 4;
		cg_clr_bitmask = 0xffffffff;
		cg_clr_base_addr = papw->regs[apu_dla_1_config];
		break;
	case 5 :
		// MDLA2
		eng_idx = 20;
		pwr_rdy_idx = 5;
		cg_clr_bitmask = 0xffffffff;
		cg_clr_base_addr = papw->regs[apu_dla_2_config];
		break;
	case 6 :
		// MDLA3
		eng_idx = 21;
		pwr_rdy_idx = 6;
		cg_clr_bitmask = 0xffffffff;
		cg_clr_base_addr = papw->regs[apu_dla_3_config];
		break;
	case 7 :
		// TDLA
		eng_idx = 22;
		pwr_rdy_idx = 7;
		cg_clr_bitmask = 0xffffffff;
		cg_clr_base_addr = papw->regs[tinydla_top_config];
		break;
	default :
		pr_info("%s Invalid eng_idx(%d)\n", __func__, eng_idx);
		break;
	}

	apu_writel((BIT(eng_idx)),
			papw->regs[apu_top_rpc_lite] + APU_RPC_USR_VOTE_SET);

	/* wait eng_pwr_rdy == 1, eng_idx
	 * reg_rpctop_con = {
	 *	16'b0, rpctop_irq[15],
	 *	 2'b0, acx_fstate[12:8],
	 *	 3'b0, eng_pwr_rdy[4:1],
	 *	 1'b0, acx_cen_rdy[0] };
	 */
	ret = readl_relaxed_poll_timeout_atomic(
			(papw->regs[apu_top_rpc_lite] + APU_RPC_TOP_CON),
			val, (val & BIT(pwr_rdy_idx)), 50, 10000);

	pr_info("%s eng_idx : %d, top_con : 0x%08x, pwr_rdy : 0x%08x\n",
			__func__, eng_idx,
			apu_readl(papw->regs[apu_top_rpc_lite] + APU_RPC_TOP_CON),
			apu_readl(papw->regs[apu_top_rpc_lite] + APU_RPC_INTF_PWR_RDY));

	if (ret) {
		pr_info("%s polling RPC RDY timeout eng_idx : %d, ret %d\n",
				__func__, eng_idx, ret);
		return -1;
	}
#if 1
	/* Clear APU_ENIGNE_CG */
	apu_writel((apu_readl(cg_clr_base_addr + CG_CLR_OFFSET)
			| cg_clr_bitmask), cg_clr_base_addr + CG_CLR_OFFSET);
#endif
	pr_info("%s power on success for eng_idx : %d\n", __func__, eng_idx);
	return 0;
}

#if APU_AOC_INIT
static void __apu_aoc_init(void)
{
	pr_info("AOC init %s %d ++\n", __func__, __LINE__);

	/* 1. Manually disable Buck els enable @SOC, vapu_ext_buck_iso */
	if (papw->env != FPGA) {
		//apu_setl((0x1 << 4), papw->regs[sys_spm] + 0xF6C);
		apu_clearl((0x1 << 1), papw->regs[sys_spm] + 0x414);
	}

	/*
	 * 2. Vsram AO clock enable
	 */
	apu_writel(0x00000001, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_CONFIG);
	udelay(1);

	/*
	 * 3. keep AOC control from vsram ao register
	 */
	// Sram AOCISO reset to 0
	apu_setl(BIT(9), papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	apu_clearl(BIT(9), papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);

	// Sram AOCLHENB set to 1
	apu_setl(BIT(10), papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	apu_clearl(BIT(10), papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);

	// Sram AOCISO set to 1
	apu_setl(BIT(9), papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_CLR);
	apu_clearl(BIT(9), papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_CLR);
	udelay(1);

// bypass this flow since we donot need to access rpclite before buck ON
#if 0
	// 4. Roll back to APU Buck on stage
	//  The following setting need to in order
	//  and wait 1uS before setup next control signal
	// APU_BUCK_ELS_EN
	apu_writel(BIT(11), papw->regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(1);
	// APU_BUCK_RST_B
	apu_writel(BIT(12), papw->regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(1);
	// APU_BUCK_PROT_REQ
	apu_writel(BIT(15), papw->regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(1);
	// SRAM_AOC_ISO
	apu_writel(BIT(7), papw->regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(1);
	/* PLL_AOC_ISO_EN */
	apu_writel(BIT(9), papw->regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(1);
#endif
	pr_info("AOC init %s %d --\n", __func__, __LINE__);
}
#endif

static int init_hw_setting(struct device *dev)
{
#if APU_AOC_INIT
	__apu_aoc_init();
#endif

#if APU_DREQ_INIT
	__apu_dreq_init();
#endif

#if APU_RPC_INIT
	__apu_rpc_init();
#endif
#if APU_RPCLITE_INIT
	__apu_rpclite_init();
#endif

#if APU_PCU_INIT
	__apu_pcu_init();
#endif

	if (papw->env != FPGA) {
#if APU_PLL_INIT
		__apu_pll_init();
#endif
#if APU_ACC_INIT
		__apu_acc_init();
#endif
	}

// ARE should be initialized in the last since dynamic calculate entry num
#if APU_ARE_INIT
	__apu_are_init(dev);
#endif

#if APU_BUCK_OFF
	__apu_buck_off_cfg();
#endif
	return 0;
}

void mt6993_power_init(struct platform_device *pdev, struct apu_power *g_papw)
{
	papw = g_papw;
	init_hw_setting(&pdev->dev);
}

#if APU_ARE_INIT
static void dump_dbg_are(void)
{
#if APUPW_DUMP_FROM_APMCU
	int idx = 0;
	void *ptr = 0x0;
	uint32_t reg;
	uint32_t data;
	uint32_t golden_data;

	for (idx = 0 ; idx < DBG_ARE_ENTRY_SIZE ; idx++) {
		golden_data = dbg_are_entry_data[idx];
		reg = dbg_are_entry_reg[idx];
		if (reg == 0x0)
			continue;
		ptr = ioremap(reg, 0x4);
		data = apu_readl(ptr);
		iounmap(ptr);
		pr_info("%s %d/0x%08x/0x%08x/0x%08x\n",
				__func__, idx, reg, data, golden_data);
	}
#endif
}
#endif

static void debug_dump_list(void)
{
	// BD brisket delegate dbg (0x19058094 ~ 0x190580B0)
//	aputop_dump_reg(apu_briske_del, 0x90, 0x30);

	// debug rpc, buck iso, sram iso, dreq iso
	aputop_dump_reg(apu_rpc, 0x0, 0x40);
	aputop_dump_reg(apu_ao_ctl, 0xc0, 0x10);
	aputop_dump_reg(apu_pcu, 0x360, 0x30);

	// debug rpclite and BD (dump after rcx on)
	aputop_dump_reg(apu_top_rpc_lite, 0x0, 0x20);

	// debug ARE related reg dump
	aputop_dump_reg(apu_are, 0x0, 0x20); // apu are sram cfg
	aputop_dump_reg(apu_are, 0x10a0, 0x10); // apu are sram (power ce dbg)
	aputop_dump_reg(apu_are, 0x10b0, 0x30); // apu are sram (are entry part)
	aputop_dump_reg(apu_are, 0x10000, 0x10); // apu are hw
	aputop_dump_reg(apu_are, 0x10030, 0x10); // apu are hw
}

int mt6993_all_on(struct platform_device *pdev)
{
	int dev_idx = 0;

	pr_info("%s dbg 1\n", __func__);

	/* wake up RCX */
	if (__apu_wake_rpc_rcx(&pdev->dev)) {
		debug_dump_list();
		return -EIO;
	}

#if APU_ACC_INIT
	//__apu_engine_acc_on(); //acc on through 26M ARE
	mtk_clk_acc_get_rate();
#endif
	for (dev_idx = 0 ; dev_idx < DEVICE_NUM ; dev_idx++) {
		if (papw->env == AO_ALL) {
			apu_rpclite_pwr_on(dev_idx);
		} else if (dev_bitmap & BIT(dev_idx)) {
			apu_rpclite_pwr_on(dev_idx);
		} else {
			pr_info("%s bypass dev_idx : %d\n", __func__, dev_idx);
		}
	}

#if TEST_DUMMY_REG_RW
	if (papw->env == AO_ALL)
		test_apu_dummy_reg_access(0);
	else if (papw->env == AO_RCX)
		test_apu_dummy_reg_access(1);
#endif
#if APU_ARE_INIT
	dump_dbg_are();
#endif
	return 0;
}

void mt6993_all_off(struct platform_device *pdev)
{
}
