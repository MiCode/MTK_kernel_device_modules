// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/io.h>
#include <linux/power_supply.h>
#include <mbraink_modules_ops_def.h>
#include "mbraink_v6989_battery.h"

struct power_supply *power_supply_get_by_name(const char *name);

static void mbraink_v6989_get_battery_info(struct mbraink_battery_data *battery_buffer,
			      long long timestamp)
{
	struct power_supply *psy;
	int ret;
	union power_supply_propval prop;

	psy = power_supply_get_by_name("mtk-gauge");
	if (psy == NULL)
		pr_notice("get battery power supply fail~~~!\n");

	if (psy != NULL) {
		battery_buffer->timestamp = timestamp;

		ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN, &prop);
		battery_buffer->qmaxt = prop.intval;

		ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_ENERGY_FULL, &prop);
		battery_buffer->quse = prop.intval;

		ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_ENERGY_NOW, &prop);
		battery_buffer->precise_soc = prop.intval;

		ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_CAPACITY_LEVEL, &prop);
		battery_buffer->precise_uisoc = prop.intval;
	}

	/*pr_info("timestamp=%lld qmaxt=%d, qusec=%d, socc=%d, uisocc=%d\n",
	 *	battery_buffer->timestamp,
	 *	battery_buffer->qmaxt,
	 *	battery_buffer->quse,
	 *	battery_buffer->precise_soc,
	 *	battery_buffer->precise_uisoc);
	 */
}

static struct mbraink_battery_ops mbraink_v6989_battery_ops = {
	.getBatteryInfo = mbraink_v6989_get_battery_info,
};

int mbraink_v6989_battery_init(void)
{
	int ret = 0;

	ret = register_mbraink_battery_ops(&mbraink_v6989_battery_ops);
	return ret;
}

int mbraink_v6989_battery_deinit(void)
{
	int ret = 0;

	ret = unregister_mbraink_battery_ops();
	return ret;
}

