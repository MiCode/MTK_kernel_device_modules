// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "./maxiam-max96851.h"

static char *global_panel_dp_name;
static char *global_panel_dp_mode;
static char *global_panel_edp_name;
static char *global_panel_edp_mode;

static const char * const props[] = {DES_I2C_ADDR, BL_I2C_ADDR, DES_LINKA_I2C_ADDR, DES_LINKA_BL_I2C_ADDR,
						DES_LINKB_I2C_ADDR, DES_LINKB_BL_I2C_ADDR};
static const char * const init_cmds[] = {SERDES_INIT_LINKA_CMD, DES_LINKA_INIT_CMD, SERDES_INIT_LINKB_CMD,
								DES_LINKB_INIT_CMD, SER_SUPERFRAME_INIT_CMD};

static DEFINE_MUTEX(edp_i2c_access);

int get_panel_name_and_mode(char *panel_name, char *panel_mode, bool is_dp)
{
	if (is_dp) {
		if (!global_panel_dp_name || !global_panel_dp_mode ||
			!panel_name || !panel_mode)
			return -EINVAL;

		strscpy(panel_name, global_panel_dp_name, PANEL_NAME_SIZE);
		strscpy(panel_mode, global_panel_dp_mode, PANEL_NAME_SIZE);
	} else {
		if (!global_panel_edp_name || !global_panel_edp_mode ||
			!panel_name || !panel_mode)
			return -EINVAL;

		strscpy(panel_name, global_panel_edp_name, PANEL_NAME_SIZE);
		strscpy(panel_mode, global_panel_edp_mode, PANEL_NAME_SIZE);
	}

	return 0;
}
EXPORT_SYMBOL(get_panel_name_and_mode);

static u8 max96851_read_byte(struct i2c_client *client,
								u16 addr)
{
	u8 buf;
	int ret = 0;
	char read_data[2] = { 0 };

	read_data[0] = addr >> 8;
	read_data[1] = addr & 0xFF;

	mutex_lock(&edp_i2c_access);

	ret = i2c_master_send(client, read_data, 2);
	if (ret <= 0) {
		mutex_unlock(&edp_i2c_access);
		pr_info("[MAX96851] Failed to send i2c command, ret = %d\n", ret);
		return ret;
	}

	ret = i2c_master_recv(client, &buf, 1);
	if (ret <= 0) {
		mutex_unlock(&edp_i2c_access);
		pr_info("[MAX96851] Failed to recv i2c data, ret = %d\n", ret);
		return -1;
	}

	mutex_unlock(&edp_i2c_access);

	return buf;
}

static int max96851_write_byte(struct i2c_client *client, u16 addr,
								unsigned char val)
{
	char write_data[3] = { 0 };
	int ret;

	if (!client) {
		pr_info("[max96851] %s: client is NULL\n", __func__);
		return -1;
	}

	mutex_lock(&edp_i2c_access);

	write_data[0] = addr >> 8;
	write_data[1] = addr & 0xFF;
	write_data[2] = val;

	ret = i2c_master_send(client, write_data, 3);
	if (ret <= 0) {
		mutex_unlock(&edp_i2c_access);
		pr_info("[MAX96851] I2C write fail, addr:0x%x val:0x%x ret:%d\n", addr, val, ret);
		return ret;
	}

	mutex_unlock(&edp_i2c_access);

	return 0;
}

static inline struct max96851_bridge *
			bridge_to_max96851(struct drm_bridge *bridge)
{
	return container_of(bridge, struct max96851_bridge, bridge);
}

static int max96851_create_i2c_client(struct max96851_bridge *max_bridge)
{
	u16 addr = 0;

	addr = (u16)max_bridge->des_i2c_addr;
	max_bridge->max96752_i2c = i2c_new_dummy_device(max_bridge->max96851_i2c->adapter, addr);

	addr = (u16)max_bridge->bl_i2c_addr;
	max_bridge->bl_i2c = i2c_new_dummy_device(max_bridge->max96851_i2c->adapter, addr);

	if (!max_bridge->max96752_i2c || !max_bridge->bl_i2c) {
		pr_info("Failed to create i2c client for max96752 or BL!\n");
		return -ENODEV;
	}

	if (max_bridge->superframe_support) {
		addr = (u16)max_bridge->linka_i2c_addr;
		max_bridge->max96752_linka_i2c = i2c_new_dummy_device(max_bridge->max96851_i2c->adapter, addr);
		addr = (u16)max_bridge->linka_bl_addr;
		max_bridge->max96752_linka_bl_i2c = i2c_new_dummy_device(max_bridge->max96851_i2c->adapter, addr);
		addr = (u16)max_bridge->linkb_i2c_addr;
		max_bridge->max96752_linkb_i2c = i2c_new_dummy_device(max_bridge->max96851_i2c->adapter, addr);
		addr = (u16)max_bridge->linkb_bl_addr;
		max_bridge->max96752_linkb_bl_i2c = i2c_new_dummy_device(max_bridge->max96851_i2c->adapter, addr);
	} else if (max_bridge->dual_link_support) {
		pr_info("[MAX96851] dual link support\n");
	} else if (max_bridge->is_support_mst) {
		pr_info("[MAX96851] DP MST support\n");
	}

	return 0;
}

static void reset_ser(struct max96851_bridge *max_bridge)
{
	gpiod_set_value(max_bridge->gpio_rst_n, 1);
	msleep(50);
	gpiod_set_value(max_bridge->gpio_rst_n, 0);
	msleep(50);
	gpiod_set_value(max_bridge->gpio_rst_n, 1);
	msleep(50);
}

