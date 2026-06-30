/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_APU_MDW_RV_H__
#define __MTK_APU_MDW_RV_H__

#include "mdw.h"
#include "mdw_rv_msg.h"

struct mdw_rv_dev;

struct mdw_rv_cmd {
	struct mdw_cmd *c;
	struct mdw_mem_map *cb;
	struct mdw_ipi_msg_sync s_msg; // for ipi
	uint64_t start_ts_ns; // create time at ap
};

struct mdw_rv_dev {
	struct rpmsg_device *rpdev;
	struct rpmsg_endpoint *ept;
	struct mdw_device *mdev;

	struct mdw_ipi_param param;
	struct idr msg_idr; // for sync msg
	struct mutex msg_mtx;
	struct mutex mtx;
	struct work_struct init_wk; // init wq to avoid ipi conflict

	/* rv information */
	uint32_t rv_version;
	unsigned long dev_mask[BITS_TO_LONGS(MDW_DEV_MAX)];
	uint8_t dev_num[MDW_DEV_MAX];
	unsigned long mem_mask[BITS_TO_LONGS(MDW_MEM_TYPE_MAX)];
	struct mdw_mem_map minfos[MDW_MEM_TYPE_MAX];
	uint8_t meta_data[MDW_DEV_MAX][MDW_DEV_META_SIZE];

	/* stat buffer */
	struct apu_sysmem_buffer *stat_buf;
	struct apu_sysmem_map *stat_map;
	struct mdw_stat *stat;
	uint64_t stat_iova;

	/* dtime handle */
	struct work_struct power_off_wk;
	struct timer_list power_off_timer;

	uint64_t enter_rv_cb_time;
	uint64_t rv_cb_time;
};

int mdw_rv_dev_init(struct mdw_device *mdev);
void mdw_rv_dev_deinit(struct mdw_device *mdev);
int mdw_rv_dev_run_cmd(struct mdw_fpriv *mpriv, struct mdw_rv_cmd *rc);
int mdw_rv_dev_set_param(struct mdw_rv_dev *mrdev, uint32_t idx, uint32_t val);
int mdw_rv_dev_get_param(struct mdw_rv_dev *mrdev, uint32_t idx, uint32_t *val);
int mdw_rv_dev_power_onoff(struct mdw_rv_dev *mrdev, enum mdw_power_type power_onoff);
int mdw_rv_dev_dtime_handle(struct mdw_rv_dev *mrdev, struct mdw_cmd *c);

int mdw_rv_sw_init(struct mdw_device *mdev);
void mdw_rv_sw_deinit(struct mdw_device *mdev);
int mdw_rv_late_init(struct mdw_device *mdev);
void mdw_rv_late_deinit(struct mdw_device *mdev);
int mdw_rv_run_cmd(struct mdw_fpriv *mpriv, struct mdw_cmd *c);
int mdw_rv_set_power(struct mdw_device *mdev, uint32_t type, uint32_t idx, uint32_t boost);
int mdw_rv_ucmd(struct mdw_device *mdev, uint32_t type, void *vaddr, uint32_t size);
int mdw_rv_set_param(struct mdw_device *mdev, enum mdw_info_type type, uint32_t val);
uint32_t mdw_rv_get_info(struct mdw_device *mdev, enum mdw_info_type type);
int mdw_rv_power_onoff(struct mdw_device *mdev, enum mdw_power_type power_onoff);
int mdw_rv_dtime_handle(struct mdw_cmd *c);
void mdw_rv_cmd_set_affinity(struct mdw_cmd *c, bool enable);
void mdw_rv_einfo_copy_out(struct mdw_cmd *c, void *rv_einfo_entry,
	uint32_t rv_cmd_einfo_size, uint32_t rv_subcmd_einfo_size);
#endif
