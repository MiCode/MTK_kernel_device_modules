// SPDX-License-Identifier: GPL-2.0
/*
 * mtk_freq_qos.c - Freq QoS debug Driver
 *
 * Copyright (c) 2023 MediaTek Inc.
 * Chung-kai Yang <Chung-kai.Yang@mediatek.com>
 */

/* system includes */
#include <linux/cpufreq.h>
#include <linux/hashtable.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/jiffies.h>
#include <linux/version.h>
#include "cpufreq-dbg-lite.h"
#include "mtk_freq_qos.h"

#define DEBUG 0
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 13, 0))
#define FREQ_QOS_VH_EXISTS 1
#else
#define FREQ_QOS_VH_EXISTS 0
#endif

#if FREQ_QOS_VH_EXISTS
#include <trace/hooks/power.h>
#endif

static DEFINE_SPINLOCK(mtk_freq_qos_buf_lock);
static DECLARE_HASHTABLE(mtk_freq_qos_ht, FREQ_QOS_HT_SHIFT_BIT);
static struct mtk_freq_qos_req *mtk_freq_req;
static struct control_mapping *control_group_master;
struct cpufreq_policy *ptr_cpufreq_policy[MAX_NR_POLICY];
static struct mtk_freq_qos_circ_buf circ_buf;
static char *record_type_str[3] = {"ADD", "UPDATE", "REMOVE"};

static void mtk_freq_qos_write_buf(struct mtk_freq_qos_data *freq_req, int type)
{
	struct mtk_freq_qos_record *record;
	unsigned long flags;

	spin_lock_irqsave(&mtk_freq_qos_buf_lock, flags);
	record = &circ_buf.buf[circ_buf.head];
	circ_buf.head = (circ_buf.head + 1) & (FREQ_QOS_CIRC_BUF_SIZE - 1);
	if (circ_buf.tail == circ_buf.head)
		circ_buf.tail = (circ_buf.tail + 1) & (FREQ_QOS_CIRC_BUF_SIZE - 1);
	spin_unlock_irqrestore(&mtk_freq_qos_buf_lock, flags);

	strscpy(record->caller_info, freq_req->caller_info, sizeof(record->caller_info));
	record->min_value = freq_req->min_value;
	record->max_value = freq_req->max_value;
	if (type == FREQ_QOS_REMOVE)
		record->ts = jiffies;
	else
		record->ts = freq_req->last_update;
	record->cpu = freq_req->cpu;
	record->type = type;
}

static struct mtk_freq_qos_data *find_and_update_req_data(
	struct freq_qos_request *req, int value)
{
	struct mtk_freq_qos_data *cur_req = NULL, *freq_req = NULL;
#if DEBUG
	u32 cur_key = (u32)(unsigned long)req;
#endif

	hash_for_each_possible(mtk_freq_qos_ht, cur_req, req_node, (unsigned long)req) {
		if (cur_req->req == req) {
			if (req->type == FREQ_QOS_MIN)
				cur_req->min_value = value;
			else if (req->type == FREQ_QOS_MAX)
				cur_req->max_value = value;
			freq_req = cur_req;
			break;
		}
	}
#if DEBUG
	pr_info("========================Before========================\n");
	dump_list();
	pr_info("========================After========================\n");
#endif

	if (freq_req == NULL) {
#if DEBUG
		pr_info("%s: %lu, %lu First time\n", __func__, (unsigned long)cur_key, (unsigned long)req);
#endif
		freq_req = kmalloc(sizeof(struct mtk_freq_qos_data), GFP_ATOMIC);
		if (!freq_req) {
			pr_info("%s: Failed to allocate freq_req\n", __func__);
			return NULL;
		}

		freq_req->cpu = PM_QOS_DEFAULT_VALUE;
		freq_req->req = req;
#if DEBUG
		pr_info("%s: Start.....\n", __func__);
#endif

		if (req->type == FREQ_QOS_MIN) {
			freq_req->min_value = value;
			freq_req->max_value = PM_QOS_DEFAULT_VALUE;
		} else if (req->type == FREQ_QOS_MAX) {
			freq_req->min_value = PM_QOS_DEFAULT_VALUE;
			freq_req->max_value = value;
		}
#if DEBUG
		pr_info("%s: Then.....\n", __func__);
#endif
		sprint_symbol(freq_req->caller_info, (unsigned long)__builtin_return_address(3));
#if DEBUG
		pr_info("%s: Add.....\n", __func__);
#endif
		hash_add(mtk_freq_qos_ht, &freq_req->req_node, (unsigned long)req);
	}

#if DEBUG
	dump_list();
#endif
	return freq_req;
}

