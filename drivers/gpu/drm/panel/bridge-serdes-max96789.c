// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/module.h>
#include <drm/drm_bridge.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <linux/gpio/consumer.h>
#include "bridge-serdes-max96789.h"

#define PANEL_NODE_NAME							"panel"
#define SER_NODE_NAME							"ser"
#define DESA_NODE_NAME							"desa"
#define DESB_NODE_NAME							"desb"
#define DESDEF_NODE_NAME						"desdef"
#define SETTING_NODE_NAME						"setting"

#define SER_INIT_CMD_NODE_NAME					"ser-init-cmd"
#define SER_TIMING_CMD_NODE_NAME				"ser-timing-cmd"
#define SERDES_DUAL_SETTING_CMD_NODE_NAME		"serdes-dual-setting-cmd"
#define SER_LUT_CMD_NODE_NAME					"ser-lut-cmd"
#define DES_INIT_CMD_NODE_NAME					"des-init-cmd"
#define BL_ON_CMD_NODE_NAME						"bl-on-cmd"
#define BL_OFF_CMD_NODE_NAME					"bl-off-cmd"

#define PANEL_MODE_NODE_NAME					"panel-mode-setting"
#define PANEL_WIDTH_NODE_NAME					"panel-mode-width"
#define PANEL_HEIGHT_NODE_NAME					"panel-mode-height"
#define PANEL_HFP_NODE_NAME						"panel-mode-hfp"
#define PANEL_HSA_NODE_NAME						"panel-mode-hsa"
#define PANEL_HBP_NODE_NAME						"panel-mode-hbp"
#define PANEL_VSA_NODE_NAME						"panel-mode-vsa"
#define PANEL_VBP_NODE_NAME						"panel-mode-vbp"
#define PANEL_VFP_NODE_NAME						"panel-mode-vfp"
#define PANEL_CLOCK_NODE_NAME					"panel-mode-clock"
#define PANEL_VREFRESH_NODE_NAME				"panel-mode-vrefresh"
#define PANEL_PLL_NODE_NAME						"panel-mode-pll"
#define PANEL_LPPF_NODE_NAME					"panel-mode-lppf"
#define PANEL_WIDTH_MM_NODE_NAME				"panel-mode-width-mm"
#define PANEL_HEIGHT_MM_NODE_NAME				"panel-mode-height-mm"

#define SERDES_INITED_IN_LK_NODE_NAME			"inited-in-lk"
#define SERDES_DES_I2C_ADDR_NODE_NAME			"des-i2c-addr"
#define SERDES_BL_I2C_ADDR_NODE_NAME			"bl-i2c-addr"
#define SERDES_SUPER_FRAME_NODE_NAME			"ser-super-frame"
#define DES_COMPATIBLE_CMD_NODE_NAME			"comp-cmd"
#define DES_COMPATIBLE_EXP_NODE_NAME			"comp-exp"
#define DES_COMPATIBLE_SETTING_NODE_NAME		"comp-setting"

#define MAX_COMPATIBLE_NUM						8
enum SLAVE_TYPE {
	SER,
	DEF_DES,
	DESA, DESB
};

struct serdes_dual_cmd {
	enum SLAVE_TYPE slave;
	u16 addr;
	u8  data;
	u8  delay_ms;
};

struct serdes_cmd {
	u16 addr;
	u8 data;
	u8 delay_ms;
};

struct bl_cmd {
	u16 len;
	u8  data[32];
};

struct comp_setting {
	u16 mask;
	u16 exp_data;
	struct device_node  *setting_node;
};

struct comp_cmd {
	u8 rw;
	u8 addr;
	u8 len;
	u8  data[32];
};

struct deserializer {
	struct device_node *des_node;
	struct i2c_client *des_client;
	struct i2c_client *bl_client;

	u8 des_iic_addr;
	u8 bl_iic_addr;

	u32 des_init_cmd_num;
	struct serdes_cmd *des_init_cmd;
	u32 bl_on_cmd_num;
	struct bl_cmd *bl_on_cmd;
	u32 bl_off_cmd_num;
	struct bl_cmd *bl_off_cmd;

	struct vdo_timing disp_mode;
};

struct serdes {
	struct device_node *setting_node;
	struct gpio_desc *reset_gpio;

	bool super_frame;
	bool inited_in_lk;
	u32 ser_init_cmd_num;
	struct serdes_cmd *ser_init_cmd;
	u32 ser_timing_cmd_num;
	struct serdes_cmd *ser_timing_cmd;
	u32 ser_lut_cmd_num;
	struct serdes_cmd *ser_lut_cmd;
	u32 serdes_dual_setting_cmd_num;
	struct serdes_dual_cmd *serdes_dual_setting_cmd;
	u32 comp_setting_cmd_num;
	struct comp_cmd comp_setting_cmd[MAX_COMPATIBLE_NUM];
	u32 comp_set_num;
	struct comp_setting cmp_set[MAX_COMPATIBLE_NUM];

	struct deserializer *desa;
	struct deserializer *desb;
	struct deserializer *desdef;

	struct drm_bridge bridge;
	struct device *dev;
	struct i2c_client *ser_client;
	bool enabled;
	bool pre_enabled;
};

static DEFINE_MUTEX(i2c_access);

static inline struct serdes *
		bridge_to_serdes(struct drm_bridge *bridge)
{
	return container_of(bridge, struct serdes, bridge);
}
static int i2c_write_byte(struct i2c_client *i2c, u16 reg_addr, u8 val)
{
	int ret = 0;
	u8 buf[3];

	buf[0] = reg_addr >> 8;
	buf[1] = reg_addr & 0xFF;
	buf[2] = val;

	pr_info("serdes: i2c%d 0x%x write 0x%x/0x%x\n", i2c->adapter->nr, i2c->addr, reg_addr, val);

	mutex_lock(&i2c_access);
	ret = i2c_master_send(i2c, buf, 3);
	mutex_unlock(&i2c_access);

	if (ret < 0) {
		pr_info("ser i2c send Fail: 0x%x/0x%x/%d\n", reg_addr, val, ret);
		return ret;
	}

	return 0;
}

static int serdes_get_des_iic_addr_from_dts(struct device_node *node)
{
	int ret;
	u32 read_value;

	if (!node)
		return -1;

	ret = of_property_read_u32(node, SERDES_DES_I2C_ADDR_NODE_NAME, &read_value);
	if (ret) {
		pr_info("error: read des iic addr error!\n");
		return -1;
	}

	return read_value;
}
static int serdes_get_bl_iic_addr_from_dts(struct device_node *node)
{
	int ret;
	u32 read_value;

	if (!node)
		return -1;

	ret = of_property_read_u32(node, SERDES_BL_I2C_ADDR_NODE_NAME, &read_value);
	if (ret) {
		pr_info("error: read eeprom iic addr error!\n");
		return -1;
	}

	return read_value;
}

static int serdes_get_inited_flag_from_dts(struct serdes *ser_des)
{
	u32 ret = 0, read_value = 0;

	if (!ser_des)
		return -1;

	ret = of_property_read_u32(ser_des->dev->of_node,
		SERDES_INITED_IN_LK_NODE_NAME, &read_value);
	if (!ret) {
		pr_info("inited_in_lk = 0x%x!\n", read_value);
		ser_des->inited_in_lk = read_value ? true : false;
	} else {
		ser_des->inited_in_lk = false;
		return -1;
	}
	pr_info("inited_in_lk = 0x%x!\n", ser_des->inited_in_lk);

	return 0;
}

