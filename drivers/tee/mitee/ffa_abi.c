// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, Linaro Limited
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/arm_ffa.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/scatterlist.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/cpumask.h>

#include <tee_drv.h>
#include "optee_ffa.h"
#include "optee_private.h"
#include "optee_rpc_cmd.h"
#include "optee_bench.h"
#include "optee_smc.h"
#include "mitee_memlog.h"

struct mutex tee_mutex;
/*
 * This file implement the FF-A ABI used when communicating with secure world
 * OP-TEE OS via FF-A.
 * This file is divided into the following sections:
 * 1. Maintain a hash table for lookup of a global FF-A memory handle
 * 2. Convert between struct tee_param and struct optee_msg_param
 * 3. Low level support functions to register shared memory in secure world
 * 4. Dynamic shared memory pool based on alloc_pages()
 * 5. Do a normal scheduled call into secure world
 * 6. Driver initialization.
 */

/*
 * 1. Maintain a hash table for lookup of a global FF-A memory handle
 *
 * FF-A assigns a global memory handle for each piece shared memory.
 * This handle is then used when communicating with secure world.
 *
 * Main functions are optee_shm_add_ffa_handle() and optee_shm_rem_ffa_handle()
 */
struct shm_rhash {
	struct tee_shm *shm;
	u64 global_id;
	struct rhash_head linkage;
};

static void rh_free_fn(void *ptr, void *arg)
{
	kfree(ptr);
}

static const struct rhashtable_params shm_rhash_params = {
	.head_offset = offsetof(struct shm_rhash, linkage),
	.key_len = sizeof(u64),
	.key_offset = offsetof(struct shm_rhash, global_id),
	.automatic_shrinking = true,
};

static struct tee_shm *optee_shm_from_ffa_handle(struct optee *optee,
						 u64 global_id)
{
	struct tee_shm *shm = NULL;
	struct shm_rhash *r;

	mutex_lock(&optee->ffa.mutex);
	r = rhashtable_lookup_fast(&optee->ffa.global_ids, &global_id,
				   shm_rhash_params);
	if (r)
		shm = r->shm;
	mutex_unlock(&optee->ffa.mutex);

	return shm;
}

static int optee_shm_add_ffa_handle(struct optee *optee, struct tee_shm *shm,
				    u64 global_id)
{
	struct shm_rhash *r;
	int rc;

	r = kmalloc(sizeof(*r), GFP_KERNEL);
	if (!r)
		return -ENOMEM;
	r->shm = shm;
	r->global_id = global_id;

	mutex_lock(&optee->ffa.mutex);
	rc = rhashtable_lookup_insert_fast(&optee->ffa.global_ids, &r->linkage,
					   shm_rhash_params);
	mutex_unlock(&optee->ffa.mutex);

	if (rc)
		kfree(r);

	return rc;
}

static int optee_shm_rem_ffa_handle(struct optee *optee, u64 global_id)
{
	struct shm_rhash *r;
	int rc = -ENOENT;

	mutex_lock(&optee->ffa.mutex);
	r = rhashtable_lookup_fast(&optee->ffa.global_ids, &global_id,
				   shm_rhash_params);
	if (r)
		rc = rhashtable_remove_fast(&optee->ffa.global_ids, &r->linkage,
					    shm_rhash_params);
	mutex_unlock(&optee->ffa.mutex);

	if (!rc)
		kfree(r);

	return rc;
}

/*
 * 2. Convert between struct tee_param and struct optee_msg_param
 *
 * optee_ffa_from_msg_param() and optee_ffa_to_msg_param() are the main
 * functions.
 */

static void from_msg_param_ffa_mem(struct optee *optee, struct tee_param *p,
				   u32 attr, const struct optee_msg_param *mp)
{
	struct tee_shm *shm = NULL;
	u64 offs_high = 0;
	u64 offs_low = 0;

	p->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT + attr -
		  OPTEE_MSG_ATTR_TYPE_FMEM_INPUT;
	p->u.memref.size = mp->u.fmem.size;

	if (mp->u.fmem.global_id != OPTEE_MSG_FMEM_INVALID_GLOBAL_ID)
		shm = optee_shm_from_ffa_handle(optee, mp->u.fmem.global_id);
	p->u.memref.shm = shm;

	if (shm) {
		offs_low = mp->u.fmem.offs_low;
		offs_high = mp->u.fmem.offs_high;
	}
	p->u.memref.shm_offs = offs_low | offs_high << 32;
}

/**
 * optee_ffa_from_msg_param() - convert from OPTEE_MSG parameters to
 *				struct tee_param
 * @optee:	main service struct
 * @params:	subsystem internal parameter representation
 * @num_params:	number of elements in the parameter arrays
 * @msg_params:	OPTEE_MSG parameters
 *
 * Returns 0 on success or <0 on failure
 */
static int optee_ffa_from_msg_param(struct optee *optee,
				    struct tee_param *params, size_t num_params,
				    const struct optee_msg_param *msg_params)
{
	size_t n;
	int rc = 0;

	for (n = 0; n < num_params; n++) {
		struct tee_param *p = params + n;
		const struct optee_msg_param *mp = msg_params + n;
		u32 attr = mp->attr & OPTEE_MSG_ATTR_TYPE_MASK;

		switch (attr) {
		case OPTEE_MSG_ATTR_TYPE_NONE:
			p->attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
			memset(&p->u, 0, sizeof(p->u));
			break;
		case OPTEE_MSG_ATTR_TYPE_VALUE_INPUT:
		case OPTEE_MSG_ATTR_TYPE_VALUE_OUTPUT:
		case OPTEE_MSG_ATTR_TYPE_VALUE_INOUT:
			optee_from_msg_param_value(p, attr, mp);
			break;
		case OPTEE_MSG_ATTR_TYPE_TMEM_INPUT:
		case OPTEE_MSG_ATTR_TYPE_TMEM_OUTPUT:
		case OPTEE_MSG_ATTR_TYPE_TMEM_INOUT:
			rc = from_msg_param_tmp_mem(p, attr, mp);
			if (rc) {
				pr_err("%s: tmp mem err: %d!\n", __func__, rc);
				return rc;
			}
			break;
		case OPTEE_MSG_ATTR_TYPE_FMEM_INPUT:
		case OPTEE_MSG_ATTR_TYPE_FMEM_OUTPUT:
		case OPTEE_MSG_ATTR_TYPE_FMEM_INOUT:
			from_msg_param_ffa_mem(optee, p, attr, mp);
			break;
		/*rpc msg do not support RMEM*/
		default:
			pr_err("%s: unsupported mem type: %d!\n", __func__,
			       attr);
			return -EINVAL;
		}
	}

	return 0;
}

