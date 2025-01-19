// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2013 Google, Inc.
 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#include <linux/platform_device.h>
#include <linux/trusty/smcall.h>
#include <linux/trusty/trusty.h>

#ifndef CONFIG_TRUSTY_FFA_TRANSPORT
#include <linux/trusty/arm_ffa.h>
#endif

#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>

#include "trusty-smc.h"
#include "trusty-private.h"
#include "trusty-trace.h"

static u32 trusty_smc_call(struct device *dev, unsigned long fid,
			   unsigned long a0, unsigned long a1,
			   unsigned long a2)
{
	u32 ret;

	trace_trusty_smc(fid, a0, a1, a2);

	ret = trusty_smc8(fid, a0, a1, a2, 0, 0, 0, 0).r0;

	trace_trusty_smc_done(ret);

	return ret;
}

#ifdef CONFIG_TRUSTY_FFA_TRANSPORT
static int trusty_smc_share_memory(struct device *dev, u64 *id,
				   struct scatterlist *sglist,
				   unsigned int nents, pgprot_t pgprot, u64 tag)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));
	int ret;
	struct ns_mem_page_info pg_inf;
	struct scatterlist *sg;
	size_t count;

	if (WARN_ON(nents < 1))
		return -EINVAL;

	if (nents != 1) {
		dev_err(s->dev, "%s: old trusty version does not support non-contiguous memory objects\n",
				__func__);
		return -EOPNOTSUPP;
	}

	count = dma_map_sg(dev, sglist, nents, DMA_BIDIRECTIONAL);
	if (count != nents) {
		dev_err(s->dev, "failed to dma map sg_table\n");
		return -EINVAL;
	}

	sg = sglist;
	ret = trusty_encode_page_info(&pg_inf, phys_to_page(sg_dma_address(sg)),
				      pgprot);
	if (ret) {
		dev_err(s->dev, "%s: trusty_encode_page_info failed\n",
			__func__);
		dma_unmap_sg(dev, sglist, nents, DMA_BIDIRECTIONAL);
		return ret;
	}

	*id = pg_inf.compat_attr;
	return 0;
}

static int trusty_smc_lend_memory(struct device *dev, u64 *id,
				  struct scatterlist *sglist,
				  unsigned int nents, pgprot_t pgprot, u64 tag)
{
	return -EOPNOTSUPP;
}

