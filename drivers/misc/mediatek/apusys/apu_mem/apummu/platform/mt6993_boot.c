// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <asm/io.h>
#include <linux/printk.h>
#include "apummu_boot.h"

#define L1_ARRAY_SIZE			(20)
#define L1_PAGE_SIZE			(128 * L1_ARRAY_SIZE)
#define TBL_SRAM_SIZE			(0x5C00 - L1_PAGE_SIZE)
#define TBL_SIZE			(0x154)
#define TBL_NUM				(TBL_SRAM_SIZE / TBL_SIZE)

enum rsv_vsid {
	RSV_VSID_UPRV = 0,
	RSV_VSID_LOGGER,
	RSV_VSID_APMCU,
	RSV_VSID_GPU,
	RSV_VSID_MDLA,
	RSV_VSID_MVPU0,
	RSV_VSID_MVPU1,
	RSV_VSID_CE,
	RSV_VSID_MAX
};

#define VSID_RSV_LAST			(TBL_NUM - 1)
#define VSID_RSV_CE			(VSID_RSV_LAST - RSV_VSID_CE)
#define VSID_RSV_MVPU1			(VSID_RSV_LAST - RSV_VSID_MVPU1)
#define VSID_RSV_MVPU0			(VSID_RSV_LAST - RSV_VSID_MVPU0)
#define VSID_RSV_MDLA			(VSID_RSV_LAST - RSV_VSID_MDLA)
#define VSID_RSV_GPU			(VSID_RSV_LAST - RSV_VSID_GPU)
#define VSID_RSV_APMCU			(VSID_RSV_LAST - RSV_VSID_APMCU)
#define VSID_RSV_LOGGER			(VSID_RSV_LAST - RSV_VSID_LOGGER)
#define VSID_RSV_UPRV			(VSID_RSV_LAST - RSV_VSID_UPRV)

#define VSID_RSV_START			(VSID_RSV_CE)
#define VSID_RSV_END			(VSID_RSV_UPRV + 1)
#define VSID_RSV_NUM			(VSID_RSV_END - VSID_RSV_START)

#define CMU_TOP_BASE			(0x19410000)
#define UPRV_TCU_BASE			(0x19260000)

#define inf_printf(fmt, args...)	pr_info("[ammu][inf] " fmt, ##args)
#define dbg_printf(fmt, args...)	pr_info("[ammu][dbg][%s] " fmt, __func__, ##args)
#define err_printf(fmt, args...)	pr_info("[ammu][err][%s] " fmt, __func__, ##args)

static void __iomem *cmut_base;
static void __iomem *uprv_base;


static int __ioremap(void)
{
	int ret;

	cmut_base = ioremap(CMU_TOP_BASE,  0x7000);
	if (!cmut_base) {
		ret = -ENOMEM;
		goto exit;
	}

	uprv_base = ioremap(UPRV_TCU_BASE, 0x1000);
	if (!uprv_base) {
		ret = -ENOMEM;
		goto ucmu;
	}

	return 0;

ucmu:
	iounmap(cmut_base);
exit:
	return ret;
}

static void __iounmap(void)
{
	iounmap(uprv_base);
	iounmap(cmut_base);
}

static u32 tbl_offset(u32 vsid)
{
#define DESC_SRAM	(0x1400 + L1_PAGE_SIZE)

	u32 val;

	/**
	 * 0x1400 + L1_PAGE_SIZE
	 * VSID desc sram
	 *   vsid table for vsid 0
	 *   vsid table for vsid 1
	 *   ...
	 */
	val  = DESC_SRAM;
	val += vsid * TBL_SIZE;

	return val;
}

static u32 seg_offset(u32 vsid, u32 seg_idx)
{
	u32 val;

	/**
	 * 0x0004
	 *   segment  0
	 * 0x0014
	 *   segment  1
	 * ...
	 * 0x00B4
	 *   segment 11
	 */
	val  = tbl_offset(vsid) + 0x0004;
	val += seg_idx * 0x10;

	return val;
}

