// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/sched/clock.h>

#include "apu_config.h"
#include "apusys_secure.h"
#include "hw_logger.h"
#include "logger_v2.h"
#include "logger_v2_ipi.h"
#include "logger_v2_procfs.h"
#include "logger_v2_addr.h"

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
#include <mt-plat/mrdump.h>
#define add_mrdump(va, pa, size, name) {\
	(void)mrdump_mini_add_extra_file((unsigned long)va, pa, size, name); }
#else
#define add_mrdump(...)
#endif


/* control register ioremap address */
void *apu_logtop, *apu_mbox;
static struct platform_device *g_pdev;

enum SMC_OP_APU_LOG_DBG {
	SMC_OP_APU_LOG_DBG_NULL = 0,
	SMC_OP_APU_LOG_DBG_OUT_TO_HOST_MSK,
	SMC_OP_APU_LOG_DBG_WAKE_HOST_MSK,
	SMC_OP_APU_LOG_DBG_OUT_TO_HOST_STA_0,
	SMC_OP_APU_LOG_DBG_OUT_TO_HOST_STA_1,
};

struct log_buf_info
{
	char *va;
	unsigned long pa;
	dma_addr_t iova;
	unsigned int size;
	bool allocated;
};
static struct log_buf_info np_log_buf;
static unsigned int intr_lbc_size;
static unsigned int log_irq_num;
static unsigned int irq_hdl_cnt;
static int last_irq_reader_lock;
static unsigned int burst_intr_cnt;

static unsigned int ioread_debug_atf(enum SMC_OP_APU_LOG_DBG dbg_read_op)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL,
		MTK_APUSYS_KERNEL_OP_APUSYS_LOGTOP_DBG_READ,
		dbg_read_op, 0, 0, 0, 0, 0, &res);

	HWLOGR_DBG("arm_smccc_smc dbg_read a0 a1 / 0x%lx 0x%lx\n", res.a0, res.a1);

	if (res.a0 != 0) {
		HWLOGR_ERR("arm_smccc_smc dbg_read error ret: 0x%lx", res.a0);
		return 0;
	}
	return res.a1;
}

void logger_v2_irq_debug_status_dump(void)
{
	logger_v2_rpc_dump();

	HWLOGR_INFO("OUT_TO_HOST_MSK: 0x%08x\n",
		ioread_debug_atf(SMC_OP_APU_LOG_DBG_OUT_TO_HOST_MSK));
	HWLOGR_INFO("WAKE_HOST_MSK: 0x%08x\n",
		ioread_debug_atf(SMC_OP_APU_LOG_DBG_WAKE_HOST_MSK));
	HWLOGR_INFO("OUT_TO_HOST_STA_0: 0x%08x\n",
		ioread_debug_atf(SMC_OP_APU_LOG_DBG_OUT_TO_HOST_STA_0));
	HWLOGR_INFO("OUT_TO_HOST_STA_1: 0x%08x\n",
		ioread_debug_atf(SMC_OP_APU_LOG_DBG_OUT_TO_HOST_STA_1));

	mt_irq_dump_status(log_irq_num);
}

void logger_v2_buf_invalidate(enum LOG_BUFF_TYPE buff_type)
{
	HWLOGR_DBG("+");
	if (!g_pdev)
		return;

	switch (buff_type) {
	case LOG_BUFF_NP:
		dma_sync_single_for_cpu(
			&g_pdev->dev, np_log_buf.iova,
			np_log_buf.size, DMA_FROM_DEVICE);
		break;
	default:
		HWLOGR_ERR("Error LOG_BUFF_TYPE (%d)\n", buff_type);
		break;
	}
	HWLOGR_DBG("-");
}

void logger_v2_clear_buf(enum LOG_BUFF_TYPE buff_type)
{
	uint64_t timestamp_ns;

	HWLOGR_DBG("+");
	if (!g_pdev)
		return;

	switch (buff_type) {
	case LOG_BUFF_NP:
		timestamp_ns = sched_clock();
		memset(np_log_buf.va, 0, np_log_buf.size);
		snprintf(np_log_buf.va, HWLOG_LINE_SIZE,
			"[%llu.%llu][APMCU] log buffer clear by user!\n",
			timestamp_ns / 1000000000, (timestamp_ns % 1000000000) / 1000);
		dma_sync_single_for_cpu(
			&g_pdev->dev, np_log_buf.iova,
			np_log_buf.size, DMA_TO_DEVICE);
		break;
	default:
		HWLOGR_ERR("Error LOG_BUFF_TYPE (%d)\n", buff_type);
		break;
	}
	HWLOGR_DBG("-");
}

