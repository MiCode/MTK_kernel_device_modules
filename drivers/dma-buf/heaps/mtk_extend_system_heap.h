/* SPDX-License-Identifier: GPL-2.0
 */
#ifndef _MTK_EXTEND_SYSTEM_HEAP_H
#define _MTK_EXTEND_SYSTEM_HEAP_H

#include "mtk_page_pool.h"

struct reserve_extend {
	bool use_reserve_pool;
	int debug;
	int use_reserve_pool_max_time;
	int start_service_alloc_time;
	int scene;
	atomic_t slowpath;
	atomic_t reserve_slowpath;
	atomic_t reserve_long_slowpath;
	atomic_t use_order[NUM_ORDERS];
	atomic_t reserve_order[NUM_ORDERS];
	atomic_t refill_count[NUM_ORDERS];
	atomic_t refill_amount[NUM_ORDERS];
	atomic_t refill_amount_last[NUM_ORDERS];
	atomic64_t use_reserve_pool_start;
};

extern struct reserve_extend reserve_extend_info;
void mtk_sys_heap_reserve_pool_init(struct mtk_dmabuf_page_pool **global_pools);
struct page *mtk_extend_sys_heap_alloc_largest_available(
			struct mtk_dmabuf_page_pool **global_pools, unsigned long size,
			unsigned int max_order);
bool need_free_to_reserve_pool(int order_index);
#endif /* _MTK_EXTEND_SYSTEM_HEAP_H */
