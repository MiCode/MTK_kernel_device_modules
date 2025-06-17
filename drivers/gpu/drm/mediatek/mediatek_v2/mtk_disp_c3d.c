// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_disp_c3d.h"

#include "mtk_disp_pq_helper.h"
#include "mtk_debug.h"

#if IS_ENABLED(CONFIG_MTK_MME_SUPPORT)
#include "mmevent_function.h"
#endif

/* field definition */
/* ----------------------------------------------- */
#define C3D_EN                             (0x000)
#define C3D_CFG                            (0x004)
#define C3D_INTEN                          (0x00C)
#define C3D_INTSTA                         (0x010)
#define C3D_SIZE                           (0x024)
#define C3D_SHADOW_CTL                     (0x030)
#define C3D_C1D_000_001                    (0x034)
#define C3D_C1D_002_003                    (0x038)
#define C3D_C1D_004_005                    (0x03C)
#define C3D_C1D_006_007                    (0x040)
#define C3D_C1D_008_009                    (0x044)
#define C3D_C1D_010_011                    (0x048)
#define C3D_C1D_012_013                    (0x04C)
#define C3D_C1D_014_015                    (0x050)
#define C3D_C1D_016_017                    (0x054)
#define C3D_C1D_018_019                    (0x058)
#define C3D_C1D_020_021                    (0x05C)
#define C3D_C1D_022_023                    (0x060)
#define C3D_C1D_024_025                    (0x064)
#define C3D_C1D_026_027                    (0x068)
#define C3D_C1D_028_029                    (0x06C)
#define C3D_C1D_030_031                    (0x070)
#define C3D_SRAM_CFG                       (0x074)
#define C3D_SRAM_STATUS                    (0x078)
#define C3D_SRAM_RW_IF_0                   (0x07C)
#define C3D_SRAM_RW_IF_1                   (0x080)
#define C3D_SRAM_RW_IF_2                   (0x084)
#define C3D_SRAM_RW_IF_3                   (0x088)
#define C3D_SRAM_PINGPONG                  (0x08C)
#define C3D_R2Y_09                         (0x0C0)
#define C3D_Y2R_09                         (0x0E8)
#define C3D_SRAM_RW_IF_4                   (0x15C)
#define C3D_SRAM_RW_IF_5                   (0x160)
#define C3D_SRAM_CFG2                      (0x164)
/* ----------------------------------------------- */

#define C3D_RELAY_MODE BIT(0)
#define C3D_ENGINE_EN BIT(1)

#define C3D_U32_PTR(x) ((unsigned int *)(unsigned long)(x))

#define C3D_REG_3(v0, off0, v1, off1, v2, off2) \
	(((v2) << (off2)) |  ((v1) << (off1)) | ((v0) << (off0)))
#define C3D_REG_2(v0, off0, v1, off1) \
	(((v1) << (off1)) | ((v0) << (off0)))

#define MME_C3D_BUFFER_SIZE (160 * 1024)

static bool debug_flow_log;
static bool debug_api_log;

#if IS_ENABLED(CONFIG_MTK_MME_SUPPORT)
#define C3DFLOW_LOG(fmt, arg...) do { \
	MME_INFO(MME_MODULE_DISP, MME_BUFFER_INDEX_8, fmt, ##arg); \
	if (debug_flow_log) \
		pr_notice("[FLOW]%s:" fmt, __func__, ##arg); \
	} while (0)

#define C3DAPI_LOG(fmt, arg...) do { \
	MME_INFO(MME_MODULE_DISP, MME_BUFFER_INDEX_8, fmt, ##arg); \
	if (debug_api_log) \
		pr_notice("[API]%s:" fmt, __func__, ##arg); \
	} while (0)

#else
#define C3DFLOW_LOG(fmt, arg...) do { \
	if (debug_flow_log) \
		pr_notice("[FLOW]%s:" fmt, __func__, ##arg); \
	} while (0)

#define C3DAPI_LOG(fmt, arg...) do { \
	if (debug_api_log) \
		pr_notice("[API]%s:" fmt, __func__, ##arg); \
	} while (0)
#endif

inline struct mtk_disp_c3d *comp_to_c3d(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_c3d, ddp_comp);
}

static int disp_c3d_create_gce_pkt(struct mtk_ddp_comp *comp, struct cmdq_pkt **pkt)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;

	if (!mtk_crtc) {
		PQ_ERR("%s:%d, invalid crtc\n",
				__func__, __LINE__);
		return -1;
	}

	if (*pkt != NULL)
		return 0;

	if (mtk_crtc->gce_obj.client[CLIENT_PQ])
		*pkt = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_PQ]);
	else
		*pkt = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);

	return 0;
}

static int disp_c3d_acquire_clock(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);

	C3DAPI_LOG("ref: %d+\n", atomic_read(&c3d_data->c3d_clock_ref));

	mutex_lock(&c3d_data->primary_data->clk_lock);
	if (atomic_read(&c3d_data->c3d_clock_ref) == 0) {
		C3DFLOW_LOG("top clock is off\n");
		mutex_unlock(&c3d_data->primary_data->clk_lock);
		return -1;
	}
	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_disp_c3d *c3d1_data = comp_to_c3d(c3d_data->companion);

		if (atomic_read(&c3d1_data->c3d_clock_ref) == 0) {
			C3DFLOW_LOG("top clock is off\n");
			mutex_unlock(&c3d_data->primary_data->clk_lock);
			return -1;
		}
		atomic_inc(&c3d1_data->c3d_clock_ref);
	}
	atomic_inc(&c3d_data->c3d_clock_ref);
	mutex_unlock(&c3d_data->primary_data->clk_lock);
	C3DAPI_LOG("ref: %d-\n", atomic_read(&c3d_data->c3d_clock_ref));
	return 0;
}

static int disp_c3d_release_clock(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);

	C3DAPI_LOG("ref: %d+\n", atomic_read(&c3d_data->c3d_clock_ref));

	mutex_lock(&c3d_data->primary_data->clk_lock);
	if (atomic_read(&c3d_data->c3d_clock_ref) == 0) {
		C3DFLOW_LOG("top clock is off\n");
		mutex_unlock(&c3d_data->primary_data->clk_lock);
		return -1;
	}
	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_disp_c3d *c3d1_data = comp_to_c3d(c3d_data->companion);

		if (atomic_read(&c3d1_data->c3d_clock_ref) == 0) {
			C3DFLOW_LOG("top clock is off\n");
			mutex_unlock(&c3d_data->primary_data->clk_lock);
			return -1;
		}
		atomic_dec(&c3d1_data->c3d_clock_ref);
	}
	atomic_dec(&c3d_data->c3d_clock_ref);
	mutex_unlock(&c3d_data->primary_data->clk_lock);
	C3DAPI_LOG("ref: %d-\n", atomic_read(&c3d_data->c3d_clock_ref));
	return 0;
}

static inline bool disp_c3d_reg_poll(struct mtk_ddp_comp *comp,
	unsigned long addr, unsigned int value, unsigned int mask)
{
	bool return_value = false;
	unsigned int reg_value = 0;
	unsigned int polling_time = 0;

	do {
		reg_value = readl(comp->regs + addr);

		if ((reg_value & mask) == value) {
			return_value = true;
			break;
		}

		udelay(10);
		polling_time += 10;
	} while (polling_time < 1000);

	return return_value;
}

static void disp_c3d_get_property(struct mtk_ddp_comp *comp, struct device_node *node)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	int ret;
	int bin_num = c3d_data->data->def_bin_num;
	int lut_bit = 10;
	unsigned int auto_flip = 0;

	DDPMSG("%s, def_bin_num:%d\n", __func__, bin_num);

	ret = of_property_read_u32(node, "bin-num", &bin_num);
	if (ret)
		PQ_ERR("%s, read dts failed use driver data :%d\n", __func__,  bin_num);

	ret = of_property_read_u32(node, "lut-bit", &lut_bit);
	if (ret)
		PQ_ERR("%s, read dts failed, use default bit: %d\n", __func__, lut_bit);

	if ((bin_num != 17) && (bin_num != 9))
		PQ_ERR("%s, read dts bin_num wrong :%d\n", __func__, bin_num);

	ret = of_property_read_u32(node, "auto-flip", &auto_flip);
	if (ret)
		PQ_ERR("%s: use default auto_flip setting: %d\n", __func__, auto_flip);

	c3d_data->bin_num = bin_num;
	c3d_data->sram_start_addr = c3d_data->data->def_sram_start_addr;
	c3d_data->sram_end_addr = c3d_data->sram_start_addr + (bin_num * bin_num * bin_num - 1) * 4;
	c3d_data->c3dlut_size = bin_num * bin_num * bin_num * 3;
	c3d_data->lut_bit = lut_bit;
	c3d_data->auto_flip = auto_flip;
	DDPMSG("%s(%s), binnum: %d, datasize: %d, lut_bit: %d, auto_flip: %d\n", __func__,
		mtk_dump_comp_str(comp), bin_num, c3d_data->c3dlut_size, c3d_data->lut_bit, c3d_data->auto_flip);
}