static int serdes_get_super_frame_from_dts(struct serdes *ser_des)
{
	int ret;
	u32 read_value = 0;

	if (!ser_des || !ser_des->setting_node)
		return -1;

	ret = of_property_read_u32(ser_des->setting_node, SERDES_SUPER_FRAME_NODE_NAME, &read_value);
	if (!ret) {
		ser_des->super_frame = read_value ? true : false;
		pr_info("super_frame = %d\n", read_value);
	} else {
		pr_info("super_frame = 0\n");
		ser_des->super_frame = false;
	}
	return 0;
}

static int serdes_get_timing_info_from_dts(struct serdes *ser_des, u8 port)
{
	int ret;
	u32 read_value = 0;
	char mode_name[] = PANEL_MODE_NODE_NAME;
	struct device_node *mode_node = NULL;
	struct deserializer *des;

	des = ser_des->super_frame ? ((port == 0) ? ser_des->desa : ser_des->desb) : ser_des->desdef;
	if (!des)
		return -1;

	if (!des->des_node)
		return -1;

	mode_node = of_find_node_by_name(des->des_node, mode_name);
	if (mode_node) {
		ret = of_property_read_u32(mode_node, PANEL_WIDTH_NODE_NAME, &read_value);
		if (!ret) {
			des->disp_mode.width = read_value;
			pr_info("disp_mode.width = %d\n", read_value);
		} else {
			pr_info("error: read width error!\n");
			return -1;
		}

		ret = of_property_read_u32(mode_node, PANEL_HEIGHT_NODE_NAME, &read_value);
		if (!ret) {
			des->disp_mode.height = read_value;
			pr_info("disp_mode.height = %d\n", read_value);
		} else {
			pr_info("error: read height error!\n");
			return -1;
		}

		ret = of_property_read_u32(mode_node, PANEL_HFP_NODE_NAME, &read_value);
		if (!ret) {
			des->disp_mode.hfp = read_value;
			pr_info("disp_mode.hfp = %d\n", read_value);
		} else {
			pr_info("error: read hfp error!\n");
			return -1;
		}

		ret = of_property_read_u32(mode_node, PANEL_HSA_NODE_NAME, &read_value);
		if (!ret) {
			des->disp_mode.hsa = read_value;
			pr_info("disp_mode.hsa = %d\n", read_value);
		} else {
			pr_info("error: read hsa error!\n");
			return -1;
		}

		ret = of_property_read_u32(mode_node, PANEL_HBP_NODE_NAME, &read_value);
		if (!ret) {
			des->disp_mode.hbp = read_value;
			pr_info("disp_mode.hbp = %d\n", read_value);
		} else {
			pr_info("error: read hbp error!\n");
			return -1;
		}

		ret = of_property_read_u32(mode_node, PANEL_VFP_NODE_NAME, &read_value);
		if (!ret) {
			des->disp_mode.vfp = read_value;
			pr_info("disp_mode.vfp = %d\n", read_value);
		} else {
			pr_info("error: read vfp error!\n");
			return -1;
		}

		ret = of_property_read_u32(mode_node, PANEL_VSA_NODE_NAME, &read_value);
		if (!ret) {
			des->disp_mode.vsa = read_value;
			pr_info("disp_mode.vsa = %d\n", read_value);
		} else {
			pr_info("error: read vsa error!\n");
			return -1;
		}

		ret = of_property_read_u32(mode_node, PANEL_VBP_NODE_NAME, &read_value);
		if (!ret) {
			des->disp_mode.vbp = read_value;
			pr_info("disp_mode.vbp = %d\n", read_value);
		} else {
			pr_info("error: read vbp error!\n");
			return -1;
		}

		ret = of_property_read_u32(mode_node, PANEL_VREFRESH_NODE_NAME, &read_value);
		if (!ret) {
			des->disp_mode.fps = read_value;
			pr_info("disp_mode.fps = %d\n", read_value);
		} else {
			pr_info("error: read fps error!\n");
			return -1;
		}

		ret = of_property_read_u32(mode_node, PANEL_PLL_NODE_NAME, &read_value);
		if (!ret) {
			des->disp_mode.pll = read_value;
			pr_info("disp_mode.pll = %d\n", read_value);
		} else {
			pr_info("pll not set!\n");
			des->disp_mode.pll = 0;
		}

		ret = of_property_read_u32(mode_node, PANEL_LPPF_NODE_NAME, &read_value);
		if (!ret) {
			des->disp_mode.lppf = read_value;
			pr_info("disp_mode.lppf = %d\n", read_value);
		} else {
			pr_info("lppf = 0!\n");
			des->disp_mode.lppf = 0;
		}

		ret = of_property_read_u32(mode_node, PANEL_WIDTH_MM_NODE_NAME, &read_value);
		if (!ret) {
			des->disp_mode.physcial_w = read_value;
			pr_info("disp_mode.physcial_w = %d\n", read_value);
		} else {
			pr_info("warning: physcial_w not set!\n");
		}

		ret = of_property_read_u32(mode_node, PANEL_HEIGHT_MM_NODE_NAME, &read_value);
		if (!ret) {
			des->disp_mode.physcial_h = read_value;
			pr_info("disp_mode.physcial_h = %d\n", read_value);
		} else {
			pr_info("warning: physcial_h not set!\n");
		}

		of_node_put(mode_node);
	} else {
		pr_info("error: %s error!\n", __func__);
		return -1;
	}

	return 0;
}

static int serdes_get_ser_init_cmd_from_dts(struct serdes *ser_des)
{
	int num = 0;
	u32 *array;
	int ret = 0, i = 0;

	if (!ser_des || !ser_des->setting_node)
		return -1;

	num = of_property_count_u32_elems(ser_des->setting_node, SER_INIT_CMD_NODE_NAME);
	if (num < 0) {
		pr_info("%s: Error:get num from %s return %d\n", __func__, ser_des->setting_node->name, num);
		return -1;
	}
	array = kcalloc(num, sizeof(u32), GFP_KERNEL);
	if (!array)
		return -1;

	ret = of_property_read_u32_array(ser_des->setting_node, SER_INIT_CMD_NODE_NAME, array, num);
	if (ret) {
		pr_info("%s:error: read ser-init-cmd fail!\n", __func__);
		kfree(array);
		return -1;
	}
	ser_des->ser_init_cmd_num = num / 3;
	ser_des->ser_init_cmd = devm_kzalloc(ser_des->dev,
		ser_des->ser_init_cmd_num * sizeof(struct serdes_cmd), GFP_KERNEL);
	for (i = 0; i < num / 3; i++) {
		ser_des->ser_init_cmd[i].addr = array[i * 3];
		ser_des->ser_init_cmd[i].data = array[i * 3 + 1];
		ser_des->ser_init_cmd[i].delay_ms = array[i * 3 + 2];
	}

	kfree(array);
	return 0;
}

