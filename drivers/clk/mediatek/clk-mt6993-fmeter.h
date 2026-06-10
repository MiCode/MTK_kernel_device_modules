/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Kuanhsin Lee <kuan-hsin.lee@mediatek.com>
 */

#ifndef FQMETER_H
#define FQMETER_H
#include "clk-fmeter.h"

/* align fclks_arr index */
enum FQMTR_ARR_ID {
    HF_FAXI_CK,
    HF_FPERI_AXI_CK,
    HF_FCH_INFRA_AXI_CK,
    HF_FCH_INFRA_CK,
    HF_FMEM_SUB_CK,
    HF_FHASH_SUB_CK,
    HF_FPERI_FMEM_SUB_CK,
    HF_FZRAM_SUB_CK,
    HF_FIO_NOC_CK,
    HF_FHASH_NOC_CK,
    HF_FPERI_NOC_CK,
    HF_FEMI_INTERFACE_546_CK,
    HF_FEMI_N_CK,
    HF_FEMI_S_CK,
    HF_FEMI_INFRA_CK,
    HF_FEMI_INFRA_SSPM_CK,
    F_FULPOSC_EMI_INFRA_CK,
    F_FEMI_INFRA_26M_CK,
    F_FCH_INFRA_SYS_26M_CK,
    HF_FCBUS_PHY_CK,
    HF_FATB_CK,
    HF_FCIRQ_CK,
    HF_FMCU_INFRA_CK,
    HF_FMCUPM_CK,
    HF_FSDF_CK,
    HF_FMFG_EB_CK,
    HF_FAPU_EXT_CK,
    F_FAP2CONN_HOST_CK,
    HF_FSSR_PKA_CK,
    HF_FSSR_DMA_CK,
    HF_FSSR_KDF_CK,
    HF_FSSR_RNG_CK,
    HF_FDXCC_CK,
    F_FEFUSE_CK,
    F_FMEM_DLY_IP_CK,
    F_FDPSW_CMP_26M_CK,
    F_FADSP_UARTHUB_BCLK_CK,
    HF_FAUD_1_CK,
    HF_FAUD_2_CK,
    HF_FDPMAIF_MAIN_CK,
    F_FIPSEAST_CK,
    F_FIPSWEST_CK,
    HF_FSMAPCK_CK,
    HF_FIPIC_CK,
    HF_FSPI0_BCLK_CK,
    HF_FSPI1_BCLK_CK,
    HF_FSPI2_BCLK_CK,
    HF_FSPI3_BCLK_CK,
    HF_FSPI4_BCLK_CK,
    HF_FSPI5_BCLK_CK,
    HF_FSPI6_BCLK_CK,
    HF_FSPI7_BCLK_CK,
    F_FPEXTP_MBIST_CK,
    HF_FTL_CK,
    HF_FTL_P1_CK,
    HF_FPWM_CK,
    HF_FAES_UFSFDE_0_CK,
    HF_FUFS_0_CK,
    F_FUFS_MBIST_0_CK,
    HF_FAES_UFSFDE_1_CK,
    HF_FUFS_1_CK,
    F_FUFS_MBIST_1_CK,
    F_FUARTHUB_BCLK_CK,
    F_FUART_CK,
    F_FI2C_PERI_CK,
    F_FI2C_NORTH_CK,
    F_FI2C_EAST_CK,
    F_FI2C_WEST_CK,
    HF_FMSDC_MACRO_1P_CK,
    HF_FMSDC_MACRO_2P_CK,
    HF_FMSDC30_1_CK,
    HF_FMSDC30_2_CK,
    HF_FCKSYS_MM_MAINPLL_D3_CK,
    HF_FCKSYS_MM_MAINPLL_D4_CK,
    HF_FCKSYS_MM_MAINPLL_D5_CK,
    HF_FCKSYS_MM_MAINPLL_D7_CK,
    HF_FCKSYS_VLP_MAINPLL_D4_CK,
    HF_FCKSYS_VLP_MAINPLL_D5_CK,
    HF_FCKSYS_VLP_MAINPLL_D6_CK,
    HF_FCKSYS_VLP_MAINPLL_D7_CK,
    HF_FCKSYS_VLP_MAINPLL_D9_CK,
    F_FGRIDSENSOR_CK,
    F_FAOV_26M_CK,
    HF_FEMI_WDAT_CK,
    CLKSQ_26M_CK,
    RTC_32K_CK,
    MAINPLL_CKDIV_CK,
    UNIVPLL_CKDIV_CK,
    EMIPLL_CKDIV_CK,
    MSDCPLL_CKDIV_CK,
    MCUSYS_ARM_CLK_OUT_ALL_CK_OUT,
    DSI1_LNTC_DSICLK_FQMTR_CK,
    AD_DSI1_MPPLL_TST_CK,
    DSI02_LNTC_DSICLK_FQMTR_CK,
    AD_DSI02_MPPLL_TST_CK,
    UFS_MP_CLK2FREQ_CK,
    GB_HD_FEMI_CK_1_,
    TOP_R0_OUT_FM,
    GB_HD_FEMI_CK_0_,
    STH_HD_FMEM2_CK,
    NTH_HD_FMEM2_CK,
    PEXTP_PHY_CLK_TO_FREQMETER_P1,
    PEXTP_PHY_CLK_TO_FREQMETER_P0,
    HF_FMMUP_CK,
    F_FMMINFRA_AO_CK,
    F_FMMINFRA_CK,
    F_FMMINFRA_SNOC_CK,
    HF_FVENC_CK,
    HF_FVENC_MDP_CK,
    HF_FVDEC_CK,
    HF_FIMG1_CK,
    HF_FIPE_CK,
    HF_FDISP_CK,
    HF_FMML_CK,
    HF_FDVO_DP_CK,
    HF_FDVO_FAVT_DP_CK,
    HF_FCAM_CK,
    F_FCAMTM_CK,
    HF_FCCUSYS_CK,
    F_FCCUTM_CK,
    F_FSENINF0_CK,
    F_FSENINF1_CK,
    F_FSENINF2_CK,
    F_FSENINF3_CK,
    F_FSENINF4_CK,
    F_FSENINF5_CK,
    F_FMMINFRA_SNOC_SLOW_CK,
    HF_FCKSYS_TOP_MMPLL_D2_CK,
    MAINPLL2_CKDIV_CK,
    UNIVPLL2_CKDIV_CK,
    MMPLL_CKDIV_CK,
    IMGPLL_CKDIV_CK,
    TVDPLL_CKDIV_CK,
    AD_CSI0A_DPHY_DELAYCAL_CK,
    AD_CSI0B_DPHY_DELAYCAL_CK,
    F_FSSPM_26M_CK,
    F_FULPOSC_SSPM_CK,
    HF_FSSPM_CK,
    HF_FSPM_CK,
    HF_FAXI_VLP_CK,
    HF_FNOC_VLP_CK,
    HF_FPWM_VLP_CK,
    HF_FSYSTIMER_26M_CK,
    HF_FDPSW_CK,
    HF_FDPSW_CENTRAL_CK,
    HF_FSRCK_CK,
    HF_FDVFSRC_CK,
    HF_FKP_IRQ_GEN_CK,
    HF_FDEBUG_ERR_FLAG_VLP_26M_CK,
    F_FIPS_CK,
    HF_FDPMSRDMA_CK,
    F_FVLP_PBUS_CK,
    F_FVLP_PBUS_26M_CK,
    F_FVCORE_PBUS_CK,
    F_FVCORE_PBUS_26M_CK,
    F_FCAMTG0_CK,
    F_FCAMTG1_CK,
    F_FCAMTG2_CK,
    F_FCAMTG3_CK,
    F_FCAMTG4_CK,
    F_FCAMTG5_CK,
    F_FCAMTG6_CK,
    F_FCAMTG7_CK,
    HF_FAUD_ENGEN1_CK,
    HF_FAUD_ENGEN2_CK,
    HF_FAUD_SW_ENGEN1_CK,
    HF_FAUD_SW_ENGEN2_CK,
    HF_FAUD_INTBUS_CK,
    HF_FAUDIO_H_CK,
    F_FUSB_TOP_CK,
    F_FSSUSB_XHCI_CK,
    F_FSPU_VLP_26M_CK,
    HF_FSPU0_VLP_CK,
    HF_FSPU1_VLP_CK,
    HF_FCRYWRAPPER_VLP_CK,
    HF_FSCP_CK,
    HF_FSCP_SPI_CK,
    HF_FSCP_IIC_CK,
    HF_FSCP_IIC_HIGH_SPD_CK,
    HF_FSCP_OIS_CK,
    HF_FUSB_MEM_VLP_CK,
    F_FDISP_PWM_CK,
    HF_FCKSYS_RSV_CK,
    F_FPWRAP_ULPOSC_CK,
    HF_FTIA_CK,
    HF_FSPMI_M_MST_CK,
    HF_FHVS_CK,
    MAINPLL_TST_CK,
    UNIVPLL_TST_CK,
    MSDCPLL_TST_CK,
    EMIPLL_TST_CK,
    MAINPLL2_TST_CK,
    UNIVPLL2_TST_CK,
    MMPLL_TST_CK,
    IMGPLL_TST_CK,
    TVDPLL_TST_CK,
    APLL1_TST_CK,
    APLL2_TST_CK,
    CCIPLL_TST_CK,
    PTPPLL_TST_CK,
    MAX_FQMTR_ARR_ID,
};

