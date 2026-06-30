// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/cpufreq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/sched/clock.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/smp.h>
#include <linux/arm-smccc.h>
#include <linux/perf_event.h>
#include <linux/kallsyms.h>

#include "cpufreq-dbg-lite.h"
#include "pf_ctrl.h"
#include "mtk_sip_svc.h"

#define CREATE_TRACE_POINTS
#include "pf_ctrl_trace.h"

#define DEFAULT_PF_MIN_FREQ		1000000
#define DEFAULT_PF_MAX_FREQ		1500000
#define DEFAULT_PF_INTERVAL		1000

#define PF_CTRL_DEBUG			0

static u32 *g_pf_ctrl_enable;
static u32 *g_pf_ctrl_max_freq;
static u32 *g_pf_ctrl_min_freq;
static u32 *g_pf_ctrl_interval;
static u32 *g_pf_ctrl_buf;
static u32 *g_pf_ctrl_ipc_buf;

static DEFINE_MUTEX(pf_ctrl_proc_mutex);
static int pf_ctrl_version;
static int pf_ctrl_enable;
static int pf_ctrl_min_freq = DEFAULT_PF_MIN_FREQ;
static int pf_ctrl_max_freq = DEFAULT_PF_MAX_FREQ;
static int pf_ctrl_interval = DEFAULT_PF_INTERVAL;
static int pf_counter;
static bool last_pf_dis;
static struct pf_work_struct pf_switch_work[COREL_NUM];
static struct delayed_work pf_top_work;
static struct pf_info pf_ctrl_info;
static struct cpufreq_policy *ptr_policy_0;
static struct workqueue_struct *pf_ctrl_wq;
static struct pf_buf pf_circ_buf;
static struct pf_ipc_buf ipc_circ_buf[COREL_NUM];

static DEFINE_PER_CPU(struct perf_event *, cycle_events);
static DEFINE_PER_CPU(struct perf_event *, inst_events);
static DEFINE_PER_CPU(unsigned long long,  cycle_count);
static DEFINE_PER_CPU(unsigned long long,  inst_count);
static struct perf_event_attr cycle_event_attr = {
	.type           = PERF_TYPE_HARDWARE,
	.config         = PERF_COUNT_HW_CPU_CYCLES,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr inst_event_attr = {
	.type           = PERF_TYPE_HARDWARE,
	.config         = PERF_COUNT_HW_INSTRUCTIONS,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};

static void pf_write_buf(u64 ts, u64 pf_off_total_time, int count, bool pf_dis)
{
	struct pf_record *record;

	if (!pf_circ_buf.buf)
		return;

	record = &pf_circ_buf.buf[pf_circ_buf.head];
	pf_circ_buf.head = (pf_circ_buf.head + 1) & (PF_CIRC_BUF_SIZE - 1);
	if (pf_circ_buf.tail == pf_circ_buf.head)
		pf_circ_buf.tail = (pf_circ_buf.tail + 1) & (PF_CIRC_BUF_SIZE - 1);

	record->ts = (unsigned int)(ts / MSEC_PER_NSEC);
	record->pf_off_total_time = (unsigned int)(pf_off_total_time / MSEC_PER_NSEC);
	record->count = pf_counter;
	record->pf_dis = pf_dis;
}

static int pf_perf_event_read_local(struct perf_event *ev, u64 *value)
{
	return perf_event_read_local(ev, value, NULL, NULL);
}

static void pf_ipc_write_buf(unsigned int cpu, unsigned long long inst,
			unsigned long long cycle, bool pf_dis, u64 ts)
{
	struct pf_ipc_record *record;

	if (cpu >= COREL_NUM || !ipc_circ_buf[cpu].buf)
		return;

	record = &ipc_circ_buf[cpu].buf[ipc_circ_buf[cpu].head];
	ipc_circ_buf[cpu].head = (ipc_circ_buf[cpu].head + 1) & (PF_IPC_CIRC_BUF_SIZE - 1);
	if (ipc_circ_buf[cpu].tail == ipc_circ_buf[cpu].head)
		ipc_circ_buf[cpu].tail = (ipc_circ_buf[cpu].tail + 1) & (PF_IPC_CIRC_BUF_SIZE - 1);

	record->inst = inst;
	record->cycle = cycle;
	if (cycle / 100 != 0)
		record->ipc = (unsigned int)(inst / (cycle / 100));
	else
		record->ipc = 0;
	record->pf_dis = pf_dis;
	record->ts = (unsigned int)(ts / MSEC_PER_NSEC);
}

static void pf_ipc_pmu_overflow_handler(struct perf_event *event,
			struct perf_sample_data *data, struct pt_regs *regs)
{
	unsigned long long count = local64_read(&event->count);

