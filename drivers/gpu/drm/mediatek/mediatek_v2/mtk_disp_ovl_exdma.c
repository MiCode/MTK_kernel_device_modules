// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <drm/drm_blend.h>
#include <drm/drm_framebuffer.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/ratelimit.h>
#include <linux/iommu.h>
#include <arm/arm-smmu-v3/arm-smmu-v3.h>
#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_drv.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_layering_rule_base.h"
#include "mtk_rect.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_graphics_base.h"
#include "mtk_drm_helper.h"
#include "mtk_drm_drv.h"
#include "platform/mtk_ovl_exdma_reg.h"
#include "platform/mtk_disp_ovlsys_reg.h"
#include "mtk_disp_ovl_exdma.h"
#include "mtk_disp_pmqos.h"
#ifdef IF_ZERO
#include "mtk_iommu_ext.h"
#endif
#include "mtk_layer_layout_trace.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_gem.h"
#include "platform/mtk_drm_platform.h"

#include "slbc_ops.h"
#include "../mml/mtk-mml.h"
#include <soc/mediatek/smi.h>
#include "mtk-smmu-v3.h"

int mtk_dprec_mmp_dump_ovl_layer(struct mtk_plane_state *plane_state);

int debug_module_bw[MAX_LAYER_NR];
module_param_array(debug_module_bw, int, NULL, 0644);

int debug_channel_wo_compress;
module_param(debug_channel_wo_compress, int, 0644);

#define REG_FLD(width, shift)                                                  \
	((unsigned int)((((width)&0xFF) << 16) | ((shift)&0xFF)))

#define REG_FLD_MSB_LSB(msb, lsb) REG_FLD((msb) - (lsb) + 1, (lsb))

#define REG_FLD_WIDTH(field) ((unsigned int)(((field) >> 16) & 0xFF))

#define REG_FLD_SHIFT(field) ((unsigned int)((field)&0xFF))

#define REG_FLD_MASK(field)                                                    \
	((unsigned int)((1ULL << REG_FLD_WIDTH(field)) - 1)                    \
	 << REG_FLD_SHIFT(field))

/*ext layer define*/
#define OVL_EXDMA_ELX_EN(module, n) \
	((module)->data->regs[OVL_EXDMA_EL0_EN] + 0x30 * (n))

#define OVL_EXDMA_ELX_CLRFMT(module, n) \
	((module)->data->regs[OVL_EXDMA_EL0_CLRFMT] + 0x30 * (n))

#define OVL_EXDMA_ELX_OFFSET(module, n) \
	((module)->data->regs[OVL_EXDMA_EL0_OFFSET] + 0x30 * (n))
#define OVL_EXDMA_ELX_SRC_SIZE(module, n) \
	((module)->data->regs[OVL_EXDMA_EL0_SRC_SIZE] + 0x30 * (n))
#define OVL_EXDMA_ELX_CLIP(module, n) \
		((module)->data->regs[OVL_EXDMA_EL0_CLIP] + 0x30 * (n))

#define OVL_EXDMA_ELX_R2R_PARA(module, n) \
	((module)->data->regs[OVL_EXDMA_EL0_R2R_PARA_R0] + 0x40 * (n))

#define OVL_EXDMA_ELX_ADDR(module, n) \
	((module)->data->regs[OVL_EXDMA_EL0_ADDR] + \
	(module)->data->el_addr_offset * (n))

#define OVL_EXDMA_ELX_CON(module, n) \
	((module)->data->regs[OVL_EXDMA_EL0_CON2] + 0x20 * (n))
#define OVL_EXDMA_ELX_SRCKEY(module, n) \
	((module)->data->regs[OVL_EXDMA_EL0_SRCKEY] + 0x20 * (n))

#define OVL_EXDMA_ELX_CLR(module, n) \
	((module)->data->regs[OVL_EXDMA_EL0_CLR] + 0x4 * (n))

#define OVL_EXDMA_ELX_PITCH_MSB(module, n) \
	((module)->data->regs[OVL_EXDMA_EL0_PITCH_MSB] + 0x20 * (n))
#define OVL_EXDMA_ELX_PITCH(module, n) \
	((module)->data->regs[OVL_EXDMA_EL0_PITCH] + 0x20 * (n))
#define OVL_EXDMA_ELX_TILE(module, n) \
	((module)->data->regs[OVL_EXDMA_EL0_TILE] + 0x20 * (n))
#define OVL_EXDMA_ELX_GUSER_EXT(module, n) \
	((module)->data->regs[OVL_EXDMA_EL0_GUSER_EXT] + 0x20 * (n))

#define OVL_EXDMA_ELX_SYSRAM_CFG(module, n) \
	((module)->data->regs[OVL_EXDMA_EL0_SYSRAM_CFG] + 0x10 * n)
#define OVL_EXDMA_ELX_SYSRAM_BUF0_ADDR(module, n) \
	((module)->data->regs[OVL_EXDMA_EL0_BUF0_ADDR] + 0x10 * n)
#define OVL_EXDMA_ELX_SYSRAM_BUF1_ADDR(module, n) \
	((module)->data->regs[OVL_EXDMA_EL0_BUF1_ADDR] + 0x10 * n)

#define OVL_EXDMA_ELX_ADDR_MSB(module, n) \
	((module)->data->regs[OVL_EXDMA_EL0_ADDR_MSB] + \
	(module)->data->el_addr_offset * (n))

#define OVL_EXDMA_ELX_HDR_ADDR(module, n) \
	((module)->data->regs[OVL_EXDMA_EL0_HDR_ADDR] + \
	(module)->data->el_hdr_addr_offset * (n))

#define OVL_EXDMA_ELX_HDR_PITCH(module, n) \
	((module)->data->regs[OVL_EXDMA_EL0_HDR_PITCH] + \
	(module)->data->el_hdr_addr_offset * (n))

/*Spefic value define*/

/*OVL_EXDMA_EN_CON*/
#define OP_8_BIT_MODE					BIT(4)

/*OVL_EXDMA_EN*/
#define ENABLE_OVL_EN					BIT(0)
#define ENABLE_FORCE_RELAY_MODE			BIT(4)

/*OVL_EXDMA_SHADOW_CTRL*/
#define OVL_EXDMA_BYPASS_SHADOW			BIT(2)

/*OVL_EXDMA_L0_EN*/
#define ENABLE_OVL_L_EN					BIT(0)
#define LSRC_COLOR						BIT(8)
#define LSRC_UFOD						BIT(9)
#define LSRC_PQ							(BIT(8) | BIT(9))

/*OVL_EXDMA_DATAPATH_CON*/
#define OVL_EXDMA_BGCLR_IN_SEL			BIT(2)
#define OVL_EXDMA_OUTPUT_CLAMP			BIT(26)

/*OVL_L0_EXDMA_CLRFMT*/
#define OVL_EXDMA_CON_CLRFMT_MAN		BIT(4)
#define OVL_EXDMA_CON_BYTE_SWAP			BIT(16)
#define OVL_EXDMA_CON_RGB_SWAP			BIT(17)

/*OVL_EXDMA_SRC_CON*/
#define OVL_EXDMA_FORCE_CONSTANT_LAYER	BIT(4)

/*OVL_EXDMA_CON2*/
#define CON_VERTICAL_FLIP				BIT(9)
#define CON_HORI_FLIP					BIT(10)

/*OVL_EXDMA_PITCH*/
#define OVL_EXDMA_LX_CONST_BLD			BIT(28)

/*OVL_EXDMA_DUMMY_REG*/
#define DISP_OVL_EXT_DDR_EN_OPT			BIT(2)
#define DISP_OVL_FORCE_EXT_DDR_EN		BIT(3)

/*OVL_EXDMA_FBDC_CFG1*/
#define FBDC_8XE_MODE					BIT(24)
#define FBDC_FILTER_EN					BIT(28)


/*---------------------------------------------------------------------------------------------*/
/*OLV EXDMA Reg Bit Field Define*/

/*OVL_EXDMA_WCG_CFG1*/
#define FLD_ELn_IGAMMA_EN(n)	REG_FLD_MSB_LSB((n)*4 + 16, (n)*4 + 16)
#define FLD_ELn_CSC_EN(n)		REG_FLD_MSB_LSB((n)*4 + 17, (n)*4 + 17)
#define FLD_ELn_GAMMA_EN(n)		REG_FLD_MSB_LSB((n)*4 + 18, (n)*4 + 18)

/*OVL_EXDMA_WCG_CFG2*/
#define FLD_ELn_IGAMMA_SEL(n)	REG_FLD_MSB_LSB((n)*4 + 17, (n)*4 + 16)
#define FLD_ELn_GAMMA_SEL(n)	REG_FLD_MSB_LSB((n)*4 + 19, (n)*4 + 18)

/*OLV EXDMA Reg Bit Field Define*/

#define OVL_LAYER_OFFSET				(0x1)
#define OVL_RDMA_DEBUG_OFFSET (0x4)
#define OVL_ROI_BGCLR (0xFF000000)

#define OVL_CON_MTX_JPEG_TO_RGB			(0x4UL << 16)
#define OVL_CON_MTX_BT709_FULL_TO_RGB	(0x5UL << 16)
#define OVL_CON_MTX_BT601_TO_RGB		(0x6UL << 16)
#define OVL_CON_MTX_BT709_TO_RGB		(0x7UL << 16)
#define OVL_CON_MTX_BT2020_FULL_TO_RGB	(0x8UL << 16)
#define OVL_CON_MTX_BT2020_TO_RGB		(0x9UL << 16)
#define OVL_CON_MTX_P3_FULL_TO_RGB		(0xAUL << 16)
#define OVL_CON_MTX_P3_TO_RGB			(0xBUL << 16)
	#define OVL_CON_CLRFMT_RGB (1UL)
	#define OVL_CON_CLRFMT_RGBA8888 (2)
	#define OVL_CON_CLRFMT_ARGB8888 (3)
#define OVL_CON_CLRFMT_DIM (1 << 8)
#define OVL_CON_CLRFMT_RGB565(module)                                          \
	(((module)->data->fmt_rgb565_is_0 == true) ? 0UL : OVL_CON_CLRFMT_RGB)
#define OVL_CON_CLRFMT_RGB888(module)                                          \
	(((module)->data->fmt_rgb565_is_0 == true) ? OVL_CON_CLRFMT_RGB : 0UL)
#define OVL_CON_CLRFMT_UYVY(module) ((module)->data->fmt_uyvy)
#define OVL_CON_CLRFMT_YUYV(module) ((module)->data->fmt_yuyv)
#define OVL_CON_AEN BIT(8)
#define OVL_CON_ALPHA 0xff

#define M4U_PORT_DISP_OVL0_HDR 1
#define M4U_PORT_DISP_OVL0 3
#define M4U_PORT_DISP_OVL0_2L_HDR ((1 << 5) + 0)
#define M4U_PORT_DISP_OVL0_2L ((1 << 5) + 2)

#define MT6985_OVL_LAYER_OFFEST				(0x4)

#define OVL_MOUT	(0xFF0UL)
	#define OVL_MOUT_OUT_DATA		BIT(0)
	#define OVL_MOUT_BGCLR_OUT		BIT(1)
	#define FLD_OVL_MOUT			REG_FLD_MSB_LSB(1, 0)

#define SMI_LARB_NON_SEC_CON        0x380
#define DISP_REG_OVL_SMI_2ND_CFG	(0x8F0)

#define MML_SRAM_SHIFT (512*1024)

/*OVL_UNDO_ALPHA*/
#define EL_UNDO_ALPHA(n) REG_FLD_MSB_LSB(0 + 8 * (n), 0 + 8 * (n))
#define EL_APPLY_ALPHA(n) REG_FLD_MSB_LSB(4 + 8 * (n), 4 + 8 * (n))

/*OVL_UNDO_ALPHA_OFFSET*/
#define EL_UNDO_ALPHA_OFFSET(n) REG_FLD_MSB_LSB(7 + 8 * (n), 0 + 8 * (n))

enum GS_OVL_FLD {
	GS_OVL_RDMA_ULTRA_TH = 0,
	GS_OVL_RDMA_PRE_ULTRA_TH,
	GS_OVL_RDMA_FIFO_THRD,
	GS_OVL_RDMA_FIFO_SIZE,
	GS_OVL_RDMA_ISSUE_REQ_TH,
	GS_OVL_RDMA_ISSUE_REQ_TH_URG,
	GS_OVL_RDMA_REQ_TH_PRE_ULTRA,
	GS_OVL_RDMA_REQ_TH_ULTRA,
	GS_OVL_RDMA_FORCE_REQ_TH,
	GS_OVL_RDMA_GREQ_NUM,     /* whole reg */
	GS_OVL_RDMA_GREQ_URG_NUM, /* whole reg */
	GS_OVL_RDMA_ULTRA_SRC,    /* whole reg */
	GS_OVL_RDMA_ULTRA_LOW_TH,
	GS_OVL_RDMA_PRE_ULTRA_LOW_TH,
	GS_OVL_RDMA_PRE_ULTRA_HIGH_TH,
	GS_OVL_RDMA_PRE_ULTRA_HIGH_DIS,
	GS_OVL_BLOCK_EXT_ULTRA,
	GS_OVL_BLOCK_EXT_PRE_ULTRA,
	GS_OVL_APB_MCYC_RD,
	GS_OVL_STASH_EN,
	GS_OVL_STASH_CFG,
	GS_OVL_FLD_NUM,
};

#define CSC_COEF_NUM 9

static s32 sRGB_to_DCI_P3[CSC_COEF_NUM] = {
215603,  46541,      0,
8702,   253442,      0,
4478,    18979, 238687};

static s32 sRGB_to_BT2020[CSC_COEF_NUM] = {
164470,  86320,  11354,
18113,  241052,   2979,
4297,    23072, 234775};

static s32 DCI_P3_to_sRGB[CSC_COEF_NUM] = {
321111, -58967,      0,
-11025, 273169,      0,
-5148,  -20614, 287906};

static s32 DCI_P3_to_BT2020[CSC_COEF_NUM] = {
197613,  52061,  12470,
11991,  246881,   3271,
-317,     4614, 257847};

static s32 BT2020_to_sRGB[CSC_COEF_NUM] = {
435288,-154047, -19097,
-32650, 296983,  -2189,
-4758,  -26366, 293268};

static s32 BT2020_to_DCI_P3[CSC_COEF_NUM] = {
352211, -73972, -16095,
-17117, 282011,  -2750,
740,     -5138, 266542};

static s32 identity[CSC_COEF_NUM] = {
262144,      0,      0,
0,      262144,      0,
0,           0, 262144};


#define DECLARE_MTK_OVL_COLORSPACE(EXPR)                                       \
	{EXPR(OVL_SRGB)                                                         \
	EXPR(OVL_P3)                                                           \
	EXPR(OVL_BT2020)                                                       \
	EXPR(OVL_CS_NUM)                                                       \
	EXPR(OVL_CS_UNKNOWN)}

enum mtk_ovl_colorspace  DECLARE_MTK_OVL_COLORSPACE(DECLARE_NUM);

static const char * const mtk_ovl_colorspace_str[] =
	DECLARE_MTK_OVL_COLORSPACE(DECLARE_STR);

#define DECLARE_MTK_OVL_TRANSFER(EXPR)                                         \
	{EXPR(OVL_GAMMA2_2)                                                     \
	EXPR(OVL_GAMMA2)                                                       \
	EXPR(OVL_LINEAR)                                                       \
	EXPR(OVL_GAMMA_NUM)                                                    \
	EXPR(OVL_GAMMA_UNKNOWN)}

enum mtk_ovl_transfer  DECLARE_MTK_OVL_TRANSFER(DECLARE_NUM);

static const char * const mtk_ovl_transfer_str[] =
	DECLARE_MTK_OVL_TRANSFER(DECLARE_STR);

#define MAX_LAYER_NUM 1
struct mtk_ovl_backup_info {
	unsigned int layer;
	unsigned int layer_en;
	unsigned int con;
	dma_addr_t addr;
	unsigned int src_size;
	unsigned int src_pitch;
	unsigned int data_path_con;
};

/**
 * struct mtk_disp_ovl_exdma - DISP_OVL driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 * @crtc - associated crtc to report vblank events to
 */
struct mtk_disp_ovl_exdma {
	struct mtk_ddp_comp ddp_comp;
	const struct mtk_disp_ovl_exdma_data *data;
	unsigned int underflow_cnt;
	bool ovl_dis;
	int bg_w, bg_h;
	struct clk *fbdc_clk;
	struct mtk_ovl_backup_info backup_info[MAX_LAYER_NUM];
	unsigned int set_partial_update;
	unsigned int roi_height;
};

#define CEILING(n, s) ((n) + ((s) - ((n) % (s))))

static inline struct mtk_disp_ovl_exdma *comp_to_ovl_exdma(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_ovl_exdma, ddp_comp);
}

int mtk_ovl_exdma_layer_num(struct mtk_ddp_comp *comp)
{
	return 1;
}
resource_size_t mtk_ovl_mmsys_mapping_MT6991(struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	switch (comp->id) {
	case DDP_COMPONENT_OVL_EXDMA0:
	case DDP_COMPONENT_OVL_EXDMA1:
	case DDP_COMPONENT_OVL_EXDMA2:
	case DDP_COMPONENT_OVL_EXDMA3:
	case DDP_COMPONENT_OVL_EXDMA4:
	case DDP_COMPONENT_OVL_EXDMA5:
	case DDP_COMPONENT_OVL_EXDMA6:
	case DDP_COMPONENT_OVL_EXDMA7:
	case DDP_COMPONENT_OVL_EXDMA8:
	case DDP_COMPONENT_OVL_EXDMA9:
		return priv->ovlsys0_regs_pa;
	case DDP_COMPONENT_OVL1_EXDMA0:
	case DDP_COMPONENT_OVL1_EXDMA1:
	case DDP_COMPONENT_OVL1_EXDMA2:
	case DDP_COMPONENT_OVL1_EXDMA3:
	case DDP_COMPONENT_OVL1_EXDMA4:
	case DDP_COMPONENT_OVL1_EXDMA5:
	case DDP_COMPONENT_OVL1_EXDMA6:
	case DDP_COMPONENT_OVL1_EXDMA7:
	case DDP_COMPONENT_OVL1_EXDMA8:
	case DDP_COMPONENT_OVL1_EXDMA9:
		return priv->ovlsys1_regs_pa;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

unsigned int mtk_ovl_sys_mapping_MT6991(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL_EXDMA0:
	case DDP_COMPONENT_OVL_EXDMA1:
	case DDP_COMPONENT_OVL_EXDMA2:
	case DDP_COMPONENT_OVL_EXDMA3:
	case DDP_COMPONENT_OVL_EXDMA4:
	case DDP_COMPONENT_OVL_EXDMA5:
	case DDP_COMPONENT_OVL_EXDMA6:
	case DDP_COMPONENT_OVL_EXDMA7:
	case DDP_COMPONENT_OVL_EXDMA8:
	case DDP_COMPONENT_OVL_EXDMA9:
		return 0;
	case DDP_COMPONENT_OVL1_EXDMA0:
	case DDP_COMPONENT_OVL1_EXDMA1:
	case DDP_COMPONENT_OVL1_EXDMA2:
	case DDP_COMPONENT_OVL1_EXDMA3:
	case DDP_COMPONENT_OVL1_EXDMA4:
	case DDP_COMPONENT_OVL1_EXDMA5:
	case DDP_COMPONENT_OVL1_EXDMA6:
	case DDP_COMPONENT_OVL1_EXDMA7:
	case DDP_COMPONENT_OVL1_EXDMA8:
	case DDP_COMPONENT_OVL1_EXDMA9:
		return 1;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return -1;
	}
}

resource_size_t mtk_ovl_mmsys_mapping_MT6993(struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	switch (comp->id) {
	case DDP_COMPONENT_OVL_EXDMA0:
	case DDP_COMPONENT_OVL_EXDMA1:
	case DDP_COMPONENT_OVL_EXDMA2:
	case DDP_COMPONENT_OVL_EXDMA3:
	case DDP_COMPONENT_OVL_EXDMA4:
	case DDP_COMPONENT_OVL_EXDMA5:
	case DDP_COMPONENT_OVL_EXDMA6:
	case DDP_COMPONENT_OVL_EXDMA7:
		return priv->ovlsys0_regs_pa;
	case DDP_COMPONENT_OVL1_EXDMA0:
	case DDP_COMPONENT_OVL1_EXDMA1:
	case DDP_COMPONENT_OVL1_EXDMA2:
	case DDP_COMPONENT_OVL1_EXDMA3:
	case DDP_COMPONENT_OVL1_EXDMA4:
	case DDP_COMPONENT_OVL1_EXDMA5:
	case DDP_COMPONENT_OVL1_EXDMA6:
	case DDP_COMPONENT_OVL1_EXDMA7:
		return priv->ovlsys1_regs_pa;
	case DDP_COMPONENT_OVL2_EXDMA0:
	case DDP_COMPONENT_OVL2_EXDMA1:
	case DDP_COMPONENT_OVL2_EXDMA2:
	case DDP_COMPONENT_OVL2_EXDMA3:
	case DDP_COMPONENT_OVL2_EXDMA4:
	case DDP_COMPONENT_OVL2_EXDMA5:
	case DDP_COMPONENT_OVL2_EXDMA6:
	case DDP_COMPONENT_OVL2_EXDMA7:
		return priv->ovlsys2_regs_pa;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

unsigned int mtk_ovl_sys_mapping_MT6993(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL_EXDMA0:
	case DDP_COMPONENT_OVL_EXDMA1:
	case DDP_COMPONENT_OVL_EXDMA2:
	case DDP_COMPONENT_OVL_EXDMA3:
	case DDP_COMPONENT_OVL_EXDMA4:
	case DDP_COMPONENT_OVL_EXDMA5:
	case DDP_COMPONENT_OVL_EXDMA6:
	case DDP_COMPONENT_OVL_EXDMA7:
		return 0;
	case DDP_COMPONENT_OVL1_EXDMA0:
	case DDP_COMPONENT_OVL1_EXDMA1:
	case DDP_COMPONENT_OVL1_EXDMA2:
	case DDP_COMPONENT_OVL1_EXDMA3:
	case DDP_COMPONENT_OVL1_EXDMA4:
	case DDP_COMPONENT_OVL1_EXDMA5:
	case DDP_COMPONENT_OVL1_EXDMA6:
	case DDP_COMPONENT_OVL1_EXDMA7:
		return 1;
	case DDP_COMPONENT_OVL2_EXDMA0:
	case DDP_COMPONENT_OVL2_EXDMA1:
	case DDP_COMPONENT_OVL2_EXDMA2:
	case DDP_COMPONENT_OVL2_EXDMA3:
	case DDP_COMPONENT_OVL2_EXDMA4:
	case DDP_COMPONENT_OVL2_EXDMA5:
	case DDP_COMPONENT_OVL2_EXDMA6:
	case DDP_COMPONENT_OVL2_EXDMA7:
		return 2;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return -1;
	}
}

unsigned int mtk_ovl_aid_sel(struct mtk_ddp_comp *comp)
{
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *priv;

	mtk_crtc = comp->mtk_crtc;
	crtc = &mtk_crtc->base;
	priv = crtc->dev->dev_private;


	switch (comp->id) {
	case DDP_COMPONENT_OVL_EXDMA0:
	case DDP_COMPONENT_OVL1_EXDMA0:
	case DDP_COMPONENT_OVL2_EXDMA0:
		return priv->reg_data->ovlsys_regs[OVL_EXDMA0_L0_AID_SETTING];
	case DDP_COMPONENT_OVL_EXDMA1:
	case DDP_COMPONENT_OVL1_EXDMA1:
	case DDP_COMPONENT_OVL2_EXDMA1:
		return priv->reg_data->ovlsys_regs[OVL_EXDMA1_L0_AID_SETTING];
	case DDP_COMPONENT_OVL_EXDMA2:
	case DDP_COMPONENT_OVL1_EXDMA2:
	case DDP_COMPONENT_OVL2_EXDMA2:
		return priv->reg_data->ovlsys_regs[OVL_EXDMA2_L0_AID_SETTING];
	case DDP_COMPONENT_OVL_EXDMA3:
	case DDP_COMPONENT_OVL1_EXDMA3:
	case DDP_COMPONENT_OVL2_EXDMA3:
		return priv->reg_data->ovlsys_regs[OVL_EXDMA3_L0_AID_SETTING];
	case DDP_COMPONENT_OVL_EXDMA4:
	case DDP_COMPONENT_OVL1_EXDMA4:
	case DDP_COMPONENT_OVL2_EXDMA4:
		return priv->reg_data->ovlsys_regs[OVL_EXDMA4_L0_AID_SETTING];
	case DDP_COMPONENT_OVL_EXDMA5:
	case DDP_COMPONENT_OVL1_EXDMA5:
	case DDP_COMPONENT_OVL2_EXDMA5:
		return priv->reg_data->ovlsys_regs[OVL_EXDMA5_L0_AID_SETTING];
	case DDP_COMPONENT_OVL_EXDMA6:
	case DDP_COMPONENT_OVL1_EXDMA6:
	case DDP_COMPONENT_OVL2_EXDMA6:
		return priv->reg_data->ovlsys_regs[OVL_EXDMA6_L0_AID_SETTING];
	case DDP_COMPONENT_OVL_EXDMA7:
	case DDP_COMPONENT_OVL1_EXDMA7:
	case DDP_COMPONENT_OVL2_EXDMA7:
		return priv->reg_data->ovlsys_regs[OVL_EXDMA7_L0_AID_SETTING];
	case DDP_COMPONENT_OVL_EXDMA8:
	case DDP_COMPONENT_OVL1_EXDMA8:
		return priv->reg_data->ovlsys_regs[OVL_EXDMA8_L0_AID_SETTING];
	case DDP_COMPONENT_OVL_EXDMA9:
	case DDP_COMPONENT_OVL1_EXDMA9:
		return priv->reg_data->ovlsys_regs[OVL_EXDMA9_L0_AID_SETTING];
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

static unsigned int mtk_ovl_phy_mapping_MT6991(struct mtk_ddp_comp *comp)
{
	/* sub_comm0: exdma2(0) + exdma7(5) + 1_exdma5(11) + (1_exdma8)
	 * sub_comm1: exdma3(1) + exdma6(4) + 1_exdma4(10) + (1_exdma9)
	 * sub_comm2: exdma4(2) + exdma9(7) + 1_exdma3(9) + (1_exdma6)
	 * sub_comm3: exdma5(3) + exdma8(6) + 1_exdma2(8) + (1_exdma7)
	 */
	switch (comp->id) {
	case DDP_COMPONENT_OVL_EXDMA2:
		return 0;
	case DDP_COMPONENT_OVL_EXDMA3:
		return 1;
	case DDP_COMPONENT_OVL_EXDMA4:
		return 2;
	case DDP_COMPONENT_OVL_EXDMA5:
		return 3;
	case DDP_COMPONENT_OVL_EXDMA6:
		return 4;
	case DDP_COMPONENT_OVL_EXDMA7:
		return 5;
	case DDP_COMPONENT_OVL_EXDMA8:
		return 6;
	case DDP_COMPONENT_OVL_EXDMA9:
		return 7;
	case DDP_COMPONENT_OVL1_EXDMA2:
		return 8;
	case DDP_COMPONENT_OVL1_EXDMA3:
		return 9;
	case DDP_COMPONENT_OVL1_EXDMA4:
		return 10;
	case DDP_COMPONENT_OVL1_EXDMA5:
		return 11;
	case DDP_COMPONENT_OVL1_EXDMA6:
		return 12;
	case DDP_COMPONENT_OVL1_EXDMA7:
		return 13;
	case DDP_COMPONENT_OVL1_EXDMA8:
		return 14;
	case DDP_COMPONENT_OVL1_EXDMA9:
		return 15;
	case DDP_COMPONENT_OVL_EXDMA0:
	case DDP_COMPONENT_OVL1_EXDMA0:
		return 16; // no use
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

static unsigned int mtk_ovl_phy_channel_mapping_MT6991(struct mtk_ddp_comp *comp)
{
	/* channel0: sub_comm0: exdma2(0) + exdma7(5) + 1_exdma5(11) + (1_exdma8)
	 * channel1: sub_comm1: exdma3(1) + exdma6(4) + 1_exdma4(10) + (1_exdma9)
	 * channel3: sub_comm2: exdma4(2) + exdma9(7) + 1_exdma3(9) + (1_exdma6)
	 * channel2: sub_comm3: exdma5(3) + exdma8(6) + 1_exdma2(8) + (1_exdma7)
	 */
	switch (comp->id) {
	case DDP_COMPONENT_OVL_EXDMA2:
	case DDP_COMPONENT_OVL_EXDMA7:
	case DDP_COMPONENT_OVL1_EXDMA5:
	case DDP_COMPONENT_OVL1_EXDMA8:
		return 0;
	case DDP_COMPONENT_OVL_EXDMA3:
	case DDP_COMPONENT_OVL_EXDMA6:
	case DDP_COMPONENT_OVL1_EXDMA4:
	case DDP_COMPONENT_OVL1_EXDMA9:
		return 1;
	case DDP_COMPONENT_OVL_EXDMA4:
	case DDP_COMPONENT_OVL_EXDMA9:
	case DDP_COMPONENT_OVL1_EXDMA3:
	case DDP_COMPONENT_OVL1_EXDMA6:
		return 3;
	case DDP_COMPONENT_OVL_EXDMA5:
	case DDP_COMPONENT_OVL_EXDMA8:
	case DDP_COMPONENT_OVL1_EXDMA2:
	case DDP_COMPONENT_OVL1_EXDMA7:
		return 2;
	case DDP_COMPONENT_OVL_EXDMA0:
	case DDP_COMPONENT_OVL1_EXDMA0:
		return 4; // no use
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 4;
	}
}

static unsigned int mtk_ovl_phy_mapping_MT6993(struct mtk_ddp_comp *comp)
{
	/* channel0: sub_comm2: exdma3(1) +   exdma6(4) + 1_exdma5(9) +  2_exdma2(12)
	 * channel1: sub_comm3: exdma4(2) + 1_exdma3(7) + 1_exdma6(10) + 2_exdma5(15)
	 * channel3: sub_comm1: exdma5(3) + 1_exdma2(6) + 1_exdma7(11) + 2_exdma4(14)
	 * channel2: sub_comm0: exdma2(0) +   exdma7(5) + 1_exdma4(8) +  2_exdma3(13)
	 */
	switch (comp->id) {
	case DDP_COMPONENT_OVL_EXDMA2:
		return 0;
	case DDP_COMPONENT_OVL_EXDMA3:
		return 1;
	case DDP_COMPONENT_OVL_EXDMA4:
		return 2;
	case DDP_COMPONENT_OVL_EXDMA5:
		return 3;
	case DDP_COMPONENT_OVL_EXDMA6:
		return 4;
	case DDP_COMPONENT_OVL_EXDMA7:
		return 5;
	case DDP_COMPONENT_OVL1_EXDMA2:
		return 6;
	case DDP_COMPONENT_OVL1_EXDMA3:
		return 7;
	case DDP_COMPONENT_OVL1_EXDMA4:
		return 8;
	case DDP_COMPONENT_OVL1_EXDMA5:
		return 9;
	case DDP_COMPONENT_OVL1_EXDMA6:
		return 10;
	case DDP_COMPONENT_OVL1_EXDMA7:
		return 11;
	case DDP_COMPONENT_OVL2_EXDMA2:
		return 12;
	case DDP_COMPONENT_OVL2_EXDMA3:
		return 13;
	case DDP_COMPONENT_OVL2_EXDMA4:
		return 14;
	case DDP_COMPONENT_OVL2_EXDMA5:
		return 15;
	case DDP_COMPONENT_OVL2_EXDMA6:
		return 16;
	case DDP_COMPONENT_OVL2_EXDMA7:
		return 17;
	case DDP_COMPONENT_OVL_EXDMA0:
	case DDP_COMPONENT_OVL1_EXDMA0:
	case DDP_COMPONENT_OVL2_EXDMA0:
		return 18; // no use
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

static unsigned int mtk_ovl_phy_channel_mapping_MT6993(struct mtk_ddp_comp *comp)
{
	/* channel0: sub_comm2: exdma3(1) +   exdma6(4) + 1_exdma5(9) +  2_exdma2(12)
	 * channel1: sub_comm3: exdma4(2) + 1_exdma3(7) + 1_exdma6(10) + 2_exdma5(15)
	 * channel3: sub_comm1: exdma5(3) + 1_exdma2(6) + 1_exdma7(11) + 2_exdma4(14)
	 * channel2: sub_comm0: exdma2(0) +   exdma7(5) + 1_exdma4(8) +  2_exdma3(13)
	 */
	switch (comp->id) {
	case DDP_COMPONENT_OVL_EXDMA3:
	case DDP_COMPONENT_OVL_EXDMA6:
	case DDP_COMPONENT_OVL1_EXDMA5:
	case DDP_COMPONENT_OVL2_EXDMA2:
		return 0;
	case DDP_COMPONENT_OVL_EXDMA4:
	case DDP_COMPONENT_OVL1_EXDMA3:
	case DDP_COMPONENT_OVL1_EXDMA6:
	case DDP_COMPONENT_OVL2_EXDMA5:
		return 1;
	case DDP_COMPONENT_OVL_EXDMA5:
	case DDP_COMPONENT_OVL1_EXDMA2:
	case DDP_COMPONENT_OVL1_EXDMA7:
	case DDP_COMPONENT_OVL2_EXDMA4:
		return 2;
	case DDP_COMPONENT_OVL_EXDMA2:
	case DDP_COMPONENT_OVL_EXDMA7:
	case DDP_COMPONENT_OVL1_EXDMA4:
	case DDP_COMPONENT_OVL2_EXDMA3:
		return 3;
	case DDP_COMPONENT_OVL_EXDMA0:
	case DDP_COMPONENT_OVL1_EXDMA0:
	case DDP_COMPONENT_OVL2_EXDMA6:
	case DDP_COMPONENT_OVL2_EXDMA7:
		return 4; // no use
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 4;
	}
}


static void mtk_ovl_update_hrt_usage(struct mtk_drm_crtc *mtk_crtc,
			struct mtk_ddp_comp *comp, struct mtk_plane_state *plane_state)
{
	struct mtk_disp_ovl_exdma *ovl = comp_to_ovl_exdma(comp);
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	struct drm_plane_state *state = &plane_state->base;
	struct drm_framebuffer *fb = state->fb;
	unsigned int fmt = 0;
	unsigned int phy_id = 0;
	int ovl_fmt, ovl_compr;

	//Don't use Pending.format here. At this time, Pending is still the previous old information.
	if (IS_ERR_OR_NULL(fb) || IS_ERR_OR_NULL(fb->format))
		return;

	fmt = fb->format->format;
	if (!ovl->data->ovl_phy_mapping)
		return;

	phy_id = ovl->data->ovl_phy_mapping(comp);
	ovl_fmt = mtk_get_format_bpp(fmt);
	ovl_compr = plane_state->prop_val[PLANE_PROP_COMPRESS];
	if (ovl_fmt > mtk_crtc->usage_ovl_fmt[phy_id])
		mtk_crtc->usage_ovl_fmt[phy_id] = ovl_fmt;
	if (ovl_compr > mtk_crtc->usage_ovl_compr[phy_id])
		mtk_crtc->usage_ovl_compr[phy_id] = ovl_compr;

	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_OVL_EXT_LAYER))
		return;

	//If ext layer not enabled, it is necessary to determine non-overlapping or not.
	//If non-overlapping in same channel we will calculate 1 layer bw avoid vcore increase.
	mtk_crtc->usage_ovl_roi[phy_id].x = state->dst.x1;
	mtk_crtc->usage_ovl_roi[phy_id].y = state->dst.y1;
	mtk_crtc->usage_ovl_roi[phy_id].width =	drm_rect_width(&state->dst);
	mtk_crtc->usage_ovl_roi[phy_id].height = drm_rect_height(&state->dst);
}

int mtk_ovl_exdma_aid_bit(struct mtk_ddp_comp *comp, bool is_ext, int id)
{
	if (is_ext)
		return mtk_ovl_exdma_layer_num(comp) + id;
	else
		return id;
}

static int mtk_ovl_exdma_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd io_cmd, void *params);

static void mtk_ovl_exdma_all_layer_off(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int keep_first_layer)
{
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;
	if (!comp) {
		DDPPR_ERR("%s comp is null\n", __func__);
		return;
	}

	int i = 0;

	DDPINFO("%s+ %s pkt: %p\n", __func__, mtk_dump_comp_str(comp), handle);
	if (keep_first_layer) {
		if (comp->id == DDP_COMPONENT_OVL_EXDMA2 || comp->id == DDP_COMPONENT_OVL_EXDMA3) {
			DDPINFO("%s+ %s not off\n", __func__, mtk_dump_comp_str(comp));
			return;
		}
	}

	if (comp && comp->bind_comp && comp->bind_comp->funcs && comp->bind_comp->funcs->stop) {
		DDPINFO("%s+ %s bind stop\n", __func__, mtk_dump_comp_str(comp));
		comp->bind_comp->funcs->layer_off(comp->bind_comp, 0, 0, handle);
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_RDMA0_CTRL], 0,
			~0);

	for (i = 0; i < OVL_PHY_LAYER_NR-1; i++)
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + OVL_EXDMA_ELX_EN(exdma, i), 0, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_L0_EN], 0, ~0);
}

static void mtk_ovl_exdma_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	unsigned int val;
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;
	const u32 *reg_fld = exdma->data->reg_fld;
	const struct exdma_compress_info *compr_info = exdma->data->compr_info;
	unsigned int value = 0, mask = 0;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *priv;
	struct mtk_crtc_state *crtc_state = NULL;

	mtk_crtc = comp->mtk_crtc;
	crtc = &mtk_crtc->base;
	priv = crtc->dev->dev_private;
	crtc_state = to_mtk_crtc_state(crtc->state);

	DDPDBG("%s+ %s\n", __func__, mtk_dump_comp_str(comp));

	mtk_ovl_exdma_io_cmd(comp, handle, IRQ_LEVEL_NORMAL, NULL);

	cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + regs[OVL_EXDMA_RST], BIT(0), BIT(0));
	cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + regs[OVL_EXDMA_RST], 0x0, BIT(0));
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_INTSTA], 0, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_EN],
		       ENABLE_OVL_EN, REG_FLD_MASK(reg_fld[FLD_OVL_EN]));

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_EN],
		       ENABLE_FORCE_RELAY_MODE, REG_FLD_MASK(reg_fld[FLD_FORCE_RELAY_MODE]));

	/* In 6779 we need to set DISP_OVL_FORCE_RELAY_MODE */
	if (compr_info && strncmp(compr_info->name, "PVRIC_V3_1", 10) == 0) {
		val = FBDC_8XE_MODE | FBDC_FILTER_EN;
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_FBDC_CFG1], val, val);
	}

	/*
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_EN,
		       DISP_OVL_FORCE_RELAY_MODE, DISP_OVL_FORCE_RELAY_MODE);
	*/

	SET_VAL_MASK(value, mask, 1, reg_fld[FLD_BURST16_EN]);
	if (drm_crtc_index(crtc) == 2)
		SET_VAL_MASK(value, mask, 0, reg_fld[FLD_DDR_EN]);
	else if (drm_crtc_index(crtc) == 1)
		SET_VAL_MASK(value, mask, 0, reg_fld[FLD_DDR_EN]);
	else if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		SET_VAL_MASK(value, mask, 0, reg_fld[FLD_DDR_EN]);
	else
		SET_VAL_MASK(value, mask, 1, reg_fld[FLD_DDR_EN]);
	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		SET_VAL_MASK(value, mask, 0, reg_fld[FLD_DDR_ACK_EN]);
	else
		SET_VAL_MASK(value, mask, 1, reg_fld[FLD_DDR_ACK_EN]);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + regs[OVL_EXDMA_RDMA_BURST_CON1],
		       value, mask);

	value = 0x0 | DISP_OVL_EXT_DDR_EN_OPT | DISP_OVL_FORCE_EXT_DDR_EN;
	mask = ~0;
	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa + regs[OVL_EXDMA_DUMMY_REG],
			   value, mask);
	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, 1, reg_fld[FLD_LAYER_SMI_ID_EN]);
	SET_VAL_MASK(value, mask, 1, reg_fld[FLD_HDR_GCLAST_EN]);
	SET_VAL_MASK(value, mask, 1, reg_fld[FLD_GCLAST_EN]);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + regs[OVL_EXDMA_DATAPATH_CON],
		value, mask);

	if (priv->data->ovl_exdma_rule
		&& comp->id == mtk_addon_path_get_cmp(crtc, 0, ONE_SCALING, MTK_OVL_EXDMA)) {
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa
			+ regs[OVL_EXDMA_DATAPATH_CON],
			OVL_EXDMA_OUTPUT_CLAMP, REG_FLD_MASK(reg_fld[FLD_OUTPUT_CLAMP]));
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
			regs[OVL_EXDMA_MOUT], 0x1, 0x3);
	} else
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
			regs[OVL_EXDMA_MOUT], 0x2, 0x3);

	/* Enable feedback real BW consumed from OVL */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + regs[OVL_EXDMA_GDRDY_PRD],
		0xFFFFFFFF, 0xFFFFFFFF);

	DDPDBG("%s-\n", __func__);
}

