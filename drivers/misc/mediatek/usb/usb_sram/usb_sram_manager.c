// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Sram Manager
 * *
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Yu-Chen.Liu <yu-chen.liu@mediatek.com>
 */

#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#include "usb_sram.h"
#include "usb_offload.h"

unsigned int allow_request;
module_param(allow_request, uint, 0644);
MODULE_PARM_DESC(allow_request, "accept memory request or not");

unsigned int debug_memory_log;
module_param(debug_memory_log, uint, 0644);
MODULE_PARM_DESC(debug_memory_log, "enable/dsable debug log");

#define usb_sram_dbg(dev, fmt, args...) do { \
	if (debug_memory_log > 0) \
		dev_info(dev, fmt, ##args); \
} while(0)

#define usb_sram_full(dev, fmt, args...) do { \
	if (debug_memory_log > 1) \
		dev_info(dev, fmt, ##args); \
} while(0)


struct usb_sram_block {
	int index;
	bool occupied;
	size_t size;
	dma_addr_t phys;
	struct list_head link;
};

struct mtk_usb_sram {
	struct device *dev;
	struct device_link *to_mtu3;
	struct device_link *to_offload;
	bool xfer_in_suspend;
	atomic_t req_cnt;
	u32 chip_ver;

	/* sram space */
	void *virt;
	dma_addr_t phys;
	size_t size;

	/* bus clock */
	void __iomem *clk_reg;
	void __iomem *clk_set;
	void __iomem *clk_clr;
	void __iomem *clk_upd;
	u32 clk_bit;
	u32 upd_bit;
	u32 pdn_bit;

	/* usb dcm */
	void __iomem *dcm_sel;
	u32 dcm_mask;

	/* vlp_mem_clk */
	void __iomem *vlp_mem_clk;
	u32 vlp_mem_clk_msk;

	/* fuse_latch_en */
	void __iomem *fuse_latch;
	u32 fuse_latch_msk;

	/* block & region management */
	spinlock_t block_lock;
	size_t block_size;
	int block_num;
	struct usb_sram_block *blocks;
	spinlock_t list_lock;
	struct list_head region_list;
};

struct tag_chipid {
	u32 size;
	u32 hw_code;
	u32 hw_submode;
	u32 hw_ver;
	u32 sw_ver;
};

static void usb_sram_get_chip_version(struct mtk_usb_sram *manager)
{
	struct device_node *chosen;
	struct tag_chipid *chip_id;
	int length;

	chosen = of_find_node_by_path("/chosen");
	if (!chosen)
		chosen = of_find_node_by_path("/chosen@0");

	manager->chip_ver = 0;

	if (chosen) {
		chip_id = (struct tag_chipid *)of_get_property(chosen, "atag,chipid", &length);
		if (chip_id)
			manager->chip_ver = chip_id->sw_ver;
		else
			dev_info(manager->dev, "error finding atag,chipid in chosen\n");
	} else
		dev_info(manager->dev, "error finding chosen node\n");

	dev_info(manager->dev, "sw chip version:%d\n", manager->chip_ver);
}

#define USB_SRAM_MIN_ORDER  8

static struct mtk_usb_sram *g_manager;
static void bus_clk_pdn(struct mtk_usb_sram *manager, bool on);
static void dcm_select(struct mtk_usb_sram *manager, bool on);
static void vlp_mem_clk_change(struct mtk_usb_sram *manager, bool on);
static void fuse_latch_change(struct mtk_usb_sram *manager, bool on);
static void bus_clk_change(struct mtk_usb_sram *manager, bool high_speed);

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

	n = snprintf(parse_info, INFO_MAX_LEN, "phys:0x%llx virt:%p size:%zu from:%d to:%d",
		region->phys, region->virt, region->size, region->from, region->to);
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

static void dump_region(struct mtk_usb_sram *manager)
{
	struct mtk_usb_sram_region *pos;

	if (list_empty(&manager->region_list))
		return;

	list_for_each_entry(pos, &manager->region_list, list) {
		usb_sram_dbg(manager->dev, "%s\n", parse_region_info(pos));
	}
}

