#include "kscene.h"

#define MAX_KEY_CNT    5

__attribute__((unused))
static struct key_val_pair *parse_key_value_pairs(const char *str, int *pair_cnt) {
	int cnt = 0;
	int i;
	int pair_start = 0;
	int pair_token = pair_start;
	int pair_end = 0;
	struct key_val_pair *pairs;
	bool reset = false;
	int key_len, val_len;

	pairs = kmalloc(sizeof(struct key_val_pair) * MAX_KEY_CNT, GFP_KERNEL);
	if (!pairs) {
		ks_dbg("kcalloc failed\n");
		return NULL;
	}

	for (i = 0; i <= strlen(str); i++) {
		//重置位置指针
		if (reset) {
			reset = false;
			pair_start = i;
			pair_token = pair_start;
			pair_end = 0;
		}
		//记录键-值分隔符
		if (str[i] == ':') {
			pair_token = i;
		}
		// 记录键值对结束符
		if (str[i] == ';' || str[i] == '\0' || str[i] == ',') {
			// 如果键值对数量超过最大值，则退出
			if (cnt >= MAX_KEY_CNT) {
				ks_dbg("too many pairs, support %d max\n", MAX_KEY_CNT);
				break;
			}
			// 空键值对
			if (i == pair_start) {
				ks_dbg("null str, invalid str\n");
				reset = true;
				continue;
			}
			pair_end = i; // 记录键值对结束符, 前闭后开
			// 空键值
			if (pair_start == pair_token) {
				ks_dbg("null key");
				reset = true;
				continue;
			}

			//可用键值对
			key_len = pair_token - pair_start;
			val_len = pair_end - pair_token - 1;
			if (val_len == 0) {
				ks_dbg("null value");
				reset = true;
				continue;
			}
			pairs[cnt].key = kstrndup(str + pair_start, key_len, GFP_KERNEL);
			pairs[cnt].value = kstrndup(str + pair_token + 1, val_len, GFP_KERNEL);
			//ks_dbg("Found pair: %s:%s\n", pairs[cnt].key, pairs[cnt].value);
			cnt++;
			reset = true;
		}
	}

	*pair_cnt = cnt;
	//ks_dbg("Total pairs: %d\n", cnt);
	return pairs;
}

__attribute__((unused))
static void free_key_val_pairs(struct key_val_pair* pairs, int cnt) {
	for (int i = 0; i < cnt; i++) {
		kfree(pairs[i].key);
		kfree(pairs[i].value);
	}
	kfree(pairs);
}

