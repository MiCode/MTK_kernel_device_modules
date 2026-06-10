// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek eUSB2 MT6379 Repeater Driver
 *
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>
#include <linux/delay.h>
#include <linux/bits.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/pm_wakeup.h>
#include <linux/pm_wakeirq.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "phy-mtk-io.h"
#include "../../misc/mediatek/extcon/extcon-mtk-usb.h"

/* MT6379 eUSB2 control registers */
#define RG_USB_EN_SRC_SEL		(1 << 5)
#define RG_USB20_BC11_SW_EN		(1 << 0)
#define RG_USB20_REV_A_5		(1 << 5)
#define RG_USB20_HSRX_BIAS_EN_SEL	(1 << 2)
#define RG_FM_DUMMY			(1 << 4)
#define RG_INIT_SW_PHY_SEL		(1 << 7)
#define RG_USB20_BGR_EN			(1 << 0)
#define RG_USB20_ROSC_EN		(1 << 1)
#define RG_USB20_REV_COM		(0x3 << 6)
#define RG_EUSB20_HSRX_BIAS_EN_SEL		(0x3 << 2)
#define RG_RFC_USB20_HSRX_BIAS_EN_SEL		(0x1)
#define RG_USB20_HSRX_BIAS_EN_SEL_LP		(0x3 << 1)
#define RG_USB20_HSTX_SRCTRL		(0x1 << 3)
#define RG_EUSB_LOGRST		(0x1)

#define PHYA_U2_CR0_0			0x10
#define RG_USB20_VRT_SEL		GENMASK(6,4)
#define RG_USB20_VRT_SEL_SHIFT		4
#define RG_USB20_VRT_SEL_MASK		0x7

#define PHYA_U2_CR0_1			0x11
#define RG_USB20_INTR_CAL		GENMASK(5,0)
#define RG_USB20_INTR_CAL_SHIFT		0
#define RG_USB20_INTR_CAL_MASK		0x3F

#define PHYA_U2_CR1_0			0x14
#define RG_USB20_FS_SR			GENMASK(2,0)
#define RG_USB20_FS_SR_SHIFT		0
#define RG_USB20_FS_SR_MASK		0x7

#define PHYA_U2_CR1_3			0x17
#define RG_USB20_SQD			GENMASK(1,0)
#define RG_USB20_SQD_SHIFT		0
#define RG_USB20_SQD_MASK		0x3

#define PHYA_U2_CR2_0			0x18
#define RG_USB20_SQTH			GENMASK(3,0)
#define RG_USB20_SQTH_SHIFT		0
#define RG_USB20_SQTH_MASK		0xF

#define RG_USB20_DISCTH			GENMASK(7,4)
#define RG_USB20_DISCTH_SHIFT		4
#define RG_USB20_DISCTH_MASK		0xF

#define PHYA_U2_CR2_2			0x1A
#define RG_USB20_TERM_CAL		GENMASK(3,0)
#define RG_USB20_TERM_CAL_SHIFT		0
#define RG_USB20_TERM_CAL_MASK		0xF

#define PHYA_U2_CR2_3			0x1B
#define RG_USB20_HS_EQ			GENMASK(3,0)
#define RG_USB20_HS_EQ_SHIFT		0
#define RG_USB20_HS_EQ_MASK		0xF

#define RG_USB20_HS_PE			GENMASK(6,4)
#define RG_USB20_HS_PE_SHIFT		4
#define RG_USB20_HS_PE_MASK		0x7

#define PHYA_EU2_CR0_0			0x20
#define RG_EUSB20_TXLDO_VREF_SEL	BIT(2)

#define PHYA_EU2_CR1_0			0x24
#define RG_EUSB20_FS_CR			GENMASK(2,0)
#define RG_EUSB20_FS_CR_SHIFT	0
#define RG_EUSB20_FS_CR_MASK		0x7

#define RG_EUSB20_LS_CR			GENMASK(6,4)
#define RG_EUSB20_LS_CR_SHIFT	4
#define RG_EUSB20_LS_CR_MASK		0x7

#define PHYA_EU2_CR1_3			0x27
#define RG_EUSB20_FSRX_HYS_SEL	GENMASK(7,6)
#define RG_EUSB20_FSRX_HYS_SEL_SHIFT	6
#define RG_EUSB20_FSRX_HYS_SEL_MASK		0x3

#define PHYA_EU2_CR2_0			0x28
#define RG_EUSB20_HSRX_TERM_CAL		GENMASK(7,4)
#define RG_EUSB20_HSRX_TERM_CAL_SHIFT	4
#define RG_EUSB20_HSRX_TERM_CAL_MASK	0xF

#define PHYA_EU2_CR2_1			0x29
#define RG_EUSB20_HSTX_IMPN		GENMASK(4,0)
#define RG_EUSB20_HSTX_IMPN_SHIFT	0
#define RG_EUSB20_HSTX_IMPN_MASK	0x1F

#define PHYA_EU2_CR2_2			0x2A
#define RG_EUSB20_HSTX_IMPP		GENMASK(4,0)
#define RG_EUSB20_HSTX_IMPP_SHIFT	0
#define RG_EUSB20_HSTX_IMPP_MASK	0x1F

#define PHYA_U2_EXT_CR0_0		0x30
#define RG_USB20_OSC_CALI		GENMASK(6,0)
#define RG_USB20_OSC_CALI_SHIFT		0
#define RG_USB20_OSC_CALI_MASK		0x7F

#define PHYA_U2_EXT_CR0_1               0x31
#define RG_USB20_OSC_IBAND              GENMASK(7,0)
#define RG_USB20_OSC_IBAND_SHIFT	0
#define RG_USB20_OSC_IBAND_MASK		0xFF

#define PHYA_U2_EXT_CR1_0		0x34
#define RH_USB20_OSC_BUF_0		BIT(0)
#define RH_USB20_OSC_BUF_0_SHIFT	0
#define RH_USB20_OSC_BUF_0_MASK		0x1

#define PHYA_U2_EXT_CR1_1		0x35
#define RG_USB20_OSC_BIAS		GENMASK(7,0)
#define RG_USB20_OSC_BIAS_SHIFT		0
#define RG_USB20_OSC_BIAS_MASK		0xFF

#define RG_USB20_OSC_BIAS_0		BIT(4)

#define PHYD_COM_CR2_0			0x88
#define RG_USB20_MANU_MODE		BIT(3)
#define RG_USB20_PU_DP			BIT(6)

#define PHYD_COM_CR2_1			0x89
#define RG_INIT_SW_SEL			BIT(6)
#define RG_INIT_SW_SEL_SHIFT		6
#define RG_INIT_SW_SEL_MASK		0x1

#define PHYD_COM_CR2_2			0x8A
#define RG_RX_DBC_CNT			GENMASK(2,0)
#define RG_RX_DBC_CNT_SHIFT		0
#define RG_RX_DBC_CNT_MASK		0x7

#define PHYA_COM_CR0_0			0x0
#define PHYA_COM_CR0_2			0x2
#define PHYA_COM_CR0_3			0x3
#define PHYA_U2_CR0_2			0x12
#define PHYA_U2_CR0_3			0x13
#define PHYA_U2_CR1_2			0x16
#define PHA_EU2_CR0_1			0x21
#define PHA_EU2_CR0_2			0x22
#define PHYA_EU2_CR1_2			0x27
#define PHYD_COM_CR2_3			0x8b

#define PHYD_COM_CR3_2			0x8E
#define RG_USB20_LPM_EN			BIT(1)

#define PHYA_U2_CR2_1			0x19
#define PHYD_FM_CR0_1			0xA5
#define PHYD_DBG_CR0_1			0x91

/* MT6379 VID */
#define MT6379_REG_DEV_INFO	0x00
#define MT6379_CHIP_REV_MASK	GENMASK(3, 0)
#define MT6379_VENID_MASK	GENMASK(7, 4)
#define MT6379_VENDOR_ID	0x70

/* EUSB EFUSE */
#define EUSB_RSV_23	0x323
#define EUSB_RSV_24	0x324
#define EUSB_RSV_25	0x325
#define EUSB_RSV_26	0x326
#define EUSB_RSV_27	0x327
#define EUSB_RSV_81	0x381
#define EUSB_RSV_82	0x382

/* EFUSE TOP MASK */
#define RG_USB20_INTR_CAL_TOP_MASK	 0x3F
#define RG_USB20_TERM_CAL_TOP_MASK	 0xF
#define RG_EUSB20_HSTX_IMPN_TOP_MASK	 0xF8
#define RG_EUSB20_HSTX_IMPP_TOP_MASK	 0xF8
#define RG_EUSB20_HSRX_TERM_CAL_TOP_MASK 0xF0
#define RG_USB20_OSC_CALI_TOP_MASK	 0x7F
#define RG_EUSB20_FS_CR_TOP_MASK		 0x7
#define RG_EUSB20_LS_CR_TOP_MASK		 0x7

/* EFUSE TOP SHIFT */
#define RG_EUSB20_HSTX_IMPN_TOP_SFT	3
#define RG_EUSB20_HSTX_IMPP_TOP_SFT	3

/* U2 */
#define MTK_USB_STR		"mtk_eusb2"
#define VRT_SEL_STR		"vrt_sel"
#define DISCTH_STR		"discth"
#define RX_SQTH_STR		"rx_sqth"
#define PRE_EMP_STR		"pre_emphasis"
#define EQ_STR			"equalization"
#define INTR_OFS_STR		"intr_ofs"
#define TERM_OFS_STR		"term_ofs"
#define EUSB2_SQTH_STR		"eusb2_sqth"
#define EUSB2_HSTX_SR_STR	"eusb2_hstx_sr"
#define EUSB2_REV_COM		"eusb2_tx_swing_enhance"

#define REPEATER_SYSFS_CLASS_NAME "usb_enhance"
#define REPEATER_SYSFS_DEVICE_NAME "usb_enhance"

struct eusb2_repeater {
	struct device *dev;
	struct regmap *regmap;
	struct phy *phy;
	struct delayed_work dwork;
	u16 base;
	/* sw efuse */
	int intr_cal;
	int term_cal;
	int osc_cali;
	int hstx_impn;
	int hstx_impp;
	int hsrx_term_cal;
	int fs_cr;
	int ls_cr;
	int intr_ofs;
	int term_ofs;
	int host_intr_ofs;
	int host_term_ofs;
	/* Device */
	int vrt_sel;
	int rx_sqth;
	int discth;
	int pre_emphasis;
	int equalization;
	int device_prop_num;
	struct eusb2_prop_table *device_prop_table;
	/* Host */
	int host_vrt_sel;
	int host_rx_sqth;
	int host_discth;
	int host_pre_emphasis;
	int host_equalization;
	int host_prop_num;
	struct eusb2_prop_table *host_prop_table;
	int submode;
	/* Common */
	int usb20_fs_sr;
	int eusb20_fsrx_hys_sel;
	int prop_index;
	enum phy_mode mode;
	struct proc_dir_entry *root;
	struct work_struct procfs_work;
	struct workqueue_struct *wq;
	u8			repeater_enhance_support_mode;
	u8			repeater_enhance_state;
	u8			repeater_current_eye_tune;
	u8			repeater_product_enable;
	u8			repeater_storage_enable;
	u8			repeater_carwith_enable;
	u8			repeater_global_enable;
	u8			repeater_not_single_port;
	u8			repeater_which_port;
	u8			repeater_dynamic_adjustment;
	u32			*param_override_seq;
	u8			param_override_seq_cnt;
	u32			*param_override_seq_port1;
	u8			param_override_seq_cnt_port1;
	u32			*param_override_seq_carwith;
	u8			param_override_seq_cnt_carwith;
	u32			*param_override_seq_carwith_port1;
	u8			param_override_seq_cnt_carwith_port1;
	u32			*host_param_override_seq;
	u8			host_param_override_seq_cnt;
	u32			*host_param_override_seq_port1;
	u8			host_param_override_seq_cnt_port1;
	u32			*host_param_override_seq_storage;
	u8			host_param_override_seq_cnt_storage;
	u32			*host_param_override_seq_storage_port1;
	u8			host_param_override_seq_cnt_storage_port1;
	/*sysfs*/
	struct device *sysfs_dev;
	bool otg_gender;
};

#define EUSB2_PROP_NUM 7

struct eusb2_prop_table {
	int vrt_sel;
	int rx_sqth;
	int discth;
	int pre_emphasis;
	int equalization;
	int term_ofs;
	int intr_ofs;
};

typedef enum{
	TYPE_HOST_NORMAL = 0,
	TYPE_HOST_STORAGE = 1,
	TYPE_DEVICE_NORMAL = 0,
	TYPE_DEVICE_CARWITH = 2,
}Repeater_Type;

