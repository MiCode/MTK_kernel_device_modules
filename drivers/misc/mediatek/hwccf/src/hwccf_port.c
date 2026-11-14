// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <hwccf_provider.h> /* Kernel only */
#include <hwccf_provider_data.h> /* Kernel only */

/* hwccf are declared in AP View, which needs to be remapped on different XPU */
uint32_t hwccf_remap(uint32_t addr)
{
	uint32_t remap_addr;

	if (addr == 0) {
		HWCCF_ERR("%s: addr Null\n", __func__);
		configASSERT(0);
	}

	/* hwccf address remap */
	remap_addr = addr;

	return remap_addr;
}

void hwccf_write(struct regmap* regmap, uint32_t ofs, uint32_t value)
{
	if (regmap == NULL) {
		HWCCF_ERR("%s: regmap is NULL\n", __func__);
		configASSERT(0);
		return;
	}

	HWCCF_DBG("RG_Write ofs(0x%x) = 0x%x\n", ofs, value);

	regmap_write(regmap, ofs, value);
}

void hwccf_update_bit(struct regmap* regmap, uint32_t ofs, uint32_t mask, uint32_t value)
{
	if (regmap == NULL) {
		HWCCF_ERR("%s: regmap is NULL\n", __func__);
		configASSERT(0);
		return;
	}

	HWCCF_DBG("RG_Update ofs(0x%x) with mask 0x%x, value 0x%x\n", ofs, mask, value);

	if (regmap_update_bits(regmap, ofs, mask, value) != 0) {
		HWCCF_ERR("%s: Failed to update bits in regmap at offset 0x%x\n", __func__, ofs);
	}
}

#ifdef HWCCF_TEST_MODE
uint32_t hwccf_read(struct regmap* regmap, uint32_t ofs)
{
	uint32_t rval;
	int ret;

	if (regmap == NULL) {
		HWCCF_ERR("%s: regmap is NULL\n", __func__);
		configASSERT(0);
		return -HWV_TEST_EARLY_RET;
    }

	ret = regmap_read(regmap, ofs, &rval);

	if (ret != 0) {
		HWCCF_ERR("%s: regmap_read failed with error %d\n", __func__, ret);
		configASSERT(0);
		return -HWV_TEST_EARLY_RET;
	}

	HWCCF_DBG("RG_Read ofs(0x%x) = 0x%x\n", ofs, rval);

	return -HWV_TEST_EARLY_RET;
}
#else
uint32_t hwccf_read(struct regmap* regmap, uint32_t ofs)
{
	uint32_t rval;
	int ret;

	if (regmap == NULL) {
		HWCCF_ERR("%s: regmap is NULL\n", __func__);
		configASSERT(0);
		return -HWV_REGMAP_NULL;
	}

	ret = regmap_read(regmap, ofs, &rval);

	if (ret != 0) {
		HWCCF_ERR("%s: regmap_read failed with error %d\n", __func__, ret);
		configASSERT(0);
		return -HWV_READ_FAIL;
	}

	HWCCF_DBG("RG_Read ofs(0x%x) = 0x%x\n", ofs, rval);

	return rval;
}
#endif

hwccf_time_t hwccf_get_time_us(void)
{
	ktime_t kt = ktime_get();
	return ktime_to_us(kt);
}
