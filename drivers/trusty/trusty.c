// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2013 Google, Inc.
 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/trusty/smcall.h>
#include <linux/trusty/sm_err.h>
#include <linux/trusty/trusty.h>

#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>

#if CONFIG_ARM64
#include <asm/daifflags.h>
#endif

#ifdef MTK_ADAPTED
#ifdef CONFIG_TRUSTY_FFA_TRANSPORT
#include "trusty-ffa.h"
#endif
#else
#include "trusty-ffa.h"
#endif
#include "trusty-irq.h"
#include "trusty-private.h"
#include "trusty-trace.h"
#include "trusty-sched-share-api.h"

static struct platform_driver trusty_driver;
static int trusty_cpuhp_slot = -1;

static bool use_high_wq;
module_param(use_high_wq, bool, 0660);

static int nop_nice_value = -20; /* default to highest */
module_param(nop_nice_value, int, 0660);

static u64 trusty_poll_period_ms = 100;
module_param(trusty_poll_period_ms, ullong, 0660);

#ifdef MTK_ADAPTED
static u32 real_drv;
#endif

s32 trusty_fast_call32(struct device *dev, u32 smcnr, u32 a0, u32 a1, u32 a2)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	if (WARN_ON(!s))
		return SM_ERR_INVALID_PARAMETERS;
	if (WARN_ON(!SMC_IS_FASTCALL(smcnr)))
		return SM_ERR_INVALID_PARAMETERS;
	if (WARN_ON(SMC_IS_SMC64(smcnr)))
		return SM_ERR_INVALID_PARAMETERS;

	return s->call_ops->trusty_fast_call(dev, smcnr, a0, a1, a2);
}
EXPORT_SYMBOL(trusty_fast_call32);

#ifdef CONFIG_64BIT
s64 trusty_fast_call64(struct device *dev, u64 smcnr, u64 a0, u64 a1, u64 a2)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	if (WARN_ON(!s))
		return SM_ERR_INVALID_PARAMETERS;
	if (WARN_ON(!SMC_IS_FASTCALL(smcnr)))
		return SM_ERR_INVALID_PARAMETERS;
	if (WARN_ON(!SMC_IS_SMC64(smcnr)))
		return SM_ERR_INVALID_PARAMETERS;

	return s->call_ops->trusty_fast_call(dev, smcnr, a0, a1, a2);
}
EXPORT_SYMBOL(trusty_fast_call64);
#endif

static unsigned long trusty_std_call_inner(struct device *dev,
					   unsigned long smcnr,
					   unsigned long a0, unsigned long a1,
					   unsigned long a2)
{
	unsigned long ret;
	int retry = 5;
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	dev_dbg(dev, "%s(0x%lx 0x%lx 0x%lx 0x%lx)\n",
		__func__, smcnr, a0, a1, a2);
	while (true) {
		ret = s->call_ops->trusty_std_call(dev, smcnr, a0, a1, a2);
		while ((s32)ret == SM_ERR_FIQ_INTERRUPTED)
			ret = s->call_ops->trusty_std_call(dev,
							   SMC_SC_RESTART_FIQ,
							   0, 0, 0);
		if ((int)ret != SM_ERR_BUSY || !retry)
			break;

		dev_dbg(dev, "%s(0x%lx 0x%lx 0x%lx 0x%lx) returned busy, retry\n",
			__func__, smcnr, a0, a1, a2);
		retry--;
	}

	return ret;
}

#if CONFIG_ARM64

static void trusty_local_irq_disable_before_smc(void)
{
	local_daif_mask();
}

static void trusty_local_irq_enable_after_smc(void)
{
	local_daif_restore(DAIF_PROCCTX);
}

#else

static void trusty_local_irq_disable_before_smc(void)
{
	local_irq_disable();
}

static void trusty_local_irq_enable_after_smc(void)
{
	local_irq_enable();
}

#endif

