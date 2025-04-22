/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_APU_HDS_H__
#define __MTK_APU_HDS_H__

#include <linux/device.h>
#include <linux/rpmsg.h>
#include <linux/dma-buf.h>
#include <linux/types.h>
#include <apusys_device.h>

struct apu_hds_device;

/* hds platform related functions */
struct hds_plat_func {
	int (*plat_init)(struct apu_hds_device *hdev);
	void (*plat_deinit)(struct apu_hds_device *hdev);
	int (*cmd_postprocess_late)(struct apu_hds_device *hdev, void *va, uint32_t size,
		uint32_t power_plcy);
};

/* hds related structure */
struct apu_hds_device {
	/* control */
	struct rpmsg_device *rpdev;
	struct rpmsg_endpoint *ept;

	struct apu_sysmem_allocator *allocator;
	struct apu_sysmem_buffer *workbuf;
	struct apu_sysmem_map *workmap;

	bool inited;
	uint32_t pmu_lv;
	uint32_t pmu_tag_en;

	struct mutex power_mtx;
	uint32_t power_cnt;

	/* queried info */
	uint32_t version_hw;
	uint32_t version_date;
	uint32_t version_revision;
	uint32_t init_workbuf_size;
	uint32_t per_cmd_appendix_size;
	uint32_t per_subcmd_appendix_size;

	struct work_struct ipi_wk;
	struct list_head msgs; //for ipi_wk
	struct mutex msg_mtx;

	/* plat func */
	struct hds_plat_func *plat_func;
};

/* hds api */
int apu_hds_dev_init(struct apu_hds_device *hdev);
void apu_hds_dev_deinit(struct apu_hds_device *hdev);
int apu_hds_sysfs_init(void);
void apu_hds_sysfs_deinit(void);
int hds_cmd_init(void);

/* hds log definition */
extern uint32_t g_hds_klog; //hds_procfs.c
extern struct apu_hds_device *g_hdev; //hds_drv.c

enum {
	APU_HDS_LOG_LV_ERROR = 0x1,
	APU_HDS_LOG_LV_WARN = 0x2,
	APU_HDS_LOG_LV_INFO = 0x3,
	APU_HDS_LOG_LV_DEBUG = 0x4,
};

static inline bool hds_debug_on(int lv)
{
	return g_hds_klog >= lv;
}

#define apu_hds_err(x, args...) do { \
	if (hds_debug_on(APU_HDS_LOG_LV_ERROR)) \
		dev_info(&g_hdev->rpdev->dev, "[error] %s/%d " x, __func__, __LINE__, ##args); \
	} while (0)
#define apu_hds_warn(x, args...) do { \
	if (hds_debug_on(APU_HDS_LOG_LV_WARN)) \
		dev_info(&g_hdev->rpdev->dev, "[warn] %s/%d " x, __func__, __LINE__, ##args); \
	} while (0)
#define apu_hds_info(x, args...) do { \
	if (hds_debug_on(APU_HDS_LOG_LV_INFO)) \
		dev_info(&g_hdev->rpdev->dev, "[info] %s/%d " x, __func__, __LINE__, ##args); \
	} while (0)
#define apu_hds_debug(x, args...) do { \
	if (hds_debug_on(APU_HDS_LOG_LV_DEBUG)) \
		dev_info(&g_hdev->rpdev->dev, "[debug] %s/%d " x, __func__, __LINE__, ##args); \
	} while (0)

#endif // __MTK_APU_HDS_H__