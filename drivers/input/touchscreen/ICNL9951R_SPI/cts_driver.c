#ifdef    CONFIG_CTS_I2C_HOST
#define LOG_TAG         "I2CDrv"
#else
#define LOG_TAG         "SPIDrv"
#endif

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_sysfs.h"
#include "cts_charger_detect.h"
#include "cts_earjack_detect.h"
#include "cts_oem.h"
#include "cts_strerror.h"
#include "cts_firmware.h"
#include "cts_tcs.h"

#ifdef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
#include "tpd.h"
#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#else /* CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED */
#ifdef CFG_CTS_DRM_NOTIFIER
#include <drm/drm_panel.h>
static struct drm_panel *active_panel;
static int check_dt(struct device_node *np);
#endif /* CFG_CTS_DRM_NOTIFIER */
#endif /* CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED */

#if defined(ICNL_MTK_DRM_NOTIFY)
#include "../../../gpu/drm/mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../../../gpu/drm/mediatek/mediatek_v2/mtk_disp_notify.h"
#endif /* ICNL_MTK_DRM_NOTIFY */

static void cts_resume_work_func(struct work_struct *work);

bool cts_show_debug_log;
#ifdef CTS_MTK_GET_PANEL
static char *active_panel_name;
#endif

module_param_named(debug_log, cts_show_debug_log, bool, 0660);
MODULE_PARM_DESC(debug_log, "Show debug log control");

struct chipone_ts_data *g_cts_data = NULL;

struct spi_delay cts_cs_setup = {
	.value = 300,
	.unit = 1,
};
struct spi_delay cts_cs_hold = {
	.value = 300,
	.unit = 1,
};

int cts_suspend(struct chipone_ts_data *cts_data)
{
    int ret;

    cts_info("Suspend");

#ifdef CONFIG_CTS_TP_PROXIMITY
	if (cts_is_proximity_enable(&cts_data->cts_dev))
	{
	    cts_info("Proximity enable mode is ture to return in the cts_suspend");
		cts_plat_release_all_touch(cts_data->pdata);
		return 0;
	}
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE)
	update_palm_sensor_value(0);
	if (g_cts_data->palm_sensor_switch) {
		cts_info("palm sensor on status, switch to off\n");
		cts_disable_pocket_mode(&cts_data->cts_dev);
		g_cts_data->palm_sensor_switch = false;
	}
#endif

    cts_lock_device(&cts_data->cts_dev);
    ret = cts_suspend_device(&cts_data->cts_dev);
    cts_unlock_device(&cts_data->cts_dev);

    if (ret)
        cts_err("Suspend device failed %d", ret);

    ret = cts_stop_device(&cts_data->cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        return ret;
    }
#ifdef CFG_CTS_GESTURE
    /* Enable IRQ wake if gesture wakeup enabled */
    if (cts_is_gesture_wakeup_enabled(&cts_data->cts_dev)) {
        ret = cts_plat_enable_irq_wake(cts_data->pdata);
        if (ret) {
            cts_err("Enable IRQ wake failed %d", ret);
            return ret;
        }
        ret = cts_plat_enable_irq(cts_data->pdata);
        if (ret) {
            cts_err("Enable IRQ failed %d", ret);
            return ret;
        }
    }
#endif /* CFG_CTS_GESTURE */

/** - To avoid waking up while not sleeping,
 *delay 20ms to ensure reliability
 */
    msleep(20);

    return 0;
}

int cts_resume(struct chipone_ts_data *cts_data)
{
    int ret;

    cts_info("Resume");

#ifdef CONFIG_CTS_TP_PROXIMITY
	if (cts_is_proximity_enable(&cts_data->cts_dev))
	{
        cts_plat_release_all_touch(cts_data->pdata);
	}
#endif

#ifdef CFG_CTS_GESTURE
    if (cts_is_gesture_wakeup_enabled(&cts_data->cts_dev)) {
        ret = cts_plat_disable_irq_wake(cts_data->pdata);
        if (ret)
            cts_warn("Disable IRQ wake failed %d", ret);
        ret = cts_plat_disable_irq(cts_data->pdata);
        if (ret < 0)
            cts_err("Disable IRQ failed %d", ret);
    }
#endif /* CFG_CTS_GESTURE */

    cts_lock_device(&cts_data->cts_dev);
    ret = cts_resume_device(&cts_data->cts_dev);
    cts_unlock_device(&cts_data->cts_dev);
    if (ret) {
        cts_warn("Resume device failed %d", ret);
        return ret;
    }

#ifdef CFG_CTS_GESTURE
	if(cts_is_gesture_wakeup_enabled(&cts_data->cts_dev) &&
	    cts_is_device_enabled(&cts_data->cts_dev)){
		if (cts_plat_enable_irq(cts_data->cts_dev.pdata)) {
			cts_err("Enable IRQ failed");
		}
	}
#endif

    ret = cts_start_device(&cts_data->cts_dev);
    if (ret) {
        cts_err("Start device failed %d", ret);
        return ret;
    }

    return 0;
}

static void cts_resume_work_func(struct work_struct *work)
{
    struct chipone_ts_data *cts_data =
        container_of(work, struct chipone_ts_data, ts_resume_work);
    cts_info("%s", __func__);
    cts_resume(cts_data);
}

