// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/rtc.h>
#include <linux/iommu.h>
#include <mtk-smmu-v3.h>
#include "mtk_drm_drv.h"
#include "mtk_drm_ddp_comp.h"
#include "scp.h"
#include "mtk_log.h"
#include "mtk_drm_crtc.h"
#include "mtk_disp_pmqos.h"
#include "mtk_disp_oddmr/mtk_disp_oddmr.h"

/* AOD-SCP Support format: RGBA8888/ RGBA10101012/ YUYV
 * This header is digit raw data for demo, customer can replace to their own picture
 */
#include "mtk_aod_scp_digit.h"


/* AO Mode: trigger AOD SCP before switch to doze suspend
 * It's only for early develop or debug purpsoe in MTK
 */
#define AO_MODE	(0)


struct aod_scp_ipi_receive_info {
	unsigned int aod_id;
};

static int CFG_DISPLAY_WIDTH;
static int CFG_DISPLAY_HEIGHT;
static int CFG_DISPLAY_VREFRESH;

#define ALIGN_TO(x, n)  (((x) + ((n) - 1)) & ~((n) - 1))
#define MTK_FB_ALIGNMENT 32
#define CFG_DISPLAY_ALIGN_WIDTH   ALIGN_TO(CFG_DISPLAY_WIDTH, MTK_FB_ALIGNMENT)
#define AOD_TEMP_BACKLIGHT 1800

/* Use SCP DRAM REGION for AOD shared info
 * frame config: 0x0 ~ sizeof(disp_frame_config)
 * layer config: sizeof(disp_frame_config) ~ sizeof(disp_input_config)*6
 * module_backup: 0xA000 ~ 0xFFFF
 * aod pictures: 0x10000 ~ scp_get_reserve_mem_size(SCP_AOD_MEM_ID)
 */
#define AOD_BR_ODDMR0_OFFSET   0xA000
#define AOD_BR_SPR0_OFFSET     0xC000
#define AOD_BR_DSC0_OFFSET     0xD000
#define AOD_BR_MIPITX0_OFFSET  0xE000
#define AOD_BR_DSI0_OFFSET     0xF000
#define AOD_SCP_PICTURE_OFFSET 0x10000

enum CLOCL_DIGIT {
	SECONDS = BIT(0),
	SECOND_TENS = BIT(1),
	MINUTES = BIT(2),
	MINUTE_TENS = BIT(3),
	HOURS = BIT(4),
	HOUR_TENS = BIT(5),
};

struct disp_input_config {
	unsigned int layer;
	unsigned int layer_en;
	unsigned int fmt;
	unsigned int addr;
	unsigned int src_x;
	unsigned int src_y;
	unsigned int src_pitch;
	unsigned int dst_x;
	unsigned int dst_y;
	unsigned int dst_w;
	unsigned int dst_h;
	unsigned int aen;
	unsigned char alpha;
	unsigned char time_digit;
	unsigned char blend_mode;
};

struct disp_frame_config {
	unsigned int interval; //ms
	unsigned int ulps_wakeup_prd;
	unsigned int layer_info_addr[6];
	unsigned int digits_addr[10];
	unsigned int frame_w;
	unsigned int frame_h;
};

struct disp_module_backup_info {
	char *module_name;
	unsigned int base;
	unsigned int size;
	unsigned int offset;
};

static struct aod_scp_ipi_receive_info aod_scp_msg;
static int aod_state;
static unsigned int aod_scp_ulps_wakeup_prd;
static struct mtk_aod_scp_cb aod_scp_cb;

static phys_addr_t scp_aod_mem_pa;
static phys_addr_t scp_aod_mem_va;
static phys_addr_t scp_aod_mem_size;

static struct disp_module_backup_info module_list[] = {
	{"dsi0", 0x0, 0x0, AOD_BR_DSI0_OFFSET},
	{"dsc0", 0x0, 0x0, AOD_BR_DSC0_OFFSET},
	{"mipitx0", 0x0, 0x0, AOD_BR_MIPITX0_OFFSET},
	{"spr0", 0x0, 0x0, AOD_BR_SPR0_OFFSET},
	{"oddmr0", 0x0, 0x0, AOD_BR_ODDMR0_OFFSET},
};

