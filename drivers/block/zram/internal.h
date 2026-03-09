/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2024, X-Ring technologies Inc., All rights reserved.
 */
#ifndef __ZRAM_INTERNAL_H
#define __ZRAM_INTERNAL_H

#include <linux/memcontrol.h>

/* Avoid too many header ordering problems. */
struct zram;

#ifdef CONFIG_XRING_ZRAM_MEMCG
int zgroup_track_obj_fault(struct zram *zram, u32 index);
void zgroup_track_obj(struct zram *zram, u32 index, struct mem_cgroup *memcg);
void zgroup_untrack_obj(struct zram *zram, u32 index);
int zgroup_alloc(struct zram *zram, size_t nr_objs);
void zgroup_free(struct zram *zram);
int mctrl_init(void);
void mctrl_exit(void);
#endif

#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
int zgroup_read_obj(struct zram *zram, u32 index);
void zgroup_reset_bdev(struct zram *zram);
long zram_control_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
int zgroup_wb_init(void);
void zgroup_wb_exit(void);
#endif

#ifdef CONFIG_XRING_SMART_CACHE
int smart_cache_init(void);
void smart_cache_exit(void);
#endif

#ifdef CONFIG_XRING_ZRAM_XSWAPD
int xswapd_init(void);
void xswapd_exit(void);
#endif

#endif
