// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2024 MediaTek Inc.

#include <linux/bitfield.h>
#include <linux/irqflags.h>
#include <linux/module.h>
#include <linux/mfd/mt6661/registers.h>
#include <linux/mfd/mt6667/registers.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>
#include <linux/spmi.h>
#include <dt-bindings/spmi/spmi.h>
#include "mtk-spmi-pmic-debug.h"

#define MT6316_SWCID_L_E4_CODE		(0x30)
#define PARITY_ERROR_CLR_RDY		0
/* For mt6688 parity error count */
#define MT6688_REG_TOP_INFO1		0x00
#define MT6688_REG_SPMI_DEBUG_SEL	0x21A
#define MT6688_MASK_VENID		GENMASK(7, 4)
#define MT6688_MASK_REVISION		GENMASK(3, 0)
#define MT6688_MASK_RG_PARITY_ERR_CLR	BIT(3)
#define MT6688_MASK_RG_SPMI_DBGMUX_SEL	GENMASK(5, 0)
#define MT6688_VENDOR_ID		0x80
#define MT6688_PARITY_ERROR_TYPE_4	0x38
#define MT6688_CHIP_REV_E1		0
#define MT6688_CHIP_REV_E2		2
#define MT6688_CHIP_REV_E3		3

#define MTK_SPMI_DBG			0
#ifdef BUF_SIZE
#undef BUF_SIZE
#endif
#define BUF_SIZE			15

struct mutex dump_mutex;

struct pmic_pre_ot_info {
	unsigned int enable;
	unsigned int cnt_reg;
	unsigned int cnt_clr_reg;
	unsigned int cnt_clr_mask;
	unsigned int cnt_clr_shift;
};

struct pmic_pre_lvsys_info {
	unsigned int enable;
	unsigned int cnt_reg;
	unsigned int cnt_clr_reg;
	unsigned int cnt_clr_mask;
	unsigned int cnt_clr_shift;
};

struct pmic_curr_clamping_info {
	unsigned int enable;
	unsigned int cnt_reg[PMIC_CURR_CLAMPING_CNT_NUM];
	unsigned int cnt_clr_reg;
	unsigned int cnt_clr_mask;
	unsigned int cnt_clr_shift;
};

struct spmi_pmic_debug_rg_info {
	unsigned int enable;
	unsigned int dbg_reg_addr[SPMI_PMIC_DEBUG_RG_NUM];
	unsigned int dbg_reg_mask[SPMI_PMIC_DEBUG_RG_NUM];
	unsigned int dbg_reg_shift[SPMI_PMIC_DEBUG_RG_NUM];
};


struct spmi_pmic_dump_rg_info {
	raw_spinlock_t spin_lock;
	unsigned int npkt_cclp_err;
	unsigned int npkt_cclp_clr;
	unsigned int npkt_cclp_clr_en;
};

struct mtk_spmi_pmic_debug_data {
	struct device *dev;
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
	unsigned int spmi_parity_err_clr;
	unsigned int spmi_parity_err_sts;
	unsigned int spmi_parity_err_cnt;
	struct pmic_pre_ot_info pre_ot_info;
	struct pmic_pre_lvsys_info pre_lvsys_info;
	struct pmic_curr_clamping_info curr_clamping_info;
	struct spmi_pmic_debug_rg_info debug_rg_info;
	struct spmi_pmic_dump_rg_info dump_rg_info;
};

static struct mtk_spmi_pmic_debug_data *mtk_spmi_pmic_debug[SPMI_MAX_SLAVE_ID];

static ssize_t pmic_access_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
#if IS_ENABLED(CONFIG_MTK_PMIC_ADB_EN)
	struct mtk_spmi_pmic_debug_data *data = dev_get_drvdata(dev);

	dev_info(dev, "0x%x\n", data->reg_value);
	return snprintf(buf, BUF_SIZE, "0x%x\n", data->reg_value);
#else
	return snprintf(buf, BUF_SIZE, "\n");
#endif /* IS_ENABLED(CONFIG_MTK_PMIC_ADB_EN) */
}
#undef BUF_SIZE

