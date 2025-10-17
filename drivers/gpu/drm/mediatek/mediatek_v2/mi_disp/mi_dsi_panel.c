// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#define pr_fmt(fmt)	"mi-dsi-panel:[%s:%d] " fmt, __func__, __LINE__
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/rtc.h>
#include <video/mipi_display.h>
#include <linux/fs.h>
#include "../../../../kernel/irq/internals.h"
#include <uapi/drm/mi_disp.h>

#include "../mtk_drm_crtc.h"
#include "../mtk_drm_drv.h"
#include "../mtk_dsi.h"

#include "mi_dsi_display.h"
#include "mi_dsi_panel.h"
#include "mi_disp_feature.h"
#include "mtk_panel_ext.h"
#include "mi_disp_print.h"
#include "mi_panel_ext.h"
#include "mtk_drm_ddp_comp.h"
#include "mi_disp_lhbm.h"
#ifdef CONFIG_MI_DISP_DFS_EVENT
#include "mi_disp_event.h"
#endif

#ifdef CONFIG_VIS_DISPLAY_DALI
#include "vis_display.h"
#endif

#if defined(CONFIG_VIS_DISPLAY)
#include "vis_display.h"
int mi_dsi_panel_chk_tricking_status(void)
{
	int ret = -1;
	unsigned int j = 0;
	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	u8 tx[10] = {0};
	char *cmd = "00 00 29 01 00 00 00 00 06 F0 55 AA 52 08 01";

	if (!cmd_msg) {
		DISP_ERROR("cmd msg is NULL\n");
		return ret;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->tx_cmd_num = 1;
	cmd_msg->type[0] = 0x06;
	tx[0] = 0xC3;
	cmd_msg->tx_buf[0] = tx;
	cmd_msg->tx_len[0] = 1;

	cmd_msg->rx_cmd_num = 1;
	cmd_msg->rx_buf[0] = vmalloc(9 * sizeof(unsigned char));
	memset(cmd_msg->rx_buf[0], 0, 9);
	cmd_msg->rx_len[0] = 9;

	ret = mi_dsi_panel_write_mipi_reg(cmd);
	if (ret != 0) {
		DISP_ERROR("%s error\n", __func__);
		goto  done;
	}

	ret = mtk_ddic_dsi_read_cmd(cmd_msg);
	if (ret != 0) {
		DISP_ERROR("%s error\n", __func__);
		goto  done;
	}

	pr_info("read lcm addr:0x%02x--dlen:%d\n",
		*(char *)(cmd_msg->tx_buf[0]), (int)cmd_msg->rx_len[0]);
	for (j = 0; j < cmd_msg->rx_len[0]; j++) {
		pr_info("read lcm addr:0x%02x--byte:%d,val:0x%02x\n",
			(*(char *)(cmd_msg->tx_buf[0])), j,
			(*(char *)(cmd_msg->rx_buf[0] + j)));
	}

	if(*((char *)(cmd_msg->rx_buf[0]) + 4) == 0x00)
	{
		ret = 0;	//tricking off
	}
	else if(*((char *)(cmd_msg->rx_buf[0]) + 4) == 0xFF)
	{
		ret = 1;	//tricking on
	}
	else
	{
		ret = 2;	//invalid
	}

done:
	vfree(cmd_msg->rx_buf[0]);
	vfree(cmd_msg);
	return ret;
}
EXPORT_SYMBOL(mi_dsi_panel_chk_tricking_status);
#endif

//static struct LCM_param_read_write lcm_param_read_write = {0};
struct LCM_mipi_read_write lcm_mipi_read_write = {0};
EXPORT_SYMBOL(lcm_mipi_read_write);
struct LCM_led_i2c_read_write lcm_led_i2c_read_write = {0};
//extern struct frame_stat fm_stat;
struct mi_disp_notifier g_notify_data;

#define DEFAULT_MAX_BRIGHTNESS_CLONE 8191
#define DEFAULT_MAX_BRIGHTNESS  2047
#define MAX_CMDLINE_PARAM_LEN 64
#define PANEL_GIR_OFF_DELAY 120

static char lockdown_info[64] = {0};
extern void mipi_dsi_dcs_write_gce2(struct mtk_dsi *dsi, struct cmdq_pkt *dummy,
					  const void *data, size_t len);

unsigned char temp[MAX_TX_CMD_NUM][255] = {0};
bool is_backlight_set_skip(struct mtk_dsi *dsi, u32 bl_lvl)
{
	if (dsi->mi_cfg.in_fod_calibration ||
		dsi->mi_cfg.feature_val[DISP_FEATURE_HBM_FOD] == FEATURE_ON) {
		DISP_INFO("panel skip set backlight %d due to fod hbm "
				"or fod calibration\n", bl_lvl);
		return true;
	}
	return false;
}

void dsi_panel_gesture_enable(bool enable)
{

	struct mtk_dsi *dsi = mi_get_primary_dsi_display();
	if (!dsi) {
		pr_err("[%s] invalid params\n", __func__);
		return;
	}
	dsi->mi_cfg.tddi_gesture_flag = enable;
	pr_info("[%s]tddi_gesture_flag = %d\n", __func__, dsi->mi_cfg.tddi_gesture_flag);
}
EXPORT_SYMBOL(dsi_panel_gesture_enable);

void mi_display_gesture_callback_register(void (*cb)(void))
{
	struct mtk_dsi *dsi = mi_get_primary_dsi_display();
	if (!dsi) {
		pr_err("%s invalid params\n", __func__);
		return;
	}
	dsi->mi_cfg.mi_display_gesture_cb = cb;
	pr_info("%s func %pF is set.\n", __func__, cb);
	return;
}
EXPORT_SYMBOL(mi_display_gesture_callback_register);

bool dsi_panel_initialized(struct mtk_dsi *dsi)
{
	int ret = 0;
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	if (!dsi) {
		pr_err("NULL dsi\n");
		return false;
	}

	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	if (!(panel_ext && panel_ext->funcs)) {
		pr_info("panel_ext func not defined\n");
		return false;
	}

	if (panel_ext->funcs->get_panel_initialized)
		ret = panel_ext->funcs->get_panel_initialized(dsi->panel);
	return ret;
}

static char string_to_hex(const char *str)
{
	char val_l = 0;
	char val_h = 0;

	if (str[0] >= '0' && str[0] <= '9')
		val_h = str[0] - '0';
	else if (str[0] <= 'f' && str[0] >= 'a')
		val_h = 10 + str[0] - 'a';
	else if (str[0] <= 'F' && str[0] >= 'A')
		val_h = 10 + str[0] - 'A';

	if (str[1] >= '0' && str[1] <= '9')
		val_l = str[1]-'0';
	else if (str[1] <= 'f' && str[1] >= 'a')
		val_l = 10 + str[1] - 'a';
	else if (str[1] <= 'F' && str[1] >= 'A')
		val_l = 10 + str[1] - 'A';

	return (val_h << 4) | val_l;
}

static int string_merge_into_buf(const char *str, int len, char *buf)
{
	int buf_size = 0;
	int i = 0;
	const char *p = str;

	while (i < len) {
		if (((p[0] >= '0' && p[0] <= '9') ||
			(p[0] <= 'f' && p[0] >= 'a') ||
			(p[0] <= 'F' && p[0] >= 'A'))
			&& ((i + 1) < len)) {
			buf[buf_size] = string_to_hex(p);
			pr_debug("0x%02x ", buf[buf_size]);
			buf_size++;
			i += 2;
			p += 2;
		} else {
			i++;
			p++;
		}
	}
	return buf_size;
}

ssize_t dsi_panel_write_mipi_reg(char *buf, size_t count)
{
	int retval = 0;
	int dlen = 0;
	unsigned int read_enable = 0;
	unsigned int packet_count = 0;
	unsigned int register_value = 0;
	char *input = NULL;
	char *data = NULL;
	unsigned char pbuf[3] = {0};
	u8 tx[10] = {0};
	unsigned int  i = 0, j = 0;
	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	pr_info("[%s]: mipi_write_date source: count = %d,buf = %s ", __func__, (int)count, buf);

	input = buf;
	memcpy(pbuf, input, 2);
	pbuf[2] = '\0';
	retval = kstrtou32(pbuf, 10, &read_enable);
	if (retval)
		goto exit;
	lcm_mipi_read_write.read_enable = !!read_enable;
	input = input + 3;
	memcpy(pbuf, input, 2);
	pbuf[2] = '\0';
	packet_count = (unsigned int)string_to_hex(pbuf);
	if (lcm_mipi_read_write.read_enable && !packet_count) {
		retval = -EINVAL;
		goto exit;
	}
	input = input + 3;
	memcpy(pbuf, input, 2);
	pbuf[2] = '\0';
	register_value = (unsigned int)string_to_hex(pbuf);
	lcm_mipi_read_write.lcm_setting_table.cmd = register_value;

	if(lcm_mipi_read_write.read_enable) {
		lcm_mipi_read_write.read_count = packet_count;

		cmd_msg->channel = 0;
		cmd_msg->tx_cmd_num = 1;
		cmd_msg->type[0] = 0x06;
		tx[0] = lcm_mipi_read_write.lcm_setting_table.cmd;
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->tx_len[0] = 1;

		cmd_msg->rx_cmd_num = 1;
		cmd_msg->rx_buf[0] = lcm_mipi_read_write.read_buffer;
		memset(cmd_msg->rx_buf[0], 0, lcm_mipi_read_write.read_count);
		cmd_msg->rx_len[0] = lcm_mipi_read_write.read_count;
		retval = mtk_ddic_dsi_read_cmd(cmd_msg);
		if (retval != 0) {
			pr_err("%s error\n", __func__);
		}

		pr_info("read lcm addr:0x%02x--dlen:%d\n",
			*(char *)(cmd_msg->tx_buf[0]), (int)cmd_msg->rx_len[0]);
		for (j = 0; j < cmd_msg->rx_len[0]; j++) {
			pr_info("read lcm addr:0x%02x--byte:%d,val:0x%02x 0x%02x\n",
				*(char *)(cmd_msg->tx_buf[0]), j,
				*(char *)(cmd_msg->rx_buf[0] + j), *(char *)(cmd_msg->rx_buf[0] + j));
		}
		goto exit;
	} else {
		lcm_mipi_read_write.lcm_setting_table.count = (unsigned char)packet_count;
		memcpy(lcm_mipi_read_write.lcm_setting_table.para_list, "",64);
		if(count > 8)
		{
			data = kzalloc(count - 6, GFP_KERNEL);
			if (!data) {
				retval = -ENOMEM;
				goto exit;
			}
			data[count-6-1] = '\0';
			//input = input + 3;
			dlen = string_merge_into_buf(input,count -6,data);
			memcpy(lcm_mipi_read_write.lcm_setting_table.para_list, data,dlen);

			cmd_msg->channel = packet_count;
			cmd_msg->flags = 2;
			cmd_msg->tx_cmd_num = 1;
			cmd_msg->type[0] = 0x39;

			if (2 == dlen) {
				cmd_msg->type[0] = 0x15;
			} else if (1 == dlen) {
				cmd_msg->type[0] = 0x05;
			}

			cmd_msg->tx_buf[0] = data;
			cmd_msg->tx_len[0] = dlen;
			for (i = 0; i < (int)cmd_msg->tx_cmd_num; i++) {
				pr_debug("send lcm tx_len[%d]=%d\n",
					i, (int)cmd_msg->tx_len[i]);
				for (j = 0; j < (int)cmd_msg->tx_len[i]; j++) {
					pr_debug(
						"send lcm type[%d]=0x%x, tx_buf[%d]--byte:%d,val:%pad\n",
						i, cmd_msg->type[i], i, j,
						&(*(char *)(cmd_msg->tx_buf[i] + j)));
				}
			}

			mtk_ddic_dsi_send_cmd(cmd_msg, true);
		}
	}

	pr_debug("[%s]: mipi_write done!\n", __func__);
	pr_debug("[%s]: write cmd = %d,len = %d\n", __func__,lcm_mipi_read_write.lcm_setting_table.cmd,lcm_mipi_read_write.lcm_setting_table.count);
	pr_debug("[%s]: mipi_write data: ", __func__);
	for(i=0; i<count-3; i++)
	{
		pr_debug("0x%x ", lcm_mipi_read_write.lcm_setting_table.para_list[i]);
	}
	pr_debug("\n ");

	if(count > 8)
	{
		kfree(data);
	}
exit:
	retval = count;
	vfree(cmd_msg);
	return retval;
}

ssize_t  dsi_panel_read_mipi_reg(char *buf)
{
	int i = 0;
	ssize_t count = 0;

	if (lcm_mipi_read_write.read_enable) {
		for (i = 0; i < lcm_mipi_read_write.read_count; i++) {
			if (i ==  lcm_mipi_read_write.read_count - 1) {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02x\n",
				     lcm_mipi_read_write.read_buffer[i]);
			} else {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02x ",
				     lcm_mipi_read_write.read_buffer[i]);
			}
		}
	}
	return count;
}

