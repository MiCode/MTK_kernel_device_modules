/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef _ENGINE_FIFO_H_
#define _ENGINE_FIFO_H_

#include <linux/types.h>
#include <linux/mm.h>
#include <linux/static_key.h>
#include <inc/engine_regs.h>

/*
 * Compression FIFO - BIT[15]: Tag-bit, BIT[10..0]: Entry-bits
 */
#define ENGINE_COMP_FIFO_MAX_BITS		(16)
#define ENGINE_COMP_FIFO_MAX_ENTRY_BITS		(11)	/* Just for sanity check */
#define ENGINE_COMP_FIFO_TAG_BIT		(ENGINE_COMP_FIFO_MAX_BITS - 1)
#define ENGINE_COMP_FIFO_TAG_BIT_MASK		(1 << ENGINE_COMP_FIFO_TAG_BIT)

/* Compression FIFOs */
#define ENGINE_COMP_FIFO_ENTRY_BITS		(CONFIG_ZRAM_ENGINE_COMP_FIFO_BITS)
#define ENGINE_COMP_FIFO_CARRY_BIT		(ENGINE_COMP_FIFO_ENTRY_BITS)
#define ENGINE_COMP_FIFO_ENTRY_MASK		((1UL << ENGINE_COMP_FIFO_ENTRY_BITS) - 1)
#define ENGINE_COMP_FIFO_ENTRY_CARRY_BITS	(ENGINE_COMP_FIFO_ENTRY_BITS + 1)
#define ENGINE_COMP_FIFO_ENTRY_CARRY_MASK	((1UL << ENGINE_COMP_FIFO_ENTRY_CARRY_BITS) - 1)
#define ENGINE_COMP_FIFO_PROPAGATION		((1UL << ENGINE_COMP_FIFO_TAG_BIT) \
						 - ENGINE_COMP_FIFO_ENTRY_MASK - 1)
#define ENGINE_COMP_FIFO_INDEX_MASK		(((1UL << ENGINE_COMP_FIFO_MAX_BITS) - 1) \
						 & ~ENGINE_COMP_FIFO_PROPAGATION)
#define ENGINE_COMP_FIFO_TAG_CARRY_OFFSET	(ENGINE_COMP_FIFO_TAG_BIT - ENGINE_COMP_FIFO_CARRY_BIT)

/* Batch interrupt for compression */
#define COMP_BATCH_INTR_CNT_BITS		(ENGINE_COMP_FIFO_ENTRY_BITS - 1)
#define ENGINE_COMP_BATCH_INTR_CNT_BITS		((COMP_BATCH_INTR_CNT_BITS > 6) ? COMP_BATCH_INTR_CNT_BITS : 6)

/* The number of compression fifos */
#define MAX_COMP_NR	(2)

/*
 * Decompression FIFO - BIT[15]: Tag-bit, BIT[6..0]: Entry-bits
 */
#define ENGINE_DCOMP_FIFO_MAX_BITS		(16)
#define ENGINE_DCOMP_FIFO_MAX_ENTRY_BITS	(7)
#define ENGINE_DCOMP_FIFO_TAG_BIT		(ENGINE_DCOMP_FIFO_MAX_BITS - 1)
/* Same as ENGINE_COMP_FIFO_TAG_BIT_MASK */
#define ENGINE_DCOMP_FIFO_TAG_BIT_MASK		(1 << ENGINE_DCOMP_FIFO_TAG_BIT)

/* Decompression Per-CPU FIFOs */
#define ENGINE_DCOMP_FIFO_ENTRY_BITS		(CONFIG_ZRAM_ENGINE_DECOMP_FIFO_BITS)
#define ENGINE_DCOMP_FIFO_CARRY_BIT		(ENGINE_DCOMP_FIFO_ENTRY_BITS)
#define ENGINE_DCOMP_FIFO_ENTRY_MASK		((1UL << ENGINE_DCOMP_FIFO_ENTRY_BITS) - 1)
#define ENGINE_DCOMP_FIFO_ENTRY_CARRY_BITS	(ENGINE_DCOMP_FIFO_ENTRY_BITS + 1)
#define ENGINE_DCOMP_FIFO_ENTRY_CARRY_MASK	((1UL << ENGINE_DCOMP_FIFO_ENTRY_CARRY_BITS) - 1)
#define ENGINE_DCOMP_FIFO_PROPAGATION		((1UL << ENGINE_DCOMP_FIFO_TAG_BIT) \
						 - ENGINE_DCOMP_FIFO_ENTRY_MASK - 1)
