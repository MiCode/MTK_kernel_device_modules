// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/sched/clock.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>

#include "mt-plat/aee.h"
#include "iommu_debug.h"
#include "apusys_power.h"
#include "apusys_secure.h"
#include "../apu.h"
#include "../apu_debug.h"
#include "../apu_config.h"
#include "../apu_hw.h"
#include "../apu_excep.h"
#include "apummu_tbl.h"

/* for IPI IRQ affinity tuning*/
static struct cpumask perf_cpus, normal_cpus;

/* for power off timeout detection */
static struct mtk_apu *g_apu;
static struct workqueue_struct *apu_workq;
static struct delayed_work timeout_work;

/*
 * COLD BOOT power off timeout
 *
 * exception will trigger
 * if power on and not power off
 * without ipi send
 */
#define APU_PWROFF_TIMEOUT_MS (90 * 1000)

/*
 * WARM BOOT power off timeout
 *
 * excpetion will trigger
 * if power off not done after ipi send
 */
#define IPI_PWROFF_TIMEOUT_MS (60 * 1000)

#define CHECK_BIT(var, pos) ((var) & (1<<(pos)))

enum APU_EXCEPTION_ID {
	rcx_ao_infra_apb_timeout = 0,
	rcx_ao_infra_apb_secure_vio_irq,
	rcx_infra_apb_timeout,
	rcx_infra_apb_secure_vio_irq,
	apu_idle2max_arb_to_irq,
	mbox_err_irq_0,
	mbox_err_irq_1,
	mbox_err_irq_2,
	mbox_err_irq_3,
	mbox_err_irq_4,
	mbox_err_irq_5,
	mbox_err_irq_6,
	mbox_err_irq_7,
	mbox_err_irq_8,
	mbox_err_irq_9,
	mbox_err_irq_10,
	mbox_err_irq_11,
	mbox_err_irq_12,
	mbox_err_irq_13,
	mbox_err_irq_14,
	mbox_err_irq_15,
	are_abnormal_irq,
	north_mmu_m0_hit_set,
	north_mmu_m1_hit_set,
	south_mmu_m0_hit_set,
	south_mmu_m1_hit_set,
	acx0_infra_apb_timeout,
	acx0_infra_apb_secure_vio_irq,
	reserved_0,
	reserved_1,
	reserved_2,
	reserved_3,
	apu_mmu_cmu_irq
};

static void apu_timeout_work(struct work_struct *work)
{
	struct mtk_apu *apu = g_apu;
	struct device *dev = apu->dev;

	if (apu->bypass_pwr_off_chk) {
		dev_info(dev, "%s: skip aee\n", __func__);
		return;
	}

	apu->bypass_pwr_off_chk = true;
	apusys_rv_aee_warn("APUSYS_RV", "APUSYS_RV_POWER_OFF_TIMEOUT");
}

static uint32_t apusys_rv_smc_call(struct device *dev, uint32_t smc_id,
	uint32_t a2)
{
	struct arm_smccc_res res;

	dev_info(dev, "%s: smc call %d\n",
			__func__, smc_id);

	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL, smc_id,
				a2, 0, 0, 0, 0, 0, &res);
	if (((int) res.a0) < 0)
		dev_info(dev, "%s: smc call %d return error(%ld)\n",
			__func__,
			smc_id, res.a0);

	return res.a0;
}

static int mt6897_rproc_init(struct mtk_apu *apu)
{
	return 0;
}

static int mt6897_rproc_exit(struct mtk_apu *apu)
{
	return 0;
}

static void apu_setup_devapc(struct mtk_apu *apu)
{
	int32_t ret;
	struct device *dev = apu->dev;

	ret = (int32_t)apusys_rv_smc_call(dev,
		MTK_APUSYS_KERNEL_OP_DEVAPC_INIT_RCX, 0);

	dev_info(dev, "%s: %d\n", __func__, ret);
}