#define AOD_STAT_SET(stat) (aod_state |= (stat))
#define AOD_STAT_CLR(stat) (aod_state &= ~(stat))
#define AOD_STAT_MATCH(stat) ((aod_state & (stat)) == (stat))

enum AOD_SCP_STATE {
	AOD_STAT_ENABLE = BIT(0),
	AOD_STAT_CONFIGED = BIT(1),
	AOD_STAT_ACTIVE = BIT(2),
};

enum OVL_INPUT_FORMAT {
	OVL_INPUT_FORMAT_BGR565     = 0,
	OVL_INPUT_FORMAT_RGB888     = 1,
	OVL_INPUT_FORMAT_RGBA8888   = 2,
	OVL_INPUT_FORMAT_ARGB8888   = 3,
	OVL_INPUT_FORMAT_VYUY       = 4,
	OVL_INPUT_FORMAT_YVYU       = 5,
	OVL_INPUT_FORMAT_RGB565     = 6,
	OVL_INPUT_FORMAT_BGR888     = 7,
	OVL_INPUT_FORMAT_BGRA8888   = 8,
	OVL_INPUT_FORMAT_ABGR8888   = 9,
	OVL_INPUT_FORMAT_UYVY       = 10,
	OVL_INPUT_FORMAT_YUYV       = 11,
	OVL_INPUT_FORMAT_UNKNOWN    = 32,
};

enum OVL_BLEND_MODE {
	BLD_MODE_NONE,
	BLD_MODE_PREMULTIPLIED,
	BLD_MODE_COVERAGE,
	BLD_MODE_SRC_ATOP,
	BLD_MODE_DST_ATOP,
	BLD_MODE_DST_OVER,
};

