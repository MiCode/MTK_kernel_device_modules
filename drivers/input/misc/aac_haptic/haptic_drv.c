/*
 * drivers/haptic/haptic_drv.c
 *
 * Copyright (c) 2022 ICSense Semiconductor CO., LTD
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation (version 2 of the License only).
 */
#define  DEBUG
#include <linux/pinctrl/consumer.h>
#include <linux/gpio.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/pm_qos.h>
#include <linux/mman.h>

#include "haptic_util.h"
#include "haptic_drv.h"
#include "richtap_drv.h"
#include "input_drv.h"
#include "rt6010.h"

#ifdef  ENABLE_PIN_CONTROL
static const char *const pctl_names[] = {
        "ics_reset_reset",
        "ics_reset_active",
        "ics_interrupt_active",
};
#endif

char *haptic_config_name = "haptic_config.bin";
char preset_waveform_name[][MAX_PRESET_NAME_LEN] =
{
	{"0_click_P_RTP.bin"},
        {"1_doubelClick_P_RTP.bin"},
        {"2_tick_P_RTP.bin"},
        {"3_thud_P_RTP.bin"},
        {"4_pop_P_RTP.bin"},
        {"5_heavyClick_P_RTP.bin"},
        {"6_ringTone1_P_RTP.bin"},
        {"7_ringTone2_P_RTP.bin"},
        {"8_ringTone3_P_RTP.bin"},
        {"9_ringTone4_P_RTP.bin"},
	{"10_ringTone4_P_RTP.bin"},
        {"11_ringTone4_P_RTP.bin"},
        {"12_ringTone4_P_RTP.bin"},
        {"13_ringTone4_P_RTP.bin"},
	{"14_ringTone4_P_RTP.bin"},
        {"15_ringTone4_P_RTP.bin"},
        {"16_ringTone4_P_RTP.bin"},
        {"17_ringTone4_P_RTP.bin"},
	{"18_ringTone4_P_RTP.bin"},
        {"19_ringTone4_P_RTP.bin"},
        {"20_ringTone4_P_RTP.bin"},
	/*{"aw8697_rtp_1.bin"},*/
	/*{"aw8697_rtp_1.bin"},*/
	{"AcousticGuitar_RTP.bin"}, /*21*/
	{"Blues_RTP.bin"},
	{"Candy_RTP.bin"},
	{"Carousel_RTP.bin"},
	{"Celesta_RTP.bin"},
	{"Childhood_RTP.bin"},
	{"Country_RTP.bin"},
	{"Cowboy_RTP.bin"},
	{"Echo_RTP.bin"},
	{"Fairyland_RTP.bin"},
	{"Fantasy_RTP.bin"},
	{"Field_Trip_RTP.bin"},
	{"Glee_RTP.bin"},
	{"Glockenspiel_RTP.bin"},
	{"Ice_Latte_RTP.bin"},
	{"Kung_Fu_RTP.bin"},
	{"Leisure_RTP.bin"},
	{"Lollipop_RTP.bin"},
	{"MiMix2_RTP.bin"},
	{"Mi_RTP.bin"},
	{"MiHouse_RTP.bin"},
	{"MiJazz_RTP.bin"},
	{"MiRemix_RTP.bin"},
	{"Mountain_Spring_RTP.bin"},
	{"Orange_RTP.bin"},
	{"WindChime_RTP.bin"},
	{"Space_Age_RTP.bin"},
	{"ToyRobot_RTP.bin"},
	{"Vigor_RTP.bin"},
	{"Bottle_RTP.bin"},
	{"Bubble_RTP.bin"},
	{"Bullfrog_RTP.bin"},
	{"Burst_RTP.bin"},
	{"Chirp_RTP.bin"},
	{"Clank_RTP.bin"},
	{"Crystal_RTP.bin"},
	{"FadeIn_RTP.bin"},
	{"FadeOut_RTP.bin"},
	{"Flute_RTP.bin"},
	{"Fresh_RTP.bin"},
	{"Frog_RTP.bin"},
	{"Guitar_RTP.bin"},
	{"Harp_RTP.bin"},
	{"IncomingMessage_RTP.bin"},
	{"MessageSent_RTP.bin"},
	{"Moment_RTP.bin"},
	{"NotificationXylophone_RTP.bin"},
	{"Potion_RTP.bin"},
	{"Radar_RTP.bin"},
	{"Spring_RTP.bin"},
	{"Swoosh_RTP.bin"}, /*71*/
	{"Gesture_UpSlide_RTP.bin"},
	{"FOD_Motion_Planet_RTP.bin"},
	{"Charge_Wire_RTP.bin"},
	{"Charge_Wireless_RTP.bin"},
	{"Unlock_Failed_RTP.bin"},
	{"FOD_Motion1_RTP.bin"},
	{"FOD_Motion2_RTP.bin"},
	{"FOD_Motion3_RTP.bin"},
	{"FOD_Motion4_RTP.bin"},
	{"FOD_Motion_Aurora_RTP.bin"},
	{"FaceID_Wrong2_RTP.bin"}, /*82*/
	{"uninstall_animation_rtp.bin"},
	{"uninstall_dialog_rtp.bin"},
	{"screenshot_rtp.bin"},
	{"lockscreen_camera_entry_rtp.bin"},
	{"launcher_edit_rtp.bin"},
	{"launcher_icon_selection_rtp.bin"},
	{"taskcard_remove_rtp.bin"},
	{"task_cleanall_rtp.bin"},
	{"new_iconfolder_rtp.bin"},
	{"notification_remove_rtp.bin"},
	{"notification_cleanall_rtp.bin"},
	{"notification_setting_rtp.bin"},
	{"game_turbo_rtp.bin"},
	{"NFC_card_rtp.bin"},
	{"wakeup_voice_assistant_rtp.bin"},
	{"NFC_card_slow_rtp.bin"},
	{"aw8697_rtp_1.bin"}, /*99*/
	{"aw8697_rtp_1.bin"}, /*100*/
	{"offline_countdown_RTP.bin"},
	{"scene_bomb_injury_RTP.bin"},
	{"scene_bomb_RTP.bin"}, /*103*/
	{"door_open_RTP.bin"},
	{"aw8697_rtp_1.bin"},
	{"scene_step_RTP.bin"}, /*106*/
	{"crawl_RTP.bin"},
	{"scope_on_RTP.bin"},
	{"scope_off_RTP.bin"},
	{"magazine_quick_RTP.bin"},
	{"grenade_RTP.bin"},
	{"scene_getshot_RTP.bin"}, /*112*/
	{"grenade_explosion_RTP.bin"},
	{"punch_RTP.bin"},
	{"pan_RTP.bin"},
	{"bandage_RTP.bin"},
	{"aw8697_rtp_1.bin"},
	{"scene_jump_RTP.bin"},
	{"vehicle_plane_RTP.bin"}, /*119*/
	{"scene_openparachute_RTP.bin"}, /*120*/
	{"scene_closeparachute_RTP.bin"}, /*121*/
	{"vehicle_collision_RTP.bin"},
	{"vehicle_buggy_RTP.bin"}, /*123*/
	{"vehicle_dacia_RTP.bin"}, /*124*/
	{"vehicle_moto_RTP.bin"}, /*125*/
	{"firearms_akm_RTP.bin"}, /*126*/
	{"firearms_m16a4_RTP.bin"}, /*127*/
	{"aw8697_rtp_1.bin"},
	{"firearms_awm_RTP.bin"}, /*129*/
	{"firearms_mini14_RTP.bin"}, /*130*/
	{"firearms_vss_RTP.bin"}, /*131*/
	{"firearms_qbz_RTP.bin"}, /*132*/
	{"firearms_ump9_RTP.bin"}, /*133*/
	{"firearms_dp28_RTP.bin"}, /*134*/
	{"firearms_s1897_RTP.bin"}, /*135*/
	{"aw8697_rtp_1.bin"},
	{"firearms_p18c_RTP.bin"}, /*137*/
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"CFM_KillOne_RTP.bin"},
	{"CFM_Headshot_RTP.bin"}, /*141*/
	{"CFM_MultiKill_RTP.bin"},
	{"CFM_KillOne_Strong_RTP.bin"},
	{"CFM_Headshot_Strong_RTP.bin"},
	{"CFM_MultiKill_Strong_RTP.bin"},
	{"CFM_Weapon_Grenade_Explode_RTP.bin"},
	{"CFM_Weapon_Grenade_KillOne_RTP.bin"},
	{"CFM_ImpactFlesh_Normal_RTP.bin"},
	{"CFM_Weapon_C4_Installed_RTP.bin"},
	{"CFM_Hero_Appear_RTP.bin"},
	{"CFM_UI_Reward_OpenBox_RTP.bin"},
	{"CFM_UI_Reward_Task_RTP.bin"},
	{"CFM_Weapon_BLT_Shoot_RTP.bin"}, /*153*/
	{"Atlantis_RTP.bin"},
	{"DigitalUniverse_RTP.bin"},
	{"Reveries_RTP.bin"},
	{"FOD_Motion_Triang_RTP.bin"},
	{"FOD_Motion_Flare_RTP.bin"},
	{"FOD_Motion_Ripple_RTP.bin"},
	{"FOD_Motion_Spiral_RTP.bin"},
	{"gamebox_launch_rtp.bin"}, /*161*/
	{"Gesture_Back_Pull_RTP.bin"}, /*162*/
	{"Gesture_Back_Release_RTP.bin"}, /*163*/
	{"alert_rtp.bin"}, /*164*/
	{"feedback_negative_light_rtp.bin"}, /*165*/
	{"feedback_neutral_rtp.bin"}, /*166*/
	{"feedback_positive_rtp.bin"}, /*167*/
	{"fingerprint_record_rtp.bin"}, /*168*/
	{"lockdown_rtp.bin"}, /*169*/
	{"sliding_damping_rtp.bin"}, /*170*/
	{"todo_alldone_rtp.bin"}, /*171*/
	{"uninstall_animation_icon_rtp.bin"}, /*172*/
	{"signal_button_highlight_rtp.bin"}, /*173*/
	{"signal_button_negative_rtp.bin"},
	{"signal_button_rtp.bin"},
	{"signal_clock_high_rtp.bin"}, /*176*/
	{"signal_clock_rtp.bin"},
	{"signal_clock_unit_rtp.bin"},
	{"signal_inputbox_rtp.bin"},
	{"signal_key_high_rtp.bin"},
	{"signal_key_unit_rtp.bin"}, /*181*/
	{"signal_list_highlight_rtp.bin"},
	{"signal_list_rtp.bin"},
	{"signal_picker_rtp.bin"},
	{"signal_popup_rtp.bin"},
	{"signal_seekbar_rtp.bin"}, /*186*/
	{"signal_switch_rtp.bin"},
	{"signal_tab_rtp.bin"},
	{"signal_text_rtp.bin"},
	{"signal_transition_light_rtp.bin"},
	{"signal_transition_rtp.bin"}, /*191*/
	{"haptics_video_rtp.bin"}, /*192*/
	{"keyboard_clicky_down_rtp.bin"},
	{"keyboard_clicky_up_rtp.bin"},
	{"keyboard_linear_down_rtp.bin"},
	{"keyboard_linear_up_rtp.bin"}, /*196*/    
};

