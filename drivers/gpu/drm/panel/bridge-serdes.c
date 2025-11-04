// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <drm/drm_bridge.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <linux/gpio/consumer.h>
#if IS_ENABLED(CONFIG_ENABLE_SERDES_HOTPLUG)
#include <uapi/linux/sched/types.h>
#include <linux/of_irq.h>
#endif
#include "bridge-serdes.h"

#define ENABLE_HOTPLUG_INT 0
#define ENALBE_INIT_WORK   1

#define PANEL_NODE_NAME							"panel"
#define MASTER_NODE_NAME						"master"
#define CONFIG_NODE_NAME						"config"
#define SERDES_SUPERFRAME_FLAG_NODE_NAME		"superframe"
#define SERDES_MST_FLAG_NODE_NAME				"mst"
#define SERDES_DUAL_LINK_FLAG_NODE_NAME			"dual-link"

#define PRE_INIT_CMD_NODE_NAME					"pre-init-cmd"
#define I2C_REMAP_CMD_NODE_NAME					"i2c-remap-cmd"
#define POST_INIT_CMD_NODE_NAME					"post-init-cmd"
#define LINKA_INIT_CMD_NODE_NAME				"linka-init-cmd"
#define LINKB_INIT_CMD_NODE_NAME				"linkb-init-cmd"
#define LINKA_DENIT_CMD_NODE_NAME				"linka-deinit-cmd"
#define LINKB_DENIT_CMD_NODE_NAME				"linkb-deinit-cmd"
#define DEINIT_CMD_NODE_NAME					"deinit-cmd"
#define COMPATIBLE_PRE_CMD_NODE_NAME			"comp-cmd"
#define COMPATIBLE_TYPE_NODE_NAME				"comp-type"

#define SER_STATUS_CMD_NODE_NAME				"ser-status"
#define LINKA_STATUS_CMD_NODE_NAME				"linka-status"
#define LINKB_STATUS_CMD_NODE_NAME				"linkb-status"
#define LINKA_INITED_STATUS_CMD_NODE_NAME		"linka-inited-status"
#define LINKB_INITED_STATUS_CMD_NODE_NAME		"linkb-inited-status"

#define PANEL_TIMING_A_NODE_NAME				"panel-timing-a"
#define PANEL_TIMING_B_NODE_NAME				"panel-timing-b"
#define PANEL_WIDTH_NODE_NAME					"width"
#define PANEL_HEIGHT_NODE_NAME					"height"
#define PANEL_HFP_NODE_NAME						"hfp"
#define PANEL_HSA_NODE_NAME						"hsa"
#define PANEL_HBP_NODE_NAME						"hbp"
#define PANEL_VSA_NODE_NAME						"vsa"
#define PANEL_VBP_NODE_NAME						"vbp"
#define PANEL_VFP_NODE_NAME						"vfp"
#define PANEL_CLOCK_NODE_NAME					"clock"
#define PANEL_FPS_NODE_NAME						"fps"
#define PANEL_PLL_NODE_NAME						"pll"
#define PANEL_DSI_PREFETCH_NODE_NAME			"prefetch"
#define PANEL_WIDTH_MM_NODE_NAME				"width-mm"
#define PANEL_HEIGHT_MM_NODE_NAME				"height-mm"

#define NUM_OF_COMP_CONFIG_TYPE					6 // elem num in comp_config_type_cmd

struct node_map {
	u32 type;
	const char *name;
};

enum SERDES_STATUS_TYPE {
	SER_STATUS = 0,
	LINKA_STATUS,
	LINKB_STATUS,
	LINKA_INITED_STATUS,
	LINKB_INITED_STATUS,
	MAX_STATUS
};

static struct node_map status_node_name[] = {
	{ SER_STATUS, SER_STATUS_CMD_NODE_NAME },
	{ LINKA_STATUS, LINKA_STATUS_CMD_NODE_NAME },
	{ LINKB_STATUS, LINKB_STATUS_CMD_NODE_NAME },
	{ LINKA_INITED_STATUS, LINKA_INITED_STATUS_CMD_NODE_NAME },
	{ LINKB_INITED_STATUS, LINKB_INITED_STATUS_CMD_NODE_NAME },
};

enum CMD_TYPE {
	PRE_INIT_CMD,
	I2C_REMAP_CMD,
	POST_INIT_CMD,
	LINKA_INIT_CMD,
	LINKB_INIT_CMD,
	LINKA_DEINIT_CMD,
	LINKB_DEINIT_CMD,
	DEINIT_CMD,
	MAX_CONFIG_CMD
};

static struct node_map cmd_node_name[] = {
	{ PRE_INIT_CMD, PRE_INIT_CMD_NODE_NAME },
	{ I2C_REMAP_CMD, I2C_REMAP_CMD_NODE_NAME },
	{ POST_INIT_CMD, POST_INIT_CMD_NODE_NAME },
	{ LINKA_INIT_CMD, LINKA_INIT_CMD_NODE_NAME },
	{ LINKB_INIT_CMD, LINKB_INIT_CMD_NODE_NAME },
	{ LINKA_DEINIT_CMD, LINKA_DENIT_CMD_NODE_NAME },
	{ LINKB_DEINIT_CMD, LINKB_DENIT_CMD_NODE_NAME },
	{ DEINIT_CMD, DEINIT_CMD_NODE_NAME },
};

struct i2c_cmd_single {
	u16 addr;
	u8 data;
	u8 delay_ms;
};

struct i2c_cmd_multi {
	u16 len;
	u8  data[32];
};

struct device_cmd {
	u8 dev_addr;
	u8 reg_width;
	union {
		struct i2c_cmd_single single_i2c_cmd;
		struct i2c_cmd_multi multi_i2c_cmd;
	};
};

struct serdes_config_cmd {
	u32 dev_cmd_num;
	struct device_cmd **dev_cmd;
};

struct serdes_status_cmd {
	u8 dev_addr;
	u8 reg_width;
	u16 reg_addr;
	u8 mask;
	u8 exp_data;
};

struct comp_config_type_cmd {
	u8 dev_addr;
	u8 reg_width;
	u16 reg_addr;
	u8 mask;
	u8 exp_data;
	struct device_node *config_node;
};

struct comp_config_cmd {
	u32 cfg_type_cmd_num;
	struct comp_config_type_cmd **cfg_type_cmd;
};

enum DRIVER_TYPE {
	SERDES_FOR_DSI,
	SERDES_FOR_EDP,
	SERDES_FOR_DP
};
struct serdes_driver_data {
	int port;
	enum DRIVER_TYPE type;
	u32 init_flag_mask;
};

enum LINK_TYPE {
	LINK_TYPE_SST,
	LINK_TYPE_MST,
	LINK_TYPE_SUPERFRAME,
	LINK_TYPE_DUAL_LINK,
};

struct serdes_bridge {
	struct drm_bridge bridge;
	struct device *dev;
	struct i2c_client *client;
	struct device_node *config_node;
	struct gpio_desc *reset_gpio;
	struct gpio_desc **power_en_gpio;
	int power_en_gpios_count;

	bool inited_in_lk;
	enum LINK_TYPE link_type;

	bool port0_enabled;
	bool port0_pre_enabled;
	bool port1_enabled;
	bool port1_pre_enabled;
	bool is_suspend;

	struct serdes_config_cmd *cfg_cmd[MAX_CONFIG_CMD];
	struct serdes_status_cmd *sta_cmd[MAX_STATUS];
	struct comp_config_cmd *comp_cfg_cmd;
	struct vdo_timing panel_timing[2];

	struct serdes_driver_data *driver_data;
	struct serdes_bridge *master_serdes;
	struct priv_panel_data panel_data;

#if ENALBE_INIT_WORK
	struct work_struct serdes_pre_init_work;
	struct work_struct serdes_init_work;
	struct completion serdes_pre_init_complete;
	struct completion serdes_init_complete;
#endif

#if IS_ENABLED(CONFIG_ENABLE_SERDES_HOTPLUG)
	struct task_struct *hotplug_task;
	wait_queue_head_t hotplug_wq;
	atomic_t hotplug_event;
	bool stop_thread;
#if ENABLE_HOTPLUG_INT
	int irq_num;
#endif
#endif
};

static DEFINE_MUTEX(i2c_access);

static inline struct serdes_bridge *
		bridge_to_serdes(struct drm_bridge *bridge)
{
	return container_of(bridge, struct serdes_bridge, bridge);
}

static inline struct serdes_bridge *
		panel_to_serdes(struct drm_panel *panel)
{
	struct priv_panel_data *panel_data = container_of(panel, struct priv_panel_data, panel);

	if (panel_data)
		return container_of(panel_data, struct serdes_bridge, panel_data);
	return NULL;
}

