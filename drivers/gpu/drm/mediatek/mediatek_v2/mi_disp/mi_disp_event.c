/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */
#define pr_fmt(fmt) "mi_disp_event:[%s:%d] " fmt, __func__, __LINE__

#include "mi_disp_print.h"
#include "mi_disp_event.h"
#include <uapi/drm/mi_disp.h>

#if IS_ENABLED(CONFIG_MIEV)
#include <miev/mievent.h>

const char *get_mievent_type_name(int event_type)
{
	switch (event_type) {
	case MI_EVENT_PRI_PANEL_REG_ESD:
		return "pri_panel_reg_esd";
	case MI_EVENT_PRI_PANEL_IRQ_ESD:
		return "pri_panel_irq_esd";
	case MI_EVENT_PRI_PLATFORM_ESD:
		return "pri_platform_esd";
	case MI_EVENT_DSI_ERROR:
		return "dsi_error";
	case MI_EVENT_PANEL_HW_RESOURCE_GET_FAILED:
		return "panel_hw_resource_get_failed";
	case MI_EVENT_PANEL_RECOGNIZE_ERR:
		return "panel_recognize_err";
	case MI_EVENT_PANEL_WP_READ_FAILED:
		return "panel_wp_read_failed";
	case MI_EVENT_PANEL_UNDERRUN:
		return "panel_underrun";
	case MI_EVENT_ALL_ZERO_PCC:
		return "df_all_zero_pcc";
	case MI_EVENT_INVALID_FRAME_PENDING:
		return "sde_invalid_frame_pending";
	case MI_EVENT_CMD_RECEIVE_FAILED:
		return "dsi_cmd_receive_failed";
	case MI_EVENT_CMD_SEND_FAILED:
		return "dsi_cmd_send_failed";
	case MI_EVENT_SMMU_FAULT:
		return "sde_smmu_fault";
	case MI_EVENT_FENCE_SIGNAL_ERR:
		return "sde_fence_signal_err";
	case MI_EVENT_CLOCK_ERR:
		return "dsi_clock_err";
	case MI_EVENT_DMA_BUF_ALLOCATE_FAILED:
		return "dma_buf_allocate_failed";
	case MI_EVENT_BACKLIGHT_ZERO:
		return "backlight_zero";
	case MI_EVENT_PINGPONG_TIMEOUT:
		return "dsi_pingpong_timeout";
	case MI_EVENT_CMDQ_TIMEOUT:
		return "cmdq_timeout";
	case MI_EVENT_DMA_CMD_TIMEOUT:
		return "dsi_dma_cmd_timeout";
	case MI_EVENT_RD_PTR_TIMEOUT:
		return "dpu_irq_rdptr_timeout";
	case MI_EVENT_WR_PTR_TIMEOUT:
		return "dpu_irq_wrptr_timeout";
	case MI_EVENT_DP_READ_EDID_FAIL:
		return "EDID_read_fail";
	case MI_EVENT_SEC_PANEL_REG_ESD:
		return "sec_panel_reg_esd";
	case MI_EVENT_SEC_PANEL_IRQ_ESD:
		return "sec_panel_irq_esd";
	case MI_EVENT_SEC_PLATFORM_ESD:
		return "sec_platform_esd";
	case MI_EVENT_PANEL_HARDWARE_ERR:
		return "panel_hardware_err";
	case MI_EVENT_PRI_PANEL_REG_ESD_RECOVERY:
		return "pri_panel_reg_esd_recover";
	case MI_EVENT_PRI_PANEL_IRQ_ESD_RECOVERY:
		return "pri_panel_irq_esd_recover";
	case MI_EVENT_PRI_PLATFORM_ESD_RECOVERY:
		return "pri_platform_esd_recover";
	case MI_EVENT_SEC_PANEL_REG_ESD_RECOVERY:
		return "sec_panel_reg_esd_recover";
	case MI_EVENT_SEC_PANEL_IRQ_ESD_RECOVERY:
		return "sec_panel_irq_esd_recover";
	case MI_EVENT_SEC_PLATFORM_ESD_RECOVERY:
		return "sec_platform_esd_recover";
	default:
		return "unknown";
	}
}

unsigned int get_mievent_recovery_type(int event_type)
{
	switch (event_type) {
	case MI_EVENT_PRI_PANEL_REG_ESD:
		return MI_EVENT_PRI_PANEL_REG_ESD_RECOVERY;
	break;
	case MI_EVENT_PRI_PANEL_IRQ_ESD:
		return MI_EVENT_PRI_PANEL_IRQ_ESD_RECOVERY;
	break;
	case MI_EVENT_PRI_PLATFORM_ESD:
		return MI_EVENT_PRI_PLATFORM_ESD_RECOVERY;
	break;
	case MI_EVENT_SEC_PANEL_REG_ESD:
		return MI_EVENT_SEC_PANEL_REG_ESD_RECOVERY;
	break;
	case MI_EVENT_SEC_PANEL_IRQ_ESD:
		return MI_EVENT_SEC_PANEL_IRQ_ESD_RECOVERY;
	break;
	case MI_EVENT_SEC_PLATFORM_ESD:
		return MI_EVENT_SEC_PLATFORM_ESD_RECOVERY;
	break;
	default:
		return 0;
	}
}
#endif

