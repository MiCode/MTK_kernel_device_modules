// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#include <asm/kvm_pkvm_module.h>
#include <linux/arm-smccc.h>

#include "pkvm_p1_hyp.h"


const struct pkvm_module_ops *pkvm_p1_ops;



/*******************************************************************************
 * Internal Functions
 ******************************************************************************/
int Cam_Pkvm_Config_DMA_Sec(int bM4UEn, int bSecure, int domain, int dma_port)
{
	int pkvm_register_tag = SECIO_ISP_CAM_A;
	int dma_index;
	TZ_RESULT ret = TZ_RESULT_SUCCESS;

	if (dma_port >= CAM_DMA_PORT_MAX) {
		CALL_FROM_OPS(puts, PFX "ERROR: dma_port out of range!\n");
		return -1;
	}

	if (dma_port < CAM_IMGO_R1_B) {
		/* TG_A */
		pkvm_register_tag = SECIO_ISP_CAM_A;
		dma_index = dma_port;
	} else if (dma_port < CAM_DMA_PORT_MAX) {
		/* TG_B */
		pkvm_register_tag = SECIO_ISP_YUV_CAM_A;
		dma_index = dma_port - CAM_IMGO_R1_B;
	} else {
		/* TG_C */
		CALL_FROM_OPS(puts, PFX "ERROR: TG_C is not supported\n");
		return -1;
	}

	switch (dma_index) {
	case CAM_IMGO_R1_A: /* IMGO */
	{
		union REG_CAMDMATOP2_R1_CAMDMATOP2_SECURE_SMI_PORT_IMGO_R1  imgo_secure_port;

		CALL_FROM_OPS(puts, PFX "config_port_array: imgo\n");
		ret |= SECIO_READ(pkvm_register_tag, SECURE_SMI_PORT_IMGO_R1_OFFSET, &imgo_secure_port.Raw);
		if (bSecure) {
			imgo_secure_port.Bits.CAMDMATOP2_IMGO_R1_GDOMAIN = 0x7;
			imgo_secure_port.Bits.CAMDMATOP2_IMGO_R1_GSECURE = 1;
		} else {
			imgo_secure_port.Bits.CAMDMATOP2_IMGO_R1_GDOMAIN = 0;
			imgo_secure_port.Bits.CAMDMATOP2_IMGO_R1_GSECURE = 0;
		}
		ret |= SECIO_WRITE(pkvm_register_tag, SECURE_SMI_PORT_IMGO_R1_OFFSET, imgo_secure_port.Raw);
		port_sec_enable[dma_port] = bSecure;

		if (ret != TZ_RESULT_SUCCESS)
			CALL_FROM_OPS(puts, PFX "SECIO_ACCESS FAILED: imgo\n");
		else
			CALL_FROM_OPS(puts, PFX "SECIO_ACCESS success: imgo\n");
		break;
	}
	case CAM_RRZO_R1_A: /* RRZO */
	{
		union REG_CAMDMATOP2_R1_CAMDMATOP2_SECURE_SMI_PORT_RRZO_R1  rrzo_secure_port;

		CALL_FROM_OPS(puts, PFX "config_port_array: rrzo\n");
		ret |= SECIO_READ(pkvm_register_tag, SECURE_SMI_PORT_RRZO_R1_OFFSET, &rrzo_secure_port.Raw);
		if (bSecure) {
			rrzo_secure_port.Bits.CAMDMATOP2_RRZO_R1_GDOMAIN = 0x7;
			rrzo_secure_port.Bits.CAMDMATOP2_RRZO_R1_GSECURE = 1;
		} else {
			rrzo_secure_port.Bits.CAMDMATOP2_RRZO_R1_GDOMAIN = 0;
			rrzo_secure_port.Bits.CAMDMATOP2_RRZO_R1_GSECURE = 0;
		}
		ret |= SECIO_WRITE(pkvm_register_tag, SECURE_SMI_PORT_RRZO_R1_OFFSET, rrzo_secure_port.Raw);
		port_sec_enable[dma_port] = bSecure;

		if (ret != TZ_RESULT_SUCCESS)
			CALL_FROM_OPS(puts, PFX "SECIO_ACCESS FAILED: rrzo\n");
		else
			CALL_FROM_OPS(puts, PFX "SECIO_ACCESS success: rrzo\n");
	}
		break;
	case CAM_YUVO_R1_A: /* YUVO */
	{
		union REG_CAMDMATOP2_R1_CAMDMATOP2_SECURE_SMI_PORT_YUVO_R1  yuv_secure_port;

		CALL_FROM_OPS(puts, PFX "config_port_array: yuvo\n");
		ret |= SECIO_READ(pkvm_register_tag, SECURE_SMI_PORT_YUVO_R1_OFFSET, &yuv_secure_port.Raw);
		if (bSecure) {
			yuv_secure_port.Bits.CAMDMATOP2_YUVO_R1_GDOMAIN  = 0x7;
			yuv_secure_port.Bits.CAMDMATOP2_YUVBO_R1_GDOMAIN = 0x7;
			yuv_secure_port.Bits.CAMDMATOP2_YUVCO_R1_GDOMAIN = 0x7;
			yuv_secure_port.Bits.CAMDMATOP2_YUVO_R1_GSECURE  = 1;
			yuv_secure_port.Bits.CAMDMATOP2_YUVBO_R1_GSECURE = 1;
			yuv_secure_port.Bits.CAMDMATOP2_YUVCO_R1_GSECURE = 1;
		} else {
			yuv_secure_port.Bits.CAMDMATOP2_YUVO_R1_GDOMAIN  = 0;
			yuv_secure_port.Bits.CAMDMATOP2_YUVBO_R1_GDOMAIN = 0;
			yuv_secure_port.Bits.CAMDMATOP2_YUVCO_R1_GDOMAIN = 0;
			yuv_secure_port.Bits.CAMDMATOP2_YUVO_R1_GSECURE  = 0;
			yuv_secure_port.Bits.CAMDMATOP2_YUVBO_R1_GSECURE = 0;
			yuv_secure_port.Bits.CAMDMATOP2_YUVCO_R1_GSECURE = 0;
		}
		ret |= SECIO_WRITE(pkvm_register_tag, SECURE_SMI_PORT_YUVO_R1_OFFSET, yuv_secure_port.Raw);
		port_sec_enable[dma_port] = bSecure;

		if (ret != TZ_RESULT_SUCCESS)
			CALL_FROM_OPS(puts, PFX "SECIO_ACCESS FAILED: yuvo\n");
		else
			CALL_FROM_OPS(puts, PFX "SECIO_ACCESS success: yuvo\n");
		break;
	}
	case CAM_CRZO_R1_A: /* CRZO */
	{
		union REG_CAMDMATOP2_R1_CAMDMATOP2_SECURE_SMI_PORT_CRZO_R1  crzo_secure_port;

		CALL_FROM_OPS(puts, PFX "config_port_array: crzo\n");
		ret |= SECIO_READ(pkvm_register_tag, SECURE_SMI_PORT_CRZO_R1_OFFSET, &crzo_secure_port.Raw);
		if (bSecure) {
			crzo_secure_port.Bits.CAMDMATOP2_CRZO_R1_GDOMAIN  = 0x7;
			crzo_secure_port.Bits.CAMDMATOP2_CRZO_R1_GSECURE  = 1;
			crzo_secure_port.Bits.CAMDMATOP2_CRZBO_R1_GDOMAIN = 0x7;
			crzo_secure_port.Bits.CAMDMATOP2_CRZBO_R1_GSECURE = 1;
			crzo_secure_port.Bits.CAMDMATOP2_CRZO_R2_GDOMAIN  = 0x7;
			crzo_secure_port.Bits.CAMDMATOP2_CRZO_R2_GSECURE  = 1;
		} else {
			crzo_secure_port.Bits.CAMDMATOP2_CRZO_R1_GDOMAIN  = 0;
			crzo_secure_port.Bits.CAMDMATOP2_CRZO_R1_GSECURE  = 0;
			crzo_secure_port.Bits.CAMDMATOP2_CRZBO_R1_GDOMAIN = 0;
			crzo_secure_port.Bits.CAMDMATOP2_CRZBO_R1_GSECURE = 0;
			crzo_secure_port.Bits.CAMDMATOP2_CRZO_R2_GDOMAIN  = 0;
			crzo_secure_port.Bits.CAMDMATOP2_CRZO_R2_GSECURE  = 0;
		}
		ret |= SECIO_WRITE(pkvm_register_tag, SECURE_SMI_PORT_CRZO_R1_OFFSET, crzo_secure_port.Raw);
		port_sec_enable[dma_port] = bSecure;

		if (ret != TZ_RESULT_SUCCESS)
			CALL_FROM_OPS(puts, PFX "SECIO_ACCESS FAILED: crzo\n");
		else
			CALL_FROM_OPS(puts, PFX "SECIO_ACCESS success: crzo\n");
		break;
	}
	case CAM_UFDI_R2_A: /* UFDI_R2 */
	{
		union REG_CAMDMATOP2_R1_CAMDMATOP2_SECURE_SMI_PORT_UFDI_R2  ufdi_secure_port;

		CALL_FROM_OPS(puts, PFX "config_port_array: ufdi_r2\n");
		ret |= SECIO_READ(pkvm_register_tag, SECURE_SMI_PORT_UFDI_R2_OFFSET, &ufdi_secure_port.Raw);
		if (bSecure) {
			ufdi_secure_port.Bits.CAMDMATOP2_UFDI_R2_GDOMAIN = 0x7;
			ufdi_secure_port.Bits.CAMDMATOP2_UFDI_R2_GSECURE = 1;
		} else {
			ufdi_secure_port.Bits.CAMDMATOP2_UFDI_R2_GDOMAIN = 0;
			ufdi_secure_port.Bits.CAMDMATOP2_UFDI_R2_GSECURE = 0;
		}
		ret |= SECIO_WRITE(pkvm_register_tag, SECURE_SMI_PORT_UFDI_R2_OFFSET, ufdi_secure_port.Raw);
		port_sec_enable[dma_port] = bSecure;

		if (ret != TZ_RESULT_SUCCESS)
			CALL_FROM_OPS(puts, PFX "SECIO_ACCESS FAILED: ufdi_r2\n");
		else
			CALL_FROM_OPS(puts, PFX "SECIO_ACCESS success: ufdi_r2\n");
		break;
	}
	case CAM_RAWI_R2_A: /* RAWI_R2 */
	{
		union REG_CAMDMATOP2_R1_CAMDMATOP2_SECURE_SMI_PORT_RAWI_R2  rawi_r2_secure_port;

		CALL_FROM_OPS(puts, PFX "config_port_array: rawi_r2\n");
		ret |= SECIO_READ(pkvm_register_tag, SECURE_SMI_PORT_RAWI_R2_OFFSET, &rawi_r2_secure_port.Raw);
		if (bSecure) {
			rawi_r2_secure_port.Bits.CAMDMATOP2_RAWI_R2_GDOMAIN = 0x7;
			rawi_r2_secure_port.Bits.CAMDMATOP2_RAWI_R2_GSECURE = 1;
		} else {
			rawi_r2_secure_port.Bits.CAMDMATOP2_RAWI_R2_GDOMAIN = 0;
			rawi_r2_secure_port.Bits.CAMDMATOP2_RAWI_R2_GSECURE = 0;
		}
		ret |= SECIO_WRITE(pkvm_register_tag, SECURE_SMI_PORT_RAWI_R2_OFFSET, rawi_r2_secure_port.Raw);
		port_sec_enable[dma_port] = bSecure;

		if (ret != TZ_RESULT_SUCCESS)
			CALL_FROM_OPS(puts, PFX "SECIO_ACCESS FAILED: rawi_r2\n");
		else
			CALL_FROM_OPS(puts, PFX "SECIO_ACCESS success: rawi_r2\n");
		break;
	}
	case CAM_RAWI_R3_A: /* RAWI_R3 */
	{
		union REG_CAMDMATOP2_R1_CAMDMATOP2_SECURE_SMI_PORT_RAWI_R3  rawi_r3_secure_port;

		CALL_FROM_OPS(puts, PFX "config_port_array: rawi_r3\n");
		ret |= SECIO_READ(pkvm_register_tag, SECURE_SMI_PORT_RAWI_R3_OFFSET, &rawi_r3_secure_port.Raw);
		if (bSecure) {
			rawi_r3_secure_port.Bits.CAMDMATOP2_RAWI_R3_GDOMAIN = 0x7;
			rawi_r3_secure_port.Bits.CAMDMATOP2_RAWI_R3_GSECURE = 1;
		} else {
			rawi_r3_secure_port.Bits.CAMDMATOP2_RAWI_R3_GDOMAIN = 0;
			rawi_r3_secure_port.Bits.CAMDMATOP2_RAWI_R3_GSECURE = 0;
		}
		ret |= SECIO_WRITE(pkvm_register_tag, SECURE_SMI_PORT_RAWI_R3_OFFSET, rawi_r3_secure_port.Raw);
		port_sec_enable[dma_port] = bSecure;

		if (ret != TZ_RESULT_SUCCESS)
			CALL_FROM_OPS(puts, PFX "SECIO_ACCESS FAILED: rawi_r3\n");
		else
			CALL_FROM_OPS(puts, PFX "SECIO_ACCESS success: rawi_r3\n");
		break;
	}
	case CAM_RSSO_R1_A: /* RSSO */
	{
		union REG_CAMDMATOP2_R1_CAMDMATOP2_SECURE_SMI_PORT_RSSO_R1  rsso_secure_port;

		CALL_FROM_OPS(puts, PFX "config_port_array: rsso\n");
		ret |= SECIO_READ(pkvm_register_tag, SECURE_SMI_PORT_RSSO_R1_OFFSET, &rsso_secure_port.Raw);
		if (bSecure) {
			rsso_secure_port.Bits.CAMDMATOP2_RSSO_R1_GDOMAIN = 0x7;
			rsso_secure_port.Bits.CAMDMATOP2_RSSO_R1_GSECURE = 1;
			rsso_secure_port.Bits.CAMDMATOP2_RSSO_R2_GDOMAIN = 0x7;
			rsso_secure_port.Bits.CAMDMATOP2_RSSO_R2_GSECURE = 1;

		} else {
			rsso_secure_port.Bits.CAMDMATOP2_RSSO_R1_GDOMAIN = 0;
			rsso_secure_port.Bits.CAMDMATOP2_RSSO_R1_GSECURE = 0;
			rsso_secure_port.Bits.CAMDMATOP2_RSSO_R2_GDOMAIN = 0;
			rsso_secure_port.Bits.CAMDMATOP2_RSSO_R2_GSECURE = 0;
		}
		ret |= SECIO_WRITE(pkvm_register_tag, SECURE_SMI_PORT_RSSO_R1_OFFSET, rsso_secure_port.Raw);
		port_sec_enable[dma_port] = bSecure;

		if (ret != TZ_RESULT_SUCCESS)
			CALL_FROM_OPS(puts, PFX "SECIO_ACCESS FAILED: rsso\n");
		else
			CALL_FROM_OPS(puts, PFX "SECIO_ACCESS success: rsso\n");
		break;
	}
	default:
		CALL_FROM_OPS(puts, PFX "portID NOT Supported!!\n");
		CALL_FROM_OPS(putx64, dma_port);
		break;
	}

	CALL_FROM_OPS(puts, PFX "config_port_array success\n");

	return ret;
}