	pr_info("pf_ctrl: ignoring spurious overflow on cpu %u, config=%llu, count=%llu\n",
	       event->cpu,
	       event->attr.config,
	       count);
}

static int pf_ipc_pmu_probe_cpu(int cpu)
{
	struct perf_event *event;
	struct perf_event *i_event = per_cpu(inst_events, cpu);
	struct perf_event *c_event = per_cpu(cycle_events, cpu);

	if (!i_event) {
		event = perf_event_create_kernel_counter(
			&inst_event_attr,
			cpu,
			NULL,
			pf_ipc_pmu_overflow_handler,
			NULL);

		if (IS_ERR(event))
			goto error;

		per_cpu(inst_events, cpu) = event;
		i_event = event;
	}

	if (!c_event) {
		event = perf_event_create_kernel_counter(
			&cycle_event_attr,
			cpu,
			NULL,
			pf_ipc_pmu_overflow_handler,
			NULL);

		if (IS_ERR(event))
			goto error;

		per_cpu(cycle_events, cpu) = event;
		c_event = event;
	}

	if (i_event) {
		perf_event_enable(i_event);
		pf_perf_event_read_local(i_event,
					&per_cpu(inst_count, cpu));
	}

	if (c_event) {
		perf_event_enable(c_event);
		pf_perf_event_read_local(c_event,
					&per_cpu(cycle_count, cpu));
	}

	return 0;

error:
	pr_info("%s: probe cpu %d fail\n", __func__, cpu);

	return -1;
}

static void pf_ipc_pmu_remove_cpu(int cpu)
{
	struct perf_event *i_event = per_cpu(inst_events, cpu);
	struct perf_event *c_event = per_cpu(cycle_events, cpu);

	if (i_event) {
		perf_event_disable(i_event);
		per_cpu(inst_events, cpu) = NULL;
		perf_event_release_kernel(i_event);
	}

	if (c_event) {
		perf_event_disable(c_event);
		per_cpu(cycle_events, cpu) = NULL;
		perf_event_release_kernel(c_event);
	}
}

static unsigned long long pf_ipc_get_inst_count(int cpu)
{
	struct perf_event *event = per_cpu(inst_events, cpu);
	unsigned long long new = 0;
	unsigned long long old = per_cpu(inst_count, cpu);
	unsigned long long diff = 0;

	if (event && event->state == PERF_EVENT_STATE_ACTIVE) {
		pf_perf_event_read_local(event, &new);
		if (new > old)
			diff = new - old;

		per_cpu(inst_count, cpu) = new;
	}

#if PF_CTRL_DEBUG
	pr_info("%s: CPU%d -> new=%llu, old=%llu, diff=%llu\n",
		__func__, cpu, new, old, diff);
#endif

	return diff;
}

static unsigned long long pf_ipc_get_cycle_count(int cpu)
{
	struct perf_event *event = per_cpu(cycle_events, cpu);
	unsigned long long new = 0;
	unsigned long long old = per_cpu(cycle_count, cpu);
	unsigned long long diff = 0;

	if (event && event->state == PERF_EVENT_STATE_ACTIVE) {
		pf_perf_event_read_local(event, &new);
		if (new > old)
			diff = new - old;

		per_cpu(cycle_count, cpu) = new;
	}

#if PF_CTRL_DEBUG
	pr_info("%s: CPU%d -> new=%llu, old=%llu, diff=%llu\n",
		__func__, cpu, new, old, diff);
#endif

	return diff;
}

static int pf_ctrl_enable_proc_show(struct seq_file *m, void *v)
{
	int pf_ctrl_en = READ_ONCE(pf_ctrl_enable);
	int cpu;

	if (pf_ctrl_en == 0)
		seq_puts(m, "pf_ctrl is disabled[0]\n");
	else
		seq_printf(m, "pf_ctrl is enabled[%d]\n", pf_ctrl_en);

	if (READ_ONCE(last_pf_dis))
		seq_printf(m, "pf is turned off, counter=%d\n", pf_counter);
	else
		seq_printf(m, "pf is turned on, counter=%d\n", pf_counter);

	seq_printf(m, "pf off total time (ms)=%llu\n",
		pf_ctrl_info.pf_off_total_time / MSEC_PER_NSEC);

	for (cpu = 0; cpu < COREL_NUM; cpu++)
		seq_printf(m, "cpu=%d, set=%d, get=%d, ts=%llu\n",
			cpu, pf_ctrl_info.pf_set[cpu], pf_ctrl_info.pf_get[cpu],
			pf_ctrl_info.pf_ts[cpu] / MSEC_PER_NSEC);

	return 0;
}

static int pf_ctrl_setting_check(void)
{
	if (pf_ctrl_max_freq == 0 || pf_ctrl_min_freq == 0
					|| pf_ctrl_min_freq > pf_ctrl_max_freq) {
		pr_info("Invalid min/max\n");
		return -EINVAL;
	}
	if (pf_ctrl_interval == 0) {
		pr_info("Invalid interval\n");
		return -EINVAL;
	}
	return 0;
}

static inline bool __get_pf_ctrl_enable_by_user(unsigned int user)
{
	return (READ_ONCE(pf_ctrl_enable) & (1 << user));
}

static int __set_pf_ctrl_enable(bool enable, unsigned int user)
{
	int pf_ctrl_enable_old, pf_ctrl_enable_new;

	if (enable && pf_ctrl_setting_check())
		return -EINVAL;

	mutex_lock(&pf_ctrl_proc_mutex);

	pf_ctrl_enable_old = READ_ONCE(pf_ctrl_enable);
	if (enable)
		pf_ctrl_enable_new = (pf_ctrl_enable_old | (1 << user));
	else
		pf_ctrl_enable_new = (pf_ctrl_enable_old & ~(1 << user));

	WRITE_ONCE(pf_ctrl_enable, pf_ctrl_enable_new);

	if (pf_ctrl_enable_old && !pf_ctrl_enable_new)
		cancel_delayed_work_sync(&pf_top_work);

	if ((pf_ctrl_enable_old && !pf_ctrl_enable_new) ||
	    (!pf_ctrl_enable_old && pf_ctrl_enable_new))
		schedule_delayed_work(&pf_top_work, 0);

	mutex_unlock(&pf_ctrl_proc_mutex);

	return 0;
}

#if PF_CTRL_DEBUG
static ssize_t pf_ctrl_enable_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int enable, ret;
	unsigned int user;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count > 255) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	if (copy_from_user(buf, buffer, count)) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	if (sscanf(buf, "%d %u", &enable, &user) != 2) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	if (enable < 0 || enable > 1 || user >= PF_CTRL_USER_NUM) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	if (enable == __get_pf_ctrl_enable_by_user(user)) {
		pr_notice_ratelimited("Duplicated operation!\n");
		free_page((unsigned long)buf);
		return count;
	}

	ret = __set_pf_ctrl_enable(enable, user);
	if (ret) {
		free_page((unsigned long)buf);
		return ret;
	}

	free_page((unsigned long)buf);
	return count;
}
PROC_FOPS_RW(pf_ctrl_enable);
#else
PROC_FOPS_RO(pf_ctrl_enable);
#endif


