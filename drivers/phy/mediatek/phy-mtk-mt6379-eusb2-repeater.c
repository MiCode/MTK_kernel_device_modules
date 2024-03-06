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

/* MT6379 eUSB2 control registers */
#define RG_USB_EN_SRC_SEL		(1 << 5)
#define RG_USB20_BC11_SW_EN		(1 << 0)
#define RG_USB20_REV_A_5		(1 << 5)
#define RG_USB20_HSRX_BIAS_EN_SEL	(1 << 2)
#define RG_FM_DUMMY			(1 << 4)
#define RG_INIT_SW_PHY_SEL		(1 << 7)
#define RG_USB20_BGR_EN			(1 << 0)
#define RG_USB20_ROSC_EN		(1 << 1)

#define PHYA_U2_CR0_0			0x10
#define RG_USB20_VRT_SEL		GENMASK(6,4)
#define RG_USB20_VRT_SEL_SHIFT		4
#define RG_USB20_VRT_SEL_MASK		0x7

#define PHYA_U2_CR0_1			0x11
#define RG_USB20_INTR_CAL		GENMASK(5,0)
#define RG_USB20_INTR_CAL_SHIFT		0
#define RG_USB20_INTR_CAL_MASK		0x3f

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

#define PHYA_COM_CR0_0			0x0
#define PHYA_COM_CR0_3			0x3
#define PHYA_U2_CR0_2			0x12
#define PHYA_U2_CR0_3			0x13
#define PHA_EU2_CR0_1			0x21
#define PHA_EU2_CR0_2			0x22
#define PHYD_COM_CR2_1			0x89
#define PHYD_COM_CR2_3			0x8b
#define PHYA_U2_CR2_1			0x0019
#define PHYD_FM_CR0_1			0x00A5

/* MT6379 VID */
#define MT6379_REG_DEV_INFO	0x70
#define MT6379_VENID_MASK	GENMASK(7, 4)

/* EUSB EFUSE */
#define EUSB_RSV_81			0x381
#define EUSB_RSV_82			0x382

/* U2 */
#define MTK_USB_STR	"mtk_eusb2"
#define VRT_SEL_STR	"vrt_sel"
#define DISCTH_STR	"discth"
#define RX_SQTH_STR	"rx_sqth"
#define PRE_EMP_STR	"pre_emphasis"
#define EQ_STR		"equalization"
#define INTR_CAL_STR	"intr_cal"
#define TERM_CAL_STR	"term_cal"
#define EUSB2_SQTH_STR	"eusb2_sqth"
#define EUSB2_HSTX_SR_STR	"eusb2_hstx_sr"
#define EUSB2_REV_COM		"eusb2_tx_swing_enhance"

struct eusb2_repeater {
	struct device *dev;
	struct regmap *regmap;
	struct phy *phy;
	u16 base;
	/* tuning parameter */
	int intr_cal;
	int term_cal;
	/* Device */
	int vrt_sel;
	int rx_sqth;
	int discth;
	int pre_emphasis;
	int equalization;
	/* Host */
	int host_vrt_sel;
	int host_rx_sqth;
	int host_discth;
	int host_pre_emphasis;
	int host_equalization;
	enum phy_mode mode;
	struct proc_dir_entry *root;
	struct work_struct procfs_work;
	struct workqueue_struct *wq;
};

static void eusb2_rptr_prop_parse(struct eusb2_repeater *rptr)
{
	struct device *dev = rptr->dev;

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

	dev_info(dev, "host-vrt-vref:%d, host-rx-sqth:%d, host-discth:%d, host-pre-emphasis:%d, host-eq:%d",
		rptr->host_vrt_sel, rptr->host_rx_sqth, rptr->host_discth, rptr->host_pre_emphasis,
		rptr->host_equalization);


}

static void eusb2_prop_set(struct eusb2_repeater *rptr)
{

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

}

static void eusb2_host_prop_set(struct eusb2_repeater *rptr)
{

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

}