static int to_msg_param_ffa_mem(struct optee_msg_param *mp,
				const struct tee_param *p)
{
	struct tee_shm *shm = p->u.memref.shm;

	mp->attr = OPTEE_MSG_ATTR_TYPE_FMEM_INPUT + p->attr -
		   TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;

	if (shm) {
		u64 shm_offs = p->u.memref.shm_offs;

		mp->u.fmem.internal_offs = shm->offset;

		mp->u.fmem.offs_low = shm_offs;
		mp->u.fmem.offs_high = shm_offs >> 32;
		/* Check that the entire offset could be stored. */
		if (mp->u.fmem.offs_high != shm_offs >> 32)
			return -EINVAL;

		mp->u.fmem.global_id = shm->sec_world_id;
	} else {
		memset(&mp->u, 0, sizeof(mp->u));
		mp->u.fmem.global_id = OPTEE_MSG_FMEM_INVALID_GLOBAL_ID;
	}
	mp->u.fmem.size = p->u.memref.size;

	return 0;
}

/**
 * optee_ffa_to_msg_param() - convert from struct tee_params to OPTEE_MSG
 *			      parameters
 * @optee:	main service struct
 * @msg_params:	OPTEE_MSG parameters
 * @num_params:	number of elements in the parameter arrays
 * @params:	subsystem itnernal parameter representation
 * Returns 0 on success or <0 on failure
 */
static int optee_ffa_to_msg_param(struct optee *optee,
				  struct optee_msg_param *msg_params,
				  size_t num_params,
				  const struct tee_param *params)
{
	size_t n;
	int rc = 0;

	for (n = 0; n < num_params; n++) {
		const struct tee_param *p = params + n;
		struct optee_msg_param *mp = msg_params + n;

		switch (p->attr) {
		case TEE_IOCTL_PARAM_ATTR_TYPE_NONE:
			mp->attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
			memset(&mp->u, 0, sizeof(mp->u));
			break;
		case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INOUT:
			optee_to_msg_param_value(mp, p);
			break;
		case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT:
			if (tee_shm_is_registered(p->u.memref.shm))
				rc = to_msg_param_ffa_mem(mp, p);
			else
				rc = to_msg_param_tmp_mem(mp, p);
			if (rc) {
				pr_err("%s attr: %#llx failed: %d!\n", __func__,
				       p->attr, rc);
				return rc;
			}
			break;
		default:
			pr_err("%s unsupport attr: %#llx!\n", __func__,
			       p->attr);
			return -EINVAL;
		}
	}

	return 0;
}

/*
 * 3. Low level support functions to register shared memory in secure world
 *
 * Functions to register and unregister shared memory both for normal
 * clients and for tee-supplicant.
 */

static int optee_ffa_shm_register(struct tee_context *ctx, struct tee_shm *shm,
				  struct page **pages, size_t num_pages,
				  unsigned long start)
{
	struct optee *optee = tee_get_drvdata(ctx->teedev);
	struct ffa_device *ffa_dev = optee->ffa.ffa_dev;
	const struct ffa_mem_ops *mem_ops = ffa_dev->ops->mem_ops;
	struct ffa_mem_region_attributes mem_attr = {
		.receiver = ffa_dev->vm_id,
		.attrs = FFA_MEM_RW,
	};
	struct ffa_mem_ops_args args = {
		.use_txbuf = true,
		.attrs = &mem_attr,
		.nattrs = 1,
	};
	struct sg_table sgt;
	int rc;
#ifdef MITEE_SHM_DEBUG
	int iter = 0;
#endif

	if (optee->supp_teedev == ctx->teedev) {
		pr_err("%s do not support supp !\n", __func__);
		return -EACCES;
	}

	rc = optee_check_mem_type(start, num_pages);
	if (rc)
		return rc;

#ifdef MITEE_SHM_DEBUG
	pr_info("%s dump page list\n", __func__);
	for (iter = 0; iter < num_pages; iter++) {
		pr_info("page pfn: %#lx\n", page_to_pfn(pages[iter]));
	}
#endif

	rc = sg_alloc_table_from_pages(&sgt, pages, num_pages, 0,
				       num_pages * PAGE_SIZE, GFP_KERNEL);
	if (rc)
		return rc;
	args.sg = sgt.sgl;
	rc = mem_ops->memory_share(&args);
	sg_free_table(&sgt);
	if (rc)
		return rc;

	rc = optee_shm_add_ffa_handle(optee, shm, args.g_handle);
	if (rc) {
		pr_err("%s add handle failed: %d!\n", __func__, rc);
		mem_ops->memory_reclaim(args.g_handle, 0);
		return rc;
	}

#ifdef MITEE_SHM_DEBUG
	pr_info("%s get handle: %#llx!\n", __func__, args.g_handle);
#endif
	shm->sec_world_id = args.g_handle;

	return 0;
}

