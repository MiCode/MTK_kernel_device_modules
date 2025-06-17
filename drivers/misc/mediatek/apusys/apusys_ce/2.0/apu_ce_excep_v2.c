// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <mt-plat/aee.h>

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
#include <mt-plat/mrdump.h>
#endif

#include "apu.h"
#include "apu_ce_excep.h"
#include "apu_ce_excep_v2.h"
#include "apu_config.h"
#include "apusys_secure.h"
#include "apu_regdump.h"

/* apuce mmap address */
static void *apu_are_sram;
static void *apu_ce_reg;


enum CE_V2_JOB_ID {
	CE_V2_JOB_ID_TPPA_PLUS_BW_ACC = 0,
	CE_V2_JOB_ID_NORM2LP,
	CE_V2_JOB_ID_LP2NORM,
	CE_V2_JOB_ID_RESERVED_3,
	CE_V2_JOB_ID_RESERVED_4,
	CE_V2_JOB_ID_RESERVED_5,
	CE_V2_JOB_ID_RESERVED_6,
	CE_V2_JOB_ID_RESERVED_7,
	CE_V2_JOB_ID_RESERVED_8,
	CE_V2_JOB_ID_RESERVED_9,
	CE_V2_JOB_ID_RESERVED_10,
	CE_V2_JOB_ID_RESERVED_11,
	CE_V2_JOB_ID_RESERVED_12,
	CE_V2_JOB_ID_RESERVED_13,
	CE_V2_JOB_ID_ACX_MDLA_MTCMOS_OFF,
	CE_V2_JOB_ID_RCX_MDLA_MTCMOS_OFF,
	CE_V2_JOB_ID_RCX_WAKEUP,
	CE_V2_JOB_ID_RCX_SLEEP,
	CE_V2_JOB_ID_TPPA_PLUS_PSC,
	CE_V2_JOB_ID_MNOC_BW_JOB_KICKER,
	CE_V2_JOB_ID_BRISKET_OUTER_LOOP,
	CE_V2_JOB_ID_RESERVED_21,
	CE_V2_JOB_ID_BW_PREDICTION,
	CE_V2_JOB_ID_QOS_EVENT_DRIVEN,
	CE_V2_JOB_ID_RESERVED_24,
	CE_V2_JOB_ID_RESERVED_25,
	CE_V2_JOB_ID_SMMU_RESTORE,
	CE_V2_JOB_ID_RCX_NOC_BW_ACC,
	CE_V2_JOB_ID_ACX0_NOC_BW_ACC,
	CE_V2_JOB_ID_ACX1_NOC_BW_ACC,
	CE_V2_JOB_ID_NCX_NOC_BW_ACC,
	CE_V2_JOB_ID_DVFS,
	CE_V2_JOB_ID_MAX
};

#define CE_V2_SW_FLAG_MAX 16

const char *CE_V2_JOB_NAME[CE_V2_JOB_ID_MAX] = {
	"APUSYS_CE_LOGGER_UT",
	"APUSYS_CE_PSC",
	"APUSYS_CE_RESERVED_2",
	"APUSYS_CE_RESERVED_3",
	"APUSYS_CE_RESERVED_4",
	"APUSYS_CE_RESERVED_5",
	"APUSYS_CE_RESERVED_6",
	"APUSYS_CE_RESERVED_7",
	"APUSYS_CE_RESERVED_8",
	"APUSYS_CE_RESERVED_9",
	"APUSYS_CE_RESERVED_10",
	"APUSYS_CE_RESERVED_11",
	"APUSYS_CE_RESERVED_12",
	"APUSYS_CE_RESERVED_13",
	"APUSYS_CE_RESERVED_14",
	"APUSYS_CE_RESERVED_15",
	"APUSYS_CE_RCX_WAKEUP",
	"APUSYS_CE_RCX_SLEEP",
	"APUSYS_CE_MNOC_BW_ACC_QOS",
	"APUSYS_CE_AVS",
	"APUSYS_CE_OUTER_LOOP",
	"APUSYS_CE_RESERVED_21",
	"APUSYS_CE_BW_PREDICTION",
	"APUSYS_CE_QOS_EVENT_DRIVEN",
	"APUSYS_CE_RESERVED_24",
	"APUSYS_CE_RESERVED_25",
	"APUSYS_CE_SMMU_RESTORE",
	"APUSYS_CE_RCX_NOC_BW_ACC",
	"APUSYS_CE_RESERVED_28",
	"APUSYS_CE_RESERVED_29",
	"APUSYS_CE_POWER_BUDGET",
	"APUSYS_CE_DVFS"
};

