// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_dp_hal.h"
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/sched/clock.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include "mtk_dp_reg.h"
#include "mtk_dp.h"
/* TODO: porting*/
/* #include "mtk_devinfo.h" */
/* display port control related SMC call */
#define MTK_SIP_DP_CONTROL \
	(0x82000523 | 0x40000000)

u32 mtk_dp_read(struct mtk_dp *mtk_dp, u32 offset)
{
	u32 read_val = 0;

	if (offset > 0x8000) {
		DPTXERR("dptx %s, error reg 0x%p, offset 0x%x\n",
			__func__, mtk_dp->regs, offset);
		return 0;
	}

	read_val = readl(mtk_dp->regs + offset - (offset%4))
			>> ((offset % 4) * 8);

	return read_val;
}

void mtk_dp_write(struct mtk_dp *mtk_dp, u32 offset, u32 val)
{
	if ((offset % 4 != 0) || (offset > 0x8000)) {
		DPTXERR("dptx %s, error reg 0x%p, offset 0x%x, value 0x%x\n",
			__func__, mtk_dp->regs, offset, val);
		return;
	}

	writel(val, mtk_dp->regs + offset);
}

void mtk_dp_mask(struct mtk_dp *mtk_dp, u32 offset, u32 val, u32 mask)
{
	void __iomem *reg = mtk_dp->regs + offset;
	u32 tmp;

	if ((offset % 4 != 0) || (offset > 0x8000)) {
		DPTXERR("dptx %s, error reg 0x%p, offset 0x%x, value 0x%x\n",
			__func__, mtk_dp->regs, offset, val);
		return;
	}

	tmp = readl(reg);
	tmp = (tmp & ~mask) | (val & mask);
	writel(tmp, reg);
}

void mtk_dp_write_byte(struct mtk_dp *mtk_dp,
	u32 addr, u8 val, u32 mask)
{
	if (addr % 2) {
		mtk_dp_write(mtk_dp, DP_TX_TOP_APB_WSTRB, 0x12);
		mtk_dp_mask(mtk_dp, addr - 1, (u32)(val << 8), (mask << 8));
	} else {
		mtk_dp_write(mtk_dp, DP_TX_TOP_APB_WSTRB, 0x11);
		mtk_dp_mask(mtk_dp, addr, (u32)val, mask);
	}

	mtk_dp_write(mtk_dp, DP_TX_TOP_APB_WSTRB, 0x00);
}

u32 mtk_dp_phy_read(struct mtk_dp *mtk_dp, u32 offset)
{
	u32 read_val = 0;

	if (offset > 0x1500) {
		DPTXERR("dptx %s, error offset 0x%x\n",
			__func__, offset);
		return 0;
	}

	read_val = readl(mtk_dp->phyd_regs + offset - (offset%4))
			>> ((offset % 4) * 8);

	return read_val;
}

void mtk_dp_phy_write(struct mtk_dp *mtk_dp, u32 offset, u32 val)
{
	if ((offset % 4 != 0) || (offset > 0x1500)) {
		DPTXERR("dptx %s, error offset 0x%x, value 0x%x\n",
			__func__, offset, val);
		return;
	}

	writel(val, mtk_dp->phyd_regs + offset);
}

void mtk_dp_phy_mask(struct mtk_dp *mtk_dp, u32 offset, u32 val, u32 mask)
{
	void __iomem *reg = mtk_dp->phyd_regs + offset;
	u32 tmp;

	if ((offset % 4 != 0) || (offset > 0x1500)) {
		DPTXERR("dptx %s, error reg 0x%p, offset 0x%x, value 0x%x\n",
			__func__, mtk_dp->phyd_regs, offset, val);
		return;
	}

	tmp = readl(reg);
	tmp = (tmp & ~mask) | (val & mask);
	writel(tmp, reg);
}

void mtk_dp_phy_write_byte(struct mtk_dp *mtk_dp,
	u32 addr, u8 val, u32 mask)
{
	if (addr % 2) {
		mtk_dp_write(mtk_dp, DP_TX_TOP_APB_WSTRB, 0x12);
		mtk_dp_phy_mask(mtk_dp, addr - 1, (u32)(val << 8), (mask << 8));
	} else {
		mtk_dp_write(mtk_dp, DP_TX_TOP_APB_WSTRB, 0x11);
		mtk_dp_phy_mask(mtk_dp, addr, (u32)val, mask);
	}

	mtk_dp_write(mtk_dp, DP_TX_TOP_APB_WSTRB, 0x00);
}

unsigned long mtk_dp_atf_call(unsigned int cmd, unsigned int para)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct arm_smccc_res res;
	u32 x3 = (cmd << 16) | para;

	arm_smccc_smc(MTK_SIP_DP_CONTROL, cmd, para,
		x3, 0xFEFD, 0, 0, 0, &res);

	DPTXDBG("%s cmd 0x%x, p1 0x%x, ret 0x%lx-0x%lx",
		__func__, cmd, para, res.a0, res.a1);
	return res.a1;
#else
	return 0;
#endif
}

void mhal_dump_reg(struct mtk_dp *mtk_dp)
{
	u32 i, val[4], reg;
	u8  Data[5] = {0};

	for (i = 0x0; i < 0xE00; i += 16) {
		reg = 0x3000 + i;
		val[0] = msRead4Byte(mtk_dp, reg);
		val[1] = msRead4Byte(mtk_dp, reg + 4);
		val[2] = msRead4Byte(mtk_dp, reg + 8);
		val[3] = msRead4Byte(mtk_dp, reg + 12);
		DPTXMSG("[DP Debug]dptx mac reg[0x%x] = 0x%x 0x%x 0x%x 0x%x",
			reg, val[0], val[1], val[2], val[3]);
	}

	for (i = 0x0; i < 0x1500; i += 16) {
		reg = 0x0000 + i;
		val[0] = msPhyRead4Byte(mtk_dp, reg);
		val[1] = msPhyRead4Byte(mtk_dp, reg + 4);
		val[2] = msPhyRead4Byte(mtk_dp, reg + 8);
		val[3] = msPhyRead4Byte(mtk_dp, reg + 12);
		DPTXMSG("[DP Debug]dptx phy reg[0x%x] = 0x%x 0x%x 0x%x 0x%x",
			reg, val[0], val[1], val[2], val[3]);
	}
	////////////////////////////////////////////////////////

	Data[0]=0x2;
	drm_dp_dpcd_write(&mtk_dp->aux, 0x10B, Data, 0x1);
	drm_dp_dpcd_write(&mtk_dp->aux, 0x10C, Data, 0x1);
	drm_dp_dpcd_write(&mtk_dp->aux, 0x10D, Data, 0x1);
	drm_dp_dpcd_write(&mtk_dp->aux, 0x10E, Data, 0x1);

	drm_dp_dpcd_read(&mtk_dp->aux, 0x210, Data, 0x1);
	DPTXMSG("[DP Debug]DPCD 210 = 0x%x\n",Data[0]);
	drm_dp_dpcd_read(&mtk_dp->aux, 0x211, Data, 0x1);
	DPTXMSG("[DP Debug]DPCD 211 = 0x%x\n",Data[0]);
	drm_dp_dpcd_read(&mtk_dp->aux, 0x212, Data, 0x1);
	DPTXMSG("[DP Debug]DPCD 212 = 0x%x\n",Data[0]);
	drm_dp_dpcd_read(&mtk_dp->aux, 0x213, Data, 0x1);
	DPTXMSG("[DP Debug]DPCD 213 = 0x%x\n",Data[0]);
	drm_dp_dpcd_read(&mtk_dp->aux, 0x214, Data, 0x1);
	DPTXMSG("[DP Debug]DPCD 214 = 0x%x\n",Data[0]);
	drm_dp_dpcd_read(&mtk_dp->aux, 0x215, Data, 0x1);
	DPTXMSG("[DP Debug]DPCD 215 = 0x%x\n",Data[0]);
	drm_dp_dpcd_read(&mtk_dp->aux, 0x216, Data, 0x1);
	DPTXMSG("[DP Debug]DPCD 216 = 0x%x\n",Data[0]);
	drm_dp_dpcd_read(&mtk_dp->aux, 0x217, Data, 0x1);
	DPTXMSG("[DP Debug]DPCD 217 = 0x%x\n",Data[0]);

}

void mhal_DPTx_Verify_Clock(struct mtk_dp *mtk_dp)
{
	u32 m, n, Ls_clk, pix_clk;

	m = msRead4Byte(mtk_dp, REG_33C8_DP_ENCODER1_P0);
	n = 0x8000;
	Ls_clk = mtk_dp->training_info.ubLinkRate;
	Ls_clk *= 27;

	pix_clk = m * Ls_clk / n;
	DPTXMSG("DPTX calc pixel clock = %d MHz, dp_intf clock = %dMHz\n",
		pix_clk, pix_clk/4);
}

void mhal_DPTx_fec_init_setting(struct mtk_dp *mtk_dp)
{
	msWrite4ByteMask(mtk_dp, REG_3540_DP_TRANS_P0,
				1 << FEC_CLOCK_EN_MODE_DP_TRANS_P0_FLDMASK_POS,
				FEC_CLOCK_EN_MODE_DP_TRANS_P0_FLDMASK);
	msWrite4ByteMask(mtk_dp, REG_3540_DP_TRANS_P0,
				2 << FEC_FIFO_UNDER_POINT_DP_TRANS_P0_FLDMASK_POS,
				FEC_FIFO_UNDER_POINT_DP_TRANS_P0_FLDMASK);
}

void mhal_DPTx_InitialSetting(struct mtk_dp *mtk_dp)
{
	if (mtk_dp->cfg_ver == 2) {
		msWrite4ByteMask(mtk_dp, DP_TX_TOP_PWR_STATE,
				(0x3 << DP_PWR_STATE_FLDMASK_POS), DP_PWR_STATE_FLDMASK);

		msWriteByte(mtk_dp, REG_342C_DP_TRANS_P0, 0x68);//26M xtal clock

		mhal_DPTx_fec_init_setting(mtk_dp);

		msWrite4ByteMask(mtk_dp, REG_31EC_DP_ENCODER0_P0 , BIT(4), BIT(4));
		msWrite4ByteMask(mtk_dp, REG_304C_DP_ENCODER0_P0 , 0, BIT(8));
		msWrite4ByteMask(mtk_dp, REG_304C_DP_ENCODER0_P0 , BIT(3), BIT(3));

		//31C4[13] : DSC bypass [11]pps bypass
		msWrite2ByteMask(mtk_dp, REG_31C4_DP_ENCODER0_P0,
				0,
				PPS_HW_BYPASS_MASK_DP_ENCODER0_P0_FLDMASK);

		msWrite2ByteMask(mtk_dp, REG_31C4_DP_ENCODER0_P0,
				0,
				DSC_BYPASS_EN_DP_ENCODER0_P0_FLDMASK);

		msWrite2ByteMask(mtk_dp, REG_336C_DP_ENCODER1_P0,
				0,
				DSC_BYTE_SWAP_DP_ENCODER1_P0_FLDMASK);
	} else {
		msWriteByte(mtk_dp, REG_342C_DP_TRANS_P0, 0x69);
		msWrite4ByteMask(mtk_dp, REG_3540_DP_TRANS_P0, BIT(3), BIT(3));
		msWrite4ByteMask(mtk_dp, REG_31EC_DP_ENCODER0_P0, BIT(4), BIT(4));
		msWrite4ByteMask(mtk_dp, REG_304C_DP_ENCODER0_P0, 0, BIT(8));
		msWrite4ByteMask(mtk_dp, DP_TX_TOP_IRQ_MASK, BIT(2), BIT(2));
	}
}

void mhal_DPTx_DataLanePNSwap(struct mtk_dp *mtk_dp, bool bENABLE)
{
	if (bENABLE) {
		msWriteByteMask(mtk_dp, REG_3428_DP_TRANS_P0,
			POST_MISC_PN_SWAP_EN_LANE0_DP_TRANS_P0_FLDMASK_POS,
			POST_MISC_PN_SWAP_EN_LANE0_DP_TRANS_P0_FLDMASK);
		msWriteByteMask(mtk_dp, REG_3428_DP_TRANS_P0,
			POST_MISC_PN_SWAP_EN_LANE1_DP_TRANS_P0_FLDMASK_POS,
			POST_MISC_PN_SWAP_EN_LANE1_DP_TRANS_P0_FLDMASK);
		msWriteByteMask(mtk_dp, REG_3428_DP_TRANS_P0,
			POST_MISC_PN_SWAP_EN_LANE2_DP_TRANS_P0_FLDMASK_POS,
			POST_MISC_PN_SWAP_EN_LANE2_DP_TRANS_P0_FLDMASK);
		msWriteByteMask(mtk_dp, REG_3428_DP_TRANS_P0,
			POST_MISC_PN_SWAP_EN_LANE3_DP_TRANS_P0_FLDMASK_POS,
			POST_MISC_PN_SWAP_EN_LANE3_DP_TRANS_P0_FLDMASK);
	} else {
		msWriteByteMask(mtk_dp, REG_3428_DP_TRANS_P0, 0,
			POST_MISC_PN_SWAP_EN_LANE0_DP_TRANS_P0_FLDMASK);
		msWriteByteMask(mtk_dp, REG_3428_DP_TRANS_P0, 0,
			POST_MISC_PN_SWAP_EN_LANE1_DP_TRANS_P0_FLDMASK);
		msWriteByteMask(mtk_dp, REG_3428_DP_TRANS_P0, 0,
			POST_MISC_PN_SWAP_EN_LANE2_DP_TRANS_P0_FLDMASK);
		msWriteByteMask(mtk_dp, REG_3428_DP_TRANS_P0, 0,
			POST_MISC_PN_SWAP_EN_LANE3_DP_TRANS_P0_FLDMASK);
	}
}

void mhal_recover_safe_mode_settting(struct mtk_dp *mtk_dp)
{
	//disable safe mode setting
	msWrite4ByteMask(mtk_dp, 0x304C , 0x2 , 0x3);
	msWrite4ByteMask(mtk_dp, 0x303C , 0x1000 , 0x3F);
	msWrite4ByteMask(mtk_dp, 0x3358 , 0x0, 0x80);
	msWrite4ByteMask(mtk_dp, 0x3004 , 0x0, 0x100);
}

void mhal_DPTx_Set_Efuse_Value(struct mtk_dp *mtk_dp, bool plug_orientation)
{
	void *base;
	uint32_t efuse_value;
	uint32_t efuse_base[4] = {
		mtk_dp->read_from_efuse[0],// 0x132601b4
		mtk_dp->read_from_efuse[1],// 0x132601bc
		mtk_dp->read_from_efuse[2],// 0x132601b8
		mtk_dp->read_from_efuse[3]//  0x132601c0
	};

	if ((efuse_base[0] == 0x0) || (efuse_base[1] == 0x0
			|| efuse_base[2] == 0x0) || (efuse_base[3] ==
			0x0)) {
		DPTXMSG("no efuse, use default value!\n");
		return;
	}

	// Bandgap
	if (plug_orientation == 1) { // Normal
		// RG_XTP_GLB_BIAS_V2V_VTRIM[3:0]
		efuse_value = (efuse_base[1] >> 6) & 0xF;
		msPhyWrite4ByteMask(mtk_dp, 0x0604, efuse_value << 10, 0xF << 10);

		// RG_XTP_GLB_BIAS_INTR_CTRL[3:0]
		efuse_value = efuse_base[1] & 0xF;
		msPhyWrite4ByteMask(mtk_dp, 0x0088, efuse_value << 0, 0xF << 0);
	} else { // Flipped
		// RG_XTP_GLB_BIAS_V2V_VTRIM[3:0]
		efuse_value = (efuse_base[0] >> 6) & 0xF;
		msPhyWrite4ByteMask(mtk_dp, 0x0604, efuse_value << 10, 0xF << 10);

		// RG_XTP_GLB_BIAS_INTR_CTRL[3:0]
		efuse_value = efuse_base[0] & 0xF;
		msPhyWrite4ByteMask(mtk_dp, 0x0088, efuse_value << 0, 0xF << 0);
	}

	// AUX R50
	if (plug_orientation == 1) { // Normal
		// RG_CKM_CKTX_IMPSEL_PMOS[3:0]
		efuse_value = (efuse_base[1] >> 10) & 0xF;
		msPhyWrite4ByteMask(mtk_dp, 0x0600, efuse_value << 8, 0xF << 8);

		// RG_XTP_GLB_BIAS_INTR_CTRL[5:0]
		efuse_value = (efuse_base[1] >> 14) & 0xF;
		msPhyWrite4ByteMask(mtk_dp, 0x0600, efuse_value << 12, 0xF << 12);
	} else { // Flipped
		// RG_CKM_CKTX_IMPSEL_PMOS[3:0]
		efuse_value = (efuse_base[0] >> 10) & 0xF;
		msPhyWrite4ByteMask(mtk_dp, 0x0600, efuse_value << 8, 0xF << 8);

		// RG_XTP_GLB_BIAS_INTR_CTRL[5:0]
		efuse_value = (efuse_base[0] >> 14) & 0xF;
		msPhyWrite4ByteMask(mtk_dp, 0x0600, efuse_value << 12, 0xF << 12);
	}

	// Main-Link
	if (plug_orientation == 1) { // Normal
		// RG_XTP_LN0_TX_IMPSEL_PMOS[4:0]
		efuse_value = (efuse_base[1] >> 22) & 0x1F;
		msPhyWrite4ByteMask(mtk_dp, 0x0088, efuse_value << 10, 0x1F << 10);

		// RG_XTP_LN0_TX_IMPSEL_NMOS[4:0]
		efuse_value = (efuse_base[1] >> 27) & 0x1F;
		msPhyWrite4ByteMask(mtk_dp, 0x0088, efuse_value << 15, 0x1F << 15);

		// RG_XTP_LN1_TX_IMPSEL_PMOS[4:0]
		efuse_value = (efuse_base[3] >> 19) & 0x1F;
		msPhyWrite4ByteMask(mtk_dp, 0x0088, efuse_value << 20, 0x1F << 20);

		// RG_XTP_LN1_TX_IMPSEL_NMOS[4:0]
		efuse_value = (efuse_base[3] >> 24) & 0x1F;
		msPhyWrite4ByteMask(mtk_dp, 0x0088, efuse_value << 25, 0x1F << 25);

		// RG_XTP_LN2_TX_IMPSEL_PMOS[4:0]
		efuse_value = (efuse_base[2] >> 19) & 0x1F;
		msPhyWrite4ByteMask(mtk_dp, 0x008C, efuse_value << 0, 0x1F << 0);

		// RG_XTP_LN2_TX_IMPSEL_NMOS[4:0]
		efuse_value = (efuse_base[2] >> 24) & 0x1F;
		msPhyWrite4ByteMask(mtk_dp, 0x008C, efuse_value << 8, 0x1F << 8);

		// RG_XTP_LN3_TX_IMPSEL_PMOS[4:0]
		efuse_value = (efuse_base[0] >> 22) & 0x1F;
		msPhyWrite4ByteMask(mtk_dp, 0x008C, efuse_value << 16, 0x1F << 16);

		// RG_XTP_LN3_TX_IMPSEL_NMOS[4:0]
		efuse_value = (efuse_base[0] >> 27) & 0x1F;
		msPhyWrite4ByteMask(mtk_dp, 0x008C, efuse_value << 24, 0x1F << 24);
	} else { // Flipped
		// RG_XTP_LN0_TX_IMPSEL_PMOS[4:0]
		efuse_value = (efuse_base[0] >> 22) & 0x1F;
		msPhyWrite4ByteMask(mtk_dp, 0x0088, efuse_value << 10, 0x1F << 10);

		// RG_XTP_LN0_TX_IMPSEL_NMOS[4:0]
		efuse_value = (efuse_base[0] >> 27) & 0x1F;
		msPhyWrite4ByteMask(mtk_dp, 0x0088, efuse_value << 15, 0x1F << 15);

		// RG_XTP_LN1_TX_IMPSEL_PMOS[4:0]
		efuse_value = (efuse_base[2] >> 19) & 0x1F;
		msPhyWrite4ByteMask(mtk_dp, 0x0088, efuse_value << 20, 0x1F << 20);

		// RG_XTP_LN1_TX_IMPSEL_NMOS[4:0]
		efuse_value = (efuse_base[2] >> 24) & 0x1F;
		msPhyWrite4ByteMask(mtk_dp, 0x0088, efuse_value << 25, 0x1F << 25);

		// RG_XTP_LN2_TX_IMPSEL_PMOS[4:0]
		efuse_value = (efuse_base[3] >> 19) & 0x1F;
		msPhyWrite4ByteMask(mtk_dp, 0x008C, efuse_value << 0, 0x1F << 0);

		// RG_XTP_LN2_TX_IMPSEL_NMOS[4:0]
		efuse_value = (efuse_base[3] >> 24) & 0x1F;
		msPhyWrite4ByteMask(mtk_dp, 0x008C, efuse_value << 8, 0x1F << 8);

		// RG_XTP_LN3_TX_IMPSEL_PMOS[4:0]
		efuse_value = (efuse_base[1] >> 22) & 0x1F;
		msPhyWrite4ByteMask(mtk_dp, 0x008C, efuse_value << 16, 0x1F << 16);

		// RG_XTP_LN3_TX_IMPSEL_NMOS[4:0]
		efuse_value = (efuse_base[1] >> 27) & 0x1F;
		msPhyWrite4ByteMask(mtk_dp, 0x008C, efuse_value << 24, 0x1F << 24);
	}
}

