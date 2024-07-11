// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include "mbraink_hypervisor.h"

long mbraink_hyp_getWfeExitCountInfo(struct mbraink_hyp_wfe_exit_buf *wfe_exit_buf)
{
	long ret = 0;
	int result = 0;
	struct mbraink_hyp_wfe_exit_buf *tmp_wfe_exit_buf;

	if (!wfe_exit_buf)
		return -1;

	result = start_record_wfe_exit();
	if (result < 0) {
		pr_info("start record hypervisor wfe exit count failed\n");
		return result;
	}

	msleep(100);

	result = stop_record_wfe_exit();
	if (result < 0) {
		pr_info("stop record hypervisor wfe exit count failed\n");
		return result;
	}

	tmp_wfe_exit_buf = (struct mbraink_hyp_wfe_exit_buf *)get_wfe_exit_buf();
	memcpy(wfe_exit_buf, tmp_wfe_exit_buf, sizeof(struct mbraink_hyp_wfe_exit_buf));

	return ret;
}

long handle_hyp_wfe_exit_count_info(const void *arg, void *mbraink_data)
{
	struct mbraink_hyp_wfe_exit_buf *hyp_wfe_exit_buf =
		(struct mbraink_hyp_wfe_exit_buf *)(mbraink_data);
	long ret = 0;

	memset(hyp_wfe_exit_buf,
		0x00,
		sizeof(struct mbraink_hyp_wfe_exit_buf));
	ret = mbraink_hyp_getWfeExitCountInfo(hyp_wfe_exit_buf);
	if (copy_to_user((struct mbraink_hyp_wfe_exit_buf *) arg,
			hyp_wfe_exit_buf,
			sizeof(struct mbraink_hyp_wfe_exit_buf))) {
		pr_notice("Copy hyp_wfe_exit_buf to UserSpace error!\n");
		return -EPERM;
	}
	return ret;
}

long mbraink_hyp_getIpiInfo(struct mbraink_hyp_cpu_ipi_data_ringbuffer *cpu_ipi_data_ringbuffer)
{
	long ret = 0;
	int result = 0;
	struct mbraink_hyp_cpu_ipi_data_ringbuffer *tmp_cpu_ipi_data_ringbuffer;

	if (!cpu_ipi_data_ringbuffer)
		return -1;

	result = start_record_cpu_ipi_delay();
	if (result < 0) {
		pr_info("start record hypervisor ipi latency failed\n");
		return result;
	}

	msleep(100);

	result = stop_record_cpu_ipi_delay();
	if (result < 0) {
		pr_info("stop record hypervisor ipi latency failed\n");
		return result;
	}

	tmp_cpu_ipi_data_ringbuffer = (struct mbraink_hyp_cpu_ipi_data_ringbuffer *)
									get_cpu_ipi_delay_buf();
	memcpy(cpu_ipi_data_ringbuffer, tmp_cpu_ipi_data_ringbuffer,
			sizeof(struct mbraink_hyp_cpu_ipi_data_ringbuffer));

	return ret;
}

long handle_hyp_ipi_info(const void *arg, void *mbraink_data)
{
	struct mbraink_hyp_cpu_ipi_data_ringbuffer *hyp_cpu_ipi_data_ringbuffer =
		(struct mbraink_hyp_cpu_ipi_data_ringbuffer *)(mbraink_data);
	long ret = 0;

	memset(hyp_cpu_ipi_data_ringbuffer,
		0x00,
		sizeof(struct mbraink_hyp_cpu_ipi_data_ringbuffer));
	ret = mbraink_hyp_getIpiInfo(hyp_cpu_ipi_data_ringbuffer);
	if (copy_to_user((struct mbraink_hyp_cpu_ipi_data_ringbuffer *) arg,
			hyp_cpu_ipi_data_ringbuffer,
			sizeof(struct mbraink_hyp_cpu_ipi_data_ringbuffer))) {
		pr_notice("Copy hyp_cpu_ipi_data_ringbuffer to UserSpace error!\n");
		return -EPERM;
	}
	return ret;
}

