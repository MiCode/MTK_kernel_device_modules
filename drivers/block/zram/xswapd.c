// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright(c) 2024 X-Ring technologies Inc.
 */

#define pr_fmt(fmt) "xswapd: " fmt

#include <linux/cpu.h>
#include <linux/file.h>
#include <linux/freezer.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/memory.h>
#include <linux/nodemask.h>
#include <linux/swap.h>

#include <trace/hooks/mm.h>
#include <trace/hooks/vmscan.h>

#include <uapi/linux/sched/types.h>

#include "internal.h"
#include "mctrl.h"
#include "xswapd.h"
#include "zgroup.h"

static struct xswapd_ctrl xctrl;

DEFINE_PER_CPU(struct xswapd_event_state, xswapd_event_states);

static inline void __xswapd_count_event(enum xswapd_event_item item)
{
	raw_cpu_inc(xswapd_event_states.event[item]);
}

static inline void xswapd_count_event(enum xswapd_event_item item)
{
	this_cpu_inc(xswapd_event_states.event[item]);
}

static inline void __xswapd_count_events(enum xswapd_event_item item,
					 long delta)
{
	raw_cpu_add(xswapd_event_states.event[item], delta);
}

static inline void xswapd_count_events(enum xswapd_event_item item, long delta)
{
	this_cpu_add(xswapd_event_states.event[item], delta);
}

static void xswapd_get_events(unsigned long *ret)
{
	int cpu;
	int i;

	cpus_read_lock();

	memset(ret, 0, NR_XSWAPD_ITEMS * sizeof(unsigned long));

	for_each_online_cpu(cpu) {
		struct xswapd_event_state *this = &per_cpu(xswapd_event_states,
							   cpu);

		for (i = 0; i < NR_XSWAPD_ITEMS; i++)
			ret[i] += this->event[i];
	}

	cpus_read_unlock();
}

#define XCTRL_GET_PARAM(name)			\
static inline unsigned int get_##name(void)	\
{						\
	return atomic_read(&xctrl.name);	\
}

XCTRL_GET_PARAM(mem_press_watermark)
XCTRL_GET_PARAM(mem_high_watermark)
XCTRL_GET_PARAM(mem_low_watermark)
XCTRL_GET_PARAM(zram_press_watermark)
XCTRL_GET_PARAM(reclaim_size)

XCTRL_GET_PARAM(wake_interval)
XCTRL_GET_PARAM(dry_run_wake_interval)
XCTRL_GET_PARAM(dry_run_reclaim_nr)

XCTRL_GET_PARAM(memcg_shrink_interval)
XCTRL_GET_PARAM(shrink_interval)
XCTRL_GET_PARAM(shrink_nr)

XCTRL_GET_PARAM(fault_interval)
XCTRL_GET_PARAM(refault_ratio)

static bool xswapd_stop_reclaim(void)
{
	if (xctrl.stop_swap & BIT(XSWAPD_STOP_RECLAIM)) {
		xctrl.stop_swap &= ~BIT(XSWAPD_STOP_RECLAIM);
		return true;
	} else {
		return false;
	}
}

bool xswapd_stop_wb(void)
{
	if (xctrl.stop_swap & BIT(XSWAPD_STOP_WB)) {
		xctrl.stop_swap &= ~BIT(XSWAPD_STOP_WB);
		return true;
	} else {
		return false;
	}
}

static inline bool is_xswapd_enable(void)
{
	return xctrl.enable;
}

static inline bool is_xswapd_umrenable(void)
{
	return xctrl.umrenable;
}

static inline bool is_wb_limit(void)
{
	return atomic64_read(&xctrl.wb_limit) <= 0;
}

static inline void update_wb_limit(unsigned long sz_wb)
{
	atomic64_sub(sz_wb, &xctrl.wb_limit);
}

static unsigned int get_swap_total(void)
{
	struct sysinfo i;

	si_swapinfo(&i);

	/* page to MB */
	return i.totalswap >> PAGES_PER_MB_SHIFT;
}

static inline unsigned int get_mem_available(void)
{
	/* page to MB */
	return si_mem_available() >> PAGES_PER_MB_SHIFT;
}

static inline bool mem_press_watermark_ok(void)
{
	return get_mem_available() >= get_mem_press_watermark();
}

static inline bool mem_high_watermark_ok(void)
{
	return get_mem_available() >= get_mem_high_watermark();
}

static inline bool mem_low_watermark_ok(void)
{
	return get_mem_available() >= get_mem_low_watermark();
}

static inline bool zram_press_watermark_ok(void)
{
	unsigned int high = get_mem_high_watermark();
	unsigned int avail = get_mem_available();
	unsigned int zram_use, zram_total;

	if (avail >= high)
		return true;

	zram_use = zgroup_get_memcg_stat(NULL, ZGROUP_OBJ_PAGES)
		   >> PAGES_PER_MB_SHIFT;
	zram_total = get_swap_total();

	/* zram_use + mem_need / compress_ratio <= zram_total * zram_wm */
	return zram_use + (high - avail) * 3 <=
	       zram_total * get_zram_press_watermark() / 100;
}

static bool xswapd_refault_ok(struct xswapd_data *xdat)
{
	unsigned long cur_fault, can_fault, cur_time;

	cur_time = jiffies_to_msecs(jiffies - xdat->fault_time) ?: 1;
	cur_fault = zgroup_get_memcg_stat(NULL, ZGROUP_OBJ_FAULT);
	can_fault = cur_time * get_refault_ratio() / 1000;

	if (cur_fault - xdat->last_fault >= can_fault)
		return false;

	return true;
}

static inline bool is_memcg_reclaimable(struct memcg_ctrl *mctrl)
{
	if (mctrl->score <= 100)
		return false;
	return true;
}

static unsigned long memcg_anon_pages(pg_data_t *pgdat, struct mem_cgroup *memcg)
{
	struct lruvec *lruvec;

	if (!memcg)
		return 0;

	lruvec = mem_cgroup_lruvec(memcg, pgdat);

	return lruvec_page_state(lruvec, NR_ACTIVE_ANON) +
	       lruvec_page_state(lruvec, NR_INACTIVE_ANON);
}