#ifndef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
#ifdef CONFIG_CTS_PM_FB_NOTIFIER
#if defined(CFG_CTS_DRM_NOTIFIER)
static int fb_notifier_callback(struct notifier_block *nb,
        unsigned long action, void *data)
{
    volatile int blank;
    const struct cts_platform_data *pdata =
    container_of(nb, struct cts_platform_data, fb_notifier);
    struct chipone_ts_data *cts_data =
    container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);
    struct drm_panel_notifier *evdata = data;

    cts_info("FB notifier callback");
    if (!evdata || !evdata->data || !cts_data)
        return 0;

    blank = *(int *)evdata->data;
    cts_info("action=%lu, blank=%d", action, blank);

    if (action == DRM_PANEL_EARLY_EVENT_BLANK) {
        if (blank == DRM_PANEL_BLANK_POWERDOWN)
            cts_suspend(cts_data);
    } else {
        blank = *(int *)evdata->data;
        if (action == DRM_PANEL_EVENT_BLANK) {
            if (blank == DRM_PANEL_BLANK_UNBLANK)
                /* cts_resume(cts_data); */
                queue_work(cts_data->workqueue,
                    &cts_data->ts_resume_work);
        }
    }

    return 0;
}
#elif defined(ICNL_MTK_DRM_NOTIFY)
static int fb_notifier_callback(struct notifier_block *nb,
                     unsigned long action, void *data)
{

    const struct cts_platform_data *pdata =
    container_of(nb, struct cts_platform_data, fb_notifier);
    struct chipone_ts_data *cts_data =
    container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);
    int *blank = (int *)data;

    cts_info("DRM notifier callback");
     if (!blank) {
        cts_err("Invalid blank\n");
        return -1;
    }

    cts_info("action=%lu, blank=%d", action, *blank);
    if (action == MTK_DRM_EARLY_EVENT_BLANK) {
        if (*blank == MTK_DRM_BLANK_POWERDOWN)
            cts_suspend(cts_data);
    } else {
        if (action == MTK_DRM_EVENT_BLANK) {
            if (*blank == MTK_DRM_BLANK_UNBLANK)
                /* cts_resume(cts_data); */
                queue_work(cts_data->workqueue,
                    &cts_data->ts_resume_work);
        }
    }

    return 0;
}
#else
static int fb_notifier_callback(struct notifier_block *nb,
        unsigned long action, void *data)
{
    volatile int blank;
    const struct cts_platform_data *pdata =
    container_of(nb, struct cts_platform_data, fb_notifier);
    struct chipone_ts_data *cts_data =
    container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);
    struct fb_event *evdata = data;

    cts_info("FB notifier callback");

    if (evdata && evdata->data) {
        if (action == FB_EVENT_BLANK) {
            blank = *(int *)evdata->data;
            if (blank == FB_BLANK_UNBLANK) {
                /* cts_resume(cts_data); */
                queue_work(cts_data->workqueue,
                    &cts_data->ts_resume_work);
                return NOTIFY_OK;
            }
        } else if (action == FB_EARLY_EVENT_BLANK) {
            blank = *(int *)evdata->data;
            if (blank == FB_BLANK_POWERDOWN) {
                cts_suspend(cts_data);
                return NOTIFY_OK;
            }
        }
    }

    return NOTIFY_DONE;
}
#endif

static int cts_init_pm_fb_notifier(struct chipone_ts_data *cts_data)
{
    cts_info("Init FB notifier");

    cts_data->pdata->fb_notifier.notifier_call = fb_notifier_callback;

#if defined(CFG_CTS_DRM_NOTIFIER)
    {
        int ret = -ENODEV;

        if (active_panel) {
            ret = drm_panel_notifier_register(active_panel,
                    &cts_data->pdata->fb_notifier);
            if (ret)
                cts_err("register drm_notifier failed. ret=%d\n", ret);
        }
        return ret;
    }
#elif defined(ICNL_MTK_DRM_NOTIFY)
    {
    int ret = -ENODEV;

    ret = mtk_disp_notifier_register(CFG_CTS_DEVICE_NAME, &cts_data->pdata->fb_notifier);
    if (ret)
        cts_err("register mtk_disp_notifier failed\n");
    return ret;
    }
#else
    return fb_register_client(&cts_data->pdata->fb_notifier);
#endif
}

static int cts_deinit_pm_fb_notifier(struct chipone_ts_data *cts_data)
{
    cts_info("Deinit FB notifier");
#if defined(CFG_CTS_DRM_NOTIFIER)
    {
        int ret = 0;

        if (active_panel) {
            ret = drm_panel_notifier_unregister(active_panel,
                    &cts_data->pdata->fb_notifier);
            if (ret)
                cts_err("Error occurred while unregistering drm_notifier.\n");
        }
        return ret;
    }

#elif defined(ICNL_MTK_DRM_NOTIFY)
    {

        int ret = 0;

        ret = mtk_disp_notifier_unregister(&cts_data->pdata->fb_notifier);
        if (ret)
            cts_err("Error occurred while mtk_disp_notifier_unregister.\n");
       
        return ret;

    }
#else
    return fb_unregister_client(&cts_data->pdata->fb_notifier);
#endif
}
#endif /* CONFIG_CTS_PM_FB_NOTIFIER */

#ifdef CFG_CTS_DRM_NOTIFIER
static int check_dt(struct device_node *np)
{
    int i;
    int count;
    struct device_node *node;
    struct drm_panel *panel;

    count = of_count_phandle_with_args(np, "panel", NULL);
    if (count <= 0)
        return 0;

    for (i = 0; i < count; i++) {
        node = of_parse_phandle(np, "panel", i);
        panel = of_drm_find_panel(node);
        of_node_put(node);
        if (!IS_ERR(panel)) {
            cts_info("check active_panel");
            active_panel = panel;
            return 0;
        }
    }
    if (node)
        cts_err("%s: %s not actived", __func__, node->name);
    return -ENODEV;
}

static int check_default_tp(struct device_node *dt, const char *prop)
{
    const char *active_tp;
    const char *compatible;
    char *start;
    int ret;

    ret = of_property_read_string(dt->parent, prop, &active_tp);
    if (ret) {
        cts_err("%s:fail to read %s %d", __func__, prop, ret);
        return -ENODEV;
    }

    ret = of_property_read_string(dt, "compatible", &compatible);
    if (ret < 0) {
        cts_err("%s:fail to read %s %d", __func__, "compatible", ret);
        return -ENODEV;
    }

    start = strnstr(active_tp, compatible, strlen(active_tp));
    if (start == NULL) {
        cts_err("no match compatible, %s, %s", compatible, active_tp);
        ret = -ENODEV;
    }

    return ret;
}
#endif
#endif

#ifdef CTS_MTK_GET_PANEL
char panel_name[50] = { 0 };

