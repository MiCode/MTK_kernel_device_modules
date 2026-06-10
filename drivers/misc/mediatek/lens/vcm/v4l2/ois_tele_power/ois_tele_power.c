// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#define DRIVER_NAME "ois_tele_power"

#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define OIS_TELE_POWER_NAME				"ois_tele_power"

#define OIS_TELE_POWER_CTRL_DELAY_US			5000

/* ois_tele_power device structure */
struct ois_tele_power_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
	struct pinctrl *vcamaf_pinctrl;
	struct pinctrl_state *vcamaf_on;
	struct pinctrl_state *vcamaf_off;
};


#define OIS_DATA_NUMBER 32
struct OisInfo {
	int32_t is_ois_supported;
	int32_t data_mode;  /* ON/OFF */
	int32_t samples;
	int32_t x_shifts[OIS_DATA_NUMBER];
	int32_t y_shifts[OIS_DATA_NUMBER];
	int64_t timestamps[OIS_DATA_NUMBER];
};

struct mtk_ois_pos_info {
	struct OisInfo *p_ois_info;
};

/* Control commnad */
#define VIDIOC_MTK_S_OIS_MODE _IOW('V', BASE_VIDIOC_PRIVATE + 2, int32_t)

#define VIDIOC_MTK_G_OIS_POS_INFO _IOWR('V', BASE_VIDIOC_PRIVATE + 3, struct mtk_ois_pos_info)


static inline struct ois_tele_power_device *to_ois_tele_power_ois(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct ois_tele_power_device, ctrls);
}

static inline struct ois_tele_power_device *sd_to_ois_tele_power_ois(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct ois_tele_power_device, sd);
}

static int ois_tele_power_release(struct ois_tele_power_device *ois_tele_power)
{
	return 0;
}

static int ois_tele_power_init(struct ois_tele_power_device *ois_tele_power)
{
	/* struct i2c_client *client = v4l2_get_subdevdata(&ois_tele_power->sd); */
	return 0;
}

/* Power handling */
static int ois_tele_power_off(struct ois_tele_power_device *ois_tele_power)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = ois_tele_power_release(ois_tele_power);
	if (ret)
		LOG_INF("ois_tele_power release failed!\n");

	ret = regulator_disable(ois_tele_power->vin);
	if (ret)
		return ret;

	ret = regulator_disable(ois_tele_power->vdd);
	if (ret)
		return ret;

	if (ois_tele_power->vcamaf_pinctrl && ois_tele_power->vcamaf_off)
		ret = pinctrl_select_state(ois_tele_power->vcamaf_pinctrl,
					ois_tele_power->vcamaf_off);

	return ret;
}

static int ois_tele_power_on(struct ois_tele_power_device *ois_tele_power)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = regulator_enable(ois_tele_power->vin);
	if (ret < 0)
		return ret;

	ret = regulator_enable(ois_tele_power->vdd);
	if (ret < 0)
		return ret;

	if (ois_tele_power->vcamaf_pinctrl && ois_tele_power->vcamaf_on)
		ret = pinctrl_select_state(ois_tele_power->vcamaf_pinctrl,
					ois_tele_power->vcamaf_on);

	if (ret < 0)
		return ret;

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	usleep_range(OIS_TELE_POWER_CTRL_DELAY_US, OIS_TELE_POWER_CTRL_DELAY_US + 100);

	ret = ois_tele_power_init(ois_tele_power);
	if (ret < 0)
		goto fail;

	return 0;

fail:
	regulator_disable(ois_tele_power->vin);
	regulator_disable(ois_tele_power->vdd);
	if (ois_tele_power->vcamaf_pinctrl && ois_tele_power->vcamaf_off) {
		pinctrl_select_state(ois_tele_power->vcamaf_pinctrl,
				ois_tele_power->vcamaf_off);
	}

	return ret;
}

static int ois_tele_power_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ois_tele_power_device *ois_tele_power = sd_to_ois_tele_power_ois(sd);

	LOG_INF("+\n");
	ois_tele_power_on(ois_tele_power);
	LOG_INF("-\n");

	return 0;
}

static int ois_tele_power_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ois_tele_power_device *ois_tele_power = sd_to_ois_tele_power_ois(sd);

	LOG_INF("+\n");
	ois_tele_power_off(ois_tele_power);
	LOG_INF("-\n");

	return 0;
}

