// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
#include <linux/io.h>
//#include <mt-plat/mtk_io.h>
//#include <mt-plat/sync_write.h>
//#include <mt-plat/mtk_secure_api.h>

#include <mt6765_dcm_internal.h>
#include "mtk_dcm.h"

unsigned int init_dcm_type = ALL_DCM_TYPE;


#if defined(__KERNEL__) && IS_ENABLED(CONFIG_OF)
/* TODO: Fix base addresses. */
unsigned long dcm_infracfg_ao_base;
unsigned long dcm_mcucfg_base;
unsigned long dcm_mcucfg_phys_base;
unsigned long dcm_dramc0_ao_base;
unsigned long dcm_dramc1_ao_base;
unsigned long dcm_ddrphy0_ao_base;
unsigned long dcm_ddrphy1_ao_base;
unsigned long dcm_chen0_emi_base;
unsigned long dcm_chn0_emi_base;
unsigned long dcm_chn1_emi_base;
unsigned long dcm_emi_base;

#define DCM_NODE "mediatek,mt6765-dcm"

#endif /* #if defined(__KERNEL__) && defined(CONFIG_OF) */

short is_dcm_bringup(void)
{
#ifdef DCM_BRINGUP
	dcm_pr_info("%s: skipped for bring up\n", __func__);
	return 1;
#else
	return 0;
#endif
}

/*****************************************
 * following is implementation per DCM module.
 * 1. per-DCM function is 1-argu with ON/OFF/MODE option.
 *****************************************/
static int dcm_topckg_is_on(void)
{
	int ret = 1;

	return ret;
}

int dcm_topckg(int on)
{
	return 0;
}

void dcm_infracfg_ao_emi_indiv(int on)
{
}

int dcm_infra_preset(int on)
{
	return 0;
}

static int dcm_infra_is_on(void)
{
	bool ret = true;

	ret &= dcm_infracfg_ao_infra_peri_is_on();
	ret &= dcm_infracfg_ao_infra_mem_is_on();
	ret &= dcm_infracfg_ao_p2p_dsi_csi_is_on();

	ret &= dcm_infracfg_ao_audio_is_on();
	ret &= dcm_infracfg_ao_icusb_is_on();
	ret &= dcm_infracfg_ao_ssusb_is_on();

	return ret;
}
int dcm_infra(int on)
{
	/*
	 * dcm_infracfg_ao_infra_bus_dcm(on);
	 */
	dcm_infracfg_ao_infra_peri(on);
	/*
	 * dcm_infracfg_ao_infra_emi_local_dcm(on);
	 */
	dcm_infracfg_ao_infra_mem(on);
	/*
	 * dcm_infracfg_ao_infra_rx_p2p_dcm(on);
	 */
	dcm_infracfg_ao_p2p_dsi_csi(on);
	/*
	 * dcm_infracfg_ao_peri_bus_dcm(on);
	 */
	/* in dcm_infracfg_ao_infra_peri(on); */
	/*
	 * dcm_infracfg_ao_peri_module_dcm(on);
	 */
	dcm_infracfg_ao_audio(on);
	dcm_infracfg_ao_icusb(on);
	dcm_infracfg_ao_ssusb(on);

	return 0;
}

static int dcm_peri_is_on(void)
{
	int ret = 1;

	return ret;
}

int dcm_peri(int on)
{
	return 0;
}

static int dcm_armcore_is_on(void)
{
	bool ret = true;

	ret &= dcm_mcu_misccfg_bus_arm_pll_divider_dcm_is_on();
	ret &= dcm_mcu_misccfg_mp0_arm_pll_divider_dcm_is_on();
	ret &= dcm_mcu_misccfg_mp1_arm_pll_divider_dcm_is_on();
	return ret;
}
int dcm_armcore(int mode)
{
	dcm_mcu_misccfg_bus_arm_pll_divider_dcm(mode);
	dcm_mcu_misccfg_mp0_arm_pll_divider_dcm(mode);
	dcm_mcu_misccfg_mp1_arm_pll_divider_dcm(mode);

	return 0;
}