static void dump_group(struct mtk_usb_sram *manager, struct usb_sram_block *block)
{
	struct usb_sram_block *pos;

	usb_sram_full(manager->dev, "%s\n", decode_block(block));


	if (list_empty(&block->link))
		return;

	list_for_each_entry(pos, &block->link, link) {
		usb_sram_full(manager->dev, "%s\n", decode_block(pos));
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

static size_t init_group(struct usb_sram_block *start, struct usb_sram_block *end)
{
	struct usb_sram_block *cur;
	size_t group_size = 0;

	if (!start || !end || start > end)
		return 0;

	cur = start;
	occupy_block(cur);
	group_size = cur->size;

	while (cur != end) {
		cur = cur + 1;
		occupy_block(cur);
		group_size += cur->size;
		list_add_tail(&cur->link, &start->link);
	}

	return group_size;
}

static void deinit_group(struct usb_sram_block *start)
{
	struct usb_sram_block *pos, *next;

	if (!list_empty(&start->link)) {
		list_for_each_entry_safe(pos, next, &start->link, link) {
			vacate_block(pos);
			list_del(&pos->link);
			INIT_LIST_HEAD(&pos->link);
		}
	}
	vacate_block(start);
	INIT_LIST_HEAD(&start->link);
}

struct mtk_usb_sram_region *mtk_usb_sram_allocate(unsigned int size, int align, int start_idx)
{
	struct device *dev;
	struct mtk_usb_sram *manager = g_manager;
	struct usb_sram_block *start, *end, *cur;
	struct mtk_usb_sram_region *region = NULL;
	int request = size, residue;
	size_t group_size;
	int i = start_idx;
	bool found_block = false;

	if (!allow_request)
		goto error;

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

		usb_sram_full(dev, "start [%s] req:%d align:%d\n", decode_block(start), size, align);

		if (start->occupied)
			goto next_block;

		/* check if address met align requirement */
		if (align > 0 && (start->phys & ((dma_addr_t)(align - 1))) != 0)
			goto next_block;

		/* residue = 0 means that group has sufficient space */
		while (i < manager->block_num && residue > 0) {
			residue -= cur->size;
			usb_sram_full(dev, "checking [%s], req:%d res:%d\n", decode_block(cur), request, residue);
			if (cur->occupied)
				goto next_block;
			if (residue <= 0) {
				end = cur;
				dev_info(manager->dev, "found block%d ~ block%d\n", start->index, end->index);
				group_size = init_group(start, end);
				if (group_size > 0) {
					found_block = true;
					dump_group(manager, start);
				} else
					dev_info(manager->dev, "fail to init group (block%d ~ block%d)\n",
						start->index, end->index);
			}
			cur = &manager->blocks[++i];
		}

next_block:
		i++;
	}

	spin_unlock(&manager->block_lock);
	if (found_block) {
		region = kzalloc(sizeof(struct mtk_usb_sram_region), GFP_KERNEL);
		if (!region) {
			dev_info(manager->dev, "%s can't create region\n", __func__);
			goto error;
		}
		region->from = start->index;
		region->to = end->index;
		region->phys = start->phys;
		region->size = (size_t)request;
		region->virt = (void *)ioremap_wc(region->phys, region->size);

		dev_info(manager->dev, "allocate [%s]\n", parse_region_info(region));

		spin_lock(&manager->list_lock);
		atomic_inc(&manager->req_cnt);
		dev_info(manager->dev, "req_cnt:%d\n", atomic_read(&manager->req_cnt));
		if (list_empty(&manager->region_list)) {
			bus_clk_pdn(manager, false);
			dcm_select(manager, false);
		}
		list_add_tail(&region->list, &manager->region_list);
		dump_region(manager);
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

	spin_lock(&manager->list_lock);
	atomic_dec(&manager->req_cnt);
	dev_info(manager->dev, "req_cnt:%d\n", atomic_read(&manager->req_cnt));
	list_del(&region->list);
	if (list_empty(&manager->region_list)) {
		bus_clk_pdn(manager, true);
		dcm_select(manager, true);
	}
	dump_region(manager);
	spin_unlock(&manager->list_lock);

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
		spin_unlock(&manager->list_lock);
		return -EINVAL;
	}

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
		spin_unlock(&manager->list_lock);
		return -EINVAL;
	}

	spin_unlock(&manager->list_lock);
	return _mtk_usb_sram_free(manager, region);
}
EXPORT_SYMBOL_GPL(mtk_usb_sram_free_virt);