static int trusty_smc_reclaim_memory(struct device *dev, u64 id,
				     struct scatterlist *sglist,
				     unsigned int nents)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	if (WARN_ON(nents < 1))
		return -EINVAL;

	if (WARN_ON(s->api_version >= TRUSTY_API_VERSION_MEM_OBJ))
		return -EINVAL;

	if (nents != 1) {
		dev_err(s->dev, "%s: not supported\n", __func__);
		return -EOPNOTSUPP;
	}

	dma_unmap_sg(dev, sglist, nents, DMA_BIDIRECTIONAL);

	return 0;
}
#else
static int share_or_lend_memory(struct device *dev, u64 *id,
				struct scatterlist *sglist, unsigned int nents,
				pgprot_t pgprot, u64 tag, bool lend)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));
	int ret;
	struct ns_mem_page_info pg_inf;
	struct scatterlist *sg;
	size_t count;
	size_t i;
	size_t len = 0;
	u64 ffa_handle = 0;
	size_t total_len;
	size_t endpoint_count = 1;
	struct ffa_mtd *mtd = s->ffa_tx;
	size_t comp_mrd_offset = offsetof(struct ffa_mtd, emad[endpoint_count]);
	struct ffa_comp_mrd *comp_mrd = s->ffa_tx + comp_mrd_offset;
	struct ffa_cons_mrd *cons_mrd = comp_mrd->address_range_array;
	size_t cons_mrd_offset = (void *)cons_mrd - s->ffa_tx;
	struct smc_ret8 smc_ret;
	u32 cookie_low;
	u32 cookie_high;

	if (WARN_ON(nents < 1))
		return -EINVAL;

	if (nents != 1 && s->api_version < TRUSTY_API_VERSION_MEM_OBJ) {
		dev_err(s->dev, "%s: old trusty version does not support non-contiguous memory objects\n",
			__func__);
		return -EOPNOTSUPP;
	}

	if (lend && s->api_version < TRUSTY_API_VERSION_MEM_OBJ) {
		dev_err(s->dev, "%s: old trusty version does not support lending memory objects\n",
			__func__);
		return -EOPNOTSUPP;
	}

	count = dma_map_sg(dev, sglist, nents, DMA_BIDIRECTIONAL);
	if (count != nents) {
		dev_err(s->dev, "failed to dma map sg_table\n");
		return -EINVAL;
	}

	sg = sglist;
	ret = trusty_encode_page_info(&pg_inf, phys_to_page(sg_dma_address(sg)),
				      pgprot);
	if (ret) {
		dev_err(s->dev, "%s: trusty_encode_page_info failed\n",
			__func__);
		goto err_encode_page_info;
	}

	if (s->api_version < TRUSTY_API_VERSION_MEM_OBJ) {
		*id = pg_inf.compat_attr;
		return 0;
	}

	len = 0;
	for_each_sg(sglist, sg, nents, i)
		len += sg_dma_len(sg);

	trace_trusty_share_memory(len, nents, lend);

	mutex_lock(&s->share_memory_msg_lock);

	mtd->sender_id = s->ffa_local_id;
	mtd->memory_region_attributes = pg_inf.ffa_mem_attr;
	mtd->reserved_3 = 0;
	mtd->flags = 0;
	mtd->handle = 0;
	mtd->tag = tag;
	mtd->reserved_24_27 = 0;
	mtd->emad_count = endpoint_count;
	for (i = 0; i < endpoint_count; i++) {
		struct ffa_emad *emad = &mtd->emad[i];
		/* TODO: support stream ids */
		emad->mapd.endpoint_id = s->ffa_remote_id;
		emad->mapd.memory_access_permissions = pg_inf.ffa_mem_perm;
		emad->mapd.flags = 0;
		emad->comp_mrd_offset = comp_mrd_offset;
		emad->reserved_8_15 = 0;
	}
	comp_mrd->total_page_count = len / FFA_PAGE_SIZE;
	comp_mrd->address_range_count = nents;
	comp_mrd->reserved_8_15 = 0;

	total_len = cons_mrd_offset + nents * sizeof(*cons_mrd);
	sg = sglist;
	while (count) {
		size_t lcount =
			min_t(size_t, count, (PAGE_SIZE - cons_mrd_offset) /
			      sizeof(*cons_mrd));
		size_t fragment_len = lcount * sizeof(*cons_mrd) +
				      cons_mrd_offset;

		for (i = 0; i < lcount; i++) {
			cons_mrd[i].address = sg_dma_address(sg);
			cons_mrd[i].page_count = sg_dma_len(sg) / FFA_PAGE_SIZE;
			cons_mrd[i].reserved_12_15 = 0;
			sg = sg_next(sg);
		}
		count -= lcount;
		if (cons_mrd_offset) {
			u32 smc = lend ? SMC_FC_FFA_MEM_LEND :
					 SMC_FC_FFA_MEM_SHARE;
			/* First fragment */
			smc_ret = trusty_smc8(smc, total_len,
					      fragment_len, 0, 0, 0, 0, 0);
		} else {
			smc_ret = trusty_smc8(SMC_FC_FFA_MEM_FRAG_TX,
					      cookie_low, cookie_high,
					      fragment_len, 0, 0, 0, 0);
		}
		if (smc_ret.r0 == SMC_FC_FFA_MEM_FRAG_RX) {
			cookie_low = smc_ret.r1;
			cookie_high = smc_ret.r2;
			dev_dbg(s->dev, "cookie %x %x", cookie_low,
				cookie_high);
			if (!count) {
				/*
				 * We have sent all our descriptors. Expected
				 * SMC_FC_FFA_SUCCESS, not a request to send
				 * another fragment.
				 */
				dev_err(s->dev, "%s: fragment_len %zd/%zd, unexpected SMC_FC_FFA_MEM_FRAG_RX\n",
					__func__, fragment_len, total_len);
				ret = -EIO;
				break;
			}
		} else if (smc_ret.r0 == SMC_FC_FFA_SUCCESS) {
			ffa_handle = smc_ret.r2 | (u64)smc_ret.r3 << 32;
			dev_dbg(s->dev, "%s: fragment_len %zu/%zu, got handle 0x%llx\n",
				__func__, fragment_len, total_len,
				ffa_handle);
			if (count) {
				/*
				 * We have not sent all our descriptors.
				 * Expected SMC_FC_FFA_MEM_FRAG_RX not
				 * SMC_FC_FFA_SUCCESS.
				 */
				dev_err(s->dev, "%s: fragment_len %zu/%zu, unexpected SMC_FC_FFA_SUCCESS, count %zu != 0\n",
					__func__, fragment_len, total_len,
					count);
				ret = -EIO;
				break;
			}
		} else {
			dev_err(s->dev, "%s: fragment_len %zu/%zu, SMC_FC_FFA_MEM_SHARE failed 0x%lx 0x%lx 0x%lx",
				__func__, fragment_len, total_len,
				smc_ret.r0, smc_ret.r1, smc_ret.r2);
			ret = -EIO;
			break;
		}

		cons_mrd = s->ffa_tx;
		cons_mrd_offset = 0;
	}

	mutex_unlock(&s->share_memory_msg_lock);

	if (!ret) {
		*id = ffa_handle;
		goto done;
	}

	dev_err(s->dev, "%s: failed %d", __func__, ret);

