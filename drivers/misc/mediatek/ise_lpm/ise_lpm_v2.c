// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#include <asm/compiler.h>
#include <linux/arm-smccc.h>
#include <linux/arm_ffa.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/soc/mediatek/mtk_ise.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>

#include <mtk_ise_lpm.h>

#define ISE_LPM_FREERUN_TIMEOUT_MS		7000
#define ISE_LPM_PWR_OFF_DEBOUNCE_MS		1000
#define ISE_LPM_DEINIT_TIMEOUT_MS		10000

struct ise_lpm_work_struct {
	struct work_struct work;
	unsigned int flags;
	unsigned int id;
};

enum ise_lpm_wq_cmd {
	ISE_LPM_FREERUN,
	ISE_PWR_OFF,
};

enum ise_pwr_ut_id_enum {
	ISE_AWAKE_LOCK = 1,
	ISE_AWAKE_UNLOCK,
	ISE_LOWPOWER_UT
};

enum ise_doorbell_cmd {
	ISE_DOORBELL_REQ,
	ISE_DOORBELL_RLS,
};

enum ise_doorbell_sta {
	DOORBELL_ACK_OFF        = 0x00000000,
	DOORBELL_REQ_ON		= 0x00000001,
	DOORBELL_ACK_ON		= 0x00010001,
	DOORBELL_REQ_OFF	= 0x00010000,
};


struct device *ise_lpm_dev;
struct mutex mutex_ise_lpm;
static struct timer_list ise_lpm_timer;
static struct timer_list ise_lpm_pd_timer;
static struct ise_lpm_work_struct ise_lpm_work;
static struct workqueue_struct *ise_lpm_wq;
static struct wakeup_source *ise_wakelock;

static uint32_t ise_state;
static uint32_t ise_awake_cnt;
static uint32_t ise_wakelock_en;
static uint32_t ise_lpm_freerun_en;
static uint32_t ise_lpm_fr_ms;
static uint32_t ise_req_pending_cnt;
static uint32_t ise_awake_user_list[ISE_AWAKE_ID_NUM];
static uint32_t ise_doorbell_pa, ise_doorbell_size;
static uint64_t ise_boot_cnt;
static void __iomem *ise_doorbell_va;

static uint32_t ise_power_on(void);
static uint32_t ise_power_off(void);

static void inc_ise_awake_cnt(enum mtk_ise_awake_id_t mtk_ise_awake_id)
{
	if (mtk_ise_awake_id >= ISE_AWAKE_ID_NUM) {
		pr_notice("err id %d\n", mtk_ise_awake_id);
		return;
	}
	ise_awake_user_list[mtk_ise_awake_id]++;
	ise_awake_cnt++;
}

static void dec_ise_awake_cnt(enum mtk_ise_awake_id_t mtk_ise_awake_id)
{
	if (mtk_ise_awake_id >= ISE_AWAKE_ID_NUM) {
		pr_notice("err id %d\n", mtk_ise_awake_id);
		return;
	}
	ise_awake_user_list[mtk_ise_awake_id]--;
	ise_awake_cnt--;
}

enum mtk_ise_awake_ack_t mtk_ise_awake_lock(enum mtk_ise_awake_id_t mtk_ise_awake_id)
{
	uint64_t start_time, end_time;
	uint32_t ret = ISE_SUCCESS;

	if (!ise_wakelock_en) {
		pr_notice("ise wakelock disable!!\n");
		return ISE_ERR_WAKELOCK_DISABLE;
	}
	if (mtk_ise_awake_id >= ISE_AWAKE_ID_NUM) {
		pr_notice("err id %d\n", mtk_ise_awake_id);
		return ISE_ERR_UID;
	}

	pr_notice("%s cnt%d user%d\n", __func__, ise_awake_cnt, mtk_ise_awake_id);

	mutex_lock(&mutex_ise_lpm);
	start_time = cpu_clock(0);
	inc_ise_awake_cnt(mtk_ise_awake_id);
	if (ise_awake_cnt == 1) {
		if (ise_req_pending_cnt == 1)
			ise_req_pending_cnt--;
		else
			ret = ise_power_on();
	}
	end_time = cpu_clock(0);
	mutex_unlock(&mutex_ise_lpm);

	pr_notice("%s cnt%d user%d start=%llu, end=%llu diff=%llu\n",
		__func__, ise_awake_cnt, mtk_ise_awake_id,
		start_time, end_time, end_time - start_time);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_ise_awake_lock);

