// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/pm_runtime.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>

#include "mtk_cam.h"
#include "mtk_cam-feature.h"
#include "mtk_cam-raw_pipeline.h"

static unsigned int debug_raw;
module_param(debug_raw, uint, 0644);
MODULE_PARM_DESC(debug_raw, "activates debug info");

static int debug_raw_num = -1;
module_param(debug_raw_num, int, 0644);
MODULE_PARM_DESC(debug_raw_num, "debug: num of used raw devices");

static int debug_pixel_mode = -1;
module_param(debug_pixel_mode, int, 0644);
MODULE_PARM_DESC(debug_pixel_mode, "debug: pixel mode");

static int debug_clk_idx = -1;
module_param(debug_clk_idx, int, 0644);
MODULE_PARM_DESC(debug_clk_idx, "debug: clk idx");

#define MTK_CAMSYS_RES_IDXMASK		0xF0
#define MTK_CAMSYS_RES_BIN_TAG		0x10
#define MTK_CAMSYS_RES_FRZ_TAG		0x20
#define MTK_CAMSYS_RES_HWN_TAG		0x30
#define MTK_CAMSYS_RES_CLK_TAG		0x40

#define MTK_CAMSYS_RES_PLAN_NUM		10
#define TGO_MAX_PXLMODE		8
#define FRZ_PXLMODE_THRES		71
#define MHz		1000000
#define MTK_CAMSYS_PROC_DEFAULT_PIXELMODE	2
#define DC_DEFAULT_CAMSV_PIXELMODE			8

#define DC_SUPPORT_RAW_FEATURE_MASK	\
	(STAGGER_2_EXPOSURE_LE_SE | STAGGER_2_EXPOSURE_SE_LE)

#define sizeof_u32(__struct__) ((sizeof(__struct__) + sizeof(u32) - 1)/ \
				sizeof(u32))

enum MTK_CAMSYS_RES_STEP {
	E_RES_BASIC,
	E_RES_BIN_S = MTK_CAMSYS_RES_BIN_TAG,
	E_RES_BIN0 = E_RES_BIN_S,
	E_RES_BIN1,
	E_RES_BIN_E,
	E_RES_FRZ_S = MTK_CAMSYS_RES_FRZ_TAG,
	E_RES_FRZ0 = E_RES_FRZ_S,
	E_RES_FRZ1,
	E_RES_FRZ_E,
	E_RES_HWN_S = MTK_CAMSYS_RES_HWN_TAG,
	E_RES_HWN0 = E_RES_HWN_S,
	E_RES_HWN1,
	E_RES_HWN2,
	E_RES_HWN_E,
	E_RES_CLK_S = MTK_CAMSYS_RES_CLK_TAG,
	E_RES_CLK0 = E_RES_CLK_S,
	E_RES_CLK1,
	E_RES_CLK2,
	E_RES_CLK3,
	E_RES_CLK_E,
};

enum MTK_CAMSYS_MAXLB_CHECK_RESULT {
	LB_CHECK_OK = 0,
	LB_CHECK_CBN,
	LB_CHECK_QBN,
	LB_CHECK_BIN,
	LB_CHECK_FRZ,
	LB_CHECK_TWIN,
	LB_CHECK_RAW,
};

#define CAM_RAW_PROCESS_MAX_LINE_BUFFER		(6632)
#define CAM_RAW_FRZ_MAX_LINE_BUFFER		(6632)
#define CAM_RAW_BIN_MAX_LINE_BUFFER		(12000)
#define CAM_RAW_QBND_MAX_LINE_BUFFER		(16000)
#define CAM_RAW_CBN_MAX_LINE_BUFFER		(18472)
#define CAM_TWIN_PROCESS_MAX_LINE_BUFFER	(12400)

struct cam_resource_plan {
	int cam_resource[MTK_CAMSYS_RES_STEP_NUM];
};

enum resource_strategy_id {
	RESOURCE_STRATEGY_QPR = 0,
	RESOURCE_STRATEGY_PQR,
	RESOURCE_STRATEGY_RPQ,
	RESOURCE_STRATEGY_QRP,
	RESOURCE_STRATEGY_NUMBER
};

static const struct cam_resource_plan raw_resource_strategy_plan[] = {
	[RESOURCE_STRATEGY_QPR] = {
		.cam_resource = {
			E_RES_BASIC, E_RES_HWN1, E_RES_CLK1, E_RES_CLK2,
			E_RES_CLK3, E_RES_FRZ1, E_RES_BIN1, E_RES_HWN2} },
	[RESOURCE_STRATEGY_PQR] = {
		.cam_resource = {
			E_RES_BASIC, E_RES_HWN1, E_RES_HWN2, E_RES_FRZ1,
			E_RES_BIN1, E_RES_CLK1, E_RES_CLK2, E_RES_CLK3} },
	[RESOURCE_STRATEGY_RPQ] = {
		.cam_resource = {
			E_RES_BASIC, E_RES_FRZ1, E_RES_BIN1, E_RES_CLK1,
			E_RES_CLK2, E_RES_CLK3, E_RES_HWN1, E_RES_HWN2} },
	[RESOURCE_STRATEGY_QRP] = {
		.cam_resource = {
			E_RES_BASIC, E_RES_CLK1, E_RES_CLK2, E_RES_CLK3,
			E_RES_HWN1, E_RES_HWN2, E_RES_FRZ1, E_RES_BIN1} },
};

/* stagger raw select and mode decision preference */
#define STAGGER_MAX_STREAM_NUM 4

struct cam_stagger_select {
	int raw_select;
	int mode_decision;
};

struct cam_stagger_order {
	struct cam_stagger_select stagger_select[STAGGER_MAX_STREAM_NUM];
};

enum cam_stagger_stream_mode_plan {
	STAGGER_STREAM_PLAN_OTF_ALL = 0,
	STAGGER_STREAM_PLAN_OFF_ALL,
	STAGGER_STREAM_PLAN_OTF_DCIF_OFF_2EXP,
	STAGGER_STREAM_PLAN_OTF_DCIF_OFF_3EXP,
	STAGGER_STREAM_PLAN_NUM_MAX
};

enum cam_stagger_raw_select {
	RAW_SELECTION_A = 1 << MTKCAM_PIPE_RAW_A,
	RAW_SELECTION_B = 1 << MTKCAM_PIPE_RAW_B,
	RAW_SELECTION_C = 1 << MTKCAM_PIPE_RAW_C,
	RAW_SELECTION_AB_AUTO =
	(1 << MTKCAM_PIPE_RAW_A | 1 << MTKCAM_PIPE_RAW_B),
	RAW_SELECTION_AUTO =
	(1 << MTKCAM_PIPE_RAW_A | 1 << MTKCAM_PIPE_RAW_B | 1 << MTKCAM_PIPE_RAW_C),
};

static const struct cam_stagger_order stagger_mode_plan[] = {
	[STAGGER_STREAM_PLAN_OTF_ALL] = {
		.stagger_select = {
			{.raw_select = RAW_SELECTION_AUTO, .mode_decision = HW_MODE_ON_THE_FLY},
			{.raw_select = RAW_SELECTION_AUTO, .mode_decision = HW_MODE_ON_THE_FLY},
			{.raw_select = RAW_SELECTION_AUTO, .mode_decision = HW_MODE_ON_THE_FLY},
			{.raw_select = RAW_SELECTION_AUTO, .mode_decision = HW_MODE_ON_THE_FLY}
			} },
	[STAGGER_STREAM_PLAN_OFF_ALL] = {
		.stagger_select = {
			{.raw_select = RAW_SELECTION_C, .mode_decision = HW_MODE_OFFLINE},
			{.raw_select = RAW_SELECTION_C, .mode_decision = HW_MODE_OFFLINE},
			{.raw_select = RAW_SELECTION_C, .mode_decision = HW_MODE_OFFLINE},
			{.raw_select = RAW_SELECTION_C, .mode_decision = HW_MODE_OFFLINE}
			} },
	[STAGGER_STREAM_PLAN_OTF_DCIF_OFF_2EXP] = {
		.stagger_select = {
			{.raw_select = RAW_SELECTION_AUTO, .mode_decision = HW_MODE_ON_THE_FLY},
			{.raw_select = RAW_SELECTION_AUTO, .mode_decision = HW_MODE_DIRECT_COUPLED},
			{.raw_select = RAW_SELECTION_AUTO, .mode_decision = HW_MODE_OFFLINE},
			{.raw_select = RAW_SELECTION_AUTO, .mode_decision = HW_MODE_OFFLINE}
			} },
	[STAGGER_STREAM_PLAN_OTF_DCIF_OFF_3EXP] = {
		.stagger_select = {
			{.raw_select = RAW_SELECTION_AB_AUTO, .mode_decision = HW_MODE_ON_THE_FLY},
			{.raw_select = RAW_SELECTION_C, .mode_decision = HW_MODE_DIRECT_COUPLED},
			{.raw_select = RAW_SELECTION_AUTO, .mode_decision = HW_MODE_OFFLINE},
			{.raw_select = RAW_SELECTION_AUTO, .mode_decision = HW_MODE_OFFLINE}
			} },
};

static const struct v4l2_mbus_framefmt mfmt_default = {
	.code = MEDIA_BUS_FMT_SBGGR10_1X10,
	.width = DEFAULT_WIDTH,
	.height = DEFAULT_HEIGHT,
	.field = V4L2_FIELD_NONE,
	.colorspace = V4L2_COLORSPACE_SRGB,
	.xfer_func = V4L2_XFER_FUNC_DEFAULT,
	.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT,
	.quantization = V4L2_QUANTIZATION_DEFAULT,
};

static inline int mtk_pixelmode_val(int pxl_mode)
{
	int val = 0;

	switch (pxl_mode) {
	case 1:
		val = 0;
		break;
	case 2:
		val = 1;
		break;
	case 4:
		val = 2;
		break;
	case 8:
		val = 3;
		break;
	default:
		break;
	}

	return val;
}

static int mtk_raw_get_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_cam_resource *user_res;
	struct mtk_raw_pipeline *pipeline;
	struct device *dev;
	int ret = 0;

	pipeline = mtk_cam_ctrl_handler_to_raw_pipeline(ctrl->handler);
	dev = pipeline->raw->devs[pipeline->id];

	if (ctrl->id == V4L2_CID_MTK_CAM_RAW_RESOURCE_CALC) {
		user_res = (struct mtk_cam_resource *)ctrl->p_new.p;
		user_res->sensor_res = pipeline->user_res.sensor_res;
		user_res->raw_res = pipeline->user_res.raw_res;
		if (user_res->sink_fmt)
			ret = mtk_cam_res_copy_fmt_to_user(pipeline,
							   user_res,
							   pipeline->user_res.sink_fmt);
	} else if (ctrl->id == V4L2_CID_MTK_CAM_RAW_RESOURCE_UPDATE) {
		ctrl->val = pipeline->sensor_mode_update;
		dev_info(dev,
			 "%s:pipe(%d): V4L2_CID_MTK_CAM_RAW_RESOURCE_UPDATE: %d\n",
			 __func__, pipeline->id, pipeline->sensor_mode_update);
	} else if (ctrl->id == V4L2_CID_MTK_CAM_SYNC_ID) {
		mutex_lock(&pipeline->res_config.resource_lock);
		*ctrl->p_new.p_s64 = pipeline->sync_id;
		mutex_unlock(&pipeline->res_config.resource_lock);
	} else {
		/**
		 * Read the determined resource for the "set" format
		 * negotiations result
		 */
		mutex_lock(&pipeline->res_config.resource_lock);
		switch (ctrl->id) {
		case V4L2_CID_MTK_CAM_USED_ENGINE:
			ctrl->val = pipeline->res_config.raw_num_used;
			break;
		case V4L2_CID_MTK_CAM_BIN_LIMIT:
			ctrl->val = pipeline->res_config.bin_limit;
			break;
		case V4L2_CID_MTK_CAM_BIN:
			ctrl->val = pipeline->res_config.bin_enable;
			break;
		case V4L2_CID_MTK_CAM_FRZ:
			ctrl->val = pipeline->res_config.frz_enable ?
				    pipeline->res_config.frz_ratio : 100;
			break;
		default:
			dev_info(dev,
				 "%s:pipe(%d): unknown try resource CID: %d\n",
				 __func__, pipeline->id, ctrl->id);
			break;
		}
		mutex_unlock(&pipeline->res_config.resource_lock);

	}

	dev_dbg(dev, "%s:pipe(%d):id(%s) val(%d)\n",
		__func__, pipeline->id, ctrl->name, ctrl->val);
	return ret;
}

int mtk_cam_raw_res_store(struct mtk_raw_pipeline *pipeline,
			  struct mtk_cam_resource *res_user, char *dbg_str, bool log)
{
	struct device *dev = pipeline->raw->devs[pipeline->id];

	if (log)
		dev_info(dev,
			 "%s:%s:pipe(%d): from user: sensor:%d/%d/%lld/%lld/%d/%d, raw:%d/%d/%d/%d/%d/%d/%d/%d/%lld/%d\n",
			 __func__, dbg_str, pipeline->id,
			 res_user->sensor_res.hblank, res_user->sensor_res.vblank,
			 res_user->sensor_res.pixel_rate,
			 res_user->sensor_res.cust_pixel_rate,
			 res_user->sensor_res.interval.denominator,
			 res_user->sensor_res.interval.numerator,
			 res_user->raw_res.feature, res_user->raw_res.bin,
			 res_user->raw_res.path_sel, res_user->raw_res.raw_max,
			 res_user->raw_res.raw_min, res_user->raw_res.raw_used,
			 res_user->raw_res.strategy, res_user->raw_res.pixel_mode,
			 res_user->raw_res.throughput, res_user->raw_res.hw_mode);
	else
		dev_dbg(dev,
			"%s:%s:pipe(%d): from user: sensor:%d/%d/%lld/%lld/%d/%d, raw:%d/%d/%d/%d/%d/%d/%d/%d/%lld/%d\n",
			__func__, dbg_str, pipeline->id,
			res_user->sensor_res.hblank, res_user->sensor_res.vblank,
			res_user->sensor_res.pixel_rate,
			res_user->sensor_res.cust_pixel_rate,
			res_user->sensor_res.interval.denominator,
			res_user->sensor_res.interval.numerator,
			res_user->raw_res.feature, res_user->raw_res.bin,
			res_user->raw_res.path_sel, res_user->raw_res.raw_max,
			res_user->raw_res.raw_min, res_user->raw_res.raw_used,
			res_user->raw_res.strategy, res_user->raw_res.pixel_mode,
			res_user->raw_res.throughput, res_user->raw_res.hw_mode);

	/* check user value of sensor input parameters */
	if (!mtk_cam_feature_is_pure_m2m(res_user->raw_res.feature) &&
		 (!res_user->sensor_res.pixel_rate || !res_user->sensor_res.hblank ||
	    !res_user->sensor_res.vblank ||
	    !res_user->sensor_res.interval.denominator ||
	    !res_user->sensor_res.interval.numerator)) {
		dev_info(dev,
			 "%s:%s:pipe(%d): sensor info MUST be provided (TEST_PATTERN case)\n",
			 __func__, dbg_str, pipeline->id);
		/*test pattern case resource default copy*/
		pipeline->res_config.raw_num_used = 1;
		pipeline->res_config.bin_enable = 0;
		pipeline->res_config.tgo_pxl_mode = 1;
		pipeline->res_config.tgo_pxl_mode_before_raw = 1;
		pipeline->res_config.raw_path = 0;
		pipeline->res_config.hwn_limit_min = 1;
		pipeline->res_config.raw_feature = res_user->raw_res.feature;
		pipeline->feature_pending = res_user->raw_res.feature;
		pipeline->feature_active = res_user->raw_res.feature;
		pipeline->hw_mode = res_user->raw_res.hw_mode;
		pipeline->hw_mode_pending = res_user->raw_res.hw_mode;
		return 0;
	}

	/* check user value of raw input parameters */
	if (res_user->raw_res.feature == 0xFFFFFFFF)
		res_user->raw_res.feature = 0;

	if (res_user->raw_res.bin == 0xFF)
		res_user->raw_res.bin = 0;

	if (res_user->raw_res.strategy == 0xFFFF)
		res_user->raw_res.strategy = RESOURCE_STRATEGY_QRP;

	if (res_user->raw_res.raw_max == 0xFF)
		res_user->raw_res.raw_max = 2;

