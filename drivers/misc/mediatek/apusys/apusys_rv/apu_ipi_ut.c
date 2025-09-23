// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/lockdep.h>
#include <linux/time64.h>
#include <linux/kernel.h>
#include <linux/ratelimit.h>
#include <linux/rpmsg.h>
#include <linux/delay.h>
#include <linux/random.h>

#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#endif

#include <mt-plat/aee.h>
#include "mtk_apu_rpmsg.h"
#include "apu.h"
#include "apu_excep.h"
#include "apusys_rv_trace.h"

#define RV_BSP_RX_MAX_DATA (32)
enum {
	RV_BSP_RX_CMD_UT = 0,
	RV_BSP_RX_CMD_ERR_REPORT,
	MAX_RV_BSP_RX_CMD,
};

struct rv_bsp_rx_ipi_data {
	uint32_t cmd_id;
	uint32_t reason;
};

struct rv_bsp_rx_rpmsg_device {
	struct rpmsg_endpoint *ept;
	struct rpmsg_device *rpdev;
};

static struct mtk_apu *g_apu;
static struct rv_bsp_rx_rpmsg_device rv_bsp_rx_rpm_dev;

/* #define APU_IPI_USE_PROCFS */

#if IS_ENABLED(CONFIG_DEBUG_FS)

#define APU_DMA_UT_SIZE (16)
#define APU_IPI_UT_MAX_DATA (32)
enum {
	CMD_UT = 0,
	CMD_UT_RANDOM,
	CMD_GET_PWR_ON_OFF_TIME,
	CMD_PWR_TIME_PROFILE_INTERNAL,
	CMD_UT_IPI_OUTBOX_FUZZ_WRITE,
	CMD_S5_IDEL_DBG,
	CMD_DEEP_IDEL_DBG,
	CMD_PANIC,
	CMD_S5_IDLE_PROFILE,
	CMD_DMA_UT,
	MAX_CMD_UT_ID,
};

struct apu_ipi_ut_ipi_data {
	uint32_t cmd_id;
	uint32_t data[APU_IPI_UT_MAX_DATA];
};

struct apu_ipi_ut_rpmsg_device {
	struct rpmsg_endpoint *ept;
	struct rpmsg_device *rpdev;
	struct completion ack;
};

struct s5_idle_ts {
	uint32_t last;
	uint32_t max;
	uint32_t avg;
};

static struct apu_ipi_ut_rpmsg_device apu_ipi_ut_rpm_dev;
static struct mutex apu_ipi_ut_mtx;

static uint32_t rcx_on_ce_ts_last;
static uint32_t rcx_off_ce_ts_last;
static uint32_t rcx_on_ce_ts_avg;
static uint32_t rcx_off_ce_ts_avg;
static uint32_t rcx_on_ce_ts_max;
static uint32_t rcx_off_ce_ts_max;

static uint32_t warmboot_on_ts_last;
static uint32_t dpidle_off_ts_last;
static uint32_t smmu_hw_sem_ts_last;
static uint32_t dvfs_skip_ts_last;
static uint32_t warmboot_on_ts_avg;
static uint32_t dpidle_off_ts_avg;
static uint32_t smmu_hw_sem_ts_avg;
static uint32_t dvfs_skip_ts_avg;
static uint32_t warmboot_on_ts_max;
static uint32_t dpidle_off_ts_max;
static uint32_t smmu_hw_sem_ts_max;
static uint32_t dvfs_skip_ts_max;
static uint32_t smmu_hw_sem_ts_larger_1ms_cnt;

static uint64_t apu_on_ts_last;
static uint64_t apu_off_ts_last;
static uint64_t apu_on_ts_avg;
static uint64_t apu_off_ts_avg;
static uint64_t apu_on_ts_max;
static uint64_t apu_off_ts_max;
static uint64_t apu_on_ts_acc;
static uint64_t apu_off_ts_acc;

static uint64_t smc_on_ts_last;
static uint64_t smc_off_ts_last;
static uint64_t smc_on_ts_avg;
static uint64_t smc_off_ts_avg;
static uint64_t smc_on_ts_max;
static uint64_t smc_off_ts_max;
static uint64_t smc_on_ts_acc;
static uint64_t smc_off_ts_acc;

static uint64_t apu_on_over_1ms_cnt;
static uint64_t apu_off_over_1ms_cnt;

static uint64_t apu_on_ts_cnt;
static uint64_t apu_off_ts_cnt;

static uint32_t boot_count;

struct s5_idle_ts s5_idle_overall_ts;
struct s5_idle_ts s5_idle_wfi_ts;
struct s5_idle_ts s5_idle_enter_ts;
struct s5_idle_ts s5_idle_leave_ts;
struct s5_idle_ts s5_idle_sw_enter_ts;
struct s5_idle_ts s5_idle_sw_leave_ts;
struct s5_idle_ts s5_idle_rcx_on_ce_ts;
struct s5_idle_ts s5_idle_rcx_off_ce_ts;

uint32_t s5_idle_func_trigger_cnt;
uint32_t s5_idle_dpidle_skip_cnt;
uint32_t s5_idle_ipi_skip_cnt;
uint32_t s5_idle_wakeup_cnt;

