/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Jie Qiu <jie.qiu@mediatek.com>
 */
#ifndef __MTK_DPI_REGS_H
#define __MTK_DPI_REGS_H

#define DVO_EN			0x00
#define EN				BIT(0)
#define DVO_FORCE_ON	BIT(4)
#define LINK_OFF		BIT(8)



#define DVO_RET			0x04
#define SWRST				BIT(0)
#define SWRST_SEL				BIT(4)

#define DVO_INTEN		0x08
#define INT_VFP_START_EN			BIT(0)
#define INT_VSYNC_START_EN			BIT(1)
#define INT_VSYNC_END_EN		BIT(2)
#define INT_VDE_START_EN			BIT(3)
#define INT_VDE_END_EN		BIT(4)
#define INT_WR_INFOQ_REG_EN			BIT(5)
#define INT_TARGET_LINE0_EN		BIT(6)
#define INT_TARGET_LINE1_EN			BIT(7)
#define INT_TARGET_LINE2_EN		BIT(8)
#define INT_TARGET_LINE3_EN			BIT(9)
#define INT_WR_INFOQ_START_EN		BIT(10)
#define INT_WR_INFOQ_END_EN			BIT(11)
#define EXT_VSYNC_START_EN		BIT(12)
#define EXT_VSYNC_END_EN			BIT(13)
#define EXT_VDE_START_EN		BIT(14)
#define EXT_VDE_END_EN			BIT(15)
#define EXT_VBLANK_END_EN		BIT(16)
#define UNDERFLOW_EN			BIT(17)
#define INFOQ_ABORT_EN		BIT(18)

#define DVO_INTSTA		0x0C
#define INT_VFP_START_STA			BIT(0)
#define INT_VSYNC_START_STA			BIT(1)
#define INT_VSYNC_END_STA		BIT(2)
#define INT_VDE_START_STA			BIT(3)
#define INT_VDE_END_STA		BIT(4)
#define INT_WR_INFOQ_REG_STA			BIT(5)
#define INT_TARGET_LINE0_STA		BIT(6)
#define INT_TARGET_LINE1_STA		BIT(7)
#define INT_TARGET_LINE2_STA		BIT(8)
#define INT_TARGET_LINE3_STA			BIT(9)
#define INT_WR_INFOQ_START_STA		BIT(10)
#define INT_WR_INFOQ_END_STA			BIT(11)
#define EXT_VSYNC_START_STA		BIT(12)
#define EXT_VSYNC_END_STA			BIT(13)
#define EXT_VDE_START_STA		BIT(14)
#define EXT_VDE_END_STA		BIT(15)
#define EXT_VBLANK_END_STA		BIT(16)
#define INT_UNDERFLOW_STA			BIT(17)
#define INFOQ_ABORT_STA			BIT(18)

#define DVO_CON			0x10
#define INTL_EN				BIT(0)

#define DVO_OUTPUT_SET	0x18
#define DVO_OUT_1T1P_SEL    0x0
#define DVO_OUT_1T2P_SEL    0x1
#define DVO_OUT_1T4P_SEL    0x2
#define OUT_NP_SEL_MASK     0x3

#define BIT_SWAP			BIT(4)
#define CH_SWAP_MASK			(0x7 << 5)
#define CH_SWAP_SHIFT		0x5
#define SWAP_RGB			0x00
#define SWAP_GBR			0x01
#define SWAP_BRG			0x02
#define SWAP_RBG			0x03
#define SWAP_GRB			0x04
#define SWAP_BGR			0x05
#define PXL_SWAP			BIT(8)
#define R_MASK				BIT(12)
#define G_MASK				BIT(13)
#define B_MASK				BIT(14)
#define DE_MASK				BIT(16)
#define HS_MASK				BIT(17)
#define VS_MASK				BIT(18)
#define HS_INV				BIT(19)
#define VS_INV			BIT(20)
#define DE_POL              BIT(19)
#define HSYNC_POL			BIT(20)
#define VSYNC_POL			BIT(21)
#define CK_POL              BIT(22)

