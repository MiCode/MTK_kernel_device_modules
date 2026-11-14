/*N6 code for HQ-308002 by dingying at 2023/08/15 start*/
#include "focaltech_flash.h"
/*N6 code for HQ-308002 by dingying at 2023/08/15 end*/

#define ORIENTATION_0_OR_180    0    /* anticlockwise 0 or 180 degrees */
#define NORMAL_ORIENTATION_90    1    /* anticlockwise 90 degrees in normal */
#define NORMAL_ORIENTATION_270    2    /* anticlockwise 270 degrees in normal */
#define GAME_ORIENTATION_90    3    /* anticlockwise 90 degrees in game */
#define GAME_ORIENTATION_270    4    /* anticlockwise 270 degrees in game */

static void fts_restore_mode_value(int mode, int value_type)
{
    xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
        xiaomi_touch_interfaces.touch_mode[mode][value_type];
}

static void fts_restore_normal_mode(void)
{
    int i = 0;
    for (i = 0; i <= Touch_Panel_Orientation; i++) {
        if (i != Touch_Panel_Orientation)
            fts_restore_mode_value(i, GET_DEF_VALUE);
    }
}

static void fts_config_game_mode_cmd(struct fts_ts_data *ts_data, u8 *cmd, bool is_expert_mode)
{
    int temp_value = 0;
    struct fts_ts_platform_data *pdata = ts_data->pdata;

    temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE];
    cmd[1] = (u8)(temp_value);
    temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][SET_CUR_VALUE];
    cmd[2] = (u8)(temp_value ? 30 : 3);

    if (is_expert_mode) {
        temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][SET_CUR_VALUE];
        cmd[3] = (u8)(*(pdata->touch_expert_array + (temp_value - 1) * 4));
        cmd[4] = (u8)(*(pdata->touch_expert_array + (temp_value - 1) * 4 + 1));
        cmd[5] = (u8)(*(pdata->touch_expert_array + (temp_value - 1) * 4 + 2));
        cmd[6] = (u8)(*(pdata->touch_expert_array + (temp_value - 1) * 4 + 3));
    } else {
        temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE];
        cmd[3] = (u8)(*(pdata->touch_range_array + temp_value - 1));
        temp_value = xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE];
        cmd[4] = (u8)(*(pdata->touch_range_array + temp_value - 1));
        temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][SET_CUR_VALUE];
        cmd[5] = (u8)(*(pdata->touch_range_array + temp_value - 1));
        temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][SET_CUR_VALUE];
        cmd[6] = (u8)(*(pdata->touch_range_array + temp_value - 1));
    }
}

/* N6 code for HQ-330294 by liaoxianguo at 2023/9/21 start */
static int fts_touch_hdle_mode_set(bool value)
{
    int ret = 0;

    ret = fts_write_reg(FTS_REG_HDLEMODE, value);
    if (ret >= 0)
        FTS_DEBUG("set hdle mode to %d successfully !", value);
    else
        FTS_ERROR("send hdle mode cmd failed : %d", value);

    return ret;
}
/* N6 code for HQ-330294 by liaoxianguo at 2023/9/21 end */

