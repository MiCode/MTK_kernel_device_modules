/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#define pr_fmt(fmt)	"mi-disp-lhbm:" fmt

#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>

#include "mi_disp_print.h"
#include "mi_disp_lhbm.h"
#include "mi_dsi_display.h"

static struct disp_lhbm_fod *g_lhbm_fod[MI_DISP_MAX];
static atomic_t mi_disp_fod_status = ATOMIC_INIT(0);
static int mi_disp_lhbm_fod_thread_fn(void *arg);

struct mtk_dsi * mi_get_primary_dsi_display(void)
{
	struct disp_feature *df = mi_get_disp_feature();
	struct disp_display *dd_ptr = NULL;
	struct mtk_dsi *dsi = NULL;

	if (df) {
		dd_ptr = &df->d_display[MI_DISP_PRIMARY];
		if (dd_ptr->display && dd_ptr->intf_type == MI_INTF_DSI) {
			dsi = (struct mtk_dsi *)dd_ptr->display;
			DISP_INFO("return mtk_dsi\n");
			return dsi;
		} else {
			return NULL;
		}
	} else {
		return NULL;
	}
}

bool is_local_hbm(int disp_id)
{
	struct mtk_dsi *dsi = mi_get_primary_dsi_display();

	if (dsi)
		return dsi->mi_cfg.local_hbm_enabled;
	return false;
}

bool mi_disp_lhbm_fod_enabled(struct mtk_dsi *dsi)
{
	if (dsi)
		return dsi->mi_cfg.local_hbm_enabled;
	return false;
}

struct disp_lhbm_fod *mi_get_disp_lhbm_fod(int disp_id)
{
	if (is_support_disp_id(disp_id))
		return g_lhbm_fod[disp_id];
	else
		return NULL;
}
EXPORT_SYMBOL_GPL(mi_get_disp_lhbm_fod);

int mi_disp_lhbm_fod_thread_create(struct disp_feature *df, int disp_id)
{
	int ret = 0;
	struct mtk_dsi *dsi = NULL;
	struct disp_lhbm_fod *lhbm_fod = NULL;
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	if (!df || !is_support_disp_id(disp_id)) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (!df->d_display[disp_id].display ||
		df->d_display[disp_id].intf_type != MI_INTF_DSI) {
		DISP_ERROR("unsupported display(%s intf)\n",
			get_disp_intf_type_name(df->d_display[disp_id].intf_type));
		return -EINVAL;
	}

	dsi = (struct mtk_dsi *)df->d_display[disp_id].display;
	if(!dsi) {
		DISP_ERROR("dsi is null point");
		return -EINVAL;
	}

	comp = &dsi->ddp_comp;
	if(!comp) {
		DISP_ERROR("comp is null point");
		return -EINVAL;
	}

	panel_ext = mtk_dsi_get_panel_ext(comp);
	if(!panel_ext) {
		DISP_ERROR("panel_ext is null point");
		return -EINVAL;
	}

	if(panel_ext && panel_ext->funcs && 
		panel_ext->funcs->panel_fod_lhbm_init) {
		if(panel_ext->funcs->panel_fod_lhbm_init(dsi))
			pr_err("panel_fod_lhbm_init failed\n");
	}
  
	if (!mi_disp_lhbm_fod_enabled(dsi)) {
		DISP_INFO("%s panel is not local hbm\n", get_disp_id_name(disp_id));
		return 0;
	}

	lhbm_fod = kzalloc(sizeof(struct disp_lhbm_fod), GFP_KERNEL);

	if (!lhbm_fod) {
		DISP_ERROR("can not allocate buffer for disp_lhbm\n");
		return -ENOMEM;
	}

	df->d_display[disp_id].lhbm_fod_ptr = lhbm_fod;
	lhbm_fod->dsi = dsi;


	INIT_LIST_HEAD(&lhbm_fod->event_list);
	spin_lock_init(&lhbm_fod->spinlock);

	atomic_set(&lhbm_fod->allow_tx_lhbm, 1);
	atomic_set(&lhbm_fod->fingerprint_status, FINGERPRINT_NONE);
	atomic_set(&lhbm_fod->last_fod_status, FOD_EVENT_MAX);

	init_waitqueue_head(&lhbm_fod->fod_pending_wq);

	lhbm_fod->fod_thread = kthread_run(mi_disp_lhbm_fod_thread_fn,
			lhbm_fod, "disp_lhbm_fod:%d", disp_id);
	if (IS_ERR(lhbm_fod->fod_thread)) {
		DISP_ERROR("failed to create disp_fod:%d kthread\n", disp_id);
		ret = PTR_ERR(lhbm_fod->fod_thread);
		lhbm_fod->fod_thread = NULL;
		goto error;
	}
	/* set realtime priority */
	sched_set_fifo(lhbm_fod->fod_thread);

	g_lhbm_fod[disp_id] = lhbm_fod;

	DISP_INFO("create disp_lhbm_fod:%d kthread success\n", disp_id);

	return ret;

error:
	kfree(lhbm_fod);
	return ret;

}