void mtk_module_backup(struct drm_crtc *crtc, unsigned int ulps_wakeup_prd)
{
	struct mtk_ddp_comp *comp = NULL;
	struct iommu_domain *domain;
	struct disp_frame_config *frame0;
	char *bkup_buf, *scp_sh_mem, *module_base;
	void __iomem *va = 0;
	int i, size, ret;

	if (!AOD_STAT_MATCH(AOD_STAT_ENABLE) || !crtc)
		return;

	CFG_DISPLAY_WIDTH = crtc->state->mode.hdisplay;
	CFG_DISPLAY_HEIGHT = crtc->state->mode.vdisplay;
	CFG_DISPLAY_VREFRESH = drm_mode_vrefresh(&crtc->state->mode);

	scp_sh_mem = (char *)scp_get_reserve_mem_virt(SCP_AOD_MEM_ID);

	if (!scp_sh_mem) {
		DDPMSG("%s: Get shared memory fail\n", __func__);
		return;
	}

	/* Setup exdma pa2va mode for AOD_SCP picture memory */
	if (!AOD_STAT_MATCH(AOD_STAT_CONFIGED)) {
		comp = mtk_ddp_comp_sel_in_cur_crtc_path(to_mtk_crtc(crtc), MTK_OVL_EXDMA, 0);
		if (!comp)
			return;

		domain = iommu_get_domain_for_dev(mtk_smmu_get_shared_device(comp->dev));
		if (domain == NULL) {
			DDPPR_ERR("%s, aod-scp iommu_get_domain for MTK_OVL_EXDMA fail\n", __func__);
			return;
		}

		ret = iommu_map(domain,
				ROUNDUP(scp_aod_mem_pa + AOD_SCP_PICTURE_OFFSET, PAGE_SIZE),
				ROUNDUP(scp_aod_mem_pa + AOD_SCP_PICTURE_OFFSET, PAGE_SIZE),
				ROUNDUP(scp_aod_mem_size, PAGE_SIZE),
				IOMMU_READ | IOMMU_WRITE, GFP_KERNEL);
		if (ret < 0)
			DDPPR_ERR("%s, aod-scp iommu_map fail\n", __func__);

		DDPMSG("%s: set 0x%llx as pa2va for exdma success\n",
			__func__, ROUNDUP(scp_aod_mem_pa + AOD_SCP_PICTURE_OFFSET, PAGE_SIZE));
	}

	frame0 = (void *)scp_sh_mem;
	frame0->ulps_wakeup_prd = ulps_wakeup_prd;
	aod_scp_ulps_wakeup_prd = ulps_wakeup_prd;
	DDPMSG("%s ulps_wakeup_prd %d %d\n",
		__func__, frame0->ulps_wakeup_prd, aod_scp_ulps_wakeup_prd);

	size = ARRAY_SIZE(module_list);

	for (i = 0; i < size; i++) {
		//DDPMSG("%s: %s base: 0x%x ,offset: 0x%x\n", __func__, module_list[i].module_name,
			//module_list[i].base, module_list[i].offset);

		if (!module_list[i].size)
			continue;

		va = ioremap(module_list[i].base, module_list[i].size);

		if (!va) {
			DDPMSG("%s ioremap error\n", __func__);
			return;
		}

		if(!strcmp("spr0", module_list[i].module_name)) {
			mtk_drm_spr_backup(crtc, scp_get_reserve_mem_phys, scp_get_reserve_mem_virt,
				module_list[i].offset, module_list[i].size);
			continue;
		}
		if(!strcmp("oddmr0", module_list[i].module_name)) {
			mtk_drm_dmr_backup(crtc, scp_get_reserve_mem_phys, scp_get_reserve_mem_virt,
				module_list[i].offset, module_list[i].size);
			continue;
		}
		module_base = (char *)va;

		bkup_buf = scp_sh_mem + module_list[i].offset;
		memcpy(bkup_buf, module_base, module_list[i].size);

		iounmap(va);
	}

	DDPMSG("%s, bl: %lld, fps: %d\n", __func__, mtk_get_cur_backlight(crtc), CFG_DISPLAY_VREFRESH);
	mtk_drm_dbi_backup(crtc, scp_get_reserve_mem_phys,
		scp_get_reserve_mem_virt, scp_get_reserve_mem_size,
		AOD_TEMP_BACKLIGHT, CFG_DISPLAY_VREFRESH, 26);

	AOD_STAT_SET(AOD_STAT_CONFIGED);
}

void mtk_module_backup_setup(struct device_node *node)
{
	uint16_t i, size = ARRAY_SIZE(module_list);
	char *name;
	struct of_phandle_args args;
	struct platform_device *module_pdev;
	struct resource *res = NULL;

	for (i = 0; i < size; i++) {
		name = module_list[i].module_name;

		if (!of_property_read_bool(node, name))
			continue;

		if (of_parse_phandle_with_fixed_args(node, name, 1, 0, &args)) {
			DDPMSG("%s: [%d] invalid property content\n", __func__, i);
			continue;
		}

		module_pdev = of_find_device_by_node(args.np);
		if (!module_pdev) {
			DDPMSG("%s: [%d] can't find module node\n", __func__, i);
			continue;
		}

		res = platform_get_resource(module_pdev, IORESOURCE_MEM, 0);

		module_list[i].base = (unsigned int)res->start;
		module_list[i].size = args.args[0];

		DDPMSG("%s: [%d] 0x%x, 0x%x\n", __func__,
			i, module_list[i].base, module_list[i].size);
	}
}

