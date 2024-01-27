// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek USB3.1 gen2 xsphy Driver
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Chunfeng Yun <chunfeng.yun@mediatek.com>
 *
 */

#include <dt-bindings/phy/phy.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

/* u2 phy banks */
#define SSUSB_SIFSLV_MISC		0x000
#define SSUSB_SIFSLV_U2FREQ		0x100
#define SSUSB_SIFSLV_U2PHY_COM	0x300

/* u3 phy shared banks */
#define SSPXTP_SIFSLV_DIG_GLB		0x000
#define SSPXTP_SIFSLV_PHYA_GLB		0x100

/* u3 phy banks */
#define SSPXTP_SIFSLV_DIG_LN_TOP	0x000
#define SSPXTP_SIFSLV_DIG_LN_TX0	0x100
#define SSPXTP_SIFSLV_DIG_LN_RX0	0x200
#define SSPXTP_SIFSLV_DIG_LN_DAIF	0x300
#define SSPXTP_SIFSLV_PHYA_LN		0x400

#define XSP_U2FREQ_FMCR0	((SSUSB_SIFSLV_U2FREQ) + 0x00)
#define P2F_RG_FREQDET_EN	BIT(24)
#define P2F_RG_CYCLECNT		GENMASK(23, 0)
#define P2F_RG_CYCLECNT_VAL(x)	((P2F_RG_CYCLECNT) & (x))

#define XSP_U2FREQ_MMONR0  ((SSUSB_SIFSLV_U2FREQ) + 0x0c)

#define XSP_U2FREQ_FMMONR1	((SSUSB_SIFSLV_U2FREQ) + 0x10)
#define P2F_RG_FRCK_EN		BIT(8)
#define P2F_USB_FM_VALID	BIT(0)

#define XSP_USBPHYACR0	((SSUSB_SIFSLV_U2PHY_COM) + 0x00)
#define P2A0_RG_INTR_EN	BIT(5)

#define XSP_USBPHYACR1		((SSUSB_SIFSLV_U2PHY_COM) + 0x04)
#define P2A1_RG_VRT_SEL			GENMASK(14, 12)
#define P2A1_RG_VRT_SEL_VAL(x)	((0x7 & (x)) << 12)
#define P2A1_RG_TERM_SEL		GENMASK(10, 8)
#define P2A1_RG_TERM_SEL_VAL(x)	((0x7 & (x)) << 8)

#define XSP_USBPHYACR2		((SSUSB_SIFSLV_U2PHY_COM) + 0x08)

#define XSP_USBPHYACR5		((SSUSB_SIFSLV_U2PHY_COM) + 0x014)
#define P2A5_RG_HSTX_SRCAL_EN	BIT(15)
#define P2A5_RG_HSTX_SRCTRL		GENMASK(14, 12)
#define P2A5_RG_HSTX_SRCTRL_VAL(x)	((0x7 & (x)) << 12)

#define XSP_USBPHYACR6		((SSUSB_SIFSLV_U2PHY_COM) + 0x018)
#define P2A6_RG_U2_PHY_REV6		GENMASK(31, 30)
#define P2A6_RG_U2_PHY_REV6_VAL(x)	((0x3 & (x)) << 30)
#define P2A6_RG_U2_PHY_REV6_MASK	(0x3)
#define P2A6_RG_U2_PHY_REV6_OFET	(30)
#define P2A6_RG_U2_PHY_REV1		BIT(25)
#define P2A6_RG_BC11_SW_EN	BIT(23)
#define P2A6_RG_OTG_VBUSCMP_EN	BIT(20)
#define P2A6_RG_U2_DISCTH		GENMASK(7, 4)
#define P2A6_RG_U2_DISCTH_VAL(x)	((0xf & (x)) << 4)
#define P2A6_RG_U2_DISCTH_MASK	(0xf)
#define P2A6_RG_U2_DISCTH_OFET	(4)

#define XSP_USBPHYACR3		((SSUSB_SIFSLV_U2PHY_COM) + 0x01c)
#define P2A3_RG_USB20_PUPD_BIST_EN	BIT(12)
#define P2A3_RG_USB20_EN_PU_DP		BIT(9)

#define XSP_USBPHYACR4		((SSUSB_SIFSLV_U2PHY_COM) + 0x020)
#define P2A4_RG_USB20_GPIO_CTL		BIT(9)
#define P2A4_USB20_GPIO_MODE		BIT(8)
#define P2A4_U2_GPIO_CTR_MSK (P2A4_RG_USB20_GPIO_CTL | P2A4_USB20_GPIO_MODE)

#define XSP_USBPHYA_RESERVE	((SSUSB_SIFSLV_U2PHY_COM) + 0x030)
#define P2AR_RG_INTR_CAL		GENMASK(29, 24)
#define P2AR_RG_INTR_CAL_VAL(x)		((0x3f & (x)) << 24)

#define XSP_USBPHYA_RESERVEA	((SSUSB_SIFSLV_U2PHY_COM) + 0x034)
#define P2ARA_RG_TERM_CAL		GENMASK(11, 8)
#define P2ARA_RG_TERM_CAL_VAL(x)	((0xf & (x)) << 8)

#define XSP_U2PHYDTM0		((SSUSB_SIFSLV_U2PHY_COM) + 0x068)
#define P2D_FORCE_UART_EN		BIT(26)
#define P2D_FORCE_DATAIN		BIT(23)
#define P2D_FORCE_DM_PULLDOWN		BIT(21)
#define P2D_FORCE_DP_PULLDOWN		BIT(20)
#define P2D_FORCE_XCVRSEL		BIT(19)
#define P2D_FORCE_SUSPENDM		BIT(18)
#define P2D_FORCE_TERMSEL		BIT(17)
#define P2D_RG_DATAIN			GENMASK(13, 10)
#define P2D_RG_DATAIN_VAL(x)		((0xf & (x)) << 10)
#define P2D_RG_DMPULLDOWN		BIT(7)
#define P2D_RG_DPPULLDOWN		BIT(6)
#define P2D_RG_XCVRSEL			GENMASK(5, 4)
#define P2D_RG_XCVRSEL_VAL(x)		((0x3 & (x)) << 4)
#define P2D_RG_SUSPENDM			BIT(3)
#define P2D_RG_TERMSEL			BIT(2)
#define P2D_DTM0_PART_MASK \
		(P2D_FORCE_DATAIN | P2D_FORCE_DM_PULLDOWN | \
		P2D_FORCE_DP_PULLDOWN | P2D_FORCE_XCVRSEL | \
		P2D_FORCE_SUSPENDM | P2D_FORCE_TERMSEL | \
		P2D_RG_DMPULLDOWN | P2D_RG_DPPULLDOWN | \
		P2D_RG_TERMSEL)