int32_t ics_rtp_name_len = sizeof(preset_waveform_name) / MAX_PRESET_NAME_LEN;

static int32_t haptic_hw_reset(struct ics_haptic_data *haptic_data);

static ssize_t chip_id_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;

    ret = haptic_data->func->get_chip_id(haptic_data);
    check_error_return(ret);

    return snprintf(buf, PAGE_SIZE, "%02X\n", haptic_data->chip_config.chip_id);
}

static ssize_t f0_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    ssize_t len = 0;

    mutex_lock(&haptic_data->lock);
    ret = haptic_data->func->get_f0(haptic_data);
    mutex_unlock(&haptic_data->lock);
    check_error_return(ret);

    len += snprintf(buf + len, PAGE_SIZE - len, "%u\n", haptic_data->chip_config.f0);
    return len;
}

static ssize_t f0_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    int32_t val = 0;

    ret = kstrtoint(buf, 10, &val);
    check_error_return(ret);

    if (abs((int32_t)haptic_data->chip_config.f0 - val) > RESAMPLE_THRESHOLD)
    {
        haptic_data->chip_config.f0 = val;
        haptic_data->func->resample_ram_waveform(haptic_data);
    }

    return count;
}

static ssize_t f0_check_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    ssize_t len = 0;
    uint32_t bemf_f0 = 0;


    mutex_lock(&haptic_data->lock);
    bemf_f0 = haptic_data->bemf_f0;
    ics_dbg("%s: bemf f0 = %d\n", __func__, bemf_f0);
    mutex_unlock(&haptic_data->lock);

    len += scnprintf(buf + len, PAGE_SIZE - len, "%u\n", bemf_f0 == 0 ? 0 : 1);

    return len;
}

static ssize_t reg_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    ssize_t len = 0;
    uint32_t reg_val = 0, i;

    len += snprintf(buf + len, PAGE_SIZE - len, "reg list (0x%02X):\n", haptic_data->chip_config.reg_size);
    for (i = 0; i < haptic_data->chip_config.reg_size; i++)
    {
        ret = haptic_data->func->get_reg(haptic_data, i, &reg_val);
        check_error_return(ret);
        len += snprintf(buf + len, PAGE_SIZE - len, "0x%02X=0x%02X\n", (uint8_t)i, (uint8_t)reg_val);
    }

    return len;
}

static ssize_t reg_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    uint32_t data_buf[2] = { 0, 0 };

    if (sscanf(buf, "%X %X", &data_buf[0], &data_buf[1]) == 2)
    {
        ret = haptic_data->func->set_reg(haptic_data, data_buf[0], data_buf[1]);
        check_error_return(ret);
    }

    return count;
}

static ssize_t vbst_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    ssize_t len = 0;

    len += snprintf(buf + len, PAGE_SIZE - len, "%u\n", haptic_data->chip_config.boost_vol);
    return len;
}

static ssize_t vbst_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    uint32_t val = 0;

    ret = kstrtouint(buf, 10, &val);
    check_error_return(ret);

    mutex_lock(&haptic_data->lock);
    ret = haptic_data->func->set_bst_vol(haptic_data, val);
    mutex_unlock(&haptic_data->lock);
    check_error_return(ret);

    return count;
}

