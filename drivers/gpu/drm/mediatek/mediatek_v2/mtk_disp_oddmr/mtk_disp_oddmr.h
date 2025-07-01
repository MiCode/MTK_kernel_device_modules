/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_DISP_ODDMR_H__
#define __MTK_DISP_ODDMR_H__
#include <linux/uaccess.h>
#include <uapi/drm/mediatek_drm.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/ratelimit.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include "../mtk_drm_crtc.h"
#include "../mtk_drm_ddp_comp.h"
#include "../mtk_dump.h"
#include "../mtk_drm_mmp.h"
#include "../mtk_drm_gem.h"
#include "../mtk_drm_fb.h"
#include "../mtk_dsi.h"

#define OD_TABLE_MAX 128
#define DMR_TABLE_MAX 2
#define DMR_GAIN_MAX 15
#define OD_GAIN_MAX 15

#define DMR_DBV_TABLE_MAX 4
#define DMR_FPS_TABLE_MAX 4
#define DBI_GET_RAW_TYPE_FRAME_NUM (10)

#define MAX_BIN_NUM 8
#define MAX_BINSET_NUM 32
#define DMR_LINE_BUFFER 19
#define MAX_PID_LENGTH 256
#define MAX_DBV_MODE_NUM 5
#define CUS_MAX_DBV_NUM 50

#define ODDMR_SECTION_WHOLE 0
#define ODDMR_SECTION_END 0xFEFE
//MTK_ODDMR_OD_BASIC_SUB_ID
#define OD_BASIC_WHOLE ODDMR_SECTION_WHOLE
#define OD_BASIC_PARAM 0x0100
#define OD_BASIC_PQ 0x0200
#define OD_BASIC_END ODDMR_SECTION_END

//MTK_ODDMR_OD_TABLE_SUB_ID
#define OD_TABLE_WHOLE ODDMR_SECTION_WHOLE
#define OD_TABLE_BASIC_INFO 0x0100
#define OD_TABLE_GAIN_TABLE 0x0200
#define OD_TABLE_PQ_OD 0x0300
#define OD_TABLE_FPS_GAIN_TABLE 0x0400
#define OD_TABLE_DBV_GAIN_TABLE 0x0500
#define OD_TABLE_DATA 0x0600
#define OD_TABLE_END ODDMR_SECTION_END

enum MTK_ODDMR_PARAM_DATA_TYPE {
	ODDMR_OD_BASIC_INFO = 0x03,
	ODDMR_OD_TABLE = 0x04,
};

enum ODDMR_STATE {
	ODDMR_INVALID = 0,
	ODDMR_LOAD_PARTS,
	ODDMR_RELOAD,
	ODDMR_LOAD_DONE,
	ODDMR_INIT_DONE,
	ODDMR_TABLE_UPDATING,
	ODDMR_MODE_DONE,
};
enum ODDMR_USER_CMD {
	ODDMR_CMD_OD_SET_WEIGHT,
	ODDMR_CMD_OD_ENABLE,
	ODDMR_CMD_DMR_ENABLE,
	ODDMR_CMD_OD_INIT_END,
	ODDMR_CMD_EOF_CHECK_TRIGGER,
	ODDMR_CMD_OD_TUNING_WRITE_SRAM,
	ODDMR_CMD_ODDMR_DDREN_OFF,
	ODDMR_CMD_ODDMR_REMAP_EN,
	ODDMR_CMD_ODDMR_REMAP_OFF,
	ODDMR_CMD_ODDMR_REMAP_CHG,
	ODDMR_CMD_SET_SPR2RGB,
};
enum MTK_ODDMR_MODE_CHANGE_INDEX {
	MODE_ODDMR_NOT_DEFINED = 0,
	MODE_ODDMR_DSI_VFP = BIT(0),
	MODE_ODDMR_DSI_HFP = BIT(1),
	MODE_ODDMR_DSI_CLK = BIT(2),
	MODE_ODDMR_DSI_RES = BIT(3),
	MODE_ODDMR_DDIC = BIT(4),
	MODE_ODDMR_MSYNC20 = BIT(5),
	MODE_ODDMR_DUMMY_PKG = BIT(6),
	MODE_ODDMR_LTPO = BIT(7),
	MODE_ODDMR_MAX,
};

