// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
/* #include <mt-plat/mtk_io.h> */
/* #include <mt-plat/sync_write.h> */
/* #include <mt-plat/mtk_secure_api.h> */
#include <mt6833_dcm_internal.h>
#include "mtk_dcm.h"


unsigned int init_dcm_type = ALL_DCM_TYPE;

#if defined(__KERNEL__) && IS_ENABLED(CONFIG_OF)
unsigned long dcm_infracfg_ao_base;
unsigned long dcm_infra_ao_bcrm_base;
unsigned long dcm_mcusys_par_wrap_base;
unsigned long dcm_mp_cpusys_top_base;
unsigned long dcm_cpccfg_reg_base;
unsigned long dcm_mcusys_cfg_reg_base;

#define DCM_NODE "mediatek,mt6833-dcm"

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
	int ret = 1;

	ret &= dcm_infracfg_ao_aximem_bus_dcm_is_on();
	ret &= dcm_infracfg_ao_infra_bus_dcm_is_on();
	ret &= dcm_infracfg_ao_infra_rx_p2p_dcm_is_on();
	ret &= dcm_infracfg_ao_peri_bus_dcm_is_on();
	ret &= dcm_infracfg_ao_peri_module_dcm_is_on();
	ret &= dcm_infra_ao_bcrm_infra_bus_dcm_is_on();
	ret &= dcm_infra_ao_bcrm_peri_bus_dcm_is_on();

	return ret;
}

int dcm_infra(int on)
{
	dcm_infracfg_ao_aximem_bus_dcm(on);
	dcm_infracfg_ao_infra_bus_dcm(on);
	dcm_infracfg_ao_infra_conn_bus_dcm(on);
	dcm_infracfg_ao_infra_rx_p2p_dcm(on);
	dcm_infracfg_ao_peri_bus_dcm(on);
	dcm_infracfg_ao_peri_module_dcm(on);
	dcm_infra_ao_bcrm_infra_bus_dcm(on);
	dcm_infra_ao_bcrm_peri_bus_dcm(on);

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
	int ret = 1;

	ret &= dcm_mp_cpusys_top_bus_pll_div_dcm_is_on();

	return ret;
}

int dcm_armcore(int on)
{
	dcm_mp_cpusys_top_bus_pll_div_dcm(on);

	return 0;
}

static int dcm_mcusys_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_big_dcm_is_on();
	ret &= dcm_mcusys_par_wrap_little_dcm_is_on();
	ret &= dcm_mp_cpusys_top_adb_dcm_is_on();
	ret &= dcm_mp_cpusys_top_apb_dcm_is_on();
	ret &= dcm_mp_cpusys_top_cpubiu_dcm_is_on();
	ret &= dcm_mp_cpusys_top_last_cor_idle_dcm_is_on();
	ret &= dcm_mp_cpusys_top_misc_dcm_is_on();
	ret &= dcm_mp_cpusys_top_mp0_qdcm_is_on();
	ret &= dcm_cpccfg_reg_emi_wfifo_is_on();
	ret &= dcm_mcusys_cfg_reg_apb_dcm_is_on();
	ret &= dcm_mcusys_cfg_reg_mp0_qdcm_is_on();

	return ret;
}

int dcm_mcusys(int on)
{
	dcm_mcusys_par_wrap_big_dcm(on);
	dcm_mcusys_par_wrap_little_dcm(on);
	dcm_mp_cpusys_top_adb_dcm(on);
	dcm_mp_cpusys_top_apb_dcm(on);
	dcm_mp_cpusys_top_cpubiu_dcm(on);
	dcm_mp_cpusys_top_last_cor_idle_dcm(on);
	dcm_mp_cpusys_top_misc_dcm(on);
	dcm_mp_cpusys_top_mp0_qdcm(on);
	dcm_cpccfg_reg_emi_wfifo(on);
	dcm_mcusys_cfg_reg_apb_dcm(on);
	dcm_mcusys_cfg_reg_mp0_qdcm(on);

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
	return 0;
}

static int dcm_stall_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_cfg_reg_core_stall_dcm_is_on();
	ret &= dcm_mcusys_cfg_reg_fcm_stall_dcm_is_on();

	return ret;
}

int dcm_stall(int on)
{
	dcm_mcusys_cfg_reg_core_stall_dcm(on);
	dcm_mcusys_cfg_reg_fcm_stall_dcm(on);

	return 0;
}

static int dcm_gic_sync_is_on(void)
{
	int ret = true;

	return ret;
}

int dcm_gic_sync(int on)
{
	return 0;
}

static int dcm_last_core_is_on(void)
{
	int ret = true;

	return ret;
}

int dcm_last_core(int on)
{
	return 0;
}

static int dcm_rgu_is_on(void)
{
	int ret = true;

	return ret;
}

int dcm_rgu(int on)
{
	return 0;
}
static int dcm_dramc_ao_is_on(void)
{
	int ret = 1;

	return ret;
}

int dcm_dramc_ao(int on)
{
	return 0;
}

static int dcm_ddrphy_is_on(void)
{
	int ret = 1;

	return ret;
}

int dcm_ddrphy(int on)
{

	return 0;
}

static int dcm_emi_is_on(void)
{
	int ret = 1;

	return ret;
}


int dcm_emi(int on)
{

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

	return ret;
}
int dcm_mcsi(int on)
{
	return 0;
}

void dcm_dump_regs(void)
{
	dcm_pr_info("\n******** dcm dump register *********\n");

	REG_DUMP(MCUSYS_PAR_WRAP_CPU_STALL_DCM_CTRL);
	REG_DUMP(MCUSYS_PAR_WRAP_STALL_DCM_CONF);
	REG_DUMP(MP_CPUSYS_TOP_BUS_PLLDIV_CFG);
	REG_DUMP(MP_CPUSYS_TOP_MCSIC_DCM0);
	REG_DUMP(MP_CPUSYS_TOP_MP_ADB_DCM_CFG0);
	REG_DUMP(MP_CPUSYS_TOP_MP_ADB_DCM_CFG4);
	REG_DUMP(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0);
	REG_DUMP(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0);
	REG_DUMP(CPCCFG_REG_EMI_WFIFO);
	REG_DUMP(MCUSYS_CFG_REG_MP0_DCM_CFG0);
	REG_DUMP(MCUSYS_CFG_REG_MP0_DCM_CFG7);
	REG_DUMP(INFRA_BUS_DCM_CTRL);
	REG_DUMP(PERI_BUS_DCM_CTRL);
	REG_DUMP(P2P_RX_CLK_ON);
	REG_DUMP(MODULE_SW_CG_2_STA);
	REG_DUMP(INFRA_AXIMEM_IDLE_BIT_EN_0);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_9);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_10);
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
	DCM_BASE_INFO(dcm_infra_ao_bcrm_base),
	DCM_BASE_INFO(dcm_mcusys_par_wrap_base),
	DCM_BASE_INFO(dcm_mp_cpusys_top_base),
	DCM_BASE_INFO(dcm_cpccfg_reg_base),
	DCM_BASE_INFO(dcm_mcusys_cfg_reg_base),
};

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
	 {0},
};

void dcm_set_hotplug_nb(void) {}

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

static int __init mt6833_dcm_init(void)
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

static void __exit mt6833_dcm_exit(void)
{
}
MODULE_SOFTDEP("pre:mtk_dcm.ko");
module_init(mt6833_dcm_init);
module_exit(mt6833_dcm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek DCM driver");
