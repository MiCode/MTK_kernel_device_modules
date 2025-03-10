// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/io.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
/* #include <mt-plat/mtk_io.h> */
/* #include <mt-plat/sync_write.h> */
/* include <mt-plat/mtk_secure_api.h> */
#include "mt6993_dcm_internal.h"
#include "mtk_dcm.h"

#define DEBUGLINE dcm_pr_info("%s %d\n", __func__, __LINE__)

unsigned int init_dcm_type = ALL_DCM_TYPE;

#if defined(__KERNEL__) && IS_ENABLED(CONFIG_OF)
unsigned long dcm_mcusys_par_wrap_base;
unsigned long dcm_bcrm_apinfra_io_ctrl_ao;
unsigned long dcm_bcrm_apinfra_io_noc_ao;
unsigned long dcm_bcrm_apinfra_mem_intf_noc_ao;
unsigned long dcm_bcrm_apinfra_hash_ctrl_ao;
unsigned long dcm_bcrm_apinfra_mem_ctrl_ao;
unsigned long dcm_bcrm_apinfra_io_east_ao;
unsigned long dcm_peri_ao_bcrm_base;
unsigned long dcm_pericfg_ao_reg_base;
unsigned long dcm_vlp_ao_bcrm_base;

#define DCM_NODE "mediatek,mt6993-dcm"

#endif /* #if defined(__KERNEL__) && IS_ENABLED(CONFIG_OF) */

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
static int dcm_infra(int on)
{
	dcm_bcrm_apinfra_hash_ctrl_ao_infra_bus_dcm(on);
	dcm_bcrm_apinfra_io_ctrl_ao_apinfra_faxi_off(on);
	dcm_bcrm_apinfra_io_east_ao_apinfra_io_est_faxi_off(on);
	dcm_bcrm_apinfra_io_noc_ao_apinfra_io_noc_off(on);
	dcm_bcrm_apinfra_mem_ctrl_ao_infra_bus_dcm(on);
	dcm_bcrm_apinfra_mem_intf_noc_ao_infra_bus_dcm(on);
	return 0;
}

static int dcm_mcusys_adb(int on)
{
	dcm_mcusys_par_wrap_mcu_adb_dcm(on);
	return 0;
}

static int dcm_mcusys_apb(int on)
{
	dcm_mcusys_par_wrap_mcu_apb_dcm(on);
	return 0;
}

static int dcm_mcusys_cdb(int on)
{
	dcm_mcusys_par_wrap_mcu_cdb_dcm(on);
	return 0;
}

static int dcm_mcusys_core(int on)
{
	dcm_mcusys_par_wrap_mcu_core_qdcm(on);
	return 0;
}

static int dcm_mcusys_dbgnw(int on)
{
	dcm_mcusys_par_wrap_mcu_dbgnw_dcm(on);
	return 0;
}

static int dcm_mcusys_dsu(int on)
{
	dcm_mcusys_par_wrap_mcu_dsu_qdcm(on);
	return 0;
}

static int dcm_mcusys_dsu_bus(int on)
{
	dcm_mcusys_par_wrap_dsu_bus_dcm(on);
	return 0;
}

static int dcm_mcusys_gic(int on)
{
	dcm_mcusys_par_wrap_mcu_gic_qdcm(on);
	dcm_mcusys_par_wrap_mcu_gic_spi_dcm(on);
	return 0;
}

static int dcm_mcusys_l3c(int on)
{
	dcm_mcusys_par_wrap_mcu_l3c_dcm(on);
	return 0;
}

static int dcm_mcusys_mpsys_bus(int on)
{
	dcm_mcusys_par_wrap_mpsys_bus_qdcm(on);
	return 0;
}

static int dcm_mcusys_mpsys_dbg(int on)
{
	dcm_mcusys_par_wrap_mpsys_dbg_qdcm(on);
	return 0;
}

static int dcm_mcusys_mst_vps_bus(int on)
{
	dcm_mcusys_par_wrap_mcu_mst_vps_bus_dcm(on);
	return 0;
}

static int dcm_mcusys_slv_vc_bus(int on)
{
	dcm_mcusys_par_wrap_mcu_slv_vc_bus_dcm(on);
	return 0;
}

static int dcm_mcusys_slv_vps_bus(int on)
{
	dcm_mcusys_par_wrap_mcu_slv_vps_bus_dcm(on);
	return 0;
}

static int dcm_mcusys_stall(int on)
{
	dcm_mcusys_par_wrap_mcu_stalldcm(on);
	return 0;
}

