
#include "hq_fuel_algorithm.h"
#include "hq_printk.h"
#include "hq_fg_class.h"

#if IS_ENABLED(CONFIG_HUAQIN_SOC_REACH_100_ALGO_SUPPORT)

static int get_temp_index(struct fuel_algo_data *data, int temp)
{
	int index = 0, i;

	for (i = 0; i < data->temp_range_length; i++) {
		if (temp >= data->temp_range_data[i])
			index = i;
	}

	return index;
}

static int get_cycle_count_index(struct fuel_algo_data *data, int cycle_count)
{
	int index = 0, i;

	for (i = 0; i < data->cycle_range_length; i++) {
		if (cycle_count >= data->cycle_range_data[i])
			index = i;
	}

	return index;
}

static int fg_soc_reaching_100_algo(void)
{
	int fg_soc;
	int final_soc;
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager;
	struct fuel_algo_data *data;
	int temp_index = 0, cycle_index = 0, batt_id = 0, ret;
	union power_supply_propval pval = {0, };

	batt_psy = power_supply_get_by_name("battery");
	if (IS_ERR_OR_NULL(batt_psy)) {
		hq_err("get battery psy fail\n");
		return -EINVAL;
	}

	manager = (struct charger_manager *)power_supply_get_drvdata(batt_psy);
	if (IS_ERR_OR_NULL(manager) || IS_ERR_OR_NULL(manager->fuel_gauge)) {
		hq_err("get charger manager or fg psy fail\n");
		return -EINVAL;
	}
	data = manager->fuel_algo;

	if (IS_ERR_OR_NULL(manager->fuel_algo->temp_soc_reach_100_rate)) {
		hq_err("temp_soc_reach_100_rate is err\n");
		return -EINVAL;
	}

	#if IS_ENABLED(CONFIG_ONEWIRE_GPIO)
	if (IS_ERR_OR_NULL(manager->auth_dev)) {
		manager->auth_dev = get_batt_auth_by_name("secret_ic");
		if (IS_ERR_OR_NULL(manager->auth_dev)) {
			hq_err("%s: manager->auth_dev is_err_or_null\n");
			return -EINVAL;
		}
	}
	#endif
	batt_id = manager->auth_dev->battery_id;

	ret = power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if (ret < 0) {
		hq_err("Couldn't read chg cycle, ret=%d\n", ret);
		return -EINVAL;
	}
	cycle_index = get_cycle_count_index(data, pval.intval);
	temp_index = get_temp_index(data, manager->tbat);

	data->soc_reach_100_rate = data->cycle_soc_reach_100_rate[cycle_index][batt_id] + data->temp_soc_reach_100_rate[temp_index][batt_id];

	fg_soc = fuel_gauge_get_rsoc(manager->fuel_gauge);
	if (fg_soc <= 0) {
		hq_err("get capacity from mtk gauge fail\n");
		return -EINVAL;
	}

	if (fg_soc <= 100)
		final_soc = 1;
	else
		final_soc = SOC_REACH_100_RATE_NUMERATOR(data->soc_reach_100_rate) /
				SOC_REACH_100_RATE_DENOMINATOR(data->soc_reach_100_rate);
	if (final_soc >= 100)
		final_soc = 100;
	hq_info("final soc1 = %d, fg_soc = %d, soc_reach_100_rate = %d",final_soc, fg_soc, data->soc_reach_100_rate);
	return final_soc;
}
#endif

#if IS_ENABLED(CONFIG_HUAQIN_SOC_SMOOTH_SUPPORT)
static int calculate_delta_time(struct fuel_algo_data *data)
{
	/* default to delta time = 0 if anything fails */
	data->delta_time_s = 0;
	data->time_now_s = ktime_get_seconds();
	data->delta_time_s = (data->time_now_s - data->last_time_s);

	return 0;
}

static void calculate_average_current(int batt_current, int *batt_ma_avg )
{
	static int samples_index = 0, samples_num = 0, batt_ma_avg_samples[BATT_MA_AVG_SAMPLES];
	static int batt_ma_prev = 0;
	static int last_batt_ma_avg = 0;
	int sum_ma = 0;
	int i;

	if(batt_current == batt_ma_prev)
		goto unchanged;
	else
		batt_ma_prev = batt_current;

	batt_ma_avg_samples[samples_index] = batt_current;
	samples_index = (samples_index + 1) % BATT_MA_AVG_SAMPLES;
	samples_num += 1;

	if(samples_num >= BATT_MA_AVG_SAMPLES)
		samples_num = BATT_MA_AVG_SAMPLES;

	for( i = 0; i <  samples_num; i++){
		sum_ma += batt_ma_avg_samples[i];
	}
	last_batt_ma_avg = sum_ma / samples_num;

unchanged:
	*batt_ma_avg = last_batt_ma_avg;
}