void mhal_DPTx_Set_VideoInterlance(struct mtk_dp *mtk_dp, bool bENABLE)
{
	if (bENABLE) {
		msWriteByteMask(mtk_dp, REG_3030_DP_ENCODER0_P0 + 1,
			BIT(6)|BIT(5), BIT(6)|BIT(5));
		msWriteByteMask(mtk_dp, REG_3368_DP_ENCODER1_P0 + 1,
			0, BIT(5)|BIT(4));
		DPTXMSG("DPTX imode force-ov\n");
	} else {
		msWriteByteMask(mtk_dp, REG_3030_DP_ENCODER0_P0 + 1,
			BIT(6), BIT(6)|BIT(5));
		msWriteByteMask(mtk_dp, REG_3368_DP_ENCODER1_P0 + 1,
			BIT(4), BIT(5)|BIT(4));
		DPTXMSG("DPTX pmode force-ov\n");
	}
}

void mhal_DPTx_EnableBypassMSA(struct mtk_dp *mtk_dp, bool bENABLE)
{
	if (bENABLE)
		msWrite2ByteMask(mtk_dp, REG_3030_DP_ENCODER0_P0,
			0, 0x03FF);
	else
		msWrite2ByteMask(mtk_dp, REG_3030_DP_ENCODER0_P0,
			0x03FF, 0x03FF);
}

void mhal_DPTx_SetMSA(struct mtk_dp *mtk_dp)
{
	struct DPTX_TIMING_PARAMETER *DPTX_TBL = &mtk_dp->info.DPTX_OUTBL;

	msWrite2Byte(mtk_dp, REG_3010_DP_ENCODER0_P0, DPTX_TBL->Htt);
	msWrite2Byte(mtk_dp, REG_3018_DP_ENCODER0_P0,
		DPTX_TBL->Hsw + DPTX_TBL->Hbp);
	msWrite2ByteMask(mtk_dp, REG_3028_DP_ENCODER0_P0,
		DPTX_TBL->Hsw << HSW_SW_DP_ENCODER0_P0_FLDMASK_POS,
		HSW_SW_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3028_DP_ENCODER0_P0,
		DPTX_TBL->bHsp << HSP_SW_DP_ENCODER0_P0_FLDMASK_POS,
		HSP_SW_DP_ENCODER0_P0_FLDMASK);
	msWrite2Byte(mtk_dp, REG_3020_DP_ENCODER0_P0, DPTX_TBL->Hde);
	msWrite2Byte(mtk_dp, REG_3014_DP_ENCODER0_P0, DPTX_TBL->Vtt);
	msWrite2Byte(mtk_dp, REG_301C_DP_ENCODER0_P0,
		DPTX_TBL->Vsw + DPTX_TBL->Vbp);
	msWrite2ByteMask(mtk_dp, REG_302C_DP_ENCODER0_P0,
		DPTX_TBL->Vsw << VSW_SW_DP_ENCODER0_P0_FLDMASK_POS,
		VSW_SW_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_302C_DP_ENCODER0_P0,
		DPTX_TBL->bVsp << VSP_SW_DP_ENCODER0_P0_FLDMASK_POS,
		VSP_SW_DP_ENCODER0_P0_FLDMASK);
	msWrite2Byte(mtk_dp, REG_3024_DP_ENCODER0_P0, DPTX_TBL->Vde);
	msWrite2Byte(mtk_dp, REG_3064_DP_ENCODER0_P0, DPTX_TBL->Hde);
	msWrite2Byte(mtk_dp, REG_3154_DP_ENCODER0_P0, (DPTX_TBL->Htt));
	msWrite2Byte(mtk_dp, REG_3158_DP_ENCODER0_P0, (DPTX_TBL->Hfp));
	msWrite2Byte(mtk_dp, REG_315C_DP_ENCODER0_P0, (DPTX_TBL->Hsw));
	msWrite2Byte(mtk_dp, REG_3160_DP_ENCODER0_P0,
		DPTX_TBL->Hbp + DPTX_TBL->Hsw);
	msWrite2Byte(mtk_dp, REG_3164_DP_ENCODER0_P0, (DPTX_TBL->Hde));
	msWrite2Byte(mtk_dp, REG_3168_DP_ENCODER0_P0, DPTX_TBL->Vtt);
	msWrite2Byte(mtk_dp, REG_316C_DP_ENCODER0_P0, DPTX_TBL->Vfp);
	msWrite2Byte(mtk_dp, REG_3170_DP_ENCODER0_P0, DPTX_TBL->Vsw);
	msWrite2Byte(mtk_dp, REG_3174_DP_ENCODER0_P0,
		DPTX_TBL->Vbp + DPTX_TBL->Vsw);
	msWrite2Byte(mtk_dp, REG_3178_DP_ENCODER0_P0, DPTX_TBL->Vde);

	DPTXMSG("MSA:Htt=%d Vtt=%d Hact=%d Vact=%d, fps=%d\n",
			DPTX_TBL->Htt, DPTX_TBL->Vtt,
			DPTX_TBL->Hde, DPTX_TBL->Vde, DPTX_TBL->FrameRate);
}

void mhal_DPTx_SetColorFormat(struct mtk_dp *mtk_dp, BYTE  enOutColorFormat)
{
	msWriteByteMask(mtk_dp, REG_3034_DP_ENCODER0_P0,
		(enOutColorFormat << 0x1), MASKBIT(2 : 1));  //MISC0

	if ((enOutColorFormat == DP_COLOR_FORMAT_RGB_444)
		|| (enOutColorFormat == DP_COLOR_FORMAT_YUV_444))
		msWriteByteMask(mtk_dp, REG_303C_DP_ENCODER0_P0 + 1,
			(0), MASKBIT(6 : 4));
	else if (enOutColorFormat == DP_COLOR_FORMAT_YUV_422)
		msWriteByteMask(mtk_dp, REG_303C_DP_ENCODER0_P0 + 1,
			(BIT(4)), MASKBIT(6 : 4));
	else if (enOutColorFormat == DP_COLOR_FORMAT_YUV_420)
		msWriteByteMask(mtk_dp, REG_303C_DP_ENCODER0_P0 + 1, (BIT(5)),
			MASKBIT(6 : 4));
}

void mhal_DPTx_SetColorDepth(struct mtk_dp *mtk_dp, BYTE coloer_depth)
{
	msWriteByteMask(mtk_dp, REG_3034_DP_ENCODER0_P0,
		(coloer_depth << 0x5), 0xE0);

	switch (coloer_depth) {
	case DP_COLOR_DEPTH_6BIT:
		msWriteByteMask(mtk_dp,
			REG_303C_DP_ENCODER0_P0 + 1,
			4,
			0x07);
		break;
	case DP_COLOR_DEPTH_8BIT:
		msWriteByteMask(mtk_dp,
			REG_303C_DP_ENCODER0_P0 + 1,
			3,
			0x07);
		break;
	case DP_COLOR_DEPTH_10BIT:
		msWriteByteMask(mtk_dp,
			REG_303C_DP_ENCODER0_P0 + 1,
			2,
			0x07);
		break;
	case DP_COLOR_DEPTH_12BIT:
		msWriteByteMask(mtk_dp,
			REG_303C_DP_ENCODER0_P0 + 1,
			1,
			0x07);
		break;
	case DP_COLOR_DEPTH_16BIT:
		msWriteByteMask(mtk_dp,
			REG_303C_DP_ENCODER0_P0 + 1,
			0,
			0x07);
		break;
	default:
		break;
	}
}

void mhal_DPTx_Set_BS2BS_Cnt(struct mtk_dp *mtk_dp, bool bEnable, DWORD uiHTT)
{
	DWORD uiVideoMValue = 0;
	DWORD uiVideoNValue = 0;
	DWORD uiBS2BS_Cnt = 1;
	enum mtk_mmsys_id id = MMSYS_MT6985;

	if (mtk_dp->priv && mtk_dp->priv->data)
		id = mtk_dp->priv->data->mmsys_id;

	if (id != MMSYS_MT6983 && id != MMSYS_MT6895)
		return;

	//For DTPX_BS2BS_CNT_SWMODE_DSCBYPASS, uiPara is HTotal
	//REG_DPTX_ENCODER_E0_1_78[15:0] = (Htotal * (link_rate/pix_rate) / 4)
	//                               =  (Htotal x (video_N/video_M) / 4)

	uiVideoNValue = (msRead2Byte(mtk_dp, REG_3050_DP_ENCODER0_P0) |
					msReadByte(mtk_dp, REG_3054_DP_ENCODER0_P0) << 16);
	uiVideoMValue = (msRead2Byte(mtk_dp, REG_33C8_DP_ENCODER1_P0) |
					msReadByte(mtk_dp, REG_33CC_DP_ENCODER1_P0) << 16);

	if (uiVideoMValue != 0)
		uiBS2BS_Cnt = ((uiHTT >> 2) * uiVideoNValue) / uiVideoMValue;

	msWrite2Byte(mtk_dp, REG_33E0_DP_ENCODER1_P0, uiBS2BS_Cnt-1);

	//REG_DPTX_ENCODER_E0_1_7B[10]:BS to BS cnt SW sel
	msWriteByteMask(mtk_dp, REG_33EC_DP_ENCODER1_P0 + 1, (bEnable ? BIT(2) : 0), BIT(2));
}


void mhal_DPTx_SetMISC(struct mtk_dp *mtk_dp, BYTE ucMISC[2])
{
	msWriteByteMask(mtk_dp, REG_3034_DP_ENCODER0_P0,
		ucMISC[0], 0xFE);
	msWriteByteMask(mtk_dp, REG_3034_DP_ENCODER0_P0 + 1,
		ucMISC[1], 0xFF);
}

void mhal_DPTx_Set_MVIDx2(struct mtk_dp *mtk_dp, bool bEnable)
{
	if (bEnable)
		msWriteByteMask(mtk_dp, REG_300C_DP_ENCODER0_P0 + 1,
			BIT(4), BIT(6)|BIT(5)|BIT(4));
	else
		msWriteByteMask(mtk_dp, REG_300C_DP_ENCODER0_P0 + 1,
			0, BIT(6)|BIT(5)|BIT(4));
}

bool mhal_DPTx_OverWrite_MN(struct mtk_dp *mtk_dp,
	bool bEnable, DWORD ulVideo_M, DWORD ulVideo_N)
{
	if (bEnable) {
		//Turn-on overwrite MN
		msWrite2Byte(mtk_dp,
			REG_3008_DP_ENCODER0_P0,
			ulVideo_M & 0xFFFF);
		msWriteByte(mtk_dp,
			REG_300C_DP_ENCODER0_P0,
			((ulVideo_M >> 16) & 0xFF));
		msWrite2Byte(mtk_dp,
			REG_3044_DP_ENCODER0_P0,
			ulVideo_N & 0xFFFF);
		msWriteByte(mtk_dp,
			REG_3048_DP_ENCODER0_P0,
			(ulVideo_N >> 16) & 0xFF);
		msWrite2Byte(mtk_dp,
			REG_3050_DP_ENCODER0_P0,
			ulVideo_N & 0xFFFF);
		// legerII add
		msWriteByte(mtk_dp,
			REG_3054_DP_ENCODER0_P0,
			(ulVideo_N >> 16) & 0xFF);
		//LegerII add
		msWriteByteMask(mtk_dp,
			REG_3004_DP_ENCODER0_P0 + 1,
			BIT(0),
			BIT(0));
	} else {
		//Turn-off overwrite MN
		msWriteByteMask(mtk_dp,
			REG_3004_DP_ENCODER0_P0 + 1,
			0,
			BIT(0));
	}

	return true;
}

BYTE mhal_DPTx_GetColorBpp(struct mtk_dp *mtk_dp)
{
	BYTE ColorBpp;
	BYTE ubDPTXColorDepth = mtk_dp->info.depth;
	BYTE ubDPTXColorFormat = mtk_dp->info.format;

	switch (ubDPTXColorDepth) {
	case DP_COLOR_DEPTH_6BIT:
		if (ubDPTXColorFormat == DP_COLOR_FORMAT_YUV_422)
			ColorBpp = 16;
		else if (ubDPTXColorFormat == DP_COLOR_FORMAT_YUV_420)
			ColorBpp = 12;
		else
			ColorBpp = 18;
		break;
	case DP_COLOR_DEPTH_8BIT:
		if (ubDPTXColorFormat == DP_COLOR_FORMAT_YUV_422)
			ColorBpp = 16;
		else if (ubDPTXColorFormat == DP_COLOR_FORMAT_YUV_420)
			ColorBpp = 12;
		else
			ColorBpp = 24;
		break;
	case DP_COLOR_DEPTH_10BIT:
		if (ubDPTXColorFormat == DP_COLOR_FORMAT_YUV_422)
			ColorBpp = 20;
		else if (ubDPTXColorFormat == DP_COLOR_FORMAT_YUV_420)
			ColorBpp = 15;
		else
			ColorBpp = 30;
		break;
	case DP_COLOR_DEPTH_12BIT:
		if (ubDPTXColorFormat == DP_COLOR_FORMAT_YUV_422)
			ColorBpp = 24;
		else if (ubDPTXColorFormat == DP_COLOR_FORMAT_YUV_420)
			ColorBpp = 18;
		else
			ColorBpp = 36;
		break;
	case DP_COLOR_DEPTH_16BIT:
		if (ubDPTXColorFormat == DP_COLOR_FORMAT_YUV_422)
			ColorBpp = 32;
		else if (ubDPTXColorFormat == DP_COLOR_FORMAT_YUV_420)
			ColorBpp = 24;
		else
			ColorBpp = 48;
		break;
	default:
		ColorBpp = 24;
		DPTXMSG("Set Wrong Bpp = %d\n", ColorBpp);
		break;
	}

	return ColorBpp;
}

void mhal_DPTx_SetTU_SramRdStart(struct mtk_dp *mtk_dp, WORD uwValue)
{
	//[5:0]video sram start address=>modify in 480P case only, default=0x1F
	msWriteByteMask(mtk_dp, REG_303C_DP_ENCODER0_P0, uwValue, 0x3F);
}

void mhal_DPTx_SetSDP_DownCntinitInHblanking(struct mtk_dp *mtk_dp,
	WORD uwValue)
{
	 //[11 : 0]mtk_dp, REG_sdp_down_cnt_init_in_hblank
	msWrite2ByteMask(mtk_dp, REG_3364_DP_ENCODER1_P0, uwValue, 0x0FFF);

}

void mhal_DPTx_SetSDP_DownCntinit(struct mtk_dp *mtk_dp, WORD uwValue)
{
	//[11 : 0]mtk_dp, REG_sdp_down_cnt_init
	msWrite2ByteMask(mtk_dp, REG_3040_DP_ENCODER0_P0, uwValue, 0x0FFF);
}

void mhal_DPTx_SetTU_SetEncoder(struct mtk_dp *mtk_dp)
{
	msWriteByteMask(mtk_dp, REG_303C_DP_ENCODER0_P0 + 1, BIT(7), BIT(7));
	msWrite2Byte(mtk_dp, REG_3040_DP_ENCODER0_P0, 0x2020);
	msWrite2ByteMask(mtk_dp, REG_3364_DP_ENCODER1_P0, 0x2020, 0x0FFF);
	msWriteByteMask(mtk_dp, REG_3300_DP_ENCODER1_P0 + 1, 0x02, BIT(1)|BIT(0));
	msWriteByteMask(mtk_dp, REG_3364_DP_ENCODER1_P0 + 1, 0x40, 0x70);
	if (mtk_dp->priv && mtk_dp->priv->data &&
			(mtk_dp->priv->data->mmsys_id == MMSYS_MT6897 ||
			mtk_dp->priv->data->mmsys_id == MMSYS_MT6989))
		msWrite2Byte(mtk_dp, REG_3368_DP_ENCODER1_P0,
			(0x1 << BS_FOLLOW_SEL_DP_ENCODER1_P0_FLDMASK_POS) |
			(0x4 << BS2BS_MODE_DP_ENCODER1_P0_FLDMASK_POS) |
			(0x1 << SDP_DP13_EN_DP_ENCODER1_P0_FLDMASK_POS) |
			(0x1 << VIDEO_STABLE_CNT_THRD_DP_ENCODER1_P0_FLDMASK_POS) |
			(0x1 << VIDEO_SRAM_FIFO_CNT_RESET_SEL_DP_ENCODER1_P0_FLDMASK_POS));
	else
		msWrite2Byte(mtk_dp, REG_3368_DP_ENCODER1_P0, 0x1111);

}

void mhal_DPTx_PGEnable(struct mtk_dp *mtk_dp, bool bENABLE)
{
	if (bENABLE)
		msWriteByteMask(mtk_dp,
			REG_3038_DP_ENCODER0_P0 + 1,
			BIT(3),
			BIT(3));
	else
		msWriteByteMask(mtk_dp,
			REG_3038_DP_ENCODER0_P0 + 1,
			0,
			BIT(3));

	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0, BIT(6), BIT(6));
}

void mhal_DPTx_PG_Pure_Color(struct mtk_dp *mtk_dp, BYTE BGR, DWORD ColorDepth)
{
	msWriteByteMask(mtk_dp, REG_3038_DP_ENCODER0_P0 + 1, BIT(3), BIT(3));
	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0, 0, MASKBIT(6:4));

	switch (BGR) {
	case DPTX_PG_PURECOLOR_BLUE:
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0,
			ColorDepth, MASKBIT(11:0));
		break;
	case DPTX_PG_PURECOLOR_GREEN:
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0,
			ColorDepth, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		break;
	case DPTX_PG_PURECOLOR_RED:
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0,
			ColorDepth, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		break;
	default:
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0,
			ColorDepth, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		break;
	}
}

void mhal_DPTx_PG_VerticalRamping(struct mtk_dp *mtk_dp, BYTE BGR,
	DWORD ColorDepth, BYTE Location)
{
	msWriteByteMask(mtk_dp, REG_3038_DP_ENCODER0_P0 + 1, BIT(3), BIT(3));
	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0, BIT(4), MASKBIT(6:4));
	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0, BIT(7), BIT(7));

	switch (Location) {
	case DPTX_PG_LOCATION_ALL:
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0,
			ColorDepth, MASKBIT(11:0));
		msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0,
			BGR, MASKBIT(2:0));
		msWrite2Byte(mtk_dp, REG_31A0_DP_ENCODER0_P0, 0x3FFF);
		break;
	case DPTX_PG_LOCATION_TOP:
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0,
			ColorDepth, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0,
			BGR, MASKBIT(2:0));
		msWrite2Byte(mtk_dp, REG_31A0_DP_ENCODER0_P0, 0x40);
		break;
	case DPTX_PG_LOCATION_BOTTOM:
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0,
			ColorDepth, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0,
			BGR, MASKBIT(2:0));
		msWrite2Byte(mtk_dp, REG_31A0_DP_ENCODER0_P0, 0x2FFF);
		break;
	default:
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0,
			ColorDepth, MASKBIT(11:0));
		msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0,
			BGR, MASKBIT(2:0));
		msWrite2Byte(mtk_dp, REG_31A0_DP_ENCODER0_P0, 0x3FFF);
		break;
	}
}

void mhal_DPTx_PG_HorizontalRamping(struct mtk_dp *mtk_dp, BYTE BGR,
	DWORD ColorDepth, BYTE Location)
{
	DWORD Ramp = 0x3FFF;


	msWriteByteMask(mtk_dp, REG_3038_DP_ENCODER0_P0 + 1, BIT(3), BIT(3));
	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0, BIT(5), MASKBIT(6:4));
	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0, BIT(7), BIT(7));
	msWrite2Byte(mtk_dp, REG_31A0_DP_ENCODER0_P0, Ramp);

	switch (Location) {
	case DPTX_PG_LOCATION_ALL:
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0,
			BGR, MASKBIT(2:0));
		break;
	case DPTX_PG_LOCATION_LEFT_OF_TOP:
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0,
			BGR, MASKBIT(2:0));
		msWrite2Byte(mtk_dp, REG_31A0_DP_ENCODER0_P0, 0x3FFF);
		break;
	case DPTX_PG_LOCATION_LEFT_OF_BOTTOM:
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0,
			BGR, MASKBIT(2:0));
		msWrite2Byte(mtk_dp, REG_31A0_DP_ENCODER0_P0, 0x3FFF);
		break;
	default:
		break;

	}
}