static void ser_init_loop(struct max96851_bridge *max_bridge, struct serdes_cmd_info *cmd_data)
{
	int i, ret;

	for (i = 0; i < cmd_data->length; i++) {
		ret = max96851_write_byte(max_bridge->max96851_i2c, cmd_data->addr[i], cmd_data->data[i]);
		if (ret) {
			pr_info("[MAX96851] Write ser command failed, addr=0x%x data=0x%x ret: %d\n",
					cmd_data->addr[i], cmd_data->data[i], ret);
		}

		if (cmd_data->delay_ms[i])
			msleep(cmd_data->delay_ms[i]);
	}
}

static void des_init_loop(struct max96851_bridge *max_bridge, struct serdes_cmd_info *cmd_data)
{
	int i, ret;

	for (i = 0; i < cmd_data->length; i++) {
		ret = max96851_write_byte(max_bridge->max96752_i2c, cmd_data->addr[i], cmd_data->data[i]);
		if (ret) {
			pr_info("[MAX96851] Write des command failed, addr=0x%x data=0x%x ret: %d\n",
					cmd_data->addr[i], cmd_data->data[i], ret);
		}

		if (cmd_data->delay_ms[i])
			msleep(cmd_data->delay_ms[i]);
	}
}

static void serdes_init_loop(struct max96851_bridge *max_bridge, struct serdes_cmd_info *cmd_data)
{
	int i, ret;
	struct i2c_client *client = NULL;

	for (i = 0; i < cmd_data->length; i++) {
		switch (cmd_data->obj[i]) {
		case 0:
			client = max_bridge->max96851_i2c;
			break;
		case 1:
			client = max_bridge->max96752_i2c;
			break;
		case 2:
			client = max_bridge->max96752_linka_i2c;
			break;
		case 3:
			client = max_bridge->max96752_linkb_i2c;
			break;
		default:
			pr_info("[MAX96851] Invalid obj id: %d\n", cmd_data->obj[i]);
			return;
		}
		ret = max96851_write_byte(client, cmd_data->addr[i], cmd_data->data[i]);
		if (ret) {
			pr_info("[MAX96851] Write seres command failed, addr=0x%x data=0x%x ret: %d\n",
					cmd_data->addr[i], cmd_data->data[i], ret);
		}

		if (cmd_data->delay_ms[i])
			msleep(cmd_data->delay_ms[i]);
	}
}


static int parse_data_by_prop_from_dts(struct device_node *np, const char *prop,
										u32 *data)
{
	int ret = 0;

	ret = of_property_read_u32(np, prop, data);
	if (ret)
		return ret;
	return 0;
}

static int parse_init_cmd_by_prop_from_dts(struct max96851_bridge *max_bridge,
							struct device_node *np, const char *init_cmd_prop,
							struct serdes_cmd_info *init_cmd)
{
	u32 *array = NULL;
	int ret = 0, num = 0, i = 0;

	num = of_property_count_u32_elems(np, init_cmd_prop);
	if (num < 0) {
		pr_info("[MAX96851] %s read fail\n", init_cmd_prop);
		return -EINVAL;
	} else if (num % 4 != 0) {
		pr_info("[MAX96851] %s data error\n", init_cmd_prop);
		return -EINVAL;
	}

	array = devm_kzalloc(max_bridge->dev, num * sizeof(u32), GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	init_cmd->length = num / 4;
	init_cmd->obj = devm_kzalloc(max_bridge->dev, num * sizeof(u8) / 4, GFP_KERNEL);
	init_cmd->addr = devm_kzalloc(max_bridge->dev, num * sizeof(u16) / 4, GFP_KERNEL);
	init_cmd->data = devm_kzalloc(max_bridge->dev, num * sizeof(u8) / 4, GFP_KERNEL);
	init_cmd->delay_ms = devm_kzalloc(max_bridge->dev, num * sizeof(u8) / 4, GFP_KERNEL);

	if (!init_cmd->addr || !init_cmd->data || !init_cmd->delay_ms)
		goto err;

	ret = of_property_read_u32_array(np, init_cmd_prop, array, num);
	if (ret)
		goto err;

	for (i = 0; i < init_cmd->length; i++) {
		for (ret = 0; ret < 4; ret++) {
			if (ret == 0)
				init_cmd->obj[i] = (u8)array[i*4 + ret];
			else if (ret == 1)
				init_cmd->addr[i] = (u16)(array[i*4 + ret]);
			else if (ret == 2)
				init_cmd->data[i] = (u8)array[i*4 + ret];
			else
				init_cmd->delay_ms[i] = (u8)array[i*4 + ret];
		}
	}

	devm_kfree(max_bridge->dev, array);
	return 0;
err:
	devm_kfree(max_bridge->dev, array);
	return ret;
}

static int parse_feature_cmd_by_prop_from_dts(struct max96851_bridge *max_bridge,
					struct feature_info *feature_info, const char *feature_cmd_prop,
					struct device_node *feature_handle_np)
{
	u32 *array = NULL;
	int ret = 0, num = 0, i = 0, j = 0, k = 0;

	num = of_property_count_u32_elems(feature_handle_np, feature_cmd_prop);
	if (num < 0) {
		pr_info("[MAX96851] %s read fail\n", feature_cmd_prop);
		return -EINVAL;
	}

