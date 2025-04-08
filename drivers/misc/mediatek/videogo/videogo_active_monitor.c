// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/thermal.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/pm_opp.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>

#include "videogo_driver.h"

void get_cpu_opp_tables(void)
{
}

int read_ttj_values(int *cpu_ttj, int *gpu_ttj, int *apu_ttj)
{
	return 0;
}

int videogo_active_fn(void *arg)
{
	struct thermal_zone_device *tz;
	int temp, max_temp, i;
	int temp_bound = 60;
	int cpu_ttj = 0, gpu_ttj = 0, apu_ttj = 0;

	get_cpu_opp_tables();

	while(!kthread_should_stop()) {
		ssleep(5);  // Sleep for 5 seconds

		max_temp = 0;
		for (i = 0; i < ARRAY_SIZE(thermal_zones); i++) {
			// Get the thermal zone device for the CPU
			tz = thermal_zone_get_zone_by_name(thermal_zones[i]);
			if (IS_ERR(tz)) {
				pr_err("Failed to get thermal zone device: %s\n",
					   thermal_zones[i]);
				continue;
			}

			// Read the temperature from the thermal zone
			if (thermal_zone_get_temp(tz, &temp)) {
				pr_err("Failed to read temperature from: %s\n",
					   thermal_zones[i]);
				continue;
			}

			// Convert temperature to degree Celsius
			temp /= 1000;

			pr_info("%s Temperature: %d C\n", thermal_zones[i], temp);

			if (temp > max_temp)
				max_temp = temp;
		}

		// Read TTJ values
		if (read_ttj_values(&cpu_ttj, &gpu_ttj, &apu_ttj) == 0) {
			pr_info("TTJ values - CPU: %d, GPU: %d, APU: %d\n",
					cpu_ttj, gpu_ttj, apu_ttj);
		} else {
			pr_err("Failed to read TTJ values\n");
		}

		// Check thermal status
		bool thermal_status = max_temp > temp_bound;

		if (thermal_status) {
			// Adjust CPU frequency
			// 1. Set Uclamp of Related Thread

			// 2. Adjust CPU Freq.
		}
	}
	return 0;
}