err_encode_page_info:
	dma_unmap_sg(dev, sglist, nents, DMA_BIDIRECTIONAL);
done:
	trace_trusty_share_memory_done(len, nents, lend, ffa_handle, ret);
	return ret;
}

static int trusty_smc_share_memory(struct device *dev, u64 *id,
				   struct scatterlist *sglist,
				   unsigned int nents, pgprot_t pgprot, u64 tag)
{
	return share_or_lend_memory(dev, id, sglist, nents, pgprot, tag, false);
}

static int trusty_smc_lend_memory(struct device *dev, u64 *id,
				  struct scatterlist *sglist,
				  unsigned int nents, pgprot_t pgprot, u64 tag)
{
	return share_or_lend_memory(dev, id, sglist, nents, pgprot, tag, true);
}

static int trusty_smc_reclaim_memory(struct device *dev, u64 id,
				     struct scatterlist *sglist,
				     unsigned int nents)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));
	int ret = 0;
	struct smc_ret8 smc_ret;

	if (WARN_ON(nents < 1))
		return -EINVAL;

	if (s->api_version < TRUSTY_API_VERSION_MEM_OBJ) {
		if (nents != 1) {
			dev_err(s->dev, "%s: not supported\n", __func__);
			return -EOPNOTSUPP;
		}

		dma_unmap_sg(dev, sglist, nents, DMA_BIDIRECTIONAL);

		return 0;
	}

	mutex_lock(&s->share_memory_msg_lock);

	smc_ret = trusty_smc8(SMC_FC_FFA_MEM_RECLAIM, (u32)id, id >> 32, 0, 0,
			      0, 0, 0);
	if (smc_ret.r0 != SMC_FC_FFA_SUCCESS) {
		dev_err(s->dev, "%s: SMC_FC_FFA_MEM_RECLAIM failed 0x%lx 0x%lx 0x%lx",
			__func__, smc_ret.r0, smc_ret.r1, smc_ret.r2);
		if (smc_ret.r0 == SMC_FC_FFA_ERROR &&
		    smc_ret.r2 == FFA_ERROR_DENIED)
			ret = -EBUSY;
		else
			ret = -EIO;
	}

	mutex_unlock(&s->share_memory_msg_lock);

	if (ret != 0)
		goto err_ffa_mem_reclaim;

	dma_unmap_sg(dev, sglist, nents, DMA_BIDIRECTIONAL);