const char *CE_V2_SW_FLAG_DISPATCH_NAME[CE_V2_SW_FLAG_MAX] = {
	"APUSYS_CE_SW_CUSTOMIZED_0",
	"APUSYS_CE_SW_CUSTOMIZED_1",
	"APUSYS_CE_SW_CUSTOMIZED_2",
	"APUSYS_CE_SW_CUSTOMIZED_3",
	"APUSYS_CE_SW_CUSTOMIZED_4",
	"APUSYS_CE_SW_CUSTOMIZED_5",
	"APUSYS_CE_SW_CUSTOMIZED_6",
	"APUSYS_CE_SW_CUSTOMIZED_7",
	"APUSYS_CE_SW_CUSTOMIZED_8",
	"APUSYS_CE_SW_CUSTOMIZED_9",
	"APUSYS_CE_SW_CUSTOMIZED_10",
	"APUSYS_CE_SW_CUSTOMIZED_11",
	"APUSYS_CE_SW_CUSTOMIZED_12",
	"APUSYS_CE_SW_CUSTOMIZED_13",
	"APUSYS_CE_SW_CUSTOMIZED_14",
	"APUSYS_CE_SW_CUSTOMIZED_15",
};

const enum CE_V2_JOB_ID CE_V2_HW_TIMER[] = {
	CE_V2_JOB_ID_TPPA_PLUS_PSC,
	CE_V2_JOB_ID_MNOC_BW_JOB_KICKER,
	CE_V2_JOB_ID_BRISKET_OUTER_LOOP,
	CE_V2_JOB_ID_MAX
};

const enum CE_V2_JOB_ID CE_V2_HW_TIMER_EXP_BYPASS[] = {
	CE_V2_JOB_ID_BRISKET_OUTER_LOOP,
};

struct apu_coredump_work_struct {
	struct mtk_apu *apu;
	struct work_struct work;
};

static int burst_intr_cnt;
static int exception_job_id = -1;
static int exception_sw_flag_id = -1;  /* sw customized exception */
static struct apu_ce_v2_ops *g_apu_ce_ops;
static struct platform_device *g_apu_pdev;

static struct apu_coredump_work_struct apu_ce_coredump_work;
#define APU_CE_DUMP_TIMEOUT_MS (1)
#define CHECK_BIT(var, pos) ((var) & (1<<(pos)))
#define CE_EXP_INTR_THRESHOLD 5
#define PROC_WRITE_TEMP_BUFF_SIZE (16)

#define PRELOAD_FW	(0)

#if PRELOAD_FW
static int apusys_rv_smc_call_short(struct device *dev, uint32_t smc_id, uint32_t a2)
{
	struct arm_smccc_res res;

	dev_info(dev, "%s: smc call %d\n", __func__, smc_id);

	if (smc_id != MTK_APUSYS_KERNEL_OP_APUSYS_RV_PWR_CTRL)
		dev_info(dev, "%s: smc call %d(a2 = %d)\n", __func__, smc_id, a2);

	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL, smc_id, a2, 0, 0, 0, 0, 0, &res);
	if (((int) res.a0) < 0)
		dev_info(dev, "%s: smc call %d return error(%ld)\n",
			__func__, smc_id, res.a0);

	return res.a0;
}
#endif

static uint32_t apusys_rv_smc_call(struct device *dev, uint32_t smc_id,
	uint32_t param0, uint32_t param1, uint32_t *ret0, uint32_t *ret1, uint32_t *ret2)
{
	struct arm_smccc_res res;

	dev_info(dev, "%s: smc call %d\n", __func__, smc_id);

	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL, smc_id,
				param0, param1, 0, 0, 0, 0, &res);

	if (res.a0 != 0) {
		dev_info(dev, "%s: smc call %d param0 %d param1 %d return error(%ld)\n",
			__func__, smc_id, param0, param1, res.a0);
	} else {
		if (ret0 != NULL)
			*ret0 = res.a1;
		if (ret1 != NULL)
			*ret1 = res.a2;
		if (ret2 != NULL)
			*ret2 = res.a3;

		dev_info(dev, "%s: smc call %d param0 %d param1 %d return (%ld %ld %ld %ld)\n",
			__func__, smc_id, param0, param1, res.a0, res.a1, res.a2, res.a3);
	}

	return res.a0;
}

int apu_ce_memmap_v2(struct platform_device *pdev,
		struct mtk_apu *apu)
{
	struct device *dev = apu->dev;

	apu_are_sram = ioremap(APU_V2_CE_SRAMBASE, APU_V2_ARE_SRAM_SIZE);
	if (IS_ERR((void const *)apu_are_sram)) {
		dev_info(dev, "apu_are_sram remap base fail\n");
		return -1;
	}

	apu_ce_reg = ioremap(APU_V2_ARE_REG_BASE, APU_V2_ARE_SRAM_SIZE);
	if (IS_ERR((void const *)apu_ce_reg)) {
		dev_info(dev, "apu_ce_reg remap base fail\n");
		return -1;
	}

	return 0;

}

static const char *get_ce_job_name_by_id(int32_t job_id)
{
	if ((job_id < CE_V2_JOB_ID_MAX) && (job_id >= 0))
		return CE_V2_JOB_NAME[job_id];
	else
		return "APUSYS_CE_UNDEFINED";
}

static const char *get_dispatch_key_by_sw_flag_id(int32_t flag_id)
{
	if ((flag_id < CE_V2_SW_FLAG_MAX) && (flag_id >= 0))
		return CE_V2_SW_FLAG_DISPATCH_NAME[flag_id];
	else
		return "APUSYS_CE_UNDEFINED";
}

