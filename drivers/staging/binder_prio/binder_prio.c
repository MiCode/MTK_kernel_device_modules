#include <linux/swap.h>
#include <linux/module.h>
#include <trace/hooks/binder.h>
#include <uapi/linux/android/binder.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched/prio.h>
#include <drivers/android/binder_internal.h>
#include <sched/sched.h>
#include <linux/string.h>

static const char * const task_name[] = {
	"com.miui.home",
	"ndroid.systemui",
	"surfaceflinger",
        "com.android.chrome",
        "com.facebook.katana",
        "com.dts.freefiremax",
        "com.instagram.android",
        "com.facebook.lite",
        "com.mxtech.videoplayer.ad",
        "com.google.android.apps.maps",
        "com.facebook.orca",
        "com.netflix.mediaclient",
        "com.playit.videoplayer",
        "com.android.vending",
        "com.snapchat.android",
        "com.spotify.music",
        "org.telegram.messenger",
        "com.truecaller",
        "com.whatsapp.w4b",
        "com.whatsapp",
        "com.imo.android.imoim",
        "com.google.android.youtube",
        "com.twitter.android",
        "m.facebook.orca",
        "android.vending",
        "nt.tmgp.pubgmhd",
};

static const char *RenderThread = "RenderThread";
static const char *wmshell_main = "wmshell.main";
static const char *com_android_systemui = "ndroid.systemui";
static const char *wmshell_splashworker = "ll.splashscreen";

static const char * const lunch_name_binder_tid[][3] = {
        {"composer@3.4-se", "binder:", "binder:"},
};

static const char * const lunch_name_tid[][3] = {
        {"surfaceflinger", "binder:", "binder:"},
#ifdef CONFIG_BINDER_PRIO_MIDDLE
        {"ndroid.systemui", "wmshell.main", "com.miui.home"},
        {"system_server", "binder:", "ndroid.systemui"},
#endif
        {"com.miui.home", "binder:", "system_server"},
        {"com.miui.home", "com.miui.home", "system_server"},
};

static int to_userspace_prio(int policy, int kernel_priority) {
	if (fair_policy(policy))
		return PRIO_TO_NICE(kernel_priority);
	else
		return MAX_RT_PRIO - 1 - kernel_priority;
}

static bool set_binder_rt_task(struct binder_transaction *t) {
	int i;
	if (!t || !t->from || !t->from->task || !t->to_proc || !t->to_proc->tsk) {
		return false;
	}

	if (t->flags & TF_ONE_WAY) {
		return false;
	}

	if (!rt_policy(t->from->task->policy)) {
		return false;
	}

        if ((strncmp(t->from->task->group_leader->comm, com_android_systemui, strlen(com_android_systemui)) == 0)
                && (strncmp(t->from->task->comm, wmshell_main, strlen(wmshell_main)) == 0)) {
                return true;
        }

        if ((strncmp(t->from->task->group_leader->comm, com_android_systemui, strlen(com_android_systemui)) == 0)
                && (strncmp(t->from->task->comm, wmshell_splashworker, strlen(wmshell_splashworker)) == 0)) {
                return true;
        }

	if ((strncmp(t->from->task->group_leader->comm, task_name[0], strlen(task_name[0])) == 0)
		&& (strncmp(t->from->task->comm, RenderThread, strlen(RenderThread)) == 0)
		&& (strncmp(t->to_proc->tsk->comm, task_name[2], strlen(task_name[2])) == 0)) {
		return true;
	}

	if (t->from->task->pid == t->from->task->tgid) {
		for(i = 0; i < sizeof(task_name)/sizeof(task_name[0]); i++) {
			if (strncmp(t->from->task->comm, task_name[i], strlen(task_name[i])) == 0) {
				return true;
			}
		}
		return false;
	}
    	if (t && t->from && t->from->task && (!(t->flags & TF_ONE_WAY))
	    && rt_policy(t->from->task->policy)) {
          	for (int j = 0; j < sizeof(lunch_name_tid)/sizeof(lunch_name_tid[0]); j ++) {
		    if ((strncmp(t->from->task->group_leader->comm, lunch_name_tid[j][0], strlen(lunch_name_tid[j][0])) == 0)
                	    && (strstr(t->from->task->comm, lunch_name_tid[j][1]) != NULL)
                	    && (strncmp(t->to_proc->tsk->comm, lunch_name_tid[j][2], strlen(lunch_name_tid[j][2])) == 0)) {
                	    return true;
        	    }
                }
                return false;
	}
	if (t && t->from && t->from->task && (!(t->flags & TF_ONE_WAY))
	    && rt_policy(t->from->task->policy)) {
		for (int j = 0; j < sizeof(lunch_name_binder_tid)/sizeof(lunch_name_binder_tid[0]); j ++) {
			if ((strncmp(t->from->task->group_leader->comm, lunch_name_binder_tid[j][0], strlen(lunch_name_binder_tid[j][0])) == 0)
                	    && (strstr(t->from->task->comm, lunch_name_binder_tid[j][1]) != NULL)
                	    && (strncmp(t->to_proc->tsk->comm, lunch_name_binder_tid[j][2], strlen(lunch_name_binder_tid[j][2])) == 0)) {
				return true;
			}
		}
        	return false;
	}
	return false;
}

static void extend_surfacefinger_binder_set_priority_handler(void *data, struct binder_transaction *t, struct task_struct *task) {
	struct sched_param params;
	struct binder_priority desired;
	unsigned int policy;
	struct binder_node *target_node = t->buffer->target_node;

	desired.prio = target_node->min_priority;
	desired.sched_policy = target_node->sched_policy;
	policy = desired.sched_policy;
	if (set_binder_rt_task(t)) {
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
	if (target_proc && target_proc->tsk && strncmp(target_proc->tsk->comm, "surfaceflinger",
		strlen("surfaceflinger")) == 0) {
		if (thread && proc && tr && thread->transaction_stack
			&& (!(thread->transaction_stack->flags & TF_ONE_WAY))) {
			target_proc->default_priority.sched_policy = SCHED_FIFO;
			target_proc->default_priority.prio = 98;
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
                sched_setscheduler_nocheck(binder_th_task, SCHED_FIFO | SCHED_RESET_ON_FORK, &param);
            }
        }
    }
}

int __init binder_prio_init(void)
{
    pr_info("binder_prio: module init!");
    register_trace_android_vh_binder_set_priority(extend_surfacefinger_binder_set_priority_handler, NULL);
    register_trace_android_vh_binder_trans(extend_surfacefinger_binder_trans_handler, NULL);
    register_trace_android_vh_binder_proc_transaction_finish(extend_android_vh_binder_proc_transaction_finish, NULL);

    return 0;
}

void __exit binder_prio_exit(void)
{
    unregister_trace_android_vh_binder_set_priority(extend_surfacefinger_binder_set_priority_handler, NULL);
    unregister_trace_android_vh_binder_trans(extend_surfacefinger_binder_trans_handler, NULL);
    unregister_trace_android_vh_binder_proc_transaction_finish(extend_android_vh_binder_proc_transaction_finish, NULL);

    pr_info("binder_prio: module exit!");
}

module_init(binder_prio_init);
module_exit(binder_prio_exit);
MODULE_LICENSE("GPL");