EXPORT_SYMBOL_GPL(mi_disp_lhbm_fod_thread_create);

int mi_disp_lhbm_fod_thread_destroy(struct disp_feature *df, int disp_id)
{
	int ret = 0;
	struct disp_lhbm_fod *lhbm_fod = NULL;

	if (!df || !is_support_disp_id(disp_id)) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	lhbm_fod = df->d_display[disp_id].lhbm_fod_ptr;
	if (lhbm_fod) {
		if (lhbm_fod->fod_thread) {
			kthread_stop(lhbm_fod->fod_thread);
			lhbm_fod->fod_thread = NULL;
		}
		kfree(lhbm_fod);
	}

	df->d_display[disp_id].lhbm_fod_ptr = NULL;
	g_lhbm_fod[disp_id] = NULL;

	DISP_INFO("destroy disp_lhbm_fod:%d kthread success\n", disp_id);

	return ret;
}
EXPORT_SYMBOL_GPL(mi_disp_lhbm_fod_thread_destroy);

int mi_disp_lhbm_fod_allow_tx_lhbm(struct mtk_dsi *dsi,
		bool enable)
{
	struct disp_lhbm_fod *lhbm_fod = NULL;

	if (!dsi) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	if (!mi_disp_lhbm_fod_enabled(dsi)) {
		DISP_DEBUG("%s panel is not local hbm\n", mi_get_disp_id("primary"));
		return 0;
	}

	lhbm_fod = mi_get_disp_lhbm_fod(mi_get_disp_id("primary"));
	if (!lhbm_fod) {
		DISP_ERROR("Invalid lhbm_fod ptr\n");
		return -EINVAL;
	}

	if (lhbm_fod->dsi == dsi &&
		atomic_read(&lhbm_fod->allow_tx_lhbm) != enable) {
		atomic_set(&lhbm_fod->allow_tx_lhbm, enable);
		DISP_INFO("%s display allow_tx_lhbm = %d\n",
			mi_get_disp_id("primary"), enable);
		if (enable) {
			wake_up_interruptible(&lhbm_fod->fod_pending_wq);
			DISP_INFO("primary display wake up local disp_fod kthread\n");
		}
	}
	return 0;
}

int mi_disp_lhbm_fod_set_fingerprint_status(struct mtk_dsi *dsi,
		int fingerprint_status)
{
	struct disp_lhbm_fod *lhbm_fod = NULL;

	if (!dsi) {
		DISP_ERROR("Invalid dsi ptr\n");
		return -EINVAL;
	}

	if (!mi_disp_lhbm_fod_enabled(dsi)) {
		DISP_DEBUG("%s dsi is not local hbm\n", mi_get_disp_id("primary"));
		return 0;
	}

	lhbm_fod = mi_get_disp_lhbm_fod(mi_get_disp_id("primary"));
	if (!lhbm_fod) {
		DISP_ERROR("Invalid lhbm_fod ptr\n");
		return -EINVAL;
	}

	atomic_set(&lhbm_fod->fingerprint_status, fingerprint_status);

	DISP_UTC_INFO("set fingerprint_status = %s\n",
		get_fingerprint_status_name(fingerprint_status));

	return 0;
}