static ssize_t gain_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    ssize_t len = 0;

    len += snprintf(buf + len, PAGE_SIZE - len, "%u\n", haptic_data->chip_config.gain);
    return len;
}

static ssize_t gain_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    uint32_t reg_val = 0;

    ret = kstrtouint(buf, 10, &reg_val);
    check_error_return(ret);

    mutex_lock(&haptic_data->lock);
    ret = haptic_data->func->set_gain(haptic_data, reg_val);
    mutex_unlock(&haptic_data->lock);
    check_error_return(ret);

    return count;
}

static ssize_t rtp_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    uint32_t index = 0;
    uint32_t preset_num = sizeof(preset_waveform_name) / MAX_PRESET_NAME_LEN;

    ret = kstrtouint(buf, 10, &index);
    check_error_return(ret);

    mutex_lock(&haptic_data->lock);
    if (index < preset_num)
    {
        haptic_data->preset_wave_index = index;
        ics_info("preset_waveform_name[%u]: %s\n", index, preset_waveform_name[index]);
        schedule_work(&haptic_data->preset_work);
    }
    else
    {
        ics_err("%s: specified invalid preset waveform index : %d\n", __func__, index);
    }
    mutex_unlock(&haptic_data->lock);

    return count;
}

static ssize_t index_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    ssize_t len = 0;

    len += snprintf(buf + len, PAGE_SIZE - len, "%u\n", haptic_data->ram_wave_index);
    return len;
}

static ssize_t index_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    uint32_t val = 0;
    uint8_t buf_play_list[6];

    ret = kstrtouint(buf, 10, &val);
    check_error_return(ret);

    haptic_data->ram_wave_index = val;
    buf_play_list[0] = 0x01;
    buf_play_list[1] = 0x00;
    buf_play_list[2] = 0x01;  //play once
    buf_play_list[3] = (uint8_t)haptic_data->ram_wave_index;
    buf_play_list[4] = 0x00;
    buf_play_list[5] = 0x00;
    haptic_data->func->set_play_list(haptic_data, buf_play_list, sizeof(buf_play_list));
    haptic_data->func->set_play_mode(haptic_data, PLAY_MODE_RAM);
    haptic_data->func->play_go(haptic_data);

    return count;
}

static ssize_t duration_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    ktime_t time_rem;
    s64 time_ms = 0;

    if (hrtimer_active(&haptic_data->timer))
    {
        time_rem = hrtimer_get_remaining(&haptic_data->timer);
        time_ms = ktime_to_ms(time_rem);
    }
    return snprintf(buf, PAGE_SIZE, "%lld\n", time_ms);
}

static ssize_t duration_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    uint32_t val = 0;

    ret = kstrtouint(buf, 10, &val);
    check_error_return(ret);

    // setting 0 on duration is NOP
    if (val > 0)
    {
        haptic_data->duration = val;
    }

    return count;
}

static ssize_t activate_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    return snprintf(buf, PAGE_SIZE, "%d\n", haptic_data->activate_state);
}

static ssize_t activate_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int ret = 0;
    uint32_t val = 0;

    ret = kstrtouint(buf, 10, &val);
    check_error_return(ret);

    mutex_lock(&haptic_data->lock);
    hrtimer_cancel(&haptic_data->timer);
    haptic_data->activate_state = val;
    mutex_unlock(&haptic_data->lock);
    schedule_work(&haptic_data->vibrator_work);

    return count;
}

static ssize_t playlist_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    ssize_t len = 0;
    uint32_t i, size = haptic_data->chip_config.ram_size;

    if (haptic_data->gp_buf == NULL)
    {
        return 0;
    }

    ret = haptic_data->func->get_ram_data(haptic_data, haptic_data->gp_buf, &size);
    if (ret >= 0)
    {
        for(i = haptic_data->chip_config.list_base_addr; i < haptic_data->chip_config.wave_base_addr; i++)
        {
            len += snprintf(buf + len, PAGE_SIZE - len, "%02X", haptic_data->gp_buf[i]);
        }
    }
    check_error_return(ret);

    return len;
}

static ssize_t playlist_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;

    if (count > 0)
    {
        ret = haptic_data->func->set_play_list(haptic_data, (uint8_t*)buf, count);
    }
    check_error_return(ret);

    return count;
}

static ssize_t waveform_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    ssize_t len = 0;
    uint32_t i, size = haptic_data->chip_config.ram_size;

    if (haptic_data->gp_buf == NULL)
    {
        return 0;
    }

    ics_info("%s: waveform_show: %u\n", __func__, size);
    ret = haptic_data->func->get_ram_data(haptic_data, haptic_data->gp_buf, &size);
    if (ret >= 0)
    {
        for(i = haptic_data->chip_config.wave_base_addr; i < haptic_data->chip_config.ram_size; i++)
        {
            len += snprintf(buf + len, PAGE_SIZE - len, "%02X", haptic_data->gp_buf[i]);
        }
    }
    check_error_return(ret);

    return len;
}

static ssize_t waveform_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;

    if (count > 0)
    {
        if (count > haptic_data->waveform_size)
        {
            vfree(haptic_data->waveform_data);
            haptic_data->waveform_data = vmalloc(count);
            if (haptic_data->waveform_data == NULL)
            {
                ics_err("%s: failed to allocate memory for waveform data\n", __func__);
                return -1;
            }
        }
        haptic_data->waveform_size = count;
        memcpy(haptic_data->waveform_data, (uint8_t*)buf, count);
        if (abs((int32_t)haptic_data->chip_config.f0
            - (int32_t)haptic_data->chip_config.sys_f0) > RESAMPLE_THRESHOLD)
        {
            haptic_data->func->resample_ram_waveform(haptic_data);
        }
        else
        {
            ret = haptic_data->func->set_waveform_data(haptic_data, (uint8_t*)buf, count);
            check_error_return(ret);
        }
    }

    return count;
}

static ssize_t play_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    ssize_t len = 0;

    ret = haptic_data->func->get_play_status(haptic_data);
    check_error_return(ret);
    len += snprintf(buf + len, PAGE_SIZE - len, "%u", haptic_data->play_status);

    return len;
}

static ssize_t play_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    uint32_t val = 0;

    ret = kstrtouint(buf, 10, &val);
    check_error_return(ret);

    mutex_lock(&haptic_data->lock);
    if ((val & 0x01) > 0)
    {
        ret = haptic_data->func->play_go(haptic_data);
    }
    else
    {
        ret = haptic_data->func->play_stop(haptic_data);
    }
    mutex_unlock(&haptic_data->lock);
    check_error_return(ret);

    return count;
}

static ssize_t stream_start_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    uint32_t start;

    ret = kstrtouint(buf, 10, &start);
    check_error_return(ret);

    if (start > 0)
    {
        mutex_lock(&haptic_data->lock);
        haptic_data->stream_start = true;
        kfifo_reset(&haptic_data->stream_fifo);
        haptic_data->func->set_play_mode(haptic_data, PLAY_MODE_STREAM);
        haptic_data->func->clear_stream_fifo(haptic_data);
        haptic_data->func->play_go(haptic_data);
        mutex_unlock(&haptic_data->lock);
    }

    return count;
}

static ssize_t stream_data_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    ssize_t len = 0;
    uint32_t count;

    count = kfifo_avail(&haptic_data->stream_fifo);
    len += snprintf(buf + len, PAGE_SIZE - len, "%u", count);

    return len;
}