static unsigned long trusty_std_call_helper(struct device *dev,
					    unsigned long smcnr,
					    unsigned long a0, unsigned long a1,
					    unsigned long a2)
{
	unsigned long ret;
	int sleep_time = 1;
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	while (true) {
		trusty_local_irq_disable_before_smc();

		/* tell Trusty scheduler what the current priority is */
		WARN_ON_ONCE(current->policy != SCHED_NORMAL);
		trusty_set_actual_nice(smp_processor_id(),
				s->trusty_sched_share_state, task_nice(current));

		atomic_notifier_call_chain(&s->notifier, TRUSTY_CALL_PREPARE,
					   NULL);

		ret = trusty_std_call_inner(dev, smcnr, a0, a1, a2);
		if (ret == SM_ERR_PANIC) {
			s->trusty_panicked = true;
			if (IS_ENABLED(CONFIG_TRUSTY_CRASH_IS_PANIC))
				panic("trusty crashed");
			else
				WARN_ONCE(1, "trusty crashed");
		}

		atomic_notifier_call_chain(&s->notifier, TRUSTY_CALL_RETURNED,
					   NULL);
		if (ret == SM_ERR_INTERRUPTED) {
			/*
			 * Make sure this cpu will eventually re-enter trusty
			 * even if the std_call resumes on another cpu.
			 */
			trusty_enqueue_nop(dev, NULL);
		}
		trusty_local_irq_enable_after_smc();

		if ((int)ret != SM_ERR_BUSY)
			break;

		if (sleep_time == 256)
			dev_warn(dev, "%s(0x%lx 0x%lx 0x%lx 0x%lx) returned busy\n",
				 __func__, smcnr, a0, a1, a2);
		dev_dbg(dev, "%s(0x%lx 0x%lx 0x%lx 0x%lx) returned busy, wait %d ms\n",
			__func__, smcnr, a0, a1, a2, sleep_time);

		msleep(sleep_time);
		if (sleep_time < 1000)
			sleep_time <<= 1;

		dev_dbg(dev, "%s(0x%lx 0x%lx 0x%lx 0x%lx) retry\n",
			__func__, smcnr, a0, a1, a2);
	}

	if (sleep_time > 256)
		dev_warn(dev, "%s(0x%lx 0x%lx 0x%lx 0x%lx) busy cleared\n",
			 __func__, smcnr, a0, a1, a2);

	return ret;
}

static void trusty_std_call_cpu_idle(struct trusty_state *s)
{
	int ret;

	ret = wait_for_completion_timeout(&s->cpu_idle_completion, HZ * 10);
	if (!ret) {
		dev_warn(s->dev,
			 "%s: timed out waiting for cpu idle to clear, retry anyway\n",
			 __func__);
	}
}

s32 trusty_std_call32(struct device *dev, u32 smcnr, u32 a0, u32 a1, u32 a2)
{
	int ret;
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	if (WARN_ON(SMC_IS_FASTCALL(smcnr)))
		return SM_ERR_INVALID_PARAMETERS;

	if (WARN_ON(SMC_IS_SMC64(smcnr)))
		return SM_ERR_INVALID_PARAMETERS;

	if (s->trusty_panicked) {
		/*
		 * Avoid calling the notifiers if trusty has panicked as they
		 * can trigger more calls.
		 */
		return SM_ERR_PANIC;
	}

	trace_trusty_std_call32(smcnr, a0, a1, a2);

	if (smcnr != SMC_SC_NOP) {
		mutex_lock(&s->smc_lock);
		reinit_completion(&s->cpu_idle_completion);
	}

	dev_dbg(dev, "%s(0x%x 0x%x 0x%x 0x%x) started\n",
		__func__, smcnr, a0, a1, a2);

	ret = trusty_std_call_helper(dev, smcnr, a0, a1, a2);
	while (ret == SM_ERR_INTERRUPTED || ret == SM_ERR_CPU_IDLE) {
		dev_dbg(dev, "%s(0x%x 0x%x 0x%x 0x%x) interrupted\n",
			__func__, smcnr, a0, a1, a2);
		if (ret == SM_ERR_CPU_IDLE)
			trusty_std_call_cpu_idle(s);
		ret = trusty_std_call_helper(dev, SMC_SC_RESTART_LAST, 0, 0, 0);
	}
	dev_dbg(dev, "%s(0x%x 0x%x 0x%x 0x%x) returned 0x%x\n",
		__func__, smcnr, a0, a1, a2, ret);

	if (smcnr == SMC_SC_NOP)
		complete(&s->cpu_idle_completion);
	else
		mutex_unlock(&s->smc_lock);

	trace_trusty_std_call32_done(ret);

	return ret;
}
EXPORT_SYMBOL(trusty_std_call32);