int logger_v2_get_buf_info(enum LOG_BUFF_TYPE buff_type,
	char **buf_base, unsigned int *buf_size)
{
	switch (buff_type) {
	case LOG_BUFF_NP:
		*buf_base = np_log_buf.va;
		*buf_size = np_log_buf.size;
		break;
	default:
		HWLOGR_ERR("Error LOG_BUFF_TYPE (%d)\n", buff_type);
		break;
	}
	return 0;
}

unsigned int logger_v2_get_w_ofs(void)
{
	int reader_lock;
	unsigned int w_ptr_reg = 0, w_ofs = 0;

	/* reader_lock return value:
	*   0: semaphore acquired successfully
	*   -EINVAL: semaphore operation error
	*   -EBUSY: semaphore acquired fail(npu off)
	*/
	reader_lock = logger_v2_counting_hw_sema_reader_trylock();

	if (reader_lock == 0) {
		w_ptr_reg = ioread32(APU_LOG_BUF_W_PTR);
		logger_v2_counting_hw_sema_reader_unlock();
	} else if (reader_lock != -EBUSY) {
		HWLOGR_INFO("hw_sema_reader_trylock operation error: %d\n", reader_lock);
	}

	if (w_ptr_reg == 0)
		w_ofs = ioread32(LOG_W_OFS_MBOX);
	else
		w_ofs = ((unsigned long long)w_ptr_reg << 4) - np_log_buf.iova;

	return w_ofs;
}

static irqreturn_t apu_logtop_irq_handler(int irq, void *private_data)
{
	int reader_lock;
	unsigned int ctrl_flag = 0, w_ptr_reg = 0, r_ptr_reg = 0;
	unsigned long long handle_time = sched_clock();

	// check apu power on and w1c reg
	reader_lock = logger_v2_counting_hw_sema_reader_trylock();

	if (reader_lock == 0) {
		ctrl_flag = ioread32(APU_LOGTOP_CON_ADDR);
		r_ptr_reg = ioread32(APU_LOG_BUF_R_PTR);
		w_ptr_reg = ioread32(APU_LOG_BUF_W_PTR);
		iowrite32(ctrl_flag, APU_LOGTOP_CON_ADDR);
		iowrite32(w_ptr_reg, APU_LOG_BUF_R_PTR);
		HWLOGR_DBG("w1c ctrl_flag = 0x%x\n", ctrl_flag);

		logger_v2_counting_hw_sema_reader_unlock();
		logger_v2_notify_mblog(0);
		handle_time = sched_clock() - handle_time;

		if ((irq_hdl_cnt++ % 100 == 0) || (handle_time > 1000000)) {
			HWLOGR_INFO(
				"ctrl_flag = 0x%x r_ptr = 0x%x w_ptr = 0x%x cnt = %d time = %lld us\n",
				ctrl_flag, r_ptr_reg, w_ptr_reg, irq_hdl_cnt, handle_time / 1000);
		}
		if ((ctrl_flag & APU_LOGTOP_CON_MSK) == 0) {
			HWLOGR_INFO("no irq pending: ctrl_flag = 0x%x burst_intr_cnt = %d\n",
				ctrl_flag, burst_intr_cnt);
			burst_intr_cnt++;
		} else {
			burst_intr_cnt = 0;
		}
	} else if (reader_lock == -EBUSY)  {
		if (last_irq_reader_lock != -EBUSY) {
			HWLOGR_INFO("apu power off / s5 idle burst_intr_cnt = %d\n",
				burst_intr_cnt);
		}
		burst_intr_cnt++;
	} else {
		HWLOGR_INFO("hw_sema_reader_trylock operation error: %d\n", reader_lock);
	}

	last_irq_reader_lock = reader_lock;
	return IRQ_HANDLED;
}