uint32_t apu_ce_reg_dump_v2(struct device *dev)
{
	return apusys_rv_smc_call(dev,
		MTK_APUSYS_KERNEL_OP_APUSYS_CE_DEBUG_REGDUMP, 0, 0, NULL, NULL, NULL);
}

uint32_t apu_ce_sram_dump_v2(struct device *dev)
{
	return apusys_rv_smc_call(dev,
		MTK_APUSYS_KERNEL_OP_APUSYS_CE_SRAM_DUMP, 0, 0, NULL, NULL, NULL);
}

static void apu_ce_coredump_work_func(struct work_struct *p_work)
{
	struct apu_coredump_work_struct *apu_coredump_work =
			container_of(p_work, struct apu_coredump_work_struct, work);
	struct mtk_apu *apu = apu_coredump_work->apu;
	struct device *dev = apu->dev;

	dev_info(dev, "%s +\n", __func__);

	if ((exception_job_id >= 0) || (exception_sw_flag_id >= 0)) {
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_CE_SRAM_DUMP, 0, 0, NULL, NULL, NULL);

		apu_regdump();

		const char *crdispatch_key = ((exception_job_id >= 0)?
			get_ce_job_name_by_id(exception_job_id):
			get_dispatch_key_by_sw_flag_id(exception_sw_flag_id));

		if ((apu->platdata->flags & F_EXCEPTION_KE) && !apu->disable_ke &&
			(ktime_get() / 1000000) > BOOT_BYPASS_APU_KE_MS) {
			panic("APUSYS_CE exception: %s\n", crdispatch_key);
		} else {
			dev_info(dev, "%s: bypass KE due to %s%s%s\n", __func__,
				(apu->platdata->flags & F_EXCEPTION_KE) ? "":"F_EXCEPTION_KE not enabled",
				!apu->disable_ke ? "":"disabled by cmd",
				((ktime_get() / 1000000) > BOOT_BYPASS_APU_KE_MS) ? "":"bootup");

			apusys_ce_exception_aee_warn(crdispatch_key);
		}

		exception_sw_flag_id = -1;
		exception_job_id = -1;
	}
}

