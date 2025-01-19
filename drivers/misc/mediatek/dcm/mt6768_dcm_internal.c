/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
#include <linux/io.h>
//#include <mt-plat/mtk_io.h>
//#include <mt-plat/sync_write.h>
//#include <mt-plat/mtk_secure_api.h>

#include "mt6768_dcm_internal.h"
#include "mtk_dcm.h"

#define DEBUGLINE dcm_pr_info("%s %d\n", __func__, __LINE__)

unsigned int init_dcm_type = ALL_DCM_TYPE;

#if IS_ENABLED(__KERNEL__) && IS_ENABLED(CONFIG_OF)
unsigned long dcm_infracfg_ao_base;
unsigned long dcm_mcucfg_base;
unsigned long dcm_cpccfg_rg_base;
unsigned long dcm_dramc_ch0_top0_ao_base;
unsigned long dcm_dramc_ch1_top0_ao_base;
unsigned long dcm_dramc_ch0_top1_ao_base;
unsigned long dcm_dramc_ch1_top1_ao_base;
unsigned long dcm_ch0_emi_base;
unsigned long dcm_ch1_emi_base;
unsigned long dcm_emi_base;

#define DCM_NODE "mediatek,mt6768-dcm"


#endif /* #if IS_ENABLED(__KERNEL__) && IS_ENABLED(CONFIG_OF) */

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

void dcm_infracfg_ao_emi_indiv(int on)
{
}

int dcm_infra_preset(int on)
{
	return 0;
}

int dcm_infra(int on)
{
	dcm_infracfg_ao_infra_bus(on);
	dcm_infracfg_ao_peri_bus(on);

	dcm_infracfg_ao_audio_bus(on);
	dcm_infracfg_ao_icusb_bus(on);

	return 0;
}

int dcm_armcore(int mode)
{
	dcm_mp_cpusys_top_bus_pll_div_dcm(mode);
	dcm_mp_cpusys_top_cpu_pll_div_0_dcm(mode);
	dcm_mp_cpusys_top_cpu_pll_div_1_dcm(mode);
	dcm_mp_cpusys_top_cpu_pll_div_2_dcm(mode);

	return 0;
}