static int mi_dsi_panel_get_cmd_pkt_count(const char *data, u32 length, u32 *cnt)
{
	const u32 cmd_set_min_size = 7;
	u32 count = 0;
	u32 packet_length;
	u32 tmp;

	while (length >= cmd_set_min_size) {
		packet_length = cmd_set_min_size;
		tmp = ((data[5] << 8) | (data[6]));
		packet_length += tmp;
		if (packet_length > length) {
			DISP_ERROR("format error\n");
			return -EINVAL;
		}
		length -= packet_length;
		data += packet_length;
		count++;
	}

	*cnt = count;
	return 0;
}

static int mi_dsi_panel_create_cmd_packets(const char *data,
					u32 count,
					struct dsi_cmd_desc *cmd)
{
	int rc = 0;
	int i, j;
	u8 *payload;

	for (i = 0; i < count; i++) {
		u32 size;

		cmd[i].msg.type = data[0];
		cmd[i].last_command = (data[1] == 1);
		cmd[i].msg.channel = data[2];
		cmd[i].msg.flags |= data[3];
		cmd[i].post_wait_ms = data[4];
		cmd[i].msg.tx_len = ((data[5] << 8) | (data[6]));

		size = cmd[i].msg.tx_len * sizeof(u8);

		payload = kzalloc(size, GFP_KERNEL);
		if (!payload) {
			rc = -ENOMEM;
			goto error_free_payloads;
		}

		for (j = 0; j < cmd[i].msg.tx_len; j++)
			payload[j] = data[7 + j];

		cmd[i].msg.tx_buf = payload;
		data += (7 + cmd[i].msg.tx_len);
	}

	return rc;
error_free_payloads:
	for (i = i - 1; i >= 0; i--) {
		cmd--;
		kfree(cmd->msg.tx_buf);
	}

	return rc;
}

ssize_t mi_dsi_panel_enable_gir(struct mtk_dsi *dsi, char *buf)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;
	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	dsi->mi_cfg.gir_state = GIR_ON;
	dsi->mi_cfg.feature_val[DISP_FEATURE_GIR] = GIR_ON;
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);
	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->panel_set_gir_on)) {
		pr_info("panel_set_gir_on func not defined");
		return 0;
	} else {
		panel_ext->funcs->panel_set_gir_on(dsi->panel);
	}
	return 0;
}

static void panel_gir_off_control(struct mtk_dsi *dsi)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;
	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return;
	}

	comp = &dsi->ddp_comp;

	if (!comp) {
		pr_info("%s comp nullptr", __func__);
		return;
	}
	panel_ext = mtk_dsi_get_panel_ext(comp);
	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->panel_set_gir_off)) {
		pr_info("%s panel_set_gir_off func not defined", __func__);
		return;
	} else {
		panel_ext->funcs->panel_set_gir_off(dsi->panel);
	}

	return;
}

void panel_gir_off_delayed_work(struct work_struct* work)
{
	struct mtk_dsi* dsi = container_of(work, struct mtk_dsi, gir_off_delayed_work.work);
	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return;
	}
	panel_gir_off_control(dsi);
}

ssize_t mi_dsi_panel_disable_gir(struct mtk_dsi *dsi, char *buf)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;
	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	dsi->mi_cfg.gir_state = GIR_OFF;
	dsi->mi_cfg.feature_val[DISP_FEATURE_GIR] = GIR_OFF;
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);
	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->panel_set_gir_off)) {
		pr_info("panel_set_gir_off func not defined");
		return 0;
	} else {
		panel_ext->funcs->panel_set_gir_off(dsi->panel);
	}
	return 0;
}

int mi_dsi_panel_get_gir_status(struct mtk_dsi *dsi)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;
	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);
	if (panel_ext && panel_ext->funcs && panel_ext->funcs->panel_get_gir_status) {
		return panel_ext->funcs->panel_get_gir_status(dsi->panel);
	} else {
		return dsi->mi_cfg.gir_state;
	}
	return -EINVAL;
}

ssize_t mi_dsi_panel_write_mipi_reg(char *buf)
{
	int rc = 0, read_length = 0, read_buffer_position = 0;
	u32 packet_count = 0;
	char *token, *input_copy, *input_dup = NULL;
	const char *delim = " ";
	char *buffer = NULL;
	u32 buf_size = 0;
	u32 tmp_data = 0;
	int i = 0, j = 0;
	char tx[10] = {0};
	char temp_read_buffer[10] = {0};
	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	struct dsi_cmd_desc *cmds = NULL;
#ifdef CONFIG_MI_DISP_DFS_EVENT
	struct mi_event_info mi_event = {0};
#endif

	DISP_INFO("input buffer:{%s}\n", buf);

	input_copy = kstrdup(buf, GFP_KERNEL);
	if (!input_copy) {
		rc = -ENOMEM;
		goto exit;
	}

	input_dup = input_copy;
	/* removes leading and trailing whitespace from input_copy */
	input_copy = strim(input_copy);

	/* Split a string into token */
	token = strsep(&input_copy, delim);
	if (token) {
		rc = kstrtoint(token, 10, &tmp_data);
		if (rc) {
			DISP_ERROR("input buffer conversion failed\n");
			goto exit_free0;
		}
		lcm_mipi_read_write.read_enable = !!tmp_data;
	}

	/* Removes leading whitespace from input_copy */
	if (input_copy)
		input_copy = skip_spaces(input_copy);
	else
		goto exit_free0;

	token = strsep(&input_copy, delim);
	if (token) {
		rc = kstrtoint(token, 10, &tmp_data);
		if (rc) {
			DISP_ERROR("input buffer conversion failed\n");
			goto exit_free0;
		}
		if (tmp_data > sizeof(lcm_mipi_read_write.read_buffer)) {
			DISP_ERROR("read size exceeding the limit %lu\n",
					sizeof(lcm_mipi_read_write.read_buffer));
			goto exit_free0;
		}
		lcm_mipi_read_write.read_count = tmp_data;
	}

	/* Removes leading whitespace from input_copy */
	if (input_copy)
		input_copy = skip_spaces(input_copy);
	else
		goto exit_free0;

	buffer = kzalloc(strlen(input_copy), GFP_KERNEL);
	if (!buffer) {
		rc = -ENOMEM;
		goto exit_free0;
	}

	token = strsep(&input_copy, delim);
	while (token) {
		rc = kstrtoint(token, 16, &tmp_data);
		if (rc) {
			DISP_ERROR("input buffer conversion failed\n");
			goto exit_free1;
		}
		DISP_DEBUG("buffer[%d] = 0x%02x\n", buf_size, tmp_data);
		buffer[buf_size++] = (tmp_data & 0xff);
		/* Removes leading whitespace from input_copy */
		if (input_copy) {
			input_copy = skip_spaces(input_copy);
			token = strsep(&input_copy, delim);
		} else {
			token = NULL;
		}
	}

	if (lcm_mipi_read_write.read_enable) {
#if defined(CONFIG_VIS_DISPLAY)
		if (is_mi_dev_support_nova()) {
			if(vis_display_get_CurrentUsecaseID()!=0) {
				pr_info("send 335 analog switch on cmd");
				vis_display_ops_analog_switch_on();
			}
		}
#endif
		cmd_msg->channel = 0;
		cmd_msg->tx_cmd_num = 1;
		cmd_msg->type[0] = 0x06;
		tx[0] = buffer[7];
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->tx_len[0] = 1;

		cmd_msg->rx_cmd_num = 1;
		lcm_mipi_read_write.lcm_setting_table.cmd = tx[0];
		if (lcm_mipi_read_write.read_count > 10) {
			read_length = lcm_mipi_read_write.read_count;
			read_buffer_position = 0;
			while (read_length > 0) {
				cmd_msg->rx_buf[0] = temp_read_buffer;
				memset(temp_read_buffer, 0, sizeof(temp_read_buffer));
				cmd_msg->rx_len[0] = read_length > 10 ? 10 : read_length;
				rc = mtk_ddic_dsi_read_cmd(cmd_msg);
				if (rc != 0) {
					pr_err("%s error\n", __func__);
				}
				memcpy(&lcm_mipi_read_write.read_buffer[read_buffer_position], temp_read_buffer, cmd_msg->rx_len[0]);
				read_buffer_position += cmd_msg->rx_len[0];
				read_length -= 10;
			}
		} else {
			cmd_msg->rx_buf[0] = lcm_mipi_read_write.read_buffer;
			memset(cmd_msg->rx_buf[0], 0, lcm_mipi_read_write.read_count);
			cmd_msg->rx_len[0] = lcm_mipi_read_write.read_count;
			rc = mtk_ddic_dsi_read_cmd(cmd_msg);
			if (rc != 0) {
				pr_err("%s error\n", __func__);
			}
			pr_info("read lcm addr:0x%02x--dlen:%d\n",
				*(char *)(cmd_msg->tx_buf[0]), (int)cmd_msg->rx_len[0]);
			for (j = 0; j < cmd_msg->rx_len[0]; j++) {
				pr_info("read lcm addr:0x%02x--byte:%d,val:0x%02x\n",
					*(char *)(cmd_msg->tx_buf[0]), j,
					*(char *)(cmd_msg->rx_buf[0] + j));
			}
		}
#if defined(CONFIG_VIS_DISPLAY)
		if (is_mi_dev_support_nova()) {
			if(vis_display_get_CurrentUsecaseID()!=0) {
				pr_info("send 335 analog switch off cmd");
				vis_display_ops_analog_switch_off();
			}
		}
#endif
		goto exit;
	} else {
		rc = mi_dsi_panel_get_cmd_pkt_count(buffer, buf_size, &packet_count);
			if (!packet_count) {
				DISP_ERROR("get pkt count failed!\n");
				goto exit_free1;
		}

		cmds = kzalloc(sizeof(struct dsi_cmd_desc) * packet_count, GFP_KERNEL);
		if (!cmds) {
			rc = -ENOMEM;
			goto exit_free0;
		}

		rc = mi_dsi_panel_create_cmd_packets(buffer, packet_count, cmds);
		if (rc) {
			DISP_ERROR("panel failed to create cmd packets, rc=%d\n", rc);
			goto exit_free2;
		}

		cmd_msg->channel = 0;
		cmd_msg->flags = MIPI_DSI_MSG_USE_LPM;
		for (i = 0; i < packet_count; i++) {
			cmd_msg->type[j] = cmds[i].msg.type;
			cmd_msg->tx_len[j] = cmds[i].msg.tx_len;
			cmd_msg->tx_buf[j] = cmds[i].msg.tx_buf;
			if (cmds[i].last_command) {
				cmd_msg->tx_cmd_num = j + 1;
				mtk_ddic_dsi_send_cmd(cmd_msg, true);
				j = 0;
			} else
				j++;
		}

		for (i = 0; i < packet_count; i++) {
			pr_info("send lcm tx_len[%d]=%zu\n",
				i, cmds[i].msg.tx_len);
			for (j = 0; j < cmds[i].msg.tx_len; j++) {
				pr_info(
					"send lcm type[%d]=0x%x, tx_buf[%d]--byte:%d,val:%x\n",
					i, cmds[i].msg.type, i, j,
					((char *)(cmds[i].msg.tx_buf))[j]);
			}
		}
	}

	rc = 0;

	for (i = 0; i < packet_count; i++) {
		kfree(cmds[i].msg.tx_buf);
	}

exit_free2:
	kfree(cmds);
exit_free1:
	kfree(buffer);
exit_free0:
	kfree(input_dup);
exit:
	vfree(cmd_msg);
#ifdef CONFIG_MI_DISP_DFS_EVENT
	if (rc != 0) {
		mi_event.event_type = MI_EVENT_CMD_SEND_FAILED;
		mi_disp_mievent_int(MI_DISP_PRIMARY, &mi_event);
	}
#endif
	return rc;
}

