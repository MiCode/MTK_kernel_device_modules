// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <asm/stacktrace.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <soc/mediatek/emi.h>

struct emi_isu {
	unsigned int buf_size;
	void __iomem *buf_addr;
	void __iomem *ver_addr;
	void __iomem *con_addr;
	unsigned int ctrl_intf;
	unsigned int enable;
};

struct emi_isu_ctrl_trace {
	unsigned long long ts;
	unsigned int val;
	unsigned int readback;
};

/* global pointer for sysfs operations*/
static struct emi_isu *global_emi_isu;

#define ISU_CTRL_TRACE_MAX 256
static struct emi_isu_ctrl_trace isu_ctrl_trace[ISU_CTRL_TRACE_MAX];
static unsigned int cur_ctrl_trace_idx;

#define ISU_CTRL_ENABLED_MASK 0xFFF0FFF0
#define ISU_CTRL_ENABLED_FLAG 0xDEC0DEC0
#define ISU_CTRL_ENABLED_RECORDING 0xDECDDECD
#define ISU_CTRL_ENABLED_STOPED 0xDEC0DEC0

#define ISU_CTRL_NODE_PROP_ENABLE 0xDEC5DEC5
#define ISU_CTRL_NODE_MANUAL_ENABLE ISU_CTRL_ENABLED_RECORDING

#define ISU_CTRL_NODE_PROP_DISABLE 0xDECADECA
#define ISU_CTRL_NODE_MANUAL_DISABLE ISU_CTRL_ENABLED_STOPED

#define KERN_REC_ON_ISU_BUF_READ 0x15
#define KERN_REC_ON_TIMER_CALLBACK 0x17
#define KERN_REC_OFF_OTHER_DRV 0x1A
#define KERN_REC_OFF_PROBE 0x1F

#define ISU_CTRL_NODE_MET_DISABLE 0x0
#define ISU_CTRL_UNKNOWN 0xFFFFFFFF

static unsigned int isu_ctrl_flag_last = ISU_CTRL_UNKNOWN;
static struct timer_list check_isu_timer;

static DEFINE_MUTEX(emiisu_ctrl_mutex);

/**
 * Updates ISU control state based on source value.
 *
 * @param src Source of ISU control update
 * @return Updated ISU control state
 */