void mhal_DPTx_PG_VerticalColorBar(struct mtk_dp *mtk_dp, BYTE Location)
{
	msWriteByteMask(mtk_dp, REG_3038_DP_ENCODER0_P0 + 1, BIT(3), BIT(3));
	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0,
		BIT(5)|BIT(4), MASKBIT(6:4));

	switch (Location) {
	case DPTX_PG_LOCATION_ALL:
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0,
			0, MASKBIT(5:4));
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0,
			0, MASKBIT(2:0));
		break;
	case DPTX_PG_LOCATION_LEFT:
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0,
			BIT(4), MASKBIT(5:4));
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0,
			0, MASKBIT(2:0));
		break;
	case DPTX_PG_LOCATION_RIGHT:
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0,
			BIT(4), MASKBIT(5:4));
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0,
			BIT(2), MASKBIT(2:0));
		break;
	case DPTX_PG_LOCATION_LEFT_OF_LEFT:
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0,
			BIT(5)|BIT(4), MASKBIT(5:4));
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0,
			0, MASKBIT(2:0));
		break;
	case DPTX_PG_LOCATION_RIGHT_OF_LEFT:
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0,
			BIT(5)|BIT(4), MASKBIT(5:4));
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0,
			BIT(1), MASKBIT(2:0));
		break;
	case DPTX_PG_LOCATION_LEFT_OF_RIGHT:
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0,
			BIT(5)|BIT(4), MASKBIT(5:4));
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0,
			BIT(2), MASKBIT(2:0));
		break;
	case DPTX_PG_LOCATION_RIGHT_OF_RIGHT:
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0,
			BIT(5)|BIT(4), MASKBIT(5:4));
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0,
			BIT(2)|BIT(1), MASKBIT(2:0));
		break;
	default:
		break;
	}
}

void mhal_DPTx_PG_HorizontalColorBar(struct mtk_dp *mtk_dp, BYTE Location)
{
	msWriteByteMask(mtk_dp, REG_3038_DP_ENCODER0_P0 + 1, BIT(3), BIT(3));
	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0, BIT(6), MASKBIT(6:4));
	switch (Location) {
	case DPTX_PG_LOCATION_ALL:
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0,
			0, MASKBIT(5:4));
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0,
			0, MASKBIT(2:0));
		break;
	case DPTX_PG_LOCATION_TOP:
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0,
			BIT(4), MASKBIT(5:4));
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0,
			0, MASKBIT(2:0));
		break;
	case DPTX_PG_LOCATION_BOTTOM:
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0,
			BIT(4), MASKBIT(5:4));
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0,
			BIT(2), MASKBIT(2:0));
		break;
	default:
		break;
	}
}

void mhal_DPTx_PG_Chessboard(struct mtk_dp *mtk_dp, BYTE Location,
	WORD Hde, WORD Vde)
{
	msWriteByteMask(mtk_dp, REG_3038_DP_ENCODER0_P0 + 1, BIT(3), BIT(3));
	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0, BIT(6)|BIT(4),
		MASKBIT(6:4));

	switch (Location) {
	case DPTX_PG_LOCATION_ALL:
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0,
			0xFFF, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0,
			0xFFF, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0,
			0xFFF, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3194_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3198_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_319C_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_31A8_DP_ENCODER0_P0,
			(Hde/8), MASKBIT(13:0));
		msWrite2ByteMask(mtk_dp, REG_31AC_DP_ENCODER0_P0,
			(Vde/8), MASKBIT(13:0));
		break;
	default:
		break;
	}
}

void mhal_DPTx_PG_SubPixel(struct mtk_dp *mtk_dp, BYTE Location)
{
	msWriteByteMask(mtk_dp, REG_3038_DP_ENCODER0_P0 + 1, BIT(3), BIT(3));
	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0,
		BIT(6)|BIT(5), MASKBIT(6:4));

	switch (Location) {
	case DPTX_PG_PIXEL_ODD_MASK:
		msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0 + 1, 0, BIT(5));
		break;
	case DPTX_PG_PIXEL_EVEN_MASK:
		msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0 + 1,
			BIT(5), BIT(5));
		break;
	default:
		break;
	}
}

void mhal_DPTx_PG_Frame(struct mtk_dp *mtk_dp, BYTE Location,
	WORD Hde, WORD Vde)
{
	msWriteByteMask(mtk_dp, REG_3038_DP_ENCODER0_P0 + 1, BIT(3), BIT(3));
	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0, BIT(6)|BIT(5)|BIT(4),
		MASKBIT(6:4));

	switch (Location) {
	case DPTX_PG_PIXEL_ODD_MASK:
		msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0 + 1,
			0, BIT(5));
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0,
			0xFFF, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0,
			0xFFF, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0,
			0xFFF, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3194_DP_ENCODER0_P0,
			0xFFF, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_3198_DP_ENCODER0_P0,
			0xFFF, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_319C_DP_ENCODER0_P0,
			0, MASKBIT(11:0));
		msWrite2ByteMask(mtk_dp, REG_31A8_DP_ENCODER0_P0,
			((Hde/8)-12), MASKBIT(13:0));
		msWrite2ByteMask(mtk_dp, REG_31AC_DP_ENCODER0_P0,
			((Vde/8)-12), MASKBIT(13:0));
		msWriteByteMask(mtk_dp, REG_31B4_DP_ENCODER0_P0,
			0x0B, MASKBIT(3:0));
		break;

	default:
		break;
	}
}

void mhal_DPTx_Audio_Setting(struct mtk_dp *mtk_dp, BYTE Channel, BYTE bEnable)
{
	if (bEnable) {
		msWrite4Byte(mtk_dp, 0x112101FC, 0x110E10E4);

		if (Channel == 2)//audio dptx config [1] : 0=2ch, 1=8ch
			msWrite4Byte(mtk_dp, 0x11210558, 0x0000FF01);
		else
			msWrite4Byte(mtk_dp, 0x11210558, 0x0000FF03);

		//channel status setting [13:12]:0=16bck, 1=24bck, 2=32bck
		//[11:10]:0=2ch, 1=4ch, 2=8ch
		//[9:8]:0=8bit, 1=16bit, 2=32bit [0]:enable
		if (Channel == 2)
			msWrite4Byte(mtk_dp, 0x11210548, 0x00002201);
		else
			msWrite4Byte(mtk_dp, 0x11210548, 0x00002A01);

		msWrite4Byte(mtk_dp, 0x11210000, 0x60004000); //turn on clock
		msWrite4Byte(mtk_dp, 0x11210010, 0x00000003); //turn on afe
	} else {
		msWrite4Byte(mtk_dp, 0x11210000, 0x00000000); //turn off clock
		msWrite4Byte(mtk_dp, 0x11210010, 0x00000000); //turn off afe
	}
}

void mhal_DPTx_audio_sample_arrange(struct mtk_dp *mtk_dp, BYTE bEnable)
{
	uint32_t value = 0;

	// 0x3374 [12] = enable
	// 0x3374 [11:0] = hblank * link_rate(MHZ) / pix_clk(MHZ) / 4 * 0.8
	value = (mtk_dp->info.DPTX_OUTBL.Htt - mtk_dp->info.DPTX_OUTBL.Hde) *
		mtk_dp->training_info.ubLinkRate * 27 * 200 /
		mtk_dp->info.DPTX_OUTBL.PixRateKhz;

	if (bEnable) {
		msWrite4ByteMask(mtk_dp,
			REG_3370_DP_ENCODER1_P0 + 4, BIT(12), BIT(12));
		msWrite4ByteMask(mtk_dp,
			REG_3370_DP_ENCODER1_P0 + 4, (uint16_t)value, BITMASK(11:0));
	} else {
		msWrite4ByteMask(mtk_dp,
			REG_3370_DP_ENCODER1_P0 + 4, 0, BIT(12));
		msWrite4ByteMask(mtk_dp,
			REG_3370_DP_ENCODER1_P0 + 4, 0, BITMASK(11:0));
	}
	DPTXMSG("Htt=%d, Hde=%d, ubLinkRate=%d, PixRateKhz=%lu\n",
		mtk_dp->info.DPTX_OUTBL.Htt, mtk_dp->info.DPTX_OUTBL.Hde,
		mtk_dp->training_info.ubLinkRate, mtk_dp->info.DPTX_OUTBL.PixRateKhz);

	DPTXMSG("Audio arrange patch enable = %d, value = 0x%x\n", bEnable, value);
}

void mhal_DPTx_Audio_PG_EN(struct mtk_dp *mtk_dp, BYTE Channel,
	BYTE Fs, BYTE bEnable)
{
	if (bEnable) {
		msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
			AU_GEN_EN_DP_ENCODER0_P0_FLDMASK,
			AU_GEN_EN_DP_ENCODER0_P0_FLDMASK);
		//[9 : 8] set 0x3 : PG	mtk_dp
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
			AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK,
			AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK);

	} else {
		msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
			0, AU_GEN_EN_DP_ENCODER0_P0_FLDMASK);
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
			0, AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK);
	}
	DPTXMSG("fs = %d, ch = %d\n", Fs, Channel);

	//audio channel count change reset
	msWriteByteMask(mtk_dp, REG_33F4_DP_ENCODER1_P0 + 1, BIT(1), BIT(1));

	msWrite2ByteMask(mtk_dp, REG_3304_DP_ENCODER1_P0,
		AU_PRTY_REGEN_DP_ENCODER1_P0_FLDMASK,
		AU_PRTY_REGEN_DP_ENCODER1_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3304_DP_ENCODER1_P0,
		AU_CH_STS_REGEN_DP_ENCODER1_P0_FLDMASK,
		AU_CH_STS_REGEN_DP_ENCODER1_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3304_DP_ENCODER1_P0,
		AUDIO_SAMPLE_PRSENT_REGEN_DP_ENCODER1_P0_FLDMASK,
		AUDIO_SAMPLE_PRSENT_REGEN_DP_ENCODER1_P0_FLDMASK);

	msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
		AUDIO_2CH_SEL_DP_ENCODER0_P0_FLDMASK,
		AUDIO_2CH_SEL_DP_ENCODER0_P0_FLDMASK); //[15]
	msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
		AUDIO_MN_GEN_EN_DP_ENCODER0_P0_FLDMASK,
		AUDIO_MN_GEN_EN_DP_ENCODER0_P0_FLDMASK); // [12]
	msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
		AUDIO_8CH_SEL_DP_ENCODER0_P0_FLDMASK,
		AUDIO_8CH_SEL_DP_ENCODER0_P0_FLDMASK); //[8]
	msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
		AU_EN_DP_ENCODER0_P0_FLDMASK,
		AU_EN_DP_ENCODER0_P0_FLDMASK); //[6]

	switch (Fs) {
	case FS_44K:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
			0x0 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS,
			AUDIO_PATTERN_GEN_FS_SEL_DP_ENCODER1_P0_FLDMASK);
		break;
	case FS_48K:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
			0x1 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS,
			AUDIO_PATTERN_GEN_FS_SEL_DP_ENCODER1_P0_FLDMASK);
		break;
	case FS_192K:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
			0x2 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS,
			AUDIO_PATTERN_GEN_FS_SEL_DP_ENCODER1_P0_FLDMASK);
		break;
	default:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
			0x0 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS,
			AUDIO_PATTERN_GEN_FS_SEL_DP_ENCODER1_P0_FLDMASK);
	break;
	}

	msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
		0, AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
		0, AUDIO_8CH_EN_DP_ENCODER0_P0_FLDMASK);

	switch (Channel) {
	case 2:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
			0x0 << AUDIO_PATGEN_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS,
			AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
			AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK,
			AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK);
		break;
	case 8:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
			0x1 << AUDIO_PATGEN_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS,
			AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
			AUDIO_8CH_EN_DP_ENCODER0_P0_FLDMASK,
			AUDIO_8CH_EN_DP_ENCODER0_P0_FLDMASK);
		break;
	case 16:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
			0x2 << AUDIO_PATGEN_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS,
			AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		break;
	case 32:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
			0x3 << AUDIO_PATGEN_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS,
			AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		break;
	default:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
			0x0 << AUDIO_PATGEN_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS,
			AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		break;
	}

	//audio channel count change reset
	msWriteByteMask(mtk_dp, REG_33F4_DP_ENCODER1_P0 + 1, 0, BIT(1));

	//enable audio reset
	msWriteByteMask(mtk_dp, REG_33F4_DP_ENCODER1_P0, BIT(0), BIT(0));

	// enable audio sample arrange
	mhal_DPTx_audio_sample_arrange(mtk_dp, TRUE);
}