	if (res_user->raw_res.raw_min == 0xFF)
		res_user->raw_res.raw_min = 1;

	if (res_user->raw_res.raw_min > 1)
		res_user->raw_res.strategy = RESOURCE_STRATEGY_QPR;

	if (res_user->raw_res.path_sel == 0xFF)
		res_user->raw_res.path_sel = 0;

	if (res_user->raw_res.hw_mode == 0xFF)
		res_user->raw_res.hw_mode = HW_MODE_DEFAULT;

	if (log)
		dev_info(dev,
			 "%s:%s:pipe(%d): driver adjusted: raw:%d/%d/%d/%d/%d/%d/%d/%d/%lld/%d\n",
			 __func__, dbg_str, pipeline->id,
			 res_user->raw_res.feature, res_user->raw_res.bin,
			 res_user->raw_res.path_sel, res_user->raw_res.raw_max,
			 res_user->raw_res.raw_min, res_user->raw_res.raw_used,
			 res_user->raw_res.strategy, res_user->raw_res.pixel_mode,
			 res_user->raw_res.throughput, res_user->raw_res.hw_mode);
	else
		dev_dbg(dev,
			"%s:%s:pipe(%d): driver adjusted: raw:%d/%d/%d/%d/%d/%d/%d/%d/%lld/%d\n",
			__func__, dbg_str, pipeline->id,
			res_user->raw_res.feature, res_user->raw_res.bin,
			res_user->raw_res.path_sel, res_user->raw_res.raw_max,
			res_user->raw_res.raw_min, res_user->raw_res.raw_used,
			res_user->raw_res.strategy, res_user->raw_res.pixel_mode,
			res_user->raw_res.throughput, res_user->raw_res.hw_mode);

	return 0;
}

static s64 mtk_cam_calc_pure_m2m_pixelrate(s64 width, s64 height,
					struct v4l2_fract *interval)
{
/* process + r/wdma margin = (1 + 5%) x (1 + 10%) */
#define PURE_M2M_PROCESS_MARGIN_N 11550
#define PURE_M2M_PROCESS_MARGIN_D 10000
	s64 prate = 0;
	int fps_n = interval->numerator;
	int fps_d = interval->denominator;

	prate = width * height * fps_d * PURE_M2M_PROCESS_MARGIN_N;
	do_div(prate, fps_n * PURE_M2M_PROCESS_MARGIN_D);

	pr_info("%s:width:%d height:%d interval:%d/%d prate:%lld\n",
		__func__, width, height, fps_n, fps_d, prate);
	return prate;
}

static int mtk_cam_raw_set_res_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_raw_pipeline *pipeline;
	struct mtk_cam_resource *res_user;
	struct v4l2_mbus_framefmt sink_fmt;
	struct device *dev;
	int ret = 0;

	pipeline = mtk_cam_ctrl_handler_to_raw_pipeline(ctrl->handler);
	dev = pipeline->raw->devs[pipeline->id];
	res_user = (struct mtk_cam_resource *)ctrl->p_new.p;

	ret = mtk_cam_raw_res_store(pipeline, res_user, "s_ctrl", true);
	pipeline->user_res = *res_user;

	if (pipeline->subdev.entity.stream_count) {
		/* If the pipeline is streaming, pending the change */
		dev_dbg(dev, "%s:pipe(%d): pending res calc\n",
			__func__, pipeline->id);
		return ret;
	}

	ret = mtk_cam_res_copy_fmt_from_user(pipeline, res_user, &sink_fmt);
	if (ret)
		return ret;

	pipeline->res_config.sink_fmt = sink_fmt;
	ret = mtk_cam_raw_try_res_ctrl(pipeline, res_user,
				       &pipeline->res_config, &sink_fmt,
				       "s_ctrl", true);

	if (ret)
		return -EINVAL;

	return ret;
}

static int mtk_raw_set_res_ctrl(struct device *dev, struct v4l2_ctrl *ctrl,
				struct mtk_cam_resource_config *res_cfg,
				int pipe_id)
{
	int ret = 0;
	struct mtk_raw_pipeline *pipeline;

	pipeline = mtk_cam_ctrl_handler_to_raw_pipeline(ctrl->handler);

	if (ctrl->type == V4L2_CTRL_TYPE_INTEGER64)
		dev_dbg(dev, "%s:pipe(%d):(name:%s, val:%ld)\n", __func__,
			pipe_id, ctrl->name, *ctrl->p_new.p_s64);
	else
		dev_dbg(dev, "%s:pipe(%d):(name:%s, val:%d)\n", __func__,
			pipe_id, ctrl->name, ctrl->val);

	mutex_lock(&res_cfg->resource_lock);
	switch (ctrl->id) {
	case V4L2_CID_MTK_CAM_USED_ENGINE_LIMIT:
		res_cfg->hwn_limit_max = ctrl->val;
		break;
	case V4L2_CID_MTK_CAM_BIN_LIMIT:
		res_cfg->bin_limit = ctrl->val;
		break;
	case V4L2_CID_MTK_CAM_FRZ_LIMIT:
		res_cfg->frz_limit = ctrl->val;
		break;
	case V4L2_CID_MTK_CAM_RAW_PATH_SELECT:
		res_cfg->raw_path = ctrl->val;
		break;
	case V4L2_CID_MTK_CAM_SYNC_ID:
		pipeline->sync_id = *ctrl->p_new.p_s64;
		break;
	case V4L2_CID_MTK_CAM_MSTREAM_EXPOSURE:
		pipeline->mstream_exposure = *(struct mtk_cam_mstream_exposure *)ctrl->p_new.p;
		pipeline->mstream_exposure.valid = 1;
		break;
	case V4L2_CID_MTK_CAM_HSF_EN:
		res_cfg->enable_hsf_raw = ctrl->val;
		break;
	default:
		dev_info(dev,
			 "%s:pipe(%d):ctrl(id:0x%x,val:%d) not handled\n",
			 __func__, pipe_id, ctrl->id, ctrl->val);
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&res_cfg->resource_lock);

	return ret;
}

static int mtk_raw_try_ctrl(struct v4l2_ctrl *ctrl)
{
	struct device *dev;
	struct mtk_raw_pipeline *pipeline;
	struct mtk_cam_resource *res_user;
	struct mtk_cam_resource_config res_cfg;
	int ret = 0;

	pipeline = mtk_cam_ctrl_handler_to_raw_pipeline(ctrl->handler);
	dev = pipeline->raw->devs[pipeline->id];

	switch (ctrl->id) {
	case V4L2_CID_MTK_CAM_RAW_RESOURCE_CALC:
		res_user = (struct mtk_cam_resource *)ctrl->p_new.p;
		ret = mtk_cam_raw_res_store(pipeline, res_user, "try_ctrl", false);
		if (ret)
			break;

		ret = mtk_cam_res_copy_fmt_from_user(pipeline, res_user,
						     &res_cfg.sink_fmt);
		if (ret) {
			ret = 0;
			/* only corret the user's input, it may be request-based try and set fmt */
			dev_dbg(dev, "%s:pipe(%d): no sink_fmt from user, not calc res\n",
				__func__, pipeline->id);
			break;
		}

		ret = mtk_cam_raw_try_res_ctrl(pipeline, res_user, &res_cfg,
					       &res_cfg.sink_fmt, "try_ctrl", false);
		break;
	case V4L2_CID_MTK_CAM_RAW_RESOURCE_UPDATE:
		dev_info(dev,
			 "%s:pipe(%d): skip V4L2_CID_MTK_CAM_RAW_RESOURCE_UPDATE: %d\n",
			 __func__, pipeline->id, ctrl->val);
		ret = 0; /* no support */
		break;
	case V4L2_CID_MTK_CAM_TG_FLASH_CFG:
		ret = mtk_cam_tg_flash_try_ctrl(ctrl);
		break;
	/* skip control value checks */
	case V4L2_CID_MTK_CAM_MSTREAM_EXPOSURE:
	case V4L2_CID_MTK_CAM_FEATURE:
	case V4L2_CID_MTK_CAM_CAMSYS_HW_MODE:
	case V4L2_CID_MTK_CAM_USED_ENGINE_LIMIT:
	case V4L2_CID_MTK_CAM_BIN_LIMIT:
	case V4L2_CID_MTK_CAM_FRZ_LIMIT:
	case V4L2_CID_MTK_CAM_RAW_PATH_SELECT:
	case V4L2_CID_MTK_CAM_SYNC_ID:
	case V4L2_CID_MTK_CAM_HSF_EN:
		ret = 0;
		break;
	default:
		break;
	}

	return ret;
}

static int mtk_raw_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct device *dev;
	struct mtk_raw_pipeline *pipeline;
	int ret = 0;

	pipeline = mtk_cam_ctrl_handler_to_raw_pipeline(ctrl->handler);
	dev = pipeline->raw->devs[pipeline->id];

	switch (ctrl->id) {
	case V4L2_CID_MTK_CAM_RAW_RESOURCE_CALC:
		/**
		 * It also updates V4L2_CID_MTK_CAM_FEATURE and
		 * V4L2_CID_MTK_CAM_RAW_PATH_SELECT to device
		 */
		ret = mtk_cam_raw_set_res_ctrl(ctrl);
		break;
	case V4L2_CID_MTK_CAM_RAW_RESOURCE_UPDATE:
		/**
		 * sensor_mode_update should be reset by driver after the completion of
		 * resource updating (seamless switch)
		 */
		pipeline->sensor_mode_update = ctrl->val;
		dev_info(dev, "%s:pipe(%d):streaming(%d), sensor_mode_update(%d)\n",
			 __func__, pipeline->id, pipeline->subdev.entity.stream_count,
			 pipeline->sensor_mode_update);
		break;
	case V4L2_CID_MTK_CAM_TG_FLASH_CFG:
		ret = mtk_cam_tg_flash_s_ctrl(ctrl);
		break;
	case V4L2_CID_MTK_CAM_FEATURE:
		pipeline->feature_pending = *ctrl->p_new.p_s64;

		dev_dbg(dev,
			"%s:pipe(%d):streaming(%d), feature_pending(0x%x), feature_active(0x%x)\n",
			__func__, pipeline->id, pipeline->subdev.entity.stream_count,
			pipeline->feature_pending, pipeline->feature_active);
		ret = 0;
		break;
	case V4L2_CID_MTK_CAM_CAMSYS_HW_MODE:
	{
		pipeline->hw_mode_pending = *ctrl->p_new.p_s64;

		dev_dbg(dev,
			"%s:pipe(%d):streaming(%d), hw_mode(0x%x)\n",
			__func__, pipeline->id, pipeline->subdev.entity.stream_count,
			pipeline->hw_mode_pending);

		ret = 0;
	}
		break;
	default:
		ret = mtk_raw_set_res_ctrl(pipeline->raw->devs[pipeline->id],
					   ctrl, &pipeline->res_config,
					   pipeline->id);
		break;
	}

	return ret;
}

static const struct v4l2_ctrl_ops cam_ctrl_ops = {
	.g_volatile_ctrl = mtk_raw_get_ctrl,
	.s_ctrl = mtk_raw_set_ctrl,
	.try_ctrl = mtk_raw_try_ctrl,
};

static int mtk_raw_pde_get_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_raw_pipeline *pipeline;
	struct mtk_raw_pde_config *pde_cfg;
	struct mtk_cam_pde_info *pde_info_p;
	struct device *dev;
	int ret = 0;

	pipeline = mtk_cam_ctrl_handler_to_raw_pipeline(ctrl->handler);
	pde_cfg = &pipeline->pde_config;
	pde_info_p = ctrl->p_new.p;
	dev = pipeline->raw->devs[pipeline->id];

	switch (ctrl->id) {
	case V4L2_CID_MTK_CAM_PDE_INFO:
		pde_info_p->pdo_max_size = pde_cfg->pde_info.pdo_max_size;
		pde_info_p->pdi_max_size = pde_cfg->pde_info.pdi_max_size;
		pde_info_p->pd_table_offset = pde_cfg->pde_info.pd_table_offset;
		break;
	default:
		dev_info(dev, "%s(id:0x%x,val:%d) is not handled\n",
			 __func__, ctrl->id, ctrl->val);
		ret = -EINVAL;
	}

	return ret;
}

static int mtk_raw_pde_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_raw_pipeline *pipeline;
	struct mtk_raw_pde_config *pde_cfg;
	struct mtk_cam_pde_info *pde_info_p;
	struct device *dev;
	int ret = 0;
	struct mtk_cam_video_device *node;
	struct mtk_cam_dev_node_desc *desc;
	const struct v4l2_format *default_fmt;

	pipeline = mtk_cam_ctrl_handler_to_raw_pipeline(ctrl->handler);
	pde_cfg = &pipeline->pde_config;
	pde_info_p = ctrl->p_new.p;
	dev = pipeline->raw->devs[pipeline->id];

	node = &pipeline->vdev_nodes[MTK_RAW_META_IN - MTK_RAW_SINK_NUM];
	desc = &node->desc;
	default_fmt = &desc->fmts[desc->default_fmt_idx].vfmt;

	switch (ctrl->id) {
	case V4L2_CID_MTK_CAM_PDE_INFO:
		if (!pde_info_p->pdo_max_size || !pde_info_p->pdi_max_size) {
			dev_info(dev,
				 "%s:pdo_max_sz(%d)/pdi_max_sz(%d) cannot be 0\n",
				 __func__, pde_info_p->pdo_max_size,
				 pde_info_p->pdi_max_size);
			ret = -EINVAL;
			break;
		}

		pde_cfg->pde_info.pdo_max_size = pde_info_p->pdo_max_size;
		pde_cfg->pde_info.pdi_max_size = pde_info_p->pdi_max_size;
		pde_cfg->pde_info.pd_table_offset =
			default_fmt->fmt.meta.buffersize;
		break;
	default:
		dev_info(dev, "%s(id:0x%x,val:%d) is not handled\n",
			 __func__, ctrl->id, ctrl->val);
		ret = -EINVAL;
	}

	return ret;
}

static const struct v4l2_ctrl_ops cam_pde_ctrl_ops = {
	.g_volatile_ctrl = mtk_raw_pde_get_ctrl,
	.s_ctrl = mtk_raw_pde_set_ctrl,
	.try_ctrl = mtk_raw_pde_set_ctrl,
};

static const struct v4l2_ctrl_config hwn_limit = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_USED_ENGINE_LIMIT,
	.name = "Engine resource limitation",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 1,
	.max = 2,
	.step = 1,
	.def = 2,
};

static const struct v4l2_ctrl_config bin_limit = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_BIN_LIMIT,
	.name = "Binning limitation",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 0xfff,
	.step = 1,
	.def = 0,
};

static const struct v4l2_ctrl_config frz_limit = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_FRZ_LIMIT,
	.name = "Resizer limitation",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 70,
	.max = 100,
	.step = 1,
	.def = 100,
};

static const struct v4l2_ctrl_config hwn = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_USED_ENGINE,
	.name = "Engine resource",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 1,
	.max = 2,
	.step = 1,
	.def = 2,
};

static const struct v4l2_ctrl_config bin = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_BIN,
	.name = "Binning",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 1,
};

static const struct v4l2_ctrl_config hsf = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_HSF_EN,
	.name = "HSF raw",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 1,
};

static const struct v4l2_ctrl_config frz = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_FRZ,
	.name = "Resizer",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 70,
	.max = 100,
	.step = 1,
	.def = 100,
};

static const struct v4l2_ctrl_config raw_path = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_RAW_PATH_SELECT,
	.name = "Raw Path",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 7,
	.step = 1,
	.def = 1,
};

static const struct v4l2_ctrl_config hwn_try = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_USED_ENGINE_TRY,
	.name = "Engine resource",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 1,
	.max = 2,
	.step = 1,
	.def = 2,
};

static const struct v4l2_ctrl_config bin_try = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_BIN_TRY,
	.name = "Binning",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 1,
};

static const struct v4l2_ctrl_config frz_try = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_FRZ_TRY,
	.name = "Resizer",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 70,
	.max = 100,
	.step = 1,
	.def = 100,
};

static const struct v4l2_ctrl_config frame_sync_id = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_SYNC_ID,
	.name = "Frame sync id",
	.type = V4L2_CTRL_TYPE_INTEGER64,
	.min = -1,
	.max = 0x7FFFFFFF,
	.step = 1,
	.def = -1,
};