void apu_power_on_off_profile(u32 on, u32 off,
	uint64_t time_diff_ns, uint64_t time_diff_ns2)
{
	if (on) {
		apu_on_ts_last = time_diff_ns/1000;
		apu_on_ts_cnt++;
		if (apu_on_ts_last >= 1000)
			apu_on_over_1ms_cnt++;
		apu_on_ts_acc += apu_on_ts_last;
		if (apu_on_ts_cnt != 0)
			apu_on_ts_avg = apu_on_ts_acc / apu_on_ts_cnt;
		apu_on_ts_max = max(apu_on_ts_max, apu_on_ts_last);

		smc_on_ts_last = time_diff_ns2/1000;
		smc_on_ts_acc += smc_on_ts_last;
		if (apu_on_ts_cnt != 0)
			smc_on_ts_avg = smc_on_ts_acc / apu_on_ts_cnt;
		smc_on_ts_max = max(smc_on_ts_max, smc_on_ts_last);
	} else if (off) {
		apu_off_ts_last = time_diff_ns/1000;
		apu_off_ts_cnt++;
		if (apu_off_ts_last >= 1000)
			apu_off_over_1ms_cnt++;
		apu_off_ts_acc += apu_off_ts_last;
		if (apu_off_ts_cnt != 0)
			apu_off_ts_avg = apu_off_ts_acc / apu_off_ts_cnt;
		apu_off_ts_max = max(apu_off_ts_max, apu_off_ts_last);

		smc_off_ts_last = time_diff_ns2/1000;
		smc_off_ts_acc += smc_off_ts_last;
		if (apu_off_ts_cnt != 0)
			smc_off_ts_avg = smc_off_ts_acc / apu_off_ts_cnt;
		smc_off_ts_max = max(smc_off_ts_max, smc_off_ts_last);
	}
}

static int apu_ipi_ut_send(struct apu_ipi_ut_ipi_data *d, bool wait_ack)
{
	int ret = 0, ret2 = 0, i = 0, retry_cnt = 10;
	int size = 256, num;
	uint8_t buf[256], *ptr = buf;
	struct mtk_apu *apu = g_apu;
	struct mtk_apu_hw_ops *hw_ops;
	struct timespec64 ts, te;
	bool polling_mode = false;

	if (!apu) {
		pr_info("%s: apu == NULL\n", __func__);
		return -1;
	}
	hw_ops = &apu->platdata->ops;

	if (d->cmd_id == CMD_PWR_TIME_PROFILE_INTERNAL &&
		hw_ops->polling_rpc_status && d->data[0] == 1)
		polling_mode = true;

	if (!apu_ipi_ut_rpm_dev.ept) {
		pr_info("%s: apu_ipi_ut_rpm_dev.ept == NULL\n", __func__);
		return -1;
	}

	if (wait_ack) {
		mutex_lock(&apu_ipi_ut_mtx);
		reinit_completion(&apu_ipi_ut_rpm_dev.ack);
	}

	num = snprintf(ptr, size, "%s: cmd_id = %d, data = ",
		__func__, d->cmd_id);
	if (num <= 0) {
		pr_info("%s: snprintf return error(num = %d)\n",
			__func__, num);
		return num;
	}
	size -= num;
	ptr += num;

	for (i = 0; i < APU_IPI_UT_MAX_DATA; i++) {
		num = snprintf(ptr, size, "%u ", d->data[i]);
		if (num <= 0) {
			pr_info("%s: snprintf return error(num = %d), i = %d\n",
				__func__, num, i);
			return num;
		}
		size -= num;
		ptr += num;
	}
	/* pr_info("%s\n", buf); */

	if (polling_mode)
		ktime_get_ts64(&ts);

	/* power on */
	ret = rpmsg_sendto(apu_ipi_ut_rpm_dev.ept, NULL, 1, 0);
	if (ret && ret != -EOPNOTSUPP) {
		pr_info("%s: rpmsg_sendto(power on) fail(%d)\n", __func__, ret);
		goto out;
	}

	if (polling_mode) {
		ret = hw_ops->polling_rpc_status(apu, 1, 1000000);
		if (ret) {
			pr_info("%s: polling rpc timeout(%d)\n", __func__, ret);
		} else {
			ktime_get_ts64(&te);
			ts = timespec64_sub(te, ts);
			pr_info("%s: diff = %llu ns\n", __func__, timespec64_to_ns(&ts));
		}
	}

	for (i = 1; i <= retry_cnt; i++) {
		ret = rpmsg_send(apu_ipi_ut_rpm_dev.ept, d, sizeof(*d));

		/* send busy, retry */
		if (ret == -EBUSY || ret == -EAGAIN) {
			pr_info("%s: re-send ipi(retry_cnt = %d/%d)\n", __func__, i, retry_cnt);
			mdelay(50);
			continue;
		}
		break;
	}
	if (ret) {
		pr_info("%s: rpmsg_send fail(%d)\n", __func__, ret);
		/* power off to restore ref cnt */
		ret2 = rpmsg_sendto(apu_ipi_ut_rpm_dev.ept, NULL, 0, 1);
		if (ret2 && ret2 != -EOPNOTSUPP)
			pr_info("%s: rpmsg_sendto(power off) fail(%d)\n", __func__, ret2);
		goto out;
	}

	if (wait_ack) {
		ret = wait_for_completion_timeout(
				&apu_ipi_ut_rpm_dev.ack, msecs_to_jiffies(1000));
		if (ret == 0) {
			pr_info("%s: wait for completion timeout\n", __func__);
			ret = -1;
		} else {
			ret = 0;
		}
	}

out:
	if (wait_ack)
		mutex_unlock(&apu_ipi_ut_mtx);

	return ret;
}