enum MTK_ODDMR_OD_MODE_TYPE {
	OD_MODE_TYPE_RGB444 = 0,
	OD_MODE_TYPE_RGB565 = 1,
	OD_MODE_TYPE_COMPRESS_18 = 2,
	OD_MODE_TYPE_RGB666 = 4,
	OD_MODE_TYPE_COMPRESS_12 = 5,
	OD_MODE_TYPE_RGB555 = 6,
	OD_MODE_TYPE_RGB888 = 7,
};

enum MTK_ODDMR_DMR_MODE_TYPE {
	DMR_MODE_TYPE_RGB8X8L4 = 0,
	DMR_MODE_TYPE_RGB8X8L8 = 1,
	DMR_MODE_TYPE_RGB4X4L4 = 2,
	DMR_MODE_TYPE_RGB4X4L8 = 3,
	DMR_MODE_TYPE_W2X2L4 = 4,
	DMR_MODE_TYPE_W2X2Q = 5,
	DMR_MODE_TYPE_RGB4X4Q = 6,
	DMR_MODE_TYPE_W4X4Q = 7,
	DMR_MODE_TYPE_RGB7X8Q = 8,
};

enum DISP_ODDMR_SLC_IDX {
	DMR_SLC,
	OD_SLC,
	DBI_SLC,
	ODDMR_SLC_NUM,
};

enum MTK_DBV_MODE {
	DBV_PWM_MODE,
	DBV_DC_MODE,
};

enum DMR_TIMING_STATE {
	DBV_MODE_CHG = 0x1,
	FPS_CHG = 0x2,
	DBV_CHG = 0x4,
	BINSET_CHG = 0x8,
};

/***************** file parsing ******************/
struct mtk_oddmr_pq_pair {
	uint32_t addr;
	uint32_t value;
};

struct mtk_oddmr_pq_param {
	uint32_t counts;
	struct mtk_oddmr_pq_pair *param;
};

struct mtk_oddmr_table_raw {
	/* size unit 1 byte */
	uint32_t size;
	uint8_t *value;
};

struct mtk_oddmr_table_gain {
	uint32_t item;
	uint32_t value_r;
	uint32_t value; // used as g from mt6993
	uint32_t value_b;
};

/***************** od param ******************/
/* od alloc table pq gain_table */
/* od table */
struct mtk_oddmr_od_table_basic_info {
	uint32_t width;
	uint32_t height;
	uint32_t fps;
	uint32_t dbv;
	uint32_t min_fps;
	uint32_t max_fps;
	uint32_t min_dbv;
	uint32_t max_dbv;
	uint32_t remap_gian; // added from mt6993
	uint32_t table_offset; // added from mt6993
	uint32_t reserved;
};
struct mtk_oddmr_od_table {
	struct mtk_oddmr_od_table_basic_info table_basic_info;
	uint8_t *gain_table_raw;
	struct mtk_oddmr_pq_param pq_od;
	uint32_t fps_cnt;
	struct mtk_oddmr_table_gain fps_table[OD_GAIN_MAX];
	uint32_t bl_cnt;
	struct mtk_oddmr_table_gain bl_table[OD_GAIN_MAX];
	struct mtk_oddmr_table_raw raw_table;
};
struct mtk_oddmr_od_basic_param {
	uint32_t bin_version; // added from mt6993
	struct mtk_oddmr_panelid panelid;
	/* 0:AP 1:ddic */
	uint32_t resolution_switch_mode;
	uint32_t panel_width;
	uint32_t panel_height;
	uint32_t table_cnt;
	uint32_t od_mode;
	/* 0:no_dither 1:12to11 2:12to10 */
	uint32_t dither_sel;
	uint32_t dither_ctl;
	/* bit(0) hscaling, bit(1) vscaling */
	uint32_t scaling_mode;
	uint32_t nonlinear_node_cnt; // added from mt6993
	uint32_t od_hsk_2;
	uint32_t od_hsk_3;
	uint32_t od_hsk_4;
	uint32_t reserved;
};
/* od basic info */
struct mtk_oddmr_od_basic_info {
	struct mtk_oddmr_od_basic_param basic_param;
	struct mtk_oddmr_pq_param basic_pq;
};
struct mtk_oddmr_od_param {
	struct mtk_oddmr_od_basic_info od_basic_info;
	struct mtk_oddmr_od_table *od_tables[OD_TABLE_MAX];
	bool valid_table[OD_TABLE_MAX];
	int valid_table_cnt;
	int updata_dram_table;
};

