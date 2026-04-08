/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 XiaoMi, Inc.
 * All Rights Reserved.
 */

#ifndef __TUI_COMMON_H__
#define __TUI_COMMON_H__

enum tpd_vendor_id_t {
	INVALID_VENDOR_ID = 0,
	ST_VENDOR_ID,
	SYNA_VENDOR_ID,
	GOODIX_VENDOR_ID,
	FTS_VENDOR_ID
};

void register_tpd_tui_request(int (*enter_func)(void), int (*exit_func)(void), enum tpd_vendor_id_t (*get_vendor_id_func)(void));

#endif //  __TUI_SHM_H__
