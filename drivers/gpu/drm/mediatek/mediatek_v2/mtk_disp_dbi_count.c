// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/ratelimit.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/dma-buf.h>
#include <linux/dma-direction.h>
#include <linux/dma-heap.h>
#include <linux/vmalloc.h>
#include <linux/of_platform.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_drv.h"
#include "mtk_fence.h"
#include "mtk_sync.h"
#include "mtk_debug.h"
#include "mtk_log.h"

#include "mtk_disp_dbi_count.h"

#define DISP_DBI_COUNT_TOP_SHADOW_CTRL (0x0004)
	#define BYPASS_SHADOW REG_FLD_MSB_LSB(0, 0)
	#define FORCE_COMMIT REG_FLD_MSB_LSB(1, 1)
	#define READ_WRK_REG REG_FLD_MSB_LSB(2, 2)
	#define SR2WRK_CG_ON REG_FLD_MSB_LSB(3, 3)
#define REG_DBI_FRAME_DROP_BLOCK_FUNC (0x0008)
	#define FRAME_DROP_BLOCK_FUNC REG_FLD_MSB_LSB(0, 0)
#define REG_DBI_SAMPLING_PQ_SINGLE_TRIGGER_SW_EN 0x000C
	#define SAMPLING_PQ_SINGLE_TRIGGER_SW_EN REG_FLD_MSB_LSB(0, 0)
	#define SAMPLING_PQ_SINGLE_TRIGGER REG_FLD_MSB_LSB(4, 4)
#define REG_DBI_SAMPLING_HDROP_EN 0x0024
	#define SAMPLING_HDROP_EN REG_FLD_MSB_LSB(0, 0)
	#define SAMPLING_HDROP_COUNT REG_FLD_MSB_LSB(7, 4)
	#define SAMPLING_HDROP_IDX_SW_EN REG_FLD_MSB_LSB(8, 8)
	#define SAMPLING_HDROP_IDX_SW REG_FLD_MSB_LSB(15, 12)
#define REG_DBI_SAMPLING_HCROP_EN 0x0028
	#define SAMPLING_HCROP_EN REG_FLD_MSB_LSB(0, 0)
	#define SAMPLING_HCROP_COUNT REG_FLD_MSB_LSB(7, 4)
	#define SAMPLING_HCROP_IDX_SW_EN REG_FLD_MSB_LSB(8, 8)
	#define SAMPLING_HCROP_IDX_SW REG_FLD_MSB_LSB(15, 12)
#define REG_DBI_SAMPLING_HCROP_SLICE_WIDTH 0x002C
	#define SAMPLING_HCROP_SLICE_WIDTH REG_FLD_MSB_LSB(12, 0)
#define REG_DBI_SAMPLING_VDROP_EN 0x0030
	#define SAMPLING_VDROP_EN REG_FLD_MSB_LSB(0, 0)
	#define SAMPLING_VDROP_COUNT REG_FLD_MSB_LSB(7, 4)
	#define SAMPLING_VDROP_IDX_SW_EN REG_FLD_MSB_LSB(8, 8)
	#define SAMPLING_VDROP_IDX_SW REG_FLD_MSB_LSB(15, 12)
#define REG_DBI_COUNTING_HW_ENABLE 0x0034
	#define COUNTING_HW_ENABLE REG_FLD_MSB_LSB(0, 0)
	#define COUNTING_PQ_ENABLE REG_FLD_MSB_LSB(4, 4)
	#define COUNTING_RESET_TIMESTAMP REG_FLD_MSB_LSB(8, 8)
	#define COUNTING_RESET_INDEX REG_FLD_MSB_LSB(12, 12)
#define REG_DBI_COUNTING_SW_TIMEDIFF_EN 0x0038
	#define COUNTING_SW_TIMEDIFF_EN REG_FLD_MSB_LSB(0, 0)
	#define COUNTING_SW_TIMEDIFF REG_FLD_MSB_LSB(23, 8)
#define REG_DBI_COUNTING_SW_SYS_TIME_EN 0x003c
	#define COUNTING_SW_SYS_TIME_EN REG_FLD_MSB_LSB(0, 0)
	#define COUNTING_SW_SYS_CLOCK REG_FLD_MSB_LSB(23, 8)
#define REG_DBI_COUNTING_SW_IDX_EN 0x0040
	#define COUNTING_SW_IDX_EN REG_FLD_MSB_LSB(0, 0)
	#define COUNTING_SW_IDX REG_FLD_MSB_LSB(10, 4)
#define REG_DBI_COUNTING_GAIN_R (0x0710)
	#define COUNTING_GAIN_R REG_FLD_MSB_LSB(15, 0)
#define REG_DBI_COUNTING_GAIN_G (0x0714)
	#define COUNTING_GAIN_G REG_FLD_MSB_LSB(15, 0)
#define REG_DBI_COUNTING_GAIN_B (0x0718)
	#define COUNTING_GAIN_B REG_FLD_MSB_LSB(15, 0)
#define REG_DBI_COUNTING_SH1 (0x0728)
	#define DBI_COUNTING_SH1 REG_FLD_MSB_LSB(2, 0)
	#define DBI_COUNTING_SH2 REG_FLD_MSB_LSB(4, 7)

#define REG_DBI_COUNTING_MODE (0x0730)
	#define COUNTING_MODE REG_FLD_MSB_LSB(0, 0)
	#define COUNTING_SLICE_MODE REG_FLD_MSB_LSB(5, 4)
	#define COUNTING_SLICE_NUM REG_FLD_MSB_LSB(14, 8)

#define DISP_DBI_COUNT_TOP_CTR_1 (0x0748)
	#define REG_VS_RE_GEN_CYC REG_FLD_MSB_LSB(7, 0)
#define DISP_DBI_COUNT_TOP_CTR_2 (0x074c)
	#define REG_VS_RE_GEN REG_FLD_MSB_LSB(0, 0)
#define DISP_DBI_COUNT_TOP_CTR_3 (0x0750)
	#define REG_DBI_COUNT_BYPASS REG_FLD_MSB_LSB(0, 0)
	#define REG_FORCE_IN_DONE REG_FLD_MSB_LSB(1, 1)
	#define REG_FORCE_OUT_DONE REG_FLD_MSB_LSB(2, 2)
	#define REG_FORCE_UDMA_W_DONE REG_FLD_MSB_LSB(3, 3)
	#define REG_EOF_SEL REG_FLD_MSB_LSB(6, 6)
	#define REG_EOF_USER REG_FLD_MSB_LSB(7, 7)
	#define REG_DBI_COUNT_TOP_CLK_GATING_DB_EN REG_FLD_MSB_LSB(14, 14)
	#define REG_DBI_COUNT_TOP_CLK_FORCE_EN REG_FLD_MSB_LSB(15, 15)
#define DISP_DBI_COUNT_TOP_CTR_4 (0x0754)
	#define REG_DBI_COUNT_SW_RST REG_FLD_MSB_LSB(0, 0)

#define DISP_DBI_COUNT_IRQ_MASK (0x075C)
#define DISP_DBI_COUNT_IRQ_RAW_STATUS (0x0760)
#define DISP_DBI_COUNT_IRQ_STATUS (0x0764)
#define DISP_DBI_COUNT_IRQ_CLR (0x0768)
	#define DBI_COUNT_INT_DONE BIT(11)
	#define DBI_COUNT_EOF BIT(6)
	#define DBI_COUNT_FRAME_DONE BIT(0)

#define DISP_DBI_COUNT_SMI_SB_FLG_DBIR (0x76c)
	#define REG_DBIR_PREULTRA_RE_ULTRA_MASK REG_FLD_MSB_LSB(0, 0)
	#define REG_DBIR_PREULTRA_RE_ULTRA_FRCE REG_FLD_MSB_LSB(1, 1)
	#define REG_DBIR_ULTRA_RE_MASK REG_FLD_MSB_LSB(2, 2)
	#define REG_DBIR_ULTRA_RE_FRCE REG_FLD_MSB_LSB(3, 3)
	#define REG_DBIR_RE_ULTRA_MODE REG_FLD_MSB_LSB(11, 8)
	#define REG_DBIR_STASH_PREULTRA_RE_ULTRA_MASK REG_FLD_MSB_LSB(16, 16)
	#define REG_DBIR_STASH_PREULTRA_RE_ULTRA_FRCE REG_FLD_MSB_LSB(17, 17)
	#define REG_DBIR_STASH_ULTRA_RE_MASK REG_FLD_MSB_LSB(18, 18)
	#define REG_DBIR_STASH_ULTRA_RE_FRCE REG_FLD_MSB_LSB(19, 19)
	#define REG_DBIR_STASH_RE_ULTRA_MODE REG_FLD_MSB_LSB(27, 24)
#define DISP_DBI_COUNT_SMI_SB_FLG_DBIW (0x770)
	#define REG_DBIW_PREULTRA_WR_ULTRA_MASK REG_FLD_MSB_LSB(0, 0)
	#define REG_DBIW_PREULTRA_WR_ULTRA_FRCE REG_FLD_MSB_LSB(1, 1)
	#define REG_DBIW_ULTRA_WR_MASK REG_FLD_MSB_LSB(2, 2)
	#define REG_DBIW_ULTRA_WR_FRCE REG_FLD_MSB_LSB(3, 3)
	#define REG_DBIW_WR_ULTRA_MODE REG_FLD_MSB_LSB(11, 8)
	#define REG_DBIW_STASH_PREULTRA_WR_ULTRA_MASK REG_FLD_MSB_LSB(16, 16)
	#define REG_DBIW_STASH_PREULTRA_WR_ULTRA_FRCE REG_FLD_MSB_LSB(17, 17)
	#define REG_DBIW_STASH_ULTRA_WR_MASK REG_FLD_MSB_LSB(18, 18)
	#define REG_DBIW_STASH_ULTRA_WR_FRCE REG_FLD_MSB_LSB(19, 19)
	#define REG_DBIW_STASH_WR_ULTRA_MODE REG_FLD_MSB_LSB(27, 24)
#define DISP_DBI_COUNT_GUSR_CTRL_DBIR (0x774)
	#define REG_DBIR_GUSER_CTRL REG_FLD_MSB_LSB(31, 0)
#define DISP_DBI_COUNT_GUSR_CTRL_DBIW (0x778)
	#define REG_DBIW_GUSER_CTRL REG_FLD_MSB_LSB(31, 0)
#define DISP_DBI_COUNT_DDREN_CTRL_DBIW (0x77c)
	#define REG_DBIW_DDREN_REQ_DISABLE REG_FLD_MSB_LSB(0, 0)
	#define REG_DBIW_USE_HRT_DDREN_REQ REG_FLD_MSB_LSB(1, 1)
	#define REG_DBIW_SW_DDREN_REQ REG_FLD_MSB_LSB(2, 2)
	#define REG_DBIW_DDREN_REQ_SW_MODE_EN REG_FLD_MSB_LSB(3, 3)
	#define REG_DBIW_DDREN_SMI_RESET REG_FLD_MSB_LSB(4, 4)
	#define REG_DBIW_SRT_DDREN_REQ REG_FLD_MSB_LSB(5, 5)
	#define REG_DBIW_SMI_ACTIVE_PROT_DISABLE REG_FLD_MSB_LSB(6, 6)
	#define REG_DBIW_DDREN_REQ_DEBUG REG_FLD_MSB_LSB(15, 8)
	#define REG_DBIW_STASH_DDREN_REQ_DISABLE REG_FLD_MSB_LSB(16, 16)
	#define REG_DBIW_STASH_USE_HRT_DDREN_REQ REG_FLD_MSB_LSB(17, 17)
	#define REG_DBIW_STASH_SW_DDREN_REQ REG_FLD_MSB_LSB(18, 18)
	#define REG_DBIW_STASH_DDREN_SW_MODE_EN REG_FLD_MSB_LSB(19, 19)
	#define REG_DBIW_STASH_DDREN_SMI_RESET REG_FLD_MSB_LSB(20, 20)
	#define REG_DBIW_STASH_SRT_DDREN_REQ REG_FLD_MSB_LSB(21, 21)
	#define REG_DBIW_STASH_DDREN_REQ_DEBUG REG_FLD_MSB_LSB(31, 24)
#define DISP_DBI_COUNT_DDREN_CTRL_DBIR (0x780)
	#define REG_DBIR_DDREN_REQ_DISABLE REG_FLD_MSB_LSB(0, 0)
	#define REG_DBIR_USE_HRT_DDREN_REQ REG_FLD_MSB_LSB(1, 1)
	#define REG_DBIR_SW_DDREN_REQ REG_FLD_MSB_LSB(2, 2)
	#define REG_DBIR_DDREN_REQ_SW_MODE_EN REG_FLD_MSB_LSB(3, 3)
	#define REG_DBIR_DDREN_SMI_RESET REG_FLD_MSB_LSB(4, 4)
	#define REG_DBIR_SRT_DDREN_REQ REG_FLD_MSB_LSB(5, 5)
	#define REG_DBIR_SMI_ACTIVE_PROT_DISABLE REG_FLD_MSB_LSB(6, 6)
	#define REG_DBIR_DDREN_REQ_DEBUG REG_FLD_MSB_LSB(15, 8)
	#define REG_DBIR_STASH_DDREN_REQ_DISABLE REG_FLD_MSB_LSB(16, 16)
	#define REG_DBIR_STASH_USE_HRT_DDREN_REQ REG_FLD_MSB_LSB(17, 17)
	#define REG_DBIR_STASH_SW_DDREN_REQ REG_FLD_MSB_LSB(18, 18)
	#define REG_DBIR_STASH_DDREN_SW_MODE_EN REG_FLD_MSB_LSB(19, 19)
	#define REG_DBIR_STASH_DDREN_SMI_RESET REG_FLD_MSB_LSB(20, 20)
	#define REG_DBIR_STASH_SRT_DDREN_REQ REG_FLD_MSB_LSB(21, 21)
	#define REG_DBIR_STASH_DDREN_REQ_DEBUG REG_FLD_MSB_LSB(31, 24)
#define REG_DBI_COUNT_FRAME_WIDTH (0x784)
	#define COUNT_FRAME_WIDTH REG_FLD_MSB_LSB(12, 0)
#define REG_DBI_COUNT_FRAME_HEIGHT (0x788)
	#define COUNT_FRAME_HEIGHT REG_FLD_MSB_LSB(12, 0)
#define REG_DBI_COUNT_OUTP_IN_HSIZE (0x78c)
	#define COUNT_OUTP_IN_HSIZE REG_FLD_MSB_LSB(12, 0)
#define REG_DBI_COUNT_OUTP_IN_VSIZE (0x790)
	#define COUNT_OUTP_IN_VSIZE REG_FLD_MSB_LSB(12, 0)
#define REG_DBI_COUNT_OUTP_OUT_HSIZE (0x794)
	#define COUNT_OUTP_OUT_HSIZE REG_FLD_MSB_LSB(12, 0)
#define REG_DBI_COUNT_OUTP_OUT_VSIZE (0x798)
	#define COUNT_OUTP_OUT_VSIZE REG_FLD_MSB_LSB(12, 0)
#define REG_DBI_COUNT_REAL_FRAME_WIDTH (0x7a4)
	#define COUNT_REAL_FRAME_WIDTH REG_FLD_MSB_LSB(12, 0)
#define REG_DBI_COUNT_REAL_FRAME_HEIGHT (0x7a8)
	#define COUNT_REAL_FRAME_HEIGHT REG_FLD_MSB_LSB(12, 0)
#define REG_DBIR_UDMA_AUTO_CLK_DB_EN (0x7ac)
	#define DBIR_UDMA_AUTO_CLK_DB_EN REG_FLD_MSB_LSB(1, 0)
	#define DBIR_UDMA_AUTO_CLK_EN REG_FLD_MSB_LSB(3, 2)
	#define DBIR_UDMA_AUTO_CLK_SW_MODE_EN REG_FLD_MSB_LSB(5, 4)
	#define DBIW_UDMA_AUTO_CLK_DB_EN REG_FLD_MSB_LSB(9, 8)
	#define DBIW_UDMA_AUTO_CLK_EN REG_FLD_MSB_LSB(11, 10)
	#define DBIW_UDMA_AUTO_CLK_SW_MODE_EN REG_FLD_MSB_LSB(13, 12)
#define REG_DBIR_UDMA_BASE_ADDR (0x7b0)
	#define DBIR_UDMA_BASE_ADDR REG_FLD_MSB_LSB(31, 0)
