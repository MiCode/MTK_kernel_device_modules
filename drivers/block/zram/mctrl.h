/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2024, X-Ring technologies Inc., All rights reserved.
 */
#ifndef __MCTRL_H
#define __MCTRL_H

#include <linux/list.h>

#define MEMCG_NAME_LEN_MAX	32
#define MEMCG_SCORE_MAX		1000
#define MCTRL_GROUP_MAX		10
#ifdef CONFIG_XRING_SMART_CACHE
#define MEMCG_LEVEL_MAX	3
#endif

struct memcg_ctrl {
	struct mem_cgroup *memcg;

	char name[MEMCG_NAME_LEN_MAX];
	struct list_head list;
	unsigned int score;

	unsigned int comp_ratio;
	unsigned int wb_ratio;
	unsigned int refault_ratio;

#ifdef CONFIG_XRING_SMART_CACHE
	unsigned int level;
	unsigned long max;
#endif

	unsigned long last_fault;
};

struct mctrl_group_param {
	unsigned int min_score;
	unsigned int max_score;
	unsigned int comp_ratio;
	unsigned int wb_ratio;
	unsigned int refault_ratio;
};

#define MEMCG_OEM_DATA(memcg) \
	((struct memcg_ctrl *)(memcg)->android_oem_data1[1])

#define register_android_vh(name) \
	register_trace_android_vh_##name(vh_##name, NULL)
#define unregister_android_vh(name) \
	unregister_trace_android_vh_##name(vh_##name, NULL)

struct mem_cgroup *get_next_memcg(struct mem_cgroup *prev, bool level);
void get_next_memcg_break(struct mem_cgroup *memcg);

#ifdef CONFIG_XRING_SMART_CACHE
struct mem_cgroup *get_cache_memcg(unsigned int level);
#endif
#endif
