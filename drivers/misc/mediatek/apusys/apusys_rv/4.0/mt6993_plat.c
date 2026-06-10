// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/sched/clock.h>
#include <linux/sched.h>
#include <linux/of_platform.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/seq_file.h>
#include <linux/dma-mapping.h>
#include <uapi/linux/sched/types.h>
#include "mt-plat/aee.h"

#include "apusys_rv_trace.h"

#include "apusys_power.h"
#include "apusys_secure.h"
#include "apu_regdump.h"
#include "../apu.h"
#include "../apu_debug.h"
#include "../apu_config.h"
#include "../apu_hw.h"
#include "../apu_excep.h"
#include "../apu_ipi.h"

#include "mt6993_plat.h"

#include "apummu_tbl.h"

static struct mutex reader_hw_sema_lock;
static unsigned int cnting_hw_sema_ref_cnt[MBOX_HW_SEMA_RD_KRN_USR_MAX];

typedef enum {
	APU_HW_SEM_SYS_APU = 0UL,
	APU_HW_SEM_SYS_GZ = 1UL,
	APU_HW_SEM_SYS_SCP = 3UL,
	APU_HW_SEM_SYS_APMCU = 11UL,
} APU_MBOX_HW_SEM_SYS_ID;

typedef enum {
	APU_COUNTING_HW_SEM_READER_SYS_KERNEL = 0UL,
	APU_COUNTING_HW_SEM_READER_SYS_GZ = 1UL,
	APU_COUNTING_HW_SEM_READER_SYS_SCP = 3UL,
	APU_COUNTING_HW_SEM_READER_SYS_TFA = 11UL,
} APU_MBOX_COUNTING_HW_SEM_READER_SYS_ID;

typedef enum{
	PWR_HW_SEM  = 0,
	SMMU_HW_SEM = 1,
	COUNTING_HW_SEM = 2,
	MBOX_HW_SEM_MODE_MAX = 3,
} ENUM_MBOX_HW_SEM_MODE;

typedef enum{
	MBOX_HW_SEM_RELEASE = 0,
	MBOX_HW_SEM_ACQUIRE  = 1,
	MBOX_HW_SEM_WRITER_RELEASE = 2,
	MBOX_HW_SEM_WRITER_ACQUIRE = 3,
	MBOX_HW_SEM_READER_RELEASE = 4,
	MBOX_HW_SEM_READER_ACQUIRE = 5,
	MBOX_HW_SEM_CTR_MAX = 6,
} ENUM_MBOX_HW_SEM_CTRL;

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

#define APU_ACE_MIRRORED_TIMESTAMP_0 (0x538)
#define APU_ACE_MIRRORED_TIMESTAMP_1 (0x53c)
#define APU_ACE_MIRRORED_TIMESTAMP_DIFF_0 (0x5c8)
#define APU_ACE_MIRRORED_TIMESTAMP_DIFF_1 (0x5cc)
#define APU_ACE_TIMESTAMP_DIFF_31_0 (0x5f8)
#define APU_ACE_TIMESTAMP_DIFF_63_32 (0x5fc)

#if APU_PRG_SUPPORT
#define APU_ARE_REG_BASE_ADDR (0x34050000)
#else
#define APU_ARE_REG_BASE_ADDR (0x19050000)
#endif
#define APU_ARE_REG_SZ 0x1000

#define APU_L2CACHE_WAY_NUM 8
#define APU_CACHE_DUMP_SETTING_MODE 16

#define PWR_OFF_TIMEOUT_DETECTION (1)

static void *apu_are_reg;

/* for IPI IRQ affinity tuning*/
static struct cpumask perf_cpus, normal_cpus;
static unsigned int mbox0_irq_id;
static unsigned int g_ipi_clamp;

/* for power off timeout detection */
static struct mtk_apu *g_apu;
static struct workqueue_struct *apu_workq;
static struct delayed_work timeout_work;

#if IS_ENABLED(CONFIG_PM_SLEEP)
static struct wakeup_source *ws;
#endif

static bool is_under_lp_scp_recovery_flow;

static struct delayed_work apu_polling_on_work;

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

/*
 * power on polling timeout
 *
 * excpetion will trigger
 * if polling power on status timeout
 */
#define APU_PWRON_TIMEOUT_MS (1000)

static void apu_timeout_work(struct work_struct *work)
{
	int i;
	struct mtk_apu *apu = g_apu;
	struct device *dev = apu->dev;

	if (apu->bypass_pwr_off_chk) {
		dev_info(dev, "%s: skip aee\n", __func__);
		return;
	}

	apu->bypass_pwr_off_chk = true;

	dev_info(dev, "ipi_debug_dump:\n");

	dev_info(dev, "local_pwr_ref_cnt = %d\n",
		apu->local_pwr_ref_cnt);
	for (i = 0; i < APU_IPI_MAX; i++)
		dev_info(dev, "ipi_pwr_ref_cnt[%d] = %d\n",
			i, apu->ipi_pwr_ref_cnt[i]);

#if IS_ENABLED(CONFIG_PM_SLEEP)
	dev_info(dev, "wake_lock_ref_cnt = %d\n",
		apu->wake_lock_ref_cnt);
	for (i = 0; i < APU_IPI_MAX; i++)
		dev_info(dev, "ipi_wake_lock_ref_cnt[%d] = %d\n",
			i, apu->ipi_wake_lock_ref_cnt[i]);
#endif

	apusys_rv_aee_warn("APUSYS_RV", "APUSYS_RV_POWER_OFF_TIMEOUT");
}

static int apusys_rv_smc_call(struct device *dev, uint32_t smc_id,
	uint32_t a2)
{
	struct arm_smccc_res res;

	if (smc_id != MTK_APUSYS_KERNEL_OP_APUSYS_RV_PWR_CTRL)
		dev_info(dev, "%s: smc call %d(a2 = %d)\n",
				__func__, smc_id, a2);

	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL, smc_id,
				a2, 0, 0, 0, 0, 0, &res);
	if (((int) res.a0) < 0)
		dev_info(dev, "%s: smc call %d return error(%ld)\n",
			__func__,
			smc_id, res.a0);

	return res.a0;
}

static void apu_setup_apummu(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	unsigned long hw_logger_addr = 0;

	hw_logger_addr = (u32)(apu->apummu_hwlog_buf_da);

	dev_info(dev, "%s: apummu_hwlog_buf_da= 0x%llx, md32_tcm_start_addr = 0x%llx\n",
		__func__, (unsigned long long) apu->apummu_hwlog_buf_da,
		(unsigned long long) apu->md32_tcm_start_addr);

	if (apu->platdata->flags & F_SECURE_BOOT) {
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_SETUP_APUMMU, 0);
	} else {
		if (apu->platdata->flags & F_PRELOAD_FIRMWARE) {
			rv_boot(apu->code_da, 0, hw_logger_addr, eAPUMMU_PAGE_LEN_4MB, apu->md32_tcm_start_addr,
				eAPUMMU_PAGE_LEN_512KB);
		} else {
			dev_info(dev, "%s: uP_seg_output = 0x0\n", __func__);
			if (BOOT_FROM_APU_TCM)
				rv_boot(APU_TCM_BASE + CODE_BUF_OFS, 0, hw_logger_addr, eAPUMMU_PAGE_LEN_4MB,
					apu->md32_tcm_start_addr, eAPUMMU_PAGE_LEN_512KB);
			else
				rv_boot(0, 0, hw_logger_addr, eAPUMMU_PAGE_LEN_4MB, apu->md32_tcm_start_addr, eAPUMMU_PAGE_LEN_512KB);
		}
	}
}