static int get_exception_job_id(struct device *dev, int32_t *job_id, bool *by_pass)
{
	uint32_t i, op, w1c_val = 0;
	uint32_t ce_task[4];
	int32_t exception_ce_id, exception_timer_id;
	uint32_t ce_flag = 0, ace_flag = 0, user_flag = 0;
	uint32_t axi_wr_status = 0, axi_rd_status = 0, apb_status = 0;

	op = GET_SMC_OP_V2(SMC_OP_APU_V2_ACE_ABN_IRQ_FLAG_CE,
					SMC_OP_APU_V2_ACE_ABN_IRQ_FLAG_ACE_SW,
					SMC_OP_APU_V2_ACE_ABN_IRQ_FLAG_USER);

	if (apusys_rv_smc_call(dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP,
			op, 0, &ce_flag, &ace_flag, &user_flag))
		return -1;

	dev_info(dev, "APU_ACE_ABN_IRQ_FLAG_CE: 0x%08x\n", ce_flag);
	dev_info(dev, "APU_ACE_ABN_IRQ_FLAG_ACE_SW: 0x%08x\n", ace_flag);
	dev_info(dev, "APU_ACE_ABN_IRQ_FLAG_USER: 0x%08x\n", user_flag);

	op = GET_SMC_OP_V2(SMC_OP_APU_V2_ACE_CE0_TASK_ING,
					SMC_OP_APU_V2_ACE_CE1_TASK_ING,
					SMC_OP_APU_V2_ACE_CE2_TASK_ING);

	if (apusys_rv_smc_call(dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP,
			op, 0, &ce_task[0], &ce_task[1], &ce_task[2]))
		return -1;

	dev_info(dev, "APU_ACE_CE0_TASK_ING: 0x%08x\n", ce_task[0]);
	dev_info(dev, "APU_ACE_CE1_TASK_ING: 0x%08x\n", ce_task[1]);
	dev_info(dev, "APU_ACE_CE2_TASK_ING: 0x%08x\n", ce_task[2]);

	op = GET_SMC_OP_V2(SMC_OP_APU_V2_ACE_CE3_TASK_ING,
					SMC_OP_APU_V2_ACE_AXI_MST_WR_STATUS_ERR,
					SMC_OP_APU_V2_ACE_AXI_MST_RD_STATUS_ERR);

	if (apusys_rv_smc_call(dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP,
			op, 0, &ce_task[3], &axi_wr_status, &axi_rd_status))
		return -1;

	dev_info(dev, "APU_ACE_CE3_TASK_ING: 0x%08x\n", ce_task[3]);
	dev_info(dev, "APU_ACE_AXI_MST_WR_STATUS_ERR: 0x%08x\n", axi_wr_status);
	dev_info(dev, "APU_ACE_AXI_MST_RD_STATUS_ERR: 0x%08x\n", axi_rd_status);

	if (ce_flag == 0 && ace_flag == 0 && user_flag == 0) {
		dev_info(dev, "no error flag\n");
		return -1;
	}

	exception_ce_id = -1;
	exception_timer_id = -1;
	exception_sw_flag_id = -1;

	if (ce_flag) {
		if (ce_flag & APU_V2_CE_0_IRQ_MASK)
			exception_ce_id = 0;
		else if (ce_flag & APU_V2_CE_1_IRQ_MASK)
			exception_ce_id = 1;
		else if (ce_flag & APU_V2_CE_2_IRQ_MASK)
			exception_ce_id = 2;
		else if (ce_flag & APU_V2_CE_3_IRQ_MASK)
			exception_ce_id = 3;
	} else if (ace_flag) {
		if (ace_flag & APU_V2_CE_MISS_TYPE2_REQ_FLAG_0_MSK) {
			exception_timer_id = 0;
			w1c_val = APU_V2_CE_MISS_TYPE2_REQ_FLAG_0_MSK;
		} else if (ace_flag & APU_V2_CE_MISS_TYPE2_REQ_FLAG_1_MSK) {
			exception_timer_id = 1;
			w1c_val = APU_V2_CE_MISS_TYPE2_REQ_FLAG_1_MSK;
		} else if (ace_flag & APU_V2_CE_MISS_TYPE2_REQ_FLAG_2_MSK) {
			exception_timer_id = 2;
			w1c_val = APU_V2_CE_MISS_TYPE2_REQ_FLAG_2_MSK;
		} else if (ace_flag & APU_V2_CE_MISS_TYPE2_REQ_FLAG_3_MSK) {
			exception_timer_id = 3;
			w1c_val = APU_V2_CE_MISS_TYPE2_REQ_FLAG_3_MSK;
		}

		else if (ace_flag & APU_V2_CE_NON_ALIGNED_APB_FLAG_MSK) {
			if (ace_flag & APU_V2_CE_NON_ALIGNED_APB_OUT_FLAG_MSK)
				apb_status = axi_wr_status;
			else if (ace_flag & APU_V2_CE_NON_ALIGNED_APB_IN_FLAG_MSK)
				apb_status = axi_rd_status;

			if (apb_status & APU_V2_CE_APB_ERR_STATUS_CE0_MSK)
				exception_ce_id = 0;
			else if (apb_status & APU_V2_CE_APB_ERR_STATUS_CE1_MSK)
				exception_ce_id = 1;
			else if (apb_status & APU_V2_CE_APB_ERR_STATUS_CE2_MSK)
				exception_ce_id = 2;
			else if (apb_status & APU_V2_CE_APB_ERR_STATUS_CE3_MSK)
				exception_ce_id = 3;
		}
	} else if (user_flag) {
		exception_sw_flag_id = fls(user_flag) - 1; // get MSB position
		dev_info(dev, "CE exception by sw customized, flag id: %d\n", exception_sw_flag_id);
	}

	if (exception_ce_id < 0 && exception_timer_id < 0)
		return -1;

	if (exception_ce_id >= 0) {
		dev_info(dev, "CE_%d cause exception\n", exception_ce_id);
		*job_id = (ce_task[exception_ce_id] >> APU_V2_CE_TASK_JOB_SFT) & APU_V2_CE_TASK_JOB_MSK;

		dev_info(dev, "CE_%d is running job %d (%s)\n",
			exception_ce_id, *job_id, get_ce_job_name_by_id(*job_id));
	}
	if (exception_timer_id >= 0) {
		dev_info(dev, "HW_Timer_%d cause exception\n", exception_timer_id);
		*job_id = CE_V2_HW_TIMER[exception_timer_id];

		dev_info(dev, "HW_Timer_%d mapping to job id %d (%s)\n",
			exception_timer_id, *job_id, get_ce_job_name_by_id(*job_id));

		for (i = 0; i < sizeof(CE_V2_HW_TIMER_EXP_BYPASS) / sizeof(enum CE_V2_JOB_ID); i++)
			if (*job_id == CE_V2_HW_TIMER_EXP_BYPASS[i]) {
				dev_info(dev, "Bypass ce exception, job id %d\n", *job_id);
				dev_info(dev, "W1C APU_ACE_ABN_IRQ_FLAG_ACE_SW, val 0x%08x\n", w1c_val);
				*by_pass = true;

				apusys_rv_smc_call(dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REG_WRITE,
					SMC_OP_APU_V2_ACE_ABN_IRQ_FLAG_ACE_SW, w1c_val, NULL, NULL, NULL);
			}
	}

	return 0;
}

