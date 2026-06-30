// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include "apu.h"
#include "apu_ipi.h"
#include "hw_logger.h"

static struct mtk_apu *g_apu;

struct hw_log_level_data {
	unsigned int level;
};

static int check_g_apu_init(void)
{
	if (!g_apu) {
		HWLOGR_ERR("IPI not init yet\n");
		return -EINVAL;
	}
	return 0;
}

int logger_v2_rpc_dump(void)
{
	if (check_g_apu_init())
		return -EINVAL;

	if (g_apu->apu_rpc == NULL)
		return -EINVAL;

	HWLOGR_INFO("RPC_IO_DEBUG: 0x%08x\n",
		ioread32(g_apu->apu_rpc + 0xc));
	HWLOGR_INFO("CE_CTRL_RDATA: 0x%08x\n",
		ioread32(g_apu->apu_rpc + 0x28));

	return 0;
}

int logger_v2_counting_hw_sema_reader_trylock(void)
{
	if (check_g_apu_init())
		return -EINVAL;

	return g_apu->platdata->ops.mbox_counting_hw_sem_reader_trylock(
		g_apu, MBOX_HW_SEMA_RD_KRN_USR_LOGGER);
}

int logger_v2_counting_hw_sema_reader_unlock(void) {
	if (check_g_apu_init())
		return -EINVAL;

	return g_apu->platdata->ops.mbox_counting_hw_sem_reader_unlock(
		g_apu, MBOX_HW_SEMA_RD_KRN_USR_LOGGER);
}

int logger_v2_debug_info_dump(struct seq_file *s)
{
	struct mtk_apu_hw_ops *hw_ops;

	if (check_g_apu_init())
		return -EINVAL;

	hw_ops = &g_apu->platdata->ops;

	if (!hw_ops->debug_info_dump) {
		HWLOGR_DBG("debug_info_dump not support\n");
		return -EINVAL;
	}

	hw_ops->debug_info_dump(g_apu, s);
	return 0;
}

void set_log_level(int level)
{
	int ret;
	struct hw_log_level_data hw_ipi_loglv_data;

	if (check_g_apu_init())
		return;

	hw_ipi_loglv_data.level = level;
	HWLOGR_INFO("set uP debug lv = 0x%x\n", level);

	ret = apu_power_on_off(g_apu->pdev, APU_IPI_LOG_LEVEL, 1, 0);
	if (ret && ret != -EOPNOTSUPP) {
		HWLOGR_ERR("apu_power_on_off fail(%d)\n", ret);
		return;
	}

	ret = apu_ipi_send(g_apu, APU_IPI_LOG_LEVEL,
			&hw_ipi_loglv_data, sizeof(hw_ipi_loglv_data), 1000);
	if (ret)
		HWLOGR_ERR("Failed for hw_logger log level send.\n");

}

static void apu_hw_log_level_ipi_handler(void *data, unsigned int len, void *priv)
{
	int ret;
	unsigned int log_level = *(unsigned int *)data;

	HWLOGR_INFO("log_level = 0x%x (%d)\n", log_level, len);
	ret = apu_power_on_off(g_apu->pdev, APU_IPI_LOG_LEVEL, 0, 1);
	if (ret && ret != -EOPNOTSUPP)
		HWLOGR_ERR("apu_power_on_off fail(%d)\n", ret);
}

int logger_v2_ipi_init(struct mtk_apu *apu)
{
	int ret = 0;
	g_apu = apu;

	ret = apu_ipi_register(g_apu, APU_IPI_LOG_LEVEL, NULL,
			apu_hw_log_level_ipi_handler, NULL);
	if (ret)
		HWLOGR_ERR("Fail in hw_log_level_ipi_init\n");
	return 0;
}

void logger_v2_ipi_remove(struct mtk_apu *apu)
{
	if (check_g_apu_init())
		return;
	apu_ipi_unregister(apu, APU_IPI_LOG_LEVEL);
}