/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#if !defined(__MRDUMP_PRIVATE_H__)
#define __MRDUMP_PRIVATE_H__

#include <asm/cputype.h>
#include <asm/memory.h>
#include <asm/smp_plat.h>
#include <asm-generic/sections.h>
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
#include <mt-plat/mboot_params.h>
#endif
#include <mt-plat/mrdump.h>
#include "mrdump_helper.h"

#define DEBUG_COMPATIBLE "mediatek,aee_debug_kinfo"

#define MBOOT_PARAMS_DRAM_OFF	0x1000
#define MBOOT_PARAMS_DRAM_SIZE	0x1000

struct pt_regs;

struct mrdump_params {
	int aee_enable;

	char lk_version[12];
	bool drm_ready;

	phys_addr_t cb_addr;
	phys_addr_t cb_size;
};

extern struct mrdump_control_block *mrdump_cblock;
extern const unsigned long kallsyms_addresses[] __weak;
extern const int kallsyms_offsets[] __weak;
extern const u8 kallsyms_names[] __weak;
extern const u8 kallsyms_token_table[] __weak;
extern const u16 kallsyms_token_index[] __weak;
extern const unsigned long kallsyms_markers[] __weak;
extern const unsigned long kallsyms_num_syms
__attribute__((weak, section(".rodata")));

#ifdef MODULE
int mrdump_module_init_mboot_params(void);
#endif
void mrdump_cblock_init(const struct mrdump_params *mparams);
void mrdump_cblock_late_init(void);
int mrdump_full_init(const char *version);
int mrdump_mini_init(const struct mrdump_params *mparams);

void mrdump_save_control_register(void *creg);
void mrdump_arch_fill_machdesc(struct mrdump_machdesc *machdesc_p);
void mrdump_arch_show_regs(const struct pt_regs *regs);

#if defined(__arm__)
static inline void crash_setup_regs(struct pt_regs *newregs,
				    struct pt_regs *oldregs)
{
	if (oldregs) {
		memcpy(newregs, oldregs, sizeof(*newregs));
	} else {
		__asm__ __volatile__ (
			"stmia	%[regs_base], {r0-r12}\n\t"
			"mov	%[_ARM_sp], sp\n\t"
			"str	lr, %[_ARM_lr]\n\t"
			"adr	%[_ARM_pc], 1f\n\t"
			"mrs	%[_ARM_cpsr], cpsr\n\t"
		"1:"
			: [_ARM_pc] "=r" (newregs->ARM_pc),
			  [_ARM_cpsr] "=r" (newregs->ARM_cpsr),
			  [_ARM_sp] "=r" (newregs->ARM_sp),
			  [_ARM_lr] "=o" (newregs->ARM_lr)
			: [regs_base] "r" (&newregs->ARM_r0)
			: "memory"
		);
	}
}
#endif

#endif /* __MRDUMP_PRIVATE_H__ */
