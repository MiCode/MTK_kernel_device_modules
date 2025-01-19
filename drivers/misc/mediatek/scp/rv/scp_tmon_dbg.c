// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include "scp_mbrain_dbg.h"
#include "scp_tmon_dbg.h"
#include "scp_dvfs.h"
#include "scp.h"
#include "scp_helper.h"

static struct scp_res_mbrain_header header;

static void get_scp_tmon_header(void)
{
	header.mbrain_module = SCP_TMON_DATA_MODULE_ID;
	header.version = SCP_TMON_DATA_VERSION;
	header.data_offset = sizeof(struct scp_res_mbrain_header);
	header.index_data_length = sizeof(struct task_monitor_info)*MAX_TASKS
							 + header.data_offset;
	pr_notice("scp_tmon size=%lx, header.data_offset size=%hx\n",
			sizeof(struct task_monitor_info)*MAX_TASKS, header.data_offset);
}

static void *scp_data_copy(void *dest, void *src, uint64_t size)
{
	memcpy(dest, src, size);
	dest += size;
	return dest;
}

static int scp_mbrain_get_sys_tmon_data(void *address, uint32_t size)
{
	struct task_monitor_info *tmon = NULL;
	/* uint64_t suspend_time = 0;*/

	pr_notice("[SCP] %s start\n",__func__);
	tmon = (struct task_monitor_info *)scp_get_reserve_mem_virt(SCP_TMON_DBG_MEM_ID);

	pr_notice("Prepare header\n");
	/* cpy header */
	get_scp_tmon_header();
	pr_notice("address=%p, header size=%lx\n", address, sizeof(struct scp_res_mbrain_header));
	address = scp_data_copy(address, &header, sizeof(struct scp_res_mbrain_header));
	pr_notice("Prepare data\n");
	/* cpy res data */
	pr_notice("address=%p, data size=%lx\n", address, sizeof(struct task_monitor_info)*MAX_TASKS);
	address = scp_data_copy(address, tmon, sizeof(struct task_monitor_info)*MAX_TASKS);
	pr_notice("[SCP] %s end\n",__func__);
	return 0;
}

static unsigned int scp_mbrain_get_sys_tmon_length(void)
{
	get_scp_tmon_header();
	return header.index_data_length;
}

static struct scp_res_mbrain_dbg_ops scp_tmon_mbrain_ops = {
	.get_length = scp_mbrain_get_sys_tmon_length,
	.get_data = scp_mbrain_get_sys_tmon_data,
};

int scp_sys_tmon_mbrain_plat_init (void)
{
	return register_scp_mbrain_tmon_ops(&scp_tmon_mbrain_ops);
}
