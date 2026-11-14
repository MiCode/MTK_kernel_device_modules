// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/of_device.h>
#include <linux/dma-mapping.h>

#include "apu.h"

void apu_mem_remove(struct mtk_apu *apu)
{
	if ((apu->platdata->flags & F_DEBUG_MEM_SUPPORT) &&
		((apu->platdata->flags & F_BYPASS_IOMMU) == 0)) {
		dma_unmap_single(apu->dev, apu->debug_memory_iova, PAGE_SIZE, DMA_FROM_DEVICE);
	}
}

int apu_mem_init(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int ret;

	if ((apu->platdata->flags & F_BYPASS_IOMMU) == 0) {
		ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));
		if (ret) {
			dev_info(dev, "%s: dma_set_mask_and_coherent fail(%d)\n", __func__, ret);
			return -ENOMEM;
		}

		if (apu->platdata->flags & F_DEBUG_MEM_SUPPORT) {
			apu->debug_memory_iova = dma_map_single(dev, apu->debug_memory, PAGE_SIZE,
				DMA_FROM_DEVICE);

			ret = dma_mapping_error(dev, apu->debug_memory_iova);
			if (ret) {
				dev_info(dev, "%s: dma_map_single fail for debug_memory_iova (%d)\n", __func__, ret);
				return -ENOMEM;
			}

			dev_info(dev, "%s: debug_memory_iova = 0x%llx\n",
				__func__, apu->debug_memory_iova);
		}
	}

	return 0;
}
