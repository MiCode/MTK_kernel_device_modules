#include "asm/bug.h"
#include "kscene.h"
//#include "kscene_atrace.h"
#include "kscene_ioctl.h"
#include "linux/hrtimer.h"
#include "linux/moduleparam.h"
#include "linux/pid.h"
#include "linux/rcupdate.h"
#include "linux/sched.h"
#include "linux/uaccess.h"
#include <linux/kfifo.h>

#define USE_TEST_KTHREAD 0

#define KSCENE_OEM_DATA_IDX 4
#define KSCENE_MAGIC 0xF2F0000000000000ULL

#define KSCENE_TYPE_MASK  0xFF
#define KSCENE_MAGIC_MASK 0xFFFF000000000000ULL

#define FG_GROUP_ID	      ((2-1)*10)
#define TOP_APP_GROUP_ID  ((4-1)*10)

enum KSCENE_TASK_TYPE {
	KSCENE_RENDER = 1,
	KSCENE_WEB_RENDER,
	KSCENE_APPBRAND_RENDER,
	KSCENE_FLUTTER_RENDER,
	KSCENE_TYPE_MAX = 128,
};

// --- local function declaration
static void kscene_wakeup_kthr(void);

//static const char *TOUCH_TAG = "kscene_touch";

DEFINE_KFIFO(render_frames, int, 16);

struct kscene_task {
	char comm[TASK_COMM_LEN];
	enum KSCENE_TASK_TYPE type;
};

static struct kscene_state {
	bool enable;
	bool enable_sched;
	bool is_up_limit;
	bool ui_frame_begin;
	bool frame_finished;
	bool last_frame_finished;
	bool is_stm;
	bool is_temp_failure;
	bool is_no_limit;
	int ux_cnt;
	int last_ux_cnt;
	int board_temp;
	int sbe_min_cap;
	int traversal_cnt;
	int last_traversal_cnt;
	pid_t render_tid;
	int64_t vsync_id;
	int64_t last_vsync_id;
	u64 last_home_anim_ts;
	unsigned long long vsync_period_ns;
	enum kscene_touch_state touch_state;
	enum kscene_frame_event frame_step;
	enum kscene_gaidance guidance;
	struct sched_avg top_avg;
	struct sched_avg render_avg;
} kscene_state;

struct kscene_task THREAD_NAME[]= {
		{ "RenderThread", KSCENE_RENDER },
		{ "Chrome_IOThread", KSCENE_WEB_RENDER },
		{ "CrRendererMain", KSCENE_WEB_RENDER },
		{ "Compositor", KSCENE_WEB_RENDER },
		{ "CrGpuMain", KSCENE_WEB_RENDER },
		{ "VizCompositorTh", KSCENE_WEB_RENDER },
		{ "Chrome_InProcRe", KSCENE_APPBRAND_RENDER },
		{ "Chrome_InProcGp", KSCENE_APPBRAND_RENDER },
};

static pid_t pid_home = 0;
static pid_t pid_systemui = 0;
static pid_t pid_system_server = 0;
static pid_t pid_sf = 0;

static int   focused_type = 0;
static pid_t pid_focused = 0;

static struct hrtimer frame_timer;
static struct hrtimer click_timer;

int debug = 0;
int trace_level = 1;
static int enable = true;
static int enable_sched_enhance = false;
static int game_mode = false;

static int debug_pick_next = false;
static int debug_preempt = false;

static int cluster_1_max_freq = 2800000;
static int cluster_2_max_freq = 2900000;
static int failure_temp = 45;
static int recovery_temp = 43;

static struct kscene_cpu_topo {
	int cpus;
	int clusters;
	uint (*cluster_cpus)[KSCENE_CPU_TOPO_ARRAY_DIM];
} kscene_topo;

static atomic_t boost_state = ATOMIC_INIT(0);
enum kscene_action {
	KSCENE_ACTION_INVALID = -1,
	KSCENE_ACTION_BOOST,
	KSCENE_ACTION_RESTORE,
	KSCENE_ACTION_CNT,
};
static atomic_t pending_action = ATOMIC_INIT(KSCENE_ACTION_INVALID);
static bool have_pending_action = false;

static void update_boost_state(bool state) {
	atomic_set(&boost_state, state);
}

static int is_boosting(void) {
	return atomic_read(&boost_state);
}

static void set_pending_action(enum kscene_action action) {
	atomic_set(&pending_action, action);
	if (action != KSCENE_ACTION_INVALID) {
		have_pending_action = true;
	} else {
		have_pending_action = false;
	}
}

static enum kscene_action get_pending_action(void) {
	return atomic_read(&pending_action);
}

static const int64_t INVALID_VSYNC_ID = -999999;

static const char *STR_ACTION = "kscene_action";
static const char *TIMER_TAG = "tm";

static struct task_struct *kscene_thr = NULL;
static DECLARE_WAIT_QUEUE_HEAD(kscene_thr_wq);

#ifdef USE_TEST_KTHREAD
struct event_data {
	int event;
	void *data;
	struct list_head list;
};

static LIST_HEAD(event_queue);
static DEFINE_SPINLOCK(queue_lock);

static DECLARE_WAIT_QUEUE_HEAD(event_wait);

static struct task_struct *loop_kthr;

#endif

// local functions declaration
static void kscene_cancel_frame_timer(void);
static int kscene_init_click_hrtimer(void);
static void kscene_cancel_click_timer(void);
static void kscene_start_click_timer(void);

extern void set_next_entity(struct cfs_rq *cfs, struct sched_entity *se);

// global feature switch
bool kscene_enabled(void) {
	return enable != 0;
}

// logic enable state query
bool kscene_logic_enabled(void) {
	return kscene_state.enable;
}

__attribute__((unused))
static noinline int tracing_mark_write(const char* format, ...)
{
	char buf[TRACE_BUFFER_LEN] = {0};
	int pid = TRACE_FAKE_PID;
	va_list args;
	int len = 0;

	snprintf(buf, TRACE_BUFFER_LEN, "E|%d\n", pid);
	trace_puts(buf);
	memset(buf, 0, TRACE_BUFFER_LEN);

	va_start(args, format);
	snprintf(buf, TRACE_BUFFER_LEN, "B|%d|KSCENE: ", pid);
	len = strlen(buf);
	vsnprintf(buf + len, TRACE_BUFFER_LEN - len, format, args);
	trace_puts(buf);
	va_end(args);

	return 0;
}

__attribute__((unused))
static int kscene_queue_render_frame(int vsync_id) {
	unsigned int num_queued = kfifo_in(&render_frames, &vsync_id, sizeof(int));
	if (num_queued != 1) {
		ks_dbg("Error: kfifo_in failed");
		return -1;
	}
	return 0;
}

__attribute__((unused))
static int kscene_dequeue_render_frame(void) {
	int vsync_id;

	unsigned int num_dequeued = kfifo_out(&render_frames, &vsync_id, sizeof(int));

	if (num_dequeued != 1) {
		ks_dbg("Error: kfifo_out failed");
		return -1;
	}
	return 0;
}

static void kscene_clear_render_frame(void) {
	kfifo_reset(&render_frames);
}

__attribute__((unused))
static bool kscene_has_pending_render_frame(void) {
	return !kfifo_is_empty(&render_frames);
}

