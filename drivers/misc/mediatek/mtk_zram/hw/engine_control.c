// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#include <linux/printk.h>
#include <linux/vmalloc.h>
#include <linux/seq_file.h>
#include <inc/engine_fifo.h>
#include <inc/engine_regs.h>
#include <inc/helpers.h>

#define zram_writel(val, reg) \
	writel(val, reg)
#define zram_readl(reg)	\
	readl(reg)

void engine_control_deinit(struct platform_device *pdev, struct engine_control_t *ctrl)
{
	struct device *dev = &pdev->dev;

	dev_info(dev, "%s\n", __func__);
}

/* Initialize register base addresses. Return 0 if success */
int engine_control_init(struct platform_device *pdev, struct engine_control_t *ctrl)
{
	struct device *dev = &pdev->dev;
	struct resource *res = NULL;

	dev_info(dev, "%s\n", __func__);

	if (!dev) {
		pr_info("%s: fail to find mtk hwzram device.\n", __func__);
		return -ENOENT;
	}

	/********************************/
	/* Set up register base address */
	/********************************/

	/* ZRAM_PM */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "zram_pm");
	if (!res) {
		pr_info("%s: fail to get resource ZRAM_PM.\n", __func__);
		return -ENOENT;
	}
	ctrl->zram_pm_base = devm_ioremap(dev, res->start, resource_size(res));
#ifdef ZRAM_ENGINE_DEBUG
	pr_info("ZRAM_PM: 0x%llx, 0x%llx (iomem: %llx)\n",
			res->start, resource_size(res), (unsigned long long)ctrl->zram_pm_base);
#endif
	if (!ctrl->zram_pm_base) {
		pr_info("fail to ioremap ZRAM_PM: 0x%llx", res->start);
		return -ENOMEM;
	}

	/* ZRAM_CONFIG */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "zram_config");
	if (!res) {
		pr_info("%s: fail to get resource ZRAM_CONFIG.\n", __func__);
		return -ENOENT;
	}
	ctrl->zram_config_base = devm_ioremap(dev, res->start, resource_size(res));
#ifdef ZRAM_ENGINE_DEBUG
	pr_info("ZRAM_CONFIG: 0x%llx, 0x%llx (iomem: %llx)\n",
			res->start, resource_size(res), (unsigned long long)ctrl->zram_config_base);
#endif
	if (!ctrl->zram_config_base) {
		pr_info("fail to ioremap ZRAM_CONFIG: 0x%llx", res->start);
		return -ENOMEM;
	}
#if IS_ENABLED(CONFIG_MTK_VM_DEBUG)
	ctrl->zram_config_res_sz = resource_size(res);
#endif

	/* ZRAM_SMMU */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "zram_smmu");
	if (!res) {
		pr_info("%s: fail to get resource ZRAM_SMMU.\n", __func__);
		return -ENOENT;
	}
	ctrl->zram_smmu_base = devm_ioremap(dev, res->start, resource_size(res));
#ifdef ZRAM_ENGINE_DEBUG
	pr_info("ZRAM_SMMU: 0x%llx, 0x%llx (iomem: %llx)\n",
			res->start, resource_size(res), (unsigned long long)ctrl->zram_smmu_base);
#endif
	if (!ctrl->zram_smmu_base) {
		pr_info("fail to ioremap ZRAM_SMMU: 0x%llx", res->start);
		return -ENOMEM;
	}

	/* ZRAM_DEC */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "zram_dec");
	if (!res) {
		pr_info("%s: fail to get resource ZRAM_DEC.\n", __func__);
		return -ENOENT;
	}
	ctrl->zram_dec_base = devm_ioremap(dev, res->start, resource_size(res));
#ifdef ZRAM_ENGINE_DEBUG
	pr_info("ZRAM_DEC: 0x%llx, 0x%llx (iomem: %llx)\n",
			res->start, resource_size(res), (unsigned long long)ctrl->zram_dec_base);
#endif
	if (!ctrl->zram_dec_base) {
		pr_info("fail to ioremap ZRAM_DEC: 0x%llx", res->start);
		return -ENOMEM;
	}
#if IS_ENABLED(CONFIG_MTK_VM_DEBUG)
	ctrl->zram_dec_res_sz = 0x4c0;
#endif

	/* ZRAM_ENC */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "zram_enc");
	if (!res) {
		pr_info("%s: fail to get resource ZRAM_ENC.\n", __func__);
		return -ENOENT;
	}
	ctrl->zram_enc_base = devm_ioremap(dev, res->start, resource_size(res));
#ifdef ZRAM_ENGINE_DEBUG
	pr_info("ZRAM_ENC: 0x%llx, 0x%llx (iomem: %llx)\n",
			res->start, resource_size(res), (unsigned long long)ctrl->zram_enc_base);
#endif
	if (!ctrl->zram_enc_base) {
		pr_info("fail to ioremap ZRAM_ENC: 0x%llx", res->start);
		return -ENOMEM;
	}
#if IS_ENABLED(CONFIG_MTK_VM_DEBUG)
	ctrl->zram_enc_res_sz = 0x38c;
#endif

	/* Set default IRQ as off */
	engine_set_irq_off(ctrl, true, true);

	dev_info(dev, "%s done\n", __func__);

	return 0;
}

