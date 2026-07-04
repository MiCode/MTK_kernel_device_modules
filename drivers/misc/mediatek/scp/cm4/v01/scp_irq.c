// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include "scp_ipi.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_dvfs.h"
#include "scp_feature_define.h"
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif
#include "situation.h"
#include "sensor_event.h"

static struct situation_context *situation_context_obj;
struct wakeup_source *ws;

int sar_exception_data_report(void);
int sar_algo_exception_data_report(void);
int sar_algo_top_exception_data_report(void);

static struct situation_context *situ_context_alloc_object(void)
{
	struct situation_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	int index;

	pr_debug("[SCP] %s start\n", __func__);
	if (!obj) {
		pr_err("Alloc situ object error!\n");
		return NULL;
	}
	mutex_init(&obj->situation_op_mutex);
	for (index = inpocket; index < max_situation_support; ++index) {
		obj->ctl_context[index].power = 0;
		obj->ctl_context[index].enable = 0;
		obj->ctl_context[index].delay_ns = -1;
		obj->ctl_context[index].latency_ns = -1;
	}
	obj->mdev.minor = ID_WAKE_GESTURE;
	pr_debug("[SCP] %s end\n", __func__);
	return obj;
}

int situ_register_control(void)
{
	char wake_lock_name[20] = {0};
	char s[] = "scp_irq";
	sprintf(wake_lock_name, "sar_wakelock-%s", s);
	ws = wakeup_source_register(NULL, wake_lock_name);
	if (!ws) {
		pr_err("%s: wakeup source init fail\n", __func__);
		return -1;
	}

	return 0;
}

/*
 * handler for wdt irq for scp
 * dump scp register
 */
static void scp_A_wdt_handler(void)
{
	pr_debug("[SCP] CM4 A WDT exception\n");
	scp_A_dump_regs();
}

/*
 * dispatch scp irq
 * reset scp and generate exception if needed
 * @param irq:      irq id
 * @param dev_id:   should be NULL
 */
irqreturn_t scp_A_irq_handler(int irq, void *dev_id)
{
	unsigned int reg = readl(SCP_A_TO_HOST_REG);

#if SCP_RECOVERY_SUPPORT
	/* if WDT and IPI triggered on the same time, ignore the IPI */
	if (reg & SCP_IRQ_WDT) {
		int retry;
		unsigned long tmp;
		situation_context_obj = situ_context_alloc_object();
		situ_register_control();

		scp_A_wdt_handler();
		sar_exception_data_report();
		sar_algo_exception_data_report();
		sar_algo_top_exception_data_report();
		if (scp_set_reset_status() == RESET_STATUS_STOP) {
			pr_debug("[SCP] CM4 WDT handler start to reset scp...\n");
			scp_send_reset_wq(RESET_TYPE_WDT);
		} else
			pr_notice("scp_A_wdt_handler: scp resetting\n");

		/* clr after SCP side INT trigger,
		 * or SCP may lost INT max wait 5000*40u = 200ms
		 */
		for (retry = SCP_AWAKE_TIMEOUT; retry > 0; retry--) {
			tmp = readl(SCP_GPR_CM4_A_REBOOT);
			if (tmp == CM4_A_READY_TO_REBOOT)
				break;
			udelay(40);
		}
		if (retry == 0)
			pr_debug("[SCP] SCP_A wakeup timeout\n");
		udelay(10);
		writel(SCP_IRQ_WDT, SCP_A_TO_HOST_REG);
	} else if (reg & SCP_IRQ_SCP2HOST) {
		/* if WDT and IPI triggered on the same time, ignore the IPI */
		scp_A_ipi_handler();
		writel(SCP_IRQ_SCP2HOST, SCP_A_TO_HOST_REG);
	}
#else
	int reboot = 0;

	if (reg & SCP_IRQ_WDT) {
		scp_A_wdt_handler();
		reboot = 1;
		reg &= SCP_IRQ_WDT;
	}

	if (reg & SCP_IRQ_SCP2HOST) {
		/* if WDT and IPI triggered on the same time, ignore the IPI */
		if (!reboot)
			scp_A_ipi_handler();
		reg &= SCP_IRQ_SCP2HOST;
	}

	writel(reg, SCP_A_TO_HOST_REG);

	if (reboot)
		scp_aed_reset(EXCEP_RUNTIME, SCP_A_ID);
#endif  // SCP_RECOVERY_SUPPORT

	return IRQ_HANDLED;
}