int util_str_cmd_store(const char *buf, const struct kernel_param *param) {
#if 0
	struct key_val_pair* pairs = NULL;
	char *str;
	int pair_cnt= 0;
	int ret = 0;

	int dur = 0;
	int64_t frame_id = INVALID_VSYNC_ID;
	pid_t frame_pid = 0;
	pid_t render_tid = 0;
	pid_t top_pid = 0;
	//ktime_t now;
	enum frame_event frame_event = FRAME_EVENT_INVALID;

	str = kstrdup(buf, GFP_KERNEL);
	if (!str)
		return -ENOMEM;
	ks_dbg("store: %s\n", str);
	pairs = parse_key_value_pairs(str, &pair_cnt);
	if (!pairs) {
		ks_dbg("Failed to parse key-value pairs.\n");
	}

	for (int i = 0; i < pair_cnt; i++) {
		switch (pairs[i].key[0]) {
			case 't': // top-app
				if (0 == strcmp(pairs[i].key, "top") && pairs[i].value) {
					ret = kstrtoint(pairs[i].value, 10, &top_pid);
					if (ret) {
						continue;
					}
					ks_dbg("TOP-APP, top_pid = %d\n", top_pid);
				}
				break;
			case 'a': // core scene animation
				if (0 == strcmp(pairs[i].key, "anim-s") && pairs[i].value) {
					ret = kstrtoint(pairs[i].value, 10, &pid_focused);
					if (ret) {
						continue;
					}
					ks_dbg("core-anim-start, pid_focused = %d\n", pid_focused);
				} else if (0 == strcmp(pairs[i].key, "anim-e")) {
					pid_focused = -1; //TODO:
					ks_dbg("core-anim-end, pid_focused = %d\n", pid_focused);
					if (!pid_focused) {
						pr_err("pid_focused is NULL\n");
					}
				} else if (0 == strcmp(pairs[i].key, "a-m-u")) { //onUp
					tracing_counter(TRACE_LEVEL_DEBUG, TOUCH_TAG, TOUCH_STAT_UP);
					ks_dbg("motion: onUP");
				} else if (0 == strcmp(pairs[i].key, "a-m-d")) { //onDown
					tracing_counter(TRACE_LEVEL_DEBUG, TOUCH_TAG, TOUCH_STAT_DOWN);
					ks_dbg("motion: onDown");
				} else if (0 == strcmp(pairs[i].key, "a-m-sb")) { //onScroll begin
					tracing_counter(TRACE_LEVEL_DEBUG, TOUCH_TAG, TOUCH_STAT_SCROLL);
					ks_dbg("motion: onScroll begin");
					kscene_state.touch_state = TOUCH_STAT_SCROLL;
				} else if (0 == strcmp(pairs[i].key, "a-m-fs") && pairs[i].value) { //onDown
					ret = kstrtoint(pairs[i].value, 10, &dur);
					if (ret) {
						continue;
					}
					tracing_counter(TRACE_LEVEL_DEBUG, TOUCH_TAG, TOUCH_STAT_FLING);
					ks_dbg("motion: onFling dur=%d", dur);
					kscene_state.touch_state = TOUCH_STAT_FLING;
					kscene_state.desire_fling_end = ktime_add_ns(ktime_get(), dur * 1000000);
					tracing_counter(TRACE_LEVEL_DEBUG, "fling_dur", dur);
				} else if (0 == strcmp(pairs[i].key, "a-m-fe")) { //onDown
					ks_dbg("fling end");
					tracing_counter(TRACE_LEVEL_DEBUG, TOUCH_TAG, TOUCH_STAT_IDLE);
					kscene_state.touch_state = TOUCH_STAT_IDLE;
				}
				break;
			case 'f': // frame event
				//if (pairs[i].key[1] == '-') {
				//    kscene_parse_frame_event_str(pairs, pair_cnt, &frame_pid, &render_tid, &frame_id);
				//    return 0;
				//}

				if (0 == strcmp(pairs[i].key, "fcs") && pairs[i].value) {
					ret = kstrtoint(pairs[i].value, 10, &pid_focused);
					if (ret) {
						continue;
					}
					ks_dbg("new focus pid = %d", pid_focused);
					kscene_handle_proc_focus(pid_focused);
				} else if (0 == strcmp(pairs[i].key, "f-u-s") && pairs[i].value) {
					ret = kstrtoll(pairs[i].value, 10, &frame_id);
					if (ret) {
						continue;
					}
					//ks_dbg("frame-ui-start, frame_id = %lld", frame_id);
					frame_event = FRAME_EVENT_MAIN_START;
				} else if (0 == strcmp(pairs[i].key, "f-v-l")) {
					frame_event = FRAME_EVENT_ON_VSYNC_LATE;
				} else if (0 == strcmp(pairs[i].key, "f-u-e") && pairs[i].value) {
					ret = kstrtoll(pairs[i].value, 10, &frame_id);
					if (ret) {
						continue;
					}
					//ks_dbg("frame-ui-end, frame_id = %lld", frame_id);
					frame_event = FRAME_EVENT_MAIN_END;
				} else if (0 == strcmp(pairs[i].key, "f-u-n") && pairs[i].value) {
					//ret = kstrtoll(pairs[i].value, 10, &frame_id);
					//if (ret) {
					//    continue;
					//}
					//ks_dbg("frame-ui-end, frame_id = %lld", frame_id);
					frame_event = FRAME_EVENT_MAIN_NULL_TRAVERSAL;
				} else if (0 == strcmp(pairs[i].key, "f-r-s") && pairs[i].value) {
					ret = kstrtoll(pairs[i].value, 10, &frame_id);
					if (ret) {
						continue;
					}
					//ks_dbg("frame-render-start, frame_id = %lld", frame_id);
					frame_event = FRAME_EVENT_RENDER_START;
				} else if (0 == strcmp(pairs[i].key, "f-r-e") && pairs[i].value) {
					ret = kstrtoll(pairs[i].value, 10, &frame_id);
					if (ret) {
						continue;
					}
					//ks_dbg("frame-render-end, frame_id = %lld", frame_id);
					frame_event = FRAME_EVENT_RENDER_END;
				} else if (0 == strcmp(pairs[i].key, "f-p") && pairs[i].value) {
					ret = kstrtoint(pairs[i].value, 10, &frame_pid);
					if (ret) {
						continue;
					}
					//ks_dbg("frame-pid, pid=%d\n", frame_pid);
				} else if (0 == strcmp(pairs[i].key, "f-r") && pairs[i].value) {
					ret = kstrtoint(pairs[i].value, 10, &render_tid);
					if (ret) {
						continue;
					}
					//ks_dbg("frame-render-tid, render tid=%d\n", render_tid);
				}
				break;
			case 'p': // pid set
				if (0 == strcmp(pairs[i].key, "p-home") && pairs[i].value) {
					ret = kstrtoint(pairs[i].value, 10, &pid_home);
					if (ret) {
						continue;
					}
					ks_dbg("update home pid: %d\n", pid_home);
				} else if (0 == strcmp(pairs[i].key, "p-sysui") && pairs[i].value) {
					ret = kstrtoint(pairs[i].value, 10, &pid_systemui);
					if (ret) {
						continue;
					}
					ks_dbg("update systemui pid: %d\n", pid_systemui);
				} else if (0 == strcmp(pairs[i].key, "p-ss") && pairs[i].value) {
					ret = kstrtoint(pairs[i].value, 10, &pid_system_server);
					if (ret) {
						continue;
					}
					ks_dbg("update system_server pid: %d\n", pid_system_server);
				} else if (0 == strcmp(pairs[i].key, "p-sf") && pairs[i].value) {
					ret = kstrtoint(pairs[i].value, 10, &pid_sf);
					if (ret) {
						continue;
					}
					ks_dbg("update SF pid: %d\n", pid_sf);
				}
				break;
			default:
				break;
		}
	}

	kfree(str);
	free_key_val_pairs(pairs, pair_cnt);

	//ks_dbg("frame: event=%d, frame_id=%lld, pid=%d, tid=%d\n", frame_event, frame_id, frame_pid, render_tid);
	if (pid_focused != 0 && frame_pid == pid_focused) {
		// UI thread running, and not do onVsync yet, start timer
		if (frame_event == FRAME_EVENT_ON_VSYNC_LATE
				&& (kscene_state.touch_state == TOUCH_STAT_SCROLL
				|| (kscene_state.touch_state == TOUCH_STAT_FLING && kscene_state.ux_cnt <=2))) {
			ks_dbg("frame-vsync-late, To start timer");
			kscene_start_frame_timer(FRAME_TIMER_DUR_NOW);
		} else if ((frame_id != INVALID_VSYNC_ID && frame_event != FRAME_EVENT_INVALID
				&& frame_pid != 0) || frame_event == FRAME_EVENT_MAIN_NULL_TRAVERSAL) {
			#if CX_TEST_KSCENE
			//ks_dbg("frame_pid=%d, pid_focused=%d", frame_pid, pid_focused);
			ks_dbg("TEST: focus vsyncId-%lld\n", frame_id);
			if (frame_event == FRAME_EVENT_RENDER_START && frame_pid == kscene_state.top_pid
					&& render_tid != kscene_state.render_tid) {
				kscene_state.render_tid = render_tid;
			}
			if (frame_event == FRAME_EVENT_MAIN_START) {
				ks_dbg("To start timer");
				#if 0 // works not good
				if (kscene_state.touch_state == TOUCH_STAT_FLING) {
					// TODO: check with exec delta time...
					now = ktime_get();
					ks_dbg("check main interval %lld, %lld, %lld", now, kscene_state.last_ui_frame_end, now - kscene_state.last_ui_frame_end);
					tracing_counter(TRACE_LEVEL_DEBUG, "ui_end_begin_interval", now - kscene_state.last_ui_frame_end);
					if (ktime_get() > kscene_state.desire_fling_end
							&& (now - kscene_state.last_ui_frame_end) > cur_period_ns) {
						ks_dbg("Fling end");
						kscene_state.touch_state = TOUCH_STAT_IDLE;
						tracing_counter(TRACE_LEVEL_DEBUG, TOUCH_TAG, TOUCH_STAT_IDLE);
					}
				}
				#endif
				if (kscene_state.ux_cnt <= 2) {
					if (kscene_state.touch_state == TOUCH_STAT_DOWN || kscene_state.touch_state == TOUCH_STAT_SCROLL) {
						kscene_start_frame_timer(kscene_state.vsync_period_ns * FRAME_TIMER_DUR_MULTIP_NORMAL / FRAME_TIMER_DUR_DIV_NORMAL);
					} else if (kscene_state.touch_state == TOUCH_STAT_FLING) {
						kscene_start_frame_timer(kscene_state.vsync_period_ns * FRAME_TIMER_DUR_MULTIP_URGENT / FRAME_TIMER_DUR_DIV_URGENT);
					}
				}
			} else if (frame_event == FRAME_EVENT_RENDER_END) {
				ks_dbg("To cancel timer");
				kscene_cancel_frame_timer();
			} else if (frame_event == FRAME_EVENT_MAIN_NULL_TRAVERSAL) {
				ks_dbg("To cancel timer, null traversal");
				kscene_cancel_frame_timer();
			}
		}
		#endif
	}
#endif
	return 0;
}