static void find_and_remove_req_data(struct freq_qos_request *req)
{
	struct mtk_freq_qos_data *cur_req = NULL, *freq_req = NULL;
	int cpu, policy_idx;
	struct cpufreq_policy *policy;

	hash_for_each_possible(mtk_freq_qos_ht, cur_req, req_node, (unsigned long)req) {
		if (cur_req->req == req) {
			freq_req = cur_req;
			break;
		}
	}

	if (freq_req) {
		hash_del(&freq_req->req_node);
		for_each_possible_cpu(cpu) {
			policy = cpufreq_cpu_get(cpu);
			if (!policy)
				continue;

			policy_idx = control_group_master[policy->cpu].policy_idx;
			if (policy_idx == -1) {
				cpufreq_cpu_put(policy);
				continue;
			}

			if (mtk_freq_req[policy_idx].last_min_req == freq_req)
				mtk_freq_req[policy_idx].last_min_req = NULL;
			if (mtk_freq_req[policy_idx].min_dominant == freq_req)
				mtk_freq_req[policy_idx].min_dominant = NULL;
			if (mtk_freq_req[policy_idx].last_max_req == freq_req)
				mtk_freq_req[policy_idx].last_max_req = NULL;
			if (mtk_freq_req[policy_idx].max_dominant == freq_req)
				mtk_freq_req[policy_idx].max_dominant = NULL;

			cpu = cpumask_last(policy->related_cpus);
			cpufreq_cpu_put(policy);
		}
		mtk_freq_qos_write_buf(freq_req, FREQ_QOS_REMOVE);
		kfree(freq_req);
		pr_info("%s: freq_req %lu removed\n", __func__, (unsigned long)req);
	} else
		pr_info("%s: freq_req %lu not found\n", __func__, (unsigned long)req);
}

static struct mtk_freq_qos_data *get_req_data(struct freq_qos_request *req)
{
	struct mtk_freq_qos_data *cur_req = NULL, *freq_req = NULL;

	hash_for_each_possible(mtk_freq_qos_ht, cur_req, req_node, (unsigned long)req) {
		if (cur_req->req == req) {
			freq_req = cur_req;
			break;
		}
	}

	if (!freq_req) {
		pr_info("%s: *************** Not Found ***************\n", __func__);
		pr_info("%s: val: %d\n", __func__, req->pnode.prio);
	}

	return freq_req;
}

void mtk_freq_qos_add_request(void *data, struct freq_constraints *qos,
	struct freq_qos_request *req, enum freq_qos_req_type type,
	int value, int ret)
{
	struct mtk_freq_qos_data *freq_req;
	struct cpufreq_policy *policy = NULL;
	int i, policy_idx;

	for (i = 0; i < MAX_NR_POLICY; i++) {
		if (ptr_cpufreq_policy[i] && &ptr_cpufreq_policy[i]->constraints == qos)
			policy = ptr_cpufreq_policy[i];
	}
	if (!policy) {
		pr_info("%s: policy not found: %lu\n", __func__, (unsigned long)req);
		return;
	}

	freq_req = find_and_update_req_data(req, value);
	if (!freq_req) {
		pr_info("%s: req not found: %lu\n", __func__, (unsigned long)req);
		return;
	}

	freq_req->cpu = policy->cpu;
	policy_idx = control_group_master[policy->cpu].policy_idx;

	if (policy_idx == -1) {
		pr_info("%s: Cpu %d not initialized\n", __func__, policy->cpu);
		return;
	}

	if (type == FREQ_QOS_MIN)
		mtk_freq_req[policy_idx].last_min_req = freq_req;
	else if (type == FREQ_QOS_MAX)
		mtk_freq_req[policy_idx].last_max_req = freq_req;

	freq_req->last_update = jiffies;
	mtk_freq_qos_write_buf(freq_req, FREQ_QOS_ADD);
#if DEBUG
	pr_info("%s: %lu: %s: cpu: %d, t: %lu, Add done\n",
		__func__, (unsigned long)freq_req->req, freq_req->caller_info,
		policy->cpu, freq_req->last_update);
#endif
}

