
// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include "mtk-mm-monitor-controller.h"
#include <asm/arch_timer.h>
#include <linux/sched/clock.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <soc/mediatek/smi.h>
#include <linux/iommu.h>
#include "mtk-smmu-v3.h"

#define CREATE_TRACE_POINTS
#include "mmmc_events.h"

#define MM_MONITOR_DRIVER_NAME		"mtk-mm-monitor-controller"

/* settings for AXI Monitor */
#define MON_BMAN		0x03C
#define MON_BMAN2		0x044
/* Bandwidth */
#define MON_DBWA		0x000
#define MON_DBWA_2ND		0x004
#define MON_DBWB		0x008
#define MON_DBWB_2ND		0x00C
#define MON_DBWC		0x010
#define MON_DBWC_2ND		0x014
#define MON_DBWD		0x018
#define MON_DBWD_2ND		0x01C
/* Latency */
#define MON_TTYPE_BWRW0		0x050
#define MON_TTYPE0_CONA		0x06C
#define MON_TTYPE1_CONA		0x074
/* Limiter */
#define MON_BWLMTE1		0x0D0
#define MON_BWLMTE1_WA		0x080
#define MON_BWLMTE2		0x0D4
#define MON_BWLMTE2_WA		0x084
#define MON_BWLMTE3		0x0D8
#define MON_BWLMTE3_WA		0x088
/* ID Filter */
#define MON_ID_TMP0		0x0DC
#define MON_ID_MASK0		0x0E0
#define MON_ID_TMP1		0x0E4
#define MON_ID_MASK1		0x0E8
#define MON_ID_TMP2		0x0EC
#define MON_ID_MASK2		0x0F0
#define MON_ID_TMP3		0x0F4
#define MON_ID_MASK3		0x0F8

/* settings for ELA */
#define ELA_SIG_MAX		8
#define CTRL		0x000
#define TIMECTRL		0x004
#define ATBCTRL		0x00C
#define PTACTION		0x010
#define SIGSEL(i)		(0x100 + (i << 8))
#define TRIGCTRL(i)		(0x104 + (i << 8))
#define NEXTSTATE(i)		(0x108 + (i << 8))
#define ACTION(i)		(0x10C + (i << 8))
#define COMPCTRL(i)		(0x118 + (i << 8))
#define ALTCOMPCTRL(i)		(0x11C + (i << 8))
#define COUNTCOMP(i)		(0x120 + (i << 8))
#define TWBSEL(i)		(0x128 + (i << 8))
#define EXTMASK(i)		(0x130 + (i << 8))
#define EXTCOMP(i)		(0x134 + (i << 8))
#define QUALMASK(i)		(0x138 + (i << 8))
#define QUALCOMP(i)		(0x13C + (i << 8))
#define SIGMASK_31_0(i)		(0x140 + (i << 8))
#define SIGMASK_63_32(i)		(0x144 + (i << 8))
#define SIGMASK_95_64(i)		(0x148 + (i << 8))
#define SIGMASK_127_96(i)		(0x14C + (i <<8))

/* settings for CTI/CTM */
#define CTICONTROL		0x000
#define CTIAPPSET		0x014
#define CTIINEN0(cti_chan)		0x020 + (cti_chan * 4)
#define CTIOUTEN0(cti_chan)		0x0A0 + (cti_chan * 4)
#define LAR		0xFB0

/* settings for Fake Engine */
#define FAKE_ENG0_EN		0x0
#define FAKE_ENG0_CON0		0xC
#define FAKE_ENG0_CON1		0x10
#define FAKE_ENG0_CON2		0x14
#define FAKE_ENG0_CON3		0x18
#define FAKE_ENG0_WR_ADDR		0x1C
#define FAKE_ENG0_HASH			0xB8
#define FAKE_ENG0_RD_ADDR		0xBC
#define FAKE_ENG0_SWITCH		0xFFC
#define MMINFRA_START_ADDR(i)	(0x70000000 + (i) * 0x1000000)

/* smi monitor */
#define SMI_MON_ENA		0x1a0
#define SMI_MON_CLR		0x1a4
#define SMI_MON_TYPE	0x1ac
#define SMI_MON_CON		0x1b0
#define SMI_MON_ACT		0x1c0
#define SMI_MON_BYT		0x1d0

#define SMI_MON_BYT_P0RD	0x1d0
#define SMI_MON_BYT_P0WR	0x1c0
#define SMI_MON_BYT_P1RD	0x1c4
#define SMI_MON_BYT_P1WR	0x1cc

#define LINE_MAX_LEN		(800)
#define BUF_ALLOC_SIZE		(PAGE_SIZE)

static struct mtk_mmmc_power_domain *g_mmmc_power_domain[MMMC_POWER_NUM_MAX];
static struct mtk_mminfra2_config *g_mtk_mminfra2_config;
static struct mtk_mm_fake_engine *g_mtk_mm_fake_engine;
static u8 g_mmmc_power_domain_cnt, g_bwr_cnt, g_ela_cnt, g_cti_cnt, g_fake_engines_cnt;
static struct mtk_bwr *g_mtk_bwr[BWR_NUM_MAX];
static struct mtk_ela *g_mtk_ela[ELA_NUM_MAX];
static struct mtk_cti *g_mtk_cti[CTI_NUM_MAX];

static struct task_struct *smi_mon_kthr, *emi_mon_kthr;

/* AXI monitor Limiter */
static uint32_t *urate_freq_table;
static uint32_t urate_lut_size;
static ostdbl *ostdbl_lut;
static uint32_t ostdbl_lut_size;

comm_axi_mon_mapping *aximon_comm_map;
static struct mtk_mm_axi_mon *g_mtk_axi_mon;

module_param(mmmc_log, int, 0644);

int mmmc_smi_mon_interval = 1;
EXPORT_SYMBOL(mmmc_smi_mon_interval);
module_param(mmmc_smi_mon_interval, int, 0644);

int mmmc_smi_mon_dump_interval = 2000;
EXPORT_SYMBOL(mmmc_smi_mon_dump_interval);
module_param(mmmc_smi_mon_dump_interval, int, 0644);

int mmmc_smi_mon_comm0 = 0x11000;
EXPORT_SYMBOL(mmmc_smi_mon_comm0);
module_param(mmmc_smi_mon_comm0, int, 0644);

int mmmc_smi_mon_comm1 = 0x11000;
EXPORT_SYMBOL(mmmc_smi_mon_comm1);
module_param(mmmc_smi_mon_comm1, int, 0644);

int mmmc_fixed_r_ostdbl = 210;
EXPORT_SYMBOL(mmmc_fixed_r_ostdbl);

int mmmc_fixed_w_ostdbl = 120;
EXPORT_SYMBOL(mmmc_fixed_w_ostdbl);

u32 mmmc_state;
static bool hrt_debug_enabled;

int validate_r_ostdbl(const char *val, const struct kernel_param *kp)
{
	int param_val = 0, index, power_domain_id;
	int ret = kstrtoint(val, 0, &param_val);
	u32 bwr_total_cnt;
	struct mtk_mmmc_power_domain *mmmc_power_domain;

	if (ret != 0 || param_val <= 0 || param_val > 0xFFF) {
		MM_MONITOR_ERR("Invalid value for ostdbl: %d\n", param_val);
		return -EINVAL;
	}
	MM_MONITOR_DBG("Change %s from %d to %d", kp->name, *(int *)kp->arg, param_val);
	for (index = 0; index < get_mmmc_subsys_max(); index++) {
		int i;

		if (!g_mmmc_power_domain[index])
			continue;
		power_domain_id = g_mmmc_power_domain[index]->power_domain_id;
		if (g_mmmc_power_domain[index]->kernel_no_ctrl) {
			MM_MONITOR_DBG("power_domain_id:%d kernel_no_control:%d",
				power_domain_id, g_mmmc_power_domain[index]->kernel_no_ctrl);
				continue;
		}
		mmmc_power_domain = g_mmmc_power_domain[index];
		bwr_total_cnt = mmmc_power_domain->bwr_total_cnt;
		for (i = 0; i < bwr_total_cnt; i++) {
			struct mtk_bwr *bwr = mmmc_power_domain->bwr[i];

			if (!bwr)
				continue;
			bwr->r_ostdbl = param_val;
		}
	}

	return param_set_int(val, kp);
}

const struct kernel_param_ops ostdbl_r_ops = {
	.set = validate_r_ostdbl,
	.get = param_get_int,
};

int validate_w_ostdbl(const char *val, const struct kernel_param *kp)
{
	int param_val = 0, index, power_domain_id;
	int ret = kstrtoint(val, 0, &param_val);
	u32 bwr_total_cnt;
	struct mtk_mmmc_power_domain *mmmc_power_domain;

	if (ret != 0 || param_val <= 0 || param_val > 0xFFF) {
		MM_MONITOR_ERR("Invalid value for ostdbl: %d\n", param_val);
		return -EINVAL;
	}
	MM_MONITOR_DBG("Change %s from %d to %d", kp->name, *(int *)kp->arg, param_val);
	for (index = 0; index < get_mmmc_subsys_max(); index++) {
		int i;

		if (!g_mmmc_power_domain[index])
			continue;
		power_domain_id = g_mmmc_power_domain[index]->power_domain_id;
		if (g_mmmc_power_domain[index]->kernel_no_ctrl) {
			MM_MONITOR_DBG("power_domain_id:%d kernel_no_control:%d",
				power_domain_id, g_mmmc_power_domain[index]->kernel_no_ctrl);
				continue;
		}
		mmmc_power_domain = g_mmmc_power_domain[index];
		bwr_total_cnt = mmmc_power_domain->bwr_total_cnt;
		for (i = 0; i < bwr_total_cnt; i++) {
			struct mtk_bwr *bwr = mmmc_power_domain->bwr[i];

			if (!bwr)
				continue;
			bwr->w_ostdbl = param_val;
		}
	}

	return param_set_int(val, kp);
}

const struct kernel_param_ops ostdbl_w_ops = {
	.set = validate_w_ostdbl,
	.get = param_get_int,
};

module_param_cb(mmmc_fixed_r_ostdbl, &ostdbl_r_ops, &mmmc_fixed_r_ostdbl, 0644);
module_param_cb(mmmc_fixed_w_ostdbl, &ostdbl_w_ops, &mmmc_fixed_w_ostdbl, 0644);

int mtk_mmmc_set_rw_ostdbl(const char *val, const struct kernel_param *kp)
{
	u32 result, subsys_id, r_ostdbl, w_ostdbl;
	int i;
	struct mtk_mmmc_power_domain *mmmc_power_domain;
	u32 bwr_total_cnt;

	result = sscanf(val, "%d %d %d", &subsys_id, &r_ostdbl, &w_ostdbl);
	if (result != 3 || subsys_id >= get_mmmc_subsys_max()) {
		MM_MONITOR_ERR("subsys_id:%d set ostdbl RD%d WR%d fail result:%d",
			subsys_id, r_ostdbl, w_ostdbl, result);

		return result;
	}

	if (!hrt_debug_enabled) {
		MM_MONITOR_ERR("restrict api in user load, hrt_debug_enabled:%d", hrt_debug_enabled);
		return 0;
	}

	mmmc_power_domain = g_mmmc_power_domain[subsys_id];
	if (!mmmc_power_domain) {
		MM_MONITOR_ERR("power_domain:%d empty data", subsys_id);
		return -EINVAL;
	}
	bwr_total_cnt = mmmc_power_domain->bwr_total_cnt;
	for (i = 0; i < bwr_total_cnt; i++) {
		struct mtk_bwr *bwr = mmmc_power_domain->bwr[i];

		if (!bwr)
			continue;
		MM_MONITOR_DBG("Change bwr %d from RD%d to RD%d, WR%d to WR%d",
			bwr->hwid, bwr->r_ostdbl, r_ostdbl, bwr->w_ostdbl, w_ostdbl);
		bwr->r_ostdbl = r_ostdbl;
		bwr->w_ostdbl = w_ostdbl;
	}

	return 0;
}
static const struct kernel_param_ops mmmc_set_ostbl_ops = {
	.set = mtk_mmmc_set_rw_ostdbl,
};
module_param_cb(mtk_mmmc_set_rw_ostdbl, &mmmc_set_ostbl_ops, NULL, 0644);
MODULE_PARM_DESC(mtk_mmmc_set_rw_ostdbl, "set ostdbl by power domain");

int mtk_mmmc_set_ostdbl_en(const char *val, const struct kernel_param *kp)
{
	u32 result, subsys_id, ostdbl_enable;
	int i;
	struct mtk_mmmc_power_domain *mmmc_power_domain;
	u32 bwr_total_cnt;

	result = sscanf(val, "%d %d", &subsys_id, &ostdbl_enable);
	if (result != 2 || subsys_id >= get_mmmc_subsys_max()) {
		MM_MONITOR_ERR("subsys_id:%d enable:%d fail result:%d", subsys_id, ostdbl_enable, result);

		return result;
	}

	if (!hrt_debug_enabled) {
		MM_MONITOR_ERR("restrict api in user load, hrt_debug_enabled:%d", hrt_debug_enabled);
		return 0;
	}

	mmmc_power_domain = g_mmmc_power_domain[subsys_id];
	if (!mmmc_power_domain) {
		MM_MONITOR_ERR("power_domain:%d empty data", subsys_id);
		return -EINVAL;
	}
	bwr_total_cnt = mmmc_power_domain->bwr_total_cnt;
	for (i = 0; i < bwr_total_cnt; i++) {
		struct mtk_bwr *bwr = mmmc_power_domain->bwr[i];

		if (!bwr)
			continue;
		MM_MONITOR_DBG("Change bwr %d enable from %d to %d",
			bwr->hwid, bwr->disable_limiter, bwr->disable_limiter^ostdbl_enable);
		bwr->disable_limiter ^= ostdbl_enable;
	}

	return 0;
}
static const struct kernel_param_ops mmmc_set_ostbl_en_ops = {
	.set = mtk_mmmc_set_ostdbl_en,
};
module_param_cb(mtk_mmmc_set_ostdbl_en, &mmmc_set_ostbl_en_ops, NULL, 0644);
MODULE_PARM_DESC(mtk_mmmc_set_ostdbl_en, "set ostdbl by power domain");

