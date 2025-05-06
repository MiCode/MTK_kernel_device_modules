/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef _ENGINE_REGS_H_
#define _ENGINE_REGS_H_

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/iommu.h>

/* Engine base registers */
struct engine_control_t {

	/* (frequently used) Register base address */
	void __iomem *zram_pm_base;
	void __iomem *zram_config_base;
	void __iomem *zram_smmu_base;
	void __iomem *zram_dec_base;
	void __iomem *zram_enc_base;

	/* Engine ENC/DEC IRQ setting */
	uint32_t enc_irq_setting;
	uint32_t dec_irq_setting;

	/* Control by gear enable/disable clock */
	atomic_t enc_irq_status;
	atomic_t dec_irq_status;

	/* Use smmu s2 or not */
	bool smmu_s2;

#if IS_ENABLED(CONFIG_MTK_VM_DEBUG)
	/* Register size range */
	resource_size_t zram_config_res_sz;
	resource_size_t zram_dec_res_sz;
	resource_size_t zram_enc_res_sz;
#endif

} ____cacheline_internodealigned_in_smp;

#define ENGINE_MAX_IRQ_COUNT	(10)
struct engine_irq_t {
	const char *name;
	const irq_handler_t handler;
	const unsigned long flags;	/* IRQ handling flags */
	void *priv;			/* Private data for handler */
	int irq;			/* IRQ number */
};

struct engine_error_t {
	union {
		struct {
			uint32_t inst_error;
			uint32_t apb_error;
		};
		uint64_t error_status;
	};
};

// ---------------------- ZRAM_PM Definitions ---------------------- //
#define ZRAM_SSYSPM_CON						0x90A0

// ---------------------- ZRAM_CONFIG Definitions ---------------------- //
#define	ZRAM_CONFIG_ZRAM_CG_CON0				0x0100
#define	ZRAM_CONFIG_ZRAM_CG_SET0				0x0104
#define	ZRAM_CONFIG_ZRAM_CG_CLR0				0x0108
#define	ZRAM_CONFIG_ZRAM_CG_CON1				0x0110
#define	ZRAM_CONFIG_ZRAM_CG_CLR1				0x0118
#define	ZRAM_CONFIG_ZRAM_CG_CON2				0x0120
#define	ZRAM_CONFIG_ZRAM_CG_CLR2				0x0128
#define	ZRAM_CONFIG_ZRAM_CG_CON3				0x0130
#define	ZRAM_CONFIG_ZRAM_CG_CLR3				0x0138
#define	ZRAM_CONFIG_ZRAM_CG_CON4				0x0140
#define	ZRAM_CONFIG_ZRAM_CG_CLR4				0x0148
#define	ZRAM_CONFIG_ZRAM_VERSION				0x0FFC
#define	ZRAM_CONFIG_ZRAM_PWR_PROT_EN_0				0x09A0
#define	ZRAM_CONFIG_ZRAM_PWR_PROT_RDY_0				0x09B0

// ---------------------- ZRAM_SMMU Definitions ---------------------- //
#define SMMU_GLB_CTL0		0x000
#define SMMU_GLB_CTL1		0x004
#define SMMU_GLB_CTL2		0x008
#define SMMU_GLB_CTL4		0x010
#define SMMU_GLB_CTL6		0x018

#define SMMU_GLB_MON0		0x050
#define SMMU_GLB_MON1		0x054
#define SMMU_GLB_MON2		0x058
#define SMMU_GLB_MON3		0x05C
#define SMMU_GLB_MON4		0x060

#define SMMU_PMU_CTL0		0x070
#define SMMU_PMU_MON0		0x074
#define SMMU_LMU_CTL0		0x078

#define SMMU_IRQ_STA		0x080
#define SMMU_IRQ_ACK		0x084
#define SMMU_IRQ_ACK_CNT	0x0088
#define SMMU_IRQ_DIS		0x08C

#define SMMU_IRQ_CNT		0x100

#define SMMU_TBU0_CTL0		0x300
#define SMMU_TBU0_CTL1		0x304
#define SMMU_TBU0_CTL2		0x308
#define SMMU_TBU0_CTL4		0x310
#define SMMU_TBU0_CTL5		0x314
#define SMMU_TBU0_CTL6		0x318
#define SMMU_TBU0_CTL7		0x31C