int mi_disp_lhbm_fod_update_layer_state(struct mtk_dsi *dsi,
		struct mi_layer_flags flags)
{
	struct disp_lhbm_fod *lhbm_fod = NULL;

	if (!dsi) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	if (!mi_disp_lhbm_fod_enabled(dsi)) {
		DISP_DEBUG("%s panel is not local hbm\n", mi_get_disp_id("primary"));
		return 0;
	}

	lhbm_fod = mi_get_disp_lhbm_fod(mi_get_disp_id("primary"));
	if (!lhbm_fod) {
		DISP_ERROR("Invalid lhbm_fod ptr\n");
		return -EINVAL;
	}

	spin_lock(&lhbm_fod->spinlock);
	lhbm_fod->layer_flags = flags;
	spin_unlock(&lhbm_fod->spinlock);

	return 0;
}

static int mi_disp_lhbm_fod_event_notify(struct disp_lhbm_fod *lhbm_fod, int fod_status)
{
	struct mtk_dsi *dsi = NULL;
	u32 fod_ui_ready = 0, refresh_rate = 0;
	u32 ui_ready_delay_frame = 0;
	u64 delay_us = 0;

	if (!lhbm_fod || !lhbm_fod->dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	dsi = lhbm_fod->dsi;

	if (!dsi){
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (fod_status == FOD_EVENT_DOWN &&
		dsi->mi_cfg.lhbm_ui_ready_delay_frame > 0) {
		refresh_rate = 120;
		if (is_aod_and_panel_initialized(dsi) &&
				dsi->mi_cfg.lhbm_ui_ready_delay_frame_aod)
			ui_ready_delay_frame = dsi->mi_cfg.lhbm_ui_ready_delay_frame_aod;
		else
			ui_ready_delay_frame = dsi->mi_cfg.lhbm_ui_ready_delay_frame;
		delay_us = 1000000 / refresh_rate * ui_ready_delay_frame;
		DISP_INFO("refresh_rate(%d), delay (%d) frame, delay_us(%llu)\n",
				refresh_rate, ui_ready_delay_frame, delay_us);
		usleep_range(delay_us, delay_us + 10);
	}

	if (fod_status == FOD_EVENT_DOWN) {
		if (lhbm_fod->target_brightness == LHBM_TARGET_BRIGHTNESS_WHITE_110NIT)
			fod_ui_ready = LOCAL_HBM_UI_READY | FOD_LOW_BRIGHTNESS_CAPTURE;
		else
			fod_ui_ready = LOCAL_HBM_UI_READY;
	} else {
		fod_ui_ready = LOCAL_HBM_UI_NONE;
	}

	if (atomic_read(&lhbm_fod->allow_tx_lhbm)) {
		mi_disp_feature_event_notify_by_type(mi_get_disp_id("primary"), MI_DISP_EVENT_FOD, 
			sizeof(fod_ui_ready), fod_ui_ready);
		DISP_INFO("%s display fod_ui_ready notify=%d\n",
			dsi->display_type, fod_ui_ready);
	} else {
		DISP_INFO("lhbm_fod->allow_tx_lhbm read false, dsi disabled");
		return -1;
	}

	DISP_INFO("%s display fod_ui_ready notify=%d\n",
		dsi->display_type, fod_ui_ready);

	return 0;
}

static int mi_disp_lhbm_fod_get_target_brightness(struct mtk_dsi *dsi)
{
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	int brightness_clone = 0;
	int target_brightness = LHBM_TARGET_BRIGHTNESS_WHITE_1000NIT;

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &dsi->mi_cfg;

	/* Heart rate detection, local hbm green 500nit */
	if (mi_cfg->feature_val[DISP_FEATURE_FP_STATUS] == HEART_RATE_START) {
		target_brightness = LHBM_TARGET_BRIGHTNESS_GREEN_500NIT;
		return target_brightness;
	}

	/* fingerprint enroll, local hbm white 1000nit */
	if (mi_cfg->feature_val[DISP_FEATURE_FP_STATUS] == ENROLL_START) {
		target_brightness = LHBM_TARGET_BRIGHTNESS_WHITE_1000NIT;
		return target_brightness;
	}

	/* fingerprint unlock, check low brightness status */
	if (mi_cfg->feature_val[DISP_FEATURE_LOW_BRIGHTNESS_FOD] &&
		mi_cfg->fod_low_brightness_allow) {
		if (is_aod_and_panel_initialized(dsi)) {
			if (mi_cfg->feature_val[DISP_FEATURE_SENSOR_LUX] <=
				mi_cfg->fod_low_brightness_lux_threshold)
				target_brightness = LHBM_TARGET_BRIGHTNESS_WHITE_110NIT;
		} else {
			brightness_clone = mi_cfg->real_brightness_clone;
			if (brightness_clone < mi_cfg->fod_low_brightness_clone_threshold
				&& (mi_cfg->feature_val[DISP_FEATURE_SENSOR_LUX] <=
					mi_cfg->fod_low_brightness_lux_threshold)) {
				target_brightness = LHBM_TARGET_BRIGHTNESS_WHITE_110NIT;
			}
		}
	} else {
		target_brightness = LHBM_TARGET_BRIGHTNESS_WHITE_1000NIT;
	}

	return target_brightness;
}

static int mi_disp_lhbm_fod_set_disp_param(struct disp_lhbm_fod *lhbm_fod, u32 op_code)
{

	struct mtk_dsi *dsi = NULL;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	struct disp_feature_ctl ctl;
	int rc = 0;

	if (!lhbm_fod || !lhbm_fod->dsi || !lhbm_fod->dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	memset(&ctl, 0, sizeof(struct disp_feature_ctl));
	dsi = lhbm_fod->dsi;

	//mutex_lock(&panel->panel_lock);

	mi_cfg = &dsi->mi_cfg;

	switch (op_code) {
	case MI_FOD_HBM_ON:
		ctl.feature_id = DISP_FEATURE_LOCAL_HBM;
		lhbm_fod->target_brightness = mi_disp_lhbm_fod_get_target_brightness(dsi);
		if (lhbm_fod->target_brightness == LHBM_TARGET_BRIGHTNESS_GREEN_500NIT) {
			ctl.feature_val = LOCAL_HBM_NORMAL_GREEN_500NIT;
		} else if (lhbm_fod->target_brightness == LHBM_TARGET_BRIGHTNESS_WHITE_1000NIT) {
			if (is_aod_and_panel_initialized(dsi) &&
				(dsi->mi_cfg.feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] == DOZE_BRIGHTNESS_HBM
				||dsi->mi_cfg.feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] == DOZE_BRIGHTNESS_LBM))
				ctl.feature_val = LOCAL_HBM_HLPM_WHITE_1000NIT;
			else
				ctl.feature_val = LOCAL_HBM_NORMAL_WHITE_1000NIT;
		} else if (lhbm_fod->target_brightness == LHBM_TARGET_BRIGHTNESS_WHITE_110NIT) {
			if (is_aod_and_panel_initialized(dsi) &&
				(dsi->mi_cfg.feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] == DOZE_BRIGHTNESS_HBM
				||dsi->mi_cfg.feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] == DOZE_BRIGHTNESS_LBM))
				ctl.feature_val = LOCAL_HBM_HLPM_WHITE_110NIT;
			else
				ctl.feature_val = LOCAL_HBM_NORMAL_WHITE_110NIT;
		} else {
			DISP_ERROR("invalid target_brightness = %d\n", lhbm_fod->target_brightness);
		}
		break;
	case MI_FOD_HBM_OFF:
		ctl.feature_id = DISP_FEATURE_LOCAL_HBM;
		if (mi_cfg->feature_val[DISP_FEATURE_FP_STATUS] == AUTH_STOP) {
			if (is_aod_and_panel_initialized(dsi))
				ctl.feature_val = LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE;
			else
				ctl.feature_val = LOCAL_HBM_OFF_TO_NORMAL;
		} else if (is_aod_and_panel_initialized(dsi)) {
			ctl.feature_val = LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT;
		} else {
			ctl.feature_val = LOCAL_HBM_OFF_TO_NORMAL;
		}
		break;
	default:
		break;
	}

	//mutex_unlock(&panel->panel_lock);

	//SDE_ATRACE_BEGIN("local_hbm_fod");
	rc = mi_dsi_display_set_disp_param(lhbm_fod->dsi, &ctl);
	//SDE_ATRACE_BEGIN("local_hbm_fod");

	return rc;

}