/* Setup smmu relatives */
int engine_smmu_setup(struct platform_device *pdev, struct engine_control_t *ctrl)
{
	struct device *dev = &pdev->dev;
	struct device_node *dev_node = dev_of_node(dev);
	const char *str;
	struct iommu_domain *domain;
	u64 reg[2] = {0, 0};
	unsigned long iova;
	size_t size;
	phys_addr_t phys;
	int prot = IOMMU_READ | IOMMU_WRITE;
	int ret;

	/* Check the support of smmu s2 firstly */
	if (of_property_read_string(dev_node, "mtk,smmu-dma-mode", &str))
		str = "default";

	if (!strcmp(str, "disable")) {
		dev_info(dev, "Use SMMU S2 for ZRAM\n");
		ctrl->smmu_s2 = true;
		return 0;
	}

	/* Use smmu s1 */
	ctrl->smmu_s2 = false;

	/* Setup relatives for smmu s1 */
	domain = iommu_get_domain_for_dev(dev);
	if (!domain) {
		dev_info(dev, "no IOMMU domain found for ZRAM\n");
		return -ENOENT;
	}

	ret = of_property_read_u64_array(dev_node, "mtk,iommu-dma-range", reg, 2);
	if (ret < 0) {
		pr_info("%s - of_property_read_u64_array err : %d\n", __func__, ret);
		return -EINVAL;
	}

	iova = reg[0];
	size = reg[1];
	phys = reg[0];
	pr_info("%s - Setup identical IOMMU mapping from IOVA(0x%lx) to PA(0x%llx) with the range of (0x%lx)\n",
		__func__, iova, phys, size);

	/* Support coherence */
	if (!engine_coherence_disabled())
		prot |= IOMMU_CACHE;

	/* Create identical mapping for ZRAM with SMMU S1 */
	ret = iommu_map(domain, iova, phys, size, prot, GFP_KERNEL);
	if (ret) {
		pr_info("%s - iommu_map err : %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

/* Destroy smmu relatives */
void engine_smmu_destroy(struct platform_device *pdev, struct engine_control_t *ctrl)
{
	struct device *dev = &pdev->dev;
	struct device_node *dev_node = dev_of_node(dev);
	struct iommu_domain *domain;
	u64 reg[2] = {0, 0};
	unsigned long iova;
	size_t size;
	int ret;

	/* It's smmu s2, just return */
	if (ctrl->smmu_s2)
		return;

	domain = iommu_get_domain_for_dev(dev);
	if (!domain) {
		dev_info(dev, "no IOMMU domain found for ZRAM\n");
		return;
	}

	ret = of_property_read_u64_array(dev_node, "mtk,iommu-dma-range", reg, 2);
	if (ret < 0) {
		pr_info("%s - of_property_read_u64_array err : %d\n", __func__, ret);
		return;
	}

	iova = reg[0];
	size = reg[1];
	pr_info("%s - Destroy identical IOMMU mapping for IOVA(0x%lx) with the range of (0x%lx)\n",
		__func__, iova, size);

	/* Create identical mapping for ZRAM with SMMU S1 */
	size = iommu_unmap(domain, iova, size);
	pr_info("%s - iommu_unmap size (0x%lx)\n", __func__, size);
}

void engine_free_interrupts(struct platform_device *pdev, struct engine_control_t *ctrl,
		struct engine_irq_t *irqs, unsigned int count)
{
	struct device *dev = &pdev->dev;
	int i;

	dev_info(dev, "%s\n", __func__);

	if (!dev) {
		pr_info("%s: fail to find mtk hwzram device.\n", __func__);
		return;
	}

	/* Don't exceed the max possible count of engine interrupts */
	if (count > ENGINE_MAX_IRQ_COUNT)
		return;

	for (i = 0; i < count; i++) {
		// TODO: wait & clear irq status (?)

		devm_free_irq(dev, irqs[i].irq, irqs[i].priv);
	}
}

/* Return 0 if success */
int engine_request_interrupts(struct platform_device *pdev, struct engine_control_t *ctrl,
		struct engine_irq_t *irqs, unsigned int count)
{
	struct device *dev = &pdev->dev;
	int i, irq_id;
	int ret;

	dev_info(dev, "%s\n", __func__);

	if (!dev) {
		pr_info("%s: fail to find mtk hwzram device.\n", __func__);
		return -ENOENT;
	}

	/* Don't exceed the max possible count of engine interrupts */
	if (count > ENGINE_MAX_IRQ_COUNT)
		return -EINVAL;

	for (i = 0; i < count; i++) {
		irq_id = platform_get_irq_byname(pdev, irqs[i].name);
		if (irq_id < 0) {
			pr_info("%s: failed to get irq for (%s), err(%d)\n",
					__func__, irqs[i].name, irq_id);
			return -ENOENT;
		}

		ret = devm_request_irq(dev, irq_id, irqs[i].handler, irqs[i].flags,
				pdev->name, irqs[i].priv);
		if (ret) {
			pr_info("%s: failed to request irq for (%s:%d), err(%d)\n",
					__func__, irqs[i].name, irq_id, ret);
			return -ENOMEM;
		}

		irqs[i].irq = irq_id;
	}

	return 0;
}

#define ZRAM_SMMU_PROT_EN	(0x1UL << 3)
static inline void engine_wait_smmu_prot_off(struct engine_control_t *ctrl)
{
	void __iomem *reg = ctrl->zram_config_base + ZRAM_CONFIG_ZRAM_PWR_PROT_EN_0;
	uint32_t reg_val = zram_readl(reg);

	/* Disable SMMU prot */
	reg_val &= (~ZRAM_SMMU_PROT_EN);
	zram_writel(reg_val, reg);

	/* Wait for ready */
	do {
		reg_val = zram_readl(reg);
	} while ((reg_val & ZRAM_SMMU_PROT_EN) == ZRAM_SMMU_PROT_EN);

	reg = ctrl->zram_config_base + ZRAM_CONFIG_ZRAM_PWR_PROT_RDY_0;
	do {
		reg_val = zram_readl(reg);
	} while ((reg_val & ZRAM_SMMU_PROT_EN) == ZRAM_SMMU_PROT_EN);
}

static inline void engine_wait_smmu_prot_on(struct engine_control_t *ctrl)
{
	void __iomem *reg = ctrl->zram_config_base + ZRAM_CONFIG_ZRAM_PWR_PROT_EN_0;
	uint32_t reg_val = zram_readl(reg);

	/* Enable SMMU prot */
	reg_val |= ZRAM_SMMU_PROT_EN;
	zram_writel(reg_val, reg);

	/* Wait for ready */
	do {
		reg_val = zram_readl(reg);
	} while ((reg_val & ZRAM_SMMU_PROT_EN) != ZRAM_SMMU_PROT_EN);

	reg = ctrl->zram_config_base + ZRAM_CONFIG_ZRAM_PWR_PROT_RDY_0;
	do {
		reg_val = zram_readl(reg);
	} while ((reg_val & ZRAM_SMMU_PROT_EN) != ZRAM_SMMU_PROT_EN);
}

/* Power on only - no reference count */
#define ZRAM_SSYS_PWR_ON_OFF	(0x1UL << 4)
#define ZRAM_SSYS_RTFF_GRP_EN	(0xFUL << 8)
#define ZRAM_SSYS_PWR_ACK	(0x1UL << 31)
int engine_power_on(struct engine_control_t *ctrl)
{
	void __iomem *reg = ctrl->zram_pm_base + ZRAM_SSYSPM_CON;
	uint32_t reg_val = zram_readl(reg);

	/* POWER on */
	reg_val |= ZRAM_SSYS_PWR_ON_OFF;
	zram_writel(reg_val, reg);

	/* Waiting for ACK */
	do {
		reg_val = zram_readl(reg);
	} while ((reg_val & ZRAM_SSYS_PWR_ACK) != ZRAM_SSYS_PWR_ACK);

	/* Wait for SMMU prot off */
	engine_wait_smmu_prot_off(ctrl);

	/* Do self-check */
	engine_enc_self_check_before_kick(ctrl);
	engine_dec_self_check_before_kick(ctrl);

#ifdef ZRAM_ENGINE_DEBUG
	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);
#endif

	return 0;
}

/* Power off only - no reference count */
#define ZRAM_SSYS_SRAM_DORMANT_MASK	(~(0x7UL << 13))
void engine_power_off(struct engine_control_t *ctrl)
{
	void __iomem *reg = ctrl->zram_pm_base + ZRAM_SSYSPM_CON;
	uint32_t reg_val = zram_readl(reg);

	/* Wait for SMMU prot on */
	engine_wait_smmu_prot_on(ctrl);

	/* POWER off */
	reg_val |= ZRAM_SSYS_RTFF_GRP_EN;
	zram_writel(reg_val, reg);

	reg_val &= ZRAM_SSYS_SRAM_DORMANT_MASK;
	zram_writel(reg_val, reg);

	reg_val &= (~ZRAM_SSYS_PWR_ON_OFF);
	zram_writel(reg_val, reg);

	/* Waiting for ACK */
	do {
		reg_val = zram_readl(reg);
	} while ((reg_val & ZRAM_SSYS_PWR_ACK) == ZRAM_SSYS_PWR_ACK);

#ifdef ZRAM_ENGINE_DEBUG
	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);
#endif
}

#define RSC_BUS_PLL_REQ	(1UL << 14)
#define RSC_INFRA_REQ	(1UL << 15)
#define RSC_26M_REQ	(1UL << 16)
#define RSC_PMIC_REQ	(1UL << 17)
#define RSC_VCORE_REQ	(1UL << 18)
#define RSC_REQ_MASK	(RSC_BUS_PLL_REQ | RSC_INFRA_REQ | RSC_26M_REQ | RSC_PMIC_REQ | RSC_VCORE_REQ)

#define RSC_BUS_PLL_ACK	(1UL << 19)
#define RSC_INFRA_ACK	(1UL << 20)
#define RSC_26M_ACK	(1UL << 21)
#define RSC_PMIC_ACK	(1UL << 22)
#define RSC_VCORE_ACK	(1UL << 23)
#define RSC_ACK_MASK	(RSC_BUS_PLL_ACK | RSC_INFRA_ACK | RSC_26M_ACK | RSC_PMIC_ACK | RSC_VCORE_ACK)

/* Request vcore, pmic, 26m, infra, bus_pll */
int engine_request_resource(struct engine_control_t *ctrl)
{
	void __iomem *reg = ctrl->zram_enc_base + ZRAM_ENC_RESOURCE_SETTING;
	uint32_t reg_val, ack_status = 0x0;

	reg_val = zram_readl(reg);
	reg_val |= RSC_REQ_MASK;
	zram_writel(reg_val, reg);

	do {
		reg_val = zram_readl(reg);
		ack_status |= (reg_val & RSC_ACK_MASK);
	} while (ack_status != RSC_ACK_MASK);

	return 0;
}

/* Release vcore, pmic, 26m, infra, bus_pll */
void engine_release_resource(struct engine_control_t *ctrl)
{
	void __iomem *reg = ctrl->zram_enc_base + ZRAM_ENC_RESOURCE_SETTING;
	uint32_t reg_val, ack_status = RSC_ACK_MASK;

	reg_val = zram_readl(reg);
	reg_val &= ~RSC_REQ_MASK;
	zram_writel(reg_val, reg);

	do {
		reg_val = zram_readl(reg);
		ack_status &= (reg_val & RSC_ACK_MASK);
	} while (ack_status != 0x0);
}

#define ZRAM_CFG_CLK_GATING_MASK	((uint32_t)~0UL)
int engine_clock_init(struct engine_control_t *ctrl)
{
	/* No HW DCM */
	zram_writel(ZRAM_CFG_CLK_GATING_MASK, ctrl->zram_config_base + ZRAM_CONFIG_ZRAM_CG_CLR0);
	zram_writel(ZRAM_CFG_CLK_GATING_MASK, ctrl->zram_config_base + ZRAM_CONFIG_ZRAM_CG_CLR1);
	zram_writel(ZRAM_CFG_CLK_GATING_MASK, ctrl->zram_config_base + ZRAM_CONFIG_ZRAM_CG_CLR2);
	zram_writel(ZRAM_CFG_CLK_GATING_MASK, ctrl->zram_config_base + ZRAM_CONFIG_ZRAM_CG_CLR3);
	zram_writel(ZRAM_CFG_CLK_GATING_MASK, ctrl->zram_config_base + ZRAM_CONFIG_ZRAM_CG_CLR4);
	return 0;
}

/* (TODO) will be removed */
#define ZRAM_SMMU_S_GLB_CTL3	(0x1000C)
void engine_smmu_join(struct engine_control_t *ctrl)
{
	void __iomem *reg = ctrl->zram_smmu_base + ZRAM_SMMU_S_GLB_CTL3;
	uint32_t reg_val;

	zram_writel(0x0, reg);
	reg_val = zram_readl(reg);
	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);
}