ssize_t  mi_dsi_panel_read_mipi_reg(char *buf)
{
	int i = 0;
	ssize_t count = 0;
	DISP_INFO("lcm_mipi_read_write.read_count =%d\n",lcm_mipi_read_write.read_count);
	if(buf == NULL) {
		DISP_INFO("buf == NULL\n");
		return 0;
	}

	if (lcm_mipi_read_write.read_enable) {
		for (i = 0; i < lcm_mipi_read_write.read_count; i++) {
			if (lcm_mipi_read_write.lcm_setting_table.cmd == 0x52 &&
					lcm_mipi_read_write.read_inversion == 1) { 
				if (i == lcm_mipi_read_write.read_count - 1) {
					count += snprintf(buf + count, PAGE_SIZE - count, "0x%02X\n",
					     lcm_mipi_read_write.read_buffer[lcm_mipi_read_write.read_count-i-1]);
				} else {
					count += snprintf(buf + count, PAGE_SIZE - count, "0x%02X,",
					     lcm_mipi_read_write.read_buffer[lcm_mipi_read_write.read_count-i-1]);
				}
			} else {
				if (i == lcm_mipi_read_write.read_count - 1) {
					count += snprintf(buf + count, PAGE_SIZE - count, "0x%02X\n",
						lcm_mipi_read_write.read_buffer[i]);
				} else {
					count += snprintf(buf + count, PAGE_SIZE - count, "0x%02X,",
					     lcm_mipi_read_write.read_buffer[i]);
				}
			}
		}
	}
	return count;
}

ssize_t  mi_dsi_panel_read_mipi_reg_cp(char *exitbuf, char *enterbuf, int read_buf_pos)
{
	int i = 0;
	int j = 0;
	ssize_t count = 0;
	if(exitbuf == NULL || enterbuf == NULL) {
		DISP_INFO("buf == NULL\n");
		return 0;
	}

	DISP_INFO("lcm_mipi_read_write.read_count = %d\n",lcm_mipi_read_write.read_count);
	if (lcm_mipi_read_write.read_enable) {
		for (i = read_buf_pos; i < read_buf_pos + lcm_mipi_read_write.read_count; i++) {
			exitbuf[i] = lcm_mipi_read_write.read_buffer[j];
			enterbuf[i] = exitbuf[i];
			DISP_DEBUG(" exitbuf[%d] %x", i, exitbuf[i]);
			count++;
			j++;
		}

	}

	return count;
}

void  mi_dsi_panel_rewrite_enterDClut(char *exitDClut, char *enterDClut, int count)
{
	int i = 0, j = 0;
	DISP_DEBUG("mi_dsi_panel_rewrite_enterDClut +\n");
	if(exitDClut == NULL || enterDClut == NULL) {
		DISP_ERROR("buf == NULL\n");
		return;
	}

	DISP_DEBUG("mi_dsi_panel_rewrite_enterDClut count = %d\n", count);
	for (i = 0; i < count/5; i++) {
		for (j = i * 5; j < (i * 5 + 3) ; j++) {
			enterDClut[j] = exitDClut[i * 5 + 3];
		}
	}

	for (i = 0; i < count; i++)
		DISP_DEBUG(" enterDClut[%d] %x", i, enterDClut[i]);

	return;
}

static int mi_get_dc_lut(char* page4buf, char*offsetbuf, char *read_exitDClut, char* exitDClut, char* enterDClut, int read_buf_pos)
{
	ssize_t write_count = 0;
	ssize_t read_count = 0;

	DISP_DEBUG("+\n");
	write_count = dsi_panel_write_mipi_reg(page4buf, strlen(page4buf) + 1);
	if(!write_count) {
		DISP_ERROR("DC LUT switch to page4 failed\n");
		goto end;
	}
	write_count = dsi_panel_write_mipi_reg(offsetbuf, strlen(offsetbuf) + 1);
	if(!write_count) {
		DISP_ERROR("DC LUT switch to offsetbuf failed\n");
		goto end;
	}
	write_count = dsi_panel_write_mipi_reg(read_exitDClut, strlen(read_exitDClut) + 1);
	if(!write_count) {
		DISP_ERROR("DC LUT switch to read_exitDClut failed\n");
		goto end;
	}
	read_count = mi_dsi_panel_read_mipi_reg_cp(exitDClut, enterDClut, read_buf_pos);
	DISP_DEBUG("read_count = %zd\n", read_count);
	if(!read_count) {
		DISP_ERROR("DC LUT read exitDClut failed\n");
		goto end;
	}
end:
	DISP_DEBUG("-\n");
	return read_count;
}

ssize_t mi_dsi_panel_read_and_update_dc_param(struct mtk_dsi *dsi)
{
	char page4buf[24] = {0};
	char offsetbuf[12] = {0};
	char read_exitDClut60[9] = {0};
	char read_exitDClut120[12] = {0};
	char exitDClut60[75] = {0};
	char enterDClut60[75] = {0};
	char exitDClut120[75] = {0};
	char enterDClut120[75] = {0};
	char read_buf[3] = {0};
	int count = 0;
	struct mtk_panel_ext *panel_ext = NULL;
	struct mtk_ddp_comp *comp =  NULL;

	int offset_count = 0;
	int read_length = 75;
	int len = 0;
	int read_buf_pos = 0;
	int read_offset = 0;

	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	DISP_INFO("+\n");

	strcpy(page4buf, "00 05 F0 55 AA 52 08 04");
	strcpy(offsetbuf, "00 01 6F 0A");
	strcpy(read_exitDClut60, "01 0A D2");
	strcpy(read_exitDClut120, "01 0A D4");

	while (read_length > 0) {
		len = read_length > 10 ? 10 : read_length;
		DISP_DEBUG("read_length = %d len = %d read_buf_pos = %d\n", read_length, len, read_buf_pos);
		offset_count = snprintf(offsetbuf + strlen(offsetbuf) - 2, 3, "%02x", read_offset);
		DISP_DEBUG("offsetbuf = %s strlen(offsetbuf) = %zu offset_count = %d\n", offsetbuf, strlen(offsetbuf), offset_count);
		offset_count = snprintf(read_buf, 3, "%02x", len);
		DISP_DEBUG("read_buf = %s strlen(read_buf) = %zu offset_count = %d len = %d\n", read_buf, strlen(read_buf), offset_count, len);
		read_exitDClut60[3] = read_buf[0];
		read_exitDClut60[4] = read_buf[1];
		read_exitDClut120[3] = read_buf[0];
		read_exitDClut120[4] = read_buf[1];
		DISP_DEBUG("read_exitDClut60 = %s strlen(read_exitDClut60) = %zu offset_count = %d len = %d\n", read_exitDClut60, strlen(read_exitDClut60), offset_count, len);
		DISP_DEBUG("read_exitDClut120 = %s strlen(read_exitDClut120) = %zu offset_count = %d len = %d\n", read_exitDClut120, strlen(read_exitDClut120), offset_count, len);
		count = mi_get_dc_lut(page4buf, offsetbuf, read_exitDClut60, exitDClut60, enterDClut60, read_buf_pos);
		if(count < 0){
			DISP_ERROR("mi_get_dc_lut 60hz count = %d\n", count);
			goto end;
		}
		count = mi_get_dc_lut(page4buf, offsetbuf, read_exitDClut120, exitDClut120, enterDClut120, read_buf_pos);
		if(count < 0){
			DISP_ERROR("mi_get_dc_lut 120hz count = %d\n", count);
			goto end;
		}
		read_buf_pos += len;
		read_length -= 10;
		read_offset += 10;
	}
	DISP_DEBUG("mi_dsi_panel_read_and_update_dc_param start read_buf_pos = %d 60hz\n", read_buf_pos);
	mi_dsi_panel_rewrite_enterDClut(exitDClut60, enterDClut60, read_buf_pos);
	DISP_DEBUG("mi_dsi_panel_read_and_update_dc_param start read_buf_pos = %d 120hz\n", read_buf_pos);
	mi_dsi_panel_rewrite_enterDClut(exitDClut120, enterDClut120, read_buf_pos);

	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->panel_set_dc_lut_params)) {
		pr_info("panel_set_dc_lut_params func not defined");
		goto end;
	} else {
		panel_ext->funcs->panel_set_dc_lut_params(dsi->panel, exitDClut60, enterDClut60, exitDClut120, enterDClut120, read_buf_pos);
	}
end:
	DISP_INFO("-\n");
	return 0;
}