#define SMMU_TBU0_MON0		0x330
#define SMMU_TBU0_MON1		0x334
#define SMMU_TBU0_MON2		0x338
#define SMMU_TBU0_MON3		0x33C
#define SMMU_TBU0_MON4		0x340
#define SMMU_TBU0_MON5		0x344
#define SMMU_TBU0_MON6		0x348
#define SMMU_TBU0_MON7		0x34C
#define SMMU_TBU0_MON8		0x350
#define SMMU_TBU0_MON9		0x354
#define SMMU_TBU0_MON10		0x358

#define SMMU_TBU0_DBG0		0x360
#define SMMU_TBU0_DBG1		0x364
#define SMMU_TBU0_DBG2		0x368
#define SMMU_TBU0_DBG3		0x36C
#define SMMU_TBU0_DBG4		0x370
#define SMMU_TBU0_DBG5		0x374

#define SMMU_TBU0_RTFM0		0x380
#define SMMU_TBU0_RTFM1		0x384
#define SMMU_TBU0_RTFM2		0x388

#define SMMU_TBU0_WTFM0		0x390
#define SMMU_TBU0_WTFM1		0x394
#define SMMU_TBU0_WTFM2		0x398

#define SMMU_TBU0_MOGC		0x3A0

#define SMMU_TBU0_MOGL0		0x3B0
#define SMMU_TBU0_MOGH0		0x3B4
#define SMMU_TBU0_MOGL1		0x3B8
#define SMMU_TBU0_MOGH1		0x3BC
#define SMMU_TBU0_MOGL2		0x3C0
#define SMMU_TBU0_MOGH2		0x3C4
#define SMMU_TBU0_MOGL3		0x3C8
#define SMMU_TBU0_MOGH3		0x3CC

#define SMMU_TBU0_MON11		0x3d0
#define SMMU_TBU0_MON12		0x3d4
#define SMMU_TBU0_MON13		0x3d8

struct smmuwp_reg {
	unsigned int offset;
	const char *name;
};