u32 mmmc_get_state(void)
{
	return mmmc_state;
}
EXPORT_SYMBOL(mmmc_get_state);

static int mmmc_set_state(const char *val, const struct kernel_param *kp)
{
	u32 state = 0;
	int ret;

	ret = kstrtou32(val, 0, &state);
	if (ret) {
		MM_MONITOR_ERR("failed:%d state:%#x", ret, state);
		return ret;
	}

	MM_MONITOR_DBG("sync mmmc_state: %d -> %d", mmmc_state, state);
	mmmc_state = state;

	return ret;
}

static const struct kernel_param_ops mmmc_state_ops = {
	.set = mmmc_set_state,
	.get = param_get_uint,
};
module_param_cb(mmmc_state, &mmmc_state_ops, &mmmc_state, 0644);
MODULE_PARM_DESC(mmmc_state, "mmmc_state");

static u64 sched_clock_value;

static int mmmc_get_timer(char *val, const struct kernel_param *kp)
{
	u64 systimer_cnt;

	systimer_cnt = arch_timer_read_counter();
	sched_clock_value = sched_clock();

	MM_MONITOR_DBG("systimer:%llu ns, ktime: %llu ns", systimer_cnt, sched_clock_value);

	return 0;
}

static const struct kernel_param_ops get_timer_ops = {
	.get = mmmc_get_timer,
};
module_param_cb(get_timer, &get_timer_ops, &sched_clock_value, 0444);
MODULE_PARM_DESC(get_timer, "get_timer");

mux_axi_mon_pair *get_mux_axi_pair_by_comm_port(uint32_t comm_id, uint32_t port_id)
{
	int i;

	if (!g_mtk_axi_mon) {
		MM_MONITOR_ERR("g_mtk_axi_mon is null");
		return NULL;
	}
	if (!aximon_comm_map) {
		MM_MONITOR_ERR("aximon_comm_map is null");
		return NULL;
	}
	MM_MONITOR_INFO("comm_id:%d, port_id:%d", comm_id, port_id);
	for (i = 0; i < g_mtk_axi_mon->aximon_comm_map_size; i++) {
		if (aximon_comm_map[i].input.comm_id == comm_id &&
			aximon_comm_map[i].input.port_id == port_id)
			return &aximon_comm_map[i].output;
	}

	return NULL;
}
EXPORT_SYMBOL(get_mux_axi_pair_by_comm_port);

larb_axi_mon_mapping *aximon_larb_map;

mux_axi_mon_pair *get_mux_axi_pair_by_larb(uint32_t larb_id)
{
	int i;

	if (!g_mtk_axi_mon) {
		MM_MONITOR_ERR("g_mtk_axi_mon is null");
		return NULL;
	}
	if (!aximon_larb_map) {
		MM_MONITOR_ERR("aximon_larb_map is null");
		return NULL;
	}
	MM_MONITOR_INFO("larb:%d", larb_id);
	for (i = 0; i < g_mtk_axi_mon->aximon_larb_map_size; i++) {
		if (aximon_larb_map[i].larb_id == larb_id)
			return &aximon_larb_map[i].output;
	}

	return NULL;
}
EXPORT_SYMBOL(get_mux_axi_pair_by_larb);

u32 get_min_freq_from_axi_mon(uint32_t mux_id)
{
	u32 mminfra_freq = 0, local_bus_freq = 0, min_freq = 0;

	mminfra_freq = get_freq_from_mux_id(MMINFRA_MUX_ID);
	if (MMINFRA_MUX_ID != mux_id) {
		local_bus_freq = get_freq_from_mux_id(mux_id);
		if (local_bus_freq == 0)
			min_freq = mminfra_freq;
		else if (local_bus_freq <= mminfra_freq)
			min_freq = local_bus_freq;
		else
			min_freq = mminfra_freq;
	} else {
		min_freq = mminfra_freq;
	}
	MM_MONITOR_INFO("mminfra_freq:%d, local_bus_freq:%d, min_freq:%d",
		mminfra_freq, local_bus_freq, min_freq);
	if (min_freq == 0)
		MM_MONITOR_ERR("get min_freq=0 from axi_mon failed!");

	return min_freq;
}
EXPORT_SYMBOL(get_min_freq_from_axi_mon);

static RAW_NOTIFIER_HEAD(mtk_mmmc_smmu_factor_notifier_list);
int mtk_mmmc_smmu_factor_register_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&mtk_mmmc_smmu_factor_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(mtk_mmmc_smmu_factor_register_notifier);

u32 get_ostdbl_smmu_factor(void)
{
	if (!g_mtk_axi_mon) {
		MM_MONITOR_ERR("g_mtk_axi_mon is NULL");
		return 0;
	}
	return (g_mtk_axi_mon->ostdbl_bef_smmu_r_factor & 0xff) << 24 |
		(g_mtk_axi_mon->ostdbl_bef_smmu_w_factor & 0xff) << 16 |
		(g_mtk_axi_mon->ostdbl_af_smmu_r_factor & 0xff) << 8 |
		(g_mtk_axi_mon->ostdbl_af_smmu_w_factor & 0xff);
}
EXPORT_SYMBOL(get_ostdbl_smmu_factor);

static int mtk_mmmc_set_smmu_factor(const char *val, const struct kernel_param *kp)
{
	u32 bef_r, bef_w, af_r, af_w;
	int ret;

	ret = sscanf(val, "%u %u %u %u", &bef_r, &bef_w, &af_r, &af_w);
	if (ret != 4) {
		MM_MONITOR_ERR("fail ret=%d", ret);
		return ret;
	}

	MM_MONITOR_INFO("update bef_r=%u bef_w=%u af_r=%u af_w=%u", bef_r, bef_w, af_r, af_w);
	g_mtk_axi_mon->ostdbl_bef_smmu_r_factor = bef_r;
	g_mtk_axi_mon->ostdbl_bef_smmu_w_factor = bef_w;
	g_mtk_axi_mon->ostdbl_af_smmu_r_factor = af_r;
	g_mtk_axi_mon->ostdbl_af_smmu_w_factor = af_w;

	raw_notifier_call_chain(&mtk_mmmc_smmu_factor_notifier_list, 0, NULL);
	return 0;
}
static const struct kernel_param_ops mmmc_smmu_factor_ops = {
	.set = mtk_mmmc_set_smmu_factor,
};
module_param_cb(mtk_mmmc_smmu_factor, &mmmc_smmu_factor_ops, NULL, 0644);
MODULE_PARM_DESC(mtk_mmmc_smmu_factor, "set smmu factor");


static RAW_NOTIFIER_HEAD(mtk_mmmc_threshold_us_notifier_list);
int mtk_mmmc_threshold_us_register_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&mtk_mmmc_threshold_us_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(mtk_mmmc_threshold_us_register_notifier);

u32 get_axi_mon_threshold_us(void)
{
	if (!g_mtk_axi_mon) {
		MM_MONITOR_ERR("g_mtk_axi_mon is NULL");
		return 0;
	}
	return g_mtk_axi_mon->threshold_us;
}
EXPORT_SYMBOL(get_axi_mon_threshold_us);

static int mtk_mmmc_set_threshold_us(const char *val, const struct kernel_param *kp)
{
	u32 threshold_us;
	int ret;

	ret = kstrtou32(val, 10, &threshold_us);
	if (ret) {
		MM_MONITOR_ERR("fail ret=%d", ret);
		return ret;
	}


	MM_MONITOR_INFO("update threshold_us=%u", threshold_us);
	g_mtk_axi_mon->threshold_us = threshold_us;

	raw_notifier_call_chain(&mtk_mmmc_threshold_us_notifier_list, 0, NULL);
	return 0;
}
static const struct kernel_param_ops mmmc_threshold_us_ops = {
	.set = mtk_mmmc_set_threshold_us,
};
module_param_cb(mtk_mmmc_threshold_us, &mmmc_threshold_us_ops, NULL, 0644);
MODULE_PARM_DESC(mtk_mmmc_threshold_us, "set smmu factor");

void write_axi_register(uint32_t axi_mon_id, uint32_t offset, uint32_t value)
{
	void *base;

	base = g_mtk_bwr[axi_mon_id]->base_addr_va;

	MM_MONITOR_INFO("id: %d, base:%#x, offset:%#x, value:%#x", axi_mon_id,
		g_mtk_bwr[axi_mon_id]->base_addr_pa, offset, value);

	writel(value, base + offset);
	return;
}

uint32_t read_axi_register(uint32_t axi_mon_id, uint32_t offset)
{
	void *base;
	uint32_t value;

	base = g_mtk_bwr[axi_mon_id]->base_addr_va;
	value = readl(base + offset);

	MM_MONITOR_INFO("id: %d, addr:%#x, offset:%#x, value:%#x", axi_mon_id,
		g_mtk_bwr[axi_mon_id]->base_addr_pa, offset, value);

	return value;
}

void write_axi_register_with_mask(uint32_t axi_mon_id, uint32_t offset, uint32_t mask, uint32_t value)
{
	uint32_t current_val = read_axi_register(axi_mon_id, offset);
	uint32_t new_val = (current_val & ~mask) | value;

	write_axi_register(axi_mon_id, offset, new_val);
}

void set_ostdbl_to_aximon(uint32_t axi_mon_id, uint32_t r_ostdbl, uint32_t w_ostdbl)
{
	uint32_t value;

	// 0x0D8[10:0] = r_ostdbl, [26:16] = w_ostdbl
	if (r_ostdbl & ~0x7FF) {
		MM_MONITOR_ERR("r_ostdbl %d is too large!", r_ostdbl);
		r_ostdbl = 0x7FF;
	}
	if (w_ostdbl & ~0x7FF) {
		MM_MONITOR_ERR("w_ostdbl %d is too large!", w_ostdbl);
		w_ostdbl = 0x7FF;
	}

	value = ((w_ostdbl << g_mtk_axi_mon->ostdbl_w_shift) | r_ostdbl);
	write_axi_register(axi_mon_id, MON_BWLMTE3, value);
	write_axi_register(axi_mon_id, MON_BWLMTE3_WA, value);
	MM_MONITOR_INFO("bwr id:%d, reg:%#x, value:%#x", axi_mon_id, MON_BWLMTE3, value);
	return;
}

ostdbl get_ostdbl_from_urate_lut(uint32_t freq)
{
	for (int i = 0; i < urate_lut_size; i++) {
		if (freq < urate_freq_table[i])
			return ostdbl_lut[i];
	}
	return ostdbl_lut[ostdbl_lut_size-1];
}

void mtk_calculate_ostdbl(uint32_t *r_ostdbl, uint32_t *w_ostdbl)
{
	*r_ostdbl = *r_ostdbl * g_mtk_axi_mon->ostdbl_master_r_factor / 100;
	*w_ostdbl = *w_ostdbl * g_mtk_axi_mon->ostdbl_master_w_factor / 100;

	*r_ostdbl = (*r_ostdbl == 0) ? 1 : *r_ostdbl;
	*w_ostdbl = (*w_ostdbl == 0) ? 1 : *w_ostdbl;
}

void mtk_mmmc_set_ostdbl(uint32_t hwid, uint32_t min_freq)
{
	u32 r_ostdbl, w_ostdbl;
	ostdbl freq_ostdbl;

	freq_ostdbl = get_ostdbl_from_urate_lut(min_freq);

	r_ostdbl = freq_ostdbl.r_ostdbl;
	w_ostdbl = freq_ostdbl.w_ostdbl;

	mtk_calculate_ostdbl(&r_ostdbl, &w_ostdbl);
	MM_MONITOR_INFO("[mmqos] hwid:%d, r_ostdbl:%d, freq_r_ostdbl:%d",
			hwid, r_ostdbl, freq_ostdbl.r_ostdbl);
	MM_MONITOR_INFO("[mmqos] hwid:%d, w_ostdbl:%d, freq_w_ostdbl:%d",
			hwid, w_ostdbl, freq_ostdbl.w_ostdbl);

	set_ostdbl_to_aximon(hwid, r_ostdbl, w_ostdbl);
	trace_mmmc__axi_mon_ostdbl("r", r_ostdbl, hwid);
	trace_mmmc__axi_mon_ostdbl("w", w_ostdbl, hwid);
}
EXPORT_SYMBOL(mtk_mmmc_set_ostdbl);

void mtk_mmmc_set_ostdbl_by_larb(uint32_t hwid, uint32_t avg_r_bw, uint32_t avg_w_bw,
						uint32_t peak_r_bw, uint32_t peak_w_bw, uint32_t min_freq)
{
	u32 r_ostdbl, w_ostdbl, r_srt_ostdbl = 0, w_srt_ostdbl = 0;
	ostdbl freq_ostdbl;

	freq_ostdbl = get_ostdbl_from_urate_lut(min_freq);

	r_ostdbl = freq_ostdbl.r_ostdbl + g_mtk_bwr[hwid]->ostdbl_r_nps;
	w_ostdbl = freq_ostdbl.w_ostdbl + g_mtk_bwr[hwid]->ostdbl_w_nps;

	if ((peak_r_bw == 0) && (peak_w_bw == 0)) {
		r_srt_ostdbl = avg_r_bw >> 4;
		w_srt_ostdbl = avg_w_bw >> 4;
		r_ostdbl = (r_ostdbl > r_srt_ostdbl) ? r_srt_ostdbl : r_ostdbl;
		w_ostdbl = (w_ostdbl > w_srt_ostdbl) ? w_srt_ostdbl : w_ostdbl;
	}
	mtk_calculate_ostdbl(&r_ostdbl, &w_ostdbl);
	MM_MONITOR_INFO("[mmqos]hwid:%d, r_ostdbl:%d, freq_r_ostdbl:%d, ostdbl_r_nps:%d, r_srt_ostdbl:%d",
			hwid, r_ostdbl, freq_ostdbl.r_ostdbl,
			g_mtk_bwr[hwid]->ostdbl_r_nps, r_srt_ostdbl);
	MM_MONITOR_INFO("[mmqos]hwid:%d, w_ostdbl:%d, freq_w_ostdbl:%d, ostdbl_w_nps:%d, w_srt_ostdbl:%d",
			hwid, w_ostdbl, freq_ostdbl.w_ostdbl,
			g_mtk_bwr[hwid]->ostdbl_w_nps, w_srt_ostdbl);

	set_ostdbl_to_aximon(hwid, r_ostdbl, w_ostdbl);
	trace_mmmc__axi_mon_ostdbl("r", r_ostdbl, hwid);
	trace_mmmc__axi_mon_ostdbl("w", w_ostdbl, hwid);
}
EXPORT_SYMBOL(mtk_mmmc_set_ostdbl_by_larb);

