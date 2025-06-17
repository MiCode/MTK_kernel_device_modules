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

static unsigned int mtk_vgo_uclamp_min_ta_val = 100;
module_param(mtk_vgo_uclamp_min_ta_val, uint, 0644);

static bool mtk_vgo_gpu_freq_min = true;
module_param(mtk_vgo_gpu_freq_min, bool, 0644);

static unsigned int mtk_vgo_gpu_freq_min_opp = 7;
module_param(mtk_vgo_gpu_freq_min_opp, uint, 0644);

static bool mtk_vgo_ct_to_vip = true;
module_param(mtk_vgo_ct_to_vip, bool, 0644);

// VP low power
static bool mtk_vgo_margin_ctrl = true;
module_param(mtk_vgo_margin_ctrl, bool, 0644);

static unsigned int mtk_vgo_margin_ctrl_val[3];
static int num_margin_ctrl = ARRAY_SIZE(mtk_vgo_margin_ctrl_val);
module_param_array(mtk_vgo_margin_ctrl_val, uint, &num_margin_ctrl, 0644);

static bool mtk_vgo_runnable_boost_disable = true;
module_param(mtk_vgo_runnable_boost_disable, bool, 0644);

static bool mtk_vgo_util_est_boost_disable;
module_param(mtk_vgo_util_est_boost_disable, bool, 0644);

static bool mtk_vgo_rt_non_idle_preempt = true;
module_param(mtk_vgo_rt_non_idle_preempt, bool, 0644);

static bool mtk_vgo_cpu_pf_ctrl = true;
module_param(mtk_vgo_cpu_pf_ctrl, bool, 0644);

static bool mtk_vgo_slc_wce_ctrl = true;
module_param(mtk_vgo_slc_wce_ctrl, bool, 0644);

// Instance device open
static bool mtk_vgo_runnable_boost_enable = true;
module_param(mtk_vgo_runnable_boost_enable, bool, 0644);

static bool mtk_vgo_cgroup_colocate = true;
module_param(mtk_vgo_cgroup_colocate, bool, 0644);
#endif // VIDEOGO_PARAM_CONFIG_H
