// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <asm/io.h>
#include <asm/kvm_hyp.h>
#include <asm/kvm_pkvm_module.h>

#include <mtk-iommu-defines.h>
#include "include/hypmmu.h"
#include "include/mtk-iommu.h"

static u32 pa_to_ttbr(u64 pa)
{
	return (u32)((pa & TTBR_LO_MASK) | (pa >> 32));
}

static u64 ttbr_to_pa(u32 ttbr)
{
	return ((ttbr & TTBR_LO_MASK) | ((u64)(ttbr & TTBR_HI_MASK) << 32));
}

static void pgd_share_hyp(u64 pa, u64 size)
{
	u64 pfn = pa >> PAGE_SHIFT;
	u64 nr_pages = size >> PAGE_SHIFT;
	int ret;

	for (u32 i = 0; i < nr_pages; i++) {
		ret = mod_ops->host_share_hyp(pfn + i);
		if (ret)
			MOD_PUTS2("pgd share hyp failed", i, pfn);
	}
}

static void write_to_ns_bank(struct mtk_iommu_device *dev, u32 reg_val, u32 offset)
{
	void *va = dev->bank_reg[NS_BANK].base_va + offset;

	writel(reg_val, va);
}

static void write_to_prot_banks(struct mtk_iommu_device *dev, u32 reg_val, u32 offset)
{
	void *va;

	va = dev->bank_reg[PROT_1_BANK].base_va + offset;
	writel(reg_val, va);

	if (is_enable_jpeg_prot_2()) {
		va = dev->bank_reg[PROT_2_BANK].base_va + offset;
		writel(reg_val, va);
	}
}

static void write_pgd_to_bank(struct mtk_iommu_device *dev, u32 id, u64 pgd_pa)
{
	u32 ttbr = pa_to_ttbr(pgd_pa);
	void *va = dev->bank_reg[id].base_va + REG_MMU_PT_BASE_ADDR;

	writel(ttbr, va);

#if (DEBUG_IOMMU)
	MOD_PUTS3("write-pgd-to-bank: bank_id pgd_pa ttbr", id, pgd_pa, ttbr);
#endif
}

static bool emu_write_mmu_ttbr(struct mtk_iommu_device *dev, u64 reg_val)
{
	u32 ttbr = (u32)reg_val;
	u64 pa = ttbr_to_pa(ttbr);

#if (DEBUG_IOMMU)
	MOD_PUTS2("emu_write_mmu_pt: [ttbr] [pa]", ttbr, pa);
#endif

	if (!dev->guest.ttbr) {
		mod_ops->puts("first write guest ttbr");
		/* first write to guest ttbr */
		dev->guest.ttbr = ttbr;
		/* share pgd table to hyp */
		pgd_share_hyp(pa, MTK_IOMMU_PGD_TABLE_SIZE);
		dev->guest.pgd = mod_ops->hyp_va(pa);
		set_guest_pgd(pa, dev->table_id);
	} else {
		/*
		 * ttbr register is written when system resumes.
		 * make sure the restore value is same as init value
		 */
		if (dev->guest.ttbr != ttbr) {
			MOD_PUTS2("guest.ttbr != ttbr", dev->guest.ttbr, ttbr);
			WARN_ON(1);
		}
	}

	/* write hw pgd to ns bank*/
	write_pgd_to_bank(dev, NS_BANK, query_hw_pgd(dev->table_id));
	/* write prot pgd to prot-1 bank*/
	write_pgd_to_bank(dev, PROT_1_BANK, query_prot_pgd(dev->table_id));
	/* binding hw pgd into prot-2 bank for jpeg */
	if (is_enable_jpeg_prot_2())
		write_pgd_to_bank(dev, PROT_2_BANK, query_hw_pgd(dev->table_id));

	/* return handled */
	return true;
}