static int i2c_write_byte(struct i2c_client *i2c, struct device_cmd *dev_cmd)
{
	int ret = 0, len = 0;
	u8 buf[32] = {0};
	u8 tmp_dev_addr = 0;

	if (!i2c || !dev_cmd) {
		pr_info("serdes %s: i2c or dev_cmd is NULL!\n", __func__);
		return -1;
	}

	if (dev_cmd->reg_width == 16) {
		buf[0] = dev_cmd->single_i2c_cmd.addr >> 8;
		buf[1] = dev_cmd->single_i2c_cmd.addr & 0xFF;
		buf[2] = dev_cmd->single_i2c_cmd.data;
		len = 3;
	} else if (dev_cmd->reg_width == 8) {
		buf[0] = dev_cmd->single_i2c_cmd.addr;
		buf[1] = dev_cmd->single_i2c_cmd.data;
		len = 2;
	} else if (dev_cmd->reg_width == 0) {
		if (dev_cmd->multi_i2c_cmd.len >= 32) {
			pr_info("serdes %s: Invalid data length[%d]!\n",
				__func__, dev_cmd->multi_i2c_cmd.len);
			return -1;
		}
		memcpy(buf, dev_cmd->multi_i2c_cmd.data, dev_cmd->multi_i2c_cmd.len);
		len = dev_cmd->multi_i2c_cmd.len;
	} else {
		pr_info("%s: Invalid register width[%d]!\n", __func__, dev_cmd->reg_width);
		return -1;
	}

	mutex_lock(&i2c_access);
	tmp_dev_addr = i2c->addr;
	i2c->addr = dev_cmd->dev_addr;
	ret = i2c_master_send(i2c, buf, len);
	i2c->addr = tmp_dev_addr;
	mutex_unlock(&i2c_access);

	pr_info("serdes: i2c%d[0x%x] write[%d] -> 0x%02x 0x%02x 0x%02x 0x%02x %s [%s:%d]!\n",
		i2c->adapter->nr, dev_cmd->dev_addr, len, buf[0], buf[1], buf[2], buf[3],
		len > 3 ? "..." : "", ret == len ? "OK" : "FAIL", ret);

	return ret;
}

static int i2c_write_read_byte(struct i2c_client *i2c,
	u8 dev_addr, u16 reg_addr, u8 reg_width, u8 *val)
{
	int ret = 0;
	u8 buf[2] = {0};
	u8 len = 0;
	u8 tmp_dev_addr = 0;

	if (!i2c) {
		pr_info("serdes %s: i2c is NULL!\n", __func__);
		return -1;
	}

	if (reg_width == 16) {
		buf[0] = reg_addr >> 8;
		buf[1] = reg_addr & 0xFF;
		len = 2;
	} else if (reg_width == 8) {
		buf[0] = reg_addr;
		len = 1;
	} else {
		pr_info("%s: Invalid register width[%d]!\n", __func__, reg_width);
		return -1;
	}

	mutex_lock(&i2c_access);
	tmp_dev_addr = i2c->addr;
	i2c->addr = dev_addr;
	ret = i2c_master_send(i2c, buf, len);
	i2c->addr = tmp_dev_addr;
	mutex_unlock(&i2c_access);

	if (ret < 0) {
		pr_info("%s: i2c%d[0x%x] write 0x%02x [FAIL:%d]!\n", __func__,
			i2c->adapter->nr, dev_addr, reg_addr, ret);
		return ret;
	}

	mutex_lock(&i2c_access);
	tmp_dev_addr = i2c->addr;
	i2c->addr = dev_addr;
	ret = i2c_master_recv(i2c, val, 1);
	i2c->addr = tmp_dev_addr;
	mutex_unlock(&i2c_access);

	pr_info("serdes: i2c%d[0x%x] read[0x%x]->0x%02x [%s:%d]!\n",
		i2c->adapter->nr, dev_addr, reg_addr, *val, ret == 1 ? "OK" : "Fail", ret);

	return ret;
}

static int serdes_get_link_type_flag_from_dts(struct serdes_bridge *ser_des)
{
	int ret;
	u32 read_value = 0;

	if (!ser_des || !ser_des->config_node) {
		pr_info("%s: serdes or config_node is NULL!\n", __func__);
		return -1;
	}

	ret = of_property_read_u32(ser_des->config_node, SERDES_SUPERFRAME_FLAG_NODE_NAME, &read_value);
	if (!ret && read_value == 1) {
		ser_des->link_type = LINK_TYPE_SUPERFRAME;
		pr_info("[i2c%d] Serdes in superframe mode!\n", ser_des->client->adapter->nr);
		return 0;
	}

	ret = of_property_read_u32(ser_des->config_node, SERDES_MST_FLAG_NODE_NAME, &read_value);
	if (!ret && read_value == 1) {
		ser_des->link_type = LINK_TYPE_MST;
		pr_info("[i2c%d] Serdes in MST mode!\n", ser_des->client->adapter->nr);
		return 0;
	}
	ser_des->link_type = LINK_TYPE_SST;
	ret = of_property_read_u32(ser_des->config_node, SERDES_DUAL_LINK_FLAG_NODE_NAME, &read_value);
	if (!ret && read_value == 1) {
		ser_des->link_type = LINK_TYPE_DUAL_LINK;
		pr_info("[i2c%d] Serdes in DUAL link mode!\n", ser_des->client->adapter->nr);
		return 0;
	}
	ser_des->link_type = LINK_TYPE_SST;
	pr_info("[i2c%d] Serdes in SST mode!\n", ser_des->client->adapter->nr);

	return 0;
}

static int serdes_get_panel_timing_from_dts(struct serdes_bridge *ser_des)
{
	struct prop_map {
		const char *dts_name;
		size_t offset;
	};
	static const struct prop_map props[] = {
		{ PANEL_WIDTH_NODE_NAME, offsetof(struct vdo_timing, width) },
		{ PANEL_HEIGHT_NODE_NAME, offsetof(struct vdo_timing, height) },
		{ PANEL_HFP_NODE_NAME, offsetof(struct vdo_timing, hfp) },
		{ PANEL_HSA_NODE_NAME, offsetof(struct vdo_timing, hsa) },
		{ PANEL_HBP_NODE_NAME, offsetof(struct vdo_timing, hbp) },
		{ PANEL_VFP_NODE_NAME, offsetof(struct vdo_timing, vfp) },
		{ PANEL_VSA_NODE_NAME, offsetof(struct vdo_timing, vsa) },
		{ PANEL_VBP_NODE_NAME, offsetof(struct vdo_timing, vbp) },
		{ PANEL_FPS_NODE_NAME, offsetof(struct vdo_timing, fps) },
		{ PANEL_PLL_NODE_NAME, offsetof(struct vdo_timing, pll) },
		{ PANEL_DSI_PREFETCH_NODE_NAME, offsetof(struct vdo_timing, prefetch) },
		{ PANEL_WIDTH_MM_NODE_NAME, offsetof(struct vdo_timing, physcial_w) },
		{ PANEL_HEIGHT_MM_NODE_NAME, offsetof(struct vdo_timing, physcial_h) },
	};
	struct device_node *timing_node = NULL;
	static const char * const timing_name[] = {
		PANEL_TIMING_A_NODE_NAME,
		PANEL_TIMING_B_NODE_NAME
	};
	u32 i = 0, j = 0, read_times = 0;
	int ret = 0;

	if (!ser_des || !ser_des->config_node) {
		pr_info("%s: Invalid serdes or config node!\n", __func__);
		return -1;
	}

	pr_info("%s[i2c%d]: +\n", __func__, ser_des->client->adapter->nr);

	if (ser_des->link_type == LINK_TYPE_SUPERFRAME
		|| ser_des->link_type == LINK_TYPE_MST
		|| ser_des->link_type == LINK_TYPE_DUAL_LINK)
		read_times = 1;

	for (j = 0; j <= read_times; j++) {
		timing_node = of_find_node_by_name(ser_des->config_node, timing_name[j]);
		if (!timing_node) {
			pr_info("%s: no %s in %s!\n", __func__,
				timing_name[j], ser_des->config_node->name);
			return -1;
		}
		for (i = 0; i < ARRAY_SIZE(props); i++) {
			ret = of_property_read_u32(timing_node, props[i].dts_name,
				(u32 *)((u8 *)&ser_des->panel_timing[j] + props[i].offset));
			if (ret)
				pr_info("%s: no %s in %s!\n", __func__, props[i].dts_name, timing_name[j]);
		}
	}
	pr_info("%s[i2c%d]: -\n", __func__, ser_des->client->adapter->nr);

	return 0;
}

static int serdes_get_status_cmd_from_dts(struct serdes_bridge *ser_des,
	const char *type, struct serdes_status_cmd **sta_cmd)
{
	int len = 0;
	const __be32 *p = NULL;
	struct serdes_status_cmd *tmp_sta_cmd = NULL;

	if (!ser_des || !type) {
		pr_info("%s: Invalid serdes or type!\n", __func__);
		*sta_cmd = NULL;
		return -1;
	}
	p = of_get_property(ser_des->config_node, type, &len);
	if (!p) {
		pr_info("%s: no %s in %s!\n", __func__, type, ser_des->config_node->name);
		*sta_cmd = NULL;
		return -1;
	}

	len /= sizeof(u32);

	tmp_sta_cmd = devm_kzalloc(ser_des->dev, sizeof(struct serdes_status_cmd), GFP_KERNEL);
	if (!tmp_sta_cmd)
		return -1;

	tmp_sta_cmd->dev_addr = be32_to_cpu(*p++);
	tmp_sta_cmd->reg_width = be32_to_cpu(*p++);
	tmp_sta_cmd->reg_addr = be32_to_cpu(*p++);
	tmp_sta_cmd->mask = be32_to_cpu(*p++);
	tmp_sta_cmd->exp_data = be32_to_cpu(*p++);

	*sta_cmd = tmp_sta_cmd;

	return 0;
}