static void apu_setup_devapc(struct mtk_apu *apu)
{
	int32_t ret;
	struct device *dev = apu->dev;

	ret = (int32_t)apusys_rv_smc_call(dev,
		MTK_APUSYS_KERNEL_OP_DEVAPC_INIT_RCX, 0);

	dev_info(dev, "%s: %d\n", __func__, ret);
}

static void apu_setup_boot(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	unsigned long flags;

	if (apu->platdata->flags & F_SECURE_BOOT) {
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_SETUP_BOOT, 0);
	} else {
		spin_lock_irqsave(&apu->reg_lock, flags);
		/* set dbg_en for monitor */
		iowrite32(ioread32(apu->apu_rv_wrap + UP_SYSCTRL)
			| RV55_DBG_EN,
			apu->apu_rv_wrap + UP_SYSCTRL);
		/* set core_fetch_block */
		iowrite32(ioread32(apu->apu_rv_wrap + RV_CORE_COMM_CTRL_C0_0)
			| CORE_FETCH_BLOCK,
			apu->apu_rv_wrap + RV_CORE_COMM_CTRL_C0_0);
		iowrite32(ioread32(apu->apu_rv_wrap + RV_CORE_COMM_CTRL_C1_0)
			| CORE_FETCH_BLOCK,
			apu->apu_rv_wrap + RV_CORE_COMM_CTRL_C1_0);

		/* set boot_pc */
		iowrite32(0x0, apu->apu_rv_wrap + RV_CORE_COMM_CTRL_C0_1);
		iowrite32(0x0, apu->apu_rv_wrap + RV_CORE_COMM_CTRL_C1_1);

		spin_unlock_irqrestore(&apu->reg_lock, flags);

		dev_info(dev, "%s: RV_CORE_COMM_CTRL_C0_0 = 0x%x\n",
			__func__, ioread32(apu->apu_rv_wrap + RV_CORE_COMM_CTRL_C0_0));
		dev_info(dev, "%s: RV_CORE_COMM_CTRL_C1_0 = 0x%x\n",
			__func__, ioread32(apu->apu_rv_wrap + RV_CORE_COMM_CTRL_C1_0));
		dev_info(dev, "%s: RV_CORE_COMM_CTRL_C0_1 = 0x%x\n",
			__func__, ioread32(apu->apu_rv_wrap + RV_CORE_COMM_CTRL_C0_1));
		dev_info(dev, "%s: RV_CORE_COMM_CTRL_C1_1 = 0x%x\n",
			__func__, ioread32(apu->apu_rv_wrap + RV_CORE_COMM_CTRL_C1_1));
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
		/* set rv55_core_cg_en */
		iowrite32(ioread32(apu->apu_rv_wrap + UP_CG_EN)
			| RV55_CORE_CG_EN | G2B_CG_EN, apu->apu_rv_wrap + UP_CG_EN);

		/* clear core_fetch_block */
		iowrite32(ioread32(apu->apu_rv_wrap + RV_CORE_COMM_CTRL_C0_0)
			& ~CORE_FETCH_BLOCK,
			apu->apu_rv_wrap + RV_CORE_COMM_CTRL_C0_0);
		iowrite32(ioread32(apu->apu_rv_wrap + RV_CORE_COMM_CTRL_C1_0)
			& ~CORE_FETCH_BLOCK,
			apu->apu_rv_wrap + RV_CORE_COMM_CTRL_C1_0);
		spin_unlock_irqrestore(&apu->reg_lock, flags);

		dev_info(dev, "%s: UP_CG_EN = 0x%x\n",
			__func__, ioread32(apu->apu_rv_wrap + UP_CG_EN));
		dev_info(dev, "%s: RV_CORE_COMM_CTRL_C0_0 = 0x%x\n",
			__func__, ioread32(apu->apu_rv_wrap + RV_CORE_COMM_CTRL_C0_0));
		dev_info(dev, "%s: RV_CORE_COMM_CTRL_C1_0 = 0x%x\n",
			__func__, ioread32(apu->apu_rv_wrap + RV_CORE_COMM_CTRL_C1_0));

		for (i = 0; i < 20; i++) {
			dev_info(dev, "apu boot: pc_core_0=%08x, sp_core_0=%08x\n",
			ioread32(apu->apu_rv_wrap + RV_CORE_MON_PC_T0_0),
				ioread32(apu->apu_rv_wrap + RV_CORE_MON_SP_T0_0));
			dev_info(dev, "apu boot: pc_core_1=%08x, sp_core_1=%08x\n",
			ioread32(apu->apu_rv_wrap + RV_CORE_MON_PC_T0_1),
				ioread32(apu->apu_rv_wrap+RV_CORE_MON_SP_T0_1));
			usleep_range(0, 20);
		}
	}
}

static int mt6993_rproc_start(struct mtk_apu *apu)
{
	if ((apu->platdata->flags & F_BRINGUP) == 0)
		apu_setup_devapc(apu);

	apu_setup_apummu(apu);

	apu_setup_boot(apu);

	apu_start_mp(apu);

	return 0;
}

static int mt6993_rproc_stop(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;

	/* gating cg to stop */
	if (apu->platdata->flags & F_SECURE_BOOT)
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_STOP_MP, 0);
	else
		iowrite32(ioread32(apu->apu_rv_wrap + UP_CG_EN)
			& ~RV55_CORE_CG_EN,
			apu->apu_rv_wrap + UP_CG_EN);

	return 0;
}

static void mt6993_apu_pwr_wake_lock(struct mtk_apu *apu, uint32_t id)
{
#if IS_ENABLED(CONFIG_PM_SLEEP)
	unsigned long flags;

	spin_lock_irqsave(&apu->wakelock_spinlock, flags);
	apu->ipi_wake_lock_ref_cnt[id]++;
	apu->wake_lock_ref_cnt++;
	if (apu->wake_lock_ref_cnt == 1)
		__pm_stay_awake(ws);
	spin_unlock_irqrestore(&apu->wakelock_spinlock, flags);
	/* remove to reduce log
	 * dev_info(apu->dev, "%s(%d): wake_lock_ref_cnt = %d\n",
	 *	__func__, id, apu->wake_lock_ref_cnt);
	 */
#endif
}

static void mt6993_apu_pwr_wake_unlock(struct mtk_apu *apu, uint32_t id)
{
#if IS_ENABLED(CONFIG_PM_SLEEP)
	unsigned long flags;

	spin_lock_irqsave(&apu->wakelock_spinlock, flags);
	apu->ipi_wake_lock_ref_cnt[id]--;
	apu->wake_lock_ref_cnt--;
	if (apu->wake_lock_ref_cnt == 0)
		__pm_relax(ws);
	spin_unlock_irqrestore(&apu->wakelock_spinlock, flags);
	/* remove to reduce log
	 * dev_info(apu->dev, "%s(%d): wake_lock_ref_cnt = %d\n",
	 *	__func__, id, apu->wake_lock_ref_cnt);
	 */
#endif
}

/*
 * mbox_hw_sema_ctrl
 *
 * mode: reference ENUM_MBOX_HW_SEM_MODE
 * lv1_usr_bit: subsys_id(mbox id)
 * lv2_usr_bit: subuser_id for each susys(only for reader of counting hw semaphore)
 * ctrl: reference ENUM_MBOX_HW_SEM_CTRL
 * timeout_us: 0 for try lock
 */
