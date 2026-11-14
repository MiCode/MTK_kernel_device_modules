// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Offload Platform Policy Control
 * *
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Yu-chen.Liu <yu-chen.liu@mediatek.com>
 */

#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/string.h>
#include "usb_offload.h"
#include "../usb_xhci/xhci.h"
#include "../usb_xhci/xhci-mtk.h"
#include "mtu3.h"

void usb_offload_hub_working(bool dev_on_hub, bool hold)
{
	struct wakeup_source *ws = uodev->dev->power.wakeup;

	USB_OFFLOAD_DBG("support_hub:%d dev_on_hub:%d hold:%d ws:%p\n",
		uodev->policy.support_hub, dev_on_hub, hold, ws);

	if (!uodev->policy.support_hub || !dev_on_hub || !ws)
		return;

	USB_OFFLOAD_INFO("hold:%d active:%d\n", hold, ws->active);

	if (hold && !ws->active)
		pm_stay_awake(uodev->dev);
	else if (!hold && ws->active)
		pm_relax(uodev->dev);
}

enum uo_provider_type usb_offload_mem_type(void)
{
	if (uodev->policy.all_on_sram)
		return usb_offload_mem_type_lp();

	return UO_PROV_DRAM;
}

enum uo_provider_type usb_offload_mem_type_lp(void)
{
	if (!uodev->policy.adv_lowpwr)
		return UO_PROV_DRAM;

	return uodev->adv_lowpwr ? UO_PROV_SRAM : UO_PROV_DRAM;
}

enum uo_provider_type usb_offload_mem_type_lp_ex(int direction)
{
	if (uodev->policy.adv_lowpwr_dl_only && direction == SNDRV_PCM_STREAM_CAPTURE)
		return UO_PROV_DRAM;

	return usb_offload_mem_type_lp();
}

void usb_offload_improve_idle_power(bool start)
{
	if (uodev->policy.support_idle_lowpwr)
		ssusb_offload_streaming(uodev->ssusb_offload_notify, start);
}

void usb_offload_platform_action(struct device *dev, enum usb_plat_action action)
{
	USB_OFFLOAD_DBG("action:%d\n", action);
}

enum offload_smc_request {
    OFFLOAD_SMC_AUD_SUSPEND = 6,
    OFFLOAD_SMC_AUD_RESUME = 7,
};

char *usb_offload_sram_source_string(enum uo_source_type id)
{
	switch (id) {
	case UO_SOURCE_USB_SRAM:
		return "usb-sram";
	case UO_SOURCE_AFE_SRAM:
		return "afe-sram";
	default:
		return "unknown";
	}
}

enum uo_source_type usb_offload_sram_source_id(const char *buf)
{
	if (!strncmp(buf, "usb-sram", 8))
		return UO_SOURCE_USB_SRAM;
	else if (!strncmp(buf, "afe-sram", 8))
		return UO_SOURCE_AFE_SRAM;
	else
		return UO_SOURCE_NUM;
}

struct policy_member {
	const char *name;
	size_t length;
	size_t offset;
};

#define POLICY_MAP(member, length) \
 { #member, length, offsetof(struct usb_offload_policy, member) }

static struct policy_member flow_control[] = {
	POLICY_MAP(adv_lowpwr,          10),
	POLICY_MAP(force_on_secondary,  18),
	POLICY_MAP(support_fb,          10),
	POLICY_MAP(support_hub,         11),
	POLICY_MAP(hid_disable_offload, 19),
	POLICY_MAP(hid_disable_sync,    16),
	POLICY_MAP(hid_tr_switch,       13),
	POLICY_MAP(support_idle_lowpwr, 19),
	POLICY_MAP(all_on_sram,         11),
	POLICY_MAP(ready_for_xhci,      14),
};

#define MAX_INPUT_NUM   50
static ssize_t flow_ctrl_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_offload_policy *policy = &uodev->policy;
	char buffer[MAX_INPUT_NUM];
	char *input = buffer;
	const char *field1, *field2;
	const char * const delim = " \0\n\t";
	bool enable, *bool_value;
	int i;

	strscpy(buffer, buf, sizeof(buffer) <= count ? sizeof(buffer) : count);
	field1 = strsep(&input, delim);
	field2 = strsep(&input, delim);
	if (!field1 || !field2)
		return -EINVAL;

	if (kstrtobool(field2, &enable))
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(flow_control); i++) {
		bool_value = (void *)policy + flow_control[i].offset;
		if (!strncmp(field1, flow_control[i].name, flow_control[i].length)) {
			*bool_value = enable;
			return count;
		}
	}

	return -EINVAL;
}

