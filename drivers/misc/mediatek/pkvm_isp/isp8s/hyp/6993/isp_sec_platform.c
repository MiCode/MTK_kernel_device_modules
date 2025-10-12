// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#include "pkvm_isp_hyp.h"
#include "isp_sec_api.h"
#include "isp_sec_platform.h"
#include <pkvm_sys.h>
#include <pkvm_trustzone.h>

uint64_t camsys_sec_readio(int tag, uint32_t addr)
{
	int ret = TZ_RESULT_SUCCESS, val = 0;

	SECIO_READ(tag, addr, &val);

	if(ret != TZ_RESULT_SUCCESS){
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "SECIO_READ fail, ret = ");

		CALL_FROM_OPS(putx64, ret);
		CALL_FROM_OPS(puts, "tag:");
		CALL_FROM_OPS(putx64, ret);
		CALL_FROM_OPS(puts, "addr:");
		CALL_FROM_OPS(putx64, addr);
		CALL_FROM_OPS(puts, "val:");
		CALL_FROM_OPS(putx64, val);
	}

	return val;
}

TZ_RESULT camsys_sec_writeio(int tag, uint32_t addr, uint64_t val)
{
	TZ_RESULT ret = TZ_RESULT_SUCCESS;

	ret = SECIO_WRITE(tag, addr, val);

	if(ret != TZ_RESULT_SUCCESS){
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "SECIO_WRITE fail, ret = ");
		CALL_FROM_OPS(putx64, ret);
		CALL_FROM_OPS(puts, "tag:");
		CALL_FROM_OPS(putx64, ret);
		CALL_FROM_OPS(puts, "addr:");
		CALL_FROM_OPS(putx64, addr);
		CALL_FROM_OPS(puts, "val:");
		CALL_FROM_OPS(putx64, val);
	}
	return ret;
}

