/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#ifndef VIDEOGO_PARAM_CONFIG_H
#define VIDEOGO_PARAM_CONFIG_H

#include <linux/module.h>
static bool mtk_vgo_debug;
module_param(mtk_vgo_debug, bool, 0644);

// Transcoding Scenario
static bool mtk_vgo_uclamp_min_ta = true;
module_param(mtk_vgo_uclamp_min_ta, bool, 0644);

static int mtk_vgo_uclamp_min_ta_val = 100;
module_param(mtk_vgo_uclamp_min_ta_val, int, 0644);

static bool mtk_vgo_gpu_freq_min = true;
module_param(mtk_vgo_gpu_freq_min, bool, 0644);

static int mtk_vgo_gpu_freq_min_opp = 7;
module_param(mtk_vgo_gpu_freq_min_opp, int, 0644);

static bool mtk_vgo_ct_to_vip = true;
module_param(mtk_vgo_ct_to_vip, bool, 0644);

// VP low power
static bool mtk_vgo_margin_control = true;
module_param(mtk_vgo_margin_control, bool, 0644);

static bool mtk_vgo_runnable_boost_disable = true;
module_param(mtk_vgo_runnable_boost_disable, bool, 0644);

static bool mtk_vgo_util_est_boost = true;
module_param(mtk_vgo_util_est_boost, bool, 0644);

static bool mtk_vgo_rt_non_idle_preempt = true;
module_param(mtk_vgo_rt_non_idle_preempt, bool, 0644);

// Instance device open
static bool mtk_vgo_runnable_boost_enable = true;
module_param(mtk_vgo_runnable_boost_enable, bool, 0644);

static bool mtk_vgo_cgroup_colocate = true;
module_param(mtk_vgo_cgroup_colocate, bool, 0644);
#endif // VIDEOGO_PARAM_CONFIG_H
