/*
 * drivers/haptic/input_drv.c
 *
 * Copyright (c) 2023 ICSense Semiconductor CO., LTD
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation (version 2 of the License only).
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <linux/kfifo.h>
#include <linux/syscalls.h>
#include <linux/power_supply.h>
#include <linux/pm_qos.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/regmap.h>
#include <linux/mman.h>

#include "input_drv.h"
#include "rt6010.h"
#include "haptic_util.h"

extern char preset_waveform_name[][MAX_PRESET_NAME_LEN];
extern int32_t ics_rtp_name_len;
extern int32_t send_stream_data(struct ics_haptic_data *haptic_data, uint32_t fifo_available_size);

//static int32_t ics_pump_rb_data(struct ics_haptic_data *haptic_data);

static int32_t ics_haptic_upload_effect(
    struct input_dev *dev,
    struct ff_effect *effect,
    struct ff_effect *old)
{
    struct ics_haptic_data *haptic_data = input_get_drvdata(dev);
    //struct qti_hap_play_info *play = &haptic_data->play;
    s16 data[CUSTOM_DATA_LEN];
    ktime_t rem;
    s64 time_us;
    int32_t ret = 0;

    ics_dbg("%s: effect->type=0x%x,FF_CONSTANT=0x%x,FF_PERIODIC=0x%x\n",
        __func__, effect->type, FF_CONSTANT, FF_PERIODIC);

    if (hrtimer_active(&haptic_data->input_timer))
    {
        rem = hrtimer_get_remaining(&haptic_data->input_timer);
        time_us = ktime_to_us(rem);
        ics_info("waiting for playing clear sequence: %lld us\n", time_us);
        usleep_range(time_us, time_us + 100);
    }
    haptic_data->effect_type = effect->type;
    mutex_lock(&haptic_data->input_lock);
    while (atomic_read(&haptic_data->exit_in_rtp_loop))
    {
       ics_info("%s  going to waiting input_lock\n", __func__);
       mutex_unlock(&haptic_data->input_lock);
       ics_info("%s  going to waiting rtp  exit\n", __func__);
       ret = wait_event_interruptible(haptic_data->stop_wait_q,
               atomic_read(&haptic_data->exit_in_rtp_loop) == 0);
       ics_info("%s wakeup\n", __func__);
       if (ret == -ERESTARTSYS)
       {
           mutex_unlock(&haptic_data->input_lock);
           ics_err("%s wake up by signal return erro\n", __func__);
           return ret;
       }
       mutex_lock(&haptic_data->input_lock);
    }

    if (haptic_data->effect_type == FF_CONSTANT)
    {
        ics_dbg("%s: effect_type is  FF_CONSTANT! id = %d, duration = %d\n",
            __func__, effect->id, effect->replay.length);
        haptic_data->duration = effect->replay.length;
        haptic_data->activate_mode = PLAY_MODE_RAM_LOOP;
        haptic_data->effect_id = 0;
    }
    else if (haptic_data->effect_type == FF_PERIODIC)
    {
        if (haptic_data->effects_count == 0)
        {
            mutex_unlock(&haptic_data->input_lock);
            return -EINVAL;
        }

        ics_dbg("%s: effect_type is  FF_PERIODIC!\n", __func__);
        if (copy_from_user(data, effect->u.periodic.custom_data,
                   sizeof(s16) * CUSTOM_DATA_LEN))
        {
            mutex_unlock(&haptic_data->input_lock);
            return -EFAULT;
        }

        haptic_data->effect_id = data[0];
        haptic_data->magnitude = effect->u.periodic.magnitude;
        ics_dbg("%s: haptic_data->effect_id =%d\n", __func__, haptic_data->effect_id);

        if (haptic_data->effect_id < 0 ||
            haptic_data->effect_id > haptic_data->effect_max)
        {
            mutex_unlock(&haptic_data->input_lock);
            return 0;
        }
        haptic_data->is_custom_wave = 0;

        if (haptic_data->effect_id < haptic_data->chip_config.ram_effect_count)
        {
            haptic_data->activate_mode = PLAY_MODE_RAM;
            ics_dbg("%s: haptic_data->effect_id=%d , haptic_data->activate_mode = %d, play_time=%d, play_time=%d\n",
                __func__, haptic_data->effect_id, haptic_data->activate_mode,
                haptic_data->chip_config.play_time[haptic_data->effect_id]/1000,
                haptic_data->chip_config.play_time[haptic_data->effect_id]);
            /*second data*/
            data[1] = haptic_data->chip_config.play_time[haptic_data->effect_id]/1000;
            /*millisecond data*/
            data[2] = haptic_data->chip_config.play_time[haptic_data->effect_id];
        }
        else
        {
            haptic_data->activate_mode = PLAY_MODE_STREAM;
            ics_dbg("%s: haptic_data->effect_id=%d , haptic_data->activate_mode = %d\n",
                __func__, haptic_data->effect_id, haptic_data->activate_mode);

            if (haptic_data->effect_id == haptic_data->effect_max)
            {
                haptic_data->is_custom_wave = 1;
                kfifo_reset(&haptic_data->stream_fifo);
                atomic_set(&haptic_data->wave_data_done, 0);
                atomic_set(&haptic_data->exit_write_loop, 0);
                //rb_init();
                data[1] = 30;
                data[2] = 0;
            }
            else
            {
                size_t size_of_play_time = sizeof(haptic_data->chip_config.play_time) / sizeof(haptic_data->chip_config.play_time[0]);
                ics_dbg("%s: size of play_time %zu\n", __func__, size_of_play_time);
                if (haptic_data->effect_id <= size_of_play_time)
                {
                    /*second data*/
                    data[1] = haptic_data->chip_config.play_time[haptic_data->effect_id]/1000;
                    /*millisecond data*/
                    data[2] = haptic_data->chip_config.play_time[haptic_data->effect_id];
                }
                else
                {
                    data[1] = 30;
                    data[2] = 0;
                }
            }
            ics_dbg("%s: play_time %d, play_time=%d\n", __func__, data[1], data[2]);
        }

        if (copy_to_user(effect->u.periodic.custom_data, data,
            sizeof(s16) * CUSTOM_DATA_LEN))
        {
            mutex_unlock(&haptic_data->input_lock);
            return -EFAULT;
        }
    }
    else
    {
        ics_err("%s Unsupported effect type: %d\n", __func__, effect->type);
    }

    mutex_unlock(&haptic_data->input_lock);
    return 0;
}