int trusty_share_memory(struct device *dev, u64 *id,
			struct scatterlist *sglist, unsigned int nents,
			pgprot_t pgprot, u64 tag)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));
	int ret;

	if (WARN_ON(dev->driver != &trusty_driver.driver))
		return -EINVAL;

	ret = s->mem_ops->trusty_share_memory(dev, id, sglist, nents, pgprot,
					       tag);

	return ret;
}
EXPORT_SYMBOL(trusty_share_memory);

int trusty_lend_memory(struct device *dev, u64 *id,
		       struct scatterlist *sglist, unsigned int nents,
		       pgprot_t pgprot, u64 tag)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));
	int ret;

	if (WARN_ON(dev->driver != &trusty_driver.driver))
		return -EINVAL;

	ret = s->mem_ops->trusty_lend_memory(dev, id, sglist, nents, pgprot,
					      tag);

	return ret;
}
EXPORT_SYMBOL(trusty_lend_memory);

/*
 * trusty_share_memory_compat - trusty_share_memory wrapper for old apis
 *
 * Call trusty_share_memory and filter out memory attributes if trusty version
 * is old. Used by clients that used to pass just a physical address to trusty
 * instead of a physical address plus memory attributes value.
 */
int trusty_share_memory_compat(struct device *dev, u64 *id,
			       struct scatterlist *sglist, unsigned int nents,
			       pgprot_t pgprot)
{
	int ret;
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	ret = trusty_share_memory(dev, id, sglist, nents, pgprot,
				  TRUSTY_DEFAULT_MEM_OBJ_TAG);
	if (!ret && s->api_version < TRUSTY_API_VERSION_PHYS_MEM_OBJ)
		*id &= 0x0000FFFFFFFFF000ull;

	return ret;
}
EXPORT_SYMBOL(trusty_share_memory_compat);

int trusty_reclaim_memory(struct device *dev, u64 id,
			  struct scatterlist *sglist, unsigned int nents)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));
	int ret;

	if (WARN_ON(dev->driver != &trusty_driver.driver))
		return -EINVAL;

	trace_trusty_reclaim_memory(id);

	ret = s->mem_ops->trusty_reclaim_memory(dev, id, sglist, nents);

	trace_trusty_reclaim_memory_done(id, ret);
	return ret;
}
EXPORT_SYMBOL(trusty_reclaim_memory);

int trusty_call_notifier_register(struct device *dev, struct notifier_block *n)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	return atomic_notifier_chain_register(&s->notifier, n);
}
EXPORT_SYMBOL(trusty_call_notifier_register);

int trusty_call_notifier_unregister(struct device *dev,
				    struct notifier_block *n)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	return atomic_notifier_chain_unregister(&s->notifier, n);
}
EXPORT_SYMBOL(trusty_call_notifier_unregister);

static int trusty_remove_child(struct device *dev, void *data)
{
	platform_device_unregister(to_platform_device(dev));
	return 0;
}

static ssize_t trusty_version_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	return scnprintf(buf, PAGE_SIZE, "%s\n", s->version_str ?: "unknown");
}

static DEVICE_ATTR(trusty_version, 0400, trusty_version_show, NULL);

static struct attribute *trusty_attrs[] = {
	&dev_attr_trusty_version.attr,
	NULL,
};
ATTRIBUTE_GROUPS(trusty);

const char *trusty_version_str_get(struct device *dev)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	return s->version_str;
}
EXPORT_SYMBOL(trusty_version_str_get);

static void trusty_init_version(struct trusty_state *s, struct device *dev)
{
	int ret;
	int i;
	int version_str_len;

	ret = trusty_fast_call32(dev, SMC_FC_GET_VERSION_STR, -1, 0, 0);
	if (ret <= 0)
		goto err_get_size;

	version_str_len = ret;

	s->version_str = kmalloc(version_str_len + 1, GFP_KERNEL);
	for (i = 0; i < version_str_len; i++) {
		ret = trusty_fast_call32(dev, SMC_FC_GET_VERSION_STR, i, 0, 0);
		if (ret < 0)
			goto err_get_char;
		s->version_str[i] = ret;
	}
	s->version_str[i] = '\0';

	dev_info(dev, "trusty version: %s\n", s->version_str);
	return;

err_get_char:
	kfree(s->version_str);
	s->version_str = NULL;
err_get_size:
	dev_err(dev, "failed to get version: %d\n", ret);
}

