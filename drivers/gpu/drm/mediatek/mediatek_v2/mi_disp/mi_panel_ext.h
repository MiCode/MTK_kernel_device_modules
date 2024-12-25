/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 Xiaomi Inc.
 */

#ifndef __MI_PANEL_EXT_H__
#define __MI_PANEL_EXT_H__

#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <drm/mediatek_drm.h>
#include "../mtk_panel_ext.h"

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[130];
};

struct LCM_param_read_write {
	unsigned int read_count;
	unsigned char read_buffer[64];
};

struct LCM_mipi_read_write {
	unsigned int read_enable;
	unsigned int read_count;
	unsigned char read_buffer[80];
	struct LCM_setting_table lcm_setting_table;
};

struct LCM_led_i2c_read_write {
	unsigned int read_enable;
	unsigned int read_count;
	unsigned char buffer[64];
};

enum LCM_send_cmd_format {
	FORMAT_WAIT_TE_SEND = BIT(0),
	FORMAT_BLOCK = BIT(1),
	FORMAT_LP_MODE = BIT(2),
};

enum KTZ8863A_REG_RW {
	KTZ8863A_REG_WRITE,
	KTZ8863A_REG_READ,
};

extern struct mi_disp_notifier g_notify_data;
extern struct LCM_mipi_read_write lcm_mipi_read_write;
extern struct LCM_led_i2c_read_write lcm_led_i2c_read_write;

int mtk_ddic_dsi_read_cmd(struct mtk_ddic_dsi_msg *cmd_msg);
int mtk_ddic_dsi_send_cmd(struct mtk_ddic_dsi_msg *cmd_msg, bool blocking);
int mtk_ddic_dsi_wait_te_send_cmd(struct mtk_ddic_dsi_msg *cmd_msg, bool blocking);
/*
 format :
 BIT0: 0 for no wait te, 1 for wait te(only cmd panel)
 BIT1: 0 for no block when send cmd, 1 for block when send cmd
 BIT2: 0 for HS mode, 1 for LP mode
*/
int mi_disp_panel_ddic_send_cmd(struct LCM_setting_table *table, unsigned int count,  unsigned int format);
int mi_dsi_panel_set_doze_brightness(struct mtk_dsi *dsi, int doze_brightness);

#endif