static void mt6897_rv_boot_non_secure(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	unsigned long hw_logger_addr = 0;
	unsigned long tcm_addr = 0;

	//apu_setup_devapc(apu);
	dev_info(dev, ">>>>>  calling  rv_boot >>>>\n");
	dev_info(dev, "<%s>  apu->apummu_hwlog_buf_da= 0x%llx\n",
		__func__, (unsigned long long)apu->apummu_hwlog_buf_da);
	//dev_info(dev, "<%s>  apu->md32_tcm = 0x%8x\n", __func__, apu->md32_tcm);
	//apu->apummu_hwlog_buf_da = apu->apummu_hwlog_buf_da >> 12;
	apu->apummu_hwlog_buf_da = apu->apummu_hwlog_buf_da >> 12;
	hw_logger_addr = (u32)(apu->apummu_hwlog_buf_da);
	dev_info(dev, "<%s>  (u32)apu->apummu_hwlog_buf_da= 0x%llx\n",
		__func__, (unsigned long long)apu->apummu_hwlog_buf_da);
	tcm_addr = 0x1d000000;
	tcm_addr = (u32)(tcm_addr >> 12);
	rv_boot(apu->code_da >> 12, 0, hw_logger_addr, eAPUMMU_PAGE_LEN_1MB,
		tcm_addr, eAPUMMU_PAGE_LEN_256KB);

}

static void apu_setup_apummu(struct mtk_apu *apu, int boundary, int ns, int domain)
{
	struct device *dev = apu->dev;

	if (apu->platdata->flags & F_SECURE_BOOT) {
		/* call apummu init smc call */
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_SETUP_APUMMU, 0);
	} else {
		/* call apummu init API (non scure)*/
		mt6897_rv_boot_non_secure(apu);
	}
}

static void apu_reset_mp(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	unsigned long flags;

	if (apu->platdata->flags & F_SECURE_BOOT) {
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_RESET_MP, 0);
	} else {
		spin_lock_irqsave(&apu->reg_lock, flags);
		/* reset uP */
		iowrite32(0, apu->md32_sysctrl + MD32_SYS_CTRL);
		spin_unlock_irqrestore(&apu->reg_lock, flags);

		udelay(10);

		spin_lock_irqsave(&apu->reg_lock, flags);
		/* md32_g2b_cg_en | md32_dbg_en | md32_soft_rstn */
		iowrite32(0xc01, apu->md32_sysctrl + MD32_SYS_CTRL);
		spin_unlock_irqrestore(&apu->reg_lock, flags);
		apu_drv_debug("%s: MD32_SYS_CTRL = 0x%x\n",
			__func__, ioread32(apu->md32_sysctrl + MD32_SYS_CTRL));

		spin_lock_irqsave(&apu->reg_lock, flags);
		/* md32 clk enable */
		iowrite32(0x1, apu->md32_sysctrl + MD32_CLK_EN);
		/* set up_wake_host_mask0 for wdt/mbox irq */
		iowrite32(0x1c0001, apu->md32_sysctrl + UP_WAKE_HOST_MASK0);
		spin_unlock_irqrestore(&apu->reg_lock, flags);
		apu_drv_debug("%s: MD32_CLK_EN = 0x%x\n",
			__func__, ioread32(apu->md32_sysctrl + MD32_CLK_EN));
		apu_drv_debug("%s: UP_WAKE_HOST_MASK0 = 0x%x\n",
			__func__, ioread32(apu->md32_sysctrl + UP_WAKE_HOST_MASK0));
	}
}

static void apu_setup_boot(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	unsigned long flags;
	int boot_from_tcm;

	if (TCM_OFFSET == 0)
		boot_from_tcm = 1;
	else
		boot_from_tcm = 0;

	if (apu->platdata->flags & F_SECURE_BOOT) {
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_SETUP_BOOT, 0);
	} else {
		/* Set uP boot addr to DRAM.
		 * If boot from tcm == 1, boot addr will always map to
		 * 0x1d000000 no matter what value boot_addr is
		 */
		spin_lock_irqsave(&apu->reg_lock, flags);
		if ((apu->platdata->flags & F_BYPASS_IOMMU) ||
			(apu->platdata->flags & F_PRELOAD_FIRMWARE))
			iowrite32((u32)apu->code_da,
				apu->apu_ao_ctl + MD32_BOOT_CTRL);
		else
			iowrite32((u32)CODE_BUF_DA | boot_from_tcm,
				apu->apu_ao_ctl + MD32_BOOT_CTRL);
		spin_unlock_irqrestore(&apu->reg_lock, flags);
		apu_drv_debug("%s: MD32_BOOT_CTRL = 0x%x\n",
			__func__, ioread32(apu->apu_ao_ctl + MD32_BOOT_CTRL));

		spin_lock_irqsave(&apu->reg_lock, flags);
		/* set predefined MPU region for cache access */
		iowrite32(0xAB, apu->apu_ao_ctl + MD32_PRE_DEFINE);
		spin_unlock_irqrestore(&apu->reg_lock, flags);
		apu_drv_debug("%s: MD32_PRE_DEFINE = 0x%x\n",
			__func__, ioread32(apu->apu_ao_ctl + MD32_PRE_DEFINE));
	}
}