void mtk_prepare_config_map(void)
{
	char *scp_sh_mem;
	struct disp_frame_config *frame0;
	struct disp_input_config *input[6];
	int i;
	unsigned int dst_h = 0, dst_w = 0;

	if (!AOD_STAT_MATCH(AOD_STAT_CONFIGED | AOD_STAT_ACTIVE))
		DDPMSG("[AOD] %s: invalid state:0x%x\n", __func__, aod_state);

	scp_sh_mem = (char *)scp_get_reserve_mem_virt(SCP_AOD_MEM_ID);

	/* prepare frame config map */
	frame0 = (void *)scp_sh_mem;
	scp_sh_mem += sizeof(struct disp_frame_config);
	input[0] = (void *)scp_sh_mem;
	scp_sh_mem += sizeof(struct disp_input_config);
	input[1] = (void *)scp_sh_mem;
	scp_sh_mem += sizeof(struct disp_input_config);
	input[2] = (void *)scp_sh_mem;
	scp_sh_mem += sizeof(struct disp_input_config);
	input[3] = (void *)scp_sh_mem;
	scp_sh_mem += sizeof(struct disp_input_config);
	input[4] = (void *)scp_sh_mem;

	dst_w = 180;
	dst_h = 180;

	memset(input[0], 0, sizeof(struct disp_input_config));
	input[0]->layer	= 0;
	input[0]->layer_en	= 0;	/* for background color */
	input[0]->fmt		= OVL_INPUT_FORMAT_BGRA8888;
	input[0]->addr		= scp_aod_mem_pa + AOD_SCP_PICTURE_OFFSET;
	input[0]->src_x		= 0;
	input[0]->src_y		= 0;
	input[0]->src_pitch = CFG_DISPLAY_ALIGN_WIDTH*4;
	input[0]->dst_x		= 0;
	input[0]->dst_y		= 0;
	input[0]->dst_w		= CFG_DISPLAY_WIDTH;
	input[0]->dst_h		= CFG_DISPLAY_HEIGHT;
	input[0]->aen		= 1;
	input[0]->alpha		= 0xff;
	input[0]->time_digit = 0;
	input[0]->blend_mode = BLD_MODE_PREMULTIPLIED;

	memcpy(input[1], input[0], sizeof(struct disp_input_config));
	input[1]->layer	= 1;
	input[1]->layer_en	= 1;
	input[1]->addr	= 0x0;
	input[1]->src_pitch = dst_w * 4;
	input[1]->dst_x		= 120 * (2 * (input[1]->layer - 1) + 1);
	input[1]->dst_y		= 400;
	input[1]->dst_w		= dst_w;
	input[1]->dst_h		= dst_h;
	input[1]->time_digit = MINUTE_TENS;

	memcpy(input[2], input[1], sizeof(struct disp_input_config));
	input[2]->layer	= 2;
	input[2]->layer_en	= 1;
	input[2]->dst_x		= 120 * (2 * (input[2]->layer - 1) + 1);
	input[2]->time_digit = MINUTES;

	memcpy(input[3], input[1], sizeof(struct disp_input_config));
	input[3]->layer	= 3;
	input[3]->layer_en	= 1;
	input[3]->dst_x		= 120 * (2 * (input[3]->layer - 1) + 1);
	input[3]->time_digit  = SECOND_TENS;

	memcpy(input[4], input[1], sizeof(struct disp_input_config));
	input[4]->layer	= 4;
	input[4]->layer_en	= 1;
	input[4]->dst_x		= 120 * (2 * (input[4]->layer - 1) + 1);
	input[4]->time_digit = SECONDS;

	frame0->interval = 1000;
	frame0->ulps_wakeup_prd = aod_scp_ulps_wakeup_prd;
	frame0->frame_h = CFG_DISPLAY_HEIGHT;
	frame0->frame_w = CFG_DISPLAY_WIDTH;

	/* PA for SCP. */
	frame0->layer_info_addr[0] = scp_aod_mem_pa + sizeof(struct disp_frame_config);
	frame0->layer_info_addr[1] = frame0->layer_info_addr[0] + sizeof(struct disp_input_config);
	frame0->layer_info_addr[2] = frame0->layer_info_addr[1] + sizeof(struct disp_input_config);
	frame0->layer_info_addr[3] = frame0->layer_info_addr[2] + sizeof(struct disp_input_config);
	frame0->layer_info_addr[4] = frame0->layer_info_addr[3] + sizeof(struct disp_input_config);
	frame0->layer_info_addr[5] = 0x0;

	for (i = 0; i < 10; i++)
		frame0->digits_addr[i] = input[0]->addr + DIGITS_OFFSET + AOD_DIGIT_SZ * i;

}