#define XSP_U2PHYDTM1		((SSUSB_SIFSLV_U2PHY_COM) + 0x06C)
#define P2D_RG_UART_EN		BIT(16)
#define P2D_FORCE_IDDIG		BIT(9)
#define P2D_RG_VBUSVALID	BIT(5)
#define P2D_RG_SESSEND		BIT(4)
#define P2D_RG_AVALID		BIT(2)
#define P2D_RG_IDDIG		BIT(1)

#define SSPXTP_DIG_GLB_28		((SSPXTP_SIFSLV_DIG_GLB) + 0x028)
#define RG_XTP_DAIF_GLB_TXPLL_IR		GENMASK(17, 13)
#define RG_XTP_DAIF_GLB_TXPLL_IR_VAL(x)	((0x1f & (x)) << 13)

#define SSPXTP_DIG_GLB_38		((SSPXTP_SIFSLV_DIG_GLB) + 0x038)
#define RG_XTP_DAIF_GLB_SPLL_IR		GENMASK(17, 13)
#define RG_XTP_DAIF_GLB_SPLL_IR_VAL(x)	((0x1f & (x)) << 13)

#define SSPXTP_PHYA_GLB_00		((SSPXTP_SIFSLV_PHYA_GLB) + 0x00)
#define RG_XTP_GLB_BIAS_INTR_CTRL		GENMASK(21, 16)
#define RG_XTP_GLB_BIAS_INTR_CTRL_VAL(x)	((0x3f & (x)) << 16)

#define SSPXTP_DAIG_LN_DAIF_04	((SSPXTP_SIFSLV_DIG_LN_DAIF) + 0x04)
#define RG_XTP0_DAIF_FRC_LN_RX_AEQ_ATT		BIT(17)

#define SSPXTP_DAIG_LN_DAIF_14	((SSPXTP_SIFSLV_DIG_LN_DAIF) + 0x014)
#define RG_XTP0_DAIF_LN_RX_AEQ_ATT		GENMASK(20, 18)
#define RG_XTP0_DAIF_LN_RX_AEQ_ATT_VAL(x)	((0x7 & (x)) << 18)

#define SSPXTP_DAIG_LN_DAIF_20	((SSPXTP_SIFSLV_DIG_LN_DAIF) + 0x020)
#define RG_XTP0_DAIF_LN_G1_RX_SGDT_HF		GENMASK(23, 22)
#define RG_XTP0_DAIF_LN_G1_RX_SGDT_HF_VAL(x)	((0x3 & (x)) << 22)

#define SSPXTP_DAIG_LN_DAIF_2C	((SSPXTP_SIFSLV_DIG_LN_DAIF) + 0x02C)
#define RG_XTP0_DAIF_LN_G2_RX_SGDT_HF		GENMASK(23, 22)
#define RG_XTP0_DAIF_LN_G2_RX_SGDT_HF_VAL(x)	((0x3 & (x)) << 22)

#define SSPXTP_PHYA_LN_04	((SSPXTP_SIFSLV_PHYA_LN) + 0x04)
#define RG_XTP_LN0_TX_IMPSEL		GENMASK(4, 0)
#define RG_XTP_LN0_TX_IMPSEL_VAL(x)	(0x1f & (x))

#define SSPXTP_PHYA_LN_08	((SSPXTP_SIFSLV_PHYA_LN) + 0x08)
#define RG_XTP_LN0_TX_RXDET_HZ		BIT(13)

#define SSPXTP_PHYA_LN_14	((SSPXTP_SIFSLV_PHYA_LN) + 0x014)
#define RG_XTP_LN0_RX_IMPSEL		GENMASK(4, 0)
#define RG_XTP_LN0_RX_IMPSEL_VAL(x)	(0x1f & (x))

#define SSPXTP_PHYA_LN_30	((SSPXTP_SIFSLV_PHYA_LN) + 0x030)
#define RG_XTP_LN0_RX_AEQ_ATT		BIT(14)

#define XSP_REF_CLK		26	/* MHZ */
#define XSP_SLEW_RATE_COEF	17
#define XSP_SR_COEF_DIVISOR	1000
#define XSP_FM_DET_CYCLE_CNT	1024

#define XSP_MODE_UART_STR "usb2uart_mode=1"
#define XSP_MODE_JTAG_STR "usb2jtag_mode=1"

enum mtk_xsphy_mode {
	XSP_MODE_USB = 0,
	XSP_MODE_UART,
	XSP_MODE_JTAG,
};

enum mtk_xsphy_submode {
	PHY_MODE_BC11_SW_SET = 1,
	PHY_MODE_BC11_SW_CLR,
	PHY_MODE_DPDMPULLDOWN_SET,
	PHY_MODE_DPDMPULLDOWN_CLR,
	PHY_MODE_DPPULLUP_SET,
	PHY_MODE_DPPULLUP_CLR,
};

enum mtk_xsphy_jtag_version {
	XSP_JTAG_V1 = 1,
	XSP_JTAG_V2,
};

enum mtk_phy_efuse {
	INTR_CAL = 0,
	TERM_CAL,
	IEXT_INTR_CTRL,
	RX_IMPSEL,
	TX_IMPSEL,
};

static char *efuse_name[5] = {
	"intr_cal",
	"term_cal",
	"iext_intr_ctrl",
	"rx_impsel",
	"tx_impsel",
};

struct xsphy_instance {
	struct phy *phy;
	void __iomem *port_base;
	struct clk *ref_clk;	/* reference clock of anolog phy */
	u32 index;
	u32 type;
	/* only for HQA test */
	int efuse_intr;
	int efuse_term_cal;
	int efuse_tx_imp;
	int efuse_rx_imp;
	/* u2 eye diagram */
	int eye_src;
	int eye_vrt;
	int eye_term;
	int discth;
	int rev6;
	/* u2 eye diagram for host */
	int eye_src_host;
	int eye_vrt_host;
	int eye_term_host;
	int rev6_host;
};

struct mtk_xsphy {
	struct device *dev;
	void __iomem *glb_base;	/* only shared u3 sif */
	struct xsphy_instance **phys;
	int nphys;
	int src_ref_clk; /* MHZ, reference clock for slew rate calibrate */
	int src_coef;    /* coefficient for slew rate calibrate */
};

static void u2_phy_props_set(struct mtk_xsphy *xsphy,
		struct xsphy_instance *inst);
static void u2_phy_host_props_set(struct mtk_xsphy *xsphy,
		struct xsphy_instance *inst);
static void u3_phy_props_set(struct mtk_xsphy *xsphy,
		struct xsphy_instance *inst);

static void u3_phy_instance_power_on(struct mtk_xsphy *xsphy,
				     struct xsphy_instance *inst)
{
	void __iomem *pbase = inst->port_base;
	u32 tmp;

	/* clear hz mode */
	tmp = readl(pbase + SSPXTP_PHYA_LN_08);
	tmp &= ~RG_XTP_LN0_TX_RXDET_HZ;
	writel(tmp, pbase + SSPXTP_PHYA_LN_08);