static void mtk_ovl_exdma_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;
	const u32 *reg_fld = exdma->data->reg_fld;

	DDPDBG("%s+ %s\n", __func__, mtk_dump_comp_str(comp));

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_INTEN], 0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
			regs[OVL_EXDMA_EN],	0x0,
			REG_FLD_MASK(reg_fld[FLD_OVL_EN]));

	mtk_ovl_exdma_all_layer_off(comp, handle, 0);

	comp->qos_bw = 0;
	comp->qos_bw_other = 0;
	comp->fbdc_bw = 0;
	comp->hrt_bw = 0;
	comp->hrt_bw_other = 0;
	DDPDBG("%s-\n", __func__);
}

static void mtk_ovl_exdma_reset(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;

	DDPDBG("%s+ %s\n", __func__, mtk_dump_comp_str(comp));
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_RST], BIT(0), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_RST], 0, ~0);
	DDPDBG("%s-\n", __func__);
}

static void _store_bg_roi(struct mtk_ddp_comp *comp, int h, int w)
{
	struct mtk_disp_ovl_exdma *ovl = comp_to_ovl_exdma(comp);

	ovl->bg_h = h;
	ovl->bg_w = w;
}

static void _get_bg_roi(struct mtk_ddp_comp *comp, int *h, int *w)
{
	struct mtk_disp_ovl_exdma *ovl = comp_to_ovl_exdma(comp);

	if (ovl->set_partial_update != MTK_PARTIAL_UPDATE_SISO)
		*h = ovl->bg_h;
	else
		*h = ovl->roi_height;

	*w = ovl->bg_w;
}

static int mtk_ovl_exdma_golden_setting(struct mtk_ddp_comp *comp,
				  bool cfg_dc,
				  struct cmdq_pkt *handle);

static void mtk_ovl_exdma_config(struct mtk_ddp_comp *comp,
			   struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	unsigned int width, height;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned long crtc_idx = (unsigned long)drm_crtc_index(crtc);
	int fps;
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;
	const u32 *reg_fld = exdma->data->reg_fld;

	DDPINFO("exdma_config:%s\n", mtk_dump_comp_str(comp));

	if (comp->mtk_crtc->is_dual_pipe) {
		if (cfg->tile_overhead.is_support) {
			if (exdma->data->is_right_ovl_comp && exdma->data->is_right_ovl_comp(comp))
				width = cfg->tile_overhead.right_in_width;
			else
				width = cfg->tile_overhead.left_in_width;
		} else
			width = cfg->w / 2;
		if (drm_crtc_index(crtc) == 2 && (width % 2)) {
			if (exdma->data->is_right_ovl_comp && exdma->data->is_right_ovl_comp(comp))
				width += 1;
			else
				width -= 1;
		}
	} else
		width = cfg->w;

	if (exdma->set_partial_update != MTK_PARTIAL_UPDATE_SISO)
		height = cfg->h;
	else
		height = exdma->roi_height;

	if (cfg->w != 0 && cfg->h != 0) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + regs[OVL_EXDMA_ROI_SIZE],
				   height << 16 | width, ~0);

		_store_bg_roi(comp, height, width);
	}
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_EN],
		       ENABLE_OVL_EN, REG_FLD_MASK(reg_fld[FLD_OVL_EN]));
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_EN],
		ENABLE_FORCE_RELAY_MODE, REG_FLD_MASK(reg_fld[FLD_FORCE_RELAY_MODE]));
#ifdef IF_ZERO
	//enable sram dbg reg:0x900~0x934
	cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + regs[OVL_EXDMA_TRIG], 0x1000, 0x1000);

	mtk_ddp_write(comp, (height * 9) / 10,
		regs[OVL_EXDMA_ROI_TIMING_0], handle);

	DDPINFO("%s -> %u\n", __func__, (height * 9) / 10);
#endif
	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_OVL_BW_MONITOR) &&
		(crtc_idx == 0)) {
		unsigned int bw_monitor_config, line_time, h;

		/****************************************************************/
		/*BURST_ACC_FBDC: 1/0:fbdc size/actual BW(fbdc+sBCH)            */
		/*BURST_ACC_EN: 1: enable bw monitor 0: disable                 */
		/*BURST_ACC_WIN_SIZE:200us / (4AFBC line times(us) /1.2(Vblank))*/
		/****************************************************************/
		bw_monitor_config = REG_FLD_VAL(reg_fld[FLD_BURST_ACC_EN], 1);
		bw_monitor_config |= REG_FLD_VAL(reg_fld[FLD_BURST_ACC_FBDC], 0);

		if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params &&
			mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps != 0)
			fps = mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps;
		else
			fps = drm_mode_vrefresh(&crtc->state->adjusted_mode);
		h = crtc->state->adjusted_mode.vdisplay;
		line_time = 1000000 * 4 * 10 / fps / h / 12;

		ovl_win_size = (200 % line_time) ? (200 / line_time + 1) : (200 / line_time);
		bw_monitor_config |= REG_FLD_VAL(reg_fld[FLD_BURST_ACC_WIN_SIZE], ovl_win_size - 1);

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_BURST_MON_CFG], bw_monitor_config, ~0);
	}

	mtk_ovl_exdma_golden_setting(comp, cfg->p_golden_setting_context->is_dc, handle);
}

static void mtk_ovl_exdma_layer_on(struct mtk_ddp_comp *comp, unsigned int idx,
			     unsigned int ext_idx, struct cmdq_pkt *handle)
{
	unsigned int con;
	struct mtk_disp_ovl_exdma *exdma = (comp) ? comp_to_ovl_exdma(comp) : NULL;
	const u16 *regs = (exdma) ? exdma->data->regs : NULL;
	const u32 *reg_fld = (exdma) ? exdma->data->reg_fld : NULL;

	if (!comp)
		return;

	comp->layer_idx_bit |= (1 << ext_idx);

	DDPDBG("%s %s pkt:%p idx:%d, ext_idx:%d layer_idx_bit:0x%08x\n", __func__,
		mtk_dump_comp_str(comp), handle, idx, ext_idx, comp->layer_idx_bit);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_EN],
		       ENABLE_OVL_EN, REG_FLD_MASK(reg_fld[FLD_OVL_EN]));
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_EN],
		ENABLE_FORCE_RELAY_MODE, REG_FLD_MASK(reg_fld[FLD_FORCE_RELAY_MODE]));

	if (ext_idx != LYE_NORMAL) {
		unsigned int con_mask;

		con_mask = 0xFFFF << ((ext_idx - 1) * 4 + 16);
		con = idx << ((ext_idx - 1) * 4 + 16);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_EXDMA_DATAPATH_EXT_CON],
			       con, con_mask);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + OVL_EXDMA_ELX_EN(exdma, ext_idx - 1),
			       ENABLE_OVL_L_EN, REG_FLD_MASK(reg_fld[FLD_L0_EN]));
	} else
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_L0_EN], ENABLE_OVL_L_EN,
			REG_FLD_MASK(reg_fld[FLD_L0_EN]));

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_RDMA0_CTRL], 0x1, ~0);
}

static void mtk_ovl_exdma_stash_off(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	 unsigned int idx, unsigned int ext_idx)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_drm_private *priv;
	unsigned int te_duration = 0;
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;
	const u32 *reg_fld = exdma->data->reg_fld;

	priv = mtk_crtc->base.dev->dev_private;

	if (!(mtk_crtc->crtc_caps.crtc_ability & ABILITY_STASH_CMD))
		return;

	if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params)
		te_duration = mtk_crtc->panel_ext->params->real_te_duration;

	if (priv->data->mmsys_id == MMSYS_MT6991 &&
		priv->sw_ver == A0_CHIP && (te_duration && te_duration <= 2778))
		return;


	if (ext_idx != LYE_NORMAL) { //ext layer
		unsigned int id = ext_idx - 1;

		switch (id) {
		case 0:
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_STASH_CFG0],
				0, (REG_FLD_MASK(reg_fld[FLD_EL0_STASH_EN])));
			break;
		case 1:
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_STASH_CFG0],
				0, (REG_FLD_MASK(reg_fld[FLD_EL1_STASH_EN])));
			break;
		case 2:
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_STASH_CFG0],
				0, (REG_FLD_MASK(reg_fld[FLD_EL2_STASH_EN])));
			break;
		default:
			DDPPR_ERR("%s id %d\n", __func__, id);
			break;
		}
	} else { //phy layer
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_STASH_CFG0],
			0, (REG_FLD_MASK(reg_fld[FLD_L0_STASH_EN])));
	}
}

static void mtk_ovl_exdma_layer_off(struct mtk_ddp_comp *comp, unsigned int idx,
			      unsigned int ext_idx, struct cmdq_pkt *handle)
{
	u32 wcg_mask = 0, wcg_value = 0, sel_value = 0, sel_mask = 0;
	struct mtk_disp_ovl_exdma *exdma = (comp) ? comp_to_ovl_exdma(comp) : NULL;
	const u16 *regs = (exdma) ? exdma->data->regs : NULL;
	const u32 *reg_fld = (exdma) ? exdma->data->reg_fld : NULL;

	if (!comp)
		return;

	comp->layer_idx_bit = comp->layer_idx_bit & (~(0x0 | 1 << ext_idx));

	DDPDBG("%s, %s pkt:%p idx:%d, ext_idx:%d layer_idx_bit:0x%08x\n", __func__,
		mtk_dump_comp_str(comp), handle, idx, ext_idx, comp->layer_idx_bit);

		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + regs[OVL_EXDMA_RST], BIT(0), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + regs[OVL_EXDMA_RST], 0, ~0);

	if (ext_idx != LYE_NORMAL) {
		SET_VAL_MASK(wcg_value, wcg_mask, 0,
			     FLD_ELn_IGAMMA_EN(ext_idx - 1));
		SET_VAL_MASK(wcg_value, wcg_mask, 0,
			     FLD_ELn_GAMMA_EN(ext_idx - 1));
		SET_VAL_MASK(wcg_value, wcg_mask, 0,
			     FLD_ELn_CSC_EN(ext_idx - 1));
		SET_VAL_MASK(sel_value, sel_mask, 0,
			     FLD_ELn_IGAMMA_SEL(ext_idx - 1));
		SET_VAL_MASK(sel_value, sel_mask, 0,
			     FLD_ELn_GAMMA_SEL(ext_idx - 1));
	} else {
		SET_VAL_MASK(wcg_value, wcg_mask, 0, reg_fld[FLD_L0_IGAMMA_EN]);
		SET_VAL_MASK(wcg_value, wcg_mask, 0, reg_fld[FLD_L0_GAMMA_EN]);
		SET_VAL_MASK(wcg_value, wcg_mask, 0, reg_fld[FLD_L0_CSC_EN]);
		SET_VAL_MASK(sel_value, sel_mask, 0, reg_fld[FLD_L0_IGAMMA_SEL]);
		SET_VAL_MASK(sel_value, sel_mask, 0, reg_fld[FLD_L0_GAMMA_SEL]);
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + regs[OVL_EXDMA_WCG_CFG1], wcg_value,
		       wcg_mask);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + regs[OVL_EXDMA_WCG_CFG2], sel_value,
		       sel_mask);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + regs[OVL_EXDMA_WCG_CFG1], wcg_value,
		       wcg_mask);

	mtk_ovl_exdma_stash_off(comp, handle, idx, ext_idx);

	if (ext_idx != LYE_NORMAL) {
		unsigned int id = ext_idx - 1;
		DDPINFO("REMOVE EXT ext_idx %d comp->bind_comp %d\n",ext_idx, comp->bind_comp->id);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + OVL_EXDMA_ELX_EN(exdma, ext_idx-1), 0,
				REG_FLD_MASK(reg_fld[FLD_L0_EN]) |
				REG_FLD_MASK(reg_fld[FLD_L0_FBDC_EN]));
		cmdq_pkt_write(handle, comp->cmdq_base,	comp->regs_pa +
				OVL_EXDMA_ELX_ADDR(exdma, id), 0x0, ~0);

		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
				OVL_EXDMA_ELX_ADDR_MSB(exdma, id), 0x0, 0xf);
	} else {
		/*cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_DATAPATH_CON, 0,
			       BIT(0));*/

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_L0_EN], 0,
			REG_FLD_MASK(reg_fld[FLD_L0_EN]) |
			REG_FLD_MASK(reg_fld[FLD_L0_FBDC_EN]));

		cmdq_pkt_write(handle, comp->cmdq_base,	comp->regs_pa +
			regs[OVL_EXDMA_L0_HDR_ADDR], 0x0, ~0);

		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
			regs[OVL_EXDMA_L0_ADDR_MSB], 0x0, 0xf);

	}

#if !IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	if (comp->bind_comp && !comp->layer_idx_bit) {
		mtk_drm_crtc_exdma_ovl_path(comp->mtk_crtc, comp, comp->bind_comp->id, handle, true, false);
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_EN],
		       0x0, REG_FLD_MASK(reg_fld[FLD_OVL_EN]));
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_EN],
			0x0, REG_FLD_MASK(reg_fld[FLD_FORCE_RELAY_MODE]));
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_EXDMA_DATAPATH_CON], 0,
			       BIT(0));
		cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa + regs[OVL_EXDMA_RDMA0_CTRL], 0,
			   ~0);
	}
#endif

	if (comp && comp->bind_comp && comp->bind_comp->funcs
		&& comp->bind_comp->funcs->layer_off && !comp->bind_comp->blank_mode)
		comp->bind_comp->funcs->layer_off(comp->bind_comp, idx, ext_idx, handle);

	/*if (comp->id == DDP_COMPONENT_OVL_EXDMA2)
		comp->bind_comp	= NULL;*/
}

static unsigned int ovl_fmt_convert(struct mtk_disp_ovl_exdma *ovl, unsigned int fmt,
				    uint64_t modifier, unsigned int compress)
{
	switch (fmt) {
	default:
	case DRM_FORMAT_RGB565:
		return OVL_CON_CLRFMT_RGB565(ovl) | (compress ? OVL_EXDMA_CON_BYTE_SWAP : 0UL);
	case DRM_FORMAT_BGR565:
		return (unsigned int)OVL_CON_CLRFMT_RGB565(ovl) |
		       OVL_EXDMA_CON_BYTE_SWAP;
	case DRM_FORMAT_RGB888:
		return OVL_CON_CLRFMT_RGB888(ovl);
	case DRM_FORMAT_BGR888:
		return (unsigned int)OVL_CON_CLRFMT_RGB888(ovl) |
		       OVL_EXDMA_CON_BYTE_SWAP;
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA8888:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_EXDMA_CON_CLRFMT_MAN |
				OVL_EXDMA_CON_BYTE_SWAP | OVL_EXDMA_CON_RGB_SWAP;
		else
			return OVL_CON_CLRFMT_ARGB8888;
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_BGRA8888:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_EXDMA_CON_BYTE_SWAP |
			       OVL_EXDMA_CON_CLRFMT_MAN;
		else
			return OVL_CON_CLRFMT_ARGB8888 | OVL_EXDMA_CON_BYTE_SWAP;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_EXDMA_CON_CLRFMT_MAN;
		else
			return OVL_CON_CLRFMT_RGBA8888;
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_Y410:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_EXDMA_CON_CLRFMT_MAN |
			       OVL_EXDMA_CON_RGB_SWAP;
		else
			return OVL_CON_CLRFMT_RGBA8888 | OVL_EXDMA_CON_BYTE_SWAP;
	case DRM_FORMAT_UYVY:
		return OVL_CON_CLRFMT_UYVY(ovl);
	case DRM_FORMAT_YUYV:
		return OVL_CON_CLRFMT_YUYV(ovl);
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_XBGR2101010:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_EXDMA_CON_CLRFMT_MAN |
			       OVL_EXDMA_CON_RGB_SWAP;
		return OVL_CON_CLRFMT_RGBA8888 | OVL_EXDMA_CON_BYTE_SWAP;
	case DRM_FORMAT_ABGR16161616F:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_EXDMA_CON_CLRFMT_MAN |
			       OVL_EXDMA_CON_RGB_SWAP;
		return OVL_CON_CLRFMT_RGBA8888 | OVL_EXDMA_CON_BYTE_SWAP;
	case DRM_FORMAT_C8:
		return OVL_CON_CLRFMT_DIM | OVL_CON_CLRFMT_RGB888(ovl);
	}
}

static const char *mtk_ovl_get_transfer_str(enum mtk_ovl_transfer transfer)
{
	if (transfer < 0) {
		DDPPR_ERR("%s: Invalid ovl transfer:%d\n", __func__, transfer);
		transfer = 0;
	}

	return mtk_ovl_transfer_str[transfer];
}

static const char *
mtk_ovl_get_colorspace_str(enum mtk_ovl_colorspace colorspace)
{
	return mtk_ovl_colorspace_str[colorspace];
}

static enum mtk_ovl_colorspace mtk_ovl_map_cs(enum mtk_drm_dataspace ds)
{
	enum mtk_ovl_colorspace cs = OVL_SRGB;

	switch (ds & MTK_DRM_DATASPACE_STANDARD_MASK) {
	case MTK_DRM_DATASPACE_STANDARD_DCI_P3:
		cs = OVL_P3;
		break;
	case MTK_DRM_DATASPACE_STANDARD_BT2020:
		fallthrough;
	case MTK_DRM_DATASPACE_STANDARD_BT2020_CONSTANT_LUMINANCE:
		cs = OVL_BT2020;
		break;
	case MTK_DRM_DATASPACE_STANDARD_ADOBE_RGB:
		DDPINFO("%s: ovl get cs ADOBE_RGB\n", __func__);
		fallthrough;
	default:
		cs = OVL_SRGB;
		break;
	}

	return cs;
}

static enum mtk_ovl_transfer mtk_ovl_map_transfer(enum mtk_drm_dataspace ds)
{
	enum mtk_ovl_transfer xfr = OVL_GAMMA_UNKNOWN;

	switch (ds & MTK_DRM_DATASPACE_TRANSFER_MASK) {
	case MTK_DRM_DATASPACE_TRANSFER_LINEAR:
		xfr = OVL_LINEAR;
		break;
	case MTK_DRM_DATASPACE_TRANSFER_SMPTE_170M:
		xfr = OVL_GAMMA2;
		break;
	case MTK_DRM_DATASPACE_TRANSFER_GAMMA2_6:
		fallthrough;
	case MTK_DRM_DATASPACE_TRANSFER_GAMMA2_8:
		DDPINFO("%s: ovl does not support gamma 2.6/2.8, use gamma 2.2\n", __func__);
		fallthrough;
	case MTK_DRM_DATASPACE_TRANSFER_ST2084:
		fallthrough;
	case MTK_DRM_DATASPACE_TRANSFER_HLG:
		DDPINFO("%s: HDR transfer\n", __func__);
		fallthrough;
	default:
		xfr = OVL_GAMMA2_2;
		break;
	}

	return xfr;
}

static int mtk_ovl_do_transfer(unsigned int idx,
			       enum mtk_drm_dataspace plane_ds,
			       enum mtk_drm_dataspace lcm_ds, bool *gamma_en,
			       bool *igamma_en, u32 *gamma_sel, u32 *igamma_sel)
{
	enum mtk_ovl_transfer xfr_in = OVL_GAMMA2_2, xfr_out = OVL_GAMMA2_2;
	enum mtk_ovl_colorspace cs_in = OVL_CS_UNKNOWN, cs_out = OVL_CS_UNKNOWN;
	bool en = false;

	xfr_in = mtk_ovl_map_transfer(plane_ds);
	xfr_out = mtk_ovl_map_transfer(lcm_ds);
	cs_in = mtk_ovl_map_cs(plane_ds);
	cs_out = mtk_ovl_map_cs(lcm_ds);

	DDPDBG("%s+ idx:%d transfer:%s->%s\n", __func__, idx,
	       mtk_ovl_get_transfer_str(xfr_in),
	       mtk_ovl_get_transfer_str(xfr_out));

	en = (xfr_in != OVL_LINEAR);
	if (en) {
		*igamma_en = true;
		*igamma_sel = xfr_in;
	} else
		*igamma_en = false;

	en = xfr_out != OVL_LINEAR;
	if (en) {
		*gamma_en = true;
		*gamma_sel = xfr_out;
	} else
		*gamma_en = false;

	return 0;
}

static unsigned int mtk_crtc_WCG_by_color_mode(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = NULL;

	if (crtc && crtc->dev)
		priv = crtc->dev->dev_private;
	if (priv && mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_OVL_WCG_BY_COLOR_MODE))
		return 1;
	DDPDBG("WCG by color mode off\n");
	return 0;
}

static unsigned int mtk_crtc_get_color_mode(struct drm_crtc *crtc)
{
	struct mtk_crtc_state *mtk_crtc_state;
	unsigned int ret = 0;

	if (crtc == NULL)
		return ret;

	mtk_crtc_state = to_mtk_crtc_state(crtc->state);
	if (mtk_crtc_state)
		ret = mtk_crtc_state->prop_val[CRTC_PROP_WCG_BY_COLOR_MODE];

	return ret;
}

static s32 *mtk_get_ovl_csc(enum mtk_ovl_colorspace in,
			    enum mtk_ovl_colorspace out, struct drm_crtc *crtc)
{
	static s32 *ovl_csc[OVL_CS_NUM][OVL_CS_NUM];
	static unsigned int inited = 0xffffffff;
	unsigned int i, j;

	if (out < 0) {
		DDPPR_ERR("%s: Invalid ovl colorspace in:%d\n", __func__, out);
		out = 0;
	}

	if (in < 0) {
		DDPPR_ERR("%s: Invalid ovl colorspace in:%d\n", __func__, in);
		in = 0;
	}