static void fts_update_touchmode_data(struct fts_ts_data *ts_data)
{
    int ret = 0;
    int mode = 0;
    u8 mode_set_value = 0;
    u8 mode_addr = 0;
    bool game_mode_state_change = false;
    u8 cmd[7] = {0xC1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
    if (ts_data && ts_data->pm_suspend) {
        FTS_ERROR("SYSTEM is in suspend mode, don't set touch mode data");
        return;
    }
#endif
    pm_stay_awake(ts_data->dev);
    mutex_lock(&ts_data->cmd_update_mutex);
    fts_config_game_mode_cmd(ts_data, cmd, ts_data->is_expert_mode);
    ret = fts_write(cmd, sizeof(cmd));
    if (ret < 0) {
        FTS_ERROR("write game mode parameter failed\n");
    } else {
        FTS_INFO("update game mode cmd: %02X,%02X,%02X,%02X,%02X,%02X,%02X",
                cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6]);
        for (mode = Touch_Game_Mode; mode <= Touch_Expert_Mode; mode++) {
            if (mode == Touch_Game_Mode &&
                (xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE] !=
                    xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE])) {
                game_mode_state_change = true;
                fts_data->gamemode_enabled = xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE];
            }
            xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE] =
                xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE];
        }
    }
    mode = Touch_Panel_Orientation;
    mode_set_value = xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE];
    if (mode_set_value != xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE] ||
            game_mode_state_change) {
        mode_addr = FTS_REG_ORIENTATION;
        game_mode_state_change = false;
        if (mode_set_value == PANEL_ORIENTATION_DEGREE_0 ||
                mode_set_value == PANEL_ORIENTATION_DEGREE_180) {
            mode_set_value = ORIENTATION_0_OR_180;
        } else if (mode_set_value == PANEL_ORIENTATION_DEGREE_90) {
            mode_set_value = fts_data->gamemode_enabled ?
                GAME_ORIENTATION_90 : NORMAL_ORIENTATION_90;
        } else if (mode_set_value == PANEL_ORIENTATION_DEGREE_270) {
            mode_set_value = fts_data->gamemode_enabled ?
                GAME_ORIENTATION_270 : NORMAL_ORIENTATION_270;
        }
        ret = fts_write_reg(mode_addr, mode_set_value);
        if (ret < 0) {
            FTS_ERROR("write touch mode:%d reg failed", mode);
        } else {
            FTS_INFO("write touch mode:%d, value: %d, addr:0x%02X",
                mode, mode_set_value, mode_addr);
            xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE] =
                xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE];
        /* N6 code for HQ-330294 by liaoxianguo at 2023/9/21 start */
            fts_touch_hdle_mode_set(fts_data->gamemode_enabled);
        /* N6 code for HQ-330294 by liaoxianguo at 2023/9/21 end */
        }
    }
    mode = Touch_Edge_Filter;
    mode_set_value = xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE];
    if (mode_set_value != xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE]) {
        mode_addr = FTS_REG_EDGE_FILTER_LEVEL;
        ret = fts_write_reg(mode_addr, mode_set_value);
        if (ret < 0) {
            FTS_ERROR("write touch mode:%d reg failed", mode);
        } else {
            FTS_INFO("write touch mode:%d, value: %d, addr:0x%02X",
                mode, mode_set_value, mode_addr);
            xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE] =
                xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE];
        }
    }
    mutex_unlock(&ts_data->cmd_update_mutex);
    pm_relax(ts_data->dev);
}

static int fts_get_mode_value(int mode, int value_type)
{
    int value = -1;
    if (mode < Touch_Mode_NUM && mode >= 0) {
        value = xiaomi_touch_interfaces.touch_mode[mode][value_type];
        FTS_INFO("mode:%d, value_type:%d, value:%d", mode, value_type, value);
    } else
        FTS_ERROR("mode:%d don't support", mode);
    return value;
}

static int fts_reset_mode(int mode)
{
    if (mode == Touch_Game_Mode) {
        fts_restore_normal_mode();
        fts_data->gamemode_enabled = false;
        fts_data->is_expert_mode = false;
    } else if (mode < Touch_Report_Rate)
        fts_restore_mode_value(mode, GET_DEF_VALUE);
    else
        FTS_ERROR("mode:%d don't support", mode);
    FTS_INFO("mode:%d reset", mode);
    fts_update_touchmode_data(fts_data);
    return 0;
}

static int fts_get_mode_all(int mode, int *value)
{
    if (mode < Touch_Mode_NUM && mode >= 0) {
        value[0] = xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE];
        value[1] = xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
        value[2] = xiaomi_touch_interfaces.touch_mode[mode][GET_MIN_VALUE];
        value[3] = xiaomi_touch_interfaces.touch_mode[mode][GET_MAX_VALUE];
    } else {
        FTS_ERROR("mode:%d don't support", mode);
    }
    FTS_INFO("mode:%d, value:%d:%d:%d:%d", mode,
                value[0], value[1], value[2], value[3]);
    return 0;
}

static int fts_touch_edge_mode_set(int value)
{
    int ret = 0;
    if ((value != ORIENTATION_0_OR_180) && (value != NORMAL_ORIENTATION_90) &&
        value != NORMAL_ORIENTATION_270) {
        FTS_ERROR("not support edge value: %d", value);
        return ret;
    }
    if (fts_data->edge_mode != value) {
        ret = fts_write_reg(FTS_REG_EDGE_MODE_EN, value);
        if (ret >= 0) {
            fts_data->edge_mode = value;
            FTS_DEBUG("MODE_EDGE switch to %d successfully !", fts_data->edge_mode);
        }
    }
    return ret;
}