static int mbox_hw_sema_ctrl(struct mtk_apu *apu, uint32_t mode, uint32_t lv1_usr_bit, uint32_t lv2_usr_bit,
	uint32_t ctrl, uint32_t timeout_us)
{
	struct device *dev = apu->dev;
	uint32_t ctl_bit = 0, chk_sta_bit = 0;
	uint32_t apu_sema_ctrl = 0, apu_sema_sta = 0;
	uint32_t timeout_cnt = 0;

	if (mode >= MBOX_HW_SEM_MODE_MAX || lv1_usr_bit >= 16 || lv2_usr_bit >= 16 || ctrl >= MBOX_HW_SEM_CTR_MAX) {
		dev_info(dev, "%s: input check fail(mode=%u user_bit=%u,%u ctrl=%u timeout=%u)\n",
			__func__, mode, lv1_usr_bit, lv2_usr_bit, ctrl, timeout_us);
		return -EINVAL;
	}

	if (mode == PWR_HW_SEM) {
		apu_sema_ctrl = APU_MBOX_OFFSET(lv1_usr_bit) + APU_MBOX_SEMA0_CTRL;
		apu_sema_sta = APU_MBOX_OFFSET(lv1_usr_bit) + APU_MBOX_SEMA0_STA;
	} else if (mode == SMMU_HW_SEM) {
		apu_sema_ctrl = APU_MBOX_OFFSET(lv1_usr_bit) + APU_MBOX_SEMA1_CTRL;
		apu_sema_sta = APU_MBOX_OFFSET(lv1_usr_bit) + APU_MBOX_SEMA1_STA;
	} else { /* mode == COUNTING_HW_SEM */
		if (ctrl == MBOX_HW_SEM_WRITER_RELEASE || ctrl == MBOX_HW_SEM_WRITER_ACQUIRE) {
			apu_sema_ctrl = APU_MBOX_OFFSET(lv1_usr_bit) + APU_MBOX_SEMA2_WRITER_CTRL;
			apu_sema_sta = APU_MBOX_OFFSET(lv1_usr_bit) + APU_MBOX_SEMA2_STA;
		} else if (ctrl == MBOX_HW_SEM_READER_RELEASE || ctrl == MBOX_HW_SEM_READER_ACQUIRE) {
			apu_sema_ctrl = APU_MBOX_OFFSET(lv1_usr_bit) + APU_MBOX_SEMA2_READER_CTRL;
			apu_sema_sta = APU_MBOX_OFFSET(lv1_usr_bit) + APU_MBOX_SEMA2_READER_USER_STA;
		} else{
			dev_info(dev, "%s: counting sema invalid operation(mode=%u user_bit=%u,%u ctrl=%u timeout=%u)\n",
				__func__, mode, lv1_usr_bit, lv2_usr_bit, ctrl, timeout_us);
			return -EINVAL;
		}
	}

	switch (ctrl) {
	case MBOX_HW_SEM_ACQUIRE:
	case MBOX_HW_SEM_WRITER_ACQUIRE:
		/* sema_key_set */
		ctl_bit = BIT(0);
		chk_sta_bit = BIT(lv1_usr_bit);
		break;
	case MBOX_HW_SEM_RELEASE:
	case MBOX_HW_SEM_WRITER_RELEASE:
		/* sema_key_clr */
		ctl_bit = BIT(1);
		chk_sta_bit = BIT(lv1_usr_bit);
		break;
	case MBOX_HW_SEM_READER_ACQUIRE:
		/* sema2_reader_key_set */
		ctl_bit = BIT(lv2_usr_bit);
		chk_sta_bit = BIT(lv2_usr_bit);
		break;
	case MBOX_HW_SEM_READER_RELEASE:
		/* sema2_reader_key_clr */
		ctl_bit = BIT(16 + lv2_usr_bit);
		chk_sta_bit = BIT(lv2_usr_bit);
		break;
	default:
		return -EINVAL;
	}

	/* release hw sem, check if semaphore currently not held by this user, return failed */
	if ((ctrl == MBOX_HW_SEM_RELEASE || ctrl == MBOX_HW_SEM_WRITER_RELEASE || ctrl == MBOX_HW_SEM_READER_RELEASE) &&
		((ioread32(apu->apu_mbox + apu_sema_sta) & chk_sta_bit) == 0)) {
		dev_info(dev,"%s: release error: mode=%d usr_bit=%d,%d ctrl=%d sema_sta(0x%08x) = 0x%08x\n",
			__func__, mode, lv1_usr_bit, lv2_usr_bit, ctrl, apu_sema_sta, ioread32(apu->apu_mbox + apu_sema_sta));
		return -EINVAL;
	}

	/* write hw sem */
	iowrite32(ctl_bit, apu->apu_mbox + apu_sema_ctrl);

	/* no need to check register value for semaphore release
	 * need to consider other host may acquire hw sem right after release
	 * -> register check may be fail but actually no error occured
	 */
	if (ctrl == MBOX_HW_SEM_RELEASE || ctrl == MBOX_HW_SEM_WRITER_RELEASE || ctrl == MBOX_HW_SEM_READER_RELEASE)
		goto end;

	/* retry set hw sem if take by the other user */
	while ((ioread32(apu->apu_mbox + apu_sema_sta) & chk_sta_bit) != chk_sta_bit) {
		if (timeout_cnt++ >= timeout_us) {
			/* timeout_us == 0 denotes trylock, no need to print error log */
			if (timeout_us > 0)
				dev_info(dev,
					"%s timeout mode=%d usr_bit=%d,%d ctrl=%d sema_sta(0x%08x) = 0x%08x\n",
						__func__, mode, lv1_usr_bit, lv2_usr_bit, ctrl, apu_sema_sta,
						ioread32(apu->apu_mbox + apu_sema_sta));
			return -EBUSY;
		}

		/* write hw sem */
		iowrite32(ctl_bit, apu->apu_mbox + apu_sema_ctrl);
		udelay(1);
	}

end:
	if (apu->platdata->flags & F_DEBUG_LOG_ON && (mode != COUNTING_HW_SEM))
		dev_info(dev, "%s: hw_sem_ctrl: mode=%d usr_bit=%d,%d ctrl=%d sema_sta(0x%08x) = 0x%08x\n",
			__func__, mode, lv1_usr_bit, lv2_usr_bit, ctrl, apu_sema_sta,
			ioread32(apu->apu_mbox + apu_sema_sta));

	return 0;
}

/* return value:
 *     0: semaphore acquired successfully
 *     -EINVAL: semaphore operation error
 *     -EBUSY: semaphore acquired fail(npu off)
 */
static int mt6993_mbox_counting_hw_sem_reader_trylock(struct mtk_apu *apu, uint32_t user)
{
	int ret = 0;

	if (user >= MBOX_HW_SEMA_RD_KRN_USR_MAX) {
		dev_info(apu->dev, "%s: invalid user ID(%u)\n", __func__, user);
		return -EINVAL;
	}

	mutex_lock(&reader_hw_sema_lock);

	cnting_hw_sema_ref_cnt[user]++;
	if (cnting_hw_sema_ref_cnt[user] == 1)
		ret = mbox_hw_sema_ctrl(apu, COUNTING_HW_SEM, APU_COUNTING_HW_SEM_READER_SYS_KERNEL, user,
			MBOX_HW_SEM_READER_ACQUIRE, 0);

	/* restore ref cnt if lock fail */
	if (ret)
		cnting_hw_sema_ref_cnt[user]--;

	mutex_unlock(&reader_hw_sema_lock);

	/* dev_info(apu->dev, "%s: mbox_hw_sema_ctrl return %d\n", __func__, ret); */

	return ret;
}