#define REG_DBIW_UDMA_BASE_ADDR (0x7b4)
	#define DBIW_UDMW_BASE_ADDR REG_FLD_MSB_LSB(31, 0)
#define REG_DBI_COUNT_OUTP_EN (0x7b8)
	#define COUNT_OUTP_EN REG_FLD_MSB_LSB(0, 0)
	#define COUNT_OUTP_RST REG_FLD_MSB_LSB(1, 1)
	#define COUNT_OUTP_1TNP REG_FLD_MSB_LSB(3, 2)
	#define COUNT_INP_EN REG_FLD_MSB_LSB(4, 4)
	#define COUNT_INP_RST REG_FLD_MSB_LSB(5, 5)
	#define COUNT_INP_1TNP REG_FLD_MSB_LSB(7, 6)
	#define COUNT_OUTP_STATUS_CLEAR REG_FLD_MSB_LSB(8, 8)
	#define COUNT_INP_STATUS_CLEAR REG_FLD_MSB_LSB(9, 9)
	#define COUNT_ENG_STATUS_CLEAR REG_FLD_MSB_LSB(10, 10)
#define REG_DBI_COUNT_OUTP_OUT_HOFFSET (0x7bc)
#define REG_DBI_COUNT_OUTP_OF_END (0x7c0)
#define REG_DBI_COUNT_OUTP_IN_PIX_CNT (0x7c4)
#define REG_DBI_COUNT_OUTP_OUT_LINE_CNT (0x7c8)
#define REG_DBI_COUNT_OUTP_OUT_PIX_CNT (0x7cc)
#define REG_DBI_COUNT_INP_IF_END (0x7d0)
#define REG_DBI_COUNT_INP_IN_PIX_CNT (0x7d4)
#define REG_DBI_COUNT_RESERVE (0x7d8)
#define REG_DBI_SAMPLING_BYPASS (0x7e0)
#define REG_DBI_COUNTING_BYPASS (0x7e4)
#define REG_DBI_COUNT_ALL_AUTO_CLK_EN (0x7e8)
#define REG_DBI_COUNT_UDMA_W_EN (0x7ec)
	#define COUNT_UDMA_W_EN REG_FLD_MSB_LSB(0, 0)
	#define COUNT_UDMA_R_EN REG_FLD_MSB_LSB(4, 4)
#define REG_DBI_COUNT_PATH_CONTROL (0x7f0)
#define REG_SRAM_SW_TRIG (0x7f4)
#define REG_SRAM_SW_DO (0x7f8)
#define REG_DBI_COUNT_DISABLE_BIT_MASK (0x804)
#define REG_DBI_COUNT_EMI_SIDE_BAND (0x808)
	#define REG_CNTR_ULTRA_MODE REG_FLD_MSB_LSB(11, 8)
	#define REG_CNTW_ULTRA_MODE REG_FLD_MSB_LSB(27, 24)
	#define REG_CNTR_STASH_ULTRA_FRCE REG_FLD_MSB_LSB(6, 6)
	#define REG_CNTW_STASH_ULTRA_FRCE REG_FLD_MSB_LSB(22, 22)

#define REG_DBI_GATING (0x80c)
	#define REG_SMP_CLK_FORCE_EN REG_FLD_MSB_LSB(0, 0)
	#define REG_SMP_CLK_GATING_DB_EN REG_FLD_MSB_LSB(1, 1)
	#define REG_TMR_CLK_FORCE_EN REG_FLD_MSB_LSB(2, 2)
	#define REG_TMR_CLK_GATING_DB_EN REG_FLD_MSB_LSB(3, 3)
	#define REG_SCL_CLK_FORCE_EN REG_FLD_MSB_LSB(4, 4)
	#define REG_SCL_CLK_GATING_DB_EN REG_FLD_MSB_LSB(5, 5)
	#define REG_CNT_CLK_FORCE_EN REG_FLD_MSB_LSB(6, 6)
	#define REG_CNT_CLK_GATING_DB_EN REG_FLD_MSB_LSB(7, 7)
	#define REG_GATING_RESERVE REG_FLD_MSB_LSB(31, 8)
#define REG_DBI_COUNT_PPCTRL_0 (0x810)
#define REG_DBI_COUNT_PPCTRL_1 (0x814)
#define REG_DBI_COUNT_DBG_0 (0x818)
#define REG_DBI_COUNT_DITHER (0x81c)
#define REG_DBI_COUNT_VPP_RSZ_BYPASS (0x820)
#define REG_DBI_COUNT_HCROP_IDX (0x824)

#define DISP_DBI_COUNT_UDMA_R_CTRL30 (0xc6c)
#define DISP_DBI_COUNT_UDMA_W_CTR_1B (0xe6c)


#define DISP_DBI_COUNT_UDMA_R_CTRL37 (0xc74)
	#define REG_FRAME_CMD_MODE_R REG_FLD_MSB_LSB(15, 15)

#define DISP_DBI_COUNT_UDMA_W_CTRL37 (0xE74)
	#define REG_FRAME_CMD_MODE_W REG_FLD_MSB_LSB(15, 15)

#define DISP_DBI_COUNT_UDMA_W_CTRL47 (0xF00)
	#define REG_PRTCL_PROT_OFF_W REG_FLD_MSB_LSB(7, 7)
#define DISP_DBI_COUNT_UDMA_R_CTR70 (0xD00)
	#define REG_PRTCL_PROT_OFF_R REG_FLD_MSB_LSB(7, 7)

static void mtk_dbi_hw_count_trigger(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, uint32_t slice_num,
	uint32_t slice_id, uint32_t time_ms, u64 addr, struct mtk_dbi_count_hw_param *count_param);

static void mtk_dbi_count_config(struct mtk_ddp_comp *comp,
		struct mtk_ddp_config *cfg,
		struct cmdq_pkt *handle);
static void mtk_dbi_count_srt_cal(struct mtk_ddp_comp *comp, int en, int slice_num);
static void mtk_dbi_count_change_mode(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle);
static int mtk_dbi_count_get_mode_by_fmt(struct mtk_dbi_count_helper *helper, enum MTK_PANEL_SPR_MODE data_fmt);

#define DBI_SPIN_LOCK(lock, name, line, flag)                        \
	do {                                                         \
		DDPINFO("DBI_SPIN_LOCK:%s[%d] +\n", name, line);     \
		spin_lock_irqsave(lock, flag);                       \
	} while (0)

#define DBI_SPIN_UNLOCK(lock, name, line, flag)                                \
	do {                                                                   \
		DDPINFO("DBI_SPIN_UNLOCK:%s[%d] +\n", name, line);             \
		spin_unlock_irqrestore(lock, flag);                            \
	} while (0)

#define log_en (1)

#define DBI_COUNT_INFO(fmt, arg...) do { \
			if (log_en) \
				DDPINFO("[DBI_COUNT]%s:" fmt, __func__, ##arg); \
		} while (0)

#define DBI_COUNT_MSG(fmt, arg...) do { \
			if (log_en) \
				DDPMSG("[DBI_COUNT]%s:" fmt, __func__, ##arg); \
		} while (0)

static inline unsigned int mtk_dbi_count_read(struct mtk_ddp_comp *comp,
	unsigned int offset)
{
	u32 max_offset = 0x1000;

	if (offset >= max_offset || (offset % 4) != 0) {
		PC_ERR("%s: invalid addr 0x%x\n",
		__func__, offset);
		return 0;
	}
	return readl(comp->regs + offset);
}

static inline void mtk_dbi_count_write_mask_cpu(struct mtk_ddp_comp *comp,
	unsigned int value, unsigned int offset, unsigned int mask)
{
	u32 max_offset = 0x1000;
	unsigned int tmp;

	if (offset >= max_offset || (offset % 4) != 0) {
		PC_ERR("%s: invalid addr 0x%x\n",
		__func__, offset);
		return;
	}

	tmp = readl(comp->regs + offset);
	tmp = (tmp & ~mask) | (value & mask);
	writel(tmp, comp->regs + offset);
}

static inline void mtk_dbi_count_write_cpu(struct mtk_ddp_comp *comp,
	unsigned int value, unsigned int offset)
{
	u32 max_offset = 0x1000;

	if (offset >= max_offset || (offset % 4) != 0) {
		PC_ERR("%s: invalid addr 0x%x\n",
		__func__, offset);
		return;
	}
		writel(value, comp->regs + offset);
}

static inline void mtk_dbi_count_write(struct mtk_ddp_comp *comp, unsigned int value,
	unsigned int offset, void *handle)
{
	u32 max_offset = 0x1000;

	if (comp == NULL) {
		PC_ERR("%s: invalid comp\n", __func__);
		return;
	}

	if (offset >= max_offset || (offset % 4) != 0) {
		PC_ERR("%s: invalid addr 0x%x\n",
		__func__, offset);
		return;
	}

	if (handle != NULL) {
		cmdq_pkt_write((struct cmdq_pkt *)handle, comp->cmdq_base,
			comp->regs_pa + offset, value, ~0);
	} else {
		writel(value, comp->regs + offset);
	}
}

static inline void mtk_dbi_count_write_mask(struct mtk_ddp_comp *comp, unsigned int value,
	unsigned int offset, unsigned int mask, void *handle)
{
	u32 max_offset = 0x1000;

	if (offset >= max_offset || (offset % 4) != 0) {
		PC_ERR("%s: invalid addr 0x%x\n",
		__func__, offset);
		return;
	}

	if (handle != NULL) {
		cmdq_pkt_write((struct cmdq_pkt *)handle, comp->cmdq_base,
			comp->regs_pa + offset, value, mask);
	} else {
		mtk_dbi_count_write_mask_cpu(comp, value, offset, mask);
	}
}

static inline struct mtk_disp_dbi_count *comp_to_dbi_count(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_dbi_count, ddp_comp);
}

static void dmabuf_iova_free(struct mtk_dbi_dma_buf *dma)
{
	dma_buf_unmap_attachment_unlocked(dma->attach, dma->sgt, DMA_FROM_DEVICE);
	dma_buf_detach(dma->dmabuf, dma->attach);

	dma->sgt = NULL;
	dma->attach = NULL;
}

struct device *mtk_dbi_smmu_get_shared_device(struct device *dev, const char *name)
{
	struct device_node *node;
	struct platform_device *shared_pdev;
	struct device *shared_dev = dev;

	node = of_parse_phandle(dev->of_node, name, 0);
	if (node) {
		shared_pdev = of_find_device_by_node(node);
		if (shared_pdev)
			shared_dev = &shared_pdev->dev;
	}

	return shared_dev;
}

static int dmabuf_to_iova(struct drm_device *dev, struct mtk_dbi_dma_buf *dma)
{
	int err;
	struct mtk_drm_private *priv = dev->dev_private;

	dma->attach = dma_buf_attach(dma->dmabuf, mtk_dbi_smmu_get_shared_device(priv->dma_dev,"mtk,smmu-shared-sec"));
	if (IS_ERR_OR_NULL(dma->attach)) {
		err = PTR_ERR(dma->attach);
		DDPMSG("%s attach fail buf %p dev %p err %d",
			__func__, dma->dmabuf, priv->dma_dev, err);
		goto err;
	}

	dma->attach->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;
	dma->sgt = dma_buf_map_attachment_unlocked(dma->attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(dma->sgt)) {
		err = PTR_ERR(dma->sgt);
		DDPMSG("%s map failed err %d attach %p dev %p",
			__func__, err, dma->attach, priv->dma_dev);
		goto err_detach;
	}

	dma->iova = sg_dma_address(dma->sgt->sgl);
	if (!dma->iova) {
		DDPMSG("%s iova map fail dev %p", __func__, priv->dma_dev);
		err = -ENOMEM;
		goto err_detach;
	}

	return 0;

err_detach:
	dma_buf_detach(dma->dmabuf, dma->attach);
	dma->sgt = NULL;
err:
	dma->attach = NULL;
	return err;
}

void mtk_crtc_release_dbi_count_fence_by_idx(
	struct drm_crtc *crtc, int session_id, unsigned int fence_idx)
{
	if (fence_idx && fence_idx != -1) {
		DDPINFO("output fence_idx:%d\n", fence_idx);
		mtk_release_fence(session_id,
			mtk_fence_get_dbi_count_timeline_id(), fence_idx);
	}
}

void mtk_crtc_dbi_count_pre_cfg(struct mtk_drm_crtc *mtk_crtc, struct mtk_crtc_state *crtc_state)
{
	unsigned int slice_num = crtc_state->prop_val[CRTC_PROP_DBI_COUNT_SLICE_NUM];
	unsigned int enable = crtc_state->prop_val[CRTC_PROP_DBI_COUNT_ENABLE];
	struct mtk_ddp_comp *dbi_count = NULL;
	struct mtk_disp_dbi_count *count_data;

	if (!mtk_crtc->dbi_data.support)
		return;

	dbi_count = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_DBI_COUNT, 0);

	if(!dbi_count)
		return;

	count_data = comp_to_dbi_count(dbi_count);

	if(count_data->status < DBI_COUNT_SW_INIT)
		return;

	mtk_dbi_count_srt_cal(dbi_count, enable, slice_num);
	if (enable)
		mtk_crtc->dbi_trigger = true;
	else
		mtk_crtc->dbi_trigger = false;

}