enum mtk_ise_awake_ack_t mtk_ise_awake_unlock(enum mtk_ise_awake_id_t mtk_ise_awake_id)
{
	uint64_t start_time, end_time;
	uint32_t ret = ISE_SUCCESS;

	if (!ise_wakelock_en) {
		pr_notice("ise wakelock disable!!\n");
		return ISE_ERR_WAKELOCK_DISABLE;
	}
	if (mtk_ise_awake_id >= ISE_AWAKE_ID_NUM) {
		pr_notice("err id %d\n", mtk_ise_awake_id);
		return ISE_ERR_UID;
	}
	if (ise_awake_user_list[mtk_ise_awake_id] == 0) {
		pr_notice("unlock err id %d\n", mtk_ise_awake_id);
		return ISE_ERR_UNLOCK_BEFORE_LOCK;
	}
	if (ise_awake_cnt == 0) {
		pr_notice("ref cnt err id %d\n", mtk_ise_awake_id);
		return ISE_ERR_REF_CNT;
	}

	pr_notice("%s cnt%d user%d\n", __func__, ise_awake_cnt, mtk_ise_awake_id);

	mutex_lock(&mutex_ise_lpm);
	start_time = cpu_clock(0);
	dec_ise_awake_cnt(mtk_ise_awake_id);
	if (ise_awake_cnt == 0) {
		ise_req_pending_cnt++;
		mod_timer(&ise_lpm_pd_timer,
			jiffies + msecs_to_jiffies(ISE_LPM_PWR_OFF_DEBOUNCE_MS));
	}
	end_time = cpu_clock(0);
	pr_notice("%s cnt%d user%d start=%llu, end=%llu diff=%llu\n",
		__func__, ise_awake_cnt, mtk_ise_awake_id,
		start_time, end_time, end_time - start_time);
	mutex_unlock(&mutex_ise_lpm);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_ise_awake_unlock);

static uint32_t ise_doorbell_req(void)
{
	uint32_t ret = 0;
	uint32_t tmp = 0;
	uint32_t retry = 0, retry_limit = 20;

	tmp = readl(ise_doorbell_va);
	tmp |= 0x1;
	writel(tmp, ise_doorbell_va);
	do {
		tmp = readl(ise_doorbell_va);
		if (tmp == DOORBELL_ACK_ON){
			pr_info("power on done, retry=%d, cnt%llu\n", retry, ++ise_boot_cnt);
			break;
		}
		if(retry >= retry_limit) {
			ret = 1;
			pr_err("iSE power on failed\n");
			BUG_ON(1);
			break;
		}
		udelay(500);
	} while (++retry);

	return ret;
}

static uint32_t ise_doorbell_rls(void)
{
	uint32_t ret = 0;
	uint32_t tmp = 0;
	uint32_t retry = 0, retry_limit = 20;

	tmp = readl(ise_doorbell_va);
	tmp &= ~0x1;
	writel(tmp, ise_doorbell_va);
	do {
		tmp = readl(ise_doorbell_va);
		if (tmp == DOORBELL_ACK_OFF){
			pr_info("power off done, retry=%d\n", retry);
			break;
		}
		if(retry >= retry_limit) {
			ret = 1;
			pr_err("iSE power off failed\n");
			BUG_ON(1);
			break;
		}
		udelay(500);
	} while (++retry);

	return ret;
}

static uint32_t ise_doorbell_op(enum ise_doorbell_cmd cmd)
{
	uint32_t ret = 0;

	ise_state = readl(ise_doorbell_va);
	switch(ise_state) {
	case DOORBELL_ACK_OFF:
		if (cmd == ISE_DOORBELL_REQ)
			ret = ise_doorbell_req();
		else {
			pr_err("error cmd %d in state %d\n", cmd, ise_state);
			BUG_ON(1);
		}
		break;
	case DOORBELL_ACK_ON:
		if (cmd == ISE_DOORBELL_RLS)
			ret = ise_doorbell_rls();
		else {
			pr_err("error cmd %d in state %d\n", cmd, ise_state);
			BUG_ON(1);
		}
		break;
	default:
		pr_notice("%s abnormal state %d\n", __func__, ise_state);
		break;
	}

	return ret;
}

