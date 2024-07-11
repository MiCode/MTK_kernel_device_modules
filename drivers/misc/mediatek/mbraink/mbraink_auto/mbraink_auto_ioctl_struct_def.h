/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef MBRAINK_AUTO_IOCTL_STRUCT_H
#define MBRAINK_AUTO_IOCTL_STRUCT_H

/*auto ioctl case*/
#define AUTO_CPULOAD_INFO 0

struct trace_vcpu_rec {
	u64 vmid : 2;
#define TRACE_YOCTO_VMID      0
#define TRACE_ANDROID_VMID    1
	u64 vcpu : 5;
	u64 pcpu : 5;
	u64 flag : 2;
	u64 timestamp : 50;
};

struct nbl_trace_buf_trans {
	u32 trans_type;
	u32 length;
	u64 current_time;
	u64 cntcvt;
	u64 cntfrq;
	void *vcpu_data;
};

struct mbraink_auto_ioctl_info {
	u32 auto_ioctl_type;
	void *auto_ioctl_data;
};
#endif