void mtk_crtc_dbi_count_cfg(struct mtk_drm_crtc *mtk_crtc, struct mtk_crtc_state *crtc_state, struct cmdq_pkt *handle)
{
	unsigned int slice_num = crtc_state->prop_val[CRTC_PROP_DBI_COUNT_SLICE_NUM];
	unsigned int slice_size = crtc_state->prop_val[CRTC_PROP_DBI_COUNT_SLICE_SIZE];
	unsigned int enable = crtc_state->prop_val[CRTC_PROP_DBI_COUNT_ENABLE];
	unsigned int fence = crtc_state->prop_val[CRTC_PROP_DBI_COUNT_FENCE_IDX];
	int session_id = 0;
	struct mtk_dbi_timer *dbi_timer;
	int sec;
	unsigned long flags;
	struct mtk_ddp_comp *dbi_count = NULL;
	struct mtk_ddp_comp *oddmr = NULL;
	struct mtk_disp_dbi_count *count_data;
	int crtc_index = drm_crtc_index(&mtk_crtc->base);
	uint32_t value = 0;
	uint32_t panel_height;
	int mode_id;
	struct mtk_dbi_count_hw_param *count_param;

	if (!mtk_crtc->dbi_data.support)
		return;

	dbi_count = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_DBI_COUNT, 0);

	if(!dbi_count)
		return;

	count_data = comp_to_dbi_count(dbi_count);

	if(count_data->status < DBI_COUNT_SW_INIT)
		return;

	session_id = mtk_get_session_id(&mtk_crtc->base);
	mtk_dbi_count_srt_cal(dbi_count, enable, slice_num);
	if (enable) {
		if (atomic_read(&mtk_crtc->dbi_data.disable_finish) == 1) {
			/* enable again*/
			if (mtk_crtc->dbi_data.slice_idx >= mtk_crtc->dbi_data.slice_num) {
				mtk_crtc->dbi_data.slice_idx = 0;
				mtk_crtc->dbi_data.slice_num = slice_num;
				mtk_crtc->dbi_data.real_idx = slice_size;
				count_data->buffer_time += count_data->current_freq * 1000;
				DBI_COUNT_INFO("%d %d\n", count_data->buffer_time, count_data->buffer_cfg.sw_timer_ms);
				if (count_data->buffer_time >= count_data->buffer_cfg.sw_timer_ms){
					count_data->buffer_time = 0;
					CRTC_MMP_MARK(0, dbi_merge, 0, 0);
					atomic_set(&count_data->buffer_full, 1);
				}
				if(atomic_read(&count_data->current_count_mode) !=
					atomic_read(&count_data->new_count_mode)) {
					DBI_COUNT_INFO("change count mode from %d to %d\n",
						atomic_read(&count_data->current_count_mode),
						atomic_read(&count_data->new_count_mode));
					atomic_set(&count_data->current_count_mode,
						atomic_read(&count_data->new_count_mode));
					count_data->count_cnt = 0;
					mtk_dbi_count_change_mode(dbi_count, handle);
				}
				count_data->count_cnt++;
			}
			mtk_crtc->dbi_data.fence_idx = fence;
			mtk_crtc->dbi_data.fence_unreleased = 0;
			mtk_crtc_release_dbi_count_fence_by_idx(&mtk_crtc->base,
					session_id, mtk_crtc->dbi_data.fence_idx);
			atomic_set(&mtk_crtc->dbi_data.disable_finish, 0);
		}

		if (mtk_crtc->dbi_data.slice_idx < mtk_crtc->dbi_data.slice_num){
			mode_id = atomic_read(&count_data->current_count_mode);
			count_param = &count_data->count_cfg.count_cfg.hw_count_param[mode_id];
			if(count_param->irdrop_enable) {
				oddmr = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_ODDMR, 0);
				if (oddmr) {
					panel_height = count_data->count_cfg.basic_info.panel_height;
					mtk_oddmr_dbi_trigger_ir_drop(oddmr, handle, panel_height);
				}
			}
			mtk_dbi_hw_count_trigger(dbi_count, handle, slice_num,
					mtk_crtc->dbi_data.real_idx,
					count_data->current_freq * 1000, count_data->count_buffer.iova, count_param);
			CRTC_MMP_MARK(crtc_index, dbi_trigger, (unsigned long)mtk_crtc->dbi_data.real_idx,
				(unsigned long)((mtk_crtc->dbi_data.slice_num << 16) | mtk_crtc->dbi_data.slice_idx));
			DBI_COUNT_INFO("%d %d %d\n", mtk_crtc->dbi_data.slice_idx,
				mtk_crtc->dbi_data.slice_num, mtk_crtc->dbi_data.real_idx);
			mtk_crtc->dbi_data.slice_idx++;
			mtk_crtc->dbi_data.real_idx++;
			if(mtk_crtc->dbi_data.real_idx >= mtk_crtc->dbi_data.slice_num)
				mtk_crtc->dbi_data.real_idx = 0;
			if (mtk_crtc->dbi_data.slice_idx >= mtk_crtc->dbi_data.slice_num) {
				value = 0;
				value |= DBI_COUNT_FRAME_DONE;
				mtk_dbi_count_write_mask(dbi_count, value,
					DISP_DBI_COUNT_IRQ_CLR, value, handle);
				mtk_dbi_count_write_mask(dbi_count, value,
					DISP_DBI_COUNT_IRQ_MASK, value, handle);
			}

			disp_pq_set_test_flag(TEST_FLAG_DBI_COUNT);
		}
	} else {
		if (mtk_crtc->dbi_data.fence_unreleased) {
			mtk_crtc_release_dbi_count_fence_by_idx(&mtk_crtc->base,
				session_id, mtk_crtc->dbi_data.fence_idx);
			mtk_crtc->dbi_data.fence_unreleased = 0;
		}
		if(!atomic_read(&mtk_crtc->dbi_data.disable_finish)){
			atomic_set(&mtk_crtc->dbi_data.disable_finish, 1);
			wake_up_all(&mtk_crtc->dbi_data.disable_finish_wq);
		}
	}

	dbi_timer = &mtk_crtc->dbi_data.dbi_timer;
	DBI_SPIN_LOCK(&dbi_timer->lock, __func__, __LINE__, flags);
	if (dbi_timer->active) {
		sec = dbi_timer->sec;
		mod_timer(&dbi_timer->base, jiffies + msecs_to_jiffies(sec*1000));
	}
	DBI_SPIN_UNLOCK(&dbi_timer->lock, __func__, __LINE__, flags);

}

static void mtk_oddmr_dbi_count_done(struct work_struct *work_item)
{
	struct mtk_dbi_event *dbi_event = container_of(work_item,
		struct mtk_dbi_event, task);
	unsigned long flags;

	CRTC_MMP_MARK(0, dbi_count_done, 0, 0);

	DBI_SPIN_LOCK(&dbi_event->lock, __func__, __LINE__, flags);
	dbi_event->event |= (1<<DBI_COUNT_DONE);
	wake_up_all(&dbi_event->event_wq);
	DBI_SPIN_UNLOCK(&dbi_event->lock, __func__, __LINE__, flags);

}

void mtk_crtc_dbi_count_init(struct mtk_drm_crtc *mtk_crtc)
{
	spin_lock_init(&mtk_crtc->dbi_data.dbi_timer.lock);

	mtk_crtc->dbi_data.support = 1;
	mtk_crtc->dbi_data.dbi_timer.mtk_crtc = mtk_crtc;
	init_waitqueue_head(&mtk_crtc->dbi_data.dbi_event.event_wq);
	spin_lock_init(&mtk_crtc->dbi_data.dbi_event.lock);

	mtk_crtc->dbi_data.dbi_event.work_queue = create_singlethread_workqueue("mtk_dbic_wq");
	if (!mtk_crtc->dbi_data.dbi_event.work_queue) {
		PC_ERR("Failed to create mtk_dbic_wq workqueue\n");
		mtk_crtc->dbi_data.support = 0;
	}

	INIT_WORK(&mtk_crtc->dbi_data.dbi_event.task, mtk_oddmr_dbi_count_done);

	init_waitqueue_head(&mtk_crtc->dbi_data.idle_count_info.wait_wq);
	for(int i = 0; i < MAX_CHECK_FENCE_NUM; i++)
		atomic_set(&mtk_crtc->dbi_data.idle_count_info.update[i], 0);

	init_waitqueue_head(&mtk_crtc->dbi_data.disable_finish_wq);

}

void mtk_dbi_idle_count_insert_wb_fence(struct mtk_drm_crtc *mtk_crtc, unsigned int fence)
{
	struct mtk_disp_dbi_idle_count *info = &mtk_crtc->dbi_data.idle_count_info;

	if(!mtk_crtc->dbi_data.support)
		return;

	info->fence_idx[info->idx] = fence;
	atomic_set(&info->update[info->idx], 0);
	info->idx++;
	if(info->idx == MAX_CHECK_FENCE_NUM)
		info->idx = 0;
	info->insert = true;

}

void mtk_dbi_idle_count_update_wb_fence(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_disp_dbi_idle_count *info = &mtk_crtc->dbi_data.idle_count_info;

	if(!mtk_crtc->dbi_data.support)
		return;

	if(info->insert) {
		info->insert = false;
		for(int i=0; i< MAX_CHECK_FENCE_NUM;i++)
			atomic_set(&info->update[i], 1);
		wake_up_all(&info->wait_wq);
	}
}

int mtk_dbi_count_wait_event(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	int ret = 0;
	unsigned int *event = data;
	struct mtk_dbi_event *dbi_event = &mtk_crtc->dbi_data.dbi_event;
	unsigned long flags_event;

	wait_event_interruptible_timeout(dbi_event->event_wq,
		dbi_event->event, msecs_to_jiffies(10000));

	DBI_SPIN_LOCK(&dbi_event->lock, __func__, __LINE__, flags_event);
	if (dbi_event->event) {
		*event = dbi_event->event;
		dbi_event->event = 0;
		DBI_SPIN_UNLOCK(&dbi_event->lock, __func__, __LINE__, flags_event);
		return ret;
	}
	DBI_SPIN_UNLOCK(&dbi_event->lock, __func__, __LINE__, flags_event);

	return ret;
}

int mtk_dbi_count_clear_event(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	int ret = 0;
	struct mtk_dbi_event *dbi_event = &mtk_crtc->dbi_data.dbi_event;
	unsigned long flags_event;
	unsigned int *event = data;

	DBI_SPIN_LOCK(&dbi_event->lock, __func__, __LINE__, flags_event);
	*event = dbi_event->event;
	dbi_event->event = 0;
	DBI_SPIN_UNLOCK(&dbi_event->lock, __func__, __LINE__, flags_event);

	return ret;
}


int mtk_dbi_count_wait_new_frame(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	unsigned int *fence_idx = data;
	unsigned int *out = data;
	struct mtk_disp_dbi_idle_count *info = &mtk_crtc->dbi_data.idle_count_info;
	int i=0;

	DDP_MUTEX_LOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, false);
	for(i=0;i<MAX_CHECK_FENCE_NUM;i++){
		if(info->fence_idx[i] == *fence_idx)
			break;
	}
	DDP_MUTEX_UNLOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, false);

	if(i == MAX_CHECK_FENCE_NUM)
		return -1;

	wait_event_interruptible_timeout(info->wait_wq,
		atomic_read(&info->update[i]), msecs_to_jiffies(20000));
	*out = atomic_read(&info->update[i]);

	return 0;
}



void mtk_dbi_count_timer_callback( struct timer_list *timer)
{
	struct mtk_dbi_timer *dbi_timer;
	struct mtk_drm_crtc *mtk_crtc;
	unsigned long flags;
	struct mtk_dbi_event *dbi_event;
	unsigned long flags_event;

	dbi_timer = to_dbi_timer(timer);
	mtk_crtc = dbi_timer->mtk_crtc;
	dbi_event = &mtk_crtc->dbi_data.dbi_event;

	DBI_COUNT_INFO("%s +++\n",__func__);

	DBI_SPIN_LOCK(&dbi_timer->lock, __func__, __LINE__, flags);
	dbi_timer->active = 0;
	DBI_SPIN_UNLOCK(&dbi_timer->lock, __func__, __LINE__, flags);

	DBI_SPIN_LOCK(&dbi_event->lock, __func__, __LINE__, flags_event);
	dbi_event->event |= (1<<DBI_COUNT_IDLE_TIMER_TRIGGER);
	wake_up_all(&mtk_crtc->dbi_data.dbi_event.event_wq);
	DBI_SPIN_UNLOCK(&dbi_event->lock, __func__, __LINE__, flags_event);

}

int mtk_dbi_count_create_timer(struct mtk_ddp_comp *comp, void *data, bool need_lock, bool update_sec)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	int ret = 0;
	unsigned int sec;
	struct timer_list *timer = &mtk_crtc->dbi_data.dbi_timer.base;
	struct mtk_dbi_timer *dbi_timer =  &mtk_crtc->dbi_data.dbi_timer;
	unsigned long flags;
	struct mtk_dbi_event *dbi_event = &mtk_crtc->dbi_data.dbi_event;
	unsigned long flags_event;

	DBI_COUNT_INFO("+++\n");

	if (need_lock)
		DBI_SPIN_LOCK(&dbi_timer->lock, __func__, __LINE__, flags);
	if (dbi_timer->enable)
		del_timer(timer);
	if (update_sec) {
		sec = *(unsigned int *)data;
		dbi_timer->sec = sec;
		DBI_COUNT_INFO("sec %d\n",dbi_timer->sec);
	}
	dbi_timer->active = 1;
	dbi_timer->enable = 1;
	dbi_timer->suspend = 0;

	DBI_SPIN_LOCK(&dbi_event->lock, __func__, __LINE__, flags_event);
	dbi_event->event &= ~(1<<DBI_COUNT_IDLE_TIMER_TRIGGER);
	DBI_SPIN_UNLOCK(&dbi_event->lock, __func__, __LINE__, flags_event);

	timer_setup(timer, mtk_dbi_count_timer_callback, 0);
	mod_timer(timer, jiffies + msecs_to_jiffies((mtk_crtc->dbi_data.dbi_timer.sec)*1000));
	if (need_lock)
		DBI_SPIN_UNLOCK(&dbi_timer->lock, __func__, __LINE__, flags);
	return ret;
}

int mtk_dbi_count_delete_timer(struct mtk_ddp_comp *comp, bool need_lock, bool mark_suspend)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	int ret = 0;
	struct timer_list *timer = &mtk_crtc->dbi_data.dbi_timer.base;
	struct mtk_dbi_timer *dbi_timer =  &mtk_crtc->dbi_data.dbi_timer;
	unsigned long flags;

	DBI_COUNT_INFO(" +++\n");

	if (need_lock)
		DBI_SPIN_LOCK(&dbi_timer->lock, __func__, __LINE__, flags);
	if (mtk_crtc->dbi_data.dbi_timer.enable) {
		del_timer(timer);
		dbi_timer->active =  0;
		dbi_timer->enable = 0;
		if(mark_suspend)
			dbi_timer->suspend = 1;
	}
	if (!mark_suspend)
		dbi_timer->suspend = 0;
	if (need_lock)
		DBI_SPIN_UNLOCK(&dbi_timer->lock, __func__, __LINE__, flags);

	return ret;
}

int mtk_dbi_count_timer_disable(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int ret = 0;
	struct mtk_dbi_timer *dbi_timer = &mtk_crtc->dbi_data.dbi_timer;
	unsigned long flags;
	unsigned int crtc_id = drm_crtc_index(crtc);
	struct mtk_crtc_state *mtk_state = to_mtk_crtc_state(crtc->state);
	struct mtk_ddp_comp *comp;
	int i,j;

	if (mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE])
		return ret;

	if (crtc_id)
		return ret;

	DBI_COUNT_INFO("+++\n");
	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		if (mtk_ddp_comp_get_type(comp->id) == MTK_DISP_DBI_COUNT){
			DBI_SPIN_LOCK(&dbi_timer->lock, __func__, __LINE__, flags);
			mtk_dbi_count_delete_timer(comp, false, true);
			DBI_SPIN_UNLOCK(&dbi_timer->lock, __func__, __LINE__, flags);
		}
	}
	return ret;
}


int mtk_dbi_count_timer_enable(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int ret = 0;
	struct mtk_dbi_timer *dbi_timer = &mtk_crtc->dbi_data.dbi_timer;
	unsigned long flags;
	struct mtk_crtc_state *mtk_state = to_mtk_crtc_state(crtc->state);
	unsigned int crtc_id = drm_crtc_index(crtc);
	struct mtk_ddp_comp *comp;
	int i,j;

	if (mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE])
		return ret;

	if (crtc_id)
		return ret;

	DBI_COUNT_INFO("+++\n");
	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		if (mtk_ddp_comp_get_type(comp->id) == MTK_DISP_DBI_COUNT){
			DBI_SPIN_LOCK(&dbi_timer->lock, __func__, __LINE__, flags);
			if (mtk_crtc->dbi_data.dbi_timer.suspend)
				mtk_dbi_count_create_timer(comp, NULL, false, false);
			DBI_SPIN_UNLOCK(&dbi_timer->lock, __func__, __LINE__, flags);
		}
	}

	return ret;
}



int mtk_drm_crtc_get_count_fence_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_crtc *crtc;
	struct drm_mtk_fence *args = data;
	struct mtk_drm_private *private;
	struct fence_data fence;
	unsigned int fence_idx;
	struct mtk_fence_info *l_info = NULL;
	int tl, idx;

	crtc = drm_crtc_find(dev, file_priv, args->crtc_id);
	if (!crtc) {
		PC_ERR("Unknown CRTC ID %d\n", args->crtc_id);
		ret = -ENOENT;
		return ret;
	}

	idx = drm_crtc_index(crtc);
	if (!crtc->dev) {
		PC_ERR("%s:%d dev is null\n", __func__, __LINE__);
		ret = -EFAULT;
		return ret;
	}
	if (!crtc->dev->dev_private) {
		PC_ERR("%s:%d dev private is null\n", __func__, __LINE__);
		ret = -EFAULT;
		return ret;
	}
	private = crtc->dev->dev_private;
	fence_idx = atomic_read(&private->crtc_dbi_count[idx]);
	tl = mtk_fence_get_dbi_count_timeline_id();
	l_info = mtk_fence_get_layer_info(mtk_get_session_id(crtc), tl);
	if (!l_info) {
		PC_ERR("%s:%d layer_info is null\n", __func__, __LINE__);
		ret = -EFAULT;
		return ret;
	}

	/* create fence */
	fence.fence = MTK_INVALID_FENCE_FD;
	fence.value = ++fence_idx;
	atomic_inc(&private->crtc_dbi_count[idx]);
	ret = mtk_sync_fence_create(l_info->timeline, &fence);
	if (ret) {
		PC_ERR("%d,L%d create Fence Object failed!\n",
			  MTK_SESSION_DEV(mtk_get_session_id(crtc)), tl);
		ret = -EFAULT;
	}

	args->fence_fd = fence.fence;
	args->fence_idx = fence.value;

	return ret;
}

struct dbi_count_block_info mtk_dbi_count_get_block_info(uint32_t block_h, uint32_t block_v)
{
	struct dbi_count_block_info  ret = { 0 };

	if((block_h == 1) && (block_v == 1)) {
		ret.block_h = 2;
		ret.block_v = 1;
		ret.channel = 4;
		return ret;
	} else if((block_h == 2) && (block_v == 1)){
		ret.block_h = 2;
		ret.block_v = 1;
		ret.channel = 3;
		return ret;
	} else if((block_h == 2) && (block_v == 2)){
		ret.block_h = 2;
		ret.block_v = 2;
		ret.channel = 3;
		return ret;
	}
	return ret;
}