static inline bool disp_c3d_write_sram_direct(struct mtk_ddp_comp *comp,
	unsigned int addr, unsigned int value)
{
	bool return_value = false;

	do {
		writel(addr, comp->regs + C3D_SRAM_RW_IF_0);
		writel(value, comp->regs + C3D_SRAM_RW_IF_1);

		return_value = true;
	} while (0);

	return return_value;
}

static inline bool disp_c3d_read_sram_direct(struct mtk_ddp_comp *comp,
	unsigned int addr, unsigned int *value)
{
	bool return_value = false;

	do {
		writel(addr, comp->regs + C3D_SRAM_RW_IF_2);

		if (disp_c3d_reg_poll(comp, C3D_SRAM_STATUS,
				(0x1 << 17), (0x1 << 17)) != true)
			break;

		*value = readl(comp->regs + C3D_SRAM_RW_IF_3);

		return_value = true;
	} while (0);

	return return_value;
}

static bool disp_c3d_check_sram(struct mtk_ddp_comp *comp,
	 bool check_sram)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	unsigned int read_value = 0;
	int sram_apb = 0, sram_int = 0;
	char comp_name[64] = {0};

	mtk_ddp_comp_get_name(comp, comp_name, sizeof(comp_name));
	if (check_sram) {
		read_value = readl(comp->regs + C3D_SRAM_CFG);
		sram_apb = (read_value >> 5) & 0x1;
		sram_int = (read_value >> 6) & 0x1;
		C3DFLOW_LOG("[SRAM] hist_apb(%d) hist_int(%d) 0x%08x in (SOF) comp:%s\n",
			sram_apb, sram_int, read_value, comp_name);
		// after suspend/resume, set FORCE_SRAM_APB = FORCE_SRAM_INT;
		// so need to config C3D_SRAM_CFG on ping-pong mode correctly.
		if (sram_apb == sram_int) {
			mtk_ddp_write_mask_cpu(comp, (sram_int << 6) | (!sram_int << 5) | (1 << 4),
						C3D_SRAM_CFG, 0x7 << 4);
			C3DFLOW_LOG("%s: C3D_SRAM_CFG(0x%08x)\n", __func__, readl(comp->regs + C3D_SRAM_CFG));
		}
		if (sram_int != atomic_read(&c3d_data->c3d_force_sram_apb)) {
			pr_notice("c3d: SRAM config %d != %d config", sram_int,
				atomic_read(&c3d_data->c3d_force_sram_apb));

			if ((comp->mtk_crtc->is_dual_pipe) && (!c3d_data->is_right_pipe)) {
				pr_notice("%s: set update_sram_ignore=true", __func__);
				primary_data->update_sram_ignore = true;

				return false;
			}
		}


		if ((comp->mtk_crtc->is_dual_pipe) && c3d_data->is_right_pipe
				&& primary_data->set_lut_flag && primary_data->update_sram_ignore) {
			primary_data->update_sram_ignore = false;
			primary_data->skip_update_sram = true;

			pr_notice("%s: set update_sram_ignore=false", __func__);

			return false;
		}
	}
	return true;
}

static void disp_c3d_write_3dlut_sram(struct mtk_ddp_comp *comp,
	 enum C3D_CMDQ_TYPE cmd_type )
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	struct cmdq_pkt *handle = NULL;
	unsigned int *cfg, *cfg_r_g, *cfg_b;
	struct cmdq_reuse *reuse;
	unsigned int sram_offset = 0;
	unsigned int write_value = 0, write_value_r_g = 0, write_value_b = 0;

	cfg = primary_data->c3d_sram_cfg;
	cfg_r_g = primary_data->c3d_sram_cfg_r_g;
	cfg_b = primary_data->c3d_sram_cfg_b;

	if ((c3d_data->bin_num != 17) && (c3d_data->bin_num != 9))
		DDPMSG("%s: %d bin Not support!", __func__, c3d_data->bin_num);

	reuse = c3d_data->reuse_c3d[cmd_type];
	handle = primary_data->sram_pkt[cmd_type];

	if (handle == NULL) {
		PQ_ERR("%s: handle is NULL\n", __func__);
		return;
	}

	(handle)->no_pool = true;

	// Write 3D LUT to SRAM
	if (!c3d_data->pkt_reused[cmd_type]) {
		cmdq_pkt_write_value_addr_reuse(handle, comp->regs_pa + C3D_SRAM_RW_IF_0,
			c3d_data->sram_start_addr, ~0, &reuse[0]);
		reuse[0].val = c3d_data->sram_start_addr;
		for (sram_offset = c3d_data->sram_start_addr;
			sram_offset <= c3d_data->sram_end_addr;
				sram_offset += 4) {
			if (c3d_data->lut_bit == 10) {
				write_value = cfg[sram_offset/4];

				// use cmdq reuse to save time
				cmdq_pkt_write_value_addr_reuse(handle, comp->regs_pa + C3D_SRAM_RW_IF_1,
					write_value, ~0, &reuse[sram_offset/4 + 1]);
				//reuse[sram_offset/4 + 1].val = write_value;
			}

			if (c3d_data->lut_bit == 12) {
				write_value_r_g = cfg_r_g[sram_offset/4];
				write_value_b = cfg_b[sram_offset/4];

				// use cmdq reuse to save time
				cmdq_pkt_write_value_addr_reuse(handle, comp->regs_pa + C3D_SRAM_RW_IF_4,
					write_value_b, ~0, &reuse[sram_offset/4*2 + 1]);
				cmdq_pkt_write_value_addr_reuse(handle, comp->regs_pa + C3D_SRAM_RW_IF_1,
					write_value_r_g, ~0, &reuse[sram_offset/4*2 + 2]);
			}
		}
		c3d_data->pkt_reused[cmd_type] = true;
	} else {
		for (sram_offset = c3d_data->sram_start_addr;
			sram_offset <= c3d_data->sram_end_addr;
				sram_offset += 4) {
			if (c3d_data->lut_bit == 10) {
				reuse[sram_offset/4 + 1].val = cfg[sram_offset/4];
				cmdq_pkt_reuse_value(handle, &reuse[sram_offset/4 + 1]);
			}

			if (c3d_data->lut_bit == 12) {
				reuse[sram_offset/4*2 + 1].val = cfg_b[sram_offset/4];
				reuse[sram_offset/4*2 + 2].val = cfg_r_g[sram_offset/4];
				cmdq_pkt_reuse_value(handle, &reuse[sram_offset/4*2 + 1]);
				cmdq_pkt_reuse_value(handle, &reuse[sram_offset/4*2 + 2]);
			}
		}
	}
	if (cmd_type == C3D_USERSPACE)
		c3d_data->c3dlut_updated = true;
	if (cmd_type == C3D_RESUME)
		c3d_data->c3dlut_updated = false;
}

