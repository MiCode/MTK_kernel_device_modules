// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kconfig.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include "mtk-smap-common.h"
#include "smap-v6993.h"

static struct mtk_smap *smap_data;
static smap_mbrain_callback mbrain_cb;

int register_smap_mbrain_cb(smap_mbrain_callback smap_mbrain_cb)
{
	if (!smap_mbrain_cb)
		return -ENODATA;

	mbrain_cb = smap_mbrain_cb;

	smap_print("register mbrain callback done\n");
	return 0;
}
EXPORT_SYMBOL(register_smap_mbrain_cb);

int get_smap_mbrain_data(struct smap_mbrain *debug_data)
{
	if (!smap_data) {
		smap_print("No smap_data\n");
		return -EINVAL;
	}

	memcpy(debug_data, &smap_data->debug_data, sizeof(struct smap_mbrain));
	return 0;
}
EXPORT_SYMBOL(get_smap_mbrain_data);

static inline u32 smap_read(u32 offset)
{
	int len;

	if (!smap_data) {
		smap_print("No smap_data\n");
		return 0;
	}

	len = readl(smap_data->regs + offset);
	return len;
}

static inline void smap_write(u32 offset, u32 val)
{
	if (!smap_data) {
		smap_print("No smap_data\n");
		return;
	}

	writel(val, smap_data->regs + offset);
}

static int smap_set_entry(struct smap_entry *entry)
{
	int i, ret = 0;

	for (i = 0; i < SMAP_INIT_SETTING_ARRAY_LEN; i++) {
		if (entry[i].addr == 0)
			break;
		if (entry[i].mask == 1)
			continue;

		smap_write(entry[i].addr, entry[i].value);
	}

	return ret;
}

static void smap_init(enum SMAP_MODE mode)
{
	smap_print("mode: %d\n", mode);

	if (!smap_data) {
		smap_print("No smap_data\n");
		return;
	}

	if (mode == MODE_NORMAL)
		smap_set_entry(SMAP_NORMAL_MODE_ENTRY);
	else if (mode == MODE_TEST1)
		smap_set_entry(SMAP_DVT_MODE_ENTRY);
	else
		smap_print("wrong mode\n");
}

static ssize_t dump_and_send_smap_staus(char *buf, enum SMAP_DUMP_LOG_TYPE log_type,
	enum SMAP_SEND_LOG_TYPE send_type)
{
	unsigned int dm_cnt, len = 0;
	struct smap_mbrain *dbg;
	struct timespec64 tv = {0};

	if (!smap_data) {
		smap_print("No smap_data\n");
		return -ENODATA;
	}

	dbg = &smap_data->debug_data;

	dm_cnt = smap_read(VSMR_LEN_CON);
	dbg->enable = smap_read(SMAP_ENABLE);
	dbg->dump_cnt = dm_cnt & 0xFFFF;
	dbg->mitigation_cnt = dm_cnt >> 16;
	if (dbg->dump_cnt != 0)
		dbg->mitigation_rate = dbg->mitigation_cnt * 100 / dbg->dump_cnt;
	else
		dbg->mitigation_rate = 0;
	dbg->dect_cnt = smap_read(PMSR_RESERVED_RW_REG_4);
	dbg->temp_cnt = smap_read(PMSR_RESERVED_RW_REG_5);
	dbg->sys_time = smap_read(PMSR_RESERVED_RW_REG_6);
	dbg->dect_result = smap_read(PMSR_RESERVED_RW_REG_7);
	dbg->dyn_base = (smap_data->debug_data.dect_result >> 4) & 0xFFF;
	dbg->cg_subsys_dyn = (smap_data->debug_data.dect_result >> 16) & 0xFFF;
	dbg->cg_ratio = (smap_data->debug_data.dect_result >> 28) & 0xF;
	dbg->dram0_smap_snapshot = smap_read(DRAM0_SMAP_SNAPSHOT);
	dbg->dram1_smap_snapshot = smap_read(DRAM1_SMAP_SNAPSHOT);
	dbg->dram2_smap_snapshot = smap_read(DRAM2_SMAP_SNAPSHOT);
	dbg->dram3_smap_snapshot = smap_read(DRAM3_SMAP_SNAPSHOT);
	dbg->chinf0_smap_snapshot = smap_read(CHINF0_SMAP_SNAPSHOT);
	dbg->chinf1_smap_snapshot = smap_read(CHINF1_SMAP_SNAPSHOT);
	dbg->venc0_smap_snapshot = smap_read(VENC0_SMAP_SNAPSHOT);
	dbg->venc1_smap_snapshot = smap_read(VENC1_SMAP_SNAPSHOT);
	dbg->venc2_smap_snapshot = smap_read(VENC2_SMAP_SNAPSHOT);
	dbg->emi_snapshot = smap_read(EMI_SMAP_SNAPSHOT);
	dbg->emi_s_snapshot = smap_read(EMI_SMAP_SOUTH_SNAPSHOT);
	dbg->zram_snapshot = smap_read(ZRAM_SMAP_SNAPSHOT);
	dbg->apu_snapshot = smap_read(APU_SMAP_SNAPSHOT);