int mtk_aod_scp_get_time(void)
{
	struct rtc_time tm;
	struct rtc_time tm_android;
	struct timespec64 tv = {0};
	struct timespec64 tv_android = {0};

	ktime_get_real_ts64(&tv);
	tv_android = tv;
	rtc_time64_to_tm(tv.tv_sec, &tm);
	tv_android.tv_sec -= (uint64_t)sys_tz.tz_minuteswest * 60;
	rtc_time64_to_tm(tv_android.tv_sec, &tm_android);

	DDPMSG("%s: UTC %02d:%02d:%02d, Android %02d:%02d:%02d (tz_minuteswest %02d)\n",
		__func__, tm.tm_hour, tm.tm_min, tm.tm_sec,
		tm_android.tm_hour, tm_android.tm_min, tm_android.tm_sec, sys_tz.tz_minuteswest);

	return sys_tz.tz_minuteswest;
}

static DEFINE_MUTEX(spm_sema_lock);
static void __iomem *spm_base;
#define OFST_M2_SCP 0x6A4
#define OFST_M6_AP  0x6B4
#define KEY_HOLE    BIT(1)

/* API: use SPM HW semaphore to protect reading MFG_TOP_CFG */
int mtk_aod_scp_set_semaphore(bool lock)
{
	int i = 0;
	bool key = false;
	void __iomem *SPM_SEMA_AP = NULL, *SPM_SEMA_SCP = NULL;

	if (!spm_base) {
		DDPPR_ERR("%s, invalid spm base\n", __func__);
		goto fail;
	}

	SPM_SEMA_AP = spm_base + OFST_M6_AP;
	SPM_SEMA_SCP = spm_base + OFST_M2_SCP;

	mutex_lock(&spm_sema_lock);

	key = ((readl(SPM_SEMA_AP) & KEY_HOLE) == KEY_HOLE);
	if (key == lock) {
		DDPINFO("%s, skip %s sema\n", __func__, lock ? "get" : "put");
		mutex_unlock(&spm_sema_lock);
		return 1;
	}

	if (lock) {
		do {
			/* 10ms timeout */
			if (unlikely(++i > 1000))
				goto fail;
			writel(KEY_HOLE, SPM_SEMA_AP);
			udelay(10);
		} while ((readl(SPM_SEMA_AP) & KEY_HOLE) != KEY_HOLE);
	} else {
		writel(KEY_HOLE, SPM_SEMA_AP);
		do {
			/* 10ms timeout */
			if (unlikely(++i > 1000))
				goto fail;
			udelay(10);
		} while (readl(SPM_SEMA_AP) & KEY_HOLE);
	}

	mutex_unlock(&spm_sema_lock);
	return 1;

fail:
	DDPPR_ERR("%s: %s sema:0x%lx/0x%lx fail(0x%x), retry:%d\n",
		__func__, lock ? "get" : "put", (unsigned long)SPM_SEMA_AP,
		(unsigned long)SPM_SEMA_SCP, readl(SPM_SEMA_AP), i);
	return 0;
}