struct mtk_drm_dmr_basic_info {
	unsigned int panel_id_len;
	unsigned char panel_id[16];
	unsigned int panel_width;
	unsigned int panel_height;
	unsigned int h_num;
	unsigned int v_num;
	unsigned int catch_bit;
	unsigned int partial_update_scale_factor_h;
	unsigned int partial_update_scale_factor_v;
	unsigned int partial_update_real_frame_width;
	unsigned int partial_update_real_frame_height;
	unsigned int blank_bit;
	unsigned int zero_bit;
//	unsigned int scale_factor_v; //block size
//	unsigned int real_frame_height;
};

struct mtk_drm_oddmr_partial_update_params {
	unsigned int scale_factor_h;
	unsigned int scale_factor_v;
	unsigned int real_frame_width;
	unsigned int real_frame_height;
	unsigned int is_compression_mode;
	unsigned int compression_mode_ln_offset;
	unsigned int slice_num;
	unsigned int *slice_size; //byte base
	unsigned int *slice_height; //pixel base
};

struct mtk_drm_dmr_static_cfg {
	unsigned int reg_num;
	unsigned int *reg_offset;
	unsigned int *reg_mask;
	unsigned int *reg_value;
};

struct mtk_drm_oddmr_reg_tuning {
	unsigned int reg_num;
	unsigned int *reg_addr;
	unsigned int *reg_mask;
	unsigned int *reg_value;
};

struct mtk_drm_dmr_table_index {
	unsigned int DBV_table_num;
	unsigned int FPS_table_num;
	unsigned int table_byte_num;
	unsigned int DC_table_flag;
	unsigned int *DBV_table_idx; // 0, 2048
	unsigned int *FPS_table_idx; // 0, 60

};

struct mtk_drm_dmr_table_content {
	unsigned char *table_single;     // table_single[dbv0][fps0][0~table_bit_num]
	unsigned char *table_single_DC;

	unsigned char *table_L_single;
	unsigned char *table_L_single_DC;
	unsigned char *table_R_single;
	unsigned char *table_R_single_DC;
};

struct mtk_drm_dmr_fps_dbv_node {
	unsigned int DBV_num;
	unsigned int FPS_num;
	unsigned int DC_flag;
	unsigned int remap_gain_address;
	unsigned int remap_gain_mask;
	unsigned int remap_gain_target_code;
	unsigned int remap_reduce_offset_num;
	unsigned int remap_dbv_gain_num;
	unsigned int *DBV_node; // 0, 1024, 2048, 4095
	unsigned int *FPS_node; // 0, 30, 60, 120
	unsigned int *remap_reduce_offset_node;
	unsigned int *remap_reduce_offset_value;
	unsigned int *remap_dbv_gain_node;
	unsigned int *remap_dbv_gain_value;

};

struct mtk_drm_dmr_fps_dbv_change_cfg {
	unsigned int reg_num;
	unsigned int *reg_offset;
	unsigned int *reg_mask;
	unsigned int reg_total_count;
	unsigned int *reg_value; // 3D changed_reg[DBV_ind][FPS_ind],
	unsigned int reg_DC_total_count;
	unsigned int *reg_DC_value; // 3D changed_reg_DC[DBV_ind][FPS_ind],
};

struct mtk_drm_oddmr_dbv_node {
	unsigned int DBV_num;
	unsigned int *DBV_node; // 0, 1024, 2048, 4095
};

struct mtk_drm_oddmr_dbv_chg_cfg {
	unsigned int reg_num;
	unsigned int reg_total_count;
	unsigned int *reg_offset;
	unsigned int *reg_mask;
	unsigned int *reg_value;
};

struct mtk_drm_oddmr_panel_ID {
	uint32_t data_byte_num;
	uint8_t data[MAX_PID_LENGTH];
};

struct mtk_drm_dmr_cfg_info {
	struct mtk_drm_dmr_basic_info basic_info;
	struct mtk_drm_dmr_static_cfg static_cfg;
	struct mtk_drm_dmr_fps_dbv_node fps_dbv_node;
	struct mtk_drm_dmr_fps_dbv_change_cfg fps_dbv_change_cfg;
	struct mtk_drm_dmr_table_index table_index;
	struct mtk_drm_dmr_table_content table_content;
	struct mtk_drm_oddmr_partial_update_params dmr_pu_info;
	struct mtk_drm_oddmr_panel_ID panel_id;
};

