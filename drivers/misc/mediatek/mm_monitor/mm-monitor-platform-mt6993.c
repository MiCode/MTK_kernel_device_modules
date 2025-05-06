
// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include "mtk-mm-monitor-controller.h"
#include <dt-bindings/memory/mt6993-smi-pd.h>

#define MM_MONITOR_SUBSYS_MAX	36
#define MM_MONITOR_AXI_MAX	32
#define ELA_HW_ID_INIT		0x63

/* fake engine related settings */
#define SSIDV_TBL_PA		(0x30010000)
#define MMINFRA2_SMI_DISP_RSIU		(0x30b30000)
#define MMINFRA2_SMI_MDP_RSIU		(0x30B31000)
#define MMINFRA2_SMI_DISP_RSIU1		(0x30b32000)
#define MMINFRA2_SMI_MDP_RSIU1		(0x30b33000)
#define MMINFRA_TX27_RSIU		(0x3002f000)
#define MMINFRA_TX28_RSIU		(0x3002e000)
#define MMINFRA_TX31_RSIU		(0x3002d000)
#define MDP_COMMON_BASE_PA		(0x30021000)
#define DISP_COMMON_BASE_PA		(0x30020000)
#define MMSRAM_COMMON_BASE_PA		(0x30022000)
#define MMINFRA2_NOC_DCM_0		(0x30b39000)
#define MMINFRA2_NOC_DCM_1		(0x30b38000)
#define MMINFRA_NOC_DCM_0		(0x30025000)
#define MMINFRA_NOC_DCM_1		(0x30026000)
#define MMINFRA_NOC_DCM_2		(0x30027000)

/* CTI related settings */
#define MM1_ATB_FUNNEL_PA		(0x30a2f000)

/* EMI monitor settings */
#define EMI_INFRA_PDN_SEC		(0x11027000)
#define NEMI_AO_SEC		(0x11044000)
#define SEMI_AO_SEC		(0x11084000)

/* MUX ID */
#define CKSYS_MM		0x10040000
#define CKSYS2_CLK_CFG_0	0x0010
#define CKSYS2_CLK_CFG_2	0x0030
#define CKSYS2_CLK_CFG_3	0x0040

static struct mmmc_mux mux[MUX_NUM] = {
	[MMINFRA_MUX_ID]  = {CKSYS_MM + CKSYS2_CLK_CFG_0,
		16, {1, 4,  7, 10, 13, 14, 15}, {130, 312, 364, 458, 624, 728, 916}, },
	[DISP_MUX_ID]	  = {CKSYS_MM + CKSYS2_CLK_CFG_2,
		 8, {1, 1,  3,  5,  6,  7,  8}, {273, 273, 364, 458, 546, 624, 624}, },
	[CAM_MAIN_MUX_ID] = {CKSYS_MM + CKSYS2_CLK_CFG_3,
		 8, {4, 7, 10, 15, 15, 15, 15}, {260, 343, 416, 564, 660, 660, 660}, },
};

u16 get_freq_from_mux_id(enum MUX_ID id)
{
	uint8_t bit;
	uint16_t freq;
	uint32_t read_value;
	int i;

	if (id >= MUX_NUM) {
		MM_MONITOR_ERR("invalid id:%u MUX_NUM:%u", id, MUX_NUM);
		return 0;
	}

	if (!mux[id].va)
		mux[id].va = ioremap(mux[id].pa, 0x1000);

	read_value = readl(mux[id].va);
	bit = (read_value >> mux[id].shift) & 0xf;
	MM_MONITOR_INFO("mux pa:%#x, va:%#lx, value:%#x, shift:%d, bit:%x",
		mux[id].pa, (unsigned long) mux[id].va, read_value, mux[id].shift, bit);

	for (i = 0; i < MAX_LEVEL; i++)
		if (mux[id].bit[i] == bit)
			break;
	freq = i < MAX_LEVEL ? mux[id].freq[i] : 0;
	MM_MONITOR_INFO("mux_id:%d, i:%d, freq:%u", id, i, freq);

	return freq;
}
EXPORT_SYMBOL(get_freq_from_mux_id);

u32 get_mmmc_subsys_max(void)
{
	return MM_MONITOR_SUBSYS_MAX;
}
EXPORT_SYMBOL(get_mmmc_subsys_max);

u32 get_mminfra_pd(void)
{
	return MT6993_SMI_PD_MMINFRA1;
}
EXPORT_SYMBOL(get_mminfra_pd);

