/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef MBRAINK_H
#define MBRAINK_H

#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <net/sock.h>
#include <linux/pid.h>

#include "mbraink_ioctl_struct_def.h"

#if IS_ENABLED(CONFIG_GRT_HYPERVISOR)
#include <mbraink_auto_ioctl_struct_def.h>
#endif

#define IOC_MAGIC	'k'

#define MAX_BUF_SZ			1024

/*Mbrain Delegate Info List*/
#define AUTO_IOCTL				'0'
#define POWER_INFO				'1'
#define VIDEO_INFO				'2'
#define POWER_SUSPEND_EN		'3'
#define PROCESS_MEMORY_INFO		'4'
#define PROCESS_STAT_INFO		'5'
#define THREAD_STAT_INFO		'6'
#define SET_MINITOR_PROCESS		'7'
#define MEMORY_DDR_INFO			'8'
#define IDLE_RATIO_INFO         '9'
#define TRACE_PROCESS_INFO      'a'
#define VCORE_INFO              'b'
#define CPUFREQ_NOTIFY_INFO		'c'
#define BATTERY_INFO			'e'
#define FEATURE_EN				'f'
#define WAKEUP_INFO				'g'
#define PMU_EN					'h'
#define PMU_INFO				'i'
#define POWER_SPM_RAW			'j'
#define MODEM_INFO				'k'
#define MEMORY_MDV_INFO			'l'
#define MONITOR_BINDER_PROCESS         'm'
#define TRACE_BINDER_INFO              'o'
#define VCORE_VOTE_INFO			'p'
#define POWER_SPM_L2_INFO		'q'
#define POWER_SCP_INFO			'r'
#define POWER_SPMI_INFO		's'
#define POWER_UVLO_INFO		't'
#define GPU_OPP_INFO			'u'
#define GPU_STATE_INFO			'v'
#define GPU_LOADING_INFO		'w'
#define PMIC_VOLTAGE_INFO		'x'
#define OPERATION_MODE_INFO		'y'
#define GNSS_LP_INFO			'z'
#define GNSS_MCU_INFO			'A'
#define MMDVFS_INFO				'B'
#define WIFI_RATE_INFO			'C'
#define WIFI_RADIO_INFO			'D'
#define WIFI_AC_INFO			'E'
#define WIFI_LP_INFO			'F'
#define POWER_THROTTLE_HW_INFO	'G'
#define LPM_STATE_INFO			'H'
#define TRACE_OOM_INFO                  'I'
#define UFS_INFO				'J'
#define WIFI_TXTIMEOUT_INFO		'K'
#define VDEC_FPS_INFO			'L'
#define POWER_SCP_TASK_INFO		'M'
#define TOUCH_GHOST_INFO		'N'
#define NETLINK_TRIGGER_RECV	'O'
#define POWER_SPMI_GLITCH_INFO  'P'
#define POWER_DVFSRC_INFO		'Q'
#define WIFI_PCIE_INFO			'R'
#define WIFI_TXPWR_RPT			'S'
#define WIFI_RXTXPERF_INFO		'T'
#define MEMORY_EMI_INFO	'U'
#define WIFI_WAKEUP_INFO		'V'
#define MMDVFS_USER_INFO		'W'
#define TIMER_MAPPING_INFO	'X'
#define MMQOS_BW_INFO			'Y'
#define MEMORY_CM_PROFILE_INFO	'Z'

#define NR_CODE_LAGACY_MAX      122 // ('z')
#define TRACE_CPUFREQ_INFO      (NR_CODE_LAGACY_MAX + 1)
#define POWER_THROTTLE_HW_OC_INFO	(NR_CODE_LAGACY_MAX + 2)
#define MEMORY_VSMR_INFO	(NR_CODE_LAGACY_MAX + 3)
#define POWER_SMAP_INFO	(NR_CODE_LAGACY_MAX + 4)

/*Mbrain Delegate IOCTL List*/
#define AUTO_IOCTL_INFO			_IOR(IOC_MAGIC, AUTO_IOCTL, \
							struct mbraink_auto_ioctl_info*)