static int serdes_get_des_init_cmd_from_dts(struct serdes *ser_des, u8 port)
{
	int num = 0;
	u32 *array;
	int ret = 0, i = 0;
	struct deserializer *des;

	des = ser_des->super_frame ? ((port == 0) ? ser_des->desa : ser_des->desb) : ser_des->desdef;
	if (!des || !des->des_node)
		return -1;

	num = of_property_count_u32_elems(des->des_node, DES_INIT_CMD_NODE_NAME);
	if (num < 0) {
		pr_info("%s: Error:get num from %s return %d\n", __func__, des->des_node->name, num);
		return -1;
	}
	array = kcalloc(num, sizeof(u32), GFP_KERNEL);
	if (!array)
		return -1;

	ret = of_property_read_u32_array(des->des_node, DES_INIT_CMD_NODE_NAME, array, num);
	if (ret) {
		pr_info("%s:error: read des-init-cmd fail!\n", __func__);
		kfree(array);
		return -1;
	}

	des->des_init_cmd_num = num / 3;
	des->des_init_cmd = devm_kzalloc(ser_des->dev,
		des->des_init_cmd_num * sizeof(struct serdes_cmd), GFP_KERNEL);
	for (i = 0; i < num / 3; i++) {
		des->des_init_cmd[i].addr = array[i * 3];
		des->des_init_cmd[i].data = array[i * 3 + 1];
		des->des_init_cmd[i].delay_ms = array[i * 3 + 2];
	}

	kfree(array);
	return 0;
}

static int serdes_get_bl_on_cmd_from_dts(struct serdes *ser_des, u8 port)
{
	int num = 0;
	u32 *array;
	int ret = 0, i = 0;
	struct deserializer *des;

	des = ser_des->super_frame ? ((port == 0) ? ser_des->desa : ser_des->desb) : ser_des->desdef;
	if (!des || !des->des_node)
		return -1;

	num = of_property_count_u32_elems(des->des_node, BL_ON_CMD_NODE_NAME);
	if (num < 0) {
		pr_info("%s: Error:get num from %s return %d\n", __func__, des->des_node->name, num);
		return -1;
	}
	array = kcalloc(num, sizeof(u32), GFP_KERNEL);
	if (!array)
		return -1;

	ret = of_property_read_u32_array(des->des_node, BL_ON_CMD_NODE_NAME, array, num);
	if (ret) {
		pr_info("%s:error: read des-init-cmd fail!\n", __func__);
		kfree(array);
		return -1;
	}

	des->bl_on_cmd_num = 1;
	des->bl_on_cmd = devm_kzalloc(ser_des->dev,
		sizeof(struct bl_cmd), GFP_KERNEL);
	des->bl_on_cmd->len = num;
	for (i = 0; i < num; i++)
		des->bl_on_cmd->data[i] = array[i];

	kfree(array);
	return 0;
}
static int serdes_get_bl_off_cmd_from_dts(struct serdes *ser_des, u8 port)
{
	int num = 0;
	u32 *array;
	int ret = 0, i = 0;
	struct deserializer *des;

	des = ser_des->super_frame ? ((port == 0) ? ser_des->desa : ser_des->desb) : ser_des->desdef;
	if (!des || !des->des_node)
		return -1;

	num = of_property_count_u32_elems(des->des_node, BL_OFF_CMD_NODE_NAME);
	if (num < 0) {
		pr_info("%s: Error:get num from %s return %d\n", __func__, des->des_node->name, num);
		return -1;
	}
	array = kcalloc(num, sizeof(u32), GFP_KERNEL);
	if (!array)
		return -1;

	ret = of_property_read_u32_array(des->des_node, BL_OFF_CMD_NODE_NAME, array, num);
	if (ret) {
		pr_info("%s:error: read des-init-cmd fail!\n", __func__);
		kfree(array);
		return -1;
	}

	des->bl_off_cmd_num = 1;
	des->bl_off_cmd = devm_kzalloc(ser_des->dev,
		sizeof(struct bl_cmd), GFP_KERNEL);
	des->bl_off_cmd->len = num;
	for (i = 0; i < num; i++)
		des->bl_off_cmd->data[i] = array[i];

	kfree(array);
	return 0;
}

static int serdes_get_compatible_setting_from_dts(struct serdes *ser_des, u8 port)
{
	struct device_node *comp_ext_node;
	u32 *array;
	int num = 0;
	int ret = 0, i = 0, j = 0;

	pr_info("%s +\n", __func__);
	if (!ser_des || !ser_des->setting_node)
		return -1;

	num = of_property_count_u32_elems(ser_des->setting_node, DES_COMPATIBLE_CMD_NODE_NAME);
	if (num < 0) {
		pr_info("%s: Error:get comp-cmd from %s return %d\n", __func__, ser_des->setting_node->name, num);
		return -1;
	}
	pr_info("%s num=%d\n", __func__, num);
	array = kcalloc(num, sizeof(u32), GFP_KERNEL);
	if (!array)
		return -1;
	ret = of_property_read_u32_array(ser_des->setting_node, DES_COMPATIBLE_CMD_NODE_NAME, array, num);
	if (ret) {
		pr_info("%s:error: read compatible-cmd fail!\n", __func__);
		kfree(array);
		return -1;
	}
	ser_des->comp_setting_cmd_num = 0;
	for (i = 0; i < num;) {
		ser_des->comp_setting_cmd[ser_des->comp_setting_cmd_num].rw = array[i + 0];
		ser_des->comp_setting_cmd[ser_des->comp_setting_cmd_num].addr = array[i + 1];
		ser_des->comp_setting_cmd[ser_des->comp_setting_cmd_num].len = array[i + 2];
		if(array[i + 0] == 1) {
			for (j = 0; j < array[i + 2]; j++)
				ser_des->comp_setting_cmd[ser_des->comp_setting_cmd_num].data[j] = array[i + 3 + j];
		} else
			j = 0;
		i += j + 3;
		ser_des->comp_setting_cmd_num++;
	}

	kfree(array);

	comp_ext_node = of_find_node_by_name(ser_des->setting_node, DES_COMPATIBLE_EXP_NODE_NAME);
	if (!comp_ext_node) {
		pr_info("%s: get comp-exp fail!\n", __func__);
		return -1;
	}

	num = of_property_count_u32_elems(comp_ext_node, DES_COMPATIBLE_SETTING_NODE_NAME);
	if (num < 0) {
		pr_info("%s: Error:get comp-set from %s return %d\n", __func__, comp_ext_node->name, num);
		return -1;
	}
	pr_info("%s num=%d\n", __func__, num);

	array = kcalloc(num, sizeof(u32), GFP_KERNEL);
	if (!array)
		return -1;

	ret = of_property_read_u32_array(comp_ext_node, DES_COMPATIBLE_SETTING_NODE_NAME, array, num);
	if (ret) {
		pr_info("%s:error: read compatible-cmd fail!\n", __func__);
		kfree(array);
		return -1;
	}

	ser_des->comp_set_num = 0;
	for (i = 0; i < num / 3; i++) {
		ser_des->cmp_set[i].mask = array[i * 3 + 0];
		ser_des->cmp_set[i].exp_data = array[i * 3 + 1];
		ser_des->cmp_set[i].setting_node = of_parse_phandle(comp_ext_node,
			DES_COMPATIBLE_SETTING_NODE_NAME, i * 3 + 2);
		ser_des->comp_set_num++;

		pr_info("%s: comp_set[%d]=<0x%x 0x%x %s>\n", __func__, i,
			ser_des->cmp_set[i].mask,
			ser_des->cmp_set[i].exp_data, ser_des->cmp_set[i].setting_node->name);
	}
	kfree(array);

	return 0;
}


