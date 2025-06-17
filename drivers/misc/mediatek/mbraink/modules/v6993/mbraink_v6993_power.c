// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/rtc.h>
#include <linux/vmalloc.h>
#include <linux/sched/clock.h>
#include <linux/regulator/consumer.h>
#include <mtk_low_battery_throttling.h>
#include <mtk_battery_oc_throttling.h>
#include <mtk_peak_power_budget_mbrain.h>
#include <mtk-mmdvfs-debug.h>
#include <mbraink_modules_ops_def.h>
#include <mmqos.h>
#include "mbraink_v6993_power.h"

#include <scp_mbrain_dbg.h>
#include <dvfsrc-mb.h>
#include <mtk-smap-common.h>

#if IS_ENABLED(CONFIG_DEVICE_MODULES_SPMI_MTK_PMIF)
#include <spmi-mtk.h>
#define MAX_SPMI_SLVID slvid_cnt
#define MAX_SPMI_NACK_CNT spmi_nack_idx_cnt
#else
#define MAX_SPMI_SLVID 32
#define MAX_SPMI_NACK_CNT 64
#endif

#if IS_ENABLED(CONFIG_MFD_MTK_SPMI_PMIC)
#include <mtk-spmi-pmic-debug.h>
#define MAX_SPMI_GLITCH_ID spmi_glitch_idx_cnt
#define MAX_SPMI_PARITY_ERR_CNT spmi_parity_err_idx_cnt
#define MAX_SPMI_PRE_OT_CNT PMIC_PRE_OT_BUF_SIZE
#define MAX_SPMI_PRE_LVSYS_CNT PMIC_PRE_LVSYS_BUF_SIZE
#define MAX_SPMI_CURR_CLAMPING_CNT PMIC_CURR_CLAMPING_BUF_SIZE
#define MAX_SPMI_RG_CNT SPMI_PMIC_DEBUG_RG_BUF_SIZE
#define MAZ_SPMI_CRC_ERR_CNT spmi_crc_err_idx_cnt
#else
#define MAX_SPMI_GLITCH_ID 96
#define MAX_SPMI_PARITY_ERR_CNT 64
#define MAX_SPMI_PRE_OT_CNT 32
#define MAX_SPMI_PRE_LVSYS_CNT 32
#define MAX_SPMI_CURR_CLAMPING_CNT 128
#define MAX_SPMI_RG_CNT 192
#define MAZ_SPMI_CRC_ERR_CNT 64
#endif

#if IS_ENABLED(CONFIG_MTK_SWPM_MODULE)

#include <swpm_module_psp.h>

#include "mbraink_sys_res_mbrain_dbg.h"
#include "mbraink_sys_res_mbrain_plat.h"
#include "mbraink_sys_res_plat.h"

/*spinlock for mbraink spm l2 data*/
static DEFINE_SPINLOCK(spm_l1_lock);

unsigned char *g_spm_raw;
unsigned int g_data_size;
int g_spm_l2_sig_tbl_size;
unsigned int g_spm_l2_sig_num;
unsigned char *g_spm_l2_ls_ptr = NULL, *g_spm_l2_sig_tbl_ptr = NULL;
long long g_spm_l1_data[SPM_L1_DATA_NUM];

#endif

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
#include "mtk_ccci_common.h"

unsigned int g_md_last_has_data_blk_idx;
unsigned int g_md_last_read_blk_idx;
unsigned int g_md_read_count;
#endif

#if IS_ENABLED(CONFIG_MTK_LOW_POWER_MODULE)
#include <lpm_dbg_common_v2.h>
#endif

#if IS_ENABLED(CONFIG_MTK_DVFSRC_MB)
#define MAX_DVFSRC_NUM DVFS_INFO_REG_NUM
#else
#define MAX_DVFSRC_NUM 64
#endif

static struct device *pMBrainkPlatDev;

#if IS_ENABLED(CONFIG_MTK_LOW_POWER_MODULE)
void lpm_get_suspend_event_info(struct lpm_dbg_lp_info *info);
#endif

static int mbraink_v6993_power_getVcoreInfo(struct mbraink_power_vcoreInfo *pmbrainkPowerVcoreInfo)
{
	int ret = 0;
	int i = 0;
	int32_t core_vol_num = 0, core_ip_num = 0;
	struct ip_stats *core_ip_stats_ptr = NULL;
	struct vol_duration *core_duration_ptr = NULL;
	int32_t core_vol_check = 0, core_ip_check = 0;
	uint32_t record_cnt = 0;
	int32_t retV = 0;

	core_vol_num = get_vcore_vol_num();
	core_ip_num = get_vcore_ip_num();

	core_duration_ptr = kcalloc(core_vol_num, sizeof(struct vol_duration), GFP_KERNEL);
	if (core_duration_ptr == NULL) {
		ret = -1;
		goto End;
	}

	core_ip_stats_ptr = kcalloc(core_ip_num, sizeof(struct ip_stats), GFP_KERNEL);
	if (core_ip_stats_ptr == NULL) {
		ret = -1;
		goto End;
	}

	for (i = 0; i < core_ip_num; i++) {
		core_ip_stats_ptr[i].times =
		kzalloc(sizeof(struct ip_times), GFP_KERNEL);
		if (core_ip_stats_ptr[i].times == NULL) {
			pr_notice("times failure\n");
			goto End;
		}
	}

	for (i = 0; i < 2; i++) {
		retV = sync_latest_data();
		if (retV == SWPM_PSP_SUCCESS)
			break;
		pr_notice("%s(%d), (%d) retV(%d) sync latest data again\n",
			__func__, __LINE__, i, retV);
	}

	get_vcore_vol_duration(core_vol_num, core_duration_ptr);
	get_vcore_ip_vol_stats(core_ip_num, core_vol_num, core_ip_stats_ptr);
	get_data_record_number(&record_cnt);

	if (core_vol_num > MAX_VCORE_NUM) {
		pr_notice("core vol num over (%d)", MAX_VCORE_NUM);
		core_vol_check = MAX_VCORE_NUM;
	} else
		core_vol_check = core_vol_num;

	if (core_ip_num > MAX_VCORE_IP_NUM) {
		pr_notice("core_ip_num over (%d)", MAX_VCORE_IP_NUM);
		core_ip_check = MAX_VCORE_IP_NUM;
	} else
		core_ip_check = core_ip_num;

	pmbrainkPowerVcoreInfo->totalVCNum = core_vol_check;
	pmbrainkPowerVcoreInfo->totalVCIpNum = core_ip_check;

	for (i = 0; i < core_vol_check; i++) {
		pmbrainkPowerVcoreInfo->vcoreDurationInfo[i].vol =
			core_duration_ptr[i].vol;
		pmbrainkPowerVcoreInfo->vcoreDurationInfo[i].duration =
			core_duration_ptr[i].duration;
	}

	for (i = 0; i < core_ip_check; i++) {
		strscpy(pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].ip_name,
			core_ip_stats_ptr[i].ip_name,
			MAX_IP_NAME_LENGTH - 1);
		pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].times.active_time =
			core_ip_stats_ptr[i].times->active_time;
		pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].times.idle_time =
			core_ip_stats_ptr[i].times->idle_time;
		pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].times.off_time =
			core_ip_stats_ptr[i].times->off_time;
	}

	pmbrainkPowerVcoreInfo->updateCnt = record_cnt;