u32 trusty_get_api_version(struct device *dev)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	return s->api_version;
}
EXPORT_SYMBOL(trusty_get_api_version);

bool trusty_get_panic_status(struct device *dev)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));
	if (WARN_ON(dev->driver != &trusty_driver.driver))
		return false;
	return s->trusty_panicked;
}
EXPORT_SYMBOL(trusty_get_panic_status);

int trusty_init_api_version(struct trusty_state *s, struct device *dev,
			    u32 (*trusty_fast_call)(struct device *dev,
						    unsigned long fid,
						    unsigned long a0,
						    unsigned long a1,
						    unsigned long a2))
{
	u32 api_version;

	api_version = trusty_fast_call(dev, SMC_FC_API_VERSION,
				       TRUSTY_API_VERSION_CURRENT, 0, 0);
	if (api_version == SM_ERR_UNDEFINED_SMC)
		api_version = 0;

	if (api_version > TRUSTY_API_VERSION_CURRENT) {
		dev_err(dev, "unsupported api version %u > %u\n",
			api_version, TRUSTY_API_VERSION_CURRENT);
		return -EINVAL;
	}

	dev_info(dev, "selected api version: %u (requested %u)\n",
		 api_version, TRUSTY_API_VERSION_CURRENT);
	s->api_version = api_version;

	return 0;
}

static bool dequeue_nop(struct trusty_state *s, u32 *args)
{
	unsigned long flags;
	struct trusty_nop *nop = NULL;
	struct trusty_work *tw;
	bool ret = false;
	bool signaled;
	bool nop_dequeued = false;
	bool queue_emptied = false;

	spin_lock_irqsave(&s->nop_lock, flags);
	tw = this_cpu_ptr(s->nop_works);
	signaled = tw->signaled;
	if (!list_empty(&s->nop_queue)) {
		nop = list_first_entry(&s->nop_queue,
				       struct trusty_nop, node);
		list_del_init(&nop->node);
		args[0] = nop->args[0];
		args[1] = nop->args[1];
		args[2] = nop->args[2];

		nop_dequeued = true;
		if (list_empty(&s->nop_queue))
			queue_emptied = true;

		ret = true;
	} else {
		args[0] = 0;
		args[1] = 0;
		args[2] = 0;

		ret = tw->signaled;
	}
	tw->signaled = false;
	spin_unlock_irqrestore(&s->nop_lock, flags);

	/* don't log when false as it is preempt case which can be very noisy */
	if (ret)
		trace_trusty_dequeue_nop(signaled, nop_dequeued, queue_emptied);

	return ret;
}

int trusty_nop_nice_value(void)
{
	return nop_nice_value;
}

static void locked_nop_work_func(struct trusty_work *tw, bool force)
{
	int ret;
	struct trusty_state *s = tw->s;

	ret = trusty_std_call32(s->dev, SMC_SC_LOCKED_NOP, 0, 0, 0);
	if (ret != 0)
		dev_err(s->dev, "%s: SMC_SC_LOCKED_NOP failed %d",
			__func__, ret);

	dev_dbg(s->dev, "%s: done\n", __func__);
}

enum cpunice_cause {
	CPUNICE_CAUSE_DEFAULT,
	CPUNICE_CAUSE_USE_HIGH_WQ,
	CPUNICE_CAUSE_TRUSTY_REQ,
	CPUNICE_CAUSE_NOP_ESCALATE,
	CPUNICE_CAUSE_ENQUEUE_BOOST,
};

static void trusty_adjust_nice_nopreempt(struct trusty_state *s, bool do_nop)
{
	int req_nice, cur_nice;
	int cause_id = CPUNICE_CAUSE_DEFAULT;
	unsigned long flags;
	struct trusty_work *tw;

	local_irq_save(flags);

	cur_nice = task_nice(current);

	/* check to see if another signal has come in since dequeue_nop */
	tw = this_cpu_ptr(s->nop_works);
	do_nop |= tw->signaled;

	if (use_high_wq) {
		/* use highest priority (lowest nice) for everything */
		req_nice = LINUX_NICE_FOR_TRUSTY_PRIORITY_HIGH;
		cause_id = CPUNICE_CAUSE_USE_HIGH_WQ;
	} else {
		/* read trusty request for this cpu if available */
		req_nice = trusty_get_requested_nice(smp_processor_id(),
				s->trusty_sched_share_state);
		cause_id = CPUNICE_CAUSE_TRUSTY_REQ;
	}

	/* ensure priority will not be lower than system request
	 * when there is more work to do
	 */
	if (do_nop && nop_nice_value < req_nice) {
		req_nice = nop_nice_value;
		cause_id = CPUNICE_CAUSE_NOP_ESCALATE;
	}

	/* trace entry only if changing */
	if (req_nice != cur_nice)
		trace_trusty_change_cpu_nice(cur_nice, req_nice, cause_id);

	/* tell Linux the desired priority */
	set_user_nice(current, req_nice);

	local_irq_restore(flags);
}