static int pf_ctrl_max_freq_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "pf_ctrl Max Freq: %d (kHz)\n", pf_ctrl_max_freq);
	return 0;
}

static int pf_ctrl_min_freq_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "pf_ctrl Min Freq: %d (kHz)\n", pf_ctrl_min_freq);
	return 0;
}

static int pf_ctrl_interval_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "pf_ctrl Interval: %d (ms)\n", pf_ctrl_interval);
	return 0;
}

#if PF_CTRL_DEBUG
static ssize_t pf_ctrl_max_freq_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int freq = 0;
	int rc;
	char *buf = (char *) __get_free_page(GFP_USER);

	if(READ_ONCE(pf_ctrl_enable)) {
		pr_info("Modifications can only be made if pf_ctrl_enable is disabled\n");
		return -EPERM;
	}

	if (!buf)
		return -ENOMEM;

	if (count > 255) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	if (copy_from_user(buf, buffer, count)) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	rc = kstrtoint(buf, 10, &freq);

	if (rc < 0)
		pr_info("Usage: echo max freq (kHz)\n");
	else
		pf_ctrl_max_freq = freq;

	free_page((unsigned long)buf);
	return count;
}
PROC_FOPS_RW(pf_ctrl_max_freq);

static ssize_t pf_ctrl_min_freq_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int freq = 0;
	int rc;
	char *buf = (char *) __get_free_page(GFP_USER);

	if(READ_ONCE(pf_ctrl_enable)) {
		pr_info("Modifications can only be made if pf_ctrl_enable is disabled\n");
		return -EPERM;
	}

	if (!buf)
		return -ENOMEM;

	if (count > 255) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	if (copy_from_user(buf, buffer, count)) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	rc = kstrtoint(buf, 10, &freq);

	if (rc < 0)
		pr_info("Usage: echo min freq (kHz)\n");
	else
		pf_ctrl_min_freq = freq;

	free_page((unsigned long)buf);
	return count;
}
PROC_FOPS_RW(pf_ctrl_min_freq);

