// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include "game.h"
#include "game_ec.h"
#include <linux/mutex.h>
#include <linux/module.h>

static int engine_cooler_enable = -1;
static int yield_duration = 2000;
static int lr_frame_time_buffer = 300000;
static int smallest_yield_time;
#define MAX_ENGINE_COOLER_DATA_SIZE 2
#if !defined(UINT64_MAX)
#define UINT64_MAX ((uint64_t)0xFFFFFFFFFFFFFFFFULL)
#endif

#if !defined(SEC2NSEC)
#define SEC2NSEC (1000000000)
#endif

static struct engine_cooler_data_internal s_ECData[MAX_ENGINE_COOLER_DATA_SIZE];
static bool is_register_fpsgo_cb = false;
static DEFINE_MUTEX(s_ec_lock);
DEFINE_MUTEX(g_ec_private_lock);

module_param(engine_cooler_enable, int, 0644);
module_param(yield_duration, int ,0644);
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
	int i = 0;
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
	for (i = 0; i < MAX_ENGINE_COOLER_DATA_SIZE; i++) {

		if (s_ECData[i].data.pid == 0)
			continue;

		if (ec_data->pid == s_ECData[i].data.pid) {
			memcpy(ec_data, &(s_ECData[i].data), sizeof(struct engine_cooler_data));
			break;
		}
	}
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
	for (int i = 0; i < MAX_ENGINE_COOLER_DATA_SIZE; i++) {
		if (s_ECData[i].data.pid == pid) {
			s_ECData[i].data.last_sleep_duration_ns = duration;
			break;
		}
	}
	mutex_unlock(&s_ec_lock);
}
EXPORT_SYMBOL(engine_cooler_set_last_sleep_duration);

int engine_cooler_get_firtst_sleep_duration(void)
{
	return yield_duration * 1000;
}
EXPORT_SYMBOL(engine_cooler_get_firtst_sleep_duration);

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

int get_render_frame_info(struct game_package *pack)
{
	int ret = 0, i = 0, j = 0, k = 0;
	unsigned long query_mask = 0;
	int max_dep_pid_loading = 0;
	int max_dep_loading_idx = 0;
	int render_idx = -1;
	int oldest_update_idx = 0;
	uint64_t oldest_update_time = UINT64_MAX;
	bool find_render_target = false;
	struct render_frame_info *render_info = NULL;

	if (pack->data)
		query_mask = *((unsigned long *)(pack->data));

	render_info = game_alloc_atomic(sizeof(struct render_frame_info) * MAX_RENDER_SIZE);
	if (!render_info) {
		pr_debug("%s: allocate memory failed\n", __func__);
		goto end;
	}
	ret = get_fpsgo_frame_info(MAX_RENDER_SIZE, query_mask, 1, -1, render_info);
	for (j = 0; j < MAX_RENDER_SIZE; j++) {
		max_dep_pid_loading = 0;
		max_dep_loading_idx = 0;
		for (k = 0; k < render_info[j].dep_num; k++) {
			if (max_dep_pid_loading < render_info[j].dep_arr[k].loading) {
				max_dep_pid_loading = render_info[j].dep_arr[k].loading;
				max_dep_loading_idx = k; //find max dep loading idx
			}

			if (render_info[j].dep_arr[max_dep_loading_idx].pid == pack->cur_pid)
				render_idx = j;
		}
		if (render_idx != -1)
			break;
	}

	if (render_idx == -1)
		goto end;

	mutex_lock(&s_ec_lock);
	for (i = 0, find_render_target = false; i < MAX_ENGINE_COOLER_DATA_SIZE; i++) {
		if (s_ECData[i].data.pid == pack->cur_pid) {
			s_ECData[i].data.heaviest_pid = render_info[render_idx].dep_arr[max_dep_loading_idx].pid;
			s_ECData[i].data.render_pid = render_info[render_idx].pid;
			s_ECData[i].data.target_fps = render_info[render_idx].target_fps;
			s_ECData[i].latest_update_timestamp = get_now_time();
			find_render_target = true;
			game_print_trace("find target cur_pid = %d, render_idx = %d, render_pid = %d, heaviest_pid = %d, target_fps = %d",
				pack->cur_pid, render_idx, s_ECData[i].data.render_pid, s_ECData[i].data.heaviest_pid, s_ECData[i].data.target_fps);
			break;
		}

		if (oldest_update_time > s_ECData[i].latest_update_timestamp) {
			oldest_update_time = s_ECData[i].latest_update_timestamp;
			oldest_update_idx = i;
		}
	}

	if (find_render_target == false) {
		memset(&(s_ECData[oldest_update_idx]), 0, sizeof(struct engine_cooler_data_internal));
		s_ECData[oldest_update_idx].latest_update_timestamp = get_now_time();
		s_ECData[oldest_update_idx].data.pid = pack->cur_pid;
		s_ECData[oldest_update_idx].data.heaviest_pid =
			render_info[render_idx].dep_arr[max_dep_loading_idx].pid;
		s_ECData[oldest_update_idx].data.last_sleep_duration_ns = 0;
		s_ECData[oldest_update_idx].data.render_pid = render_info[render_idx].pid;
		s_ECData[oldest_update_idx].data.target_fps = render_info[render_idx].target_fps;
		game_print_trace("Re-asign target cur_pid = %d, render_idx = %d, render_pid = %d, heaviest_pid = %d, target_fps = %d",
			pack->cur_pid, render_idx, s_ECData[oldest_update_idx].data.render_pid,
			s_ECData[oldest_update_idx].data.heaviest_pid,
			s_ECData[oldest_update_idx].data.target_fps);
	}
	mutex_unlock(&s_ec_lock);

end:
	game_free(render_info, sizeof(struct render_frame_info) * MAX_RENDER_SIZE);
	return ret;
}

void update_engine_cooler_data(int cur_pid)
{
	int i = 0;

	mutex_lock(&s_ec_lock);
	for (i = 0; i < MAX_ENGINE_COOLER_DATA_SIZE; i++) {
		if (s_ECData[i].data.render_pid == cur_pid) {
			s_ECData[i].data.last_sleep_duration_ns = 0;
			break;
		}
	}
	mutex_unlock(&s_ec_lock);
}
