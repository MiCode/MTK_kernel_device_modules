// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

/**
 * @file    gpueb_ipi_init.c
 * @brief   IPI init flow for gpueb
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/seq_file.h>
#include <linux/pm_runtime.h>
#include <mboot_params.h>

#include "gpueb_ipi.h"
#include "gpueb_helper.h"

// MTK common IPI/MBOX
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include <linux/soc/mediatek/mtk-mbox.h>

struct mtk_mbox_device   gpueb_mboxdev;
struct mtk_ipi_device    gpueb_ipidev;
struct mtk_mbox_info     *gpueb_mbox_info;
struct mtk_mbox_pin_send *gpueb_mbox_pin_send;
struct mtk_mbox_pin_recv *gpueb_mbox_pin_recv;
const char *gpueb_mbox_pin_send_name[20];
const char *gpueb_mbox_pin_recv_name[20];
unsigned int g_mbox_size = 0;
unsigned int g_slot_size = 0;
unsigned int g_ts_mbox;
static void __iomem *g_gpueb_mbox_ipi;

#if IPI_TEST
struct test_msg {
	int msg;
	int padding[3];
} msg_tx[32], msg_rx[32];
#endif

static int gpueb_ipi_table_init(struct platform_device *pdev)
{
	enum table_item_num {
		send_item_num = 3,
		recv_item_num = 5
	};
	int ret;
	u32 i, mbox_id, recv_opt, cb_opt, pin_name_size, cnt_elems;

	gpueb_mboxdev.name = GPUEB_MBOXDEV_NAME;

	// Get MBOX num
	of_property_read_u32(pdev->dev.of_node, "mbox-count",
			&gpueb_mboxdev.count);
	if (!gpueb_mboxdev.count) {
		gpueb_pr_debug("mbox count not found");
		return false;
	}

	// Get MBOX size
	of_property_read_u32(pdev->dev.of_node, "mbox-size",
			&g_mbox_size);
	if (g_mbox_size == 0) {
		gpueb_pr_debug("mbox size not found");
		return false;
	}

	// Get SLOT size
	of_property_read_u32(pdev->dev.of_node, "slot-size",
			&g_slot_size);
	if (g_slot_size == 0) {
		gpueb_pr_debug("slot size not found");
		return false;
	}

	// Get mbox for timesync
	of_property_read_u32(pdev->dev.of_node, "ts-mbox",
			&g_ts_mbox);
	if (g_ts_mbox > gpueb_mboxdev.count) {
		gpueb_pr_debug("ts-mbox(%d) > mbox-count(%d)",
			g_ts_mbox, gpueb_mboxdev.count);
		return false;
	}

	// Get send PIN num
	cnt_elems = of_property_count_u32_elems(
			pdev->dev.of_node, "send-table");
	if (cnt_elems <= 0) {
		gpueb_pr_debug("send table not found");
		return false;
	}
	gpueb_mboxdev.send_count = cnt_elems / send_item_num;

	// Get recv PIN num
	cnt_elems = of_property_count_u32_elems(
			pdev->dev.of_node, "recv-table");
	if (cnt_elems <= 0) {
		gpueb_pr_debug("recv table not found");
		return false;
	}
	gpueb_mboxdev.recv_count = cnt_elems / recv_item_num;

	// Get send PIN name
	ret = of_property_read_string_array(pdev->dev.of_node,
			"send-name-table",
			gpueb_mbox_pin_send_name,
			gpueb_mboxdev.send_count);
	if (ret < 0) {
		gpueb_pr_debug("Could not find send-name-table in dts");
		return false;
	}

	// Check if #element in gpueb_mbox_pin_send_name is enough or not
	pin_name_size = ARRAY_SIZE(gpueb_mbox_pin_send_name);
	if (pin_name_size < gpueb_mboxdev.send_count) {
		gpueb_pr_debug("gpueb_mbox_pin_send_name size(%d) smaller than send_count:%d",
				pin_name_size, gpueb_mboxdev.send_count);
		return false;
	}

	for (i = 0; i < gpueb_mboxdev.send_count; i++) {
		gpueb_pr_debug("send-name-table[%d] = %s",
				i, gpueb_mbox_pin_send_name[i]);
	}

	// Get recv PIN name
	ret = of_property_read_string_array(pdev->dev.of_node,
			"recv-name-table",
			gpueb_mbox_pin_recv_name,
			gpueb_mboxdev.recv_count);
	if (ret < 0) {
		gpueb_pr_debug("Could not find recv-name-table in dts");
		return false;
	}

	// Check if #element in gpueb_mbox_pin_send_name is enough or not
	pin_name_size = ARRAY_SIZE(gpueb_mbox_pin_recv_name);
	if (pin_name_size < gpueb_mboxdev.recv_count) {
		gpueb_pr_debug("gpueb_mbox_pin_recv_name size(%d) smaller than recv_count:%d",
				pin_name_size, gpueb_mboxdev.recv_count);
		return false;
	}

	for (i = 0; i < gpueb_mboxdev.recv_count; i++) {
		gpueb_pr_debug("recv-name-table[%d] = %s",
				i, gpueb_mbox_pin_recv_name[i]);
	}

	// Alloc and init mtk_mbox_info for GPUEB
	gpueb_mboxdev.info_table = vzalloc(sizeof(struct mtk_mbox_info)
			* gpueb_mboxdev.count);
	if (!gpueb_mboxdev.info_table)
		return false;
	gpueb_mbox_info = gpueb_mboxdev.info_table;
	for (i = 0; i < gpueb_mboxdev.count; i++) {
		gpueb_mbox_info[i].id = i;
		gpueb_mbox_info[i].slot = g_mbox_size;
		gpueb_mbox_info[i].enable = 1;
		gpueb_mbox_info[i].is64d = 0;
		gpueb_mbox_info[i].opt = MBOX_OPT_SMEM;
	}

	// Alloc and init send PIN table
	gpueb_mboxdev.pin_send_table = vzalloc(sizeof(struct mtk_mbox_pin_send)
			* gpueb_mboxdev.send_count);
	if (!gpueb_mboxdev.pin_send_table)
		return false;
	gpueb_mbox_pin_send = gpueb_mboxdev.pin_send_table;
	for (i = 0; i < gpueb_mboxdev.send_count; i++) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"send-table",
				i * send_item_num,
				&gpueb_mbox_pin_send[i].chan_id);
		if (ret) {
			gpueb_pr_debug("Cannot get ipi id (%d):%d", i, __LINE__);
			return false;
		}
		gpueb_mbox_pin_send[i].pin_index = gpueb_mbox_pin_send[i].chan_id;

		ret = of_property_read_u32_index(pdev->dev.of_node,
				"send-table",
				i * send_item_num + 1,
				&mbox_id);
		if (ret) {
			gpueb_pr_debug("Cannot get mbox id (%d):%d", i, __LINE__);
			return false;
		}
		/* Because mbox is a bit-field */
		gpueb_mbox_pin_send[i].mbox = mbox_id;

		ret = of_property_read_u32_index(pdev->dev.of_node,
				"send-table",
				i * send_item_num + 2,
				&gpueb_mbox_pin_send[i].msg_size);
		if (ret) {
			gpueb_pr_debug("Cannot get pin size (%d):%d", i, __LINE__);
			return false;
		}
	}

	// Alloc and init recv PIN table
	gpueb_mboxdev.pin_recv_table = vzalloc(sizeof(struct mtk_mbox_pin_recv)
			* gpueb_mboxdev.recv_count);
	if (!gpueb_mboxdev.pin_recv_table)
		return false;
	gpueb_mbox_pin_recv = gpueb_mboxdev.pin_recv_table;
	for (i = 0; i < gpueb_mboxdev.recv_count; ++i) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv-table",
				i * recv_item_num,
				&gpueb_mbox_pin_recv[i].chan_id);
		if (ret) {
			gpueb_pr_debug("Cannot get ipi id (%d):%d", i, __LINE__);
			return false;
		}
		gpueb_mbox_pin_recv[i].pin_index = gpueb_mbox_pin_recv[i].chan_id;

		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv-table",
				i * recv_item_num + 1,
				&mbox_id);
		if (ret) {
			gpueb_pr_debug("Cannot get mbox id (%d):%d", i, __LINE__);
			return false;
		}
		/* Because mbox is a bit-field */
		gpueb_mbox_pin_recv[i].mbox = mbox_id;

		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv-table",
				i * recv_item_num + 2,
				&gpueb_mbox_pin_recv[i].msg_size);
		if (ret) {
			gpueb_pr_debug("Cannot get pin size (%d):%d", i, __LINE__);
			return false;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv-table",
				i * recv_item_num + 3,
				&recv_opt);
		if (ret) {
			gpueb_pr_debug("Cannot get recv opt (%d):%d", i, __LINE__);
			return false;
		}
		/* because recv_opt(0:receive ,1: response) is a bit-field */
		gpueb_mbox_pin_recv[i].recv_opt = recv_opt;

		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv-table",
				i * recv_item_num + 4,
				&cb_opt);
		if (ret) {
			gpueb_pr_debug("Cannot get callback opt (%d):%d", i, __LINE__);
			return false;
		}
		/* because cb_ctx_opt(0:isr context, 1:process context) is a bit-field */
		gpueb_mbox_pin_recv[i].cb_ctx_opt = cb_opt;
	}

	return true;
}