bool calculat_budget_and_shf(uint32_t value, uint32_t *budget, uint32_t *budget_shf)
{
	for (uint32_t shf = 0; shf <= g_mtk_axi_mon->max_bwl_budget_shf; shf++) {
		uint32_t bud = value >> shf;
		if (bud <= g_mtk_axi_mon->max_bwl_budget) {
			*budget = bud;
			*budget_shf = shf;
			return true;
		}
	}
	*budget = g_mtk_axi_mon->max_bwl_budget;
	*budget_shf = g_mtk_axi_mon->max_bwl_budget_shf;
	return false;
}

void set_bwl_to_aximon(uint32_t axi_mon_id, uint32_t r_total_budget, uint32_t r_threshold, uint32_t w_total_budget, uint32_t w_threshold)
{
	uint32_t r_budget = 0, r_shf = 0, w_budget = 0, w_shf = 0, value = 0;

	if(!calculat_budget_and_shf(r_total_budget, &r_budget, &r_shf)) {
		MM_MONITOR_ERR("r_total_budget %d is out of range", r_total_budget);

	}
	if(!calculat_budget_and_shf(w_total_budget, &w_budget, &w_shf)) {
		MM_MONITOR_ERR("w_total_budget %d is out of range", w_total_budget);
	}

	if (r_threshold & ~g_mtk_axi_mon->max_bwl_up_bnd) {
		MM_MONITOR_ERR("r_threshold %d is out of range", r_threshold);
		r_threshold = g_mtk_axi_mon->max_bwl_up_bnd;
	}
	if (w_threshold & ~g_mtk_axi_mon->max_bwl_up_bnd) {
		MM_MONITOR_ERR("w_threshold %d is out of range", w_threshold);
		w_threshold = g_mtk_axi_mon->max_bwl_up_bnd;
	}

	MM_MONITOR_INFO("r_budget:%d, r_shf:%d, w_budget:%d, w_shf:%d",
		r_budget, r_shf, w_budget, w_shf);
	// [31:16] = up_bnd, [9:3] = budget, [2:0] = shf
	value = ((r_threshold & g_mtk_axi_mon->max_bwl_up_bnd) << g_mtk_axi_mon->bwl_up_bnd_shift)
			| ((r_budget & g_mtk_axi_mon->max_bwl_budget) << g_mtk_axi_mon->bwl_budget_shift) | (r_shf & g_mtk_axi_mon->max_bwl_budget_shf);
	write_axi_register(axi_mon_id, MON_BWLMTE1, value);
	write_axi_register(axi_mon_id, MON_BWLMTE1_WA, value);
	MM_MONITOR_INFO("R(%#x):%#x, value:%#x",
		MON_BWLMTE1_WA, read_axi_register(axi_mon_id, MON_BWLMTE1_WA), value);

	value = ((w_threshold & g_mtk_axi_mon->max_bwl_up_bnd) << g_mtk_axi_mon->bwl_up_bnd_shift)
			| ((w_budget & g_mtk_axi_mon->max_bwl_budget) << g_mtk_axi_mon->bwl_budget_shift) | (w_shf & g_mtk_axi_mon->max_bwl_budget_shf);
	write_axi_register(axi_mon_id, MON_BWLMTE2, value);
	write_axi_register(axi_mon_id, MON_BWLMTE2_WA, value);
	MM_MONITOR_INFO("W(%#x):%#x, value:%#x",
		MON_BWLMTE2_WA, read_axi_register(axi_mon_id, MON_BWLMTE2_WA), value);
}

void mtk_mmmc_enable_axi_limiter(uint32_t hwid, uint32_t axi_mon_state)
{
	u32 config = 0;
	u32 mask = 0xc03;

	if (axi_mon_state & 1 << AXI_MON_OSTDBL)
		config = 0x403;
	if (axi_mon_state & 1 << AXI_MON_BWL)
		config |= 0x803;
	write_axi_register_with_mask(hwid, MON_BMAN2, mask, config);
	MM_MONITOR_INFO("(%#x):%#x, value:%#x",
		MON_BMAN2, read_axi_register(hwid, MON_BMAN2), config);
}
EXPORT_SYMBOL(mtk_mmmc_enable_axi_limiter);

void mtk_mmmc_set_bw_limiter(uint32_t hwid, uint32_t r_bw, uint32_t w_bw, uint32_t min_freq)
{
	uint32_t r_bwl = 0, w_bwl = 0, r_budget = 0, w_budget = 0, r_threshold = 0, w_threshold = 0;
	uint32_t line_bw = 0;

	line_bw = min_freq * BUS_WIDTH;

	r_bwl = (line_bw == 0 || r_bw < line_bw) ? r_bw : line_bw;
	w_bwl = (line_bw == 0 || w_bw < line_bw) ? w_bw : line_bw;

	r_budget = r_bwl / g_mtk_axi_mon->bwl_budget_size;
	w_budget = w_bwl / g_mtk_axi_mon->bwl_budget_size;
	r_threshold = r_bwl * g_mtk_axi_mon->threshold_us;
	w_threshold = w_bwl * g_mtk_axi_mon->threshold_us;

	r_budget = (r_budget == 0) ? 1 : r_budget;
	w_budget = (w_budget == 0) ? 1 : w_budget;
	r_threshold = (r_threshold == 0) ? 1 : r_threshold;
	w_threshold = (w_threshold == 0) ? 1 : w_threshold;

	MM_MONITOR_INFO("hwid:%d, r_bw:%d, w_bw:%d, r_bwl:%d, w_bwl:%d, "
		"r_budget:%d, r_threshold:%d, w_budget:%d, w_threshold:%d",
		hwid, r_bw, w_bw, r_bwl, w_bwl, r_budget, r_threshold, w_budget, w_threshold);
	set_bwl_to_aximon(hwid, r_budget, r_threshold, w_budget, w_threshold);
	trace_mmmc__axi_mon_bwl_threshold("r", hwid, r_threshold);
	trace_mmmc__axi_mon_bwl_threshold("w", hwid, w_threshold);
	trace_mmmc__axi_mon_bwl_budget("r", hwid, r_budget);
	trace_mmmc__axi_mon_bwl_budget("w", hwid, w_budget);
}
EXPORT_SYMBOL(mtk_mmmc_set_bw_limiter);

void enable_ela(struct mtk_mmmc_power_domain *mmmc_power_domain)
{
	u32 ela_total_cnt = mmmc_power_domain->ela_total_cnt;
	int i;

	for (i = 0; i < ela_total_cnt; i++) {
		struct mtk_ela *ela = mmmc_power_domain->ela[i];
		void *base;

		if (!ela)
			continue;

		base = ela->base_addr_va;
		/* enable ELA */
		writel(0x1, base + CTRL);
	}
}

void dump_bwr(struct mtk_mmmc_power_domain *mmmc_power_domain, s32 bwr_hwid)
{
	u32 bwr_total_cnt = mmmc_power_domain->bwr_total_cnt;
	int i;
	u32 val;
	s32 j, len, ret = 0;
	bool dump_all = (bwr_hwid == DUMP_ALL_BWR) ? true:false;

	for (i = 0; i < bwr_total_cnt; i++) {
		struct mtk_bwr *bwr = mmmc_power_domain->bwr[i];
		void *base;
		char	buf[LINE_MAX_LEN + 1] = {0};

		if (!bwr)
			continue;

		if (!dump_all && bwr_hwid != bwr->hwid)
			continue;

		base = bwr->base_addr_va;
		MM_MONITOR_DBG("BWR hwid:%d base:0x%lx pa:0x%lx power_domain:%d",
			bwr->hwid, (unsigned long)base,
			(unsigned long)bwr->base_addr_pa,
			MTK_SMI_ID2SUBSYS_ID(bwr->power_domain_id));
		for (j = 0x000, len = 0; j < 0x110; j+=4) {
			val = readl_relaxed(base + j);
			if (!val)
				continue;

			ret = snprintf(buf + len, LINE_MAX_LEN - len, " %#x=%#x,", j, val);
			if (ret < 0 || ret >= LINE_MAX_LEN - len) {
				ret = snprintf(buf + len, LINE_MAX_LEN - len, "%c", '\0');
				if (ret < 0)
					MM_MONITOR_ERR("print error:%d", ret);
				MM_MONITOR_DBG("%s", buf);
				len = 0;
				memset(buf, '\0', sizeof(char) * ARRAY_SIZE(buf));
				ret = snprintf(buf + len, LINE_MAX_LEN - len, " %#x=%#x,",
					j, val);
				if (ret < 0)
					MM_MONITOR_ERR("print error:%d", ret);
			}
			len += ret;
		}
		ret = snprintf(buf + len, LINE_MAX_LEN - len, "%c", '\0');
		if (ret < 0)
			MM_MONITOR_ERR("print error:%d", ret);
		MM_MONITOR_DBG("%s", buf);
	}
}

void dump_ela(struct mtk_mmmc_power_domain *mmmc_power_domain)
{
	u32 ela_total_cnt = mmmc_power_domain->ela_total_cnt;
	int i;
	s32 j, len, ret = 0;
	u32 val;

	for (i = 0; i < ela_total_cnt; i++) {
		struct mtk_ela *ela = mmmc_power_domain->ela[i];
		void *base;
		char	buf[LINE_MAX_LEN + 1] = {0};

		if (!ela)
			continue;

		base = ela->base_addr_va;
		MM_MONITOR_DBG("ELA hwid:%d base:0x%lx pa:0x%lx power_domain:%d",
			ela->hwid, (unsigned long)base,
			(unsigned long)ela->base_addr_pa,
			MTK_SMI_ID2SUBSYS_ID(ela->power_domain_id));
		for (j = 0x000, len = 0; j < 0x1000; j+=4) {
			val = readl_relaxed(base + j);
			if (!val)
				continue;
			ret = snprintf(buf + len, LINE_MAX_LEN - len, " %#x=%#x,", j, val);
			if (ret < 0 || ret >= LINE_MAX_LEN - len) {
				ret = snprintf(buf + len, LINE_MAX_LEN - len, "%c", '\0');
				if (ret < 0)
					MM_MONITOR_ERR("print error:%d", ret);
				MM_MONITOR_DBG("%s", buf);
				len = 0;
				memset(buf, '\0', sizeof(char) * ARRAY_SIZE(buf));
				ret = snprintf(buf + len, LINE_MAX_LEN - len, " %#x=%#x,",
					j, val);
				if (ret < 0)
					MM_MONITOR_ERR("print error:%d", ret);
			}
			len += ret;
		}
		ret = snprintf(buf + len, LINE_MAX_LEN - len, "%c", '\0');
		if (ret < 0)
			MM_MONITOR_ERR("print error:%d", ret);
		MM_MONITOR_DBG("%s", buf);
	}
}

void dump_cti(struct mtk_mmmc_power_domain *mmmc_power_domain)
{
	u32 cti_total_cnt = mmmc_power_domain->cti_total_cnt;
	int i;
	s32 j, len, ret = 0;
	u32 val;
	for (i = 0; i < cti_total_cnt; i++) {
		struct mtk_cti *cti = mmmc_power_domain->cti[i];
		void *base;
		char	buf[LINE_MAX_LEN + 1] = {0};
		if (!cti)
			continue;
		base = cti->base_addr_va;
		MM_MONITOR_DBG("CTI hwid:%d base:0x%lx pa:0x%lx power_domain:%d",
			cti->hwid, (unsigned long)base,
			(unsigned long)cti->base_addr_pa,
			MTK_SMI_ID2SUBSYS_ID(cti->power_domain_id));
		for (j = 0x000, len = 0; j < 0x1000; j+=4) {
			val = readl_relaxed(base + j);
			if (!val)
				continue;
			ret = snprintf(buf + len, LINE_MAX_LEN - len, " %#x=%#x,", j, val);
			if (ret < 0 || ret >= LINE_MAX_LEN - len) {
				ret = snprintf(buf + len, LINE_MAX_LEN - len, "%c", '\0');
				if (ret < 0)
					MM_MONITOR_ERR("print error:%d", ret);
				MM_MONITOR_DBG("%s", buf);
				len = 0;
				memset(buf, '\0', sizeof(char) * ARRAY_SIZE(buf));
				ret = snprintf(buf + len, LINE_MAX_LEN - len, " %#x=%#x,",
					j, val);
				if (ret < 0)
					MM_MONITOR_ERR("print error:%d", ret);
			}
			len += ret;
		}
		ret = snprintf(buf + len, LINE_MAX_LEN - len, "%c", '\0');
		if (ret < 0)
			MM_MONITOR_ERR("print error:%d", ret);
		MM_MONITOR_DBG("%s", buf);
	}
}

