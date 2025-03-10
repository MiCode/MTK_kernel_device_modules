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
#include "mt6895_dcm_internal.h"
#include "mtk_dcm.h"

#define enable_infra_aximem 0
#define DEBUGLINE dcm_pr_info("%s %d\n", __func__, __LINE__)

static short dcm_cpu_cluster_stat;


unsigned int init_dcm_type = ALL_DCM_TYPE;

#if defined(__KERNEL__) && defined(CONFIG_OF)
unsigned long dcm_infracfg_ao_base;
unsigned long dcm_infra_ao_bcrm_base;
unsigned long dcm_infracfg_ao_mem_base;
unsigned long dcm_peri_ao_bcrm_base;
unsigned long dcm_vlp_ao_bcrm_base;
unsigned long dcm_mp_cpusys_top_base;
unsigned long dcm_cpccfg_reg_base;

#define DCM_NODE "mediatek,mt6895-dcm"

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
int dcm_topckg(int on)
{
	return 0;
}

static int dcm_topckg_is_on(void)
{
	return 1;
}

void dcm_infracfg_ao_emi_indiv(int on)
{
}

int dcm_infra_preset(int on)
{
	return 0;
}

int dcm_infra(int on)
{
	dcm_infracfg_ao_aximem_bus_dcm(on);
	dcm_infracfg_ao_infra_bus_dcm(on);
	dcm_infracfg_ao_infra_rx_p2p_dcm(on);
	dcm_infra_ao_bcrm_infra_bus_dcm(on);
	dcm_infra_ao_bcrm_infra_bus_fmem_sub_dcm(on);

	return 0;
}

static int dcm_infra_is_on(void)
{
	int ret = 1;
#if enable_infra_aximem
	ret &= dcm_infracfg_ao_aximem_bus_dcm_is_on();
#endif
	ret &= dcm_infracfg_ao_infra_bus_dcm_is_on();
	ret &= dcm_infracfg_ao_infra_rx_p2p_dcm_is_on();
	ret &= dcm_infra_ao_bcrm_infra_bus_dcm_is_on();
	ret &= dcm_infra_ao_bcrm_infra_bus_fmem_sub_dcm_is_on();
	return ret;
}

int dcm_peri(int on)
{
	dcm_peri_ao_bcrm_peri_bus_dcm(on);
	return 0;
}

static int dcm_peri_is_on(void)
{
	int ret = 1;
	return ret;
}

int dcm_mcusys_acp(int on)
{
	return 0;
}

static int dcm_mcusys_acp_is_on(void)
{
	return 1;
}

int dcm_mcusys_adb(int on)
{
	return 0;
}

static int dcm_mcusys_adb_is_on(void)
{
	return 1;
}

int dcm_mcusys_bus(int on)
{
	return 0;
}

static int dcm_mcusys_bus_is_on(void)
{
	return 1;
}

int dcm_mcusys_cbip(int on)
{
	return 0;
}

static int dcm_mcusys_cbip_is_on(void)
{
	return 1;
}

int dcm_mcusys_core(int on)
{
	return 0;
}

static int dcm_mcusys_core_is_on(void)
{
	return 1;
}

int dcm_mcusys_io(int on)
{
	return 0;
}

static int dcm_mcusys_io_is_on(void)
{
	return 1;
}

int dcm_mcusys_cpc_pbi(int on)
{
	return 0;
}

static int dcm_mcusys_cpc_pbi_is_on(void)
{
	return 1;
}

int dcm_mcusys_cpc_turbo(int on)
{
	return 0;
}

static int dcm_mcusys_cpc_turbo_is_on(void)
{
	return 1;
}

int dcm_mcusys_stall(int on)
{
	return 0;
}

static int dcm_mcusys_stall_is_on(void)
{
	return 1;
}

int dcm_mcusys_apb(int on)
{
	return 0;
}

static int dcm_mcusys_apb_is_on(void)
{
	return 1;
}

int dcm_vlp(int on)
{
	dcm_vlp_ao_bcrm_vlp_bus_dcm(on);
	return 0;
}

static int dcm_vlp_is_on(void)
{
	int ret = 1;

	ret &= dcm_vlp_ao_bcrm_vlp_bus_dcm_is_on();
	return ret;
}