void mtk_freq_qos_update_request(void *data, struct freq_qos_request *req, int value)
{
	struct mtk_freq_qos_data *freq_req;
	struct cpufreq_policy *policy = NULL;
	int i, policy_idx = 0;

	for (i = 0; i < MAX_NR_POLICY; i++) {
		if (ptr_cpufreq_policy[i] && &ptr_cpufreq_policy[i]->constraints == req->qos)
			policy = ptr_cpufreq_policy[i];
	}
	if (!policy) {
		pr_info("%s: policy not found: %lu\n", __func__, (unsigned long)req);
		return;
	}

	freq_req = find_and_update_req_data(req, value);
	if (!freq_req) {
		pr_info("%s: req not found: %lu\n", __func__, (unsigned long)req);
		return;
	}

	if (freq_req->cpu == PM_QOS_DEFAULT_VALUE)
		freq_req->cpu = policy->cpu;

	policy_idx = control_group_master[policy->cpu].policy_idx;

	if (policy_idx == -1) {
		pr_info("%s: Cpu %d not initialized\n", __func__, policy->cpu);
		return;
	}

	if (req->type == FREQ_QOS_MIN)
		mtk_freq_req[policy_idx].last_min_req = freq_req;
	else if (req->type == FREQ_QOS_MAX)
		mtk_freq_req[policy_idx].last_max_req = freq_req;

	freq_req->last_update = jiffies;
	mtk_freq_qos_write_buf(freq_req, FREQ_QOS_UPDATE);
#if DEBUG
	if (req->type == FREQ_QOS_MIN)
		pr_info("%s: %d: %lu: key: %lu, freq_req: %lu, min_key: %lu",
			__func__,
			policy_idx,
			jiffies,
			(unsigned long)req,
			(unsigned long)freq_req->req,
			(unsigned long)mtk_freq_req[policy_idx].last_min_req->req);
	else if (req->type == FREQ_QOS_MAX)
		pr_info("%s: %d: %lu: key: %lu, freq_req: %lu, max_key: %lu",
			__func__,
			policy_idx,
			jiffies,
			(unsigned long)req,
			(unsigned long)freq_req->req,
			(unsigned long)mtk_freq_req[policy_idx].last_max_req->req);
#endif
}

void mtk_freq_qos_remove_request(void *data, struct freq_qos_request *req)
{
	find_and_remove_req_data(req);
}

static int mtk_freq_qos_notifier_min(struct notifier_block *nb,
					unsigned long freq, void *ptr)
{
	struct mtk_freq_qos_req cur_req;
	int policy_idx;

	cur_req = *(container_of(nb, struct mtk_freq_qos_req, nb_min));
	if (!cur_req.last_min_req) {
#if DEBUG
		pr_info("%s: No last req\n", __func__);
#endif
		return 0;
	}

	policy_idx = cur_req.policy_idx;
	mtk_freq_req[policy_idx].min_dominant =
		mtk_freq_req[policy_idx].last_min_req;
	mtk_freq_req[policy_idx].min_value = freq;

	return 0;
}

static int mtk_freq_qos_notifier_max(struct notifier_block *nb,
					unsigned long freq, void *ptr)
{
	struct mtk_freq_qos_req cur_req;
	int policy_idx;

	cur_req = *(container_of(nb, struct mtk_freq_qos_req, nb_max));
	if (!cur_req.last_max_req) {
#if DEBUG
		pr_info("%s: No last req\n", __func__);
#endif
		return 0;
	}

	policy_idx = cur_req.policy_idx;
	mtk_freq_req[policy_idx].max_dominant =
		mtk_freq_req[policy_idx].last_max_req;
	mtk_freq_req[policy_idx].max_value = freq;

	return 0;
}