static void nop_work_func(struct trusty_work *tw, bool force)
{
	int ret;
	bool do_nop;
	u32 args[3];
	u32 last_arg0;
	struct trusty_state *s = tw->s;

	do_nop = force || dequeue_nop(s, args);

	if (do_nop) {
		/* we have been signaled or there's a nop so
		 * adjust priority before making SMC call below
		 */
		trusty_adjust_nice_nopreempt(s, do_nop);
	}

	while (do_nop) {
		dev_dbg(s->dev, "%s: %x %x %x\n",
			__func__, args[0], args[1], args[2]);

		if (kthread_should_park())
			kthread_parkme();

		preempt_disable();

		if (tw != this_cpu_ptr(s->nop_works)) {
			dev_warn_ratelimited(s->dev,
					     "trusty-nop-%d ran on wrong cpu, %u\n",
					     tw->cpu, smp_processor_id());
		}

		last_arg0 = args[0];
		ret = trusty_std_call32(s->dev, SMC_SC_NOP,
					args[0], args[1], args[2]);

		do_nop = dequeue_nop(s, args);

		/* adjust priority in case Trusty has requested a change */
		trusty_adjust_nice_nopreempt(s, do_nop);

		if (ret == SM_ERR_NOP_INTERRUPTED) {
			do_nop = true;
		} else if (ret != SM_ERR_NOP_DONE) {
			dev_err(s->dev, "%s: SMC_SC_NOP %x failed %d",
				__func__, last_arg0, ret);
			if (last_arg0) {
				/*
				 * Don't break out of the loop if a non-default
				 * nop-handler returns an error.
				 */
				do_nop = true;
			}
		}

		preempt_enable();
	}
	dev_dbg(s->dev, "%s: done\n", __func__);
}

void trusty_enqueue_nop(struct device *dev, struct trusty_nop *nop)
{
	unsigned long flags;
	struct trusty_work *tw;
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));
	int cur_nice;

	trace_trusty_enqueue_nop(nop);
	preempt_disable();
	tw = this_cpu_ptr(s->nop_works);
	if (nop) {
		WARN_ON(s->api_version < TRUSTY_API_VERSION_SMP_NOP);

		spin_lock_irqsave(&s->nop_lock, flags);
		if (list_empty(&nop->node))
			list_add_tail(&nop->node, &s->nop_queue);
		spin_unlock_irqrestore(&s->nop_lock, flags);
	}

	/* boost the priority here so the thread can get to it fast */
	cur_nice = task_nice(tw->nop_thread);
	if (nop_nice_value < cur_nice) {
		trace_trusty_change_cpu_nice(cur_nice, nop_nice_value,
				CPUNICE_CAUSE_ENQUEUE_BOOST);
		set_user_nice(tw->nop_thread, nop_nice_value);
	}

	/* indicate that this cpu was signaled */
	tw->signaled = true;

	wake_up_interruptible(&tw->nop_event_wait);
	preempt_enable();
}
EXPORT_SYMBOL(trusty_enqueue_nop);

void trusty_dequeue_nop(struct device *dev, struct trusty_nop *nop)
{
	unsigned long flags;
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	if (WARN_ON(!nop))
		return;

	spin_lock_irqsave(&s->nop_lock, flags);
	if (!list_empty(&nop->node))
		list_del_init(&nop->node);
	spin_unlock_irqrestore(&s->nop_lock, flags);
}
EXPORT_SYMBOL(trusty_dequeue_nop);