static const struct v4l2_ctrl_config mtk_feature = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_FEATURE,
	.name = "Mediatek camsys feature",
	.type = V4L2_CTRL_TYPE_INTEGER64,
	.min = 0,
	.max = RAW_FUNCTION_END,
	.step = 1,
	.def = 0,
};

static struct v4l2_ctrl_config cfg_res_ctrl = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_RAW_RESOURCE_CALC,
	.name = "resource ctrl",
	.type = V4L2_CTRL_COMPOUND_TYPES, /* V4L2_CTRL_TYPE_U32,*/
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0xffffffff,
	.step = 1,
	.dims = {sizeof(struct mtk_cam_resource)},
};

static struct v4l2_ctrl_config cfg_res_update = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_RAW_RESOURCE_UPDATE,
	.name = "resource update",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 0xf,
	.step = 1,
	.def = 0,
};

static const struct v4l2_ctrl_config mstream_exposure = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_MSTREAM_EXPOSURE,
	.name = "mstream exposure",
	.type = V4L2_CTRL_TYPE_U32,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0xFFFFFFFF,
	.step = 1,
	.dims = {sizeof_u32(struct mtk_cam_mstream_exposure)},
};

static const struct v4l2_ctrl_config mtk_cam_tg_flash_enable = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_TG_FLASH_CFG,
	.name = "Mediatek camsys tg flash",
	.type = V4L2_CTRL_COMPOUND_TYPES,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0xffffffff,
	.step = 1,
	.dims = {sizeof(struct mtk_cam_tg_flash_config)},
};

static const struct v4l2_ctrl_config cfg_pde_info = {
	.ops = &cam_pde_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_PDE_INFO,
	.name = "pde information",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_VOLATILE,
	.min = 0,
	.max = 0x1fffffff,
	.step = 1,
	.def = 0,
	.dims = {sizeof_u32(struct mtk_cam_pde_info)},
};

static const struct v4l2_ctrl_config mtk_camsys_hw_mode = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_CAMSYS_HW_MODE,
	.name = "Mediatek camsys hardware mode",
	.type = V4L2_CTRL_TYPE_INTEGER64,
	.min = 0,
	.max = 0x7FFFFFFF,
	.step = 1,
	.def = HW_MODE_DEFAULT,
};

struct v4l2_subdev *mtk_cam_find_sensor(struct mtk_cam_ctx *ctx,
					struct media_entity *entity)
{
	struct media_graph *graph;
	struct v4l2_subdev *sensor = NULL;

	graph = &ctx->pipeline.graph;
	media_graph_walk_start(graph, entity);

	while ((entity = media_graph_walk_next(graph))) {
		dev_dbg(ctx->cam->dev, "linked entity: %s\n", entity->name);
		sensor = NULL;

		switch (entity->function) {
		case MEDIA_ENT_F_CAM_SENSOR:
			sensor = media_entity_to_v4l2_subdev(entity);
			break;
		default:
			break;
		}

		if (sensor)
			break;
	}

	return sensor;
}

void mtk_cam_update_sensor(struct mtk_cam_ctx *ctx, struct v4l2_subdev *sensor)
{
	ctx->prev_sensor = ctx->sensor;
	ctx->sensor = sensor;
}

bool mtk_raw_sel_get_res(struct v4l2_subdev *sd,
					  struct v4l2_subdev_selection *sel,
					  struct mtk_cam_resource *res)
{
	void *user_ptr;
	u64 addr;

	addr = ((u64)sel->reserved[1] << 32) | sel->reserved[2];
	user_ptr = (void *)addr;
	if (!user_ptr) {
		dev_info(sd->v4l2_dev->dev, "%s: mtk_cam_resource is null\n",
			__func__);
		return false;
	}

	if (copy_from_user(res, user_ptr, sizeof(*res))) {
		dev_info(sd->v4l2_dev->dev, "%s: copy_from_user failedm user_ptr:%p\n",
			__func__, user_ptr);
		return false;
	}

	return res;
}

bool mtk_raw_fmt_get_res(struct v4l2_subdev *sd,
					  struct v4l2_subdev_format *fmt,
					  struct mtk_cam_resource *res)
{
	void *user_ptr;
	u64 addr;

	addr = ((u64)fmt->reserved[1] << 32) | fmt->reserved[2];
	user_ptr = (void *)addr;
	if (!user_ptr) {
		dev_info(sd->v4l2_dev->dev, "%s: mtk_cam_resource is null\n",
			__func__);
		return false;
	}

	if (copy_from_user(res, user_ptr, sizeof(*res))) {
		dev_info(sd->v4l2_dev->dev, "%s: copy_from_user failedm user_ptr:%p\n",
			__func__, user_ptr);
		return false;
	}

	dev_dbg(sd->v4l2_dev->dev, "%s:sensor:%d/%d/%lld/%d/%d, raw:%d/%d/%d/%d/%d/%d/%d/%d/%lld/%d\n",
		__func__,
		res->sensor_res.hblank, res->sensor_res.vblank,
		res->sensor_res.pixel_rate,	res->sensor_res.interval.denominator,
		res->sensor_res.interval.numerator,
		res->raw_res.feature, res->raw_res.bin, res->raw_res.path_sel,
		res->raw_res.raw_max, res->raw_res.raw_min, res->raw_res.raw_used,
		res->raw_res.strategy, res->raw_res.pixel_mode,
		res->raw_res.throughput, res->raw_res.hw_mode);

	return res;
}

unsigned int mtk_raw_get_hdr_scen_id(
	struct mtk_cam_ctx *ctx)
{
	unsigned int hw_scen =
		(1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER);

	if (mtk_cam_hw_is_otf(ctx))
		hw_scen = (1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER);
	else if (mtk_cam_hw_is_dc(ctx))
		hw_scen = (1 << MTKCAM_IPI_HW_PATH_OFFLINE_SRT_DCIF_STAGGER);
	else if (mtk_cam_hw_is_offline(ctx))
		hw_scen = (1 << MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER);

	return hw_scen;
}

struct mtk_raw_pipeline*
mtk_cam_get_link_enabled_raw(struct v4l2_subdev *seninf)
{
	struct mtk_cam_device *cam;
	int i;

	cam = container_of(seninf->v4l2_dev->mdev, struct mtk_cam_device, media_dev);
	for (i = MTKCAM_SUBDEV_RAW_0; i <= MTKCAM_SUBDEV_RAW_END; i++) {
		if (cam->raw.pipelines[i].res_config.seninf == seninf)
			return &cam->raw.pipelines[i];
	}

	return NULL;
}

struct v4l2_mbus_framefmt*
mtk_raw_pipeline_get_fmt(struct mtk_raw_pipeline *pipe,
			 struct v4l2_subdev_state *state,
			 int padid, int which)
{
	/* format invalid and return default format */
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(&pipe->subdev, state, padid);

	if (WARN_ON(padid >= pipe->subdev.entity.num_pads))
		return &pipe->cfg[0].mbus_fmt;

	return &pipe->cfg[padid].mbus_fmt;
}

struct v4l2_rect*
mtk_raw_pipeline_get_selection(struct mtk_raw_pipeline *pipe,
			       struct v4l2_subdev_state *state,
			       int pad, int which)
{
	/* format invalid and return default format */
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_crop(&pipe->subdev, state, pad);

	if (WARN_ON(pad >= pipe->subdev.entity.num_pads))
		return &pipe->cfg[0].crop;

	return &pipe->cfg[pad].crop;
}

int
mtk_cam_raw_try_res_ctrl(struct mtk_raw_pipeline *pipeline,
			 struct mtk_cam_resource *res_user,
			 struct mtk_cam_resource_config *res_cfg,
			 struct v4l2_mbus_framefmt *sink_fmt,
			 char *dbg_str, bool log)
{
	s64 prate = 0;
	int width, height;
	struct device *dev = pipeline->raw->devs[pipeline->id];

	res_cfg->bin_limit = res_user->raw_res.bin; /* 1: force bin on */
	res_cfg->frz_limit = 0;
	res_cfg->hwn_limit_max = res_user->raw_res.raw_max;
	res_cfg->hwn_limit_min = res_user->raw_res.raw_min;
	res_cfg->hblank = res_user->sensor_res.hblank;
	res_cfg->vblank = res_user->sensor_res.vblank;
	res_cfg->sensor_pixel_rate = res_user->sensor_res.pixel_rate;
	res_cfg->res_plan = res_user->raw_res.strategy;
	res_cfg->raw_feature = res_user->raw_res.feature;
	res_cfg->raw_path = res_user->raw_res.path_sel;
	res_cfg->hw_mode = res_user->raw_res.hw_mode;

	// currently only support normal & stagger 2-exp
	if (res_cfg->hw_mode != 0) {
		if (res_cfg->raw_feature & ~DC_SUPPORT_RAW_FEATURE_MASK) {
			dev_info(dev, "feature(%d) not support hw_mode(%d)",
				res_cfg->raw_feature, res_cfg->hw_mode);
			res_cfg->hw_mode = HW_MODE_DEFAULT;
			res_user->raw_res.hw_mode = HW_MODE_DEFAULT;
		}
	}

	if (res_user->sensor_res.cust_pixel_rate)
		prate = res_user->sensor_res.cust_pixel_rate;
	else if (mtk_cam_feature_is_pure_m2m(res_cfg->raw_feature))
		prate = mtk_cam_calc_pure_m2m_pixelrate(
			sink_fmt->width, sink_fmt->height, &res_cfg->interval);
	else
		prate = mtk_cam_seninf_calc_pixelrate
					(pipeline->raw->cam_dev, sink_fmt->width,
					 sink_fmt->height,
					 res_user->sensor_res.hblank,
					 res_user->sensor_res.vblank,
					 res_user->sensor_res.interval.denominator,
					 res_user->sensor_res.interval.numerator,
					 res_user->sensor_res.pixel_rate);
	/*worst case throughput prepare for stagger dynamic switch exposure num*/
	if (mtk_cam_feature_is_stagger(res_cfg->raw_feature)) {
		if (mtk_cam_feature_is_2_exposure(res_cfg->raw_feature)) {
			if (log)
				dev_info(dev,
					 "%s:%s:pipe(%d): worst case stagger 2exp prate (0x%x):%lld->%lld\n",
					 __func__, dbg_str, pipeline->id, res_cfg->raw_feature,
					 prate, prate * 3);
			prate = 2 * prate;
		} else if (mtk_cam_feature_is_3_exposure(res_cfg->raw_feature)) {
			if (log)
				dev_info(dev,
					 "%s:%s:pipe(%d): worst case stagger 3exp prate (0x%x):%lld->%lld\n",
					 __func__, dbg_str, pipeline->id, res_cfg->raw_feature,
					 prate, prate * 3);
			prate = 3 * prate;
		}
	}
	mtk_raw_resource_calc(dev_get_drvdata(pipeline->raw->cam_dev),
			      res_cfg, prate,
			      res_cfg->res_plan, sink_fmt->width,
			      sink_fmt->height, &width, &height);

	if (res_user->raw_res.bin && !res_cfg->bin_enable) {
		dev_info(dev,
			 "%s:%s:pipe(%d): res calc failed on fource bin: user(%d)/bin_enable(%d)\n",
			 __func__, dbg_str, pipeline->id, res_user->raw_res.bin,
			 res_cfg->bin_enable);
		return -EINVAL;
	}

	if (res_cfg->raw_num_used > res_user->raw_res.raw_max ||
	    res_cfg->raw_num_used < res_user->raw_res.raw_min) {
		dev_info(dev,
			 "%s:%s: pipe(%d): res calc failed on raw used: user(%d/%d)/raw_num_used(%d)\n",
			 __func__, dbg_str, pipeline->id, res_user->raw_res.raw_max,
			 res_user->raw_res.raw_min, res_cfg->raw_num_used);
	}

	res_user->raw_res.pixel_mode = res_cfg->tgo_pxl_mode;
	res_user->raw_res.raw_used = res_cfg->raw_num_used;
	if (res_cfg->bin_limit == BIN_AUTO)
		res_user->raw_res.bin = res_cfg->bin_enable;
	else
		res_user->raw_res.bin = res_cfg->bin_limit;
	res_user->sensor_res.driver_buffered_pixel_rate = prate;

	if (log)
		dev_info(dev,
			 "%s:%s:pipe(%d): res calc result: raw_used(%d)/bin(%d)/pixelmode(%d)/strategy(%d)/hw_mode(%d)\n",
			 __func__, dbg_str, pipeline->id, res_user->raw_res.raw_used,
			 res_user->raw_res.bin, res_user->raw_res.pixel_mode,
			 res_user->raw_res.strategy, res_user->raw_res.hw_mode);
	else
		dev_dbg(dev,
			"%s:%s:pipe(%d): res calc result: raw_used(%d)/bin(%d)/pixelmode(%d)/strategy(%d)/hw_mode(%d)\n",
			__func__, dbg_str, pipeline->id, res_user->raw_res.raw_used,
			res_user->raw_res.bin, res_user->raw_res.pixel_mode,
			res_user->raw_res.strategy, res_user->raw_res.hw_mode);

	/**
	 * Other output not reveal to user now:
	 * res_cfg->res_strategy[MTK_CAMSYS_RES_STEP_NUM];
	 * res_cfg->clk_target;
	 * res_cfg->frz_enable;
	 * res_cfg->frz_ratio;
	 * res_cfg->tgo_pxl_mode;
	 */
	if (width != sink_fmt->width || height != sink_fmt->height) {
		dev_info(dev,
			 "%s:%s:pipe(%d): size adjust info: raw: sink(%d,%d) res:(%d,%d)\n",
			 __func__, dbg_str, pipeline->id, sink_fmt->width,
			 sink_fmt->height, width, height);
	}

	return 0;
}

int
mtk_cam_res_copy_fmt_from_user(struct mtk_raw_pipeline *pipeline,
			       struct mtk_cam_resource *res_user,
			       struct v4l2_mbus_framefmt *dest)
{
	long bytes;
	struct device *dev = pipeline->raw->devs[pipeline->id];

	if (!res_user->sink_fmt) {
		dev_info(dev,
			 "%s:pipe(%d): sink_fmt can't be NULL for res ctrl\n",
			 __func__, pipeline->id);

		return -EINVAL;
	}

	bytes = copy_from_user(dest, (void *)res_user->sink_fmt,
			       sizeof(*dest));
	if (bytes) {
		dev_info(dev,
			 "%s:pipe(%d): copy_from_user on sink_fmt failed (%ld)\n",
			 __func__, pipeline->id, bytes);
		return -EINVAL;
	}

	return 0;
}

int
mtk_cam_res_copy_fmt_to_user(struct mtk_raw_pipeline *pipeline,
			     struct mtk_cam_resource *res_user,
			     struct v4l2_mbus_framefmt *src)
{
	long bytes;
	struct device *dev = pipeline->raw->devs[pipeline->id];

	/* return the fmt to the users */
	bytes = copy_to_user((void *)res_user->sink_fmt, src, sizeof(*src));
	if (bytes) {
		dev_info(dev,
			 "%s:pipe(%d): copy_to_user on sink_fmt failed (%ld)\n",
			 __func__, pipeline->id, bytes);

		return -EINVAL;
	}

	return 0;
}

static void mtk_raw_update_debug_param(struct mtk_cam_device *cam,
				       struct mtk_cam_resource_config *res,
				       int *clk_idx)
{
	struct mtk_camsys_dvfs *clk = &cam->camsys_ctrl.dvfs_info;

	/* skip if debug is not enabled */
	if (!debug_raw)
		return;

	dev_dbg(cam->dev, "%s:before:BIN/FRZ/HWN/CLK/pxl/pxl(seninf):", __func__);
	dev_dbg(cam->dev, "%d/%d(%d)/%d/%d/%d/%d, clk:%d\n",
		res->bin_enable, res->frz_enable, res->frz_ratio,
		res->raw_num_used, *clk_idx, res->tgo_pxl_mode,
		res->tgo_pxl_mode_before_raw, res->clk_target);

	if (debug_raw_num > 0) {
		dev_info(cam->dev, "DEBUG: force raw_num_used: %d\n",
			 debug_raw_num);
		res->raw_num_used = debug_raw_num;
	}

	if (debug_pixel_mode >= 0) {
		dev_info(cam->dev, "DEBUG: force debug_pixel_mode (log2): %d\n",
			 debug_pixel_mode);
		res->tgo_pxl_mode = debug_pixel_mode;
		res->tgo_pxl_mode_before_raw = debug_pixel_mode;
	}

	if (debug_clk_idx >= 0) {
		dev_info(cam->dev, "DEBUG: force debug_clk_idx: %d\n",
			 debug_clk_idx);
		*clk_idx = debug_clk_idx;
		res->clk_target = clk->clklv[debug_clk_idx];
	}

	dev_dbg(cam->dev,
		"%s:after:BIN/FRZ/HWN/CLK/pxl/pxl(seninf)=%d/%d(%d)/%d/%d/%d/%d, clk:%d\n",
		__func__, res->bin_enable, res->frz_enable, res->frz_ratio,
		res->raw_num_used, *clk_idx, res->tgo_pxl_mode,
		res->tgo_pxl_mode_before_raw, res->clk_target);

}

