// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#include <linux/module.h>
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

#include <linux/kmemleak.h>

#include <linux/pm.h>
#include <linux/pm_runtime.h>

#include <linux/psi.h>

#include <linux/mutex.h>
#include <linux/delay.h>
#include <asm-generic/rwonce.h>

#include <inc/engine_fifo.h>
#include <inc/engine_regs.h>
#include <inc/engine_gears.h>
#include <inc/engine_ops.h>
#include <inc/helpers.h>

#include <hwcomp_bridge.h>

/* TBU monitor */
#include <mtk-iommu-util.h>

/* Thermal notifier */
#include "thermal_interface.h"
#include <linux/notifier.h>

/*
 * DST copy -
 * [0]: 2048, [1]: 1024, [2]: 512, [3]: 256, [4]: 128, [5]: 64
 *
 * No DST copy -
 * [6]: 4096
 */
static struct kmem_cache *size_allocator[ENGINE_NUM_OF_BUF_SIZES];
const unsigned int bufsz[ENGINE_NUM_OF_BUF_SIZES] = { 2048, 1024, 512, 256, 128, 64, 4096 };

struct zram_engine_t {

	/* Compression fifo for kswapd only (lockless) */
	struct hwfifo fifo_1;

	/* Compression fifo for others */
	struct hwfifo fifo_2;
	spinlock_t fifo_2_lock;

	/* The number of compression requests */
	atomic_t comp_cnt;

	CACHELINE_PADDING(__pad__0);

	/* Decompression per-cpu fifos */
	struct hwfifo dcomp_fifo[MAX_DCOMP_NR];

	/* Engine controls */
	struct engine_control_t ctrl;

	/* Engine operations for DST & No-DST copy modes */
	const struct engine_operations_struct *ops;

	/* The number of decompression requests */
	atomic_t dcomp_cnt;

	CACHELINE_PADDING(__pad__1);

	/* Engine gear controls */
	struct engine_gear_control_t gear_ctrl;

	/* Compression post processing relatives */
	wait_queue_head_t comp_wait;
	struct task_struct *comp_pp_work;

	CACHELINE_PADDING(__pad__2);

	/* Decompression post processing relatives */
	wait_queue_head_t dcomp_wait;
	struct task_struct *dcomp_pp_work;

	/* Gear-level-adjust relatives */
	wait_queue_head_t gla_wait;
	struct task_struct *gla_work;

	/* Reference count */
	refcount_t refcount;

	/* Avoid mode switching between No-DST and DST copy */
	atomic_t mode_locked;

	/* List for available HW engine */
	struct list_head list;
};

/* Available HW engines */
#define HWZ_INIT_REFCNT		(1)
#define HWZ_MAX_REFCNT		(2)
static LIST_HEAD(hwz_list);

/* Mode lock relatives */
#define HWCOMP_MODE_UNLOCK	(0)

/*
 * Global lock for engine related controls -
 * 1. Operations on hwz_list
 * 2. Mode switch related operations
 */
static DEFINE_MUTEX(hwz_mutex);

/*
 * Definition of FIFO operations
 */
ENGINE_FIFO_OPS(COMP, comp, _1)
ENGINE_FIFO_OPS(COMP, comp, _2)
ENGINE_FIFO_OPS(DCOMP, dcomp, )

/*
 * ATTENTION -
 * Following callbacks are initialized ONLY in hwcomp_create.
 */

/* Post-process callback for compression */
compress_pp_fn hwcomp_compress_post_process_dc;
#if IS_ENABLED(CONFIG_HWCOMP_SUPPORT_NO_DST_COPY)
compress_pp_fn hwcomp_compress_post_process_ndc;
#endif

/* Post-process callback for decompression */
decompress_pp_fn hwcomp_decompress_post_process;

/*
 * Static Key Controls
 */

/* Coherence control */
DEFINE_STATIC_KEY_FALSE(engine_no_coherence);

/* Async(by interrupt) or Sync(by polling) mode control */
DEFINE_STATIC_KEY_FALSE(engine_sync_mode);

/*
 * Power efficiency -
 * This may bring some overhead when sending initial requests.
 */
DEFINE_STATIC_KEY_TRUE(engine_power_efficiency);

/* RTFF check control */
DEFINE_STATIC_KEY_FALSE(engine_rtff_check);

/* Statistics for suspect hang */
static atomic_t enc_suspect_hang_count = ATOMIC_INIT(0);
static atomic_t dec_suspect_hang_count = ATOMIC_INIT(0);

/* Statistics for rtff status */
static atomic_t engine_rtff_pass_count = ATOMIC_INIT(0);
static atomic_t engine_rtff_fail_count = ATOMIC_INIT(0);

/*
 * Request to adjust gear level
 */
enum glaflags {
	GLA_notify,		/* Someone has a request */
	GLA_encset,		/* Set to default gear level for compression */
	GLA_decset,		/* Set to default gear level for decompression */
	GLA_encnotready,	/* Gear set up is ready for compression */
	GLA_encdone,		/* Gear down for compression */
	GLA_decdone,		/* Gear down for decompression */
	GLA_encrst,		/* Notify requesters we will reset compression. Don't bother us */
	GLA_decrst,		/* Notify requesters we will reset decompression. Don't bother us */
};