static int serdes_get_timing_cmd_from_dts(struct serdes *ser_des)
{
	int num = 0;
	u32 *array;
	int ret = 0, i = 0;

	num = of_property_count_u32_elems(ser_des->setting_node, SER_TIMING_CMD_NODE_NAME);
	if (num < 0) {
		pr_info("%s: Error:get timing-cmd from %s return %d\n",
			__func__, ser_des->setting_node->name, num);
		return -1;
	}
	pr_info("%s num=%d\n", __func__, num);

	array = kcalloc(num, sizeof(u32), GFP_KERNEL);
	if (!array)
		return -1;

	ret = of_property_read_u32_array(ser_des->setting_node,
		SER_TIMING_CMD_NODE_NAME, array, num);
	if (ret) {
		pr_info("%s:error: read ser-timing-cmd fail!\n", __func__);
		kfree(array);
		return -1;
	}

	ser_des->ser_timing_cmd_num = num / 3;
	ser_des->ser_timing_cmd = devm_kzalloc(ser_des->dev,
		ser_des->ser_timing_cmd_num * sizeof(struct serdes_cmd), GFP_KERNEL);
	for (i = 0; i < num / 3; i++) {
		ser_des->ser_timing_cmd[i].addr = array[i * 3];
		ser_des->ser_timing_cmd[i].data = array[i * 3 + 1];
		ser_des->ser_timing_cmd[i].delay_ms = array[i * 3 + 2];
	}

	kfree(array);
	return 0;
}

static int serdes_get_lut_cmd_from_dts(struct serdes *ser_des)
{
	int num = 0;
	u32 *array;
	int ret = 0, i = 0;

	num = of_property_count_u32_elems(ser_des->setting_node, SER_LUT_CMD_NODE_NAME);
	if (num < 0) {
		pr_info("%s: Error:get lut-cmd from %s return %d\n",
			__func__, ser_des->setting_node->name, num);
		return -1;
	}

	pr_info("%s num=%d\n", __func__, num);
	array = kcalloc(num, sizeof(u32), GFP_KERNEL);
	if (!array)
		return -1;

	ret = of_property_read_u32_array(ser_des->setting_node, SER_LUT_CMD_NODE_NAME, array, num);
	if (ret) {
		pr_info("%s:error: read ser-dual-lut-cmd fail!\n", __func__);
		kfree(array);
		return -1;
	}

	ser_des->ser_lut_cmd_num = num / 3;
	ser_des->ser_lut_cmd = devm_kzalloc(ser_des->dev,
		ser_des->ser_lut_cmd_num * sizeof(struct serdes_cmd), GFP_KERNEL);
	for (i = 0; i < num / 3; i++) {
		ser_des->ser_lut_cmd[i].addr = array[i * 3];
		ser_des->ser_lut_cmd[i].data = array[i * 3 + 1];
		ser_des->ser_lut_cmd[i].delay_ms = array[i * 3 + 2];
	}

	kfree(array);
	return 0;
}

static int serdes_get_dual_setting_cmd_from_dts(struct serdes *ser_des)
{
	int num = 0;
	u32 *array;
	int ret = 0, i = 0;

	num = of_property_count_u32_elems(ser_des->setting_node, SERDES_DUAL_SETTING_CMD_NODE_NAME);
	if (num < 0) {
		pr_info("%s: Error:get dual-setting-cmd from %s return %d\n", __func__,
			ser_des->setting_node->name, num);
		return -1;
	}

	array = kcalloc(num, sizeof(u32), GFP_KERNEL);
	if (!array)
		return -1;

	ret = of_property_read_u32_array(ser_des->setting_node,
		SERDES_DUAL_SETTING_CMD_NODE_NAME, array, num);
	if (ret) {
		pr_info("%s:error: read ser-des-dual-setting fail!\n", __func__);
		kfree(array);
		return -1;
	}

	ser_des->serdes_dual_setting_cmd_num = num / 4;
	ser_des->serdes_dual_setting_cmd = devm_kzalloc(ser_des->dev,
		ser_des->serdes_dual_setting_cmd_num * sizeof(struct serdes_dual_cmd), GFP_KERNEL);
	for (i = 0; i < num / 4; i++) {
		ser_des->serdes_dual_setting_cmd[i].slave = array[i * 4];
		ser_des->serdes_dual_setting_cmd[i].addr = array[i * 4 + 1];
		ser_des->serdes_dual_setting_cmd[i].data = array[i * 4 + 2];
		ser_des->serdes_dual_setting_cmd[i].delay_ms = array[i * 4 + 3];
	}

	kfree(array);
	return 0;
}
int serdes_bl_on(struct serdes *ser_des)
{
	int i;
	int ret = 0;

	mutex_lock(&i2c_access);
	pr_info("%s +\n", __func__);
	if (!ser_des->super_frame) {
		if (!ser_des->desdef) {
			mutex_unlock(&i2c_access);
			return -1;
		}
		if (ser_des->desdef->bl_on_cmd_num && ser_des->desdef->bl_client) {
			for (i = 0; i < ser_des->desdef->bl_on_cmd_num; i++) {
				ret = i2c_master_send(ser_des->desdef->bl_client,
					ser_des->desdef->bl_on_cmd[i].data,
					ser_des->desdef->bl_on_cmd[i].len);
				if (ret < 0) {
					mutex_unlock(&i2c_access);
					pr_info("%s: send cmd error!\n", __func__);
					return -1;
				}
			}
		}
	} else {
		if (ser_des->desa && ser_des->desa->bl_on_cmd_num && ser_des->desa->bl_client) {
			for (i = 0; i < ser_des->desa->bl_on_cmd_num; i++) {
				ret = i2c_master_send(ser_des->desa->bl_client,
					ser_des->desa->bl_on_cmd[i].data,
					ser_des->desa->bl_on_cmd[i].len);
				if (ret < 0) {
					mutex_unlock(&i2c_access);
					pr_info("%s: send cmd error!\n", __func__);
					return -1;
				}
			}
		}
		if (ser_des->desb && ser_des->desb->bl_on_cmd_num && ser_des->desb->bl_client) {
			for (i = 0; i < ser_des->desb->bl_on_cmd_num; i++) {
				ret = i2c_master_send(ser_des->desb->bl_client,
					ser_des->desb->bl_on_cmd[i].data,
					ser_des->desb->bl_on_cmd[i].len);
				if (ret < 0) {
					mutex_unlock(&i2c_access);
					pr_info("%s: send cmd error!\n", __func__);
					return -1;
				}
			}
		}
	}
	pr_info("%s -\n", __func__);
	mutex_unlock(&i2c_access);
	return ret;
}