static int apu_dma_ut(int cmd, unsigned int *args)
{
	uint32_t i;
	uint8_t *data;
	void *buff_va;
	dma_addr_t buff_iova;
	struct apu_ipi_ut_ipi_data d;
	struct mtk_apu *apu = g_apu;
	struct device *dev = apu->dev;

	dev_info(dev, "%s: cmd: %d, args[0]: %u, args[1]: %u\n",
		__func__, cmd, args[0], args[1]);

	if (cmd != CMD_DMA_UT) {
		dev_info(dev, "%s: invalid cmd id %d\n", __func__, cmd);
		return -EINVAL;
	}

	if (args[0] == 1) {
		if (rpmsg_sendto(apu_ipi_ut_rpm_dev.ept, NULL, 1, 0)) {
			dev_info(dev, "%s: rpmsg_sendto (power on) fail\n", __func__);
			return -EBUSY;
		}
		msleep(20);
	}

	buff_va = dma_alloc_coherent(dev,
		APU_DMA_UT_SIZE, &buff_iova, GFP_KERNEL);

	if (buff_va == NULL || buff_iova == 0) {
		dev_info(dev, "%s: dma_alloc_coherent fail\n", __func__);
		return -ENOMEM;
	}

	memset(buff_va, 0, APU_DMA_UT_SIZE);

	d.cmd_id = cmd;
	d.data[0] = cmd;
	d.data[1] = (uint32_t)buff_iova;
	d.data[2] = APU_DMA_UT_SIZE;

	apu_ipi_ut_send(&d, true);

	data = (uint8_t *)buff_va;
	for (i = 0; i < APU_DMA_UT_SIZE; i++) {
		if (data[i] != 0xA5) {
			dev_info(dev, "%s: compair fail at index %d\n", __func__, i);
			apusys_rv_aee_warn("APUSYS_RV", "APUSYS_RV_DMA_UT_CHECK_FAIL");
			break;
		}
	}

	dma_free_coherent(dev, APU_DMA_UT_SIZE, buff_va, buff_iova);

	if (args[0] == 1) {
		if (rpmsg_sendto(apu_ipi_ut_rpm_dev.ept, NULL, 0, 1))
			dev_info(dev, "%s: rpmsg_sendto (power off) fail\n", __func__);
	}

	return 0;
}

static int apu_ipi_ut_rpmsg_probe(struct rpmsg_device *rpdev)
{
	pr_info("%s: name=%s, src=%d\n",
			__func__, rpdev->id.name, rpdev->src);

	apu_ipi_ut_rpm_dev.ept = rpdev->ept;
	apu_ipi_ut_rpm_dev.rpdev = rpdev;

	pr_info("%s: rpdev->ept = %p\n", __func__, rpdev->ept);

	return 0;
}