static int get_nr_policy(void) {
	int i, nr_cpus = 0, count = 0, cpu, master = 0;
	struct cpufreq_policy *policy, *last_policy = NULL;

	nr_cpus = num_possible_cpus();
	for (i = 0; i < nr_cpus; i++) {
		control_group_master[i].master = -1;
		control_group_master[i].policy_idx = -1;
	}

	for_each_possible_cpu(cpu) {
		if (cpu >= MAX_NR_POLICY) // for coverity
			break;

		policy = cpufreq_cpu_get(cpu);
		if (!policy) {
			pr_info("No cpufreq policy for cpu %d\n", cpu);
			continue;
		}

		ptr_cpufreq_policy[cpu] = policy;

		if (policy == last_policy) {
			control_group_master[cpu].master = master;
			control_group_master[cpu].policy_idx = count - 1;
			cpufreq_cpu_put(policy);
			continue;
		}

		master = cpu;
		control_group_master[cpu].master = master;
		control_group_master[cpu].policy_idx = count;
		last_policy = policy;
		cpufreq_cpu_put(policy);
		count++;
	}

	return count;
}

static bool check_mapping_invalid(int cpu) {
	return control_group_master[cpu].master == -1 ||
		control_group_master[cpu].policy_idx == -1;
}

static bool check_master(int cpu) {
	return control_group_master[cpu].master == cpu;
}

static int mtk_freq_limit_notifier_register(void) {
	struct cpufreq_policy *policy;
	int cpu, policy_idx = 0, ret = 0, nr_policy = 0;

	nr_policy = get_nr_policy();
#if DEBUG
	pr_info("%s: nr_policy: %d\n", __func__, nr_policy);
#endif
	mtk_freq_req = kcalloc(nr_policy, sizeof(struct mtk_freq_qos_req), GFP_KERNEL);
	if (!mtk_freq_req) {
		pr_info("%s: Failed to allocate memory for mtk_freq_req.\n", __func__);
		return -ENOMEM;
	}

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy || !check_master(cpu) || check_mapping_invalid(cpu))
			continue;

		policy_idx = control_group_master[cpu].policy_idx;
#if DEBUG
		pr_info("%s: cpu%d: policy_idx: %d\n", __func__, cpu, policy_idx);
#endif
		mtk_freq_req[policy_idx].nb_min.notifier_call
			= mtk_freq_qos_notifier_min;
		mtk_freq_req[policy_idx].nb_max.notifier_call
			= mtk_freq_qos_notifier_max;
		mtk_freq_req[policy_idx].policy_idx = policy_idx;

		ret = freq_qos_add_notifier(&policy->constraints, FREQ_QOS_MIN,
				&mtk_freq_req[policy_idx].nb_min);
		if (ret) {
			pr_info("%s: Failed to register MIN QoS notifier: %d (%*pbl, %d)\n",
				__func__, ret, cpumask_pr_args(policy->cpus), policy_idx);
			goto err_free_obj;
		}

		ret = freq_qos_add_notifier(&policy->constraints, FREQ_QOS_MAX,
				&mtk_freq_req[policy_idx].nb_max);
		if (ret) {
			pr_info("%s: Failed to register MAX QoS notifier: %d (%*pbl, %d)\n",
				__func__, ret, cpumask_pr_args(policy->cpus), policy_idx);
			goto err_min_qos_notifier;
		}

		cpufreq_cpu_put(policy);
		cpu = cpumask_last(policy->related_cpus);
	}

	return ret;

err_min_qos_notifier:
	freq_qos_remove_notifier(&policy->constraints, FREQ_QOS_MIN,
					&mtk_freq_req[policy_idx].nb_min);
err_free_obj:
	cpufreq_cpu_put(policy);
	kfree(mtk_freq_req);

	return ret;
}

#if DEBUG
void dump_list(void) {
	struct mtk_freq_qos_data *cur_req;
	int key;

	hash_for_each(mtk_freq_qos_ht, key, cur_req, req_node) {
		pr_info("k: %lu, min_v: %7d, max_v: %7d, cpu: %d, c2: %s\n",
			(unsigned long)cur_req->req, cur_req->min_value, cur_req->max_value,
			cur_req->cpu, cur_req->caller_info);
	}
}
#endif

/*
 * show max freq QoS requests
 */
