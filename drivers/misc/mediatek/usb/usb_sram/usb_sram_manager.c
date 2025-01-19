// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Sram Manager
 * *
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Yu-Chen.Liu <yu-chen.liu@mediatek.com>
 */

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#include "usb_sram.h"
#include "usb_offload.h"

struct usb_sram_block {
	int index;
	bool occupied;
	size_t size;
	dma_addr_t phys;
	struct list_head link;
};

struct mtk_usb_sram {
	struct device *dev;
	void *virt;
	dma_addr_t phys;
	size_t size;

	spinlock_t block_lock;
	size_t block_size;
	int block_num;
	struct usb_sram_block *blocks;

	spinlock_t list_lock;
	struct list_head region_list;
};

#define USB_SRAM_MIN_ORDER  8

static struct mtk_usb_sram *g_manager;

#define INFO_MAX_LEN 500
static char parse_info[INFO_MAX_LEN];

static char *decode_block(struct usb_sram_block *block)
{
	int n;

	n = snprintf(parse_info, INFO_MAX_LEN, "index:%d occupied:%d size:%zu phys:0x%llx",
		block->index, block->occupied, block->size, block->phys);
	parse_info[n < INFO_MAX_LEN ? n : INFO_MAX_LEN - 1] = '\0';

	return parse_info;
}

static char *parse_region_info(struct mtk_usb_sram_region *region)
{
	int n;

	n = snprintf(parse_info, INFO_MAX_LEN, "phys:0x%llx virt:%p size:%zu from:%d",
		region->phys, region->virt, region->size, region->from);
	parse_info[n < INFO_MAX_LEN ? n : INFO_MAX_LEN - 1] = '\0';

	return parse_info;
}

static struct mtk_usb_sram_region *in_region_list(struct mtk_usb_sram *manager, dma_addr_t phy)
{
	struct mtk_usb_sram_region *pos;

	if (list_empty(&manager->region_list))
		return NULL;

	list_for_each_entry(pos, &manager->region_list, list) {
		if (pos->phys == phy)
			return pos;
	}

	return NULL;
}

static struct mtk_usb_sram_region *in_region_list_virt(struct mtk_usb_sram *manager, void *virt)
{
	struct mtk_usb_sram_region *pos;

	if (list_empty(&manager->region_list))
		return NULL;

	list_for_each_entry(pos, &manager->region_list, list) {
		if (pos->virt == virt)
			return pos;
	}

	return NULL;
}

static void dump_group(struct mtk_usb_sram *manager, struct usb_sram_block *block)
{
	struct usb_sram_block *pos;

	dev_dbg(manager->dev, "%s\n", __func__);
	dev_dbg(manager->dev, "%s\n", decode_block(block));


	if (list_empty(&block->link))
		return;

	list_for_each_entry(pos, &block->link, link) {
		dev_dbg(manager->dev, "%s\n", decode_block(pos));
	}

}

static void occupy_block(struct usb_sram_block *block)
{
	block->occupied = true;
}

static void vacate_block(struct usb_sram_block *block)
{
	block->occupied = false;
}

static void init_group(struct usb_sram_block *start, struct usb_sram_block *end)
{
	struct usb_sram_block *cur;

	cur = start;
	occupy_block(cur);
	while (cur != end) {
		cur = cur + 1;
		occupy_block(cur);
		list_add_tail(&cur->link, &start->link);
	}
}

static void deinit_group(struct usb_sram_block *start)
{
	struct usb_sram_block *pos, *next;

	if (!list_empty(&start->link)) {
		list_for_each_entry_safe(pos, next, &start->link, link) {
			vacate_block(pos);
			list_del(&pos->link);
		}
	}
	vacate_block(start);
	INIT_LIST_HEAD(&start->link);
}

struct mtk_usb_sram_region *mtk_usb_sram_allocate(unsigned int size, int align)
{
	struct device *dev;
	struct mtk_usb_sram *manager = g_manager;
	struct usb_sram_block *start, *end, *cur;
	struct mtk_usb_sram_region *region = NULL;
	int request = size, residue;
	int i = 0;
	bool found_block = false;