static void eusb2_rptr_prop_table_parse(struct eusb2_repeater *rptr)
{
	struct device *dev = rptr->dev;
	struct device_node *np = dev->of_node;
	struct eusb2_prop_table *prop_table;
	const __be32 *list;
	u32 size = 0;
	int i;

	/* get and save device prop table */
	list = of_get_property(np, "mediatek,u2-device-prop-table", &size);
	of_node_put(np);
	if (!list)
		goto host_prop_table;

	size = size / sizeof(__be32);
	if (size % EUSB2_PROP_NUM || (size / EUSB2_PROP_NUM) > PHY_MODE_PROPERTY_MAX) {
		dev_info(dev, "%s mediatek,u2-device-prop-table size=%d\n", __func__, size);
		goto host_prop_table;
	}

	rptr->device_prop_num = (size / EUSB2_PROP_NUM) + 1;

	dev_info(dev, "%s device_prop_table=%d size=%d\n", __func__, rptr->device_prop_num, size);

	rptr->device_prop_table = kcalloc(rptr->device_prop_num,
			sizeof(*rptr->device_prop_table), GFP_KERNEL);
	if (!rptr->device_prop_table) {
		rptr->device_prop_num = 0;
		goto host_prop_table;
	}


	prop_table = &rptr->device_prop_table[0];
	/* save the default prop value */
	prop_table->vrt_sel = rptr->vrt_sel;
	prop_table->rx_sqth = rptr->rx_sqth;
	prop_table->discth = rptr->discth;
	prop_table->pre_emphasis = rptr->pre_emphasis;
	prop_table->equalization = rptr->equalization;
	prop_table->term_ofs = rptr->term_ofs;
	prop_table->intr_ofs = rptr->intr_ofs;

	for (i = 1; i < rptr->device_prop_num; i++) {
		prop_table = &rptr->device_prop_table[i];
		/* update prop value */
		prop_table->vrt_sel = be32_to_cpu(*list++);
		prop_table->rx_sqth = be32_to_cpu(*list++);
		prop_table->discth = be32_to_cpu(*list++);
		prop_table->pre_emphasis = be32_to_cpu(*list++);
		prop_table->equalization = be32_to_cpu(*list++);
		prop_table->term_ofs = be32_to_cpu(*list++);
		prop_table->intr_ofs = be32_to_cpu(*list++);

		dev_info(dev, "prop table%d, vrt-vref:%d, rx-sqth:%d, discth:%d\n",
			i, prop_table->vrt_sel, prop_table->rx_sqth, prop_table->discth);
		dev_info(dev, "pre-emphasis:%d, eq:%d, term-ofs:%d, intr-ofs:%d\n",
			prop_table->pre_emphasis, prop_table->equalization, prop_table->term_ofs,
			prop_table->intr_ofs);
	}

host_prop_table:
	/* get and save host prop table */
	list = of_get_property(np, "mediatek,u2-host-prop-table", &size);
	of_node_put(np);
	if (!list)
		return;

	size = size / sizeof(__be32);
	if (size % EUSB2_PROP_NUM || (size / EUSB2_PROP_NUM) > PHY_MODE_PROPERTY_MAX) {
		dev_info(dev, "%s mediatek,u2-host-prop-table size=%d\n", __func__, size);
		return;
	}

	rptr->host_prop_num = (size / EUSB2_PROP_NUM) + 1;
	rptr->host_prop_table = kcalloc(rptr->host_prop_num,
			sizeof(*rptr->host_prop_table), GFP_KERNEL);
	if (!rptr->host_prop_table) {
		rptr->host_prop_num = 0;
		return;
	}

	prop_table = &rptr->host_prop_table[0];
	/* save the default prop value */
	prop_table->vrt_sel = rptr->host_vrt_sel;
	prop_table->rx_sqth = rptr->host_rx_sqth;
	prop_table->discth = rptr->host_discth;
	prop_table->pre_emphasis = rptr->host_pre_emphasis;
	prop_table->equalization = rptr->host_equalization;
	prop_table->term_ofs = rptr->host_term_ofs;
	prop_table->intr_ofs = rptr->host_intr_ofs;

	for (i = 1; i < rptr->host_prop_num; i++) {
		prop_table = &rptr->host_prop_table[i];
		/* update prop value */
		prop_table->vrt_sel = be32_to_cpu(*list++);
		prop_table->rx_sqth = be32_to_cpu(*list++);
		prop_table->discth = be32_to_cpu(*list++);
		prop_table->pre_emphasis = be32_to_cpu(*list++);
		prop_table->equalization = be32_to_cpu(*list++);
		prop_table->term_ofs = be32_to_cpu(*list++);
		prop_table->intr_ofs = be32_to_cpu(*list++);

		dev_info(dev, "prop table%d, host-vrt-vref:%d, host-rx-sqth:%d, host-discth:%d\n",
			i, prop_table->vrt_sel, prop_table->rx_sqth, prop_table->discth);
		dev_info(dev, "host-pre-emphasis:%d, host-eq:%d,  host-term-ofs:%d, host-intr-ofs:%d\n",
			prop_table->pre_emphasis, prop_table->equalization, prop_table->term_ofs,
			prop_table->intr_ofs);
	}
}

static void eusb2_rptr_prop_apply(struct eusb2_repeater *rptr, bool is_host, int index)
{
	dev_info(rptr->dev, "%s, index=%d, is_host=%d\n", __func__, index, is_host);

	if (!is_host) {
		if (rptr->device_prop_table && rptr->device_prop_num > index) {
			/* restore the default prop value */
			rptr->vrt_sel = rptr->device_prop_table[index].vrt_sel;
			rptr->rx_sqth = rptr->device_prop_table[index].rx_sqth;
			rptr->discth = rptr->device_prop_table[index].discth;
			rptr->pre_emphasis = rptr->device_prop_table[index].pre_emphasis;
			rptr->equalization = rptr->device_prop_table[index].equalization;
			rptr->term_ofs = rptr->device_prop_table[index].term_ofs;
			rptr->intr_ofs = rptr->device_prop_table[index].intr_ofs;
		}
	} else {
		if (rptr->host_prop_table && rptr->host_prop_num > index) {
			/* restore the default prop value */
			rptr->host_vrt_sel = rptr->host_prop_table[index].vrt_sel;
			rptr->host_rx_sqth = rptr->host_prop_table[index].rx_sqth;
			rptr->host_discth = rptr->host_prop_table[index].discth;
			rptr->host_pre_emphasis = rptr->host_prop_table[index].pre_emphasis;
			rptr->host_equalization = rptr->host_prop_table[index].equalization;
			rptr->host_term_ofs = rptr->host_prop_table[index].term_ofs;
			rptr->host_intr_ofs = rptr->host_prop_table[index].intr_ofs;
		}
	}
}

static enum phy_mode new_role_value = PHY_MODE_INVALID;
static enum phy_mode last_data_role = PHY_MODE_INVALID;
static struct class *eusb2_repeater_class = NULL;

static void eusb2_repeater_send_uevent(struct work_struct *work)
{
	struct eusb2_repeater *rptr = container_of(
		work, struct eusb2_repeater, dwork.work);
	char *envp[2];
	char state_str[32];

	rptr->repeater_enhance_state = 0;

	snprintf(state_str, sizeof(state_str), "USB_ENHANCE_STATE=%d", rptr->repeater_enhance_state);

	envp[0] = state_str;
	envp[1] = NULL;

	kobject_uevent_env(&rptr->dev->kobj, KOBJ_CHANGE, envp);
	pr_err("send event %s\n", state_str);
}

static void eusb2_repeater_send_uevent_function(struct eusb2_repeater *rptr)
{
	char *envp[2];
	char state_str[32];

	snprintf(state_str, sizeof(state_str), "USB_ENHANCE_STATE=%d", rptr->repeater_enhance_state);

	envp[0] = state_str;
	envp[1] = NULL;

	kobject_uevent_env(&rptr->dev->kobj, KOBJ_CHANGE, envp);
	pr_err("send event %s\n", state_str);
}

static void phy_check_role_event(enum phy_mode role,struct eusb2_repeater *rptr)
{
	new_role_value = role;
	pr_err("new_role %d  , last_role%d\n", new_role_value, last_data_role);

	if( rptr->repeater_enhance_state == TYPE_DEVICE_CARWITH){
		switch(last_data_role)
		{
			case PHY_MODE_USB_HOST:
			{
				switch(new_role_value)
				{
					case PHY_MODE_USB_HOST:
						break;
					case PHY_MODE_USB_DEVICE:
						break;
					case PHY_MODE_INVALID:
						break;
					default:
						pr_err("Hi MI PHY_MODE_USB_HOST "
							"Invalid value phy mode\n");
						break;
				}
				break;
			}
			case PHY_MODE_USB_DEVICE:
			{
				switch(new_role_value)
				{
					case PHY_MODE_USB_HOST:
						break;
					case PHY_MODE_USB_DEVICE:
						break;
					case PHY_MODE_INVALID:
						cancel_delayed_work(&rptr->dwork);
						udelay(100);
						schedule_delayed_work(&rptr->dwork,
							msecs_to_jiffies(300000));
						break;
					default:
						pr_err("Hi MI PHY_MODE_USB_DEVICE "
							"Invalid value phy mode\n");
						break;
				}
				break;
			}
			case PHY_MODE_INVALID:
			{
				switch(new_role_value)
				{
					case PHY_MODE_USB_HOST:
						cancel_delayed_work(&rptr->dwork);
						rptr->repeater_enhance_state = TYPE_HOST_NORMAL;
						eusb2_repeater_send_uevent_function(rptr);
						break;
					case PHY_MODE_USB_DEVICE:
						cancel_delayed_work(&rptr->dwork);
						break;
					case PHY_MODE_INVALID:
						break;
					default:
						pr_err("Hi MI PHY_MODE_INVALID "
							"Invalid value phy mode\n");
						break;
				}
				break;
			}
			default:
				pr_err("Hi MI Invalid value phy mode\n");
				break;
		}
	}
	last_data_role = new_role_value;
	pr_err("phy_check_role_event done!\n");
}

static ssize_t usb_enhance_support_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct eusb2_repeater *rptr = dev_get_drvdata(dev);

	if (!rptr)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%d\n", rptr->repeater_enhance_support_mode);
}
static DEVICE_ATTR_RO(usb_enhance_support_mode);

static ssize_t current_eye_tune_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct eusb2_repeater *rptr = dev_get_drvdata(dev);

	if (!rptr)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%d\n", rptr->repeater_current_eye_tune);
}
static DEVICE_ATTR_RO(current_eye_tune);

static ssize_t usb_enhance_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct eusb2_repeater *rptr = dev_get_drvdata(dev);

	if (!rptr)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%d\n", rptr->repeater_enhance_state);
}

static ssize_t usb_enhance_state_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct eusb2_repeater *rptr = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	if (!rptr)
		return -ENODEV;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	rptr->repeater_enhance_state = val;
	eusb2_repeater_send_uevent_function(rptr);

	if(rptr->repeater_enhance_state == TYPE_DEVICE_CARWITH)
	{
		cancel_delayed_work(&rptr->dwork);
		udelay(100);
		schedule_delayed_work(&rptr->dwork, msecs_to_jiffies(300000));
		dev_err(dev, "usb_ready_use updated to: %d\n", rptr->repeater_enhance_state);
	}

	return count;
}
static DEVICE_ATTR(usb_enhance_state, 0644, usb_enhance_state_show, usb_enhance_state_store);

static ssize_t usb_dynamic_adjustment_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct eusb2_repeater *rptr = dev_get_drvdata(dev);
	if (!rptr)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%d\n", rptr->repeater_dynamic_adjustment);
}

static ssize_t usb_dynamic_adjustment_store(struct device *dev,
		struct device_attribute *attr,const char *buf, size_t count)
{
	struct eusb2_repeater *rptr = dev_get_drvdata(dev);
	if (!rptr)
		return -ENODEV;
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
	    return ret;
	if(val== 1)
		rptr->repeater_dynamic_adjustment = 1;
	else
		rptr->repeater_dynamic_adjustment = 0;

	dev_err(dev, "usb_ready_use updated to: %d\n", rptr->repeater_dynamic_adjustment);
	return count;
}
static DEVICE_ATTR(usb_dynamic_adjustment, 0644,
		usb_dynamic_adjustment_show, usb_dynamic_adjustment_store);

static struct attribute *repeater_sysfs_attrs[] = {
	&dev_attr_usb_enhance_support_mode.attr,
	&dev_attr_current_eye_tune.attr,
	&dev_attr_usb_enhance_state.attr,
	&dev_attr_usb_dynamic_adjustment.attr,
	NULL,
};

static struct attribute_group repeater_sysfs_attr_group = {
	.name = NULL,
	.attrs = repeater_sysfs_attrs,
};

static int repeater_sysfs_init(struct eusb2_repeater *rptr, struct device *dev)
{
	int ret;
	struct device *my_dev;
	bool class_created_here = false;

	if (!eusb2_repeater_class) {
		eusb2_repeater_class = class_create(REPEATER_SYSFS_CLASS_NAME);
		if (IS_ERR(eusb2_repeater_class)) {
			dev_err(dev, "Failed to create class %s\n", REPEATER_SYSFS_CLASS_NAME);
			eusb2_repeater_class = NULL;
			return PTR_ERR(eusb2_repeater_class);
		}
		class_created_here = true;
	}

	my_dev = device_create(eusb2_repeater_class, dev, MKDEV(0, 0), rptr,
		REPEATER_SYSFS_DEVICE_NAME);

	if (IS_ERR(my_dev)) {
		dev_err(dev, "Failed to create sysfs device %s\n", REPEATER_SYSFS_DEVICE_NAME);
		ret = PTR_ERR(my_dev);
		goto device_error;
	}

	ret = sysfs_create_group(&my_dev->kobj, &repeater_sysfs_attr_group);
	if (ret) {
		dev_err(dev, "Failed to create sysfs attributes: %d\n", ret);
		goto group_error;
	}

	rptr->sysfs_dev = my_dev;
	dev_info(dev, "Sysfs nodes created for device %s\n", REPEATER_SYSFS_DEVICE_NAME);

	return 0;

group_error:
	device_destroy(eusb2_repeater_class, MKDEV(0, 0));

device_error:
	if (class_created_here) {
		class_destroy(eusb2_repeater_class);
		eusb2_repeater_class = NULL;
	}

	return ret;
}