static void log_ce_register(struct device *dev)
{
	uint32_t op, res0 = 0, res1 = 0, res2 = 0;

	op = GET_SMC_OP_V2(SMC_OP_APU_V2_ACE_ABN_IRQ_FLAG_CE,
					SMC_OP_APU_V2_ACE_ABN_IRQ_FLAG_ACE_SW,
					SMC_OP_APU_V2_ACE_ABN_IRQ_FLAG_USER);

	if (apusys_rv_smc_call(
			dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP, op, 0, &res0, &res1, &res2) == 0) {
		dev_info(dev, "APU_ACE_ABN_IRQ_FLAG_CE: 0x%08x\n", res0);
		dev_info(dev, "APU_ACE_ABN_IRQ_FLAG_ACE_SW: 0x%08x\n", res1);
		dev_info(dev, "APU_CE2_ACE_ABN_IRQ_FLAG_USER: 0x%08x\n", res2);
	}

	op = GET_SMC_OP_V2(SMC_OP_APU_V2_CE0_RUN_INSTR,
					SMC_OP_APU_V2_CE1_RUN_INSTR,
					SMC_OP_APU_V2_CE2_RUN_INSTR);

	if (apusys_rv_smc_call(
			dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP, op, 0, &res0, &res1, &res2) == 0) {
		dev_info(dev, "APU_CE0_RUN_INSTR: 0x%08x\n", res0);
		dev_info(dev, "APU_CE1_RUN_INSTR: 0x%08x\n", res1);
		dev_info(dev, "APU_CE2_RUN_INSTR: 0x%08x\n", res2);
	}

	op = GET_SMC_OP_V2(SMC_OP_APU_V2_CE3_RUN_INSTR,
					SMC_OP_APU_V2_CE0_RUN_PC,
					SMC_OP_APU_V2_CE1_RUN_PC);

	if (apusys_rv_smc_call(
			dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP, op, 0, &res0, &res1, &res2) == 0) {
		dev_info(dev, "APU_CE3_RUN_INSTR: 0x%08x\n", res0);
		dev_info(dev, "APU_CE0_RUN_PC: 0x%08x\n", res1);
		dev_info(dev, "APU_CE1_RUN_PC: 0x%08x\n", res2);
	}

	op = GET_SMC_OP_V2(SMC_OP_APU_V2_CE2_RUN_PC,
					SMC_OP_APU_V2_CE3_RUN_PC,
					SMC_OP_APU_V2_ACE_CMD_Q_STATUS_0);

	if (apusys_rv_smc_call(
			dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP, op, 0, &res0, &res1, &res2) == 0) {
		dev_info(dev, "APU_CE2_RUN_PC: 0x%08x\n", res0);
		dev_info(dev, "APU_CE3_RUN_PC: 0x%08x\n", res1);
		dev_info(dev, "APU_ACE_CMD_Q_STATUS_0: 0x%08x\n", res2);
	}

	op = GET_SMC_OP_V2(SMC_OP_APU_V2_ACE_CMD_Q_STATUS_1,
					SMC_OP_APU_V2_ACE_CMD_Q_STATUS_2,
					SMC_OP_APU_V2_ACE_CMD_Q_STATUS_3);

	if (apusys_rv_smc_call(
		dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP, op, 0, &res0, &res1, &res2) == 0) {
		dev_info(dev, "APU_ACE_CMD_Q_STATUS_1: 0x%08x\n", res0);
		dev_info(dev, "APU_ACE_CMD_Q_STATUS_2: 0x%08x\n", res1);
		dev_info(dev, "APU_ACE_CMD_Q_STATUS_3: 0x%08x\n", res2);
	}

	op = GET_SMC_OP_V2(SMC_OP_APU_V2_ACE_CMD_Q_STATUS_4,
					SMC_OP_APU_V2_ACE_CMD_Q_STATUS_5,
					SMC_OP_APU_V2_ACE_CMD_Q_STATUS_6);

	if (apusys_rv_smc_call(
		dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP, op, 0, &res0, &res1, &res2) == 0) {
		dev_info(dev, "APU_ACE_CMD_Q_STATUS_4: 0x%08x\n", res0);
		dev_info(dev, "APU_ACE_CMD_Q_STATUS_5: 0x%08x\n", res1);
		dev_info(dev, "APU_ACE_CMD_Q_STATUS_6: 0x%08x\n", res2);
	}

	op = GET_SMC_OP_V2(SMC_OP_APU_V2_CE0_FOOTPRINT_0,
					SMC_OP_APU_V2_CE0_FOOTPRINT_1,
					SMC_OP_APU_V2_CE0_FOOTPRINT_2);

	if (apusys_rv_smc_call(
		dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP, op, 0, &res0, &res1, &res2) == 0) {
		dev_info(dev, "APU_CE0_FOOTPRINT_0: 0x%08x\n", res0);
		dev_info(dev, "APU_CE0_FOOTPRINT_1: 0x%08x\n", res1);
		dev_info(dev, "APU_CE0_FOOTPRINT_2: 0x%08x\n", res2);
	}

	op = GET_SMC_OP_V2(SMC_OP_APU_V2_CE0_FOOTPRINT_3,
					SMC_OP_APU_V2_CE1_FOOTPRINT_0,
					SMC_OP_APU_V2_CE1_FOOTPRINT_1);

	if (apusys_rv_smc_call(
		dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP, op, 0, &res0, &res1, &res2) == 0) {
		dev_info(dev, "APU_CE0_FOOTPRINT_3: 0x%08x\n", res0);
		dev_info(dev, "APU_CE1_FOOTPRINT_0: 0x%08x\n", res1);
		dev_info(dev, "APU_CE1_FOOTPRINT_1: 0x%08x\n", res2);
	}

	op = GET_SMC_OP_V2(SMC_OP_APU_V2_CE1_FOOTPRINT_2,
					SMC_OP_APU_V2_CE1_FOOTPRINT_3,
					SMC_OP_APU_V2_CE2_FOOTPRINT_0);

	if (apusys_rv_smc_call(
		dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP, op, 0, &res0, &res1, &res2) == 0) {
		dev_info(dev, "APU_CE1_FOOTPRINT_2: 0x%08x\n", res0);
		dev_info(dev, "APU_CE1_FOOTPRINT_3: 0x%08x\n", res1);
		dev_info(dev, "APU_CE2_FOOTPRINT_0: 0x%08x\n", res2);
	}

	op = GET_SMC_OP_V2(SMC_OP_APU_V2_CE2_FOOTPRINT_1,
					SMC_OP_APU_V2_CE2_FOOTPRINT_2,
					SMC_OP_APU_V2_CE2_FOOTPRINT_3);

	if (apusys_rv_smc_call(
		dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP, op, 0, &res0, &res1, &res2) == 0) {
		dev_info(dev, "APU_CE2_FOOTPRINT_1: 0x%08x\n", res0);
		dev_info(dev, "APU_CE2_FOOTPRINT_2: 0x%08x\n", res1);
		dev_info(dev, "APU_CE2_FOOTPRINT_3: 0x%08x\n", res2);
	}

	op = GET_SMC_OP_V2(SMC_OP_APU_V2_CE3_FOOTPRINT_0,
					SMC_OP_APU_V2_CE3_FOOTPRINT_1,
					SMC_OP_APU_V2_CE3_FOOTPRINT_2);

	if (apusys_rv_smc_call(
		dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP, op, 0, &res0, &res1, &res2) == 0) {
		dev_info(dev, "APU_CE3_FOOTPRINT_0: 0x%08x\n", res0);
		dev_info(dev, "APU_CE3_FOOTPRINT_1: 0x%08x\n", res1);
		dev_info(dev, "APU_CE3_FOOTPRINT_2: 0x%08x\n", res2);
	}

	op = GET_SMC_OP_V2(SMC_OP_APU_V2_CE3_FOOTPRINT_3,
					SMC_OP_APU_V2_ACE_APB_MST_IN_STATUS,
					0);

	if (apusys_rv_smc_call(
		dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP, op, 0, &res0, &res1, &res2) == 0) {
		dev_info(dev, "APU_CE3_FOOTPRINT_3: 0x%08x\n", res0);
		dev_info(dev, "SMC_OP_APU_V2_ACE_APB_MST_IN_STATUS: 0x%08x\n", res1);
		dev_info(dev, "SMC_OP_APU_V2_ACE_AXI_MST_WR_STATUS: 0x%08x\n", res2);
	}
}

