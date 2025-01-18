// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#undef pr_fmt
#define pr_fmt(fmt) "MKP: " fmt

#include <asm/kvm_pkvm_module.h>
#include <asm/archrandom.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <trace/hooks/vendor_hooks.h>
#include <trace/hooks/avc.h>
#ifdef SUPPORT_CREDS
#include <trace/hooks/creds.h>
#endif
#include <trace/hooks/selinux.h>
#include <trace/hooks/syscall_check.h>
#include <linux/types.h> // for list_head
#include <linux/module.h> // module_layout
#include <linux/init.h> // rodata_enable support
#include <linux/mutex.h>
#include <linux/kernel.h> // round_up
#include <linux/reboot.h>
#include <linux/workqueue.h>
#include <linux/tracepoint.h>
#include <linux/of.h>
#include <linux/libfdt.h> // fdt32_ld
#include <linux/vmalloc.h>
#include <linux/mm.h>

#include "selinux/mkp_security.h"
#include "selinux/mkp_policycap.h"

#include "mkp_demo.h"
#include "mkp_hvc.h"
#include "mkp.h"
#include "trace_mkp.h"
#define CREATE_TRACE_POINTS
#include "trace_mtk_mkp.h"

#define mkp_debug 0
DEBUG_SET_LEVEL(DEBUG_LEVEL_ERR);

#define SUPPORT_FULL_KERNEL_CODE_2M
#define DEFAULT_MAX_PID 32768

int __kvm_nvhe_mkp_hyp_init(const struct pkvm_module_ops *ops);
void __kvm_nvhe_handle__mkp_hyp_hvc(struct kvm_cpu_context *ctx);
int hvc_number;

struct work_struct *avc_work;

static uint32_t g_ro_cred_handle __ro_after_init;
int avc_array_sz __ro_after_init;
int cred_array_sz __ro_after_init;
int rem;
const struct selinux_state *g_selinux_state;

#if mkp_debug
static void mkp_trace_event_func(struct timer_list *unused);
static DEFINE_TIMER(mkp_trace_event_timer, mkp_trace_event_func);
#define MKP_TRACE_EVENT_TIME 10
#endif

const char *mkp_trace_print_array(void)
{
	static char str[30] = "mkp test trace point\n";

	return str;
}

#if mkp_debug
static void mkp_trace_event_func(struct timer_list *unused) // do not use sleep
{
	char test[1024];

	memset(test, 0, 1024);
	memcpy(test, "hello world.", 13);
	trace_mkp_trace_event_test(test);
	MKP_DEBUG("timer start\n");
	mod_timer(&mkp_trace_event_timer, jiffies
		+ MKP_TRACE_EVENT_TIME * HZ);

}
#endif

struct rb_root mkp_rbtree = RB_ROOT;
DEFINE_RWLOCK(mkp_rbtree_rwlock);

#if !IS_ENABLED(CONFIG_KASAN_GENERIC) && !IS_ENABLED(CONFIG_KASAN_SW_TAGS)
#if !IS_ENABLED(CONFIG_GCOV_KERNEL)
static void *p_stext;
static void *p_etext;
static void *p__init_begin;
#endif
#endif

int mkp_hook_trace_on;
module_param(mkp_hook_trace_on, int, 0600);

bool mkp_hook_trace_enabled(void)
{
	return !!mkp_hook_trace_on;
}

#ifdef SUPPORT_FULL_KERNEL_CODE_2M
bool full_kernel_code_2m;
#endif

#if !IS_ENABLED(CONFIG_KASAN_GENERIC) && !IS_ENABLED(CONFIG_KASAN_SW_TAGS)
#if !IS_ENABLED(CONFIG_GCOV_KERNEL)
static void mkp_protect_kernel_work_fn(struct work_struct *work);

