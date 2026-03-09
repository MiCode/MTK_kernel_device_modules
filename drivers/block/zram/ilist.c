// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright(c) 2024 X-Ring technologies Inc.
 */

#include <linux/processor.h>
#include <linux/bit_spinlock.h>
#include <linux/slab.h>

#include "ilist.h"

/* TODO: more debug methods */
#ifdef CONFIG_XRING_ZRAM_MEMCG_DEBUG
static inline void ilist_add_valid(u32 new, u32 prev, u32 next,
				   struct ilist_table *tab)
{
	if (CHECK_DATA_CORRUPTION(ilist_get_node(prev, tab)->next != next,
		"ilist_add corruption. prev->next should be next (%u), but was %u. (prev=%u).\n",
		next, ilist_get_node(prev, tab)->next, prev) ||
	    CHECK_DATA_CORRUPTION(ilist_get_node(next, tab)->prev != prev,
		"ilist_add corruption. next->prev should be prev (%u), but was %u. (next=%u).\n",
		prev, ilist_get_node(next, tab)->prev, next) ||
	    CHECK_DATA_CORRUPTION(ilist_get_node(new, tab)->prev != new,
		"ilist_add corruption. new->prev should be new (%u), but was %u. (new=%u).\n",
		new, ilist_get_node(next, tab)->prev, new) ||
	    CHECK_DATA_CORRUPTION(ilist_get_node(new, tab)->next != new,
		"ilist_add corruption. new->next should be new (%u), but was %u. (new=%u).\n",
		new, ilist_get_node(next, tab)->next, new) ||
	    CHECK_DATA_CORRUPTION(new == prev || new == next,
		"ilist_add double add: new=%u, prev=%u, next=%u.\n",
		new, prev, next))
		return;
}

static inline void ilist_add_check(u32 new, u32 prev, u32 next,
				   struct ilist_table *tab)
{
	if (CHECK_DATA_CORRUPTION(ilist_get_node(prev, tab)->next != new,
		"ilist_add corruption. prev->next should be new (%u), but was %u. (prev=%u).\n",
		new, ilist_get_node(prev, tab)->next, prev) ||
	    CHECK_DATA_CORRUPTION(ilist_get_node(next, tab)->prev != new,
		"ilist_add corruption. next->prev should be new (%u), but was %u. (next=%u).\n",
		new, ilist_get_node(next, tab)->prev, next) ||
	    CHECK_DATA_CORRUPTION(ilist_get_node(new, tab)->prev != prev,
		"ilist_add corruption. new->prev should be prev (%u), but was %u. (new=%u).\n",
		prev, ilist_get_node(next, tab)->prev, new) ||
	    CHECK_DATA_CORRUPTION(ilist_get_node(new, tab)->next != next,
		"ilist_add corruption. new->next should be next (%u), but was %u. (new=%u).\n",
		next, ilist_get_node(next, tab)->next, new))
		return;
}

static inline void ilist_del_valid(u32 cur, u32 prev, u32 next,
				   struct ilist_table *tab)
{
	if (CHECK_DATA_CORRUPTION(ilist_get_node(prev, tab)->next != cur,
		"ilist_del corruption. prev->next should be cur (%u), but was %u. (prev=%u)\n",
		cur, ilist_get_node(prev, tab)->next, prev) ||
	    CHECK_DATA_CORRUPTION(ilist_get_node(next, tab)->prev != cur,
		"ilist_del corruption. next->prev should be cur (%u), but was %u. (next=%u)\n",
		cur, ilist_get_node(next, tab)->prev, next) ||
	    CHECK_DATA_CORRUPTION(ilist_get_node(cur, tab)->prev != prev,
		"ilist_del corruption. cur->prev should be prev (%u), but was %u. (cur=%u).\n",
		prev, ilist_get_node(cur, tab)->prev, cur) ||
	    CHECK_DATA_CORRUPTION(ilist_get_node(cur, tab)->next != next,
		"ilist_del corruption. cur->next should be next (%u), but was %u. (cur=%u).\n",
		next, ilist_get_node(cur, tab)->next, next) ||
	    CHECK_DATA_CORRUPTION(cur == prev || cur == next,
		"ilist_del double del: cur=%u, prev=%u, next=%u.\n",
		cur, prev, next))
		return;
}

static inline void ilist_del_check(u32 cur, u32 prev, u32 next,
				   struct ilist_table *tab)
{
	if (CHECK_DATA_CORRUPTION(ilist_get_node(prev, tab)->next != next,
		"ilist_del corruption. prev->next should be next (%u), but was %u. (prev=%u).\n",
		next, ilist_get_node(prev, tab)->next, prev) ||
	    CHECK_DATA_CORRUPTION(ilist_get_node(next, tab)->prev != prev,
		"ilist_del corruption. next->prev should be prev (%u), but was %u. (next=%u).\n",
		prev, ilist_get_node(next, tab)->prev, next) ||
	    CHECK_DATA_CORRUPTION(ilist_get_node(cur, tab)->prev != cur,
		"ilist_del corruption. cur->prev should be cur (%u), but was %u. (cur=%u).\n",
		cur, ilist_get_node(next, tab)->prev, cur) ||
	    CHECK_DATA_CORRUPTION(ilist_get_node(cur, tab)->next != cur,
		"ilist_del corruption. cur->next should be cur (%u), but was %u. (cur=%u).\n",
		cur, ilist_get_node(next, tab)->next, cur))
		return;
}
#else
static inline void ilist_add_valid(u32 new, u32 prev, u32 next,
				   struct ilist_table *tab) {};