int serdes_bl_off(struct serdes *ser_des)
{
	int i;
	int ret = 0;

	mutex_lock(&i2c_access);
	pr_info("%s +\n", __func__);
	if (!ser_des->super_frame) {
		if (!ser_des->desdef) {
			mutex_unlock(&i2c_access);
			return -1;
		}

		if (ser_des->desdef->bl_off_cmd_num && ser_des->desdef->bl_client) {
			for (i = 0; i < ser_des->desdef->bl_off_cmd_num; i++) {
				ret = i2c_master_send(ser_des->desdef->bl_client,
					ser_des->desdef->bl_off_cmd[i].data,
					ser_des->desdef->bl_off_cmd[i].len);
				if (ret < 0) {
					mutex_unlock(&i2c_access);
					pr_info("%s: send cmd error!\n", __func__);
					return -1;
				}
			}
		}
	} else {
		if (ser_des->desa && ser_des->desa->bl_off_cmd_num && ser_des->desa->bl_client) {
			for (i = 0; i < ser_des->desa->bl_off_cmd_num; i++) {
				ret = i2c_master_send(ser_des->desa->bl_client,
					ser_des->desa->bl_off_cmd[i].data,
					ser_des->desa->bl_off_cmd[i].len);
				if (ret < 0) {
					mutex_unlock(&i2c_access);
					pr_info("%s: send cmd error!\n", __func__);
					return -1;
				}
			}
		}
		if (ser_des->desb->bl_off_cmd_num && ser_des->desb->bl_client) {
			for (i = 0; i < ser_des->desb->bl_off_cmd_num; i++) {
				ret = i2c_master_send(ser_des->desb->bl_client,
					ser_des->desb->bl_off_cmd[i].data,
					ser_des->desb->bl_off_cmd[i].len);
				if (ret < 0) {
					mutex_unlock(&i2c_access);
					pr_info("%s: send cmd error!\n", __func__);
					return -1;
				}
			}
		}
	}

	pr_info("%s +-\n", __func__);
	mutex_unlock(&i2c_access);

	return ret;
}

//after pre-init serdes, it must be access eeprom by ser
static void serdes_init_ser(struct serdes *ser_des)
{
	int i;

	pr_info("%s +\n", __func__);
	if (!ser_des)
		return;

	if (ser_des->ser_init_cmd_num) {
		for (i = 0; i < ser_des->ser_init_cmd_num; i++) {
			i2c_write_byte(ser_des->ser_client,
				ser_des->ser_init_cmd[i].addr, ser_des->ser_init_cmd[i].data);
			if (ser_des->ser_init_cmd[i].delay_ms)
				mdelay(ser_des->ser_init_cmd[i].delay_ms);
		}
	}

	pr_info("%s -\n", __func__);
}

static void serdes_init_des(struct serdes *ser_des, u8 port)
{
	int i;
	struct deserializer *des;

	pr_info("%s +\n", __func__);
	if (!ser_des)
		return;
	des = ser_des->super_frame ? ((port == 0) ? ser_des->desa : ser_des->desb) : ser_des->desdef;
	if (!des)
		return;

	if (des->des_init_cmd_num) {
		for (i = 0; i < des->des_init_cmd_num; i++) {
			i2c_write_byte(des->des_client,
				des->des_init_cmd[i].addr, des->des_init_cmd[i].data);
			if (des->des_init_cmd[i].delay_ms)
				mdelay(des->des_init_cmd[i].delay_ms);
		}
	}

	pr_info("%s -\n", __func__);
}

static void serdes_get_comp_setting_by_send_comp_cmd(struct serdes *ser_des)
{
	struct i2c_client *client;
	char val[32] = { 0 };
	u32 i, j;
	int ret = 0;
	u32 cmd_ret = 0;

	pr_info("%s +\n", __func__);

	if (!ser_des)
		return;

	if (ser_des->comp_setting_cmd_num) {
		for (i = 0; i < ser_des->comp_setting_cmd_num; i++) {
			client = i2c_new_dummy_device(ser_des->ser_client->adapter, ser_des->comp_setting_cmd[i].addr);
			if (ser_des->comp_setting_cmd[i].rw == 1) {
				mutex_lock(&i2c_access);
				ret = i2c_master_send(client,
					ser_des->comp_setting_cmd[i].data, ser_des->comp_setting_cmd[i].len);
				mutex_unlock(&i2c_access);
				if (ret < 0) {
					cmd_ret = 0xFFFF;
					i2c_unregister_device(client);
					break;
				}
			} else {
				mutex_lock(&i2c_access);
				ret = i2c_master_recv(client, val, ser_des->comp_setting_cmd[i].len);
				mutex_unlock(&i2c_access);
				if (ret < 0) {
					cmd_ret = 0xFFFF;
					i2c_unregister_device(client);
					break;
				}

				for (j = 0; j < ser_des->comp_setting_cmd[i].len; j++)
					pr_info("read[%d]=[0x%x]\n", j, val[j]);
			}
			i2c_unregister_device(client);
		}

		if (cmd_ret == 0xFFFF) {
			pr_info("compatible cmd error, used last setting as default!\n");
			ser_des->setting_node = ser_des->cmp_set[ser_des->comp_set_num - 1].setting_node;
		}

		for (i = 0; i < ser_des->comp_set_num; i++) {
			pr_info("find setting[%d],val=0x%x\n", i, val[0]);
			if (ser_des->cmp_set[i].exp_data == (val[0] & ser_des->cmp_set[i].mask)) {
				ser_des->setting_node = ser_des->cmp_set[i].setting_node;
				pr_info("%s: find! setting node=<%p>\n",
					__func__, ser_des->cmp_set[i].setting_node);
				break;
			}
		}
		if (i >= ser_des->comp_set_num) {
			pr_info("Not Find, used last setting as default!\n");
			ser_des->setting_node = ser_des->cmp_set[ser_des->comp_set_num - 1].setting_node;
		}
	}

	pr_info("%s: setting_node = %s\n", __func__, ser_des->setting_node->name);
}

void serdes_deinit_serdes(struct serdes *max96789)
{
}

void serdes_poweron_ser(struct serdes *max96789)
{
}

static void serdes_set_timing(struct serdes *ser_des)
{
	int i;

	pr_info("%s +\n", __func__);

	if (ser_des->ser_timing_cmd_num) {
		for (i = 0; i < ser_des->ser_timing_cmd_num; i++) {
			i2c_write_byte(ser_des->ser_client,
				ser_des->ser_timing_cmd[i].addr, ser_des->ser_timing_cmd[i].data);
			if (ser_des->ser_timing_cmd[i].delay_ms)
				mdelay(ser_des->ser_timing_cmd[i].delay_ms);
		}
	}

	pr_info("%s -\n", __func__);
}

static void serdes_set_lut(struct serdes *ser_des)
{
	u16 i = 0;

	pr_info("%s +\n", __func__);

	if (ser_des->ser_lut_cmd_num) {
		for (i = 0; i < ser_des->ser_lut_cmd_num; i++) {
			i2c_write_byte(ser_des->ser_client,
				ser_des->ser_lut_cmd[i].addr, ser_des->ser_lut_cmd[i].data);
			if (ser_des->ser_lut_cmd[i].delay_ms)
				mdelay(ser_des->ser_lut_cmd[i].delay_ms);
		}
	}

	pr_info("%s -\n", __func__);
}

