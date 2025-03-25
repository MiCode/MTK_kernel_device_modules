// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#include <lpm_module.h>
#include <lpm_dbg_fs_common.h>
#include <mtk_ocla_sysfs.h>

#define ocla_dbg_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)

#define SPM_OCLA_MAGIC_NUM	(1095517007U)
#define SPM_OCLA_CTRL_MASK	(0xFFU)
#define SPM_OCLA_CTRL_PARA_MASK	(0xFF00U)

enum spm_ocla_smc_ctrl_type {
	SPM_OCLA_SMC_ENABLE,
	SPM_OCLA_SMC_SIGNAL,
	SPM_OCLA_SMC_BIT_EN,
	SPM_OCLA_SMC_MONITOR,
	SPM_OCLA_SMC_USER_SEL,
};

static unsigned int ocla_dbg_enable;
static ssize_t ocla_dbg_enable_write(char *FromUserBuf, size_t sz, void *priv)
{
	unsigned int magic, enable;

	if (sscanf(FromUserBuf, "%x %x", &magic, &enable) == 2) {
		if (magic == SPM_OCLA_MAGIC_NUM) {
			if (enable <= 1) {
				ocla_dbg_enable = enable;
				return sz;
			}
		}
	}
	return -EINVAL;
}

static const struct mtk_lp_sysfs_op ocla_dbg_enable_fops = {
	.fs_write = ocla_dbg_enable_write,
};

static unsigned int ocla_enable_get(void)
{
	return lpm_smc_spm_dbg(MT_SPM_DBG_SMC_OCLA, MT_LPM_SMC_ACT_GET,
			       SPM_OCLA_SMC_ENABLE, 0);
}

static void ocla_enable_set(unsigned int enable)
{
	lpm_smc_spm_dbg(MT_SPM_DBG_SMC_OCLA, MT_LPM_SMC_ACT_SET,
			SPM_OCLA_SMC_ENABLE, enable);
}

static ssize_t ocla_enable_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	unsigned int enabled;

	if (!ocla_dbg_enable)
		return -EINVAL;

	enabled = ocla_enable_get();
	ocla_dbg_log("ocla: %s\n", enabled? "enabled": "disabled");

	return p - ToUserBuf;
}

static ssize_t ocla_enable_write(char *FromUserBuf, size_t sz, void *priv)
{
	unsigned int magic, enable;

	if (!ocla_dbg_enable)
		return -EINVAL;

	if (sscanf(FromUserBuf, "%x %x", &magic, &enable) == 2) {
		if (magic == SPM_OCLA_MAGIC_NUM) {
			if (enable <= 1) {
				ocla_enable_set(enable);
				return sz;
			}
		}
	}
	return -EINVAL;
}

static const struct mtk_lp_sysfs_op ocla_enable_fops = {
	.fs_read = ocla_enable_read,
	.fs_write = ocla_enable_write,
};

static ssize_t ocla_packet_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;

	if (!ocla_dbg_enable)
		return -EINVAL;

	ocla_dbg_log("Packet setting: 0x%lx 0x%lx 0x%lx 0x%lx\n",
			lpm_smc_spm_dbg(MT_SPM_DBG_SMC_OCLA,
				MT_LPM_SMC_ACT_GET, SPM_OCLA_SMC_SIGNAL, 0),
			lpm_smc_spm_dbg(MT_SPM_DBG_SMC_OCLA,
				MT_LPM_SMC_ACT_GET, SPM_OCLA_SMC_BIT_EN, 0),
			lpm_smc_spm_dbg(MT_SPM_DBG_SMC_OCLA,
				MT_LPM_SMC_ACT_GET, SPM_OCLA_SMC_MONITOR, 0),
			lpm_smc_spm_dbg(MT_SPM_DBG_SMC_OCLA,
				MT_LPM_SMC_ACT_GET, SPM_OCLA_SMC_MONITOR | 0x100, 0)
			);

	return p - ToUserBuf;
}

static ssize_t ocla_packet_write(char *FromUserBuf, size_t sz, void *priv)
{
	unsigned int magic, signal, enable_bit, monitor_0, monitor_1;

	if (!ocla_dbg_enable)
		return -EINVAL;

	if (sscanf(FromUserBuf, "%x %x %x %x %x",
			&magic, &signal, &enable_bit, &monitor_0, &monitor_1) == 5) {
		if (magic != SPM_OCLA_MAGIC_NUM)
			return -EINVAL;
		lpm_smc_spm_dbg(MT_SPM_DBG_SMC_OCLA, MT_LPM_SMC_ACT_SET,
						SPM_OCLA_SMC_SIGNAL, signal);
		lpm_smc_spm_dbg(MT_SPM_DBG_SMC_OCLA, MT_LPM_SMC_ACT_SET,
						SPM_OCLA_SMC_BIT_EN, enable_bit);
		lpm_smc_spm_dbg(MT_SPM_DBG_SMC_OCLA, MT_LPM_SMC_ACT_SET,
						SPM_OCLA_SMC_MONITOR, monitor_0);
		lpm_smc_spm_dbg(MT_SPM_DBG_SMC_OCLA, MT_LPM_SMC_ACT_SET,
						SPM_OCLA_SMC_MONITOR | 0x100, monitor_1);
		return sz;
	}
	return -EINVAL;
}