unsigned int ESD_TYPE = 0;

void mi_disp_mievent_str(unsigned int event_type){
#if IS_ENABLED(CONFIG_MIEV)
	const char *event_describe;
	const char *event_name = NULL;
	struct misight_mievent *event  = cdev_tevent_alloc(event_type);

	event_name = get_mievent_type_name(event_type);
	DISP_INFO("[DIS-TF-DISPLAY] event_type[%d],event_name[%s]\n",event_type,event_name);

	switch(event_type){
	case MI_EVENT_DSI_ERROR:
		event_describe = "panel dsi error";
	break;
	case MI_EVENT_PANEL_HW_RESOURCE_GET_FAILED:
		event_describe = "panel HW Resource Get Failed";
	break;
	case MI_EVENT_PANEL_RECOGNIZE_ERR:
		event_describe = "wrong panel";
	break;
	case MI_EVENT_PANEL_WP_READ_FAILED:
		event_describe = "panel WP Read Failed";
	break;
	case MI_EVENT_PANEL_HARDWARE_ERR:
		event_describe = "panel Hardware Err";
	break;
	default:
		DISP_ERROR("It's a invalid event_type");
		cdev_tevent_destroy(event);
		return;
	}

	cdev_tevent_add_str(event, event_name, event_describe);
	cdev_tevent_write(event);
	cdev_tevent_destroy(event);
#endif
}