static void apu_start_mp(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int i;
	unsigned long flags;

	if (apu->platdata->flags & F_SECURE_BOOT) {
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_START_MP, 0);
	} else {
		spin_lock_irqsave(&apu->reg_lock, flags);
		/* Release runstall */
		iowrite32(0x0, apu->apu_ao_ctl + MD32_RUNSTALL);
		spin_unlock_irqrestore(&apu->reg_lock, flags);

		if ((apu->platdata->flags & F_SECURE_BOOT) == 0)
			for (i = 0; i < 20; i++) {
				dev_info(dev, "apu boot: pc=%08x, sp=%08x\n",
				ioread32(apu->md32_sysctrl + 0x838),
						ioread32(apu->md32_sysctrl+0x840));
				usleep_range(0, 20);
			}
	}
}

static int mt6897_rproc_start(struct mtk_apu *apu)
{
	int ns = 1; /* Non Secure */
	int domain = 0;
	int boundary = (u32) upper_32_bits(apu->code_da);

	apu_setup_devapc(apu);

	apu_setup_apummu(apu, boundary, ns, domain);

	apu_reset_mp(apu);

	apu_setup_boot(apu);

	apu_start_mp(apu);

	return 0;
}

static int mt6897_rproc_stop(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;

	/* Hold runstall */
	if (apu->platdata->flags & F_SECURE_BOOT)
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_STOP_MP, 0);
	else
		iowrite32(0x1, apu->apu_ao_ctl + 8);

	return 0;
}

static int mt6897_apu_power_init(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	struct device_node *np;
	struct platform_device *pdev;
	char wq_name[sizeof("apupwr")];

	/* power dev */
	np = of_parse_phandle(dev->of_node, "mediatek,apusys-power", 0);
	if (!np) {
		dev_info(dev, "failed to parse apusys-power node\n");
		return -EINVAL;
	}

	if (!of_device_is_available(np)) {
		dev_info(dev, "unable to find apusys-power node\n");
		of_node_put(np);
		return -ENODEV;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		dev_info(dev, "apusys-power is not ready yet\n");
		of_node_put(np);
		return -EPROBE_DEFER;
	}

	dev_info(dev, "%s: get power_dev, name=%s\n", __func__, pdev->name);

	apu->power_dev = &pdev->dev;
	of_node_put(np);


	/* apu iommu 0 */
	np = of_parse_phandle(dev->of_node, "apu-iommu0", 0);
	if (!np) {
		dev_info(dev, "failed to parse apu-iommu0 node\n");
		return -EINVAL;
	}

	if (!of_device_is_available(np)) {
		dev_info(dev, "unable to find apu-iommu0 node\n");
		of_node_put(np);
		return -ENODEV;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		dev_info(dev, "apu-iommu0 is not ready yet\n");
		of_node_put(np);
		return -EPROBE_DEFER;
	}

	dev_info(dev, "%s: get apu-iommu0 device, name=%s\n", __func__, pdev->name);

	apu->apu_iommu0 = &pdev->dev;
	of_node_put(np);


	/* apu iommu 1 */
	np = of_parse_phandle(dev->of_node, "apu-iommu1", 0);
	if (!np) {
		dev_info(dev, "failed to parse apu-iommu1 node\n");
		return -EINVAL;
	}

	if (!of_device_is_available(np)) {
		dev_info(dev, "unable to find apu-iommu1 node\n");
		of_node_put(np);
		return -ENODEV;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		dev_info(dev, "apu-iommu1 is not ready yet\n");
		of_node_put(np);
		return -EPROBE_DEFER;
	}

	dev_info(dev, "%s: get apu-iommu1 device, name=%s\n", __func__, pdev->name);

	apu->apu_iommu1 = &pdev->dev;
	of_node_put(np);

	/* init delay worker for power off detection */
	INIT_DELAYED_WORK(&timeout_work, apu_timeout_work);
	apu_workq = alloc_ordered_workqueue(wq_name, WQ_MEM_RECLAIM);
	g_apu = apu;

	return 0;
}

