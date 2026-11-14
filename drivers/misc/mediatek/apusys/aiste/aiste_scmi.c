// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>

#include "aiste_debug.h"
#include "aiste_scmi.h"

static struct scmi_tinysys_info_st *tinfo;
static int feature_id;

//TODO: Implement different DDR configurations for different platforms.
const struct DdrBoostConfig ddrBoostConfigs[] = {
	{.threshold = 0,   .performance_level = AISTE_PERFORMANCE_OFF}, // Default DDR boost
	{.threshold = 25,  .performance_level = AISTE_PERFORMANCE_L3_ON},
	{.threshold = 50,  .performance_level = AISTE_PERFORMANCE_L2_ON},
	{.threshold = 75,  .performance_level = AISTE_PERFORMANCE_L1_ON}
};

void aiste_scmi_init(unsigned int g_aiste_addr, unsigned int g_aiste_size)
{
	aiste_drv_debug("%s:addr=0x%x,size=0x%x\n", __func__, g_aiste_addr, g_aiste_size);
	int err;

	if ((g_aiste_addr > 0) && (g_aiste_size > 0)) {
		if (!tinfo) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
			tinfo = get_scmi_tinysys_info();
#endif
			if ((IS_ERR_OR_NULL(tinfo)) || (IS_ERR_OR_NULL(tinfo->ph))) {
				aiste_err("%s: tinfo or tinfo->ph is wrong!!\n", __func__);
				tinfo = NULL;
				} else {
					err = of_property_read_u32(tinfo->sdev->dev.of_node,
						"scmi-aiste", &feature_id);
					if (err) {
						aiste_err("%s: get scmi-aiste fail\n", __func__);
						return;
					}

					aiste_drv_debug("%s: get scmi_smi succeed id=%d!!\n",
						__func__, feature_id);

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
					err = scmi_tinysys_common_set(tinfo->ph, feature_id, AISTE_INIT,
						g_aiste_addr, g_aiste_size, 0, 0);
#endif
					if (err)
						aiste_err("%s: call scmi_tinysys_common_set err=%d\n",
						__func__, err);
			}
		}
	}
	aiste_drv_debug("%s++\n", __func__);
}

int aiste_scmi_set(uint16_t ddr_boost)
{
	int err = 0;
	int performance_level = AISTE_PERFORMANCE_OFF;

	/* Find matching configuration based on ddr_boost level */
	for (int i = ARRAY_SIZE(ddrBoostConfigs) - 1; i >= 0; --i) {
		if (ddr_boost >= ddrBoostConfigs[i].threshold) {
			performance_level = ddrBoostConfigs[i].performance_level;
			break;
		}
	}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	err = scmi_tinysys_common_set(tinfo->ph, feature_id, performance_level, 0, 0, 0, 0);
#endif
	if (err) {
		aiste_err("%s: call scmi_tinysys_common_set err=%d\n", __func__, err);
		return -EINVAL;
	}
	aiste_qos_debug("%s: set ddr performance level=%d\n", __func__, performance_level);

	return 0;
}