	/* DA_XTP_GLB_TXPLL_IR[4:0], 5'b00100 */
	tmp = readl(xsphy->glb_base + SSPXTP_DIG_GLB_28);
	tmp &= ~RG_XTP_DAIF_GLB_TXPLL_IR;
	tmp |= RG_XTP_DAIF_GLB_TXPLL_IR_VAL(0x4);
	writel(tmp, xsphy->glb_base + SSPXTP_DIG_GLB_28);

	/* DA_XTP_GLB_SPLL_IR[4:0], 5'b00100 */
	tmp = readl(xsphy->glb_base + SSPXTP_DIG_GLB_38);
	tmp &= ~RG_XTP_DAIF_GLB_SPLL_IR;
	tmp |= RG_XTP_DAIF_GLB_SPLL_IR_VAL(0x4);
	writel(tmp, xsphy->glb_base + SSPXTP_DIG_GLB_38);

	/* DA_XTP_LN0_RX_SGDT_HF[1:0], 2'b10 */
	tmp = readl(pbase + SSPXTP_DAIG_LN_DAIF_20);
	tmp &= ~RG_XTP0_DAIF_LN_G1_RX_SGDT_HF;
	tmp |= RG_XTP0_DAIF_LN_G1_RX_SGDT_HF_VAL(0x2);
	writel(tmp, pbase + SSPXTP_DAIG_LN_DAIF_20);

	tmp = readl(pbase + SSPXTP_DAIG_LN_DAIF_2C);
	tmp &= ~RG_XTP0_DAIF_LN_G2_RX_SGDT_HF;
	tmp |= RG_XTP0_DAIF_LN_G2_RX_SGDT_HF_VAL(0x2);
	writel(tmp, pbase + SSPXTP_DAIG_LN_DAIF_2C);

	/* DA_XTP_LN0_RX_AEQ_OFORCE[10], 1'b1 */
	tmp = readl(pbase + SSPXTP_PHYA_LN_30);
	tmp |= RG_XTP_LN0_RX_AEQ_ATT;
	writel(tmp, pbase + SSPXTP_PHYA_LN_30);

	/* rg_sspxtp0_datf_ln_rx_aeq_att[2:0], 3'b111 */
	tmp = readl(pbase + SSPXTP_DAIG_LN_DAIF_14);
	tmp &= ~RG_XTP0_DAIF_LN_RX_AEQ_ATT;
	tmp |= RG_XTP0_DAIF_LN_RX_AEQ_ATT_VAL(0x7);
	writel(tmp, pbase + SSPXTP_DAIG_LN_DAIF_14);

	/* rg_sspxtp0_datf_frc_ln_rx_aeq_att, 1'b1 */
	tmp = readl(pbase + SSPXTP_DAIG_LN_DAIF_04);
	tmp |= RG_XTP0_DAIF_FRC_LN_RX_AEQ_ATT;
	writel(tmp, pbase + SSPXTP_DAIG_LN_DAIF_04);

	dev_info(xsphy->dev, "%s(%d)\n", __func__, inst->index);
}

static void u3_phy_instance_power_off(struct mtk_xsphy *xsphy,
				      struct xsphy_instance *inst)
{
	void __iomem *pbase = inst->port_base;
	u32 tmp;

	/* enable hz mode */
	tmp = readl(pbase + SSPXTP_PHYA_LN_08);
	tmp |= RG_XTP_LN0_TX_RXDET_HZ;
	writel(tmp, pbase + SSPXTP_PHYA_LN_08);

	dev_info(xsphy->dev, "%s(%d)\n", __func__, inst->index);
}

static void u2_phy_slew_rate_calibrate(struct mtk_xsphy *xsphy,
					struct xsphy_instance *inst)
{
	void __iomem *pbase = inst->port_base;
	int calib_val;
	int fm_out;
	u32 tmp;

	/* use force value */
	if (inst->eye_src)
		return;

	/* enable USB ring oscillator */
	tmp = readl(pbase + XSP_USBPHYACR5);
	tmp |= P2A5_RG_HSTX_SRCAL_EN;
	writel(tmp, pbase + XSP_USBPHYACR5);
	udelay(1);	/* wait clock stable */

	/* enable free run clock */
	tmp = readl(pbase + XSP_U2FREQ_FMMONR1);
	tmp |= P2F_RG_FRCK_EN;
	writel(tmp, pbase + XSP_U2FREQ_FMMONR1);

	/* set cycle count as 1024 */
	tmp = readl(pbase + XSP_U2FREQ_FMCR0);
	tmp &= ~(P2F_RG_CYCLECNT);
	tmp |= P2F_RG_CYCLECNT_VAL(XSP_FM_DET_CYCLE_CNT);
	writel(tmp, pbase + XSP_U2FREQ_FMCR0);

	/* enable frequency meter */
	tmp = readl(pbase + XSP_U2FREQ_FMCR0);
	tmp |= P2F_RG_FREQDET_EN;
	writel(tmp, pbase + XSP_U2FREQ_FMCR0);

	/* ignore return value */
	readl_poll_timeout(pbase + XSP_U2FREQ_FMMONR1, tmp,
			   (tmp & P2F_USB_FM_VALID), 10, 200);

	fm_out = readl(pbase + XSP_U2FREQ_MMONR0);

	/* disable frequency meter */
	tmp = readl(pbase + XSP_U2FREQ_FMCR0);
	tmp &= ~P2F_RG_FREQDET_EN;
	writel(tmp, pbase + XSP_U2FREQ_FMCR0);

	/* disable free run clock */
	tmp = readl(pbase + XSP_U2FREQ_FMMONR1);
	tmp &= ~P2F_RG_FRCK_EN;
	writel(tmp, pbase + XSP_U2FREQ_FMMONR1);

	if (fm_out) {
		/* (1024 / FM_OUT) x reference clock frequency x coefficient */
		tmp = xsphy->src_ref_clk * xsphy->src_coef;
		tmp = (tmp * XSP_FM_DET_CYCLE_CNT) / fm_out;
		calib_val = DIV_ROUND_CLOSEST(tmp, XSP_SR_COEF_DIVISOR);
	} else {
		/* if FM detection fail, set default value */
		calib_val = 3;
	}
	dev_dbg(xsphy->dev, "phy.%d, fm_out:%d, calib:%d (clk:%d, coef:%d)\n",
		inst->index, fm_out, calib_val,
		xsphy->src_ref_clk, xsphy->src_coef);

	/* set HS slew rate */
	tmp = readl(pbase + XSP_USBPHYACR5);
	tmp &= ~P2A5_RG_HSTX_SRCTRL;
	tmp |= P2A5_RG_HSTX_SRCTRL_VAL(calib_val);
	writel(tmp, pbase + XSP_USBPHYACR5);

	/* disable USB ring oscillator */
	tmp = readl(pbase + XSP_USBPHYACR5);
	tmp &= ~P2A5_RG_HSTX_SRCAL_EN;
	writel(tmp, pbase + XSP_USBPHYACR5);
}