static int cts_get_panel(void)
{
    int ret = -1;

    cts_info("Enter cts_get_panel");
    if (saved_command_line) {
        char *sub;
        char key_prefix[] = "mipi_mot_vid_";
        char ic_prefix[] = "icnl";

        cts_info("saved_command_line is %s", saved_command_line);
        sub = strstr(saved_command_line, key_prefix);
        if (sub) {
            char *d;
            int n, len, len_max = 50;

            d = strstr(sub, " ");
            if (d)
                n = strlen(sub) - strlen(d);
            else
                n = strlen(sub);

            if (n > len_max)
                len = len_max;
            else
                len = n;

            strncpy(panel_name, sub, len);
            active_panel_name = panel_name;
            if (strstr(active_panel_name, ic_prefix))
                cts_info("active_panel_name=%s", active_panel_name);
            else {
                cts_info("Not chipone panel!");
                return ret;
            }

        } else {
            cts_info("chipone active panel not found!");
            return ret;
        }
    } else {
        cts_info("saved_command_line null!");
        return ret;
    }

    return 0;
}
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE)
static struct xiaomi_touch_interface xiaomi_touch_interfaces;
static int icnl_palm_sensor_write(int value)
{
	int ret = 0;
	cts_info("enter value = %d palm_sensor_switch = %d\n", value, g_cts_data->palm_sensor_switch);
	g_cts_data->palm_sensor_switch = value;
	if (g_cts_data->pm_suspend) {
		cts_info("tp has pm_suspend status\n");
		return 0;
	}
	if (!(g_cts_data->cts_dev.rtdata.suspended)) {
        if (g_cts_data->palm_sensor_switch == true) {
            cts_enable_pocket_mode(&g_cts_data->cts_dev);
        } else {
            cts_disable_pocket_mode(&g_cts_data->cts_dev);
        }

	}
	return ret;
}
static void cts_init_touchmode_data(void)
{
	int i;
	cts_info("%s,ENTER\n", __func__);
	/* Touch Game Mode Switch */
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_CUR_VALUE] = 0;
	/* Acitve Mode */
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_CUR_VALUE] = 0;
	/* Sensivity */
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_CUR_VALUE] = 0;
	/* Tolerance */
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_CUR_VALUE] = 0;
	/* Panel orientation*/
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_DEF_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_CUR_VALUE] = 0;
	/* Edge filter area*/
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_CUR_VALUE] = 0;
	for (i = 0; i < Touch_Mode_NUM; i++) {
		cts_info("mode:%d, set cur:%d, get cur:%d, def:%d min:%d max:%d\n",
			i,
			xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_MIN_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_MAX_VALUE]);
	}
	return;
}
static int cts_set_cur_value(int nvt_mode, int cts_value)
{
	uint8_t temp_value = 0;
	uint8_t ret = 0;

	if (nvt_mode < Touch_Mode_NUM && nvt_mode >= 0) {
		xiaomi_touch_interfaces.touch_mode[nvt_mode][SET_CUR_VALUE] = cts_value;
		if (xiaomi_touch_interfaces.touch_mode[nvt_mode][SET_CUR_VALUE] >
				xiaomi_touch_interfaces.touch_mode[nvt_mode][GET_MAX_VALUE]) {
			xiaomi_touch_interfaces.touch_mode[nvt_mode][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[nvt_mode][GET_MAX_VALUE];
		} else if (xiaomi_touch_interfaces.touch_mode[nvt_mode][SET_CUR_VALUE] <
				xiaomi_touch_interfaces.touch_mode[nvt_mode][GET_MIN_VALUE]) {
			xiaomi_touch_interfaces.touch_mode[nvt_mode][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[nvt_mode][GET_MIN_VALUE];
		}
		switch (nvt_mode) {
		case Touch_Game_Mode:
			temp_value =
				xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE];
			cts_tcs_set_game_mode(&g_cts_data->cts_dev, temp_value);
			break;
		case Touch_Active_MODE:
			break;
		case Touch_UP_THRESHOLD:
			temp_value =
				xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE];
			cts_tcs_set_game_threshold_value(&g_cts_data->cts_dev, temp_value);
			break;
		case Touch_Tolerance:
			temp_value =
				xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE];
			cts_tcs_set_game_tolerance_value(&g_cts_data->cts_dev, temp_value);
			break;
		case Touch_Edge_Filter:
			/* filter 0,1,2,3 = default,1,2,3 level*/
			temp_value =
				xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE];
			cts_tcs_set_game_edge_inhibition_value(&g_cts_data->cts_dev, temp_value);
			break;
		case Touch_Panel_Orientation:
			/* 0,1,2,3 = 0, 90, 180,270 Counter-clockwise*/
			temp_value =
				xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE];
			if (temp_value == 0 ) {
				cts_tcs_set_panel_direction(&g_cts_data->cts_dev, 0);
			} else if (temp_value == 1) {
				cts_tcs_set_panel_direction(&g_cts_data->cts_dev, 1);
			} else if (temp_value == 2) {
				cts_tcs_set_panel_direction(&g_cts_data->cts_dev, 3);
			} else if (temp_value == 3) {
				cts_tcs_set_panel_direction(&g_cts_data->cts_dev, 2);
			}
			break;
        case Touch_Doubletap_Mode:
			if (cts_value == 0) {
				cts_disable_gesture_wakeup(g_cts_data->pdata->cts_dev);
			    cts_info("gesture disabled:%d", g_cts_data->pdata->cts_dev->rtdata.gesture_wakeup_enabled);
			} else if (cts_value == 1) {
				cts_enable_gesture_wakeup(g_cts_data->pdata->cts_dev);
			    cts_info("gesture enabled:%d", g_cts_data->pdata->cts_dev->rtdata.gesture_wakeup_enabled);
			}
			panel_60hz_check_gesture_mode(g_cts_data->pdata->cts_dev->rtdata.gesture_wakeup_enabled);
	        panel_90hz_check_gesture_mode(g_cts_data->pdata->cts_dev->rtdata.gesture_wakeup_enabled);
	        panel_90hz_check_gesture_thinning_mode(g_cts_data->pdata->cts_dev->rtdata.gesture_wakeup_enabled);
			break;
		default:
			/* Don't support */
			cts_info("Cmd is not support,skip!");
			break;
		};
	}
	return ret;
}