static int apu_ipi_ut_rpmsg_cb(struct rpmsg_device *rpdev, void *data,
		int len, void *priv, u32 src)
{
	int ret;
	struct apu_ipi_ut_ipi_data *d = data;
	uint32_t rand_num = 0;
	uint32_t val;
	struct mtk_apu *apu = g_apu;
	struct mtk_apu_hw_ops *hw_ops;
	struct timespec64 ts, te;
	bool polling_mode = false;

	if (!apu) {
		pr_info("%s: apu == NULL\n", __func__);
		return -1;
	}
	hw_ops = &apu->platdata->ops;

	if (d->cmd_id == CMD_PWR_TIME_PROFILE_INTERNAL &&
		hw_ops->polling_rpc_status && d->data[0] == 1)
		polling_mode = true;

	if (d->cmd_id == CMD_UT_RANDOM) {
		get_random_bytes(&rand_num, sizeof(rand_num));
		rand_num = rand_num % 100;
		/* usleep_range(0, rand_num); */
		msleep(rand_num);
	}

	val = d->data[0];
	pr_info("%s: cmd_id = %u, val = %d, rand_num = %u\n",
		__func__, d->cmd_id, val, rand_num);
	if (d->cmd_id != CMD_UT_RANDOM)
		complete(&apu_ipi_ut_rpm_dev.ack);

	if (d->cmd_id == CMD_GET_PWR_ON_OFF_TIME ||
		d->cmd_id == CMD_PWR_TIME_PROFILE_INTERNAL) {
		rcx_on_ce_ts_last = d->data[1];
		rcx_off_ce_ts_last = d->data[2];
		rcx_on_ce_ts_avg = d->data[3];
		rcx_off_ce_ts_avg = d->data[4];
		rcx_on_ce_ts_max = d->data[5];
		rcx_off_ce_ts_max = d->data[6];
		warmboot_on_ts_last = d->data[7];
		dpidle_off_ts_last = d->data[8];
		smmu_hw_sem_ts_last = d->data[9];
		dvfs_skip_ts_last = d->data[10];
		warmboot_on_ts_avg = d->data[11];
		dpidle_off_ts_avg = d->data[12];
		smmu_hw_sem_ts_avg = d->data[13];
		dvfs_skip_ts_avg = d->data[14];
		warmboot_on_ts_max = d->data[15];
		dpidle_off_ts_max = d->data[16];
		smmu_hw_sem_ts_max = d->data[17];
		dvfs_skip_ts_max = d->data[18];
		smmu_hw_sem_ts_larger_1ms_cnt = d->data[19];
		boot_count = d->data[20];
	} else if (d->cmd_id == CMD_S5_IDLE_PROFILE) {
		s5_idle_func_trigger_cnt = d->data[1] ;
		s5_idle_dpidle_skip_cnt = d->data[2] ;
		s5_idle_ipi_skip_cnt = d->data[3] ;
		s5_idle_wakeup_cnt = d->data[4] ;
		s5_idle_overall_ts.last = d->data[5] ;
		s5_idle_overall_ts.avg = d->data[6] ;
		s5_idle_overall_ts.max = d->data[7] ;
		s5_idle_wfi_ts.last = d->data[8] ;
		s5_idle_wfi_ts.avg = d->data[9] ;
		s5_idle_wfi_ts.max = d->data[10];
		s5_idle_enter_ts.last = d->data[11];
		s5_idle_enter_ts.avg = d->data[12];
		s5_idle_enter_ts.max = d->data[13];
		s5_idle_leave_ts.last = d->data[14];
		s5_idle_leave_ts.avg = d->data[15];
		s5_idle_leave_ts.max = d->data[16];
		s5_idle_sw_enter_ts.last = d->data[17];
		s5_idle_sw_enter_ts.avg = d->data[18];
		s5_idle_sw_enter_ts.max = d->data[19];
		s5_idle_sw_leave_ts.last = d->data[20];
		s5_idle_sw_leave_ts.avg = d->data[21];
		s5_idle_sw_leave_ts.max = d->data[22];
		s5_idle_rcx_on_ce_ts.last = d->data[23];
		s5_idle_rcx_on_ce_ts.avg = d->data[24];
		s5_idle_rcx_on_ce_ts.max = d->data[25];
		s5_idle_rcx_off_ce_ts.last = d->data[26];
		s5_idle_rcx_off_ce_ts.avg = d->data[27];
		s5_idle_rcx_off_ce_ts.max = d->data[28];
	}

	if (polling_mode)
		ktime_get_ts64(&ts);

	/* power off */
	ret = rpmsg_sendto(apu_ipi_ut_rpm_dev.ept, NULL, 0, 1);
	if (ret && ret != -EOPNOTSUPP)
		pr_info("%s: rpmsg_sendto(power off) fail(%d)\n", __func__, ret);

	if (polling_mode) {
		ret = hw_ops->polling_rpc_status(apu, 0, 1000000);
		if (ret) {
			pr_info("%s: polling rpc timeout(%d)\n", __func__, ret);
		} else {
			ktime_get_ts64(&te);
			ts = timespec64_sub(te, ts);
			pr_info("%s: diff = %llu ns\n", __func__, timespec64_to_ns(&ts));
		}
	}

	return 0;
}

static void apu_ipi_ut_rpmsg_remove(struct rpmsg_device *rpdev)
{
}

static const struct of_device_id apu_ipi_ut_rpmsg_of_match[] = {
	{ .compatible = "mediatek,apu-ipi-ut-rpmsg", },
	{ },
};

static struct rpmsg_driver apu_ipi_ut_rpmsg_driver = {
	.drv = {
		.name = "apu-ipi-ut-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = apu_ipi_ut_rpmsg_of_match,
	},
	.probe = apu_ipi_ut_rpmsg_probe,
	.callback = apu_ipi_ut_rpmsg_cb,
	.remove = apu_ipi_ut_rpmsg_remove,
};

static int apu_ipi_ut_val, apu_ipi_ut_cmd;