/*******************************************************************************
 * pkvm el2 APIs
 ******************************************************************************/
void pkvm_p1_hyp_sec_config(struct user_pt_regs *regs)
{
	uint32_t port;
	TZ_RESULT ret = TZ_RESULT_SUCCESS;

	if (!regs) {
		CALL_FROM_OPS(puts, PFX "ERROR: regs is NULL\n");
		return;
	}
	port = regs->regs[1];

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "++ port=");
	CALL_FROM_OPS(putx64, port);

	if (port == CAMA_IMGO || port == CAMB_IMGO) {
		ret += Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, CAMA_IMGO);
		ret += Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, CAMB_IMGO);
	} else if (port == CAMA_RRZO || port == CAMB_RRZO) {
		ret += Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, CAMA_RRZO);
		ret += Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, CAMB_RRZO);
	} else {
		ret = Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, port);
	}

	if (ret != TZ_RESULT_SUCCESS) {
		CALL_FROM_OPS(puts, PFX "ERROR: sec_config FAILED\n");
		regs->regs[1] = false;
	} else {
		CALL_FROM_OPS(puts, PFX "sec_config success\n");
		regs->regs[1] = true;
	}

	regs->regs[0] = SMCCC_RET_SUCCESS;
}


void pkvm_p1_hyp_set_sec_cam(struct user_pt_regs *regs)
{
	uint32_t CamModule;
	TZ_RESULT ret = TZ_RESULT_SUCCESS;

	if (!regs) {
		CALL_FROM_OPS(puts, PFX "ERROR: regs is NULL\n");
		return;
	}
	CamModule = regs->regs[1];

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "++ CamModule=");
	CALL_FROM_OPS(putx64, CamModule);

	if (CamModule == TG_A) {
		ret += Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, CAMA_IMGO);
		ret += Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, CAMB_IMGO);
		ret += Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, CAMA_RRZO);
		ret += Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, CAMB_RRZO);
		ret += Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, CAMA_UFDI);
		ret += Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, CAMA_RAWI);
		ret += Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, CAMA_RAWI + 1);
		ret += Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, CAMA_RSSO);
		if (ret != TZ_RESULT_SUCCESS)
			CALL_FROM_OPS(puts, PFX "ERROR: CAMA Cam_Pkvm_Config_DMA_Sec FAILED\n");
		else
			CALL_FROM_OPS(puts, PFX "CAMA Cam_Pkvm_Config_DMA_Sec success\n");
	} else if (CamModule == TG_B) {
		ret += Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, CAMA_IMGO);
		ret += Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, CAMB_IMGO);
		ret += Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, CAMA_RRZO);
		ret += Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, CAMB_RRZO);
		ret += Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, CAMB_UFDI);
		ret += Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, CAMB_RAWI);
		ret += Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, CAMB_RAWI + 1);
		ret += Cam_Pkvm_Config_DMA_Sec(1, 1, MM_DOMAIN, CAMB_RSSO);
		if (ret != TZ_RESULT_SUCCESS)
			CALL_FROM_OPS(puts, PFX "ERROR: CAMB Cam_Pkvm_Config_DMA_Sec FAILED\n");
		else
			CALL_FROM_OPS(puts, PFX "CAMB Cam_Pkvm_Config_DMA_Sec success\n");
	} else {
		CALL_FROM_OPS(puts, PFX "ERROR: Not supported CamModule!!\n");
	}

	if (ret != TZ_RESULT_SUCCESS) {
		CALL_FROM_OPS(puts, PFX "ERROR: set_sec_cam FAILED\n");
		regs->regs[1] = false;
	} else {
		CALL_FROM_OPS(puts, PFX "set_sec_cam success\n");
		regs->regs[1] = true;
	}

	regs->regs[0] = SMCCC_RET_SUCCESS;
}