#define ENGINE_DCOMP_FIFO_INDEX_MASK		(((1UL << ENGINE_DCOMP_FIFO_MAX_BITS) - 1) \
						 & ~ENGINE_DCOMP_FIFO_PROPAGATION)
#define ENGINE_DCOMP_FIFO_TAG_CARRY_OFFSET	(ENGINE_DCOMP_FIFO_TAG_BIT - ENGINE_DCOMP_FIFO_CARRY_BIT)

/* Batch interrupt for decompression */
#define ENGINE_DCOMP_BATCH_INTR_CNT_BITS	(3)	/* 2^3 */

/* The number of dcompression set */
#define MAX_DCOMP_NR	(8)

/* Hang detect upper bound */
#define HANG_DETECT_BOUND	(3)
#define SUSPECT_HANG_BOUND	(10000)

/*
 * Structure for HW engine FIFO
 */
struct hwfifo {
	/*
	 * SW copies for indices of HW fifo -
	 *
	 * When translating SW index to HW one, the entry bits will be copied directly.
	 * The carry-bit(C) of SW index will be assigned to the tag-bit(T) of HW one.
	 *
	 * [SW] 1 Carry bit(C) + N entry bits	                     (total 1+N bits)
	 *
	 *  		| |  entry-bits |
	 *   	 	^ N-1           0
	 *   	 	C
	 *
	 * [HW] 1 Tag bit(T) + (16-1-N) reserved bits + N entry bits  (total 16 bits)
	 *
	 * 	| | reserved-bits | entry-bits |
	 *      ^ 14              N-1          0
	 *      T
	 */
	uint32_t write_idx;
	uint32_t complete_idx;
	uint32_t pp_prev_end;
	uint32_t accu_usage;	/* Accumulated usage (monotonically increased or set to 0 when fifo switch) */

	/* Corresponding Registers */
	void __iomem *write_idx_reg;
	void __iomem *complete_idx_reg;
	void __iomem *offset_idx_reg;

	/* Keep track of ongoing operations */
	void *completion;

	/* fifo depth */
	uint32_t size;

	/* fifo private id */
	uint32_t id;

	/* In DRAM fifo */
	void *buf;
	phys_addr_t buf_pa;	/* Must be 4K-aligned */

	/* Private data for current fifo implementation (May be NULL) */
	void *priv;

	/* - No use of fields for mask operations will save up to 5 more instructions per function call - */

} ____cacheline_internodealigned_in_smp;

/*
 * Macros to create function defintions for fifos -
 *
 * ex. ENGINE_FIFO_OPS(COMP, comp) will create following functions,
 *
 * // Get the SW copy of fifo's write index
 *	static inline uint32_t comp_fifo_write_entry(struct hwfifo *fifo)
 * // Get the SW copy of fifo's complete index
 *	static inline uint32_t comp_fifo_complete_entry(struct hwfifo *fifo)
 * // Query whether fifo is full
 *	static bool comp_fifo_full(struct hwfifo *fifo)
 * // Query whether fifo is empty
 *	static bool comp_fifo_empty(struct hwfifo *fifo)
 * // Increase the fifo's write index (including the HW one) by 1
 *	static inline void update_comp_fifo_write_index(struct hwfifo *fifo)
 * // Increase the fifo's write index (including the HW one) by 1 without kick
 *	static inline void update_comp_fifo_write_index_nokick(struct hwfifo *fifo)
 * // Increase the SW copy of fifo's complete index by 1
 *	static inline void update_comp_fifo_complete_index(struct hwfifo *fifo)
 * // Translate the HW complete index to the SW one
 *	static inline uint32_t comp_fifo_HtS_complete_index(struct hwfifo *fifo)
 * // Translate the SW complete index to the HW one (debug purpose)
 *	static inline uint32_t comp_fifo_StH_complete_index(struct hwfifo *fifo)
 */