int register_dump(int cam, int camsv)
{
	int SECIO_CAM = SECIO_ISP_CAM_A;
	int SECIO_RMS = SECIO_ISP_RMS_CAM_A;
	int SECIO_YUV = SECIO_ISP_YUV_CAM_A;
	int SECIO_CAM_CTL = SECIO_ISP_CAMCTL_CAM_A;
	int SECIO_CAM_CTL2 = SECIO_ISP_CAMCTL2_CAM_A;
	int SECIO_CAM_CTL3 = SECIO_ISP_CAMCTL3_CAM_A;
	int SECIO_CAMSV = SECIO_ISP_CAMSV_A;
	int j = 0;

	CALL_FROM_OPS(puts, PFX);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, "+ cam_id:");
	CALL_FROM_OPS(putx64, cam);
	CALL_FROM_OPS(puts, "camsv_id:");
	CALL_FROM_OPS(putx64, camsv);

	if(cam){
		switch (cam - 1) {
		case CAMA:
			SECIO_CAM = SECIO_ISP_CAM_A;
			SECIO_RMS = SECIO_ISP_RMS_CAM_A;
			SECIO_YUV = SECIO_ISP_YUV_CAM_A;
			SECIO_CAM_CTL = SECIO_ISP_CAMCTL_CAM_A;
			SECIO_CAM_CTL2 = SECIO_ISP_CAMCTL2_CAM_A;
			SECIO_CAM_CTL3 = SECIO_ISP_CAMCTL3_CAM_A;
			break;
		case CAMB:
			SECIO_CAM = SECIO_ISP_CAM_B;
			SECIO_RMS = SECIO_ISP_RMS_CAM_B;
			SECIO_YUV = SECIO_ISP_YUV_CAM_B;
			SECIO_CAM_CTL = SECIO_ISP_CAMCTL_CAM_B;
			SECIO_CAM_CTL2 = SECIO_ISP_CAMCTL2_CAM_B;
			SECIO_CAM_CTL3 = SECIO_ISP_CAMCTL3_CAM_B;
			break;
		case CAMC:
			SECIO_CAM = SECIO_ISP_CAM_C;
			SECIO_RMS = SECIO_ISP_RMS_CAM_C;
			SECIO_YUV = SECIO_ISP_YUV_CAM_C;
			SECIO_CAM_CTL = SECIO_ISP_CAMCTL_CAM_C;
			SECIO_CAM_CTL2 = SECIO_ISP_CAMCTL2_CAM_C;
			SECIO_CAM_CTL3 = SECIO_ISP_CAMCTL3_CAM_C;
			break;
		default:
			CALL_FROM_OPS(puts, "cam invalid! cam: ");
			CALL_FROM_OPS(putx64, cam);
			return -1;
		}

		CALL_FROM_OPS(puts, "CID camctl/camctl2/camctl3 ");
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_CAM, CAMCTL_ROOT_CID));
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_YUV, CAMCTL2_ROOT_CID));
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_RMS, CAMCTL3_ROOT_CID));
		CALL_FROM_OPS(puts, "STAT_PROT_EN camctl/camctl2/camctl3 ");
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_CAM, CAMCTL_ROOT_STAT_PROT_EN));
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_YUV, CAMCTL2_ROOT_STAT_PROT_EN));
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_RMS, CAMCTL3_ROOT_STAT_PROT_EN));
		CALL_FROM_OPS(puts, "RAWDMA SECURE CTRL ");
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_CAM, CAMRAWDMA_ROOT_SECURE_CTRL));
		CALL_FROM_OPS(puts, "RAWDMA SECURE_0/1 ");
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_CAM, CAMRAWDMA_ROOT_SECURE_REGISTER_0));
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_CAM, CAMRAWDMA_ROOT_SECURE_REGISTER_1));
		CALL_FROM_OPS(puts, "RAWDMA DOMAIN 2/3/6/9 = ");
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_CAM, CAMRAWDMA_ROOT_DOMAIN_REGISTER_2));
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_CAM, CAMRAWDMA_ROOT_DOMAIN_REGISTER_3));
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_CAM, CAMRAWDMA_ROOT_DOMAIN_REGISTER_6));
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_CAM, CAMRAWDMA_ROOT_DOMAIN_REGISTER_9));
		CALL_FROM_OPS(puts, "YUVDMA SECURE CTRL ");
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_YUV, CAMYUVDMA_ROOT_SECURE_CTRL));
		CALL_FROM_OPS(puts, "YUVDMA SECURE_0 ");
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_YUV, CAMYUVDMA_ROOT_SECURE_REGISTER_0));

		CALL_FROM_OPS(puts, "YUVDMA DOMAIN 0/1/2/3/4 = ");
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_YUV, CAMYUVDMA_ROOT_DOMAIN_REGISTER_0));
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_YUV, CAMYUVDMA_ROOT_DOMAIN_REGISTER_1));
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_YUV, CAMYUVDMA_ROOT_DOMAIN_REGISTER_2));
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_YUV, CAMYUVDMA_ROOT_DOMAIN_REGISTER_3));
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_YUV, CAMYUVDMA_ROOT_DOMAIN_REGISTER_4));
		CALL_FROM_OPS(puts, "RAW_SEL_SECURE_LOCK = ");
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_CAM, CAMCTL_ROOT_RAW_SEL_SECURE_LOCK));
		CALL_FROM_OPS(puts, "DL_EN ");
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_CAM_CTL, CAMCTL_TIF_DL_EN));
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_CAM_CTL, CAMCTL_OTR_DL_EN));
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_CAM_CTL3, CAMCTL3_TIF_DL_EN));
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_CAM_CTL2, CAMCTL2_TIF_DL_EN));
	}
	// Camsv
	if(camsv){
		switch (camsv - 1) {
		case CAMA:
			SECIO_CAMSV = SECIO_ISP_CAMSV_A;
			break;
		case CAMB:
			SECIO_CAMSV = SECIO_ISP_CAMSV_B;
			break;
		case CAMC:
			SECIO_CAMSV = SECIO_ISP_CAMSV_C;
			break;
		default:
			CALL_FROM_OPS(puts, "camsv invalid! camsv: ");
			CALL_FROM_OPS(putx64, camsv);
			return -1;
		}

		CALL_FROM_OPS(puts, "CAMSVCENTRAL_CID_CHK_SENINF ");
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_CAMSV, CAMSVCENTRAL_CID_CHK_SENINF));
		CALL_FROM_OPS(puts, "CAMSVCENTRAL_CID_CHK_RAW_DCIF ");
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_CAMSV, CAMSVCENTRAL_CID_CHK_RAW_DCIF));
		CALL_FROM_OPS(puts, "CAMSVCENTRAL_ROOT_CAMSV_CID ");
		CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_CAMSV, CAMSVCENTRAL_ROOT_CAMSV_CID));

		for (j = 0; j < 0x1C; j += 0x10) {
			CALL_FROM_OPS(puts, "CAMSV_DMA_SHIFT_");
			CALL_FROM_OPS(putx64, 0x0 + j);
			CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(
				SECIO_CAMSV, CAMSV_DMA_SHIFT + j));
			CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(
				SECIO_CAMSV, CAMSV_DMA_SHIFT + (j + 0x4)));
			CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(
				SECIO_CAMSV, CAMSV_DMA_SHIFT + (j + 0x8)));
			CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(
				SECIO_CAMSV, CAMSV_DMA_SHIFT + (j + 0xC)));
		}
		for (j = 0; j < 0x1C; j += 0x10) {
			CALL_FROM_OPS(puts, "CAMSV_DMA_SHIFT_");
			CALL_FROM_OPS(putx64, 0x40 + (j * 4));
			CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(
				SECIO_CAMSV, CAMSV_DMA_SHIFT + 0x40 + j));
			CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(
				SECIO_CAMSV, CAMSV_DMA_SHIFT + 0x40 + (j + 0x4)));
			CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(
				SECIO_CAMSV, CAMSV_DMA_SHIFT + 0x40 + (j + 0x8)));
			CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(
				SECIO_CAMSV, CAMSV_DMA_SHIFT + 0x40 + (j + 0xC)));
		}
	}
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, "-");

	return 0;
}