static int freq_req_proc_show(struct seq_file *m, void *v)
{
	struct mtk_freq_qos_data *cur_req;
	struct freq_qos_request *cur_freq_req;
	struct pm_qos_constraints *c;
	struct plist_node *cur_node;
	int cpu, policy_idx;
	struct cpufreq_policy *policy;
	unsigned long cur_time = jiffies;

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;

		policy_idx = control_group_master[policy->cpu].policy_idx;
		seq_printf(m, "Policy: %d, CPU: %d\n", policy_idx, policy->cpu);

		if (policy_idx == -1) {
			cpufreq_cpu_put(policy);
			continue;
		}

		if (mtk_freq_req[policy_idx].last_min_req)
			seq_printf(m, "%20s %10p, Min Value: %10d\n",
				"Last Request:",
				(void *)mtk_freq_req[policy_idx].last_min_req->req,
				mtk_freq_req[policy_idx].last_min_req->min_value);

		if (mtk_freq_req[policy_idx].min_dominant)
			seq_printf(m, "%20s %10p, Min Value: %10d\n",
				"Dominant Request:",
				(void *)mtk_freq_req[policy_idx].min_dominant->req,
				mtk_freq_req[policy_idx].min_dominant->min_value);

		seq_printf(m, "%10s, %10s, %10s, %21s, %70s\n",
				"Request", "Min Value", "Prio Value", "Duration Time(ms)", "Caller");
		c = &policy->constraints.min_freq;
		plist_for_each(cur_node, &c->list) {
			cur_freq_req = container_of(cur_node, struct freq_qos_request, pnode);
			cur_req = get_req_data(cur_freq_req);
			if (cur_req)
				seq_printf(m, "%10p, %10d, %10d, %21d, %70s\n",
					(void *)cur_freq_req,
					cur_req->min_value,
					cur_node->prio,
					jiffies_to_msecs(cur_time - cur_req->last_update),
					cur_req->caller_info);
			else
				seq_printf(m, "%10p, %10d, %10d, %21s, %70s\n",
					(void *)cur_freq_req,
					cur_node->prio, cur_node->prio,
					"N/A", "N/A");
		}

		if (mtk_freq_req[policy_idx].last_max_req)
			seq_printf(m, "%20s %10p, Max Value: %10d\n",
				"Last Request:",
				(void *)mtk_freq_req[policy_idx].last_max_req->req,
				mtk_freq_req[policy_idx].last_max_req->max_value);

		if (mtk_freq_req[policy_idx].max_dominant)
			seq_printf(m, "%20s %10p, Max Value: %10d\n",
				"Dominant Request:",
				(void *)mtk_freq_req[policy_idx].max_dominant->req,
				mtk_freq_req[policy_idx].max_dominant->max_value);

		seq_printf(m, "%10s, %10s, %10s, %21s, %70s\n",
				"Request", "Max Value", "Prio Value", "Duration Time(ms)", "Caller");
		c = &policy->constraints.max_freq;
		plist_for_each(cur_node, &c->list) {
			cur_freq_req = container_of(cur_node, struct freq_qos_request, pnode);
			cur_req = get_req_data(cur_freq_req);
			if (cur_req)
				seq_printf(m, "%10p, %10d, %10d, %21d, %70s\n",
					(void *)cur_freq_req,
					cur_req->max_value,
					cur_node->prio,
					jiffies_to_msecs(cur_time - cur_req->last_update),
					cur_req->caller_info);
			else
				seq_printf(m, "%10p, %10d, %10d, %21s, %70s\n",
					(void *)cur_freq_req,
					cur_node->prio,
					cur_node->prio,
					"N/A",
					"N/A");
		}

		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
	}

	return 0;
}
PROC_FOPS_RO(freq_req);

/*
 * show min freq QoS requests
 */