static int cts_get_mode_value(int mode, int value_type)
{
	int value = -1;
	if (mode < Touch_Mode_NUM && mode >= 0)
		value = xiaomi_touch_interfaces.touch_mode[mode][value_type];
	else
		cts_err("%s, don't support\n", __func__);
	return value;
}
static int cts_get_mode_all(int mode, int *value)
{
	if (mode < Touch_Mode_NUM && mode >= 0) {
		value[0] = xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE];
		value[1] = xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
		value[2] = xiaomi_touch_interfaces.touch_mode[mode][GET_MIN_VALUE];
		value[3] = xiaomi_touch_interfaces.touch_mode[mode][GET_MAX_VALUE];
	} else {
		cts_err("%s, don't support\n",  __func__);
	}
	cts_info("%s, mode:%d, value:%d:%d:%d:%d\n", __func__, mode, value[0],
					value[1], value[2], value[3]);
	return 0;
}

static int cts_reset_mode(int mode)
{
	int i = 0;
	cts_info("cts_reset_mode enter\n");
	if (mode < Touch_Report_Rate && mode > 0) {
		xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
			xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
		cts_set_cur_value(mode, xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE]);
	} else if (mode == 0) {
		for (i = 0; i <= Touch_Report_Rate; i++) {
			if (i == Touch_Panel_Orientation) {
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE];
			} else {
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE];
			}
			cts_set_cur_value(i, xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE]);
		}
	} else {
		cts_err("%s, don't support\n",  __func__);
	}
	cts_info("%s, mode:%d\n",  __func__, mode);
	return 0;
}
#endif /*CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE*/

#if IS_ENABLED(CONFIG_MIEV)
static void cts_touch_dfs_test(int value){
	switch(value){
		case TOUCH_EVENT_TRANSFER_ERR:
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_TRANSFER_ERR, 0, "TpTransferErr", "ICNL", -1);
			break;
		case TOUCH_EVENT_FWLOAD_ERR:
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_FWLOAD_ERR, 0, "TpFirmwareLoadFail", "ICNL", -1);
			break;
		case TOUCH_EVENT_PARAM_ERR:
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_PARAM_ERR, 0, "TpParamParseFail", "ICNL", ERROR_REGULATOR_INIT);
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_PARAM_ERR, 0, "TpParamParseFail", "ICNL", ERROR_GPIO_REQUEST);
			break;
		case TOUCH_EVENT_OPENTEST_FAIL:
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_OPENTEST_FAIL, 0, "TpOpenTestFail", "ICNL", -1);
			break;
		case TOUCH_EVENT_SHORTTEST_FAIL:
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_SHORTTEST_FAIL, 0, "TpShortTestFail", "ICNL", -1);
			break;
		default:
			cts_err("don't support touch dfs test");
			break;
	}
}
#endif

#ifdef CONFIG_CTS_I2C_HOST
static int cts_driver_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
#else
static int cts_driver_probe(struct spi_device *client)
#endif
{
    struct chipone_ts_data *cts_data = NULL;
    struct cts_device *cts_dev = NULL;
    struct cts_firmware *firmware = NULL;
    int ret = 0;

#ifdef CTS_MTK_GET_PANEL
    ret = cts_get_panel();
    if (ret) {
        cts_info("MTK get chipone panel error");
        return ret;
    }
#endif

#ifdef CFG_CTS_DRM_NOTIFIER
    {
        struct device_node *dp = client->dev.of_node;

        if (check_dt(dp)) {
            if (!check_default_tp(dp, "qcom,i2c-touch-active"))
                ret = -EPROBE_DEFER;
            else
                ret = -ENODEV;

            cts_err("%s: %s not actived\n", __func__, dp->name);
            return ret;
        }
    }
#endif

#ifdef CONFIG_CTS_I2C_HOST
    if (client == NULL) {
        cts_err("Probe i2c client = NULL");
        return -EINVAL;
    }
    cts_info("Probe i2c client: name='%s' addr=0x%02x flags=0x%02x irq=%d",
            client->name, client->addr, client->flags, client->irq);

#if !defined(CONFIG_MTK_PLATFORM)
    if (client->addr != CTS_DEV_NORMAL_MODE_I2CADDR) {
        cts_err("Probe i2c addr 0x%02x != driver config addr 0x%02x",
                client->addr, CTS_DEV_NORMAL_MODE_I2CADDR);
        return -ENODEV;
    };
#endif

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        cts_err("Check functionality failed");
        return -ENODEV;
    }
#else
    if (client == NULL) {
        cts_info("Probe spi client = NULL");
        return -EINVAL;
    }
    cts_info("Probe spi device '%s': "
             "mode='%u' speed=%u bits_per_word=%u "
             "chip_select=%u, irq=%d",
        client->modalias, client->mode, client->max_speed_hz, client->bits_per_word,
        client->chip_select, client->irq);
#endif

    cts_data = kzalloc(sizeof(struct chipone_ts_data), GFP_KERNEL);
    if (cts_data == NULL) {
        cts_err("Allocate chipone_ts_data failed");
        return -ENOMEM;
    }

    cts_data->pdata = kzalloc(sizeof(struct cts_platform_data), GFP_KERNEL);
    if (cts_data->pdata == NULL) {
        cts_err("Allocate cts_platform_data failed");
        ret = -ENOMEM;
        goto err_free_cts_data;
    }

#ifdef CONFIG_CTS_I2C_HOST
    i2c_set_clientdata(client, cts_data);
    cts_data->i2c_client = client;
    cts_data->device = &client->dev;
#else
    spi_set_drvdata(client, cts_data);
    cts_data->spi_client = client;
    cts_data->device = &client->dev;
    memcpy(&cts_data->spi_client->cs_setup, &cts_cs_setup, sizeof(struct spi_delay));
    memcpy(&cts_data->spi_client->cs_hold, &cts_cs_hold, sizeof(struct spi_delay));