#define RO_POWER				_IOR(IOC_MAGIC, POWER_INFO, char*)
#define RO_VIDEO				_IOR(IOC_MAGIC, VIDEO_INFO, char*)
#define WO_SUSPEND_POWER_EN		_IOW(IOC_MAGIC, POWER_SUSPEND_EN, char*)
#define RO_PROCESS_MEMORY		_IOR(IOC_MAGIC, PROCESS_MEMORY_INFO, \
							struct mbraink_process_memory_data*)
#define RO_PROCESS_STAT			_IOR(IOC_MAGIC, PROCESS_STAT_INFO, \
							struct mbraink_process_stat_data*)
#define RO_THREAD_STAT			_IOR(IOC_MAGIC, THREAD_STAT_INFO, \
							struct mbraink_thread_stat_data*)
#define WO_MONITOR_PROCESS		_IOW(IOC_MAGIC, SET_MINITOR_PROCESS, \
							struct mbraink_monitor_processlist*)
#define RO_MEMORY_DDR_INFO		_IOR(IOC_MAGIC, MEMORY_DDR_INFO, \
							struct mbraink_memory_ddrInfo*)
#define RO_IDLE_RATIO                  _IOR(IOC_MAGIC, IDLE_RATIO_INFO, \
							struct mbraink_audio_idleRatioInfo*)
#define RO_TRACE_PROCESS            _IOR(IOC_MAGIC, TRACE_PROCESS_INFO, \
							struct mbraink_tracing_pid_data*)
#define RO_VCORE_INFO                 _IOR(IOC_MAGIC, VCORE_INFO, \
							struct mbraink_power_vcoreInfo*)
#define RO_CPUFREQ_NOTIFY		_IOR(IOC_MAGIC, CPUFREQ_NOTIFY_INFO, \
							struct mbraink_cpufreq_notify_struct_data*)
#define RO_BATTERY_INFO			_IOR(IOC_MAGIC, BATTERY_INFO, \
							struct mbraink_battery_data*)
#define WO_FEATURE_EN		_IOW(IOC_MAGIC, FEATURE_EN, \
							struct mbraink_feature_en*)
#define RO_WAKEUP_INFO			_IOR(IOC_MAGIC, WAKEUP_INFO, \
							struct mbraink_power_wakeup_data*)
#define WO_PMU_EN				_IOW(IOC_MAGIC, PMU_EN, \
							struct mbraink_pmu_en*)
#define RO_PMU_INFO				_IOR(IOC_MAGIC, PMU_INFO, \
							struct mbraink_pmu_info*)

#define RO_POWER_SPM_RAW			_IOR(IOC_MAGIC, POWER_SPM_RAW, \
								struct mbraink_power_spm_raw*)

#define RO_MODEM_INFO			_IOR(IOC_MAGIC, MODEM_INFO, \
							struct mbraink_modem_raw*)
#define WO_MONITOR_BINDER_PROCESS	_IOW(IOC_MAGIC,	MONITOR_BINDER_PROCESS,	\
						struct mbraink_monitor_processlist*)
#define RO_TRACE_BINDER			_IOR(IOC_MAGIC, TRACE_BINDER_INFO,	\
						struct mbraink_binder_trace_data*)

#define RO_MEMORY_MDV_INFO			_IOR(IOC_MAGIC, MEMORY_MDV_INFO, \
							struct mbraink_memory_mdvInfo*)

#define RO_VCORE_VOTE			_IOR(IOC_MAGIC, VCORE_VOTE_INFO, \
						struct mbraink_voting_struct_data*)

#define RO_POWER_SPM_L2_INFO	_IOR(IOC_MAGIC, POWER_SPM_L2_INFO, \
						struct mbraink_power_spm_l2_info*)

#define RO_POWER_SCP_INFO	_IOR(IOC_MAGIC, POWER_SCP_INFO, \
						struct mbraink_power_scp_info*)