static const struct mtk_lp_sysfs_op ocla_packet_fops = {
	.fs_read = ocla_packet_read,
	.fs_write = ocla_packet_write,
};

static unsigned int ocla_sel_tmp;
static ssize_t ocla_usr_sel_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;

	if (!ocla_dbg_enable)
		return -EINVAL;

	if (ocla_sel_tmp) {
		ocla_dbg_log("Sel%x: %lx\n", ocla_sel_tmp,
			     lpm_smc_spm_dbg(MT_SPM_DBG_SMC_OCLA, MT_LPM_SMC_ACT_GET,
					     ocla_sel_tmp | SPM_OCLA_SMC_USER_SEL, 0));
	}
	return p - ToUserBuf;
}

static ssize_t ocla_usr_sel_write(char *FromUserBuf, size_t sz, void *priv)
{
	unsigned int magic, sel, val;

	if (!ocla_dbg_enable)
		return -EINVAL;

	if (sscanf(FromUserBuf, "%x %x %x", &magic, &sel, &val) == 3) {
		if (magic != SPM_OCLA_MAGIC_NUM)
			return -EINVAL;
		if ( !sel || sel & ~SPM_OCLA_CTRL_PARA_MASK)
			return -EINVAL;
		ocla_sel_tmp = sel;
		lpm_smc_spm_dbg(MT_SPM_DBG_SMC_OCLA, MT_LPM_SMC_ACT_SET,
				ocla_sel_tmp | SPM_OCLA_SMC_USER_SEL, val);
	}
	return sz;
}

static const struct mtk_lp_sysfs_op ocla_usr_sel_fops = {
	.fs_read = ocla_usr_sel_read,
	.fs_write = ocla_usr_sel_write,
};

static void *ocla_sram_base;
static size_t ocla_sram_size;
static ssize_t ocla_sram_dump(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	unsigned int enabled;
	size_t cpy_size;

	enabled = ocla_enable_get();
	cpy_size = min(sz, ocla_sram_size);

	if (enabled)
		ocla_enable_set(0);

	memcpy(p, ocla_sram_base, cpy_size);
	p += cpy_size;

	if (enabled)
		ocla_enable_set(enabled);

	return p - ToUserBuf;
}

static const struct mtk_lp_sysfs_op ocla_sram_dump_fops = {
	.fs_read = ocla_sram_dump,
};

void lpm_ocla_fs_init(void)
{
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,ocla-sram");

	if (node) {
		struct resource res;

		if (!of_address_to_resource(node, 0, &res)) {
			ocla_sram_size = resource_size(&res);
			ocla_sram_base = of_iomap(node, 0);
			if (!ocla_sram_base) {
				pr_info("[name:mtk_lpm][P] - Failed to map ocla sram (%s:%d)\n",
					__func__, __LINE__);
				return;
			}
		}
		of_node_put(node);
	} else {
		pr_info("[name:mtk_lpm][P] - Failed to get ocla node (%s:%d)\n",
			__func__, __LINE__);
		return;
	}
	mtk_ocla_sysfs_root_entry_create();
	mtk_ocla_sysfs_entry_node_add("ocla_dbg_enable", 0200,
					&ocla_dbg_enable_fops, NULL);
	mtk_ocla_sysfs_entry_node_add("ocla_enable", 0644,
					&ocla_enable_fops, NULL);
	mtk_ocla_sysfs_entry_node_add("ocla_sram_dump", 0444,
					&ocla_sram_dump_fops, NULL);
	mtk_ocla_sysfs_entry_node_add("ocla_packet", 0644,
					&ocla_packet_fops, NULL);
	mtk_ocla_sysfs_entry_node_add("ocla_user_sel", 0644,
					&ocla_usr_sel_fops, NULL);
}
EXPORT_SYMBOL(lpm_ocla_fs_init);

int lpm_ocla_fs_deinit(void)
{
	return 0;
}
EXPORT_SYMBOL(lpm_ocla_fs_deinit);