static bool is_cbn_en(int bin_flag)
{
	switch (bin_flag) {
	case CBN_2X2_ON:
	case CBN_3X3_ON:
	case CBN_4X4_ON:
		return true;
	default:
		return false;
	}
}

static int mtk_raw_pixelmode_calc(int rawpxl, int b_twin, bool b_bin,
				  bool b_frz, int min_ratio)
{
	int pixelmode = rawpxl;

	pixelmode = (b_twin == 2) ? pixelmode << 2 : pixelmode;
	pixelmode = (b_twin == 1) ? pixelmode << 1 : pixelmode;
	pixelmode = b_bin ? pixelmode << 1 : pixelmode;
	pixelmode = (b_frz && (min_ratio < FRZ_PXLMODE_THRES))
			? pixelmode << 1 : pixelmode;
	pixelmode = (pixelmode > TGO_MAX_PXLMODE) ?
		TGO_MAX_PXLMODE : pixelmode;

	return pixelmode;
}

static int mtk_raw_linebuf_chk(bool b_twin, bool b_bin, bool b_frz, bool b_qbn,
			       bool b_cbn, int tg_x, int *frz_ratio)
{
	int input_x = tg_x;
	/* max line buffer check for frontal binning and resizer */
	if (b_twin) {
		if (input_x > CAM_TWIN_PROCESS_MAX_LINE_BUFFER)
			return LB_CHECK_TWIN;
		input_x = input_x >> 1;
	}
	if (b_cbn) {
		if (input_x > CAM_RAW_CBN_MAX_LINE_BUFFER)
			return LB_CHECK_CBN;
		input_x = input_x >> 1;
	}
	if (b_qbn) {
		if (input_x > CAM_RAW_QBND_MAX_LINE_BUFFER)
			return LB_CHECK_QBN;
		input_x = input_x >> 1;
	}
	if (b_bin) {
		if (input_x > CAM_RAW_BIN_MAX_LINE_BUFFER)
			return LB_CHECK_BIN;
		input_x = input_x >> 1;
	}
	if (input_x <= CAM_RAW_PROCESS_MAX_LINE_BUFFER) {
		return LB_CHECK_OK;
	} else if (b_frz) {
		if (input_x > CAM_RAW_FRZ_MAX_LINE_BUFFER)
			return LB_CHECK_FRZ;

		*frz_ratio = input_x * 100 /
			CAM_RAW_PROCESS_MAX_LINE_BUFFER;
		return LB_CHECK_OK;
	} else {
		return LB_CHECK_RAW;
	}
}

bool mtk_raw_resource_calc(struct mtk_cam_device *cam,
			   struct mtk_cam_resource_config *res,
			   s64 pixel_rate, int res_plan,
			   int in_w, int in_h, int *out_w, int *out_h)
{
	struct mtk_camsys_dvfs *clk = &cam->camsys_ctrl.dvfs_info;
	u64 eq_throughput = clk->clklv[0];
	int res_step_type = 0;
	int tgo_pxl_mode = 1;
	int tgo_pxl_mode_before_raw = 1;
	int pixel_mode[MTK_CAMSYS_RES_STEP_NUM] = {0};
	int bin_temp = 0, frz_temp = 0, hwn_temp = 0;
	int bin_en = 0, frz_en = 0, twin_en = 0, clk_cur = 0;
	int idx = 0, clk_res = 0, idx_res = 0;
	bool res_found = false;
	int lb_chk_res = -1;
	int frz_ratio = 100;
	int p;
	bool find_max = 0;

	res->res_plan = res_plan;
	res->pixel_rate = pixel_rate;
	/* test pattern */
	if (res->pixel_rate == 0)
		res->pixel_rate = 450 * MHz;
	if (res->raw_feature & MTK_CAM_FEATURE_TIMESHARE_MASK)
		res->hwn_limit_max  = 1;
	dev_dbg(cam->dev,
		"[Res] PR = %lld, w/h=%d/%d HWN(%d)/BIN(%d)/FRZ(%d),Plan:%d\n",
		res->pixel_rate, in_w, in_h,
		res->hwn_limit_max, res->bin_limit, res->frz_limit, res->res_plan);
	memcpy(res->res_strategy, raw_resource_strategy_plan + res->res_plan,
	       MTK_CAMSYS_RES_STEP_NUM * sizeof(int));
	res->bin_enable = 0;
	res->raw_num_used = 1;
	res->frz_enable = 0;
	res->frz_ratio = frz_ratio;
	for (idx = 0; idx < MTK_CAMSYS_RES_STEP_NUM ; idx++) {
		res_step_type = res->res_strategy[idx] & MTK_CAMSYS_RES_IDXMASK;
		switch (res_step_type) {
		case MTK_CAMSYS_RES_BIN_TAG:
			bin_temp = res->res_strategy[idx] - E_RES_BIN_S;
			if (bin_temp <= res->bin_limit)
				bin_en = bin_temp;
			if (bin_en && frz_en)
				frz_en = 0;
			break;
		case MTK_CAMSYS_RES_FRZ_TAG:
			frz_temp = res->res_strategy[idx] - E_RES_FRZ_S;
			if (res->frz_limit < 100)
				frz_en = frz_temp;
			break;
		case MTK_CAMSYS_RES_HWN_TAG:
			hwn_temp = res->res_strategy[idx] - E_RES_HWN_S;
			if (hwn_temp + 1 <= res->hwn_limit_max)
				twin_en = hwn_temp;
			break;
		case MTK_CAMSYS_RES_CLK_TAG:
			clk_cur = res->res_strategy[idx] - E_RES_CLK_S;
			break;
		default:
			break;
		}

		/* 1 for force bin on */
		if (res->bin_limit >= 1)
			bin_en = 1;

		if (res->hwn_limit_min > 1)
			twin_en = 1;

		/* max line buffer check*/
		lb_chk_res = mtk_raw_linebuf_chk(twin_en, res->bin_limit & BIN_ON,
						 frz_en, res->bin_limit & QBND_ON,
						 is_cbn_en(res->bin_limit),
						 in_w, &frz_ratio);
		/* frz ratio*/
		if (res_step_type == MTK_CAMSYS_RES_FRZ_TAG) {
			if (eq_throughput > res->pixel_rate &&
			    lb_chk_res == LB_CHECK_OK)
				res->frz_ratio = frz_ratio;
			else
				res->frz_ratio =
					res->frz_limit < FRZ_PXLMODE_THRES
					? res->frz_limit : FRZ_PXLMODE_THRES;
		}
		if (res->raw_feature & MTK_CAM_FEATURE_TIMESHARE_MASK ||
			res->hw_mode == HW_MODE_DIRECT_COUPLED) {
			if (res->raw_feature & MTK_CAM_FEATURE_TIMESHARE_MASK)
				find_max = true;

			tgo_pxl_mode = mtk_raw_pixelmode_calc(MTK_CAMSYS_PROC_DEFAULT_PIXELMODE,
				twin_en, bin_en, frz_en, res->frz_ratio);
			pixel_mode[idx] = tgo_pxl_mode;
			if ((lb_chk_res == LB_CHECK_OK) &&
				(find_max || res_found == false)) {
				res->bin_enable = bin_en;
				res->frz_enable = frz_en;
				res->raw_num_used = twin_en + 1;
				clk_res = clk_cur;
				idx_res = idx;
				res->clk_target = clk->clklv[clk_res];
				res_found = true;
			}
		} else {
			/*try 1-pixel mode first*/
			for (p = 1; p <= MTK_CAMSYS_PROC_DEFAULT_PIXELMODE; p++) {
				tgo_pxl_mode = mtk_raw_pixelmode_calc(p, twin_en, bin_en, frz_en,
								      res->frz_ratio);
				/**
				 * isp throughput along resource strategy
				 * (compared with pixel rate)
				 */
				pixel_mode[idx] = tgo_pxl_mode;
				eq_throughput = ((u64)tgo_pxl_mode) * clk->clklv[clk_cur];
				if (eq_throughput > res->pixel_rate &&
				    lb_chk_res == LB_CHECK_OK) {
					if (!res_found) {
						res->bin_enable = bin_en;
						res->frz_enable = frz_en;
						res->raw_num_used = twin_en + 1;
						clk_res = clk_cur;
						idx_res = idx;
						res->clk_target = clk->clklv[clk_res];
						res_found = true;
						break;
					}
				}
			}
		}
		dev_dbg(cam->dev, "Res-%d B/F/H/C=%d/%d/%d/%d -> %d/%d/%d/%d (%d)(%d):%10llu\n",
			idx, bin_temp, frz_temp, hwn_temp, clk_cur, bin_en,
			frz_en, twin_en, clk_cur, lb_chk_res, pixel_mode[idx],
			eq_throughput);
	}

	tgo_pxl_mode = pixel_mode[idx_res];
	res->tgo_pxl_mode = mtk_pixelmode_val(tgo_pxl_mode);

	tgo_pxl_mode_before_raw = (res->hw_mode == HW_MODE_DIRECT_COUPLED) ?
		DC_DEFAULT_CAMSV_PIXELMODE : tgo_pxl_mode;
	res->tgo_pxl_mode_before_raw = mtk_pixelmode_val(tgo_pxl_mode_before_raw);

	mtk_raw_update_debug_param(cam, res, &clk_res);

	eq_throughput = ((u64)(1 << res->tgo_pxl_mode)) * res->clk_target;
	if (res_found) {
		dev_info(cam->dev, "Res-end:%d BIN/FRZ/HWN/CLK/pxl/pxl(seninf):", idx_res);
		dev_info(cam->dev,
			"Res-end:%d %d/%d(%d)/%d/%d/%d/%d:%10llu, clk:%d\n",
			idx_res, res->bin_enable, res->frz_enable, res->frz_ratio,
			res->raw_num_used, clk_res, res->tgo_pxl_mode,
			res->tgo_pxl_mode_before_raw, eq_throughput, res->clk_target);
	} else {
		dev_dbg(cam->dev, "[%s] Error resource result; use %dMhz\n",
			__func__, clk->clklv[clk_cur]);
		res->clk_target = clk->clklv[clk_cur];
	}
	if (res->bin_enable) {
		*out_w = in_w >> 1;
		*out_h = in_h >> 1;
	} else if (res->frz_enable) {
		*out_w = in_w * res->frz_ratio / 100;
		*out_h = in_h * res->frz_ratio / 100;
	} else {
		*out_w = in_w;
		*out_h = in_h;
	}

	return res_found;
}

static int mtk_raw_sd_subscribe_event(struct v4l2_subdev *subdev,
				      struct v4l2_fh *fh,
				      struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_FRAME_SYNC:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	case V4L2_EVENT_REQUEST_DRAINED:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	case V4L2_EVENT_EOS:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	case V4L2_EVENT_REQUEST_DUMPED:
		return v4l2_event_subscribe(fh, sub, 0, NULL);

	default:
		return -EINVAL;
	}
}

static int mtk_raw_available_resource(struct mtk_raw *raw)
{
	struct device *dev = raw->cam_dev;
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	int res_status = 0;
	int i, j;

	for (i = 0; i < cam_dev->num_raw_drivers; i++) {
		struct mtk_raw_pipeline *pipe = raw->pipelines + i;

		for (j = 0; j < ARRAY_SIZE(raw->devs); j++) {
			if (pipe->enabled_raw & 1 << j)
				res_status |= 1 << j;
		}
	}
	dev_dbg(raw->cam_dev, "%s raw_status:0x%x Available Engine:A/B/C:%d/%d/%d\n",
		 __func__, res_status,
			!(res_status & (1 << MTKCAM_SUBDEV_RAW_0)),
			!(res_status & (1 << MTKCAM_SUBDEV_RAW_1)),
			!(res_status & (1 << MTKCAM_SUBDEV_RAW_2)));

	return res_status;
}

static int raw_stagger_select(struct mtk_cam_ctx *ctx,
			int raw_status,
			int pipe_hw_mode,
			struct mtk_raw_stagger_select *result)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct cam_stagger_order stagger_order;
	struct cam_stagger_select stagger_select;
	struct mtk_cam_ctx *ctx_chk;
	int i, m;
	int stagger_ctx_num = 0, mode = 0;
	int stagger_plan = STAGGER_STREAM_PLAN_OTF_ALL;
	int stagger_order_mask[STAGGER_MAX_STREAM_NUM] = {0};
	int mask = 0x0;
	bool selected = false;

	stagger_order = stagger_mode_plan[stagger_plan];
	/* check how many stagger sensors are running, */
	/* this will affect the decision of which mode */
	/* to run for following stagger sensor         */
	for (i = 0;  i < cam->max_stream_num; i++) {
		ctx_chk = &cam->ctxs[i];
		if (ctx_chk != ctx && ctx_chk->streaming && mtk_cam_is_stagger(ctx_chk)) {
			for (m = 0; m < STAGGER_MAX_STREAM_NUM; m++) {
				mode = stagger_order.stagger_select[m].mode_decision;
				dev_info(cam->dev, "[%s:stagger check] i:%d/m:%d; mode:%d\n",
				__func__, i, m, mode);
				// TODO: hw_mode or hw_mode_pending
				if (mode == ctx_chk->pipe->hw_mode) {
					stagger_order_mask[m] = 1;
					break;
				}
			}
			stagger_ctx_num++;
		}
	}
	dev_info(cam->dev, "[%s:counter] num:%d; this ctx order is :%d (%d/%d/%d/%d)\n",
		__func__, stagger_ctx_num, stagger_ctx_num + 1,
		stagger_order_mask[0], stagger_order_mask[1],
		stagger_order_mask[2], stagger_order_mask[3]);

	/* check this ctx should use which raw_select_mask and mode */
	for (i = 0; i < stagger_ctx_num + 1; i++) {
		if (stagger_order_mask[i] == 0) {
			stagger_select = stagger_order.stagger_select[i];
			result->hw_mode = (pipe_hw_mode == HW_MODE_DEFAULT) ?
				stagger_select.mode_decision : pipe_hw_mode;
			dev_info(cam->dev, "[%s:plan:%d] raw_status 0x%x, stagger_select_raw_mask:0x%x hw mode:0x%x\n",
				__func__, i, raw_status, stagger_select.raw_select,
				result->hw_mode);
		}
	}

	result->enabled_raw = 0;
	for (m = MTKCAM_SUBDEV_RAW_0; m < RAW_PIPELINE_NUM; m++) {
		mask = 1 << m;
		if (stagger_select.raw_select & mask) { /*check stagger raw select mask*/
			if (!(raw_status & mask)) { /*check available raw select mask*/
				result->enabled_raw |= mask;
				selected = true;
				break;
			}
		} else {
			dev_info(cam->dev, "[%s:select] traversed current raw %d/0x%x, stagger_select_raw_mask:0x%x\n",
				__func__, m, mask, stagger_select.raw_select);
		}
	}

	return selected;
}

int mtk_cam_raw_stagger_select(struct mtk_cam_ctx *ctx,
			struct mtk_raw_pipeline *pipe, int raw_status)
{
	bool selected;
	struct mtk_raw_stagger_select result;

	selected = raw_stagger_select(ctx,
				raw_status,
				ctx->pipe->hw_mode,
				&result);

	ctx->pipe->hw_mode = result.hw_mode;
	ctx->pipe->hw_mode_pending = result.hw_mode;
	pipe->enabled_raw |= result.enabled_raw;

	return selected;
}

