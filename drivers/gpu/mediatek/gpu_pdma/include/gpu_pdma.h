/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef GPU_PDMA_H
#define GPU_PDMA_H

/* Export function */
void pdma_lock_reclaim(u32 kctx_id);
u32 pdma_request_extended_pbha(u32 kctx_id);
void pdma_release_extended_pbha(u32 kctx_id, u32 pbha_id);
void pdma_zombie_entry_clean_up(void);

#endif /* GPU_PDMA_H */