static int dcm_mcusys_vdnr(int on)
{
	dcm_mcusys_par_wrap_mcu_vdnr_dcm(on);
	return 0;
}

static int dcm_peri(int on)
{
	dcm_peri_ao_bcrm_peri_bus_dcm(on);
	dcm_pericfg_ao_reg_peri_bus_dcm(on);
	return 0;
}

static int dcm_vlp(int on)
{
	dcm_vlp_ao_bcrm_vlp_bus_dcm(on);
	return 0;
}


/*****************************************
 * following is implementation per DCM module.
 * 1. per-DCM function will return DCM status (On/Off)
 *****************************************/
static int dcm_infra_is_on(void)
{
	int ret = 1;

	ret &= dcm_bcrm_apinfra_hash_ctrl_ao_infra_bus_dcm_is_on();
	ret &= dcm_bcrm_apinfra_io_ctrl_ao_apinfra_faxi_off_is_on();
	ret &= dcm_bcrm_apinfra_io_east_ao_apinfra_io_est_faxi_off_is_on();
	ret &= dcm_bcrm_apinfra_io_noc_ao_apinfra_io_noc_off_is_on();
	ret &= dcm_bcrm_apinfra_mem_ctrl_ao_infra_bus_dcm_is_on();
	ret &= dcm_bcrm_apinfra_mem_intf_noc_ao_infra_bus_dcm_is_on();

	return ret;
}

static int dcm_mcusys_adb_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_adb_dcm_is_on();

	return ret;
}

static int dcm_mcusys_apb_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_apb_dcm_is_on();

	return ret;
}

static int dcm_mcusys_cdb_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_cdb_dcm_is_on();

	return ret;
}

static int dcm_mcusys_core_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_core_qdcm_is_on();

	return ret;
}

static int dcm_mcusys_dbgnw_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_dbgnw_dcm_is_on();

	return ret;
}

static int dcm_mcusys_dsu_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_dsu_qdcm_is_on();

	return ret;
}

static int dcm_mcusys_dsu_bus_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_dsu_bus_dcm_is_on();

	return ret;
}

static int dcm_mcusys_gic_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_gic_qdcm_is_on();
	ret &= dcm_mcusys_par_wrap_mcu_gic_spi_dcm_is_on();

	return ret;
}

static int dcm_mcusys_l3c_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_l3c_dcm_is_on();

	return ret;
}

static int dcm_mcusys_mpsys_bus_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mpsys_bus_qdcm_is_on();

	return ret;
}

static int dcm_mcusys_mpsys_dbg_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mpsys_dbg_qdcm_is_on();

	return ret;
}

static int dcm_mcusys_mst_vps_bus_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_mst_vps_bus_dcm_is_on();

	return ret;
}

static int dcm_mcusys_slv_vc_bus_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_slv_vc_bus_dcm_is_on();

	return ret;
}

static int dcm_mcusys_slv_vps_bus_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_slv_vps_bus_dcm_is_on();

	return ret;
}

static int dcm_mcusys_stall_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_stalldcm_is_on();

	return ret;
}

static int dcm_mcusys_vdnr_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_vdnr_dcm_is_on();

	return ret;
}

static int dcm_peri_is_on(void)
{
	int ret = 1;

	ret &= dcm_peri_ao_bcrm_peri_bus_dcm_is_on();
	ret &= dcm_pericfg_ao_reg_peri_bus_dcm_is_on();

	return ret;
}

static int dcm_vlp_is_on(void)
{
	int ret = 1;

	ret &= dcm_vlp_ao_bcrm_vlp_bus_dcm_is_on();

	return ret;
}


