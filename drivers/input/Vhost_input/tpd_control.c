// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include "tpd.h"
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/input/mt.h>
#include <linux/workqueue.h>

#include "tpd_hyp_virtio.h"

#if IS_ENABLED(CONFIG_GRT_VM)
#include "mtk_virt_output.h"
#endif

static struct input_dev *touch_input_dev_mipi;
static struct input_dev *touch_input_dev_DSI_0;
static struct input_dev *touch_input_dev_DP_1;
static struct input_dev *touch_input_dev_DSI_1_1;

#define touch_input_dev_mipi_port	0
#define touch_input_dev_DSI_0_port	1
#define touch_input_dev_DP_1_port	3
#define touch_input_dev_DSI_1_1_port	4

static int mipi_release_cnt = 1;
static int mipi_release_cnt_1 = 1;

struct work_data {
struct work_struct work;
struct input_dev *dev;
};


void set_input_dev_pro_mipi(struct input_dev *dev, int Max_X, int Max_Y, int touch_MAJOR, int touch_tracking_ID)
{
	set_bit(EV_ABS, dev->evbit);
	set_bit(EV_KEY, dev->evbit);
	set_bit(ABS_PRESSURE, dev->absbit);
	set_bit(ABS_X, dev->absbit);
	set_bit(ABS_Y, dev->absbit);
	set_bit(BTN_TOUCH, dev->keybit);

	set_bit(INPUT_PROP_DIRECT, dev->propbit);
	set_bit(ABS_MT_POSITION_X, dev->absbit);
	set_bit(ABS_MT_POSITION_Y, dev->absbit);
	set_bit(ABS_MT_TRACKING_ID, dev->absbit);
	set_bit(ABS_MT_TOUCH_MAJOR, dev->absbit);
	set_bit(ABS_MT_TOUCH_MINOR,dev->absbit);
	set_bit(ABS_MT_POSITION_X,dev->absbit);
	set_bit(ABS_MT_POSITION_Y, dev->absbit);
	input_set_abs_params(dev, ABS_MT_POSITION_X, 0, Max_X, 0, 0);
	input_set_abs_params(dev, ABS_MT_POSITION_Y, 0, Max_Y, 0, 0);
	input_set_abs_params(dev, ABS_MT_TOUCH_MAJOR, 0, touch_MAJOR, 0, 0);
	input_set_abs_params(dev, ABS_MT_TRACKING_ID, 0, touch_tracking_ID, 0, 0);

}

void set_input_dev_pro_hy(struct input_dev *dev, int Max_X, int Max_Y, int touch_MAJOR, int touch_tracking_ID)
{
	set_bit(ABS_MT_TRACKING_ID, dev->absbit);
	set_bit(ABS_MT_TOUCH_MAJOR, dev->absbit);
	set_bit(ABS_MT_TOUCH_MINOR,dev->absbit);
	set_bit(ABS_MT_POSITION_X,dev->absbit);
	set_bit(ABS_MT_POSITION_Y, dev->absbit);
	input_set_abs_params(dev, ABS_MT_POSITION_X, 0, Max_X, 0, 0);
	input_set_abs_params(dev, ABS_MT_POSITION_Y, 0, Max_Y, 0, 0);
	input_set_abs_params(dev, ABS_MT_TOUCH_MAJOR, 0, touch_MAJOR, 0, 0);
	input_set_abs_params(dev, ABS_MT_TRACKING_ID, 0, touch_tracking_ID, 0, 0);
	input_mt_init_slots(dev, 16, INPUT_MT_DIRECT);

}