static int mt6897_apu_power_on(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int ret, timeout, i;

	/* to force apu top power on synchronously */
	ret = pm_runtime_get_sync(apu->power_dev);

	/* set_apu_pm_status - power on */
	mtk_iommu_update_pm_status(1, 0, true);
	mtk_iommu_update_pm_status(1, 1, true);

	if (ret < 0) {
		dev_info(dev,
			 "%s: call to get_sync(power_dev) failed, ret=%d\n",
			 __func__, ret);
		/* apusys_rv_aee_warn("APUSYS_RV", "APUSYS_RV_RPM_GET_PWR_ERROR"); */
		return ret;
	}

	/* to notify IOMMU power on */
	/* workaround possible nested disable issue */
	i = 0;
	do {
		ret = pm_runtime_get_sync(apu->apu_iommu0);
		/*try atmost 7 times since disable_depth is 3-bit wide */
		if (ret == -EACCES && i <= 7) {
			pm_runtime_enable(apu->apu_iommu0);
			pm_runtime_put_sync(apu->apu_iommu0);
			i++;
			dev_info(apu->dev,
				 "%s: %s is disabled. Enable and retry(%d)\n",
				 __func__,
				 to_platform_device(apu->apu_iommu0)->name, i);
		} else if (ret < 0)
			goto iommu_get_error;

	} while (ret < 0);

	i = 0;
	/* workaround possible nested disable issue */
	do {
		ret = pm_runtime_get_sync(apu->apu_iommu1);
		/*try atmost 7 times since disable_depth is 3-bit wide */
		if (ret == -EACCES && i <= 7) {
			pm_runtime_enable(apu->apu_iommu1);
			pm_runtime_put_sync(apu->apu_iommu1);
			i++;
			dev_info(apu->dev,
				 "%s: %s is disabled. Enable and retry(%d)\n",
				 __func__,
				 to_platform_device(apu->apu_iommu1)->name, i);
			continue;
		} else if (ret < 0)
			pm_runtime_put_sync(apu->apu_iommu0);

	} while (ret < 0);

iommu_get_error:
	if (ret < 0) {
		dev_info(apu->dev,
			 "%s: call to get_sync(iommu) failed, ret=%d\n",
			 __func__, ret);
		apusys_rv_aee_warn("APUSYS_RV", "APUSYS_IOMMU_RPM_GET_ERROR");
		goto error_put_power_dev;
	}

	/* polling IOMMU rpm state till active */
	timeout = 50000;
	while ((!pm_runtime_active(apu->apu_iommu0) ||
	       !pm_runtime_active(apu->apu_iommu1)) && timeout-- > 0)
		usleep_range(100, 200);
	if (timeout <= 0) {
		dev_info(apu->dev, "%s: polling iommu on timeout!!\n",
			 __func__);
		WARN_ON(0);
		apusys_rv_aee_warn("APUSYS_RV", "APUSYS_RV_IOMMU_ON_TIMEOUT");
		ret = -ETIMEDOUT;
		goto error_put_iommu_dev;
	}

	ret = pm_runtime_get_sync(apu->dev);
	if (ret < 0) {
		dev_info(apu->dev,
			 "%s: call to get_sync(dev) failed, ret=%d\n",
			 __func__, ret);
		apusys_rv_aee_warn("APUSYS_RV", "APUSYS_RV_RPM_GET_ERROR");
		goto error_put_iommu_dev;
	}

	queue_delayed_work(apu_workq,
			   &timeout_work,
			   msecs_to_jiffies(APU_PWROFF_TIMEOUT_MS));

	return 0;

error_put_iommu_dev:
	pm_runtime_put_sync(apu->apu_iommu1);
	pm_runtime_put_sync(apu->apu_iommu0);

error_put_power_dev:
	pm_runtime_put_sync(apu->power_dev);

	return ret;
}