static const struct smmuwp_reg zram_smmuwp_regs[] = {
	{ SMMU_GLB_CTL0, "GLB_CTL0" },
	{ SMMU_GLB_CTL1, "GLB_CTL1" },
	{ SMMU_GLB_CTL2, "GLB_CTL2" },
	{ SMMU_GLB_CTL4, "GLB_CTL4" },
	{ SMMU_GLB_CTL6, "GLB_CTL6" },
	{ SMMU_GLB_MON0, "GLB_MON0" },
	{ SMMU_GLB_MON1, "GLB_MON1" },
	{ SMMU_GLB_MON2, "GLB_MON2" },
	{ SMMU_GLB_MON3, "GLB_MON3" },
	{ SMMU_GLB_MON4, "GLB_MON4" },
	{ SMMU_PMU_CTL0, "PMU_CTL0" },
	{ SMMU_PMU_MON0, "PMU_MON0" },
	{ SMMU_LMU_CTL0, "LMU_CTL0" },
	{ SMMU_IRQ_STA, "IRQ_STA" },
	{ SMMU_IRQ_ACK, "IRQ_ACK" },
	{ SMMU_IRQ_ACK_CNT, "IRQ_ACK_CNT" },
	{ SMMU_IRQ_DIS, "IRQ_DIS" },
	{ SMMU_IRQ_CNT, "IRQ_CNT" },
	{ SMMU_TBU0_CTL0, "TBU0_CTL0" },
	{ SMMU_TBU0_CTL1, "TBU0_CTL1" },
	{ SMMU_TBU0_CTL2, "TBU0_CTL2" },
	{ SMMU_TBU0_CTL4, "TBU0_CTL4" },
	{ SMMU_TBU0_CTL5, "TBU0_CTL5" },
	{ SMMU_TBU0_CTL6, "TBU0_CTL6" },
	{ SMMU_TBU0_CTL7, "TBU0_CTL7" },
	{ SMMU_TBU0_MON0, "TBU0_MON0" },
	{ SMMU_TBU0_MON1, "TBU0_MON1" },
	{ SMMU_TBU0_MON2, "TBU0_MON2" },
	{ SMMU_TBU0_MON3, "TBU0_MON3" },
	{ SMMU_TBU0_MON4, "TBU0_MON4" },
	{ SMMU_TBU0_MON5, "TBU0_MON5" },
	{ SMMU_TBU0_MON6, "TBU0_MON6" },
	{ SMMU_TBU0_MON7, "TBU0_MON7" },
	{ SMMU_TBU0_MON8, "TBU0_MON8" },
	{ SMMU_TBU0_MON9, "TBU0_MON9" },
	{ SMMU_TBU0_MON10, "TBU0_MON10" },
	{ SMMU_TBU0_DBG0, "TBU0_DBG0" },
	{ SMMU_TBU0_DBG1, "TBU0_DBG1" },
	{ SMMU_TBU0_DBG2, "TBU0_DBG2" },
	{ SMMU_TBU0_DBG3, "TBU0_DBG3" },
	{ SMMU_TBU0_DBG4, "TBU0_DBG4" },
	{ SMMU_TBU0_DBG5, "TBU0_DBG5" },
	{ SMMU_TBU0_RTFM0, "TBU0_RTFM0" },
	{ SMMU_TBU0_RTFM1, "TBU0_RTFM1" },
	{ SMMU_TBU0_RTFM2, "TBU0_RTFM2" },
	{ SMMU_TBU0_WTFM0, "TBU0_WTFM0" },
	{ SMMU_TBU0_WTFM1, "TBU0_WTFM1" },
	{ SMMU_TBU0_WTFM2, "TBU0_WTFM2" },
	{ SMMU_TBU0_MOGC, "TBU0_MOGC" },
	{ SMMU_TBU0_MOGL0, "TBU0_MOGL0" },
	{ SMMU_TBU0_MOGH0, "TBU0_MOGH0" },
	{ SMMU_TBU0_MOGL1, "TBU0_MOGL1" },
	{ SMMU_TBU0_MOGH1, "TBU0_MOGH1" },
	{ SMMU_TBU0_MOGL2, "TBU0_MOGL2" },
	{ SMMU_TBU0_MOGH2, "TBU0_MOGH2" },
	{ SMMU_TBU0_MOGL3, "TBU0_MOGL3" },
	{ SMMU_TBU0_MOGH3, "TBU0_MOGH3" },
	{ SMMU_TBU0_MON11, "TBU0_MON11" },
	{ SMMU_TBU0_MON12, "TBU0_MON12" },
	{ SMMU_TBU0_MON13, "TBU0_MON13" },
};