static void *allocate_for_offload(dma_addr_t *phys_addr, unsigned int size, int align)
{
	struct mtk_usb_sram_region *blk;

	blk = mtk_usb_sram_allocate(size, align, 0);
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

static void bit_op(struct device *dev, void __iomem *addr,
	u32 mask, bool on, const char *name)
{
	u32 value;

	if (!dev || !addr)
		return;

	value = readl(addr);
	if (on)
		value |= mask;
	else
		value &= ~(mask);
	writel(value, addr);

	dev_info(dev, "%s(on:%d) <%s[msk:0x%x]>=0x%x\n", __func__, on, name, mask, readl(addr));
}

static void dcm_select(struct mtk_usb_sram *manager, bool on)
{
	if (!manager || !manager->dcm_sel)
		return;

	bit_op(manager->dev, manager->dcm_sel, manager->dcm_mask, on, "usb-dcm");
}

static void vlp_mem_clk_change(struct mtk_usb_sram *manager, bool on)
{
	if (!manager || !manager->vlp_mem_clk)
		return;

	bit_op(manager->dev, manager->vlp_mem_clk,
		manager->vlp_mem_clk_msk, on, "vlp-mem-clk");
}

static void fuse_latch_change(struct mtk_usb_sram *manager, bool on)
{
	if (!manager || !manager->fuse_latch)
		return;

	bit_op(manager->dev, manager->fuse_latch,
		manager->fuse_latch_msk, on, "fuse-latch-en");
}

static void bus_clk_pdn(struct mtk_usb_sram *manager, bool on)
{
	u32 mask;

	if (!manager || !manager->clk_reg)
		return;
	mask = (0x1U << manager->pdn_bit);

	bit_op(manager->dev, manager->clk_reg, mask, on, "bus-clk-pdn");
}

static void bus_clk_change(struct mtk_usb_sram *manager, bool high_speed)
{
	void __iomem *operation;

	if (!manager || !manager->clk_reg || !manager->clk_set ||
		!manager->clk_clr || !manager->clk_upd)
		return;

	/* clear(0):26m set(1):ocs */
	operation = high_speed ? manager->clk_set : manager->clk_clr;
	bit_op(manager->dev, operation, (0x1U << manager->clk_bit), true,
		high_speed ? "clk-set" : "clk-clr");

	/* update */
	bit_op(manager->dev, manager->clk_upd, (0x1U << manager->upd_bit), true, "clk-upd");

	dev_info(manager->dev, "%s high:%d bus-clk:0x%x\n", __func__, high_speed, readl(manager->clk_reg));
}

#define BUS_CLK_INFO_COUNT	8
static void usb_sram_bus_clk_parse(struct mtk_usb_sram *manager)
{
	u32 bus_clk[BUS_CLK_INFO_COUNT];
	u32 clk_base, reg_ofs, set_ofs, clr_ofs, upd_ofs;
	uintptr_t reg_physical;
	uintptr_t set_physical;
	uintptr_t clr_physical;
	uintptr_t upd_physical;

	if (!manager)
		return;

	manager->clk_reg = NULL;
	manager->clk_set = NULL;
	manager->clk_clr = NULL;
	manager->clk_upd = NULL;

	if (!device_property_read_u32_array(manager->dev, "bus-clk", bus_clk, BUS_CLK_INFO_COUNT)) {
		clk_base = bus_clk[0];
		reg_ofs = bus_clk[1];
		set_ofs = bus_clk[2];
		clr_ofs = bus_clk[3];
		upd_ofs = bus_clk[4];
		manager->clk_bit = bus_clk[5];
		manager->upd_bit = bus_clk[6];
		manager->pdn_bit = bus_clk[7];

		reg_physical = (uintptr_t)(clk_base + reg_ofs); /* for setting pdn */
		set_physical = (uintptr_t)(clk_base + set_ofs); /* for setting source */
		clr_physical = (uintptr_t)(clk_base + clr_ofs); /* for clearing source */
		upd_physical = (uintptr_t)(clk_base + upd_ofs); /* for updating source */

		manager->clk_reg = ioremap((phys_addr_t)reg_physical, sizeof(u32));
		manager->clk_set = ioremap((phys_addr_t)set_physical, sizeof(u32));
		manager->clk_clr = ioremap((phys_addr_t)clr_physical, sizeof(u32));
		manager->clk_upd = ioremap((phys_addr_t)upd_physical, sizeof(u32));

		dev_info(manager->dev, "reg:0x%lx set:0x%lx clr:0x%lx upd:0x%lx (clk:0x%x upd:0x%x pdn:0x%x)\n",
			reg_physical, set_physical,	clr_physical, upd_physical,
			manager->clk_bit, manager->upd_bit, manager->pdn_bit);
	}
}

static void usb_sram_unmap_all(struct mtk_usb_sram *manager)
{
	if (manager->clk_reg) {
		iounmap(manager->clk_reg);
		manager->clk_reg = NULL;
	}
	if (manager->clk_set) {
		iounmap(manager->clk_set);
		manager->clk_set = NULL;
	}
	if (manager->clk_clr) {
		iounmap(manager->clk_clr);
		manager->clk_clr = NULL;
	}
	if (manager->clk_upd) {
		iounmap(manager->clk_upd);
		manager->clk_upd = NULL;
	}
	if (manager->dcm_sel) {
		iounmap(manager->dcm_sel);
		manager->dcm_sel = NULL;
	}
	if (manager->vlp_mem_clk) {
		iounmap(manager->vlp_mem_clk);
		manager->vlp_mem_clk = NULL;
	}
	if (manager->fuse_latch) {
		iounmap(manager->fuse_latch);
		manager->fuse_latch = NULL;
	}
}

static int usb_sram_allocate_dbg(int index, u32 size)
{
	struct mtk_usb_sram *manager = g_manager;
	struct mtk_usb_sram_region *region;

	if (index >= manager->block_num || index < 0)
		return -EINVAL;

	region = mtk_usb_sram_allocate(size, -1, index);

	return region ? 0 : -EINVAL;
}

static int usb_sram_free_dbg(int index)
{
	struct mtk_usb_sram *manager = g_manager;

	if (index >= manager->block_num || index < 0)
		return -EINVAL;

	return mtk_usb_sram_free(manager->blocks[index].phys);
}

#define MAX_INPUT_NUM   50
static ssize_t dbg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_usb_sram *manager = g_manager;
	char buffer[MAX_INPUT_NUM];
	char *input = buffer;
	const char *field1, *field2, *field3;
	const char * const delim = " \0\n\t";
	int start_index, ret;
	u32 size;

	strscpy(buffer, buf, sizeof(buffer) <= count ? sizeof(buffer) : count);
	dev_info(manager->dev, "%s input: [%s]\n", __func__, input);
	field1 = strsep(&input, delim);
	field2 = strsep(&input, delim);
	if (!field1 || !field2)
		goto invalid_input;

	ret = kstrtoint(field2, 10, &start_index);
	if (ret < 0)
		goto invalid_input;

	if (!strncmp(field1, "allocate", 8)) {
		field3 = strsep(&input, delim);
		if (!field3)
			goto invalid_input;
		ret = kstrtou32(field3, 10, &size);
		if (ret < 0)
			goto invalid_input;
		dev_info(manager->dev, "%s allocate: index:%d size:%d\n", __func__, start_index, size);
		if (!usb_sram_allocate_dbg(start_index, size))
			goto success;
		else
			goto error;
	} else if (!strncmp(field1, "free", 4)) {
		dev_info(manager->dev, "%s free: index:%d\n", __func__, start_index);
		if (!usb_sram_free_dbg(start_index))
			goto success;
		else
			goto error;
	} else
		goto invalid_input;

invalid_input:
	dev_info(manager->dev, "%s invalid input\n", __func__);
	return -EINVAL;

error:
	dev_info(manager->dev, "%s operation error\n", __func__);
	return -EINVAL;
success:
	return count;
}