static irqreturn_t apu_ce_isr(int irq, void *private_data)
{
	struct mtk_apu *apu = (struct mtk_apu *) private_data;
	struct device *dev = apu->dev;
	bool by_pass = false;
	int ret;

	dev_info(dev, "%s ,are_abnormal_irq bottom\n", __func__);

	if (g_apu_ce_ops->check_apu_exp_irq(apu)) {

		/* find exception job id */
		ret = get_exception_job_id(dev, &exception_job_id, &by_pass);
		dev_info(dev, "CE exception job id %d\n", exception_job_id);

		if (by_pass)
			return IRQ_HANDLED;

		if (ret && burst_intr_cnt < CE_EXP_INTR_THRESHOLD) {
			dev_info(dev, "get_exception_job_id fail, intr cnt %d\n", burst_intr_cnt);
			burst_intr_cnt++;
			return IRQ_HANDLED;
		}

		disable_irq_nosync(apu->ce_exp_irq_number);

		apusys_rv_smc_call(dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REG_WRITE,
			SMC_OP_APU_V2_CE0_STEP, 0x1, NULL, NULL, NULL);
		apusys_rv_smc_call(dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REG_WRITE,
			SMC_OP_APU_V2_CE1_STEP, 0x1, NULL, NULL, NULL);
		apusys_rv_smc_call(dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REG_WRITE,
			SMC_OP_APU_V2_CE2_STEP, 0x1, NULL, NULL, NULL);
		apusys_rv_smc_call(dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REG_WRITE,
			SMC_OP_APU_V2_CE3_STEP, 0x1, NULL, NULL, NULL);

		dev_info(dev, "Pause all CE\n");

		/* log important register */
		log_ce_register(dev);

		/* dump ce register to apusys_ce_fw_sram buffer */
		if (apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_CE_DEBUG_REGDUMP, 0, 0, NULL, NULL, NULL) == 0) {
			dev_info(dev, "Dump CE register to apusys_ce_fw_sram\n");
		}

		/**
		 * schedule task, the task will
		 * 1. dump ARE SRAM to apusys_ce_fw_sram buffer
		 * 2. dump register to apusys_regdump buffer
		 * 3. trigger aee by ce exception
		 */
		schedule_work(&(apu_ce_coredump_work.work));
	}
	return IRQ_HANDLED;
}