/*
 * scp irq initialize
 */
void scp_A_irq_init(void)
{
	writel(SCP_IRQ_SCP2HOST, SCP_A_TO_HOST_REG); /* clear scp irq */
}

static int handle_to_index(int handle)
{
	int index = -1;

	switch (handle) {
	case ID_SAR:
		index = sar;
		break;
	case ID_SAR_ALGO:
		index = saralgo;
		break;
	case ID_SAR_ALGO_TOP:
		index = saralgo_top;
		break;
	default:
		index = -1;
		pr_err("%s invalid handle:%d,index:%d\n", __func__,
			handle, index);
		return index;
	}
	pr_debug("%s handle:%d, index:%d\n", __func__, handle, index);
	return index;
}

int sar_exception_data_report(void)
{
	int err = 0, index = -1;
	struct sensor_event event;
	//struct situation_context *cxt = situation_context_obj;

	memset(&event, 0, sizeof(struct sensor_event));
	pr_info("[SCP]sar_exception_data_report\n");

	index = handle_to_index(ID_SAR);
	if (index < 0) {
		pr_err("[%s] invalid index\n", __func__);
		return -1;
	}
	event.handle = ID_SAR;
	event.flush_action = DATA_ACTION;
	event.word[0] = (30000 << 16)|30000;         //top diff|bottom diff
	event.word[1] = (10000 << 16)|10000;         //top useful|bottom useful
	event.word[2] = (30001 << 16)|30001;         //top average|bottom average
	event.word[3] = (30000 << 16)|10000;         //ref diff|ref userful
	event.word[4] = (30001 << 16)|(3 << 8)|3;    //ref average|top_state|bottom_state
	event.word[5] = 0;
	event.word[6] = 0;
	event.word[7] = 0;

	err = sensor_input_event(situation_context_obj->mdev.minor, &event);

	__pm_wakeup_event(ws, 250);

	return err;
}

int sar_algo_exception_data_report(void)
{
	int err = 0, index = -1;
	struct sensor_event event;
	//struct situation_context *cxt = situation_context_obj;

	memset(&event, 0, sizeof(struct sensor_event));
	pr_info("[SCP] sar_algo_exception_data_report\n");

	index = handle_to_index(ID_SAR_ALGO);
	if (index < 0) {
		pr_err("[%s] invalid index\n", __func__);
		return -1;
	}
	event.handle = ID_SAR_ALGO;
	event.flush_action = DATA_ACTION;
	event.word[0] = 3;         //bottom state
	err = sensor_input_event(situation_context_obj->mdev.minor, &event);
	pr_err("exception bottom_state[0] = %d\n", event.word[0]);

	__pm_wakeup_event(ws, 250);

	return err;
}

int sar_algo_top_exception_data_report(void)
{
	int err = 0, index = -1;
	struct sensor_event event;
	//struct situation_context *cxt = situation_context_obj;

	memset(&event, 0, sizeof(struct sensor_event));
	pr_info("[SCP] sar_algo_top_exception_data_report\n");

	index = handle_to_index(ID_SAR_ALGO_TOP);
	if (index < 0) {
		pr_err("[%s] invalid index\n", __func__);
		return -1;
	}
	event.handle = ID_SAR_ALGO_TOP;
	event.flush_action = DATA_ACTION;
	event.word[0] = 3;         //top state
	err = sensor_input_event(situation_context_obj->mdev.minor, &event);
	pr_err("exception top_state[0] = %d\n", event.word[0]);

	__pm_wakeup_event(ws, 250);

	return err;
}

