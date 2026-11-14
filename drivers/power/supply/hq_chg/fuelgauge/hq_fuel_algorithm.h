#ifndef __HQ_FUEL_ALGORITHM_H
#define __HQ_FUEL_ALGORITHM_H

#include <linux/ktime.h>
#include "hq_charger_manager.h"

#define MONITOR_SOC_WAIT_MS             1000
#define INIT_MONITOR_SOC_WAIT_MS        10000
#define BATT_MA_AVG_SAMPLES             8
#define INITIAL_TARGET_CAPACITY         -2
#define SOC_REACH_100_RATE_NUMERATOR(x) (fg_soc +100 - x + 1)
#define SOC_REACH_100_RATE_DENOMINATOR(x) (100 - x)

struct fuel_algo_data {
	int time_now_s;
	int last_time_s;
	int delta_time_s;
	int soc;
	int OptimalSoc;
	int last_OptimalSoc;
	int last_chg_status;
	int heavy_curr;
	int normal_temp;
	int soc_update_rate_fast;
	int soc_update_rate_normal;
	int soc_update_rate_low;
	int batt_id_count;
	int soc_reach_100_rate;
	int *cycle_range_data;
	int cycle_range_length;
	int *temp_range_data;
	int temp_range_length;
	int **cycle_soc_reach_100_rate;
	int **temp_soc_reach_100_rate;
	struct delayed_work soc_monitor_work;
};

#endif