static unsigned int set_isu_ctrl_flag(unsigned int src)
{
	unsigned int isu_ctrl_ret = ISU_CTRL_UNKNOWN;
	int i = 0;

	if ((isu_ctrl_flag_last == KERN_REC_ON_ISU_BUF_READ) && (src == KERN_REC_ON_ISU_BUF_READ)) {
		//pr_info("%s: during iSU_BUF_READ, skip to update cur_ctrl_trace\n", __func__);
		return ISU_CTRL_ENABLED_RECORDING;
	}

	// Log current state
	pr_info("%s: lsu_ctrl_flag_last: 0x%x\n", __func__, isu_ctrl_flag_last);
	pr_info("%s: cur_ctrl_trace_idx=%u\n", __func__, cur_ctrl_trace_idx);

	// Update control trace
	isu_ctrl_trace[cur_ctrl_trace_idx].ts = sched_clock();
	isu_ctrl_trace[cur_ctrl_trace_idx].val = src;

	if (global_emi_isu && global_emi_isu->con_addr)
		isu_ctrl_trace[cur_ctrl_trace_idx].readback = readl(global_emi_isu->con_addr);
	else
		isu_ctrl_trace[cur_ctrl_trace_idx].readback = ISU_CTRL_UNKNOWN;

	cur_ctrl_trace_idx = (cur_ctrl_trace_idx + 1) % ISU_CTRL_TRACE_MAX;

	// Log updated trace
	for (i = 0; i < ISU_CTRL_TRACE_MAX; i++) {
		if (isu_ctrl_trace[i].ts != 0ULL) {
			pr_info("%s: isu_ctrl_trace[%d]: ts=%llu, val=%x, readback=%x\n", __func__,
				i, isu_ctrl_trace[i].ts, isu_ctrl_trace[i].val, isu_ctrl_trace[i].readback);
		}
	}

	isu_ctrl_flag_last = src;

	switch (src) {
	case ISU_CTRL_NODE_PROP_ENABLE:
		pr_info("%s: ISU_CTRL_NODE_PROP_ENABLE\n", __func__);
		isu_ctrl_ret = ISU_CTRL_ENABLED_RECORDING;
		break;
	case ISU_CTRL_NODE_MANUAL_ENABLE:
		pr_info("%s: ISU_CTRL_NODE_MANUAL_ENABLE\n", __func__);
		isu_ctrl_ret = ISU_CTRL_ENABLED_RECORDING;
		break;
	case KERN_REC_ON_ISU_BUF_READ:
		pr_info("%s: KERN_REC_ON_ISU_BUF_READ\n", __func__);
		isu_ctrl_ret = ISU_CTRL_ENABLED_RECORDING;
		break;
	case KERN_REC_ON_TIMER_CALLBACK:
		pr_info("%s: KERN_REC_ON_TIMER_CALLBACK\n", __func__);
		isu_ctrl_ret = ISU_CTRL_ENABLED_RECORDING;
		break;
	case ISU_CTRL_NODE_PROP_DISABLE:
		pr_info("%s: ISU_CTRL_NODE_PROP_DISABLE\n", __func__);
		isu_ctrl_ret = ISU_CTRL_ENABLED_STOPED;
		break;
	case ISU_CTRL_NODE_MANUAL_DISABLE:
		pr_info("%s: ISU_CTRL_NODE_MANUAL_DISABLE\n", __func__);
		isu_ctrl_ret = ISU_CTRL_ENABLED_STOPED;
		break;
	case KERN_REC_OFF_OTHER_DRV:
		pr_info("%s: KERN_REC_OFF_OTHER_DRV\n", __func__);
		isu_ctrl_ret = ISU_CTRL_ENABLED_STOPED;
		break;
	case KERN_REC_OFF_PROBE:
		pr_info("%s: KERN_REC_OFF_PROBE\n", __func__);
		isu_ctrl_ret = ISU_CTRL_ENABLED_STOPED;
		break;
	case ISU_CTRL_NODE_MET_DISABLE:
		pr_info("%s: ISU_CTRL_NODE_MET_DISABLE\n", __func__);
		isu_ctrl_ret = ISU_CTRL_NODE_MET_DISABLE;
		break;
	default:
		pr_info("%s: unknown ISU source\n", __func__);
		isu_ctrl_flag_last = ISU_CTRL_UNKNOWN;
		break;
	}

	return isu_ctrl_ret;
}

void check_isu_timer_callback(struct timer_list *timer)
{
	struct emi_isu *isu;
	unsigned int state;

	if (!global_emi_isu)
		return;

	isu = global_emi_isu;

	if (!(isu->con_addr))
		return;

	state = readl(isu->con_addr);
	pr_info("%s: readback isu_ctrl: 0x%x, isu_ctrl_flag_last: 0x%x\n", __func__, state, isu_ctrl_flag_last);

	if (state == ISU_CTRL_ENABLED_RECORDING) {
		pr_info("%s: ISU is currently recording\n", __func__);
	} else if (isu_ctrl_flag_last == KERN_REC_OFF_PROBE) {
		pr_info("%s: ISU stopped due to probe disable by default\n", __func__);
	} else if (isu_ctrl_flag_last == ISU_CTRL_NODE_MET_DISABLE) {
		pr_info("%s: ISU stopped due to MET (0x0)\n", __func__);
	} else if (isu_ctrl_flag_last == ISU_CTRL_NODE_PROP_DISABLE) {
		pr_info("%s: ISU stopped due to system property disabled\n", __func__);
	} else if (isu_ctrl_flag_last == ISU_CTRL_NODE_MANUAL_DISABLE) {
		pr_info("%s: ISU stopped due to manually disabled\n", __func__);
	} else {
		pr_info("%s: ISU start recording by timer\n", __func__);
		mtk_emiisu_record_on();
		set_isu_ctrl_flag(KERN_REC_ON_TIMER_CALLBACK);
	}
}