int32_t send_stream_data(struct ics_haptic_data *haptic_data, uint32_t fifo_available_size)
{
    int32_t ret = 0;
    uint32_t buf_fifo_used = 0, size;

    if (haptic_data->gp_buf == NULL)
    {
        return 0;
    }

    buf_fifo_used = kfifo_len(&haptic_data->stream_fifo);
    size = min(fifo_available_size, buf_fifo_used);
    size = kfifo_out(&haptic_data->stream_fifo, haptic_data->gp_buf, size);
    if (size > 0)
    {
        ics_dbg("kfifo out %d\n", size);
        if (waitqueue_active(&haptic_data->kfifo_wait_q))
        {
            atomic_set(&haptic_data->kfifo_available, 1);
            wake_up_interruptible(&haptic_data->kfifo_wait_q);
        }        
        ret = haptic_data->func->set_stream_data(haptic_data, haptic_data->gp_buf, size);
    }
    check_error_return(ret);

    return 0;
}

static ssize_t stream_data_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    uint32_t size = 0, chip_fifo_size = haptic_data->chip_config.fifo_size;
    uint32_t available = kfifo_avail(&haptic_data->stream_fifo);

    size = count;
    if (size > available)
    {
        ics_dbg("stream data size is bigger than stream fifo available size! \
            available=%u, size=%u\n", available, size);
        size = available;
    }
    if (size > 0)
    {
        kfifo_in(&haptic_data->stream_fifo, buf, size);
    }

    if (haptic_data->stream_start)
    {
        haptic_data->stream_start = false;

        ret = send_stream_data(haptic_data, chip_fifo_size);
        check_error_return(ret);
    }

    return count;
}

static ssize_t vbat_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    ssize_t len = 0;

    mutex_lock(&haptic_data->lock);
    ret = haptic_data->func->get_vbat(haptic_data);
    mutex_unlock(&haptic_data->lock);
    check_error_return(ret);

    len += snprintf(buf + len, PAGE_SIZE - len, "%u\n", haptic_data->chip_config.vbat);
    return len;
}

static ssize_t resistance_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    ssize_t len = 0;

    mutex_lock(&haptic_data->lock);
    ret = haptic_data->func->get_resistance(haptic_data);
    mutex_unlock(&haptic_data->lock);
    check_error_return(ret);

    len += snprintf(buf + len, PAGE_SIZE - len, "%u\n", haptic_data->chip_config.resistance);
    return len;
}

static ssize_t state_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    ssize_t len = 0;

    mutex_lock(&haptic_data->lock);
    ret = haptic_data->func->get_sys_state(haptic_data);
    mutex_unlock(&haptic_data->lock);
    check_error_return(ret);

    len += snprintf(buf + len, PAGE_SIZE - len, "%u\n", haptic_data->sys_state);
    return len;
}

static ssize_t reset_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    uint32_t reset = 0;

    ret = kstrtouint(buf, 10, &reset);
    check_error_return(ret);

    if (reset > 0)
    {
        if (gpio_is_valid(haptic_data->gpio_en))
        {
            haptic_hw_reset(haptic_data);
            ics_info("hardware reset successfully!\n");
        }
        else
        {
            ics_info("hardware reset gpio is NOT valid!\n");
        }
    }

    return count;
}

static ssize_t adc_offset_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    ssize_t len = 0;

    mutex_lock(&haptic_data->lock);
    ret = haptic_data->func->get_adc_offset(haptic_data);
    mutex_unlock(&haptic_data->lock);
    check_error_return(ret);

    len += snprintf(buf + len, PAGE_SIZE - len, "%u\n", haptic_data->adc_offset);
    return len;
}

static ssize_t brake_en_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    ssize_t len = 0;

    len += snprintf(buf + len, PAGE_SIZE - len, "%u\n", haptic_data->chip_config.brake_en);
    return len;
}

static ssize_t brake_en_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    uint32_t enable = 0;

    ret = kstrtouint(buf, 10, &enable);
    check_error_return(ret);

    enable = enable > 0 ? 1 : 0;
    ret = haptic_data->func->set_brake_en(haptic_data, enable);
    check_error_return(ret);

    return count;
}

static ssize_t daq_en_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    uint32_t enable = 0;

    ret = kstrtouint(buf, 10, &enable);
    check_error_return(ret);
    enable = enable > 0 ? 1 : 0;
    ret = haptic_data->func->set_daq_en(haptic_data, enable);
    check_error_return(ret);

    return count;
}

static ssize_t f0_en_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    int32_t ret = 0;
    uint32_t enable = 0;

    ret = kstrtouint(buf, 10, &enable);
    check_error_return(ret);
    enable = enable > 0 ? 1 : 0;
    ret = haptic_data->func->set_f0_en(haptic_data, enable);
    check_error_return(ret);

    return count;
}

static ssize_t daq_data_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);

    if (haptic_data->daq_size > 0)
    {
        memcpy(buf, haptic_data->daq_data, haptic_data->daq_size);
    }

    return haptic_data->daq_size;
}

///////////////////////////////////////////////////////////////////////////////
// haptic sys attribute nodes
///////////////////////////////////////////////////////////////////////////////
static DEVICE_ATTR(chip_id, S_IWUSR | S_IRUGO, chip_id_show, NULL);
static DEVICE_ATTR(f0, S_IWUSR | S_IRUGO, f0_show, f0_store);
static DEVICE_ATTR(f0_check, S_IWUSR | S_IRUGO, f0_check_show, NULL);
static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO, reg_show, reg_store);
static DEVICE_ATTR(vbst, S_IWUSR | S_IRUGO, vbst_show, vbst_store);
static DEVICE_ATTR(gain, S_IWUSR | S_IRUGO, gain_show, gain_store);
static DEVICE_ATTR(rtp, S_IWUSR | S_IRUGO, NULL, rtp_store);
static DEVICE_ATTR(playlist, S_IWUSR | S_IRUGO, playlist_show, playlist_store);
static DEVICE_ATTR(waveform, S_IWUSR | S_IRUGO, waveform_show, waveform_store);
static DEVICE_ATTR(daq_data, S_IWUSR | S_IRUGO, daq_data_show, NULL);
static DEVICE_ATTR(brake_en, S_IWUSR | S_IRUGO, brake_en_show, brake_en_store);
static DEVICE_ATTR(daq_en, S_IWUSR | S_IRUGO, NULL, daq_en_store);
static DEVICE_ATTR(f0_en, S_IWUSR | S_IRUGO, NULL, f0_en_store);
static DEVICE_ATTR(play, S_IWUSR | S_IRUGO, play_show, play_store);
static DEVICE_ATTR(stream_start, S_IWUSR | S_IRUGO, NULL, stream_start_store);
static DEVICE_ATTR(stream_data, S_IWUSR | S_IRUGO, stream_data_show, stream_data_store);
static DEVICE_ATTR(index, S_IWUSR | S_IRUGO, index_show, index_store);
static DEVICE_ATTR(duration, S_IWUSR | S_IRUGO, duration_show, duration_store);
static DEVICE_ATTR(activate, S_IWUSR | S_IRUGO, activate_show, activate_store);
static DEVICE_ATTR(vbat, S_IWUSR | S_IRUGO, vbat_show, NULL);
static DEVICE_ATTR(resistance, S_IWUSR | S_IRUGO, resistance_show, NULL);
static DEVICE_ATTR(lra_resistance, S_IWUSR | S_IRUGO, resistance_show, NULL);
static DEVICE_ATTR(state, S_IWUSR | S_IRUGO, state_show, NULL);
static DEVICE_ATTR(reset, S_IWUSR | S_IRUGO, NULL, reset_store);
static DEVICE_ATTR(adc_offset, S_IWUSR | S_IRUGO, adc_offset_show, NULL);

