// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/io.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
//#include <mt-plat/mtk_io.h>
//#include <mt-plat/sync_write.h>
//#include <mt-plat/mtk_secure_api.h>

#include <mt6781_dcm_internal.h>
#include "mtk_dcm.h"

unsigned int init_dcm_type = ALL_DCM_TYPE;



#if defined(__KERNEL__) && IS_ENABLED(CONFIG_OF)
/* TODO: Fix base addresses. */

unsigned long dcm_infracfg_ao_base;
unsigned long dcm_mp_cpusys_top_base;
unsigned long dcm_cpccfg_reg_base;

#define DCM_NODE "mediatek,mt6781-dcm"

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
/* TODO: Fix*/

static int dcm_infra_is_on(void)
{
	int ret = true;

	ret &= dcm_infracfg_ao_audio_bus_is_on();
	ret &= dcm_infracfg_ao_icusb_bus_is_on();
	ret &= dcm_infracfg_ao_infra_bus_is_on();
	ret &= dcm_infracfg_ao_p2p_rx_clk_is_on();
	ret &= dcm_infracfg_ao_peri_bus_is_on();

	return ret;
}

int dcm_infra(int on)
{
	dcm_infracfg_ao_audio_bus(on);
	dcm_infracfg_ao_icusb_bus(on);
	dcm_infracfg_ao_infra_bus(on);
	dcm_infracfg_ao_p2p_rx_clk(on);
	dcm_infracfg_ao_peri_bus(on);

	return 0;
}

static int dcm_armcore_is_on(void)
{
	int ret = true;

	ret &= dcm_mp_cpusys_top_bus_pll_div_dcm_is_on();
	ret &= dcm_mp_cpusys_top_cpu_pll_div_0_dcm_is_on();
	ret &= dcm_mp_cpusys_top_cpu_pll_div_1_dcm_is_on();

	return ret;
}

int dcm_armcore(int on)
{
	dcm_mp_cpusys_top_bus_pll_div_dcm(on);
	dcm_mp_cpusys_top_cpu_pll_div_0_dcm(on);
	dcm_mp_cpusys_top_cpu_pll_div_1_dcm(on);

	return 0;
}

static int dcm_mcusys_is_on(void)
{
	int ret = true;

	ret &= dcm_mp_cpusys_top_adb_dcm_is_on();
	ret &= dcm_mp_cpusys_top_apb_dcm_is_on();
	ret &= dcm_mp_cpusys_top_cpubiu_dbg_cg_is_on();
	ret &= dcm_mp_cpusys_top_cpubiu_dcm_is_on();
	ret &= dcm_mp_cpusys_top_last_cor_idle_dcm_is_on();
	ret &= dcm_mp_cpusys_top_misc_dcm_is_on();
	ret &= dcm_mp_cpusys_top_mp0_qdcm_is_on();
	ret &= dcm_cpccfg_reg_emi_wfifo_is_on();

	return ret;
}

int dcm_mcusys(int on)
{
	dcm_mp_cpusys_top_adb_dcm(on);
	dcm_mp_cpusys_top_apb_dcm(on);
	dcm_mp_cpusys_top_cpubiu_dbg_cg(on);
	dcm_mp_cpusys_top_cpubiu_dcm(on);
	dcm_mp_cpusys_top_last_cor_idle_dcm(on);
	dcm_mp_cpusys_top_misc_dcm(on);
	dcm_mp_cpusys_top_mp0_qdcm(on);
	dcm_cpccfg_reg_emi_wfifo(on);

	return 0;
}



static int dcm_stall_is_on(void)
{
	int ret = true;

	ret &= dcm_mp_cpusys_top_core_stall_dcm_is_on();
	ret &= dcm_mp_cpusys_top_fcm_stall_dcm_is_on();

	return ret;
}

int dcm_stall(int on)
{
	dcm_mp_cpusys_top_core_stall_dcm(on);
	dcm_mp_cpusys_top_fcm_stall_dcm(on);

	return 0;
}

static int dcm_busdvt_is_on(void)
{
	int ret = true;

	ret &= dcm_infracfg_ao_infra_bus_is_on();
	ret &= dcm_infracfg_ao_peri_bus_is_on();

	return ret;
}

int dcm_busdvt(int on)
{
	/* bus dvt */
	dcm_infracfg_ao_infra_bus(on);
	dcm_infracfg_ao_peri_bus(on);

	return 0;
}
/*****************************************
 * following is implementation per DCM module.
 * 1. per-DCM function is 1-argu with ON/OFF/MODE option.
 *****************************************/

void dcm_dump_regs(void)
{
	dcm_pr_info("\n******** dcm dump register *********\n");

	REG_DUMP(MP_CPUSYS_TOP_CPU_PLLDIV_CFG0);
	REG_DUMP(MP_CPUSYS_TOP_CPU_PLLDIV_CFG1);
	REG_DUMP(MP_CPUSYS_TOP_BUS_PLLDIV_CFG);
	REG_DUMP(MP_CPUSYS_TOP_MCSI_CFG2);
	REG_DUMP(MP_CPUSYS_TOP_MCSIC_DCM0);
	REG_DUMP(MP_CPUSYS_TOP_MP_ADB_DCM_CFG4);
	REG_DUMP(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0);
	REG_DUMP(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0);
	REG_DUMP(CPCCFG_REG_EMI_WFIFO);
	REG_DUMP(MP_CPUSYS_TOP_MP0_DCM_CFG0);
	REG_DUMP(MP_CPUSYS_TOP_MP0_DCM_CFG7);
	REG_DUMP(INFRA_BUS_DCM_CTRL);
	REG_DUMP(PERI_BUS_DCM_CTRL);
	REG_DUMP(P2P_RX_CLK_ON);
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
	DCM_BASE_INFO(dcm_mp_cpusys_top_base),
	DCM_BASE_INFO(dcm_cpccfg_reg_base),
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
	 .typeid = STALL_DCM_TYPE,
	 .name = "STALL_DCM",
	 .func = (DCM_FUNC) dcm_stall,
	 .is_on_func = dcm_stall_is_on,
	 .default_state = STALL_DCM_ON,
	 },
	{
	 .typeid = BUSDVT_DCM_TYPE,
	 .name = "MCSI_DCM",
	 .func = (DCM_FUNC) dcm_busdvt,
	 .is_on_func = dcm_busdvt_is_on,
	 .default_state = BUSDVT_DCM_ON,
	 },
	 {0},
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

static int __init mt6781_dcm_init(void)
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

static void __exit mt6781_dcm_exit(void)
{
}

MODULE_SOFTDEP("pre:mtk_dcm.ko");
module_init(mt6781_dcm_init);
module_exit(mt6781_dcm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek DCM driver");



