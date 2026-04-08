#ifndef _KSCENE_H
#define _KSCENE_H
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/atomic.h>
#include <linux/export.h>
#include <linux/gfp_types.h>
#include <linux/kstrtox.h>
#include <linux/kthread.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/sched/cputime.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <trace/hooks/sched.h>
#include <uapi/linux/sched/types.h>
#include "kscene_atrace.h"
#include "../../../../kernel-6.6/kernel/sched/sched.h"
#include "../../../../kernel-6.6/kernel/sched/pelt.h"

#include "kscene_ioctl.h"


// --- macros
// function switch
#define ENABLE_KSCENE_RT_UTIL 0

#define ENABLE_KSCENE_SCHED_ENHANCE 0

extern int debug;
#define ks_dbg(fmt, ...) \
	do { \
        if (debug) \
            printk("%s[%d]: KSCENE " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define TRACE_FAKE_PID 99999
#define TRACE_LEVEL_DEBUG		3
#define TRACE_LEVEL_INFO		2
#define TRACE_LEVEL_ALWAYS		1
extern int trace_level;
__attribute__((unused))
static inline void tracing_counter(int level, const char *s, int val) {
    if (level > trace_level) {
        return;
    }
	char buf[256] = {0};

	snprintf(buf, 256, "C|%d|%s|%d\n", TRACE_FAKE_PID, s, val);

	trace_puts(buf);
}

#define INVALID_FREQ 0

// pelt macros
#define LOAD_AVG_PERIOD     			32
#define KSCENE_DELTA_UP_MULTIPLIER      (4)
#define KSCENE_DELTA_DOWN_MULTIPLIER    32

#define CLICK_TIMER_DURATION_NS             300000000 // 300ms
#define FRAME_TIMER_DUR_NOW						10

// veyns_period_ns * TIMER_DUR_MULTIP_NORMAL / TIMER_DUR_DIV_NORMAL
#define FRAME_TIMER_DUR_MULTIP_NORMAL			3
#define FRAME_TIMER_DUR_DIV_NORMAL				4

#define FRAME_TIMER_DUR_MULTIP_URGENT			1
#define FRAME_TIMER_DUR_DIV_URGENT				3

#define KSCENE_UP_LIMIT_CAP_THRESHOLD           600

#define STR_HOME_COMM       "com.miui.home"
#define STR_SYSTEMUI_COMM   "ndroid.systemui"

#define STR_FG_GRP_NAME     "foreground"
#define STR_TA_GRP_NAME     "top-app"

// --- extern funxtions
// thermal freq limit hook
extern void register_freq_limit_update_notify_func(void (*)(uint32_t, uint32_t));
extern void unregister_freq_limit_update_notify_func(void);
extern void register_board_temp_update_notify_func(void (*)(int32_t));
extern void unregister_board_temp_update_notify_func(void);
extern void thermal_set_freq_max(unsigned int cpu, unsigned int max_freq);

// fpsgo ux buffer count hook
extern void register_ux_buffer_cnt_notify_func(void (*func)(int pid,int count, int max_buffer));
extern void unregister_ux_buffer_cnt_notify_func(void);

extern void register_vsync_period_notify_func(void (*func)(unsigned long long period_ns));
extern void unregister_vsync_period_notify_func(void);

extern void register_render_end_notify_func(
		void (*func)(int tgid, int render_tid, unsigned long long vsync_id));
extern void unregister_render_end_notify_func(void);

extern void register_pick_next_task_fair_hook_func(void (*func)(void *nouse, struct rq *rq,
		struct task_struct **p, struct sched_entity **se,
		bool *repick, bool simple, struct task_struct *prev));
extern void unregister_pick_next_task_fair_hook_func(void);
extern void register_kscene_check_preempt_wakeup_func (
        void (*check_preempt_wakeup_hook)(void *nonus, struct rq *rq,
		struct task_struct *p, bool *preempt, bool *nopreempt,
		int wake_flags, struct sched_entity *se,
		struct sched_entity *pse, int next_buddy_marked));
extern void unregister_kscene_check_preempt_wakeup_func (void);
extern void register_kscene_binder_prio_hook_func(bool (*func)(pid_t));
extern void unregister_kscene_binder_prio_hook_func(void);

extern void register_notify_kscene_set_cap_func(void (*func)(int pid, int min_cap));
extern void unregister_notify_kscene_set_cap_func(void);
// --- enum
enum kscene_cpu_topo_enum {
	KSCENE_CPU_TOPO_CLUSTER_START_IDX = 0,
	KSCENE_CPU_TOPO_CLUSTER_END_IDX = 1,
	KSCENE_CPU_TOPO_MAX_FREQ_IDX = 2,
	KSCENE_CPU_TOPO_THERMAL_FREQ_IDX = 3,
	KSCENE_CPU_TOPO_SET_UP_FREQ = 4,
	KSCENE_CPU_TOPO_ARRAY_DIM = 5, //[cluster_cpu_start][cluster_cpu_end][max_freq][thermal_freq]
};

enum kscene_gaidance {
    KSCENE_GUIDANCE_NORMAL = 0,
    KSCENE_GUIDANCE_HOME_ANIM = 1,
    KSCENE_GUIDANCE_SYSTEMUI_ANIM = 2,
};

enum kscene_frame_event {
	// frame
	KSCENE_EVENT_FRAME_BEGIN = 100,
	KSCENE_EVENT_FRAME_UI_BEGIN,
	KSCENE_EVENT_FRAME_UI_END,
	KSCENE_EVENT_FRAME_UI_EMPTY,
	KSCENE_EVENT_FRAME_RENDER_BEGIN,
	KSCENE_EVENT_FRAME_RENDER_END,

	KSCENE_EVENT_FRAME_STEP_0,
	KSCENE_EVENT_FRAME_ON_VSYNC_LATE,
	KSCENE_EVENT_FRAME_STEP_INPUT,
	KSCENE_EVENT_FRAME_STEP_ANIMATION,
	KSCENE_EVENT_FRAME_STEP_TRAVERSAL,
	KSCENE_EVENT_FRAME_STEP_RENDER,
	KSCENE_EVENT_FRAME_STEP_END,
};

enum kscene_event_id {
	// touch
	KSCENE_EVENT_TOUCH_DOWN = 0,
	KSCENE_EVENT_TOUCH_UP,
	KSCENE_EVENT_TOUCH_SCROLL_BEGIN,
	KSCENE_EVENT_TOUCH_SCROLL_END,
	KSCENE_EVENT_TOUCH_FLING_BEGIN,
	KSCENE_EVENT_TOUCH_FLING_END,

	// action
	KSCENE_EVENT_ACTION_FOCUS_PROC = 20,
	KSCENE_EVENT_ACTION_HOME_ANIM_BEGIN,
	KSCENE_EVENT_ACTION_HOME_ANIM_END,
	KSCENE_EVENT_ACTION_SYSTEMUI_ANIM_BEGIN,
	KSCENE_EVENT_ACTION_SYSTEMUI_ANIM_END,

	// pid
	KSCENE_EVENT_PID_HOME = 40,
	KSCENE_EVENT_PID_SF,
	KSCENE_EVENT_PID_SYSTEMUI,
	KSCENE_EVENT_PID_SYSTEM_SERVER,

	// other
	KSCENE_EVENT_MAX,
};

enum kscene_data_id {
    KSCENE_DATA_NONE = 0,
    KSCENE_DATA_FOCUS_WEBVIEW ,
    KSCENE_DATA_FOCUS_APPBRAND,
    KSCENE_DATA_FOCUS_FLUTTER,
};

enum kscene_touch_state {
    TOUCH_STAT_IDLE = 0,
    TOUCH_STAT_DOWN,
    TOUCH_STAT_UP,
    TOUCH_STAT_SCROLL,
    TOUCH_STAT_FLING,
};

enum kscene_cmd {
    KSCENE_CMD_SET_ENABLE = 0x900100,
    KSCENE_CMD_SET_ENABLE_SCHED_ENHANCE = 0x900101,
    KSCENE_CMD_SET_CLUSTER_1_MAX_FREQ = 0x900200,
    KSCENE_CMD_SET_CLUSTER_2_MAX_FREQ = 0x900201,
    KSCENE_CMD_SET_FAILURE_TEMP = 0x900300,
    KSCENE_CMD_SET_RECOVERY_TEMP = 0x900301,
    KSCENE_CMD_SET_DEBUG = 0x900900,
    KSCENE_CMD_SET_DEBUG_PICK = 0x900901,
    KSCENE_CMD_SET_DEBUG_PREEMPT = 0x900902,
};

struct key_val_pair{
	char* key;
	char* value;
};


void kscene_notify_frame_event(enum kscene_frame_event step, int pid, int tid, int64_t vsync_id, int begin);
void kscene_notify_event(unsigned int type, enum kscene_event_id event, int pid, int tid, int data);

int util_str_cmd_store(const char *buf, const struct kernel_param *param);
int update_rt_rq_load_avg_copy(u64 now, struct sched_avg *sa, int running);

bool kscene_enabled(void);
bool kscene_logic_enabled(void);


#endif