	if (mtk_crtc_WCG_by_color_mode(crtc)) {
		/* WCG by color mode */
		unsigned int color_mode = mtk_crtc_get_color_mode(crtc);

		if (inited == color_mode)
			goto done;

		for (i = 0; i < OVL_CS_NUM; i++)
			for (j = 0; j < OVL_CS_NUM; j++)
				ovl_csc[i][j] = identity;

		switch (color_mode) {
		case HAL_COLOR_MODE_BT2020:
		case HAL_COLOR_MODE_DISPLAY_BT2020:
			DDPDBG("WCG by color mode[%d], BT2020 mode\n", color_mode);
			ovl_csc[OVL_SRGB][OVL_SRGB] = sRGB_to_BT2020;
			ovl_csc[OVL_SRGB][OVL_P3] = sRGB_to_BT2020;
			ovl_csc[OVL_SRGB][OVL_BT2020] = sRGB_to_BT2020;
			ovl_csc[OVL_P3][OVL_SRGB] = DCI_P3_to_BT2020;
			ovl_csc[OVL_P3][OVL_P3] = DCI_P3_to_BT2020;
			ovl_csc[OVL_P3][OVL_BT2020] = DCI_P3_to_BT2020;
			break;
		case HAL_COLOR_MODE_DISPLAY_P3:
		case HAL_COLOR_MODE_DCI_P3:
			DDPDBG("WCG by color mode[%d], P3 mode\n", color_mode);
			ovl_csc[OVL_SRGB][OVL_SRGB] = sRGB_to_DCI_P3;
			ovl_csc[OVL_SRGB][OVL_P3] = sRGB_to_DCI_P3;
			ovl_csc[OVL_SRGB][OVL_BT2020] = sRGB_to_DCI_P3;
			ovl_csc[OVL_BT2020][OVL_SRGB] = BT2020_to_DCI_P3;
			ovl_csc[OVL_BT2020][OVL_P3] = BT2020_to_DCI_P3;
			ovl_csc[OVL_BT2020][OVL_BT2020] = BT2020_to_DCI_P3;
			break;
		case HAL_COLOR_MODE_SRGB:
			DDPDBG("WCG by color mode[%d], SRGB mode\n", color_mode);
			ovl_csc[OVL_P3][OVL_SRGB] = DCI_P3_to_sRGB;
			ovl_csc[OVL_P3][OVL_P3] = DCI_P3_to_sRGB;
			ovl_csc[OVL_P3][OVL_BT2020] = DCI_P3_to_sRGB;
			ovl_csc[OVL_BT2020][OVL_SRGB] = BT2020_to_sRGB;
			ovl_csc[OVL_BT2020][OVL_P3] = BT2020_to_sRGB;
			ovl_csc[OVL_BT2020][OVL_BT2020] = BT2020_to_sRGB;
			break;
		case HAL_COLOR_MODE_NATIVE:
		default:
			DDPDBG("WCG by color mode[%d], NATIVE mode\n", color_mode);
			/* default: HAL_COLOR_MODE_NATIVE */
			/* csc do nothing */
			break;
		}
		inited = color_mode;
	} else {
		if (inited == 1)
			goto done;

		for (i = 0; i < OVL_CS_NUM; i++)
			for (j = 0; j < OVL_CS_NUM; j++)
				ovl_csc[i][j] = identity;

		DDPDBG("original WCG mode\n");
		ovl_csc[OVL_SRGB][OVL_P3] = sRGB_to_DCI_P3;
		ovl_csc[OVL_SRGB][OVL_BT2020] = sRGB_to_BT2020;
		ovl_csc[OVL_P3][OVL_SRGB] = DCI_P3_to_sRGB;
		ovl_csc[OVL_P3][OVL_BT2020] = DCI_P3_to_BT2020;
		ovl_csc[OVL_BT2020][OVL_SRGB] = BT2020_to_sRGB;
		ovl_csc[OVL_BT2020][OVL_P3] = BT2020_to_DCI_P3;
		inited = 1;
	}

done:
	return ovl_csc[in][out];
}

static int mtk_ovl_do_csc(unsigned int idx, enum mtk_drm_dataspace plane_ds,
			  enum mtk_drm_dataspace lcm_ds, bool *csc_wcg_en,
			  s32 **csc, struct drm_crtc *crtc)
{
	enum mtk_ovl_colorspace in = OVL_SRGB, out = OVL_SRGB;
	bool en = false;

	in = mtk_ovl_map_cs(plane_ds);
	out = mtk_ovl_map_cs(lcm_ds);

	DDPDBG("%s+ idx:%d csc:%s->%s\n", __func__, idx,
	       mtk_ovl_get_colorspace_str(in), mtk_ovl_get_colorspace_str(out));
	if (mtk_crtc_WCG_by_color_mode(crtc))
		en = 1;
	else
		en = in != out;

	if (en)
		*csc_wcg_en = true;
	else
		*csc_wcg_en = false;

	if (!en)
		return 0;
	if (!csc) {
		DDPPR_ERR("%s+ invalid csc\n", __func__);
		return 0;
	}

	*csc = mtk_get_ovl_csc(in, out, crtc);
	if (!(*csc)) {
		DDPPR_ERR("%s+ idx:%d no ovl csc %s to %s, disable csc\n",
			  __func__, idx, mtk_ovl_get_colorspace_str(in),
			  mtk_ovl_get_colorspace_str(out));
		*csc_wcg_en = false;
	}

	return 0;
}

static enum mtk_drm_dataspace
mtk_ovl_map_lcm_color_mode(enum mtk_drm_color_mode cm)
{
	enum mtk_drm_dataspace ds = MTK_DRM_DATASPACE_SRGB;

	switch (cm) {
	case HAL_COLOR_MODE_BT2100_PQ:
		ds = MTK_DRM_DATASPACE_BT2020_PQ;
		break;
	case HAL_COLOR_MODE_DISPLAY_BT2020:
		ds = MTK_DRM_DATASPACE_DISPLAY_BT2020;
		break;
	case MTK_DRM_COLOR_MODE_DISPLAY_P3:
		ds = MTK_DRM_DATASPACE_DISPLAY_P3;
		break;
	default:
		ds = MTK_DRM_DATASPACE_SRGB;
		break;
	}

	return ds;
}

/* WCG must on first                 */
/* customization condition, from HWC */
/* return default is 0 ==> WCG on    */
static unsigned int mtk_crtc_dynamic_WCG_off(struct drm_crtc *crtc)
{
	struct mtk_crtc_state *mtk_crtc_state;
	unsigned int ret = 0;

	if (crtc == NULL)
		return ret;

	mtk_crtc_state = to_mtk_crtc_state(crtc->state);
	if (mtk_crtc_state)
		ret = mtk_crtc_state->prop_val[CRTC_PROP_DYNAMIC_WCG_OFF];

	return ret;
}

/* we want WCG first ==> bri ==> CT                              */
/* for combination:                                              */
/* (CT 4x4 matrix) x (bri 4x4 matrix) x (WCG 4x4 matrix) x (RGB) */
void mtk_ovl_csc_exdma_combination(bool csc_wcg_en, unsigned int ocfbn, s32 *csc,
	struct mtk_crtc_ovl_csc_config *occ, s32 *csc_final)
{
	unsigned int i, j;
	long long csc_final_temp[16] = {0};

	/* debug log */
	if (csc) {
		for (i = 0; i < 3; i++)
			DDPDBG("WCG<%d> = <%d, %d, %d>\n", i, *(csc + i * 3 + 0),
				*(csc + i * 3 + 1), *(csc + i * 3 + 2));
	}
	for (i = 0; i < 4; i++)
		DDPDBG("brightness<%d> = <%d, %d, %d, %d>\n", i,
			occ->setbrightness[i * 4 + 0], occ->setbrightness[i * 4 + 1],
			occ->setbrightness[i * 4 + 2], occ->setbrightness[i * 4 + 3]);
	for (i = 0; i < 4; i++)
		DDPDBG("setcolortransform<%d> = <%d, %d, %d, %d>\n", i,
			occ->setcolortransform[i * 4 + 0], occ->setcolortransform[i * 4 + 1],
			occ->setcolortransform[i * 4 + 2], occ->setcolortransform[i * 4 + 3]);

	if (csc_wcg_en && csc) {
		/* WCG csc */
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				*(csc_final + i * 4 + j) = *(csc + i * 3 + j);
	} else {
		/* no WCG, set relay matrix */
		*(csc_final + 0 * 4 + 0) = 1 << ocfbn;
		*(csc_final + 1 * 4 + 1) = 1 << ocfbn;
		*(csc_final + 2 * 4 + 2) = 1 << ocfbn;
	}
	/* for offset csc_combination */
	*(csc_final + 15) = 1 << ocfbn;

	/* debug log */
	for (i = 0; i < 4; i++)
		DDPDBG("csc_final_WCG<%d> = <%d, %d, %d, %d>\n", i, csc_final[i * 4 + 0],
			csc_final[i * 4 + 1], csc_final[i * 4 + 2], csc_final[i * 4 + 3]);

	if (occ) {
		/* brightness para */
		for (i = 0; i < 4; i++)
			for (j = 0; j < 4; j++) {
				csc_final_temp[i * 4 + j] =
				(long long)(occ->setbrightness[i * 4 + 0]) *
					(long long)*(csc_final + 0 * 4 + j) +
				(long long)(occ->setbrightness[i * 4 + 1]) *
					(long long)*(csc_final + 1 * 4 + j) +
				(long long)(occ->setbrightness[i * 4 + 2]) *
					(long long)*(csc_final + 2 * 4 + j) +
				(long long)(occ->setbrightness[i * 4 + 3]) *
					(long long)*(csc_final + 3 * 4 + j);
			}
		/* copy 4x4 back */
		for (i = 0; i < 4; i++)
			for (j = 0; j < 4; j++)
				if (csc_final_temp[i * 4 + j] < 0)
					csc_final_temp[i * 4 + j] =
						(csc_final_temp[i * 4 + j] >> ocfbn) |
						(0xffffffffffffffff << (64 - ocfbn));
				else
					csc_final_temp[i * 4 + j] =
						csc_final_temp[i * 4 + j] >> ocfbn;
		for (i = 0; i < 4; i++)
			for (j = 0; j < 4; j++)
				*(csc_final + i * 4 + j) = csc_final_temp[i * 4 + j];

		/* debug log */
		for (i = 0; i < 4; i++)
			DDPDBG("csc_final_bri<%d> = <%d, %d, %d, %d>\n", i,
				csc_final[i * 4 + 0], csc_final[i * 4 + 1],
				csc_final[i * 4 + 2], csc_final[i * 4 + 3]);

		/* CT para */
		for (i = 0; i < 4; i++)
			for (j = 0; j < 4; j++) {
				csc_final_temp[i * 4 + j] =
				(long long)(occ->setcolortransform[i * 4 + 0]) *
					(long long)*(csc_final + 0 * 4 + j) +
				(long long)(occ->setcolortransform[i * 4 + 1]) *
					(long long)*(csc_final + 1 * 4 + j) +
				(long long)(occ->setcolortransform[i * 4 + 2]) *
					(long long)*(csc_final + 2 * 4 + j) +
				(long long)(occ->setcolortransform[i * 4 + 3]) *
					(long long)*(csc_final + 3 * 4 + j);
			}
		/* copy 4x4 back */
		for (i = 0; i < 4; i++)
			for (j = 0; j < 4; j++)
				if (csc_final_temp[i * 4 + j] < 0)
					csc_final_temp[i * 4 + j] =
						(csc_final_temp[i * 4 + j] >> ocfbn) |
						(0xffffffffffffffff << (64 - ocfbn));
				else
					csc_final_temp[i * 4 + j] =
						csc_final_temp[i * 4 + j] >> ocfbn;
		for (i = 0; i < 4; i++)
			for (j = 0; j < 4; j++)
				*(csc_final + i * 4 + j) = csc_final_temp[i * 4 + j];

		/* debug log */
		for (i = 0; i < 4; i++)
			DDPDBG("csc_final_CT<%d> = <%d, %d, %d, %d>\n", i,
				csc_final[i * 4 + 0], csc_final[i * 4 + 1],
				csc_final[i * 4 + 2], csc_final[i * 4 + 3]);
	}
}

void mtk_ovl_exdma_swap(int *a, int *b)
{
	int tmp;

	tmp = *a;
	*a = *b;
	*b = tmp;
}

void mtk_ovl_exdma_transpose(int *matrix, int size)
{
	int i, j;

	for (i = 0; i < size; i++)
		for (j = 0; j < i; j++)
			if (i != j)
				mtk_ovl_exdma_swap(matrix + i * size + j, matrix + j * size + i);
}

void mtk_ovl_get_ovl_exdma_csc_data(struct drm_crtc *crtc,
	struct mtk_plane_state *mtk_plane_state, struct mtk_drm_plane *mtk_plane,
	unsigned int ocfbn)
{
	int blob_id = 0;
	struct drm_property_blob *blob1 = NULL;
	struct drm_property_blob *blob2 = NULL;
	int i = 0;

	/* get brightness 4x4 by blob_id */
	blob_id = mtk_plane_state->prop_val[PLANE_PROP_OVL_CSC_SET_BRIGHTNESS];
	if (blob_id) {
		blob1 = drm_property_lookup_blob(crtc->dev, blob_id);
		if (blob1 && blob1->data && blob1->length ==
			(sizeof(int) * 16)/* 4x4 matrix */) {
			memcpy(mtk_plane->occ.setbrightness, blob1->data, blob1->length);
			mtk_ovl_exdma_transpose(mtk_plane->occ.setbrightness, 4);
			mtk_plane->csc_bc_en = 1;
		} else {
			mtk_plane->csc_bc_en = 0;
			DDPINFO("Cannot get ovl_csc_config: SET_BRIGHTNESS, blob: %d!\n",
										blob_id);
			if (blob1)
				DDPINFO("size of blob1->data is %lu\n", blob1->length);

			for (i = 0; i < 4; i++) { /* set matrix identity */
				mtk_plane->occ.setbrightness[i * 4 + i] = 1 << ocfbn;
			}
		}

		if (blob1)
			drm_property_blob_put(blob1);
	}

	/* get colortransform 4x4 by blob_id */
	blob_id = mtk_plane_state->prop_val[PLANE_PROP_OVL_CSC_SET_COLORTRANSFORM];
	if (blob_id) {
		blob2 = drm_property_lookup_blob(crtc->dev, blob_id);
		if (blob2 && blob2->data && blob2->length ==
			(sizeof(int) * 16)/* 4x4 matrix */) {
			memcpy(mtk_plane->occ.setcolortransform, blob2->data, blob2->length);
			mtk_ovl_exdma_transpose(mtk_plane->occ.setcolortransform, 4);
			mtk_plane->csc_bc_en = 1;
		} else {
			mtk_plane->csc_bc_en = 0;
			DDPINFO("Cannot get ovl_csc_config: SET_COLORTRANSFORM, blob: %d!\n",
										blob_id);
			if (blob2)
				DDPINFO("size of blob2->data is %lu\n", blob2->length);

			for (i = 0; i < 4; i++) { /* set matrix identity */
				mtk_plane->occ.setcolortransform[i * 4 + i] = 1 << ocfbn;
			}
		}
		if (blob2)
			drm_property_blob_put(blob2);
	}
}

static int mtk_ovl_color_manage(struct mtk_ddp_comp *comp, unsigned int idx,
			struct mtk_plane_state *state, struct cmdq_pkt *handle)
{
	unsigned int lye_idx = 0, ext_lye_idx = 0;
	struct mtk_plane_pending_state *pending = &state->pending;
	struct drm_crtc *crtc = state->crtc;
	struct mtk_drm_private *priv;
	bool gamma_en = false, igamma_en = false, csc_wcg_en = false;
	u32 gamma_sel = 0, igamma_sel = 0;
	s32 *csc = NULL;
	u32 wcg_mask = 0, wcg_value = 0, sel_mask = 0, sel_value = 0, reg = 0;
	enum mtk_drm_color_mode lcm_cm;
	enum mtk_drm_dataspace lcm_ds = 0, plane_ds = 0;
	struct mtk_panel_params *params;
	int i;
	int csc_bc_support = 0; /* csc setbright & setcolortransform support or not */
	unsigned int ocfbn = 0;
	s32 csc_final[16] = {0}; /* 4x4 matrix */
	static s32 csc_tmp;
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;
	const u32 *reg_fld = exdma->data->reg_fld;
	struct mtk_drm_plane *mtk_plane = NULL;

	if (state->comp_state.comp_id) {
		lye_idx = state->comp_state.lye_id;
		ext_lye_idx = state->comp_state.ext_lye_id;
	} else
		lye_idx = idx;

	if (!crtc)
		goto done;

	mtk_plane = to_mtk_plane(state->base.plane);
	if (!mtk_plane)
		goto done;

	/* init */
	if (comp && comp->mtk_crtc) /* get ocfbn */
		ocfbn = comp->mtk_crtc->crtc_caps.ovl_csc_bit_number;
	if (ocfbn > 0)
		csc_bc_support = 1;
	for (i = 0; i < 4; i++) { /* set matrix identity */
		csc_final[i * 4 + i] = 1 << ocfbn;
	}

	/* get brightness/layercolortransform csc */
	if (csc_bc_support)
		mtk_ovl_get_ovl_exdma_csc_data(crtc, state, mtk_plane, ocfbn);

	priv = crtc->dev->dev_private;
	if ((mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_OVL_WCG) &&
		(mtk_crtc_dynamic_WCG_off(crtc) == 0) &&
		pending->enable) ||	/* WCG condition */
	    mtk_plane->csc_bc_en) {	/* ovl csc condition */
		params = mtk_drm_get_lcm_ext_params(crtc);
		if (params)
			lcm_cm = params->lcm_color_mode;
		else
			lcm_cm = MTK_DRM_COLOR_MODE_NATIVE;

		lcm_ds = mtk_ovl_map_lcm_color_mode(lcm_cm);
		plane_ds =
			(enum mtk_drm_dataspace)pending->prop_val[PLANE_PROP_DATASPACE];
		DDPDBG("%s+ idx:%d ds:0x%08x->0x%08x\n", __func__, idx, plane_ds,
		       lcm_ds);

		mtk_ovl_do_transfer(idx, plane_ds, lcm_ds, &gamma_en, &igamma_en,
				    &gamma_sel, &igamma_sel);

		/* get WCG CSC */
		if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_OVL_WCG) &&
			(mtk_crtc_dynamic_WCG_off(crtc) == 0) &&
			pending->enable)
			mtk_ovl_do_csc(idx, plane_ds, lcm_ds, &csc_wcg_en, &csc, crtc);
	}
	DDPDBG("%s, g, ig, gs, igs <%d><%d><%d><%d>\n",
		__func__, gamma_en, igamma_en, gamma_sel, igamma_sel);

	/* csc combination */
	if (csc_wcg_en || mtk_plane->csc_bc_en)
		mtk_ovl_csc_exdma_combination(csc_wcg_en, ocfbn,
			csc, &mtk_plane->occ, csc_final);

done:
	if (ext_lye_idx != LYE_NORMAL) {
		SET_VAL_MASK(wcg_value, wcg_mask, igamma_en,
			     FLD_ELn_IGAMMA_EN(ext_lye_idx - 1));
		SET_VAL_MASK(wcg_value, wcg_mask, gamma_en,
			     FLD_ELn_GAMMA_EN(ext_lye_idx - 1));
		SET_VAL_MASK(wcg_value, wcg_mask, ((csc_wcg_en ||
				 (mtk_plane ? mtk_plane->csc_bc_en : 0)) ? 1 : 0),
			     FLD_ELn_CSC_EN(ext_lye_idx - 1));
		SET_VAL_MASK(sel_value, sel_mask, igamma_sel,
			     FLD_ELn_IGAMMA_SEL(ext_lye_idx - 1));
		SET_VAL_MASK(sel_value, sel_mask, gamma_sel,
			     FLD_ELn_GAMMA_SEL(ext_lye_idx - 1));
	} else {
		SET_VAL_MASK(wcg_value, wcg_mask, igamma_en,
			     reg_fld[FLD_L0_IGAMMA_EN]);
		SET_VAL_MASK(wcg_value, wcg_mask, gamma_en,
			     reg_fld[FLD_L0_GAMMA_EN]);
		SET_VAL_MASK(wcg_value, wcg_mask, ((csc_wcg_en ||
				 (mtk_plane ? mtk_plane->csc_bc_en : 0)) ? 1 : 0),
			     reg_fld[FLD_L0_CSC_EN]);
		SET_VAL_MASK(sel_value, sel_mask, igamma_sel,
			     reg_fld[FLD_L0_IGAMMA_SEL]);
		SET_VAL_MASK(sel_value, sel_mask, gamma_sel,
			     reg_fld[FLD_L0_GAMMA_SEL]);
	}

	DDPDBG("%s, lye_idx%d,ext_lye_idx%d,csc_wcg_en%d,ovl_csc_en%d,wcg_value0x%x,sel_value0x%x\n",
		__func__, lye_idx, ext_lye_idx, csc_wcg_en, mtk_plane ? mtk_plane->csc_bc_en : 0,
		wcg_value, sel_value);
	DDPDBG("%s, WCG Dymanic off = %d, WCG by color mode[%d][%d]\n", __func__,
		mtk_crtc_dynamic_WCG_off(crtc),
		mtk_crtc_WCG_by_color_mode(crtc),
		mtk_crtc_get_color_mode(crtc));

	/* enable, gamma, igamma */
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + regs[OVL_EXDMA_WCG_CFG1], wcg_value,
		       wcg_mask);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + regs[OVL_EXDMA_WCG_CFG2], sel_value,
		       sel_mask);

	if (ext_lye_idx != LYE_NORMAL)
		reg = OVL_EXDMA_ELX_R2R_PARA(exdma, ext_lye_idx - 1);
	else
		reg = regs[OVL_EXDMA_L0_R2R_PARA_R0];

	/* 3x3 write reg */
	for (i = 0; i < CSC_COEF_NUM; i++)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + reg + 4 * i, csc_final[((i / 3) * 4) + i % 3], ~0);

	/* offset write reg */
	/* offset in coda is s12.6 for u8, but value from HWC is s5.18 and normalize value */
	for (i = 0; i < 3; i++)
		if (csc_final[i * 4 + 3] < 0)
			csc_final[i * 4 + 3] = csc_final[i * 4 + 3] >> 4 | 0xf0000000;
		else
			csc_final[i * 4 + 3] = csc_final[i * 4 + 3] >> 4;

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + reg + 4 * 12, csc_final[3], ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + reg + 4 * 13, csc_final[7], ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa + reg + 4 * 14, csc_final[11], ~0);

	if (csc_tmp != csc_final[0]) {
		csc_tmp = csc_final[0];
		CRTC_MMP_MARK(0, csc_bl, csc_tmp, 0);
	}

	return 0;
}

static int mtk_ovl_yuv_matrix_convert(enum mtk_drm_dataspace plane_ds)
{
	int ret = 0;

	switch (plane_ds & MTK_DRM_DATASPACE_STANDARD_MASK) {
	case MTK_DRM_DATASPACE_STANDARD_BT601_625:
	case MTK_DRM_DATASPACE_STANDARD_BT601_625_UNADJUSTED:
	case MTK_DRM_DATASPACE_STANDARD_BT601_525:
	case MTK_DRM_DATASPACE_STANDARD_BT601_525_UNADJUSTED:
	case MTK_DRM_DATASPACE_STANDARD_DCI_P3://P3 Must align AOSP use BT601 FULL to do Y2R convert
		switch (plane_ds & MTK_DRM_DATASPACE_RANGE_MASK) {
		case MTK_DRM_DATASPACE_RANGE_UNSPECIFIED:
		case MTK_DRM_DATASPACE_RANGE_LIMITED:
			ret = OVL_CON_MTX_BT601_TO_RGB;
			break;
		default:
			ret = OVL_CON_MTX_JPEG_TO_RGB;
			break;
		}
		break;

	case MTK_DRM_DATASPACE_STANDARD_BT709:
		switch (plane_ds & MTK_DRM_DATASPACE_RANGE_MASK) {
		case MTK_DRM_DATASPACE_RANGE_UNSPECIFIED:
		case MTK_DRM_DATASPACE_RANGE_LIMITED:
			ret = OVL_CON_MTX_BT709_TO_RGB;
			break;
		default:
			ret = OVL_CON_MTX_BT709_FULL_TO_RGB;
			break;
		}
		break;
	case MTK_DRM_DATASPACE_STANDARD_BT2020:
	case MTK_DRM_DATASPACE_STANDARD_BT2020_CONSTANT_LUMINANCE:
		switch (plane_ds & MTK_DRM_DATASPACE_RANGE_MASK) {
		case MTK_DRM_DATASPACE_RANGE_UNSPECIFIED:
		case MTK_DRM_DATASPACE_RANGE_LIMITED:
			ret = OVL_CON_MTX_BT2020_TO_RGB;
			break;
		default:
			ret = OVL_CON_MTX_BT2020_FULL_TO_RGB;
			break;
		}
		break;
	case 0:
		switch (plane_ds & 0xffff) {
		case MTK_DRM_DATASPACE_JFIF:
		case MTK_DRM_DATASPACE_BT601_625:
		case MTK_DRM_DATASPACE_BT601_525:
			ret = OVL_CON_MTX_BT601_TO_RGB;
			break;

		case MTK_DRM_DATASPACE_SRGB_LINEAR:
		case MTK_DRM_DATASPACE_SRGB:
		case MTK_DRM_DATASPACE_BT709:
			ret = OVL_CON_MTX_BT709_TO_RGB;
			break;
		}
	}

	if (ret)
		return ret;

	return OVL_CON_MTX_BT601_TO_RGB;
}

static void write_phy_layer_addr_cmdq(struct mtk_ddp_comp *comp,
				      struct cmdq_pkt *handle, int id,
				      dma_addr_t addr)
{
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + regs[OVL_EXDMA_L0_ADDR],
		       addr, ~0);

	if (exdma->data->is_support_34bits)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_EXDMA_L0_ADDR_MSB],
			       (addr >> 32), 0xf);
}

static void write_ext_layer_addr_cmdq(struct mtk_ddp_comp *comp,
				      struct cmdq_pkt *handle, int id,
				      dma_addr_t addr)
{
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + OVL_EXDMA_ELX_ADDR(exdma, id),
			addr, ~0);

	if (exdma->data->is_support_34bits)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + OVL_EXDMA_ELX_ADDR_MSB(exdma, id),
			       (addr >> 32), 0xf);
}

static void write_phy_layer_hdr_addr_cmdq(struct mtk_ddp_comp *comp,
				      struct cmdq_pkt *handle, int id,
				      dma_addr_t addr)
{
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_L0_HDR_ADDR],
			addr, ~0);

	if (exdma->data->is_support_34bits)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_EXDMA_L0_ADDR_MSB],
			       ((addr >> 32) << 8), 0xf00);
}

static void write_ext_layer_hdr_addr_cmdq(struct mtk_ddp_comp *comp,
				      struct cmdq_pkt *handle, int id,
				      dma_addr_t addr)
{
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + OVL_EXDMA_ELX_HDR_ADDR(exdma, id),
			addr, ~0);

	if (exdma->data->is_support_34bits)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + OVL_EXDMA_ELX_ADDR_MSB(exdma, id),
			       (addr >> 24), 0xf00);
}