void mhal_DPTx_Audio_TDM_PG_EN(struct mtk_dp *mtk_dp, BYTE Channel,
	BYTE Fs, BYTE bEnable)
{
	DPTXMSG("TDM_PG_EN enable = %d\n", bEnable);
	if (bEnable) {
		msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
				 AU_GEN_EN_DP_ENCODER0_P0_FLDMASK,
				 AU_GEN_EN_DP_ENCODER0_P0_FLDMASK);

		//[9 : 8] set 0x3 : PG	mtk_dp
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
				0x3 << AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK_POS,
				AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK);
		msWrite2ByteMask(mtk_dp, REG_331C_DP_ENCODER1_P0,
				0x0, TDM_AUDIO_DATA_EN_DP_ENCODER1_P0_FLDMASK);
	}

	else {
		msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
				 0, AU_GEN_EN_DP_ENCODER0_P0_FLDMASK);
		//[ 9 : 8] set 0x0 : dprx, for Source project, it means for front-end audio
		//[10 : 8] set 0x4 : TDM after (include) Posnot
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
				0x4 << AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK_POS
				, AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK);
		//[0]: TDM to DPTX transfer enable
		msWrite2ByteMask(mtk_dp, REG_331C_DP_ENCODER1_P0,
				TDM_AUDIO_DATA_EN_DP_ENCODER1_P0_FLDMASK,
				TDM_AUDIO_DATA_EN_DP_ENCODER1_P0_FLDMASK);
		//[12:8]: TDM audio data 32 bit
		//32bit:0x1F
		//24bit:0x17
		//20bit:0x13
		//16bit:0x0F
		msWrite2ByteMask(mtk_dp, REG_331C_DP_ENCODER1_P0,
				(0x1F << TDM_AUDIO_DATA_BIT_DP_ENCODER1_P0_FLDMASK_POS),
				TDM_AUDIO_DATA_BIT_DP_ENCODER1_P0_FLDMASK);
	}

	msWriteByteMask(mtk_dp, REG_33F4_DP_ENCODER1_P0, 0, BIT(0));

	DPTXMSG("fs = %d, ch = %d\n", Fs, Channel);

	//audio channel count change reset
	msWriteByteMask(mtk_dp, (REG_33F4_DP_ENCODER1_P0 + 1), BIT(1), BIT(1));

	msWrite2ByteMask(mtk_dp, REG_3304_DP_ENCODER1_P0,
			 AU_PRTY_REGEN_DP_ENCODER1_P0_FLDMASK,
			 AU_PRTY_REGEN_DP_ENCODER1_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3304_DP_ENCODER1_P0,
			 AU_CH_STS_REGEN_DP_ENCODER1_P0_FLDMASK,
			 AU_CH_STS_REGEN_DP_ENCODER1_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3304_DP_ENCODER1_P0,
			 AUDIO_SAMPLE_PRSENT_REGEN_DP_ENCODER1_P0_FLDMASK,
			 AUDIO_SAMPLE_PRSENT_REGEN_DP_ENCODER1_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
			 AUDIO_2CH_SEL_DP_ENCODER0_P0_FLDMASK,
			 AUDIO_2CH_SEL_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
			 AUDIO_MN_GEN_EN_DP_ENCODER0_P0_FLDMASK,
			 AUDIO_MN_GEN_EN_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
			 AUDIO_8CH_SEL_DP_ENCODER0_P0_FLDMASK,
			 AUDIO_8CH_SEL_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
			 AU_EN_DP_ENCODER0_P0_FLDMASK,
			 AU_EN_DP_ENCODER0_P0_FLDMASK);

	if (!bEnable) {
		msWrite2ByteMask(mtk_dp, REG_3040_DP_ENCODER0_P0,
			AUDIO_16CH_SEL_DP_ENCODER0_P0_FLDMASK,
			AUDIO_16CH_SEL_DP_ENCODER0_P0_FLDMASK);
		msWrite2ByteMask(mtk_dp, REG_3040_DP_ENCODER0_P0,
			AUDIO_32CH_SEL_DP_ENCODER0_P0_FLDMASK,
			AUDIO_32CH_SEL_DP_ENCODER0_P0_FLDMASK);
	}

	switch (Fs) {
	case FS_44K:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
				 (0x0 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_FS_SEL_DP_ENCODER1_P0_FLDMASK);
		break;

	case FS_48K:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
				 (0x1 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_FS_SEL_DP_ENCODER1_P0_FLDMASK);
		break;

	case FS_192K:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
				 (0x2 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_FS_SEL_DP_ENCODER1_P0_FLDMASK);
		break;

	default:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
				 (0x0 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_FS_SEL_DP_ENCODER1_P0_FLDMASK);
		break;
	}

	msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0, 0, AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0, 0, AUDIO_8CH_EN_DP_ENCODER0_P0_FLDMASK);

	if (!bEnable) {
		msWrite2ByteMask(mtk_dp, REG_3040_DP_ENCODER0_P0, 0,
			AUDIO_16CH_EN_DP_ENCODER0_P0_FLDMASK);
		msWrite2ByteMask(mtk_dp, REG_3040_DP_ENCODER0_P0, 0,
			AUDIO_32CH_EN_DP_ENCODER0_P0_FLDMASK);
	}
	switch (Channel) {
	case 2:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
				 (0x0 << AUDIO_PATGEN_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
				(0x1 << AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK_POS),
				AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK);
		if (!bEnable)	//TDM audio interface, audio channel number, 1: 2ch
			msWrite2ByteMask(mtk_dp, REG_331C_DP_ENCODER1_P0,
				(0x1 << TDM_AUDIO_DATA_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS),
				TDM_AUDIO_DATA_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		break;

	case 8:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
				 (0x1 << AUDIO_PATGEN_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
				 (0x1 << AUDIO_8CH_EN_DP_ENCODER0_P0_FLDMASK_POS),
				 AUDIO_8CH_EN_DP_ENCODER0_P0_FLDMASK);
		if (!bEnable) {	//TDM audio interface, audio channel number, 7: 8ch
			if (mtk_dp->cfg_ver == 2) {
				msWrite2ByteMask(mtk_dp, REG_331C_DP_ENCODER1_P0,
				(0x1 << TDM_AUDIO_DATA_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS),
				TDM_AUDIO_DATA_CH_NUM_DP_ENCODER1_P0_FLDMASK);
			} else {
				msWrite2ByteMask(mtk_dp, REG_331C_DP_ENCODER1_P0,
				(0x7 << TDM_AUDIO_DATA_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS),
				TDM_AUDIO_DATA_CH_NUM_DP_ENCODER1_P0_FLDMASK);
			}
		}
		break;

	case 16:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
				 (0x2 << AUDIO_PATGEN_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		if (!bEnable)
			msWrite2ByteMask(mtk_dp, REG_3040_DP_ENCODER0_P0,
				(0x1 << AUDIO_16CH_EN_DP_ENCODER0_P0_FLDMASK_POS),
				AUDIO_16CH_EN_DP_ENCODER0_P0_FLDMASK);
		break;

	case 32:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
				 (0x3 << AUDIO_PATGEN_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		if (!bEnable)
			msWrite2ByteMask(mtk_dp, REG_3040_DP_ENCODER0_P0,
				(0x1 << AUDIO_32CH_EN_DP_ENCODER0_P0_FLDMASK_POS),
				AUDIO_32CH_EN_DP_ENCODER0_P0_FLDMASK);
		break;

	default:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
				 (0x0 << AUDIO_PATGEN_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		if (!bEnable) {
			msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
				(0x1 << AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK_POS),
				AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK);
			//TDM audio interface, audio channel number, 1: 2ch
			msWrite2ByteMask(mtk_dp, REG_331C_DP_ENCODER1_P0,
				(0x1 << TDM_AUDIO_DATA_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS),
				TDM_AUDIO_DATA_CH_NUM_DP_ENCODER1_P0_FLDMASK);
			}
		break;
	}
	if (!bEnable) {
		//TDM to DPTX reset [1]
		msWriteByteMask(mtk_dp, (REG_331C_DP_ENCODER1_P0),
				TDM_AUDIO_RST_DP_ENCODER1_P0_FLDMASK,
				TDM_AUDIO_RST_DP_ENCODER1_P0_FLDMASK);
		msWrite2ByteMask(mtk_dp, (REG_3004_DP_ENCODER0_P0),
				0x1 << SDP_RESET_SW_DP_ENCODER0_P0_FLDMASK_POS,
				SDP_RESET_SW_DP_ENCODER0_P0_FLDMASK);

		udelay(5);
		msWriteByteMask(mtk_dp, (REG_331C_DP_ENCODER1_P0),
				0x0, TDM_AUDIO_RST_DP_ENCODER1_P0_FLDMASK);
		msWrite2ByteMask(mtk_dp, (REG_3004_DP_ENCODER0_P0),
				0,
				SDP_RESET_SW_DP_ENCODER0_P0_FLDMASK);
	}
	//audio channel count change reset
	msWriteByteMask(mtk_dp, (REG_33F4_DP_ENCODER1_P0 + 1), 0, BIT(1));
	// enable audio sample arrange
	mhal_DPTx_audio_sample_arrange(mtk_dp, TRUE);
}

#if (DPTX_AutoTest_ENABLE == 0x1) && (DPTX_PHY_TEST_PATTERN_EN == 0x1)
void mhal_DPTx_AudioClock(struct mtk_dp *mtk_dp, BYTE Channel, BYTE Fs)
{
	int bck, mck;
	int div4, divb;
	int clk_source = 196608000, clk_value; //KHZ

	switch (Fs) {
	case FS_44K:
		clk_value = 44100;
		break;
	case FS_48K:
		clk_value = 48000;
		break;
	case FS_96K:
		clk_value = 96000;
		break;
	case FS_192K:
		clk_value = 192000;
		break;
	default:
		clk_value = 44100;
		break;
	}

	bck = clk_value * Channel * 32;
	mck = clk_value * 256;
	div4 = mck / bck - 1;
	divb = clk_source / mck - 1;
	DPTXMSG("bck = %d, mck = %d, div4 = %d, divb=%d\n",
		bck, mck, div4, divb);

	msWrite4Byte(mtk_dp, 0x1000c320, 0x6F28BD4C);
	msWrite4Byte(mtk_dp, 0x1000c040, 0x6F28BD4D);
	msWrite4Byte(mtk_dp, 0x1000c328, 0x00000001);
	msWrite4Byte(mtk_dp, 0x1000c318, 0x000003e1);
	msWrite4Byte(mtk_dp, 0x1000c31c, 0x84000000);
	msWrite4Byte(mtk_dp, 0x1000c334, 0x78FD5264);
	msWrite4Byte(mtk_dp, 0x1000c044, 0x78FD5265);
	msWrite4Byte(mtk_dp, 0x1000c33C, 0x00000001);
	msWrite4Byte(mtk_dp, 0x1000c32C, 0x000003E1);
	msWrite4Byte(mtk_dp, 0x1000c330, 0x84000000);
	msWrite4Byte(mtk_dp, 0x1000c00c, 0x0000003F);
	msWrite4Byte(mtk_dp, 0x100000d4, 0x01000000);
	msWrite4Byte(mtk_dp, 0x100000e4, 0x00000001);
	msWrite4Byte(mtk_dp, 0x10000008, 0x00300000);
	msWrite4Byte(mtk_dp, 0x10000328, div4 << 8 | divb);
	msWrite4Byte(mtk_dp, 0x10000320, 0x00001000);
}
#endif

void mhal_DPTx_Audio_Ch_Status_Set(struct mtk_dp *mtk_dp, BYTE Channel,
	BYTE Fs, BYTE Wordlength)
{
	union DPRX_AUDIO_CHSTS AudChSts;

	memset(&AudChSts, 0, sizeof(AudChSts));

	switch (Fs) {
	case FS_32K:
		AudChSts.iec_ch_sts.SamplingFreq = 3;
		break;
	case FS_44K:
		AudChSts.iec_ch_sts.SamplingFreq = 0;
		break;
	case FS_48K:
		AudChSts.iec_ch_sts.SamplingFreq = 2;
		break;
	case FS_88K:
		AudChSts.iec_ch_sts.SamplingFreq = 8;
		break;
	case FS_96K:
		AudChSts.iec_ch_sts.SamplingFreq = 0xA;
		break;
	case FS_192K:
		AudChSts.iec_ch_sts.SamplingFreq = 0xE;
		break;
	default:
		AudChSts.iec_ch_sts.SamplingFreq = 0x1;
		break;
	}

	switch (Wordlength) {
	case WL_16bit:
		AudChSts.iec_ch_sts.WordLen = 0x02;
		break;
	case WL_20bit:
		AudChSts.iec_ch_sts.WordLen = 0x03;
		break;
	case WL_24bit:
		AudChSts.iec_ch_sts.WordLen = 0x0B;
		break;
	}

	msWrite2Byte(mtk_dp, REG_308C_DP_ENCODER0_P0,
		AudChSts.AUD_CH_STS[1] << 8 | AudChSts.AUD_CH_STS[0]);
	msWrite2Byte(mtk_dp, REG_3090_DP_ENCODER0_P0,
		AudChSts.AUD_CH_STS[3] << 8 | AudChSts.AUD_CH_STS[2]);
	msWriteByte(mtk_dp, REG_3094_DP_ENCODER0_P0,
		AudChSts.AUD_CH_STS[4]);
}

void mhal_DPTx_Audio_SDP_Setting(struct mtk_dp *mtk_dp, BYTE Channel)
{
	msWriteByteMask(mtk_dp, REG_312C_DP_ENCODER0_P0,
		0x00, 0xFF);	//[7 : 0] //HB2

	if (Channel == 8)
		msWrite2ByteMask(mtk_dp, REG_312C_DP_ENCODER0_P0,
			0x0700, 0xFF00);//[15 : 8]channel-1
	else
		msWrite2ByteMask(mtk_dp, REG_312C_DP_ENCODER0_P0,
			0x0100, 0xFF00);
}

void mhal_DPTx_Audio_M_Divider_Setting(struct mtk_dp *mtk_dp, BYTE Div)
{
	msWrite2ByteMask(mtk_dp, REG_30BC_DP_ENCODER0_P0,
		Div << AUDIO_M_CODE_MULT_DIV_SEL_DP_ENCODER0_P0_FLDMASK_POS,
		AUDIO_M_CODE_MULT_DIV_SEL_DP_ENCODER0_P0_FLDMASK);
}

bool mhal_DPTx_GetHPDPinLevel(struct mtk_dp *mtk_dp)
{
	bool ret = (msReadByte(mtk_dp, REG_3414_DP_TRANS_P0) & BIT(2)) ? 1 : 0;

	return ret | mtk_dp->bPowerOn;
}

void mhal_DPTx_SPKG_SDP(struct mtk_dp *mtk_dp, bool bEnable, BYTE ucSDPType,
	BYTE *pHB, BYTE *pDB)
{
	BYTE ucDBOffset;
	WORD ucSTOffset;
	BYTE ucpHBOffset;
	BYTE bRegIndex;

	if (bEnable) {
		for (ucDBOffset = 0; ucDBOffset < 0x10; ucDBOffset++)
			for (bRegIndex = 0; bRegIndex < 2; bRegIndex++) {
				u32 addr = REG_3200_DP_ENCODER1_P0
					+ ucDBOffset * 4 + bRegIndex;

				msWriteByte(mtk_dp, addr,
					pDB[ucDBOffset * 2 + bRegIndex]);
			}

		if (ucSDPType == DPTx_SDPTYP_DRM)
			for (ucpHBOffset = 0; ucpHBOffset < 4/2; ucpHBOffset++)
				for (bRegIndex = 0;
					bRegIndex < 2; bRegIndex++) {
					u32 addr = REG_3138_DP_ENCODER0_P0
						+ ucpHBOffset * 4 + bRegIndex;
					BYTE pOffset = ucpHBOffset * 2
						+ bRegIndex;

					msWriteByte(mtk_dp, addr, pHB[pOffset]);
					DPTXMSG("W Reg addr: %x, index %d\n",
						addr, pOffset);
				}
		else if (ucSDPType >= DPTx_SDPTYP_PPS0
			&& ucSDPType <= DPTx_SDPTYP_PPS3) {
			for (ucpHBOffset = 0; ucpHBOffset < (4/2);
				ucpHBOffset++)
				for (bRegIndex = 0; bRegIndex < 2;
						bRegIndex++) {
					u32 addr = REG_3130_DP_ENCODER0_P0
						+ ucpHBOffset * 4 + bRegIndex;
					BYTE pOffset = ucpHBOffset * 2
						+ bRegIndex;

					msWriteByte(mtk_dp, addr, pHB[pOffset]);
					DPTXMSG("W H1 Reg addr:%x,index:%d\n",
						addr, pOffset);
				}
		} else {
			ucSTOffset = (ucSDPType - DPTx_SDPTYP_ACM) * 8;
			for (ucpHBOffset = 0; ucpHBOffset < 4/2; ucpHBOffset++)
				for (bRegIndex = 0; bRegIndex < 2;
					bRegIndex++) {
					u32 addr = REG_30D8_DP_ENCODER0_P0
						+ ucSTOffset
						+ ucpHBOffset * 4 + bRegIndex;
					BYTE pOffset = ucpHBOffset * 2
						+ bRegIndex;

					msWriteByte(mtk_dp, addr, pHB[pOffset]);
					DPTXMSG("W H2 Reg addr: %x,index %d\n",
						addr, pOffset);
				}
		}
	}

	switch (ucSDPType) {
	case DPTx_SDPTYP_NONE:
		break;
	case DPTx_SDPTYP_ACM:
		msWriteByte(mtk_dp, REG_30B4_DP_ENCODER0_P0, 0x00);

		if (bEnable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				DPTx_SDPTYP_ACM, BIT(4)|BIT(3)|BIT(2)|BIT(1)|BIT(0));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_30B4_DP_ENCODER0_P0, 0x05);
			DPTXMSG("SENT SDP TYPE ACM\n");
		}
		break;
	case DPTx_SDPTYP_ISRC:
		msWriteByte(mtk_dp, REG_30B4_DP_ENCODER0_P0 + 1, 0x00);
		if (bEnable) {
			msWriteByte(mtk_dp, REG_31EC_DP_ENCODER0_P0 + 1, 0x1C);
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				DPTx_SDPTYP_ISRC, BIT(4)|BIT(3)|BIT(2)|BIT(1)|BIT(0));
			if (pHB[3] & BIT(2))
				msWriteByteMask(mtk_dp, REG_30BC_DP_ENCODER0_P0,
					BIT(0), BIT(0));
			else
				msWriteByteMask(mtk_dp, REG_30BC_DP_ENCODER0_P0,
					0, BIT(0));

			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_30B4_DP_ENCODER0_P0 + 1, 0x05);
			DPTXMSG("SENT SDP TYPE ISRC\n");
		}
		break;
	case DPTx_SDPTYP_AVI:
		msWriteByte(mtk_dp, REG_30A4_DP_ENCODER0_P0 + 1, 0x00);
		if (bEnable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				DPTx_SDPTYP_AVI, BIT(4)|BIT(3)|BIT(2)|BIT(1)|BIT(0));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_30A4_DP_ENCODER0_P0 + 1, 0x05);
			DPTXMSG("SENT SDP TYPE AVI\n");
		}
		break;
	case DPTx_SDPTYP_AUI:
		msWriteByte(mtk_dp, REG_30A8_DP_ENCODER0_P0, 0x00);
		if (bEnable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				DPTx_SDPTYP_AUI, BIT(4)|BIT(3)|BIT(2)|BIT(1)|BIT(0));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_30A8_DP_ENCODER0_P0, 0x05);
			DPTXMSG("SENT SDP TYPE AUI\n");
		}
		break;
	case DPTx_SDPTYP_SPD:
		msWriteByte(mtk_dp, REG_30A8_DP_ENCODER0_P0 + 1, 0x00);
		if (bEnable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				DPTx_SDPTYP_SPD, BIT(4)|BIT(3)|BIT(2)|BIT(1)|BIT(0));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_30A8_DP_ENCODER0_P0 + 1, 0x05);
			DPTXMSG("SENT SDP TYPE SPD\n");
		}
		break;
	case DPTx_SDPTYP_MPEG:
		msWriteByte(mtk_dp, REG_30AC_DP_ENCODER0_P0, 0x00);
		if (bEnable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				DPTx_SDPTYP_MPEG, BIT(4)|BIT(3)|BIT(2)|BIT(1)|BIT(0));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_30AC_DP_ENCODER0_P0, 0x05);
			DPTXMSG("SENT SDP TYPE MPEG\n");
		}
		break;
	case DPTx_SDPTYP_NTSC:
		msWriteByte(mtk_dp, REG_30AC_DP_ENCODER0_P0 + 1, 0x00);
		if (bEnable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				DPTx_SDPTYP_NTSC, BIT(4)|BIT(3)|BIT(2)|BIT(1)|BIT(0));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_30AC_DP_ENCODER0_P0 + 1, 0x05);
			DPTXMSG("SENT SDP TYPE NTSC\n");
		}
		break;
	case DPTx_SDPTYP_VSP:
		msWriteByte(mtk_dp, REG_30B0_DP_ENCODER0_P0, 0x00);
		if (bEnable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				DPTx_SDPTYP_VSP, BIT(4)|BIT(3)|BIT(2)|BIT(1)|BIT(0));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_30B0_DP_ENCODER0_P0, 0x05);
			DPTXMSG("SENT SDP TYPE VSP\n");
		}
		break;
	case DPTx_SDPTYP_VSC:
		msWriteByte(mtk_dp, REG_30B8_DP_ENCODER0_P0, 0x00);
		if (bEnable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				DPTx_SDPTYP_VSC, BIT(4)|BIT(3)|BIT(2)|BIT(1)|BIT(0));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_30B8_DP_ENCODER0_P0, 0x05);
			DPTXMSG("SENT SDP TYPE VSC\n");
		}
		break;
	case DPTx_SDPTYP_EXT:
		msWriteByte(mtk_dp, REG_30B0_DP_ENCODER0_P0 + 1, 0x00);
		if (bEnable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				DPTx_SDPTYP_EXT, BIT(4)|BIT(3)|BIT(2)|BIT(1)|BIT(0));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_30B0_DP_ENCODER0_P0 + 1, 0x05);
			DPTXMSG("SENT SDP TYPE EXT\n");
		}
		break;
	case DPTx_SDPTYP_PPS0:
		msWriteByte(mtk_dp, REG_31E8_DP_ENCODER0_P0, 0x00);
		if (bEnable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				DPTx_SDPTYP_PPS0, BIT(4)|BIT(3)|BIT(2)|BIT(1)|BIT(0));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_31E8_DP_ENCODER0_P0, 0x05);
			DPTXMSG("SENT SDP TYPE PPS0\n");
		}
		break;
	case DPTx_SDPTYP_PPS1:
		msWriteByte(mtk_dp, REG_31E8_DP_ENCODER0_P0, 0x00);
		if (bEnable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				DPTx_SDPTYP_PPS1, BIT(4)|BIT(3)|BIT(2)|BIT(1)|BIT(0));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_31E8_DP_ENCODER0_P0, 0x05);
			DPTXMSG("SENT SDP TYPE PPS1\n");
		}
		break;
	case DPTx_SDPTYP_PPS2:
		msWriteByte(mtk_dp, REG_31E8_DP_ENCODER0_P0, 0x00);
		if (bEnable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				DPTx_SDPTYP_PPS2, BIT(4)|BIT(3)|BIT(2)|BIT(1)|BIT(0));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_31E8_DP_ENCODER0_P0, 0x05);
			DPTXMSG("SENT SDP TYPE PPS2\n");
		}
		break;
	case DPTx_SDPTYP_PPS3:
		msWriteByte(mtk_dp, REG_31E8_DP_ENCODER0_P0, 0x00);
		if (bEnable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				DPTx_SDPTYP_PPS3, BIT(4)|BIT(3)|BIT(2)|BIT(1)|BIT(0));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_31E8_DP_ENCODER0_P0, 0x05);
			DPTXMSG("SENT SDP TYPE PPS3\n");
		}
		break;
	case DPTx_SDPTYP_DRM:
		msWriteByte(mtk_dp, REG_31DC_DP_ENCODER0_P0, 0x00);
		if (bEnable) {
			msWriteByte(mtk_dp, REG_3138_DP_ENCODER0_P0, pHB[0]);
			msWriteByte(mtk_dp, REG_3138_DP_ENCODER0_P0 + 1,
				pHB[1]);
			msWriteByte(mtk_dp, REG_313C_DP_ENCODER0_P0, pHB[2]);
			msWriteByte(mtk_dp, REG_313C_DP_ENCODER0_P0 + 1,
				pHB[3]);
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				DPTx_SDPTYP_DRM, BIT(4)|BIT(3)|BIT(2)|BIT(1)|BIT(0));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0,
				BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_31DC_DP_ENCODER0_P0, 0x05);
			DPTXMSG("SENT SDP TYPE DRM\n");
		}
		break;
	default:
		break;
	}
}

void mhal_DPTx_SPKG_VSC_EXT_VESA(struct mtk_dp *mtk_dp, bool bEnable,
	BYTE ucHDR_NUM, BYTE *pDB)
{

	BYTE VSC_HB1 = 0x20;	// VESA : 0x20; CEA : 0x21
	BYTE VSC_HB2;
	BYTE ucPkgCnt;
	WORD ucDBOffset;
	BYTE ucDPloop;
	BYTE ucpDBOffset;
	BYTE bRegIndex;

	if (!bEnable) {
		msWriteByteMask(mtk_dp, REG_30A0_DP_ENCODER0_P0 + 1, 0, BIT(0));
		msWriteByteMask(mtk_dp, REG_328C_DP_ENCODER1_P0, 0, BIT(7));
		return;
	}

	VSC_HB2 = (ucHDR_NUM > 0) ? BIT(6) : 0x00;

	msWriteByte(mtk_dp, REG_31C8_DP_ENCODER0_P0, 0x00);
	msWriteByte(mtk_dp, REG_31C8_DP_ENCODER0_P0 + 1, VSC_HB1);
	msWriteByte(mtk_dp, REG_31CC_DP_ENCODER0_P0, VSC_HB2);
	msWriteByte(mtk_dp, REG_31CC_DP_ENCODER0_P0 + 1, 0x00);
	msWriteByte(mtk_dp, REG_31D8_DP_ENCODER0_P0, ucHDR_NUM);

	msWriteByteMask(mtk_dp, REG_328C_DP_ENCODER1_P0, BIT(0), BIT(0));
	msWriteByteMask(mtk_dp, REG_328C_DP_ENCODER1_P0, BIT(2), BIT(2));
	udelay(50);
	msWriteByteMask(mtk_dp, REG_328C_DP_ENCODER1_P0, 0, BIT(2));
	udelay(50);

	for (ucPkgCnt = 0; ucPkgCnt < (ucHDR_NUM+1); ucPkgCnt++) {
		ucDBOffset = 0;
		for (ucDPloop = 0; ucDPloop < 4; ucDPloop++) {
			for (ucpDBOffset = 0; ucpDBOffset < 8/2; ucpDBOffset++)
				for (bRegIndex = 0; bRegIndex < 2;
						bRegIndex++) {
					u32 addr = REG_3290_DP_ENCODER1_P0
						+ ucpDBOffset * 4 + bRegIndex;
					BYTE pOffset = ucDBOffset
						+ ucpDBOffset * 2 + bRegIndex;

					msWriteByte(mtk_dp,
					addr,
					pDB[pOffset]);
				}
			msWriteByteMask(mtk_dp, REG_328C_DP_ENCODER1_P0,
				BIT(6), BIT(6));
			ucDBOffset += 8;
		}
	}

	msWriteByteMask(mtk_dp, REG_30A0_DP_ENCODER0_P0 + 1, BIT(0), BIT(0));
	msWriteByteMask(mtk_dp, REG_328C_DP_ENCODER1_P0, BIT(7), BIT(7));
}

void mhal_DPTx_SPKG_VSC_EXT_CEA(struct mtk_dp *mtk_dp, bool bEnable,
	BYTE ucHDR_NUM, BYTE *pDB)
{

	BYTE VSC_HB1 = 0x21;
	BYTE VSC_HB2;
	BYTE ucPkgCnt;
	WORD ucDBOffset;
	BYTE ucDPloop;
	BYTE ucpDBOffset;
	BYTE bRegIndex;

	if (!bEnable) {
		msWriteByteMask(mtk_dp, REG_30A0_DP_ENCODER0_P0 + 1, 0, BIT(4));
		msWriteByteMask(mtk_dp, REG_32A0_DP_ENCODER1_P0, 0, BIT(7));
		return;
	}

	VSC_HB2 = (ucHDR_NUM > 0) ? 0x40 : 0x00;

	msWriteByte(mtk_dp, REG_31D0_DP_ENCODER0_P0, 0x00);
	msWriteByte(mtk_dp, REG_31D0_DP_ENCODER0_P0 + 1, VSC_HB1);
	msWriteByte(mtk_dp, REG_31D4_DP_ENCODER0_P0, VSC_HB2);
	msWriteByte(mtk_dp, REG_31D4_DP_ENCODER0_P0 + 1, 0x00);
	msWriteByte(mtk_dp, REG_31D8_DP_ENCODER0_P0 + 1, ucHDR_NUM);

	msWriteByteMask(mtk_dp, REG_32A0_DP_ENCODER1_P0, BIT(0), BIT(0));
	msWriteByteMask(mtk_dp, REG_32A0_DP_ENCODER1_P0, BIT(2), BIT(2));
	udelay(50);

	msWriteByteMask(mtk_dp, REG_32A0_DP_ENCODER1_P0, 0, BIT(2));

	for (ucPkgCnt = 0; ucPkgCnt < (ucHDR_NUM+1); ucPkgCnt++) {
		ucDBOffset = 0;
		for (ucDPloop = 0; ucDPloop < 4; ucDPloop++) {
			for (ucpDBOffset = 0; ucpDBOffset < 8/2; ucpDBOffset++)
				for (bRegIndex = 0; bRegIndex < 2;
						bRegIndex++) {
					u32 addr = REG_32A4_DP_ENCODER1_P0
						+ ucpDBOffset * 4 + bRegIndex;
					BYTE pOffset = ucDBOffset
						+ ucpDBOffset * 2 + bRegIndex;

					msWriteByte(mtk_dp,
						addr,
						pDB[pOffset]);
				}
			msWriteByteMask(mtk_dp, REG_32A0_DP_ENCODER1_P0,
				BIT(6), BIT(6));
			ucDBOffset += 8;
		}
	}


	msWriteByteMask(mtk_dp, REG_30A0_DP_ENCODER0_P0 + 1, BIT(4), BIT(4));
	msWriteByteMask(mtk_dp, REG_32A0_DP_ENCODER1_P0, BIT(7), BIT(7));
}

