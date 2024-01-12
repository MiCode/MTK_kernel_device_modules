/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DSI_H__
#define __MTK_DSI_H__

#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/clk.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_encoder.h>
#include <drm/drm_connector.h>
#include <drm/drm_panel.h>
#include <drm/drm_bridge.h>
#include <video/videomode.h>
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_panel_ext.h"
#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#ifdef CONFIG_MI_DISP
#include "mi_disp/mi_dsi_panel.h"
#include "mi_disp/mi_dsi_panel_count.h"
#endif


enum DSI_N_Version {
	VER_N12 = 0,
	VER_N7,
	VER_N6,
	VER_N5,
	VER_N4,
	VER_N3,
};

struct mtk_dsi_driver_data {
	const u32 reg_cmdq0_ofs;
	const u32 reg_cmdq1_ofs;
	const u32 reg_vm_cmd_con_ofs;
	const u32 reg_vm_cmd_data0_ofs;
	const u32 reg_vm_cmd_data10_ofs;
	const u32 reg_vm_cmd_data20_ofs;
	const u32 reg_vm_cmd_data30_ofs;
	s32 (*poll_for_idle)(struct mtk_dsi *dsi, struct cmdq_pkt *handle);
	irqreturn_t (*irq_handler)(int irq, void *dev_id);
	char *esd_eint_compat;
	bool support_shadow;
	bool need_bypass_shadow;
	bool need_wait_fifo;
	bool new_rst_dsi;
	const u32 buffer_unit;
	const u32 sram_unit;
	const u32 urgent_lo_fifo_us;
	const u32 urgent_hi_fifo_us;
	bool dsi_buffer;
	bool smi_dbg_disable;
	u32 max_vfp;
	void (*mmclk_by_datarate)(struct mtk_dsi *dsi,
		struct mtk_drm_crtc *mtk_crtc, unsigned int en);
	const unsigned int bubble_rate;
	const enum DSI_N_Version n_verion;
};

struct mtk_dsi {
	struct mtk_ddp_comp ddp_comp;
	struct device *dev;
	struct mipi_dsi_host host;
	struct drm_encoder encoder;
	struct drm_connector conn;
	struct drm_panel *panel;
	struct mtk_panel_ext *ext;
	struct cmdq_pkt_buffer cmdq_buf;
	struct drm_bridge *bridge;
	struct phy *phy;
	bool is_slave;
	struct mtk_dsi *slave_dsi;
	struct mtk_dsi *master_dsi;
	struct mtk_drm_connector_caps connector_caps;
	uint32_t connector_caps_blob_id;

	void __iomem *regs;

	struct clk *engine_clk;
	struct clk *digital_clk;
	struct clk *hs_clk;

	u32 data_rate;
	u32 d_rate;
	u32 ulps_wakeup_prd;

	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	unsigned int lanes;
	struct videomode vm;
	int clk_refcnt;
	bool output_en;
	bool doze_enabled;
	u32 irq_data;
	wait_queue_head_t irq_wait_queue;
	struct mtk_dsi_driver_data *driver_data;

	struct t_condition_wq enter_ulps_done;
	struct t_condition_wq exit_ulps_done;
	struct t_condition_wq te_rdy;
	struct t_condition_wq frame_done;
	unsigned int hs_trail;
	unsigned int hs_prpr;
	unsigned int hs_zero;
	unsigned int lpx;
	unsigned int ta_get;
	unsigned int ta_sure;
	unsigned int ta_go;
	unsigned int da_hs_exit;
	unsigned int cont_det;
	unsigned int clk_zero;
	unsigned int clk_hs_prpr;
	unsigned int clk_hs_exit;
	unsigned int clk_hs_post;

	unsigned int vsa;
	unsigned int vbp;
	unsigned int vfp;
	unsigned int hsa_byte;
	unsigned int hbp_byte;
	unsigned int hfp_byte;

	bool mipi_hopping_sta;
	bool panel_osc_hopping_sta;
	unsigned int data_phy_cycle;
	/* for Panel Master dcs read/write */
	struct mipi_dsi_device *dev_for_PM;
	atomic_t ulps_async;
	bool pending_switch;
	struct mtk_drm_esd_ctx *esd_ctx;
	unsigned int cnt;
	unsigned int skip_vblank;
	/* Added by Xiaomi */
#if CONFIG_MI_DISP
	bool fod_backlight_flag;
	bool fod_hbm_flag;
	bool normal_hbm_flag;
	bool dc_flag;
	uint32_t dc_status;
	struct mutex dsi_lock;
	struct mi_dsi_panel_cfg mi_cfg;
	int panel_event;
	struct completion bl_wait_completion;
	struct completion aod_wait_completion;
	struct delayed_work gir_off_delayed_work;
#ifdef CONFIG_MI_DISP_FOD_SYNC
	struct mi_layer_state mi_layer_state;
#endif
	const char * display_type;
	bool need_fod_animal_in_normal;
#endif
};

#if CONFIG_MI_DISP
struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *dvdd_gpio;
	struct gpio_desc *cam_gpio;
	struct gpio_desc *leden_gpio;
	struct gpio_desc *vddio18_gpio;
	struct gpio_desc *vci30_gpio;

	bool prepared;
	bool enabled;
	bool hbm_en;
	bool wqhd_en;
	bool dc_status;
	bool hbm_enabled;
	bool lhbm_en;
	bool doze_suspend;

	int error;
	const char *panel_info;
	int dynamic_fps;
	u32 doze_brightness_state;
	u32 doze_state;

	struct pinctrl *pinctrl_gpios;
	struct pinctrl_state *err_flag_irq;
	struct drm_connector *connector;

	u32 max_brightness_clone;
	u32 factory_max_brightness;
	struct mutex panel_lock;
	int bl_max_level;
	int gir_status;
	int spr_status;
	int crc_level;
	int mode_index;
	unsigned int gate_ic;
	int panel_id;
	int gray_level;

	/* DDIC auto update gamma */
	u32 last_refresh_rate;
	bool need_auto_update_gamma;
	ktime_t last_mode_switch_time;
	int peak_hdr_status;
};
#endif

s32 mtk_dsi_poll_for_idle(struct mtk_dsi *dsi, struct cmdq_pkt *handle);
irqreturn_t mtk_dsi_irq_status(int irq, void *dev_id);
void mtk_dsi_set_mmclk_by_datarate_V1(struct mtk_dsi *dsi,
	struct mtk_drm_crtc *mtk_crtc, unsigned int en);
void mtk_dsi_set_mmclk_by_datarate_V2(struct mtk_dsi *dsi,
	struct mtk_drm_crtc *mtk_crtc, unsigned int en);
void mtk_dsi_get_mmclk_by_datarate_V2(struct mtk_dsi *dsi,
	struct mtk_drm_crtc *mtk_crtc, unsigned int *mmclk);

#endif