u32 is_valid_offset_value(u32 hw, u32 id, u32 offset, u32 value)
{
	switch (hw) {
	case MM_AXI:
		if (id >= MM_MONITOR_AXI_MAX) {
			MM_MONITOR_ERR("unknown HW:%d id:%d", hw, id);
			return -EINVAL;
		}
		if (offset > 0x10c)
			goto invalid_value;
		break;
	case MM_ELA:
		if (offset > 0x908)
			goto invalid_value;
		break;
	case MM_CTI:
		if (offset > 0xffc)
			goto invalid_value;
		break;
	case MM_MMINFRA:
		break;
	default:
		MM_MONITOR_ERR("unknown HW:%d", hw);
		return -EINVAL;
	}

	return 0;

invalid_value:
	MM_MONITOR_ERR("invalid value HW:%d id:%d offset:%#x value:%#x",
		hw, id, offset, value);
	return -EINVAL;
}
EXPORT_SYMBOL(is_valid_offset_value);

void enable_mminfra_funnel(void)
{
	void __iomem *mm1_atb_funnel_base;

	/* enable funnel port */
	mm1_atb_funnel_base = ioremap(MM1_ATB_FUNNEL_PA, 0x1000);
	writel(0xc5acce55, mm1_atb_funnel_base + 0xfb0);
	writel(0xff, mm1_atb_funnel_base + 0x000);
	MM_MONITOR_DBG("enable funnel port %#x:%#x %#x:%#x",
		0xfb0, readl(mm1_atb_funnel_base + 0xfb0), 0x00,
		readl(mm1_atb_funnel_base + 0x000));
}
EXPORT_SYMBOL(enable_mminfra_funnel);

void mminfra_fake_engine_bus_settings(void)
{
	void __iomem *DISP_COMMON_BASE, *MDP_COMMON_BASE, *MMSRAM_COMMON_BASE;
#ifdef MMMC_SUPPORT_FPGA
	void __iomem *SSIDV_TBL_PA_BASE;
#endif
	void __iomem *MMINFRA2_SMI_DISP_RSIU_BASE, *MMINFRA2_SMI_MDP_RSIU_BASE;
	void __iomem *MMINFRA2_SMI_DISP_RSIU1_BASE, *MMINFRA2_SMI_MDP_RSIU1_BASE;
	void __iomem *MMINFRA_TX27_RSIU_DCM_CON, *MMINFRA_TX28_RSIU_DCM_CON, *MMINFRA_TX31_RSIU_DCM_CON;
	void __iomem *MMINFRA2_NOC_DCM_0_BASE, *MMINFRA2_NOC_DCM_1_BASE, *MMINFRA_NOC_DCM_0_BASE;
	void __iomem *MMINFRA_NOC_DCM_1_BASE, *MMINFRA_NOC_DCM_2_BASE;

#ifdef MMMC_SUPPORT_FPGA
	//MMINFRA SSIDV TABLE
	SSIDV_TBL_PA_BASE = ioremap(SSIDV_TBL_PA, 0x1000);
	writel(0xffff, SSIDV_TBL_PA_BASE + 0x00c);
	writel(0xffff, SSIDV_TBL_PA_BASE + 0x10c);
	writel(0xffff, SSIDV_TBL_PA_BASE + 0x20c);
	writel(0xffff, SSIDV_TBL_PA_BASE + 0x30c);
	writel(0xffff, SSIDV_TBL_PA_BASE + 0x40c);
	writel(0xffff, SSIDV_TBL_PA_BASE + 0x50c);
	writel(0xffff, SSIDV_TBL_PA_BASE + 0x60c);
	writel(0xffff, SSIDV_TBL_PA_BASE + 0x70c);
	writel(0xffff, SSIDV_TBL_PA_BASE + 0x80c);
	writel(0xffff, SSIDV_TBL_PA_BASE + 0x90c);
	writel(0xffff, SSIDV_TBL_PA_BASE + 0xa0c);
	writel(0xffff, SSIDV_TBL_PA_BASE + 0xb0c);
#endif

	// CHAN SEL
	MDP_COMMON_BASE = ioremap(MDP_COMMON_BASE_PA, 0x1000);
	writel(0x5101, MDP_COMMON_BASE + 0x220);
	DISP_COMMON_BASE = ioremap(DISP_COMMON_BASE_PA, 0x1000);
	writel(0x4444, DISP_COMMON_BASE + 0x220);
	MMSRAM_COMMON_BASE = ioremap(MMSRAM_COMMON_BASE_PA, 0x1000);
	writel(0x4444, MMSRAM_COMMON_BASE + 0x220);
	MM_MONITOR_DBG("chan sel mdp:%#x disp:%#x",
		readl(MDP_COMMON_BASE + 0x220),
		readl(DISP_COMMON_BASE + 0x220));
	// RSI DCM
	MMINFRA2_SMI_DISP_RSIU_BASE = ioremap(MMINFRA2_SMI_DISP_RSIU, 0x1000);
	writel((0x1<<3) | (0x1<<2) | (0x1<<1) | (0x1<<0), MMINFRA2_SMI_DISP_RSIU_BASE + 0x4);
	MMINFRA2_SMI_MDP_RSIU_BASE = ioremap(MMINFRA2_SMI_MDP_RSIU, 0x1000);
	writel((0x1<<3) | (0x1<<2) | (0x1<<1) | (0x1<<0), MMINFRA2_SMI_MDP_RSIU_BASE + 0x4);
	MMINFRA2_SMI_DISP_RSIU1_BASE = ioremap(MMINFRA2_SMI_DISP_RSIU1, 0x1000);
	writel((0x1<<3) | (0x1<<2) | (0x1<<1) | (0x1<<0), MMINFRA2_SMI_DISP_RSIU1_BASE + 0x4);
	MMINFRA2_SMI_MDP_RSIU1_BASE = ioremap(MMINFRA2_SMI_MDP_RSIU1, 0x1000);
	writel((0x1<<3) | (0x1<<2) | (0x1<<1) | (0x1<<0), MMINFRA2_SMI_MDP_RSIU1_BASE + 0x4);
	MMINFRA_TX27_RSIU_DCM_CON = ioremap(MMINFRA_TX27_RSIU, 0x1000);
	writel((0x1<<3) | (0x1<<2) | (0x1<<1) | (0x1<<0), MMINFRA_TX27_RSIU_DCM_CON + 0x4);
	MMINFRA_TX28_RSIU_DCM_CON = ioremap(MMINFRA_TX28_RSIU, 0x1000);
	writel((0x1<<3) | (0x1<<2) | (0x1<<1) | (0x1<<0), MMINFRA_TX28_RSIU_DCM_CON + 0x4);
	MMINFRA_TX31_RSIU_DCM_CON = ioremap(MMINFRA_TX31_RSIU, 0x1000);
	writel((0x1<<3) | (0x1<<2) | (0x1<<1) | (0x1<<0), MMINFRA_TX31_RSIU_DCM_CON + 0x4);
	// MMINFRA NOC DCM
	MMINFRA2_NOC_DCM_0_BASE = ioremap(MMINFRA2_NOC_DCM_0, 0x1000);
	writel((0x1<<3), MMINFRA2_NOC_DCM_0_BASE + 0x350);
	MMINFRA2_NOC_DCM_1_BASE = ioremap(MMINFRA2_NOC_DCM_1, 0x1000);
	writel((0x1<<3), MMINFRA2_NOC_DCM_1_BASE + 0x350);
	MMINFRA_NOC_DCM_0_BASE = ioremap(MMINFRA_NOC_DCM_0, 0x1000);
	writel((0x1<<3), MMINFRA_NOC_DCM_0_BASE + 0x350);
	MMINFRA_NOC_DCM_1_BASE = ioremap(MMINFRA_NOC_DCM_1, 0x1000);
	writel((0x1<<3), MMINFRA_NOC_DCM_1_BASE + 0x350);
	MMINFRA_NOC_DCM_2_BASE = ioremap(MMINFRA_NOC_DCM_2, 0x1000);
	writel((0x1<<3), MMINFRA_NOC_DCM_2_BASE + 0x350);
}
EXPORT_SYMBOL(mminfra_fake_engine_bus_settings);