/* config addr, pitch, src_size */
static void _ovl_exdma_common_config(struct mtk_ddp_comp *comp, unsigned int idx,
			       struct mtk_plane_state *state,
			       struct cmdq_pkt *handle)
{
	struct mtk_drm_private *priv;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_plane_pending_state *pending = &state->pending;
	unsigned int fmt = pending->format;
	unsigned int pitch = pending->pitch & 0xffff;
	unsigned int pitch_msb = ((pending->pitch >> 16) & 0xf);
	unsigned int dst_h = pending->height;
	unsigned int dst_w = pending->width;
	unsigned int src_x = pending->src_x;
	unsigned int src_y = pending->src_y;
	unsigned int lye_idx = 0, ext_lye_idx = 0;
	unsigned int src_size = 0;
	unsigned int offset = 0;
	unsigned int clip = 0;
	unsigned int buf_size = 0;
	int blender_need_align = 0;
	int rotate = 0;
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;
	const u32 *reg_fld = exdma->data->reg_fld;
	unsigned int aid_sel_offset = 0;
	resource_size_t mmsys_reg = 0;
	int sec_bit;
	bool rpo_check_flag = false;
	struct mtk_crtc_state *crtc_state = NULL;
	unsigned int cmp_id = DDP_COMPONENT_ID_MAX;
	/* OVL comp might not attach to CRTC in layer_config(), need to check */
	if (unlikely(!comp->mtk_crtc)) {
		DDPPR_ERR("%s, %s has no CRTC\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	mtk_crtc = comp->mtk_crtc;
	crtc = &mtk_crtc->base;
	priv = crtc->dev->dev_private;
	crtc_state = to_mtk_crtc_state(crtc->state);

	if (fmt == DRM_FORMAT_YUYV || fmt == DRM_FORMAT_YVYU ||
	    fmt == DRM_FORMAT_UYVY || fmt == DRM_FORMAT_VYUY) {
		if (src_x % 2) {
			src_x -= 1;
			dst_w += 1;
			blender_need_align = 1;
			clip |= REG_FLD_VAL(reg_fld[FLD_LX_SRC_LEFT_CLIP], 1);
		}
		if ((src_x + dst_w) % 2) {
			dst_w += 1;
			blender_need_align = 1;
			clip |= REG_FLD_VAL(reg_fld[FLD_LX_SRC_RIGHT_CLIP], 1);
		}
	}

	src_size = (dst_h << 16) | dst_w;

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
	if (drm_crtc_index(&comp->mtk_crtc->base) == 0)
		rotate = 1;
#endif

	if (rotate)
		offset = (src_x + dst_w) * mtk_drm_format_plane_cpp(fmt, 0) +
			 (src_y + dst_h - 1) * pitch - 1;
	else
		offset = src_x * mtk_drm_format_plane_cpp(fmt, 0) + src_y * pitch;

	if (state->comp_state.comp_id) {
		lye_idx = state->comp_state.lye_id;
		ext_lye_idx = state->comp_state.ext_lye_id;
	} else
		lye_idx = idx;

	buf_size = (dst_h - 1) * pending->pitch +
		dst_w * mtk_drm_format_plane_cpp(fmt, 0);

	if (exdma->data->mmsys_mapping)
		mmsys_reg = exdma->data->mmsys_mapping(comp);

	aid_sel_offset = mtk_ovl_aid_sel(comp);

	if (state->comp_state.layer_caps & (MTK_DISP_RSZ_LAYER))
		rpo_check_flag = true;

	if (ext_lye_idx != LYE_NORMAL) {
		unsigned int id = ext_lye_idx - 1;

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + OVL_EXDMA_ELX_PITCH_MSB(exdma, id),
			pitch_msb, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + OVL_EXDMA_ELX_PITCH(exdma, id),
			pitch, ~0);

		if (mmsys_reg && aid_sel_offset) {
			sec_bit = mtk_ovl_exdma_aid_bit(comp, true, id);
			if (exdma->data->aid_per_layer_setting == true) {
				if (pending->is_sec && pending->addr) {
					cmdq_pkt_write(handle, comp->cmdq_base,
						(mmsys_reg + aid_sel_offset
						+ MT6985_OVL_LAYER_OFFEST * sec_bit),
						BIT(0), BIT(0));
				} else {
					cmdq_pkt_write(handle, comp->cmdq_base,
						(mmsys_reg + aid_sel_offset
						+ MT6985_OVL_LAYER_OFFEST * sec_bit),
						0, BIT(0));
				}
			} else {
				if (pending->is_sec && pending->addr)
					cmdq_pkt_write(handle, comp->cmdq_base,
						mmsys_reg + aid_sel_offset,
						BIT(sec_bit), BIT(sec_bit));
				else
					cmdq_pkt_write(handle, comp->cmdq_base,
						mmsys_reg + aid_sel_offset,
						0, BIT(sec_bit));
			}
		}

		write_ext_layer_addr_cmdq(comp, handle, id, pending->addr + offset);

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + OVL_EXDMA_ELX_SRC_SIZE(exdma, id),
			src_size, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + OVL_EXDMA_ELX_CLIP(exdma, id), clip,
			~0);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_L0_PITCH_MSB],
			pitch_msb, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_L0_PITCH],
			pitch, ~0);

		if (mmsys_reg && aid_sel_offset) {
			sec_bit = mtk_ovl_exdma_aid_bit(comp, false, lye_idx);
			if (exdma->data->aid_per_layer_setting == true) {
				if (pending->is_sec && pending->addr) {
					cmdq_pkt_write(handle, comp->cmdq_base,
						(mmsys_reg + aid_sel_offset
						+ MT6985_OVL_LAYER_OFFEST * sec_bit),
						BIT(0), BIT(0));
				} else {
					cmdq_pkt_write(handle, comp->cmdq_base,
						(mmsys_reg + aid_sel_offset
						+ MT6985_OVL_LAYER_OFFEST * sec_bit),
						0, BIT(0));
				}
			} else {
				if (pending->is_sec && pending->addr)
					cmdq_pkt_write(handle, comp->cmdq_base,
						mmsys_reg + aid_sel_offset,
						BIT(sec_bit), BIT(sec_bit));
				else
					cmdq_pkt_write(handle, comp->cmdq_base,
						mmsys_reg + aid_sel_offset,
						0, BIT(sec_bit));
			}
		}

		if (pending->mml_mode == MML_MODE_RACING) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + regs[OVL_EXDMA_L0_SYSRAM_CFG], 1,
				~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + regs[OVL_EXDMA_L0_BUF0_ADDR],
				pending->addr + offset, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + regs[OVL_EXDMA_L0_BUF1_ADDR],
				pending->addr + offset + MML_SRAM_SHIFT,
				~0);

			/* setting SMI for read SRAM */
			if (!exdma->data->skip_larb_con && comp->larb_cons)
				cmdq_pkt_write(handle, comp->cmdq_base, comp->larb_cons[lye_idx],
					       GENMASK(19, 16), GENMASK(19, 16));
			else
				DDPPR_ERR("%s: comp %d larb_cons is null\n", __func__, comp->id);
		}

		write_phy_layer_addr_cmdq(comp, handle, lye_idx, pending->addr + offset);

		mtk_addon_get_comp(crtc, crtc_state->lye_state.rpo_lye, &cmp_id, NULL);

		if (pending->pq_loop_type == 2) {
			if (priv->data->ovl_exdma_rule &&
				comp->id == cmp_id && cmp_id < DDP_COMPONENT_ID_MAX) {
				cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
					regs[OVL_EXDMA_L0_SRC_SIZE], src_size, ~0);
			} else {
				cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
					regs[OVL_EXDMA_L0_SRC_SIZE], pending->dst_roi, ~0);
			}
		} else
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
				regs[OVL_EXDMA_L0_SRC_SIZE], src_size, ~0);

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_L0_CLIP], clip,
			~0);

		if (comp->bind_comp) { // need_to_think
			if (pending->pq_loop_type == 2) {
				if (priv->data->ovl_exdma_rule &&
					comp->id == cmp_id && cmp_id < DDP_COMPONENT_ID_MAX) {
					if (blender_need_align == 1)
						cmdq_pkt_write(handle, comp->cmdq_base, comp->bind_comp->regs_pa +
							regs[OVL_EXDMA_L0_SRC_SIZE], pending->dst_roi + 1, ~0);
					else
						cmdq_pkt_write(handle, comp->cmdq_base, comp->bind_comp->regs_pa +
							regs[OVL_EXDMA_L0_SRC_SIZE], pending->dst_roi, ~0);
				} else
					cmdq_pkt_write(handle, comp->cmdq_base, comp->bind_comp->regs_pa +
						regs[OVL_EXDMA_L0_SRC_SIZE], pending->dst_roi, ~0);
			} else
				cmdq_pkt_write(handle, comp->cmdq_base, comp->bind_comp->regs_pa +
					regs[OVL_EXDMA_L0_SRC_SIZE], src_size, ~0);

			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->bind_comp->regs_pa + regs[OVL_EXDMA_L0_CLIP], clip,
				~0);
		}
	}
}

static void mtk_ovl_exdma_vcsel_config(struct mtk_ddp_comp *comp, unsigned int enable,
		struct cmdq_pkt *handle)
{
	unsigned int value0 = 0, mask0 = 0;//(OVL_RDMA0_L0_VCSEL | OVL_RDMA0_HDR_L0_VCSEL);
	unsigned int value1 = 0, mask1 = 0;//(OVL_RDMA0_EL_VCSEL | OVL_RDMA0_HDR_EL_VCSEL);
	unsigned int i;
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;
	const u32 *reg_fld = exdma->data->reg_fld;

	if (enable) {
		SET_VAL_MASK(value0, mask0, 1, reg_fld[FLD_OVL_RDMA0_L0_VCSEL]);
		SET_VAL_MASK(value0, mask0, 1, reg_fld[FLD_OVL_RDMA0_HDR_L0_VCSEL]);

		SET_VAL_MASK(value1, mask1, 1, reg_fld[FLD_OVL_RDMA0_ELX_VCSEL]);
		SET_VAL_MASK(value1, mask1, 1, reg_fld[FLD_OVL_RDMA0_HDR_ELX_VCSEL]);
		//value0 = (OVL_RDMA0_L0_VCSEL | OVL_RDMA0_HDR_L0_VCSEL);
		//value1 = (OVL_RDMA0_EL_VCSEL | OVL_RDMA0_HDR_EL_VCSEL);
	}

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_L0_GUSER_EXT],
			   value0, mask0);

	for (i = 0; i < 3; i++)
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
		OVL_EXDMA_ELX_GUSER_EXT(exdma, i), value1, mask1);
}

static void mtk_ovl_exdma_stash_config(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	unsigned int lye_idx, unsigned int ext_lye_idx,	struct mtk_plane_state *state)
{
	unsigned int vrefresh = 0;
	unsigned int l_time = 0, fifo_l = 0, hdr_fifo_l = 0;
	unsigned int roi_stall = 0, hdr_roi_stall = 0, gmc_stall = 0;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_ddp_comp *output_comp;
	struct drm_crtc *crtc;
	struct drm_display_mode *mode = NULL;
	struct mtk_drm_private *priv;
	unsigned int te_duration = 0;
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;
	const u32 *reg_fld = exdma->data->reg_fld;
	unsigned int value = 0, mask = 0;
	unsigned int decompr_en = 0;
	unsigned int pref_line_lead = 0;
	unsigned int hdr_pref_l_lead = 0;
	unsigned int phy_id = 0;
	int bpp = 0;
	unsigned int narrow_t = 0, bh =0;
	unsigned int l_width = 0;

	mtk_crtc = comp->mtk_crtc;
	crtc = &mtk_crtc->base;
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	priv = mtk_crtc->base.dev->dev_private;

	if (exdma->data->ovl_phy_mapping)
		phy_id = exdma->data->ovl_phy_mapping(comp);

	if (!(mtk_crtc->crtc_caps.crtc_ability & ABILITY_STASH_CMD))
		return;

	if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params)
		te_duration = mtk_crtc->panel_ext->params->real_te_duration;

	/* MT6991 && A0 chip && 360TE, not enable stash cmd */
	if (priv->data->mmsys_id == MMSYS_MT6991 &&
		priv->sw_ver == A0_CHIP && (te_duration && te_duration <= 2778))
		return;

	if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params &&
		mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps != 0)
		vrefresh =
			mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps;
	else
		vrefresh = drm_mode_vrefresh(&crtc->state->adjusted_mode);

	if (output_comp && ((output_comp->id == DDP_COMPONENT_DSI0) ||
			(output_comp->id == DDP_COMPONENT_DSI1)))
		mtk_ddp_comp_io_cmd(output_comp, NULL,
			DSI_GET_MODE_BY_MAX_VREFRESH, &mode);
	else
		mode = &crtc->state->adjusted_mode;

	if (!mode) {
		DDPPR_ERR("%s mode is null!\n", __func__);
		return;
	}

	if ((!vrefresh) || (!mode->vtotal) || (!mode->hdisplay)) {
		DDPPR_ERR("%s, vrefresh=%d, vtotal=%d, hdisplay=%d is invalid!\n",
			__func__, vrefresh, mode->vtotal, mode->hdisplay);
		return;
	}

	if (!state) {
		l_width = mode->vdisplay;
		bpp = 4;
		decompr_en = 0;
	} else {
		l_width = state->pending.width;
		bpp = mtk_get_format_bpp(state->pending.format);
		decompr_en = state->pending.prop_val[PLANE_PROP_COMPRESS];
	}

	l_time = 1000000 * 100 / vrefresh / mode->vtotal; //L

	if (!l_time) {
		DDPPR_ERR("%s, l_time=%d is invalid!\n", __func__, l_time);
		return;
	}

	if (bpp == 4)
		bh = 4;
	else if (bpp == 2)
		bh = 8;

	narrow_t = (l_width * bpp < 16) ? DIV_ROUND_UP(16, (l_width * bpp)) : 0;

	fifo_l = exdma->data->ovl_fifo_depth * 16 / bpp * l_time / mode->hdisplay; //D
	hdr_fifo_l = exdma->data->ovl_hdr_fifo_depth * 32 * bh * l_time / mode->hdisplay; //HD

	pref_line_lead =
		DIV_ROUND_UP(fifo_l, l_time) + (decompr_en * 4) + narrow_t; //(D/L)
	hdr_pref_l_lead =
		DIV_ROUND_UP(hdr_fifo_l, l_time); //(HD/L)
	roi_stall =
		DIV_ROUND_UP(((exdma->data->lead_time * 100 + fifo_l * 10) / l_time), 10)
		+ (decompr_en * 4) + narrow_t; //((X+D)/L)
	hdr_roi_stall =
		DIV_ROUND_UP(((exdma->data->lead_time * 100 + hdr_fifo_l * 10) / l_time), 10); //((X+HD)/L)
	gmc_stall =
		DIV_ROUND_UP((exdma->data->lead_time * 100 / l_time), 10); //(X/L)

	DDPDBG("%s, lye_idx=%u, ext_lye_idx=%u, l_width:%u, bpp:%d\n",
		__func__, lye_idx, ext_lye_idx, l_width, bpp);
	DDPDBG(
		"%s, bh=%d, narrow_t=%d, l_time=%d, fifo_l=%d, hdr_fifo_l=%d, hdr_roi_stall=%d, roi_stall=%d, gmc_stall=%d\n",
		__func__, bh, narrow_t, l_time, fifo_l, hdr_fifo_l,
		hdr_roi_stall, roi_stall, gmc_stall);
	DDPDBG("%s, pref_line_lead=%d, hdr_pref_l_lead=%d, decompr_en=%d, hdisplay=%d\n",
		__func__, pref_line_lead, hdr_pref_l_lead, decompr_en, mode->hdisplay);


	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_STASH_CFG1],
			   ((gmc_stall << 8) + 0), ~0);

	SET_VAL_MASK(value, mask, 1, reg_fld[FLD_L0_STASH_EN]);
	SET_VAL_MASK(value, mask, 1, reg_fld[FLD_EL0_STASH_EN]);
	SET_VAL_MASK(value, mask, 1, reg_fld[FLD_EL1_STASH_EN]);
	SET_VAL_MASK(value, mask, 1, reg_fld[FLD_EL2_STASH_EN]);
	SET_VAL_MASK(value, mask, 1, reg_fld[FLD_STASH_ROI_Y_SEL]);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_STASH_CFG0],
			   value, mask);

	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, 0, reg_fld[FLD_STASH_CACHE_NUM]);
	SET_VAL_MASK(value, mask, 1, reg_fld[FLD_STASH_ULTRA_MAN]);
	SET_VAL_MASK(value, mask, 1, reg_fld[FLD_STASH_PREULTRA]);
	SET_VAL_MASK(value, mask, 1, reg_fld[FLD_STASH_ULTRA]);
	SET_VAL_MASK(value, mask, 1, reg_fld[FLD_STASH_HDR_ULTRA_MAN]);
	SET_VAL_MASK(value, mask, 1, reg_fld[FLD_STASH_HDR_PREULTRA]);
	SET_VAL_MASK(value, mask, 1, reg_fld[FLD_STASH_HDR_ULTRA]);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_STASH_CFG2],
			   value, mask);

	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, 0, reg_fld[FLD_PREF_START_CTRL]);
	SET_VAL_MASK(value, mask, 0, reg_fld[FLD_PREF_LINE_CTRL]);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_PREF_LEAD_CFG0],
					value, mask);

	if (ext_lye_idx != LYE_NORMAL) { //ext layer
		unsigned int id = ext_lye_idx - 1;

		switch (id) {
		case 0:
			value = 0;
			mask = 0;
			SET_VAL_MASK(value, mask, roi_stall, reg_fld[FLD_EL0_STASH_ROI_LINE_STALL]);
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_STASH_STALL_CFG1],
				value, mask);
			value = 0;
			mask = 0;
			SET_VAL_MASK(value, mask, hdr_roi_stall, reg_fld[FLD_EL0_STASH_HDR_ROI_LINE_STALL]);
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_STASH_STALL_CFG3],
				value, mask);
			value = 0;
			mask = 0;
			SET_VAL_MASK(value, mask, pref_line_lead, reg_fld[FLD_EL0_PREF_LINE_LEAD]);
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_PREF_LEAD_CFG2],
				value, mask);
			value = 0;
			mask = 0;
			SET_VAL_MASK(value, mask, hdr_pref_l_lead, reg_fld[FLD_EL0_HDR_PREF_LINE_LEAD]);
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_PREF_LEAD_CFG4],
				value, mask);
			break;
		case 1:
			value = 0;
			mask = 0;
			SET_VAL_MASK(value, mask, roi_stall, reg_fld[FLD_EL1_STASH_ROI_LINE_STALL]);
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_STASH_STALL_CFG1],
				value, mask);
			value = 0;
			mask = 0;
			SET_VAL_MASK(value, mask, hdr_roi_stall, reg_fld[FLD_EL1_STASH_HDR_ROI_LINE_STALL]);
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_STASH_STALL_CFG3],
				value, mask);
			value = 0;
			mask = 0;
			SET_VAL_MASK(value, mask, pref_line_lead, reg_fld[FLD_EL1_PREF_LINE_LEAD]);
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_PREF_LEAD_CFG2],
				value, mask);
			value = 0;
			mask = 0;
			SET_VAL_MASK(value, mask, hdr_pref_l_lead, reg_fld[FLD_EL1_HDR_PREF_LINE_LEAD]);
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_PREF_LEAD_CFG4],
				   value, mask);
			break;
		case 2:
			value = 0;
			mask = 0;
			SET_VAL_MASK(value, mask, roi_stall, reg_fld[FLD_EL2_STASH_ROI_LINE_STALL]);
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_STASH_STALL_CFG1],
				value, mask);
			value = 0;
			mask = 0;
			SET_VAL_MASK(value, mask, hdr_roi_stall, reg_fld[FLD_EL2_STASH_HDR_ROI_LINE_STALL]);
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_STASH_STALL_CFG3],
				value, mask);
			value = 0;
			mask = 0;
			SET_VAL_MASK(value, mask, pref_line_lead, reg_fld[FLD_EL2_PREF_LINE_LEAD]);
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_PREF_LEAD_CFG2],
				value, mask);
			value = 0;
			mask = 0;
			SET_VAL_MASK(value, mask, hdr_pref_l_lead, reg_fld[FLD_EL2_HDR_PREF_LINE_LEAD]);
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_PREF_LEAD_CFG4],
				value, mask);
			break;
		default:
			DDPPR_ERR("%s id %d\n", __func__, id);
			break;
		}
	} else { //phy layer
		value = 0;
		mask = 0;
		SET_VAL_MASK(value, mask, roi_stall, reg_fld[FLD_L0_STASH_ROI_LINE_STALL]);
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_STASH_STALL_CFG0],
			value, mask);

		value = 0;
		mask = 0;
		SET_VAL_MASK(value, mask, hdr_roi_stall, reg_fld[FLD_L0_STASH_HDR_ROI_LINE_STALL]);
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_STASH_STALL_CFG2],
			value, mask);

		value = 0;
		mask = 0;
		SET_VAL_MASK(value, mask, pref_line_lead, reg_fld[FLD_L0_PREF_LINE_LEAD]);
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_PREF_LEAD_CFG1],
			value, mask);

		value = 0;
		mask = 0;
		SET_VAL_MASK(value, mask, hdr_pref_l_lead, reg_fld[FLD_L0_HDR_PREF_LINE_LEAD]);
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_PREF_LEAD_CFG3],
			value, mask);
	}
}

static void mtk_ovl_exdma_layer_config(struct mtk_ddp_comp *comp, unsigned int idx,
				 struct mtk_plane_state *state,
				 struct cmdq_pkt *handle)
{
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	struct mtk_plane_pending_state *pending = &state->pending;
	int rotate = 0;
	unsigned int fmt = pending->format;
	unsigned int offset;
	unsigned int Ln_CLRFMT = 0, layer_src = 0, con = 0;
	unsigned int lye_idx = 0, ext_lye_idx = 0;
	unsigned int alpha = 0;
	unsigned int alpha_con = 1;
	unsigned int fmt_ex = 0;
	unsigned long long temp_bw, temp_peak_bw;
	unsigned int dim_color;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_crtc_state *mtk_crtc_state;
	unsigned int frame_idx;
	unsigned long alloc_id = state->prop_val[PLANE_PROP_BUFFER_ALLOC_ID];
	unsigned int avg_ratio = 0;
	struct mtk_drm_private *priv;
	unsigned long crtc_idx;
	int i = 0;
	unsigned int disp_reg_ovl_pitch = 0;
	unsigned int pixel_blend_mode = DRM_MODE_BLEND_PIXEL_NONE;
	unsigned int modifier = 0;
	unsigned int blender_id = 0;
	bool rpo_check_flag = false;
	unsigned int cmp_id = DDP_COMPONENT_ID_MAX;
	const u16 *regs = exdma->data->regs;
	const u32 *reg_fld = exdma->data->reg_fld;

	if (!comp)
		return;

	/* OVL comp might not attach to CRTC in layer_config(), need to check */
	if (unlikely(!comp->mtk_crtc)) {
		DDPPR_ERR("%s, %s has no CRTC\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	mtk_crtc = comp->mtk_crtc;
	crtc = &mtk_crtc->base;
	mtk_crtc_state = to_mtk_crtc_state(crtc->state);
	frame_idx = mtk_crtc_state->prop_val[CRTC_PROP_OVL_DSI_SEQ];
	priv = crtc->dev->dev_private;
	crtc_idx = (unsigned long)drm_crtc_index(crtc);
	blender_id = state->comp_state.blender_comp_id;

	/* handle dim layer for compression flag & color dim*/
	if (fmt == DRM_FORMAT_C8) {
		pending->prop_val[PLANE_PROP_COMPRESS] = 0;
		dim_color = (unsigned int)pending->prop_val[PLANE_PROP_DIM_COLOR];
	} else {
		dim_color = 0xff000000;
	}

	/* handle buffer de-compression */
	if (exdma->data->compr_info && exdma->data->compr_info->l_config) {
		if (exdma->data->compr_info->l_config(comp,
				idx, state, handle)) {
			DDPPR_ERR("wrong fbdc input config\n");
			return;
		}
	} else {
		/* Config common register which would be different according
		 * with
		 * this layer is compressed or not, i.e.: addr, pitch...
		 */
		_ovl_exdma_common_config(comp, idx, state, handle);
	}

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
	if (drm_crtc_index(crtc) == 0)
		rotate = 1;
#endif

	if (state->comp_state.comp_id) {
		lye_idx = state->comp_state.lye_id;
		ext_lye_idx = state->comp_state.ext_lye_id;
	} else
		lye_idx = idx;

	if (fmt == DRM_FORMAT_RGB332) {
		pending->enable = false;
		DDPINFO("%s: DRM_FORMAT_RGB332 not support, so skip it\n", __func__);
	}
	if (exdma->ovl_dis == true && pending->enable == true) {
		if (mtk_crtc_is_frame_trigger_mode(crtc))
			pending->enable = false;

		DDPPR_ERR("%s, %s, idx:%d, lye_idx:%d, ext_idx:%d, en:%d\n",
			__func__, mtk_dump_comp_str_id(comp->id), idx, lye_idx,
			ext_lye_idx, pending->enable);
		exdma->ovl_dis = false;
	}

	if (!pending->enable)
		mtk_ovl_exdma_layer_off(comp, lye_idx, ext_lye_idx, handle);

	if (state->comp_state.layer_caps & (MTK_DISP_RSZ_LAYER))
		rpo_check_flag = true;

#if !IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	if (blender_id)
		mtk_drm_crtc_exdma_ovl_path(mtk_crtc, comp, blender_id, handle, false, rpo_check_flag);
	else
		mtk_drm_crtc_exdma_ovl_path(mtk_crtc, comp, comp->bind_comp->id, handle, false,
			rpo_check_flag);
#endif

	mtk_ovl_color_manage(comp, idx, state, handle);

	alpha = 0xFF & (state->base.alpha >> 8);

	DDPDBG("Blending: state->base.alpha =0x%x, alpha = 0x%x\n", state->base.alpha, alpha);
	if (state->base.fb) {
		if (state->base.fb->format->has_alpha)
			pixel_blend_mode = state->base.pixel_blend_mode;

		DDPDBG("Blending: has_alpha %d pixel_blend_mode=0x%x fmt=0x%x\n",
			state->base.fb->format->has_alpha, state->base.pixel_blend_mode, fmt);
	}

	if (pixel_blend_mode == DRM_MODE_BLEND_PREMULTI)
		modifier |= MTK_FMT_PREMULTIPLIED;

	Ln_CLRFMT = ovl_fmt_convert(exdma, fmt, modifier,
			pending->prop_val[PLANE_PROP_COMPRESS]);
	con |= (alpha_con << 8) | alpha;

	if (fmt == DRM_FORMAT_UYVY || fmt == DRM_FORMAT_YUYV ||
		pending->mml_mode == MML_MODE_DIRECT_LINK) {
		unsigned int prop = (unsigned int)pending->prop_val[PLANE_PROP_DATASPACE];

		con |= mtk_ovl_yuv_matrix_convert((enum mtk_drm_dataspace)prop);
	} else if (fmt == DRM_FORMAT_Y410) {
		DDPDBG("%s: DRM_FORMAT_Y410, dataspace set as BT601_FULL\n", __func__);
		con |= mtk_ovl_yuv_matrix_convert(MTK_DRM_DATASPACE_V0_JFIF);
	}

	mtk_addon_get_comp(crtc, mtk_crtc_state->lye_state.rpo_lye, &cmp_id, NULL);

	if ((!pending->addr && pending->pq_loop_type == 0) || fmt == DRM_FORMAT_C8)
		layer_src |= LSRC_COLOR;
	else if ((pending->pq_loop_type == 2)
		&& (!priv->data->ovl_exdma_rule ||
		comp->id != cmp_id))
		layer_src |= LSRC_PQ;

	if (rotate) {
		unsigned int bg_w = 0, bg_h = 0;

		_get_bg_roi(comp, &bg_h, &bg_w);
		offset = ((bg_h - pending->height - pending->dst_y) << 16) +
			 (bg_w - pending->width - pending->dst_x);
		DDPINFO("bg(%d,%d) (%d,%d,%dx%d)\n", bg_w, bg_h, pending->dst_x,
			pending->dst_y, pending->width, pending->height);
		con |= (CON_HORI_FLIP + CON_VERTICAL_FLIP);
	} else {
		offset = pending->offset;
	}

	if (fmt == DRM_FORMAT_ABGR2101010 || fmt == DRM_FORMAT_XBGR2101010)
		fmt_ex = 1;
	else if (fmt == DRM_FORMAT_ABGR16161616F)
		fmt_ex = 3;

	Ln_CLRFMT |= fmt_ex << 8;

	/*MML DL out is alwayer YUV*/
	if (pending->mml_mode == MML_MODE_DIRECT_LINK) {
		con |= REG_FLD_VAL(reg_fld[FLD_MTX_AUTO_DIS], 1);
		con |= REG_FLD_VAL(reg_fld[FLD_MTX_EN], 1);
	}

	if (fmt == DRM_FORMAT_Y410) {
		DDPDBG("%s: DRM_FORMAT_Y410, enable OVL Y2R\n", __func__);
		con |= REG_FLD_VAL(reg_fld[FLD_MTX_AUTO_DIS], 1);
		con |= REG_FLD_VAL(reg_fld[FLD_MTX_EN], 1);
		/* if format is DRM_FORMAT_Y410, enable Y2R inside OVL */
	}

	if (fmt == DRM_FORMAT_Y410 || fmt == DRM_FORMAT_UYVY || fmt == DRM_FORMAT_YUYV)
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_EN_CON],
			OP_8_BIT_MODE, REG_FLD_MASK(reg_fld[FLD_OP_8BIT_MODE]));
	else
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_EN_CON],
			0x0, REG_FLD_MASK(reg_fld[FLD_OP_8BIT_MODE]));

	if (ext_lye_idx != LYE_NORMAL) {
		unsigned int id = ext_lye_idx - 1;

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + OVL_EXDMA_ELX_CLRFMT(exdma, ext_lye_idx-1),
			Ln_CLRFMT,	~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + OVL_EXDMA_ELX_EN(exdma, ext_lye_idx-1), layer_src,
			REG_FLD_MASK(reg_fld[FLD_L0_LAYER_SRC]));

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + OVL_EXDMA_ELX_CON(exdma, id), con,
			~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + OVL_EXDMA_ELX_OFFSET(exdma, id),
			offset, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + OVL_EXDMA_ELX_CLR(exdma, id),
			dim_color, ~0);

		if (comp->bind_comp) { // need_to_think
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->bind_comp->regs_pa +
				OVL_EXDMA_ELX_CLRFMT(exdma, id),
				Ln_CLRFMT,	~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->bind_comp->regs_pa + OVL_EXDMA_ELX_EN(exdma, id),
				(layer_src==LSRC_PQ)?0:layer_src,
				REG_FLD_MASK(reg_fld[FLD_L0_LAYER_SRC]));
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->bind_comp->regs_pa + OVL_EXDMA_ELX_OFFSET(exdma, id), offset, ~0);
		}

		disp_reg_ovl_pitch = OVL_EXDMA_ELX_PITCH(exdma, id);

		/* ext layer is the same as attached phy layer */
		if (!IS_ERR_OR_NULL(comp->qos_req_other) &&
			priv->data->mmsys_id != MMSYS_MT6989) {
			int val = (lye_idx % 2);

			cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_EXDMA_SMI_2ND_CFG],
			       (val << (id + 4)), (1 << (id + 4)));
		}
		if (fmt == DRM_FORMAT_C8)
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + OVL_EXDMA_ELX_EN(exdma, id), 0,
				REG_FLD_MASK(reg_fld[FLD_L0_EN]));

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_UNDO_ALPHA],
			(modifier & MTK_FMT_PREMULTIPLIED) ? ~0: 0,
			REG_FLD_MASK(EL_UNDO_ALPHA(ext_lye_idx)) |
			REG_FLD_MASK(EL_APPLY_ALPHA(ext_lye_idx)));
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_L0_CLRFMT], Ln_CLRFMT,
			~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_L0_CON2], con, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_L0_EN], layer_src,
			REG_FLD_MASK(reg_fld[FLD_L0_LAYER_SRC]));
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_L0_OFFSET], offset, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_L0_CLR],
			dim_color, ~0);

		if (comp->bind_comp) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->bind_comp->regs_pa + regs[OVL_EXDMA_L0_CLRFMT], Ln_CLRFMT,
				~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->bind_comp->regs_pa + regs[OVL_EXDMA_L0_EN],
				(layer_src==LSRC_PQ)?0:layer_src,
				REG_FLD_MASK(reg_fld[FLD_L0_LAYER_SRC]));
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->bind_comp->regs_pa + regs[OVL_EXDMA_L0_OFFSET], offset, ~0);
		}

		disp_reg_ovl_pitch = regs[OVL_EXDMA_L0_PITCH];

		/*
		 * layer0 --> larb0, layer1 --> larb1
		 * layer2 --> larb0, layer3 --> larb1
		 */
		if (!IS_ERR_OR_NULL(comp->qos_req_other) &&
			priv->data->mmsys_id != MMSYS_MT6989) {
			int val = (lye_idx % 2);

			cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_EXDMA_SMI_2ND_CFG],
			       (val << lye_idx), (1 << lye_idx));
		}
		if (fmt == DRM_FORMAT_C8)
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + regs[OVL_EXDMA_L0_EN], 0,
				REG_FLD_MASK(reg_fld[FLD_L0_EN]));

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_UNDO_ALPHA],
			(modifier & MTK_FMT_PREMULTIPLIED) ? ~0 : 0,
			REG_FLD_MASK(reg_fld[FLD_L0_UNDO_ALPHA]) |
			REG_FLD_MASK(reg_fld[FLD_L0_APPLY_ALPHA]));
	}

	if (priv->data->mmsys_id == MMSYS_MT6991) {
		if (pending->enable) {//enable and not ext layer
			if (ext_lye_idx == 0)
				mtk_crtc->usage_ovl_fmt[(exdma->data->ovl_phy_mapping(comp) + lye_idx)] =
					mtk_get_format_bpp(fmt);
		} else {
			mtk_crtc->usage_ovl_fmt[(exdma->data->ovl_phy_mapping(comp) + lye_idx)] = 0;
		}
	}