void engine_smmu_bypass(struct engine_control_t *ctrl)
{
	void __iomem *reg = ctrl->zram_smmu_base + ZRAM_SMMU_S_GLB_CTL3;
	uint32_t reg_val;

	zram_writel(0x1, reg);
	reg_val = zram_readl(reg);
	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);
}

#ifdef REFERENCE_ONLY	/* For Reference Only */
#define ZRAM_SEC_CONFIG_SW0_RST	(0xD00)
#define ZRAM_RST_RELEASE_MASK	((uint32_t)~(0UL))
#define ZRAM_ENC_RST_MASK	((uint32_t)~(1UL << 2))
void engine_enc_reset(struct engine_control_t *ctrl)
{

#ifdef USE_SEC_REG	/* (TODO) will be removed */
	zram_writel(ZRAM_ENC_RST_MASK, ctrl->zram_sec_config_base + ZRAM_SEC_CONFIG_SW0_RST);
	zram_writel(ZRAM_RST_RELEASE_MASK, ctrl->zram_sec_config_base + ZRAM_SEC_CONFIG_SW0_RST);
#endif

	/* Reset write index to 0 */
	zram_writel(0, ctrl->zram_enc_base + ZRAM_ENC_CMD_MAIN_FIFO_WRITE_INDEX);
	zram_writel(0, ctrl->zram_enc_base + ZRAM_ENC_CMD_SECOND_FIFO_WRITE_INDEX);

	/* After reset, write and complete indices will be 0 */
}

#define ZRAM_DEC_RST_MASK	((uint32_t)~(1UL << 1))
void engine_dec_reset(struct engine_control_t *ctrl)
{
	unsigned int i;

#ifdef USE_SEC_REG	/* (TODO) will be removed */
	zram_writel(ZRAM_DEC_RST_MASK, ctrl->zram_sec_config_base + ZRAM_SEC_CONFIG_SW0_RST);
	zram_writel(ZRAM_RST_RELEASE_MASK, ctrl->zram_sec_config_base + ZRAM_SEC_CONFIG_SW0_RST);
#endif

	/* Reset write index to 0 */
	for (i = 0; i < MAX_DCOMP_NR; i++)
		zram_writel(0, ctrl->zram_dec_base + ZRAM_DEC_CMD_FIFO_0_WRITE_INDEX + (i * 4));

	/* After reset, write and complete indices will be 0 */
}
#endif

