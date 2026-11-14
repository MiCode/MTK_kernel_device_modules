// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mdw_sanity.h"
#include "mdw.h"


/* check cmd exec infos */
int mdw_sanity_einfo_check(struct mdw_cmd *c)
{
	if (c->exec_infos->size != sizeof(struct mdw_cmd_exec_info) +
		c->num_subcmds * sizeof(struct mdw_subcmd_exec_info)) {
		mdw_drv_err("s(0x%llx)cmd invalid(0x%llx/0x%llx) einfo(%llu/%lu)\n",
			(uint64_t)c->mpriv, c->uid, c->kid,
			c->exec_infos->size,
			sizeof(struct mdw_cmd_exec_info) +
			c->num_subcmds * sizeof(struct mdw_subcmd_exec_info));
		return -EINVAL;
	}
	return 0;
}

/* check cmd adj matrix */
int mdw_sanity_adj_check(struct mdw_cmd *c)
{
	uint32_t i = 0, j = 0;

    /* check execute_order exist */
	if (c->adj_matrix == NULL) {
		mdw_drv_err("s(0x%llx)cmd(0x%llx/0x%llx) miss adj matrix(%pK)\n",
			(uint64_t)c->mpriv, c->uid, c->kid, c->adj_matrix);
		return -EINVAL;
	}

	for (i = 0; i < c->num_subcmds; i++) {
		for (j = 0; j < c->num_subcmds; j++) {
			if (i == j) {
				c->adj_matrix[i * c->num_subcmds + j] = 0;
				continue;
			}

			if (i < j)
				continue;

			if (!c->adj_matrix[i * c->num_subcmds + j] ||
				!c->adj_matrix[i + j * c->num_subcmds])
				continue;

			mdw_drv_err("s(0x%llx)c(0x%llx/0x%llx) adj matrix(%u/%u) fail\n",
				(uint64_t)c->mpriv, c->uid, c->kid, i, j);
			return -EINVAL;
		}
	}
	return 0;
}

/* check cmd links */
int mdw_sanity_link_check(struct mdw_cmd *c)
{
	uint32_t i = 0;

	for (i = 0; i < c->num_links; i++) {
		if (c->links[i].producer_idx > c->num_subcmds ||
			c->links[i].consumer_idx > c->num_subcmds ||
			!c->links[i].x || !c->links[i].y ||
			!c->links[i].va) {
			mdw_drv_err("link(%u) invalid(%u/%u)(%llu/%llu)(0x%llx)\n", i,
				c->links[i].producer_idx,
				c->links[i].consumer_idx,
				c->links[i].x,
				c->links[i].y,
				c->links[i].va);
			return -EINVAL;
		}
	}
	return 0;
}

/* check cmd execute order */
int mdw_sanity_order_check(struct mdw_cmd *c)
{
	uint32_t i = 0, *val = c->execute_orders;

	/* check execute_order exist */
	if (c->execute_orders == NULL) {
		mdw_drv_err("s(0x%llx)cmd(0x%llx/0x%llx) miss execution order(%pK)\n",
			(uint64_t)c->mpriv, c->uid, c->kid, c->execute_orders);
		return -EINVAL;
	}

	for (i = 0; i < c->num_subcmds; i++) {
		if (*val > c->num_subcmds) {
			mdw_drv_err("execut order(%u) is invalid(%u)\n", i, *val);
			return -EINVAL;
		}
		val++;
	}
	return 0;
}