int touch_input_init(void)
{
	int result;
	uint32_t width = 800;
	uint32_t height = 1200;
	/* mipi vir touch */
	touch_input_dev_mipi = input_allocate_device();
	if (!touch_input_dev_mipi) {
		pr_info("Failed to allocate touch input device.\n");
		return -ENOMEM;
	}
	touch_input_dev_mipi->name = "mtk-tpd";
	touch_input_dev_mipi->phys = "mipi_touch/input0";
#if IS_ENABLED(CONFIG_GRT_VM)
	mtk_virt_get_panel_size(MTK_DISP_DSI0, &width, &height);
#endif
	set_input_dev_pro_mipi(touch_input_dev_mipi,width,height,255,3);
	result = input_register_device(touch_input_dev_mipi);
	if (result) {
		pr_info("Failed to register touch input device.\n");
		input_free_device(touch_input_dev_mipi);
		return result;
	}
	/* DSI_0 vir touch */
	touch_input_dev_DSI_0 = input_allocate_device();
	if (!touch_input_dev_DSI_0) {
		pr_info("Failed to allocate touch input device.\n");
		return -ENOMEM;
	}
	touch_input_dev_DSI_0->name = "DSI_0_touch_vir";
	touch_input_dev_DSI_0->phys = "DSI_0_touch/input0";
	width = 1920;
	height = 1080;
#if IS_ENABLED(CONFIG_GRT_VM)
	mtk_virt_get_panel_size(MTK_DISP_DSI0, &width, &height);
#endif
	set_input_dev_pro_hy(touch_input_dev_DSI_0, width,height,255,16);
	result = input_register_device(touch_input_dev_DSI_0);
	if (result) {
		pr_info("Failed to register touch input device.\n");
		input_free_device(touch_input_dev_DSI_0);
		return result;
	}

	/* DP_1 vir touch */
	touch_input_dev_DP_1 = input_allocate_device();
	if (!touch_input_dev_DP_1) {
		pr_info("Failed to allocate touch input device.\n");
		return -ENOMEM;
	}
	touch_input_dev_DP_1->name = "DP_1_touch_vir";
	touch_input_dev_DP_1->phys = "DP_1_touch/input0";
#if IS_ENABLED(CONFIG_GRT_VM)
	mtk_virt_get_panel_size(MTK_DISP_DP0, &width, &height);
#endif
	set_input_dev_pro_hy(touch_input_dev_DP_1, width,height,255,16);
	result = input_register_device(touch_input_dev_DP_1);
	if (result) {
		pr_info("Failed to register touch input device.\n");
		input_free_device(touch_input_dev_DP_1);
		return result;
	}
	/* DSI_1_1 vir touch */
	touch_input_dev_DSI_1_1 = input_allocate_device();
	if (!touch_input_dev_DSI_1_1) {
		pr_info("Failed to allocate touch input device.\n");
		return -ENOMEM;
	}
	touch_input_dev_DSI_1_1->name = "DSI1_1_touch_vir";
	touch_input_dev_DSI_1_1->phys = "DSI1_1_touch/input0";
#if IS_ENABLED(CONFIG_GRT_VM)
	mtk_virt_get_panel_size(MTK_DISP_DSI1_1, &width, &height);
#endif
	set_input_dev_pro_hy(touch_input_dev_DSI_1_1, width, height,255,16);
	result = input_register_device(touch_input_dev_DSI_1_1);
	if (result) {
		pr_info("Failed to register touch input device.\n");
		input_free_device(touch_input_dev_DSI_1_1);
		return result;
	}
	return 0;
}


void report_touch_down_mipi(struct input_dev *dev, int X, int Y, int H, int id, int W)
{
	if(dev ==NULL)
		return;
	if (W!= 66 && W!= 67){
		input_report_key(dev, BTN_TOUCH, 1);
		input_event(dev, EV_ABS, ABS_MT_POSITION_X, Y);
		input_event(dev, EV_ABS, ABS_MT_POSITION_Y, X);
		input_event(dev, EV_ABS, ABS_MT_TOUCH_MAJOR, H);
		//input_event(touch_input_dev, EV_ABS, ABS_MT_WIDTH_MAJOR, W);
		input_event(dev, EV_ABS, ABS_MT_TRACKING_ID, id);
		input_mt_sync(dev);
		mipi_release_cnt = 1;
		mipi_release_cnt_1 = 1;
	} else if(W == 67)
		pr_info("send Null data");
	else
		input_sync(dev);
}

void report_touch_release(struct input_dev *dev, int un_relase_flag )
{
	if(dev ==NULL)
		return;
	if(mipi_release_cnt_1 == 1){
		input_report_key(dev, BTN_TOUCH, 0);
		input_mt_sync(dev);
		input_sync(dev);
		input_sync(dev);
		mipi_release_cnt_1++;
	}
	if(un_relase_flag == 10){
		if (mipi_release_cnt < 2){
			input_sync(dev);
			input_sync(dev);
			mipi_release_cnt++;
		}
	}
	if(un_relase_flag == 67)
		pr_info("send Null data");
}

void report_touch_release_hy(struct input_dev *dev)
{

}

