/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef MTK_MMDVFS_DEBUG_H
#define MTK_MMDVFS_DEBUG_H

#include <linux/seq_file.h>
#include <linux/types.h>

#define MAX_OPP		(8)
#define MMDVFS_RES_DATA_MODULE_ID	8
#define MMDVFS_RES_USR_DATA_MODULE_ID	10
#define MMDVFS_RES_DATA_VERSION		0

struct mmdvfs_res_mbrain_header {
	uint8_t mbrain_module;
	uint8_t version;
	uint16_t data_offset;
	uint32_t index_data_length;
};

struct mmdvfs_opp_record {
	uint64_t opp_duration[MAX_OPP];
};

struct mmdvfs_record_opp {
	uint64_t sec;
	uint64_t usec;
	uint8_t opp;
};

enum {
	MMDVFS_POWER_0,
	MMDVFS_POWER_1,
	MMDVFS_POWER_2,
	MMDVFS_ALONE_CAM,
	MMDVFS_OPP_RECORD_NUM
};

enum {
	MMDVFS_USER_0,
	MMDVFS_USER_1,
	MMDVFS_USER_2,
	MMDVFS_USER_3,
	MMDVFS_USER_4,
	MMDVFS_USER_5,
	MMDVFS_USER_6,
	MMDVFS_USER_7,
	MMDVFS_USER_8,
	MMDVFS_USER_9,
	MMDVFS_USER_10,
	MMDVFS_USER_11,
	MMDVFS_USER_12,
	MMDVFS_USER_13,
	MMDVFS_USER_14,
	MMDVFS_USER_15,
	MMDVFS_USER_OPP_RECORD_NUM
};

struct mmdvfs_res_mbrain_debug_ops {
	unsigned int (*get_length)(void);
	int (*get_data)(void *address, uint32_t size);
};

struct mmdvfs_res_mbrain_debug_ops *get_mmdvfs_mbrain_dbg_ops(void);
struct mmdvfs_res_mbrain_debug_ops *get_mmdvfs_mbrain_usr_dbg_ops(void);
int mtk_mmdvfs_debug_release_step0(void);
void mtk_mmdvfs_debug_ulposc_enable(const bool enable);
int mtk_mmdvfs_debug_force_vcore_notify(const u32 val);
bool mtk_is_mmdvfs_v3_debug_init_done(void);
int mmdvfs_debug_status_dump(struct seq_file *file);
int mmdvfs_stop_record(void);
inline void mmdvfs_vcp_cb_mutex_lock(void);
inline void mmdvfs_vcp_cb_mutex_unlock(void);
inline bool mmdvfs_vcp_cb_ready_get(void);

int mmdvfs_debug_force_step(const u8 idx, const s8 opp);
int mmdvfs_debug_vote_step(const u8 idx, const s8 opp);

struct mmdvfs_debug_ops {
	int (*force_step_fp)(const char *val, const struct kernel_param *kp);
	int (*vote_step_fp)(const char *val, const struct kernel_param *kp);
	int (*status_dump_fp)(struct seq_file *file);
	int (*record_snapshot_fp)(void);
	int (*force_vcore_fp)(const u32 val);
	struct mmdvfs_res_mbrain_debug_ops *(*mmdvfs_mbrain_fp)(void);
	struct mmdvfs_res_mbrain_debug_ops *(*mmdvfs_mbrain_usr_fp)(void);
	int (*ap_set_rate_fp)(const char *val, const struct kernel_param *kp);
	void (*release_step_fp)(void);
	int (*mmdvfs_ftrace_fp)(const char *val, const struct kernel_param *kp);
};
void mmdvfs_debug_ops_set(struct mmdvfs_debug_ops *_ops);
#endif