/**
* mi_disp_mievent_int
* @param disp_id MI_DISP_PRIMARY or MI_DISP_SECONDARY
* @param mievent
*	event_type: the fault code for the problem event，eg："911001014"
*	event_type_param1: first argument for the fault code,eg:"done_count"
*	event_type_param2: second argument for the fault code,eg:"commit_count"
**/
void mi_disp_mievent_int(int disp_id,struct mi_event_info *mievent){
#if IS_ENABLED(CONFIG_MIEV)
	static int esd_count[2][3] = {0};
	static int err_count[15] = {0};
	const char *event_name = NULL;
	int esd_index = 0;
	const bool smmu_fault = true;
	static int esd_num = 0;
	static ktime_t time_end;
	static ktime_t time_start_esd;
	static ktime_t time_start_cmdr;
	static ktime_t time_start_cmds;
	unsigned long wait_timeout = msecs_to_jiffies(10000);
	static unsigned long last_time_cmdr = 0;
	static unsigned long last_time_cmds = 0;
	struct misight_mievent *event;

	event_name = get_mievent_type_name(mievent->event_type);
	if (!strcmp(event_name, "unknown")) {
		DISP_ERROR("Invalid event_type[%d]",mievent->event_type);
		return;
	}
	event  = cdev_tevent_alloc(mievent->event_type);

	switch (mievent->event_type) {
	case MI_EVENT_PANEL_UNDERRUN:
		err_count[0]++;
		DISP_INFO("[DIS-TF-DISPLAY] event_type[%d],event_name[%s],err_count = %d\n",
			mievent->event_type,event_name,err_count[0]);
		cdev_tevent_add_int(event, event_name, err_count[0]);
	break;
	case MI_EVENT_ALL_ZERO_PCC:
		err_count[1]++;
		DISP_INFO("[DIS-TF-DISPLAY] event_type[%d],event_name[%s],err_count = %d\n",
			mievent->event_type,event_name,err_count[1]);
		cdev_tevent_add_int(event, event_name, err_count[1]);
	break;
	case MI_EVENT_INVALID_FRAME_PENDING:
		err_count[2]++;
		DISP_INFO("[DIS-TF-DISPLAY] event_type[%d],event_name[%s],err_count = %d\n",
			mievent->event_type,event_name,err_count[2]);
		cdev_tevent_add_int(event, event_name, err_count[2]);
		cdev_tevent_add_int(event, "frame_pending_value", mievent->event_type_param1);
	break;
	case MI_EVENT_CMD_RECEIVE_FAILED:
		if(err_count[3] == 0)
			time_start_cmdr = ktime_get_boottime();

		err_count[3]++;
		DISP_INFO("[DIS-TF-DISPLAY] event_type[%d],event_name[%s],err_count = %d\n",
			mievent->event_type,event_name,err_count[3]);

		if(time_after(jiffies , last_time_cmdr + wait_timeout)) {
			last_time_cmdr = jiffies;
			cdev_tevent_add_int(event, event_name, err_count[3]);
			cdev_tevent_add_int(event, "err_type", mievent->event_type_param1);
		} else {
			cdev_tevent_destroy(event);
			return;
		}

		if (err_count[3] > MI_EVENT_CMD_COUNT_MAX){
			time_end = ktime_get_boottime();
			if(ktime_to_ms(ktime_sub(time_end,time_start_cmdr)) < (MI_EVENT_ESD_TIMEOUT * 60 * 1000)){
				mi_disp_mievent_str(MI_EVENT_PANEL_HARDWARE_ERR);
			}
			err_count[3] = 0;
		}
	break;
	case MI_EVENT_CMD_SEND_FAILED:
		if(err_count[4] == 0)
			time_start_cmds = ktime_get_boottime();

		err_count[4]++;
		DISP_INFO("[DIS-TF-DISPLAY] event_type[%d],event_name[%s],err_count = %d\n",
			mievent->event_type,event_name,err_count[4]);

		if(time_after(jiffies , last_time_cmds + wait_timeout)) {
			last_time_cmds = jiffies;
			cdev_tevent_add_int(event, event_name, err_count[4]);
			cdev_tevent_add_int(event, "dsi_cmd_set_type", mievent->event_type_param1);
		} else {
			cdev_tevent_destroy(event);
			return;
		}

		if (err_count[4] > MI_EVENT_CMD_COUNT_MAX){
			time_end = ktime_get_boottime();
			if(ktime_to_ms(ktime_sub(time_end,time_start_cmds)) < (MI_EVENT_ESD_TIMEOUT * 60 * 1000)){
				mi_disp_mievent_str(MI_EVENT_PANEL_HARDWARE_ERR);
			}
			err_count[4] = 0;
		}
	break;
	case MI_EVENT_SMMU_FAULT:
		DISP_INFO("[DIS-TF-DISPLAY] event_type[%d],event_name[%s]\n",
			mievent->event_type,event_name);
		cdev_tevent_add_int(event, event_name, smmu_fault);
	break;
	case MI_EVENT_FENCE_SIGNAL_ERR:
		err_count[5]++;
		DISP_INFO("[DIS-TF-DISPLAY] event_type[%d],event_name[%s],err_count = %d\n",
			mievent->event_type,event_name,err_count[5]);
		cdev_tevent_add_int(event, event_name, err_count[5]);
		cdev_tevent_add_int(event, "done_count", mievent->event_type_param1);
		cdev_tevent_add_int(event, "commit_count", mievent->event_type_param2);
	break;
	case MI_EVENT_CLOCK_ERR:
		err_count[6]++;
		DISP_INFO("[DIS-TF-DISPLAY] event_type[%d],event_name[%s],err_count = %d\n",
			mievent->event_type,event_name,err_count[6]);
		cdev_tevent_add_int(event, event_name, err_count[6]);
		cdev_tevent_add_int(event, "err_type", mievent->event_type_param1);
	break;
	case MI_EVENT_DMA_BUF_ALLOCATE_FAILED:
		err_count[7]++;
		DISP_INFO("[DIS-TF-DISPLAY] event_type[%d],event_name[%s],err_count = %d\n",
			mievent->event_type,event_name,err_count[7]);
		cdev_tevent_add_int(event, event_name, err_count[7]);
		cdev_tevent_add_int(event, "err_type", mievent->event_type_param1);
	break;
	case MI_EVENT_BACKLIGHT_ZERO:
		err_count[8]++;
		DISP_INFO("[DIS-TF-DISPLAY] event_type[%d],event_name[%s], %s panel err_count = %d\n",
			mievent->event_type,event_name, get_disp_id_name(disp_id), err_count[8]);
		cdev_tevent_add_int(event, event_name, err_count[8]);
	break;
	case MI_EVENT_PINGPONG_TIMEOUT:
		err_count[9]++;
		DISP_INFO("[DIS-TF-DISPLAY] event_type[%d],event_name[%s],err_count = %d\n",
			mievent->event_type,event_name,err_count[9]);
		cdev_tevent_add_int(event, event_name, err_count[9]);
	break;
	case MI_EVENT_DMA_CMD_TIMEOUT:
		err_count[10]++;
		DISP_INFO("[DIS-TF-DISPLAY] event_type[%d],event_name[%s],err_count = %d\n",
			mievent->event_type,event_name,err_count[10]);
		cdev_tevent_add_int(event, event_name, err_count[10]);
	break;
	case MI_EVENT_RD_PTR_TIMEOUT:
		err_count[11]++;
		DISP_INFO("[DIS-TF-DISPLAY] event_type[%d],event_name[%s], %s panel err_count = %d\n",
			mievent->event_type,event_name, get_disp_id_name(disp_id), err_count[11]);
		cdev_tevent_add_int(event, event_name, err_count[11]);
	break;
	case MI_EVENT_WR_PTR_TIMEOUT:
		err_count[12]++;
		DISP_INFO("[DIS-TF-DISPLAY] event_type[%d],event_name[%s], %s panel err_count = %d\n",
			mievent->event_type,event_name, get_disp_id_name(disp_id), err_count[12]);
		cdev_tevent_add_int(event, event_name, err_count[12]);
	break;
	case MI_EVENT_DP_READ_EDID_FAIL:
		err_count[13]++;
		DISP_INFO("[DIS-TF-DISPLAY] event_type[%d],event_name[%s],err_count = %d\n",
			mievent->event_type,event_name,err_count[13]);
		cdev_tevent_add_int(event, event_name, err_count[13]);
	break;
	case MI_EVENT_CMDQ_TIMEOUT:
		err_count[14]++;
		DISP_INFO("[DIS-TF-DISPLAY] event_type[%d],event_name[%s],err_count = %d\n",
			mievent->event_type,event_name,err_count[14]);
		cdev_tevent_add_int(event, event_name, err_count[14]);
	break;
	default:
		ESD_TYPE = mievent->event_type;
		esd_index = (mievent->event_type % 10) -1;
		if(esd_index < 0 || esd_index >2 || !is_support_disp_id(disp_id)){
			DISP_ERROR("esd_index[%d]/disp_id[%d] is invalid",esd_index,disp_id);
			cdev_tevent_destroy(event);
			return;
		}
		esd_count[disp_id][esd_index]++;
		DISP_INFO("[DIS-TF-DISPLAY] event_type[%d],event_name[%s],esd_count[%d][%d] = %d\n",
			mievent->event_type,event_name,disp_id,esd_index,esd_count[disp_id][esd_index]);
		cdev_tevent_add_int(event, event_name, esd_count[disp_id][esd_index]);

		if(esd_num == 0)
			time_start_esd = ktime_get_boottime();

		if (++esd_num > MI_EVENT_ESD_COUNT_MAX){
			time_end = ktime_get_boottime();
			if(ktime_to_ms(ktime_sub(time_end,time_start_esd)) < (MI_EVENT_ESD_TIMEOUT * 60 * 1000)){
				mi_disp_mievent_str(MI_EVENT_PANEL_HARDWARE_ERR);
			}
			esd_num = 0;
		}
	}

	cdev_tevent_write(event);
	cdev_tevent_destroy(event);
#endif
}