	smap_write(SMAP_SNAPSHOT_CLR, 0x1);
	smap_write(SMAP_SNAPSHOT_CLR, 0x0);
	smap_write(VSMR_LEN_CON, 0x0);

	if (log_type == DUMP_HEADER) {
		len += snprintf(buf + len, PAGE_SIZE - len, "ENABLE=%u\n", dbg->enable);
		len += snprintf(buf + len, PAGE_SIZE - len, "MITIGATION_RATE=%u%%\n", dbg->mitigation_rate);
		len += snprintf(buf + len, PAGE_SIZE - len, "MITIGATION_CNT=%u\n", dbg->mitigation_cnt);
		len += snprintf(buf + len, PAGE_SIZE - len, "Dump_CNT=%u\n", dbg->dump_cnt);
		len += snprintf(buf + len, PAGE_SIZE - len, "DECT_CNT=%u\n", dbg->dect_cnt);
		len += snprintf(buf + len, PAGE_SIZE - len, "TEMP_CNT=%u\n", dbg->temp_cnt);
		len += snprintf(buf + len, PAGE_SIZE - len, "SYS_TIME=%u\n", dbg->sys_time);
		len += snprintf(buf + len, PAGE_SIZE - len, "DECT_RESULT=0x%x\n", dbg->dect_result);
		len += snprintf(buf + len, PAGE_SIZE - len, "DYN_BASE=0x%x\n", dbg->dyn_base);
		len += snprintf(buf + len, PAGE_SIZE - len, "CG_SUBSYS_DYN=0x%x\n",
			dbg->cg_subsys_dyn);
		len += snprintf(buf + len, PAGE_SIZE - len, "CG_RATIO=0x%x\n", dbg->cg_ratio);
		len += snprintf(buf + len, PAGE_SIZE - len,
			"SNAPSHOT[DRAM0/1/2/3]=0x%x,0x%x,0x%x,0x%x",
			dbg->dram0_smap_snapshot, dbg->dram1_smap_snapshot,
			dbg->dram2_smap_snapshot, dbg->dram3_smap_snapshot);
		len += snprintf(buf + len, PAGE_SIZE - len, "[CHINF0/1]=0x%x,0x%x",
			dbg->chinf0_smap_snapshot, dbg->chinf1_smap_snapshot);
		len += snprintf(buf + len, PAGE_SIZE - len, "[VENC0/1/2]=0x%x,0x%x,0x%x",
			dbg->venc0_smap_snapshot, dbg->venc1_smap_snapshot,
			dbg->venc2_smap_snapshot);
		len += snprintf(buf + len, PAGE_SIZE - len,
			"[EMI/EMI_S/ZRAM/APU]=0x%x,0x%x,0x%x,0x%x", dbg->emi_snapshot,
			dbg->emi_s_snapshot, dbg->zram_snapshot, dbg->apu_snapshot);
	} else if (log_type == DUMP_NO_HEADER)
		len += snprintf(buf + len, PAGE_SIZE - len,
			"%u,%u,%u,%u,%u,%u,%u,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x\n",
			dbg->enable, dbg->dect_cnt,
			dbg->mitigation_rate, dbg->mitigation_cnt, dbg->dump_cnt,
			dbg->temp_cnt, dbg->sys_time, dbg->dect_result,
			dbg->dyn_base, dbg->cg_subsys_dyn, dbg->cg_ratio,
			dbg->dram0_smap_snapshot, dbg->dram1_smap_snapshot,
			dbg->dram2_smap_snapshot, dbg->dram3_smap_snapshot,
			dbg->chinf0_smap_snapshot, dbg->chinf1_smap_snapshot,
			dbg->venc0_smap_snapshot, dbg->venc1_smap_snapshot,
			dbg->venc2_smap_snapshot, dbg->emi_snapshot,
			dbg->emi_s_snapshot, dbg->zram_snapshot, dbg->apu_snapshot);
	else if (log_type == DUMP_KERNEL) {
		len += snprintf(buf + len, PAGE_SIZE - len, "ENABLE=%u ", dbg->enable);
		len += snprintf(buf + len, PAGE_SIZE - len, "MITIGATION_RATE=%u%% ", dbg->mitigation_rate);
		len += snprintf(buf + len, PAGE_SIZE - len, "MITIGATION_CNT=%u ", dbg->mitigation_cnt);
		len += snprintf(buf + len, PAGE_SIZE - len, "Dump_CNT=%u ", dbg->dump_cnt);
		len += snprintf(buf + len, PAGE_SIZE - len, "DECT_CNT=%u ", dbg->dect_cnt);
		len += snprintf(buf + len, PAGE_SIZE - len, "TEMP_CNT=%u ", dbg->temp_cnt);
		len += snprintf(buf + len, PAGE_SIZE - len, "SYS_TIME=%u ", dbg->sys_time);
		len += snprintf(buf + len, PAGE_SIZE - len, "DECT_RESULT=0x%x ", dbg->dect_result);
		len += snprintf(buf + len, PAGE_SIZE - len, "DYN_BASE=0x%x ", dbg->dyn_base);
		len += snprintf(buf + len, PAGE_SIZE - len, "CG_SUBSYS_DYN=0x%x ",
			dbg->cg_subsys_dyn);
		len += snprintf(buf + len, PAGE_SIZE - len, "CG_RATIO=0x%x ", dbg->cg_ratio);
		len += snprintf(buf + len, PAGE_SIZE - len,
			"SNAPSHOT[DRAM0/1/2/3]=0x%x,0x%x,0x%x,0x%x",
			dbg->dram0_smap_snapshot, dbg->dram1_smap_snapshot,
			dbg->dram2_smap_snapshot, dbg->dram3_smap_snapshot);
		len += snprintf(buf + len, PAGE_SIZE - len, "[CHINF0/1]=0x%x,0x%x",
			dbg->chinf0_smap_snapshot, dbg->chinf1_smap_snapshot);
		len += snprintf(buf + len, PAGE_SIZE - len, "[VENC0/1/2]=0x%x,0x%x,0x%x",
			dbg->venc0_smap_snapshot, dbg->venc1_smap_snapshot,
			dbg->venc2_smap_snapshot);
		len += snprintf(buf + len, PAGE_SIZE - len,
			"[EMI/EMI_S/ZRAM/APU]=0x%x,0x%x,0x%x,0x%x",
			dbg->emi_snapshot, dbg->emi_s_snapshot,
			dbg->zram_snapshot, dbg->apu_snapshot);
	}

