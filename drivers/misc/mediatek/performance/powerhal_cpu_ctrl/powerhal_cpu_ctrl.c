// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/preempt.h>
#include <linux/trace_events.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include <linux/sort.h>
#include <linux/mutex.h>

#ifndef CREATE_TRACE_POINTS
#define CREATE_TRACE_POINTS
#endif
#include "powerhal_trace_event.h"
#include "powerhal_cpu_ctrl.h"
//#include "mtk_perfmgr_internal.h"
/* PROCFS */
#define PROC_FOPS_RW(name) \
static const struct proc_ops perfmgr_ ## name ## _proc_fops = { \
	.proc_read	= perfmgr_ ## name ## _proc_show, \
	.proc_write	= perfmgr_ ## name ## _proc_write,\
	.proc_open	= perfmgr_proc_open, \
}

#define PROC_FOPS_RO(name) \
static const struct proc_ops perfmgr_ ## name ## _proc_fops = { \
	.proc_read	= perfmgr_ ## name ## _proc_show, \
	.proc_open	= perfmgr_proc_open, \
}

#define PROC_ENTRY(name) {__stringify(name), &perfmgr_ ## name ## _proc_fops}

#define show_debug(fmt, x...) \
	do { \
		if (debug_enable) \
			pr_debug(fmt, ##x); \
	} while (0)

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define CLUSTER_MAX 10

typedef struct _cpufreq {
	int min;
	int max;
} _cpufreq;

static int policy_num;
static int *opp_count;
static unsigned int **opp_table;
static int *_opp_cnt;
static unsigned int **cpu_opp_tbl;
static DEFINE_MUTEX(cpu_ctrl_lock);
static struct _cpufreq freq_to_set[CLUSTER_MAX];
struct freq_qos_request *freq_min_request;
struct freq_qos_request *freq_max_request;

// ADPF
#define ADPF_MAX_SESSION 64
#define ADPF_MAX_CALLBACK 64
static struct _SESSION *sessionList[ADPF_MAX_SESSION];
static adpfCallback adpfCallbackList[ADPF_MAX_CALLBACK];
static DEFINE_MUTEX(adpf_mutex);

static void _adpf_systrace(int val, const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;
	char buf[256];

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		log[255] = '\0';

	len = snprintf(buf, sizeof(buf), "C|%d|%s|%d\n", current->pid, log, val);
	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf[255] = '\0';

	trace_powerhal_adpf(buf);
}

static void _cpu_ctrl_systrace(int val, const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;
	char buf[256];

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		log[255] = '\0';

	len = snprintf(buf, sizeof(buf), "C|%d|%s|%d\n", current->pid, log, val);
	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf[255] = '\0';

	trace_powerhal_cpu_freq_user_setting(buf);
}

char *perfmgr_copy_from_user_for_proc(const char __user *buffer, size_t count)
{
	char *buf = (char *)__get_free_page(GFP_USER);

	if (!buf)
		return NULL;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	return buf;

out:
	free_page((unsigned long)buf);

	return NULL;
}

int cmp_uint(const void *a, const void *b)
{
	return *(unsigned int *)b - *(unsigned int *)a;
}

void cpu_policy_init(void)
{
	int cpu;
	int num = 0, count;
	struct cpufreq_policy *policy;
	struct cpufreq_frequency_table *pos;

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);

		if (policy) {
			pr_info("%s, policy[%d]: first:%d, min:%d, max:%d",
				__func__, num, cpu, policy->min, policy->max);

			num++;
			cpu = cpumask_last(policy->related_cpus);
			cpufreq_cpu_put(policy);
		}
	}

	policy_num = num;

	if (policy_num == 0) {
		pr_info("%s, no policy", __func__);
		return;
	}

	opp_count = kcalloc(policy_num, sizeof(int), GFP_KERNEL);
	opp_table = kcalloc(policy_num, sizeof(unsigned int *), GFP_KERNEL);
	if (opp_count == NULL || opp_table == NULL)
		return;

	num = 0;
	for_each_possible_cpu(cpu) {
		if (num >= policy_num)
			break;

		policy = cpufreq_cpu_get(cpu);

		if (!policy)
			continue;

		/* calc opp count */
		count = 0;
		cpufreq_for_each_entry(pos, policy->freq_table) {
			count++;
		}
		opp_count[num] = count;
		opp_table[num] = kcalloc(count, sizeof(unsigned int), GFP_KERNEL);
		count = 0;
		cpufreq_for_each_entry(pos, policy->freq_table) {
			opp_table[num][count] = pos->frequency;
			count++;
		}

		sort(opp_table[num], opp_count[num], sizeof(unsigned int), cmp_uint, NULL);

		num++;
		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
	}
}