static ssize_t pf_ctrl_interval_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int interval = 0;
	int rc;
	char *buf = (char *) __get_free_page(GFP_USER);

	if(READ_ONCE(pf_ctrl_enable)) {
		pr_info("Modifications can only be made if pf_ctrl_enable is disabled\n");
		return -EPERM;
	}

	if (!buf)
		return -ENOMEM;

	if (count > 255) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	if (copy_from_user(buf, buffer, count)) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	rc = kstrtoint(buf, 10, &interval);

	if (rc < 0)
		pr_info("Usage: echo interval (ms)\n");
	else if (interval < PF_MIN_INTERVAL)
		pr_info("Usage: min interval is %d (ms)\n", PF_MIN_INTERVAL);
	else if (interval > PF_MAX_INTERVAL)
		pr_info("Usage: max interval is %d (ms)\n", PF_MAX_INTERVAL);
	else
		pf_ctrl_interval = interval;

	free_page((unsigned long)buf);
	return count;
}
PROC_FOPS_RW(pf_ctrl_interval);
#else
PROC_FOPS_RO(pf_ctrl_max_freq);
PROC_FOPS_RO(pf_ctrl_min_freq);
PROC_FOPS_RO(pf_ctrl_interval);
#endif

static void print_record(struct seq_file *m, int i)
{
	seq_printf(m, "%5u, %6d, %10u, %10u\n",
			pf_circ_buf.buf[i].count,
			pf_circ_buf.buf[i].pf_dis,
			pf_circ_buf.buf[i].ts,
			pf_circ_buf.buf[i].pf_off_total_time);
}