static u32 tbl_address(u32 vsid)
{
#define DESC_SRAM_START	(0x0400 + L1_PAGE_SIZE)

	u32 val;

	/**
	 * 0x0400 + L1_PAGE_SIZE
	 * VSID desc sram
	 *   vsid table for vsid 0
	 *   vsid table for vsid 1
	 *   ...
	 */
	val  = DESC_SRAM_START;
	val += vsid * TBL_SIZE;

	return val;
}

static u32 page_offset(u32 vsid, u32 page_sel)
{
	u32 val;

	/**
	 * 0x00C4
	 *   page array 0
	 * 0x00DC
	 *   page array 1
	 * ...
	 * 0x13C
	 *   page array 5
	 */
	val  = tbl_offset(vsid) + 0x00C4;
	val += page_sel * 0x18;

	return val;
}

static int sram_config(void)
{
#define DESC_INDEX	0x1000

	u32 i;

	/**
	 * config reserved vsid: START~END
	 */
	for (i = VSID_RSV_START; i < VSID_RSV_END; i++) {
		u32 ofs, val, j;

		/**
		 * 0x1000
		 * VSID desc index
		 *   0x0000 -> VSID   0 descriptor address
		 *   0x0004 -> VSID   1 descriptor address
		 *   ...
		 *   0x03FC -> VSID 255 descriptor address
		 */
		ofs = DESC_INDEX + i * 4;
		val = tbl_address(i);
		iowrite32(val, cmut_base + ofs);

		/**
		 * 0x0000
		 *   [11:00]: seg_vld[11:0]
		 *   [17:12]: RH_enable[5:0]
		 *   [23:18]: RH indirect[5:0]
		 */
		ofs = tbl_offset(i) + 0x0000;
		val = 0x0;
		iowrite32(val, cmut_base + ofs);

		for (j = 0; j < 6; j++) {
			/**
			 * 0x0014
			 *   [05:00]: Page enable num
			 */
			ofs = page_offset(i, j) + 0x0014;
			val = 0x0;
			iowrite32(val, cmut_base + ofs);
		}
	}

	return 0;
}

static int ammu_enable(void)
{
#define CMU_CON		0x0000

	u32 ofs, val;

	/**
	 * 0x0000
	 * cmu_con
	 *   [00:00]: APU_MMU_enable
	 *   [01:01]: tcu_apb_dbg_en
	 *   [02:02]: tcu_secure_chk_en
	 *   [03:03]: CMU_socket_dcm_en
	 *   [04:04]: CMU_sram_wrapper_dcm_en
	 *   [05:05]: CMU_sram_auto_clr_dcm_en
	 *   [06:06]: delegate_dcm_en
	 *   [10:07]: rrmx_dcm_en
	 */
	ofs  = CMU_CON;
	val  = ioread32(cmut_base + ofs);
	val |= 0x1;
	iowrite32(val, cmut_base + ofs);

	return 0;
}

static int boot_init(void)
{
	int ret;

	ret = sram_config();
	if (ret)
		goto exit;

	ret = ammu_enable();
	if (ret)
		goto exit;
exit:
	return ret;
}

#define SID_NO_SEC	0x9
#define SID_01_04G	0xB