End:
	if (core_duration_ptr != NULL)
		kfree(core_duration_ptr);

	if (core_ip_stats_ptr != NULL) {
		for (i = 0; i < core_ip_num; i++) {
			if (core_ip_stats_ptr[i].times != NULL)
				kfree(core_ip_stats_ptr[i].times);
		}
		kfree(core_ip_stats_ptr);
	}

	return ret;
}

static void mbraink_v6993_get_power_wakeup_info(
	struct mbraink_power_wakeup_data *wakeup_info_buffer)
{
	struct wakeup_source *ws = NULL;
	ktime_t total_time;
	ktime_t max_time;
	unsigned long active_count;
	ktime_t active_time;
	ktime_t prevent_sleep_time;
	int lockid = 0;
	unsigned short i = 0;
	unsigned short pos = 0;
	unsigned short count = 5000;

	if (wakeup_info_buffer == NULL)
		return;

	lockid = wakeup_sources_read_lock();

	ws = wakeup_sources_walk_start();
	pos = wakeup_info_buffer->next_pos;
	while (ws && (pos > i) && (count > 0)) {
		ws = wakeup_sources_walk_next(ws);
		i++;
		count--;
	}

	for (i = 0; i < MAX_WAKEUP_SOURCE_NUM; i++) {
		if (ws != NULL) {
			wakeup_info_buffer->next_pos++;

			total_time = ws->total_time;
			max_time = ws->max_time;
			prevent_sleep_time = ws->prevent_sleep_time;
			active_count = ws->active_count;
			if (ws->active) {
				ktime_t now = ktime_get();

				active_time = ktime_sub(now, ws->last_time);
				total_time = ktime_add(total_time, active_time);
				if (active_time > max_time)
					max_time = active_time;

				if (ws->autosleep_enabled) {
					prevent_sleep_time = ktime_add(prevent_sleep_time,
						ktime_sub(now, ws->start_prevent_time));
				}
			} else {
				active_time = 0;
			}

			if (ws->name != NULL) {
				if ((strlen(ws->name) > 0) && (strlen(ws->name) < MAX_NAME_SZ)) {
					memcpy(wakeup_info_buffer->drv_data[i].name,
					ws->name, strlen(ws->name));
				}
			}

			wakeup_info_buffer->drv_data[i].active_count = active_count;
			wakeup_info_buffer->drv_data[i].event_count = ws->event_count;
			wakeup_info_buffer->drv_data[i].wakeup_count = ws->wakeup_count;
			wakeup_info_buffer->drv_data[i].expire_count = ws->expire_count;
			wakeup_info_buffer->drv_data[i].active_time = ktime_to_ms(active_time);
			wakeup_info_buffer->drv_data[i].total_time = ktime_to_ms(total_time);
			wakeup_info_buffer->drv_data[i].max_time = ktime_to_ms(max_time);
			wakeup_info_buffer->drv_data[i].last_time = ktime_to_ms(ws->last_time);
			wakeup_info_buffer->drv_data[i].prevent_sleep_time = ktime_to_ms(
				prevent_sleep_time);

			ws = wakeup_sources_walk_next(ws);
		}
	}
	wakeup_sources_read_unlock(lockid);

	if (ws != NULL)
		wakeup_info_buffer->is_has_data = 1;
	else
		wakeup_info_buffer->is_has_data = 0;
}

#if IS_ENABLED(CONFIG_MTK_SWPM_MODULE)
static int mbraink_v6993_power_get_spm_l1_info(long long *out_spm_l1_array, int spm_l1_size)
{
	int ret = -1;
	int i = 0;
	long long value = 0;
	struct mbraink_sys_res_mbrain_dbg_ops *sys_res_mbrain_ops = NULL;
	unsigned char *spm_l1_data = NULL;
	unsigned int data_size = 0;
	int size = 0;

	if (out_spm_l1_array == NULL)
		return -1;

	if (spm_l1_size != SPM_L1_DATA_NUM)
		return -1;

	sys_res_mbrain_ops = get_mbraink_dbg_ops();
	if (sys_res_mbrain_ops &&
		sys_res_mbrain_ops->get_last_suspend_res_data) {

		data_size = SPM_L1_SZ;
		spm_l1_data = kmalloc(data_size, GFP_KERNEL);
		if (spm_l1_data != NULL) {
			memset(spm_l1_data, 0, data_size);
			if (sys_res_mbrain_ops->get_last_suspend_res_data(spm_l1_data,
					data_size) == 0) {
				size = sizeof(value);
				for (i = 0; i < SPM_L1_DATA_NUM; i++) {
					if ((i*size + size) <= SPM_L1_SZ)
						memcpy(&value, (const void *)(spm_l1_data
							+ i*size), size);
					out_spm_l1_array[i] = value;
				}
				ret = 0;
			}
		}
	}

	if (spm_l1_data != NULL) {
		kfree(spm_l1_data);
		spm_l1_data = NULL;
	}

	return ret;
}

static int mbraink_v6993_power_get_spm_l2_info(struct mbraink_power_spm_l2_info *spm_l2_info)
{
	struct mbraink_sys_res_mbrain_dbg_ops *sys_res_mbrain_ops = NULL;
	uint32_t thr[4];
	unsigned int wpos = 0, wsize = 0;
	unsigned int get_sig = 0;
	bool bfree = false;
	int res_size = (int)sizeof(struct mbraink_sys_res_sig_info);

	if (spm_l2_info == NULL)
		return 0;

	sys_res_mbrain_ops = get_mbraink_dbg_ops();
	if (sys_res_mbrain_ops &&
		sys_res_mbrain_ops->get_over_threshold_num &&
		sys_res_mbrain_ops->get_over_threshold_data) {

		thr[0] = spm_l2_info->value[0];
		thr[1] = spm_l2_info->value[1];
		thr[2] = spm_l2_info->value[2];
		thr[3] = spm_l2_info->value[3];

		memcpy(&get_sig, spm_l2_info->spm_data + 28, sizeof(get_sig));
		if (get_sig == 0) {
			if (g_spm_l2_ls_ptr != NULL) {
				kfree(g_spm_l2_ls_ptr);
				g_spm_l2_ls_ptr = NULL;
			}

			if (g_spm_l2_sig_tbl_ptr != NULL) {
				kfree(g_spm_l2_sig_tbl_ptr);
				g_spm_l2_sig_tbl_ptr = NULL;
			}

			g_spm_l2_ls_ptr = kmalloc(SPM_L2_LS_SZ, GFP_KERNEL);
			if (g_spm_l2_ls_ptr == NULL) {
				bfree = true;
				goto End;
			}

			if (sys_res_mbrain_ops->get_over_threshold_num(g_spm_l2_ls_ptr,
				SPM_L2_LS_SZ, thr, 4) == 0) {
				memcpy(&g_spm_l2_sig_num, g_spm_l2_ls_ptr + 24,
					sizeof(g_spm_l2_sig_num));

				if (g_spm_l2_sig_num > SPM_TOTAL_MAX_SIG_NUM) {
					bfree = true;
					goto End;
				}

				g_spm_l2_sig_tbl_size = g_spm_l2_sig_num*res_size;
				g_spm_l2_sig_tbl_ptr = kmalloc(g_spm_l2_sig_tbl_size, GFP_KERNEL);
				if (g_spm_l2_sig_tbl_ptr == NULL) {
					bfree = true;
					goto End;
				}

				if (sys_res_mbrain_ops->get_over_threshold_data(
					g_spm_l2_sig_tbl_ptr, g_spm_l2_sig_tbl_size) != 0) {
					bfree = true;
					goto End;
				}
			}
		}

		if (g_spm_l2_ls_ptr != NULL && g_spm_l2_sig_tbl_ptr != NULL) {
			memcpy(spm_l2_info->spm_data, g_spm_l2_ls_ptr, SPM_L2_LS_SZ);
			wpos += SPM_L2_LS_SZ;

			if ((wpos + (g_spm_l2_sig_num - get_sig)*res_size) <= SPM_L2_SZ) {
				wsize = (g_spm_l2_sig_num - get_sig)*res_size;
				memcpy(spm_l2_info->spm_data + wpos,
					g_spm_l2_sig_tbl_ptr + get_sig*res_size, wsize);
			} else {
				wsize = (sizeof(spm_l2_info->spm_data) - SPM_L2_LS_SZ)
					/res_size*res_size;
				memcpy(spm_l2_info->spm_data + SPM_L2_LS_SZ, g_spm_l2_sig_tbl_ptr
					+ get_sig*res_size, wsize);
			}
			get_sig += wsize/res_size;
			memcpy(spm_l2_info->spm_data + 28, &get_sig, sizeof(get_sig));
		}
	}
End:

	if (get_sig == g_spm_l2_sig_num || bfree) {
		if (g_spm_l2_ls_ptr != NULL) {
			kfree(g_spm_l2_ls_ptr);
			g_spm_l2_ls_ptr = NULL;
		}

		if (g_spm_l2_sig_tbl_ptr != NULL) {
			kfree(g_spm_l2_sig_tbl_ptr);
			g_spm_l2_sig_tbl_ptr = NULL;
		}
	}

	return 0;
}

