// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/pm_opp.h>

#include "mtk-interconnect.h"
#include "mtk_cam.h"
#include "mtk_cam-feature.h"
#include "mtk_cam-dvfs_qos.h"
#include "soc/mediatek/mmqos.h"

static unsigned int debug_mmqos;
module_param(debug_mmqos, uint, 0644);
MODULE_PARM_DESC(debug_mmqos, "activates debug mmqos");

enum sv_qos_port_id {
	sv_cqi = 0,
	sv_imgo,
	sv_qos_port_num
};

enum mraw_qos_port_id {
	mraw_cqi_m1 = 0,
	mraw_imgo,
	mraw_imgbo,
	mraw_qos_port_num
};

struct mraw_mmqos {
	char *port[mraw_qos_port_num];
};

struct sv_mmqos {
	char *port[sv_qos_port_num];
};
/* to be removed */
__maybe_unused
static struct raw_mmqos _raw_qos = {
	.port = {
		"cqi_r1",
		"rawi_r2",
		"rawi_r3",
		"rawi_r5",
		"imgo_r1",
		"bpci_r1",
		"lsci_r1",
		"ufeo_r1",
		"ltmso_r1",
		"drzb2no_r1",
		"aao_r1",
		"afo_r1",
		"yuvo_r1",
		"yuvo_r3",
		"yuvo_r2",
		"yuvo_r5",
		"rgbwi_r1",
		"tcyso_r1",
		"drz4no_r3"
	}
};

static struct sv_mmqos sv_qos[] = {
	[CAMSV_0] = {
		.port = {
			"l14_cqi_a",
			"l14_imgo_a"
		},
	},
	[CAMSV_1] = {
		.port = {
			"l13_cqi_b",
			"l13_imgo_b"
		},
	},
	[CAMSV_2] = {
		.port = {
			"l29_cqi_c",
			"l29_imgo_c"
		},
	},
	[CAMSV_3] = {
		.port = {
			"l29_cqi_d",
			"l29_imgo_d"
		},
	},
	[CAMSV_4] = {
		.port = {
			"l29_cqi_e",
			"l29_imgo_e"
		},
	},
	[CAMSV_5] = {
		.port = {
			"l29_cqi_f",
			"l29_imgo_f"
		},
	},
};

static struct mraw_mmqos mraw_qos[] = {
	[MRAW_0] = {
		.port = {
			"l25_cqi_m1_0",
			"l25_imgo_m1_0",
			"l25_imgbo_m1_0"
		},
	},
	[MRAW_1] = {
		.port = {
			"l26_cqi_m1_1",
			"l26_imgo_m1_1",
			"l26_imgbo_m1_1"
		},
	},
	[MRAW_2] = {
		.port = {
			"l25_cqi_m1_2",
			"l25_imgo_m1_2",
			"l25_imgbo_m1_2"
		},
	},
	[MRAW_3] = {
		.port = {
			"l26_cqi_m1_3",
			"l26_imgo_m1_3",
			"l26_imgbo_m1_3"
		},
	},
};

static void mtk_cam_dvfs_enumget_clktarget(struct mtk_cam_device *cam)
{
	struct mtk_camsys_dvfs *dvfs = &cam->camsys_ctrl.dvfs_info;
	int clk_streaming_max = dvfs->clklv[0];
	int i;

	for (i = MTKCAM_SUBDEV_RAW_START;  i < MTKCAM_SUBDEV_RAW_END; i++) {
		if (cam->ctxs[i].streaming && cam->ctxs[i].pipe) {
			struct mtk_cam_resource_config *res =
				&cam->ctxs[i].pipe->res_config;
			if (clk_streaming_max < res->clk_target)
				clk_streaming_max = res->clk_target;
			dev_dbg(cam->dev, "on ctx:%d clk needed:%d", i, res->clk_target);
		} else {
			dev_dbg(cam->dev, "on ctx:%d not streaming or pipe null", i);
		}
	}
	dvfs->clklv_target = clk_streaming_max;
	dev_dbg(cam->dev, "[%s] dvfs->clk=%d", __func__, dvfs->clklv_target);
}

static void mtk_cam_dvfs_get_clkidx(struct mtk_cam_device *cam)
{
	struct mtk_camsys_dvfs *dvfs = &cam->camsys_ctrl.dvfs_info;

	u64 freq_cur = 0;
	int i;

	freq_cur = dvfs->clklv_target;
	for (i = 0; i < dvfs->clklv_num; i++) {
		if (freq_cur == dvfs->clklv[i])
			dvfs->clklv_idx = i;
	}
	dev_dbg(cam->dev, "[%s] get clk=%d, idx=%d",
		 __func__, dvfs->clklv_target, dvfs->clklv_idx);
}

void mtk_cam_dvfs_update_clk(struct mtk_cam_device *cam)
{
	struct mtk_camsys_dvfs *dvfs = &cam->camsys_ctrl.dvfs_info;
	int ret;

	if (dvfs->mmdvfs_clk && dvfs->clklv_num &&
		(atomic_read(&dvfs->fixed_clklv) == 0)) {
		mtk_cam_dvfs_enumget_clktarget(cam);
		mtk_cam_dvfs_get_clkidx(cam);
		ret = clk_set_rate(dvfs->mmdvfs_clk, dvfs->clklv_target);
		if (ret < 0) {
			dev_info(cam->dev, "[%s] clk set rate fail", __func__);
			return;
		}
		dev_info(cam->dev, "[%s] update idx:%d clk:%d volt:%d", __func__,
			dvfs->clklv_idx, dvfs->clklv_target, dvfs->voltlv[dvfs->clklv_idx]);
	}
}

/* for DC mode seamless switch to force clk level */
void mtk_cam_dvfs_force_clk(struct mtk_cam_device *cam, bool enable)
{
#define FORCE_CLK_LEVEL 4

	struct mtk_camsys_dvfs *dvfs = &cam->camsys_ctrl.dvfs_info;
	unsigned int clklv = dvfs->clklv[FORCE_CLK_LEVEL];
	int ret = 0;

	if (dvfs->mmdvfs_clk && dvfs->clklv_num) {
		if (enable) {
			atomic_set(&dvfs->fixed_clklv, clklv);
			ret = clk_set_rate(dvfs->mmdvfs_clk, clklv);
			dev_info(cam->dev, "[%s] force clk:%d volt:%d (ret:%d)",
				__func__, clklv, dvfs->voltlv[FORCE_CLK_LEVEL], ret);
		} else {
			if (atomic_cmpxchg(&dvfs->fixed_clklv, clklv, 0)) {
				mtk_cam_dvfs_update_clk(cam);
			}
		}
	}
}

void mtk_cam_dvfs_uninit(struct mtk_cam_device *cam)
{
	struct mtk_camsys_dvfs *dvfs_info = &cam->camsys_ctrl.dvfs_info;

	if (dvfs_info->clklv_num)
		dev_pm_opp_of_remove_table(dvfs_info->dev);
}