struct mtk_drm_oddmr_binset_info {
	unsigned int dbv_interval_num;
	unsigned int *dbv_interval_node; //1024 1520 2048:{0~1024, 1024~1520, 1520~2048, >2048}
	int *dbv_interval_bin_idx; // 0 1 2 -1
};

struct mtk_drm_oddmr_binset_cfg_info {
	unsigned int binfile_num;
	unsigned int binset_num;
	struct mtk_drm_dmr_basic_info basic_info;
	struct mtk_drm_oddmr_binset_info binset_list[MAX_BINSET_NUM];
	struct mtk_drm_oddmr_panel_ID panel_id;
	struct mtk_drm_dmr_fps_dbv_node remap_params;
};

struct mtk_drm_cus_setting_info {
	unsigned int dbv_mode_num;
	unsigned int default_dbv_mode;
	struct mtk_drm_dmr_fps_dbv_node fps_dbv_node[MAX_DBV_MODE_NUM];
	struct mtk_drm_dmr_fps_dbv_change_cfg fps_dbv_change_cfg[MAX_DBV_MODE_NUM];
};

struct cus_own_data {
	unsigned int size;
	void *data;
};

struct mtk_dbi_curve_2d {
	uint32_t num;
	union { int32_t *x; uint32_t *ux; float *fx; };
	union { int32_t *y; uint32_t *uy; float *fy; };
};

struct mtk_dbi_count_hw_param{
	// basic gain
	struct mtk_dbi_curve_2d dbv_gain_curve[3];
	struct mtk_dbi_curve_2d fps_gain_curve[3];
	struct mtk_dbi_curve_2d temp_gain_curve[3];

	// irdrop gain
	uint32_t irdrop_enable;
	uint32_t irdrop_total_weight[3];
	struct mtk_dbi_curve_2d irdrop_total_gain_curve;
	struct mtk_dbi_curve_2d irdrop_ratio_gain_curve[3];
	struct mtk_dbi_curve_2d irdrop_dbv_gain_curve[3];

	// gain norm (time norm)
	uint32_t gain_norm[3];

};


struct mtk_dbi_count_helper {

	// used with struct pc_dbi_mode_reg_list static_cfg
	// change by hw counting / hw sampling
	uint32_t static_hw_counting_mode_index;
	uint32_t static_hw_sampling_mode_index;
	uint32_t static_default_mode_index;

	uint32_t hw_sampling_slice_mode;

	// used with struct pc_dbi_mode_reg_list dfmt_cfg
	// change by spr on/off
	uint32_t dfmt_rgbg_mode_index;
	uint32_t dfmt_bgrg_mode_index;
	uint32_t dfmt_rgb_mode_index;

	// used with struct pc_dbi_hw_count_config hw_count_cfg
	uint32_t hw_count_temp_offset;

	// data shape after downsampling by spr / counting block size
	uint32_t in_height;
	uint32_t in_width;
	uint32_t in_channel;

};

#define DBI_COUNTING_PARAM_MAX_NUM 10

struct mtk_dbi_reg_list {
	uint32_t reg_num;
	uint32_t *addr;
	uint32_t *mask;
	uint32_t *value;
};

struct mtk_dbi_mode_reg_list {
	uint32_t reg_num;
	uint32_t *addr;
	uint32_t *mask;
	uint32_t mode_num;
	uint32_t *mode_value; // shape: [mode_num][reg_num]
};


struct mtk_dbi_hw_count_config {
	uint32_t hw_count_param_num;
	struct mtk_dbi_count_hw_param hw_count_param[DBI_COUNTING_PARAM_MAX_NUM];
	//struct mtk_dbi_reg_list hw_count_reg_list;
	struct mtk_dbi_mode_reg_list code_gain_packed;
};

#define DBI_COMP_CURVE_NUM 3

enum DBIChannel{
	DBI_CH_R = 0,
	DBI_CH_G = 1,
	DBI_CH_B = 2,
	DBI_CHANNEL_NUM = 3
};