static int fg_battery_soc_smooth_tracking()
{

  	int batt_ma_avg;
	int delta_time = 0;
	int soc_changed = 0;
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager;
	struct fuel_algo_data *data;

	batt_psy = power_supply_get_by_name("battery");
	if (IS_ERR_OR_NULL(batt_psy)) {
		hq_err("get battery psy fail\n");
		return -EINVAL;
	}

	manager = (struct charger_manager *)power_supply_get_drvdata(batt_psy);
	if (IS_ERR_OR_NULL(manager)) {
		hq_err("get charger manager or fg psy fail\n");
		return -EINVAL;
	}
	data = manager->fuel_algo;
	if (data->soc <= 0)
		return -EINVAL;

	/* initial variable */
	if (data->last_time_s == 0)
		data->last_time_s = ktime_get_seconds();
	if (data->OptimalSoc == INITIAL_TARGET_CAPACITY) {
		data->OptimalSoc = data->soc;
		data->last_OptimalSoc = data->OptimalSoc;
	}

	/* smooth algorithm */
	calculate_delta_time(data);
	calculate_average_current((manager->ibat / 1000), &batt_ma_avg);

	if (manager->tbat > data->normal_temp) {
	/*  Battery in normal temperature */
		if (batt_ma_avg > data->heavy_curr)
		/* Heavy loading current, ignore battery soc limit*/
			delta_time = data->delta_time_s / data->soc_update_rate_fast;
		else if (batt_ma_avg < 0 || abs (data->OptimalSoc - data->soc) > 2)
			delta_time = data->delta_time_s / data->soc_update_rate_normal;
		else
			delta_time = data->delta_time_s / data->soc_update_rate_low;
	} else {
		/* Calculated average current > HEAVY_LOADING_CURRENT */
		if (batt_ma_avg > data->heavy_curr ||  abs (data->OptimalSoc - data->soc) > 2)
			delta_time = data->delta_time_s / data->soc_update_rate_fast;
		else
			delta_time = data->delta_time_s / data->soc_update_rate_normal;
	}

	if (delta_time < 0)
		delta_time = 0;
	soc_changed = min(1, delta_time);
	if(manager->chg_status != data->last_chg_status) {
		data->last_time_s = data->time_now_s;
		soc_changed = 0;
	}

	#if IS_ENABLED(CONFIG_HUAQIN_ERP_SUPPORT)
	if (data->OptimalSoc >= 0) {
		if (manager->product_name_index == EEA){
			if (data->OptimalSoc < 100 &&  manager->chg_status == POWER_SUPPLY_STATUS_FULL)
				data->OptimalSoc = data->OptimalSoc + soc_changed;
			else if (data->OptimalSoc < data->soc && manager->ibat < 0)
				/* Battery in charging status
				* update the soc when resuming device
				*/
				if(data->OptimalSoc == 99 && manager->chg_status != POWER_SUPPLY_STATUS_FULL)
					hq_info("ERP:in charging, hold 99 until charge status is full.");
				else
					data->OptimalSoc = data->OptimalSoc + soc_changed;
			else if (data->OptimalSoc > data->soc && manager->ibat > 0) {
					/* Battery in discharging status
					* update the soc when resuming device
					*/
					data->OptimalSoc  = data->OptimalSoc - soc_changed;
					if (data->OptimalSoc <= 1)
						data->OptimalSoc = 1;
			}
		} else {
			if (data->OptimalSoc < 100 &&  manager->chg_status == POWER_SUPPLY_STATUS_FULL)
				data->OptimalSoc = data->OptimalSoc + soc_changed;
			else if (data->OptimalSoc < data->soc && manager->ibat < 0)
				/* Battery in charging status
				* update the soc when resuming device
				*/
				data->OptimalSoc = data->OptimalSoc + soc_changed;
			else if (data->OptimalSoc > data->soc && manager->ibat > 0 &&
					manager->chg_status != POWER_SUPPLY_STATUS_FULL) {
					/* Battery in discharging status
					* update the soc when resuming device
					*/
					data->OptimalSoc  = data->OptimalSoc - soc_changed;
					if (data->OptimalSoc <= 1)
						data->OptimalSoc = 1;
			}
		}
	}
	#else
	if (data->OptimalSoc >= 0) {
		if (data->OptimalSoc < 100 &&  manager->chg_status == POWER_SUPPLY_STATUS_FULL)
			data->OptimalSoc = data->OptimalSoc + soc_changed;
		else if (data->OptimalSoc < data->soc && manager->ibat < 0)
			/* Battery in charging status
			* update the soc when resuming device
			*/
			data->OptimalSoc = data->OptimalSoc + soc_changed;
		else if (data->OptimalSoc > data->soc && manager->ibat > 0 &&
				manager->chg_status != POWER_SUPPLY_STATUS_FULL) {
			/* Battery in discharging status
			* update the soc when resuming device
			*/
			data->OptimalSoc  = data->OptimalSoc - soc_changed;
			if (data->OptimalSoc <= 1)
				data->OptimalSoc = 1;
		}
	}
	#endif
	else {
		data->OptimalSoc = 0;
	}

	if(data->last_OptimalSoc != data->OptimalSoc ){
		data->last_time_s = data->time_now_s;
		data->last_OptimalSoc = data->OptimalSoc;
		if(manager->batt_psy)
			power_supply_changed(manager->batt_psy);
	}
	data->last_chg_status = manager->chg_status;

	hq_info("OptimalSoc = %d, Monotonicsoc = %d, soc_changed = %d, batt_ma_avg = %d",data->OptimalSoc, data->soc, soc_changed, batt_ma_avg);
	return 0;
}
#endif