static int optee_ffa_yielding_call(struct tee_context *ctx,
				   struct ffa_send_direct_data *data,
				   struct optee_msg_arg *rpc_arg
				   __attribute__((unused)));

static int optee_ffa_shm_unregister(struct tee_context *ctx,
				    struct tee_shm *shm)
{
	struct optee *optee = tee_get_drvdata(ctx->teedev);
	struct ffa_device *ffa_dev = optee->ffa.ffa_dev;
	const struct ffa_mem_ops *mem_ops = ffa_dev->ops->mem_ops;
	u64 global_handle = shm->sec_world_id;
	struct optee_msg_arg *msg_arg = NULL;
	struct tee_shm *shm_arg = NULL;
	struct ffa_send_direct_data data;
	phys_addr_t parg;
	int rc;

	shm_arg = optee_get_msg_arg(ctx, 1, &msg_arg);
	if (IS_ERR(shm_arg))
		return PTR_ERR(shm_arg);

	rc = tee_shm_get_pa(shm_arg, 0, &parg);
	if (rc) {
		pr_err("%s: failed rc %d\n", __func__, rc);
		tee_shm_free(shm_arg);
		return rc;
	}

	msg_arg->cmd = OPTEE_MSG_CMD_UNREGISTER_SHM;
	msg_arg->params[0].attr = OPTEE_MSG_ATTR_TYPE_FMEM_INPUT;
	msg_arg->params[0].u.fmem.global_id = global_handle;

	data.data0 = OPTEE_FFA_YIELDING_CALL_WITH_ARG;
	data.data1 = (u32)(parg >> 32);
	data.data2 = (u32)parg;

	if (DEBUG_CALL)
		pr_info("%s data1: %#lx data2: %#lx!\n", __func__, data.data1,
			data.data2);

	optee_shm_rem_ffa_handle(optee, global_handle);
	shm->sec_world_id = 0;

	/* change from fastcall to yielding call */
	rc = optee_ffa_yielding_call(ctx, &data, NULL);
	if (rc)
		pr_err("Unregister SHM id 0x%llx rc %d\n", global_handle, rc);

	rc = mem_ops->memory_reclaim(global_handle, 0);
	if (rc)
		pr_err("mem_reclain: 0x%llx %d", global_handle, rc);

	tee_shm_free(shm_arg);

	return rc;
}

static int optee_ffa_shm_unregister_supp(struct tee_context *ctx,
					 struct tee_shm *shm)
{
	struct optee *optee = tee_get_drvdata(ctx->teedev);
	const struct ffa_mem_ops *mem_ops;
	u64 global_handle = shm->sec_world_id;
	int rc;

	/*
	 * We're skipping the OPTEE_FFA_YIELDING_CALL_UNREGISTER_SHM call
	 * since this is OP-TEE freeing via RPC so it has already retired
	 * this ID.
	 */

	optee_shm_rem_ffa_handle(optee, global_handle);
	mem_ops = optee->ffa.ffa_dev->ops->mem_ops;
	rc = mem_ops->memory_reclaim(global_handle, 0);
	if (rc)
		pr_err("mem_reclain: 0x%llx %d", global_handle, rc);

	shm->sec_world_id = 0;

	return rc;
}

/*
 * 4. Dynamic shared memory pool based on alloc_pages()
 *
 * Implements an OP-TEE specific shared memory pool.
 * The main function is optee_ffa_shm_pool_alloc_pages().
 */

static int pool_ffa_op_alloc(struct tee_shm_pool_mgr *poolm,
			     struct tee_shm *shm, size_t size)
{
	return optee_pool_op_alloc_helper(poolm, shm, size,
					  optee_ffa_shm_register);
}

static void pool_ffa_op_free(struct tee_shm_pool_mgr *poolm,
			     struct tee_shm *shm)
{
	optee_ffa_shm_unregister(shm->ctx, shm);
	free_pages((unsigned long)shm->kaddr, get_order(shm->size));
	shm->kaddr = NULL;
}

static void pool_ffa_op_destroy_poolmgr(struct tee_shm_pool_mgr *poolm)
{
	kfree(poolm);
}

static const struct tee_shm_pool_mgr_ops pool_ffa_ops = {
	.alloc = pool_ffa_op_alloc,
	.free = pool_ffa_op_free,
	.destroy_poolmgr = pool_ffa_op_destroy_poolmgr,
};

/**
 * optee_ffa_shm_pool_alloc_pages() - create page-based allocator pool
 *
 * This pool is used with OP-TEE over FF-A. In this case command buffers
 * and such are allocated from kernel's own memory.
 */
static struct tee_shm_pool_mgr *optee_ffa_shm_pool_alloc_pages(void)
{
	struct tee_shm_pool_mgr *mgr = kzalloc(sizeof(*mgr), GFP_KERNEL);

	if (!mgr)
		return ERR_PTR(-ENOMEM);

	mgr->ops = &pool_ffa_ops;

	return mgr;
}

/*
 * 5. Do a normal scheduled call into secure world
 *
 * The function optee_ffa_do_call_with_arg() performs a normal scheduled
 * call into secure world. During this call may normal world request help
 * from normal world using RPCs, Remote Procedure Calls. This includes
 * delivery of non-secure interrupts to for instance allow rescheduling of
 * the current task.
 */

static void handle_ffa_rpc_func_cmd_shm_alloc(struct tee_context *ctx,
					      struct optee_msg_arg *arg)
{
	struct tee_shm *shm;

	if (arg->num_params != 1 ||
	    arg->params[0].attr != OPTEE_MSG_ATTR_TYPE_VALUE_INPUT) {
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
		return;
	}