void mtk_dbi_count_hrt_cal(struct drm_device *dev, int disp_idx, uint32_t en, uint32_t slice_size, uint32_t slice_num,
	uint32_t block_h, uint32_t block_v, int *oddmr_hrt)
{
	uint32_t panel_width = 1440;
	uint32_t panel_height = 3200;
	struct dbi_count_block_info block;
	uint32_t slice_width;
	uint32_t slice_height;
	uint32_t hrt = 0;
	uint32_t layer_size;
	struct drm_crtc *crtc = NULL;
	struct mtk_ddp_comp *comp;
	struct mtk_disp_dbi_count *dbi_count;
	struct mtk_drm_crtc *mtk_crtc;
	int index;

	drm_for_each_crtc(crtc, dev) {
		comp = NULL;
		mtk_crtc = to_mtk_crtc(crtc);
		index = drm_crtc_index(crtc);
		if(index != disp_idx)
			continue;
		comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_DBI_COUNT, 0);
		if(comp && en) {
			dbi_count = comp_to_dbi_count(comp);
			panel_width = dbi_count->count_cfg.basic_info.panel_width;
			panel_height = dbi_count->count_cfg.basic_info.panel_height;
			block = mtk_dbi_count_get_block_info(block_h, block_v);
			slice_width = panel_width / block.block_h * block.channel * 2;
			slice_height = (panel_height/block.block_v + slice_num - 1)/slice_num;
			hrt = slice_width * slice_height * 2;
			layer_size = panel_width * panel_height * 4;
			hrt = (400 * hrt + layer_size -1)/layer_size;
			*oddmr_hrt += hrt;
			DBI_COUNT_INFO("en:%d, total:%d, hrt:%d\n", en, *oddmr_hrt, hrt);
		}
	}
}

void mtk_dbi_count_hrt_cal_ratio(struct mtk_ddp_comp *comp, enum CHANNEL_TYPE type, int *dbi_count_hrt)
{
	uint32_t panel_width = 1440;
	uint32_t panel_height = 3200;
	struct dbi_count_block_info block;
	uint32_t slice_width;
	uint32_t slice_height;
	uint32_t hrt = 0;
	uint32_t layer_size;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_crtc_state *crtc_state = to_mtk_crtc_state(crtc->state);
	uint32_t en = crtc_state->prop_val[CRTC_PROP_DBI_COUNT_ENABLE];
	uint32_t block_h = crtc_state->prop_val[CRTC_PROP_DBI_COUNT_BLOCK_H];
	uint32_t block_v = crtc_state->prop_val[CRTC_PROP_DBI_COUNT_BLOCK_V];
	uint32_t slice_num = crtc_state->prop_val[CRTC_PROP_DBI_COUNT_SLICE_NUM];
	unsigned long long res_ratio = 1000;
	struct mtk_disp_dbi_count *dbi_count = comp_to_dbi_count(comp);

	if(en) {
		panel_width = dbi_count->count_cfg.basic_info.panel_width;
		panel_height = dbi_count->count_cfg.basic_info.panel_height;
		block = mtk_dbi_count_get_block_info(block_h, block_v);
		slice_width = panel_width / block.block_h * block.channel * 2;
		slice_height = (panel_height/block.block_v + slice_num - 1)/slice_num;
		hrt = slice_width * slice_height;
		layer_size = panel_width * panel_height * 4;
		hrt = (400 * hrt + layer_size - 1)/layer_size;
		if (mtk_crtc->scaling_ctx.scaling_en) {
			res_ratio =
			((unsigned long long)mtk_crtc->scaling_ctx.lcm_width *
			mtk_crtc->scaling_ctx.lcm_height * 1000) /
			((unsigned long long)mtk_crtc->base.state->adjusted_mode.vdisplay *
			mtk_crtc->base.state->adjusted_mode.hdisplay);
		}
		hrt = hrt * res_ratio / 1000;
		*dbi_count_hrt += hrt;
		DBI_COUNT_INFO("en:%d, total:%d, hrt:%d\n", en, *dbi_count_hrt, hrt);
	}
}


int mtk_dbi_count_wait_disable_finish(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	unsigned int *event = data;

	DBI_COUNT_INFO("+++\n");

	wait_event_interruptible_timeout(mtk_crtc->dbi_data.disable_finish_wq,
		atomic_read(&mtk_crtc->dbi_data.disable_finish), msecs_to_jiffies(10000));
	*event = atomic_read(&mtk_crtc->dbi_data.disable_finish);

	return 0;
}

int mtk_dbi_count_check_buffer(struct mtk_ddp_comp *comp, void *data)
{
	unsigned int *event = data;
	struct mtk_disp_dbi_count *dbi_count = comp_to_dbi_count(comp);

	DBI_COUNT_INFO("+++\n");
	*event = atomic_read(&dbi_count->buffer_full);
	atomic_set(&dbi_count->buffer_full, 0);

	return 0;
}

int mtk_dbi_count_set_freq(struct mtk_ddp_comp *comp, void *data)
{
	int *freq = data;
	struct mtk_disp_dbi_count *dbi_count = comp_to_dbi_count(comp);

	DBI_COUNT_INFO("+++ %d\n", *freq);
	dbi_count->current_freq = *freq;

	return 0;
}

int mtk_dbi_count_set_temp(struct mtk_ddp_comp *comp, void *data)
{
	int *temp = data;
	struct mtk_disp_dbi_count *dbi_count = comp_to_dbi_count(comp);

	if(*temp != dbi_count->current_temp){
		DBI_COUNT_INFO("+++ %d\n", *temp);
		dbi_count->current_temp = *temp;
		dbi_count->temp_chg = true;
	}

	return 0;
}

int mtk_dbi_count_set_counting_mode(struct mtk_ddp_comp *comp, void *data)
{
	int *mode_id = data;
	struct mtk_disp_dbi_count *dbi_count = comp_to_dbi_count(comp);


	DBI_COUNT_INFO("+++ %d\n", *mode_id);
	atomic_set(&dbi_count->new_count_mode, *mode_id);

	return 0;
}

int mtk_dbi_count_load_buffer(struct mtk_ddp_comp *comp,void *data)
{
	int *param = (int *)data;
	int fd = param[0];
	int size = param[1];
	int ret;
	struct drm_crtc *crtc = &comp->mtk_crtc->base;
	struct mtk_disp_dbi_count *dbi_count = comp_to_dbi_count(comp);
	struct mtk_dbi_dma_buf *dbi_buf = &dbi_count->count_buffer;


	DBI_COUNT_INFO("+++\n");
	if(dbi_buf->used) {
		DBI_COUNT_INFO("free buffer\n");
		dmabuf_iova_free(dbi_buf);
		dma_buf_put(dbi_buf->dmabuf);
		dbi_buf->used = 0;
	}

	dbi_buf->dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dbi_buf->dmabuf)) {
		PC_ERR("%s: fail to get dma_buf by fd : %d\n", __func__, fd);
		return -1;
	}

	ret = dmabuf_to_iova(crtc->dev, dbi_buf);
	if (ret < 0) {
		PC_ERR("%s: fail to dmabuf_to_iova : %d\n", __func__, ret);
		return -1;
	}
	dbi_buf->size = size;
	dbi_buf->used = 1;

	DBI_COUNT_INFO("iova map success, iova: %llx, size:%d\n", dbi_buf->iova, dbi_buf->size);

	return 0;
}

void mtk_dbi_count_bypass(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	uint32_t value = 0, mask = 0;

	SET_VAL_MASK(value, mask, 1, REG_DBI_COUNT_BYPASS);
	mtk_dbi_count_write_mask(comp, value,
			DISP_DBI_COUNT_TOP_CTR_3, mask, handle);
	mtk_dbi_count_write(comp, 1, REG_DBI_SAMPLING_BYPASS, handle);
	mtk_dbi_count_write(comp, 1, REG_DBI_COUNTING_BYPASS, handle);

	// udma disable
	value = 0; mask = 0;
	SET_VAL_MASK(value, mask, 0, COUNT_UDMA_W_EN);
	SET_VAL_MASK(value, mask, 0, COUNT_UDMA_R_EN);
	mtk_dbi_count_write_mask(comp, value, REG_DBI_COUNT_UDMA_W_EN, mask, handle);

	// hw disable
	value = 0; mask = 0;
	SET_VAL_MASK(value, mask, 0, COUNTING_HW_ENABLE);
	mtk_dbi_count_write_mask(comp, value, REG_DBI_COUNTING_HW_ENABLE, mask, handle);

	value = 0; mask = 0;
	SET_VAL_MASK(value, mask, 0, SAMPLING_PQ_SINGLE_TRIGGER);
	SET_VAL_MASK(value, mask, 1, SAMPLING_PQ_SINGLE_TRIGGER_SW_EN);
	mtk_dbi_count_write_mask(comp, value, REG_DBI_SAMPLING_PQ_SINGLE_TRIGGER_SW_EN, mask, handle);

	mtk_dbi_count_write(comp, 1,
				REG_DBI_FRAME_DROP_BLOCK_FUNC, handle);
}

void mtk_dbi_count_close_clk_if_no_error(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	uint32_t value = 0, mask = 0;
	GCE_COND_DECLARE;
	struct cmdq_operand lop, rop;
	const u16 var1 = CMDQ_THR_SPR_IDX2;
	const u16 var2 = 0;

	GCE_COND_ASSIGN(handle, CMDQ_THR_SPR_IDX1, CMDQ_GPR_R07);
	/* get dbi status */
	lop.reg = true;
	lop.idx = var1;
	rop.reg = false;
	rop.value = 1;

	cmdq_pkt_read(handle, NULL,
		mtk_get_gce_backup_slot_pa(comp->mtk_crtc, DISP_SLOT_DBI_COUNT_ERROR), var1);
	cmdq_pkt_logic_command(handle, CMDQ_LOGIC_AND, var1, &lop, &rop);
	lop.reg = true;
	lop.idx = var1;
	rop.reg = false;
	rop.idx = var2;
	rop.value = 1;
	GCE_IF(lop, R_CMDQ_EQUAL, rop);
	/* condition true: DBI enabled, enable dbi ddren */
		cmdq_pkt_write(handle, comp->mtk_crtc->gce_obj.base,
				mtk_get_gce_backup_slot_pa(comp->mtk_crtc,
				DISP_SLOT_DBI_COUNT_ERROR), 0, ~0);


	GCE_ELSE;

	value = 0;mask = 0;
	SET_VAL_MASK(value, mask, 0, REG_CNT_CLK_FORCE_EN);
	SET_VAL_MASK(value, mask, 0, REG_SCL_CLK_FORCE_EN);
	SET_VAL_MASK(value, mask, 0, REG_SMP_CLK_FORCE_EN);
	mtk_dbi_count_write_mask(comp, value, REG_DBI_GATING, mask, handle);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + REG_DBI_COUNT_UDMA_W_EN, 0, ~0);

	GCE_FI;
}

void mtk_oddmr_dbi_count_clk_off(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	GCE_COND_DECLARE;
	struct cmdq_operand lop, rop;
	const u16 var1 = CMDQ_THR_SPR_IDX2;
	const u16 var2 = 0;

	GCE_COND_ASSIGN(handle, CMDQ_THR_SPR_IDX1, CMDQ_GPR_R07);
	/* get dbi status */
	lop.reg = true;
	lop.idx = var1;
	rop.reg = false;
	rop.value = 1;
	cmdq_pkt_read(handle, NULL,
		comp->regs_pa + REG_DBI_COUNT_UDMA_W_EN, var1);
	cmdq_pkt_logic_command(handle, CMDQ_LOGIC_AND, var1, &lop, &rop);
	lop.reg = true;
	lop.idx = var1;
	rop.reg = false;
	rop.idx = var2;
	rop.value = 1;
	GCE_IF(lop, R_CMDQ_EQUAL, rop);
	/* condition true: DBI enabled, enable dbi ddren */

	mtk_dbi_count_close_clk_if_no_error(comp, handle);
	GCE_FI;
}

static void mtk_dbi_set_reg_by_mode(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle,struct mtk_dbi_mode_reg_list *mode_cfg, int mode)
{
	int size;

	if(!mode_cfg) {
		PC_ERR("%s:%d mode_cfg is null\n", __func__, __LINE__);
		return;
	}

	size = mode_cfg->reg_num;
	for(int i=0;i<size;i++) {
		if(mode_cfg->mask[i])
			mtk_dbi_count_write_mask(comp, mode_cfg->mode_value[size*mode+i],
				mode_cfg->addr[i], mode_cfg->mask[i], handle);
	}
}

int mtk_dbi_curve_interpolate(struct mtk_dbi_curve_2d *curve, uint32_t x)
{
	uint32_t x_l;
	uint32_t x_r;
	uint32_t y_l;
	uint32_t y_r;

	if(curve->num <=0)
		return 0;

	if(x<=curve->ux[0])
		return curve->uy[0];

	if (x>=curve->ux[curve->num-1])
		return curve->uy[curve->num-1];

	for (int r=1; r<curve->num; r++) {
		if (x==curve->ux[r])
			return curve->uy[r];
		else if (x < curve->ux[r]) {
			x_l = curve->ux[r-1];
			x_r = curve->ux[r];
			y_l = curve->uy[r-1];
			y_r = curve->uy[r];
			return ((x-x_l)*y_r + (x_r-x)*y_l)/(x_r - x_l);
		}
	}
	return 0;
}

void mtk_dbi_debug(struct drm_crtc *crtc, const char *opt)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;
	struct mtk_disp_dbi_count *dbi_count;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_DBI_COUNT, 0);
	if (!comp) {
		PC_ERR("%s, comp is null!\n", __func__);
		return;
	}
	dbi_count = comp_to_dbi_count(comp);

	if (strncmp(opt, "gain_log:", 9) == 0) {
		dbi_count->show_gain = strncmp(opt + 9, "1", 1) == 0;
		DBI_COUNT_MSG("gain_log = %d\n", dbi_count->show_gain);
	}
}

void mtk_dbi_show_gain_status(uint32_t dbv, uint32_t fps, int temp,
	uint32_t *dbv_gain, uint32_t *fps_gain, uint32_t *temp_gain, uint32_t *irdrop_gain)
{
	uint32_t b, rsh;

	b = 4;
	rsh = 10-b;

	DBI_COUNT_MSG("dbv:%u, dbv_gain:R = %u / G = %u / B = %u (format=fix point .%d)\n",
		dbv, dbv_gain[DBI_CH_R] >> rsh, dbv_gain[DBI_CH_G] >> rsh, dbv_gain[DBI_CH_B] >> rsh, b);
	DBI_COUNT_MSG("fps:%u, fps_gain:R = %u / G = %u / B = %u (format=fix point .%d)\n",
		fps, fps_gain[DBI_CH_R] >> rsh, fps_gain[DBI_CH_G] >> rsh, fps_gain[DBI_CH_B] >> rsh, b);
	DBI_COUNT_MSG("temp:%u, temp_gain:R = %u / G = %u / B = %u (format=fix point .%d)\n",
		temp, temp_gain[DBI_CH_R] >> rsh, temp_gain[DBI_CH_G] >> rsh, temp_gain[DBI_CH_B] >> rsh, b);
	DBI_COUNT_MSG("irdrop_gain:R = %u / G = %u / B = %u (format=fix point .%d)\n",
		irdrop_gain[DBI_CH_R] >> rsh, irdrop_gain[DBI_CH_G] >> rsh, irdrop_gain[DBI_CH_B] >> rsh, b);

}

void mtk_dbi_show_irdrop_status(uint64_t *ir_drop_stat_acc, uint64_t *ir_drop_squa_acc)
{
	DBI_COUNT_MSG("ir_drop_stat_acc:R = %llu / G = %llu / B = %llu\n",
		ir_drop_stat_acc[DBI_CH_R], ir_drop_stat_acc[DBI_CH_G], ir_drop_stat_acc[DBI_CH_B]);
	DBI_COUNT_MSG("ir_drop_stat_squa_acc:R = %llu / G = %llu / B = %llu\n",
		ir_drop_squa_acc[DBI_CH_R], ir_drop_squa_acc[DBI_CH_G], ir_drop_squa_acc[DBI_CH_B]);
}