static void u2_phy_instance_init(struct mtk_xsphy *xsphy,
				 struct xsphy_instance *inst)
{
	void __iomem *pbase = inst->port_base;
	u32 tmp;

	/* DP/DM BC1.1 path Disable */
	tmp = readl(pbase + XSP_USBPHYACR6);
	tmp &= ~P2A6_RG_BC11_SW_EN;
	writel(tmp, pbase + XSP_USBPHYACR6);

	tmp = readl(pbase + XSP_USBPHYACR0);
	tmp |= P2A0_RG_INTR_EN;
	writel(tmp, pbase + XSP_USBPHYACR0);
}

static void u2_phy_instance_power_on(struct mtk_xsphy *xsphy,
				     struct xsphy_instance *inst)
{
	void __iomem *pbase = inst->port_base;
	u32 index = inst->index;
	u32 tmp;

	tmp = readl(pbase + XSP_U2PHYDTM0);
	tmp |= P2D_FORCE_SUSPENDM;
	writel(tmp, pbase + XSP_U2PHYDTM0);

	tmp = readl(pbase + XSP_U2PHYDTM0);
	tmp &= ~P2D_RG_SUSPENDM;
	writel(tmp, pbase + XSP_U2PHYDTM0);

	tmp = readl(pbase + XSP_U2PHYDTM0);
	tmp |= P2D_RG_SUSPENDM;
	writel(tmp, pbase + XSP_U2PHYDTM0);

	udelay(30);

	tmp = readl(pbase + XSP_U2PHYDTM0);
	tmp &= ~P2D_FORCE_SUSPENDM;
	writel(tmp, pbase + XSP_U2PHYDTM0);

	tmp = readl(pbase + XSP_U2PHYDTM0);
	tmp &= ~P2D_RG_SUSPENDM;
	writel(tmp, pbase + XSP_U2PHYDTM0);

	tmp = readl(pbase + XSP_U2PHYDTM0);
	tmp &= ~(P2D_FORCE_UART_EN);
	writel(tmp, pbase + XSP_U2PHYDTM0);

	tmp = readl(pbase + XSP_U2PHYDTM1);
	tmp &= ~P2D_RG_UART_EN;
	writel(tmp, pbase + XSP_U2PHYDTM1);

	tmp = readl(pbase + XSP_USBPHYACR4);
	tmp &= ~P2A4_U2_GPIO_CTR_MSK;
	writel(tmp, pbase + XSP_USBPHYACR4);

	tmp = readl(pbase + XSP_U2PHYDTM0);
	tmp &= ~P2D_FORCE_SUSPENDM;
	writel(tmp, pbase + XSP_U2PHYDTM0);

	tmp = readl(pbase + XSP_U2PHYDTM0);
	tmp &= ~(P2D_RG_XCVRSEL | P2D_RG_DATAIN | P2D_DTM0_PART_MASK);
	writel(tmp, pbase + XSP_U2PHYDTM0);

	tmp = readl(pbase + XSP_USBPHYACR6);
	tmp &= ~P2A6_RG_BC11_SW_EN;
	writel(tmp, pbase + XSP_USBPHYACR6);

	tmp = readl(pbase + XSP_USBPHYACR6);
	tmp |= P2A6_RG_OTG_VBUSCMP_EN;
	writel(tmp, pbase + XSP_USBPHYACR6);

	tmp = readl(pbase + XSP_U2PHYDTM1);
	tmp |= P2D_RG_VBUSVALID | P2D_RG_AVALID;
	tmp &= ~P2D_RG_SESSEND;
	writel(tmp, pbase + XSP_U2PHYDTM1);

	tmp = readl(pbase + XSP_USBPHYACR6);
	tmp &= ~P2A6_RG_U2_PHY_REV6;
	tmp |= P2A6_RG_U2_PHY_REV6_VAL(1);
	writel(tmp, pbase + XSP_USBPHYACR6);

	udelay(800);

	dev_info(xsphy->dev, "%s(%d)\n", __func__, index);
}

static void u2_phy_instance_power_off(struct mtk_xsphy *xsphy,
				      struct xsphy_instance *inst)
{
	void __iomem *pbase = inst->port_base;
	u32 index = inst->index;
	u32 tmp;

	tmp = readl(pbase + XSP_U2PHYDTM0);
	tmp &= ~(P2D_FORCE_UART_EN);
	writel(tmp, pbase + XSP_U2PHYDTM0);

	tmp = readl(pbase + XSP_U2PHYDTM1);
	tmp &= ~P2D_RG_UART_EN;
	writel(tmp, pbase + XSP_U2PHYDTM1);

	tmp = readl(pbase + XSP_USBPHYACR4);
	tmp &= ~P2A4_U2_GPIO_CTR_MSK;
	writel(tmp, pbase + XSP_USBPHYACR4);

	tmp = readl(pbase + XSP_USBPHYACR6);
	tmp &= ~P2A6_RG_BC11_SW_EN;
	writel(tmp, pbase + XSP_USBPHYACR6);

	tmp = readl(pbase + XSP_USBPHYACR6);
	tmp &= ~P2A6_RG_OTG_VBUSCMP_EN;
	writel(tmp, pbase + XSP_USBPHYACR6);

	tmp = readl(pbase + XSP_U2PHYDTM1);
	tmp &= ~(P2D_RG_VBUSVALID | P2D_RG_AVALID);
	tmp |= P2D_RG_SESSEND;
	writel(tmp, pbase + XSP_U2PHYDTM1);

	tmp = readl(pbase + XSP_U2PHYDTM0);
	tmp |= P2D_RG_SUSPENDM | P2D_FORCE_SUSPENDM;
	writel(tmp, pbase + XSP_U2PHYDTM0);

	mdelay(2);

	tmp = readl(pbase + XSP_U2PHYDTM0);
	tmp &= ~P2D_RG_DATAIN;
	tmp |= (P2D_RG_XCVRSEL_VAL(1) | P2D_DTM0_PART_MASK);
	writel(tmp, pbase + XSP_U2PHYDTM0);

	tmp = readl(pbase + XSP_USBPHYACR6);
	tmp |= P2A6_RG_U2_PHY_REV6_VAL(1);
	writel(tmp, pbase + XSP_USBPHYACR6);

	udelay(800);

	tmp = readl(pbase + XSP_U2PHYDTM0);
	tmp &= ~P2D_RG_SUSPENDM;
	writel(tmp, pbase + XSP_U2PHYDTM0);

	udelay(1);

	dev_info(xsphy->dev, "%s(%d)\n", __func__, index);
}