void init_bwr(struct mtk_mmmc_power_domain *mmmc_power_domain, bool dump)
{
	u32 bwr_total_cnt = mmmc_power_domain->bwr_total_cnt;
	int i;
	u32 value = 0;

	for (i = 0; i < bwr_total_cnt; i++) {
		struct mtk_bwr *bwr = mmmc_power_domain->bwr[i];
		void *base;
		bool bwr_limiter_disable = false;

		if (!bwr)
			continue;

		bwr_limiter_disable = bwr->disable_limiter;
		base = bwr->base_addr_va;
		if (bwr->bwr_ela == NULL) {
			writel(0x10, base + MON_BMAN);
		} else
			writel(0x11, base + MON_BMAN);
		writel(BIT(8) | readl(base + MON_BMAN2), base + MON_BMAN2);
		/* BW Monitor - write ultra, qos ignore */
		writel(0x12, base + MON_DBWA);
		writel(0x80000000, base + MON_DBWA_2ND);
		/* BW Monitor - read ultra, qos ignore */
		writel(0x11, base + MON_DBWB);
		writel(0x80000000, base + MON_DBWB_2ND);
		/* BW Monitor - write total, qos ignore */
		writel(0x16, base + MON_DBWC);
		writel(0x0, base + MON_DBWC_2ND);
		/* BW Monitor - read total, qos ignore */
		writel(0x15, base + MON_DBWD);
		writel(0x0, base + MON_DBWD_2ND);
		/* Latency Monitor - read x 1 / write x 1 */
		writel(0x102, base + MON_TTYPE_BWRW0);
		writel(0x16, base + MON_TTYPE0_CONA);
		writel(0x15, base + MON_TTYPE1_CONA);
		if (mmmc_state & DEF_LIMITER_ENABLE) {
			writel(0x640008, base + MON_BWLMTE1);
			writel(0x640008, base + MON_BWLMTE1_WA);
			writel(0x640008, base + MON_BWLMTE2);
			writel(0x640008, base + MON_BWLMTE2_WA);
			writel(0x100010, base + MON_BWLMTE3);
			writel(0x100010, base + MON_BWLMTE3_WA);
			if (bwr_limiter_disable)
				writel(readl(base + MON_BMAN2) & ~0xc03, base + MON_BMAN2);
			else
				writel((readl(base + MON_BMAN2) & ~0xc03) | 0xc03, base + MON_BMAN2);
		}
		if (mmmc_state & FIXED_OSTDBL_ENABLE) {
			writel(0xFFFFFFFF, base + MON_BWLMTE1);
			writel(0xFFFFFFFF, base + MON_BWLMTE1_WA);
			writel(0xFFFFFFFF, base + MON_BWLMTE2);
			writel(0xFFFFFFFF, base + MON_BWLMTE2_WA);
			value = ((bwr->w_ostdbl & 0xFFF) << 16) + (bwr->r_ostdbl & 0xFFF);
			writel(value,   base + MON_BWLMTE3);
			writel(value,   base + MON_BWLMTE3_WA);
			if (bwr_limiter_disable)
				writel(readl(base + MON_BMAN2) & ~0xc03, base + MON_BMAN2);
			else {
				writel((readl(base + MON_BMAN2) & ~0xc03) | 0x403, base + MON_BMAN2);
				MM_MONITOR_INFO("FIXED_OSTDBL_ENABLE, r_ostdbl:%d, w_ostdbl:%d, value:%#x",
					bwr->r_ostdbl, bwr->w_ostdbl, value);
			}
		}
		if (mmmc_state & CAM_ID_FILTER_ENABLE) {
			MM_MONITOR_INFO("CAM_ID_FILTER_ENABLE");
			if (bwr->cam_sel_id_0[0] != 0) {
				writel(bwr->cam_sel_id_0[0], base + MON_ID_TMP0);
				writel(bwr->cam_sel_id_0[1], base + MON_ID_MASK0);
				writel(0x1a, base + MON_DBWA);
				writel(0x1e, base + MON_TTYPE0_CONA);
				MM_MONITOR_DBG("cam_sel_id_0: %#x %#x",
					bwr->cam_sel_id_0[0], bwr->cam_sel_id_0[1]);
			}
			if (bwr->cam_sel_id_1[0] != 0) {
				writel(bwr->cam_sel_id_1[0], base + MON_ID_TMP1);
				writel(bwr->cam_sel_id_1[1], base + MON_ID_MASK1);
				writel(0x19, base + MON_DBWB);
				writel(0x1d, base + MON_TTYPE0_CONA);
				MM_MONITOR_DBG("cam_sel_id_1: %#x %#x",
					bwr->cam_sel_id_1[0], bwr->cam_sel_id_1[1]);
			}
			if (bwr->cam_sel_id_2[0] != 0) {
				writel(bwr->cam_sel_id_2[0], base + MON_ID_TMP2);
				writel(bwr->cam_sel_id_2[1], base + MON_ID_MASK2);
				writel(0x1e, base + MON_DBWC);
				MM_MONITOR_DBG("cam_sel_id_2: %#x %#x",
					bwr->cam_sel_id_2[0], bwr->cam_sel_id_2[1]);
			}
			if (bwr->cam_sel_id_3[0] != 0) {
				writel(bwr->cam_sel_id_3[0], base + MON_ID_TMP3);
				writel(bwr->cam_sel_id_3[1], base + MON_ID_MASK3);
				writel(0x1d, base + MON_DBWD);
				MM_MONITOR_DBG("cam_sel_id_3: %#x %#x",
					bwr->cam_sel_id_3[0], bwr->cam_sel_id_3[1]);
			}
		}
	}
	if (dump)
		dump_bwr(mmmc_power_domain, DUMP_ALL_BWR);
}

void init_ela(struct mtk_mmmc_power_domain *mmmc_power_domain, bool dump)
{
	u32 ela_total_cnt = mmmc_power_domain->ela_total_cnt;
	int i, j;

	for (i = 0; i < ela_total_cnt; i++) {
		struct mtk_ela *ela = mmmc_power_domain->ela[i];
		void *base;

		if (!ela)
			continue;

		base = ela->base_addr_va;
		/* disable ELA */
		writel(0x0, base + CTRL);
		/* reset ELA */
		for (j = 0; j < ELA_SIG_MAX; j++) {
			writel(0x0, base + COMPCTRL(j));
			writel(0x0, base + ALTCOMPCTRL(j));
			writel(0x0, base + EXTMASK(j));
			writel(0x0, base + EXTCOMP(j));
			writel(0x0, base + QUALMASK(j));
			writel(0x0, base + QUALCOMP(j));
			writel(0x0, base + SIGMASK_31_0(j));
			writel(0x0, base + SIGMASK_63_32(j));
			writel(0x0, base + SIGMASK_95_64(j));
			writel(0x0, base + SIGMASK_127_96(j));
		}
		/* scenario setting */
		writel(0x4, base + EXTMASK(0));
		for (j = 0; j < ELA_SIG_MAX; j++) {
			writel(BIT(j), base + SIGSEL(j));
			writel(0xffff, base + TWBSEL(j));
		}
		writel(0x08, base + PTACTION);
		for (j = 0; j < ELA_SIG_MAX; j++)
			writel(0x08, base + ACTION(j));
		writel(0x17000, base + TIMECTRL);
		/* trigger state setting */
		writel(0x21, base + TRIGCTRL(0));
		writel(0x2, base + NEXTSTATE(0));
		writel(0x4, base + EXTCOMP(0));
		for (j = 1; j < ELA_SIG_MAX; j++) {
			writel(0x158, base + TRIGCTRL(j));
			writel(16, base + COUNTCOMP(j));
		}
		for (j = 1; j < ELA_SIG_MAX - 1; j++) {
			writel(BIT(j + 1), base + NEXTSTATE(j));
		}
		writel(0x01, base + NEXTSTATE(7));
		/* set ATB ID */
		writel((ela->hwid + ELA_HW_ID_INIT) << 8, base + ATBCTRL);
	}
	if (dump)
		dump_ela(mmmc_power_domain);
}

void init_cti(struct mtk_mmmc_power_domain *mmmc_power_domain, bool dump)
{
	u32 cti_total_cnt = mmmc_power_domain->cti_total_cnt;
	int i, j;

	for (i = 0; i < cti_total_cnt; i++) {
		struct mtk_cti *cti = mmmc_power_domain->cti[i];
		void *base;

		if (!cti)
			continue;
		MM_MONITOR_DBG("CTI hwid=%d", i);
		base = cti->base_addr_va;
		/* enable CTI */
		writel(0xC5ACCE55, base + LAR);
		writel(0x01, base + CTICONTROL);
		/* CTI settings */
		for (j = 0; j < cti->cti_in_settings; j++) {
			writel(0x1 << cti->cti_in_chnn_start[j][1],
				base + CTIINEN0(cti->cti_in_chnn_start[j][0]));
			writel(0x1 << cti->cti_in_chnn_stop[j][1],
				base + CTIINEN0(cti->cti_in_chnn_stop[j][0]));
		}
		if (cti->cti_out_chnn_start[0] != -1)
			writel(0x1 << cti->cti_out_chnn_start[1],
				base + CTIOUTEN0(cti->cti_out_chnn_start[0]));
		if (cti->cti_out_chnn_stop[0] != -1)
			writel(0x1 << cti->cti_out_chnn_stop[1],
				base + CTIOUTEN0(cti->cti_out_chnn_stop[0]));
		if (mmmc_state & CTI_SW_ENABLE) {
			writel(0x2, base + CTIAPPSET);
		}
	}
	if (dump)
		dump_cti(mmmc_power_domain);
}

s32 mtk_set_mmmc_rg(u32 hw, u32 id, u32 offset, u32 value, u32 mask)
{
	void *base = NULL;
	u32 base_addr_pa = 0;
	bool limiter_debug = false;

	switch (hw) {
	case MM_AXI:
		if (id >= BWR_NUM_MAX || g_mtk_bwr[id] == NULL) {
			MM_MONITOR_ERR("unknown HW:%d id:%d", hw, id);
			return -EINVAL;
		}
		base = g_mtk_bwr[id]->base_addr_va;
		base_addr_pa = g_mtk_bwr[id]->base_addr_pa;
		if (offset >= 0xd0 && offset <= 0xd8)
			limiter_debug = true;
		break;
	case MM_ELA:
		if (id >= ELA_NUM_MAX || g_mtk_ela[id] == NULL) {
			MM_MONITOR_ERR("unknown HW:%d id:%d", hw, id);
			return -EINVAL;
		}
		base = g_mtk_ela[id]->base_addr_va;
		base_addr_pa = g_mtk_ela[id]->base_addr_pa;
		break;
	case MM_CTI:
		if (id >= CTI_NUM_MAX || g_mtk_cti[id] == NULL) {
			MM_MONITOR_ERR("unknown HW:%d id:%d", hw, id);
			return -EINVAL;
		}
		base = g_mtk_cti[id]->base_addr_va;
		base_addr_pa = g_mtk_cti[id]->base_addr_pa;
		break;
	case MM_MMINFRA:
		base = g_mtk_mminfra2_config->base_addr_va;
		base_addr_pa = g_mtk_mminfra2_config->base_addr_pa;
		break;
	default:
		return 0;
	}

	MM_MONITOR_INFO("pa:0x%lx base:%lx value:%#x read value:%#x before set",
		(unsigned long)base_addr_pa + offset,
		(unsigned long)base + offset, value, readl(base + offset));
	writel((readl(base + offset) & ~mask) | (value & mask), base + offset);
	if (limiter_debug) {
		offset -= 0x50;
		writel((readl(base + offset) & ~mask) | (value & mask), base + offset);
	}
	MM_MONITOR_INFO("pa:0x%lx base:%lx value:%#x read value:%#x after set",
		(unsigned long)base_addr_pa + offset,
		(unsigned long)base + offset, value, readl(base + offset));

	return 0;
}
EXPORT_SYMBOL(mtk_set_mmmc_rg);

u32 mtk_init_monitor_by_subsys_id(u32 subsys_id, bool dump_and_force_init)
{
	struct mtk_mmmc_power_domain *mmmc_power_domain;
	mmmc_power_domain = g_mmmc_power_domain[subsys_id];

	if (!mmmc_power_domain) {
		MM_MONITOR_ERR("power_domain:%d empty data", subsys_id);
		return -EINVAL;
	}
	MM_MONITOR_DBG("subsys_id:%d bwr_cnt:%d ela_cnt:%d cti_cnt:%d hrt_debug_enabled:%d kernel_no_ctrl:%d",
		subsys_id, mmmc_power_domain->bwr_total_cnt,
		mmmc_power_domain->ela_total_cnt, mmmc_power_domain->cti_total_cnt,
		hrt_debug_enabled, mmmc_power_domain->kernel_no_ctrl);

	if (!hrt_debug_enabled || mmmc_power_domain->kernel_no_ctrl)
		return 0;

	if (dump_and_force_init || (mmmc_state & MONITOR_ENABLE)) {
		if (subsys_id == MTK_SMI_ID2SUBSYS_ID(get_mminfra_pd()))
			enable_mminfra_funnel();
		init_bwr(mmmc_power_domain, dump_and_force_init);
		init_ela(mmmc_power_domain, dump_and_force_init);
		init_cti(mmmc_power_domain, dump_and_force_init);
		enable_ela(mmmc_power_domain);
	}

	return 0;
}

u32 mtk_init_monitor(u32 power_domain_id, bool dump_and_force_init)
{
	u32 subsys_id;

	subsys_id = MTK_SMI_ID2SUBSYS_ID(power_domain_id);
	return mtk_init_monitor_by_subsys_id(subsys_id, dump_and_force_init);
}
EXPORT_SYMBOL(mtk_init_monitor);

u32 mtk_dump_monitor_by_subsys_id(u32 subsys_id)
{
	struct mtk_mmmc_power_domain *mmmc_power_domain;
	mmmc_power_domain = g_mmmc_power_domain[subsys_id];

	if (!mmmc_power_domain) {
		MM_MONITOR_ERR("power_domain:%d empty data", subsys_id);
		return -EINVAL;
	}

	MM_MONITOR_DBG("subsys_id:%d bwr_cnt:%d ela_cnt:%d cti_cnt:%d kernel_no_ctrl:%d",
		subsys_id, mmmc_power_domain->bwr_total_cnt,
		mmmc_power_domain->ela_total_cnt, mmmc_power_domain->cti_total_cnt,
		mmmc_power_domain->kernel_no_ctrl);

	if (mmmc_power_domain->kernel_no_ctrl)
		return 0;

	dump_bwr(mmmc_power_domain, DUMP_ALL_BWR);
	dump_ela(mmmc_power_domain);
	dump_cti(mmmc_power_domain);

	return 0;
}

u32 mtk_dump_monitor(u32 power_domain_id)
{
	u32 subsys_id;

	subsys_id = MTK_SMI_ID2SUBSYS_ID(power_domain_id);
	return mtk_dump_monitor_by_subsys_id(subsys_id);
}
EXPORT_SYMBOL(mtk_dump_monitor);