	switch (arg->params[0].u.value.a) {
	case OPTEE_RPC_SHM_TYPE_APPL:
		shm = optee_rpc_cmd_alloc_suppl(ctx, arg->params[0].u.value.b);
		break;
	case OPTEE_RPC_SHM_TYPE_KERNEL:
		shm = tee_shm_alloc(ctx, arg->params[0].u.value.b,
				    TEE_SHM_MAPPED | TEE_SHM_PRIV);
		break;
	default:
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
		return;
	}

	if (IS_ERR(shm)) {
		arg->ret = TEEC_ERROR_OUT_OF_MEMORY;
		return;
	}

	arg->params[0] = (struct optee_msg_param){
		.attr = OPTEE_MSG_ATTR_TYPE_TMEM_OUTPUT,
		.u.tmem.buf_ptr = shm->paddr,
		.u.tmem.size = tee_shm_get_size(shm),
		.u.tmem.shm_ref = (unsigned long)shm,
	};

	arg->ret = TEEC_SUCCESS;
}

static void handle_ffa_rpc_func_cmd_shm_free(struct tee_context *ctx,
					     struct optee *optee,
					     struct optee_msg_arg *arg)
{
	struct tee_shm *shm;

	if (arg->num_params != 1 ||
	    arg->params[0].attr != OPTEE_MSG_ATTR_TYPE_VALUE_INPUT)
		goto err_bad_param;

	//shm = optee_shm_from_ffa_handle(optee, arg->params[0].u.value.b);
	shm = (struct tee_shm *)arg->params[0].u.value.b;
	if (!shm)
		goto err_bad_param;
	switch (arg->params[0].u.value.a) {
	case OPTEE_RPC_SHM_TYPE_APPL:
		optee_rpc_cmd_free_suppl(ctx, shm);
		break;
	case OPTEE_RPC_SHM_TYPE_KERNEL:
		tee_shm_free(shm);
		break;
	default:
		goto err_bad_param;
	}
	arg->ret = TEEC_SUCCESS;
	return;

err_bad_param:
	arg->ret = TEEC_ERROR_BAD_PARAMETERS;
}

/*RPC cmd message only support transfer in reserved memory*/
static void handle_ffa_rpc_func_cmd(struct tee_context *ctx,
				    struct tee_shm *shm)
{
	struct optee_msg_arg *arg;
	struct optee *optee = tee_get_drvdata(ctx->teedev);

	arg = tee_shm_get_va(shm, 0);
	if (IS_ERR(arg)) {
		pr_err("%s: tee_shm_get_va %p failed\n", __func__, shm);
		return;
	}

	if (DEBUG_CALL)
		pr_info("%s: rpc inner cmd=%x\n", __func__, arg->cmd);

	arg->ret_origin = TEEC_ORIGIN_COMMS;
	switch (arg->cmd) {
	case OPTEE_RPC_CMD_SHM_ALLOC:
		handle_ffa_rpc_func_cmd_shm_alloc(ctx, arg);
		break;
	case OPTEE_RPC_CMD_SHM_FREE:
		handle_ffa_rpc_func_cmd_shm_free(ctx, optee, arg);
		break;
	default:
		optee_rpc_cmd(ctx, optee, arg);
	}
}

static void optee_handle_ffa_rpc(struct tee_context *ctx, u32 cmd,
				 struct ffa_send_direct_data *data)
{
	struct tee_shm *shm;
	phys_addr_t pa;

	if (DEBUG_CALL)
		pr_info("%s: rpc cmd=%x\n", __func__, cmd);

	switch (cmd) {
	/*MITEE support reserved shm alloc/free in rpc call*/
	case OPTEE_FFA_YIELDING_CALL_RETURN_RPC_ALLOC:
		shm = tee_shm_alloc(ctx, data->data3, TEE_SHM_MAPPED);
		if (!IS_ERR(shm) && !tee_shm_get_pa(shm, 0, &pa)) {
			data->data1 = OPTEE_FFA_YIELDING_CALL_RETURN_RPC_ALLOC;
			data->data3 = pa;
			data->data4 = (unsigned long)shm;
			if (DEBUG_CALL)
				pr_info("%s shmptr=%#lx pa=%#lx\n", __func__,
					(unsigned long)shm, data->data3);
		} else {
			data->data1 = OPTEE_FFA_YIELDING_CALL_RETURN_RPC_ALLOC;
			data->data3 = 0;
			data->data4 = 0;
		}
		break;
	case OPTEE_FFA_YIELDING_CALL_RETURN_RPC_FREE:
		shm = (struct tee_shm *)data->data3;
		tee_shm_free(shm);
		/*must be clear*/
		data->data1 = OPTEE_FFA_YIELDING_CALL_RETURN_RPC_FREE;
		data->data3 = 0;
		data->data4 = 0;
		break;
	case OPTEE_FFA_YIELDING_CALL_RETURN_RPC_CMD:
		shm = (struct tee_shm *)data->data3;
		handle_ffa_rpc_func_cmd(ctx, shm);
		/*must be clear*/
		data->data1 = OPTEE_FFA_YIELDING_CALL_RETURN_RPC_CMD;
		data->data3 = 0;
		data->data4 = 0;
		break;
	case OPTEE_FFA_YIELDING_CALL_RETURN_INTERRUPT:
		/* Interrupt delivered by now */
		break;
	default:
		pr_err("%s: Unknown RPC func 0x%x\n", __func__, cmd);
		break;
	}
}