void mtk_cam_dvfs_init(struct mtk_cam_device *cam)
{
	struct mtk_camsys_dvfs *dvfs_info = &cam->camsys_ctrl.dvfs_info;
	struct dev_pm_opp *opp;
	unsigned long freq = 0;
	int ret = 0, clk_num = 0, i = 0;
	struct device *dev = cam->dev;

	memset((void *)dvfs_info, 0x0, sizeof(struct mtk_camsys_dvfs));
	dvfs_info->dev = cam->dev;

	/* mmqos initialize */
	mtk_cam_qos_init(cam);

	ret = dev_pm_opp_of_add_table(dvfs_info->dev);
	if (ret < 0) {
		dev_info(dvfs_info->dev, "fail to init opp table: %d\n", ret);
		goto opp_default_table;
	}

	clk_num = dev_pm_opp_get_opp_count(dvfs_info->dev);
	while (!IS_ERR(opp = dev_pm_opp_find_freq_ceil(dvfs_info->dev, &freq))) {
		dvfs_info->clklv[i] = freq;
		dvfs_info->voltlv[i] = dev_pm_opp_get_voltage(opp);
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}
	dvfs_info->clklv_num = clk_num;
	dvfs_info->clklv_target = dvfs_info->clklv[0];
	dvfs_info->clklv_idx = 0;
	atomic_set(&dvfs_info->fixed_clklv, 0);
	for (i = 0; i < dvfs_info->clklv_num; i++) {
		dev_info(cam->dev, "[%s] idx=%d, clk=%d volt=%d\n",
			 __func__, i, dvfs_info->clklv[i], dvfs_info->voltlv[i]);
	}

	dvfs_info->mmdvfs_clk = devm_clk_get(dev, "mmdvfs_clk");
	if (IS_ERR(dvfs_info->mmdvfs_clk)) {
		dvfs_info->mmdvfs_clk = NULL;
		dev_info(dvfs_info->dev, "can't get mmdvfs_clk\n");
	}

	return;

opp_default_table:
	/* TODO: need to get from CCF or others api */
	dvfs_info->voltlv[0] = 650000;
	dvfs_info->clklv[0] = 546000000;
	dvfs_info->clklv_target = 546000000;
	dvfs_info->clklv_num = 1;

}

#define MTK_CAM_QOS_LSCI_TABLE_MAX_SIZE (32768)
#define MTK_CAM_QOS_CACI_TABLE_MAX_SIZE (32768)
#define BW_B2KB(value) ((value) / 1024)
#define BW_B2KB_WITH_RATIO(value) ((value) * 5 / 4 / 1024)