/* return value:
 *     0: semaphore release successfully
 *     -EINVAL: semaphore operation error
 */
static int mt6993_mbox_counting_hw_sem_reader_unlock(struct mtk_apu *apu, uint32_t user)
{
	int ret = 0;

	if (user >= MBOX_HW_SEMA_RD_KRN_USR_MAX) {
		dev_info(apu->dev, "%s: invalid user ID(%u)\n", __func__, user);
		return -EINVAL;
	}

	mutex_lock(&reader_hw_sema_lock);

	cnting_hw_sema_ref_cnt[user]--;
	if (cnting_hw_sema_ref_cnt[user] == 0)
		ret = mbox_hw_sema_ctrl(apu, COUNTING_HW_SEM, APU_COUNTING_HW_SEM_READER_SYS_KERNEL, user,
			MBOX_HW_SEM_READER_RELEASE, 0);

	/* restore ref cnt if unlock fail */
	if (ret)
		cnting_hw_sema_ref_cnt[user]++;

	mutex_unlock(&reader_hw_sema_lock);

	/* dev_info(apu->dev, "%s: mbox_hw_sema_ctrl return %d\n", __func__, ret); */

	return ret;
}


/*
 * op:
 *      0 - power off
 *      1 - power on
 */
static int apu_power_ctrl(struct mtk_apu *apu, uint32_t op)
{
	int ret = 0;
	struct device *dev = apu->dev;
	uint32_t timeout = 300; /* 300 us to align with tf-a driver */
	uint32_t global_ref_cnt;
	struct timespec64 ts, te;

	if (apu->platdata->flags & F_SECURE_BOOT) {
		ktime_get_ts64(&ts);
		ret = apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_PWR_CTRL, op);
		ktime_get_ts64(&te);
		ts = timespec64_sub(te, ts);
		apu->smc_time_diff_ns = timespec64_to_ns(&ts);
	} else {
		ret = mbox_hw_sema_ctrl(apu, PWR_HW_SEM, APU_HW_SEM_SYS_APMCU, 0, MBOX_HW_SEM_ACQUIRE, timeout);
		if (ret) {
			dev_info(dev, "%s(%d): sem acquire timeout\n", __func__, op);
			return ret;
		}

		global_ref_cnt = ioread32(apu->apu_mbox + APU_MBOX_OFFSET(APU_HW_SEM_SYS_APMCU) + APU_MBOX_DUMMY);
		dev_info(dev, "%s: op = %d, current global_ref_cnt = %d\n",
			__func__, op, global_ref_cnt);

		if (global_ref_cnt > 2) {
			/* only possible to be 0/1/2 */
			dev_info(dev, "%s: global_ref_cnt(%d) > 2\n",
				__func__, global_ref_cnt);
			ret = mbox_hw_sema_ctrl(apu, PWR_HW_SEM, APU_HW_SEM_SYS_APMCU, 0, MBOX_HW_SEM_RELEASE, timeout);
			if (ret)
				dev_info(dev, "%s(%d): sem release timeout\n", __func__, op);

			return -EINVAL;
		}

		if (op == 0) {
			global_ref_cnt--;
			iowrite32(global_ref_cnt, apu->apu_mbox + APU_MBOX_OFFSET(APU_HW_SEM_SYS_APMCU) + APU_MBOX_DUMMY);
			/* global_ref_cnt is from 1 to 0, need to power off */
			if (global_ref_cnt == 0) {
				/* set wkup bit to 0(use mbox11 for linux power ctrl) */
				iowrite32(0, apu->apu_mbox + APU_MBOX_OFFSET(APU_HW_SEM_SYS_APMCU) + MBOX_WKUP_CFG);
			}
		} else if (op == 1) {
			global_ref_cnt++;
			iowrite32(global_ref_cnt, apu->apu_mbox + APU_MBOX_OFFSET(APU_HW_SEM_SYS_APMCU) + APU_MBOX_DUMMY);
			/* global_ref_cnt is from 0 to 1, need to power on */
			if (global_ref_cnt == 1) {
				/* set wkup bit to 1(use mbox11 for linux power ctrl) */
				iowrite32(1, apu->apu_mbox + APU_MBOX_OFFSET(APU_HW_SEM_SYS_APMCU) + MBOX_WKUP_CFG);
			}
		}

		ret = mbox_hw_sema_ctrl(apu, PWR_HW_SEM, APU_HW_SEM_SYS_APMCU, 0, MBOX_HW_SEM_RELEASE, timeout);
		if (ret) {
			dev_info(dev, "%s(%d): sem release timeout\n", __func__, op);
			return ret;
		}

		dev_info(dev, "%s: op = %d, current global_ref_cnt = %d\n",
			__func__, op, global_ref_cnt);
	}

	return ret;
}

static uint64_t timer_get_ace_timestamp_cycle(void)
{
	uint64_t val;
	uint32_t tmr_low, tmr_high;
	uint32_t ov_high;

read_again:
	tmr_high = ioread32(apu_are_reg + APU_ACE_MIRRORED_TIMESTAMP_1);
	tmr_low = ioread32(apu_are_reg + APU_ACE_MIRRORED_TIMESTAMP_0);
	ov_high = ioread32(apu_are_reg + APU_ACE_MIRRORED_TIMESTAMP_1);

	/* check if wrap around occurs or not */
	if (tmr_high != ov_high)
	{
		/* wrap occurs try read again*/
		goto read_again;
	}

	val =
		((((uint64_t) tmr_high) << 32) & 0xFFFFFFFF00000000ULL)
		| (((uint64_t) tmr_low) & 0x00000000FFFFFFFFULL);

	return val;
}

static void mt6993_timesync_update(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	u64 timertick = 0;
	uint32_t sys_timer_clk_mhz;
	unsigned long flags;

	if (apu->platdata->flags & F_FPGA_EP)
		sys_timer_clk_mhz = 2948;
	else if (APU_PRG_SUPPORT)
		sys_timer_clk_mhz = 1280;
	else
		sys_timer_clk_mhz = 1000;

	spin_lock_irqsave(&apu->reg_lock, flags);
	if (APU_PRG_SUPPORT)
		timertick = timer_get_ace_timestamp_cycle();
	else
		timertick = arch_timer_read_counter();
	apu->conf_buf->time_offset = sched_clock();
	spin_unlock_irqrestore(&apu->reg_lock, flags);

	/* Calculate time diff */
	apu->conf_buf->time_diff =
		((timertick / sys_timer_clk_mhz) - (apu->conf_buf->time_offset)/1000)*1000;
	apu->conf_buf->time_diff_cycle = timertick - (apu->conf_buf->time_offset * sys_timer_clk_mhz / 1000);

	/* notify uP to do timesync */
	iowrite32(1, apu->apu_mbox + MBOX_RV_TIMESYNC_FLG);

	if (apu->platdata->flags & F_DEBUG_LOG_ON)
		dev_info(dev,
			"%s: t_diff = %llu, t_diff_cycle = %llu, t_offset = %llu, tmrtick = %llu\n",
			__func__, apu->conf_buf->time_diff, apu->conf_buf->time_diff_cycle,
			apu->conf_buf->time_offset, timertick);
}