static void disp_c3d_write_3dlut_sram_v2(struct mtk_ddp_comp *comp,
	 enum C3D_CMDQ_TYPE cmd_type)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	struct cmdq_pkt *handle = NULL;
	unsigned int *cfg, *cfg_r_g, *cfg_b;
	struct cmdq_reuse *reuse;
	unsigned int sram_offset = 0;
	unsigned int write_value = 0, write_value_r_g = 0, write_value_b = 0;

	cfg = primary_data->c3d_sram_cfg;
	cfg_r_g = primary_data->c3d_sram_cfg_r_g;
	cfg_b = primary_data->c3d_sram_cfg_b;

	if ((c3d_data->bin_num != 17) && (c3d_data->bin_num != 9))
		DDPMSG("%s: %d bin Not support!", __func__, c3d_data->bin_num);

	reuse = c3d_data->reuse_c3d[cmd_type];
	handle = primary_data->sram_pkt[cmd_type];

	if (handle == NULL) {
		PQ_ERR("%s: handle is NULL\n", __func__);
		return;
	}

	C3DFLOW_LOG("c3dBinNum: %d, cmd_type: %d, reuse: %d, lut_bit: %d\n",
		c3d_data->bin_num, (uint32_t)cmd_type,
		c3d_data->pkt_reused[cmd_type], c3d_data->lut_bit);

	(handle)->no_pool = true;

	// Write 3D LUT to SRAM
	if (!c3d_data->pkt_reused[cmd_type]) {
		if (cmd_type == C3D_USERSPACE)
			mtk_vidle_user_power_keep_by_gce(DISP_VIDLE_USER_PQ_CMDQ, handle, 0);
		cmdq_pkt_write_value_addr_reuse(handle, comp->regs_pa + C3D_SRAM_CFG2,
			0, ~0, &reuse[0]);
		reuse[0].val = 0;
		cmdq_pkt_write_value_addr_reuse(handle, comp->regs_pa + C3D_SRAM_RW_IF_0,
			c3d_data->sram_start_addr, ~0, &reuse[1]);
		reuse[1].val = c3d_data->sram_start_addr;
		for (sram_offset = c3d_data->sram_start_addr;
			sram_offset <= c3d_data->sram_end_addr;
				sram_offset += 4) {
			if (c3d_data->lut_bit == 10) {
				write_value = cfg[sram_offset/4];

				// use cmdq reuse to save time
				cmdq_pkt_write_value_addr_reuse(handle, comp->regs_pa + C3D_SRAM_RW_IF_1,
					write_value, ~0, &reuse[sram_offset/4 + 2]);
			}

			if (c3d_data->lut_bit == 12) {
				write_value_r_g = cfg_r_g[sram_offset/4];
				write_value_b = cfg_b[sram_offset/4];

				// use cmdq reuse to save time
				cmdq_pkt_write_value_addr_reuse(handle, comp->regs_pa + C3D_SRAM_RW_IF_4,
					write_value_b, ~0, &reuse[sram_offset/4*2 + 2]);
				cmdq_pkt_write_value_addr_reuse(handle, comp->regs_pa + C3D_SRAM_RW_IF_1,
					write_value_r_g, ~0, &reuse[sram_offset/4*2 + 3]);
			}
		}

		if (c3d_data->lut_bit == 10) {
			cmdq_pkt_write_value_addr_reuse(handle, comp->regs_pa + C3D_SRAM_CFG2,
				1, ~0, &reuse[sram_offset/4 + 3]);
			reuse[sram_offset/4 + 3].val = 1;
		}
		if (c3d_data->lut_bit == 12) {
			cmdq_pkt_write_value_addr_reuse(handle, comp->regs_pa + C3D_SRAM_CFG2,
				1, ~0, &reuse[sram_offset/4*2 + 4]);
			reuse[sram_offset/4*2 + 4].val = 1;
		}
		c3d_data->pkt_reused[cmd_type] = true;

		if (cmd_type == C3D_USERSPACE)
			mtk_vidle_user_power_release_by_gce(DISP_VIDLE_USER_PQ_CMDQ, handle);
	} else {
		for (sram_offset = c3d_data->sram_start_addr;
			sram_offset <= c3d_data->sram_end_addr;
				sram_offset += 4) {
			if (c3d_data->lut_bit == 10) {
				reuse[sram_offset/4 + 2].val = cfg[sram_offset/4];
				cmdq_pkt_reuse_value(handle, &reuse[sram_offset/4 + 2]);
			}

			if (c3d_data->lut_bit == 12) {
				reuse[sram_offset/4*2 + 2].val = cfg_b[sram_offset/4];
				reuse[sram_offset/4*2 + 3].val = cfg_r_g[sram_offset/4];
				cmdq_pkt_reuse_value(handle, &reuse[sram_offset/4*2 + 2]);
				cmdq_pkt_reuse_value(handle, &reuse[sram_offset/4*2 + 3]);
			}
		}
	}

	if (cmd_type == C3D_USERSPACE)
		c3d_data->c3dlut_updated = true;
	if (cmd_type == C3D_RESUME)
		c3d_data->c3dlut_updated = false;
}

static void disp_c3d_async_flush_done_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;

	if (cb_data != NULL) {
		struct mtk_ddp_comp *comp = cb_data->comp;
		struct cmdq_pkt *cmdq_handle = cb_data->cmdq_handle;
		struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);

		atomic_dec(&c3d_data->c3d_clock_ref);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_disp_c3d *c3d1_data = comp_to_c3d(c3d_data->companion);

			atomic_dec(&c3d1_data->c3d_clock_ref);
		}

		CRTC_MMP_MARK(0, c3d_frame_config,
			atomic_read(&c3d_data->c3d_clock_ref), (unsigned long)cmdq_handle);
		C3DFLOW_LOG("clk_ref: %d\n", atomic_read(&c3d_data->c3d_clock_ref));
	}
}

static bool disp_c3d_flush_3dlut_sram(struct mtk_ddp_comp *comp, enum C3D_CMDQ_TYPE cmd_type)
{
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_disp_c3d *c3d_data = NULL;
	struct mtk_disp_c3d_primary *primary_data = NULL;
	struct cmdq_pkt *cmdq_handle = NULL;
	struct drm_crtc *crtc = NULL;
	struct mtk_cmdq_cb_data *cb_data = NULL;

	C3DFLOW_LOG("(comp: %s, cmd_type: %d)\n", mtk_dump_comp_str(comp), cmd_type);

	mtk_crtc = comp->mtk_crtc;
	if (IS_ERR_OR_NULL(mtk_crtc)) {
		DDPMSG("%s: mtk_crtc is NULL\n", __func__);
		return false;
	}

	crtc = &mtk_crtc->base;
	if (IS_ERR_OR_NULL(crtc)) {
		DDPMSG("%s: crtc is NULL\n", __func__);
		return false;
	}

	c3d_data = comp_to_c3d(comp);
	primary_data = c3d_data->primary_data;
	cmdq_handle = primary_data->sram_pkt[cmd_type];
	cb_data = primary_data->cb_data;

	if (!cmdq_handle) {
		DDPMSG("%s: cmdq handle is null.\n", __func__);
		return false;
	}

	if (!cb_data) {
		DDPMSG("%s: cb_data is null.\n", __func__);
		return false;
	}

	switch (cmd_type) {
	case C3D_USERSPACE:
		if (c3d_data->auto_flip == 1) {
			cmdq_pkt_refinalize(cmdq_handle);
			atomic_inc(&c3d_data->c3d_clock_ref);
			if (comp->mtk_crtc->is_dual_pipe) {
				struct mtk_disp_c3d *c3d1_data = comp_to_c3d(c3d_data->companion);

				atomic_inc(&c3d1_data->c3d_clock_ref);
			}

			if (cmdq_pkt_flush_async(cmdq_handle,
					disp_c3d_async_flush_done_cb, (void *)cb_data) < 0) {
				PQ_ERR("failed to flush %s\n", __func__);
				atomic_dec(&c3d_data->c3d_clock_ref);
				if (comp->mtk_crtc->is_dual_pipe) {
					struct mtk_disp_c3d *c3d1_data = comp_to_c3d(c3d_data->companion);

					atomic_dec(&c3d1_data->c3d_clock_ref);
				}
			} else
				atomic_set(&primary_data->pkt_async_flush, 1);
		} else {
			cmdq_pkt_refinalize(cmdq_handle);
			cmdq_pkt_flush(cmdq_handle);
		}
		CRTC_MMP_MARK(0, c3d_frame_config, comp->id, (unsigned long)cmdq_handle);
		break;

	case C3D_RESUME:
		bool async = mtk_drm_idlemgr_get_async_status(crtc);

		if (async == false)
			mtk_drm_clear_async_cb_list(crtc);

		cmdq_pkt_refinalize(cmdq_handle);
		if (async == false)
			cmdq_pkt_flush(cmdq_handle);
		else {
			int ret = 0;

			ret = mtk_drm_idle_async_flush_cust(crtc, comp->id,
						cmdq_handle, false, NULL);
			if (ret < 0) {
				cmdq_pkt_flush(cmdq_handle);
				DDPMSG("%s, failed of async flush, %d\n", __func__, ret);
			}
		}
		break;
	default:
		break;
	}

	return true;
}

