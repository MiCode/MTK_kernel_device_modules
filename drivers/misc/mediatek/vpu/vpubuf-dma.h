/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef __VPU_DMA_H__
#define __VPU_DMA_H__

#include <linux/platform_device.h>
#include <linux/scatterlist.h>

#define APU_BMAP_NAME_LEN 16
#define is_au_align(ab, val) (!((val) & (ab->au - 1)))

struct apu_bmap {
	/* input */
	uint32_t start;
	uint32_t end;
	uint32_t au;  // allocation unit (in bytes)
	unsigned long align_mask;
	char name[16];

	// output
	uint32_t size;
	unsigned long *b;     // bitmap
	unsigned long nbits;  // number of bits
	spinlock_t lock;
};

int apu_bmap_init(struct apu_bmap *ab, const char *name);
void apu_bmap_exit(struct apu_bmap *ab);

#endif

