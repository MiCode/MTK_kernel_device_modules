// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Sram Manager
 * *
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Yu-Chen.Liu <yu-chen.liu@mediatek.com>
 */

#include <linux/list.h>

/**
 * mtk_usb_sram_region - information of requested space
 * @phys: physical address of space
 * @size: size of space
 * @list: private field for management
 */
struct mtk_usb_sram_region {
	void *virt;
	dma_addr_t phys;
	size_t size;
	struct list_head list;
	int from;
	int to;
};

/**
 * mtk_usb_sram_allocate - request space on usb sram
 * @size: desire size
 * @align: address alignment
 * @start: start index of block
 *
 * Return poniter to sram block on success or NULL on failure
 */
extern struct mtk_usb_sram_region *mtk_usb_sram_allocate(
	unsigned int size, int align, int start);

/**
 * mtk_usb_sram_free - give back requested space
 * @physical: physical address of requested space
 *
 * Return 0 if block was actually freed or non-zero on failure
 */
extern int mtk_usb_sram_free(dma_addr_t physical);

/**
 * mtk_usb_sram_free - give back requested space
 * @virtual: virtual address of requested space
 *
 * Return 0 if block was actually freed or non-zero on failure
 */
extern int mtk_usb_sram_free_virt(void *virtual);