// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include "game.h"
#include "game_ec.h"
#include <linux/mutex.h>
#include <linux/module.h>

static int engine_cooler_enable = -1;
static int ec_duration = 2000;
static int lr_frame_time_buffer = 300000;
static int smallest_yield_time;
#define MAX_ENGINE_COOLER_DATA_SIZE 2
#define MAX_HEAVY_THREAD_DATA_SIZE 10
#if !defined(UINT64_MAX)
#define UINT64_MAX ((uint64_t)0xFFFFFFFFFFFFFFFFULL)
#endif

#if !defined(SEC2NSEC)
#define SEC2NSEC (1000000000)
#endif

static int s_tgid;
static struct engine_cooler_data_internal s_ECData;
static struct engine_cooler_heavy_thread_data s_HeavyThreadData[MAX_HEAVY_THREAD_DATA_SIZE];
static struct render_frame_info s_render_info[MAX_RENDER_SIZE];
static bool is_register_fpsgo_cb = false;
static DEFINE_MUTEX(s_ec_lock);
DEFINE_MUTEX(g_ec_private_lock);

module_param(engine_cooler_enable, int, 0644);
module_param(ec_duration, int ,0644);
module_param(lr_frame_time_buffer, int ,0644);
module_param(smallest_yield_time, int, 0644);

void get_mutext_lock(void **lock)
{
	*lock = &g_ec_private_lock;
	(void)lock;
}
EXPORT_SYMBOL(get_mutext_lock);

extern int get_fpsgo_frame_info(int max_num, unsigned long mask,
	int filter_bypass, int tgid, struct render_frame_info *frame_info_arr);

void engine_cooler_get_info(struct engine_cooler_data *ec_data)
{
	struct game_package *pack = NULL;
	unsigned long *query_mask = NULL;
	static uint64_t last_update_time;

	if (!ec_data)
		goto end;

	if (current)
		ec_data->pid = current->pid;

	if (ec_data->pid == 0){
		pr_debug("current pid is zero\n");
		goto end;
	}

	mutex_lock(&s_ec_lock);
	if (ec_data->pid == s_ECData.data.pid)
		memcpy(ec_data, &(s_ECData.data), sizeof(struct engine_cooler_data));
	mutex_unlock(&s_ec_lock);

	if (get_now_time() - last_update_time < SEC2NSEC)
		goto end;

	pack = (struct game_package *)game_alloc_atomic(sizeof(struct game_package));

	if (!pack) {
		pr_debug("out of memory\n");
		goto end;
	}
	pack->event = GAME_EVENT_GET_FPSGO_RENDER_INFO;
	pack->cur_pid = current->pid;
	query_mask = (unsigned long *)game_alloc_atomic(sizeof(unsigned long));
	if (!query_mask) {
		pr_debug("out of memory\n");
		game_free(pack, sizeof(struct game_package));
		goto end;
	}
	*query_mask = (1 << GET_FPSGO_TARGET_FPS | 1 << GET_FPSGO_QUEUE_FPS | 1 << GET_FPSGO_DEP_LIST);
	pack->data = (void *)query_mask;

	if (!game_queue_work(pack))
		last_update_time = get_now_time();

end:
	return;
}
EXPORT_SYMBOL(engine_cooler_get_info);

void engine_cooler_set_last_sleep_duration(int pid, uint64_t duration)
{
	mutex_lock(&s_ec_lock);
	if (pid == s_ECData.data.pid)
		s_ECData.data.last_sleep_duration_ns = duration;
	mutex_unlock(&s_ec_lock);
}
EXPORT_SYMBOL(engine_cooler_set_last_sleep_duration);

int engine_cooler_trigger_duration(void)
{
	return ec_duration * 1000;
}
EXPORT_SYMBOL(engine_cooler_trigger_duration);

static void engine_cooler_cb_from_fpsgo(unsigned long cmd, struct render_frame_info *iter)
{
	if (cmd == 1 << GET_FPSGO_QUEUE_END) {
		if (iter)
			update_engine_cooler_data(iter->pid); //need to check queue_end
	}
}

static void engine_cooler_register_fpsgo_cb(bool enable_register)
{
	struct game_package *pack = NULL;
	pack = (struct game_package *)game_alloc_atomic(sizeof(struct game_package));
	if (!pack) {
		pr_debug("out of memory");
		goto end;
	}
	pack->event = GAME_EVENT_REGISTER_FPSGO_CALLBACK;
	pack->data = game_alloc_atomic(sizeof(FUNC_REGISTER));

	if (!(pack->data)) {
		pr_debug("out of memory");
		game_free(pack, sizeof(struct game_package));
		goto end;
	}
	if (enable_register) {
		((FUNC_REGISTER *)(pack->data))->is_register = true;
		((FUNC_REGISTER *)(pack->data))->mask = 1 << GET_FPSGO_QUEUE_END;
	} else
		((FUNC_REGISTER *)(pack->data))->is_register = false;

	((FUNC_REGISTER *)(pack->data))->register_cb = engine_cooler_cb_from_fpsgo;
	game_queue_work(pack);
end:
	return;
}
	

void engine_cooler_UnRegister_fpsgo_cb(unsigned long cmd, struct render_frame_info *iter)
{
	struct game_package *pack = NULL;
	pack = (struct game_package *)game_alloc_atomic(sizeof(struct game_package));
	if (!pack) {
		pr_debug("out of memory");
		goto end;
	}
	pack->event = GAME_EVENT_REGISTER_FPSGO_CALLBACK;
	pack->data = game_alloc_atomic(sizeof(FUNC_REGISTER));
end:
	return;
}