/* Watch out there is a mutex lock lying in sensor g_frame_interval */
static void __mtk_cam_qos_bw_calc(struct mtk_cam_ctx *ctx, struct mtk_raw_device *raw_dev,
						 unsigned long raw_dmas, bool force)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_camsys_dvfs *dvfs_info = &cam->camsys_ctrl.dvfs_info;
	struct mtk_cam_video_device *vdev;
	struct mtk_raw_pipeline *pipe = ctx->pipe;
	struct v4l2_subdev_frame_interval fi;
	struct v4l2_subdev_format sd_fmt;
	struct v4l2_ctrl *ctrl;
	struct mraw_mmqos *mraw_mmqos;
	unsigned int ipi_video_id, ipi_mraw_video_id;
	int engine_id = raw_dev->id;
	unsigned int qos_port_id, port_id;
	unsigned int ipi_fmt;
	int i, j, pixel_bits, plane_factor;
	unsigned long vblank = 0, fps, height, PBW_MB_s = 0, ABW_MB_s = 0;
	unsigned int width_mbn = 0, height_mbn = 0;
	unsigned int width_cpi = 0, height_cpi = 0;
	bool is_raw_srt = mtk_cam_is_srt(pipe->hw_mode);
	struct raw_mmqos *raw_qos = NULL;

	CALL_PLAT_V4L2(get_mmqos_port, &raw_qos);

	memset(&sd_fmt, 0, sizeof(sd_fmt));
	memset(&fi, 0, sizeof(fi));
	if (force == true)
		dvfs_info->updated_raw_dmas[engine_id] = 0;
	else
		raw_dmas |= dvfs_info->updated_raw_dmas[engine_id];

	/* mstream may no settings */
	if (raw_dmas == 0 || dvfs_info->updated_raw_dmas[engine_id] == raw_dmas)
		return;

	dev_info(cam->dev, "[%s] force(%d), engine_id(%d) enabled_raw(%d) enable_dmas(0x%lx) raw_dmas(0x%lx), updated_raw_dmas(0x%lx)\n",
		__func__, force, engine_id,
		ctx->pipe->enabled_raw, pipe->enabled_dmas, raw_dmas,
		dvfs_info->updated_raw_dmas[engine_id]);

	dvfs_info->updated_raw_dmas[engine_id] = raw_dmas;

	if (ctx->sensor) {
		fi.pad = 0;
		fi.reserved[0] = V4L2_SUBDEV_FORMAT_ACTIVE;
		v4l2_subdev_call(ctx->sensor, video, g_frame_interval, &fi);
		fps = fi.interval.denominator / fi.interval.numerator;
		pipe->res_config.interval.denominator = fi.interval.denominator;
		pipe->res_config.interval.numerator = fi.interval.numerator;
		ctrl = v4l2_ctrl_find(ctx->sensor->ctrl_handler, V4L2_CID_VBLANK);
		if (!ctrl)
			dev_info(cam->dev, "[%s] ctrl is NULL\n", __func__);
		else
			vblank = v4l2_ctrl_g_ctrl(ctrl);
		sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		sd_fmt.pad = PAD_SRC_RAW0;
		v4l2_subdev_call(ctx->seninf, pad, get_fmt, NULL, &sd_fmt);
		height = sd_fmt.format.height;
		dev_info(cam->dev, "[%s] FPS:%lu/%lu:%lu, H:%lu, VB:%lu\n",
			 __func__, fi.interval.denominator, fi.interval.numerator,
			 fps, height, vblank);
	}

	/* clear raw qos */
	for (i = 0; i < raw_qos_port_num; i++) {
		qos_port_id = engine_id * raw_qos_port_num + i;
		dvfs_info->qos_bw_avg[qos_port_id] = 0;
		dvfs_info->qos_bw_peak[qos_port_id] = 0;
	}

	for (i = 0; i < MTKCAM_IPI_RAW_ID_MAX; i++) {
		if (raw_dmas & 1ULL<<i)
			ipi_video_id = i;
		else
			continue;
		/* update buffer internal address */
		switch (ipi_video_id) {
		case MTKCAM_IPI_RAW_IMGO:
			// imgo + fho (fho almost zero)
			qos_port_id = engine_id * raw_qos_port_num + imgo_r1;
			vdev = &pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
			ipi_fmt = mtk_cam_get_img_fmt(vdev->active_fmt.fmt.pix_mp.pixelformat);
			pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
			plane_factor = mtk_cam_get_fmt_size_factor(ipi_fmt);
			PBW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
						(vblank + height) * pixel_bits *
						plane_factor / 8 / 100;
			ABW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
						vdev->active_fmt.fmt.pix_mp.height * pixel_bits *
						plane_factor / 8 / 100;
			dvfs_info->qos_bw_peak[qos_port_id] = PBW_MB_s;
			dvfs_info->qos_bw_avg[qos_port_id] = ABW_MB_s;
			// remove me
			// qos_port_id = engine_id * raw_qos_port_num + rgbwi_r1;
			// dvfs_info->qos_bw_peak[qos_port_id] = PBW_MB_s;
			// dvfs_info->qos_bw_avg[qos_port_id] = ABW_MB_s;
			if (unlikely(debug_mmqos))
				dev_info(cam->dev, "[%16s] qos_idx:%2d video_id/ipifmt/bits/plane/w/h : %2d/%2d/%2d/%d/%5d/%5d BW(B/s)(avg:%lu,peak:%lu)\n",
				  raw_qos->port[imgo_r1],
				  qos_port_id, ipi_video_id, ipi_fmt,
				  pixel_bits, plane_factor,
				  vdev->active_fmt.fmt.pix_mp.width,
				  vdev->active_fmt.fmt.pix_mp.height,
				  ABW_MB_s, PBW_MB_s);
			break;
		case MTKCAM_IPI_RAW_YUVO_1:
			qos_port_id = engine_id * raw_qos_port_num + yuvo_r1;
			vdev = &pipe->vdev_nodes[MTK_RAW_YUVO_1_OUT - MTK_RAW_SINK_NUM];
			ipi_fmt = mtk_cam_get_img_fmt(vdev->active_fmt.fmt.pix_mp.pixelformat);
			pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
			plane_factor = mtk_cam_get_fmt_size_factor(ipi_fmt);
			PBW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
				(vblank + height) * pixel_bits *
				plane_factor / 8 / 100;
			if (is_yuv_ufo(vdev->active_fmt.fmt.pix_mp.pixelformat)) {
				//bitstream: yuvo + yuvbo apply compression ratio 0.7
				ABW_MB_s = (vdev->active_fmt.fmt.pix_mp.width * fps *
					vdev->active_fmt.fmt.pix_mp.height * pixel_bits *
					plane_factor / 8 / 100) * 7 / 10;
				//table: yuvco + yuvdo
				ABW_MB_s += DIV_ROUND_UP(vdev->active_fmt.fmt.pix_mp.width, 64) *
					fps * vdev->active_fmt.fmt.pix_mp.height *
					plane_factor / 100;
			} else {
				ABW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
					vdev->active_fmt.fmt.pix_mp.height * pixel_bits *
					plane_factor / 8 / 100;
			}
			dvfs_info->qos_bw_peak[qos_port_id] = PBW_MB_s;
			dvfs_info->qos_bw_avg[qos_port_id] = ABW_MB_s;
			if (unlikely(debug_mmqos))
				dev_info(cam->dev, "[%16s] qos_idx:%2d video_id/ipifmt/bits/plane/w/h : %2d/%2d/%2d/%d/%5d/%5d BW(B/s)(avg:%lu,peak:%lu)\n",
				  raw_qos->port[yuvo_r1],
				  qos_port_id, ipi_video_id, ipi_fmt,
				  pixel_bits, plane_factor,
				  vdev->active_fmt.fmt.pix_mp.width,
				  vdev->active_fmt.fmt.pix_mp.height,
				  ABW_MB_s, PBW_MB_s);
			break;
		case MTKCAM_IPI_RAW_YUVO_3:
			qos_port_id = engine_id * raw_qos_port_num + yuvo_r3;
			vdev = &pipe->vdev_nodes[MTK_RAW_YUVO_3_OUT - MTK_RAW_SINK_NUM];
			ipi_fmt = mtk_cam_get_img_fmt(vdev->active_fmt.fmt.pix_mp.pixelformat);
			pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
			plane_factor = mtk_cam_get_fmt_size_factor(ipi_fmt);
			PBW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
				(vblank + height) * pixel_bits *
				plane_factor / 8 / 100;
			if (is_yuv_ufo(vdev->active_fmt.fmt.pix_mp.pixelformat)) {
				//bitstream: yuvo + yuvbo apply compression ratio 0.7
				ABW_MB_s = (vdev->active_fmt.fmt.pix_mp.width * fps *
					vdev->active_fmt.fmt.pix_mp.height * pixel_bits *
					plane_factor / 8 / 100) * 7 / 10;
				//table: yuvco + yuvdo
				ABW_MB_s += DIV_ROUND_UP(vdev->active_fmt.fmt.pix_mp.width, 64) *
					fps * vdev->active_fmt.fmt.pix_mp.height *
					plane_factor / 100;
			} else {
				ABW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
					vdev->active_fmt.fmt.pix_mp.height * pixel_bits *
					plane_factor / 8 / 100;
			}
			dvfs_info->qos_bw_peak[qos_port_id] = PBW_MB_s;
			dvfs_info->qos_bw_avg[qos_port_id] = ABW_MB_s;

			if (unlikely(debug_mmqos))
				dev_info(cam->dev, "[%16s] qos_idx:%2d video_id/ipifmt/bits/plane/w/h : %2d/%2d/%2d/%d/%5d/%5d BW(B/s)(avg:%lu,peak:%lu)\n",
				  raw_qos->port[yuvo_r3],
				  qos_port_id, ipi_video_id, ipi_fmt,
				  pixel_bits, plane_factor,
				  vdev->active_fmt.fmt.pix_mp.width,
				  vdev->active_fmt.fmt.pix_mp.height,
				  ABW_MB_s, PBW_MB_s);
			break;
		case MTKCAM_IPI_RAW_YUVO_2:
		case MTKCAM_IPI_RAW_YUVO_4:
			qos_port_id = engine_id * raw_qos_port_num + yuvo_r2;
			if (ipi_video_id == MTKCAM_IPI_RAW_YUVO_2)
				vdev = &pipe->vdev_nodes[MTK_RAW_YUVO_2_OUT - MTK_RAW_SINK_NUM];
			else if (ipi_video_id == MTKCAM_IPI_RAW_YUVO_4)
				vdev = &pipe->vdev_nodes[MTK_RAW_YUVO_4_OUT - MTK_RAW_SINK_NUM];

			ipi_fmt = mtk_cam_get_img_fmt(vdev->active_fmt.fmt.pix_mp.pixelformat);
			pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
			plane_factor = mtk_cam_get_fmt_size_factor(ipi_fmt);
			PBW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
						(vblank + height) * pixel_bits *
						plane_factor / 8 / 100;
			ABW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
						vdev->active_fmt.fmt.pix_mp.height * pixel_bits *
						plane_factor / 8 / 100;
			dvfs_info->qos_bw_peak[qos_port_id] += PBW_MB_s;
			dvfs_info->qos_bw_avg[qos_port_id] += ABW_MB_s;

			if (unlikely(debug_mmqos))
				dev_info(cam->dev, "[%16s] qos_idx:%2d video_id/ipifmt/bits/plane/w/h : %2d/%2d/%2d/%d/%5d/%5d BW(B/s)(avg:%lu,peak:%lu)\n",
				  raw_qos->port[yuvo_r2],
				  qos_port_id, ipi_video_id, ipi_fmt,
				  pixel_bits, plane_factor,
				  vdev->active_fmt.fmt.pix_mp.width,
				  vdev->active_fmt.fmt.pix_mp.height,
				  ABW_MB_s, PBW_MB_s);
			break;
		case MTKCAM_IPI_RAW_YUVO_5:
			qos_port_id = engine_id * raw_qos_port_num + yuvo_r5;
			vdev = &pipe->vdev_nodes[MTK_RAW_YUVO_5_OUT - MTK_RAW_SINK_NUM];
			ipi_fmt = mtk_cam_get_img_fmt(vdev->active_fmt.fmt.pix_mp.pixelformat);
			pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
			plane_factor = mtk_cam_get_fmt_size_factor(ipi_fmt);
			PBW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
						(vblank + height) * pixel_bits *
						plane_factor / 8 / 100;
			ABW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
						vdev->active_fmt.fmt.pix_mp.height * pixel_bits *
						plane_factor / 8 / 100;
			dvfs_info->qos_bw_peak[qos_port_id] = PBW_MB_s;
			dvfs_info->qos_bw_avg[qos_port_id] = ABW_MB_s;

			if (unlikely(debug_mmqos))
				dev_info(cam->dev, "[%16s] qos_idx:%2d video_id/ipifmt/bits/plane/w/h : %2d/%2d/%2d/%d/%5d/%5d BW(B/s)(avg:%lu,peak:%lu)\n",
				  raw_qos->port[yuvo_r5],
				  qos_port_id, ipi_video_id, ipi_fmt,
				  pixel_bits, plane_factor,
				  vdev->active_fmt.fmt.pix_mp.width,
				  vdev->active_fmt.fmt.pix_mp.height,
				  ABW_MB_s, PBW_MB_s);
			break;
		case MTKCAM_IPI_RAW_RZH1N2TO_3:
		case MTKCAM_IPI_RAW_RZH1N2TO_1:
			if (ipi_video_id == MTKCAM_IPI_RAW_RZH1N2TO_3)
				vdev = &pipe->vdev_nodes[MTK_RAW_RZH1N2TO_3_OUT - MTK_RAW_SINK_NUM];
			else if (ipi_video_id == MTKCAM_IPI_RAW_RZH1N2TO_1)
				vdev = &pipe->vdev_nodes[MTK_RAW_RZH1N2TO_1_OUT - MTK_RAW_SINK_NUM];

			ipi_fmt = mtk_cam_get_img_fmt(vdev->active_fmt.fmt.pix_mp.pixelformat);
			pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
			plane_factor = mtk_cam_get_fmt_size_factor(ipi_fmt);

			//1 plane
			PBW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
						(vblank + height) * pixel_bits / 8;
			ABW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
						vdev->active_fmt.fmt.pix_mp.height *
						pixel_bits / 8;
			dvfs_info->qos_bw_peak[engine_id*raw_qos_port_num + drz4no_r3] += PBW_MB_s;
			dvfs_info->qos_bw_avg[engine_id*raw_qos_port_num + drz4no_r3] += ABW_MB_s;
			if (unlikely(debug_mmqos))
				dev_info(cam->dev, "[%16s] qos_idx:%2d video_id/ipifmt/bits/plane/w/h : %2d/%2d/%2d/%d/%5d/%5d BW(B/s)(avg:%lu,peak:%lu)\n",
				  raw_qos->port[drz4no_r3],
				  qos_port_id, ipi_video_id, ipi_fmt,
				  pixel_bits, plane_factor,
				  vdev->active_fmt.fmt.pix_mp.width,
				  vdev->active_fmt.fmt.pix_mp.height,
				  ABW_MB_s, PBW_MB_s);

			//2 plane
			PBW_MB_s = (vdev->active_fmt.fmt.pix_mp.width * fps *
						(vblank + height) * pixel_bits *
						plane_factor / 8 / 100) - PBW_MB_s;
			ABW_MB_s = (vdev->active_fmt.fmt.pix_mp.width * fps *
						vdev->active_fmt.fmt.pix_mp.height * pixel_bits *
						plane_factor / 8 / 100) - ABW_MB_s;
			dvfs_info->qos_bw_peak[engine_id * raw_qos_port_num + yuvo_r5] += PBW_MB_s;
			dvfs_info->qos_bw_avg[engine_id * raw_qos_port_num + yuvo_r5] += ABW_MB_s;
			if (unlikely(debug_mmqos))
				dev_info(cam->dev, "[%16s] qos_idx:%2d video_id/ipifmt/bits/plane/w/h : :%2d/%2d/%2d/%d/%5d/%5d BW(B/s)(avg:%lu,peak:%lu)\n",
				  raw_qos->port[yuvo_r5],
				  qos_port_id, ipi_video_id, ipi_fmt,
				  pixel_bits, plane_factor,
				  vdev->active_fmt.fmt.pix_mp.width,
				  vdev->active_fmt.fmt.pix_mp.height,
				  ABW_MB_s, PBW_MB_s);
			break;
		case MTKCAM_IPI_RAW_DRZS4NO_3:
			qos_port_id = engine_id * raw_qos_port_num + drz4no_r3;
			vdev = &pipe->vdev_nodes[MTK_RAW_DRZS4NO_3_OUT - MTK_RAW_SINK_NUM];
			ipi_fmt = mtk_cam_get_img_fmt(vdev->active_fmt.fmt.pix_mp.pixelformat);
			pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
			plane_factor = mtk_cam_get_fmt_size_factor(ipi_fmt);
			PBW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
						(vblank + height) * pixel_bits *
						plane_factor / 8 / 100;
			ABW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
						vdev->active_fmt.fmt.pix_mp.height * pixel_bits *
						plane_factor / 8 / 100;
			dvfs_info->qos_bw_peak[qos_port_id] += PBW_MB_s;
			dvfs_info->qos_bw_avg[qos_port_id] += ABW_MB_s;

			if (unlikely(debug_mmqos))
				dev_info(cam->dev, "[%16s] qos_idx:%2d video_id/ipifmt/bits/plane/w/h : %2d/%2d/%2d/%d/%5d/%5d BW(B/s)(avg:%lu,peak:%lu)\n",
				  raw_qos->port[drz4no_r3],
				  qos_port_id, ipi_video_id, ipi_fmt,
				  pixel_bits, plane_factor,
				  vdev->active_fmt.fmt.pix_mp.width,
				  vdev->active_fmt.fmt.pix_mp.height,
				  ABW_MB_s, PBW_MB_s);
			break;
		case MTKCAM_IPI_RAW_RZH1N2TO_2:
		case MTKCAM_IPI_RAW_DRZS4NO_1:
			/* tcyso_r1 : tcyso + rzh1n2to_r2 + drzs4no_r1 + drzh2no_r8 */
			if (ipi_video_id == MTKCAM_IPI_RAW_RZH1N2TO_2)
				vdev = &pipe->vdev_nodes[MTK_RAW_RZH1N2TO_2_OUT - MTK_RAW_SINK_NUM];
			else if (ipi_video_id == MTKCAM_IPI_RAW_DRZS4NO_1)
				vdev = &pipe->vdev_nodes[MTK_RAW_DRZS4NO_1_OUT - MTK_RAW_SINK_NUM];

			qos_port_id = engine_id * raw_qos_port_num + tcyso_r1;
			ipi_fmt = mtk_cam_get_img_fmt(vdev->active_fmt.fmt.pix_mp.pixelformat);
			pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
			plane_factor = mtk_cam_get_fmt_size_factor(ipi_fmt);
			PBW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
						(vblank + height) * pixel_bits *
						plane_factor / 8 / 100;
			ABW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
						vdev->active_fmt.fmt.pix_mp.height * pixel_bits *
						plane_factor / 8 / 100;
			dvfs_info->qos_bw_peak[qos_port_id] += PBW_MB_s;
			dvfs_info->qos_bw_avg[qos_port_id] += ABW_MB_s;

			if (unlikely(debug_mmqos))
				dev_info(cam->dev, "[%16s] qos_idx:%2d video_id/ipifmt/bits/plane/w/h : %2d/%2d/%2d/%d/%5d/%5d BW(B/s)(avg:%lu,peak:%lu)\n",
				  raw_qos->port[tcyso_r1],
				  qos_port_id, ipi_video_id, ipi_fmt,
				  pixel_bits, plane_factor,
				  vdev->active_fmt.fmt.pix_mp.width,
				  vdev->active_fmt.fmt.pix_mp.height,
				  ABW_MB_s, PBW_MB_s);
			break;
		case MTKCAM_IPI_RAW_DRZB2NO_1:
			qos_port_id = engine_id * raw_qos_port_num + drzb2no_r1;
			vdev = &pipe->vdev_nodes[MTK_RAW_DRZB2NO_1_OUT - MTK_RAW_SINK_NUM];
			ipi_fmt = mtk_cam_get_img_fmt(vdev->active_fmt.fmt.pix_mp.pixelformat);
			pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
			plane_factor = mtk_cam_get_fmt_size_factor(ipi_fmt);
			PBW_MB_s += vdev->active_fmt.fmt.pix_mp.width * fps *
						(vblank + height) * pixel_bits *
						plane_factor / 8 / 100;
			ABW_MB_s += vdev->active_fmt.fmt.pix_mp.width * fps *
						vdev->active_fmt.fmt.pix_mp.height * pixel_bits *
						plane_factor / 8 / 100;
			dvfs_info->qos_bw_peak[qos_port_id] = PBW_MB_s;
			dvfs_info->qos_bw_avg[qos_port_id] = ABW_MB_s;

			if (unlikely(debug_mmqos))
				dev_info(cam->dev, "[%16s] qos_idx:%2d video_id/ipifmt/bits/plane/w/h : %2d/%2d/%2d/%d/%5d/%5d BW(B/s)(avg:%lu,peak:%lu)\n",
				  raw_qos->port[drzb2no_r1],
				  qos_port_id, ipi_video_id, ipi_fmt,
				  pixel_bits, plane_factor,
				  vdev->active_fmt.fmt.pix_mp.width,
				  vdev->active_fmt.fmt.pix_mp.height,
				  ABW_MB_s, PBW_MB_s);
			break;
		case MTKCAM_IPI_RAW_META_STATS_CFG:
			/* main+sub cq desc size/virtual addr size */
			qos_port_id = engine_id * raw_qos_port_num + cqi_r1;
			PBW_MB_s = CQ_BUF_SIZE * fps;
			dvfs_info->qos_bw_avg[qos_port_id] =
				dvfs_info->qos_bw_peak[qos_port_id] = PBW_MB_s;
			if (unlikely(debug_mmqos))
				dev_info(cam->dev, "[%16s] qos_idx:%2d BW(B/s):%lu\n",
					raw_qos->port[cqi_r1], qos_port_id, PBW_MB_s);

			/* lsci_r1 = lsci + pdi + aai */
			// lsci = MTK_CAM_QOS_LSCI_TABLE_MAX_SIZE
			// pdi = implement later
			// aai = implement later
			qos_port_id = engine_id * raw_qos_port_num + lsci_r1;
			PBW_MB_s = MTK_CAM_QOS_LSCI_TABLE_MAX_SIZE * fps;
			dvfs_info->qos_bw_avg[qos_port_id] =
				dvfs_info->qos_bw_peak[qos_port_id] = PBW_MB_s;
			if (unlikely(debug_mmqos))
				dev_info(cam->dev, "[%16s] qos_idx:%2d BW(B/s):%lu\n",
					raw_qos->port[lsci_r1], qos_port_id, PBW_MB_s);

			/* aao_r1 = aao + aaho  */
			// aao = MTK_CAM_UAPI_AAO_MAX_BUF_SIZE (twin = /2)
			// aaho = MTK_CAM_UAPI_AAHO_HIST_SIZE
			qos_port_id = engine_id * raw_qos_port_num + aao_r1;
			PBW_MB_s = CALL_PLAT_V4L2(get_port_bw, AAO, height, fps) +
						CALL_PLAT_V4L2(get_port_bw, AAHO, height, fps);
			dvfs_info->qos_bw_avg[qos_port_id] =
				dvfs_info->qos_bw_peak[qos_port_id] = PBW_MB_s;
			if (unlikely(debug_mmqos))
				dev_info(cam->dev, "[%16s] qos_idx:%2d BW(B/s):%lu\n",
					raw_qos->port[aao_r1], qos_port_id, PBW_MB_s);

			/* afo_r1 = afo + tsfo  */
			// afo = MTK_CAM_UAPI_AFO_MAX_BUF_SIZE (twin = /2)
			// tsfso x 2 = MTK_CAM_UAPI_TSFSO_SIZE * 2
			qos_port_id = engine_id * raw_qos_port_num + afo_r1;
			PBW_MB_s = GET_PLAT_V4L2(meta_stats1_size) * fps +
						CALL_PLAT_V4L2(get_port_bw, TSFSO, height, fps);
			dvfs_info->qos_bw_avg[qos_port_id] =
				dvfs_info->qos_bw_peak[qos_port_id] = PBW_MB_s;
			if (unlikely(debug_mmqos))
				dev_info(cam->dev, "[%16s] qos_idx:%2d BW(B/s):%lu\n",
					raw_qos->port[afo_r1], qos_port_id, PBW_MB_s);

			/* ltmso_r1 = ltmso + ltmsho */
			// ltmso = MTK_CAM_UAPI_LTMSO_SIZE
			// ltmsho = MTK_CAM_UAPI_LTMSHO_SIZE
			qos_port_id = engine_id * raw_qos_port_num + ltmso_r1;
			PBW_MB_s = CALL_PLAT_V4L2(get_port_bw, LTMSO, height, fps);
			dvfs_info->qos_bw_avg[qos_port_id] =
				dvfs_info->qos_bw_peak[qos_port_id] = PBW_MB_s;
			if (unlikely(debug_mmqos))
				dev_info(cam->dev, "[%16s] qos_idx:%2d BW(B/s):%lu\n",
					raw_qos->port[ltmso_r1], qos_port_id, PBW_MB_s);

			/* ufeo_r1 = flko + ufeo + pdo */
			// flko = MTK_CAM_UAPI_FLK_BLK_SIZE * MTK_CAM_UAPI_FLK_MAX_STAT_BLK_NUM
			//						* sensor height
			// ufeo = implement later
			// pdo = implement later
			qos_port_id = engine_id * raw_qos_port_num + ufeo_r1;
			PBW_MB_s = CALL_PLAT_V4L2(get_port_bw, FLKO, height, fps);
			dvfs_info->qos_bw_avg[qos_port_id] =
				dvfs_info->qos_bw_peak[qos_port_id] = PBW_MB_s;
			if (unlikely(debug_mmqos))
				dev_info(cam->dev, "[%16s] qos_idx:%2d BW(B/s):%lu\n",
					raw_qos->port[ufeo_r1], qos_port_id, PBW_MB_s);

			/* tcyso_r1 = tcyso + rzh1n2to_r2 + drzs4no_r1 + drzh2no_r8 */
			qos_port_id = engine_id * raw_qos_port_num + tcyso_r1;
			PBW_MB_s = CALL_PLAT_V4L2(get_port_bw, TCYSO, height, fps);
			dvfs_info->qos_bw_avg[qos_port_id] =
				dvfs_info->qos_bw_peak[qos_port_id] += PBW_MB_s;
			if (unlikely(debug_mmqos))
				dev_info(cam->dev, "[%16s] qos_idx:%2d BW(B/s):%lu\n",
					raw_qos->port[tcyso_r1], qos_port_id, PBW_MB_s);

			/* TODO: bpci_r1 = bpci_r1 + bpci_r2 + bpci_r3 + caci_r1 */
			break;
		case MTKCAM_IPI_RAW_RAWI_2:
		case MTKCAM_IPI_RAW_RAWI_3:
		case MTKCAM_IPI_RAW_RAWI_5:
			if (ipi_video_id == MTKCAM_IPI_RAW_RAWI_2)
				port_id = rawi_r2;
			else if (ipi_video_id == MTKCAM_IPI_RAW_RAWI_3)
				port_id = rawi_r3;
			else if (ipi_video_id == MTKCAM_IPI_RAW_RAWI_5)
				port_id = rawi_r5;

			qos_port_id = engine_id * raw_qos_port_num + port_id;
			vdev = &pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
			ipi_fmt = mtk_cam_get_img_fmt(vdev->active_fmt.fmt.pix_mp.pixelformat);
			pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
			plane_factor = mtk_cam_get_fmt_size_factor(ipi_fmt);
			PBW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
						(vblank + height) * pixel_bits *
						plane_factor / 8 / 100;
			ABW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
						vdev->active_fmt.fmt.pix_mp.height * pixel_bits *
						plane_factor / 8 / 100;
			dvfs_info->qos_bw_peak[qos_port_id] = PBW_MB_s;
			dvfs_info->qos_bw_avg[qos_port_id] = ABW_MB_s;

			if (unlikely(debug_mmqos))
				dev_info(cam->dev, "[%16s] qos_idx:%2d ipifmt/bits/plane/w/h : %2d/%2d/%d/%5d/%5d BW(B/s)(avg:%lu,peak:%lu)\n",
				  raw_qos->port[port_id],
				  qos_port_id, ipi_fmt,
				  pixel_bits, plane_factor,
				  vdev->active_fmt.fmt.pix_mp.width,
				  vdev->active_fmt.fmt.pix_mp.height,
				  ABW_MB_s, PBW_MB_s);
			break;
		/* todo : rawi_w,  imgo_w */
		case MTKCAM_IPI_RAW_META_STATS_0:
			break;
		case MTKCAM_IPI_RAW_META_STATS_1:
			break;
		default:
			break;
		}
	}

	mtk_cam_qos_sv_bw_calc(ctx, force);

	for (i = 0; i < ctx->used_mraw_num; i++) {
		for (j = MTKCAM_IPI_MRAW_META_STATS_CFG; j < MTKCAM_IPI_MRAW_ID_MAX; j++) {
			if (ctx->mraw_pipe[i]->enabled_dmas & 1ULL<<j)
				ipi_mraw_video_id = j;
			else
				continue;
			/* update buffer internal address */
			switch (ipi_mraw_video_id) {
			case MTKCAM_IPI_MRAW_META_STATS_CFG:
				/* common */
				ipi_fmt = ctx->mraw_pipe[i]->res_config.tg_fmt;
				pixel_bits = 16;
				plane_factor = mtk_cam_get_fmt_size_factor(ipi_fmt);
				mraw_mmqos = &mraw_qos[ctx->mraw_pipe[i]->id -
					MTKCAM_SUBDEV_MRAW_START];
				mtk_cam_mraw_get_mbn_size(
					cam, ctx->mraw_pipe[i]->id, &width_mbn, &height_mbn);
				mtk_cam_mraw_get_cpi_size(
					cam, ctx->mraw_pipe[i]->id, &width_cpi, &height_cpi);

				/* imgo */
				qos_port_id = ((ctx->mraw_pipe[i]->id - MTKCAM_SUBDEV_MRAW_START)
								* mraw_qos_port_num)
								+ mraw_imgo;
				PBW_MB_s = width_mbn * fps *
					(vblank + height_mbn) * pixel_bits * plane_factor / 8 / 100;
				ABW_MB_s = width_mbn * fps *
					height_mbn * pixel_bits * plane_factor / 8 / 100;
				dvfs_info->mraw_qos_bw_peak[qos_port_id] = PBW_MB_s;
				dvfs_info->mraw_qos_bw_avg[qos_port_id] = ABW_MB_s;
				if (unlikely(debug_mmqos))
					dev_info(cam->dev, "[%16s] qos_idx:%2d BW(B/s)(avg:%lu,peak:%lu)\n",
						mraw_mmqos->port[qos_port_id % mraw_qos_port_num],
						qos_port_id, ABW_MB_s, PBW_MB_s);
				/* imgbo */
				qos_port_id = ((ctx->mraw_pipe[i]->id - MTKCAM_SUBDEV_MRAW_START)
								* mraw_qos_port_num)
								+ mraw_imgbo;
				PBW_MB_s = width_mbn * fps *
					(vblank + height_mbn) * pixel_bits * plane_factor / 8 / 100;
				ABW_MB_s = width_mbn * fps *
					height_mbn * pixel_bits * plane_factor / 8 / 100;
				/* cpio */
				PBW_MB_s += ((width_cpi + 7) / 8) * fps *
					(vblank + height_cpi) * plane_factor / 8 / 100;
				ABW_MB_s += ((width_cpi + 7) / 8) * fps *
					height_cpi * plane_factor / 8 / 100;
				dvfs_info->mraw_qos_bw_peak[qos_port_id] = PBW_MB_s;
				dvfs_info->mraw_qos_bw_avg[qos_port_id] = ABW_MB_s;
				if (unlikely(debug_mmqos))
					dev_info(cam->dev, "[%16s] qos_idx:%2d BW(B/s)(avg:%lu,peak:%lu)\n",
						mraw_mmqos->port[qos_port_id % mraw_qos_port_num],
						qos_port_id, ABW_MB_s, PBW_MB_s);
				/* CQ */
				qos_port_id = ((ctx->mraw_pipe[i]->id - MTKCAM_SUBDEV_MRAW_START)
								* mraw_qos_port_num)
								+ mraw_cqi_m1;
				PBW_MB_s = ABW_MB_s = CQ_BUF_SIZE * fps / 10;
				dvfs_info->mraw_qos_bw_peak[qos_port_id] = PBW_MB_s;
				dvfs_info->mraw_qos_bw_avg[qos_port_id] = ABW_MB_s;
				if (unlikely(debug_mmqos))
					dev_info(cam->dev, "[%16s] qos_idx:%2d BW(B/s):%lu\n",
						mraw_mmqos->port[qos_port_id % mraw_qos_port_num],
						qos_port_id, PBW_MB_s);
				break;

			case MTKCAM_IPI_MRAW_META_STATS_0:
				break;
			}
		}
	}
	/* by engine update */
	for (i = 0; i < raw_qos_port_num; i++) {
		qos_port_id = engine_id * raw_qos_port_num + i;
		if (unlikely(debug_mmqos))
			dev_info(cam->dev, "[%s] port idx/name:%2d/%16s BW(kB/s)(avg:%lu,peak:%lu)\n",
			  __func__, qos_port_id, raw_qos->port[i],
			  BW_B2KB_WITH_RATIO(dvfs_info->qos_bw_avg[qos_port_id]),
			  (is_raw_srt ? 0 : BW_B2KB(dvfs_info->qos_bw_peak[qos_port_id])));
#ifdef DVFS_QOS_READY
		if (dvfs_info->qos_req[qos_port_id]) {
			mtk_icc_set_bw(dvfs_info->qos_req[qos_port_id],
				kBps_to_icc(BW_B2KB_WITH_RATIO(
					dvfs_info->qos_bw_avg[qos_port_id])),
				(is_raw_srt ? 0 : kBps_to_icc(BW_B2KB(
					dvfs_info->qos_bw_peak[qos_port_id]))));
		}
#endif
	}
	for (i = 0; i < MTK_CAM_MRAW_PORT_NUM; i++) {
		if (dvfs_info->mraw_qos_bw_avg[i] != 0) {
			if (unlikely(debug_mmqos))
				dev_info(cam->dev, "[%s] mraw port idx:%2d BW(kB/s)(avg:%lu,peak:%lu)\n",
				  __func__, i,
				  BW_B2KB_WITH_RATIO(dvfs_info->mraw_qos_bw_avg[i]),
				  BW_B2KB(dvfs_info->mraw_qos_bw_peak[i]));
#ifdef DVFS_QOS_READY
			if (dvfs_info->mraw_qos_req[i])
				mtk_icc_set_bw(dvfs_info->mraw_qos_req[i],
					kBps_to_icc(BW_B2KB_WITH_RATIO(
						dvfs_info->mraw_qos_bw_avg[i])),
					kBps_to_icc(BW_B2KB(
						dvfs_info->mraw_qos_bw_peak[i])));
#endif
		}
	}
#ifdef DVFS_QOS_READY
	mtk_mmqos_wait_throttle_done();
#endif
}

void mtk_cam_qos_bw_calc(struct mtk_cam_ctx *ctx, unsigned long raw_dmas, bool force)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_raw_pipeline *pipe = ctx->pipe;
	struct mtk_raw_device *master_raw = get_master_raw_dev(cam, pipe);
	struct mtk_raw_device *slave_raw = get_slave_raw_dev(cam, pipe);
	unsigned long slave_raw_dmas = 0;
	unsigned int i = 0;

	__mtk_cam_qos_bw_calc(ctx, master_raw, raw_dmas, force);

	if (slave_raw) {
		for (i = 0; i < MTKCAM_IPI_RAW_ID_MAX; i++) {
			if (!(raw_dmas & 1ULL<<i))
				continue;

			switch (i) {
			case MTKCAM_IPI_RAW_IMGO_W:
				slave_raw_dmas |= 1ULL << MTKCAM_IPI_RAW_IMGO;
				break;
			case MTKCAM_IPI_RAW_RAWI_2_W:
				slave_raw_dmas |= 1ULL << MTKCAM_IPI_RAW_RAWI_2;
				break;
			case MTKCAM_IPI_RAW_RAWI_3_W:
				slave_raw_dmas |= 1ULL << MTKCAM_IPI_RAW_RAWI_3;
				break;
			case MTKCAM_IPI_RAW_RAWI_5_W:
				slave_raw_dmas |= 1ULL << MTKCAM_IPI_RAW_RAWI_5;
				break;
			default:
				break;
			}
		}

		if (slave_raw_dmas)
			__mtk_cam_qos_bw_calc(ctx, slave_raw, slave_raw_dmas, force);
	}
}

void mtk_cam_qos_sv_bw_calc(struct mtk_cam_ctx *ctx, bool force)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_camsys_dvfs *dvfs_info = &cam->camsys_ctrl.dvfs_info;
	struct v4l2_subdev_frame_interval fi;
	struct v4l2_subdev_format sd_fmt;
	struct v4l2_ctrl *ctrl;
	struct sv_mmqos *sv_mmqos;
	unsigned int qos_port_id;
	unsigned int ipi_fmt;
	int i, j, pixel_bits, plane_factor;
	unsigned long vblank = 0, fps = 0, height = 0, PBW_MB_s, ABW_MB_s;
	struct mtkcam_ipi_input_param *cfg_in_param;

	memset(&fi, 0, sizeof(fi));
	memset(&sd_fmt, 0, sizeof(sd_fmt));
	/* Reset all dvfs_info bw */
	for (j = 0; j < sv_qos_port_num; j++) {
		qos_port_id = (ctx->sv_dev->id * sv_qos_port_num) + j;
		dvfs_info->sv_qos_bw_peak[qos_port_id] = 0;
		dvfs_info->sv_qos_bw_avg[qos_port_id] = 0;
	}

	for (i = SVTAG_START; i < SVTAG_END; i++) {
		if (!(ctx->sv_dev->enabled_tags & (1 << i)))
			continue;

		if (ctx->sensor) {
			fi.pad = 0;
			fi.reserved[0] = V4L2_SUBDEV_FORMAT_ACTIVE;
			v4l2_subdev_call(ctx->sensor, video, g_frame_interval, &fi);
			fps = fi.interval.denominator / fi.interval.numerator;
			ctrl = v4l2_ctrl_find(ctx->sensor->ctrl_handler, V4L2_CID_VBLANK);
			if (!ctrl)
				dev_info(cam->dev, "[%s] ctrl is NULL\n", __func__);
			else
				vblank = v4l2_ctrl_g_ctrl(ctrl);
			sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
			sd_fmt.pad = ctx->sv_dev->tag_info[i].seninf_padidx;
			v4l2_subdev_call(ctx->seninf, pad, get_fmt, NULL, &sd_fmt);
			height = sd_fmt.format.height;
			dev_info(cam->dev, "[%s] FPS:%lu/%lu:%lu, H:%lu, VB:%lu\n",
				__func__, fi.interval.denominator, fi.interval.numerator,
				fps, height, vblank);
		}

		cfg_in_param = &ctx->sv_dev->tag_info[i].cfg_in_param;
		/* imgo */
		qos_port_id = (ctx->sv_dev->id * sv_qos_port_num) + sv_imgo;
		sv_mmqos = &sv_qos[ctx->sv_dev->id];
		ipi_fmt = mtk_cam_get_img_fmt(
			ctx->sv_dev->tag_info[i].img_fmt.fmt.pix_mp.pixelformat);
		pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
		plane_factor = mtk_cam_get_fmt_size_factor(ipi_fmt);
		PBW_MB_s = cfg_in_param->in_crop.s.w * fps *
					(vblank + height) * pixel_bits *
					plane_factor / 8 / 100;
		ABW_MB_s = cfg_in_param->in_crop.s.w * fps *
					cfg_in_param->in_crop.s.h * pixel_bits *
					plane_factor / 8 / 100;
		dvfs_info->sv_qos_bw_peak[qos_port_id] += PBW_MB_s;
		dvfs_info->sv_qos_bw_avg[qos_port_id] += ABW_MB_s;
		if (unlikely(debug_mmqos))
			dev_info(cam->dev, "[%16s] qos_idx:%2d ipifmt/bits/plane/w/h : %2d/%2d/%d/%5d/%5d BW(B/s)(avg:%lu,peak:%lu)\n",
			  sv_mmqos->port[qos_port_id % sv_qos_port_num], qos_port_id, ipi_fmt,
			  pixel_bits, plane_factor,
			  cfg_in_param->in_crop.s.w,
			  cfg_in_param->in_crop.s.h,
			  ABW_MB_s, PBW_MB_s);
	}
	/* scq */
	if (ctx->sv_dev->enabled_tags) {
		qos_port_id = (ctx->sv_dev->id * sv_qos_port_num) + sv_cqi;
		sv_mmqos = &sv_qos[ctx->sv_dev->id];
		PBW_MB_s = ABW_MB_s = CQ_BUF_SIZE * fps / 10;
		dvfs_info->sv_qos_bw_peak[qos_port_id] = PBW_MB_s;
		dvfs_info->sv_qos_bw_avg[qos_port_id] = ABW_MB_s;
		if (unlikely(debug_mmqos))
			dev_info(cam->dev, "[%16s] qos_idx:%2d BW(B/s):%lu\n",
			  sv_mmqos->port[qos_port_id % sv_qos_port_num], qos_port_id,
			  PBW_MB_s);
	}
	/* by engine update */
	for (i = 0; i < MTK_CAM_SV_PORT_NUM; i++) {
		if (dvfs_info->sv_qos_bw_avg[i] != 0) {
			if (unlikely(debug_mmqos))
				dev_info(cam->dev, "[%s] camsv port idx:%2d BW(kB/s)(avg:%lu,peak:%lu)\n",
				  __func__, i,
				  BW_B2KB_WITH_RATIO(dvfs_info->sv_qos_bw_avg[i]),
				  BW_B2KB(dvfs_info->sv_qos_bw_peak[i]));
#ifdef DVFS_QOS_READY
			if (dvfs_info->sv_qos_req[i])
				mtk_icc_set_bw(dvfs_info->sv_qos_req[i],
					kBps_to_icc(BW_B2KB_WITH_RATIO(
						dvfs_info->sv_qos_bw_avg[i])),
					kBps_to_icc(BW_B2KB(
						dvfs_info->sv_qos_bw_peak[i])));
#endif
		}
	}
#ifdef DVFS_QOS_READY
	mtk_mmqos_wait_throttle_done();
#endif
}

void mtk_cam_qos_init(struct mtk_cam_device *cam)
{
	struct mtk_camsys_dvfs *dvfs_info = &cam->camsys_ctrl.dvfs_info;
	struct sv_mmqos *sv_mmqos;
	struct mraw_mmqos *mraw_mmqos;
	struct mtk_raw_device *raw_dev;
	struct mtk_yuv_device *yuv_dev;
	struct mtk_camsv_device *sv_dev;
	struct mtk_mraw_device *mraw_dev;
	struct device *engineraw_dev;
	struct device *engineyuv_dev;
	struct device *enginesv_dev;
	struct device *enginemraw_dev;
	int i, raw_num, port_id, engine_id;
	struct raw_mmqos *raw_qos = NULL;

	CALL_PLAT_V4L2(get_mmqos_port, &raw_qos);

	for (i = 0; i < MTK_CAM_RAW_PORT_NUM; i++) {
		dvfs_info->qos_req[i] = 0;
		dvfs_info->qos_bw_avg[i] = 0;
		dvfs_info->qos_bw_peak[i] = 0;
	}
	for (i = 0; i < MTK_CAM_SV_PORT_NUM; i++) {
		dvfs_info->sv_qos_req[i] = 0;
		dvfs_info->sv_qos_bw_avg[i] = 0;
		dvfs_info->sv_qos_bw_peak[i] = 0;
	}
	for (i = 0; i < MTK_CAM_MRAW_PORT_NUM; i++) {
		dvfs_info->mraw_qos_req[i] = 0;
		dvfs_info->mraw_qos_bw_avg[i] = 0;
		dvfs_info->mraw_qos_bw_peak[i] = 0;
	}
	i = 0;
	for (raw_num = 0; raw_num < cam->num_raw_drivers; raw_num++) {
		engine_id = RAW_A + raw_num;
		engineraw_dev = cam->raw.devs[engine_id];
		engineyuv_dev = cam->raw.yuvs[engine_id];

		for (port_id = 0; port_id < raw_qos_port_num; port_id++) {
			if (port_id < yuvo_r1) {
				raw_dev = dev_get_drvdata(engineraw_dev);
#ifdef DVFS_QOS_READY
				dvfs_info->qos_req[i] =
					of_mtk_icc_get(raw_dev->dev, raw_qos->port[port_id]);
#endif
			} else {
				yuv_dev = dev_get_drvdata(engineyuv_dev);
#ifdef DVFS_QOS_READY
				dvfs_info->qos_req[i] =
					of_mtk_icc_get(yuv_dev->dev, raw_qos->port[port_id]);
#endif
			}
			i++;
		}
		dvfs_info->updated_raw_dmas[engine_id] = 0;
		dev_info(raw_dev->dev, "[%s] raw_engine_id=%d, port_num=%d\n",
			 __func__, engine_id, i);
	}
	i = 0;
	for (engine_id = CAMSV_START; engine_id < CAMSV_END; engine_id++) {
		sv_mmqos = sv_qos + engine_id;
		enginesv_dev = cam->sv.devs[engine_id];
		for (port_id = 0; port_id < sv_qos_port_num; port_id++) {
			sv_dev = dev_get_drvdata(enginesv_dev);
#ifdef DVFS_QOS_READY
			dvfs_info->sv_qos_req[i] =
				of_mtk_icc_get(sv_dev->dev, sv_mmqos->port[port_id]);
#endif
			i++;
		}
		dev_info(sv_dev->dev, "[%s] sv_engine_id=%d, port_num=%d\n",
			 __func__, engine_id, i);
	}
	i = 0;
	for (engine_id = MRAW_START; engine_id < MRAW_END; engine_id++) {
		mraw_mmqos = mraw_qos + engine_id;
		enginemraw_dev = cam->mraw.devs[engine_id];
		for (port_id = 0; port_id < mraw_qos_port_num; port_id++) {
			mraw_dev = dev_get_drvdata(enginemraw_dev);
#ifdef DVFS_QOS_READY
			dvfs_info->mraw_qos_req[i] =
				of_mtk_icc_get(mraw_dev->dev, mraw_mmqos->port[port_id]);
#endif
			i++;
		}
		dev_info(mraw_dev->dev, "[%s] mraw_engine_id=%d, port_num=%d\n",
			 __func__, engine_id, i);
	}
}

void mtk_cam_qos_bw_reset(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_raw_pipeline *pipe = ctx->pipe;
	struct mtk_raw_device *raw_dev = get_master_raw_dev(cam, pipe);
	struct mtk_camsys_dvfs *dvfs_info = &cam->camsys_ctrl.dvfs_info;
	unsigned int qos_port_id;
	int engine_id = raw_dev->id;
	int i, j;

	dev_info(cam->dev, "[%s] enabled_raw(%d),used_sv_num(%d),used_mraw_num(%d)\n",
		__func__, ctx->pipe->enabled_raw, ctx->used_sv_num, ctx->used_mraw_num);

	dvfs_info->updated_raw_dmas[engine_id] = 0;

	for (i = 0; i < raw_qos_port_num; i++) {
		qos_port_id = engine_id * raw_qos_port_num + i;
		dvfs_info->qos_bw_avg[qos_port_id] = 0;
		dvfs_info->qos_bw_peak[qos_port_id] = 0;
#ifdef DVFS_QOS_READY
		if (dvfs_info->qos_req[qos_port_id])
			mtk_icc_set_bw(dvfs_info->qos_req[qos_port_id], 0, 0);
#endif
	}

	mtk_cam_qos_sv_bw_reset(ctx);

	for (i = 0; i < ctx->used_mraw_num; i++) {
		for (j = 0; j < mraw_qos_port_num; j++) {
			qos_port_id = ((ctx->mraw_pipe[i]->id - MTKCAM_SUBDEV_MRAW_START)
				* mraw_qos_port_num) + j;
			dvfs_info->mraw_qos_bw_avg[qos_port_id] = 0;
			dvfs_info->mraw_qos_bw_peak[qos_port_id] = 0;
#ifdef DVFS_QOS_READY
			if (dvfs_info->mraw_qos_req[qos_port_id])
				mtk_icc_set_bw(dvfs_info->mraw_qos_req[qos_port_id], 0, 0);
#endif
		}
	}
}

void mtk_cam_qos_sv_bw_reset(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_camsys_dvfs *dvfs_info = &cam->camsys_ctrl.dvfs_info;
	unsigned int qos_port_id;
	int i;

	for (i = 0; i < sv_qos_port_num; i++) {
		qos_port_id = (ctx->sv_dev->id * sv_qos_port_num) + i;
		dvfs_info->sv_qos_bw_peak[qos_port_id] = 0;
		dvfs_info->sv_qos_bw_avg[qos_port_id] = 0;
#ifdef DVFS_QOS_READY
			if (dvfs_info->sv_qos_req[qos_port_id])
				mtk_icc_set_bw(dvfs_info->sv_qos_req[qos_port_id], 0, 0);
#endif
	}
}