void gpueb_mbox_setup_pin_table(unsigned int mbox)
{
	unsigned int i;
	int last_ofs = 0;

	for (i = 0; i < gpueb_mboxdev.send_count; i++) {
		if (mbox == gpueb_mbox_pin_send[i].mbox) {
			gpueb_mbox_pin_send[i].offset = last_ofs;
			last_ofs += gpueb_mbox_pin_send[i].msg_size;
		}
	}

	for (i = 0; i < gpueb_mboxdev.recv_count; i++) {
		if (mbox == gpueb_mbox_pin_recv[i].mbox) {
			gpueb_mbox_pin_recv[i].offset = last_ofs;
			last_ofs += gpueb_mbox_pin_recv[i].msg_size;
		}
	}

	if (last_ofs > g_mbox_size)
		gpueb_pr_debug("mbox%d exceed the maximum size", mbox);

	return;
}

void gpueb_plat_ipi_timeout_cb(int ipi_id)
{
	gpueb_pr_debug("Error: possible error IPI %d", ipi_id);

	ipi_monitor_dump(&gpueb_ipidev);
	//mtk_emidbg_dump();
	//BUG_ON(1);

	return;
}

int gpueb_ipi_init(struct platform_device *pdev)
{
	int i = 0;
	int ret;
	struct device *gpueb_dev = &pdev->dev;
	struct resource *res = NULL;

	ret = gpueb_ipi_table_init(pdev);
	if (ret == 0)
		return -ENODEV;

	// Create mbox dev
	gpueb_pr_debug("mbox probe start");
	for (i = 0; i < gpueb_mboxdev.count; i++) {
		gpueb_mbox_info[i].mbdev = &gpueb_mboxdev;
		ret = mtk_mbox_probe(pdev, gpueb_mbox_info[i].mbdev, i);
		if (ret < 0 || gpueb_mboxdev.info_table[i].irq_num < 0) {
			gpueb_pr_debug("mbox%d probe fail, ret = %d", i, ret);
			continue;
		}

		ret = enable_irq_wake(gpueb_mboxdev.info_table[i].irq_num);
		if (ret < 0) {
			gpueb_pr_debug("mbox%d enable irq fail, ret = %d", i, ret);
			continue;
		}
		gpueb_mbox_setup_pin_table(i);
	}

	gpueb_ipidev.name = "gpueb_ipidev";
	gpueb_ipidev.id = IPI_DEV_GPUEB;
	gpueb_ipidev.mbdev = &gpueb_mboxdev;
	gpueb_ipidev.timeout_handler = gpueb_plat_ipi_timeout_cb;

	/* initialize mbox (share memory) */
	for (i = 1; i < gpueb_mboxdev.count; i++) {
		ret = mtk_smem_init(pdev, gpueb_mbox_info[i].mbdev, i,
			gpueb_mbox_info[i].mbdev->info_table[i].base,
			gpueb_mbox_info[i].mbdev->info_table[i].set_irq_reg,
			gpueb_mbox_info[i].mbdev->info_table[i].clr_irq_reg,
			gpueb_mbox_info[i].mbdev->info_table[i].send_status_reg,
			gpueb_mbox_info[i].mbdev->info_table[i].recv_status_reg);
		if (ret) {
			gpueb_pr_debug("mbox%d smem init fali, ret = %d", i, ret);
			return ret;
		}
	}

	/*
	 * IPI device register
	 *
	 * It must be noted that the number of GPUEB's
	 * send pin and receive pin are the same.
	 * If you need specific design on it, you must adjust
	 * the registered ipi num to the big one.
	 */
	ret = mtk_ipi_device_register(
			&gpueb_ipidev,
			pdev,
			&gpueb_mboxdev,
			gpueb_mboxdev.send_count);
	if (ret != IPI_ACTION_DONE) {
		gpueb_pr_debug("ipi devcie register fail!");
		return ret;
	}
	gpueb_pr_debug("mbox probe done");

#if IPI_TEST
	ret = gpueb_ipi_test_init();
	if (ret) {
		gpueb_pr_info("fail to init ipi register (%d)", ret);
		WARN_ON(1);
		return ret;
	}
#endif

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mbox0_send");
	if (unlikely(!res)) {
		gpueb_pr_info("fail to get resource MBOX0_SEND");
		return -1;
	}
	g_gpueb_mbox_ipi = devm_ioremap(gpueb_dev, res->start, resource_size(res));
	if (unlikely(!g_gpueb_mbox_ipi)) {
		gpueb_pr_info("fail to ioremap MBOX0_SEND: 0x%llx", res->start);
		return -1;
	}

	return 0;
}

