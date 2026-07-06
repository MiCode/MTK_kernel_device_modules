// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/spmi.h>
#include <dt-bindings/spmi/spmi.h>
#include "mtk-spmi-pmic-debug.h"

#define MT6316_SWCID_L_E4_CODE                (0x30)
struct mutex dump_mutex;
struct mtk_spmi_pmic_debug_data {
	struct mutex lock;
	struct regmap *regmap;
	unsigned int reg_value;
	u8 usid;
	u8 cid;
	unsigned int cid_addr_h;
	unsigned int cid_addr_l;
	unsigned int spmi_glitch_sta_sel;
	unsigned int spmi_glitch_sts0;
	unsigned int spmi_debug_out_l;
	unsigned int spmi_debug_out_l_mask;
	unsigned int spmi_debug_out_l_shift;
};

static struct mtk_spmi_pmic_debug_data *mtk_spmi_pmic_debug[SPMI_MAX_SLAVE_ID];

static ssize_t pmic_access_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct mtk_spmi_pmic_debug_data *data = dev_get_drvdata(dev);

	dev_info(dev, "0x%x\n", data->reg_value);
	return sprintf(buf, "0x%x\n", data->reg_value);
}

static ssize_t pmic_access_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t size)
{
	struct mtk_spmi_pmic_debug_data *data;
	struct regmap *regmap;
	unsigned int reg_val = 0, reg_adr = 0;
	int ret = 0;
	char *pvalue = NULL, *addr, *val;
	u8 usid = 0;

	if (dev) {
		data = dev_get_drvdata(dev);
		if (!data)
			return -ENODEV;
	} else
		return -ENODEV;

	if (buf != NULL && size != 0) {
		dev_info(dev, "size is %d, buf is %s\n", (int)size, buf);

		pvalue = (char *)buf;
		addr = strsep(&pvalue, " ");
		val = strsep(&pvalue, " ");
		if (addr) {
			ret = kstrtou32(addr, 16, (unsigned int *)&reg_adr);
			if (ret < 0)
				dev_notice(dev, "%s failed to use kstrtou32\n", __func__);
		}
		if (reg_adr & 0xF0000) {
			usid = (u8)((reg_adr & 0xF0000) >> 16);
			if (!mtk_spmi_pmic_debug[usid]) {
				data->reg_value = 0;
				dev_info(dev, "invalid slave-%d\n", usid);
				return -ENODEV;
			}
			regmap = mtk_spmi_pmic_debug[usid]->regmap;
			reg_adr &= (0xFFFF);
		} else {
			usid = data->usid;
			regmap = data->regmap;
		}
		mutex_lock(&data->lock);

		if (val) {
			ret = kstrtou32(val, 16, (unsigned int *)&reg_val);
			if (ret < 0)
				dev_notice(dev, "%s failed to use kstrtou32\n", __func__);
			regmap_write(regmap, reg_adr, reg_val);
		} else
			regmap_read(regmap, reg_adr, &data->reg_value);

		mutex_unlock(&data->lock);
		dev_info(dev, "%s slave-%d PMIC Reg[0x%x]=0x%x!\n",
			 (val ? "write" : "read"), usid, reg_adr,
			 (val ? reg_val : data->reg_value));
	}
	return size;
}
static DEVICE_ATTR_RW(pmic_access);