static int mbraink_v6993_power_get_spm_info(struct mbraink_power_spm_raw *spm_buffer)
{
	bool bfree = false;
	struct mbraink_sys_res_mbrain_dbg_ops *sys_res_mbrain_ops = NULL;
	int bufIdx = 0;
	int size = 0;

	if (spm_buffer == NULL) {
		bfree = true;
		goto End;
	}

	if (spm_buffer->type == 1) {
		sys_res_mbrain_ops = get_mbraink_dbg_ops();

		if (sys_res_mbrain_ops && sys_res_mbrain_ops->get_length) {
			g_data_size = sys_res_mbrain_ops->get_length();
			g_data_size += MAX_POWER_HD_SZ;
			pr_notice("g_data_size(%d)\n", g_data_size);
		}

		if (g_data_size == 0) {
			bfree = true;
			goto End;
		}

		if (g_spm_raw != NULL) {
			vfree(g_spm_raw);
			g_spm_raw = NULL;
		}

		if (g_data_size <= SPM_TOTAL_MAX_SIG_NUM*sizeof(struct mbraink_sys_res_sig_info)
			*MBRAINK_SCENE_RELEASE_NUM) {
			g_spm_raw = vmalloc(g_data_size);
			if (g_spm_raw != NULL) {
				memset(g_spm_raw, 0, g_data_size);

				if (sys_res_mbrain_ops && sys_res_mbrain_ops->get_data &&
					sys_res_mbrain_ops->get_data(g_spm_raw, g_data_size) != 0) {
					bfree = true;
					goto End;
				}
			}
		}
	}

	if (g_spm_raw != NULL) {
		if (spm_buffer->type == 1) {
			size = sizeof(struct mbraink_sys_res_mbrain_header);
			bufIdx = 0;

			if (((bufIdx + size) <= g_data_size) &&
				(size <= sizeof(spm_buffer->spm_data))) {
				memcpy(spm_buffer->spm_data + bufIdx, g_spm_raw, size);
				bufIdx += size;
			}

			size = sizeof(struct mbraink_sys_res_scene_info)*MBRAINK_SCENE_RELEASE_NUM;
			if (((bufIdx + size) <= g_data_size) &&
				(size <= sizeof(spm_buffer->spm_data))) {
				memcpy(spm_buffer->spm_data + bufIdx, g_spm_raw + bufIdx, size);
				bufIdx += size;
			}
		} else {
			if (((spm_buffer->pos + spm_buffer->size) > (g_data_size)) ||
			spm_buffer->size > sizeof(spm_buffer->spm_data)) {
				bfree = true;
				goto End;
			}
			memcpy(spm_buffer->spm_data, g_spm_raw + spm_buffer->pos, spm_buffer->size);

			if (spm_buffer->type == 0) {
				bfree = true;
				goto End;
			}
		}
	}

End:
	if (bfree == true) {
		if (g_spm_raw != NULL) {
			vfree(g_spm_raw);
			g_spm_raw = NULL;
		}
		g_data_size = 0;
	}

	return 0;
}
#else

static int mbraink_v6993_power_get_spm_l1_info(long long *out_spm_l1_array, int spm_l1_size)
{
	pr_info("[Mbraink][SPM][%s] SWPM not support\n", __func__);
}

static int mbraink_v6993_power_get_spm_l2_info(struct mbraink_power_spm_l2_info *spm_l2_info)
{
	pr_info("[Mbraink][SPM][%s] SWPM not support\n", __func__);
}

static int mbraink_v6993_power_get_spm_info(struct mbraink_power_spm_raw *spm_buffer)
{
	pr_info("[Mbraink][SPM][%s] SWPM not support\n", __func__);
}

#endif

static int mbraink_v6993_power_get_scp_info(struct mbraink_power_scp_info *scp_info)
{
	struct scp_res_mbrain_dbg_ops *scp_res_mbrain_ops = NULL;
	unsigned int data_size = 0;
	unsigned char *ptr = NULL;

	if (scp_info == NULL)
		return -1;

	scp_res_mbrain_ops = get_scp_mbrain_dbg_ops();

	if (scp_res_mbrain_ops &&
		scp_res_mbrain_ops->get_length &&
		scp_res_mbrain_ops->get_data) {

		data_size = scp_res_mbrain_ops->get_length();

		if (data_size > 0 && data_size <= sizeof(scp_info->scp_data)) {
			ptr = kmalloc(data_size, GFP_KERNEL);
			if (ptr == NULL)
				goto End;

			scp_res_mbrain_ops->get_data(ptr, data_size);
			if (data_size <= sizeof(scp_info->scp_data))
				memcpy(scp_info->scp_data, ptr, data_size);

		} else {
			goto End;
		}
	}

End:
	if (ptr != NULL) {
		kfree(ptr);
		ptr = NULL;
	}

	return 0;
}