static int min_freq_req_proc_show(struct seq_file *m, void *v)
{
	struct mtk_freq_qos_data *cur_req;
	struct freq_qos_request *cur_freq_req;
	struct pm_qos_constraints *c;
	struct plist_node *cur_node;
	int cpu, policy_idx;
	struct cpufreq_policy *policy;
	unsigned long cur_time = jiffies;

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;

		policy_idx = control_group_master[policy->cpu].policy_idx;
		seq_printf(m, "Policy: %d, CPU: %d\n", policy_idx, policy->cpu);

		if (policy_idx == -1) {
			cpufreq_cpu_put(policy);
			continue;
		}

		if (mtk_freq_req[policy_idx].last_min_req)
			seq_printf(m, "%20s %10p, Min Value: %10d\n",
				"Last Request:",
				(void *)mtk_freq_req[policy_idx].last_min_req->req,
				mtk_freq_req[policy_idx].last_min_req->min_value);

		if (mtk_freq_req[policy_idx].min_dominant)
			seq_printf(m, "%20s %10p, Min Value: %10d\n",
				"Dominant Request:",
				(void *)mtk_freq_req[policy_idx].min_dominant->req,
				mtk_freq_req[policy_idx].min_dominant->min_value);

		seq_printf(m, "%10s, %10s, %10s, %21s, %70s\n",
				"Request", "Min Value", "Prio Value", "Duration Time(ms)", "Caller");
		c = &policy->constraints.min_freq;
		plist_for_each(cur_node, &c->list) {
			cur_freq_req = container_of(cur_node, struct freq_qos_request, pnode);
			cur_req = get_req_data(cur_freq_req);
			if (cur_req)
				seq_printf(m, "%10p, %10d, %10d, %21d, %70s\n",
					(void *)cur_freq_req,
					cur_req->min_value,
					cur_node->prio,
					jiffies_to_msecs(cur_time - cur_req->last_update),
					cur_req->caller_info);
			else
				seq_printf(m, "%10p, %10d, %10d, %21s, %70s\n",
					(void *)cur_freq_req,
					cur_node->prio, cur_node->prio,
					"N/A", "N/A");
		}

		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
	}

	return 0;
}
PROC_FOPS_RO(min_freq_req);

/*
 * show max freq QoS requests
 */
static int max_freq_req_proc_show(struct seq_file *m, void *v)
{
	struct mtk_freq_qos_data *cur_req;
	struct freq_qos_request *cur_freq_req;
	struct pm_qos_constraints *c;
	struct plist_node *cur_node;
	int cpu, policy_idx;
	struct cpufreq_policy *policy;
	unsigned long cur_time = jiffies;

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;

		policy_idx = control_group_master[policy->cpu].policy_idx;
		seq_printf(m, "Policy: %d, CPU: %d\n", policy_idx, policy->cpu);

		if (policy_idx == -1) {
			cpufreq_cpu_put(policy);
			continue;
		}

		if (mtk_freq_req[policy_idx].last_max_req)
			seq_printf(m, "%20s %10p, Max Value: %10d\n",
				"Last Request:",
				(void *)mtk_freq_req[policy_idx].last_max_req->req,
				mtk_freq_req[policy_idx].last_max_req->max_value);

		if (mtk_freq_req[policy_idx].max_dominant)
			seq_printf(m, "%20s %10p, Max Value: %10d\n",
				"Dominant Request:",
				(void *)mtk_freq_req[policy_idx].max_dominant->req,
				mtk_freq_req[policy_idx].max_dominant->max_value);

		seq_printf(m, "%10s, %10s, %10s, %21s, %70s\n",
				"Request", "Max Value", "Prio Value", "Duration Time(ms)", "Caller");
		c = &policy->constraints.max_freq;
		plist_for_each(cur_node, &c->list) {
			cur_freq_req = container_of(cur_node, struct freq_qos_request, pnode);
			cur_req = get_req_data(cur_freq_req);
			if (cur_req)
				seq_printf(m, "%10p, %10d, %10d, %21d, %70s\n",
					(void *)cur_freq_req,
					cur_req->max_value,
					cur_node->prio,
					jiffies_to_msecs(cur_time - cur_req->last_update),
					cur_req->caller_info);
			else
				seq_printf(m, "%10p, %10d, %10d, %21s, %70s\n",
					(void *)cur_freq_req,
					cur_node->prio,
					cur_node->prio,
					"N/A",
					"N/A");
		}

		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
	}

	return 0;
}
PROC_FOPS_RO(max_freq_req);

/*
 * show min dominant requests
 */