static struct attribute *ics_haptic_attributes[] = {
    &dev_attr_chip_id.attr,
    &dev_attr_f0.attr,
    &dev_attr_f0_check.attr,
    &dev_attr_reg.attr,
    &dev_attr_vbst.attr,
    &dev_attr_gain.attr,
    &dev_attr_rtp.attr,
    &dev_attr_playlist.attr,
    &dev_attr_waveform.attr,
    &dev_attr_daq_data.attr,
    &dev_attr_brake_en.attr,
    &dev_attr_daq_en.attr,
    &dev_attr_f0_en.attr,
    &dev_attr_play.attr,
    &dev_attr_stream_start.attr,
    &dev_attr_stream_data.attr,
    &dev_attr_index.attr,
    &dev_attr_duration.attr,
    &dev_attr_activate.attr,
    &dev_attr_vbat.attr,
    &dev_attr_resistance.attr,
    &dev_attr_lra_resistance.attr,
    &dev_attr_state.attr,
    &dev_attr_reset.attr,
    &dev_attr_adc_offset.attr,
    NULL
};

static struct attribute_group ics_haptic_attribute_group = {
    .attrs = ics_haptic_attributes
};

static irqreturn_t ics_haptic_irq_handler(int irq, void *data)
{
    struct ics_haptic_data *haptic_data = data;
    int32_t ret = 0;
    uint32_t data_size, fifo_used;

    atomic_set(&haptic_data->is_in_rtp_loop, 1);
    ret = haptic_data->func->get_irq_state(haptic_data);
    ics_dbg("%s: irq state = 0x%02X\n", __func__, (uint8_t)(haptic_data->irq_state));
    if (ret < 0)
    {
        goto irq_exit;
    }

    if (haptic_data->func->is_irq_protection(haptic_data))
    {
        ret = haptic_data->func->clear_protection(haptic_data);
        return ret;
    }

    if (haptic_data->func->is_irq_play_done(haptic_data))
    {
        if (haptic_data->daq_en == 1)
        {
            ics_dbg("%s: start acquiring bemf data\n", __func__);
            haptic_data->daq_en = 0;
            ret = haptic_data->func->get_daq_data(haptic_data, 
                haptic_data->daq_data, &haptic_data->daq_size);
        }

        if (haptic_data->f0_en == 1)
        {
            haptic_data->f0_en = 0;
        }
    }

#ifdef AAC_RICHTAP_SUPPORT
    ret = richtap_irq_handler(haptic_data);
    if (ret >= 0)
    {
        return IRQ_HANDLED;
    }
#endif

    if (haptic_data->func->is_irq_fifo_ae(haptic_data))
    {
        if (haptic_data->gp_buf == NULL)
        {
            goto irq_exit;
        }
        data_size = haptic_data->chip_config.fifo_size - haptic_data->chip_config.fifo_ae;
        fifo_used = kfifo_len(&haptic_data->stream_fifo);
        data_size = min(data_size, fifo_used);
        data_size = kfifo_out(&haptic_data->stream_fifo, haptic_data->gp_buf, data_size);
        if (data_size > 0)
        {
            ics_dbg("kfifo out %d\n", data_size);
            if (waitqueue_active(&haptic_data->kfifo_wait_q))
            {
                atomic_set(&haptic_data->kfifo_available, 1);
                wake_up_interruptible(&haptic_data->kfifo_wait_q);
            }            
            ret = haptic_data->func->set_stream_data(haptic_data, haptic_data->gp_buf, data_size);
            if (ret < 0)
            {
                goto irq_exit;
            }
        }
    }

irq_exit:
    atomic_set(&haptic_data->is_in_rtp_loop, 0);
    ics_dbg("%s: wake up wait_q\n", __func__);
    wake_up_interruptible(&haptic_data->wait_q);
    return IRQ_HANDLED;
}

static void brake_guard_work_routine(struct work_struct *work)
{
    struct ics_haptic_data *haptic_data = container_of(work, struct ics_haptic_data,
                           brake_guard_work);

    int32_t ret = 0;
    int32_t brake_timeout = 0;

    while(1)
    {
        mutex_lock(&haptic_data->lock);
        ret = haptic_data->func->get_sys_state(haptic_data);
        mutex_unlock(&haptic_data->lock);
        if (ret < 0)
        {
            return;
        }

        if (haptic_data->sys_state == 0x02)
        {
            brake_timeout++;
            if (brake_timeout >= 25)  //over 50ms
            {
                haptic_data->chip_config.brake_en = 0;
                mutex_lock(&haptic_data->lock);
                ret = haptic_data->func->chip_init(haptic_data, 
                    haptic_data->waveform_data, haptic_data->waveform_size);
                mutex_unlock(&haptic_data->lock);

                return;
            }
        }
        else if (haptic_data->sys_state == 0x01)
        {
            return;
        }

        mdelay(2);
    }
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
    struct ics_haptic_data *haptic_data = container_of(timer, struct ics_haptic_data, timer);

    haptic_data->activate_state = 0;
    schedule_work(&haptic_data->vibrator_work);

    return HRTIMER_NORESTART;
}

static void vibrator_work_routine(struct work_struct *work)
{
    struct ics_haptic_data *haptic_data = container_of(work, struct ics_haptic_data,
                           vibrator_work);
    uint8_t buf[6];

    mutex_lock(&haptic_data->lock);
    haptic_data->func->play_stop(haptic_data);
    if (haptic_data->activate_state)
    {
        buf[0] = 0x01;
        buf[1] = 0x00;
        buf[2] = 0x7F;
        buf[3] = 0x00;  //fixed num0 waveform buf[3] = (uint8_t)haptic_data->ram_wave_index;
        buf[4] = 0x00;
        buf[5] = 0x00;
        haptic_data->func->set_play_list(haptic_data, buf, sizeof(buf));
        haptic_data->func->set_play_mode(haptic_data, PLAY_MODE_RAM);
        haptic_data->func->play_go(haptic_data);
        // run ms time
        hrtimer_start(&haptic_data->timer, ktime_set(haptic_data->duration / 1000,
            (haptic_data->duration % 1000) * 1000000), HRTIMER_MODE_REL);
    }
    mutex_unlock(&haptic_data->lock);
}