static int apu_ce_irq_register(struct platform_device *pdev, struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int ret = 0;

	apu->ce_exp_irq_number = platform_get_irq_byname(pdev, "ce_exp_irq");

	dev_info(dev, "%s: ce_exp_irq_number = %d\n", __func__, apu->ce_exp_irq_number);

	ret = devm_request_threaded_irq(
		&pdev->dev, apu->ce_exp_irq_number,
		NULL, apu_ce_isr,
		IRQF_ONESHOT,
		"apusys_ce_excep", apu);

	if (ret < 0)
		dev_info(dev, "%s: devm_request_irq Failed to request irq %d: %d\n",
				__func__, apu->ce_exp_irq_number, ret);

	return ret;
}

void apu_ce_mrdump_register_v2(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int ret = 0;
	unsigned long base_va = 0;
	unsigned long base_pa = 0;
	unsigned long size = 0;

	//CE FW + CE sram start addr & total size
	base_pa = apu->apusys_aee_coredump_mem_start +
			apu->apusys_aee_coredump_info->ce_bin_ofs;
	base_va = (unsigned long) apu->apu_aee_coredump_mem_base +
			apu->apusys_aee_coredump_info->ce_bin_ofs;

	size = apu->apusys_aee_coredump_info->ce_bin_sz +
		apu->apusys_aee_coredump_info->are_sram_sz;
	dev_info(dev, "%s: ce_bin_sz = 0x%x, are_sram_sz = 0x%x\n", __func__,
		apu->apusys_aee_coredump_info->ce_bin_sz,
		apu->apusys_aee_coredump_info->are_sram_sz);

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	ret = mrdump_mini_add_extra_file(base_va, base_pa, size,
		"APUSYS_CE_FW_SRAM");
#endif
	if (ret)
		dev_info(dev, "%s: APUSYS_CE_FW_SRAM add fail(%d)\n",
			__func__, ret);
}

static ssize_t dump_ce_fw_sram_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	char tmp[PROC_WRITE_TEMP_BUFF_SIZE] = {0};
	int ret;
	unsigned int input = 0;

	if (count >= PROC_WRITE_TEMP_BUFF_SIZE - 1)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);
	if (ret) {
		dev_info(&g_apu_pdev->dev, "%s: copy_from_user failed (%d)\n", __func__, ret);
		goto out;
	}

	tmp[count] = '\0';
	ret = kstrtouint(tmp, PROC_WRITE_TEMP_BUFF_SIZE, &input);
	if (ret) {
		dev_info(&g_apu_pdev->dev, "%s: kstrtouint failed (%d)\n", __func__, ret);
		goto out;
	}

	dev_info(&g_apu_pdev->dev, "%s: dump ops (0x%x)\n", __func__, input);

	if (input & (0x1)) {
		if (apu_ce_reg_dump(&g_apu_pdev->dev) == 0)
			dev_info(&g_apu_pdev->dev, "%s: dump ce register success\n", __func__);
		else
			dev_info(&g_apu_pdev->dev, "%s: dump ce register smc call fail\n", __func__);
	}
	if (input & (0x2)) {
		if (apu_ce_sram_dump(&g_apu_pdev->dev) == 0)
			dev_info(&g_apu_pdev->dev, "%s: dump are sram success\n", __func__);
		else
			dev_info(&g_apu_pdev->dev, "%s: dump are sram call fail\n", __func__);
	}
out:
	return count;
}

static int ce_fw_sram_show(struct seq_file *s, void *v)
{
	uint32_t *ce_reg_addr = NULL;
	uint32_t *ce_sram_addr = NULL;
	uint32_t start, end, size, offset = 0;
	struct mtk_apu *apu = (struct mtk_apu *)platform_get_drvdata(g_apu_pdev);

	ce_reg_addr = (uint32_t *)((unsigned long)apu->apu_aee_coredump_mem_base +
		(unsigned long)apu->apusys_aee_coredump_info->ce_bin_ofs);

	while (ce_reg_addr[offset++] == APU_V2_CE_REG_DUMP_MAGIC_NUM) {
		start = ce_reg_addr[offset++];
		end = ce_reg_addr[offset++];
		size = end - start;

		seq_printf(s, "---- dump ce register from 0x%08x to 0x%08x ----\n",
			start, end - 4);

		seq_hex_dump(s, "", DUMP_PREFIX_OFFSET, 16, 4, ce_reg_addr + offset, size, false);
		offset += size / 4;
	}

	seq_printf(s, "---- dump ce sram start from 0x%08x ----\n", APU_V2_CE_SRAMBASE);
	ce_sram_addr = (uint32_t *)((unsigned long)apu->apu_aee_coredump_mem_base +
		(unsigned long)apu->apusys_aee_coredump_info->are_sram_ofs);
	size = apu->apusys_aee_coredump_info->are_sram_sz;

	seq_hex_dump(s, "", DUMP_PREFIX_OFFSET, 16, 4, ce_sram_addr, size, false);

	return 0;
}

static int ce_fw_sram_sqopen(struct inode *inode, struct file *file)
{
	return single_open(file, ce_fw_sram_show, NULL);
}