static int min_dominant_proc_show(struct seq_file *m, void *v)
{
	struct mtk_freq_qos_data *cur_req;
	int cpu, policy_idx;
	struct cpufreq_policy *policy;
	unsigned long cur_time = jiffies;

	seq_printf(m, "%10s, %10s, %10s, %21s, %70s\n",
			"Policy", "Request", "Min Value", "Duration Time(ms)", "Caller");

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;

		policy_idx = control_group_master[policy->cpu].policy_idx;

		if (policy_idx == -1) {
			cpufreq_cpu_put(policy);
			continue;
		}

		if (mtk_freq_req[policy_idx].min_dominant) {
			cur_req = mtk_freq_req[policy_idx].min_dominant;
			seq_printf(m, "%10d, %10p, %10d, %21d, %70s\n",
				control_group_master[cpu].master,
				(void *)cur_req->req,
				cur_req->min_value,
				jiffies_to_msecs(cur_time - cur_req->last_update),
				cur_req->caller_info);
		} else
			seq_printf(m, "%10d, %10s, %10s, %21s, %70s\n",
				control_group_master[cpu].master,
				"N/A", "N/A", "N/A", "N/A");

		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
	}

	return 0;
}
PROC_FOPS_RO(min_dominant);

/*
 * show max dominant requests
 */
static int max_dominant_proc_show(struct seq_file *m, void *v)
{
	struct mtk_freq_qos_data *cur_req;
	int cpu, policy_idx;
	struct cpufreq_policy *policy;
	unsigned long cur_time = jiffies;

	seq_printf(m, "%10s, %10s, %10s, %21s, %70s\n",
			"Policy", "Request", "Max Value", "Duration Time(ms)", "Caller");

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;

		policy_idx = control_group_master[policy->cpu].policy_idx;

		if (policy_idx == -1) {
			cpufreq_cpu_put(policy);
			continue;
		}

		if (mtk_freq_req[policy_idx].max_dominant) {
			cur_req = mtk_freq_req[policy_idx].max_dominant;
			seq_printf(m, "%10d, %10p, %10d, %21d, %70s\n",
				control_group_master[cpu].master,
				(void *)cur_req->req,
				cur_req->max_value,
				jiffies_to_msecs(cur_time - cur_req->last_update),
				cur_req->caller_info);
		} else
			seq_printf(m, "%10d, %10s, %10s, %21s, %70s\n",
				control_group_master[cpu].master,
				"N/A", "N/A", "N/A", "N/A");

		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
	}

	return 0;
}
PROC_FOPS_RO(max_dominant);

/*
 * Dump freq QoS requests from hash table
 */
static int freq_req_tbl_proc_show(struct seq_file *m, void *v)
{
	struct mtk_freq_qos_data *cur_req;
	int key;
	unsigned long cur_time = jiffies;

	seq_printf(m, "%10s, %10s, %10s, %3s, %21s, %70s\n",
				"Request", "Min Value", "Max Value", "CPU", "Last Time(ms)", "Caller");
	hash_for_each(mtk_freq_qos_ht, key, cur_req, req_node) {
		seq_printf(m, "%10p, %10d, %10d, %3d, %21d, %70s\n",
				(void *)cur_req->req,
				cur_req->min_value, cur_req->max_value,
				cur_req->cpu, jiffies_to_msecs(cur_time - cur_req->last_update),
				cur_req->caller_info);
	}

	return 0;
}
PROC_FOPS_RO(freq_req_tbl);

/*
 * Dump freq QoS request record from circular buffer
 */