int mi_dsi_panel_write_dsi_cmd(struct dsi_cmd_rw_ctl *ctl)
{
	int rc = 0;
	u32 packet_count = 0;
	int i = 0, j = 0;
	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	struct dsi_cmd_desc *cmds = NULL;

	if (!ctl->tx_len || !ctl->tx_ptr) {
		DISP_ERROR("panel invalid params\n");
		rc = -EINVAL;
		goto exit;
	}

	rc = mi_dsi_panel_get_cmd_pkt_count(ctl->tx_ptr, ctl->tx_len, &packet_count);
		if (!packet_count) {
			DISP_ERROR("get pkt count failed!\n");
			goto exit;
	}

	cmds = kzalloc(sizeof(struct dsi_cmd_desc) * packet_count, GFP_KERNEL);
	if (!cmds) {
		rc = -ENOMEM;
		goto exit;
	}

	rc = mi_dsi_panel_create_cmd_packets(ctl->tx_ptr, packet_count, cmds);
	if (rc) {
		DISP_ERROR("panel failed to create cmd packets, rc=%d\n", rc);
		goto exit_free1;
	}

	cmd_msg->channel = 0;
	if (ctl->tx_state == MI_DSI_CMD_LP_STATE) {
		cmd_msg->flags = MIPI_DSI_MSG_USE_LPM;
	} else if (ctl->tx_state == MI_DSI_CMD_HS_STATE) {
		cmd_msg->flags = 0;
	} else {
		DISP_ERROR("panel command state unrecognized-%d\n", ctl->tx_state);
		goto exit_free2;
	}
	for (i = 0; i < packet_count; i++) {
		cmd_msg->type[j] = cmds[i].msg.type;
		cmd_msg->tx_len[j] = cmds[i].msg.tx_len;
		cmd_msg->tx_buf[j] = cmds[i].msg.tx_buf;
		if (cmds[i].last_command) {
			cmd_msg->tx_cmd_num = j + 1;
			mtk_ddic_dsi_send_cmd(cmd_msg, true);
			j = 0;
		} else
			j++;
		}

	for (i = 0; i < packet_count; i++) {
		pr_debug("send lcm tx_len[%d]=%zu\n",
			i, cmds[i].msg.tx_len);
		for (j = 0; j < cmds[i].msg.tx_len; j++) {
			pr_debug(
				"send lcm type[%d]=0x%hhu, tx_buf[%d]--byte:%d,val:%c\n",
				i, cmds[i].msg.type, i, j,
				((char *)(cmds[i].msg.tx_buf))[j]);
		}
	}

exit_free2:
	if (ctl->tx_len && ctl->tx_ptr) {
		for (i = 0; i < packet_count; i++) {
			kfree(cmds[i].msg.tx_buf);
		}
	}
exit_free1:
	if (ctl->tx_len && ctl->tx_ptr) {
		kfree(cmds);
	}
exit:
	vfree(cmd_msg);
	return rc;
}

static int dsi_panel_get_lockdown_from_cmdline(unsigned char *plockdowninfo)
{
	int ret = -1;

	if (sscanf(lockdown_info, "0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx",
		&plockdowninfo[0], &plockdowninfo[1], &plockdowninfo[2], &plockdowninfo[3],
		&plockdowninfo[4], &plockdowninfo[5], &plockdowninfo[6], &plockdowninfo[7]) != 8) {
		pr_err("failed to parse lockdown info from cmdline !\n");
	} else {
		if(plockdowninfo[0] == 0 && plockdowninfo[1] == 0 &&
			plockdowninfo[2] == 0 && plockdowninfo[3] == 0 && plockdowninfo[4] == 0 &&
			plockdowninfo[5] == 0 && plockdowninfo[6] == 0 && plockdowninfo[7] == 0)
		{
			pr_err("No panel is Connected !\n");
			ret = -1;
		}
		pr_info("lockdown info from cmdline = 0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,"
			"0x%02hhx,0x%02hhx,0x%02hhx",
			plockdowninfo[0], plockdowninfo[1], plockdowninfo[2], plockdowninfo[3],
			plockdowninfo[4], plockdowninfo[5], plockdowninfo[6], plockdowninfo[7]);
		if (plockdowninfo[1] == 0x42 || plockdowninfo[1] == 0x36) {
			ret = 0;
		} else {
			ret = -1;
		}
	}
	return ret;
}

int get_lockdown_info_for_nvt(unsigned char* p_lockdown_info) {
	int ret = 0;
	int i = 0;

	/* CMD2 Page2 is selected */
	char select_page_cmd[] = {0xFF, 0x22};
	char restore_default_page[] = {0xFF, 0x10};
	u8 tx[10] = {0};
	struct mtk_ddic_dsi_msg *cmd_msg;

	if (!p_lockdown_info)
		return -EINVAL;

	if (!dsi_panel_get_lockdown_from_cmdline(p_lockdown_info))
		return 0;

	cmd_msg = vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	if (!cmd_msg)
		goto NOMEM;

	cmd_msg->channel = 1;
	cmd_msg->flags = 2;
	cmd_msg->tx_cmd_num = 1;

	cmd_msg->type[0] = 0x15;
	cmd_msg->tx_buf[0] = select_page_cmd;
	cmd_msg->tx_len[0] = 2;

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true);
	if (ret != 0) {
		pr_err("%s error\n", __func__);
		goto DONE2;
	}

	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	/* Read the first 8 byte of CMD2 page1 0x00 reg */
	cmd_msg->channel = 0;
	cmd_msg->tx_cmd_num = 1;
	cmd_msg->type[0] = 0x06;
	tx[0] = 0x00;
	cmd_msg->tx_buf[0] = tx;
	cmd_msg->tx_len[0] = 1;

	cmd_msg->rx_cmd_num = 1;
	cmd_msg->rx_buf[0] = kzalloc(8 * sizeof(unsigned char), GFP_KERNEL);
	if (!cmd_msg->rx_buf[0]) {
		pr_err("%s: memory allocation failed\n", __func__);
		ret = -ENOMEM;
		goto DONE2;
	}

	cmd_msg->rx_len[0] = 8;
	ret = mtk_ddic_dsi_read_cmd(cmd_msg);
	if (ret != 0) {
		pr_err("%s error\n", __func__);
		goto DONE1;
	}

	for (i = 0; i < 8; i++) {
		pr_info("read lcm addr:0x%x--byte:%d,val:0x%02hhx\n",
				*(unsigned char *)(cmd_msg->tx_buf[0]), i,
				*(unsigned char *)(cmd_msg->rx_buf[0] + i));
		p_lockdown_info[i] = *(unsigned char *)(cmd_msg->rx_buf[0] + i);
	}

	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));
	cmd_msg->channel = 1;
	cmd_msg->flags = 2;
	cmd_msg->tx_cmd_num = 1;

	cmd_msg->type[0] = 0x15;
	cmd_msg->tx_buf[0] = restore_default_page;
	cmd_msg->tx_len[0] = sizeof(restore_default_page) / sizeof(char);

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true);
	if (ret != 0) {
		pr_err("%s send cmd error\n", __func__);
	}

DONE1:
	kfree(cmd_msg->rx_buf[0]);
DONE2:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);
	return ret;

NOMEM:
	pr_err("%s: memory allocation failed\n", __func__);
	return -ENOMEM;
}
EXPORT_SYMBOL(get_lockdown_info_for_nvt);

ssize_t  led_i2c_set_backlight(struct mtk_dsi *dsi, unsigned int level)
{
	ssize_t retval = -EINVAL;
	struct mtk_ddp_comp *comp = NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	pr_info("%s +\n", __func__);
	comp = &dsi->ddp_comp;
	if(comp == NULL) {
		pr_info("[%s], comp is NULL", __func__);
		return retval;
	}
	panel_ext = mtk_dsi_get_panel_ext(comp);
	if (panel_ext && panel_ext->funcs && panel_ext->funcs->set_backlight_i2c) {
		panel_ext->funcs->set_backlight_i2c(dsi->panel, level);
		retval = 0;
	} else {
		pr_info("[%s], fail to set backlight", __func__);
	}

	return retval;
}

ssize_t  led_i2c_reg_write(struct mtk_dsi *dsi, char *buf, unsigned long  count)
{
	ssize_t retval = -EINVAL;
	unsigned int read_enable = 0;
	unsigned int packet_count = 0;
	char register_addr = 0;
	char *input = NULL;
	unsigned char pbuf[3] = {0};

	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	pr_info("%s +\n", __func__);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	pr_info("[%s], count  = %ld, buf = %s ", __func__, count, buf);

	if (count < 9 || buf == NULL) {
		/* 01 01 01      -- read 0x01 register, len:1*/
		/* 00 01 08 17 -- write 0x17 to 0x08 register,*/
		pr_info("[%s], command is invalid, count  = %ld,buf = %s ", __func__, count, buf);
		return retval;
	}

	input = buf;
	memcpy(pbuf, input, 2);
	pbuf[2] = '\0';
	retval = kstrtou32(pbuf, 10, &read_enable);
	if (retval)
		return retval;
	lcm_led_i2c_read_write.read_enable = !!read_enable;
	input = input + 3;
	memcpy(pbuf, input, 2);
	pbuf[2] = '\0';
	packet_count = (unsigned int)string_to_hex(pbuf);
	if (lcm_led_i2c_read_write.read_enable && !packet_count) {
		retval = -EINVAL;
		return retval;
	}
	input = input + 3;
	memcpy(pbuf, input, 2);
	pbuf[2] = '\0';
	register_addr = string_to_hex(pbuf);
	if (lcm_led_i2c_read_write.read_enable) {
		lcm_led_i2c_read_write.read_count = packet_count;
		memset(lcm_led_i2c_read_write.buffer, 0, sizeof(lcm_led_i2c_read_write.buffer));
		lcm_led_i2c_read_write.buffer[0] = (unsigned char)register_addr;
		if (panel_ext->funcs->led_i2c_reg_op)
			retval = panel_ext->funcs->led_i2c_reg_op(lcm_led_i2c_read_write.buffer,
							KTZ8863A_REG_READ, lcm_led_i2c_read_write.read_count);
	} else {
		if (count < 12) {
			pr_err("%s params count error", __func__);
			return retval;
		}

		memset(lcm_led_i2c_read_write.buffer, 0, sizeof(lcm_led_i2c_read_write.buffer));
		lcm_led_i2c_read_write.buffer[0] = (unsigned char)register_addr;
		input = input + 3;
		memcpy(pbuf, input, 2);
		pbuf[2] = '\0';
		lcm_led_i2c_read_write.buffer[1] = (unsigned char)string_to_hex(pbuf);

		if (panel_ext->funcs->led_i2c_reg_op)
			retval = panel_ext->funcs->led_i2c_reg_op(lcm_led_i2c_read_write.buffer, KTZ8863A_REG_WRITE, 0);
	}

	return retval <= 0 ? retval : count;
}

ssize_t  led_i2c_reg_read(struct mtk_dsi *dsi, char *buf)
{
	int i = 0;
	ssize_t count = 0;

	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	pr_info("%s +\n", __func__);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	if (lcm_led_i2c_read_write.read_enable) {
		for (i = 0; i < lcm_led_i2c_read_write.read_count; i++) {
			if (i ==  lcm_led_i2c_read_write.read_count - 1) {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02x\n",
				     lcm_led_i2c_read_write.buffer[i]);
			} else {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02x ",
				     lcm_led_i2c_read_write.buffer[i]);
			}
		}
	}
	return count;
}