UINT8 mhal_DPTx_AuxRead_Bytes(struct mtk_dp *mtk_dp, BYTE ubCmd,
	DWORD  usDPCDADDR, size_t ubLength, BYTE *pRxBuf)
{
	bool bVaildCmd = false;
	BYTE ubReplyCmd = 0xFF;
	BYTE ubRdCount = 0x0;
	BYTE uAuxIrqStatus = 0;
	UINT8 ret = AUX_HW_FAILED;
	unsigned int WaitReplyCount = AuxWaitReplyLpCntNum;
	u32 aux_state = 0;

	if (mtk_dp->fake_comeplete_irq && ((ubCmd == AUX_CMD_NATIVE_R) || (ubCmd == AUX_CMD_I2C_R))) {
		if (mtk_dp->cfg_ver == 2)
			msWrite4ByteMask(mtk_dp, REG_36F4_AUX_TX_P0,
				1<<IDLE_TO_PRECHARGE_DATA_ONE_EN_AUX_TX_P0_FLDMASK_POS,
				IDLE_TO_PRECHARGE_DATA_ONE_EN_AUX_TX_P0_FLDMASK);
		else
			msWriteByteMask(mtk_dp, REG_3614_AUX_TX_P0, 0x18, MASKBIT(6 : 0));
	}
	msWriteByte(mtk_dp, REG_3640_AUX_TX_P0, 0x7F);
	udelay(AUX_WRITE_READ_WAIT_TIME);

	if ((ubLength > 16) ||
		((ubCmd == AUX_CMD_NATIVE_R) && (ubLength == 0x0)))
		return AUX_INVALID_CMD;

	msWriteByte(mtk_dp, REG_3650_AUX_TX_P0 + 1, 0x01);
	msWriteByte(mtk_dp, REG_3644_AUX_TX_P0, ubCmd);
	msWrite2Byte(mtk_dp, REG_3648_AUX_TX_P0, usDPCDADDR&0x0000FFFF);
	msWriteByte(mtk_dp, REG_364C_AUX_TX_P0, (usDPCDADDR>>16)&0x0000000F);

	if (ubLength > 0) {
		msWrite2ByteMask(mtk_dp, REG_3650_AUX_TX_P0,
			(ubLength-1) << MCU_REQ_DATA_NUM_AUX_TX_P0_FLDMASK_POS,
			MCU_REQUEST_DATA_NUM_AUX_TX_P0_FLDMASK);
		msWriteByte(mtk_dp, REG_362C_AUX_TX_P0, 0x00);
	}


	if ((ubCmd == AUX_CMD_I2C_R) || (ubCmd == AUX_CMD_I2C_R_MOT0))
		if (ubLength == 0x0)
			msWrite2ByteMask(mtk_dp, REG_362C_AUX_TX_P0,
				0x01 << AUX_NO_LENGTH_AUX_TX_P0_FLDMASK_POS,
				AUX_NO_LENGTH_AUX_TX_P0_FLDMASK);

	msWrite2ByteMask(mtk_dp, REG_3630_AUX_TX_P0,
		0x01 << AUX_TX_REQUEST_READY_AUX_TX_P0_FLDMASK_POS,
		AUX_TX_REQUEST_READY_AUX_TX_P0_FLDMASK);

	while (--WaitReplyCount) {
		uAuxIrqStatus = msReadByte(mtk_dp, REG_3640_AUX_TX_P0) & 0xFF;
		if (uAuxIrqStatus & AUX_RX_RECV_COMPLETE_IRQ_TX_P0_FLDMASK) {
			aux_state = (msRead2Byte(mtk_dp, REG_3644_AUX_TX_P0)
				& AUX_STATE_AUX_TX_P0_FLDMASK)
				>> AUX_STATE_AUX_TX_P0_FLDMASK_POS;

			if (aux_state == 0x1) {
				DPTXDBG("[AUX] Aux Recv Complete!\n");
				bVaildCmd = true;
				break;
			}
			msWriteByte(mtk_dp, REG_3640_AUX_TX_P0, 0x7F);
			mtk_dp->fake_comeplete_irq = true;
			DPTXERR("[AUX] Fake Complete irq! Clear IRQ\n");
		}

		if (uAuxIrqStatus & AUX_RX_EDID_RECV_COMPLETE_IRQ_AUX_TX_P0_FLDMASK) {
			if (ubCmd == AUX_CMD_I2C_R) {
				aux_state = (msRead2Byte(mtk_dp, REG_3644_AUX_TX_P0)
					& AUX_STATE_AUX_TX_P0_FLDMASK)
					>> AUX_STATE_AUX_TX_P0_FLDMASK_POS;

				if (aux_state == 0x1) {
					bVaildCmd = true;
					DPTXDBG("[AUX] AUX_RX_EDID_RECV_COMPLETE_IRQ\n");
					break;
				}
				mtk_dp->fake_comeplete_irq = true;
				DPTXERR("[AUX] Fake Complete irq in EDID read! Clear IRQ\n");
			}
			msWriteByteMask(mtk_dp, REG_3640_AUX_TX_P0,
			1 << AUX_RX_EDID_RECV_COMPLETE_IRQ_AUX_TX_P0_FLDMASK_POS,
			AUX_RX_EDID_RECV_COMPLETE_IRQ_AUX_TX_P0_FLDMASK);
			DPTXERR("[AUX] Un-expected EDID complete IRQ\n");
		}

		if (uAuxIrqStatus & AUX_400US_TIMEOUT_IRQ_AUX_TX_P0_FLDMASK) {
			usleep_range(AUX_NO_REPLY_WAIT_TIME, AUX_NO_REPLY_WAIT_TIME);
			DPTXMSG("(AUX Read)HW Timeout 400us irq");
			break;
		}
	}

	if (mtk_dp->fake_comeplete_irq && ((ubCmd == AUX_CMD_NATIVE_R) || (ubCmd == AUX_CMD_I2C_R))) {
		if (mtk_dp->cfg_ver == 2)
			msWrite4ByteMask(mtk_dp, REG_36F4_AUX_TX_P0, 0x0,
				IDLE_TO_PRECHARGE_DATA_ONE_EN_AUX_TX_P0_FLDMASK);
		else
			msWriteByteMask(mtk_dp, REG_3614_AUX_TX_P0, 0xD, MASKBIT(6 : 0));
	}

	if (WaitReplyCount == 0x0) {
		BYTE phyStatus = 0x00;

		phyStatus = msReadByte(mtk_dp, REG_3628_AUX_TX_P0);
		if (phyStatus != 0x01)
			DPTXERR("Aux R:Aux hang,need SW reset\n");

		msWrite2ByteMask(mtk_dp, REG_3650_AUX_TX_P0,
			0x01 << MCU_ACK_TRAN_COMPLETE_AUX_TX_P0_FLDMASK_POS,
			MCU_ACK_TRANSACTION_COMPLETE_AUX_TX_P0_FLDMASK);
		msWriteByte(mtk_dp, REG_3640_AUX_TX_P0, 0x7F);

		//udelay(AUX_WRITE_READ_WAIT_TIME);
		DPTXMSG("WaitReplyCount =%x TimeOut", WaitReplyCount);
		return AUX_HW_FAILED;
	}

	ubReplyCmd = msReadByte(mtk_dp, REG_3624_AUX_TX_P0) & 0x0F;
	if (ubReplyCmd)
		DPTXMSG("ubReplyCmd =%x NACK or Defer\n", ubReplyCmd);

	if (ubLength == 0) {
		msWriteByte(mtk_dp, REG_362C_AUX_TX_P0, 0x00);
	}

	if (ubReplyCmd == AUX_REPLY_ACK) {
		msWrite2ByteMask(mtk_dp, REG_3620_AUX_TX_P0,
			0x0 << AUX_RD_MODE_AUX_TX_P0_FLDMASK_POS,
			AUX_RD_MODE_AUX_TX_P0_FLDMASK);

		for (ubRdCount = 0x0; ubRdCount < ubLength;
				ubRdCount++) {
			msWrite2ByteMask(mtk_dp, REG_3620_AUX_TX_P0,
			0x01 << AUX_RX_FIFO_R_PULSE_TX_P0_FLDMASK_POS,
			AUX_RX_FIFO_READ_PULSE_TX_P0_FLDMASK);

			*(pRxBuf + ubRdCount)
				= msReadByte(mtk_dp,
					REG_3620_AUX_TX_P0);
		}
	}

	msWrite2ByteMask(mtk_dp, REG_3650_AUX_TX_P0,
		0x01 << MCU_ACK_TRAN_COMPLETE_AUX_TX_P0_FLDMASK_POS,
		MCU_ACK_TRANSACTION_COMPLETE_AUX_TX_P0_FLDMASK);
	msWriteByte(mtk_dp, REG_3640_AUX_TX_P0, 0x7F);

	if (bVaildCmd) {
		//DPTXDBG("[AUX] Read reply_cmd = %d\n", ubReplyCmd);
		ret = ubReplyCmd;
	} else {
		DPTXMSG("[AUX] Timeout Read reply_cmd = %d\n", ubReplyCmd);
		ret = AUX_HW_FAILED;
	}

	return ret;
}

UINT8 mhal_DPTx_AuxWrite_Bytes(struct mtk_dp *mtk_dp,
	BYTE ubCmd, DWORD  usDPCDADDR, size_t ubLength, BYTE *pData)
{
	bool bVaildCmd = false;
	BYTE ubReplyCmd = 0x0;
	BYTE i;
	WORD WaitReplyCount = AuxWaitReplyLpCntNum;
	BYTE bRegIndex;
	UINT8 ret = AUX_HW_FAILED;

	if ((ubLength > 16) || ((ubCmd == AUX_CMD_NATIVE_W) && (ubLength == 0x0)))
		return AUX_INVALID_CMD;

	msWriteByteMask(mtk_dp, REG_3704_AUX_TX_P0,
		1 << AUX_TX_FIFO_NEW_MODE_EN_AUX_TX_P0_FLDMASK_POS,
		AUX_TX_FIFO_NEW_MODE_EN_AUX_TX_P0_FLDMASK);
	msWriteByte(mtk_dp, REG_3650_AUX_TX_P0 + 1, 0x01);
	msWriteByte(mtk_dp, REG_3640_AUX_TX_P0, 0x7F);
	udelay(AUX_WRITE_READ_WAIT_TIME);

	msWriteByte(mtk_dp, REG_3650_AUX_TX_P0 + 1, 0x01);
	msWriteByte(mtk_dp, REG_3644_AUX_TX_P0, ubCmd);
	msWriteByte(mtk_dp, REG_3648_AUX_TX_P0, usDPCDADDR & 0x00FF);
	msWriteByte(mtk_dp, REG_3648_AUX_TX_P0 + 1, (usDPCDADDR >> 8) & 0x00FF);
	msWriteByte(mtk_dp, REG_364C_AUX_TX_P0, (usDPCDADDR >> 16) & 0x000F);

	if (ubLength > 0) {
		msWriteByte(mtk_dp, REG_362C_AUX_TX_P0, 0x00);
		for (i = 0x0; i < (ubLength+1)/2; i++)
			for (bRegIndex = 0; bRegIndex < 2; bRegIndex++)
				if ((i * 2 + bRegIndex) < ubLength)
					msWriteByte(mtk_dp,
					REG_3708_AUX_TX_P0 + i * 4 + bRegIndex,
					pData[i * 2 + bRegIndex]);
		msWriteByte(mtk_dp, REG_3650_AUX_TX_P0 + 1,
			((ubLength - 1) & 0x0F) << 4);
	} else
		msWriteByte(mtk_dp, REG_362C_AUX_TX_P0, 0x01);

	msWriteByteMask(mtk_dp, REG_3704_AUX_TX_P0,
		AUX_TX_FIFO_WRITE_DATA_NEW_MODE_TOGGLE_AUX_TX_P0_FLDMASK,
		AUX_TX_FIFO_WRITE_DATA_NEW_MODE_TOGGLE_AUX_TX_P0_FLDMASK);
	msWriteByte(mtk_dp, REG_3630_AUX_TX_P0, 0x08);

	while (--WaitReplyCount) {
		BYTE uAuxIrqStatus;

		uAuxIrqStatus = msReadByte(mtk_dp, REG_3640_AUX_TX_P0) & 0xFF;
		udelay(1);
		if (uAuxIrqStatus & AUX_RX_RECV_COMPLETE_IRQ_TX_P0_FLDMASK) {
			bVaildCmd = true;
			break;
		}

		if (uAuxIrqStatus & AUX_400US_TIMEOUT_IRQ_AUX_TX_P0_FLDMASK) {
			usleep_range(AUX_NO_REPLY_WAIT_TIME, AUX_NO_REPLY_WAIT_TIME);
			DPTXMSG("(AUX write)HW Timeout 400us irq");
			break;
		}
	}

	if (WaitReplyCount == 0x0) {
		BYTE phyStatus = 0x00;

		phyStatus = msReadByte(mtk_dp, REG_3628_AUX_TX_P0);
		if (phyStatus != 0x01)
			DPTXERR("Aux Write: Aux hang, need SW reset!\n");

		msWriteByte(mtk_dp, REG_3650_AUX_TX_P0 + 1, 0x01);
		msWriteByte(mtk_dp, REG_3640_AUX_TX_P0, 0x7F);

		//udelay(AUX_WRITE_READ_WAIT_TIME);

		DPTXMSG("ubReplyCmd = 0x%x WaitReplyCount = %d\n",
			ubReplyCmd, WaitReplyCount);
		return AUX_HW_FAILED;
	}

	ubReplyCmd = msReadByte(mtk_dp, REG_3624_AUX_TX_P0) & 0x0F;
	if (ubReplyCmd)
		DPTXMSG("ubReplyCmd =%x NACK or Defer\n", ubReplyCmd);

	msWriteByte(mtk_dp, REG_3650_AUX_TX_P0 + 1, 0x01);

	if (ubLength == 0)
		msWriteByte(mtk_dp, REG_362C_AUX_TX_P0, 0x00);

	msWriteByte(mtk_dp, REG_3640_AUX_TX_P0, 0x7F);

	if (bVaildCmd) {
		//DPTXMSG("[AUX] Write reply_cmd = %d\n", ubReplyCmd);
		ret = ubReplyCmd;
	} else {
		DPTXMSG("[AUX] Timeout Write reply_cmd = %d\n", ubReplyCmd);
		ret = AUX_HW_FAILED;
	}

	return ret;
}

