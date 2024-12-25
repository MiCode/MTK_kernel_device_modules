/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2020-01-20 File created.
 */

#include "fsm_public.h"
#if defined(CONFIG_FSM_SYSFS)
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/version.h>

static uint8_t g_reg_addr;
static char *ch_name[4] = {"pri_l", "pri_r", "sec_l", "sec_r"};
#ifdef CONFIG_FSM_SYSCAL
static int fsm_check_dev_index(int ndev, int index)
{
	struct fsm_dev *fsm_dev;
	int dev_idx;
	int i;

	for (i = 0; i < ndev; i++) {
		fsm_dev = fsm_get_fsm_dev_by_id(i);
		if (fsm_dev == NULL)
			continue;
		dev_idx = fsm_get_index_by_position(fsm_dev->pos_mask);
		if (index == dev_idx)
			return 0;
	}

	return -EINVAL;
}

static ssize_t fsm_re25_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	fsm_config_t *cfg = fsm_get_config();
	struct fsadsp_cmd_re25 cmd_re25;
	int i, re25, size;
	int ret;

	cfg->force_calib = true;
	fsm_set_calib_mode();
	fsm_delay_ms(2500);

	memset(&cmd_re25, 0, sizeof(struct fsadsp_cmd_re25));
	ret = fsm_afe_save_re25(&cmd_re25);
	if (ret) {
		pr_err("save re25 failed:%d", ret);
		cfg->force_calib = false;
		return ret;
	}

	for (i = 0, size = 0; i < cmd_re25.ndev; i++) {
		re25 = cmd_re25.cal_data[i].re25;
		size += scnprintf(buf + size, PAGE_SIZE, "%d,", re25);
	}
	buf[size - 1] = '\n';
	cfg->force_calib = false;

	return size;
}

static ssize_t fsm_f0_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int payload[FSM_CALIB_PAYLOAD_SIZE];
	struct preset_file *pfile;
	struct fsm_afe afe;
	int i, f0, size;
	int ret;

	// fsm_set_calib_mode();
	// fsm_delay_ms(5000);
	afe.module_id = AFE_MODULE_ID_FSADSP_RX;
	afe.port_id = fsm_afe_get_rx_port();
	afe.param_id  = CAPI_V2_PARAM_FSADSP_CALIB;
	afe.op_set = false;
	ret = fsm_afe_send_apr(&afe, payload, sizeof(payload));
	if (ret) {
		pr_err("send apr failed:%d", ret);
		return ret;
	}
	pfile = fsm_get_presets();
	if (!pfile) {
		pr_debug("not found firmware");
		return -EINVAL;
	}

	for (i = 0, size = 0; i < pfile->hdr.ndev; i++) {
		f0 = (fsm_check_dev_index(pfile->hdr.ndev, i) == 0) ?
			payload[3+6*i] : -65535;
		size += scnprintf(buf + size, PAGE_SIZE, "%d,", f0);
	}
	buf[size - 1] = '\n';

	return size;
}
#endif

static ssize_t fsm_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	fsm_version_t version;
	struct preset_file *pfile;
	int dev_count;
	int len = 0;

	fsm_get_version(&version);
	len  = scnprintf(buf + len, PAGE_SIZE, "version: %s\n",
			version.code_version);
	len += scnprintf(buf + len, PAGE_SIZE, "branch : %s\n",
			version.git_branch);
	len += scnprintf(buf + len, PAGE_SIZE, "commit : %s\n",
			version.git_commit);
	len += scnprintf(buf + len, PAGE_SIZE, "date   : %s\n",
			version.code_date);
	pfile = fsm_get_presets();
	dev_count = (pfile ? pfile->hdr.ndev : 0);
	len += scnprintf(buf + len, PAGE_SIZE, "device : [%d, %d]\n",
			dev_count, fsm_dev_count());

	return len;
}

static ssize_t fsm_debug_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t len)
{
	fsm_config_t *cfg = fsm_get_config();
	int value = simple_strtoul(buf, NULL, 0);

	if (cfg) {
		cfg->i2c_debug = !!value;
	}
	pr_info("i2c debug: %s", (cfg->i2c_debug ? "ON" : "OFF"));

	return len;
}