int mi_disp_panel_ddic_send_cmd(struct LCM_setting_table *table,
	unsigned int count, unsigned int format)
{
	int i = 0, j = 0, k = 0;
	int ret = 0;
	unsigned char cmd;
	bool wait_te_send;
	bool block;
	bool lp_mode;
	bool no_lock = false;
	bool gce_block = false;

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 1,
		.tx_cmd_num = count,
	};

	if (table == NULL) {
		pr_err("invalid ddic cmd \n");
		return ret;
	}

	if (count == 0 || count > MAX_TX_CMD_NUM) {
		pr_err("cmd count invalid, value:%d \n", count);
		return ret;
	}

	wait_te_send = ((format & FORMAT_WAIT_TE_SEND) == 0)? false: true;
	block = ((format & FORMAT_BLOCK) == 0)? false: true;
	lp_mode = ((format & FORMAT_LP_MODE) == 0)? false: true;
	no_lock = ((format & FORMAT_NOLOCK) == 0)? false: true;
	gce_block = ((format & FORMAT_GCE_BLOCK) == 0)? false: true;

	if (count > LP_MODE_CMD_NUM_LIMITITION) {
		lp_mode = false;
		pr_info("cmd count is too much, force high-speed");
	}

	cmd_msg.flags = (lp_mode == true)? MIPI_DSI_MSG_USE_LPM: 1;

	pr_info("%s wait_te:%d, block: %d, lp_mode: %d, conut:%d, no_lock:%d, gce_block:%d\n",
		__func__, wait_te_send, block, lp_mode, count, no_lock, gce_block);
	for (i = 0;i < count; i++) {
		memset(temp[i], 0, sizeof(temp[i]));
		/* LCM_setting_table format: {cmd, count, {para_list[]}} */
		cmd = (u8)table[i].cmd;
		temp[i][0] = cmd;
		for (j = 0; j < table[i].count; j++) {
			temp[i][j+1] = table[i].para_list[j];
		}

		cmd_msg.type[i] = table[i].count > 1 ? 0x39 : 0x15;
		cmd_msg.tx_buf[i] = temp[i];
		cmd_msg.tx_len[i] = table[i].count + 1;

		for (k = 0; k < cmd_msg.tx_len[i]; k++) {
			DISP_DEBUG("%s cmd_msg.tx_buf:0x%02x\n", __func__, temp[i][k]);
		}

		DISP_DEBUG("%s cmd_msg.tx_len:%zu\n", __func__, cmd_msg.tx_len[i]);
	}
	if (wait_te_send)
		ret = mtk_ddic_dsi_wait_te_send_cmd(&cmd_msg, block);
	else
		ret = mtk_ddic_dsi_send_cmd_with_lock(&cmd_msg, block, no_lock, gce_block);

	if (ret != 0) {
		pr_err("%s: failed to send ddic cmd\n", __func__);
	}
	return ret;
}
EXPORT_SYMBOL(mi_disp_panel_ddic_send_cmd);

int mi_dsi_panel_get_panel_info(struct mtk_dsi *dsi,
			char *buf)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	pr_info("%s +\n", __func__);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->get_panel_info)) {
		pr_info("%s get_panel_info func not defined", __func__);
		return 0;
	} else {
		return panel_ext->funcs->get_panel_info(dsi->panel, buf);
	}
}

int mi_dsi_panel_get_wp_info(struct mtk_dsi *dsi, char *buf, size_t size)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	int count = 0;

	pr_info("%s: +\n", __func__);

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		goto err;
	}

	if(dsi->ddp_comp.mtk_crtc)
		mtk_crtc = dsi->ddp_comp.mtk_crtc;

	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);
	if (panel_ext && panel_ext->funcs && panel_ext->funcs->get_wp_info) {
		if (mtk_drm_lcm_is_connect(mtk_crtc))
			count = panel_ext->funcs->get_wp_info(dsi->panel, buf, size);
		else {
			DISP_ERROR("panel is not connetced\n");
			count = panel_ext->funcs->get_wp_info(NULL, buf, size);
		}
	}

err:
#ifdef CONFIG_MI_DISP_DFS_EVENT
	if (!count)
		mi_disp_mievent_str(MI_EVENT_PANEL_WP_READ_FAILED);
#endif
	pr_info("%s: -\n", __func__);
	return count;
}

int mi_dsi_panel_get_sn_info(struct mtk_dsi *dsi, char *buf, size_t size)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	int count = 0;
	int i =0;
	pr_info("%s: +\n", __func__);

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		goto err;
	}

	if(dsi->ddp_comp.mtk_crtc)
		mtk_crtc = dsi->ddp_comp.mtk_crtc;

	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);
	if (panel_ext && panel_ext->funcs && panel_ext->funcs->get_sn_info) {
		if (mtk_drm_lcm_is_connect(mtk_crtc)){
			count = panel_ext->funcs->get_sn_info(dsi->panel, buf, size);
			for(i=0; i<14; i++)
			DISP_ERROR("panel is sn[%d] =%02x \n", i, buf[i]);
			}
		else {
			DISP_ERROR("panel is not connetced\n");
			count = panel_ext->funcs->get_sn_info(NULL, buf, size);
		}
	}

err:
#ifdef CONFIG_MI_DISP_DFS_EVENT
	//if (!count)
		//mi_disp_mievent_str(MI_EVENT_PANEL_WP_READ_FAILED);
#endif
	pr_info("%s: -\n", __func__);
	return count;
}

int mi_dsi_panel_get_grayscale_info(struct mtk_dsi *dsi, char *buf, size_t size)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;
	int count = 0;

	pr_info("%s: +\n", __func__);

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		goto err;
	}

	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);
	if (panel_ext && panel_ext->funcs && panel_ext->funcs->get_grayscale_info) {
		count = panel_ext->funcs->get_grayscale_info(dsi->panel, buf, size);
	}

err:
	pr_info("%s: -\n", __func__);
	return count;
}

int mi_dsi_panel_get_fps(struct mtk_dsi *dsi,
			u32 *fps)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	pr_debug("%s +\n", __func__);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->get_panel_dynamic_fps)) {
		pr_info("%s get_panel_dynamic_fps func not defined", __func__);
		return 0;
	} else {
		return panel_ext->funcs->get_panel_dynamic_fps(dsi->panel, fps);
	}
}
int mi_dsi_panel_get_ic_type(struct mtk_dsi *dsi,
			u32 *ic_type)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	pr_info("%s +\n", __func__);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->get_ic_type)) {
		pr_info("%s get_ic_type func not defined", __func__);
		return -1;
	} else {
		return panel_ext->funcs->get_ic_type(dsi->panel, ic_type);
	}
}

int mi_dsi_panel_get_max_brightness_clone(struct mtk_dsi *dsi,
			u32 *max_brightness_clone)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	pr_info("%s +\n", __func__);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->get_panel_max_brightness_clone)) {
		pr_info("%s get_panel_max_brightness_clone func not defined", __func__);
		return -1;
	} else {
		return panel_ext->funcs->get_panel_max_brightness_clone(dsi->panel, max_brightness_clone);
	}
}

int mi_dsi_panel_get_factory_max_brightness(struct mtk_dsi *dsi,
			u32 *max_brightness_clone)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;
	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	pr_info("%s +\n", __func__);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);
	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->get_panel_factory_max_brightness)) {
		pr_info("%s get_panel_max_brightness_clone func not defined", __func__);
		return -1;
	} else {
		return panel_ext->funcs->get_panel_factory_max_brightness(dsi->panel, max_brightness_clone);
	}
}

int mi_dsi_panel_set_doze_brightness(struct mtk_dsi *dsi,
			int doze_brightness)
{
	int ret = 0;
	int set_doze_ret = 0;
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	pr_info("%s +, doze_brightness = %d\n", __func__, doze_brightness);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	mutex_lock(&dsi->dsi_lock);

	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->set_doze_brightness)) {
		pr_info("%s set_doze_brightness func not defined", __func__);
		ret = 0;
	} else {
#ifdef CONFIG_MI_DISP_FOD_SYNC
		mi_disp_feature_event_notify_by_type(mi_get_disp_id("primary"), MI_DISP_EVENT_DOZE,
			sizeof(doze_brightness), doze_brightness + DOZE_BRIGHTNESS_MAX);
		if (panel_ext && panel_ext->params && panel_ext->params->aod_delay_enable) {
			if (doze_brightness && !(dsi->mi_cfg.aod_wait_frame)) {
				ret = wait_for_completion_timeout(&dsi->aod_wait_completion, msecs_to_jiffies(200));
				pr_info("aod wait_for_completion_timeout return %d\n", ret);
				dsi->mi_cfg.aod_wait_frame = true;
			}
		}
#endif
#ifdef CONFIG_MI_DISP_VDO_AOD
		mi_disp_feature_event_notify_by_type(mi_get_disp_id("primary"), MI_DISP_EVENT_DOZE,
			sizeof(doze_brightness), doze_brightness + DOZE_BRIGHTNESS_MAX);
#endif

		set_doze_ret = panel_ext->funcs->set_doze_brightness(dsi->panel, doze_brightness);
		if (set_doze_ret != 0) {
			DISP_ERROR("set panel doze brightness failed\n");
		}
		mi_disp_feature_event_notify_by_type(mi_get_disp_id("primary"), MI_DISP_EVENT_DOZE, sizeof(doze_brightness), doze_brightness);
		dsi->mi_cfg.feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] = doze_brightness;
	}

	mutex_unlock(&dsi->dsi_lock);
	return ret;
}EXPORT_SYMBOL(mi_dsi_panel_set_doze_brightness);

int mi_dsi_panel_get_doze_brightness(struct mtk_dsi *dsi,
			u32 *doze_brightness)
{
	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	pr_info("%s +\n", __func__);
	mutex_lock(&dsi->dsi_lock);
	*doze_brightness = dsi->mi_cfg.feature_val[DISP_FEATURE_DOZE_BRIGHTNESS];
	mutex_unlock(&dsi->dsi_lock);
	return 0;
}

int mi_dsi_panel_set_brightness(struct mtk_dsi *dsi,
			int brightness)
{
	int ret = 0;
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	pr_info("%s +, brightness = %d\n", __func__, brightness);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);
	mutex_lock(&dsi->dsi_lock);
	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->setbacklight_control)) {
		pr_info("%s set_backlight_control func not defined", __func__);
		ret = 0;
	} else {
		if (!panel_ext->funcs->setbacklight_control(dsi->panel, brightness)) {
			mi_disp_feature_event_notify_by_type(mi_get_disp_id("primary"), MI_DISP_EVENT_DOZE, sizeof(brightness), brightness);
			dsi->mi_cfg.last_bl_level = brightness;
		}
	}

	mutex_unlock(&dsi->dsi_lock);
	return ret;
}

int mi_dsi_panel_get_brightness(struct mtk_dsi *dsi,
			u32 *brightness)
{
	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	pr_info("%s +\n", __func__);
	mutex_lock(&dsi->dsi_lock);
	*brightness = dsi->mi_cfg.last_bl_level;
	mutex_unlock(&dsi->dsi_lock);
	return 0;
}