static ssize_t dbg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_usb_sram *manager = g_manager;
	int size = PAGE_SIZE;
	int cnt, total_cnt = 0, i;

	for (i = 0; i < manager->block_num; i++) {
		cnt = snprintf(buf + total_cnt, size - total_cnt,
			"%s\n", decode_block(&manager->blocks[i]));
		if (cnt > 0 && cnt < size)
			total_cnt += cnt;
		else
			break;
	}

	return total_cnt;
}

static DEVICE_ATTR_RW(dbg);

static struct attribute *usb_sram_attrs[] = {
	&dev_attr_dbg.attr,
	NULL,
};

static const struct attribute_group usb_sram_group = {
	.attrs = usb_sram_attrs,
};

#define RG_INFO_CNT		2
static void usb_sram_dts_parse(struct device_node *node, struct mtk_usb_sram *manager)
{
	u32 rg_info[RG_INFO_CNT];
	u32 block_size;

	if (!node || !manager)
		return;

	if (of_property_read_u32(node, "block-size", &block_size))
		/* default block size = 1024 */
		block_size = 0x400;
	manager->block_size = (size_t)block_size;

	allow_request = of_property_read_bool(node, "allow-request");

	/* map usb dcm rg */
	manager->dcm_sel = NULL;
	manager->dcm_mask = 0;
	if (!device_property_read_u32_array(manager->dev, "dcm-sel", rg_info, RG_INFO_CNT)) {
		dev_info(manager->dev, "[usb-dcm] addr:0x%x mask:0x%x\n", rg_info[0], rg_info[1]);
		manager->dcm_sel = ioremap((phys_addr_t)(uintptr_t)rg_info[0], sizeof(u32));
		manager->dcm_mask = rg_info[1];
	}

	/* map vlp_mem_clk mux */
	manager->vlp_mem_clk = NULL;
	manager->vlp_mem_clk_msk = 0;
	if (!device_property_read_u32_array(manager->dev, "vlp-mem-clk", rg_info, RG_INFO_CNT)) {
		dev_info(manager->dev, "[vlp-mem-clk] addr:0x%x bit:%d\n", rg_info[0], rg_info[1]);
		manager->vlp_mem_clk = ioremap((phys_addr_t)(uintptr_t)rg_info[0], sizeof(u32));
		manager->vlp_mem_clk_msk = rg_info[1];
	}

	/* map fuse_latch_en rg*/
	manager->fuse_latch = NULL;
	manager->fuse_latch_msk = 0;
	if (manager->chip_ver != 0 &&
		of_device_is_compatible(node, "mediatek,mt6993-usb-sram") &&
		!device_property_read_u32_array(manager->dev, "fuse-latch", rg_info, RG_INFO_CNT)) {
		dev_info(manager->dev, "[fuse-latch] addr:0x%x bit:%d\n", rg_info[0], rg_info[1]);
		manager->fuse_latch = ioremap((phys_addr_t)(uintptr_t)rg_info[0], sizeof(u32));
		manager->fuse_latch_msk = rg_info[1];
	}

}

