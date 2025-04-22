/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_MDW_CB_APPENDIX_H__
#define __MTK_MDW_CB_APPENDIX_H__

uint32_t mdw_cb_appendix_num(void);
uint32_t mdw_cb_appendix_get_owner(uint32_t idx);
uint32_t mdw_cb_appendix_size_by_idx(uint32_t idx, uint32_t num_subcmds);
int mdw_cb_appendix_process(int process_type, uint32_t idx, struct apusys_cmd_info *cmd_info,
	void *va, uint32_t size);
int mdw_cb_appendix_init(void);
void mdw_cb_appendix_deinit(void);

#endif