struct mtk_drm_dbi_cfg_info {
	struct mtk_drm_dmr_basic_info basic_info;
	struct mtk_drm_dmr_static_cfg static_cfg;
	struct mtk_drm_dmr_fps_dbv_node fps_dbv_node;
	struct mtk_drm_dmr_fps_dbv_change_cfg fps_dbv_change_cfg;
	struct mtk_drm_oddmr_dbv_node dbv_node;
	struct mtk_drm_oddmr_dbv_chg_cfg dbv_change_cfg;

/*dbi count*/
	struct mtk_dbi_count_helper count_helper;
	struct mtk_dbi_hw_count_config count_cfg;
	struct mtk_dbi_mode_reg_list count_static_cfg;
	struct mtk_dbi_mode_reg_list count_dfmt_cfg;
};

struct mtk_dbi_count_buf_cfg{

	uint32_t level;
	uint32_t capacity_ms;      // ms
	uint32_t _hw_threshold; // for debug
	uint32_t sw_timer_ms; // ms
	struct mtk_dbi_reg_list buf_reg_list;

};

struct bitstream_buffer {
	uint8_t *_self;
	uint8_t *_buffer;
	uint32_t used_entry;
	uint32_t used_bit;
	uint32_t size;
	uint32_t read_bit;
	struct mtk_drm_dmr_static_cfg static_cfg;
	struct mtk_drm_dmr_fps_dbv_change_cfg fps_dbv_change_cfg;
};

enum DBICompTableFormat {
	DBI_COMP_TABLE_TRUNC = 0,
	DBI_COMP_TABLE_COMPRESSION = 1
};

struct bitstream_buffer_v2{
	uint8_t *_self;
	uint8_t *_buffer;
	bool _buffer_from_external;
	uint32_t used_entry;
	uint32_t used_bit;
	uint32_t size;
	uint32_t read_bit;
};

struct mtk_dbi_alg_comp_hw_param {

	void *_mem_base_addr; // support share mem
	uint32_t _mem_used_size;
	struct bitstream_buffer_v2 dram_table;
	uint32_t table_format;
	uint32_t slice_height;
	uint32_t slice_num;
	uint32_t *slice_offset;
	uint32_t max_line_size; // byte
	struct mtk_drm_dmr_static_cfg static_cfg;
	struct mtk_drm_dmr_fps_dbv_change_cfg fps_dbv_change_cfg;
};

struct mtk_drm_dbi_rg_backup {
	unsigned int size;
	unsigned int backup_offset_pa;
	unsigned int backup_value_pa;
};
struct mtk_drm_dbi_counting_info {
	unsigned int size;
	unsigned int addr_pa;
};
struct mtk_drm_dbi_share_info {
	unsigned int unused_offset;
	int dbi_init_done;
	int dbi_hw_enable;
	unsigned int panel_width;
	unsigned int panel_height;
	unsigned int curr_fps;
	unsigned int curr_bl;
	unsigned int curr_temp;
	unsigned int spr_format;
	struct mtk_drm_dbi_rg_backup backup;
	struct mtk_drm_dbi_counting_info counting_info;
	unsigned int lifecycle_addr_pa;
	unsigned int lifecycle_addr_va;
	unsigned int pic_addr_pa[2];
	unsigned int pic_addr_va[2];
	unsigned int table_addr_pa;
	unsigned int table_addr_va;
};

struct mtk_drm_dmr_share_info {
	unsigned int backup_reg_size;
	unsigned int backup_reg_pa;
	unsigned int backup_value_pa;
	unsigned int dmr_hw_enable;
	unsigned int panel_width;
	unsigned int panel_height;
};

enum mtk_dbi_version {
	MTK_DBI_V1,
	MTK_DBI_V2,
	MTK_DBI_V3,
};

enum mtk_dmr_version {
	MTK_DMR_V1,
};

enum mtk_od_version {
	MTK_OD_V1,
	MTK_OD_V2,
	MTK_OD_V3,
};