static bool serdes_get_serdes_status(struct serdes_bridge *ser_des,
	enum SERDES_STATUS_TYPE type)
{
	int ret = 0;
	u8 val = 0, tmp_addr = 0;
	struct serdes_status_cmd *sta_cmd = NULL;

	if (!ser_des || !ser_des->client) {
		pr_info("%s[i2c%d]: ser_des or i2c client is null!\n",
			__func__, ser_des->client->adapter->nr);
		return true;
	}

	sta_cmd = ser_des->sta_cmd[type];
	if (!sta_cmd) {
		pr_info("%s[i2c%d]: cmd[%s] is NULL!\n", __func__,
			ser_des->client->adapter->nr, status_node_name[type].name);
		return true;
	}

	tmp_addr = ser_des->client->addr;
	ser_des->client->addr = sta_cmd->dev_addr;
	ret = i2c_write_read_byte(ser_des->client, sta_cmd->dev_addr,
		sta_cmd->reg_addr, sta_cmd->reg_width, &val);
	ser_des->client->addr = tmp_addr;
	if (ret < 0)
		return false;

	return ((val & sta_cmd->mask) == sta_cmd->exp_data) ? true : false;
}

static int serdes_get_cmd_from_dts(struct serdes_bridge *ser_des,
		const char *cmd_name, struct serdes_config_cmd **cfg_cmd)
{
	const __be32 *p = NULL, *tmp_p = NULL;
	int len = 0, offset = 0, count = 0;
	struct serdes_config_cmd *tmp_cfg_cmd = NULL;
	u8 reg_width = 0;

	if (!ser_des || !cmd_name) {
		pr_info("%s: Invalid serdes or cmd_name!\n", __func__);
		return -1;
	}

	p = of_get_property(ser_des->config_node, cmd_name, &len);
	if (!p) {
		pr_info("%s: no %s in %s!\n", __func__, cmd_name, ser_des->config_node->name);
		*cfg_cmd = NULL;
		return -1;
	}

	len /= sizeof(u32);
	tmp_cfg_cmd = devm_kzalloc(ser_des->dev, sizeof(struct serdes_config_cmd), GFP_KERNEL);
	if (!tmp_cfg_cmd)
		return -1;

	tmp_p = p;
	while (offset < len) {
		tmp_p++;
		reg_width = be32_to_cpu(*tmp_p++);
		if (reg_width == 0) {
			offset += (3 + be32_to_cpu(*tmp_p));
			tmp_p += be32_to_cpu(*tmp_p);
		} else {
			offset += 5;
			tmp_p += 3;
		}
		count++;
	}

	tmp_cfg_cmd->dev_cmd = devm_kzalloc(ser_des->dev,
		sizeof(struct device_cmd *) * count, GFP_KERNEL);
	if (!tmp_cfg_cmd->dev_cmd)
		return -1;

	tmp_cfg_cmd->dev_cmd_num = count;
	count = 0;
	offset = 0;
	tmp_p = p;
	while (offset < len) {
		tmp_cfg_cmd->dev_cmd[count] = devm_kzalloc(ser_des->dev,
			sizeof(struct device_cmd), GFP_KERNEL);
		tmp_cfg_cmd->dev_cmd[count]->dev_addr = be32_to_cpu(*tmp_p++);
		tmp_cfg_cmd->dev_cmd[count]->reg_width = be32_to_cpu(*tmp_p++);
		if (tmp_cfg_cmd->dev_cmd[count]->reg_width == 0) {
			tmp_cfg_cmd->dev_cmd[count]->multi_i2c_cmd.len = be32_to_cpu(*tmp_p++);
			for (u32 i = 0; i < tmp_cfg_cmd->dev_cmd[count]->multi_i2c_cmd.len; i++)
				tmp_cfg_cmd->dev_cmd[count]->multi_i2c_cmd.data[i] = be32_to_cpu(*tmp_p++);
			offset += 3 + tmp_cfg_cmd->dev_cmd[count]->multi_i2c_cmd.len;
		} else {
			tmp_cfg_cmd->dev_cmd[count]->single_i2c_cmd.addr = be32_to_cpu(*tmp_p++);
			tmp_cfg_cmd->dev_cmd[count]->single_i2c_cmd.data = be32_to_cpu(*tmp_p++);
			tmp_cfg_cmd->dev_cmd[count]->single_i2c_cmd.delay_ms = be32_to_cpu(*tmp_p++);
			offset += 5;
		}
		count++;
	}

	*cfg_cmd = tmp_cfg_cmd;

	return 0;
}

static void serdes_send_cmd(struct serdes_bridge *ser_des, enum CMD_TYPE type)
{
	u32 i = 0;
	struct serdes_config_cmd *cfg_cmd = NULL;

	if (!ser_des) {
		pr_info("%s: Invalid serdes!\n", __func__);
		return;
	}
	pr_info("%s[i2c%d] +\n", __func__, ser_des->client->adapter->nr);
	if (!ser_des)
		return;

	cfg_cmd = ser_des->cfg_cmd[type];
	if (!cfg_cmd) {
		pr_info("%s[i2c%d]: no %s!\n", __func__,
			ser_des->client->adapter->nr, cmd_node_name[type].name);
		return;
	}

	if (cfg_cmd->dev_cmd_num) {
		for (i = 0; i < cfg_cmd->dev_cmd_num; i++) {
			i2c_write_byte(ser_des->client, cfg_cmd->dev_cmd[i]);
			if (cfg_cmd->dev_cmd[i]->reg_width && cfg_cmd->dev_cmd[i]->single_i2c_cmd.delay_ms)
				mdelay(cfg_cmd->dev_cmd[i]->single_i2c_cmd.delay_ms);
		}
	}

	pr_info("%s[i2c%d] -\n", __func__, ser_des->client->adapter->nr);
}

static int serdes_get_comp_config_type_cmd_from_dts(struct serdes_bridge *ser_des,
	struct comp_config_cmd **comp_cfg_cmd)
{
	int total_num = 0;
	u32 i = 0, elem_num = 0;
	const __be32 *p;
	struct comp_config_cmd *tmp_cfg_cmd = NULL;

	if (!ser_des || !ser_des->config_node) {
		pr_info("%s: Invalid serdes or config_node!\n", __func__);
		return -1;
	}
	p = of_get_property(ser_des->config_node, COMPATIBLE_TYPE_NODE_NAME, &total_num);
	if (!p) {
		pr_info("%s[i2c%d]: not compatible mode! [%s]!\n", __func__, ser_des->client->adapter->nr,
			ser_des->config_node->name);
		return -1;
	}

	total_num /= sizeof(u32);
	elem_num = total_num / NUM_OF_COMP_CONFIG_TYPE;

	tmp_cfg_cmd = devm_kzalloc(ser_des->dev,
			sizeof(struct comp_config_cmd), GFP_KERNEL);
	if (!tmp_cfg_cmd)
		return -1;

	tmp_cfg_cmd->cfg_type_cmd = devm_kzalloc(ser_des->dev,
		sizeof(struct comp_config_type_cmd *) * elem_num, GFP_KERNEL);
	if (!tmp_cfg_cmd->cfg_type_cmd)
		return -1;

	for (i = 0; i < elem_num; i++) {
		tmp_cfg_cmd->cfg_type_cmd[i] =  devm_kzalloc(ser_des->dev,
			sizeof(struct comp_config_type_cmd), GFP_KERNEL);
		if (!tmp_cfg_cmd->cfg_type_cmd[i])
			return -1;

		tmp_cfg_cmd->cfg_type_cmd[i]->dev_addr = be32_to_cpu(*p++);
		tmp_cfg_cmd->cfg_type_cmd[i]->reg_width = be32_to_cpu(*p++);
		tmp_cfg_cmd->cfg_type_cmd[i]->reg_addr = be32_to_cpu(*p++);
		tmp_cfg_cmd->cfg_type_cmd[i]->mask = be32_to_cpu(*p++);
		tmp_cfg_cmd->cfg_type_cmd[i]->exp_data = be32_to_cpu(*p++);
		tmp_cfg_cmd->cfg_type_cmd[i]->config_node = of_find_node_by_phandle(be32_to_cpu(*p++));
	}

	tmp_cfg_cmd->cfg_type_cmd_num = elem_num;
	*comp_cfg_cmd = tmp_cfg_cmd;

	return 0;
}