static int mbraink_v6993_power_get_scp_task_info(struct mbraink_power_scp_task_info *scp_task_info)
{
	unsigned char *g_scp_raw = NULL;
	unsigned int data_size = 0;
	struct scp_res_mbrain_dbg_ops *scp_tmon_mbrain_ops = NULL;

	if (scp_task_info == NULL)
		return -1;

	scp_tmon_mbrain_ops = get_scp_mbrain_tmon_ops();
	if (scp_tmon_mbrain_ops) {

		data_size = scp_tmon_mbrain_ops->get_length();
		data_size += DATA_HD_SZ;
		pr_notice("[Mbraink][SCP] scp task data size=(%d)\n", data_size);

		if (data_size && (data_size < (SCP_TASK_SZ * 20) + DATA_HD_SZ)) {
			g_scp_raw = vmalloc(data_size);
			if (g_scp_raw) {
				memset(g_scp_raw, 0, data_size);
				if (scp_tmon_mbrain_ops->get_data(g_scp_raw, data_size) == 0) {
					if (((scp_task_info->pos + scp_task_info->size)
						<= data_size) && scp_task_info->size <= sizeof(
						scp_task_info->scp_task_data)) {
						memcpy(scp_task_info->scp_task_data, g_scp_raw
							+ scp_task_info->pos, scp_task_info->size);
					} else
						pr_info("[Mbraink][SCP] scp tmon get data fail\n");
				}
				vfree(g_scp_raw);

			} else {
				pr_notice("[Mbraink][SCP] malloc fail\n");
			}
		}
	}

	return 0;
}

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
static int mbraink_v6993_power_get_modem_info(struct mbraink_modem_raw *modem_buffer)
{
	int shm_size = 0;
	void __iomem *shm_addr = NULL;
	unsigned char *base_addr = NULL;
	unsigned char *read_addr = NULL;
	int i = 0;
	unsigned int mem_status = 0;
	unsigned int read_blk_idx = 0;
	unsigned int offset = 0;
	bool ret = true;

	if (modem_buffer == NULL)
		return 0;

	shm_addr = get_smem_start_addr(SMEM_USER_32K_LOW_POWER, &shm_size);
	if (shm_addr == NULL) {
		pr_notice("get_smem_start_addr addr is null\n");
		return 0;
	}

	if (shm_size == 0 || MD_MAX_SZ > shm_size) {
		pr_notice("get_smem_start_addr size(%d) is incorrect\n", shm_size);
		return 0;
	}

	base_addr = (unsigned char *)shm_addr;

	if (modem_buffer->type == 0) {
		read_addr = base_addr;
		memcpy(modem_buffer->data1, read_addr, MD_HD_SZ);
		read_addr = base_addr + MD_HD_SZ;
		memcpy(modem_buffer->data2, read_addr, MD_MDHD_SZ);

		if (modem_buffer->data1[0] != 1 ||  modem_buffer->data1[2] != 8) {
			modem_buffer->is_has_data = 0;
			modem_buffer->count = 0;
			return 0;
		}

		g_md_read_count = 0;
		g_md_last_read_blk_idx = g_md_last_has_data_blk_idx;

		pr_notice("g_md_last_read_blk_idx(%d)", g_md_last_read_blk_idx);
	}

	read_blk_idx = g_md_last_read_blk_idx;
	i = 0;
	ret = true;
	while ((g_md_read_count < MD_BLK_MAX_NUM) && (i < MD_SECBLK_NUM)) {
		offset = MD_HD_SZ + MD_MDHD_SZ + read_blk_idx*MD_BLK_SZ;
		if (offset > shm_size) {
			ret = false;
			break;
		}
		read_addr = base_addr + offset;
		memcpy(&mem_status, read_addr, sizeof(mem_status));

		read_blk_idx = (read_blk_idx + 1) % MD_BLK_MAX_NUM;
		g_md_read_count++;

		if (mem_status == MD_STATUS_W_DONE) {
			offset = i*MD_BLK_SZ;
			if ((offset + MD_BLK_SZ) > sizeof(modem_buffer->data3)) {
				ret = false;
				break;
			}

			memcpy(modem_buffer->data3 + offset, read_addr, MD_BLK_SZ);
			//reset mem_status mem_count after read data
			mem_status = MD_STATUS_R_DONE;
			memcpy(read_addr, &mem_status, sizeof(mem_status));
			memset(read_addr + sizeof(mem_status), 0, 4); //mem_count
			i++;
			g_md_last_has_data_blk_idx = read_blk_idx;
		}
	}

	if ((g_md_read_count < MD_BLK_MAX_NUM) && (ret == true))
		modem_buffer->is_has_data = 1;
	else
		modem_buffer->is_has_data = 0;

	g_md_last_read_blk_idx = read_blk_idx;
	modem_buffer->count = i;

	return 0;
}

#else

static int mbraink_v6993_power_get_modem_info(struct mbraink_modem_raw *modem_buffer)
{
	pr_notice("not support eccci modem interface\n");
	return 0;
}

#endif

static int mbraink_v6993_power_get_spmi_info(
	struct mbraink_spmi_struct_data *mbraink_spmi_data)
{
	unsigned int nackBuf[MAX_SPMI_NACK_CNT] = {0};
	u16 parityErrBuf[MAX_SPMI_PARITY_ERR_CNT] = {0};
	u16 preOtBuf[MAX_SPMI_PRE_OT_CNT] = {0};
	u16 preLvsysBuf[MAX_SPMI_PRE_LVSYS_CNT] = {0};
	u16 currClampingBuf[MAX_SPMI_CURR_CLAMPING_CNT] = {0};
	int ret = 0;
	int num = 0;
	unsigned int rg[MAX_SPMI_RG_CNT] = {0};
	u16 crcBuf[MAZ_SPMI_CRC_ERR_CNT] = {0};

	if (mbraink_spmi_data == NULL) {
		pr_info("mbraink_spmi_data is null\n");
		return -1;
	}

	//get nack cnt
	get_spmi_slvid_nack_cnt(nackBuf);
	num = (MAX_PMIC_SPMI_SZ > MAX_SPMI_NACK_CNT) ?
		MAX_SPMI_NACK_CNT : MAX_PMIC_SPMI_SZ;
	memcpy(mbraink_spmi_data->spmi, nackBuf, sizeof(unsigned int)*num);
	mbraink_spmi_data->spmi_count = num;

	//get parity err cnt
	mtk_spmi_pmic_get_parity_err_cnt(parityErrBuf);
	num = (MAX_SPMI_PARITY_ERR_SZ > MAX_SPMI_PARITY_ERR_CNT) ?
		MAX_SPMI_PARITY_ERR_CNT : MAX_SPMI_PARITY_ERR_SZ;
	memcpy(mbraink_spmi_data->spmi_parity_err, parityErrBuf, sizeof(u16)*num);
	mbraink_spmi_data->spmi_parity_err_count = num;

	//get pre ot cnt
	mtk_spmi_pmic_get_pre_ot_cnt(preOtBuf);
	num = (MAX_SPMI_PRE_OT_SZ > MAX_SPMI_PRE_OT_CNT) ?
		MAX_SPMI_PRE_OT_CNT : MAX_SPMI_PRE_OT_SZ;
	memcpy(mbraink_spmi_data->spmi_pre_ot, preOtBuf, sizeof(u16)*num);
	mbraink_spmi_data->spmi_pre_ot_count = num;

	//get pre lvsys cnt
	mtk_spmi_pmic_get_pre_lvsys_cnt(preLvsysBuf);
	num = (MAX_SPMI_PRE_LVSYS_SZ > MAX_SPMI_PRE_LVSYS_CNT) ?
		MAX_SPMI_PRE_LVSYS_CNT : MAX_SPMI_PRE_LVSYS_SZ;
	memcpy(mbraink_spmi_data->spmi_pre_lvsys, preLvsysBuf, sizeof(u16)*num);
	mbraink_spmi_data->spmi_pre_lvsys_count = num;

