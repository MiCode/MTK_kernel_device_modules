
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#ifndef __MTK_MM_MONITOR_CONTROLLER_H__
#define __MTK_MM_MONITOR_CONTROLLER_H__

#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <soc/mediatek/smi.h>

#define MMMC_POWER_NUM_MAX	36
#define BWR_NUM_DOMAIN_MAX	10
#define ELA_NUM_DOMAIN_MAX	3
#define CTI_NUM_DOMAIN_MAX	3
#define MM_FAKE_ENGINE_MAX	4
#define BWR_NUM_MAX		32
#define ELA_NUM_MAX		7
#define CTI_NUM_MAX		6
#define ELA_HW_ID_INIT		0x63
#define CTI_SETTINGS_MAX	4

#define BUS_WIDTH		16
#define DUMP_ALL_BWR	-1

enum smi_pd_cb_ids {
	DISP_VCORE = 0,
	ISP_VCORE = 1,
	CAM_VCORE = 2,
	CAM_RAWA = 3,
	CAM_RAWB = 4,
	CAM_RAWC = 5,
	VEN_MDP = 6,
};

struct power_domain_cb {
	int (*callback)(struct notifier_block *nb,
		unsigned long is_on, void *v);
};

struct mm_monitor_engine {
	u32 engine;
	struct power_domain_cb *pd_cb;
};

/* MUX ID */
#define MAX_LEVEL 8
enum MUX_ID {
	MMINFRA_MUX_ID = 0,
	DISP_MUX_ID = 1,
	CAM_MAIN_MUX_ID = 2,
	MUX_NUM
};

struct mmmc_mux {
	const uint32_t pa;
	const uint8_t shift;
	const uint8_t bit[MAX_LEVEL];
	const uint16_t freq[MAX_LEVEL];
	void *va;
};

/* AXI MAPPNG */
enum AXI_MON_STATE {
	AXI_MON_OSTDBL = 0,
	AXI_MON_BWL = 1,
};

typedef struct {
	int mux_id;
	int axi_mon_id;
} mux_axi_mon_pair;

typedef struct {
	uint32_t comm_id;
	uint32_t port_id;
} comm_input;

typedef struct {
	uint32_t larb_id;
	mux_axi_mon_pair output;
} larb_axi_mon_mapping;

typedef struct {
	comm_input input;
	mux_axi_mon_pair output;
} comm_axi_mon_mapping;

typedef struct {
	int r_ostdbl;
	int w_ostdbl;
} ostdbl;

struct mtk_mm_axi_mon {
	u32 aximon_comm_map_size;
	u32 aximon_larb_map_size;
	u32 ostdbl_master_r_factor;
	u32 ostdbl_master_w_factor;
	//AXI Monitor OSTD Burst Length Limiter
	u32 ostdbl_w_shift;
	// AXI Monitor BW Limiter
	u32 max_bwl_budget_shf;
	u32 max_bwl_budget;
	u32 max_bwl_up_bnd;
	u32 bwl_budget_shift;
	u32 bwl_up_bnd_shift;
	u32 bwl_budget_size;
	u32 threshold_us;
	u32 ostdbl_bef_smmu_r_factor;
	u32 ostdbl_bef_smmu_w_factor;
	u32 ostdbl_af_smmu_r_factor;
	u32 ostdbl_af_smmu_w_factor;
};

extern larb_axi_mon_mapping *aximon_larb_map;
extern comm_axi_mon_mapping *aximon_comm_map;

enum axi_remap_id {
	AXI_NOT_REMAP = 0,
	AXI_REMAP_DONE = 1,
	AXI_REMAP_NO_FOUND = 2,
	AXI_REMAP_MAX,
};

enum mmmc_state_level {
	MMMC_DISABLE = 0,
	MONITOR_ENABLE = BIT(0),
	CTI_SW_ENABLE = BIT(1),
	DEF_LIMITER_ENABLE = BIT(2),
	FIXED_OSTDBL_ENABLE = BIT(3),
	CAM_ID_FILTER_ENABLE = BIT(4),
};
extern u32 mmmc_state;