int is_engine_cooler_enabled(void)
{
	if (engine_cooler_enable > 0) {
		if (!is_register_fpsgo_cb) {
			engine_cooler_register_fpsgo_cb(true);
			is_register_fpsgo_cb = true;
		}
		return 1;
	} else {
		if (is_register_fpsgo_cb) {
			engine_cooler_register_fpsgo_cb(false);
			is_register_fpsgo_cb = false;
		}
		return 0;
	}
}
EXPORT_SYMBOL(is_engine_cooler_enabled);

int engine_cooler_get_lr_frame_time_buffer(void)
{
	return lr_frame_time_buffer;
}
EXPORT_SYMBOL(engine_cooler_get_lr_frame_time_buffer);

int engine_cooler_get_smallest_yield_time(void)
{
	return smallest_yield_time;
}
EXPORT_SYMBOL(engine_cooler_get_smallest_yield_time);

static int get_max_loading_thread(struct render_frame_info *render_info)
{
	int max_dep_pid_loading = 0;
	int max_dep_loading_idx = -1;
	int pid = -1;

	for (int k = 0; k < render_info->dep_num; k++) {
		if (render_info->dep_arr[k].pid == render_info->pid)
			continue;
		if (max_dep_pid_loading < render_info->dep_arr[k].loading) {
			max_dep_pid_loading = render_info->dep_arr[k].loading;
			max_dep_loading_idx = k; //find max dep loading idx
		}
	}
	if (max_dep_loading_idx != -1)
		pid = render_info->dep_arr[max_dep_loading_idx].pid;

	return pid;
}

int get_render_frame_info(struct game_package *pack)
{
	int ret = 0, i = 0, j = 0;
	unsigned long query_mask = 0;
	int render_idx = -1;
	bool find_heavy_target = false;
	int max_loading_pid = -1;
	uint64_t less_heaviest_count = UINT64_MAX;
	int less_heaviest_idx = -1;
	int most_heaviest_count = 0;
	int most_heaviest_idx = -1;


	if (pack->data)
		query_mask = *((unsigned long *)(pack->data));

	ret = get_fpsgo_frame_info(MAX_RENDER_SIZE, query_mask, 1, -1, s_render_info);

	for (j = 0; j < MAX_RENDER_SIZE; j++) {
		for (int k = 0; k < s_render_info->dep_num; k++) {
			if (s_render_info[j].dep_arr[k].pid == pack->cur_pid)
				render_idx = j;
		}
	}
	if (render_idx >= 0)
		max_loading_pid = get_max_loading_thread(&s_render_info[render_idx]);

	if (render_idx < 0 || max_loading_pid < 0) {
		game_print_trace("cannot find specific render & logic");
		goto end;
	}

	mutex_lock(&s_ec_lock);
	if (s_tgid != game_get_tgid(pack->cur_pid)) {
		s_tgid = game_get_tgid(pack->cur_pid);
		memset(&s_HeavyThreadData, 0, sizeof(struct engine_cooler_heavy_thread_data) *
			MAX_HEAVY_THREAD_DATA_SIZE);
	}
	for (i = 0, find_heavy_target = false; i < MAX_HEAVY_THREAD_DATA_SIZE; i++) {
		if (s_HeavyThreadData[i].pid == max_loading_pid) {
			s_HeavyThreadData[i].heaviest_count += 1;
			find_heavy_target = true;
		}
		if (s_HeavyThreadData[i].heaviest_count < less_heaviest_count) {
			less_heaviest_idx = i;
			less_heaviest_count = s_HeavyThreadData[i].heaviest_count;
		}
		if (s_HeavyThreadData[i].heaviest_count > most_heaviest_count) {
			most_heaviest_idx = i;
			most_heaviest_count = s_HeavyThreadData[i].heaviest_count;
		}
		if (s_HeavyThreadData[i].heaviest_count != 0)
			game_print_trace("s_HeavyThreadData[%d] pid = %d heaviest_count=%d", i,
				s_HeavyThreadData[i].pid, s_HeavyThreadData[i].heaviest_count);
	}

	if (less_heaviest_idx != -1 && find_heavy_target == false) {
		s_HeavyThreadData[less_heaviest_idx].pid = max_loading_pid;
		s_HeavyThreadData[less_heaviest_idx].heaviest_count = 1;
	}

	if (most_heaviest_idx != -1 && s_HeavyThreadData[most_heaviest_idx].pid == pack->cur_pid) {
		s_ECData.data.pid = pack->cur_pid;
		s_ECData.data.heaviest_pid = pack->cur_pid;
		s_ECData.data.render_pid = s_render_info[render_idx].pid;
		s_ECData.data.target_fps = s_render_info[render_idx].target_fps;
		if (s_ECData.data.pid != pack->cur_pid)
			s_ECData.data.last_sleep_duration_ns = 0;
	}
	mutex_unlock(&s_ec_lock);

end:
	return ret;
}

void update_engine_cooler_data(int cur_pid)
{
	mutex_lock(&s_ec_lock);
	if (s_ECData.data.render_pid == cur_pid)
		s_ECData.data.last_sleep_duration_ns = 0;
	mutex_unlock(&s_ec_lock);
}

void game_ec_init(void)
{
	mutex_lock(&s_ec_lock);
	memset(&s_ECData, 0, sizeof(struct engine_cooler_data_internal));
	memset(s_HeavyThreadData, 0, sizeof(struct engine_cooler_heavy_thread_data) * MAX_HEAVY_THREAD_DATA_SIZE);
	memset(s_render_info, 0, sizeof(struct render_frame_info) * MAX_RENDER_SIZE);
	mutex_unlock(&s_ec_lock);
}