#define _LAYER_CONFIG_FMT \
	"%s %s idx:%d lye_idx:%d ext_idx:%d en:%d fmt:0x%x " \
	"addr:0x%lx compr:%d con:0x%x offset:0x%x lye_cap:%x mml:%d layer_src:%d\n"
	DDPINFO(_LAYER_CONFIG_FMT, __func__,
		mtk_dump_comp_str_id(comp->id), idx, lye_idx, ext_lye_idx,
		pending->enable, pending->format, (unsigned long)pending->addr,
		(unsigned int)pending->prop_val[PLANE_PROP_COMPRESS], con, offset,
		state->comp_state.layer_caps & (MTK_DISP_RSZ_LAYER | DISP_MML_CAPS_MASK),
		pending->mml_mode, layer_src);

	DDPINFO("%s alpha= 0x%x, con=0x%x, blend = 0x%x, reg_ovl_pitch=0x%x\n",
		 __func__,
		alpha,
		alpha_con,
		pixel_blend_mode,
		disp_reg_ovl_pitch);

	if (pixel_blend_mode == DRM_MODE_BLEND_PIXEL_NONE)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + disp_reg_ovl_pitch,
			OVL_EXDMA_LX_CONST_BLD,
			REG_FLD_MASK(reg_fld[FLD_LX_CONST_BLD]));
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + disp_reg_ovl_pitch,
			0,
			REG_FLD_MASK(reg_fld[FLD_LX_CONST_BLD]));

	if (pending->enable) {
		u32 vrefresh;
		u32 ratio_tmp = 0;
		unsigned int vact = 0;
		unsigned int vtotal = 0;
		struct mtk_ddp_comp *output_comp;
		struct drm_display_mode *mode = NULL;

		output_comp = mtk_ddp_comp_request_output(mtk_crtc);

		if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params &&
			mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps != 0)
			vrefresh =
				mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps;
		else
			vrefresh = drm_mode_vrefresh(&crtc->state->adjusted_mode);

		/* query display mode anyway, would use it even in CMD mode */
		if (output_comp &&
		    ((output_comp->id == DDP_COMPONENT_DSI0) ||
		     (output_comp->id == DDP_COMPONENT_DSI1) ||
		     (output_comp->id == DDP_COMPONENT_DSI2)))
			mtk_ddp_comp_io_cmd(output_comp, NULL,
				DSI_GET_MODE_BY_MAX_VREFRESH, &mode);
		if (mode && !(mtk_dsi_is_cmd_mode(output_comp))) {
			vtotal = mode->vtotal;
			vact = mode->vdisplay;
			ratio_tmp = vtotal * 100 / vact;
		} else
			ratio_tmp = 100;

		DDPDBG("%s, vrefresh=%d, ratio_tmp=%d\n",
			__func__, vrefresh, ratio_tmp);
		DDPDBG("%s, vtotal=%d, vact=%d\n",
			__func__, vtotal, vact);

		if (fmt != DRM_FORMAT_C8)
			mtk_ovl_exdma_layer_on(comp, lye_idx, ext_lye_idx, handle);
		mtk_ovl_exdma_stash_config(comp, handle, lye_idx, ext_lye_idx, state);
		mtk_ovl_exdma_vcsel_config(comp, 1, handle);

		/*constant color :non RDMA source*/
		/* TODO: cause RPO abnormal */
		/* TODO: consider FBDC */
		/* SRT BW (one layer) =
		 * layer_w * layer_h * bpp * vrefresh * max fps blanking_ratio
		 * Sum_SRT(all layer) *= 1.33
		 */
		/* use full frame size's peak BW request bus capability, because tiny region layer */
		/* peak BW should be the same with full frame */
		temp_bw = (unsigned long long)pending->width * pending->height;
		temp_bw *= mtk_get_format_bpp(fmt);

		if (crtc->state)
			temp_peak_bw = (unsigned long long)crtc->state->adjusted_mode.vdisplay *
				crtc->state->adjusted_mode.hdisplay;
		else
			temp_peak_bw = (unsigned long long)pending->width * pending->height;
		temp_peak_bw *= mtk_get_format_bpp(fmt);

		/* COMPRESS ratio */
		if (pending->prop_val[PLANE_PROP_COMPRESS]) {
			temp_bw *= 7;
			do_div(temp_bw, 10);
		}
		do_div(temp_bw, 1000);
		temp_bw *= ratio_tmp;
		do_div(temp_bw, 100);
		temp_bw = temp_bw * vrefresh;
		do_div(temp_bw, 1000);

		do_div(temp_peak_bw, 1000);
		temp_peak_bw *= ratio_tmp;
		do_div(temp_peak_bw, 100);
		temp_peak_bw = temp_peak_bw * vrefresh;
		do_div(temp_peak_bw, 1000);

		DDPDBG("comp %d lye %u bw %llu peak %llu vtotal:%d vact:%d\n",
			comp->id, lye_idx, temp_bw, temp_peak_bw, vtotal, vact);

		if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_OVL_BW_MONITOR) &&
			(crtc_idx == 0) && (pending->prop_val[PLANE_PROP_COMPRESS]) &&
			(priv->data->mmsys_id != MMSYS_MT6989) &&
			((state->comp_state.layer_caps & MTK_HWC_UNCHANGED_LAYER) ||
			(state->comp_state.layer_caps & MTK_HWC_INACTIVE_LAYER) ||
			(state->comp_state.layer_caps & MTK_HWC_UNCHANGED_FBT_LAYER))) {
			uint64_t key = 0;
			int fbt_layer_id = -1;
			unsigned long long temp_bw_old = temp_bw;

			fbt_layer_id = mtk_crtc->fbt_layer_id;
			/* if layer is fbt layer need find fbt layer ratio to cal bw */
			if (idx == fbt_layer_id) {
				key = frame_idx - BWM_GPUC_TUNING_FRAME;
				for (i = 0; i < MAX_FRAME_RATIO_NUMBER; i++) {
					if ((key == fbt_layer_compress_ratio_tb[i].key_value) &&
						(fbt_layer_compress_ratio_tb[i].average_ratio
						!= 0) &&
						(fbt_layer_compress_ratio_tb[i].average_ratio
						<= 1000)) {
						avg_ratio =
						fbt_layer_compress_ratio_tb[i].average_ratio;
						temp_bw = temp_bw * avg_ratio;
						do_div(temp_bw, 1000);
						break;
					}
				}
			} else {
				int have_get_ratio = 0;

				key = frame_idx + alloc_id - BWM_GPUC_TUNING_FRAME;
				for (i = 0; i < MAX_LAYER_RATIO_NUMBER; i++) {
					if ((alloc_id ==
						unchanged_compress_ratio_table[i].key_value) &&
						(unchanged_compress_ratio_table[i].average_ratio
						!= 0) &&
						(unchanged_compress_ratio_table[i].average_ratio
						<= 1000)) {
						avg_ratio =
						unchanged_compress_ratio_table[i].average_ratio;
						temp_bw = temp_bw * avg_ratio;
						do_div(temp_bw, 1000);
						have_get_ratio = 1;
						break;
					}
				}
				for (i = 0; i < MAX_FRAME_RATIO_NUMBER*MAX_LAYER_RATIO_NUMBER;
					i++) {
					if (have_get_ratio)
						break;

					if ((key == normal_layer_compress_ratio_tb[i].key_value) &&
						(normal_layer_compress_ratio_tb[i].average_ratio
						!= 0) &&
						(normal_layer_compress_ratio_tb[i].average_ratio
						<= 1000)) {
						avg_ratio =
						normal_layer_compress_ratio_tb[i].average_ratio;
						temp_bw = temp_bw * avg_ratio;
						do_div(temp_bw, 1000);
						break;
					}
				}
			}

			/* Due to low ratio, bw will be 0 */
			/* We want the min value to be 1 */
			if (temp_bw <= 0)
				temp_bw = 1;

			DDPINFO("BWM:frame idx:%u alloc id:%lu key:%llu lye_idx:%u bw:%llu(%llu)\n",
					frame_idx, alloc_id, key, idx, temp_bw, temp_bw_old);
		}

		/* if source is not from memory, no need to report module hrt and srt */
		if ((layer_src & LSRC_PQ) || (layer_src & LSRC_COLOR) ||
		    (pending->mml_mode == MML_MODE_RACING)) {
			temp_bw = 0;
			temp_peak_bw = 0;
		}
		comp->qos_bw = temp_bw;
		comp->hrt_bw = temp_peak_bw;
	}

	if (comp && comp->bind_comp && comp->bind_comp->funcs
		&& comp->bind_comp->funcs->layer_config && !comp->bind_comp->blank_mode)
		comp->bind_comp->funcs->layer_config(comp->bind_comp, idx, state, handle);
}

bool compr_ovl_exdma_l_config_AFBC_V1_2(struct mtk_ddp_comp *comp,
			unsigned int idx, struct mtk_plane_state *state,
			struct cmdq_pkt *handle)
{
	struct mtk_drm_private *priv;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_crtc_state *crtc_state = NULL;
	unsigned int cmp_id = DDP_COMPONENT_ID_MAX;
	/* input config */
	struct mtk_plane_pending_state *pending = &state->pending;
	dma_addr_t addr = pending->addr;
	bool enable = pending->enable;
	unsigned int pitch = pending->pitch & 0xffff;
	unsigned int vpitch = (unsigned int)pending->prop_val[PLANE_PROP_VPITCH];
	unsigned int src_x = pending->src_x, src_y = pending->src_y;
	unsigned int src_w = pending->width, src_h = pending->height;
	unsigned int fmt = pending->format;
	unsigned int Bpp = mtk_drm_format_plane_cpp(fmt, 0);
	unsigned int lye_idx = 0, ext_lye_idx = 0;
	unsigned int compress = (unsigned int)pending->prop_val[PLANE_PROP_COMPRESS];
	int rotate = 0;

	/* variable to do calculation */
	unsigned int tile_w = AFBC_V1_2_TILE_W;
	unsigned int tile_h = AFBC_V1_2_TILE_H;
	unsigned int tile_body_size = tile_w * tile_h * Bpp;
	unsigned int dst_h = pending->height;
	unsigned int dst_w = pending->width;
	unsigned int src_x_align, src_w_align;
	unsigned int src_y_align, src_y_half_align;
	unsigned int src_y_end_align, src_y_end_half_align;
	unsigned int src_h_align, src_h_half_align = 0;
	unsigned int header_offset, tile_offset;
	dma_addr_t buf_addr;
	unsigned int src_buf_tile_num = 0;
	unsigned int buf_size = 0;
	unsigned int buf_total_size = 0;

	/* variable to config into register */
	unsigned int lx_fbdc_en;
	dma_addr_t lx_addr, lx_hdr_addr;
	unsigned int lx_pitch, lx_hdr_pitch;
	unsigned int lx_clip, lx_src_size;
	unsigned int lx_2nd_subbuf = 0;
	unsigned int lx_pitch_msb = 0;

	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;
	const u32 *reg_fld = exdma->data->reg_fld;
	unsigned int aid_sel_offset = 0;
	resource_size_t mmsys_reg = 0;
	int sec_bit;
	bool rpo_check_flag = false;
	/* OVL comp might not attach to CRTC in layer_config(), need to check */
	if (unlikely(!comp->mtk_crtc)) {
		DDPPR_ERR("%s, %s has no CRTC\n", __func__, mtk_dump_comp_str(comp));
		return false;
	}

	mtk_crtc = comp->mtk_crtc;
	crtc = &mtk_crtc->base;
	priv = crtc->dev->dev_private;
	crtc_state = to_mtk_crtc_state(crtc->state);

	DDPDBG("%s:%d, addr:0x%lx, pitch:%d, vpitch:%d\n",
		__func__, __LINE__, (unsigned long)addr,
		pitch, vpitch);
	DDPDBG("src:(%d,%d,%d,%d), fmt:%d, Bpp:%d, compress:%d\n",
		src_x, src_y,
		src_w, src_h,
		fmt, Bpp,
		compress);

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
	if (drm_crtc_index(&comp->mtk_crtc->base) == 0)
		rotate = 1;
#endif

	if (state->comp_state.comp_id) {
		lye_idx = state->comp_state.lye_id;
		ext_lye_idx = state->comp_state.ext_lye_id;
	} else
		lye_idx = idx;

	if (state->comp_state.layer_caps & (MTK_DISP_RSZ_LAYER))
		rpo_check_flag = true;

	/* 1. cal & set OVL_LX_FBDC_EN */
	lx_fbdc_en = (compress != 0);
	if (ext_lye_idx != LYE_NORMAL) {
		unsigned int id = ext_lye_idx - 1;

		cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + OVL_EXDMA_ELX_EN(exdma, id), lx_fbdc_en << 4,
		       REG_FLD_MASK(reg_fld[FLD_L0_FBDC_EN]));
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + OVL_EXDMA_ELX_SYSRAM_CFG(exdma, id), 0,
			~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + OVL_EXDMA_ELX_SYSRAM_BUF0_ADDR(exdma, id),
			0, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + OVL_EXDMA_ELX_SYSRAM_BUF1_ADDR(exdma, id),
			0, ~0);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_L0_EN],
			lx_fbdc_en << 4, REG_FLD_MASK(reg_fld[FLD_L0_FBDC_EN]));

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_L0_SYSRAM_CFG], 0,
			~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_L0_BUF0_ADDR],
			0, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_L0_BUF1_ADDR],
			0, ~0);
	}

	/* setting SMI for read DRAM */
	if (!exdma->data->skip_larb_con && comp->larb_cons)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->larb_cons[lye_idx], 0, GENMASK(19, 16));

	/* if no compress, do common config and return */
	if (compress == 0 || (pending->mml_mode == MML_MODE_RACING) ||
			     (pending->mml_mode == MML_MODE_DIRECT_LINK)) {
		_ovl_exdma_common_config(comp, idx, state, handle);
		return 0;
	}

	/* 2. pre-calculation */
	if (Bpp == 0) {
		DDPPR_ERR("%s fail, no Bpp info\n", __func__);
		return 0;
	}
	src_buf_tile_num = ALIGN_TO(pitch / Bpp, tile_w) *
	    ALIGN_TO(vpitch, tile_h);
	src_buf_tile_num /= (tile_w * tile_h);
	header_offset = ALIGN_TO(
		src_buf_tile_num * AFBC_V1_2_HEADER_SIZE_PER_TILE_BYTES,
		AFBC_V1_2_HEADER_ALIGN_BYTES);
	buf_addr = addr + header_offset;

	/* calculate for alignment */
	src_x_align = (src_x / tile_w) * tile_w;
	src_w_align = (1 + (src_x + src_w - 1) / tile_w) * tile_w - src_x_align;

	/* src_y_half_align, src_y_end_half_align,
	 * the start y offset and  stop y offset if half tile align
	 * such as 0 and 3, then the src_h_align is 4
	 */
	src_y_align = (src_y / tile_h) * tile_h;
	src_y_end_align = (1 + (src_y + src_h - 1) / tile_h) * tile_h - 1;
	src_h_align = src_y_end_align - src_y_align + 1;

	src_y_half_align = (src_y / (tile_h >> 1)) * (tile_h >> 1);
	src_y_end_half_align =
		(1 + (src_y + src_h - 1) / (tile_h >> 1)) * (tile_h >> 1) - 1;
	src_h_half_align = src_y_end_half_align - src_y_half_align + 1;

	if (rotate) {
		tile_offset = (src_x_align + src_w_align - tile_w) / tile_w +
			(pitch / tile_w / Bpp) *
			(src_y_align + src_h_align - tile_h) /
			tile_h;
		if (src_y_end_align == src_y_end_half_align)
			lx_2nd_subbuf = 1;
	} else {
		tile_offset = src_x_align / tile_w +
			(pitch / tile_w / Bpp) * src_y_align / tile_h;
		if (src_y_align != src_y_half_align)
			lx_2nd_subbuf = 1;
	}

	/* 3. cal OVL_LX_ADDR * OVL_LX_PITCH */
	lx_addr = buf_addr + (dma_addr_t) tile_offset * (dma_addr_t) tile_body_size;
	lx_pitch = ((pitch * tile_h) & 0xFFFF);
	lx_pitch_msb = (REG_FLD_VAL((reg_fld[FLD_LX_YUV_TRANS]), (1)) |
		REG_FLD_VAL((reg_fld[FLD_LX_2ND_SUBBUF]), (lx_2nd_subbuf)) |
		REG_FLD_VAL((reg_fld[FLD_LX_SRC_PITCH_MSB]),
		((pitch * tile_h) >> 16) & 0xF));

	/* 4. cal OVL_LX_HDR_ADDR, OVL_LX_HDR_PITCH */
	lx_hdr_addr = addr + tile_offset *
	    AFBC_V1_2_HEADER_SIZE_PER_TILE_BYTES;
	lx_hdr_pitch = pitch / tile_w / Bpp *
	    AFBC_V1_2_HEADER_SIZE_PER_TILE_BYTES;

	/* 5. calculate OVL_LX_SRC_SIZE, RGB565 use layout 4, src_h needs align to tile_h*/
	if (fmt != DRM_FORMAT_RGB565 && fmt != DRM_FORMAT_BGR565) {
		src_h_align = src_h_half_align;
		src_y_align = src_y_half_align;
	}
	lx_src_size = (src_h_align << 16) | src_w_align;

	/* 6. calculate OVL_LX_CLIP */
	lx_clip = 0;
	if (rotate) {
		if (src_x > src_x_align)
			lx_clip |= REG_FLD_VAL(reg_fld[FLD_LX_SRC_RIGHT_CLIP],
				src_x - src_x_align);
		if (src_x + src_w < src_x_align + src_w_align)
			lx_clip |= REG_FLD_VAL(reg_fld[FLD_LX_SRC_LEFT_CLIP],
				src_x_align + src_w_align - src_x - src_w);
		if (src_y > src_y_align)
			lx_clip |= REG_FLD_VAL(reg_fld[FLD_LX_SRC_BOTTOM_CLIP],
				src_y - src_y_align);
		if (src_y + src_h < src_y_align + src_h_align)
			lx_clip |= REG_FLD_VAL(reg_fld[FLD_LX_SRC_TOP_CLIP],
				src_y_align + src_h_align -
				src_y - src_h);
	} else {
		if (src_x > src_x_align)
			lx_clip |= REG_FLD_VAL(reg_fld[FLD_LX_SRC_LEFT_CLIP],
				src_x - src_x_align);
		if (src_x + src_w < src_x_align + src_w_align)
			lx_clip |= REG_FLD_VAL(reg_fld[FLD_LX_SRC_RIGHT_CLIP],
				src_x_align + src_w_align - src_x - src_w);
		if (src_y > src_y_align)
			lx_clip |= REG_FLD_VAL(reg_fld[FLD_LX_SRC_TOP_CLIP],
				src_y - src_y_align);
		if (src_y + src_h < src_y_align + src_h_align)
			lx_clip |= REG_FLD_VAL(reg_fld[FLD_LX_SRC_BOTTOM_CLIP],
				src_y_align + src_h_align -
				src_y - src_h);
	}

	/* 7. config register */
	buf_size = (dst_h - 1) * pitch + dst_w * Bpp;
	buf_total_size = header_offset + src_buf_tile_num * tile_body_size;

	if (exdma->data->mmsys_mapping)
		mmsys_reg = exdma->data->mmsys_mapping(comp);

	aid_sel_offset = mtk_ovl_aid_sel(comp);

	if (ext_lye_idx != LYE_NORMAL) {
		unsigned int id = ext_lye_idx - 1;

		if (mmsys_reg && aid_sel_offset) {
			sec_bit = mtk_ovl_exdma_aid_bit(comp, true, id);
			if (exdma->data->aid_per_layer_setting == true) {
				if (pending->is_sec && pending->addr) {
					cmdq_pkt_write(handle, comp->cmdq_base,
						(mmsys_reg + aid_sel_offset
						+ MT6985_OVL_LAYER_OFFEST * sec_bit),
						BIT(0), BIT(0));
				} else {
					cmdq_pkt_write(handle, comp->cmdq_base,
						(mmsys_reg + aid_sel_offset
						+ MT6985_OVL_LAYER_OFFEST * sec_bit),
						0, BIT(0));
				}
			} else {
				if (pending->is_sec && pending->addr)
					cmdq_pkt_write(handle, comp->cmdq_base,
						mmsys_reg + aid_sel_offset,
						BIT(sec_bit), BIT(sec_bit));
				else
					cmdq_pkt_write(handle, comp->cmdq_base,
						mmsys_reg + aid_sel_offset,
						0, BIT(sec_bit));
			}
		}

		write_ext_layer_addr_cmdq(comp, handle, id, lx_addr);
		write_ext_layer_hdr_addr_cmdq(comp, handle, id, lx_hdr_addr);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + OVL_EXDMA_ELX_PITCH_MSB(exdma, id),
			lx_pitch_msb, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + OVL_EXDMA_ELX_PITCH(exdma, id),
			lx_pitch, 0xffff);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + OVL_EXDMA_ELX_SRC_SIZE(exdma, id),
			lx_src_size, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + OVL_EXDMA_ELX_CLIP(exdma, id),
			lx_clip, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + OVL_EXDMA_ELX_HDR_PITCH(exdma, id),
			lx_hdr_pitch, ~0);
	} else {
		if (mmsys_reg && aid_sel_offset) {
			sec_bit = mtk_ovl_exdma_aid_bit(comp, false, lye_idx);
			if (exdma->data->aid_per_layer_setting == true) {
				if (pending->is_sec && pending->addr) {
					cmdq_pkt_write(handle, comp->cmdq_base,
						(mmsys_reg + aid_sel_offset
						+ MT6985_OVL_LAYER_OFFEST * sec_bit),
						BIT(0), BIT(0));
				} else {
					cmdq_pkt_write(handle, comp->cmdq_base,
						(mmsys_reg + aid_sel_offset
						+ MT6985_OVL_LAYER_OFFEST * sec_bit),
						0, BIT(0));
				}
			} else {
				if (pending->is_sec && pending->addr)
					cmdq_pkt_write(handle, comp->cmdq_base,
						mmsys_reg + aid_sel_offset,
						BIT(sec_bit), BIT(sec_bit));
				else
					cmdq_pkt_write(handle, comp->cmdq_base,
						mmsys_reg + aid_sel_offset,
						0, BIT(sec_bit));
			}
		}

		write_phy_layer_addr_cmdq(comp, handle, lye_idx, lx_addr);
		write_phy_layer_hdr_addr_cmdq(comp, handle, lye_idx,
					lx_hdr_addr);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_L0_PITCH_MSB],
			lx_pitch_msb, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_L0_PITCH],
			lx_pitch, 0xffff);

		mtk_addon_get_comp(crtc, crtc_state->lye_state.rpo_lye, &cmp_id, NULL);

		if (pending->pq_loop_type == 2) {
			if (priv->data->ovl_exdma_rule &&
				comp->id == cmp_id && cmp_id < DDP_COMPONENT_ID_MAX) {
				cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
					       regs[OVL_EXDMA_L0_SRC_SIZE], lx_src_size, ~0);
			} else {
				cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
					       regs[OVL_EXDMA_L0_SRC_SIZE], pending->dst_roi, ~0);
			}
		} else
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + regs[OVL_EXDMA_L0_SRC_SIZE], lx_src_size,
				       ~0);

		if (exdma->ovl_dis == false && enable == 1 && compress == 1 &&
			((lx_src_size&0xffff) * (lx_src_size>>16) == 0)) {
			DDPPR_ERR("%s, %s, idx=%d, en=%d, size:0x%08x\n", __func__,
				mtk_dump_comp_str_id(comp->id), lye_idx, enable, lx_src_size);
			exdma->ovl_dis = true;
		}
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_L0_CLIP],
			lx_clip, ~0);

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_L0_HDR_PITCH],
			lx_hdr_pitch, ~0);

		if (comp->bind_comp) {
			if (pending->pq_loop_type == 2
				&& (priv->data->ovl_exdma_rule &&
				comp->id == cmp_id && cmp_id < DDP_COMPONENT_ID_MAX))
				cmdq_pkt_write(handle, comp->cmdq_base, comp->bind_comp->regs_pa +
					regs[OVL_EXDMA_L0_SRC_SIZE], pending->dst_roi, ~0);
			else
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->bind_comp->regs_pa + regs[OVL_EXDMA_L0_SRC_SIZE], lx_src_size,
					~0);

			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->bind_comp->regs_pa + regs[OVL_EXDMA_L0_CLIP],
				lx_clip, ~0);
		}

	}

	return 0;
}

static int _ovl_UFOd_in(struct mtk_ddp_comp *comp, int connect,
			struct cmdq_pkt *handle)
{
	unsigned int value = 0, mask = 0;
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;
	const u32 *reg_fld = exdma->data->reg_fld;

	if (!connect) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_EXDMA_SRC_CON], 0,
			REG_FLD_MASK(reg_fld[FLD_LC_EN]));
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_EXDMA_LC_CON], 0, ~0);
		return 0;
	}

	SET_VAL_MASK(value, mask, 2, reg_fld[FLD_L0_LAYER_SRC]);
	SET_VAL_MASK(value, mask, 0, reg_fld[FLD_ALPHA_EN]);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + regs[OVL_EXDMA_LC_CON], value, mask);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + regs[OVL_EXDMA_LC_SRC_SEL], 0, 0x7);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + regs[OVL_EXDMA_SRC_CON],
		       OVL_EXDMA_FORCE_CONSTANT_LAYER,
		       REG_FLD_MASK(reg_fld[FLD_LC_EN]));

	return 0;
}

static void
mtk_ovl_exdma_addon_rsz_config(struct mtk_ddp_comp *comp, enum mtk_ddp_comp_id prev,
			 enum mtk_ddp_comp_id next, struct mtk_rect rsz_src_roi,
			 struct mtk_rect rsz_dst_roi, struct cmdq_pkt *handle)
{
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_crtc *crtc;
	struct mtk_crtc_state *crtc_state = NULL;
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;

	mtk_crtc = comp->mtk_crtc;
	crtc = &mtk_crtc->base;
	crtc_state = to_mtk_crtc_state(crtc->state);

	if (prev == DDP_COMPONENT_RSZ0 ||
		prev == DDP_COMPONENT_RSZ1 ||
		prev == DDP_COMPONENT_Y2R0 ||
		prev == DDP_COMPONENT_Y2R1) {
		int lc_x = rsz_dst_roi.x, lc_y = rsz_dst_roi.y;
		int lc_w = rsz_dst_roi.width, lc_h = rsz_dst_roi.height;

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
		{
			int bg_w, bg_h;

			_get_bg_roi(comp, &bg_h, &bg_w);
			lc_y = bg_h - lc_h - lc_y;
			lc_x = bg_w - lc_w - lc_x;
		}
#endif
		_ovl_UFOd_in(comp, 1, handle);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_EXDMA_LC_OFFSET],
			       ((lc_y << 16) | lc_x), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_EXDMA_LC_SRC_SIZE],
			       ((lc_h << 16) | lc_w), ~0);
	} else
		_ovl_UFOd_in(comp, 0, handle);

	if (prev == -1 &&
		comp->id != mtk_addon_path_get_cmp(crtc, 0, ONE_SCALING, MTK_OVL_EXDMA)) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_EXDMA_ROI_SIZE],
			       rsz_src_roi.height << 16 | rsz_src_roi.width,
			       ~0);
		_store_bg_roi(comp, rsz_src_roi.height, rsz_src_roi.width);
	}
}

static void mtk_ovl_exdma_addon_config(struct mtk_ddp_comp *comp,
				 enum mtk_ddp_comp_id prev,
				 enum mtk_ddp_comp_id next,
				 union mtk_addon_config *addon_config,
				 struct cmdq_pkt *handle)
{
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_crtc *crtc;
	struct mtk_crtc_state *crtc_state = NULL;
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;
	const u32 *reg_fld = exdma->data->reg_fld;

	mtk_crtc = comp->mtk_crtc;
	crtc = &mtk_crtc->base;
	crtc_state = to_mtk_crtc_state(crtc->state);

	if ((addon_config->config_type.module == DISP_RSZ ||
		addon_config->config_type.module == DISP_RSZ_v2 ||
		addon_config->config_type.module == DISP_RSZ_v3 ||
		addon_config->config_type.module == DISP_RSZ_v4 ||
		addon_config->config_type.module == DISP_RSZ_v5 ||
		addon_config->config_type.module == DISP_RSZ_v6) &&
		addon_config->config_type.type == ADDON_BETWEEN) {
		struct mtk_addon_rsz_config *config =
			&addon_config->addon_rsz_config;

		mtk_ovl_exdma_addon_rsz_config(comp, prev, next, config->rsz_src_roi,
					 config->rsz_dst_roi, handle);
	}

	if ((addon_config->config_type.module == DISP_MML_IR_PQ ||
		addon_config->config_type.module == DISP_MML_IR_PQ_1 ||
		addon_config->config_type.module == DISP_MML_IR_PQ_v3) &&
		(addon_config->config_type.type == ADDON_CONNECT ||
		addon_config->config_type.type == ADDON_DISCONNECT)) {
		struct mtk_addon_mml_config *config = &addon_config->addon_mml_config;
		struct mtk_rect src, dst;

		src = config->mml_src_roi[config->pipe];
		dst = config->mml_dst_roi[config->pipe];

		/* this rsz means enlarge/narrow, not component */
		mtk_ovl_exdma_addon_rsz_config(comp, prev, next, src, dst, handle);
	}

	if (((addon_config->config_type.module == OVL_RSZ_2)) &&
		addon_config->config_type.type == ADDON_BEFORE) {
		struct mtk_addon_rsz_config *config =
			&addon_config->addon_rsz_config;

		unsigned int width, height;
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		struct drm_crtc *crtc = &mtk_crtc->base;
		struct mtk_drm_private *priv = crtc->dev->dev_private;
		unsigned long crtc_idx = (unsigned long)drm_crtc_index(crtc);
		int fps;

		DDPDBG("exdma_addon_config:%s\n", mtk_dump_comp_str(comp));

		if (comp->mtk_crtc->is_dual_pipe) {
			if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_TILE_OVERHEAD))
				width = config->rsz_src_roi.width;
			else
				width = config->rsz_src_roi.width / 2;
			if (drm_crtc_index(crtc) == 2 && (width % 2)) {
				if (exdma->data->is_right_ovl_comp && exdma->data->is_right_ovl_comp(comp))
					width += 1;
				else
					width -= 1;
			}
		} else
			width = config->rsz_src_roi.width;

		height = config->rsz_src_roi.height;
		if (config->rsz_src_roi.width != 0
			&& config->rsz_src_roi.height != 0) {
			cmdq_pkt_write(handle, comp->cmdq_base,
					   comp->regs_pa + regs[OVL_EXDMA_ROI_SIZE],
					   height << 16 | width, ~0);
			_store_bg_roi(comp, height, width);
		}
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_EN],
				   ENABLE_OVL_EN, REG_FLD_MASK(reg_fld[FLD_OVL_EN]));
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_EN],
			ENABLE_FORCE_RELAY_MODE, REG_FLD_MASK(reg_fld[FLD_FORCE_RELAY_MODE]));

		if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_OVL_BW_MONITOR) &&
			(crtc_idx == 0)) {
			unsigned int bw_monitor_config, line_time, h;

			/****************************************************************/
			/*BURST_ACC_FBDC: 1/0:fbdc size/actual BW(fbdc+sBCH)			*/
			/*BURST_ACC_EN: 1: enable bw monitor 0: disable					*/
			/*BURST_ACC_WIN_SIZE:200us / 4AFBC line times(us)               */
			/****************************************************************/
			bw_monitor_config = REG_FLD_VAL(reg_fld[FLD_BURST_ACC_EN], 1);
			bw_monitor_config |= REG_FLD_VAL(reg_fld[FLD_BURST_ACC_FBDC], 0);

			if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params &&
				mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps != 0)
				fps = mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps;
			else
				fps = drm_mode_vrefresh(&crtc->state->adjusted_mode);
			h = crtc->state->adjusted_mode.vdisplay;
			line_time = 1000000 * 4 * 10 / fps / h / 12;

			ovl_win_size = (200 % line_time) ? (200 / line_time + 1) : (200 / line_time);
			bw_monitor_config |= REG_FLD_VAL(reg_fld[FLD_BURST_ACC_WIN_SIZE], ovl_win_size - 1);

			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + regs[OVL_EXDMA_BURST_MON_CFG], bw_monitor_config, ~0);
		}

		mtk_ovl_exdma_golden_setting(comp, config->is_dc, handle);

		if (priv->data->ovl_exdma_rule
			&& comp->id == mtk_addon_path_get_cmp(crtc, 0, ONE_SCALING, MTK_OVL_EXDMA)) {
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa
				+ regs[OVL_EXDMA_DATAPATH_CON], OVL_EXDMA_OUTPUT_CLAMP,
				REG_FLD_MASK(reg_fld[FLD_OUTPUT_CLAMP]));
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
				regs[OVL_EXDMA_MOUT], 0x1, 0x3);
		} else
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
				regs[OVL_EXDMA_MOUT], 0x2, 0x3);

		mtk_ovl_exdma_addon_rsz_config(comp, prev, next, config->rsz_src_roi,
					 config->rsz_dst_roi, handle);
	}

}