static unsigned long xswapd_shrink_anon(pg_data_t *pgdat,
					unsigned long nr_to_reclaim,
					unsigned int priority)
{
	struct mem_cgroup *memcg = NULL;
	unsigned long nr_reclaimed = 0;
	unsigned long shrink_time, shrink_interval, max_shrink_interval;
	unsigned int reclaim_op = MEMCG_RECLAIM_MAY_SWAP |
				  MEMCG_RECLAIM_PROACTIVE;

	shrink_time = jiffies;
	max_shrink_interval = msecs_to_jiffies(get_memcg_shrink_interval());

	while ((memcg = get_next_memcg(memcg, false))) {
		unsigned long nr_anon, nr_comp, nr_total;
		unsigned long nr_wb = 0;
		unsigned long memcg_can_reclaim, memcg_to_reclaim;
		unsigned long cur_fault, can_fault;
		struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);

		if (mem_high_watermark_ok()) {
			get_next_memcg_break(memcg);
			break;
		}

		if(!is_memcg_reclaimable(mctrl))
			continue;

		/*
		 * HACK: nr_anon calculates the current node, but nr_comp
		 * calculates all nodes, we haven't considered the impact
		 * of numa for now.
		 */
		nr_anon = memcg_anon_pages(pgdat, memcg);
		nr_comp = zgroup_get_memcg_stat(memcg, ZGROUP_OBJ_PAGES);
#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
		nr_wb = zgroup_get_memcg_stat(memcg, ZGROUP_WB_PAGES);
#endif
		nr_total = nr_anon + nr_comp + nr_wb;

		cur_fault = zgroup_get_memcg_stat(memcg, ZGROUP_OBJ_FAULT);
		can_fault = nr_total * mctrl->refault_ratio / 100;

		if (cur_fault - mctrl->last_fault >= can_fault) {
			pr_debug("[%s] fault skip %lu %lu %lu %u/%lu\n",
				 mctrl->name,
				 cur_fault, mctrl->last_fault, can_fault,
				 mctrl->refault_ratio, nr_total);
			xswapd_count_event(XSWAPD_MEMCG_REFAULT_SKIP);
			continue;
		}

		/*
		 * HACK: Currently we calculate the compression ratio of all
		 * zrams, because there is usually only one device.
		 */
		memcg_can_reclaim = nr_total * mctrl->comp_ratio / 100;
		memcg_to_reclaim = memcg_can_reclaim > nr_comp + nr_wb ?
				   memcg_can_reclaim - nr_comp - nr_wb : 0;

		memcg_to_reclaim >>= priority;
		if (!memcg_to_reclaim) {
			pr_debug("[%s] cmp skip %lu %lu %lu %u\n",
				 mctrl->name,
				 nr_anon, nr_comp, nr_wb, mctrl->comp_ratio);
			xswapd_count_event(XSWAPD_MEMCG_CMP_SKIP);
			continue;
		}

		xswapd_count_event(XSWAPD_SHRINK_ANON);
		nr_reclaimed += try_to_free_mem_cgroup_pages(memcg,
							     memcg_to_reclaim,
							     GFP_KERNEL,
							     reclaim_op);

		pr_debug("[%s] reclaim %lu %lu/%lu mem %lu %lu %lu %d fault %lu %lu %d",
			 mctrl->name,
			 memcg_to_reclaim, nr_reclaimed, nr_to_reclaim,
			 nr_anon, nr_comp, nr_wb, mctrl->comp_ratio,
			 cur_fault, mctrl->last_fault,
			 mctrl->refault_ratio);

		if (nr_reclaimed >= nr_to_reclaim) {
			get_next_memcg_break(memcg);
			break;
		}

		shrink_interval = jiffies - shrink_time;
		if (shrink_interval > max_shrink_interval) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(shrink_interval);
			shrink_time = jiffies;
		}
	}

	return nr_reclaimed;
}

static unsigned long count_sz_to_reclaim(void)
{
	unsigned long mem_high_wm, mem_avail;

	mem_high_wm = get_mem_high_watermark();
	mem_avail = get_mem_available();

	if (mem_avail >= mem_high_wm)
		return 0;

	return min(mem_high_wm - mem_avail, get_reclaim_size());
}

static inline unsigned long count_nr_to_reclaim(void)
{
	return count_sz_to_reclaim() << PAGES_PER_MB_SHIFT;
}

/* TODO: add perf metrics */
static void xswapd_shrink_node(pg_data_t *pgdat)
{
	unsigned long nr_to_reclaim;
	unsigned long nr_reclaimed = 0;
	unsigned int priority = DEF_PRIORITY / 2;
	struct xswapd_data *xdat = PGDAT_OEM_DATA(pgdat);

	do {
		if (mem_high_watermark_ok())
			break;

		nr_to_reclaim = count_nr_to_reclaim();
		if (!nr_to_reclaim)
			break;

		nr_reclaimed += xswapd_shrink_anon(pgdat, nr_to_reclaim,
						   priority);

		pr_debug("reclaim %lu %lu priority %d",
			 nr_reclaimed, nr_to_reclaim, priority);

		if (try_to_freeze() || kthread_should_stop())
			break;
	} while (--priority >= 1);

	xdat->shrink_nr += nr_reclaimed;
	xswapd_count_event(XSWAPD_SHRINK_NODE);
	xswapd_count_events(XSWAPD_RECLAIMED, nr_reclaimed);

	if (nr_reclaimed < get_dry_run_reclaim_nr()) {
		xswapd_count_event(XSWAPD_DRY_RUN);
		pr_debug("dry_run %d wake_inter %u reclaimed %lu",
			 xdat->dry_run, xdat->wake_interval, nr_reclaimed);

		if (xdat->dry_run)
			xdat->wake_interval = min(xdat->wake_interval * 2,
					      get_wake_interval());
		else
			xdat->wake_interval = get_dry_run_wake_interval();
		xdat->dry_run = true;
	} else {
		xdat->wake_interval = 0;
		xdat->dry_run = false;
	}
}

#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK

static void xswapd_wb(void)
{
	struct mem_cgroup *memcg = NULL;
	unsigned long sz_wb_total = 0;
	unsigned long sz_to_wb_total;

	/* MB to B */
	sz_to_wb_total = count_sz_to_reclaim() << 20;
	if (sz_to_wb_total == 0)
		return;

	if (is_wb_limit())
		return;

	while ((memcg = get_next_memcg(memcg, false))) {
		unsigned long sz_comp, sz_wb, sz_total, sz_can_wb, sz_to_wb;
		unsigned long io_wb = 0;
		struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);

		if (!is_memcg_reclaimable(mctrl))
		continue;

		sz_comp = zgroup_get_memcg_stat(memcg, ZGROUP_OBJ_SIZE);
		sz_wb = zgroup_get_memcg_stat(memcg, ZGROUP_WB_SIZE);
		sz_total = sz_comp + sz_wb;

		sz_can_wb = sz_total * mctrl->wb_ratio / 100;
		sz_to_wb = sz_can_wb > sz_wb ? sz_can_wb - sz_wb : 0;
		if (!sz_to_wb) {
			xswapd_count_event(XSWAPD_MEMCG_WB_SKIP);
			continue;
		}

		sz_wb_total += zgroup_memcg_wb(memcg, sz_to_wb, &io_wb, false);
		//pr_err("zhang writeback sz_wb_total is %lu,id is %d\n",sz_wb_total,memcg->id.id);
        update_wb_limit(io_wb);
		if (sz_wb_total >= sz_to_wb_total || is_wb_limit()) {
			get_next_memcg_break(memcg);
			break;
		}
	}

	xswapd_count_event(XSWAPD_SHRINK_WB);
	xswapd_count_events(XSWAPD_WRITEBACK, sz_wb_total);
	pr_debug("writeback %lu expect %lu", sz_wb_total, sz_to_wb_total);
}
#else
static inline void xswapd_wb(void) {}
#endif

static void xswapd_pressure_signal(enum xswapd_pressure_state state)
{
	if (!xctrl.press_eventfd[state])
		return;

	eventfd_signal(xctrl.press_eventfd[state], 1);
	pr_debug("mm press %d ", state);
}

static int xswapd(void *p)
{
	struct task_struct *tsk = current;
	pg_data_t *pgdat = (pg_data_t *)p;
	struct xswapd_data *xdat = PGDAT_OEM_DATA(pgdat);
	const struct cpumask *cpumask = cpumask_of_node(pgdat->node_id);

	if (!cpumask_empty(cpumask))
		set_cpus_allowed_ptr(tsk, cpumask);

	set_freezable();

	for ( ; ; ) {
		wait_event_freezable(xdat->wait, atomic_read(&xdat->wait_flag));
		atomic_set(&xdat->wait_flag, 0);
		xdat->wake_time = jiffies;

		if (kthread_should_stop())
			break;

		xswapd_count_event(XSWAPD_WAKEUP);

		if (xswapd_refault_ok(xdat))
			xswapd_shrink_node(pgdat);
		else
			xswapd_count_event(XSWAPD_REFAULT_SKIP);

		if (!zram_press_watermark_ok())
			xswapd_wb();

		if (!mem_press_watermark_ok()) {
			if (!zram_press_watermark_ok()) {
				xswapd_pressure_signal(XSWAPD_PS_HIGH);
				xswapd_count_event(XSWAPD_HIGH_PRESS);
			} else {
				xswapd_pressure_signal(XSWAPD_PS_LOW);
				xswapd_count_event(XSWAPD_LOW_PRESS);
			}
		} else {
			xswapd_pressure_signal(XSWAPD_PS_MIN);
			xswapd_count_event(XSWAPD_MIN_PRESS);
		}
	}

	return 0;
}

static void wakeup_xswapd(pg_data_t *pgdat)
{
	unsigned int wake_interval, shrink_interval;
	struct xswapd_data *xdat = PGDAT_OEM_DATA(pgdat);

	if (!xdat || !xdat->xswapd)
		return;

	/* No memory barrier is needed since we are just trying to wake up */
	if (!waitqueue_active(&xdat->wait))
		return;

	wake_interval = jiffies_to_msecs(jiffies - xdat->wake_time);
	if (wake_interval < xdat->wake_interval) {
		xswapd_count_event(XSWAPD_INTERVAL_SKIP);
		return;
	}

	if (mem_low_watermark_ok()) {
		xswapd_count_event(XSWAPD_LOW_MEM_SKIP);
		return;
	}

	shrink_interval = jiffies_to_msecs(jiffies - xdat->shrink_time);
	if (shrink_interval < get_shrink_interval()) {
		if (xdat->shrink_nr >= get_shrink_nr()) {
			xswapd_count_event(XSWAPD_SHRINK_SKIP);
			return;
		}
	} else {
		xdat->shrink_time = jiffies;
		xdat->shrink_nr = 0;
	}

	atomic_set(&xdat->wait_flag, 1);
	wake_up_interruptible(&xdat->wait);
}

void wakeup_xswapds(void)
{
	pg_data_t *pgdat = NULL;
	int nid;

	if (!is_xswapd_enable())
		return;

	for_each_online_node(nid) {
		pgdat = NODE_DATA(nid);
		wakeup_xswapd(pgdat);
	}
}
EXPORT_SYMBOL(wakeup_xswapds);

static void xswapd_update_refault_dwork(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct xswapd_data *xdat = container_of(dwork, struct xswapd_data, dwork);
	struct mem_cgroup *memcg = NULL;

	if (!is_xswapd_enable()) {
		schedule_delayed_work(&xdat->dwork,
			msecs_to_jiffies(get_fault_interval()));
		return;
	}

	while ((memcg = get_next_memcg(memcg, false))) {
		struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);

		mctrl->last_fault = zgroup_get_memcg_stat(memcg,
							  ZGROUP_OBJ_FAULT);
	}

	xdat->last_fault = zgroup_get_memcg_stat(NULL, ZGROUP_OBJ_FAULT);
	xdat->fault_time = jiffies;
	xswapd_count_event(XSWAPD_UPDATE_REFAULT);

	schedule_delayed_work(&xdat->dwork,
			msecs_to_jiffies(get_fault_interval()));
}

static int run_xswapd(int nid)
{
	pg_data_t *pgdat = NODE_DATA(nid);
	struct xswapd_data *xdat = PGDAT_OEM_DATA(pgdat);
	struct sched_param param = {
		.sched_priority = MAX_PRIO - 5,
	};
	struct task_struct *xswapd_data;

	init_waitqueue_head(&xdat->wait);
	atomic_set(&xdat->wait_flag, 0);
	xswapd_data = kthread_create(xswapd, pgdat, "xswapd%d", nid);
	if (IS_ERR(xswapd_data))
		return PTR_ERR(xswapd_data);
	xdat->xswapd = xswapd_data;
	INIT_DELAYED_WORK(&xdat->dwork, xswapd_update_refault_dwork);
	schedule_delayed_work(&xdat->dwork,
			msecs_to_jiffies(get_fault_interval()));

	sched_setscheduler_nocheck(xdat->xswapd, SCHED_NORMAL, &param);
	set_user_nice(xdat->xswapd, PRIO_TO_NICE(param.sched_priority));
	wake_up_process(xdat->xswapd);

	return 0;
}