#define FM_UARTHUB_B_CK F_FUARTHUB_BCLK_CK
#define FM_UART_CK F_FUART_CK

#define ERR_RATIO 5
#define UPPER(tf, err_ratio) (((tf) * (100 + (err_ratio))) / (100 + 1))
#define LOWER(tf, err_ratio) (((tf) * (100 - (err_ratio))) / (100 - 1))

// preloader only
#define PLATFORM_KERNEL

#ifdef PLATFORM_PL
    #include "typedefs.h"
    #include "platform.h"
    #define fq_pr_err(fmt, ...) \
        print("[PLFQMTR][Error] %s:%d: " fmt, __func__, __LINE__, ##__VA_ARGS__)
    #define fq_pr_dbg(fmt, ...) \
        print("[PLFQMTR][DBG] %s:%d: " fmt, __func__, __LINE__, ##__VA_ARGS__)
    #define FQMTR_READL(addr) (DRV_Reg32(addr))
    #define FQMTR_WRITEL(val, addr) (DRV_WriteReg32(addr, val))
    #define configASSERT(x)
    #define DO_FAIL_MACRO() do { return -1; } while (0)
    #define SHOW_RESULT_MACRO() do {} while (0)

    #define CLKSQ_CTRL_BASE 0x13930000
    #define MAINPLL_BASE 0x1c213000
    #define UNIVPLL_BASE 0x1c213100
    #define MSDCPLL_BASE 0x1c213200
    #define EMIPLL_BASE 0x1c213300
    #define MAINPLL2_BASE 0x10043000
    #define UNIVPLL2_BASE 0x10043100
    #define MMPLL_BASE 0x10043200
    #define IMGPLL_BASE 0x10043300
    #define TVDPLL_BASE 0x10043400
    #define APLL1_BASE 0x1c203000
    #define APLL2_BASE 0x1c203100
    #define CCIPLL_BASE 0x05080000
    #define PTPPLL_BASE 0x05084000
    #define CKSYS_BASE 0x1C210000 //FM_CKSYS
    #define CKMTR_TOP_BASE 0x1C211000
    #define CKSYS2_BASE 0x10040000 //FM_CKSYS_GP2
    #define CKMTR_MM_BASE 0x10041000
    #define VLP_CKSYS_BASE 0x1C200000 //FM_VLP_CKSYS
    #define CKMTR_VLP_BASE 0x1C201000


#elif defined(PLATFORM_KERNEL)
    #include <linux/io.h>
	/*
	#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		#include <mt-plat/aee.h>
	    #define configASSERT(x) \
	    	do { aee_kernel_warning("fqmeter", "fqmeter error\n"); } while(0)
	#else
	*/
	#define configASSERT(x)              WARN_ON(1)
    #define FQMTR_READL(addr)   readl(addr)
    #define FQMTR_WRITEL(val, addr)   \
        do { writel(val, addr); wmb(); } while (0) /* sync write */
    #define fq_pr_err(fmt, ...) \
        pr_err("[FQMTR][Error] %s:%d: " fmt, __func__, __LINE__, ##__VA_ARGS__)
    #define fq_pr_dbg(fmt, ...) \
        pr_notice("[FQMTR][DBG] %s:%d: " fmt, __func__, __LINE__, ##__VA_ARGS__)
    #define DO_FAIL_MACRO() do { return -1; } while (0)
    #define SHOW_RESULT_MACRO() do {} while (0)

#elif defined(PLATFORM_COSIM)
    #include "project.h"
    #include "API.h"
    #include "cmessage.h"
    #define uint32_t UINT32
    #define fq_pr_err(fmt, ...)
    #define fq_pr_dbg(fmt, ...)
    #define FQMTR_WRITEL(val, addr) (*(volatile unsigned int *)(long)(addr)) = ((unsigned int) val)
    #define FQMTR_READL(addr) (*(volatile unsigned int *)(long)(addr))
    static UINT32 fail_result[MAX_FQMTR_ARR_ID];
    #define DO_FAIL_MACRO() do { fail_result[i] = cur_freq; } while (0)
    #define SHOW_RESULT_MACRO() \
        do { \
            int j; \
            for (j = 0; j < MAX_FQMTR_ARR_ID; j++) { \
               /*TINFO = "fail_result[%d] = %d Khz\n", j, fail_result[j]*/\
            } \
        } while (0)
#endif

// Timeout count
#define FQMTR_TIMEOUT_CNT 1000

#define UNIT_FOR_32K_1T_33US 0x7
#define UNIT_MARGIN 56
#define FQMTR_UDELAY_CNT (((UNIT_FOR_32K_1T_33US + 1)*33) + UNIT_MARGIN)

#define clk_dbg_cfg_ofs (0x340)
#define clk_misc_cfg_0_ofs   (0x380)
#define cksys2_clk_dbg_cfg_ofs (0x340)
#define cksys2_clk_misc_cfg_0_ofs (0x380)
#define vlp_fqmtr_con0_ofs (0x344)

// Register Offsets
#define CLKMON_REG_CON0_OFS(x) (0x0000 + x)
#define CLKMON_REG_CON1_OFS(x) (0x0004 + x)
#define CLKMON_REG_CON2_OFS(x) (0x0008 + x)
#define CLKMON_REG_CON3_OFS(x) (0x000C + x)
#define CLKMON_REG_CON4_OFS(x) (0x0010 + x)
#define CLKMON_REG_CON5_OFS(x) (0x0014 + x)
#define CLKMON_REG_CON6_OFS(x) (0x0018 + x)

#define clkmon_con_ofs 0x0040
#define pll_con0_ofs 0x0008
#define pll_con1_ofs 0x000C
#define ckmtr_base_ofs 0x0080

// Function Declarations
uint32_t cksys_top_fqmtr(enum FQMTR_ARR_ID);
uint32_t cksys_top_abist_fqmtr(enum FQMTR_ARR_ID);
uint32_t cksys_top_abist2_fqmtr(enum FQMTR_ARR_ID);
//uint32_t CKSYS_TOP_FQMTR_CHECK(uint32_t ID, uint32_t IDEAL_FREQ, uint32_t ERROR_RATIO);

uint32_t cksys_mm_fqmtr(enum FQMTR_ARR_ID);
uint32_t cksys_mm_abist_fqmtr(enum FQMTR_ARR_ID);
uint32_t cksys_mm_abist2_fqmtr(uint32_t ID);
//uint32_t CKSYS_MM_FQMTR_CHECK(uint32_t ID, uint32_t IDEAL_FREQ, uint32_t ERROR_RATIO);

uint32_t cksys_vlp_fqmtr(enum FQMTR_ARR_ID);
//uint32_t CKSYS_VLP_FQMTR_CHECK(uint32_t ID, uint32_t IDEAL_FREQ, uint32_t ERROR_RATIO);

uint32_t fqmtr_cal(enum DOMAIN_BASE domain, uint32_t cali_mode, uint32_t fqmtr_div,
                uint32_t load_cnt, uint32_t fqmtr_clkmux_sel, uint32_t refck_clmux_sel, uint32_t extra_ofs);

uint32_t pll_fqmtr(enum FQMTR_ARR_ID);

extern int post_init_fmeter_check(void);

#endif // FQMETER_H