enum mm_monitor_engine_id {
	MM_AXI = 0,
	MM_ELA = 1,
	MM_CTI = 2,
	MM_MMINFRA = 3,
	MM_FAKE_ENGINE = 4,
	MM_AXI_LIMITER = 5,
	MM_MONITOR_ENGINE_MAX,
};

struct mtk_bwr {
	u32 hwid;
	u32 base_addr_pa;
	void __iomem *base_addr_va;
	u32 power_domain_id;
	u32 ostdbl_r_nps;
	u32 ostdbl_w_nps;
	struct platform_device *bwr_ela;
	u32 cam_sel_id_0[2];
	u32 cam_sel_id_1[2];
	u32 cam_sel_id_2[2];
	u32 cam_sel_id_3[2];
	bool	disable_limiter;
	int r_ostdbl;
	int w_ostdbl;
};

struct mtk_ela {
	u32 hwid;
	u32 base_addr_pa;
	void __iomem *base_addr_va;
	u32 power_domain_id;
	struct platform_device *ela_cti;
};

struct mtk_cti {
	u32 hwid;
	u32 base_addr_pa;
	void __iomem *base_addr_va;
	u32 power_domain_id;
	u32 cti_in_chnn_stop[CTI_SETTINGS_MAX][2];
	u32 cti_in_chnn_start[CTI_SETTINGS_MAX][2];
	u32 cti_in_settings;
	u32 cti_out_chnn_stop[2];
	u32 cti_out_chnn_start[2];
	struct platform_device *cti_ela;
};

struct mtk_mmmc_power_domain {
	u32 power_domain_id;
	struct mtk_bwr *bwr[BWR_NUM_DOMAIN_MAX];
	u32 bwr_total_cnt;
	struct mtk_ela *ela[ELA_NUM_DOMAIN_MAX];
	u32 ela_total_cnt;
	struct mtk_cti *cti[CTI_NUM_DOMAIN_MAX];
	u32 cti_total_cnt;
	struct notifier_block smi_nb;
	bool kernel_no_ctrl;
};

struct mtk_mminfra2_config {
	u32 base_addr_pa;
	void __iomem *base_addr_va;
	u32 power_domain_id;
	u32 enable_16qos_offset;
	bool enable_16qos;
	bool use_subsys_16qos;
	u32 ultra2qos_disp_offset;
	u32 mminfra_ultra2qos0_val;
	u32 ultra2qos_mdp_offset;
	u32 mminfra_ultra2qos1_val;
	void __iomem	*emi_16qos_monitor_base;
};

struct fake_engine {
	u32 offset;
    u32 fake_engine_id;
	const char *fake_engine_name;
};

struct mtk_mm_fake_engine {
	u32 base_addr_pa;
	void __iomem *base_addr_va;
	struct device *dev;
	struct fake_engine *fake_engines[MM_FAKE_ENGINE_MAX];
	u32 smi_mon_comm0_pa;
	u32 smi_mon_comm1_pa;
	void __iomem *smi_mon_comm0_va;
	void __iomem *smi_mon_comm1_va;
};

