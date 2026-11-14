/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef _HELPERS_H_
#define _HELPERS_H_

#include <linux/printk.h>
#include <linux/sprintf.h>
#include <linux/seq_file.h>
#include <inc/engine_fifo.h>

int printbinary(char *buf, unsigned long x, int nbits);
void dump_cmd(void *cmdp, unsigned int cmdsize);
void dump_comp_cmd(struct compress_cmd *cmdp);
void seq_dump_comp_cmd(struct seq_file *m, struct compress_cmd *cmdp);
void dump_all_comp_cmd(struct hwfifo *fifo);
void dump_dcomp_cmd(struct decompress_cmd *cmdp);
void dump_all_dcomp_cmd(struct hwfifo *fifo);
void flush_dcache(unsigned long start, unsigned long end);
void invalidate_dcache(unsigned long start, unsigned long end);

static inline void *cmd_buf_addr_to_va(uint32_t *addr)
{
	uint64_t addr_to_be_free;

	addr_to_be_free = *addr;

	return phys_to_virt(DST_ADDR_TO_PHYS(addr_to_be_free));
}

#define ZRAM_DEBUG_DUMP(buf, copied, bufp, strlen, fmt, args...)	\
	do {								\
		if (buf) {						\
			copied += snprintf(bufp, strlen, fmt, ##args);	\
		} else {						\
			pr_info(fmt, ##args);				\
		}							\
	} while (0)

#endif /* _HELPERS_H_ */
