/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __SCP_AUDIO_LOGGER_H__
#define __SCP_AUDIO_LOGGER_H__

struct log_ctrl_s {
	unsigned int base;
	unsigned int size;
	unsigned int enable;
	unsigned int info_ofs;
	unsigned int buff_ofs;
	unsigned int buff_size;
};

struct buffer_info_s {
	unsigned int r_pos;
	unsigned char resv1[124]; /* dummy bytes for 128-byte align */
	unsigned int w_pos;
	unsigned char resv2[124]; /* dummy bytes for 128-byte align */
};

extern struct device_attribute dev_attr_log_enable;
int scp_audio_logger_init(struct platform_device *pdev);
int scp_audio_logger_init_message(void);
void reset_adsp_logger_status(void);

#endif /* __SCP_AUDIO_LOGGER_H__ */
