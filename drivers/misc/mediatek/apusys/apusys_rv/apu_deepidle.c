// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/completion.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/sched/clock.h>
#include <linux/spinlock.h>
#include <linux/time64.h>

#include <linux/delay.h>

#include <mt-plat/aee.h>

#include "apu.h"
#include "apu_config.h"
#include "mtk_apu_rpmsg.h"
#include "apu_excep.h"

#include "apu_hw.h"
#include "hw_logger.h"
#include "apu_regdump.h"

/* cmd */
enum {
	DPIDLE_CMD_LOCK_IPI = 0x5a00,
	DPIDLE_CMD_UNLOCK_IPI = 0x5a01,
	DPIDLE_CMD_PDN_UNLOCK = 0x5a02,
};

/* ack */
enum {
	DPIDLE_ACK_OK = 0,
	DPIDLE_ACK_LOCK_BUSY,
	DPIDLE_ACK_POWER_DOWN_FAIL,
};

struct dpidle_msg {
	uint32_t cmd;
	uint32_t ack;
};

/* #define APU_DEEPIDLE_WQ */
#define BOOT_WARN_LOG_TIME_US 3000

static struct mtk_apu *g_apu;
static struct work_struct pwron_dbg_wk;
#ifdef APU_DEEPIDLE_WQ
static struct workqueue_struct *apu_deepidle_workq;
#endif
static struct dpidle_msg recv_msg;

static void apu_deepidle_pwron_dbg_fn(struct work_struct *work)
{
	struct mtk_apu *apu = g_apu;
	struct device *dev = apu->dev;
	int i;

	dev_info(dev, "mbox dummy= 0x%08x 0x%08x 0x%08x 0x%08x\n",
		ioread32(apu->apu_mbox + 0x40),
		ioread32(apu->apu_mbox + 0x44),
		ioread32(apu->apu_mbox + 0x48),
		ioread32(apu->apu_mbox + 0x4c));

	usleep_range(0, 1000);
	for (i = 0; i < 20; i++) {
		dev_info(apu->dev, "apu boot: pc=%08x, sp=%08x\n",
		ioread32(apu->md32_sysctrl + 0x838),
				ioread32(apu->md32_sysctrl+0x840));
		usleep_range(0, 1000);
	}

	dev_info(dev, "%s: UP_NORMAL_DOMAIN_NS = 0x%x\n",
		__func__,
		ioread32(apu->apu_sctrl_reviser + UP_NORMAL_DOMAIN_NS));
	dev_info(dev, "%s: UP_PRI_DOMAIN_NS = 0x%x\n",
		__func__,
		ioread32(apu->apu_sctrl_reviser + UP_PRI_DOMAIN_NS));
	dev_info(dev, "%s: USERFW_CTXT = 0x%x\n",
		__func__,
		ioread32(apu->apu_sctrl_reviser + USERFW_CTXT));
	dev_info(dev, "%s: SECUREFW_CTXT = 0x%x\n",
		__func__,
		ioread32(apu->apu_sctrl_reviser + SECUREFW_CTXT));

	dev_info(dev, "%s: MD32_SYS_CTRL = 0x%x\n",
		__func__, ioread32(apu->md32_sysctrl + MD32_SYS_CTRL));

	dev_info(dev, "%s: MD32_CLK_EN = 0x%x\n",
		__func__, ioread32(apu->md32_sysctrl + MD32_CLK_EN));
	dev_info(dev, "%s: UP_WAKE_HOST_MASK0 = 0x%x\n",
		__func__, ioread32(apu->md32_sysctrl + UP_WAKE_HOST_MASK0));

	dev_info(dev, "%s: MD32_BOOT_CTRL = 0x%x\n",
		__func__, ioread32(apu->apu_ao_ctl + MD32_BOOT_CTRL));

	dev_info(dev, "%s: MD32_PRE_DEFINE = 0x%x\n",
		__func__, ioread32(apu->apu_ao_ctl + MD32_PRE_DEFINE));
}