static inline void ilist_add_check(u32 new, u32 prev, u32 next,
				   struct ilist_table *tab) {};
static inline void ilist_del_valid(u32 cur, u32 prev, u32 next,
				   struct ilist_table *tab) {};
static inline void ilist_del_check(u32 cur, u32 prev, u32 next,
				   struct ilist_table *tab) {};
#endif

static inline void ilist_node_lock(struct ilist_node *node)
{
	bit_spin_lock(ILIST_LOCK_BIT, (unsigned long *)node);
}

static inline void ilist_node_unlock(struct ilist_node *node)
{
	bit_spin_unlock(ILIST_LOCK_BIT, (unsigned long *)node);
}

void ilist_lock(u32 idx, struct ilist_table *tab)
{
	ilist_node_lock(ilist_get_node(idx, tab));
}

void ilist_unlock(u32 idx, struct ilist_table *tab)
{
	ilist_node_unlock(ilist_get_node(idx, tab));
}

/* must call with hidx lock */
void ilist_add_nolock(u32 hidx, u32 idx, struct ilist_table *tab)
{
	struct ilist_node *node, *hnode, *nnode;
	u32 nidx;

	node = ilist_get_node(idx, tab);
	hnode = ilist_get_node(hidx, tab);

	nidx = hnode->next;
	nnode = ilist_get_node(nidx, tab);

	ilist_add_valid(idx, hidx, nidx, tab);

	/* TODO: check overhead */
	ilist_node_lock(node);
	node->prev = hidx;
	node->next = nidx;
	ilist_node_unlock(node);

	if (nidx != hidx)
		ilist_node_lock(nnode);
	nnode->prev = idx;
	if (nidx != hidx)
		ilist_node_unlock(nnode);

	hnode->next = idx;

	ilist_add_check(idx, hidx, nidx, tab);
}

/* must call with hidx lock */
void ilist_del_nolock(u32 hidx, u32 idx, struct ilist_table *tab)
{
	struct ilist_node *node, *pnode, *nnode;
	u32 pidx, nidx;

	node = ilist_get_node(idx, tab);
	pidx = node->prev;
	nidx = node->next;
	pnode = ilist_get_node(pidx, tab);
	nnode = ilist_get_node(nidx, tab);

	ilist_del_valid(idx, pidx, nidx, tab);

	ilist_node_lock(node);
	node->prev = idx;
	node->next = idx;
	ilist_node_unlock(node);

	if (pidx != hidx)
		ilist_node_lock(pnode);
	pnode->next = nidx;
	if (pidx != hidx)
		ilist_node_unlock(pnode);

	if (nidx != hidx)
		ilist_node_lock(nnode);
	nnode->prev = pidx;
	if (nidx != hidx)
		ilist_node_unlock(nnode);

	ilist_del_check(idx, pidx, nidx, tab);
}

void ilist_add(u32 hidx, u32 idx, struct ilist_table *tab)
{
	struct ilist_node *hnode;

	hnode = ilist_get_node(hidx, tab);

	ilist_node_lock(hnode);
	ilist_add_nolock(hidx, idx, tab);
	ilist_node_unlock(hnode);
}

void ilist_del(u32 hidx, u32 idx, struct ilist_table *tab)
{
	struct ilist_node *hnode;

	hnode = ilist_get_node(hidx, tab);

	ilist_node_lock(hnode);
	ilist_del_nolock(hidx, idx, tab);
	ilist_node_unlock(hnode);
}

bool ilist_is_isolated(u32 idx, struct ilist_table *tab)
{
	struct ilist_node *node;
	bool ret;

	node = tab->get_node(idx, tab->priv);

	ilist_node_lock(node);
	ret = node->next == idx && node->prev == idx;
	ilist_node_unlock(node);

	return ret;
}

void ilist_set_priv(u32 idx, struct ilist_table *tab)
{
	struct ilist_node *node;

	node = tab->get_node(idx, tab->priv);

	ilist_node_lock(node);
	set_bit(ILIST_PRIV_BIT, (unsigned long *)node);
	ilist_node_unlock(node);
}

void ilist_clear_priv(u32 idx, struct ilist_table *tab)
{
	struct ilist_node *node;

	node = tab->get_node(idx, tab->priv);

	ilist_node_lock(node);
	clear_bit(ILIST_PRIV_BIT, (unsigned long *)node);
	ilist_node_unlock(node);
}

void ilist_clear_priv_nolock(u32 idx, struct ilist_table *tab)
{
	struct ilist_node *node;

	node = tab->get_node(idx, tab->priv);

	clear_bit(ILIST_PRIV_BIT, (unsigned long *)node);
}

bool ilist_test_priv_nolock(u32 idx, struct ilist_table *tab)
{
	struct ilist_node *node;

	node = tab->get_node(idx, tab->priv);

	return test_bit(ILIST_PRIV_BIT, (unsigned long *)node);
}

void ilist_node_init(u32 idx, struct ilist_table *tab)
{
	struct ilist_node *node;

	node = tab->get_node(idx, tab->priv);

	memset(node, 0, sizeof(struct ilist_node));
	node->prev = idx;
	node->next = idx;
}

struct ilist_table *ilist_table_alloc(ilist_get_node_func get_node,
				      void *priv)
{
	struct ilist_table *tab;

	tab = kzalloc(sizeof(struct ilist_table), GFP_KERNEL);
	if (!tab)
		return NULL;

	tab->get_node = get_node;
	tab->priv = priv;

	return tab;
}

void ilist_table_free(struct ilist_table *tab)
{
	kfree(tab);
}