/*
 * Approximate:
 *   val * y^n,    where y^32 ~= 0.5 (~1 scheduling period)
 */
static u64 decay_load_copy(u64 val, u64 n)
{
	unsigned int local_n;

	tracing_counter(TRACE_LEVEL_DEBUG, "decay_periods:", n);
	if (unlikely(n > LOAD_AVG_PERIOD * 63)) {
		tracing_counter(TRACE_LEVEL_DEBUG, "decay_ret_0:", 0);
		return 0;
	}

	/* after bounds checking we can collapse to 32-bit */
	local_n = n;

	/*
	 * As y^PERIOD = 1/2, we can combine
	 *    y^n = 1/2^(n/PERIOD) * y^(n%PERIOD)
	 * With a look-up table which covers y^n (n<PERIOD)
	 *
	 * To achieve constant time decay_load.
	 */
	if (unlikely(local_n >= LOAD_AVG_PERIOD)) {
		val >>= local_n / LOAD_AVG_PERIOD;
		local_n %= LOAD_AVG_PERIOD;
	}

	kscene_trace_mark_u64("decay_val", val);
	kscene_trace_mark_u64("decay_mul", runnable_avg_yN_inv[local_n]);
	val = mul_u64_u32_shr(val, runnable_avg_yN_inv[local_n], 32);
	kscene_trace_mark_u64("decay_ret", val);
	return val;
}

