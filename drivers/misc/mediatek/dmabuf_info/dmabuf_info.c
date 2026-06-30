// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>

#include "dmabuf_info.h"
#define ENTRIES_MAX (9600)

static int get_dmabuf_info(struct miscdevice *mdev, struct dmabuf_info *info)
{
	struct dma_mem_entry *ents;
	struct dma_buf_attachment *buf_att;
	struct dma_buf *dmabuf;
	struct sg_table *sgt;
	struct scatterlist *sg;
	unsigned int num_ents;
	int ret = 0, i;

	dmabuf = dma_buf_get(info->fd);
	if (IS_ERR(dmabuf)) {
		pr_err("%s dma_buf_get fail, info->fd: %d\n",
			__func__, info->fd);
		return -EINVAL;
	}

	buf_att = dma_buf_attach(dmabuf, mdev->this_device);
	if (IS_ERR(buf_att)) {
		ret = PTR_ERR(buf_att);
		pr_err("%s dma_buf_attach fail, ret = %d\n",
			__func__, ret);
		goto err_put;
	}

	sgt = dma_buf_map_attachment(buf_att, DMA_TO_DEVICE);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		pr_err("%s dma_buf_map_attachment fail, ret = %d, info->fd:%d\n",
			__func__, ret, info->fd);
		goto err_detach;
	}

	ents = kcalloc(sgt->nents, sizeof(*ents), GFP_KERNEL);
	if (!ents) {
		ret = -ENOMEM;
		pr_err("getting dmabuf info alloc entries fail\n");
		goto out_unmap_attach;
	}

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		ents[i].addr = cpu_to_le64(sg_dma_address(sg));

		ents[i].length = cpu_to_le32(sg->length);
	}

	num_ents = sgt->nents;

	info->nr_entries = num_ents;
	info->entries = ents;
out_unmap_attach:
	dma_buf_unmap_attachment(buf_att, sgt, DMA_TO_DEVICE);
err_detach:
	dma_buf_detach(dmabuf, buf_att);
err_put:
	dma_buf_put(dmabuf);
	return ret;
}

static long dmabuf_info_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct dmabuf_info info;
	int ret = 0;
	struct miscdevice *mdev = (struct miscdevice *)file->private_data;
	struct dma_mem_entry *entries;
	__u32 nr_entries;

	switch (cmd) {
	case DMABUF_INFO_IOCTL_CMD_GET_ENTRY_NUM:
		ret = copy_from_user(&info, (struct dmabuf_info *)arg, sizeof(struct dmabuf_info));
		if (ret != 0)
			return -EFAULT;

		entries = info.entries;
		nr_entries = info.nr_entries;

		ret = get_dmabuf_info(mdev, &info);
		if (ret != 0)
			return ret;

		ret = copy_to_user((struct dmabuf_info *)arg, &info, sizeof(struct dmabuf_info));
		if (ret != 0) {
			ret = -EFAULT;
			goto free_out;
		}
		break;
	case DMABUF_INFO_IOCTL_CMD_GET_ALL:
		ret = copy_from_user(&info, (struct dmabuf_info *)arg, sizeof(struct dmabuf_info));
		if (ret != 0)
			return -EFAULT;

		entries = info.entries;
		nr_entries = info.nr_entries;

		ret = get_dmabuf_info(mdev, &info);
		if (ret != 0)
			return ret;

		if (nr_entries != info.nr_entries) {
			pr_err("nr_entries mismatch, expected %d, got %d\n", nr_entries, info.nr_entries);
			ret = -EINVAL;
			goto free_out;
		}

		if (info.nr_entries > ENTRIES_MAX){
			pr_err("nr_entries %d exceeds the ENTRIES_MAX %d\n", info.nr_entries, ENTRIES_MAX);
			ret = -EINVAL;
			goto free_out;
		}

		ret = copy_to_user(entries, info.entries, sizeof(struct dma_mem_entry) * info.nr_entries);
		if (ret != 0) {
			pr_err("entries copy_to_user failed\n");
			ret = -EFAULT;
			goto free_out;
		}
		kfree(info.entries);

		info.entries = entries;
		ret = copy_to_user((struct dmabuf_info *)arg, &info, sizeof(struct dmabuf_info));
		if (ret != 0) {
			pr_err("dmabuf_info copy_to_user failed\n");
			return -EFAULT;
		}
		info.entries = NULL;
		break;
	case DMABUF_INFO_IOCTL_CMD_GET_HW_CLK: {
		uint64_t hw_clk = __arch_counter_get_cntpct();

		ret = copy_to_user((uint64_t *)arg, &hw_clk, sizeof(uint64_t));
		if (ret != 0) {
			pr_err("dmabuf_info copy_to_user for hw-clk. failed %d.\n", ret);
			return -EFAULT;
		}
		return 0;
	}
	default:
			return -EINVAL;
	}
// kfree info.entries both in suc and fail case.
free_out:
	kfree(info.entries);
	return ret;
}

static const struct file_operations dmabuf_info_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = dmabuf_info_ioctl,
};

static struct miscdevice dmabuf_info_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,
	.fops = &dmabuf_info_fops,
};

static u64 DMABUF_DMA_MASK = DMA_BIT_MASK(35);
static int __init dmabuf_info_init(void)
{
	int ret;
	u64 dma_mask;
	uint64_t mask = 35;
	struct device *dev;

	ret = misc_register(&dmabuf_info_miscdev);
	if (ret != 0) {
		pr_err("Failed to register dmabuf_info_miscdev\n");
		return ret;
	}

	dev = dmabuf_info_miscdev.this_device;
	dev->dma_mask = &DMABUF_DMA_MASK;

	dma_mask = dma_get_mask(dev);

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(mask));
	if (ret) {
		pr_info("dmabuf_info unable to set DMA mask coherent: %d\n", ret);
		return ret;
	}

	dma_mask = dma_get_mask(dev);
	pr_info("dmabuf_info after set, DMA mask is %llx\n", dma_mask);

	return 0;
}

static void __exit dmabuf_info_exit(void)
{
	misc_deregister(&dmabuf_info_miscdev);

	pr_info("dmabuf_info driver exited\n");
}

module_init(dmabuf_info_init);
module_exit(dmabuf_info_exit);

MODULE_IMPORT_NS(DMA_BUF);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lei.huang@mediatek.com");
MODULE_DESCRIPTION("dmabuf_info driver");