int mtk_aod_scp_set_semaphore_noirq(bool lock)
{
	int i = 0;
	bool key = false;
	void __iomem *SPM_SEMA_AP = NULL, *SPM_SEMA_SCP = NULL;

	if (!spm_base) {
		DDPPR_ERR("%s, invalid spm base\n", __func__);
		goto fail;
	}

	SPM_SEMA_AP = spm_base + OFST_M6_AP;
	SPM_SEMA_SCP = spm_base + OFST_M2_SCP;

	//mutex_lock(&spm_sema_lock);

	key = ((readl(SPM_SEMA_AP) & KEY_HOLE) == KEY_HOLE);
	if (key == lock) {
		DDPINFO("%s, skip %s sema\n", __func__, lock ? "get" : "put");
		//mutex_unlock(&spm_sema_lock);
		return 1;
	}

	if (lock) {
		do {
			/* 40ms timeout */
			if (unlikely(++i > 4000))
				goto fail;
			writel(KEY_HOLE, SPM_SEMA_AP);
			udelay(10);
		} while ((readl(SPM_SEMA_AP) & KEY_HOLE) != KEY_HOLE);
	} else {
		writel(KEY_HOLE, SPM_SEMA_AP);
		do {
			/* 10ms timeout */
			if (unlikely(++i > 1000))
				goto fail;
			udelay(10);
		} while (readl(SPM_SEMA_AP) & KEY_HOLE);
	}

	//mutex_unlock(&spm_sema_lock);

	return 1;
fail:
	DDPPR_ERR("%s: %s sema:0x%lx/0x%lx fail(0x%x), retry:%d\n",
		__func__, lock ? "get" : "put", (unsigned long)SPM_SEMA_AP,
		(unsigned long)SPM_SEMA_SCP, readl(SPM_SEMA_AP), i);
	return 0;
}

int mtk_aod_scp_ipi_send(int value)
{
	unsigned int retry_cnt = 0;
	int ret;

	DDPMSG("%s+\n", __func__);

	if (value == 0)
		value = mtk_aod_scp_get_time();

	for (retry_cnt = 0; retry_cnt <= 10; retry_cnt++) {
		ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_SCP_AOD,
					0, &value, 1, 0);

		if (ret == IPI_ACTION_DONE) {
			DDPMSG("%s ipi send msg done\n", __func__);
			break;
		}
	}

	if (ret != IPI_ACTION_DONE)
		DDPMSG("%s ipi send msg fail:%d\n", __func__, ret);

	return ret;
}

int mtk_aod_scp_doze_update(int doze)
{
	if (!AOD_STAT_MATCH(AOD_STAT_ENABLE))
		return 0;

	if (doze) {
		mtkfb_set_backlight_level_AOD(AOD_TEMP_BACKLIGHT);
		//mtk_aod_scp_set_BW();

		AOD_STAT_SET(AOD_STAT_ACTIVE);
		mtk_prepare_config_map();

#if (AO_MODE)
		DDPMSG("%s: before doze suspend, trigger aod scp on!\n", __func__);
		mtk_aod_scp_set_semaphore_noirq(0);
		mtk_aod_scp_ipi_send(0);
		mdelay(10000);
		DDPMSG("%s: after mdelay, trigger aod scp off!\n", __func__);
		mtk_aod_scp_ipi_send(1);
		mtk_aod_scp_set_semaphore_noirq(1);
	} else {
		DDPMSG("%s: not in doze mode, notify aod scp off!\n", __func__);
		mtk_aod_scp_ipi_send(1);
		mtk_aod_scp_set_semaphore_noirq(1);
		AOD_STAT_CLR(AOD_STAT_ACTIVE);
	}
#else
	} else
		AOD_STAT_CLR(AOD_STAT_ACTIVE);
#endif

	return 0;
}

static int mtk_aod_scp_recv_handler(unsigned int id, void *prdata, void *data, unsigned int len)
{
	DDPMSG("%s\n", __func__);

	return 0;
}

static int mtk_aod_scp_ipi_register(void)
{
	int ret;

	ret = mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_AOD,
			(void *)mtk_aod_scp_recv_handler, NULL, &aod_scp_msg);

	if (ret != IPI_ACTION_DONE)
		DDPMSG("%s resigter ipi fail: %d\n", __func__, ret);
	else
		DDPMSG("%s register ipi done\n", __func__);

	return ret;
}

static int mtk_aod_scp_get_scp_show_count(void)
{
	/* TODO: current mtk AOD_SCP do not sync status with
	 * AOD_CPU, need implement the sync mechanism and
	 * this API.
	 * If customer want to use this API, please implement
	 * this.
	 */
	return 0;
}

