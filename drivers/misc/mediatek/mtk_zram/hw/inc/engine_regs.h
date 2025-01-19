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

// ---------------------- ZRAM_ENC Definitions ---------------------- //
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
void engine_smmu_destroy(struct platform_device *pdev);

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

void engine_enc_init(struct engine_control_t *ctrl, bool dst_copy);
void engine_dec_init(struct engine_control_t *ctrl, bool src_snoop);

void engine_setup_enc_main_fifo(struct engine_control_t *ctrl, phys_addr_t addr, unsigned int sz_bits);
void engine_setup_enc_second_fifo(struct engine_control_t *ctrl, phys_addr_t addr, unsigned int sz_bits);
void engine_setup_dec_fifo(struct engine_control_t *ctrl, unsigned int id, phys_addr_t addr, unsigned int sz_bits);

void engine_enc_debug_sel(struct engine_control_t *ctrl, uint32_t reg_val);
void engine_enc_debug_show(struct engine_control_t *ctrl);
void engine_enc_debug_show_more(struct engine_control_t *ctrl);

void engine_dec_debug_sel(struct engine_control_t *ctrl, uint32_t reg_val);
void engine_dec_debug_show(struct engine_control_t *ctrl);
void engine_dec_debug_show_more(struct engine_control_t *ctrl);

void engine_restore_enc_fifo_via_offset(struct engine_control_t *ctrl);
void engine_restore_dec_fifo_via_offset(struct engine_control_t *ctrl);

#if IS_ENABLED(CONFIG_MTK_VM_DEBUG)
void engine_dump_all_registers(struct engine_control_t *ctrl);
int engine_compare_all_registers(struct engine_control_t *ctrl);
#endif

int engine_get_reg_status(struct engine_control_t *ctrl, char *buf);

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

#endif /* _ENGINE_REGS_H_ */