#endif

    ret = cts_init_platform_data(cts_data->pdata, client);
    if (ret) {
        cts_err("Init platform data failed %d", ret);
        goto err_free_pdata;
    }

    cts_data->cts_dev.pdata = cts_data->pdata;
    cts_data->pdata->cts_dev = &cts_data->cts_dev;
    cts_dev = cts_data->pdata->cts_dev;
    g_cts_data = cts_data;

    firmware = kzalloc(sizeof(struct cts_firmware), GFP_KERNEL);
    if (firmware == NULL) {
        cts_err("Create firmware buffer failed");
        goto err_free_firmware_buffer;
    }

    firmware->data = kzalloc(CFG_CTS_FIRMWARE_SIZE, GFP_KERNEL);
    if (firmware->data == NULL) {
        cts_err("Create firmware data buffer failed");
        goto err_free_firmware_data_buffer;
    }

    cts_dev->firmware = firmware;

    cts_data->workqueue =
        create_singlethread_workqueue(CFG_CTS_DEVICE_NAME "-workqueue");
    if (cts_data->workqueue == NULL) {
        cts_err("Create workqueue failed");
        ret = -ENOMEM;
        goto err_deinit_platform_data;
    }
    get_fw_name_match_lcm_name();

#ifdef CONFIG_CTS_ESD_PROTECTION
    cts_data->esd_workqueue =
        create_singlethread_workqueue(CFG_CTS_DEVICE_NAME "-esd_workqueue");
    if (cts_data->esd_workqueue == NULL) {
        cts_err("Create esd workqueue failed");
        ret = -ENOMEM;
        goto err_destroy_workqueue;
    }
#endif

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    cts_data->heart_workqueue =
        create_singlethread_workqueue(CFG_CTS_DEVICE_NAME "-heart_workqueue");
    if (cts_data->heart_workqueue == NULL) {
        cts_err("Create heart workqueue failed");
        ret = -ENOMEM;
        goto err_destroy_esd_workqueue;
    }
#endif

    ret = cts_plat_request_resource(cts_data->pdata);
    if (ret < 0) {
        cts_err("Request resource failed %d", ret);
        goto err_destroy_heart_workqueue;
    }

    ret = cts_reset_device(cts_dev);
    if (ret < 0) {
        cts_err("Reset device failed %d", ret);
        goto err_free_resource;
    }

    ret = cts_probe_device(cts_dev);
    if (ret) {
        cts_err("Probe device failed %d", ret);
        goto err_free_resource;
    }

    ret = cts_plat_init_touch_device(cts_data->pdata);
    if (ret < 0) {
        cts_err("Init touch device failed %d", ret);
        goto err_free_resource;
    }

    ret = cts_plat_init_vkey_device(cts_data->pdata);
    if (ret < 0) {
        cts_err("Init vkey device failed %d", ret);
        goto err_deinit_touch_device;
    }

    ret = cts_plat_init_gesture(cts_data->pdata);
    if (ret < 0) {
        cts_err("Init gesture failed %d", ret);
        goto err_deinit_vkey_device;
    }

    cts_init_esd_protection(cts_data);

    ret = cts_tool_init(cts_data);
    if (ret < 0)
        cts_warn("Init tool node failed %d", ret);

    ret = cts_sysfs_add_device(&client->dev);
    if (ret < 0)
        cts_warn("Add sysfs entry for device failed %d", ret);

#ifdef CTS_PATCH_COMERR_PM
    init_completion(&cts_data->pm_completion);
    cts_data->pm_suspend = false;
#endif

#ifndef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
#ifdef CONFIG_CTS_PM_FB_NOTIFIER
    ret = cts_init_pm_fb_notifier(cts_data);
    if (ret) {
        cts_err("Init FB notifier failed %d", ret);
        goto err_deinit_sysfs;
    }
#endif
#endif

    ret = cts_plat_request_irq(cts_data->pdata);
    if (ret < 0) {
        cts_err("Request IRQ failed %d", ret);
        goto err_register_fb;
    }

#ifdef CONFIG_CTS_CHARGER_DETECT
    ret = cts_charger_detect_init(cts_data);
    if (ret)
        cts_err("Init charger detect failed %d", ret);
        /* Ignore this error */
#endif

#ifdef CONFIG_CTS_EARJACK_DETECT
    ret = cts_earjack_detect_init(cts_data);
    if (ret) {
        cts_err("Init earjack detect failed %d", ret);
        // Ignore this error
    }
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE)
    memset(&xiaomi_touch_interfaces, 0x00, sizeof(struct xiaomi_touch_interface));
    xiaomi_touch_interfaces.palm_sensor_write = icnl_palm_sensor_write;
    xiaomi_touch_interfaces.setModeValue = cts_set_cur_value;
    xiaomi_touch_interfaces.getModeValue = cts_get_mode_value;
    xiaomi_touch_interfaces.getModeAll = cts_get_mode_all;
    xiaomi_touch_interfaces.resetMode = cts_reset_mode;
#if IS_ENABLED(CONFIG_MIEV)
	xiaomi_touch_interfaces.touch_dfs_test = cts_touch_dfs_test;
#endif
    cts_init_touchmode_data();
    xiaomitouch_register_modedata(&xiaomi_touch_interfaces);
#endif

#ifdef CONFIG_CTS_TP_PROXIMITY
	ret = cts_register_psenable_callback();
	if (ret) {
        cts_err("cts %s fail ret = %d", __func__, ret);
    }
#endif

    ret = cts_oem_init(cts_data);
    if (ret < 0) {
        cts_warn("Init oem specific faild %d", ret);
        goto err_deinit_oem;
    }
    ret = proc_node_init(cts_data);
    if (ret < 0) {
        cts_warn("Init oem specific faild %d", ret);
        goto err_deinit_openshort;
    }
#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    INIT_DELAYED_WORK(&cts_data->heart_work, cts_heartbeat_mechanism_work);
#endif
    INIT_WORK(&cts_data->ts_resume_work, cts_resume_work_func);

    /* Init firmware upgrade work and schedule */
    INIT_DELAYED_WORK(&cts_data->fw_upgrade_work, cts_firmware_upgrade_work);
    queue_delayed_work(cts_data->workqueue, &cts_data->fw_upgrade_work,
            msecs_to_jiffies(5 * 1000));

#ifdef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
#ifdef CONFIG_MTK_PLATFORM
    tpd_load_status = 1;