/*
 * Function to check if ISU is disabled due to MET
 * @param struct emi_isu *isu Pointer to the ISU structure
 * @return unsigned int Returns 1 if ISU is disabled due to MET, 0 otherwise
 */
static unsigned int isu_met_disable_check(struct emi_isu *isu)
{
	const unsigned int isu_ctrl_readback = readl(isu->con_addr);
	//pr_info("%s: readback isu_ctrl: 0x%x\n", __func__, isu_ctrl_readback);

	if (isu_ctrl_readback == ISU_CTRL_NODE_MET_DISABLE) {
		pr_info("%s: ISU stopped due to MET (0x0)\n", __func__);
		return 1;
	}

	return 0;
}

void mtk_emiisu_record_off(void)
{
	struct emi_isu *isu;
	unsigned int state;

	if (!global_emi_isu)
		return;

	isu = global_emi_isu;

	if (!(isu->con_addr))
		return;

	dump_stack();

	state = readl(isu->con_addr);
	pr_info("%s: readback isu_ctrl: 0x%x, isu_ctrl_flag_last: 0x%x\n", __func__, state, isu_ctrl_flag_last);
	if ((state == ISU_CTRL_ENABLED_STOPED) || (state == ISU_CTRL_NODE_MET_DISABLE)) {
		pr_info("%s: ISU is already stopped\n", __func__);
		return;
	}

	writel(ISU_CTRL_ENABLED_STOPED, isu->con_addr);
	dsb(sy);
	pr_info("%s: Turn off EMIISU dump\n", __func__);
	set_isu_ctrl_flag(KERN_REC_OFF_OTHER_DRV);

	// Prepare the timer to check if we need to restart ISU recording
	mod_timer(&check_isu_timer, jiffies + msecs_to_jiffies(100 * 1000));
}
EXPORT_SYMBOL(mtk_emiisu_record_off);

void mtk_emiisu_record_on(void)
{
	struct emi_isu *isu;

	if (!global_emi_isu)
		return;

	isu = global_emi_isu;

	if (!(isu->con_addr))
		return;

	if (!(isu->enable))
		return;

	if (isu_met_disable_check(isu))
		return;

	writel(ISU_CTRL_ENABLED_RECORDING, isu->con_addr);
	dsb(sy);
	//pr_info("%s: Turn on EMIISU dump\n", __func__);
	//dump_stack();
}
EXPORT_SYMBOL(mtk_emiisu_record_on);

unsigned int get_emi_total_bw(void)
{
	struct emi_isu *isu;
	unsigned int emi_total_bw;

	if (!global_emi_isu)
		return 0;

	isu = global_emi_isu;

	if (!(isu->con_addr))
		return 0;

	emi_total_bw = readl(isu->con_addr + 0x4);

	return emi_total_bw;
}
EXPORT_SYMBOL(get_emi_total_bw);

static ssize_t emiisu_ctrl_show(struct device_driver *driver, char *buf)
{
	struct emi_isu *isu;
	unsigned int state;

	if (!global_emi_isu)
		return 0;

	isu = global_emi_isu;

	if (!(isu->con_addr))
		return 0;

	state = readl(isu->con_addr);

	return snprintf(buf, PAGE_SIZE, "isu_state: 0x%x\n", state);
}