// ---------------------- ZRAM_ENC Definitions ---------------------- //
#define ZRAM_ENC_RESET						0x0018
#define ZRAM_ENC_GMCIF_CON_READ_INSTN				0x001C
#define ZRAM_ENC_GMCIF_CON_READ_DATA				0x0020
#define ZRAM_ENC_GMCIF_CON_WRITE_INSTN				0x0024
#define ZRAM_ENC_GMCIF_CON_WRITE_DATA				0x0028
#define ZRAM_ENC_RESOURCE_SETTING				0x002C
#define ZRAM_ENC_CMD_MAIN_FIFO_CONFIG				0x0030
#define ZRAM_ENC_CMD_SECOND_FIFO_CONFIG				0x0034
#define ZRAM_ENC_CMD_MAIN_FIFO_WRITE_INDEX			0x0038
#define ZRAM_ENC_CMD_SECOND_FIFO_WRITE_INDEX			0x003C
#define ZRAM_ENC_CMD_MAIN_FIFO_COMPLETE_INDEX			0x0040
#define ZRAM_ENC_CMD_SECOND_FIFO_COMPLETE_INDEX			0x0044
#define ZRAM_ENC_CMD_MAIN_FIFO_OFFSET_INDEX			0x0048
#define ZRAM_ENC_CMD_SECOND_FIFO_OFFSET_INDEX			0x004C
#define ZRAM_ENC_COMPLETED_INDEX_CLR				0x0050
#define ZRAM_ENC_IRQ_EN						0x00B0
#define ZRAM_ENC_CFG						0x00B4
#define ZRAM_ENC_IRQ_STATUS					0x00B8
#define ZRAM_ENC_ERROR_TYPE_INST				0x00BC
#define ZRAM_ENC_ERROR_TYPE_APB					0x00C0
#define ZRAM_ENC_CONTROL					0x00C4
#define ZRAM_ENC_DEBUG_CON					0x00C8
#define ZRAM_ENC_DEBUG_REG					0x00CC
#define ZRAM_ENC_KERNEL_DEBUG_REG				0x00D0
#define ZRAM_ENC_STATUS						0x00D4
#define ZRAM_ENC_FIFO_THRESHOLD_READ_INSTN_PREULTRA		0x0214
#define ZRAM_ENC_FIFO_THRESHOLD_READ_DATA_PREULTRA_0		0x022C
#define ZRAM_ENC_FIFO_THRESHOLD_READ_DATA_PREULTRA_1		0x0230
#define ZRAM_ENC_FIFO_THRESHOLD_READ_DATA_PREULTRA_2		0x0234
#define ZRAM_ENC_FIFO_THRESHOLD_READ_DATA_PREULTRA_3		0x0238
#define ZRAM_ENC_FIFO_THRESHOLD_WRITE_INSTN_PREULTRA		0x0250
#define ZRAM_ENC_FIFO_THRESHOLD_WRITE_PAGE_PREULTRA		0x025C
#define ZRAM_ENC_SW_LIMIT					0x0268
#define ZRAM_ENC_DBG_0						0x0500
#define ZRAM_ENC_DBG_1						0x0504
#define ZRAM_ENC_DBG_2						0x0508
#define ZRAM_ENC_DBG_3						0x050C

// ---------------------- ZRAM_DEC Definitions ---------------------- //
#define ZRAM_DEC_RESET						0x0010
#define	ZRAM_DEC_GMCIF_CON_READ_CMD				0x0018
#define	ZRAM_DEC_GMCIF_CON_READ_DATA				0x001C
#define	ZRAM_DEC_GMCIF_CON_WRITE_CMD				0x0020
#define	ZRAM_DEC_GMCIF_CON_WRITE_DATA				0x0024
#define	ZRAM_DEC_RESOURCE_SETTING				0x0028
#define	ZRAM_DEC_CMD_FIFO_CONFIG_0				0x0030
#define	ZRAM_DEC_CMD_FIFO_0_WRITE_INDEX				0x0050
#define	ZRAM_DEC_CMD_FIFO_0_COMPLETE_INDEX			0x0070
#define	ZRAM_DEC_CMD_FIFO_0_OFFSET_INDEX			0x0090
#define	ZRAM_DEC_IRQ_EN						0x00B0
#define	ZRAM_DEC_CFG						0x00B4
#define	ZRAM_DEC_IRQ_STATUS					0x00B8
#define	ZRAM_DEC_ERROR_TYPE_INST				0x00BC
#define	ZRAM_DEC_ERROR_TYPE_APB					0x00C0
#define	ZRAM_DEC_CONTROL					0x00C4
#define	ZRAM_DEC_DEBUG_CON					0x00C8
#define	ZRAM_DEC_DEBUG_REG					0x00CC
#define	ZRAM_DEC_STATUS						0x00D4
#define	ZRAM_DEC_FIFO_EMPTY					0x00D8
#define	ZRAM_DEC_FIFO_FULL					0x00DC
#define	ZRAM_DEC_COMPLETED_INDEX_CLR				0x00E0
#define	ZRAM_DEC_SW_LIMIT					0x0288

// ---------------------- MISC declaration or definition ---------------------- //
#define ENC_IRQ_ID	(4147)
#define DEC_IRQ_ID	(4148)

// ---------------------- Function declaration or definition ---------------------- //

int engine_control_init(struct platform_device *pdev, struct engine_control_t *ctrl);
void engine_control_deinit(struct platform_device *pdev, struct engine_control_t *ctrl);

int engine_smmu_setup(struct platform_device *pdev, struct engine_control_t *ctrl);
void engine_smmu_destroy(struct platform_device *pdev, struct engine_control_t *ctrl);