	//get current clamping cnt
	mtk_spmi_pmic_get_current_clamping_cnt(currClampingBuf);
	num = (MAX_SPMI_CURR_CLAMPING_SZ > MAX_SPMI_CURR_CLAMPING_CNT) ?
		MAX_SPMI_CURR_CLAMPING_CNT : MAX_SPMI_CURR_CLAMPING_SZ;
	memcpy(mbraink_spmi_data->spmi_curr_clamping, currClampingBuf, sizeof(u16)*num);
	mbraink_spmi_data->spmi_curr_clamping_count = num;

	//get rg info
	mtk_spmi_pmic_get_debug_rg_info(rg);
	num = (MAX_SPMI_RG_SZ > MAX_SPMI_RG_CNT) ?
		MAX_SPMI_RG_CNT : MAX_SPMI_RG_SZ;
	memcpy(mbraink_spmi_data->spmi_rg, rg, sizeof(unsigned int)*num);
	mbraink_spmi_data->spmi_rg_count = num;

	//get CRC info
	mtk_spmi_pmic_get_crc_err_cnt(crcBuf);
	num = (MAX_SPMI_CRC_ERR_SZ > MAZ_SPMI_CRC_ERR_CNT) ?
		MAZ_SPMI_CRC_ERR_CNT : MAX_SPMI_CRC_ERR_SZ;
	memcpy(mbraink_spmi_data->spmi_crc_err, crcBuf, sizeof(u16)*num);
	mbraink_spmi_data->spmi_crc_err_count = num;

	return ret;
}

static int mbraink_v6993_power_get_pmic_voltage_info(
	struct mbraink_pmic_voltage_info *pmicVoltageInfo)
{
	unsigned int vcore = 0;
	unsigned int vsram_core = 0;
	int ret = 0;
	int err = 0;
	struct regulator *reg_vcore;
	struct regulator *reg_vsram_core;

	if (pMBrainkPlatDev == NULL) {
		pr_info("get mbraink platform device is null");
		return -1;
	}

	if (pmicVoltageInfo == NULL) {
		pr_info("get pmicVoltageInfo is null");
		return -1;
	}

	reg_vcore = devm_regulator_get_optional(pMBrainkPlatDev, "vcore");
	if (IS_ERR(reg_vcore)) {
		err = PTR_ERR(reg_vcore);
		pr_notice("Failed to get 'vcore' regulator: %d\n", err);
	} else {
		ret = regulator_get_voltage(reg_vcore);
		if (ret > 0) {
			vcore = ret;
			pr_info("get vcore: %d", vcore);
		} else {
			pr_info("failed to get vcore: %d", vcore);
		}
		regulator_put(reg_vcore);
	}

	reg_vsram_core = devm_regulator_get_optional(pMBrainkPlatDev, "vsram");
	if (IS_ERR(reg_vsram_core)) {
		err = PTR_ERR(reg_vsram_core);
		pr_notice("Failed to get 'vsram' regulator: %d\n", err);
	} else {
		ret = regulator_get_voltage(reg_vsram_core);
		if (ret > 0) {
			vsram_core = ret;
			pr_info("get vsram_core: %d", vsram_core);
		} else {
			pr_info("failed to get vsram_core: %d", vsram_core);
		}
		regulator_put(reg_vsram_core);
	}

	pmicVoltageInfo->vcore = vcore;
	pmicVoltageInfo->vsram_core = vsram_core;

	return 0;
}

static int mbraink_v6993_power_sys_res_init(void)
{
	int ret = 0;

	ret = mbraink_sys_res_plat_init();
	if (!ret)
		ret = mbraink_sys_res_mbrain_plat_init();
	else
		return -1;

	return 0;
}

static int mbraink_v6993_power_sys_res_deinit(void)
{
	mbraink_sys_res_plat_deinit();
	mbraink_sys_res_mbrain_plat_deinit();

	return 0;
}

static void mbraink_v6993_power_send_spml1(long long last_resume_timestamp)
{
	unsigned long flags;
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;

	spin_lock_irqsave(&spm_l1_lock, flags);

	if (g_spm_l1_data[0] == 0 && g_spm_l1_data[1] == 0 && g_spm_l1_data[2] == 0 &&
		g_spm_l1_data[3] == 0) {
		spin_unlock_irqrestore(&spm_l1_lock, flags);
		return;
	}

	n = snprintf(netlink_buf, NETLINK_EVENT_MESSAGE_SIZE,
		"%s %lld:%lld:%lld:%lld:%lld:%lld:%lld:%lld:%lld:%lld:%lld:%lld:%lld:%lld:%lld",
		NETLINK_EVENT_SYSNOTIFIER_PS,
		last_resume_timestamp,
		g_spm_l1_data[0],
		g_spm_l1_data[1],
		g_spm_l1_data[2],
		g_spm_l1_data[3],
		g_spm_l1_data[4],
		g_spm_l1_data[5],
		g_spm_l1_data[6],
		g_spm_l1_data[7],
		g_spm_l1_data[8],
		g_spm_l1_data[9],
		g_spm_l1_data[10],
		g_spm_l1_data[11],
		g_spm_l1_data[12],
		g_spm_l1_data[13]
	);

	if (n < 0 || n > NETLINK_EVENT_MESSAGE_SIZE)
		pr_info("%s : snprintf error n = %d\n", __func__, n);
	else
		mbraink_netlink_send_msg(netlink_buf);

	spin_unlock_irqrestore(&spm_l1_lock, flags);
}

static void mbraink_v6993_power_suspend_prepare(void)
{
	unsigned long flags;

	spin_lock_irqsave(&spm_l1_lock, flags);
	memset(g_spm_l1_data, 0, sizeof(g_spm_l1_data));
	spin_unlock_irqrestore(&spm_l1_lock, flags);
}

static void mbraink_v6993_power_post_suspend(long long last_resume_timestamp)
{
	mbraink_v6993_power_send_spml1(last_resume_timestamp);
}

static int mbraink_v6993_power_device_suspend(struct device *dev)
{
	int ret = 0;
	struct mbraink_sys_res_mbrain_dbg_ops *sys_res_mbrain_ops = NULL;

	sys_res_mbrain_ops = get_mbraink_dbg_ops();

	if (sys_res_mbrain_ops && sys_res_mbrain_ops->update) {
		ret = sys_res_mbrain_ops->update();
		if (ret != 0)
			pr_info("suspend_prepare mbraink update sys res fail");
	}
	return ret;
}

static int mbraink_v6993_power_device_resume(struct device *dev)
{
	unsigned long flags;
	int ret;
	long long spm_l1_data[SPM_L1_DATA_NUM];

	memset(spm_l1_data, 0, sizeof(spm_l1_data));
	ret = mbraink_v6993_power_get_spm_l1_info(spm_l1_data, SPM_L1_DATA_NUM);

	spin_lock_irqsave(&spm_l1_lock, flags);
	memcpy(g_spm_l1_data, spm_l1_data, sizeof(spm_l1_data));
	spin_unlock_irqrestore(&spm_l1_lock, flags);

	return ret;
}