static void batt_soc_monitor_work(struct work_struct *work)
{
	struct fuel_algo_data *data = container_of(work, struct fuel_algo_data, soc_monitor_work.work);
#if (IS_ENABLED(CONFIG_HUAQIN_SOC_SMOOTH_SUPPORT) && !IS_ENABLED(CONFIG_HUAQIN_SOC_REACH_100_ALGO_SUPPORT))
	struct power_supply *bms_psy = NULL;
	union power_supply_propval pval = {0, };
	int ret = 0;

	bms_psy = power_supply_get_by_name("bms");
	if (IS_ERR_OR_NULL(bms_psy)) {
		hq_err("get bms psy fail\n");
		goto err;
	}
	ret = power_supply_get_property(bms_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0) {
		hq_err("get capacity from bms psy fail\n");
		return -EINVAL;
	}
	data->soc = pval.intval;
	fg_battery_soc_smooth_tracking();
err:
#elif (IS_ENABLED(CONFIG_HUAQIN_SOC_SMOOTH_SUPPORT) && IS_ENABLED(CONFIG_HUAQIN_SOC_REACH_100_ALGO_SUPPORT))
	data->soc = fg_soc_reaching_100_algo();
	fg_battery_soc_smooth_tracking();
#elif IS_ENABLED(CONFIG_HUAQIN_SOC_REACH_100_ALGO_SUPPORT)
	data->OptimalSoc = fg_soc_reaching_100_algo();
#else
	hq_info("No algorithm support\n");
	return;
#endif
	schedule_delayed_work(&data->soc_monitor_work, msecs_to_jiffies(MONITOR_SOC_WAIT_MS));
}