static void serdes_set_dual_setting(struct serdes *ser_des)
{
	int i;
	struct i2c_client *client;

	pr_info("%s +\n", __func__);

	if (ser_des->serdes_dual_setting_cmd_num) {
		for (i = 0; i < ser_des->serdes_dual_setting_cmd_num; i++) {
			switch (ser_des->serdes_dual_setting_cmd[i].slave) {
			case SER:
				client = ser_des->ser_client;
				break;
			case DEF_DES:
				client = ser_des->desdef->des_client;
				break;
			case DESA:
				client = ser_des->desa->des_client;
				break;
			case DESB:
				client = ser_des->desb->des_client;
				break;
			default:
				client = ser_des->ser_client;
				break;
			};
			if (!client)
				return;
			i2c_write_byte(client,
				ser_des->serdes_dual_setting_cmd[i].addr,
				ser_des->serdes_dual_setting_cmd[i].data);
			if (ser_des->serdes_dual_setting_cmd[i].delay_ms)
				mdelay(ser_des->serdes_dual_setting_cmd[i].delay_ms);
		}
	}

	pr_info("%s -\n", __func__);
}

void serdes_reset_ser(struct serdes *max96789)
{
	if (!max96789->reset_gpio)
		return;

	gpiod_set_value(max96789->reset_gpio, 1);
	msleep(50);
	gpiod_set_value(max96789->reset_gpio, 0);
	msleep(50);
	gpiod_set_value(max96789->reset_gpio, 1);
	msleep(50);
}
static int serdes_get_general_info_from_dts(struct serdes *ser_des)
{
	struct device_node *desdef_node, *desa_node, *desb_node;

	if (!ser_des) {
		pr_info("error: %s serdes is NULL!\n", __func__);
		return -1;
	}

	pr_info("%s: get compatible setting\n", __func__);
	if (!serdes_get_compatible_setting_from_dts(ser_des, 0)) {
		if (!ser_des->inited_in_lk) {
			serdes_poweron_ser(ser_des);
			serdes_reset_ser(ser_des);
		}
		serdes_get_comp_setting_by_send_comp_cmd(ser_des);
	}

	desdef_node = of_find_node_by_name(ser_des->setting_node, DESDEF_NODE_NAME);
	ser_des->desdef->des_node = desdef_node;

	ser_des->desdef->des_iic_addr = serdes_get_des_iic_addr_from_dts(ser_des->desdef->des_node);
	if (ser_des->desdef->des_iic_addr == 0xFF) {
		pr_info("error: get default des iic addr fail\n");
		return -1;
	}
	pr_info("default des iic addr = 0x%x\n", ser_des->desdef->des_iic_addr);

	ser_des->desdef->bl_iic_addr = serdes_get_bl_iic_addr_from_dts(ser_des->desdef->des_node);
	if (ser_des->desdef->bl_iic_addr == 0xFF) {
		pr_info("error: get default bl iic addr fail, bl not control by mcu?\n");
		//return -1;
	}
	pr_info("default bl iic addr = 0x%x\n", ser_des->desdef->bl_iic_addr);

	serdes_get_super_frame_from_dts(ser_des);
	pr_info("super frame = 0x%x\n", ser_des->super_frame);

	if (ser_des->super_frame) {
		desa_node = of_find_node_by_name(ser_des->setting_node, DESA_NODE_NAME);
		ser_des->desa->des_node = desa_node;
		pr_info("desa name = %s, node=<%p>\n", desa_node->name, desa_node);
		ser_des->desa->des_iic_addr =
			serdes_get_des_iic_addr_from_dts(ser_des->desa->des_node);
		if (ser_des->desa->des_iic_addr == 0xFF) {
			pr_info("error: get desa iic addr fail\n");
			return -1;
		}
		pr_info("desa iic addr = 0x%x\n", ser_des->desa->des_iic_addr);

		ser_des->desa->bl_iic_addr =
			serdes_get_bl_iic_addr_from_dts(ser_des->desa->des_node);
		if (ser_des->desa->bl_iic_addr == 0xFF) {
			pr_info("error: get desa bl iic addr fail, bl not control by mcu?\n");
			//return -1;
		}
		pr_info("desa bl iic addr = 0x%x\n", ser_des->desa->bl_iic_addr);

		desb_node = of_find_node_by_name(ser_des->setting_node, DESB_NODE_NAME);
		pr_info("desb name = %s, offset=<%p>\n", desb_node->name, desb_node);

		ser_des->desb->des_node = desb_node;
		ser_des->desb->des_iic_addr =
			serdes_get_des_iic_addr_from_dts(ser_des->desb->des_node);
		if (ser_des->desb->des_iic_addr == 0xFF) {
			pr_info("error: get desb iic addr fail\n");
			return -1;
		}
		pr_info("desb iic addr = 0x%x\n", ser_des->desb->des_iic_addr);

		ser_des->desb->bl_iic_addr =
			serdes_get_bl_iic_addr_from_dts(ser_des->desb->des_node);
		if (ser_des->desb->bl_iic_addr == 0xFF) {
			pr_info("error: get desb bl iic addr fail, bl not control by mcu?\n");
			//return -1;
		}
		pr_info("desb bl iic addr = 0x%x\n", ser_des->desb->bl_iic_addr);
	}
	return 0;
}