static int optee_ffa_yielding_call(struct tee_context *ctx,
				   struct ffa_send_direct_data *data,
				   struct optee_msg_arg *rpc_arg
				   __attribute__((unused)))
{
	struct optee *optee = tee_get_drvdata(ctx->teedev);
	struct ffa_device *ffa_dev = optee->ffa.ffa_dev;
	const struct ffa_msg_ops *msg_ops = ffa_dev->ops->msg_ops;
	struct optee_call_waiter w;
	struct cpumask save_mask_enter = {};
	long prev_nice = task_nice(current);
	struct cpumask save_mask;
	bool need_restore_mask = false;
	u32 cmd = data->data0;
	u32 w4 = data->data1;
	u32 w5 = data->data2;
	u32 w6 = data->data3;
	int rc;

	if (DEBUG_CALL)
		pr_info("%s data0:%x data1:%x data2:%x data3:%x\n", __func__,
			cmd, w4, w5, w6);
	/* Initialize waiter */
	optee_cq_wait_init(&optee->call_queue, &w);
	mutex_lock(&tee_mutex);

	memcpy(&save_mask_enter, &(current->cpus_mask), sizeof(struct cpumask));

	if ((current->flags & PF_NO_SETAFFINITY) != PF_NO_SETAFFINITY) {
		rc = set_cpus_allowed_ptr(current, &optee->cpus_allowed);
		if (rc)
			pr_warn("ep binding core failed(%d), cpu list: %*pbl\n",
					rc, cpumask_pr_args(cpu_online_mask));
	}
	set_user_nice(current, -20);

	while (true) {
		optee_bm_timestamp();
		rc = msg_ops->sync_send_receive(ffa_dev, data);
		if (rc)
			goto done;
		atomic_notifier_call_chain(&optee->notifier,
					   MITEE_CALL_RETURNED, NULL);
		optee_bm_timestamp();

		switch ((int)data->data0) {
		case TEEC_SUCCESS:
			break;
		case TEEC_ERROR_BUSY:
			if (cmd == OPTEE_FFA_YIELDING_CALL_RESUME) {
				rc = -EIO;
				goto done;
			}

			/*
			 * Out of threads in secure world, wait for a thread
			 * become available.
			 */
			optee_cq_wait_for_completion(&optee->call_queue, &w);
			data->data0 = cmd;
			data->data1 = w4;
			data->data2 = w5;
			data->data3 = w6;
			continue;
		default:
			pr_warn("%s receive err: %#lx\n", __func__,
				data->data0);
			rc = -EIO;
			goto done;
		}

		if (data->data1 == OPTEE_FFA_YIELDING_CALL_RETURN_DONE) {
			if (need_restore_mask) {
				set_cpus_allowed_ptr(current, &save_mask);
			}
			goto done;
		}

		/*
		 * OP-TEE has returned with a RPC request.
		 *
		 * Note that data->data4 (passed in register w7) is already
		 * filled in by ffa_mem_ops->sync_send_receive() returning
		 * above.
		 */
		cond_resched();

		if (data->data2 >= NR_CPUS) {
			pr_err("unknown cpu %#lx\n", data->data2);
		} else if (!need_restore_mask) {
			// RPC out, bind to the core last in TEE
			memcpy(&save_mask, &(current->cpus_mask),
			       sizeof(struct cpumask));
			rc = set_cpus_allowed_ptr(current,
					     get_cpu_mask(data->data2));
			if (rc)
				pr_warn("rpc binding core failed(%d), cpu list: %*pbl\n",
						rc, cpumask_pr_args(cpu_online_mask));
			need_restore_mask = true;
		}
		optee_handle_ffa_rpc(ctx, data->data1, data);
		cmd = OPTEE_FFA_YIELDING_CALL_RESUME;
		data->data0 = cmd;
		/*data2 is cpu id, data1/data3/data4 should be filled in optee_handle_ffa_rpc*/
	}
done:
	/*
	 * We're done with our thread in secure world, if there's any
	 * thread waiters wake up one.
	 */
	optee_cq_wait_final(&optee->call_queue, &w);

	set_cpus_allowed_ptr(current, &save_mask_enter);
	set_user_nice(current, prev_nice);
	mutex_unlock(&tee_mutex);

	return rc;
}

/**
 * optee_ffa_do_call_with_arg() - Do a FF-A call to enter OP-TEE in secure world
 * @ctx:	calling context
 * @shm:	shared memory holding the message to pass to secure world
 *
 * Does a FF-A call to OP-TEE in secure world and handles eventual resulting
 * Remote Procedure Calls (RPC) from OP-TEE.
 *
 * Returns return code from FF-A, 0 is OK
 */

static int optee_ffa_do_call_with_arg(struct tee_context *ctx,
				      struct tee_shm *shm)
{
	struct ffa_send_direct_data data = {
		.data0 = OPTEE_FFA_YIELDING_CALL_WITH_ARG,
		.data1 = (u32)(shm->paddr >> 32),
		.data2 = (u32)shm->paddr,
		.data3 = shm->offset,
	};
#if 1
	struct optee_msg_arg *rpc_arg = NULL;
#else
	/*removed, cause mitee do not support per-thread rpc param*/
	struct optee_msg_arg *arg = tee_shm_get_va(shm, 0);
	unsigned int rpc_arg_offs = OPTEE_MSG_GET_ARG_SIZE(arg->num_params);
	struct optee_msg_arg *rpc_arg = tee_shm_get_va(shm, rpc_arg_offs);
#endif

	return optee_ffa_yielding_call(ctx, &data, rpc_arg);
}