static int apu_ipi_dbg_show(struct seq_file *s, void *unused)
{
	struct mtk_apu *apu = g_apu;

	seq_printf(s, "apu_ipi_ut_cmd = %d, apu_ipi_ut_val = %d\n", apu_ipi_ut_cmd, apu_ipi_ut_val);

	if (apu_ipi_ut_cmd == CMD_GET_PWR_ON_OFF_TIME) {
		seq_printf(s, "rcx_on_avg_time = %u us\n",
			warmboot_on_ts_avg + rcx_on_ce_ts_avg);
		seq_printf(s, "rcx_off_avg_time = %u us\n",
			dpidle_off_ts_avg + rcx_off_ce_ts_avg);
		seq_printf(s, "boot_count = %u\n", boot_count);
	} else if (apu_ipi_ut_cmd == CMD_PWR_TIME_PROFILE_INTERNAL) {
		seq_printf(s, "pwr_profile_polling_mode = %u\n", apu->pwr_profile_polling_mode);
		seq_printf(s, "pwr_on_polling_dbg_mode = %u\n", apu->pwr_on_polling_dbg_mode);
		seq_printf(s, "ce_dbg_polling_dump_mode = %u\n", apu->ce_dbg_polling_dump_mode);
		seq_printf(s, "apusys_rv_trace_on = %u\n", apu->apusys_rv_trace_on);

		seq_printf(s, "rcx_on_avg_time = %u us\n",
			warmboot_on_ts_avg + rcx_on_ce_ts_avg);
		seq_printf(s, "rcx_off_avg_time = %u us\n",
			dpidle_off_ts_avg + rcx_off_ce_ts_avg);
		seq_printf(s, "boot_count = %u\n", boot_count);
		seq_printf(s, "rcx_on_ce_ts_last = %u us\n", rcx_on_ce_ts_last);
		seq_printf(s, "rcx_off_ce_ts_last = %u us\n", rcx_off_ce_ts_last);
		seq_printf(s, "rcx_on_ce_ts_avg = %u us\n", rcx_on_ce_ts_avg);
		seq_printf(s, "rcx_off_ce_ts_avg = %u us\n", rcx_off_ce_ts_avg);
		seq_printf(s, "rcx_on_ce_ts_max = %u us\n", rcx_on_ce_ts_max);
		seq_printf(s, "rcx_off_ce_ts_max = %u us\n", rcx_off_ce_ts_max);

		seq_puts(s, "--------------------------------\n");

		seq_printf(s, "warmboot_on_ts_last = %u us\n", warmboot_on_ts_last);
		seq_printf(s, "dpidle_off_ts_last = %u us\n", dpidle_off_ts_last);
		seq_printf(s, "smmu_hw_sem_ts_last = %u us\n", smmu_hw_sem_ts_last);
		seq_printf(s, "dvfs_skip_ts_last = %u us\n", dvfs_skip_ts_last);
		seq_printf(s, "warmboot_on_ts_avg = %u us\n", warmboot_on_ts_avg);
		seq_printf(s, "dpidle_off_ts_avg = %u us\n", dpidle_off_ts_avg);
		seq_printf(s, "smmu_hw_sem_ts_avg = %u us\n", smmu_hw_sem_ts_avg);
		seq_printf(s, "dvfs_skip_ts_avg = %u us\n", dvfs_skip_ts_avg);
		seq_printf(s, "warmboot_on_ts_max = %u us\n", warmboot_on_ts_max);
		seq_printf(s, "dpidle_off_ts_max = %u us\n", dpidle_off_ts_max);
		seq_printf(s, "smmu_hw_sem_ts_max = %u us\n", smmu_hw_sem_ts_max);
		seq_printf(s, "dvfs_skip_ts_max = %u us\n", dvfs_skip_ts_max);
		seq_printf(s, "smmu_hw_sem_ts_larger_1ms_cnt = %u\n", smmu_hw_sem_ts_larger_1ms_cnt);

		seq_puts(s, "--------------------------------\n");

		seq_printf(s, "apu_on_ts_last = %llu us\n", apu_on_ts_last);
		seq_printf(s, "apu_off_ts_last = %llu us\n", apu_off_ts_last);
		seq_printf(s, "apu_on_ts_avg = %llu us\n", apu_on_ts_avg);
		seq_printf(s, "apu_off_ts_avg = %llu us\n", apu_off_ts_avg);
		seq_printf(s, "apu_on_ts_max = %llu us\n", apu_on_ts_max);
		seq_printf(s, "apu_off_ts_max = %llu us\n", apu_off_ts_max);
		seq_printf(s, "smc_on_ts_last = %llu us\n", smc_on_ts_last);
		seq_printf(s, "smc_off_ts_last = %llu us\n", smc_off_ts_last);
		seq_printf(s, "smc_on_ts_avg = %llu us\n", smc_on_ts_avg);
		seq_printf(s, "smc_off_ts_avg = %llu us\n", smc_off_ts_avg);
		seq_printf(s, "smc_on_ts_max = %llu us\n", smc_on_ts_max);
		seq_printf(s, "smc_off_ts_max = %llu us\n", smc_off_ts_max);
		seq_printf(s, "apu_on_over_1ms_cnt = %llu\n", apu_on_over_1ms_cnt);
		seq_printf(s, "apu_off_over_1ms_cnt = %llu\n", apu_off_over_1ms_cnt);
		seq_printf(s, "apu_on_ts_cnt = %llu\n", apu_on_ts_cnt);
		seq_printf(s, "apu_off_ts_cnt = %llu\n", apu_off_ts_cnt);

	} else if (apu_ipi_ut_cmd == CMD_S5_IDLE_PROFILE) {
		seq_printf(s, "s5_idle_func_trigger_cnt = %u\n", s5_idle_func_trigger_cnt);
		seq_printf(s, "s5_idle_dpidle_skip_cnt = %u\n", s5_idle_dpidle_skip_cnt);
		seq_printf(s, "s5_idle_ipi_skip_cnt = %u\n", s5_idle_ipi_skip_cnt);
		seq_printf(s, "s5_idle_wakeup_cnt = %u\n", s5_idle_wakeup_cnt);
		seq_printf(s, "s5_idle_overall_ts.last = %u us\n", s5_idle_overall_ts.last);
		seq_printf(s, "s5_idle_overall_ts.avg = %u us\n", s5_idle_overall_ts.avg);
		seq_printf(s, "s5_idle_overall_ts.max = %u us\n", s5_idle_overall_ts.max);
		seq_printf(s, "s5_idle_wfi_ts.last = %u us\n", s5_idle_wfi_ts.last);
		seq_printf(s, "s5_idle_wfi_ts.avg = %u us\n", s5_idle_wfi_ts.avg);
		seq_printf(s, "s5_idle_wfi_ts.max = %u us\n", s5_idle_wfi_ts.max);
		seq_printf(s, "s5_idle_enter_ts.last = %u us\n", s5_idle_enter_ts.last);
		seq_printf(s, "s5_idle_enter_ts.avg = %u us\n", s5_idle_enter_ts.avg);
		seq_printf(s, "s5_idle_enter_ts.max = %u us\n", s5_idle_enter_ts.max);
		seq_printf(s, "s5_idle_leave_ts.last = %u us\n", s5_idle_leave_ts.last);
		seq_printf(s, "s5_idle_leave_ts.avg = %u us\n", s5_idle_leave_ts.avg);
		seq_printf(s, "s5_idle_leave_ts.max = %u us\n", s5_idle_leave_ts.max);
		seq_printf(s, "s5_idle_sw_enter_ts.last = %u us\n", s5_idle_sw_enter_ts.last);
		seq_printf(s, "s5_idle_sw_enter_ts.avg = %u us\n", s5_idle_sw_enter_ts.avg);
		seq_printf(s, "s5_idle_sw_enter_ts.max = %u us\n", s5_idle_sw_enter_ts.max);
		seq_printf(s, "s5_idle_sw_leave_ts.last = %u us\n", s5_idle_sw_leave_ts.last);
		seq_printf(s, "s5_idle_sw_leave_ts.avg = %u us\n", s5_idle_sw_leave_ts.avg);
		seq_printf(s, "s5_idle_sw_leave_ts.max = %u us\n", s5_idle_sw_leave_ts.max);
		seq_printf(s, "s5_idle_rcx_on_ce_ts.last = %u us\n", s5_idle_rcx_on_ce_ts.last);
		seq_printf(s, "s5_idle_rcx_on_ce_ts.avg = %u us\n", s5_idle_rcx_on_ce_ts.avg);
		seq_printf(s, "s5_idle_rcx_on_ce_ts.max = %u us\n", s5_idle_rcx_on_ce_ts.max);
		seq_printf(s, "s5_idle_rcx_off_ce_ts.last = %u us\n", s5_idle_rcx_off_ce_ts.last);
		seq_printf(s, "s5_idle_rcx_off_ce_ts.avg = %u us\n", s5_idle_rcx_off_ce_ts.avg);
		seq_printf(s, "s5_idle_rcx_off_ce_ts.max = %u us\n", s5_idle_rcx_off_ce_ts.max);
	}

	return 0;
}