static struct device *get_target_device(const char *name)
{
	struct platform_device *pdev = NULL;
	struct device_node *node;
	struct device *dev = NULL;

	node = of_find_node_by_name(NULL, name);
	if (node) {
		pdev = of_find_device_by_node(node);
		if (pdev)
			dev = &pdev->dev;
		of_node_put(node);
	}

	return dev;
}

static void usb_sram_unlink(struct mtk_usb_sram *manager)
{
	if (manager->to_mtu3) {
		device_link_del(manager->to_mtu3);
		manager->to_mtu3 = NULL;
	}

	if (manager->to_offload) {
		device_link_del(manager->to_offload);
		manager->to_offload = NULL;
	}
}

/* suspend seq: usb_offload.ko => usb_sram.ko => mtu3.ko
 * resume seq : usb_offload.ko <= usb_sram.ko <= mtu3.ko
 */
static int usb_sram_link(struct mtk_usb_sram *manager)
{
	struct device *mtu3;
	struct device *usb_offload;

	manager->to_mtu3 = NULL;
	manager->to_offload = NULL;

	/* link mtu3.ko */
	mtu3 = get_target_device("usb0");
	if (mtu3) {
		manager->to_mtu3 =  device_link_add(
			manager->dev, mtu3, DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS);
		if (!manager->to_mtu3) {
			dev_info(manager->dev, "fail to link mtu3\n");
			goto error;
		}
	} else {
		dev_info(manager->dev, "fail to get mtu3 device!\n");
		goto error;
	}

	/* link usb_offload.ko */
	usb_offload = get_target_device("usb-offload");
	if (usb_offload) {
		manager->to_offload =  device_link_add(
			usb_offload, manager->dev, DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS);
		if (!manager->to_offload) {
			dev_info(manager->dev, "fail to link usb_offload\n");
			goto error;
		}
	} else {
		dev_info(manager->dev, "fail to get usb_offload device!\n");
		goto error;
	}

	return 0;
error:
	usb_sram_unlink(manager);
	return -EINVAL;
}