static int hq_fuel_algo_parse_dts(struct charger_manager *manager)
{
	#if (IS_ENABLED(CONFIG_HUAQIN_SOC_SMOOTH_SUPPORT) || IS_ENABLED(CONFIG_HUAQIN_SOC_REACH_100_ALGO_SUPPORT))
	struct device_node *node = manager->dev->of_node;
	struct fuel_algo_data *data = manager->fuel_algo;
	#if IS_ENABLED(CONFIG_HUAQIN_SOC_REACH_100_ALGO_SUPPORT)
	int cycle_soc_rate_length = 0, temp_soc_rate_length = 0;
	int row, col, array_rows = 0, array_cols = 0;
	#endif
	#endif
	int ret = 0;

	#if IS_ENABLED(CONFIG_HUAQIN_SOC_SMOOTH_SUPPORT)
	ret |= of_property_read_u32(node, "hq_fuel_algo,heavy_loading_current", &data->heavy_curr);
	ret |= of_property_read_u32(node, "hq_fuel_algo,normal_temp", &data->normal_temp);
	ret |= of_property_read_u32(node, "hq_fuel_algo,soc_update_rate_fast", &data->soc_update_rate_fast);
	ret |= of_property_read_u32(node, "hq_fuel_algo,soc_update_rate_normal", &data->soc_update_rate_normal);
	ret |= of_property_read_u32(node, "hq_fuel_algo,soc_update_rate_low", &data->soc_update_rate_low);
	#endif
	#if IS_ENABLED(CONFIG_HUAQIN_SOC_REACH_100_ALGO_SUPPORT)
	ret |= of_property_read_u32(node, "hq_fuel_algo,batt_id_count", &data->batt_id_count);
	if (ret < 0)
		return ret;

	data->temp_range_length = of_property_count_elems_of_size(node, "hq_fuel_algo,temp_range_data", sizeof(u32));
	if (data->temp_range_length < 0) {
		hq_err("failed to read total_length of temp_range_length\n");
		return data->temp_range_length;
	}

	data->cycle_range_length = of_property_count_elems_of_size(node, "hq_fuel_algo,cycle_range_data", sizeof(u32));
	if (data->cycle_range_length < 0) {
		hq_err("failed to read total_length of cycle_range_data\n");
		return data->cycle_range_length;
	}

	cycle_soc_rate_length = of_property_count_elems_of_size(node, "hq_fuel_algo,cycle_soc_reach_100_rate", sizeof(u32));
	if (cycle_soc_rate_length < 0) {
		hq_err("failed to read total_length of cycle_soc_rate_length\n");
		return cycle_soc_rate_length;
	}

	temp_soc_rate_length = of_property_count_elems_of_size(node, "hq_fuel_algo,temp_soc_reach_100_rate", sizeof(u32));
	if (temp_soc_rate_length < 0) {
		hq_err("failed to read total_length of temp_soc_reach_100_rate\n");
		return temp_soc_rate_length;
	}

	data->temp_range_data = (int *)devm_kzalloc(manager->dev, sizeof(int) * data->temp_range_length, GFP_KERNEL);
	if (IS_ERR_OR_NULL(data->temp_range_data)) {
		hq_err("malloc data->temp_range_data fail\n");
		goto err_temp_range_data;
	}

	data->cycle_range_data = (int *)devm_kzalloc(manager->dev, sizeof(int) * cycle_soc_rate_length, GFP_KERNEL);
	if (IS_ERR_OR_NULL(data->cycle_range_data)) {
		hq_err("malloc data->cycle_range_data fail\n");
		goto err_cycle_range_data;
	}
	
	array_rows = cycle_soc_rate_length / data->batt_id_count;
	array_cols = data->batt_id_count;
	data->cycle_soc_reach_100_rate = (int **)devm_kzalloc(manager->dev, sizeof(int*) * array_rows, GFP_KERNEL);
	if (IS_ERR_OR_NULL(data->cycle_soc_reach_100_rate)) {
		hq_err("malloc data->cycle_soc_reach_100_rate\n");
		goto err_cycle_soc_reach_100_rate;
	}
	for (row = 0; row < array_rows; row++) {
		data->cycle_soc_reach_100_rate[row] = (int *)devm_kzalloc(manager->dev, sizeof(int) * array_cols, GFP_KERNEL);
		if (IS_ERR_OR_NULL(data->cycle_soc_reach_100_rate[row])) {
			hq_err("malloc cycle_data->vol_data[%d] fail\n", row);
			goto err_cycle_soc_reach_100_rate;
		}
		for (col = 0; col < array_cols; col++) {
			ret = of_property_read_u32_index(node, "hq_fuel_algo,cycle_soc_reach_100_rate",
				row * array_cols + col, &(data->cycle_soc_reach_100_rate[row][col]));
			if (ret) {
				hq_err("failed to parse cycle_vol_data\n");
				goto err_cycle_soc_reach_100_rate;
			}
		}
	}

	array_rows = temp_soc_rate_length / data->batt_id_count;
	array_cols = data->batt_id_count;
	data->temp_soc_reach_100_rate = (int **)devm_kzalloc(manager->dev, sizeof(int*) * array_rows, GFP_KERNEL);
	if (IS_ERR_OR_NULL(data->temp_soc_reach_100_rate)) {
		hq_err("malloc data->cycle_soc_reach_100_rate\n");
		goto err_temp_soc_reach_100_rate;
	}
	for (row = 0; row < array_rows; row++) {
		data->temp_soc_reach_100_rate[row] = (int *)devm_kzalloc(manager->dev, sizeof(int) * array_cols, GFP_KERNEL);
		if (IS_ERR_OR_NULL(data->temp_soc_reach_100_rate[row])) {
			hq_err("malloc cycle_data->vol_data[%d] fail\n", row);
			goto err_temp_soc_reach_100_rate;
		}
		for (col = 0; col < array_cols; col++) {
			ret = of_property_read_u32_index(node, "hq_fuel_algo,temp_soc_reach_100_rate",
				row * array_cols + col, &(data->temp_soc_reach_100_rate[row][col]));
			if (ret) {
				hq_err("failed to parse cycle_vol_data\n");
				goto err_temp_soc_reach_100_rate;
			}
		}
	}

	ret |= of_property_read_u32_array(node, "hq_fuel_algo,temp_range_data", data->temp_range_data, data->temp_range_length);
	if (ret)
	{
		hq_err("failed to parse temp_data\n");
		goto err_temp_soc_reach_100_rate;
	}

	ret |= of_property_read_u32_array(node, "hq_fuel_algo,cycle_range_data", data->cycle_range_data, data->cycle_range_length);
	if (ret)
	{
		hq_err("failed to parse temp_data\n");
		goto err_temp_soc_reach_100_rate;
	}

	return ret;

err_temp_soc_reach_100_rate:
	for (row = 0; row < temp_soc_rate_length / data->batt_id_count; row++) {
		if (!IS_ERR_OR_NULL(data->temp_soc_reach_100_rate[row]))
			devm_kfree(manager->dev, data->temp_soc_reach_100_rate[row]);
		data->temp_soc_reach_100_rate[row] = NULL;
	}
	devm_kfree(manager->dev, data->temp_soc_reach_100_rate);
	data->temp_soc_reach_100_rate = NULL;
err_cycle_soc_reach_100_rate:
	for (row = 0; row < cycle_soc_rate_length / data->batt_id_count; row++) {
		if (!IS_ERR_OR_NULL(data->cycle_soc_reach_100_rate[row]))
			devm_kfree(manager->dev, data->cycle_soc_reach_100_rate[row]);
		data->temp_soc_reach_100_rate[row] = NULL;
	}
	devm_kfree(manager->dev, data->temp_soc_reach_100_rate);
	data->temp_soc_reach_100_rate = NULL;
err_cycle_range_data:
	devm_kfree(manager->dev, data->cycle_range_data);
	data->cycle_range_data = NULL;
err_temp_range_data:
	devm_kfree(manager->dev, data->temp_range_data);
	data->temp_range_data = NULL;
	#endif

	return ret;
}