static int apu_ipi_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, apu_ipi_dbg_show, inode->i_private);
}

static int apu_ipi_dbg_exec_cmd(int cmd, unsigned int *args)
{
	struct apu_ipi_ut_ipi_data d;
	bool wait_ack;

	d.cmd_id = cmd;
	apu_ipi_ut_val = args[0];
	wait_ack = false;

	switch (cmd) {
	case CMD_UT:
		wait_ack = true;
		d.data[0] = args[0];
		break;
	case CMD_UT_RANDOM:
	case CMD_GET_PWR_ON_OFF_TIME:
	case CMD_PWR_TIME_PROFILE_INTERNAL:
		d.data[0] = args[0];
		break;
	case CMD_UT_IPI_OUTBOX_FUZZ_WRITE:
	case CMD_S5_IDEL_DBG:
	case CMD_DEEP_IDEL_DBG:
	case CMD_PANIC:
	case CMD_S5_IDLE_PROFILE:
		d.data[0] = args[0];
		d.data[1] = args[1];
		d.data[2] = args[2];
		break;
	case CMD_DMA_UT:
		return apu_dma_ut(cmd, args);
	default:
		pr_info("%s: unknown cmd %d\n", __func__, cmd);
		return -EINVAL;
	}

	return apu_ipi_ut_send(&d, wait_ack);
}

