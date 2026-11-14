// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include "apu.h"
#include "apu_config.h"

#include "mvpu_plat.h"
#include "mvpu2_preempt.h"

#define PREEMPT_L1_BUFFER   (0x80000) // 512kB = 512 * 1024
#define PREEMPT_ITCM_BUFFER (0x40000) // 128kB = 128 * 1024

struct mvpu2_preempt_buffer_va {
	uint32_t *itcm_kernel_addr_core_0[5];
	uint32_t *l1_kernel_addr_core_0[5];
	uint32_t *itcm_kernel_addr_core_1[5];
	uint32_t *l1_kernel_addr_core_1[5];
};

struct mvpu2_preempt_buffer_iova {
	uint32_t itcm_buffer_core_0[5];
	uint32_t l1_buffer_core_0[5];
	uint32_t itcm_buffer_core_1[5];
	uint32_t l1_buffer_core_1[5];
} __packed;

int mvpu2_preempt_dram_init(struct mtk_apu *apu)
{
	uint32_t   *buf_itcm_va  , *buf_l1_va;
	dma_addr_t  buf_itcm_iova,  buf_l1_iova;
	struct mvpu2_preempt_buffer_iova *buffer_iova = (struct mvpu2_preempt_buffer_iova *) get_apu_config_user_ptr(apu->conf_buf, eMVPU_PREEMPT_DATA);
	struct mvpu2_preempt_buffer_va   *buffer_va   = (struct mvpu2_preempt_buffer_va *) (&g_mvpu_platdata->preempt_buffer);

	pr_info("%s core number = %d, sw_preemption_level = 0x%x\n", __func__, g_mvpu_platdata->core_num, g_mvpu_platdata->sw_preemption_level);

	for (uint32_t core_id = 0; core_id < g_mvpu_platdata->core_num; core_id++) {
		for (uint32_t level = 0; level < g_mvpu_platdata->sw_preemption_level; level++) {
			buf_itcm_va = dma_alloc_coherent(apu->dev, PREEMPT_ITCM_BUFFER, &buf_itcm_iova, GFP_KERNEL);
			if (buf_itcm_va == NULL || buf_itcm_iova == 0) {
				pr_info("%s: dma_alloc_coherent fail\n", __func__);
				return -ENOMEM;
			}
			memset(buf_itcm_va, 0, PREEMPT_ITCM_BUFFER);

			buf_l1_va = dma_alloc_coherent(apu->dev, PREEMPT_L1_BUFFER, &buf_l1_iova, GFP_KERNEL);
			if (buf_l1_va == NULL || buf_l1_iova == 0) {
				pr_info("%s: dma_alloc_coherent fail\n", __func__);
				dma_free_coherent(apu->dev, PREEMPT_ITCM_BUFFER, buf_itcm_va, buf_itcm_iova);
				return -ENOMEM;
			}
			memset(buf_l1_va, 0, PREEMPT_L1_BUFFER);

			if (core_id == 0) {
				buffer_va->itcm_kernel_addr_core_0[level] = buf_itcm_va;
				buffer_iova->itcm_buffer_core_0[level]                = (uint32_t) buf_itcm_iova;
				buffer_va->l1_kernel_addr_core_0[level]   = buf_l1_va;
				buffer_iova->l1_buffer_core_0[level]                  = (uint32_t) buf_l1_iova;

			} else if (core_id == 1) {
				buffer_va->itcm_kernel_addr_core_1[level] = buf_itcm_va;
				buffer_iova->itcm_buffer_core_1[level]                = (uint32_t) buf_itcm_iova;
				buffer_va->l1_kernel_addr_core_1[level]   = buf_l1_va;
				buffer_iova->l1_buffer_core_1[level]                  = (uint32_t) buf_l1_iova;

			} else {
				pr_info("core_num error\n");
			}

			pr_info("core %d itcm kernel va = 0x%lx, iova = 0x%llx\n", core_id, (unsigned long)buf_itcm_va, buf_itcm_iova);
			pr_info("core %d l1   kernel va = 0x%lx, iova = 0x%llx\n", core_id, (unsigned long)buf_l1_va  , buf_l1_iova);
		}
	}
	return 0;
}

int mvpu2_preempt_dram_deinit(struct mtk_apu *apu)
{
	struct mvpu2_preempt_buffer_iova *buffer_iova = (struct mvpu2_preempt_buffer_iova *) get_apu_config_user_ptr(apu->conf_buf, eMVPU_PREEMPT_DATA);
	struct mvpu2_preempt_buffer_va   *buffer_va   = (struct mvpu2_preempt_buffer_va *) (&g_mvpu_platdata->preempt_buffer);

	for (uint32_t core_id = 0; core_id < g_mvpu_platdata->core_num; core_id++) {
		for (uint32_t level = 0; level < g_mvpu_platdata->sw_preemption_level; level++) {

			if (core_id == 0) {

				if (!buffer_va->l1_kernel_addr_core_0[level] || !buffer_iova->l1_buffer_core_0[level]) {
					dma_free_coherent(apu->dev, PREEMPT_L1_BUFFER, buffer_va->l1_kernel_addr_core_0[level], buffer_iova->l1_buffer_core_0[level]);
				}

				if (!buffer_va->itcm_kernel_addr_core_0[level] || !buffer_iova->itcm_buffer_core_0[level]) {
					dma_free_coherent(apu->dev, PREEMPT_ITCM_BUFFER, buffer_va->itcm_kernel_addr_core_0[level], buffer_iova->itcm_buffer_core_0[level]);
				}

			} else if (core_id == 1) {
				if (!buffer_va->l1_kernel_addr_core_1[level] || !buffer_iova->l1_buffer_core_1[level]) {
					dma_free_coherent(apu->dev, PREEMPT_L1_BUFFER, buffer_va->l1_kernel_addr_core_1[level], buffer_iova->l1_buffer_core_1[level]);
				}

				if (!buffer_va->itcm_kernel_addr_core_1[level] || !buffer_iova->itcm_buffer_core_1[level]) {
					dma_free_coherent(apu->dev, PREEMPT_ITCM_BUFFER, buffer_va->itcm_kernel_addr_core_1[level], buffer_iova->itcm_buffer_core_1[level]);
				}
			}

		}
	}
	return 0;
}