int serdes_build_timing_cmd(struct serdes *ser_des)
{
	int i = 0;
	u16 width;
	u16 hfp;
	u16 hsa;
	u16 hbp;
	u16 height;
	u16 vfp;
	u16 vbp;
	u16 vsa;

	if (!ser_des->super_frame) {
		pr_info("%s: single link mode no need!\n", __func__);
		return 0;
	}

	width = ser_des->desa->disp_mode.width + ser_des->desb->disp_mode.width;
	hfp = ser_des->desa->disp_mode.hfp + ser_des->desb->disp_mode.hfp;
	hsa = ser_des->desa->disp_mode.hsa + ser_des->desb->disp_mode.hsa;
	hbp = ser_des->desa->disp_mode.hbp + ser_des->desb->disp_mode.hbp;
	height = (ser_des->desa->disp_mode.height >= ser_des->desb->disp_mode.height) ?
		ser_des->desa->disp_mode.height : ser_des->desb->disp_mode.height;
	vfp = (ser_des->desa->disp_mode.height >= ser_des->desb->disp_mode.height) ?
		ser_des->desa->disp_mode.vfp : ser_des->desb->disp_mode.vfp;
	vsa = (ser_des->desa->disp_mode.height >= ser_des->desb->disp_mode.height) ?
		ser_des->desa->disp_mode.vsa : ser_des->desb->disp_mode.vsa;
	vbp = (ser_des->desa->disp_mode.height >= ser_des->desb->disp_mode.height) ?
		ser_des->desa->disp_mode.vbp : ser_des->desb->disp_mode.vbp;

	// 0x385 hsa[7:0]
	ser_des->ser_timing_cmd[0].data = hsa & 0xFF;
	// 0x386 vsa[7:0]
	ser_des->ser_timing_cmd[1].data = vsa & 0xFF;
	// 0x387 vsa[11:8]hsa[11:8]
	ser_des->ser_timing_cmd[2].data = ((vsa >> 8) << 4) | (hsa >> 8);
	// 0x3A5 vfp[7:0]
	ser_des->ser_timing_cmd[3].data = vfp & 0xFF;
	// 0x3A7 vhp[11:4]
	ser_des->ser_timing_cmd[4].data = vbp >> 4;
	// 0x3A6 vhp[3:0]vfp[11:8] note: array index are fixed
	ser_des->ser_timing_cmd[5].data = ((vbp & 0x0F) << 4) | (vfp >> 8);
	// 0x3A8 vact[7:0]
	ser_des->ser_timing_cmd[6].data = height & 0xFF;
	// 0x3A9 vact[12:8]
	ser_des->ser_timing_cmd[7].data = (height >> 8) & 0x1F;
	// 0x3AA hfp[7:0]
	ser_des->ser_timing_cmd[8].data = hfp & 0xFF;
	// 0x3AC hbp[11:4]
	ser_des->ser_timing_cmd[9].data = hbp >> 4;
	// 0x3AB hbp[3:0]hfp[11:8] note: array index are fixed
	ser_des->ser_timing_cmd[10].data = ((hbp & 0x0F) << 4) | (hfp >> 8);
	// 0x3AD hact[7:0]
	ser_des->ser_timing_cmd[11].data = width & 0xFF;
	// 0x3AE hact[12:8]
	ser_des->ser_timing_cmd[12].data = (width >> 8) & 0x1F;

	for (i = 0; i < 13; i++)
		pr_info("vdo-timing-cmd: addr=0x%x, data=0x%x, delay=0x%x\n",
			ser_des->ser_timing_cmd[i].addr,
			ser_des->ser_timing_cmd[i].data,
			ser_des->ser_timing_cmd[i].delay_ms);

	return 0;
}
static int serdes_get_serdes_info_from_dts(struct serdes *ser_des)
{
	u32 ret = 0;

	pr_info("%s 1. get ser init cmd\n", __func__);
	serdes_get_ser_init_cmd_from_dts(ser_des);
	pr_info("%s 2. get des init cmd\n", __func__);
	serdes_get_des_init_cmd_from_dts(ser_des, 0);
	pr_info("%s 3. get bl on cmd\n", __func__);
	serdes_get_bl_on_cmd_from_dts(ser_des, 0);
	pr_info("%s 4. get bl off cmd\n", __func__);
	serdes_get_bl_off_cmd_from_dts(ser_des, 0);
	pr_info("%s 5. get timing info\n", __func__);
	ret = serdes_get_timing_info_from_dts(ser_des, 0);
	if (ret) {
		pr_info("error: get timing info from des fail\n");
		return -1;
	}

	if (ser_des->super_frame) {
		pr_info("%s 6. super frame, get timing cmd\n", __func__);
		serdes_get_timing_cmd_from_dts(ser_des);
		pr_info("%s 7. super frame, get lut cmd\n", __func__);
		serdes_get_lut_cmd_from_dts(ser_des);
		pr_info("%s 8. super frame, get dual setting cmd\n", __func__);
		serdes_get_dual_setting_cmd_from_dts(ser_des);
		pr_info("%s 9. super frame, get desb init cmd\n", __func__);
		serdes_get_des_init_cmd_from_dts(ser_des, 1);
		pr_info("%s 10. super frame, get desb timing info\n", __func__);
		serdes_get_timing_info_from_dts(ser_des, 1);
		pr_info("%s 11 get desb bl on cmd\n", __func__);
		serdes_get_bl_on_cmd_from_dts(ser_des, 1);
		pr_info("%s 12 get desb bl off cmd\n", __func__);
		serdes_get_bl_off_cmd_from_dts(ser_des, 1);
		pr_info("%s 13 build dual timing cmd\n", __func__);
		serdes_build_timing_cmd(ser_des);
	}

	return 0;
}

void serdes_enable(struct drm_bridge *bridge)
{
	struct serdes *ser_des = bridge_to_serdes(bridge);

	pr_info("%s +\n", __func__);

	if (ser_des->enabled)
		return;

	if (ser_des->super_frame) {
		serdes_set_timing(ser_des);
		serdes_set_dual_setting(ser_des);
		serdes_set_lut(ser_des);
		serdes_init_des(ser_des, 1);
	}
	serdes_init_des(ser_des, 0);
	serdes_bl_on(ser_des);
	ser_des->enabled = true;

	pr_info("%s -\n", __func__);
}
EXPORT_SYMBOL(serdes_enable);

void serdes_pre_enable(struct drm_bridge *bridge)
{
	struct serdes *ser_des = bridge_to_serdes(bridge);

	pr_info("%s +\n", __func__);

	if (ser_des->pre_enabled)
		return;

	// 1. power on ser/des/panel
	serdes_poweron_ser(ser_des);

	// 2. reset ser
	serdes_reset_ser(ser_des);

	// 3. init ser and des
	serdes_init_ser(ser_des);

	ser_des->pre_enabled = true;
	pr_info("%s -\n", __func__);
}
EXPORT_SYMBOL(serdes_pre_enable);

void serdes_disable(struct drm_bridge *bridge)
{
	struct serdes *ser_des = bridge_to_serdes(bridge);

	pr_info("%s +\n", __func__);
	ser_des->enabled = false;
	ser_des->pre_enabled = false;

	serdes_bl_off(ser_des);

	serdes_deinit_serdes(ser_des);
	gpiod_set_value(ser_des->reset_gpio, 0);
	pr_info("%s -\n", __func__);
}
EXPORT_SYMBOL(serdes_disable);

