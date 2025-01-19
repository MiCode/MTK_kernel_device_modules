/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#ifndef MBRAINK_AUTO_CPULOAD_H
#define MBRAINK_AUTO_CPULOAD_H
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/sched/clock.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>

#include <mbraink_ioctl_struct_def.h>

// vcpu execution time flag
enum {
	VCPU_EXEC_ENTER,
	VCPU_EXEC_EXIT,
};

// vcpu exit reason
enum {
	VCPU_EXIT_PHYSICAL_INTERRUPT,
	VCPU_EXIT_WFI_WFE_INSTRUCTION,
	VCPU_EXIT_SMC_INSTRUCTION,
	VCPU_EXIT_HVC_INSTRUCTION,
	VCPU_EXIT_SYSTEM_INSTRUCTION,
	VCPU_EXIT_INSTRUCTION_ABORT,
	VCPU_EXIT_DATA_ABORT,
	VCPU_EXIT_UNKNOWN,
	VCPU_EXIT_FAILURE,
	VCPU_EXIT_COUNT,
};

static const char *const vcpu_exit_reason[] = {
	[VCPU_EXIT_PHYSICAL_INTERRUPT] = "physical-interrupt",
	[VCPU_EXIT_WFI_WFE_INSTRUCTION] = "wfi_wfe_instruction",
	[VCPU_EXIT_SMC_INSTRUCTION] = "smc_instruction",
	[VCPU_EXIT_HVC_INSTRUCTION] = "hvc_instruction",
	[VCPU_EXIT_SYSTEM_INSTRUCTION] = "system_instruction",
	[VCPU_EXIT_INSTRUCTION_ABORT] = "instruction_abort",
	[VCPU_EXIT_DATA_ABORT] = "data_abort",
	[VCPU_EXIT_UNKNOWN] = "unknown",
	[VCPU_EXIT_FAILURE] = "failure",
};

#define NBL_TRACE_VCPU_CNT  8
#define NBL_TRACE_VM_CNT    2
struct trace_vcpu_exit {
	uint32_t reason[NBL_TRACE_VCPU_CNT][NBL_TRACE_VM_CNT][VCPU_EXIT_COUNT];
};

#define NBL_TRACE_INTERRUPT_MAX_V9    1024
struct trace_irq_count {
	uint32_t irq_count[NBL_TRACE_INTERRUPT_MAX_V9];
};

struct trace_vcpu_accum {
	uint64_t enter_tick[NBL_TRACE_VCPU_CNT][NBL_TRACE_VM_CNT];
	uint64_t exec_tick[NBL_TRACE_VCPU_CNT][NBL_TRACE_VM_CNT];
	uint64_t cur_tick;
};

struct trace_vcpu_ringbuf_hdr {
	uint64_t magic;
#define NBL_TRACE_MAGIC  0x5472616365637075 // Tracecpu
	uint64_t write_pos;
	uint64_t read_pos;
	size_t hdr_size;
	size_t size;
	size_t rec_size;
	uint32_t rec_cnt;
	uint8_t reserved0[4];
	bool enable;
	uint8_t reserved1[7];
};

// trace vm entey/exit timestamp
struct trace_vcpu_exec {
	struct trace_vcpu_ringbuf_hdr rb_hdr;
	struct trace_vcpu_rec recs[];
};

struct trace_irq_delay_ringbuf_hdr {
	uint64_t write_pos;
	uint64_t read_pos;
	size_t hdr_size;
	size_t size;
	size_t rec_size;
	uint32_t rec_cnt;
	bool enable;
	uint8_t reserved[3];
};

struct trace_irq_rec {
	uint64_t flag : 1;
	uint64_t vector : 13;
	uint64_t timestamp : 50;
};

#define TRACE_VCPU_IRQ_DELAY_MAX_RECS       10240
struct trace_irq_delay {
	struct trace_irq_delay_ringbuf_hdr rb_hdr;
	struct trace_irq_rec rec[TRACE_VCPU_IRQ_DELAY_MAX_RECS];
};