static void preset_work_routine(struct work_struct *work)
{
    struct ics_haptic_data *haptic_data = container_of(work, struct ics_haptic_data, preset_work);
    int32_t ret = 0;
    const struct firmware *preset_file;
    int32_t data_size, src_offset, batch_size, dst_size;
    bool resample_flag;
    uint32_t chip_fifo_size = haptic_data->chip_config.list_base_addr;

    mutex_lock(&haptic_data->preset_lock);
    ret = request_firmware(&preset_file,
                   preset_waveform_name[haptic_data->preset_wave_index], haptic_data->dev);
    if (ret < 0)
    {
        ics_err("%s: failed to read preset file %s\n", __func__,
            preset_waveform_name[haptic_data->preset_wave_index]);
        mutex_unlock(&haptic_data->preset_lock);
        return;
    }

    resample_flag = (abs((int32_t)haptic_data->chip_config.f0
        - (int32_t)haptic_data->chip_config.sys_f0) > RESAMPLE_THRESHOLD);

    data_size = (resample_flag == true)
        ? (preset_file->size * haptic_data->chip_config.sys_f0 / haptic_data->chip_config.f0 + 1)
        : preset_file->size;
    if (data_size > MAX_STREAM_FIFO_SIZE)
    {
        kfifo_free(&haptic_data->stream_fifo);
        ret = kfifo_alloc(&haptic_data->stream_fifo, data_size, GFP_KERNEL);
        if (ret < 0)
        {
            ics_err("%s: failed to allocate fifo for stream!\n", __func__);
            return;
        }
    }
    kfifo_reset(&haptic_data->stream_fifo);
    ics_resample_reset();
    src_offset = 0;
    if (resample_flag == true)
    {
        while (src_offset < preset_file->size)
        {
            batch_size = min(haptic_data->chip_config.ram_size, (uint32_t)(preset_file->size - src_offset));
            dst_size = ics_resample(
                preset_file->data + src_offset,
                batch_size,
                haptic_data->chip_config.sys_f0,
                haptic_data->gp_buf,
                haptic_data->chip_config.ram_size,
                haptic_data->chip_config.f0);
            kfifo_in(&haptic_data->stream_fifo, haptic_data->gp_buf, dst_size);
            ics_info("%s: src_offset=%d, batch_size=%d, dst_size=%d", __func__, src_offset, batch_size, dst_size);
            src_offset += batch_size;
        }
    }
    else
    {
        kfifo_in(&haptic_data->stream_fifo, preset_file->data, preset_file->size);
    }
    mutex_unlock(&haptic_data->preset_lock);
    release_firmware(preset_file);

    mutex_lock(&haptic_data->lock);
    haptic_data->func->play_stop(haptic_data);
    haptic_data->func->get_irq_state(haptic_data);
    haptic_data->func->clear_stream_fifo(haptic_data);
    haptic_data->func->set_play_mode(haptic_data, PLAY_MODE_STREAM);
    haptic_data->func->play_go(haptic_data);
    send_stream_data(haptic_data, chip_fifo_size);

    mutex_unlock(&haptic_data->lock);
}

#ifdef TIMED_OUTPUT
static int vibrator_get_time(struct timed_output_dev *dev)
{
    struct ics_haptic_data *haptic_data = container_of(dev, struct ics_haptic_data, vib_dev);

    if (hrtimer_active(&haptic_data->timer))
    {
        ktime_t r = hrtimer_get_remaining(&haptic_data->timer);
        return ktime_to_ms(r);
    }
    return 0;
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
    struct ics_haptic_data *haptic_data = container_of(dev, struct ics_haptic_data, vib_dev);

    mutex_lock(&haptic_data->lock);

    haptic_data->func->play_stop(haptic_data);
    if (value > 0)
    {
        //TODO:
        haptic_data->func->set_play_mode(haptic_data, PLAY_MODE_RAM);
        haptic_data->func->play_go(haptic_data);
    }
    mutex_unlock(&haptic_data->lock);
}
#else
/*
static enum led_brightness brightness_get(struct led_classdev *vdev)
{
    struct ics_haptic_data *haptic_data = container_of(vdev, struct ics_haptic_data, vib_dev);

    return haptic_data->amplitude;
}
*/
/*
static void brightness_set(struct led_classdev *vdev, enum led_brightness level)
{
    struct ics_haptic_data *haptic_data = container_of(vdev, struct ics_haptic_data, vib_dev);

    haptic_data->amplitude = level;
    mutex_lock(&haptic_data->lock);
    haptic_data->func->play_stop(haptic_data);
    if (haptic_data->amplitude > 0)
    {
        // TODO: Build map between amplitude and gain
        haptic_data->func->set_play_mode(haptic_data, PLAY_MODE_RAM);
        haptic_data->func->play_go(haptic_data);
    }
    mutex_unlock(&haptic_data->lock);
}
*/
#endif

static int32_t vibrator_init(struct ics_haptic_data *haptic_data)
{
    int ret = 0;

#ifdef TIMED_OUTPUT
    ics_info("%s: TIMED_OUTPUT framework!\n", __func__);
    haptic_data->vib_dev.name = haptic_data->vib_name;
    haptic_data->vib_dev.get_time = vibrator_get_time;
    haptic_data->vib_dev.enable = vibrator_enable;

    ret = timed_output_dev_register(&haptic_data->vib_dev);
    if (ret < 0)
    {
        ics_err("%s: failed to create timed output dev!\n", __func__);
        return ret;
    }
    ret = sysfs_create_group(&haptic_data->vib_dev.dev->kobj,
                 &ics_haptic_attribute_group);
    if (ret < 0)
    {
        ics_err("%s: failed to create sysfs attr files!\n", __func__);
        return ret;
    }
#else
    ics_info("%s: create sysfs!\n", __func__);
    //haptic_data->vib_dev.name = haptic_data->vib_name;
    //haptic_data->vib_dev.brightness_get = brightness_get;
    //haptic_data->vib_dev.brightness_set = brightness_set;
    ret = sysfs_create_group(&haptic_data->client->dev.kobj, &ics_haptic_attribute_group);
    if (ret < 0)
    {
        ics_err("%s: error creating sysfs attr files\n", __func__);
        return ret;
    }
#endif
    hrtimer_init(&haptic_data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    haptic_data->timer.function = vibrator_timer_func;

    INIT_WORK(&haptic_data->vibrator_work, vibrator_work_routine);
    INIT_WORK(&haptic_data->preset_work, preset_work_routine);
    mutex_init(&haptic_data->lock);
    mutex_init(&haptic_data->preset_lock);

    return 0;
}

static void load_chip_config(const struct firmware *config_fw, void *context)
{
    struct ics_haptic_data *haptic_data = context;
    //struct ics_haptic_chip_config *chip_config;
    int ret = 0;

    ics_info("load chip config\n");

    if (!config_fw)
    {
        ics_err("%s: failed to read %s\n", __func__, haptic_config_name);
        release_firmware(config_fw);
        return;
    }
    ics_info("chip config firmware size = %lu\n", config_fw->size);
    haptic_data->waveform_size = config_fw->size;
    haptic_data->waveform_data = vmalloc(config_fw->size);
    if (haptic_data->waveform_data == NULL)
    {
        ics_err("%s: failed to allocate memory for waveform data\n", __func__);
        goto on_chip_config_err;
    }
    memcpy(haptic_data->waveform_data, (uint8_t*)(config_fw->data), config_fw->size);
    ret = haptic_data->func->chip_init(haptic_data, 
        haptic_data->waveform_data, haptic_data->waveform_size);
    if (ret)
    {
        ics_err("%s: failed to initialize chip!\n", __func__);
    }
    else
    {
        haptic_data->chip_initialized = true;
        mutex_lock(&haptic_data->lock);
        ret = haptic_data->func->get_f0(haptic_data);
        mutex_unlock(&haptic_data->lock);
        if (ret < 0){
            ics_err("%s: failed to cali f0! ret= %d\n", __func__, ret);
        }
        if (abs((int32_t)haptic_data->chip_config.f0 -(int32_t)haptic_data->chip_config.sys_f0) > RESAMPLE_THRESHOLD){
        ics_info("resample ram waveform, f0 %d sys_f0 %d\n", haptic_data->chip_config.f0,haptic_data->chip_config.sys_f0);
        haptic_data->func->resample_ram_waveform(haptic_data);
        }
    }

on_chip_config_err:

    release_firmware(config_fw);
}

static void chip_init_work_routine(struct work_struct *work)
{
    struct ics_haptic_data *haptic_data = container_of(work, struct ics_haptic_data, chip_init_work.work);

    haptic_data->chip_initialized = false;
    request_firmware_nowait(
        THIS_MODULE,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 1)
        FW_ACTION_UEVENT,
#else
        FW_ACTION_HOTPLUG,
#endif
        haptic_config_name, haptic_data->dev, GFP_KERNEL,
        haptic_data, load_chip_config);
}