int dcm_mcusys(int on)
{
	dcm_mp_cpusys_top_adb_dcm(on);
	dcm_mp_cpusys_top_apb_dcm(on);
	dcm_mp_cpusys_top_cpubiu_dbg_cg(on);
	dcm_mp_cpusys_top_cpubiu_dcm(on);
	dcm_mp_cpusys_top_misc_dcm(on);
	dcm_mp_cpusys_top_mp0_qdcm(on);
	dcm_cpccfg_reg_emi_wfifo(on);
	dcm_mp_cpusys_top_last_cor_idle_dcm(on);

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

int dcm_stall_preset(int on)
{
	return 0;
}

int dcm_stall(int on)
{
	dcm_mp_cpusys_top_core_stall_dcm(on);
	dcm_mp_cpusys_top_fcm_stall_dcm(on);
	return 0;
}

int dcm_dramc_ao(int on)
{
	dcm_dramc_ch0_top1_dramc_dcm(on);
	dcm_dramc_ch1_top1_dramc_dcm(on);
	return 0;
}

int dcm_ddrphy(int on)
{
	dcm_dramc_ch0_top0_ddrphy(on);
	dcm_dramc_ch1_top0_ddrphy(on);
	return 0;
}

int dcm_emi(int on)
{
	dcm_emi_emi_dcm(on);
	dcm_chn0_emi_emi_dcm(on);
	dcm_chn1_emi_emi_dcm(on);
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

void dcm_dump_regs(void)
{
	dcm_pr_info("\n******** dcm dump register *********\n");
	REG_DUMP(CPU_PLLDIV_CFG0);
	REG_DUMP(CPU_PLLDIV_CFG1);
	REG_DUMP(CPU_PLLDIV_CFG2);
	REG_DUMP(BUS_PLLDIV_CFG);
	REG_DUMP(MCSI_CFG2);
	REG_DUMP(MCSI_DCM0);
	REG_DUMP(MP_ADB_DCM_CFG4);
	REG_DUMP(MP_MISC_DCM_CFG0);
	REG_DUMP(MCUSYS_DCM_CFG0);
	REG_DUMP(EMI_WFIFO);
	REG_DUMP(MP0_DCM_CFG0);
	REG_DUMP(MP0_DCM_CFG7);
	REG_DUMP(INFRA_BUS_DCM_CTRL);
	REG_DUMP(PERI_BUS_DCM_CTRL);
	REG_DUMP(MEM_DCM_CTRL);
	REG_DUMP(DFS_MEM_DCM_CTRL);
	/*REG_DUMP(PMIC_WRAP_DCM_EN);*/
	REG_DUMP(EMI_CONM);
	REG_DUMP(EMI_CONN);
	REG_DUMP(DRAMC_CH0_TOP0_MISC_CG_CTRL0);
	REG_DUMP(DRAMC_CH0_TOP0_MISC_CG_CTRL2);
	REG_DUMP(DRAMC_CH0_TOP0_MISC_CTRL3);
	REG_DUMP(DRAMC_CH0_TOP1_DRAMC_PD_CTRL);
	REG_DUMP(DRAMC_CH0_TOP1_CLKAR);
	REG_DUMP(CHN0_EMI_CHN_EMI_CONB);
	REG_DUMP(DRAMC_CH1_TOP0_MISC_CG_CTRL0);
	REG_DUMP(DRAMC_CH1_TOP0_MISC_CG_CTRL2);
	REG_DUMP(DRAMC_CH1_TOP0_MISC_CTRL3);
	REG_DUMP(DRAMC_CH1_TOP1_DRAMC_PD_CTRL);
	REG_DUMP(DRAMC_CH1_TOP1_CLKAR);
	REG_DUMP(CHN1_EMI_CHN_EMI_CONB);
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
	DCM_BASE_INFO(dcm_cpccfg_rg_base),
	DCM_BASE_INFO(dcm_dramc_ch0_top0_ao_base),
	DCM_BASE_INFO(dcm_dramc_ch1_top0_ao_base),
	DCM_BASE_INFO(dcm_dramc_ch0_top1_ao_base),
	DCM_BASE_INFO(dcm_dramc_ch1_top1_ao_base),
	DCM_BASE_INFO(dcm_emi_base),
	DCM_BASE_INFO(dcm_ch0_emi_base),
	DCM_BASE_INFO(dcm_ch1_emi_base),
};

static int dcm_armcore_is_on(void)
{
	int ret = 1;

	ret &= dcm_mp_cpusys_top_bus_pll_div_dcm_is_on();
	ret &= dcm_mp_cpusys_top_cpu_pll_div_0_dcm_is_on();
	ret &= dcm_mp_cpusys_top_cpu_pll_div_1_dcm_is_on();
	// ret &= dcm_mp_cpusys_top_cpu_pll_div_2_dcm_is_on();

	return ret;
}

static int dcm_mcusys_is_on(void)
{
	int ret = 1;

	ret &= dcm_mp_cpusys_top_adb_dcm_is_on();
	ret &= dcm_mp_cpusys_top_apb_dcm_is_on();
	ret &= dcm_mp_cpusys_top_cpubiu_dbg_cg_is_on();
	ret &= dcm_mp_cpusys_top_cpubiu_dcm_is_on();
	ret &= dcm_mp_cpusys_top_misc_dcm_is_on();
	ret &= dcm_mp_cpusys_top_mp0_qdcm_is_on();
	ret &= dcm_cpccfg_reg_emi_wfifo_is_on();
	ret &= dcm_mp_cpusys_top_last_cor_idle_dcm_is_on();

	return ret;
}

static int dcm_infra_is_on(void)
{
	int ret = 1;

	ret &= dcm_infracfg_ao_infra_bus_is_on();
	ret &= dcm_infracfg_ao_peri_bus_is_on();
	ret &= dcm_infracfg_ao_audio_bus_is_on();
	ret &= dcm_infracfg_ao_icusb_bus_is_on();

	return ret;
}

static int dcm_emi_is_on(void)
{
	int ret = 1;

	ret &= dcm_emi_emi_dcm_is_on();
	ret &= dcm_chn0_emi_emi_dcm_is_on();
	ret &= dcm_chn1_emi_emi_dcm_is_on();

	return ret;
}

static int dcm_dramc_ao_is_on(void)
{
	int ret = 1;

	ret &= dcm_emi_emi_dcm_is_on();
	ret &= dcm_chn0_emi_emi_dcm_is_on();
	ret &= dcm_chn1_emi_emi_dcm_is_on();

	return ret;
}

static int dcm_ddrphy_is_on(void)
{
	int ret = 1;

	ret &= dcm_dramc_ch0_top0_ddrphy_is_on();
	ret &= dcm_dramc_ch1_top0_ddrphy_is_on();

	return ret;
}

static int dcm_stall_is_on(void)
{
	int ret = 1;

	ret &= dcm_mp_cpusys_top_core_stall_dcm_is_on();
	ret &= dcm_mp_cpusys_top_fcm_stall_dcm_is_on();

	return ret;
}

static struct DCM dcm_array[] = {
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
	/* Keep this NULL element for array traverse */
	{0},
};

/**/
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

static int __init mt6768_dcm_init(void)
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

static void __exit mt6768_dcm_exit(void)
{
}

MODULE_SOFTDEP("pre:mtk_dcm.ko");
module_init(mt6768_dcm_init);
module_exit(mt6768_dcm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek DCM driver");