static ssize_t flow_ctrl_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	struct usb_offload_policy *policy = &uodev->policy;
	bool *bool_value;
	int size = PAGE_SIZE;
	int i, cnt, total_cnt = 0;

	for (i = 0; i < ARRAY_SIZE(flow_control); i++) {
		bool_value = (void *)policy + flow_control[i].offset;
		cnt = snprintf(buf + total_cnt, size - total_cnt, "%s=%d\n",
			flow_control[i].name, *bool_value);
		if (cnt > 0 && cnt < size)
			total_cnt += cnt;
		else
			break;
	}

	return total_cnt;
}

static DEVICE_ATTR_RW(flow_ctrl);

static ssize_t sram_source_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_offload_policy *policy = &uodev->policy;
	enum uo_source_type id;
	char buffer[MAX_INPUT_NUM];
	char *input = buffer, *field1, *field2;
	const char * const delim = " \0\n\t";

	strscpy(buffer, buf, sizeof(buffer) <= count ? sizeof(buffer) : count);
	field1 = strsep(&input, delim);
	field2 = strsep(&input, delim);
	if (!field1 || !field2)
		return -EINVAL;

	id = usb_offload_sram_source_id(field2);
	if (!strncmp(field2, "main", 4))
		policy->main_sram = id;
	else if (!strncmp(field2, "secondary", 9))
		policy->secondary_sram = id;
	else
		return -EINVAL;

	return count;
}

static ssize_t sram_source_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	struct usb_offload_policy *policy = &uodev->policy;
	char *main_string, *secondary_string;

	main_string = usb_offload_sram_source_string(policy->main_sram);
	secondary_string = usb_offload_sram_source_string(policy->secondary_sram);

	return snprintf(buf, PAGE_SIZE, "main:%s secondary:%s\n", main_string, secondary_string);
}

static DEVICE_ATTR_RW(sram_source);

static u32 source_status_output;
static ssize_t source_status_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
	char buffer[MAX_INPUT_NUM];
	char *input = buffer;
	const char *field1, *field2;
	const char * const delim = " \0\n\t";
	struct uo_provider *provider;
	enum uo_provider_type id;

	strscpy(buffer, buf, sizeof(buffer) <= count ? sizeof(buffer) : count);
	field1 = strsep(&input, delim);
	field2 = strsep(&input, delim);
	if (!field1 || !field2)
		return -EINVAL;

	if (!strncmp(field1, "dram", 4))
		id = UO_PROV_DRAM;
	else if (!strncmp(field1, "main-sram", 9))
		id = UO_PROV_SRAM;
	else if (!strncmp(field1, "secondary-sram", 14))
		id = UO_PROV_SRAM_2;
	else
		goto error;

	provider = &uodev->provider[id];
	if (!provider || !provider->is_init)
		goto error;

	if (!strncmp(field2, "power", 5))
		source_status_output = provider->power;
	else if (!strncmp(field2, "count", 5))
		source_status_output = provider->struct_cnt;
	else
		goto error;

	return count;
error:
	source_status_output = 0;
	return -EINVAL;
}

static ssize_t source_status_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", source_status_output);
}

static DEVICE_ATTR_RW(source_status);

static struct attribute *usb_offload_attrs[] = {
	&dev_attr_sram_source.attr,
	&dev_attr_flow_ctrl.attr,
	&dev_attr_source_status.attr,
	NULL,
};

static const struct attribute_group usb_offload_group = {
	.attrs = usb_offload_attrs,
};

static void usb_offload_link_mtu3(struct device *uo_dev, struct usb_offload_policy *policy)
{
	struct device_node *node;
	struct device *mtu3_dev;
	struct device_link *link;
	struct platform_device *pdev = NULL;

	node = of_find_node_by_name(NULL, "usb0");
	if (node) {
		pdev = of_find_device_by_node(node);
		if (!pdev) {
			USB_OFFLOAD_ERR("no device found by ssusb node!\n");
			policy->hid_disable_offload = true;
			goto put_node;
		}
		mtu3_dev = &pdev->dev;

		link =  device_link_add(uo_dev, mtu3_dev,
						DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS);
		if (!link) {
			USB_OFFLOAD_ERR("fail to link mtu3\n");
			policy->hid_disable_offload = true;
		} else {
			USB_OFFLOAD_ERR("success to link mtu3\n");
			policy->hid_disable_offload = false;
		}
put_node:
		of_node_put(node);
	} else {
		USB_OFFLOAD_ERR("no 'usb0' node!\n");
		policy->hid_disable_offload = true;
	}
}