	if (unlikely(manager == NULL))
		goto error;

	dev = manager->dev;
	if ((size_t)size > manager->size) {
		dev_info(dev, "%s exceed max size, request:%d max:%zu\n",
			__func__, size, manager->size);
		goto error;
	}

	spin_lock(&manager->block_lock);
	while (i < manager->block_num && !found_block) {
		/* set start and end block as current block */
		start = &manager->blocks[i];
		end = &manager->blocks[i];
		cur = &manager->blocks[i];
		residue = request;

		dev_dbg(dev, "start [%s] req:%d align:%d\n", decode_block(start), size, align);

		if (start->occupied)
			goto next_block;

		/* check if address met align requirement */
		if (align > 0 && (start->phys & ((dma_addr_t)(align - 1))) != 0)
			goto next_block;

		/* residue = 0 means that group has sufficient space */
		while (i < manager->block_num && residue > 0) {
			residue -= cur->size;
			dev_dbg(dev, "checking [%s], req:%d res:%d\n", decode_block(cur), request, residue);
			if (cur->occupied)
				goto next_block;
			if (residue <= 0) {
				end = cur;
				found_block = true;
				dev_info(manager->dev, "found block%d ~ block%d\n", start->index, end->index);
				init_group(start, end);
				dump_group(manager, start);
			}
			cur = &manager->blocks[++i];
		}

next_block:
		i++;
	}

	spin_unlock(&manager->block_lock);
	if (found_block) {
		region = kzalloc(sizeof(*region), GFP_KERNEL);
		if (!region) {
			dev_info(manager->dev, "%s can't create region\n", __func__);
			goto error;
		}
		region->from = start->index;
		region->phys = start->phys;
		region->size = (size_t)request;
		region->virt = (void *)ioremap_wc(region->phys, region->size);

		spin_lock(&manager->list_lock);
		list_add_tail(&region->list, &manager->region_list);
		spin_unlock(&manager->list_lock);
	}

error:
	return region;
}
EXPORT_SYMBOL_GPL(mtk_usb_sram_allocate);

static int _mtk_usb_sram_free(struct mtk_usb_sram *manager, struct mtk_usb_sram_region *region)
{
	if (region->from >= manager->block_num)
		return -EINVAL;

	dev_info(manager->dev, "free [%s]\n", parse_region_info(region));

	spin_lock(&manager->block_lock);
	deinit_group(&manager->blocks[region->from]);
	spin_unlock(&manager->block_lock);

	iounmap(region->virt);
	kfree(region);

	return 0;
}

int mtk_usb_sram_free(dma_addr_t physical)
{
	struct mtk_usb_sram *manager = g_manager;
	struct mtk_usb_sram_region *region;

	if (manager == NULL)
		return -EINVAL;

	spin_lock(&manager->list_lock);

	region = in_region_list(manager, physical);
	if (!region) {
		dev_info(manager->dev, "%s can't find phy:%llx\n", __func__, physical);
		return -EINVAL;
	}
	list_del(&region->list);

	spin_unlock(&manager->list_lock);

	return _mtk_usb_sram_free(manager, region);
}
EXPORT_SYMBOL_GPL(mtk_usb_sram_free);

int mtk_usb_sram_free_virt(void *virtual)
{
	struct mtk_usb_sram *manager = g_manager;
	struct mtk_usb_sram_region *region;

	if (manager == NULL)
		return -EINVAL;

	spin_lock(&manager->list_lock);

	region = in_region_list_virt(manager, virtual);
	if (!region) {
		dev_info(manager->dev, "%s can't find virt:%p\n", __func__, virtual);
		return -EINVAL;
	}
	list_del(&region->list);

	spin_unlock(&manager->list_lock);

	return _mtk_usb_sram_free(manager, region);
}
EXPORT_SYMBOL_GPL(mtk_usb_sram_free_virt);