static void repeater_sysfs_cleanup(struct eusb2_repeater *rptr,struct device *dev)
{
	pr_info("Removing sysfs nodes\n");
	if (rptr->sysfs_dev) {
		if (eusb2_repeater_class){
			sysfs_remove_group(&rptr->sysfs_dev->kobj, &repeater_sysfs_attr_group);
			device_destroy(eusb2_repeater_class, MKDEV(0, 0));
		}
		rptr->sysfs_dev = NULL;
	}


	if (eusb2_repeater_class)
		class_destroy(eusb2_repeater_class);

	pr_info("Sysfs nodes removed\n");
}

static int xsphy_more_eyes_read_overrides(struct device *dev,
	const char *prop, u32 **seq, u8 *seq_cnt)
{
	int num_elem, ret;

	num_elem = of_property_count_elems_of_size(dev->of_node, prop, sizeof(**seq));
	if(num_elem > 0){
		if (num_elem % 2)
		{
			dev_err(dev, "invalid len for %s\n", prop);
			return -EINVAL;
		}

		*seq_cnt = num_elem;
		*seq = devm_kcalloc(dev, num_elem, sizeof(**seq), GFP_KERNEL);
		if(!*seq)
			return -ENOMEM;

		ret = of_property_read_u32_array(dev->of_node, prop, *seq, num_elem);
		if(ret){
			dev_err(dev, "%s failed to read %d\n", prop, ret);
		}
	}

	return 0;
}

static int xsphy_more_eyes_u2_property(struct eusb2_repeater *rptr)
{
	struct device *dev = rptr->dev;
	int ret = 0;

	/*device normal & factroy*/
#ifdef CONFIG_FACTORY_BUILD
	ret = xsphy_more_eyes_read_overrides(dev,
			"mediatek,param-override-seq-factory",
			&rptr->param_override_seq,
			&rptr->param_override_seq_cnt);

	pr_err("Hi MI usb_can_usb += factory devices port0!\n");
	if(ret < 0)
		goto err_probe;

	if(rptr->repeater_not_single_port == 1){
		ret = xsphy_more_eyes_read_overrides(dev,
				"mediatek,param-override-seq-factory-port1",
				&rptr->param_override_seq_port1,
				&rptr->param_override_seq_cnt_port1);

		pr_err("Hi MI usb_can_usb += factory devices port1!\n");
		if(ret < 0)
			goto err_probe;
	}
#else
	if(rptr->repeater_global_enable == 1){
		ret = xsphy_more_eyes_read_overrides(dev,
				"mediatek,param-override-seq-global",
				&rptr->param_override_seq,
				&rptr->param_override_seq_cnt);

		pr_err("Hi MI usb_can_usb += global normal devices port0!\n");
		if(ret < 0)
			goto err_probe;

		/*device carwith*/
		if(rptr->repeater_carwith_enable == 1){
			ret = xsphy_more_eyes_read_overrides(dev,
				"mediatek,param-override-seq-global-carwith",
				&rptr->param_override_seq_carwith,
				&rptr->param_override_seq_cnt_carwith);

			rptr->repeater_enhance_support_mode |= TYPE_DEVICE_CARWITH;
			pr_err("HI MI usb_can_use += global carwith port0!\n");
			if(ret < 0)
				goto err_probe;

			if(rptr->repeater_not_single_port == 1){
				ret = xsphy_more_eyes_read_overrides(dev,
					"mediatek,param-override-seq-global-carwith-port1",
					&rptr->param_override_seq_carwith_port1,
					&rptr->param_override_seq_cnt_carwith_port1);

				rptr->repeater_enhance_support_mode |= TYPE_DEVICE_CARWITH;
				pr_err("Hi MI usb_can_usb += global carwith port1!\n");
				if(ret < 0)
					goto err_probe;
			}
		}

		if(rptr->repeater_not_single_port == 1){
			ret = xsphy_more_eyes_read_overrides(dev,
				"mediatek,param-override-seq-global-port1",
				&rptr->param_override_seq_port1,
				&rptr->param_override_seq_cnt_port1);

			pr_err("Hi MI usb_can_usb += global normal devices  port1!\n");
			if(ret < 0)
				goto err_probe;
		}
	} else {
		ret = xsphy_more_eyes_read_overrides(dev,
				"mediatek,param-override-seq",
				&rptr->param_override_seq,
				&rptr->param_override_seq_cnt);

		pr_err("Hi MI usb_can_usb += normal devices port0!\n");
		if(ret < 0)
			goto err_probe;

		/*device carwith*/
		if(rptr->repeater_carwith_enable == 1){
			ret = xsphy_more_eyes_read_overrides(dev,
					"mediatek,param-override-seq-carwith",
					&rptr->param_override_seq_carwith,
					&rptr->param_override_seq_cnt_carwith);

			rptr->repeater_enhance_support_mode |= TYPE_DEVICE_CARWITH;
			pr_err("HI MI usb_can_use += carwith port0!\n");
			if(ret < 0)
				goto err_probe;

			if(rptr->repeater_not_single_port == 1){
				ret = xsphy_more_eyes_read_overrides(dev,
						"mediatek,param-override-seq-carwith-port1",
						&rptr->param_override_seq_carwith_port1,
						&rptr->param_override_seq_cnt_carwith_port1);

				rptr->repeater_enhance_support_mode |= TYPE_DEVICE_CARWITH;
				pr_err("Hi MI usb_can_usb += carwith port1!\n");
				if(ret < 0)
					goto err_probe;
			}
		}

		if(rptr->repeater_not_single_port == 1){
			ret = xsphy_more_eyes_read_overrides(dev,
					"mediatek,param-override-seq-port1",
					&rptr->param_override_seq_port1,
					&rptr->param_override_seq_cnt_port1);

			pr_err("Hi MI usb_can_usb += normal devices port1!\n");
			if(ret < 0)
				goto err_probe;
		}
	}
#endif

    /*host normal & factroy*/
#ifdef CONFIG_FACTORY_BUILD
	ret = xsphy_more_eyes_read_overrides(dev,
			"mediatek,host-param-override-seq-factory",
			&rptr->host_param_override_seq,
			&rptr->host_param_override_seq_cnt);

	pr_err("HI MI usb_can_use += factory host port0!\n");
	if(ret < 0)
		goto err_probe;

	if(rptr->repeater_not_single_port == 1){
		ret = xsphy_more_eyes_read_overrides(dev,
				"mediatek,host-param-override-seq-factory-port1",
				&rptr->host_param_override_seq_port1,
				&rptr->host_param_override_seq_cnt_port1);

		pr_err("HI MI usb_can_use += factory host port1!\n");
		if(ret < 0)
			goto err_probe;
	}
#else
	if(rptr->repeater_global_enable == 1){
		ret = xsphy_more_eyes_read_overrides(dev,
				"mediatek,host-param-override-seq-global",
				&rptr->host_param_override_seq,
				&rptr->host_param_override_seq_cnt);

		pr_err("HI MI usb_can_use += global normal host port0!\n");
		if(ret < 0)
			goto err_probe;

		/*host storage*/
		if(rptr->repeater_storage_enable == 1){
			ret = xsphy_more_eyes_read_overrides(dev,
					"mediatek,host-param-override-seq-global-storage",
					&rptr->host_param_override_seq_storage,
					&rptr->host_param_override_seq_cnt_storage);

			rptr->repeater_enhance_support_mode |= TYPE_HOST_STORAGE;
			pr_err("HI MI usb_can_use += global storage port0!\n");
			if(ret < 0)
				goto err_probe;

			if(rptr->repeater_not_single_port == 1){
				ret = xsphy_more_eyes_read_overrides(dev,
						"mediatek,host-param-override-seq-global-storage-port1",
						&rptr->host_param_override_seq_storage_port1,
						&rptr->host_param_override_seq_cnt_storage_port1);

				rptr->repeater_enhance_support_mode |= TYPE_HOST_STORAGE;
				pr_err("HI MI usb_can_use += global storage port1!\n");
				if(ret < 0)
					goto err_probe;
			}
		}

		if(rptr->repeater_not_single_port == 1){
			ret = xsphy_more_eyes_read_overrides(dev,
					"mediatek,host-param-override-seq-global-port1",
					&rptr->host_param_override_seq_port1,
					&rptr->host_param_override_seq_cnt_port1);

			pr_err("HI MI usb_can_use += global normal host port1!\n");
			if(ret < 0)
				goto err_probe;
		}
	} else {
		ret = xsphy_more_eyes_read_overrides(dev,
				"mediatek,host-param-override-seq",
				&rptr->host_param_override_seq,
				&rptr->host_param_override_seq_cnt);

		pr_err("HI MI usb_can_use += normal host port0!\n");
		if(ret < 0)
			goto err_probe;

		/*host storage*/
		if(rptr->repeater_storage_enable == 1){
			ret = xsphy_more_eyes_read_overrides(dev,
					"mediatek,host-param-override-seq-storage",
					&rptr->host_param_override_seq_storage,
					&rptr->host_param_override_seq_cnt_storage);

			rptr->repeater_enhance_support_mode |= TYPE_HOST_STORAGE;
			pr_err("HI MI usb_can_use += storage port0!\n");
			if(ret < 0)
				goto err_probe;

			if(rptr->repeater_not_single_port == 1){
				ret = xsphy_more_eyes_read_overrides(dev,
						"mediatek,host-param-override-seq-storage-port1",
						&rptr->host_param_override_seq_storage_port1,
						&rptr->host_param_override_seq_cnt_storage_port1);

				rptr->repeater_enhance_support_mode |= TYPE_HOST_STORAGE;
				pr_err("HI MI usb_can_use += storage port1!\n");
				if(ret < 0)
					goto err_probe;
			}
		}

		if(rptr->repeater_not_single_port == 1){
			ret = xsphy_more_eyes_read_overrides(dev,
					"mediatek,host-param-override-seq-port1",
					&rptr->host_param_override_seq_port1,
					&rptr->host_param_override_seq_cnt_port1);

			pr_err("HI MI usb_can_use += normal host port1!\n");
			if(ret < 0)
				goto err_probe;
		}
	}
#endif

err_probe:
	return ret;
}

static void eusb2_rptr_prop_parse(struct eusb2_repeater *rptr)
{
	struct device *dev = rptr->dev;
	const char *ofs_str;
	u32 val = 0;

	/* Common */
	if (device_property_read_u32(dev, "mediatek,usb20-fs-sr",
				&rptr->usb20_fs_sr) || rptr->usb20_fs_sr < 0)
		rptr->usb20_fs_sr =-EINVAL;

	if (device_property_read_u32(dev, "mediatek,eusb20-fsrx-hys-sel",
		&rptr->eusb20_fsrx_hys_sel) || rptr->eusb20_fsrx_hys_sel < 0)
		rptr->eusb20_fsrx_hys_sel =-EINVAL;

	if(rptr->repeater_product_enable){
		xsphy_more_eyes_u2_property(rptr);
	} else {
		/* Device */
		if (device_property_read_u32(dev, "mediatek,vrt-sel",
				&rptr->vrt_sel) || rptr->vrt_sel < 0)
			rptr->vrt_sel =-EINVAL;

		if (device_property_read_u32(dev, "mediatek,rx-sqth",
				&rptr->rx_sqth) || rptr->rx_sqth < 0)
			rptr->rx_sqth = -EINVAL;

		if (device_property_read_u32(dev, "mediatek,discth",
				&rptr->discth) || rptr->discth < 0)
			rptr->discth = -EINVAL;

		if (device_property_read_u32(dev, "mediatek,pre-emphasis",
				&rptr->pre_emphasis) || rptr->pre_emphasis < 0)
			rptr->pre_emphasis = -EINVAL;

		if (device_property_read_u32(dev, "mediatek,equalization",
				&rptr->equalization) || rptr->equalization < 0)
			rptr->equalization = -EINVAL;

		dev_info(dev, "vrt-vref:%d, rx_sqth:%d, discth:%d, pre-emphasis:%d, eq:%d",
			rptr->vrt_sel, rptr->rx_sqth, rptr->discth, rptr->pre_emphasis,
			rptr->equalization);
		/* Host */
		if (device_property_read_u32(dev, "mediatek,host-vrt-sel",
				&rptr->host_vrt_sel) || rptr->host_vrt_sel < 0)
			rptr->host_vrt_sel =-EINVAL;

		if (device_property_read_u32(dev, "mediatek,host-rx-sqth",
				&rptr->host_rx_sqth) || rptr->host_rx_sqth < 0)
			rptr->host_rx_sqth = -EINVAL;

		if (device_property_read_u32(dev, "mediatek,host-discth",
				&rptr->host_discth) || rptr->host_discth < 0)
			rptr->host_discth = -EINVAL;

		if (device_property_read_u32(dev, "mediatek,host-pre-emphasis",
				&rptr->host_pre_emphasis) || rptr->host_pre_emphasis < 0)
			rptr->host_pre_emphasis = -EINVAL;

		if (device_property_read_u32(dev, "mediatek,host-equalization",
				&rptr->host_equalization) || rptr->host_equalization < 0)
			rptr->host_equalization = -EINVAL;

		dev_info(dev, "host-vrt-vref:%d, host-rx-sqth:%d, host-discth:%d,"
			"host-pre-emphasis:%d, host-eq:%d",
			rptr->host_vrt_sel, rptr->host_rx_sqth, rptr->host_discth,
			rptr->host_pre_emphasis, rptr->host_equalization);
		/* HW efuse, SW mode */
		if (device_property_read_string(dev, "mediatek,intr-ofs",
				&ofs_str) || kstrtoint(ofs_str, 10, &rptr->intr_ofs) < 0)
			rptr->intr_ofs = -(RG_USB20_INTR_CAL_MASK + 1);

		if (device_property_read_string(dev, "mediatek,term-ofs",
				&ofs_str) || kstrtoint(ofs_str, 10, &rptr->term_ofs) < 0)
			rptr->term_ofs = -(RG_USB20_TERM_CAL_MASK + 1);

		if (device_property_read_string(dev, "mediatek,host-intr-ofs",
				&ofs_str) || kstrtoint(ofs_str, 10, &rptr->host_intr_ofs) < 0)
			rptr->host_intr_ofs = -(RG_USB20_INTR_CAL_MASK + 1);

		if (device_property_read_string(dev, "mediatek,host-term-ofs",
				&ofs_str) || kstrtoint(ofs_str, 10, &rptr->host_term_ofs) < 0)
			rptr->host_term_ofs = -(RG_USB20_TERM_CAL_MASK + 1);
	}

	/* Read efuse RG to set this default value */
	regmap_read(rptr->regmap, rptr->base + PHYA_U2_CR0_1, &val);
	rptr->intr_cal = val;
	regmap_read(rptr->regmap, rptr->base + PHYA_U2_CR2_2, &val);
	rptr->term_cal = val;

	dev_info(dev, "intr-cal:%d, term-cal:%d",
		rptr->intr_cal, rptr->term_cal);
	dev_info(dev, "intr-cal-ofs:%d, term-cal-ofs:%d, host-intr-cal-ofs:%d, host-term-cal-ofs:%d",
		rptr->intr_ofs, rptr->term_ofs,
		rptr->host_intr_ofs, rptr->host_term_ofs);

}