#endif /* CONFIG_MTK_PLATFORM */
#endif

    return 0;

err_deinit_oem:
    cts_oem_deinit(cts_data);

#ifdef CONFIG_CTS_CHARGER_DETECT
    cts_charger_detect_deinit(cts_data);
#endif
#ifdef CONFIG_CTS_EARJACK_DETECT
    cts_earjack_detect_deinit(cts_data);
#endif

    cts_plat_free_irq(cts_data->pdata);
err_deinit_openshort:
    proc_node_deinit(cts_data);
err_register_fb:
#ifndef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
#ifdef CONFIG_CTS_PM_FB_NOTIFIER
    cts_deinit_pm_fb_notifier(cts_data);
err_deinit_sysfs:
#endif
#endif

    cts_sysfs_remove_device(&client->dev);
#ifdef CONFIG_CTS_LEGACY_TOOL
    cts_tool_deinit(cts_data);
#endif

#ifdef CONFIG_CTS_ESD_PROTECTION
    cts_deinit_esd_protection(cts_data);
#endif

#ifdef CFG_CTS_GESTURE
    cts_plat_deinit_gesture(cts_data->pdata);
#endif

err_deinit_vkey_device:
#ifdef CONFIG_CTS_VIRTUALKEY
    cts_plat_deinit_vkey_device(cts_data->pdata);
#endif

err_deinit_touch_device:
    cts_plat_deinit_touch_device(cts_data->pdata);

err_free_resource:
    cts_plat_free_resource(cts_data->pdata);

err_destroy_heart_workqueue:
#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    destroy_workqueue(cts_data->heart_workqueue);
err_destroy_esd_workqueue:
#endif

#ifdef CONFIG_CTS_ESD_PROTECTION
    destroy_workqueue(cts_data->esd_workqueue);
err_destroy_workqueue:
#endif
    destroy_workqueue(cts_data->workqueue);

err_deinit_platform_data:
    if (firmware->data) {
        kfree(firmware->data);
        firmware->data = NULL;
    }

err_free_firmware_data_buffer:
    if (firmware) {
        kfree(firmware);
        firmware = NULL;
    }

err_free_firmware_buffer:
#ifndef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
    cts_deinit_platform_data(cts_data->pdata);
#endif
err_free_pdata:
    kfree(cts_data->pdata);
err_free_cts_data:
    kfree(cts_data);

    cts_err("Probe failed %d", ret);

    return ret;
}

#ifdef CONFIG_CTS_I2C_HOST
static int cts_driver_remove(struct i2c_client *client)
#else
static void cts_driver_remove(struct spi_device *client)
#endif
{
    struct chipone_ts_data *cts_data;
    int ret = 0;

    cts_info("Remove");

#ifdef CONFIG_CTS_I2C_HOST
    cts_data = (struct chipone_ts_data *)i2c_get_clientdata(client);
#else
    cts_data = (struct chipone_ts_data *)spi_get_drvdata(client);
#endif
    if (cts_data) {
        ret = cts_stop_device(&cts_data->cts_dev);
        if (ret)
            cts_warn("Stop device failed %d", ret);

#ifdef CONFIG_CTS_CHARGER_DETECT
        cts_charger_detect_deinit(cts_data);
#endif

#ifdef CONFIG_CTS_EARJACK_DETECT
        cts_earjack_detect_deinit(cts_data);
#endif

        cts_plat_free_irq(cts_data->pdata);

#ifndef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
#ifdef CONFIG_CTS_PM_FB_NOTIFIER
        cts_deinit_pm_fb_notifier(cts_data);
#endif
#endif

        cts_tool_deinit(cts_data);

        cts_sysfs_remove_device(&client->dev);

        cts_deinit_esd_protection(cts_data);

        cts_plat_deinit_touch_device(cts_data->pdata);

        cts_plat_deinit_vkey_device(cts_data->pdata);

        cts_plat_deinit_gesture(cts_data->pdata);

        cts_plat_free_resource(cts_data->pdata);

        cts_oem_deinit(cts_data);
        proc_node_deinit(cts_data);

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
        if (cts_data->heart_workqueue)
            destroy_workqueue(cts_data->heart_workqueue);
#endif

#ifdef CONFIG_CTS_ESD_PROTECTION
        if (cts_data->esd_workqueue)
            destroy_workqueue(cts_data->esd_workqueue);
#endif

        if (cts_data->workqueue)
            destroy_workqueue(cts_data->workqueue);

        if (cts_data->cts_dev.firmware->data) {
            kfree(cts_data->cts_dev.firmware->data);
            cts_data->cts_dev.firmware->data = NULL;
        }

        if (cts_data->cts_dev.firmware) {
            kfree(cts_data->cts_dev.firmware);
            cts_data->cts_dev.firmware = NULL;
        }

#ifndef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
        cts_deinit_platform_data(cts_data->pdata);
#endif
        if (cts_data->pdata)
            kfree(cts_data->pdata);

        kfree(cts_data);
    } else {
        cts_warn("Chipone i2c driver remove while NULL chipone_ts_data");
    }
}

#ifndef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
#ifdef CONFIG_CTS_PM_LEGACY
static int cts_i2c_driver_suspend(struct device *dev, pm_message_t state)
{
    cts_info("Suspend by legacy power management");
    return cts_suspend(dev_get_drvdata(dev));
}

static int cts_i2c_driver_resume(struct device *dev)
{
    cts_info("Resume by legacy power management");
    return cts_resume(dev_get_drvdata(dev));
}
#endif /* CONFIG_CTS_PM_LEGACY */

#ifdef CONFIG_CTS_PM_GENERIC
static int cts_i2c_driver_pm_suspend(struct device *dev)
{
    cts_info("Suspend by bus power management");
    return cts_suspend(dev_get_drvdata(dev));
}

static int cts_i2c_driver_pm_resume(struct device *dev)
{
    cts_info("Resume by bus power management");
    return cts_resume(dev_get_drvdata(dev));
}