static uint32_t ise_power_on(void)
{
	uint32_t ret = 0;

	/* APMCU wakelock */
	__pm_stay_awake(ise_wakelock);

	/* Do DoorBell access */
	ret = ise_doorbell_op(ISE_DOORBELL_REQ);

	return ret;
}

static uint32_t ise_power_off(void)
{
	uint32_t ret = 0;
	/* Do DoorBell access*/
	ret = ise_doorbell_op(ISE_DOORBELL_RLS);

	/* reease APMCU wakelock */
	__pm_relax(ise_wakelock);

	return ret;
}

static void ise_lpm_schedule_work(struct ise_lpm_work_struct *ise_lpm_ws)
{
	queue_work(ise_lpm_wq, &ise_lpm_ws->work);
}

static void ise_lpm_send_wq(enum ise_lpm_wq_cmd cmd)
{
	ise_lpm_work.flags = (uint32_t) cmd;
	ise_lpm_schedule_work(&ise_lpm_work);
}

static void ise_lpm_timeout(struct timer_list *t)
{
	ise_lpm_send_wq(ISE_LPM_FREERUN);
	del_timer(&ise_lpm_timer);
}

static void ise_lpm_pwr_off_cb(struct timer_list *t)
{
	ise_lpm_send_wq(ISE_PWR_OFF);
}

void ise_lpm_work_handle(struct work_struct *ws)
{
	struct ise_lpm_work_struct *ise_lpm_ws
		= container_of(ws, struct ise_lpm_work_struct, work);
	uint32_t ise_lpm_cmd = ise_lpm_ws->flags;
	int ret;

	pr_notice("%s cmd=%d\n", __func__, ise_lpm_cmd);
	switch (ise_lpm_cmd) {
	case ISE_LPM_FREERUN:
		ret = mtk_ise_awake_unlock(ISE_PM_INIT);
		if (ret != ISE_SUCCESS) {
			pr_notice("%s err %d", __func__, ret);
			WARN_ON_ONCE(1);
		}
		break;
	case ISE_PWR_OFF:
		mutex_lock(&mutex_ise_lpm);
		if (ise_req_pending_cnt > 0) {
			ise_req_pending_cnt--;
			ise_power_off();
		} else
			pr_notice("%s drop pwr off %d", __func__, ise_req_pending_cnt);
		mutex_unlock(&mutex_ise_lpm);
		break;
	}
}

ssize_t ise_lpm_dbg(struct file *file, const char __user *buffer,
			size_t count, loff_t *data)
{
	char *parm_str, *cmd_str, *pinput;
	char input[32] = {0};
	long param;
	uint32_t len;
	int err;

	len = (count < (sizeof(input) - 1)) ? count : (sizeof(input) - 1);
	if (copy_from_user(input, buffer, len)) {
		pr_notice("%s: copy from user failed\n", __func__);
		return -EFAULT;
	}

	input[len] = '\0';
	pinput = input;

	cmd_str = strsep(&pinput, " ");

	if (!cmd_str)
		return -EINVAL;

	parm_str = strsep(&pinput, " ");

	if (!parm_str)
		return -EINVAL;

	err = kstrtol(parm_str, 10, &param);

	if (err)
		return err;

	if (!strncmp(cmd_str, "ise_pwr", sizeof("ise_pwr"))) {
		if (param == ISE_AWAKE_LOCK)
			mtk_ise_awake_lock(ISE_PM_INIT);
		else if (param == ISE_AWAKE_UNLOCK)
			mtk_ise_awake_unlock(ISE_PM_INIT);
		else if (param == ISE_LOWPOWER_UT) {
			mtk_ise_awake_lock(ISE_PM_UT);
			/* Do iSE jobs here */
			mtk_ise_awake_unlock(ISE_PM_UT);
		} else
			pr_notice("%s unknown pwr ut\n", __func__);
	} else {
		return -EINVAL;
	}

	return count;
}

static const struct proc_ops ise_lpm_dbg_fops = {
	.proc_write = ise_lpm_dbg,
};