static int dcm_mcusys_is_on(void)
{
	bool ret = true;

	ret &= dcm_mcu_misccfg_adb400_dcm_is_on();
	ret &= dcm_mcu_misccfg_bus_sync_dcm_is_on();
	ret &= dcm_mcu_misccfg_bus_clock_dcm_is_on();
	ret &= dcm_mcu_misccfg_bus_fabric_dcm_is_on();
	ret &= dcm_mcu_misccfg_l2_shared_dcm_is_on();
	ret &= dcm_mcu_misccfg_mp0_sync_dcm_enable_is_on();
	ret &= dcm_mcu_misccfg_mp1_sync_dcm_enable_is_on();
	ret &= dcm_mcu_misccfg_mcu_misc_dcm_is_on();
	return ret;
}
int dcm_mcusys(int on)
{
	dcm_mcu_misccfg_adb400_dcm(on);
	dcm_mcu_misccfg_bus_sync_dcm(on);
	dcm_mcu_misccfg_bus_clock_dcm(on);
	dcm_mcu_misccfg_bus_fabric_dcm(on);
	dcm_mcu_misccfg_l2_shared_dcm(on);
	dcm_mcu_misccfg_mp0_sync_dcm_enable(on);
	dcm_mcu_misccfg_mp1_sync_dcm_enable(on);
	dcm_mcu_misccfg_mcu_misc_dcm(on);

	return 0;
}

int dcm_mcusys_preset(int on)
{
	return 0;
}

int dcm_big_core_preset(void)
{
	return 0;
}

static int dcm_big_core_is_on(void)
{
	int ret = 1;

	return ret;
}

int dcm_big_core(int on)
{
	return 0;
}

int dcm_stall_preset(int on)
{
	/* Not gen'ed as MT6763. Check if necessary.
	 * dcm_mcu_misccfg_mp_stall_dcm(on);
	 */
	reg_write(SYNC_DCM_CLUSTER_CONFIG, 0x063f0000);

	return 0;
}

static int dcm_stall_is_on(void)
{
	bool ret = true;

	ret &= dcm_mcu_misccfg_mp0_stall_dcm_is_on();
	ret &= dcm_mcu_misccfg_mp1_stall_dcm_is_on();

	return ret;
}

int dcm_stall(int on)
{
	dcm_mcu_misccfg_mp0_stall_dcm(on);
	dcm_mcu_misccfg_mp1_stall_dcm(on);

	return 0;
}
static int dcm_gic_sync_is_on(void)
{
	bool ret = true;

	ret &= dcm_mcu_misccfg_gic_sync_dcm_is_on();

	return ret;
}
int dcm_gic_sync(int on)
{
	dcm_mcu_misccfg_gic_sync_dcm(on);

	return 0;
}

static int dcm_last_core_is_on(void)
{
	int ret = 1;

	return ret;
}

int dcm_last_core(int on)
{
	return 0;
}

static int dcm_rgu_is_on(void)
{
	bool ret = true;

	ret &= dcm_mp0_cpucfg_mp0_rgu_dcm_is_on();
	ret &= dcm_mp1_cpucfg_mp1_rgu_dcm_is_on();
	return ret;
}

int dcm_rgu(int on)
{
	dcm_mp0_cpucfg_mp0_rgu_dcm(on);
	dcm_mp1_cpucfg_mp1_rgu_dcm(on);

	return 0;
}

static int dcm_dramc_ao_is_on(void)
{
	int ret = 1;

	return ret;
}

int dcm_dramc_ao(int on)
{
	/* Not gen'd, Check why
	 * dcm_dramc_ch0_top1_dramc_dcm(on);
	 * dcm_dramc_ch1_top1_dramc_dcm(on);
	 */

	return 0;
}

static int dcm_ddrphy_is_on(void)
{
	int ret = 1;

	return ret;
}

int dcm_ddrphy(int on)
{
	/* Not gen'd, Check why
	 * dcm_dramc_ch0_top0_ddrphy(on);
	 * dcm_dramc_ch1_top0_ddrphy(on);
	 */

	return 0;
}

static int dcm_emi_is_on(void)
{
	int ret = 1;

	ret &= dcm_chn0_emi_dcm_emi_group_is_on();
	ret &= dcm_chn1_emi_dcm_emi_group_is_on();
	return ret;
}

int dcm_emi(int on)
{
	/* Not gen'd, Check why
	 * dcm_emi_dcm_emi_group(on);
	 */
	dcm_chn0_emi_dcm_emi_group(on);
	dcm_chn1_emi_dcm_emi_group(on);

	return 0;
}

static int dcm_lpdma_is_on(void)
{
	int ret = 1;

	return ret;
}

int dcm_lpdma(int on)
{
	return 0;
}

int dcm_pwrap(int on)
{
	return 0;
}

int dcm_mcsi_preset(int on)
{
	return 0;
}

static int dcm_mcsi_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcucfg_mcsi_dcm_is_on();
	return ret;
}

int dcm_mcsi(int on)
{
	dcm_mcucfg_mcsi_dcm(on);

	return 0;
}