err_ffa_mem_reclaim:
	return ret;
}
#endif

static const struct trusty_call_ops trusty_smc_call_ops = {
	.trusty_std_call = &trusty_smc_call,
	.trusty_fast_call = &trusty_smc_call,
};

static const struct trusty_mem_ops trusty_smc_mem_ops = {
	.trusty_share_memory = &trusty_smc_share_memory,
	.trusty_lend_memory = &trusty_smc_lend_memory,
	.trusty_reclaim_memory = &trusty_smc_reclaim_memory,
};

#ifdef CONFIG_TRUSTY_FFA_TRANSPORT
static int trusty_init_msg_buf(struct trusty_state *s, struct device *dev)
{
	return 0;
}

static void trusty_free_msg_buf(struct trusty_state *s, struct device *dev)
{
}
#else
static int trusty_init_msg_buf(struct trusty_state *s, struct device *dev)
{
	phys_addr_t tx_paddr;
	phys_addr_t rx_paddr;
	int ret;
	struct smc_ret8 smc_ret;

	if (s->api_version < TRUSTY_API_VERSION_MEM_OBJ)
		return 0;

	/* Get supported FF-A version and check if it is compatible */
	smc_ret = trusty_smc8(SMC_FC_FFA_VERSION, FFA_CURRENT_VERSION, 0, 0,
			      0, 0, 0, 0);
	if (FFA_VERSION_TO_MAJOR(smc_ret.r0) != FFA_CURRENT_VERSION_MAJOR) {
		dev_err(s->dev,
			"%s: Unsupported FF-A version 0x%lx, expected 0x%x\n",
			__func__, smc_ret.r0, FFA_CURRENT_VERSION);
		ret = -EIO;
		goto err_version;
	}

	/* Check that SMC_FC_FFA_MEM_SHARE is implemented */
	smc_ret = trusty_smc8(SMC_FC_FFA_FEATURES, SMC_FC_FFA_MEM_SHARE, 0, 0,
			      0, 0, 0, 0);
	if (smc_ret.r0 != SMC_FC_FFA_SUCCESS) {
		dev_err(s->dev,
			"%s: SMC_FC_FFA_FEATURES(SMC_FC_FFA_MEM_SHARE) failed 0x%lx 0x%lx 0x%lx\n",
			__func__, smc_ret.r0, smc_ret.r1, smc_ret.r2);
		ret = -EIO;
		goto err_features;
	}

	/*
	 * Set FF-A endpoint IDs.
	 *
	 * Hardcode 0x8000 for the secure os.
	 * TODO: Use FF-A call or device tree to configure this dynamically
	 */
	smc_ret = trusty_smc8(SMC_FC_FFA_ID_GET, 0, 0, 0, 0, 0, 0, 0);
	if (smc_ret.r0 != SMC_FC_FFA_SUCCESS) {
		dev_err(s->dev,
			"%s: SMC_FC_FFA_ID_GET failed 0x%lx 0x%lx 0x%lx\n",
			__func__, smc_ret.r0, smc_ret.r1, smc_ret.r2);
		ret = -EIO;
		goto err_id_get;
	}

	s->ffa_local_id = smc_ret.r2;
	s->ffa_remote_id = 0x8000;

	/*
	 * The pKVM hypervisor uses the same page size as the host, including for
	 * stage-2 mappings. So the rx/tx buffers need to be page-sized multiple,
	 * and page-aligned.
	 *
	 * TODO: This can be made more generic by discovering the required size
	 * through SMC_FC_FFA_FEATURES later.
	 */
	s->ffa_tx = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!s->ffa_tx) {
		ret = -ENOMEM;
		goto err_alloc_tx;
	}
	tx_paddr = virt_to_phys(s->ffa_tx);
	if (WARN_ON(tx_paddr & (PAGE_SIZE - 1))) {
		ret = -EINVAL;
		goto err_unaligned_tx_buf;
	}

	s->ffa_rx = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!s->ffa_rx) {
		ret = -ENOMEM;
		goto err_alloc_rx;
	}
	rx_paddr = virt_to_phys(s->ffa_rx);
	if (WARN_ON(rx_paddr & (PAGE_SIZE - 1))) {
		ret = -EINVAL;
		goto err_unaligned_rx_buf;
	}

	smc_ret = trusty_smc8(SMC_FCZ_FFA_RXTX_MAP, tx_paddr, rx_paddr,
			      PAGE_SIZE / FFA_PAGE_SIZE, 0, 0, 0, 0);
	if (smc_ret.r0 != SMC_FC_FFA_SUCCESS) {
		dev_err(s->dev, "%s: SMC_FCZ_FFA_RXTX_MAP failed 0x%lx 0x%lx 0x%lx\n",
			__func__, smc_ret.r0, smc_ret.r1, smc_ret.r2);
		ret = -EIO;
		goto err_rxtx_map;
	}

	return 0;