static int serdes_get_comp_config(struct serdes_bridge *ser_des)
{
	u8 val = 0;
	u32 i = 0;
	struct serdes_config_cmd *comp_cmd = NULL;
	struct comp_config_cmd *comp_cfg_cmd = NULL;

	if (!ser_des || !ser_des->client) {
		pr_info("%s[i2c%d]: ser_des or i2c client is null!\n", __func__, ser_des->client->adapter->nr);
		return -1;
	}

	comp_cfg_cmd = ser_des->comp_cfg_cmd;
	if (!comp_cfg_cmd) {
		pr_info("%s[i2c%d]: not compatible mode!\n", __func__,
			ser_des->client->adapter->nr);
		return -1;
	}

	serdes_get_cmd_from_dts(ser_des, COMPATIBLE_PRE_CMD_NODE_NAME, &comp_cmd);
	if (comp_cmd->dev_cmd_num) {
		for (i = 0; i < comp_cmd->dev_cmd_num; i++) {
			i2c_write_byte(ser_des->client, comp_cmd->dev_cmd[i]);
			if (comp_cmd->dev_cmd[i]->reg_width && comp_cmd->dev_cmd[i]->single_i2c_cmd.delay_ms)
				mdelay(comp_cmd->dev_cmd[i]->single_i2c_cmd.delay_ms);
		}
	}

	for (i = 0; i < comp_cfg_cmd->cfg_type_cmd_num; i++) {
		i2c_write_read_byte(ser_des->client, comp_cfg_cmd->cfg_type_cmd[i]->dev_addr,
			comp_cfg_cmd->cfg_type_cmd[i]->reg_addr, comp_cfg_cmd->cfg_type_cmd[i]->reg_width, &val);

		if ((val & comp_cfg_cmd->cfg_type_cmd[i]->mask) == comp_cfg_cmd->cfg_type_cmd[i]->exp_data) {
			ser_des->config_node = comp_cfg_cmd->cfg_type_cmd[i]->config_node;
			return 0;
		}
	}
	ser_des->config_node = comp_cfg_cmd->cfg_type_cmd[comp_cfg_cmd->cfg_type_cmd_num - 1]->config_node;
	return 0;
}

void serdes_reset_ser(struct serdes_bridge *ser_des)
{
	pr_info("%s[i2c%d] +\n", __func__, ser_des->client->adapter->nr);
	if (!ser_des || !ser_des->reset_gpio) {
		pr_info("%s: invalid serdes_bridge or reset_gpio\n", __func__);
		return;
	}

	gpiod_set_value(ser_des->reset_gpio, 1);
	msleep(50);
	gpiod_set_value(ser_des->reset_gpio, 0);
	msleep(50);
	gpiod_set_value(ser_des->reset_gpio, 1);
	msleep(50);
	pr_info("%s[i2c%d] -\n", __func__, ser_des->client->adapter->nr);
}

int serdes_get_link_status(struct serdes_bridge *ser_des)
{
	bool linka_status = false, linkb_status = false,
		linka_inited_status = false, linkb_inited_status = false;

	if (!ser_des) {
		pr_info("%s: Invalid serdes!\n", __func__);
		return -1;
	}
	linka_status = serdes_get_serdes_status(ser_des, LINKA_STATUS);
	linka_inited_status = serdes_get_serdes_status(ser_des, LINKA_INITED_STATUS);
	if (ser_des->link_type == LINK_TYPE_SUPERFRAME
		 || ser_des->link_type == LINK_TYPE_MST
		 || ser_des->link_type == LINK_TYPE_DUAL_LINK) {
		linkb_status = serdes_get_serdes_status(ser_des, LINKB_STATUS);
		linkb_inited_status = serdes_get_serdes_status(ser_des, LINKB_INITED_STATUS);
	}

	return linkb_inited_status << 3 |
			linka_inited_status << 2 |
			linkb_status << 1 |
			linka_status;
}

#if IS_ENABLED(CONFIG_ENABLE_SERDES_HOTPLUG)
#if ENABLE_HOTPLUG_INT
static irqreturn_t serdes_interrupt_handler(int irq, void *data)
{
	struct serdes_bridge *ser_des = (struct serdes_bridge *)data;

	if (!ser_des) {
		pr_info("%s ser_des is NULL\n", __func__);
		return -1;
	}

	pr_debug("%s: interrupt!\n", __func__);

	atomic_set(&ser_des->hotplug_event, 1);
	wake_up_interruptible(&ser_des->hotplug_wq);
	return IRQ_HANDLED;
}
#endif

static int serdes_hotplug_kthread(void *data)
{
	struct sched_param param = {.sched_priority = DEFAULT_PRIO};
	struct serdes_bridge *ser_des = (struct serdes_bridge *)data;
	int status = 0, reset_a = 0, reset_b = 0;

	sched_setscheduler(current, SCHED_NORMAL, &param);
	if (!ser_des) {
		pr_info("%s ser_des is NULL\n", __func__);
		return -1;
	}

	pr_info("%s[i2c%d] +\n", __func__, ser_des->client->adapter->nr);

	while (!kthread_should_stop()) {
		wait_event_interruptible_timeout(
				ser_des->hotplug_wq,
				atomic_read(&ser_des->hotplug_event), 2 * HZ);
		atomic_set(&ser_des->hotplug_event, 0);
#if ENABLE_HOTPLUG_INT
		if (ser_des->irq_num)
			disable_irq(ser_des->irq_num);
#endif
		if (ser_des->stop_thread) {
			ser_des->stop_thread = false;
			pr_info("%s[i2c%d]: -\n", __func__, ser_des->client->adapter->nr);
			return 0;
		}

		reset_a = reset_b = 0;
		status = serdes_get_link_status(ser_des);

		pr_info("%s:[i2c%d] serdes status=0x%x!\n", __func__,
			ser_des->client->adapter->nr, status);

		if ((status & 0x5) == 0x1)
			reset_a = 1;
		if ((status & 0xa) == 0x2)
			reset_b = 1;

		if ((ser_des->link_type == LINK_TYPE_SUPERFRAME
			 || ser_des->link_type == LINK_TYPE_MST
			 || ser_des->link_type == LINK_TYPE_DUAL_LINK)
			&& (reset_a || reset_b))
			serdes_send_cmd(ser_des, I2C_REMAP_CMD);
		if (reset_a)
			serdes_send_cmd(ser_des, LINKA_INIT_CMD);
		if (reset_b)
			serdes_send_cmd(ser_des, LINKB_INIT_CMD);

#if ENABLE_HOTPLUG_INT
		if (ser_des->irq_num)
			enable_irq(ser_des->irq_num);
#endif
	}
	pr_info("%s[i2c%d] -\n", __func__, ser_des->client->adapter->nr);
	return 0;
}
#endif

void serdes_poweron_ser(struct serdes_bridge *ser_des)
{
	int i = 0;

	pr_info("%s[i2c%d] +\n", __func__, ser_des->client->adapter->nr);
	if (!ser_des->power_en_gpio || !ser_des->power_en_gpios_count)
		return;
	for (i = 0; i < ser_des->power_en_gpios_count && ser_des->power_en_gpio[i]; i++)
		gpiod_set_value(ser_des->power_en_gpio[i], 1);
	pr_info("%s[i2c%d] -\n", __func__, ser_des->client->adapter->nr);
}

void serdes_poweroff_ser(struct serdes_bridge *ser_des)
{
	int i = 0;

	pr_info("%s[i2c%d] +\n", __func__, ser_des->client->adapter->nr);
	if (!ser_des->power_en_gpio || !ser_des->power_en_gpios_count)
		return;
	for (i = ser_des->power_en_gpios_count; (i > 0) && ser_des->power_en_gpio[i - 1]; i--)
		gpiod_set_value(ser_des->power_en_gpio[i - 1], 0);
	pr_info("%s[i2c%d] -\n", __func__, ser_des->client->adapter->nr);
}

#if ENALBE_INIT_WORK
void serdes_bridge_pre_init_work_fn(struct work_struct *work)
{
	struct serdes_bridge *serdes = container_of(work, struct serdes_bridge, serdes_pre_init_work);
	struct serdes_bridge *ser_des;

	if (!serdes || !serdes->driver_data) {
		pr_info("%s: serdes is NULL\n", __func__);
		return;
	}
	ser_des = serdes->driver_data->port ? serdes->master_serdes : serdes;
	if (!ser_des) {
		pr_info("%s ser_des is NULL\n", __func__);
		return;
	}

	pr_info("%s +\n", __func__);
	if (ser_des->driver_data->port == 1)
		ser_des->port1_pre_enabled = true;
	if (ser_des->driver_data->port == 0)
		ser_des->port0_pre_enabled = true;

	serdes_send_cmd(ser_des, PRE_INIT_CMD);
	serdes_send_cmd(ser_des, I2C_REMAP_CMD);
	serdes_send_cmd(ser_des, POST_INIT_CMD);

	complete(&ser_des->serdes_pre_init_complete);
	pr_info("%s -\n", __func__);
}

void serdes_bridge_init_work_fn(struct work_struct *work)
{
	struct serdes_bridge *serdes = container_of(work, struct serdes_bridge, serdes_init_work);
	struct serdes_bridge *ser_des;
	int port = 0;

	if (!serdes || !serdes->driver_data) {
		pr_info("%s: serdes is NULL\n", __func__);
		return;
	}
	ser_des = serdes->driver_data->port ? serdes->master_serdes : serdes;
	port = serdes->driver_data->port;
	if (!ser_des) {
		pr_info("%s ser_des is NULL\n", __func__);
		return;
	}
	if (ser_des->driver_data->port == 1)
		ser_des->port1_enabled = true;
	if (ser_des->driver_data->port == 0)
		ser_des->port0_enabled = true;

	pr_info("%s +\n", __func__);
	serdes_send_cmd(ser_des, (port == 0) ? LINKA_INIT_CMD : LINKB_INIT_CMD);
	complete(&ser_des->serdes_init_complete);
	pr_info("%s -\n", __func__);
}
#endif