int mi_dsi_panel_set_disp_param(struct mtk_dsi *dsi, struct disp_feature_ctl *ctl)
{
	int rc = 0;
	bool need_lock = false;
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;
	struct drm_panel *panel = NULL;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	struct mtk_drm_private *private = NULL;
	struct disp_feature_ctl *ctl_temp = NULL;

	if (!dsi || !ctl) {
		pr_err("NULL dsi or ctl\n");
		return 0;
	}

	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);
	panel = dsi->panel;
	mi_cfg = &dsi->mi_cfg;

	ctl_temp = kmalloc(sizeof(ctl), GFP_KERNEL);
	if (!ctl_temp) {
		pr_err("%s %d disp_feature_ctl clt_temp kmalloc failed!", __func__, __LINE__);
		return -EFAULT;
	}
	memcpy(ctl_temp, ctl, sizeof(ctl));

	DISP_UTC_INFO("panel feature: %s, value: %d\n",
		get_disp_feature_id_name(ctl_temp->feature_id), ctl_temp->feature_val);

	if (!(panel_ext && panel_ext->funcs)) {
		pr_info("panel_ext func not defined\n");
		kfree(ctl_temp);
		return 0;
	}

	if (!dsi_panel_initialized(dsi)){
		if (DISP_FEATURE_DBI == ctl_temp->feature_id
				&& panel_ext->funcs->set_gray_by_temperature) {
			panel_ext->funcs->set_gray_by_temperature(dsi->panel, ctl_temp->feature_val);
			mi_cfg->feature_val[DISP_FEATURE_DBI] = ctl_temp->feature_val;
		}
		DISP_ERROR("Panel not initialized!\n");
		kfree(ctl_temp);
		return 0;
	}

	if (dsi->encoder.crtc && dsi->encoder.crtc->dev && dsi->encoder.crtc->dev->dev_private)
		private = dsi->encoder.crtc->dev->dev_private;

	mutex_lock(&dsi->dsi_lock);
	switch (ctl_temp->feature_id) {
	case DISP_FEATURE_DIMMING:
		if (!panel_ext->funcs->panel_elvss_control)
			break;
		if (ctl_temp->feature_val == FEATURE_ON)
			panel_ext->funcs->panel_elvss_control(dsi->panel, true);
		else
			panel_ext->funcs->panel_elvss_control(dsi->panel, false);
		mi_cfg->feature_val[DISP_FEATURE_DIMMING] = ctl_temp->feature_val;
		
		break;
	case DISP_FEATURE_HBM:
		mi_cfg->feature_val[DISP_FEATURE_HBM] = ctl_temp->feature_val;
		mi_disp_feature_event_notify_by_type(mi_get_disp_id("primary"), MI_DISP_EVENT_HBM, sizeof(ctl_temp->feature_val), ctl_temp->feature_val);
		if (panel_ext->funcs->normal_hbm_control) {
			if (ctl_temp->feature_val == FEATURE_ON) {
				panel_ext->funcs->normal_hbm_control(dsi->panel, 1);
				dsi->mi_cfg.normal_hbm_flag = true;
			} else {
				panel_ext->funcs->normal_hbm_control(dsi->panel, 0);
				dsi->mi_cfg.normal_hbm_flag = false;
			}
		}
		break;
	case DISP_FEATURE_HBM_FOD:
		mi_cfg->feature_val[DISP_FEATURE_HBM_FOD] = ctl_temp->feature_val;
		if (panel_ext->funcs->hbm_fod_control) {
			if (ctl_temp->feature_val == FEATURE_ON) {
				panel_ext->funcs->hbm_fod_control(dsi->panel, true);
			} else {
				panel_ext->funcs->hbm_fod_control(dsi->panel, false);
			}
		}
		break;
	case DISP_FEATURE_LOCAL_HBM:
		mi_cfg->feature_val[DISP_FEATURE_LOCAL_HBM] = ctl_temp->feature_val;
		if (panel_ext->funcs->set_lhbm_fod)
			panel_ext->funcs->set_lhbm_fod(dsi, ctl_temp->feature_val);

		break;
	case DISP_FEATURE_BRIGHTNESS:
		if (!panel_ext->funcs->setbacklight_control) {
			pr_info("%s setbacklight_control func not defined", __func__);
		} else {
			if (!panel_ext->funcs->setbacklight_control(dsi->panel, ctl_temp->feature_val))
				dsi->mi_cfg.last_bl_level = ctl_temp->feature_val;
		}
		break;
	case DISP_FEATURE_DOZE_BRIGHTNESS:
		DISP_INFO("DOZE BRIGHTNESS:%d\n", ctl_temp->feature_val);
#if IS_ENABLED(CONFIG_DRM_PANEL_N11A_42_02_0A_DSC_VDO) || IS_ENABLED(CONFIG_DRM_PANEL_N11A_41_02_0B_DSC_VDO)
		if (is_support_doze_brightness(ctl_temp->feature_val)) {
			panel_ext->funcs->set_doze_brightness(dsi->panel, ctl_temp->feature_val);
			mi_cfg->feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] = ctl_temp->feature_val;
#else
		if (!panel_ext->funcs->set_doze_brightness
			|| !panel_ext->funcs->doze_disable
			|| !panel_ext->funcs->doze_enable) {
			DISP_ERROR("doze brightness func not defined\n");
			break;
		}
		if (is_support_doze_brightness(ctl_temp->feature_val)) {
			if (private)
				mutex_lock(&private->commit.lock);
			mtk_drm_idlemgr_kick(__func__, dsi->encoder.crtc, 1);
			if (!dsi->output_en) {
				DISP_ERROR("doze brightness%d setting on dsi disabled\n", ctl_temp->feature_val);
				if (private)
					mutex_unlock(&private->commit.lock);
				break;
			}
			if (ctl_temp->feature_val == DOZE_TO_NORMAL && mi_cfg->feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] != DOZE_TO_NORMAL) {
				panel_ext->funcs->doze_disable(dsi->panel, dsi, mipi_dsi_dcs_write_gce2, NULL);
			} else if (ctl_temp->feature_val != DOZE_TO_NORMAL && mi_cfg->feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] == DOZE_TO_NORMAL) {
				panel_ext->funcs->doze_enable(dsi->panel, dsi, mipi_dsi_dcs_write_gce2, NULL);
				if (panel_ext->funcs->doze_suspend)
					panel_ext->funcs->doze_suspend(dsi->panel, dsi, mipi_dsi_dcs_write_gce2, NULL);
			}
			if (private)
				mutex_unlock(&private->commit.lock);
			panel_ext->funcs->set_doze_brightness(dsi->panel, ctl_temp->feature_val);
			mi_cfg->feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] = ctl_temp->feature_val;
