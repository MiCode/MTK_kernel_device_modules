/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#ifndef MBRAINK_V6989_POWER_H
#define MBRAINK_V6989_POWER_H

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <mbraink_ioctl_struct_def.h>

#if IS_ENABLED(CONFIG_MTK_LOW_POWER_MODULE) && \
		IS_ENABLED(CONFIG_MTK_SYS_RES_DBG_SUPPORT)
#include <lpm_dbg_common_v2.h>
#endif

#define MAX_POWER_HD_SZ 8
#define SPM_DATA_SZ (5640)
#define SPM_TOTAL_SZ (MAX_POWER_HD_SZ+SPM_DATA_SZ)

#define MMDVFS_DATA_SZ (256)
#define MMDVFS_TOTAL_SZ (MAX_POWER_HD_SZ+MMDVFS_DATA_SZ)

#if IS_ENABLED(CONFIG_MTK_LPM_MT6985) && \
	IS_ENABLED(CONFIG_MTK_LOW_POWER_MODULE) && \
	IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
extern struct md_sleep_status before_md_sleep_status;
int is_md_sleep_info_valid(struct md_sleep_status *md_data);
void get_md_sleep_time(struct md_sleep_status *md_data);
#endif /*end of CONFIG_MTK_LPM_MT6985 && CONFIG_MTK_LOW_POWER_MODULE && CONFIG_MTK_ECCCI_DRIVER*/

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
#define MD_BLK_MAX_NUM 108
#define MD_DATA_TOTAL_SZ (MD_MDHD_SZ+MD_BLK_SZ*MD_BLK_MAX_NUM)
#define MD_MAX_SZ (MD_HD_SZ+MD_DATA_TOTAL_SZ)
#define MD_STATUS_W_DONE 0xEDEDEDED
#define MD_STATUS_W_ING  0xEEEEEEEE
#define MD_STATUS_R_DONE 0xFFFFFFFF
#endif

extern int mbraink_netlink_send_msg(const char *msg);


int vcorefs_get_src_req_num(void);
unsigned int *vcorefs_get_src_req(void);

int mbraink_v6989_power_init(void);
int mbraink_v6989_power_deinit(void);


#endif /*end of MBRAINK_V6989_POWER_H*/