#if IPI_TEST
int gpueb_ipi_test_init(void)
{
	int ret = 0;
	int ipi = 0;
	unsigned int ipi_count = gpueb_mboxdev.send_count;

	/* Register IPI channel */
	for (ipi = 0; ipi < ipi_count; ipi++) {
		ret = mtk_ipi_register(&gpueb_ipidev,
				ipi,
				NULL,
				NULL,
				(void *)&msg_rx[ipi]);
		if (ret != IPI_ACTION_DONE) {
			gpueb_pr_debug("ipi:#%d register fail! ret = %d",
					ipi, ret);
			if (ret == IPI_DUPLEX) {
				/* ipi already registered, unregister it and register again */
				gpueb_pr_debug("ipi:#%d register: IPI_DUPLEX",
					ipi);
				ret = mtk_ipi_unregister(&gpueb_ipidev, ipi);
				if (ret != IPI_ACTION_DONE) {
					gpueb_pr_debug("ipi:#%d unregister fail! ret = %d",
						ipi, ret);
					break;
				}
				ret = mtk_ipi_register(&gpueb_ipidev, ipi, NULL, NULL,
					(void *)&msg_rx[ipi]);
				if (ret != IPI_ACTION_DONE) {
					gpueb_pr_debug("ipi:#%d register fail again! ret = %d",
						ipi, ret);
					break;
				}
			} else {
				break;
			}
		}
	}
	return ret;
}

