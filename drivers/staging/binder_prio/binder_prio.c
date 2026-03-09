#include <linux/swap.h>
#include "linux/sysctl.h"
#include <linux/sysfs.h>
#include "linux/types.h"
#include <linux/module.h>
#include <trace/hooks/binder.h>
#include <uapi/linux/android/binder.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched/prio.h>
#include <linux/sched/cputime.h>
#include <../../../../kernel-6.6/drivers/android/binder_internal.h>
#include <../../../../kernel-6.6/kernel/sched/sched.h>
#include <linux/string.h>

#define CREATE_TRACE_POINTS
#include "binder_prio_trace.h"

static struct ctl_table_header *sysctl_header;
struct binder_priority home_saved_priority = {SCHED_NORMAL, 120};

static pid_t composer_pid = 0;
static pid_t allocator_pid = 0;
static pid_t arm_mali_pid = 0;

static const char * const task_name[] = {
	"com.miui.home",
	"ndroid.systemui",
	"surfaceflinger",
	".globallauncher",
};

static const char * const task_name_tid[] = {
	"wmshell.main",
	"ll.splashscreen",
};

static const char * const lunch_name_tid[][3] = {
        {"ndroid.systemui", "wmshell.main", "com.miui.home"},
        {"ndroid.systemui", "ll.splashscreen", "system_server"},
        {"system_server", "android.anim", "ndroid.systemui"},
        {"system_server", "binder:", "ndroid.systemui"},
        {"system_server", "binder:", "com.miui.home"},
        {"surfaceflinger", "surfaceflinger", "com.miui.home"},
};

static const char *passBlur = "passBlur";

static int to_userspace_prio(int policy, int kernel_priority) {
	if (fair_policy(policy))
		return PRIO_TO_NICE(kernel_priority);
	else
		return MAX_RT_PRIO - 1 - kernel_priority;
}

static inline bool taskname_in_list(char* name, const char * const list[], int size) {
	int i;
	for(i = 0; i < size; i++) {
		if (strncmp(name, list[i], strlen(list[i])) == 0) {
			return true;
		}
	}
	return false;
}

static bool set_binder_rt_allocator(struct binder_transaction *t, struct task_struct* traget) {

	if (unlikely(allocator_pid == 0)) return false;
	if (unlikely(t->from->proc == NULL || t->to_proc->tsk == NULL)) return false;
	if (likely(traget->group_leader->pid != allocator_pid)) return false;

	if(taskname_in_list(t->from->proc->tsk->comm, task_name, sizeof(task_name)/sizeof(task_name[0]))) {
		return true;
	}

	if (t->from->proc->tsk->group_leader != NULL &&
		strncmp(t->from->proc->tsk->group_leader->comm, task_name[0], strlen(task_name[0])) == 0) {
        return true;
    }

	if (composer_pid > 0 && t->from->proc->tsk->pid == composer_pid) return true;

	return false;
}

static bool set_binder_rt_task(struct binder_transaction *t, struct task_struct* traget) {
	if (unlikely(!t || !t->from || !t->from->task || !traget || traget->group_leader == NULL)) {
		return false;
	}

	if (t->flags & TF_ONE_WAY) {
		return false;
	}

	if (unlikely(set_binder_rt_allocator(t, traget))) {
		return true;
	}

	if (!rt_policy(t->from->task->policy)) {
		return false;
	}

	if (traget->pid == traget->tgid) {
		if (strncmp(traget->comm, task_name[3], strlen(task_name[3])) == 0) {
			return true;
		}
		if (arm_mali_pid > 0 && traget->pid == arm_mali_pid) {
			return true;
		}
	}

	if ((strncmp(t->from->task->group_leader->comm, task_name[2], strlen(task_name[2])) == 0)
		&& (strncmp(t->from->task->comm, passBlur, strlen(passBlur)) == 0)) {
		return true;
	}

	if (t->from->task->pid == t->from->task->tgid) {
		if(taskname_in_list(t->from->task->comm, task_name, sizeof(task_name)/sizeof(task_name[0]))) {
			return true;
		}
	} else {
		if(taskname_in_list(t->from->task->comm, task_name_tid, sizeof(task_name_tid)/sizeof(task_name_tid[0]))) {
			return true;
		}
	}
	return false;
}

static void extend_binder_restore_prio_handler(void *data, struct binder_transaction *t, struct task_struct *task) {
	if (t != NULL && t->to_proc) {
		trace_binder_prio_restore(t->debug_id, t->from_pid, t->from_tid, t->to_proc->pid,
								  t->saved_priority.sched_policy, t->saved_priority.prio);
	} else {
		trace_binder_prio_restore(0, 0, 0, 0, 0, 0);
	}
}

static void extend_surfacefinger_binder_set_priority_handler(void *data, struct binder_transaction *t, struct task_struct *task) {
	struct sched_param params;
	struct binder_priority desired;
	unsigned int policy;
	struct binder_node *target_node = t->buffer->target_node;

	desired.prio = target_node->min_priority;
	desired.sched_policy = target_node->sched_policy;
	policy = desired.sched_policy;
	if (set_binder_rt_task(t, task)) {
		desired.sched_policy = SCHED_FIFO;
		desired.prio = 98;
		policy = desired.sched_policy;
	}
	if (rt_policy(policy) && task->policy != policy) {
		params.sched_priority = to_userspace_prio(policy, desired.prio);
		sched_setscheduler_nocheck(task, policy | SCHED_RESET_ON_FORK, &params);
	}
}