s32 mtk_dump_bwr(u32 power_domain_id, s32 bwr_hwid)
{
	u32 subsys_id;
	struct mtk_mmmc_power_domain *mmmc_power_domain;

	subsys_id = MTK_SMI_ID2SUBSYS_ID(power_domain_id);
	mmmc_power_domain = g_mmmc_power_domain[subsys_id];
	if (!mmmc_power_domain) {
		MM_MONITOR_ERR("power_domain:%d empty data", subsys_id);
		return -EINVAL;
	}

	dump_bwr(mmmc_power_domain, bwr_hwid);

	return 0;
}
EXPORT_SYMBOL(mtk_dump_bwr);

int mtk_mmmc_reinit(const char *val, const struct kernel_param *kp)
{
	u32 result, subsys_id;
	int index, power_domain_id = -1;

	result = sscanf(val, "%d", &subsys_id);
	if (result != 1 || subsys_id > get_mmmc_subsys_max()) {
		MM_MONITOR_ERR("subsys_id:%d set reinit fail", subsys_id);

		return result;
	}

	if (!hrt_debug_enabled) {
		MM_MONITOR_ERR("restrict api in user load, hrt_debug_enabled:%d", hrt_debug_enabled);
		return 0;
	}

	if (subsys_id == get_mmmc_subsys_max()) {
		for (index = 0; index < get_mmmc_subsys_max(); index++) {
			if (!g_mmmc_power_domain[index])
				continue;
			power_domain_id = g_mmmc_power_domain[index]->power_domain_id;
			if (g_mmmc_power_domain[index]->kernel_no_ctrl) {
				MM_MONITOR_DBG("power_domain_id:%d kernel_no_control:%d",
					power_domain_id, g_mmmc_power_domain[index]->kernel_no_ctrl);
					continue;
			}
			result = mtk_init_monitor(power_domain_id, true /* dump & force init */);
		}
	} else {
		result = mtk_init_monitor_by_subsys_id(subsys_id, true);
	}

	return result;
}
static const struct kernel_param_ops mmmc_reinit_ops = {
	.set = mtk_mmmc_reinit,
};
module_param_cb(mtk_mmmc_reinit, &mmmc_reinit_ops, NULL, 0644);
MODULE_PARM_DESC(mtk_mmmc_reinit, "reinit to default settings by power domain");

int mtk_mmmc_dump(const char *val, const struct kernel_param *kp)
{
	u32 result, subsys_id;
	int index, power_domain_id;

	result = sscanf(val, "%d", &subsys_id);
	if (result != 1 || subsys_id > get_mmmc_subsys_max()) {
		MM_MONITOR_ERR("subsys_id:%d dump fail", subsys_id);

		return result;
	}

	if (!hrt_debug_enabled) {
		MM_MONITOR_ERR("restrict api in user load, hrt_debug_enabled:%d", hrt_debug_enabled);
		return 0;
	}

	if (subsys_id == get_mmmc_subsys_max()) {
		for (index = 0; index < get_mmmc_subsys_max(); index++) {
			if (!g_mmmc_power_domain[index])
				continue;
			power_domain_id = g_mmmc_power_domain[index]->power_domain_id;
			if (g_mmmc_power_domain[index]->kernel_no_ctrl) {
				MM_MONITOR_DBG("power_domain_id:%d kernel_no_control:%d",
					power_domain_id, g_mmmc_power_domain[index]->kernel_no_ctrl);
					continue;
			}
			result = mtk_dump_monitor(power_domain_id);
		}
	} else {
		result = mtk_dump_monitor(subsys_id);
	}

	return result;
}
static const struct kernel_param_ops mmmc_dump_ops = {
	.set = mtk_mmmc_dump,
};
module_param_cb(mtk_mmmc_dump, &mmmc_dump_ops, NULL, 0644);
MODULE_PARM_DESC(mtk_mmmc_dump, "dump all settings by power domain");

int mtk_mmmc_set_rg(const char *val, const struct kernel_param *kp)
{
	u32 hw, id, offset, value, mask;
	s32 result;

	result = sscanf(val, "%d %d %x %x %x", &hw, &id, &offset, &value, &mask);
	if (result != 5 || hw > MM_MONITOR_ENGINE_MAX) {
		MM_MONITOR_ERR("hw:%d id:%d offset:%#x value:%#x set fail",
			hw, id, offset, value);
		return result;
	}

	if (!hrt_debug_enabled) {
		MM_MONITOR_ERR("restrict api in user load, hrt_debug_enabled:%d", hrt_debug_enabled);
		return 0;
	}

	MM_MONITOR_DBG("hw:%d id:%d offset:%#x value:%#x mask:%#x set result:%d",
		hw, id, offset, value, mask, result);

	result = is_valid_offset_value(hw, id, offset, value);
	if (result < 0) {
		MM_MONITOR_ERR("hw:%d id:%d offset:%#x value:%#x invalid value",
			hw, id, offset, value);
		return result;
	}

	result = mtk_set_mmmc_rg(hw, id, offset, value, mask);
	if (result < 0) {
		MM_MONITOR_ERR("hw:%d id:%d offset:%#x value:%#x invalid id",
			hw, id, offset, value);
		return result;
	}

	return 0;
}
static const struct kernel_param_ops mmmc_set_ops = {
	.set = mtk_mmmc_set_rg,
};
module_param_cb(mtk_mmmc_set_rg, &mmmc_set_ops, NULL, 0644);
MODULE_PARM_DESC(mtk_mmmc_set_rg, "set register values by HWID");

int mtk_mmmc_fake_engine(const char *val, const struct kernel_param *kp)
{
	u32 result, id, wr_pat, length, burst, dis_rd, dis_wr, latency, loop, dma, start;
	u32 *dma_va = NULL, dma_mask_bit = 0;
	dma_addr_t dma_pa = 0;
	struct device *dev;
	void *base;
	struct fake_engine *engine;

	result = sscanf(val, "%d %d %d %d %d %d %d %d %d %d",
		&id, &wr_pat, &length, &burst, &dis_rd, &dis_wr, &latency, &loop, &dma, &start);
	if (result != 10 || latency > 1023) {
		MM_MONITOR_ERR("id:%d wr_pat:%d len:%d burst:%d dis_r:%d dis_w:%d lat:%d loop:%d dma:%d start:%d",
			id, wr_pat, length, burst, dis_rd, dis_wr, latency, loop, dma, start);

		return result;
	}

	if (!hrt_debug_enabled) {
		MM_MONITOR_ERR("restrict api in user load, hrt_debug_enabled:%d", hrt_debug_enabled);
		return 0;
	}

	dev = g_mtk_mm_fake_engine->dev;

	dma_mask_bit = 32;
	result = dma_set_coherent_mask(mtk_smmu_get_shared_device(dev), DMA_BIT_MASK(dma_mask_bit));
	if (result) {
		MM_MONITOR_ERR("set dma mask bit:%u failed:%d", dma_mask_bit, result);
		return result;
	}
	dma_va = dma_alloc_coherent(mtk_smmu_get_shared_device(dev), BUF_ALLOC_SIZE, &dma_pa, GFP_KERNEL);
	if (!dma_va) {
		MM_MONITOR_ERR("alloc dma buffer fail dev:0x%p failed", dev);
		return -ENOMEM;
	}

	base = g_mtk_mm_fake_engine -> base_addr_va;
	engine = g_mtk_mm_fake_engine->fake_engines[id];
	base = base + engine->offset;

	mminfra_fake_engine_bus_settings();

	// switch larb to fake engine
	writel(0x1, base + FAKE_ENG0_SWITCH);
	MM_MONITOR_DBG("FAKE_ENG0_SWITCH 0x%lx = 0x%x",
		(unsigned long)(base + FAKE_ENG0_SWITCH), readl(base + FAKE_ENG0_SWITCH));
	// start address
	if (dma == 0) { //default value
		writel(MMINFRA_START_ADDR(id), base + FAKE_ENG0_WR_ADDR);
		MM_MONITOR_DBG("MMINFRA_START_ADDR 0x%lx = 0x%x",
			(unsigned long)(base + FAKE_ENG0_WR_ADDR), readl(base + FAKE_ENG0_WR_ADDR));
		writel(MMINFRA_START_ADDR(id), base + FAKE_ENG0_RD_ADDR);
		MM_MONITOR_DBG("MMINFRA_START_ADDR 0x%lx = 0x%x",
			(unsigned long)(base + FAKE_ENG0_RD_ADDR), readl(base + FAKE_ENG0_RD_ADDR));
	} else if (dma == 1) { //dma value
		writel(dma_pa, base + FAKE_ENG0_WR_ADDR);
		MM_MONITOR_DBG("MMINFRA_START_ADDR 0x%lx = 0x%x",
			(unsigned long)(base + FAKE_ENG0_WR_ADDR), readl(base + FAKE_ENG0_WR_ADDR));
		writel(dma_pa, base + FAKE_ENG0_RD_ADDR);
		MM_MONITOR_DBG("MMINFRA_START_ADDR 0x%lx = 0x%x",
			(unsigned long)(base + FAKE_ENG0_RD_ADDR), readl(base + FAKE_ENG0_RD_ADDR));
	}
	// disable channel/emi dispatch
	writel(0x0, base + FAKE_ENG0_HASH);
	MM_MONITOR_DBG("FAKE_ENG0_HASH 0x%lx = 0x%x",
		(unsigned long)(base + FAKE_ENG0_HASH), readl(base + FAKE_ENG0_HASH));
	// fake engine con setting
	writel((0x1 << 2) | (dis_wr << 1) | dis_rd, base + FAKE_ENG0_CON0);
	MM_MONITOR_DBG("FAKE_ENG0_CON0 0x%lx = 0x%x",
		(unsigned long)(base + FAKE_ENG0_CON0), readl(base + FAKE_ENG0_CON0));
	writel(burst | (0x4 << 4), base + FAKE_ENG0_CON2);
	MM_MONITOR_DBG("FAKE_ENG0_CON2 0x%lx = 0x%x",
		(unsigned long)(base + FAKE_ENG0_CON2), readl(base + FAKE_ENG0_CON2));
	writel((latency << 10) | (0x1f << 5) | 0x1f, base + FAKE_ENG0_CON1);
	MM_MONITOR_DBG("FAKE_ENG0_CON1 0x%lx = 0x%x",
		(unsigned long)(base + FAKE_ENG0_CON1), readl(base + FAKE_ENG0_CON1));
	writel(0x1, base + FAKE_ENG0_CON3);
	MM_MONITOR_DBG("FAKE_ENG0_CON3 0x%lx = 0x%x",
		(unsigned long)(base + FAKE_ENG0_CON3), readl(base + FAKE_ENG0_CON3));
	// enable fake engine
	if (start)
		writel(0x1, base + FAKE_ENG0_EN);
	else
		writel(0x0, base + FAKE_ENG0_EN);
	MM_MONITOR_DBG("FAKE_ENG0_EN 0x%lx = 0x%x",
		(unsigned long)(base + FAKE_ENG0_EN), readl(base + FAKE_ENG0_EN));

	return 0;
}
static const struct kernel_param_ops mmmc_tests_ops = {
	.set = mtk_mmmc_fake_engine,
};
module_param_cb(mtk_mmmc_fake_engine, &mmmc_tests_ops, NULL, 0644);
MODULE_PARM_DESC(mtk_mmmc_fake_engine, "fake engine read / write test");

static int mmmc_smi_monitor(void *data)
{
	u64 dump_mmmc = (u64)data;
	void __iomem *comm0_va = g_mtk_mm_fake_engine->smi_mon_comm0_va;
	void __iomem *comm1_va = g_mtk_mm_fake_engine->smi_mon_comm1_va;
	int index;
	struct mtk_mmmc_power_domain *mmmc_power_domain;

	while (!kthread_should_stop()) {
		/* monitor start */
		if (comm0_va) {
			writel(0x1, comm0_va + SMI_MON_CLR);
			writel(0xA, comm0_va + SMI_MON_TYPE);
			writel(mmmc_smi_mon_comm0, comm0_va + SMI_MON_CON);
			writel(0x3, comm0_va + SMI_MON_ENA);
			MM_MONITOR_DBG("comm0 clr:%#x type:%#x con:%#x ena:%#x",
				readl(comm0_va + SMI_MON_CLR), readl(comm0_va + SMI_MON_TYPE),
				readl(comm0_va + SMI_MON_CON), readl(comm0_va + SMI_MON_ENA));
		}

		if (comm1_va) {
			writel(0x1, comm1_va + SMI_MON_CLR);
			writel(0xA, comm1_va + SMI_MON_TYPE);
			writel(mmmc_smi_mon_comm1, comm1_va + SMI_MON_CON);
			writel(0x3, comm1_va + SMI_MON_ENA);
			MM_MONITOR_DBG("comm1 clr:%#x type:%#x con:%#x ena:%#x",
				readl(comm1_va + SMI_MON_CLR), readl(comm1_va + SMI_MON_TYPE),
				readl(comm1_va + SMI_MON_CON), readl(comm1_va + SMI_MON_ENA));
		}

		msleep(mmmc_smi_mon_interval);
		/* monitor stop */
		if (comm0_va) {
			writel(0x0, comm0_va + SMI_MON_ENA);
			MM_MONITOR_DBG("comm0 SMI_MON_P0RD:%u, SMI_MON_P0WR:%u, SMI_MON_P1RD:%u, SMI_MON_P1WR:%u",
				readl(comm0_va + SMI_MON_BYT_P0RD),
				readl(comm0_va + SMI_MON_BYT_P0WR),
				readl(comm0_va + SMI_MON_BYT_P1RD),
				readl(comm0_va + SMI_MON_BYT_P1WR));
		}
		if (comm1_va) {
			writel(0x0, comm1_va + SMI_MON_ENA);
			MM_MONITOR_DBG("comm1 SMI_MON_P0RD:%u, SMI_MON_P0WR:%u, SMI_MON_P1RD:%u, SMI_MON_P1WR:%u",
				readl(comm1_va + SMI_MON_BYT_P0RD),
				readl(comm1_va + SMI_MON_BYT_P0WR),
				readl(comm1_va + SMI_MON_BYT_P1RD),
				readl(comm1_va + SMI_MON_BYT_P1WR));
		}
		if (dump_mmmc) {
			for (index = 0; index < get_mmmc_subsys_max(); index++) {
				if (!g_mmmc_power_domain[index])
					continue;

				mmmc_power_domain = g_mmmc_power_domain[index];
				dump_bwr(mmmc_power_domain, DUMP_ALL_BWR);
			}
		}
		msleep(mmmc_smi_mon_dump_interval);
	}

	return 0;
}
int mtk_mmmc_smi_mon(const char *val, const struct kernel_param *kp)
{
	u32 result, comm, start;
	u64 dump_mmmc;

	if (!hrt_debug_enabled) {
		MM_MONITOR_ERR("restrict api in user load, hrt_debug_enabled:%d", hrt_debug_enabled);
		return 0;
	}

	result = sscanf(val, "%d %llu %d", &comm, &dump_mmmc, &start);
	if (result != 3 || comm > 1) {
		MM_MONITOR_ERR("smi monitor comm:%d dump_mmmc:%llu", comm, dump_mmmc);
		return result;
	}

	if (start) {
		smi_mon_kthr = kthread_run(mmmc_smi_monitor,
			(void *)dump_mmmc, "mmmc_smi_monitor");
		if (IS_ERR(smi_mon_kthr))
			MM_MONITOR_ERR("Unable to run smi monitor kthread err %ld", PTR_ERR(smi_mon_kthr));
	} else if (!start) {
		if (smi_mon_kthr) {
			kthread_stop(smi_mon_kthr);
			MM_MONITOR_DBG("Stop smi monitor kthread");
		}
	}

	return 0;
}
static const struct kernel_param_ops mmmc_smi_mon_ops = {
	.set = mtk_mmmc_smi_mon,
};
module_param_cb(mtk_mmmc_smi_mon, &mmmc_smi_mon_ops, NULL, 0644);
MODULE_PARM_DESC(mtk_mmmc_smi_mon, "start smi monitor kthrd");