struct mtk_disp_oddmr_data {
	bool need_bypass_shadow;
	/* dujac not support update od table */
	bool is_od_support_table_update;
	bool is_support_rtff;
	bool is_od_support_hw_skip_first_frame;
	bool is_od_need_crop_garbage;
	bool is_od_need_force_clk;
	bool is_od_support_sec;
	bool is_od_merge_lines;
	bool is_od_table_bl_chg;
	int tile_overhead;
	uint32_t dmr_buffer_size;
	uint32_t dbir_buffer_size;
	uint32_t odr_buffer_size;
	uint32_t odw_buffer_size;
	/*p_num: 1tNp, pixel num*/
	uint32_t p_num;
	irqreturn_t (*irq_handler)(int irq, void *dev_id);
	enum mtk_dbi_version dbi_version;
	enum mtk_dmr_version dmr_version;
	enum mtk_od_version od_version;
	bool is_dmr_support_stash;
	unsigned int stash_lead_time;
	bool is_dbi_support_stash;
	bool is_od_support_stash;
	unsigned int min_stash_port_bw;
	int slc_read_alloc;
	int slc_period;
	void (*sodi_config)(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
	bool dbi_compress_support;
};

struct mtk_disp_oddmr_od_data {
	uint32_t ln_offset;
	uint32_t merge_lines;
	int bpp;
	uint32_t base_line_jump;
	int od_set_pu_done;
	int od_sram_read_sel;
	uint32_t spr_rgbg_mode;
	uint32_t od_dram_sel[2];
	int od_sram_table_idx[2];
	/* TODO: sram 0,1 fixed pkg, need support sram1 update */
	/* od_sram_pkgs[a][b]
	 *	a:which table for dram
	 *	b:this table save in which sram
	 */
	struct cmdq_pkt *od_sram_pkgs[OD_TABLE_MAX][2];
	struct mtk_drm_gem_obj *r_channel;
	struct mtk_drm_gem_obj *g_channel;
	struct mtk_drm_gem_obj *b_channel;
	struct mtk_drm_gem_obj *channel;
	unsigned int hrt_idx;
	bool od_sram_check;
	bool od_sram_reading;
	uint8_t *buf_read_sram;
};

struct mtk_disp_oddmr_dbi_data {
	atomic_t cur_dbv_node;
	atomic_t cur_fps_node;
	atomic_t cur_table_idx;
	atomic_t update_table_idx;
	atomic_t update_table_done;
	atomic_t enter_scp;
	struct mtk_drm_gem_obj *dbi_table[2];
	unsigned int dbi_table_block_h[2];
	unsigned int dbi_table_block_v[2];
	unsigned int dbi_table_size[2];
	int table_format[2];
	unsigned int slice_height[2];
	unsigned int slice_num[2];
	unsigned int *slice_offset[2];
	unsigned int used_entry[2];

	int curr_table_format;
	unsigned int curr_slice_height;
	unsigned int curr_slice_num;
	unsigned int *curr_slice_offset;
	unsigned int curr_used_entry;

	unsigned int table_size;
	unsigned int min_block_v;
	unsigned int min_block_h;