static DECLARE_DELAYED_WORK(mkp_pk_work, mkp_protect_kernel_work_fn);
static int retry_num = 100;
static void mkp_protect_kernel_work_fn(struct work_struct *work)
{
	int ret = 0;
	uint32_t policy = 0;
	uint32_t handle = 0;
	unsigned long addr_start;
	unsigned long addr_end;
	phys_addr_t phys_addr;
	int nr_pages;
	int init = 0;

#ifdef SUPPORT_FULL_KERNEL_CODE_2M
	// Map all kernel code in the EL1S2 with the granularity of 2M
	bool kernel_code_perf = false;
	unsigned long addr_start_2m = 0, addr_end_2m = 0;
#endif

	if (policy_ctrl[MKP_POLICY_KERNEL_CODE] &&
		policy_ctrl[MKP_POLICY_KERNEL_RODATA]) {
		mkp_get_krn_info(&p_stext, &p_etext, &p__init_begin);
		if (p_stext == NULL || p_etext == NULL || p__init_begin == NULL) {
			pr_info("%s: retry in 0.1 second", __func__);
			if (--retry_num >= 0)
				schedule_delayed_work(&mkp_pk_work, HZ / 10);
			else
				MKP_ERR("protect krn failed\n");
			return;
		}
		init = 1;
	}
	if (policy_ctrl[MKP_POLICY_KERNEL_CODE] != 0) {
		if (!init)
			mkp_get_krn_code(&p_stext, &p_etext);
		if (p_stext == NULL || p_etext == NULL) {
			pr_info("%s: retry in 0.1 second", __func__);
			if (--retry_num >= 0)
				schedule_delayed_work(&mkp_pk_work, HZ / 10);
			else
				MKP_ERR("protect krn failed\n");
			return;
		}
	}
	if (policy_ctrl[MKP_POLICY_KERNEL_RODATA] != 0) {
		if (!init)
			mkp_get_krn_rodata(&p_etext, &p__init_begin);
		if (p_etext == NULL || p__init_begin == NULL) {
			pr_info("%s: retry in 0.1 second", __func__);
			if (--retry_num >= 0)
				schedule_delayed_work(&mkp_pk_work, HZ / 10);
			else
				MKP_ERR("protect krn failed\n");
			return;
		}
	}

	if (policy_ctrl[MKP_POLICY_KERNEL_CODE] &&
		policy_ctrl[MKP_POLICY_KERNEL_RODATA]) {

#ifdef SUPPORT_FULL_KERNEL_CODE_2M
		// It may ONLY take effects when BOTH KERNEL_CODE & KERNEL_RODATA are enabled
		if (full_kernel_code_2m)
			kernel_code_perf = true;
#endif
	}

	if (policy_ctrl[MKP_POLICY_KERNEL_CODE] != 0) {
		// round down addr before minus operation
		addr_start = (unsigned long)p_stext;
		addr_end = (unsigned long)p_etext;
		addr_start = round_up(addr_start, PAGE_SIZE);
		addr_end = round_down(addr_end, PAGE_SIZE);

#ifdef SUPPORT_FULL_KERNEL_CODE_2M
		// Try to round_down/up the boundary in 2M
		if (kernel_code_perf) {
			addr_start_2m = round_down(addr_start, SZ_2M);
			// The range size of _text and _stext should SEGMENT_ALIGN
			if ((addr_start - addr_start_2m) == SEGMENT_ALIGN) {
				addr_start = addr_start_2m;
				addr_end_2m = round_up(addr_end, SZ_2M);
				addr_end = addr_end_2m;
			}
		}
#endif
		if (addr_start == 0) {
			MKP_ERR("Cannot find the kernel text\n");
			goto protect_krn_fail;
		}

		nr_pages = (addr_end-addr_start)>>PAGE_SHIFT;
		phys_addr = __pa_symbol((void *)addr_start);
		policy = MKP_POLICY_KERNEL_CODE;
		handle = mkp_create_handle(policy, (unsigned long)phys_addr, nr_pages<<12);
		if (handle == 0) {
			MKP_ERR("%s:%d: Create handle fail\n", __func__, __LINE__);
		} else {
			ret = mkp_set_mapping_x(policy, handle);
			/* Set kernel code as RO will casue boot fail, seems that there is an issue with tracepoint. */
			// ret = mkp_set_mapping_ro(policy, handle);
			pr_info("mkp: protect krn code done\n");
		}
	}

	if (policy_ctrl[MKP_POLICY_KERNEL_RODATA] != 0) {
		// round down addr before minus operation
		addr_start = (unsigned long)p_etext;
		addr_end = (unsigned long)p__init_begin;
		addr_start = round_up(addr_start, PAGE_SIZE);
		addr_end = round_down(addr_end, PAGE_SIZE);


#ifdef SUPPORT_FULL_KERNEL_CODE_2M
		// Try to round_down/up the boundary in 2M
		if (kernel_code_perf && (addr_end_2m != 0) && (addr_end_2m <= addr_end))
			addr_start = addr_end_2m;
#endif
		if (addr_start == 0) {
			MKP_ERR("Cannot find the kernel rodata\n");
			goto protect_krn_fail;
		}

		nr_pages = (addr_end-addr_start)>>PAGE_SHIFT;
		phys_addr = __pa_symbol((void *)addr_start);
		policy = MKP_POLICY_KERNEL_RODATA;
		handle = mkp_create_handle(policy, (unsigned long)phys_addr, nr_pages<<12);
		if (handle == 0)
			MKP_ERR("%s:%d: Create handle fail\n", __func__, __LINE__);
		else {
			ret = mkp_set_mapping_ro(policy, handle);
			pr_info("mkp: protect krn rodata done\n");
		}
	}

protect_krn_fail:
	p_stext = NULL;
	p_etext = NULL;
	p__init_begin = NULL;
}
#endif
#endif

