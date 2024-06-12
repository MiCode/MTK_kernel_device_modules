/* SPDX-License-Identifier: GPL-2.0
 *
 * This file is a modification based on the Linux kernel source file:
 * drivers/block/zram/mtk_zcomp.h
 *
 * Original authors of the source file:
 * Copyright (C) 2008, 2009, 2010 Nitin Gupta
 *               2012, 2013 Minchan Kim
 *
 * Modified by:
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef _MTK_ZCOMP_H_
#define _MTK_ZCOMP_H_
#include <linux/local_lock.h>

struct zcomp_strm {
	/* The members ->buffer and ->tfm are protected by ->lock. */
	local_lock_t lock;
	/* compression/decompression buffer */
	void *buffer;
	struct crypto_comp *tfm;
};

/* dynamic per-device compression frontend */
struct zcomp {
	struct zcomp_strm __percpu *stream;
	const char *name;
	struct hlist_node node;
};

int zcomp_cpu_up_prepare(unsigned int cpu, struct hlist_node *node);
int zcomp_cpu_dead(unsigned int cpu, struct hlist_node *node);
ssize_t zcomp_available_show(const char *comp, char *buf);
bool zcomp_available_algorithm(const char *comp);

struct zcomp *zcomp_create(const char *alg);
void zcomp_destroy(struct zcomp *comp);

struct zcomp_strm *zcomp_stream_get(struct zcomp *comp);
void zcomp_stream_put(struct zcomp *comp);

int zcomp_compress(struct zcomp_strm *zstrm,
		const void *src, unsigned int *dst_len);

int zcomp_decompress(struct zcomp_strm *zstrm,
		const void *src, unsigned int src_len, void *dst);

bool zcomp_set_max_streams(struct zcomp *comp, int num_strm);
#endif /* _MTK_ZCOMP_H_ */