static int add_seg(u32 vsid, u32 seg_idx, u32 in_adr, u32 map_adr,
	u32 page_len, u32 domain, u32 ns, u32 sid)
{
	u32 tbl, seg, val;

	// init
	seg = seg_offset(vsid, seg_idx);

	/**
	 * 0x0000
	 *   [02:00]: Page select
	 *   [07:07]: NS
	 *   [31:08]: Input address [35:12]
	 */
	val  = (0x7)          << 0;
	val |= (ns ? 1 : 0)   << 7;
	val |= (in_adr >> 12) << 8;
	iowrite32(val, cmut_base + seg + 0x0000);

	/**
	 * 0x0004
	 *   [07:00]: smmu_sid
	 *   [31:08]: Mapped address base [35:12]
	 */
	val  = (sid & 0xff)    << 0;
	val |= (map_adr >> 12) << 8;
	iowrite32(val, cmut_base + seg + 0x0004);

	/**
	 * 0x0008
	 *   [15:12]: Domain
	 *   [23:16]: smmu_ssid
	 *   [24:24]: smmu_sec (axmmusecsid)
	 *   [25:25]: SSID valid (axmmussidv)
	 */
	val  = (domain & 0xf) << 12;
	val |= (0x0)          << 16;
	val |= (0x0)          << 24;
	val |= (0x0)          << 25;
	iowrite32(val, cmut_base + seg + 0x0008);

	/**
	 * 0x000C
	 *   [03:00]: Page length
	 */
	val  = (page_len & 0xf) << 0;
	iowrite32(val, cmut_base + seg + 0x000C);

	/**
	 * 0x0000
	 *   [11:00]: seg_vld[11:0]
	 *   [17:12]: RH_enable[5:0]
	 *   [23:18]: RH indirect[5:0]
	 */
	tbl  = tbl_offset(vsid);
	val  = ioread32(cmut_base + tbl + 0x0000);
	val |= 0x1 << seg_idx;
	iowrite32(val, cmut_base + tbl + 0x0000);

	return 0;
}

static int enable_vsid(u32 vsid)
{
#define VSID_ENABLE	0x0050
#define VSID_VALID	0x00B0

	u32 ofs, val;

	/**
	 * 0x0050
	 * VSID_enable0_set
	 *   [31:00]: the_VSID31_0_enable_set_register W1S
	 * VSID_enable1_set
	 * ...
	 */
	ofs = VSID_ENABLE + (vsid >> 5) * 4;
	val = 0x1 << (vsid & 0x1f);
	iowrite32(val, cmut_base + ofs);

	/**
	 * 0x00B0
	 * VSID_valid0_set
	 *   [31:00]: the_VSID31_0_valid_set_register W1S
	 * VSID_valid1_set
	 * ...
	 */
	ofs = VSID_VALID + (vsid >> 5) * 4;
	val = 0x1 << (vsid & 0x1f);
	iowrite32(val, cmut_base + ofs);

	return 0;
}

static int add_rv_map(u32 map_adr)
{
	u32 domain = 7, ns = 1;
	int ret;

	ret = add_seg(VSID_RSV_UPRV, 0, 0, map_adr, eAPUMMU_PAGE_LEN_1MB, domain, ns, SID_NO_SEC);
	if (ret)
		goto exit;

	ret = add_seg(VSID_RSV_UPRV, 1, 0, 0, eAPUMMU_PAGE_LEN_512MB, domain, ns, SID_NO_SEC);
	if (ret)
		goto exit;

	ret = add_seg(VSID_RSV_UPRV, 2, 0, 0, eAPUMMU_PAGE_LEN_1GB, domain, ns, SID_NO_SEC);
	if (ret)
		goto exit;

	ret = add_seg(VSID_RSV_UPRV, 3, 0, 0, eAPUMMU_PAGE_LEN_4GB, domain, ns, SID_01_04G);
	if (ret)
		goto exit;

	/* enable */

	ret = enable_vsid(VSID_RSV_UPRV);
	if (ret)
		goto exit;
exit:
	return ret;
}

static int bind_vsid(void *base, u32 vsid, u32 thread)
{
	u32 ofs, val;

	/**
	 * 0x0000
	 * thread0_mapping_table0_0
	 *   [00:00]: vsid_vld
	 *   [01:01]: corid_vld
	 *   [02:02]: vsid_prefetch_trigger
	 *   [10:03]: vsid
	 *   [18:11]: cor_id
	 *   [19:19]: vsid_auto_invalid_en
	 *   [31:20]: rh_info0
	 * 0x0010
	 * thread1_mapping_table0_0
	 * ...
	 */
	ofs  = thread * 0x10;
	val  = (0x1)         <<  0;
	val |= (0x0)         <<  1;
	val |= (vsid & 0xff) <<  3;
	val |= (0x0)         << 11;
	iowrite32(val, base + ofs);

	return 0;
}