static int run_xswapds(void)
{
	int nid, ret;

	for_each_node_state(nid, N_MEMORY) {
		ret = run_xswapd(nid);
		if (ret) {
			pr_err("run xswapd fail: %d", ret);
			return ret;
		}
	}

	return 0;
}

static void stop_swapd(int nid)
{
	struct pglist_data *pgdat = NODE_DATA(nid);
	struct xswapd_data *xdat = PGDAT_OEM_DATA(pgdat);

	if (xdat->xswapd) {
		atomic_set(&xdat->wait_flag, 1);
		kthread_stop(xdat->xswapd);
		xdat->xswapd = NULL;

		/* flush workqueue when stop */
		cancel_delayed_work_sync(&xdat->dwork);
	}
}

static void stop_swapds(void)
{
	int nid;

	for_each_node_state(nid, N_MEMORY)
		stop_swapd(nid);
}

static int xswapd_cpu_online(unsigned int cpu)
{
	int nid;

	for_each_node_state(nid, N_MEMORY) {
		pg_data_t *pgdat = NODE_DATA(nid);
		struct xswapd_data *xdat = PGDAT_OEM_DATA(pgdat);
		const struct cpumask *mask;

		mask = cpumask_of_node(pgdat->node_id);
		if (cpumask_any_and(cpu_online_mask, mask) < nr_cpu_ids)
			/* One of our CPUs online: restore mask */
			if (xdat->xswapd)
				set_cpus_allowed_ptr(xdat->xswapd, mask);
	}

	return 0;
}