static int mtk_cam_s_data_raw_stagger_select(struct mtk_cam_request_stream_data *s_data,
			    int raw_status)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_raw_pipeline *pipe;
	bool selected;
	struct mtk_raw_stagger_select *result;
	struct mtk_cam_req_raw_pipe_data *s_raw_pipe_data;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	pipe = ctx->pipe;

	s_raw_pipe_data = mtk_cam_s_data_get_raw_pipe_data(s_data);
	if (!s_raw_pipe_data) {
		dev_info(ctx->cam->dev, "%s: failed to get raw_pipe_data (pipe:%d, seq:%d)\n",
			 __func__, s_data->pipe_id, s_data->frame_seq_no);
		return -EINVAL;
	}

	result = &s_raw_pipe_data->stagger_select;
	selected = raw_stagger_select(ctx,
				raw_status,
				pipe->hw_mode,
				result);

	ctx->pipe->hw_mode_pending = result->hw_mode;
	s_raw_pipe_data->enabled_raw |= result->enabled_raw;

	return selected;

}

int mtk_cam_s_data_raw_select(struct mtk_cam_request_stream_data *s_data,
			    struct mtkcam_ipi_input_param *cfg_in_param)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	struct mtk_raw_pipeline *pipe;
	int raw_used = 0;
	bool selected = false;
	int feature;

	feature = mtk_cam_s_data_get_res_feature(s_data);
	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	pipe = ctx->pipe;

	if (pipe->enabled_raw & MTKCAM_SUBDEV_RAW_MASK)
		raw_used = MTKCAM_SUBDEV_RAW_MASK & ~pipe->enabled_raw;
	else
		raw_used = mtk_raw_available_resource(pipe->raw);

	if (mtk_cam_feature_is_stagger(feature))
		selected = mtk_cam_s_data_raw_stagger_select(s_data, raw_used);

	mtk_raw_available_resource(pipe->raw);

	if (!selected)
		return -EINVAL;

	return 0;
}

int mtk_cam_raw_select(struct mtk_cam_ctx *ctx,
		       struct mtkcam_ipi_input_param *cfg_in_param)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_raw_pipeline *pipe = ctx->pipe;
	struct mtk_raw_pipeline *pipe_chk;
	int raw_status = 0;
	int mask = 0x0;
	bool selected = false;
	int m;

	raw_status = mtk_raw_available_resource(pipe->raw);

	if (mtk_cam_is_stagger(ctx)) {
		selected = mtk_cam_raw_stagger_select(ctx, pipe, raw_status);
	} else if (pipe->feature_pending & MTK_CAM_FEATURE_TIMESHARE_MASK) {
		int ts_id, ts_id_chk;
		/*First, check if group ID used in every pipeline*/
		/*if yes , use same engine*/
		for (m = MTKCAM_SUBDEV_RAW_0; m < ARRAY_SIZE(pipe->raw->devs); m++) {
			pipe_chk = pipe->raw->pipelines + m;
			dev_info(cam->dev, "[%s] checking idx:%d pipe_id:%d pipe_chk_id:%d\n",
				__func__, m, pipe->id, pipe_chk->id);
			if (pipe->id != pipe_chk->id) {
				ts_id = pipe->feature_active &
						MTK_CAM_FEATURE_TIMESHARE_MASK;
				ts_id_chk = pipe_chk->feature_active &
							MTK_CAM_FEATURE_TIMESHARE_MASK;
				if (ts_id == ts_id_chk &&
					pipe_chk->enabled_raw != 0) {
					mask = pipe_chk->enabled_raw & MTKCAM_SUBDEV_RAW_MASK;
					pipe->enabled_raw |= mask;
					selected = true;
					break;
				}
			}
		}
		if (selected) {
			dev_info(cam->dev, "[%s] Timeshared (%d)- enabled_raw:0x%x as pipe:%d enabled_raw:0x%x\n",
				__func__, ts_id >> 8, pipe->enabled_raw,
				pipe_chk->id, pipe_chk->enabled_raw);
		} else {
			/*if no , use new engine from a->b->c*/
			for (m = MTKCAM_SUBDEV_RAW_0; m < ARRAY_SIZE(pipe->raw->devs); m++) {
				pipe_chk = pipe->raw->pipelines + m;
				mask = 1 << m;
				if (!(raw_status & mask)) {
					pipe->enabled_raw |= mask;
					selected = true;
					break;
				}
			}
		}
	} else if (pipe->res_config.raw_num_used == 3) {
		mask = 1 << MTKCAM_SUBDEV_RAW_0
			| 1 << MTKCAM_SUBDEV_RAW_1 | 1 << MTKCAM_SUBDEV_RAW_2;
		if (!(raw_status & mask)) {
			pipe->enabled_raw |= mask;
			selected = true;
		}
	} else if (pipe->res_config.raw_num_used == 2) {
		for (m = MTKCAM_SUBDEV_RAW_0; m >= MTKCAM_SUBDEV_RAW_0; m--) {
			mask = (1 << m) | (1 << (m + 1));
			if (!(raw_status & mask)) {
				pipe->enabled_raw |= mask;
				selected = true;
				break;
			}
		}
	} else {
		for (m = MTKCAM_SUBDEV_RAW_0; m < ARRAY_SIZE(pipe->raw->devs); m++) {
			mask = 1 << m;
			if (!(raw_status & mask)) {
				pipe->enabled_raw |= mask;
				selected = true;
				break;
			}
		}
	}
	mtk_raw_available_resource(pipe->raw);
	if (!selected)
		return -EINVAL;
	/**
	 * TODO: duplicated using raw case will implement in time sharing isp case
	 */

	return 0;
}

static int mtk_raw_sd_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_raw *raw = pipe->raw;
	struct mtk_cam_device *cam = dev_get_drvdata(raw->cam_dev);
	struct mtk_cam_ctx *ctx = mtk_cam_find_ctx(cam, &sd->entity);
	unsigned int i;

	if (WARN_ON(!ctx))
		return -EINVAL;

	if (enable) {
		pipe->feature_active = pipe->user_res.raw_res.feature;
		pipe->enabled_dmas = 0;
		ctx->pipe = pipe;
		ctx->used_raw_num++;
		pipe->feature_active = pipe->user_res.raw_res.feature;

		ctx->pipe->hw_mode = pipe->res_config.hw_mode;
		ctx->pipe->hw_mode_pending = pipe->res_config.hw_mode;

		for (i = 0; i < ARRAY_SIZE(pipe->vdev_nodes); i++) {
			if (!pipe->vdev_nodes[i].enabled)
				continue;
			pipe->enabled_dmas |=
				(1ULL << pipe->vdev_nodes[i].desc.dma_port);
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(raw->devs); i++) {
			if (pipe->enabled_raw & 1 << i) {
				dev_info(raw->cam_dev, "%s: power off raw (%d)\n", __func__, i);
				pm_runtime_put_sync(raw->devs[i]);
			}
		}
	}

	dev_info(raw->cam_dev, "%s:raw-%d: en %d, dev 0x%x dmas 0x%x hw_mode %d\n",
		 __func__, pipe->id, enable, pipe->enabled_raw,
		 pipe->enabled_dmas, ctx->pipe->hw_mode);

	return 0;
}

static int mtk_raw_init_cfg(struct v4l2_subdev *sd,
			    struct v4l2_subdev_state *state)
{
	struct v4l2_mbus_framefmt *mf;
	unsigned int i;
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_raw *raw = pipe->raw;

	for (i = 0; i < sd->entity.num_pads; i++) {
		mf = v4l2_subdev_get_try_format(sd, state, i);
		*mf = mfmt_default;
		pipe->cfg[i].mbus_fmt = mfmt_default;

		dev_dbg(raw->cam_dev, "%s init pad:%d format:0x%x\n",
			sd->name, i, mf->code);
	}

	return 0;
}

static bool mtk_raw_try_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_format *fmt)
{
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_raw *raw = pipe->raw;
	unsigned int user_fmt;

	dev_dbg(raw->cam_dev, "%s:s_fmt: check format 0x%x, w:%d, h:%d field:%d\n",
		sd->name, fmt->format.code, fmt->format.width,
		fmt->format.height, fmt->format.field);

	/* check sensor format */
	if (fmt->pad == MTK_RAW_SINK) {
		user_fmt = mtk_cam_get_sensor_fmt(fmt->format.code);
		if (user_fmt == MTKCAM_IPI_IMG_FMT_UNKNOWN)
			return false;
	}

	return true;
}

static void propagate_fmt(struct v4l2_mbus_framefmt *sink_mf,
			  struct v4l2_mbus_framefmt *source_mf, int w, int h)
{
	source_mf->code = sink_mf->code;
	source_mf->colorspace = sink_mf->colorspace;
	source_mf->field = sink_mf->field;
	source_mf->width = w;
	source_mf->height = h;
}

int mtk_raw_set_src_pad_selection_default(struct v4l2_subdev *sd,
					  struct v4l2_subdev_state *state,
					  struct v4l2_mbus_framefmt *sink_fmt,
					  struct mtk_cam_resource *res,
					  int pad, int which)
{
	struct v4l2_rect *source_sel;
	struct mtk_raw_pipeline *pipe;

	pipe = container_of(sd, struct mtk_raw_pipeline, subdev);
	source_sel = mtk_raw_pipeline_get_selection(pipe, state, pad, which);
	if (source_sel->width > sink_fmt->width) {
		source_sel->width = sink_fmt->width;
		/* may need some log */
	}

	if (source_sel->height > sink_fmt->height) {
		source_sel->height = sink_fmt->height;
		/* may need some log */
	}

	return 0;

}

int mtk_raw_set_src_pad_selection_yuv(struct v4l2_subdev *sd,
					  struct v4l2_subdev_state *state,
					  struct v4l2_mbus_framefmt *sink_fmt,
					  struct mtk_cam_resource *res,
					  int pad, int which)
{
	int i;
	struct v4l2_rect *prev_yuv = NULL, *source_sel, *tmp_sel;
	struct v4l2_mbus_framefmt *framefmt;
	struct mtk_raw_pipeline *pipe;

	pipe = container_of(sd, struct mtk_raw_pipeline, subdev);
	mtk_raw_set_src_pad_selection_default(sd, state, sink_fmt, res, pad, which);
	source_sel = mtk_raw_pipeline_get_selection(pipe, state, pad, which);

	for (i = MTK_RAW_YUVO_1_OUT; i < pad; i++) {
		framefmt = mtk_raw_pipeline_get_fmt(pipe, state, pad, which);
		tmp_sel = mtk_raw_pipeline_get_selection(pipe, state, pad, which);

		/* Skip disabled YUV pad */
		if (!mtk_cam_is_pad_fmt_enable(framefmt))
			continue;

		prev_yuv = tmp_sel;
	}

	if (prev_yuv) {
		if (source_sel->width != prev_yuv->width) {
			source_sel->width = prev_yuv->width;
			/* may need some log */
		}

		if (source_sel->height != prev_yuv->height) {
			source_sel->height = prev_yuv->height;
			/* may need some log */
		}
	}

	return 0;

}

struct v4l2_mbus_framefmt*
mtk_raw_get_sink_pad_framefmt(struct v4l2_subdev *sd,
					  struct v4l2_subdev_state *state, int which)
{
	struct v4l2_mbus_framefmt *sink_fmt = NULL, *tmp_fmt;
	struct mtk_raw_pipeline *pipe;
	int i;

	pipe = container_of(sd, struct mtk_raw_pipeline, subdev);
	for (i = MTK_RAW_SINK; i < MTK_RAW_SOURCE_BEGIN; i++) {
		tmp_fmt = mtk_raw_pipeline_get_fmt(pipe, state, i, which);
		if (i != MTK_RAW_META_IN &&
			mtk_cam_is_pad_fmt_enable(tmp_fmt)) {
			sink_fmt = tmp_fmt;
			break;
		}
	}

	return sink_fmt;
}

static int mtk_raw_set_pad_selection(struct v4l2_subdev *sd,
					  struct v4l2_subdev_state *state,
					  struct v4l2_subdev_selection *sel)
{
	struct v4l2_mbus_framefmt *sink_fmt = NULL;
	struct mtk_raw_pipeline *pipe;
	struct mtk_cam_video_device *node;
	struct mtk_cam_resource *res = NULL;
	struct v4l2_rect *crop;
	int ret;

	if (sel->pad < MTK_RAW_MAIN_STREAM_OUT || sel->pad >= MTK_RAW_META_OUT_BEGIN)
		return -EINVAL;

	pipe = container_of(sd, struct mtk_raw_pipeline, subdev);

	/*
	 * Find the sink pad fmt, there must be one eanbled sink pad at least
	 */
	sink_fmt = mtk_raw_get_sink_pad_framefmt(sd, state, sel->which);
	if (!sink_fmt)
		return -EINVAL;

	node = &pipe->vdev_nodes[sel->pad - MTK_RAW_SINK_NUM];
	crop = mtk_raw_pipeline_get_selection(pipe, state, sel->pad, sel->which);
	*crop = sel->r;
	ret = node->desc.pad_ops->set_pad_selection(sd, state, sink_fmt, res, sel->pad, sel->which);
	if (ret)
		return -EINVAL;

	sel->r = *crop;

	return 0;
}

static int mtk_raw_get_pad_selection(struct v4l2_subdev *sd,
					  struct v4l2_subdev_state *state,
					  struct v4l2_subdev_selection *sel)
{
	return 0;
}

int mtk_raw_set_sink_pad_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *state,
			   struct v4l2_subdev_format *fmt)
{
	struct device *dev;
	struct mtk_cam_video_device *node;
	const char *node_str;
	const struct mtk_cam_format_desc *fmt_desc;
	struct mtk_raw_pipeline *pipe;
	int i;
	int ipi_fmt;
	struct v4l2_mbus_framefmt *framefmt, *source_fmt = NULL, *tmp_fmt;

	/* Do nothing for pad to meta video device */
	if (fmt->pad == MTK_RAW_META_IN)
		return 0;

	dev = sd->v4l2_dev->dev;
	pipe = container_of(sd, struct mtk_raw_pipeline, subdev);
	framefmt = mtk_raw_pipeline_get_fmt(pipe, state, fmt->pad, fmt->which);

	/* If data from sensor, we check the size with max imgo size*/
	if (fmt->pad < MTK_RAW_SINK_NUM) {
		/* from sensor */
		node = &pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
		node_str = "sink";
	} else {
		/* from memory */
		node = &pipe->vdev_nodes[fmt->pad - MTK_RAW_SINK_NUM];
		node_str = node->desc.name;
	}

	ipi_fmt = mtk_cam_get_sensor_fmt(framefmt->code);
	if (ipi_fmt == MTKCAM_IPI_IMG_FMT_UNKNOWN) {
		/**
		 * Set imgo's default fmt, the user must check
		 * if the pad sink format is the same as the
		 * source format of the link before stream on.
		 */
		fmt_desc = &node->desc.fmts[node->desc.default_fmt_idx];
		framefmt->code = fmt_desc->pfmt.code;
		dev_info(dev,
			"%s(%d): Adjust unaccept fmt code on sink pad:%d, 0x%x->0x%x\n",
			__func__, fmt->which, fmt->pad, fmt->format.code, framefmt->code);
	}

	/* Reset pads' enable state*/
	for (i = MTK_RAW_SINK; i < MTK_RAW_META_OUT_BEGIN; i++) {
		if (i == MTK_RAW_META_IN)
			continue;

		tmp_fmt = mtk_raw_pipeline_get_fmt(pipe, state, i, fmt->which);
		mtk_cam_pad_fmt_enable(tmp_fmt, false);
	}

	/* TODO: copy the filed we are used only*/
	*framefmt = fmt->format;
	if (framefmt->width > node->desc.frmsizes->stepwise.max_width)
		framefmt->width = node->desc.frmsizes->stepwise.max_width;

	if (framefmt->height > node->desc.frmsizes->stepwise.max_height)
		framefmt->height = node->desc.frmsizes->stepwise.max_height;

	mtk_cam_pad_fmt_enable(framefmt, true);

	dev_info(dev,
		"%s(%d): Set fmt pad:%d(%s), code/w/h = 0x%x/%d/%d\n",
		__func__, fmt->which, fmt->pad, node_str,
		framefmt->code, framefmt->width, framefmt->height);