static u64 emu_read_cpe_done(struct mtk_iommu_device *dev)
{
	u32 ns_cpe, prot_1_cpe, prot_2_cpe;
	void *ns_reg, *prot_1_reg, *prot_2_reg;
	u32 reg_val;

	ns_reg = (void *)(dev->bank_reg[NS_BANK].base_va + REG_MMU_CPE_DONE);
	prot_1_reg = (void *)(dev->bank_reg[PROT_1_BANK].base_va + REG_MMU_CPE_DONE);

	if (is_enable_jpeg_prot_2())
		prot_2_reg = (void *)(dev->bank_reg[PROT_2_BANK].base_va + REG_MMU_CPE_DONE);

	ns_cpe = readl(ns_reg);
	prot_1_cpe = readl(prot_1_reg);

	if (is_enable_jpeg_prot_2())
		prot_2_cpe = readl(prot_2_reg);
	else
		prot_2_cpe = 1; /* ignore prot2, 1 means always done */

	/* protect bank participate the cpe_done if it's inv_range */
	if (dev->guest.invld & F_MMU_INV_RANGE) {
		/* return 1 when all banks are done */
		reg_val = (ns_cpe && prot_1_cpe && prot_2_cpe) ? 1 : 0;
#if (DEBUG_IOMMU >=2)
		MOD_PUTS3("tlb range: ns_cpe prot1_cpe reg_ret", ns_cpe, prot_1_cpe, reg_val);
#endif
	} else {
#if (DEBUG_IOMMU >=2)
		MOD_PUTS1("tlb all: ns_cpe", ns_cpe);
#endif
		reg_val = ns_cpe;
	}

	return (u64)reg_val;
}

static void mmio_write(struct user_pt_regs *regs, struct mtk_iommu_device *dev,
		      u64 esr, size_t reg_offset)
{
	u64 is_64 = (esr & ESR_ELx_SF) >> ESR_ELx_SF_SHIFT;
	u32 x = (esr & ESR_ELx_SRT_MASK) >> ESR_ELx_SRT_SHIFT;
	u64 reg_val = regs->regs[x];
	bool sync_write = false;
	bool handled = false;

	if (is_64)
		return; /* no 64 bits registers, write ignore */

	switch(reg_offset) {
	case REG_MMU_PT_BASE_ADDR:
		handled = emu_write_mmu_ttbr(dev, reg_val);
		break;
	case REG_MMU_INVLD_START_A:
		dev->guest.invld_sa = (u32)reg_val;
		sync_write = true;
		break;
	case REG_MMU_INVLD_END_A:
		dev->guest.invld_ea = (u32)reg_val;
		sync_write = true;
		break;
	case REG_MMU_INVALIDATE:
		dev->guest.invld = (u32)reg_val;
		sync_write = true;
		break;
	case REG_MMU_CPE_DONE:
		sync_write = true;
		break;
	case REG_MMU_INV_SEL_GEN2:
		sync_write = true;
		break;
	}

	if (!handled) {
		/* direct write to ns bank */
		write_to_ns_bank(dev, reg_val, reg_offset);
		if (!sync_write)
			return;
		/* sync write to prot banks */
		write_to_prot_banks(dev, reg_val, reg_offset);
	}
}

static void mmio_read(struct user_pt_regs *regs, struct mtk_iommu_device *dev,
		      u64 esr, size_t reg_offset)
{
	u64 is_64 = (esr & ESR_ELx_SF) >> ESR_ELx_SF_SHIFT;
	u32 x = (esr & ESR_ELx_SRT_MASK) >> ESR_ELx_SRT_SHIFT;
	u64 reg_val;
	void *va;
	bool handled = false;

	va = (void *)(dev->bank_reg[NS_BANK].base_va + reg_offset);

	if (is_64) {
		/* mtk-iommu does not have 64 bits access just reutrn 0 */
		regs->regs[x] = 0;
		return;
	}

	switch(reg_offset) {
	case REG_MMU_PT_BASE_ADDR:
		regs->regs[x] = (u64)dev->guest.ttbr;
		handled = true;
		break;
	case REG_MMU_CPE_DONE:
		regs->regs[x] = emu_read_cpe_done(dev);
		handled = true;
		break;
	}
	/* direct read */
	if (!handled) {
		reg_val = (u64)readl(va);
		regs->regs[x] = reg_val;
	}
}

bool mtkiommu_dabt_handler(struct user_pt_regs *regs, u64 esr, u64 addr)
{
	u64 offset;
	bool is_write;
	struct mtk_iommu_device *dev;

	is_write = esr & ESR_ELx_WNR;

	/* lookup mtk-iommu device by ns_bank addr */
	dev = mtk_iommu_lookup_dev(addr);
	if (!dev)
		return false;

#if (DEBUG_IOMMU >= 2)
	MOD_PUTS2("found mtkiommu dev", addr);
#endif

	offset = addr - dev->bank_reg[NS_BANK].reg_base;

	/* Handle mmio trap for NS bank, others PROT BANKS just R/W ignore */
	if (offset >= SZ_4K)
		return true;

	if (is_write)
		mmio_write(regs, dev, esr, offset);
	else
		mmio_read(regs, dev, esr, offset);

	/* true menas this trap has been handled by us */
	return true;
}