static void eusb2_rptr_prop_set(struct eusb2_repeater *rptr)
{
	u32 value1;
	u32 value2;

	/* eUSB2 eFUSE */
	regmap_read(rptr->regmap, EUSB_RSV_81, &value1);
	regmap_read(rptr->regmap, EUSB_RSV_82, &value2);

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

static int eusb2_repeater_power_on(struct phy *phy)
{
	struct eusb2_repeater *rptr = phy_get_drvdata(phy);

	dev_info(rptr->dev, "eusb2 repeater power on\n");

	regmap_update_bits(rptr->regmap, rptr->base + 0x92, 0x7, 0x7);

	/* off */
	/* RG_USB_EN_SRC_SEL = 0x1 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_3, RG_USB_EN_SRC_SEL, RG_USB_EN_SRC_SEL);

	/* RG_USB20_BC11_SW_EN = 0x1 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3, RG_USB20_BC11_SW_EN, RG_USB20_BC11_SW_EN);

	/* RG_USB20_REV_A[5] = 0x0 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_2, RG_USB20_REV_A_5, 0);

	/* on */
	/* RG_USB_EN_SRC_SEL = 0x0 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_3, RG_USB_EN_SRC_SEL, 0);

	/* RG_INIT_SW_PHY_SEL = 0x1 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_1, RG_INIT_SW_PHY_SEL, RG_INIT_SW_PHY_SEL);

	/* RG_USB20_BGR_EN = 0x1 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_0, RG_USB20_BGR_EN, RG_USB20_BGR_EN);

	/* RG_USB20_ROSC_EN = 0x0 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3, RG_USB20_ROSC_EN, 0);

	/* RG_USB20_BC11_SW_EN = 0x0 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3, RG_USB20_BC11_SW_EN, 0);

	/* RG_USB20_ROSC_EN = 0x1 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3, RG_USB20_ROSC_EN, RG_USB20_ROSC_EN);

	udelay(100);

	/* RG_INIT_SW_PHY_SEL = 0x0 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_1, RG_INIT_SW_PHY_SEL, 0);

	/* RG_USB20_REV_A[5] = 0x1 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_2, RG_USB20_REV_A_5, RG_USB20_REV_A_5);

	/* The default value of HS disconnect threshold is too high, adjust HS DISC TH */
	/* (default 0x0D18 [7:4]= 4b'1110 to 0x0D18 [7:4]= 4b'0110) */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_0, RG_USB20_DISCTH, 0x6 << RG_USB20_DISCTH_SHIFT);

	/* FS enumeration fail, change the decoupling cap value in bias circuit */
	/* (default 0x0D18 [10:9]= 2b'10 to [10:9]= 2b'00, 0x0DA4 [15:12]= 4b'0000 to 0x0DA4 [15:12]= 4b'0001) */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_1, RG_USB20_HSRX_BIAS_EN_SEL, 0);
	regmap_update_bits(rptr->regmap, rptr->base + PHYD_FM_CR0_1, RG_FM_DUMMY, RG_FM_DUMMY);

	/* Set phy tuning */
	eusb2_rptr_prop_set(rptr);

	return 0;
}