	/* Propagation inside subdev */
	for (i = MTK_RAW_SOURCE_BEGIN; i < MTK_RAW_META_OUT_BEGIN; i++) {
		source_fmt = mtk_raw_pipeline_get_fmt(pipe, state, i, fmt->which);

		/* Get default format's desc for the pad */
		node = &pipe->vdev_nodes[i - MTK_RAW_SINK_NUM];

		/**
		 * Propagate the size from sink pad to source pades, adjusted
		 * based on each pad's default format.
		 */
		if (source_fmt->width > node->desc.frmsizes->stepwise.max_width)
			source_fmt->width = node->desc.frmsizes->stepwise.max_width;
		else
			source_fmt->width = framefmt->width;

		if (source_fmt->height > node->desc.frmsizes->stepwise.max_height)
			source_fmt->height = node->desc.frmsizes->stepwise.max_height;
		else
			source_fmt->height = framefmt->height;

		dev_dbg(dev,
			"%s(%d): Propagate to pad:%d(%s), (0x%x/%d/%d)\n",
			__func__, fmt->which, fmt->pad, node->desc.name,
			source_fmt->code, source_fmt->width, source_fmt->height);

	}

	return 0;
}

int mtk_raw_set_src_pad_fmt_default(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *state,
			   struct v4l2_mbus_framefmt *sink_fmt,
			   struct mtk_cam_resource *res,
			   int pad, int which)
{
	struct device *dev;
	struct v4l2_mbus_framefmt *source_fmt;
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_cam_video_device *node;

	dev = sd->v4l2_dev->dev;
	node = &pipe->vdev_nodes[pad - MTK_RAW_SINK_NUM];
	source_fmt = mtk_raw_pipeline_get_fmt(pipe, state, pad, which);
	if (source_fmt->width > sink_fmt->width) {
		dev_info(dev,
			"%s(%d): adjusted: width(%d) over sink (%d)\n",
			__func__, which, pad, node->desc.name,
			source_fmt->width, sink_fmt->width);
		source_fmt->width = sink_fmt->width;

	}

	if (source_fmt->height > sink_fmt->height) {
		dev_info(dev,
			"%s(%d): adjusted: width(%d) over sink (%d)\n",
			__func__, which, pad, node->desc.name,
			source_fmt->height, sink_fmt->height);
		source_fmt->height = sink_fmt->height;
	}

	if (source_fmt->width > node->desc.frmsizes->stepwise.max_width) {
		dev_info(dev,
			"%s(%d): adjusted: width(%d) over max (%d)\n",
			__func__, which, pad, node->desc.name,
			source_fmt->width, node->desc.frmsizes->stepwise.max_width);
		source_fmt->width = node->desc.frmsizes->stepwise.max_width;
	}

	if (source_fmt->height > node->desc.frmsizes->stepwise.max_height) {
		dev_info(dev,
			"%s(%d): adjusted: height(%d) over max (%d)\n",
			__func__, which, pad, node->desc.name,
			source_fmt->height, node->desc.frmsizes->stepwise.max_height);
	}

	return 0;

}

int mtk_raw_set_src_pad_fmt_rzh1n2(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *state,
			   struct v4l2_mbus_framefmt *sink_fmt,
			   struct mtk_cam_resource *res,
			   int pad, int which)
{
	struct device *dev;
	struct v4l2_mbus_framefmt *source_fmt;
	struct v4l2_mbus_framefmt *tmp_fmt;
	struct mtk_cam_video_device *node;
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);

	dev = sd->v4l2_dev->dev;
	mtk_raw_set_src_pad_fmt_default(sd, state, sink_fmt, res, pad, which);
	source_fmt = mtk_raw_pipeline_get_fmt(pipe, state, pad, which);
	node = &pipe->vdev_nodes[pad - MTK_RAW_SINK_NUM];

	/* rzh1n2to_r1 and rzh1n2to_r3 size must be the same */
	if (pad == MTK_RAW_RZH1N2TO_3_OUT) {
		tmp_fmt = mtk_raw_pipeline_get_fmt(pipe, state, MTK_RAW_RZH1N2TO_1_OUT, which);
		if (mtk_cam_is_pad_fmt_enable(tmp_fmt) &&
			source_fmt->height != tmp_fmt->height &&
			source_fmt->width != tmp_fmt->width) {
			dev_info(dev,
				"%s(%d): adjusted: rzh1n2to_r3(%d,%d) and rzh1n2to_r1(%d,%d) must have the same sz\n",
				__func__, which, pad, node->desc.name,
				source_fmt->width, source_fmt->height,
				tmp_fmt->width, tmp_fmt->height);
			source_fmt->width = tmp_fmt->width;
			source_fmt->height = tmp_fmt->height;
		}

	}

	return 0;

}

int mtk_raw_set_src_pad_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *state,
			   struct v4l2_subdev_format *fmt)
{
	struct device *dev;
	struct mtk_cam_resource res;
	struct mtk_cam_video_device *node;
	const struct mtk_cam_format_desc *fmt_desc;
	struct mtk_raw_pipeline *pipe;
	int ret = 0;
	struct v4l2_mbus_framefmt *source_fmt, *sink_fmt = NULL;

	/* Do nothing for pad to meta video device */
	if (fmt->pad >= MTK_RAW_META_OUT_BEGIN)
		return 0;

	pipe = container_of(sd, struct mtk_raw_pipeline, subdev);
	dev = sd->v4l2_dev->dev;
	node = &pipe->vdev_nodes[fmt->pad - MTK_RAW_SINK_NUM];

	/*
	 * Find the sink pad fmt, there must be one eanbled sink pad at least
	 */
	sink_fmt = mtk_raw_get_sink_pad_framefmt(sd, state, fmt->which);
	if (!sink_fmt) {
		dev_info(dev,
			"%s(%d): Set fmt pad:%d(%s), no s_fmt on sink pad\n",
			__func__, fmt->which, fmt->pad, node->desc.name);
		return -EINVAL;
	}

	fmt_desc = &node->desc.fmts[node->desc.default_fmt_idx];
	if (!mtk_raw_fmt_get_res(sd, fmt, &res)) {
		dev_info(dev,
			"%s(%d): Set fmt pad:%d(%s), no mtk_cam_resource found\n",
			__func__, fmt->which, fmt->pad, node->desc.name);
		return -EINVAL;
	}

	if (node->desc.pad_ops->set_pad_fmt) {
		/* call source pad's set_pad_fmt op to adjust fmt by pad */
		source_fmt = mtk_raw_pipeline_get_fmt(pipe, state, fmt->pad, fmt->which);
		/* TODO: copy the fileds we are used only*/
		*source_fmt = fmt->format;
		ret = node->desc.pad_ops->set_pad_fmt(sd, state, sink_fmt, &res, fmt->pad,
											fmt->which);
	}

	if (ret)
		return ret;

	dev_dbg(dev,
		"%s(%d): s_fmt to pad:%d(%s), user(0x%x/%d/%d) driver(0x%x/%d/%d)\n",
		__func__, fmt->which, fmt->pad, node->desc.name,
		fmt->format.code, fmt->format.width, fmt->format.height,
		source_fmt->code, source_fmt->width, source_fmt->height);
	mtk_cam_pad_fmt_enable(source_fmt, false);
	fmt->format = *source_fmt;

	return 0;
}

int mtk_raw_try_pad_fmt(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_format *fmt)
{

	if (fmt->pad >= MTK_RAW_SINK && fmt->pad < MTK_RAW_SOURCE_BEGIN)
		mtk_raw_set_sink_pad_fmt(sd, state, fmt);
	else if (fmt->pad < MTK_RAW_PIPELINE_PADS_NUM)
		mtk_raw_set_src_pad_fmt(sd, state, fmt);
	else
		return -EINVAL;

	return 0;
}

unsigned int mtk_cam_get_rawi_sensor_pixel_fmt(unsigned int fmt)
{
	// return V4L2_PIX_FMT_MTISP for matching
	// length returned by mtk_cam_get_pixel_bits()
	// with ipi_fmt returned by mtk_cam_get_sensor_fmt()

	switch (fmt & SENSOR_FMT_MASK) {
	case MEDIA_BUS_FMT_SBGGR8_1X8:
		return V4L2_PIX_FMT_SBGGR8;
	case MEDIA_BUS_FMT_SGBRG8_1X8:
		return V4L2_PIX_FMT_SGBRG8;
	case MEDIA_BUS_FMT_SGRBG8_1X8:
		return V4L2_PIX_FMT_SGRBG8;
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		return V4L2_PIX_FMT_SRGGB8;
	case MEDIA_BUS_FMT_SBGGR10_1X10:
		return V4L2_PIX_FMT_MTISP_SBGGR10;
	case MEDIA_BUS_FMT_SGBRG10_1X10:
		return V4L2_PIX_FMT_MTISP_SGBRG10;
	case MEDIA_BUS_FMT_SGRBG10_1X10:
		return V4L2_PIX_FMT_MTISP_SGRBG10;
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		return V4L2_PIX_FMT_MTISP_SGRBG10;
	case MEDIA_BUS_FMT_SBGGR12_1X12:
		return V4L2_PIX_FMT_MTISP_SBGGR12;
	case MEDIA_BUS_FMT_SGBRG12_1X12:
		return V4L2_PIX_FMT_MTISP_SGBRG12;
	case MEDIA_BUS_FMT_SGRBG12_1X12:
		return V4L2_PIX_FMT_MTISP_SGRBG12;
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		return V4L2_PIX_FMT_MTISP_SRGGB12;
	case MEDIA_BUS_FMT_SBGGR14_1X14:
		return V4L2_PIX_FMT_MTISP_SBGGR14;
	case MEDIA_BUS_FMT_SGBRG14_1X14:
		return V4L2_PIX_FMT_MTISP_SGBRG14;
	case MEDIA_BUS_FMT_SGRBG14_1X14:
		return V4L2_PIX_FMT_MTISP_SGRBG14;
	case MEDIA_BUS_FMT_SRGGB14_1X14:
		return V4L2_PIX_FMT_MTISP_SRGGB14;
	default:
		break;
	}
	return V4L2_PIX_FMT_MTISP_SBGGR14;
}

static int mtk_raw_call_set_fmt(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_format *fmt,
				bool streaming)
{
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_raw *raw = pipe->raw;
	struct v4l2_mbus_framefmt *mf;
	unsigned int sink_ipi_fmt = MTKCAM_IPI_IMG_FMT_UNKNOWN;

	if (!sd || !fmt) {
		dev_dbg(raw->cam_dev, "%s: Required sd(%p), fmt(%p)\n",
			__func__, sd, fmt);
		return -EINVAL;
	}

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY && !state) {
		dev_dbg(raw->cam_dev, "%s: Required sd(%p), state(%p) for FORMAT_TRY\n",
					__func__, sd, state);
		return -EINVAL;
	}


	if (!mtk_raw_try_fmt(sd, fmt) &&
		!mtk_cam_feature_is_pure_m2m(
			pipe->feature_pending)) {
		mf = mtk_raw_pipeline_get_fmt(pipe, state, fmt->pad, fmt->which);
		fmt->format = *mf;
		dev_info(raw->cam_dev,
			 "sd:%s pad:%d didn't apply and keep format w/h/code %d/%d/0x%x\n",
			 sd->name, fmt->pad, mf->width, mf->height, mf->code);
	} else {
		mf = mtk_raw_pipeline_get_fmt(pipe, state, fmt->pad, fmt->which);
		*mf = fmt->format;
		dev_dbg(raw->cam_dev,
			"sd:%s pad:%d set format w/h/code %d/%d/0x%x\n",
			sd->name, fmt->pad, mf->width, mf->height, mf->code);
	}

	/* sink pad format propagate to source pad */
	if (fmt->pad == MTK_RAW_SINK) {
		struct v4l2_mbus_framefmt *source_mf;
		struct v4l2_format *img_fmt;

		if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
			img_fmt = &pipe->vdev_nodes[MTK_RAW_SINK].sink_fmt_for_dc_rawi;
			img_fmt->fmt.pix_mp.width = mf->width;
			img_fmt->fmt.pix_mp.height = mf->height;

			sink_ipi_fmt = mtk_cam_get_sensor_fmt(mf->code);

			if (sink_ipi_fmt == MTKCAM_IPI_IMG_FMT_UNKNOWN) {
				dev_info(raw->cam_dev,
					"%s: sink_ipi_fmt not found\n");

				sink_ipi_fmt = MTKCAM_IPI_IMG_FMT_BAYER14;
			}

			img_fmt->fmt.pix_mp.pixelformat =
				mtk_cam_get_rawi_sensor_pixel_fmt(mf->code);

			img_fmt->fmt.pix_mp.plane_fmt[0].bytesperline =
				mtk_cam_dmao_xsize(mf->width, sink_ipi_fmt, 3);
			img_fmt->fmt.pix_mp.plane_fmt[0].sizeimage =
				img_fmt->fmt.pix_mp.plane_fmt[0].bytesperline *
				img_fmt->fmt.pix_mp.height;

			img_fmt = &pipe->vdev_nodes[MTK_RAW_SINK].pending_fmt;

			img_fmt->fmt.pix_mp.width = mf->width;
			img_fmt->fmt.pix_mp.height = mf->height;

			dev_dbg(raw->cam_dev,
				"%s: sd:%s update sink pad format %dx%d code 0x%x\n",
				__func__, sd->name,
				img_fmt->fmt.pix_mp.width,
				img_fmt->fmt.pix_mp.height,
				mf->code);
		}

		source_mf = mtk_raw_pipeline_get_fmt(pipe, state,
						     MTK_RAW_MAIN_STREAM_OUT,
						     fmt->which);

		if (streaming) {
			propagate_fmt(mf, source_mf, mf->width, mf->height);

			return 0;
		}

		/**
		 * User will trigger resource calc with V4L2_CID_MTK_CAM_RAW_RESOURCE_CALC
		 * so we don't need to trigger it here anymore.
		 */
		propagate_fmt(mf, source_mf, mf->width, mf->height);
	}

	return 0;
}

static int mtk_raw_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *state,
			   struct v4l2_subdev_format *fmt)
{
	struct media_request *req;
	struct mtk_cam_request *cam_req;
	struct mtk_cam_request_stream_data *stream_data;

	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_cam_device *cam = dev_get_drvdata(pipe->raw->cam_dev);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		return mtk_raw_try_pad_fmt(sd, state, fmt);

	/* if the pipeline is streaming, pending the change */
	if (!sd->entity.stream_count)
		return mtk_raw_call_set_fmt(sd, state, fmt, false);

	if (v4l2_subdev_format_request_fd(fmt) <= 0)
		return -EINVAL;

	req = media_request_get_by_fd(&cam->media_dev,
				v4l2_subdev_format_request_fd(fmt));
	if (req) {
		cam_req = to_mtk_cam_req(req);
		dev_info(cam->dev, "sd:%s pad:%d pending success, req fd(%d)\n",
			sd->name, fmt->pad, v4l2_subdev_format_request_fd(fmt));
	} else {
		dev_info(cam->dev, "sd:%s pad:%d pending failed, req fd(%d) invalid\n",
			sd->name, fmt->pad, v4l2_subdev_format_request_fd(fmt));
		return -EINVAL;
	}

	stream_data = mtk_cam_req_get_s_data_no_chk(cam_req, pipe->id, 0);
	stream_data->pad_fmt_update |= (1 << fmt->pad);
	stream_data->pad_fmt[fmt->pad] = *fmt;

	media_request_put(req);

	return 0;
}

static int mtk_raw_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *fmt)
{
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_raw *raw = pipe->raw;
	struct v4l2_mbus_framefmt *mf;

	mf = mtk_raw_pipeline_get_fmt(pipe, state, fmt->pad, fmt->which);
	fmt->format = *mf;
	dev_dbg(raw->cam_dev, "sd:%s pad:%d get format w/h/code %d/%d/0x%x\n",
		sd->name, fmt->pad, fmt->format.width, fmt->format.height,
		fmt->format.code);

	return 0;
}

int mtk_raw_call_pending_set_fmt(struct v4l2_subdev *sd,
				 struct v4l2_subdev_format *fmt)
{
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_cam_device *cam = dev_get_drvdata(pipe->raw->cam_dev);

	/* We only allow V4L2_SUBDEV_FORMAT_ACTIVE for pending set fmt */
	if (fmt->which != V4L2_SUBDEV_FORMAT_ACTIVE) {
		dev_info(cam->dev,
			"%s:pipe(%d):pad(%d): only allow V4L2_SUBDEV_FORMAT_ACTIVE\n",
			__func__, pipe->id, fmt->pad);
		return -EINVAL;
	}

	return mtk_raw_call_set_fmt(sd, NULL, fmt, true);
}