int apu_deepidle_power_on_aputop(struct mtk_apu *apu)
{
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;
	struct device *dev = apu->dev;
	struct timespec64 begin, end, delta;
	uint32_t wait_ms = 10000;
	int retry = 0;
	int ret;
	u64 t, timertick;
	unsigned long flags;

	if (pm_runtime_suspended(apu->dev)) {

		init_waitqueue_head(&apu->run.wq);
		apu->run.signaled = 0;

		if (!(apu->platdata->flags & F_SECURE_BOOT))
			dev_info(apu->dev,
				 "%s: before warm boot pc=%08x, sp=%08x\n",
				 __func__,
				 ioread32(apu->md32_sysctrl + 0x838),
				 ioread32(apu->md32_sysctrl + 0x840));

		spin_lock_irqsave(&apu->reg_lock, flags);
		apu->conf_buf->time_offset = sched_clock();
		timertick = arch_timer_read_counter();
		spin_unlock_irqrestore(&apu->reg_lock, flags);

		/* Calculate time diff for warm boot */
		apu->conf_buf->time_diff =
			(timertick * 1000 / 13) - apu->conf_buf->time_offset;
		apu->conf_buf->time_diff_cycle =
			timertick - (apu->conf_buf->time_offset * 13 / 1000);

		ret = hw_ops->power_on(apu);

		if (ret == 0)
			hw_logger_deep_idle_leave();
		else
			return ret;

		if (!(apu->platdata->flags & F_SECURE_BOOT))
			schedule_work(&pwron_dbg_wk);

		ktime_get_ts64(&begin);
wait_for_warm_boot:
		/* wait for remote warm boot done */
		ret = wait_event_interruptible_timeout(apu->run.wq,
						       apu->run.signaled,
						       msecs_to_jiffies(wait_ms));
		if (ret == -ERESTARTSYS) {
			ktime_get_ts64(&end);
			delta = timespec64_sub(end, begin);
			if (delta.tv_sec > (wait_ms/1000)) {
				dev_info(dev, "%s: retry(%d) over %u seconds!\n",
					__func__, retry, (wait_ms/1000));
				dev_info(dev, "APU warm boot timeout!!\n");
				apu_regdump();
				/*
				 * since exception is triggered
				 * so bypass power off timeout check
				 */
				apu->bypass_pwr_off_chk = true;
				apusys_rv_aee_warn("APUSYS_RV",
					"APUSYS_RV_TIMEOUT");
				return -1;
			}
			if (retry % 50 == 0)
				dev_info(dev,
					"%s: wait APU interrupted by a signal, retry again\n",
					__func__);
			retry++;
			msleep(20);
			goto wait_for_warm_boot;
		}

		if (ret == 0) {
			dev_info(dev, "APU warm boot timeout!!\n");
			apu_regdump();
			apu->bypass_pwr_off_chk = true;
			apusys_rv_aee_warn("APUSYS_RV",
				"APUSYS_RV_TIMEOUT");
			return -1;
		}

		ktime_get_ts64(&end);
		delta = timespec64_sub(end, begin);
		t = timespec64_to_ns(&delta);
		if (t > BOOT_WARN_LOG_TIME_US * 1000)
			dev_info(dev, "%s: warm boot done (%lldns)\n",
				__func__, t);
		else
			apu_info_ratelimited(dev,
				"%s: warm boot done\n", __func__);
	}

	return 0;
}

static int apu_deepidle_send_ack(struct mtk_apu *apu, uint32_t cmd, uint32_t ack)
{
	struct dpidle_msg msg;
	int ret;

	msg.cmd = cmd;
	msg.ack = ack;

	ret = apu_ipi_send(apu, APU_IPI_DEEP_IDLE, &msg, sizeof(msg), 0);
	if (ret)
		dev_info(apu->dev,
			 "%s: failed to send ack msg, ack=%d, ret=%d\n",
			 __func__, ack, ret);

	return ret;
}