int get_cpu_opp_info(int **opp_cnt, unsigned int ***opp_tbl)
{
	int i, j;

	if (policy_num <= 0)
		return -EFAULT;

	*opp_cnt = kcalloc(policy_num, sizeof(int), GFP_KERNEL);
	*opp_tbl = kcalloc(policy_num, sizeof(unsigned int *), GFP_KERNEL);

	if (*opp_cnt == NULL || *opp_tbl == NULL)
		return -1;

	for (i = 0; i < policy_num; i++) {
		(*opp_cnt)[i] = opp_count[i];
		(*opp_tbl)[i] = kcalloc(opp_count[i], sizeof(unsigned int), GFP_KERNEL);

		for (j = 0; j < opp_count[i]; j++)
			(*opp_tbl)[i][j] = opp_table[i][j];
	}

	return 0;
}

int get_cpu_topology(void)
{
	if (get_cpu_opp_info(&_opp_cnt, &cpu_opp_tbl) < 0)
		return -EFAULT;

	return 0;
}

int update_userlimit_cpufreq_max(int cid, int value)
{
	int ret = -1;

	ret = freq_qos_update_request(&(freq_max_request[cid]), value);
	_cpu_ctrl_systrace(value, "powerhal_cpu_ctrl c%d Max", cid);

	return ret;
}

int update_userlimit_cpufreq_min(int cid, int value)
{
	int ret = -1;

	ret = freq_qos_update_request(&(freq_min_request[cid]), value);
	_cpu_ctrl_systrace(value, "powerhal_cpu_ctrl c%d min", cid);

	return ret;
}

static int perfmgr_proc_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}

static ssize_t perfmgr_perfserv_freq_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *_data)
{
	int i = 0, data;
	unsigned int arg_num = policy_num * 2;
	char *tok, *tmp;
	char *buf = perfmgr_copy_from_user_for_proc(ubuf, cnt);
	//int ret = 0;
	int update_failed = 0;

	if (!buf) {
		pr_debug("buf is null\n");
		goto out1;
	}

	tmp = buf;
	while ((tok = strsep(&tmp, " ")) != NULL) {
		if (i == arg_num) {
			pr_debug("@%s: number of arguments > %d\n", __func__, arg_num);
			goto out;
		}

		if (kstrtoint(tok, 10, &data)) {
			pr_debug("@%s: invalid input: %s\n", __func__, tok);
			goto out;
		} else {
			if (i % 2) /* max */
				freq_to_set[i/2].max = data;
			else /* min */
				freq_to_set[i/2].min = data;
			i++;
		}
	}

	if (i < arg_num) {
		pr_info("@%s: number of arguments %d < %d\n", __func__, i, arg_num);
	} else {
		for (i = 0; i < policy_num; i++) {
			if ((update_userlimit_cpufreq_max(i, freq_to_set[i].max) < 0) ||
					(update_userlimit_cpufreq_min(i, freq_to_set[i].min) < 0)) {
				pr_info("update cpufreq failed.");
				update_failed = 1;
			}
		}
	}

out:
	free_page((unsigned long)buf);
out1:
	if (update_failed)
		return -1;
	else
		return cnt;
}

static ssize_t perfmgr_perfserv_freq_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int i, n = 0;
	char buffer[512] = "";
	char _buf[64] = "";

	if (*ppos != 0)
		goto out;

	for (i = 0; i < policy_num; i++) {
		scnprintf(_buf, 64, "%d %d ", freq_to_set[i].min, freq_to_set[i].max);
		strncat(buffer, _buf, strlen(_buf));
	}
	n = scnprintf(buffer, 512, "%s\n", buffer);

out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

PROC_FOPS_RW(perfserv_freq);