void dcm_dump_regs(void)
{
	dcm_pr_info("\n******** dcm dump register *********\n");

	REG_DUMP(BCRM_APINFRA_IO_EAST_AO_APINFRA_IO_EST_FAXI_OFF_REG0);
	REG_DUMP(CLK_AXI_VDNR_DCM_TOP_APINFRA_IO_INTX_BUS_u_APINFRA_IO_INTX_BUS_CTRL_0);
	REG_DUMP(CLK_FHASH_CFG_VDNR_DCM_TOP_APINFRA_HASH_INTX_BUS_u_APINFRA_HASH_INTX_BUS_CTRL_0);
	REG_DUMP(CLK_FHASH_NOC_VDNR_DCM_TOP_APINFRA_HASH_INTX_BUS_u_APINFRA_HASH_INTX_BUS_CTRL_0);
	REG_DUMP(CLK_FHASH_VDNR_DCM_TOP_APINFRA_HASH_INTX_BUS_u_APINFRA_HASH_INTX_BUS_CTRL_0);
	REG_DUMP(CLK_FMEM_SUB_CFG_VDNR_DCM_TOP_APINFRA_MEM_INTX_BUS_u_APINFRA_MEM_INTX_BUS_CTRL_0);
	REG_DUMP(CLK_FMEM_SUB_VDNR_DCM_TOP_APINFRA_MEM_INTX_BUS_u_APINFRA_MEM_INTX_BUS_CTRL_0);
	REG_DUMP(CLK_FMEM_SUB_VDNR_DCM_TOP_APINFRA_MEM_INTX_BUS_u_APINFRA_MEM_INTX_BUS_CTRL_1);
	REG_DUMP(CLK_FMEM_SUB_VDNR_DCM_TOP_APINFRA_MEM_INTX_BUS_u_APINFRA_MEM_INTX_BUS_CTRL_2);
	REG_DUMP(CLK_FMEM_SUB_VDNR_DCM_TOP_APINFRA_MEM_INTX_BUS_u_APINFRA_MEM_INTX_BUS_CTRL_3);
	REG_DUMP(CLK_FMEM_SUB_VDNR_DCM_TOP_APINFRA_MEM_INTX_BUS_u_APINFRA_MEM_INTX_BUS_CTRL_4);
	REG_DUMP(CLK_IO_NOC_VDNR_DCM_TOP_APINFRA_IO_INTF_PAR_BUS_u_APINFRA_IO_INTF_PAR_BUS_CTRL_0);
	REG_DUMP(CLUSTER0_MCUSYS_MST_VLP_DSU_DCM_CFG);
	REG_DUMP(CLUSTER0_MCUSYS_MST_VLP_GIC_SPI_SLOW_CK_CFG);
	REG_DUMP(CLUSTER0_MCUSYS_MST_VLP_L3_SHARE_DCM_CTRL);
	REG_DUMP(CLUSTER0_MCUSYS_MST_VLP_MCUSYS_MST_VPS_AO_DCM_CFG);
	REG_DUMP(CLUSTER0_MCUSYS_MST_VLP_MCUSYS_MST_VPS_DCM_CFG);
	REG_DUMP(CLUSTER0_MCUSYS_MST_VLP_MCUSYS_SLV_VC_DCM_CFG);
	REG_DUMP(CLUSTER0_MCUSYS_MST_VLP_MCUSYS_SLV_VPS_DCM_CFG);
	REG_DUMP(CLUSTER0_MCUSYS_MST_VLP_MPSYS_DCM_CFG);
	REG_DUMP(CLUSTER0_MCUSYS_MST_VLP_QDCM_CONFIG0);
	REG_DUMP(CLUSTER0_MCUSYS_MST_VLP_QDCM_CONFIG1);
	REG_DUMP(CLUSTER0_MCUSYS_MST_VLP_QDCM_CONFIG2);
	REG_DUMP(CLUSTER0_MCUSYS_MST_VLP_QDCM_CONFIG3);
	REG_DUMP(CLUSTER0_MCUSYS_MST_VLP_QDCM_CONFIG4);
	REG_DUMP(PERICFG_AO_REG_PERI_MASK_IDLE_TO_CKSYS);
	REG_DUMP(VDNR_DCM_TOP_APINFRA_MEM_INTF_PAR_BUS_u_APINFRA_MEM_INTF_PAR_BUS_CTRL_0);
	REG_DUMP(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0);
	REG_DUMP(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1);
	REG_DUMP(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2);
	REG_DUMP(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_3);
	REG_DUMP(VDNR_DCM_TOP_VLP_PAR_BUS_TOP_u_VLP_PAR_BUS_TOP_CTRL_0);
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
	DCM_BASE_INFO(dcm_mcusys_par_wrap_base),
	DCM_BASE_INFO(dcm_bcrm_apinfra_io_ctrl_ao),
	DCM_BASE_INFO(dcm_bcrm_apinfra_io_noc_ao),
	DCM_BASE_INFO(dcm_bcrm_apinfra_mem_intf_noc_ao),
	DCM_BASE_INFO(dcm_bcrm_apinfra_hash_ctrl_ao),
	DCM_BASE_INFO(dcm_bcrm_apinfra_mem_ctrl_ao),
	DCM_BASE_INFO(dcm_bcrm_apinfra_io_east_ao),
	DCM_BASE_INFO(dcm_peri_ao_bcrm_base),
	DCM_BASE_INFO(dcm_pericfg_ao_reg_base),
	DCM_BASE_INFO(dcm_vlp_ao_bcrm_base),
};