static void *allocate_for_offload(dma_addr_t *phys_addr, unsigned int size, int align)
{
	struct mtk_usb_sram_region *blk;

	blk = mtk_usb_sram_allocate(size, align);
	if (!blk) {
		return NULL;
	}

	*phys_addr = blk->phys;

	return blk->virt;
}

static int free_for_offload(dma_addr_t phys_addr)
{
	return mtk_usb_sram_free(phys_addr);
}

static int usb_sram_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct mtk_usb_sram *manager;
	const __be32 *regaddr_p;
	u64 regaddr64, size64;
	int ret = 0, i;
	u32 block_size;

	manager = devm_kzalloc(dev, sizeof(*manager), GFP_KERNEL);
	if (!manager)
		return -ENOMEM;

	platform_set_drvdata(pdev, manager);
	manager->dev = dev;
	spin_lock_init(&manager->block_lock);
	spin_lock_init(&manager->list_lock);

	/* get virtual address */
	manager->virt = of_iomap(node, 0);
	if (!manager->virt) {
		dev_info(dev, "fail get virtual address\n");
		return -ENODEV;
	}

	/* get physical address*/
	regaddr_p = of_get_address(node, 0, &size64, NULL);
	if (!regaddr_p) {
		dev_info(dev, "fail get physical address\n");
		return -ENODEV;
	}
	regaddr64 = of_translate_address(node, regaddr_p);
	manager->phys = (dma_addr_t)regaddr64;
	manager->size = (size_t)size64;

	if (of_property_read_u32(node, "block-size", &block_size))
		/* default block size = 1024 */
		block_size = 0x400;
	manager->block_size = (size_t)block_size;

	manager->block_num = (manager->size / manager->block_size);
	manager->blocks = devm_kcalloc(dev, manager->block_num, sizeof(struct usb_sram_block), GFP_KERNEL);
	if (!manager->blocks) {
		dev_info(dev, "%s fail to create block array\n", __func__);
		return -ENOMEM;
	}

	dev_info(manager->dev, "%s virt:%p phys:0x%llx size:%zu block_size:%zu block_num:%d\n",
		__func__, manager->virt, manager->phys, manager->size,
		manager->block_size, manager->block_num);

	for (i = 0; i < manager->block_num; i++) {
		manager->blocks[i].index = i;
		manager->blocks[i].occupied = false;
		manager->blocks[i].size = manager->block_size;
		manager->blocks[i].phys = manager->phys + ((dma_addr_t)i * manager->block_size);
		INIT_LIST_HEAD(&manager->blocks[i].link);
	}

	INIT_LIST_HEAD(&manager->region_list);
	g_manager = manager;

	mtk_register_usb_sram_ops(allocate_for_offload, free_for_offload);

	return ret;
}

static int usb_sram_remove(struct platform_device *pdev)
{
	struct mtk_usb_sram *manager = platform_get_drvdata(pdev);
	struct mtk_usb_sram_region *cur, *next;
	int ret = 0;

	if (!manager)
		return ret;

	if (!list_empty(&manager->region_list)) {
		list_for_each_entry_safe(cur, next, &manager->region_list, list) {
			dev_info(manager->dev, "%s [%s] not freed\n", __func__, parse_region_info(cur));
			list_del(&cur->list);
		}
	}

	return ret;
}

static const struct of_device_id usb_sram_of_match[] = {
	{.compatible = "mediatek,usb-sram",},
	{},
};

static struct platform_driver usb_sram_driver = {
	.probe = usb_sram_probe,
	.remove = usb_sram_remove,
	.driver = {
		.name = "mtk-usb-sram",
		.of_match_table = of_match_ptr(usb_sram_of_match),
	},
};

module_platform_driver(usb_sram_driver);

MODULE_AUTHOR("Yu-Chen.Liu <yu-chen.liu@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek USB Sram Driver");