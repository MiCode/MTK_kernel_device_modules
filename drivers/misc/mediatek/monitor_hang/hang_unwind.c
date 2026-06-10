// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#ifdef __aarch64__
#include <linux/sched.h>
#include <asm/pointer_auth.h>
#include <asm/stacktrace/common.h>
#endif
#include <asm/current.h>
#include <asm/stacktrace.h>
#include <linux/mm.h>
#include "hang_unwind.h"

#ifdef __aarch64__
unsigned int hang_kernel_trace(struct task_struct *tsk,
					unsigned long *store, unsigned int size)
{
	struct unwind_state frame;
	unsigned long fp;
	unsigned int store_len = 1;

	if (tsk == current)
		fp = (unsigned long)__builtin_frame_address(0);
	else
		fp = thread_saved_fp(tsk);
	frame.fp = fp;
	frame.pc = thread_saved_pc(tsk);
	if (!frame.pc) {
		pr_info("err stack:%lx\n", thread_saved_sp(tsk));
		return 0;
	}
	*store = frame.pc;
	while(store_len < size) {
		if (!on_task_stack(tsk, fp, 16) || !IS_ALIGNED(fp, 8))
			break;
		frame.fp = READ_ONCE_NOCHECK(*(unsigned long *)(fp));
		frame.pc = READ_ONCE_NOCHECK(*(unsigned long *)(fp + 8));
		fp = frame.fp;
		if (!frame.pc)
			continue;

		frame.pc = ptrauth_strip_kernel_insn_pac(frame.pc);

		*(++store) = frame.pc;
		store_len += 1;
	}
	return store_len;
}

const char *hang_arch_vma_name(struct vm_area_struct *vma)
{
	return NULL;
}

#else /*__aarch64__*/

unsigned int hang_kernel_trace(struct task_struct *tsk,
					unsigned long *store, unsigned int size)
{
#if IS_ENABLED(CONFIG_ARM_UNWIND)
	struct pt_regs regs;
	unsigned int store_len = 1;

	if (tsk == current) {
		store_len = stack_trace_save_tsk(tsk, store, size, 0);
	} else {
		/* task blocked in __switch_to */
		regs.ARM_fp = thread_saved_fp(tsk);
		regs.ARM_sp = thread_saved_sp(tsk);
		/*
		 * The function calling __switch_to cannot be a leaf function
		 * so LR is recovered from the stack.
		 */
		regs.ARM_lr = 0;
		regs.ARM_pc = thread_saved_pc(tsk);
		store_len = stack_trace_save_regs(&regs, store, size, 0);
	}
	return store_len;
#else
	return 0;
#endif
}

#ifdef MODULE
const char *hang_arch_vma_name(struct vm_area_struct *vma)
{
	return NULL;
}
#else
const char *hang_arch_vma_name(struct vm_area_struct *vma)
{
	return arch_vma_name(vma);
}
#endif

#endif

