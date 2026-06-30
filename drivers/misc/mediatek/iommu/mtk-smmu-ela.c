// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Mingyuan Ma <mingyuan.ma@mediatek.com>
 * This driver adds support for SMMU performance monitor via ELA.
 */

#if IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)
#define pr_fmt(fmt)    "mtk_smmu: ela " fmt

#include <linux/device.h>
#include <linux/export.h>
#include <linux/errno.h>
#include <linux/types.h>

#include "mtk-iommu-util.h"
#include "mtk-smmu-ela.h"
#include "mtk-smmu-v3.h"

#define SMMU_ELA_SNAPSHOT_MAX		(6)
#define SMMU_ELA_MONITOR_MAX		(24)
#define SMMU_WPCFG_OFFSET		(0x1e0000)

#define TCU_EVT_TRANSCATION		(0x1)
#define TBU_EVT_TLB_MISS		(0x2)
#define PMU_SPAN_EXACT_MATCH		(0)
#define PMU_SPAN_PATTERN_MATCH		(1)
#define PMU_FILTER_SID_MASK		(0xffff)
#define PMU_MONITOR_MAX_EVT		(0xe7)

#define SMMU_ELA_DUMP_MAX		(PAGE_SIZE - 1)
#define SMMU_ELA_INPUT_MAX		(512)