static void initialize_chip(struct ics_haptic_data *haptic_data)
{
    int ram_timer_val = 1000;

    INIT_DELAYED_WORK(&haptic_data->chip_init_work, chip_init_work_routine);
    schedule_delayed_work(&haptic_data->chip_init_work, msecs_to_jiffies(ram_timer_val));
}


#ifdef ENABLE_PIN_CONTROL
static int ics_select_pin_ctl(struct ics_haptic_data *haptic_data, const char *name)
{
        size_t i;
        int rc;

        for (i = 0; i < ARRAY_SIZE(haptic_data->pinctrl_state); i++) {
                const char *n = pctl_names[i];

                if (!strncmp(n, name, strlen(n))) {
                        rc = pinctrl_select_state(haptic_data->ics_pinctrl,
                                                  haptic_data->pinctrl_state[i]);
                        if (rc)
                                ics_err("%s: cannot select '%s'\n", __func__,
                                       name);
                        else
                                ics_err("%s: Selected '%s'\n", __func__, name);
                        goto exit;
                }
        }

        rc = -EINVAL;
        ics_info("%s: '%s' not found\n", __func__, name);

exit:
        return rc;
}

static int ics_set_interrupt(struct ics_haptic_data *haptic_data)
{
        int rc = ics_select_pin_ctl(haptic_data, "ics_interrupt_active");
        return rc;
}

static int ics_control_rest_pin(struct ics_haptic_data *haptic_data)
{
        int ret = 0;

        ret = ics_select_pin_ctl(haptic_data, "ics_reset_active");
        if (ret < 0) {
                ics_err("%s select reset failed!\n", __func__);
                return ret;
        }
        usleep_range(5000, 5500);
        ret = ics_select_pin_ctl(haptic_data, "ics_reset_reset");
        if (ret < 0) {
                ics_err("%s select reset failed!\n", __func__);
                return ret;
        }
        usleep_range(5000, 5500);
        ret = ics_select_pin_ctl(haptic_data, "ics_reset_active");
        usleep_range(8000, 8500);
        if (ret < 0) {
                ics_err("%s select reset failed!\n", __func__);
                return ret;
        }
        return 0;
}
#endif

static int32_t haptic_parse_dt(struct ics_haptic_data *haptic_data)
{
    const char *str_val = NULL;
    struct device_node *dev_node = haptic_data->dev->of_node;
    if (NULL == dev_node) 
    {
        ics_err("%s: no device tree node was found\n", __func__);
        return -EINVAL;
    }

    haptic_data->gpio_en = of_get_named_gpio(dev_node, "gpio-en", 0);
    if (haptic_data->gpio_en < 0) 
    {
        ics_err("%s: no gpio-en provided\n", __func__);
    }
    else 
    {
        ics_info("gpio-en provided ok\n");
    }

    haptic_data->gpio_irq = of_get_named_gpio(dev_node, "gpio-irq", 0);
    if (haptic_data->gpio_irq < 0) 
    {
        ics_err("%s: no gpio-irq provided\n", __func__);
    }
    else 
    {
        ics_info("gpio-irq provided ok\n");
    }

    if (of_property_read_string(dev_node, "device-name", &str_val))
    {
        ics_err("%s: can NOT find device name in DT!\n", __func__);
        memcpy(haptic_data->vib_name, DEFAULT_DEV_NAME, sizeof(DEFAULT_DEV_NAME));
    }
    else
    {
        memcpy(haptic_data->vib_name, str_val, strlen(str_val));
        ics_info("provided device name is : %s\n", haptic_data->vib_name);
    }

    if (of_property_read_string(dev_node, "richtap-name", &str_val))
    {
        ics_err("%s: can NOT find richtap name in DT!\n", __func__);
        memcpy(haptic_data->richtap_misc_name, DEFAULT_RICHTAP_NAME, sizeof(DEFAULT_RICHTAP_NAME));
    }
    else
    {
        memcpy(haptic_data->richtap_misc_name, str_val, strlen(str_val));
        ics_info("provided richtap name is : %s\n", haptic_data->richtap_misc_name);
    }

    return 0;
}

int32_t haptic_hw_reset(struct ics_haptic_data *haptic_data)
{
    int rc = 0;

    if (!haptic_data->enable_pin_control) {
        if (haptic_data && gpio_is_valid(haptic_data->gpio_en)) {
            gpio_set_value_cansleep(haptic_data->gpio_en, 0);
            usleep_range(1000, 2000);
            gpio_set_value_cansleep(haptic_data->gpio_en, 1);
            usleep_range(500, 600);
        } else {
            ics_err("%s:  failed\n", __func__);
        }
    } else {
#ifdef ENABLE_PIN_CONTROL
         rc = ics_control_rest_pin(haptic_data);
         if (rc < 0)
             return rc;
#endif
    }

    return rc;
}