static bool pwr_on_fail_aee_triggered;
static bool pwr_off_fail_aee_triggered;
static int mt6993_cold_boot_power_on(struct mtk_apu *apu)
{
	int ret;
	struct device *dev = apu->dev;

	apu->ipi_pwr_ref_cnt[APU_IPI_INIT]++;
	apu->local_pwr_ref_cnt++;

	/* initialize global ref cnt to zero because apu_top may already
	 * call power on smc call
	 */
	if (apu->platdata->flags & F_SECURE_BOOT) {
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_COLD_BOOT_CLR_MBOX_DUMMY, 0);
	} else {
		iowrite32(0, apu->apu_mbox + APU_MBOX_OFFSET(11) + APU_MBOX_DUMMY);
	}

	ret = apu_power_ctrl(apu, 1);
	if (ret && !pwr_on_fail_aee_triggered) {
		apusys_rv_aee_warn("APUSYS_RV",
			"APUSYS_RV_POWER_ON_FAIL");
		pwr_on_fail_aee_triggered = true;
	}

	return ret;
}

static int mt6993_polling_rpc_status(struct mtk_apu *apu, u32 pwr_stat, u32 timeout)
{
	int ret = 0;
	void *addr = 0;
	uint32_t val = 0;
	uint32_t polling_val = 0;

	if (pwr_stat == 0) {
		addr = apu->apu_rpc + APU_RPC_INTF_PWR_RDY;
		polling_val = 0x3UL;
	} else {
		addr = apu->apu_mbox + MBOX_RV_PWR_STA_FLG;
		polling_val = 0x1UL;
	}

	ret = readl_relaxed_poll_timeout_atomic(addr, val,
		((val & polling_val) == pwr_stat), 1, timeout);

	if (ret)
		dev_info(apu->dev, "%s(pwr_stat = %u): timeout\n", __func__, pwr_stat);

	return ret;
}

static inline void profile_start(struct timespec64 *ts)
{
	ktime_get_ts64(ts);
}

static inline uint64_t profile_end(struct timespec64 *ts, struct timespec64 *te)
{
	ktime_get_ts64(te);
	*ts = timespec64_sub(*te, *ts);
	return timespec64_to_ns(ts);
}

/* TODO: block power on off after pwr_on_fail_aee_triggered
 *		 or pwr_off_fail_aee_triggered?
 */
static int mt6993_power_on_off_locked(struct mtk_apu *apu, u32 id, u32 on, u32 off)
{
	int ret = 0;
	struct device *dev = apu->dev;
	uint32_t rpc_state = 0, pwr_ready = 0;
	struct timespec64 ts, te;

	if (on == 1 && off == 0) {
		/* block normal cmds when under scp lp recovery flow  */
		if (is_under_lp_scp_recovery_flow)
			return -EBUSY;
		/* pwr on */
		if (apu->ipi_pwr_ref_cnt[id] == U32_MAX) {
			dev_info(dev, "%s: ipi_pwr_ref_cnt[%u] == U32_MAX\n", __func__, id);
			ret = -EINVAL;
		} else {
			profile_start(&ts);
			apu->ipi_pwr_ref_cnt[id]++;
			apu->local_pwr_ref_cnt++;
			mt6993_apu_pwr_wake_lock(apu, id);
			apu->sub_latency[0] = profile_end(&ts, &te);

			if (apu->local_pwr_ref_cnt == 1) {
				profile_start(&ts);
				pwr_ready = ioread32(apu->apu_rpc + APU_RPC_INTF_PWR_RDY) & 0x1;
				rpc_state = ioread32(apu->apu_rpc + APU_RPC_STATUS_1) & 0x1;
				/* rpc_state == 1 means in lp mode, need to retry
				 * only APU_IPI_SCP_NP_RECOVER can bypass the check
				 */
				if (pwr_ready == 1 && id != APU_IPI_SCP_NP_RECOVER && rpc_state == 1) {
					dev_info(dev, "%s(%d): APU_RPC_STATUS_1 = 0x%x\n",
						__func__, ret,
						ioread32(apu->apu_rpc + APU_RPC_STATUS_1));
					mt6993_apu_pwr_wake_unlock(apu, id);
					apu->ipi_pwr_ref_cnt[id]--;
					apu->local_pwr_ref_cnt--;
					return -EBUSY;
				}
				apu->sub_latency[1] = profile_end(&ts, &te);

				profile_start(&ts);
				mt6993_timesync_update(apu);
				apu->sub_latency[2] = profile_end(&ts, &te);

				profile_start(&ts);
				if (apu->apusys_rv_trace_on)
					apusys_rv_trace_begin("apu_power_ctrl(%d/%d/%d)", id, on, off);
				ret = apu_power_ctrl(apu, 1);
				if (apu->apusys_rv_trace_on)
					apusys_rv_trace_end();
				apu->sub_latency[3] = profile_end(&ts, &te);

				profile_start(&ts);
				if (!ret) {
					/* for power timeout detection */
					if (PWR_OFF_TIMEOUT_DETECTION)
						queue_delayed_work(apu_workq,
							&timeout_work,
							msecs_to_jiffies(APU_PWROFF_TIMEOUT_MS));
					if (id == APU_IPI_SCP_NP_RECOVER && rpc_state == 1)
						is_under_lp_scp_recovery_flow = true;

					if (apu->pwr_on_polling_dbg_mode)
						queue_delayed_work(apu_workq,
							&apu_polling_on_work,
							msecs_to_jiffies(APU_PWRON_TIMEOUT_MS));
				} else {
					mt6993_apu_pwr_wake_unlock(apu, id);
					apu->ipi_pwr_ref_cnt[id]--;
					apu->local_pwr_ref_cnt--;
					dev_info(dev, "%s: power on fail(%d)\n", __func__, ret);
				}
				apu->sub_latency[4] = profile_end(&ts, &te);
			}
		}
	} else if (on == 0 && off == 1) {
		/* pwr off */
		if (apu->ipi_pwr_ref_cnt[id] == 0) {
			dev_info(dev, "%s: ipi_pwr_ref_cnt[%u] == 0\n", __func__, id);
			ret = -EINVAL;
		} else {
			profile_start(&ts);
			apu->ipi_pwr_ref_cnt[id]--;
			apu->local_pwr_ref_cnt--;
			mt6993_apu_pwr_wake_unlock(apu, id);
			apu->sub_latency[0] = profile_end(&ts, &te);

			if (apu->local_pwr_ref_cnt == 0) {
				profile_start(&ts);

				if (apu->pwr_on_polling_dbg_mode)
					cancel_delayed_work_sync(&apu_polling_on_work);

				apu->sub_latency[1] = profile_end(&ts, &te);

				profile_start(&ts);
				ret = apu_power_ctrl(apu, 0);
				apu->sub_latency[2] = profile_end(&ts, &te);

				profile_start(&ts);
				if (!ret) {
					/* clear status & cancel timeout worker */
					apu->bypass_pwr_off_chk = false;
					if (PWR_OFF_TIMEOUT_DETECTION)
						cancel_delayed_work_sync(&timeout_work);
					if (id == APU_IPI_SCP_NP_RECOVER)
						is_under_lp_scp_recovery_flow = false;
				} else {
					mt6993_apu_pwr_wake_lock(apu, id);
					apu->ipi_pwr_ref_cnt[id]++;
					apu->local_pwr_ref_cnt++;
					dev_info(dev, "%s: power off fail(%d)\n", __func__, ret);
				}
				apu->sub_latency[3] = profile_end(&ts, &te);
			}
		}
	} else {
		dev_info(apu->dev, "%s: invalid operation: id(%d), on(%d), off(%d)\n",
			__func__, id, on, off);
		ret = -EINVAL;
	}

	return ret;
}