/* bus control the suspend/resume procedure */
static const struct dev_pm_ops cts_i2c_driver_pm_ops = {
    .suspend = cts_i2c_driver_pm_suspend,
    .resume = cts_i2c_driver_pm_resume,
};
#endif /* CONFIG_CTS_PM_GENERIC */
#endif

#ifdef CONFIG_CTS_SYSFS
static ssize_t reset_pin_show(struct device_driver *driver, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "CFG_CTS_HAS_RESET_PIN: %c\n",
#ifdef CFG_CTS_HAS_RESET_PIN
            'Y'
#else
            'N'
#endif
        );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(reset_pin, S_IRUGO, reset_pin_show, NULL);
#else
static DRIVER_ATTR_RO(reset_pin);
#endif

static ssize_t force_update_show(struct device_driver *dev, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "CFG_CTS_HAS_RESET_PIN: %c\n",
#ifdef CFG_CTS_FIRMWARE_FORCE_UPDATE
            'Y'
#else
            'N'
#endif
        );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(force_update, S_IRUGO, force_update_show, NULL);
#else
static DRIVER_ATTR_RO(force_update);
#endif

static ssize_t max_touch_num_show(struct device_driver *dev, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "CFG_CTS_MAX_TOUCH_NUM: %d\n",
            CFG_CTS_MAX_TOUCH_NUM);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(max_touch_num, S_IRUGO, max_touch_num_show, NULL);
#else
static DRIVER_ATTR_RO(max_touch_num);
#endif

static ssize_t vkey_show(struct device_driver *dev, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "CONFIG_CTS_VIRTUALKEY: %c\n",
#ifdef CONFIG_CTS_VIRTUALKEY
            'Y'
#else
            'N'
#endif
        );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(vkey, S_IRUGO, vkey_show, NULL);
#else
static DRIVER_ATTR_RO(vkey);
#endif

static ssize_t gesture_show(struct device_driver *dev, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "CFG_CTS_GESTURE: %c\n",
#ifdef CFG_CTS_GESTURE
            'Y'
#else
            'N'
#endif
        );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(gesture, S_IRUGO, gesture_show, NULL);
#else
static DRIVER_ATTR_RO(gesture);
#endif

static ssize_t esd_protection_show(struct device_driver *dev, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "CONFIG_CTS_ESD_PROTECTION: %c\n",
#ifdef CONFIG_CTS_ESD_PROTECTION
            'Y'
#else
            'N'
#endif
        );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(esd_protection, S_IRUGO, esd_protection_show, NULL);
#else
static DRIVER_ATTR_RO(esd_protection);
#endif

static ssize_t slot_protocol_show(struct device_driver *dev, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "CONFIG_CTS_SLOTPROTOCOL: %c\n",
#ifdef CONFIG_CTS_SLOTPROTOCOL
            'Y'
#else
            'N'
#endif
        );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(slot_protocol, S_IRUGO, slot_protocol_show, NULL);
#else
static DRIVER_ATTR_RO(slot_protocol);
#endif

static ssize_t max_xfer_size_show(struct device_driver *dev, char *buf)
{
#ifdef CONFIG_CTS_I2C_HOST
    return snprintf(buf, PAGE_SIZE, "CFG_CTS_MAX_I2C_XFER_SIZE: %d\n",
            CFG_CTS_MAX_I2C_XFER_SIZE);
#else
    return snprintf(buf, PAGE_SIZE, "CFG_CTS_MAX_SPI_XFER_SIZE: %d\n",
            CFG_CTS_MAX_SPI_XFER_SIZE);
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(max_xfer_size, S_IRUGO, max_xfer_size_show, NULL);
#else
static DRIVER_ATTR_RO(max_xfer_size);
#endif

static ssize_t driver_info_show(struct device_driver *dev, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "Driver version: %s\n",
            CFG_CTS_DRIVER_VERSION);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(driver_info, S_IRUGO, driver_info_show, NULL);
#else
static DRIVER_ATTR_RO(driver_info);
#endif

static struct attribute *cts_driver_config_attrs[] = {
    &driver_attr_reset_pin.attr,
    &driver_attr_force_update.attr,
    &driver_attr_max_touch_num.attr,
    &driver_attr_vkey.attr,
    &driver_attr_gesture.attr,
    &driver_attr_esd_protection.attr,
    &driver_attr_slot_protocol.attr,
    &driver_attr_max_xfer_size.attr,
    &driver_attr_driver_info.attr,
    NULL
};

static const struct attribute_group cts_driver_config_group = {
    .name = "config",
    .attrs = cts_driver_config_attrs,
};

static const struct attribute_group *cts_driver_config_groups[] = {
    &cts_driver_config_group,
    NULL,
};
#endif /* CONFIG_CTS_SYSFS */

#ifdef CTS_PATCH_COMERR_PM
static int cts_spi_driver_pm_suspend(struct device *dev)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    cts_err("system enters into pm_suspend");
    cts_data->pm_suspend = true;
    reinit_completion(&cts_data->pm_completion);
    return 0;
}
static int cts_spi_driver_pm_resume(struct device *dev)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    cts_err("system resumes from pm_suspend");
    cts_data->pm_suspend = false;
    complete(&cts_data->pm_completion);
    return 0;
}
/* bus control the suspend/resume procedure */
static const struct dev_pm_ops cts_spi_driver_pm_ops = {
    .suspend = cts_spi_driver_pm_suspend,
    .resume = cts_spi_driver_pm_resume,
};
#endif /* CONFIG_CTS_PM_GENERIC */

#ifdef CONFIG_CTS_OF
static const struct of_device_id cts_driver_of_match_table[] = {
    {.compatible = CFG_CTS_OF_DEVICE_ID_NAME, },
    { },
};

MODULE_DEVICE_TABLE(of, cts_driver_of_match_table);
#endif /* CONFIG_CTS_OF */

#ifdef CONFIG_CTS_I2C_HOST
static const struct i2c_device_id cts_device_id_table[] = {
    { CFG_CTS_DEVICE_NAME, 0 },
    { }
};
#else
static const struct spi_device_id cts_device_id_table[] = {
    { CFG_CTS_DEVICE_NAME, 0 },
    { }
};
#endif