void disp_c3d_flip_3dlut_sram(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	const char *caller)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	u32 sram_apb = 0, sram_int = 0, sram_cfg;
	unsigned int read_value = 0;

	read_value = readl(comp->regs + C3D_SRAM_CFG);
	sram_apb = (read_value >> 5) & 0x1;
	sram_int = (read_value >> 6) & 0x1;

	if (sram_apb == sram_int) {
		pr_notice("%s: sram_apb = sram_int = %d, skip flip!\n", __func__, sram_int);
		return;
	}

	if (atomic_cmpxchg(&c3d_data->c3d_force_sram_apb, 0, 1) == 0) {
		sram_apb = 0;
		sram_int = 1;
	} else if (atomic_cmpxchg(&c3d_data->c3d_force_sram_apb, 1, 0) == 1) {
		sram_apb = 1;
		sram_int = 0;
	} else {
		DDPINFO("[SRAM] Error when get hist_apb in %s", caller);
	}
	sram_cfg = (sram_int << 6) | (sram_apb << 5) | (1 << 4);
	C3DFLOW_LOG("[SRAM] hist_apb(%d) hist_int(%d) 0x%08x in %s",
		sram_apb, sram_int, sram_cfg, caller);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + C3D_SRAM_CFG, sram_cfg, (0x7 << 4));
}

static int disp_c3d_set_3dlut(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_C3D_LUT *c3d_lut)
{
	int i;
	int copysize = 0;
	struct mtk_disp_c3d *c3d = NULL;
	struct mtk_disp_c3d_primary *primary_data = NULL;
	struct mtk_ddp_comp *comp_c3d1 = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	int pm_ret = 0, clock_ret = -1;
	bool ret;

	if (IS_ERR_OR_NULL(comp)) {
		PQ_ERR("%s: comp is NULL\n", __func__);
		return -1;
	}

	c3d = comp_to_c3d(comp);
	primary_data = c3d->primary_data;
	comp_c3d1 = c3d->companion;
	mtk_crtc = comp->mtk_crtc;

	if (IS_ERR_OR_NULL(mtk_crtc)) {
		PQ_ERR("%s: mtk_crtc is NULL\n", __func__);
		return -1;
	}

	if ((c3d->bin_num != 9) && (c3d->bin_num != 17)) {
		PQ_ERR("%s, c3d bin num: %d not support\n", __func__, c3d->bin_num);
		return -1;
	}

	copysize = c3d->c3dlut_size * sizeof(unsigned int);
	C3DFLOW_LOG("c3dBinNum: %d, copysize:%d\n", c3d->bin_num, copysize);

	if (copy_from_user(&primary_data->c3d_reg,
		C3D_U32_PTR(c3d_lut->lut3d), copysize) == 0) {
		mutex_lock(&primary_data->data_lock);
		if (c3d->lut_bit == 10) {
			for (i = 0; i < c3d->c3dlut_size; i += 3) {
				primary_data->c3d_sram_cfg[i / 3] =
					primary_data->c3d_reg.lut3d_reg[i] |
					(primary_data->c3d_reg.lut3d_reg[i+1] << 10) |
					(primary_data->c3d_reg.lut3d_reg[i+2] << 20);
			}
		}

		if (c3d->lut_bit == 12) {
			for (i = 0; i < c3d->c3dlut_size; i += 3) {
				primary_data->c3d_sram_cfg_r_g[i / 3] =
					primary_data->c3d_reg.lut3d_reg[i] |
					(primary_data->c3d_reg.lut3d_reg[i+1] << 16);
				primary_data->c3d_sram_cfg_b[i / 3] =
					primary_data->c3d_reg.lut3d_reg[i+2];
			}
		}
	} else {
		PQ_ERR("%s, c3d copyfrom use fail %d\n", __func__, c3d->bin_num);
		return -1;
	}

	disp_c3d_write_3dlut_sram(comp, C3D_USERSPACE);
	if (mtk_crtc->is_dual_pipe && comp_c3d1)
		disp_c3d_write_3dlut_sram(comp_c3d1, C3D_USERSPACE);

	mutex_unlock(&primary_data->data_lock);

	DDP_MUTEX_LOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, false);
	if (!(mtk_crtc->enabled)) {
		DDPMSG("%s:%d, slepted\n", __func__, __LINE__);
		DDP_MUTEX_UNLOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, false);
		return 0;
	}
	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);
	DDP_MUTEX_UNLOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, false);

	// 2. lock for protect crtc & power
	clock_ret = disp_c3d_acquire_clock(comp);
	if (clock_ret == 0) {
		pm_ret = mtk_vidle_pq_power_get(__func__);
		if (pm_ret) {
			PQ_ERR("%s pq_power_get failed %d, skip\n", __func__, pm_ret);
			disp_c3d_release_clock(comp);
			return -1;
		}

		ret = disp_c3d_check_sram(comp, true);
		if(!ret) {
			if (!pm_ret)
				mtk_vidle_pq_power_put(__func__);
			disp_c3d_release_clock(comp);
			return -1;
		}

		disp_c3d_flush_3dlut_sram(comp, C3D_USERSPACE);
		primary_data->set_lut_flag = false;
		if (primary_data->skip_update_sram) {
			PQ_ERR("%s, skip_update_sram %d return\n", __func__,
					primary_data->skip_update_sram);
			if (!pm_ret)
				mtk_vidle_pq_power_put(__func__);
			disp_c3d_release_clock(comp);
			return -EFAULT;
		}

		disp_c3d_flip_3dlut_sram(comp, handle, __func__);
		if (mtk_crtc->is_dual_pipe && comp_c3d1)
			disp_c3d_flip_3dlut_sram(comp_c3d1, handle, __func__);

		if (!pm_ret)
			mtk_vidle_pq_power_put(__func__);
		disp_c3d_release_clock(comp);
	}

	return 1;
}

static int disp_c3d_set_3dlut_v2(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_C3D_LUT *c3d_lut)
{
	int i;
	int copysize = 0;
	struct mtk_disp_c3d *c3d = NULL;
	struct mtk_disp_c3d_primary *primary_data = NULL;
	struct mtk_ddp_comp *comp_c3d1 = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct cmdq_pkt *pq_handle = NULL;
	int pm_ret = 0, clk_ret = -1;

	if (IS_ERR_OR_NULL(comp)) {
		PQ_ERR("%s: comp is NULL\n", __func__);
		return -1;
	}

	c3d = comp_to_c3d(comp);
	primary_data = c3d->primary_data;
	comp_c3d1 = c3d->companion;
	mtk_crtc = comp->mtk_crtc;

	if (IS_ERR_OR_NULL(mtk_crtc)) {
		PQ_ERR("%s: mtk_crtc is NULL\n", __func__);
		return -1;
	}

	if ((c3d->bin_num != 9) && (c3d->bin_num != 17)) {
		PQ_ERR("%s, c3d bin num: %d not support\n", __func__, c3d->bin_num);
		return -1;
	}

	copysize = c3d->c3dlut_size * sizeof(unsigned int);
	C3DFLOW_LOG("c3dBinNum: %d, copysize:%d, lut_bit: %d\n",
		c3d->bin_num, copysize, c3d->lut_bit);

	CRTC_MMP_EVENT_START(0, c3d_frame_config, 0, 0);
	if (copy_from_user(&primary_data->c3d_reg,
		C3D_U32_PTR(c3d_lut->lut3d), copysize) == 0) {
		mutex_lock(&primary_data->data_lock);
		if (c3d->lut_bit == 10) {
			for (i = 0; i < c3d->c3dlut_size; i += 3) {
				primary_data->c3d_sram_cfg[i / 3] =
					primary_data->c3d_reg.lut3d_reg[i] |
					(primary_data->c3d_reg.lut3d_reg[i+1] << 10) |
					(primary_data->c3d_reg.lut3d_reg[i+2] << 20);
			}
		}

		if (c3d->lut_bit == 12) {
			for (i = 0; i < c3d->c3dlut_size; i += 3) {
				primary_data->c3d_sram_cfg_r_g[i / 3] =
					primary_data->c3d_reg.lut3d_reg[i] |
					(primary_data->c3d_reg.lut3d_reg[i+1] << 16);
				primary_data->c3d_sram_cfg_b[i / 3] =
					primary_data->c3d_reg.lut3d_reg[i+2];
			}
		}
	} else {
		PQ_ERR("%s, c3d copyfrom use fail %d\n", __func__, c3d->bin_num);
		CRTC_MMP_EVENT_END(0, c3d_frame_config, 0xE, 2);
		return -1;
	}

	// wait prev frame config pkt async flush done
	pq_handle = primary_data->sram_pkt[C3D_USERSPACE];
	if ((pq_handle != NULL) && (atomic_read(&primary_data->pkt_async_flush) == 1)) {
		cmdq_pkt_wait_complete(pq_handle);
		atomic_set(&primary_data->pkt_async_flush, 0);
	}

	disp_c3d_write_3dlut_sram_v2(comp, C3D_USERSPACE);
	if (mtk_crtc->is_dual_pipe && comp_c3d1)
		disp_c3d_write_3dlut_sram_v2(comp_c3d1, C3D_USERSPACE);

	mutex_unlock(&primary_data->data_lock);

	DDP_MUTEX_LOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, false);
	if (!(mtk_crtc->enabled)) {
		DDPMSG("%s:%d, slepted\n", __func__, __LINE__);
		DDP_MUTEX_UNLOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, false);
		CRTC_MMP_EVENT_END(0, c3d_frame_config, 0xE, 3);
		return 0;
	}
	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);
	DDP_MUTEX_UNLOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, false);

	// 2. lock for protect crtc & power
	clk_ret = disp_c3d_acquire_clock(comp);
	if (clk_ret == 0) {
		pm_ret = mtk_vidle_pq_power_get(__func__);
		if (pm_ret) {
			PQ_ERR("%s pq_power_get failed %d, skip\n", __func__, pm_ret);
			disp_c3d_release_clock(comp);
			CRTC_MMP_EVENT_END(0, c3d_frame_config, 0xE, 5);
			return -1;
		}

		disp_c3d_flush_3dlut_sram(comp, C3D_USERSPACE);
		if (!pm_ret)
			mtk_vidle_pq_power_put(__func__);
		disp_c3d_release_clock(comp);
	}
	CRTC_MMP_EVENT_END(0, c3d_frame_config, 0, 1);

	return 1;
}