static int mtk_cam_media_link_setup(struct media_entity *entity,
				    const struct media_pad *local,
				    const struct media_pad *remote, u32 flags)
{
	struct mtk_raw_pipeline *pipe =
		container_of(entity, struct mtk_raw_pipeline, subdev.entity);
	struct mtk_raw *raw = pipe->raw;
	u32 pad = local->index;

	dev_dbg(raw->cam_dev, "%s: raw %d: remote:%s:%d->local:%s:%d flags:0x%x\n",
		__func__, pipe->id, remote->entity->name, remote->index,
		local->entity->name, local->index, flags);

	if (pad < MTK_RAW_PIPELINE_PADS_NUM && pad != MTK_RAW_SINK)
		pipe->vdev_nodes[pad - MTK_RAW_SINK_NUM].enabled =
			!!(flags & MEDIA_LNK_FL_ENABLED);

	if (!entity->stream_count && !(flags & MEDIA_LNK_FL_ENABLED))
		memset(pipe->cfg, 0, sizeof(pipe->cfg));

	if (pad == MTK_RAW_SINK && flags & MEDIA_LNK_FL_ENABLED) {
		struct mtk_seninf_pad_data_info result;

		pipe->res_config.seninf =
			media_entity_to_v4l2_subdev(remote->entity);
		mtk_cam_seninf_get_pad_data_info(pipe->res_config.seninf,
			PAD_SRC_GENERAL0, &result);
		if (result.exp_hsize) {
			pipe->cfg[MTK_RAW_META_SV_OUT_0].mbus_fmt.width =
				result.exp_hsize;
			pipe->cfg[MTK_RAW_META_SV_OUT_0].mbus_fmt.height =
				result.exp_vsize;
			dev_info(raw->cam_dev, "[%s:meta0] hsize/vsize:%d/%d\n",
				__func__, result.exp_hsize, result.exp_vsize);
		}
	}

	return 0;
}

static int
mtk_raw_s_frame_interval(struct v4l2_subdev *sd,
			 struct v4l2_subdev_frame_interval *interval)
{
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_raw *raw = pipe->raw;

	if (v4l2_frame_interval_which(interval) == V4L2_SUBDEV_FORMAT_TRY) {
		dev_dbg(raw->cam_dev, "%s:pipe(%d):try res: fps = %d/%d, not used\n",
			__func__, pipe->id,
			interval->interval.numerator,
			interval->interval.denominator);
	} else {
		dev_dbg(raw->cam_dev, "%s:pipe(%d):current res: fps = %d/%d",
			__func__, pipe->id,
			interval->interval.numerator,
			interval->interval.denominator);
		pipe->res_config.interval = interval->interval;
	}

	return 0;
}

static const struct v4l2_subdev_core_ops mtk_raw_subdev_core_ops = {
	.subscribe_event = mtk_raw_sd_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops mtk_raw_subdev_video_ops = {
	.s_stream =  mtk_raw_sd_s_stream,
	.s_frame_interval = mtk_raw_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops mtk_raw_subdev_pad_ops = {
	.link_validate = mtk_cam_link_validate,
	.init_cfg = mtk_raw_init_cfg,
	.set_fmt = mtk_raw_set_fmt,
	.get_fmt = mtk_raw_get_fmt,
	.set_selection = mtk_raw_set_pad_selection,
	.get_selection = mtk_raw_get_pad_selection,
};

static const struct v4l2_subdev_ops mtk_raw_subdev_ops = {
	.core = &mtk_raw_subdev_core_ops,
	.video = &mtk_raw_subdev_video_ops,
	.pad = &mtk_raw_subdev_pad_ops,
};

static const struct media_entity_operations mtk_cam_media_entity_ops = {
	.link_setup = mtk_cam_media_link_setup,
	.link_validate = v4l2_subdev_link_validate,
};

static const struct v4l2_ioctl_ops mtk_cam_v4l2_vout_ioctl_ops = {
	.vidioc_querycap = mtk_cam_vidioc_querycap,
	.vidioc_enum_framesizes = mtk_cam_vidioc_enum_framesizes,
	.vidioc_enum_fmt_vid_cap = mtk_cam_vidioc_enum_fmt,
	.vidioc_g_fmt_vid_out_mplane = mtk_cam_vidioc_g_fmt,
	.vidioc_s_fmt_vid_out_mplane = mtk_cam_vidioc_s_fmt,
	.vidioc_try_fmt_vid_out_mplane = mtk_cam_vidioc_try_fmt,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static const struct v4l2_ioctl_ops mtk_cam_v4l2_vcap_ioctl_ops = {
	.vidioc_querycap = mtk_cam_vidioc_querycap,
	.vidioc_enum_framesizes = mtk_cam_vidioc_enum_framesizes,
	.vidioc_enum_fmt_vid_cap = mtk_cam_vidioc_enum_fmt,
	.vidioc_g_fmt_vid_cap_mplane = mtk_cam_vidioc_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane = mtk_cam_vidioc_s_fmt,
	.vidioc_try_fmt_vid_cap_mplane = mtk_cam_vidioc_try_fmt,
	.vidioc_s_selection = mtk_cam_vidioc_s_selection,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static const struct v4l2_ioctl_ops mtk_cam_v4l2_meta_cap_ioctl_ops = {
	.vidioc_querycap = mtk_cam_vidioc_querycap,
	.vidioc_enum_fmt_meta_cap = mtk_cam_vidioc_meta_enum_fmt,
	.vidioc_g_fmt_meta_cap = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_s_fmt_meta_cap = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_try_fmt_meta_cap = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
};

static const struct v4l2_ioctl_ops mtk_cam_v4l2_meta_out_ioctl_ops = {
	.vidioc_querycap = mtk_cam_vidioc_querycap,
	.vidioc_enum_fmt_meta_out = mtk_cam_vidioc_meta_enum_fmt,
	.vidioc_g_fmt_meta_out = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_s_fmt_meta_out = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_try_fmt_meta_out = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
};

static const struct mtk_cam_format_desc stream_out_fmts[] = {
	/* This is a default image format */
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR8,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SBGGR8_1X8,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR10,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SBGGR10_1X10,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR10,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SBGGR10_1X10,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR12,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SBGGR12_1X12,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR12,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SBGGR12_1X12,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR14,
			 .num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SBGGR14_1X14,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR14,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SBGGR14_1X14,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG8,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SGBRG8_1X8,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG10,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SGBRG10_1X10,
		}

	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG10,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SGBRG10_1X10,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG12,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SGBRG12_1X12,
		}

	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG12,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SGBRG12_1X12,
		}

	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG14,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SGBRG14_1X14,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG14,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SGBRG14_1X14,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG8,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SGRBG8_1X8,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG10,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SGRBG10_1X10,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG10,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SGRBG10_1X10,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG12,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SGRBG12_1X12,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG12,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SGRBG12_1X12,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG14,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SGRBG14_1X14,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG14,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SGRBG14_1X14,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB8,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SRGGB8_1X8,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB10,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SRGGB10_1X10,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB10,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SRGGB10_1X10,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB10P,
		},
	},

	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB12,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB12,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SRGGB12_1X12,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB14,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SRGGB14_1X14,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB14,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SRGGB14_1X14,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR16,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SBGGR16_1X16,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG16,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SGBRG16_1X16,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG16,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SGRBG16_1X16,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB16,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SRGGB16_1X16,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_BAYER8_UFBC,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_BAYER10_UFBC,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_BAYER12_UFBC,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_BAYER14_UFBC,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_YUYV,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_YVYU,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_UYVY,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_VYUY,
		},
	},
};

static const struct mtk_cam_format_desc yuv_out_group1_fmts[] = {
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12_10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21_10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12_12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21_12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_12P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_12P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_YUV420,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_UFBC,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_UFBC,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_10_UFBC,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_10_UFBC,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_12_UFBC,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_12_UFBC,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRB8F,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRB10F,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRB12F,
		},
	}
};

static const struct mtk_cam_format_desc yuv_out_group2_fmts[] = {
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12_10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21_10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12_12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21_12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_12P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_12P,
		},
	}
};

static const struct mtk_cam_format_desc rzh1n2to1_out_fmts[] = {
	{
		.vfmt.fmt.pix_mp = {
			.width = RZH1N2TO1_MAX_WIDTH,
			.height = RZH1N2TO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = RZH1N2TO1_MAX_WIDTH,
			.height = RZH1N2TO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = RZH1N2TO1_MAX_WIDTH,
			.height = RZH1N2TO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV16,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = RZH1N2TO1_MAX_WIDTH,
			.height = RZH1N2TO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV61,
		},
	}
};

static const struct mtk_cam_format_desc rzh1n2to2_out_fmts[] = {
	{
		.vfmt.fmt.pix_mp = {
			.width = RZH1N2TO2_MAX_WIDTH,
			.height = RZH1N2TO2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_YUYV,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = RZH1N2TO2_MAX_WIDTH,
			.height = RZH1N2TO2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_YVYU,
		},
	}
};

static const struct mtk_cam_format_desc rzh1n2to3_out_fmts[] = {
	{
		.vfmt.fmt.pix_mp = {
			.width = RZH1N2TO3_MAX_WIDTH,
			.height = RZH1N2TO3_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = RZH1N2TO3_MAX_WIDTH,
			.height = RZH1N2TO3_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = RZH1N2TO3_MAX_WIDTH,
			.height = RZH1N2TO3_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV16,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = RZH1N2TO3_MAX_WIDTH,
			.height = RZH1N2TO3_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV61,
		},
	}
};

static const struct mtk_cam_format_desc drzs4no1_out_fmts[] = {
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_GREY,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV16,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV61,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV16_10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV61_10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV16_10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV61_10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12_10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21_10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_10P,
		},
	},
};

static const struct mtk_cam_format_desc drzs4no2_out_fmts[] = {
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO2_MAX_WIDTH,
			.height = DRZS4NO2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_GREY,
		},
	}
};

static const struct mtk_cam_format_desc drzs4no3_out_fmts[] = {
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO3_MAX_WIDTH,
			.height = DRZS4NO3_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_GREY,
		},
	}
};

#define MTK_RAW_TOTAL_OUTPUT_QUEUES 4