void serdes_bridge_enable(struct drm_bridge *bridge)
{
	struct serdes_bridge *serdes = bridge_to_serdes(bridge);
	struct serdes_bridge *ser_des;
	int port = 0;

	if (!serdes || !serdes->driver_data) {
		pr_info("%s: serdes is NULL\n", __func__);
		return;
	}
	ser_des = serdes->driver_data->port ? serdes->master_serdes : serdes;
	port = serdes->driver_data->port;
	if (!ser_des) {
		pr_info("%s ser_des is NULL\n", __func__);
		return;
	}

	pr_info("%s[i2c%d]port%d + [%d/%d]\n", __func__,
		ser_des->client->adapter->nr, port,
		ser_des->port0_enabled, ser_des->port1_enabled);

	if (!serdes_get_serdes_status(ser_des, SER_STATUS)) {
		pr_info("%s: error: serdes[i2c%d] not connect!\n", __func__,
			ser_des->client->adapter->nr);
		return;
	}
	if (ser_des->port0_enabled && (port == 0))
		return;
	if (ser_des->port1_enabled && (port == 1))
		return;

#if ENALBE_INIT_WORK
	if (ser_des->driver_data->type == SERDES_FOR_EDP
		|| ser_des->driver_data->type == SERDES_FOR_DP) {
		reinit_completion(&ser_des->serdes_init_complete);
		schedule_work(&ser_des->serdes_init_work);
		wait_for_completion_timeout(&ser_des->serdes_init_complete, msecs_to_jiffies(1000));
	} else
#endif
		serdes_send_cmd(ser_des, (port == 0) ? LINKA_INIT_CMD : LINKB_INIT_CMD);

#if IS_ENABLED(CONFIG_ENABLE_SERDES_HOTPLUG)
	ser_des->stop_thread = false;
	if (ser_des->sta_cmd[LINKA_STATUS]) {
		pr_info("%s:[i2c%d] create hotplug thread!\n", __func__, ser_des->client->adapter->nr);
		ser_des->hotplug_task = kthread_run(serdes_hotplug_kthread, ser_des, "hotplug");
#if ENABLE_HOTPLUG_INT
		enable_irq(ser_des->irq_num);
#endif
	}
#endif
	if (port)
		ser_des->port1_enabled = true;
	else
		ser_des->port0_enabled = true;

	pr_info("%s[i2c%d] -[%d/%d]\n", __func__, ser_des->client->adapter->nr,
		ser_des->port0_enabled, ser_des->port1_enabled);
}

void serdes_bridge_pre_enable(struct drm_bridge *bridge)
{
	struct serdes_bridge *serdes = bridge_to_serdes(bridge);
	struct serdes_bridge *ser_des;
	int port = 0;

	if (!serdes || !serdes->driver_data) {
		pr_info("%s: serdes is NULL\n", __func__);
		return;
	}
	ser_des = serdes->driver_data->port ? serdes->master_serdes : serdes;
	port = serdes->driver_data->port;
	if (!ser_des) {
		pr_info("%s ser_des is NULL\n", __func__);
		return;
	}

	pr_info("%s[i2c%d]port%d + [%d/%d]\n", __func__,
		ser_des->client->adapter->nr, port,
		ser_des->port0_pre_enabled, ser_des->port1_pre_enabled);

	if (ser_des->port0_pre_enabled || ser_des->port1_pre_enabled) {
		if (port == 0)
			ser_des->port0_pre_enabled = true;
		if ((ser_des->link_type == LINK_TYPE_SUPERFRAME
			|| ser_des->link_type == LINK_TYPE_MST
			|| ser_des->link_type == LINK_TYPE_DUAL_LINK)
			&& port == 1)
			ser_des->port1_pre_enabled = true;
		return;
	}

	serdes_poweron_ser(ser_des);
	serdes_reset_ser(ser_des);
	if (!serdes_get_serdes_status(ser_des, SER_STATUS)) {
		pr_info("%s: error: serdes[i2c%d] not connect!\n", __func__,
			ser_des->client->adapter->nr);
		return;
	}

#if ENALBE_INIT_WORK
	if (ser_des->driver_data->type == SERDES_FOR_EDP
		|| ser_des->driver_data->type == SERDES_FOR_DP) {
		reinit_completion(&ser_des->serdes_pre_init_complete);
		schedule_work(&ser_des->serdes_pre_init_work);
		wait_for_completion_timeout(&ser_des->serdes_pre_init_complete, msecs_to_jiffies(1000));
	} else {
#endif
		serdes_send_cmd(ser_des, PRE_INIT_CMD);
		serdes_send_cmd(ser_des, I2C_REMAP_CMD);
		serdes_send_cmd(ser_des, POST_INIT_CMD);
#if ENALBE_INIT_WORK
	}
#endif

	if (port == 0)
		ser_des->port0_pre_enabled = true;
	if ((ser_des->link_type == LINK_TYPE_SUPERFRAME
		|| ser_des->link_type == LINK_TYPE_MST
		|| ser_des->link_type == LINK_TYPE_DUAL_LINK)
		&& port == 1)
		ser_des->port1_pre_enabled = true;

	ser_des->is_suspend = false;

	pr_info("%s[i2c%d] -[%d/%d]\n", __func__, ser_des->client->adapter->nr,
		ser_des->port0_pre_enabled, ser_des->port1_pre_enabled);
}

void serdes_bridge_disable(struct drm_bridge *bridge)
{
	struct serdes_bridge *serdes = bridge_to_serdes(bridge);
	struct serdes_bridge *ser_des;
	int port = 0;

	if (!serdes || !serdes->driver_data) {
		pr_info("%s: serdes is NULL\n", __func__);
		return;
	}
	ser_des = serdes->driver_data->port ? serdes->master_serdes : serdes;
	port = serdes->driver_data->port;
	if (!ser_des) {
		pr_info("%s ser_des is NULL\n", __func__);
		return;
	}

	pr_info("%s [i2c%d]port%d [%d/%d][%d/%d]+\n", __func__,
		ser_des->client->adapter->nr, port,
		ser_des->port0_pre_enabled, ser_des->port1_pre_enabled,
		ser_des->port0_enabled, ser_des->port1_enabled);

	if (!serdes_get_serdes_status(ser_des, SER_STATUS)) {
		pr_info("%s: error: serdes[i2c%d] not connect!\n", __func__,
			ser_des->client->adapter->nr);
		return;
	}
	if (port == 1) {
		ser_des->port1_enabled = false;
		ser_des->port1_pre_enabled = false;
	} else {
		ser_des->port0_enabled = false;
		ser_des->port0_pre_enabled = false;
	}

	serdes_send_cmd(ser_des, (port == 0) ? LINKA_DEINIT_CMD : LINKB_DEINIT_CMD);

	if (!ser_des->port1_enabled && !ser_des->port0_enabled) {
#if IS_ENABLED(CONFIG_ENABLE_SERDES_HOTPLUG)
#if ENABLE_HOTPLUG_INT
		if (ser_des->irq_num)
			disable_irq(ser_des->irq_num);
#endif
		if (!IS_ERR_OR_NULL(ser_des->hotplug_task)) {
			pr_info("%s:[i2c%d] stop hotplug thread!\n", __func__, ser_des->client->adapter->nr);
			ser_des->stop_thread = true;
			atomic_set(&ser_des->hotplug_event, 1);
			wake_up_interruptible(&ser_des->hotplug_wq);
			kthread_stop(ser_des->hotplug_task);
			ser_des->hotplug_task = NULL;
		}
#endif
		serdes_send_cmd(ser_des, DEINIT_CMD);
		gpiod_set_value(ser_des->reset_gpio, 0);
		serdes_poweroff_ser(ser_des);
	}
	pr_info("%s[i2c%d] -[%d/%d][%d/%d]\n", __func__, ser_des->client->adapter->nr,
		ser_des->port0_pre_enabled, ser_des->port1_pre_enabled,
		ser_des->port0_enabled, ser_des->port1_enabled);
}

int serdes_bridge_get_modes(struct drm_bridge *bridge, struct drm_connector *connector)
{
	struct serdes_bridge *serdes = bridge_to_serdes(bridge);
	struct serdes_bridge *ser_des;
	int port = 0;
	struct drm_display_mode mode, *mode1;

	if (!serdes || !serdes->driver_data) {
		pr_info("%s: serdes is NULL\n", __func__);
		return -1;
	}
	ser_des = serdes->driver_data->port ? serdes->master_serdes : serdes;
	if (!ser_des) {
		pr_info("%s ser_des is NULL\n", __func__);
		return -1;
	}

	port = serdes->driver_data->port;
	if (ser_des->link_type == LINK_TYPE_MST) {
		if (connector->name)
			pr_info("%s get mst mode from %s\n", __func__, connector->name);
		else {
			pr_info("%s connector name is null\n",  __func__);
			return -EINVAL;
		}

		if (!strncmp(connector->name, "DP-1", 4))
			port = 0;
		else if (!strncmp(connector->name, "DP-2", 4))
			port = 1;
		else
			return -EINVAL;
	}

	pr_info("%s[i2c%d] port[%d]+\n", __func__, ser_des->client->adapter->nr, port);
	if (connector) {

		mode.hdisplay = ser_des->panel_timing[port].width;
		mode.hsync_start = mode.hdisplay + ser_des->panel_timing[port].hfp;
		mode.hsync_end = mode.hsync_start + ser_des->panel_timing[port].hsa;
		mode.htotal = mode.hsync_end + ser_des->panel_timing[port].hbp;

		mode.vdisplay = ser_des->panel_timing[port].height;
		mode.vsync_start = mode.vdisplay + ser_des->panel_timing[port].vfp;
		mode.vsync_end = mode.vsync_start + ser_des->panel_timing[port].vsa;
		mode.vtotal = mode.vsync_end + ser_des->panel_timing[port].vbp;
		mode.clock = mode.htotal * mode.vtotal * ser_des->panel_timing[port].fps / 1000;

		pr_info("%s[i2c%d]: Display timing: %dx%d@%d [%d,%d,%d %d,%d,%d]\n", __func__,
			ser_des->client->adapter->nr,
			mode.hdisplay, mode.vdisplay, ser_des->panel_timing[port].fps,
			mode.hsync_start, mode.hsync_end, mode.htotal,
			mode.vsync_start, mode.vsync_end, mode.vtotal);
		mode1 = drm_mode_duplicate(connector->dev, &mode);
		if (!mode1)
			return 0;
		mode1->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
		drm_mode_set_name(mode1);
		drm_mode_probed_add(connector, mode1);

		connector->display_info.width_mm = ser_des->panel_timing[port].physcial_w;
		connector->display_info.height_mm = ser_des->panel_timing[port].physcial_h;
	}
	pr_info("%s[i2c%d] -\n", __func__, ser_des->client->adapter->nr);
	return 1;
}