static void u2_phy_instance_set_mode(struct mtk_xsphy *xsphy,
				     struct xsphy_instance *inst,
				     enum phy_mode mode,
				     int submode)
{
	u32 tmp;

	dev_info(xsphy->dev, "%s mode(%d), submode(%d)\n", __func__,
		mode, submode);

	if (!submode) {
		tmp = readl(inst->port_base + XSP_U2PHYDTM1);
		switch (mode) {
		case PHY_MODE_USB_DEVICE:
			u2_phy_props_set(xsphy, inst);
			tmp |= P2D_FORCE_IDDIG | P2D_RG_IDDIG;
			break;
		case PHY_MODE_USB_HOST:
			u2_phy_host_props_set(xsphy, inst);
			tmp |= P2D_FORCE_IDDIG;
			tmp &= ~P2D_RG_IDDIG;
			break;
		case PHY_MODE_USB_OTG:
			tmp &= ~(P2D_FORCE_IDDIG | P2D_RG_IDDIG);
			break;
		default:
			return;
		}
		writel(tmp, inst->port_base + XSP_U2PHYDTM1);
	} else {
		switch (submode) {
		case PHY_MODE_BC11_SW_SET:
			tmp = readl(inst->port_base + XSP_USBPHYACR6);
			tmp |= P2A6_RG_BC11_SW_EN;
			writel(tmp, inst->port_base + XSP_USBPHYACR6);
			break;
		case PHY_MODE_BC11_SW_CLR:
			tmp = readl(inst->port_base + XSP_USBPHYACR6);
			tmp &= ~P2A6_RG_BC11_SW_EN;
			writel(tmp, inst->port_base + XSP_USBPHYACR6);
			break;
		case PHY_MODE_DPDMPULLDOWN_SET:
			tmp = readl(inst->port_base + XSP_U2PHYDTM0);
			tmp |= P2D_RG_DPPULLDOWN | P2D_RG_DMPULLDOWN;
			writel(tmp, inst->port_base + XSP_U2PHYDTM0);

			tmp = readl(inst->port_base + XSP_USBPHYACR6);
			tmp &= ~P2A6_RG_U2_PHY_REV1;
			writel(tmp, inst->port_base + XSP_USBPHYACR6);

			tmp = readl(inst->port_base + XSP_USBPHYACR6);
			tmp |= P2A6_RG_BC11_SW_EN;
			writel(tmp, inst->port_base + XSP_USBPHYACR6);
			break;
		case PHY_MODE_DPDMPULLDOWN_CLR:
			tmp = readl(inst->port_base + XSP_U2PHYDTM0);
			tmp &= ~(P2D_RG_DPPULLDOWN | P2D_RG_DMPULLDOWN);
			writel(tmp, inst->port_base + XSP_U2PHYDTM0);

			tmp = readl(inst->port_base + XSP_USBPHYACR6);
			tmp |= P2A6_RG_U2_PHY_REV1;
			writel(tmp, inst->port_base + XSP_USBPHYACR6);

			tmp = readl(inst->port_base + XSP_USBPHYACR6);
			tmp &= ~P2A6_RG_BC11_SW_EN;
			writel(tmp, inst->port_base + XSP_USBPHYACR6);
			break;
		case PHY_MODE_DPPULLUP_SET:
			tmp = readl(inst->port_base + XSP_USBPHYACR3);
			tmp |= P2A3_RG_USB20_PUPD_BIST_EN |
				P2A3_RG_USB20_EN_PU_DP;
			writel(tmp, inst->port_base + XSP_USBPHYACR3);
			break;
		case PHY_MODE_DPPULLUP_CLR:
			tmp = readl(inst->port_base + XSP_USBPHYACR3);
			tmp &= ~(P2A3_RG_USB20_PUPD_BIST_EN |
				P2A3_RG_USB20_EN_PU_DP);
			writel(tmp, inst->port_base + XSP_USBPHYACR3);
			break;
		default:
			return;
		}
	}
}

static u32 phy_get_efuse_value(struct xsphy_instance *inst,
			     enum mtk_phy_efuse type)
{
	struct device *dev = &inst->phy->dev;
	struct device_node *np = dev->of_node;
	u32 val, mask;
	int index = 0, ret = 0;

	index = of_property_match_string(np,
			"nvmem-cell-names", efuse_name[type]);
	if (index < 0)
		goto no_efuse;

	ret = of_property_read_u32_index(np, "nvmem-cell-masks",
			index, &mask);
	if (ret)
		goto no_efuse;

	ret = nvmem_cell_read_u32(dev, efuse_name[type], &val);
	if (ret)
		goto no_efuse;

	if (!val || !mask)
		goto no_efuse;

	val = (val & mask) >> (ffs(mask) - 1);
	dev_dbg(dev, "%s, %s=0x%x\n", __func__, efuse_name[type], val);

	return val;

no_efuse:
	return 0;
}

static void phy_parse_efuse_property(struct mtk_xsphy *xsphy,
			     struct xsphy_instance *inst)
{
	u32 val = 0;

	switch (inst->type) {
	case PHY_TYPE_USB2:
		val = phy_get_efuse_value(inst, INTR_CAL);
		if (val)
			inst->efuse_intr = val;

		val = phy_get_efuse_value(inst, TERM_CAL);
		if (val)
			inst->efuse_term_cal = val;
		break;
	case PHY_TYPE_USB3:
		val = phy_get_efuse_value(inst, IEXT_INTR_CTRL);
		if (val)
			inst->efuse_intr = val;

		val = phy_get_efuse_value(inst, RX_IMPSEL);
		if (val)
			inst->efuse_rx_imp = val;

		val = phy_get_efuse_value(inst, TX_IMPSEL);
		if (val)
			inst->efuse_tx_imp = val;
		break;
	default:
		return;
	}
}

static void phy_parse_property(struct mtk_xsphy *xsphy,
				struct xsphy_instance *inst)
{
	struct device *dev = &inst->phy->dev;

	switch (inst->type) {
	case PHY_TYPE_USB2:
		device_property_read_u32(dev, "mediatek,efuse-intr",
					 &inst->efuse_intr);
		device_property_read_u32(dev, "mediatek,efuse-term",
					 &inst->efuse_term_cal);
		device_property_read_u32(dev, "mediatek,eye-src",
					 &inst->eye_src);
		device_property_read_u32(dev, "mediatek,eye-vrt",
					 &inst->eye_vrt);
		device_property_read_u32(dev, "mediatek,eye-term",
					 &inst->eye_term);
		device_property_read_u32(dev, "mediatek,discth",
				 &inst->discth);
		device_property_read_u32(dev, "mediatek,rev6",
				 &inst->rev6);
		device_property_read_u32(dev, "mediatek,eye-src-host",
					 &inst->eye_src_host);
		device_property_read_u32(dev, "mediatek,eye-vrt-host",
					 &inst->eye_vrt_host);
		device_property_read_u32(dev, "mediatek,eye-term-host",
					 &inst->eye_term_host);
		device_property_read_u32(dev, "mediatek,rev6-host",
				 &inst->rev6_host);
		dev_dbg(dev, "intr:%d, term_cal, src:%d, vrt:%d, term:%d\n",
			inst->efuse_intr, inst->efuse_term_cal, inst->eye_src,
			inst->eye_vrt, inst->eye_term);
		dev_dbg(dev, "src_host:%d, vrt_host:%d, term_host:%d\n",
			inst->eye_src_host, inst->eye_vrt_host,
			inst->eye_term_host);
		dev_dbg(dev, "discth:%d, rev6:%d, rev6_host:%d\n",
			inst->discth, inst->rev6, inst->rev6_host);

		break;
	case PHY_TYPE_USB3:
		device_property_read_u32(dev, "mediatek,efuse-intr",
					 &inst->efuse_intr);
		device_property_read_u32(dev, "mediatek,efuse-tx-imp",
					 &inst->efuse_tx_imp);
		device_property_read_u32(dev, "mediatek,efuse-rx-imp",
					 &inst->efuse_rx_imp);
		dev_dbg(dev, "intr:%d, tx-imp:%d, rx-imp:%d\n",
			inst->efuse_intr, inst->efuse_tx_imp,
			inst->efuse_rx_imp);
		break;
	default:
		dev_err(xsphy->dev, "incompatible phy type\n");
		return;
	}
}