void emi_moniter_settings(void)
{
	void __iomem *emi_infra_pdn_sec, *nemi_ao_sec, *semi_ao_sec;

	emi_infra_pdn_sec = ioremap(EMI_INFRA_PDN_SEC, 0x1000);
	nemi_ao_sec = ioremap(NEMI_AO_SEC, 0x1000);
	semi_ao_sec = ioremap(SEMI_AO_SEC, 0x1000);

	/* emi infra bypass */
	writel(0x1FFFFF, emi_infra_pdn_sec + 0xbd4);
	writel(0x6, semi_ao_sec + 0x0fc);
	writel(0x6, nemi_ao_sec + 0x0fc);
}
EXPORT_SYMBOL(emi_moniter_settings);

u32 power_domains[] = {
	MT6993_SMI_PD_VDISP_PERI,
	MT6993_SMI_PD_ISP_VCORE,
	MT6993_SMI_PD_CAM_VCORE,
	MT6993_SMI_PD_CAM_RAWA,
	MT6993_SMI_PD_CAM_RAWB,
	MT6993_SMI_PD_CAM_RAWC,
	MT6993_SMI_PD_VEN_MDP,
};
u32 get_power_domains(int index)
{
	return power_domains[index];
}
EXPORT_SYMBOL(get_power_domains);

static int __init mm_monitor_platform_init(void)
{
	MM_MONITOR_DBG("enter");

	return 0;
}
module_init(mm_monitor_platform_init);
MODULE_LICENSE("GPL");