static void mtk_aod_scp_clear_scp_show_count(void)
{
	/* TODO: current mtk AOD_SCP do not sync status with
	 * AOD_CPU, need implement the sync mechanism and
	 * this API.
	 * If customer want to use this API, please implement
	 * this.
	 */
}

static void mtk_aod_scp_set_is_ddic_spr(bool is_is_ddic_spr)
{
	/* TODO: current mtk AOD_SCP do not sync status with
	 * AOD_CPU, need implement the sync mechanism and
	 * this API.
	 * If customer want to use this API, please implement
	 * this.
	 */
}

static void prepare_aod_scp_picture(void *addr_va)
{
	char *dest;

	DDPMSG("%s: prepare aod-scp demo picture to 0x%llx\n", __func__, (uint64_t)addr_va);
	dest = addr_va + DIGITS_OFFSET;
	memcpy(dest, &digit_0_raw, sizeof(digit_0_raw));

	dest += sizeof(digit_0_raw);
	memcpy(dest, &digit_1_raw, sizeof(digit_1_raw));

	dest += sizeof(digit_1_raw);
	memcpy(dest, &digit_2_raw, sizeof(digit_2_raw));

	dest += sizeof(digit_2_raw);
	memcpy(dest, &digit_3_raw, sizeof(digit_3_raw));

	dest += sizeof(digit_3_raw);
	memcpy(dest, &digit_4_raw, sizeof(digit_4_raw));

	dest += sizeof(digit_4_raw);
	memcpy(dest, &digit_5_raw, sizeof(digit_5_raw));

	dest += sizeof(digit_5_raw);
	memcpy(dest, &digit_6_raw, sizeof(digit_6_raw));

	dest += sizeof(digit_6_raw);
	memcpy(dest, &digit_7_raw, sizeof(digit_7_raw));

	dest += sizeof(digit_7_raw);
	memcpy(dest, &digit_8_raw, sizeof(digit_8_raw));

	dest += sizeof(digit_8_raw);
	memcpy(dest, &digit_9_raw, sizeof(digit_9_raw));
}

static int mtk_aod_scp_probe(struct platform_device *pdev)
{
	struct device_node *smp_node = NULL;
	struct platform_device *spm_pdev = NULL;
	struct resource *res = NULL;
	static unsigned int aod_scp_pic;
	const char *status = NULL;
	int ret = 0;

	DDPMSG("%s+\n", __func__);

	ret = of_property_read_string(pdev->dev.of_node, "status", &status);
	if (ret < 0 || strncmp(status, "okay", sizeof("okay"))) {
		DDPMSG("aod scp not enabled\n");
	} else {
		aod_state = 0;
		aod_scp_pic = 0;

		mtk_aod_scp_ipi_register();

		aod_scp_cb.send_ipi = mtk_aod_scp_doze_update;
		aod_scp_cb.module_backup = mtk_module_backup;
		aod_scp_cb.get_scp_show_count = mtk_aod_scp_get_scp_show_count;
		aod_scp_cb.clear_scp_show_count = mtk_aod_scp_clear_scp_show_count;
		aod_scp_cb.set_is_ddic_spr = mtk_aod_scp_set_is_ddic_spr;
		mtk_aod_scp_ipi_init(&aod_scp_cb);

		smp_node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
		if (smp_node) {
			spm_pdev = of_find_device_by_node(smp_node);
			of_node_put(smp_node);
			if (!spm_pdev) {
				DDPPR_ERR("%s: invalid spm device\n", __func__);
				return 0;
			}

			res = platform_get_resource(spm_pdev, IORESOURCE_MEM, 0);

			spm_base = devm_ioremap(&spm_pdev->dev, res->start, resource_size(res));
			if (unlikely(!spm_base)) {
				DDPPR_ERR("%s: fail to ioremap SPM: 0x%llx", __func__, res->start);
				return 0;
			}
			//sent AOD SCP sema to vsidp
			_mtk_sent_aod_scp_sema(spm_base + OFST_M6_AP);
		}

		mtk_module_backup_setup(pdev->dev.of_node);

		AOD_STAT_SET(AOD_STAT_ENABLE);

		mtk_aod_scp_set_semaphore_noirq(1);

		scp_aod_mem_pa = scp_get_reserve_mem_phys(SCP_AOD_MEM_ID);
		scp_aod_mem_va = scp_get_reserve_mem_virt(SCP_AOD_MEM_ID);
		scp_aod_mem_size = scp_get_reserve_mem_size(SCP_AOD_MEM_ID);

		DDPMSG("%s: aod(id=%d) in scp rsv mem 0x%llx(0x%llx), size 0x%llx\n",
			__func__, SCP_AOD_MEM_ID, scp_aod_mem_va, scp_aod_mem_pa, scp_aod_mem_size);

		if (of_property_read_u32(pdev->dev.of_node, "aod-scp-pic", &aod_scp_pic)) {
			DDPMSG("%s: aod_scp_pic not define, not to prepare demo picture\n", __func__);
		} else {
			DDPMSG("%s find aod_scp_pic(%d) in FDT\n", __func__, aod_scp_pic);
			if (aod_scp_pic) {
				if (scp_aod_mem_size >= (AOD_SCP_PICTURE_OFFSET + DIGITS_OFFSET + AOD_DIGIT_SZ*10)) {
					prepare_aod_scp_picture((void *)(scp_aod_mem_va + AOD_SCP_PICTURE_OFFSET));
					DDPMSG("%s prepare demo picture done\n", __func__);
				} else {
					DDPMSG("%s: can't prepare demo picture, check dts 'scp-mem-tbl' size\n",
						__func__);
					AOD_STAT_CLR(AOD_STAT_ENABLE);
				}
			}
		}
	}

	DDPMSG("%s-\n", __func__);

	return 0;
}

