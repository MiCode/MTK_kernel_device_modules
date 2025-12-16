// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author Terry Chang <terry.chang@mediatek.com>
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/slab.h>

#include <linux/workqueue.h>
#include "../Vhost_input_YO/touch_hypervisor.h"
#include <linux/completion.h>

#define MAX_SLOTS 10

#include "input_handler.h"

struct touch_event_work {
struct work_struct work;
char str[25];
};

static struct workqueue_struct *touch_event_wq;
static struct workqueue_struct *touch_event_wq2;
static DECLARE_COMPLETION(touch_event_done);

static struct input_dev *my_virtual_dev;
static struct input_handle *gt9xx_handle;
static int current_slot;

#define Touch_IVC_SLOT	10
#define Touch_IVC_SLOT_lOCATION	11
#define Touch_IVC_SLOT_DOWN_X	12
#define Touch_IVC_SLOT_DOWN_Y	121
#define Touch_IVC_SLOT_DOWN_W	122

#define Touch_IVC_SLOT_UP	13
#define Touch_IVC_SLOT_SYNC	14
#define Touch_IVC_BUFFER	32

#define KEYS_ACTION	128
#define KEYS_SYNC	129

#define TYPE_KEY    16

struct input_dev_info {
	char *name;
};

struct input_dev_info keys_info[] = {
	{ .name = "mtk-pmic-keys_listener" },
	{ .name = "mtk-kpd_listener" },
	{ .name = "gpio-keys_listener" },
	{ },
};

static void touch_event_work_func(struct work_struct *work)
{
	struct touch_event_work *touch_work = container_of(work, struct touch_event_work, work);

	touch_event(touch_work->str);
	kfree(touch_work);
}

static void touch_event_work_func_release(struct work_struct *work)
{
	struct touch_event_work *touch_work = container_of(work, struct touch_event_work, work);

	touch_event(touch_work->str);
	complete(&touch_event_done);
	kfree(touch_work);

}


// 创建虚拟input device
static int create_virtual_input_device(void)
{
	int err;

	my_virtual_dev = input_allocate_device();
	if (!my_virtual_dev)
		return -ENOMEM;

	my_virtual_dev->name = "my_virtual_touch";
	my_virtual_dev->phys = "my_virtual_touch/input0";

	// 支持的事件类型和码
	__set_bit(EV_ABS, my_virtual_dev->evbit);
	__set_bit(EV_SYN, my_virtual_dev->evbit);
	__set_bit(EV_KEY, my_virtual_dev->evbit);

	__set_bit(ABS_MT_SLOT, my_virtual_dev->absbit);
	__set_bit(ABS_MT_TRACKING_ID, my_virtual_dev->absbit);
	__set_bit(ABS_MT_POSITION_X, my_virtual_dev->absbit);
	__set_bit(ABS_MT_POSITION_Y, my_virtual_dev->absbit);

	__set_bit(ABS_MT_TOUCH_MAJOR, my_virtual_dev->absbit);
	__set_bit(ABS_MT_TOUCH_MINOR, my_virtual_dev->absbit);
	__set_bit(ABS_MT_WIDTH_MAJOR, my_virtual_dev->absbit);
	__set_bit(ABS_MT_WIDTH_MINOR, my_virtual_dev->absbit);

	__set_bit(BTN_TOUCH, my_virtual_dev->keybit);
	__set_bit(KEY_VOLUMEDOWN, my_virtual_dev->keybit);
	__set_bit(KEY_VOLUMEUP, my_virtual_dev->keybit);
	__set_bit(KEY_POWER, my_virtual_dev->keybit);

	// 设置坐标范围
	input_set_abs_params(my_virtual_dev, ABS_MT_SLOT, 0, MAX_SLOTS-1, 0, 0);
	input_set_abs_params(my_virtual_dev, ABS_MT_TRACKING_ID, 0, 65535, 0, 0);
	input_set_abs_params(my_virtual_dev, ABS_MT_POSITION_X, 0, 1920, 0, 0);
	input_set_abs_params(my_virtual_dev, ABS_MT_POSITION_Y, 0, 1080, 0, 0);
	input_set_abs_params(my_virtual_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);

	err = input_register_device(my_virtual_dev);
	if (err) {
		input_free_device(my_virtual_dev);
		return err;
	}
	return 0;
}