int engine_request_interrupts(struct platform_device *pdev, struct engine_control_t *ctrl,
			struct engine_irq_t *irqs, unsigned int count);

void engine_free_interrupts(struct platform_device *pdev, struct engine_control_t *ctrl,
			struct engine_irq_t *irqs, unsigned int count);

int engine_power_on(struct engine_control_t *ctrl);
void engine_power_off(struct engine_control_t *ctrl);

int engine_request_resource(struct engine_control_t *ctrl);
void engine_release_resource(struct engine_control_t *ctrl);

int engine_clock_init(struct engine_control_t *ctrl);

void engine_smmu_join(struct engine_control_t *ctrl);
void engine_smmu_bypass(struct engine_control_t *ctrl);

void engine_enc_wait_idle(struct engine_control_t *ctrl);
void engine_dec_wait_idle(struct engine_control_t *ctrl);
int engine_enc_wait_idle_timeout(struct engine_control_t *ctrl, unsigned long timeout);

void engine_enc_init(struct engine_control_t *ctrl, bool dst_copy);
void engine_dec_init(struct engine_control_t *ctrl, bool src_snoop);

void engine_setup_enc_fifo(struct engine_control_t *ctrl, unsigned int id, phys_addr_t addr, unsigned int sz_bits);
void engine_setup_dec_fifo(struct engine_control_t *ctrl, unsigned int id, phys_addr_t addr, unsigned int sz_bits);
void engine_reset_enc_indices(struct engine_control_t *ctrl);
void engine_reset_dec_indices(struct engine_control_t *ctrl);
void engine_reset_all_indices(struct engine_control_t *ctrl);

void engine_enc_debug_sel(struct engine_control_t *ctrl, uint32_t reg_val);
void engine_enc_debug_show(struct engine_control_t *ctrl);
void engine_enc_debug_show_more(struct engine_control_t *ctrl);

void engine_dec_debug_sel(struct engine_control_t *ctrl, uint32_t reg_val);
void engine_dec_debug_show(struct engine_control_t *ctrl);
void engine_dec_debug_show_more(struct engine_control_t *ctrl);

static inline void engine_enc_change_mode(struct engine_control_t *ctrl, bool offset_mode)
{
	if (offset_mode)
		writel(0x1, ctrl->zram_enc_base + ZRAM_ENC_CONTROL);
	else
		writel(0x0, ctrl->zram_enc_base + ZRAM_ENC_CONTROL);
}

static inline void engine_dec_change_mode(struct engine_control_t *ctrl, bool offset_mode)
{
	if (offset_mode)
		writel(0x1, ctrl->zram_dec_base + ZRAM_DEC_CONTROL);
	else
		writel(0x0, ctrl->zram_dec_base + ZRAM_DEC_CONTROL);
}

/* A function provided from zram_engine.c to check fifo rtff result */
void engine_dec_self_check_before_kick(struct engine_control_t *ctrl);
void engine_enc_self_check_before_kick(struct engine_control_t *ctrl);

#if IS_ENABLED(CONFIG_MTK_VM_DEBUG)
void engine_dump_all_registers(struct engine_control_t *ctrl);
int engine_compare_all_registers(struct engine_control_t *ctrl);
#endif

#define LINE_SZ	(130)
int engine_get_enc_reg_status(struct engine_control_t *ctrl, char *buf, int buf_offset);
int engine_get_dec_reg_status(struct engine_control_t *ctrl, char *buf, int buf_offset);
void engine_get_smmu_reg_dump(struct engine_control_t *ctrl, struct seq_file *s);

/* Whether IRQ is available */
#define ENGINE_IRQ_ON	(0x1)	// Set when clk is enabled
#define ENGINE_IRQ_OFF	(0x0)	// Set when clk is disabled
static inline bool engine_enc_irq_off(struct engine_control_t *ctrl)
{
	return (atomic_read(&ctrl->enc_irq_status) == ENGINE_IRQ_OFF);
}

static inline bool engine_dec_irq_off(struct engine_control_t *ctrl)
{
	return (atomic_read(&ctrl->dec_irq_status) == ENGINE_IRQ_OFF);
}