static long ois_tele_power_ops_core_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct ois_tele_power_device *ois_tele_power = sd_to_ois_tele_power_ois(sd);

	switch (cmd) {

	case VIDIOC_MTK_S_OIS_MODE:
	{
		int *ois_mode = arg;

		if (*ois_mode) {
			LOG_INF("VIDIOC_MTK_S_OIS_MODE Enable\n");
			ret = regulator_enable(ois_tele_power->vin);
			if (ret < 0) {
				LOG_INF("regulator_enable failed!\n");
				return ret;
			}
		} else {
			LOG_INF("VIDIOC_MTK_S_OIS_MODE Disable\n");
			ret = regulator_disable(ois_tele_power->vin);
			if (ret) {
				LOG_INF("regulator_disable failed!\n");
				return ret;
			}
		}
	}
	break;

	case VIDIOC_MTK_G_OIS_POS_INFO:
	{
		struct mtk_ois_pos_info *info = arg;
		struct OisInfo pos_info;

		memset(&pos_info, 0, sizeof(struct OisInfo));

		/* To Do */

		if (copy_to_user((void *)info->p_ois_info, &pos_info, sizeof(pos_info)))
			ret = -EFAULT;
	}
	break;

	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

static const struct v4l2_subdev_internal_ops ois_tele_power_int_ops = {
	.open = ois_tele_power_open,
	.close = ois_tele_power_close,
};

static struct v4l2_subdev_core_ops ois_tele_power_ops_core = {
	.ioctl = ois_tele_power_ops_core_ioctl,
};

static const struct v4l2_subdev_ops ois_tele_power_ops = {
	.core = &ois_tele_power_ops_core,
};

static void ois_tele_power_subdev_cleanup(struct ois_tele_power_device *ois_tele_power)
{
	v4l2_async_unregister_subdev(&ois_tele_power->sd);
	v4l2_ctrl_handler_free(&ois_tele_power->ctrls);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&ois_tele_power->sd.entity);
#endif
}

static int ois_tele_power_init_controls(struct ois_tele_power_device *ois_tele_power)
{
	struct v4l2_ctrl_handler *hdl = &ois_tele_power->ctrls;

	v4l2_ctrl_handler_init(hdl, 1);

	if (hdl->error)
		return hdl->error;

	ois_tele_power->sd.ctrl_handler = hdl;

	return 0;
}

static int ois_tele_power_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct ois_tele_power_device *ois_tele_power;
	int ret;

	LOG_INF("%s\n", __func__);

	ois_tele_power = devm_kzalloc(dev, sizeof(*ois_tele_power), GFP_KERNEL);
	if (!ois_tele_power)
		return -ENOMEM;

	ois_tele_power->vin = devm_regulator_get(dev, "vin");
	if (IS_ERR(ois_tele_power->vin)) {
		ret = PTR_ERR(ois_tele_power->vin);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vin regulator\n");
		return ret;
	}

	ret = regulator_set_voltage(ois_tele_power->vin, 3300000, 3300000);	//set 3.3v voltage
	if (ret < 0)
		return ret;

	ois_tele_power->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(ois_tele_power->vdd)) {
		ret = PTR_ERR(ois_tele_power->vdd);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vdd regulator\n");
		return ret;
	}

	ois_tele_power->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(ois_tele_power->vcamaf_pinctrl)) {
		ret = PTR_ERR(ois_tele_power->vcamaf_pinctrl);
		ois_tele_power->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		ois_tele_power->vcamaf_on = pinctrl_lookup_state(
			ois_tele_power->vcamaf_pinctrl, "vcamaf_on");

		if (IS_ERR(ois_tele_power->vcamaf_on)) {
			ret = PTR_ERR(ois_tele_power->vcamaf_on);
			ois_tele_power->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}

		ois_tele_power->vcamaf_off = pinctrl_lookup_state(
			ois_tele_power->vcamaf_pinctrl, "vcamaf_off");

		if (IS_ERR(ois_tele_power->vcamaf_off)) {
			ret = PTR_ERR(ois_tele_power->vcamaf_off);
			ois_tele_power->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}

	v4l2_i2c_subdev_init(&ois_tele_power->sd, client, &ois_tele_power_ops);
	ois_tele_power->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ois_tele_power->sd.internal_ops = &ois_tele_power_int_ops;

	ret = ois_tele_power_init_controls(ois_tele_power);
	if (ret)
		goto err_cleanup;

#if defined(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&ois_tele_power->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	ois_tele_power->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&ois_tele_power->sd);
	if (ret < 0)
		goto err_cleanup;

	return 0;

err_cleanup:
	ois_tele_power_subdev_cleanup(ois_tele_power);
	return ret;
}

static void ois_tele_power_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ois_tele_power_device *ois_tele_power = sd_to_ois_tele_power_ois(sd);

	LOG_INF("%s\n", __func__);

	ois_tele_power_subdev_cleanup(ois_tele_power);
}

static const struct i2c_device_id ois_tele_power_id_table[] = {
	{ OIS_TELE_POWER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, ois_tele_power_id_table);

static const struct of_device_id ois_tele_power_of_table[] = {
	{ .compatible = "mediatek,ois_tele_power" },
	{ },
};
MODULE_DEVICE_TABLE(of, ois_tele_power_of_table);

static struct i2c_driver ois_tele_power_i2c_driver = {
	.driver = {
		.name = OIS_TELE_POWER_NAME,
		.of_match_table = ois_tele_power_of_table,
	},
	.probe  = ois_tele_power_probe,
	.remove = ois_tele_power_remove,
	.id_table = ois_tele_power_id_table,
};

module_i2c_driver(ois_tele_power_i2c_driver);

MODULE_AUTHOR("Sam Hung <Sam.Hung@mediatek.com>");
MODULE_DESCRIPTION("OIS_TELE_POWER VCM driver");
MODULE_LICENSE("GPL v2");