static void kscene_handle_proc_focus(int pid, int type) {
	focused_type = type;
	if (pid != pid_focused && pid > 0) {
		ks_dbg("new proc_focus: %d", pid);
		kscene_clear_render_frame();
		tracing_counter(TRACE_LEVEL_DEBUG, "cancel-timer", 4);
		kscene_cancel_frame_timer();
		kscene_cancel_click_timer();
		kscene_state.touch_state = TOUCH_STAT_IDLE;
		pid_focused = pid;
		kscene_state.render_tid = 0;
		tracing_counter(TRACE_LEVEL_ALWAYS, "rt", kscene_state.render_tid);
		tracing_counter(TRACE_LEVEL_ALWAYS, "f", pid_focused);

		kscene_state.ux_cnt = 0;
		kscene_state.vsync_id = INVALID_VSYNC_ID;
		memset(&kscene_state.top_avg, 0, sizeof(kscene_state.top_avg));
		memset(&kscene_state.render_avg, 0, sizeof(kscene_state.render_avg));

		rcu_read_lock();
		struct pid *pid_struct = find_vpid(pid_focused);
		if (!pid_struct) {
			rcu_read_unlock();
			goto out;
		}
		struct task_struct *tsk = get_pid_task(pid_struct, PIDTYPE_PID);
		if (!tsk) {
			rcu_read_unlock();
			goto out;
		}
		if (strncmp(STR_HOME_COMM, tsk->comm, TASK_COMM_LEN) == 0) {
			pid_home = pid_focused;
		} else if (strncmp(STR_SYSTEMUI_COMM, tsk->comm, TASK_COMM_LEN) == 0) {
			pid_systemui = pid_focused;
		}
		rcu_read_unlock();
	}
out:
	return;
}

static void update_thermal_limit_info(uint32_t cpu_id, uint32_t freq) {

	for (int i = 1; i < kscene_topo.clusters; i++) {
		uint cpu = kscene_topo.cluster_cpus[i][KSCENE_CPU_TOPO_CLUSTER_START_IDX];
		if (cpu == cpu_id) {
			kscene_topo.cluster_cpus[i][KSCENE_CPU_TOPO_THERMAL_FREQ_IDX] = freq;
			break;
		}
	}

	if (kscene_topo.clusters == 3
			&& kscene_topo.cluster_cpus[1][KSCENE_CPU_TOPO_THERMAL_FREQ_IDX] >= kscene_topo.cluster_cpus[1][KSCENE_CPU_TOPO_SET_UP_FREQ]
			&& kscene_topo.cluster_cpus[2][KSCENE_CPU_TOPO_THERMAL_FREQ_IDX] >= kscene_topo.cluster_cpus[2][KSCENE_CPU_TOPO_SET_UP_FREQ]) {
		tracing_counter(TRACE_LEVEL_ALWAYS, "no_limit", 1);
		kscene_state.is_no_limit = true;
		return;
	}
	tracing_counter(TRACE_LEVEL_ALWAYS, "no_limit", 0);
	kscene_state.is_no_limit = false;
}

static void restore_thermal_limit(void) {
	uint cpu, freq;

	update_boost_state(false);
	for (int i = 1; i < kscene_topo.clusters; i++) {
		cpu = kscene_topo.cluster_cpus[i][KSCENE_CPU_TOPO_CLUSTER_START_IDX];
		freq = kscene_topo.cluster_cpus[i][KSCENE_CPU_TOPO_THERMAL_FREQ_IDX];
		if (freq != INVALID_FREQ) {
			thermal_set_freq_max(cpu, freq);
		}
	}
}

static void kscene_do_higher_max_freq(void) {
	ks_dbg("do higher max freq");
	update_boost_state(true);
	for (int i = 1; i < kscene_topo.clusters; i++) { // skip litter core
		uint cpu = kscene_topo.cluster_cpus[i][KSCENE_CPU_TOPO_CLUSTER_START_IDX];
		uint max_freq = kscene_topo.cluster_cpus[i][KSCENE_CPU_TOPO_SET_UP_FREQ];
		ks_dbg("do higher max freq cpu %d, freq %d", cpu, max_freq);
		if (max_freq == INVALID_FREQ) {
			continue;
		}
		if (max_freq <= kscene_topo.cluster_cpus[i][KSCENE_CPU_TOPO_THERMAL_FREQ_IDX]) {
			continue;
		}
		if (debug) {
			if (cpu == 4)
				tracing_counter(TRACE_LEVEL_INFO, "cpu-4-max", max_freq);
			if (cpu == 7)
				tracing_counter(TRACE_LEVEL_INFO, "cpu-7-max", max_freq);
		}
		thermal_set_freq_max(cpu, max_freq);
	}
}

static enum hrtimer_restart frame_timer_func(struct hrtimer *timer)
{
	tracing_counter(TRACE_LEVEL_ALWAYS, TIMER_TAG, 2);
	tracing_counter(TRACE_LEVEL_DEBUG, "frame-timer-func_buf-cnt", kscene_state.ux_cnt);
	ks_dbg("Called, touch_state: %d, ux_cnt: %d, min_cap=%d", kscene_state.touch_state, kscene_state.ux_cnt, kscene_state.sbe_min_cap);

	if (kscene_state.touch_state != TOUCH_STAT_IDLE && kscene_state.ux_cnt <= 2) {
		kscene_state.is_up_limit = true;
		if (!is_boosting()) {
			if (kscene_state.touch_state < TOUCH_STAT_SCROLL
					|| kscene_state.sbe_min_cap >= KSCENE_UP_LIMIT_CAP_THRESHOLD) {
				ks_dbg("wakeup kthr");
				set_pending_action(KSCENE_ACTION_BOOST);
				kscene_wakeup_kthr();
			}
		}
	}

	return HRTIMER_NORESTART;
}