static int mbraink_v6993_power_get_mmdfvs_info(struct mbraink_mmdvfs_info *mmdvfsInfo)
{
	unsigned int mmdvfs_data_size = 0;
	int ret = 0;
	struct mmdvfs_res_mbrain_debug_ops *res_mbrain_debug_ops = NULL;

	if (mmdvfsInfo == NULL)
		return -1;

	res_mbrain_debug_ops = get_mmdvfs_mbrain_dbg_ops();

	if (res_mbrain_debug_ops &&
		res_mbrain_debug_ops->get_length &&
		res_mbrain_debug_ops->get_data) {

		mmdvfs_data_size = res_mbrain_debug_ops->get_length();
		if (mmdvfs_data_size <= MMDVFS_TOTAL_SZ) {
			mmdvfsInfo->size = mmdvfs_data_size;
			ret = res_mbrain_debug_ops->get_data(mmdvfsInfo->mmdvfs_data, mmdvfsInfo->size);
			pr_info("mmdvfs opp, size: %d", mmdvfsInfo->size);
		} else {
			pr_info("incorrect size: %d for mmdvfs opp", mmdvfsInfo->size);
			ret = -1;
		}
	} else {
		pr_info("failed to get mmdvfs opp");
		ret = -1;
	}

	return ret;
}

static int mbraink_v6993_power_get_mmdvfs_user_info(struct mbraink_mmdvfs_user_info *mmdvfs_user_info)
{
	unsigned int mmdvfs_data_size = 0;
	int ret = 0;
	struct mmdvfs_res_mbrain_debug_ops *res_mbrain_debug_ops = NULL;

	if (mmdvfs_user_info == NULL)
		return -1;

	res_mbrain_debug_ops = get_mmdvfs_mbrain_usr_dbg_ops();

	if (res_mbrain_debug_ops &&
		res_mbrain_debug_ops->get_length &&
		res_mbrain_debug_ops->get_data) {

		mmdvfs_data_size = res_mbrain_debug_ops->get_length();
		if (mmdvfs_data_size <= MMDVFS_USER_TOTAL_SZ) {
			mmdvfs_user_info->size = mmdvfs_data_size;
			ret = res_mbrain_debug_ops->get_data(
				mmdvfs_user_info->mmdvfs_user_data,
				mmdvfs_user_info->size
			);
			pr_info("mmdvfs opp, size: %d", mmdvfs_user_info->size);
		} else {
			pr_info("incorrect size: %d for mmdvfs opp", mmdvfs_user_info->size);
			ret = -1;
		}
	} else {
		pr_info("failed to get mmdvfs opp");
		ret = -1;
	}

	return ret;
}

void pt2mbrain_hint_low_battery_volt_throttle(struct lbat_mbrain lbat_mbrain)
{
	int user = lbat_mbrain.user;
	unsigned int thd_volt = lbat_mbrain.thd_volt;
	unsigned int level = lbat_mbrain.level;
	unsigned int soc = lbat_mbrain.soc;
	unsigned int bat_temp = lbat_mbrain.bat_temp;
	unsigned int temp_stage = lbat_mbrain.temp_stage;

	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;

	struct timespec64 tv = { 0 };
	long long timestamp = 0;

	ktime_get_real_ts64(&tv);
	timestamp = (tv.tv_sec*1000)+(tv.tv_nsec/1000000);
	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s:%llu:%d:%d:%d:%d:%d:%d",
			NETLINK_EVENT_LOW_BATTERY_VOLTAGE_THROTTLE,
			timestamp,
			user,
			thd_volt,
			level,
			soc,
			bat_temp,
			temp_stage);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return;
	mbraink_netlink_send_msg(netlink_buf);
}

void pt2mbrain_hint_battery_oc_throttle(struct battery_oc_mbrain bat_oc_mbrain)
{
	unsigned int level = bat_oc_mbrain.level;

	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;

	struct timespec64 tv = { 0 };
	long long timestamp = 0;

	ktime_get_real_ts64(&tv);
	timestamp = (tv.tv_sec*1000)+(tv.tv_nsec/1000000);
	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s:%llu:%d",
			NETLINK_EVENT_BATTERY_OVER_CURRENT_THROTTLE,
			timestamp,
			level);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return;
	mbraink_netlink_send_msg(netlink_buf);
}

void pt2mbrain_ppb_notify_func(void)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;

	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s",
			NETLINK_EVENT_PPB_NOTIFY);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return;
	mbraink_netlink_send_msg(netlink_buf);
}

static int mbraink_v6993_power_get_power_throttle_hw_info
	(struct mbraink_power_throttle_hw_data *power_throttle_hw_data)
{
	int ret = 0;
	struct ppb_mbrain_data *res_ppb_mbrain_data = NULL;

	res_ppb_mbrain_data = kzalloc(sizeof(struct ppb_mbrain_data), GFP_KERNEL);
	if (!res_ppb_mbrain_data) {
		ret = -ENOMEM;
		goto End;
	}

	memset(res_ppb_mbrain_data, 0x00, sizeof(struct ppb_mbrain_data));
	ret = get_ppb_mbrain_data(res_ppb_mbrain_data);

	power_throttle_hw_data->kernel_time = res_ppb_mbrain_data->kernel_time;
	power_throttle_hw_data->duration = res_ppb_mbrain_data->duration;
	power_throttle_hw_data->soc = res_ppb_mbrain_data->soc;
	power_throttle_hw_data->temp = res_ppb_mbrain_data->temp;
	power_throttle_hw_data->soc_rdc = res_ppb_mbrain_data->soc_rdc;
	power_throttle_hw_data->soc_rac = res_ppb_mbrain_data->soc_rac;
	power_throttle_hw_data->hpt_bat_budget = res_ppb_mbrain_data->hpt_bat_budget;
	power_throttle_hw_data->hpt_cg_budget = res_ppb_mbrain_data->hpt_cg_budget;
	power_throttle_hw_data->ppb_cg_budget = res_ppb_mbrain_data->ppb_cg_budget;
	power_throttle_hw_data->hpt_cpub_thr_cnt = res_ppb_mbrain_data->hpt_cpub_thr_cnt;
	power_throttle_hw_data->hpt_cpub_thr_time = res_ppb_mbrain_data->hpt_cpub_thr_time;
	power_throttle_hw_data->hpt_cpum_thr_cnt = res_ppb_mbrain_data->hpt_cpum_thr_cnt;
	power_throttle_hw_data->hpt_cpum_thr_time = res_ppb_mbrain_data->hpt_cpum_thr_time;
	power_throttle_hw_data->hpt_gpu_thr_cnt = res_ppb_mbrain_data->hpt_gpu_thr_cnt;
	power_throttle_hw_data->hpt_gpu_thr_time = res_ppb_mbrain_data->hpt_gpu_thr_time;
	power_throttle_hw_data->hpt_cpub_sf = res_ppb_mbrain_data->hpt_cpub_sf;
	power_throttle_hw_data->hpt_cpum_sf = res_ppb_mbrain_data->hpt_cpum_sf;
	power_throttle_hw_data->hpt_gpu_sf = res_ppb_mbrain_data->hpt_gpu_sf;
	power_throttle_hw_data->ppb_combo = res_ppb_mbrain_data->ppb_combo;
	power_throttle_hw_data->ppb_c_combo0 = res_ppb_mbrain_data->ppb_c_combo0;
	power_throttle_hw_data->ppb_g_combo0 = res_ppb_mbrain_data->ppb_g_combo0;
	power_throttle_hw_data->ppb_g_flavor = res_ppb_mbrain_data->ppb_g_flavor;

End:
	if (res_ppb_mbrain_data != NULL)
		kfree(res_ppb_mbrain_data);

	return ret;
}