#endif
		} else
			DISP_ERROR("invaild doze brightness%d\n", ctl_temp->feature_val);
		break;
	case DISP_FEATURE_FOD_CALIBRATION_BRIGHTNESS:
		if (ctl_temp->feature_val == -1) {
		    DISP_INFO("FOD calibration brightness restore last_bl_level=%d\n",
			mi_cfg->last_bl_level);
		if (panel_ext->funcs->setbacklight_control)
			panel_ext->funcs->setbacklight_control(dsi->panel, mi_cfg->last_bl_level);
		    mi_cfg->in_fod_calibration = false;
		} else {
		    if (ctl_temp->feature_val >= 0 &&
			ctl_temp->feature_val <= 2047) {
			mi_cfg->in_fod_calibration = true;
					if (panel_ext->funcs->panel_elvss_control)
						panel_ext->funcs->panel_elvss_control(dsi->panel, false);
					if (panel_ext->funcs->setbacklight_control)
						panel_ext->funcs->setbacklight_control(dsi->panel, ctl_temp->feature_val);
			mi_cfg->dimming_state = STATE_NONE;
		    } else {
			mi_cfg->in_fod_calibration = false;
			DISP_ERROR("FOD calibration invalid brightness level:%d\n",
				ctl_temp->feature_val);
		    }
		}
		mi_cfg->feature_val[DISP_FEATURE_FOD_CALIBRATION_BRIGHTNESS] = ctl_temp->feature_val;
		break;
	case DISP_FEATURE_FOD_CALIBRATION_HBM:
		if (ctl_temp->feature_val == -1) {
			DISP_INFO("FOD calibration HBM restore last_bl_level=%d\n",
			    mi_cfg->last_bl_level);
		if (panel_ext->funcs->hbm_fod_control)
			panel_ext->funcs->hbm_fod_control(dsi->panel, false);
		mi_cfg->dimming_state = STATE_DIM_RESTORE;
		mi_cfg->in_fod_calibration = false;
		} else {
			mi_cfg->in_fod_calibration = true;
			mi_cfg->dimming_state = STATE_DIM_BLOCK;
			if (panel_ext->funcs->hbm_fod_control)
				panel_ext->funcs->hbm_fod_control(dsi->panel, true);
		}
		mi_cfg->feature_val[DISP_FEATURE_FOD_CALIBRATION_HBM] = ctl_temp->feature_val;
		break;
	case DISP_FEATURE_SENSOR_LUX:
		DISP_DEBUG("DISP_FEATURE_SENSOR_LUX=%d\n", ctl_temp->feature_val);
		mi_cfg->feature_val[DISP_FEATURE_SENSOR_LUX] = ctl_temp->feature_val;
		break;
	case DISP_FEATURE_LOW_BRIGHTNESS_FOD:
		DISP_INFO("DISP_FEATURE_LOW_BRIGHTNESS_FOD=%d\n", ctl_temp->feature_val);
		mi_cfg->feature_val[DISP_FEATURE_LOW_BRIGHTNESS_FOD] = ctl_temp->feature_val;
		break;
	case DISP_FEATURE_DC:
		if (!panel_ext->funcs->panel_set_dc)
			break;
		DISP_INFO("DC mode state:%d\n", ctl_temp->feature_val);
		if (ctl_temp->feature_val == FEATURE_ON){
			panel_ext->funcs->panel_set_dc(dsi->panel, true);
			need_lock = true;
#if IS_ENABLED(CONFIG_DRM_PANEL_L11_38_0A_0A_DSC_CMD) || IS_ENABLED(CONFIG_DRM_PANEL_L11A_38_0A_0A_DSC_CMD) || IS_ENABLED(CONFIG_DRM_PANEL_L2M_38_0A_0A_DSC_CMD)
#ifdef CONFIG_MI_DISP_SILKY_BRIGHTNESS_CRC
			mtk_ddp_comp_io_cmd(comp, NULL, MI_RESTORE_CRC_LEVEL, &need_lock);
			mtk_ddp_comp_io_cmd(comp, NULL, SET_DC_SYNC_TE_MODE_ON, NULL);
#endif
#endif
		} else {
			panel_ext->funcs->panel_set_dc(dsi->panel, false);
#if IS_ENABLED(CONFIG_DRM_PANEL_L11_38_0A_0A_DSC_CMD) || IS_ENABLED(CONFIG_DRM_PANEL_L11A_38_0A_0A_DSC_CMD) || IS_ENABLED(CONFIG_DRM_PANEL_L2M_38_0A_0A_DSC_CMD)
#ifdef CONFIG_MI_DISP_SILKY_BRIGHTNESS_CRC
			mtk_ddp_comp_io_cmd(comp, NULL, SET_DC_SYNC_TE_MODE_OFF, NULL);
#endif
#endif
		}
		mi_cfg->feature_val[DISP_FEATURE_DC] = ctl_temp->feature_val;
		mi_disp_feature_event_notify_by_type(mi_get_disp_id("primary"), MI_DISP_EVENT_DC, sizeof(ctl_temp->feature_val), ctl_temp->feature_val);
		break;
	case DISP_FEATURE_CRC:
		DISP_INFO("CRC:%d\n", ctl_temp->feature_val);
		if (mi_cfg->crc_state == ctl_temp->feature_val) {
			DISP_INFO("CRC is the same, return\n");
			break;
		}
		switch (ctl_temp->feature_val) {
		case CRC_SRGB:
		{
			if (!panel_ext->funcs->panel_set_crc_srgb)
				break;
			if(mi_cfg->feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] != DOZE_TO_NORMAL){
				DISP_INFO("Has enter doze, crc return\n");
				break;
			}
			pr_info("CRC srgb");
			panel_ext->funcs->panel_set_crc_srgb(dsi->panel);
			break;
		}
		case CRC_P3:
		{
			if (!panel_ext->funcs->panel_set_crc_p3)
				break;
			pr_info("CRC p3");
			panel_ext->funcs->panel_set_crc_p3(dsi->panel);
			break;
		}
		case CRC_P3_D65:
		{
			if (!panel_ext->funcs->panel_set_crc_p3_d65)
				break;
			pr_info("CRC p3 d65");
			panel_ext->funcs->panel_set_crc_p3_d65(dsi->panel);
			break;
		}
		case CRC_P3_FLAT:
		{
			if (!panel_ext->funcs->panel_set_crc_p3_flat)
				break;
			pr_info("CRC p3 flat");
			panel_ext->funcs->panel_set_crc_p3_flat(dsi->panel);
			break;
		}
		case CRC_OFF:
		{
			if (!panel_ext->funcs->panel_set_crc_off)
				break;
			pr_info("CRC off");
			panel_ext->funcs->panel_set_crc_off(dsi->panel);
			break;
		}
		default:
			break;
		}
		mi_cfg->crc_state = ctl_temp->feature_val;
		DISP_INFO(" CRC: end mi_cfg->crc_state = %d\n", mi_cfg->crc_state);
		break;
	case DISP_FEATURE_FLAT_MODE:
	case DISP_FEATURE_GIR:
		if (panel_ext && panel_ext->funcs && panel_ext->funcs->panel_get_gir_status) {
			if (panel_ext->funcs->panel_get_gir_status(dsi->panel)) {
					mi_cfg->gir_state = GIR_ON;
			} else {
					mi_cfg->gir_state = GIR_OFF;
			}
		}
		DISP_INFO("GIR:%d\n", ctl_temp->feature_val);
		if (mi_cfg->gir_state == ctl_temp->feature_val) {
			DISP_INFO("GIR is the same, return\n");
			break;
		}
		if(mi_cfg->feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] != DOZE_TO_NORMAL){
			DISP_INFO("Has enter doze, gir return\n");
			break;
		}
		switch (ctl_temp->feature_val) {
		case GIR_ON:
		{
			if (!panel_ext->funcs->panel_set_gir_on)
				break;
			pr_info("GIR on");
			panel_ext->funcs->panel_set_gir_on(dsi->panel);
			break;
		}
		case GIR_OFF:
		{
			if (!panel_ext->funcs->panel_set_gir_off)
				break;
			pr_info("GIR off");
			panel_ext->funcs->panel_set_gir_off(dsi->panel);
			break;
		}
		default:
			break;
		}
		mi_cfg->gir_state = ctl_temp->feature_val;
		mi_cfg->feature_val[DISP_FEATURE_GIR] = ctl_temp->feature_val;
		mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE] = ctl_temp->feature_val;
		mi_disp_feature_event_notify_by_type(mi_get_disp_id("primary"),
			MI_DISP_EVENT_FLAT_MODE, sizeof(ctl_temp->feature_val), ctl_temp->feature_val);
		break;
	case DISP_FEATURE_FP_STATUS:
		DISP_INFO("DISP_FEATURE_FP_STATUS=%d\n", ctl_temp->feature_val);
		mi_cfg->feature_val[DISP_FEATURE_FP_STATUS] = ctl_temp->feature_val;
		break;
	case DISP_FEATURE_LCD_HBM:

		if(is_support_lcd_hbm_level(ctl_temp->feature_val)) {
			DISP_INFO("DISP_FEATURE_LCD_HBM=%d\n", ctl_temp->feature_val);
			if (panel_ext->funcs->normal_hbm_control) {
				panel_ext->funcs->normal_hbm_control(dsi->panel, ctl_temp->feature_val);
				dsi->mi_cfg.normal_hbm_flag = ctl_temp->feature_val == FEATURE_ON ?
					true:false;
			}
			mi_cfg->feature_val[DISP_FEATURE_LCD_HBM] = ctl_temp->feature_val;
		} else
			DISP_ERROR("invaild lcd hbm level %d\n", ctl_temp->feature_val);
		break;
	case DISP_FEATURE_SPR_RENDER:
		DISP_INFO("SPR:%d\n", ctl_temp->feature_val);
		if (mi_cfg->feature_val[DISP_FEATURE_SPR_RENDER] == ctl_temp->feature_val) {
			DISP_INFO("SPR is the same, return\n");
			break;
		}
		if (panel_ext->funcs->set_spr_status) {
			panel_ext->funcs->set_spr_status(dsi->panel, ctl_temp->feature_val);
		}
		mi_cfg->feature_val[DISP_FEATURE_SPR_RENDER] = ctl_temp->feature_val;
		break;
	case DISP_FEATURE_PEAK_HDR_MODE:
        break;
	case DISP_FEATURE_CABC:
		DISP_INFO("CABC:%d\n", ctl_temp->feature_val);
		if (panel_ext->funcs->set_cabc_mode) {
			panel_ext->funcs->set_cabc_mode(dsi->panel, ctl_temp->feature_val);
		}
		mi_cfg->feature_val[DISP_FEATURE_CABC] = ctl_temp->feature_val;
		break;
	case DISP_FEATURE_DBI:
		DISP_INFO("gray_level:%x\n", ctl_temp->feature_val);
		if (mi_cfg->feature_val[DISP_FEATURE_DBI] == ctl_temp->feature_val) {
			DISP_INFO("gray_level is the same, return\n");
			break;
		}
		if (panel_ext->funcs->set_gray_by_temperature) {
			panel_ext->funcs->set_gray_by_temperature(dsi->panel, ctl_temp->feature_val);
		}
		mi_cfg->feature_val[DISP_FEATURE_DBI] = ctl_temp->feature_val;
		break;
	case DISP_FEATURE_COLORMODE_NOTIFY:
		DISP_INFO("color_mode:%x\n", ctl_temp->feature_val);
		mi_cfg->feature_val[DISP_FEATURE_COLORMODE_NOTIFY] = ctl_temp->feature_val;
		mi_dsi_panel_set_cur_color_mode(ctl_temp->feature_val);
		break;
	case DISP_FEATURE_DOLBY_STATUS:
		DISP_INFO("dolby_status:%x\n", ctl_temp->feature_val);
		mi_cfg->feature_val[DISP_FEATURE_DOLBY_STATUS] = ctl_temp->feature_val;
		mi_dsi_panel_set_cur_dolby_status(ctl_temp->feature_val);
		break;
	case DISP_FEATURE_REPAINT_STATUS:
		if (private) {
			rc = mi_dsi_panel_set_repaint_status(private->helper_opt, ctl_temp->feature_val);
			DISP_INFO("repaint_status:%x, rc=%d\n", ctl_temp->feature_val, rc);
		}
		break;
#if defined(CONFIG_VIDLE_ENABLE)
	case DISP_FEATURE_VIDLE:
		mi_cfg->feature_val[DISP_FEATURE_VIDLE] = ctl_temp->feature_val;
		if (ctl_temp->feature_val == FEATURE_ON) {
			mtk_vidle_hint_update(VIDLE_HINT_CLOUD_CONTROL_ON);
		} else {
			mtk_vidle_hint_update(VIDLE_HINT_CLOUD_CONTROL_OFF);
		}
		break;
#endif
	case DISP_FEATURE_BACKLIGHT:
		break;
	default:
		DISP_ERROR("invalid feature argument: %d\n", ctl_temp->feature_id);
		break;
	}

	kfree(ctl_temp);
	mutex_unlock(&dsi->dsi_lock);
	return rc;
}

ssize_t mi_dsi_panel_get_disp_param(struct mtk_dsi *dsi,
			char *buf, size_t size)
{
	struct mi_dsi_panel_cfg *mi_cfg;
	ssize_t count = 0;
	int i = 0;

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EAGAIN;
	}

	mi_cfg = &dsi->mi_cfg;

	count = snprintf(buf, size, "%s: feature value\n", "feature name[feature id]");

	for (i = DISP_FEATURE_DIMMING; i < DISP_FEATURE_MAX; i++) {
		count += snprintf(buf + count, size - count, "%s[%02d]: %d\n",
				     get_disp_feature_id_name(i), i, mi_cfg->feature_val[i]);
	}

	return count;
}

int mi_dsi_panel_set_fp_unlock_state(struct mtk_dsi *dsi,
			u32 fp_unlock_value)
{
	int rc = 0;
	struct mtk_ddp_comp *comp = NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	pr_info("fp_unlock_value:%d \n", fp_unlock_value);
	if (!dsi) {
		pr_info("%s invalid params\n", __func__);
		return -EFAULT;
	}

	comp = &dsi->ddp_comp;
	if (!comp) {
		pr_info("%s comp nullpt \nr", __func__);
		return -EFAULT;
	}

#ifdef CONFIG_MI_DISP_FP_STATE
	if (fp_unlock_value == FINGERPRINT_UNLOCK_SUCCESS) {
		panel_ext = mtk_dsi_get_panel_ext(comp);
		if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->fp_state_restore_backlight)) {
			pr_info("%s fp_state_restore_backlight func not defined\n", __func__);
			return -EFAULT;
		} else {
			panel_ext->funcs->fp_state_restore_backlight(dsi);
		}
	}
#endif
	return rc;
}

int mi_dsi_panel_set_count_info(struct mtk_dsi *dsi, struct disp_count_info *count_info)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;
	int rc = 0;

	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	if (!dsi || !count_info) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	DISP_UTC_INFO("panel count info type: %s, value: %d\n",
		get_disp_count_info_type_name(count_info->count_info_type), count_info->count_info_val);

	switch (count_info->count_info_type) {
	case DISP_COUNT_INFO_POWERSTATUS:
		DISP_INFO("power get:%d\n", count_info->count_info_val);
		dsi->mi_cfg.count_info_val[DISP_COUNT_INFO_POWERSTATUS] = count_info->count_info_val;

		break;
	case DISP_COUNT_INFO_SYSTEM_BUILD_VERSION:
		DISP_INFO("system build version:%s\n", count_info->tx_ptr);

		break;
	case DISP_COUNT_INFO_FRAME_DROP_COUNT:
		DISP_INFO("frame drop count:%d\n", count_info->count_info_val);
		break;
	case DISP_COUNT_INFO_SWITCH_KERNEL_FUNCTION_TIMER:
		DISP_INFO("swith function timer:%d\n", count_info->count_info_val);
		break;
	default:
		break;
	}

	return rc;
}

void mi_dsi_panel_set_cur_color_mode(int curColorMode)
{
	sCurColorMode = curColorMode;
}

int mi_dsi_panel_get_cur_color_mode(void)
{
	return sCurColorMode;
}

void mi_dsi_panel_set_cur_dolby_status(int curDolbyStatus)
{
	sDolbyStatus = curDolbyStatus;
}

int mi_dsi_panel_get_cur_dolby_status(void)
{
	return sDolbyStatus;
}

int mi_dsi_panel_set_repaint_status(struct mtk_drm_helper *helper_opt, int value)
{
	int ret = 0;
	ret = mtk_drm_helper_set_opt_by_name(helper_opt, "MTK_DRM_OPT_IDLEMGR_BY_REPAINT", value);
	return ret;
}