void mi_disp_lhbm_animal_status_update(struct mtk_dsi *dsi,
		bool is_layer_exit)
{
	struct disp_feature_ctl ctl;

	if (!dsi->mi_cfg.need_fod_animal_in_normal)
		return;

	ctl.feature_id = DISP_FEATURE_AOD_TO_NORMAL;
	if (is_layer_exit)
		ctl.feature_val = FEATURE_ON;
	else
		ctl.feature_val = FEATURE_OFF;

	DISP_INFO("mi_disp_lhbm_animal_status_update feature_val:%d\n", ctl.feature_val);
	mi_dsi_display_set_disp_param(dsi, &ctl);
}

static bool mi_disp_lhbm_fod_thread_should_wake(struct disp_lhbm_fod *lhbm_fod)
{
	bool should_wake = false;
	unsigned long flags;

	spin_lock_irqsave(&lhbm_fod->spinlock, flags);

	if (list_empty(&lhbm_fod->event_list) || !atomic_read(&lhbm_fod->allow_tx_lhbm))
		should_wake = false;
	else
		should_wake = true;

	spin_unlock_irqrestore(&lhbm_fod->spinlock, flags);

	return should_wake;
}

static int mi_disp_lhbm_fod_thread_fn(void *arg)
{
	int rc = 0;
	struct disp_lhbm_fod *lhbm_fod = (struct disp_lhbm_fod *)arg;
	struct lhbm_fod_event *entry = NULL, *temp = NULL;
	struct lhbm_fod_event fod_event;
	unsigned long flag = 0;
	u32 op_code;

	while (!kthread_should_stop()) {
		rc = wait_event_interruptible(lhbm_fod->fod_pending_wq,
				mi_disp_lhbm_fod_thread_should_wake(lhbm_fod));
		if (rc) {
			/* Some event woke us up */
			DISP_WARN("wait_event_interruptible rc = %d\n", rc);
			continue;
		}

		spin_lock_irqsave(&lhbm_fod->spinlock, flag);
		entry = list_last_entry(&lhbm_fod->event_list, struct lhbm_fod_event, link);
		DISP_INFO("from_touch(%d), fod_status(%d)\n", entry->from_touch, entry->fod_status);
		memcpy(&fod_event, entry, sizeof(fod_event));
		list_for_each_entry_safe(entry, temp, &lhbm_fod->event_list, link) {
			DISP_DEBUG("in list, from_touch(%d), fod_status(%d)\n",
					entry->from_touch, entry->fod_status);
			list_del(&entry->link);
			kfree(entry);
		}

		if (atomic_read(&lhbm_fod->last_fod_status) == fod_event.fod_status) {
			spin_unlock_irqrestore(&lhbm_fod->spinlock, flag);
			DISP_INFO("same fod event: %d, return\n", fod_event.fod_status);
		} else {
			atomic_set(&lhbm_fod->last_fod_status, fod_event.fod_status);
			if (fod_event.fod_status == FOD_EVENT_DOWN) {
				op_code = MI_FOD_HBM_ON;
			} else {
				op_code = MI_FOD_HBM_OFF;
			}
			spin_unlock_irqrestore(&lhbm_fod->spinlock, flag);

			if (fod_event.fod_status == FOD_EVENT_UP) {
				mi_disp_lhbm_fod_event_notify(lhbm_fod, FOD_EVENT_UP);
			}

			rc = mi_disp_lhbm_fod_set_disp_param(lhbm_fod, op_code);
			if (rc) {
				DISP_ERROR("lhbm_fod failed to set_disp_param, rc = %d\n", rc);
			} else if (fod_event.fod_status == FOD_EVENT_DOWN) {
				if (-1 == mi_disp_lhbm_fod_event_notify(lhbm_fod, FOD_EVENT_DOWN) && entry->from_touch) {
					atomic_set(&mi_disp_fod_status, 0);
					atomic_set(&lhbm_fod->last_fod_status, FOD_EVENT_UP);
				}
			}
		}
	}

	return 0;
}