static int mmmc_emi_monitor(void *data)
{
	void __iomem *base = g_mtk_mminfra2_config->emi_16qos_monitor_base;
	s32 j, len, ret = 0;
	u32 val;

	while (!kthread_should_stop()) {
		char	buf[LINE_MAX_LEN + 1] = {0};

		msleep(1000);
		for (j = 0xd70, len = 0; j <= 0xdac; j+=4) {
			val = readl_relaxed(base + j);
			ret = snprintf(buf + len, LINE_MAX_LEN - len, " %#x=%#x,", j, val);
			if (ret < 0 || ret >= LINE_MAX_LEN - len) {
				ret = snprintf(buf + len, LINE_MAX_LEN - len, "%c", '\0');
				if (ret < 0)
					MM_MONITOR_ERR("print error:%d", ret);
				MM_MONITOR_DBG("%s", buf);
				len = 0;
				memset(buf, '\0', sizeof(char) * ARRAY_SIZE(buf));
				ret = snprintf(buf + len, LINE_MAX_LEN - len, " %#x=%#x,",
					j, val);
				if (ret < 0)
					MM_MONITOR_ERR("print error:%d", ret);
			}
			len += ret;
		}
		ret = snprintf(buf + len, LINE_MAX_LEN - len, "%c", '\0');
		if (ret < 0)
			MM_MONITOR_ERR("print error:%d", ret);
		MM_MONITOR_DBG("%s", buf);
	}

	return 0;
}
int mtk_mmmc_emi_monitor(const char *val, const struct kernel_param *kp)
{
	u32 result, start, filter;
	void __iomem *base = g_mtk_mminfra2_config->emi_16qos_monitor_base;

	if (!hrt_debug_enabled) {
		MM_MONITOR_ERR("restrict api in user load, hrt_debug_enabled:%d", hrt_debug_enabled);
		return 0;
	}

	result = sscanf(val, "%d %d", &start, &filter);

	if (result != 2) {
		MM_MONITOR_ERR("emi monitor set:%d", start);
		return result;
	}

	if (start) {
		emi_moniter_settings();
		/* filter */
		if (filter == 0) // qos level
			writel(readl(base + 0xb24) & ~(1 << 13), base + 0xb24);
		else if (filter == 1) // master id
			writel(readl(base + 0xb24) | (1 << 13), base + 0xb24);
		writel(0xffffffff, base + 0xd68);
		/* window */
		writel(readl(base + 0xd6c) | 0x3ff, base + 0xd6c);

		/* enable */
		writel(readl(base + 0xc10) | 0x1, base + 0xc10);
		writel(readl(base + 0xd64) | 0xffff, base + 0xd64);
		emi_mon_kthr = kthread_run(mmmc_emi_monitor, NULL, "mmmc_emi_monitor");
		if (IS_ERR(emi_mon_kthr))
			MM_MONITOR_ERR("Unable to run emi monitor kthread err %ld",
				PTR_ERR(emi_mon_kthr));
	} else if (!start) {
		if (emi_mon_kthr) {
			kthread_stop(emi_mon_kthr);
			MM_MONITOR_DBG("Stop emi monitor kthread");
		}
	}

	return 0;
}
static const struct kernel_param_ops mmmc_emi_mon_ops = {
	.set = mtk_mmmc_emi_monitor,
};
module_param_cb(mtk_mmmc_emi_monitor, &mmmc_emi_mon_ops, NULL, 0644);
MODULE_PARM_DESC(mtk_mmmc_emi_monitor, "start emi monitor kthrd");


int mtk_mm_axi_mon_limiter_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int ret;
	char	buf[LINE_MAX_LEN + 1] = {0};
	s32 len = 0;

	if (!np) {
		MM_MONITOR_ERR("Device node is NULL");
		return -EINVAL;
	}
	urate_lut_size = of_property_count_u32_elems(np, "urate-freq-table");
	if (urate_lut_size == 0) {
		MM_MONITOR_ERR("Failed to get urate-freq-table size");
		return -EINVAL;
	}
	urate_freq_table = devm_kzalloc(dev, urate_lut_size * sizeof(*urate_freq_table), GFP_KERNEL);
	if (!urate_freq_table) {
		MM_MONITOR_ERR("Failed to allocate memory for urate_freq_table");
		return -ENOMEM;
	}
	ret = of_property_read_u32_array(np, "urate-freq-table", urate_freq_table, urate_lut_size);
	if (ret) {
		MM_MONITOR_ERR("Failed to read urate-freq-table");
		return ret;
	}

	ostdbl_lut_size = of_property_count_u32_elems(np, "ostdbl-lut") / 2;
	if (ostdbl_lut_size == 0) {
		MM_MONITOR_ERR("Failed to get ostdbl-lut size");
		return -EINVAL;
	}
	ostdbl_lut = kzalloc(ostdbl_lut_size * sizeof(*ostdbl_lut), GFP_KERNEL);
	if (!ostdbl_lut) {
		MM_MONITOR_ERR("Failed to allocate memory for ostdbl_lut");
		return -ENOMEM;
	}
	ret = of_property_read_u32_array(np, "ostdbl-lut", (uint32_t *)ostdbl_lut, ostdbl_lut_size * 2);
	if (ret) {
		kfree(ostdbl_lut);
		MM_MONITOR_ERR("Failed to read ostdbl-lut");
		return ret;
	}

	MM_MONITOR_DBG("urate_freq_table: ");
	for (int i = 0; i < urate_lut_size; i++) {
		ret = snprintf(buf + len, LINE_MAX_LEN - len, " %u ", urate_freq_table[i]);
		if (ret < 0)
			MM_MONITOR_ERR("print error:%d", ret);
		len += ret;
	}
	MM_MONITOR_DBG("%s", buf);

	MM_MONITOR_DBG("ostdbl_lut: ");
	for (int i = 0; i < ostdbl_lut_size; i++) {
		MM_MONITOR_DBG("r_ostdbl = %u, w_ostdbl = %u", ostdbl_lut[i].r_ostdbl, ostdbl_lut[i].w_ostdbl);
	}

	g_mtk_axi_mon = devm_kzalloc(&pdev->dev, sizeof(*g_mtk_axi_mon), GFP_KERNEL);
	if (!g_mtk_axi_mon) {
		MM_MONITOR_ERR("[mmqos]probe axi:error");
		return -ENOMEM;
	}

	of_property_read_u32(np, "ostdbl-master-r-factor", &g_mtk_axi_mon->ostdbl_master_r_factor);
	of_property_read_u32(np, "ostdbl-master-w-factor", &g_mtk_axi_mon->ostdbl_master_w_factor);

	of_property_read_u32(np, "ostdbl-w-shift", &g_mtk_axi_mon->ostdbl_w_shift);
	of_property_read_u32(np, "max-bwl-budget-shf", &g_mtk_axi_mon->max_bwl_budget_shf);
	of_property_read_u32(np, "max-bwl-budget", &g_mtk_axi_mon->max_bwl_budget);
	of_property_read_u32(np, "max-bwl-up-bnd", &g_mtk_axi_mon->max_bwl_up_bnd);
	of_property_read_u32(np, "bwl-budget-shift", &g_mtk_axi_mon->bwl_budget_shift);
	of_property_read_u32(np, "bwl-up-bnd-shift", &g_mtk_axi_mon->bwl_up_bnd_shift);
	of_property_read_u32(np, "bwl-budget-size", &g_mtk_axi_mon->bwl_budget_size);
	of_property_read_u32(np, "threshold-us", &g_mtk_axi_mon->threshold_us);

	of_property_read_u32(np, "ostdbl-bef-smmu-r-factor", &g_mtk_axi_mon->ostdbl_bef_smmu_r_factor);
	of_property_read_u32(np, "ostdbl-bef-smmu-w-factor", &g_mtk_axi_mon->ostdbl_bef_smmu_w_factor);
	of_property_read_u32(np, "ostdbl-af-smmu-r-factor", &g_mtk_axi_mon->ostdbl_af_smmu_r_factor);
	of_property_read_u32(np, "ostdbl-af-smmu-w-factor", &g_mtk_axi_mon->ostdbl_af_smmu_w_factor);

	return 0;
}

#define COMMON_SIZE	16
#define LARB_SIZE	10
int mtk_mm_bwr_probe(struct platform_device *pdev, u32 power_domain_id)
{
	struct device *dev = &pdev->dev;
	struct mtk_bwr *mtk_bwr;
	struct device_node *node;
	struct resource *res;
	u32 hwid, bwr_index = 0, subsys_id;
	u32 commid = -1, larbid = -1;
	int ret;

	if (of_property_read_u32(dev->of_node, "axi-common-id", &commid) == 0 ||
		of_property_read_u32(dev->of_node, "axi-larb-id", &larbid) == 0) {
		if (!g_mtk_axi_mon) {
			MM_MONITOR_ERR("g_mtk_axi_mon is null");
			return -EPROBE_DEFER;
		}
	}

	mtk_bwr = devm_kzalloc(dev, sizeof(*mtk_bwr), GFP_KERNEL);
	if (!mtk_bwr) {
		MM_MONITOR_ERR("failed to alloc");
		return -ENOMEM;
	}

	subsys_id = MTK_SMI_ID2SUBSYS_ID(power_domain_id);
	of_property_read_u32(dev->of_node, "hw-id", &hwid);
	bwr_index = g_mmmc_power_domain[subsys_id]->bwr_total_cnt;
	g_mmmc_power_domain[subsys_id]->bwr[bwr_index] = mtk_bwr;
	mtk_bwr->hwid = hwid;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		MM_MONITOR_ERR("failed to get resource");
		return -EINVAL;
	}

	mtk_bwr->base_addr_va = devm_ioremap_resource(dev, res);
	mtk_bwr->base_addr_pa = res->start;
	if (IS_ERR(mtk_bwr->base_addr_va)) {
		MM_MONITOR_ERR("failed to ioremap mm_bwr hwid:%d", mtk_bwr->hwid);
		return PTR_ERR(mtk_bwr->base_addr_va);
	}

	g_bwr_cnt++;
	g_mtk_bwr[hwid] = mtk_bwr;
	g_mmmc_power_domain[subsys_id]->bwr_total_cnt++;
	mtk_bwr->power_domain_id = power_domain_id;

	MM_MONITOR_DBG("BWR hwid:%d base:0x%lx pa:0x%lx power_domain:%d total_bwr_cnt:%d",
		mtk_bwr->hwid, (unsigned long)mtk_bwr->base_addr_va,
		(unsigned long)mtk_bwr->base_addr_pa, subsys_id, g_bwr_cnt);
	of_property_read_u32(dev->of_node, "ostdbl-r-nps", &mtk_bwr->ostdbl_r_nps);
	of_property_read_u32(dev->of_node, "ostdbl-w-nps", &mtk_bwr->ostdbl_w_nps);

	mtk_bwr->r_ostdbl = mmmc_fixed_r_ostdbl;
	mtk_bwr->w_ostdbl = mmmc_fixed_w_ostdbl;

	node = of_parse_phandle(dev->of_node, "bwr-ela", 0);
	if (node) {
		mtk_bwr->bwr_ela = of_find_device_by_node(node);
	} else
		mtk_bwr->bwr_ela = NULL;

	if (of_property_read_bool(dev->of_node, "disable-limiter"))
		mtk_bwr->disable_limiter = true;

	MM_MONITOR_DBG("BWR hwid:%d ostdbl_r_nps:%d ostdbl_w_nps:%d bwr_ela:%s disable_limiter:%d",
		mtk_bwr->hwid, mtk_bwr->ostdbl_r_nps, mtk_bwr->ostdbl_w_nps,
		node ? dev_name(&mtk_bwr->bwr_ela->dev) : "NULL",
		mtk_bwr->disable_limiter);

	if (of_property_read_u32(dev->of_node, "axi-common-id", &commid) == 0) {
		if (g_mtk_axi_mon->aximon_comm_map_size == 0)
			aximon_comm_map = devm_krealloc(dev, aximon_comm_map,
				COMMON_SIZE * sizeof(*aximon_comm_map), GFP_KERNEL);
		if (!aximon_comm_map) {
			MM_MONITOR_ERR("failed to alloc comm map size");
			return -ENOMEM;
		}
		aximon_comm_map[g_mtk_axi_mon->aximon_comm_map_size].input.comm_id = commid;
		of_property_read_u32(dev->of_node, "axi-port-id",
			&aximon_comm_map[g_mtk_axi_mon->aximon_comm_map_size].input.port_id);
		aximon_comm_map[g_mtk_axi_mon->aximon_comm_map_size].output.axi_mon_id = hwid;
		of_property_read_u32(dev->of_node, "axi-mux-id",
			&aximon_comm_map[g_mtk_axi_mon->aximon_comm_map_size].output.mux_id);
		g_mtk_axi_mon->aximon_comm_map_size++;
	}

	if (of_property_read_u32(dev->of_node, "axi-larb-id", &larbid) == 0) {
		if (g_mtk_axi_mon->aximon_larb_map_size == 0)
			aximon_larb_map = devm_krealloc(dev, aximon_larb_map,
				LARB_SIZE * sizeof(*aximon_larb_map), GFP_KERNEL);
		if (!aximon_larb_map) {
			MM_MONITOR_ERR("failed to alloc larb map size");
			return -ENOMEM;
		}
		aximon_larb_map[g_mtk_axi_mon->aximon_larb_map_size].larb_id = commid;
		aximon_larb_map[g_mtk_axi_mon->aximon_larb_map_size].output.axi_mon_id = hwid;
		of_property_read_u32(dev->of_node, "axi-mux-id",
			&aximon_larb_map[g_mtk_axi_mon->aximon_larb_map_size].output.mux_id);
		g_mtk_axi_mon->aximon_larb_map_size++;
	}

	ret = of_property_read_u32_array(dev->of_node, "cam_sel_id_0", mtk_bwr->cam_sel_id_0, 2);
	if (ret)
		MM_MONITOR_INFO("Failed to read cam_sel_id_0");

	ret = of_property_read_u32_array(dev->of_node, "cam_sel_id_1", mtk_bwr->cam_sel_id_1, 2);
	if (ret)
		MM_MONITOR_INFO("Failed to read cam_sel_id_1");

	ret = of_property_read_u32_array(dev->of_node, "cam_sel_id_2", mtk_bwr->cam_sel_id_2, 2);
	if (ret)
		MM_MONITOR_INFO("Failed to read cam_sel_id_2");

	ret = of_property_read_u32_array(dev->of_node, "cam_sel_id_3", mtk_bwr->cam_sel_id_3, 2);
	if (ret)
		MM_MONITOR_INFO("Failed to read cam_sel_id_3");

	MM_MONITOR_DBG("cam_sel_id 0: %#x %#x 1: %#x %#x 2: %#x %#x 3: %#x %#x",
		mtk_bwr->cam_sel_id_0[0], mtk_bwr->cam_sel_id_0[1],
		mtk_bwr->cam_sel_id_1[0], mtk_bwr->cam_sel_id_1[1],
		mtk_bwr->cam_sel_id_2[0], mtk_bwr->cam_sel_id_2[1],
		mtk_bwr->cam_sel_id_3[0], mtk_bwr->cam_sel_id_3[1]);
	return 0;
}