static ssize_t dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fsm_dev *fsm_dev = dev_get_drvdata(dev);
	uint16_t val;
	ssize_t len;
	uint8_t reg;
	int ret;

	if (fsm_dev == NULL)
		return -EINVAL;

	for (len = 0, reg = 0; reg <= 0xCF; reg++) {
		ret = fsm_reg_read(fsm_dev, reg, &val);
		if (ret)
			return ret;
		len += snprintf(buf + len, PAGE_SIZE - len,
				"%02X:%04X%c", reg, val,
				(reg & 0x7) == 0x7 ? '\n' : ' ');
	}
	buf[len - 1] = '\n';

	return len;
}

static ssize_t reg_rw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fsm_dev *fsm_dev = dev_get_drvdata(dev);
	uint16_t val;
	ssize_t len;
	int ret;

	if (fsm_dev == NULL)
		return -EINVAL;

	ret = fsm_reg_read(fsm_dev, g_reg_addr, &val);
	if (ret)
		return ret;

	len = snprintf(buf, PAGE_SIZE, "%02x:%04x\n", g_reg_addr, val);

	return len;
}

static ssize_t reg_rw_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct fsm_dev *fsm_dev = dev_get_drvdata(dev);
	int data;
	int ret;

	g_reg_addr = 0x00;

	if (fsm_dev == NULL)
		return -EINVAL;

	if (count < 2 || count > 9)
		return -EINVAL;

	ret = kstrtoint(buf, 0, &data);
	if (ret)
		return ret;

	if (count < 6) { // "0xXX"
		g_reg_addr = data & 0xFF;
		return count;
	}

	g_reg_addr = data >> 16;
	ret = fsm_reg_write(fsm_dev, g_reg_addr, data & 0xFFFF);
	if (ret)
		return ret;

	return count;
}

#ifdef CONFIG_FSM_SYSCAL
static DEVICE_ATTR(fsm_re25, S_IRUGO, fsm_re25_show, NULL);
static DEVICE_ATTR(fsm_f0, S_IRUGO, fsm_f0_show, NULL);
#endif
static DEVICE_ATTR(fsm_info, S_IRUGO, fsm_info_show, NULL);
static DEVICE_ATTR(fsm_debug, S_IWUSR, NULL, fsm_debug_store);
static DEVICE_ATTR_RO(dump);
static DEVICE_ATTR_RW(reg_rw);

static struct attribute *fs19xx_sysfs_attrs[] = {
#ifdef CONFIG_FSM_SYSCAL
	&dev_attr_fsm_re25.attr,
	&dev_attr_fsm_f0.attr,
#endif
	&dev_attr_fsm_info.attr,
	&dev_attr_fsm_debug.attr,
	&dev_attr_dump.attr,
	&dev_attr_reg_rw.attr,
	NULL,
};

static struct attribute *fs183x_sysfs_attrs[] = {
	&dev_attr_fsm_info.attr,
	&dev_attr_fsm_debug.attr,
	&dev_attr_dump.attr,
	&dev_attr_reg_rw.attr,
	NULL,
};

ATTRIBUTE_GROUPS(fs19xx_sysfs);
ATTRIBUTE_GROUPS(fs183x_sysfs);
static ssize_t aw_cali_class_f0_show(struct  class *class, struct class_attribute *attr, char *buf)
{
	int payload[FSM_CALIB_PAYLOAD_SIZE];
	struct preset_file *pfile;
	struct fsm_afe afe;
	int i, f0;
	ssize_t len = 0;
	int ret;
	unsigned int result = 1;

	afe.module_id = AFE_MODULE_ID_FSADSP_RX;
	afe.port_id = fsm_afe_get_rx_port();
	afe.param_id  = CAPI_V2_PARAM_FSADSP_CALIB;
	afe.op_set = false;
	ret = fsm_afe_send_apr(&afe, payload, sizeof(payload));
	if (ret) {
		pr_err("send apr failed:%d", ret);
		return ret;
	}
	pfile = fsm_get_presets();
	if (!pfile) {
		pr_debug("not found firmware");
		return -EINVAL;
	}

	for (i = 0; i < pfile->hdr.ndev; i++) {
		f0 = (fsm_check_dev_index(pfile->hdr.ndev, i) == 0) ?
			payload[3+6*i] : -65535;
		f0 = f0 / 256;
		len += snprintf(buf+len, PAGE_SIZE-len, "%s:%d", ch_name[i], f0);
	}

	len += snprintf(buf+len, PAGE_SIZE-len, ",result=%d", result);
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");

	return len;
}