static struct regmap_config ics_haptic_regmap =
{
    .reg_bits = 8,
    .val_bits = 8,
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)
static int ics_haptic_probe(struct i2c_client *client)
#else
static int ics_haptic_probe(struct i2c_client *client, const struct i2c_device_id *id)
#endif
{
    int32_t ret = 0;
    struct ics_haptic_data *haptic_data;
    struct device* dev = &client->dev;

#ifdef ENABLE_PIN_CONTROL
    int i;
#endif

    ics_info("ics haptic driver! ver: %s\n", ICS_HAPTIC_VERSION);
    ics_info("ics haptic probe! addr=0x%X\n", client->addr);

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
    {
        ics_err("%s: failed to check i2c functionality!\n", __func__);
        return -EIO;
    }

    haptic_data = devm_kzalloc(&client->dev, sizeof(struct ics_haptic_data), GFP_KERNEL);
    if (haptic_data == NULL)
    {
        ics_err("%s: failed to allocate memory for ics haptic data!\n", __func__);
        ret = -ENOMEM;
        goto probe_err;
    }

    haptic_data->dev = dev;
    haptic_data->client = client;
    dev_set_drvdata(dev, haptic_data);
    i2c_set_clientdata(client, haptic_data);

    haptic_data->regmap = devm_regmap_init_i2c(client, &ics_haptic_regmap);
    if (IS_ERR(haptic_data->regmap))
    {
        ret = PTR_ERR(haptic_data->regmap);
        ics_err("%s: failed to initialize register map: %d\n", __func__, ret);
        goto probe_err;
    }

    // TODO: assign function list according to chip id
    haptic_data->func = &rt6010_func_list;

    // enable and irp gpio configuration
    haptic_parse_dt(haptic_data);
    haptic_data->enable_pin_control = 0;

#ifdef ENABLE_PIN_CONTROL
    haptic_data->ics_pinctrl = devm_pinctrl_get(&client->dev);
    if (IS_ERR(haptic_data->ics_pinctrl)) {
        if (PTR_ERR(haptic_data->ics_pinctrl) == -EPROBE_DEFER) {
                ics_info("%s pinctrl not ready\n", __func__);
                ret = -EPROBE_DEFER;
                return ret;
        }
        ics_info("%s Target does not use pinctrl\n", __func__);
        haptic_data->ics_pinctrl = NULL;
        ret = -EINVAL;
        return ret;
    }
    for (i = 0; i < ARRAY_SIZE(haptic_data->pinctrl_state); i++) {
        const char *n = pctl_names[i];
        struct pinctrl_state *state =
            pinctrl_lookup_state(haptic_data->ics_pinctrl, n);
        if (!IS_ERR(state)) {
            ics_info("%s: found pin control %s\n", __func__, n);
            haptic_data->pinctrl_state[i] = state;
            haptic_data->enable_pin_control = 1;
            ics_set_interrupt(haptic_data);
            continue;
        }
        ics_info("%s cannot find '%s'\n", __func__, n);
    }
#endif
    if (!haptic_data->enable_pin_control) {
        if (gpio_is_valid(haptic_data->gpio_en)) {
            ret = devm_gpio_request_one(&client->dev,
                    haptic_data->gpio_en,
                    GPIOF_OUT_INIT_LOW,
                    "ics_rst");
            if (ret) {
                ics_err("%s: rst request failed\n", __func__);
                goto probe_err;
            }
        }else{
            ret = -EINVAL;
            ics_err("%s: cannot find gpio_en\n", __func__);
            goto probe_err;
        }
    }

    if (gpio_is_valid(haptic_data->gpio_en))
    {
        haptic_hw_reset(haptic_data);
    }
    ret = vibrator_init(haptic_data);
    if (ret < 0)
    {
        ics_err("%s: failed to initialize vibrator interfaces! ret = %d\n", __func__, ret);
        goto probe_err;
    }

    // following initialization steps are not necessary for group broadcast device
    if (client->addr == 0x5C)
    {
        return ret;
    }

    ret = haptic_data->func->get_chip_id(haptic_data);
    if (ret < 0)
    {
        ics_err("%s: failed to get chipid!\n", __func__);
        goto probe_err;
    }
	
    ret = kfifo_alloc(&haptic_data->stream_fifo, MAX_STREAM_FIFO_SIZE, GFP_KERNEL);
    if (ret < 0)
    {
        ics_err("%s: failed to allocate fifo for stream!\n", __func__);
        ret = -ENOMEM;
        goto probe_err;
    }

    haptic_data->gp_buf = kmalloc(GP_BUFFER_SIZE, GFP_KERNEL);
    if (haptic_data->gp_buf == NULL)
    {
        ics_err("%s: failed to allocate memory for gp buffer\n", __func__);
        goto probe_err;
    }

    initialize_chip(haptic_data);
    INIT_WORK(&haptic_data->brake_guard_work, brake_guard_work_routine);

    // register irq handler
    if (gpio_is_valid(haptic_data->gpio_irq))
    {
        ret = devm_request_threaded_irq(dev, gpio_to_irq(haptic_data->gpio_irq),
            NULL, ics_haptic_irq_handler, IRQF_TRIGGER_LOW | IRQF_ONESHOT | IRQF_SHARED,
            ICS_HAPTIC_NAME, haptic_data);
        if (ret < 0) 
        {
            ics_err("%s: failed to request threaded irq! ret = %d\n", __func__, ret);
            goto probe_err;
        }
    }

    // initialize input device
    ret = ics_input_dev_register(haptic_data);
    if (ret < 0)
    {
        ics_err("%s: failed to initialize input device! ret = %d\n", __func__, ret);
        goto probe_err;
    }

#ifdef AAC_RICHTAP_SUPPORT
    ret = richtap_misc_register(haptic_data);
    if (ret < 0)
    {
        ics_err("%s: failed to initialize richtap device! ret = %d\n", __func__, ret);
        goto probe_err;
    }
#endif

    return 0;

probe_err:
    if (haptic_data != NULL)
    {
        if (kfifo_initialized(&haptic_data->stream_fifo)) 
        {
            kfifo_free(&haptic_data->stream_fifo);
        }
        if (haptic_data->gp_buf != NULL)
        {
            kfree(haptic_data->gp_buf);
        }
        //devm_free_irq(&client->dev, gpio_to_irq(haptic_data->gpio_irq), haptic_data);
        if (gpio_is_valid(haptic_data->gpio_en))
        {
            //devm_kfree(&client->dev, haptic_data);
        }
        devm_kfree(&client->dev, haptic_data);
    }

    return ret;
}

static void ics_haptic_remove(struct i2c_client *client)
{
    struct ics_haptic_data *haptic_data = i2c_get_clientdata(client);

#ifdef AAC_RICHTAP_SUPPORT
    richtap_misc_remove(haptic_data);
#endif
    ics_input_dev_remove(haptic_data);

    cancel_work_sync(&haptic_data->preset_work);
    cancel_work_sync(&haptic_data->vibrator_work);
    cancel_work_sync(&haptic_data->brake_guard_work);
    hrtimer_cancel(&haptic_data->timer);

    mutex_destroy(&haptic_data->lock);
    mutex_destroy(&haptic_data->preset_lock);

    kfifo_free(&haptic_data->stream_fifo);
    if (haptic_data->gp_buf != NULL)
    {
        kfree(haptic_data->gp_buf);
    }

    devm_free_irq(&client->dev, gpio_to_irq(haptic_data->gpio_irq), haptic_data);
    if (gpio_is_valid(haptic_data->gpio_en))
    {
        //devm_kfree(&client->dev, haptic_data);
    }
}

static int __maybe_unused ics_haptic_suspend(struct device *dev)
{
    int ret = 0;

    return ret;
}

static int __maybe_unused ics_haptic_resume(struct device *dev)
{
    int ret = 0;

    return ret;
}

static SIMPLE_DEV_PM_OPS(ics_haptic_pm_ops, ics_haptic_suspend, ics_haptic_resume);
static const struct i2c_device_id ics_haptic_id[] =
{
    { ICS_HAPTIC_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, ics_haptic_id);

static struct of_device_id ics_haptic_dt_match[] =
{
    { .compatible = "ics,haptic_rt" },
    { },
};

static struct i2c_driver ics_haptic_driver =
{
    .driver =
    {
        .name = ICS_HAPTIC_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(ics_haptic_dt_match),
        .pm = &ics_haptic_pm_ops,
    },
    .id_table = ics_haptic_id,
    .probe = ics_haptic_probe,
    .remove = ics_haptic_remove,
};

module_i2c_driver(ics_haptic_driver);

MODULE_DESCRIPTION("ICS Haptic Driver");
MODULE_AUTHOR("chenmaomao@icsense.com.cn, ICSense Semiconductor Co., Ltd");
MODULE_LICENSE("GPL v2");
