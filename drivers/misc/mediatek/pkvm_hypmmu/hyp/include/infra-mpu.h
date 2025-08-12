/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#define MTK_SIP_HYP_IMPU_CONTROL 0xC2000826
#define KEY_SYNC 0xEE6760F0
#define CORE_NUM 8
#define SECURE_REGION_ENABLE 0x1
#define SECURE_REGION_DISABLE 0x2

/*
 *	INFRA_BUF_ENTRY format
 *	page number = PA >> PAGE_SHIFT
 *	 _______________________________________
 *	|   sr_info  | page order | page number |
 *	|____________|____________|_____________|
 *	31         28 27        24 23          0
 */
#define SRINFO_SHIFT (28UL)
#define ORDER_SHIFT (24UL)
#define INFRA_BUF_ENTRY(pa, page_order, sr_info) \
	((pa >> PAGE_SHIFT) | ((u32)(page_order) << ORDER_SHIFT) | \
	(((u32)(sr_info) & 0xf) << SRINFO_SHIFT))

struct infra_buf {
	u64 buf_paddr;
	u32 buf_size;
	void *buf_ptr;
	u32 counter;
};

extern struct infra_buf infra_shared_buf[CORE_NUM];