static void u2_phy_props_set(struct mtk_xsphy *xsphy,
			     struct xsphy_instance *inst)
{
	void __iomem *pbase = inst->port_base;
	u32 tmp;

	if (inst->efuse_intr) {
		tmp = readl(pbase + XSP_USBPHYA_RESERVE);
		tmp &= ~P2AR_RG_INTR_CAL;
		tmp |= P2AR_RG_INTR_CAL_VAL(inst->efuse_intr);
		writel(tmp, pbase + XSP_USBPHYA_RESERVE);
	}

	if (inst->efuse_term_cal) {
		tmp = readl(pbase + XSP_USBPHYA_RESERVEA);
		tmp &= ~P2ARA_RG_TERM_CAL;
		tmp |= P2ARA_RG_TERM_CAL_VAL(inst->efuse_term_cal);
		writel(tmp, pbase + XSP_USBPHYA_RESERVEA);
	}


	if (inst->eye_src) {
		tmp = readl(pbase + XSP_USBPHYACR5);
		tmp &= ~P2A5_RG_HSTX_SRCTRL;
		tmp |= P2A5_RG_HSTX_SRCTRL_VAL(inst->eye_src);
		writel(tmp, pbase + XSP_USBPHYACR5);
	}

	if (inst->eye_vrt) {
		tmp = readl(pbase + XSP_USBPHYACR1);
		tmp &= ~P2A1_RG_VRT_SEL;
		tmp |= P2A1_RG_VRT_SEL_VAL(inst->eye_vrt);
		writel(tmp, pbase + XSP_USBPHYACR1);
	}

	if (inst->eye_term) {
		tmp = readl(pbase + XSP_USBPHYACR1);
		tmp &= ~P2A1_RG_TERM_SEL;
		tmp |= P2A1_RG_TERM_SEL_VAL(inst->eye_term);
		writel(tmp, pbase + XSP_USBPHYACR1);
	}

	if (inst->discth) {
		tmp = readl(pbase + XSP_USBPHYACR6);
		tmp &= ~P2A6_RG_U2_DISCTH;
		tmp |= P2A6_RG_U2_DISCTH_VAL(inst->discth);
		writel(tmp, pbase + XSP_USBPHYACR6);
	}

	if (inst->rev6) {
		tmp = readl(pbase + XSP_USBPHYACR6);
		tmp &= ~P2A6_RG_U2_PHY_REV6;
		tmp |= P2A6_RG_U2_PHY_REV6_VAL(inst->rev6);
		writel(tmp, pbase + XSP_USBPHYACR6);
	}
}

static void u2_phy_host_props_set(struct mtk_xsphy *xsphy,
			     struct xsphy_instance *inst)
{
	void __iomem *pbase = inst->port_base;
	u32 tmp;

	if (inst->eye_src_host) {
		tmp = readl(pbase + XSP_USBPHYACR5);
		tmp &= ~P2A5_RG_HSTX_SRCTRL;
		tmp |= P2A5_RG_HSTX_SRCTRL_VAL(inst->eye_src_host);
		writel(tmp, pbase + XSP_USBPHYACR5);
	}

	if (inst->eye_vrt_host) {
		tmp = readl(pbase + XSP_USBPHYACR1);
		tmp &= ~P2A1_RG_VRT_SEL;
		tmp |= P2A1_RG_VRT_SEL_VAL(inst->eye_vrt_host);
		writel(tmp, pbase + XSP_USBPHYACR1);
	}

	if (inst->eye_term_host) {
		tmp = readl(pbase + XSP_USBPHYACR1);
		tmp &= ~P2A1_RG_TERM_SEL;
		tmp |= P2A1_RG_TERM_SEL_VAL(inst->eye_term_host);
		writel(tmp, pbase + XSP_USBPHYACR1);
	}

	if (inst->rev6_host) {
		tmp = readl(pbase + XSP_USBPHYACR6);
		tmp &= ~P2A6_RG_U2_PHY_REV6;
		tmp |= P2A6_RG_U2_PHY_REV6_VAL(inst->rev6_host);
		writel(tmp, pbase + XSP_USBPHYACR6);
	}
}

static void u3_phy_props_set(struct mtk_xsphy *xsphy,
			     struct xsphy_instance *inst)
{
	void __iomem *pbase = inst->port_base;
	u32 tmp;

	if (inst->efuse_intr) {
		tmp = readl(xsphy->glb_base + SSPXTP_PHYA_GLB_00);
		tmp &= ~RG_XTP_GLB_BIAS_INTR_CTRL;
		tmp |= RG_XTP_GLB_BIAS_INTR_CTRL_VAL(inst->efuse_intr);
		writel(tmp, xsphy->glb_base + SSPXTP_PHYA_GLB_00);
	}

	if (inst->efuse_tx_imp) {
		tmp = readl(pbase + SSPXTP_PHYA_LN_04);
		tmp &= ~RG_XTP_LN0_TX_IMPSEL;
		tmp |= RG_XTP_LN0_TX_IMPSEL_VAL(inst->efuse_tx_imp);
		writel(tmp, pbase + SSPXTP_PHYA_LN_04);
	}

	if (inst->efuse_rx_imp) {
		tmp = readl(pbase + SSPXTP_PHYA_LN_14);
		tmp &= ~RG_XTP_LN0_RX_IMPSEL;
		tmp |= RG_XTP_LN0_RX_IMPSEL_VAL(inst->efuse_rx_imp);
		writel(tmp, pbase + SSPXTP_PHYA_LN_14);
	}
}