int hq_fuel_algo_init(struct charger_manager *manager)
{
	int ret = 0;
	manager->fuel_algo = devm_kzalloc(manager->dev, sizeof(struct fuel_algo_data), GFP_KERNEL);

	if (IS_ERR_OR_NULL(manager->fuel_algo)) {
		hq_err("alloc fuel_algo_data fail");
		return -ENOMEM;
	}

	manager->fuel_algo->OptimalSoc = INITIAL_TARGET_CAPACITY;
	manager->fuel_algo->last_chg_status = POWER_SUPPLY_STATUS_UNKNOWN;
	ret = hq_fuel_algo_parse_dts(manager);
	if (ret < 0) {
		hq_err("parse device tree fail");
		return -EINVAL;
	}

	INIT_DELAYED_WORK(&manager->fuel_algo->soc_monitor_work, batt_soc_monitor_work);
	schedule_delayed_work(&manager->fuel_algo->soc_monitor_work, msecs_to_jiffies(MONITOR_SOC_WAIT_MS));

	return 0;
}
EXPORT_SYMBOL(hq_fuel_algo_init);

MODULE_DESCRIPTION("Huaqin fuel algo");
MODULE_LICENSE("GPL v2");
/*
 * hq fuelgauge algorithm Release Note
 * 3.0.0
 * (1) Add CONFIG_HUAQIN_ERP_SUPPORT to control ERP.
 * Distinguish between the EU region and other regions based on the board ID.
 * 99% of charging is not smoothed, and the full state during discharging is not used as a judgment condition.
 *
 * 2.0.0
 * Enable CONFIG_HUAQIN_SOC_REACH_100_ALGO_SUPPORT toAdjust the true SOC of the electricity meter to 
 * correspond to 100% of the UISOC,It can be adjusted by setting SOC_REACH_100_RATE. For example, 
 * if the value of SOC_REACH_100_RATE is 3, then when the SOC of the electricity meter is 97, the UISOC is 100.
 *
 * 1.0.0
 * (1) Add driver for hq fuelgauge algorithm
 * (2) Enable CONFIG_HUAQIN_FUEL_ALGORITHM to load the driver,
 *     start executing the hq_fuel_algo_init and soc_monitor_work functions
 * (3) Enable CONFIG_HUAQIN_SOC_SMOOTH_SUPPORT to load the SOC smoothing function,
 *     batt_soc_monitor_work will execute the fg_battery_soc_smooth_tracing function to perform soc smoothing
 *
 */