int mtk_mm_ela_probe(struct platform_device *pdev, u32 power_domain_id)
{
	struct device *dev = &pdev->dev;
	struct mtk_ela *mtk_ela;
	struct device_node *node;
	struct resource *res;
	u32 hwid, ela_index = 0, subsys_id;

	mtk_ela = devm_kzalloc(dev, sizeof(*mtk_ela), GFP_KERNEL);
	if (!mtk_ela) {
		MM_MONITOR_ERR("failed to alloc");
		return -ENOMEM;
	}

	subsys_id = MTK_SMI_ID2SUBSYS_ID(power_domain_id);
	of_property_read_u32(dev->of_node, "hw-id", &hwid);
	ela_index = g_mmmc_power_domain[subsys_id]->ela_total_cnt;
	g_mmmc_power_domain[subsys_id]->ela[ela_index] = mtk_ela;
	mtk_ela->hwid = hwid;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		MM_MONITOR_ERR("failed to get resource");
		return -EINVAL;
	}

	mtk_ela->base_addr_va = devm_ioremap_resource(dev, res);
	mtk_ela->base_addr_pa = res->start;
	if (IS_ERR(mtk_ela->base_addr_va)) {
		MM_MONITOR_ERR("failed to ioremap mm_ela hwid:%d", mtk_ela->hwid);
		return PTR_ERR(mtk_ela->base_addr_va);
	}

	g_ela_cnt++;
	g_mtk_ela[hwid] = mtk_ela;
	g_mmmc_power_domain[subsys_id]->ela_total_cnt++;
	mtk_ela->power_domain_id = power_domain_id;

	node = of_parse_phandle(dev->of_node, "ela-cti", 0);
	if (node) {
		mtk_ela->ela_cti = of_find_device_by_node(node);
	} else
		mtk_ela->ela_cti = NULL;
	MM_MONITOR_DBG("ELA hwid:%d base:0x%lx pa:0x%lx power_domain:%d ela_cti:%s",
		mtk_ela->hwid, (unsigned long)mtk_ela->base_addr_va,
		(unsigned long)mtk_ela->base_addr_pa, subsys_id,
		dev ? dev_name(&mtk_ela->ela_cti->dev) : "NULL");

	return 0;
}
int mtk_mm_cti_probe(struct platform_device *pdev, u32 power_domain_id)
{
	struct device *dev = &pdev->dev;
	struct mtk_cti *mtk_cti;
	struct device_node *node;
	struct resource *res;
	u32 hwid, cti_index = 0, subsys_id;
	const __be32 *cti_in_stop = NULL, *cti_in_start = NULL;
	int len_start = 0, len_stop = 0, i;
	s32 err;

	mtk_cti = devm_kzalloc(dev, sizeof(*mtk_cti), GFP_KERNEL);
	if (!mtk_cti) {
		MM_MONITOR_ERR("failed to alloc");
		return -ENOMEM;
	}

	subsys_id = MTK_SMI_ID2SUBSYS_ID(power_domain_id);
	of_property_read_u32(dev->of_node, "hw-id", &hwid);
	cti_index = g_mmmc_power_domain[subsys_id]->cti_total_cnt;
	g_mmmc_power_domain[subsys_id]->cti[cti_index] = mtk_cti;
	mtk_cti->hwid = hwid;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		MM_MONITOR_ERR("failed to get resource");
		return -EINVAL;
	}

	g_cti_cnt++;
	g_mtk_cti[hwid] = mtk_cti;
	g_mmmc_power_domain[subsys_id]->cti_total_cnt++;
	mtk_cti->base_addr_va = devm_ioremap_resource(dev, res);
	mtk_cti->base_addr_pa = res->start;
	if (IS_ERR(mtk_cti->base_addr_va)) {
		MM_MONITOR_ERR("failed to ioremap mm_cti hwid:%d", mtk_cti->hwid);
		return PTR_ERR(mtk_cti->base_addr_va);
	}

	mtk_cti->power_domain_id = power_domain_id;
	node = of_parse_phandle(dev->of_node, "cti-ela", 0);
	if (node) {
		mtk_cti->cti_ela = of_find_device_by_node(node);
	} else
		mtk_cti->cti_ela = NULL;

	MM_MONITOR_DBG("CTI hwid:%d base:0x%lx pa:0x%lx power_domain:%d",
		mtk_cti->hwid, (unsigned long)mtk_cti->base_addr_va,
		(unsigned long)mtk_cti->base_addr_pa, subsys_id);

	cti_in_stop = of_get_property(dev->of_node, "cti-in-chnn-stop", &len_stop);
	cti_in_start = of_get_property(dev->of_node, "cti-in-chnn-start", &len_start);
	if (len_stop % (sizeof(__be32) * 2) != 0 || len_start % (sizeof(__be32) * 2) != 0) {
		MM_MONITOR_ERR("Invalid length for cti-in-chnn-stop or cti-in-chnn-start");
		return -EINVAL;
	}
	len_stop /= (sizeof(__be32) * 2);
	for (i = 0; i < len_stop; i++) {
		mtk_cti->cti_in_settings++;
		mtk_cti->cti_in_chnn_stop[i][0] = be32_to_cpu(cti_in_stop[(i * 2)]);
		mtk_cti->cti_in_chnn_stop[i][1]	= be32_to_cpu(cti_in_stop[(i * 2 + 1)]);
		mtk_cti->cti_in_chnn_start[i][0] = be32_to_cpu(cti_in_start[(i * 2)]);
		mtk_cti->cti_in_chnn_start[i][1] = be32_to_cpu(cti_in_start[(i * 2 + 1)]);
		MM_MONITOR_DBG("CTI hwid:%d set:%d cti_in_chnn_stop:%d %d cti_in_chnn_start:%d %d",
			mtk_cti->hwid, i,
			mtk_cti->cti_in_chnn_stop[i][0], mtk_cti->cti_in_chnn_stop[i][1],
			mtk_cti->cti_in_chnn_start[i][0], mtk_cti->cti_in_chnn_start[i][1]);
	}

	err = of_property_read_u32_array(dev->of_node, "cti-out-chnn-stop",
		mtk_cti->cti_out_chnn_stop, 2);
	if (err)
		mtk_cti->cti_out_chnn_stop[0] = -1;

	err = of_property_read_u32_array(dev->of_node, "cti-out-chnn-start",
		mtk_cti->cti_out_chnn_start, 2);
	if (err)
		mtk_cti->cti_out_chnn_start[0] = -1;

	MM_MONITOR_DBG("CTI hwid:%d cti_out_chnn_stop:%d %d cti_out_chnn_start:%d %d",
		mtk_cti->hwid, mtk_cti->cti_out_chnn_stop[0], mtk_cti->cti_out_chnn_stop[1],
        mtk_cti->cti_out_chnn_start[0], mtk_cti->cti_out_chnn_start[1]);

	return 0;
}

int mtk_mmmc_config_probe(struct platform_device *pdev, u32 power_domain_id,
	struct power_domain_cb *power_domain_callbacks)
{
	struct device *dev = &pdev->dev;
	struct mtk_mminfra2_config *mtk_mminfra2_config;
	u32 emi_monitor_pa;
	struct resource *res;
	int len = 0, i, ret;
	const __be32 *smi_cb = NULL;
	struct mtk_mmmc_power_domain *mmmc_power_domain;
	struct device_node *chosen_node;
	const char *name = NULL;

	chosen_node = of_find_node_by_path("/chosen");
	if (chosen_node) {
		ret = of_property_read_string_index(chosen_node, "mtk_fabric_hrt_debug", 0, &name);
		if (!ret && (!strncmp("on", name, sizeof("on"))))
			hrt_debug_enabled = true;
		else
			hrt_debug_enabled = false;
	}

	mtk_mminfra2_config = devm_kzalloc(dev, sizeof(*mtk_mminfra2_config), GFP_KERNEL);
	if (!mtk_mminfra2_config) {
		MM_MONITOR_ERR("failed to alloc");
		return -ENOMEM;
	}

	g_mtk_mminfra2_config = mtk_mminfra2_config;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		MM_MONITOR_ERR("failed to get resource");
		return -EINVAL;
	}

	mtk_mminfra2_config->base_addr_va = devm_ioremap_resource(dev, res);
	mtk_mminfra2_config->base_addr_pa = res->start;
	if (IS_ERR(mtk_mminfra2_config->base_addr_va)) {
		MM_MONITOR_ERR("failed to ioremap mtk_mminfra2_config");
		return PTR_ERR(mtk_mminfra2_config->base_addr_va);
	}

	mtk_mminfra2_config->power_domain_id = power_domain_id;

	if (!of_property_read_u32(dev->of_node, "emi-16qos-monitor", &emi_monitor_pa))
		mtk_mminfra2_config->emi_16qos_monitor_base = ioremap(emi_monitor_pa, 0x1000);

	MM_MONITOR_DBG("MMINFRA2 base:0x%lx pa:0x%lx power_domain:%d emi_16qos_monitor:%#x",
		(unsigned long)mtk_mminfra2_config->base_addr_va,
		(unsigned long)mtk_mminfra2_config->base_addr_pa,
		MTK_SMI_ID2SUBSYS_ID(power_domain_id), emi_monitor_pa);

	of_property_read_u32(dev->of_node, "16qos-enable-offset",
		&mtk_mminfra2_config->enable_16qos_offset);
	if (of_property_read_bool(dev->of_node, "16qos-debug-enable"))
		mtk_mminfra2_config->enable_16qos = true;
	if (of_property_read_bool(dev->of_node, "use-subsys-16qos"))
		mtk_mminfra2_config->use_subsys_16qos = true;

	MM_MONITOR_DBG("MMINFRA2 enable_16qos_offset:%#x enable_16qos:%d use_subsys_16qos:%d",
		mtk_mminfra2_config->enable_16qos_offset,
		mtk_mminfra2_config->enable_16qos, mtk_mminfra2_config->use_subsys_16qos);

	of_property_read_u32(dev->of_node, "ultra2qos-disp-offset",
		&mtk_mminfra2_config->ultra2qos_disp_offset);
	of_property_read_u32(dev->of_node, "mminfra-ultra2qos0-val",
		&mtk_mminfra2_config->mminfra_ultra2qos0_val);
	MM_MONITOR_DBG("MMINFRA2 ultra2qos_disp_offset:%#x mminfra_ultra2qos0_val:%#x",
		mtk_mminfra2_config->ultra2qos_disp_offset,
		mtk_mminfra2_config->mminfra_ultra2qos0_val);

	of_property_read_u32(dev->of_node, "ultra2qos-mdp-offset",
		&mtk_mminfra2_config->ultra2qos_mdp_offset);
	of_property_read_u32(dev->of_node, "mminfra-ultra2qos1-val",
		&mtk_mminfra2_config->mminfra_ultra2qos1_val);
	MM_MONITOR_DBG("MMINFRA2 ultra2qos_mdp_offset:%#x mminfra_ultra2qos1_val:%#x",
		mtk_mminfra2_config->ultra2qos_mdp_offset,
		mtk_mminfra2_config->mminfra_ultra2qos1_val);

	smi_cb = of_get_property(dev->of_node, "power-init-cb", &len);
	if (len > 0) {
		len /= sizeof(__be32);
		for (i = 0; i < len; i++) {
			u32 subsys_id = MTK_SMI_ID2SUBSYS_ID(be32_to_cpu(smi_cb[i]));
			MM_MONITOR_DBG("subsys_id:%d smi pwr cb", subsys_id);
			if (!g_mmmc_power_domain[subsys_id]) {
				g_mmmc_power_domain[subsys_id] = devm_kzalloc(dev, sizeof(*mmmc_power_domain), GFP_KERNEL);
				g_mmmc_power_domain_cnt++;
			}
			g_mmmc_power_domain[subsys_id]->smi_nb.notifier_call = power_domain_callbacks[i].callback;
			mtk_smi_pd_register_notifier(&g_mmmc_power_domain[subsys_id]->smi_nb, subsys_id);
		}
	}

	of_property_read_u32(dev->of_node, "mmmc-state", &mmmc_state);
	MM_MONITOR_DBG("mmmc state: %#x", mmmc_state);

	return 0;
}