static int mbraink_v6993_power_get_lpmstate_info(struct mbraink_lpm_state_data *lpmStateInfo)
{
	int ret = 0;
	struct lpm_dbg_lp_info lpm_info;
	int state_size = 0;
	int i = 0;

	pr_info("(%s)(%d)", __func__, __LINE__);
	if (lpmStateInfo == NULL)
		return -1;

	memset(&lpm_info, 0x00, sizeof(struct lpm_dbg_lp_info));
	lpm_get_suspend_event_info(&lpm_info);

	lpmStateInfo->state_num = NUM_SPM_STAT;

	if (NUM_SPM_STAT > MAX_LPM_STATE_NUM)
		state_size = MAX_LPM_STATE_NUM;
	else
		state_size = NUM_SPM_STAT;

	for (i = 0; i < state_size; i++) {
		lpmStateInfo->lpm_state_info[i].count = lpm_info.record[i].count;
		lpmStateInfo->lpm_state_info[i].duration = lpm_info.record[i].duration;
	}

	return ret;
}

static int mbraink_v6993_power_get_spmi_glitch_info(
	struct mbraink_spmi_glitch_struct_data *mbraink_spmi_glitch_data)
{
	u16 Buf[MAX_SPMI_GLITCH_ID] = {0};
	int ret = 0;
	int num = 0;

	if (mbraink_spmi_glitch_data == NULL) {
		pr_info("mbraink_spmi_glitch_data is null\n");
		return -1;
	}

	mtk_spmi_pmic_get_glitch_cnt(Buf);
	num = (MAX_PMIC_SPMI_GLITCH_SZ > MAX_SPMI_GLITCH_ID) ?
		MAX_SPMI_GLITCH_ID : MAX_PMIC_SPMI_GLITCH_SZ;

	memcpy(mbraink_spmi_glitch_data->spmi_glitch, Buf, sizeof(u16)*num);
	mbraink_spmi_glitch_data->spmi_glitch_count = num;

	return ret;
}

static int mbraink_v6993_power_get_dvfsrc_info(
	struct mbraink_dvfsrc_struct_data *mbraink_dvfsrc_data)
{
	struct mtk_dvfsrc_dvfs_info_header dvfsrcInfoHeader;
	int ret = 0;
	int num = 0;

	if (mbraink_dvfsrc_data == NULL) {
		pr_info("mbraink_dvfsrc_data is null\n");
		return -1;
	}

	memset(&dvfsrcInfoHeader, 0x00, sizeof(struct mtk_dvfsrc_dvfs_info_header));
	dvfsrc_get_dvfs_info(&dvfsrcInfoHeader);
	num = (MAX_DVFSRC_INFO_SZ > MAX_DVFSRC_NUM) ?
		MAX_DVFSRC_NUM : MAX_DVFSRC_INFO_SZ;
	memcpy(mbraink_dvfsrc_data->dvfsrc_info,
		dvfsrcInfoHeader.dvfs_info_val, sizeof(unsigned int)*num);
	mbraink_dvfsrc_data->dvfsrc_size = num;
	mbraink_dvfsrc_data->version = dvfsrcInfoHeader.dvfs_info_version;

	return ret;
}

static int mbraink_v6993_power_get_smap_info
	(struct mbraink_power_smap_info *smap_info)
{
	int ret = 0;
	struct smap_mbrain smap_mbrain_data;

	if (smap_info == NULL)
		return -1;

	memset(&smap_mbrain_data, 0x00, sizeof(struct smap_mbrain));
	ret = get_smap_mbrain_data(&smap_mbrain_data);
	if (ret < 0) {
		pr_info("failed to get smap info");
		return ret;
	}

	smap_info->version = 3;
	smap_info->chipid = smap_mbrain_data.chipid;
	smap_info->cnt = smap_mbrain_data.cnt;
	smap_info->type = smap_mbrain_data.type;
	smap_info->enable = smap_mbrain_data.enable;
	smap_info->dect_cnt = smap_mbrain_data.dect_cnt;
	smap_info->temp_cnt = smap_mbrain_data.temp_cnt;
	smap_info->sys_time = smap_mbrain_data.sys_time;
	smap_info->dect_result = smap_mbrain_data.dect_result;
	smap_info->dyn_base = smap_mbrain_data.dyn_base;
	smap_info->cg_subsys_dyn = smap_mbrain_data.cg_subsys_dyn;
	smap_info->cg_ratio = smap_mbrain_data.cg_ratio;
	smap_info->dram0_smap_snapshot = smap_mbrain_data.dram0_smap_snapshot;
	smap_info->dram1_smap_snapshot = smap_mbrain_data.dram1_smap_snapshot;
	smap_info->dram2_smap_snapshot = smap_mbrain_data.dram2_smap_snapshot;
	smap_info->dram3_smap_snapshot = smap_mbrain_data.dram3_smap_snapshot;
	smap_info->chinf0_smap_snapshot = smap_mbrain_data.chinf0_smap_snapshot;
	smap_info->chinf1_smap_snapshot = smap_mbrain_data.chinf1_smap_snapshot;
	smap_info->venc0_smap_snapshot = smap_mbrain_data.venc0_smap_snapshot;
	smap_info->venc1_smap_snapshot = smap_mbrain_data.venc1_smap_snapshot;
	smap_info->venc2_smap_snapshot = smap_mbrain_data.venc2_smap_snapshot;
	smap_info->emi_snapshot = smap_mbrain_data.emi_snapshot;
	smap_info->emi_s_snapshot = smap_mbrain_data.emi_s_snapshot;
	smap_info->zram_snapshot = smap_mbrain_data.zram_snapshot;
	smap_info->apu_snapshot = smap_mbrain_data.apu_snapshot;
	smap_info->real_time_start = smap_mbrain_data.real_time_start;
	smap_info->real_time_end = smap_mbrain_data.real_time_end;
	smap_info->dump_cnt = smap_mbrain_data.dump_cnt;
	smap_info->mitigation_cnt = smap_mbrain_data.mitigation_cnt;
	smap_info->mitigation_rate = smap_mbrain_data.mitigation_rate;

	return ret;
}

void smap2mbrain_notify(struct smap_mbrain *smap_mbrain_data)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;

	n = snprintf(netlink_buf + pos,
		NETLINK_EVENT_MESSAGE_SIZE - pos,
		"%s",
		NETLINK_EVENT_SMAP_NOTIFY);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return;

	pr_info("%s(%d) [%s]\n", __func__, __LINE__, netlink_buf);

	mbraink_netlink_send_msg(netlink_buf);
}

int mbraink_v6993_power_ccci_event_cb(enum CCCI_MBRAIN_EVENT_TYPE event_type,
	void *ccci_mbrain_data)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n __maybe_unused = 0;
	struct fsm_poll_data *ccci_raw = NULL;

	if (!ccci_mbrain_data) {
		pr_info("[%s] ccci_mbrain_data is null\n", __func__);
		return -1;
	}

	ccci_raw = (struct fsm_poll_data *)ccci_mbrain_data;

	n = snprintf(netlink_buf, NETLINK_EVENT_MESSAGE_SIZE,
		"%s:%d:%d:%s:%llu:%d",
		NETLINK_EVENT_CCCI_NOTIFY,
		(unsigned int)event_type,
		(unsigned int)ccci_raw->version,
		ccci_raw->key_info,
		(unsigned long long)ccci_raw->time_stamp,
		(unsigned int)ccci_raw->cost_time
	);

	if (n < 0 || n > NETLINK_EVENT_MESSAGE_SIZE)
		pr_info("%s : snprintf error n = %d\n", __func__, n);
	else
		mbraink_netlink_send_msg(netlink_buf);

	pr_info("[%s] netlink_buf(%s)\n", __func__, netlink_buf);

	return 0;
}

