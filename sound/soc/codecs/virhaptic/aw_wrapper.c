/*
 * aw_wrapper.c -- VirHaptic ALSA SoC audio driver
 *
 * Copyright 2024 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/pm_runtime.h>
#include <sound/core.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/gpio.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/of_irq.h>
#include <linux/completion.h>
#include <linux/err.h>
#include <linux/firmware.h>
#include <linux/timekeeping.h>
#include <linux/uaccess.h>
#include <asm/atomic.h>

#include <linux/workqueue.h>
#include "./include/aw_wrapper.h"

#include "../../../../../kernel_device_modules-6.6/drivers/input/misc/aw8693x_haptic/haptic_hv.h"
#include "../../../../../kernel_device_modules-6.6/drivers/input/misc/aw8693x_haptic/ringbuffer.h"

static atomic_t is_work_loop;
static struct snd_pcm_substream *g_substream;
static bool isfirst = false;

static struct work_struct  vir_work;
static struct workqueue_struct *vir_wq;


static void mtk_haptic_consume_handler(struct snd_pcm_substream *substream) {
	pr_err("%s  enter\n", __func__);
	snd_pcm_period_elapsed(substream);
}

static void virhaptic_work_routine(struct work_struct *work)
{
	while (atomic_read(&is_work_loop)) {
		pr_err("%s  sleep\n", __func__);
		wait_event_interruptible(vir_aw_haptic->wait_q,
				atomic_read(&vir_aw_haptic->is_consume_data) == 1);
		pr_err("%s  wakeup\n", __func__);
		mtk_haptic_consume_handler(g_substream);

		atomic_set(&vir_aw_haptic->is_consume_data, 0);
	}
}

int aw_haptic_open(struct snd_pcm_substream *substream) {
	pr_err("%s  enter\n", __func__);
    int ret = 0;
    // haptic add
	int16_t data[3] = {0, 0, 0};
	struct ff_effect effect;
    // add end

	// haptic add
	if (vir_aw_haptic == NULL) {
		pr_err("vir_haptic_open failed ! awinic is NULL \n");
		ret = -1;
	}
	if (vir_aw_haptic->input_dev == NULL) {
		pr_err("vir_haptic_open failed ! input_dev is NULL \n");
		ret = -1;
		return ret;
	}
	g_head = 0;
    data[0] = 197;
    effect.type = FF_PERIODIC;
    effect.u.periodic.waveform = FF_CUSTOM;
    effect.u.periodic.magnitude = 0x7fff;
    effect.u.periodic.custom_data = data;
    effect.u.periodic.custom_len = sizeof(int16_t) * 3;
	if(!vir_aw_haptic->ram_init) {
		pr_err("vir_aw_haptic ram init failed, not allow to play! !\n");
		return -ERANGE;
	}
    ret = aw_haptic_upload_effect_to_ext(vir_aw_haptic->input_dev, &effect, NULL);
	if(ret < 0) {
		pr_err("aw_haptic_upload_effect_to_ext failed !\n");
	}

	ret = input_playback(vir_aw_haptic->input_dev, 0, 1);
	if(ret < 0) {
		pr_err("aw_haptics_playback failed !\n");
	}
    // add end

	atomic_set(&is_work_loop, 1);
	if (isfirst) {
		queue_work(vir_wq, &vir_work);
	}
	isfirst = true;
	g_substream = substream;
	return ret;
}
EXPORT_SYMBOL_GPL(aw_haptic_open);


int aw_haptic_copy(void* buf,unsigned long bytes) {
    int ret = 0;
	pr_err("%s  enter, aw_haptic->ram.base_addr %u, bytes %lu\n", __func__, vir_aw_haptic->ram.base_addr, bytes);
    ret = aw_haptic_custom_wave_store_to_ext(vir_aw_haptic->vib_dev.dev, NULL, buf, bytes);
	if(ret < 0) {
        pr_err("aw_haptic_copy failed !\n");
    }
    return ret;
}
EXPORT_SYMBOL_GPL(aw_haptic_copy);

snd_pcm_sframes_t aw_haptic_pointer(struct snd_pcm_substream *substream) {

    pr_err("vir_haptic_pointer g_head:%d g_head:%ld\n",g_head,bytes_to_frames(substream->runtime, g_head));
	return bytes_to_frames(substream->runtime, g_head);
}
EXPORT_SYMBOL_GPL(aw_haptic_pointer);

int aw_haptic_close(void) {
    int ret = 0;
    atomic_set(&is_work_loop, 0);
	input_playback(vir_aw_haptic->input_dev, 0, 0);
    return ret;
}
EXPORT_SYMBOL_GPL(aw_haptic_close);

int aw_haptic_work_routine(void) {
    int ret = 0;
    atomic_set(&is_work_loop, 0);
	vir_wq = create_singlethread_workqueue("VirHaptic_work_queue");

	if (!vir_wq) {
		pr_err("%s: creating VirHaptic_work_queue\n",__func__);
	}

	INIT_WORK(&vir_work, virhaptic_work_routine);
    return ret;
}
EXPORT_SYMBOL_GPL(aw_haptic_work_routine);