static int __init protect_mkp_self(void)
{
	module_enable_ro(THIS_MODULE, false, MKP_POLICY_MKP);
	module_enable_nx(THIS_MODULE, MKP_POLICY_MKP);
	module_enable_x(THIS_MODULE, MKP_POLICY_MKP);

	// mkp_start_granting_hvc_call();
	return 0;
}

int mkp_reboot_notifier_event(struct notifier_block *nb, unsigned long event, void *v)
{
	MKP_DEBUG("mkp reboot notifier\n");
	return NOTIFY_DONE;
}

/* For probing interesting tracepoints */
struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	int policy;
};

static void mkp_task_newtask(void *ignore, struct task_struct *task, unsigned long clone_flags)
{
	int ret = -1;
	struct cred_sbuf_content c;

	if (g_ro_cred_handle == 0)
		return;

	MKP_HOOK_BEGIN(__func__);

	c.csc.uid.val = task->cred->uid.val;
	c.csc.gid.val = task->cred->gid.val;
	c.csc.euid.val = task->cred->euid.val;
	c.csc.egid.val = task->cred->egid.val;
	c.csc.fsuid.val = task->cred->fsuid.val;
	c.csc.fsgid.val = task->cred->fsgid.val;
	c.csc.security = task->cred->security;
	ret = mkp_update_sharebuf_4_argu(MKP_POLICY_TASK_CRED, g_ro_cred_handle,
			(unsigned long)task->pid,
			c.args[0], c.args[1], c.args[2], c.args[3]);

	MKP_HOOK_END(__func__);
}


static struct tracepoints_table mkp_tracepoints[] = {
{.name = "task_newtask", .func = mkp_task_newtask, .tp = NULL, .policy = MKP_POLICY_TASK_CRED},
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(mkp_tracepoints) / sizeof(struct tracepoints_table); i++)

/* Map full kernel text in the granularity of 2MB */
static const struct of_device_id mkp_of_match[] = {
	{ .compatible = "mediatek,mkp-drv", },
	{ }
};
MODULE_DEVICE_TABLE(of, mkp_of_match);

static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(mkp_tracepoints[i].name, tp->name) == 0)
			mkp_tracepoints[i].tp = tp;
	}
}

/*
 * Find out interesting tracepoints and try to register them.
 * Update policy_ctrl if needed.
 */
static void __init mkp_hookup_tracepoints(void)
{
	int i;
	int ret;

	// Find out interesting tracepoints
	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	// Update policy control if needed
	FOR_EACH_INTEREST(i) {
		if (policy_ctrl[i] != 0 && mkp_tracepoints[i].tp == NULL) {
			MKP_ERR("%s not found for policy %d\n",
				mkp_tracepoints[i].name, mkp_tracepoints[i].policy);
			policy_ctrl[i] = 0;
		}
	}

	// Probing found tracepoints
	FOR_EACH_INTEREST(i) {
		if (policy_ctrl[i] != 0 && mkp_tracepoints[i].tp != NULL) {
			ret = tracepoint_probe_register(mkp_tracepoints[0].tp,
							mkp_tracepoints[0].func,  NULL);
			if (ret) {
				MKP_ERR("Failed to register %s for policy %d\n",
					mkp_tracepoints[i].name, mkp_tracepoints[i].policy);
				policy_ctrl[i] = 0;
			}
		}
	}
}

#ifndef SUPPORT_FULL_KERNEL_CODE_2M
static void free_reserved_memory(phys_addr_t start_phys, phys_addr_t end_phys)
{
	phys_addr_t pos;
	unsigned long nr_pages = 0;

	if (end_phys <= start_phys) {
		pr_info("%s: end_phys is smaller than start_phys start_phys:0x%pa end_phys:0x%pa\n",
				__func__, &start_phys, &end_phys);
		return;
	}
	for (pos = start_phys; pos < end_phys; pos += PAGE_SIZE, nr_pages++)
		free_reserved_page(phys_to_page(pos));

	if (nr_pages) {
		pr_info("freeing mkp %ldK reserved memory\n",
				nr_pages << (PAGE_SHIFT - 10));
	}
}
#endif