static void mtk_ovl_exdma_config_begin(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle, const u32 idx)
{
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;
	const u32 *reg_fld = exdma->data->reg_fld;
	unsigned int value = 0, mask = 0;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_crtc_state *crtc_state = NULL;


	if (!comp->mtk_crtc)
		return;

	/* no need connect to OVL PQ_LOOP or PQ_OUT path for external display so far */
	if (comp->mtk_crtc->base.index != 0)
		return;

	crtc_state = to_mtk_crtc_state(crtc->state);

	if (priv->data->ovl_exdma_rule &&
		comp->id == mtk_addon_path_get_cmp(crtc, 0, ONE_SCALING, MTK_OVL_EXDMA)) {
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa
			+ regs[OVL_EXDMA_DATAPATH_CON], OVL_EXDMA_OUTPUT_CLAMP,
			REG_FLD_MASK(reg_fld[FLD_OUTPUT_CLAMP]));
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
			regs[OVL_EXDMA_MOUT], 0x1, 0x3);
	} else
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa +
			regs[OVL_EXDMA_MOUT], 0x2, 0x3);


	SET_VAL_MASK(value, mask, 1, reg_fld[FLD_BURST16_EN]);
	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL) {
		SET_VAL_MASK(value, mask, 0, reg_fld[FLD_DDR_EN]);
		SET_VAL_MASK(value, mask, 0, reg_fld[FLD_DDR_ACK_EN]);
	} else {
		SET_VAL_MASK(value, mask, 1, reg_fld[FLD_DDR_EN]);
		SET_VAL_MASK(value, mask, 1, reg_fld[FLD_DDR_ACK_EN]);
	}
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + regs[OVL_EXDMA_RDMA_BURST_CON1],
		       value, mask);

#ifdef IF_ZERO
	if (idx != 0) {
		value |= OVL_EXDMA_BGCLR_IN_SEL;
		mask |= OVL_EXDMA_BGCLR_IN_SEL;
	}

	SET_VAL_MASK(value, mask, 0, reg_fld[FLD_PQ_OUT_EN]);
	SET_VAL_MASK(value, mask, 0, reg_fld[FLD_PQ_OUT_OPT]);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_EXDMA_DATAPATH_CON],
			value, mask);
#endif
}

static void mtk_ovl_exdma_connect(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			    enum mtk_ddp_comp_id prev,
			    enum mtk_ddp_comp_id next)
{
	DDPINFO("%s,%d\n", __func__, __LINE__);
}

void mtk_ovl_exdma_cal_golden_setting(bool cfg_dc,
				struct mtk_ddp_comp *comp, unsigned int *gs)
{
	bool is_dc = cfg_dc;
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const struct mtk_disp_ovl_exdma_data *data = exdma->data;

	DDPDBG("%s,is_dc:%d\n", __func__, is_dc);

	/* OVL_RDMA_MEM_GMC_SETTING_1 */
	gs[GS_OVL_RDMA_ULTRA_TH] = 0x3ff;
	gs[GS_OVL_RDMA_PRE_ULTRA_TH] = (!is_dc) ? 0x3ff : data->preultra_th_dc;

	/* OVL_RDMA_FIFO_CTRL */
	gs[GS_OVL_RDMA_FIFO_THRD] = 0;
	gs[GS_OVL_RDMA_FIFO_SIZE] = data->fifo_size;

	/* OVL_RDMA_MEM_GMC_SETTING_2 */
	gs[GS_OVL_RDMA_ISSUE_REQ_TH] = (!is_dc) ? data->issue_req_th_dl :
				data->issue_req_th_dc;
	gs[GS_OVL_RDMA_ISSUE_REQ_TH_URG] = (!is_dc) ? data->issue_req_th_urg_dl :
				data->issue_req_th_urg_dc;
	gs[GS_OVL_RDMA_REQ_TH_PRE_ULTRA] = 0;
	gs[GS_OVL_RDMA_REQ_TH_ULTRA] = 1;
	gs[GS_OVL_RDMA_FORCE_REQ_TH] = 0;

	/* OVL_RDMA_GREQ_NUM */
	gs[GS_OVL_RDMA_GREQ_NUM] = (!is_dc) ? (0xF1FF00FF | data->greq_num_dl)
						: 0xF1FFFFFF;

	/* OVL_RDMA_GREQURG_NUM */
	gs[GS_OVL_RDMA_GREQ_URG_NUM] = (!is_dc) ? data->greq_num_dl : 0x0;

	/* OVL_RDMA_ULTRA_SRC */
	gs[GS_OVL_RDMA_ULTRA_SRC] = (!is_dc) ? 0x8040 : 0xA040;

	/* OVL_RDMA_BUF_LOW_TH */
	gs[GS_OVL_RDMA_ULTRA_LOW_TH] = 0;
	gs[GS_OVL_RDMA_PRE_ULTRA_LOW_TH] = (!is_dc) ?
				0 : (gs[GS_OVL_RDMA_FIFO_SIZE] / 8);

	/* OVL_RDMA_BUF_HIGH_TH */
	gs[GS_OVL_RDMA_PRE_ULTRA_HIGH_TH] = (!is_dc) ?
				0 : (gs[GS_OVL_RDMA_FIFO_SIZE] * 6 / 8);
	gs[GS_OVL_RDMA_PRE_ULTRA_HIGH_DIS] = 1;

	/* OVL_EN */
	gs[GS_OVL_BLOCK_EXT_ULTRA] = (!is_dc) ? 0 : 1;
	gs[GS_OVL_BLOCK_EXT_PRE_ULTRA] = (!is_dc) ? 0 : 1;
	gs[GS_OVL_APB_MCYC_RD] = (!is_dc) ? data->apb_mcyc_rd : 1;

}

static int mtk_ovl_exdma_golden_setting(struct mtk_ddp_comp *comp,
				  bool cfg_dc,
				  struct cmdq_pkt *handle)
{
	unsigned long baddr = comp->regs_pa;
	unsigned int regval;
	unsigned int gs[GS_OVL_FLD_NUM];
	int i, layer_num;
	unsigned long Lx_base;
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;
	const u32 *reg_fld = exdma->data->reg_fld;

	layer_num = mtk_ovl_exdma_layer_num(comp);

	/* calculate ovl golden setting */
	mtk_ovl_exdma_cal_golden_setting(cfg_dc, comp, gs);

	/* OVL_RDMA_MEM_GMC_SETTING_1 */
	regval =
		gs[GS_OVL_RDMA_ULTRA_TH] + (gs[GS_OVL_RDMA_PRE_ULTRA_TH] << 16);
	for (i = 0; i < layer_num; i++) {
		Lx_base = i * OVL_LAYER_OFFSET + baddr;

		cmdq_pkt_write(handle, comp->cmdq_base,
			       Lx_base + regs[OVL_EXDMA_RDMA0_MEM_GMC_SETTING1],
			       regval, ~0);
	}

	/* OVL_RDMA_FIFO_CTRL */
	regval = gs[GS_OVL_RDMA_FIFO_THRD] + (gs[GS_OVL_RDMA_FIFO_SIZE] << 16);
	for (i = 0; i < layer_num; i++) {
		Lx_base = i * OVL_LAYER_OFFSET + baddr;

		cmdq_pkt_write(handle, comp->cmdq_base,
			       Lx_base + regs[OVL_EXDMA_RDMA0_FIFO_CTRL], regval,
			       ~0);
	}

	/* OVL_RDMA_MEM_GMC_SETTING_2 */
	regval = gs[GS_OVL_RDMA_ISSUE_REQ_TH] +
		 (gs[GS_OVL_RDMA_ISSUE_REQ_TH_URG] << 16) +
		 (gs[GS_OVL_RDMA_REQ_TH_PRE_ULTRA] << 28) +
		 (gs[GS_OVL_RDMA_REQ_TH_ULTRA] << 29) +
		 (gs[GS_OVL_RDMA_FORCE_REQ_TH] << 30);
	for (i = 0; i < layer_num; i++)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       baddr + regs[OVL_EXDMA_RDMA0_MEM_GMC_SETTING2] + i * 4,
			       regval, ~0);

	/* DISP_REG_OVL_RDMA_GREQ_NUM */
	regval = gs[GS_OVL_RDMA_GREQ_NUM];
	cmdq_pkt_write(handle, comp->cmdq_base,
		       baddr + regs[OVL_EXDMA_RDMA_GREQ_NUM], regval, ~0);

	/* DISP_REG_OVL_RDMA_GREQ_URG_NUM */
	regval = gs[GS_OVL_RDMA_GREQ_URG_NUM];
	cmdq_pkt_write(handle, comp->cmdq_base,
		       baddr + regs[OVL_EXDMA_RDMA_GREQ_URG_NUM], regval, ~0);

	/* DISP_REG_OVL_RDMA_ULTRA_SRC */
	regval = gs[GS_OVL_RDMA_ULTRA_SRC];
	cmdq_pkt_write(handle, comp->cmdq_base,
		       baddr + regs[OVL_EXDMA_RDMA_ULTRA_SRC], regval, ~0);

	/* DISP_REG_OVL_RDMAn_BUF_LOW */
	regval = gs[GS_OVL_RDMA_ULTRA_LOW_TH] +
		 (gs[GS_OVL_RDMA_PRE_ULTRA_LOW_TH] << 12);

	cmdq_pkt_write(handle, comp->cmdq_base,
			baddr + regs[OVL_EXDMA_RDMA0_BUF_LOW], regval,
			~0);

	/* DISP_REG_OVL_RDMAn_BUF_HIGH */
	regval = (gs[GS_OVL_RDMA_PRE_ULTRA_HIGH_TH] << 12) +
		 (gs[GS_OVL_RDMA_PRE_ULTRA_HIGH_DIS] << 31);

	cmdq_pkt_write(handle, comp->cmdq_base,
			baddr + regs[OVL_EXDMA_RDMA0_BUF_HIGH], regval,
			~0);

	/* OVL_EN */
	regval = (gs[GS_OVL_BLOCK_EXT_ULTRA] << 18) +
		 (gs[GS_OVL_BLOCK_EXT_PRE_ULTRA] << 19) +
		 (gs[GS_OVL_APB_MCYC_RD] << 23);
	cmdq_pkt_write(handle, comp->cmdq_base, baddr + regs[OVL_EXDMA_EN_CON],
		       regval, 0x23 << 18);

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL) {
		struct mtk_drm_crtc *mtk_crtc;
		struct drm_crtc *crtc;
		struct mtk_drm_private *priv;
		unsigned int inten;

		mtk_crtc = comp->mtk_crtc;
		crtc = &mtk_crtc->base;
		priv = mtk_crtc->base.dev->dev_private;

		switch (priv->data->mmsys_id) {
			case MMSYS_MT6993:
				inten = REG_FLD_VAL(reg_fld[FLD_CFG_DDR_EN_MAN], 1) |
						REG_FLD_VAL(reg_fld[FLD_CFG_OUT_DDR_EN], 1) |
						REG_FLD_VAL(reg_fld[FLD_CFG_DDREN_ACT_SEL], 1);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + regs[OVL_EXDMA_DDREN_CFG],
					inten, ~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + regs[OVL_EXDMA_STASH_CFG1],
					1 , ~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + regs[OVL_EXDMA_STASH_STALL_CFG0],
					0 , ~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + regs[OVL_EXDMA_STASH_STALL_CFG1],
					0 , ~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + regs[OVL_EXDMA_STASH_STALL_CFG2],
					0 , ~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + regs[OVL_EXDMA_STASH_STALL_CFG3],
					0 , ~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + regs[OVL_EXDMA_PREF_LEAD_CFG0],
					3 , ~0);
				break;
			default:
				break;
		}
	}

	return 0;
}

static dma_addr_t read_phy_layer_addr(struct mtk_ddp_comp *comp, int id)
{
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	dma_addr_t layer_addr = 0;
	const u16 *regs = exdma->data->regs;

	if (exdma->data->is_support_34bits) {
		layer_addr = readl(comp->regs + regs[OVL_EXDMA_L0_ADDR_MSB]);
		layer_addr = ((layer_addr & 0xf) << 32);
	}

	layer_addr += readl(comp->regs + regs[OVL_EXDMA_L0_ADDR]);

	return layer_addr;
}

static dma_addr_t read_ext_layer_addr(struct mtk_ddp_comp *comp, int id)
{
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	dma_addr_t layer_addr = 0;

	if (exdma->data->is_support_34bits) {
		layer_addr = readl(comp->regs + OVL_EXDMA_ELX_ADDR_MSB(exdma, id));
		layer_addr = ((layer_addr & 0xf) << 32);
	}

	layer_addr += readl(comp->regs + OVL_EXDMA_ELX_ADDR(exdma, id));

	return layer_addr;
}

static dma_addr_t read_phy_layer_hdr_addr(struct mtk_ddp_comp *comp, int id)
{
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	dma_addr_t layer_addr = 0;
	const u16 *regs = exdma->data->regs;

	if (exdma->data->is_support_34bits) {
		layer_addr = readl(comp->regs + regs[OVL_EXDMA_L0_ADDR_MSB]);
		layer_addr = ((layer_addr & 0xf00) << 24);
	}

	layer_addr += readl(comp->regs + regs[OVL_EXDMA_L0_HDR_ADDR]);

	return layer_addr;
}

static dma_addr_t read_ext_layer_hdr_addr(struct mtk_ddp_comp *comp, int id)
{
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	dma_addr_t layer_addr = 0;

	if (exdma->data->is_support_34bits) {
		layer_addr = readl(comp->regs + OVL_EXDMA_ELX_ADDR_MSB(exdma, id));
		layer_addr += ((layer_addr & 0xf00) << 24);
	}

	layer_addr += readl(comp->regs + OVL_EXDMA_ELX_HDR_ADDR(exdma, id));

	return layer_addr;
}

static int mtk_ovl_replace_bootup_mva(struct mtk_ddp_comp *comp,
				      struct cmdq_pkt *handle, void *params,
				      struct mtk_ddp_fb_info *fb_info)
{
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	unsigned int src_on = readl(comp->regs + exdma->data->regs[OVL_EXDMA_L0_EN]);
	dma_addr_t layer_addr, layer_mva;
	struct iommu_domain *domain;
	struct mtk_drm_private *priv;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_display_mode *mode = NULL;
	unsigned int bw;
	int ret = 0;
	unsigned int aid_sel_offset = 0;
	resource_size_t mmsys_reg = 0;

	if (unlikely(!(comp && comp->mtk_crtc))) {
		DDPPR_ERR("%s invalid comp or mtk_crtc\n", __func__);
		return -EINVAL;
	}

	mtk_crtc = comp->mtk_crtc;
	crtc = &mtk_crtc->base;
	priv = crtc->dev->dev_private;

	if (crtc->state) {
		mode = &crtc->state->adjusted_mode;
		bw = _layering_get_frame_bw(crtc, mode);
	}
	if (src_on & ENABLE_OVL_L_EN) {
		if (exdma->data->mmsys_mapping)
			mmsys_reg = exdma->data->mmsys_mapping(comp);

		aid_sel_offset = mtk_ovl_aid_sel(comp);

		if (mmsys_reg && aid_sel_offset &&
			(comp->id == DDP_COMPONENT_OVL_EXDMA3 || comp->id == DDP_COMPONENT_OVL_EXDMA4))
			cmdq_pkt_write(handle, comp->cmdq_base,	mmsys_reg + aid_sel_offset,
				0x0, 0x7);
		layer_addr = read_phy_layer_addr(comp, 0);
		if (comp->id == DDP_COMPONENT_OVL_EXDMA2 || comp->id == DDP_COMPONENT_OVL_EXDMA3) {
			DDPMSG("%s, replace mva same as pa %pad\n", __func__, &layer_addr);
			mtk_crtc->lk_dma_addr = layer_addr;
			domain = iommu_get_domain_for_dev(mtk_smmu_get_shared_device(comp->dev));
			if (domain == NULL) {
				DDPPR_ERR("%s, iommu_get_domain fail\n", __func__);
				return -1;
			}
			ret = iommu_map(domain, layer_addr, layer_addr,
				ROUNDUP(fb_info->size, PAGE_SIZE),
				IOMMU_READ | IOMMU_WRITE, GFP_KERNEL);
			write_phy_layer_addr_cmdq(comp, handle, 0, layer_addr);
		} else {
			layer_mva = layer_addr - fb_info->fb_pa + fb_info->fb_gem->dma_addr;
			write_phy_layer_addr_cmdq(comp, handle, 0, layer_mva);
		}
		if (mode)
			comp->qos_bw = bw;
	}

	return 0;
}

static int mtk_ovl_calc_layer_hrt_bw(struct mtk_drm_crtc *mtk_crtc, unsigned int phy_id,
		struct mtk_disp_ovl_exdma *exdma, unsigned int uncompr_bw, bool usage_ovl_compr,
		unsigned int *body_bw, unsigned int *hdr_bw,
		unsigned int *stash_body_bw, unsigned int *stash_hdr_bw)
{
	unsigned int compr_ratio = 0, unit = 100;
	struct mtk_drm_private *priv = NULL;
	unsigned int total = 0;
	u32 stash_bw_min = 0;
	u32 afbc_header_bw_min = 0;

	if (exdma && exdma->data->stash_min_ostdl > 1)
		stash_bw_min = (exdma->data->stash_min_ostdl - 1) * 16 + 1;
	else
		stash_bw_min = 49;

	if (exdma && exdma->data->afbc_header_min_ostdl > 1)
		afbc_header_bw_min = (exdma->data->afbc_header_min_ostdl - 1) * 32 + 1;
	else
		afbc_header_bw_min = 129;

	if (IS_ERR_OR_NULL(mtk_crtc) || phy_id >= MAX_LAYER_NR)
		return -EINVAL;

	priv = mtk_crtc->base.dev->dev_private;
	if (IS_ERR_OR_NULL(priv) || !mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_MMQOS_SUPPORT))
		return -EINVAL;

	if (usage_ovl_compr) {
		if (!IS_ERR_OR_NULL(body_bw)) {
			compr_ratio = mtk_bwm_get_layer_compress_ratio(mtk_crtc, phy_id, true);
			if (compr_ratio > 0 && compr_ratio < 1000)
				*body_bw = uncompr_bw * compr_ratio / 1000 + 1;
			else
				*body_bw = uncompr_bw;
			total += *body_bw;
		}
		if (!IS_ERR_OR_NULL(hdr_bw)) {
			if (uncompr_bw) {
				*hdr_bw = (uncompr_bw > 32) ? (uncompr_bw / 32 + 1) : 1;
				*hdr_bw = (*hdr_bw > afbc_header_bw_min) ? *hdr_bw : afbc_header_bw_min;
				total += *hdr_bw;
			} else
				*hdr_bw = 0;
		}
		if (!IS_ERR_OR_NULL(stash_body_bw)) {
			if (uncompr_bw) {
				*stash_body_bw = uncompr_bw * 2 / 256 + 1;
				*stash_body_bw = *stash_body_bw > stash_bw_min ? *stash_body_bw : stash_bw_min;
				total += *stash_body_bw;
			} else
				*stash_body_bw = 0;
		}
		if (!IS_ERR_OR_NULL(stash_hdr_bw)) {
			if (uncompr_bw) {
				*stash_hdr_bw = uncompr_bw * 2 / 32 / 256 + 1;
				*stash_hdr_bw = *stash_hdr_bw > stash_bw_min ? *stash_hdr_bw : stash_bw_min;
				total += *stash_hdr_bw;
			} else
				*stash_hdr_bw = 0;
		}
		/* avoid of compress ratio floating*/
		if (compr_ratio > 0 && compr_ratio < 1000)
			total = total % unit ? (total / unit + 1) * unit : total / unit * unit;
		if (total > uncompr_bw)
			total = uncompr_bw;
	} else {
		if (!IS_ERR_OR_NULL(body_bw)) {
			*body_bw = uncompr_bw;
			total += *body_bw;
		}
		if (!IS_ERR_OR_NULL(hdr_bw))
			*hdr_bw = 0;
		if (!IS_ERR_OR_NULL(stash_body_bw)) {
			if (uncompr_bw) {
				*stash_body_bw = uncompr_bw / 256 + 1;
				*stash_body_bw = *stash_body_bw > stash_bw_min ? *stash_body_bw : stash_bw_min;
				total += *stash_body_bw;
			} else
				*stash_body_bw = 0;
		}
		if (!IS_ERR_OR_NULL(stash_hdr_bw))
			*stash_hdr_bw = 0;
	}
	DDPDBG("%s,phy_id:%u,compr:%d,ratio:%u,bw:%u(%u,%u,%u,%u),uncompr_bw:%u\n",
		__func__, phy_id, usage_ovl_compr, compr_ratio, total,
		IS_ERR_OR_NULL(body_bw) ? 999999 : *body_bw,
		IS_ERR_OR_NULL(hdr_bw) ? 999999 : *hdr_bw,
		IS_ERR_OR_NULL(stash_body_bw) ? 999999 : *stash_body_bw,
		IS_ERR_OR_NULL(stash_hdr_bw) ? 999999 : *stash_hdr_bw,
		uncompr_bw);
	return total;
}

static void mtk_ovl_backup_info_cmp(struct mtk_ddp_comp *comp, bool *compare)
{
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;
	const u32 *reg_fld = exdma->data->reg_fld;
	void __iomem *baddr = comp->regs, *Lx_base = NULL;
	int i = 0;
	unsigned int src_on = readl(exdma->data->regs[OVL_EXDMA_L0_EN] + baddr);
	struct mtk_ovl_backup_info cur_info[MAX_LAYER_NUM];

	memset(cur_info, 0, sizeof(cur_info));
	for (i = 0; i < mtk_ovl_exdma_layer_num(comp); i++) {
		unsigned int val = 0;

		Lx_base = i * OVL_LAYER_OFFSET + baddr;
		cur_info[i].layer = i;
		cur_info[i].layer_en = src_on & (0x1 << i);
		if (!cur_info[i].layer_en) {
			DDPDBG("%s:layer%d,en %d,size 0x%x,addr 0x%lx\n",
			       __func__, i, cur_info[i].layer_en,
			       cur_info[i].src_size, (unsigned long)cur_info[i].addr);
			continue;
		}

		cur_info[i].con = readl(regs[OVL_EXDMA_L0_CON2] + baddr);
		cur_info[i].addr = read_phy_layer_addr(comp, i);
		cur_info[i].src_size =
			readl(regs[OVL_EXDMA_L0_SRC_SIZE] + Lx_base);

		val = readl(regs[OVL_EXDMA_L0_PITCH] + Lx_base);
		cur_info[i].src_pitch =
			REG_FLD_VAL_GET(reg_fld[FLD_LX_SRC_PITCH], val);

		val = readl(regs[OVL_EXDMA_DATAPATH_CON] + Lx_base);
		cur_info[i].data_path_con =
			readl(regs[OVL_EXDMA_DATAPATH_CON] + Lx_base);

		DDPDBG("%s:layer%d,en %d,size 0x%x, addr 0x%lx\n", __func__, i,
		       cur_info[i].layer_en, cur_info[i].src_size,
		       (unsigned long)cur_info[i].addr);
		if (memcmp(&cur_info[i], &exdma->backup_info[i],
			   sizeof(struct mtk_ovl_backup_info)) != 0)
			*compare = true;
	}
	memcpy(exdma->backup_info, cur_info,
	       sizeof(struct mtk_ovl_backup_info) * MAX_LAYER_NUM);
}

static int mtk_ovl_exdma_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd io_cmd, void *params)
{
	int ret = 0;
	struct mtk_drm_private *priv;
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;
	const u32 *reg_fld = exdma->data->reg_fld;

	if (!(comp->mtk_crtc && comp->mtk_crtc->base.dev)) {
		DDPINFO("%s %s %u has invalid CRTC or device\n",
			__func__, mtk_dump_comp_str(comp), io_cmd);
		return -INVALID;
	}

	priv = comp->mtk_crtc->base.dev->dev_private;
	switch (io_cmd) {
	case MTK_IO_CMD_OVL_GOLDEN_SETTING: {
		struct mtk_ddp_config *cfg;

		cfg = (struct mtk_ddp_config *)params;
		mtk_ovl_exdma_golden_setting(comp,
			cfg->p_golden_setting_context->is_dc, handle);
		break;
	}
	case OVL_ALL_LAYER_OFF:
	{
		int *keep_first_layer = params;

		mtk_ovl_exdma_all_layer_off(comp, handle, *keep_first_layer);
		break;
	}
	case IRQ_LEVEL_ALL: {
		unsigned int inten;

		inten = REG_FLD_VAL(reg_fld[FLD_RDMA0_EOF_ABNORMAL_INTEN], 1) |
			REG_FLD_VAL(reg_fld[FLD_RDMA1_EOF_ABNORMAL_INTEN], 1) |
			REG_FLD_VAL(reg_fld[FLD_RDMA2_EOF_ABNORMAL_INTEN], 1) |
			REG_FLD_VAL(reg_fld[FLD_RDMA3_EOF_ABNORMAL_INTEN], 1) |
			REG_FLD_VAL(reg_fld[FLD_ABNORMAL_SOF_INTEN], 1) |
			REG_FLD_VAL(reg_fld[FLD_OVL_FME_UND_INTEN], 1) |
			REG_FLD_VAL(reg_fld[FLD_OVL_START_INTEN], 1);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_EXDMA_INTSTA], 0,
			       ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_EXDMA_INTEN], inten,
			       ~0);
		break;
	}
	case IRQ_LEVEL_NORMAL: {
		unsigned int inten;

		//inten = REG_FLD_VAL(INTEN_FLD_FME_UND_INTEN, 1) |
		//		REG_FLD_VAL(INTEN_FLD_FME_CPL_INTEN, 1);
		inten = 0; /* remove me after irq handling done */
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_EXDMA_INTSTA], 0,
			       ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_EXDMA_INTEN], inten,
			       ~0);
		break;
	}
	case IRQ_LEVEL_IDLE: {
		unsigned int inten;

		inten = REG_FLD_VAL(reg_fld[FLD_OVL_REG_CMT_INTEN], 1) |
			REG_FLD_VAL(reg_fld[FLD_OVL_FME_CPL_INTEN], 1) |
			REG_FLD_VAL(reg_fld[FLD_OVL_START_INTEN], 1);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_EXDMA_INTEN], 0, inten);
		break;
	}
	case PMQOS_SET_BW: {
#ifdef IF_ZERO /* not ready for dummy register method */
		struct mtk_drm_crtc *mtk_crtc;
		struct cmdq_pkt_buffer *cmdq_buf;
		u32 ovl_bw, slot_num;

		if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT))
			break;

		mtk_crtc = comp->mtk_crtc;
		cmdq_buf = &(mtk_crtc->gce_obj.buf);

		/* process FBDC */
		slot_num = __mtk_disp_pmqos_slot_look_up(comp->id,
					    DISP_BW_FBDC_MODE);
		ovl_bw = *(unsigned int *)(cmdq_buf->va_base +
					    DISP_SLOT_PMQOS_BW(slot_num));

		__mtk_disp_set_module_srt(comp->fbdc_qos_req, comp->id, ovl_bw, 0,
					    DISP_BW_FBDC_MODE, priv->data->real_srt_ostdl);

		/* process normal */
		slot_num = __mtk_disp_pmqos_slot_look_up(comp->id,
					    DISP_BW_NORMAL_MODE);
		ovl_bw = *(unsigned int *)(cmdq_buf->va_base +
					    DISP_SLOT_PMQOS_BW(slot_num));

		__mtk_disp_set_module_srt(comp->qos_req, comp->id, ovl_bw, 0,
					    DISP_BW_NORMAL_MODE, priv->data->real_srt_ostdl);