static int mt6897_apu_power_off(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int ret, timeout, i;

	ret = pm_runtime_put_sync(apu->dev);
	if (ret) {
		dev_info(dev,
			 "%s: call to put_sync(dev) failed, ret=%d\n",
			 __func__, ret);
		apusys_rv_aee_warn("APUSYS_RV", "APUSYS_RV_RPM_PUT_ERROR");
		return ret;
	}

	/* to notify IOMMU power off */
	/* workaround possible nested disable issue */
	i = 0;
	do {
		ret = pm_runtime_put_sync(apu->apu_iommu1);
		/*try atmost 7 times since disable_depth is 3-bit wide */
		if (ret == -EACCES && i <= 7) {
			pm_runtime_enable(apu->apu_iommu1);
			pm_runtime_get_sync(apu->apu_iommu1);
			i++;
			dev_info(apu->dev,
				 "%s: %s is disabled. Enable and retry(%d)\n",
				 __func__,
				 to_platform_device(apu->apu_iommu1)->name, i);
		} else if (ret < 0)
			goto iommu_put_error;

	} while (ret < 0);

	i = 0;
	do {
		ret = pm_runtime_put_sync(apu->apu_iommu0);
		/*try atmost 7 times since disable_depth is 3-bit wide */
		if (ret == -EACCES && i <= 7) {
			pm_runtime_enable(apu->apu_iommu0);
			pm_runtime_get_sync(apu->apu_iommu0);
			i++;
			dev_info(apu->dev,
				 "%s: %s is disabled. Enable and retry(%d)\n",
				 __func__,
				 to_platform_device(apu->apu_iommu0)->name, i);
		} else if (ret < 0)
			pm_runtime_get_sync(apu->apu_iommu1);

	} while (ret < 0);

iommu_put_error:
	if (ret < 0) {
		dev_info(apu->dev,
			 "%s: call to put_sync(iommu) failed, ret=%d\n",
			 __func__, ret);
		apusys_rv_aee_warn("APUSYS_RV", "APUSYS_IOMMU_RPM_PUT_ERROR");
		goto error_get_rv_dev;
	}

	/* polling IOMMU rpm state till suspended */
	timeout = 50000;
	while ((!pm_runtime_suspended(apu->apu_iommu0) ||
	       !pm_runtime_suspended(apu->apu_iommu1)) && timeout-- > 0)
		usleep_range(100, 200);
	if (timeout <= 0) {
		dev_info(apu->dev, "%s: polling iommu off timeout!!\n",
			 __func__);
		WARN_ON(0);
		apusys_rv_aee_warn("APUSYS_RV", "APUSYS_RV_IOMMU_OFF_TIMEOUT");
		ret = -ETIMEDOUT;
		goto error_get_iommu_dev;
	}

	/* set_apu_pm_status - power off */
	mtk_iommu_update_pm_status(1, 1, false);
	mtk_iommu_update_pm_status(1, 0, false);

	/* to force apu top power off synchronously */
	ret = pm_runtime_put_sync(apu->power_dev);
	if (ret) {
		dev_info(apu->dev,
			 "%s: call to put_sync(power_dev) failed, ret=%d\n",
			 __func__, ret);
		/* apusys_rv_aee_warn("APUSYS_RV", "APUSYS_RV_RPM_PUT_PWR_ERROR"); */
		goto error_get_iommu_dev;
	}

	/* polling APU TOP rpm state till suspended */
	timeout = 50000;
	while (!pm_runtime_suspended(apu->power_dev) && timeout-- > 0)
		usleep_range(100, 200);
	if (timeout <= 0) {
		dev_info(apu->dev, "%s: polling power off timeout!!\n",
			 __func__);
		WARN_ON(0);
		apusys_rv_aee_warn("APUSYS_RV", "APUSYS_RV_PWRDN_TIMEOUT");
		ret = -ETIMEDOUT;
		goto error_get_power_dev;
	}

	/* clear status & cancel timeout worker */
	apu->bypass_pwr_off_chk = false;
	cancel_delayed_work_sync(&timeout_work);

	return 0;

error_get_power_dev:
	pm_runtime_get_sync(apu->power_dev);
error_get_iommu_dev:
	pm_runtime_get_sync(apu->apu_iommu0);
	pm_runtime_get_sync(apu->apu_iommu1);
error_get_rv_dev:
	pm_runtime_get_sync(apu->dev);

	return ret;
}