int usb_offload_link_xhci(struct device *dev)
{
	struct device_node *node;
	struct platform_device *pdev = NULL;
	struct xhci_hcd_mtk *mtk;
	struct xhci_hcd *xhci;
	int ret = -EOPNOTSUPP;

	node = of_parse_phandle(dev->of_node, "xhci-host", 0);
	if (node) {
		pdev = of_find_device_by_node(node);
		if (!pdev) {
			USB_OFFLOAD_ERR("no device found by node!\n");
			goto err;
		}
		of_node_put(node);

		mtk = platform_get_drvdata(pdev);
		if (!mtk) {
			USB_OFFLOAD_ERR("no drvdata set!\n");
			goto err;
		}
		xhci = hcd_to_xhci(mtk->hcd);
		uodev->xhci = xhci;
		ret = 0;
	} else
		uodev->xhci = NULL;
err:
	return ret;
}

void usb_offload_platform_policy_init(struct device *dev, struct usb_offload_policy *policy)
{
	struct device_node *node = dev->of_node;
	const char *sram_source;
	enum uo_source_type id;

	policy->force_on_secondary = false;
	policy->support_fb = of_property_read_bool(node, "mediatek,explicit-feedback");
	policy->support_hub = of_property_read_bool(node, "mediatek,hub-offload");
	policy->support_idle_lowpwr = of_property_read_bool(node, "mediatek,idle-lowpwr");
	policy->all_on_sram = of_property_read_bool(node, "mediatek,all-on-sram");
	policy->ready_for_xhci = of_property_read_bool(node, "mediatek,ready-for-xhci");

	policy->hid_disable_sync = false;
	policy->hid_tr_switch = true;

	usb_offload_link_mtu3(dev, policy);

	if (of_property_read_bool(node, "mediatek,smc-ctrl")) {
		policy->smc_suspend = OFFLOAD_SMC_AUD_SUSPEND;
		policy->smc_resume = OFFLOAD_SMC_AUD_RESUME;
	} else {
		policy->smc_suspend = -1;
		policy->smc_resume = -1;
	}

	/* advanced power mode was enabled as long as main-sram was defined */
	if (!of_property_read_string(node, "mediatek,main-sram", &sram_source)) {
		policy->adv_lowpwr = true;

		id = usb_offload_sram_source_id(sram_source);
		policy->main_sram = id;
		policy->adv_lowpwr = id != UO_SOURCE_NUM ? true : false;

	} else {
		USB_OFFLOAD_ERR("main sram wasn't defined\n");
		policy->adv_lowpwr = false;
	}

	if (policy->adv_lowpwr) {
		if (!of_property_read_string(node, "mediatek,secondary-sram", &sram_source)) {
			id = usb_offload_sram_source_id(sram_source);
			policy->secondary_sram = id;
		} else
			USB_OFFLOAD_ERR("secondary sram wasn't defined\n");
	}

	if (of_property_read_u32(node, "mediatek,reserved-sram-size", &policy->reserved_size)) {
		USB_OFFLOAD_ERR("size of reserved part on main sram wasn't defined\n");
		policy->adv_lowpwr= false;
	}

	policy->adv_lowpwr_dl_only = of_property_read_bool(node, "adv-lowpower-dl-only");

	USB_OFFLOAD_INFO("adv_lowpwr:%d(dl_only:%d) main-sram:%d(%s) secondary-sram:%d(%s) rsv_size:%d\n",
		policy->adv_lowpwr, policy->adv_lowpwr_dl_only,
		policy->main_sram, usb_offload_sram_source_string(policy->main_sram),
		policy->secondary_sram, usb_offload_sram_source_string(policy->secondary_sram),
		policy->reserved_size);

	if (sysfs_create_group(&dev->kobj, &usb_offload_group))
		USB_OFFLOAD_ERR("fail to create sysfs attribtues\n");
}