int dcm_armcore(int on)
{
	dcm_mp_cpusys_top_cpu_pll_div_0_dcm(on);
	dcm_mp_cpusys_top_cpu_pll_div_1_dcm(on);
	dcm_mp_cpusys_top_cpu_pll_div_2_dcm(on);

	return 0;
}
static int dcm_armcore_is_on(void)
{
	int ret = 1;

	ret &= dcm_mp_cpusys_top_cpu_pll_div_0_dcm_is_on();
	ret &= dcm_mp_cpusys_top_cpu_pll_div_1_dcm_is_on();
	ret &= dcm_mp_cpusys_top_cpu_pll_div_2_dcm_is_on();
	return ret;
}
int dcm_mcusys(int on)
{
	dcm_mp_cpusys_top_adb_dcm(on);
	dcm_mp_cpusys_top_apb_dcm(on);
	dcm_mp_cpusys_top_bus_pll_div_dcm(on);
	dcm_mp_cpusys_top_core_stall_dcm(on);
	dcm_mp_cpusys_top_cpubiu_dcm(on);
	dcm_mp_cpusys_top_fcm_stall_dcm(on);
	dcm_mp_cpusys_top_last_cor_idle_dcm(on);
	dcm_mp_cpusys_top_misc_dcm(on);
	dcm_mp_cpusys_top_mp0_qdcm(on);
	dcm_cpccfg_reg_emi_wfifo(on);

	return 0;
}

static int dcm_mcusys_is_on(void)
{
	int ret = 1;

	ret &= dcm_mp_cpusys_top_adb_dcm_is_on();
	ret &= dcm_mp_cpusys_top_apb_dcm_is_on();
	ret &= dcm_mp_cpusys_top_bus_pll_div_dcm_is_on();
	ret &= dcm_mp_cpusys_top_core_stall_dcm_is_on();
	ret &= dcm_mp_cpusys_top_cpubiu_dcm_is_on();
	ret &= dcm_mp_cpusys_top_fcm_stall_dcm_is_on();
	ret &= dcm_mp_cpusys_top_last_cor_idle_dcm_is_on();
	ret &= dcm_mp_cpusys_top_misc_dcm_is_on();
	ret &= dcm_mp_cpusys_top_mp0_qdcm_is_on();
	ret &= dcm_cpccfg_reg_emi_wfifo_is_on();
	return ret;
}

int dcm_mcusys_preset(int on)
{
	return 0;
}

int dcm_big_core_preset(void)
{
	return 0;
}

int dcm_big_core(int on)
{
	return 0;
}

static int dcm_big_core_is_on(void)
{
	return 1;
}

int dcm_stall_preset(int on)
{
	return 0;
}

int dcm_stall(int on)
{

	return 0;
}

static int dcm_stall_is_on(void)
{

	return 1;
}

int dcm_gic_sync(int on)
{
	return 0;
}

static int dcm_gic_sync_is_on(void)
{
	return 1;
}

int dcm_last_core(int on)
{
	return 0;
}

static int dcm_last_core_is_on(void)
{
	return 1;
}


int dcm_rgu(int on)
{
	return 0;
}

static int dcm_rgu_is_on(void)
{
	return 1;
}

int dcm_dramc_ao(int on)
{

	return 0;
}

static int dcm_dramc_ao_is_on(void)
{

	return 1;
}

int dcm_ddrphy(int on)
{

	return 0;
}

static int dcm_ddrphy_is_on(void)
{

	return 1;
}

int dcm_emi(int on)
{

	return 0;
}

static int dcm_emi_is_on(void)
{

	return 1;
}

int dcm_lpdma(int on)
{
	return 0;
}

static int dcm_lpdma_is_on(void)
{
	return 1;
}

int dcm_pwrap(int on)
{
	return 0;
}

int dcm_mcsi_preset(int on)
{
	return 0;
}

int dcm_mcsi(int on)
{
	return 0;
}

static int dcm_mcsi_is_on(void)
{
	return 1;
}