static int mt6897_ipi_send_post(struct mtk_apu *apu)
{
	cancel_delayed_work_sync(&timeout_work);

	/* bypass SAPU case */
	if (apu->ipi_id == APU_IPI_SAPU_LOCK)
		apu->bypass_pwr_off_chk = true;

	if (!apu->bypass_pwr_off_chk)
		queue_delayed_work(apu_workq,
			&timeout_work,
			msecs_to_jiffies(IPI_PWROFF_TIMEOUT_MS));

	return 0;
}

static int mt6897_irq_affin_init(struct mtk_apu *apu)
{
	int i;

	/* init perf_cpus mask 0x80, CPU7 only */
	cpumask_clear(&perf_cpus);
	cpumask_set_cpu(7, &perf_cpus);

	/* init normal_cpus mask 0x0f, CPU0~CPU4 */
	cpumask_clear(&normal_cpus);
	for (i = 0; i < 4; i++)
		cpumask_set_cpu(i, &normal_cpus);

	irq_set_affinity_hint(apu->mbox0_irq_number, &normal_cpus);

	return 0;
}

static int mt6897_irq_affin_set(struct mtk_apu *apu)
{
	irq_set_affinity_hint(apu->mbox0_irq_number, &perf_cpus);

	return 0;
}

static int mt6897_irq_affin_unset(struct mtk_apu *apu)
{
	irq_set_affinity_hint(apu->mbox0_irq_number, &normal_cpus);

	return 0;
}

static int mt6897_check_apu_exp_irq(struct mtk_apu *apu, char *ce_module)
{
	struct device *dev = apu->dev;
	int ret = 0;

	ret = apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_OP_DECODE_APU_EXP_IRQ, 0);

	dev_info(dev, "%s: apu_exp_id: %x\n", __func__, ret);

	for (uint32_t i = 0; i < 32; i++) {
		if (CHECK_BIT(ret, i) != 0) {
			switch (i) {
			case are_abnormal_irq:
				if (strcmp("are_abnormal_irq", ce_module) == 0)
					return 1;
				else
					return 0;
				break;
			default:
				break;
			}
		}
	}
	return 0;
}