static int mt6993_power_on_off(struct mtk_apu *apu, u32 id, u32 on, u32 off)
{
	int ret = 0;
	struct device *dev = apu->dev;
	/* struct timespec64 ts, te; */
	uint32_t retry_cnt = 500, i = 0;
	struct timespec64 ts, te;
	struct timespec64 t1, t2;

	/* ktime_get_ts64(&ts); */

	profile_start(&t1);
	for (i = 0; i < retry_cnt; i++) {
		mutex_lock(&apu->power_lock);

		profile_start(&ts);
		ret = mt6993_power_on_off_locked(apu, id, on, off);
		apu->sub_latency[5] = profile_end(&ts, &te);

		mutex_unlock(&apu->power_lock);
		/* retry if return value is -EBUSY because
		 * hw sem may be blocked by other host or apu under lp mode
		 */
		if (ret == -EBUSY) {
			if (!(i % 10))
				dev_info(dev, "%s: retry on(%u) off(%u)(%u/%u)\n",
					__func__, on, off, i, retry_cnt);
			if (i < 10)
				usleep_range(200, 500);
			else if (i < 50)
				usleep_range(1000, 2000);
			else
				usleep_range(10000, 11000);
			continue;
		}
		break;
	}
	apu->sub_latency[6] = profile_end(&t1, &t2);
	apu->sub_latency[7] = i;

	if (ret) {
		if (on == 1 && off == 0 && !pwr_on_fail_aee_triggered) {
			apusys_rv_aee_warn("APUSYS_RV",
				"APUSYS_RV_POWER_ON_FAIL");
			pwr_on_fail_aee_triggered = true;
		} else if (on == 0 && off == 1 && !pwr_off_fail_aee_triggered) {
			apusys_rv_aee_warn("APUSYS_RV",
				"APUSYS_RV_POWER_OFF_FAIL");
			pwr_off_fail_aee_triggered = true;
		}
	}

	/* remove to reduce latency
	 * ktime_get_ts64(&te);
	 * ts = timespec64_sub(te, ts);
	 * apu_info_ratelimited(dev,
	 *  "%s(%d/%d/%d): local_pwr_ref_cnt = %d, ipi_pwr_ref_cnt = %d, time = %lld ns\n",
	 *  __func__, id, on, off, apu->local_pwr_ref_cnt,
	 *  apu->ipi_pwr_ref_cnt[id], timespec64_to_ns(&ts));
	 */

	return ret;
}

static int mt6993_ipi_send_pre(struct mtk_apu *apu, uint32_t id, bool is_host_initiated)
{
	if (!PWR_OFF_TIMEOUT_DETECTION)
		return 0;

	if (!is_host_initiated)
		return 0;

	cancel_delayed_work_sync(&timeout_work);

	queue_delayed_work(apu_workq,
		&timeout_work,
		msecs_to_jiffies(IPI_PWROFF_TIMEOUT_MS));

	return 0;
}

static void mt6993_debug_info_dump(struct mtk_apu *apu, struct seq_file *s)
{
	int i;

	seq_puts(s, "\ndebug_info_dump:\n");

	seq_printf(s, "fw_ver: %s\n", apu->fw_ver);

	seq_printf(s, "local_pwr_ref_cnt = %d\n",
		apu->local_pwr_ref_cnt);
	for (i = 0; i < APU_IPI_MAX; i++)
		seq_printf(s, "ipi_pwr_ref_cnt[%d] = %d\n",
			i, apu->ipi_pwr_ref_cnt[i]);

	if (apu->platdata->flags & F_DEBUG_MEM_SUPPORT) {
		dma_sync_single_for_cpu(
			apu->dev, apu->debug_memory_iova, PAGE_SIZE, DMA_FROM_DEVICE);
		for (i = 0; i < DEBUG_MEMORY_DUMP_SIZE; i++)
			seq_printf(s, "debug_memory[%d] = 0x%x(%u)\n",
				i, apu->debug_memory[i], apu->debug_memory[i]);
	}

#if IS_ENABLED(CONFIG_PM_SLEEP)
	seq_printf(s, "wake_lock_ref_cnt = %d\n",
		apu->wake_lock_ref_cnt);
	for (i = 0; i < APU_IPI_MAX; i++)
		seq_printf(s, "ipi_wake_lock_ref_cnt[%d] = %d\n",
			i, apu->ipi_wake_lock_ref_cnt[i]);
#endif
}

static int mt6993_irq_affin_init(struct mtk_apu *apu)
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
	mbox0_irq_id = apu->mbox0_irq_number;

	return 0;
}

static int mt6993_irq_affin_set(struct mtk_apu *apu)
{
	irq_set_affinity_hint(apu->mbox0_irq_number, &perf_cpus);

	return 0;
}

static int mt6993_irq_affin_unset(struct mtk_apu *apu)
{
	irq_set_affinity_hint(apu->mbox0_irq_number, &normal_cpus);

	return 0;
}

static int mt6993_irq_affin_clear(struct mtk_apu *apu)
{
	irq_set_affinity_hint(apu->mbox0_irq_number, NULL);

	return 0;
}

static int mt6993_irq_affin_online(unsigned int cpu)
{
	if (cpu == 2)
		irq_set_affinity_hint(mbox0_irq_id, cpumask_of(cpu));

	return 0;
}

static int mt6993_irq_affin_offline(unsigned int cpu)
{
	/* Reset cpumask to &normal_cpus */
	if (cpu == 2)
		irq_set_affinity_hint(mbox0_irq_id, &normal_cpus);

	return 0;
}