#define DVO_SRC_SIZE		0x20
#define SRC_HSIZE				0
#define SRC_HSIZE_MASK		(0xFFFF << 0)
#define SRC_VSIZE				16
#define SRC_VSIZE_MASK		(0xFFFF << 16)

#define DVO_PIC_SIZE		0x24
#define PIC_HSIZE				0
#define PIC_HSIZE_MASK		(0xFFFF << 0)
#define PIC_VSIZE				16
#define PIC_VSIZE_MASK		(0xFFFF << 16)

#define DVO_TGEN_H0		0x50
#define HFP				0
#define HFP_MASK			(0xFFFF << 0)
#define HSYNC				16
#define HSYNC_MASK			(0xFFFF << 16)

#define DVO_TGEN_H1		0x54
#define HSYNC2ACT				0
#define HSYNC2ACT_MASK			(0xFFFF << 0)
#define HACT				16
#define HACT_MASK			(0xFFFF << 16)

#define DVO_TGEN_V0		0x58
#define VFP				0
#define VFP_MASK			(0xFFFF << 0)
#define VSYNC				16
#define VSYNC_MASK			(0xFFFF << 16)

#define DVO_TGEN_V1		0x5C
#define VSYNC2ACT				0
#define VSYNC2ACT_MASK			(0xFFFF << 0)
#define VACT				16
#define VACT_MASK			(0xFFFF << 16)



#define DVO_TGEN_INFOQ_LATENCY		0x80
#define INFOQ_START_LATENCY					0
#define INFOQ_START_LATENCY_MASK			(0xFFFF << 0)
#define INFOQ_END_LATENCY				16
#define INFOQ_END_LATENCY_MASK			(0xFFFF << 16)

#define DVO_MATRIX_SET      0x140
#define CSC_EN              BIT(0)
#define MATRIX_SEL_RGB_TO_JPEG      (0x0 << 4)
#define MATRIX_SEL_RGB_TO_BT601     (0x2 << 4)
#define MATRIX_SEL_RGB_TO_BT709     (0x3 << 4)
#define INT_MTX_SEL         (0x2 << 4)
#define INT_MTX_SEL_MASK            GENMASK(8, 4)

#define DVO_YUV422_SET		0x170
#define YUV422_EN           BIT(0)
#define VYU_MAP             BIT(8)

#define DVO_BUF_CON0		0x220
#define DISP_BUF_EN			BIT(0)
#define FIFO_UNDERFLOW_DONE_BLOCK				BIT(4)


#define DVO_TGEN_V_LAST_TRAILING_BLANK		0x6c
#define V_LAST_TRAILING_BLANK					0
#define V_LAST_TRAILING_BLANK_MASK			(0xFFFF << 0)


#define DVO_TGEN_OUTPUT_DELAY_LINE		0x7c
#define EXT_TG_DLY_LINE					0
#define EXT_TG_DLY_LINE_MASK			(0xFFFF << 0)

#define DVO_PATTERN_CTRL		0x100
#define PRE_PAT_EN			BIT(0)
#define PRE_PAT_SEL_MASK			(0x7 << 4)
#define COLOR_BAR			(0x4<<4)
#define PRE_PAT_FORCE_ON			BIT(8)

#define DVO_PATTERN_COLOR		0x104
#define PAT_R			(0x3FF << 0)
#define PAT_G			(0x3FF << 10)
#define PAT_B			(0x3FF << 20)

#define DVO_SHADOW_CTRL		0x190
#define FORCE_COMMIT			BIT(0)
#define BYPASS_SHADOW			BIT(1)
#define READ_WRK_REG			BIT(2)

#define DVO_SIZE		0x18
#define DVO_TGEN_VWIDTH		0x28
#define DVO_TGEN_VPORCH		0x2C
#define DVO_TGEN_HPORCH		0x24
#define DVO_TGEN_HWIDTH		0x20

#define DVO_BUF_SODI_HIGHT		0x230
#define DVO_BUF_SODI_LOW		0x234
#define DVO_DISP_BUF_MASK		0xFFFFFFFF

#define DVO_STATUS			0xE00

#endif /* __MTK_DPI_REGS_H */