void report_touch_down_hy_dsi_0(struct input_dev *dev, int flag, int X, int Y, int H, int id, int W)
{
	if(dev == NULL) {
		pr_info("dev is NULL\n");
		return;
	}

	if(flag == 10)
		input_mt_slot(dev,id);
	if(flag == 11)
		input_mt_report_slot_state(dev,MT_TOOL_FINGER, true);
	if(flag == 13)
		input_mt_report_slot_state(dev,MT_TOOL_FINGER, false);
	if(flag == 14) {
		input_mt_sync_frame(dev);
		input_sync(dev);
	}
	if(flag == 12) {
		input_event(dev, EV_ABS, ABS_MT_POSITION_X, X);
		input_event(dev, EV_ABS, ABS_MT_POSITION_Y, Y);
		input_event(dev, EV_ABS, ABS_MT_TOUCH_MAJOR, W);
		input_event(dev, EV_ABS, ABS_MT_TRACKING_ID, id);
	}
}

void unregister_device_work(struct work_struct *work)
{

	struct work_data *data = container_of(work, struct work_data, work);

	input_unregister_device(data->dev);
	kfree(data);
}

void __maybe_unused touch_handle_resoltion_changed(struct input_dev *dev,int Xmax, int Ymax)
{
	struct work_data *data;

	data = kmalloc(sizeof(struct work_data), GFP_ATOMIC);
	if (!data)
		return;
	INIT_WORK(&data->work, unregister_device_work);
	data->dev = dev;
	schedule_work(&data->work);
}

int touch_virtul(int flag,int X, int Y, int H, int W, int id, int touch_type)
{
	int Touch_type = touch_type;

	if(Touch_type == touch_input_dev_mipi_port){
		report_touch_down_mipi(touch_input_dev_mipi,X, Y,  H, id, W);
		return 0;
	}else if(Touch_type == touch_input_dev_DSI_0_port){
		report_touch_down_hy_dsi_0(touch_input_dev_DSI_0,flag, X, Y, H, id,W);
		return 0;
	}else if(Touch_type == touch_input_dev_DP_1_port){
		report_touch_down_hy_dsi_0(touch_input_dev_DP_1,flag, X, Y, H, id,W);
		return 0;
	}else if(Touch_type == touch_input_dev_DSI_1_1_port){
		report_touch_down_hy_dsi_0(touch_input_dev_DSI_1_1,flag, X, Y, H, id,W);
		return 0;
	}else if(Touch_type == 10){
		//touch_handle_resoltion_changed(touch_input_dev_mipi,X, Y);
		input_abs_set_res(touch_input_dev_mipi, ABS_MT_POSITION_X, 0);
		input_abs_set_res(touch_input_dev_mipi, ABS_MT_POSITION_Y, 0);
		input_set_abs_params(touch_input_dev_mipi, ABS_MT_POSITION_X, 0, X, 0, 0);
		input_set_abs_params(touch_input_dev_mipi, ABS_MT_POSITION_Y, 0, Y, 0, 0);
		input_sync(touch_input_dev_mipi);
		return 0;
	}else{
		return 0;
	}
}

int touch_virtul_release( int touch_type,int un_relase_flag)
{

	int Touch_type = touch_type;

	if(Touch_type == touch_input_dev_mipi_port){
		report_touch_release(touch_input_dev_mipi,un_relase_flag);
		return 0;
	}else if(Touch_type == touch_input_dev_DSI_0_port){
		report_touch_release_hy(touch_input_dev_DSI_0);
		return 0;
	}else if(Touch_type == touch_input_dev_DP_1_port){
		report_touch_release_hy(touch_input_dev_DP_1);
		return 0;
	}else if(Touch_type == touch_input_dev_DSI_1_1_port){
		report_touch_release_hy(touch_input_dev_DSI_1_1);
		return 0;
	}else{
		return 0;
	}
}

static int __init tpd_device_init(void)
{
	int res = 0;
	/* load touch driver first  */
	res = virtio_tpd_init();
	if (res)
		pr_info("touch virtio init failed.\n");
	touch_input_init();
	if (!res)
		pr_info("tpd : touch device init failed res:%d\n", res);
	return 0;
}


late_initcall(tpd_device_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek touch panel driver");
MODULE_AUTHOR("Pavel<Pavel@mediatek.com>");