struct DCM dcm_array[] = {
	{
	 .typeid = ARMCORE_DCM_TYPE,
	 .name = "ARMCORE_DCM",
	 .func = (DCM_FUNC) dcm_armcore,
	 .is_on_func = dcm_armcore_is_on,
	 .default_state = ARMCORE_DCM_MODE1,
	 },
	{
	 .typeid = MCUSYS_DCM_TYPE,
	 .name = "MCUSYS_DCM",
	 .func = (DCM_FUNC) dcm_mcusys,
	 .is_on_func = dcm_mcusys_is_on,
	 .default_state = MCUSYS_DCM_ON,
	 },
	{
	 .typeid = INFRA_DCM_TYPE,
	 .name = "INFRA_DCM",
	 .func = (DCM_FUNC) dcm_infra,
	 .is_on_func = dcm_infra_is_on,
	 .default_state = INFRA_DCM_ON,
	 },
	{
	 .typeid = PERI_DCM_TYPE,
	 .name = "PERI_DCM",
	 .func = (DCM_FUNC) dcm_peri,
	 .is_on_func = dcm_peri_is_on,
	 .default_state = PERI_DCM_ON,
	 },
	{
	 .typeid = EMI_DCM_TYPE,
	 .name = "EMI_DCM",
	 .func = (DCM_FUNC) dcm_emi,
	 .is_on_func = dcm_emi_is_on,
	 .default_state = EMI_DCM_ON,
	 },
	{
	 .typeid = DRAMC_DCM_TYPE,
	 .name = "DRAMC_DCM",
	 .func = (DCM_FUNC) dcm_dramc_ao,
	 .is_on_func = dcm_dramc_ao_is_on,
	 .default_state = DRAMC_AO_DCM_ON,
	 },
	{
	 .typeid = DDRPHY_DCM_TYPE,
	 .name = "DDRPHY_DCM",
	 .func = (DCM_FUNC) dcm_ddrphy,
	 .is_on_func = dcm_ddrphy_is_on,
	 .default_state = DDRPHY_DCM_ON,
	 },
	{
	 .typeid = STALL_DCM_TYPE,
	 .name = "STALL_DCM",
	 .func = (DCM_FUNC) dcm_stall,
	 .is_on_func = dcm_stall_is_on,
	 .default_state = STALL_DCM_ON,
	 },
	{
	 .typeid = BIG_CORE_DCM_TYPE,
	 .name = "BIG_CORE_DCM",
	 .func = (DCM_FUNC) dcm_big_core,
	 .is_on_func = dcm_big_core_is_on,
	 .default_state = BIG_CORE_DCM_ON,
	 },
	{
	 .typeid = GIC_SYNC_DCM_TYPE,
	 .name = "GIC_SYNC_DCM",
	 .func = (DCM_FUNC) dcm_gic_sync,
	 .is_on_func = dcm_gic_sync_is_on,
	 .default_state = GIC_SYNC_DCM_ON,
	 },
	{
	 .typeid = LAST_CORE_DCM_TYPE,
	 .name = "LAST_CORE_DCM",
	 .func = (DCM_FUNC) dcm_last_core,
	 .is_on_func = dcm_last_core_is_on,
	 .default_state = LAST_CORE_DCM_ON,
	 },
	{
	 .typeid = RGU_DCM_TYPE,
	 .name = "RGU_CORE_DCM",
	 .func = (DCM_FUNC) dcm_rgu,
	 .is_on_func = dcm_rgu_is_on,
	 .default_state = RGU_DCM_ON,
	 },
	{
	 .typeid = TOPCKG_DCM_TYPE,
	 .name = "TOPCKG_DCM",
	 .func = (DCM_FUNC) dcm_topckg,
	 .is_on_func = dcm_topckg_is_on,
	 .default_state = TOPCKG_DCM_ON,
	 },
	{
	 .typeid = LPDMA_DCM_TYPE,
	 .name = "LPDMA_DCM",
	 .func = (DCM_FUNC) dcm_lpdma,
	 .is_on_func = dcm_lpdma_is_on,
	 .default_state = LPDMA_DCM_ON,
	 },
	{
	 .typeid = MCSI_DCM_TYPE,
	 .name = "MCSI_DCM",
	 .func = (DCM_FUNC) dcm_mcsi,
	 .is_on_func = dcm_mcsi_is_on,
	 .default_state = MCSI_DCM_ON,
	 },
	/* Keep this NULL element for array traverse */
	{0},
};