static int ise_lpm_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;

	if (!node) {
		dev_info(&pdev->dev, "of_node required\n");
		return -EINVAL;
	}

	ise_boot_cnt = 0;
	ise_wakelock_en = 0;
	if (!of_property_read_u32(pdev->dev.of_node, "ise-wakelock", &ise_wakelock_en))
		pr_info("ise-wakelock %d\n", ise_wakelock_en);

	ise_lpm_freerun_en = 0;
	if (!of_property_read_u32(pdev->dev.of_node, "ise-lpm-freerun", &ise_lpm_freerun_en))
		pr_info("ise_lpm_freerun_en %d\n", ise_lpm_freerun_en);

	ise_lpm_fr_ms = 0;
	if (!of_property_read_u32(pdev->dev.of_node, "ise-lpm-fr-ms", &ise_lpm_fr_ms))
		pr_info("ise_lpm_fr_ms %d\n", ise_lpm_fr_ms);

	if (ise_lpm_fr_ms < ISE_LPM_FREERUN_TIMEOUT_MS)
		ise_lpm_fr_ms = ISE_LPM_FREERUN_TIMEOUT_MS;

	/* probe doorbell info */
	{
		if (of_property_read_u32_index(pdev->dev.of_node, "ise-doorbell", 0, &ise_doorbell_pa))
			pr_notice("ise-doorbell node not exist\n");
		if (of_property_read_u32_index(pdev->dev.of_node, "ise-doorbell", 1, &ise_doorbell_size))
			pr_notice("ise-doorbell node not exist\n");
		if ((!ise_doorbell_pa) | (!ise_doorbell_size))
			pr_notice("ise-doorbell reg error 0x%x 0x%x\n", ise_doorbell_pa, ise_doorbell_size);
		else {
			pr_notice("ise-doorbell addr/size 0x%x 0x%x\n", ise_doorbell_pa, ise_doorbell_size);
			ise_doorbell_va = ioremap(ise_doorbell_pa, ise_doorbell_size);
			if (IS_ERR(ise_doorbell_va)) {
				pr_notice("could not ioremap resource for ise doorbell:%ld\n",
														PTR_ERR(ise_doorbell_va));
				return -ENOMEM;
			}
		}
		pr_notice("ise doorbell probe done\n");
	}

	if (ise_wakelock_en) {
		mutex_init(&mutex_ise_lpm);
		mutex_lock(&mutex_ise_lpm);
		/*
		 * Since iSE already boot up at BL2 phase, so init following
		 * ise_awake_cnt = 1
		 * ise_awake_user_list[ISE_PM_INIT] = 1
		 */
		ise_awake_cnt = 1;
		ise_awake_user_list[ISE_PM_INIT] = 1;

		ise_req_pending_cnt = 0;
		timer_setup(&ise_lpm_pd_timer, ise_lpm_pwr_off_cb, 0);
		mutex_unlock(&mutex_ise_lpm);
		proc_create("ise_lpm_dbg", 0664, NULL, &ise_lpm_dbg_fops);

		/* System wakelock */
		ise_wakelock = wakeup_source_register(NULL, "ise_lpm wakelock");
	}

	if (ise_lpm_freerun_en) {
		timer_setup(&ise_lpm_timer, ise_lpm_timeout, 0);
		mod_timer(&ise_lpm_timer,
			jiffies + msecs_to_jiffies(ise_lpm_fr_ms));
	}

	ise_lpm_wq = create_singlethread_workqueue("ISE_LPM_WQ");
	INIT_WORK(&ise_lpm_work.work, ise_lpm_work_handle);

	return 0;
}

static int ise_lpm_remove(struct platform_device *pdev)
{
	return 0;
}

static void ise_lpm_shutdown(struct platform_device *pdev)
{
	/* stop service ise wakelock */
	ise_wakelock_en = 0;
	pr_info("%s ise-wakelock %d\n", __func__, ise_wakelock_en);
}

static const struct of_device_id ise_lpm_of_match[] = {
	{ .compatible = "mediatek,ise-lpm", },
	{},
};

static struct platform_driver ise_lpm_driver = {
	.probe = ise_lpm_probe,
	.remove = ise_lpm_remove,
	.shutdown = ise_lpm_shutdown,
	.driver	= {
		.name = "ise-lpm",
		.owner = THIS_MODULE,
		.of_match_table = ise_lpm_of_match,
	},
};

static int __init ise_lpm_driver_init(void)
{
	return platform_driver_register(&ise_lpm_driver);
}

static void __exit ise_lpm_driver_exit(void)
{
	platform_driver_unregister(&ise_lpm_driver);
}
device_initcall_sync(ise_lpm_driver_init);
module_exit(ise_lpm_driver_exit);

MODULE_DESCRIPTION("MEDIATEK Module ise_lpm_v2 driver");
MODULE_AUTHOR("Mediatek");
MODULE_LICENSE("GPL");