	if (send_type == SEND_MBRAIN && mbrain_cb) {
		ktime_get_real_ts64(&tv);
		dbg->real_time_end = (tv.tv_sec*1000) + (tv.tv_nsec/1000000);
		dbg->real_time_start = dbg->real_time_end - smap_data->delay_ms;
		mbrain_cb(dbg);
	}

	smap_print("Log type:%u, Send type:%u\n", log_type, send_type);

	return len;
}

static void smap_update_result(struct mtk_smap *smap)
{
	static unsigned int last_sys_time;
	unsigned int len, enable, sys_time;
	char buf[STR_SIZE];

	if (!smap_data) {
		smap_print("No smap_data\n");
		return;
	}

	enable = smap_read(SMAP_ENABLE);
	sys_time = smap_read(PMSR_RESERVED_RW_REG_6);

	if (sys_time != last_sys_time) {
		len = dump_and_send_smap_staus(buf, DUMP_KERNEL, SEND_MBRAIN);
		smap_print("Mitigation happened, SMAP enalbe:%u\n", enable);
	} else
		len = dump_and_send_smap_staus(buf, DUMP_KERNEL, NO_SEND);

	if (len)
		smap_print("%s\n", buf);

	last_sys_time = sys_time;
}

static void smap_periodic_work_handler(struct work_struct *work)
{
	struct mtk_smap *smap =
		container_of(work, struct mtk_smap, defer_work.work);

	smap_update_result(smap);

	queue_delayed_work(smap->smap_wq, &smap->defer_work,
		msecs_to_jiffies(smap->delay_ms));
}

static ssize_t smap_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int len = 0;

	if (!smap_data) {
		smap_print("No smap_data\n");
		return -ENODATA;
	}

	len = dump_and_send_smap_staus(buf, DUMP_HEADER, NO_SEND);

	return len;
}
static DEVICE_ATTR_RO(smap_status);

static ssize_t smap_dbg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int len = 0;

	if (!smap_data) {
		smap_print("No smap_data\n");
		return -ENODATA;
	}

	len = dump_and_send_smap_staus(buf, DUMP_NO_HEADER, NO_SEND);

	return len;
}