int32_t ics_haptic_playback(struct input_dev *dev, int32_t effect_id, int32_t val)
{
    struct ics_haptic_data *haptic_data = input_get_drvdata(dev);
    int ret = 0;

    ics_dbg("%s: effect_id=%d , activate_mode = %d val = %d\n",
        __func__, haptic_data->effect_id, haptic_data->activate_mode, val);

    if (val > 0)
    {
        haptic_data->state = 1;
    }
    if (val <= 0)
    {
        haptic_data->state = 0;
    }
    hrtimer_cancel(&haptic_data->input_timer);

    if (haptic_data->effect_type == FF_CONSTANT &&
        haptic_data->activate_mode == PLAY_MODE_RAM_LOOP)
    {
        ics_dbg("%s: enter cont_mode\n", __func__);
        queue_work(haptic_data->input_work_queue, &haptic_data->input_vibrator_work);
    }
    else if (haptic_data->effect_type == FF_PERIODIC &&
        haptic_data->activate_mode == PLAY_MODE_RAM)
    {
        ics_dbg("%s: enter ram_mode\n", __func__);
        queue_work(haptic_data->input_work_queue, &haptic_data->input_vibrator_work);
    }
    else if ((haptic_data->effect_type == FF_PERIODIC) &&
        haptic_data->activate_mode == PLAY_MODE_STREAM)
    {
        ics_dbg("%s: enter rtp_mode\n", __func__);
        queue_work(haptic_data->input_work_queue, &haptic_data->input_rtp_work);
        /*if we are in the play mode, force to exit*/
        if (val == 0)
        {
            ics_dbg("%s: playback val = 0\n", __func__);
            atomic_set(&haptic_data->exit_in_rtp_loop, 0);
            //rb_force_exit();
            if (waitqueue_active(&haptic_data->kfifo_wait_q)){
                atomic_set(&haptic_data->kfifo_available, 1);
                wake_up_interruptible(&haptic_data->kfifo_wait_q);
            }
            atomic_set(&haptic_data->exit_write_loop, 1);
            atomic_set(&haptic_data->wave_data_done, 1);
            wake_up_interruptible(&haptic_data->stop_wait_q);
        }
    }
    else
    {
        /*other mode */
        ics_dbg("%s: enter other mode\n", __func__);
    }

    return ret;
}