/* Wait for ENC Idle */
#define ZRAM_ENC_STATUS_IDLE_MASK	(0x1)
void engine_enc_wait_idle(struct engine_control_t *ctrl)
{
	void __iomem *reg = ctrl->zram_enc_base + ZRAM_ENC_STATUS;
	uint32_t reg_val;

	do {
		reg_val = zram_readl(reg);
	} while ((reg_val & ZRAM_ENC_STATUS_IDLE_MASK) != ZRAM_ENC_STATUS_IDLE_MASK);
}

/* Wait for ENC Idle with timeout */
int engine_enc_wait_idle_timeout(struct engine_control_t *ctrl, unsigned long timeout)
{
	void __iomem *reg = ctrl->zram_enc_base + ZRAM_ENC_STATUS;
	uint32_t reg_val;

	/* Update timeout according to jiffies */
	timeout = jiffies + msecs_to_jiffies(timeout);

	/* Polling idle */
	reg_val = zram_readl(reg);
	while ((reg_val & ZRAM_ENC_STATUS_IDLE_MASK) != ZRAM_ENC_STATUS_IDLE_MASK) {

		cpu_relax();
		if (time_after(jiffies, timeout))
			return -ETIME;

		reg_val = zram_readl(reg);
	}

	return 0;
}

/* Wait for DEC Idle */
#define ZRAM_DEC_STATUS_IDLE_MASK	(0x1)
void engine_dec_wait_idle(struct engine_control_t *ctrl)
{
	void __iomem *reg = ctrl->zram_dec_base + ZRAM_DEC_STATUS;
	uint32_t reg_val;

	do {
		reg_val = zram_readl(reg);
	} while ((reg_val & ZRAM_DEC_STATUS_IDLE_MASK) != ZRAM_DEC_STATUS_IDLE_MASK);
}

#define ZRAM_ENC_CFG_DST_STASHING_EN	(1UL << 4)
void engine_enc_init(struct engine_control_t *ctrl, bool dst_copy)
{
	uint32_t reg_val;

	/* Setting for engine without coherence */
	if (engine_coherence_disabled()) {
		zram_writel(0x7, ctrl->zram_enc_base + ZRAM_ENC_GMCIF_CON_READ_INSTN);
		zram_writel(0x7, ctrl->zram_enc_base + ZRAM_ENC_GMCIF_CON_READ_DATA);
		zram_writel(0x7, ctrl->zram_enc_base + ZRAM_ENC_GMCIF_CON_WRITE_INSTN);
		zram_writel(0x7, ctrl->zram_enc_base + ZRAM_ENC_GMCIF_CON_WRITE_DATA);
		goto next;
	}

	/* Setting for engine with coherence */
	zram_writel(0x2007, ctrl->zram_enc_base + ZRAM_ENC_GMCIF_CON_READ_INSTN);
	zram_writel(0x2007, ctrl->zram_enc_base + ZRAM_ENC_GMCIF_CON_READ_DATA);
	zram_writel(0x2007, ctrl->zram_enc_base + ZRAM_ENC_GMCIF_CON_WRITE_INSTN);

	/* DST stashing */
	if (dst_copy)
		zram_writel(0x2007, ctrl->zram_enc_base + ZRAM_ENC_GMCIF_CON_WRITE_DATA);
	else
		zram_writel(0x7, ctrl->zram_enc_base + ZRAM_ENC_GMCIF_CON_WRITE_DATA);

next:
	/* General config setting */
	if (dst_copy)
		zram_writel(0x1000, ctrl->zram_enc_base + ZRAM_ENC_SW_LIMIT);
	else
		zram_writel(0xf00, ctrl->zram_enc_base + ZRAM_ENC_SW_LIMIT);

	reg_val = ENGINE_COMP_BATCH_INTR_CNT_BITS;
	if (dst_copy) {
		pr_info("%s: it needs dst copy!\n", __func__);
		/* CURRENTLY only for ENGINE_BUF_ENABLE == 0x40. */
		reg_val |= ZRAM_ENC_CFG_DST_STASHING_EN;
	}
	zram_writel(reg_val, ctrl->zram_enc_base + ZRAM_ENC_CFG);

	/* FIFO Threshold setting */
	zram_writel(0x12561256, ctrl->zram_enc_base + ZRAM_ENC_FIFO_THRESHOLD_READ_INSTN_PREULTRA);
	zram_writel(0x005a01b2, ctrl->zram_enc_base + ZRAM_ENC_FIFO_THRESHOLD_READ_DATA_PREULTRA_0);
	zram_writel(0x005a01b2, ctrl->zram_enc_base + ZRAM_ENC_FIFO_THRESHOLD_READ_DATA_PREULTRA_1);
	zram_writel(0x005a01b2, ctrl->zram_enc_base + ZRAM_ENC_FIFO_THRESHOLD_READ_DATA_PREULTRA_2);
	zram_writel(0x005a01b2, ctrl->zram_enc_base + ZRAM_ENC_FIFO_THRESHOLD_READ_DATA_PREULTRA_3);
	zram_writel(0x002400ad, ctrl->zram_enc_base + ZRAM_ENC_FIFO_THRESHOLD_WRITE_INSTN_PREULTRA);
	zram_writel(0x00b40364, ctrl->zram_enc_base + ZRAM_ENC_FIFO_THRESHOLD_WRITE_PAGE_PREULTRA);

	/* DDREN HW mode setting */
	zram_writel(0x0006c040, ctrl->zram_enc_base + ZRAM_ENC_RESOURCE_SETTING);

	/* IRQ setting: Do support of batch & idle interrupt currently */
	reg_val = ZRAM_ENC_ERROR_INTR_MASK;

	/* Keep IRQ setting for future configuration */
	ctrl->enc_irq_setting = reg_val;
}