/*Load parameter configuration and handle missing and misplaced values*/
static void xsphy_u2_update_vaule_seq(struct eusb2_repeater *rptr,
	u32 *seq, u8 cnt)
{
	int i=0;
	if(seq == NULL)
		return;

	if(rptr->repeater_dynamic_adjustment == 1)
		return;

	rptr->discth = -EINVAL;
	rptr->equalization = -EINVAL;
	rptr->vrt_sel = -EINVAL;
	rptr->pre_emphasis = -EINVAL;
	rptr->rx_sqth = -EINVAL;
	rptr->term_ofs = -EINVAL;
	rptr->intr_ofs = -EINVAL;

	while(i<cnt/2){
		switch(seq[i*2]){
			case 0x1:
				rptr->discth = seq[i*2+1] == 0xff ? -EINVAL : seq[i*2+1];
				break;
			case 0x2:
				rptr->equalization = seq[i*2+1] == 0xff ? -EINVAL : seq[i*2+1];
				break;
			case 0x3:
				rptr->vrt_sel = seq[i*2+1] == 0xff ? -EINVAL : seq[i*2+1];
				break;
			case 0x4:
				rptr->pre_emphasis = seq[i*2+1] == 0xff ? -EINVAL : seq[i*2+1];
				break;
			case 0x5:
				rptr->rx_sqth = seq[i*2+1] == 0xff ? -EINVAL : seq[i*2+1];
				break;
			case 0x6:
				rptr->term_ofs = seq[i*2+1] == 0xff ? -EINVAL : seq[i*2+1];
				break;
			case 0x7:
				rptr->intr_ofs = seq[i*2+1] == 0xff ? -EINVAL : seq[i*2+1];
				break;
		}
		i+=1;
	}

}

static void xsphy_u2_update_vaule_host_seq(struct eusb2_repeater *rptr,
	u32 *seq, u8 cnt)
{
	int i=0;
	if(seq == NULL)
		return;

	if(rptr->repeater_dynamic_adjustment == 1)
		return;

	rptr->host_discth = -EINVAL;
	rptr->host_equalization = -EINVAL;
	rptr->host_vrt_sel = -EINVAL;
	rptr->host_pre_emphasis = -EINVAL;
	rptr->host_rx_sqth = -EINVAL;
	rptr->host_term_ofs = -EINVAL;
	rptr->host_intr_ofs = -EINVAL;

	while(i<cnt/2){
		switch(seq[i*2]){
			case 0x1:
				rptr->host_discth = seq[i*2+1]== 0xff ? -EINVAL : seq[i*2+1];
				break;
			case 0x2:
				rptr->host_equalization = seq[i*2+1] == 0xff ? -EINVAL : seq[i*2+1];
				break;
			case 0x3:
				rptr->host_vrt_sel = seq[i*2+1] == 0xff ? -EINVAL : seq[i*2+1];
				break;
			case 0x4:
				rptr->host_pre_emphasis = seq[i*2+1] == 0xff ? -EINVAL : seq[i*2+1];
				break;
			case 0x5:
				rptr->host_rx_sqth = seq[i*2+1] == 0xff ? -EINVAL : seq[i*2+1];
				break;
			case 0x6:
				rptr->host_term_ofs = seq[i*2+1] == 0xff ? -EINVAL : seq[i*2+1];
				break;
			case 0x7:
				rptr->host_intr_ofs = seq[i*2+1] == 0xff ? -EINVAL : seq[i*2+1];
				break;
		}
		i+=1;
	}

}

static void xsphy_u2_device_update_seq(struct eusb2_repeater *rptr)
{
	int final_use = 0;
	final_use = rptr->repeater_enhance_state;
	rptr->repeater_which_port = usb_port_use();
	pr_err("HI MI test log rptr->repeater_which_port = %d\n",rptr->repeater_which_port);

	switch(final_use){
		case TYPE_DEVICE_NORMAL:
			if(rptr->repeater_not_single_port == 1 &&
				rptr->repeater_which_port == 1) {
				xsphy_u2_update_vaule_seq(rptr,
					rptr->param_override_seq_port1,
					rptr->param_override_seq_cnt_port1);
				pr_err("HI MI init device port1!\n");
			} else {
				xsphy_u2_update_vaule_seq(rptr,rptr->param_override_seq,
					rptr->param_override_seq_cnt);
				pr_err("HI MI init device port0!\n");
			}
			rptr->repeater_current_eye_tune = TYPE_DEVICE_NORMAL;
			break;
		case TYPE_DEVICE_CARWITH:
			if(rptr->repeater_not_single_port == 1 &&
				rptr->repeater_which_port == 1) {
				xsphy_u2_update_vaule_seq(rptr,
					rptr->param_override_seq_carwith_port1,
					rptr->param_override_seq_cnt_carwith_port1);
				pr_err("HI MI init carwith device port1!\n");
			} else {
				xsphy_u2_update_vaule_seq(rptr,
					rptr->param_override_seq_carwith,
					rptr->param_override_seq_cnt_carwith);
				pr_err("HI MI init carwith device port0!\n");
			}
			rptr->repeater_current_eye_tune = TYPE_DEVICE_CARWITH;
			break;
		default:
			if(rptr->repeater_not_single_port == 1 &&
				rptr->repeater_which_port == 1) {
				xsphy_u2_update_vaule_seq(rptr,
					rptr->param_override_seq_port1,
					rptr->param_override_seq_cnt_port1);
				pr_err("HI MI init device port1!\n");
			} else {
				xsphy_u2_update_vaule_seq(rptr,rptr->param_override_seq,
					rptr->param_override_seq_cnt);
				pr_err("HI MI init device port0!\n");
			}
			rptr->repeater_current_eye_tune = 0;
			break;
    }

}

static void xsphy_u2_host_update_seq(struct eusb2_repeater *rptr)
{
	int final_use = 0;
	final_use = rptr->repeater_enhance_state;
	rptr->repeater_which_port = usb_port_use();
	pr_err("HI MI test log rptr->repeater_which_port = %d\n",rptr->repeater_which_port);

	switch(final_use){
		case TYPE_HOST_STORAGE:
			if(	rptr->repeater_storage_enable == 1 &&
				rptr->repeater_which_port == 1) {
				xsphy_u2_update_vaule_host_seq(rptr,
					rptr->host_param_override_seq_storage_port1,
					rptr->host_param_override_seq_cnt_storage_port1);
				pr_err("HI MI init storage host port1!\n");
			} else {
				xsphy_u2_update_vaule_host_seq(rptr,
					rptr->host_param_override_seq_storage,
					rptr->host_param_override_seq_cnt_storage);
				pr_err("HI MI init storage host port0!\n");
			}
			rptr->repeater_current_eye_tune = TYPE_HOST_STORAGE;
			break;
		case TYPE_HOST_NORMAL:
			if(rptr->repeater_not_single_port == 1 &&
				rptr->repeater_which_port == 1) {
				xsphy_u2_update_vaule_host_seq(rptr,
					rptr->host_param_override_seq_port1,
					rptr->host_param_override_seq_cnt_port1);
				pr_err("HI MI init host port1!\n");
			} else{
				xsphy_u2_update_vaule_host_seq(rptr,
					rptr->host_param_override_seq,
					rptr->host_param_override_seq_cnt);
				pr_err("HI MI init host port0!\n");
			}
			rptr->repeater_current_eye_tune = TYPE_HOST_NORMAL;
			break;
		default:
			if(rptr->repeater_not_single_port == 1 &&
				rptr->repeater_which_port == 1) {
				xsphy_u2_update_vaule_host_seq(rptr,
					rptr->host_param_override_seq_port1,
					rptr->host_param_override_seq_cnt_port1);
				pr_err("HI MI init host port1!\n");
			} else{
				xsphy_u2_update_vaule_host_seq(rptr,
					rptr->host_param_override_seq,
					rptr->host_param_override_seq_cnt);
				pr_err("HI MI init host port0!\n");
			}
			rptr->repeater_current_eye_tune = 0;
			break;
	}
}

static void eusb2_device_prop_set(struct eusb2_repeater *rptr)
{

	if(rptr->repeater_product_enable)
	{
		xsphy_u2_device_update_seq(rptr);
	}
	if (rptr->vrt_sel != -EINVAL)
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_0, RG_USB20_VRT_SEL,
			rptr->vrt_sel << RG_USB20_VRT_SEL_SHIFT);

	if (rptr->rx_sqth != -EINVAL)
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_0, RG_USB20_SQTH,
			rptr->rx_sqth << RG_USB20_SQTH_SHIFT);

	if (rptr->discth != -EINVAL)
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_0, RG_USB20_DISCTH,
			rptr->discth << RG_USB20_DISCTH_SHIFT);

	if (rptr->pre_emphasis != -EINVAL)
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_3, RG_USB20_HS_PE,
			rptr->pre_emphasis << RG_USB20_HS_PE_SHIFT);

	if (rptr->equalization != -EINVAL)
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_3, RG_USB20_HS_EQ,
			rptr->equalization << RG_USB20_HS_EQ_SHIFT);

	/* HW efuse, sw mode */
	if (rptr->intr_cal != -EINVAL) {
		int intr_cal_val = rptr->intr_cal + rptr->intr_ofs;

		if (rptr->intr_ofs < -RG_USB20_INTR_CAL_MASK ||
				rptr->intr_ofs > RG_USB20_INTR_CAL_MASK ||
				intr_cal_val < 0 || intr_cal_val > RG_USB20_INTR_CAL_MASK)
			intr_cal_val = rptr->intr_cal;

		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_1, RG_USB20_INTR_CAL,
			intr_cal_val << RG_USB20_INTR_CAL_SHIFT);
	}

	if (rptr->term_cal != -EINVAL) {
		int term_cal_val = rptr->term_cal + rptr->term_ofs;

		if (rptr->term_ofs < -RG_USB20_TERM_CAL_MASK ||
				rptr->term_ofs > RG_USB20_TERM_CAL_MASK ||
				term_cal_val < 0 || term_cal_val > RG_USB20_TERM_CAL_MASK)
			term_cal_val = rptr->term_cal;

		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_2, RG_USB20_TERM_CAL,
			term_cal_val << RG_USB20_TERM_CAL_SHIFT);
	}

}

static void eusb2_host_prop_set(struct eusb2_repeater *rptr)
{

	if(rptr->repeater_product_enable)
	{
		xsphy_u2_host_update_seq(rptr);
	}
	if (rptr->host_vrt_sel != -EINVAL)
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_0, RG_USB20_VRT_SEL,
			rptr->host_vrt_sel << RG_USB20_VRT_SEL_SHIFT);

	if (rptr->host_rx_sqth != -EINVAL)
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_0, RG_USB20_SQTH,
			rptr->host_rx_sqth << RG_USB20_SQTH_SHIFT);

	if (rptr->host_discth != -EINVAL)
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_0, RG_USB20_DISCTH,
			rptr->host_discth << RG_USB20_DISCTH_SHIFT);

	if (rptr->host_pre_emphasis != -EINVAL)
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_3, RG_USB20_HS_PE,
			rptr->host_pre_emphasis << RG_USB20_HS_PE_SHIFT);

	if (rptr->host_equalization != -EINVAL)
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_3, RG_USB20_HS_EQ,
			rptr->host_equalization << RG_USB20_HS_EQ_SHIFT);

	/* HW efuse, sw mode */
	if (rptr->intr_cal != -EINVAL) {
		int host_intr_cal_val = rptr->intr_cal + rptr->host_intr_ofs;

		if (rptr->host_intr_ofs < -RG_USB20_INTR_CAL_MASK ||
				rptr->host_intr_ofs > RG_USB20_INTR_CAL_MASK ||
				host_intr_cal_val < 0 || host_intr_cal_val > RG_USB20_INTR_CAL_MASK)
			host_intr_cal_val = rptr->intr_cal;

		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_1, RG_USB20_INTR_CAL,
			host_intr_cal_val << RG_USB20_INTR_CAL_SHIFT);
	}

	if (rptr->term_cal != -EINVAL) {
		int host_term_cal_val = rptr->term_cal + rptr->host_term_ofs;

		if (rptr->host_term_ofs < -RG_USB20_TERM_CAL_MASK ||
				rptr->host_term_ofs > RG_USB20_TERM_CAL_MASK ||
				host_term_cal_val < 0 || host_term_cal_val > RG_USB20_TERM_CAL_MASK)
			host_term_cal_val = rptr->term_cal;

		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_2, RG_USB20_TERM_CAL,
			host_term_cal_val << RG_USB20_TERM_CAL_SHIFT);
	}

}