	array = devm_kzalloc(max_bridge->dev, num * sizeof(u32), GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	ret = of_property_read_u32_array(feature_handle_np, feature_cmd_prop, array, num);
	if (ret)
		goto err;

	/* cmd group*/
	feature_info->group_num = (u8)array[0];

	feature_info->feature_cmd = devm_kzalloc(max_bridge->dev,
						feature_info->group_num * sizeof(struct feature_info), GFP_KERNEL);

	/* parse cmd */
	for (i = 0; i < feature_info->group_num; i++) {
		for ( j = 1; j < num; j++) {
			feature_info->feature_cmd[i].cmd_type = (u8)array[j++];
			feature_info->feature_cmd[i].i2c_addr = (u16)array[j++];
			feature_info->feature_cmd[i].cmd_length = (u8)array[j++];
			feature_info->feature_cmd[i].data = devm_kzalloc(max_bridge->dev,
				feature_info->feature_cmd[i].cmd_length * sizeof(u8), GFP_KERNEL);
			for (k = 0; k < feature_info->feature_cmd[i].cmd_length; k++)
				feature_info->feature_cmd[i].data[k] = (u8)array[j++];
		}
	}

	devm_kfree(max_bridge->dev, array);
	return 0;
err:
	devm_kfree(max_bridge->dev, array);
	return ret;

}

static int excute_feature_check_cmd(struct max96851_bridge *max_bridge, struct feature_cmd feature_cmd)
{
	struct i2c_client *client = NULL;
	int ret = 0;

	client = i2c_new_dummy_device(max_bridge->max96851_i2c->adapter, feature_cmd.i2c_addr);
	if (IS_ERR(client)) {
		pr_info("[MAX96851] Failed to create dummy I2C device 0x%x\n", feature_cmd.i2c_addr);
		devm_kfree(max_bridge->dev, feature_cmd.data);
		return PTR_ERR(client);
	}

	if (feature_cmd.cmd_type) {
		ret = i2c_master_send(client, feature_cmd.data, feature_cmd.cmd_length);
		if (ret < 0) {
			pr_info("[MAX96851] Failed to send data to device 0x%x %d\n", feature_cmd.i2c_addr, ret);
			goto err;
		}
	}

	devm_kfree(max_bridge->dev, feature_cmd.data);
	i2c_unregister_device(client);
	return 0;
err:
	devm_kfree(max_bridge->dev, feature_cmd.data);
	i2c_unregister_device(client);
	return ret;

}

static int excute_feature_verify_cmd(struct max96851_bridge *max_bridge, struct feature_cmd feature_cmd)
{
	struct i2c_client *client = NULL;
	int ret = 0, i = 0, j = 0;
	u8 value[32] = {0};

	client = i2c_new_dummy_device(max_bridge->max96851_i2c->adapter, feature_cmd.i2c_addr);
	if (IS_ERR(client)) {
		pr_info("[MAX96851] Failed to create dummy I2C device 0x%x\n", feature_cmd.i2c_addr);
		devm_kfree(max_bridge->dev, feature_cmd.data);
		return PTR_ERR(client);
	}

	if (!feature_cmd.cmd_type) {
		ret = i2c_master_recv(client, value, feature_cmd.cmd_length / 2);
		if (ret < 0) {
			pr_info("[MAX96851] Failed to receive data from device 0x%x %d\n", feature_cmd.i2c_addr, ret);
			goto err;
		}
	}

	/* verify the received data */
	for (i = 0; i < feature_cmd.cmd_length; i += 2) {
		pr_info("[MAX96851] value:%x mask:%x verify:%x\n",value[j],feature_cmd.data[i],feature_cmd.data[i+1]);
		if ((value[j++] & feature_cmd.data[i]) != feature_cmd.data[i+1]) {
			pr_info("[MAX96851] Received data does not match with expected value, doesn't apply this setting\n");
			ret = -EINVAL;
			goto err;
		}
	}

	devm_kfree(max_bridge->dev, feature_cmd.data);
	i2c_unregister_device(client);
	return 0;
err:
	devm_kfree(max_bridge->dev, feature_cmd.data);
	i2c_unregister_device(client);
	return ret;

}

static int feature_handle(struct max96851_bridge *max_bridge, struct device_node *feature_handle_np)
{
	struct feature_info check_cmd;
	struct feature_info verify_cmd;
	char *feature_setting = NULL;
	int ret = 0, i = 0;

	ret = parse_feature_cmd_by_prop_from_dts(max_bridge, &check_cmd, FEATURE_CHECK_CMD, feature_handle_np);
	if (ret)
		return ret;

	ret = parse_feature_cmd_by_prop_from_dts(max_bridge, &verify_cmd, FEATURE_VERIFY_CMD, feature_handle_np);
	if (ret) {
		devm_kfree(max_bridge->dev, check_cmd.feature_cmd);
		return ret;
	}

	if (check_cmd.group_num != verify_cmd.group_num)
		goto err;

	for (i = 0; i < check_cmd.group_num; i++) {
		ret = excute_feature_check_cmd(max_bridge, check_cmd.feature_cmd[i]);
		if (ret)
			goto err;
		ret = excute_feature_verify_cmd(max_bridge, verify_cmd.feature_cmd[i]);
		if (!ret)
			break;
	}

	/* apply feature setting */
	if (!ret) {
		feature_setting = (char *)of_get_property(feature_handle_np, FEATURE_SETTING, NULL);
		if (feature_setting) {
			memset(max_bridge->panel_name, 0, PANEL_NAME_SIZE);
			strscpy(max_bridge->panel_name, feature_setting, PANEL_NAME_SIZE);
		}
	}

	devm_kfree(max_bridge->dev, check_cmd.feature_cmd);
	devm_kfree(max_bridge->dev, verify_cmd.feature_cmd);
	return 0;
err:
	devm_kfree(max_bridge->dev, check_cmd.feature_cmd);
	devm_kfree(max_bridge->dev, verify_cmd.feature_cmd);
	return ret;
}

static int parse_feature_info_from_dts(struct max96851_bridge *max_bridge, struct device_node *feature_np,
									u32 feature_handle_num)
{
	int ret = 0, i = 0;
	char feature_handle_name[64] = {0};
	struct device_node *feature_handle_np = NULL;

	reset_ser(max_bridge);

	/* feature-handle number process */
	for (i = 0; i < feature_handle_num; i++) {
		memset(feature_handle_name, 0, 64);
		sprintf(feature_handle_name, "%s%d", PANEL_FEATURE_HANDLE, i);
		feature_handle_np = of_get_child_by_name(feature_np, feature_handle_name);
		if (!feature_handle_np) {
			pr_info("%s: failed to find feature handle node %s\n", __func__, feature_handle_name);
			return -EINVAL;
		}

		/* parse feature handle info from dts */
		ret = feature_handle(max_bridge, feature_handle_np);
		if (ret) {
			pr_info("%s: failed to handle feature\n", __func__);
			goto err;
		}
	}

	of_node_put(feature_handle_np);
	return 0;
err:
	of_node_put(feature_handle_np);
	return -EINVAL;
}

static int parse_and_process_bl_cmd_from_dts(struct max96851_bridge *max_bridge,
				struct device_node *np, const char *prop_name, struct bl_cmd_info *bl_cmd)
{
	u32 *array;
	int ret = 0, num = 0, i = 0;