static int pf_ctrl_buf_proc_show(struct seq_file *m, void *v)
{
	int i;

	seq_printf(m, "%5s, %6s, %10s, %10s\n",
			"count", "pf_dis", "ts", "total_time");

	if (pf_circ_buf.tail == pf_circ_buf.head) {
		// buffer empty
		return 0;
	} else if (pf_circ_buf.tail < pf_circ_buf.head) {
		// buffer not full
		for (i = pf_circ_buf.tail; i < pf_circ_buf.head; i++)
			print_record(m, i);
	} else {
		// buffer full
		for (i = pf_circ_buf.tail; i < PF_CIRC_BUF_SIZE; i++)
			print_record(m, i);
		for (i = 0; i < pf_circ_buf.head; i++)
			print_record(m, i);
	}

	return 0;
}
PROC_FOPS_RO(pf_ctrl_buf);

static void print_ipc_record(struct seq_file *m, int cpu, int i)
{
	if (cpu < 0 || cpu >= COREL_NUM)
		return;

	seq_printf(m, "%3d, %13llu, %13llu, %7u, %6d, %10u\n",
			cpu,
			ipc_circ_buf[cpu].buf[i].inst,
			ipc_circ_buf[cpu].buf[i].cycle,
			ipc_circ_buf[cpu].buf[i].ipc,
			ipc_circ_buf[cpu].buf[i].pf_dis,
			ipc_circ_buf[cpu].buf[i].ts);
}

static int pf_ctrl_ipc_buf_proc_show(struct seq_file *m, void *v)
{
	int cpu, i;

	seq_printf(m, "%3s, %13s, %13s, %7s, %6s, %10s\n",
			"CPU", "inst", "cycle", "IPC*100", "pf_dis", "ts");

	for (cpu = 0; cpu < COREL_NUM; cpu++) {
		if (ipc_circ_buf[cpu].tail == ipc_circ_buf[cpu].head) {
			// buffer empty
			return 0;
		} else if (ipc_circ_buf[cpu].tail < ipc_circ_buf[cpu].head) {
			// buffer not full
			for (i = ipc_circ_buf[cpu].tail; i < ipc_circ_buf[cpu].head; i++)
				print_ipc_record(m, cpu, i);
		} else {
			// buffer full
			for (i = ipc_circ_buf[cpu].tail; i < PF_IPC_CIRC_BUF_SIZE; i++)
				print_ipc_record(m, cpu, i);
			for (i = 0; i < ipc_circ_buf[cpu].head; i++)
				print_ipc_record(m, cpu, i);
		}
	}

	return 0;
}
PROC_FOPS_RO(pf_ctrl_ipc_buf);

static int create_pf_ctrl_fs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
		void *data;
	};

	const struct pentry entries[] = {
		PROC_ENTRY_DATA(pf_ctrl_enable),
		PROC_ENTRY_DATA(pf_ctrl_max_freq),
		PROC_ENTRY_DATA(pf_ctrl_min_freq),
		PROC_ENTRY_DATA(pf_ctrl_interval),
		PROC_ENTRY_DATA(pf_ctrl_buf),
		PROC_ENTRY_DATA(pf_ctrl_ipc_buf),
	};

	/* create /proc/pf_ctrl */
	dir = proc_mkdir("pf_ctrl", NULL);
	if (!dir) {
		pr_info("fail to create /proc/pf_ctrl @ %s()\n",
								__func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create_data
			(entries[i].name, 0664,
			dir, entries[i].fops, NULL))
			pr_info("%s(), create /proc/pf_ctrl/%s failed\n",
						__func__, entries[i].name);
	}

	return 0;
}