static void send_touch_event(int gt_flag, int gt_x, int gt_y, int gt_h, int gt_w, int gt_id, int touch_type)
{
	struct touch_event_work *work;

	work = kmalloc(sizeof(struct touch_event_work), GFP_KERNEL);
	if (!work)
		return; // handle memory failure

	INIT_WORK(&work->work, touch_event_work_func);

	sprintf(work->str, "%d %d %d %d %d %d %d ", gt_flag, gt_x, gt_y, gt_h, gt_w, gt_id, touch_type);
	queue_work(touch_event_wq, &work->work);
}

static void keys_input_event(struct input_handle *handle, unsigned int type, unsigned int code, int value)
{

	if (type == EV_SYN && code == SYN_REPORT) {
		send_touch_event(KEYS_SYNC, code, value, 0, 0, 0, TYPE_KEY);
	} else if (type == EV_KEY) {
		switch (code) {
		case KEY_VOLUMEUP:
			send_touch_event(KEYS_ACTION, code, value, 0, 0, 0, TYPE_KEY);
			pr_info("%s: call ACTION send_touch_event, code %d, value %d\n", __func__, code, value);
			break;
		case KEY_VOLUMEDOWN:
			send_touch_event(KEYS_ACTION, code, value, 0, 0, 0, TYPE_KEY);
			pr_info("%s: call ACTION send_touch_event, code %d, value %d\n", __func__, code, value);
			break;
		case KEY_POWER:
			send_touch_event(KEYS_ACTION, code, value, 0, 0, 0, TYPE_KEY);
			pr_info("%s: call ACTION send_touch_event, code %d, value %d\n", __func__, code, value);
			break;
		default:
			pr_info("%s: ERROR call send_touch_event, code %d, value %d\n", __func__, code, value);
			break;
		}
	}
}
// 事件转发和case处理
static void my_input_event(struct input_handle *handle, unsigned int type, unsigned int code, int value)
{
	char evnt_str[Touch_IVC_BUFFER];
	int gt_flag = 0, gt_x = 0, gt_y = 0, gt_h = 0, gt_w = 0, gt_id = 0, touch_type = 1;


	if (type == EV_ABS) {
		switch (code) {
		case ABS_MT_SLOT:
			current_slot = value;
			//gt_flag = Touch_IVC_SLOT;
			//input_mt_slot(ts->input_dev, points->id);
			gt_id = current_slot;
			gt_flag = Touch_IVC_SLOT;
			send_touch_event(gt_flag, gt_x, gt_y, gt_h, gt_w, gt_id, touch_type);
			break;
		case ABS_MT_TRACKING_ID:
			if (value >= 0){
				// Touch_IVC_SLOT_lOCATION
				//input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
				gt_flag = Touch_IVC_SLOT_lOCATION;
				gt_id = value;
				send_touch_event(gt_flag, gt_x, gt_y, gt_h, gt_w, gt_id, touch_type);
			} else {
				//gt_flag = Touch_IVC_SLOT_UP;
				//input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
				gt_flag = Touch_IVC_SLOT_UP;
				send_touch_event(gt_flag, gt_x, gt_y, gt_h, gt_w, gt_id, touch_type);
			}
			break;
		default:
			if(code == 53){
				gt_flag = Touch_IVC_SLOT_DOWN_X;
				gt_x = value;
				send_touch_event(gt_flag, gt_x, gt_y, gt_h, gt_w, gt_id, touch_type);
			}
			if(code == 54){
				gt_flag = Touch_IVC_SLOT_DOWN_Y;
				gt_y = value;
				send_touch_event(gt_flag, gt_x, gt_y, gt_h, gt_w, gt_id, touch_type);
			}
			if(code == 48){
				gt_w = value;
				gt_flag = Touch_IVC_SLOT_DOWN_W;
				send_touch_event(gt_flag, gt_x, gt_y, gt_h, gt_w, gt_id, touch_type);
			}
			break;
		}
	} else if (type == EV_SYN && code == SYN_REPORT) {
		//gt_flag = Touch_IVC_SLOT_SYNC;
		//input_mt_sync_frame(ts->input_dev);
		//input_sync(ts->input_dev);
		gt_flag = Touch_IVC_SLOT_SYNC;
		send_touch_event(gt_flag, gt_x, gt_y, gt_h, gt_w, gt_id, touch_type);
	}
}