static int get_reserved_memory(struct device *dev)
{
	struct device_node *np;
	struct reserved_mem *rmem;

	np = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (!np) {
		dev_info(dev, "no memory-region\n");
		return -EINVAL;
	}

	rmem = of_reserved_mem_lookup(np);
	of_node_put(np);

	if (!rmem) {
		dev_info(dev, "no memory-region\n");
		return -EINVAL;
	}

#ifdef SUPPORT_FULL_KERNEL_CODE_2M
	/* Enable the support of full kernel code with 2M mapping */
	full_kernel_code_2m = true;
	pr_info("resource base=%pa, size=%pa\n", &rmem->base, &rmem->size);
	MKP_INFO("Support FULL_KERNEL_CODE_2M\n");
#else
	free_reserved_memory(rmem->base, rmem->base + rmem->size);
	MKP_INFO("Not Support FULL_KERNEL_CODE_2M\n");
#endif /* SUPPORT_FULL_KERNEL_CODE_2M */

	return 0;
}

static int mkp_probe(struct platform_device *pdev)
{
	get_reserved_memory(&pdev->dev);

	return 0;
}

struct platform_driver mkp_driver = {
	.probe = mkp_probe,
	.remove = NULL,
	.driver = {
		.name = "mkp-drv",
		.owner = THIS_MODULE,
		.of_match_table = mkp_of_match,
	},
};

int __init mkp_demo_init(void)
{
	int ret = 0, nid = 0; //, ret_erri_line;
	unsigned long token;
	bool smccc_trng_available;
	pg_data_t *pgdat;
	struct reserved_mem *rmem = NULL;
	u64 DRAM_SIZE = 0;
	unsigned long start_pfn = 0;
	unsigned long end_pfn = 0;
	// unsigned long size = 0x100000;
	struct device_node *node;
	phys_addr_t rmem_base, rmem_size;
	u32 mkp_policy_default = 0x0001fffb; // disable selinux_state policy as default
	u32 mkp_policy = 0x0001ffff;
	struct arm_smccc_res res;

	/* load mkp el2 module */
	ret = pkvm_load_el2_module(__kvm_nvhe_mkp_hyp_init, &token);
	if (ret) {
		pr_info("%s:%d, ret: %d\n", __func__, __LINE__, ret);
		return ret;
	}

	/* register hvc call  */
	ret = pkvm_register_el2_mod_call(__kvm_nvhe_handle__mkp_hyp_hvc, token);
	if (ret < 0)
		return ret;

	hvc_number = ret;

	// Get smccc_trng_available
	smccc_trng_available = smccc_probe_trng();

	// Get Dram size
	pgdat = NODE_DATA(nid);
	start_pfn = pgdat->node_start_pfn;
	end_pfn = start_pfn + pgdat->node_spanned_pages - 1;
	DRAM_SIZE = (pgdat->node_spanned_pages) << PAGE_SHIFT;

	// Get mkp reserved memory information
	node = of_find_compatible_node(NULL, NULL, "mediatek,security_pkvm_mkp");
	rmem = of_reserved_mem_lookup(node);
	rmem_base = rmem->base;
	rmem_size = rmem->size;

	// mkp prepare
	res = mkp_el2_mod_call(hvc_number, MKP_HVC_CALL_ID(0, HVC_FUNC_MKP_HYP_PREPARE),
				DRAM_SIZE, rmem_base, rmem_size, smccc_trng_available);
	/* Set policy control */
	mkp_set_policy(mkp_policy & mkp_policy_default);

	/* Hook up interesting tracepoints and update corresponding policy_ctrl */
	mkp_hookup_tracepoints();

	/* Protect kernel code & rodata */
	if (policy_ctrl[MKP_POLICY_KERNEL_CODE] != 0 ||
			policy_ctrl[MKP_POLICY_KERNEL_RODATA] != 0) {
#if !IS_ENABLED(CONFIG_KASAN_GENERIC) && !IS_ENABLED(CONFIG_KASAN_SW_TAGS)
#if !IS_ENABLED(CONFIG_GCOV_KERNEL)
		schedule_delayed_work(&mkp_pk_work, 0);
#endif
#endif
	}

	/* Protect MKP itself */
	if (policy_ctrl[MKP_POLICY_MKP] != 0)
		ret = protect_mkp_self();

	return 0;
}