#define dump_ela(file, fmt, args...)				\
	do {							\
		if (file)					\
			seq_printf(file, fmt, ##args);		\
		else						\
			pr_info(fmt, ##args);			\
	} while (0)

struct smmu_pmu {
	void __iomem		*reg_base;
	void __iomem		*reloc_base;
	u32 			num_cntrs;
	u64			supp_evts[2];
};

struct smmu_ela_ctrl {
	bool			inited;
	void __iomem 		*wp_base;
	struct smmu_pmu		pmu[SMMU_TXU_CNT_MAX];
};

static const char *HRT_DEBUG_EN_PROP_NAME = "mtk_fabric_hrt_debug";
static const char *CHOSEN_NODE_PATH = "/chosen";
static struct smmu_ela_ctrl *ela_ctrl[SMMU_TYPE_NUM];
static int pmu_cntr0_evt[] = {
	TCU_EVT_TRANSCATION,
	TBU_EVT_TLB_MISS,
	TBU_EVT_TLB_MISS,
	TBU_EVT_TLB_MISS,
	TBU_EVT_TLB_MISS,
};

static inline bool smmu_ela_inited(u32 smmu_type)
{
	return smmu_type < SMMU_TYPE_NUM && ela_ctrl[smmu_type] &&
	       ela_ctrl[smmu_type]->inited;
}

static void smmu_enable_hwpmu(u32 smmu_type)
{
	smmu_write_field(ela_ctrl[smmu_type]->wp_base, SMMUWP_GLB_CTL0,
			 CTL0_CFG_HWPMU_EN, CTL0_CFG_HWPMU_EN);
}

static void smmu_disable_hwpmu(u32 smmu_type)
{
	smmu_write_field(ela_ctrl[smmu_type]->wp_base, SMMUWP_GLB_CTL0,
			 CTL0_CFG_HWPMU_EN, 0);
}

static bool smmu_hwpmu_enabled(u32 smmu_type)
{
	return !!smmu_read_field(ela_ctrl[smmu_type]->wp_base,
				 SMMUWP_GLB_CTL0, CTL0_CFG_HWPMU_EN);
}

static void smmu_pmu_counter_enable(void __iomem *pmu_reg, u32 idx)
{
	smmu_write_reg64(pmu_reg, SMMU_PMCG_CNTENSET0, BIT(idx));
}

static void smmu_pmu_counter_disable(void __iomem *pmu_reg, u32 idx)
{
	smmu_write_reg64(pmu_reg, SMMU_PMCG_CNTENCLR0, BIT(idx));
}

static void smmu_pmu_counter_set_val(void __iomem *pmu_reloc, u32 idx, u64 val)
{
        smmu_write_reg(pmu_reloc, SMMU_PMCG_EVCNTR(idx, 4), val);
}

static void smmu_pmu_enable(void __iomem *pmu_reg)
{
	smmu_write_reg(pmu_reg, SMMU_PMCG_CR, SMMU_PMCG_CR_ENABLE);
}

static void smmu_pmu_disable(void __iomem *pmu_reg)
{
	smmu_write_reg(pmu_reg, SMMU_PMCG_CR, 0);
}

static void smmu_pmu_set_evtyper(void __iomem *pmu_reg, u32 idx, u32 val)
{
	smmu_write_reg(pmu_reg, SMMU_PMCG_EVTYPER(idx), val);
}

static void smmu_pmu_set_smr(void __iomem *pmu_reg, u32 idx, u32 val)
{
	smmu_write_reg(pmu_reg, SMMU_PMCG_SMR(idx), val);
}

static void smmu_pmu_set_event_filter(void __iomem *pmu_reg, u32 idx,
				      u32 evt_id, u32 span)
{
	u32 evtyper = evt_id | span << SMMU_PMCG_SID_SPAN_SHIFT;;

	smmu_pmu_set_evtyper(pmu_reg, idx, evtyper);
	if (idx == 0)
		smmu_pmu_set_smr(pmu_reg, idx, PMU_FILTER_SID_MASK);
}

static void smmu_pmu_reset(struct smmu_pmu *pmu)
{
	smmu_pmu_disable(pmu->reg_base);
	smmu_write_reg64(pmu->reg_base, SMMU_PMCG_CNTENCLR0,
			 GENMASK_ULL(pmu->num_cntrs - 1, 0));
}

static void smmu_pmu_add_event(struct smmu_pmu *pmu, u32 idx, u32 evt_id)
{
	smmu_pmu_set_event_filter(pmu->reg_base, idx, evt_id,
				  PMU_SPAN_PATTERN_MATCH);
	smmu_pmu_counter_set_val(pmu->reloc_base, idx, 0);
	smmu_pmu_counter_enable(pmu->reg_base, idx);
}

static void smmu_pmu_del_event(struct smmu_pmu *pmu, u32 idx)
{
	smmu_pmu_counter_disable(pmu->reg_base, idx);
}

static bool smmu_pmu_check_event(struct smmu_pmu *pmu, u32 evt_id)
{
	if (evt_id < 64) {
		if (!(BIT(evt_id) & pmu->supp_evts[0]))
			return false;
        } else if (evt_id < SMMU_PMCG_ARCH_MAX_EVENTS) {
		if (!(BIT(evt_id - 64) & pmu->supp_evts[1]))
			return false;
	} else if (evt_id > PMU_MONITOR_MAX_EVT) {
		return false;
	}

	return true;
}

static void smmu_start_pmu(u32 smmu_type)
{
	struct smmu_pmu *pmu;
	int i;

	for (i = 0; i < SMMU_TXU_CNT(smmu_type); i++) {
		pmu = &ela_ctrl[smmu_type]->pmu[i];
		smmu_pmu_reset(pmu);
		smmu_pmu_add_event(pmu, 0, pmu_cntr0_evt[i]);
		smmu_pmu_enable(pmu->reg_base);
	}
}

static void smmu_stop_pmu(u32 smmu_type)
{
	struct smmu_pmu *pmu;
	int i;

	for (i = 0; i < SMMU_TXU_CNT(smmu_type); i++) {
		pmu = &ela_ctrl[smmu_type]->pmu[i];
		smmu_pmu_disable(pmu->reg_base);
		smmu_pmu_del_event(pmu, 0);
	}
}

static void smmu_setup_hwpmu_snapshot(u32 smmu_type)
{
	void __iomem *wp_base = ela_ctrl[smmu_type]->wp_base;
	int  i, snap = 0;

	/* Setting HW snapshot address and write data for PMU */
	smmu_write_field(wp_base, SMMUWP_PMU_SNAP_A(snap), PMU_SNAPSHOT_ADDR,
			 FIELD_PREP(PMU_SNAPSHOT_ADDR, SMMU_TCU_OFFSET +
			 SMMU_TCU_PMU_PAGE1 + SMMU_PMCG_CAPR));
	smmu_write_reg(wp_base, SMMUWP_PMU_SNAP_D(snap++),
		       SMMU_PMCG_CAPR_START);
	for (i = 0; i < SMMU_TBU_CNT(smmu_type); i++) {
		smmu_write_field(wp_base, SMMUWP_PMU_SNAP_A(snap),
				 PMU_SNAPSHOT_ADDR,
				 FIELD_PREP(PMU_SNAPSHOT_ADDR,
				 SMMU_TBUx_OFFSET(i) +
				 SMMU_TBU_PMU_PAGE1 + SMMU_PMCG_CAPR));
		smmu_write_reg(wp_base, SMMUWP_PMU_SNAP_D(snap++),
			       SMMU_PMCG_CAPR_START);
	}

	/* Setting HW snapshot address and write data for LMU */
	smmu_write_field(wp_base, SMMUWP_PMU_SNAP_A(snap), PMU_SNAPSHOT_ADDR,
			 FIELD_PREP(PMU_SNAPSHOT_ADDR, SMMU_WPCFG_OFFSET +
			 SMMUWP_LMU_CTL0));
	smmu_write_reg(wp_base, SMMUWP_PMU_SNAP_D(snap++), CTL0_LAT_MON_START);

	if (snap > SMMU_ELA_SNAPSHOT_MAX)
		pr_info("[%s] smmu_%u ela snapshot(%d) exceed max!\n",
			__func__, smmu_type, snap);
}

static void smmu_setup_hwpmu_monitor(u32 smmu_type)
{
	void __iomem *wp_base = ela_ctrl[smmu_type]->wp_base;
	int i, monitor = 0;

	/* Setting HW monitor address for TBU LMU */
	for (i = 0; i < SMMU_TBU_CNT(smmu_type); i++) {
		smmu_write_field(wp_base, SMMUWP_PMU_POLL_A(monitor++),
				 PMU_POLL_ADDR, FIELD_PREP(PMU_POLL_ADDR,
				 SMMU_WPCFG_OFFSET + SMMUWP_TBUx_MON4(i)));
		smmu_write_field(wp_base, SMMUWP_PMU_POLL_A(monitor++),
				 PMU_POLL_ADDR, FIELD_PREP(PMU_POLL_ADDR,
				 SMMU_WPCFG_OFFSET + SMMUWP_TBUx_MON11(i)));
		smmu_write_field(wp_base, SMMUWP_PMU_POLL_A(monitor++),
				 PMU_POLL_ADDR, FIELD_PREP(PMU_POLL_ADDR,
				 SMMU_WPCFG_OFFSET + SMMUWP_TBUx_MON12(i)));
		smmu_write_field(wp_base, SMMUWP_PMU_POLL_A(monitor++),
				 PMU_POLL_ADDR, FIELD_PREP(PMU_POLL_ADDR,
				 SMMU_WPCFG_OFFSET + SMMUWP_TBUx_MON13(i)));
	}

	/* Setting HW monitor address for TBU PMU */
	for (i = 0; i < SMMU_TBU_CNT(smmu_type); i++) {
		smmu_write_field(wp_base, SMMUWP_PMU_POLL_A(monitor++),
				 PMU_POLL_ADDR,
				 FIELD_PREP(PMU_POLL_ADDR,
				 SMMU_TBUx_OFFSET(i) +
				 SMMU_TBU_PMU_PAGE1 + SMMU_PMCG_EVCNTR(0, 4)));
	}

	/* Setting HW monitor address for TCU PMU */
	smmu_write_field(wp_base, SMMUWP_PMU_POLL_A(monitor++), PMU_POLL_ADDR,
			 FIELD_PREP(PMU_POLL_ADDR, SMMU_TCU_OFFSET +
			 SMMU_TCU_PMU_PAGE1 + SMMU_PMCG_EVCNTR(0, 4)));

	/* Setting HW monitor address for TCU LMU */
	smmu_write_field(wp_base, SMMUWP_PMU_POLL_A(monitor++), PMU_POLL_ADDR,
			 FIELD_PREP(PMU_POLL_ADDR, SMMU_WPCFG_OFFSET +
			 SMMUWP_TCU_MON1));
	smmu_write_field(wp_base, SMMUWP_PMU_POLL_A(monitor++), PMU_POLL_ADDR,
			 FIELD_PREP(PMU_POLL_ADDR, SMMU_WPCFG_OFFSET +
			 SMMUWP_TCU_MON8));

	if (monitor > SMMU_ELA_MONITOR_MAX)
		pr_info("[%s] smmu_%u ela monitor(%d) exceed max!\n",
			__func__, smmu_type, monitor);
}

static int smmu_pmu_hw_init(u32 smmu_type)
{
	struct smmu_pmu *pmu = &ela_ctrl[smmu_type]->pmu[0];
	bool global_filter;
	u32 i, cfgr;

	for (i = 0; i < SMMU_TXU_CNT(smmu_type); i++) {
		cfgr = smmu_read_reg(pmu[i].reg_base, SMMU_PMCG_CFGR);
		global_filter = !!(cfgr & SMMU_PMCG_CFGR_SID_FILTER_TYPE);
		if (!global_filter) {
			pr_info("pmu%u global_filter not supported!", i);
			return -EINVAL;
		}

		pmu[i].num_cntrs = FIELD_GET(SMMU_PMCG_CFGR_NCTR, cfgr) + 1;
		if (!(cfgr & SMMU_PMCG_CFGR_RELOC_CTRS))
			pmu[i].reloc_base = pmu[i].reg_base;

		pmu[i].supp_evts[0] = smmu_read_reg64(pmu[i].reg_base,
						      SMMU_PMCG_CEID0);
		pmu[i].supp_evts[1] = smmu_read_reg64(pmu[i].reg_base,
						      SMMU_PMCG_CEID1);
	}

	return 0;
}

static int mtk_smmu_ela_hw_init(u32 smmu_type)
{
	int ret = 0;

	ret = mtk_smmu_rpm_get(smmu_type);
	if (ret) {
		pr_info("%s, smmu_%u power_status:%d\n", __func__, smmu_type, ret);
		return ret;
	}

	ret = smmu_pmu_hw_init(smmu_type);
	if (ret) {
		mtk_smmu_rpm_put(smmu_type);
		return ret;
	}

	smmu_start_pmu(smmu_type);
	smmu_setup_hwpmu_snapshot(smmu_type);
	smmu_setup_hwpmu_monitor(smmu_type);
	smmu_enable_hwpmu(smmu_type);
	pr_info("%s, smmu_%u ela enable:%d\n", __func__, smmu_type,
		smmu_hwpmu_enabled(smmu_type));
	mtk_smmu_rpm_put(smmu_type);

	return 0;
}

static bool mm_hrt_debug_enabled(void)
{
	struct device_node *chosen_node;
	const char *name = NULL;
	int ret = 0;

	chosen_node = of_find_node_by_path(CHOSEN_NODE_PATH);
	if (chosen_node) {
		ret = of_property_read_string_index(chosen_node, HRT_DEBUG_EN_PROP_NAME, 0, &name);
		if (!ret && (!strncmp("on", name, sizeof("on"))))
			return true;
	}

	return false;
}

static int mtk_smmu_ela_data_init(u32 smmu_type)
{
	struct arm_smmu_device *smmu;
	struct mtk_smmu_data *data;
	struct smmu_ela_ctrl *ela;
	struct smmu_pmu *pmu;
	int i;

	if (!mm_hrt_debug_enabled())
		return -EINVAL;

	data = get_smmu_data(smmu_type);
	if (!data) {
		pr_info("%s, get smmu_%u data fail\n", __func__, smmu_type);
		return -EINVAL;
	}

	smmu = &data->smmu;
	ela = devm_kzalloc(smmu->dev, sizeof(struct smmu_ela_ctrl),
			   GFP_KERNEL);
	if (!ela)
		return -ENOMEM;

	ela->wp_base = smmu->wp_base;
	pmu = &ela->pmu[0];
	for (i = 0; i < SMMU_TXU_CNT(smmu_type); i++) {
		pmu[i].reg_base = data->pmu_reg[i];
		if (IS_ERR(pmu[i].reg_base))
			return -ENOMEM;

		pmu[i].reloc_base = data->pmu_reloc[i];
		if (IS_ERR(pmu[i].reloc_base))
			return -ENOMEM;
	}

	ela_ctrl[smmu_type] = ela;

	return 0;
}

int mtk_smmu_ela_init(u32 smmu_type)
{
	int ret = 0;

	if (smmu_type >= SMMU_TYPE_NUM)
		return -EINVAL;

	if (smmu_ela_inited(smmu_type))
		return 0;

	ret = mtk_smmu_ela_data_init(smmu_type);
	if (ret)
		return ret;

	ret = mtk_smmu_ela_hw_init(smmu_type);
	if (ret)
		return ret;

	ela_ctrl[smmu_type]->inited = true;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_smmu_ela_init);

int mtk_smmu_enable_ela(u32 smmu_type)
{
	int ret = 0;

	if (!smmu_ela_inited(smmu_type))
		return -EINVAL;

	ret = mtk_smmu_rpm_get(smmu_type);
	if (ret) {
		pr_info("%s, smmu_%u power_status:%d\n", __func__, smmu_type, ret);
		return ret;
	}

	smmu_start_pmu(smmu_type);
	smmu_enable_hwpmu(smmu_type);
	mtk_smmu_rpm_put(smmu_type);
	pr_info("smmu_%u enable ela\n", smmu_type);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_smmu_enable_ela);

int mtk_smmu_disable_ela(u32 smmu_type)
{
	int ret = 0;

	if (!smmu_ela_inited(smmu_type))
		return -EINVAL;

	ret = mtk_smmu_rpm_get(smmu_type);
	if (ret) {
		pr_info("%s, smmu_%u power_status:%d\n", __func__, smmu_type, ret);
		return ret;
	}

	smmu_stop_pmu(smmu_type);
	smmu_disable_hwpmu(smmu_type);
	mtk_smmu_rpm_put(smmu_type);
	pr_info("smmu_%u disable ela\n", smmu_type);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_smmu_disable_ela);

bool mtk_smmu_ela_enabled(u32 smmu_type)
{
	bool is_enabled = false;
	int ret = 0;

	if (!smmu_ela_inited(smmu_type))
		return false;

	ret = mtk_smmu_rpm_get(smmu_type);
	if (ret) {
		pr_info("%s, smmu_%u power_status:%d\n", __func__, smmu_type, ret);
		return false;
	}

	is_enabled = smmu_hwpmu_enabled(smmu_type);
	mtk_smmu_rpm_put(smmu_type);

	return is_enabled;
}
EXPORT_SYMBOL_GPL(mtk_smmu_ela_enabled);

void mtk_smmu_ela_dump(struct seq_file *s, u32 smmu_type)
{
	struct mtk_smmu_data *data;
	void __iomem *wp_base;
	struct smmu_pmu *pmu;
	int i, ret = 0;

	data = get_smmu_data(smmu_type);
	if (!data || data->hw_init_flag != 1 || !data->ela_support)
		return;

	ret = mtk_smmu_rpm_get(smmu_type);
	if (ret) {
		dump_ela(s, "smmu_%u ela dump, power_status:%d\n", smmu_type, ret);
		return;
	}

	if (!smmu_ela_inited(smmu_type))
		goto dump_hwpmu;

	dump_ela(s, "smmu_%u ela dump, EN:%u\n",
		 smmu_type, smmu_hwpmu_enabled(smmu_type));

	pmu = &ela_ctrl[smmu_type]->pmu[0];
	for (i = 0; i < SMMU_TXU_CNT(smmu_type); i++) {
		dump_ela(s,
			 "PMU%u: CFGR:0x%x, CR:0x%x, CNTEN0:0x%llx, EVTYPER0:0x%x, SMR0:0x%x\n",
			 i, smmu_read_reg(pmu[i].reg_base, SMMU_PMCG_CFGR),
			 smmu_read_reg(pmu[i].reg_base, SMMU_PMCG_CR),
			 smmu_read_reg64(pmu[i].reg_base, SMMU_PMCG_CNTENSET0),
			 smmu_read_reg(pmu[i].reg_base, SMMU_PMCG_EVTYPER(0)),
			 smmu_read_reg(pmu[i].reg_base, SMMU_PMCG_SMR(0)));
	}

dump_hwpmu:
	wp_base = data->smmu.wp_base;
	dump_ela(s, "SNAP_A: 0x%04x=0x%-8x 0x%04x=0x%-8x 0x%04x=0x%-8x 0x%04x=0x%x\n",
		 SMMUWP_PMU_SNAP_A(0), smmu_read_reg(wp_base, SMMUWP_PMU_SNAP_A(0)),
		 SMMUWP_PMU_SNAP_A(1), smmu_read_reg(wp_base, SMMUWP_PMU_SNAP_A(1)),
		 SMMUWP_PMU_SNAP_A(2), smmu_read_reg(wp_base, SMMUWP_PMU_SNAP_A(2)),
		 SMMUWP_PMU_SNAP_A(3), smmu_read_reg(wp_base, SMMUWP_PMU_SNAP_A(3)));
	dump_ela(s, "SNAP_A: 0x%04x=0x%-8x 0x%04x=0x%x\n",
		 SMMUWP_PMU_SNAP_A(4), smmu_read_reg(wp_base, SMMUWP_PMU_SNAP_A(4)),
		 SMMUWP_PMU_SNAP_A(5), smmu_read_reg(wp_base, SMMUWP_PMU_SNAP_A(5)));
	dump_ela(s, "SNAP_D: 0x%04x=0x%-8x 0x%04x=0x%-8x 0x%04x=0x%-8x 0x%04x=0x%x\n",
		 SMMUWP_PMU_SNAP_D(0), smmu_read_reg(wp_base, SMMUWP_PMU_SNAP_D(0)),
		 SMMUWP_PMU_SNAP_D(1), smmu_read_reg(wp_base, SMMUWP_PMU_SNAP_D(1)),
		 SMMUWP_PMU_SNAP_D(2), smmu_read_reg(wp_base, SMMUWP_PMU_SNAP_D(2)),
		 SMMUWP_PMU_SNAP_D(3), smmu_read_reg(wp_base, SMMUWP_PMU_SNAP_D(3)));
	dump_ela(s, "SNAP_D: 0x%04x=0x%-8x 0x%04x=0x%x\n",
		 SMMUWP_PMU_SNAP_D(4), smmu_read_reg(wp_base, SMMUWP_PMU_SNAP_D(4)),
		 SMMUWP_PMU_SNAP_D(5), smmu_read_reg(wp_base, SMMUWP_PMU_SNAP_D(5)));
	for (i = 0; i < SMMU_ELA_MONITOR_MAX; i += 4) {
		dump_ela(s, "POLL_A: 0x%04x=0x%-8x 0x%04x=0x%-8x 0x%04x=0x%-8x 0x%04x=0x%x\n",
			 SMMUWP_PMU_POLL_A(i),
			 smmu_read_reg(wp_base, SMMUWP_PMU_POLL_A(i)),
			 SMMUWP_PMU_POLL_A(i + 1),
			 smmu_read_reg(wp_base, SMMUWP_PMU_POLL_A(i + 1)),
			 SMMUWP_PMU_POLL_A(i + 2),
			 smmu_read_reg(wp_base, SMMUWP_PMU_POLL_A(i + 2)),
			 SMMUWP_PMU_POLL_A(i + 3),
			 smmu_read_reg(wp_base, SMMUWP_PMU_POLL_A(i + 3)));
	}

	mtk_smmu_rpm_put(smmu_type);
}
EXPORT_SYMBOL_GPL(mtk_smmu_ela_dump);

static int smmu_ela_evts_set(const char *val, const struct kernel_param *kp)
{
	u32 tcu_evt = 0, tbu_evt = 0;
	u32 smmu_type = MM_SMMU;
	struct smmu_pmu *pmu;
	char *input, *input_tmp, *token;
	int i;

	if (!smmu_ela_inited(smmu_type))
		return -EINVAL;

	pmu = &ela_ctrl[smmu_type]->pmu[0];
	input = kstrndup(val, SMMU_ELA_INPUT_MAX, GFP_KERNEL);
	if (!input)
		return -ENOMEM;

	input_tmp = input;
	token = strsep(&input_tmp, " \t\n");
	if (!token || kstrtou32(token, 10, &tcu_evt) ||
	    !smmu_pmu_check_event(&pmu[0], tcu_evt))
		goto err_free;

	token = strsep(&input_tmp, " \t\n");
	if (!token || kstrtou32(token, 10, &tbu_evt) ||
	    !smmu_pmu_check_event(&pmu[1], tbu_evt))
		goto err_free;

	kfree(input);
	pmu_cntr0_evt[0] = tcu_evt;
	for (i = 0; i < SMMU_TBU_CNT(smmu_type); i++)
		pmu_cntr0_evt[i + 1] = tbu_evt;

	pr_info("smmu_%u ela set pmu cntr0 tcu_evt:0x%x, tbu_evt:0x%x\n",
		smmu_type, tcu_evt, tbu_evt);

	return 0;

err_free:
	kfree(input);
	pr_info("%s input error, tcu_evt:0x%x, tbu_evt:0x%x\n",
		__func__, tcu_evt, tbu_evt);

	return -EINVAL;
}

static const struct kernel_param_ops smmu_ela_set_pmu_evts_ops = {
	.set = smmu_ela_evts_set,
};
module_param_cb(smmu_ela_evts, &smmu_ela_set_pmu_evts_ops, NULL, 0644);
MODULE_PARM_DESC(smmu_ela_evts, "smmu ela pmu events");

static int smmu_ela_mons_set(const char *val, const struct kernel_param *kp)
{
	u32 smmu_type = MM_SMMU;
	u32 ela_mons[SMMU_ELA_MONITOR_MAX] = { 0 };
	void __iomem *wp_base;
	char *input, *input_tmp, *token;
	int i, ret = 0;

	if (!smmu_ela_inited(smmu_type))
		return -EINVAL;

	input = kstrndup(val, SMMU_ELA_INPUT_MAX, GFP_KERNEL);
	if (!input)
		return -ENOMEM;

	input_tmp = input;
	for (i = 0; i < SMMU_ELA_MONITOR_MAX; i++) {
		token = strsep(&input_tmp, " \t\n");
		if (!token || kstrtou32(token, 16, &ela_mons[i]) ||
		    ela_mons[i] >= SMMU_WPCFG_OFFSET + SMMUWP_REG_SZ) {
			pr_info("%s input error, ela_mons[%d]:0x%x\n",
				__func__, i, ela_mons[i]);
			kfree(input);
			return -EINVAL;
		}
	}

	kfree(input);
	ret = mtk_smmu_rpm_get(smmu_type);
	if (ret) {
		pr_info("%s, smmu_%u power_status:%d\n", __func__, smmu_type, ret);
		return -EINVAL;
	}

	if (smmu_hwpmu_enabled(smmu_type)) {
		mtk_smmu_rpm_put(smmu_type);
		pr_info("%s, smmu_%u ela is on, not support set\n", __func__,
			smmu_type);
		return -EINVAL;
	}

	wp_base = ela_ctrl[smmu_type]->wp_base;
	for (i = 0; i < SMMU_ELA_MONITOR_MAX; i++)
		smmu_write_field(wp_base, SMMUWP_PMU_POLL_A(i), PMU_POLL_ADDR,
			 	 FIELD_PREP(PMU_POLL_ADDR, ela_mons[i]));
	mtk_smmu_rpm_put(smmu_type);
	pr_info("smmu_%u ela set monitor regs\n", smmu_type);

	return 0;
}

static const struct kernel_param_ops smmu_ela_set_mon_regs_ops = {
	.set = smmu_ela_mons_set,
};
module_param_cb(smmu_ela_mons, &smmu_ela_set_mon_regs_ops, NULL, 0644);
MODULE_PARM_DESC(smmu_ela_mons, "smmu ela monitor registers");

static int smmu_ela_enable_set(const char *val, const struct kernel_param *kp)
{
	u32 smmu_type = MM_SMMU;
	u32 enable = 0;
	int ret = 0;

	if (!smmu_ela_inited(smmu_type))
		return -EINVAL;

	ret = kstrtou32(val, 10, &enable);
	if (ret != 0) {
	        pr_info("%s input error, ret:%d, enable:%u\n", __func__,
			ret, enable);
		return -EINVAL;
	}

	ret = mtk_smmu_rpm_get(smmu_type);
	if (ret) {
		pr_info("%s, smmu_%u power_status:%d\n", __func__, smmu_type, ret);
		return -EINVAL;
	}

	if (enable)
		ret = mtk_smmu_enable_ela(smmu_type);
	else
		ret = mtk_smmu_disable_ela(smmu_type);

	mtk_smmu_rpm_put(smmu_type);

	return ret;
}

static const struct kernel_param_ops smmu_ela_set_enable_ops = {
	.set = smmu_ela_enable_set,
};
module_param_cb(smmu_ela_ena, &smmu_ela_set_enable_ops, NULL, 0644);
MODULE_PARM_DESC(smmu_ela_ena, "smmu ela enable");

static int smmu_ela_dump_get(char *buf, const struct kernel_param *kp)
{
	int i, written = 0, len = 0;
	u32 smmu_type = MM_SMMU;
	void __iomem *wp_base;
	struct smmu_pmu *pmu;
	int ret = 0;

	if (!smmu_ela_inited(smmu_type))
		return -EINVAL;

	ret = mtk_smmu_rpm_get(smmu_type);
	if (ret) {
		pr_info("%s, smmu_%u power_status:%d\n", __func__, smmu_type, ret);
		return -EINVAL;
	}

	mtk_smmu_ela_dump(NULL, smmu_type);

	pmu = &ela_ctrl[smmu_type]->pmu[0];
	written = snprintf(buf, SMMU_ELA_DUMP_MAX,
			   "ELA EN:%u, PMU0 EVTYPER0:0x%x, PMU1 EVTYPER0:0x%x\n",
			   smmu_hwpmu_enabled(smmu_type),
			   smmu_read_reg(pmu[0].reg_base, SMMU_PMCG_EVTYPER(0)),
			   smmu_read_reg(pmu[1].reg_base, SMMU_PMCG_EVTYPER(0)));

	if (written >= SMMU_ELA_DUMP_MAX || written < 0) {
		mtk_smmu_rpm_put(smmu_type);
		return 0;
	}

	wp_base = ela_ctrl[smmu_type]->wp_base;
	for (i = 0; i < SMMU_ELA_MONITOR_MAX; i += 4) {
		len = snprintf(buf + written, SMMU_ELA_DUMP_MAX - written,
			       "POLL_A: 0x%04x=0x%-8x 0x%04x=0x%-8x 0x%04x=0x%-8x 0x%04x=0x%x\n",
			       SMMUWP_PMU_POLL_A(i),
			       smmu_read_reg(wp_base, SMMUWP_PMU_POLL_A(i)),
			       SMMUWP_PMU_POLL_A(i + 1),
			       smmu_read_reg(wp_base, SMMUWP_PMU_POLL_A(i + 1)),
			       SMMUWP_PMU_POLL_A(i + 2),
			       smmu_read_reg(wp_base, SMMUWP_PMU_POLL_A(i + 2)),
			       SMMUWP_PMU_POLL_A(i + 3),
			       smmu_read_reg(wp_base, SMMUWP_PMU_POLL_A(i + 3)));

		if (len >= (SMMU_ELA_DUMP_MAX - written) || len < 0)
			break;

		written += len;
	}
	mtk_smmu_rpm_put(smmu_type);

	return written;
}

static const struct kernel_param_ops smmu_ela_dump_ops = {
	.get = smmu_ela_dump_get,
};
module_param_cb(smmu_ela_dump, &smmu_ela_dump_ops, NULL, 0644);
MODULE_PARM_DESC(smmu_ela_dump, "smmu ela dump");
#endif /* CONFIG_DEVICE_MODULES_ARM_SMMU_V3 */