static ssize_t emiisu_ctrl_store
	(struct device_driver *driver, const char *buf, size_t count)
{
	struct emi_isu *isu;
	unsigned long state;
	unsigned int isu_ctrl_readback = ISU_CTRL_UNKNOWN;
	unsigned int isu_ctrl_written = ISU_CTRL_UNKNOWN;
	char *command;
	char *backup_command;
	char *ptr;
	char *token[MTK_EMI_MAX_TOKEN];
	int i;

	if (!global_emi_isu)
		return count;

	isu = global_emi_isu;

	if (!(isu->ctrl_intf))
		return count;

	if (!(isu->con_addr))
		return count;

	if (!(isu->enable))
		return count;

	if ((strlen(buf) + 1) > MTK_EMI_MAX_CMD_LEN) {
		pr_info("%s: store command overflow\n", __func__);
		return count;
	}

	command = kmalloc((size_t)MTK_EMI_MAX_CMD_LEN, GFP_KERNEL);
	if (!command)
		return count;
	backup_command = command;
	strncpy(command, buf, (size_t)MTK_EMI_MAX_CMD_LEN);

	for (i = 0; i < MTK_EMI_MAX_TOKEN; i++) {
		ptr = strsep(&command, " ");
		if (!ptr)
			break;
		token[i] = ptr;
	}

	if (i < 1)
		goto emiisu_ctrl_store_end;

	mutex_lock(&emiisu_ctrl_mutex);

	if (kstrtoul(token[0], 16, &state) == 0) {
		pr_info("%s: received 0x%lx from isu_ctrl\n", __func__, state);

		isu_ctrl_written = set_isu_ctrl_flag((unsigned int)state);
		writel(isu_ctrl_written, isu->con_addr);
		dsb(sy);

		pr_info("%s: write 0x%x to isu_ctrl\n", __func__, isu_ctrl_written);
	}

	// check emiisu_ctrl status
	isu_ctrl_readback = readl(isu->con_addr);
	pr_info("%s: readback isu_ctrl: 0x%x\n", __func__, isu_ctrl_readback);
	if (isu_ctrl_readback == isu_ctrl_written)
		pr_info("%s: write isu_con successfully\n", __func__);
	else
		pr_info("%s: write isu_con fail\n", __func__);

	if (isu_ctrl_readback == ISU_CTRL_ENABLED_RECORDING)
		pr_info("%s: Start EMIISU recording\n", __func__);
	else if (isu_ctrl_readback == ISU_CTRL_ENABLED_STOPED) {
		pr_info("%s: Stop EMIISU recording\n", __func__);
		//dump_stack();
	}

	mutex_unlock(&emiisu_ctrl_mutex);

emiisu_ctrl_store_end:
	kfree(backup_command);

	return count;
}

static DRIVER_ATTR_RW(emiisu_ctrl);

static ssize_t read_emi_isu_buf(struct file *filp, struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buff, loff_t pos, size_t count)
{
	struct emi_isu *isu;
	ssize_t bytes = 0, ret;

	if (!global_emi_isu)
		return 0;

	isu = global_emi_isu;

	if (!(isu->buf_addr))
		return 0;
	if (!(isu->ver_addr))
		return 0;

	/*
	 * The EMI ISU buffer consists of a 4-byte header (which is stored in
	 * isu->ver_addr) and the content (which is stored in isu->buf_addr).
	 * Concatenate isu->ver_addr and isu->buf_addr and return to the reader.
	 */

	if (pos == 0) {
		if (count < 4)
			return -EINVAL;
		memcpy(buff, isu->ver_addr, 4);
		bytes += 4;
		buff += 4;
		count -= 4;
		pos += 4;
	}

	pos -= 4;
	ret = memory_read_from_buffer(buff, count, &pos,
				isu->buf_addr, isu->buf_size);

	if (!isu_met_disable_check(isu)) {
		mtk_emiisu_record_on();
		set_isu_ctrl_flag(KERN_REC_ON_ISU_BUF_READ);
	}

	if (ret < 0)
		return ret;
	else
		return bytes + ret;
}

static struct bin_attribute emi_isu_buf_attr = {
	.attr = {.name = "emi_isu_buf", .mode = 0444},
	.read = read_emi_isu_buf,
};

static const struct of_device_id emiisu_of_ids[] = {
	{.compatible = "mediatek,common-emiisu",},
	{}
};