static void fts_game_mode_recovery(struct fts_ts_data *ts_data)
{
    xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_CUR_VALUE] =
        xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_DEF_VALUE];
    xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_CUR_VALUE] =
        xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_DEF_VALUE];
    xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_CUR_VALUE] =
        xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_DEF_VALUE];
    fts_update_touchmode_data(ts_data);
}

static void fts_init_touchmode_data(struct fts_ts_data *ts_data)
{
    struct fts_ts_platform_data *pdata = ts_data->pdata;
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
    /* UP_THRESHOLD */
    xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MAX_VALUE] = 5;
    xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MIN_VALUE] = 1;
    xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_DEF_VALUE] = pdata->touch_def_array[0];
    xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE] = pdata->touch_def_array[0];
    xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_CUR_VALUE] = pdata->touch_def_array[0];
    /*  Tolerance */
    xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MAX_VALUE] = 5;
    xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MIN_VALUE] = 1;
    xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_DEF_VALUE] = pdata->touch_def_array[1];
    xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE] = pdata->touch_def_array[1];
    xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_CUR_VALUE] = pdata->touch_def_array[1];
    /*  Aim_Sensitivity */
    xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_MAX_VALUE] = 5;
    xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_MIN_VALUE] = 1;
    xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_DEF_VALUE] = pdata->touch_def_array[2];
    xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][SET_CUR_VALUE] = pdata->touch_def_array[2];
    xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_CUR_VALUE] = pdata->touch_def_array[2];
    /*  Tap_Stability */
    xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_MAX_VALUE] = 5;
    xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_MIN_VALUE] = 1;
    xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_DEF_VALUE] = pdata->touch_def_array[3];
    xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][SET_CUR_VALUE] = pdata->touch_def_array[3];
    xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_CUR_VALUE] = pdata->touch_def_array[3];
    /* panel orientation*/
    xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MAX_VALUE] = 3;
    xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MIN_VALUE] = 0;
    xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_DEF_VALUE] = 0;
    xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE] = 0;
    xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_CUR_VALUE] = 0;
    /* Expert_Mode*/
    xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][GET_MAX_VALUE] = 3;
    xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][GET_MIN_VALUE] = 1;
    xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][GET_DEF_VALUE] = 1;
    xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][SET_CUR_VALUE] = 1;
    xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][GET_CUR_VALUE] = 1;
    /* edge filter area*/
    xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MAX_VALUE] = 3;
    xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MIN_VALUE] = 0;
    xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_DEF_VALUE] = 2;
    xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE] = 2;
    xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_CUR_VALUE] = 2;
    FTS_INFO("touchfeature value init done");
}

/*N6 code for HQ-308002 by dingying at 2023/08/15 start*/
static u8 fts_panel_vendor_read(void)
{
    if (fts_data)
        return fts_data->lockdown_info[0];
    else
        return 0;
}

static u8 fts_panel_color_read(void)
{
    if (fts_data)
        return fts_data->lockdown_info[2];
    else
        return 0;
}

static u8 fts_panel_display_read(void)
{
    if (fts_data)
        return fts_data->lockdown_info[1];
    else
        return 0;
}

static char fts_touch_vendor_read(void)
{
    return FTS_VENDOR_TOUCH_IC;
}

int fts_init_lockdown_info(u8 *buf)
{
    u32 lockdown_addr = 0x3F000;
    int ret = 0;
    u8 lockdown_info[0x20] = {0};
    int count = 0;
    int i = 0;

    /* N6 code for HQ-330297 by liaoxianguo at 2023/9/20 start */
    struct fts_ts_data *ts_data = fts_data;

    if (ts_data->fw_loading) {
        FTS_INFO("fw upgrade in process, can't init lockdown");
        return -EINVAL;
    }
    /* N6 code for HQ-330297 by liaoxianguo at 2023/9/20 end */

    ret = fts_flash_read(lockdown_addr, lockdown_info, sizeof(lockdown_info));
    if (ret < 0) {
        FTS_ERROR("fail to read lockdown info from tp");
        return ret;
    }
    for(i = 0; i < 8; i++) {
        count += sprintf(buf + count, "%c", lockdown_info[i]);
    }
    FTS_INFO("lockdown info: %s", buf);
    return 0;
}
/*N6 code for HQ-308002 by dingying at 2023/08/15 end*/