static int32_t ics_haptic_erase(struct input_dev *dev, int32_t effect_id)
{
    struct ics_haptic_data *haptic_data = input_get_drvdata(dev);
    int rc = 0;

    ics_dbg("%s: enter\n", __func__);
    haptic_data->effect_type = 0;
    haptic_data->is_custom_wave = 0;
    haptic_data->duration = 0;
    return rc;
}

static void ics_input_set_gain_work_routine(struct work_struct *work)
{
    struct ics_haptic_data *haptic_data = container_of(work, struct ics_haptic_data, input_set_gain_work);
    int32_t ret = 0;
    
    if (haptic_data->new_gain >= 0x7FFF) {
        haptic_data->level = 0x80;
    } else if (haptic_data->new_gain <= 0x3FFF) {
        haptic_data->level = 0x1E;
    } else {
        haptic_data->level = (haptic_data->new_gain - 16383) / 128;
    }

    if (haptic_data->level < 0x1E) {
        haptic_data->level = 0x1E;
    }
    ics_info("%s: set gain, new gain = %x, reg %x\n",
            __func__, haptic_data->new_gain, haptic_data->level);
    ret = haptic_data->func->set_gain(haptic_data, haptic_data->level);
    if (ret < 0) {
        pr_info("%s: check_error_return! ret = %d\n", __func__, ret);
    }
}

static int32_t ics_haptic_play_go(struct ics_haptic_data *haptic_data)
{
    ics_dbg("%s in\n", __func__);
    haptic_data->func->play_go(haptic_data);
    haptic_data->input_pre_time = ktime_get();
    ics_dbg("%s out\n", __func__);
    return 0;
}

static int32_t ics_haptic_play_stop(struct ics_haptic_data *haptic_data)
{
    ics_dbg("%s in\n", __func__);
    while (1) {
        if ((haptic_data->func->get_sys_state(haptic_data) & RT6010_BIT_SYS_MODE_MASK)
                == RT6010_BIT_SYS_MODE_READY) {
            break;
        } else {
            haptic_data->input_cur_time = ktime_get();
            haptic_data->input_interval_us =
                ktime_to_us(ktime_sub(haptic_data->input_cur_time, haptic_data->input_pre_time));
            if (haptic_data->input_interval_us > 30000)
                break;
            ics_dbg("%s:play time us = %lu, less than 30ms, wait\n",
                    __func__, haptic_data->input_interval_us);
            usleep_range(5000,5500);
        }
    }
    haptic_data->func->play_stop(haptic_data);
    ics_dbg("%s out\n", __func__);
    return 0;
}

static void ics_haptic_set_gain(struct input_dev *dev, uint16_t gain)
{
    struct ics_haptic_data *haptic_data = input_get_drvdata(dev);

    ics_info("%s: set gain, new gain = %x\n",
            __func__, gain);
    haptic_data->new_gain = gain;
    queue_work(haptic_data->input_work_queue, &haptic_data->input_set_gain_work);
}

int32_t ics_input_effect_magnitude(struct ics_haptic_data *haptic_data)
{
    ics_dbg("%s: haptic_data->magnitude =0x%x\n", __func__,
         haptic_data->magnitude);
    if (haptic_data->magnitude >= 0x7FFF)
        haptic_data->level = 0x80; /*128*/
    else if (haptic_data->magnitude <= 0x3FFF)
        haptic_data->level = 0x1E; /*30*/
    else
        haptic_data->level = (haptic_data->magnitude - 16383) / 128;
    if (haptic_data->level < 0x1E)
        haptic_data->level = 0x1E; /*30*/

    ics_info("%s: haptic->level =0x%x\n", __func__, haptic_data->level);
    return 0;
}