static int kscene_init_frame_hrtimer(void) {
	hrtimer_init(&frame_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	frame_timer.function = &frame_timer_func;

	return 0;
}

static void kscene_restore_thermal_limit(void) {
	if (is_boosting()) {
		set_pending_action(KSCENE_ACTION_RESTORE);
		kscene_wakeup_kthr();
	}
}

static void kscene_cancel_frame_timer(void) {
	if (hrtimer_active(&frame_timer)) {
		ks_dbg("Cancel timer");
		tracing_counter(TRACE_LEVEL_ALWAYS, TIMER_TAG, 0);
		hrtimer_cancel(&(frame_timer));
	}

	kscene_restore_thermal_limit();
}

static void kscene_start_frame_timer(unsigned long long dur_ns) {
	tracing_counter(TRACE_LEVEL_ALWAYS, TIMER_TAG, 1);
	hrtimer_start(&(frame_timer), ns_to_ktime(dur_ns), HRTIMER_MODE_REL);
}

static void set_kthr_cfs(void) {
	if (kscene_thr) {
		struct sched_param param = { .sched_priority = 0};
		sched_setscheduler_nocheck(kscene_thr, SCHED_NORMAL, &param);
		ks_dbg("kthr: policy=%d, priority=%d\n", kscene_thr->policy, kscene_thr->prio);
	}
}

static void set_kthr_fifo(void) {
	if (kscene_thr) {
		struct sched_param param = { .sched_priority = 1 };
		sched_setscheduler_nocheck(kscene_thr, SCHED_FIFO, &param);
		ks_dbg("kthr: policy=%d, priority=%d\n", kscene_thr->policy, kscene_thr->prio);
	}
}

static void kscene_update_enable_state(void) {

	// board temp
	if (kscene_state.is_temp_failure) {
		if (kscene_state.board_temp < recovery_temp * 1000) {
			kscene_state.is_temp_failure = false;
			set_kthr_fifo();
		}
	} else if (kscene_state.board_temp > failure_temp * 1000) {
		kscene_state.is_temp_failure = true;
		set_kthr_cfs();
		if (recovery_temp >= failure_temp) {
			recovery_temp = failure_temp - 2;
		}
	}

	kscene_state.enable = enable && !kscene_state.is_temp_failure &&
				  !game_mode && !kscene_state.is_no_limit;
	tracing_counter(TRACE_LEVEL_ALWAYS, "en", kscene_state.enable);
	if (!kscene_state.enable && is_boosting()) {
		kscene_restore_thermal_limit();
	}
	kscene_state.enable_sched = enable_sched_enhance && kscene_state.enable;
}


static enum hrtimer_restart click_timer_func(struct hrtimer *timer)
{
	tracing_counter(TRACE_LEVEL_ALWAYS, "ctmr", 2);
	// TODO: add onUp event in FWK.
	if (kscene_state.touch_state == TOUCH_STAT_DOWN) {
		kscene_state.touch_state = TOUCH_STAT_IDLE;
		tracing_counter(TRACE_LEVEL_ALWAYS, "th", kscene_state.touch_state);
		if (hrtimer_active(&frame_timer)) {
			tracing_counter(TRACE_LEVEL_DEBUG, "cancel-timer", 5);
			kscene_cancel_frame_timer();
		}
	}

	return HRTIMER_NORESTART;
}

static int kscene_init_click_hrtimer(void) {
	hrtimer_init(&click_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	click_timer.function = &click_timer_func;

	return 0;
}

static void kscene_cancel_click_timer(void) {
	tracing_counter(TRACE_LEVEL_ALWAYS, "ctmr", 0);
	ks_dbg("Cancel click timer");
	hrtimer_cancel(&click_timer);
}

static void kscene_start_click_timer(void) {
	tracing_counter(TRACE_LEVEL_ALWAYS, "ctmr", 1);
	ks_dbg("Start click timer");
	hrtimer_start(&click_timer, ns_to_ktime(CLICK_TIMER_DURATION_NS), HRTIMER_MODE_REL);
}

__attribute__((unused))
static int kscene_cmder_store(const char *buf, const struct kernel_param *param) {
	return util_str_cmd_store(buf, param);
}

static int kscene_state_get(char *buf, const struct kernel_param *param)
{
	unsigned int cnt = 0;

	cnt += scnprintf(buf, PAGE_SIZE,
		"kscene state:\n \
		enable: %d\n \
		enable_sched: %d\n \
		game_mode: %d\n \
		no_limit: %d\n \
		debug:%d\n \
		debug_pick_next: %d\n \
		debug_preempt: %d\n \
		failure_temp: %d\n \
		recovery_temp: %d\n \
		cluster_1_max_khz: %d\n \
		cluster_2_max_khz: %d\n",
		kscene_state.enable, kscene_state.enable_sched, game_mode, kscene_state.is_no_limit,
		debug, debug_pick_next, debug_preempt, failure_temp, recovery_temp,
		cluster_1_max_freq, cluster_2_max_freq);
	if (cnt < PAGE_SIZE)
		cnt += scnprintf(buf + cnt, PAGE_SIZE - cnt, "\n");

	return cnt;
}

static void kscene_update_touch(enum kscene_event_id event, int pid, int tid, int data) {
	ks_dbg("touch event: %d, pid: %d, tid: %d, data: %d", event, pid, tid, data);
	if (kscene_state.guidance != KSCENE_GUIDANCE_NORMAL) {
		kscene_state.touch_state = TOUCH_STAT_IDLE;
		kscene_cancel_click_timer();
		kscene_cancel_frame_timer();
	} else {
		switch (event) {
			case KSCENE_EVENT_TOUCH_DOWN:
				kscene_state.touch_state = TOUCH_STAT_DOWN;
				kscene_start_click_timer();
				break;
			case KSCENE_EVENT_TOUCH_SCROLL_BEGIN:
				kscene_cancel_click_timer();
				kscene_state.touch_state = TOUCH_STAT_SCROLL;
				break;
			case KSCENE_EVENT_TOUCH_SCROLL_END:
				if (kscene_state.touch_state == TOUCH_STAT_SCROLL) {
					ks_dbg("No fling...");
					kscene_state.touch_state = TOUCH_STAT_IDLE;
					tracing_counter(TRACE_LEVEL_DEBUG, "cancel-timer", 6);
					kscene_cancel_frame_timer();
				}
				break;
			case KSCENE_EVENT_TOUCH_FLING_BEGIN:
				kscene_state.touch_state = TOUCH_STAT_FLING;
				break;
			case KSCENE_EVENT_TOUCH_FLING_END:
				kscene_state.touch_state = TOUCH_STAT_IDLE;
				break;
			default:
				break;
		}
	}
	tracing_counter(TRACE_LEVEL_ALWAYS, "th", kscene_state.touch_state);
}

static void kscene_update_action(int event, int pid, int tid, int data) {
	bool need_update_enable = false;

	ks_dbg("action event: %d, pid: %d, tid: %d, data: %d", event, pid, tid, data);
	switch (event) {
		case KSCENE_EVENT_ACTION_FOCUS_PROC:
			kscene_handle_proc_focus(pid, data);
			break;
		case KSCENE_EVENT_ACTION_HOME_ANIM_BEGIN:
			tracing_counter(TRACE_LEVEL_ALWAYS, "anim", pid_home);
			if (pid_home > 0) {
				pid_focused = pid_home;
				kscene_state.guidance = KSCENE_GUIDANCE_HOME_ANIM;
			}
			break;
		case KSCENE_EVENT_ACTION_HOME_ANIM_END:
			tracing_counter(TRACE_LEVEL_ALWAYS, "anim", 0);
			kscene_state.guidance = KSCENE_GUIDANCE_NORMAL;
			ks_dbg("home anim end !!!");
			break;
		case KSCENE_EVENT_ACTION_SYSTEMUI_ANIM_BEGIN:
			tracing_counter(TRACE_LEVEL_ALWAYS, "anim-sys", pid_systemui);
			if (pid_systemui > 0) {
				pid_focused = pid_systemui;
				kscene_state.guidance = KSCENE_GUIDANCE_SYSTEMUI_ANIM;
			}
			break;
		case KSCENE_EVENT_ACTION_SYSTEMUI_ANIM_END:
			tracing_counter(TRACE_LEVEL_ALWAYS, "anim-sys", 0);
			ks_dbg("systemui anim end !!!");
			kscene_state.guidance = KSCENE_GUIDANCE_NORMAL;
			break;
		case KSCENE_CMD_SET_ENABLE:
			enable = data;
			need_update_enable = true;
			ks_dbg("set enable: %d", enable);
			break;
		case KSCENE_CMD_SET_ENABLE_SCHED_ENHANCE:
			enable_sched_enhance = data;
			need_update_enable = true;
			ks_dbg("set enable_sched_enhance: %d", enable_sched_enhance);
			break;
		case KSCENE_CMD_SET_CLUSTER_1_MAX_FREQ:
			if (data < 1000000 || data > 5000000) {
				return;
			}
			cluster_1_max_freq = data;
			if (kscene_topo.clusters > 1) {
				kscene_topo.cluster_cpus[1][KSCENE_CPU_TOPO_SET_UP_FREQ] = data;
			}
			ks_dbg("set cluster_1_max_freq : %d", cluster_1_max_freq);
			break;
		case KSCENE_CMD_SET_CLUSTER_2_MAX_FREQ:
			if (data < 1000000 || data > 5000000) {
				return;
			}
			cluster_2_max_freq = data;
			if (kscene_topo.clusters > 2) {
				kscene_topo.cluster_cpus[2][KSCENE_CPU_TOPO_SET_UP_FREQ] = data;
			}
			ks_dbg("set cluster_2_max_freq : %d", cluster_2_max_freq);
			break;
		case KSCENE_CMD_SET_FAILURE_TEMP:
			if (data < 0 || data > 60) {
				return;
			}
			failure_temp = data;
			need_update_enable = true;
			ks_dbg("set failure_temp : %d", failure_temp);
			break;
		case KSCENE_CMD_SET_RECOVERY_TEMP:
			if (data < 0 || data > 60) {
				return;
			}
			recovery_temp = data;
			need_update_enable = true;
			ks_dbg("set recovery_temp: %d", recovery_temp);
			break;
		case KSCENE_CMD_SET_DEBUG:
			debug = data;
			ks_dbg("set debug: %d", debug);
			break;
		case KSCENE_CMD_SET_DEBUG_PICK:
			debug_pick_next = data;
			ks_dbg("set debug_pick_next: %d", debug_pick_next);
			break;
		case KSCENE_CMD_SET_DEBUG_PREEMPT:
			debug_preempt = data;
			ks_dbg("set debug_preempt: %d", debug_preempt);
			break;
		default:
			break;
	}

	kscene_update_enable_state();
}

static void kscene_update_pid(enum kscene_event_id event, int pid, int tid, int data) {
	ks_dbg("pid event: %d, pid: %d, tid: %d, data: %d", event, pid, tid, data);
	switch (event) {
		case KSCENE_EVENT_PID_HOME:
			pid_home = pid;
			break;
		case KSCENE_EVENT_PID_SF:
			pid_sf = pid;
			break;
		case KSCENE_EVENT_PID_SYSTEM_SERVER:
			pid_system_server = pid;
			break;
		case KSCENE_EVENT_PID_SYSTEMUI:
			pid_systemui = pid;
			break;
		default:
			break;
	}
}

static void
kscene_handle_frame_step(enum kscene_frame_event step, int pid, int tid, int64_t vsync_id, int begin) {
	// idle, do nothing
	if (kscene_state.touch_state == TOUCH_STAT_IDLE) {
		if (is_boosting()) {
			tracing_counter(TRACE_LEVEL_DEBUG, "cancel-timer", 7);
			kscene_cancel_frame_timer();
		}
		return;
	}

	/* focus on ui thread doFrame only */
	if (tid != pid && step != KSCENE_EVENT_FRAME_RENDER_BEGIN &&
		step != KSCENE_EVENT_FRAME_RENDER_END) {
		return;
	}

	tracing_counter(TRACE_LEVEL_ALWAYS, "frm", step - 100);
	tracing_counter(TRACE_LEVEL_DEBUG, "vsync_id", (int)vsync_id);

	switch (step) {
		case KSCENE_EVENT_FRAME_STEP_0:
			// vsync_id = 0
			if (kscene_state.vsync_id == 0) {
				kscene_state.ui_frame_begin = true;
				kscene_state.last_vsync_id = kscene_state.vsync_id;
				kscene_state.vsync_id = vsync_id;
				kscene_state.last_frame_finished = kscene_state.frame_finished;
				if (kscene_state.touch_state == TOUCH_STAT_FLING && kscene_state.ux_cnt < 2) {
					kscene_start_frame_timer(FRAME_TIMER_DUR_NOW);
				} else {
					kscene_start_frame_timer(kscene_state.vsync_period_ns
							* FRAME_TIMER_DUR_MULTIP_NORMAL / FRAME_TIMER_DUR_DIV_NORMAL);
				}
			}
			break;
		case KSCENE_EVENT_FRAME_UI_BEGIN:
			// frame ui begin
			if (kscene_state.vsync_id == 0 && kscene_state.vsync_id != vsync_id
					&& kscene_state.ui_frame_begin == true) {
				kscene_state.vsync_id = vsync_id;
				kscene_state.traversal_cnt = 1;
			} else {
				kscene_state.ui_frame_begin = true;
				kscene_state.last_vsync_id = kscene_state.vsync_id;
				kscene_state.last_traversal_cnt = kscene_state.traversal_cnt;
				tracing_counter(TRACE_LEVEL_DEBUG, "last_traversal_cnt", kscene_state.last_traversal_cnt);
				kscene_state.vsync_id = vsync_id;
				kscene_state.last_frame_finished = kscene_state.frame_finished;
				kscene_start_frame_timer(kscene_state.vsync_period_ns
						* FRAME_TIMER_DUR_MULTIP_NORMAL / FRAME_TIMER_DUR_DIV_NORMAL);
			}
			break;
		case KSCENE_EVENT_FRAME_STEP_TRAVERSAL:
			if (kscene_state.vsync_id == vsync_id) {
				kscene_state.traversal_cnt = begin;
				tracing_counter(TRACE_LEVEL_DEBUG, "traversal_cnt", kscene_state.traversal_cnt);
			}
			break;
		case KSCENE_EVENT_FRAME_ON_VSYNC_LATE:
			tracing_counter(TRACE_LEVEL_INFO, "onvsync_late-vid", vsync_id);
			tracing_counter(TRACE_LEVEL_INFO, "onvsync_late-state-vid", kscene_state.vsync_id);
			if (kscene_state.vsync_id != 0 && vsync_id == 0) {
				// new frame begin
				kscene_state.ui_frame_begin = true;
				kscene_state.last_vsync_id = kscene_state.vsync_id;
				kscene_state.vsync_id = vsync_id;
				kscene_state.last_frame_finished = kscene_state.frame_finished;
				if (kscene_state.touch_state == TOUCH_STAT_FLING && kscene_state.ux_cnt < 2) {
					kscene_start_frame_timer(FRAME_TIMER_DUR_NOW);
				} else {
					kscene_start_frame_timer(kscene_state.vsync_period_ns
							* FRAME_TIMER_DUR_MULTIP_NORMAL / FRAME_TIMER_DUR_DIV_NORMAL);
				}
			}
			if (kscene_state.touch_state != TOUCH_STAT_FLING
					|| (kscene_state.touch_state == TOUCH_STAT_FLING && kscene_state.ux_cnt <=2)) {
				kscene_start_frame_timer(FRAME_TIMER_DUR_NOW);
			}
			break;
		case KSCENE_EVENT_FRAME_UI_EMPTY:
			kscene_state.frame_finished = true;
			if (kscene_state.last_frame_finished) {
				// last frame finished, all frames done.
				tracing_counter(TRACE_LEVEL_DEBUG, "cancel-timer", 1);
				kscene_cancel_frame_timer();
			}
			kscene_state.ui_frame_begin = false;
			break;
		case KSCENE_EVENT_FRAME_UI_END:
			kscene_state.ui_frame_begin = false;
			break;
		case KSCENE_EVENT_FRAME_RENDER_END:
			if (kscene_state.render_tid == 0) {
				kscene_state.render_tid = tid;
			}
			tracing_counter(TRACE_LEVEL_ALWAYS, "rt", kscene_state.render_tid);
			if (kscene_state.last_vsync_id == kscene_state.vsync_id) {
				ks_dbg("Error: same vsync id: %lld", vsync_id);
				break;
			}
			if (vsync_id == kscene_state.last_vsync_id) {
				-- kscene_state.last_traversal_cnt;
				tracing_counter(TRACE_LEVEL_DEBUG, "last_traversal_cnt", kscene_state.last_traversal_cnt);
				if (kscene_state.last_traversal_cnt <= 0) {
					kscene_state.last_frame_finished = true;
				}
			} else if (vsync_id == kscene_state.vsync_id) {
				-- kscene_state.traversal_cnt;
				tracing_counter(TRACE_LEVEL_DEBUG, "traversal_cnt", kscene_state.traversal_cnt);
				if (kscene_state.traversal_cnt <= 0) {
					kscene_state.frame_finished = true;
				}
			}

			if (kscene_state.frame_finished) {
				// current frame finished, all frames done.
				tracing_counter(TRACE_LEVEL_DEBUG, "cancel-timer", 2);
				kscene_cancel_frame_timer();
			} else {
				if (is_boosting()) {
					if (kscene_state.last_frame_finished) {
						if (!kscene_state.ui_frame_begin || hrtimer_active(&frame_timer)) {
							// no frame drawing, or timer not triggered for current frame
							kscene_restore_thermal_limit();
						} else {
							// boosting for current frame, keep it
						}
					} else {
						//last frame not finished, keep it
					}
				}
			}
			break;
		case KSCENE_EVENT_FRAME_RENDER_BEGIN:
			break;
		default:
			break;
	}
}

void kscene_notify_frame_event(enum kscene_frame_event step, int pid, int tid, int64_t vsync_id, int begin) {
	ks_dbg("ioctl frame event : %d, pid: %d, tid: %d, vsync_id: %lld, begin: %d, pid_focus=%d\n",
			step, pid, tid, vsync_id, begin, pid_focused);
	if (pid != pid_focused) {
		return;
	}
	kscene_handle_frame_step(step, pid, tid, vsync_id, begin);
}

void kscene_notify_event(unsigned int type, enum kscene_event_id event, int pid, int tid, int data) {
	ks_dbg("ioctl event: %d, pid: %d, tid: %d, data: %d\n", event, pid, tid, data);
	ks_dbg("type=%u, actiontype=%lu, touchtype=%lu, pidtype=%lu", type, KSCENE_IOCTL_ACTION, KSCENE_IOCTL_TOUCH, KSCENE_IOCTL_PID);
	switch (type) {
		case KSCENE_IOCTL_ACTION:
			kscene_update_action(event, pid, tid, data);
			break;
		case KSCENE_IOCTL_TOUCH:
			ks_dbg("mark");
			kscene_update_touch(event, pid, tid, data);
			break;
		case KSCENE_IOCTL_PID:
			kscene_update_pid(event, pid, tid, data);
			break;
		default:
			break;
	}
}

void notify_freq_limit_update(uint32_t cpu, uint32_t freq_max) {
	if (trace_level >= TRACE_LEVEL_DEBUG) {
		char buf[24];
		sprintf(buf, "ther-limit-%d", cpu);
		tracing_counter(TRACE_LEVEL_DEBUG, buf, freq_max);
	}
	ks_dbg("notify_freq_limit_update: cpu-%d, freq: %d\n", cpu, freq_max);
	update_thermal_limit_info(cpu, freq_max);
	kscene_update_enable_state();
}

void notify_board_temp_update(int32_t temp) {
	ks_dbg("board temp update: %d", temp);
	kscene_state.board_temp = temp;
	tracing_counter(TRACE_LEVEL_DEBUG, "btmp", temp);
	kscene_update_enable_state();
}

#ifdef USE_TEST_KTHREAD
int loop_kthr_func(void *data) {
	struct event_data *ed;

	while (!kthread_should_stop()) {
		spin_lock(&queue_lock);
		if (list_empty(&event_queue)) {
			spin_unlock(&queue_lock);
			wait_event_interruptible(event_wait, !list_empty(&event_queue));
			continue;
		}

		ed = list_first_entry(&event_queue, struct event_data, list);
		list_del(&ed->list);
		spin_unlock(&queue_lock);

		switch (ed->event) {
			case 1:
				break;
			case 2:
				break;
			default:
				break;
		}

		kfree(ed);
	}

	return 0;
}

__attribute__((unused))
static void submit_event_data(int event, void *data) {
	struct event_data *ed;

	ed = kmalloc(sizeof(*ed), GFP_KERNEL);
	if (!ed)
		return;

	ed->event = event;
	ed->data = data;

	spin_lock(&queue_lock);
	list_add_tail(&ed->list, &event_queue);
	spin_unlock(&queue_lock);

	wake_up_interruptible(&event_wait);
}
#endif

static void kscene_thread_main_logic(void) {
	enum kscene_action action;

	action = get_pending_action();
	tracing_counter(TRACE_LEVEL_DEBUG, STR_ACTION, action);
	ks_dbg("action = %d", action);
	switch (action) {
		case KSCENE_ACTION_INVALID:
			return;
		case KSCENE_ACTION_BOOST:
			ks_dbg("thr action: higher max freq\n");
			kscene_do_higher_max_freq();
			break;
		case KSCENE_ACTION_RESTORE:
			ks_dbg("thr action: restore\n");
			restore_thermal_limit();
			break;
		default:
			break;
	}
	set_pending_action(KSCENE_ACTION_INVALID);
}

static int kscene_thread_func(void *data) {
	ks_dbg("kscene thread start!\n");

	while (!kthread_should_stop()) {
		wait_event_interruptible(kscene_thr_wq, have_pending_action);
		ks_dbg("kscene thread wakeup!\n");

		kscene_thread_main_logic();
	}

	return 0;
}

static void kscene_wakeup_kthr(void) {
	if (kscene_logic_enabled() || is_boosting()) {
		ks_dbg("To wakeup kscene thread, have_pending_action=%d\n", have_pending_action);
		wake_up_interruptible(&kscene_thr_wq);
	}
}

static int kscene_init_kthread(void) {
	struct sched_param param = { .sched_priority = 1 };

	kscene_thr = kthread_create(kscene_thread_func, NULL, "kscene");
	if (IS_ERR(kscene_thr)) {
		pr_err("kscene_thr kthread_create failed\n");
		return -1;
	}

	sched_setscheduler_nocheck(kscene_thr, SCHED_FIFO, &param);
	wake_up_process(kscene_thr);

#ifdef USE_TEST_KTHREAD
	loop_kthr = kthread_create(loop_kthr_func, NULL, "loop_kthr");
	if (IS_ERR(loop_kthr)) {
		pr_err("loop_kthr kthread_create failed\n");
		return -1;
	}
#endif
	return 0;
}

static void kscene_exit_kthread(void) {
	kthread_stop(kscene_thr);
	ks_dbg("kscene_thr stopped");

#ifdef USE_TEST_KTHREAD
	kthread_stop(loop_kthr);
#endif
}

static void notify_vsync_period(unsigned long long vsync_period) {
	kscene_state.vsync_period_ns = vsync_period;
}

static void notify_render_end(int tgid, int render_tid, unsigned long long vsync_id) {
	if (!kscene_logic_enabled()) {
		return;
	}

	ks_dbg("%d-%d  vsync_id=%llu, current_render_tid=%d", tgid, render_tid, vsync_id, kscene_state.render_tid);
	if (tgid == pid_focused) {
		kscene_notify_frame_event(KSCENE_EVENT_FRAME_RENDER_END, tgid, render_tid,
			vsync_id==1 ? -1 : (int64_t)vsync_id, 0);
	}
}

static void notify_ux_buffer_cnt(int pid,int count, int max_buffer) {
	if (!kscene_enabled()) {
		return;
	}

	if (trace_level >= TRACE_LEVEL_DEBUG) {
		char buf[128];
		sprintf(buf, "%d-UX_buf_cnt", pid);
		tracing_counter(TRACE_LEVEL_DEBUG, buf, count);
	}

	if (pid == kscene_state.render_tid) {
		tracing_counter(TRACE_LEVEL_DEBUG, "frame-begin", kscene_state.ui_frame_begin);
		kscene_state.ux_cnt = count;
		if (kscene_logic_enabled() && kscene_state.touch_state == TOUCH_STAT_FLING) {
			if (count <= 2 && kscene_state.ui_frame_begin) {
				kscene_start_frame_timer(
					kscene_state.vsync_period_ns *
					(FRAME_TIMER_DUR_MULTIP_URGENT / FRAME_TIMER_DUR_DIV_URGENT) * (count - 1));
			} else if (count == 3 && kscene_state.last_ux_cnt == 2) {
				tracing_counter(TRACE_LEVEL_DEBUG, "cancel-timer", 3);
				kscene_cancel_frame_timer();
			}
		}
		kscene_state.last_ux_cnt = kscene_state.ux_cnt;
		kscene_state.ux_cnt = count;
	}
}

module_param_call(state, NULL, kscene_state_get, NULL, 0400);
MODULE_PARM_DESC(state, "kscene");

static int kscene_init_cpu_topology(void) {
	int cpu, cluster, last_real_cpu;
	int last_cluster_id = 0;

	kscene_topo.cpus = cpumask_weight(cpu_possible_mask);
	struct cpu_topology *cpu_topo = &cpu_topology[kscene_topo.cpus - 1];
	kscene_topo.clusters = cpu_topo->cluster_id + 1;
	ks_dbg("topo.cpus=%d, topo.clusters=%d", kscene_topo.cpus, kscene_topo.clusters);

	kscene_topo.cluster_cpus = kmalloc(
			kscene_topo.clusters * KSCENE_CPU_TOPO_ARRAY_DIM, GFP_KERNEL);
	if (!kscene_topo.cluster_cpus) {
		pr_err("kmalloc failed\n");
		return -1;
	}

	kscene_topo.cluster_cpus[last_cluster_id][KSCENE_CPU_TOPO_CLUSTER_START_IDX] = 0;
	for_each_possible_cpu(cpu) {
		last_real_cpu = cpu;
		ks_dbg("loop cpu %d", last_real_cpu);
		struct cpu_topology *cpu_topo = &cpu_topology[cpu];
		cluster = cpu_topo->cluster_id;
		ks_dbg("loop cpu %d, cluster=%d", cpu, cluster);
		if (cluster != last_cluster_id) {
			kscene_topo.cluster_cpus[last_cluster_id][KSCENE_CPU_TOPO_CLUSTER_END_IDX] = cpu - 1;
			last_cluster_id = cluster;
			kscene_topo.cluster_cpus[last_cluster_id][KSCENE_CPU_TOPO_CLUSTER_START_IDX] = cpu;
		}
	}
	kscene_topo.cluster_cpus[last_cluster_id][1] = last_real_cpu;

	for (int i = 0; i < kscene_topo.clusters; i++) {
		ks_dbg("cluster-%d, [%d, %d]", i,
				kscene_topo.cluster_cpus[i][KSCENE_CPU_TOPO_CLUSTER_START_IDX],
				kscene_topo.cluster_cpus[i][KSCENE_CPU_TOPO_CLUSTER_END_IDX]);
		uint cpu = kscene_topo.cluster_cpus[i][KSCENE_CPU_TOPO_CLUSTER_START_IDX];

		// get cpufreq policy
		struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);
		if (!policy) {
			pr_err("Failed to get policy for CPU %d\n", cpu);
			continue;
		}

		// get max frequency
		uint max_freq_khz = policy->cpuinfo.max_freq;
		ks_dbg("CPU %d max frequency: %u KHz", cpu, max_freq_khz);
		kscene_topo.cluster_cpus[i][KSCENE_CPU_TOPO_MAX_FREQ_IDX] = max_freq_khz;
		kscene_topo.cluster_cpus[i][KSCENE_CPU_TOPO_THERMAL_FREQ_IDX] = INVALID_FREQ;
		kscene_topo.cluster_cpus[i][KSCENE_CPU_TOPO_SET_UP_FREQ] = INVALID_FREQ;

		cpufreq_cpu_put(policy);
	}
	if (kscene_topo.clusters > 1) {
		kscene_topo.cluster_cpus[1][KSCENE_CPU_TOPO_SET_UP_FREQ] = cluster_1_max_freq;
	}
	if (kscene_topo.clusters > 2) {
		kscene_topo.cluster_cpus[2][KSCENE_CPU_TOPO_SET_UP_FREQ] = cluster_2_max_freq;
	}

	return 0;
}

static void kscene_free_cpu_topology(void) {
	if (kscene_topo.cluster_cpus != NULL) {
		kfree(kscene_topo.cluster_cpus);
	}
}

#define __node_2_se(node) \
	rb_entry((node), struct sched_entity, run_node)


__attribute__((unused))
static struct sched_entity *__pick_next_entity(struct sched_entity *se)
{
	struct rb_node *next = rb_next(&se->run_node);

	if (!next)
		return NULL;

	return __node_2_se(next);
}

static inline bool is_tsk_of_grp(struct task_struct *tsk, int *grp_id, const char *grp_name) {
#if IS_ENABLED(CONFIG_CGROUP_SCHED)
	struct cgroup *grp;

	rcu_read_lock();
	grp = task_cgroup(tsk, cpu_cgrp_id);
	rcu_read_unlock();

	if (*grp_id >= 0)
		return (grp->kn->id == *grp_id);
	else {
		if (strncmp(grp->kn->name, grp_name, 32) == 0) {
			*grp_id = grp->kn->id;
			return true;
		}
		return false;
	}
#else
	return false;
#endif
}

static bool is_target_group(struct sched_entity *group_se, int grp_id) {
#if IS_ENABLED(CONFIG_CGROUP_SCHED)
	struct task_group *tg;
	struct cgroup_subsys_state *css;
	struct cgroup *cgrp;

	// get task_group of group_se
	if (!group_se->my_q) return false;
	tg = group_se->my_q->tg;
	if (!tg) return false;

	// get struct cgroup by css
	css = &tg->css;
	if (!css) return false;
	cgrp = css->cgroup;
	if (!cgrp || !cgrp->kn) return false;

	return cgrp->kn->id == grp_id;
#endif
	return false;
}

__attribute__((unused))
static void mark(int line_no) {
	tracing_counter(TRACE_LEVEL_DEBUG, "line", line_no);
}

static void print_task_info(struct task_struct *tsk) {
	if (!tsk) return;
	ks_dbg("iter cfs_rq: tsk: %d-%d, %s", tsk->tgid, tsk->pid, tsk->comm);
}

#define CFS_RQ_RB_ROOT(cfs_rq) (&(cfs_rq)->tasks_timeline.rb_root)
#define ks_dbg_pick_next(fmt, ...) \
	do { if (debug_pick_next) ks_dbg(fmt, ##__VA_ARGS__); } while (0)

static void iterate_cfs_tasks(struct cfs_rq *cfs_rq)
{
	struct rb_node *node;
	struct sched_entity *se;

	for (node = rb_first(CFS_RQ_RB_ROOT(cfs_rq)); node; node = rb_next(node)) {
		se = rb_entry(node, struct sched_entity, run_node);

		if (!entity_is_task(se)) {
			struct cfs_rq *child = group_cfs_rq(se);
			if (child) iterate_cfs_tasks(child);
		} else {
			print_task_info(task_of(se));
		}
	}

	if (cfs_rq->curr && cfs_rq->curr->on_rq) {
		print_task_info(task_of(cfs_rq->curr));
	}
}

static bool traverse_cfs_rq(struct cfs_rq *cfs_rq, int *count, const pid_t tgt_pid,
		struct task_struct **tgt_tsk, int tgt_grp_id, int tgt_type) {
	struct rb_node *node;
	struct sched_entity *se;
	struct task_struct *task;

	if (debug_pick_next) {
		ks_dbg_pick_next("rq->nr_running: %d", cfs_rq->nr_running);
		iterate_cfs_tasks(cfs_rq);
	}
	for (node = rb_first_cached(&cfs_rq->tasks_timeline); node; node = rb_next(node)) {
		if (*count >= 5 || *tgt_tsk) return false;

		se = rb_entry(node, struct sched_entity, run_node);
		(*count)++;

		// if se is group_se, sheck if it's target group
		if (se->my_q) {
			ks_dbg_pick_next("enter group_se: %s, running=%d", 
					se->my_q->tg->css.cgroup->kn->name, se->my_q->nr_running);
			if (!is_target_group(se, tgt_grp_id)) {
				ks_dbg_pick_next("Skipping non-%d group, this group: %s",
						tgt_grp_id, se->my_q->tg->css.cgroup->kn->name);
				continue; // skip this group_se
			}
			if (traverse_cfs_rq(se->my_q, count, tgt_pid, tgt_tsk, tgt_grp_id, tgt_type)) {
				return true;
			}
			ks_dbg_pick_next("exit group_se: %s", se->my_q->tg->css.cgroup->kn->name);
		} else {
			// top get task_struct and match
			task = container_of(se, struct task_struct, se);
			if (kscene_state.guidance != KSCENE_GUIDANCE_NORMAL) {
				if (task->tgid == tgt_pid) goto find_tgt;
			} else {
				if (task->pid == tgt_pid) goto find_tgt;

				if ((task->android_oem_data1[KSCENE_OEM_DATA_IDX] & KSCENE_MAGIC_MASK) != KSCENE_MAGIC) goto find_none;
				if ((task->android_oem_data1[KSCENE_OEM_DATA_IDX] & KSCENE_TYPE_MASK) == KSCENE_RENDER) goto find_tgt;
				if (tgt_type == 0) goto find_none;
			}

			find_tgt:
			ks_dbg_pick_next("found task: %d-%d %s", task->tgid, task->pid, task->comm);
			*tgt_tsk = task;
			return true;

			find_none:
			ks_dbg_pick_next("not match: %d-%d", task->tgid, task->pid);
		}
	}
	return false;
}

#ifdef CONFIG_FAIR_GROUP_SCHED
/* Walk up scheduling entities hierarchy */
#define for_each_sched_entity(se) \
		for (; se; se = se->parent)
#else
#define for_each_sched_entity(se) \
		for (; se; se = NULL)
#endif

void __maybe_unused kscene_pick_next_task_fair_hook(void *nouse, struct rq *rq,
		struct task_struct **p, struct sched_entity **se,
		bool *repick, bool simple, struct task_struct *prev)
{
	int cnt = 0;
	pid_t tgt_pid = 0;
	int   tgt_type = 0;
	int tgt_grp_id = 0;

	struct task_struct *lucky_tsk = NULL;
	struct cfs_rq *cfs_rq;

	if (!kscene_state.enable_sched) {
		return;
	}

	if (rq->nr_running <= 1) {
		return;
	}

	if (kscene_state.guidance == KSCENE_GUIDANCE_NORMAL && kscene_state.touch_state == TOUCH_STAT_IDLE) {
		return;
	}

	if ( kscene_state.guidance == KSCENE_GUIDANCE_HOME_ANIM && pid_home > 0) {
		tgt_pid = pid_home;
		tgt_type = 0;
		tgt_grp_id = FG_GROUP_ID;
	} else if (kscene_state.guidance == KSCENE_GUIDANCE_SYSTEMUI_ANIM && pid_systemui > 0) {
		tgt_pid = pid_systemui;
		tgt_type = 0;
		tgt_grp_id = TOP_APP_GROUP_ID;
	} else if (kscene_state.touch_state >= TOUCH_STAT_SCROLL && pid_focused > 0) {
		tgt_pid = pid_focused;
		tgt_type = focused_type;
		tgt_grp_id = TOP_APP_GROUP_ID;
	} else {
		return;
	}

	ks_dbg_pick_next("tgt_pid=%d, *tgt_grp_id=%d", tgt_pid, tgt_grp_id);

	traverse_cfs_rq(&rq->cfs, &cnt, tgt_pid, &lucky_tsk, tgt_grp_id, tgt_type);

	if (debug_pick_next) {
		char buf[32];
		sprintf(buf, "cpu-%d_pick", rq->cpu);
		tracing_counter(TRACE_LEVEL_DEBUG, buf, lucky_tsk ? lucky_tsk->pid : -1);
		memset(buf, 0, 32);
		sprintf(buf, "cpu-%d_loop-cnt", rq->cpu);
		tracing_counter(TRACE_LEVEL_DEBUG, buf, cnt);
		ks_dbg("cpu-%d_pick=%d, loop-cnt=%d", rq->cpu, lucky_tsk ? lucky_tsk->pid : -1, cnt);
	}
	if (lucky_tsk) {
		*p = lucky_tsk;
		*se = &(*p)->se;
#ifndef CONFIG_FAIR_GROUP_SCHED
		cfs_rq = cfs_rq_of(*se);
		set_next_entity(cfs_rq, *se);
#endif
		if (simple) {
			for_each_sched_entity((*se)) {
			/*
			 * TODO If CFS_BANDWIDTH is enabled, we might pick
			 * from a throttled cfs_rq*/
				cfs_rq = cfs_rq_of(*se);
				set_next_entity(cfs_rq, *se);
			}
		}
		*repick = true;
	}
}

static bool __maybe_unused hook_binder_set_prio(pid_t from_tid) {
	if (!kscene_state.enable_sched) {
		return false;
	}

	if ((kscene_state.guidance == KSCENE_GUIDANCE_HOME_ANIM && from_tid == pid_home)
			|| (kscene_state.guidance == KSCENE_GUIDANCE_SYSTEMUI_ANIM && from_tid == pid_systemui)
			|| (kscene_state.guidance == KSCENE_GUIDANCE_NORMAL && from_tid == pid_focused)) {
		if (debug_pick_next) {
			tracing_counter(TRACE_LEVEL_DEBUG, "binder_set_prio", smp_processor_id());
		}
		return true;
	}
	return false;
}

/*
 * p: the wakeuped task
 */
static void __maybe_unused hook_check_preempt_wakeup(void *nonus, struct rq *rq,
		struct task_struct *p, bool *preempt, bool *nopreempt,
		int wake_flags, struct sched_entity *se,
		struct sched_entity *pse, int next_buddy_marked) {
	struct task_struct *curr = rq->curr;

	if (!kscene_state.enable_sched) {
		return;
	}

	if ((kscene_state.guidance == KSCENE_GUIDANCE_HOME_ANIM && curr->tgid == pid_home)
			|| (kscene_state.guidance == KSCENE_GUIDANCE_SYSTEMUI_ANIM && curr->pid == pid_systemui)) {
		*nopreempt = true;
		if (debug_preempt) {
			tracing_counter(TRACE_LEVEL_DEBUG, "preempt", rq->cpu * 100 + *nopreempt);
		}
	} else {
		if (kscene_state.touch_state != TOUCH_STAT_IDLE && curr->tgid == pid_focused) {
			ktime_t now = ktime_get();
			if (ktime_to_ns(now) - ktime_to_ns(curr->se.exec_start) < 500000 /*500us*/) {
				*nopreempt = true;
				if (debug_preempt) {
					tracing_counter(TRACE_LEVEL_DEBUG, "preempt", rq->cpu * 10 + *nopreempt);
				}
			}
		}
	}
}

static void sbe_set_cap_notify(int pid, int min_cap) {
	if (kscene_state.enable && pid == pid_focused) {
		kscene_state.sbe_min_cap = min_cap;
		tracing_counter(TRACE_LEVEL_DEBUG, "sbe_cap", min_cap);
		if (kscene_state.is_up_limit) {
			if (kscene_state.touch_state < TOUCH_STAT_SCROLL) {
				return;
			}
			if (kscene_state.ux_cnt >= 2) {
				return;
			}
			if (min_cap < KSCENE_UP_LIMIT_CAP_THRESHOLD && is_boosting()) {
				kscene_restore_thermal_limit();
			} else if (min_cap >= KSCENE_UP_LIMIT_CAP_THRESHOLD && !is_boosting()) {
				ks_dbg("wakeup kthr");
				set_pending_action(KSCENE_ACTION_BOOST);
				kscene_wakeup_kthr();
			}
		}
	}
}

__attribute__((unused))
static void hook_trace_android_rvh_update_rt_rq_load_avg(void *unused_data,
		u64 now, struct rq *rq, struct task_struct *tsk, int running) {
	if (tsk->pid != pid_focused) {
		return ;
	}
	kscene_trace_mark_u64("now", now);
	kscene_trace_mark_u64("ktime_now", now);
	kscene_trace_mark_u64("pelt_rq_now", rq_clock_pelt(rq));
	kscene_trace_mark_u64("pelt_task_rq_now", rq_clock_pelt(task_rq(tsk)));
	tracing_counter(TRACE_LEVEL_DEBUG, "update_load_running:", running);

	update_rt_rq_load_avg_copy(ktime_get_ns(), &kscene_state.top_avg, running);

	tracing_counter(TRACE_LEVEL_DEBUG, "util-sum", kscene_state.top_avg.util_sum);
	tracing_counter(TRACE_LEVEL_DEBUG, "util-avg", kscene_state.top_avg.util_avg);
	if (running) {
		tracing_counter(TRACE_LEVEL_DEBUG, "update_rt_rq_load_avg 1", tsk->pid);
	} else {
		tracing_counter(TRACE_LEVEL_DEBUG, "update_rt_rq_load_avg 0", tsk->pid);
	}
	tracing_counter(TRACE_LEVEL_DEBUG, "cfs-rq-avg", task_rq(tsk)->cfs.avg.util_avg);
	tracing_counter(TRACE_LEVEL_DEBUG, "rt-avg", task_rq(tsk)->avg_rt.util_avg);
}

static void __maybe_unused hook_trace_android_vh_set_task_comm(void *unused_data,
		struct task_struct *tsk)  {
	tsk->android_oem_data1[KSCENE_OEM_DATA_IDX] = 0;
	for (int i = 0; i < sizeof(THREAD_NAME)/sizeof(THREAD_NAME[0]); ++i) {
		if (strcmp(THREAD_NAME[i].comm, tsk->comm) == 0) {
			tsk->android_oem_data1[KSCENE_OEM_DATA_IDX] = KSCENE_MAGIC | THREAD_NAME[i].type;
		}
	}
}

static int kscene_set_and_update_enable(const char* val, const struct kernel_param *kp)
{
	int ret;
	ret = param_set_int(val, kp);
	kscene_update_enable_state();
	return ret;
}

static const struct kernel_param_ops param_ops_set_enable = {
	.set = kscene_set_and_update_enable,
	.get = param_get_int,
};

static int kscene_set_up_freq(const char* val, const struct kernel_param *kp)
{
	int ret;
	ret = param_set_int(val, kp);
	if (kscene_topo.clusters > 2) {
		kscene_topo.cluster_cpus[1][KSCENE_CPU_TOPO_SET_UP_FREQ] = cluster_1_max_freq;
		kscene_topo.cluster_cpus[2][KSCENE_CPU_TOPO_SET_UP_FREQ] = cluster_2_max_freq;
	}
	return ret;
}

static const struct kernel_param_ops param_ops_set_up_freq = {
	.set = kscene_set_up_freq,
	.get = param_get_int,
};

module_param(debug, int , 0660);
module_param(trace_level, int , 0660);
module_param(debug_pick_next, int, 0660);
module_param(debug_preempt, int, 0660);
module_param_cb(enable, &param_ops_set_enable, &enable, 0644);
module_param_cb(enable_sched_enhance, &param_ops_set_enable, &enable_sched_enhance, 0644);
module_param_cb(game_mode, &param_ops_set_enable, &game_mode, 0664);
module_param_cb(failure_temp, &param_ops_set_enable, &failure_temp, 0644);
module_param_cb(recovery_temp, &param_ops_set_enable, &recovery_temp, 0644);
module_param_cb(cluster_1_max_freq , &param_ops_set_up_freq, &cluster_1_max_freq, 0660);
module_param_cb(cluster_2_max_freq , &param_ops_set_up_freq, &cluster_2_max_freq, 0660);

static int __init kscene_init(void) {
	pr_debug("kscene init!\n");

	if (kscene_init_ioctl()) {
		pr_err("kscene_init_ioctl failed\n");
		return -1;
	}

	if (kscene_init_cpu_topology()) {
		pr_err("kscene_init_cpu_topology failed\n");
		return -1;
	}

	if (kscene_init_frame_hrtimer()) {
		pr_err("kscene_init_frame_hrtimer failed\n");
		return -1;
	}

	if (kscene_init_click_hrtimer()) {
		pr_err("kscene_init_click_hrtimer failed\n");
		return -1;
	}

	if (kscene_init_kthread()) {
		pr_err("kscene_init_kthread failed\n");
		return -1;
	}

	//register notify funcs
	register_freq_limit_update_notify_func(notify_freq_limit_update);
	register_board_temp_update_notify_func(notify_board_temp_update);
	register_ux_buffer_cnt_notify_func(notify_ux_buffer_cnt);
	register_vsync_period_notify_func(notify_vsync_period);
	register_notify_kscene_set_cap_func(sbe_set_cap_notify);
	register_render_end_notify_func(notify_render_end);
#if ENABLE_KSCENE_SCHED_ENHANCE
	register_pick_next_task_fair_hook_func(kscene_pick_next_task_fair_hook);
	register_kscene_check_preempt_wakeup_func(hook_check_preempt_wakeup);
	register_kscene_binder_prio_hook_func(hook_binder_set_prio);
	register_trace_android_vh_set_task_comm(hook_trace_android_vh_set_task_comm, NULL);
#endif

	#if 0
	if (register_trace_android_rvh_update_rt_rq_load_avg(hook_trace_android_rvh_update_rt_rq_load_avg, NULL)) {
		pr_err("register_trace_android_rvh_update_rt_rq_load_avg failed\n");
		return -1;
	}
	#endif

	kscene_update_enable_state();

	return 0;
}

static void __exit kscene_exit(void) {
	ks_dbg("Module exit!!");

	kscene_exit_ioctl();

	kscene_cancel_frame_timer();
	kscene_cancel_click_timer();

	kscene_exit_kthread();

	unregister_freq_limit_update_notify_func();
	unregister_board_temp_update_notify_func();
	unregister_ux_buffer_cnt_notify_func();
	unregister_vsync_period_notify_func();
	unregister_notify_kscene_set_cap_func();
	unregister_render_end_notify_func();
#if ENABLE_KSCENE_SCHED_ENHANCE
	unregister_pick_next_task_fair_hook_func();
	unregister_kscene_binder_prio_hook_func();
	unregister_kscene_check_preempt_wakeup_func();
	unregister_trace_android_vh_set_task_comm(hook_trace_android_vh_set_task_comm, NULL);
#endif

	kscene_free_cpu_topology();

	ks_dbg(KERN_INFO "kscene exit!\n");
}

module_init(kscene_init);
module_exit(kscene_exit);
MODULE_AUTHOR("chenxiong1");
MODULE_LICENSE("GPL");