static void pf_switch_work_function(struct work_struct *work)
{
	struct arm_smccc_res res;
	unsigned int cpu = smp_processor_id();
	struct pf_work_struct *pf_work = container_of(work, struct pf_work_struct, work);
	unsigned int smc_act = pf_work->pf_type == PF_DISABLE ?
			MT_PREFETCH_SMC_ACT_SET : MT_PREFETCH_SMC_ACT_CLR;
	u64 off_ts = 0;

	off_ts = pf_ctrl_info.pf_ts[cpu];
	pf_ctrl_info.pf_ts[cpu] = sched_clock();
	if (cpu == 0 && off_ts != 0 && pf_work->pf_type == PF_ENABLE)
		pf_ctrl_info.pf_off_total_time += pf_ctrl_info.pf_ts[cpu] - off_ts;

	pf_ctrl_info.pf_set[cpu] = pf_work->pf_type == PF_DISABLE ? 1 : 0;
	arm_smccc_smc(MTK_SIP_CACHE_CONTROL,
			smc_act | MT_PREFETCH_SMC_MAGIC,
			0, 0, 0, 0, 0, 0, &res);
	if (res.a0)
		pr_info("%s: MTK_SIP_CACHE_CONTROL fail: %lu\n", __func__, res.a0);

	arm_smccc_smc(MTK_SIP_CACHE_CONTROL,
			MT_PREFETCH_SMC_ACT_GET | MT_PREFETCH_SMC_MAGIC,
			0, 0, 0, 0, 0, 0, &res);
	pf_ctrl_info.pf_get[cpu] = (bool)(res.a0 & CPUECTLR_EL1_PREFETCH_MASK);

	if (cpu == 0) {
		pf_write_buf(
			pf_ctrl_info.pf_ts[cpu],
			pf_ctrl_info.pf_off_total_time,
			pf_counter,
			pf_work->pf_type == PF_DISABLE ? 1 : 0
		);
	}

	pf_ipc_write_buf(cpu,
		pf_ipc_get_inst_count(cpu),
		pf_ipc_get_cycle_count(cpu),
		pf_work->pf_type == PF_DISABLE ? 0 : 1,
		pf_ctrl_info.pf_ts[cpu]);
}

static void trigger_pf_work(int type)
{
	static cpumask_t flush_cpus;
	int cpu;

	cpus_read_lock();
	cpumask_clear(&flush_cpus);
	for_each_online_cpu(cpu) {
		if (cpu >= COREL_NUM)
			continue;
		if (type == PF_DISABLE || type == PF_ENABLE){
			pf_switch_work[cpu].pf_type = type;
			queue_work_on(cpu, pf_ctrl_wq, &pf_switch_work[cpu].work);
			cpumask_set_cpu(cpu, &flush_cpus);
		}
	}

	for_each_cpu(cpu, &flush_cpus)
		flush_work(&pf_switch_work[cpu].work);

	cpus_read_unlock();

	trace_trigger_pf_work(type, pf_ctrl_info.pf_off_total_time);
}

static void pf_main_work(struct work_struct *work)
{
	bool pf_dis_flag = READ_ONCE(last_pf_dis);
	unsigned int cur_freq;

	if (!ptr_policy_0) {
		pr_info("%s: policy0 not exists\n", __func__);
		return;
	}

	cur_freq = ptr_policy_0->cur;
	if (cur_freq < pf_ctrl_min_freq && !pf_dis_flag)
		pf_dis_flag = true;
	else if (cur_freq > pf_ctrl_max_freq && pf_dis_flag)
		pf_dis_flag = false;

	if (!READ_ONCE(pf_ctrl_enable)) {
		if (READ_ONCE(last_pf_dis)) {
			trigger_pf_work(PF_ENABLE);
			WRITE_ONCE(last_pf_dis, false);
			pf_counter++;
		}
		return;
	}

	if (READ_ONCE(last_pf_dis) != pf_dis_flag) {
		if (pf_dis_flag)
			trigger_pf_work(PF_DISABLE);
		else
			trigger_pf_work(PF_ENABLE);

		WRITE_ONCE(last_pf_dis, pf_dis_flag);
		pf_counter++;
	}

	if (READ_ONCE(pf_ctrl_enable))
		schedule_delayed_work(&pf_top_work, msecs_to_jiffies(pf_ctrl_interval));
}