int isp_sec_streamOn_platform(int without_tg, int CamModule)
{
	int SECIO_CAM = SECIO_ISP_CAM_A;
	int ret = 0;

	CALL_FROM_OPS(puts, PFX "+ without_tg:");
	CALL_FROM_OPS(putx64, without_tg);
	CALL_FROM_OPS(puts, ",CamModule:");
	CALL_FROM_OPS(putx64, CamModule);

	REG_R_CAMCTL_ROOT_RAW_SEL_SECURE_LOCK  hsf_raw_sel_lock;
	//fill raw_sel secure lock
	if(without_tg) {
		//only accept data from rawi_r5
		hsf_raw_sel_lock.Raw = 0x41;
	} else {
		//only accept data from tg
		hsf_raw_sel_lock.Raw = 0x1;
	}
	switch (CamModule) {
	case CAMA:
		SECIO_CAM = SECIO_ISP_CAM_A;
		break;
	case CAMB:
		SECIO_CAM = SECIO_ISP_CAM_B;
		break;
	case CAMC:
		SECIO_CAM = SECIO_ISP_CAM_C;
		break;
	default:
		CALL_FROM_OPS(puts, "CamModule invalid! CamModule: ");
		CALL_FROM_OPS(putx64, CamModule);
		return -1;
	}

	//set correct raw_sel secure lock
	ret |= camsys_sec_writeio(SECIO_CAM, CAMCTL_ROOT_RAW_SEL_SECURE_LOCK, hsf_raw_sel_lock.Raw);
	CALL_FROM_OPS(puts, "RAW_SEL_LOCK:");
	CALL_FROM_OPS(putx64, (unsigned int)camsys_sec_readio(SECIO_CAM, CAMCTL_ROOT_RAW_SEL_SECURE_LOCK));
	CALL_FROM_OPS(puts, ret ? "Fail" : "Success");
	CALL_FROM_OPS(putx64, ret);

	return ret;
}