static ssize_t aw_cali_class_re_show(struct  class *class, struct class_attribute *attr, char *buf)
{
	int i, re25;
	struct preset_file *preset;
	fsm_dev_t *fsm_dev;
	ssize_t len = 0;
	unsigned int result = 1;

	preset = fsm_get_presets();
	if (!preset) {
		pr_debug("not found firmware");
		return -EINVAL;
	}


	for (i = 0; i < preset->hdr.ndev; i++) {
		fsm_dev = fsm_get_fsm_dev_by_id(i);
		if (fsm_dev == NULL || fsm_skip_device(fsm_dev))
			continue;

		re25 = (fsm_dev->re25 * 1000) / 4096;
		if (re25 < 5500 || re25 > 9500)
			result &= 0;
		len += snprintf(buf+len, PAGE_SIZE-len, "%s:%d mOhms", ch_name[i], re25);
	}

	len += snprintf(buf+len, PAGE_SIZE-len, ",result=%d", result);
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	return len;
}

static ssize_t aw_cali_class_cali_re_show(struct  class *class, struct class_attribute *attr, char *buf)
{
        fsm_config_t *cfg = fsm_get_config();
        struct fsadsp_cmd_re25 cmd_re25;
        int i, re25, size;
        int ret;

        cfg->force_calib = true;
        fsm_set_calib_mode();
        fsm_delay_ms(2500);

        memset(&cmd_re25, 0, sizeof(struct fsadsp_cmd_re25));
        ret = fsm_afe_save_re25(&cmd_re25);
        if (ret) {
                pr_err("save re25 failed:%d", ret);
                cfg->force_calib = false;
                return ret;
        }

        for (i = 0, size = 0; i < cmd_re25.ndev; i++) {
                re25 = cmd_re25.cal_data[i].re25;
                size += scnprintf(buf + size, PAGE_SIZE-size, "%s:%d mOhms ", ch_name[i], re25);
        }
        size += snprintf(buf+size, PAGE_SIZE-size, "\n");
        cfg->force_calib = false;

        return size;
}

static ssize_t aw_cali_class_cali_re_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t len)
{
	int dev, ret;
	int re_data[AW_DEV_CH_MAX] = {0};
	struct fsm_re25_data re25_data;
	struct preset_file *pfile;

	ret = sscanf(buf, "pri_l:%d pri_r:%d sec_l:%d sec_r:%d tert_l:%d tert_r:%d quat_l:%d quat_r:%d",
					&re_data[AW_DEV_CH_PRI_L],
					&re_data[AW_DEV_CH_PRI_R],
					&re_data[AW_DEV_CH_SEC_L],
					&re_data[AW_DEV_CH_SEC_R],
					&re_data[AW_DEV_CH_TERT_L],
					&re_data[AW_DEV_CH_TERT_R],
					&re_data[AW_DEV_CH_QUAT_L],
					&re_data[AW_DEV_CH_QUAT_R]);
	if (ret <= 0) {
		pr_err("unsupport str[%s]", buf);
	}

	pfile = fsm_get_presets();
	if (pfile == NULL) {
		pr_err("not found firmware");
		return -EINVAL;
	}
	re25_data.count = pfile->hdr.ndev;
	for (dev = 0; dev < re25_data.count; dev++) {
		re25_data.re25[dev] = re_data[dev];
	}

	fsm_set_re25_data(&re25_data);
	return len;
}

