// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include "apu.h"
#include "apu_config.h"

#include "mvpu_plat.h"
#include "mvpu3_preempt.h"

#define PREEMPT_L1_BUFFER    (0x80000) // 512kB = 512 * 1024
#define PREEMPT_ITCM_BUFFER  (0x40000) // 128kB = 128 * 1024
#define PREEMPT_VUL1_BUFFER  (0x4000)  // 10kB  =  10 * 1024
#define PREEMPT_TOTAL_BUFFER (PREEMPT_L1_BUFFER + PREEMPT_ITCM_BUFFER + PREEMPT_VUL1_BUFFER)

struct mvpu3_preempt_buffer_va {
	uint32_t *backup_buf_va[MAX_CORE_NUM];
};

struct mvpu3_preempt_buffer_iova {
	uint32_t backup_buf_iova[MAX_CORE_NUM];
} __packed;

int mvpu3_preempt_dram_init(struct mtk_apu *apu)
{
	uint32_t   *backup_buf_va;
	dma_addr_t  backup_buf_iova;
	struct mvpu3_preempt_buffer_iova *buffer_iova = (struct mvpu3_preempt_buffer_iova *) get_apu_config_user_ptr(apu->conf_buf, eMVPU_PREEMPT_DATA);
	struct mvpu3_preempt_buffer_va   *buffer_va   = (struct mvpu3_preempt_buffer_va *) (&g_mvpu_platdata->preempt_buffer);

	pr_info("%s core number = %d, sw_preemption_level = 0x%x\n", __func__, g_mvpu_platdata->core_num, g_mvpu_platdata->sw_preemption_level);

	for (uint32_t core_id = 0; core_id < g_mvpu_platdata->core_num; core_id++) {
		backup_buf_va = dma_alloc_coherent(apu->dev, PREEMPT_TOTAL_BUFFER, &backup_buf_iova, GFP_KERNEL);
		if (backup_buf_va == NULL || backup_buf_iova == 0) {
			pr_info("%s: dma_alloc_coherent preempt buffer fail\n", __func__);
			return -ENOMEM;
		}
		memset(backup_buf_va, 0, PREEMPT_TOTAL_BUFFER);

		buffer_va->backup_buf_va[core_id]   = backup_buf_va;
		buffer_iova->backup_buf_iova[core_id] = (uint32_t) backup_buf_iova;

		pr_info("core %d kernel va = 0x%lx, iova = 0x%llx, size = 0x%x\n", core_id, (unsigned long)backup_buf_va, backup_buf_iova, PREEMPT_TOTAL_BUFFER);
	}
	return 0;
}

int mvpu3_preempt_dram_deinit(struct mtk_apu *apu)
{
	struct mvpu3_preempt_buffer_iova *buffer_iova = (struct mvpu3_preempt_buffer_iova *) get_apu_config_user_ptr(apu->conf_buf, eMVPU_PREEMPT_DATA);
	struct mvpu3_preempt_buffer_va   *buffer_va   = (struct mvpu3_preempt_buffer_va *) (&g_mvpu_platdata->preempt_buffer);

	for (uint32_t core_id = 0; core_id < g_mvpu_platdata->core_num; core_id++) {
		if (!buffer_va->backup_buf_va[core_id] || !buffer_iova->backup_buf_iova[core_id]) {
			dma_free_coherent(apu->dev, PREEMPT_TOTAL_BUFFER, buffer_va->backup_buf_va[core_id], buffer_iova->backup_buf_iova[core_id]);
		}
	}
	return 0;
}