static struct mipi_dsi_host *serdes_get_dsi_host(struct device_node *node)
{
	struct device_node *endpoint = NULL, *dsi_host_node = NULL;
	struct mipi_dsi_host *dsi_host = NULL;

	endpoint = of_graph_get_next_endpoint(node, NULL);
	if (!endpoint)
		return ERR_PTR(-ENODEV);

	dsi_host_node = of_graph_get_remote_port_parent(endpoint);
	of_node_put(endpoint);
	if (!dsi_host_node)
		return ERR_PTR(-ENODEV);

	dsi_host = of_find_mipi_dsi_host_by_node(dsi_host_node);
	of_node_put(dsi_host_node);
	if (IS_ERR_OR_NULL(dsi_host))
		return ERR_PTR(-EPROBE_DEFER);

	return dsi_host;
}

static int serdes_bridge_attach(struct drm_bridge *bridge,
	enum drm_bridge_attach_flags flags)
{
	struct serdes_bridge *ser_des = bridge_to_serdes(bridge);
	struct mipi_dsi_host *dsi_host;
	struct mipi_dsi_device *dsi;
	struct mipi_dsi_device_info info = {
		.type = "serdes",
		.channel = 0,
		.node = NULL,
	};
	int ret;

	pr_info("%s [i2c%d] +\n", __func__, ser_des->client->adapter->nr);

	if (!ser_des || !ser_des->driver_data) {
		pr_info("%s serdes is NULL\n", __func__);
		return -1;
	}

	if (ser_des->driver_data->type == SERDES_FOR_DSI) {
		dsi_host = serdes_get_dsi_host(ser_des->dev->of_node);
		if (IS_ERR(dsi_host)) {
			pr_info("%s: get dsi host fail[ret=%ld]!\n", __func__, PTR_ERR(dsi_host));
			return -1;
		}
		dsi = devm_mipi_dsi_device_register_full(ser_des->dev, dsi_host, &info);
		if (!dsi) {
			pr_info("%s: register dsi device fail: %ld!\n", __func__, PTR_ERR(dsi));
			return -1;
		}
		dsi->lanes = 4;
		dsi->format = MIPI_DSI_FMT_RGB888;
		dsi->mode_flags = MIPI_DSI_MODE_VIDEO;
		ret = devm_mipi_dsi_attach(ser_des->dev, dsi);
		if (ret < 0) {
			pr_info("%s: attach dsi fail!\n", __func__);
			return -1;
		}
		pr_info("%s [i2c%d]-\n", __func__, ser_des->client->adapter->nr);
		// return -1 to make dsi create the connector
		// for mtk_dsi only
		return -1;
	}

#if ENALBE_INIT_WORK
	INIT_WORK(&ser_des->serdes_pre_init_work, serdes_bridge_pre_init_work_fn);
	INIT_WORK(&ser_des->serdes_init_work, serdes_bridge_init_work_fn);
	init_completion(&ser_des->serdes_pre_init_complete);
	init_completion(&ser_des->serdes_init_complete);
	schedule_work(&ser_des->serdes_pre_init_work);
#else
	ser_des = ser_des->driver_data->port ? ser_des->master_serdes : ser_des;
	serdes_send_cmd(ser_des, PRE_INIT_CMD);
	serdes_send_cmd(ser_des, I2C_REMAP_CMD);
	serdes_send_cmd(ser_des, POST_INIT_CMD);
	if (ser_des->driver_data->port == 1)
		ser_des->port1_pre_enabled = true;
	if (ser_des->driver_data->port == 0)
		ser_des->port0_pre_enabled = true;
#endif
	pr_info("%s [i2c%d]-\n", __func__, ser_des->client->adapter->nr);

	return 0;
}

static int serdes_get_real_timing(struct serdes_bridge *serdes,
	struct drm_display_mode *mode)
{
	struct serdes_bridge *ser_des = serdes->driver_data->port ? serdes->master_serdes : serdes;
	int port = serdes->driver_data->port;

	if (serdes->link_type == LINK_TYPE_SUPERFRAME) {
		mode->hdisplay = ser_des->panel_timing[0].width
			+ ser_des->panel_timing[1].width;
		mode->hsync_start = mode->hdisplay +
			ser_des->panel_timing[0].hfp + ser_des->panel_timing[1].hfp;
		mode->hsync_end = mode->hsync_start
			+ ser_des->panel_timing[0].hsa + ser_des->panel_timing[1].hsa;
		mode->htotal = mode->hsync_end
			+ ser_des->panel_timing[0].hbp + ser_des->panel_timing[1].hbp;

		mode->vdisplay = ser_des->panel_timing[0].height;
		mode->vsync_start = mode->vdisplay + ser_des->panel_timing[0].vfp;
		mode->vsync_end = mode->vsync_start + ser_des->panel_timing[0].vsa;
		mode->vtotal = mode->vsync_end + ser_des->panel_timing[0].vbp;

		mode->clock = mode->htotal * mode->vtotal * ser_des->panel_timing[0].fps / 1000;
	} else {
		mode->hdisplay = ser_des->panel_timing[port].width;
		mode->hsync_start = mode->hdisplay + ser_des->panel_timing[port].hfp;
		mode->hsync_end = mode->hsync_start + ser_des->panel_timing[port].hsa;
		mode->htotal = mode->hsync_end + ser_des->panel_timing[port].hbp;

		mode->vdisplay = ser_des->panel_timing[port].height;
		mode->vsync_start = mode->vdisplay + ser_des->panel_timing[port].vfp;
		mode->vsync_end = mode->vsync_start + ser_des->panel_timing[port].vsa;
		mode->vtotal = mode->vsync_end + ser_des->panel_timing[port].vbp;

		mode->clock = mode->htotal * mode->vtotal * ser_des->panel_timing[port].fps / 1000;
	}

	pr_info("%s[i2c%d]: Real timing: %dx%d@%d [%d,%d,%d %d,%d,%d]\n", __func__,
		ser_des->client->adapter->nr,
		mode->hdisplay, mode->vdisplay, ser_des->panel_timing[0].fps,
		mode->hsync_start, mode->hsync_end, mode->htotal,
		mode->vsync_start, mode->vsync_end, mode->vtotal);
	return 0;
}

static int serdes_panel_get_real_vdo_timing(struct drm_panel *panel,
	struct drm_display_mode *mode)
{
	struct serdes_bridge *serdes = panel_to_serdes(panel);

	if (!serdes) {
		pr_info("%s: serdes is NULL\n", __func__);
		return -1;
	}

	serdes_get_real_timing(serdes, mode);
	return 0;
}

static int serdes_panel_get_link_status(struct drm_panel *panel)
{
	struct serdes_bridge *ser_des = panel_to_serdes(panel);

	pr_info("%s +\n", __func__);
	return serdes_get_link_status(ser_des->driver_data->port
		? ser_des->master_serdes : ser_des);
	pr_info("%s -\n", __func__);
}

static const struct drm_bridge_funcs serdes_bridge_funcs = {
	.attach = serdes_bridge_attach,
	.pre_enable = serdes_bridge_pre_enable,
	.enable = serdes_bridge_enable,
	.disable = serdes_bridge_disable,
	.get_modes = serdes_bridge_get_modes,
};

static struct mtk_panel_funcs ext_funcs = {
	.get_real_vdo_timing = serdes_panel_get_real_vdo_timing,
	.get_link_status = serdes_panel_get_link_status,
};

static int serdes_panel_unprepare(struct drm_panel *panel)
{
	struct serdes_bridge *ser_des = panel_to_serdes(panel);

	pr_info("%s[i2c%d] +\n", __func__, ser_des->client->adapter->nr);

	pr_info("%s[i2c%d] -\n", __func__, ser_des->client->adapter->nr);
	return 0;
}

static int serdes_panel_prepare(struct drm_panel *panel)
{
	struct serdes_bridge *ser_des = panel_to_serdes(panel);

	pr_info("%s[i2c%d] +\n", __func__, ser_des->client->adapter->nr);
	serdes_bridge_pre_enable(&ser_des->bridge);
	pr_info("%s[i2c%d] -\n", __func__, ser_des->client->adapter->nr);
	return 0;
}