static int eusb2_repeater_power_off(struct phy *phy)
{
	struct eusb2_repeater *rptr = phy_get_drvdata(phy);

	dev_info(rptr->dev, "eusb2 repeater power off\n");

	/* off */
	/* RG_USB_EN_SRC_SEL = 0x1 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_3, RG_USB_EN_SRC_SEL, RG_USB_EN_SRC_SEL);
	/* RG_USB20_BC11_SW_EN = 0x1 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3, RG_USB20_BC11_SW_EN, RG_USB20_BC11_SW_EN);
	/* RG_USB20_REV_A[5] = 0x0 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_2, RG_USB20_REV_A_5, 0);

	return 0;
}

static int eusb2_repeater_set_mode(struct phy *phy,
				   enum phy_mode mode, int submode)
{
	struct eusb2_repeater *rptr = phy_get_drvdata(phy);

	dev_info(rptr->dev, "rptr set mode:%d submode:%d\n", mode, submode);

	if (!submode) {
		switch (mode) {
		case PHY_MODE_USB_DEVICE:
			eusb2_prop_set(rptr);
			break;
		case PHY_MODE_USB_HOST:
			eusb2_host_prop_set(rptr);
			break;
		default:
			return -EINVAL;
		}
	}

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

static void cover_val_to_str(u32 val, u8 width, char *str)
{
	int i, temp;

	temp = val;
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
	u32 tmp;
	char str[16];

	regmap_read(rptr->regmap, rptr->base + PHYA_U2_CR0_0, &tmp);
	tmp >>= RG_USB20_VRT_SEL_SHIFT;
	tmp &= RG_USB20_VRT_SEL_MASK;

	cover_val_to_str(tmp, 3, str);

	seq_printf(s, "\n%s = %s\n", VRT_SEL_STR, str);
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
	u32 val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 2, &val))
		return -EINVAL;

	rptr->vrt_sel = val;

	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_0, RG_USB20_VRT_SEL,
			rptr->vrt_sel << RG_USB20_VRT_SEL_SHIFT);

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
	u32 tmp;
	char str[16];

	regmap_read(rptr->regmap, rptr->base + PHYA_U2_CR2_0, &tmp);
	tmp >>= RG_USB20_DISCTH_SHIFT;
	tmp &= RG_USB20_DISCTH_MASK;

	cover_val_to_str(tmp, 4, str);

	seq_printf(s, "\n%s = %s\n", DISCTH_STR, str);
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
	u32 val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 2, &val))
		return -EINVAL;

	rptr->discth = val;

	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_0, RG_USB20_DISCTH,
			rptr->discth << RG_USB20_DISCTH_SHIFT);

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
	u32 tmp;
	char str[16];

	regmap_read(rptr->regmap, rptr->base + PHYA_U2_CR2_0, &tmp);
	tmp >>= RG_USB20_SQTH_SHIFT;
	tmp &= RG_USB20_SQTH_MASK;

	cover_val_to_str(tmp, 4, str);

	seq_printf(s, "\n%s = %s\n", RX_SQTH_STR, str);
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
	u32 val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 2, &val))
		return -EINVAL;

	rptr->rx_sqth = val;

	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_0, RG_USB20_SQTH,
			rptr->rx_sqth << RG_USB20_SQTH_SHIFT);

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
	u32 tmp;
	char str[16];

	regmap_read(rptr->regmap, rptr->base + PHYA_U2_CR2_3, &tmp);
	tmp >>= RG_USB20_HS_PE_SHIFT;
	tmp &= RG_USB20_HS_PE_MASK;

	cover_val_to_str(tmp, 3, str);

	seq_printf(s, "\n%s = %s\n", PRE_EMP_STR, str);
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
	u32 val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 2, &val))
		return -EINVAL;

	rptr->pre_emphasis = val;

	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_3, RG_USB20_HS_PE,
			rptr->pre_emphasis << RG_USB20_HS_PE_SHIFT);

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
	u32 tmp;
	char str[16];

	regmap_read(rptr->regmap, rptr->base + PHYA_U2_CR2_3, &tmp);
	tmp >>= RG_USB20_HS_EQ_SHIFT;
	tmp &= RG_USB20_HS_EQ_MASK;

	cover_val_to_str(tmp, 4, str);

	seq_printf(s, "\n%s = %s\n", EQ_STR, str);
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
	u32 val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 2, &val))
		return -EINVAL;

	rptr->equalization = val;

	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_3, RG_USB20_HS_EQ,
			rptr->equalization << RG_USB20_HS_EQ_SHIFT);

	return count;
}

static const struct proc_ops proc_equalization_fops = {
	.proc_open = proc_equalization_open,
	.proc_write = proc_equalization_write,
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

static int eusb2_repeater_probe(struct platform_device *pdev)
{
	struct eusb2_repeater *rptr;
	struct device *dev = &pdev->dev;
	struct phy_provider *phy_provider;
	struct device_node *np = dev->of_node;
	u32 res;
	int ret;

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

	eusb2_rptr_prop_parse(rptr);

	rptr->phy = devm_phy_create(dev, np, &eusb2_repeater_ops);
	if (IS_ERR(rptr->phy)) {
		dev_info(dev, "failed to create PHY: %d\n", ret);
		return PTR_ERR(rptr->phy);
	}

	phy_set_drvdata(rptr->phy, rptr);

	/* create rptr workqueue */
	rptr->wq = create_singlethread_workqueue("rptr_phy");
	if (!rptr->wq)
		return -ENOMEM;

	INIT_WORK(&rptr->procfs_work, mtk_rptr_procfs_init_worker);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider))
		return PTR_ERR(phy_provider);

	dev_info(dev, "MTK MT6379 eusb2 repeater probe done\n");

	return 0;
}

static int eusb2_rptr_procfs_exit(struct eusb2_repeater *rptr)
{
	proc_remove(rptr->root);
	return 0;
}

static int eusb2_repeater_remove(struct platform_device *pdev)
{
	struct eusb2_repeater *rptr = dev_get_drvdata(&pdev->dev);

	eusb2_rptr_procfs_exit(rptr);
	return 0;
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