	num = of_property_count_u32_elems(np, prop_name);
	if (num < 0) {
		pr_info("[MAX96851] %s read fail\n", prop_name);
		return -EINVAL;
	}

	bl_cmd->length = num;
	array = devm_kzalloc(max_bridge->dev, num * sizeof(u32), GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	bl_cmd->data = devm_kzalloc(max_bridge->dev, num * sizeof(u8), GFP_KERNEL);
	if (!bl_cmd->data)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, prop_name, array, num);
	if (ret)
		return ret;

	for (i = 0; i < num; i++)
		bl_cmd->data[i] = (u8)array[i];

	devm_kfree(max_bridge->dev, array);

	return 0;
}

static int get_bl_on_off_cmd_info_from_dts(struct max96851_bridge *max_bridge, struct device_node *np)
{
	int ret;

	ret = parse_and_process_bl_cmd_from_dts(max_bridge, np, BL_ON_CMD, &max_bridge->bl_on_cmd);
	if (ret)
		return ret;

	ret = parse_and_process_bl_cmd_from_dts(max_bridge, np, BL_OFF_CMD, &max_bridge->bl_off_cmd);
	if (ret)
		return ret;

	return 0;
}

static int parse_multi_view_cmd_from_dts(struct max96851_bridge *max_bridge, struct device_node *multi_view_setting_np)
{
	int ret = 0;

	for (int i = 0; i < ARRAY_SIZE(props); i++) {
		ret = parse_data_by_prop_from_dts(multi_view_setting_np, props[i], &max_bridge->des_i2c_addr + i);
		if (ret) {
			pr_info("[MAX96851] failed to parse %s property\n", props[i]);
			return ret;
		}
	}

	pr_info("[MAX96851] des-i2c-addr:0x%x bl-i2c-addr:0x%x\n", max_bridge->des_i2c_addr, max_bridge->bl_i2c_addr);
	pr_info("[MAX96851] des-linka-i2c-addr:0x%x des-linka-bl-i2c-addr:0x%x\n", max_bridge->linka_i2c_addr,
			max_bridge->linka_bl_addr);
	pr_info("[MAX96851] des-linkb-i2c-addr:0x%x des-linkb-bl-i2c-addr:0x%x\n", max_bridge->linkb_i2c_addr,
			max_bridge->linkb_bl_addr);

	/* Parse init commands */

	for (int i = 0; i < ARRAY_SIZE(init_cmds); i++) {
		ret = parse_init_cmd_by_prop_from_dts(max_bridge, multi_view_setting_np, init_cmds[i],
					&max_bridge->serdes_init_linka_cmd + i);
		if (ret) {
			pr_info("[MAX96851] failed to parse %s property\n", init_cmds[i]);
			return ret;
		}
	}

	if (max_bridge->is_support_touch) {
		ret = parse_init_cmd_by_prop_from_dts(max_bridge, multi_view_setting_np,
										SERDES_SUPERFRAME_TOUCH_INIT_CMD,
								&max_bridge->serdes_superframe_touch_init_cmd);
		if (ret) {
			pr_info("[MAX96851] failed to parse %s property\n", SERDES_SUPERFRAME_TOUCH_INIT_CMD);
			return ret;
		}
	}

	/* Parse backlight settings */
	ret = get_bl_on_off_cmd_info_from_dts(max_bridge, multi_view_setting_np);
	if (ret) {
		pr_info("[MAX96851] Failed to parse superframe backlight setting\n");
		return ret;
	}

	return 0;
}


static int parse_multi_view_setting(struct max96851_bridge *max_bridge, struct device_node *superframe_setting_np)
{
	struct device_node *multi_view_np;
	u32 read_value = 0;
	int ret;

	/* asymmetric-multi-view check */
	ret = of_property_read_u32(superframe_setting_np, ASYMMETRIC_MULTI_VIEW, &read_value);
	if (!ret)
		max_bridge->asymmetric_multi_view = read_value ? true : false;
	else
		max_bridge->asymmetric_multi_view = false;

	pr_info("[MAX96851] Asymmetric multi-view: %d\n", max_bridge->asymmetric_multi_view);

	if (max_bridge->asymmetric_multi_view)
		multi_view_np = of_get_child_by_name(superframe_setting_np, ASYSMMETRIC_MULTI_VIEW_SETTING);
	else
		multi_view_np = of_get_child_by_name(superframe_setting_np, SYSMMETRIC_MULTI_VIEW_SETTING);

	if (!multi_view_np) {
		pr_info("[MAX96851] No multi-view setting node found!\n");
		return -EINVAL;
	}

	ret = parse_multi_view_cmd_from_dts(max_bridge, multi_view_np);
	of_node_put(multi_view_np);

	return ret;
}

static void get_general_info_from_dts(struct max96851_bridge *max_bridge)
{
	struct device_node *np = max_bridge->dev->of_node;
	u32 read_value = 0;
	int ret;

	ret = of_property_read_u32(np, SERDES_SUPER_FRAME, &read_value);
	max_bridge->superframe_support = (!ret) ? !!read_value : false;

	ret = of_property_read_u32(np, SERDES_DUAL_LINK, &read_value);
	max_bridge->dual_link_support = (!ret) ? !!read_value : false;

	ret = of_property_read_u32(np, SUPPORT_MST, &read_value);
	max_bridge->is_support_mst = (!ret) ? !!read_value : false;

	ret = of_property_read_u32(np, SUPPORT_TOUCH, &read_value);
	max_bridge->is_support_touch = (!ret) ? !!read_value : false;

	pr_info("[MAX96851] ser-super-frame: %d ser-dual-link: %d ser-dp-mst: %d support-touch: %d\n",
			max_bridge->superframe_support, max_bridge->dual_link_support,
			max_bridge->is_support_mst, max_bridge->is_support_touch);
}

static int get_panel_feature_info_from_dts(struct max96851_bridge *max_bridge)
{
	struct device_node *np = max_bridge->dev->of_node;
	struct device_node *feature_np = NULL;
	const char *panel_name;
	u32 feature_handle_num = 0;
	int ret = 0;

	panel_name = of_get_property(np, PANEL_NAME, NULL);
	if (!panel_name) {
		pr_info("[MAX96851] Panel name is not provided\n");
		return -EINVAL;
	}

	max_bridge->panel_name = devm_kzalloc(max_bridge->dev, PANEL_NAME_SIZE, GFP_KERNEL);
	if (!max_bridge->panel_name)
		return -ENOMEM;

	strscpy(max_bridge->panel_name, panel_name, PANEL_NAME_SIZE);

	/* Get child node feature */
	feature_np = of_get_child_by_name(np, PANEL_FEATURE);
	if (!feature_np) {
		pr_info("[MAX96851] Failed to find panel feature use default setting\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(feature_np, PANEL_FEATURE_HANDLE_NUM, &feature_handle_num);
	if (ret || !feature_handle_num) {
		pr_info("[MAX96851] feature-handle-num not exist or value is zero, use  default setting\n");
		goto err;
	}

	ret = parse_feature_info_from_dts(max_bridge, feature_np, feature_handle_num);
	if (ret) {
		pr_info("[MAX96851] failed to parse panel feature info from dts, use default setting\n");
		goto err;
	}

	of_node_put(feature_np);
	pr_info("[MAX96851] Supported panel: %s\n", max_bridge->panel_name);
	return 0;
err:
	of_node_put(feature_np);
	pr_info("[MAX96851] Supported panel: %s\n", max_bridge->panel_name);
	return ret;
}

static int get_signal_setting_from_dts(struct max96851_bridge *max_bridge,
									  struct device_node *panel_setting_np)
{
	struct device_node *signal_setting_np;
	int ret;

	/* Get child node max96851_signal_setting */
	signal_setting_np = of_get_child_by_name(panel_setting_np, MAX96851_SIGNAL_SETTING);
	if (!signal_setting_np) {
		pr_info("%s: No signal setting node!\n", __func__);
		return -EINVAL;
	}

	/* Parse DES and BL I2C addresses */
	ret = parse_data_by_prop_from_dts(signal_setting_np, DES_I2C_ADDR, &max_bridge->des_i2c_addr);
	if (ret) {
		pr_info("[MAX96851] Failed to parse %s property\n", DES_I2C_ADDR);
		goto err;
	}

	ret = parse_data_by_prop_from_dts(signal_setting_np, BL_I2C_ADDR, &max_bridge->bl_i2c_addr);
	if (ret) {
		pr_info("[MAX96851] Failed to parse %s property\n", BL_I2C_ADDR);
		goto err;
	}

	pr_info("[MAX96851] DES I2C Addr: 0x%x, BL I2C Addr: 0x%x\n",
			max_bridge->des_i2c_addr, max_bridge->bl_i2c_addr);

	/* Parse signal init command info from dts */
	ret = parse_init_cmd_by_prop_from_dts(max_bridge, signal_setting_np,
				SER_INIT_CMD_NAME, &max_bridge->signal_ser_init_cmd);
	if (ret) {
		pr_info("[MAX96851] Failed to parse %s property\n", SER_INIT_CMD_NAME);
		goto err;
	}

	ret = parse_init_cmd_by_prop_from_dts(max_bridge, signal_setting_np,
					DES_INIT_CMD_NAME, &max_bridge->signal_des_init_cmd);
	if (ret) {
		pr_info("[MAX96851] Failed to parse %s property\n", DES_INIT_CMD_NAME);
		goto err;
	}

	if (max_bridge->is_support_touch) {
		ret = parse_init_cmd_by_prop_from_dts(max_bridge, signal_setting_np, TOUCH_INIT_CMD_ADDR,
							&max_bridge->signal_touch_init_cmd);
		if (ret) {
			pr_info("[MAX96851] Failed to parse %s property\n", TOUCH_INIT_CMD_ADDR);
			goto err;
		}
	}

	/* Parse signal backlight settings from dts */
	ret = get_bl_on_off_cmd_info_from_dts(max_bridge, signal_setting_np);
	if (ret) {
		pr_info("[MAX96851] Failed to parse backlight setting\n");
		goto err;
	}

	of_node_put(signal_setting_np);
	return 0;

err:
	of_node_put(signal_setting_np);
	return ret;
}

static int get_superframe_setting_from_dts(struct max96851_bridge *max_bridge, struct device_node *panel_setting_np)
{
	struct device_node *superframe_setting_np;
	int ret = 0;

	superframe_setting_np = of_get_child_by_name(panel_setting_np, MAX96851_SUPERFRAME_SETTING);
	if (!superframe_setting_np) {
		pr_info("%s: No superframe setting node found!\n", __func__);
		return -EINVAL;
	}

	ret = parse_multi_view_setting(max_bridge, superframe_setting_np);
	if (ret)
		pr_info("%s: Failed to parse multi view setting from DTS!\n", __func__);

	of_node_put(superframe_setting_np);

	return ret;
}

static int get_dual_link_setting_from_dts(struct max96851_bridge *max_bridge, struct device_node *panel_setting_np)
{
	return 0;
}

static int get_mst_setting_from_dts(struct max96851_bridge *max_bridge,
										struct device_node *panel_setting_np)
{
	return 0;
}

static int get_serdes_setting_from_dts(struct max96851_bridge *max_bridge)
{
	int ret = 0;
	struct device_node *np = max_bridge->dev->of_node;
	struct device_node *panel_setting_np;

	panel_setting_np = of_get_child_by_name(np, max_bridge->panel_name);
	if (!panel_setting_np) {
		pr_info("%s: failed to find %s node\n", __func__, max_bridge->panel_name);
		return -EINVAL;
	}

	if (max_bridge->is_dp) {
		global_panel_dp_name = devm_kzalloc(max_bridge->dev, PANEL_NAME_SIZE, GFP_KERNEL);
		if (!global_panel_dp_name)
			return -ENOMEM;

		global_panel_dp_mode = devm_kzalloc(max_bridge->dev, PANEL_NAME_SIZE, GFP_KERNEL);
		if (!global_panel_dp_mode)
			return -ENOMEM;

		strscpy(global_panel_dp_name, max_bridge->panel_name, PANEL_NAME_SIZE);
	} else {
		global_panel_edp_name = devm_kzalloc(max_bridge->dev, PANEL_NAME_SIZE, GFP_KERNEL);
		if (!global_panel_edp_name)
			return -ENOMEM;

		global_panel_edp_mode = devm_kzalloc(max_bridge->dev, PANEL_NAME_SIZE, GFP_KERNEL);
		if (!global_panel_edp_mode)
			return -ENOMEM;

		strscpy(global_panel_edp_name, max_bridge->panel_name, PANEL_NAME_SIZE);
	}

	if (max_bridge->superframe_support) {
		/* parse superframe setting */
		if (max_bridge->is_dp)
			strscpy(global_panel_dp_mode, SUPERFRAME_SETTING, PANEL_NAME_SIZE);
		else
			strscpy(global_panel_edp_mode, SUPERFRAME_SETTING, PANEL_NAME_SIZE);
		ret = get_superframe_setting_from_dts(max_bridge, panel_setting_np);
		if (ret)
			return ret;
		else
			return 0;
	} else if (max_bridge->dual_link_support) {
		/* parse dual link setting */
		ret = get_dual_link_setting_from_dts(max_bridge, panel_setting_np);
		if (ret)
			return ret;
	} else if (max_bridge->is_support_mst) {
		/* parse mst setting */
		ret = get_mst_setting_from_dts(max_bridge, panel_setting_np);
		if (ret)
			return ret;
	} else {
		/* parse signal setting */
		if (max_bridge->is_dp)
			strscpy(global_panel_dp_mode, SINGAL_SETTING, PANEL_NAME_SIZE);
		else
			strscpy(global_panel_edp_mode, SINGAL_SETTING, PANEL_NAME_SIZE);
		ret = get_signal_setting_from_dts(max_bridge, panel_setting_np);
		if (ret)
			return ret;
	}

	of_node_put(panel_setting_np);
	return 0;
}

static int max96851_dt_parse(struct max96851_bridge *max_bridge, struct device *dev)
{
	int ret = 0;
	struct device_node *np = dev->of_node;

	if (!np)
		return -EINVAL;

	pr_info("[MAX96851] %s+\n", __func__);

	max_bridge->is_dp = of_device_is_compatible(np, "maxiam,max96851-dp") ? true : false;
	pr_info("[MAX96851] Serdes for: %d [0: eDP, 1: DP]\n", max_bridge->is_dp);

	get_general_info_from_dts(max_bridge);

	ret = get_panel_feature_info_from_dts(max_bridge);
	if (ret)
		pr_info("[MAX96851] use default setting\n");

	ret = get_serdes_setting_from_dts(max_bridge);
	if (ret)
		return ret;

	pr_info("[MAX96851] %s-\n", __func__);
	return 0;
}

static int turn_on_off_bl(struct max96851_bridge *max_bridge, bool enable, u8 type)
{
	int ret = 0;
	struct i2c_client *client = NULL;

	if (max_bridge->superframe_support) {
		if (type == 0x1)
			client = max_bridge->max96752_linka_bl_i2c;
		else if (type == 0x2)
			client = max_bridge->max96752_linkb_bl_i2c;
	} else if(max_bridge->dual_link_support) {
		client = max_bridge->bl_i2c;
	} else if (max_bridge->is_support_mst)
		client = max_bridge->bl_i2c;
	else
		client = max_bridge->bl_i2c;

	pr_info("[MAX96851] %s enable: %d\n", __func__, enable);
	if (!client) {
		pr_info("[MAX96851] No client found for BL\n");
		return -ENODEV;
	}

	if (enable) {
		ret = i2c_master_send(client, max_bridge->bl_on_cmd.data, max_bridge->bl_on_cmd.length);
		if (ret < 0) {
			pr_info("[MAX96851] Failed to send bl on cmd\n");
			return ret;
		}
	} else {
		ret = i2c_master_send(client, max_bridge->bl_off_cmd.data, max_bridge->bl_off_cmd.length);
		if (ret < 0) {
			pr_info("[MAX96851] Failed to send bl off cmd\n");
			return ret;
		}
	}

	return 0;
}

static void serdes_init_by_signal(struct max96851_bridge *max_bridge)
{
	dev_info(max_bridge->dev, "%s+\n", __func__);
	/* ser init by default */
	ser_init_loop(max_bridge, &max_bridge->signal_ser_init_cmd);

	/* des init by default */
	des_init_loop(max_bridge, &max_bridge->signal_des_init_cmd);

	/* touch init by default */
	serdes_init_loop(max_bridge, &max_bridge->signal_touch_init_cmd);

	dev_info(max_bridge->dev, "%s-\n", __func__);
}

static void serdes_init_by_superframe(struct max96851_bridge *max_bridge)
{
	dev_info(max_bridge->dev, "%s+\n", __func__);
	/* serdes to init linka */
	serdes_init_loop(max_bridge, &max_bridge->serdes_init_linka_cmd);

	/*des to init linka */
	serdes_init_loop(max_bridge, &max_bridge->des_linka_init_cmd);

	/*serdes to init linkb */
	serdes_init_loop(max_bridge, &max_bridge->serdes_init_linkb_cmd);

	/* des to init linkb */
	serdes_init_loop(max_bridge, &max_bridge->des_linkb_init_cmd);

	/* ser to init superframe */
	ser_init_loop(max_bridge, &max_bridge->ser_superframe_init_cmd);

	/* serdes to init superframe */
	serdes_init_loop(max_bridge, &max_bridge->serdes_superframe_touch_init_cmd);

	dev_info(max_bridge->dev, "%s-\n", __func__);
}

static void serdes_init_by_dual_link(struct max96851_bridge *max_bridge)
{
	dev_info(max_bridge->dev, "%s+\n", __func__);

	dev_info(max_bridge->dev, "%s-\n", __func__);
}

static void serdes_init_by_mst(struct max96851_bridge *max_bridge)
{
	dev_info(max_bridge->dev, "%s+\n", __func__);

	dev_info(max_bridge->dev, "%s-\n", __func__);
}

static void max96851_pre_enable(struct drm_bridge *bridge)
{
	struct max96851_bridge *max_bridge = bridge_to_max96851(bridge);
	u8 dev_id = 0x0;

	pr_info("[MAX96851] Serdes DP: %d %s+\n", max_bridge->is_dp, __func__);

	if (max_bridge->prepared)
		return;
	/* reset serdes */
	reset_ser(max_bridge);

	/* device identifier  0xC4: Without HDCP 0xC5: With HDCP */
	dev_id = max96851_read_byte(max_bridge->max96851_i2c, DEVICE_IDENTIFIER_ADDR);
	pr_notice("[MAX96851] Device Identifier = 0x02%x\n", dev_id);

	if (max_bridge->superframe_support)
		serdes_init_by_superframe(max_bridge);
	else if(max_bridge->dual_link_support)
		serdes_init_by_dual_link(max_bridge);
	else if (max_bridge->is_support_mst)
		serdes_init_by_mst(max_bridge);
	else
		serdes_init_by_signal(max_bridge);

	pr_info("[MAX96851] Serdes DP: %d %s-\n", max_bridge->is_dp, __func__);
}

static void max96851_enable(struct drm_bridge *bridge)
{
	struct max96851_bridge *max_bridge = bridge_to_max96851(bridge);

	pr_info("[MAX96851] Serdes DP: %d %s+\n", max_bridge->is_dp, __func__);

	if (max_bridge->enabled)
		return ;

	/* turn on backlight */
	if (max_bridge->superframe_support) {
		turn_on_off_bl(max_bridge, true, 0x01);
		turn_on_off_bl(max_bridge, true, 0x02);
	} else if(max_bridge->dual_link_support) {
		turn_on_off_bl(max_bridge, true, 0x0);
	} else if (max_bridge->is_support_mst)
		turn_on_off_bl(max_bridge, true, 0x0);
	else
		turn_on_off_bl(max_bridge, true, 0x0);

	max_bridge->enabled = true;

	pr_info("[MAX96851] Serdes DP: %d %s-\n", max_bridge->is_dp, __func__);
}

static void max96851_disable(struct drm_bridge *bridge)
{
	struct max96851_bridge *max_bridge = bridge_to_max96851(bridge);

	pr_info("[MAX96851] Serdes DP: %d %s+\n", max_bridge->is_dp, __func__);

	if (!max_bridge->enabled && !max_bridge->prepared)
		return ;

	/* turn off backlight */
	if (max_bridge->superframe_support) {
		turn_on_off_bl(max_bridge, false, 0x01);
		turn_on_off_bl(max_bridge, false, 0x02);
	} else if(max_bridge->dual_link_support) {
		turn_on_off_bl(max_bridge, false, 0x0);
	} else if (max_bridge->is_support_mst)
		turn_on_off_bl(max_bridge, false, 0x0);
	else
		turn_on_off_bl(max_bridge, false, 0x0);

	gpiod_set_value(max_bridge->gpio_rst_n, 0);
	msleep(20);

	max_bridge->prepared = false;
	max_bridge->enabled = false;

	pr_info("[MAX96851] Serdes DP: %d %s-\n", max_bridge->is_dp, __func__);
}

static int max96851_bridge_attach(struct drm_bridge *bridge,
				enum drm_bridge_attach_flags flags)
{
	struct max96851_bridge *max_bridge = bridge_to_max96851(bridge);
	struct device *dev = NULL;
	int ret;

	pr_info("[MAX96851] Serdes DP: %d %s+\n", max_bridge->is_dp, __func__);

	dev = &max_bridge->max96851_i2c->dev;

	ret = drm_bridge_attach(bridge->encoder, max_bridge->panel_bridge,
					bridge, flags | DRM_BRIDGE_ATTACH_NO_CONNECTOR);
	if (ret < 0) {
		pr_info("[MAX96851] Failed to attach panel_bridge: %d\n", ret);
		return ret;
	}

	if (!(flags & DRM_BRIDGE_ATTACH_NO_CONNECTOR)) {
		pr_info("[MAX96851] Driver does not provide a connector\n");
		return -EINVAL;
	}

	if (!bridge->encoder) {
		pr_info("[MAX96851] Parent encoder object not found\n");
		return -ENODEV;
	}

	pr_info("[MAX96851] Serdes DP: %d %s-\n", max_bridge->is_dp, __func__);

	return 0;
}

static int max96851_bridge_get_modes(struct drm_bridge *bridge,
			 struct drm_connector *connector)
{
	pr_info("[MAX96851] %s+\n", __func__);
	pr_info("[MAX96851] %s-\n", __func__);
	return 0;
}

static const struct drm_bridge_funcs max96851_bridge_funcs = {
	.pre_enable = max96851_pre_enable,
	.enable = max96851_enable,
	.disable = max96851_disable,
	.attach = max96851_bridge_attach,
	.get_modes = max96851_bridge_get_modes,
};

static int max96851_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct drm_bridge *panel_bridge = NULL;
	struct drm_panel *panel = NULL;
	struct max96851_bridge *max_bridge = NULL;
	int ret;

	dev_info(dev, "[MAX96851] %s+\n", __func__);

	max_bridge = devm_kzalloc(dev, sizeof(*max_bridge), GFP_KERNEL);
	if (!max_bridge)
		return -ENOMEM;

	ret = drm_of_find_panel_or_bridge(dev->of_node, 1, 0, &panel, NULL);
	if (!panel) {
		pr_info("[MAX96851] Failed to find panel %d\n", ret);
		return -EPROBE_DEFER;
	}

	dev_info(dev, "[MAX96851] Found panel node: %pOF\n", panel->dev->of_node);

	panel_bridge = devm_drm_panel_bridge_add(dev, panel);
	if (IS_ERR(panel_bridge)) {
		dev_info(dev, "[MAX96851] Failed to create panel bridge\n");
		return PTR_ERR(panel_bridge);
	}
	max_bridge->panel_bridge = panel_bridge;

	max_bridge->gpio_rst_n = devm_gpiod_get(dev, "reset",
							GPIOD_OUT_HIGH);
	if (IS_ERR(max_bridge->gpio_rst_n)) {
		ret = PTR_ERR(max_bridge->gpio_rst_n);
		dev_info(dev, "[MAX96851] Cannot get reset gpio %d\n", ret);
		return ret;
	}

	i2c_set_clientdata(client, max_bridge);
	max_bridge->client = client;
	max_bridge->dev = dev;
	max_bridge->max96851_i2c = client;

	ret = max96851_dt_parse(max_bridge, dev);
	if (ret) {
		dev_info(dev, "[MAX96851] DT parse failed\n");
		return ret;
	}

	ret = max96851_create_i2c_client(max_bridge);
	if (ret) {
		dev_info(dev, "[MAX96851] create i2c client failed\n");
		return ret;
	}

	if (max_bridge->is_dp)
		max_bridge->bridge.type = DRM_MODE_CONNECTOR_DisplayPort;
	else
		max_bridge->bridge.type = DRM_MODE_CONNECTOR_eDP;

	max_bridge->bridge.funcs = &max96851_bridge_funcs;
	max_bridge->bridge.of_node = dev->of_node;
	drm_bridge_add(&max_bridge->bridge);

	if (max_bridge->is_dp) {
		max96851_pre_enable(&max_bridge->bridge);
		max96851_enable(&max_bridge->bridge);
	}

	dev_info(dev, "[MAX96851] %s-\n", __func__);

	return 0;
}

static void max96851_remove(struct i2c_client *client)
{
	struct max96851_bridge *max_bridge = i2c_get_clientdata(client);

	i2c_unregister_device(max_bridge->max96752_i2c);
	i2c_unregister_device(max_bridge->bl_i2c);
	if (max_bridge->dual_link_support) {
		i2c_unregister_device(max_bridge->max96752_linka_i2c);
		i2c_unregister_device(max_bridge->max96752_linkb_i2c);
	}
	i2c_unregister_device(max_bridge->max96851_i2c);

	drm_bridge_remove(&max_bridge->bridge);
}

static const struct i2c_device_id max96851_edp_i2c_table[] = {
	{"max96851-dp", 0},
	{"max96851-edp", 0},
	{ },
};

MODULE_DEVICE_TABLE(i2c, max96851_edp_i2c_table);

static const struct of_device_id max96851_serdes_match[] = {
	{.compatible = "maxiam,max96851-dp"},
	{.compatible = "maxiam,max96851-edp"},
	{ },
};

MODULE_DEVICE_TABLE(of, max96851_serdes_match);

static struct i2c_driver max96851_edp_driver = {
	.id_table = max96851_edp_i2c_table,
	.probe = max96851_probe,
	.remove = max96851_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "max96851-serdes",
		.of_match_table = of_match_ptr(max96851_serdes_match),
	},

};

module_i2c_driver(max96851_edp_driver);

MODULE_AUTHOR("Jacky Hu <jie-h.hu@mediatek.com>");
MODULE_DESCRIPTION("max96851 SerDes driver");
MODULE_LICENSE("GPL");
