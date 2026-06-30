/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MBRAINK_V6993_GPU_H
#define MBRAINK_V6993_GPU_H
#include <mbraink_ioctl_struct_def.h>

extern int mbraink_netlink_send_msg(const char *msg);

int mbraink_v6993_gpu_init(void);
int mbraink_v6993_gpu_deinit(void);

void mbraink_v6993_gpu_setQ2QTimeoutInNS(unsigned long long q2qTimeoutInNS);
void mbraink_v6993_gpu_setPerfIdxTimeoutInNS(unsigned long long perfIdxTimeoutInNS);
void mbraink_v6993_gpu_setPerfIdxLimit(int perfIdxLimit);
void mbraink_v6993_gpu_dumpPerfIdxList(void);
void mbraink_v6993_gpu_fpsgoSetGameMode(int isGameMode);

#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
void gpu2mbrain_hint_fenceTimeoutNotify(int pid, void *data, unsigned long long time);
void gpu2mbrain_hint_GpuResetDoneNotify(unsigned long long time);
#endif


#endif /*end of MBRAINK_V6993_GPU_H*/