#ifdef CONFIG_CTS_I2C_HOST
static struct i2c_driver cts_i2c_driver = {
#else
static struct spi_driver cts_spi_driver = {
#endif
    .probe = cts_driver_probe,
    .remove = cts_driver_remove,
    .driver = {
        .name = CFG_CTS_DRIVER_NAME,
        .owner = THIS_MODULE,
#ifdef CONFIG_CTS_OF
        .of_match_table = of_match_ptr(cts_driver_of_match_table),
#endif /* CONFIG_CTS_OF */
#ifdef CONFIG_CTS_SYSFS
        .groups = cts_driver_config_groups,
#endif /* CONFIG_CTS_SYSFS */
#ifndef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
#ifdef CONFIG_CTS_PM_LEGACY
        .suspend = cts_i2c_driver_suspend,
        .resume = cts_i2c_driver_resume,
#endif /* CONFIG_CTS_PM_LEGACY */
#ifdef CONFIG_CTS_PM_GENERIC
        .pm = &cts_i2c_driver_pm_ops,
#endif /* CONFIG_CTS_PM_GENERIC */

#ifdef CTS_PATCH_COMERR_PM
        .pm = &cts_spi_driver_pm_ops,
#endif
#endif
        },
    .id_table = cts_device_id_table,
};

#ifdef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
static int cts_driver_init(void)
#else
static int __init cts_driver_init(void)
#endif
{
    int ret = 0;

    cts_info("Chipone touch driver init, version: "CFG_CTS_DRIVER_VERSION);

#ifdef CONFIG_CTS_I2C_HOST
    cts_info(" - Register i2c driver");
    ret = i2c_add_driver(&cts_i2c_driver);
    if (ret) {
        cts_info("Register i2c driver failed %d(%s)", ret, cts_strerror(ret));
        return ret;
    }
#else
    cts_info(" - Register spi driver");
    ret = spi_register_driver(&cts_spi_driver);
    if (ret) {
        cts_info("Register spi driver failed %d(%s)", ret, cts_strerror(ret));
        return ret;
    }
#endif

    cts_info(" - Register touch driver successfully");

    return 0;
}

#ifdef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
static void cts_driver_exit(void)
#else
static void __exit cts_driver_exit(void)
#endif
{
    cts_info("Exit");

#ifdef CONFIG_CTS_I2C_HOST
    cts_info(" - Delete i2c driver");
    i2c_del_driver(&cts_i2c_driver);
#else
    cts_info(" - Delete spi driver");
    spi_unregister_driver(&cts_spi_driver);
#endif
}

#ifndef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
module_init(cts_driver_init);
module_exit(cts_driver_exit);

#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif
MODULE_DESCRIPTION("Chipone TDDI touchscreen Driver for QualComm platform");
MODULE_VERSION(CFG_CTS_DRIVER_VERSION);
MODULE_AUTHOR("Miao Defang <dfmiao@chiponeic.com>");
MODULE_LICENSE("GPL");

#else /* CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED */

static int chipone_tpd_local_init(void)
{
#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
    static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
    static int tpd_wb_end_local[TPD_WARP_CNT] = TPD_WARP_END;
#endif

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
    static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif

    int ret;

    if ((ret = cts_driver_init()) != 0) {
        cts_err("Init driver failed %d", ret);
        return ret;
    }

    if (tpd_load_status == 0) {
        cts_err("Driver not load successfully, exit.");
        cts_driver_exit();
        return -1;
    }

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
    TPD_DO_WARP = 1;
    memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT * 4);
    memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT * 4);
#endif

#if (defined(CONFIG_TPD_HAVE_CALIBRATION) && !defined(CONFIG_TPD_CUSTOM_CALIBRATION))
    memcpy(tpd_calmat, tpd_def_calmat_local, 8 * 4);
    memcpy(tpd_def_calmat, tpd_def_calmat_local, 8 * 4);
#endif

    tpd_type_cap = 1;

    return 0;
}

/* TPD pass dev = NULL */
static void chipone_tpd_suspend(struct device *dev)
{
    cts_suspend(g_cts_data);
}

/* TPD pass dev = NULL */
static void chipone_tpd_resume(struct device *dev)
{
    cts_resume(g_cts_data);
}

static struct tpd_driver_t chipone_tpd_driver = {
    .tpd_device_name = CFG_CTS_DRIVER_NAME,
    .tpd_local_init = chipone_tpd_local_init,
    .suspend = chipone_tpd_suspend,
    .resume = chipone_tpd_resume,
    .tpd_have_button = 0,
    .attrs = {
        .attr = NULL,
        .num  = 0,
    }
};

static int __init chipone_tpd_driver_init(void)
{
    int ret;

    cts_info("Chipone TDDI TPD driver %s", CFG_CTS_DRIVER_VERSION);

    tpd_get_dts_info();
    if (tpd_dts_data.touch_max_num < 2) {
        tpd_dts_data.touch_max_num = 2;
    } else if (tpd_dts_data.touch_max_num > CFG_CTS_MAX_TOUCH_NUM) {
        tpd_dts_data.touch_max_num = CFG_CTS_MAX_TOUCH_NUM;
    }

    if((ret = tpd_driver_add(&chipone_tpd_driver)) < 0) {
        cts_err("Add TPD driver failed %d", ret);
        return ret;
    }

    return 0;
}

static void __exit chipone_tpd_driver_exit(void)
{
    cts_info("Chipone TPD driver exit");

    tpd_driver_remove(&chipone_tpd_driver);
}

module_init(chipone_tpd_driver_init);
module_exit(chipone_tpd_driver_exit);

#ifdef CFG_CTS_KERNEL_BUILTIN_FIRMWARE
MODULE_FIRMWARE(CFG_CTS_FIRMWARE_FILENAME_9916);
#endif /* CFG_CTS_KERNEL_BUILTIN_FIRMWARE */

MODULE_DESCRIPTION("Chipone TDDI TPD Driver for MTK platform");
MODULE_VERSION(CFG_CTS_DRIVER_VERSION);
MODULE_AUTHOR("Miao Defang <dfmiao@chiponeic.com>");
MODULE_LICENSE("GPL");

#endif /* CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED */
