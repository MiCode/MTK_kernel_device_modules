// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_dp_debug.h"
#include "mtk_dp.h"
#include "mtk_dp_api.h"
#include "mtk_dp_hal.h"
#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif

static bool g_dptx_log;

void mtk_dp_debug_enable(bool enable)
{
	g_dptx_log = enable;
}

bool mtk_dp_debug_get(void)
{
	return g_dptx_log;
}

void mtk_dp_debug(const char *opt)
{
	DPTXFUNC("[debug]: %s\n", opt);

	if (strncmp(opt, "force_cvt:", 10) == 0) {
		unsigned int ret, res_h, res_v, fps;

		ret = sscanf(opt, "force_cvt:%d,%d,%d\n", &res_h, &res_v, &fps);
		if (ret != 3) {
			DDPINFO("error to parse force_cvt cmd\n");
			return;
		}
		mtk_dp_force_timing_cvt(res_h, res_v, fps);
	} else if (strncmp(opt, "force_cea:", 10) == 0) {
		unsigned int ret, res_h, res_v, fps;

		ret = sscanf(opt, "force_cea:%d,%d,%d\n", &res_h, &res_v, &fps);
		if (ret != 3) {
			DDPINFO("error to parse force_cea cmd\n");
			return;
		}
		mtk_dp_force_timing_cea(res_h, res_v, fps);
	} else if (strncmp(opt, "cancel_force_timing", 19) == 0)
		mtk_dp_cancel_force_timing();
	else if (strncmp(opt, "force_detail:", 13) == 0) {
		unsigned int ret;
		unsigned int value[9];

		ret = sscanf(opt, "force_detail:%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			&value[0], &value[1], &value[2], &value[3], &value[4], &value[5],
			&value[6], &value[7], &value[8]);
		if (ret != 9) {
			DDPINFO("error to parse force_detail cmd\n");
			return;
		}
		mtk_dp_force_timing_detail(value);
	} else if (strncmp(opt, "fec:", 4) == 0) {
		if (strncmp(opt + 4, "enable", 6) == 0)
			mtk_dp_fec_enable(1);
		else if (strncmp(opt + 4, "disable", 7) == 0)
			mtk_dp_fec_enable(0);
		else
			DDPINFO("fec:enable/disable error msg\n");

	} else if (strncmp(opt, "power:", 6) == 0) {
		if (strncmp(opt + 6, "on", 2) == 0)
			mtk_dp_power_save(0x1);
		else if (strncmp(opt + 6, "off", 3) == 0)
			mtk_dp_power_save(0x0);

	} else if (strncmp(opt, "dptest:", 7) == 0) {
		if (strncmp(opt + 7, "2", 1) == 0)
			mtk_dp_test(2);
		else if (strncmp(opt + 7, "4", 1) == 0)
			mtk_dp_test(4);
		else if (strncmp(opt + 7, "8", 1) == 0)
			mtk_dp_test(8);
		else
			DDPINFO("dptest error msg\n");
	} else if (strncmp(opt, "debug_log:", 10) == 0) {
		if (strncmp(opt + 10, "on", 2) == 0)
			mtk_dp_debug_enable(true);
		else if (strncmp(opt + 10, "off", 3) == 0)
			mtk_dp_debug_enable(false);
	} else if (strncmp(opt, "force_hdcp13:", 13) == 0) {
		if (strncmp(opt + 13, "enable", 6) == 0)
			mtk_dp_force_hdcp1x(true);
		else
			mtk_dp_force_hdcp1x(false);

	} else if (strncmp(opt, "hdcp:", 5) == 0) {
		if (strncmp(opt + 5, "enable", 6) == 0)
			mtk_dp_hdcp_enable(true);
		else if (strncmp(opt + 5, "disable", 7) == 0)
			mtk_dp_hdcp_enable(false);
	} else if (strncmp(opt, "setpowermode", 12) == 0) {
		mtk_dp_SWInterruptSet(2);
		mdelay(100);
		mtk_dp_SWInterruptSet(4);

	} else if (strncmp(opt, "setdisconmode", 13) == 0)
		mtk_dp_SWInterruptSet(2);
	else if (strncmp(opt, "pattern:", 8) == 0) {
		int ret = 0;
		int enable, resolution;

		ret = sscanf(opt, "pattern:%d,%d\n", &enable, &resolution);
		if (ret != 2) {
			DPTXMSG("ret = %d\n", ret);
			return;
		}

		DPTXMSG("Paterrn Gen:enable = %d, resolution =%d\n",
			enable, resolution);
		mdrv_DPTx_PatternSet(enable, resolution);
	} else if (strncmp(opt, "format:", 7) == 0) {
		int ret = 0;
		int bpc, format;

		ret = sscanf(opt, "format:%d,%d\n", &bpc, &format);
		if (ret != 2) {
			DPTXMSG("ret = %d\n", ret);
			return;
		}
		DPTXMSG("set bpc:%d format:%d\n",bpc, format);
		mdrv_DPTx_ColorSet(bpc, format);
	} else if (strncmp(opt, "maxlinkrate:", 12) == 0) {
		int ret = 0;
		int enable, maxlinkrate;

		ret = sscanf(opt, "maxlinkrate:%d,%d\n", &enable, &maxlinkrate);
		if (ret != 2) {
			DPTXMSG("ret = %d\n", ret);
			return;
		}

		DPTXMSG("set max link rate:enable = %d, maxlinkrate =%d\n",
			enable, maxlinkrate);
		mdrv_DPTx_set_maxlinkrate(enable, maxlinkrate);
	} else if (strncmp(opt, "max_lane_set:", 13) == 0) {
		int ret = 0;
		int number;
		bool enable;

		ret = sscanf(opt, "max_lane_set:%d,%d\n",&enable ,&number);
		if (ret != 2) {
			DPTXMSG("ret = %d\n", ret);
			return;
		}

		DPTXMSG("set max lane = %d\n", number);
		mtk_dp_set_force_lane(enable, number);
	} else if (strncmp(opt, "video_clock:", 12) == 0) {
		int ret = 0;
		unsigned int clksrc;
		unsigned int con1;

		ret = sscanf(opt, "video_clock:%x,%x\n", &clksrc, &con1);
		if (ret != 2) {
			DPTXERR("ret = %d\n", ret);
			return;
		}
		mtk_dp_clock_debug(clksrc, con1);
	} else if (strncmp(opt, "reset_all", 9) == 0) {
		mtk_dp_reset_all();
	} else if (strncmp(opt, "delay:", 6) == 0) {
		unsigned int delay_enable;
		unsigned int mode;
		unsigned int delay_time;
		bool enable;
		int ret = 0;

		ret = sscanf(opt, "delay:%d,%d,%d\n",&delay_enable, &mode, &delay_time);
		if (ret != 3) {
			DPTXERR("[DP Debug]invalid input, ret = %d\n", ret);
			return;
		}
		enable = (delay_enable!= 0);
		mtk_dp_set_delay(enable, mode, delay_time);
	} else if (strncmp(opt, "dump:", 5) == 0) {
		dptx_dump_reg();
	} else if (strncmp(opt, "dptx_video_pg:", 14) == 0) {
		int ret = 0;
		unsigned int pg_enable;
		bool enable;

		ret = sscanf(opt, "dptx_video_pg:%d\n", &pg_enable);
		if (ret != 1) {
			DPTXERR("[DP Debug]invalid input, ret = %d\n", ret);
			return;
		}
		enable = (pg_enable!= 0);
		mtk_dp_MacVideoPatternGenEn(enable);
	} else if (strncmp(opt, "dptx_audio_pg:", 14) == 0) {
		int ret = 0;
		unsigned int pg_enable;
		int enable;

		ret = sscanf(opt, "dptx_audio_pg:%d\n", &pg_enable);
		if (ret != 1) {
			DPTXERR("[DP Debug]invalid input, ret = %d\n", ret);
			return;
		}
		enable = (pg_enable!= 0);
		mtk_dp_MacAudioPatternGenEn(enable);
	} else if (strncmp(opt, "dpintf_pg:", 10) == 0) {
		int ret = 0;
		int pg_mode;

		ret = sscanf(opt, "dpintf_pg:%d\n", &pg_mode);
		if (ret != 1) {
			DPTXERR("[DP Debug]invalid input, ret = %d\n", ret);
			return;
		}
		mtk_dp_intfPatternGenEn(pg_mode);
	} else if (strncmp(opt, "force_timing:", 13) == 0) {
		unsigned int ret = 0;
		unsigned int force_timing_enable;
		unsigned int mode;
		bool enable;

		ret = sscanf(opt, "force_timing:%d,%d\n", &force_timing_enable, &mode);
		if (ret != 2) {
			DPTXERR("[DP Debug]invalid input, ret = %d\n", ret);
			return;
		}
		enable = (force_timing_enable!= 0);
		mtk_dp_force_timing(enable, mode);
	} else if (strncmp(opt, "mute_all", 8) == 0) {
		int ret = 0;
		unsigned int mute_enable;
		bool enable;

		ret = sscanf(opt, "mute_all:%d\n", &mute_enable);
		if (ret != 1) {
			DPTXERR("ret = %d\n", ret);
			return;
		}
		enable = (mute_enable!= 0);
		dptx_mute_all_command(enable);
	} else if (strncmp(opt, "dp_con", 6) == 0) {
		mtk_dp_dpconnector_setting();
	} else if (strncmp(opt, "dp_fakeRX_en:", 13) == 0) {
		int ret = 0;
		int mode;

		ret = sscanf(opt, "dp_fakeRX_en:%d\n", &mode);
		if (ret != 1) {
			DPTXERR("ret = %d\n", ret);
			return;
		}
		mtk_dp_fakeRX_enable(mode);
	} else if (strncmp(opt, "dvo_checksum_en", 15) == 0)
		mtk_dp_dvo_ChecksumTrigger();
}

#ifdef MTK_DPINFO
#if IS_ENABLED(CONFIG_DEBUG_FS)
static struct dentry *mtkdp_dbgfs;
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
static struct proc_dir_entry *mtkdp_procfs;
#endif

struct mtk_dp_debug_info {
	char *name;
	uint8_t index;
};

enum mtk_dp_debug_index {
	DP_INFO_HDCP      = 0,
	DP_INFO_PHY       = 1,
	DP_INFO_MAX
};

static struct mtk_dp_debug_info dp_info[DP_INFO_MAX] = {
	{"HDCP", DP_INFO_HDCP},
	{"PHY", DP_INFO_PHY},
};

static uint8_t g_infoIndex = DP_INFO_HDCP;

static int mtk_dp_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}

static ssize_t mtk_dp_debug_read(struct file *file, char __user *ubuf,
	size_t count, loff_t *ppos)
{
	int ret = 0;
	char *buffer;

	buffer = kmalloc(PAGE_SIZE / 8, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	switch (g_infoIndex) {
	case DP_INFO_HDCP:
		ret = mtk_dp_hdcp_getInfo(buffer, PAGE_SIZE / 8);
		break;
	case DP_INFO_PHY:
		ret = mtk_dp_phy_getInfo(buffer, PAGE_SIZE / 8);
		break;
	default:
		DPTXERR("Invalid inedx!");
	}

	if (ret > 0)
		ret = simple_read_from_buffer(ubuf, count, ppos, buffer, ret);

	kfree(buffer);
	return ret;
}

static void mtk_dp_process_dbg_opt(const char *opt)
{
	int i = 0;

	for (i = 0; i < DP_INFO_MAX; i++) {
		if (!strncmp(opt, dp_info[i].name, strlen(dp_info[i].name))) {
			g_infoIndex = dp_info[i].index;
			break;
		}
	}

	if (g_infoIndex == DP_INFO_MAX)
		g_infoIndex = DP_INFO_HDCP;
}

static ssize_t mtk_dp_debug_write(struct file *file, const char __user *ubuf,
	size_t count, loff_t *ppos)
{
	const int debug_bufmax = 512 - 1;
	size_t ret;
	char cmd_buffer[512];
	char *tok;
	char *cmd = cmd_buffer;

	ret = count;
	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&cmd_buffer, ubuf, count) != 0ULL)
		return -EFAULT;

	cmd_buffer[count] = '\0';

	DPTXMSG("[mtkdp_dbg]%s!\n", cmd_buffer);
	while ((tok = strsep(&cmd, " ")) != NULL)
		mtk_dp_process_dbg_opt(tok);

	return ret;
}

static const struct file_operations dp_debug_fops = {
	.read = mtk_dp_debug_read,
	.write = mtk_dp_debug_write,
	.open = mtk_dp_debug_open,
};

static const struct proc_ops dp_debug_proc_fops = {
	.proc_read = mtk_dp_debug_read,
	.proc_write = mtk_dp_debug_write,
	.proc_open = mtk_dp_debug_open,
};

int mtk_dp_debugfs_init(void)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)
	mtkdp_dbgfs = debugfs_create_file("mtk_dpinfo", 0644,
		NULL, NULL, &dp_debug_fops);
	if (IS_ERR_OR_NULL(mtkdp_dbgfs))
		return -ENOMEM;
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
	mtkdp_procfs = proc_create("mtk_dpinfo", 0644, NULL,
		&dp_debug_proc_fops);
	if (IS_ERR_OR_NULL(mtkdp_procfs))
		return -ENOMEM;
#endif
	return 0;
}

void mtk_dp_debugfs_deinit(void)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)
	debugfs_remove(mtkdp_dbgfs);
	mtkdp_dbgfs = NULL;
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
	if (mtkdp_procfs) {
		proc_remove(mtkdp_procfs);
		mtkdp_procfs = NULL;
	}
#endif

}
#endif