int isp_sec_configCam_platform(int bSecure, int CamModule)
{
	int SECIO_CAM = SECIO_ISP_CAM_A;
	int SECIO_RMS = SECIO_ISP_RMS_CAM_A;
	int SECIO_YUV = SECIO_ISP_YUV_CAM_A;
	int SECIO_CAM_CTL = SECIO_ISP_CAMCTL_CAM_A;
	int SECIO_CAM_CTL2 = SECIO_ISP_CAMCTL2_CAM_A;
	int SECIO_CAM_CTL3 = SECIO_ISP_CAMCTL3_CAM_A;
	int ret = 0, i = 0;

	CALL_FROM_OPS(puts, "+ Secure:");
	CALL_FROM_OPS(putx64, bSecure);
	CALL_FROM_OPS(puts, ",CamModule:");
	CALL_FROM_OPS(putx64, CamModule);

	REG_R_CAMCTL_ROOT_CID                  hsf_camctl_cid;
	REG_R_CAMCTL3_ROOT_CID                 hsf_camctl3_cid;
	REG_R_CAMCTL2_ROOT_CID                 hsf_camctl2_cid;
	REG_R_CAMCTL_ROOT_STAT_PROT_EN         hsf_camctl_prot;
	REG_R_CAMCTL3_ROOT_STAT_PROT_EN        hsf_camctl3_prot;
	REG_R_CAMCTL2_ROOT_STAT_PROT_EN        hsf_camctl2_prot;
	REG_R_CAMCTL_ROOT_RAW_SEL_SECURE_LOCK  hsf_raw_sel_lock;
	REG_R_CAMRAWDMA_ROOT_SECURE_REGISTER_0 hsf_sec_0;
	REG_R_CAMRAWDMA_ROOT_SECURE_REGISTER_1 hsf_sec_1;
	REG_R_CAMYUVDMA_ROOT_SECURE_REGISTER_0 hsf_yuv_sec_0;
	REG_R_CAMRAWDMA_ROOT_SECURE_CTRL       hsf_raw_sec_ctrl;
	REG_R_CAMYUVDMA_ROOT_SECURE_CTRL       hsf_yuv_sec_ctrl;
	REG_R_CAMRAWDMA_ROOT_DOMAIN_REGISTER_2 hsf_dom_2;
	REG_R_CAMRAWDMA_ROOT_DOMAIN_REGISTER_3 hsf_dom_3;
	REG_R_CAMRAWDMA_ROOT_DOMAIN_REGISTER_6 hsf_dom_6;
	REG_R_CAMRAWDMA_ROOT_DOMAIN_REGISTER_9 hsf_dom_9;
	REG_R_CAMYUVDMA_ROOT_DOMAIN_REGISTER_0 hsf_yuv_dom_0;
	REG_R_CAMYUVDMA_ROOT_DOMAIN_REGISTER_1 hsf_yuv_dom_1;
	REG_R_CAMYUVDMA_ROOT_DOMAIN_REGISTER_2 hsf_yuv_dom_2;
	REG_R_CAMYUVDMA_ROOT_DOMAIN_REGISTER_3 hsf_yuv_dom_3;
	REG_R_CAMYUVDMA_ROOT_DOMAIN_REGISTER_4 hsf_yuv_dom_4;
	REG_R_CAMCTL_TIF_DL_EN                 hsf_camctl_tif_dl_en;
	REG_R_CAMCTL_OTR_DL_EN                 hsf_camctl_otr_dl_en;
	REG_R_CAMCTL3_TIF_DL_EN                hsf_camctl3_tif_dl_en;
	REG_R_CAMCTL2_TIF_DL_EN                hsf_camctl2_tif_dl_en;

	for(i = 0; i < SEC_CAM_MAX; i++) {
		if(CamModule & (1 << i)) {
			switch (i){
			case SEC_CAM_A:
				SECIO_CAM = SECIO_ISP_CAM_A;
				SECIO_RMS = SECIO_ISP_RMS_CAM_A;
				SECIO_YUV = SECIO_ISP_YUV_CAM_A;
				SECIO_CAM_CTL = SECIO_ISP_CAMCTL_CAM_A;
				SECIO_CAM_CTL2 = SECIO_ISP_CAMCTL2_CAM_A;
				SECIO_CAM_CTL3 = SECIO_ISP_CAMCTL3_CAM_A;
				break;
			case SEC_CAM_B:
				SECIO_CAM = SECIO_ISP_CAM_B;
				SECIO_RMS = SECIO_ISP_RMS_CAM_B;
				SECIO_YUV = SECIO_ISP_YUV_CAM_B;
				SECIO_CAM_CTL = SECIO_ISP_CAMCTL_CAM_B;
				SECIO_CAM_CTL2 = SECIO_ISP_CAMCTL2_CAM_B;
				SECIO_CAM_CTL3 = SECIO_ISP_CAMCTL3_CAM_B;
				break;
			case SEC_CAM_C:
				SECIO_CAM = SECIO_ISP_CAM_C;
				SECIO_RMS = SECIO_ISP_RMS_CAM_C;
				SECIO_YUV = SECIO_ISP_YUV_CAM_C;
				SECIO_CAM_CTL = SECIO_ISP_CAMCTL_CAM_C;
				SECIO_CAM_CTL2 = SECIO_ISP_CAMCTL2_CAM_C;
				SECIO_CAM_CTL3 = SECIO_ISP_CAMCTL3_CAM_C;
				break;
			default:
				CALL_FROM_OPS(puts, "CamModule invalid! CamModule: ");
				CALL_FROM_OPS(putx64, CamModule);
				return -1;
			}
			if(bSecure){
				//set cid
				hsf_camctl_cid.Raw =        CAMCTL_ROOT_CID_VALUE;
				hsf_camctl3_cid.Raw =       CAMCTL3_ROOT_CID_VALUE;
				hsf_camctl2_cid.Raw =       CAMCTL2_ROOT_CID_VALUE;
				//set statistic dma protetion
				hsf_camctl_prot.Raw =       CAMCTL_ROOT_PROT_VALUE;
				hsf_camctl3_prot.Raw =      CAMCTL3_ROOT_PROT_VALUE;
				hsf_camctl2_prot.Raw =      CAMCTL2_ROOT_PROT_VALUE;
				//set dma secure ctrl (local sid)
				hsf_raw_sec_ctrl.Raw =      RAW_ROOT_SECURE_CTRL_VALUE;
				hsf_yuv_sec_ctrl.Raw =      YUV_ROOT_SECURE_CTRL_VALUE;
				//set gsecure value
				hsf_sec_0.Raw =             RAW_SECURE_VALUE_0;
				hsf_sec_1.Raw =             RAW_SECURE_VALUE_1;
				hsf_yuv_sec_0.Raw =         YUV_SECURE_VALUE_0;
				//set raw gdomain value
				hsf_dom_2.Raw =             RAW_DOMAIN_VALUE_2;
				hsf_dom_3.Raw =             RAW_DOMAIN_VALUE_3;
				hsf_dom_6.Raw =             RAW_DOMAIN_VALUE_6;
				hsf_dom_9.Raw =             RAW_DOMAIN_VALUE_9;
				//set yuv gdomain value
				hsf_yuv_dom_0.Raw =         YUV_DOMAIN_VALUE_0;
				hsf_yuv_dom_1.Raw =         YUV_DOMAIN_VALUE_1;
				hsf_yuv_dom_2.Raw =         YUV_DOMAIN_VALUE_2;
				hsf_yuv_dom_3.Raw =         YUV_DOMAIN_VALUE_3;
				hsf_yuv_dom_4.Raw =         YUV_DOMAIN_VALUE_4;
				//set dl_en
				hsf_camctl_tif_dl_en.Raw =  CAMCTL_TIF_DL_EN_VALUE;
				hsf_camctl_otr_dl_en.Raw =  CAMCTL_OTR_DL_EN_VALUE;
				hsf_camctl3_tif_dl_en.Raw = CAMCTL3_TIF_DL_EN_VALUE;
				hsf_camctl2_tif_dl_en.Raw = CAMCTL2_TIF_DL_EN_VALUE;
				/*
				 * raw_sel secure lock is enabled & set to "only accept data from rawi_r2".
				 * since no data comes from rawi_r2, this can prevent hacker trying to open
				 * vf before seninf is ready. (vf will not be protected by DAPC on 6993)
				 *
				 * The correct raw_sel secure lock will be set in function camsys_stream_on().
				 */
				hsf_raw_sel_lock.Raw = 0x21;
			} else {
				//clear all the setting if hsf disabled
				hsf_camctl_cid.Raw =   0; hsf_camctl3_cid.Raw =  0; hsf_camctl2_cid.Raw =  0;
				hsf_camctl_prot.Raw =  0; hsf_camctl3_prot.Raw = 0; hsf_camctl2_prot.Raw = 0;
				hsf_raw_sec_ctrl.Raw = 0; hsf_yuv_sec_ctrl.Raw = 0;
				hsf_sec_0.Raw =        0; hsf_sec_1.Raw =        0; hsf_yuv_sec_0.Raw =    0;
				hsf_dom_2.Raw =        0; hsf_dom_3.Raw =        0;
				hsf_dom_6.Raw =        0; hsf_dom_9.Raw =        0;
				hsf_yuv_dom_0.Raw =    0; hsf_yuv_dom_1.Raw =    0; hsf_yuv_dom_2.Raw =    0;
				hsf_yuv_dom_3.Raw =    0; hsf_yuv_dom_4.Raw =    0;
				hsf_raw_sel_lock.Raw = 0;
			}
			//DL_EN
		/*******************************************************************************************/
			ret |= camsys_sec_writeio(SECIO_CAM_CTL, CAMCTL_TIF_DL_EN, 0x0);
			ret |= camsys_sec_writeio(SECIO_CAM_CTL, CAMCTL_OTR_DL_EN, 0x0);
			ret |= camsys_sec_writeio(SECIO_CAM_CTL3, CAMCTL3_TIF_DL_EN, 0x0);
			ret |= camsys_sec_writeio(SECIO_CAM_CTL2, CAMCTL2_TIF_DL_EN, 0x0);
		/*******************************************************************************************/
			//CAMCTL_ROOT_CID
		/*******************************************************************************************/
			ret |= camsys_sec_writeio(SECIO_CAM, CAMCTL_ROOT_CID, hsf_camctl_cid.Raw);
			ret |= camsys_sec_writeio(SECIO_RMS, CAMCTL3_ROOT_CID, hsf_camctl3_cid.Raw);
			ret |= camsys_sec_writeio(SECIO_YUV, CAMCTL2_ROOT_CID, hsf_camctl2_cid.Raw);
		/*******************************************************************************************/
			//set statistic dma protetion
		/*******************************************************************************************/
			ret |= camsys_sec_writeio(SECIO_CAM, CAMCTL_ROOT_STAT_PROT_EN, hsf_camctl_prot.Raw);
			ret |= camsys_sec_writeio(SECIO_RMS, CAMCTL3_ROOT_STAT_PROT_EN, hsf_camctl3_prot.Raw);
			ret |= camsys_sec_writeio(SECIO_YUV, CAMCTL2_ROOT_STAT_PROT_EN, hsf_camctl2_prot.Raw);
		/*******************************************************************************************/
			//set dma secure ctrl (local sid)
			ret |= camsys_sec_writeio(SECIO_CAM, CAMRAWDMA_ROOT_SECURE_CTRL, hsf_raw_sec_ctrl.Raw);
			ret |= camsys_sec_writeio(SECIO_YUV, CAMYUVDMA_ROOT_SECURE_CTRL, hsf_yuv_sec_ctrl.Raw);
			//set gsecure value
			ret |= camsys_sec_writeio(SECIO_CAM, CAMRAWDMA_ROOT_SECURE_REGISTER_0, hsf_sec_0.Raw);
			ret |= camsys_sec_writeio(SECIO_CAM, CAMRAWDMA_ROOT_SECURE_REGISTER_1, hsf_sec_1.Raw);
			ret |= camsys_sec_writeio(SECIO_YUV, CAMYUVDMA_ROOT_SECURE_REGISTER_0, hsf_yuv_sec_0.Raw);
			//set raw gdomain value
			ret |= camsys_sec_writeio(SECIO_CAM, CAMRAWDMA_ROOT_DOMAIN_REGISTER_2, hsf_dom_2.Raw);
			ret |= camsys_sec_writeio(SECIO_CAM, CAMRAWDMA_ROOT_DOMAIN_REGISTER_3, hsf_dom_3.Raw);
			ret |= camsys_sec_writeio(SECIO_CAM, CAMRAWDMA_ROOT_DOMAIN_REGISTER_6, hsf_dom_6.Raw);
			ret |= camsys_sec_writeio(SECIO_CAM, CAMRAWDMA_ROOT_DOMAIN_REGISTER_9, hsf_dom_9.Raw);
			//set yuv gdomain value
			ret |= camsys_sec_writeio(SECIO_YUV, CAMYUVDMA_ROOT_DOMAIN_REGISTER_0, hsf_yuv_dom_0.Raw);
			ret |= camsys_sec_writeio(SECIO_YUV, CAMYUVDMA_ROOT_DOMAIN_REGISTER_1, hsf_yuv_dom_1.Raw);
			ret |= camsys_sec_writeio(SECIO_YUV, CAMYUVDMA_ROOT_DOMAIN_REGISTER_2, hsf_yuv_dom_2.Raw);
			ret |= camsys_sec_writeio(SECIO_YUV, CAMYUVDMA_ROOT_DOMAIN_REGISTER_3, hsf_yuv_dom_3.Raw);
			ret |= camsys_sec_writeio(SECIO_YUV, CAMYUVDMA_ROOT_DOMAIN_REGISTER_4, hsf_yuv_dom_4.Raw);
		/*******************************************************************************************/
			//set raw_sel secure lock
			ret |= camsys_sec_writeio(SECIO_CAM, CAMCTL_ROOT_RAW_SEL_SECURE_LOCK, hsf_raw_sel_lock.Raw);
			if(bSecure){
				//reopen dl_en after cid setting
				ret |= camsys_sec_writeio(SECIO_CAM_CTL, CAMCTL_TIF_DL_EN, hsf_camctl_tif_dl_en.Raw);
				ret |= camsys_sec_writeio(SECIO_CAM_CTL, CAMCTL_OTR_DL_EN, hsf_camctl_otr_dl_en.Raw);
				ret |= camsys_sec_writeio(SECIO_CAM_CTL3, CAMCTL3_TIF_DL_EN, hsf_camctl3_tif_dl_en.Raw);
				ret |= camsys_sec_writeio(SECIO_CAM_CTL2, CAMCTL2_TIF_DL_EN, hsf_camctl2_tif_dl_en.Raw);
			}
			register_dump(i + 1, 0);
		}
	}
	CALL_FROM_OPS(puts, "secure register config ");
	CALL_FROM_OPS(puts, ret ? "Fail" : "Success");
	CALL_FROM_OPS(putx64, ret);

	return ret;
}