static int emiisu_probe(struct platform_device *pdev)
{
	struct device_node *emiisu_node = pdev->dev.of_node;
	struct emi_isu *isu;
	unsigned long long addr_temp;
	unsigned int isu_ctrl_val = 0;
	int ret;

	dev_info(&pdev->dev, "driver probed\n");

	isu = devm_kzalloc(&pdev->dev,
		sizeof(struct emi_isu), GFP_KERNEL);
	if (!isu)
		return -ENOMEM;

	ret = of_property_read_u32(emiisu_node,
		"buf_size", &(isu->buf_size));
	if (ret) {
		dev_err(&pdev->dev, "No buf_size\n");
		return -EINVAL;
	}

	ret = of_property_read_u64(emiisu_node, "buf_addr", &addr_temp);
	if (ret) {
		dev_err(&pdev->dev, "No buf_addr\n");
		return -EINVAL;
	}
	if (addr_temp)
		isu->buf_addr = ioremap_wc(
			(phys_addr_t)addr_temp, isu->buf_size);
	else
		isu->buf_addr = NULL;

	ret = of_property_read_u64(emiisu_node, "ver_addr", &addr_temp);
	if (ret) {
		dev_err(&pdev->dev, "No ver_addr\n");
		return -EINVAL;
	}
	if (addr_temp)
		isu->ver_addr = ioremap((phys_addr_t)addr_temp, 4);
	else
		isu->ver_addr = NULL;

	ret = of_property_read_u64(emiisu_node, "con_addr", &addr_temp);
	if (ret) {
		dev_err(&pdev->dev, "No con_addr\n");
		return -EINVAL;
	}
	if (addr_temp)
		isu->con_addr = ioremap((phys_addr_t)addr_temp, 4);
	else
		isu->con_addr = NULL;

	if (isu->con_addr) {
		isu_ctrl_val = readl(isu->con_addr);
		if ((isu_ctrl_val & ISU_CTRL_ENABLED_MASK) == ISU_CTRL_ENABLED_FLAG) {
			/* if ISU compact version, still enable by default */
			isu->enable = 1;
		}
	}

	ret = of_property_read_u32(emiisu_node,
		"ctrl-intf", &(isu->ctrl_intf));
	if (ret) {
		dev_err(&pdev->dev, "No ctrl_intf\n");
		return -EINVAL;
	}

	/* max buffer size = ISU buffer size + 4 bytes of version header */
	emi_isu_buf_attr.size = isu->buf_size + 4;

	ret = sysfs_create_bin_file(&pdev->dev.kobj, &emi_isu_buf_attr);
	if (ret) {
		dev_err(&pdev->dev, "Failed to create buf file\n");
		return ret;
	}

	platform_set_drvdata(pdev, isu);
	global_emi_isu = isu;

	timer_setup(&check_isu_timer, check_isu_timer_callback, 0);

	if ((isu_ctrl_val == ISU_CTRL_ENABLED_STOPED) && isu->con_addr) {
		/* ISU compact version, stop ISU recording by default */
		writel(ISU_CTRL_ENABLED_STOPED, isu->con_addr);
		dsb(sy);
		pr_info("%s: Turn off EMIISU dump\n", __func__);
		set_isu_ctrl_flag(KERN_REC_OFF_PROBE);
	}

	dev_info(&pdev->dev, "%s(%x),%s(%lx),%s(%lx),%s(%lx),%s(%d)\n",
		"buf_size", isu->buf_size,
		"buf_addr", (unsigned long)(isu->buf_addr),
		"ver_addr", (unsigned long)(isu->ver_addr),
		"con_addr", (unsigned long)(isu->con_addr),
		"ctrl_intf", isu->ctrl_intf);

	return 0;
}

static void emiisu_remove(struct platform_device *pdev)
{
	del_timer(&check_isu_timer);

	sysfs_remove_bin_file(&pdev->dev.kobj, &emi_isu_buf_attr);

	global_emi_isu = NULL;
}

static struct platform_driver emiisu_drv = {
	.probe = emiisu_probe,
	.remove = emiisu_remove,
	.driver = {
		.name = "emiisu_drv",
		.owner = THIS_MODULE,
		.of_match_table = emiisu_of_ids,
	},
};

int emiisu_init(void)
{
	int ret;

	pr_info("emiisu wad loaded\n");

	ret = platform_driver_register(&emiisu_drv);
	if (ret) {
		pr_err("emiisu: failed to register dirver\n");
		return ret;
	}

	ret = driver_create_file(&emiisu_drv.driver,
		&driver_attr_emiisu_ctrl);
	if (ret) {
		pr_err("emiisu: failed to create control file\n");
		return ret;
	}

	return 0;
}

MODULE_DESCRIPTION("MediaTek EMI Driver");
MODULE_LICENSE("GPL v2");
