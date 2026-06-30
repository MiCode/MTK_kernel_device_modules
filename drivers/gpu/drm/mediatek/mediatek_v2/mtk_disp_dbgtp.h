/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __MTK_DISP_DBGTP_H__
#define __MTK_DISP_DBGTP_H__


#define FIFO_MON_NUM (4)
#define DISPSYS_NUM (4)
#define OVLSYS_NUM (3)
#define MMLSYS_NUM (3)
#define MAX_SMI_MON_NUM (4)
#define SMI_MON_PORT_NUM (4)

struct dbgtp_dsi {
	bool dsi_mon_en;
	bool dsi_mon_reset_byf;
	unsigned int dsi_mon_sel;
	unsigned int dsi_buf_sel;
	unsigned int dsi_tgt_pix;
};

struct dbgtp_smi {
	bool smi_mon_en;
	bool rst_by_frame;
	unsigned int slice_time;
	unsigned int smi_mon_dump_sel;
	unsigned int smi_mon_addr[SMI_MON_PORT_NUM];
	unsigned int smi_mon_portid[SMI_MON_PORT_NUM];
	bool smi_mon_cg_ctl[SMI_MON_PORT_NUM];
};

struct dbgtp_subsys {
	bool subsys_mon_en;
	bool need_update;
	bool subsys_smi_trig_en;
	bool subsys_dsi_trig_en;
	bool subsys_inlinerotate_info_en;
	bool subsys_crossbar_info_en;
	bool subsys_mon_info_en;
	unsigned int crossbar_mon_cfg0;
	unsigned int crossbar_mon_cfg1;
	unsigned int crossbar_mon_cfg2;
	unsigned int crossbar_mon_cfg3;
	unsigned int crossbar_mon_cfg4;

	struct dbgtp_smi smi_mon[MAX_SMI_MON_NUM];
	struct dbgtp_dsi dsi_mon;
};

struct mtk_dbgtp {
	bool dbgtp_en;
	bool need_update;
	unsigned int dbgtp_switch;
	bool dbgtp_prd_trig_en;
	unsigned int dbgtp_trig_prd;
	bool dbgtp_timeout_en;
	unsigned int dbgtp_timeout_prd;
	bool dsi_lpc_mon_en;
	bool is_validation_mode;

	/* For camera ELA trace be covered issues */
	bool is_cam_hrt_issue;
	/* For debug top trace cover other trace issues */
	bool is_disp_hrt_issue;
	unsigned int hrt_time_count;

	/* debug FIFO mon */
	bool fifo_mon_en[FIFO_MON_NUM];
	unsigned int fifo_mon_trig_thrd[FIFO_MON_NUM];
	bool fifo_mon_sel;
	unsigned int disp_bwr_sel;

	/* dpc */
	unsigned int dbgtp_dpc_mon_cfg;

	/* subsys */
	struct dbgtp_subsys dispsys[DISPSYS_NUM];
	struct dbgtp_subsys ovlsys[OVLSYS_NUM];
	struct dbgtp_subsys mmlsys[MMLSYS_NUM];
};

struct mtk_drm_private;
struct mtk_drm_crtc;

void mtk_dbgtp_config_restore(struct mtk_drm_private *priv);
void mtk_dbgtp_config(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle);
void mtk_dbgtp_all_setting_dump(struct mtk_drm_private *priv);
void mtk_dbgtp_all_regs_dump(struct mtk_drm_private *priv);
void mtk_dbgtp_default_cfg_load(struct mtk_drm_private *priv);
void mtk_dbgtp_load_all_open_setting(struct mtk_drm_private *priv);
void mtk_dbgtp_switch(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle, bool en);
dma_addr_t mtk_get_dbgtp_comp_pa(void);
void mtk_dbgtp_set_mminfra_funnel(bool en);
void mtk_dbgtp_set_trace_top_funnel(bool en);
void mtk_dbgtp_dump_mminfra_funnel(void);
int mtk_dbgtp_dump(void);

/* Just for mt6993*/
void mtk_dbgtp_dsi_gce_event_config(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle);
void mtk_dbgtp_fifo_mon_config(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle);
void mtk_dbgtp_fifo_mon_set_trig_threshold(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle);
void mtk_dbgtp_update(struct mtk_drm_private *priv);

struct dbgtp_funcs {
	/* only for mml debug driver */
	void (*dbgtp_mmlsys_config)(struct cmdq_pkt *cmdq_handle, struct cmdq_base *clt_base, unsigned int mmlsys_id,
		resource_size_t config_regs_pa, void __iomem *config_regs);
	void (*dbgtp_mmlsys_config_dump)(void __iomem *config_regs, unsigned int mmlsys_id);
};
#endif