static int disp_c3d_write_1dlut(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle, int lock)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	unsigned int *lut1d;
	int ret = 0;
	char comp_name[64] = {0};

	mtk_ddp_comp_get_name(comp, comp_name, sizeof(comp_name));

	if (lock)
		mutex_lock(&primary_data->data_lock);
	lut1d = &primary_data->c3d_lut1d[0];

	C3DFLOW_LOG("%x, %x, %x, %x, %x", lut1d[0],
			lut1d[2], lut1d[3], lut1d[5], lut1d[6]);
	c3d_data->has_set_1dlut = true;

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_C1D_000_001,
			C3D_REG_2(lut1d[1], 0, lut1d[0], 16), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_C1D_002_003,
			C3D_REG_2(lut1d[3], 0, lut1d[2], 16), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_C1D_004_005,
			C3D_REG_2(lut1d[5], 0, lut1d[4], 16), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_C1D_006_007,
			C3D_REG_2(lut1d[7], 0, lut1d[6], 16), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_C1D_008_009,
			C3D_REG_2(lut1d[9], 0, lut1d[8], 16), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_C1D_010_011,
			C3D_REG_2(lut1d[11], 0, lut1d[10], 16), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_C1D_012_013,
			C3D_REG_2(lut1d[13], 0, lut1d[12], 16), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_C1D_014_015,
			C3D_REG_2(lut1d[15], 0, lut1d[14], 16), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_C1D_016_017,
			C3D_REG_2(lut1d[17], 0, lut1d[16], 16), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_C1D_018_019,
			C3D_REG_2(lut1d[19], 0, lut1d[18], 16), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_C1D_020_021,
			C3D_REG_2(lut1d[21], 0, lut1d[20], 16), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_C1D_022_023,
			C3D_REG_2(lut1d[23], 0, lut1d[22], 16), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_C1D_024_025,
			C3D_REG_2(lut1d[25], 0, lut1d[24], 16), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_C1D_026_027,
			C3D_REG_2(lut1d[27], 0, lut1d[26], 16), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_C1D_028_029,
			C3D_REG_2(lut1d[29], 0, lut1d[28], 16), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_C1D_030_031,
			C3D_REG_2(lut1d[31], 0, lut1d[30], 16), ~0);

	if (lock)
		mutex_unlock(&primary_data->data_lock);

	return ret;
}

static int disp_c3d_set_1dlut(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
		void *data)
{
	int ret;
	struct mtk_disp_c3d *c3d = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d->primary_data;
	struct mtk_ddp_comp *comp_c3d1 = c3d->companion;
	struct DISP_C3D_LUT *c3d_lut = (struct DISP_C3D_LUT *) data;
	unsigned int *c3d_lut1d;

	if (c3d_lut == NULL) {
		ret = -EFAULT;
		PQ_ERR("%s: c3d_lut is NULL\n", __func__);
		return ret;
	}
	c3d_lut1d = (unsigned int *) &(c3d_lut->lut1d[0]);
	C3DFLOW_LOG("%x, %x, %x, %x, %x", c3d_lut1d[0],
			c3d_lut1d[2], c3d_lut1d[3], c3d_lut1d[5], c3d_lut1d[6]);

	mutex_lock(&primary_data->data_lock);
	if (!c3d->has_set_1dlut )
		memcpy(&primary_data->c3d_lut1d[0], c3d_lut1d,
				sizeof(primary_data->c3d_lut1d));

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + C3D_CFG, 0x1 << 1, 0x1 << 1);
	ret = disp_c3d_write_1dlut(comp, handle, 0);
	if (comp->mtk_crtc->is_dual_pipe && comp_c3d1) {
		cmdq_pkt_write(handle, comp_c3d1->cmdq_base,
			comp_c3d1->regs_pa + C3D_CFG, 0x1 << 1, 0x1 << 1);
		ret = disp_c3d_write_1dlut(comp_c3d1, handle, 0);
	}
	mutex_unlock(&primary_data->data_lock);

	return ret;
}

static void disp_c3d_bypass(struct mtk_ddp_comp *comp, int bypass,
	int caller, struct cmdq_pkt *handle)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	struct mtk_ddp_comp *companion = c3d_data->companion;

	DDPINFO("%s: comp: %s, bypass: %d, caller: %d, relay_state: 0x%x\n",
		__func__, mtk_dump_comp_str(comp), bypass, caller, primary_data->relay_state);

	mutex_lock(&primary_data->data_lock);
	if (bypass == 1) {
		if (primary_data->relay_state == 0) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + C3D_CFG, C3D_RELAY_MODE, C3D_RELAY_MODE);
			if (comp->mtk_crtc->is_dual_pipe && companion)
				cmdq_pkt_write(handle, companion->cmdq_base,
					companion->regs_pa + C3D_CFG, C3D_RELAY_MODE, C3D_RELAY_MODE);
		}
		primary_data->relay_state |= (1 << caller);
	} else {
		if (primary_data->relay_state != 0) {
			primary_data->relay_state &= ~(1 << caller);
			if (primary_data->relay_state == 0) {
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + C3D_CFG, 0x0, C3D_RELAY_MODE);
				if (comp->mtk_crtc->is_dual_pipe && companion)
					cmdq_pkt_write(handle, companion->cmdq_base,
						companion->regs_pa + C3D_CFG, 0x0, C3D_RELAY_MODE);
			}
		}
	}
	mutex_unlock(&primary_data->data_lock);
}

static int disp_c3d_user_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	unsigned int cmd, void *data)
{
	C3DFLOW_LOG("cmd: %d\n", cmd);
	switch (cmd) {
	default:
		PQ_ERR("error cmd: %d\n", cmd);
		return -EINVAL;
	}
	return 0;
}

static void disp_c3d_config_overhead(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);

	DDPINFO("line: %d\n", __LINE__);

	if (cfg->tile_overhead.is_support) {
		/*set component overhead*/
		if (!c3d_data->is_right_pipe) {
			c3d_data->tile_overhead.comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.left_overhead +=
				c3d_data->tile_overhead.comp_overhead;
			cfg->tile_overhead.left_in_width +=
				c3d_data->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			c3d_data->tile_overhead.in_width =
				cfg->tile_overhead.left_in_width;
			c3d_data->tile_overhead.overhead =
				cfg->tile_overhead.left_overhead;
		} else if (c3d_data->is_right_pipe) {
			c3d_data->tile_overhead.comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.right_overhead +=
				c3d_data->tile_overhead.comp_overhead;
			cfg->tile_overhead.right_in_width +=
				c3d_data->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			c3d_data->tile_overhead.in_width =
				cfg->tile_overhead.right_in_width;
			c3d_data->tile_overhead.overhead =
				cfg->tile_overhead.right_overhead;
		}
	}

}

static void disp_c3d_config_overhead_v(struct mtk_ddp_comp *comp,
	struct total_tile_overhead_v  *tile_overhead_v)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);

	DDPDBG("line: %d\n", __LINE__);

	/*set component overhead*/
	c3d_data->tile_overhead_v.comp_overhead_v = 0;
	/*add component overhead on total overhead*/
	tile_overhead_v->top_overhead_v +=
		c3d_data->tile_overhead_v.comp_overhead_v;
	tile_overhead_v->bot_overhead_v +=
		c3d_data->tile_overhead_v.comp_overhead_v;
	/*copy from total overhead info*/
	c3d_data->tile_overhead_v.top_overhead_v = tile_overhead_v->top_overhead_v;
	c3d_data->tile_overhead_v.bot_overhead_v = tile_overhead_v->bot_overhead_v;
}