static int serdes_panel_disable(struct drm_panel *panel)
{
	struct serdes_bridge *ser_des = panel_to_serdes(panel);

	pr_info("%s[i2c%d] +\n", __func__, ser_des->client->adapter->nr);
	serdes_bridge_disable(&ser_des->bridge);
	pr_info("%s[i2c%d] -\n", __func__, ser_des->client->adapter->nr);
	return 0;
}

static int serdes_panel_enable(struct drm_panel *panel)
{
	struct serdes_bridge *ser_des = panel_to_serdes(panel);

	pr_info("%s[i2c%d] +\n", __func__, ser_des->client->adapter->nr);
	serdes_bridge_enable(&ser_des->bridge);
	pr_info("%s[i2c%d] -\n", __func__, ser_des->client->adapter->nr);
	return 0;
}

static int serdes_panel_get_modes(struct drm_panel *panel,
	struct drm_connector *connector)
{
	struct serdes_bridge *ser_des = panel_to_serdes(panel);

	pr_info("%s[i2c%d] +\n", __func__, ser_des->client->adapter->nr);
	serdes_bridge_get_modes(&ser_des->bridge, connector);
	pr_info("%s[i2c%d] -\n", __func__, ser_des->client->adapter->nr);
	return 0;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = serdes_panel_disable,
	.unprepare = serdes_panel_unprepare,
	.prepare = serdes_panel_prepare,
	.enable = serdes_panel_enable,
	.get_modes = serdes_panel_get_modes,
};

static unsigned int serdes_get_lk_display_flag(void)
{
	struct tag_videolfb {
		u64 fb_base;
		u32 islcmfound;
		u32 fps;
		u32 vram;
		u32 primary_vram;
		char lcmname[1]; /* this is the minimum size */
	};
	struct device_node *chosen_node;

	chosen_node = of_find_node_by_path("/chosen");
	if (chosen_node) {
		struct tag_videolfb *videolfb_tag = NULL;
		unsigned long size = 0;

		videolfb_tag = (struct tag_videolfb *)of_get_property(
			chosen_node,
			"atag,videolfb",
			(int *)&size);
		if (videolfb_tag) {
			pr_info("%s: videolfb_tag->islcmfound:0x%x\n", __func__,
					((videolfb_tag->islcmfound >> 16) & 0xFFFF));
			return ((videolfb_tag->islcmfound >> 16) & 0xFFFF);
		}
	} else
		pr_info("[DT][videolfb] of_chosen not found\n");

	return 0;
}
static const struct serdes_driver_data serdes_dsi0_data = {
	.port = 0,
	.type = SERDES_FOR_DSI,
	.init_flag_mask = (1 << 0),
};
static const struct serdes_driver_data serdes_dsi1_data = {
	.port = 0,
	.type = SERDES_FOR_DSI,
	.init_flag_mask = (1 << 4),
};
static const struct serdes_driver_data serdes_dsi2_data = {
	.port = 0,
	.type = SERDES_FOR_DSI,
	.init_flag_mask = (1 << 5),
};
static const struct serdes_driver_data serdes_dsi_data_virtual = {
	.port = 1,
	.type = SERDES_FOR_DSI,
};

static const struct serdes_driver_data serdes_edp_data = {
	.port = 0,
	.type = SERDES_FOR_EDP,
	.init_flag_mask = (1 << 3),
};
static const struct serdes_driver_data serdes_edp_data_virtual = {
	.port = 1,
	.type = SERDES_FOR_EDP,
};

static const struct serdes_driver_data serdes_dp_data = {
	.port = 0,
	.type = SERDES_FOR_DP,
	.init_flag_mask = (1 << 1),
};

static const struct of_device_id serdes_iic_match[] = {
	{.compatible = "maxiam,max96789,dsi0", .data = &serdes_dsi0_data},
	{.compatible = "maxiam,max96789,dsi1", .data = &serdes_dsi1_data},
	{.compatible = "maxiam,max96789,dsi2", .data = &serdes_dsi2_data},
	{.compatible = "maxiam,max96789,dsi,virtual", .data = &serdes_dsi_data_virtual},
	{.compatible = "maxiam,max96851,edp", .data = &serdes_edp_data},
	{.compatible = "maxiam,max96851,edp,virtual", .data = &serdes_edp_data_virtual},
	{.compatible = "maxiam,max96851,dp", .data = &serdes_dp_data},
	{},
};