void mtk_dbi_get_irdrop_gain(struct mtk_dbi_count_hw_param *count_param, uint32_t total_pixel,
	uint32_t dbv, uint64_t *code_sum, uint64_t *code_square_sum, uint32_t *ret_irdrop_gain)
{
	uint32_t weight_code_sum = 0;
	uint32_t weight_sum = 0;
	uint32_t irdrop_gain_tmp[DBI_CHANNEL_NUM];
	uint64_t avg_code;
	uint32_t weight;
	uint32_t ratio;
	uint32_t ratio_gain;
	uint32_t dbv_gain;
	uint32_t total_code;
	uint32_t total_code_gain;

	for(int ch= 0;ch<DBI_CHANNEL_NUM;ch++) {
		avg_code = ((code_sum[ch]<<10) + (total_pixel >> 1))/total_pixel;
		weight = count_param->irdrop_total_weight[ch];
		weight_code_sum += (avg_code*weight) >>10;
		weight_sum += weight;

		ratio = code_square_sum[ch] == 0 ? 0:(avg_code*code_sum[ch]/code_square_sum[ch])>>2;
		ratio_gain = mtk_dbi_curve_interpolate(&count_param->irdrop_ratio_gain_curve[ch],ratio);
		dbv_gain = mtk_dbi_curve_interpolate(&count_param->irdrop_dbv_gain_curve[ch],dbv);
		irdrop_gain_tmp[ch] = (dbv_gain * ratio_gain) >> 10;
	}

	total_code = weight_code_sum / weight_sum;
	total_code_gain = mtk_dbi_curve_interpolate(&count_param->irdrop_total_gain_curve,total_code);

	for(int ch= 0;ch<DBI_CHANNEL_NUM;ch++) {
		ret_irdrop_gain[ch] = (total_code_gain * irdrop_gain_tmp[ch]) >> 10;
		ret_irdrop_gain[ch] = MIN(ret_irdrop_gain[ch], (1<<14)-1);
	}

}

static void mtk_dbi_update_count_gain(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, uint32_t dbv, uint32_t fps, int temp)
{
	struct mtk_disp_dbi_count *dbi_count = comp_to_dbi_count(comp);
	int mode_id = atomic_read(&dbi_count->current_count_mode);
	struct mtk_dbi_count_hw_param *count_param = &dbi_count->count_cfg.count_cfg.hw_count_param[mode_id];
	struct mtk_dbi_count_helper *count_helper = &dbi_count->count_cfg.count_helper;
	uint32_t dbv_gain, fps_gain, temp_gain, gain_norm, total_gain, irdrop_gain;
	uint32_t gains[DBI_CHANNEL_NUM] = { 0 };
	uint32_t max_gain = 0;
	uint32_t sh = 0;
	uint32_t irdrop_gains[DBI_CHANNEL_NUM] = {1<<10, 1<<10, 1<<10};
	uint32_t dbv_gains[DBI_CHANNEL_NUM];
	uint32_t fps_gains[DBI_CHANNEL_NUM];
	uint32_t temp_gains[DBI_CHANNEL_NUM];
	int real_temp = temp;
	uint64_t code_sum[DBI_CHANNEL_NUM] = { 0 };
	uint64_t code_square_sum[DBI_CHANNEL_NUM] = { 0 };
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	uint32_t check_sum = 0;
	uint32_t check_sum_cal = 0;
	uint32_t stat_r, stat_g, stat_b, squa_r_0, squa_r_1, squa_g_0, squa_g_1, squa_b_0, squa_b_1 = 0;
	uint32_t try_num = 10;
	bool hit = false;
	uint32_t panel_width, panel_height;
	uint32_t value = 0, mask = 0;

	if(dbi_count->status < DBI_COUNT_SW_INIT)
		return;

	DBI_COUNT_INFO("dbv/fps/temp %d/%d/%d\n", dbv, fps, temp);
	temp += count_helper->hw_count_temp_offset;

	if(count_param->irdrop_enable && (dbi_count->count_cnt > 1)) {
		for(int i = 0;i<try_num;i++){
			stat_r = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_DBI_IR_DROP_STAT_R);
			stat_g = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_DBI_IR_DROP_STAT_G);
			stat_b = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_DBI_IR_DROP_STAT_B);
			squa_r_0 = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_DBI_IR_DROP_SQUA_R_0);
			squa_r_1 = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_DBI_IR_DROP_SQUA_R_1);
			squa_g_0 = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_DBI_IR_DROP_SQUA_G_0);
			squa_g_1 = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_DBI_IR_DROP_SQUA_G_1);
			squa_b_0 = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_DBI_IR_DROP_SQUA_B_0);
			squa_b_1 = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_DBI_IR_DROP_SQUA_B_1);
			check_sum = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_DBI_IR_DROP_CHECK_SUM);
			check_sum_cal = 0;
			check_sum_cal += stat_r;
			check_sum_cal += stat_g;
			check_sum_cal += stat_b;
			check_sum_cal += squa_r_0;
			check_sum_cal += squa_r_1;
			check_sum_cal += squa_g_0;
			check_sum_cal += squa_g_1;
			check_sum_cal += squa_b_0;
			check_sum_cal += squa_b_1;
			if(check_sum_cal == check_sum) {
				hit = true;
				break;
			}
		}
		if(hit) {
			code_sum[DBI_CH_R] = stat_r;
			code_sum[DBI_CH_G] = stat_g;
			code_sum[DBI_CH_B] = stat_b;
			code_square_sum[DBI_CH_R] = (uint64_t)squa_r_1 << 32 | (uint64_t)squa_r_0;
			code_square_sum[DBI_CH_G] = (uint64_t)squa_g_1 << 32 | (uint64_t)squa_g_0;
			code_square_sum[DBI_CH_B] = (uint64_t)squa_b_1 << 32 | (uint64_t)squa_b_0;
			if(dbi_count->show_gain)
				mtk_dbi_show_irdrop_status(code_sum, code_square_sum);
			panel_width = dbi_count->count_cfg.basic_info.panel_width;
			panel_height = dbi_count->count_cfg.basic_info.panel_height;
			mtk_dbi_get_irdrop_gain(count_param, panel_width * panel_height,
				dbv, code_sum, code_square_sum, irdrop_gains);
		}else
			DBI_COUNT_MSG("read irdrop fail\n");
	}

	for (int ch = 0; ch < DBI_CHANNEL_NUM; ch++) {
		dbv_gain = mtk_dbi_curve_interpolate(&count_param->dbv_gain_curve[ch],dbv);
		fps_gain = mtk_dbi_curve_interpolate(&count_param->fps_gain_curve[ch],fps);
		temp_gain = mtk_dbi_curve_interpolate(&count_param->temp_gain_curve[ch],temp);
		irdrop_gain = irdrop_gains[ch];
		gain_norm = count_param->gain_norm[ch];
		total_gain = (((uint64_t)dbv_gain) * fps_gain * temp_gain)>>18;
		total_gain = ((uint64_t)total_gain * irdrop_gain * gain_norm) >> 20;
		gains[ch] = total_gain;
		max_gain = MAX(max_gain, total_gain>>12);
		dbv_gains[ch] = dbv_gain;
		fps_gains[ch] = fps_gain;
		temp_gains[ch] = temp_gain;
	}

	if(dbi_count->show_gain)
		mtk_dbi_show_gain_status(dbv, fps, real_temp,
			dbv_gains, fps_gains, temp_gains, irdrop_gains);
	if(max_gain >= (1<<7))
		sh = 0;
	else if(max_gain >= (1<<6))
		sh = 1;
	else if(max_gain >= (1<<5))
		sh = 2;
	else if(max_gain >= (1<<4))
		sh = 3;
	else
		sh = 4;

	for (int ch = 0; ch < DBI_CHANNEL_NUM; ch++) {
		if (ch == DBI_CH_R) {
			value = 0;
			mask = 0;
			SET_VAL_MASK(value, mask, MIN(gains[ch]>>(4-sh), 0xffff), COUNTING_GAIN_R);
			mtk_dbi_count_write_mask(comp, value, REG_DBI_COUNTING_GAIN_R, mask, handle);
		}
		if (ch == DBI_CH_G) {
			value = 0;
			mask = 0;
			SET_VAL_MASK(value, mask, MIN(gains[ch]>>(4-sh), 0xffff), COUNTING_GAIN_G);
			mtk_dbi_count_write_mask(comp, value, REG_DBI_COUNTING_GAIN_G, mask, handle);
		}
		if (ch == DBI_CH_B) {
			value = 0;
			mask = 0;
			SET_VAL_MASK(value, mask, MIN(gains[ch]>>(4-sh), 0xffff), COUNTING_GAIN_B);
			mtk_dbi_count_write_mask(comp, value, REG_DBI_COUNTING_GAIN_B, mask, handle);
		}
	}

	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, sh, DBI_COUNTING_SH1);
	mtk_dbi_count_write_mask(comp, value, REG_DBI_COUNTING_SH1, mask, handle);
}

static void mtk_dbi_set_slice(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, uint32_t slice_num, uint32_t slice_id)
{

	uint32_t value = 0, mask = 0;

	SET_VAL_MASK(value, mask, (slice_num - 1), COUNTING_SLICE_NUM);
	mtk_dbi_count_write_mask(comp, value, REG_DBI_COUNTING_MODE, mask, handle);

	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, 1, COUNTING_SW_IDX_EN);
	SET_VAL_MASK(value, mask, slice_id, COUNTING_SW_IDX);
	mtk_dbi_count_write_mask(comp, value, REG_DBI_COUNTING_SW_IDX_EN, mask, handle);
}

static void mtk_dbi_hw_count_trigger(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, uint32_t slice_num,
	uint32_t slice_id, uint32_t time_ms, u64 addr, struct mtk_dbi_count_hw_param *count_param)
{

	uint32_t value = 0, mask = 0;
	uint32_t time_diff;
	struct mtk_disp_dbi_count *dbi_count = comp_to_dbi_count(comp);

	if(dbi_count->status < DBI_COUNT_SW_INIT){
		PC_ERR("%s:%d trigger fail\n", __func__, __LINE__);
		return;
	}

	if(dbi_count->status == DBI_COUNT_SW_INIT)
		mtk_dbi_count_config(comp, NULL, handle);

	//buf config
	for(int i = 0; i < dbi_count->buffer_cfg.buf_reg_list.reg_num;i++) {
		if(dbi_count->buffer_cfg.buf_reg_list.mask[i])
			mtk_dbi_count_write_mask(comp, dbi_count->buffer_cfg.buf_reg_list.value[i],
				dbi_count->buffer_cfg.buf_reg_list.addr[i],
				dbi_count->buffer_cfg.buf_reg_list.mask[i], handle);
	}

	if(dbi_count->temp_chg || (count_param->irdrop_enable)) {
		if(dbi_count->temp_chg)
			dbi_count->temp_chg = false;
		mtk_dbi_update_count_gain(comp, handle,
			dbi_count->current_bl, dbi_count->current_fps, dbi_count->current_temp);
	}

	if(dbi_count->irq_num && dbi_count->data->irq_handler) {
		value = 0;
		value |= DBI_COUNT_INT_DONE;
		mtk_dbi_count_write_mask(comp, value,
			DISP_DBI_COUNT_IRQ_MASK, value, handle);
	}

	// single trigger
	if(dbi_count->data->use_slot_trigger)
		cmdq_pkt_write(handle, comp->mtk_crtc->gce_obj.base,
			mtk_get_gce_backup_slot_pa(comp->mtk_crtc,
			DISP_SLOT_DBI_COUNT_SW_TRIGGER), 1, ~0);
	else {
		value = 0; mask = 0;
		SET_VAL_MASK(value, mask, 1, SAMPLING_PQ_SINGLE_TRIGGER);
		mtk_dbi_count_write_mask(comp, value, REG_DBI_SAMPLING_PQ_SINGLE_TRIGGER_SW_EN, mask, handle);
	}

	//slice and time diff
	mtk_dbi_set_slice(comp, handle, slice_num, slice_id);
	time_diff = time_ms * 1000 / 1050;
	if(time_diff > 0xffff)
		time_diff = 0xffff;
	value = 0; mask = 0;
	SET_VAL_MASK(value, mask, 1, COUNTING_SW_TIMEDIFF_EN);
	SET_VAL_MASK(value, mask, time_diff, COUNTING_SW_TIMEDIFF);
	mtk_dbi_count_write_mask(comp, value, REG_DBI_COUNTING_SW_TIMEDIFF_EN, mask, handle);

	//buffer addr
	mtk_dbi_count_write(comp, (unsigned int)(addr>>4),
		REG_DBIR_UDMA_BASE_ADDR, handle);
	mtk_dbi_count_write(comp, (unsigned int)(addr>>4),
		REG_DBIW_UDMA_BASE_ADDR, handle);

	//	clk on
	value = 0;mask = 0;
	SET_VAL_MASK(value, mask, 1, REG_CNT_CLK_FORCE_EN);
	SET_VAL_MASK(value, mask, 1, REG_SCL_CLK_FORCE_EN);
	SET_VAL_MASK(value, mask, 1, REG_SMP_CLK_FORCE_EN);
	mtk_dbi_count_write_mask(comp, value, REG_DBI_GATING, mask, handle);

	//udma enable
	value = 0; mask = 0;
	SET_VAL_MASK(value, mask, 1, COUNT_UDMA_W_EN);
	SET_VAL_MASK(value, mask, 1, COUNT_UDMA_R_EN);
	mtk_dbi_count_write_mask(comp, value, REG_DBI_COUNT_UDMA_W_EN, mask, handle);

	// hw enable
	value = 0; mask = 0;
	SET_VAL_MASK(value, mask, 1, COUNTING_HW_ENABLE);
	mtk_dbi_count_write_mask(comp, value, REG_DBI_COUNTING_HW_ENABLE, mask, handle);
	mtk_dbi_count_write(comp, 0,
				REG_DBI_SAMPLING_BYPASS, handle);
	mtk_dbi_count_write(comp, 0,
				REG_DBI_COUNTING_BYPASS, handle);
	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, 0, REG_DBI_COUNT_BYPASS);
	mtk_dbi_count_write_mask(comp, value, DISP_DBI_COUNT_TOP_CTR_3, mask, handle);


	mtk_dbi_count_write(comp,0x00000001,0x7f0,handle);
}