static struct tee_shm_pool *optee_ffa_shm_memremap(struct ffa_device *ffa_dev,
						   const struct ffa_ops *ops,
						   void **memremaped_shm)
{
	struct ffa_send_direct_data data = { OPTEE_FFA_GET_SHM_CONFIG };
	unsigned long vaddr;
	phys_addr_t paddr;
	size_t size;
	phys_addr_t begin;
	phys_addr_t end;
	void *va;
	struct tee_shm_pool_mgr *priv_mgr;
	struct tee_shm_pool_mgr *dmabuf_mgr;
	int rc = 0;
	void *pool = NULL;
	const int sz = OPTEE_SHM_NUM_PRIV_PAGES * PAGE_SIZE;
	const struct ffa_msg_ops *msg_ops = ops->msg_ops;

	/*
	 * data0: start
	 * data1: size
	 * data2: settings
	 * fast call, cannot be interrupted
	 */
	rc = msg_ops->sync_send_receive(ffa_dev, &data);
	if (rc) {
		pr_err("Unexpected error %d", rc);
		return NULL;
	}

	if (data.data2 != OPTEE_SMC_SHM_CACHED) {
		pr_err("only normal cached shared memory supported\n");
		return ERR_PTR(-EINVAL);
	}

	pr_info("get shm pool: %lx size: %lx\n", data.data0, data.data1);

	begin = roundup(data.data0, PAGE_SIZE);
	end = rounddown(data.data0 + data.data1, PAGE_SIZE);
	paddr = begin;
	size = end - begin;

	if (size < 2 * OPTEE_SHM_NUM_PRIV_PAGES * PAGE_SIZE) {
		pr_err("too small shared memory area\n");
		return ERR_PTR(-EINVAL);
	}

	va = memremap(paddr, size, MEMREMAP_WB);
	if (!va) {
		pr_err("shared memory ioremap failed\n");
		return ERR_PTR(-EINVAL);
	}
	vaddr = (unsigned long)va;

	pool = tee_shm_pool_mgr_alloc_res_mem(vaddr, paddr, sz,
					      3 /* 8 bytes aligned */);
	if (IS_ERR(pool))
		goto err_memunmap;
	priv_mgr = pool;

	vaddr += sz;
	paddr += sz;
	size -= sz;

	pool = tee_shm_pool_mgr_alloc_res_mem(vaddr, paddr, size, PAGE_SHIFT);
	if (IS_ERR(pool))
		goto err_free_priv_mgr;
	dmabuf_mgr = pool;

	pool = tee_shm_pool_alloc(priv_mgr, dmabuf_mgr);
	if (IS_ERR(pool))
		goto err_free_dmabuf_mgr;

	*memremaped_shm = va;

	return pool;

err_free_dmabuf_mgr:
	tee_shm_pool_mgr_destroy(dmabuf_mgr);
err_free_priv_mgr:
	tee_shm_pool_mgr_destroy(priv_mgr);
err_memunmap:
	memunmap(va);
	return pool;
}

#if MITEE_FEATURE_CAP_ENABLE
/*
 * 6. Driver initialization
 *
 * During driver inititialization is the OP-TEE Secure Partition is probed
 * to find out which features it supports so the driver can be initialized
 * with a matching configuration.
 */

static bool optee_ffa_api_is_compatbile(struct ffa_device *ffa_dev,
					const struct ffa_ops *ops)
{
	const struct ffa_msg_ops *msg_ops = ops->msg_ops;
	struct ffa_send_direct_data data = { OPTEE_FFA_GET_API_VERSION };
	int rc;

	msg_ops->mode_32bit_set(ffa_dev);

	rc = msg_ops->sync_send_receive(ffa_dev, &data);
	if (rc) {
		pr_err("Unexpected error %d\n", rc);
		return false;
	}
	if (data.data0 != OPTEE_FFA_VERSION_MAJOR ||
	    data.data1 < OPTEE_FFA_VERSION_MINOR) {
		pr_err("Incompatible OP-TEE API version %lu.%lu", data.data0,
		       data.data1);
		return false;
	}

	data = (struct ffa_send_direct_data){ OPTEE_FFA_GET_OS_VERSION };
	rc = msg_ops->sync_send_receive(ffa_dev, &data);
	if (rc) {
		pr_err("Unexpected error %d\n", rc);
		return false;
	}
	if (data.data2)
		pr_info("revision %lu.%lu (%08lx)", data.data0, data.data1,
			data.data2);
	else
		pr_info("revision %lu.%lu", data.data0, data.data1);

	return true;
}

static bool optee_ffa_exchange_caps(struct ffa_device *ffa_dev,
				    const struct ffa_ops *ops,
				    unsigned int *rpc_arg_count)
{
	struct ffa_send_direct_data data = { OPTEE_FFA_EXCHANGE_CAPABILITIES };
	int rc;

	rc = ops->msg_ops->sync_send_receive(ffa_dev, &data);
	if (rc) {
		pr_err("Unexpected error %d", rc);
		return false;
	}
	if (data.data0) {
		pr_err("Unexpected exchange error %lu", data.data0);
		return false;
	}

	*rpc_arg_count = (u8)data.data1;

	return true;
}
#endif

static struct tee_shm_pool *optee_ffa_config_dyn_shm(void)
{
	struct tee_shm_pool_mgr *priv_mgr;
	struct tee_shm_pool_mgr *dmabuf_mgr;
	void *rc;

	rc = optee_ffa_shm_pool_alloc_pages();
	if (IS_ERR(rc))
		return rc;
	priv_mgr = rc;

	rc = optee_ffa_shm_pool_alloc_pages();
	if (IS_ERR(rc)) {
		tee_shm_pool_mgr_destroy(priv_mgr);
		return rc;
	}
	dmabuf_mgr = rc;

	rc = tee_shm_pool_alloc(priv_mgr, dmabuf_mgr);
	if (IS_ERR(rc)) {
		tee_shm_pool_mgr_destroy(priv_mgr);
		tee_shm_pool_mgr_destroy(dmabuf_mgr);
	}

	return rc;
}

static void optee_ffa_get_version(struct tee_device *teedev,
				  struct tee_ioctl_version_data *vers)
{
	struct tee_ioctl_version_data v = {
		.impl_id = TEE_IMPL_ID_OPTEE,
		.impl_caps = TEE_OPTEE_CAP_TZ,
		.gen_caps = TEE_GEN_CAP_GP,
	};

	struct optee *optee = tee_get_drvdata(teedev);