bool mhal_DPTx_SetSwingtPreEmphasis(struct mtk_dp *mtk_dp, int lane_num,
	int swingValue, int preEmphasis)
{
	DPTXMSG("lane%d, set Swing = %x, Emp =%x\n", lane_num,
		swingValue, preEmphasis);

	switch (lane_num) {
	case DPTx_LANE0:
		if (mtk_dp->cfg_ver == 2) {
			msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
					(swingValue << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
					DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
			msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
					(preEmphasis << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
					DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
		} else {
			msWrite4ByteMask(mtk_dp,
			DP_TX_TOP_SWING_EMP,
			swingValue << DP_TX0_VOLT_SWING_FLDMASK_POS,
			DP_TX0_VOLT_SWING_FLDMASK);
			msWrite4ByteMask(mtk_dp,
				DP_TX_TOP_SWING_EMP,
				preEmphasis << DP_TX0_PRE_EMPH_FLDMASK_POS,
				DP_TX0_PRE_EMPH_FLDMASK);
		}
		break;
	case DPTx_LANE1:
		if (mtk_dp->cfg_ver == 2) {
			msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
					(swingValue << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
					DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
			msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
					(preEmphasis << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
					DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
		} else {
			msWrite4ByteMask(mtk_dp,
			DP_TX_TOP_SWING_EMP,
			swingValue << DP_TX1_VOLT_SWING_FLDMASK_POS,
			DP_TX1_VOLT_SWING_FLDMASK);
			msWrite4ByteMask(mtk_dp,
				DP_TX_TOP_SWING_EMP,
				preEmphasis << DP_TX1_PRE_EMPH_FLDMASK_POS,
				DP_TX1_PRE_EMPH_FLDMASK);
		}
		break;
	case DPTx_LANE2:
		if (mtk_dp->cfg_ver == 2) {
			msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
					(swingValue << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
					DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
			msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
					(preEmphasis << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
					DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
		} else {
			msWrite4ByteMask(mtk_dp,
			DP_TX_TOP_SWING_EMP,
			swingValue << DP_TX2_VOLT_SWING_FLDMASK_POS,
			DP_TX2_VOLT_SWING_FLDMASK);
			msWrite4ByteMask(mtk_dp,
				DP_TX_TOP_SWING_EMP,
				preEmphasis << DP_TX2_PRE_EMPH_FLDMASK_POS,
				DP_TX2_PRE_EMPH_FLDMASK);
		}
		break;
	case DPTx_LANE3:
		if (mtk_dp->cfg_ver == 2) {
			msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
					(swingValue << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
					DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
			msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
					(preEmphasis << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
					DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
		} else {
			msWrite4ByteMask(mtk_dp,
			DP_TX_TOP_SWING_EMP,
			swingValue << DP_TX3_VOLT_SWING_FLDMASK_POS,
			DP_TX3_VOLT_SWING_FLDMASK);
			msWrite4ByteMask(mtk_dp,
				DP_TX_TOP_SWING_EMP,
				preEmphasis << DP_TX3_PRE_EMPH_FLDMASK_POS,
				DP_TX3_PRE_EMPH_FLDMASK);
		}
		break;
	default:
		DPTXERR("lane number is error\n");
		return false;
	}

	return true;
}


bool mhal_DPTx_ResetSwingtPreEmphasis(struct mtk_dp *mtk_dp)
{
	if (mtk_dp->cfg_ver == 2) {
		msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
				(0x1 << DP_TX_FORCE_VOLT_SWING_EN_FLDMASK_POS),
				DP_TX_FORCE_VOLT_SWING_EN_FLDMASK);
		msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
				(0x1 << DP_TX_FORCE_PRE_EMPH_EN_FLDMASK_POS),
				DP_TX_FORCE_PRE_EMPH_EN_FLDMASK);
		msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
				(0x1 << DP_TX_FORCE_VOLT_SWING_EN_FLDMASK_POS),
				DP_TX_FORCE_VOLT_SWING_EN_FLDMASK);
		msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
				(0x1 << DP_TX_FORCE_PRE_EMPH_EN_FLDMASK_POS),
				DP_TX_FORCE_PRE_EMPH_EN_FLDMASK);
		msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
				(0x1 << DP_TX_FORCE_VOLT_SWING_EN_FLDMASK_POS),
				DP_TX_FORCE_VOLT_SWING_EN_FLDMASK);
		msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
				(0x1 << DP_TX_FORCE_PRE_EMPH_EN_FLDMASK_POS),
				DP_TX_FORCE_PRE_EMPH_EN_FLDMASK);
		msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
				(0x1 << DP_TX_FORCE_VOLT_SWING_EN_FLDMASK_POS),
				DP_TX_FORCE_VOLT_SWING_EN_FLDMASK);
		msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
				(0x1 << DP_TX_FORCE_PRE_EMPH_EN_FLDMASK_POS),
				DP_TX_FORCE_PRE_EMPH_EN_FLDMASK);

		msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
				(0x0 << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
				DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
		msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
				(0x0  << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
				DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
		msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
				(0x0 << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
				DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
		msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
				(0x0 << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
				DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
		msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
				(0x0 << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
				DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
		msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
				(0x0 << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
				DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
		msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
				(0x0 << DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK_POS),
				DP_TX_FORCE_VOLT_SWING_VAL_FLDMASK);
		msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
				(0x0 << DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK_POS),
				DP_TX_FORCE_PRE_EMPH_VAL_FLDMASK);
	} else {
		msWrite4ByteMask(mtk_dp,
		DP_TX_TOP_SWING_EMP, 0, DP_TX0_VOLT_SWING_FLDMASK);
		msWrite4ByteMask(mtk_dp,
			DP_TX_TOP_SWING_EMP, 0, DP_TX1_VOLT_SWING_FLDMASK);
		msWrite4ByteMask(mtk_dp,
			DP_TX_TOP_SWING_EMP, 0, DP_TX2_VOLT_SWING_FLDMASK);
		msWrite4ByteMask(mtk_dp,
			DP_TX_TOP_SWING_EMP, 0, DP_TX3_VOLT_SWING_FLDMASK);
		msWrite4ByteMask(mtk_dp,
			DP_TX_TOP_SWING_EMP, 0, DP_TX0_PRE_EMPH_FLDMASK);
		msWrite4ByteMask(mtk_dp,
			DP_TX_TOP_SWING_EMP, 0, DP_TX1_PRE_EMPH_FLDMASK);
		msWrite4ByteMask(mtk_dp,
			DP_TX_TOP_SWING_EMP, 0, DP_TX2_PRE_EMPH_FLDMASK);
		msWrite4ByteMask(mtk_dp,
			DP_TX_TOP_SWING_EMP, 0, DP_TX3_PRE_EMPH_FLDMASK);
	}

	return true;
}

void mhal_DPTx_ISR(struct mtk_dp *mtk_dp)
{
	static DWORD AuxIrqCnt;
	static DWORD TransIrqCnt;
	static DWORD EncIrqCnt;

	uint32_t int_status;

	int_status = msRead4Byte(mtk_dp, DP_TX_TOP_IRQ_STATUS);
	DPTXDBG("int_status = 0x%x\n", int_status);

	if (int_status & BIT(2))
		AuxIrqCnt++;

	if (int_status & BIT(1)) {
		mdrv_DPTx_HPD_ISREvent(mtk_dp);
		TransIrqCnt++;
	}

	if (int_status & BIT(0))
		EncIrqCnt++;

	DPTXDBG("AuxIrqCnt:%lu, TransIrqCnt:%lu, EncIrqCnt:%lu\n",
		AuxIrqCnt, TransIrqCnt, EncIrqCnt);
}

void mhal_DPTx_EnableFEC(struct mtk_dp *mtk_dp, bool bENABLE)
{
	DPTXFUNC("Fec enable=%d\n", bENABLE);
	if (bENABLE)
		msWriteByteMask(mtk_dp, REG_3540_DP_TRANS_P0, BIT(0), BIT(0));
	else
		msWriteByteMask(mtk_dp, REG_3540_DP_TRANS_P0, 0, BIT(0));
}

void mhal_DPTx_EnableDSC(struct mtk_dp *mtk_dp, bool bENABLE)
{
	DPTXFUNC("DSC enable=%d\n", bENABLE);
	if (bENABLE) {
		msWriteByteMask(mtk_dp,
			REG_336C_DP_ENCODER1_P0,
			BIT(0),
			BIT(0)); // [0] : DSC Enable
		msWriteByteMask(mtk_dp,
			REG_300C_DP_ENCODER0_P0 + 1,
			BIT(1),
			BIT(1)); //300C [9] : VB-ID[6] DSC enable
		msWriteByteMask(mtk_dp,
			REG_303C_DP_ENCODER0_P0 + 1,
			0x7,
			MASKBIT(2 : 0)); //303C[10 : 8] : DSC color depth
		msWriteByteMask(mtk_dp,
			REG_303C_DP_ENCODER0_P0 + 1,
			0x7 << 4,
			MASKBIT(6 : 4)); //303C[14 : 12] : DSC color format
		msWriteByteMask(mtk_dp,
			REG_31FC_DP_ENCODER0_P0 + 1,
			BIT(4),
			BIT(4)); //31FC[12] : HDE last num control
	} else {
		msWriteByteMask(mtk_dp,
			REG_336C_DP_ENCODER1_P0,
			0,
			BIT(0)); // DSC Disable
		msWriteByteMask(mtk_dp,
			REG_300C_DP_ENCODER0_P0 + 1,
			0,
			BIT(1));
		msWriteByteMask(mtk_dp,
			REG_303C_DP_ENCODER0_P0 + 1,
			0x3,
			MASKBIT(2 : 0)); //default 8bit
		msWriteByteMask(mtk_dp,
			REG_303C_DP_ENCODER0_P0 + 1,
			0x0,
			MASKBIT(6 : 4)); //default RGB
		msWriteByteMask(mtk_dp,
			REG_31FC_DP_ENCODER0_P0 + 1,
			0x0,
			BIT(4));
	}
}

void mhal_DPTx_SetChunkSize(struct mtk_dp *mtk_dp,
	BYTE slice_num, WORD chunk_num, BYTE remainder,
	BYTE lane_count, BYTE hde_last_num, BYTE hde_num_even)
{
	msWriteByteMask(mtk_dp,
		REG_336C_DP_ENCODER1_P0,
		slice_num << 4,
		MASKBIT(7 : 4));
	msWriteByteMask(mtk_dp,
		REG_336C_DP_ENCODER1_P0 + 1,
		remainder,
		MASKBIT(3 : 0));
	msWrite2Byte(mtk_dp,
		REG_3370_DP_ENCODER1_P0,
		chunk_num); //set chunk_num

	if (lane_count == 1) {
		msWriteByteMask(mtk_dp,
			REG_31FC_DP_ENCODER0_P0,
			hde_last_num,
			MASKBIT(1 : 0)); //last data catch on lane 0
		msWriteByteMask(mtk_dp,
			REG_31FC_DP_ENCODER0_P0 + 1,
			hde_num_even,
			BIT(0)); //sram last data catch on lane 0
	} else {
		msWriteByteMask(mtk_dp,
			REG_31FC_DP_ENCODER0_P0,
			hde_last_num,
			MASKBIT(1 : 0));
		msWriteByteMask(mtk_dp,
			REG_31FC_DP_ENCODER0_P0,
			hde_last_num << 2,
			MASKBIT(3 : 2));
		msWriteByteMask(mtk_dp,
			REG_31FC_DP_ENCODER0_P0 + 1,
			hde_num_even,
			BIT(0));
		msWriteByteMask(mtk_dp,
			REG_31FC_DP_ENCODER0_P0 + 1,
			hde_num_even << 1,
			BIT(1));
	}
}

void mhal_DPTx_Fake_Plugin(struct mtk_dp *mtk_dp, bool conn)
{
	if (conn) {
		msWriteByteMask(mtk_dp, REG_3414_DP_TRANS_P0, 0,
			HPD_OVR_EN_DP_TRANS_P0_FLDMASK);
		msWriteByteMask(mtk_dp, REG_3414_DP_TRANS_P0, 0,
			HPD_SET_DP_TRANS_P0_FLDMASK);
	} else {
		msWriteByteMask(mtk_dp, REG_3414_DP_TRANS_P0,
			HPD_OVR_EN_DP_TRANS_P0_FLDMASK,
			HPD_OVR_EN_DP_TRANS_P0_FLDMASK);
		msWriteByteMask(mtk_dp, REG_3414_DP_TRANS_P0, 0,
			HPD_SET_DP_TRANS_P0_FLDMASK);
	}
}

void mhal_DPTx_USBC_HPD(struct mtk_dp *mtk_dp, bool conn)
{
	msWriteByteMask(mtk_dp,
		REG_3414_DP_TRANS_P0,
		HPD_OVR_EN_DP_TRANS_P0_FLDMASK,
		HPD_OVR_EN_DP_TRANS_P0_FLDMASK);

	if (conn)
		msWriteByteMask(mtk_dp,
			REG_3414_DP_TRANS_P0,
			HPD_SET_DP_TRANS_P0_FLDMASK,
			HPD_SET_DP_TRANS_P0_FLDMASK);
	else
		msWriteByteMask(mtk_dp,
			REG_3414_DP_TRANS_P0,
			0,
			HPD_SET_DP_TRANS_P0_FLDMASK);

	DPTXFUNC("REG3414 = 0x%x\n", msReadByte(mtk_dp, REG_3414_DP_TRANS_P0));
}

WORD mhal_DPTx_GetSWIRQStatus(struct mtk_dp *mtk_dp)
{
	return msRead2Byte(mtk_dp, REG_35D0_DP_TRANS_P0);
}

void mhal_DPTx_SWInterruptSet(struct mtk_dp *mtk_dp, WORD bstatus)
{
	msWrite2Byte(mtk_dp, REG_35C0_DP_TRANS_P0, bstatus);

}
void mhal_DPTx_SWInterruptClr(struct mtk_dp *mtk_dp, WORD bstatus)
{
	msWrite2Byte(mtk_dp, REG_35C8_DP_TRANS_P0, bstatus);
	msWrite2Byte(mtk_dp, REG_35C8_DP_TRANS_P0, 0);

}

void mhal_DPTx_SWInterruptEnable(struct mtk_dp *mtk_dp, bool enable)
{
	if (enable)
		msWrite2Byte(mtk_dp, REG_35C4_DP_TRANS_P0, 0);
	else
		msWrite2Byte(mtk_dp, REG_35C4_DP_TRANS_P0, 0xFFFF);
}

BYTE mhal_DPTx_GetHPDIRQStatus(struct mtk_dp *mtk_dp)
{
	return (msReadByte(mtk_dp, REG_3418_DP_TRANS_P0 + 1) & 0xE0) >> 4;
}

void mhal_DPTx_HPDInterruptClr(struct mtk_dp *mtk_dp, BYTE bstatus)
{
	DPTXFUNC();
	msWriteByteMask(mtk_dp,
		REG_3418_DP_TRANS_P0,
		bstatus,
		BIT(3)|BIT(2)|BIT(1));
	msWriteByteMask(mtk_dp,
		REG_3418_DP_TRANS_P0,
		0,
		BIT(3)|BIT(2)|BIT(1));
}

void mhal_DPTx_HPDInterruptEnable(struct mtk_dp *mtk_dp, bool enable)
{
	DPTXFUNC();
	// [7]:int[6]:Con[5]DisCon[4]No-Use:UnMASK HPD Port
	if (enable)
		msWriteByteMask(mtk_dp,
			REG_3418_DP_TRANS_P0,
			0,
			BIT(7)|BIT(6)|BIT(5));
	else
		msWriteByteMask(mtk_dp,
			REG_3418_DP_TRANS_P0,
			BIT(7)|BIT(6)|BIT(5),
			BIT(7)|BIT(6)|BIT(5));
}

void mhal_DPTx_HPDDetectSetting(struct mtk_dp *mtk_dp)
{
	if (mtk_dp->cfg_ver == 2) {
		//Crystal frequency value for 1us timing normalization
		//[7:2]: Integer value
		//[1:0]: Fractional value
		//0x30: 12.0 us //0x68: 26us
		msWrite2ByteMask(mtk_dp, REG_366C_AUX_TX_P0,
				0x68 << XTAL_FREQ_AUX_TX_P0_FLDMASK_POS,
				XTAL_FREQ_AUX_TX_P0_FLDMASK);

		//Adjust Tx reg_hpd_disc_thd to 2ms, it is because of the spec. "HPD pulse" description
		//Low Bound: 3'b010 ~ 500us
		//Up Bound: 3'b110 ~1.9ms
		msWrite2ByteMask(mtk_dp, REG_364C_AUX_TX_P0,
			(0x32 << HPD_INT_THD_AUX_TX_P0_FLDMASK_POS),
			HPD_INT_THD_AUX_TX_P0_FLDMASK);
		msWrite2ByteMask(mtk_dp, REG_364C_AUX_TX_P0,
			(0x32 << HPD_INT_THD_AUX_TX_P0_FLDMASK_POS),
			HPD_INT_THD_AUX_TX_P0_FLDMASK);
		/* dptx phy setting for usbc */
		msWrite4ByteMask(mtk_dp, REG_364C_AUX_TX_P0, BIT(11), BIT(11));
		msWrite4ByteMask(mtk_dp, REG_364C_AUX_TX_P0, BIT(10), BIT(10));
	} else {
		msWriteByteMask(mtk_dp,
			REG_3410_DP_TRANS_P0,
			0x8,
			MASKBIT(3 : 0));
		msWriteByteMask(mtk_dp,
			REG_3410_DP_TRANS_P0,
			(0x0A << 4),
			MASKBIT(7 : 4));

		// [7 : 4] Con Thd = 1.5ms+Vx0.1ms[3 : 0] : DisCon Thd = 1.5ms+Vx0.1ms
		msWriteByte(mtk_dp,
			REG_3410_DP_TRANS_P0 + 1,
			0x55);
		msWriteByte(mtk_dp,
			REG_3430_DP_TRANS_P0,
			0x02); //1113 MK
	}
}

void mhal_DPTx_PhyCheckReady(struct mtk_dp *mtk_dp, u8 lane_count)
{
	UINT8 i;
	UINT8 phyd_rdy_status;
	UINT8 phyd_rdy_bmp = (RGS_BIAS_READY_FLDMASK | RGS_TX_LN0_READY_FLDMASK);

	switch (lane_count) {
	case 4:
		phyd_rdy_bmp |= (RGS_TX_LN3_READY_FLDMASK |
				RGS_TX_LN2_READY_FLDMASK);
		break;
	case 2:
		phyd_rdy_bmp |= RGS_TX_LN1_READY_FLDMASK;
		break;
	default:
		break;
	}

	for (i = 0; i < 100; i++) {
		phyd_rdy_status = msPhyReadByte(mtk_dp, PHYD_DIG_GLB_OFFSET +
							DP_PHY_DIG_GLB_STATUS_02);
		phyd_rdy_status &= phyd_rdy_bmp;

		if (phyd_rdy_status == phyd_rdy_bmp) {
			DPTXMSG("Polling tx_ln status pass\n");
			return;
		}
		DPTXMSG("Polling tx_ln status %x\n", phyd_rdy_status);
	}
	DPTXERR("Polling tx_ln status fail %x\n", phyd_rdy_status);
}

void mhal_DPTx_PhyDPowerOn(struct mtk_dp *mtk_dp)
{
	UINT8 i;
	UINT8 phyd_rdy_status;

	msPhyWriteByteMask(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_0,
			0x1 << FORCE_PWR_STATE_EN_FLDMASK_POS, FORCE_PWR_STATE_EN_FLDMASK);
	msPhyWriteByteMask(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_0,
			0x3 << FORCE_PWR_STATE_VAL_FLDMASK_POS, FORCE_PWR_STATE_VAL_FLDMASK);

	for (i = 0; i < 100; i++) {
		phyd_rdy_status = msPhyReadByte(mtk_dp, PHYD_DIG_GLB_OFFSET +
							DP_PHY_DIG_GLB_STATUS_02);
		phyd_rdy_status &= RGS_BIAS_READY_FLDMASK;

		if (phyd_rdy_status == RGS_BIAS_READY_FLDMASK) {
			DPTXMSG("DPTX PHYD power on\n");
			return;
		}
		DPTXMSG("Polling BIAS status %x\n", phyd_rdy_status);
	}
	DPTXERR("Polling BIAS status fail %x\n", phyd_rdy_status);
}

void mhal_DPTx_PhySetLanePower(struct mtk_dp *mtk_dp, u8 lane_count)
{
	// ***===power ON flow===***
	// for 4Lane: -> 0x8 -> 0xC -> 0xE -> 0xF
	// for 2Lane: -> 0x2 -> 0x3
	// for 1Lane: -> 0x1
	int power_indx = lane_count - 1;
	UINT8 power_bmp = BIT(power_indx);

	do {
		power_bmp |= BIT(power_indx);
		msPhyWriteByteMask(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_TX_CTL_0,
				power_bmp << TX_LN_EN_FLDMASK_POS,
				TX_LN_EN_FLDMASK);
		DPTXMSG("set lane pwr %x\n", (msPhyReadByte(mtk_dp, PHYD_DIG_GLB_OFFSET
						+ DP_PHY_DIG_TX_CTL_0) &
					TX_LN_EN_FLDMASK) >> TX_LN_EN_FLDMASK_POS);
		DPTXMSG("power_indx =%x\n",power_indx);
	} while (--power_indx >= 0);
}

void mhal_DPTx_PhyClearLanePower(struct mtk_dp *mtk_dp)
{
	// ***===power OFF flow===***
	// for 4Lane: -> 0x7 -> 0x3 -> 0x1 -> 0x0
	// for 2Lane: -> 0x1 -> 0x0
	// for 1Lane: -> 0x0
	//UINT8 power_bmp; = (1 << lane_count) - 1;
	UINT8 power_bmp = (msPhyReadByte(mtk_dp, PHYD_DIG_GLB_OFFSET
							+ DP_PHY_DIG_TX_CTL_0) &
						TX_LN_EN_FLDMASK) >> TX_LN_EN_FLDMASK_POS;

	do {
		power_bmp >>= 1;
		msPhyWriteByteMask(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_TX_CTL_0,
				power_bmp << TX_LN_EN_FLDMASK_POS,
				TX_LN_EN_FLDMASK);
		DPTXMSG("clear lane pwr %x\n", (msPhyReadByte(mtk_dp, PHYD_DIG_GLB_OFFSET
							+ DP_PHY_DIG_TX_CTL_0) &
						TX_LN_EN_FLDMASK) >> TX_LN_EN_FLDMASK_POS);
	} while (power_bmp > 0);
}

void mhal_DPTx_PhyPowerDown(struct mtk_dp *mtk_dp)
{
	mhal_DPTx_PhyClearLanePower(mtk_dp);

	msPhyWriteByteMask(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_0,
		0x1 << FORCE_PWR_STATE_EN_FLDMASK_POS, FORCE_PWR_STATE_EN_FLDMASK);
	// power off TPLL and Lane;
	msPhyWriteByteMask(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_0,
		0x1 << FORCE_PWR_STATE_VAL_FLDMASK_POS, FORCE_PWR_STATE_VAL_FLDMASK);

	msPhyWriteByteMask(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_SW_RST, 0, BIT(1)|BIT(3));
	msPhyWriteByteMask(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_SW_RST, BIT(1)|BIT(3), BIT(1)|BIT(3));

	DPTXMSG("DPTX PHYD power down\n");
}

void mhal_DPTx_phyd_power_off(struct mtk_dp *mtk_dp)
{
	mhal_DPTx_PHYD_Reset(mtk_dp);
	msPhyWriteByteMask(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_0,
			0x0, FORCE_PWR_STATE_VAL_FLDMASK);
}

void mhal_DPTx_reset_all(struct mtk_dp *mtk_dp)
{
	mhal_DPTx_PHYD_Reset(mtk_dp);
	msWrite4ByteMask(mtk_dp, 0x2020, 0x0, 0x1);
	mdelay(30);
	msWrite4ByteMask(mtk_dp, 0x2020, 0x1, 0x1);
}

void mhal_DPTx_hw_phy_set_param(struct mtk_dp *mtk_dp, BYTE MAX_LANECOUNT)
{
	//UINT32 value = 0;
	UINT8 i;
	UINT32 usb_info;
	UINT32 usb_info_bit19;
	UINT32 usb_info_bit18;

	//phy threshold refine
	msPhyWrite4ByteMask(mtk_dp, 0x8, 0x00 , BIT(0)|BIT(1));
	msPhyWrite4ByteMask(mtk_dp, 0xc, 0x10 , BIT(4)|BIT(5)|BIT(6));

	/*4Lane issue*/
	msPhyWriteByteMask(mtk_dp, 0x0614, BIT(0), BIT(0));
	msPhyWrite4ByteMask(mtk_dp, 0x0700, 0x0, BIT(20));

	//////////////////// Debug for usb C RG
	usb_info = readl(mtk_dp->ip_mux_regs);
	// Extract the 19th bit of the USB info
	usb_info_bit19 = (usb_info >> 19) & 0x1; // Shift right by 19 bits and mask the LSB
	usb_info_bit18 = (usb_info >> 18) & 0x1; // Shift right by 18 bits and mask the LSB
	// Print the result
	DPTXMSG("USB Info Bit 19(2 lane:0,4 lane:1): %d\n", usb_info_bit19);
	DPTXMSG("USB Info Bit 18(normal:0,flipped:1): %d\n", usb_info_bit18);

	//////////////////// inter-lane skew improvement
	DPTXMSG("DPTX MAX LANE COUNT: %d\n", MAX_LANECOUNT);
	if (MAX_LANECOUNT == 4){
		for (i = 1; i <= 4; i++)
			msPhyWrite4ByteMask(mtk_dp, 0x0100 * i, BIT(12)|BIT(13) ,BIT(12)|BIT(13));
	} else {
		msPhyWrite4ByteMask(mtk_dp, 0x0100, BIT(12) ,BIT(12)|BIT(13));
		msPhyWrite4ByteMask(mtk_dp, 0x0200, BIT(12) ,BIT(12)|BIT(13));
	}
	// phy para provide by SA
	mhal_DPTx_swing_pre_emp_optimized(mtk_dp);
}

void mhal_DPTx_swing_pre_emp_optimized(struct mtk_dp *mtk_dp)
{
	if (mtk_dp->phy_params[0] != 0 || mtk_dp->phy_params[1] != 0 ||
			mtk_dp->phy_params[2] != 0 || mtk_dp->phy_params[3] != 0 ||
			mtk_dp->phy_params[4] != 0 || mtk_dp->phy_params[5] != 0) {
		msPhyWrite4Byte(mtk_dp, 0x1138, mtk_dp->phy_params[0]);
		msPhyWrite4Byte(mtk_dp, 0x1238, mtk_dp->phy_params[0]);
		msPhyWrite4Byte(mtk_dp, 0x1338, mtk_dp->phy_params[0]);
		msPhyWrite4Byte(mtk_dp, 0x1438, mtk_dp->phy_params[0]);

		msPhyWrite4Byte(mtk_dp, 0x113C, mtk_dp->phy_params[1]);
		msPhyWrite4Byte(mtk_dp, 0x123C, mtk_dp->phy_params[1]);
		msPhyWrite4Byte(mtk_dp, 0x133C, mtk_dp->phy_params[1]);
		msPhyWrite4Byte(mtk_dp, 0x143C, mtk_dp->phy_params[1]);

		msPhyWrite4Byte(mtk_dp, 0x1140, mtk_dp->phy_params[2]);
		msPhyWrite4Byte(mtk_dp, 0x1240, mtk_dp->phy_params[2]);
		msPhyWrite4Byte(mtk_dp, 0x1340, mtk_dp->phy_params[2]);
		msPhyWrite4Byte(mtk_dp, 0x1440, mtk_dp->phy_params[2]);

		msPhyWrite4Byte(mtk_dp, 0x1144, mtk_dp->phy_params[3]);
		msPhyWrite4Byte(mtk_dp, 0x1244, mtk_dp->phy_params[3]);
		msPhyWrite4Byte(mtk_dp, 0x1344, mtk_dp->phy_params[3]);
		msPhyWrite4Byte(mtk_dp, 0x1444, mtk_dp->phy_params[3]);

		msPhyWrite4Byte(mtk_dp, 0x1148, mtk_dp->phy_params[4]);
		msPhyWrite4Byte(mtk_dp, 0x1248, mtk_dp->phy_params[4]);
		msPhyWrite4Byte(mtk_dp, 0x1348, mtk_dp->phy_params[4]);
		msPhyWrite4Byte(mtk_dp, 0x1448, mtk_dp->phy_params[4]);

		msPhyWrite4Byte(mtk_dp, 0x114C, mtk_dp->phy_params[5]);
		msPhyWrite4Byte(mtk_dp, 0x124C, mtk_dp->phy_params[5]);
		msPhyWrite4Byte(mtk_dp, 0x134C, mtk_dp->phy_params[5]);
		msPhyWrite4Byte(mtk_dp, 0x144C, mtk_dp->phy_params[5]);
	} else
		DPTXMSG("set phy_params, use default val");
}

void mhal_DPTx_PHYSetting(struct mtk_dp *mtk_dp, BYTE MAX_LANECOUNT)
{
	if (mtk_dp->cfg_ver == 2) {
		mhal_DPTx_hw_phy_set_param(mtk_dp, MAX_LANECOUNT);
		mhal_DPTx_PhyDPowerOn(mtk_dp);
	} else {

		msWrite4ByteMask(mtk_dp,
			DP_TX_TOP_PWR_STATE,
			0x3 << DP_PWR_STATE_FLDMASK_POS, DP_PWR_STATE_FLDMASK);

		msWrite4Byte(mtk_dp, 0x2000, 0x00000001);
		msWrite4Byte(mtk_dp, 0x103C, 0x00000000);
		msWrite4Byte(mtk_dp, 0x2000, 0x00000003);

		// phy para provide by SA
		mhal_DPTx_swing_pre_emp_optimized(mtk_dp);

		//PORTING FROM CTP
		msWrite4ByteMask(mtk_dp, 0x003C, 0x004 << 24, BITMASK(28:24));
		msWrite4ByteMask(mtk_dp, 0x0008, 0x7 << 3, BITMASK(6:3));
		msWrite4ByteMask(mtk_dp, 0x003C, BIT(23), BIT(23));
		msWrite4ByteMask(mtk_dp, 0x0054, BIT(23), BIT(23));
		msWrite4ByteMask(mtk_dp, 0x0054, 0x004 << 24, BITMASK(28:24));
		//PORTING FROM CTP END

		msWrite4ByteMask(mtk_dp, 0x3690, BIT(8), BIT(8));
	}
}

void mhal_DPTx_SSCOnOffSetting(struct mtk_dp *mtk_dp, bool bENABLE)
{
	DPTXMSG("SSC_enable = %d\n", bENABLE);
	if (mtk_dp->cfg_ver == 2) {
		// power off TPLL and Lane;
		msPhyWriteByteMask(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_0,
			0x1 << FORCE_PWR_STATE_VAL_FLDMASK_POS, FORCE_PWR_STATE_VAL_FLDMASK);

		// Set SSC disable
		msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_1, 0x0, TPLL_SSC_EN_FLDMASK);

		//delta1 = 0.05% and delta=0.05%
		// HBR3 8.1G
		msPhyWrite4ByteMask(mtk_dp, 0x10D4, 79 << 16, BITMASK(31:16)); //delta1
		msPhyWrite4ByteMask(mtk_dp, 0x10DC, 49 << 16, BITMASK(31:16)); //delta

		// HBR2 5.4G
		msPhyWrite4ByteMask(mtk_dp, 0x10D4, 105, BITMASK(15:0)); //delta1
		msPhyWrite4ByteMask(mtk_dp, 0x10DC, 65, BITMASK(15:0)); //delta

		// HBR 2.7G
		msPhyWrite4ByteMask(mtk_dp, 0x10D0, 105 << 16, BITMASK(31:16)); //delta1
		msPhyWrite4ByteMask(mtk_dp, 0x10D8, 65 << 16, BITMASK(31:16)); //delta

		// RBR 1.62G
		msPhyWrite4ByteMask(mtk_dp, 0x10D0, 63, BITMASK(15:0)); //delta1
		msPhyWrite4ByteMask(mtk_dp, 0x10D8, 39, BITMASK(15:0)); //delta

		if (bENABLE)
			// Set SSC enable
			msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_1,
					TPLL_SSC_EN_FLDMASK, TPLL_SSC_EN_FLDMASK);
		else
			msPhyWrite4ByteMask(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_1,
					0, TPLL_SSC_EN_FLDMASK);

		// power on BandGap, TPLL and Lane;
		msPhyWriteByteMask(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_0,
			0x3 << FORCE_PWR_STATE_VAL_FLDMASK_POS, FORCE_PWR_STATE_VAL_FLDMASK);
	} else {
		msWrite4ByteMask(mtk_dp, 0x2000, BIT(0), BITMASK(1:0));

		msWrite4ByteMask(mtk_dp, 0x1014, 0x0, BIT(3));

		//delta1 = 0.05% and delta=0.05%
		// HBR3 8.1G
		msWrite4ByteMask(mtk_dp, 0x10D4, 79 << 16, BITMASK(31:16)); //delta1
		msWrite4ByteMask(mtk_dp, 0x10DC, 49 << 16, BITMASK(31:16)); //delta

		// HBR2 5.4G
		msWrite4ByteMask(mtk_dp, 0x10D4, 105, BITMASK(15:0)); //delta1
		msWrite4ByteMask(mtk_dp, 0x10DC, 65, BITMASK(15:0)); //delta

		// HBR 2.7G
		msWrite4ByteMask(mtk_dp, 0x10D0, 105 << 16, BITMASK(31:16)); //delta1
		msWrite4ByteMask(mtk_dp, 0x10D8, 65 << 16, BITMASK(31:16)); //delta

		// RBR 1.62G
		msWrite4ByteMask(mtk_dp, 0x10D0, 63, BITMASK(15:0)); //delta1
		msWrite4ByteMask(mtk_dp, 0x10D8, 39, BITMASK(15:0)); //delta

		if (bENABLE)
			msWrite4ByteMask(mtk_dp, 0x1014, BIT(3), BIT(3));
		else
			msWrite4ByteMask(mtk_dp, 0x1014, 0x0, BIT(3));

		msWrite4ByteMask(mtk_dp, 0x2000, BIT(0)|BIT(1), BITMASK(1:0));
	}
	udelay(50);
}

void mhal_DPTx_SafeModeSetting(struct mtk_dp *mtk_dp)
{
	unsigned long long TU_value_fixed;
	unsigned long long M_value = 0;
	unsigned int shift = 1;
	unsigned int rgb_num = 24;
	unsigned int fixed_point;
	unsigned int SCALE = 1000000;

	DPTXMSG("Link Rate = %u, Lane Count = %u, Depth = %u\n",
	mtk_dp->training_info.ubLinkRate, mtk_dp->training_info.ubLinkLaneCount, mtk_dp->info.depth);
	if (mtk_dp->info.depth == DP_COLOR_DEPTH_6BIT)
		rgb_num = 18;
	else if (mtk_dp->info.depth == DP_COLOR_DEPTH_8BIT)
		rgb_num = 24;
	else if (mtk_dp->info.depth == DP_COLOR_DEPTH_10BIT)
		rgb_num = 30;

	TU_value_fixed = 25200000ULL * 8 * rgb_num;
	DPTXMSG("Initial TU_value_fixed (scaled) = %llu\n", TU_value_fixed);

	msWrite4ByteMask(mtk_dp, 0x304C, 0x0, 0x3);
	msWrite4ByteMask(mtk_dp, 0x303C, 0x8, 0x3F);
	msWrite4ByteMask(mtk_dp, 0x3358, 0x3F, 0x7F);
	msWrite4ByteMask(mtk_dp, 0x3358, 0x80, 0x80);
	msWrite4ByteMask(mtk_dp, 0x335C, 0x0, 0xffffffff);

	if (mtk_dp->training_info.ubLinkRate == DP_LINKRATE_RBR) {
		TU_value_fixed = TU_value_fixed / 162;
		M_value = (25200000ULL * 32768) / (162 * SCALE);
	} else if (mtk_dp->training_info.ubLinkRate == DP_LINKRATE_HBR) {
		TU_value_fixed = TU_value_fixed / 270;
		M_value = (25200000ULL * 32768) / (270 * SCALE);
	} else if (mtk_dp->training_info.ubLinkRate == DP_LINKRATE_HBR2) {
		TU_value_fixed = TU_value_fixed / 540;
		M_value = (25200000ULL * 32768) / (540 * SCALE);
	} else if (mtk_dp->training_info.ubLinkRate == DP_LINKRATE_HBR3) {
		TU_value_fixed = TU_value_fixed / 810;
		M_value = (25200000ULL * 32768) / (810 * SCALE);
	}

	if (mtk_dp->training_info.ubLinkLaneCount == DP_LANECOUNT_1) {
		shift = 32;
	} else if (mtk_dp->training_info.ubLinkLaneCount == DP_LANECOUNT_2) {
		TU_value_fixed = TU_value_fixed / 2;
		shift = 64;
	} else if (mtk_dp->training_info.ubLinkLaneCount == DP_LANECOUNT_4) {
		TU_value_fixed = TU_value_fixed / 4;
		shift = 128;
	}

	TU_value_fixed = TU_value_fixed * shift;
	fixed_point = (unsigned int)(TU_value_fixed / SCALE);

	DPTXMSG("Final TU_value = %u (register value after fixed point conversion)\n", fixed_point);
	DPTXMSG("M_value = 0x%llx\n", M_value);

	msWrite4ByteMask(mtk_dp, 0x3360, fixed_point, 0xffff);
	msWrite4ByteMask(mtk_dp, 0x3004, 0x100, 0x100);
	msWrite4ByteMask(mtk_dp, 0x3008, (unsigned int)M_value, 0xFFFF);
	msWrite4ByteMask(mtk_dp, 0x300C, 0x0, 0xFF);
	msWrite4ByteMask(mtk_dp, 0x3154, 0x7C0, 0xFFFF);
}

void mhal_DPTx_AuxSetting(struct mtk_dp *mtk_dp)
{
	if (mtk_dp->cfg_ver == 2) {
		// modify timeout threshold = 1595 [12 : 8]
		msWrite2ByteMask(mtk_dp,
			REG_360C_AUX_TX_P0,
			0x1D0C,
			AUX_TIMEOUT_THR_AUX_TX_P0_FLDMASK);
		msWriteByteMask(mtk_dp,
			REG_3658_AUX_TX_P0,
			0,
			BIT(0));    //[0]mtk_dp, REG_aux_tx_ov_en

		msWrite2Byte(mtk_dp, REG_36A0_AUX_TX_P0, 0xFFFC);

		//26M
		msWrite2ByteMask(mtk_dp, REG_3634_AUX_TX_P0,
				0x19 << AUX_TX_OVER_SAMPLE_RATE_AUX_TX_P0_FLDMASK_POS,
				AUX_TX_OVER_SAMPLE_RATE_AUX_TX_P0_FLDMASK);
		msWriteByteMask(mtk_dp,
			REG_3614_AUX_TX_P0,
			0x0D,
			MASKBIT(6 : 0));    // Modify, 13 for 26M
		msWrite4ByteMask(mtk_dp,
			REG_37C8_AUX_TX_P0,
			0x01 << MTK_ATOP_EN_AUX_TX_P0_FLDMASK_POS,
			MTK_ATOP_EN_AUX_TX_P0_FLDMASK);
		// disable aux sync_stop detect function
		msWrite4ByteMask(mtk_dp,REG_3690_AUX_TX_P0,
				RX_REPLY_COMPLETE_MODE_AUX_TX_P0_FLDMASK,
				RX_REPLY_COMPLETE_MODE_AUX_TX_P0_FLDMASK);

		//Con Thd = 1.5ms+Vx0.1ms
		msWrite4ByteMask(mtk_dp,REG_367C_AUX_TX_P0,
			5 << HPD_CONN_THD_AUX_TX_P0_FLDMASK_POS,
			HPD_CONN_THD_AUX_TX_P0_FLDMASK);
		//DisCon Thd = 1.5ms+Vx0.1ms
		msWrite4ByteMask(mtk_dp,REG_37A0_AUX_TX_P0,
			5 << HPD_DISC_THD_AUX_TX_P0_FLDMASK_POS,
			HPD_DISC_THD_AUX_TX_P0_FLDMASK);

		msWrite4ByteMask(mtk_dp,REG_3690_AUX_TX_P0,
			RX_REPLY_COMPLETE_MODE_AUX_TX_P0_FLDMASK,
			RX_REPLY_COMPLETE_MODE_AUX_TX_P0_FLDMASK);

		/* dptx phy setting for usbc */
		msWrite4ByteMask(mtk_dp, REG_364C_AUX_TX_P0, BIT(11), BIT(11));
		msWrite4ByteMask(mtk_dp, REG_364C_AUX_TX_P0, BIT(10), BIT(10));
	} else {
		// modify timeout threshold = 1595 [12 : 8]
		msWrite2ByteMask(mtk_dp,
			REG_360C_AUX_TX_P0,
			0x1FFE,  // 630us
			AUX_TIMEOUT_THR_AUX_TX_P0_FLDMASK);
		msWriteByteMask(mtk_dp,
			REG_3658_AUX_TX_P0,
			0,
			BIT(0));    //[0]mtk_dp, REG_aux_tx_ov_en
		msWriteByte(mtk_dp,
			REG_3634_AUX_TX_P0 + 1,
			0x19);  //  25 for 26M
		msWriteByteMask(mtk_dp,
			REG_3614_AUX_TX_P0,
			0x0D,
			MASKBIT(6 : 0));    // Modify, 13 for 26M
		msWrite4ByteMask(mtk_dp,
			REG_37C8_AUX_TX_P0,
			0x01 << MTK_ATOP_EN_AUX_TX_P0_FLDMASK_POS,
			MTK_ATOP_EN_AUX_TX_P0_FLDMASK);
	}
}

static void mhal_DPTx_spkg_asp_hb32(struct mtk_dp *mtk_dp, u8 enable, u8 HB3, u8 HB2)
{

	msWrite2ByteMask(mtk_dp, REG_30BC_DP_ENCODER0_P0 ,
			(enable ? 0x01 : 0x00) << ASP_HB23_SEL_DP_ENCODER0_P0_FLDMASK_POS,
			ASP_HB23_SEL_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_312C_DP_ENCODER0_P0,
			HB2 << ASP_HB2_DP_ENCODER0_P0_FLDMASK_POS,
			ASP_HB2_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_312C_DP_ENCODER0_P0,
			HB3 << ASP_HB3_DP_ENCODER0_P0_FLDMASK_POS,
			ASP_HB3_DP_ENCODER0_P0_FLDMASK);
}

void mtk_dptx_hal_encoder_reset(struct mtk_dp *mtk_dp)
{
	// dp tx encoder reset all sw
	msWrite2ByteMask(mtk_dp, (REG_3004_DP_ENCODER0_P0 ),
			1 << DP_TX_ENCODER_4P_RESET_SW_DP_ENCODER0_P0_FLDMASK_POS,
			DP_TX_ENCODER_4P_RESET_SW_DP_ENCODER0_P0_FLDMASK);
	mdelay(1);

	// dp tx encoder reset all sw
	msWrite2ByteMask(mtk_dp, (REG_3004_DP_ENCODER0_P0),
			0,
			DP_TX_ENCODER_4P_RESET_SW_DP_ENCODER0_P0_FLDMASK);
}

void mhal_DPTx_DigitalSetting(struct mtk_dp *mtk_dp)
{
	DPTXFUNC();
	if (mtk_dp->cfg_ver == 2) {
		mhal_DPTx_spkg_asp_hb32(mtk_dp, FALSE, DPTX_SDP_ASP_HB3_AU02CH, 0x0);
		// Mengkun suggest: disable reg_sdp_down_cnt_new_mode
		msWriteByteMask(mtk_dp, REG_304C_DP_ENCODER0_P0, 0,
						SDP_DOWN_CNT_NEW_MODE_DP_ENCODER0_P0_FLDMASK);
		// reg_sdp_asp_insert_in_hblank: default = 1
		msWrite2ByteMask(mtk_dp, REG_3374_DP_ENCODER1_P0,
			0x1 << SDP_ASP_INSERT_IN_HBLANK_DP_ENCODER1_P0_FLDMASK_POS,
			SDP_ASP_INSERT_IN_HBLANK_DP_ENCODER1_P0_FLDMASK);

		msWriteByteMask(mtk_dp,
			REG_304C_DP_ENCODER0_P0,
			0,
			VBID_VIDEO_MUTE_DP_ENCODER0_P0_FLDMASK);
		mhal_DPTx_SetColorFormat(mtk_dp, DP_COLOR_FORMAT_RGB);//MISC0
		// [13 : 12] : = 2b'01 VDE check BS2BS & set min value
		mhal_DPTx_SetColorDepth(mtk_dp, DP_COLOR_DEPTH_8BIT);
		msWrite4Byte(mtk_dp, REG_3368_DP_ENCODER1_P0,
			//(0x1 << BS_FOLLOW_SEL_DP_ENCODER1_P0_FLDMASK_POS) |
			(0x1 << 15) |
			(0x4 << BS2BS_MODE_DP_ENCODER1_P0_FLDMASK_POS) |
			(0x1 << SDP_DP13_EN_DP_ENCODER1_P0_FLDMASK_POS) |
			(0x1 << VIDEO_STABLE_CNT_THRD_DP_ENCODER1_P0_FLDMASK_POS) |
			(0x1 << VIDEO_SRAM_FIFO_CNT_RESET_SEL_DP_ENCODER1_P0_FLDMASK_POS));

		mtk_dptx_hal_encoder_reset(mtk_dp);
	} else {
		msWriteByteMask(mtk_dp,
			REG_304C_DP_ENCODER0_P0,
			0,
			VBID_VIDEO_MUTE_DP_ENCODER0_P0_FLDMASK);
		mhal_DPTx_SetColorFormat(mtk_dp, DP_COLOR_FORMAT_RGB);//MISC0
		// [13 : 12] : = 2b'01 VDE check BS2BS & set min value
		mhal_DPTx_SetColorDepth(mtk_dp, DP_COLOR_DEPTH_8BIT);
		msWriteByteMask(mtk_dp,
			REG_3368_DP_ENCODER1_P0 + 1,
			BIT(4),
			MASKBIT(5 : 4));
		msWriteByteMask(mtk_dp,
			REG_3004_DP_ENCODER0_P0 + 1,
			BIT(1),
			BIT(1));// dp tx encoder reset all sw
		//DELAY_NOP(10);
		mdelay(1);
		// dp tx encoder reset all sw
		msWriteByteMask(mtk_dp, REG_3004_DP_ENCODER0_P0 + 1, 0, BIT(1));
	}
}

void mhal_DPTx_MacVideoPatternGenEn(struct mtk_dp *mtk_dp, bool enable)
{
	if(enable) {
		msWrite4ByteMask(mtk_dp, REG_3038_DP_ENCODER0_P0, BIT(11), BIT(11));
		DPTXMSG("[DP DEBUG]DPTX MAC VIDEO PG ENABLE\n");
	} else {
		msWrite4ByteMask(mtk_dp, REG_3038_DP_ENCODER0_P0, 0, BIT(11));
		DPTXMSG("[DP DEBUG]DPTX MAC VIDEO PG DISABLE\n");
	}
}

void mhal_DPTx_MacAudioPatternGenEn(struct mtk_dp *mtk_dp, bool enable)
{
	if(enable) {
		msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
				 AU_GEN_EN_DP_ENCODER0_P0_FLDMASK,
				 AU_GEN_EN_DP_ENCODER0_P0_FLDMASK);
		// msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
		// 0x3 << AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK_POS
		// , AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK);
		DPTXMSG("[DP DEBUG]DPTX MAC AUDIO PG ENABLE\n");
	} else {
		msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
				 0, AU_GEN_EN_DP_ENCODER0_P0_FLDMASK);
		// msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0,
		// 0x4 << AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK_POS
		// , AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK);
		DPTXMSG("[DP DEBUG]DPTX MAC AUDIO PG DISABLE\n");
	}
}