static int trusty_nop_thread(void *context)
{
	struct trusty_work *tw = context;
	struct trusty_state *s = tw->s;
	void (*work_func)(struct trusty_work *tw, bool force);
	int ret = 0;
	long timeout;

	DEFINE_WAIT_FUNC(wait, woken_wake_function);

	if (s->api_version < TRUSTY_API_VERSION_SMP)
		work_func = locked_nop_work_func;
	else
		work_func = nop_work_func;

	add_wait_queue(&tw->nop_event_wait, &wait);
	for (;;) {
		if (kthread_should_stop())
			break;

		timeout = msecs_to_jiffies(trusty_poll_period_ms);
		timeout = wait_woken(&wait, TASK_INTERRUPTIBLE, timeout);

		if (kthread_should_park())
			kthread_parkme();

		/* process work */
		work_func(tw, !timeout);
	};
	remove_wait_queue(&tw->nop_event_wait, &wait);

	return ret;
}

static int trusty_cpu_up(unsigned int cpu, struct hlist_node *node)
{
	struct trusty_state *s;
	struct trusty_work *tw;

	s = container_of(node, struct trusty_state, cpuhp_node);
	tw = this_cpu_ptr(s->nop_works);
	kthread_unpark(tw->nop_thread);

	dev_dbg(s->dev, "cpu %d up\n", cpu);

	return 0;
}

static int trusty_cpu_down(unsigned int cpu, struct hlist_node *node)
{
	struct trusty_state *s;
	struct trusty_work *tw;

	s = container_of(node, struct trusty_state, cpuhp_node);
	tw = this_cpu_ptr(s->nop_works);

	dev_dbg(s->dev, "cpu %d down\n", cpu);
	kthread_park(tw->nop_thread);

	return 0;
}

static int
trusty_transports_setup(const struct trusty_transport_desc **transports,
			struct device *dev)
{
	const struct trusty_transport_desc *transport;
	int ret;
	int transports_ret = -ENODEV;

	if (!transports)
		return -EINVAL;

	for (; (transport = *transports); transports++) {
		if (!transport || !transport->setup)
			return -EINVAL;

		ret = transport->setup(dev);
		if (ret == -EPROBE_DEFER) {
			dev_dbg(dev, "transport %s: defer probe\n",
				transport->name);
			return ret;
		}
		transports_ret &= ret;
	}

	/* One transport needs to complete setup without error. */
	if (transports_ret < 0)
		return -ENODEV;

	return 0;
}

static void
trusty_transports_cleanup(const struct trusty_transport_desc **transports,
			  struct device *dev)
{
	const struct trusty_transport_desc *transport;

	for (; (transport = *transports); transports++) {
		if (!transport->cleanup)
			continue;

		transport->cleanup(dev);
	}
}

#ifdef MTK_ADAPTED
u32 is_google_real_driver(void)
{
	return real_drv;
}
EXPORT_SYMBOL(is_google_real_driver);
#endif

static int trusty_probe(struct platform_device *pdev)
{
	int ret;
	unsigned int cpu;
	struct trusty_state *s;
	struct device_node *node = pdev->dev.of_node;
	const struct trusty_transport_desc **descs;

#ifdef MTK_ADAPTED
	if (!is_google_real_driver()) {
		dev_info(&pdev->dev, "%s: google trusty dummy driver\n", __func__);
		return 0;
	}
#endif

	if (!node) {
		dev_err(&pdev->dev, "of_node required\n");
		return -EINVAL;
	}

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s) {
		ret = -ENOMEM;
		goto err_allocate_state;
	}

	s->dev = &pdev->dev;
	spin_lock_init(&s->nop_lock);
	INIT_LIST_HEAD(&s->nop_queue);
	mutex_init(&s->smc_lock);
#ifndef CONFIG_TRUSTY_FFA_TRANSPORT
	mutex_init(&s->share_memory_msg_lock);