static inline void engine_set_irq_on(struct engine_control_t *ctrl, bool enc_on, bool dec_on)
{
	/* Enable engine ENC interrupt */
	if (enc_on) {
		/* ENC ISR is allowed to work */
		atomic_set(&ctrl->enc_irq_status, ENGINE_IRQ_ON);
		writel(ctrl->enc_irq_setting, ctrl->zram_enc_base + ZRAM_ENC_IRQ_EN);
	}

	/* Enable engine DEC interrupt */
	if (dec_on) {
		/* DEC ISR is allowed to work */
		atomic_set(&ctrl->dec_irq_status, ENGINE_IRQ_ON);
		writel(ctrl->dec_irq_setting, ctrl->zram_dec_base + ZRAM_DEC_IRQ_EN);
	}
}

static inline void engine_set_irq_off(struct engine_control_t *ctrl, bool enc_off, bool dec_off)
{
	/* Disable engine ENC interrupt */
	if (enc_off) {
		writel(0x0, ctrl->zram_enc_base + ZRAM_ENC_IRQ_EN);
		/* ENC ISR is not allowed to work */
		atomic_set(&ctrl->enc_irq_status, ENGINE_IRQ_OFF);
		/* Clear avoid pending IRQs */
		writel(0x0, ctrl->zram_enc_base + ZRAM_ENC_IRQ_STATUS);
	}

	/* Disable engine DEC interrupt */
	if (dec_off) {
		writel(0x0, ctrl->zram_dec_base + ZRAM_DEC_IRQ_EN);
		/* DEC ISR is not allowed to work */
		atomic_set(&ctrl->dec_irq_status, ENGINE_IRQ_OFF);
		/* Clear avoid pending IRQs */
		writel(0x0, ctrl->zram_dec_base + ZRAM_DEC_IRQ_STATUS);
	}
}

/* ENC Interrupt Type */
#define ZRAM_ENC_BATCH_INTR_MASK	(1UL << 0)
#define ZRAM_ENC_IDLE_INTR_MASK		(1UL << 1)
#define ZRAM_ENC_ERROR_INTR_MASK	(1UL << 2)
#define ZRAM_ENC_FIFO_CMD_INTR_MASK	(1UL << 16)
static inline uint32_t engine_get_enc_irq_status(struct engine_control_t *ctrl)
{
	uint32_t status = readl(ctrl->zram_enc_base + ZRAM_ENC_IRQ_STATUS);

	/* Write 0 to clear */
	writel(0x0, ctrl->zram_enc_base + ZRAM_ENC_IRQ_STATUS);

	return status;
}

static inline void engine_set_enc_cmd_intr(struct engine_control_t *ctrl)
{
	uint32_t reg_val = readl(ctrl->zram_enc_base + ZRAM_ENC_IRQ_EN);

	reg_val |= ZRAM_ENC_FIFO_CMD_INTR_MASK;
	writel(reg_val, ctrl->zram_enc_base + ZRAM_ENC_IRQ_EN);
}

static inline void engine_clear_enc_cmd_intr(struct engine_control_t *ctrl)
{
	uint32_t reg_val = readl(ctrl->zram_enc_base + ZRAM_ENC_IRQ_EN);

	reg_val &= (~ZRAM_ENC_FIFO_CMD_INTR_MASK);
	writel(reg_val, ctrl->zram_enc_base + ZRAM_ENC_IRQ_EN);
}

/* DEC Interrupt Type */
#define ZRAM_DEC_BATCH_INTR_MASK		(1UL << 0)
#define ZRAM_DEC_IDLE_INTR_MASK			(1UL << 1)
#define ZRAM_DEC_ERROR_INTR_MASK		(1UL << 2)
#define ZRAM_DEC_FIFO_CMD_INTR_MASK		(1UL << 16)
#define ZRAM_DEC_ERROR_FIFO_ID_INTR_MASK	(1UL << 17)
#define ZRAM_DEC_KERNEL_HANG_INTR_MASK		(1UL << 18)
#define ZRAM_DEC_ERROR_MASKS			(ZRAM_DEC_ERROR_INTR_MASK | \
						ZRAM_DEC_ERROR_FIFO_ID_INTR_MASK | \
						ZRAM_DEC_KERNEL_HANG_INTR_MASK)