void mhal_DPTx_DigitalSwReset(struct mtk_dp *mtk_dp)
{
	DPTXFUNC();

	msWriteByteMask(mtk_dp, REG_340C_DP_TRANS_P0 + 1, BIT(5), BIT(5));
	mdelay(1);
	msWriteByteMask(mtk_dp, REG_340C_DP_TRANS_P0 + 1, 0, BIT(5));
}

void mhal_DPTx_SetTxLaneToLane(struct mtk_dp *mtk_dp, BYTE ucLaneNum,
	BYTE ucSetLaneNum)
{
	msWriteByteMask(mtk_dp,
		REG_3408_DP_TRANS_P0 + 1,
		(ucSetLaneNum << (ucLaneNum*2)),
		(BIT(1)|BIT(0)) << (ucLaneNum*2)); // swap Lane A  to Lane B
}

void mhal_DPTx_PHYD_Reset(struct mtk_dp *mtk_dp)
{
	if (mtk_dp->cfg_ver == 2) {
		msPhyWriteByteMask(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_SW_RST, 0, BIT(0));
		udelay(50);
		msPhyWriteByteMask(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_SW_RST, BIT(0), BIT(0));
	} else {
		msWriteByteMask(mtk_dp, 0x1038, 0, BIT(0));
		udelay(50);
		msWriteByteMask(mtk_dp, 0x1038, BIT(0), BIT(0));
	}
}