static struct DCM dcm_array[] = {
	{
	 .typeid = INFRA_DCM_TYPE,
	 .name = "INFRA_DCM",
	 .func = dcm_infra,
	 .is_on_func = dcm_infra_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_ADB_DCM_TYPE,
	 .name = "MCUSYS_ADB_DCM",
	 .func = dcm_mcusys_adb,
	 .is_on_func = dcm_mcusys_adb_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_APB_DCM_TYPE,
	 .name = "MCUSYS_APB_DCM",
	 .func = dcm_mcusys_apb,
	 .is_on_func = dcm_mcusys_apb_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_CDB_DCM_TYPE,
	 .name = "MCUSYS_CDB_DCM",
	 .func = dcm_mcusys_cdb,
	 .is_on_func = dcm_mcusys_cdb_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_CORE_DCM_TYPE,
	 .name = "MCUSYS_CORE_DCM",
	 .func = dcm_mcusys_core,
	 .is_on_func = dcm_mcusys_core_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_DBGNW_DCM_TYPE,
	 .name = "MCUSYS_DBGNW_DCM",
	 .func = dcm_mcusys_dbgnw,
	 .is_on_func = dcm_mcusys_dbgnw_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_DSU_DCM_TYPE,
	 .name = "MCUSYS_DSU_DCM",
	 .func = dcm_mcusys_dsu,
	 .is_on_func = dcm_mcusys_dsu_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_DSU_BUS_DCM_TYPE,
	 .name = "MCUSYS_DSU_BUS_DCM",
	 .func = dcm_mcusys_dsu_bus,
	 .is_on_func = dcm_mcusys_dsu_bus_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_GIC_DCM_TYPE,
	 .name = "MCUSYS_GIC_DCM",
	 .func = dcm_mcusys_gic,
	 .is_on_func = dcm_mcusys_gic_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_L3C_DCM_TYPE,
	 .name = "MCUSYS_L3C_DCM",
	 .func = dcm_mcusys_l3c,
	 .is_on_func = dcm_mcusys_l3c_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_MPSYS_BUS_DCM_TYPE,
	 .name = "MCUSYS_MPSYS_BUS_DCM",
	 .func = dcm_mcusys_mpsys_bus,
	 .is_on_func = dcm_mcusys_mpsys_bus_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_MPSYS_DBG_DCM_TYPE,
	 .name = "MCUSYS_MPSYS_DBG_DCM",
	 .func = dcm_mcusys_mpsys_dbg,
	 .is_on_func = dcm_mcusys_mpsys_dbg_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_MST_VPS_BUS_DCM_TYPE,
	 .name = "MCUSYS_MST_VPS_BUS_DCM",
	 .func = dcm_mcusys_mst_vps_bus,
	 .is_on_func = dcm_mcusys_mst_vps_bus_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_SLV_VC_BUS_DCM_TYPE,
	 .name = "MCUSYS_SLV_VC_BUS_DCM",
	 .func = dcm_mcusys_slv_vc_bus,
	 .is_on_func = dcm_mcusys_slv_vc_bus_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_SLV_VPS_BUS_DCM_TYPE,
	 .name = "MCUSYS_SLV_VPS_BUS_DCM",
	 .func = dcm_mcusys_slv_vps_bus,
	 .is_on_func = dcm_mcusys_slv_vps_bus_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_STALL_DCM_TYPE,
	 .name = "MCUSYS_STALL_DCM",
	 .func = dcm_mcusys_stall,
	 .is_on_func = dcm_mcusys_stall_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_VDNR_DCM_TYPE,
	 .name = "MCUSYS_VDNR_DCM",
	 .func = dcm_mcusys_vdnr,
	 .is_on_func = dcm_mcusys_vdnr_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = PERI_DCM_TYPE,
	 .name = "PERI_DCM",
	 .func = dcm_peri,
	 .is_on_func = dcm_peri_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = VLP_DCM_TYPE,
	 .name = "VLP_DCM",
	 .func = dcm_vlp,
	 .is_on_func = dcm_vlp_is_on,
	 .default_state = DCM_ON,
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

static int __init mt6993_dcm_init(void)
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

static void __exit mt6993_dcm_exit(void)
{
}
MODULE_SOFTDEP("pre:mtk_dcm.ko");
module_init(mt6993_dcm_init);
module_exit(mt6993_dcm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek DCM driver");