int isp_sec_configCamsv_platform(int bSecure, int CamSVModule)
{
	int SECIO_CAMSV = SECIO_ISP_CAMSV_A;
	int ret = 0;

	CALL_FROM_OPS(puts, "+ Secure:");
	CALL_FROM_OPS(putx64, bSecure);
	CALL_FROM_OPS(puts, ",CamSVModule:");
	CALL_FROM_OPS(putx64, CamSVModule);

	int i, j;
	REG_E_CAMSVCENTRAL_CID_CHK_SENINF   sv_cid_chk_en;
	REG_E_CAMSVCENTRAL_CID_CHK_RAW_DCIF    sv_cid_chk_raw_dcif_en;

	for (i = 0; i < SEC_CAM_MAX; i++) {
		if(CamSVModule & (1 << i)) {
			switch (i) {
			case SEC_CAM_A:
				SECIO_CAMSV = SECIO_ISP_CAMSV_A;
				break;
			case SEC_CAM_B:
				SECIO_CAMSV = SECIO_ISP_CAMSV_B;
				break;
			case SEC_CAM_C:
				SECIO_CAMSV = SECIO_ISP_CAMSV_C;
				break;
			default:
				CALL_FROM_OPS(puts, "Camsv Module invalid! CamSVModule: ");
				CALL_FROM_OPS(putx64, CamSVModule);
				return -1;
			}
			sv_cid_chk_en.Bits.CAMSVCENTRAL_CAMSV_SENINF_CID_CHK_EN = 0;
			sv_cid_chk_raw_dcif_en.Bits.CAMSVCENTRAL_RAW_TO_CAMSV_DCIF_CID_CHK_EN = 0;
			ret |= camsys_sec_writeio(SECIO_CAMSV, CAMSVCENTRAL_CID_CHK_SENINF, sv_cid_chk_en.Raw);
			ret |= camsys_sec_writeio(SECIO_CAMSV,
				CAMSVCENTRAL_CID_CHK_RAW_DCIF, sv_cid_chk_raw_dcif_en.Raw);
			if (bSecure) {
				// CID 14 [3:0]
				ret |= camsys_sec_writeio(SECIO_CAMSV, CAMSVCENTRAL_ROOT_CAMSV_CID, CAMSV_ROOT_CID_VALUE);
				for (j = CAMSV_TAG_IMG_START; j < CAMSV_TAG_IMG_END; j++) {
					// ROOT_SECURE_REG = gsecure[31]; gdomain[7:0]
					// gdomain = AID 7 [3:0]; SID 9 [7:4]
					// img
					ret |= camsys_sec_writeio(SECIO_CAMSV, CAMSV_DMA_SHIFT + 0x0 + (j * 4), CAMSV_DMA_VALUE);
					// len
					ret |= camsys_sec_writeio(SECIO_CAMSV, CAMSV_DMA_SHIFT + 0x40 + (j * 4), CAMSV_DMA_VALUE);
				}
			} else {
				// CID 14 [3:0]
				ret |= camsys_sec_writeio(SECIO_CAMSV, CAMSVCENTRAL_ROOT_CAMSV_CID, 0x0);
				for (j = CAMSV_TAG_IMG_START; j < CAMSV_TAG_IMG_END; j++) {
					// ROOT_SECURE_REG = gsecure[31]; gdomain[7:0]
					// gdomain = AID 7 [3:0]; SID 9 [7:4]
					// img
					ret |= camsys_sec_writeio(SECIO_CAMSV, CAMSV_DMA_SHIFT + 0x0 + (j * 4), 0x0);
					// len
					ret |= camsys_sec_writeio(SECIO_CAMSV, CAMSV_DMA_SHIFT + 0x40 + (j * 4), 0x0);
				}
			}
			sv_cid_chk_en.Bits.CAMSVCENTRAL_CAMSV_SENINF_CID_CHK_EN = 1;
			sv_cid_chk_raw_dcif_en.Bits.CAMSVCENTRAL_RAW_TO_CAMSV_DCIF_CID_CHK_EN = 1;
			ret |= camsys_sec_writeio(SECIO_CAMSV, CAMSVCENTRAL_CID_CHK_SENINF, sv_cid_chk_en.Raw);
			ret |= camsys_sec_writeio(SECIO_CAMSV, CAMSVCENTRAL_CID_CHK_RAW_DCIF, sv_cid_chk_raw_dcif_en.Raw);
			register_dump(0, i + 1);
		}
	}
	CALL_FROM_OPS(puts, "camsv secure register config ");
	CALL_FROM_OPS(puts, ret ? "Fail" : "Success");
	CALL_FROM_OPS(putx64, ret);

	return ret;
}