static void mtk_dbi_count_common_init(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	uint32_t value = 0, mask = 0;
	unsigned int dsi_line_time = 0;
	unsigned int stash_lead_time = 12;
	unsigned int stash_lead_cnt = 0;
	unsigned int reg_val = 0;
	struct mtk_disp_dbi_count *dbi_count = comp_to_dbi_count(comp);
	struct mtk_ddp_comp *output_comp = NULL;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;

	SET_VAL_MASK(value, mask, 0x38, REG_VS_RE_GEN_CYC);
	mtk_dbi_count_write_mask(comp, value, DISP_DBI_COUNT_TOP_CTR_1, mask, handle);

	value = 0;mask = 0;
	SET_VAL_MASK(value, mask, 1, REG_VS_RE_GEN);
	mtk_dbi_count_write_mask(comp, value, DISP_DBI_COUNT_TOP_CTR_2, mask, handle);

	value = 0;mask = 0;
	SET_VAL_MASK(value, mask, 0, REG_CNT_CLK_FORCE_EN);
	SET_VAL_MASK(value, mask, 0, REG_SCL_CLK_FORCE_EN);
	SET_VAL_MASK(value, mask, 0, REG_SMP_CLK_FORCE_EN);
	mtk_dbi_count_write_mask(comp, value, REG_DBI_GATING, mask, handle);

	//close prtcl_prot
	value = 0;mask = 0;
	SET_VAL_MASK(value, mask, 1, REG_PRTCL_PROT_OFF_W);
	mtk_dbi_count_write_mask(comp, value, DISP_DBI_COUNT_UDMA_W_CTRL47, mask, handle);

	value = 0;mask = 0;
	SET_VAL_MASK(value, mask, 1, REG_PRTCL_PROT_OFF_R);
	mtk_dbi_count_write_mask(comp, value, DISP_DBI_COUNT_UDMA_R_CTR70, mask, handle);

	// ddren
	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, 0, REG_DBIW_DDREN_REQ_DISABLE);
	SET_VAL_MASK(value, mask, 1, REG_DBIW_SRT_DDREN_REQ);
	SET_VAL_MASK(value, mask, 0, REG_DBIW_STASH_DDREN_REQ_DISABLE);
	SET_VAL_MASK(value, mask, 1, REG_DBIW_STASH_SRT_DDREN_REQ);

	mtk_dbi_count_write_mask(comp, value, DISP_DBI_COUNT_DDREN_CTRL_DBIW, mask, handle);
	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, 0, REG_DBIR_DDREN_REQ_DISABLE);
	SET_VAL_MASK(value, mask, 1, REG_DBIR_SRT_DDREN_REQ);
	SET_VAL_MASK(value, mask, 0, REG_DBIR_STASH_DDREN_REQ_DISABLE);
	SET_VAL_MASK(value, mask, 1, REG_DBIR_STASH_SRT_DDREN_REQ);
	mtk_dbi_count_write_mask(comp, value, DISP_DBI_COUNT_DDREN_CTRL_DBIR, mask, handle);

	value = 0;mask = 0;
	SET_VAL_MASK(value, mask, 1, COUNT_INP_EN);
	mtk_dbi_count_write_mask(comp, value, REG_DBI_COUNT_OUTP_EN, mask, handle);

	//stash
	if (dbi_count->data->is_support_stash) {
		stash_lead_time = dbi_count->data->stash_lead_time;
		output_comp = mtk_ddp_comp_request_output(mtk_crtc);
		if (output_comp && (mtk_ddp_comp_get_type(output_comp->id) == MTK_DSI))
			mtk_ddp_comp_io_cmd(output_comp, NULL,
				DSI_GET_LINE_TIME_NS, &dsi_line_time);
		dsi_line_time /= 1000;
		if (dsi_line_time > 0)
			stash_lead_cnt = (stash_lead_time + dsi_line_time - 1) / dsi_line_time;
		reg_val = (1 << 8) | stash_lead_cnt;

		mtk_dbi_count_write(comp, reg_val,
			DISP_DBI_COUNT_UDMA_R_CTRL30, handle);
		mtk_dbi_count_write(comp, reg_val,
			DISP_DBI_COUNT_UDMA_W_CTR_1B, handle);
	}

	//side band
	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, 2, REG_CNTR_ULTRA_MODE);
	SET_VAL_MASK(value, mask, 2, REG_CNTW_ULTRA_MODE);
	SET_VAL_MASK(value, mask, 1, REG_CNTR_STASH_ULTRA_FRCE);
	SET_VAL_MASK(value, mask, 1, REG_CNTW_STASH_ULTRA_FRCE);
	mtk_dbi_count_write_mask(comp, value, REG_DBI_COUNT_EMI_SIDE_BAND, mask, handle);

}

static void mtk_dbi_count_change_mode(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_dbi_count *dbi_count = comp_to_dbi_count(comp);

		mtk_dbi_update_count_gain(comp, handle,
			dbi_count->current_bl, dbi_count->current_fps, dbi_count->current_temp);

		// set count mode
		mtk_dbi_set_reg_by_mode(comp, handle,
			&dbi_count->count_cfg.count_cfg.code_gain_packed, atomic_read(&dbi_count->current_count_mode));

}

static void mtk_dbi_count_config(struct mtk_ddp_comp *comp,
		struct mtk_ddp_config *cfg,
		struct cmdq_pkt *handle)
{
	uint32_t value = 0, mask = 0;
	struct mtk_disp_dbi_count *dbi_count = comp_to_dbi_count(comp);
	int mode_idx;

	DBI_COUNT_INFO("%s +++ %d\n", mtk_dump_comp_str(comp), dbi_count->data->need_bypass_shadow);

	if (dbi_count->data->need_bypass_shadow == true) {
		SET_VAL_MASK(value, mask, 1, BYPASS_SHADOW);
		mtk_dbi_count_write_mask(comp, value,
			DISP_DBI_COUNT_TOP_SHADOW_CTRL, mask, handle);
	}

	mtk_dbi_count_common_init(comp, handle);

	if(dbi_count->status >= DBI_COUNT_SW_INIT) {
		if(dbi_count->status == DBI_COUNT_SW_INIT)
			dbi_count->status = DBI_COUNT_HW_INIT;

		//set hw static cfg
		if(dbi_count->current_mode == HW_COUNTING_MODE)
			mode_idx = dbi_count->count_cfg.count_helper.static_hw_counting_mode_index;
		else
			mode_idx = dbi_count->count_cfg.count_helper.static_hw_sampling_mode_index;
		mtk_dbi_set_reg_by_mode(comp, handle,&dbi_count->count_cfg.count_static_cfg, mode_idx);

		// set dfmt temp use rgb

		mode_idx = mtk_dbi_count_get_mode_by_fmt(&dbi_count->count_cfg.count_helper,
			dbi_count->data_fmt);

		mtk_dbi_set_reg_by_mode(comp, handle,&dbi_count->count_cfg.count_dfmt_cfg, mode_idx);

		//update hw count gain
		mtk_dbi_update_count_gain(comp, handle,
			dbi_count->current_bl, dbi_count->current_fps, dbi_count->current_temp);

		// set count mode
		mtk_dbi_set_reg_by_mode(comp, handle,
			&dbi_count->count_cfg.count_cfg.code_gain_packed, atomic_read(&dbi_count->current_count_mode));

		if(dbi_count->irq_num && dbi_count->data->irq_handler) {
			value = 0;
			value |= DBI_COUNT_EOF;
			mtk_dbi_count_write_mask(comp, value,
				DISP_DBI_COUNT_IRQ_MASK, value, handle);
		}
	}
	mtk_dbi_count_bypass(comp, handle);
}

static void mtk_dbi_count_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DBI_COUNT_INFO("%s count_start\n", mtk_dump_comp_str(comp));
}

static void mtk_dbi_count_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_dbi_count *dbi_count = comp_to_dbi_count(comp);

	DBI_COUNT_INFO("%s count_stop\n", mtk_dump_comp_str(comp));
	dbi_count->qos_srt = 0;
}

int mtk_dbi_count_analysis(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_dbi_count *dbi_count = comp_to_dbi_count(comp);

	DDPDUMP("== %s ANALYSIS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	DDPDUMP("dbi_count init status %d\n", dbi_count->status);
	DDPDUMP("dbi_count current_bl %d\n", dbi_count->current_bl);
	DDPDUMP("dbi_count current_fps %d\n", dbi_count->current_fps);
	DDPDUMP("dbi_count current_freq %d\n", dbi_count->current_freq);
	DDPDUMP("dbi_count current_fmt 0x%x\n", dbi_count->data_fmt);

	return 0;
}

void mtk_dbi_count_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	void __iomem *mbaddr;
	int i;

	DDPDUMP("== %s REGS:%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	DDPDUMP("-- Start dump dbi count registers --\n");

	mbaddr = baddr;

	for (i = 0; i < 0x110; i += 16) {
		DDPDUMP("DBI_COUNT+%x: 0x%x 0x%x 0x%x 0x%x\n", i, readl(mbaddr + i),
		readl(mbaddr + i + 0x4), readl(mbaddr + i + 0x8),
		readl(mbaddr + i + 0xc));
	}

	for (i = 0x700; i < 0x830; i += 16) {
		DDPDUMP("DBI_COUNT+%x: 0x%x 0x%x 0x%x 0x%x\n", i, readl(mbaddr + i),
		readl(mbaddr + i + 0x4), readl(mbaddr + i + 0x8),
		readl(mbaddr + i + 0xc));
	}
	for (i = 0xD00; i < 0xD10; i += 16) {
		DDPDUMP("DBI_COUNT+%x: 0x%x 0x%x 0x%x 0x%x\n", i, readl(mbaddr + i),
		readl(mbaddr + i + 0x4), readl(mbaddr + i + 0x8),
		readl(mbaddr + i + 0xc));
	}
	for (i = 0xf00; i < 0xf10; i += 16) {
		DDPDUMP("DBI_COUNT+%x: 0x%x 0x%x 0x%x 0x%x\n", i, readl(mbaddr + i),
		readl(mbaddr + i + 0x4), readl(mbaddr + i + 0x8),
		readl(mbaddr + i + 0xc));
	}
}

static void mtk_dbi_count_prepare(struct mtk_ddp_comp *comp)
{
	DBI_COUNT_INFO("%s +++\n", mtk_dump_comp_str(comp));
	mtk_ddp_comp_clk_prepare(comp);
}

static void mtk_dbi_count_unprepare(struct mtk_ddp_comp *comp)
{

	DBI_COUNT_INFO("%s +++\n", mtk_dump_comp_str(comp));
	mtk_ddp_comp_clk_unprepare(comp);

}

static void mtk_dbi_count_srt_cal(struct mtk_ddp_comp *comp, int en, int slice_num)
{
	struct mtk_disp_dbi_count *dbi_count = comp_to_dbi_count(comp);
	unsigned int buffer_size = dbi_count->count_buffer.size;
	uint32_t vrefresh;
	uint32_t srt = 0;

	if (en) {
		srt = (buffer_size + slice_num -1)/slice_num;
		vrefresh = dbi_count->current_fps;
		//blanking ratio
		srt = DO_COMMON_DIV(srt, 1000);
		srt *= 125;
		srt = DO_COMMON_DIV(srt, 100);
		srt = srt * vrefresh;
		srt = DO_COMMON_DIV(srt, 1000);
		dbi_count->qos_srt = srt;
	} else
		dbi_count->qos_srt = 0;
}

static int mtk_dbi_count_get_mode_by_fmt(struct mtk_dbi_count_helper *helper, enum MTK_PANEL_SPR_MODE data_fmt)
{

	if(data_fmt == MTK_PANEL_RGBG_BGRG_TYPE)
		return helper->dfmt_rgbg_mode_index;
	else if(data_fmt == MTK_PANEL_BGRG_RGBG_TYPE)
		return helper->dfmt_bgrg_mode_index;
	else if(data_fmt == MTK_PANEL_SPR_OFF_TYPE)
		return helper->dfmt_rgb_mode_index;

	PC_ERR("%s:%d, fail %d\n", __func__, __LINE__, data_fmt);
	return -1;
}

void mtk_dbi_count_slot_trigger(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	uint32_t value = 0, mask = 0;

	GCE_COND_DECLARE;
	struct cmdq_operand lop, rop;
	const u16 var1 = CMDQ_THR_SPR_IDX2;
	const u16 var2 = 0;

	GCE_COND_ASSIGN(handle, CMDQ_THR_SPR_IDX1, CMDQ_GPR_R07);
	/* get dbi status */
	lop.reg = true;
	lop.idx = var1;
	rop.reg = false;
	rop.value = 1;

	cmdq_pkt_read(handle, NULL,
		mtk_get_gce_backup_slot_pa(comp->mtk_crtc, DISP_SLOT_DBI_COUNT_SW_TRIGGER), var1);
	cmdq_pkt_logic_command(handle, CMDQ_LOGIC_AND, var1, &lop, &rop);
	lop.reg = true;
	lop.idx = var1;
	rop.reg = false;
	rop.idx = var2;
	rop.value = 1;
	GCE_IF(lop, R_CMDQ_EQUAL, rop);
	/* condition true: DBI enabled, enable dbi ddren */
		cmdq_pkt_write(handle, comp->mtk_crtc->gce_obj.base,
				mtk_get_gce_backup_slot_pa(comp->mtk_crtc,
				DISP_SLOT_DBI_COUNT_SW_TRIGGER), 0, ~0);

	value = 0; mask = 0;
	SET_VAL_MASK(value, mask, 1, SAMPLING_PQ_SINGLE_TRIGGER);
	mtk_dbi_count_write_mask(comp, value, REG_DBI_SAMPLING_PQ_SINGLE_TRIGGER_SW_EN, mask, handle);

	GCE_FI;
}


static void mtk_dbi_count_config_trigger(struct mtk_ddp_comp *comp,
				   struct cmdq_pkt *handle,
				   enum mtk_ddp_comp_trigger_flag flag)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;

	if (!mtk_crtc) {
		PC_ERR("%s dbi_count comp not configure CRTC yet\n", __func__);
		return;
	}
	if (!mtk_crtc->base.dev)
		return;

	switch (flag) {
	case MTK_TRIG_FLAG_PRE_TRIGGER:
	{
		mtk_dbi_count_slot_trigger(comp, handle);
	}
		break;

	default:
		break;
	}
}


int mtk_dbi_count_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
		enum mtk_ddp_io_cmd cmd, void *params)
{
	struct mtk_drm_private *priv;
	struct mtk_disp_dbi_count *dbi_count = comp_to_dbi_count(comp);

	if (!(comp->mtk_crtc && comp->mtk_crtc->base.dev))
		return -INVALID;

	priv = comp->mtk_crtc->base.dev->dev_private;