int mtk_get_pf_ctrl_enable(void)
{
	if (!pf_ctrl_version || !pf_ctrl_wq)
		return -EINVAL;

	return READ_ONCE(pf_ctrl_enable);
}
EXPORT_SYMBOL_GPL(mtk_get_pf_ctrl_enable);

int mtk_set_pf_ctrl_enable(bool enable, unsigned int user)
{
	char caller_info[KSYM_SYMBOL_LEN];

	if (!pf_ctrl_version || !pf_ctrl_wq)
		return -EINVAL;

	if (user >= PF_CTRL_USER_NUM)
		return -EINVAL;

	sprint_symbol(caller_info, (unsigned long)__builtin_return_address(0));
	trace_set_pf_ctrl_enable(READ_ONCE(pf_ctrl_enable), enable, user, caller_info);

	if (enable == __get_pf_ctrl_enable_by_user(user)) {
		pr_notice_ratelimited("Duplicated operation!\n");
		return 0;
	}

	return __set_pf_ctrl_enable(enable, user);
}
EXPORT_SYMBOL_GPL(mtk_set_pf_ctrl_enable);

int mtk_pf_ctrl_init(void)
{
	int cpu, ret;
	struct device_node *pf_ctrl_node;

	pf_ctrl_node = of_find_node_by_name(NULL, "pf-ctrl");
	if (pf_ctrl_node == NULL) {
		pr_info("%s: pf_ctrl not supports\n", __func__);
		return -ENODEV;
	}
	ret = of_property_read_u32(pf_ctrl_node, "version", &pf_ctrl_version);
	if (ret) {
		pr_info("%s: failed to get version\n", __func__);
		return -EINVAL;
	}
	pr_info("%s: pf_ctrl version: %u\n", __func__, pf_ctrl_version);

	pf_ctrl_wq = alloc_workqueue("pf_ctrl_wq", __WQ_LEGACY, 1);
	if (!pf_ctrl_wq) {
		pr_info("%s: failed to allocate workqueue!\n", __func__);
		return -EINVAL;
	}
	ptr_policy_0 = cpufreq_cpu_get(0);
	INIT_DELAYED_WORK(&pf_top_work, pf_main_work);
	for (cpu = 0; cpu < COREL_NUM; cpu++) {
		pf_switch_work[cpu].cpu = cpu;
		pf_switch_work[cpu].pf_type = PF_ENABLE;
		INIT_WORK(&pf_switch_work[cpu].work, pf_switch_work_function);

		ret = pf_ipc_pmu_probe_cpu(cpu);
		if (ret)
			pf_ipc_pmu_remove_cpu(cpu);

		ipc_circ_buf[cpu].buf = kcalloc(PF_IPC_CIRC_BUF_SIZE,
				sizeof(struct pf_ipc_record), GFP_KERNEL);
		if (!ipc_circ_buf[cpu].buf)
			pr_info("%s: Failed to allocate ipc_circ_buf[%d].buf\n", __func__, cpu);
		ipc_circ_buf[cpu].head = 0;
		ipc_circ_buf[cpu].tail = 0;
	}
	pf_circ_buf.buf = kcalloc(PF_CIRC_BUF_SIZE,
			sizeof(struct pf_record), GFP_KERNEL);
	pf_circ_buf.head = 0;
	pf_circ_buf.tail = 0;

	create_pf_ctrl_fs();

	return 0;
}

void mtk_pf_ctrl_exit(void)
{
	int cpu;

	if (pf_ctrl_wq)
		destroy_workqueue(pf_ctrl_wq);

	for (cpu = 0; cpu < COREL_NUM; cpu++) {
		pf_ipc_pmu_remove_cpu(cpu);
		kfree(pf_circ_buf.buf);
		kfree(ipc_circ_buf[cpu].buf);
	}
}