void pkvm_p1_hyp_set_dapc_auth(struct user_pt_regs *regs)
{
	uint32_t CamModule;
	union REG_CAMCTL_R1_CAMCTL_DMA_EN reg_dma_en;
	union REG_CAMCTL_R1_CAMCTL_DMA2_EN reg_dma2_en;
	union REG_CAMCTL_R1_CAMCTL_SEL reg_sel;
	union REG_CAMCTL_R1_CAMCTL_LCES_OUT_SIZE reg_lces_size;
	uint32_t crzo_r2_en;

	if (!regs) {
		CALL_FROM_OPS(puts, PFX "ERROR: regs is NULL\n");
		return;
	}
	CamModule  = regs->regs[1];
	reg_dma_en.Raw    = regs->regs[2];
	reg_dma2_en.Raw   = regs->regs[3];
	reg_sel.Raw       = regs->regs[4];
	reg_lces_size.Raw = regs->regs[5];
	crzo_r2_en = (CamModule == TG_A) ? port_sec_enable[CAM_CRZO_R1_A] :
				(CamModule == TG_B) ? port_sec_enable[CAM_CRZO_R1_B] :
				0;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(putx64, CamModule);
	CALL_FROM_OPS(putx64, reg_dma_en.Raw);
	CALL_FROM_OPS(putx64, reg_dma2_en.Raw);
	CALL_FROM_OPS(putx64, reg_sel.Raw);
	CALL_FROM_OPS(putx64, reg_lces_size.Raw);
	CALL_FROM_OPS(putx64, crzo_r2_en);

	if (!reg_dma_en.Bits.CAMCTL_IMGO_R1_EN  ||
		!reg_dma_en.Bits.CAMCTL_RRZO_R1_EN  ||
		!reg_dma_en.Bits.CAMCTL_YUVO_R1_EN  ||
		!reg_dma_en.Bits.CAMCTL_YUVBO_R1_EN ||
		reg_dma_en.Bits.CAMCTL_RSSO_R1_EN   ||
		reg_dma_en.Bits.CAMCTL_RSSO_R2_EN   ||
		reg_dma_en.Bits.CAMCTL_CRZBO_R1_EN  ||
		(reg_dma_en.Bits.CAMCTL_CRZO_R2_EN ^ crzo_r2_en)) {
		CALL_FROM_OPS(puts, PFX "REG_CAMCTL_R1_CAMCTL_DMA_EN auth failed");
		regs->regs[1] = false;
	} else if (reg_dma2_en.Bits.CAMCTL_RAWI_R2_EN ||
		reg_dma2_en.Bits.CAMCTL_RAWI_R3_EN ||
		reg_dma2_en.Bits.CAMCTL_UFDI_R2_EN) {
		CALL_FROM_OPS(puts, PFX "REG_CAMCTL_R1_CAMCTL_DMA2_EN auth failed");
		regs->regs[1] = false;
	} else if (reg_sel.Bits.CAMCTL_RAW_SEL ||
		reg_sel.Bits.CAMCTL_CRP_R3_SEL ||
		(reg_sel.Bits.CAMCTL_IMGO_SEL != 0 && reg_sel.Bits.CAMCTL_IMGO_SEL != 2)) {
		CALL_FROM_OPS(puts, PFX "REG_CAMCTL_R1_CAMCTL_SEL auth failed");
		regs->regs[1] = false;
	} else if ((reg_lces_size.Bits.LCES_OUT_HT >=  LCES_OUT_HT_LIMIT) ||
		(reg_lces_size.Bits.LCES_OUT_WD >= LCES_OUT_WD_LIMIT)) {
		CALL_FROM_OPS(puts, PFX "REG_CAMCTL_R1_CAMCTL_LCES_OUT_SIZE auth failed");
		regs->regs[1] = false;
	} else {
		regs->regs[1] = true;
	}

	regs->regs[0] = SMCCC_RET_SUCCESS;
}