static int xswapd_notifier_call(struct notifier_block *nb,
				unsigned long action, void *data)
{
	struct memory_notify *arg = data;
	int ret;

	switch (action) {
	case MEM_ONLINE:
		ret = run_xswapd(arg->status_change_nid);
		if (ret) {
			pr_err("run xswapd fail: %d", ret);
			return notifier_from_errno(ret);
		}
		break;
	case MEM_OFFLINE:
		stop_swapd(arg->status_change_nid);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block xswapd_notifier_block = {
	.notifier_call = xswapd_notifier_call,
};

static int xswapd_alloc(void)
{
	struct pglist_data *pgdat;
	struct xswapd_data *xdat;
	int nid;

	for_each_node(nid) {
		pgdat = NODE_DATA(nid);
		if (!PGDAT_OEM_DATA(pgdat)) {
			xdat = kzalloc(sizeof(struct xswapd_data), GFP_KERNEL);
			if (!xdat)
				return -ENOMEM;
			pgdat->android_oem_data1 = (u64)xdat;
		}
	}

	return 0;
}

static void xswapd_free(void)
{
	struct pglist_data *pgdat;
	int nid;

	for_each_node(nid) {
		pgdat = NODE_DATA(nid);
		if (PGDAT_OEM_DATA(pgdat)) {
			kfree(PGDAT_OEM_DATA(pgdat));
			pgdat->android_oem_data1 = 0;
		}
	}
}

static inline bool current_is_xswapd(void)
{
	struct pglist_data *pgdat;
	struct xswapd_data *xdat;
	int nid;

	for_each_node_state(nid, N_MEMORY) {
		pgdat = NODE_DATA(nid);
		xdat = PGDAT_OEM_DATA(pgdat);

		if (current == xdat->xswapd)
			return true;
	}

	return false;
}

/* TODO: compatibility with MGLRU. */
static void vh_tune_scan_type(void *data, enum scan_balance *s_balance)
{
	if (!is_xswapd_enable() && !is_xswapd_umrenable())
		return;

	if (current_is_xswapd()) {
		*s_balance = SCAN_ANON;
		return;
	}

	/* xswapd.reclaim work */
	if (current->flags & PF_RECLAIM_TASK) {
		*s_balance = SCAN_ANON;
		return;
	}
}

static void vh_get_page_wmark(void *data, gfp_t alloc_flags,
			      unsigned long *page_wmark)
{
	if (alloc_flags)
		wakeup_xswapds();
}

static int xswapd_register_hooks(void)
{
	int ret;

	ret = register_android_vh(tune_scan_type);
	if (ret)
		goto err;

	ret = register_android_vh(get_page_wmark);
	if (ret)
		goto err_wmark;

	return 0;

err_wmark:
	unregister_android_vh(tune_scan_type);
err:
	return ret;
}

static void xswapd_unregister_hooks(void)
{
	unregister_android_vh(get_page_wmark);
	unregister_android_vh(tune_scan_type);
}

static void xswapd_ctrl_init(void)
{
	memset(&xctrl, 0, sizeof(struct xswapd_ctrl));

	atomic_set(&xctrl.mem_press_watermark, DEF_MEM_PRESS_WATERMARK);
	atomic_set(&xctrl.mem_high_watermark, DEF_MEM_HIGH_WATERMARK);
	atomic_set(&xctrl.mem_low_watermark, DEF_MEM_LOW_WATERMARK);
	atomic_set(&xctrl.zram_press_watermark, DEF_ZRAM_PRESS_WATERMARK);
	atomic_set(&xctrl.reclaim_size, DEF_RECLAIM_SIZE);

	atomic_set(&xctrl.wake_interval, DEF_WAKE_INTERVAL);
	atomic_set(&xctrl.dry_run_wake_interval, DEF_DRY_RUN_WAKE_INTERVAL);
	atomic_set(&xctrl.dry_run_reclaim_nr, DEF_DRY_RUN_RECLAIM_NR);

	atomic_set(&xctrl.memcg_shrink_interval, DEF_MEMCG_SHRINK_INTERVAL);
	atomic_set(&xctrl.shrink_interval, DEF_SHRINK_INTERVAL);
	atomic_set(&xctrl.shrink_nr, DEF_SHRINK_NR);

	atomic_set(&xctrl.fault_interval, DEF_FAULT_INTERVAL);
	atomic_set(&xctrl.refault_ratio, DEF_REFAULT_RATIO);

	atomic64_set(&xctrl.wb_limit, DEF_WB_LIMIT);
}

static ssize_t xswapd_watermark_write(struct kernfs_open_file *of, char *buf,
				      size_t nbytes, loff_t off)
{
	unsigned int mem_press_watermark;
	unsigned int mem_high_watermark;
	unsigned int mem_low_watermark;
	unsigned int zram_press_watermark;

	buf = strstrip(buf);

	if (sscanf(buf, "%u %u %u %u", &mem_press_watermark,
				       &mem_high_watermark,
				       &mem_low_watermark,
				       &zram_press_watermark) != 4)
		return -EINVAL;

	if (mem_press_watermark > mem_high_watermark ||
	    mem_press_watermark < mem_low_watermark ||
	    mem_low_watermark > mem_high_watermark)
		return -EINVAL;

	atomic_set(&xctrl.mem_press_watermark, mem_press_watermark);
	atomic_set(&xctrl.mem_high_watermark, mem_high_watermark);
	atomic_set(&xctrl.mem_low_watermark, mem_low_watermark);
	atomic_set(&xctrl.zram_press_watermark, zram_press_watermark);

	wakeup_xswapds();

	return nbytes;
}

static int xswapd_watermark_show(struct seq_file *m, void *v)
{
	seq_printf(m, "mem_press_watermark:  %u\n",
			atomic_read(&xctrl.mem_press_watermark));
	seq_printf(m, "mem_high_watermark:   %u\n",
			atomic_read(&xctrl.mem_high_watermark));
	seq_printf(m, "mem_low_watermark:    %u\n",
			atomic_read(&xctrl.mem_low_watermark));
	seq_printf(m, "zram_press_watermark: %u\n",
			atomic_read(&xctrl.zram_press_watermark));

	return 0;
}

static int xswapd_reclaim_size_write(struct cgroup_subsys_state *css,
				     struct cftype *cft, u64 val)
{
	if (val > U32_MAX)
		return -EINVAL;

	atomic_set(&xctrl.reclaim_size, val);

	return 0;
}

static u64 xswapd_reclaim_size_read(struct cgroup_subsys_state *css,
				    struct cftype *cft)
{
	return atomic_read(&xctrl.reclaim_size);
}

static ssize_t xswapd_wake_interval_write(struct kernfs_open_file *of,
					  char *buf, size_t nbytes, loff_t off)
{
	unsigned int wake_interval;
	unsigned int dry_run_wake_interval;
	unsigned int dry_run_reclaim_nr;

	buf = strstrip(buf);

	if (sscanf(buf, "%u %u %u", &wake_interval,
				    &dry_run_wake_interval,
				    &dry_run_reclaim_nr) != 3)
		return -EINVAL;

	if (wake_interval < dry_run_wake_interval)
		return -EINVAL;

	atomic_set(&xctrl.wake_interval, wake_interval);
	atomic_set(&xctrl.dry_run_wake_interval, dry_run_wake_interval);
	atomic_set(&xctrl.dry_run_reclaim_nr, dry_run_reclaim_nr);

	return nbytes;
}

static int xswapd_wake_interval_show(struct seq_file *m, void *v)
{
	seq_printf(m, "wake_interval:         %u\n",
			atomic_read(&xctrl.wake_interval));
	seq_printf(m, "dry_run_wake_interval: %u\n",
			atomic_read(&xctrl.dry_run_wake_interval));
	seq_printf(m, "dry_run_reclaim_nr:    %u\n",
			atomic_read(&xctrl.dry_run_reclaim_nr));

	return 0;
}

static ssize_t xswapd_shrink_interval_write(struct kernfs_open_file *of,
					    char *buf, size_t nbytes,
					    loff_t off)
{
	unsigned int memcg_shrink_interval;
	unsigned int shrink_interval;
	unsigned int shrink_nr;

	buf = strstrip(buf);

	if (sscanf(buf, "%u %u %u", &memcg_shrink_interval,
				    &shrink_interval,
				    &shrink_nr) != 3)
		return -EINVAL;

	atomic_set(&xctrl.memcg_shrink_interval, memcg_shrink_interval);
	atomic_set(&xctrl.shrink_interval, shrink_interval);
	atomic_set(&xctrl.shrink_nr,
		   shrink_nr << PAGES_PER_MB_SHIFT);

	return nbytes;
}

static int xswapd_shrink_interval_show(struct seq_file *m, void *v)
{
	seq_printf(m, "memcg_shrink_interval: %u\n",
			atomic_read(&xctrl.memcg_shrink_interval));
	seq_printf(m, "shrink_interval:       %u\n",
			atomic_read(&xctrl.shrink_interval));
	seq_printf(m, "shrink_nr:             %u\n",
			atomic_read(&xctrl.shrink_nr) >> PAGES_PER_MB_SHIFT);

	return 0;
}

static ssize_t xswapd_refault_ctrl_write(struct kernfs_open_file *of,
					 char *buf, size_t nbytes, loff_t off)
{
	unsigned int fault_interval;
	unsigned int refault_ratio;

	buf = strstrip(buf);

	if (sscanf(buf, "%u %u", &fault_interval, &refault_ratio) != 2)
		return -EINVAL;

	atomic_set(&xctrl.fault_interval, fault_interval);
	atomic_set(&xctrl.refault_ratio, refault_ratio);
	return nbytes;
}

static int xswapd_refault_ctrl_show(struct seq_file *m, void *v)
{
	seq_printf(m, "fault_interval: %u\n",
			atomic_read(&xctrl.fault_interval));
	seq_printf(m, "refault_ratio:  %u\n",
			atomic_read(&xctrl.refault_ratio));

	return 0;
}

static ssize_t xswapd_pressure_event_write(struct kernfs_open_file *of,
					   char *buf, size_t nbytes,
					   loff_t off)
{
	unsigned int state;
	int efd;
	struct eventfd_ctx *evt_ctx;

	buf = strstrip(buf);

	if (sscanf(buf, "%u %d", &state, &efd) != 2)
		return -EINVAL;

	if (state >= NR_XSWAPD_PRESS_STATE)
		return -EINVAL;

	evt_ctx = eventfd_ctx_fdget(efd);
	if (IS_ERR(evt_ctx))
		return PTR_ERR(evt_ctx);

	xctrl.press_eventfd[state] = evt_ctx;

	return nbytes;
}

static int xswapd_pid_show(struct seq_file *m, void *v)
{
	int nid;

	for_each_node_state(nid, N_MEMORY) {
		pg_data_t *pgdat = NODE_DATA(nid);
		struct xswapd_data *xdat = PGDAT_OEM_DATA(pgdat);

		if (!xdat || !xdat->xswapd)
			continue;

		seq_printf(m, "%d\n", task_pid_nr(xdat->xswapd));
	}

	return 0;
}

static int xswapd_event_stat_show(struct seq_file *m, void *v)
{
	unsigned long events[NR_XSWAPD_ITEMS];

	xswapd_get_events(events);

#define PRINT_EVENT(s, i) seq_printf(m, s "%lu\n", events[i])

	PRINT_EVENT("wake_up:            ", XSWAPD_WAKEUP);
	PRINT_EVENT("shrink_node:        ", XSWAPD_SHRINK_NODE);
	PRINT_EVENT("shrink_anon:        ", XSWAPD_SHRINK_ANON);
	PRINT_EVENT("shrink_wb:          ", XSWAPD_SHRINK_WB);
	PRINT_EVENT("update_refault:     ", XSWAPD_UPDATE_REFAULT);
	PRINT_EVENT("refault_skip:       ", XSWAPD_REFAULT_SKIP);
	PRINT_EVENT("memcg_reafult_skip: ", XSWAPD_MEMCG_REFAULT_SKIP);
	PRINT_EVENT("memcg_cmp_skip:     ", XSWAPD_MEMCG_CMP_SKIP);
	PRINT_EVENT("memcg_wb_skip:      ", XSWAPD_MEMCG_WB_SKIP);
	PRINT_EVENT("reclaimed:          ", XSWAPD_RECLAIMED);
	PRINT_EVENT("writeback:          ", XSWAPD_WRITEBACK);
	PRINT_EVENT("min_press:          ", XSWAPD_MIN_PRESS);
	PRINT_EVENT("low_press:          ", XSWAPD_LOW_PRESS);
	PRINT_EVENT("high_press:         ", XSWAPD_HIGH_PRESS);
	PRINT_EVENT("low_mem_skip:       ", XSWAPD_LOW_MEM_SKIP);
	PRINT_EVENT("interval_skip:      ", XSWAPD_INTERVAL_SKIP);
	PRINT_EVENT("dry_run:            ", XSWAPD_DRY_RUN);

#undef PRINT_EVENT

	return 0;
}

static int xswapd_stat_show(struct seq_file *m, void *v)
{
	int nid;
	unsigned long nr_comp, sz_comp, fault_comp, drop_comp;
#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
	unsigned long nr_ext, nr_wb, sz_wb, fault_wb, drop_wb;
#endif

	nr_comp = zgroup_get_memcg_stat(NULL, ZGROUP_OBJ_PAGES);
	sz_comp = zgroup_get_memcg_stat(NULL, ZGROUP_OBJ_SIZE);
	fault_comp = zgroup_get_memcg_stat(NULL, ZGROUP_OBJ_FAULT);
	drop_comp = zgroup_get_memcg_stat(NULL, ZGROUP_OBJ_DROP);

	seq_printf(m, "nr_comp:    %lu\n", nr_comp);
	seq_printf(m, "sz_comp:    %lu\n", sz_comp);
	seq_printf(m, "fault_comp: %lu\n", fault_comp);
	seq_printf(m, "drop_comp:  %lu\n", drop_comp);

#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
	nr_ext = zgroup_get_memcg_stat(NULL, ZGROUP_WB_EXTS);
	nr_wb = zgroup_get_memcg_stat(NULL, ZGROUP_WB_PAGES);
	sz_wb = zgroup_get_memcg_stat(NULL, ZGROUP_WB_SIZE);
	fault_wb = zgroup_get_memcg_stat(NULL, ZGROUP_WB_FAULT);
	drop_wb = zgroup_get_memcg_stat(NULL, ZGROUP_WB_DROP);

	seq_printf(m, "nr_ext:     %lu\n", nr_ext);
	seq_printf(m, "nr_wb:      %lu\n", nr_wb);
	seq_printf(m, "sz_wb:      %lu\n", sz_wb);
	seq_printf(m, "fault_wb:   %lu\n", fault_wb);
	seq_printf(m, "drop_wb:    %lu\n", drop_wb);
#endif

	xswapd_event_stat_show(m, v);

	for_each_node_state(nid, N_MEMORY) {
		pg_data_t *pgdat = NODE_DATA(nid);
		struct xswapd_data *xdat = PGDAT_OEM_DATA(pgdat);

		if (!xdat || !xdat->xswapd)
			continue;

		seq_printf(m, "[nid %d]\n", nid);
		seq_printf(m, "wake_time:     %lu\n", xdat->wake_time);
		seq_printf(m, "wake_interval: %u\n", xdat->wake_interval);
		seq_printf(m, "dry_run:       %d\n", xdat->dry_run);
		seq_printf(m, "shrink_time:   %lu\n", xdat->shrink_time);
		seq_printf(m, "shrink_nr:     %lu\n", xdat->shrink_nr);
		seq_printf(m, "last_fault:    %lu\n", xdat->last_fault);
		seq_printf(m, "fault_time:    %lu\n", xdat->fault_time);
	}

	return 0;
}

/* HACK: should use memcg_page_state */
static unsigned long xswapd_stat_memcg_pages(struct mem_cgroup *memcg,
					     enum node_stat_item idx)
{
	struct lruvec *lruvec;
	pg_data_t *pgdat;
	int nid;
	unsigned long nr = 0;

	if (!memcg)
		return 0;

	for_each_node_state(nid, N_MEMORY) {
		pgdat = NODE_DATA(nid);
		lruvec = mem_cgroup_lruvec(memcg, pgdat);

		nr += lruvec_page_state(lruvec, idx);
	}

	return nr;
}

static int xswapd_iter_stat_show(struct seq_file *m, void *v)
{
	struct mem_cgroup *memcg = NULL;
	struct memcg_ctrl *mctrl;
	unsigned long nr_anon, nr_comp, sz_comp, fault_comp, drop_comp;
#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
	unsigned long nr_ext, nr_wb, sz_wb, fault_wb, drop_wb;
#endif

	seq_puts(m, "[name] score nr_anon nr_comp sz_comp fault_comp drop_comp last_fault");
#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
	seq_puts(m, " nr_ext, nr_wb, sz_wb, fault_wb drop_wb");
#endif
	seq_puts(m, "\n");

	while ((memcg = get_next_memcg(memcg, false))) {
		mctrl = MEMCG_OEM_DATA(memcg);

		nr_anon = xswapd_stat_memcg_pages(memcg, NR_INACTIVE_ANON) +
			  xswapd_stat_memcg_pages(memcg, NR_ACTIVE_ANON);
		nr_comp = zgroup_get_memcg_stat(memcg, ZGROUP_OBJ_PAGES);
		sz_comp = zgroup_get_memcg_stat(memcg, ZGROUP_OBJ_SIZE);
		fault_comp = zgroup_get_memcg_stat(memcg, ZGROUP_OBJ_FAULT);
		drop_comp = zgroup_get_memcg_stat(NULL, ZGROUP_OBJ_DROP);

		seq_printf(m, "[%s] %u %lu %lu %lu %lu %lu %lu",
			   mctrl->name, mctrl->score,
			   nr_anon, nr_comp, sz_comp, fault_comp, drop_comp,
			   mctrl->last_fault);

#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
		nr_ext = zgroup_get_memcg_stat(memcg, ZGROUP_WB_EXTS);
		nr_wb = zgroup_get_memcg_stat(memcg, ZGROUP_WB_PAGES);
		sz_wb = zgroup_get_memcg_stat(memcg, ZGROUP_WB_SIZE);
		fault_wb = zgroup_get_memcg_stat(memcg, ZGROUP_WB_FAULT);
		drop_wb = zgroup_get_memcg_stat(NULL, ZGROUP_WB_DROP);

		seq_printf(m, " %lu %lu %lu %lu %lu",
			   nr_ext, nr_wb, sz_wb, fault_wb, drop_wb);
#endif
		seq_puts(m, "\n");
	}

	return 0;
}

static void xswapd_reclaim_pages(struct mem_cgroup *memcg,
				 unsigned long nr_to_reclaim,
				 unsigned long reclaim_window,
				 bool may_swap)
{
	unsigned long nr_reclaimed = 0;
	unsigned long reclaimed_window;
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);
	unsigned int reclaim_op;

	if (!mctrl)
		return;

	if (may_swap)
		current->flags |= PF_RECLAIM_TASK;

	reclaim_op = may_swap ? MEMCG_RECLAIM_MAY_SWAP : 0;
	reclaim_op |= MEMCG_RECLAIM_PROACTIVE;

	while (nr_reclaimed < nr_to_reclaim && !xswapd_stop_reclaim()) {
		reclaimed_window = try_to_free_mem_cgroup_pages(memcg,
								reclaim_window,
								GFP_KERNEL,
								reclaim_op);

		pr_debug("[%s] force reclaim %lu %lu/%lu", mctrl->name,
			 reclaimed_window, nr_reclaimed, nr_to_reclaim);

		if (!reclaimed_window)
			break;

		nr_reclaimed += reclaimed_window;
	}

	if (may_swap)
		current->flags &= ~PF_RECLAIM_TASK;
}

static ssize_t xswapd_reclaim_write(struct kernfs_open_file *of, char *buf,
				    size_t nbytes, loff_t off)
{
	bool may_swap;
	unsigned long type, nr_to_reclaim, reclaim_window;
	unsigned long nr_can_reclaim;
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);

	if (!is_xswapd_umrenable())
		if (!is_xswapd_enable())
			return -EPERM;

	if (!mctrl)
		return -EPERM;

	if (!is_xswapd_umrenable() && !is_memcg_reclaimable(mctrl))
		return -EPERM;

	buf = strstrip(buf);

	if (sscanf(buf, "%lu %lu %lu", &type,
				       &nr_to_reclaim,
				       &reclaim_window) != 3)
		return -EINVAL;

	if (type > NR_ACTIVE_FILE)
		return -EINVAL;

	if (nr_to_reclaim < reclaim_window)
		return -EINVAL;

	/* default reclaim window is 4MB */
	reclaim_window = reclaim_window ?: 4;
	/* MB to pages */
	reclaim_window <<= PAGES_PER_MB_SHIFT;
	nr_to_reclaim <<= PAGES_PER_MB_SHIFT;

	nr_can_reclaim = xswapd_stat_memcg_pages(memcg, type);
	/* active means both active and inactive are reclaimed */
	switch (type) {
	case NR_ACTIVE_ANON:
		nr_can_reclaim += xswapd_stat_memcg_pages(memcg, type - 1);
		fallthrough;
	case NR_INACTIVE_ANON:
		may_swap = true;
		break;
	case NR_ACTIVE_FILE:
		nr_can_reclaim += xswapd_stat_memcg_pages(memcg, type - 1);
		fallthrough;
	case NR_INACTIVE_FILE:
		may_swap = false;
		break;
	/* no need for default branch since item has already checked */
	}

	pr_debug("[%s] force reclaim anon=%d to=%lu can=%lu window=%lu",
		 mctrl->name, may_swap,
		 nr_to_reclaim, nr_can_reclaim, reclaim_window);

	if (nr_to_reclaim == 0 || nr_to_reclaim > nr_can_reclaim)
		nr_to_reclaim = nr_can_reclaim;
	if (reclaim_window > nr_can_reclaim)
		reclaim_window = nr_can_reclaim;

	xswapd_reclaim_pages(memcg, nr_to_reclaim, reclaim_window, may_swap);

	return nbytes;
}