static void eusb2_efuse_prop_set(struct eusb2_repeater *rptr)
{
	u32 value1 = 0;
	u32 value2 = 0;
	u32 value3 = 0;
	u32 osc = 0;
	u32 fs_cr = 0;
	u32 ls_cr = 0;

	/* eUSB2 efuse */
	regmap_read(rptr->regmap, EUSB_RSV_81, &value1);
	regmap_read(rptr->regmap, EUSB_RSV_82, &value2);

	if (!(value1 | value2))
		goto hw_efuse_sw_mode;

	/* 0x81 */
	regmap_update_bits(rptr->regmap, rptr->base + PHA_EU2_CR0_2, (value1 & 0x40) >> 6, (value1 & 0x40) >> 6);
	regmap_update_bits(rptr->regmap, rptr->base + PHA_EU2_CR0_2, (value1 & 0x20) >> 2, (value1 & 0x20) >> 2);
	regmap_update_bits(rptr->regmap, rptr->base + PHA_EU2_CR0_2, (value1 & 0x10) >> 2, (value1 & 0x10) >> 2);
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_3, (value1 & 0x8) << 3, (value1 & 0x8) << 3);
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_3, (value1 & 0x4) << 3, (value1 & 0x4) << 3);
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_3, (value1 & 0x2) << 3, (value1 & 0x2) << 3);
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_3, (value1 & 0x1) << 3, (value1 & 0x1) << 3);

	/* 0x82 */
	regmap_update_bits(rptr->regmap, rptr->base + PHA_EU2_CR0_1, (value2 & 0x10) << 3, (value2 & 0x10) << 3);
	regmap_update_bits(rptr->regmap, rptr->base + PHA_EU2_CR0_2, (value2 & 0x8) << 4, (value2 & 0x8) << 4);
	regmap_update_bits(rptr->regmap, rptr->base + PHA_EU2_CR0_2, (value2 & 0x4) << 4, (value2 & 0x4) << 4);
	regmap_update_bits(rptr->regmap, rptr->base + PHA_EU2_CR0_2, (value2 & 0x2) << 4, (value2 & 0x2) << 4);
	regmap_update_bits(rptr->regmap, rptr->base + PHA_EU2_CR0_2, (value2 & 0x1) << 4, (value2 & 0x1) << 4);

hw_efuse_sw_mode:

	/* HW efuse, SW mode sel */
	regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_1, RG_INIT_SW_SEL, RG_INIT_SW_SEL);

	/* EFUSE_USB20_INTR_CAL */
	regmap_read(rptr->regmap, EUSB_RSV_23, &value3);
	if (value3 & RG_USB20_INTR_CAL_TOP_MASK) {
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_1, RG_USB20_INTR_CAL,
				(value3 & RG_USB20_INTR_CAL_TOP_MASK) << RG_USB20_INTR_CAL_SHIFT);
		rptr->intr_cal = value3 & RG_USB20_INTR_CAL_TOP_MASK;
	}

	/* EFUSE_USB20_TERM_CAL */
	regmap_read(rptr->regmap, EUSB_RSV_24, &value3);
	if (value3 & RG_USB20_TERM_CAL_TOP_MASK) {
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_2, RG_USB20_TERM_CAL,
				(value3 & RG_USB20_TERM_CAL_TOP_MASK) << RG_USB20_TERM_CAL_SHIFT);
		rptr->term_cal = value3 & RG_USB20_TERM_CAL_TOP_MASK;
	}

	/* EFUSE_EUSB20_HSTX_IMPN */
	regmap_read(rptr->regmap, EUSB_RSV_25, &value3);
	if (value3 & RG_EUSB20_HSTX_IMPN_TOP_MASK)
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_EU2_CR2_1, RG_EUSB20_HSTX_IMPN,
				(value3 & RG_EUSB20_HSTX_IMPN_TOP_MASK) >> RG_EUSB20_HSTX_IMPN_TOP_SFT);

	/* EFUSE_EUSB20_HSTX_IMPP */
	regmap_read(rptr->regmap, EUSB_RSV_26, &value3);
	if (value3 & RG_EUSB20_HSTX_IMPP_TOP_MASK)
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_EU2_CR2_2, RG_EUSB20_HSTX_IMPP,
				(value3 & RG_EUSB20_HSTX_IMPP_TOP_MASK) >> RG_EUSB20_HSTX_IMPP_TOP_SFT);

	/* EFUSE_EUSB20_HSRX_TERM_CAL */
	regmap_read(rptr->regmap, EUSB_RSV_24, &value3);
	if (value3 & RG_EUSB20_HSRX_TERM_CAL_TOP_MASK)
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_EU2_CR2_0, RG_EUSB20_HSRX_TERM_CAL,
				(value3 & RG_EUSB20_HSRX_TERM_CAL_TOP_MASK));


	/* since osc, fs_cr, ls_cr could be efuse 0 */
	regmap_read(rptr->regmap, EUSB_RSV_27, &osc);
	regmap_read(rptr->regmap, EUSB_RSV_25, &fs_cr);
	regmap_read(rptr->regmap, EUSB_RSV_26, &ls_cr);

	osc = (osc & RG_USB20_OSC_CALI_TOP_MASK);
	fs_cr = (fs_cr & RG_EUSB20_FS_CR_TOP_MASK);
	ls_cr = (ls_cr & RG_EUSB20_LS_CR_TOP_MASK);

	if (!(osc | fs_cr | ls_cr))
		return;

	/* EFUSE_USB20_OSC_CALI */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_EXT_CR0_0, RG_USB20_OSC_CALI,
			osc << RG_USB20_OSC_CALI_SHIFT);

	/* EFUSE_EUSB20_FS_CR */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_EU2_CR1_0, RG_EUSB20_FS_CR,
			fs_cr << RG_EUSB20_FS_CR_SHIFT);

	/* EFUSE_EUSB20_LS_CR */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_EU2_CR1_0, RG_EUSB20_LS_CR,
			ls_cr << RG_EUSB20_LS_CR_SHIFT);

}

static int eusb2_repeater_init(struct phy *phy)
{
	struct eusb2_repeater *rptr = phy_get_drvdata(phy);
	int ret = 0;

	queue_work(rptr->wq, &rptr->procfs_work);

	dev_info(rptr->dev, "MTK MT6379 eusb2 repeater init done\n");

	return ret;
}

static int eusb2_repeater_exit(struct phy *phy)
{
	struct eusb2_repeater *rptr = phy_get_drvdata(phy);

	dev_info(rptr->dev, "MTK MT6379 eusb2 repeater exit done\n");

	return 0;
}

static void eusb2_default_prop_set(struct eusb2_repeater *rptr)
{
	/* The default value of HS disconnect threshold is too high, adjust HS DISC TH */
	/* (default 0x0D18 [7:4]= 4b'1110 to 0x0D18 [7:4]= 4b'0110) */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_0,
			RG_USB20_DISCTH, 0x6 << RG_USB20_DISCTH_SHIFT);

	/* RG_USB20_FS_SR */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR1_0, RG_USB20_FS_SR,
			rptr->usb20_fs_sr << RG_USB20_FS_SR_SHIFT);

	/* RG_EUSB20_FSRX_HYS_SEL */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_EU2_CR1_3, RG_EUSB20_FSRX_HYS_SEL,
			rptr->eusb20_fsrx_hys_sel << RG_EUSB20_FSRX_HYS_SEL_SHIFT);

	/* E3 patch */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_EU2_CR0_0,
			RG_EUSB20_TXLDO_VREF_SEL, RG_EUSB20_TXLDO_VREF_SEL);
	regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_2,
			RG_RX_DBC_CNT, 0x3 << RG_RX_DBC_CNT_SHIFT);

}

static void eusb2_default_e2_prop_set(struct eusb2_repeater *rptr)
{
	/* The default value of HS disconnect threshold is too high, adjust HS DISC TH */
	/* (default 0x0D18 [7:4]= 4b'1110 to 0x0D18 [7:4]= 4b'0110) */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_0,
		RG_USB20_DISCTH, 0x6 << RG_USB20_DISCTH_SHIFT);

	/* FS enumeration fail, change the decoupling cap value in bias circuit */
	/* (default 0x0D18 [10:9]= 2b'10 to [10:9]= 2b'00 */
	/* 0x0DA4 [15:12]= 4b'0000 to 0x0DA4 [15:12]= 4b'0001) */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_1, RG_USB20_HSRX_BIAS_EN_SEL, 0);
	regmap_update_bits(rptr->regmap, rptr->base + PHYD_FM_CR0_1, RG_FM_DUMMY, RG_FM_DUMMY);


	/* RG_USB20_FS_SR */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR1_0, RG_USB20_FS_SR,
		rptr->usb20_fs_sr << RG_USB20_FS_SR_SHIFT);

	/* RG_EUSB20_FSRX_HYS_SEL */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_EU2_CR1_3, RG_EUSB20_FSRX_HYS_SEL,
		rptr->eusb20_fsrx_hys_sel << RG_EUSB20_FSRX_HYS_SEL_SHIFT);
}

