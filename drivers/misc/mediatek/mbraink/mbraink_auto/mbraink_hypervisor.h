/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MBRAINK_HYPERVISOR_H
#define MBRAINK_HYPERVISOR_H

#include <mbraink_auto_ioctl_struct_def.h>

typedef struct {
	uint32_t reason[NBL_TRACE_VCPU_CNT][NBL_TRACE_VM_CNT];
} wfe_exit_buf_t;
extern int start_record_wfe_exit(void);
extern int stop_record_wfe_exit(void);
extern wfe_exit_buf_t *get_wfe_exit_buf(void);

typedef struct {
	struct mbraink_send_timestamp sendIpi[MAX_BUF_SIZE];
	struct mbraink_received_timestamp receiveIpi[MAX_BUF_SIZE];
	uint32_t snd_point;
	uint32_t rsv_point;
	bool enable;
} cpu_ipi_delay_buf_t;
extern int start_record_cpu_ipi_delay(void);
extern int stop_record_cpu_ipi_delay(void);
extern cpu_ipi_delay_buf_t *get_cpu_ipi_delay_buf(void);

typedef struct {
	struct mbraink_hyp_comcat_trace_ringbuf_hdr rb_hdr;
	struct mbraink_hyp_comcat_rec rec[COMCAT_TRACE_MAX_RECS];
} comcat_trace_buf_t;
extern int start_record_comcat_trace(void);
extern int stop_record_comcat_trace(void);
extern comcat_trace_buf_t *get_comcat_trace_buf(void);

typedef struct {
	struct mbraink_hyp_vCpu_sched_delay_buf_hdr delay[NBL_TRACE_VM_CNT][NBL_TRACE_VCPU_CNT];
	bool enable;
} vcpu_sched_delay_buf_t;
extern int start_record_vcpu_sched_delay(void);
extern int stop_record_vcpu_sched_delay(void);
extern vcpu_sched_delay_buf_t *get_vcpu_sched_delay_buf(void);

extern long handle_hyp_wfe_exit_count_info(const void *arg, void *mbraink_data);
extern long handle_hyp_ipi_info(const void *arg, void *mbraink_data);
extern long handle_hyp_virq_inject_info(const void *arg, void *mbraink_data);
extern long handle_hyp_vCpu_sched_info(const void *arg, void *mbraink_data);

#endif /*end of MBRAINK_HYPERVISOR_H*/