#define IPI_DBG_MAX_ARGS	(3)
static ssize_t apu_ipi_dbg_write(struct file *flip, const char __user *buffer,
				 size_t count, loff_t *f_pos)
{
	struct mtk_apu *apu = g_apu;
	char *tmp, *ptr, *token;
	unsigned int args[IPI_DBG_MAX_ARGS];
	int cmd = 0;
	int ret, i;
	bool change_pwr_profile_polling_mode = false;
	bool change_pwr_on_polling_dbg_mode = false;
	bool ce_dbg_polling_dump_mode = false;
	bool change_apusys_rv_trace_on = false;

	/* to prevent integer overflow leading to undefined behavior */
	if (count == UINT_MAX) {
		pr_info("%s: invalid count = %lu\n", __func__, count);
		return -EINVAL;
	}

	tmp = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);
	if (ret) {
		pr_info("%s: failed to copy user data, ret=%d\n",
			__func__, ret);
		goto out;
	}

	tmp[count] = '\0';
	ptr = tmp;

	token = strsep(&ptr, " ");
	if (strcmp(token, "ut") == 0) {
		cmd = CMD_UT;
	} else if (strcmp(token, "ut_rand") == 0) {
		cmd = CMD_UT_RANDOM;
	} else if (strcmp(token, "get_pwr_time") == 0) {
		cmd = CMD_GET_PWR_ON_OFF_TIME;
	} else if (strcmp(token, "pwr_time_internal") == 0) {
		cmd = CMD_PWR_TIME_PROFILE_INTERNAL;
	} else if (strcmp(token, "pwr_profile_polling_mode") == 0) {
		change_pwr_profile_polling_mode = true;
	} else if (strcmp(token, "pwr_on_polling_dbg_mode") == 0) {
		change_pwr_on_polling_dbg_mode = true;
	} else if (strcmp(token, "ce_dbg_polling_dump_mode") == 0) {
		ce_dbg_polling_dump_mode = true;
	} else if (strcmp(token, "apusys_rv_trace_on") == 0) {
		change_apusys_rv_trace_on = true;
	} else if (strcmp(token, "ut_ipi_outbox_check") == 0) {
		cmd = CMD_UT_IPI_OUTBOX_FUZZ_WRITE;
	} else if (strcmp(token, "s5_idle_dbg") == 0) {
		cmd = CMD_S5_IDEL_DBG;
	} else if (strcmp(token, "deep_idle_dbg") == 0) {
		cmd = CMD_DEEP_IDEL_DBG;
	} else if (strcmp(token, "panic") == 0) {
		cmd = CMD_PANIC;
	} else if (strcmp(token, "s5_idle_profile") == 0) {
		cmd = CMD_S5_IDLE_PROFILE;
	} else if (strcmp(token, "dma_ut") == 0) {
		cmd = CMD_DMA_UT;
	} else {
		ret = -EINVAL;
		pr_info("%s: unknown ipi dbg cmd: %s\n", __func__, token);
		goto out;
	}
	apu_ipi_ut_cmd = cmd;

	for (i = 0; i < IPI_DBG_MAX_ARGS && (token = strsep(&ptr, " ")); i++) {
		ret = kstrtoint(token, 10, &args[i]);
		if (ret) {
			pr_info("%s: invalid parameter i=%d, p=%s\n",
				__func__, i, token);
			goto out;
		}
	}

	if (change_pwr_profile_polling_mode) {
		apu->pwr_profile_polling_mode = args[0];
		ret = count;
		goto out;
	} else if (change_pwr_on_polling_dbg_mode) {
		apu->pwr_on_polling_dbg_mode = args[0];
		ret = count;
		goto out;
	} else if (ce_dbg_polling_dump_mode) {
		apu->ce_dbg_polling_dump_mode = args[0];
		ret = count;
		goto out;
	} else if (change_apusys_rv_trace_on) {
		apu->apusys_rv_trace_on = args[0];
		ret = count;
		goto out;
	}

	ret = apu_ipi_dbg_exec_cmd(cmd, args);
	if (!ret)
		ret = count;

out:
	kfree(tmp);

	pr_info("%s: ret = %d\n", __func__, ret);

	return ret;
}


#ifdef APU_IPI_USE_PROCFS
#define APU_IPI_DNAME	"apu_ipi"
#define APU_IPI_FNAME	"ipi_dbg"
static struct proc_dir_entry *ipi_dbg_root, *ipi_dbg_file;