static void extend_surfacefinger_binder_trans_handler(void *data, struct binder_proc *target_proc,
    struct binder_proc *proc,struct binder_thread *thread, struct binder_transaction_data *tr) {
	if (target_proc && target_proc->tsk) {
		if (strncmp(target_proc->tsk->comm, "surfaceflinger", strlen("surfaceflinger")) == 0) {
			if (thread && proc && tr && thread->transaction_stack
				&& (!(thread->transaction_stack->flags & TF_ONE_WAY))) {
				target_proc->default_priority.sched_policy = SCHED_FIFO;
				target_proc->default_priority.prio = 98;
			}
		} else if (strncmp(target_proc->tsk->comm, task_name[0], strlen(task_name[0])) == 0) {

			if (rt_policy(target_proc->tsk->policy)) {
				if (!rt_policy(target_proc->default_priority.sched_policy)) {
					trace_binder_prio_proc_default_prio(target_proc->tsk->comm, 1);
					home_saved_priority = target_proc->default_priority;
					target_proc->default_priority.sched_policy = SCHED_FIFO;
					target_proc->default_priority.prio = 98;
				}
			} else {
				if (rt_policy(target_proc->default_priority.sched_policy)) {
					trace_binder_prio_proc_default_prio(target_proc->tsk->comm, 0);
					target_proc->default_priority = home_saved_priority;
				}
			}
		}
	}
}

static void extend_android_vh_binder_proc_transaction_finish(void *data, struct binder_proc *proc, struct binder_transaction *t,
                     struct task_struct *binder_th_task, bool pending_async, bool sync) {
	struct task_struct* call = current;

	unsigned int policy = call->policy;
	struct sched_param param = {};

	if (likely(sync || !rt_policy(call->policy))) return;
	if (unlikely(binder_th_task == NULL || call->group_leader == NULL)) return;
	if (unlikely(proc == NULL || proc->tsk == NULL)) return;

	for (int i = 0; i < sizeof(lunch_name_tid)/sizeof(lunch_name_tid[0]); i ++) {
		if (unlikely(strcmp(lunch_name_tid[i][0], call->group_leader->comm) == 0
			&& (strcmp(lunch_name_tid[i][1], call->comm) == 0 || strstr(call->comm, lunch_name_tid[i][1]) != NULL)
			&& strcmp(lunch_name_tid[i][2], proc->tsk->comm) == 0)) {

			param.sched_priority = to_userspace_prio((int) policy, call->prio);
			if (policy != binder_th_task->policy || param.sched_priority != binder_th_task->prio) {
				// trace_binder_prio_proc_transaction_finish(binder_th_task->comm);
				sched_setscheduler_nocheck(binder_th_task, SCHED_FIFO | SCHED_RESET_ON_FORK, &param);
			}
		}
	}
}

struct ctl_table binder_prio_table[] = {
	{
		.procname       = "gallocator_pid",
		.data           = &allocator_pid,
		.maxlen         = sizeof(pid_t),
		.mode           = 0666,
		.proc_handler   = proc_dointvec,
		.extra1         = SYSCTL_ZERO,
	},
	{
		.procname       = "composer_pid",
		.data           = &composer_pid,
		.maxlen         = sizeof(pid_t),
		.mode           = 0666,
		.proc_handler   = proc_dointvec,
		.extra1         = SYSCTL_ZERO,
	},
	{
		.procname       = "arm_mali_pid",
		.data           = &arm_mali_pid,
		.maxlen         = sizeof(pid_t),
		.mode           = 0666,
		.proc_handler   = proc_dointvec,
		.extra1         = SYSCTL_ZERO,
	},
	{ },
};

int __init binder_prio_init(void)
{
	pr_info("binder_prio: module init!");
	register_trace_android_vh_binder_restore_priority(extend_binder_restore_prio_handler, NULL);
	register_trace_android_vh_binder_set_priority(extend_surfacefinger_binder_set_priority_handler, NULL);
	register_trace_android_vh_binder_trans(extend_surfacefinger_binder_trans_handler, NULL);

	register_trace_android_vh_binder_proc_transaction_finish(extend_android_vh_binder_proc_transaction_finish, NULL);

	sysctl_header = register_sysctl("binder_prio", binder_prio_table);
	if (!sysctl_header) {
		pr_err("binder_prio: register_sysctl_table failed\n");
	}
	return 0;
}

void __exit binder_prio_exit(void)
{
	unregister_trace_android_vh_binder_restore_priority(extend_binder_restore_prio_handler, NULL);
	unregister_trace_android_vh_binder_set_priority(extend_surfacefinger_binder_set_priority_handler, NULL);
	unregister_trace_android_vh_binder_trans(extend_surfacefinger_binder_trans_handler, NULL);

	unregister_trace_android_vh_binder_proc_transaction_finish(extend_android_vh_binder_proc_transaction_finish, NULL);

	if (sysctl_header) {
		unregister_sysctl_table(sysctl_header);
	}

	pr_info("binder_prio: module exit!");
}

module_init(binder_prio_init);
module_exit(binder_prio_exit);
MODULE_LICENSE("GPL");