static int serdes_iic_driver_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct serdes_bridge *ser_des = NULL;
	const struct of_device_id *of_id = NULL;
	struct device_node *master_node = NULL;
	struct drm_bridge *bridge = NULL;
	struct drm_display_mode mode = {0};
	u32 i, init_flag;
	int ret;

	pr_info("%s[i2c%d]+:\n", __func__, client->adapter->nr);

	ser_des = devm_kzalloc(dev, sizeof(struct serdes_bridge), GFP_KERNEL);
	if (!ser_des)
		return -ENOMEM;

	ser_des->dev = dev;
	ser_des->client = client;
	of_id = of_match_device(serdes_iic_match, dev);
	if (!of_id) {
		pr_info("%s[i2c%d]:SERDES device match failed!\n", __func__, client->adapter->nr);
		return -1;
	}
	ser_des->driver_data = (struct serdes_driver_data *)of_id->data;
	pr_info("%s[i2c%d]:for %s, [%d/%d]\n", __func__, client->adapter->nr,
		of_id->compatible, ser_des->driver_data->type, ser_des->driver_data->port);

	if (ser_des->driver_data->port == 0) {
		ser_des->config_node = of_parse_phandle(dev->of_node, CONFIG_NODE_NAME, 0);
		if (ser_des->config_node < 0) {
			pr_info("%s: ERROR: no config node in [%s] !!\n", __func__, dev->of_node->name);
			return -1;
		}

		ser_des->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
		if (IS_ERR(ser_des->reset_gpio)) {
			pr_info("[i2c%d] ERROR: no reset-gpios [%ld]!!\n", client->adapter->nr,
				PTR_ERR(ser_des->reset_gpio));
			return -1;
		}

		ser_des->power_en_gpios_count =
			of_property_count_elems_of_size(dev->of_node, "power-en-gpios", sizeof(u32) * 3);
		if (ser_des->power_en_gpios_count < 0) {
			pr_info("%s: no power-en-gpios in [%s] !!\n", __func__, dev->of_node->name);
			ser_des->power_en_gpios_count = 0;
		} else {
			ser_des->power_en_gpio = devm_kcalloc(dev,
				ser_des->power_en_gpios_count, sizeof(struct gpio_desc *), GFP_KERNEL);
			if (!ser_des->power_en_gpio)
				return -ENOMEM;

			for (i = 0; i < ser_des->power_en_gpios_count; i++) {
				ser_des->power_en_gpio[i] = devm_gpiod_get_index(dev, "power-en", i, GPIOD_OUT_HIGH);
				if (IS_ERR(ser_des->power_en_gpio[i])) {
					pr_info("[i2c%d] Fail to get power-en-gpios[%d] %ld!\n", client->adapter->nr,
						i, PTR_ERR(ser_des->power_en_gpio));
					ser_des->power_en_gpio[i] = NULL;
				}
				if (ser_des->power_en_gpio[i])
					pr_info("[i2c%d] power-en-gpio[%d] = %d\n",
						client->adapter->nr, i, desc_to_gpio(ser_des->power_en_gpio[i]));
			}
		}

		init_flag = serdes_get_lk_display_flag();
		ser_des->inited_in_lk = init_flag ? init_flag & ser_des->driver_data->init_flag_mask : 0;
		pr_info("%s: %s %s in lk!\n", __func__, of_id->compatible,
			ser_des->inited_in_lk ? "inited" : "not init");
		if (!serdes_get_comp_config_type_cmd_from_dts(ser_des, &ser_des->comp_cfg_cmd)) {
			if (!ser_des->inited_in_lk) {
				serdes_poweron_ser(ser_des);
				serdes_reset_ser(ser_des);
			}
			serdes_get_comp_config(ser_des);
		}
		serdes_get_link_type_flag_from_dts(ser_des);
		serdes_get_panel_timing_from_dts(ser_des);

		for (i = 0; i < MAX_STATUS; i++)
			serdes_get_status_cmd_from_dts(ser_des, status_node_name[i].name, &ser_des->sta_cmd[i]);

		for (i = 0; i < MAX_CONFIG_CMD; i++)
			serdes_get_cmd_from_dts(ser_des, cmd_node_name[i].name, &ser_des->cfg_cmd[i]);

	#if IS_ENABLED(CONFIG_ENABLE_SERDES_HOTPLUG)
	#if ENABLE_HOTPLUG_INT
		ser_des->irq_num = irq_of_parse_and_map(dev->of_node, 0);
		pr_info("%s[i2c%d]: irq_num=%d!\n", __func__, client->adapter->nr, ser_des->irq_num);
		if (ser_des->irq_num) {
			if (request_irq(ser_des->irq_num, serdes_interrupt_handler,
				IRQF_TRIGGER_RISING, "serdes int", ser_des) != 0) {
				pr_info("%s[i2c%d]: failed to request irq!\n", __func__, client->adapter->nr);
				return -EBUSY;
			}
			disable_irq(ser_des->irq_num);
		}
	#endif
		atomic_set(&ser_des->hotplug_event, 0);
		init_waitqueue_head(&ser_des->hotplug_wq);
		ser_des->stop_thread = false;
	#endif
		if (ser_des->inited_in_lk) {
			ser_des->port0_enabled = true;
			ser_des->port0_pre_enabled = true;
			if (ser_des->link_type == LINK_TYPE_SUPERFRAME) {
				ser_des->port1_enabled = true;
				ser_des->port1_pre_enabled = true;
			} else if (ser_des->link_type == LINK_TYPE_DUAL_LINK)
				ser_des->port1_pre_enabled = true;
	#if IS_ENABLED(CONFIG_ENABLE_SERDES_HOTPLUG)
			if (!serdes_get_serdes_status(ser_des, SER_STATUS) && ser_des->sta_cmd[LINKA_STATUS]) {
				ser_des->hotplug_task = kthread_run(serdes_hotplug_kthread, ser_des, "hotplug");
	#if ENABLE_HOTPLUG_INT
				if (ser_des->irq_num)
					enable_irq(ser_des->irq_num);
	#else
				atomic_set(&ser_des->hotplug_event, 1);
				wake_up_interruptible(&ser_des->hotplug_wq);
	#endif
			}
	#endif
		}
	} else {
		master_node = of_parse_phandle(dev->of_node, MASTER_NODE_NAME, 0);
		if (!master_node) {
			pr_info("%s[i2c%d]: ERROR: master node is empty!\n", __func__, client->adapter->nr);
			return -1;
		}
		bridge = of_drm_find_bridge(master_node);
		if (!bridge) {
			pr_info("%s[i2c%d]: waiting for PHY bridge!\n", __func__, client->adapter->nr);
			return -EPROBE_DEFER;
		}
		ser_des->master_serdes = bridge_to_serdes(bridge);
		ser_des->link_type = ser_des->master_serdes->link_type;
	}

	if (ser_des->driver_data->type == SERDES_FOR_DSI) {
		serdes_get_real_timing(ser_des, &mode);
		if (ser_des->link_type == LINK_TYPE_SUPERFRAME) {
			ser_des->panel_data.ext_params.crop_width[0] = ser_des->panel_timing[0].width;
			ser_des->panel_data.ext_params.crop_width[1] = ser_des->panel_timing[1].width;
			ser_des->panel_data.ext_params.crop_height[0] = ser_des->panel_timing[0].height;
			ser_des->panel_data.ext_params.crop_height[1] = ser_des->panel_timing[1].height;
		} else {
			ser_des->panel_data.ext_params.crop_width[0] = 0;
			ser_des->panel_data.ext_params.crop_width[1] = 0;
			ser_des->panel_data.ext_params.crop_height[0] = 0;
			ser_des->panel_data.ext_params.crop_height[1] = 0;
		}

		ser_des->panel_data.ext_params.pll_clk =
			ser_des->panel_timing[ser_des->driver_data->port].pll ?
			ser_des->panel_timing[ser_des->driver_data->port].pll :
			DIV_ROUND_UP_ULL(mode.clock * 3, 1000);
		pr_info("%s: pll_clk = %d\n", __func__, ser_des->panel_data.ext_params.pll_clk);
		ser_des->panel_data.ext_params.prefetch_time =
			ser_des->panel_timing[ser_des->driver_data->port].prefetch;

		ser_des->panel_data.ext_params.vdo_keep_hs_perline = 1;

		drm_panel_init(&ser_des->panel_data.panel,
			dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);
		drm_panel_add(&ser_des->panel_data.panel);
		ret = mtk_panel_ext_create(dev, &ser_des->panel_data.ext_params,
			&ext_funcs, &ser_des->panel_data.panel);
		if (ret < 0) {
			pr_info("%s[i2c%d] create panel_ext error!\n", __func__, client->adapter->nr);
			return ret;
		}
	}

	ser_des->bridge.driver_private = &ser_des->panel_data;
	ser_des->bridge.funcs = &serdes_bridge_funcs;
	if (ser_des->driver_data->type == SERDES_FOR_EDP)
		ser_des->bridge.type = DRM_MODE_CONNECTOR_eDP;
	if (ser_des->driver_data->type == SERDES_FOR_DP)
		ser_des->bridge.type = DRM_MODE_CONNECTOR_DisplayPort;
	if (ser_des->driver_data->type == SERDES_FOR_DP ||
		ser_des->driver_data->type == SERDES_FOR_EDP)
		ser_des->bridge.ops = DRM_BRIDGE_OP_MODES;
	ser_des->bridge.of_node = dev->of_node;
	drm_bridge_add(&ser_des->bridge);

	i2c_set_clientdata(client, ser_des);

	pr_info("%s[i2c%d]-\n", __func__, client->adapter->nr);
	return 0;
}

static void serdes_iic_driver_remove(struct i2c_client *client)
{
	struct serdes_bridge *ser_des = i2c_get_clientdata(client);

#if IS_ENABLED(CONFIG_ENABLE_SERDES_HOTPLUG)
#if ENABLE_HOTPLUG_INT
	if (ser_des->irq_num)
		free_irq(ser_des->irq_num, ser_des);
#endif
#endif
	if (ser_des->driver_data->type == SERDES_FOR_DSI) {
		struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ser_des->panel_data.panel);

		drm_panel_remove(&ser_des->panel_data.panel);
		mtk_panel_detach(ext_ctx);
		mtk_panel_remove(ext_ctx);
	}
}

#ifdef CONFIG_PM_SLEEP
static int serdes_suspend(struct device *dev)
{
	struct serdes_bridge *ser_des = dev->driver_data;

	pr_info("%s[i2c%d] +\n", __func__, ser_des->client->adapter->nr);
	if (ser_des->is_suspend) {
		pr_info("%s already suspend\n", __func__);
		return 0;
	}

	if (ser_des->driver_data->type == SERDES_FOR_DP ||
		ser_des->driver_data->type == SERDES_FOR_EDP) {
		gpiod_set_value(ser_des->reset_gpio, 0);
		serdes_poweroff_ser(ser_des);
	}

	ser_des->is_suspend = true;
	pr_info("%s[i2c%d] -\n", __func__, ser_des->client->adapter->nr);

	return 0;
}

static int serdes_resume(struct device *dev)
{
	struct serdes_bridge *ser_des = dev->driver_data;

	pr_info("%s[i2c%d] +\n", __func__, ser_des->client->adapter->nr);
	if (!ser_des->is_suspend) {
		pr_info("%s already resume\n", __func__);
		return 0;
	}

	if (ser_des->driver_data->type == SERDES_FOR_DP ||
		ser_des->driver_data->type == SERDES_FOR_EDP) {
		serdes_poweron_ser(ser_des);
		serdes_reset_ser(ser_des);
#if ENALBE_INIT_WORK
		reinit_completion(&ser_des->serdes_pre_init_complete);
		schedule_work(&ser_des->serdes_pre_init_work);
		wait_for_completion_timeout(&ser_des->serdes_pre_init_complete, msecs_to_jiffies(1000));
#else
		if (ser_des->driver_data->port == 1)
			ser_des->port1_pre_enabled = true;
		if (ser_des->driver_data->port == 0)
			ser_des->port0_pre_enabled = true;

		serdes_send_cmd(ser_des, PRE_INIT_CMD);
		serdes_send_cmd(ser_des, I2C_REMAP_CMD);
		serdes_send_cmd(ser_des, POST_INIT_CMD);
#endif
	}

	ser_des->is_suspend = false;
	pr_info("%s[i2c%d] -\n", __func__, ser_des->client->adapter->nr);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(serdes_pm_ops, serdes_suspend, serdes_resume);
MODULE_DEVICE_TABLE(of, serdes_iic_match);

static struct i2c_driver serdes_iic_driver = {
	.driver = {
		.name = "maxiam,serdes",
		.of_match_table = serdes_iic_match,
		.pm = &serdes_pm_ops,
	},
	.probe = serdes_iic_driver_probe,
	.remove = serdes_iic_driver_remove,
};

#if !IS_ENABLED(CONFIG_MTK_YOCTO)
module_i2c_driver(serdes_iic_driver);
#else
static int serdes_init_kthread(void *data)
{
	pr_info("%s+\n", __func__);
	return i2c_add_driver(&serdes_iic_driver);
}
static int __init serdes_iic_init(void)
{
	struct task_struct *serdes_init_task = NULL;

	pr_info("%s+\n", __func__);

	serdes_init_task = kthread_run(serdes_init_kthread, NULL,
					"serdes_init_kthread");
	if (IS_ERR(serdes_init_task)) {
		pr_info("%s: Failed to create serdes_init_kthread\n", __func__);
		return PTR_ERR(serdes_init_task);
	}

	pr_info("%s-\n", __func__);

	return 0;
}
module_init(serdes_iic_init);

static void __exit serdes_iic_exit(void)
{
	i2c_del_driver(&serdes_iic_driver);
}
module_exit(serdes_iic_exit);
#endif

MODULE_AUTHOR("Henry Tu <henry.tu@mediatek.com>");
MODULE_DESCRIPTION("dsi/dp/edp serdes bridge driver");
MODULE_LICENSE("GPL");