#endif
		break;
	}
	case PMQOS_SET_HRT_BW: {
		u32 bw_val = *(unsigned int *)params;
		struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
		unsigned int phy_id = 0, usage_ovl_fmt = 0, usage_ovl_compr = 0;
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		u32 hdr_bw_val = 0;
		u32 stash_bw_val = 0;
		u32 hdr_stash_bw_val = 0;
		int ret = 0;

		if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT))
			break;

		if (!priv->data->respective_ostdl) {
			DDPPR_ERR("respective_ostdl do not set\n");
			break;
		}

		if (exdma->data->ovl_phy_mapping)
			phy_id = exdma->data->ovl_phy_mapping(comp);

		usage_ovl_fmt = mtk_crtc->usage_ovl_fmt[phy_id];
		usage_ovl_compr = mtk_crtc->usage_ovl_compr[phy_id];

		bw_val = (bw_val * usage_ovl_fmt) >> 2;

		if (debug_module_bw[phy_id])
			bw_val = debug_module_bw[phy_id];

		ret = mtk_ovl_calc_layer_hrt_bw(mtk_crtc, phy_id, exdma, bw_val, usage_ovl_compr, NULL,
				&hdr_bw_val, &stash_bw_val, &hdr_stash_bw_val);
		if (ret < 0) {
			DDPMSG("%s,%d failed to calc layer hrt bw, ret:%d\n", __func__, __LINE__, ret);
			break;
		}

		if (bw_val != comp->last_hrt_bw) {
			DDPDBG("%s bw_val %u -> %u\n",
				mtk_dump_comp_str_id(comp->id), comp->last_hrt_bw, bw_val);
			__mtk_disp_set_module_hrt(comp->hrt_qos_req, comp->id, bw_val,
				priv->data->respective_ostdl);
			comp->last_hrt_bw = bw_val;
		}

		if (!IS_ERR(comp->hdr_qos_req)) {
			if (hdr_bw_val != comp->last_hdr_bw) {
				DDPDBG("%s hdr_bw_val %u -> %u\n",
					mtk_dump_comp_str_id(comp->id), comp->last_hdr_bw, hdr_bw_val);
				__mtk_disp_set_module_hrt(comp->hdr_qos_req, comp->id, hdr_bw_val,
					priv->data->respective_ostdl);
				comp->last_hdr_bw = hdr_bw_val;
			}
		}

		if (!IS_ERR(comp->stash_qos_req)) {
			if (stash_bw_val != comp->last_stash_bw) {
				DDPDBG("%s stash_bw_val %u -> %u\n",
					mtk_dump_comp_str_id(comp->id), comp->last_stash_bw, stash_bw_val);
				__mtk_disp_set_module_hrt(comp->stash_qos_req, comp->id, stash_bw_val,
					priv->data->respective_ostdl);
				comp->last_stash_bw = stash_bw_val;
			}
		}

		if (!IS_ERR(comp->hdr_stash_qos_req)) {
			if (hdr_stash_bw_val != comp->last_hdr_stash_bw) {
				DDPDBG("%s hdr_stash_bw_val %u -> %u\n",
					mtk_dump_comp_str_id(comp->id), comp->last_hdr_stash_bw, hdr_stash_bw_val);
				__mtk_disp_set_module_hrt(comp->hdr_stash_qos_req, comp->id, hdr_stash_bw_val,
					priv->data->respective_ostdl);
				comp->last_hdr_stash_bw = hdr_stash_bw_val;
			}
		}

		ret = OVL_REQ_HRT;
		break;
	}
	case PMQOS_SET_HRT_BW_DELAY: {
		u32 bw_val = *(unsigned int *)params;
		struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
		unsigned int phy_id = 0, usage_ovl_fmt = 0, usage_ovl_compr = 0;
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		u32 hdr_bw_val = 0;
		u32 stash_bw_val = 0;
		u32 hdr_stash_bw_val = 0;
		int ret = 0;

		if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT))
			break;

		if (!priv->data->respective_ostdl) {
			DDPPR_ERR("respective_ostdl do not set\n");
			break;
		}

		if (!handle) {
			DDPPR_ERR("no cmdq handle\n");
			break;
		}

		if (exdma->data->ovl_phy_mapping)
			phy_id = exdma->data->ovl_phy_mapping(comp);

		usage_ovl_fmt = mtk_crtc->usage_ovl_fmt[phy_id];
		usage_ovl_compr = mtk_crtc->usage_ovl_compr[phy_id];

		bw_val = (bw_val * usage_ovl_fmt) >> 2;

		if (debug_module_bw[phy_id])
			bw_val = debug_module_bw[phy_id];

		ret = mtk_ovl_calc_layer_hrt_bw(mtk_crtc, phy_id, exdma, bw_val, usage_ovl_compr, NULL,
				&hdr_bw_val, &stash_bw_val, &hdr_stash_bw_val);
		if (ret < 0) {
			DDPMSG("%s,%d failed to calc layer hrt bw, ret:%d\n", __func__, __LINE__, ret);
			break;
		}

		if (bw_val > comp->last_hrt_bw) {
			DDPDBG("%s bw_val fast up %u -> %u\n",
				mtk_dump_comp_str_id(comp->id), comp->last_hrt_bw, bw_val);
			__mtk_disp_set_module_hrt(comp->hrt_qos_req, comp->id, bw_val,
				priv->data->respective_ostdl);
			cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
				mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_CUR_BW_VAL(phy_id)),
				NO_PENDING_HRT, ~0);
		} else if (bw_val < comp->last_hrt_bw) {
			DDPDBG("%s bw_val will slow down %u -> %u\n",
				mtk_dump_comp_str_id(comp->id), comp->last_hrt_bw, bw_val);
			cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
				mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_CUR_BW_VAL(phy_id)),
				bw_val, ~0);
		}
		comp->last_hrt_bw = bw_val;

		if (!IS_ERR(comp->hdr_qos_req)) {
			if (hdr_bw_val > comp->last_hdr_bw) {
				DDPDBG("%s hdr_bw fast up %u -> %u\n",
					mtk_dump_comp_str_id(comp->id), comp->last_hdr_bw, hdr_bw_val);
				__mtk_disp_set_module_hrt(comp->hdr_qos_req, comp->id, hdr_bw_val,
					priv->data->respective_ostdl);
				cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
				mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_CUR_HDR_BW_VAL(phy_id)),
					NO_PENDING_HRT, ~0);
			} else if (hdr_bw_val < comp->last_hdr_bw) {
				DDPDBG("%s hdr_bw_val will slow down %u -> %u\n",
					mtk_dump_comp_str_id(comp->id), comp->last_hdr_bw, hdr_bw_val);
				cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
				mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_CUR_HDR_BW_VAL(phy_id)),
					hdr_bw_val, ~0);
			}
			comp->last_hdr_bw = hdr_bw_val;
		}

		if (!IS_ERR(comp->stash_qos_req)) {
			if (stash_bw_val > comp->last_stash_bw) {
				DDPDBG("%s stash_bw_val fast up %u -> %u\n",
					mtk_dump_comp_str_id(comp->id), comp->last_stash_bw, stash_bw_val);
				__mtk_disp_set_module_hrt(comp->stash_qos_req, comp->id, stash_bw_val,
					priv->data->respective_ostdl);
				cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
				mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_CUR_STASH_BW_VAL(phy_id)),
					NO_PENDING_HRT, ~0);
			} else if (stash_bw_val < comp->last_stash_bw) {
				DDPDBG("%s stash_bw_val will slow down %u -> %u\n",
					mtk_dump_comp_str_id(comp->id), comp->last_stash_bw, stash_bw_val);
				cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
				mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_CUR_STASH_BW_VAL(phy_id)),
					stash_bw_val, ~0);
			}
			comp->last_stash_bw = stash_bw_val;
		}

		if (!IS_ERR(comp->hdr_stash_qos_req)) {
			if (hdr_stash_bw_val > comp->last_hdr_stash_bw) {
				DDPDBG("%s hdr_stash_bw_val fast up %u -> %u\n",
					mtk_dump_comp_str_id(comp->id), comp->last_hdr_stash_bw, hdr_stash_bw_val);
				__mtk_disp_set_module_hrt(comp->hdr_stash_qos_req, comp->id, hdr_stash_bw_val,
					priv->data->respective_ostdl);
				cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
				mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_CUR_HDR_STASH_BW_VAL(phy_id)),
					NO_PENDING_HRT, ~0);
			} else if (hdr_stash_bw_val < comp->last_hdr_stash_bw) {
				DDPDBG("%s hdr_stash_bw_val will slow down %u -> %u\n",
					mtk_dump_comp_str_id(comp->id), comp->last_hdr_stash_bw, hdr_stash_bw_val);
				cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
				mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_CUR_HDR_STASH_BW_VAL(phy_id)),
					hdr_stash_bw_val, ~0);
			}
			comp->last_hdr_stash_bw = hdr_stash_bw_val;
		}

		ret = OVL_REQ_HRT;
		break;
	}
	case PMQOS_SET_HRT_BW_DELAY_POST: {
		u32 bw_val = 0;
		struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
		unsigned int phy_id = 0;
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		u32 hdr_bw_val = 0;
		u32 stash_bw_val = 0;
		u32 hdr_stash_bw_val = 0;

		if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT))
			break;

		if (!priv->data->respective_ostdl) {
			DDPPR_ERR("respective_ostdl do not set\n");
			break;
		}

		if (exdma->data->ovl_phy_mapping)
			phy_id = exdma->data->ovl_phy_mapping(comp);

		bw_val = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
			DISP_SLOT_CUR_BW_VAL(phy_id));
		if (bw_val != NO_PENDING_HRT && bw_val <= comp->last_hrt_bw) {
			DDPINFO("%s bw_val slow down to %u\n",
						mtk_dump_comp_str_id(comp->id), comp->last_hrt_bw);
			__mtk_disp_set_module_hrt(comp->hrt_qos_req, comp->id, comp->last_hrt_bw,
					priv->data->respective_ostdl);
			*(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_CUR_BW_VAL(phy_id)) =	NO_PENDING_HRT;
		}

		if (!IS_ERR(comp->hdr_qos_req)) {
			hdr_bw_val = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_CUR_HDR_BW_VAL(phy_id));
			if (hdr_bw_val != NO_PENDING_HRT && hdr_bw_val <= comp->last_hdr_bw) {
				DDPINFO("%s hdr_bw_val slow down to %u\n",
					mtk_dump_comp_str_id(comp->id), comp->last_hdr_bw);
				__mtk_disp_set_module_hrt(comp->hdr_qos_req, comp->id, comp->last_hdr_bw,
					priv->data->respective_ostdl);
				*(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
					DISP_SLOT_CUR_HDR_BW_VAL(phy_id)) = NO_PENDING_HRT;
			}
		}

		if (!IS_ERR(comp->stash_qos_req)) {
			stash_bw_val = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_CUR_STASH_BW_VAL(phy_id));
			if (stash_bw_val != NO_PENDING_HRT && stash_bw_val <= comp->last_stash_bw) {
				DDPINFO("%s stash_bw_val slow down to %u\n",
					mtk_dump_comp_str_id(comp->id), comp->last_stash_bw);
				__mtk_disp_set_module_hrt(comp->stash_qos_req, comp->id, comp->last_stash_bw,
					priv->data->respective_ostdl);
				*(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
					DISP_SLOT_CUR_STASH_BW_VAL(phy_id)) = NO_PENDING_HRT;
			}
		}

		if (!IS_ERR(comp->hdr_stash_qos_req)) {
			hdr_stash_bw_val = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_CUR_HDR_STASH_BW_VAL(phy_id));
			if (hdr_stash_bw_val != NO_PENDING_HRT && hdr_stash_bw_val <= comp->last_hdr_stash_bw) {
				DDPINFO("%s hdr_stash_bw_val slow down to %u\n",
					mtk_dump_comp_str_id(comp->id), comp->last_hdr_stash_bw);
				__mtk_disp_set_module_hrt(comp->hdr_stash_qos_req, comp->id, comp->last_hdr_stash_bw,
					priv->data->respective_ostdl);
				*(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
					DISP_SLOT_CUR_HDR_STASH_BW_VAL(phy_id)) = NO_PENDING_HRT;
			}
		}

		ret = OVL_REQ_HRT;
		break;
	}
	case PMQOS_GET_LARB_PORT_HRT_BW: {
		struct mtk_larb_port_bw *data = (struct mtk_larb_port_bw *)params;
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		struct drm_crtc *crtc = &mtk_crtc->base;
		unsigned int bw_val = data->bw_base;
		unsigned int body_bw = 0, hdr_bw = 0, stash_body_bw = 0, stash_hdr_bw = 0;
		unsigned int phy_id = 0, usage_ovl_fmt = 0, usage_ovl_compr = 0;
		int ret = 0;

		if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT))
			break;

		data->larb_id = -1;
		data->bw = 0;
		if (data->type != CHANNEL_HRT_READ)
			break;

		if (IS_ERR_OR_NULL(comp->larb_ids))
			data->larb_id = comp->larb_id;
		else
			data->larb_id = comp->larb_ids[0];

		if (data->larb_id < 0)
			break;

		if (exdma->data->ovl_phy_mapping)
			phy_id = exdma->data->ovl_phy_mapping(comp);

		usage_ovl_fmt = mtk_crtc->usage_ovl_fmt[phy_id];
		usage_ovl_compr = mtk_crtc->usage_ovl_compr[phy_id];

		if (usage_ovl_fmt == 0)
			break;

		if (!data->bw_base)
			bw_val = mtk_drm_primary_frame_bw(crtc);
		bw_val = (bw_val * usage_ovl_fmt) >> 2;

		if (debug_channel_wo_compress) {
			if (usage_ovl_compr)
				bw_val = bw_val * 90 / 100;
			data->bw = bw_val;
		} else {
			ret = mtk_ovl_calc_layer_hrt_bw(mtk_crtc, phy_id, exdma, bw_val, usage_ovl_compr,
					&body_bw, &hdr_bw, &stash_body_bw, &stash_hdr_bw);
			if (ret < 0) {
				DDPMSG("%s,%d failed to calc layer hrt bw, ret:%d\n", __func__, __LINE__, ret);
				break;
			}
			data->bw = ret;
		}
		DDPQOS("%s,%s-%d,larb:%d,layer:%u,type:%d,bw:%d(%u,%u,%u,%u),compr:%d,base:%u,dbg:%d\n",
			__func__, mtk_dump_comp_str_id(comp->id), comp->id,
			data->larb_id, phy_id, data->type, data->bw, body_bw,
			hdr_bw, stash_body_bw, stash_hdr_bw, usage_ovl_compr,
			bw_val, debug_channel_wo_compress);
		break;
	}
	case PMQOS_UPDATE_BW: {
		struct drm_crtc *crtc;
		struct mtk_disp_ovl_exdma *ovl = comp_to_ovl_exdma(comp);
		struct mtk_drm_crtc *mtk_crtc;
		unsigned int force_update = 0; /* force_update repeat last qos BW */
		unsigned int update_pending = 0;
		unsigned int crtc_idx, channel_id = 0;

		if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT))
			break;

		mtk_crtc = comp->mtk_crtc;
		crtc = &mtk_crtc->base;
		crtc_idx = drm_crtc_index(crtc);

		if (ovl->data->ovl_ch_mapping)
			channel_id = ovl->data->ovl_ch_mapping(comp);

		/* process FBDC */
		/* qos BW only has one port for one device, no need to separate */
		//__mtk_disp_set_module_srt(comp->fbdc_qos_req, comp->id, comp->fbdc_bw, 0,
		//			    DISP_BW_FBDC_MODE, priv->data->real_srt_ostdl);

		if (params) {
			force_update = *(unsigned int *)params;
			/* tricky way use variable force update */
			update_pending = (force_update == DISP_BW_UPDATE_PENDING);
			force_update = (force_update == DISP_BW_FORCE_UPDATE) ? 1 : 0;
		}

		if (!force_update && !update_pending) {
			mtk_crtc->total_srt += comp->qos_bw;
			if (channel_id < 4)
				priv->srt_channel_bw_sum[crtc_idx][channel_id] += comp->qos_bw;
		}

		/* process normal */
		if (!force_update && comp->last_qos_bw == comp->qos_bw)
			break;

		__mtk_disp_set_module_srt(comp->qos_req, comp->id, comp->qos_bw, 0,
					    DISP_BW_NORMAL_MODE, priv->data->real_srt_ostdl);
		comp->last_qos_bw = comp->qos_bw;
		if (!force_update && update_pending) {
			mtk_crtc->total_srt += comp->qos_bw;
			if (channel_id < 4)
				priv->srt_channel_bw_sum[crtc_idx][channel_id] += comp->qos_bw;
		}

		DDPINFO("update ovl qos bw to %u, %u peak %u %u\n",
			comp->qos_bw, comp->qos_bw_other, comp->hrt_bw, comp->hrt_bw_other);
		break;
	}
	case OVL_REPLACE_BOOTUP_MVA: {
		struct mtk_ddp_fb_info *fb_info =
			(struct mtk_ddp_fb_info *)params;

		mtk_ovl_exdma_stash_config(comp, handle, 0, LYE_NORMAL, NULL);
		mtk_ovl_replace_bootup_mva(comp, handle, params, fb_info);
		if (priv->data->mmsys_id == MMSYS_MT6989 ||
			priv->data->mmsys_id == MMSYS_MT6991 ||
			priv->data->mmsys_id == MMSYS_MT6993)
			iommu_dev_disable_feature(mtk_smmu_get_shared_device(comp->dev), IOMMU_DEV_FEAT_BYPASS_S1);
		break;
	}
	case BACKUP_INFO_CMP: {
		mtk_ovl_backup_info_cmp(comp, params);
		break;
	}
	case BACKUP_OVL_STATUS: {
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		u32 idx = drm_crtc_index(&mtk_crtc->base);
		dma_addr_t slot = mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_OVL_STATUS(idx));

		cmdq_pkt_mem_move(handle, comp->cmdq_base,
			comp->regs_pa + exdma->data->regs[OVL_EXDMA_STA],
			slot, CMDQ_THR_SPR_IDX3);
		break;
	}
	case OVL_GET_SOURCE_BPC: {
		struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);

		if (exdma && exdma->data) {
			DDPINFO("%s, source_bpc[%d]\n", __func__, exdma->data->source_bpc);
			return exdma->data->source_bpc;
		}
		break;
	}
	case OVL_GET_SELFLOOP_SUPPORT: {
		DDPINFO("%s,SELFLOOP_SUPPORT not support\n", __func__);
		break;
	}
	case OVL_SET_PQ_OUT: {
		DDPINFO("%s,SET_PQ_OUT not support\n", __func__);
		break;
	}
	case GET_OVL_SYS_NUM: {
		struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);

		if (exdma->data->ovlsys_mapping) {
			DDPINFO("%s, %d, comp id:%d, ovlsys:%d\n", __func__, __LINE__,
				comp->id, exdma->data->ovlsys_mapping(comp));
			return exdma->data->ovlsys_mapping(comp);
		}

		DDPMSG("%s, %d, invalid comp\n", __func__, __LINE__);
		return -1;
		//break;
	}
	case OVL_PHY_USAGE: {
		struct mtk_plane_state *plane_state;
		struct mtk_drm_crtc *mtk_crtc;

		plane_state = (struct mtk_plane_state *)params;
		mtk_crtc = comp->mtk_crtc;

		mtk_ovl_update_hrt_usage(mtk_crtc, comp, plane_state);

		break;
	}
	case OVL_COMP_TO_PHY_ID: {
		struct mtk_disp_ovl_exdma *ovl = comp_to_ovl_exdma(comp);
		unsigned int *phy_id = (unsigned int *)params;

		if (ovl->data->ovl_phy_mapping)
			*phy_id = ovl->data->ovl_phy_mapping(comp);
		break;
	}
	default:
		break;
	}

	return ret;
}

void mtk_ovl_exdma_dump_golden_setting(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	unsigned long rg0 = 0, rg1 = 0, rg2 = 0, rg3 = 0, rg4 = 0;
	unsigned int value;
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;
	const u32 *reg_fld = exdma->data->reg_fld;

	DDPDUMP("-- %s Golden Setting --\n", mtk_dump_comp_str(comp));

	rg0 = regs[OVL_EXDMA_RDMA0_MEM_GMC_SETTING1];
	rg1 = regs[OVL_EXDMA_RDMA0_FIFO_CTRL];
	rg2 = regs[OVL_EXDMA_RDMA0_MEM_GMC_SETTING2];
	rg3 = regs[OVL_EXDMA_RDMA0_BUF_LOW];
	rg4 = regs[OVL_EXDMA_RDMA0_BUF_HIGH];
	DDPDUMP("0x%03lx:0x%08x 0x%03lx:0x%08x 0x%03lx:0x%08x\n",
		rg0, readl(rg0 + baddr), rg1, readl(rg1 + baddr),
		rg2, readl(rg2 + baddr));
	DDPDUMP("0x%03lx:0x%08x 0x%03lx:0x%08x\n",
		rg3, readl(rg3 + baddr),
		rg4, readl(rg4 + baddr));

	rg0 = regs[OVL_EXDMA_RDMA_BURST_CON1];
	DDPDUMP("0x%03lx:0x%08x\n", rg0, readl(rg0 + baddr));

	rg0 = regs[OVL_EXDMA_RDMA_GREQ_NUM];
	rg1 = regs[OVL_EXDMA_RDMA_GREQ_URG_NUM];
	rg2 = regs[OVL_EXDMA_RDMA_ULTRA_SRC];
	DDPDUMP("0x%03lx:0x%08x 0x%03lx:0x%08x 0x%03lx:0x%08x\n",
		rg0, readl(rg0 + baddr),
		rg1, readl(rg1 + baddr),
		rg2, readl(rg2 + baddr));

	rg0 = regs[OVL_EXDMA_EN_CON];
	rg1 = regs[OVL_EXDMA_DATAPATH_CON];
	rg2 = regs[OVL_EXDMA_FBDC_CFG1];
	DDPDUMP("0x%03lx:0x%08x 0x%03lx:0x%08x 0x%03lx:0x%08x\n",
		rg0, readl(rg0 + baddr),
		rg1, readl(rg1 + baddr),
		rg2, readl(rg2 + baddr));

	value = readl(regs[OVL_EXDMA_RDMA0_MEM_GMC_SETTING1] + baddr);

	DDPDUMP("RDMA0_MEM_GMC_SETTING1\n");
	DDPDUMP("[9:0]:%x [25:16]:%x [28]:%x [31]:%x\n",
		REG_FLD_VAL_GET(reg_fld[FLD_RDMA0_OSTD_ULTRA_TH], value),
		REG_FLD_VAL_GET(
			reg_fld[FLD_RDMA0_OSTD_PREULTRA_TH], value),
		REG_FLD_VAL_GET(
			reg_fld[FLD_RDMA0_ULTRA_THRESHOLD_HIGH_OFS], value),
		REG_FLD_VAL_GET(
			reg_fld[FLD_RDMA0_PRE_ULTRA_THRESHOLD_HIGH_OFS],
			value));

	value = readl(regs[OVL_EXDMA_RDMA0_FIFO_CTRL] + baddr);
	DDPDUMP("RDMA0_FIFO_CTRL\n");
	DDPDUMP("[9:0]:%u [25:16]:%u\n",
		REG_FLD_VAL_GET(reg_fld[FLD_RDMA_FIFO_THRD], value),
		REG_FLD_VAL_GET(reg_fld[FLD_RDMA_FIFO_SIZE], value));

	value = readl(regs[OVL_EXDMA_RDMA0_MEM_GMC_SETTING2] + baddr);
	DDPDUMP("RDMA0_MEM_GMC_SETTING2\n");
	DDPDUMP("[11:0]:%u [27:16]:%u [28]:%u [29]:%u [30]:%u\n",
		REG_FLD_VAL_GET(reg_fld[FLD_RDMA_ISSUE_REQ_THRESHOLD], value),
		REG_FLD_VAL_GET(
			reg_fld[FLD_RDMA_ISSUE_REQ_THRESHOLD_URG], value),
		REG_FLD_VAL_GET(
			reg_fld[FLD_RDMA_REQ_THRESHOLD_PREULTRA], value),
		REG_FLD_VAL_GET(reg_fld[FLD_RDMA_REQ_THRESHOLD_ULTRA], value),
		REG_FLD_VAL_GET(reg_fld[FLD_RDMA_FORCE_REQ_THRESHOLD], value));

	value = readl(regs[OVL_EXDMA_RDMA_BURST_CON1] + baddr);
	DDPDUMP("OVL_RDMA_BURST_CON1\n");
	DDPDUMP("[28]:%u[30]:%u[31]:%u\n",
		REG_FLD_VAL_GET(reg_fld[FLD_BURST16_EN], value),
		REG_FLD_VAL_GET(reg_fld[FLD_DDR_EN], value),
		REG_FLD_VAL_GET(reg_fld[FLD_DDR_ACK_EN], value));

	value = readl(regs[OVL_EXDMA_RDMA_GREQ_NUM] + baddr);
	DDPDUMP("RDMA_GREQ_NUM\n");
	DDPDUMP("[3:0]%u [7:4]%u [11:8]%u [15:12]%u [23:16]%x [26:24]%u\n",
		REG_FLD_VAL_GET(reg_fld[FLD_LAYER0_GREQ_NUM], value),
		REG_FLD_VAL_GET(reg_fld[FLD_LAYER1_GREQ_NUM], value),
		REG_FLD_VAL_GET(reg_fld[FLD_LAYER2_GREQ_NUM], value),
		REG_FLD_VAL_GET(reg_fld[FLD_LAYER3_GREQ_NUM], value),
		REG_FLD_VAL_GET(reg_fld[FLD_OSTD_GREQ_NUM], value),
		REG_FLD_VAL_GET(reg_fld[FLD_GREQ_DIS_CNT], value));
	DDPDUMP("[27]%u [28]%u [29]%u [30]%u [31]%u\n",
		REG_FLD_VAL_GET(reg_fld[FLD_GREQ_STOP_EN], value),
		REG_FLD_VAL_GET(reg_fld[FLD_GRP_END_STOP], value),
		REG_FLD_VAL_GET(reg_fld[FLD_GRP_BRK_STOP], value),
		REG_FLD_VAL_GET(reg_fld[FLD_IOBUF_FLUSH_PREULTRA], value),
		REG_FLD_VAL_GET(reg_fld[FLD_IOBUF_FLUSH_ULTRA], value));

	value = readl(regs[OVL_EXDMA_RDMA_GREQ_URG_NUM] + baddr);
	DDPDUMP("RDMA_GREQ_URG_NUM\n");
	DDPDUMP("[3:0]:%u [7:4]:%u [11:8]:%u [15:12]:%u [25:16]:%u [28]:%u\n",
		REG_FLD_VAL_GET(reg_fld[FLD_LAYER0_GREQ_URG_NUM], value),
		REG_FLD_VAL_GET(reg_fld[FLD_LAYER1_GREQ_URG_NUM], value),
		REG_FLD_VAL_GET(reg_fld[FLD_LAYER2_GREQ_URG_NUM], value),
		REG_FLD_VAL_GET(reg_fld[FLD_LAYER3_GREQ_URG_NUM], value),
		REG_FLD_VAL_GET(reg_fld[FLD_ARG_GREQ_URG_TH], value),
		REG_FLD_VAL_GET(reg_fld[FLD_ARG_URG_BIAS], value));

	value = readl(regs[OVL_EXDMA_RDMA_ULTRA_SRC] + baddr);
	DDPDUMP("RDMA_ULTRA_SRC\n");
	DDPDUMP("[1:0]%u [3:2]%u [5:4]%u [7:6]%u [9:8]%u\n",
		REG_FLD_VAL_GET(reg_fld[FLD_PREULTRA_BUF_SRC], value),
		REG_FLD_VAL_GET(reg_fld[FLD_PREULTRA_SMI_SRC], value),
		REG_FLD_VAL_GET(reg_fld[FLD_PREULTRA_ROI_END_SRC], value),
		REG_FLD_VAL_GET(reg_fld[FLD_PREULTRA_RDMA_SRC], value),
		REG_FLD_VAL_GET(reg_fld[FLD_ULTRA_BUF_SRC], value));
	DDPDUMP("[11:10]%u [13:12]%u [15:14]%u\n",
		REG_FLD_VAL_GET(reg_fld[FLD_ULTRA_SMI_SRC], value),
		REG_FLD_VAL_GET(reg_fld[FLD_ULTRA_ROI_END_SRC], value),
		REG_FLD_VAL_GET(reg_fld[FLD_ULTRA_RDMA_SRC], value));

	value = readl(regs[OVL_EXDMA_RDMA0_BUF_LOW] + baddr);
	DDPDUMP("RDMA0_BUF_LOW\n");
	DDPDUMP("[11:0]:%x [23:12]:%x\n",
		REG_FLD_VAL_GET(reg_fld[FLD_RDMA0_ULTRA_LOW_TH], value),
		REG_FLD_VAL_GET(reg_fld[FLD_RDMA0_PREULTRA_LOW_TH], value));

	value = readl(regs[OVL_EXDMA_RDMA0_BUF_HIGH] + baddr);
	DDPDUMP("RDMA0_BUF_HIGH\n");
	DDPDUMP("[23:12]:%x [31]:%x\n",
		REG_FLD_VAL_GET(reg_fld[FLD_RDMA0_PREULTRA_HIGH_TH], value),
		REG_FLD_VAL_GET(reg_fld[FLD_RDMA0_PREULTRA_HIGH_DIS], value));

	value = readl(regs[OVL_EXDMA_EN] + baddr);
	DDPDUMP("OVL_EN:%d\n", REG_FLD_VAL_GET(reg_fld[FLD_OVL_EN], value));
	DDPDUMP("FORCE_RELAY_MODE:%d\n", REG_FLD_VAL_GET(reg_fld[FLD_FORCE_RELAY_MODE], value));
	value = readl(regs[OVL_EXDMA_EN_CON] + baddr);
	DDPDUMP("[18]:%x [19]:%x [23]:%x\n",
		REG_FLD_VAL_GET(reg_fld[FLD_BLOCK_EXT_ULTRA], value),
		REG_FLD_VAL_GET(reg_fld[FLD_BLOCK_EXT_PREULTRA], value),
		REG_FLD_VAL_GET(reg_fld[FLD_APB_MCYC_RD], value));

	value = readl(regs[OVL_EXDMA_DATAPATH_CON] + baddr);
	DDPDUMP("DATAPATH_CON\n");
	DDPDUMP("[0]:%u [24]:%u [25]:%u [26]:%u [27]:%u [29:28]:%u\n",
		REG_FLD_VAL_GET(reg_fld[FLD_LAYER_SMI_ID_EN], value),
		REG_FLD_VAL_GET(reg_fld[FLD_GCLAST_EN], value),
		REG_FLD_VAL_GET(reg_fld[FLD_HDR_GCLAST_EN], value),
		REG_FLD_VAL_GET(reg_fld[FLD_OUTPUT_CLAMP], value),
		REG_FLD_VAL_GET(reg_fld[FLD_OUTPUT_INTERLACE], value),
		REG_FLD_VAL_GET(reg_fld[FLD_OUTPUT_ROUND], value));

	value = readl(regs[OVL_EXDMA_STASH_CFG0] + baddr);
	DDPDUMP("OVL_STASH_CFG0:0x%08x\n", value);
	value = readl(regs[OVL_EXDMA_STASH_CFG1] + baddr);
	DDPDUMP("OVL_STASH_CFG1:0x%08x\n", value);
	value = readl(regs[OVL_EXDMA_STASH_CFG2] + baddr);
	DDPDUMP("OVL_STASH_CFG2:0x%08x\n", value);

}

int mtk_ovl_exdma_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	int i;
	int offset;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	if (comp->blank_mode)
		return 0;

	DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);

	for (i = 0; i < 0x40; i++) {
		offset = 0x10 * i;
		if (offset == 0x060 || offset == 0x090 || offset == 0x0c0 || offset == 0x0f0)
			continue;
		mtk_serial_dump_reg(baddr, offset, 4);
	}
	mtk_cust_dump_reg(baddr, 0x940, 0x950, 0x954, 0x958);
	mtk_cust_dump_reg(baddr, 0x960, 0x970, 0x974, 0x978);
	mtk_cust_dump_reg(baddr, 0x97c, 0x970, 0x974, 0x978);
	mtk_cust_dump_reg(baddr, 0xAE0, 0xAE4, 0xAE8, 0xAEC);
	mtk_cust_dump_reg(baddr, 0xAF0, 0xAF4, 0xAF8, 0xAFC);
	mtk_cust_dump_reg(baddr, 0xB00, 0xB04, 0xB08, 0xB0C);
	mtk_cust_dump_reg(baddr, 0xF40, 0xF44, 0xF48, 0xF4C);
	mtk_cust_dump_reg(baddr, 0xFF0, 0xFF4, 0xFF8, 0xFFC);

	mtk_ovl_exdma_dump_golden_setting(comp);

	return 0;
}

static void ovl_exdma_printf_status(unsigned int status)
{
	DDPDUMP("- EXDMA_FLOW_CONTROL_DEBUG -\n");
	DDPDUMP("addcon_idle:%d,blend_idle:%d\n",
		(status >> 10) & (0x1), (status >> 11) & (0x1));
	DDPDUMP("out_valid:%d,out_ready:%d,out_idle:%d\n",
		(status >> 12) & (0x1), (status >> 13) & (0x1),
		(status >> 15) & (0x1));
	DDPDUMP("rdma_idle3-0:(%d,%d,%d,%d),rst:%d\n", (status >> 16) & (0x1),
		(status >> 17) & (0x1), (status >> 18) & (0x1),
		(status >> 19) & (0x1), (status >> 20) & (0x1));
	DDPDUMP("trig:%d,frame_hwrst_done:%d\n",
		(status >> 21) & (0x1), (status >> 23) & (0x1));
	DDPDUMP("frame_swrst_done:%d,frame_underrun:%d,frame_done:%d\n",
		(status >> 24) & (0x1), (status >> 25) & (0x1),
		(status >> 26) & (0x1));
	DDPDUMP("ovl_running:%d,ovl_start:%d,ovl_clr:%d\n",
		(status >> 27) & (0x1), (status >> 28) & (0x1),
		(status >> 29) & (0x1));
	DDPDUMP("reg_update:%d,ovl_upd_reg:%d\n",
		(status >> 30) & (0x1),
		(status >> 31) & (0x1));

	DDPDUMP("ovl_fms_state:\n");
	switch (status & 0x3ff) {
	case 0x1:
		DDPDUMP("idle\n");
		break;
	case 0x2:
		DDPDUMP("wait_SOF\n");
		break;
	case 0x4:
		DDPDUMP("prepare\n");
		break;
	case 0x8:
		DDPDUMP("reg_update\n");
		break;
	case 0x10:
		DDPDUMP("eng_clr(internal reset)\n");
		break;
	case 0x20:
		DDPDUMP("eng_act(processing)\n");
		break;
	case 0x40:
		DDPDUMP("h_wait_w_rst\n");
		break;
	case 0x80:
		DDPDUMP("s_wait_w_rst\n");
		break;
	case 0x100:
		DDPDUMP("h_w_rst\n");
		break;
	case 0x200:
		DDPDUMP("s_w_rst\n");
		break;
	default:
		DDPDUMP("ovl_fsm_unknown\n");
		break;
	}
}