static int bind_rv_vsid(u32 thread)
{
	int ret;

	if (thread > 15) {
		err_printf("invalid thread: %d\n", thread);
		ret = -EINVAL;
		goto exit;
	}

	ret = bind_vsid(uprv_base, VSID_RSV_UPRV, thread);
	if (ret)
		goto exit;
exit:
	return ret;
}

static int bind_hds_vsid(void)
{
#define HDS_THREAD_NUM	12
#define HDS_THREAD_MUST	13

	u32 i;
	int ret;

	for (i = 0; i < HDS_THREAD_NUM; i++) {
		ret = bind_vsid(uprv_base, VSID_RSV_UPRV, i);
		if (ret)
			goto exit;
	}

	ret = bind_vsid(uprv_base, VSID_RSV_UPRV, HDS_THREAD_MUST);
exit:
	return ret;
}

static int add_logger_map(u32 in_adr, u32 map_adr, enum eAPUMMUPAGESIZE page_size)
{
	u32 domain = 7, ns = 1;
	int ret;

	ret = add_seg(VSID_RSV_LOGGER, 0, in_adr, map_adr, page_size, domain, ns, SID_NO_SEC);
	if (ret)
		goto exit;

	ret = enable_vsid(VSID_RSV_LOGGER);
	if (ret)
		goto exit;
exit:
	return ret;
}

static int bind_logger_vsid(u32 thread)
{
	int ret;

	if (thread > 15) {
		err_printf("invalid thread: %d\n", thread);
		ret = -EINVAL;
		goto exit;
	}

	ret = bind_vsid(uprv_base, VSID_RSV_LOGGER, thread);
	if (ret)
		goto exit;
exit:
	return ret;
}

static int add_apmcu_map(u32 in_adr, u32 map_adr, enum eAPUMMUPAGESIZE page_size)
{
	u32 domain = 7, ns = 1;
	int ret;

	ret = add_seg(VSID_RSV_APMCU, 0, in_adr, map_adr, page_size, domain, ns, SID_NO_SEC);
	if (ret)
		goto exit;

	ret = enable_vsid(VSID_RSV_APMCU);
	if (ret)
		goto exit;
exit:
	return ret;
}

static int bind_apmcu_vsid(u32 thread)
{
	int ret;

	if (thread > 15) {
		err_printf("invalid thread: %d\n", thread);
		ret = -EINVAL;
		goto exit;
	}

	ret = bind_vsid(uprv_base, VSID_RSV_APMCU, thread);
	if (ret)
		goto exit;
exit:
	return ret;
}

static int mt6993_rv_boot(u32 uP_seg_output, u8 uP_hw_thread,
	u32 logger_seg_output, enum eAPUMMUPAGESIZE logger_page_size,
	u32 XPU_seg_output, enum eAPUMMUPAGESIZE XPU_page_size)
{
	int ret;

	ret = __ioremap();
	if (ret)
		goto exit;

	ret = boot_init();
	if (ret)
		goto umap;

	ret = add_rv_map(uP_seg_output);
	if (ret)
		goto umap;

	/* thread: 0 for normal */
	ret = bind_rv_vsid(uP_hw_thread);
	if (ret)
		goto umap;

	/* thread: 1 for secure */
	ret = bind_rv_vsid(uP_hw_thread + 1);
	if (ret)
		goto umap;

	/* thread: 0~11, 13 for HDS */
	ret = bind_hds_vsid();
	if (ret)
		goto umap;

	ret = add_logger_map(logger_seg_output, logger_seg_output, logger_page_size);
	if (ret)
		goto umap;

	/* thread: 14 for logger */
	ret = bind_logger_vsid(14);
	if (ret)
		goto umap;

	ret = add_apmcu_map(XPU_seg_output, XPU_seg_output, XPU_page_size);
	if (ret)
		goto umap;

	/* thread: 15 for apmcu */
	ret = bind_apmcu_vsid(15);
	if (ret)
		goto umap;
umap:
	__iounmap();
exit:
	return ret;
}

const struct mtk_apu_ammudata mt6993_ammudata = {
	.ops = {
		.rv_boot = mt6993_rv_boot,
	},
};

