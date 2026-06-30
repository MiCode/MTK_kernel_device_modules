// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/atomic.h>
#include <linux/proc_fs.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/tracepoint.h>

#include <apu_tags.h>
#include <apu_tp.h>
#include <hds_plat_1_0_tag.h>
#define CREATE_TRACE_POINTS
#include <hds_plat_1_0_events.h>

#define HDS_1_0_TAGS_NUM (3000)

static struct apu_tags *hds_1_0_tags;

enum HDS_1_0_TAG_TYPE{
	HDS_1_0_TAG_TYPE_PMU,
};

struct hds_1_0_tag {
	uint64_t type;

	union hds_tag_data {
		struct hds_tag_pmu {
			uint64_t inf_id;
			int64_t sc_idx;
			uint64_t inst_idx;
			uint64_t inst_type;
			uint64_t dev_idx;
			uint64_t dev_container;
			uint64_t ts_enque;
			uint64_t ts_dep_solv;
			uint64_t ts_disp;
			uint64_t ts_irq_recv;
		} pmu;
	} data;
};

void hds_1_0_pmu_trace(uint64_t inf_id, int64_t sc_idx,
	uint64_t inst_idx, uint64_t inst_type,
	uint64_t dev_idx, uint64_t dev_container, uint64_t ts_enque,
	uint64_t ts_dep_solv, uint64_t ts_disp, uint64_t ts_irq_recv)
{
	trace_hds_1_0_pmu(inf_id, sc_idx, inst_idx, inst_type,
		dev_idx, dev_container,
		ts_enque, ts_dep_solv, ts_disp, ts_irq_recv);
}

/* The parameters must aligned with trace_mdw_rv_cmd() */
static void
probe_hds_1_0_pmu(void *data,
	uint64_t inf_id, int64_t sc_idx, uint64_t inst_idx, uint64_t inst_type,
	uint64_t dev_idx, uint64_t dev_container,
	uint64_t ts_enque, uint64_t ts_dep_solv, uint64_t ts_disp, uint64_t ts_irq_recv)
{
	struct hds_1_0_tag t;

	if (!hds_1_0_tags)
		return;

	t.type = HDS_1_0_TAG_TYPE_PMU;
	t.data.pmu.inf_id = inf_id;
	t.data.pmu.sc_idx = sc_idx;
	t.data.pmu.inst_idx = inst_idx;
	t.data.pmu.inst_type = inst_type;
	t.data.pmu.dev_idx = dev_idx;
	t.data.pmu.dev_container = dev_container;
	t.data.pmu.ts_enque = ts_enque;
	t.data.pmu.ts_dep_solv = ts_dep_solv;
	t.data.pmu.ts_disp = ts_disp;
	t.data.pmu.ts_irq_recv = ts_irq_recv;

	apu_tag_add(hds_1_0_tags, &t);
}

static int hds_1_0_tag_seq(struct seq_file *s, void *tag, void *priv)
{
	struct hds_1_0_tag *t = (struct hds_1_0_tag *)tag;

	if (!t)
		return -ENOENT;

	seq_printf(s, "inf_id=0x%llx,", t->data.pmu.inf_id);
	seq_printf(s, "sc_idx=%lld,", t->data.pmu.sc_idx);
	seq_printf(s, "inst_idx=%llu,", t->data.pmu.inst_idx);
	seq_printf(s, "inst_type=0x%llx,", t->data.pmu.inst_type);
	seq_printf(s, "dev_idx=0x%llx,", t->data.pmu.dev_idx);
	seq_printf(s, "dev_container=0x%llx,", t->data.pmu.dev_container);
	seq_printf(s, "ts_enque=0x%llx,", t->data.pmu.ts_enque);
	seq_printf(s, "ts_dep_solv=0x%llx,", t->data.pmu.ts_dep_solv);
	seq_printf(s, "ts_disp=0x%llx,", t->data.pmu.ts_disp);
	seq_printf(s, "ts_irq_recv=0x%llx\n", t->data.pmu.ts_irq_recv);

	return 0;
}

static int hds_1_0_tag_seq_info(struct seq_file *s, void *tag, void *priv)
{
	return 0;
}

static struct apu_tp_tbl hds_1_0_tag_tp_tbl[] = {
	{.name = "hds_1_0_pmu", .func = probe_hds_1_0_pmu},
	APU_TP_TBL_END
};

int hds_1_0_tag_init(void)
{
	int ret;

	hds_1_0_tags = apu_tags_alloc("hds", "hds_block", sizeof(struct hds_1_0_tag),
		HDS_1_0_TAGS_NUM, hds_1_0_tag_seq, hds_1_0_tag_seq_info, NULL);

	if (!hds_1_0_tags)
		return -ENOMEM;

	ret = apu_tp_init(hds_1_0_tag_tp_tbl);
	if (ret)
		pr_info("%s: unable to register\n", __func__);

	return ret;
}

void hds_1_0_tag_deinit(void)
{
	apu_tp_exit(hds_1_0_tag_tp_tbl);
	apu_tags_free(hds_1_0_tags);
}