static void disp_c3d_config(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	unsigned int width;
	unsigned int top_overhead_v, bot_overhead_v;
	int sram_int;

	C3DFLOW_LOG("%s(auto-flip: %d, binNum: %d)\n", mtk_dump_comp_str(comp),
		c3d_data->auto_flip, c3d_data->bin_num);

	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support)
		width = c3d_data->tile_overhead.in_width;
	else {
		if (comp->mtk_crtc->is_dual_pipe)
			width = cfg->w / 2;
		else
			width = cfg->w;
	}

	if (c3d_data->set_partial_update != MTK_PARTIAL_UPDATE_SISO)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + C3D_SIZE, (width << 16) | cfg->h, ~0);
	else {
		top_overhead_v = (!comp->mtk_crtc->tile_overhead_v.top_overhead_v)
					? 0 : c3d_data->tile_overhead_v.top_overhead_v;
		bot_overhead_v = (!comp->mtk_crtc->tile_overhead_v.bot_overhead_v)
					? 0 : c3d_data->tile_overhead_v.bot_overhead_v;
		cmdq_pkt_write(handle, comp->cmdq_base,
		   comp->regs_pa + C3D_SIZE,
		   (width << 16) | (c3d_data->roi_height + top_overhead_v + bot_overhead_v), ~0);
	}

	if (c3d_data->bin_num == 9)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + C3D_CFG, 0x0 << 4, (0x1 << 4));

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_R2Y_09, 0, 0x1);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_Y2R_09, 0, 0x1);

	/* Bypass shadow register*/
	if (c3d_data->data->need_bypass_shadow)
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_SHADOW_CTL, 0x1, 0x1);
	else
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_SHADOW_CTL, 0x0, 0x1);

	mutex_lock(&primary_data->data_lock);
	if (atomic_read(&primary_data->c3d_sram_hw_init) == 1) {
		if (c3d_data->auto_flip == 0) {
			sram_int = !!atomic_read(&c3d_data->c3d_force_sram_apb);
			mtk_ddp_write_mask_cpu(comp, (sram_int << 6) | (sram_int << 5) | (1 << 4),
				C3D_SRAM_CFG, 0x7 << 4);
			if (c3d_data->c3dlut_updated)
				disp_c3d_write_3dlut_sram(comp, C3D_RESUME);
		}

		if ((c3d_data->auto_flip == 1) && c3d_data->c3dlut_updated)
			disp_c3d_write_3dlut_sram_v2(comp, C3D_RESUME);

		if (c3d_data->is_right_pipe || !comp->mtk_crtc->is_dual_pipe)
			disp_c3d_flush_3dlut_sram(comp, C3D_RESUME);
		disp_c3d_write_1dlut(comp, handle, 0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + C3D_CFG, 0x2, C3D_ENGINE_EN | C3D_RELAY_MODE);
	}

	if (primary_data->relay_state != 0)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + C3D_CFG, 0x3, C3D_ENGINE_EN | C3D_RELAY_MODE);

	mutex_unlock(&primary_data->data_lock);
}

static void disp_c3d_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	C3DFLOW_LOG("line: %d\n", __LINE__);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_EN, 0x1, ~0);
}

static void disp_c3d_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	C3DFLOW_LOG("line: %d\n", __LINE__);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + C3D_EN, 0x0, ~0);
}

static void disp_c3d_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct cmdq_client *client = NULL;

	mtk_ddp_comp_clk_prepare(comp);
	atomic_set(&c3d_data->c3d_clock_ref, 1);
	if (mtk_crtc->gce_obj.client[CLIENT_PQ]) {
		client = mtk_crtc->gce_obj.client[CLIENT_PQ];
		cmdq_mbox_enable(client->chan);
	}
}

static void disp_c3d_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	struct cmdq_client *client = NULL;
	int retry = 0;

	c3d_data->has_set_1dlut = false;

	C3DFLOW_LOG("comp +: %s\n", mtk_dump_comp_str(comp));

	mutex_lock(&primary_data->clk_lock);
	atomic_dec(&c3d_data->c3d_clock_ref);
	while (atomic_read(&c3d_data->c3d_clock_ref) > 0) {
		if (retry >= 5) {
			PQ_ERR("%s: can't wait clk_ref to 0\n", __func__);
			break;
		}
		DDPMSG("%s: retry: %d\n", __func__, retry);
		mutex_unlock(&primary_data->clk_lock);
		usleep_range(50, 100);
		retry++;
		mutex_lock(&primary_data->clk_lock);
	}

	if (mtk_crtc->gce_obj.client[CLIENT_PQ]) {
		client = mtk_crtc->gce_obj.client[CLIENT_PQ];
		cmdq_mbox_disable(client->chan);
	}
	mtk_ddp_comp_clk_unprepare(comp);
	mutex_unlock(&primary_data->clk_lock);
	C3DFLOW_LOG("comp -: %s\n", mtk_dump_comp_str(comp));
}

static void disp_c3d_init_cmdq_flush_cb_data(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;

	primary_data->cb_data = kmalloc(sizeof(struct mtk_cmdq_cb_data), GFP_KERNEL);
	if (!primary_data->cb_data) {
		PQ_ERR("%s: cb data creation failed\n", __func__);
		return;
	}

	primary_data->cb_data->comp = comp;
	primary_data->cb_data->cmdq_handle = primary_data->sram_pkt[C3D_USERSPACE];
}

static void disp_c3d_init_primary_data(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d *companion_data = comp_to_c3d(c3d_data->companion);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	unsigned int c3d_lut1d_init[DISP_C3D_1DLUT_SIZE] = {
		0, 256, 512, 768, 1024, 1280, 1536, 1792,
		2048, 2304, 2560, 2816, 3072, 3328, 3584, 3840,
		4096, 4608, 5120, 5632, 6144, 6656, 7168, 7680,
		8192, 9216, 10240, 11264, 12288, 13312, 14336, 15360
	};

	if (c3d_data->is_right_pipe) {
		kfree(c3d_data->primary_data);
		c3d_data->primary_data = companion_data->primary_data;
		return;
	}
	memcpy(&primary_data->c3d_lut1d, &c3d_lut1d_init,
			sizeof(c3d_lut1d_init));
	mutex_init(&primary_data->clk_lock);
	mutex_init(&primary_data->data_lock);
	// destroy used pkt and create new one
	disp_c3d_create_gce_pkt(comp, &primary_data->sram_pkt[C3D_USERSPACE]);
	disp_c3d_create_gce_pkt(comp, &primary_data->sram_pkt[C3D_RESUME]);
	atomic_set(&primary_data->c3d_sram_hw_init, 0);
	primary_data->relay_state = 0x1 << PQ_FEATURE_DEFAULT;
	disp_c3d_init_cmdq_flush_cb_data(comp);
	atomic_set(&primary_data->pkt_async_flush, 0);
}

void disp_c3d_first_cfg(struct mtk_ddp_comp *comp,
	       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	int _path_order, ret;
	bool _is_right_pipe;
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;

	disp_c3d_config(comp, cfg, handle);

	ret = mtk_ddp_comp_locate_in_cur_crtc_path(comp->mtk_crtc, comp->id,
					&_is_right_pipe, &_path_order);
	if (!ret && c3d_data->bin_num)
		pq_data->c3d_data_per_crtc.bin_num |= c3d_data->bin_num << (_path_order * 16);
	if (!ret && c3d_data->lut_bit)
		pq_data->c3d_data_per_crtc.lut_bit |= c3d_data->lut_bit << (_path_order * 16);

	DDPMSG("%s, c3d_data_per_crtc: bin_num: %d, lut_bit: %d\n",
		__func__, c3d_data->bin_num, c3d_data->lut_bit);
}

static int disp_c3d_act_get_bin_num(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	int *c3d_bin_num = (int *)data;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;

	*c3d_bin_num = pq_data->c3d_data_per_crtc.bin_num;
	DDPMSG("%s, bin_num_info: def:%d, dts:%d, caps:%d\n", __func__,
		c3d_data->data->def_bin_num, c3d_data->bin_num, pq_data->c3d_data_per_crtc.bin_num);

	return ret;
}

static int disp_c3d_act_get_lut_bit(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	int *c3d_lut_bit = (int *)data;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;

	*c3d_lut_bit = pq_data->c3d_data_per_crtc.lut_bit;
	DDPMSG("%s, accuracy_bit: 0x%x\n", __func__, *c3d_lut_bit);

	return ret;
}