//tracing pcpu's attributes
struct trace_pcpu_attr {
	// vmid , yocto: 0,  android: 1, nebula: 2
	uint64_t current_vmid : 3;
#define TRACE_NEBULA_VMID    2
	//vcpu id, 0~7 is valid, 8 means nebule or idle
	uint64_t current_vcpu_id : 5;
#define NON_VM_CPU_ID   0xff
	//thread priority of vcpu
	uint64_t vcpu_thread_priority : 8;
};

struct trace_vcpu_thread_delay_header {
	uint64_t enter_timestamp;
	uint64_t current_delay_time;
	uint64_t max_delay_time;
	uint64_t min_delay_time;
	uint64_t total_delay_time;
	uint64_t thread_id;
	uint64_t run_times;
	uint64_t bRun;
};

struct trace_vcpu_thread_delay{
	struct trace_vcpu_thread_delay_header delay[TRACE_NEBULA_VMID][NBL_TRACE_VCPU_CNT];
	bool enable;
};

//begin nbl_trace top part
#define STATE_INFO_LEN  10
#define PROC_NAME_LEN 15
#define THREAD_NAME_LEN 35
struct thread_attr {
	uint32_t   PID;
	uint32_t   TID;
	uint32_t   percent;
	char       state[STATE_INFO_LEN];//state string, 10 byte is enough
	char       proc_name[PROC_NAME_LEN]; //proc name, size 15 is enough
	char       thread_name[THREAD_NAME_LEN]; //thread name, size 35 is enough
};

#define TOP_THREAD_COUNT   50
struct nbl_trace_top {
	struct thread_attr attr_info[TOP_THREAD_COUNT];  //currently 50 is enough
	bool write_done;
	bool read_done;
	bool enable;
};

struct vcpu_rec {
	// For the case that vcpu-pcpu pairs would be changed in run-time.
	uint64_t pcpu : 5;
	uint64_t flag : 1;
	// vcpu entry/exit time measured in arm generic timer ticks
	uint64_t ticks : 58;
};

#define VCPU_EXEC_REC_CNT     80000
struct vcpu_exec {
	bool enable;
	uint32_t write_pos[NBL_TRACE_VM_CNT][NBL_TRACE_VCPU_CNT];
	struct vcpu_rec recs[NBL_TRACE_VM_CNT][NBL_TRACE_VCPU_CNT][VCPU_EXEC_REC_CNT];
};

struct nbl_trace_buf {
	uint32_t ktrace_rb_off;
	struct trace_irq_count irq_c;
	struct trace_vcpu_exit exit;
	struct trace_vcpu_accum accum;
	struct trace_vcpu_thread_delay vcpu_thread;
	struct trace_irq_delay irq_delay;
	struct trace_pcpu_attr pcpu_attr[NBL_TRACE_VCPU_CNT];
	struct nbl_trace_top top_info;
	struct vcpu_exec vcpu;
	// NOTE: exec MUST be the last one.
	// TODO: refine to remove this limitation
	struct trace_vcpu_exec exec;
};

struct nbl_trace {
	struct nbl_trace_buf *buf;
	u64 cntfrq;
	struct reserved_mem *rmem;
};

int mbraink_auto_cpuload_init(void);
void mbraink_auto_cpuload_deinit(void);

int vcpu_exec_get_rec_cnt(struct trace_vcpu_ringbuf_hdr *rb_hdr);
int mbraink_auto_vcpu_reader(struct nbl_trace *tc,
				struct nbl_trace_buf_trans *vcpu_loading_buf,
				void *vcpu_data_buffer);
int mbraink_auto_get_vcpu_record(struct nbl_trace_buf_trans *vcpu_loading_buf,
				void *vcpu_data_buffer);
int mbraink_auto_set_vcpu_record(int enable);

#endif  // MBRAINK_AUTO_CPULOAD_H