static void ics_input_rtp_work_routine(struct work_struct *work)
{
    struct ics_haptic_data *haptic_data = container_of(
        work, struct ics_haptic_data, input_rtp_work);

    const struct firmware *rtp_file;
    int ret = -1;
    int32_t data_size, src_offset, batch_size, dst_size;
    bool resample_flag;
    uint32_t chip_fifo_size = haptic_data->chip_config.list_base_addr;

    ics_info("%s: effect_id = %d state=%d activate_mode = %d\n", __func__,
        haptic_data->effect_id, haptic_data->state, haptic_data->activate_mode);

    if ((haptic_data->effect_id < 0) ||
        (haptic_data->effect_id > haptic_data->effect_max))
    {
        return;
    }

    mutex_lock(&haptic_data->input_lock);
    /* wait for irq to exit */
    atomic_set(&haptic_data->exit_in_rtp_loop, 1);
    while (atomic_read(&haptic_data->is_in_rtp_loop))
    {
        ics_info("%s  going to waiting input lock\n", __func__);
        mutex_unlock(&haptic_data->input_lock);
        ics_info("%s  going to waiting irq exit\n", __func__);
        ret = wait_event_interruptible(haptic_data->wait_q,
                atomic_read(&haptic_data->is_in_rtp_loop) == 0);
        ics_info("%s  wakeup\n", __func__);
        mutex_lock(&haptic_data->input_lock);
        if (ret == -ERESTARTSYS)
        {
            atomic_set(&haptic_data->exit_in_rtp_loop, 0);
            wake_up_interruptible(&haptic_data->stop_wait_q);
            mutex_unlock(&haptic_data->input_lock);
            ics_err("%s wake up by signal return erro\n", __func__);
            return;
        }
    }

    atomic_set(&haptic_data->exit_in_rtp_loop, 0);
    wake_up_interruptible(&haptic_data->stop_wait_q);

    /* how to force exit this call */
    if (haptic_data->is_custom_wave == 1 && haptic_data->state)
    {
        ics_err("%s buffer size %d, availbe size %d\n",
               __func__, haptic_data->chip_config.list_base_addr,
               kfifo_len(&haptic_data->stream_fifo));
        while (kfifo_len(&haptic_data->stream_fifo) < haptic_data->chip_config.list_base_addr &&
               (atomic_read(&haptic_data->wave_data_done) == 0))
        {
            mutex_unlock(&haptic_data->input_lock);
            ret = wait_event_interruptible(haptic_data->stop_wait_q,
                (kfifo_len(&haptic_data->stream_fifo) >= haptic_data->chip_config.list_base_addr) ||
                (atomic_read(&haptic_data->wave_data_done) == 1));
            //ics_info("%s  wakeup\n", __func__);
            ics_err("%s after wakeup sbuffer size %d, availbe size %d\n",
                   __func__, haptic_data->chip_config.list_base_addr,
                   kfifo_avail(&haptic_data->stream_fifo));
            if (ret == -ERESTARTSYS)
            {
                ics_err("%s wake up by signal return erro\n", __func__);
                return;
            }
            mutex_lock(&haptic_data->input_lock);
        }
    }

    ics_haptic_play_stop(haptic_data);
    haptic_data->func->clear_stream_fifo(haptic_data);
    haptic_data->func->get_irq_state(haptic_data);
    if (haptic_data->state)
    {
        pm_stay_awake(haptic_data->dev);
        /* boost voltage */
        haptic_data->func->set_bst_vol(haptic_data, haptic_data->chip_config.boost_vol);
        ics_input_effect_magnitude(haptic_data);
        haptic_data->func->set_gain(haptic_data, haptic_data->level);

        if (haptic_data->is_custom_wave == 0)
        {
            haptic_data->rtp_file_num = haptic_data->effect_id;
            ics_info("%s: haptic_data->rtp_file_num =%d\n", __func__,
                   haptic_data->rtp_file_num);
            if (haptic_data->rtp_file_num < 0)
            {
                haptic_data->rtp_file_num = 0;
            }
            if (haptic_data->rtp_file_num > (ics_rtp_name_len - 1))
            {
                haptic_data->rtp_file_num = ics_rtp_name_len - 1;
            }
            //haptic_data->rtp_routine_on = 1;
            /* fw loaded */
            ret = request_firmware(&rtp_file,
                    preset_waveform_name[haptic_data->rtp_file_num],
                    haptic_data->dev);
            if (ret < 0)
            {
                ics_err("%s: failed to read %s\n", __func__,
                    preset_waveform_name[haptic_data->rtp_file_num]);
                //haptic_data->rtp_routine_on = 0;
                pm_relax(haptic_data->dev);
                mutex_unlock(&haptic_data->input_lock);
                return;
            }
            ics_info("%s: rtp file name %s rtp_file->size =%zu\n", __func__,
                    preset_waveform_name[haptic_data->rtp_file_num], rtp_file->size);

            resample_flag = (abs((int32_t)haptic_data->chip_config.f0
                        - (int32_t)haptic_data->chip_config.sys_f0) > RESAMPLE_THRESHOLD);

            ics_info("%s: resample_flag %d f0 %d sys_f0 %d\n", __func__,
                    resample_flag, haptic_data->chip_config.f0, haptic_data->chip_config.sys_f0);

            data_size = (resample_flag == true)
                ? (rtp_file->size * haptic_data->chip_config.sys_f0 / haptic_data->chip_config.f0 + 1)
                : rtp_file->size;

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
                while (src_offset < rtp_file->size)
                {
                    batch_size = min(haptic_data->chip_config.ram_size, (uint32_t)(rtp_file->size - src_offset));
                    dst_size = ics_resample(
                        rtp_file->data + src_offset,
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
                kfifo_in(&haptic_data->stream_fifo, rtp_file->data, rtp_file->size);
            }

            release_firmware(rtp_file);
        }
        else
        {
            /*
            ret = ics_pump_rb_data(haptic_data);
            if (ret < 0)
            {
                pm_relax(haptic_data->dev);
                mutex_unlock(&haptic_data->input_lock);
                ics_err("%s: failed to pump data from ringbuffer to rtp fifo\n", __func__);
            }
            */
        }
        haptic_data->func->play_stop(haptic_data);
        haptic_data->func->get_irq_state(haptic_data);
        haptic_data->func->clear_stream_fifo(haptic_data);
        haptic_data->func->set_play_mode(haptic_data, PLAY_MODE_STREAM);
        ics_haptic_play_go(haptic_data);

        send_stream_data(haptic_data, chip_fifo_size);

        //haptic_data->rtp_routine_on = 0;
    }
    else
    {
        pm_relax(haptic_data->dev);
    }
    mutex_unlock(&haptic_data->input_lock);
}

static enum hrtimer_restart input_vibrator_timer_func(struct hrtimer *timer)
{
    struct ics_haptic_data *haptic_data = container_of(timer, struct ics_haptic_data, timer);
    ics_dbg("%s: enter\n", __func__);
    haptic_data->state = 0;
    schedule_work(&haptic_data->input_vibrator_work);
    ics_dbg("%s: exit\n", __func__);
    return HRTIMER_NORESTART;
}

static int32_t ics_haptic_play_effect_seq(struct ics_haptic_data *haptic_data)
{
    uint8_t buf[6];

    ics_info("%s: effect_id = %d state=%d activate_mode = %d\n", __func__,
        haptic_data->effect_id, haptic_data->state, haptic_data->activate_mode);

    haptic_data->func->play_stop(haptic_data);
    if (haptic_data->state)
    {
        buf[0] = 0x01;
        buf[1] = 0x00;
        if (haptic_data->activate_mode == PLAY_MODE_RAM)
        {
            ics_info("%s: PLAY_MODE_RAM activate_mode = %d\n", __func__, haptic_data->activate_mode);
            buf[2] = 0x01;
            buf[3] = (uint8_t)haptic_data->effect_id;
        }
        else if (haptic_data->activate_mode == PLAY_MODE_RAM_LOOP)
        {
            ics_info("%s: PLAY_MODE_RAM_LOOP activate_mode = %d\n", __func__, haptic_data->activate_mode);
            buf[2] = 0x7F;
            buf[3] = (uint8_t)haptic_data->effect_id;
        }
        buf[4] = 0x00;
        buf[5] = 0x00;
        haptic_data->func->set_play_list(haptic_data, buf, sizeof(buf));
        haptic_data->func->set_play_mode(haptic_data, PLAY_MODE_RAM);
        haptic_data->func->set_bst_vol(haptic_data, haptic_data->chip_config.boost_vol);
        ics_info("%s: level = %d\n", __func__, haptic_data->level);
        haptic_data->func->set_gain(haptic_data, haptic_data->level);
        haptic_data->func->play_go(haptic_data);
    }

    return 0;
}

static void ics_input_vibrator_work_routine(struct work_struct *work)
{
    struct ics_haptic_data *haptic_data = container_of(
        work, struct ics_haptic_data, input_vibrator_work);
    uint32_t reg_val = 0, count = 40;

    ics_dbg("%s enter\n", __func__);
    ics_info("%s: effect_id = %d state=%d activate_mode = %d duration = %d\n",
        __func__,
        haptic_data->effect_id, haptic_data->state, haptic_data->activate_mode,
        haptic_data->duration);
    mutex_lock(&haptic_data->input_lock);

    if (haptic_data->current_mode == PLAY_MODE_RAM)
    {
        ics_info("%s: wait previous playing done if there is any!\n", __func__);
        while (count)
        {
            haptic_data->func->get_reg(haptic_data, RT6010_REG_PLAY_CTRL, &reg_val);
            if (reg_val == 0)
            {
                ics_info("%s: in stop mode!\n", __func__);
                break;
            }
            ics_info("%s: waiting for stop!\n", __func__);
            count--;
            usleep_range(2000, 2500);
        }
    }
    else
    {
        haptic_data->func->play_stop(haptic_data);
    }

    if (haptic_data->state)
    {
        if (haptic_data->activate_mode == PLAY_MODE_RAM)
        {
            ics_info("%s: PLAY_MODE_RAM\n", __func__);
            haptic_data->current_mode = PLAY_MODE_RAM;
            ics_input_effect_magnitude(haptic_data);
            ics_haptic_play_effect_seq(haptic_data);
        }
        else if (haptic_data->activate_mode == PLAY_MODE_RAM_LOOP)
        {
            ics_info("%s: PLAY_MODE_RAM_LOOP\n", __func__);
            haptic_data->current_mode = PLAY_MODE_RAM_LOOP;
            //haptic_data->level = haptic_data->chip_config.ram_loop_gain;
            ics_input_effect_magnitude(haptic_data);
            ics_haptic_play_effect_seq(haptic_data);
            hrtimer_start(&haptic_data->input_timer,
                      ktime_set(haptic_data->duration / 1000,
                        (haptic_data->duration % 1000) *
                        1000000), HRTIMER_MODE_REL);
        }
        else
        {
            /*other mode */
        }
    }
    mutex_unlock(&haptic_data->input_lock);
    ics_dbg("%s exit\n", __func__);
}
/*
static int32_t ics_pump_rb_data(struct ics_haptic_data *haptic_data)
{
    uint32_t buf_size_6k = haptic_data->chip_config.list_base_addr;
    uint8_t *buf_6k = NULL;
    uint8_t *buf_24k = NULL;
    int32_t data_size_6k, data_size_24k, i;

    ics_info("%s: pump rb data! is_custom_wave=%d, buf_size_6k = %u\n",
        __func__, haptic_data->is_custom_wave, buf_size_6k);
    if (haptic_data->is_custom_wave == 1)
    {
        buf_6k = kmalloc(buf_size_6k, GFP_KERNEL);
        if (buf_6k == NULL)
        {
            ics_err("%s: failed to allocate memory\n", __func__);
            return -ENOMEM;
        }

        buf_24k = kmalloc(buf_size_6k * 4, GFP_KERNEL);
        if (buf_24k == NULL)
        {
            kfree(buf_6k);
            ics_err("%s: failed to allocate memory\n", __func__);
            return -ENOMEM;
        }

        do
        {
            data_size_6k = kfifo_avail(&haptic_data->stream_fifo);
            data_size_6k = min(buf_size_6k, (uint32_t)data_size_6k);
            data_size_24k = read_rb(buf_24k,  data_size_6k * 2);
            ics_dbg("%s buf_size_6k=%d, data_size_6k=%d, data_size_24k=%d\n", __func__, buf_size_6k, data_size_6k, data_size_24k);
            data_size_24k = data_size_24k - data_size_24k % 4;
            // there might be 4 bytes alignment issue
            for (i = 0; i < data_size_24k / 4; i++)
            {
                buf_6k[i] = buf_24k[i * 4];
            }
            ics_dbg("%s pump rb data size = %d\n", __func__, data_size_24k / 4);
            kfifo_in(&haptic_data->stream_fifo, buf_6k, data_size_24k / 4);
        } while ((data_size_6k != 0) && (data_size_24k == data_size_6k * 2));

        kfree(buf_6k);
        kfree(buf_24k);
    }

    return 0;
}
*/
int32_t ics_input_irq_handler(void *data)
{
    //struct ics_haptic_data *haptic_data = (struct ics_haptic_data *)data;

    return 0;
}

/* return buffer size and availbe size */
static ssize_t ics_custom_wave_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    ssize_t len = 0;

    len +=
        snprintf(buf + len, PAGE_SIZE - len, "period_size=%d;",
        haptic_data->chip_config.list_base_addr);
    len +=
        snprintf(buf + len, PAGE_SIZE - len,
        "max_size=%d;free_size=%d;",
        MAX_STREAM_FIFO_SIZE, kfifo_avail(&haptic_data->stream_fifo));
    len +=
        snprintf(buf + len, PAGE_SIZE - len,
        "custom_wave_id=%d;", haptic_data->effect_max);
    return len;
}