static int mt6897_apu_memmap_init(struct mtk_apu *apu)
{
	struct platform_device *pdev = apu->pdev;
	struct device *dev = apu->dev;
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_mbox");
	if (res == NULL) {
		dev_info(dev, "%s: apu_mbox get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->apu_mbox = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->apu_mbox)) {
		dev_info(dev, "%s: apu_mbox remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(
		pdev, IORESOURCE_MEM, "md32_sysctrl");
	if (res == NULL) {
		dev_info(dev, "%s: md32_sysctrl get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->md32_sysctrl = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->md32_sysctrl)) {
		dev_info(dev, "%s: md32_sysctrl remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(
		pdev, IORESOURCE_MEM, "md32_debug_apb");
	if (res == NULL) {
		dev_info(dev, "%s: md32_debug_apb get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->md32_debug_apb = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->md32_debug_apb)) {
		dev_info(dev, "%s: md32_debug_apb remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_wdt");
	if (res == NULL) {
		dev_info(dev, "%s: apu_wdt get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->apu_wdt = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->apu_wdt)) {
		dev_info(dev, "%s: apu_wdt remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(
		pdev, IORESOURCE_MEM, "apu_sctrl_reviser");
	if (res == NULL) {
		dev_info(dev, "%s: apu_sctrl_reviser get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->apu_sctrl_reviser = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->apu_sctrl_reviser)) {
		dev_info(dev, "%s: apu_sctrl_reviser remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_ao_ctl");
	if (res == NULL) {
		dev_info(dev, "%s: apu_ao_ctl get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->apu_ao_ctl = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->apu_ao_ctl)) {
		dev_info(dev, "%s: apu_ao_ctl remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "md32_tcm");
	if (res == NULL) {
		dev_info(dev, "%s: md32_tcm get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->md32_tcm = devm_ioremap_wc(dev, res->start, res->end - res->start + 1);
	if (IS_ERR((void const *)apu->md32_tcm)) {
		dev_info(dev, "%s: md32_tcm remap base fail\n", __func__);
		return -ENOMEM;
	}
	apu->md32_tcm_sz = res->end - res->start + 1;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "md32_cache_dump");
	if (res == NULL) {
		dev_info(dev, "%s: md32_cache_dump get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->md32_cache_dump = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->md32_cache_dump)) {
		dev_info(dev, "%s: md32_cache_dump remap base fail\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

static void mt6897_apu_memmap_remove(struct mtk_apu *apu)
{
}

static void mt6897_rv_cg_gating(struct mtk_apu *apu)
{
	iowrite32(0x0, apu->md32_sysctrl + MD32_CLK_EN);
}

static void mt6897_rv_cg_ungating(struct mtk_apu *apu)
{
	iowrite32(0x1, apu->md32_sysctrl + MD32_CLK_EN);
}

static void mt6897_rv_cachedump(struct mtk_apu *apu)
{
	int offset;
	unsigned long flags;

	struct apu_coredump *coredump =
		(struct apu_coredump *) apu->coredump;

	spin_lock_irqsave(&apu->reg_lock, flags);
	/* set APU_UP_SYS_DBG_EN for cache dump enable through normal APB */
	iowrite32(ioread32(apu->md32_sysctrl + DBG_BUS_SEL) |
		APU_UP_SYS_DBG_EN, apu->md32_sysctrl + DBG_BUS_SEL);
	spin_unlock_irqrestore(&apu->reg_lock, flags);

	for (offset = 0; offset < CACHE_DUMP_SIZE/sizeof(uint32_t); offset++)
		coredump->cachedump[offset] =
			ioread32(apu->md32_cache_dump + offset*sizeof(uint32_t));

	spin_lock_irqsave(&apu->reg_lock, flags);
	/* clear APU_UP_SYS_DBG_EN */
	iowrite32(ioread32(apu->md32_sysctrl + DBG_BUS_SEL) &
		~(APU_UP_SYS_DBG_EN), apu->md32_sysctrl + DBG_BUS_SEL);
	spin_unlock_irqrestore(&apu->reg_lock, flags);
}

const struct mtk_apu_platdata mt6897_platdata = {
	.flags		= F_PRELOAD_FIRMWARE | F_AUTO_BOOT |
				F_SECURE_BOOT | F_SECURE_COREDUMP | F_DEBUG_LOG_ON |
				F_APUSYS_RV_TAG_SUPPORT | F_CE_EXCEPTION_ON,
	.ops		= {
		.init	= mt6897_rproc_init,
		.exit	= mt6897_rproc_exit,
		.start	= mt6897_rproc_start,
		.stop	= mt6897_rproc_stop,
		.apu_memmap_init = mt6897_apu_memmap_init,
		.apu_memmap_remove = mt6897_apu_memmap_remove,
		.cg_gating = mt6897_rv_cg_gating,
		.cg_ungating = mt6897_rv_cg_ungating,
		.rv_cachedump = mt6897_rv_cachedump,
		.power_init = mt6897_apu_power_init,
		.power_on = mt6897_apu_power_on,
		.power_off = mt6897_apu_power_off,
		.ipi_send_post = mt6897_ipi_send_post,
		.irq_affin_init = mt6897_irq_affin_init,
		.irq_affin_set = mt6897_irq_affin_set,
		.irq_affin_unset = mt6897_irq_affin_unset,
		.check_apu_exp_irq = mt6897_check_apu_exp_irq,
	},
};