#define ZRAM_DEC_CFG_DST_SNOOPING_EN	(1UL << 4)
void engine_dec_init(struct engine_control_t *ctrl, bool src_snoop)
{
	uint32_t reg_val;

	/* Setting for engine without coherence */
	if (engine_coherence_disabled()) {
		zram_writel(0x7, ctrl->zram_dec_base + ZRAM_DEC_GMCIF_CON_READ_CMD);
		zram_writel(0x7, ctrl->zram_dec_base + ZRAM_DEC_GMCIF_CON_READ_DATA);
		zram_writel(0x7, ctrl->zram_dec_base + ZRAM_DEC_GMCIF_CON_WRITE_CMD);
		zram_writel(0x7, ctrl->zram_dec_base + ZRAM_DEC_GMCIF_CON_WRITE_DATA);
		goto next;
	}

	/* Setting these configurations according to src_snoop */
	if (src_snoop) {
		reg_val = zram_readl(ctrl->zram_dec_base + ZRAM_DEC_GMCIF_CON_READ_DATA);
		zram_writel(reg_val | 0x2007, ctrl->zram_dec_base + ZRAM_DEC_GMCIF_CON_READ_DATA);
	} else {
		zram_writel(0x7, ctrl->zram_dec_base + ZRAM_DEC_GMCIF_CON_READ_DATA);
	}

	reg_val = zram_readl(ctrl->zram_dec_base + ZRAM_DEC_GMCIF_CON_READ_CMD);
	zram_writel(reg_val | 0x2007, ctrl->zram_dec_base + ZRAM_DEC_GMCIF_CON_READ_CMD);

	reg_val = zram_readl(ctrl->zram_dec_base + ZRAM_DEC_GMCIF_CON_WRITE_DATA);
	zram_writel(reg_val | 0x2007, ctrl->zram_dec_base + ZRAM_DEC_GMCIF_CON_WRITE_DATA);

	reg_val = zram_readl(ctrl->zram_dec_base + ZRAM_DEC_GMCIF_CON_WRITE_CMD);
	zram_writel(reg_val | 0x2007, ctrl->zram_dec_base + ZRAM_DEC_GMCIF_CON_WRITE_CMD);

next:
	/* General config setting */
	reg_val = ENGINE_DCOMP_BATCH_INTR_CNT_BITS;
	if (src_snoop) {
		pr_info("%s: it needs src snoop for dst copy!\n", __func__);
		/* CURRENTLY only for ENGINE_BUF_ENABLE == 0x40. */
		reg_val |= ZRAM_DEC_CFG_DST_SNOOPING_EN;
	}
	zram_writel(reg_val, ctrl->zram_dec_base + ZRAM_DEC_CFG);

	/* Don't support batch & idle interrupt for decompression */
	reg_val = ZRAM_DEC_ERROR_INTR_MASK;
	reg_val |= ZRAM_DEC_FIFO_CMD_INTR_MASK;
	reg_val |= ZRAM_DEC_ERROR_FIFO_ID_INTR_MASK;
	reg_val |= ZRAM_DEC_KERNEL_HANG_INTR_MASK;

	/* Keep IRQ setting for future configuration */
	ctrl->dec_irq_setting = reg_val;
}

void engine_setup_enc_fifo(struct engine_control_t *ctrl, unsigned int id, phys_addr_t addr, unsigned int sz_bits)
{
	void __iomem *reg;
	uint32_t reg_val;

	if (!IS_ALIGNED(addr, SZ_4K)) {
		pr_info("%s: addr (0x%llx) is not 4K aligned.\n", __func__, addr);
		return;
	}

	if (sz_bits > ENGINE_COMP_FIFO_MAX_ENTRY_BITS) {
		pr_info("%s: sz_bits (%u) is too large.\n", __func__, sz_bits);
		return;
	}

	if (id >= MAX_COMP_NR) {
		pr_info("%s: id (%u) is too large.\n", __func__, id);
		return;
	}

	reg = ctrl->zram_enc_base + ZRAM_ENC_CMD_MAIN_FIFO_CONFIG + (id * 4);
	reg_val = (addr >> 7) | sz_bits;
	zram_writel(reg_val, reg);

	pr_info("%s: ID(%u) REG(%lx) VAL(%x)\n", __func__, id, (unsigned long)reg, (uint32_t)reg_val);
}

void engine_setup_dec_fifo(struct engine_control_t *ctrl, unsigned int id, phys_addr_t addr, unsigned int sz_bits)
{
	void __iomem *reg;
	uint32_t reg_val;

	if (!IS_ALIGNED(addr, SZ_4K)) {
		pr_info("%s: addr (0x%llx) is not 4K aligned.\n", __func__, addr);
		return;
	}

	if (sz_bits > ENGINE_DCOMP_FIFO_MAX_ENTRY_BITS) {
		pr_info("%s: sz_bits (%u) is too large.\n", __func__, sz_bits);
		return;
	}

	if (id >= MAX_DCOMP_NR) {
		pr_info("%s: id (%u) is too large.\n", __func__, id);
		return;
	}

	reg = ctrl->zram_dec_base + ZRAM_DEC_CMD_FIFO_CONFIG_0 + (id * 4);
	reg_val = (addr >> 7) | sz_bits;
	zram_writel(reg_val, reg);

	pr_info("%s: ID(%u) REG(%lx) VAL(%x)\n", __func__, id, (unsigned long)reg, (uint32_t)reg_val);
}

void engine_reset_enc_indices(struct engine_control_t *ctrl)
{
	/* Do warm reset & wait for idle */
	engine_enc_reset(ctrl);
	engine_enc_wait_idle(ctrl);

	/* 1. Change to offset index mode */
	zram_writel(0x1, ctrl->zram_enc_base + ZRAM_ENC_CONTROL);

	/* 2. Update offset indices */
	zram_writel(0x0, ctrl->zram_enc_base + ZRAM_ENC_CMD_MAIN_FIFO_OFFSET_INDEX);
	zram_writel(0x0, ctrl->zram_enc_base + ZRAM_ENC_CMD_MAIN_FIFO_WRITE_INDEX);
	zram_writel(0x0, ctrl->zram_enc_base + ZRAM_ENC_CMD_SECOND_FIFO_OFFSET_INDEX);
	zram_writel(0x0, ctrl->zram_enc_base + ZRAM_ENC_CMD_SECOND_FIFO_WRITE_INDEX);

	/* 3. Engine start */
	zram_writel(ENGINE_START_MASK, ctrl->zram_enc_base + ZRAM_ENC_CMD_MAIN_FIFO_WRITE_INDEX);
	engine_enc_wait_idle(ctrl);

	/* 4. Back to complete index mode */
	zram_writel(0x0, ctrl->zram_enc_base + ZRAM_ENC_CONTROL);
	engine_enc_wait_idle(ctrl);
}

void engine_reset_dec_indices(struct engine_control_t *ctrl)
{
	int i;

	/* Do warm reset & wait for idle */
	engine_dec_reset(ctrl);
	engine_dec_wait_idle(ctrl);

	/* 1. Change to offset index mode */
	zram_writel(0x1, ctrl->zram_dec_base + ZRAM_DEC_CONTROL);

	/* 2. Update offset indices */
	for (i = 0; i < MAX_DCOMP_NR; i++) {
		zram_writel(0x0, ctrl->zram_dec_base + ZRAM_DEC_CMD_FIFO_0_OFFSET_INDEX + (i * 4));
		zram_writel(0x0, ctrl->zram_dec_base + ZRAM_DEC_CMD_FIFO_0_WRITE_INDEX + (i * 4));
	}

	/* 3. Engine start */
	zram_writel(ENGINE_START_MASK, ctrl->zram_dec_base + ZRAM_DEC_CMD_FIFO_0_WRITE_INDEX);
	engine_dec_wait_idle(ctrl);

	/* 4. Back to complete index mode */
	zram_writel(0x0, ctrl->zram_dec_base + ZRAM_DEC_CONTROL);
	engine_dec_wait_idle(ctrl);
}

void engine_reset_all_indices(struct engine_control_t *ctrl)
{
	engine_reset_enc_indices(ctrl);
	engine_reset_dec_indices(ctrl);
}

void engine_enc_debug_sel(struct engine_control_t *ctrl, uint32_t reg_val)
{
	void __iomem *reg = ctrl->zram_enc_base + ZRAM_ENC_DEBUG_CON;

	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);
	zram_writel(reg_val, reg);
}