void dcm_dump_regs(void)
{
	dcm_pr_info("\n******** dcm dump register *********\n");
	/*
	 * REG_DUMP(CPUSYS_RGU_SYNC_DCM);
	 */
	REG_DUMP(L2C_SRAM_CTRL);
	REG_DUMP(CCI_CLK_CTRL);
	REG_DUMP(BUS_FABRIC_DCM_CTRL);
	REG_DUMP(MCU_MISC_DCM_CTRL);
	REG_DUMP(CCI_ADB400_DCM_CONFIG);
	REG_DUMP(SYNC_DCM_CONFIG);
	REG_DUMP(SYNC_DCM_CLUSTER_CONFIG);
	REG_DUMP(MP_GIC_RGU_SYNC_DCM);
	REG_DUMP(MP0_PLL_DIVIDER_CFG);
	REG_DUMP(MP1_PLL_DIVIDER_CFG);
	REG_DUMP(BUS_PLL_DIVIDER_CFG);
	REG_DUMP(MCSIA_DCM_EN);

	REG_DUMP(INFRA_BUS_DCM_CTRL);
	REG_DUMP(PERI_BUS_DCM_CTRL);
	REG_DUMP(MEM_DCM_CTRL);
	REG_DUMP(P2P_RX_CLK_ON);

	/* Not gen'ed
	 * REG_DUMP(EMI_CONM);
	 * REG_DUMP(EMI_CONN);
	 */
	REG_DUMP(CHN0_EMI_CHN_EMI_CONB);
	REG_DUMP(CHN1_EMI_CHN_EMI_CONB);

	/* Not gen'ed */
}
void get_init_state_and_type(unsigned int *type, int *state)
{
#if defined(DCM_DEFAULT_ALL_OFF)
	*type = ALL_DCM_TYPE;
	*state = DCM_OFF;
#elif defined(ENABLE_DCM_IN_LK)
	*type = INIT_DCM_TYPE_BY_K;
	*state = DCM_INIT;
#else
	*type = init_dcm_type;
	*state = DCM_INIT;
#endif
}

struct DCM_OPS dcm_ops = {
	.dump_regs = (DCM_FUNC_VOID_VOID) dcm_dump_regs,
	.get_init_state_and_type = (DCM_FUNC_VOID_UINTR_INTR) get_init_state_and_type,
};

struct DCM_BASE dcm_base_array[] = {
	DCM_BASE_INFO(dcm_infracfg_ao_base),
	DCM_BASE_INFO(dcm_mcucfg_base),
	DCM_BASE_INFO(dcm_mcucfg_phys_base),
	DCM_BASE_INFO(dcm_dramc0_ao_base),
	DCM_BASE_INFO(dcm_dramc1_ao_base),
	DCM_BASE_INFO(dcm_ddrphy0_ao_base),
	DCM_BASE_INFO(dcm_ddrphy1_ao_base),
	DCM_BASE_INFO(dcm_chen0_emi_base),
	DCM_BASE_INFO(dcm_chn0_emi_base),
	DCM_BASE_INFO(dcm_chn1_emi_base),
	DCM_BASE_INFO(dcm_emi_base),
};
void dcm_array_register(void)
{
	mt_dcm_array_register(dcm_array, &dcm_ops);
}
/*From DCM COMMON*/

#if IS_ENABLED(CONFIG_OF)
int mt_dcm_dts_map(void)
{
	struct device_node *node;
	unsigned int i;
	/* dcm */
	node = of_find_compatible_node(NULL, NULL, DCM_NODE);
	if (!node) {
		dcm_pr_info("error: cannot find node %s\n", DCM_NODE);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(dcm_base_array); i++) {
		//*dcm_base_array[i].base= (unsigned long)of_iomap(node, i);
		*(dcm_base_array[i].base) = (unsigned long)of_iomap(node, i);

		if (!*(dcm_base_array[i].base)) {
			dcm_pr_info("error: cannot iomap base %s\n",
				dcm_base_array[i].name);
			return -1;
		}
	}
	/* infracfg_ao */
	return 0;
}
#else
int mt_dcm_dts_map(void)
{
	return 0;
}
#endif /* #if IS_ENABLED(CONFIG_OF) */

void dcm_pre_init(void)
{
	dcm_pr_info("weak function of %s\n", __func__);
}

static int __init mt6765_dcm_init(void)
{
	int ret = 0;

	if (is_dcm_bringup())
		return 0;

	if (is_dcm_initialized())
		return 0;

	if (mt_dcm_dts_map()) {
		dcm_pr_notice("%s: failed due to DTS failed\n", __func__);
		return -1;
	}

	dcm_array_register();

	ret = mt_dcm_common_init();

	return ret;
}

static void __exit mt6765_dcm_exit(void)
{
}

MODULE_SOFTDEP("pre:mtk_dcm.ko");
module_init(mt6765_dcm_init);
module_exit(mt6765_dcm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek DCM driver");