#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
static int xswapd_swapin_write(struct cgroup_subsys_state *css,
			       struct cftype *cft, u64 val)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);
	unsigned long sz_wb, sz_rb, sz_to_rb;

	/* percentage */
	if (val > 100)
		return -EINVAL;

	if (!mctrl)
		return -EPERM;

	sz_wb = zgroup_get_memcg_stat(memcg, ZGROUP_WB_SIZE);
	sz_to_rb = sz_wb * val / 100;
	sz_rb = zgroup_memcg_rb(memcg, sz_to_rb);

	pr_debug("readback %lu expect %lu/%lu", sz_rb, sz_to_rb, sz_wb);
	return 0;
}

static ssize_t xswapd_swapout_write(struct kernfs_open_file *of, char *buf,
				    size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);
	unsigned long percent, sync_wb;
	unsigned long sz_comp, sz_wb, sz_to_wb;
	unsigned long io_wb = 0;

	if (!is_xswapd_umrenable())
		if (!is_xswapd_enable())
			return -EPERM;

	if (!mctrl)
		return -EPERM;

	if (!is_xswapd_umrenable() && !is_memcg_reclaimable(mctrl))
		return -EPERM;

	if (is_wb_limit())
		return -ENOSPC;

	buf = strstrip(buf);

	if (sscanf(buf, "%lu %lu", &percent, &sync_wb) != 2)
		return -EINVAL;

	if (percent > 100 || sync_wb > 1)
		return -EINVAL;

	sz_comp = zgroup_get_memcg_stat(memcg, ZGROUP_OBJ_SIZE);
	sz_to_wb = sz_comp * percent  / 100;
	sz_wb = zgroup_memcg_wb(memcg, sz_to_wb, &io_wb, sync_wb);
	update_wb_limit(io_wb);

	pr_debug("writeback %lu expect %lu/%lu sync_wb=%lu\n", 
		 sz_wb, sz_to_wb, sz_comp, sync_wb);
	return nbytes;
}