int mtk_mm_fake_engine_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int i;
	struct mtk_mm_fake_engine *mtk_mm_fake_engine;
	struct resource *res;
#ifdef MMMC_SUPPORT_FPGA
	void __iomem	*mminfra_va_1, *mminfra_va_2, *mminfra_va_ao;
#endif

	mtk_mm_fake_engine = devm_kzalloc(dev, sizeof(*mtk_mm_fake_engine), GFP_KERNEL);
	if (!mtk_mm_fake_engine) {
		MM_MONITOR_ERR("failed to alloc");
		return -ENOMEM;
	}

	g_mtk_mm_fake_engine = mtk_mm_fake_engine;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		MM_MONITOR_ERR("failed to get resource");
		return -EINVAL;
	}

	mtk_mm_fake_engine->base_addr_va = devm_ioremap_resource(dev, res);
	mtk_mm_fake_engine->base_addr_pa = res->start;
	if (IS_ERR(mtk_mm_fake_engine->base_addr_va)) {
		MM_MONITOR_ERR("failed to ioremap mm_fake_engine");
		return PTR_ERR(mtk_mm_fake_engine->base_addr_va);
	}
	MM_MONITOR_DBG("Fake Engine base:0x%lx pa:0x%lx",
		(unsigned long)mtk_mm_fake_engine->base_addr_va,
		(unsigned long)mtk_mm_fake_engine->base_addr_pa);

	if (!of_property_read_u32(dev->of_node, "smi-monitor-comm0",
		&mtk_mm_fake_engine->smi_mon_comm0_pa))
		mtk_mm_fake_engine->smi_mon_comm0_va = ioremap(mtk_mm_fake_engine->smi_mon_comm0_pa, 0x1000);
	if (!of_property_read_u32(dev->of_node, "smi-monitor-comm1",
		&mtk_mm_fake_engine->smi_mon_comm1_pa))
		mtk_mm_fake_engine->smi_mon_comm1_va = ioremap(mtk_mm_fake_engine->smi_mon_comm1_pa, 0x1000);
	MM_MONITOR_DBG("SMI monitor comm0 va:0x%lx pa:0x%lx comm1 va:0x%lx pa:0x%lx",
		(unsigned long)mtk_mm_fake_engine->smi_mon_comm0_va,
		(unsigned long)mtk_mm_fake_engine->smi_mon_comm0_pa,
		(unsigned long)mtk_mm_fake_engine->smi_mon_comm1_va,
		(unsigned long)mtk_mm_fake_engine->smi_mon_comm1_pa);

	mtk_mm_fake_engine->dev = &pdev->dev;
	g_fake_engines_cnt = of_property_count_strings(dev->of_node, "fake-engine-names");
	for (i = 0; i < g_fake_engines_cnt; i++) {
		struct fake_engine *engine;
		const char *name;
		struct of_phandle_args args;

		if (of_property_read_string_index(dev->of_node, "fake-engine-names",
			i, &name))
			break;
		engine = devm_kzalloc(dev, sizeof(*engine), GFP_KERNEL);
		if (!engine) {
			MM_MONITOR_ERR("failed to alloc");
			return -ENOMEM;
		}
		mtk_mm_fake_engine->fake_engines[i] = engine;
		engine->fake_engine_name = name;
		engine->fake_engine_id = i;
		of_parse_phandle_with_args(dev->of_node, "fake-engines", "#fake-engine-cells",
			i, &args);
		engine->offset = args.args[0];
		MM_MONITOR_DBG("Fake Engine idx:%d name:%s offset:%#x",
			i, engine->fake_engine_name, engine->offset);
	}

#ifdef MMMC_SUPPORT_FPGA
	MM_MONITOR_DBG("Fake Engine enter sleep protect");
	/* enable fpga sleep protect */
	mminfra_va_1 = ioremap(0x30000000, 0x1000);
	mminfra_va_2 = ioremap(0x30b00000, 0x1000);
	mminfra_va_ao = ioremap(0x30080000, 0x1000);
	writel(0x0, mminfra_va_1 + 0x9c8);
	writel(0x0, mminfra_va_1 + 0x9c0);
	writel(0x0, mminfra_va_1 + 0x9d0);
	writel(0x0, mminfra_va_1 + 0x9d8);
	writel(0x0, mminfra_va_2 + 0x97c);
	writel(0x0, mminfra_va_ao + 0x428);
#endif

	return 0;
}
static int mm_monitor_controller_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mm_monitor_engine *plat_data;
	//struct of_phandle_args args;
	u32 power_domain_id, subsys_id;
	struct mtk_mmmc_power_domain *mmmc_power_domain;
	int ret = 0;

	plat_data = (struct mm_monitor_engine *)of_device_get_match_data(dev);
	if (!plat_data) {
		MM_MONITOR_ERR("failed to get match data");
		return -EINVAL;
	}

	of_property_read_u32(dev->of_node, "power-domain", &power_domain_id);
	subsys_id = MTK_SMI_ID2SUBSYS_ID(power_domain_id);
	if (!g_mmmc_power_domain[subsys_id]) {
		mmmc_power_domain = devm_kzalloc(dev, sizeof(*mmmc_power_domain), GFP_KERNEL);
		if (!mmmc_power_domain) {
			MM_MONITOR_ERR("power_domain_id:%d failed to alloc struct",
				subsys_id);
			return -ENOMEM;
		}
		g_mmmc_power_domain[subsys_id] = mmmc_power_domain;
		mmmc_power_domain->power_domain_id = power_domain_id;
		g_mmmc_power_domain_cnt++;

		if (of_property_read_bool(dev->of_node, "mmmc-kernel-no-ctrl"))
			mmmc_power_domain->kernel_no_ctrl = true;
	}

	switch (plat_data->engine) {
		case MM_AXI:
			ret = mtk_mm_bwr_probe(pdev, power_domain_id);
			break;
		case MM_ELA:
			ret = mtk_mm_ela_probe(pdev, power_domain_id);
			break;
		case MM_CTI:
			ret = mtk_mm_cti_probe(pdev, power_domain_id);
			break;
		case MM_MMINFRA:
			ret = mtk_mmmc_config_probe(pdev, power_domain_id, plat_data->pd_cb);
			break;
		case MM_FAKE_ENGINE:
			ret = mtk_mm_fake_engine_probe(pdev);
			break;
		case MM_AXI_LIMITER:
			ret = mtk_mm_axi_mon_limiter_probe(pdev);
			break;
		default:
			break;
	}

	return ret;
}

int mmmc_pd_MT6993_SMI_PD_DISP_VCORE_cb(struct notifier_block *nb,
	unsigned long is_on, void *v)
{
	mtk_init_monitor(get_power_domains(DISP_VCORE), false);
	return 0;
}
int mmmc_pd_MT6993_SMI_PD_ISP_VCORE_cb(struct notifier_block *nb,
	unsigned long is_on, void *v)
{
	mtk_init_monitor(get_power_domains(ISP_VCORE), false);
	return 0;
}
int mmmc_pd_MT6993_SMI_PD_CAM_VCORE_cb(struct notifier_block *nb,
	unsigned long is_on, void *v)
{
	mtk_init_monitor(get_power_domains(CAM_VCORE), false);
	return 0;
}
int mmmc_pd_MT6993_SMI_PD_CAM_RAWA_cb(struct notifier_block *nb,
	unsigned long is_on, void *v)
{
	mtk_init_monitor(get_power_domains(CAM_RAWA), false);
	return 0;
}
int mmmc_pd_MT6993_SMI_PD_CAM_RAWB_cb(struct notifier_block *nb,
	unsigned long is_on, void *v)
{
	mtk_init_monitor(get_power_domains(CAM_RAWB), false);
	return 0;
}
int mmmc_pd_MT6993_SMI_PD_CAM_RAWC_cb(struct notifier_block *nb,
	unsigned long is_on, void *v)
{
	mtk_init_monitor(get_power_domains(CAM_RAWC), false);
	return 0;
}
int mmmc_pd_MT6993_SMI_PD_VEN_MDP_cb(struct notifier_block *nb,
	unsigned long is_on, void *v)
{
	mtk_init_monitor(get_power_domains(VEN_MDP), false);
	return 0;
}

struct power_domain_cb power_domain_callbacks_mt6993[] = {
	{ .callback = mmmc_pd_MT6993_SMI_PD_DISP_VCORE_cb },
	{ .callback = mmmc_pd_MT6993_SMI_PD_ISP_VCORE_cb },
	{ .callback = mmmc_pd_MT6993_SMI_PD_CAM_VCORE_cb },
	{ .callback = mmmc_pd_MT6993_SMI_PD_CAM_RAWA_cb },
	{ .callback = mmmc_pd_MT6993_SMI_PD_CAM_RAWB_cb },
	{ .callback = mmmc_pd_MT6993_SMI_PD_CAM_RAWC_cb },
	{ .callback = mmmc_pd_MT6993_SMI_PD_VEN_MDP_cb },
};

static const struct mm_monitor_engine mm_axi = {.engine = MM_AXI};
static const struct mm_monitor_engine mm_ela = {.engine = MM_ELA};
static const struct mm_monitor_engine mm_cti = {.engine = MM_CTI};
static const struct mm_monitor_engine mm_mminfra_mt6993 = {.engine = MM_MMINFRA, .pd_cb = power_domain_callbacks_mt6993};
static const struct mm_monitor_engine mm_fake_engine = {.engine = MM_FAKE_ENGINE};
static const struct mm_monitor_engine mm_axi_limiter = {.engine = MM_AXI_LIMITER};

static const struct of_device_id mm_monitor_of_ids[] = {
	{.compatible = "mediatek,mm-axi-monitor", .data = (void *)&mm_axi},
	{.compatible = "mediatek,mm-ela", .data = (void *)&mm_ela},
	{.compatible = "mediatek,mm-cti", .data = (void *)&mm_cti},
	{.compatible = "mediatek,mm-fake-engine", .data = (void *)&mm_fake_engine},
	{.compatible = "mediatek,mm-axi-monitor-limiter", .data = (void *)&mm_axi_limiter},
	{.compatible = "mediatek,mmmc-config-mt6993", .data = (void *)&mm_mminfra_mt6993},
	{}
};

static struct platform_driver mm_monitor_drv = {
	.probe = mm_monitor_controller_probe,
	.driver = {
		.name = MM_MONITOR_DRIVER_NAME,
		.of_match_table = mm_monitor_of_ids,
	}
};

static __init int mm_monitor_init(void)
{
	u32 ret = 0;
	MM_MONITOR_DBG("enter");
	ret = platform_driver_register(&mm_monitor_drv);
	if (ret) {
		MM_MONITOR_ERR("platform driver register failed:%d", ret);
		return ret;
	}
	return 0;
}

int mmmc_set_ostdbl_r_factor(const char *val, const struct kernel_param *kp)
{
	u32 r_factor = 0;
	int ret;

	ret = kstrtou32(val, 0, &r_factor);

	if (ret) {
		MM_MONITOR_ERR("failed:%d r_factor:%d", ret, r_factor);
		return ret;
	}

	g_mtk_axi_mon->ostdbl_master_r_factor = r_factor;

	return 0;
}


static const struct kernel_param_ops mmmc_set_ostdbl_r_factor_ops = {
	.set = mmmc_set_ostdbl_r_factor,
	.get = param_get_uint,
};
module_param_cb(ostdbl_r_factor, &mmmc_set_ostdbl_r_factor_ops, NULL, 0644);
MODULE_PARM_DESC(ostdbl_r_factor, "set ostdbl r factor");

int mmmc_set_ostdbl_w_factor(const char *val, const struct kernel_param *kp)
{
	u32 w_factor = 0;
	int ret;

	ret = kstrtou32(val, 0, &w_factor);

	if (ret) {
		MM_MONITOR_ERR("failed:%d w_factor:%d", ret, w_factor);
		return ret;
	}

	g_mtk_axi_mon->ostdbl_master_w_factor = w_factor;

	return 0;
}

static const struct kernel_param_ops mmmc_set_ostdbl_w_factor_ops = {
	.set = mmmc_set_ostdbl_w_factor,
	.get = param_get_uint,
};
module_param_cb(ostdbl_w_factor, &mmmc_set_ostdbl_w_factor_ops, NULL, 0644);
MODULE_PARM_DESC(ostdbl_w_factor, "set ostdbl w factor");

int mmmc_set_threshold_us(const char *val, const struct kernel_param *kp)
{
	u32 threshold_us = 0;
	int ret;

	ret = kstrtou32(val, 0, &threshold_us);

	if (ret) {
		MM_MONITOR_ERR("failed:%d threshold_us:%d", ret, threshold_us);
		return ret;
	}

	g_mtk_axi_mon->threshold_us = threshold_us;

	return 0;
}

static const struct kernel_param_ops mmmc_set_threshold_us_ops = {
	.set = mmmc_set_threshold_us,
	.get = param_get_uint,
};
module_param_cb(threshold_us, &mmmc_set_threshold_us_ops, NULL, 0644);
MODULE_PARM_DESC(threshold_us, "set threshold us");

module_init(mm_monitor_init);
MODULE_LICENSE("GPL v2");