#define ENGINE_FIFO_OPS(uname, lname)								\
static inline uint32_t lname##_fifo_write_entry(struct hwfifo *fifo)				\
{ return fifo->write_idx & ENGINE_##uname##_FIFO_ENTRY_MASK; }					\
static inline uint32_t lname##_fifo_complete_entry(struct hwfifo *fifo)				\
{ return fifo->complete_idx & ENGINE_##uname##_FIFO_ENTRY_MASK; }				\
static bool lname##_fifo_full(struct hwfifo *fifo)						\
{												\
	uint32_t write_idx = lname##_fifo_write_entry(fifo);					\
	uint32_t complete_idx = lname##_fifo_complete_entry(fifo);				\
												\
	if (write_idx != complete_idx)								\
		return false;									\
												\
	return fifo->write_idx != fifo->complete_idx;						\
}												\
static bool lname##_fifo_empty(struct hwfifo *fifo)						\
{ return fifo->write_idx == fifo->complete_idx; }						\
static inline void update_##lname##_fifo_write_index(struct hwfifo *fifo)			\
{												\
	uint32_t next_write_idx, next_hw_write_idx;						\
												\
	next_write_idx = (fifo->write_idx + 1)							\
			 & ENGINE_##uname##_FIFO_ENTRY_CARRY_MASK;				\
	next_hw_write_idx = (next_write_idx + ENGINE_##uname##_FIFO_PROPAGATION)		\
			    & ENGINE_##uname##_FIFO_INDEX_MASK;					\
												\
	wmb();											\
												\
	fifo->write_idx = next_write_idx;							\
												\
	writel(ENGINE_START_MASK | next_hw_write_idx, fifo->write_idx_reg);			\
}												\
static inline void update_##lname##_fifo_write_index_nokick(struct hwfifo *fifo)		\
{												\
	uint32_t next_write_idx, next_hw_write_idx;						\
												\
	next_write_idx = (fifo->write_idx + 1)							\
			 & ENGINE_##uname##_FIFO_ENTRY_CARRY_MASK;				\
	next_hw_write_idx = (next_write_idx + ENGINE_##uname##_FIFO_PROPAGATION)		\
			    & ENGINE_##uname##_FIFO_INDEX_MASK;					\
												\
	wmb();											\
												\
	fifo->write_idx = next_write_idx;							\
												\
	writel(next_hw_write_idx, fifo->write_idx_reg);						\
}												\
static inline void update_##lname##_pfifo_write_index(struct hwfifo *fifo)			\
{												\
	uint32_t next_write_idx, next_hw_write_idx;						\
												\
	next_write_idx = (fifo->write_idx + 1)							\
			 & ENGINE_##uname##_FIFO_ENTRY_CARRY_MASK;				\
	next_hw_write_idx = (next_write_idx + ENGINE_##uname##_FIFO_PROPAGATION)		\
			    & ENGINE_##uname##_FIFO_INDEX_MASK;					\
												\
	/* To make sure write order is perceived correctly */					\
	wmb();											\
												\
	fifo->write_idx = next_write_idx;							\
}												\
static inline void update_##lname##_fifo_complete_index(struct hwfifo *fifo)			\
{												\
	uint32_t next_complete_idx;								\
												\
	next_complete_idx = (fifo->complete_idx + 1)						\
			    & ENGINE_##uname##_FIFO_ENTRY_CARRY_MASK;				\
												\
	smp_store_release(&fifo->complete_idx, next_complete_idx);				\
}												\
static inline uint32_t lname##_fifo_HtS_complete_index(struct hwfifo *fifo)			\
{												\
	uint32_t hw_complete_idx = readl(fifo->complete_idx_reg);				\
												\
	return (((hw_complete_idx & ENGINE_COMP_FIFO_TAG_BIT_MASK)				\
			>> ENGINE_##uname##_FIFO_TAG_CARRY_OFFSET)				\
			| (hw_complete_idx & ENGINE_##uname##_FIFO_ENTRY_MASK));		\
}												\
static inline uint32_t lname##_fifo_StH_complete_index(struct hwfifo *fifo)			\
{												\
	return (fifo->complete_idx + ENGINE_##uname##_FIFO_PROPAGATION)				\
		& ENGINE_##uname##_FIFO_INDEX_MASK;						\
}												\
static inline uint32_t lname##_fifo_StH_write_index(struct hwfifo *fifo)			\
{												\
	return (fifo->write_idx + ENGINE_##uname##_FIFO_PROPAGATION)				\
		& ENGINE_##uname##_FIFO_INDEX_MASK;						\
}												\
static inline bool lname##_fifo_indices_invalid(uint32_t write_idx, uint32_t complete_idx)	\
{												\
	uint32_t write_entry, complete_entry;							\
	uint32_t write_tag, complete_tag;							\
												\
	write_entry = write_idx & ENGINE_##uname##_FIFO_ENTRY_MASK;				\
	complete_entry = complete_idx & ENGINE_##uname##_FIFO_ENTRY_MASK;			\
	write_tag = write_idx >> ENGINE_##uname##_FIFO_CARRY_BIT;				\
	complete_tag = complete_idx >> ENGINE_##uname##_FIFO_CARRY_BIT;				\
												\
	if (write_tag == complete_tag) {							\
		if (complete_entry > write_entry)						\
			return true;								\
	} else {										\
		if (write_entry > complete_entry)						\
			return true;								\
	}											\
												\
	return false;										\
}

/*
 * Common for both compression & decompression CMDs
 */
#define CMD_INTR_EN	(0x1)

/*
 * For SRC/DST addr operations
 */
#define BUF_LSB_POS_SHIFT	(6)
#define BUF_LSB_POS_MASK	((1UL << 31) - 1)
#define BUF_MSB_POS_SHIFT	(26)
#define BUF_MSB_POS_MASK	(BUF_LSB_POS_MASK << 32)

#define DST_BUF_ADDR_SHIFT	(6)
#define PHYS_ADDR_TO_DST(addr)	(addr >> DST_BUF_ADDR_SHIFT)	/* Can be stuffed in 32-bits */
#define DST_ADDR_TO_PHYS(addr)	(addr << DST_BUF_ADDR_SHIFT)	/* Be careful: it can't be stuffed into 32-bits */
#define BUF_ADDR_BITS_MASK	((1UL << 31) - 1)		/* Same number of bits as dst/src_addr_x */

/*
 * Compression FIFO CMD Format
 */
struct compress_cmd {

	/* Offset: 0x0 */
	union {
		struct {
			uint64_t status:4;
			uint64_t :1;
			uint64_t interrupt:1;
			uint64_t :2;
			uint64_t fifo:1;	/* 0b or 1b */
			uint64_t :3;
			uint64_t src_addr:24;
			uint64_t :20;
			uint64_t buf_enable:7;	/* 1000000b or 0111111b */
			uint64_t :1;
		};
		uint64_t word_0_value;
	};

	/* Offset: 0x8 */
	union {
		struct {
			uint64_t bufsel:7;
			uint64_t :1;
			uint64_t repeat_pattern:8;
			uint64_t compr_size:16;
			uint64_t hash_result:32;
		};
		uint64_t word_1_value;
	};

	/* Offset: 0x10 (reserved) */
	uint64_t word_2_value;

	/* Offset: 0x18 (reserved) */
	uint64_t word_3_value;

	/* Offset: 0x20 */
	union {
		struct {
			uint64_t dst_addr_a:31;	/* 2048 bytes: bit[4..0]:0 */
			uint64_t :1;
			uint64_t dst_addr_b:31; /* 1024 bytes: bit[35..32]:0 */
			uint64_t :1;
		};
		uint64_t word_4_value;
	};

	/* Offset: 0x28 */
	union {
		struct {
			uint64_t dst_addr_c:31;	/*  512 bytes: bit[2..0]:0 */
			uint64_t :1;
			uint64_t dst_addr_d:31; /*  256 bytes: bit[33..32]:0 */
			uint64_t :1;
		};
		uint64_t word_5_value;
	};

	/* Offset: 0x30 */
	union {
		struct {
			uint64_t dst_addr_e:31;	/*  128 bytes: bit[0]:0 */
			uint64_t :1;
			uint64_t dst_addr_f:31; /*   64 bytes */
			uint64_t :1;
		};
		uint64_t word_6_value;
	};

	/* Offset: 0x38 */
	union {
		struct {
			uint64_t dst_addr_g:31; /* 4096 bytes: bit[5..0]:0 */
			uint64_t :33;
		};
		uint64_t word_7_value;
	};
};

#define ENGINE_COMP_CMD_SIZE	sizeof(struct compress_cmd)

/*
 * Compression CMD status
 */
enum comp_cmd_status {

	/* HW only accepts IDLE or PEND for processing. */

	COMP_CMD_IDLE = 0x0,		/* Available for SW to add new request */
	COMP_CMD_PEND = 0x1,		/* Contains a pending request for HW processing.
					   Should not be updated to PEND before DST buffers are ready */

	/* Following are for the status after it is processed by HW */

	COMP_CMD_REPEATED = 0x2,	/* Contains repeated pattern */
	COMP_CMD_INCOMPRESSIBLE = 0x3,	/* Compressed size larger than ENGINE_MAX_COMPRESSIBLE_SIZE */
	COMP_CMD_COMPRESSED = 0x4,	/* Successfully compressed by HW */
	COMP_CMD_ERROR = 0x5,		/* This CMD is not processed correctly during HW compression */

	/* Operated by SW only */
	COMP_CMD_PP_ERROR = 0xf,	/* This CMD is not post-processed successfully
					   (e.g. fail to refill buffers...) */

	COMP_CMD_STATUS_MASK = 0xf,
};

#define COMP_CMD_OFFSET_0_SRC_ADDR_MASK		(0xffffffUL)		/* 24 bits */

/*
 * Check whether the cmd is valid -
 * Expected initial cmd status is COMP_CMD_IDLE.
 * (After post-processing, it may be COMP_CMD_PP_ERROR when some error occurs.)
 * If not COMP_CMD_IDLE, will reset it as COMP_CMD_IDLE and tell caller it's invalid.
 */
static inline bool comp_cmd_check_invalid(struct compress_cmd *cmd, uint32_t entry)
{
	if (cmd->status != COMP_CMD_IDLE) {
		/*
		 * This cmd is invalid.
		 * HW will bypass this cmd when it is set as IDLE.
		 * What SW needs to do is only update complete_index and try the next one.
		 */
		pr_info("%s: (0x%llx)fifo(%u) - cmd status is (0x%x), not (0x%x) at (0x%x).\n",
			__func__, cmd->word_0_value, cmd->fifo, cmd->status, COMP_CMD_IDLE, entry);
		cmd->status = COMP_CMD_IDLE;
		return true;
	}

	return false;
}

/*
 * Only valid cmd will be updated here -
 * Expected initial cmd status is COMP_CMD_IDLE, and src_addr should be 0.
 * Warning if one of above conditions is not true.
 * Initialize the cmd with the physical address of src_page and set it as COMP_CMD_PEND.
 */
static inline void update_cmd_before_compression(struct compress_cmd *cmd,
		struct page *src_page, bool cmd_intr_en)
{
	WARN_ON(cmd->status != COMP_CMD_IDLE);
	WARN_ON(cmd->src_addr != 0x0);

	/* 4K aligned: should be "page_to_phys(src_page) >> 12" if we want to support 16K page (TODO) */
	cmd->src_addr = page_to_pfn(src_page) & COMP_CMD_OFFSET_0_SRC_ADDR_MASK;

	if (cmd_intr_en)
		cmd->interrupt = CMD_INTR_EN;

	cmd->status = COMP_CMD_PEND;

	/*
	 * Setup of compression CMD is completed.
	 * Please update HW write_index to notify HW engine a newly request is coming after this function.
	 */
}

/* Return compression status */
static inline unsigned int get_comp_cmd_status(struct compress_cmd *cmd)
{
	return ((unsigned int)READ_ONCE(cmd->word_0_value)) & COMP_CMD_STATUS_MASK;
}

/* Check whether compression result is incompressible */
static inline bool comp_cmd_is_incompressible(struct compress_cmd *cmd)
{
	return (((unsigned int)READ_ONCE(cmd->word_0_value)) & COMP_CMD_STATUS_MASK) == COMP_CMD_INCOMPRESSIBLE;
}

/* Set cmd as idle and reset src_addr to 0 (used for reset case) */
static inline void set_comp_cmd_as_idle(struct compress_cmd *cmd)
{
	cmd->status = COMP_CMD_IDLE;
	cmd->src_addr = 0x0;
	cmd->word_1_value = 0x0;
	wmb();
}

/* Set cmd as error */
static inline void set_comp_cmd_as_error(struct compress_cmd *cmd)
{
	cmd->status = COMP_CMD_ERROR;

	/* To make sure write order is perceived correctly */
	wmb();
}

/*
 * Decompression FIFO CMD Format -
 */

/* ** Reserved bits in 0x0 will be cleared to 0 after HW processing. ** */
/* ** 0x10, 0x18 will also be cleared to 0 after HW processing. ** */
struct decompress_cmd {

	/* Offset: 0x0 */
	union {
		struct {
			uint64_t status:4;
			uint64_t :1;
			uint64_t interrupt:1;
			uint64_t hash_verify:1;
			uint64_t :1;
			uint64_t fifo:3;	/* 000b ~ 111b */
			uint64_t :1;
			uint64_t dst_addr:24;
			uint64_t :12;
			uint64_t buf_a:4;	/* 0: empty, 1: 64bytes, 2: 128bytes, 3: 256bytes, 4: 512bytes,
						   5: 1024bytes, 6:2048bytes, 7: 4096bytes */
			uint64_t buf_b:4;	/* The BUFs should be placed in order according to the size:
						   buf_a > buf_b > buf_c > buf_d */
			uint64_t buf_c:4;
			uint64_t buf_d:4;
		};
		uint64_t word_0_value;
	};

	/* Offset: 0x8 */
	union {
		struct {
			uint64_t sync:1;	/* Used by SW only */
			uint64_t :15;
			uint64_t compr_size:16;
			uint64_t hash_value:32;
		};
		uint64_t word_1_value;
	};

	/* Offset: 0x10 */
	union {
		struct {
			uint64_t src_addr_a:31;
			uint64_t :1;
			uint64_t src_addr_b:31;
			uint64_t :1;
		};
		uint64_t word_2_value;
	};

	/* Offset: 0x18 */
	union {
		struct {
			uint64_t src_addr_c:31;
			uint64_t :1;
			uint64_t src_addr_d:31;
			uint64_t :1;
		};
		uint64_t word_3_value;
	};
};

#define ENGINE_DCOMP_CMD_SIZE	sizeof(struct decompress_cmd)

/*
 * Decompression CMD status
 */
enum dcomp_cmd_status {

	/* HW only accepts IDLE or PEND for processing. */

	DCOMP_CMD_IDLE = 0x0,		/* Available for SW to add new request */
	DCOMP_CMD_PEND = 0x1,		/* Contains a pending request for HW processing.
					   Should not be updated to PEND before DST buffers are ready */

	/* Following are for the status after it is processed by HW */

	DCOMP_CMD_DECOMPRESSED = 0x4,	/* Successfully decompressed by HW */
	DCOMP_CMD_ERROR = 0x5,		/* This CMD is not processed correctly during HW decompression */
	DCOMP_CMD_HASH_FAIL = 0x6,	/* Successfully decompressed by HW, but the HASH verification is failed */

	DCOMP_CMD_STATUS_MASK = 0xf,
};

#define DCOMP_CMD_OFFSET_0_DST_ADDR_MASK		(0xffffffUL)	/* 24 bits */

static inline bool dcomp_cmd_check_invalid(struct decompress_cmd *cmd)
{
	if (cmd->status != DCOMP_CMD_IDLE) {
		/*
		 * This cmd is invalid.
		 * HW will bypass this cmd when it is set as IDLE.
		 * What SW needs to do is only update complete_index and try the next one.
		 */
		pr_info("%s: cmd status is (0x%x), not (0x%x).\n", __func__, cmd->status, DCOMP_CMD_IDLE);
		cmd->status = DCOMP_CMD_IDLE;
		return true;
	}

	return false;
}

#define CMD_HASH_VERIFY_EN	(0x1)
#define CMD_ASYNC		(0x0)
#define CMD_SYNC		(0x1)

static inline void update_cmd_before_decompression(struct decompress_cmd *cmd,
		struct page *dst_page, unsigned int slen, uint32_t hash_value,
		bool cmd_intr_en, bool hash_verify, bool async)
{
	WARN_ON(cmd->status != DCOMP_CMD_IDLE);
	WARN_ON(cmd->dst_addr != 0x0);

	/* Reset word_1_value as 0 for HW output */
	cmd->word_1_value = 0x0;

	/* 4K aligned: should be "page_to_phys(dst_page) >> 12" if we want to support 16K page (TODO) */
	cmd->dst_addr = page_to_pfn(dst_page) & DCOMP_CMD_OFFSET_0_DST_ADDR_MASK;
	cmd->compr_size = slen;

	if (cmd_intr_en)
		cmd->interrupt = CMD_INTR_EN;

	if (hash_verify) {
		cmd->hash_value = hash_value;
		cmd->hash_verify = CMD_HASH_VERIFY_EN;
	}

	/* Used by SW to identify if current decompression is sync or async */
	if (async)
		cmd->sync = CMD_ASYNC;
	else
		cmd->sync = CMD_SYNC;

	cmd->status = DCOMP_CMD_PEND;

	/*
	 * Setup of decompression CMD is completed.
	 * Please update HW write_index to notify HW engine a newly request is coming after this function.
	 */
}

static inline bool decompress_cmd_sync(struct decompress_cmd *cmd)
{
	return (cmd->sync == CMD_SYNC);
}

/* Poll & wait until cmd status is not cmd_status */
static inline int poll_dcomp_cmd_not_status(struct decompress_cmd *cmd,
		enum dcomp_cmd_status cmd_status, unsigned long timeout)
{
	unsigned int dcomp_status;

	/* Update timeout according to jiffies */
	timeout = jiffies + msecs_to_jiffies(timeout);
	do {
		cpu_relax();
		if (time_after(jiffies, timeout))
			return -ETIME;

		dcomp_status = ((unsigned int)READ_ONCE(cmd->word_0_value)) & DCOMP_CMD_STATUS_MASK;
	} while (dcomp_status == cmd_status);

	/* Success */
	return 0;
}

/* Poll & wait until cmd status is cmd_status */
static inline int poll_dcomp_cmd_status(struct decompress_cmd *cmd,
		enum dcomp_cmd_status cmd_status, unsigned long timeout)
{
	unsigned int dcomp_status;

	/* Update timeout according to jiffies */
	timeout = jiffies + msecs_to_jiffies(timeout);
	do {
		cpu_relax();
		if (time_after(jiffies, timeout))
			return -ETIME;

		dcomp_status = ((unsigned int)READ_ONCE(cmd->word_0_value)) & DCOMP_CMD_STATUS_MASK;
	} while (dcomp_status != cmd_status);

	/* Success */
	return 0;
}

/* Macros to acquire cmd & completion */
#define	COMP_CMD(fifo, entry)	(fifo->buf + entry * ENGINE_COMP_CMD_SIZE)
#define	COMP_CMPL(fifo, entry)	(fifo->completion + entry * sizeof(struct comp_pp_info))
#define	DCOMP_CMD(fifo, entry)	(fifo->buf + entry * ENGINE_DCOMP_CMD_SIZE)
#define	DCOMP_CMPL(fifo, entry)	(fifo->completion + entry * sizeof(struct dcomp_pp_info))

/* 64, 128, 256, 512, 1024, 2048, 4096 */
#define ENGINE_NUM_OF_BUF_SIZES		(7)
#define ENGINE_NO_DST_COPY_MAX_BUFSZ	(2048)
#define ENGINE_DST_COPY_MAX_BUFSZ	(4096)

void free_entry_buffer(uint32_t *buf_addr, int index);
int refill_entry_buffer(uint32_t *buf_addr, int index, bool invalidate);

/* Coherence control */
DECLARE_STATIC_KEY_FALSE(engine_no_coherence);
static inline bool engine_coherence_disabled(void)
{
	return static_branch_unlikely(&engine_no_coherence);
}

/* Async(by interrupt) or Sync(by polling) mode control */
DECLARE_STATIC_KEY_FALSE(engine_sync_mode);
static inline bool engine_async_mode_disabled(void)
{
	return static_branch_unlikely(&engine_sync_mode);
}

#endif /* _ENGINE_FIFO_H_ */