static int xswapd_quota_write(struct cgroup_subsys_state *css,
			      struct cftype *cft, u64 val)
{
	atomic64_set(&xctrl.wb_limit, val);

	return 0;
}

static u64 xswapd_quota_read(struct cgroup_subsys_state *css,
			     struct cftype *cft)
{
	return atomic64_read(&xctrl.wb_limit);
}
#endif

static int xswapd_stop_swap_write(struct cgroup_subsys_state *css,
				  struct cftype *cft, u64 val)
{
	if (val & ~(BIT(XSWAPD_STOP_RECLAIM) | BIT(XSWAPD_STOP_WB))) {
		pr_err("invalid stop_swap flags: %llu", val);
		return -EINVAL;
	}

	xctrl.stop_swap = val;
	return 0;
}

static u64 xswapd_stop_swap_read(struct cgroup_subsys_state *css,
				 struct cftype *cft)
{
	return xctrl.stop_swap;
}

static int xswapd_enable_write(struct cgroup_subsys_state *css,
			       struct cftype *cft, u64 val)
{
	if (val > 1) {
		pr_err("zhang val error");
		return -EINVAL;
	}

	if (!zgroup_get_enable()) {
		pr_err("zhang zgroup_get_enable fail is %d",zgroup_get_enable());
		return -EPERM;
	}