static void ovl_print_ovl_rdma_status(unsigned int status)
{
	DDPDUMP("warm_rst_cs:%d,layer_greq:%d,out_data:0x%x\n", status & 0x7,
		(status >> 3) & 0x1, (status >> 4) & 0xffffff);
	DDPDUMP("out_ready:%d,out_valid:%d,smi_busy:%d,smi_greq:%d\n",
		(status >> 28) & 0x1, (status >> 29) & 0x1,
		(status >> 30) & 0x1, (status >> 31) & 0x1);
}

static void ovl_dump_layer_info_compress(struct mtk_ddp_comp *comp, int layer,
					 bool is_ext_layer)
{
	unsigned int compr_en = 0, pitch_msb, pitch;
	void __iomem *baddr = comp->regs;
	dma_addr_t addr;
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;

	if (is_ext_layer) {
		compr_en = DISP_REG_GET_FIELD(
			REG_FLD(1, 4),
			baddr + OVL_EXDMA_ELX_EN(exdma, layer-1));
		addr = read_ext_layer_hdr_addr(comp, layer);
		pitch_msb = readl(baddr + OVL_EXDMA_ELX_PITCH_MSB(exdma, layer-1));
		pitch = readl(baddr + OVL_EXDMA_ELX_HDR_PITCH(exdma, layer-1));
	} else {
		compr_en =
			DISP_REG_GET_FIELD(REG_FLD(1, 4),
					   baddr + regs[OVL_EXDMA_L0_EN]);
		addr = read_phy_layer_hdr_addr(comp, layer);
		pitch_msb = readl(baddr + regs[OVL_EXDMA_L0_PITCH_MSB]);
		pitch = readl(baddr + regs[OVL_EXDMA_L0_HDR_PITCH]);
	}

	if (compr_en == 0) {
		DDPDUMP("compr_en:%u\n", compr_en);
		return;
	}

	DDPDUMP("compr_en:%u, pitch_msb:0x%x, hdr_addr:0x%lx, hdr_pitch:0x%x\n",
		compr_en, pitch_msb, (unsigned long)addr, pitch);
}

static void ovl_dump_layer_info(struct mtk_ddp_comp *comp, int layer,
				bool is_ext_layer)
{
	unsigned int con, lsrc, src_size, offset, pitch, clip;
	/*  enum UNIFIED_COLOR_FMT fmt; */
	void __iomem *baddr = comp->regs;
	void __iomem *Lx_base;
	dma_addr_t addr;
	static const char * const pixel_src[] = { "mem", "color", "ufod", "pq" };
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;
	const u32 *reg_fld = exdma->data->reg_fld;

	Lx_base = baddr;
	if (is_ext_layer) {
		addr = read_ext_layer_addr(comp, layer);

		con = readl(OVL_EXDMA_ELX_CON(exdma, layer) + Lx_base);
		lsrc = readl(OVL_EXDMA_ELX_EN(exdma, layer) + Lx_base);
		offset = readl(OVL_EXDMA_ELX_OFFSET(exdma, layer) + Lx_base);
		src_size = readl(OVL_EXDMA_ELX_SRC_SIZE(exdma, layer) + Lx_base);
		pitch = readl(OVL_EXDMA_ELX_PITCH(exdma, layer) + Lx_base);
		clip = readl(OVL_EXDMA_ELX_CLIP(exdma, layer) + Lx_base);

	} else {
		addr = read_phy_layer_addr(comp, 0);

		con = readl(regs[OVL_EXDMA_L0_CON2] + Lx_base);
		lsrc = readl(regs[OVL_EXDMA_L0_EN] + Lx_base);
		offset = readl(regs[OVL_EXDMA_L0_OFFSET] + Lx_base);
		src_size = readl(regs[OVL_EXDMA_L0_SRC_SIZE] + Lx_base);
		pitch = readl(regs[OVL_EXDMA_L0_PITCH] + Lx_base);
		clip = readl(regs[OVL_EXDMA_L0_CLIP] + Lx_base);
	}

	/* TODO
	 * fmt = display_fmt_reg_to_unified_fmt(
	 * REG_FLD_VAL_GET(L_CON_FLD_CFMT, con),
	 * REG_FLD_VAL_GET(L_CON_FLD_BTSW, con),
	 * REG_FLD_VAL_GET(L_CON_FLD_RGB_SWAP, con));
	 */
	DDPDUMP("%s_L%d:(%u,%u,%ux%u)\n",
		is_ext_layer ? "ext" : "phy", layer, offset & 0xfff,
		(offset >> 16) & 0xfff, src_size & 0xfff,
		(src_size >> 16) & 0xfff);
	DDPDUMP("pitch=%u,addr=0x%lx,source=%s,aen=%u,alpha=%u,cl=0x%x\n",
		pitch & 0xffff,
		(unsigned long)addr, /* unified_color_fmt_name(fmt),*/
		pixel_src[REG_FLD_VAL_GET(reg_fld[FLD_L0_LAYER_SRC], lsrc)],
		REG_FLD_VAL_GET(reg_fld[FLD_ALPHA_EN], con),
		REG_FLD_VAL_GET(reg_fld[FLD_ALPHA], con),
		clip);
	DDPDUMP("L0_SYSRAM_CFG:0x%x, L0_SYSRAM_BUF0:0x%x, L0_SYSRAM_BUF1:0x%x\n",
			readl(comp->regs + 0x880),
			readl(comp->regs + 0x884),
			readl(comp->regs + 0x888));

	ovl_dump_layer_info_compress(comp, layer, is_ext_layer);
}

int mtk_ovl_exdma_analysis(struct mtk_ddp_comp *comp)
{
	int i = 0;
	void __iomem *Lx_base;
	void __iomem *rdma_offset;
	void __iomem *baddr = comp->regs;
	unsigned int src_con;
	unsigned int ext_con, ext_en[3];
	unsigned int path_con, ovl_en;
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;
	const u32 *reg_fld = exdma->data->reg_fld;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	if (comp->blank_mode)
		return 0;

	ovl_en = readl(regs[OVL_EXDMA_EN] + baddr);
	src_con = readl(regs[OVL_EXDMA_L0_EN] + baddr);
	ext_en[0] = readl(OVL_EXDMA_ELX_EN(exdma, 0) + baddr);
	ext_en[1] = readl(OVL_EXDMA_ELX_EN(exdma, 1) + baddr);
	ext_en[2] = readl(OVL_EXDMA_ELX_EN(exdma, 2) + baddr);
	ext_con = readl(regs[OVL_EXDMA_DATAPATH_EXT_CON] + baddr);
	path_con = readl(regs[OVL_EXDMA_DATAPATH_CON] + baddr);

	DDPDUMP("== %s ANALYSIS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	DDPDUMP("ovl_en=%d,l0_en=%d,bg(%dx%d)\n",
		ovl_en & 0x1, src_con & 0x1,
		readl(regs[OVL_EXDMA_ROI_SIZE] + baddr) & 0xfff,
		(readl(regs[OVL_EXDMA_ROI_SIZE] + baddr) >> 16) & 0xfff);
	DDPDUMP("ext_layer:layer_en(%d,%d,%d),attach_layer(%d,%d,%d)\n",
		ext_en[0] & 0x1, ext_en[1] & 0x1, ext_en[2] & 0x1,
		(ext_con >> 16) & 0xf, (ext_con >> 20) & 0xf,
		(ext_con >> 24) & 0xf);
	if (!(ovl_en & 0x1) || (!(src_con & 0x1) && !(ext_en[0] & 0x1) &&
			!(ext_en[1] & 0x1) && !(ext_en[2] & 0x1)))
		return 0;
	DDPDUMP("bg_mode=%s,sta=0x%x\n",
		REG_FLD_VAL_GET(reg_fld[FLD_BGCLR_IN_SEL], path_con) ? "DL" : "const",
		readl(exdma->data->regs[OVL_EXDMA_STA] + baddr));
	DDPDUMP("pq_out_en(%u),sel(%u),pq_out_opt(%u)\n",
		REG_FLD_VAL_GET(reg_fld[FLD_PQ_OUT_EN], path_con),
		REG_FLD_VAL_GET(reg_fld[FLD_PQ_OUT_SEL], path_con),
		REG_FLD_VAL_GET(reg_fld[FLD_PQ_OUT_OPT], path_con));

	/* phy layer */
	for (i = 0; i < mtk_ovl_exdma_layer_num(comp); i++) {
		unsigned int rdma_ctrl;

		if (src_con & 0x1)
			ovl_dump_layer_info(comp, i, false);
		else
			DDPDUMP("phy_L%d:disabled\n", i);

		Lx_base = i * OVL_LAYER_OFFSET + baddr;
		rdma_ctrl = readl(Lx_base + regs[OVL_EXDMA_RDMA0_CTRL]);
		DDPDUMP("ovl rdma%d status:(en=%d,fifo_used:%d,GMC=0x%x)\n", i,
			REG_FLD_VAL_GET(reg_fld[FLD_RDMA_EN], rdma_ctrl),
			REG_FLD_VAL_GET(reg_fld[FLD_RMDA_FIFO_USED_SIZE],
					rdma_ctrl),
			readl(Lx_base + regs[OVL_EXDMA_RDMA0_MEM_GMC_SETTING1]));

		rdma_offset = i * OVL_RDMA_DEBUG_OFFSET + baddr;
		ovl_print_ovl_rdma_status(
			readl(regs[OVL_EXDMA_RDMA0_DBG] + rdma_offset));
	}

	/* ext layer */
	for (i = 0; i < 3; i++) {
		if (ext_en[i] & 0x1)
			ovl_dump_layer_info(comp, i, true);
		else
			DDPDUMP("ext_L%d:disabled\n", i);
	}
	ovl_exdma_printf_status(readl(regs[OVL_EXDMA_FLOW_CTRL_DBG] + baddr));

	return 0;
}

static void mtk_ovl_exdma_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ovl_exdma *priv = dev_get_drvdata(comp->dev);
	int ret;
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	struct mtk_drm_private *dev_priv = NULL;
	const u16 *regs = exdma->data->regs;
	const u32 *reg_fld = exdma->data->reg_fld;

	mtk_ddp_comp_clk_prepare(comp);

	if (priv->fbdc_clk != NULL) {
		ret = clk_prepare_enable(priv->fbdc_clk);
		if (ret)
			DDPPR_ERR("clk prepare enable failed:%s\n",
				mtk_dump_comp_str(comp));
	}

	/* Bypass shadow register and read shadow register */
	if (exdma->data->need_bypass_shadow)
		mtk_ddp_write_mask_cpu(comp, OVL_EXDMA_BYPASS_SHADOW,
			regs[OVL_EXDMA_SHADOW_CTRL],
			REG_FLD_MASK(reg_fld[FLD_BYPASS_SHADOW]));
	else
		mtk_ddp_write_mask_cpu(comp, 0,	regs[OVL_EXDMA_SHADOW_CTRL],
			REG_FLD_MASK(reg_fld[FLD_BYPASS_SHADOW]));

	if (comp->mtk_crtc && comp->mtk_crtc->base.dev) {
		dev_priv = comp->mtk_crtc->base.dev->dev_private;
		if (mtk_drm_helper_get_opt(dev_priv->helper_opt, MTK_DRM_OPT_LAYER_REC))
			writel(0xffffffff, comp->regs + regs[OVL_EXDMA_GDRDY_PRD]);
	}
}

static void mtk_ovl_exdma_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ovl_exdma *priv = dev_get_drvdata(comp->dev);

	if (priv->fbdc_clk != NULL)
		clk_disable_unprepare(priv->fbdc_clk);

	mtk_ddp_comp_clk_unprepare(comp);
}

static void
mtk_ovl_exdma_config_trigger(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkt,
		       enum mtk_ddp_comp_trigger_flag flag)
{
	switch (flag) {
	case MTK_TRIG_FLAG_PRE_TRIGGER:
	{
	//may cause side effect, need check
	//	if (comp->blank_mode)
	//		break;
	//	DDPINFO("%s+ %s\n", __func__, mtk_dump_comp_str(comp));

	//	cmdq_pkt_write(pkt, comp->cmdq_base,
	//			comp->regs_pa + DISP_REG_OVL_RST, 0x1, 0x1);
	//	cmdq_pkt_write(pkt, comp->cmdq_base,
	//			comp->regs_pa + DISP_REG_OVL_RST, 0x0, 0x1);
		break;
	}
#ifdef IF_ZERO /* not ready for dummy register method */
	case MTK_TRIG_FLAG_LAYER_REC:
	{
		u32 offset = 0;
		struct cmdq_pkt_buffer *qbuf;

		int i = 0;
		const int lnr = mtk_ovl_exdma_layer_num(comp);
		u32 ln_con = 0, ln_size = 0;

		struct mtk_drm_private *priv = NULL;

		if (!comp->mtk_crtc)
			return;

		priv = comp->mtk_crtc->base.dev->dev_private;
		if (!mtk_drm_helper_get_opt(priv->helper_opt,
					   MTK_DRM_OPT_LAYER_REC))
			return;

		if (comp->id == DDP_COMPONENT_OVL0_2L)
			offset = DISP_SLOT_LAYER_REC_OVL0_2L;
		else if (comp->id == DDP_COMPONENT_OVL0)
			offset = DISP_SLOT_LAYER_REC_OVL0;
		else
			return;

		qbuf = &comp->mtk_crtc->gce_obj.buf;

		cmdq_pkt_mem_move(pkt, comp->cmdq_base,
				  comp->regs_pa + regs[OVL_EXDMA_GDRDY_PRD_NUM],
				  qbuf->pa_base + offset,
				  CMDQ_THR_SPR_IDX3);

		offset += 4;
		cmdq_pkt_mem_move(pkt, comp->cmdq_base,
				  comp->regs_pa + regs[OVL_EXDMA_SRC_CON],
				  qbuf->pa_base + offset,
				  CMDQ_THR_SPR_IDX3);
		offset += 4;
		cmdq_pkt_mem_move(pkt, comp->cmdq_base,
				  comp->regs_pa + regs[OVL_EXDMA_DATAPATH_CON],
				  qbuf->pa_base + offset,
				  CMDQ_THR_SPR_IDX3);
		offset += 4;
		cmdq_pkt_mem_move(pkt, comp->cmdq_base,
				  comp->regs_pa + regs[OVL_EXDMA_DATAPATH_EXT_CON],
				  qbuf->pa_base + offset,
				  CMDQ_THR_SPR_IDX3);

		for (i = 0; i < lnr + 3; i++) {
			if (i < lnr && !i) {
				ln_con = regs[OVL_EXDMA_L0_CON2];
				ln_size = regs[OVL_EXDMA_L0_SRC_SIZE];
			} else if (i < lnr && i > 0) {
				continue;
			} else {
				ln_con = OVL_EXDMA_ELX_CON(exdma, i - lnr)
				ln_size = OVL_EXDMA_ELX_SRC_SIZE(exdma, i - lnr);
			}

			offset += 0x4;
			cmdq_pkt_mem_move(pkt, comp->cmdq_base,
					  comp->regs_pa + ln_con,
					  qbuf->pa_base + offset,
					  CMDQ_THR_SPR_IDX3);
			offset += 0x4;
			cmdq_pkt_mem_move(pkt, comp->cmdq_base,
					  comp->regs_pa + ln_size,
					  qbuf->pa_base + offset,
					  CMDQ_THR_SPR_IDX3);
		}

		if (comp->id == DDP_COMPONENT_OVL0_2L) {
			if (offset >= DISP_SLOT_LAYER_REC_OVL0)
				DDPMSG("%s:error:ovl0_2l:offset overflow:%u\n",
				       __func__, offset);
		} else if (comp->id == DDP_COMPONENT_OVL0) {
			if (offset >= DISP_SLOT_LAYER_REC_END)
				DDPMSG("%s:error:ovl0:offset overflow:%u\n",
				       __func__, offset);
		}

		break;
	}
#endif
	default:
		break;
	}
}

static int mtk_ovl_exdma_set_partial_update(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle, struct mtk_rect partial_roi, unsigned int enable)
{
	struct mtk_disp_ovl_exdma *exdma = comp_to_ovl_exdma(comp);
	const u16 *regs = exdma->data->regs;
	unsigned int full_height = mtk_crtc_get_height_by_comp(__func__,
						&comp->mtk_crtc->base, comp, false);
	struct total_tile_overhead_v to_v_info;
	unsigned int top_overhead_v, bot_overhead_v;

	DDPDBG("%s, %s set partial update, height:%d, enable:%d\n",
			__func__, mtk_dump_comp_str(comp), partial_roi.height, enable);

	exdma->set_partial_update = enable;

	to_v_info = mtk_crtc_get_total_overhead_v(comp->mtk_crtc);
	top_overhead_v = to_v_info.top_overhead_v;
	bot_overhead_v = to_v_info.bot_overhead_v;

	if (comp->mtk_crtc->res_switch == RES_SWITCH_ON_AP &&
		comp->mtk_crtc->scaling_ctx.scaling_en)
		exdma->roi_height = to_v_info.in_height;
	else
		exdma->roi_height = partial_roi.height + (top_overhead_v + bot_overhead_v);

	DDPDBG("%s, %s overhead_v T:%d overhead_v B:%d, roi_height:%d\n",
			__func__, mtk_dump_comp_str(comp),
			top_overhead_v, bot_overhead_v, exdma->roi_height);

	if (exdma->set_partial_update == MTK_PARTIAL_UPDATE_SISO) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + regs[OVL_EXDMA_ROI_SIZE],
				exdma->roi_height << 16, 0x1fff << 16);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + regs[OVL_EXDMA_ROI_SIZE],
				full_height << 16, 0x1fff << 16);
	}

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_disp_ovl_exdma_funcs = {
	.config = mtk_ovl_exdma_config,
	.first_cfg = mtk_ovl_exdma_config,
	.config_begin = mtk_ovl_exdma_config_begin,
	.start = mtk_ovl_exdma_start,
	.stop = mtk_ovl_exdma_stop,
	.reset = mtk_ovl_exdma_reset,
	.layer_on = mtk_ovl_exdma_layer_on,
	.layer_off = mtk_ovl_exdma_layer_off,
	.layer_config = mtk_ovl_exdma_layer_config,
	.addon_config = mtk_ovl_exdma_addon_config,
	.io_cmd = mtk_ovl_exdma_io_cmd,
	.prepare = mtk_ovl_exdma_prepare,
	.unprepare = mtk_ovl_exdma_unprepare,
	.connect = mtk_ovl_exdma_connect,
	.config_trigger = mtk_ovl_exdma_config_trigger,
	.partial_update = mtk_ovl_exdma_set_partial_update,
};

/* TODO: to be refactored */
int drm_ovl_exdma_tf_cb(int port, unsigned long mva, void *data)
{
	struct mtk_disp_ovl_exdma *exdma = (struct mtk_disp_ovl_exdma *)data;

	DDPINFO("%s tf mva: 0x%lx\n", mtk_dump_comp_str(&exdma->ddp_comp), mva);

	mtk_ovl_exdma_analysis(&exdma->ddp_comp);
	mtk_ovl_exdma_dump(&exdma->ddp_comp);

	return 0;
}

static int mtk_disp_ovl_exdma_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_ovl_exdma *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct mtk_drm_private *private = drm_dev->dev_private;
	int ret;
	unsigned int bg_h, bg_w;
	void __iomem *baddr;
	char buf[50];

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_MMQOS_SUPPORT)) {
		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "qos");
		priv->ddp_comp.qos_req = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "qos_other");
		priv->ddp_comp.qos_req_other = of_mtk_icc_get(dev, buf);
		if (!IS_ERR(priv->ddp_comp.qos_req_other))
			DDPMSG("%s, %s create success, dev:%s\n", __func__, buf, dev_name(dev));

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "fbdc_qos");
		priv->ddp_comp.fbdc_qos_req = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "hrt_qos");
		priv->ddp_comp.hrt_qos_req = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "hdr_qos");
		priv->ddp_comp.hdr_qos_req = of_mtk_icc_get(dev, buf);
		if (!IS_ERR(priv->ddp_comp.hdr_qos_req))
			DDPMSG("%s, %s create success, dev:%s\n", __func__, buf, dev_name(dev));

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "stash_qos");
		priv->ddp_comp.stash_qos_req = of_mtk_icc_get(dev, buf);
		if (!IS_ERR(priv->ddp_comp.stash_qos_req))
			DDPMSG("%s, %s create success, dev:%s\n", __func__, buf, dev_name(dev));

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "hdr_stash_qos");
		priv->ddp_comp.hdr_stash_qos_req = of_mtk_icc_get(dev, buf);
		if (!IS_ERR(priv->ddp_comp.hdr_stash_qos_req))
			DDPMSG("%s, %s create success, dev:%s\n", __func__, buf, dev_name(dev));

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "hrt_qos_other");
		priv->ddp_comp.hrt_qos_req_other = of_mtk_icc_get(dev, buf);
		if (!IS_ERR(priv->ddp_comp.hrt_qos_req_other))
			DDPMSG("%s, %s create success, dev:%s\n", __func__, buf, dev_name(dev));
	}

	baddr = priv->ddp_comp.regs;
	bg_w = readl(priv->data->regs[OVL_EXDMA_ROI_SIZE] + baddr) & 0xfff,
	bg_h = (readl(priv->data->regs[OVL_EXDMA_ROI_SIZE] + baddr) >> 16) & 0xfff,
	_store_bg_roi(&priv->ddp_comp, bg_h, bg_w);

	return 0;
}

static void mtk_disp_ovl_exdma_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_ovl_exdma *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_ovl_exdma_component_ops = {
	.bind = mtk_disp_ovl_exdma_bind, .unbind = mtk_disp_ovl_exdma_unbind,
};

static irqreturn_t mtk_disp_ovl_exdma_irq_handler(int irq, void *dev_id)
{
	struct mtk_disp_ovl_exdma *priv = dev_id;
	struct mtk_ddp_comp *ovl = NULL;
	struct mtk_drm_private *drv_priv = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	unsigned int val = 0;
	unsigned int ret = 0;
	static DEFINE_RATELIMIT_STATE(isr_ratelimit, 1 * HZ, 4);

	if (IS_ERR_OR_NULL(priv))
		return IRQ_NONE;

	ovl = &priv->ddp_comp;
	if (IS_ERR_OR_NULL(ovl))
		return IRQ_NONE;

	if (mtk_drm_top_clk_isr_get(ovl) == false) {
		DDPIRQ("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}

	val = readl(ovl->regs + priv->data->regs[OVL_EXDMA_INTSTA]);

	if (!val) {
		ret = IRQ_NONE;
		goto out;
	}
	DRM_MMP_MARK(IRQ, ovl->regs_pa, val);

	mtk_crtc = ovl->mtk_crtc;
	if (mtk_crtc)
		drv_priv = mtk_crtc->base.dev->dev_private;

	if (val & 0x1e0)
		DRM_MMP_MARK(abnormal_irq, val, ovl->id);

	DDPIRQ("%s irq, val:0x%x\n", mtk_dump_comp_str(ovl), val);

	writel(~val, ovl->regs + priv->data->regs[OVL_EXDMA_INTSTA]);

	if (val & (1 << 0))
		DDPIRQ("[IRQ] %s: reg commit!\n", mtk_dump_comp_str(ovl));
	if (val & (1 << 1))
		DDPIRQ("[IRQ] %s: frame done!\n", mtk_dump_comp_str(ovl));

	if ((ovl->id == DDP_COMPONENT_OVL0_2L) && (val & (1 << 15)))
		DDPIRQ("[IRQ] %s: OVL target line\n", mtk_dump_comp_str(ovl));

	if (val & (1 << 2)) {
		unsigned long long aee_now_ts = sched_clock();

		if (drv_priv && (!atomic_read(&drv_priv->need_recover))) {
			struct mtk_crtc_state *state;

			state = to_mtk_crtc_state(mtk_crtc->base.state);
			if (state) {
				atomic_set(&drv_priv->need_recover, state->lye_state.mml_ir_lye);
				mtk_dprec_snapshot();
			}
		}

		if (__ratelimit(&isr_ratelimit)) {
			unsigned int smi_cnt = 0;

			if (val & (1 << 13))
				smi_cnt = readl(ovl->regs +
				priv->data->regs[OVL_EXDMA_GREQ_LAYER_CNT]);
			DDPPR_ERR("[IRQ] %s: frame underflow! %u reqs are smi hang, cnt=%d\n",
				  mtk_dump_comp_str(ovl), smi_cnt, priv->underflow_cnt);
		}
		priv->underflow_cnt++;
		if (mtk_crtc && (mtk_crtc->last_aee_trigger_ts == 0 ||
			(aee_now_ts - mtk_crtc->last_aee_trigger_ts > TIGGER_INTERVAL_S(10)))) {
			mtk_ovl_exdma_dump(ovl);
			mtk_ovl_exdma_analysis(ovl);
			mtk_crtc->last_aee_trigger_ts = aee_now_ts;
		}
	}
	if (val & (1 << 3))
		DDPIRQ("[IRQ] %s: sw reset done!\n", mtk_dump_comp_str(ovl));

	if (drv_priv && (!mtk_drm_helper_get_opt(drv_priv->helper_opt,
						 MTK_DRM_OPT_COMMIT_NO_WAIT_VBLANK))) {
		mtk_crtc_vblank_irq(&mtk_crtc->base);
	}

	ret = IRQ_HANDLED;

out:
	mtk_drm_top_clk_isr_put(ovl);

	return ret;
}

static int mtk_disp_ovl_exdma_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_ovl_exdma *priv;
	enum mtk_ddp_comp_id comp_id;
	int irq, num_irqs;
	int ret, len;
	const __be32 *ranges = NULL;

	DDPINFO("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;


	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_OVL_EXDMA);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	DDPINFO("%s comp_id:%d\n", __func__, comp_id);

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_ovl_exdma_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}
	priv->fbdc_clk = of_clk_get(dev->of_node, 1);
	if (IS_ERR(priv->fbdc_clk))
		priv->fbdc_clk = NULL;

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	ranges = of_get_property(dev->of_node, "dma-ranges", &len);
	if (ranges && priv->data && priv->data->is_support_34bits)
		ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));

	writel(0, priv->ddp_comp.regs + priv->data->regs[OVL_EXDMA_INTSTA]);
	writel(0, priv->ddp_comp.regs + priv->data->regs[OVL_EXDMA_INTEN]);

	num_irqs = platform_irq_count(pdev);
	if (num_irqs) {
		irq = platform_get_irq(pdev, 0);

		if (irq < 0)
			return irq;

		ret = devm_request_irq(dev, irq, mtk_disp_ovl_exdma_irq_handler,
						   IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev),
						   priv);

	}

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_ovl_exdma_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);
	return ret;
}

static void mtk_disp_ovl_exdma_remove(struct platform_device *pdev)
{
	struct mtk_disp_ovl_exdma *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_ovl_exdma_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);
}

static const struct exdma_compress_info compr_info_mt6991 = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_ovl_exdma_l_config_AFBC_V1_2,
};

static const struct mtk_disp_ovl_exdma_data mt6991_ovl_exdma_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x10,
	.el_hdr_addr_offset = 0x10,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U,
	.fmt_yuyv = 5U,
	.compr_info = &compr_info_mt6991,
	.support_shadow = false,
	.need_bypass_shadow = true,
	.skip_larb_con = true,
	.preultra_th_dc = 0x3ff,
	.fifo_size = 1536,
	.issue_req_th_dl = 511,
	.issue_req_th_dc = 31,
	.issue_req_th_urg_dl = 255,
	.issue_req_th_urg_dc = 31,
	.greq_num_dl = 0xFFFF,
	.stash_en = 0,
	.stash_cfg = 0x1,
	.is_support_34bits = true,
	.aid_per_layer_setting = true,
	.mmsys_mapping = &mtk_ovl_mmsys_mapping_MT6991,
	.source_bpc = 10,
	//.is_right_ovl_comp = &is_right_ovl_comp_MT6985,
	.ovlsys_mapping = &mtk_ovl_sys_mapping_MT6991,
	.ovl_phy_mapping = &mtk_ovl_phy_mapping_MT6991,
	.ovl_ch_mapping = &mtk_ovl_phy_channel_mapping_MT6991,
	.regs = ovl_exdma_regs_mt6991,
	.reg_fld = ovl_exdma_fld_mt6991,
	.apb_mcyc_rd = 1,
	.output_interlace = 1,
	.output_round = 0,
	.lead_time = 200,
	.stash_min_ostdl = 4,
	.afbc_header_min_ostdl = 5,
	.ovl_fifo_depth = 1536,
	.ovl_hdr_fifo_depth = 320,
};

static const struct exdma_compress_info compr_info_mt6993 = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_ovl_exdma_l_config_AFBC_V1_2,
};

static const struct mtk_disp_ovl_exdma_data mt6993_ovl_exdma_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x10,
	.el_hdr_addr_offset = 0x10,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U,
	.fmt_yuyv = 5U,
	.compr_info = &compr_info_mt6993,
	.support_shadow = false,
	.need_bypass_shadow = true,
	.skip_larb_con = true,
	.preultra_th_dc = 0x3ff,
	.fifo_size = 1536,
	.issue_req_th_dl = 511,
	.issue_req_th_dc = 31,
	.issue_req_th_urg_dl = 255,
	.issue_req_th_urg_dc = 31,
	.greq_num_dl = 0x00FF,
	.stash_en = 0,
	.stash_cfg = 0x1,
	.is_support_34bits = true,
	.aid_per_layer_setting = true,
	.mmsys_mapping = &mtk_ovl_mmsys_mapping_MT6993,
	.source_bpc = 10,
	//.is_right_ovl_comp = &is_right_ovl_comp_MT6985,
	.ovlsys_mapping = &mtk_ovl_sys_mapping_MT6993,
	.ovl_phy_mapping = &mtk_ovl_phy_mapping_MT6993,
	.ovl_ch_mapping = &mtk_ovl_phy_channel_mapping_MT6993,
	.regs = ovl_exdma_regs_mt6993,
	.reg_fld = ovl_exdma_fld_mt6991,
	.apb_mcyc_rd = 0,
	.output_interlace = 0,
	.output_round = 0x2,
	.lead_time = 200,
	.stash_min_ostdl = 4,
	.afbc_header_min_ostdl = 5,
	.ovl_fifo_depth = 1536,
	.ovl_hdr_fifo_depth = 320,
};


static const struct of_device_id mtk_disp_ovl_exdma_driver_dt_match[] = {
	{.compatible = "mediatek,mt6991-disp-ovl-exdma",
	 .data = &mt6991_ovl_exdma_driver_data},
	{.compatible = "mediatek,mt6993-disp-ovl-exdma",
	 .data = &mt6993_ovl_exdma_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_ovl_exdma_driver_dt_match);

struct platform_driver mtk_ovl_exdma_driver = {
	.probe = mtk_disp_ovl_exdma_probe,
	.remove = mtk_disp_ovl_exdma_remove,
	.driver = {

			.name = "mediatek-disp-ovl-exdma",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_ovl_exdma_driver_dt_match,
		},
};
