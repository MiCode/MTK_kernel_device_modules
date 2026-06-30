// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>
#include <nvhe/spinlock.h>

#include <include/export.h>
#include "include/hypmmu.h"
#include "include/infra-mpu.h"

#define DEBUG_HAL 0

#define CMD_SYNC (KEY_SYNC | SECURE_REGION_ENABLE)
#define CMD_DEFRAGMENT (KEY_SYNC | SECURE_REGION_DISABLE)

static hyp_spinlock_t impu_lock[CORE_NUM];

static void percpu_lock(void)
{
	u8 cpu = get_cpu_id();

	hyp_spin_lock(&impu_lock[cpu]);
}

static void percpu_unlock(void)
{
	u8 cpu = get_cpu_id();

	hyp_spin_unlock(&impu_lock[cpu]);
}

static void pmm_pre_init(void)
{
	MOD_PUTS("infra-mpu pre_init");
}

static int pmm_prepare(void)
{
#if (DEBUG_HAL)
	MOD_PUTS("infra-mpu prepare");
#endif
	percpu_lock();
	return 0;
}

static int pmm_secure(u64 paddr, u32 size, u8 pmm_attr)
{
	/* legacy API. no need to implement */
	return 0;
}

static int pmm_unsecure(u64 paddr, u32 size, u8 pmm_attr)
{
	/* legacy API. no need to implement */
	return 0;
}

static int pmm_secure_v2(u64 paddr, u8 order, u8 pmm_attr)
{
	u8 cpu = get_cpu_id();
	u8 sr_info = hypmmu_get_srinfo(pmm_attr);
	struct infra_buf *p = &infra_shared_buf[cpu];
	u32 *buf, idx;

#if (DEBUG_HAL)
	MOD_PUTS3("infra-mpu secure_v2", paddr, order, pmm_attr);
#endif

	buf = (u32 *)p->buf_ptr;
	idx = p->counter;

	if (idx < (p->buf_size / sizeof(u32))) {
		buf[idx] = INFRA_BUF_ENTRY(paddr, order, sr_info);
		p->counter++;
	} else
		MOD_PUTS3("Buffer overflow in infar-mpu secure_v2", cpu, idx, p->buf_size);

	return 0;
}

static int pmm_unsecure_v2(u64 paddr, u8 order, u8 pmm_attr)
{
	u8 cpu = get_cpu_id();
	struct infra_buf *p = &infra_shared_buf[cpu];
	u32 *buf, idx;

#if (DEBUG_HAL)
	MOD_PUTS3("infra-mpu unsecure_v2", paddr, order, pmm_attr);
#endif

	buf = (u32 *)p->buf_ptr;
	idx = p->counter;

	if (idx < (p->buf_size / sizeof(u32))) {
		buf[idx] = INFRA_BUF_ENTRY(paddr, order, 0);
		p->counter++;
	} else
		MOD_PUTS3("Buffer overflow in infar-mpu unsecure_v2", cpu, idx, p->buf_size);

	return 0;
}

static int pmm_sync(void)
{
	struct arm_smccc_res res;
	u8 cpu = get_cpu_id();
	struct infra_buf *p = &infra_shared_buf[cpu];

#if (DEBUG_HAL)
	MOD_PUTS("infra-mpu sync");
#endif
	/* flush ipc buf */
	mod_ops->flush_dcache_to_poc(p->buf_ptr, p->buf_size);
	arm_smccc_1_1_smc(MTK_SIP_HYP_IMPU_CONTROL, CMD_SYNC,
			  (u64)cpu, p->counter, 0, 0, 0, 0, &res);

	p->counter = 0;
	percpu_unlock();

	if (res.a0)
		MOD_PUTS1("infar-mpu sync failed ret", res.a0);

	return 0;
}

static int pmm_defragment(void)
{
	struct arm_smccc_res res;

#if (DEBUG_HAL)
	MOD_PUTS("infra-mpu defragment");
#endif

	arm_smccc_1_1_smc(MTK_SIP_HYP_IMPU_CONTROL, CMD_DEFRAGMENT,
			  0, 0, 0, 0, 0, 0, &res);

	if (res.a0)
		MOD_PUTS1("infar-mpu defragment failed ret", res.a0);
	return 0;
}

static struct pmm_hal pmm_ops = {
	.prepare		= pmm_prepare,
	.secure			= pmm_secure,
	.unsecure		= pmm_unsecure,
	.secure_v2		= pmm_secure_v2,
	.unsecure_v2		= pmm_unsecure_v2,
	.sync			= pmm_sync,
	.defragment		= pmm_defragment,
};

static const char *pmm_hal_name = "infra-mpu";

void register_inframpu_pmm_hal(void)
{
	MOD_PUTS("register_inframpu_pmm_hal");

	pmm_ops.name = pmm_hal_name;

	pmm_pre_init();

	hyp_pmm_hal_register(&pmm_ops);
}