int adpf_register_callback(adpfCallback callback)
{
	int i = 0;

	mutex_lock(&adpf_mutex);
	while (i < ADPF_MAX_CALLBACK) {
		if (adpfCallbackList[i] == NULL) {
			adpfCallbackList[i] = callback;
			break;
		}
		i++;
	}
	mutex_unlock(&adpf_mutex);

	if (i > ADPF_MAX_CALLBACK) {
		pr_debug("%s error: %d", __func__, i);
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(adpf_register_callback);

int adpf_unregister_callback(int idx)
{
	mutex_lock(&adpf_mutex);
	if (idx < 0 || idx >= ADPF_MAX_CALLBACK) {
		pr_debug("%s error: %d", __func__, idx);
		return -1;
	}

	adpfCallbackList[idx] = NULL;
	mutex_unlock(&adpf_mutex);

	return idx;
}
EXPORT_SYMBOL(adpf_unregister_callback);

int adpf_notify_callback(unsigned int cmd, unsigned int sid)
{
	int i = 0;
	struct _SESSION session;

	mutex_lock(&adpf_mutex);
	if (sid >= ADPF_MAX_SESSION || sessionList[sid] == NULL) {
		pr_debug("[%s] error",  __func__);
		return -1;
	}

	memcpy(&session, sessionList[sid], sizeof(struct _SESSION));
	for (i = 0; i < session.work_duration_size; i++) {
		session.workDuration[i]->timeStampNanos =
				sessionList[sid]->workDuration[i]->timeStampNanos;
		session.workDuration[i]->durationNanos =
				sessionList[sid]->workDuration[i]->durationNanos;
	}
	session.cmd = cmd;

	pr_debug("[%s] callback start, cmd: %d, sid: %d, tgid: %d, uid: %d",  __func__,
			session.cmd, session.sid, session.tgid, session.uid);
	for (i = 0; i < ADPF_MAX_CALLBACK; i++) {
		if (adpfCallbackList[i])
			adpfCallbackList[i](&session);
	}
	mutex_unlock(&adpf_mutex);

	return 0;
}

int adpf_create_session_hint(unsigned int sid, unsigned int tgid,
							unsigned int uid, int *threadIds,
							int threadIds_size, long durationNanos)
{
	char log[256];

	mutex_lock(&adpf_mutex);
	if (sid >= ADPF_MAX_SESSION || sessionList[sid]->used == SESSION_USED) {
		pr_debug("[%s] sid error: %d",  __func__, sid);
		return -1;
	}

	pr_debug("[%s] sid: %d, tgid: %d, uid: %d, threadIds_size: %d",
			__func__, sid, tgid, uid, threadIds_size);

	sessionList[sid]->sid = sid;
	sessionList[sid]->tgid = tgid;
	sessionList[sid]->uid = uid;
	sessionList[sid]->threadIds_size = threadIds_size;
	sessionList[sid]->durationNanos = durationNanos;
	memcpy(sessionList[sid]->threadIds, threadIds, threadIds_size*sizeof(int));
	sessionList[sid]->used = SESSION_USED;

	mutex_unlock(&adpf_mutex);

	adpf_notify_callback(ADPF_CREATE_HINT_SESSION, sid);

	if (sprintf(log, "adpf sid: %d, tgid: %d, uid: %d", sid, tgid, uid) < 0) {
		pr_debug("[%s] sprintf failed!",  __func__);
		return -1;
	}

	_adpf_systrace(ADPF_CREATE_HINT_SESSION, log);

	return 0;
}

int adpf_get_hint_session_preferred_rate(long long *preferredRate)
{
	// Implement in the native layer

	return 0;
}

int adpf_update_work_duaration(unsigned int sid, long targetDurationNanos)
{
	char log[256];

	mutex_lock(&adpf_mutex);
	if (sid >= ADPF_MAX_SESSION || sessionList[sid]->used == SESSION_UNUSED) {
		pr_debug("[%s] sid error: %d", __func__, sid);
		return -1;
	}

	pr_debug("[%s], sid: %d, targetDurationNanos: %ld", __func__, sid, targetDurationNanos);

	sessionList[sid]->targetDurationNanos = targetDurationNanos;
	mutex_unlock(&adpf_mutex);

	adpf_notify_callback(ADPF_UPDATE_TARGET_WORK_DURATION, sid);

	if (sprintf(log, "adpf sid: %d, targetDurationNanos: %ld", sid, targetDurationNanos) < 0) {
		pr_debug("[%s] sprintf failed!",  __func__);
		return -1;
	}

	_adpf_systrace(ADPF_UPDATE_TARGET_WORK_DURATION, log);

	return 0;
}

int adpf_report_actual_work_duaration(unsigned int sid,
		struct _ADPF_WORK_DURATION *workDuration, int work_duration_size)
{
	int i = 0;
	char log[256];

	if (sid >= ADPF_MAX_SESSION || sessionList[sid]->used == SESSION_UNUSED) {
		pr_debug("[%s] sid error: %d", __func__, sid);
		return -1;
	}

	if (sprintf(log, "sid: %d, ", sid) < 0) {
		pr_debug("[%s] sprintf failed!", __func__);
		return -1;
	}

	pr_debug("[%s], sid: %d", __func__, sid);

	mutex_lock(&adpf_mutex);
	for (i = 0; i < work_duration_size; i++) {
		sessionList[sid]->workDuration[i]->timeStampNanos = workDuration[i].timeStampNanos;
		sessionList[sid]->workDuration[i]->durationNanos = workDuration[i].durationNanos;
		pr_debug("[%s], idx: %d, timeStampNanos: %ld, durationNanos: %ld",
				__func__, i,
				workDuration[i].timeStampNanos, workDuration[i].durationNanos);
		if (sprintf(log, "timeStampNanos[%d]: %ld, durationNanos[%d]: %ld, ",
				i, workDuration[i].timeStampNanos,
				i, workDuration[i].durationNanos) < 0) {
			pr_debug("[%s] sprintf failed!", __func__);
			return -1;
		}
	}

	sessionList[sid]->sid = sid;
	sessionList[sid]->work_duration_size = work_duration_size;

	mutex_unlock(&adpf_mutex);

	adpf_notify_callback(ADPF_REPORT_ACTUAL_WORK_DURATION, sid);

	return 0;
}

int adpf_pause(unsigned int sid)
{
	char log[256];

	if (sid >= ADPF_MAX_SESSION || sessionList[sid]->used == SESSION_UNUSED) {
		pr_debug("[%s] sid error: %d", __func__, sid);
		return -1;
	}

	pr_debug("[%s], sid: %d", __func__, sid);

	adpf_notify_callback(ADPF_PAUSE, sid);

	if (sprintf(log, "adpf sid: %d", sid) < 0) {
		pr_debug("[%s] sprintf failed!",  __func__);
		return -1;
	}

	_adpf_systrace(ADPF_PAUSE, log);

	return 0;
}

int adpf_resume(unsigned int sid)
{
	char log[256];

	if (sid >= ADPF_MAX_SESSION || sessionList[sid]->used == SESSION_UNUSED) {
		pr_debug("[%s] sid error: %d", __func__, sid);
		return -1;
	}

	pr_debug("[%s], sid: %d", __func__, sid);

	adpf_notify_callback(ADPF_RESUME, sid);

	if (sprintf(log, "adpf sid: %d", sid) < 0) {
		pr_debug("[%s] sprintf failed!",  __func__);
		return -1;
	}

	_adpf_systrace(ADPF_RESUME, log);

	return 0;
}

int adpf_close(unsigned int sid)
{
	char log[256];

	if (sid >= ADPF_MAX_SESSION || sessionList[sid]->used == SESSION_UNUSED) {
		pr_debug("[%s] sid error: %d", __func__, sid);
		return -1;
	}

	pr_debug("[%s], sid: %d", __func__, sid);

	mutex_lock(&adpf_mutex);
	sessionList[sid]->used = SESSION_UNUSED;
	mutex_unlock(&adpf_mutex);

	adpf_notify_callback(ADPF_CLOSE, sid);

	if (sprintf(log, "adpf sid: %d", sid) < 0) {
		pr_debug("[%s] sprintf failed!",  __func__);
		return -1;
	}

	_adpf_systrace(ADPF_CLOSE, log);

	return 0;
}

int adpf_sent_hint(unsigned int sid, int hint)
{
	char log[256];

	if (sid >= ADPF_MAX_SESSION || sessionList[sid]->used == SESSION_UNUSED) {
		pr_debug("[%s] sid error: %d", __func__, sid);
		return -1;
	}

	pr_debug("[%s], sid: %d, hint: %d", __func__, sid, hint);

	mutex_lock(&adpf_mutex);
	sessionList[sid]->hint = hint;
	mutex_unlock(&adpf_mutex);

	adpf_notify_callback(ADPF_SENT_HINT, sid);

	if (sprintf(log, "adpf sid: %d hint: %d", sid, hint) < 0) {
		pr_debug("[%s] sprintf failed!",  __func__);
		return -1;
	}

	_adpf_systrace(ADPF_SENT_HINT, log);

	return 0;
}

int adpf_set_threads(unsigned int sid, int *threadIds, int threadIds_size)
{
	char log[256];

	if (sid >= ADPF_MAX_SESSION || sessionList[sid]->used == SESSION_UNUSED) {
		pr_debug("[%s] sid error: %d", __func__, sid);
		return -1;
	}

	if (sid >= ADPF_MAX_SESSION) {
		pr_debug("[%s] sid error: %d", __func__, sid);
		return -1;
	}

	pr_debug("[%s] sid: %d, threads_size: %d", __func__, sid, threadIds_size);

	mutex_lock(&adpf_mutex);
	if (sessionList[sid] != NULL) {
		memset(sessionList[sid]->threadIds, 0,
			sessionList[sid]->threadIds_size*sizeof(int));
		memcpy(sessionList[sid]->threadIds,
			threadIds, threadIds_size*sizeof(int));
		sessionList[sid]->threadIds_size = threadIds_size;

		pr_debug("[%s] sid: %d, tgid: %d, uid: %d, threadIds_size: %d",
			__func__, sessionList[sid]->sid, sessionList[sid]->tgid,
			sessionList[sid]->uid, sessionList[sid]->threadIds_size);
	}
	mutex_unlock(&adpf_mutex);

	adpf_notify_callback(ADPF_SET_THREADS, sid);

	if (sprintf(log, "adpf sid: %d", sid) < 0) {
		pr_debug("[%s] sprintf failed!",  __func__);
		return -1;
	}

	_adpf_systrace(ADPF_SET_THREADS, log);

	return 0;
}

static int __init powerhal_cpu_ctrl_init(void)
{
	int cpu_num = 0;
	int num = 0;
	int cpu;
	int i, j, ret = 0;
	struct proc_dir_entry *lt_dir = NULL;
	struct proc_dir_entry *parent = NULL;
	struct cpufreq_policy *policy;
	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};
	const struct pentry entries[] = {
		PROC_ENTRY(perfserv_freq),
	};

	lt_dir = proc_mkdir("powerhal_cpu_ctrl", parent);
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0644, lt_dir, entries[i].fops)) {
			pr_info("%s(), lt_dir%s failed\n", __func__, entries[i].name);
			ret = -EINVAL;
			return ret;
		}
	}

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);

		if (policy) {
			pr_info("%s, policy[%d]: first:%d, min:%d, max:%d",
				__func__, cpu_num, cpu, policy->min, policy->max);

			cpu_num++;
			cpu = cpumask_last(policy->related_cpus);
			cpufreq_cpu_put(policy);
		}
	}
	policy_num = cpu_num;
	pr_info("%s, cpu_num:%d\n", __func__, policy_num);
	if (policy_num == 0) {
		pr_info("%s, no cpu policy (policy_num=%d)\n", __func__, policy_num);
		return 0;
	}

	cpu_policy_init();

	if (get_cpu_topology() < 0) {
		kvfree(opp_count);
		kvfree(opp_table);
		return -EFAULT;
	}

	for (i = 0; i < policy_num; i++) {
		freq_to_set[i].min = 0;
		freq_to_set[i].max = cpu_opp_tbl[i][0];
	}

	freq_min_request = kcalloc(policy_num, sizeof(struct freq_qos_request), GFP_KERNEL);
	freq_max_request = kcalloc(policy_num, sizeof(struct freq_qos_request), GFP_KERNEL);
	if (freq_min_request == NULL || freq_max_request == NULL)
		return 0;

	num = 0;
	for_each_possible_cpu(cpu) {
		if (num >= policy_num)
			break;

		policy = cpufreq_cpu_get(cpu);

		if (!policy)
			continue;

		freq_qos_add_request(&policy->constraints,
			&(freq_min_request[num]),
			FREQ_QOS_MIN,
			0);
		freq_qos_add_request(&policy->constraints,
			&(freq_max_request[num]),
			FREQ_QOS_MAX,
			cpu_opp_tbl[num][0]);

		num++;
		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
	}

	// ADPF
	for (i = 0; i < ADPF_MAX_SESSION; i++) {
		sessionList[i] = kcalloc(1, sizeof(struct _SESSION), GFP_KERNEL);
		for (j = 0; j < ADPF_MAX_THREAD; j++) {
			sessionList[i]->workDuration[j] =
				kcalloc(1, sizeof(struct _SESSION_WORK_DURATION), GFP_KERNEL);
		}
	}

	powerhal_adpf_create_session_hint_fp = adpf_create_session_hint;
	powerhal_adpf_get_hint_session_preferred_rate_fp = adpf_get_hint_session_preferred_rate;
	powerhal_adpf_update_work_duration_fp = adpf_update_work_duaration;
	powerhal_adpf_report_actual_work_duration_fp = adpf_report_actual_work_duaration;
	powerhal_adpf_pause_fp = adpf_pause;
	powerhal_adpf_resume_fp = adpf_resume;
	powerhal_adpf_close_fp = adpf_close;
	powerhal_adpf_sent_hint_fp = adpf_sent_hint;
	powerhal_adpf_set_threads_fp = adpf_set_threads;

	return 0;
}

static void __exit powerhal_cpu_ctrl_exit(void)
{
	kvfree(opp_count);
	kvfree(opp_table);
	kvfree(freq_min_request);
	kvfree(freq_max_request);
}

module_init(powerhal_cpu_ctrl_init);
module_exit(powerhal_cpu_ctrl_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek POWERHAL_CPU_CTRL");
MODULE_AUTHOR("MediaTek Inc.");