static int mbraink_v6993_power_get_mmqos_bw_info(struct mbraink_mmqos_bw_info *mmqos_bw_info)
{
	int ret = 0;
	struct MM_bwData *mmqos_bw_data = NULL;

	if (mmqos_bw_info == NULL)
		return -1;

	mmqos_bw_data = get_mm_bw_data_for_mbrain();
	if (mmqos_bw_data) {
		for (int i = 0; i < MAX_SUBSYS_NUMS; i++) {
			mmqos_bw_info->mmqos_bw_data[i].sid = mmqos_bw_data[i].sid;
			mmqos_bw_info->mmqos_bw_data[i].totalHRT = mmqos_bw_data[i].totalHRT;
			mmqos_bw_info->mmqos_bw_data[i].totalSRT = mmqos_bw_data[i].totalSRT;
			mmqos_bw_info->mmqos_bw_data[i].totalEMIHRT = mmqos_bw_data[i].totalEMIHRT;
			mmqos_bw_info->mmqos_bw_data[i].totalEMISRT = mmqos_bw_data[i].totalEMISRT;
			for (int j = 0; j < MAX_BW_VALUE_NUMS; j++)
				mmqos_bw_info->mmqos_bw_data[i].mmpc_chan_bw[j].bw =
				mmqos_bw_data[i].mmpc_chan_bw[j].bw;
		}
	} else {
		pr_info("failed to get mm bw");
		ret = -1;
	}

	return ret;
}

void pt2mbrain_hpt_notify_func(void)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;

	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s",
			NETLINK_EVENT_HPT_NOTIFY);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return;

	pr_info("[%s] send (%s)\n", __func__, netlink_buf);
	mbraink_netlink_send_msg(netlink_buf);
}

static int mbraink_v6993_power_get_power_throttle_hw_oc_info
	(struct mbraink_power_throttle_hw_oc_data *pt_hw_oc_data)
{
	int ret = 0;
	struct hpt_mbrain_data  res_hpt_mbrain_data;

	if (pt_hw_oc_data == NULL)
		return -1;

	memset(&res_hpt_mbrain_data, 0x00, sizeof(struct hpt_mbrain_data));
	ret = get_hpt_mbrain_data(&res_hpt_mbrain_data);
	if (ret < 0) {
		pr_info("failed to get pt ot info");
		return ret;
	}

	pt_hw_oc_data->version = 1;
	pt_hw_oc_data->oc_count = res_hpt_mbrain_data.oc_count;
	pt_hw_oc_data->oc_duration_us = res_hpt_mbrain_data.oc_duration_us;
	pr_info("[%s] oc_count(%d), oc_duration_us(%d)",
		__func__,
		pt_hw_oc_data->oc_count, pt_hw_oc_data->oc_duration_us);

	return ret;
}

static struct mbraink_power_ops mbraink_v6993_power_ops = {
	.getVotingInfo = NULL,
	.getPowerInfo = NULL,
	.getVcoreInfo = mbraink_v6993_power_getVcoreInfo,
	.getWakeupInfo = mbraink_v6993_get_power_wakeup_info,
	.getSpmInfo = mbraink_v6993_power_get_spm_info,
	.getSpmL1Info = mbraink_v6993_power_get_spm_l1_info,
	.getSpmL2Info = mbraink_v6993_power_get_spm_l2_info,
	.getScpInfo = mbraink_v6993_power_get_scp_info,
	.getScpTaskInfo = mbraink_v6993_power_get_scp_task_info,
	.getModemInfo = mbraink_v6993_power_get_modem_info,
	.getSpmiInfo = mbraink_v6993_power_get_spmi_info,
	.getUvloInfo = NULL,
	.getPmicVoltageInfo = mbraink_v6993_power_get_pmic_voltage_info,
	.suspendprepare = mbraink_v6993_power_suspend_prepare,
	.postsuspend = mbraink_v6993_power_post_suspend,
	.getMmdvfsInfo = mbraink_v6993_power_get_mmdfvs_info,
	.getMmdvfsUserInfo = mbraink_v6993_power_get_mmdvfs_user_info,
	.getPowerThrottleHwInfo = mbraink_v6993_power_get_power_throttle_hw_info,
	.getLpmStateInfo = mbraink_v6993_power_get_lpmstate_info,
	.getSpmiGlitchInfo = mbraink_v6993_power_get_spmi_glitch_info,
	.getDvfsrcInfo = mbraink_v6993_power_get_dvfsrc_info,
	.getMMBWInfo = mbraink_v6993_power_get_mmqos_bw_info,
	.deviceSuspend = mbraink_v6993_power_device_suspend,
	.deviceResume = mbraink_v6993_power_device_resume,
	.getPowerThrottleHwOcInfo = mbraink_v6993_power_get_power_throttle_hw_oc_info,
	.getPowerSmapInfo = mbraink_v6993_power_get_smap_info,
};

int mbraink_v6993_power_init(struct device *dev)
{
	int ret = 0;

	pMBrainkPlatDev = dev;
	ret = register_mbraink_power_ops(&mbraink_v6993_power_ops);

	mbraink_v6993_power_sys_res_init();
	ret = register_low_battery_mbrain_cb(pt2mbrain_hint_low_battery_volt_throttle);
	if (ret != 0) {
		pr_info("register low battery callback failed by: %d", ret);
		return ret;
	}
	ret = register_battery_oc_mbrain_cb(pt2mbrain_hint_battery_oc_throttle);
	if (ret != 0) {
		pr_info("register battery oc callback failed by: %d", ret);
		return ret;
	}
	ret = register_ppb_mbrian_cb(pt2mbrain_ppb_notify_func);
	if (ret != 0) {
		pr_info("register ppb callback failed by: %d", ret);
		return ret;
	}

	ret = register_smap_mbrain_cb(smap2mbrain_notify);
	if (ret != 0) {
		pr_info("register smap callback failed by: %d", ret);
		return ret;
	}

	ret = ccci_mbrain_register(mbraink_v6993_power_ccci_event_cb);
	if (ret != 0) {
		pr_info("register ccci callback failed by: %d", ret);
		return ret;
	}

	ret = register_hpt_mbrian_cb(pt2mbrain_hpt_notify_func);
	if (ret != 0) {
		pr_info("register hpt callback failed by: %d", ret);
		return ret;
	}

	device_enable_async_suspend(dev);

	return ret;
}

int mbraink_v6993_power_deinit(void)
{
	int ret = 0;

	ret = unregister_mbraink_power_ops();
	mbraink_v6993_power_sys_res_deinit();
	ret = unregister_ppb_mbrian_cb();
	if (ret != 0) {
		pr_info("ppb unregister callback failed by: %d", ret);
		return ret;
	}

	ret = ccci_mbrain_unregister();
	if (ret != 0) {
		pr_info("ccci unregister callback failed by: %d", ret);
		return ret;
	}

	ret = unregister_hpt_mbrian_cb();
	if (ret != 0) {
		pr_info("hpt unregister callback failed by: %d", ret);
		return ret;
	}

	return ret;
}