#endif
	ATOMIC_INIT_NOTIFIER_HEAD(&s->notifier);
	init_completion(&s->cpu_idle_completion);

	s->dev->dma_parms = &s->dma_parms;
	dma_set_max_seg_size(s->dev, 0xfffff000); /* dma_parms limit */
	/*
	 * Set dma mask to 48 bits. This is the current limit of
	 * trusty_encode_page_info.
	 */
	dma_coerce_mask_and_coherent(s->dev, DMA_BIT_MASK(48));

	platform_set_drvdata(pdev, s);

	/*
	 * Initialize Trusty transport. Trusty msg and mem ops has to be
	 * initialized as part of transport setup.
	 */
	descs = (const struct trusty_transport_desc **)of_device_get_match_data(&pdev->dev);
	ret = trusty_transports_setup(descs, s->dev);
	if (ret != 0 || !s->call_ops || !s->mem_ops)
		goto err_transport_setup;

	trusty_init_version(s, &pdev->dev);

	s->nop_works = alloc_percpu(struct trusty_work);
	if (!s->nop_works) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "Failed to allocate works\n");
		goto err_alloc_works;
	}

	for_each_possible_cpu(cpu) {
		struct trusty_work *tw = per_cpu_ptr(s->nop_works, cpu);

		tw->s = s;
		tw->cpu = cpu;
		tw->nop_thread = ERR_PTR(-EINVAL);
		init_waitqueue_head(&tw->nop_event_wait);
		tw->signaled = false;
	}

	for_each_possible_cpu(cpu) {
		struct trusty_work *tw = per_cpu_ptr(s->nop_works, cpu);

		tw->nop_thread = kthread_create_on_cpu(trusty_nop_thread, tw,
						       cpu, "trusty-nop-%d");
		if (IS_ERR(tw->nop_thread)) {
			ret = PTR_ERR(tw->nop_thread);
			dev_err(s->dev, "%s: failed to create thread for cpu= %d (%p)\n",
					__func__, cpu, tw->nop_thread);
			goto err_thread_create;
		}
		kthread_set_per_cpu(tw->nop_thread, cpu);
		kthread_park(tw->nop_thread);
	}

	ret = cpuhp_state_add_instance(trusty_cpuhp_slot, &s->cpuhp_node);
	if (ret < 0) {
		dev_err(&pdev->dev, "cpuhp_state_add_instance failed %d\n",
			ret);
		goto err_add_cpuhp_instance;
	}

	ret = trusty_alloc_sched_share(&pdev->dev, &s->trusty_sched_share_state);
	if (ret) {
		dev_err(s->dev, "%s: unabled to allocate sched memory (%d)\n",
				__func__, ret);
		goto err_alloc_sched_share;
	}

	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to add children: %d\n", ret);
		goto err_add_children;
	}

	/* attempt to share; it is optional for compatibility with Trusty
	 * versions that don't support priority sharing
	 */
	trusty_register_sched_share(s->dev, s->trusty_sched_share_state);

	return 0;

err_add_children:
	trusty_free_sched_share(s->trusty_sched_share_state);
err_alloc_sched_share:
	cpuhp_state_remove_instance(trusty_cpuhp_slot, &s->cpuhp_node);
err_add_cpuhp_instance:
err_thread_create:
	for_each_possible_cpu(cpu) {
		struct trusty_work *tw = per_cpu_ptr(s->nop_works, cpu);

		if (!IS_ERR(tw->nop_thread))
			kthread_stop(tw->nop_thread);
	}
	free_percpu(s->nop_works);
err_alloc_works:
	kfree(s->version_str);
	trusty_transports_cleanup(descs, s->dev);
err_transport_setup:
	s->dev->dma_parms = NULL;
	device_for_each_child(&pdev->dev, NULL, trusty_remove_child);
#ifndef CONFIG_TRUSTY_FFA_TRANSPORT
	mutex_destroy(&s->share_memory_msg_lock);
#endif
	mutex_destroy(&s->smc_lock);
	kfree(s);
err_allocate_state:
	return ret;
}

static void trusty_remove(struct platform_device *pdev)
{
	unsigned int cpu;
	struct trusty_state *s = platform_get_drvdata(pdev);
	const struct trusty_transport_desc **descs;

	trusty_unregister_sched_share(s->trusty_sched_share_state);

	device_for_each_child(&pdev->dev, NULL, trusty_remove_child);

	cpuhp_state_remove_instance(trusty_cpuhp_slot, &s->cpuhp_node);

	for_each_possible_cpu(cpu) {
		struct trusty_work *tw = per_cpu_ptr(s->nop_works, cpu);

		kthread_stop(tw->nop_thread);
	}
	free_percpu(s->nop_works);

	trusty_free_sched_share(s->trusty_sched_share_state);

#ifndef CONFIG_TRUSTY_FFA_TRANSPORT
	mutex_destroy(&s->share_memory_msg_lock);
#endif
	/* call transport cleanup */
	descs = (const struct trusty_transport_desc **)of_device_get_match_data(&pdev->dev);
	trusty_transports_cleanup(descs, s->dev);

	mutex_destroy(&s->smc_lock);
	s->dev->dma_parms = NULL;
	kfree(s->version_str);
	kfree(s);
}