void engine_enc_debug_show(struct engine_control_t *ctrl)
{
	void __iomem *reg;
	uint32_t reg_val;

	reg = ctrl->zram_enc_base + ZRAM_ENC_DEBUG_REG;
	reg_val = zram_readl(reg);
	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);
}

void engine_enc_debug_show_more(struct engine_control_t *ctrl)
{
	void __iomem *reg;
	uint32_t reg_val;

	reg = ctrl->zram_enc_base + ZRAM_ENC_CMD_MAIN_FIFO_COMPLETE_INDEX;
	reg_val = zram_readl(reg);
	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);

	reg = ctrl->zram_enc_base + ZRAM_ENC_CMD_SECOND_FIFO_COMPLETE_INDEX;
	reg_val = zram_readl(reg);
	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);

	reg = ctrl->zram_enc_base + ZRAM_ENC_DBG_1;
	reg_val = zram_readl(reg);
	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);

	reg = ctrl->zram_enc_base + ZRAM_ENC_DBG_2;
	reg_val = zram_readl(reg);
	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);

	reg = ctrl->zram_enc_base + ZRAM_ENC_DBG_3;
	reg_val = zram_readl(reg);
	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);

	reg = ctrl->zram_enc_base + ZRAM_ENC_IRQ_STATUS;
	reg_val = zram_readl(reg);
	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);

	reg = ctrl->zram_enc_base + ZRAM_ENC_ERROR_TYPE_INST;
	reg_val = zram_readl(reg);
	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);

	reg = ctrl->zram_enc_base + ZRAM_ENC_ERROR_TYPE_APB;
	reg_val = zram_readl(reg);
	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);

	reg = ctrl->zram_enc_base + ZRAM_ENC_RESOURCE_SETTING;
	reg_val = zram_readl(reg);
	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);
}

void engine_dec_debug_sel(struct engine_control_t *ctrl, uint32_t reg_val)
{
	void __iomem *reg = ctrl->zram_dec_base + ZRAM_DEC_DEBUG_CON;

	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);
	zram_writel(reg_val, reg);
}

void engine_dec_debug_show(struct engine_control_t *ctrl)
{
	void __iomem *reg;
	uint32_t reg_val;

	reg = ctrl->zram_dec_base + ZRAM_DEC_DEBUG_REG;
	reg_val = zram_readl(reg);
	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);
}

void engine_dec_debug_show_more(struct engine_control_t *ctrl)
{
	void __iomem *reg;
	uint32_t reg_val;

	reg = ctrl->zram_dec_base + ZRAM_CONFIG_ZRAM_CG_CON0;
	reg_val = zram_readl(reg);
	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);

	reg = ctrl->zram_dec_base + ZRAM_CONFIG_ZRAM_CG_CON1;
	reg_val = zram_readl(reg);
	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);

	reg = ctrl->zram_dec_base + ZRAM_CONFIG_ZRAM_CG_CON2;
	reg_val = zram_readl(reg);
	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);

	reg = ctrl->zram_dec_base + ZRAM_CONFIG_ZRAM_CG_CON3;
	reg_val = zram_readl(reg);
	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);

	reg = ctrl->zram_dec_base + ZRAM_CONFIG_ZRAM_CG_CON4;
	reg_val = zram_readl(reg);
	pr_info("%s: REG(%lx) VAL(%x)\n", __func__, (unsigned long)reg, (uint32_t)reg_val);
}

#if IS_ENABLED(CONFIG_MTK_VM_DEBUG)
static struct {
	uint32_t offset;
	uint32_t reg_val;
} *register_dump = NULL;

void engine_dump_all_registers(struct engine_control_t *ctrl)
{
	resource_size_t total_res_sz;
	int i, j = 0;
	void __iomem *reg;

	if (register_dump == NULL) {

		/* Total register range */
		total_res_sz = ctrl->zram_config_res_sz +
			ctrl->zram_dec_res_sz +
			ctrl->zram_enc_res_sz;

		register_dump = vzalloc(sizeof(*register_dump) * (total_res_sz / sizeof(uint32_t)));
		if (!register_dump) {
			pr_info("%s: no memory.\n", __func__);
			return;
		}
	}

	/* Record register values */
	for (i = 0; i < ctrl->zram_config_res_sz; i += sizeof(uint32_t)) {
		reg = ctrl->zram_config_base + i;
		register_dump[j].reg_val = i;
		register_dump[j].reg_val = zram_readl(reg);
		j++;
	}
	for (i = 0; i < ctrl->zram_dec_res_sz; i += sizeof(uint32_t)) {
		reg = ctrl->zram_dec_base + i;
		register_dump[j].reg_val = i;
		register_dump[j].reg_val = zram_readl(reg);
		j++;
	}
	for (i = 0; i < ctrl->zram_enc_res_sz; i += sizeof(uint32_t)) {
		reg = ctrl->zram_enc_base + i;
		register_dump[j].reg_val = i;
		register_dump[j].reg_val = zram_readl(reg);
		j++;
	}

	pr_info("%s: Record done.\n", __func__);
}

/* Return 0 if comparison pass */
int engine_compare_all_registers(struct engine_control_t *ctrl)
{
	resource_size_t total_res_sz;
	int i, j = 0, mismatch = 0, ret = 0;
	void __iomem *reg;
	uint32_t reg_val;

	if (register_dump == NULL)
		return -ENOMEM;

	/* Total register range */
	total_res_sz = ctrl->zram_config_res_sz +
		ctrl->zram_dec_res_sz +
		ctrl->zram_enc_res_sz;

	/* Start comparison with existing records */
	for (i = 0; i < ctrl->zram_config_res_sz; i += sizeof(uint32_t)) {
		reg = ctrl->zram_config_base + i;
		reg_val = zram_readl(reg);
		if (reg_val != register_dump[j].reg_val) {
			pr_info("%s:[0x%x] is (%u), not (%u)\n", "zram_config", i, reg_val, register_dump[j].reg_val);
			mismatch++;
		}
		j++;
	}

	for (i = 0; i < ctrl->zram_dec_res_sz; i += sizeof(uint32_t)) {
		reg = ctrl->zram_dec_base + i;
		reg_val = zram_readl(reg);
		if (reg_val != register_dump[j].reg_val) {
			pr_info("%s:[0x%x] is (%u), not (%u)\n", "zram_dec", i, reg_val, register_dump[j].reg_val);
			mismatch++;
		}
		j++;
	}

	for (i = 0; i < ctrl->zram_enc_res_sz; i += sizeof(uint32_t)) {
		reg = ctrl->zram_enc_base + i;
		reg_val = zram_readl(reg);
		if (reg_val != register_dump[j].reg_val) {
			pr_info("%s:[0x%x] is (%u), not (%u)\n", "zram_enc", i, reg_val, register_dump[j].reg_val);
			mismatch++;
		}
		j++;
	}

	/* Dump final result */
	if (j != (total_res_sz / sizeof(uint32_t))) {
		pr_info("%s: (%d) compared, not equal to total_res (%llu)\n",
			__func__, j, (total_res_sz / sizeof(uint32_t)));
		ret = -1;
		goto exit;
	}

	pr_info("%s: (%d) compared\n", __func__, j);

	if (mismatch != 0) {
		pr_info("%s: Fail. (%d) mismatches\n", __func__, mismatch);
		ret = -2;
		goto exit;
	}

	pr_info("%s: Pass.\n", __func__);

exit:
	return ret;
}
#endif