	if (teedev != optee->supp_teedev)
		v.gen_caps |= TEE_GEN_CAP_REG_MEM;

	*vers = v;
}

static int optee_ffa_open(struct tee_context *ctx)
{
	return optee_open(ctx, true);
}

static const struct tee_driver_ops optee_ffa_clnt_ops = {
	.get_version = optee_ffa_get_version,
	.open = optee_ffa_open,
	.release = optee_release,
	.open_session = optee_open_session,
	.close_session = optee_close_session,
	.invoke_func = optee_invoke_func,
	.cancel_req = optee_cancel_req,
	.shm_register = optee_ffa_shm_register,
	.shm_unregister = optee_ffa_shm_unregister,
};

static const struct tee_desc optee_ffa_clnt_desc = {
	.name = DRIVER_NAME "-ffa-clnt",
	.ops = &optee_ffa_clnt_ops,
	.owner = THIS_MODULE,
};

static const struct tee_driver_ops optee_ffa_supp_ops = {
	.get_version = optee_ffa_get_version,
	.open = optee_ffa_open,
	.release = optee_release_supp,
	.supp_recv = optee_supp_recv,
	.supp_send = optee_supp_send,
	.shm_register = optee_ffa_shm_register, /* same as for clnt ops */
	.shm_unregister = optee_ffa_shm_unregister_supp,
};

static const struct tee_desc optee_ffa_supp_desc = {
	.name = DRIVER_NAME "-ffa-supp",
	.ops = &optee_ffa_supp_ops,
	.owner = THIS_MODULE,
	.flags = TEE_DESC_PRIVILEGED,
};

static const struct optee_ops optee_ffa_ops = {
	.do_call_with_arg = optee_ffa_do_call_with_arg,
	.to_msg_param = optee_ffa_to_msg_param,
	.from_msg_param = optee_ffa_from_msg_param,
};

static void optee_ffa_remove(struct ffa_device *ffa_dev)
{
	struct optee *optee = ffa_dev->dev.driver_data;

	optee_remove_common(optee);

	mutex_destroy(&tee_mutex);
	mutex_destroy(&optee->ffa.mutex);
	rhashtable_free_and_destroy(&optee->ffa.global_ids, rh_free_fn, NULL);

	kfree(optee);
	optee_bm_disable();
}

static struct optee *optee_svc;

struct optee *get_optee_drv_state(void)
{
	return optee_svc;
}

/* bind task to specific cores, typ*/
static int mitee_get_cpu_allows(struct device_node* node, struct cpumask *cpus_allowed)
{
	int rc = 0;
	int cpus_num = 0;
	int i = 0;
	u32 *cpu = NULL;

	cpumask_clear(cpus_allowed);

	cpus_num = of_property_count_u32_elems(node, MITEE_CORE_ID);
	if (cpus_num <= 0) {
		pr_info("no cpu limitation(%d), execute freely\n", cpus_num);
		/* defualt value */
		memcpy(cpus_allowed, cpu_online_mask, sizeof(struct cpumask));
		return 0;
	}

	cpu = kcalloc(cpus_num, sizeof(*cpu), GFP_KERNEL);
	if (!cpu) {
		pr_err("kcalloc cpu(%d) failed\n", cpus_num);
		return -ENOMEM;
	}

	rc = of_property_read_u32_array(node, MITEE_CORE_ID, cpu, cpus_num);
	if (rc) {
		pr_err("read dts failed(%d): %s\n", rc, MITEE_CORE_ID);
		return rc;
	}

	for (i = 0; i < cpus_num; i++) {
		if (cpu[i] >= nr_cpu_ids) {
			continue;
		}
		cpumask_set_cpu(cpu[i], cpus_allowed);
	}

	pr_info("bind core list: %*pbl\n", cpumask_pr_args(cpus_allowed));

	return rc;
}

static int mitee_parse_dts(struct optee *optee)
{
	struct device_node* dev_node = NULL;
	int rc = 0;

	if (!optee)
		return -EINVAL;

	dev_node = of_find_compatible_node(NULL, NULL, MITEE_COMPATIBLE);
	if (!dev_node) {
		pr_err("no such dev: %s\n", MITEE_COMPATIBLE);
		return -ENODEV;
	}

	rc = mitee_get_cpu_allows(dev_node, &optee->cpus_allowed);
	if (rc) {
		pr_err("get cpu allowed failed: %s\n", MITEE_COMPATIBLE);
		return rc;
	}

	return rc;
}