static inline uint32_t engine_get_dec_irq_status(struct engine_control_t *ctrl)
{
	uint32_t status = readl(ctrl->zram_dec_base + ZRAM_DEC_IRQ_STATUS);

	/* Write 0 to clear */
	writel(0x0, ctrl->zram_dec_base + ZRAM_DEC_IRQ_STATUS);

	return status;
}

static inline void engine_set_dec_cmd_intr(struct engine_control_t *ctrl)
{
	uint32_t reg_val = readl(ctrl->zram_dec_base + ZRAM_DEC_IRQ_EN);

	reg_val |= ZRAM_DEC_FIFO_CMD_INTR_MASK;
	writel(reg_val, ctrl->zram_dec_base + ZRAM_DEC_IRQ_EN);
}

static inline void engine_clear_dec_cmd_intr(struct engine_control_t *ctrl)
{
	uint32_t reg_val = readl(ctrl->zram_dec_base + ZRAM_DEC_IRQ_EN);

	reg_val &= (~ZRAM_DEC_FIFO_CMD_INTR_MASK);
	writel(reg_val, ctrl->zram_dec_base + ZRAM_DEC_IRQ_EN);
}

/* ENC Error Type: INST ERROR */
#define ZRAM_ENC_ERROR_INST_INST_IDLE_MASK		(1UL << 0)
#define ZRAM_ENC_ERROR_INST_INST_SETTING_ERR_MASK	(1UL << 1)
#define ZRAM_ENC_ERROR_INST_BUF_NOT_ENOUGH_MASK		(1UL << 2)
/* ENC Error Type: APB ERROR */
#define ZRAM_ENC_ERROR_APB_INST_FIFO_OVER_SIZE_MASK	(1UL << 0)
#define ZRAM_ENC_ERROR_APB_WRITE_IDX_OVER_RANGE_MASK	(1UL << 1)
#define ZRAM_ENC_ERROR_APB_BASE_ADDR_CONFIG_CHANGE_MASK	(1UL << 2)
#define ZRAM_ENC_ERROR_APB_FIFO_SIZE_CONFIG_CHANGE_MASK	(1UL << 3)
#define ZRAM_ENC_ERROR_APB_SW_LIMIT_OVER_SIZE_MASK	(1UL << 4)
#define ZRAM_ENC_ERROR_APB_WRITE_IDX_OVERWRITE_MASK	(1UL << 5)
static inline uint64_t engine_get_enc_error_status(struct engine_control_t *ctrl)
{
	struct engine_error_t eng_error;

	eng_error.inst_error = readl(ctrl->zram_enc_base + ZRAM_ENC_ERROR_TYPE_INST);
	eng_error.apb_error = readl(ctrl->zram_enc_base + ZRAM_ENC_ERROR_TYPE_APB);

	/* Write 0 to clear */
	writel(0x0, ctrl->zram_enc_base + ZRAM_ENC_ERROR_TYPE_INST);
	writel(0x0, ctrl->zram_enc_base + ZRAM_ENC_ERROR_TYPE_APB);

	return eng_error.error_status;
}

/* DEC Error Type: INST ERROR */
#define ZRAM_DEC_ERROR_INST_INST_IDLE_MASK		(1UL << 0)
#define ZRAM_DEC_ERROR_INST_INST_STATUS_ERR_MASK	(1UL << 1)
#define ZRAM_DEC_ERROR_INST_INST_SIZE_ERR_MASK		(1UL << 2)
#define ZRAM_DEC_ERROR_INST_INST_FIFO_ID_ERR_MASK	(1UL << 3)
/* DEC Error Type: APB ERROR */
#define ZRAM_DEC_ERROR_APB_INST_FIFO_OVER_SIZE_MASK	(1UL << 0)
#define ZRAM_DEC_ERROR_APB_WRITE_IDX_OVER_RANGE_MASK	(1UL << 1)
#define ZRAM_DEC_ERROR_APB_BASE_ADDR_CONFIG_CHANGE_MASK	(1UL << 2)
#define ZRAM_DEC_ERROR_APB_FIFO_SIZE_CONFIG_CHANGE_MASK	(1UL << 3)
static inline uint64_t engine_get_dec_error_status(struct engine_control_t *ctrl)
{
	struct engine_error_t eng_error;

	eng_error.inst_error = readl(ctrl->zram_dec_base + ZRAM_DEC_ERROR_TYPE_INST);
	eng_error.apb_error = readl(ctrl->zram_dec_base + ZRAM_DEC_ERROR_TYPE_APB);

	/* Write 0 to clear */
	writel(0x0, ctrl->zram_dec_base + ZRAM_DEC_ERROR_TYPE_INST);
	writel(0x0, ctrl->zram_dec_base + ZRAM_DEC_ERROR_TYPE_APB);

	return eng_error.error_status;
}