	switch (cmd) {
	case DISP_SPR_SWITCH:
	{
		unsigned int spr_on = *(unsigned int *)params;
		int mode_idx;

		if(spr_on)
			dbi_count->data_fmt = comp->mtk_crtc->panel_ext->params->spr_params.spr_format_type;
		else
			dbi_count->data_fmt = MTK_PANEL_SPR_OFF_TYPE;

		if(!handle)
			break;

		if(dbi_count->status < DBI_COUNT_SW_INIT)
			break;


		mode_idx = mtk_dbi_count_get_mode_by_fmt(&dbi_count->count_cfg.count_helper,
			dbi_count->data_fmt);
		if(mode_idx >= 0)
			mtk_dbi_set_reg_by_mode(comp, handle,&dbi_count->count_cfg.count_dfmt_cfg, mode_idx);

	}
		break;
	case PQ_FILL_COMP_PIPE_INFO:
	{
		struct drm_display_mode *mode;

		mode = mtk_crtc_get_display_mode_by_comp(__func__, &comp->mtk_crtc->base, comp, false);
		dbi_count->current_fps = drm_mode_vrefresh(mode);
	}
		break;
	case DISP_BL_CHG:
	{
		dbi_count->current_bl = *(uint32_t *)params;
		mtk_dbi_update_count_gain(comp, handle,
			dbi_count->current_bl, dbi_count->current_fps, dbi_count->current_temp);
	}
		break;

	case PMQOS_GET_LARB_PORT_HRT_BW: {
		struct mtk_larb_port_bw *data = (struct mtk_larb_port_bw *)params;
		int weight = 0;
		unsigned int bw_base = data->bw_base;

		data->larb_id = -1;
		data->bw = 0;
		if (data->type != CHANNEL_HRT_READ && data->type != CHANNEL_HRT_WRITE)
			break;

		if (IS_ERR_OR_NULL(comp->larb_ids))
			data->larb_id = comp->larb_id;
		else
			data->larb_id = comp->larb_ids[0];

		if (data->larb_id < 0)
			break;
		mtk_dbi_count_hrt_cal_ratio(comp, data->type, &weight);
		if (weight > 0) {
			if (!bw_base)
				bw_base = mtk_drm_primary_frame_bw(&comp->mtk_crtc->base);
			data->bw = (bw_base * weight + 399) / 400;
			DDPINFO("%s, dbi_count comp:%d, larb:%d, type:%d, bw:%d, weight:%d\n",
				__func__, comp->id, data->larb_id, data->type, data->bw, weight);
		}
		break;
	}

	case ODDMR_TIMING_CHG:
	{
		struct mtk_oddmr_timing *timing = (struct mtk_oddmr_timing *)params;

		dbi_count->current_fps = timing->vrefresh;
		mtk_dbi_update_count_gain(comp, handle,
			dbi_count->current_bl, dbi_count->current_fps, dbi_count->current_temp);
	}
		break;

	case PMQOS_SET_HRT_BW_DELAY:
	{
		u32 bw_val, bw_base = *(unsigned int *)params;
		struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;
		u32 stash_bw = 17;
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		struct drm_crtc *crtc = &mtk_crtc->base;
		struct mtk_crtc_state *crtc_state = to_mtk_crtc_state(crtc->state);
		uint32_t en = crtc_state->prop_val[CRTC_PROP_DBI_COUNT_ENABLE];
		int weight= 0;

		if (!mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_MMQOS_SUPPORT))
			break;

		if (!priv->data->respective_ostdl) {
			PC_ERR("respective_ostdl do not set\n");
			break;
		}
		if (!handle) {
			PC_ERR("no cmdq handle\n");
			break;
		}

		en = !!bw_base && en;

		mtk_dbi_count_hrt_cal_ratio(comp, CHANNEL_HRT_RW, &weight);
		bw_val = (weight * bw_base + 399) / 400;
		bw_val *= (en > 0) ? 1 : 0;
		if (bw_val > dbi_count->last_hrt) {
			__mtk_disp_set_module_hrt(dbi_count->qos_req_w_hrt, comp->id, bw_val,
				priv->data->respective_ostdl);
			__mtk_disp_set_module_hrt(dbi_count->qos_req_r_hrt, comp->id, bw_val,
				priv->data->respective_ostdl);
			cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
				mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_CUR_HRT_VAL_DBI_COUNT),
				NO_PENDING_HRT, ~0);
			if(dbi_count->data->is_support_stash ){
				if(bw_val) {
					stash_bw = ((bw_val / 256) > dbi_count->data->min_stash_port_bw ?
						((bw_val / 256)) : dbi_count->data->min_stash_port_bw);
					__mtk_disp_set_module_hrt(dbi_count->qos_req_r_stash,
							comp->id, stash_bw, priv->data->respective_ostdl);
					__mtk_disp_set_module_hrt(dbi_count->qos_req_w_stash,
							comp->id, stash_bw, priv->data->respective_ostdl);
				} else {
					__mtk_disp_set_module_hrt(dbi_count->qos_req_r_stash,
							comp->id, 0, priv->data->respective_ostdl);
					__mtk_disp_set_module_hrt(dbi_count->qos_req_w_stash,
							comp->id, 0, priv->data->respective_ostdl);
				}
			}
		} else if (bw_val < dbi_count->last_hrt) {
			cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
				mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_CUR_HRT_VAL_DBI_COUNT),
				bw_val, ~0);
		}
		dbi_count->last_hrt = bw_val;
	}
		break;

	case PMQOS_SET_HRT_BW_DELAY_POST:
	{
		u32 bw_val = 0;
		u32 stash_bw = dbi_count->data->min_stash_port_bw;
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

		if (!mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_MMQOS_SUPPORT))
			break;

		if (!priv->data->respective_ostdl) {
			PC_ERR("respective_ostdl do not set\n");
			break;
		}

		bw_val = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
			DISP_SLOT_CUR_HRT_VAL_DBI_COUNT);
		if (bw_val != NO_PENDING_HRT && bw_val >= dbi_count->last_hrt) {
			__mtk_disp_set_module_hrt(dbi_count->qos_req_r_hrt, comp->id, bw_val,
				priv->data->respective_ostdl);
			__mtk_disp_set_module_hrt(dbi_count->qos_req_w_hrt, comp->id, bw_val,
				priv->data->respective_ostdl);
			*(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_CUR_HRT_VAL_DBI_COUNT) =	NO_PENDING_HRT;
			if(dbi_count->data->is_support_stash){
				if(bw_val) {
					stash_bw = ((bw_val / 256) > dbi_count->data->min_stash_port_bw ?
						((bw_val / 256)) : dbi_count->data->min_stash_port_bw);
					__mtk_disp_set_module_hrt(dbi_count->qos_req_r_stash,
							comp->id, stash_bw, priv->data->respective_ostdl);
					__mtk_disp_set_module_hrt(dbi_count->qos_req_w_stash,
							comp->id, stash_bw, priv->data->respective_ostdl);
				} else {
					__mtk_disp_set_module_hrt(dbi_count->qos_req_r_stash,
							comp->id, 0, priv->data->respective_ostdl);
					__mtk_disp_set_module_hrt(dbi_count->qos_req_w_stash,
							comp->id, 0, priv->data->respective_ostdl);
				}
			}
		}
	}
		break;

	case PMQOS_SET_HRT_BW:
	{
		u32 bw_val, bw_base = *(unsigned int *)params;
		struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;
		u32 stash_bw = 17;
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		struct drm_crtc *crtc = &mtk_crtc->base;
		struct mtk_crtc_state *crtc_state = to_mtk_crtc_state(crtc->state);
		uint32_t en = crtc_state->prop_val[CRTC_PROP_DBI_COUNT_ENABLE];
		int weight= 0;

		if (!mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_MMQOS_SUPPORT))
			break;

		en = !!bw_base && en;
		if (priv->data->respective_ostdl) {
			/* DBI outstanding */
			mtk_dbi_count_hrt_cal_ratio(comp, CHANNEL_HRT_RW, &weight);
			bw_val = (weight * bw_base + 399) / 400;
			bw_val *= (en > 0) ? 1 : 0;
			if(bw_val == dbi_count->last_hrt)
				break;
			__mtk_disp_set_module_hrt(dbi_count->qos_req_w_hrt, comp->id, bw_val,
				priv->data->respective_ostdl);
			__mtk_disp_set_module_hrt(dbi_count->qos_req_r_hrt, comp->id, bw_val,
				priv->data->respective_ostdl);
			dbi_count->last_hrt = bw_val;
			if(dbi_count->data->is_support_stash){
				if(bw_val) {
					stash_bw = ((bw_val / 256) > dbi_count->data->min_stash_port_bw ?
						((bw_val / 256)) : dbi_count->data->min_stash_port_bw);
					__mtk_disp_set_module_hrt(dbi_count->qos_req_w_stash,
							comp->id, stash_bw, priv->data->respective_ostdl);
					__mtk_disp_set_module_hrt(dbi_count->qos_req_r_stash,
							comp->id, stash_bw, priv->data->respective_ostdl);
				} else {
					__mtk_disp_set_module_hrt(dbi_count->qos_req_w_stash,
							comp->id, 0, priv->data->respective_ostdl);
					__mtk_disp_set_module_hrt(dbi_count->qos_req_r_stash,
							comp->id, 0, priv->data->respective_ostdl);
				}
			}
		} else {
			dbi_count->last_hrt = en;
			__mtk_disp_set_module_hrt(dbi_count->qos_req_w_hrt,
						comp->id, en, priv->data->respective_ostdl);
			__mtk_disp_set_module_hrt(dbi_count->qos_req_r_hrt,
						comp->id, en, priv->data->respective_ostdl);
		}
	}
		break;

	case PMQOS_UPDATE_BW:
	{
		struct mtk_drm_private *priv =
			comp->mtk_crtc->base.dev->dev_private;
		unsigned int force_update = 0; /* force_update repeat last qos BW */
		unsigned int update_pending = 0;
		unsigned int crtc_idx = 0;
		struct drm_crtc *crtc;
		struct mtk_drm_crtc *mtk_crtc;
		unsigned int channel_id = 1;

		if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT))
			break;

		mtk_crtc = comp->mtk_crtc;
		crtc = &mtk_crtc->base;
		crtc_idx = drm_crtc_index(crtc);

		if (params) {
			force_update = *(unsigned int *)params;
			update_pending = (force_update == DISP_BW_UPDATE_PENDING);
			force_update = (force_update == DISP_BW_FORCE_UPDATE) ? 1 : 0;
		}

		if (!force_update && !update_pending) {
			mtk_crtc->total_srt += (dbi_count->qos_srt * 2);
			if (channel_id < 4)
				priv->srt_channel_bw_sum[crtc_idx][channel_id] += dbi_count->qos_srt;
			if (channel_id < 4)
				priv->srt_channel_write_bw_sum[crtc_idx][channel_id] += dbi_count->qos_srt;
		}

		if (force_update || dbi_count->last_qos_srt != dbi_count->qos_srt) {
			__mtk_disp_set_module_srt(dbi_count->qos_req_w, comp->id,
				dbi_count->qos_srt, 0, DISP_BW_NORMAL_MODE,
				priv->data->real_srt_ostdl);
			__mtk_disp_set_module_srt(dbi_count->qos_req_r, comp->id,
				dbi_count->qos_srt, 0, DISP_BW_NORMAL_MODE,
				priv->data->real_srt_ostdl);
			dbi_count->last_qos_srt = dbi_count->qos_srt;
			if (!force_update && update_pending) {
				comp->mtk_crtc->total_srt += dbi_count->qos_srt;
				if (channel_id < 4)
					priv->srt_channel_bw_sum[crtc_idx][channel_id] += dbi_count->qos_srt;
				if (channel_id < 4)
					priv->srt_channel_write_bw_sum[crtc_idx][channel_id] += dbi_count->qos_srt;
			}
		}
	}
		break;

	default:
		break;
	}

	if (!(comp->mtk_crtc && comp->mtk_crtc->base.dev))
		return -INVALID;

	return 0;
}


static bool mtk_dbi_count_load_curve(struct mtk_dbi_curve_2d *curve , void **data,int *index)
{
	int size = curve->num * sizeof(int32_t);

	if(size<=0)
		return true;

	data[*index] = vmalloc(size);
	if (!data[*index]) {
		PC_ERR("%s:%d dbi count init fail\n", __func__, __LINE__);
		goto fail;
	}
	if (copy_from_user(data[*index],
		curve->x, size)) {
		PC_ERR("%s:%d, copy_from_user fail\n", __func__, __LINE__);
		goto fail;
	}
	curve->x = (uint32_t *)data[*index];
	*index = (*index) +1;

	data[*index] = vmalloc(size);
	if (!data[*index]) {
		PC_ERR("%s:%d dbi count init fail\n", __func__, __LINE__);
		goto fail;
	}
	if (copy_from_user(data[*index],
		curve->y, size)) {
		PC_ERR("%s:%d, copy_from_user fail\n", __func__, __LINE__);
		goto fail;
	}
	curve->y = (uint32_t *)data[*index];
	*index = (*index) +1;
	return true;

fail:
	return false;

}