static ssize_t aw_cali_class_cali_f0_show(struct  class *class, struct class_attribute *attr, char *buf)
{
        int payload[FSM_CALIB_PAYLOAD_SIZE];
        struct preset_file *pfile;
        struct fsm_afe afe;
        int i, f0, size;
        int ret;

        // fsm_set_calib_mode();
        // fsm_delay_ms(5000);
        afe.module_id = AFE_MODULE_ID_FSADSP_RX;
        afe.port_id = fsm_afe_get_rx_port();
        afe.param_id  = CAPI_V2_PARAM_FSADSP_CALIB;
        afe.op_set = false;
        ret = fsm_afe_send_apr(&afe, payload, sizeof(payload));
        if (ret) {
                pr_err("send apr failed:%d", ret);
                return ret;
        }
        pfile = fsm_get_presets();
        if (!pfile) {
                pr_debug("not found firmware");
                return -EINVAL;
        }

        for (i = 0, size = 0; i < pfile->hdr.ndev; i++) {
                f0 = (fsm_check_dev_index(pfile->hdr.ndev, i) == 0) ?
                        payload[3+6*i] : -65535;
                size += snprintf(buf + size, PAGE_SIZE-size, "%s:%d ", ch_name[i], f0);
        }
        size += snprintf(buf+size, PAGE_SIZE-size, "\n");

        return size;
}

static struct class_attribute class_attr_re_show =
		__ATTR(re_show, 0444,
		aw_cali_class_re_show, NULL);

static struct class_attribute class_attr_f0_show =
		__ATTR(f0_show, 0444,
		aw_cali_class_f0_show, NULL);

static struct class_attribute class_attr_re25_calib =
                __ATTR(re25_calib, 0644,
                aw_cali_class_cali_re_show, aw_cali_class_cali_re_store);

static struct class_attribute class_attr_f0_calib =
                __ATTR(f0_calib, 0644,
                aw_cali_class_cali_f0_show, NULL);

static struct class aw_cali_class = {
	.name = "smartpa",
	.owner = THIS_MODULE,
};
int fsm_sysfs_init(struct device *dev)
{
	struct fsm_dev *fsm_dev = dev_get_drvdata(dev);
	int ret;
	if (fsm_dev == NULL)
		return -EINVAL;

	if (fsm_dev->id != 0) {
		pr_err("class node already register");
		return -EINVAL;
	}

	ret = class_register(&aw_cali_class);
	if (ret < 0) {
		pr_err("error creating class node");
		return -EINVAL;
	}
	ret = class_create_file(&aw_cali_class, &class_attr_re_show);
	if (ret)
		pr_err("creat class_attr_re_show fail");

	ret = class_create_file(&aw_cali_class, &class_attr_f0_show);
	if (ret)
		pr_err("creat class_attr_f0_show fail");

        ret = class_create_file(&aw_cali_class, &class_attr_re25_calib);
        if (ret)
                pr_err("creat class_attr_re25_calib fail");

        ret = class_create_file(&aw_cali_class, &class_attr_f0_calib);
        if (ret)
                pr_err("creat class_attr_re25_calib fail");

	if (fsm_dev->is19xx)
		return sysfs_create_group(&dev->kobj, &fs19xx_sysfs_group);
	else
		return sysfs_create_group(&dev->kobj, &fs183x_sysfs_group);
}

void fsm_sysfs_deinit(struct device *dev)
{
	struct fsm_dev *fsm_dev = dev_get_drvdata(dev);

	if (fsm_dev == NULL)
		return;

	class_remove_file(&aw_cali_class, &class_attr_re_show);
	class_remove_file(&aw_cali_class, &class_attr_f0_show);
	class_remove_file(&aw_cali_class, &class_attr_re25_calib);
	class_remove_file(&aw_cali_class, &class_attr_f0_calib);
	class_unregister(&aw_cali_class);
	if (fsm_dev->is19xx)
		sysfs_remove_group(&dev->kobj, &fs19xx_sysfs_group);
	else
		sysfs_remove_group(&dev->kobj, &fs183x_sysfs_group);
}
#endif