static int usb_sram_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct mtk_usb_sram *manager;
	const __be32 *regaddr_p;
	u64 regaddr64, size64;
	int ret = 0, i;

	manager = devm_kzalloc(dev, sizeof(*manager), GFP_KERNEL);
	if (!manager)
		return -ENOMEM;
	manager->dev = dev;
	manager->xfer_in_suspend = false;

	/* get chip version (A0 or B0) */
	usb_sram_get_chip_version(manager);

	/* link to mtu3.ko & usb_offload.ko */
	ret = usb_sram_link(manager);
	if (ret < 0)
		return -ENODEV;

	platform_set_drvdata(pdev, manager);
	spin_lock_init(&manager->block_lock);
	spin_lock_init(&manager->list_lock);

	/* get virtual address */
	manager->virt = of_iomap(node, 0);
	if (!manager->virt) {
		dev_info(manager->dev, "fail get virtual address\n");
		ret = -ENODEV;
		goto unlink;
	}

	/* get physical address*/
	regaddr_p = of_get_address(node, 0, &size64, NULL);
	if (!regaddr_p) {
		dev_info(manager->dev, "fail get physical address\n");
		ret = -ENODEV;
		goto unmap;
	}
	regaddr64 = of_translate_address(node, regaddr_p);
	manager->phys = (dma_addr_t)regaddr64;
	manager->size = (size_t)size64;

	/* parse dts properties */
	usb_sram_dts_parse(node, manager);
	usb_sram_bus_clk_parse(manager);

	/* initialize blocks */
	manager->block_num = (manager->size / manager->block_size);
	manager->blocks = devm_kcalloc(dev, manager->block_num, sizeof(struct usb_sram_block), GFP_KERNEL);
	if (!manager->blocks) {
		dev_info(dev, "%s fail to create block array\n", __func__);
		ret = -ENODEV;
		goto unmap;
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

	/* register callback function of usb_offload.ko */
	mtk_register_usb_sram_ops(allocate_for_offload, free_for_offload);

	if (sysfs_create_group(&dev->kobj, &usb_sram_group))
		USB_OFFLOAD_ERR("fail to create sysfs attribtues\n");

	/* set pdn down in the last */
	bus_clk_pdn(manager, true);

	return ret;

unmap:
	iounmap(manager->virt);
unlink:
	usb_sram_unlink(manager);
	return ret;
}