static void print_record(struct seq_file *m, int i, unsigned long cur_time)
{
	char *record_type;

	if (circ_buf.buf[i].type < 3)
		record_type = record_type_str[circ_buf.buf[i].type];
	else
		record_type = "UNKNOWN";

	seq_printf(m, "%7s, %10d, %10d, %3d, %21d, %70s\n",
			record_type,
			circ_buf.buf[i].min_value,
			circ_buf.buf[i].max_value,
			circ_buf.buf[i].cpu,
			jiffies_to_msecs(cur_time - circ_buf.buf[i].ts),
			circ_buf.buf[i].caller_info);
}
static int freq_req_buf_proc_show(struct seq_file *m, void *v)
{
	int i;
	unsigned long cur_time = jiffies;

	seq_printf(m, "%7s, %10s, %10s, %3s, %21s, %70s\n",
				"Type", "Min Value", "Max Value", "CPU", "Last Time(ms)", "Caller");
	if (circ_buf.tail == circ_buf.head) {
		// buffer empty
		return 0;
	} else if (circ_buf.tail < circ_buf.head) {
		// buffer not full
		for (i = circ_buf.tail; i < circ_buf.head; i++)
			print_record(m, i, cur_time);
	} else {
		// buffer full
		for (i = circ_buf.tail; i < FREQ_QOS_CIRC_BUF_SIZE; i++)
			print_record(m, i, cur_time);
		for (i = 0; i < circ_buf.head; i++)
			print_record(m, i, cur_time);
	}

	return 0;
}
PROC_FOPS_RO(freq_req_buf);

static void create_debug_fs(void)
{
	int i;
	struct proc_dir_entry *freq_dir = NULL;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
		void *data;
	};

	struct pentry freq_entries[] = {
		PROC_ENTRY(min_freq_req),
		PROC_ENTRY(max_freq_req),
		PROC_ENTRY(min_dominant),
		PROC_ENTRY(max_dominant),
		PROC_ENTRY(freq_req),
		PROC_ENTRY(freq_req_tbl),
		PROC_ENTRY(freq_req_buf),
	};

	freq_dir = proc_mkdir("mtk_freq_qos", NULL);
	for (i = 0; i < ARRAY_SIZE(freq_entries); i++) {
		if (!proc_create(freq_entries[i].name, 0664,
					freq_dir, freq_entries[i].fops)) {
			pr_info("[%s]: create /proc/mtk_freq_qos/%s failed\n",
					__func__,
					freq_entries[i].name);
		}
	}
}

int mtk_freq_qos_init(void)
{
	int nr_cpus = 0, ret = 0;

	nr_cpus = num_possible_cpus();
	pr_info("%s: nr_cpus: %d\n", __func__, nr_cpus);
	control_group_master = kcalloc(nr_cpus, sizeof(struct control_mapping), GFP_KERNEL);
	if (!control_group_master) {
		pr_info("%s: Failed to allocate control_group_master\n", __func__);
		return -ENOMEM;
	}

	circ_buf.buf = kcalloc(FREQ_QOS_CIRC_BUF_SIZE, sizeof(struct mtk_freq_qos_record), GFP_KERNEL);
	if (!circ_buf.buf) {
		pr_info("%s: Failed to allocate circ_buf.buf\n", __func__);
		ret = -ENOMEM;
		goto err_free_mem_master;
	}
	circ_buf.head = 0;
	circ_buf.tail = 0;

	ret = mtk_freq_limit_notifier_register();
	if (ret) {
		pr_info("%s: failed to register notifier\n", __func__);
		goto err_free_mem_buf;
	}

#if FREQ_QOS_VH_EXISTS
	ret = register_trace_android_vh_freq_qos_add_request(mtk_freq_qos_add_request, NULL);
	if (ret) {
		pr_info("register android_vh_freq_qos_add_request failed\n");
		ret = -EINVAL;
		goto err_free_mem_buf;
	}

	ret = register_trace_android_vh_freq_qos_update_request(mtk_freq_qos_update_request, NULL);
	if (ret) {
		pr_info("register android_vh_freq_qos_update_request failed\n");
		ret = -EINVAL;
		goto err_free_mem_buf;
	}

	ret = register_trace_android_vh_freq_qos_remove_request(mtk_freq_qos_remove_request, NULL);
	if (ret) {
		pr_info("register android_vh_freq_qos_remove_request failed\n");
		ret = -EINVAL;
		goto err_free_mem_buf;
	}
#endif

	create_debug_fs();
	pr_info("mtk_freq_qos init\n");

	return 0;

err_free_mem_buf:
	kfree(circ_buf.buf);
err_free_mem_master:
	kfree(control_group_master);

	return ret;
}

MODULE_DESCRIPTION("MTK Freq QoS Debug Driver v0.1.1");
MODULE_AUTHOR("Chung-Kai Yang <Chung-kai.Yang@mediatek.com>");
MODULE_LICENSE("GPL v2");