void pkvm_p1_hyp_set_dapc_reg(struct user_pt_regs *regs)
{
	uint32_t CamModule;
	uint32_t index;
	uint32_t dapc_val;
	uint32_t regBase, offset;
	uint32_t secure_reg = 0;
	TZ_RESULT ret = TZ_RESULT_SUCCESS;

	if (!regs) {
		CALL_FROM_OPS(puts, PFX "ERROR: regs is NULL\n");
		return;
	}
	CamModule = regs->regs[1];
	index = regs->regs[2];
	dapc_val = regs->regs[3];

	if (index == 0) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "++ CamModule=");
		CALL_FROM_OPS(putx64, CamModule);
	}

	if (CamModule == TG_A) {
		regBase = ISP_DRV_REG_BASE_A;
		offset = gisp_drv_reg_addr[index];
		CALL_FROM_OPS(puts, "-------------------------");
		CALL_FROM_OPS(putx64, offset);
		CALL_FROM_OPS(putx64, dapc_val);
		CALL_FROM_OPS(puts, "-------------------------");
		ret |= SECIO_WRITE(SECIO_ISP_CAM_A, offset, dapc_val);
	} else if (CamModule == TG_B) {
		regBase = ISP_DRV_REG_BASE_B;
		offset = gisp_drv_reg_addr[index];
		ret |= SECIO_WRITE(SECIO_ISP_CAM_B, offset, dapc_val);
	} else {
		CALL_FROM_OPS(puts, PFX "ERROR: Not supported CamModule!!\n");
	}

	/* Move write secure_en bit here for CCU load code */
	ret |= SECIO_READ(SECIO_ISP_CAM_SYS, CAM_CAMSYS_SECURE, &secure_reg);
	secure_reg |= 0x1;
	ret |= SECIO_WRITE(SECIO_ISP_CAM_SYS, CAM_CAMSYS_SECURE, secure_reg);

	if (ret != TZ_RESULT_SUCCESS) {
		CALL_FROM_OPS(puts, PFX "ERROR: set_dapc_reg FAILED\n");
		regs->regs[1] = false;
	} else {
		CALL_FROM_OPS(puts, PFX "set_dapc_reg success\n");
		regs->regs[1] = true;
	}

	regs->regs[0] = SMCCC_RET_SUCCESS;
}