	if (!get_swap_total()) {
		pr_err("zhang !get_swap_total fail is %u",get_swap_total());
		return -EPERM;
	}

	xctrl.enable = val;
	pr_err("zhang xswapd_enable_write success val is %llu",val);

	return 0;
}

static u64 xswapd_enable_read(struct cgroup_subsys_state *css,
			      struct cftype *cft)
{
	return xctrl.enable;
}

static int xswapd_umrenable_write(struct cgroup_subsys_state *css,
                                 struct cftype *cft, u64 val)
{
	if (val > 1) {
		pr_err("zhang val error");
		return -EINVAL;
	}

	if (!zgroup_get_enable()) {
		pr_err("zhang zgroup_get_enable fail is %d",zgroup_get_enable());
		return -EPERM;
	}

	if (!get_swap_total()) {
		pr_err("zhang !get_swap_total fail is %u",get_swap_total());
		return -EPERM;
	}

	xctrl.umrenable = val;
	pr_err("zhang xswapd_enable_write success val is %llu",val);

	return 0;
}

static u64 xswapd_umrenable_read(struct cgroup_subsys_state *css,
                                struct cftype *cft)
{
	return xctrl.umrenable;
}

struct cftype xswapd_files[] = {
	{
		.name = "xswapd.watermark",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write = xswapd_watermark_write,
		.seq_show = xswapd_watermark_show,
	},
	{
		.name = "xswapd.reclaim_size",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write_u64 = xswapd_reclaim_size_write,
		.read_u64 = xswapd_reclaim_size_read,
	},
	{
		.name = "xswapd.wake_interval",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write = xswapd_wake_interval_write,
		.seq_show = xswapd_wake_interval_show,
	},
	{
		.name = "xswapd.shrink_interval",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write = xswapd_shrink_interval_write,
		.seq_show = xswapd_shrink_interval_show,
	},
	{
		.name = "xswapd.refault_ctrl",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write = xswapd_refault_ctrl_write,
		.seq_show = xswapd_refault_ctrl_show,
	},
	{
		.name = "xswapd.pressure_event",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write = xswapd_pressure_event_write,
	},
	{
		.name = "xswapd.pid",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.seq_show = xswapd_pid_show,
	},
	{
		.name = "xswapd.stat",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.seq_show = xswapd_stat_show,
	},
	{
		.name = "xswapd.iter_stat",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.seq_show = xswapd_iter_stat_show,
	},
	{
		.name = "xswapd.reclaim",
		.write = xswapd_reclaim_write,
	},
#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
	{
		.name = "xswapd.swapin",
		.write_u64 = xswapd_swapin_write,
	},
	{
		.name = "xswapd.swapout",
		.write = xswapd_swapout_write,
	},
	{
		.name = "xswapd.quota",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write_u64 = xswapd_quota_write,
		.read_u64 = xswapd_quota_read,
	},
#endif
	{
		.name = "xswapd.stop_swap",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.read_u64 = xswapd_stop_swap_read,
		.write_u64 = xswapd_stop_swap_write,
	},
	{
		.name = "xswapd.enable",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write_u64 = xswapd_enable_write,
		.read_u64 = xswapd_enable_read,
	},
	{
		.name = "xswapd.umrenable",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write_u64 = xswapd_umrenable_write,
		.read_u64 = xswapd_umrenable_read,
	},
	{ }, /* terminate */
};

int xswapd_init(void)
{
	int ret;

	xswapd_ctrl_init();

	ret = xswapd_alloc();
	if (ret) {
		pr_err("alloc xswapd fail: %d", ret);
		goto err;
	}

	ret = register_memory_notifier(&xswapd_notifier_block);
	if (ret) {
		pr_err("register memory hotplug callback fail: %d\n", ret);
		goto err;
	}

	ret = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN, "mm/xswapd:online",
					xswapd_cpu_online, NULL);
	if (ret < 0) {
		pr_err("register cpu hotplug callback fail: %d\n", ret);
		goto err_cpu;
	}
	xctrl.xswapd_cpuhp_state = ret;

	ret = run_xswapds();
	if (ret) {
		pr_err("run xswapd fail: %d", ret);
		goto err_run;
	}

	ret = cgroup_add_legacy_cftypes(&memory_cgrp_subsys, xswapd_files);
	if (ret) {
		pr_err("add xswapd_files fail: %d\n", ret);
		goto err_cft;
	}

	ret = xswapd_register_hooks();
	if (ret) {
		pr_err("register xswapd hooks fail: %d", ret);
		goto err_hook;
	}

	pr_info("xswapd init success");

	return ret;

err_hook:
	xswapd_unregister_hooks();
err_cft:
	cgroup_rm_cftypes(xswapd_files);
err_run:
	stop_swapds();
	cpuhp_remove_state_nocalls(xctrl.xswapd_cpuhp_state);
err_cpu:
	unregister_memory_notifier(&xswapd_notifier_block);
err:
	xswapd_free();

	return ret;
}

void xswapd_exit(void)
{
	xswapd_unregister_hooks();
	cgroup_rm_cftypes(xswapd_files);
	stop_swapds();
	cpuhp_remove_state_nocalls(xctrl.xswapd_cpuhp_state);
	unregister_memory_notifier(&xswapd_notifier_block);
	xswapd_free();
}
