/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_GOV_SYSFS__
#define __MTK_GOV_SYSFS__

#include "mtk_lp_sysfs.h"
#include "mtk_lp_kernfs.h"

#define MTK_GOV_SYS_FS_MODE      0644

#define MTK_GOV_NODE_INIT(_n, _r, _w) ({\
		_n.op.fs_read = _r;\
		_n.op.fs_write = _w;\
		_n.op.priv = &_n; })

#undef mtk_dbg_gov_log
#define mtk_dbg_gov_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)

struct MTK_GOV_NODE {
	const char *name;
	int type;
	struct mtk_lp_sysfs_handle handle;
	struct mtk_lp_sysfs_op op;
};

/*Get the mtk gov fs root entry handle*/
int mtk_gov_sysfs_entry_root_get(struct mtk_lp_sysfs_handle **handle);

/*Creat the entry for mtk gov system fs*/
int mtk_gov_sysfs_entry_group_add(const char *name,
		int mode, struct mtk_lp_sysfs_group *_group,
		struct mtk_lp_sysfs_handle *handle);

/*Add the child file node to mtk idle system*/
int mtk_gov_sysfs_entry_node_add(const char *name, int mode,
			const struct mtk_lp_sysfs_op *op,
			struct mtk_lp_sysfs_handle *node);

int mtk_gov_sysfs_entry_node_remove(
		struct mtk_lp_sysfs_handle *handle);

int mtk_gov_sysfs_root_entry_create(void);

int mtk_gov_sysfs_remove(void);

int mtk_gov_sysfs_sub_entry_add(const char *name, int mode,
					struct mtk_lp_sysfs_handle *parent,
					struct mtk_lp_sysfs_handle *handle);

int mtk_gov_sysfs_sub_entry_node_add(const char *name
		, int mode, const struct mtk_lp_sysfs_op *op
		, struct mtk_lp_sysfs_handle *parent
		, struct mtk_lp_sysfs_handle *handle);

#endif