static int optee_ffa_probe(struct ffa_device *ffa_dev)
{
	const struct ffa_ops *ffa_ops = NULL;
	unsigned int rpc_arg_count = 0;
	struct tee_device *teedev = NULL;
	struct optee *optee = NULL;
	void *memremaped_shm = NULL;
	u32 sec_caps = 0;
	int rc = 0;
	sec_caps |= OPTEE_SMC_SEC_CAP_HAVE_RESERVED_SHM;
	sec_caps |= OPTEE_SMC_SEC_CAP_DYNAMIC_SHM;
	rpc_arg_count = THREAD_RPC_MAX_NUM_PARAMS;

	pr_info("initializing driver\n");

	rc = mitee_tee_init();
	if (rc)
		return -EINVAL;

	ffa_ops = ffa_dev->ops;

#if MITEE_FEATURE_CAP_ENABLE
	if (!optee_ffa_api_is_compatbile(ffa_dev, ffa_ops)) {
		pr_err("ffa abi unmatch\n");
		goto err_tee_exit;
	}

	if (!optee_ffa_exchange_caps(ffa_dev, ffa_ops, &rpc_arg_count)) {
		pr_err("ffa exchange caps fail\n");
		goto err_tee_exit;
	}
#endif

	optee = kzalloc(sizeof(*optee), GFP_KERNEL);
	if (!optee) {
		rc = -ENOMEM;
		goto err_tee_exit;
	}

	optee->pool = ERR_PTR(-EINVAL);

	if ((sec_caps & OPTEE_SMC_SEC_CAP_DYNAMIC_SHM) &&
	    !(sec_caps & OPTEE_SMC_SEC_CAP_HAVE_RESERVED_SHM)) {
		optee->pool = optee_ffa_config_dyn_shm();
	}

	if (IS_ERR(optee->pool) &&
	    (sec_caps & OPTEE_SMC_SEC_CAP_HAVE_RESERVED_SHM)) {
		optee->pool = optee_ffa_shm_memremap(ffa_dev, ffa_ops,
						     &memremaped_shm);
	}

	if (IS_ERR(optee->pool)) {
		rc = PTR_ERR(optee->pool);
		optee->pool = NULL;
		goto err_free_optee;
	}

	optee->ops = &optee_ffa_ops;
	optee->ffa.ffa_dev = ffa_dev;
	optee->rpc_arg_count = rpc_arg_count;

	teedev = tee_device_alloc(&optee_ffa_clnt_desc, NULL, optee->pool,
				  optee);
	if (IS_ERR(teedev)) {
		rc = PTR_ERR(teedev);
		goto err_free_pool;
	}
	optee->teedev = teedev;

	teedev = tee_device_alloc(&optee_ffa_supp_desc, NULL, optee->pool,
				  optee);
	if (IS_ERR(teedev)) {
		rc = PTR_ERR(teedev);
		goto err_unreg_teedev;
	}
	optee->supp_teedev = teedev;

	rc = tee_device_register(optee->teedev);
	if (rc)
		goto err_unreg_supp_teedev;

	rc = tee_device_register(optee->supp_teedev);
	if (rc)
		goto err_unreg_supp_teedev;

	rc = rhashtable_init(&optee->ffa.global_ids, &shm_rhash_params);
	if (rc)
		goto err_unreg_supp_teedev;
	mutex_init(&optee->ffa.mutex);
	mutex_init(&optee->call_queue.mutex);
	mutex_init(&tee_mutex);
	INIT_LIST_HEAD(&optee->call_queue.waiters);
	optee_wait_queue_init(&optee->wait_queue);
	optee_supp_init(&optee->supp);
	ffa_dev_set_drvdata(ffa_dev, optee);
	optee->ffa.memremaped_shm = memremaped_shm;

#if MITEE_FEATURE_FW_NP_ENABLE
	rc = optee_enumerate_devices(PTA_CMD_GET_DEVICES);
	if (rc) {
		goto err_rhashtable_free;
	}
#endif
	optee_svc = optee;

	/* mitee dts parse */
	rc = mitee_parse_dts(optee);
	if (rc) {
		pr_err("failed to parse mitee dts\n");
		goto err_unregister_devices;
	}

	/* mitee log device function START */
	ATOMIC_INIT_NOTIFIER_HEAD(&optee->notifier);
	optee->mitee_memlog_pdev = platform_device_alloc("mitee_memlog", 0);
	if (IS_ERR_OR_NULL(optee->mitee_memlog_pdev)) {
		rc = PTR_ERR(teedev);
		goto err_unregister_devices;
	}

	platform_device_add(optee->mitee_memlog_pdev);

	rc = mitee_memlog_probe(ffa_dev, ffa_ops, optee->mitee_memlog_pdev);
	if (rc) {
		pr_err("failed to initial mitee_memlog driver (%d)\n", rc);
		(void)mitee_memlog_remove(optee->mitee_memlog_pdev);
		platform_device_del(optee->mitee_memlog_pdev);
		goto err_unregister_devices;
	}
	/* mitee log device function END */

	optee_bm_enable();

	pr_info("initialized driver\n");
	return 0;

err_unregister_devices:
#if MITEE_FEATURE_FW_NP_ENABLE
	optee_unregister_devices();
err_rhashtable_free:
#endif
	rhashtable_free_and_destroy(&optee->ffa.global_ids, rh_free_fn, NULL);
	optee_supp_uninit(&optee->supp);
	mutex_destroy(&tee_mutex);
	mutex_destroy(&optee->call_queue.mutex);
	mutex_destroy(&optee->ffa.mutex);

err_unreg_supp_teedev:
	/*
	 * tee_device_unregister() is safe to call even if the
	 * devices hasn't been registered with
	 * tee_device_register() yet.
	 */
	tee_device_unregister(optee->supp_teedev);
err_unreg_teedev:
	tee_device_unregister(optee->teedev);
err_free_pool:
	if (optee->pool)
		tee_shm_pool_free(optee->pool);
	if (memremaped_shm)
		memunmap(memremaped_shm);
err_free_optee:
	kfree(optee);
err_tee_exit:
	mitee_tee_exit();
	return rc;
}

static const struct ffa_device_id mitee_ffa_device_id[] = {
	/* 06bb3aea-7b38-4dad-a70ff59986572611 */
	{ UUID_INIT(0x06bb3aea, 0x7b38, 0x4dad, 0xa7, 0x0f, 0xf5, 0x99, 0x86,
		    0x57, 0x26, 0x11) },
	{}
};

static struct ffa_driver optee_ffa_driver = {
	.name = "mitee",
	.probe = optee_ffa_probe,
	.remove = optee_ffa_remove,
	.id_table = mitee_ffa_device_id,
};

int optee_ffa_abi_register(void)
{
	if (IS_REACHABLE(CONFIG_ARM_FFA_TRANSPORT))
		return ffa_register(&optee_ffa_driver);
	else
		return -EOPNOTSUPP;
}

void optee_ffa_abi_unregister(void)
{
	if (IS_REACHABLE(CONFIG_ARM_FFA_TRANSPORT))
		ffa_unregister(&optee_ffa_driver);
}