static void __apu_deepidle(struct mtk_apu *apu)
{
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;
	struct dpidle_msg *msg = &recv_msg;
	int ret;

	switch (msg->cmd) {
	case DPIDLE_CMD_LOCK_IPI:
		ret = apu_ipi_lock(apu);
		if (ret) {
			/* remove to reduce log */
			/*
			 * dev_info(apu->dev, "%s: IPI busy, ret=%d\n",
			 *	 __func__, ret);
			 */
			apu_deepidle_send_ack(apu, DPIDLE_CMD_LOCK_IPI,
					DPIDLE_ACK_LOCK_BUSY);
			return;
		}
		apu_deepidle_send_ack(apu, DPIDLE_CMD_LOCK_IPI,
					DPIDLE_ACK_OK);
		break;

	case DPIDLE_CMD_UNLOCK_IPI:
		apu_ipi_unlock(apu);
		apu_deepidle_send_ack(apu, DPIDLE_CMD_UNLOCK_IPI,
					DPIDLE_ACK_OK);
		break;

	case DPIDLE_CMD_PDN_UNLOCK:
		hw_logger_deep_idle_enter_pre();

		apu_deepidle_send_ack(apu, DPIDLE_CMD_PDN_UNLOCK,
					DPIDLE_ACK_OK);

		ret = hw_ops->power_off(apu);
		if (ret) {
			dev_info(apu->dev, "failed to power off ret=%d\n", ret);
			hw_logger_deep_idle_enter_post();
			apu_ipi_unlock(apu);
			WARN_ON(0);
			return;
		}

		hw_logger_deep_idle_enter_post();
		apu_ipi_unlock(apu);
		apu_info_ratelimited(apu->dev, "power off done\n");
		break;

	default:
		dev_info(apu->dev, "unknown cmd %x\n", msg->cmd);
		break;
	}
}

#ifdef APU_DEEPIDLE_WQ
static void apu_deepidle_work_func(struct work_struct *work)
{
	struct mtk_apu *apu = container_of(work,
		struct mtk_apu, deepidle_work);

	__apu_deepidle(apu);
}
#endif

static void apu_deepidle_ipi_handler(void *data, unsigned int len, void *priv)
{
	struct mtk_apu *apu = (struct mtk_apu *)priv;

	if (len > sizeof(recv_msg)) {
		dev_info(apu->dev, "%s: len(%u) > %lu\n", __func__, len, sizeof(recv_msg));
		return;
	}

	memcpy(&recv_msg, data, len);

#ifdef APU_DEEPIDLE_WQ
	queue_work(apu_deepidle_workq, &apu->deepidle_work);
#else
	__apu_deepidle(apu);
#endif
}

int apu_deepidle_init(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int ret;

#ifdef APU_DEEPIDLE_WQ
	apu_deepidle_workq = alloc_workqueue("apu_deepidle",
					     WQ_UNBOUND | WQ_HIGHPRI, 0);
	if (!apu_deepidle_workq) {
		dev_info(apu->dev, "%s: failed to allocate rq for deep idle\n",
			 __func__);
		return -ENOMEM;
	}

	INIT_WORK(&apu->deepidle_work, apu_deepidle_work_func);
#endif

	ret = apu_ipi_register(apu, APU_IPI_DEEP_IDLE, NULL,
			       apu_deepidle_ipi_handler, apu);
	if (ret) {
		dev_info(dev,
			 "%s: failed to register deepidle ipi, ret=%d\n",
			 __func__, ret);
	}

	g_apu = apu;
	INIT_WORK(&pwron_dbg_wk, apu_deepidle_pwron_dbg_fn);

	return ret;
}

void apu_deepidle_exit(struct mtk_apu *apu)
{
	/* module can be removed only after APU TOP is shutdown properly */
	flush_work(&pwron_dbg_wk);

	apu_ipi_unregister(apu, APU_IPI_DEEP_IDLE);

#ifdef APU_DEEPIDLE_WQ
	if (apu_deepidle_workq)
		destroy_workqueue(apu_deepidle_workq);
#endif
}