#define RO_POWER_SPMI_INFO	_IOR(IOC_MAGIC, POWER_SPMI_INFO, \
						struct mbraink_spmi_struct_data*)
#define RO_POWER_UVLO_INFO	_IOR(IOC_MAGIC, POWER_UVLO_INFO, \
							struct mbraink_uvlo_struct_data*)

#define RO_GPU_OPP_INFO	_IOR(IOC_MAGIC, GPU_OPP_INFO, \
						struct mbraink_gpu_opp_info*)

#define RO_GPU_STATE_INFO	_IOR(IOC_MAGIC, GPU_STATE_INFO, \
						struct mbraink_gpu_state_info*)

#define RO_GPU_LOADING_INFO	_IOR(IOC_MAGIC, GPU_LOADING_INFO, \
						struct mbraink_gpu_loading_info*)

#define RO_PMIC_VOLTAGE_INFO	_IOR(IOC_MAGIC, PMIC_VOLTAGE_INFO, \
						struct mbraink_pmic_voltage_info*)

#define WO_OPERATION_MODE_INFO	_IOW(IOC_MAGIC, OPERATION_MODE_INFO, \
						struct mbraink_operation_mode_info*)
#define RO_GNSS_LP		_IOR(IOC_MAGIC, GNSS_LP_INFO, \
					struct mbraink_gnss2mbr_lp_data*)
#define RO_GNSS_MCU		_IOR(IOC_MAGIC, GNSS_MCU_INFO, \
					struct mbraink_gnss2mbr_mcu_data*)
#define RO_MMDVFS_INFO	_IOR(IOC_MAGIC, MMDVFS_INFO, \
						struct mbraink_mmdvfs_info*)

#define RO_WIFI_RATE_INFO	_IOR(IOC_MAGIC, WIFI_RATE_INFO, \
					struct mbraink_wifi2mbr_lls_rate_data*)
#define RO_WIFI_RADIO_INFO	_IOR(IOC_MAGIC, WIFI_RADIO_INFO, \
					struct mbraink_wifi2mbr_lls_radio_data*)
#define RO_WIFI_AC_INFO		_IOR(IOC_MAGIC, WIFI_AC_INFO, \
					struct mbraink_wifi2mbr_lls_ac_data*)
#define RO_WIFI_LP_INFO		_IOR(IOC_MAGIC, WIFI_LP_INFO, \
					struct mbraink_wifi2mbr_lp_ratio_data*)
#define RO_POWER_THROTTLE_HW_INFO	_IOR(IOC_MAGIC, POWER_THROTTLE_HW_INFO, \
						struct mbraink_power_throttle_hw_data*)
#define RO_LPM_STATE_INFO		_IOR(IOC_MAGIC, LPM_STATE_INFO, \
						struct mbraink_lpm_state_data*)

#define RO_TRACE_OOM            _IOR(IOC_MAGIC, TRACE_OOM_INFO, \
					struct mbraink_oom_tracing_data*)
#define RO_UFS_INFO	_IOR(IOC_MAGIC, UFS_INFO, \
								struct mbraink_ufs_info*)
#define RO_WIFI_TXTIMEOUT_INFO	_IOR(IOC_MAGIC, WIFI_TXTIMEOUT_INFO, \
					struct mbraink_wifi2mbr_txtimeout_data*)

#define RO_WIFI_TXPWR_RPT	_IOR(IOC_MAGIC, WIFI_TXPWR_RPT, \
					struct mbraink_wifi2mbr_tx_power_data*)

#define RO_WIFI_WAKEUP_INFO	_IOR(IOC_MAGIC, WIFI_WAKEUP_INFO, \
					struct mbraink_wifi2mbr_wakeupinfo_data*)

#define RO_VDEC_FPS		_IOR(IOC_MAGIC, VDEC_FPS_INFO, \
					struct mbraink_vdec_fps*)

#define RO_POWER_SCP_TASK_INFO	_IOR(IOC_MAGIC, POWER_SCP_TASK_INFO, \
						struct mbraink_power_scp_task_info*)