static int eusb2_repeater_power_on(struct phy *phy)
{
	struct eusb2_repeater *rptr = phy_get_drvdata(phy);
	unsigned int chip_rev = 0;

	regmap_read(rptr->regmap, MT6379_REG_DEV_INFO, &chip_rev);
	dev_info(rptr->dev, "eusb2 repeater power on chip_rev(%x) submode(%x)\n", chip_rev, rptr->submode);

	/* set SQD to deglitch 1x */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR1_3, RG_USB20_SQD_MASK, 0x0);

	if ((chip_rev & MT6379_CHIP_REV_MASK) <= 0x2) {
		regmap_update_bits(rptr->regmap, rptr->base + 0x92, 0x7, 0x7);

		/* off */
		/* RG_USB_EN_SRC_SEL = 0x1 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_3,
			RG_USB_EN_SRC_SEL, RG_USB_EN_SRC_SEL);

		/* RG_USB20_BC11_SW_EN = 0x1 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3,
			RG_USB20_BC11_SW_EN, RG_USB20_BC11_SW_EN);

		/* RG_USB20_BGR_EN = 0x0 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_0, RG_USB20_BGR_EN, 0);

		/* RG_USB20_ROSC_EN = 0x0 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3, RG_USB20_ROSC_EN, 0);

		/* RG_USB20_REV_A[5] = 0x0 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_2, RG_USB20_REV_A_5, 0);

		udelay(300);

		/* Apply sw/hw efuse tuning */
		eusb2_efuse_prop_set(rptr);

		/* modify default value */
		eusb2_default_e2_prop_set(rptr);

		/* Modify OSC_BIAS  OSC_BIAS<2:1>, LS_CR<0> */
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_EXT_CR1_1, 0x6, 0x6);

		regmap_update_bits(rptr->regmap, rptr->base + PHYA_EU2_CR1_0, RG_USB20_OSC_BIAS_0,
				RG_USB20_OSC_BIAS_0);

		/* Set OSC_IBAND to 0x52 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_EXT_CR0_1, 0xFF, 0x52);

		/* on */
		/* RG_USB20_REV_A[5] = 0x1 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_2,
			RG_USB20_REV_A_5, RG_USB20_REV_A_5);

		/* RG_INIT_SW_PHY_SEL = 0x1 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_1,
			RG_INIT_SW_PHY_SEL, RG_INIT_SW_PHY_SEL);

		/* RG_USB_EN_SRC_SEL = 0x0 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_3, RG_USB_EN_SRC_SEL, 0);

		/* RG_USB20_BGR_EN = 0x1 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_0, RG_USB20_BGR_EN, RG_USB20_BGR_EN);

		udelay(100);

		/* RG_USB20_BC11_SW_EN = 0x0 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3, RG_USB20_BC11_SW_EN, 0);

		udelay(50);

		/* RG_USB20_OSC_BUF[0] = 0x1 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_EXT_CR1_0,
			RH_USB20_OSC_BUF_0, RH_USB20_OSC_BUF_0);

		/* RG_USB20_ROSC_EN = 0x1 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3,
			RG_USB20_ROSC_EN, RG_USB20_ROSC_EN);

		udelay(10);

		/* RG_USB20_ROSC_EN = 0x0 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3,
			RG_USB20_ROSC_EN, 0);

		udelay(30);

		/* RG_USB20_OSC_BUF[0] = 0x0 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_EXT_CR1_0,
			RH_USB20_OSC_BUF_0, 0);

		/* RG_USB20_ROSC_EN = 0x1 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3,
			RG_USB20_ROSC_EN, RG_USB20_ROSC_EN);

		udelay(100);

		/* RG_INIT_SW_PHY_SEL = 0x0 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_1,
			RG_INIT_SW_PHY_SEL, 0);

	} else if ((chip_rev & MT6379_CHIP_REV_MASK) >= 0x3) {
		if (rptr->submode == PHY_MODE_SUSPEND_DEV) {
			/* device connected */
			return 0;
		} else if (rptr->submode == PHY_MODE_SUSPEND_NO_DEV) {
			dev_info(rptr->dev, "PHY on OTG gender LP mode\n");
			rptr->otg_gender = false;
			/* OTG gender LP mode */
			/* RG_USB20_HSTX_SRCTRL[1] 1'b0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR1_2, RG_USB20_HSTX_SRCTRL, 0);

			/* RG_USB20_REV_COM[7:6]	2'b00 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_2, RG_USB20_REV_COM, 0);

			/* RG_eusb20_hsrx_bias_en_sel	2'b01 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_EU2_CR1_2,
				RG_EUSB20_HSRX_BIAS_EN_SEL, 1 << 2);

			/* RG_frc_usb20_hsrx_bias_en_sel	1'b0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR3_2, RG_RFC_USB20_HSRX_BIAS_EN_SEL, 0);

			/* RG_usb20_hsrx_bias_en_sel	2'b10 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_1,
				RG_USB20_HSRX_BIAS_EN_SEL_LP, 0x1 << 2);

			/* rg_init_sw_phy_sel	1'b1 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_1,
				RG_INIT_SW_PHY_SEL, RG_INIT_SW_PHY_SEL);

			/* RG_USB20_BC11_SW_EN 1'b0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3, RG_USB20_BC11_SW_EN, 0);

			/* RG_USB20_BGR_EN 1'b1 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_0, RG_USB20_BGR_EN, RG_USB20_BGR_EN);
			udelay(30);

			/* RG_USB20_OSC_BUF[0] = 0x1 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_EXT_CR1_0,
				RH_USB20_OSC_BUF_0, RH_USB20_OSC_BUF_0);

			/* RG_USB20_ROSC_EN = 0x1 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3,
				RG_USB20_ROSC_EN, RG_USB20_ROSC_EN);

			udelay(10);

			/* RG_USB20_ROSC_EN = 0x0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3,
				RG_USB20_ROSC_EN, 0);

			udelay(30);

			/* RG_USB20_OSC_BUF[0] = 0x0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_EXT_CR1_0,
				RH_USB20_OSC_BUF_0, 0);

			/* RG_USB20_ROSC_EN = 0x1 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3,
				RG_USB20_ROSC_EN, RG_USB20_ROSC_EN);

			udelay(100);

			/* RG_USB20_REV_A[5]	1'b1 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_2,
				RG_USB20_REV_A_5, RG_USB20_REV_A_5);

			/* RG_EUSB_LOGRST	1'b0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYD_DBG_CR0_1, RG_EUSB_LOGRST, 0);

			/* RG_INIT_SW_PHY_SEL = 0x0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_1,
				RG_INIT_SW_PHY_SEL, 0);
		} else {
			regmap_update_bits(rptr->regmap, rptr->base + 0x92, 0x7, 0x7);
			regmap_update_bits(rptr->regmap, rptr->base + 0x94, 0x2, 0x2);

			if (rptr->otg_gender == true) {
				dev_info(rptr->dev, "Wrong Status, OTG gender mode but normal pwr on...\n");
				rptr->otg_gender = false;
				/* RG_USB20_HSTX_SRCTRL[1] 1'b0 */
				regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR1_2, RG_USB20_HSTX_SRCTRL, 0);

				/* RG_USB20_REV_COM[7:6]	2'b00 */
				regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_2, RG_USB20_REV_COM, 0);

				/* RG_eusb20_hsrx_bias_en_sel	2'b01 */
				regmap_update_bits(rptr->regmap, rptr->base + PHYA_EU2_CR1_2,
					RG_EUSB20_HSRX_BIAS_EN_SEL, 1 << 2);

				/* RG_frc_usb20_hsrx_bias_en_sel	1'b0 */
				regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR3_2,
					RG_RFC_USB20_HSRX_BIAS_EN_SEL, 0);

				/* RG_usb20_hsrx_bias_en_sel	2'b10 */
				regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_1,
					RG_USB20_HSRX_BIAS_EN_SEL_LP, 0x1 << 2);

				/* rg_init_sw_phy_sel	1'b1 */
				regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_1,
					RG_INIT_SW_PHY_SEL, RG_INIT_SW_PHY_SEL);

				/* RG_USB20_BC11_SW_EN 1'b0 */
				regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3, RG_USB20_BC11_SW_EN, 0);

				/* RG_USB20_BGR_EN 1'b1 */
				regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_0, RG_USB20_BGR_EN,
					RG_USB20_BGR_EN);
				udelay(30);

				/* RG_USB20_OSC_BUF[0] = 0x1 */
				regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_EXT_CR1_0,
					RH_USB20_OSC_BUF_0, RH_USB20_OSC_BUF_0);

				/* RG_USB20_ROSC_EN = 0x1 */
				regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3,
					RG_USB20_ROSC_EN, RG_USB20_ROSC_EN);

				udelay(10);

				/* RG_USB20_ROSC_EN = 0x0 */
				regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3,
					RG_USB20_ROSC_EN, 0);

				udelay(30);

				/* RG_USB20_OSC_BUF[0] = 0x0 */
				regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_EXT_CR1_0,
					RH_USB20_OSC_BUF_0, 0);

				/* RG_USB20_ROSC_EN = 0x1 */
				regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3,
					RG_USB20_ROSC_EN, RG_USB20_ROSC_EN);

				udelay(100);

				/* RG_USB20_REV_A[5]	1'b1 */
				regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_2,
					RG_USB20_REV_A_5, RG_USB20_REV_A_5);

				/* RG_EUSB_LOGRST	1'b0 */
				regmap_update_bits(rptr->regmap, rptr->base + PHYD_DBG_CR0_1, RG_EUSB_LOGRST, 0);

				/* RG_INIT_SW_PHY_SEL = 0x0 */
				regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_1,
					RG_INIT_SW_PHY_SEL, 0);

				udelay(300);
			}


			/* off */
			/* RG_USB_EN_SRC_SEL = 0x1 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_3,
				RG_USB_EN_SRC_SEL, RG_USB_EN_SRC_SEL);

			/* RG_USB20_BC11_SW_EN = 0x1 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3,
				RG_USB20_BC11_SW_EN, RG_USB20_BC11_SW_EN);

			/* RG_USB20_BGR_EN = 0x0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_0, RG_USB20_BGR_EN, 0);

			/* RG_USB20_ROSC_EN = 0x0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3, RG_USB20_ROSC_EN, 0);

			/* RG_USB20_REV_A[5] = 0x0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_2, RG_USB20_REV_A_5, 0);

			udelay(300);

			/* Apply sw/hw efuse tuning */
			eusb2_efuse_prop_set(rptr);

			/* modify default value */
			eusb2_default_prop_set(rptr);

			/* Modify OSC_BIAS  OSC_BIAS<2:1>, LS_CR<0> */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_EXT_CR1_1, 0x6, 0x6);

			regmap_update_bits(rptr->regmap, rptr->base + PHYA_EU2_CR1_0, RG_USB20_OSC_BIAS_0,
				RG_USB20_OSC_BIAS_0);

			/* Set OSC_IBAND to 0x52 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_EXT_CR0_1, 0xFF, 0x52);

			/* on */
			/* RG_USB20_REV_A[5] = 0x1 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_2,
				RG_USB20_REV_A_5, RG_USB20_REV_A_5);

			/* RG_INIT_SW_PHY_SEL = 0x1 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_1,
				RG_INIT_SW_PHY_SEL, RG_INIT_SW_PHY_SEL);

			/* RG_USB_EN_SRC_SEL = 0x0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_3, RG_USB_EN_SRC_SEL, 0);

			/* RG_USB20_BGR_EN = 0x1 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_0, RG_USB20_BGR_EN, RG_USB20_BGR_EN);

			udelay(100);

			/* RG_USB20_BC11_SW_EN = 0x0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3, RG_USB20_BC11_SW_EN, 0);

			udelay(50);

			/* RG_USB20_OSC_BUF[0] = 0x1 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_EXT_CR1_0,
				RH_USB20_OSC_BUF_0, RH_USB20_OSC_BUF_0);

			/* RG_USB20_ROSC_EN = 0x1 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3,
				RG_USB20_ROSC_EN, RG_USB20_ROSC_EN);

			udelay(10);

			/* RG_USB20_ROSC_EN = 0x0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3,
				RG_USB20_ROSC_EN, 0);

			udelay(30);

			/* RG_USB20_OSC_BUF[0] = 0x0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_EXT_CR1_0,
				RH_USB20_OSC_BUF_0, 0);

			/* RG_USB20_ROSC_EN = 0x1 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3,
				RG_USB20_ROSC_EN, RG_USB20_ROSC_EN);

			udelay(100);

			/* RG_INIT_SW_PHY_SEL = 0x0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_1,
				RG_INIT_SW_PHY_SEL, 0);

			/* RG_USB20_LPM_EN = 0x1 */
			if ((chip_rev & MT6379_CHIP_REV_MASK) >= 0x5)
				regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR3_2,
					RG_USB20_LPM_EN, RG_USB20_LPM_EN);
		}
	}

	return 0;
}

static int eusb2_repeater_power_off(struct phy *phy)
{
	struct eusb2_repeater *rptr = phy_get_drvdata(phy);
	unsigned int chip_rev = 0;

	regmap_read(rptr->regmap, MT6379_REG_DEV_INFO, &chip_rev);
	dev_info(rptr->dev, "eusb2 repeater power off chip_rev(%x) submode(%x)\n", chip_rev, rptr->submode);

	if ((chip_rev & MT6379_CHIP_REV_MASK) <= 0x2) {
		/* off */
		/* RG_USB_EN_SRC_SEL = 0x1 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_3,
			RG_USB_EN_SRC_SEL, RG_USB_EN_SRC_SEL);

		/* RG_USB20_BC11_SW_EN = 0x1 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3,
			RG_USB20_BC11_SW_EN, RG_USB20_BC11_SW_EN);

		/* RG_USB20_BGR_EN = 0x0 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_0, RG_USB20_BGR_EN, 0);

		/* RG_USB20_ROSC_EN = 0x0 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3, RG_USB20_ROSC_EN, 0);

		/* RG_USB20_REV_A[5] = 0x0 */
		regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_2, RG_USB20_REV_A_5, 0);

		udelay(300);
	} else if ((chip_rev & MT6379_CHIP_REV_MASK) >= 0x3) {
		if (rptr->submode == PHY_MODE_SUSPEND_DEV) {
			/* device connected */
			return 0;
		} else if (rptr->submode == PHY_MODE_SUSPEND_NO_DEV) {
			dev_info(rptr->dev, "OTG gender LP mode\n");
			rptr->otg_gender = true;
			/* OTG gender LP mode */
			/* RG_EUSB_LOGRST	1'b1 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYD_DBG_CR0_1, RG_EUSB_LOGRST, RG_EUSB_LOGRST);

			/* RG_USB20_BGR_EN	1'b0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_0, RG_USB20_BGR_EN, 0);

			/* rg_usb20_rosc_en 1'b0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3, RG_USB20_ROSC_EN, 0);

			/* RG_USB20_BC11_SW_EN	1'b0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3, RG_USB20_BC11_SW_EN, 0);

			/* RG_USB20_REV_A[5]	1'b0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_2, RG_USB20_REV_A_5, 0);

			/* rg_init_sw_phy_sel	1'b1 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_1,
				RG_INIT_SW_PHY_SEL, RG_INIT_SW_PHY_SEL);

			/* RG_USB20_REV_COM[7:6]	2'b11 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_2,
				RG_USB20_REV_COM, RG_USB20_REV_COM);

			/* RG_eusb20_hsrx_bias_en_sel	2'b11 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_EU2_CR1_2,
				RG_EUSB20_HSRX_BIAS_EN_SEL, RG_EUSB20_HSRX_BIAS_EN_SEL);

			/* RG_frc_usb20_hsrx_bias_en_sel	1'b1 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR3_2,
				RG_RFC_USB20_HSRX_BIAS_EN_SEL, RG_RFC_USB20_HSRX_BIAS_EN_SEL);

			/* RG_usb20_hsrx_bias_en_sel	2'b11 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_1,
				RG_USB20_HSRX_BIAS_EN_SEL_LP, RG_USB20_HSRX_BIAS_EN_SEL_LP);

			/* RG_USB20_HSTX_SRCTRL[1]	1'b1 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR1_2, RG_USB20_HSTX_SRCTRL,
				RG_USB20_HSTX_SRCTRL);
		} else {
			/* off */
			/* RG_USB_EN_SRC_SEL = 0x1 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_3,
				RG_USB_EN_SRC_SEL, RG_USB_EN_SRC_SEL);

			/* RG_USB20_BC11_SW_EN = 0x1 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3,
				RG_USB20_BC11_SW_EN, RG_USB20_BC11_SW_EN);

			/* RG_USB20_BGR_EN = 0x0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_0, RG_USB20_BGR_EN, 0);

			/* RG_USB20_ROSC_EN = 0x0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3, RG_USB20_ROSC_EN, 0);

			/* RG_USB20_REV_A[5] = 0x0 */
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_2, RG_USB20_REV_A_5, 0);

			udelay(300);
		}
	}

	return 0;
}

static int eusb2_repeater_set_mode(struct phy *phy,
				   enum phy_mode mode, int submode)
{
	struct eusb2_repeater *rptr = phy_get_drvdata(phy);
	int index;

	rptr->submode = submode;
	phy_check_role_event(mode,rptr);

	dev_info(rptr->dev, "rptr set mode:%d submode:%d\n", mode, submode);