static int mtk_phy_init(struct phy *phy)
{
	struct xsphy_instance *inst = phy_get_drvdata(phy);
	struct mtk_xsphy *xsphy = dev_get_drvdata(phy->dev.parent);
	int ret;

	ret = clk_prepare_enable(inst->ref_clk);
	if (ret) {
		dev_err(xsphy->dev, "failed to enable ref_clk\n");
		return ret;
	}

	switch (inst->type) {
	case PHY_TYPE_USB2:
		u2_phy_instance_init(xsphy, inst);
		u2_phy_props_set(xsphy, inst);
		/* show default u2 driving setting */
		dev_info(xsphy->dev, "device src:%d vrt:%d term:%d rev6:%d\n",
			inst->eye_src, inst->eye_vrt,
			inst->eye_term, inst->rev6);
		dev_info(xsphy->dev, "host src:%d vrt:%d term:%d rev6:%d\n",
			inst->eye_src_host, inst->eye_vrt_host,
			inst->eye_term_host, inst->rev6_host);
		dev_info(xsphy->dev, "u2_intr:%d term_cal:%d discth:%d\n",
			inst->efuse_intr, inst->efuse_term_cal, inst->discth);
		break;
	case PHY_TYPE_USB3:
		u3_phy_props_set(xsphy, inst);
		/* show default u3 driving setting */
		dev_info(xsphy->dev, "u3_intr:%d, tx-imp:%d, rx-imp:%d\n",
			inst->efuse_intr, inst->efuse_tx_imp,
			inst->efuse_rx_imp);
		break;
	default:
		dev_err(xsphy->dev, "incompatible phy type\n");
		clk_disable_unprepare(inst->ref_clk);
		return -EINVAL;
	}

	return 0;
}

static int mtk_phy_power_on(struct phy *phy)
{
	struct xsphy_instance *inst = phy_get_drvdata(phy);
	struct mtk_xsphy *xsphy = dev_get_drvdata(phy->dev.parent);

	if (inst->type == PHY_TYPE_USB2) {
		u2_phy_instance_power_on(xsphy, inst);
		u2_phy_slew_rate_calibrate(xsphy, inst);
		u2_phy_props_set(xsphy, inst);
	} else if (inst->type == PHY_TYPE_USB3) {
		u3_phy_instance_power_on(xsphy, inst);
		u3_phy_props_set(xsphy, inst);
	}

	return 0;
}

static int mtk_phy_power_off(struct phy *phy)
{
	struct xsphy_instance *inst = phy_get_drvdata(phy);
	struct mtk_xsphy *xsphy = dev_get_drvdata(phy->dev.parent);

	if (inst->type == PHY_TYPE_USB2)
		u2_phy_instance_power_off(xsphy, inst);
	else if (inst->type == PHY_TYPE_USB3)
		u3_phy_instance_power_off(xsphy, inst);

	return 0;
}

static int mtk_phy_exit(struct phy *phy)
{
	struct xsphy_instance *inst = phy_get_drvdata(phy);

	clk_disable_unprepare(inst->ref_clk);
	return 0;
}

static int mtk_phy_set_mode(struct phy *phy, enum phy_mode mode, int submode)
{
	struct xsphy_instance *inst = phy_get_drvdata(phy);
	struct mtk_xsphy *xsphy = dev_get_drvdata(phy->dev.parent);

	if (inst->type == PHY_TYPE_USB2)
		u2_phy_instance_set_mode(xsphy, inst, mode, submode);

	return 0;
}

static int mtk_phy_get_mode(struct mtk_xsphy *xsphy)
{
	struct device_node *of_chosen;
	char *bootargs;
	int mode = XSP_MODE_USB;

	of_chosen = of_find_node_by_path("/chosen");
	if (!of_chosen)
		goto done;

	bootargs = (char *)of_get_property(of_chosen,
			"bootargs", NULL);
	if (!bootargs)
		goto done;

	if (strstr(bootargs, XSP_MODE_UART_STR))
		mode = XSP_MODE_UART;
	else if (strstr(bootargs, XSP_MODE_JTAG_STR))
		mode = XSP_MODE_JTAG;

done:
	return mode;
}

static int mtk_phy_uart_init(struct phy *phy)
{
	struct xsphy_instance *inst = phy_get_drvdata(phy);
	struct mtk_xsphy *xsphy = dev_get_drvdata(phy->dev.parent);
	int ret;

	if  (inst->type != PHY_TYPE_USB2)
		return 0;

	dev_info(xsphy->dev, "uart init\n");

	ret = clk_prepare_enable(inst->ref_clk);
	if (ret) {
		dev_info(xsphy->dev, "failed to enable ref_clk\n");
		return ret;
	}
	udelay(250);

	return 0;
}

static int mtk_phy_uart_exit(struct phy *phy)
{
	struct xsphy_instance *inst = phy_get_drvdata(phy);
	struct mtk_xsphy *xsphy = dev_get_drvdata(phy->dev.parent);

	if  (inst->type != PHY_TYPE_USB2)
		return 0;

	dev_info(xsphy->dev, "uart exit\n");

	clk_disable_unprepare(inst->ref_clk);
	return 0;
}

static int mtk_phy_jtag_init(struct phy *phy)
{
	struct xsphy_instance *inst = phy_get_drvdata(phy);
	struct mtk_xsphy *xsphy = dev_get_drvdata(phy->dev.parent);
	void __iomem *pbase = inst->port_base;
	struct device *dev = &phy->dev;
	struct device_node *np = dev->of_node;
	struct of_phandle_args args;
	struct regmap *reg_base;
	u32 jtag_vers;
	u32 tmp;
	int ret;

	if  (inst->type != PHY_TYPE_USB2)
		return 0;

	dev_info(xsphy->dev, "jtag init\n");

	ret = of_parse_phandle_with_fixed_args(np, "usb2jtag", 1, 0, &args);
	if (ret)
		return ret;

	jtag_vers = args.args[0];
	reg_base = syscon_node_to_regmap(args.np);
	of_node_put(args.np);

	dev_info(xsphy->dev, "base - reg:0x%x, version:%d\n",
			reg_base, jtag_vers);

	ret = clk_prepare_enable(inst->ref_clk);
	if (ret) {
		dev_info(xsphy->dev, "failed to enable ref_clk\n");
		return ret;
	}

	tmp = readl(pbase + XSP_USBPHYACR4);
	tmp |= 0xf300;
	writel(tmp, pbase + XSP_USBPHYACR4);

	tmp = readl(pbase + XSP_USBPHYACR6);
	tmp &= 0xff7ffff;
	writel(tmp, pbase + XSP_USBPHYACR6);

	tmp = readl(pbase + XSP_USBPHYACR0);
	tmp |= 0x1;
	writel(tmp, pbase + XSP_USBPHYACR0);

	tmp = readl(pbase + XSP_USBPHYACR2);
	tmp &= 0xfffdffff;
	writel(tmp, pbase + XSP_USBPHYACR2);

	udelay(100);

	switch (jtag_vers) {
	case XSP_JTAG_V1:
		regmap_read(reg_base, 0xf00, &tmp);
		tmp |= 0x4030;
		regmap_write(reg_base, 0xf00, tmp);
		break;
	case XSP_JTAG_V2:
		regmap_read(reg_base, 0x100, &tmp);
		tmp |= 0x2;
		regmap_write(reg_base, 0x100, tmp);
		break;
	default:
		break;
	}

	return 0;
}