void mhal_DPTx_SetTxLane(struct mtk_dp *mtk_dp, const enum DPTX_LANE_COUNT lane_count)
{

	DPTXFUNC();
	/*4Lane: 2, 2Lane: 1, 1Lane: 0*/
	const UINT8 val = (lane_count >> 1);

	if (val == 0)
		msWriteByteMask(mtk_dp, REG_35F0_DP_TRANS_P0, 0, (BIT(3) | BIT(2)));
	else if (val < 4)
		msWriteByteMask(mtk_dp, REG_35F0_DP_TRANS_P0, BIT(3), (BIT(3) | BIT(2)));
	else {
		DPTXMSG("Un-expected lane count %d\n", lane_count);
		return;
	}

	msWriteByteMask(mtk_dp, REG_3000_DP_ENCODER0_P0 ,
	val << LANE_NUM_DP_ENCODER0_P0_FLDMASK_POS,
	LANE_NUM_DP_ENCODER0_P0_FLDMASK);
	msWriteByteMask(mtk_dp, REG_34A4_DP_TRANS_P0,
		val << LANE_NUM_DP_TRANS_P0_FLDMASK_POS,
		LANE_NUM_DP_TRANS_P0_FLDMASK);
}

void mhal_DPTx_SetAuxSwap(struct mtk_dp *mtk_dp, bool enable)
{
	DPTXFUNC();

	if (enable) {
		// aux swap
		msWrite4ByteMask(mtk_dp, REG_360C_AUX_TX_P0, 0, BIT(15));
		msWrite4ByteMask(mtk_dp, REG_3680_AUX_TX_P0, 0, BIT(0));
		if (mtk_dp->cfg_ver == 2) {
			// Phy porting guide (inter-lane skew improvement)
			msPhyWriteByte(mtk_dp, 0x01A0, 0x46);
			msPhyWriteByte(mtk_dp, 0x02A0, 0x46);
			msPhyWriteByte(mtk_dp, 0x03A0, 0x47);
			msPhyWriteByte(mtk_dp, 0x04A0, 0x47);
		}
		DPTXMSG("set normal plug setting");
	} else {
		// aux swap
		msWrite4ByteMask(mtk_dp, REG_360C_AUX_TX_P0, BIT(15), BIT(15));
		msWrite4ByteMask(mtk_dp, REG_3680_AUX_TX_P0, BIT(0), BIT(0));
		if (mtk_dp->cfg_ver == 2) {
			// Phy porting guide (inter-lane skew improvement)
			msPhyWriteByte(mtk_dp, 0x01A0, 0x47);
			msPhyWriteByte(mtk_dp, 0x02A0, 0x47);
			msPhyWriteByte(mtk_dp, 0x03A0, 0x46);
			msPhyWriteByte(mtk_dp, 0x04A0, 0x46);
		}
		DPTXMSG("set flipped plug setting");
	}
}

void mhal_DPTx_SetTxRate(struct mtk_dp *mtk_dp, u8 Value)
{
	DPTXFUNC();
	if (mtk_dp->cfg_ver == 2) {
		switch (Value) {
		case 0x06:
			msPhyWrite4Byte(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_BIT_RATE, 0x0);
			break;
		case 0x0A:
			msPhyWrite4Byte(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_BIT_RATE, 0x1);
			break;
		case 0x14:
			msPhyWrite4Byte(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_BIT_RATE, 0x2);
			break;
		case 0x1E:
			msPhyWrite4Byte(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_BIT_RATE, 0x3);
			break;
		default:
			break;
		}
	} else {
		uint32_t phyd_rdy_bmp = 0x9;
		uint32_t phyd_rdy_status;
		uint32_t i;

		DPTXFUNC();

		//msWrite4Byte(mtk_dp, 0x2000, 0x00000001); // power off TPLL and Lane;
		msWrite4Byte(mtk_dp, 0x1010, 0x00000003); // power off TPLL and Lane;
		//msWrite4Byte(mtk_dp, 0x1038, 0x00000001); // reset phy-d;
		//msWrite4Byte(mtk_dp, 0x1038, 0x00000000); // release reset phy-d;
		msWriteByteMask(mtk_dp, 0x1038, 0, BIT(0));
		msWriteByteMask(mtk_dp, 0x1038, BIT(0), BIT(0));
		mdelay(10);

		/// Set gear : 0x0 : RBR, 0x1 : HBR, 0x2 : HBR2, 0x3 : HBR3
		msWrite4ByteMask(mtk_dp, 0x003C, 0x001 << 23, BITMASK(23:23));
		switch (Value) {
		case 0x06:
			msWrite4ByteMask(mtk_dp, 0x003C, 0x003 << 24, BITMASK(28:24));
			msWrite4Byte(mtk_dp,
				0x103C,
				0x00000000);
			break;
		case 0x0A:
			msWrite4ByteMask(mtk_dp, 0x003C, 0x005 << 24, BITMASK(28:24));
			msWrite4Byte(mtk_dp,
				0x103C,
				0x00000001);
			break;
		case 0x14:
			msWrite4ByteMask(mtk_dp, 0x003C, 0x005 << 24, BITMASK(28:24));
			msWrite4Byte(mtk_dp,
				0x103C,
				0x00000002);
			break;
		case 0x1E:
			msWrite4ByteMask(mtk_dp, 0x003C, 0x002 << 24, BITMASK(28:24));
			msWrite4Byte(mtk_dp,
				0x103C,
				0x00000003);
			break;
		default:
			break;
		}

		msWrite4Byte(mtk_dp,
			0x2000,
			0x00000003); // power on BandGap, TPLL and Lane;
		msWrite4Byte(mtk_dp, 0x1010, 0x00000007); // power on TPLL and Lane;

		for (i = 0; i < 10; i++) {
			phyd_rdy_status = msRead4Byte(mtk_dp, 0x1088);
			phyd_rdy_status &= phyd_rdy_bmp;

			if (phyd_rdy_status == phyd_rdy_bmp) {
				DPTXMSG("DPTX PHYD power on\n");
				return;
			}
			DPTXMSG("Polling BIAS status %x\n", phyd_rdy_status);
		}
		DPTXERR("Polling BIAS status fail %x\n", phyd_rdy_status);
	}
}

void mhal_DPTx_SetTxTrainingPattern(struct mtk_dp *mtk_dp, int  Value)
{
	DPTXMSG("Set Train Pattern =0x%x\n ", Value);

	msWriteByteMask(mtk_dp,
		REG_3400_DP_TRANS_P0 + 1,
		Value,
		MASKBIT(7 : 4));
}

void mhal_DPTx_PHY_SetIdlePattern(struct mtk_dp *mtk_dp, bool bENABLE)
{
	DPTXDBG("Idle pattern enable(%d)\n", bENABLE);
	if (bENABLE)
		msWriteByteMask(mtk_dp,
			REG_3580_DP_TRANS_P0 + 1,
			0x0F,
			0x0F);
	else
		msWriteByteMask(mtk_dp,
			REG_3580_DP_TRANS_P0 + 1,
			0x0,
			0x0F);
}

void mhal_DPTx_SetFreeSync(struct mtk_dp *mtk_dp, bool bENABLE)
{
	DPTXFUNC();
	if (bENABLE)//mtk_dp, REG_bs2bs_mode, [13 : 12]  = 11 freesync on
		msWriteByteMask(mtk_dp,
			REG_3368_DP_ENCODER1_P0 + 1,
			BIT(5)|BIT(4),
			BIT(5)|BIT(4));
	else//mtk_dp, REG_bs2bs_mode, [13 : 12] = 01 freesync off
		msWriteByteMask(mtk_dp,
			REG_3368_DP_ENCODER1_P0 + 1,
			BIT(4),
			BIT(5)|BIT(4));
}

void mhal_DPTx_SetEF_Mode(struct mtk_dp *mtk_dp, bool  bENABLE)
{
	if (bENABLE)//[4] REG_enhanced_frame_mode [1 : 0]mtk_dp, REG_lane_num
		msWriteByteMask(mtk_dp,
			REG_3000_DP_ENCODER0_P0,
			BIT(4),
			BIT(4));
	else //[4]mtk_dp, REG_enhanced_frame_mode [1 : 0]mtk_dp, REG_lane_num
		msWriteByteMask(mtk_dp,
		REG_3000_DP_ENCODER0_P0,
		0,
		BIT(4));

}

void mhal_DPTx_SetScramble(struct mtk_dp *mtk_dp, bool  bENABLE)
{
	if (bENABLE)
		msWriteByteMask(mtk_dp,
			REG_3404_DP_TRANS_P0,
			BIT(0),
			BIT(0)); //[0]dp tx transmitter scramble enable
	else
		msWriteByteMask(mtk_dp,
			REG_3404_DP_TRANS_P0,
			0,
			BIT(0)); //[0]dp tx transmitter scramble enable
}

void mhal_DPTx_SetScramble_Type(struct mtk_dp *mtk_dp, bool bSelType)
{
	//[1]scrambler reset data,0:DP 16'ffff, 1:eDP 16'fffe
	if (bSelType)//eDP
		msWriteByteMask(mtk_dp,
			REG_3404_DP_TRANS_P0,
			BIT(1),
			BIT(1));
	else // DP
		msWriteByteMask(mtk_dp,
			REG_3404_DP_TRANS_P0,
			0,
			BIT(1));
}

void mhal_DPTx_VideoMute(struct mtk_dp *mtk_dp, bool bENABLE)
{
	DPTXFUNC("enable = %d\n", bENABLE);

	mhal_DPTx_VideoMuteSW(mtk_dp, bENABLE);

	if (bENABLE) {
		msWriteByteMask(mtk_dp,
			REG_3000_DP_ENCODER0_P0,
			BIT(3)|BIT(2),
			BIT(3)|BIT(2)); //Video mute enable

		mtk_dp_atf_call(DP_VIDEO_UNMUTE, 1);
	} else {
		msWriteByteMask(mtk_dp,
			REG_3000_DP_ENCODER0_P0,
			BIT(3),
			BIT(3)|BIT(2));// [3] Sw ov Mode [2] mute value

		mtk_dp_atf_call(DP_VIDEO_UNMUTE, 0);
	}
	msWriteByteMask(mtk_dp, 0x402C, 0, BIT(4));
	msWriteByteMask(mtk_dp, 0x402C, 1, BIT(4));
}

void mhal_DPTx_VideoMuteSW(struct mtk_dp *mtk_dp, bool bENABLE)
{
	DPTXFUNC("enable = %d\n", bENABLE);
	if (bENABLE)
		msWriteByteMask(mtk_dp,
			REG_304C_DP_ENCODER0_P0,
			BIT(2),
			BIT(2)); //Video mute enable
	else
		msWriteByteMask(mtk_dp,
			REG_304C_DP_ENCODER0_P0,
			0,
			BIT(2));	// [3] Sw ov Mode [2] mute value
}

void mhal_DPTx_AudioMute(struct mtk_dp *mtk_dp, bool bENABLE)
{
	DPTXFUNC();
	if (bENABLE) {
		msWrite2ByteMask(mtk_dp,
			REG_3030_DP_ENCODER0_P0,
			0x01 << VBID_AUDIO_MUTE_SW_DP_ENCODER0_P0_FLDMASK_POS,
			VBID_AUDIO_MUTE_FLAG_SW_DP_ENCODER0_P0_FLDMASK);

		msWrite2ByteMask(mtk_dp,
			REG_3030_DP_ENCODER0_P0,
			0x01 << VBID_AUDIO_MUTE_SEL_DP_ENCODER0_P0_FLDMASK_POS,
			VBID_AUDIO_MUTE_FLAG_SEL_DP_ENCODER0_P0_FLDMASK);

		msWriteByteMask(mtk_dp,
			REG_3088_DP_ENCODER0_P0,
			0x0 << AU_EN_DP_ENCODER0_P0_FLDMASK_POS,
			AU_EN_DP_ENCODER0_P0_FLDMASK);
		msWriteByte(mtk_dp,
			REG_30A4_DP_ENCODER0_P0,
			0x00);

	} else {
		msWrite2ByteMask(mtk_dp,
			REG_3030_DP_ENCODER0_P0,
			0x00 << VBID_AUDIO_MUTE_SEL_DP_ENCODER0_P0_FLDMASK_POS,
			VBID_AUDIO_MUTE_FLAG_SEL_DP_ENCODER0_P0_FLDMASK);

		msWriteByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0,
			0x1 << AU_EN_DP_ENCODER0_P0_FLDMASK_POS,
			AU_EN_DP_ENCODER0_P0_FLDMASK);
		msWriteByte(mtk_dp,
			REG_30A4_DP_ENCODER0_P0,
			0x0F);
	}
}

#if (DPTX_AutoTest_ENABLE == 0x1) && (DPTX_PHY_TEST_PATTERN_EN == 0x1)

void mhal_DPTx_PHY_ResetPattern(struct mtk_dp *mtk_dp)
{
	DPTXFUNC();
	//reset pattern
	mhal_DPTx_SetTxLane(mtk_dp, DPTX_4LANE);
	mhal_DPTx_ProgramPatternEnable(mtk_dp, false);
	mhal_DPTx_PatternSelect(mtk_dp, 0x00);
	mhal_DPTx_PRBSEnable(mtk_dp, false);
	mhal_DPTx_ComplianceEyeEnSetting(mtk_dp, false);
}

void mhal_DPTx_PRBSEnable(struct mtk_dp *mtk_dp, bool bENABLE)
{
	DPTXFUNC();
	if (bENABLE)
		msWriteByteMask(mtk_dp, REG_3444_DP_TRANS_P0, (BIT(3)), BIT(3));
	else
		msWriteByteMask(mtk_dp, REG_3444_DP_TRANS_P0, (0), BIT(3));
}

void mhal_DPTx_ProgramPatternEnable(struct mtk_dp *mtk_dp, bool bENABLE)
{
	DPTXFUNC();
	if (bENABLE)
		msWriteByteMask(mtk_dp,
			REG_3440_DP_TRANS_P0,
			0x0F,
			MASKBIT(3 : 0));
	else
		msWriteByteMask(mtk_dp,
			REG_3440_DP_TRANS_P0,
			0,
			MASKBIT(3 : 0));
}

void mhal_DPTx_PatternSelect(struct mtk_dp *mtk_dp, int Value)
{
	DPTXFUNC();
	msWriteByteMask(mtk_dp,
		REG_3440_DP_TRANS_P0, (Value << 4), MASKBIT(6 : 4));
	msWriteByteMask(mtk_dp,
		REG_3440_DP_TRANS_P0 + 1, (Value), MASKBIT(2 : 0));
	msWriteByteMask(mtk_dp,
		REG_3440_DP_TRANS_P0 + 1, (Value << 4), MASKBIT(6 : 4));
	msWriteByteMask(mtk_dp,
		REG_3444_DP_TRANS_P0, (Value), MASKBIT(2 : 0));
}

void mhal_DPTx_SetProgramPattern(struct mtk_dp *mtk_dp,
	BYTE Value, BYTE  *usData)
{
	DPTXFUNC();
	//16bit RG need *2
	msWriteByte(mtk_dp, REG_3448_DP_TRANS_P0 + Value*6*2, usData[0]);
	msWriteByte(mtk_dp, REG_3448_DP_TRANS_P0 + 1 + Value*6*2, usData[1]);
	msWriteByte(mtk_dp, REG_344C_DP_TRANS_P0 + Value*6*2, usData[2]);
	msWriteByte(mtk_dp, REG_344C_DP_TRANS_P0 + 1 + Value*6*2, usData[3]);
	msWriteByte(mtk_dp, REG_3450_DP_TRANS_P0 + Value*6*2, usData[4]);
	msWriteByte(mtk_dp, REG_3450_DP_TRANS_P0 + 1 + Value*6*2, 0x00);
}

void mhal_DPTx_ComplianceEyeEnSetting(struct mtk_dp *mtk_dp, bool bENABLE)
{
	DPTXFUNC();
	if (bENABLE)
		msWriteByteMask(mtk_dp, REG_3478_DP_TRANS_P0, (BIT(0)), BIT(0));
	else
		msWriteByteMask(mtk_dp, REG_3478_DP_TRANS_P0, (0), BIT(0));
}
#endif

void mhal_DPTx_ShutDownDPTxPort(struct mtk_dp *mtk_dp)
{
	DPTXFUNC();

	//Power Down Tx Aux
	msWriteByteMask(mtk_dp, REG_367C_AUX_TX_P0 + 1, 0, BIT(4));
	msWriteByteMask(mtk_dp, REG_3670_AUX_TX_P0 + 1, BIT(2), BIT(2));
}

void mhal_DPTx_AnalogPowerOnOff(struct mtk_dp *mtk_dp, bool enable)
{
	if (enable) {
		msWriteByteMask(mtk_dp, DP_TX_TOP_RESET_AND_PROBE, 0, BIT(4));
		udelay(10);
		msWriteByteMask(mtk_dp, DP_TX_TOP_RESET_AND_PROBE, BIT(4), BIT(4));
	} else {
		msWrite2Byte(mtk_dp, TOP_OFFSET, 0x0);
		udelay(10);
		if (mtk_dp->cfg_ver == 2) {
			msPhyWrite2Byte(mtk_dp, 0x0034, 0x4AA);
			msPhyWrite2Byte(mtk_dp, 0x1040, 0x0);
			msPhyWrite2Byte(mtk_dp, 0x0038, 0x555);
		} else {
			msWrite2Byte(mtk_dp, 0x0034, 0x4AA);
			msWrite2Byte(mtk_dp, 0x1040, 0x0);
			msWrite2Byte(mtk_dp, 0x0038, 0x555);
		}
	}
}

void mhal_DPTx_PhySSCenable(struct mtk_dp *mtk_dp, bool bENABLE)
{
	if (bENABLE) {
		// Set SSC enable
		msPhyWriteByteMask(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_1,
				0x1 << TPLL_SSC_EN_FLDMASK_POS, TPLL_SSC_EN_FLDMASK);
	} else
		msPhyWriteByteMask(mtk_dp, PHYD_DIG_GLB_OFFSET + DP_PHY_DIG_PLL_CTL_1,
				0x0, TPLL_SSC_EN_FLDMASK);

	DPTXMSG("Phy SSC enable = %d\n", bENABLE);
}


void mhal_DPTx_PhyTrainingConfig(struct mtk_dp *mtk_dp, u8 ubTargetLinkRate, u8 ubTargetLaneCount)
{
	mhal_DPTx_ResetSwingtPreEmphasis(mtk_dp);
	mhal_DPTx_PhySSCenable(mtk_dp, mtk_dp->training_info.bSinkSSC_En);

	//step1: phy-d power down
	mhal_DPTx_PhyPowerDown(mtk_dp);

	//step2: phy-d set link rate
	mhal_DPTx_SetTxRate(mtk_dp, ubTargetLinkRate);
	mhal_DPTx_PhyDPowerOn(mtk_dp);

	//step3: phy-d enable lane
	mhal_DPTx_PhySetLanePower(mtk_dp, ubTargetLaneCount);

	mhal_DPTx_PhyCheckReady(mtk_dp, ubTargetLaneCount);
}


