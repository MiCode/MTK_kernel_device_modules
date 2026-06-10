// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direct.h>

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched_clock.h>
#include <linux/swap.h>
#include <linux/highmem.h>
#include <inc/helpers.h>

int printbinary(char *buf, unsigned long x, int nbits)
{
	unsigned long mask = 1UL << (nbits - 1);

	while (mask != 0) {
		*buf++ = (mask & x ? '1' : '0');
		mask >>= 1;
	}
	*buf = '\0';

	return nbits;
}

static void print_header_series(void)
{
	char headers[128];
	int len = 0;
	int i;

	len += snprintf(headers, 128, "%8s", " ");
	for (i = 63; i >= 0; i--) {
		if (i % 4 == 0)
			len += snprintf(&headers[len], 128 - len, "%d", (int)(i / 10));
		else
			len += snprintf(&headers[len], 128 - len, "%s", " ");
	}
	pr_info("%s", headers);
	len = 0;
	len += snprintf(headers, 128, "%8s", "OFFSET: ");
	for (i = 63; i >= 0; i--) {
		if (i % 4 == 0)
			len += snprintf(&headers[len], 128 - len, "%d", i % 10);
		else
			len += snprintf(&headers[len], 128 - len, "%s", "-");
	}
	pr_info("%s", headers);
}

static void seq_print_header_series(struct seq_file *m)
{
	char headers[128];
	int len = 0;
	int i;

	len += snprintf(headers, 128, "%8s", " ");
	for (i = 63; i >= 0; i--) {
		if (i % 4 == 0)
			len += snprintf(&headers[len], 128 - len, "%d", (int)(i / 10));
		else
			len += snprintf(&headers[len], 128 - len, "%s", " ");
	}
	seq_printf(m, "%s\n", headers);
	len = 0;
	len += snprintf(headers, 128, "%8s", "OFFSET: ");
	for (i = 63; i >= 0; i--) {
		if (i % 4 == 0)
			len += snprintf(&headers[len], 128 - len, "%d", i % 10);
		else
			len += snprintf(&headers[len], 128 - len, "%s", "-");
	}
	seq_printf(m, "%s\n", headers);
}

#define BIN_STRING_LEN	(sizeof(uint64_t) * 8)

void dump_cmd(void *cmdp, unsigned int cmdsize)
{
	uint64_t *ptr;
	int i, loop = cmdsize / sizeof(uint64_t);
	char bins[BIN_STRING_LEN + 1];
	int nbits;

	/* Hexadecimal dump */
	pr_info("***HEXDUMP***\n");
	ptr = (uint64_t *)cmdp;
	for (i = 0; i < loop; i++, ptr++)
		pr_info("Offset[0x%-2lx]: 0x%llx\n", i * sizeof(uint64_t), *ptr);

	/* Binary dump */
	pr_info("***BINDUMP***\n");
	ptr = (uint64_t *)cmdp;
	print_header_series();
	for (i = 0; i < loop; i++, ptr++) {
		nbits = printbinary(bins, *ptr, 64);
		if (nbits)
			pr_info("%2s0x%-2lx: %s\n", " ", i * sizeof(uint64_t), bins);
	}
}

void dump_comp_cmd(struct compress_cmd *cmdp)
{
	dump_cmd(cmdp, sizeof(*cmdp));
}

void seq_dump_comp_cmd(struct seq_file *m, struct compress_cmd *cmdp)
{
	uint64_t *ptr;
	int i, loop = sizeof(*cmdp) / sizeof(uint64_t);
	char bins[BIN_STRING_LEN + 1];
	int nbits;

	/* Hexadecimal dump */
	seq_puts(m, "***HEXDUMP***\n");
	ptr = (uint64_t *)cmdp;
	for (i = 0; i < loop; i++, ptr++)
		seq_printf(m, "Offset[0x%-2lx]: 0x%llx\n", i * sizeof(uint64_t), *ptr);

	/* Binary dump */
	seq_puts(m, "***BINDUMP***\n");
	ptr = (uint64_t *)cmdp;
	seq_print_header_series(m);
	for (i = 0; i < loop; i++, ptr++) {
		nbits = printbinary(bins, *ptr, 64);
		if (nbits)
			seq_printf(m, "%2s0x%-2lx: %s\n", " ", i * sizeof(uint64_t), bins);
	}
}

void dump_all_comp_cmd(struct hwfifo *fifo)
{
	struct compress_cmd *cmdp;
	uint32_t entry;

	if (!fifo)
		return;

	for (entry = 0; entry < fifo->size; entry++) {
		cmdp = COMP_CMD(fifo, entry);
		pr_info("\nCompression CMD: [%u]\n", entry);
		dump_comp_cmd(cmdp);
	}
}

void dump_dcomp_cmd(struct decompress_cmd *cmdp)
{
	dump_cmd(cmdp, sizeof(*cmdp));
}

void dump_all_dcomp_cmd(struct hwfifo *fifo)
{
	struct decompress_cmd *cmdp;
	uint32_t entry;

	if (!fifo)
		return;

	for (entry = 0; entry < fifo->size; entry++) {
		cmdp = DCOMP_CMD(fifo, entry);
		pr_info("\nDecompression CMD: [%u]\n", entry);
		dump_dcomp_cmd(cmdp);
	}
}

#define DEFAULT_CACHELINE_SIZE	(0x40)
#define DEFAULT_CACHELINE_MASK	(~(DEFAULT_CACHELINE_SIZE - 1))
void flush_dcache(unsigned long start, unsigned long end)
{
	unsigned long base;

	base = start & DEFAULT_CACHELINE_MASK;

	for (; base < end; base += DEFAULT_CACHELINE_SIZE)
		asm volatile ("dc civac, %0" :: "r" (base) : "memory");

	asm volatile("dsb sy" : : : "memory");
}

void invalidate_dcache(unsigned long start, unsigned long end)
{
	unsigned long base;

	base = start & DEFAULT_CACHELINE_MASK;

	for (; base < end; base += DEFAULT_CACHELINE_SIZE)
		asm volatile ("dc ivac, %0" :: "r" (base) : "memory");

	asm volatile("dsb sy" : : : "memory");
}
#undef DEFAULT_CACHELINE_MASK
#undef DEFAULT_CACHELINE_SIZE
