/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_SWPM_COMMON_SYSFS__
#define __MTK_SWPM_COMMON_SYSFS__
#include <linux/list.h>

#define MTK_SWPM_SYSFS_HAS_ENTRY		(1)

#define MTK_SWPM_SYSFS_ENTRY_NAME		"swpm"
#define MTK_SWPM_SYSFS_BUF_READSZ		8192
#define MTK_SWPM_SYSFS_BUF_WRITESZ	512

typedef ssize_t (*f_mtk_swpm_sysfs_show)(char *ToUserBuf
			, size_t sz, void *priv);
typedef ssize_t (*f_mtk_swpm_sysfs_write)(char *FromUserBuf
			, size_t sz, void *priv);


#define MTK_SWPM_SYSFS_TYPE_ENTRY		(1 << 0ul)
#define MTK_SWPM_SYSFS_BUF_SZ		(1 << 1ul)
#define MTK_SWPM_SYSFS_FREEZABLE		(1 << 2ul)

struct mtk_swpm_sysfs_op {
	unsigned int buf_sz;
	f_mtk_swpm_sysfs_show	fs_read;
	f_mtk_swpm_sysfs_write	fs_write;
	void *priv;
};

struct mtk_swpm_sysfs_handle {
	const char *name;
	unsigned int flag;
	void *_current;
	struct list_head dr;
	struct list_head np;
};

struct mtk_swpm_sysfs_attr {
	char	*name;
	umode_t	mode;
	struct mtk_swpm_sysfs_op sysfs_op;
};
struct mtk_swpm_sysfs_group {
	struct mtk_swpm_sysfs_attr **attrs;
	unsigned int attr_num;
};


#define __MTK_SWPM_SYSFS_ATTR(_name, _mode, _read, _write, _priv) {\
	.name = __stringify(_name),\
	.mode = _mode,\
	.sysfs_op.fs_read = _read,\
	.sysfs_op.fs_write = _write,\
	.sysfs_op.priv = _priv,\
}

#define DEFINE_MTK_SWPM_SYSFS_ATTR(_name, _mode, _read, _write, _priv)\
	struct mtk_swpm_sysfs_attr mtk_swpm_sysfs_attr_##_name =\
		__MTK_SWPM_SYSFS_ATTR(_name, _mode, _read, _write, _priv)

#define MTK_SWPM_SYSFS_ATTR_PTR(_name)	(&mtk_swpm_sysfs_attr_##_name)


#define IS_MTK_SWPM_SYS_HANDLE_VALID(x)\
	({ struct mtk_swpm_sysfs_handle *Po = x;\
	if ((!Po) || (!Po->_current))\
		Po = NULL;\
	(Po); })

int mtk_swpm_sysfs_entry_func_create(const char *name, int mode,
			struct mtk_swpm_sysfs_handle *parent,
			struct mtk_swpm_sysfs_handle *handle);

int mtk_swpm_sysfs_entry_func_node_add(const char *name,
		int mode, const struct mtk_swpm_sysfs_op *op,
		struct mtk_swpm_sysfs_handle *parent,
		struct mtk_swpm_sysfs_handle *node);

int mtk_swpm_sysfs_entry_func_node_remove(
		struct mtk_swpm_sysfs_handle *node);

int mtk_swpm_sysfs_entry_func_group_create(const char *name,
		int mode, struct mtk_swpm_sysfs_group *_group,
		struct mtk_swpm_sysfs_handle *parent,
		struct mtk_swpm_sysfs_handle *handle);

#endif