static int aod_scp_suspend(struct device *dev)
{
	//DDPMSG("%s+\n", __func__);
	return 0;
}

static int aod_scp_resume(struct device *dev)
{
	//DDPMSG("%s+\n", __func__);
	return 0;
}

static int aod_scp_suspend_noirq(struct device *dev)
{
	if (AOD_STAT_MATCH(AOD_STAT_ACTIVE)) {
		mtk_aod_scp_ipi_send(0);
		mtk_oddmr_scp_status(1);
	}

	mtk_aod_scp_set_semaphore_noirq(0);
	return 0;
}

static int aod_scp_resume_noirq(struct device *dev)
{
	mtk_aod_scp_ipi_send(1);
	if (mtk_aod_scp_set_semaphore_noirq(1) == 0)
		DDPAEE("[AOD]:failed to get semaphore\n");
	mtk_oddmr_scp_status(0);
	return 0;
}

static const struct dev_pm_ops aod_scp_pm_ops = {
	.suspend = aod_scp_suspend,
	.resume = aod_scp_resume,
	.suspend_noirq = aod_scp_suspend_noirq,
	.resume_noirq = aod_scp_resume_noirq,
};


static const struct of_device_id mtk_aod_scp_of_match[] = {
	{.compatible = "mediatek,aod_scp",},
	{},
};

struct platform_driver mtk_aod_scp_driver = {
	.probe = mtk_aod_scp_probe,
	.driver = {
				.name = "mtk-aod-scp",
				.of_match_table = mtk_aod_scp_of_match,
				.pm = &aod_scp_pm_ops,
			},
};

static int __init mtk_aod_scp_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&mtk_aod_scp_driver);
	if (ret < 0)
		DDPPR_ERR("Failed to register aod driver: %d\n", ret);

	return 0;
}

static void __exit mtk_aod_scp_exit(void)
{
}

module_init(mtk_aod_scp_init);
module_exit(mtk_aod_scp_exit);

MODULE_AUTHOR("Ahsin Chen <ahsin.chen@mediatek.com>");
MODULE_DESCRIPTION("Mediatek AOD-SCP");
MODULE_LICENSE("GPL v2");