static const struct proc_ops ce_fw_sram_file_ops = {
	.proc_open		= ce_fw_sram_sqopen,
	.proc_write     = dump_ce_fw_sram_write,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release
};

void apu_ce_procfs_init_v2(struct platform_device *pdev,
	struct proc_dir_entry *procfs_root)
{
	int ret;
	struct proc_dir_entry *ce_fw_sram_seqlog;

	ce_fw_sram_seqlog = proc_create("apusys_ce_fw_sram", 0440,
		procfs_root, &ce_fw_sram_file_ops);

	ret = IS_ERR_OR_NULL(ce_fw_sram_seqlog);
	if (ret)
		dev_info(&pdev->dev,
			"(%d)failed to create apusys_rv node(apusys_ce_fw_sram)\n", ret);

}

void apu_ce_procfs_remove_v2(struct platform_device *pdev,
	struct proc_dir_entry *procfs_root)
{
	remove_proc_entry("apusys_ce_fw_sram", procfs_root);
}


static int gernel_check_apu_exp_irq(struct mtk_apu *apu)
{
	return 1;
}

static int mt6993_check_apu_exp_irq(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int ret = 0;

	ret = apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_OP_DECODE_APU_EXP_IRQ,
			0, 0, NULL, NULL, NULL);

	dev_info(dev, "%s: apu_exp_id: %x\n", __func__, ret);

	if (CHECK_BIT(ret, MT6993_ARE_ABNORMAL_IRQ_BIT) != 0)
		return 1;
	else
		return 0;
}


const struct apu_ce_v2_ops v2_apu_ce_ops = {
	.check_apu_exp_irq = gernel_check_apu_exp_irq,
};

const struct apu_ce_v2_ops mt6993_apu_ce_ops = {
	.check_apu_exp_irq = mt6993_check_apu_exp_irq,
};

static const struct of_device_id apu_ce_of_match[] = {
	{ .compatible = "mediatek,mt6993-apusys_rv", .data = &v2_apu_ce_ops},
	{},
};

int apu_ce_excep_is_compatible_v2(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(apu_ce_of_match); i++)
		if (of_device_is_compatible(
				pdev->dev.of_node, apu_ce_of_match[i].compatible))
			return 1;

	return 0;
}

int apu_ce_bin_init_v2(struct platform_device *pdev, struct mtk_apu *apu)
{
	int ret = 0;
	struct device *dev = apu->dev;

	g_apu_pdev = pdev;
	dev_info(dev, "%s +\n", __func__);
#if PRELOAD_FW
	apusys_rv_smc_call_short(dev, MTK_APUSYS_KERNEL_OP_APUSYS_SETUP_CE_BIN, 0);
#endif
	return ret;
}

int apu_load_ce_bin_v2(struct platform_device *pdev, struct mtk_apu *apu)
{
	int ret = 0;
	struct device *dev = apu->dev;

	g_apu_pdev = pdev;
	dev_info(dev, "%s +\n", __func__);

	apu_are_sram = ioremap(APU_V2_CE_SRAMBASE, APU_V2_ARE_SRAM_SIZE);
	if (IS_ERR((void const *)apu_are_sram)) {
		dev_info(dev, "apu_are_sram remap base fail\n");
		return -1;
	}

	apu_ce_reg = ioremap(APU_V2_ARE_REG_BASE, APU_V2_ARE_SRAM_SIZE);
	if (IS_ERR((void const *)apu_ce_reg)) {
		dev_info(dev, "apu_ce_reg remap base fail\n");
		return -1;
	}

	return ret;
}

int apu_ce_excep_init_v2(struct platform_device *pdev, struct mtk_apu *apu)
{
	int i, ret = 0;
	struct device *dev = apu->dev;

	g_apu_pdev = pdev;
	dev_info(dev, "%s +\n", __func__);

	for (i = 0; i < ARRAY_SIZE(apu_ce_of_match); i++)
		if (of_device_is_compatible(
				pdev->dev.of_node, apu_ce_of_match[i].compatible)) {
			g_apu_ce_ops = (struct apu_ce_v2_ops *)apu_ce_of_match[i].data;
			break;
		}

	if (g_apu_ce_ops == NULL)
		return -ENODEV;

	apusys_rv_smc_call(dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_MASK_INIT,
		0, 0, NULL, NULL, NULL);

	INIT_WORK(&(apu_ce_coredump_work.work), &apu_ce_coredump_work_func);
	apu_ce_coredump_work.apu = apu;
	ret = apu_ce_irq_register(pdev, apu);
	if (ret < 0)
		return ret;

	return ret;
}

void apu_ce_excep_remove_v2(struct platform_device *pdev, struct mtk_apu *apu)
{
	struct device *dev = apu->dev;

	disable_irq(apu->ce_exp_irq_number);
	dev_info(dev, "%s: disable ce_exp_irq (%d)\n", __func__,
		apu->ce_exp_irq_number);

	cancel_work_sync(&(apu_ce_coredump_work.work));
	g_apu_ce_ops = NULL;
	g_apu_pdev = NULL;
}

int is_apu_ce_excep_init_v2(void)
{
	return g_apu_pdev != NULL;
}