int disp_c3d_cfg_set_lut(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	struct mtk_disp_c3d *c3d = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d->primary_data;
	struct DISP_C3D_LUT *c3d_lut = (struct DISP_C3D_LUT *) data;
	int ret = 0;

	primary_data->skip_update_sram = false;
	disp_pq_set_test_flag(TEST_FLAG_C3D);

	if(c3d->bin_num != c3d_lut->bin_num) {
		DDPINFO("%s: comp binsize %d and data binsize %d not matched\n",
			__func__, c3d->bin_num, c3d_lut->bin_num);
		return 0;
	}

	C3DFLOW_LOG("auto_flip: %d\n", c3d->auto_flip);

	disp_c3d_set_1dlut(comp, handle, data);
	if (c3d->auto_flip == 0)
		ret = disp_c3d_set_3dlut(comp, handle, (struct DISP_C3D_LUT *)data);
	if (c3d->auto_flip == 1)
		ret = disp_c3d_set_3dlut_v2(comp, handle, (struct DISP_C3D_LUT *)data);

	if ((ret > 0) && (atomic_read(&primary_data->c3d_sram_hw_init) == 0)) {
		disp_c3d_bypass(comp, 0, PQ_FEATURE_DEFAULT, handle);
		atomic_set(&primary_data->c3d_sram_hw_init, 1);
	}

	return ret;
}

static int disp_c3d_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
							enum mtk_ddp_io_cmd cmd, void *params)
{
	struct mtk_disp_c3d *data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = data->primary_data;

	switch (cmd) {
	case PQ_FILL_COMP_PIPE_INFO:
	{
		bool *is_right_pipe = &data->is_right_pipe;
		int ret, *path_order = &data->path_order;
		struct mtk_ddp_comp **companion = &data->companion;
		struct mtk_disp_c3d *companion_data;

		if (atomic_read(&comp->mtk_crtc->pq_data->pipe_info_filled) == 1)
			break;
		ret = disp_pq_helper_fill_comp_pipe_info(comp, path_order,
							is_right_pipe, companion);
		if (!ret && comp->mtk_crtc->is_dual_pipe && data->companion) {
			companion_data = comp_to_c3d(data->companion);
			companion_data->path_order = data->path_order;
			companion_data->is_right_pipe = !data->is_right_pipe;
			companion_data->companion = comp;
		}
		disp_c3d_init_primary_data(comp);
		if (comp->mtk_crtc->is_dual_pipe && data->companion)
			disp_c3d_init_primary_data(data->companion);
	}
		break;
	case NOTIFY_CONNECTOR_SWITCH:
	{
		DDPMSG("%s, set sram_hw_init 0\n", __func__);
		atomic_set(&primary_data->c3d_sram_hw_init, 0);
		primary_data->relay_state |= (0x1 << PQ_FEATURE_DEFAULT);
	}
		break;
	case GET_PQ_CAPS:
	{
		struct DISP_PQ_CAPS *pq_caps = (struct DISP_PQ_CAPS *)params;
		struct DISP_PQ_HW_CAPS *comp_caps;

		if (data->bin_num == 9)
			comp_caps = &pq_caps->caps[MTK_DISP_PQ_C3D9];
		else
			comp_caps = &pq_caps->caps[MTK_DISP_PQ_C3D17];
		comp_caps->valid = 1;
		comp_caps->prop1 = data->lut_bit;
	}
		break;
	default:
		break;
	}
	return 0;
}

static int disp_c3d_frame_config(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;
	/* will only call left path */
	switch (cmd) {
	case PQ_C3D_SET_LUT:
		ret = disp_c3d_cfg_set_lut(comp, handle, data, data_size);
		break;
	default:
		break;
	}
	return ret;
}

static int disp_c3d_ioctl_transact(struct mtk_ddp_comp *comp,
		unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;

	switch (cmd) {
	case PQ_C3D_GET_BIN_NUM:
		ret = disp_c3d_act_get_bin_num(comp, data);
		break;
	case PQ_C3D_GET_LUT_BIT:
		ret = disp_c3d_act_get_lut_bit(comp, data);
		break;
	default:
		break;
	}
	return ret;
}

static int disp_c3d_set_partial_update(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle, struct mtk_rect partial_roi, unsigned int enable)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
		unsigned int full_height = mtk_crtc_get_height_by_comp(__func__,
						&comp->mtk_crtc->base, comp, true);
	unsigned int top_overhead_v, bot_overhead_v;

	DDPDBG("%s, %s set partial update, height:%d, enable:%d\n",
			__func__, mtk_dump_comp_str(comp), partial_roi.height, enable);

	c3d_data->set_partial_update = enable;
	c3d_data->roi_height = partial_roi.height;
	top_overhead_v = (!comp->mtk_crtc->tile_overhead_v.top_overhead_v)
				? 0 : c3d_data->tile_overhead_v.top_overhead_v;
	bot_overhead_v = (!comp->mtk_crtc->tile_overhead_v.bot_overhead_v)
				? 0 : c3d_data->tile_overhead_v.bot_overhead_v;

	DDPDBG("%s, %s overhead_v T:%d overhead_v B:%d\n",
			__func__, mtk_dump_comp_str(comp), top_overhead_v, bot_overhead_v);

	if (c3d_data->set_partial_update == MTK_PARTIAL_UPDATE_SISO) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + C3D_SIZE,
				   c3d_data->roi_height + top_overhead_v + bot_overhead_v, 0xffff);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + C3D_SIZE, full_height, 0xffff);
	}

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_disp_c3d_funcs = {
	.config = disp_c3d_config,
	.first_cfg = disp_c3d_first_cfg,
	.start = disp_c3d_start,
	.stop = disp_c3d_stop,
	.bypass = disp_c3d_bypass,
	.user_cmd = disp_c3d_user_cmd,
	.prepare = disp_c3d_prepare,
	.unprepare = disp_c3d_unprepare,
	.config_overhead = disp_c3d_config_overhead,
	.config_overhead_v = disp_c3d_config_overhead_v,
	.io_cmd = disp_c3d_io_cmd,
	.pq_frame_config = disp_c3d_frame_config,
	.pq_ioctl_transact = disp_c3d_ioctl_transact,
	.partial_update = disp_c3d_set_partial_update,
};

static int disp_c3d_bind(struct device *dev, struct device *master,
			       void *data)
{
	struct mtk_disp_c3d *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	pr_notice("%s+\n", __func__);
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}
	pr_notice("%s-\n", __func__);
	return 0;
}

static void disp_c3d_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_c3d *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	pr_notice("%s+\n", __func__);
	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
	pr_notice("%s-\n", __func__);
}

static const struct component_ops mtk_disp_c3d_component_ops = {
	.bind	= disp_c3d_bind,
	.unbind = disp_c3d_unbind,
};

void disp_c3d_dump(struct mtk_ddp_comp *comp)
{
	void __iomem  *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	mtk_cust_dump_reg(baddr, 0x0, 0x4, 0x18, 0x8C);
	mtk_cust_dump_reg(baddr, 0x24, 0x30, 0x34, 0x38);
	mtk_cust_dump_reg(baddr, 0x3C, 0x40, 0x44, 0x48);
	mtk_cust_dump_reg(baddr, 0x4C, 0x50, 0x54, 0x58);
	mtk_cust_dump_reg(baddr, 0x5C, 0x60, 0x64, 0x68);
	mtk_cust_dump_reg(baddr, 0x6C, 0x70, 0x74, 0x78);
	mtk_cust_dump_reg(baddr, 0x7C, 0x80, 0x84, 0x88);
}

void disp_c3d_3dlut_dump(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	struct mtk_disp_c3d_primary *primary_data = c3d_data->primary_data;
	int i;

	DDPMSG(" Dump 3DLUT Register length:%d", c3d_data->c3dlut_size);

	for (i=0; i < c3d_data->c3dlut_size; i++)
		DDPMSG("c3dData[%d] = %d",i, primary_data->c3d_reg.lut3d_reg[i] );
}

void disp_c3d_regdump(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(comp);
	void __iomem  *baddr = comp->regs;
	int k;

	DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp),
			&comp->regs_pa);
	DDPDUMP("== %s RELAY_STATE: 0x%x ==\n", mtk_dump_comp_str(comp),
			c3d_data->primary_data->relay_state);
	DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(comp));
	for (k = 0; k <= 0x94; k += 16) {
		DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
			readl(baddr + k), readl(baddr + k + 0x4),
			readl(baddr + k + 0x8), readl(baddr + k + 0xc));
	}
	DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(comp));
	if (comp->mtk_crtc->is_dual_pipe && c3d_data->companion) {
		baddr = c3d_data->companion->regs;
		DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(c3d_data->companion),
				&c3d_data->companion->regs_pa);
		DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(c3d_data->companion));
		for (k = 0; k <= 0x94; k += 16) {
			DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
				readl(baddr + k), readl(baddr + k + 0x4),
				readl(baddr + k + 0x8), readl(baddr + k + 0xc));
		}
		DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(c3d_data->companion));
	}
}