#define MM_MONITOR_DBG(fmt, args...) \
	pr_notice("[mm_monitor] %s: "fmt"\n", __func__, ##args)
#define MM_MONITOR_ERR(fmt, args...) \
	pr_notice("[mm_monitor][err] %s: "fmt"\n", __func__, ##args)
static int mmmc_log;
#define MM_MONITOR_INFO(fmt, args...) \
do { \
	if (unlikely(mmmc_log)) \
		pr_info("[mm_monitor][dbg] %s: "fmt"\n", \
		__func__, ##args); \
} while (0)


#if IS_ENABLED(CONFIG_MTK_MM_MONITOR)
s32 is_valid_offset_value(u32 hw, u32 id, u32 offset, u32 value);
void enable_mminfra_funnel(void);
void mminfra_fake_engine_bus_settings(void);
void emi_moniter_settings(void);
u32 get_mmmc_subsys_max(void);
u32 get_mminfra_pd(void);
u32 mmmc_get_state(void);
u16 get_freq_from_mux_id(enum MUX_ID id);
u32 get_power_domains(int index);

u32 mtk_init_monitor(u32 power_domain_id, bool dump_and_force_init);

mux_axi_mon_pair *get_mux_axi_pair_by_larb(uint32_t larb_id);
mux_axi_mon_pair *get_mux_axi_pair_by_comm_port(uint32_t comm_id, uint32_t port_id);
u32 get_min_freq_from_axi_mon(uint32_t mux_id);
void mtk_mmmc_set_ostdbl_by_larb(uint32_t hwid, uint32_t r_bw, uint32_t v2_avg_w_bw,
						uint32_t v2_peak_r_bw, uint32_t v2_peak_w_bw, uint32_t min_freq);
void mtk_mmmc_set_ostdbl(uint32_t hwid, uint32_t min_freq);
void mtk_mmmc_set_bw_limiter(uint32_t hwid, uint32_t r_bw, uint32_t w_bw, uint32_t min_freq);
void mtk_mmmc_enable_axi_limiter(uint32_t hwid, uint32_t axi_mon_state);

u32 get_ostdbl_smmu_factor(void);
u32 get_axi_mon_threshold_us(void);
int mtk_mmmc_smmu_factor_register_notifier(struct notifier_block *nb);
int mtk_mmmc_threshold_us_register_notifier(struct notifier_block *nb);
s32 mtk_dump_bwr(u32 power_domain_id, s32 bwr_hwid);
#else
static inline s32 mtk_dump_bwr(u32 power_domain_id, s32 bwr_hwid)
{
	return 0;
}
static inline s32 is_valid_offset_value(u32 hw, u32 id, u32 offset, u32 value)
{
	return 0;
}
static inline void enable_mminfra_funnel(void)
{
}
static inline void mminfra_fake_engine_bus_settings(void)
{
}
static inline void emi_moniter_settings(void)
{
}
static inline u32 get_mmmc_subsys_max(void)
{
	return 0;
}
static inline u32 get_mminfra_pd(void)
{
	return 0;
}
static inline u16 get_freq_from_mux_id(enum MUX_ID id)
{
	return 0;
}
static inline u32 mtk_init_monitor(u32 power_domain_id, bool dump_and_force_init)
{
	return 0;
}
static inline int mtk_mmmc_reinit_all(const char *val, const struct kernel_param *kp)
{
	return 0;
}
static inline mux_axi_mon_pair *get_mux_axi_pair_by_larb(uint32_t larb_id)
{
	return NULL;
}
static inline mux_axi_mon_pair *get_mux_axi_pair_by_comm_port(uint32_t comm_id, uint32_t port_id)
{
	return NULL;
}
static inline u32 get_min_freq_from_axi_mon(uint32_t mux_id)
{
	return 0;
}
static inline void mtk_mmmc_set_ostdbl_by_larb(uint32_t hwid, uint32_t r_bw, uint32_t v2_avg_w_bw,
	uint32_t v2_peak_r_bw, uint32_t v2_peak_w_bw, uint32_t min_freq)
{
}
static inline void mtk_mmmc_set_ostdbl(uint32_t hwid, uint32_t min_freq)
{
}
static inline void mtk_mmmc_set_bw_limiter(uint32_t hwid, uint32_t r_bw, uint32_t w_bw, uint32_t min_freq)
{
}
static inline void mtk_mmmc_enable_axi_limiter(uint32_t hwid, uint32_t axi_mon_state)
{
}
static inline u32 get_ostdbl_smmu_factor(void)
{
	return 0;
}
static inline u32 get_axi_mon_threshold_us(void)
{
	return 0;
}
static inline int mtk_mmmc_smmu_factor_register_notifier(struct notifier_block *nb)
{
	return 0;
}
static inline int mtk_mmmc_threshold_us_register_notifier(struct notifier_block *nb)
{
	return 0;
}
#endif /* CONFIG_MTK_MM_MONITOR */
#endif /* __MTK_MM_MONITOR_CONTROLLER_H__ */