static int keys_input_connect(struct input_handler *handler, struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	struct input_dev_info *info = id->driver_info;


	pr_info("%s: %s[%s] connect keys_inputs!\n", __func__, dev->name, info->name);

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = info->name;

	if (input_register_handle(handle)) {
		kfree(handle);
		return -1;
	}
	if (input_open_device(handle)) {
		input_unregister_handle(handle);
		kfree(handle);
		return -1;
	}

	pr_info("%s: connected to %s\n", handle->name, dev->name);
	return 0;
}

static int my_input_connect(struct input_handler *handler, struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;

	if (strcmp(dev->name, "gt9xx_DSI-1") != 0)
		return -ENODEV;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "my_gt9xx_DSI-1_listener";

	if (input_register_handle(handle)) {
		kfree(handle);
		return -1;
	}
	if (input_open_device(handle)) {
		input_unregister_handle(handle);
		kfree(handle);
		return -1;
	}

	gt9xx_handle = handle;
	pr_info("my_gt9xx_DSI-1_listener: connected to %s\n", dev->name);
	return 0;
}

static int common_init_touch(void)
{
	touch_event_wq = alloc_workqueue("touch_event_wq", WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	touch_event_wq2 = alloc_workqueue("touch_event_wq2", WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!touch_event_wq || !touch_event_wq2) {
		pr_info("Failed to create touch_event workqueue\n");
		return -ENOMEM;
	}
	return 0;
}

static void keys_input_disconnect(struct input_handle *handle)
{
	pr_info("%s: %s disconnect keys_inputs!\n", __func__, handle->name);
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static void my_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
	gt9xx_handle = NULL;
}

static const struct input_device_id my_ids[] = {
	{ .driver_info = 1 },
	{ },
};

static const struct input_device_id keys_ids[] = {
	{
	.flags = INPUT_DEVICE_ID_MATCH_VENDOR |
		INPUT_DEVICE_ID_MATCH_PRODUCT |
		INPUT_DEVICE_ID_MATCH_VERSION |
		INPUT_DEVICE_ID_MATCH_KEYBIT,
	.vendor = 0x0001,
	.product = 0x0001,
	.version = 0x0001,
	.evbit = { BIT_MASK(EV_KEY) },
	.driver_info = (kernel_ulong_t)&keys_info[0]
	},
	{
	.flags = INPUT_DEVICE_ID_MATCH_VENDOR |
		INPUT_DEVICE_ID_MATCH_PRODUCT |
		INPUT_DEVICE_ID_MATCH_VERSION |
		INPUT_DEVICE_ID_MATCH_KEYBIT,
	.vendor = 0x0001,
	.product = 0x0002,
	.version = 0x0001,
	.evbit = { BIT_MASK(EV_KEY) },
	.driver_info = (kernel_ulong_t)&keys_info[1]
	},
	{
	.flags = INPUT_DEVICE_ID_MATCH_VENDOR |
		INPUT_DEVICE_ID_MATCH_PRODUCT |
		INPUT_DEVICE_ID_MATCH_VERSION |
		INPUT_DEVICE_ID_MATCH_KEYBIT,
	.vendor = 0x0001,
	.product = 0x0001,
	.version = 0x0100,
	.evbit = { BIT_MASK(EV_KEY) },
	.driver_info = (kernel_ulong_t)&keys_info[2]
	},
	{ },
};

static struct input_handler my_input_handler[] = {
	{
	.event = my_input_event,
	.connect = my_input_connect,
	.disconnect = my_input_disconnect,
	.name = "my_gt9xx_DSI-1_handler",
	.id_table = my_ids,
	},
	{
	.event = keys_input_event,
	.connect = keys_input_connect,
	.disconnect = keys_input_disconnect,
	.name = "keys_handler",
	.id_table = keys_ids,
	},
	{},
};

static int __init my_init(void)
{
	int err;

	current_slot = 0;
	err = create_virtual_input_device();
	if (err) {
		pr_info("Failed to create virtual input device\n");
		return err;
	}

	common_init_touch();

	input_register_handler(&my_input_handler[0]);
	input_register_handler(&my_input_handler[1]);

	return input_register_handler(&my_input_handler);
}

static void __exit my_exit(void)
{
	input_unregister_handler(&my_input_handler);
	if (my_virtual_dev) {
		input_unregister_device(my_virtual_dev);
		my_virtual_dev = NULL;
	}
}

module_init(my_init);
module_exit(my_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pavel.Xu");
MODULE_DESCRIPTION("Optimized input event listener");