static const struct proc_ops apu_ipi_dbg_fops = {
	.proc_open = apu_ipi_dbg_open,
	.proc_read = seq_read,
	.proc_write = apu_ipi_dbg_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int apu_ipi_dbg_init(void)
{
	ipi_dbg_root = proc_mkdir(APU_IPI_DNAME, NULL);
	if (IS_ERR_OR_NULL(ipi_dbg_root)) {
		pr_info("%s: failed to create debug dir %s\n",
			__func__, APU_IPI_DNAME);
		return -EINVAL;
	}

	ipi_dbg_file = proc_create(APU_IPI_FNAME, (0644), ipi_dbg_root,
					   &apu_ipi_dbg_fops);
	if (IS_ERR_OR_NULL(ipi_dbg_file)) {
		pr_info("%s: failed to create debug file %s\n",
			__func__, APU_IPI_FNAME);
		remove_proc_entry(APU_IPI_DNAME, NULL);
		return -EINVAL;
	}

	return 0;
}

static void apu_ipi_dbg_exit(void)
{
	remove_proc_entry(APU_IPI_FNAME, ipi_dbg_root);
	remove_proc_entry(APU_IPI_DNAME, NULL);
}
#else
#define APU_IPI_DNAME	"apu_ipi"
#define APU_IPI_FNAME	"ipi_dbg"
static struct dentry *ipi_dbg_root, *ipi_dbg_file;
static const struct file_operations apu_ipi_dbg_fops = {
	.open = apu_ipi_dbg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = apu_ipi_dbg_write,
};

static int apu_ipi_dbg_init(void)
{
	ipi_dbg_root = debugfs_create_dir(APU_IPI_DNAME, NULL);
	if (IS_ERR_OR_NULL(ipi_dbg_root)) {
		pr_info("%s: failed to create debug dir %s\n",
			__func__, APU_IPI_DNAME);
		return -EINVAL;
	}

	ipi_dbg_file = debugfs_create_file(APU_IPI_FNAME, (0644), ipi_dbg_root,
					   NULL, &apu_ipi_dbg_fops);
	if (IS_ERR_OR_NULL(ipi_dbg_file)) {
		pr_info("%s: failed to create debug file %s\n",
			__func__, APU_IPI_FNAME);
		debugfs_remove_recursive(ipi_dbg_root);
		return -EINVAL;
	}

	return 0;
}

static void apu_ipi_dbg_exit(void)
{
	debugfs_remove(ipi_dbg_file);
	debugfs_remove_recursive(ipi_dbg_root);
}
#endif /* #ifdef APU_IPI_USE_PROCFS */

int apu_ipi_ut_init(struct mtk_apu *apu)
{
	int ret;

	if (g_apu == NULL)
		g_apu = apu;

	init_completion(&apu_ipi_ut_rpm_dev.ack);
	mutex_init(&apu_ipi_ut_mtx);

	dev_info(apu->dev, "%s: register rpmsg...\n", __func__);
	ret = register_rpmsg_driver(&apu_ipi_ut_rpmsg_driver);
	if (ret) {
		dev_info(apu->dev, "failed to register apu ipi ut rpmsg driver\n");
		return ret;
	}

	apu_ipi_dbg_init();

	return 0;
}

void apu_ipi_ut_exit(void)
{
	apu_ipi_dbg_exit();
	unregister_rpmsg_driver(&apu_ipi_ut_rpmsg_driver);
}

#else
int apu_ipi_ut_init(struct mtk_apu *apu) { return 0; }
void apu_ipi_ut_exit(void) {}
static void apu_power_on_off_profile(u32 on, u32 off,
	uint64_t time_diff_ns, uint64_t time_diff_ns2)
{}
#endif /* IS_ENABLED(CONFIG_DEBUG_FS) */

static int rv_bsp_rx_rpmsg_probe(struct rpmsg_device *rpdev)
{
	pr_info("%s: name=%s, src=%d\n",
			__func__, rpdev->id.name, rpdev->src);

	rv_bsp_rx_rpm_dev.ept = rpdev->ept;
	rv_bsp_rx_rpm_dev.rpdev = rpdev;

	pr_info("%s: rpdev->ept = %p\n", __func__, rpdev->ept);

	return 0;
}

static int rv_bsp_rx_rpmsg_cb(struct rpmsg_device *rpdev, void *data,
		int len, void *priv, u32 src)
{
	int ret;
	struct rv_bsp_rx_ipi_data *input_data = (struct rv_bsp_rx_ipi_data *) data;
	struct rv_bsp_rx_ipi_data output_data;

	pr_info("%s: cmd_id = %d, reason = %d\n", __func__, input_data->cmd_id, input_data->reason);

	ret = rpmsg_send(rv_bsp_rx_rpm_dev.ept, &output_data, sizeof(output_data));

	if (ret)
		pr_info("%s: rpmsg_send fail(%d)\n", __func__, ret);

	return 0;
}

static void rv_bsp_rx_rpmsg_remove(struct rpmsg_device *rpdev)
{
}

static const struct of_device_id rv_bsp_rx_rpmsg_of_match[] = {
	{ .compatible = "mediatek,rv-bsp-rx-rpmsg", },
	{ },
};

static struct rpmsg_driver rv_bsp_rx_rpmsg_driver = {
	.drv = {
		.name = "rv-bsp-rx-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = rv_bsp_rx_rpmsg_of_match,
	},
	.probe = rv_bsp_rx_rpmsg_probe,
	.callback = rv_bsp_rx_rpmsg_cb,
	.remove = rv_bsp_rx_rpmsg_remove,
};

int rv_bsp_rx_init(struct mtk_apu *apu)
{
	int ret;

	if (g_apu == NULL)
		g_apu = apu;

	dev_info(apu->dev, "%s: register rpmsg...\n", __func__);
	ret = register_rpmsg_driver(&rv_bsp_rx_rpmsg_driver);
	if (ret) {
		dev_info(apu->dev, "failed to register rv_bsp_rx_rpmsg driver\n");
		return ret;
	}

	return 0;
}

void rv_bsp_rx_exit(void)
{
	unregister_rpmsg_driver(&rv_bsp_rx_rpmsg_driver);
}