void mtk_spmi_pmic_get_glitch_cnt(u16 *buf)
{
	struct regmap *regmap;

	unsigned int i, reg_val = 0, glitch_sta = 0;
	u16 sck_cnt = 0, sck_degitch_cnt = 0, sda_cnt = 0;
	u16 glitch_cnt_array[spmi_glitch_idx_cnt] = {0};

	glitch_cnt_array[0] = 1; //temp hardcode version
	mutex_lock(&dump_mutex);
	for (i = 2; i <= 15; i++) {
		if (mtk_spmi_pmic_debug[i] == NULL)
			continue;

		/* pr_info("%s slvid 0x%x cid 0x%x\n", __func__, i, */
		/* mtk_spmi_pmic_debug[i]->cid); */

		if (mtk_spmi_pmic_debug[i]->cid == 0x16) {
			regmap = mtk_spmi_pmic_debug[i]->regmap;
			regmap_read(regmap, mtk_spmi_pmic_debug[i]->cid_addr_l, &reg_val);
			pr_info("%s slvid 0x%x MT6316 %s cid_l 0x%x\n", __func__, i,
				reg_val >= MT6316_SWCID_L_E4_CODE ? "E4" : "E3", reg_val);
			if (reg_val >= MT6316_SWCID_L_E4_CODE) {
				/* dump glitch status */
				regmap_read(regmap, mtk_spmi_pmic_debug[i]->spmi_glitch_sts0, &glitch_sta);
				/* pr_info("%s glitch status: slvid 0x%x 0x%x\n", __func__, i, glitch_sta); */

				/* dump rising/falling edge deglitch counter */
				reg_val = 0x2;
				regmap_update_bits(regmap,
					mtk_spmi_pmic_debug[i]->spmi_glitch_sta_sel,
					(mtk_spmi_pmic_debug[i]->spmi_debug_out_l_mask <<
					mtk_spmi_pmic_debug[i]->spmi_debug_out_l_shift), reg_val);
				regmap_bulk_read(regmap, mtk_spmi_pmic_debug[i]->spmi_debug_out_l,
					(void *)&sck_cnt, 2);

				reg_val = 0xa;
				regmap_update_bits(regmap,
					mtk_spmi_pmic_debug[i]->spmi_glitch_sta_sel,
					(mtk_spmi_pmic_debug[i]->spmi_debug_out_l_mask <<
					mtk_spmi_pmic_debug[i]->spmi_debug_out_l_shift), reg_val);
				regmap_bulk_read(regmap, mtk_spmi_pmic_debug[i]->spmi_debug_out_l,
					(void *)&sck_degitch_cnt, 2);

				reg_val = 0xe;
				regmap_update_bits(regmap,
					mtk_spmi_pmic_debug[i]->spmi_glitch_sta_sel,
					(mtk_spmi_pmic_debug[i]->spmi_debug_out_l_mask <<
					mtk_spmi_pmic_debug[i]->spmi_debug_out_l_shift), reg_val);
				regmap_bulk_read(regmap, mtk_spmi_pmic_debug[i]->spmi_debug_out_l,
					(void *)&sda_cnt, 2);

				reg_val = 0x6;
				regmap_update_bits(regmap,
					mtk_spmi_pmic_debug[i]->spmi_glitch_sta_sel,
					(mtk_spmi_pmic_debug[i]->spmi_debug_out_l_mask <<
					mtk_spmi_pmic_debug[i]->spmi_debug_out_l_shift), reg_val);

				glitch_cnt_array[4*i+1] = glitch_sta;
				glitch_cnt_array[4*i+2] = sck_cnt;
				glitch_cnt_array[4*i+3] = sck_degitch_cnt;
				glitch_cnt_array[4*i+4] = sda_cnt;
				/* pr_info("%s scl_cnt: 0x%x, scl_degitch_cnt: 0x%x, sda_cnt: 0x%x\n", */
				/* __func__, sck_cnt, sck_degitch_cnt, sda_cnt); */
			}
		}
	}
	if (buf != NULL)
		memcpy(buf, glitch_cnt_array, spmi_glitch_idx_cnt*sizeof(u16));
	else {
		for (i = 1; i < spmi_glitch_idx_cnt; i+=4) {
			pr_info("%s SLVID(0x%x): glitch_sta 0x%x, scl_glitch_cnt 0x%x, scl-deglitch_cnt 0x%x, sda_glitch_cnt 0x%x",
				__func__, (i-1)/4, glitch_cnt_array[i],glitch_cnt_array[i+1],
				glitch_cnt_array[i+2], glitch_cnt_array[i+3]);
		}
	}
	mutex_unlock(&dump_mutex);
}
EXPORT_SYMBOL_GPL(mtk_spmi_pmic_get_glitch_cnt);