static ssize_t pmic_access_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t size)
{
#if IS_ENABLED(CONFIG_MTK_PMIC_ADB_EN)
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
#endif /* IS_ENABLED(CONFIG_MTK_PMIC_ADB_EN) */
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
		regmap = mtk_spmi_pmic_debug[i]->regmap;
		if (mtk_spmi_pmic_debug[i]->cid == 0x16) {
			regmap_read(regmap, mtk_spmi_pmic_debug[i]->cid_addr_l, &reg_val);
			pr_info("%s slvid 0x%x MT6316 %s cid_l 0x%x\n", __func__, i,
				reg_val >= MT6316_SWCID_L_E4_CODE ? "E4" : "E3", reg_val);
		}
		if ((mtk_spmi_pmic_debug[i]->cid == 0x61) || (mtk_spmi_pmic_debug[i]->cid == 0x67)) {

			/* dump glitch status */
			regmap_read(regmap, mtk_spmi_pmic_debug[i]->spmi_glitch_sts0, &glitch_sta);
			/* pr_info("%s glitch status: slvid 0x%x 0x%x\n", __func__, i, glitch_sta); */

			/* dump rising/falling edge deglitch counter */
			reg_val = 0x1;
			regmap_update_bits(regmap,
				mtk_spmi_pmic_debug[i]->spmi_glitch_sta_sel,
				(mtk_spmi_pmic_debug[i]->spmi_debug_out_l_mask <<
				mtk_spmi_pmic_debug[i]->spmi_debug_out_l_shift), reg_val);
			regmap_bulk_read(regmap, mtk_spmi_pmic_debug[i]->spmi_debug_out_l,
				(void *)&sck_cnt, 2);

			reg_val = 0x9;
			regmap_update_bits(regmap,
				mtk_spmi_pmic_debug[i]->spmi_glitch_sta_sel,
				(mtk_spmi_pmic_debug[i]->spmi_debug_out_l_mask <<
				mtk_spmi_pmic_debug[i]->spmi_debug_out_l_shift), reg_val);
			regmap_bulk_read(regmap, mtk_spmi_pmic_debug[i]->spmi_debug_out_l,
				(void *)&sck_degitch_cnt, 2);

			reg_val = 0xd;
			regmap_update_bits(regmap,
				mtk_spmi_pmic_debug[i]->spmi_glitch_sta_sel,
				(mtk_spmi_pmic_debug[i]->spmi_debug_out_l_mask <<
				mtk_spmi_pmic_debug[i]->spmi_debug_out_l_shift), reg_val);
			regmap_bulk_read(regmap, mtk_spmi_pmic_debug[i]->spmi_debug_out_l,
				(void *)&sda_cnt, 2);

			reg_val = 0x5;
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

static bool mtk_spmi_pmic_is_mt6688(struct mtk_spmi_pmic_debug_data *data, int *rev)
{
	static bool is_mt6688;
	static int last_rev_val;
	int ret;
	u32 val;

	if (!is_mt6688 && data->cid == 0x88 && data->cid_addr_h == 0x66 &&
	    data->cid_addr_l == 0x88 && strstr(dev_name(data->dev), "mtk-spmi-pmic-debug")) {
		ret = regmap_read(data->regmap, MT6688_REG_TOP_INFO1, &val);
		if (ret)
			return false;

		if ((val & MT6688_MASK_VENID) != MT6688_VENDOR_ID)
			return false;

		last_rev_val = FIELD_GET(MT6688_MASK_REVISION, val);
		is_mt6688 = true;
	}

	if (is_mt6688 && rev)
		*rev = last_rev_val;

	return is_mt6688;
}

#if PARITY_ERROR_CLR_RDY
void mtk_spmi_pmic_clr_parity_err_sts_cnt(void)
{
	struct regmap *regmap;

	unsigned int i, clr_addr;
	struct device *dev;
	int ret;

	mutex_lock(&dump_mutex);
	for (i = 2; i <= 15; i++) {
		if (mtk_spmi_pmic_debug[i] == NULL)
			continue;

		regmap = mtk_spmi_pmic_debug[i]->regmap;
		clr_addr = mtk_spmi_pmic_debug[i]->spmi_parity_err_clr;
		dev = mtk_spmi_pmic_debug[i]->dev;
		if (mtk_spmi_pmic_debug[i]->spmi_parity_err_cnt >= 0xff) {
			if ((mtk_spmi_pmic_debug[i]->cid == 0x61)
			     || (mtk_spmi_pmic_debug[i]->cid == 0x67)) {
				regmap_write(regmap, clr_addr, 1);
				regmap_write(regmap, clr_addr, 0);
			} else if (mtk_spmi_pmic_is_mt6688(mtk_spmi_pmic_debug[i], NULL)) {
				ret = regmap_write_bits(regmap, clr_addr, MT6688_MASK_RG_PARITY_ERR_CLR,
							MT6688_MASK_RG_PARITY_ERR_CLR);
				if (ret)
					dev_info(dev, "Failed to set mt6688 parity err clr to 1\n");

				ret = regmap_write_bits(regmap, clr_addr, MT6688_MASK_RG_PARITY_ERR_CLR, 0);
				if (ret)
					dev_info(dev, "Failed to set mt6688 parity err clr to 0\n");
			}
		}
	}
	mutex_unlock(&dump_mutex);
}
EXPORT_SYMBOL_GPL(mtk_spmi_pmic_clr_parity_err_sts_cnt);
#endif

void mtk_spmi_pmic_get_parity_err_cnt(u16 *buf)
{
	struct regmap *regmap;

	unsigned int i, parity_err_sta = 0, parity_err_cnt = 0, sts_addr, cnt_addr;
	u16 parity_err_cnt_array[spmi_parity_err_idx_cnt] = {0};
	struct device *dev;
	int ret, rev;

	parity_err_cnt_array[0] = 1; //temp hardcode version
	mutex_lock(&dump_mutex);
	for (i = 2; i <= 15; i++) {
		if (mtk_spmi_pmic_debug[i] == NULL)
			continue;
		dev = mtk_spmi_pmic_debug[i]->dev;
		regmap = mtk_spmi_pmic_debug[i]->regmap;
		sts_addr = mtk_spmi_pmic_debug[i]->spmi_parity_err_sts;
		cnt_addr = mtk_spmi_pmic_debug[i]->spmi_parity_err_cnt;

		if ((mtk_spmi_pmic_debug[i]->cid == 0x61)
		     || (mtk_spmi_pmic_debug[i]->cid == 0x67)) {
			/* dump parity err status */
			regmap_read(regmap, sts_addr, &parity_err_sta);

			/* dump parity err counter */
			regmap_read(regmap, cnt_addr, &parity_err_cnt);

			parity_err_cnt_array[2*i+1] = parity_err_sta;
			parity_err_cnt_array[2*i+2] = parity_err_cnt;
		} else if (mtk_spmi_pmic_is_mt6688(mtk_spmi_pmic_debug[i], &rev)) {
			ret = regmap_update_bits(regmap, MT6688_REG_SPMI_DEBUG_SEL,
						 MT6688_MASK_RG_SPMI_DBGMUX_SEL,
						 MT6688_PARITY_ERROR_TYPE_4);
			if (ret) {
				dev_info(dev, "Failed to select mt6688 dbgmux to parity error type 4\n");
				continue;
			}
			if (rev == MT6688_CHIP_REV_E3) {
				/* For E3 read parity err status only! */
				ret = regmap_read(regmap, sts_addr, &parity_err_sta);
				if (ret)
					dev_info(dev, "Failed to read mt6688 parity err status\n");
			}
			ret = regmap_read(regmap, cnt_addr, &parity_err_cnt);
			if (ret)
				dev_info(dev, "Failed to read mt6688 parity err cnt\n");

			parity_err_cnt_array[2*i+1] = parity_err_sta;
			parity_err_cnt_array[2*i+2] = parity_err_cnt;
		}
	}

	if (buf != NULL)
		memcpy(buf, parity_err_cnt_array, spmi_parity_err_idx_cnt*sizeof(u16));
	else {
		for (i = 1; i < spmi_parity_err_idx_cnt; i+=2) {
			pr_info("%s SLVID(0x%x): parity_err_sta 0x%x, parity_err_cnt 0x%x",
				__func__, (i-1)/2, parity_err_cnt_array[i], parity_err_cnt_array[i+1]);
		}
	}
	mutex_unlock(&dump_mutex);

	/* hardware issue: clear RG will clear glitch and parity */
	// mtk_spmi_pmic_clr_parity_err_sts_cnt();
}
EXPORT_SYMBOL_GPL(mtk_spmi_pmic_get_parity_err_cnt);

void mtk_spmi_pmic_get_crc_err_cnt(u16 *buf)
{
	struct regmap *regmap;

	unsigned int i, parity_err_sta = 0, parity_err_cnt = 0, sts_addr, cnt_addr;
	u16 crc_err_cnt_array[spmi_crc_err_idx_cnt] = {0};
	struct device *dev;
	int ret, rev;

	crc_err_cnt_array[0] = 1; //temp hardcode version
	mutex_lock(&dump_mutex);
	for (i = 2; i <= 15; i++) {
		if (mtk_spmi_pmic_debug[i] == NULL)
			continue;
		dev = mtk_spmi_pmic_debug[i]->dev;
		regmap = mtk_spmi_pmic_debug[i]->regmap;
		sts_addr = mtk_spmi_pmic_debug[i]->spmi_parity_err_sts;
		cnt_addr = mtk_spmi_pmic_debug[i]->spmi_parity_err_cnt;

		if ((mtk_spmi_pmic_debug[i]->cid == 0x61)
		     || (mtk_spmi_pmic_debug[i]->cid == 0x67)) {
			/* dump parity err status */
			regmap_read(regmap, sts_addr, &parity_err_sta);

			/* dump parity err counter */
			regmap_read(regmap, cnt_addr, &parity_err_cnt);

			crc_err_cnt_array[2*i+1] = parity_err_sta;
			crc_err_cnt_array[2*i+2] = parity_err_cnt;
		} else if (mtk_spmi_pmic_is_mt6688(mtk_spmi_pmic_debug[i], &rev)) {
			ret = regmap_update_bits(regmap, MT6688_REG_SPMI_DEBUG_SEL,
						 MT6688_MASK_RG_SPMI_DBGMUX_SEL,
						 MT6688_PARITY_ERROR_TYPE_4);
			if (ret) {
				dev_info(dev, "Failed to select mt6688 dbgmux to parity error type 4\n");
				continue;
			}
			if (rev == MT6688_CHIP_REV_E3) {
				/* For E3 read parity err status only! */
				ret = regmap_read(regmap, sts_addr, &parity_err_sta);
				if (ret)
					dev_info(dev, "Failed to read mt6688 parity err status\n");
			}
			ret = regmap_read(regmap, cnt_addr, &parity_err_cnt);
			if (ret)
				dev_info(dev, "Failed to read mt6688 parity err cnt\n");

			crc_err_cnt_array[2*i+1] = parity_err_sta;
			crc_err_cnt_array[2*i+2] = parity_err_cnt;
		}
	}

	if (buf != NULL)
		memcpy(buf, crc_err_cnt_array, spmi_crc_err_idx_cnt*sizeof(u16));
	else {
		for (i = 1; i < spmi_parity_err_idx_cnt; i+=2) {
			pr_info("%s SLVID(0x%x): parity_err_sta 0x%x, crc_err_cnt 0x%x",
				__func__, (i-1)/2, crc_err_cnt_array[i], crc_err_cnt_array[i+1]);
		}
	}
	mutex_unlock(&dump_mutex);

	/* hardware issue: clear RG will clear glitch and parity */
	// mtk_spmi_pmic_clr_parity_err_sts_cnt();
}
EXPORT_SYMBOL_GPL(mtk_spmi_pmic_get_crc_err_cnt);

void mtk_spmi_pmic_get_pre_ot_cnt(u16 *buf)
{
	struct regmap *regmap;
	struct pmic_pre_ot_info info;
	unsigned int val = 0, i = 0, j = 0;
	u16 pre_ot_cnt[PMIC_PRE_OT_BUF_SIZE] = {0};

	/* idx 0 for mbrain version info */
	pre_ot_cnt[PMIC_MBRAIN_VER_INFO_IDX] = 1;
	mutex_lock(&dump_mutex);
	for (i = SPMI_MIN_SLAVE_ID; i < SPMI_MAX_SLAVE_ID; i++) {
		if (!mtk_spmi_pmic_debug[i])
			continue;
		info = mtk_spmi_pmic_debug[i]->pre_ot_info;
		if (info.enable) {
			regmap = mtk_spmi_pmic_debug[i]->regmap;
			/* read pre-ot count */
			regmap_read(regmap, info.cnt_reg, &val);
			pre_ot_cnt[PMIC_PRE_OT_CNT_NUM*i+1] = val;
#if MTK_SPMI_DBG
			pre_ot_cnt[PMIC_PRE_OT_CNT_NUM*i+1] = 10;
			pr_info("slvid: 0x%x, pre_ot_cnt: %d\n",
				 i, pre_ot_cnt[PMIC_PRE_OT_CNT_NUM*i+1]);
#endif
			/* clear pre-ot count */
			/*
			regmap_update_bits(regmap,
					   info.cnt_clr_reg,
					   info.cnt_clr_mask << info.cnt_clr_shift, 1);
			regmap_update_bits(regmap,
					   info.cnt_clr_reg,
					   info.cnt_clr_mask << info.cnt_clr_shift, 0);
			*/
		}
	}
	if (buf != NULL)
		memcpy(buf, pre_ot_cnt, PMIC_PRE_OT_BUF_SIZE * sizeof(u16));
	else {
		for (i = PMIC_MBRAIN_VER_INFO_IDX + 1;
		     i < PMIC_PRE_OT_BUF_SIZE; i += PMIC_PRE_OT_CNT_NUM) {
			pr_info("\n%s SLVID(0x%x), pre_ot_cnt: ",
				__func__, (i-1) / PMIC_PRE_OT_CNT_NUM);
			for (j = i; j < i + PMIC_PRE_OT_CNT_NUM; j++)
				pr_info("%d ", pre_ot_cnt[j]);
		}
	}
	mutex_unlock(&dump_mutex);
}
EXPORT_SYMBOL_GPL(mtk_spmi_pmic_get_pre_ot_cnt);

void mtk_spmi_pmic_get_pre_lvsys_cnt(u16 *buf)
{
	struct regmap *regmap;
	struct pmic_pre_lvsys_info info;
	unsigned int val = 0, i = 0, j = 0;
	u16 pre_lvsys_cnt[PMIC_PRE_LVSYS_BUF_SIZE] = {0};

	/* idx 0 for version info */
	pre_lvsys_cnt[PMIC_MBRAIN_VER_INFO_IDX] = 1;
	mutex_lock(&dump_mutex);
	for (i = SPMI_MIN_SLAVE_ID; i < SPMI_MAX_SLAVE_ID; i++) {
		if (!mtk_spmi_pmic_debug[i])
			continue;
		info = mtk_spmi_pmic_debug[i]->pre_lvsys_info;
		if (info.enable) {
			regmap = mtk_spmi_pmic_debug[i]->regmap;
			regmap_read(regmap, mtk_spmi_pmic_debug[i]->cid_addr_l, &val);
			if (val >= PMIC_HWCID_E2) {
				/* read pre-lvsys count */
				regmap_read(regmap, info.cnt_reg, &val);
				pre_lvsys_cnt[PMIC_PRE_LVSYS_CNT_NUM*i+1] = val;
#if MTK_SPMI_DBG
				pre_lvsys_cnt[PMIC_PRE_LVSYS_CNT_NUM*i+1] = 3;
				pr_info("slvid: 0x%x, pre_lvsys_cnt: %d\n",
					i, pre_lvsys_cnt[PMIC_PRE_LVSYS_CNT_NUM*i+1]);
#endif
				/* clear pre-lvsys count */
				/*
				 * regmap_update_bits(regmap,
				 *		info.cnt_clr_reg,
				 *		info.cnt_clr_mask << info.cnt_clr_shift, 1);
				 * regmap_update_bits(regmap,
				 *		info.cnt_clr_reg,
				 *		info.cnt_clr_mask << info.cnt_clr_shift, 0);
				 */
			}
		}
	}
	if (buf != NULL)
		memcpy(buf, pre_lvsys_cnt, PMIC_PRE_LVSYS_BUF_SIZE * sizeof(u16));
	else {
		for (i = PMIC_MBRAIN_VER_INFO_IDX + 1;
		     i < PMIC_PRE_LVSYS_BUF_SIZE; i += PMIC_PRE_LVSYS_CNT_NUM) {
			pr_info("\n%s SLVID(0x%x), pre_lvsys_cnt: ",
				__func__, (i-1) / PMIC_PRE_LVSYS_CNT_NUM);
			for (j = i; j < i + PMIC_PRE_LVSYS_CNT_NUM; j++)
				pr_info("%d ", pre_lvsys_cnt[j]);
		}
	}
	mutex_unlock(&dump_mutex);
}
EXPORT_SYMBOL_GPL(mtk_spmi_pmic_get_pre_lvsys_cnt);

void mtk_spmi_pmic_get_current_clamping_cnt(u16 *buf)
{
	struct regmap *regmap;
	struct pmic_curr_clamping_info info;
	unsigned int val = 0, i = 0, j = 0;
	u16 curr_clamping_cnt[PMIC_CURR_CLAMPING_BUF_SIZE] = {0};

	/* idx 0 for version info */
	curr_clamping_cnt[PMIC_MBRAIN_VER_INFO_IDX] = 1;
	mutex_lock(&dump_mutex);
	for (i = SPMI_MIN_SLAVE_ID; i < SPMI_MAX_SLAVE_ID; i++) {
		if (!mtk_spmi_pmic_debug[i])
			continue;
		info = mtk_spmi_pmic_debug[i]->curr_clamping_info;
		if (info.enable) {
			regmap = mtk_spmi_pmic_debug[i]->regmap;
			/* read current clamping count */
			for (j = 0; j < PMIC_CURR_CLAMPING_CNT_NUM; j++) {
				regmap_read(regmap, info.cnt_reg[j], &val);
				curr_clamping_cnt[PMIC_CURR_CLAMPING_CNT_NUM*i+j+1] = val;
#if MTK_SPMI_DBG
				curr_clamping_cnt[PMIC_CURR_CLAMPING_CNT_NUM*i+j+1] = j;
				pr_info("slvid: 0x%x, j: %d, index: %d, curr_clamping_cnt: %d\n",
					 i, j, PMIC_CURR_CLAMPING_CNT_NUM*i+j+1,
					 curr_clamping_cnt[PMIC_CURR_CLAMPING_CNT_NUM*i+j+1]);
#endif
			}
			/* clear current clamping count */
			/*
			regmap_update_bits(regmap,
					   info.cnt_clr_reg,
					   info.cnt_clr_mask << info.cnt_clr_shift, 1);
			regmap_update_bits(regmap,
					   info.cnt_clr_reg,
					   info.cnt_clr_mask << info.cnt_clr_shift, 0);
			*/
		}
	}
	if (buf != NULL)
		memcpy(buf, curr_clamping_cnt, PMIC_CURR_CLAMPING_BUF_SIZE * sizeof(u16));
	else {
		for (i = PMIC_MBRAIN_VER_INFO_IDX + 1;
		     i < PMIC_CURR_CLAMPING_BUF_SIZE; i += PMIC_CURR_CLAMPING_CNT_NUM) {
			pr_info("\n%s SLVID(0x%x), curr_clamping_cnt: ",
				__func__, (i-1) / PMIC_CURR_CLAMPING_CNT_NUM);
			for (j = i; j < i + PMIC_CURR_CLAMPING_CNT_NUM; j++)
				pr_info("%d ", curr_clamping_cnt[j]);
		}
	}
	mutex_unlock(&dump_mutex);
}
EXPORT_SYMBOL_GPL(mtk_spmi_pmic_get_current_clamping_cnt);

void mtk_spmi_pmic_get_debug_rg_info(unsigned int *buf)
{
	struct regmap *regmap;
	struct spmi_pmic_debug_rg_info info;
	int ret = 0;
	unsigned int i = 0, j = 0, val = 0, debug_rg_addr_data[SPMI_PMIC_DEBUG_RG_BUF_SIZE] = {0};
	unsigned char mask = 0;
	const char mt6661_spmi_pmic_dbg_rgs[][30] = MT6661_SPMI_PMIC_DBG_RGS;
	const char mt6667_spmi_pmic_dbg_rgs[][30] = MT6667_SPMI_PMIC_DBG_RGS;

	/* idx 0 for version info */
	debug_rg_addr_data[PMIC_MBRAIN_VER_INFO_IDX] = 1;
	mutex_lock(&dump_mutex);
	for (i = SPMI_MIN_SLAVE_ID; i < SPMI_MAX_SLAVE_ID; i++) {
		if (!mtk_spmi_pmic_debug[i])
			continue;
		info = mtk_spmi_pmic_debug[i]->debug_rg_info;
		if (info.enable) {
			regmap = mtk_spmi_pmic_debug[i]->regmap;
			if (buf == NULL)
				pr_info("\n%s SLVID(0x%x), debug_rg_addr_data:\n",
					__func__, i);
			/* read spmi pmic debug rg data */
			for (j = 0; j < SPMI_PMIC_DEBUG_RG_NUM; j++) {
				ret = regmap_read(regmap, info.dbg_reg_addr[j], &val);
				if (ret)
					pr_info("regmap read error: addr = 0x%x\n",
						info.dbg_reg_addr[j]);
				debug_rg_addr_data[SPMI_PMIC_DEBUG_RG_NUM*i+j+1] |= (info.dbg_reg_addr[j] << 16);
				mask = (info.dbg_reg_mask[j] << info.dbg_reg_shift[j]);
				debug_rg_addr_data[SPMI_PMIC_DEBUG_RG_NUM*i+j+1] |= (mask << 8);
				debug_rg_addr_data[SPMI_PMIC_DEBUG_RG_NUM*i+j+1] |= val;
				if (buf == NULL) {
					if (mtk_spmi_pmic_debug[i]->cid == 0x61)
						pr_info("RG: %s, addr: 0x%x, mask:0x%x, data:0x%x\n",
							mt6661_spmi_pmic_dbg_rgs[j],
							info.dbg_reg_addr[j],
							mask,
							val);
					else if (mtk_spmi_pmic_debug[i]->cid == 0x67)
						pr_info("RG: %s, addr: 0x%x, mask:0x%x, data:0x%x\n",
							mt6667_spmi_pmic_dbg_rgs[j],
							info.dbg_reg_addr[j],
							mask,
							val);
				}
			}
		}
	}
	if (buf != NULL)
		memcpy(buf, debug_rg_addr_data, SPMI_PMIC_DEBUG_RG_BUF_SIZE * sizeof(unsigned int));
	mutex_unlock(&dump_mutex);
}
EXPORT_SYMBOL_GPL(mtk_spmi_pmic_get_debug_rg_info);

int mtk_spmi_pmic_dump_rg_data(u8 slvid, u32 *rdata, enum dump_rg rg_name)
{
	struct regmap *regmap;
	struct spmi_pmic_dump_rg_info *info;
	int ret = 0;
	unsigned long flags = 0;
	unsigned int val = 0;

	if (!mtk_spmi_pmic_debug[slvid]) {
		pr_info("%s: slvid %d not found\n", __func__, slvid);
		return -1;
	}

	info = &mtk_spmi_pmic_debug[slvid]->dump_rg_info;
	regmap = mtk_spmi_pmic_debug[slvid]->regmap;
	raw_spin_lock_irqsave(&info->spin_lock, flags);
	switch (rg_name) {
	case RGS_NPKT_CCLP_ERR:
		if (info->npkt_cclp_err == 0) {
			pr_info("%s: rg addr is not in dtsi\n", __func__);
			break;
		}
		ret |= regmap_read(regmap, info->npkt_cclp_err, &val);
		if (info->npkt_cclp_clr_en) {
			ret |= regmap_write(regmap, info->npkt_cclp_clr, 1);
			ret |= regmap_write(regmap, info->npkt_cclp_clr, 0);
		} else {
			/* clear in user, always send 1 to spmi to avoid BUG ON */
			val = 1;
		}
		if (rdata != NULL)
			*rdata = val;
		break;
	default:
		pr_info("%s: rg_name is not defined\n", __func__);
		ret |= -1;
		break;
	}
	raw_spin_unlock_irqrestore(&info->spin_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_spmi_pmic_dump_rg_data);

static int mtk_spmi_debug_parse_dt(struct device *dev, struct mtk_spmi_pmic_debug_data *data)
{
	struct device_node *node_parent, *node;
	int i = 0, err;
	u32 reg[2] = {0}, cid_addr[3] = {0}, buf[3] = {0}, cc_buf[PMIC_CURR_CLAMPING_CNT_NUM] = {0};
	u32 dbg_buf[SPMI_PMIC_DEBUG_RG_NUM] = {0};

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

	/* SPMI glitch */
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

	err = of_property_read_u32_array(node, "spmi-debug-out-l", buf, ARRAY_SIZE(buf));
	if (err)
		dev_info(dev, "%s slvid 0x%x no spmi-debug-out-l found\n", __func__, data->usid);
	else {
		data->spmi_debug_out_l = buf[0];
		data->spmi_debug_out_l_mask = buf[1];
		data->spmi_debug_out_l_shift = buf[2];
	}

	/* SPMI parity error */
	err = of_property_read_u32(node, "spmi-parity-err-clr", &data->spmi_parity_err_clr);
	if (err)
		dev_info(dev, "%s slvid 0x%x no spmi-parity-err-clr found\n", __func__, data->usid);

	err = of_property_read_u32(node, "spmi-parity-err-sts", &data->spmi_parity_err_sts);
	if (err)
		dev_info(dev, "%s slvid 0x%x no spmi-parity-err-sts found\n", __func__, data->usid);

	err = of_property_read_u32(node, "spmi-parity-err-cnt", &data->spmi_parity_err_cnt);
	if (err)
		dev_info(dev, "%s slvid 0x%x no spmi-parity-err-cnt found\n", __func__, data->usid);

	/* PMIC PRE-OT */
	err = of_property_read_u32(node, "pmic-pre-ot-cnt-enable", &data->pre_ot_info.enable);
	if (err)
		dev_info(dev, "%s slvid 0x%x does not have 'pmic-pre-ot-cnt-enable' property\n",
			 __func__, data->usid);
	if (data->pre_ot_info.enable) {
		err = of_property_read_u32(node, "pmic-pre-ot-cnt", &(data->pre_ot_info.cnt_reg));
		if (err)
			dev_info(dev, "%s slvid 0x%x does not have 'pmic-pre-ot-cnt' property\n",
				 __func__, data->usid);
		err = of_property_read_u32_array(node, "pmic-pre-ot-cnt-clr", buf, ARRAY_SIZE(buf));
		if (err)
			dev_info(dev, "%s slvid 0x%x does not have 'pmic-pre-ot-cnt-clr' property\n",
				 __func__, data->usid);
		else {
			data->pre_ot_info.cnt_clr_reg = buf[0];
			data->pre_ot_info.cnt_clr_mask = buf[1];
			data->pre_ot_info.cnt_clr_shift = buf[2];
		}
	}

	/* PMIC PRE-LVSYS */
	err = of_property_read_u32(node, "pmic-pre-lvsys-cnt-enable",
				   &(data->pre_lvsys_info.enable));
	if (err)
		dev_info(dev, "%s slvid 0x%x does not have 'pmic-pre-lvsys-cnt-enable' property\n",
			 __func__, data->usid);
	if (data->pre_lvsys_info.enable) {
		err = of_property_read_u32(node, "pmic-pre-lvsys-cnt",
					   &(data->pre_lvsys_info.cnt_reg));
		if (err)
			dev_info(dev, "%s slvid 0x%x does not have 'pmic-pre-lvsys-cnt' property\n",
				 __func__, data->usid);
		err = of_property_read_u32_array(node, "pmic-pre-lvsys-cnt-clr", buf, ARRAY_SIZE(buf));
		if (err)
			dev_info(dev, "%s slvid 0x%x does not have 'pmic-pre-lvsys-cnt-clr' property\n",
				 __func__, data->usid);
		else {
			data->pre_lvsys_info.cnt_clr_reg = buf[0];
			data->pre_lvsys_info.cnt_clr_mask = buf[1];
			data->pre_lvsys_info.cnt_clr_shift = buf[2];
		}
	}

	/* Current Clamping */
	err = of_property_read_u32(node, "pmic-curr-clamping-cnt-enable",
				   &(data->curr_clamping_info.enable));
	if (err)
		dev_info(dev, "%s slvid 0x%x does not have 'pmic-curr-clamping-cnt-enable' property\n",
			 __func__, data->usid);
	if (data->curr_clamping_info.enable) {
		err = of_property_read_u32_array(node, "pmic-curr-clamping-cnt",
						 cc_buf, ARRAY_SIZE(cc_buf));
		if (err)
			dev_info(dev, "%s slvid 0x%x does not have 'pmic-curr-clamping-cnt' property\n",
				 __func__, data->usid);
		else {
			for (i = 0; i < PMIC_CURR_CLAMPING_CNT_NUM; i++)
				data->curr_clamping_info.cnt_reg[i] = cc_buf[i];
		}
		err = of_property_read_u32_array(node, "pmic-curr-clamping-cnt-clr", buf, ARRAY_SIZE(buf));
		if (err)
			dev_info(dev, "%s slvid 0x%x does not have 'pmic-curr-clamping-cnt-clr' property\n",
				 __func__, data->usid);
		else {
			data->curr_clamping_info.cnt_clr_reg = buf[0];
			data->curr_clamping_info.cnt_clr_mask = buf[1];
			data->curr_clamping_info.cnt_clr_shift = buf[2];
		}
	}

	err = of_property_read_u32(node, "spmi-pmic-debug-rg-enable",
				   &(data->debug_rg_info.enable));
	if (err)
		dev_info(dev, "%s slvid 0x%x does not have 'spmi-pmic-debug-rg-enable' property\n",
			 __func__, data->usid);
	if (data->debug_rg_info.enable) {
		err = of_property_read_u32_array(node, "spmi-pmic-debug-rg-addr",
						 dbg_buf, ARRAY_SIZE(dbg_buf));
		if (err)
			dev_info(dev, "%s slvid 0x%x does not have 'spmi-pmic-debug-rg-addr' property\n",
				 __func__, data->usid);
		else {
			for (i = 0; i < SPMI_PMIC_DEBUG_RG_NUM; i++)
				data->debug_rg_info.dbg_reg_addr[i] = dbg_buf[i];
		}
		err = of_property_read_u32_array(node, "spmi-pmic-debug-rg-mask",
						 dbg_buf, ARRAY_SIZE(dbg_buf));
		if (err)
			dev_info(dev, "%s slvid 0x%x does not have 'spmi-pmic-debug-rg-mask' property\n",
				 __func__, data->usid);
		else {
			for (i = 0; i < SPMI_PMIC_DEBUG_RG_NUM; i++)
				data->debug_rg_info.dbg_reg_mask[i] = dbg_buf[i];
		}
		err = of_property_read_u32_array(node, "spmi-pmic-debug-rg-shift",
						 dbg_buf, ARRAY_SIZE(dbg_buf));
		if (err)
			dev_info(dev, "%s slvid 0x%x does not have 'spmi-pmic-debug-rg-shift' property\n",
				 __func__, data->usid);
		else {
			for (i = 0; i < SPMI_PMIC_DEBUG_RG_NUM; i++)
				data->debug_rg_info.dbg_reg_shift[i] = dbg_buf[i];
		}
	}

	err = of_property_read_u32(node, "spmi-pmic-debug-rg-enable",
				   &(data->debug_rg_info.enable));
	if (err)
		dev_info(dev, "%s slvid 0x%x does not have 'spmi-pmic-debug-rg-enable' property\n",
			 __func__, data->usid);
	if (data->debug_rg_info.enable) {
		err = of_property_read_u32_array(node, "spmi-pmic-debug-rg-addr",
						 dbg_buf, ARRAY_SIZE(dbg_buf));
		if (err)
			dev_info(dev, "%s slvid 0x%x does not have 'spmi-pmic-debug-rg-addr' property\n",
				 __func__, data->usid);
		else {
			for (i = 0; i < SPMI_PMIC_DEBUG_RG_NUM; i++)
				data->debug_rg_info.dbg_reg_addr[i] = dbg_buf[i];
		}
		err = of_property_read_u32_array(node, "spmi-pmic-debug-rg-mask",
						 dbg_buf, ARRAY_SIZE(dbg_buf));
		if (err)
			dev_info(dev, "%s slvid 0x%x does not have 'spmi-pmic-debug-rg-mask' property\n",
				 __func__, data->usid);
		else {
			for (i = 0; i < SPMI_PMIC_DEBUG_RG_NUM; i++)
				data->debug_rg_info.dbg_reg_mask[i] = dbg_buf[i];
		}
		err = of_property_read_u32_array(node, "spmi-pmic-debug-rg-shift",
						 dbg_buf, ARRAY_SIZE(dbg_buf));
		if (err)
			dev_info(dev, "%s slvid 0x%x does not have 'spmi-pmic-debug-rg-shift' property\n",
				 __func__, data->usid);
		else {
			for (i = 0; i < SPMI_PMIC_DEBUG_RG_NUM; i++)
				data->debug_rg_info.dbg_reg_shift[i] = dbg_buf[i];
		}
	}

	/* SPMI PMIC dump rg data */
	err = of_property_read_u32(node, "rgs-npkt-cclp-err", &(data->dump_rg_info.npkt_cclp_err));
	if (err) {
		dev_info(dev, "%s slvid 0x%x does not have 'rgs-npkt-cclp-err' property\n",
			 __func__, data->usid);
		data->dump_rg_info.npkt_cclp_err = 0;
	}
	err = of_property_read_u32(node, "rgs-npkt-cclp-clr", &(data->dump_rg_info.npkt_cclp_clr));
	if (err)
		dev_info(dev, "%s slvid 0x%x does not have 'rgs-npkt-cclp-clr' property\n",
			 __func__, data->usid);
	err = of_property_read_u32(node, "rgs-npkt-cclp-clr-enable", &(data->dump_rg_info.npkt_cclp_clr_en));
	if (err)
		dev_info(dev, "%s slvid 0x%x does not have 'rgs-npkt-cclp-clr-enable' property\n",
			 __func__, data->usid);

	return data->usid;
}

static int mtk_spmi_pmic_debug_probe(struct platform_device *pdev)
{
	struct mtk_spmi_pmic_debug_data *data;
	int ret = 0, usid = 0;
#if MTK_SPMI_DBG
	int i = 0;
	u16 pre_ot_buf[PMIC_PRE_OT_BUF_SIZE] = {0};
	u16 pre_lvsys_buf[PMIC_PRE_LVSYS_BUF_SIZE] = {0};
	u16 cc_buf[PMIC_CURR_CLAMPING_BUF_SIZE] = {0};
	u32 dbg_buf[SPMI_PMIC_DEBUG_RG_BUF_SIZE] = {0};
#endif

	data = devm_kzalloc(&pdev->dev, sizeof(struct mtk_spmi_pmic_debug_data),
			    GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = &pdev->dev;
	mutex_init(&data->lock);
	mutex_init(&dump_mutex);
	raw_spin_lock_init(&data->dump_rg_info.spin_lock);
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
	pr_info("[pmic debug] usid: %d, clr enable: %d\n", usid, data->dump_rg_info.npkt_cclp_clr_en);

#if MTK_SPMI_DBG
	mtk_spmi_pmic_get_pre_ot_cnt(pre_ot_buf);
	for (i = 0; i < PMIC_PRE_OT_BUF_SIZE; i++)
		dev_info(&pdev->dev, "pre_ot_buf[%d]: %d\n", i, pre_ot_buf[i]);
	mtk_spmi_pmic_get_pre_lvsys_cnt(pre_lvsys_buf);
	for (i = 0; i < PMIC_PRE_LVSYS_BUF_SIZE; i++)
		dev_info(&pdev->dev, "pre_lvsys_buf[%d]: %d\n", i, pre_lvsys_buf[i]);
	mtk_spmi_pmic_get_current_clamping_cnt(cc_buf);
	for (i = 0; i < PMIC_CURR_CLAMPING_BUF_SIZE; i++)
		dev_info(&pdev->dev, "cc_buf[%d]: %d\n", i, cc_buf[i]);
	mtk_spmi_pmic_get_debug_rg_info(dbg_buf);
	for (i = 0; i < SPMI_PMIC_DEBUG_RG_BUF_SIZE; i++)
		dev_info(&pdev->dev, "dbg_buf[%d]: %x\n", i, dbg_buf[i]);
	pr_info("dump spmi pmic dbg rg info...\n");
	mtk_spmi_pmic_get_debug_rg_info(NULL);
#endif

	return ret;
}

static void mtk_spmi_pmic_debug_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_pmic_access);
}

static const struct of_device_id mtk_spmi_pmic_debug_of_match[] = {
	{ .compatible = "mediatek,spmi-pmic-debug", },
	{ .compatible = "mediatek,spmi-pmic-3-debug", },
	{ .compatible = "mediatek,spmi-pmic-4-debug", },
	{ .compatible = "mediatek,spmi-pmic-5-debug", },
	{ .compatible = "mediatek,spmi-pmic-6-debug", },
	{ .compatible = "mediatek,spmi-pmic-7-debug", },
	{ .compatible = "mediatek,spmi-pmic-8-debug", },
	{ .compatible = "mediatek,spmi-pmic-12-debug", },
	{ .compatible = "mediatek,spmi-pmic-13-debug", },
	{ .compatible = "mediatek,spmi-pmic-15-debug", },
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