void pkvm_p1_hyp_APC_CamIspProtCtl(struct user_pt_regs *regs)
{
	uint32_t CamModule;
	bool isEnable;

	if (!regs) {
		CALL_FROM_OPS(puts, PFX "ERROR: regs is NULL\n");
		return;
	}
	CamModule = regs->regs[1];
	isEnable = regs->regs[2];

	if (isEnable) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "Enable APC: CamModule=");
		CALL_FROM_OPS(putx64, CamModule);

		if (CamModule == TG_A)
			APC_CamIspProtEnable(1);
		else if (CamModule == TG_B)
			APC_CamIspProtEnable(2);
		else
			CALL_FROM_OPS(puts, PFX "Not supported CamModule!");
	} else {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "Disable APC: CamModule=");
		CALL_FROM_OPS(putx64, CamModule);

		if (CamModule == TG_A)
			APC_CamIspProtDisable(1);
		else if (CamModule == TG_B)
			APC_CamIspProtDisable(2);
		else
			CALL_FROM_OPS(puts, PFX "Not supported CamModule!");
	}

	regs->regs[1] = true;
	regs->regs[0] = SMCCC_RET_SUCCESS;
}


void pkvm_p1_hyp_get_sec_fh_info(struct user_pt_regs *regs)
{
	uint64_t sec_pa;
	uint32_t index;
	void *pfixmap = NULL;
	uint32_t *sec_fh;
	uint32_t cur_sec_fh;

	if (!regs) {
		CALL_FROM_OPS(puts, PFX "ERROR: regs is NULL\n");
		return;
	}
	sec_pa = regs->regs[1];
	index = regs->regs[2];

	if (index == 0)
		CALL_FROM_OPS(puts, PFX "get_sec_fh_info +++\n");

	pfixmap = CALL_FROM_OPS(fixmap_map, sec_pa);
	sec_fh = (uint32_t *)pfixmap;
	cur_sec_fh = sec_fh[index];
	sec_fh[index] = 0;

	CALL_FROM_OPS(putx64, cur_sec_fh);
	CALL_FROM_OPS(fixmap_unmap);

	regs->regs[1] = cur_sec_fh;
	regs->regs[0] = SMCCC_RET_SUCCESS;
}