err_rxtx_map:
err_unaligned_rx_buf:
	kfree(s->ffa_rx);
	s->ffa_rx = NULL;
err_alloc_rx:
err_unaligned_tx_buf:
	kfree(s->ffa_tx);
	s->ffa_tx = NULL;
err_alloc_tx:
err_id_get:
err_features:
err_version:
	return ret;
}

static void trusty_free_msg_buf(struct trusty_state *s, struct device *dev)
{
	struct smc_ret8 smc_ret;

	smc_ret = trusty_smc8(SMC_FC_FFA_RXTX_UNMAP, 0, 0, 0, 0, 0, 0, 0);
	if (smc_ret.r0 != SMC_FC_FFA_SUCCESS) {
		dev_err(s->dev, "%s: SMC_FC_FFA_RXTX_UNMAP failed 0x%lx 0x%lx 0x%lx\n",
			__func__, smc_ret.r0, smc_ret.r1, smc_ret.r2);
	} else {
		kfree(s->ffa_rx);
		kfree(s->ffa_tx);
	}
}
#endif

int trusty_smc_transport_setup(struct device *dev)
{
	int rc;
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	rc = trusty_init_api_version(s, dev, &trusty_smc_call);
	if (rc != 0)
		return rc;

	rc = trusty_init_msg_buf(s, dev);
	if (rc < 0)
		return rc;

	/*
	 * Initialize Trusty msg calls with Trusty SMC ABI
	 */
	s->call_ops = &trusty_smc_call_ops;

	/*
	 * Initialize Trusty memory operations with Trusty SMC ABI only when
	 * Trusty API version is below TRUSTY_API_VERSION_MEM_OBJ, or we
	 * disabled the FF-A transport at build time.
	 */
	if (!IS_ENABLED(CONFIG_TRUSTY_FFA_TRANSPORT) ||
	    s->api_version < TRUSTY_API_VERSION_MEM_OBJ)
		s->mem_ops = &trusty_smc_mem_ops;

	return 0;
}

void trusty_smc_transport_cleanup(struct device *dev)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	if (s->call_ops == &trusty_smc_call_ops)
		s->call_ops = NULL;

	if (s->mem_ops == &trusty_smc_mem_ops)
		s->mem_ops = NULL;

	trusty_free_msg_buf(s, dev);
}

const struct trusty_transport_desc trusty_smc_transport = {
	.name = "smc",
	.setup = trusty_smc_transport_setup,
	.cleanup = trusty_smc_transport_cleanup,
};
