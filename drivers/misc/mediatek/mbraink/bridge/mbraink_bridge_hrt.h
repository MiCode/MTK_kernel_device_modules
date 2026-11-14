/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef MBRAINK_BRIDGE_HRT_H
#define MBRAINK_BRIDGE_HRT_H

struct mbraink2hrt_ops {
	void (*isp_hrt_notify)(int threshold);
};

void mbraink_bridge_hrt_init(void);
void mbraink_bridge_hrt_deinit(void);
int mtk_mbrain2isp_register_ops(struct mbraink2hrt_ops *ops);
int mtk_mbrain2isp_unregister_ops(void);
void mtk_mbrain2isp_hrt_cb(int threshold);
#endif /*MBRAINK_BRIDGE_HRT_H*/