static const struct
mtk_cam_dev_node_desc output_queues[] = {
	{
		.id = MTK_RAW_META_IN,
		.name = "meta input",
		.cap = V4L2_CAP_META_OUTPUT,
		.buf_type = V4L2_BUF_TYPE_META_OUTPUT,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = false,
#ifdef CONFIG_MTK_SCP
		.smem_alloc = true,
#else
		.smem_alloc = false,
#endif
		.need_cache_sync_on_prepare = true,
		.dma_port = MTKCAM_IPI_RAW_META_STATS_CFG,
		.default_fmt_idx = 0,
		.max_buf_count = 16,
		.ioctl_ops = &mtk_cam_v4l2_meta_out_ioctl_ops,
	},
	{
		.id = MTK_RAW_RAWI_2_IN,
		.name = "rawi 2",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_RAWI_2,
		.fmts = stream_out_fmts,
		.num_fmts = ARRAY_SIZE(stream_out_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vout_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = IMG_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = IMG_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_RAWI_3_IN,
		.name = "rawi 3",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.link_flags = 0,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_RAWI_3,
		.fmts = stream_out_fmts,
		.num_fmts = ARRAY_SIZE(stream_out_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vout_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = IMG_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = IMG_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_RAWI_4_IN,
		.name = "rawi 4",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.link_flags = 0,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_RAWI_3, //todo: wait backend add MTKCAM_IPI_RAW_RAWI_4
		.fmts = stream_out_fmts,
		.num_fmts = ARRAY_SIZE(stream_out_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vout_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = IMG_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = IMG_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	}
};

static const char *output_queue_names[RAW_PIPELINE_NUM][MTK_RAW_TOTAL_OUTPUT_QUEUES] = {
	{"mtk-cam raw-0 meta-input", "mtk-cam raw-0 rawi-2",
	 "mtk-cam raw-0 rawi-3", "mtk-cam raw-0 rawi-4"},
	{"mtk-cam raw-1 meta-input", "mtk-cam raw-1 rawi-2",
	 "mtk-cam raw-1 rawi-3", "mtk-cam raw-1 rawi-4"},
	{"mtk-cam raw-2 meta-input", "mtk-cam raw-2 rawi-2",
	 "mtk-cam raw-2 rawi-3", "mtk-cam raw-2 rawi-4"},
};

struct mtk_cam_pad_ops source_pad_ops_default = {
	.set_pad_fmt = mtk_raw_set_src_pad_fmt_default,
	.set_pad_selection = mtk_raw_set_src_pad_selection_default,
};

struct mtk_cam_pad_ops source_pad_ops_yuv = {
	.set_pad_fmt = mtk_raw_set_src_pad_fmt_default,
	.set_pad_selection = mtk_raw_set_src_pad_selection_yuv,
};

struct mtk_cam_pad_ops source_pad_ops_drzs4no = {
	.set_pad_fmt = mtk_raw_set_src_pad_fmt_default,
	.set_pad_selection = mtk_raw_set_src_pad_selection_default,
};

struct mtk_cam_pad_ops source_pad_ops_rzh1n2 = {
	.set_pad_fmt = mtk_raw_set_src_pad_fmt_rzh1n2,
	.set_pad_selection = mtk_raw_set_src_pad_selection_default,
};
#ifndef PREISP
#define MTK_RAW_TOTAL_CAPTURE_QUEUES 15 //todo :check backend node size
#else
#define MTK_RAW_TOTAL_CAPTURE_QUEUES 20 //todo :check backend node size
#endif
static const struct
mtk_cam_dev_node_desc capture_queues[] = {
	{
		.id = MTK_RAW_MAIN_STREAM_OUT,
		.name = "imgo",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_IMGO,
		.fmts = stream_out_fmts,
		.num_fmts = ARRAY_SIZE(stream_out_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_default,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = IMG_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = IMG_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_YUVO_1_OUT,
		.name = "yuvo 1",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_YUVO_1,
		.fmts = yuv_out_group1_fmts,
		.num_fmts = ARRAY_SIZE(yuv_out_group1_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_yuv,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = YUV_GROUP1_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = YUV_GROUP1_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_YUVO_2_OUT,
		.name = "yuvo 2",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_YUVO_2,
		.fmts = yuv_out_group2_fmts,
		.num_fmts = ARRAY_SIZE(yuv_out_group2_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_yuv,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = YUV_GROUP2_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = YUV_GROUP2_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_YUVO_3_OUT,
		.name = "yuvo 3",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_YUVO_3,
		.fmts = yuv_out_group1_fmts,
		.num_fmts = ARRAY_SIZE(yuv_out_group1_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_yuv,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = YUV_GROUP1_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = YUV_GROUP1_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_YUVO_4_OUT,
		.name = "yuvo 4",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_YUVO_4,
		.fmts = yuv_out_group2_fmts,
		.num_fmts = ARRAY_SIZE(yuv_out_group2_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_yuv,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = YUV_GROUP2_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = YUV_GROUP2_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_YUVO_5_OUT,
		.name = "yuvo 5",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_YUVO_5,
		.fmts = yuv_out_group2_fmts,
		.num_fmts = ARRAY_SIZE(yuv_out_group2_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_yuv,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = YUV_GROUP2_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = YUV_GROUP2_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_DRZS4NO_1_OUT,
		.name = "drzs4no 1",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_DRZS4NO_1,
		.fmts = drzs4no1_out_fmts,
		.num_fmts = ARRAY_SIZE(drzs4no1_out_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_drzs4no,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = DRZS4NO1_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = DRZS4NO1_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_DRZS4NO_2_OUT,
		.name = "drzs4no 2",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_DRZS4NO_2,
		.fmts = drzs4no2_out_fmts,
		.num_fmts = ARRAY_SIZE(drzs4no2_out_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_drzs4no,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = DRZS4NO2_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = DRZS4NO2_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_DRZS4NO_3_OUT,
		.name = "drzs4no 3",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_DRZS4NO_3,
		.fmts = drzs4no3_out_fmts,
		.num_fmts = ARRAY_SIZE(drzs4no3_out_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_drzs4no,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = DRZS4NO3_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = DRZS4NO3_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_RZH1N2TO_1_OUT,
		.name = "rzh1n2to 1",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_RZH1N2TO_1,
		.fmts = rzh1n2to1_out_fmts,
		.num_fmts = ARRAY_SIZE(rzh1n2to1_out_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_rzh1n2,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = RZH1N2TO1_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = RZH1N2TO1_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_RZH1N2TO_2_OUT,
		.name = "rzh1n2to 2",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_RZH1N2TO_2,
		.fmts = rzh1n2to2_out_fmts,
		.num_fmts = ARRAY_SIZE(rzh1n2to2_out_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_rzh1n2,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = RZH1N2TO2_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = RZH1N2TO2_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_RZH1N2TO_3_OUT,
		.name = "rzh1n2to 3",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_RZH1N2TO_3,
		.fmts = rzh1n2to3_out_fmts,
		.num_fmts = ARRAY_SIZE(rzh1n2to3_out_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_rzh1n2,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = RZH1N2TO3_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = RZH1N2TO3_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_MAIN_STREAM_SV_1_OUT,
		.name = "sv imgo 1",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_CAMSV_MAIN_OUT,
		.fmts = stream_out_fmts,
		.num_fmts = ARRAY_SIZE(stream_out_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_default,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = IMG_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = IMG_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_MAIN_STREAM_SV_2_OUT,
		.name = "sv imgo 2",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_CAMSV_MAIN_OUT,
		.fmts = stream_out_fmts,
		.num_fmts = ARRAY_SIZE(stream_out_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_default,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = IMG_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = IMG_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_META_OUT_0,
		.name = "partial meta 0",
		.cap = V4L2_CAP_META_CAPTURE,
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = false,
		.smem_alloc = false,
		.need_cache_sync_on_finish = true,
		.dma_port = MTKCAM_IPI_RAW_META_STATS_0,
		.default_fmt_idx = 1,
		.max_buf_count = 16,
		.ioctl_ops = &mtk_cam_v4l2_meta_cap_ioctl_ops,
	},
	{
		.id = MTK_RAW_META_OUT_1,
		.name = "partial meta 1",
		.cap = V4L2_CAP_META_CAPTURE,
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = false,
		.smem_alloc = false,
		.need_cache_sync_on_finish = true,
		.dma_port = MTKCAM_IPI_RAW_META_STATS_1,
		.default_fmt_idx = 2,
		.max_buf_count = 16,
		.ioctl_ops = &mtk_cam_v4l2_meta_cap_ioctl_ops,
	},
	{
		.id = MTK_RAW_META_OUT_2,
		.name = "partial meta 2",
		.cap = V4L2_CAP_META_CAPTURE,
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = false,
		.smem_alloc = false,
		.need_cache_sync_on_finish = true,
		.dma_port = MTKCAM_IPI_RAW_META_STATS_2,
		.default_fmt_idx = 3,
		.max_buf_count = 16,
		.ioctl_ops = &mtk_cam_v4l2_meta_cap_ioctl_ops,
	},
	{
		.id = MTK_RAW_META_SV_OUT_0,
		.name = "external meta 0",
		.cap = V4L2_CAP_META_CAPTURE,
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = false,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_CAMSV_MAIN_OUT,
		.default_fmt_idx = 4,
		.max_buf_count = 16,
		.ioctl_ops = &mtk_cam_v4l2_meta_cap_ioctl_ops,
	},
	{
		.id = MTK_RAW_META_SV_OUT_1,
		.name = "external meta 1",
		.cap = V4L2_CAP_META_CAPTURE,
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = false,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_CAMSV_MAIN_OUT,
		.default_fmt_idx = 4,
		.max_buf_count = 16,
		.ioctl_ops = &mtk_cam_v4l2_meta_cap_ioctl_ops,
	},
	{
		.id = MTK_RAW_META_SV_OUT_2,
		.name = "external meta 2",
		.cap = V4L2_CAP_META_CAPTURE,
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = false,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_CAMSV_MAIN_OUT,
		.default_fmt_idx = 4,
		.max_buf_count = 16,
		.ioctl_ops = &mtk_cam_v4l2_meta_cap_ioctl_ops,
	},
};

static const char *capture_queue_names[RAW_PIPELINE_NUM][MTK_RAW_TOTAL_CAPTURE_QUEUES] = {
	{"mtk-cam raw-0 main-stream",
	 "mtk-cam raw-0 yuvo-1", "mtk-cam raw-0 yuvo-2",
	 "mtk-cam raw-0 yuvo-3", "mtk-cam raw-0 yuvo-4",
	 "mtk-cam raw-0 yuvo-5",
	 "mtk-cam raw-0 drzs4no-1", "mtk-cam raw-0 drzs4no-2", "mtk-cam raw-0 drzs4no-3",
	 "mtk-cam raw-0 rzh1n2to-1", "mtk-cam raw-0 rzh1n2to-2", "mtk-cam raw-0 rzh1n2to-3",
	 "mtk-cam raw-0 sv-imgo-1", "mtk-cam raw-0 sv-imgo-2",
	 "mtk-cam raw-0 partial-meta-0", "mtk-cam raw-0 partial-meta-1",
	 "mtk-cam raw-0 partial-meta-2",
	 "mtk-cam raw-0 ext-meta-0", "mtk-cam raw-0 ext-meta-1",
	 "mtk-cam raw-0 ext-meta-2"},

	{"mtk-cam raw-1 main-stream",
	 "mtk-cam raw-1 yuvo-1", "mtk-cam raw-1 yuvo-2",
	 "mtk-cam raw-1 yuvo-3", "mtk-cam raw-1 yuvo-4",
	 "mtk-cam raw-1 yuvo-5",
	 "mtk-cam raw-1 drzs4no-1", "mtk-cam raw-1 drzs4no-2", "mtk-cam raw-1 drzs4no-3",
	 "mtk-cam raw-1 rzh1n2to-1", "mtk-cam raw-1 rzh1n2to-2", "mtk-cam raw-1 rzh1n2to-3",
	 "mtk-cam raw-1 sv-imgo-1", "mtk-cam raw-1 sv-imgo-2",
	 "mtk-cam raw-1 partial-meta-0", "mtk-cam raw-1 partial-meta-1",
	 "mtk-cam raw-1 partial-meta-2",
	 "mtk-cam raw-1 ext-meta-0", "mtk-cam raw-1 ext-meta-1",
	 "mtk-cam raw-1 ext-meta-2"},

	{"mtk-cam raw-2 main-stream",
	 "mtk-cam raw-2 yuvo-1", "mtk-cam raw-2 yuvo-2",
	 "mtk-cam raw-2 yuvo-3", "mtk-cam raw-2 yuvo-4",
	 "mtk-cam raw-2 yuvo-5",
	 "mtk-cam raw-2 drzs4no-1", "mtk-cam raw-2 drzs4no-2", "mtk-cam raw-2 drzs4no-3",
	 "mtk-cam raw-2 rzh1n2to-1", "mtk-cam raw-2 rzh1n2to-2", "mtk-cam raw-2 rzh1n2to-3",
	 "mtk-cam raw-2 sv-imgo-1", "mtk-cam raw-2 sv-imgo-2",
	 "mtk-cam raw-2 partial-meta-0", "mtk-cam raw-2 partial-meta-1",
	 "mtk-cam raw-2 partial-meta-2",
	 "mtk-cam raw-2 ext-meta-0", "mtk-cam raw-2 ext-meta-1",
	 "mtk-cam raw-2 ext-meta-2"},
};

/* The helper to configure the device context */
static void mtk_raw_pipeline_queue_setup(struct mtk_raw_pipeline *pipe)
{
	unsigned int node_idx, i;

	if (WARN_ON(MTK_RAW_TOTAL_OUTPUT_QUEUES + MTK_RAW_TOTAL_CAPTURE_QUEUES
	    != MTK_RAW_TOTAL_NODES))
		return;

	node_idx = 0;

	/* Setup the output queue */
	for (i = 0; i < MTK_RAW_TOTAL_OUTPUT_QUEUES; i++) {
		pipe->vdev_nodes[node_idx].desc = output_queues[i];
		if (pipe->vdev_nodes[node_idx].desc.id == MTK_RAW_META_IN)
			pipe->vdev_nodes[node_idx].desc.fmts = mtk_cam_get_meta_fmts();
		pipe->vdev_nodes[node_idx++].desc.name =
			output_queue_names[pipe->id][i];
	}

	/* Setup the capture queue */
	for (i = 0; i < MTK_RAW_TOTAL_CAPTURE_QUEUES; i++) {
		pipe->vdev_nodes[node_idx].desc = capture_queues[i];
		if (pipe->vdev_nodes[node_idx].desc.id >= MTK_RAW_META_OUT_BEGIN &&
			pipe->vdev_nodes[node_idx].desc.id <= MTK_RAW_META_SV_OUT_2)
			pipe->vdev_nodes[node_idx].desc.fmts = mtk_cam_get_meta_fmts();
		pipe->vdev_nodes[node_idx++].desc.name =
			capture_queue_names[pipe->id][i];
	}
}

static void mtk_raw_pipeline_ctrl_setup(struct mtk_raw_pipeline *pipe)
{
	struct v4l2_ctrl_handler *ctrl_hdlr;
	struct v4l2_ctrl *ctrl;
	struct device *dev = pipe->raw->devs[pipe->id];
	int ret = 0;

	ctrl_hdlr = &pipe->ctrl_handler;
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 5);
	if (ret) {
		dev_info(dev, "v4l2_ctrl_handler init failed\n");
		return;
	}
	mutex_init(&pipe->res_config.resource_lock);
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &hwn_limit, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			       V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &frz_limit, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			       V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &bin_limit, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			       V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &hwn, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &frz, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &bin, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &hsf, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &hwn_try, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &frz_try, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &bin_try, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &frame_sync_id, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			       V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &raw_path, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			       V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;

	// PDE
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &cfg_pde_info, NULL);

	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &mtk_feature, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;

	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &cfg_res_update, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;

	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &cfg_res_ctrl, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;

	/* TG flash ctrls */
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &mtk_cam_tg_flash_enable, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;

	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr,
		&mtk_camsys_hw_mode, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;

	v4l2_ctrl_new_custom(ctrl_hdlr, &mstream_exposure, NULL);
	pipe->res_config.hwn_limit_max = hwn_limit.def;
	pipe->res_config.frz_limit = frz_limit.def;
	pipe->res_config.bin_limit = bin_limit.def;
	pipe->res_config.res_plan = 1;
	pipe->feature_pending = mtk_feature.def;
	pipe->sync_id = frame_sync_id.def;
	pipe->sensor_mode_update = cfg_res_update.def;
	pipe->pde_config.pde_info.pdo_max_size = cfg_pde_info.def;
	pipe->pde_config.pde_info.pdi_max_size = cfg_pde_info.def;
	pipe->pde_config.pde_info.pd_table_offset = cfg_pde_info.def;
	pipe->subdev.ctrl_handler = ctrl_hdlr;
	pipe->hw_mode = mtk_camsys_hw_mode.def;
	pipe->hw_mode_pending = mtk_camsys_hw_mode.def;
}

static int mtk_raw_pipeline_register(unsigned int id, struct device *dev,
				     struct mtk_raw_pipeline *pipe,
			       struct v4l2_device *v4l2_dev)
{
	struct mtk_cam_device *cam = dev_get_drvdata(pipe->raw->cam_dev);
	struct v4l2_subdev *sd = &pipe->subdev;
	struct mtk_cam_video_device *video;
	unsigned int i;
	int ret;

	pipe->id = id;
	pipe->dynamic_exposure_num_max = 3;

	/* Initialize subdev */
	v4l2_subdev_init(sd, &mtk_raw_subdev_ops);
	sd->entity.function = MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER;
	sd->entity.ops = &mtk_cam_media_entity_ops;
	sd->flags = V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	snprintf(sd->name, sizeof(sd->name),
		 "%s-%d", dev_driver_string(dev), pipe->id);
	v4l2_set_subdevdata(sd, pipe);
	mtk_raw_pipeline_ctrl_setup(pipe);
	dev_info(dev, "%s: %s\n", __func__, sd->name);

	ret = v4l2_device_register_subdev(v4l2_dev, sd);
	if (ret < 0) {
		dev_info(dev, "Failed to register subdev: %d\n", ret);
		return ret;
	}

	mtk_raw_pipeline_queue_setup(pipe);
	/* setup pads of raw pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe->pads); i++) {
		pipe->pads[i].flags = i < MTK_RAW_SOURCE_BEGIN ?
			MEDIA_PAD_FL_SINK : MEDIA_PAD_FL_SOURCE;
	}

	media_entity_pads_init(&sd->entity, ARRAY_SIZE(pipe->pads), pipe->pads);

	/* setup video node */
	for (i = 0; i < ARRAY_SIZE(pipe->vdev_nodes); i++) {
		video = pipe->vdev_nodes + i;

		switch (pipe->id) {
		case MTKCAM_SUBDEV_RAW_0:
		case MTKCAM_SUBDEV_RAW_1:
		case MTKCAM_SUBDEV_RAW_2:
			video->uid.pipe_id = pipe->id;
			break;
		default:
			dev_dbg(dev, "invalid pipe id\n");
			return -EINVAL;
		}

		video->uid.id = video->desc.dma_port;
		video->ctx = &cam->ctxs[id];
		ret = mtk_cam_video_register(video, v4l2_dev);
		if (ret)
			goto fail_unregister_video;

		if (V4L2_TYPE_IS_OUTPUT(video->desc.buf_type))
			ret = media_create_pad_link(&video->vdev.entity, 0,
						    &sd->entity,
						    video->desc.id,
						    video->desc.link_flags);
		else
			ret = media_create_pad_link(&sd->entity,
						    video->desc.id,
						    &video->vdev.entity, 0,
						    video->desc.link_flags);

		if (ret)
			goto fail_unregister_video;
	}

	return 0;

fail_unregister_video:
	for (i = i - 1; i >= 0; i--)
		mtk_cam_video_unregister(pipe->vdev_nodes + i);

	return ret;
}

static void mtk_raw_pipeline_unregister(struct mtk_raw_pipeline *pipe)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(pipe->vdev_nodes); i++)
		mtk_cam_video_unregister(pipe->vdev_nodes + i);
	v4l2_ctrl_handler_free(&pipe->ctrl_handler);
	mutex_destroy(&pipe->res_config.resource_lock);
	v4l2_device_unregister_subdev(&pipe->subdev);
	media_entity_cleanup(&pipe->subdev.entity);
}

int mtk_raw_setup_dependencies(struct mtk_raw *raw)
{
	struct device *dev = raw->cam_dev;
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	struct device *consumer, *supplier;
	struct device_link *link;
	struct mtk_raw_device *raw_dev;
	struct mtk_yuv_device *yuv_dev;
	int i;

	for (i = 0; i < cam_dev->num_raw_drivers; i++) {
		consumer = raw->devs[i];
		supplier = raw->yuvs[i];
		if (!consumer || !supplier) {
			dev_info(dev, "failed to get raw/yuv dev for id %d\n", i);
			continue;
		}

		raw_dev = dev_get_drvdata(consumer);
		yuv_dev = dev_get_drvdata(supplier);
		raw_dev->yuv_base = yuv_dev->base;
		raw_dev->yuv_base_inner = yuv_dev->base_inner;

		link = device_link_add(consumer, supplier,
				       DL_FLAG_AUTOREMOVE_CONSUMER |
				       DL_FLAG_PM_RUNTIME);
		if (!link) {
			dev_info(dev, "Unable to create link between %s and %s\n",
				 dev_name(consumer), dev_name(supplier));
			return -ENODEV;
		}
	}

	return 0;
}

int mtk_raw_register_entities(struct mtk_raw *raw, struct v4l2_device *v4l2_dev)
{
	struct device *dev = raw->cam_dev;
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	unsigned int i;
	int ret;

	for (i = 0; i < cam_dev->num_raw_drivers; i++) {
		struct mtk_raw_pipeline *pipe = raw->pipelines + i;

		pipe->raw = raw;
		memset(pipe->cfg, 0, sizeof(*pipe->cfg));
		ret = mtk_raw_pipeline_register(MTKCAM_SUBDEV_RAW_0 + i,
						raw->devs[i],
						raw->pipelines + i, v4l2_dev);
		if (ret)
			return ret;
	}
	return 0;
}

void mtk_raw_unregister_entities(struct mtk_raw *raw)
{
	struct device *dev = raw->cam_dev;
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	unsigned int i;

	for (i = 0; i < cam_dev->num_raw_drivers; i++)
		mtk_raw_pipeline_unregister(raw->pipelines + i);
}
