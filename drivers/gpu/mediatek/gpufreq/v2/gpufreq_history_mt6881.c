// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 MediaTek Inc.
 */

/**
 * @file    gpufreq_history_mt6881.c
 * @brief   GPU DVFS History log DB Implementation
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */

#include <linux/sched/clock.h>
#include <linux/string.h>
#include <linux/io.h>

/* GPUFREQ */
#include <gpufreq_v2.h>
#include <gpufreq_history_common.h>
#include <gpufreq_history_mt6881.h>
#include <gpuppm.h>
#include <gpufreq_common.h>

/**
 * ===============================================
 * Variable Definition
 * ===============================================
 */

/**
 * ===============================================
 * Common Function Definition
 * ===============================================
 */

/* API: set target oppidx */
void gpufreq_set_history_target_opp(enum gpufreq_target target, int oppidx)
{

}

/* API: get target oppidx */
int gpufreq_get_history_target_opp(enum gpufreq_target target)
{
	return 0;
}

/**
 * ===============================================
 * External Function Definition
 * ===============================================
 */

/* API: set sel bit */
void __gpufreq_set_sel_bit(unsigned int sel)
{

}

/* API: get sel bit*/
unsigned int __gpufreq_get_sel_bit(void)
{
	return 0;
}

/* API: set delsel bit */
void __gpufreq_set_delsel_bit(unsigned int delsel)
{

}

/* API: get delsel bit*/
unsigned int __gpufreq_get_delsel_bit(void)
{
	return 0;
}

/***********************************************************************************
 *  Function Name      : __gpufreq_record_history_entry
 *  Inputs             : -
 *  Outputs            : -
 *  Returns            : -
 *  Description        : -
 ************************************************************************************/
void __gpufreq_record_history_entry(enum gpufreq_history_state history_state)
{

}

/***********************************************************************************
 * Function Name      : __gpufreq_history_memory_init
 * Inputs             : -
 * Outputs            : -
 * Returns            : -
 * Description        : initialize gpueb log db sysram memory
 ************************************************************************************/
void __gpufreq_history_memory_init(void)
{

}

/***********************************************************************************
 * Function Name      : __gpufreq_history_memory_reset
 * Inputs             : -
 * Outputs            : -
 * Returns            : -
 * Description        : reset gpueb log db sysram memory
 ************************************************************************************/
void __gpufreq_history_memory_reset(void)
{

}

/***********************************************************************************
 * Function Name      : __gpufreq_history_memory_uninit
 * Inputs             : -
 * Outputs            : -
 * Returns            : -
 * Description        : -
 ************************************************************************************/
void __gpufreq_history_memory_uninit(void)
{

}