static ssize_t ics_custom_wave_store(struct device *dev,
                     struct device_attribute *attr,
                     const char *buf, size_t count)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);
    uint32_t period_size, offset;
    int ret;
    uint8_t *buf_6k = NULL;
    uint8_t *buf_24k = NULL;
    int32_t data_size_6k, data_size_24k, fifo_in_size, i;

    period_size = (haptic_data->chip_config.list_base_addr);
    offset = 0;
    //ics_dbg("%s write size %zd, period size %lu", __func__, count, period_size);
    if (count % period_size || count < period_size)
    {
        atomic_set(&haptic_data->wave_data_done, 1);
        //rb_end();
    }
    if (haptic_data->is_custom_wave == 0)
    {
        ics_err("%s: not custom wave\n", __func__);
        return 0;
    }

    buf_6k = kmalloc(period_size, GFP_KERNEL);
    if (buf_6k == NULL)
    {
        ics_err("%s: failed to allocate memory\n", __func__);
        return -ENOMEM;
    }

    buf_24k = kmalloc(period_size * 4, GFP_KERNEL);
    if (buf_24k == NULL)
    {
        kfree(buf_6k);
        ics_err("%s: failed to allocate memory\n", __func__);
        return -ENOMEM;
    }

    atomic_set(&haptic_data->exit_in_rtp_loop, 0);

    while (count > 0)
    {
        if (atomic_read(&haptic_data->exit_write_loop) == 1)
        {
            ret = -1;
            goto wave_write_exit;
        }
        if (count < 4)
        {
            ret = count;
            goto wave_write_exit;
        }
        data_size_6k = kfifo_avail(&haptic_data->stream_fifo);
        data_size_6k = min(period_size, (uint32_t)data_size_6k);
        data_size_24k = min((int32_t)count, data_size_6k * 4);
        //ics_dbg("%s buf_size_6k=%d, data_size_6k=%d, data_size_24k=%d\n", __func__, period_size, data_size_6k, data_size_24k);
        fifo_in_size = (data_size_24k - data_size_24k % 4) / 4;
        if (fifo_in_size == 0)
        {
            ics_dbg("%s no space available\n", __func__);
            ret = wait_event_interruptible(haptic_data->kfifo_wait_q, atomic_read(&haptic_data->kfifo_available) == 1);
            if (ret == -ERESTARTSYS)
            {
                ics_err("%s wake up by signal return erro\n", __func__);
                goto wave_write_exit;
            }
            ics_dbg("%s buffer available, wake up\n", __func__);
            atomic_set(&haptic_data->kfifo_available, 0);
        }
        // there might be 4 bytes alignment issue
        for (i = 0; i < fifo_in_size; i++)
        {
            buf_6k[i] = buf[offset + i * 4];
        }
        //ics_dbg("%s write kfifo data size = %d\n", __func__, fifo_in_size);
        kfifo_in(&haptic_data->stream_fifo, buf_6k, fifo_in_size);

        count -= data_size_24k;
        offset += data_size_24k;
    }
    ret = offset;