#define WO_NETLINK_TRIGGER_RECV		_IOW(IOC_MAGIC, NETLINK_TRIGGER_RECV, char*)

#define RO_TOUCH_GHOST_INFO	_IOR(IOC_MAGIC, TOUCH_GHOST_INFO, \
						struct mbraink_touch_ghost_info*)

#define RO_MEMORY_EMI_INFO		_IOR(IOC_MAGIC, MEMORY_EMI_INFO, \
							struct mbraink_memory_emiInfo*)

#define RO_POWER_SPMI_GLITCH_INFO	_IOR(IOC_MAGIC, POWER_SPMI_GLITCH_INFO, \
					struct mbraink_spmi_glitch_struct_data*)
#define RO_TRACE_CPUFREQ	_IOR(IOC_MAGIC, TRACE_CPUFREQ_INFO,      \
					struct mbraink_cpufreq_trace_data*)

#define RO_POWER_DVFSRC_INFO	_IOR(IOC_MAGIC, POWER_DVFSRC_INFO, \
					struct mbraink_dvfsrc_struct_data*)

#define RO_WIFI_PCIE_INFO       _IOR(IOC_MAGIC, WIFI_PCIE_INFO, \
					struct mbraink_wifi2mbr_pcie_data*)
#define RO_WIFI_RXTXPERF_INFO       _IOR(IOC_MAGIC, WIFI_RXTXPERF_INFO, \
					struct mbraink_wifi2mbr_rxtxperf_data*)
#define RO_MMDVFS_USER_INFO	_IOR(IOC_MAGIC, MMDVFS_USER_INFO, \
						struct mbraink_mmdvfs_user_info*)
#define RO_TIMER_MAPPING_INFO	_IOR(IOC_MAGIC, TIMER_MAPPING_INFO, \
					struct mbraink_timer_mapping_info*)
#define RO_MMQOS_BW_INFO	_IOR(IOC_MAGIC, MMQOS_BW_INFO, \
						struct mbraink_mmqos_bw_info*)
#define RO_MEMORY_CM_PROFILE_INFO	_IOR(IOC_MAGIC, MEMORY_CM_PROFILE_INFO, \
						struct mbraink_memory_cmProfileInfo*)
#define RO_POWER_THROTTLE_HW_OC_INFO	_IOR(IOC_MAGIC, POWER_THROTTLE_HW_OC_INFO, \
						struct mbraink_power_throttle_hw_oc_data*)
#define RO_MEMORY_VSMR_INFO		_IOR(IOC_MAGIC, MEMORY_VSMR_INFO, \
						struct mbraink_memory_vsmrInfo*)
#define RO_POWER_SMAP_INFO		_IOR(IOC_MAGIC, POWER_SMAP_INFO, \
						struct mbraink_power_smap_info*)

#define SUSPEND_DATA	0
#define RESUME_DATA		1
#define CURRENT_DATA	2

#ifndef GENL_ID_GENERATE
#define GENL_ID_GENERATE    0
#endif

enum {
	MBRAINK_A_UNSPEC,
	MBRAINK_A_MSG,
	__MBRAINK_A_MAX,
};
#define MBRAINK_A_MAX (__MBRAINK_A_MAX - 1)

enum {
	MBRAINK_C_UNSPEC,
	MBRAINK_C_PID_CTRL,
	__MBRAINK_C_MAX,
};
#define MBRAINK_C_MAX (__MBRAINK_C_MAX - 1)

struct mbraink_data {
#define CHRDEV_NAME     "mbraink_chrdev"
	struct cdev mbraink_cdev;
	char suspend_power_info_en[2];
	long long last_suspend_timestamp;
	long long last_resume_timestamp;
	long long last_suspend_ktime;
	struct mbraink_battery_data suspend_battery_buffer;
	int client_pid;
	unsigned int feature_en;
	unsigned int pmu_en;
};

int mbraink_netlink_send_msg(const char *msg);
int logmiscdata2mbrain(long long *value, unsigned int value_num, char *buf, unsigned int buf_size);


#endif /*end of MBRAINK_H*/