/* Register dump for compression */
int engine_get_enc_reg_status(struct engine_control_t *ctrl, char *buf, int buf_offset)
{
	void __iomem *reg;
	uint32_t reg_val, reg_set;
	int i;
	int copied = buf_offset;
	char output[LINE_SZ];
	int offset;

	/*
	 * enc register dump
	 */

	offset = 0;
	for (i = 0; i < 0x100; i += sizeof(uint32_t)) {
		reg = ctrl->zram_enc_base + i;
		reg_val = zram_readl(reg);
		offset += snprintf(output + offset, LINE_SZ - offset, "%2x:0x%-8x", i, reg_val);
		if (((i + sizeof(uint32_t)) % 0x20) == 0) {
			/* do output */
			ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "%s\n", output);
			offset = 0;
		} else {
			offset += snprintf(output + offset, LINE_SZ - offset, " ");
		}
	}

	/*
	 * enc debug register dump
	 */

	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "(c8)-(cc)\n");
	offset = 0;
	reg_set = 0x00080000;
	for (i = 0; i < 16; i++) {
		zram_writel(reg_set, ctrl->zram_enc_base + 0xc8);
		reg_val = zram_readl(ctrl->zram_enc_base + 0xcc);
		offset += snprintf(output + offset, LINE_SZ - offset, "%x-%x", reg_set, reg_val);
		reg_set += 0x10000;

		/* newline or space */
		if (((i + 1) % 4) == 0) {
			/* do output */
			ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "%s\n", output);
			offset = 0;
		} else {
			offset += snprintf(output + offset, LINE_SZ - offset, " ");
		}
	}

	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "(c8)-(d0)\n");
	offset = 0;
	reg_set = 0x00180000;
	for (i = 0; i < 16; i++) {
		zram_writel(reg_set, ctrl->zram_enc_base + 0xc8);
		reg_val = zram_readl(ctrl->zram_enc_base + 0xd0);
		offset += snprintf(output + offset, LINE_SZ - offset, "%x-%x ", reg_set, reg_val);
		reg_set += 0x4;
		zram_writel(reg_set, ctrl->zram_enc_base + 0xc8);
		reg_val = zram_readl(ctrl->zram_enc_base + 0xd0);
		offset += snprintf(output + offset, LINE_SZ - offset, "%x-%x ", reg_set, reg_val);
		reg_set += 0x15;
		zram_writel(reg_set, ctrl->zram_enc_base + 0xc8);
		reg_val = zram_readl(ctrl->zram_enc_base + 0xd0);
		offset += snprintf(output + offset, LINE_SZ - offset, "%x-%x", reg_set, reg_val);
		reg_set += 0x7;

		/* do output */
		ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "%s\n", output);
		offset = 0;
	}

	reg_val = zram_readl(ctrl->zram_enc_base + 0x504);
	offset += snprintf(output + offset, LINE_SZ - offset, "504-%x ", reg_val);
	reg_val = zram_readl(ctrl->zram_enc_base + 0x508);
	offset += snprintf(output + offset, LINE_SZ - offset, "508-%x ", reg_val);
	reg_val = zram_readl(ctrl->zram_enc_base + 0x50c);
	offset += snprintf(output + offset, LINE_SZ - offset, "50c-%x", reg_val);

	/* do output */
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "%s\n", output);

	/* The last line */
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "\n");
	return copied;
}