void serdes_get_modes(struct drm_bridge *bridge, struct vdo_timing *disp_mode)
{
	struct serdes *ser_des = bridge_to_serdes(bridge);

	if (ser_des->super_frame) {
		disp_mode->width = ser_des->desa->disp_mode.width + ser_des->desb->disp_mode.width;
		disp_mode->hfp = ser_des->desa->disp_mode.hfp + ser_des->desb->disp_mode.hfp;
		disp_mode->hsa = ser_des->desa->disp_mode.hsa + ser_des->desb->disp_mode.hsa;
		disp_mode->hbp = ser_des->desa->disp_mode.hbp + ser_des->desb->disp_mode.hbp;
		disp_mode->height = (ser_des->desa->disp_mode.height
			>= ser_des->desb->disp_mode.height) ?
			ser_des->desa->disp_mode.height : ser_des->desb->disp_mode.height;
		disp_mode->vfp = (ser_des->desa->disp_mode.height
			>= ser_des->desb->disp_mode.height) ?
			ser_des->desa->disp_mode.vfp : ser_des->desb->disp_mode.vfp;
		disp_mode->vsa = (ser_des->desa->disp_mode.height
			>= ser_des->desb->disp_mode.height) ?
			ser_des->desa->disp_mode.vsa : ser_des->desb->disp_mode.vsa;
		disp_mode->vbp = (ser_des->desa->disp_mode.height
			>= ser_des->desb->disp_mode.height) ?
			ser_des->desa->disp_mode.vbp : ser_des->desb->disp_mode.vbp;
		disp_mode->fps = (ser_des->desa->disp_mode.height
			>= ser_des->desb->disp_mode.height) ?
			ser_des->desa->disp_mode.fps : ser_des->desb->disp_mode.fps;
		disp_mode->pll = 0;
		disp_mode->lppf = 1;
		disp_mode->physcial_w = (ser_des->desa->disp_mode.height
			>= ser_des->desb->disp_mode.height) ?
			ser_des->desa->disp_mode.physcial_w : ser_des->desb->disp_mode.physcial_w;
		disp_mode->physcial_h = (ser_des->desa->disp_mode.height
			>= ser_des->desb->disp_mode.height) ?
			ser_des->desa->disp_mode.physcial_h : ser_des->desb->disp_mode.physcial_h;
	} else {
		disp_mode->width = ser_des->desdef->disp_mode.width;
		disp_mode->hfp = ser_des->desdef->disp_mode.hfp;
		disp_mode->hsa = ser_des->desdef->disp_mode.hsa;
		disp_mode->hbp = ser_des->desdef->disp_mode.hbp;
		disp_mode->height = ser_des->desdef->disp_mode.height;
		disp_mode->vfp = ser_des->desdef->disp_mode.vfp;
		disp_mode->vsa = ser_des->desdef->disp_mode.vsa;
		disp_mode->vbp = ser_des->desdef->disp_mode.vbp;
		disp_mode->fps = ser_des->desdef->disp_mode.fps;
		disp_mode->pll = ser_des->desdef->disp_mode.pll;
		disp_mode->lppf = ser_des->desdef->disp_mode.lppf;
		disp_mode->physcial_w = ser_des->desdef->disp_mode.physcial_w;
		disp_mode->physcial_h = ser_des->desdef->disp_mode.physcial_h;
	}
}
EXPORT_SYMBOL(serdes_get_modes);


static int serdes_bridge_attach(struct drm_bridge *bridge,
	enum drm_bridge_attach_flags flags)
{
	pr_info("%s +\n", __func__);

	pr_info("%s -\n", __func__);
	return 0;
}

static const struct drm_bridge_funcs serdes_bridge_funcs = {
	.enable = serdes_enable,
	.disable = serdes_disable,
	.attach = serdes_bridge_attach,
};

static int serdes_iic_driver_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct serdes *max96789;
	struct device_node *setting_node;
	const phandle *setting_phandle;

	pr_info("%s+:\n", __func__);

	max96789 = devm_kzalloc(dev, sizeof(struct serdes), GFP_KERNEL);
	if (!max96789)
		return -ENOMEM;

	max96789->desdef = devm_kzalloc(dev, sizeof(struct deserializer), GFP_KERNEL);
	if (!max96789->desdef)
		return -ENOMEM;

	max96789->desa = devm_kzalloc(dev, sizeof(struct deserializer), GFP_KERNEL);
	if (!max96789->desa)
		return -ENOMEM;

	max96789->desb = devm_kzalloc(dev, sizeof(struct deserializer), GFP_KERNEL);
	if (!max96789->desb)
		return -ENOMEM;

	setting_phandle = of_get_property(dev->of_node, SETTING_NODE_NAME, NULL);
	if (!setting_phandle) {
		pr_info("no panel setting handle in: %s\n", dev->of_node->name);
		return -1;
	}
	setting_node = of_find_node_by_phandle(cpu_to_be32p(setting_phandle));

	max96789->dev = dev;
	max96789->ser_client = client;
	max96789->setting_node = setting_node;

	max96789->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(max96789->reset_gpio)) {
		pr_info("cannot get reset-gpios %ld\n",
			PTR_ERR(max96789->reset_gpio));
		return -1;
	}
	serdes_get_inited_flag_from_dts(max96789);
	serdes_get_super_frame_from_dts(max96789);
	serdes_get_general_info_from_dts(max96789);
	serdes_get_serdes_info_from_dts(max96789);

	if (max96789->desdef->des_iic_addr != 0 && max96789->desdef->des_iic_addr != 0xFF)
		max96789->desdef->des_client = i2c_new_dummy_device(client->adapter,
			max96789->desdef->des_iic_addr);
	if (max96789->desdef->bl_iic_addr != 0 && max96789->desdef->bl_iic_addr != 0xFF)
		max96789->desdef->bl_client = i2c_new_dummy_device(client->adapter,
			max96789->desdef->bl_iic_addr);

	if (max96789->super_frame) {
		if (max96789->desa->des_iic_addr != 0 && max96789->desa->des_iic_addr != 0xFF)
			max96789->desa->des_client = i2c_new_dummy_device(client->adapter,
				max96789->desa->des_iic_addr);
		if (max96789->desa->bl_iic_addr != 0 && max96789->desa->bl_iic_addr != 0xFF)
			max96789->desa->bl_client = i2c_new_dummy_device(client->adapter,
				max96789->desa->bl_iic_addr);

		if (max96789->desb->des_iic_addr != 0 && max96789->desb->des_iic_addr != 0xFF)
			max96789->desb->des_client = i2c_new_dummy_device(client->adapter,
				max96789->desb->des_iic_addr);
		if (max96789->desb->bl_iic_addr != 0 && max96789->desb->bl_iic_addr != 0xFF)
			max96789->desb->bl_client = i2c_new_dummy_device(client->adapter,
				max96789->desb->bl_iic_addr);
	}

	if (max96789->inited_in_lk) {
		max96789->enabled = true;
		max96789->pre_enabled = true;
	}
	max96789->bridge.funcs = &serdes_bridge_funcs;
	max96789->bridge.of_node = dev->of_node;
	drm_bridge_add(&max96789->bridge);

	i2c_set_clientdata(client, max96789);

	pr_info("%s-\n", __func__);
	return 0;
}

static void serdes_iic_driver_remove(struct i2c_client *client)
{
	struct serdes *max96789 = i2c_get_clientdata(client);

	pr_info("%s +\n", __func__);

	if (max96789->desdef->des_client)
		i2c_unregister_device(max96789->desdef->des_client);
	if (max96789->desdef->bl_client)
		i2c_unregister_device(max96789->desdef->bl_client);
	if (max96789->super_frame) {
		if (max96789->desa->des_client)
			i2c_unregister_device(max96789->desa->des_client);
		if (max96789->desb->des_client)
			i2c_unregister_device(max96789->desb->des_client);
		if (max96789->desa->bl_client)
			i2c_unregister_device(max96789->desa->bl_client);
		if (max96789->desb->bl_client)
			i2c_unregister_device(max96789->desb->bl_client);
	}
	pr_info("%s -\n", __func__);
}

static const struct of_device_id serdes_iic_match[] = {
	{.compatible = "maxiam,max96789"},
	{},
};

MODULE_DEVICE_TABLE(of, serdes_iic_match);

static struct i2c_driver serdes_iic_driver = {
	.driver = {
		.name = "maxiam,max96789",
		.of_match_table = serdes_iic_match,
	},
	.probe = serdes_iic_driver_probe,
	.remove = serdes_iic_driver_remove,
};

module_i2c_driver(serdes_iic_driver);

MODULE_AUTHOR("Henry Tu <henry.tu@mediatek.com>");
MODULE_DESCRIPTION("max96789 dsi bridge driver");
MODULE_LICENSE("GPL");