static u32 __accumulate_pelt_segments_copy(u64 periods, u32 d1, u32 d3)
{
	u32 c1, c2, c3 = d3; /* y^0 == 1 */

	/*
	 * c1 = d1 y^p
	 */
	c1 = decay_load_copy((u64)d1, periods);

	/*
	 *            p-1
	 * c2 = 1024 \Sum y^n
	 *            n=1
	 *
	 *              inf        inf
	 *    = 1024 ( \Sum y^n - \Sum y^n - y^0 )
	 *              n=0        n=p
	 */
	c2 = LOAD_AVG_MAX - decay_load_copy(LOAD_AVG_MAX, periods) - 1024;

	return c1 + c2 + c3;
}

static __always_inline u32
accumulate_sum_copy(u64 delta, struct sched_avg *sa,
		   unsigned long load, unsigned long runnable, int running)
{
	u32 contrib = (u32)delta; /* p == 0 -> delta < 1024 */
	u64 periods;

	delta += sa->period_contrib;
	periods = delta / 1024; /* A period is 1024us (~1ms) */

	/*
	 * Step 1: decay old *_sum if we crossed period boundaries.
	 */
	if (periods) {
		sa->load_sum = decay_load_copy(sa->load_sum, periods);
		sa->runnable_sum =
			decay_load_copy(sa->runnable_sum, periods);
		tracing_counter(TRACE_LEVEL_DEBUG, "update_load_util_sum_0:", sa->util_sum);
		sa->util_sum = decay_load_copy((u64)(sa->util_sum), periods);
		tracing_counter(TRACE_LEVEL_DEBUG, "update_load_util_sum_1:", sa->util_sum);

		/*
		 * Step 2
		 */
		delta %= 1024;
		if (load) {
			/*
			 * This relies on the:
			 *
			 * if (!load)
			 *	runnable = running = 0;
			 *
			 * clause from ___update_load_sum(); this results in
			 * the below usage of @contrib to disappear entirely,
			 * so no point in calculating it.
			 */
			contrib = __accumulate_pelt_segments_copy(periods,
					1024 - sa->period_contrib, delta);
		}
	}
	sa->period_contrib = delta;

	if (load)
		sa->load_sum += load * contrib;
	if (runnable)
		sa->runnable_sum += runnable * contrib << SCHED_CAPACITY_SHIFT;
	if (running)
		sa->util_sum += contrib << SCHED_CAPACITY_SHIFT;
	tracing_counter(TRACE_LEVEL_DEBUG, "update_load_util_sum_2:", sa->util_sum);

	return periods;
}