/* Register dump for decompression */
int engine_get_dec_reg_status(struct engine_control_t *ctrl, char *buf, int buf_offset)
{
	void __iomem *reg;
	uint32_t reg_val, reg_set;
	int i;
	int copied = buf_offset;
	char output[LINE_SZ];
	int offset;

	/*
	 * dec register dump
	 */

	offset = 0;
	for (i = 0; i < 0x100; i += sizeof(uint32_t)) {
		reg = ctrl->zram_dec_base + i;
		reg_val = zram_readl(reg);
		offset += snprintf(output + offset, LINE_SZ - offset, "%3x:0x%-8x", i, reg_val);
		if (((i + sizeof(uint32_t)) % 0x20) == 0) {
			/* do output */
			ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "%s\n", output);
			offset = 0;
		} else {
			offset += snprintf(output + offset, LINE_SZ - offset, " ");
		}
	}

	/*
	 * dec debug register dump
	 */

	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "(c8)-(cc)\n");
	offset = 0;
	reg_set = 0x00000000;
	for (i = 0; i < 4; i++) {
		zram_writel(reg_set, ctrl->zram_dec_base + 0xc8);
		reg_val = zram_readl(ctrl->zram_dec_base + 0xcc);
		offset += snprintf(output + offset, LINE_SZ - offset, "%x-%x", reg_set, reg_val);
		reg_set += 0x10000;

		/* Space */
		if (((i + 1) % 4) != 0)
			offset += snprintf(output + offset, LINE_SZ - offset, " ");
	}
	/* do output */
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "%s\n", output);
	offset = 0;

	reg_set = 0x00100000;
	for (i = 0; i < 4; i++) {
		zram_writel(reg_set, ctrl->zram_dec_base + 0xc8);
		reg_val = zram_readl(ctrl->zram_dec_base + 0xcc);
		offset += snprintf(output + offset, LINE_SZ - offset, "%x-%x", reg_set, reg_val);
		reg_set += 0x10000;

		/* Space */
		if (((i + 1) % 4) != 0)
			offset += snprintf(output + offset, LINE_SZ - offset, " ");
	}
	/* do output */
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "%s\n", output);
	offset = 0;

	reg_set = 0x00160000;
	for (i = 0; i < 4; i++) {
		zram_writel(reg_set, ctrl->zram_dec_base + 0xc8);
		reg_val = zram_readl(ctrl->zram_dec_base + 0xcc);
		offset += snprintf(output + offset, LINE_SZ - offset, "%x-%x", reg_set, reg_val);
		reg_set += 0x10000;

		/* Space */
		if (((i + 1) % 4) != 0)
			offset += snprintf(output + offset, LINE_SZ - offset, " ");
	}
	/* do output */
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "%s\n", output);
	offset = 0;

	reg_set = 0x00280000;
	for (i = 0; i < 4; i++) {
		zram_writel(reg_set, ctrl->zram_dec_base + 0xc8);
		reg_val = zram_readl(ctrl->zram_dec_base + 0xcc);
		offset += snprintf(output + offset, LINE_SZ - offset, "%x-%x", reg_set, reg_val);
		reg_set += 0x10000;

		/* Space */
		if (((i + 1) % 4) != 0)
			offset += snprintf(output + offset, LINE_SZ - offset, " ");
	}
	/* do output */
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "%s\n", output);
	offset = 0;

	/* Others */
	zram_writel(0x00050000, ctrl->zram_dec_base + 0xc8);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xcc);
	offset += snprintf(output + offset, LINE_SZ - offset, "50000-%x ", reg_val);
	zram_writel(0x00090000, ctrl->zram_dec_base + 0xc8);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xcc);
	offset += snprintf(output + offset, LINE_SZ - offset, "90000-%x ", reg_val);
	zram_writel(0x0000000c, ctrl->zram_dec_base + 0xc8);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xcc);
	offset += snprintf(output + offset, LINE_SZ - offset, "c-%x ", reg_val);
	zram_writel(0x0000000e, ctrl->zram_dec_base + 0xc8);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xcc);
	offset += snprintf(output + offset, LINE_SZ - offset, "e-%x", reg_val);
	/* do output */
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "%s\n", output);
	offset = 0;

	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "(c8)-(d0)\n");
	offset = 0;
	zram_writel(0x00000001, ctrl->zram_dec_base + 0xc8);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xd0);
	offset += snprintf(output + offset, LINE_SZ - offset, "1-%x ", reg_val);
	zram_writel(0x00000002, ctrl->zram_dec_base + 0xc8);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xd0);
	offset += snprintf(output + offset, LINE_SZ - offset, "2-%x ", reg_val);
	zram_writel(0x00000008, ctrl->zram_dec_base + 0xc8);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xd0);
	offset += snprintf(output + offset, LINE_SZ - offset, "8-%x ", reg_val);
	zram_writel(0x00000009, ctrl->zram_dec_base + 0xc8);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xd0);
	offset += snprintf(output + offset, LINE_SZ - offset, "9-%x ", reg_val);
	zram_writel(0x0000000a, ctrl->zram_dec_base + 0xc8);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xd0);
	offset += snprintf(output + offset, LINE_SZ - offset, "a-%x", reg_val);
	/* do output */
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "%s\n", output);
	offset = 0;
	zram_writel(0x00000000, ctrl->zram_dec_base + 0xc8);  // must

	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "(404)-(d0)\n");
	offset = 0;
	zram_writel(0x00000001, ctrl->zram_dec_base + 0x404);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xd0);
	offset += snprintf(output + offset, LINE_SZ - offset, "1-%x ", reg_val);
	zram_writel(0x00000002, ctrl->zram_dec_base + 0x404);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xd0);
	offset += snprintf(output + offset, LINE_SZ - offset, "2-%x ", reg_val);
	zram_writel(0x00000008, ctrl->zram_dec_base + 0x404);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xd0);
	offset += snprintf(output + offset, LINE_SZ - offset, "8-%x ", reg_val);
	zram_writel(0x00000009, ctrl->zram_dec_base + 0x404);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xd0);
	offset += snprintf(output + offset, LINE_SZ - offset, "9-%x ", reg_val);
	zram_writel(0x0000000a, ctrl->zram_dec_base + 0x404);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xd0);
	offset += snprintf(output + offset, LINE_SZ - offset, "a-%x", reg_val);
	/* do output */
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "%s\n", output);
	offset = 0;
	zram_writel(0x00000000, ctrl->zram_dec_base + 0x404);	// must

	zram_writel(0x00010000, ctrl->zram_dec_base + 0x404);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xd0);
	offset += snprintf(output + offset, LINE_SZ - offset, "10000-%x ", reg_val);
	zram_writel(0x00020000, ctrl->zram_dec_base + 0x404);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xd0);
	offset += snprintf(output + offset, LINE_SZ - offset, "20000-%x ", reg_val);
	zram_writel(0x00080000, ctrl->zram_dec_base + 0x404);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xd0);
	offset += snprintf(output + offset, LINE_SZ - offset, "80000-%x ", reg_val);
	zram_writel(0x00090000, ctrl->zram_dec_base + 0x404);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xd0);
	offset += snprintf(output + offset, LINE_SZ - offset, "90000-%x ", reg_val);
	zram_writel(0x000a0000, ctrl->zram_dec_base + 0x404);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xd0);
	offset += snprintf(output + offset, LINE_SZ - offset, "a0000-%x", reg_val);
	/* do output */
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "%s\n", output);
	offset = 0;
	zram_writel(0x00000000, ctrl->zram_dec_base + 0x404);	// must

	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "(408)-(d0)\n");
	offset = 0;
	zram_writel(0x00000001, ctrl->zram_dec_base + 0x408);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xd0);
	offset += snprintf(output + offset, LINE_SZ - offset, "1-%x ", reg_val);
	zram_writel(0x00000002, ctrl->zram_dec_base + 0x408);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xd0);
	offset += snprintf(output + offset, LINE_SZ - offset, "2-%x ", reg_val);
	zram_writel(0x00000008, ctrl->zram_dec_base + 0x408);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xd0);
	offset += snprintf(output + offset, LINE_SZ - offset, "8-%x ", reg_val);
	zram_writel(0x00000009, ctrl->zram_dec_base + 0x408);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xd0);
	offset += snprintf(output + offset, LINE_SZ - offset, "9-%x ", reg_val);
	zram_writel(0x0000000a, ctrl->zram_dec_base + 0x408);
	reg_val = zram_readl(ctrl->zram_dec_base + 0xd0);
	offset += snprintf(output + offset, LINE_SZ - offset, "a-%x", reg_val);
	/* do output */
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "%s\n", output);
	offset = 0;
	zram_writel(0x00000000, ctrl->zram_dec_base + 0x408);	// must

	/* The last line */
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "\n");
	return copied;
}

#define zram_smmu_dump(file, fmt, args...)		\
	do {						\
		if (file) {				\
			seq_printf(file, fmt, ##args);	\
		} else {				\
			pr_info(fmt, ##args);		\
		}					\
	} while (0)

void engine_get_smmu_reg_dump(struct engine_control_t *ctrl, struct seq_file *s)
{
	void __iomem *reg;
	uint32_t reg_val;
	unsigned int smmuwp_reg_nr, i;
	char output[LINE_SZ];
	int offset = 0;

	smmuwp_reg_nr = ARRAY_SIZE(zram_smmuwp_regs);
	for (i = 0; i < smmuwp_reg_nr;) {

		reg = ctrl->zram_smmu_base + zram_smmuwp_regs[i].offset;
		reg_val = zram_readl(reg);
		offset += snprintf(output + offset, LINE_SZ - offset, "%-11s:0x%03x=0x%-8x",
				zram_smmuwp_regs[i].name, zram_smmuwp_regs[i].offset, reg_val);

		/* newline or space */
		if (++i % 4 == 0) {
			zram_smmu_dump(s, "%s\n", output);
			offset = 0;
		} else {
			offset += snprintf(output + offset, LINE_SZ - offset, " ");
		}
	}
}