/* It can ONLY be called after updating write index */
static inline void engine_poll_cmd_complete(void __iomem *write_idx_reg,
		void __iomem *complete_idx_reg)
{
	uint32_t write_idx_reg_val = readl(write_idx_reg);
	uint32_t complete_idx_reg_val;

	/* Waiting for fifo empty (from HW view) */
	do {
		complete_idx_reg_val = readl(complete_idx_reg);
	} while (complete_idx_reg_val != write_idx_reg_val);
}

/* Bit mask to start engine (Same for both compression and decompression) */
#define ENGINE_START_MASK	(1UL << 31)

/* Kick engine */
static inline void engine_kick(void __iomem *write_idx_reg)
{
	uint32_t reg_val = readl(write_idx_reg);

	writel(ENGINE_START_MASK | reg_val, write_idx_reg);
}

/* Kick engine with idx */
static inline void engine_kick_with_idx(void __iomem *write_idx_reg, uint32_t hw_write_idx)
{
	writel(ENGINE_START_MASK | hw_write_idx, write_idx_reg);
}

/* Warm reset enc */
static inline void engine_enc_reset(struct engine_control_t *ctrl)
{
	writel(0x1, ctrl->zram_enc_base + ZRAM_ENC_RESET);
}

/* Warm reset dec */
static inline void engine_dec_reset(struct engine_control_t *ctrl)
{
	writel(0x1, ctrl->zram_dec_base + ZRAM_DEC_RESET);
}

/* Set offset index as complete index */
static inline void engine_set_offset_as_complete(void __iomem *offset_idx_reg, void __iomem *complete_idx_reg)
{
	uint32_t reg_val = readl(complete_idx_reg);

	writel(reg_val, offset_idx_reg);
}

/* Update offset index */
static inline void engine_set_offset_index(void __iomem *offset_idx_reg, uint32_t reg_val)
{
	writel(reg_val, offset_idx_reg);
}

/* Values for CLK set/clr */
#define ENGINE_CLK_DEC	(0x2)	/* bit[1] */
#define ENGINE_CLK_ENC	(0x4)	/* bit[2] */

/*
 * Enable clock for compression or decompression partially.
 * val -
 *      ENGINE_CLK_DEC: enable clk for decompression
 *      ENGINE_CLK_ENC: enable clk for compression
 */
static inline void engine_clock_partial_enable(struct engine_control_t *ctrl, uint32_t val)
{
	writel(val, ctrl->zram_config_base + ZRAM_CONFIG_ZRAM_CG_CLR0);
}

/*
 * Disable clock for compression or decompression partially
 * val -
 *      ENGINE_CLK_DEC: disable clk for decompression
 *      ENGINE_CLK_ENC: disable clk for compression
 */
static inline void engine_clock_partial_disable(struct engine_control_t *ctrl, uint32_t val)
{
	writel(val, ctrl->zram_config_base + ZRAM_CONFIG_ZRAM_CG_SET0);
}

/* Query SMMU TBU faulting address */
static inline uint64_t engine_get_smmu_faulting_addr(struct engine_control_t *ctrl)
{
	uint32_t reg_val;

	reg_val = readl(ctrl->zram_smmu_base + SMMU_TBU0_WTFM1);
	return ((uint64_t)(reg_val & 0xF) << 32) | (reg_val & 0xFFFFFFF0);
}

#endif /* _ENGINE_REGS_H_ */