static int disp_c3d_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_c3d *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret = -1;

	pr_notice("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		goto error_dev_init;

	priv->primary_data = kzalloc(sizeof(*priv->primary_data), GFP_KERNEL);
	if (priv->primary_data == NULL) {
		ret = -ENOMEM;
		PQ_ERR("Failed to alloc primary_data %d\n", ret);
		goto error_dev_init;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_C3D);
	if ((int)comp_id < 0) {
		PQ_ERR("Failed to identify by alias: %d\n", comp_id);
		goto error_primary;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_c3d_funcs);
	if (ret != 0) {
		PQ_ERR("Failed to initialize component: %d\n", ret);
		goto error_primary;
	}

	priv->data = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);
	disp_c3d_get_property(&priv->ddp_comp, dev->of_node);

	ret = component_add(dev, &mtk_disp_c3d_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}
#if IS_ENABLED(CONFIG_MTK_DISP_LOGGER)
#if IS_ENABLED(CONFIG_MTK_MME_SUPPORT)
		MME_REGISTER_BUFFER(MME_MODULE_DISP, "DISP_C3D", MME_BUFFER_INDEX_8, MME_C3D_BUFFER_SIZE);
#endif
#endif
	pr_notice("%s-\n", __func__);

error_primary:
	if (ret < 0)
		kfree(priv->primary_data);
error_dev_init:
	if (ret < 0)
		devm_kfree(dev, priv);

	return ret;
}

static void disp_c3d_remove(struct platform_device *pdev)
{
	struct mtk_disp_c3d *priv = dev_get_drvdata(&pdev->dev);

	pr_notice("%s+\n", __func__);
	component_del(&pdev->dev, &mtk_disp_c3d_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	pr_notice("%s-\n", __func__);
}

static const struct mtk_disp_c3d_data mt6983_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.def_bin_num = 17,
	.def_sram_start_addr = 0,
	.def_sram_end_addr = 19648,
};

static const struct mtk_disp_c3d_data mt6895_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.def_bin_num = 17,
	.def_sram_start_addr = 0,
	.def_sram_end_addr = 19648,
};

static const struct mtk_disp_c3d_data mt6879_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.def_bin_num = 9,
	.def_sram_start_addr = 0,
	.def_sram_end_addr = 2912,
};

static const struct mtk_disp_c3d_data mt6985_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.def_bin_num = 17,
	.def_sram_start_addr = 0,
	.def_sram_end_addr = 19648,
};

static const struct mtk_disp_c3d_data mt6897_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.def_bin_num = 17,
	.def_sram_start_addr = 0,
	.def_sram_end_addr = 19648,
};

static const struct mtk_disp_c3d_data mt6886_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.def_bin_num = 17,
	.def_sram_start_addr = 0,
	.def_sram_end_addr = 19648,
};

static const struct mtk_disp_c3d_data mt6989_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.def_bin_num = 17,
	.def_sram_start_addr = 0,
	.def_sram_end_addr = 19648,
};

static const struct mtk_disp_c3d_data mt6878_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.def_bin_num = 9,
	.def_sram_start_addr = 0,
	.def_sram_end_addr = 2912,
};

static const struct mtk_disp_c3d_data mt6991_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.def_bin_num = 17,
	.def_sram_start_addr = 0,
	.def_sram_end_addr = 19648,
};

static const struct mtk_disp_c3d_data mt6993_c3d_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.def_bin_num = 17,
	.def_sram_start_addr = 0,
	.def_sram_end_addr = 19648,
};

static const struct of_device_id mtk_disp_c3d_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6983-disp-c3d",
	  .data = &mt6983_c3d_driver_data},
	{ .compatible = "mediatek,mt6895-disp-c3d",
	  .data = &mt6895_c3d_driver_data},
	{ .compatible = "mediatek,mt6879-disp-c3d",
	  .data = &mt6879_c3d_driver_data},
	{ .compatible = "mediatek,mt6985-disp-c3d",
	  .data = &mt6985_c3d_driver_data},
	{ .compatible = "mediatek,mt6886-disp-c3d",
	  .data = &mt6886_c3d_driver_data},
	{ .compatible = "mediatek,mt6897-disp-c3d",
	  .data = &mt6897_c3d_driver_data},
	{ .compatible = "mediatek,mt6989-disp-c3d",
	  .data = &mt6989_c3d_driver_data},
	{ .compatible = "mediatek,mt6878-disp-c3d",
	  .data = &mt6878_c3d_driver_data},
	{ .compatible = "mediatek,mt6991-disp-c3d",
	  .data = &mt6991_c3d_driver_data},
	{ .compatible = "mediatek,mt6993-disp-c3d",
	  .data = &mt6993_c3d_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_c3d_driver_dt_match);

struct platform_driver mtk_disp_c3d_driver = {
	.probe = disp_c3d_probe,
	.remove = disp_c3d_remove,
	.driver = {
			.name = "mediatek-disp-c3d",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_c3d_driver_dt_match,
		},
};

void disp_c3d_debug(struct drm_crtc *crtc, const char *opt)
{
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_C3D, 0);
	struct mtk_disp_c3d *c3d_data;
	struct cmdq_reuse *reuse;
	u32 sram_offset;

	pr_notice("[C3D debug]: %s\n", opt);
	if (strncmp(opt, "flow_log:", 9) == 0) {
		debug_flow_log = strncmp(opt + 9, "1", 1) == 0;
		pr_notice("[C3D debug] debug_flow_log = %d\n", debug_flow_log);
	} else if (strncmp(opt, "api_log:", 8) == 0) {
		debug_api_log = strncmp(opt + 8, "1", 1) == 0;
		pr_notice("[C3D debug] debug_api_log = %d\n", debug_api_log);
	} else if (strncmp(opt, "3dlut_dump", 10) == 0) {
		pr_notice("[C3D dump] dump_3dlut_tbl\n");
		disp_c3d_3dlut_dump(comp);
	} else if (strncmp(opt, "debugdump:", 10) == 0) {
		pr_notice("[C3D debug] debug_flow_log = %d\n", debug_flow_log);
		pr_notice("[C3D debug] debug_api_log = %d\n", debug_api_log);
	} else if (strncmp(opt, "dumpsram", 8) == 0) {
		if (!comp) {
			pr_notice("[C3D debug] null pointer!\n");
			return;
		}
		c3d_data = comp_to_c3d(comp);
		reuse = c3d_data->reuse_c3d[C3D_USERSPACE];
		for (sram_offset = c3d_data->sram_start_addr;
			sram_offset + 4 <= c3d_data->sram_end_addr && sram_offset <= 100;
			sram_offset += 4) {
			DDPMSG("[debug] c3d_sram 0x%x: 0x%x, 0x%x, 0x%x, 0x%x\n",
					sram_offset, reuse[sram_offset + 1].val, reuse[sram_offset + 2].val,
					reuse[sram_offset + 3].val, reuse[sram_offset + 4].val);
		}
	}
}

unsigned int disp_c3d_bypass_info(struct mtk_drm_crtc *mtk_crtc, int num)
{
	struct mtk_ddp_comp *comp;
	struct mtk_disp_c3d *c3d_data_0;
	struct mtk_ddp_comp *comp1;
	struct mtk_disp_c3d *c3d_data_1;
	int c3d_bin_num = 0;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_C3D, 0);
	if (!comp) {
		PQ_ERR("%s, comp is null!\n", __func__);
		return 1;
	}
	c3d_data_0 = comp_to_c3d(comp);
	c3d_bin_num = mtk_crtc->pq_data->c3d_data_per_crtc.bin_num;

	if ((c3d_bin_num & 0xFF) == num )
		return c3d_data_0->primary_data->relay_state != 0 ? 1 : 0;
	else if (((c3d_bin_num >> 16) & 0xFF) == num) {
		comp1 = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_C3D, 1);
		if (!comp1) {
			PQ_ERR("%s, comp1 is null!\n", __func__);
			return 1;
		}
		c3d_data_1 = comp_to_c3d(comp1);
		return c3d_data_1->primary_data->relay_state != 0 ? 1 : 0;
	}

	return 1;
}