static int mt6993_apu_memmap_init(struct mtk_apu *apu)
{
	struct platform_device *pdev = apu->pdev;
	struct device *dev = apu->dev;
	struct device_node *np = dev->of_node;
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_rv_wrap");
	if (res == NULL) {
		dev_info(dev, "%s: apu_rpc get resource fail\n", __func__);
		return -ENODEV;
	}
	/* PRG platform must use ioremap to prevent mapping fail */
	if (APU_PRG_SUPPORT)
		apu->apu_rv_wrap = ioremap(res->start, res->end - res->start + 1);
	else
		apu->apu_rv_wrap = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->apu_rv_wrap)) {
		dev_info(dev, "%s: apu_rv_wrap remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_mbox");
	if (res == NULL) {
		dev_info(dev, "%s: apu_mbox get resource fail\n", __func__);
		return -ENODEV;
	}
	if (APU_PRG_SUPPORT)
		apu->apu_mbox = ioremap(res->start, res->end - res->start + 1);
	else
		apu->apu_mbox = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->apu_mbox)) {
		dev_info(dev, "%s: apu_mbox remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_wdt");
	if (res == NULL) {
		dev_info(dev, "%s: apu_wdt get resource fail\n", __func__);
		return -ENODEV;
	}
	if (APU_PRG_SUPPORT)
		apu->apu_wdt = ioremap(res->start, res->end - res->start + 1);
	else
		apu->apu_wdt = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->apu_wdt)) {
		dev_info(dev, "%s: apu_wdt remap base fail\n", __func__);
		return -ENOMEM;
	}

	if (apu->platdata->flags & F_SECURE_COREDUMP) {
		apu->md32_tcm = NULL;
		apu->md32_tcm_start_addr = 0;
		if (of_property_read_u32(np, "up-tcm-sz", &apu->md32_tcm_sz)) {
			dev_info(dev, "%s: missing up-tcm-sz\n", __func__);
			return -ENODEV;
		}
	} else {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "md32_tcm");
		if (res == NULL) {
			dev_info(dev, "%s: md32_tcm get resource fail\n", __func__);
			return -ENODEV;
		}
		if (APU_PRG_SUPPORT)
			apu->md32_tcm = ioremap_wc(res->start, res->end - res->start + 1);
		else
			apu->md32_tcm = devm_ioremap_wc(dev, res->start, res->end - res->start + 1);
		if (IS_ERR((void const *)apu->md32_tcm)) {
			dev_info(dev, "%s: md32_tcm remap base fail\n", __func__);
			return -ENOMEM;
		}
		/* TODO: for RV55, how to determine tcm size? */
		apu->md32_tcm_sz = res->end - res->start + 1;
		apu->md32_tcm_start_addr = res->start;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "md32_cache_dump");
	if (res == NULL) {
		dev_info(dev, "%s: md32_cache_dump get resource fail\n", __func__);
		return -ENODEV;
	}
	if (APU_PRG_SUPPORT)
		apu->md32_cache_dump = ioremap(res->start, res->end - res->start + 1);
	else
		apu->md32_cache_dump = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->md32_cache_dump)) {
		dev_info(dev, "%s: md32_cache_dump remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_rpc");
	if (res == NULL) {
		dev_info(dev, "%s: apu_rpc get resource fail\n", __func__);
		return -ENODEV;
	}
	if (APU_PRG_SUPPORT)
		apu->apu_rpc = ioremap(res->start, res->end - res->start + 1);
	else
		apu->apu_rpc = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->apu_rpc)) {
		dev_info(dev, "%s: apu_rpc remap base fail\n", __func__);
		return -ENOMEM;
	}

	if (APU_PRG_SUPPORT) {
		apu_are_reg = ioremap(APU_ARE_REG_BASE_ADDR, APU_ARE_REG_SZ);
		if (IS_ERR((void const *)apu_are_reg)) {
			dev_info(dev, "%s: apu_are_reg remap base fail\n", __func__);
			return -ENOMEM;
		}
		dev_info(dev, "<%s> apu_are_reg ioremap done\n", __func__);
	}

	if (BOOT_FROM_APU_TCM) {
		apu->apu_tcm = ioremap_wc(APU_TCM_BASE, APU_TCM_CODE_BUF_SZ);
		if (IS_ERR((void const *)apu->apu_tcm)) {
			dev_info(dev, "%s: apu_tcm remap base fail\n", __func__);
			return -ENOMEM;
		}
		memset(apu->apu_tcm, 0, APU_TCM_CODE_BUF_SZ);
		dev_info(dev, "%s: apu_tcm remap done\n", __func__);
	}

	return 0;
}

static void mt6993_apu_memmap_remove(struct mtk_apu *apu)
{
}

static void mt6993_rv_cg_gating(struct mtk_apu *apu)
{
	iowrite32(ioread32(apu->apu_rv_wrap + UP_CG_EN)
			& ~RV55_CORE_CG_EN,
			apu->apu_rv_wrap + UP_CG_EN);
}

static void mt6993_rv_cg_ungating(struct mtk_apu *apu)
{
	iowrite32(ioread32(apu->apu_rv_wrap + UP_CG_EN)
			| RV55_CORE_CG_EN,
			apu->apu_rv_wrap + UP_CG_EN);
}

static void mt6993_rv_cachedump(struct mtk_apu *apu)
{
	int offset, i;
	unsigned long flags;
	int idx = 0;

	struct apu_coredump *coredump =
		(struct apu_coredump *) apu->coredump;

	/* set CACHE_DUMP_SETTING for cache dump enable through normal APB */
	/* Core0 L1 */
	spin_lock_irqsave(&apu->reg_lock, flags);
	iowrite32(0x1, apu->apu_rv_wrap + CACHE_DUMP_SETTING);
	spin_unlock_irqrestore(&apu->reg_lock, flags);
	for (offset = 0 ; offset < (CACHE_DUMP_SIZE / sizeof(uint32_t)) ; offset++)
		coredump->cachedump[offset] =
				ioread32(apu->md32_cache_dump + offset * sizeof(uint32_t));

	idx += offset;
	/* Core1 L1 */
	spin_lock_irqsave(&apu->reg_lock, flags);
	iowrite32(0x1 | (0x1 << APU_CACHE_DUMP_SETTING_MODE), apu->apu_rv_wrap + CACHE_DUMP_SETTING);
	spin_unlock_irqrestore(&apu->reg_lock, flags);
	for (offset = 0 ; offset < (CACHE_DUMP_SIZE / sizeof(uint32_t)) ; offset++)
		coredump->cachedump[idx + offset] =
				ioread32(apu->md32_cache_dump + offset * sizeof(uint32_t));

	idx += offset;
	/* L2 */
	for (i = 0 ; i < APU_L2CACHE_WAY_NUM ; i++) {
		spin_lock_irqsave(&apu->reg_lock, flags);
		iowrite32(0x1 | ((0x8 + i) << APU_CACHE_DUMP_SETTING_MODE), apu->apu_rv_wrap + CACHE_DUMP_SETTING);
		spin_unlock_irqrestore(&apu->reg_lock, flags);
		for (offset = 0 ; offset < (L2_CACHE_DUMP_WAY_SIZE / sizeof(uint32_t)) ; offset++)
			coredump->cachedump[idx + offset] =
					ioread32(apu->md32_cache_dump + offset * sizeof(uint32_t));

		idx += offset;
	}

	spin_lock_irqsave(&apu->reg_lock, flags);
	iowrite32(0x1 | ((0x8) << APU_CACHE_DUMP_SETTING_MODE), apu->apu_rv_wrap + CACHE_DUMP_SETTING);
	spin_unlock_irqrestore(&apu->reg_lock, flags);
	for (offset = 0 ; offset < (L2_CACHE_DUMP_DICT_SIZE / sizeof(uint32_t)) ; offset++)
		coredump->cachedump[idx + offset] =
				ioread32(apu->md32_cache_dump + L2_CACHE_DICT_ADDR + offset * sizeof(uint32_t));

	spin_lock_irqsave(&apu->reg_lock, flags);
	/* clear CACHE_DUMP_SETTING */
	iowrite32(0, apu->apu_rv_wrap + CACHE_DUMP_SETTING);
	spin_unlock_irqrestore(&apu->reg_lock, flags);
}

static void mt6993_ipi_clamp(struct mtk_apu *apu)
{
	struct sched_attr attr = {};
	struct device *dev = apu->dev;

	if (!current)
		return;

	if (g_ipi_clamp != apu->ipi_boost_value) {
		attr.sched_policy = -1;
		attr.sched_flags =
			SCHED_FLAG_KEEP_ALL |
			SCHED_FLAG_UTIL_CLAMP |
			SCHED_FLAG_RESET_ON_FORK;
		attr.sched_util_min = apu->ipi_boost_value;
		attr.sched_util_max = 1024;
		if (current->policy == SCHED_FIFO || current->policy == SCHED_RR)
			attr.sched_priority = current->rt_priority;

		if (sched_setattr_nocheck(current, &attr) != 0)
			dev_info(dev, "%s set uclamp fail\n", __func__);
		else
			g_ipi_clamp = apu->ipi_boost_value;

	}
}