void mi_dsi_panel_mi_cfg_state_update(struct mtk_dsi *dsi, int power_state)
{
	struct mi_dsi_panel_cfg *mi_cfg;

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return;
	}

	mi_cfg = &dsi->mi_cfg;

	DISP_INFO("power state:%d\n", power_state);
	switch (power_state) {
	case MI_DISP_POWER_OFF:
		mi_cfg->feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] = DOZE_TO_NORMAL;
		mi_cfg->feature_val[DISP_FEATURE_HBM] = FEATURE_OFF;
		mi_cfg->feature_val[DISP_FEATURE_DC] = FEATURE_OFF;
		mi_cfg->feature_val[DISP_FEATURE_DIMMING] = FEATURE_OFF;
		break;
	case MI_DISP_POWER_ON:
	case MI_DISP_POWER_LP1:
	case MI_DISP_POWER_LP2:
	default:
		break;
	}

	return;
}

bool is_aod_and_panel_initialized(struct mtk_dsi *dsi)
{
	if (!dsi) {
		pr_err("%s NULL dsi\n", __func__);
		return false;
	}

	if (dsi->output_en && dsi->doze_enabled)
		return true;
	return false;
}

#ifdef CONFIG_VIS_DISPLAY_DALI
static void mi_extmv_info_send(struct mtk_dsi *dsi)
{
	struct mtk_ddic_dsi_msg *cmd_msg;
	u32 i = 0;
	u32 meta_size = 0;
	u32 mv_size = 0;
	u32 send_size = 0;
	u32 *meta_send_buffer = NULL;
	int ret = 0;
	bool test_pattern = false;
	u32 *meta_content = NULL;
	u32 meta_length = 0;
	u32 meta_send_buffer_index;
	u32 send_package_size = 256;
	u32 send_payload_size = 63;

	if (dsi->ext_mv_enable == 0) {
		return;
	}

	send_payload_size = (send_package_size - 4) / 4;

	mutex_lock(&dsi->extmv_lock);
	meta_content = dsi->meta_content;
	meta_length = dsi->meta_length;
	dsi->meta_content = NULL;
	mutex_unlock(&dsi->extmv_lock);
	if (meta_content == NULL) {
		return;
	}

	meta_size = meta_content[0] & 0xFFFF;
	test_pattern = !!(meta_content[0] & 0xF0000);
	if (meta_size >= 40000 && test_pattern == false) {
		kfree(meta_content);
		return;
	}

	meta_send_buffer = (u32 *)kzalloc(MAX_TX_CMD_NUM * send_package_size * sizeof(unsigned char), GFP_KERNEL);
	if (meta_send_buffer == NULL) {
		kfree(meta_content);
		pr_err("[EXTMV][%s(%d)] It is failed to allocate buffer\n", __func__, __LINE__);
		return;
	}

	cmd_msg = vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	if (!cmd_msg) {
		kfree(meta_content);
		kfree(meta_send_buffer);
		pr_err("[EXTMV][%s(%d)] It is failed to alloc memory for cmd_msg", __FUNCTION__, __LINE__);
		return;
	}

	//Metadata + Timestamp
	meta_send_buffer[0] = 0x0854564E;	//NVT
	meta_send_buffer[1] = 0x4154454D;	//META
	meta_send_buffer[2] = (meta_content[4] & 0x0000FFFF) | (meta_content[3] << 16);
	meta_send_buffer[3] = 0x10000801;	//ICMetaTimestamp, payload size = 8 bytes
	meta_send_buffer[4] = (meta_content[8] & 0x0000FFFF) | (meta_content[7] << 16);
	meta_send_buffer[5] = (meta_content[10] & 0x0000FFFF) | (meta_content[9] << 16);
	//MetaMV
	meta_send_buffer[6] = (meta_content[12] & 0x0000FFFF) | (meta_content[11] << 16);

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = meta_send_buffer;
	cmd_msg->tx_len[0] = 28;

	mv_size = meta_content[13];
	if (mv_size > (meta_length - 14)) {
		kfree(meta_content);
		kfree(meta_send_buffer);
		vfree(cmd_msg);
		pr_err("[EXTMV]%s wrong mv_size(%u) meta_length(%u)\n", __func__, mv_size, meta_length);
		return;
	}

	i = 0;
	meta_send_buffer_index = 1;
	while (mv_size  > 0) {
		*(meta_send_buffer + (meta_send_buffer_index * (send_payload_size + 1))) = 0x0954564E;	//NVT
		if(mv_size > send_payload_size) {
			send_size = send_payload_size;
		} else {
			send_size = mv_size;
		}

		memcpy(meta_send_buffer + (meta_send_buffer_index * (send_payload_size + 1) + 1), meta_content + 14 + (i * send_payload_size), send_payload_size * sizeof(u32));

		cmd_msg->type[meta_send_buffer_index] = 0x39;
		cmd_msg->tx_buf[meta_send_buffer_index] = (meta_send_buffer + (meta_send_buffer_index * (send_payload_size + 1)));
		cmd_msg->tx_len[meta_send_buffer_index] = (send_size * 4) + 4;

		mv_size -= send_size;
		i++;
		meta_send_buffer_index++;

		if (meta_send_buffer_index >= MAX_TX_CMD_NUM) {
			cmd_msg->channel = 1;
			cmd_msg->flags = 0;
			cmd_msg->tx_cmd_num = MAX_TX_CMD_NUM;

			ret = mtk_ddic_dsi_send_cmd(cmd_msg, true);
			if (ret != 0) {
				kfree(meta_content);
				kfree(meta_send_buffer);
				vfree(cmd_msg);
				pr_err("[EXTMV][%s(%d)] It is failed to send meta data ret(%d)\n", __func__, __LINE__, ret);
				return;
			}

			meta_send_buffer_index = 0;
			memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));
		}
	}

	//MetaEnd
	mv_size = meta_content[13];
	*(meta_send_buffer + (meta_send_buffer_index * (send_payload_size + 1))) = 0x0954564E;	//NVT
	*(meta_send_buffer + (meta_send_buffer_index * (send_payload_size + 1)) + 1) = (meta_content[mv_size + 15] & 0x0000FFFF) | (meta_content[mv_size + 14] << 16);
	*(meta_send_buffer + (meta_send_buffer_index * (send_payload_size + 1)) + 2) = (meta_content[mv_size + 17] & 0x0000FFFF) | (meta_content[mv_size + 16] << 16);

	cmd_msg->channel = 1;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = meta_send_buffer_index + 1;
	cmd_msg->type[meta_send_buffer_index] = 0x39;
	cmd_msg->tx_buf[meta_send_buffer_index] = (meta_send_buffer + (meta_send_buffer_index * (send_payload_size + 1)));
	cmd_msg->tx_len[meta_send_buffer_index] = 12;

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true);
	if (ret != 0) {
		vfree(cmd_msg);
		kfree(meta_send_buffer);
		kfree(meta_content);
		pr_err("[EXTMV]%s It is failed to send meta end ret(%d)\n", __func__, ret);
		return;
	}

	vfree(cmd_msg);
	kfree(meta_send_buffer);
	kfree(meta_content);
}

static int mi_extmv_monitor_thread(void *data)
{
	struct mtk_dsi *dsi = (struct mtk_dsi *)data;
	while (1) {
			wait_for_completion(&dsi->ext_mv_completion);
			mi_extmv_info_send(dsi);
			reinit_completion(&dsi->ext_mv_completion);
	}

	return 0;
}

#define EXTMV_TASK_NAME_LEN 50
static int mi_extmv_init(struct mtk_dsi *dsi)
{
	static struct task_struct *extmv_task;
	char task_name[EXTMV_TASK_NAME_LEN] = {0};

	if (!dsi) {
		pr_err("%s Invalid dsi\n", __func__);
		return -1;
	}

	mutex_init(&dsi->extmv_lock);
	dsi->meta_content = NULL;
	dsi->meta_length = 0;

	snprintf(task_name, EXTMV_TASK_NAME_LEN, "EXTMV_TASK");
	extmv_task = kthread_create(mi_extmv_monitor_thread, dsi, task_name);
	wake_up_process(extmv_task);

	return 0;
}
#endif

void mi_disp_cfg_init(struct mtk_dsi *dsi)
{
	int ret = 0;
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if (!dsi) {
		pr_err("%s NULL dsi\n", __func__);
		return;
	}
	mutex_init(&dsi->dsi_lock);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	if (!(panel_ext && panel_ext->funcs)) {
		pr_info("panel_ext func not defined\n");
		return;
	}

	mi_cfg = &dsi->mi_cfg;

	mi_cfg->feature_val[DISP_FEATURE_COLORMODE_NOTIFY] = 0;
	mi_cfg->feature_val[DISP_FEATURE_DOLBY_STATUS] = 0;

#ifdef CONFIG_MI_DISP_FOD_SYNC
	mi_cfg->bl_enable = true;
	mi_cfg->bl_wait_frame = false;
	mi_cfg->aod_wait_frame = true;
	init_completion(&dsi->bl_wait_completion);
	init_completion(&dsi->aod_wait_completion);
#endif
	mi_cfg->is_tddi_flag = false;
	mi_cfg->panel_dead_flag = false;
	mi_cfg->tddi_gesture_flag = false;

	ret = mi_dsi_panel_get_ic_type(dsi, &dsi->mi_cfg.is_tddi_flag);
	if (!ret) {
		DISP_INFO("mi_dsi_panel_get_ic_type:%d\n", dsi->mi_cfg.is_tddi_flag);
	} else {
		dsi->mi_cfg.is_tddi_flag = false;
		DISP_INFO("mi_dsi_panel_get_ic_type:%d\n", dsi->mi_cfg.is_tddi_flag);
	}
	mi_cfg->mi_display_gesture_cb = NULL;

	ret = mi_dsi_panel_get_max_brightness_clone(dsi, &dsi->mi_cfg.max_brightness_clone);
	if (!ret) {
		dsi->mi_cfg.thermal_max_brightness_clone = dsi->mi_cfg.max_brightness_clone;
		DISP_INFO("max brightness clone:%d\n", dsi->mi_cfg.max_brightness_clone);
	} else {
		dsi->mi_cfg.max_brightness_clone = DEFAULT_MAX_BRIGHTNESS_CLONE;
		dsi->mi_cfg.thermal_max_brightness_clone = DEFAULT_MAX_BRIGHTNESS_CLONE;
		DISP_INFO("default max brightness clone:%d\n", dsi->mi_cfg.max_brightness_clone);
	}
	dsi->mi_cfg.real_brightness_clone = -1;

	ret = mi_dsi_panel_get_factory_max_brightness(dsi, &dsi->mi_cfg.factory_max_brightness);
	if (!ret) {
		DISP_INFO("factory_max_brightness:%d\n", dsi->mi_cfg.factory_max_brightness);
	} else {
		dsi->mi_cfg.factory_max_brightness = 2047;
		DISP_INFO("default factory_max_brightness:%d\n", dsi->mi_cfg.factory_max_brightness);
	}

#ifdef CONFIG_VIS_DISPLAY_DALI
	if (is_mi_dev_support_nova())
	{
		dsi->ext_mv_enable = 33710;
		init_completion(&dsi->ext_mv_completion);
		if (mi_extmv_init(dsi) == 0) {
			printk("[EXTMV] It is success to init extmv\n");
		} else {
			printk("[EXTMV] Failed to init extmv\n");
		}
	}
#endif

	return;
}

module_param_string(panel_opt, lockdown_info, MAX_CMDLINE_PARAM_LEN, 0600);
MODULE_PARM_DESC(panel_opt, "mediatek_drm.panel_opt=<panel_opt> while <panel_opt> is 'panel_opt' ");