	unsigned int cur_max_time;
	atomic_t max_time_set_done;
	atomic_t remap_enable;
	atomic_t remap_gain;
	atomic_t gain_ratio;
	unsigned int scp_param_size;
	unsigned int load_scp_param;
	void *scp_param;
	unsigned int support_scp;
	void *scp_lifecycle;
	unsigned int scp_lifecycle_size;
	void __iomem *spm_base;
};


struct mtk_disp_oddmr_dmr_data {
	atomic_t cur_dbv_node;
	atomic_t cur_fps_node;
	atomic_t cur_dbv_table_idx;
	atomic_t cur_fps_table_idx;
	struct mtk_drm_gem_obj *mura_table[MAX_BIN_NUM][DMR_DBV_TABLE_MAX][DMR_FPS_TABLE_MAX];
	atomic_t remap_enable;
	atomic_t remap_gain;
	atomic_t dmr_bin_num;
	atomic_t cur_binset_idx;
	atomic_t cur_bin_idx;
	atomic_t dmr_timing_state; //bit3:binset_chg; bit2:dbv_chg; bit1:fps_chg; bit0:dbv_mode_chg
	atomic_t cus_binset_state;
	atomic_t cus_setting_state;
	atomic_t cus_own_data_state;
	atomic_t reg_tuning_chg;
	unsigned int max_table_size;
};

struct mtk_disp_oddmr_cfg {
	uint32_t width;
	uint32_t height;
	uint32_t comp_in_width;
	uint32_t comp_overhead;
	uint32_t total_overhead;
};
struct mtk_disp_oddmr_tile_overhead_v {
	unsigned int top_overhead_v;
	unsigned int bot_overhead_v;
	unsigned int comp_overhead_v;
};
struct mtk_disp_oddmr_parital_data_v {
	unsigned int dbi_y_ini;
	unsigned int dbi_udma_y_ini;
	unsigned int y_idx2_ini;
	unsigned int y_remain2_ini;
};

struct work_struct_oddmr_data {
	void *data;
	struct work_struct task;
};

/**
 * struct mtk_disp_oddmr_primary - DISP_oddmr driver structure for dualpipe common data
 */
struct mtk_disp_oddmr_primary {
	bool od_support;
	bool dmr_support;
	bool dbi_support;
	/*
	 * od_weight_trigger is used to trigger od set pq
	 * is used in resume, res switch flow frame 2
	 * frame 1: od on weight = 0 (weight_trigger == 1)
	 * frame 2: od on weight != 0 (weight_trigger == 0)
	 */
	atomic_t od_weight_trigger;
	atomic_t frame_dirty;
	atomic_t sof_irq_available;
	atomic_t sof_irq_for_od_sram;
	/* 2: need oddmr hrt, 1: oddmr hrt done, 0:nothing to do */
	atomic_t dmr_hrt_done;
	atomic_t dbi_hrt_done;
	atomic_t od_hrt_done;
	struct mtk_oddmr_timing current_timing;
	struct mtk_oddmr_timing od_content_timing;
	struct mtk_oddmr_panelid panelid;
	struct task_struct *sof_irq_event_task;
	struct wait_queue_head sof_irq_wq;
	struct wait_queue_head hrt_wq;
	struct wait_queue_head od_sram_wq;
	struct wait_queue_head od_deinit_wq;
	struct wait_queue_head frame_dirty_wq;
	struct wait_queue_head dmr_switch_wq;
	struct mutex clock_lock;
	struct mutex timing_lock;
	struct mutex dbi_data_lock;
	struct mutex dmr_data_lock;
	struct mutex dmr_cus_own_data_lock;
	enum ODDMR_STATE od_state;
	enum ODDMR_STATE dmr_state;
	enum ODDMR_STATE dbi_state;
	struct mtk_oddmr_od_param od_param;
	int od_basic_info_loaded;
	struct mtk_drm_dmr_cfg_info dmr_multi_bin[MAX_BIN_NUM];
	struct mtk_drm_oddmr_binset_cfg_info dmr_binset_cfg_info;
	struct mtk_drm_oddmr_binset_cfg_info dmr_cus_binset_info;
	struct mtk_drm_cus_setting_info dmr_cus_setting_info;
	struct cus_own_data dmr_cus_own_data;
	struct mtk_drm_oddmr_reg_tuning oddmr_reg_tuning_info;
	struct mtk_drm_dbi_cfg_info dbi_cfg_info;
	struct mtk_drm_dbi_cfg_info dbi_cfg_info_tb1;
	struct workqueue_struct *oddmr_wq;
	struct work_struct_oddmr_data update_table_work;
	bool frame_dirty_last;
	/* 0: vrefresh, 1: content fps */
	uint32_t od_fps_mode;
	/* 0: 1000/od_min_fps, >0: od_wait_time (ms) */
	uint32_t od_wait_time;
	uint32_t od_min_fps;
	uint32_t od_max_fps;
	ktime_t sof_time;
	ktime_t sof_time_last;
	atomic_t od_deinit;
	int slc_frame_cnt[ODDMR_SLC_NUM];
	struct drm_mtk_dbi_caps dbi_caps;
	bool dmr_first_en;
};

/**
 * struct mtk_disp_oddmr - DISP_oddmr driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 */
struct mtk_disp_oddmr {
	struct mtk_ddp_comp	 ddp_comp;
	const struct mtk_disp_oddmr_data *data;
	bool is_right_pipe;
	int path_order;
	struct mtk_ddp_comp *companion;
	struct mtk_disp_oddmr_primary *primary_data;
	struct mtk_disp_oddmr_od_data od_data;
	struct mtk_disp_oddmr_dmr_data dmr_data;
	struct mtk_disp_oddmr_dbi_data dbi_data;
	struct mtk_disp_oddmr_cfg cfg;
	atomic_t oddmr_clock_ref;
	int od_enable_req;
	int od_enable;
	int od_enable_last;
	int od_update_sram;
	bool od_force_off;
	bool od_force_off2;
	bool od_force_off_last;
	int dmr_enable_req;
	int dmr_enable;
	atomic_t reg_tuning_en;
	int dbi_enable_req;
	int dbi_enable;
	unsigned int spr_enable;
	unsigned int spr_relay;
	unsigned int spr_format;
	uint32_t qos_srt_dmrr;
	uint32_t last_qos_srt_dmrr;
	uint32_t qos_srt_dbir;
	uint32_t last_qos_srt_dbir;
	uint32_t srt_delay_dbi;
	uint32_t last_cal_srt_dbi;
	uint32_t qos_srt_odr;
	uint32_t last_qos_srt_odr;
	uint32_t qos_srt_odw;
	uint32_t last_qos_srt_odw;
	uint32_t last_hrt_dmrr;
	uint32_t last_hrt_dmrr_stash;
	uint32_t last_hrt_dbir;
	uint32_t last_hrt_odrw;
	uint32_t last_hrt_odrw_stash;
	struct icc_path *qos_req_dmrr;
	struct icc_path *qos_req_dbir;
	struct icc_path *qos_req_odr;
	struct icc_path *qos_req_odw;
	struct icc_path *qos_req_dmrr_hrt;
	struct icc_path *qos_req_dmrr_stash_hrt;
	struct icc_path *qos_req_dbir_hrt;
	struct icc_path *qos_req_dbir_stash_hrt;
	struct icc_path *qos_req_odr_hrt;
	struct icc_path *qos_req_odw_hrt;
	struct icc_path *qos_req_odr_stash_hrt;
	struct icc_path *qos_req_odw_stash_hrt;
	uint32_t irq_status;
	/* larb_cons idx */
	uint32_t larb_dmrr;
	uint32_t larb_dbir;
	uint32_t larb_odr;
	uint32_t larb_odw;
	uint32_t use_slc[ODDMR_SLC_NUM];
	uint32_t od_user_gain;
	/*user pq od bypass lock*/
	uint32_t pq_od_bypass;
	struct mtk_disp_oddmr_tile_overhead_v tile_overhead_v;
	unsigned int set_partial_update;
	unsigned int roi_height;
	unsigned int roi_y;
	unsigned int roi_height_last;
	unsigned int roi_y_last;
	struct mtk_disp_oddmr_parital_data_v dbi_pu_data;
};

bool mtk_drm_dbi_backup(struct drm_crtc *crtc, void *get_phys, void *get_virt,
	void *get_size, unsigned int curr_bl,unsigned int curr_fps, int curr_temp);
int mtk_drm_ioctl_oddmr_load_param(struct drm_device *dev, void *data,
		struct drm_file *file_priv);
int mtk_drm_ioctl_oddmr_ctl(struct drm_device *dev, void *data,
		struct drm_file *file_priv);
void mtk_oddmr_timing_chg(struct mtk_ddp_comp *comp, struct mtk_oddmr_timing *timing, struct cmdq_pkt *handle);
void mtk_oddmr_bl_chg(struct mtk_ddp_comp *comp, uint32_t bl_level, struct cmdq_pkt *handle);
int mtk_oddmr_hrt_cal_notify(struct drm_device *dev, int disp_idx, int *oddmr_hrt);
void mtk_disp_oddmr_debug(struct drm_crtc *crtc, const char *opt);
void mtk_oddmr_ddren(struct cmdq_pkt *cmdq_handle,
	struct drm_crtc *crtc, int en);
unsigned int check_oddmr_err_event(void);
void clear_oddmr_err_event(void);
void mtk_oddmr_scp_status(bool enable);
int mtk_oddmr_load_param(struct mtk_disp_oddmr *priv, struct mtk_drm_oddmr_param *param);

int mtk_oddmr_get_od_enable(struct mtk_ddp_comp *comp);
int mtk_oddmr_get_dmr_enable(struct mtk_ddp_comp *comp);
int mtk_oddmr_get_dbi_enable(struct mtk_ddp_comp *comp);
unsigned int mtk_oddmr_get_dbi_hw_enable(struct mtk_drm_crtc *mtk_crtc);
unsigned int mtk_oddmr_get_dbi_init_done(struct mtk_drm_crtc *mtk_crtc);
bool mtk_drm_dmr_backup(struct drm_crtc *crtc, void *get_phys, void *get_virt,
	unsigned int offset, unsigned int size);
void mtk_oddmr_dbi_trigger_ir_drop(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, uint32_t height);

#endif