/*
 * Trusty probe will try all compiled in transports and will use the transport
 * supported by the Trusty kernel.
 *
 * For Trusty API version < TRUSTY_API_VERSION_MEM_OBJ:
 *     trusty_smc_transport used for messaging.
 *
 * For Trusty API version >= TRUSTY_API_VERSION_MEM_OBJ:
 *     trusty_smc_transport used for messaging if direct messages are not supported.
 *     trusty_ffa_transport used for messaging otherwise.
 *     trusty_ffa_transport used for memory sharing.
 *
 */
static const struct trusty_transport_desc *trusty_transports[] = {
#ifdef CONFIG_TRUSTY_SMC_TRANSPORT
	&trusty_smc_transport,
#endif
#ifdef CONFIG_TRUSTY_FFA_TRANSPORT
	&trusty_ffa_transport,
#endif
	NULL,
};

static const struct of_device_id trusty_of_match[] = {
#ifdef MTK_ADAPTED
	{ .compatible = "android,google-trusty-smc-v1", .data = trusty_transports },
#else
	{ .compatible = "android,trusty-smc-v1", .data = trusty_transports },
#endif
	{},
};

MODULE_DEVICE_TABLE(trusty, trusty_of_match);

static struct platform_driver trusty_driver = {
	.probe = trusty_probe,
	.remove_new = trusty_remove,
	.driver	= {
#ifdef MTK_ADAPTED
		.name = "google-trusty",
#else
		.name = "trusty",
#endif
		.of_match_table = trusty_of_match,
		.dev_groups = trusty_groups,
	},
};

static int __init trusty_driver_init(void)
{
	int ret;
#ifdef MTK_ADAPTED
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "android,google-trusty-smc-v1");
	if (node) {
		ret = of_property_read_u32(node, "google,real-drv", &real_drv);
		if (ret || !real_drv)
			pr_info("%s: use google trusty dummy driver\n", __func__);
	} else {
		pr_info("%s: of_node required\n", __func__);
		return -EINVAL;
	}
#endif

#ifdef CONFIG_TRUSTY_FFA_TRANSPORT
#ifdef MTK_ADAPTED
	if (is_google_real_driver()) {
		ret = trusty_ffa_transport_init();
		if (ret < 0)
			goto err_ffa_transport_init;
	}
#else
	ret = trusty_ffa_transport_init();
	if (ret < 0)
		goto err_ffa_transport_init;
#endif
#endif

	/*
	 * Initialize trusty_irq_driver first since trusty_probe makes an
	 * std-call at the end where interrupts might be needed.
	 */
	ret = trusty_irq_driver_init();
	if (ret < 0)
		goto err_irq_driver_init;

	/* allocate dynamic cpuhp state slot */
	ret = cpuhp_setup_state_multi(CPUHP_AP_ONLINE_DYN,
				      "trusty:online",
				      trusty_cpu_up,
				      trusty_cpu_down);
	if (ret < 0)
		goto err_cpuhp_setup;

	trusty_cpuhp_slot = ret;

	ret = platform_driver_register(&trusty_driver);
	if (ret < 0)
		goto err_driver_register;

	return 0;

err_driver_register:
	cpuhp_remove_multi_state(trusty_cpuhp_slot);
	trusty_cpuhp_slot = -1;
err_cpuhp_setup:
err_irq_driver_init:
#ifdef CONFIG_TRUSTY_FFA_TRANSPORT
	trusty_ffa_transport_exit();
err_ffa_transport_init:
#endif
	return ret;
}

static void __exit trusty_driver_exit(void)
{
	platform_driver_unregister(&trusty_driver);
	cpuhp_remove_multi_state(trusty_cpuhp_slot);
	trusty_cpuhp_slot = -1;
#ifdef CONFIG_TRUSTY_FFA_TRANSPORT
	trusty_ffa_transport_exit();
#endif
	trusty_irq_driver_exit();
}

subsys_initcall(trusty_driver_init);
module_exit(trusty_driver_exit);

#define CREATE_TRACE_POINTS
#include "trusty-trace.h"

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Trusty core driver");