static void usb_sram_remove(struct platform_device *pdev)
{
	struct mtk_usb_sram *manager = platform_get_drvdata(pdev);
	struct mtk_usb_sram_region *cur, *next;

	if (!manager)
		return;

	if (!list_empty(&manager->region_list)) {
		list_for_each_entry_safe(cur, next, &manager->region_list, list) {
			dev_info(manager->dev, "%s [%s] not freed\n", __func__, parse_region_info(cur));
			list_del(&cur->list);
		}
	}

	usb_sram_unlink(manager);
	usb_sram_unmap_all(manager);
}

static int usb_sram_common_suspend(struct device *dev)
{
	struct mtk_usb_sram *manager = dev_get_drvdata(dev);

	dev_info(dev, "%s ++\n", __func__);

	/* req_cnt=1 for basic reserved part */
	if (atomic_read(&manager->req_cnt) > 1)
		manager->xfer_in_suspend = true;

	dev_info(dev, "req_cnt:%d xfer_in_suspend:%d\n",
		atomic_read(&manager->req_cnt), manager->xfer_in_suspend);

	if (manager->xfer_in_suspend) {
		/* select vlp clock */
		vlp_mem_clk_change(manager, true);

		/* set fuse_latch_en */
		fuse_latch_change(manager, true);

	} else {
		/* switch vcore clock to 26m */
		bus_clk_change(manager, false);
	}

	dev_info(dev, "%s --\n", __func__);

	return 0;
}

static int usb_sram_common_resume(struct device *dev)
{
	struct mtk_usb_sram *manager = dev_get_drvdata(dev);

	dev_info(dev, "%s ++\n", __func__);
	dev_info(dev, "xfer_in_suspend:%d\n", manager->xfer_in_suspend);

	if (manager->xfer_in_suspend) {
		/* select vcore clock */
		vlp_mem_clk_change(manager, false);

		/* clear fuse_latch_en */
		fuse_latch_change(manager, false);

		manager->xfer_in_suspend = false;

	} else {
		/* switch vcore clock to ocs */
		bus_clk_change(manager, true);
	}

	dev_info(dev, "%s --\n", __func__);

	return 0;
}

static int __maybe_unused usb_sram_suspend(struct device *dev)
{
	return usb_sram_common_suspend(dev);
}

static int __maybe_unused usb_sram_resume(struct device *dev)
{
	return usb_sram_common_resume(dev);
}

static int __maybe_unused usb_sram_runtime_suspend(struct device *dev)
{
	return usb_sram_common_suspend(dev);
}

static int __maybe_unused usb_sram_runtime_resume(struct device *dev)
{
	return usb_sram_common_resume(dev);
}

static const struct dev_pm_ops usb_sram_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(usb_sram_suspend, usb_sram_resume)
	SET_RUNTIME_PM_OPS(usb_sram_runtime_suspend,
					usb_sram_runtime_resume, NULL)
};

#define DEV_PM_OPS (IS_ENABLED(CONFIG_PM) ? &usb_sram_pm_ops : NULL)

static const struct of_device_id usb_sram_of_match[] = {
	{.compatible = "mediatek,usb-sram",},
	{},
};

static struct platform_driver usb_sram_driver = {
	.probe = usb_sram_probe,
	.remove = usb_sram_remove,
	.driver = {
		.name = "mtk-usb-sram",
		.pm = DEV_PM_OPS,
		.of_match_table = of_match_ptr(usb_sram_of_match),
	},
};

module_platform_driver(usb_sram_driver);

MODULE_AUTHOR("Yu-Chen.Liu <yu-chen.liu@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek USB Sram Driver");