static ssize_t smap_dbg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char cmd[20];
	u32 val = 0;
	int ret;

	if (!smap_data) {
		smap_print("No smap_data\n");
		return -ENODATA;
	}

	ret = sscanf(buf, "%19s %d", cmd, &val);
	if (ret < 1) {
		ret = -EPERM;
		goto out;
	}

	if (!strcmp(cmd, "clear")) {
		smap_write(PMSR_RESERVED_RW_REG_4, 0);
		smap_write(PMSR_RESERVED_RW_REG_5, 0);
		smap_write(PMSR_RESERVED_RW_REG_6, 0);
		smap_write(PMSR_RESERVED_RW_REG_7, 0);
	} else if (!strcmp(cmd, "enable")) {
		if (val)
			smap_write(SMAP_ENABLE, 0x1);
		else
			smap_write(SMAP_ENABLE, 0x0);
	} else if (!strcmp(cmd, "mon_en")) {
		if (smap_data->smap_wq) {
			cancel_delayed_work_sync(&smap_data->defer_work);
			if (val) {
				queue_delayed_work(smap_data->smap_wq, &smap_data->defer_work,
					msecs_to_jiffies(smap_data->delay_ms));
			}
		}
	}
out:
	if (ret < 0)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(smap_dbg);

static ssize_t smap_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len = 0;

	if (!smap_data) {
		smap_print("No smap_data\n");
		return -ENODATA;
	}

	len += snprintf(buf + len, PAGE_SIZE, "smap mode:%d\n", smap_data->mode);
	return len;
}

static ssize_t smap_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char desc[64];
	unsigned int len = 0, mode = 0;
	int ret;

	if (!smap_data) {
		smap_print("No smap_data\n");
		return -ENODATA;
	}

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buf, len))
		return 0;

	desc[len] = '\0';

	ret = kstrtouint(desc, 10, &mode);
	if (ret) {
		smap_print("parameter number not correct\n");
		return -EPERM;
	}

	if (mode < MODE_NUM) {
		smap_data->mode = mode;
		smap_init(mode);
	}

	return 0;
}
static DEVICE_ATTR_RW(smap_mode);


static struct attribute *smap_sysfs_attrs[] = {
	&dev_attr_smap_status.attr,
	&dev_attr_smap_dbg.attr,
	&dev_attr_smap_mode.attr,
	NULL,
};

static struct attribute_group smap_sysfs_attr_group = {
	.attrs = smap_sysfs_attrs,
};

int smap_register_sysfs(struct device *dev)
{
	int ret;

	ret = sysfs_create_group(&dev->kobj, &smap_sysfs_attr_group);
	if (ret)
		return ret;

	ret = sysfs_create_link(kernel_kobj, &dev->kobj, "smap");

	return ret;
}

static int smap_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct mtk_smap *priv;
	struct device_node *np = pdev->dev.of_node;
	u32 smap_mode = 0;
	u32 interval = 0;

	priv = devm_kzalloc(dev, sizeof(struct mtk_smap), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	priv->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(priv->regs))
		return PTR_ERR(priv->regs);

	priv->dev = dev;
	of_property_read_u32(np, "smap-mode", &smap_mode);
	priv->mode = smap_mode;
	priv->def_disable= of_property_read_bool(np, "smap-disable");
	if (priv->def_disable)
		return -EINVAL;

	of_property_read_u32(np, "smap-mon-interval", &interval);
	if (interval < 500)
		priv->delay_ms = 1000 * 3;
	else
		priv->delay_ms = interval;

	priv->smap_wq = create_singlethread_workqueue("smap_wq");
	INIT_DELAYED_WORK(&priv->defer_work, smap_periodic_work_handler);
	if (of_property_read_bool(np, "smap-mon-enable")) {
		if (priv->smap_wq) {
			queue_delayed_work(priv->smap_wq, &priv->defer_work,
				msecs_to_jiffies(priv->delay_ms));
		}
	}

	smap_register_sysfs(dev);
	ret = devm_of_platform_populate(dev);
	if (ret)
		smap_print("devm_of_platform_populate Failed\n");

	platform_set_drvdata(pdev, priv);
	smap_data = priv;

	smap_init(priv->mode);
	smap_print("IC[val:mask]=%u,%u, TEMP[val:mask]=%u,%u\n",
		smap_read(MC99_IC), smap_read(MC99_IC_MASK_EN),
		smap_read(HIGH_TEMP), smap_read(HIGH_TEMP_MASK_EN));

	return 0;
}

static void smap_remove(struct platform_device *pdev)
{
	struct mtk_smap *smap = platform_get_drvdata(pdev);

	if (smap->smap_wq) {
		cancel_delayed_work_sync(&smap->defer_work);
		destroy_workqueue(smap->smap_wq);
	}
}

static const struct of_device_id smap_of_match[] = {
	{ .compatible = "mediatek,mt6993-smap", },
	{}
};

static struct platform_driver smap_pdrv = {
	.probe = smap_probe,
	.remove = smap_remove,
	.driver = {
		.name = "smap",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(smap_of_match),
	},
};

static int __init smap_module_init(void)
{
	return platform_driver_register(&smap_pdrv);
}
module_init(smap_module_init);

static void __exit smap_module_exit(void)
{
	platform_driver_unregister(&smap_pdrv);
}
module_exit(smap_module_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MTK SMAP driver");