	if (!submode) {
		switch (mode) {
		case PHY_MODE_USB_DEVICE:
			eusb2_device_prop_set(rptr);
			break;
		case PHY_MODE_USB_HOST:
			eusb2_host_prop_set(rptr);
			break;
		case PHY_MODE_INVALID:
			/* reset to default property */
			if (rptr->prop_index != 0) {
				rptr->prop_index = 0;
				eusb2_rptr_prop_apply(rptr, false, rptr->prop_index);
				eusb2_rptr_prop_apply(rptr, true, rptr->prop_index);
			}
			break;
		default:
			return -EINVAL;
		}
	} else {
		/* special case for property setting */
		index = mtk_phy_mode_property_to_index(submode);
		if (index > 0) {
			rptr->prop_index = index;
			if (mode == PHY_MODE_USB_DEVICE) {
				eusb2_rptr_prop_apply(rptr, false, index);
				eusb2_device_prop_set(rptr);
			} else if (mode == PHY_MODE_USB_HOST) {
				eusb2_rptr_prop_apply(rptr, true, index);
				eusb2_host_prop_set(rptr);
			}
			return 0;
		}

		switch (submode) {
		case PHY_MODE_DPPULLUP_SET:
			regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_0,
				RG_USB20_MANU_MODE | RG_USB20_PU_DP, RG_USB20_MANU_MODE | RG_USB20_PU_DP);
			break;
		case PHY_MODE_DPPULLUP_CLR:
			regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_0,
				RG_USB20_MANU_MODE | RG_USB20_PU_DP, 0);
			break;
		case PHY_MODE_DIS_PRE_EMP:
			regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_3, RG_USB20_HS_PE, 0);
			break;
		case PHY_MODE_SUSPEND_DEV:
			fallthrough;
		case PHY_MODE_SUSPEND_NO_DEV:
			/* Do nothing, since we will use rptr->submode later */
			break;
		default:
			return -EINVAL;
		}
	}

	rptr->submode = submode;

	return 0;
}

static const struct phy_ops eusb2_repeater_ops = {
	.init		= eusb2_repeater_init,
	.exit		= eusb2_repeater_exit,
	.power_on	= eusb2_repeater_power_on,
	.power_off	= eusb2_repeater_power_off,
	.set_mode	= eusb2_repeater_set_mode,
	.owner		= THIS_MODULE,
};

static void split_str(char *str, char *value, char *mode, int size)
{
	int i = 0;

	for (i = 0; i < size; i++) {
		if (str[i] == ' ') {
			if (i+1 < sizeof(str)) {
				mode[0]= str[i+1];
				break;
			}
		}
		value[i] = str[i];
	}
}

static void cover_val_to_str(u32 val, u8 width, char *str)
{
	int i;

	str[width] = '\0';
	for (i = (width - 1); i >= 0; i--) {
		if (val % 2)
			str[i] = '1';
		else
			str[i] = '0';
		val /= 2;
	}
}

static int proc_vrt_sel_show(struct seq_file *s, void *unused)
{
	struct eusb2_repeater *rptr = s->private;
	u32 tmp = 0;
	char dstr[16] = "No assignment";
	char hstr[16] = "No assignment";
	char str[16];

	regmap_read(rptr->regmap, rptr->base + PHYA_U2_CR0_0, &tmp);
	tmp >>= RG_USB20_VRT_SEL_SHIFT;
	tmp &= RG_USB20_VRT_SEL_MASK;

	if (rptr->vrt_sel != -EINVAL)
		cover_val_to_str(rptr->vrt_sel, 3, dstr);
	if (rptr->host_vrt_sel != -EINVAL)
		cover_val_to_str(rptr->host_vrt_sel, 3, hstr);
	cover_val_to_str(tmp, 3, str);

	seq_printf(s, "device_%s = %s\n", VRT_SEL_STR, dstr);
	seq_printf(s, "host_%s = %s\n", VRT_SEL_STR, hstr);
	seq_printf(s, "RG: %s = %s\n", VRT_SEL_STR, str);
	return 0;
}

static int proc_vrt_sel_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_vrt_sel_show, pde_data(inode));
}

static ssize_t proc_vrt_sel_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct eusb2_repeater *rptr = s->private;
	char buf[20];
	char value[20];
	char mode[1];
	int min = 0;
	u32 val;

	mode[0] = '*';
	memset(buf, 0x00, sizeof(buf));
	memset(value, 0x00, sizeof(value));
	min = min_t(size_t, sizeof(buf) - 1, count);

	if (copy_from_user(&buf, ubuf, min))
		return -EFAULT;

	if (buf[0] == ' ')
		return -EINVAL;

	split_str(buf, value, mode, min);

	if (kstrtouint(value, 2, &val))
		return -EINVAL;

	if (mode[0] == 'd')
		rptr->vrt_sel = val;
	else if (mode[0] == 'h')
		rptr->host_vrt_sel = val;
	else if (mode[0] == '*') {
		rptr->vrt_sel = val;
		rptr->host_vrt_sel = val;
	} else
		return -EINVAL;

	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_0, RG_USB20_VRT_SEL,
			val << RG_USB20_VRT_SEL_SHIFT);

	return count;
}

static const struct proc_ops proc_vrt_sel_fops = {
	.proc_open = proc_vrt_sel_open,
	.proc_write = proc_vrt_sel_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int proc_discth_show(struct seq_file *s, void *unused)
{
	struct eusb2_repeater *rptr = s->private;
	u32 tmp = 0;
	char dstr[16] = "No assignment";
	char hstr[16] = "No assignment";
	char str[16];

	regmap_read(rptr->regmap, rptr->base + PHYA_U2_CR2_0, &tmp);
	tmp >>= RG_USB20_DISCTH_SHIFT;
	tmp &= RG_USB20_DISCTH_MASK;

	if (rptr->discth != -EINVAL)
		cover_val_to_str(rptr->discth, 4, dstr);
	if (rptr->host_discth != -EINVAL)
		cover_val_to_str(rptr->host_discth, 4, hstr);
	cover_val_to_str(tmp, 4, str);

	seq_printf(s, "device_%s = %s\n", DISCTH_STR, dstr);
	seq_printf(s, "host_%s = %s\n", DISCTH_STR, hstr);
	seq_printf(s, "RG: %s = %s\n", DISCTH_STR, str);
	return 0;
}

static int proc_discth_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_discth_show, pde_data(inode));
}

static ssize_t proc_discth_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct eusb2_repeater *rptr = s->private;
	char buf[20];
	char value[20];
	char mode[1];
	int min = 0;
	u32 val;

	mode[0] = '*';
	memset(buf, 0x00, sizeof(buf));
	memset(value, 0x00, sizeof(value));
	min = min_t(size_t, sizeof(buf) - 1, count);

	if (copy_from_user(&buf, ubuf, min))
		return -EFAULT;

	if (buf[0] == ' ')
		return -EINVAL;

	split_str(buf, value, mode, min);

	if (kstrtouint(value, 2, &val))
		return -EINVAL;

	if (mode[0] == 'd')
		rptr->discth = val;
	else if (mode[0] == 'h')
		rptr->host_discth = val;
	else if (mode[0] == '*') {
		rptr->discth = val;
		rptr->host_discth = val;
	} else
		return -EINVAL;

	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_0, RG_USB20_DISCTH,
			val << RG_USB20_DISCTH_SHIFT);

	return count;
}

static const struct proc_ops proc_discth_fops = {
	.proc_open = proc_discth_open,
	.proc_write = proc_discth_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int proc_rx_sqth_show(struct seq_file *s, void *unused)
{
	struct eusb2_repeater *rptr = s->private;
	u32 tmp = 0;
	char dstr[16] = "No assignment";
	char hstr[16] = "No assignment";
	char str[16];

	regmap_read(rptr->regmap, rptr->base + PHYA_U2_CR2_0, &tmp);
	tmp >>= RG_USB20_SQTH_SHIFT;
	tmp &= RG_USB20_SQTH_MASK;

	if (rptr->rx_sqth != -EINVAL)
		cover_val_to_str(rptr->rx_sqth, 4, dstr);
	if (rptr->host_rx_sqth != -EINVAL)
		cover_val_to_str(rptr->host_rx_sqth, 4, hstr);
	cover_val_to_str(tmp, 4, str);

	seq_printf(s, "device_%s = %s\n", RX_SQTH_STR, dstr);
	seq_printf(s, "host_%s = %s\n", RX_SQTH_STR, hstr);
	seq_printf(s, "RG: %s = %s\n", RX_SQTH_STR, str);
	return 0;
}

static int proc_rx_sqth_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_rx_sqth_show, pde_data(inode));
}

static ssize_t proc_rx_sqth_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct eusb2_repeater *rptr = s->private;
	char buf[20];
	char value[20];
	char mode[1];
	int min = 0;
	u32 val;

	mode[0] = '*';
	memset(buf, 0x00, sizeof(buf));
	memset(value, 0x00, sizeof(value));
	min = min_t(size_t, sizeof(buf) - 1, count);

	if (copy_from_user(&buf, ubuf, min))
		return -EFAULT;

	if (buf[0] == ' ')
		return -EINVAL;

	split_str(buf, value, mode, min);

	if (kstrtouint(value, 2, &val))
		return -EINVAL;

	if (mode[0] == 'd')
		rptr->rx_sqth = val;
	else if (mode[0] == 'h')
		rptr->host_rx_sqth = val;
	else if (mode[0] == '*') {
		rptr->rx_sqth = val;
		rptr->host_rx_sqth = val;
	} else
		return -EINVAL;

	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_0, RG_USB20_SQTH,
			val << RG_USB20_SQTH_SHIFT);

	return count;
}

static const struct proc_ops proc_rx_sqth_fops = {
	.proc_open = proc_rx_sqth_open,
	.proc_write = proc_rx_sqth_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int proc_pre_emphasis_show(struct seq_file *s, void *unused)
{
	struct eusb2_repeater *rptr = s->private;
	u32 tmp = 0;
	char dstr[16] = "No assignment";
	char hstr[16] = "No assignment";
	char str[16];

	regmap_read(rptr->regmap, rptr->base + PHYA_U2_CR2_3, &tmp);
	tmp >>= RG_USB20_HS_PE_SHIFT;
	tmp &= RG_USB20_HS_PE_MASK;

	if (rptr->pre_emphasis != -EINVAL)
		cover_val_to_str(rptr->pre_emphasis, 3, dstr);
	if (rptr->host_pre_emphasis != -EINVAL)
		cover_val_to_str(rptr->host_pre_emphasis, 3, hstr);
	cover_val_to_str(tmp, 3, str);

	seq_printf(s, "device_%s = %s\n", PRE_EMP_STR, dstr);
	seq_printf(s, "host_%s = %s\n", PRE_EMP_STR, hstr);
	seq_printf(s, "RG: %s = %s\n", PRE_EMP_STR, str);
	return 0;
}

static int proc_pre_emphasis_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_pre_emphasis_show, pde_data(inode));
}

static ssize_t proc_pre_emphasis_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct eusb2_repeater *rptr = s->private;
	char buf[20];
	char value[20];
	char mode[1];
	int min = 0;
	u32 val;

	mode[0] = '*';
	memset(buf, 0x00, sizeof(buf));
	memset(value, 0x00, sizeof(value));
	min = min_t(size_t, sizeof(buf) - 1, count);

	if (copy_from_user(&buf, ubuf, min))
		return -EFAULT;

	if (buf[0] == ' ')
		return -EINVAL;

	split_str(buf, value, mode, min);

	if (kstrtouint(value, 2, &val))
		return -EINVAL;

	if (mode[0] == 'd')
		rptr->pre_emphasis = val;
	else if (mode[0] == 'h')
		rptr->host_pre_emphasis = val;
	else if (mode[0] == '*') {
		rptr->pre_emphasis = val;
		rptr->host_pre_emphasis = val;
	} else
		return -EINVAL;

	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_3, RG_USB20_HS_PE,
			val << RG_USB20_HS_PE_SHIFT);

	return count;
}

static const struct proc_ops proc_pre_emphasis_fops = {
	.proc_open = proc_pre_emphasis_open,
	.proc_write = proc_pre_emphasis_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int proc_equalization_show(struct seq_file *s, void *unused)
{
	struct eusb2_repeater *rptr = s->private;
	u32 tmp = 0;
	char dstr[16] = "No assignment";
	char hstr[16] = "No assignment";
	char str[16];

	regmap_read(rptr->regmap, rptr->base + PHYA_U2_CR2_3, &tmp);
	tmp >>= RG_USB20_HS_EQ_SHIFT;
	tmp &= RG_USB20_HS_EQ_MASK;

	if (rptr->equalization != -EINVAL)
		cover_val_to_str(rptr->equalization, 4, dstr);
	if (rptr->host_equalization != -EINVAL)
		cover_val_to_str(rptr->host_equalization, 4, hstr);
	cover_val_to_str(tmp, 4, str);

	seq_printf(s, "device_%s = %s\n", EQ_STR, dstr);
	seq_printf(s, "host_%s = %s\n", EQ_STR, hstr);
	seq_printf(s, "RG: %s = %s\n", EQ_STR, str);
	return 0;
}

static int proc_equalization_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_equalization_show, pde_data(inode));
}