int gpueb_ipi_send_compl_test(int ipi, int msg)
{
	int ret = 0;

	/* Test mtk_ipi_send_compl */
	msg_tx[ipi].msg = msg;
	gpueb_pr_debug("ipi:#%d mtk_ipi_send_compl data: %d",
			ipi, msg_tx[ipi].msg);
	ret = mtk_ipi_send_compl(
		&gpueb_ipidev, // GPUEB's IPI device
		ipi, // Send channel
		0, // 0: wait, 1: polling
		(void *)&msg_tx[ipi], // Send data
		1, // 1 slot message = 1 * 4 = 4 bytes
		IPI_TIMEOUT_MS); // Timeout value in milisecond

	if (ret != IPI_ACTION_DONE) {
		gpueb_pr_info("IPI fail ret=%d", ret);
		return ret;
	}
	gpueb_pr_debug("ipi:#%d ack data: %d",
			ipi, msg_rx[ipi].msg);

	return msg_rx[ipi].msg;
}

int gpueb_get_send_pin_count(void)
{
	return gpueb_mboxdev.send_count;
}
#endif

unsigned int gpueb_get_ts_mbox(void)
{
	return g_ts_mbox;
}

int gpueb_get_send_PIN_offset_by_name(char *send_PIN_name)
{
	int i;

	for (i = 0; i < gpueb_mboxdev.send_count; i++) {
		if (!strcmp(gpueb_mbox_pin_send_name[i], send_PIN_name))
			return gpueb_mbox_pin_send[i].offset;
	}
	return -1;
}

int gpueb_get_send_PIN_ID_by_name(char *send_PIN_name)
{
	int i;

	for (i = 0; i < gpueb_mboxdev.send_count; i++) {
		if (!strcmp(gpueb_mbox_pin_send_name[i], send_PIN_name))
			return gpueb_mbox_pin_send[i].chan_id;
	}
	return -1;
}
EXPORT_SYMBOL_GPL(gpueb_get_send_PIN_ID_by_name);

int gpueb_get_recv_PIN_ID_by_name(char *recv_PIN_name)
{
	int i;

	for (i = 0; i < gpueb_mboxdev.recv_count; i++) {
		if (!strcmp(gpueb_mbox_pin_recv_name[i], recv_PIN_name))
			return gpueb_mbox_pin_recv[i].chan_id;
	}
	return -1;
}
EXPORT_SYMBOL_GPL(gpueb_get_recv_PIN_ID_by_name);

void *get_gpueb_ipidev(void)
{
	return &gpueb_ipidev;
}
EXPORT_SYMBOL_GPL(get_gpueb_ipidev);

unsigned int gpueb_get_mbox1_irq(void)
{
	unsigned int val = 0;

	if (g_gpueb_mbox_ipi) {
		/* g_gpueb_mbox_ipi (0x13C62000) */
		/* GPUEB to APMCU IRQ[1] SW INT Status (0x13C62088) */
		val = readl(g_gpueb_mbox_ipi + 0x88);
	} else
		gpueb_pr_info("null g_gpueb_mbox_ipi");

	return val;
}
EXPORT_SYMBOL_GPL(gpueb_get_mbox1_irq);

void gpueb_clr_mbox1_irq(unsigned int val)
{
	if (g_gpueb_mbox_ipi) {
		/* g_gpueb_mbox_ipi (0x13C62000) */
		/* GPUEB to APMCU IRQ[1] SW INT Clear (0x13C62084) */
		writel(val, (g_gpueb_mbox_ipi + 0x84));
	} else
		gpueb_pr_info("null g_gpueb_mbox_ipi");
}
EXPORT_SYMBOL_GPL(gpueb_clr_mbox1_irq);