int mi_disp_set_fod_queue_work(u32 fod_status, bool from_touch)
{
	struct disp_lhbm_fod *lhbm_fod = mi_get_disp_lhbm_fod(MI_DISP_PRIMARY);
	struct mtk_dsi *dsi = mi_get_primary_dsi_display();
	struct lhbm_fod_event *fod_event = NULL, *entry = NULL;
	unsigned long flags;
	int rc = 0;

#ifdef CONFIG_FACTORY_BUILD
	return 0;
#endif

	if (from_touch) {
		if (fod_status == atomic_read(&mi_disp_fod_status)) {
			DISP_INFO("%s Don't report after the first time anymore \n", __func__);
			return 0;
		}
		atomic_set(&mi_disp_fod_status, fod_status);
	}

	if (!dsi) {
		DISP_ERROR("invalid dsi ptr\n");
		return -EINVAL;
	}

	if (!mi_disp_lhbm_fod_enabled(dsi)) {
		DISP_DEBUG("%s panel is not local hbm\n", dsi->display_type);
		return 0;
	}

	if (!lhbm_fod || lhbm_fod->dsi != dsi) {
		DISP_ERROR("invalid lhbm_fod ptr\n");
		return -EINVAL;
	}

	DISP_INFO("start: from_touch(%d), fod_status(%d)\n", from_touch, fod_status);

	spin_lock_irqsave(&lhbm_fod->spinlock, flags);
	fod_event = kzalloc(sizeof(struct lhbm_fod_event), GFP_ATOMIC);
	if (!fod_event) {
		DISP_ERROR("failed to allocate memory for fod_event\n");
		rc = ENOMEM;
		goto exit;
	}

	fod_event->from_touch = from_touch;
	fod_event->fod_status = fod_status;
	INIT_LIST_HEAD(&fod_event->link);
	list_add_tail(&fod_event->link, &lhbm_fod->event_list);

	list_for_each_entry(entry, &lhbm_fod->event_list, link) {
		DISP_DEBUG("in list, from_touch(%d), fod_status(%d)\n",
			entry->from_touch, entry->fod_status);
	}

	DISP_UTC_INFO("from_touch(%d), fod_status(%d)\n", from_touch, fod_status);
	wake_up_interruptible(&lhbm_fod->fod_pending_wq);

exit:
	spin_unlock_irqrestore(&lhbm_fod->spinlock, flags);
	return rc;
}

EXPORT_SYMBOL_GPL(mi_disp_set_fod_queue_work);