static ssize_t proc_equalization_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct eusb2_repeater *rptr = s->private;
	char buf[20];
	char value[20];
	char mode[1];
	int min = 0;
	u32 val;

	mode[0] = '*';
	memset(buf, 0x00, sizeof(buf));
	memset(value, 0x00, sizeof(value));
	min = min_t(size_t, sizeof(buf) - 1, count);

	if (copy_from_user(&buf, ubuf, min))
		return -EFAULT;

	if (buf[0] == ' ')
		return -EINVAL;

	split_str(buf, value, mode, min);

	if (kstrtouint(value, 2, &val))
		return -EINVAL;

	if (mode[0] == 'd')
		rptr->equalization = val;
	else if (mode[0] == 'h')
		rptr->host_equalization = val;
	else if (mode[0] == '*') {
		rptr->equalization = val;
		rptr->host_equalization = val;
	} else
		return -EINVAL;

	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_3, RG_USB20_HS_EQ,
			val << RG_USB20_HS_EQ_SHIFT);

	return count;
}

static const struct proc_ops proc_equalization_fops = {
	.proc_open = proc_equalization_open,
	.proc_write = proc_equalization_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int proc_intr_ofs_show(struct seq_file *s, void *unused)
{
	struct eusb2_repeater *rptr = s->private;
	u32 tmp = 0;
	char str[16];

	regmap_read(rptr->regmap, rptr->base + PHYA_U2_CR0_1, &tmp);
	tmp >>= RG_USB20_INTR_CAL_SHIFT;
	tmp &= RG_USB20_INTR_CAL_MASK;

	cover_val_to_str(tmp, 6, str);

	seq_printf(s, "%s = %d\n", INTR_OFS_STR,
		(rptr->intr_ofs == -(RG_USB20_INTR_CAL_MASK + 1)? 0 : rptr->intr_ofs));
	seq_printf(s, "%s = %d\n", "host_intr_ofs",
		(rptr->host_intr_ofs == -(RG_USB20_INTR_CAL_MASK + 1)? 0 : rptr->host_intr_ofs));
	seq_printf(s, "%s = %d\n", "intr_cal", rptr->intr_cal);
	seq_printf(s, "%s = %s (%d)\n", "RG intr cal val", str, tmp);

	return 0;
}

static int proc_intr_ofs_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_intr_ofs_show, pde_data(inode));
}

static ssize_t proc_intr_ofs_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct eusb2_repeater *rptr = s->private;
	char buf[20];
	char value[20];
	char mode[1];
	u32 val, new_val;

	mode[0] = '*';
	memset(buf, 0x00, sizeof(buf));
	memset(value, 0x00, sizeof(value));
	if (count > sizeof(buf) - 1)
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, count))
		return -EFAULT;

	if (buf[0] == ' ')
		return -EINVAL;

	split_str(buf, value, mode, count);

	if (kstrtoint(value, 10, &val))
		return -EINVAL;

	new_val = rptr->intr_cal + val;

	if (new_val < 0 || new_val > RG_USB20_INTR_CAL_MASK) {
		dev_info(rptr->dev, "intr_cal (%d) +/- intr_ofs (%d) out of range.\n",
		rptr->intr_cal, val);
		return -EINVAL;
	}

	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_1, RG_USB20_INTR_CAL,
			new_val << RG_USB20_INTR_CAL_SHIFT);

	if (mode[0] == 'd')
		rptr->intr_ofs = val;
	else if (mode[0] == 'h')
		rptr->host_intr_ofs = val;
	else if (mode[0] == '*') {
		rptr->intr_ofs = val;
		rptr->host_intr_ofs = val;
	} else
		return -EINVAL;

	return count;
}

static const struct proc_ops proc_intr_ofs_fops = {
	.proc_open = proc_intr_ofs_open,
	.proc_write = proc_intr_ofs_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int proc_term_ofs_show(struct seq_file *s, void *unused)
{
	struct eusb2_repeater *rptr = s->private;
	u32 tmp = 0;
	char str[16];

	regmap_read(rptr->regmap, rptr->base + PHYA_U2_CR2_2, &tmp);
	tmp >>= RG_USB20_TERM_CAL_SHIFT;
	tmp &= RG_USB20_TERM_CAL_MASK;

	cover_val_to_str(tmp, 4, str);

	seq_printf(s, "%s = %d\n", TERM_OFS_STR,
		(rptr->term_ofs == -(RG_USB20_TERM_CAL_MASK + 1)? 0 : rptr->term_ofs));
	seq_printf(s, "%s = %d\n", "host_term_ofs",
		(rptr->host_term_ofs == -(RG_USB20_TERM_CAL_MASK + 1)? 0 : rptr->host_term_ofs));
	seq_printf(s, "%s = %d\n", "term_cal", rptr->term_cal);
	seq_printf(s, "%s = %s (%d)\n", "RG term cal val", str, tmp);

	return 0;
}

static int proc_term_ofs_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_term_ofs_show, pde_data(inode));
}

static ssize_t proc_term_ofs_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct eusb2_repeater *rptr = s->private;
	char buf[20];
	char value[20];
	char mode[1];
	u32 val, new_val;

	mode[0] = '*';
	memset(buf, 0x00, sizeof(buf));
	memset(value, 0x00, sizeof(value));
	if (count > sizeof(buf) - 1)
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, count))
		return -EFAULT;

	if (buf[0] == ' ')
		return -EINVAL;

	split_str(buf, value, mode, count);

	if (kstrtoint(value, 10, &val))
		return -EINVAL;

	new_val = rptr->term_cal + val;

	if (new_val < 0 || new_val > RG_USB20_TERM_CAL_MASK) {
		dev_info(rptr->dev, "term_cal (%d) +/- term_ofs (%d) out of range.\n",
		rptr->term_cal, val);
		return -EINVAL;
	}

	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_2, RG_USB20_TERM_CAL,
			new_val << RG_USB20_TERM_CAL_SHIFT);

	if (mode[0] == 'd')
		rptr->term_ofs = val;
	else if (mode[0] == 'h')
		rptr->host_term_ofs = val;
	else if (mode[0] == '*') {
		rptr->term_ofs = val;
		rptr->host_term_ofs = val;
	} else
		return -EINVAL;

	return count;
}

static const struct proc_ops proc_term_ofs_fops = {
	.proc_open = proc_term_ofs_open,
	.proc_write = proc_term_ofs_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int rptr_procfs_init(struct eusb2_repeater *rptr)
{
	struct device *dev = rptr->dev;
	struct proc_dir_entry *root = rptr->root;
	struct proc_dir_entry *file;
	int ret = 0;

	if (!root) {
		dev_info(dev, "mtk_eusb2 root not exist\n");
		ret = -ENOMEM;
		goto err0;
	}

	file = proc_create_data(VRT_SEL_STR, 0644,
			root, &proc_vrt_sel_fops, rptr);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", VRT_SEL_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(DISCTH_STR, 0644,
			root, &proc_discth_fops, rptr);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", DISCTH_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(RX_SQTH_STR, 0644,
			root, &proc_rx_sqth_fops, rptr);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", RX_SQTH_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(PRE_EMP_STR, 0644,
			root, &proc_pre_emphasis_fops, rptr);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", PRE_EMP_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(EQ_STR, 0644,
			root, &proc_equalization_fops, rptr);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", EQ_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(INTR_OFS_STR, 0644,
			root, &proc_intr_ofs_fops, rptr);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", INTR_OFS_STR);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(TERM_OFS_STR, 0644,
			root, &proc_term_ofs_fops, rptr);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", TERM_OFS_STR);
		ret = -ENOMEM;
		goto err1;
	}

	return 0;

err1:
	proc_remove(root);

err0:
	return ret;
}

static void mtk_rptr_procfs_init_worker(struct work_struct *data)
{
	struct eusb2_repeater *rptr = container_of(data,
		struct eusb2_repeater, procfs_work);
	struct proc_dir_entry *root = NULL;

	root = proc_mkdir(MTK_USB_STR, NULL);
	if (!root) {
		dev_info(rptr->dev, "%s, failed to create mtk_eusb2\n", __func__);
		return;
	}

	rptr->root = root;

	rptr_procfs_init(rptr);
}

static irqreturn_t mt6379_eusb_evt_handler(int irq, void *data)
{
	struct eusb2_repeater *rptr = data;

	/* 1. Mask all IRQ */
	/* RG_USB20_HSTX_SRCTRL[1] 1'b0 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR1_2, RG_USB20_HSTX_SRCTRL, 0);

	return IRQ_HANDLED;
}

static int eusb2_repeater_probe(struct platform_device *pdev)
{
	struct eusb2_repeater *rptr;
	struct device *dev = &pdev->dev;
	struct phy_provider *phy_provider;
	struct device_node *np = dev->of_node;
	u32 res;
	int ret, irq;

	rptr = devm_kzalloc(dev, sizeof(*rptr), GFP_KERNEL);
	if (!rptr)
		return -ENOMEM;

	rptr->dev = dev;
	dev_set_drvdata(dev, rptr);

	rptr->regmap = dev_get_regmap(dev->parent, NULL);
	if (!rptr->regmap)
		return -ENODEV;

	ret = of_property_read_u32(np, "reg", &res);
	if (ret < 0)
		return ret;

	rptr->base = res;

	/*Enable these five flag bits during initialization
	based on the country code, region code, and project requirements
	*/
	rptr->repeater_carwith_enable = 0;
	rptr->repeater_global_enable = 0;
	rptr->repeater_product_enable = 0;
	rptr->repeater_storage_enable = 0;
	rptr->repeater_not_single_port = 0;

	rptr->repeater_which_port = 0;
	rptr->repeater_enhance_support_mode = 0;
	rptr->repeater_enhance_state = 0;
	rptr->repeater_current_eye_tune = 0;
	rptr->repeater_dynamic_adjustment = 0;

	rptr->discth = -EINVAL;
	rptr->equalization = -EINVAL;
	rptr->vrt_sel = -EINVAL;
	rptr->pre_emphasis = -EINVAL;
	rptr->rx_sqth = -EINVAL;
	rptr->term_ofs = -EINVAL;
	rptr->intr_ofs = -EINVAL;

	rptr->host_discth = -EINVAL;
	rptr->host_equalization = -EINVAL;
	rptr->host_vrt_sel = -EINVAL;
	rptr->host_pre_emphasis = -EINVAL;
	rptr->host_rx_sqth = -EINVAL;
	rptr->host_term_ofs = -EINVAL;
	rptr->host_intr_ofs = -EINVAL;

	pr_err("HI MI probe enable: "
		"repeater_carwith_enable:%d, "
		"repeater_global_enable:%d, "
		"repeater_product_enable:%d, "
		"repeater_storage_enable:%d, "
		"repeater_not_single_port:%d\n",
		rptr->repeater_carwith_enable,
		rptr->repeater_global_enable,
		rptr->repeater_product_enable,
		rptr->repeater_storage_enable,
		rptr->repeater_not_single_port);

	eusb2_rptr_prop_parse(rptr);
	eusb2_rptr_prop_table_parse(rptr);
	repeater_sysfs_init(rptr,dev);
	INIT_DELAYED_WORK(&rptr->dwork, eusb2_repeater_send_uevent);

	rptr->phy = devm_phy_create(dev, np, &eusb2_repeater_ops);
	if (IS_ERR(rptr->phy)) {
		dev_info(dev, "failed to create PHY: %d\n", ret);
		return PTR_ERR(rptr->phy);
	}

	phy_set_drvdata(rptr->phy, rptr);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_info(dev, "%s, Failed to get eusb irq\n", __func__);
		return irq;
	}

	ret = devm_request_threaded_irq(rptr->dev, irq, NULL, mt6379_eusb_evt_handler,
					IRQF_ONESHOT, dev_name(rptr->dev), rptr);
	if (ret) {
		dev_info(dev, "%s, Failed to request irq\n", __func__);
		return ret;
	}

	dev_pm_set_wake_irq(rptr->dev, irq);

	ret = regmap_update_bits(rptr->regmap, 0x0E, BIT(1), 0);
	if (ret) {
		dev_info(dev, "%s, Failed to unmask eusb irq\n", __func__);
		return ret;
	}

	/* create rptr workqueue */
	rptr->wq = create_singlethread_workqueue("rptr_phy");
	if (!rptr->wq)
		return -ENOMEM;

	INIT_WORK(&rptr->procfs_work, mtk_rptr_procfs_init_worker);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider))
		return PTR_ERR(phy_provider);

	rptr->otg_gender = false;

	dev_info(dev, "MTK MT6379 eusb2 repeater probe done\n");

	return 0;
}

static int eusb2_rptr_procfs_exit(struct eusb2_repeater *rptr)
{
	proc_remove(rptr->root);
	return 0;
}

static void eusb2_repeater_remove(struct platform_device *pdev)
{
	struct eusb2_repeater *rptr = dev_get_drvdata(&pdev->dev);

	kfree(rptr->host_prop_table);
	rptr->host_prop_table = NULL;

	kfree(rptr->device_prop_table);
	rptr->device_prop_table = NULL;

	eusb2_rptr_procfs_exit(rptr);
	repeater_sysfs_cleanup(rptr,&pdev->dev);
}

static const struct of_device_id eusb2_repeater_of_match_table[] = {
	{.compatible = "mtk,mt6379-eusb2-repeater",},
	{},
};
MODULE_DEVICE_TABLE(of, eusb2_repeater_of_match_table);

static struct platform_driver eusb2_repeater_driver = {
	.driver = {
		.name = "mt6379-eusb2-repeater",
		.of_match_table = eusb2_repeater_of_match_table,
	},
	.probe = eusb2_repeater_probe,
	.remove = eusb2_repeater_remove,
};

module_platform_driver(eusb2_repeater_driver);

MODULE_DESCRIPTION("MediaTek eUSB2 MT6379 Repeater Driver");
MODULE_LICENSE("GPL");
