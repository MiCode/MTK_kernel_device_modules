/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUPPM_H__
#define __GPUPPM_H__

/**************************************************
 * Definition
 **************************************************/
#define LIMITOP(_limiter, _name, _priority, _ceiling, _c_en, _floor, _f_en) \
	{                                      \
		.limiter = _limiter,               \
		.name = _name,                     \
		.priority = _priority,             \
		.ceiling = _ceiling,               \
		.c_enable = _c_en,                 \
		.floor = _floor,                   \
		.f_enable = _f_en,                 \
	}

/**************************************************
 * Enumeration
 **************************************************/
enum gpuppm_priority {
	GPUPPM_PRIO_NONE = 0, /* the lowest priority */
	GPUPPM_PRIO_1    = 1,
	GPUPPM_PRIO_2,
	GPUPPM_PRIO_3,
	GPUPPM_PRIO_4,
	GPUPPM_PRIO_5,
	GPUPPM_PRIO_6,
	GPUPPM_PRIO_7,
	GPUPPM_PRIO_8,
	GPUPPM_PRIO_9, /* the highest priority */
};

enum gpuppm_limit_state {
	LIMIT_DISABLE = 0,
	LIMIT_ENABLE,
};

/**************************************************
 * Structure
 **************************************************/
struct gpuppm_status {
	int ceiling;
	unsigned int c_limiter;
	unsigned int c_priority;
	int floor;
	unsigned int f_limiter;
	unsigned int f_priority;
	int opp_num;
};

/**************************************************
 * Function
 **************************************************/
int gpuppm_init(enum gpufreq_target target, unsigned int gpueb_support);
void gpuppm_set_shared_status(struct gpufreq_shared_status *shared_status);
int gpuppm_limited_commit(enum gpufreq_target target, int oppidx);
int gpuppm_limited_dual_commit(int gpu_oppidx, int stack_oppidx);
int gpuppm_set_limit(enum gpufreq_target target, enum gpuppm_limiter limiter,
	int ceiling_info, int floor_info, unsigned int instant_dvfs);
int gpuppm_switch_limit(enum gpufreq_target target, enum gpuppm_limiter limiter,
	int c_enable, int f_enable, unsigned int instant_dvfs);
void gpuppm_set_stress_test(unsigned int val);
int gpuppm_get_ceiling(void);
int gpuppm_get_floor(void);
unsigned int gpuppm_get_c_limiter(void);
unsigned int gpuppm_get_f_limiter(void);

#endif /* __GPUPPM_H__ */