static int mtk_dbi_count_init(struct mtk_ddp_comp *comp, struct mtk_drm_dbi_cfg_info *cfg_info)
{
	struct mtk_disp_dbi_count *dbi_count = comp_to_dbi_count(comp);
	struct mtk_drm_dbi_cfg_info *count_cfg;
	void **data = NULL;
	int max_len = 300;
	int size = 0;
	int index = 0;
	bool ret;

	DBI_COUNT_INFO("+\n");

	if (dbi_count->status != DBI_COUNT_INVALID) {
		if(dbi_count->status >= DBI_COUNT_SW_INIT) {
			DBI_COUNT_MSG("dbi already inited, state %d\n", dbi_count->status);
			return 0;
		}
		DBI_COUNT_MSG("dbi can not init, state %d\n", dbi_count->status);
		return -1;
	}
	count_cfg = &dbi_count->count_cfg;
	memcpy(count_cfg, cfg_info, sizeof(struct mtk_drm_dbi_cfg_info));

	data = vmalloc(sizeof(void *) * max_len);
	DBI_COUNT_MSG("dbi can not init, state %lu\n", sizeof(void *) * max_len);
	if(!data) {
		PC_ERR("%s:%d dbi count init fail\n", __func__, __LINE__);
		return -1;
	}

	if(count_cfg->count_cfg.hw_count_param_num){
		for(int i=0;i<count_cfg->count_cfg.hw_count_param_num;i++){
			ret = mtk_dbi_count_load_curve(
				&count_cfg->count_cfg.hw_count_param[i].dbv_gain_curve[0], data, &index);
			if(!ret){
				PC_ERR("%s:%d, mtk_dbi_count_load_curve fail %d\n", __func__, __LINE__, i);
				goto fail;
			}
			ret = mtk_dbi_count_load_curve(
				&count_cfg->count_cfg.hw_count_param[i].dbv_gain_curve[1], data, &index);
			if(!ret){
				PC_ERR("%s:%d, mtk_dbi_count_load_curve fail %d\n", __func__, __LINE__, i);
				goto fail;
			}
			ret = mtk_dbi_count_load_curve(
				&count_cfg->count_cfg.hw_count_param[i].dbv_gain_curve[2], data, &index);
			if(!ret){
				PC_ERR("%s:%d, mtk_dbi_count_load_curve fail %d\n", __func__, __LINE__, i);
				goto fail;
			}
			ret = mtk_dbi_count_load_curve(
				&count_cfg->count_cfg.hw_count_param[i].fps_gain_curve[0], data, &index);
			if(!ret){
				PC_ERR("%s:%d, mtk_dbi_count_load_curve fail %d\n", __func__, __LINE__, i);
				goto fail;
			}
			ret = mtk_dbi_count_load_curve(
				&count_cfg->count_cfg.hw_count_param[i].fps_gain_curve[1], data, &index);
			if(!ret){
				PC_ERR("%s:%d, mtk_dbi_count_load_curve fail %d\n", __func__, __LINE__, i);
				goto fail;
			}
			ret = mtk_dbi_count_load_curve(
				&count_cfg->count_cfg.hw_count_param[i].fps_gain_curve[2], data, &index);
			if(!ret){
				PC_ERR("%s:%d, mtk_dbi_count_load_curve fail %d\n", __func__, __LINE__, i);
				goto fail;
			}
			ret = mtk_dbi_count_load_curve(
				&count_cfg->count_cfg.hw_count_param[i].temp_gain_curve[0], data, &index);
			if(!ret){
				PC_ERR("%s:%d, mtk_dbi_count_load_curve fail %d\n", __func__, __LINE__, i);
				goto fail;
			}
			ret = mtk_dbi_count_load_curve(
				&count_cfg->count_cfg.hw_count_param[i].temp_gain_curve[1], data, &index);
			if(!ret){
				PC_ERR("%s:%d, mtk_dbi_count_load_curve fail %d\n", __func__, __LINE__, i);
				goto fail;
			}
			ret = mtk_dbi_count_load_curve(
				&count_cfg->count_cfg.hw_count_param[i].temp_gain_curve[2], data, &index);
			if(!ret){
				PC_ERR("%s:%d, mtk_dbi_count_load_curve fail %d\n", __func__, __LINE__, i);
				goto fail;
			}

			if(count_cfg->count_cfg.hw_count_param[i].irdrop_enable) {
				ret = mtk_dbi_count_load_curve(
					&count_cfg->count_cfg.hw_count_param[i].irdrop_total_gain_curve, data, &index);
				if(!ret){
					PC_ERR("%s:%d, mtk_dbi_count_load_curve fail %d\n", __func__, __LINE__, i);
					goto fail;
				}
				ret = mtk_dbi_count_load_curve(
					&count_cfg->count_cfg.hw_count_param[i].irdrop_ratio_gain_curve[0],
					data, &index);
				if(!ret){
					PC_ERR("%s:%d, mtk_dbi_count_load_curve fail %d\n", __func__, __LINE__, i);
					goto fail;
				}
				ret = mtk_dbi_count_load_curve(
					&count_cfg->count_cfg.hw_count_param[i].irdrop_ratio_gain_curve[1],
					data, &index);
				if(!ret){
					PC_ERR("%s:%d, mtk_dbi_count_load_curve fail %d\n", __func__, __LINE__, i);
					goto fail;
				}
				ret = mtk_dbi_count_load_curve(
					&count_cfg->count_cfg.hw_count_param[i].irdrop_ratio_gain_curve[2],
					data, &index);
				if(!ret){
					PC_ERR("%s:%d, mtk_dbi_count_load_curve fail %d\n", __func__, __LINE__, i);
					goto fail;
				}
				ret = mtk_dbi_count_load_curve(
					&count_cfg->count_cfg.hw_count_param[i].irdrop_dbv_gain_curve[0], data, &index);
				if(!ret){
					PC_ERR("%s:%d, mtk_dbi_count_load_curve fail %d\n", __func__, __LINE__, i);
					goto fail;
				}
				ret = mtk_dbi_count_load_curve(
					&count_cfg->count_cfg.hw_count_param[i].irdrop_dbv_gain_curve[1], data, &index);
				if(!ret){
					PC_ERR("%s:%d, mtk_dbi_count_load_curve fail %d\n", __func__, __LINE__, i);
					goto fail;
				}
				ret = mtk_dbi_count_load_curve(
					&count_cfg->count_cfg.hw_count_param[i].irdrop_dbv_gain_curve[2], data, &index);
				if(!ret){
					PC_ERR("%s:%d, mtk_dbi_count_load_curve fail %d\n", __func__, __LINE__, i);
					goto fail;
				}
			}
		}
	}

	if (count_cfg->count_cfg.code_gain_packed.reg_num) {
		size = sizeof(uint32_t) * count_cfg->count_cfg.code_gain_packed.reg_num;

		data[index] = vmalloc(size);
		if (!data[index]) {
			PC_ERR("%s:%d dbi count init fail\n", __func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			count_cfg->count_cfg.code_gain_packed.addr, size)) {
			PC_ERR("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		count_cfg->count_cfg.code_gain_packed.addr = (uint32_t *)data[index];
		index ++;

		data[index] = vmalloc(size);
		if (!data[index]) {
			PC_ERR("%s:%d dbi count init fail\n", __func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			count_cfg->count_cfg.code_gain_packed.mask, size)) {
			PC_ERR("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		count_cfg->count_cfg.code_gain_packed.mask= (uint32_t *)data[index];
		index ++;

		size = sizeof(uint32_t) * count_cfg->count_cfg.code_gain_packed.reg_num *
			count_cfg->count_cfg.code_gain_packed.mode_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			PC_ERR("%s:%d dbi count init fail\n", __func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			count_cfg->count_cfg.code_gain_packed.mode_value, size)) {
			PC_ERR("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		count_cfg->count_cfg.code_gain_packed.mode_value= (uint32_t *)data[index];
		index ++;
	}


	if (count_cfg->count_static_cfg.reg_num) {
		size = sizeof(uint32_t) * count_cfg->count_static_cfg.reg_num;

		data[index] = vmalloc(size);
		if (!data[index]) {
			PC_ERR("%s:%d dbi count init fail\n", __func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			count_cfg->count_static_cfg.addr, size)) {
			PC_ERR("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		count_cfg->count_static_cfg.addr = (uint32_t *)data[index];
		index ++;

		data[index] = vmalloc(size);
		if (!data[index]) {
			PC_ERR("%s:%d dbi count init fail\n", __func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			count_cfg->count_static_cfg.mask, size)) {
			PC_ERR("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		count_cfg->count_static_cfg.mask= (uint32_t *)data[index];
		index ++;

		size = sizeof(uint32_t) * count_cfg->count_static_cfg.reg_num * count_cfg->count_static_cfg.mode_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			PC_ERR("%s:%d dbi count init fail\n", __func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			count_cfg->count_static_cfg.mode_value, size)) {
			PC_ERR("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		count_cfg->count_static_cfg.mode_value= (uint32_t *)data[index];
		index ++;
	}

	if (count_cfg->count_dfmt_cfg.reg_num) {
		size = sizeof(uint32_t) * count_cfg->count_dfmt_cfg.reg_num;

		data[index] = vmalloc(size);
		if (!data[index]) {
			PC_ERR("%s:%d dbi count init fail\n", __func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			count_cfg->count_dfmt_cfg.addr, size)) {
			PC_ERR("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		count_cfg->count_dfmt_cfg.addr = (uint32_t *)data[index];
		index ++;

		data[index] = vmalloc(size);
		if (!data[index]) {
			PC_ERR("%s:%d dbi count init fail\n", __func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			count_cfg->count_dfmt_cfg.mask, size)) {
			PC_ERR("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		count_cfg->count_dfmt_cfg.mask= (uint32_t *)data[index];
		index ++;

		size = sizeof(uint32_t) * count_cfg->count_dfmt_cfg.reg_num * count_cfg->count_dfmt_cfg.mode_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			PC_ERR("%s:%d dbi count init fail\n", __func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			count_cfg->count_dfmt_cfg.mode_value, size)) {
			PC_ERR("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		count_cfg->count_dfmt_cfg.mode_value= (uint32_t *)data[index];
		index ++;
	}

	dbi_count->current_mode = HW_COUNTING_MODE;
	dbi_count->status = DBI_COUNT_SW_INIT;

	if(data)
		vfree(data);
	return 0;

fail:
	PC_ERR("%s: dbi count init fail\n", __func__);
	for (int i = 0; i < max_len; i++) {
		if (data[i])
			vfree(data[i]);
	}
	if(data)
		vfree(data);
	return -EFAULT;

}

static int mtk_dbi_count_load_buffer_cfg(struct mtk_ddp_comp *comp, struct mtk_dbi_count_buf_cfg *cfg)
{
	struct mtk_disp_dbi_count *dbi_count = comp_to_dbi_count(comp);
	struct mtk_dbi_count_buf_cfg *buf_cfg;
	void *data[10] = {0};
	int size = 0;
	int index = 0;

	buf_cfg = &dbi_count->buffer_cfg;
	buf_cfg->sw_timer_ms = cfg->sw_timer_ms;
	dbi_count->buffer_time = 0;

	DBI_COUNT_INFO("+ reg_num %d, sw_timer_ms %d\n", buf_cfg->buf_reg_list.reg_num, buf_cfg->sw_timer_ms);
	if(buf_cfg->buf_reg_list.reg_num) {
		size = sizeof(uint32_t) * buf_cfg->buf_reg_list.reg_num;
		data[0] = (void *)buf_cfg->buf_reg_list.value;
		data[1] = (void *)buf_cfg->buf_reg_list.mask;
		data[2] = (void *)buf_cfg->buf_reg_list.addr;
		if (copy_from_user(buf_cfg->buf_reg_list.value,
			cfg->buf_reg_list.value, size)) {
			PC_ERR("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}

		if (copy_from_user(buf_cfg->buf_reg_list.mask,
			cfg->buf_reg_list.mask, size)) {
			PC_ERR("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}

		if (copy_from_user(buf_cfg->buf_reg_list.addr,
			cfg->buf_reg_list.addr, size)) {
			PC_ERR("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}

		return 0;
	}
	memcpy(buf_cfg, cfg, sizeof(struct mtk_dbi_count_buf_cfg));

	if(buf_cfg->buf_reg_list.reg_num) {
		size = sizeof(uint32_t) * buf_cfg->buf_reg_list.reg_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			PC_ERR("%s:%d dbi count init fail\n", __func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			buf_cfg->buf_reg_list.value, size)) {
			PC_ERR("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		buf_cfg->buf_reg_list.value = (uint32_t *)data[index];
		index ++;

		data[index] = vmalloc(size);
		if (!data[index]) {
			PC_ERR("%s:%d dbi count init fail\n", __func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			buf_cfg->buf_reg_list.addr, size)) {
			PC_ERR("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		buf_cfg->buf_reg_list.addr = (uint32_t *)data[index];
		index ++;

		data[index] = vmalloc(size);
		if (!data[index]) {
			PC_ERR("%s:%d dbi count init fail\n", __func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			buf_cfg->buf_reg_list.mask, size)) {
			PC_ERR("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		buf_cfg->buf_reg_list.mask = (uint32_t *)data[index];
		index ++;
	}

	return 0;

fail:
	PC_ERR("%s: dbi count init fail\n", __func__);
	for (int i = 0; i<ARRAY_SIZE(data); i++) {
		if (data[i])
			vfree(data[i]);
	}
	memset(buf_cfg, 0, sizeof(struct mtk_dbi_count_buf_cfg));
	return -EFAULT;

}


static int mtk_dbi_count_pq_ioctl_transact(struct mtk_ddp_comp *comp,
	unsigned int cmd, void *params, unsigned int size)
{
	int ret = -1;

	DBI_COUNT_INFO("+ cmd %d", cmd);

	switch (cmd) {
	case PQ_DBI_COUNT_IDLE_TIMER_INIT:
		ret = mtk_dbi_count_create_timer(comp, params, true, true);
		break;
	case PQ_DBI_COUNT_IDLE_TIMER_DELETE:
		ret = mtk_dbi_count_delete_timer(comp, true ,false);
		break;
	case PQ_DBI_COUNT_GET_EVENT:
		ret = mtk_dbi_count_wait_event(comp, params);
		break;
	case PQ_DBI_COUNT_WAIT_DISABLE_FINISH:
		ret = mtk_dbi_count_wait_disable_finish(comp, params);
		break;
	case PQ_DBI_COUNT_WAIT_NEW_FRAME:
		ret = mtk_dbi_count_wait_new_frame(comp, params);
		break;
	case PQ_DBI_COUNT_CLEAR_EVENT:
		ret = mtk_dbi_count_clear_event(comp, params);
		break;
	case PQ_DBI_COUNT_CHECK_BUFFER:
		ret = mtk_dbi_count_check_buffer(comp, params);
		break;
	case PQ_DBI_COUNT_LOAD_BUFFER:
		ret = mtk_dbi_count_load_buffer(comp, params);
		break;

	case PQ_DBI_COUNT_LOAD_PARAM:
		ret = mtk_dbi_count_init(comp, (struct mtk_drm_dbi_cfg_info *)params);
		break;

	case PQ_DBI_COUNT_LOAD_BUFFER_CFG:
		ret = mtk_dbi_count_load_buffer_cfg(comp, (struct mtk_dbi_count_buf_cfg *)params);
		break;
	case PQ_DBI_COUNT_SET_FREQ:
		ret = mtk_dbi_count_set_freq(comp, params);
		break;
	case PQ_DBI_COUNT_SET_TEMP:
		ret = mtk_dbi_count_set_temp(comp, params);
		break;
	case PQ_DBI_COUNT_SET_COUNTING_MODE:
		ret = mtk_dbi_count_set_counting_mode(comp, params);
		break;

	default:
		break;
	}
	return ret;
}

static void mtk_dbi_count_first_cfg(struct mtk_ddp_comp *comp,
		struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	struct mtk_disp_dbi_count *dbi_count = comp_to_dbi_count(comp);

	DDPMSG("%s+\n", __func__);

	if((comp->mtk_crtc->panel_ext->params->spr_params.enable == 1) &&
		(comp->mtk_crtc->panel_ext->params->spr_params.relay == 0))
		dbi_count->data_fmt = comp->mtk_crtc->panel_ext->params->spr_params.spr_format_type;
	else
		dbi_count->data_fmt = MTK_PANEL_SPR_OFF_TYPE;

	DDPMSG("%s data_fmt %d-\n", __func__, dbi_count->data_fmt);
}

static const struct mtk_ddp_comp_funcs mtk_disp_dbi_count_funcs = {
	.config = mtk_dbi_count_config,
	.start = mtk_dbi_count_start,
	.stop = mtk_dbi_count_stop,
	.prepare = mtk_dbi_count_prepare,
	.unprepare = mtk_dbi_count_unprepare,
	.io_cmd = mtk_dbi_count_io_cmd,
	.pq_ioctl_transact = mtk_dbi_count_pq_ioctl_transact,
	.first_cfg = mtk_dbi_count_first_cfg,
	.config_trigger = mtk_dbi_count_config_trigger,
};


static int mtk_disp_dbi_count_bind(struct device *dev, struct device *master,
		void *data)
{
	struct mtk_disp_dbi_count *dbi_count = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct mtk_drm_private *private = drm_dev->dev_private;
	int ret;
	char buf[50];

	pr_notice("%s+\n", __func__);
	ret = mtk_ddp_comp_register(drm_dev, &dbi_count->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
				dev->of_node->full_name, ret);
		return ret;
	}
	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_MMQOS_SUPPORT)) {
		dbi_count = comp_to_dbi_count(&dbi_count->ddp_comp);
		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&dbi_count->ddp_comp, "W");
		dbi_count->qos_req_w = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&dbi_count->ddp_comp, "W_HRT");
		dbi_count->qos_req_w_hrt= of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&dbi_count->ddp_comp, "W_STASH");
		dbi_count->qos_req_w_stash = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&dbi_count->ddp_comp, "R");
		dbi_count->qos_req_r = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&dbi_count->ddp_comp, "R_HRT");
		dbi_count->qos_req_r_hrt= of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&dbi_count->ddp_comp, "R_STASH");
		dbi_count->qos_req_r_stash = of_mtk_icc_get(dev, buf);
	}
	pr_notice("%s-\n", __func__);
	return 0;
}

static void mtk_disp_dbi_count_unbind(struct device *dev, struct device *master,
		void *data)
{
	struct mtk_disp_dbi_count *dbi_count = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	pr_notice("%s+\n", __func__);
	mtk_ddp_comp_unregister(drm_dev, &dbi_count->ddp_comp);
}

static const struct component_ops mtk_disp_dbi_count_component_ops = {
	.bind = mtk_disp_dbi_count_bind,
	.unbind = mtk_disp_dbi_count_unbind,
};


static int mtk_disp_dbi_count_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_dbi_count *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret, irq_num;

	DDPMSG("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_DBI_COUNT);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_dbi_count_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	priv->data = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	irq_num = platform_get_irq(pdev, 0);
	if (irq_num < 0 || !priv->data->irq_handler)
		dev_err(&pdev->dev, "%s failed to request dbi_count irq resource\n", __func__);
	else {
		priv->irq_num = irq_num;
		irq_set_status_flags(irq_num, IRQ_TYPE_LEVEL_HIGH);
		ret = devm_request_irq(
			&pdev->dev, irq_num, priv->data->irq_handler,
			IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(&pdev->dev), priv);
		if (ret) {
			DDPAEE("%s:%d, failed to request irq:%d ret:%d\n",
					__func__, __LINE__,
					irq_num, ret);
			ret = -EPROBE_DEFER;
			return ret;
		}
	}

	ret = component_add(dev, &mtk_disp_dbi_count_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	atomic_set(&priv->buffer_full, 0);
	atomic_set(&priv->current_count_mode, 0);
	atomic_set(&priv->new_count_mode, 0);

	DDPMSG("%s-\n", __func__);
	return ret;
}

static void mtk_disp_dbi_count_remove(struct platform_device *pdev)
{
	struct mtk_disp_dbi_count *dbi_count = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_dbi_count_component_ops);
	mtk_ddp_comp_pm_disable(&dbi_count->ddp_comp);
}


static irqreturn_t mtk_dbi_count_irq_handler(int irq, void *dev_id)
{
	struct mtk_disp_dbi_count *dbi_count = dev_id;
	uint32_t status_raw, status = 0;
	struct mtk_ddp_comp *comp;
	uint32_t value,mask;

	if (IS_ERR_OR_NULL(dbi_count))
		return IRQ_NONE;

	comp = &dbi_count->ddp_comp;
	if (mtk_drm_top_clk_isr_get(comp) == false) {
		DDPIRQ("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}

	status_raw = mtk_dbi_count_read(comp, DISP_DBI_COUNT_IRQ_RAW_STATUS);
	status = mtk_dbi_count_read(comp, DISP_DBI_COUNT_IRQ_STATUS);
	mtk_dbi_count_write(comp, status, DISP_DBI_COUNT_IRQ_CLR, NULL);
	mtk_dbi_count_write(comp, 0, DISP_DBI_COUNT_IRQ_CLR, NULL);
	DDPIRQ("%s %s irq, val:0x%x,0x%x\n", __func__, mtk_dump_comp_str(comp),
		status_raw, status);
	if(status & DBI_COUNT_INT_DONE) {
		atomic_set(&dbi_count->buffer_full, 1);
		CRTC_MMP_MARK(0, dbi_merge, 0, 0);
	}

	if(status & DBI_COUNT_EOF){
		*(unsigned int *)mtk_get_gce_backup_slot_va(comp->mtk_crtc,
			DISP_SLOT_DBI_COUNT_ERROR) = 1;
		cmdq_set_event(comp->mtk_crtc->gce_obj.client[CLIENT_CFG]->chan,
			comp->mtk_crtc->gce_obj.event[EVENT_DBI_COUNT_EOF]);
		CRTC_MMP_MARK(0, dbi_drop, 0, comp->mtk_crtc->gce_obj.event[EVENT_DBI_COUNT_EOF]);
	}

	if(status & DBI_COUNT_FRAME_DONE) {

		value = 0;
		mask = 0;
		mask |= DBI_COUNT_FRAME_DONE;
		mtk_dbi_count_write_mask(comp, value,
			DISP_DBI_COUNT_IRQ_MASK, mask, NULL);

		queue_work(comp->mtk_crtc->dbi_data.dbi_event.work_queue, &comp->mtk_crtc->dbi_data.dbi_event.task);
	}

	mtk_drm_top_clk_isr_put(comp);
	return IRQ_HANDLED;
}

static const struct mtk_disp_dbi_count_data mt6993_dbi_count_driver_data = {
	.need_bypass_shadow = true,
	.is_support_stash = true,
	.countr_buffer_size = 300,
	.countw_buffer_size = 300,
	.irq_handler = mtk_dbi_count_irq_handler,
	.stash_lead_time = 20,
	.min_stash_port_bw = 49,
	.use_slot_trigger = true,
};

static const struct of_device_id mtk_disp_dbi_count_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6993-disp-dbi-count",
	  .data = &mt6993_dbi_count_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_dbi_count_driver_dt_match);

struct platform_driver mtk_disp_dbi_count_driver = {
	.probe = mtk_disp_dbi_count_probe,
	.remove = mtk_disp_dbi_count_remove,
	.driver = {
		.name = "mediatek-disp-dbi-count",
		.owner = THIS_MODULE,
		.of_match_table = mtk_disp_dbi_count_driver_dt_match,
	},
};