void mi_disp_mievent_recovery(unsigned int event_type) {
#if IS_ENABLED(CONFIG_MIEV)
	static int Esd_Done_Count[2][3] = {0};
	int Esd_Done_index = 0;
	int disp_id = 0;
	unsigned int recover_event_type = 0;
	const char * event_name = NULL;
	const char * esd_event_type_name = "problem_code";
	struct misight_mievent *event = NULL;

	ESD_TYPE = 0;
	recover_event_type = get_mievent_recovery_type(event_type);
	event_name = get_mievent_type_name(recover_event_type);

	switch (recover_event_type) {
	case MI_EVENT_PRI_PANEL_REG_ESD_RECOVERY:
	break;
	case MI_EVENT_PRI_PANEL_IRQ_ESD_RECOVERY:
		Esd_Done_index = 1;
	break;
	case MI_EVENT_PRI_PLATFORM_ESD_RECOVERY:
		Esd_Done_index = 2;
	break;
	case MI_EVENT_SEC_PANEL_REG_ESD_RECOVERY:
		disp_id = 1;
	break;
	case MI_EVENT_SEC_PANEL_IRQ_ESD_RECOVERY:
		disp_id = 1;
		Esd_Done_index = 1;
	break;
	case MI_EVENT_SEC_PLATFORM_ESD_RECOVERY:
		disp_id = 1;
		Esd_Done_index = 2;
	break;
	default:
		DISP_ERROR("It is an invalid event_type[%d],recover_event_type[%d]",event_type,recover_event_type);
		return;
	}

	event  = cdev_tevent_alloc(recover_event_type);

	Esd_Done_Count[disp_id][Esd_Done_index]++;
	DISP_INFO("recover_event_type[%d],event_name[%s],Esd_Done_Count[%d][%d] = %d\n",
		recover_event_type,event_name,disp_id,Esd_Done_index,Esd_Done_Count[disp_id][Esd_Done_index]);
	cdev_tevent_add_int(event, event_name, Esd_Done_Count[disp_id][Esd_Done_index]);
	cdev_tevent_add_int(event, esd_event_type_name, event_type);
	cdev_tevent_write(event);
	cdev_tevent_destroy(event);
#endif
}