static int mtk_phy_jtag_exit(struct phy *phy)
{
	struct xsphy_instance *inst = phy_get_drvdata(phy);
	struct mtk_xsphy *xsphy = dev_get_drvdata(phy->dev.parent);

	if  (inst->type != PHY_TYPE_USB2)
		return 0;

	dev_info(xsphy->dev, "jtag exit\n");

	clk_disable_unprepare(inst->ref_clk);
	return 0;
}

static struct phy *mtk_phy_xlate(struct device *dev,
				 struct of_phandle_args *args)
{
	struct mtk_xsphy *xsphy = dev_get_drvdata(dev);
	struct xsphy_instance *inst = NULL;
	struct device_node *phy_np = args->np;
	int index;

	if (args->args_count != 1) {
		dev_err(dev, "invalid number of cells in 'phy' property\n");
		return ERR_PTR(-EINVAL);
	}

	for (index = 0; index < xsphy->nphys; index++)
		if (phy_np == xsphy->phys[index]->phy->dev.of_node) {
			inst = xsphy->phys[index];
			break;
		}

	if (!inst) {
		dev_err(dev, "failed to find appropriate phy\n");
		return ERR_PTR(-EINVAL);
	}

	inst->type = args->args[0];
	if (!(inst->type == PHY_TYPE_USB2 ||
	      inst->type == PHY_TYPE_USB3)) {
		dev_err(dev, "unsupported phy type: %d\n", inst->type);
		return ERR_PTR(-EINVAL);
	}

	phy_parse_property(xsphy, inst);
	phy_parse_efuse_property(xsphy, inst);

	return inst->phy;
}

static const struct phy_ops mtk_xsphy_uart_ops = {
	.init		= mtk_phy_uart_init,
	.exit		= mtk_phy_uart_exit,
	.owner		= THIS_MODULE,
};

static const struct phy_ops mtk_xsphy_jtag_ops = {
	.init		= mtk_phy_jtag_init,
	.exit		= mtk_phy_jtag_exit,
	.owner		= THIS_MODULE,
};

static const struct phy_ops mtk_xsphy_ops = {
	.init		= mtk_phy_init,
	.exit		= mtk_phy_exit,
	.power_on	= mtk_phy_power_on,
	.power_off	= mtk_phy_power_off,
	.set_mode	= mtk_phy_set_mode,
	.owner		= THIS_MODULE,
};

static const struct of_device_id mtk_xsphy_id_table[] = {
	{ .compatible = "mediatek,xsphy", },
	{ },
};
MODULE_DEVICE_TABLE(of, mtk_xsphy_id_table);

static int mtk_xsphy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child_np;
	struct phy_provider *provider;
	struct resource *glb_res;
	struct mtk_xsphy *xsphy;
	struct resource res;
	int port, retval;

	xsphy = devm_kzalloc(dev, sizeof(*xsphy), GFP_KERNEL);
	if (!xsphy)
		return -ENOMEM;

	xsphy->nphys = of_get_child_count(np);
	xsphy->phys = devm_kcalloc(dev, xsphy->nphys,
				       sizeof(*xsphy->phys), GFP_KERNEL);
	if (!xsphy->phys)
		return -ENOMEM;

	xsphy->dev = dev;
	platform_set_drvdata(pdev, xsphy);

	glb_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	/* optional, may not exist if no u3 phys */
	if (glb_res) {
		/* get banks shared by multiple u3 phys */
		xsphy->glb_base = devm_ioremap_resource(dev, glb_res);
		if (IS_ERR(xsphy->glb_base)) {
			dev_err(dev, "failed to remap glb regs\n");
			return PTR_ERR(xsphy->glb_base);
		}
	}

	xsphy->src_ref_clk = XSP_REF_CLK;
	xsphy->src_coef = XSP_SLEW_RATE_COEF;
	/* update parameters of slew rate calibrate if exist */
	device_property_read_u32(dev, "mediatek,src-ref-clk-mhz",
				 &xsphy->src_ref_clk);
	device_property_read_u32(dev, "mediatek,src-coef", &xsphy->src_coef);

	port = 0;
	for_each_child_of_node(np, child_np) {
		struct xsphy_instance *inst;
		struct phy *phy;
		int mode;

		inst = devm_kzalloc(dev, sizeof(*inst), GFP_KERNEL);
		if (!inst) {
			retval = -ENOMEM;
			goto put_child;
		}

		xsphy->phys[port] = inst;

		/* change ops to usb uart or jtage mode */
		mode = mtk_phy_get_mode(xsphy);
		switch (mode) {
		case XSP_MODE_UART:
			phy = devm_phy_create(dev, child_np,
				&mtk_xsphy_uart_ops);
			break;
		case XSP_MODE_JTAG:
			phy = devm_phy_create(dev, child_np,
				&mtk_xsphy_jtag_ops);
			break;
		default:
			phy = devm_phy_create(dev, child_np,
				&mtk_xsphy_ops);
		}

		if (IS_ERR(phy)) {
			dev_err(dev, "failed to create phy\n");
			retval = PTR_ERR(phy);
			goto put_child;
		}

		retval = of_address_to_resource(child_np, 0, &res);
		if (retval) {
			dev_err(dev, "failed to get address resource(id-%d)\n",
				port);
			goto put_child;
		}

		inst->port_base = devm_ioremap_resource(&phy->dev, &res);
		if (IS_ERR(inst->port_base)) {
			dev_err(dev, "failed to remap phy regs\n");
			retval = PTR_ERR(inst->port_base);
			goto put_child;
		}

		inst->phy = phy;
		inst->index = port;
		phy_set_drvdata(phy, inst);
		port++;

		inst->ref_clk = devm_clk_get(&phy->dev, "ref");
		if (IS_ERR(inst->ref_clk)) {
			dev_err(dev, "failed to get ref_clk(id-%d)\n", port);
			retval = PTR_ERR(inst->ref_clk);
			goto put_child;
		}
	}

	provider = devm_of_phy_provider_register(dev, mtk_phy_xlate);
	return PTR_ERR_OR_ZERO(provider);

put_child:
	of_node_put(child_np);
	return retval;
}

static struct platform_driver mtk_xsphy_driver = {
	.probe		= mtk_xsphy_probe,
	.driver		= {
		.name	= "mtk-xsphy",
		.of_match_table = mtk_xsphy_id_table,
	},
};

module_platform_driver(mtk_xsphy_driver);

MODULE_AUTHOR("Chunfeng Yun <chunfeng.yun@mediatek.com>");
MODULE_DESCRIPTION("MediaTek USB XS-PHY driver");
MODULE_LICENSE("GPL v2");