wave_write_exit:
    wake_up_interruptible(&haptic_data->stop_wait_q);
    ics_dbg("%s wave store size %d", __func__, ret);
    kfree(buf_6k);
    kfree(buf_24k);
    return ret;
}

static ssize_t ics_f0_value_show(struct device *dev,
                    struct device_attribute *attr,
                    char *buf)
{
    struct ics_haptic_data *haptic_data = dev_get_drvdata(dev);

    ics_dbg("%s f0 = %d\n", __func__, haptic_data->chip_config.f0);
    return snprintf(buf, PAGE_SIZE, "%d\n", haptic_data->chip_config.f0);
}

static DEVICE_ATTR(f0_value, S_IRUGO, ics_f0_value_show, NULL);
static DEVICE_ATTR(custom_wave, S_IWUSR | S_IRUGO, ics_custom_wave_show, ics_custom_wave_store);
static struct attribute *ics_input_vibrator_attributes[] = {
    &dev_attr_f0_value.attr,
    &dev_attr_custom_wave.attr,
    NULL
};
struct attribute_group ics_input_vibrator_attribute_group = {
    .attrs = ics_input_vibrator_attributes
};

int32_t ics_input_dev_register(struct ics_haptic_data *haptic_data)
{
    int32_t ret = -1;
    struct input_dev *input_dev;
    struct ff_device *ff;
    int32_t effect_count_max;

    haptic_data->effects_count = XM_EFFECT_COUNT;
    haptic_data->effect_max = XM_EFFECT_MAX;

    ret = sysfs_create_group(&haptic_data->client->dev.kobj, &ics_input_vibrator_attribute_group);
    if (ret < 0)
    {
        ics_info("%s error creating sysfs attr files\n", __func__);
        return ret;
    }

    ics_info("%s: start register input dev\n", __func__);
    hrtimer_init(&haptic_data->input_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    haptic_data->input_timer.function = input_vibrator_timer_func;
    mutex_init(&haptic_data->input_lock);
    init_waitqueue_head(&haptic_data->wait_q);
    init_waitqueue_head(&haptic_data->stop_wait_q);
    init_waitqueue_head(&haptic_data->kfifo_wait_q);
    atomic_set(&haptic_data->is_in_rtp_loop, 0);
    atomic_set(&haptic_data->exit_in_rtp_loop, 0);
    atomic_set(&haptic_data->exit_write_loop, 0);
    atomic_set(&haptic_data->wave_data_done, 0);
    atomic_set(&haptic_data->kfifo_available, 0);
    INIT_WORK(&haptic_data->input_vibrator_work, ics_input_vibrator_work_routine);
    INIT_WORK(&haptic_data->input_rtp_work, ics_input_rtp_work_routine);
    INIT_WORK(&haptic_data->input_set_gain_work, ics_input_set_gain_work_routine);

    haptic_data->input_work_queue = create_singlethread_workqueue("ics_haptic_work_queue");
    if (!haptic_data->input_work_queue)
    {
        ics_err("%s: failed to create ics_haptic_work_queue\n", __func__);
        ret = -1;
        goto input_err;
    }

    device_init_wakeup(haptic_data->dev, true);
    input_dev = devm_input_allocate_device(haptic_data->dev);
    if (input_dev == NULL)
    {
        ret = -ENOMEM;
        goto input_err;
    }
    haptic_data->input_dev = input_dev;

    input_dev->name = "ics_haptic";
    input_set_drvdata(input_dev, haptic_data);
    input_set_capability(input_dev, EV_FF, FF_CONSTANT);
    input_set_capability(input_dev, EV_FF, FF_GAIN);

    ics_info("%s: effects info: effects_count=%d, effect_max=%d\n", __func__,
        haptic_data->effects_count, haptic_data->effect_max);

    if (haptic_data->effects_count != 0)
    {
        input_set_capability(input_dev, EV_FF, FF_PERIODIC);
        input_set_capability(input_dev, EV_FF, FF_CUSTOM);
    }
    if (haptic_data->effects_count + 1 > FF_EFFECT_COUNT_MAX)
    {
        effect_count_max = haptic_data->effects_count + 1;
    }
    else
    {
        effect_count_max = FF_EFFECT_COUNT_MAX;
    }
    ret = input_ff_create(input_dev, effect_count_max);
    if (ret < 0) 
    {
        ics_err("%s failed to create FF input device, ret=%d\n", __func__, ret);
        goto input_err;
    }
    ff = input_dev->ff;
    ff->upload = ics_haptic_upload_effect;
    ff->playback = ics_haptic_playback;
    ff->erase = ics_haptic_erase;
    ff->set_gain = ics_haptic_set_gain;
    ret = input_register_device(input_dev);
    if (ret < 0)
    {
        ics_err("%s failed to register input device, ret=%d\n", __func__, ret);
        goto input_err;
    }
/*
    ret =  create_rb();
    if (ret < 0)
    {
        ics_info("%s error creating ringbuffer\n", __func__);
        ics_input_dev_remove(haptic_data);
        return -1; 
    }
*/
    ics_info("%s: end register input dev\n", __func__);
    return 0;

input_err:
    ics_input_dev_remove(haptic_data);
    return ret;
}

int32_t ics_input_dev_remove(struct ics_haptic_data *haptic_data)
{
    if (haptic_data->input_work_queue != NULL)
    {
        flush_workqueue(haptic_data->input_work_queue);
        destroy_workqueue(haptic_data->input_work_queue);
    }
    cancel_work_sync(&haptic_data->input_vibrator_work);
    cancel_work_sync(&haptic_data->input_rtp_work);
    cancel_work_sync(&haptic_data->input_set_gain_work);
    mutex_destroy(&haptic_data->input_lock);
    if (haptic_data->input_timer.function != NULL)
    {
        hrtimer_cancel(&haptic_data->input_timer);
    }
    if (haptic_data->input_dev != NULL)
    {
        input_ff_destroy(haptic_data->input_dev);
    }
    device_init_wakeup(haptic_data->dev, false);

    //release_rb();

    return 0;
}