static int logger_v2_config_init(struct mtk_apu *apu)
{
	struct logger_init_info *st_logger_init_info;

	HWLOGR_INFO("+");
	if (!apu || !apu->conf_buf) {
		HWLOGR_ERR("invalid argument: apu\n");
		return -EINVAL;
	}

	if (!apu_logtop) {
		HWLOGR_ERR("not probe yet\n");
		return -EFAULT;
	}

	st_logger_init_info = (struct logger_init_info *)
		get_apu_config_user_ptr(apu->conf_buf, eLOGGER_INIT_INFO);

	if (np_log_buf.allocated) {
		st_logger_init_info->iova =
			lower_32_bits(np_log_buf.iova);
		st_logger_init_info->iova_h =
			upper_32_bits(np_log_buf.iova);
		apu->apummu_hwlog_buf_da = np_log_buf.iova;
		st_logger_init_info->lbc_sz = intr_lbc_size;
		st_logger_init_info->buf_sz = np_log_buf.size;
		st_logger_init_info->burst_len = HWLOG_LINE_SIZE;

		HWLOGR_INFO("set st_logger_init_info iova = 0x%x, "
					"iova_h = 0x%x, lbc_sz = 0x%x, "
					"buf_sz = 0x%x, burst_len = 0x%x\n",
					st_logger_init_info->iova,
					st_logger_init_info->iova_h,
					st_logger_init_info->lbc_sz,
					st_logger_init_info->buf_sz,
					st_logger_init_info->burst_len)
	}
	HWLOGR_INFO("-");
	return 0;
}

static int logger_v2_buf_alloc(struct platform_device *pdev)
{
	int ret = 0;

	ret = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(64));
	if (ret) {
		HWLOGR_ERR("dma_set_coherent_mask fail (%d)\n", ret);
		ret = -ENOMEM;
		goto out;
	}

	np_log_buf.va = kzalloc(np_log_buf.size, GFP_KERNEL);
	if (!np_log_buf.va) {
		ret = -ENOMEM;
		goto out;
	}
	np_log_buf.pa = __pa_nodebug(np_log_buf.va);
	add_mrdump(np_log_buf.va, np_log_buf.pa,
		np_log_buf.size, "APUSYS_LOG");

	np_log_buf.iova = dma_map_single(&pdev->dev, np_log_buf.va,
		np_log_buf.size, DMA_FROM_DEVICE);
	ret = dma_mapping_error(&pdev->dev, np_log_buf.iova);
	if (ret) {
		HWLOGR_ERR("dma_map_single fail for np_log_buf.iova (%d)\n", ret);
		ret = -ENOMEM;
		goto out;
	}
	np_log_buf.allocated = true;
	HWLOGR_INFO("np_log_buf.size = %d\n", np_log_buf.size);
	HWLOGR_INFO("np_log_buf.va = %p\n", np_log_buf.va);
	HWLOGR_INFO("np_log_buf.pa = 0x%08lx\n", np_log_buf.pa);
	HWLOGR_INFO("np_log_buf.iova = 0x%08llx\n", np_log_buf.iova);

out:
	return ret;
}

static int logger_v2_buf_free(struct platform_device *pdev)
{
	HWLOGR_INFO("+\n");
	if (np_log_buf.iova) {
		dma_unmap_single(&pdev->dev, np_log_buf.iova, np_log_buf.size,
			DMA_FROM_DEVICE);
		np_log_buf.iova = 0;
	}

	if (np_log_buf.va) {
		kfree(np_log_buf.va);
		np_log_buf.va = NULL;
		np_log_buf.pa = 0;
	}

	return 0;
}

static int logger_v2_irq_init(struct platform_device *pdev)
{
	int ret;
	log_irq_num = platform_get_irq_byname(pdev, "apu_logtop");

	HWLOGR_INFO("log_irq_num = %d\n", log_irq_num);

	ret = devm_request_threaded_irq(&pdev->dev, log_irq_num,
		NULL, apu_logtop_irq_handler,
		irq_get_trigger_type(log_irq_num) | IRQF_ONESHOT,
		pdev->name, NULL);

	if (ret) {
		HWLOGR_ERR("failed to request IRQ (%d)\n", ret);
		log_irq_num = 0;
	}

	return ret;
}