int
___update_load_sum_copy(u64 now, struct sched_avg *sa,
		  unsigned long load, unsigned long runnable, int running)
{
	u64 delta;

	delta = now - sa->last_update_time;
	kscene_trace_mark_u64("last_update_time", sa->last_update_time);
	kscene_trace_mark_u64("delta_orig", delta);
	tracing_counter(TRACE_LEVEL_DEBUG, "update_load_delta_ns_orig:", delta);
	/*
	 * This should only happen when time goes backwards, which it
	 * unfortunately does during sched clock init when we swap over to TSC.
	 */
	if ((s64)delta < 0) {
		tracing_counter(TRACE_LEVEL_DEBUG, "update_load_delta_N:", delta);
		sa->last_update_time = now;
		return 0;
	}

	tracing_counter(TRACE_LEVEL_DEBUG, "update_load_delta_ns:", delta);
	if (running) {
		if (delta > 4000000) { // max 4ms in running.
			delta = 4000000;
		}
		delta = delta * KSCENE_DELTA_UP_MULTIPLIER;
	} else {
		if (delta > 1000000) { // 1ms
			delta = delta * KSCENE_DELTA_DOWN_MULTIPLIER;
		}
	}
	tracing_counter(TRACE_LEVEL_DEBUG, "update_load_delta_ns_fixed:", delta);

	/*
	 * Use 1024ns as the unit of measurement since it's a reasonable
	 * approximation of 1us and fast to compute.
	 */
	delta >>= 10;
	if (!delta)
		return 0;

	//sa->last_update_time += delta << 10;
	sa->last_update_time = now;

	/*
	 * running is a subset of runnable (weight) so running can't be set if
	 * runnable is clear. But there are some corner cases where the current
	 * se has been already dequeued but cfs_rq->curr still points to it.
	 * This means that weight will be 0 but not running for a sched_entity
	 * but also for a cfs_rq if the latter becomes idle. As an example,
	 * this happens during idle_balance() which calls
	 * update_blocked_averages().
	 *
	 * Also see the comment in accumulate_sum().
	 */
	if (!load)
		runnable = running = 0;

	/*
	 * Now we know we crossed measurement unit boundaries. The *_avg
	 * accrues by two steps:
	 *
	 * Step 1: accumulate *_sum since last_update_time. If we haven't
	 * crossed period boundaries, finish.
	 */
	if (!accumulate_sum_copy(delta, sa, load, runnable, running))
		return 0;

	return 1;
}

void
___update_load_avg_copy(struct sched_avg *sa, unsigned long load)
{
	u32 divider = get_pelt_divider(sa);

	/*
	 * Step 2: update *_avg.
	 */
	sa->load_avg = div_u64(load * sa->load_sum, divider);
	sa->runnable_avg = div_u64(sa->runnable_sum, divider);
	WRITE_ONCE(sa->util_avg, sa->util_sum / divider);
}

int update_rt_rq_load_avg_copy(u64 now, struct sched_avg *sa, int running)
{

	if (___update_load_sum_copy(now, sa,
				running,
				running,
				running)) {

		___update_load_avg_copy(sa, 1);
		return 1;
	}

	return 0;
}