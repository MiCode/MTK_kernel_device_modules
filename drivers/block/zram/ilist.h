/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2024, X-Ring technologies Inc., All rights reserved.
 */
#ifndef __ILIST_H
#define __ILIST_H

#include <linux/types.h>

#define ILIST_IDX_SHIFT		30
#define ILIST_IDX_MAX		((1UL << ILIST_IDX_SHIFT) - 1)
#define ILIST_LOCK_BIT		ILIST_IDX_SHIFT
#define ILIST_PRIV_BIT		((ILIST_IDX_SHIFT << 1) + 1)

struct ilist_node {
	u64 prev	: ILIST_IDX_SHIFT;
	u64 lock	: 1;
	u64 next	: ILIST_IDX_SHIFT;
	u64 priv	: 1;
};

typedef struct ilist_node *(*ilist_get_node_func)(u32 idx, void *priv);

struct ilist_table {
	ilist_get_node_func get_node;
	void *priv;
};

#define ilist_get_node(idx, tab)	\
	tab->get_node(idx, tab->priv)

#define ilist_prev_idx(idx, tab)	\
	ilist_get_node(idx, tab)->prev

#define ilist_next_idx(idx, tab)	\
	ilist_get_node(idx, tab)->next

#define ilist_for_each_idx(pos, head, tab)	\
	for (pos = ilist_next_idx(head, tab);	\
	     pos != head;			\
	     pos = ilist_next_idx(pos, tab))

#define ilist_for_each_idx_reverse(pos, head, tab)	\
	for (pos = ilist_prev_idx(head, tab);		\
	     pos != head;				\
	     pos = ilist_prev_idx(pos, tab))

#define ilist_for_each_idx_safe(pos, n, head, tab)	\
	for (pos = ilist_next_idx(head, tab),		\
	     n = ilist_next_idx(pos, tab);		\
	     pos != head;				\
	     pos = n, n = ilist_next_idx(n, tab))

void ilist_lock(u32 idx, struct ilist_table *tab);
void ilist_unlock(u32 idx, struct ilist_table *tab);
void ilist_add_nolock(u32 hidx, u32 idx, struct ilist_table *tab);
void ilist_del_nolock(u32 hidx, u32 idx, struct ilist_table *tab);
void ilist_add(u32 hidx, u32 idx, struct ilist_table *tab);
void ilist_del(u32 hidx, u32 idx, struct ilist_table *tab);
bool ilist_is_isolated(u32 idx, struct ilist_table *tab);
void ilist_set_priv(u32 idx, struct ilist_table *tab);
void ilist_clear_priv(u32 idx, struct ilist_table *tab);
void ilist_clear_priv_nolock(u32 idx, struct ilist_table *tab);
bool ilist_test_priv_nolock(u32 idx, struct ilist_table *tab);
void ilist_node_init(u32 idx, struct ilist_table *tab);
struct ilist_table *ilist_table_alloc(ilist_get_node_func get_node,
				      void *priv);
void ilist_table_free(struct ilist_table *tab);
#endif