static int logger_v2_ioremap(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_logtop");
	if (res == NULL) {
		HWLOGR_ERR("apu_logtop get resource fail\n");
		ret = -ENODEV;
		goto out;
	}

	apu_logtop = ioremap(res->start, res->end - res->start + 1);
	if (IS_ERR((void const *)apu_logtop)) {
		HWLOGR_ERR("apu_logtop remap base fail\n");
		ret = -ENOMEM;
		goto out;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_mbox");
	if (res == NULL) {
		HWLOGR_ERR("apu_mbox get resource fail\n");
		goto out;
	}

	apu_mbox = ioremap(res->start, res->end - res->start + 1);
	if (IS_ERR((void const *)apu_mbox)) {
		HWLOGR_ERR("apu_mbox remap base fail\n");
		goto out;
	}

out:
	return ret;
}

static int logger_v2_iounmap(struct platform_device *pdev)
{
	if (apu_mbox) {
		iounmap(apu_mbox);
		apu_mbox = NULL;
	}
	if (apu_logtop) {
		iounmap(apu_logtop);
		apu_logtop = NULL;
	}
	return 0;
}

static int logger_v2_disable_irq(struct platform_device *pdev)
{
	HWLOGR_INFO("+");
	if (log_irq_num) {
		HWLOGR_INFO("disable hw logger irq\n");
		disable_irq(log_irq_num);
		log_irq_num = 0;
	}
	return 0;
}

static int logger_v2_load_dts(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;

	if (of_property_read_u32(np, "log-buf-sz", &np_log_buf.size)) {
		HWLOGR_INFO("get log-buf-sz failed\n");
		np_log_buf.size = 3 * 1024 * 1024;
	}
	HWLOGR_INFO("np_log_buf.size : %u\n", np_log_buf.size);

	if (of_property_read_u32(np, "interrupt-lbc-sz", &intr_lbc_size)) {
		HWLOGR_INFO("get interrupt-lbc-sz failed\n");
		intr_lbc_size = 64 * HWLOG_LINE_SIZE;
	}
	HWLOGR_INFO("intr_lbc_size : %u\n", intr_lbc_size);

	return 0;
}

static int logger_v2_probe(struct platform_device *pdev)
{
	int ret = 0;
	HWLOGR_INFO("+");
	g_pdev = pdev;

	ret = logger_v2_load_dts(pdev);
	if (ret) {
		HWLOGR_ERR("logger_v2_load_dts fail: ret=%d\n", ret);
		goto out;
	}

	ret = logger_v2_ioremap(pdev);
	if (ret) {
		HWLOGR_ERR("logger_v2_ioremap fail: ret=%d\n", ret);
		goto remove_ioremap;
	}

	ret = logger_v2_buf_alloc(pdev);
	if (ret) {
		HWLOGR_ERR("logger_v2_buf_alloc fail: ret=%d\n", ret);
		goto remove_log_buf;
	}

	ret = logger_v2_create_procfs(pdev);
	if (ret) {
		HWLOGR_ERR("logger_v2_create_procfs fail: ret=%d\n", ret);
		goto remove_procfs;
	}

	ret = logger_v2_irq_init(pdev);
	if (ret) {
		HWLOGR_ERR("logger_v2_irq_init fail: ret=%d\n", ret);
		goto disable_irq;
	}

	HWLOGR_INFO("-");
	return 0;

disable_irq:
	logger_v2_disable_irq(pdev);

remove_procfs:
	logger_v2_remove_procfs(pdev);

remove_log_buf:
	logger_v2_buf_free(pdev);

remove_ioremap:
	logger_v2_iounmap(pdev);

out:
	g_pdev = NULL;
	HWLOGR_ERR("hw_logger probe error!!!\n");
	return ret;
}

static void logger_v2_shutdown(struct platform_device *pdev)
{
	HWLOGR_INFO("+");
	logger_v2_disable_irq(pdev);
	logger_v2_remove_procfs(pdev);
	logger_v2_buf_free(pdev);
	logger_v2_iounmap(pdev);
	g_pdev = NULL;
}

static int logger_v2_remove(struct platform_device *pdev)
{
	logger_v2_shutdown(pdev);
	return 0;
}

const struct mtk_apu_logger_platdata logger_v2_platdata = {
	.ops = {
		.v1_ops = NULL,
		.probe = logger_v2_probe,
		.remove = logger_v2_remove,
		.shutdown = logger_v2_shutdown,
		.config_init = logger_v2_config_init,
		.ipi_init = logger_v2_ipi_init,
		.ipi_remove = logger_v2_ipi_remove,
	},
};