void pkvm_p1_hyp_uninit(struct user_pt_regs *regs)
{
	uint32_t secure_reg = 0;
	TZ_RESULT ret = TZ_RESULT_SUCCESS;

	CALL_FROM_OPS(puts, __func__);

	/* Disable secure for all dma ports */
	CALL_FROM_OPS(puts, PFX "disable secure for all dma ports");
	for (int i = 0; i < CAM_DMA_PORT_MAX; i++) {
		if (port_sec_enable[i]) {
			ret |= Cam_Pkvm_Config_DMA_Sec(1, 0, 0, i);
			port_sec_enable[i] = 0;
		}
	}

	/* Disable secure: CAMSYS_SECURE */
	ret |= SECIO_READ(SECIO_ISP_CAM_SYS, CAM_CAMSYS_SECURE, &secure_reg);
	secure_reg |= 0x0;
	ret |= SECIO_WRITE(SECIO_ISP_CAM_SYS, CAM_CAMSYS_SECURE, secure_reg);

	/* Disable DAPC for all cams */
	for (int i = 0; i < CAM_NUM; i++)
		APC_CamIspProtDisable(i);

	if (ret != TZ_RESULT_SUCCESS) {
		CALL_FROM_OPS(puts, PFX "ERROR: uninit FAILED\n");
		regs->regs[1] = false;
	} else {
		CALL_FROM_OPS(puts, PFX "uninit success\n");
		regs->regs[1] = true;
	}

	regs->regs[0] = SMCCC_RET_SUCCESS;
}


/*******************************************************************************
 * pkvm init function
 ******************************************************************************/
int p1_hyp_init(const struct pkvm_module_ops *ops)
{
	pkvm_p1_ops = ops;
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "success");
	return 0;
}