static int mtk_spmi_debug_parse_dt(struct device *dev, struct mtk_spmi_pmic_debug_data *data)
{
	struct device_node *node_parent, *node;
	int err;
	u32 reg[2] = {0}, cid_addr[3] = {0}, debug_out_l[3] = {0};

	if (!dev || !dev->of_node || !dev->parent->of_node)
		return -ENODEV;
	node_parent = dev->parent->of_node;
	node = dev->of_node;

	err = of_property_read_u32_array(node_parent, "reg", reg, 2);
	if (err) {
		dev_info(dev, "%s does not have 'reg' property\n", __func__);
		return -EINVAL;
	}

	if (reg[1] != SPMI_USID) {
		dev_info(dev, "%s node contains unsupported 'reg' entry\n", __func__);
		return -EINVAL;
	}

	if ((reg[0] >= SPMI_MAX_SLAVE_ID) || (reg[0] <= 0)) {
		dev_info(dev, "%s invalid usid=%d\n", __func__, reg[0]);
		return -EINVAL;
	}

	data->usid = (u8) reg[0];

	err = of_property_read_u32_array(node, "cid-addr", cid_addr, ARRAY_SIZE(cid_addr));
	if (err) {
		dev_info(dev, "%s slvid 0x%x no cid/cid_addr found\n", __func__, data->usid);
	} else {
		data->cid = (u8) cid_addr[0];
		data->cid_addr_h = cid_addr[1];
		data->cid_addr_l = cid_addr[2];
	}

	err = of_property_read_u32(node, "spmi-glitch-sta-sel", &data->spmi_glitch_sta_sel);
	if (err)
		dev_info(dev, "%s slvid 0x%x no spmi-glitch-sta-sel found\n", __func__, data->usid);

	err = of_property_read_u32(node, "spmi-glitch-sts", &data->spmi_glitch_sts0);
	if (err)
		dev_info(dev, "%s slvid 0x%x no spmi-glitch-sts found\n", __func__, data->usid);

	err = of_property_read_u32_array(node, "spmi-debug-out-l", debug_out_l, ARRAY_SIZE(debug_out_l));
	if (err)
		dev_info(dev, "%s slvid 0x%x no spmi-debug-out-l found\n", __func__, data->usid);
	else {
		data->spmi_debug_out_l = debug_out_l[0];
		data->spmi_debug_out_l_mask = debug_out_l[1];
		data->spmi_debug_out_l_shift = debug_out_l[2];
	}
	return data->usid;
}

static int mtk_spmi_pmic_debug_probe(struct platform_device *pdev)
{
	struct mtk_spmi_pmic_debug_data *data;
	int ret = 0, usid = 0;

	data = devm_kzalloc(&pdev->dev, sizeof(struct mtk_spmi_pmic_debug_data),
			    GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	mutex_init(&data->lock);
	mutex_init(&dump_mutex);
	data->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!data->regmap)
		return -ENODEV;

	platform_set_drvdata(pdev, data);

	/* Create sysfs entry */
	ret = device_create_file(&pdev->dev, &dev_attr_pmic_access);
	if (ret < 0)
		dev_info(&pdev->dev, "failed to create sysfs file\n");
	usid = mtk_spmi_debug_parse_dt(&pdev->dev, data);
	if (usid >= 0) {
		mtk_spmi_pmic_debug[usid] = data;
		dev_info(&pdev->dev, "success to create %s slave-%d sysfs file\n", pdev->name, usid);
	} else
		dev_info(&pdev->dev, "fail to create %s slave-%d sysfs file\n", pdev->name, usid);

	return ret;
}

static int mtk_spmi_pmic_debug_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_pmic_access);

	return 0;
}

static const struct of_device_id mtk_spmi_pmic_debug_of_match[] = {
	{ .compatible = "mediatek,spmi-pmic-debug", },
	{ }
};
MODULE_DEVICE_TABLE(of, mtk_spmi_pmic_debug_of_match);

static struct platform_driver mtk_spmi_pmic_debug_driver = {
	.driver = {
		.name = "mtk-spmi-pmic-debug",
		.of_match_table = mtk_spmi_pmic_debug_of_match,
	},
	.probe = mtk_spmi_pmic_debug_probe,
	.remove = mtk_spmi_pmic_debug_remove,
};
module_platform_driver(mtk_spmi_pmic_debug_driver);

MODULE_AUTHOR("Wen Su <wen.su@mediatek.com>");
MODULE_AUTHOR("Jeter Chen <jeter.chen@mediatek.com>");
MODULE_DESCRIPTION("Debug driver for MediaTek SPMI PMIC");
MODULE_LICENSE("GPL v2");