#define SETGLAFLAG(uname, lname)					\
static __always_inline void SetGla##uname(unsigned long *flags)		\
{ set_bit(GLA_##lname, flags); }

#define CLEARGLAFLAG(uname, lname)					\
static __always_inline void ClearGla##uname(unsigned long *flags)		\
{ clear_bit(GLA_##lname, flags); }

#define TESTGLACLEARFLAG(uname, lname)					\
static __always_inline bool TestClearGla##uname(unsigned long *flags)	\
{ return test_and_clear_bit(GLA_##lname, flags); }

#define TESTGLAFLAG(uname, lname)					\
static __always_inline bool Gla##uname(unsigned long *flags)		\
{ return test_bit(GLA_##lname, flags); }

#define GLAFLAG(uname, lname)						\
	SETGLAFLAG(uname, lname)					\
	CLEARGLAFLAG(uname, lname)					\
	TESTGLAFLAG(uname, lname)					\
	TESTGLACLEARFLAG(uname, lname)

GLAFLAG(Notify, notify);
GLAFLAG(Encset, encset);
GLAFLAG(Decset, decset);
GLAFLAG(Encdone, encdone);
GLAFLAG(Decdone, decdone);
GLAFLAG(EncNotready, encnotready);
GLAFLAG(EncRst, encrst);
GLAFLAG(DecRst, decrst);

static unsigned long gla_flags;

/**************************************************/

static int __maybe_unused mtk_hwzram_suspend(struct device *dev);
static int __maybe_unused mtk_hwzram_resume(struct device *dev);

static int zram_engine_tbu_pm_get(struct smmu_tbu_device *tbu)
{
	struct zram_engine_t *hwz = dev_get_drvdata(tbu->dev);

	dev_info(tbu->dev, "%s:\n", __func__);

#ifndef FPGA_EMULATION
	WARN_ON(engine_gear_enable_clock(&hwz->ctrl, &hwz->gear_ctrl) != 0);
#endif

	/* hook to pm resume */
	return mtk_hwzram_resume(tbu->dev);
}

static int zram_engine_tbu_pm_put(struct smmu_tbu_device *tbu)
{
	struct zram_engine_t *hwz = dev_get_drvdata(tbu->dev);

	dev_info(tbu->dev, "%s:\n", __func__);

#ifndef FPGA_EMULATION
	engine_gear_disable_clock(&hwz->ctrl, &hwz->gear_ctrl);
#endif

	/* hook to pm suspend */
	return mtk_hwzram_suspend(tbu->dev);
}

static void zram_engine_tbu_debug_dump(struct smmu_tbu_device *tbu, struct seq_file *s)
{
	struct zram_engine_t *hwz = dev_get_drvdata(tbu->dev);

	/* TBU debug dump */
	dev_info(tbu->dev, "%s:\n", __func__);

	/*
	 * SMMU driver may not call pm get/put for tbu reg dump.
	 * PM get/put will be called only when accessing tbu registers directly by SMMU driver.
	 * Add clk/mtcmos on/off for the situations without pm get/put.
	 */
	if (zram_engine_tbu_pm_get(tbu)) {
		dev_info(tbu->dev, "%s: failed to enable clk/mtcmos\n", __func__);
		return;
	}

	engine_get_smmu_reg_dump(&hwz->ctrl, s);

	if (zram_engine_tbu_pm_put(tbu))
		dev_info(tbu->dev, "%s: failed to disable clk/mtcmos\n", __func__);
}

static struct smmu_tbu_impl zram_engine_tbu_impl = {
	.pm_get = zram_engine_tbu_pm_get,
	.pm_put = zram_engine_tbu_pm_put,
	.debug_dump = zram_engine_tbu_debug_dump,
};

static struct smmu_tbu_device zram_engine_tbu_device = {
	.impl = &zram_engine_tbu_impl,
};

static int zram_engine_tbu_register(struct device *dev, void __iomem *tbu_wp_base)
{
	int ret;

	if (!dev) {
		dev_info(dev, "%s: dev is NULL\n", __func__);
		return -1;
	}

	INIT_LIST_HEAD(&zram_engine_tbu_device.node);
	zram_engine_tbu_device.dev = dev;
	zram_engine_tbu_device.type = SOC_SMMU;
	zram_engine_tbu_device.tbu_base = tbu_wp_base;
	zram_engine_tbu_device.tbu_cnt = 1;

	ret = mtk_smmu_register_tbu(&zram_engine_tbu_device);
	dev_info(dev, "%s: register TBU device:%d\n", __func__, ret);

	return ret;
}

static void zram_engine_tbu_unregister(void)
{
	int ret;

	ret = mtk_smmu_unregister_tbu(&zram_engine_tbu_device);
	pr_info("%s: unregister TBU device:%d\n", __func__, ret);

	WARN_ON(ret != 0);
}

/* IRQ handler for compression */
static irqreturn_t comp_irq_handler(int irq, void *data)
{
	struct zram_engine_t *hwz = data;
	uint32_t status;
	uint64_t err_status;

	if (!hwz)
		return IRQ_HANDLED;

#ifdef ZRAM_ENGINE_DEBUG
	pr_info("%s\n", __func__);
#endif

	if (engine_irq_off(&hwz->ctrl)) {
#ifdef ZRAM_ENGINE_DEBUG
		pr_info("%s: irq is off!\n", __func__);
#endif
		return IRQ_HANDLED;
	}

	status = engine_get_enc_irq_status(&hwz->ctrl);

	if (status & (ZRAM_ENC_BATCH_INTR_MASK | ZRAM_ENC_IDLE_INTR_MASK))
		wake_up(&hwz->comp_wait);

	if (status & ZRAM_ENC_ERROR_INTR_MASK) {
		err_status = engine_get_enc_error_status(&hwz->ctrl);
		pr_info("%s: err(0x%llx)\n", __func__, err_status);

		/* TODO: how to handle */
	}

	return IRQ_HANDLED;
}

/* IRQ handler for decompression */
static irqreturn_t dcomp_irq_handler(int irq, void *data)
{
	struct zram_engine_t *hwz = data;
	uint32_t status;
	uint64_t err_status;

	if (!hwz)
		return IRQ_HANDLED;

#ifdef ZRAM_ENGINE_DEBUG
	pr_info("%s\n", __func__);
#endif

	if (engine_irq_off(&hwz->ctrl)) {
#ifdef ZRAM_ENGINE_DEBUG
		pr_info("%s: irq is off!\n", __func__);
#endif
		return IRQ_HANDLED;
	}

	status = engine_get_dec_irq_status(&hwz->ctrl);

	if (status & (ZRAM_DEC_BATCH_INTR_MASK | ZRAM_DEC_IDLE_INTR_MASK))
		wake_up(&hwz->dcomp_wait);

	if (status & ZRAM_DEC_ERROR_MASKS) {
		err_status = engine_get_dec_error_status(&hwz->ctrl);
		pr_info("%s: err(0x%llx)\n", __func__, err_status);

		/* TODO: how to handle */
	}

	return IRQ_HANDLED;
}

/*
 * Handle the following interrupts -
 * (Reliability, Availability, and Serviceability)
 * RAS CRIs - CRitical Error Interrupts
 * RAS ERIs - Error Recovery Interrupts
 * RAS FHIs - Fault Handling Interrupts
 */
static irqreturn_t smmu_irq_handler(int irq, void *data)
{
	struct zram_engine_t *hwz = data;
	//uint32_t status;

	pr_info("%s\n", __func__);

	if (!hwz)
		return IRQ_HANDLED;

	if (engine_irq_off(&hwz->ctrl)) {
#ifdef ZRAM_ENGINE_DEBUG
		pr_info("%s: irq is off!\n", __func__);
#endif
		return IRQ_HANDLED;
	}

	/* TODO: handle translation fault */

	return IRQ_HANDLED;
}

static struct engine_irq_t zram_engine_irqs[] = {
	{ .name = "zram_enc_irq", .handler = comp_irq_handler, .flags = 0, },
	{ .name = "zram_dec_irq", .handler = dcomp_irq_handler, .flags = 0, },
	{ .name = "zram_smmu_tbu_0_irq", .handler = smmu_irq_handler, .flags = IRQF_SHARED, },
	{ .name = "zram_smmu_tbu_1_irq", .handler = smmu_irq_handler, .flags = IRQF_SHARED, },
};

static int dump_fifo_idx(struct zram_engine_t *hwz, char *buf, int offset);

/* Check FIFO RTFF result and recover if necessary (only called during power on) */
void engine_self_check_before_kick(struct engine_control_t *ctrl)
{
	struct zram_engine_t *hwz = container_of(ctrl, struct zram_engine_t, ctrl);
	struct hwfifo *fifo;
	int i;
	uint32_t cmpl_idx;
	bool rtff_fail = false;

	if (!static_branch_unlikely(&engine_rtff_check))
		return;

	if (unlikely(!hwz))
		return;

	/*
	 * Check fifo status
	 */

	/* main compression fifo */
	fifo = &hwz->fifo_1;
	cmpl_idx = comp_fifo_1_HtS_complete_index(fifo);
	if (cmpl_idx != fifo->complete_idx) {
		rtff_fail = true;
		goto dump;
	}

	/* second compression fifo */
	fifo = &hwz->fifo_2;
	cmpl_idx = comp_fifo_2_HtS_complete_index(fifo);
	if (cmpl_idx != fifo->complete_idx) {
		rtff_fail = true;
		goto dump;
	}

	/* decompression fifos */
	for (i = 0; i < MAX_DCOMP_NR; i++) {
		fifo = &hwz->dcomp_fifo[i];
		cmpl_idx = dcomp_fifo_HtS_complete_index(fifo);
		if (cmpl_idx != fifo->complete_idx) {
			rtff_fail = true;
			goto dump;
		}
	}

dump:
	/* RTFF is successful */
	if (!rtff_fail) {
		atomic_inc(&engine_rtff_pass_count);
		return;
	}

	/*
	 * Dump for RTFF fail
	 */
	atomic_inc(&engine_rtff_fail_count);

	engine_get_reg_status(ctrl, NULL);
	dump_fifo_idx(hwz, NULL, 0);

	/*
	 * Try to restore
	 */

	/* main compression fifo */
	fifo = &hwz->fifo_1;
	cmpl_idx = comp_fifo_1_HtS_complete_index(fifo);
	if (cmpl_idx != fifo->complete_idx) {
		fifo->write_idx = cmpl_idx;
		fifo->complete_idx = cmpl_idx;
		fifo->pp_prev_end = cmpl_idx;
	}

	/* second compression fifo */
	fifo = &hwz->fifo_2;
	cmpl_idx = comp_fifo_2_HtS_complete_index(fifo);
	if (cmpl_idx != fifo->complete_idx) {
		fifo->write_idx = cmpl_idx;
		fifo->complete_idx = cmpl_idx;
		fifo->pp_prev_end = cmpl_idx;
	}

	/* decompression fifos */
	for (i = 0; i < MAX_DCOMP_NR; i++) {
		fifo = &hwz->dcomp_fifo[i];
		cmpl_idx = dcomp_fifo_HtS_complete_index(fifo);
		if (cmpl_idx != fifo->complete_idx) {
			fifo->write_idx = cmpl_idx;
			fifo->complete_idx = cmpl_idx;
			fifo->pp_prev_end = cmpl_idx;
		}
	}
}

/* Polling for cmd completion */
static inline void hwcomp_poll_cmd_complete(struct hwfifo *fifo)
{
	engine_poll_cmd_complete(fifo->write_idx_reg, fifo->complete_idx_reg);
}

/*
 * Pairs for allocate & release buffer for one entry
 */
void free_entry_buffer(uint32_t *buf_addr, int index)
{
	void *bufp;

	/* No buffer allocated for this entry */
	if (*buf_addr == 0)
		return;

	/* It's coherent. Just free it. */
	bufp = cmd_buf_addr_to_va(buf_addr);

#ifdef ZRAM_ENGINE_DEBUG
	pr_info("%s:%d:0x%llx:0x%x\n", __func__, index, (unsigned long long)bufp, *buf_addr);
#endif

	/* Tag is KASAN_TAG_KERNEL(0xff), so no tag match will be executed. */
	kmem_cache_free(size_allocator[index], bufp);
}

int refill_entry_buffer(uint32_t *buf_addr, int index, bool invalidate)
{
	void *bufp;

	/* Allocate memory from corresponding cache */
	bufp = kmem_cache_alloc(size_allocator[index], GFP_NOIO | __GFP_ZERO | __GFP_NOWARN);
	if (!bufp) {
		/* Mark it as zero to align the initial setting at fifo creation */
		*buf_addr = 0;
		return -1;
	}

	/*
	 * "bufp" will be translated to cmd format and kmemleak can't detect it properly.
	 * Mark it as ignore to avoid kmemleak scan.
	 */
	kmemleak_ignore(bufp);

#ifdef ZRAM_ENGINE_DEBUG
	pr_info("%s:%d:0x%llx:0x%llx:%d\n",
		__func__, index, (unsigned long long)bufp, virt_to_phys(bufp), (int)bufsz[index]);
#endif

	/* Convert it to the format in CMD */
	*buf_addr = PHYS_ADDR_TO_DST(virt_to_phys(bufp));

	/* Invalidate buffer if necessary */
	if (invalidate)
		flush_dcache((unsigned long)bufp,
				(unsigned long)bufp + bufsz[index]);

	return 0;
}

/*
 * Dedicated thread to adjust gear level.
 */
static int gear_level_adjustment(void *data)
{
	struct zram_engine_t *hwz = data;
	struct hwfifo *fifo;
	DEFINE_WAIT(wait);

	/* Default setting */
	SetGlaEncNotready(&gla_flags);

	/* Main loop for GLA thread */
	while (!kthread_should_stop()) {

		/* Wait for request notification */
		prepare_to_wait(&hwz->gla_wait, &wait, TASK_IDLE);
		if (!TestClearGlaNotify(&gla_flags) && !kthread_should_stop())
			schedule();
		finish_wait(&hwz->gla_wait, &wait);

		/* Should stop, just return. */
		if (kthread_should_stop())
			return 0;

		/* Request to set up default gear level for compression */
		if (TestClearGlaEncset(&gla_flags)) {
			while (!engine_try_to_set_gear_level(&hwz->gear_ctrl, true, ENGINE_ENC_MIN_KICK_GEAR));

			/* Gear set up is ready */
			ClearGlaEncNotready(&gla_flags);

			/* It's ok to kick compression */
			spin_lock(&hwz->fifo_2_lock);
			fifo = &hwz->fifo_2;
			engine_kick(fifo->write_idx_reg);
			spin_unlock(&hwz->fifo_2_lock);
		}

		/* Request to set up default gear level for decompression */
		if (TestClearGlaDecset(&gla_flags))
			while (!engine_try_to_set_gear_level(&hwz->gear_ctrl, false, ENGINE_DEC_MIN_KICK_GEAR));

		/* Request to start gear down for compression */
		if (TestClearGlaEncdone(&gla_flags)) {

			/* Set EncNotready to notify users should set gear if necessary */
			SetGlaEncNotready(&gla_flags);

			do {
				/* Sleep for a while */
				usleep_idle_range(50, 100);

				/* Incoming request to set up gear level for compression */
				if (GlaEncset(&gla_flags))
					break;

				/* Whether there are any pending requests */
				if (atomic_read(&hwz->comp_cnt) > 0)
					break;

				/* Demote frequency (may sleep) */
				if (engine_try_to_gear_down(&hwz->gear_ctrl, true))
					break;

			} while (1);
		}

		/* Request to start gear down for decompression */
		if (TestClearGlaDecdone(&gla_flags)) {
			do {
				/* Sleep for a while */
				usleep_idle_range(50, 100);

				/* Incoming request to set up gear level for decompression */
				if (GlaDecset(&gla_flags))
					break;

				/* Whether there are any pending requests */
				if (atomic_read(&hwz->dcomp_cnt) > 0)
					break;

				/* Demote frequency (may sleep) */
				if (engine_try_to_gear_down(&hwz->gear_ctrl, false))
					break;

			} while (1);
		}
	}

	return 0;
}

/* Called by dcomp_post_process */
static void dcomp_process_completed_cmd(struct zram_engine_t *hwz,
				struct hwfifo *fifo, uint32_t entry)
{
	struct decompress_cmd *cmdp = DCOMP_CMD(fifo, entry);
	int ret;

	/* "Sync cmd" is processed by decompress_page caller itself */
	if (decompress_cmd_sync(cmdp)) {

		/* Wait until the cmd is processed and back to IDLE */
		ret = poll_dcomp_cmd_status(cmdp, DCOMP_CMD_IDLE, 500);
		if (ret)
			pr_info("%s: timeout!\n", __func__);

		return;
	}

	hwz->ops->dcomp_process_completed_cmd(fifo, entry, true);
}

/* Return the number of processed dcomp cmds */
static uint32_t dcomp_post_processing_cmds(struct zram_engine_t *hwz, struct hwfifo *fifo)
{
	uint32_t start, end, index, entry;
	uint32_t fifo_processed = 0;

	start = fifo->complete_idx;
	end = dcomp_fifo_HtS_complete_index(fifo);

	/* Add memory barrier to make sure we can get the correct range */
	rmb();

	if (start != fifo->pp_prev_end)
		pr_info("%s: unexpected start(0x%x), not (0x%x)", __func__, start, fifo->pp_prev_end);

	for (index = start; index != end; index = (index + 1) & ENGINE_DCOMP_FIFO_ENTRY_CARRY_MASK) {
		entry = index & ENGINE_DCOMP_FIFO_ENTRY_MASK;
		dcomp_process_completed_cmd(hwz, fifo, entry);
		atomic_dec(&hwz->dcomp_cnt);
		update_dcomp_fifo_complete_index(fifo);
		fifo_processed++;
	}

	fifo->pp_prev_end = end;

	return fifo_processed;
}

/* Return the total number of processed fifo dcomp cmds */
static inline uint32_t dcomp_fifos_post_processing_cmds(struct zram_engine_t *hwz)
{
	struct hwfifo *fifo;
	int cpu;
	uint32_t processed = 0;

	/* Post-processing cmds */
	for (cpu = MAX_DCOMP_NR - 1; cpu >= 0; cpu--) {
		fifo = &hwz->dcomp_fifo[cpu];

		/*
		 * write_idx may be updated somewhere concurrently during post-process.
		 * Call smp_rmb() to make sure we can perceive the update here.
		 */
		smp_rmb();

		/* fifo is empty. Continue with the next one */
		if (dcomp_fifo_empty(fifo))
			continue;

		/* Start processing cmds */
		processed += dcomp_post_processing_cmds(hwz, fifo);
	}

	return processed;
}

#ifndef FPGA_EMULATION
static inline void dcomp_try_to_gear_down(struct zram_engine_t *hwz)
{
	/* Wake up gear level adjusting thread to start gear down for decompression */
	SetGlaDecdone(&gla_flags);
	SetGlaNotify(&gla_flags);
	wake_up(&hwz->gla_wait);
}
#else
#define dcomp_try_to_gear_down(hwz)	do { } while (0)
#endif

/* Handler when dcomp is not started successfully */
static void dcomp_hang_handle(struct zram_engine_t *hwz)
{
	struct hwfifo *fifo;
	int cpu;

	cpu = get_cpu();
	fifo = &hwz->dcomp_fifo[cpu];
	engine_kick(fifo->write_idx_reg);
	put_cpu();
}

/*
 * Decompression post-process
 */
static int dcomp_post_process(void *data)
{
	struct zram_engine_t *hwz = data;
	uint32_t processed, total_processed;
	uint32_t hang_detect, suspect_hang;
	int cnt;
	static bool warn_on_cnt_underflow = true;
	DEFINE_WAIT(wait);

	current->flags |= PF_MEMALLOC;

	while (!kthread_should_stop()) {

		/* No pending cmds */
		while (atomic_read(&hwz->dcomp_cnt) == 0) {
#ifdef ZRAM_ENGINE_DEBUG
			pr_info("%s: decompress fifos are empty.\n", __func__);
#endif
			/* Try to demote frequency (may sleep) */
			dcomp_try_to_gear_down(hwz);

			/* Try to sleep for next incoming request */
			prepare_to_wait(&hwz->dcomp_wait, &wait, TASK_IDLE);
			if (atomic_read(&hwz->dcomp_cnt) == 0 && !kthread_should_stop()) {
#ifdef ZRAM_ENGINE_DEBUG
				pr_info("%s: No newly finished request.\n", __func__);
#endif
				schedule();
			}
			finish_wait(&hwz->dcomp_wait, &wait);

			/* Should stop, just return. */
			if (kthread_should_stop())
				return 0;
		}

#ifndef FPGA_EMULATION
		WARN_ON(engine_gear_enable_clock_disable_irq(&hwz->ctrl, &hwz->gear_ctrl, false) != 0);
#endif

		/* Reset total_processed */
		total_processed = 0;

		/* Reset hang_detect */
		hang_detect = 0;

		/* Reset suspect_hang */
		suspect_hang = 0;

repeat:
		/* Processing cmds */
		processed = dcomp_fifos_post_processing_cmds(hwz);

		/* Update total_processed */
		total_processed += processed;

		/* HW is slow, just sleep for a while */
		if (processed == 0) {
			usleep_idle_range(50, 100);
			hang_detect++;
		} else {
			/* not hang */
			hang_detect = 0;
			suspect_hang = 0;
		}

		/* dcomp may hang. Try to kick it again. */
		if (hang_detect > HANG_DETECT_BOUND) {
			dcomp_hang_handle(hwz);
			hang_detect = 0;
			suspect_hang++;
		}

		/* dcomp hang. Try to dump more information & do something. */
		if (suspect_hang > SUSPECT_HANG_BOUND) {
			engine_get_reg_status(&hwz->ctrl, NULL);
			//engine_fatal_get_reg_status(&hwz->ctrl, NULL);
			dump_fifo_idx(hwz, NULL, 0);
			engine_gear_get_status(&hwz->gear_ctrl, NULL);

			/* do something */
			suspect_hang = 0;
			WARN_ON_ONCE(1);

			/* Increase the suspect hang count */
			atomic_inc(&dec_suspect_hang_count);
		}

		/* Repeat until all decompression cmds are processed */
		cnt = atomic_read(&hwz->dcomp_cnt);
		if (cnt > 0) {
			goto repeat;
		} else if (cnt < 0) {
			/* Show warning & dump information once to avoid log flooding */
			if (READ_ONCE(warn_on_cnt_underflow)) {
				WARN_ON(1);
				engine_get_reg_status(&hwz->ctrl, NULL);
				dump_fifo_idx(hwz, NULL, 0);
				engine_gear_get_status(&hwz->gear_ctrl, NULL);
				WRITE_ONCE(warn_on_cnt_underflow, false);
			}
		}

#ifndef FPGA_EMULATION
		/*
		 * Disable clock for zram engine -
		 * Paired with the one in dcomp_post_process. (decrease the ref count by "total_processed + 1")
		 */
		engine_gear_disable_clock_by_cnt(&hwz->ctrl, &hwz->gear_ctrl, total_processed + 1);
#endif
	}

	return 0;
}

/* Return the number of processed comp second cmds */
static inline uint32_t comp_second_post_processing_cmds(struct zram_engine_t *hwz, struct hwfifo *fifo)
{
	uint32_t start, end, index, entry;
	uint32_t processed = 0;

	/* Add memory barrier to make sure we can get the correct range */
	rmb();

	/* Acquire the range for post-processing */
	start = fifo->complete_idx;
	end = comp_fifo_2_HtS_complete_index(fifo);

	smp_rmb();

	/* I am empty, just leave. */
	if (comp_fifo_2_empty(fifo))
		return 0;

	/* The wake up is premature */
	if (start == end)
		return 0;

	/* Start post-processing cmds */
	if (start != fifo->pp_prev_end)
		pr_info("%s: unexpected start(0x%x), not (0x%x)", __func__, start, fifo->pp_prev_end);

	for (index = start; index != end; index = (index + 1) & ENGINE_COMP_FIFO_2_ENTRY_CARRY_MASK) {
		entry = index & ENGINE_COMP_FIFO_2_ENTRY_MASK;
		hwz->ops->comp_process_completed_cmd(fifo, entry);
		atomic_dec(&hwz->comp_cnt);
		update_comp_fifo_2_complete_index(fifo);
		processed++;
	}

	fifo->pp_prev_end = end;

	return processed;
}

/* Return the number of processed comp main cmds */
static inline uint32_t comp_main_post_processing_cmds(struct zram_engine_t *hwz, struct hwfifo *fifo)
{
	uint32_t start, end, index, entry;
	uint32_t processed = 0;

	/* Add memory barrier to make sure we can get the correct range */
	rmb();

	/* Acquire the range for post-processing */
	start = fifo->complete_idx;
	end = comp_fifo_1_HtS_complete_index(fifo);

	smp_rmb();

	/* I am empty, just leave. */
	if (comp_fifo_1_empty(fifo))
		return 0;

	/* The wake up is premature */
	if (start == end)
		return 0;

	/* Start post-processing cmds */
	if (start != fifo->pp_prev_end)
		pr_info("%s: unexpected start(0x%x), not (0x%x)", __func__, start, fifo->pp_prev_end);

	for (index = start; index != end; index = (index + 1) & ENGINE_COMP_FIFO_1_ENTRY_CARRY_MASK) {
		entry = index & ENGINE_COMP_FIFO_1_ENTRY_MASK;
		hwz->ops->comp_process_completed_cmd(fifo, entry);
		atomic_dec(&hwz->comp_cnt);
		update_comp_fifo_1_complete_index(fifo);
		processed++;
	}

	fifo->pp_prev_end = end;

	return processed;
}

#ifndef FPGA_EMULATION
static inline void comp_try_to_gear_down(struct zram_engine_t *hwz)
{
	/* Wake up gear level adjusting thread to start gear down for compression */
	SetGlaEncdone(&gla_flags);
	SetGlaNotify(&gla_flags);
	wake_up(&hwz->gla_wait);
}
#else
#define comp_try_to_gear_down(hwz)	do { } while (0)
#endif

/* Handler when comp is not started successfully */
static void comp_hang_handle(struct zram_engine_t *hwz)
{
	struct hwfifo *fifo;

	spin_lock(&hwz->fifo_2_lock);
	fifo = &hwz->fifo_2;
	engine_kick(fifo->write_idx_reg);
	spin_unlock(&hwz->fifo_2_lock);
}

/* Handler (with engine reset) when comp is not started successfully */
static void comp_hang_handle_with_reset(struct zram_engine_t *hwz)
{
	struct hwfifo *fifo;
	uint32_t monitor_widx;
	uint32_t start, end, index, entry;
	struct compress_cmd *cmdp;
	struct comp_pp_info *pp_info;

	/*
	 * Set atomic flag to notify we are handling reset and take a nap.
	 * In the meantime, kswapd may being adding new request or trying slowpath.
	 * Concurent requests from trying slowpath can be avoided by the fifo_2_lock.
	 * But for concurrent ones from kswapd adding new request to main fifo and to
	 * avoid unnecessary overhead in kswapd, we can try a longer nap to monitor
	 * the change of write index.
	 */
	SetGlaEncRst(&gla_flags);
	fifo = &hwz->fifo_1;

retry:
	monitor_widx = fifo->write_idx;
	usleep_idle_range(5000, 10000);

	/*
	 * Before doing reset, fifo_2_lock should be acquired to avoid
	 * other concurrent compression requests.
	 */
	spin_lock(&hwz->fifo_2_lock);

	if (fifo->write_idx != monitor_widx) {
		pr_info("%s: main fifo busy...\n", __func__);
		spin_unlock(&hwz->fifo_2_lock);
		goto retry;
	}

	/* Do warm reset */
	engine_enc_reset(&hwz->ctrl);
	udelay(1);

	/* Check the range for reset */

	/* main fifo */
	if (!comp_fifo_1_empty(fifo)) {

		start = comp_fifo_1_HtS_complete_index(fifo);
		end = fifo->write_idx;
		fifo->write_idx = start;

		/* Show information */
		pr_info("%s: main fifo pending cmds (0x%-4x)~(0x%-4x)\n", __func__, start, end);

		/* Send cmds again */
		for (index = start; index != end; index = (index + 1) & ENGINE_COMP_FIFO_1_ENTRY_CARRY_MASK) {
			entry = index & ENGINE_COMP_FIFO_1_ENTRY_MASK;
			cmdp = COMP_CMD(fifo, entry);
			pp_info = COMP_CMPL(fifo, entry);
			pr_info("%s: index(%u)\n", __func__, pp_info->index);
			set_comp_cmd_as_idle(cmdp);
			hwz->ops->fill_compression_info(fifo, entry, pp_info->page, pp_info, false);
			update_comp_fifo_1_write_index(fifo);
		}
	}

	/* second fifo */
	fifo = &hwz->fifo_2;
	if (!comp_fifo_2_empty(fifo)) {

		start = comp_fifo_2_HtS_complete_index(fifo);
		end = fifo->write_idx;
		fifo->write_idx = start;

		/* Show information */
		pr_info("%s: second fifo pending cmds (0x%-4x)~(0x%-4x)\n", __func__, start, end);

		/* Send cmds again */
		for (index = start; index != end; index = (index + 1) & ENGINE_COMP_FIFO_2_ENTRY_CARRY_MASK) {
			entry = index & ENGINE_COMP_FIFO_2_ENTRY_MASK;
			cmdp = COMP_CMD(fifo, entry);
			pp_info = COMP_CMPL(fifo, entry);
			pr_info("%s: index(%u)\n", __func__, pp_info->index);
			set_comp_cmd_as_idle(cmdp);
			hwz->ops->fill_compression_info(fifo, entry, pp_info->page, pp_info, false);
			update_comp_fifo_2_write_index(fifo);
		}
	}

	/* Unlock */
	spin_unlock(&hwz->fifo_2_lock);

	/* Reset is done */
	ClearGlaEncRst(&gla_flags);
}

/* Handler when comp is not started successfully */
#if 0
static void dump_pending_comp_cmds(struct zram_engine_t *hwz)
{
	struct hwfifo *fifo;
	uint32_t start, end, index, entry;

	/* main fifo */
	fifo = &hwz->fifo_1;
	start = comp_fifo_1_HtS_complete_index(fifo);
	end = fifo->write_idx;

	if (start != end)
		pr_info("%s: main fifo pending cmds (0x%-4x)~(0x%-4x)\n", __func__, start, end);

	for (index = start; index != end; index = (index + 1) & ENGINE_COMP_FIFO_1_ENTRY_CARRY_MASK) {
		entry = index & ENGINE_COMP_FIFO_1_ENTRY_MASK;
		dump_comp_cmd(COMP_CMD(fifo, entry));
	}

	/* second fifo */
	fifo = &hwz->fifo_2;
	start = comp_fifo_2_HtS_complete_index(fifo);
	end = fifo->write_idx;

	if (start != end)
		pr_info("%s: second fifo pending cmds (0x%-4x)~(0x%-4x)\n", __func__, start, end);

	for (index = start; index != end; index = (index + 1) & ENGINE_COMP_FIFO_2_ENTRY_CARRY_MASK) {
		entry = index & ENGINE_COMP_FIFO_2_ENTRY_MASK;
		dump_comp_cmd(COMP_CMD(fifo, entry));
	}
}
#endif

/*
 * Compression post-process
 */
static int comp_post_process(void *data)
{
	struct zram_engine_t *hwz = data;
	unsigned long pflags;
	uint32_t processed, total_processed;
	uint32_t hang_detect, suspect_hang;
	int cnt;
	DEFINE_WAIT(wait);

	current->flags |= PF_MEMALLOC;

	while (!kthread_should_stop()) {

		/* No pending cmds */
		while (atomic_read(&hwz->comp_cnt) == 0) {
#ifdef ZRAM_ENGINE_DEBUG
			pr_info("%s: fifo is empty!!!!!\n", __func__);
#endif
			/* Try to demote frequency (may sleep) */
			comp_try_to_gear_down(hwz);

			/* Try to sleep for next incoming request */
			prepare_to_wait(&hwz->comp_wait, &wait, TASK_IDLE);
			if (atomic_read(&hwz->comp_cnt) == 0 && !kthread_should_stop()) {
#ifdef ZRAM_ENGINE_DEBUG
				pr_info("%s: No newly finished request.\n", __func__);
#endif
				schedule();
			}
			finish_wait(&hwz->comp_wait, &wait);

			/* Should stop, just return. */
			if (kthread_should_stop())
				return 0;
		}

#ifndef FPGA_EMULATION
		WARN_ON(engine_gear_enable_clock_disable_irq(&hwz->ctrl, &hwz->gear_ctrl, true) != 0);
#endif

		/* Start of memory stall section - it's ok for current usage as zram swap. */
		psi_memstall_enter(&pflags);

		/* Reset total_processed */
		total_processed = 0;

		/* Reset hang_detect */
		hang_detect = 0;

		/* Reset suspect_hang */
		suspect_hang = 0;

repeat:
		/* Processing main fifo cmds */
		processed = comp_main_post_processing_cmds(hwz, &hwz->fifo_1);

		/* Processing second fifo cmds */
		processed += comp_second_post_processing_cmds(hwz, &hwz->fifo_2);

		/* Update total_processed */
		total_processed += processed;

		/* HW is slow, just sleep for a while */
		if (processed == 0) {
			usleep_idle_range(50, 100);

			/* Not hang, don't count it as hang */
			if (!GlaEncNotready(&gla_flags))
				hang_detect++;
		} else {
			/* not hang */
			hang_detect = 0;
			suspect_hang = 0;
		}

		/* comp may hang. Try to kick it again. */
		if (hang_detect > HANG_DETECT_BOUND) {
			comp_hang_handle(hwz);
			hang_detect = 0;
			suspect_hang++;
		}

		/* comp hang. Try to dump more information & do something. */
		if (suspect_hang > SUSPECT_HANG_BOUND) {
			engine_get_reg_status(&hwz->ctrl, NULL);
			//engine_fatal_get_reg_status(&hwz->ctrl, NULL);
			dump_fifo_idx(hwz, NULL, 0);
			engine_gear_get_status(&hwz->gear_ctrl, NULL);

			/* do something */
			//dump_pending_comp_cmds(hwz);
			suspect_hang = 0;
			WARN_ON_ONCE(1);

			/* Increase the suspect hang count */
			atomic_inc(&enc_suspect_hang_count);

			/* Try to warm reset engine */
			comp_hang_handle_with_reset(hwz);
		}

		/* Repeat until all compression cmds are processed */
		cnt = atomic_read(&hwz->comp_cnt);
		if (cnt > 0) {
			goto repeat;
		} else if (cnt < 0) {
			WARN_ON_ONCE(1);
			engine_get_reg_status(&hwz->ctrl, NULL);
			dump_fifo_idx(hwz, NULL, 0);
			engine_gear_get_status(&hwz->gear_ctrl, NULL);
		}

		/* End of memory stall section */
		psi_memstall_leave(&pflags);

#ifndef FPGA_EMULATION
		/*
		 * Disable clock for zram engine -
		 * Paired with the one in comp_post_process. (decrease the ref count by "total_processed + 1")
		 */
		engine_gear_disable_clock_by_cnt(&hwz->ctrl, &hwz->gear_ctrl, total_processed + 1);
#endif
	}

	return 0;
}

/* Main entry for hwcomp_decompress_page in sync */
static inline int __hwcomp_decompress_page_sync(void *hw, void *src, unsigned int slen,
				struct page *page, struct dcomp_pp_info *pp_info,
				zspool_to_hwcomp_buffer_fn zspool_to_hwcomp_buffer,
				bool from_async)
{
	struct zram_engine_t *hwz = hw;
	struct hwfifo *fifo;
	uint32_t entry;
	int cpu;
	bool valid = false;

	if (!hwz)
		return -EINVAL;

	/* Support 4KB page only (16KB:TODO) */
	if (PAGE_SIZE != SZ_4K)
		return -EIO;

#ifdef ZRAM_ENGINE_DEBUG
	pr_info("%s\n", __func__);
#endif

#ifndef FPGA_EMULATION
retry:
#endif

	cpu = get_cpu();
	fifo = &hwz->dcomp_fifo[cpu];

next_dcmd:

	/*
	 * complete_idx may be updated somewhere concurrently during post-process.
	 * Call smp_rmb() to make sure we can perceive the update here.
	 */
	smp_rmb();

	/* Return directly if fifo is full */
	if (dcomp_fifo_full(fifo)) {
		put_cpu();

#ifndef FPGA_EMULATION
		/* Promote frequency. Retry if gear up successfully (may sleep) */
		if (engine_try_to_gear_up(&hwz->gear_ctrl, false))
			goto retry;
#endif

		return -EBUSY;
	}

	/*
	 * Increment the usage count and enable the gear clock
	 * if possible before committing the request.
	 */
#ifndef FPGA_EMULATION
	WARN_ON(engine_gear_enable_clock(&hwz->ctrl, &hwz->gear_ctrl) != 0);
#endif

	/* Increment the number of decompression request */
	atomic_inc(&hwz->dcomp_cnt);

	/* Query entry and fill request */
	entry = dcomp_fifo_write_entry(fifo);
	valid = hwz->ops->fill_decompression_info(fifo, entry, src, slen, page, pp_info,
						from_async, zspool_to_hwcomp_buffer);

	update_dcomp_fifo_write_index(fifo);

	/* Try next cmd if necessary */
	if (!valid)
		goto next_dcmd;

	/* Polling for cmd completion */
	hwcomp_poll_cmd_complete(fifo);

	/* Process the cmd after decompression */
	hwz->ops->dcomp_process_completed_cmd(fifo, entry, from_async);

	/*
	 * CMD finish or idle interrupts for decompression will be disabled when engine
	 * is in this mode. So callers need to update complete index by themselves.
	 */
	if (engine_async_mode_disabled() || engine_coherence_disabled()) {

		/* Decrement the number of decompression request */
		atomic_dec(&hwz->dcomp_cnt);

		update_dcomp_fifo_complete_index(fifo);

		/*
		 * Decrement the usage count and disable the gear clock
		 * if possible after finishing the request.
		 * It's safe trying to disable clock here because no more
		 * HW register access per request now.
		 */
#ifndef FPGA_EMULATION
		engine_gear_disable_clock(&hwz->ctrl, &hwz->gear_ctrl);
#endif
	}

	put_cpu();
	return 0;
}

/* Compress one page (4KB only) - synchronous */
int hwcomp_decompress_page_sync(void *hw, void *src, unsigned int slen, struct page *page,
			struct dcomp_pp_info *pp_info,
			zspool_to_hwcomp_buffer_fn zspool_to_hwcomp_buffer)
{
	return __hwcomp_decompress_page_sync(hw, src, slen, page, pp_info,
					zspool_to_hwcomp_buffer, false);

}
EXPORT_SYMBOL(hwcomp_decompress_page_sync);

/* Compress one page (4KB only) - asynchronous */
int hwcomp_decompress_page(void *hw, void *src, unsigned int slen, struct page *page,
			struct dcomp_pp_info *pp_info,
			zspool_to_hwcomp_buffer_fn zspool_to_hwcomp_buffer)
{
	struct zram_engine_t *hwz = hw;
	struct hwfifo *fifo;
	uint32_t entry;
	int cpu;
	bool valid = false;
	bool wake_up_pp = false;

	/*
	 * Using sync mode when user indicates or engine has no coherence support -
	 * CMD finish or idle interrupts for decompression will be disabled when engine
	 * is in this mode. So callers need to update complete index and do the remaining
	 * post-processing. (i.e redirecting to hwcomp_decompress_page_sync)
	 */
	if (engine_async_mode_disabled() || engine_coherence_disabled())
		return __hwcomp_decompress_page_sync(hw, src, slen, page, pp_info,
						zspool_to_hwcomp_buffer, true);

	if (!hwz)
		return -EINVAL;

	/* Support 4KB page only (16KB:TODO) */
	if (PAGE_SIZE != SZ_4K)
		return -EIO;

#ifdef ZRAM_ENGINE_DEBUG
	pr_info("%s\n", __func__);
#endif

#ifndef FPGA_EMULATION
retry:
#endif

	cpu = get_cpu();
	fifo = &hwz->dcomp_fifo[cpu];

next_dcmd:

	/*
	 * complete_idx may be updated somewhere concurrently during post-process.
	 * Call smp_rmb() to make sure we can perceive the update here.
	 */
	smp_rmb();

	/* Return directly if fifo is full */
	if (dcomp_fifo_full(fifo)) {
		put_cpu();

		/* Post-process may not start yet */
		wake_up(&hwz->dcomp_wait);

#ifndef FPGA_EMULATION
		/* Promote frequency. Retry if gear up successfully (may sleep) */
		if (engine_try_to_gear_up(&hwz->gear_ctrl, false))
			goto retry;
#endif

		return -EBUSY;
	}

	/*
	 * Increment the usage count and enable the gear clock
	 * if possible before committing the request.
	 */
#ifndef FPGA_EMULATION
	WARN_ON(engine_gear_enable_clock(&hwz->ctrl, &hwz->gear_ctrl) != 0);
#endif

	/* Query entry and fill request */
	entry = dcomp_fifo_write_entry(fifo);
	valid = hwz->ops->fill_decompression_info(fifo, entry, src, slen, page, pp_info,
						true, zspool_to_hwcomp_buffer);
	update_dcomp_fifo_write_index(fifo);

	/*
	 * Increment the number of decompression request after update hw write index
	 * and remember to wake up post-process.
	 */
	if (atomic_inc_return(&hwz->dcomp_cnt) == 1)
		wake_up_pp = true;

	/* Try next cmd if necessary */
	if (!valid)
		goto next_dcmd;

	put_cpu();

	/* Wake up dcomp_post_process */
	if (wake_up_pp)
		wake_up(&hwz->dcomp_wait);

	return 0;
}
EXPORT_SYMBOL(hwcomp_decompress_page);

/* Compress one page (4KB only) */
int hwcomp_compress_page(void *hw, struct page *page, struct comp_pp_info *pp_info)
{
	struct zram_engine_t *hwz = hw;
	struct hwfifo *fifo;
	uint32_t entry;
	bool valid = false;
	bool wake_up_pp = false;

	if (!hwz)
		return -EINVAL;

	/* Support 4KB page only (16KB:TODO) */
	if (PAGE_SIZE != SZ_4K)
		return -EIO;

#ifdef ZRAM_ENGINE_DEBUG
	pr_info("%s\n", __func__);
#endif

#ifndef FPGA_EMULATION
retry:
#endif

	/* Engine is doing reset, don't bother it */
	if (GlaEncRst(&gla_flags))
		return -EBUSY;

	/* Lockless fifo for kswapd only */
	if (current_is_kswapd()) {
		fifo = &hwz->fifo_1;

next_cmd_fifo_1:

		/*
		 * complete_idx may be updated somewhere concurrently during post-process.
		 * Call smp_rmb() to make sure we can perceive the update here.
		 */
		smp_rmb();

		/* Fallback to slow path if fifo full */
		if (comp_fifo_1_full(fifo)) {

			/* Post-process may not start yet */
			wake_up(&hwz->comp_wait);

			goto slowpath;
		}

		/*
		 * Increment the usage count and enable the gear clock
		 * if possible before committing the request.
		 */
#ifndef FPGA_EMULATION
		WARN_ON(engine_gear_enable_clock(&hwz->ctrl, &hwz->gear_ctrl) != 0);
#endif

		/* Increment the number of compression request and remember to wake up post-process. */
		if (atomic_inc_return(&hwz->comp_cnt) == 1)
			wake_up_pp = true;

		/* Query entry and fill request */
		entry = comp_fifo_1_write_entry(fifo);
		valid = hwz->ops->fill_compression_info(fifo, entry, page, pp_info, true);
		if (GlaEncNotready(&gla_flags)) {

			/* Wake up gear level adjusting thread to set up default gear if necessary */
			if (!GlaEncset(&gla_flags)) {
				SetGlaEncset(&gla_flags);
				SetGlaNotify(&gla_flags);
				wake_up(&hwz->gla_wait);
			}

			/* Update write index without kick */
			update_comp_fifo_1_write_index_nokick(fifo);
		} else {
			update_comp_fifo_1_write_index(fifo);
		}

		/* Try next cmd if necessary */
		if (!valid)
			goto next_cmd_fifo_1;

		/* Using sync mode */
		if (engine_async_mode_disabled()) {
			hwcomp_poll_cmd_complete(fifo);
			wake_up(&hwz->comp_wait);
		}

		/* Request is sent, just leave. */
		goto exit;
	}

slowpath:

	/* For other callers */
	spin_lock(&hwz->fifo_2_lock);
	fifo = &hwz->fifo_2;

next_cmd_fifo_2:

	/*
	 * complete_idx may be updated somewhere concurrently during post-process.
	 * Call smp_rmb() to make sure we can perceive the update here.
	 */
	smp_rmb();

	/* If fifo is full, then return -EBUSY */
	if (comp_fifo_2_full(fifo)) {
		spin_unlock(&hwz->fifo_2_lock);

		/* Post-process may not start yet */
		wake_up(&hwz->comp_wait);

#ifndef FPGA_EMULATION
		/* Promote frequency. Retry if gear up successfully (may sleep) */
		if (engine_try_to_gear_up(&hwz->gear_ctrl, true))
			goto retry;
#endif

		return -EBUSY;
	}

	/*
	 * Increment the usage count and enable the gear clock
	 * if possible before committing the request.
	 */
#ifndef FPGA_EMULATION
	WARN_ON(engine_gear_enable_clock(&hwz->ctrl, &hwz->gear_ctrl) != 0);
#endif

	/* Increment the number of compression request and remember to wake up post-process. */
	if (atomic_inc_return(&hwz->comp_cnt) == 1)
		wake_up_pp = true;

	/* Query entry and fill request */
	entry = comp_fifo_2_write_entry(fifo);
	valid = hwz->ops->fill_compression_info(fifo, entry, page, pp_info, true);
	if (GlaEncNotready(&gla_flags)) {

		/* Wake up gear level adjusting thread to set up default gear if necessary */
		if (!GlaEncset(&gla_flags)) {
			SetGlaEncset(&gla_flags);
			SetGlaNotify(&gla_flags);
			wake_up(&hwz->gla_wait);
		}

		/* Update write index without kick */
		update_comp_fifo_2_write_index_nokick(fifo);
	} else {
		update_comp_fifo_2_write_index(fifo);
	}

	/* Try next cmd if necessary */
	if (!valid)
		goto next_cmd_fifo_2;

	/* Using sync mode */
	if (engine_async_mode_disabled()) {
		hwcomp_poll_cmd_complete(fifo);
		wake_up(&hwz->comp_wait);
	}

	spin_unlock(&hwz->fifo_2_lock);

exit:
	/* Wake up comp_post_process */
	if (wake_up_pp)
		wake_up(&hwz->comp_wait);

	return 0;
}
EXPORT_SYMBOL(hwcomp_compress_page);

/**************************************************/

static const struct of_device_id mtk_hwzram_of_match[] = {
	{ .compatible = "mediatek,mtk-hwzram", },
	{ }
};
MODULE_DEVICE_TABLE(of, mtk_hwzram_of_match);

/* Uninitialize platform resource for zram engine */
static void zram_engine_platform_deinit(struct platform_device *pdev, struct zram_engine_t *hwz)
{
	/* Unregister TBU monitor before disable clock & put runtime pm */
	zram_engine_tbu_unregister();

#ifndef FPGA_EMULATION
	/*
	 * Disable clock for zram engine -
	 * Paired with the one in zram_engine_platform_init.
	 */
	engine_gear_disable_clock(&hwz->ctrl, &hwz->gear_ctrl);
#endif

	/* Power off zram engine & remove the support of kernel PM */
	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	/* Clear drvdata */
	platform_set_drvdata(pdev, NULL);

	/* Release platform resource */
	engine_free_interrupts(pdev, &hwz->ctrl, zram_engine_irqs, ARRAY_SIZE(zram_engine_irqs));
	engine_smmu_destroy(pdev, &hwz->ctrl);
#ifndef FPGA_EMULATION
	engine_gear_deinit(pdev, &hwz->gear_ctrl);
#endif
	engine_control_deinit(pdev, &hwz->ctrl);
}

/*
 * Initialize platform resource for zram engine -
 * Platform clock for zram engine will be enabled after successful return.
 */
static int zram_engine_platform_init(struct platform_device *pdev, struct zram_engine_t *hwz)
{
	int ret;
	int i;

	if (!pdev || !hwz)
		return -ENODEV;

	ret = engine_control_init(pdev, &hwz->ctrl);
	if (ret) {
		pr_info("%s: engine_control_init fail: (%d)\n", __func__, ret);
		return ret;
	}

#ifndef FPGA_EMULATION
	ret = engine_gear_init(pdev, &hwz->gear_ctrl);
	if (ret) {
		pr_info("%s: engine_gear_init fail: (%d)\n", __func__, ret);
		return ret;
	}
#endif

	/* For LDVT stress with SMMU S1. Should be removed later? (TODO) */
	ret = engine_smmu_setup(pdev, &hwz->ctrl);
	if (ret) {
		pr_info("%s: engine_smmu_setup fail: (%d)\n", __func__, ret);
		return ret;
	}

	/* Prepare the information of interrupt handlers */
	for (i = 0; i < ARRAY_SIZE(zram_engine_irqs); i++)
		zram_engine_irqs[i].priv = hwz;

	/* Setup interrupts */
	ret = engine_request_interrupts(pdev, &hwz->ctrl, zram_engine_irqs, ARRAY_SIZE(zram_engine_irqs));
	if (ret) {
		pr_info("%s: engine_request_interrupts fail: (%d)\n", __func__, ret);
		return ret;
	}

	/* Setup of platform resource is completed. Associate hwz to pdev */
	platform_set_drvdata(pdev, hwz);

	/* Initialize for kernel PM & power on zram engine */
	pm_runtime_enable(&pdev->dev);
	ret = pm_runtime_get_sync(&pdev->dev);
	if (ret) {
		pr_info("%s: failed to do initialization for kernel PM .\n", __func__);
		return ret;
	}

#ifndef FPGA_EMULATION
	/* Enable clock for zram engine */
	ret = engine_gear_enable_clock(&hwz->ctrl, &hwz->gear_ctrl);
	if (ret) {
		pr_info("%s: engine_gear_enable_clock fail: (%d)\n", __func__, ret);
		return ret;
	}
#endif

	/*
	 * Power & clock is prepared. It's safe to start engine initialization.
	 */

	/* Register TBU monitor after clock enable & runtime pm get */
	ret = zram_engine_tbu_register(&pdev->dev, hwz->ctrl.zram_smmu_base);
	if (ret) {
		pr_info("%s: zram_engine_tbu_register fail: (%d)\n", __func__, ret);
		return ret;
	}

	return 0;
}

/* Uninitialize zram HW engine. */
static void zram_engine_hw_deinit(struct zram_engine_t *hwz)
{
	/* Nothing to do */
}

/* Initialize zram HW engine. Return 0 if success */
static int zram_engine_hw_init(struct zram_engine_t *hwz)
{
	int ret;

	if (!hwz)
		return -ENODEV;

	/* Configure zram engine clock */
	ret = engine_clock_init(&hwz->ctrl);
	if (ret) {
		pr_info("%s: engine_clock_init fail (%d)\n", __func__, ret);
		return ret;
	}

	/* Initialize compression & decompression modules */
	engine_enc_init(&hwz->ctrl, (hwz->ops == &engine_dc_ops));
	engine_dec_init(&hwz->ctrl, (hwz->ops == &engine_dc_ops));

	return 0;
}

/* Destroy size allocator */
static void destroy_size_allocator(void)
{
	int i;

	for (i = 0; i < ENGINE_NUM_OF_BUF_SIZES; i++)
		kmem_cache_destroy(size_allocator[i]);
}

/* Create size allocator for dst buffers in compress cmd */
static int create_size_allocator(void)
{
	int i;

	/*
	 * [0] for 2048, [1] for 1024, [2] for 512, [3] for 256, [4] for 128, [5] for 64
	 * [6] for 4096
	 */
	for (i = 0; i < ENGINE_NUM_OF_BUF_SIZES; i++) {
		char name[16];
		unsigned int size = ENGINE_NO_DST_COPY_MAX_BUFSZ >> i;
		slab_flags_t flags = SLAB_HWCACHE_ALIGN;

		/* Set size for the last buffer */
		if (i == (ENGINE_NUM_OF_BUF_SIZES - 1))
			size = ENGINE_DST_COPY_MAX_BUFSZ;

		/* Any debug flags? (TODO) */

		snprintf(name, 16, "hwz-size-%u", size);
		size_allocator[i] = kmem_cache_create(name, size, size, flags , NULL);

		if (!size_allocator[i]) {
			pr_info("%s: unable to create alloctor for size %u\n", __func__, size);
			destroy_size_allocator();
			return -ENOMEM;
		}

		pr_info("%s: allocator (%s) is created.\n", __func__, name);
	}

	return 0;
}

/* Release fifo & completion memory */
static void destroy_fifo_memory(struct hwfifo *fifo)
{
	kfree(fifo->buf);
	fifo->buf = NULL;
	kfree(fifo->completion);
	fifo->completion = NULL;
}

/* Allocate memory for fifo & completion structure */
static int create_fifo_memory(struct hwfifo *fifo, uint32_t fifo_size, uint32_t cmd_size, uint32_t cmpl_size)
{
	int ret = 0;
	void *fifo_buf;
	void *cmpl_buf;

	if (!fifo)
		return -EINVAL;

	fifo->size = fifo_size;
	fifo_buf = kzalloc(round_up(fifo->size * cmd_size, SZ_4K), GFP_KERNEL);
	if (!fifo_buf) {
		pr_info("%s: failed to allocate fifo memory.\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}

	/* fifo buffer must be cmd_size aligned */
	if (!IS_ALIGNED((unsigned long)fifo_buf, cmd_size)) {
		pr_info("%s: fifo memory is not aligned properly.\n", __func__);
		ret = -ENOMEM;
		goto free_fifo;
	}
	fifo->buf = fifo_buf;
	fifo->buf_pa = virt_to_phys(fifo_buf);

	cmpl_buf = kzalloc(fifo->size * cmpl_size, GFP_KERNEL);
	if (!cmpl_buf) {
		pr_info("%s: failed to allocate memory for completion.\n", __func__);
		ret = -ENOMEM;
		goto free_fifo;
	}
	fifo->completion = cmpl_buf;

#ifdef ZRAM_ENGINE_DEBUG
	pr_info("%s: fifo_va(%llx) fifo_pa(%llx)\n", __func__, (uint64_t)fifo->buf, (uint64_t)fifo->buf_pa);
#endif

	return 0;

free_fifo:
	kfree(fifo_buf);
	fifo->buf = NULL;

exit:
	return ret;
}

static void zram_engine_destroy_fifos(struct zram_engine_t *hwz)
{
	int i;

	/* Release all allocated buffers for fifo_1 */
	hwz->ops->release_comp_fifo_dst_buffers(&hwz->fifo_1);

	/* Destroy compression fifo#1 */
	destroy_fifo_memory(&hwz->fifo_1);

	/* Release all allocated buffers for fifo_2 */
	hwz->ops->release_comp_fifo_dst_buffers(&hwz->fifo_2);

	/* Destroy compression fifo#2 */
	destroy_fifo_memory(&hwz->fifo_2);

	/* Release all allocated buffers for dcomp_fifo# */
	for (i = 0; i < MAX_DCOMP_NR; i++) {
		hwz->ops->release_dcomp_fifo_src_buffers(&hwz->dcomp_fifo[i]);
		destroy_fifo_memory(&hwz->dcomp_fifo[i]);
	}
}

static int zram_engine_setup_fifos(struct zram_engine_t *hwz, bool reset_idx)
{
	struct hwfifo *fifo;
	int ret = 0, i, j;

	/*
	 * Compression FIFO #1
	 */

	/* Initialize compression fifo for kswapd only */
	fifo = &hwz->fifo_1;
	ret = create_fifo_memory(fifo, 1 << ENGINE_COMP_FIFO_1_ENTRY_BITS,
			ENGINE_COMP_CMD_SIZE, sizeof(struct comp_pp_info));
	if (ret) {
		pr_info("%s: failed to prepare fifo_1: (%d)\n", __func__, ret);
		ret = -ENOMEM;
		goto exit;
	}

	if (reset_idx) {
		fifo->write_idx = 0;
		fifo->complete_idx = 0;
		fifo->pp_prev_end = 0;
		fifo->write_idx_reg = hwz->ctrl.zram_enc_base +
			ZRAM_ENC_CMD_MAIN_FIFO_WRITE_INDEX;
		fifo->complete_idx_reg = hwz->ctrl.zram_enc_base +
			ZRAM_ENC_CMD_MAIN_FIFO_COMPLETE_INDEX;
	}

	/* Allocate dst buffers for every fifo cmds & set fifo ID */
	ret = hwz->ops->fill_comp_fifo_dst_buffers(fifo, ENGINE_COMP_FIFO_1_ID);
	if (ret) {
		pr_info("%s: failed to prepare cmd dst buffers for fifo_1: (%d)\n", __func__, ret);
		ret = -ENOMEM;
		goto free_fifo_1;
	}

	/* Setup registers for compression fifo #1 */
	engine_setup_enc_main_fifo(&hwz->ctrl, fifo->buf_pa, ENGINE_COMP_FIFO_1_ENTRY_BITS);

	/*
	 * Compression FIFO #2
	 */

	/* Initialize compression fifo & lock for others */
	fifo = &hwz->fifo_2;
	ret = create_fifo_memory(fifo, 1 << ENGINE_COMP_FIFO_2_ENTRY_BITS,
			ENGINE_COMP_CMD_SIZE, sizeof(struct comp_pp_info));
	if (ret) {
		pr_info("%s: failed to prepare fifo_2: (%d)\n", __func__, ret);
		ret = -ENOMEM;
		goto free_fifo_1_dstbuf;
	}

	if (reset_idx) {
		fifo->write_idx = 0;
		fifo->complete_idx = 0;
		fifo->pp_prev_end = 0;
		fifo->write_idx_reg = hwz->ctrl.zram_enc_base +
			ZRAM_ENC_CMD_SECOND_FIFO_WRITE_INDEX;
		fifo->complete_idx_reg = hwz->ctrl.zram_enc_base +
			ZRAM_ENC_CMD_SECOND_FIFO_COMPLETE_INDEX;
	}
	spin_lock_init(&hwz->fifo_2_lock);

	/* Allocate dst buffers for every fifo cmds & set fifo ID */
	ret = hwz->ops->fill_comp_fifo_dst_buffers(fifo, ENGINE_COMP_FIFO_2_ID);
	if (ret) {
		pr_info("%s: failed to prepare cmd dst buffers for fifo_2: (%d)\n", __func__, ret);
		ret = -ENOMEM;
		goto free_fifo_2;
	}

	/* Setup registers for compression fifo #2 */
	engine_setup_enc_second_fifo(&hwz->ctrl, fifo->buf_pa, ENGINE_COMP_FIFO_2_ENTRY_BITS);

	/*
	 * Decompression FIFOs
	 */

	/* Initialize decompression fifos */
	for (i = 0; i < MAX_DCOMP_NR; i++) {
		unsigned long offset = i * 4;

		fifo = &hwz->dcomp_fifo[i];
		ret = create_fifo_memory(fifo, 1 << ENGINE_DCOMP_FIFO_ENTRY_BITS,
				ENGINE_DCOMP_CMD_SIZE, sizeof(struct dcomp_pp_info));
		if (ret) {
			pr_info("%s: failed to prepare decompression fifo[%d]: (%d)\n",
					__func__, i, ret);
			ret = -ENOMEM;
			goto free_fifo_2_dstbuf;
		}

		if (reset_idx) {
			fifo->write_idx = 0;
			fifo->complete_idx = 0;
			fifo->pp_prev_end = 0;
			fifo->write_idx_reg = hwz->ctrl.zram_dec_base +
				ZRAM_DEC_CMD_FIFO_0_WRITE_INDEX + offset;
			fifo->complete_idx_reg = hwz->ctrl.zram_dec_base +
				ZRAM_DEC_CMD_FIFO_0_COMPLETE_INDEX + offset;
		}

#ifdef ZRAM_ENGINE_DEBUG
		pr_info("(%d): write_idx_reg(%llx), complete_idx_reg(%llx)\n",
			i, (unsigned long long)fifo->write_idx_reg, (unsigned long long)fifo->complete_idx_reg);
#endif

		/* Setup registers for compression fifo #2 */
		engine_setup_dec_fifo(&hwz->ctrl, i, fifo->buf_pa, ENGINE_DCOMP_FIFO_ENTRY_BITS);
	}

	/* Allocate src buffers for every fifo cmds */
	for (i = 0; i < MAX_DCOMP_NR; i++) {
		fifo = &hwz->dcomp_fifo[i];
		ret = hwz->ops->fill_dcomp_fifo_src_buffers(fifo, i);
		if (ret) {
			pr_info("%s: failed to prepare decompression src buffers for fifo: (%d)\n",
					__func__, i);
			ret = -ENOMEM;
			goto free_dcomp_fifos;
		}
	}

	/* Reset engine all fifo indices */
	if (reset_idx)
		engine_reset_all_indices(&hwz->ctrl);

	/* Setup fifos successfully. */
	return 0;

free_dcomp_fifos:

	/* Release all allocated buffers for dcomp_fifo# */
	for (i = 0; i < MAX_DCOMP_NR; i++)
		hwz->ops->release_dcomp_fifo_src_buffers(&hwz->dcomp_fifo[i]);

free_fifo_2_dstbuf:

	/* Destroy initialized decompression fifos */
	for (j = 0; j < i; j++)
		destroy_fifo_memory(&hwz->dcomp_fifo[j]);

	/* Release all allocated buffers for fifo_2 */
	hwz->ops->release_comp_fifo_dst_buffers(&hwz->fifo_2);

free_fifo_2:

	/* Destroy compression fifo#2 */
	destroy_fifo_memory(&hwz->fifo_2);

free_fifo_1_dstbuf:

	/* Release all allocated buffers for fifo_1 */
	hwz->ops->release_comp_fifo_dst_buffers(&hwz->fifo_1);

free_fifo_1:

	/* Destroy compression fifo#1 */
	destroy_fifo_memory(&hwz->fifo_1);

exit:
	return ret;
}

static void zram_engine_sw_deinit(struct zram_engine_t *hwz)
{
	/* Stop all kernel threads */
	kthread_stop(hwz->gla_work);
	kthread_stop(hwz->dcomp_pp_work);
	kthread_stop(hwz->comp_pp_work);

	/* Destroy all fifos */
	zram_engine_destroy_fifos(hwz);

	/* Destroy allocators */
	destroy_size_allocator();
}

static int zram_engine_sw_init(struct zram_engine_t *hwz)
{
	int ret = 0;

	pr_info("%s: start\n", __func__);

	ret = create_size_allocator();
	if (ret) {
		pr_info("%s: failed to create size allocator.\n", __func__);
		goto exit;
	}

	/* Initialize fifos */
	ret = zram_engine_setup_fifos(hwz, true);
	if (ret) {
		pr_info("%s: failed to setup fifo(s).\n", __func__);
		goto destroy_allocator;
	}

	/* Initialize workers for post-process & gear-level-adjustment */
	init_waitqueue_head(&hwz->comp_wait);
	init_waitqueue_head(&hwz->dcomp_wait);
	init_waitqueue_head(&hwz->gla_wait);
	hwz->comp_pp_work = kthread_run(comp_post_process, hwz, "comp_pp_worker");
	hwz->dcomp_pp_work = kthread_run(dcomp_post_process, hwz, "dcomp_pp_worker");
	hwz->gla_work = kthread_run(gear_level_adjustment, hwz, "gla_worker");

	pr_info("%s: done\n", __func__);
	return 0;

destroy_allocator:
	destroy_size_allocator();

exit:
	return ret;
}

/* Check whether all FIFO requests are finished */
static bool hwcomp_all_fifo_empty(struct zram_engine_t *hwz)
{
	struct hwfifo *fifo;
	int cpu;

	if (!comp_fifo_1_empty(&hwz->fifo_1)) {
		pr_info("%s: comp(0) is busy!\n", __func__);
		return false;
	}

	if (!comp_fifo_2_empty(&hwz->fifo_2)) {
		pr_info("%s: comp(1) is busy!\n", __func__);
		return false;
	}

	for (cpu = 0; cpu < MAX_DCOMP_NR; cpu++) {
		fifo = &hwz->dcomp_fifo[cpu];
		if (!dcomp_fifo_empty(fifo)) {
			pr_info("%s: decomp(%d) is busy!\n", __func__, cpu);
			return false;
		}
	}

	/* Wait for HW lat_fifo empty */
	engine_enc_wait_idle(&hwz->ctrl);
	engine_dec_wait_idle(&hwz->ctrl);

	return true;
}

/* Reset HW & release previous allocated fifo memory */
static bool __hwcomp_mode_switch_reset(struct zram_engine_t *hwz, bool dst_copy)
{
	smp_rmb();

	if (!hwcomp_all_fifo_empty(hwz))
		return false;

	/* Release previous allocated fifo memory */
	zram_engine_destroy_fifos(hwz);

	/* Init HW */
	engine_enc_init(&hwz->ctrl, dst_copy);
	engine_dec_init(&hwz->ctrl, dst_copy);

	return true;
}

static inline bool hwcomp_mode_locked(struct zram_engine_t *hwz)
{
	return atomic_read(&hwz->mode_locked) != HWCOMP_MODE_UNLOCK;
}

#if IS_ENABLED(CONFIG_HWCOMP_SUPPORT_NO_DST_COPY)
/* Switch hwcomp to No-DST copy mode */
static int hwcomp_use_no_dst_copy_mode(struct zram_engine_t *hwz)
{
	int ret = 0;

	/* Sanity check */
	if (!hwz)
		return -EINVAL;

	/* No switch when it's locked */
	if (hwcomp_mode_locked(hwz)) {
		pr_info("%s: switch mode is forbidden.\n", __func__);
		ret = -EBUSY;
		goto exit;
	}

	/* It's already No-DST copy mode */
	if (hwz->ops == &engine_ndc_ops)
		goto exit;

	/* Reset HW & release previous allocated fifo memory */
	if (!__hwcomp_mode_switch_reset(hwz, false)) {
		ret = -EBUSY;
		goto exit;
	}

	/* Ok, it's time to change mode */
	smp_wmb();
	hwz->ops = &engine_ndc_ops;

	/* Init fifo memory */
	ret = zram_engine_setup_fifos(hwz, false);
	if (ret) {
		pr_info("%s: failed to setup fifo(s).\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}

	/* Switching mode complete. */
	pr_info("%s: Switch to No-DST copy mode successfully!\n", __func__);

exit:
	return ret;
}
#endif

/* Switch hwcomp to DST copy mode */
static int hwcomp_use_dst_copy_mode(struct zram_engine_t *hwz)
{
	int ret = 0;

	/* Sanity check */
	if (!hwz)
		return -EINVAL;

	/* No switch when it's locked */
	if (hwcomp_mode_locked(hwz)) {
		pr_info("%s: switch mode is forbidden.\n", __func__);
		ret = -EBUSY;
		goto exit;
	}

	/* It's already DST copy mode */
	if (hwz->ops == &engine_dc_ops)
		goto exit;

	/* Reset HW & release previous allocated fifo memory */
	if (!__hwcomp_mode_switch_reset(hwz, true)) {
		ret = -EBUSY;
		goto exit;
	}

	/* Ok, it's time to change mode */
	smp_wmb();
	hwz->ops = &engine_dc_ops;

	/* Init fifo memory */
	ret = zram_engine_setup_fifos(hwz, false);
	if (ret) {
		pr_info("%s: failed to setup fifo(s).\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}

	/* Switching mode complete. */
	pr_info("%s: Switch to DST copy mode successfully!\n", __func__);

exit:
	return ret;
}

/* Tell hwcomp engine it's forbidden to switch mode now */
static void hwcomp_lock_mode(struct zram_engine_t *hwz)
{
	/* Sanity check */
	if (!hwz)
		return;

	atomic_inc(&hwz->mode_locked);
	pr_info("%s: lock mode (%d)\n", __func__, atomic_read(&hwz->mode_locked));
}

/* Tell hwcomp engine it's ok to switch mode now */
static void hwcomp_unlock_mode(struct zram_engine_t *hwz)
{
	/* Sanity check */
	if (!hwz)
		return;

	atomic_dec_if_positive(&hwz->mode_locked);
	pr_info("%s: unlock mode (%d)\n", __func__, atomic_read(&hwz->mode_locked));
}

static int thermal_callback(struct notifier_block *nb, unsigned long event, void *data)
{
	struct zram_engine_t *hwz = ERR_PTR(-ENODEV);
	int retval;

	mutex_lock(&hwz_mutex);
	if (!list_empty(&hwz_list))
		hwz = list_first_entry(&hwz_list, struct zram_engine_t, list);

	if (IS_ERR(hwz)) {
		retval = -ENOENT;
		pr_info("%s: failed to get hwz.\n", __func__);
		goto exit;
	}

#ifndef FPGA_EMULATION
	/* Enable clock for zram engine */
	retval = engine_gear_enable_clock(&hwz->ctrl, &hwz->gear_ctrl);
	if (retval) {
		pr_info("%s: engine_gear_enable_clock fail: (%d)\n", __func__, retval);
		goto exit;
	}
#endif

	/* Thermal hint received */
	if (event == 1)
		engine_fix_gear_level(&hwz->gear_ctrl, 3 - ENGINE_GEAR_DTS_BASE);
	else
		engine_free_gear_level(&hwz->gear_ctrl);

#ifndef FPGA_EMULATION
	/*
	 * Disable clock for zram engine -
	 * Paired with the one in thermal_callback.
	 */
	engine_gear_disable_clock(&hwz->ctrl, &hwz->gear_ctrl);
#endif
exit:
	mutex_unlock(&hwz_mutex);

	return NOTIFY_OK;
}

static struct notifier_block thermal_notifier = {
	.notifier_call = thermal_callback,
};

static int mtk_hwzram_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct zram_engine_t *hwz = NULL;

	pr_info("%s: start\n", __func__);

	hwz = kzalloc(sizeof(struct zram_engine_t), GFP_KERNEL);
	if (!hwz) {
		pr_info("%s: failed to allocate hwz structure.\n", __func__);
		return -ENOMEM;
	}

	/* Default is DST Copy */
	hwz->ops = &engine_dc_ops;

	/* Reset comp_cnt & dcomp_cnt */
	atomic_set(&hwz->comp_cnt, 0);
	atomic_set(&hwz->dcomp_cnt, 0);

	/* Setup platform resource for zram engine */
	ret = zram_engine_platform_init(pdev, hwz);
	if (ret) {
		pr_info("%s: failed to setup platform resource (%d).\n", __func__, ret);
		goto free_hwz;
	}

	/* Initialize zram engine */
	ret = zram_engine_hw_init(hwz);
	if (ret) {
		pr_info("%s: failed to setup engine (%d).\n", __func__, ret);
		goto clear_plat;
	}

	/* Initialize SW resource for zram engine */
	ret = zram_engine_sw_init(hwz);
	if (ret) {
		pr_info("%s: failed to initialize SW resource (%d).\n", __func__, ret);
		goto clear_hw;
	}

	/*
	 * Set a refcount to hwz & add to hw_list.
	 * This refcount is counted for the user doing probing.
	 */
	refcount_set(&hwz->refcount, HWZ_INIT_REFCNT);
	pr_info("%s:%d refcount:%x\n", __func__, __LINE__, refcount_read(&hwz->refcount));

	/* Initialize mode_locked to unlock */
	atomic_set(&hwz->mode_locked, HWCOMP_MODE_UNLOCK);

	/* It's safe to enable rtff check */
	static_branch_enable(&engine_rtff_check);

	/* Add it to hwz_list */
	mutex_lock(&hwz_mutex);
	WARN_ON_ONCE(!list_empty(&hwz_list));	/* Suppose the list is empty here */
	list_add(&hwz->list, &hwz_list);
	mutex_unlock(&hwz_mutex);

	/* Register thermal hint notifier */
	mtk_thermal_hint_notify_register("zram_cooling", &thermal_notifier);

#ifndef FPGA_EMULATION
	/*
	 * Disable clock for zram engine -
	 * Paired with the one in mtk_hwzram_remove.
	 */
	engine_gear_disable_clock(&hwz->ctrl, &hwz->gear_ctrl);
#endif

	pr_info("%s: done\n", __func__);
	return 0;

clear_hw:
	zram_engine_hw_deinit(hwz);

clear_plat:
	zram_engine_platform_deinit(pdev, hwz);

free_hwz:
	kfree(hwz);

	return ret;
}

static void mtk_hwzram_remove(struct platform_device *pdev)
{
	struct zram_engine_t *hwz = platform_get_drvdata(pdev);
	int ret;

	if (!hwz) {
		pr_info("%s: no hwz instance found.\n", __func__);
		return;
	}

#ifndef FPGA_EMULATION
	/* Enable clock for zram engine */
	ret = engine_gear_enable_clock(&hwz->ctrl, &hwz->gear_ctrl);
	if (ret) {
		pr_info("%s: engine_gear_enable_clock fail: (%d)\n", __func__, ret);
		return;
	}
#endif

	/* Unregister thermal hint notifier */
	mtk_thermal_hint_notify_unregister("zram_cooling", &thermal_notifier);

	/* Remove it from hwz_list */
	mutex_lock(&hwz_mutex);
	list_del(&hwz->list);
	mutex_unlock(&hwz_mutex);

	/* It's time to disable rtff check */
	static_branch_disable(&engine_rtff_check);

	/* Clear resource for SW control */
	zram_engine_sw_deinit(hwz);

	/* Reset zram engine */
	zram_engine_hw_deinit(hwz);

	/* Release platform resource for zram engine */
	zram_engine_platform_deinit(pdev, hwz);

	/* Free it */
	kfree(hwz);
}

static int __maybe_unused mtk_hwzram_suspend(struct device *dev)
{
	struct zram_engine_t *hwz = dev_get_drvdata(dev);
#if !defined(FPGA_EMULATION)
	int ret;
#endif

	pr_info("++ %s ++\n", __func__);

	/*
	 * Do NOT proceed if no hwz instance.
	 * The LAST power control is completed by driver DIRECTLY, not through PM.
	 */
	if (!hwz) {
		pr_info("%s: no hwz instance found.\n", __func__);
		return 0;
	}

#if !defined(FPGA_EMULATION)
	if (!engine_power_efficiency_enabled()) {
		ret = engine_gear_power_off(&hwz->ctrl, &hwz->gear_ctrl);
		if (ret) {
			pr_info("%s: engine_power_off fail: (%d)\n", __func__, ret);
			return ret;
		}
	}

#endif

	pr_info("-- %s --\n", __func__);
	return 0;
}

static int __maybe_unused mtk_hwzram_resume(struct device *dev)
{
	struct zram_engine_t *hwz = dev_get_drvdata(dev);
#if !defined(FPGA_EMULATION)
	int ret;
#endif

	pr_info("++ %s ++\n", __func__);

	/*
	 * Do NOT proceed if no hwz instance.
	 * The INITIAL power control is completed by driver DIRECTLY, not through PM.
	 */
	if (!hwz) {
		pr_info("%s: no hwz instance found.\n", __func__);
		return 0;
	}

#if !defined(FPGA_EMULATION)
	if (!engine_power_efficiency_enabled()) {
		ret = engine_gear_power_on(&hwz->ctrl, &hwz->gear_ctrl);
		if (ret) {
			pr_info("%s: engine_power_on fail: (%d)\n", __func__, ret);
			return ret;
		}
	}
#endif

	pr_info("-- %s --\n", __func__);
	return 0;
}

static int __maybe_unused mtk_hwzram_runtime_suspend(struct device *dev)
{
	return mtk_hwzram_suspend(dev);
}

static int __maybe_unused mtk_hwzram_runtime_resume(struct device *dev)
{
	return mtk_hwzram_resume(dev);
}

static const struct dev_pm_ops mtk_hwzram_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_hwzram_suspend, mtk_hwzram_resume)
	SET_RUNTIME_PM_OPS(mtk_hwzram_runtime_suspend, mtk_hwzram_runtime_resume, NULL)
};

#define DEV_PM_OPS (IS_ENABLED(CONFIG_PM) ? &mtk_hwzram_pm_ops : NULL)

struct platform_driver mtk_hwzram = {
	.probe = mtk_hwzram_probe,
	.remove = mtk_hwzram_remove,
	.driver = {
		.name = "mtk-hwzram",
		/* .bus = &platform_bus_type, (TODO) Using this with dma-ranges is a formal way to query dma-mask. */
		.pm = DEV_PM_OPS,
		.owner = THIS_MODULE,
		.of_match_table = mtk_hwzram_of_match,
	},
};

static int __init zram_engine_init(void)
{
	int ret;

	ret = platform_driver_register(&mtk_hwzram);
	if (ret)
		pr_info("%s: failed to register platform driver.\n", __func__);

	return ret;
}
module_init(zram_engine_init);

static void __exit zram_engine_exit(void)
{
	platform_driver_unregister(&mtk_hwzram);
}
module_exit(zram_engine_exit);

#if IS_ENABLED(CONFIG_MTK_VM_DEBUG)

/* It's forbidden for operations of the following range when hwcomp is locked. */
#define HWE_EXP_FORBIDDEN_MIN	(1)
#define HWE_EXP_FORBIDDEN_MAX	(10)

static int kick_hwe_exp(const char *val, const struct kernel_param *kp)
{
	int retval;
	unsigned long tmp;
	struct zram_engine_t *hwz = ERR_PTR(-ENODEV);

	retval = kstrtoul(val, 0, &tmp);
	if (retval != 0) {
		pr_info("%s: failed to do operation!\n", __func__);
		return retval;
	}

	mutex_lock(&hwz_mutex);

	if (!list_empty(&hwz_list))
		hwz = list_first_entry(&hwz_list, struct zram_engine_t, list);

	if (IS_ERR(hwz)) {
		retval = -ENOENT;
		pr_info("%s: failed to get hwz.\n", __func__);
		goto pre_exit;
	}

#ifndef FPGA_EMULATION
	/* Enable clock for zram engine */
	retval = engine_gear_enable_clock(&hwz->ctrl, &hwz->gear_ctrl);
	if (retval) {
		pr_info("%s: engine_gear_enable_clock fail: (%d)\n", __func__, retval);
		goto pre_exit;
	}
#endif

	if (!hwcomp_all_fifo_empty(hwz)) {
		retval = -EBUSY;
		pr_info("%s: There are pending requests in FIFO(s).\n", __func__);
		goto exit;
	}

	/* Forbidden operations when hwcomp is locked */
	if (tmp >= HWE_EXP_FORBIDDEN_MIN && tmp < HWE_EXP_FORBIDDEN_MAX) {
		if (hwcomp_mode_locked(hwz)) {
			retval = -EBUSY;
			pr_info("%s: forbidden for %lu.\n", __func__, tmp);
			goto exit;
		}
	}

	switch (tmp) {

	/*** Take effect ONLY AFTER HW initialization ***/
	case 1:
		/* Disable Coherence */
		pr_info("%s: Disable Coherence.\n", __func__);
		static_branch_enable(&engine_no_coherence);
		break;
	case 2:
		/* Enable Coherence */
		pr_info("%s: Enable Coherence.\n", __func__);
		static_branch_disable(&engine_no_coherence);
		break;
	case 3:
		/* Async mode off */
		pr_info("%s: Async mode off.\n", __func__);
		static_branch_enable(&engine_sync_mode);
		break;
	case 4:
		/* Async mode on */
		pr_info("%s: Async mode on.\n", __func__);
		static_branch_disable(&engine_sync_mode);
		break;

	/*** Take effect without HW initialization ***/
	case 13:
		/* Bypass SMMU */
		pr_info("%s: Bypass SMMU.\n", __func__);
		engine_smmu_bypass(&hwz->ctrl);
		break;
	case 14:
		/* Join SMMU */
		pr_info("%s: Join SMMU.\n", __func__);
		engine_smmu_join(&hwz->ctrl);
		break;

	/*** Check registers ***/
	case 90:
		/* Dump all registers and do comparison if necessary */
		engine_dump_all_registers(&hwz->ctrl);
		break;
	case 91:
		/* Compare all registers with the previous record */
		retval = engine_compare_all_registers(&hwz->ctrl);
		break;
	case 92:
		/* Same as kick_hwe_ctrl, but output to kernel log */
		engine_get_reg_status(&hwz->ctrl, NULL);
		dump_fifo_idx(hwz, NULL, 0);
		engine_gear_get_status(&hwz->gear_ctrl, NULL);
		break;
	case 93:
		/* Dump more debug registers */
		engine_fatal_get_reg_status(&hwz->ctrl, NULL);
		break;
	default:
		pr_info("%s invalid ops!\n", __func__);
		retval = -EINVAL;
		break;
	}

	/* Reinit enc & dec for the following operations */
	if (tmp >= HWE_EXP_FORBIDDEN_MIN && tmp < HWE_EXP_FORBIDDEN_MAX) {
		engine_enc_init(&hwz->ctrl, (hwz->ops == &engine_dc_ops));
		engine_dec_init(&hwz->ctrl, (hwz->ops == &engine_dc_ops));
	}

exit:
#ifndef FPGA_EMULATION
	/*
	 * Disable clock for zram engine -
	 * Paired with the one in kick_hwe_exp.
	 */
	engine_gear_disable_clock(&hwz->ctrl, &hwz->gear_ctrl);
#endif

pre_exit:
	mutex_unlock(&hwz_mutex);
	return retval;
}
static const struct kernel_param_ops hwe_param_ops = {
	.set = &kick_hwe_exp,
};
module_param_cb(kick_hwe_exp, &hwe_param_ops, NULL, 0200);

static int kick_hwe_power(const char *val, const struct kernel_param *kp)
{
	int retval;
	unsigned long tmp;
	struct zram_engine_t *hwz = ERR_PTR(-ENODEV);

	retval = kstrtoul(val, 0, &tmp);
	if (retval != 0) {
		pr_info("%s: failed to do operation!\n", __func__);
		return retval;
	}

	mutex_lock(&hwz_mutex);

	if (!list_empty(&hwz_list))
		hwz = list_first_entry(&hwz_list, struct zram_engine_t, list);

	if (IS_ERR(hwz)) {
		retval = -ENOENT;
		pr_info("%s: failed to get hwz.\n", __func__);
		goto exit;
	}

	switch (tmp) {
	case 11:
		/* Power off ZRAM */
		pr_info("%s: Power off ZRAM.\n", __func__);
		retval = engine_gear_power_off(&hwz->ctrl, &hwz->gear_ctrl);
		break;
	case 12:
		/* Power on ZRAM */
		pr_info("%s: Power on ZRAM.\n", __func__);
		retval = engine_gear_power_on(&hwz->ctrl, &hwz->gear_ctrl);
		break;
	default:
		pr_info("%s invalid ops!\n", __func__);
		retval = -EINVAL;
		break;
	}

exit:
	mutex_unlock(&hwz_mutex);
	return retval;
}
static const struct kernel_param_ops hwe_power_param_ops = {
	.set = &kick_hwe_power,
	.get = NULL,	/* power status can be queried from get_hwe_gear */
};
module_param_cb(kick_hwe_power, &hwe_power_param_ops, NULL, 0600);
#endif

static int kick_hwe_gear(const char *val, const struct kernel_param *kp)
{
	int retval;
	unsigned long tmp;
	struct zram_engine_t *hwz = ERR_PTR(-ENODEV);

	retval = kstrtoul(val, 0, &tmp);
	if (retval != 0) {
		pr_info("%s: failed to do operation!\n", __func__);
		return retval;
	}

	mutex_lock(&hwz_mutex);

	if (!list_empty(&hwz_list))
		hwz = list_first_entry(&hwz_list, struct zram_engine_t, list);

	if (IS_ERR(hwz)) {
		retval = -ENOENT;
		pr_info("%s: failed to get hwz.\n", __func__);
		goto pre_exit;
	}

#ifndef FPGA_EMULATION
	/* Enable clock for zram engine */
	retval = engine_gear_enable_clock(&hwz->ctrl, &hwz->gear_ctrl);
	if (retval) {
		pr_info("%s: engine_gear_enable_clock fail: (%d)\n", __func__, retval);
		goto pre_exit;
	}
#endif

	if (!hwcomp_all_fifo_empty(hwz)) {
		retval = -EBUSY;
		pr_info("%s: There are pending requests in FIFO(s).\n", __func__);
		goto exit;
	}

	switch (tmp) {
	case 0:
		/* Max frequency */
		engine_fix_gear_level(&hwz->gear_ctrl, ENGINE_MAX_GEAR);
		break;
	case 3:
		/* CLK_SEL(3): 218.4MHz with 0.55v or 0.575v */
		engine_fix_gear_level(&hwz->gear_ctrl, 3 - ENGINE_GEAR_DTS_BASE);
		break;
	case 4:
		/* CLK_SEL(4): 384MHz with 0.6v */
		engine_fix_gear_level(&hwz->gear_ctrl, 4 - ENGINE_GEAR_DTS_BASE);
		break;
	case 5:
		/* CLK_SEL(5): 436.8MHz with 0.65v */
		engine_fix_gear_level(&hwz->gear_ctrl, 5 - ENGINE_GEAR_DTS_BASE);
		break;
	case 6:
		/* CLK_SEL(6): 546MHz with 0.725v */
		engine_fix_gear_level(&hwz->gear_ctrl, 6 - ENGINE_GEAR_DTS_BASE);
		break;
	case 7:
		/* CLK_SEL(7): 728MHz with 0.825v or 0.95v */
		engine_fix_gear_level(&hwz->gear_ctrl, 7 - ENGINE_GEAR_DTS_BASE);
		break;

#if IS_ENABLED(CONFIG_MTK_VM_DEBUG)
	case ENGINE_ENABLE_GEAR_PE:
		static_branch_enable(&engine_power_efficiency);
		break;
	case ENGINE_DISABLE_GEAR_PE:
		static_branch_disable(&engine_power_efficiency);
		break;
#endif
	case ENGINE_FREE_RUN_GEAR:
		/* gear free-run */
		engine_free_gear_level(&hwz->gear_ctrl);
		break;
	default:
		pr_info("%s: invalid gear!\n", __func__);
		retval = -EINVAL;
		break;
	}

exit:
#ifndef FPGA_EMULATION
	/*
	 * Disable clock for zram engine -
	 * Paired with the one in kick_hwe_gear.
	 */
	engine_gear_disable_clock(&hwz->ctrl, &hwz->gear_ctrl);
#endif

pre_exit:
	mutex_unlock(&hwz_mutex);
	return retval;
}

static int get_hwe_gear(char *buf, const struct kernel_param *kp)
{
	int retval;
	struct zram_engine_t *hwz = ERR_PTR(-ENODEV);

	mutex_lock(&hwz_mutex);

	if (!list_empty(&hwz_list))
		hwz = list_first_entry(&hwz_list, struct zram_engine_t, list);

	if (IS_ERR(hwz)) {
		retval = -ENOENT;
		pr_info("%s: failed to get hwz.\n", __func__);
		goto exit;
	}

	retval = engine_gear_get_status(&hwz->gear_ctrl, buf);
exit:
	mutex_unlock(&hwz_mutex);
	return retval;
}

static const struct kernel_param_ops hwe_gear_param_ops = {
	.set = &kick_hwe_gear,
	.get = &get_hwe_gear,
};
module_param_cb(kick_hwe_gear, &hwe_gear_param_ops, NULL, 0600);

static int dump_fifo_idx(struct zram_engine_t *hwz, char *buf, int offset)
{
	struct hwfifo *fifo;
	int copied = offset;
	int i;

	fifo = &hwz->fifo_1;
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied,
			"[enc:main] - 0x%-4x:0x%-4x:0x%-4x\n",
			fifo->write_idx, fifo->complete_idx, fifo->pp_prev_end);
	fifo = &hwz->fifo_2;
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied,
			"[enc:second] - 0x%-4x:0x%-4x:0x%-4x\n",
			fifo->write_idx, fifo->complete_idx, fifo->pp_prev_end);
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied,
			"Total comp cnts: %d, dcomp cnts: %d\n",
			atomic_read(&hwz->comp_cnt), atomic_read(&hwz->dcomp_cnt));
	for (i = 0; i < MAX_DCOMP_NR; i++) {
		fifo = &hwz->dcomp_fifo[i];
		ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied,
				"[dec:%d] - 0x%-4x:0x%-4x:0x%-4x\n",
				i, fifo->write_idx, fifo->complete_idx, fifo->pp_prev_end);
	}
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied,
			"comp hang: %d, dcomp hang: %d, rtff pass: %d, rtff fail: %d\n",
			atomic_read(&enc_suspect_hang_count), atomic_read(&dec_suspect_hang_count),
			atomic_read(&engine_rtff_pass_count), atomic_read(&engine_rtff_fail_count));

	return copied;
}

static int get_hwe_ctrl(char *buf, const struct kernel_param *kp)
{
	int retval;
	struct zram_engine_t *hwz = ERR_PTR(-ENODEV);

	mutex_lock(&hwz_mutex);

	if (!list_empty(&hwz_list))
		hwz = list_first_entry(&hwz_list, struct zram_engine_t, list);

	if (IS_ERR(hwz)) {
		retval = -ENOENT;
		pr_info("%s: failed to get hwz.\n", __func__);
		goto exit;
	}

#ifndef FPGA_EMULATION
	/* Enable clock for zram engine */
	retval = engine_gear_enable_clock(&hwz->ctrl, &hwz->gear_ctrl);
	if (retval) {
		pr_info("%s: engine_gear_enable_clock fail: (%d)\n", __func__, retval);
		goto exit;
	}
#endif

	retval = engine_get_reg_status(&hwz->ctrl, buf);
	retval = dump_fifo_idx(hwz, buf, retval);
exit:
#ifndef FPGA_EMULATION
	/*
	 * Disable clock for zram engine -
	 * Paired with the one in get_hwe_ctrl.
	 */
	engine_gear_disable_clock(&hwz->ctrl, &hwz->gear_ctrl);
#endif

	mutex_unlock(&hwz_mutex);
	return retval;
}

static const struct kernel_param_ops hwe_ctrl_param_ops = {
	.get = &get_hwe_ctrl,
};
module_param_cb(kick_hwe_ctrl, &hwe_ctrl_param_ops, NULL, 0400);

void *hwcomp_create(int mode, compress_pp_fn comp_pp_cb, decompress_pp_fn dcomp_pp_cb)
{
	struct zram_engine_t *hwz = ERR_PTR(-ENODEV);
	int ret = -1;

	pr_info("++%s++\n", __func__);

	if (!comp_pp_cb || !dcomp_pp_cb) {
		pr_info("%s: comp_pp_cb or dcomp_pp_cb is NULL.\n", __func__);
		return NULL;
	}

	mutex_lock(&hwz_mutex);

	if (!list_empty(&hwz_list))
		hwz = list_first_entry(&hwz_list, struct zram_engine_t, list);

	if (IS_ERR(hwz) || refcount_read(&hwz->refcount) >= HWZ_MAX_REFCNT) {
		hwz = NULL;
		pr_info("%s: failed to get hwz.\n", __func__);
		goto exit;

	}

#ifndef FPGA_EMULATION
	/* Enable clock for zram engine */
	ret = engine_gear_enable_clock(&hwz->ctrl, &hwz->gear_ctrl);
	if (ret) {
		hwz = NULL;
		pr_info("%s: engine_gear_enable_clock fail: (%d)\n", __func__, ret);
		goto exit;
	}
#endif

	/* Increment the refcount only the hwz is already in hwz_list. */
	refcount_inc(&hwz->refcount);
	pr_info("%s: Get hwz successfully - (new) refcount:%x\n",
			__func__, refcount_read(&hwz->refcount));

	/* Setup callbacks and switch to proper mode */
	hwcomp_decompress_post_process = dcomp_pp_cb;
	if (mode == NO_DST_COPY_MODE) {
#if IS_ENABLED(CONFIG_HWCOMP_SUPPORT_NO_DST_COPY)
		hwcomp_compress_post_process_ndc = comp_pp_cb;
		hwcomp_compress_post_process_dc = NULL;
		ret = hwcomp_use_no_dst_copy_mode(hwz);
#else
		pr_info("%s: No-DST copy mode is not available!\n", __func__);
		ret = -EINVAL;
#endif
	} else if (mode == DST_COPY_MODE) {
#if IS_ENABLED(CONFIG_HWCOMP_SUPPORT_NO_DST_COPY)
		hwcomp_compress_post_process_ndc = NULL;
#endif
		hwcomp_compress_post_process_dc = comp_pp_cb;
		ret = hwcomp_use_dst_copy_mode(hwz);
	}

#ifndef FPGA_EMULATION
	/*
	 * Disable clock for zram engine -
	 * Paired with the one in hwcomp_create.
	 */
	engine_gear_disable_clock(&hwz->ctrl, &hwz->gear_ctrl);
#endif

	if (ret != 0) {
		refcount_dec(&hwz->refcount);
		hwz = NULL;
		pr_info("%s: Failed to set mode to (%d), ret (%d)\n", __func__, mode, ret);
		goto exit;
	}

	/* Lock mode to avoid unexpected switch */
	hwcomp_lock_mode(hwz);
exit:
	mutex_unlock(&hwz_mutex);
	return hwz;
}
EXPORT_SYMBOL(hwcomp_create);

void hwcomp_destroy(void *priv)
{
	struct zram_engine_t *hwz = priv, *tmp = NULL;

	if (!hwz)
		return;

	pr_info("++%s++\n", __func__);

	mutex_lock(&hwz_mutex);

	if (!list_empty(&hwz_list)) {
		/* Validate hwz (we have only one hwz instance) */
		tmp = list_first_entry(&hwz_list, struct zram_engine_t, list);
		if (tmp != hwz) {
			pr_info("%s: not valid hwz.\n", __func__);
			goto exit;
		}

		/* Decrease the refcount */
		refcount_dec(&hwz->refcount);

		/* Unlock mode */
		hwcomp_unlock_mode(hwz);
	} else {
		/* Should not be here ... */
		WARN_ON_ONCE(1);
	}

exit:
	mutex_unlock(&hwz_mutex);
}
EXPORT_SYMBOL(hwcomp_destroy);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MediaTek ZRAM Engine");