static void apu_polling_on_work_func(struct work_struct *p_work)
{
	int ret;
	struct mtk_apu *apu = g_apu;
	struct device *dev = apu->dev;

	ret = mt6993_polling_rpc_status(apu, 1, 1);
	if (ret) {
		apu_regdump();
		apu->bypass_pwr_off_chk = true;
		dev_info(dev, "%s: APU_RPC_TOP_CON = 0x%x\n",
			__func__, ioread32(apu->apu_rpc + 0x0));
		dev_info(dev, "%s: APU_RPC_TOP_SEL = 0x%x\n",
			__func__, ioread32(apu->apu_rpc + 0x4));
		dev_info(dev, "%s: APU_RPC_IO_DEBUG = 0x%x\n",
			__func__, ioread32(apu->apu_rpc + 0xc));
		dev_info(dev, "%s: APU_RPC_STATUS = 0x%x\n",
			__func__, ioread32(apu->apu_rpc + 0x14));
		dev_info(dev, "%s: APU_RPC_TOP_SEL_1 = 0x%x\n",
			__func__, ioread32(apu->apu_rpc + 0x18));
		dev_info(dev, "%s: APU_RPC_CE_CTRL_RDATA = 0x%x\n",
			__func__, ioread32(apu->apu_rpc + 0x28));
		dev_info(dev, "%s: APU_RPC_TOP_SEL_2 = 0x%x\n",
			__func__, ioread32(apu->apu_rpc + 0x2c));
		dev_info(dev, "%s: APU_RPC_STATUS_1 = 0x%x\n",
			__func__, ioread32(apu->apu_rpc + 0x34));
		dev_info(dev, "%s: APU_RPC_INTF_PWR_RDY = 0x%x\n",
			__func__, ioread32(apu->apu_rpc + APU_RPC_INTF_PWR_RDY));

		dev_info(dev, "%s: MBOX0_RV_PWR_STA = 0x%x\n",
			__func__, ioread32(apu->apu_mbox + MBOX_RV_PWR_STA_FLG));
		apusys_rv_aee_warn("APUSYS_RV", "APUSYS_RV_TIMEOUT");
	}
}

static int mt6993_apu_power_init(struct mtk_apu *apu)
{
	/* init delay worker for power off detection */
	INIT_DELAYED_WORK(&timeout_work, apu_timeout_work);
	INIT_DELAYED_WORK(&apu_polling_on_work, &apu_polling_on_work_func);
	apu_workq = alloc_ordered_workqueue("apusys_rv_pwr", WQ_MEM_RECLAIM);
	g_apu = apu;

	return 0;
}

static int mt6993_rproc_init(struct mtk_apu *apu)
{
	int ret;
#if IS_ENABLED(CONFIG_PM_SLEEP)
	apu->wake_lock_ref_cnt = 0;
	spin_lock_init(&apu->wakelock_spinlock);
	ws = wakeup_source_register(NULL, "mt6993_apusys_rv");
	if (!ws) {
		dev_info(apu->dev, "%s: wakelock register fail\n", __func__);
		return -1;
	}

	/* cold boot done will call unlock */
	mt6993_apu_pwr_wake_lock(apu, APU_IPI_INIT);
#endif

	mutex_init(&reader_hw_sema_lock);

	/* TODO: change pwr_on_polling_dbg_mode to false to reduce latency */
	if ((apu->platdata->flags & F_BRINGUP) == 0)
		apu->pwr_on_polling_dbg_mode = true;
	else
		apu->pwr_on_polling_dbg_mode = false;
	apu->ce_dbg_polling_dump_mode = false;
	apu->apusys_rv_trace_on = false;

	is_under_lp_scp_recovery_flow = false;

	/* for cold boot(already power on by apu_top.ko) */
	ret = mt6993_cold_boot_power_on(apu);
	if (ret)
		dev_info(apu->dev, "%s: call mt6993_cold_boot_power_on fail(%d)\n",
			__func__, ret);

	return ret;
}

static int mt6993_rproc_exit(struct mtk_apu *apu)
{
#if IS_ENABLED(CONFIG_PM_SLEEP)
	wakeup_source_unregister(ws);
#endif
	cancel_delayed_work_sync(&apu_polling_on_work);

	return 0;
}

 static void mt6993_get_chip_ver(struct mtk_apu *apu)
 {
	struct device_node *node;
	struct tag_chipid *chip_id = NULL;
	int len;

	node = of_find_node_by_path("/chosen");
	if (!node)
		node = of_find_node_by_path("/chosen@0");

	if (!node) {
		pr_info("%s chosen node not found in device tree\n", __func__);
		return;
	}

	chip_id = (struct tag_chipid *)of_get_property(node, "atag,chipid", &len);
	if (!chip_id) {
		pr_info("%s could not found atag,chipid in chosen\n", __func__);
		return;
	}

	apu->chip_id.sw_ver = chip_id->sw_ver;

	dev_info(apu->dev, "%s: current sw version: %d\n", __func__, apu->chip_id.sw_ver);
 }

const struct mtk_apu_platdata mt6993_platdata = {
	.flags		= F_AUTO_BOOT | F_FAST_ON_OFF | F_APU_IPI_UT_SUPPORT |
					F_SMMU_SUPPORT | F_DEBUG_MEM_SUPPORT | F_PRELOAD_FIRMWARE |
					F_APUSYS_RV_TAG_SUPPORT | F_EXCEPTION_KE | F_AOV_UNSUPPORT |
					F_RV_BSP_RX_SUPPORT | F_COREDUMP_RV55 | F_SECURE_BOOT |
					F_CE_EXCEPTION_ON | F_SECURE_COREDUMP,
	.ops		= {
		.init	= mt6993_rproc_init,
		.exit	= mt6993_rproc_exit,
		.start	= mt6993_rproc_start,
		.stop	= mt6993_rproc_stop,
		.apu_memmap_init = mt6993_apu_memmap_init,
		.apu_memmap_remove = mt6993_apu_memmap_remove,
		.power_on_off = mt6993_power_on_off,
		.polling_rpc_status = mt6993_polling_rpc_status,
		.wake_lock = mt6993_apu_pwr_wake_lock,
		.wake_unlock = mt6993_apu_pwr_wake_unlock,
		.debug_info_dump = mt6993_debug_info_dump,
		.cg_gating = mt6993_rv_cg_gating,
		.cg_ungating = mt6993_rv_cg_ungating,
		.rv_cachedump = mt6993_rv_cachedump,
		.power_init = mt6993_apu_power_init,
		.ipi_send_pre = mt6993_ipi_send_pre,
		.irq_affin_init = mt6993_irq_affin_init,
		.irq_affin_set = mt6993_irq_affin_set,
		.irq_affin_unset = mt6993_irq_affin_unset,
		.irq_affin_clear = mt6993_irq_affin_clear,
		.irq_affin_online = mt6993_irq_affin_online,
		.irq_affin_offline = mt6993_irq_affin_offline,
		.ipi_clamp = mt6993_ipi_clamp,
		.timesync_update = mt6993_timesync_update,
		.mbox_counting_hw_sem_reader_trylock = mt6993_mbox_counting_hw_sem_reader_trylock,
		.mbox_counting_hw_sem_reader_unlock = mt6993_mbox_counting_hw_sem_reader_unlock,
		.get_chip_ver = mt6993_get_chip_ver,
	},
};
