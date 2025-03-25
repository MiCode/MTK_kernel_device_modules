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

#include "cpufreq-dbg-lite.h"
#include "pf_ctrl.h"
#include "mtk_sip_svc.h"

#define CREATE_TRACE_POINTS
#include "pf_ctrl_trace.h"

#define DEFAULT_PF_MIN_FREQ		1000000
#define DEFAULT_PF_MAX_FREQ		1500000
#define DEFAULT_PF_INTERVAL		1000

#define PF_CTRL_DEBUG			0

u32 *g_pf_ctrl_enable;
u32 *g_pf_ctrl_max_freq;
u32 *g_pf_ctrl_min_freq;
u32 *g_pf_ctrl_interval;

static DEFINE_MUTEX(pf_ctrl_proc_mutex);
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

static int pf_ctrl_enable_proc_show(struct seq_file *m, void *v)
{
	int pf_ctrl_en = READ_ONCE(pf_ctrl_enable);
	int cpu;

	if (pf_ctrl_en == 0)
		seq_puts(m, "pf_ctrl is disabled[0]\n");
	else if (pf_ctrl_en == 1)
		seq_puts(m, "pf_ctrl is enabled[1]\n");
	else
		seq_printf(m, "pf_ctrl is Unknown mode [%d]\n", pf_ctrl_en);

	if (READ_ONCE(last_pf_dis))
		seq_printf(m, "pf is turned off, counter=%d\n", pf_counter);
	else
		seq_printf(m, "pf is turned on, counter=%d\n", pf_counter);

	seq_printf(m, "pf off total time (ns)=%llu\n", pf_ctrl_info.pf_off_total_time);

	for (cpu = 0; cpu < COREL_NUM; cpu++)
		seq_printf(m, "cpu=%d, set=%d, get=%d, ts=%llu\n",
			cpu, pf_ctrl_info.pf_set[cpu], pf_ctrl_info.pf_get[cpu],
			pf_ctrl_info.pf_ts[cpu]);

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

int __set_pf_ctrl_enable(bool enable)
{
	if (enable && pf_ctrl_setting_check())
		return -EINVAL;

	mutex_lock(&pf_ctrl_proc_mutex);
	WRITE_ONCE(pf_ctrl_enable, enable);

	if (!READ_ONCE(pf_ctrl_enable)) {
		pr_notice_ratelimited("%s pf_ctrl_enable=false, turn on pf\n", __func__);
		cancel_delayed_work_sync(&pf_top_work);
	}
	schedule_delayed_work(&pf_top_work, 0);
	mutex_unlock(&pf_ctrl_proc_mutex);

	return 0;
}

#if PF_CTRL_DEBUG
static ssize_t pf_ctrl_enable_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int enable = 0;
	int rc, ret;
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

	rc = kstrtoint(buf, 10, &enable);

	if (rc < 0)
		pr_info("Usage: echo (0:disable 1:enable)\n");
	else if (enable == READ_ONCE(pf_ctrl_enable))
		pr_info("Duplicated operation!\n");
	else if (enable == 0 || enable == 1) {
		ret = __set_pf_ctrl_enable(enable);
		if (ret) {
			free_page((unsigned long)buf);
			return ret;
		}
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

bool mtk_get_pf_ctrl_enable(void)
{
	return READ_ONCE(pf_ctrl_enable);
}
EXPORT_SYMBOL_GPL(mtk_get_pf_ctrl_enable);

int mtk_set_pf_ctrl_enable(bool enable)
{
	if (!pf_ctrl_wq)
		return -EINVAL;

	if (enable == READ_ONCE(pf_ctrl_enable)) {
		pr_notice_ratelimited("Duplicated operation!\n");
		return 0;
	}

	return __set_pf_ctrl_enable(enable);
}
EXPORT_SYMBOL_GPL(mtk_set_pf_ctrl_enable);

int mtk_pf_ctrl_init(void)
{
	int cpu;

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
	}

	create_pf_ctrl_fs();

	return 0;
}

void mtk_pf_ctrl_exit(void)
{
	if (pf_ctrl_wq)
		destroy_workqueue(pf_ctrl_wq);
}