long mbraink_hyp_getvIrqInjectLatency(struct mbraink_hyp_comcat_trace_buf *hyp_comcat_trace_buf)
{
	long ret = 0;
	int result = 0;
	struct mbraink_hyp_comcat_trace_buf *tmp_hyp_comcat_trace_buf;

	if (!hyp_comcat_trace_buf)
		return -1;

	result = start_record_comcat_trace();
	if (result < 0) {
		pr_info("start record hypervisor vIrq inject latency failed\n");
		return result;
	}

	msleep(100);

	result = stop_record_comcat_trace();
	if (result < 0) {
		pr_info("stop record hypervisor vIrq inject latency failed\n");
		return result;
	}

	tmp_hyp_comcat_trace_buf = (struct mbraink_hyp_comcat_trace_buf *)get_comcat_trace_buf();
	memcpy(hyp_comcat_trace_buf, tmp_hyp_comcat_trace_buf,
			sizeof(struct mbraink_hyp_comcat_trace_buf));

	return ret;
}

long handle_hyp_virq_inject_info(const void *arg, void *mbraink_data)
{
	struct mbraink_hyp_comcat_trace_buf *hyp_comcat_trace_buf =
		(struct mbraink_hyp_comcat_trace_buf *)(mbraink_data);
	long ret = 0;

	memset(hyp_comcat_trace_buf,
		0x00,
		sizeof(struct mbraink_hyp_comcat_trace_buf));
	ret = mbraink_hyp_getvIrqInjectLatency(hyp_comcat_trace_buf);
	if (copy_to_user((struct mbraink_hyp_comcat_trace_buf *) arg,
			hyp_comcat_trace_buf,
			sizeof(struct mbraink_hyp_comcat_trace_buf))) {
		pr_notice("Copy hyp_comcat_trace_buf to UserSpace error!\n");
		return -EPERM;
	}
	return ret;
}

long mbraink_hyp_getvCpuSchedInfo(struct mbraink_hyp_vCpu_sched_delay_buf *vCpu_sched_delay_buf)
{
	long ret = 0;
	int result = 0;
	struct mbraink_hyp_vCpu_sched_delay_buf *tmp_vCpu_sched_delay_buf;

	if (!vCpu_sched_delay_buf)
		return -1;

	result = start_record_vcpu_sched_delay();
	if (result < 0) {
		pr_info("start record hypervisor vcpu sched delay failed\n");
		return result;
	}

	msleep(1000);

	result = stop_record_vcpu_sched_delay();
	if (result < 0) {
		pr_info("stop record hypervisor vcpu sched delay failed\n");
		return result;
	}
	tmp_vCpu_sched_delay_buf = (struct mbraink_hyp_vCpu_sched_delay_buf *)
								get_vcpu_sched_delay_buf();
	memcpy(vCpu_sched_delay_buf, tmp_vCpu_sched_delay_buf,
			sizeof(struct mbraink_hyp_vCpu_sched_delay_buf));

	return ret;
}

long handle_hyp_vCpu_sched_info(const void *arg, void *mbraink_data)
{
	struct mbraink_hyp_vCpu_sched_delay_buf *hyp_vCpu_sched_delay_buf =
		(struct mbraink_hyp_vCpu_sched_delay_buf *)(mbraink_data);
	long ret = 0;

	memset(hyp_vCpu_sched_delay_buf,
		0x00,
		sizeof(struct mbraink_hyp_vCpu_sched_delay_buf));
	ret = mbraink_hyp_getvCpuSchedInfo(hyp_vCpu_sched_delay_buf);
	if (copy_to_user((struct mbraink_hyp_vCpu_sched_delay_buf *) arg,
			hyp_vCpu_sched_delay_buf,
			sizeof(struct mbraink_hyp_vCpu_sched_delay_buf))) {
		pr_notice("Copy hyp_vCpu_sched_delay_buf to UserSpace error!\n");
		return -EPERM;
	}
	return ret;
}