void dcm_dump_regs(void)
{
	dcm_pr_info("\n******** dcm dump register *********\n");

	REG_DUMP(MP_CPUSYS_TOP_CPU_PLLDIV_CFG0);
	REG_DUMP(MP_CPUSYS_TOP_CPU_PLLDIV_CFG1);
	REG_DUMP(MP_CPUSYS_TOP_CPU_PLLDIV_CFG2);
	REG_DUMP(MP_CPUSYS_TOP_BUS_PLLDIV_CFG);
	REG_DUMP(MP_CPUSYS_TOP_MCSIC_DCM0);
	REG_DUMP(MP_CPUSYS_TOP_MP_ADB_DCM_CFG0);
	REG_DUMP(MP_CPUSYS_TOP_MP_ADB_DCM_CFG4);
	REG_DUMP(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0);
	REG_DUMP(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0);
	REG_DUMP(CPCCFG_REG_EMI_WFIFO);
	REG_DUMP(MP_CPUSYS_TOP_MP0_DCM_CFG0);
	REG_DUMP(MP_CPUSYS_TOP_MP0_DCM_CFG7);
	REG_DUMP(INFRA_BUS_DCM_CTRL);
	REG_DUMP(P2P_RX_CLK_ON);
	REG_DUMP(INFRA_AXIMEM_IDLE_BIT_EN_0);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2);
	REG_DUMP(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0);
	REG_DUMP(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1);
	REG_DUMP(VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_0);
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
	DCM_BASE_INFO(dcm_infracfg_ao_mem_base),
	DCM_BASE_INFO(dcm_peri_ao_bcrm_base),
	DCM_BASE_INFO(dcm_vlp_ao_bcrm_base),
	DCM_BASE_INFO(dcm_mp_cpusys_top_base),
	DCM_BASE_INFO(dcm_cpccfg_reg_base),
};

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
	 .typeid = PERI_DCM_TYPE,
	 .name = "PERI_DCM",
	 .func = (DCM_FUNC) dcm_peri,
	 .is_on_func = dcm_peri_is_on,
	 .default_state = PERI_DCM_ON,
	 },
	{
	 .typeid = MCUSYS_ACP_DCM_TYPE,
	 .name = "MCU_ACP_DCM",
	 .func = (DCM_FUNC) dcm_mcusys_acp,
	 .is_on_func = dcm_mcusys_acp_is_on,
	 .default_state = MCUSYS_ACP_DCM_ON,
	},
	{
	 .typeid = MCUSYS_ADB_DCM_TYPE,
	 .name = "MCU_ADB_DCM",
	 .func = (DCM_FUNC) dcm_mcusys_adb,
	 .is_on_func = dcm_mcusys_adb_is_on,
	 .default_state = MCUSYS_ADB_DCM_ON,
	},
	{
	 .typeid = MCUSYS_BUS_DCM_TYPE,
	 .name = "MCU_BUS_DCM",
	 .func = (DCM_FUNC) dcm_mcusys_bus,
	 .is_on_func = dcm_mcusys_bus_is_on,
	 .default_state = MCUSYS_BUS_DCM_ON,
	},
	{
	 .typeid = MCUSYS_CBIP_DCM_TYPE,
	 .name = "MCU_CBIP_DCM",
	 .func = (DCM_FUNC) dcm_mcusys_cbip,
	 .is_on_func = dcm_mcusys_cbip_is_on,
	 .default_state = MCUSYS_CBIP_DCM_ON,
	},
	{
	 .typeid = MCUSYS_CORE_DCM_TYPE,
	 .name = "MCU_CORE_DCM",
	 .func = (DCM_FUNC) dcm_mcusys_core,
	 .is_on_func = dcm_mcusys_core_is_on,
	 .default_state = MCUSYS_CORE_DCM_ON,
	},
	{
	 .typeid = MCUSYS_IO_DCM_TYPE,
	 .name = "MCU_IO_DCM",
	 .func = (DCM_FUNC) dcm_mcusys_io,
	 .is_on_func = dcm_mcusys_io_is_on,
	 .default_state = MCUSYS_IO_DCM_ON,
	},
	{
	 .typeid = MCUSYS_CPC_PBI_DCM_TYPE,
	 .name = "MCU_CPC_PBI_DCM",
	 .func = (DCM_FUNC) dcm_mcusys_cpc_pbi,
	 .is_on_func = dcm_mcusys_cpc_pbi_is_on,
	 .default_state = MCUSYS_CPC_PBI_DCM_ON,
	},
	{
	 .typeid = MCUSYS_CPC_TURBO_DCM_TYPE,
	 .name = "MCU_CPC_TURBO_DCM",
	 .func = (DCM_FUNC) dcm_mcusys_cpc_turbo,
	 .is_on_func = dcm_mcusys_cpc_turbo_is_on,
	 .default_state = MCUSYS_CPC_TURBO_DCM_ON,
	},
	{
	 .typeid = MCUSYS_STALL_DCM_TYPE,
	 .name = "MCU_STALL_DCM",
	 .func = (DCM_FUNC) dcm_mcusys_stall,
	 .is_on_func = dcm_mcusys_stall_is_on,
	 .default_state = MCUSYS_STALL_DCM_ON,
	},
	{
	 .typeid = MCUSYS_APB_DCM_TYPE,
	 .name = "MCU_APB_DCM",
	 .func = (DCM_FUNC) dcm_mcusys_apb,
	 .is_on_func = dcm_mcusys_apb_is_on,
	 .default_state = MCUSYS_APB_DCM_ON,
	},
	{
	 .typeid = VLP_DCM_TYPE,
	 .name = "VLP_DCM",
	 .func = (DCM_FUNC) dcm_vlp,
	 .is_on_func = dcm_vlp_is_on,
	 .default_state = VLP_DCM_ON,
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

void dcm_set_hotplug_nb(void) {}

short dcm_get_cpu_cluster_stat(void)
{
	return dcm_cpu_cluster_stat;
}

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

static int __init mt6895_dcm_init(void)
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

static void __exit mt6895_dcm_exit(void)
{
}
MODULE_SOFTDEP("pre:mtk_dcm.ko");
module_init(mt6895_dcm_init);
module_exit(mt6895_dcm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek DCM driver");
