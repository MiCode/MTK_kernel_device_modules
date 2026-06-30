// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/compat.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/math.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/ioport.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/dma-mapping.h>

/* gpu header */
#include <gpu_pdma_mt6991.h>
#include <mtk_gpufreq.h>
#include <ghpm_wrapper.h>
#include <ged_gpu_slc.h>

/* define */
#define PDMA_DEVNAME "gpu_pdma_driver"
#define PDMA_MISC_DEVNAME "gpu_pdma"
#define RING_BUFFER_PAGE_SIZE(x)		((1 << (x)) << PAGE_SHIFT)

/* Define */
#define CCMD_STATUS_CH0						0x20
#define CCMD_RING_BUFFER_CONTROL	0x84
#define CCMD_CID_COMMAND			0x9C
#define CIDX_RING_BUFFER_HRPTR(n)	(0xC0 + n * 0x8)
#define CIDX_RING_BUFFER_HWPTR(n)	(0xC4 + n * 0x8)
#define CIDX_RING_BUFFER_PA_0_L(n)	(0x100 + n * 0x20)
#define CIDX_RING_BUFFER_PA_0_H(n)	(0x104 + n * 0x20)
#define CCMD_RINGBUF_PA_REG_WIDTH	0x8 /* 64-bit */
#define CCMD_RING_BUFFER_PA_VALID	0x1
#define CCMD_INIT_RINGBUG_PA			1
#define CCMD_CONFIG_MODE_AP				1
#define CCMD_CONFIG_MODE_GPUEB		0
#define CCMD_PAGE_SIZE_4K					0x1000
#define POLICY_TEX_CACHE_LSC_ALLOC	0x4
#define CCMD_POWER_ON							1
#define CCMD_POWER_OFF						0

#define CCMD_UNSUPPORTED_CID		0xFFFFFFFF
#define CCMD_RESERVED_PBHA_NUM		5
#define CCMD_UNSUPPORTED_PBHA_ID	0
#define CCMD_V2_SUPPORT			0

/* variables */
static struct pdma_device *g_pdma_dev;
static u32 g_reserved_pbha_id[CCMD_RESERVED_PBHA_NUM] = {
	0, 6, 7, 9, 15};

static unsigned int g_pdma_open_cnt;

static DEFINE_SPINLOCK(g_pdma_file_lock);

static void __pdma_release_extended_pbha(u32 kctx_id, u32 pbha_id);

struct tag_chipid {
	uint32_t size;
	uint32_t hw_code;
	uint32_t hw_subcode;
	uint32_t hw_ver;
	uint32_t sw_ver;
};

static int pdma_get_chipid(struct pdma_device *pdma_dev)
{
	struct tag_chipid *chip_id;
	struct device_node *node = of_find_node_by_path("/chosen");

	if (!pdma_dev)
		return -EFAULT;

	node = of_find_node_by_path("/chosen");
	if (!node)
		node = of_find_node_by_path("/chosen@0");

	if (!node) {
		pr_notice("chosen node not found in device tree\n");
		return -ENODEV;
	}

	chip_id = (struct tag_chipid *)of_get_property(node, "atag,chipid", NULL);
	if (!chip_id) {
		pr_notice("could not found atag,chipid in chosen\n");
		return -ENODEV;
	}

	pdma_dev->sw_version = (u8)chip_id->sw_ver;

	pr_info("[CCMD] PDMA sw_ver: 0x%x", pdma_dev->sw_version);

	return 0;
}

static struct pdma_device *get_PDMA_Device(void)
{
	return g_pdma_dev;
}

static int ccmd_power_control(int power)
{
	int ret = 0;

	if (power == CCMD_POWER_ON) {
		/* On mfg0 and gpueb */
		ret = gpueb_ctrl(GHPM_ON, MFG1_OFF, SUSPEND_POWER_ON);
		if (ret) {
			pr_info("[CCMD] gpueb on fail, return value=%d\n", ret);
			return ret;
		}
		/* on,off/ SWCG(BG3D)/ MTCMOS/ BUCK */
		if (gpufreq_power_control(GPU_PWR_ON) < 0) {
			pr_info("[CCMD] Power On Failed\n");
			return 1;
		}

		/* Control runtime active-sleep state of GPU */
		if (gpufreq_active_sleep_control(GPU_PWR_ON) < 0) {
			pr_info("[CCMD] Active Failed (on)\n");
			return 1;
		}
	} else if (power == CCMD_POWER_OFF) {
		/* Control runtime active-sleep state of GPU */
		if (gpufreq_active_sleep_control(GPU_PWR_OFF) < 0) {
			pr_info("[CCMD] Sleep Failed (off)\n");
			return 1;
		}

		/* on,off/ SWCG(BG3D)/ MTCMOS/ BUCK */
		if (gpufreq_power_control(GPU_PWR_OFF) < 0){
			pr_info( "[CCMD] Power Off Failed\n");
			return 1;
		}

		/* Off mfg0 and gpueb */
		ret = gpueb_ctrl(GHPM_OFF, MFG1_OFF, SUSPEND_POWER_OFF);
		if (ret) {
			pr_info("[CCMD] gpueb off fail, return value=%d\n", ret);
			return ret;
		}
	} else {
		pr_info("%s Unexpected power state %d\n", __func__, power);
		ret = 2;
	}
	return ret;
}

static void init_pbha_pool(struct device_node *node, struct pdma_device *pdma_dev)
{
	struct extended_pbha *pbha_entry;
	struct list_head *entry, *tmp;
	u32 index, resv_idx;
	u32 extended_pbha_cnt;


	INIT_LIST_HEAD(&pdma_dev->extened_pbha_pool);

	if (of_property_read_u8(node, "extended-pbha-bits",
			&pdma_dev->extended_pbha_bits) < 0) {
		/* Support single context only if not specified */
		pdma_dev->extended_pbha_bits = 0;
		pr_info("[CCMD] Not support extended PBHA\n");
	}

	if (pdma_dev->extended_pbha_bits == 0) {
		pr_info("[CCMD] %s NOT support extended PBHA\n", __func__);
		return;
	}

	extended_pbha_cnt = (1 << pdma_dev->extended_pbha_bits);
	for (index = 0 ; index < extended_pbha_cnt ; index++) {
		pbha_entry = kzalloc(sizeof(struct extended_pbha), GFP_KERNEL);
		pbha_entry->id = index;
		/* in descending order */
		list_add(&pbha_entry->entry, &pdma_dev->extened_pbha_pool);
	}
	pr_info("Init PBHA list with %u PBHA entries", extended_pbha_cnt);

	for (resv_idx = 0 ; resv_idx < CCMD_RESERVED_PBHA_NUM; resv_idx++) {
		list_for_each_safe(entry, tmp, &pdma_dev->extened_pbha_pool) {
			pbha_entry = list_entry(entry, struct extended_pbha, entry);
			if (pbha_entry->id == g_reserved_pbha_id[resv_idx]) {
				list_del(&pbha_entry->entry);
				pr_info("remove %u from extened_pbha_pool\n", g_reserved_pbha_id[resv_idx]);
				break;
			}
		}
	}
}

static bool IsValidCcmdCtx(struct ccmd_context *ccmd_ctx)
{
	struct pdma_device *pdma_dev;
	struct list_head *entry, *tmp;
	struct ccmd_context *ctx;

	if (!ccmd_ctx)
		return false;
	if (ccmd_ctx->cid == CCMD_UNSUPPORTED_CID) {
		pr_info("[CCMD] %s : Unsupported CID (%p)\n", __func__, ccmd_ctx);
		return false;
	}

	pdma_dev = ccmd_ctx->pdma_dev;

	if (!pdma_dev)
		return false;

	lockdep_assert_held(&pdma_dev->pdma_device_lock);

	/* vaidate ccmd_ctx */
	list_for_each_safe(entry, tmp, &pdma_dev->ctx_list) {
		ctx = list_entry(entry, struct ccmd_context, entry);
		if (ccmd_ctx == ctx)
			return true;
	}

	pr_info("[CCMD] Context %p is not found in context list\n",
		(void *)ccmd_ctx);
	return false;
}

/* Must be called with power on */
static void __ccmd_reset_hw(struct pdma_device *pdma_dev,
	struct ccmd_context *ccmd_ctx)
{
//	int *ringbuf_pa0_setting_l;
#if CCMD_V2_SUPPORT
	unsigned int pdma_status;
	unsigned int poll_timeout = 1000;
#endif
	if (!pdma_dev || !ccmd_ctx) {
		pr_info("[CCMD] %s, Invalid arguments.\n", __func__);
		return;
	}

	lockdep_assert_held(&pdma_dev->pdma_device_lock);

//	ringbuf_pa0_setting_l = pdma_dev->pdma_reg_base_kva +
//		CIDX_RING_BUFFER_PA_0_L(ccmd_ctx->cid);

#if CCMD_V2_SUPPORT
	/*ch0 is for initializing custom command buffer process.*/
	/* Disable and poll Ch0*/
	writel((readl(pdma_dev->pdma_reg_base_kva) & 0xFFFDFFFF),
		pdma_dev->pdma_reg_base_kva);

	pr_info("[CCMD] %s Polling AutoDMA ch0", __func__);
	do {
		udelay(10);
		pdma_status =
			readl((pdma_dev->pdma_reg_base_kva + CCMD_STATUS_CH0));
		poll_timeout--;
	}while(pdma_status == 0 && poll_timeout > 0);
	/* status 0: ch0 is running */

	/* reset HW */
	/* CCB */
	writel(0x3, (pdma_dev->pdma_reg_base_kva + CCMD_RING_BUFFER_CONTROL));

	/* CID_COMMAND */
	writel((ccmd_ctx->cid << 8) | (0x3 << 10),
		pdma_dev->pdma_reg_base_kva + CCMD_CID_COMMAND);
	/* release GID command of the CID, and reset hrptr&hwptr */

	/* Enable ch0*/
	writel((readl(pdma_dev->pdma_reg_base_kva) | (0x1 << 17)),
		pdma_dev->pdma_reg_base_kva);
#endif
}

/* Must be called with hw lock held */
static int init_ccmd_hw(struct pdma_device *pdma_dev,
	struct ccmd_context *ccmd_ctx)
{
	unsigned long ringbuf_pa;
	int *ringbuf_pa_setting_l, *ringbuf_pa_setting_h;
	int page_num;
	int ret = 0;

	if (!pdma_dev)
		return -ENODEV;

	if (!ccmd_ctx)
		return -EBADF;

	if (pdma_dev->config_mode == CCMD_CONFIG_MODE_GPUEB)
		return ret;

	lockdep_assert_held(&pdma_dev->pdma_device_lock);

	ringbuf_pa = ccmd_ctx->ringbuf_paddr;
	// TODO: change to pr_debug
	pr_info("PDMA Ring buffer addr PA0 0x%lx\n", ringbuf_pa);

	if (ccmd_power_control(CCMD_POWER_ON))
		return 1;

#if CCMD_V2_SUPPORT
	/* CCMD mode */
	writel(0x423B8000, pdma_dev->pdma_reg_base_kva);

	/* Zombie_IRQ_CONTROL */
	writel(0x90000200, pdma_dev->pdma_reg_base_kva + 0x6C);

	/* SLC Policy Attr */
	writel(0x400, (pdma_dev->pdma_reg_base_kva + 0x300));
	writel(0x402, (pdma_dev->pdma_reg_base_kva + 0x304));
	writel(0x406, (pdma_dev->pdma_reg_base_kva + 0x308));
	writel(0x412, (pdma_dev->pdma_reg_base_kva + 0x30C));
	writel(0x800, (pdma_dev->pdma_reg_base_kva + 0x310));
	writel(0x840, (pdma_dev->pdma_reg_base_kva + 0x314));
	writel(0x8c0, (pdma_dev->pdma_reg_base_kva + 0x318));
	writel(0x000, (pdma_dev->pdma_reg_base_kva + 0x31C));
	writel(0xCC2, (pdma_dev->pdma_reg_base_kva + 0x320));
#endif

	for (page_num = 0; page_num < (1 << pdma_dev->page_order); page_num++) {
		ringbuf_pa_setting_l =
			pdma_dev->pdma_reg_base_kva +
			CIDX_RING_BUFFER_PA_0_L(ccmd_ctx->cid) +
			(CCMD_RINGBUF_PA_REG_WIDTH * page_num);
		ringbuf_pa_setting_h =
			pdma_dev->pdma_reg_base_kva +
			CIDX_RING_BUFFER_PA_0_H(ccmd_ctx->cid) +
			(CCMD_RINGBUF_PA_REG_WIDTH * page_num);

		if (page_num == 0)
			writel((ringbuf_pa | CCMD_RING_BUFFER_PA_VALID) & 0xFFFFFFFF,
				ringbuf_pa_setting_l);
		else
			writel(ringbuf_pa & 0xFFFFFFFF, ringbuf_pa_setting_l);
		writel((ringbuf_pa >> 32) & 0xFFFFFFFF, ringbuf_pa_setting_h);

		pdma_dev->pdma_sram_base_kva->ringbuf[page_num] = (ringbuf_pa >> 12);

		pr_debug("Config CCMD_RING_BUFFER_PA_%d: 0x%08x%08x\n", page_num,
			readl(ringbuf_pa_setting_h), readl(ringbuf_pa_setting_l));
			/* ring_buffer_pa config per 4k range */
			ringbuf_pa += CCMD_PAGE_SIZE_4K;
	}
	/* reset HW */
	//writel(0x3, (g_pdma_reg_base_kva + CCMD_RING_BUFFER_CONTROL));
	__ccmd_reset_hw(pdma_dev, ccmd_ctx);

#ifndef CCMD_DEBUG_MODE
	if (ccmd_power_control(CCMD_POWER_OFF))
		return 2;
#endif

	return ret;
}

/* Must be called with hw lock held */
static int reset_ccmd_hw(struct pdma_device *pdma_dev,
	struct ccmd_context *ccmd_ctx)
{

	int ret = 0;

	if (pdma_dev->config_mode == CCMD_CONFIG_MODE_GPUEB)
		return ret;
#ifndef CCMD_DEBUG_MODE
	if (ccmd_power_control(CCMD_POWER_ON))
		return 1;
#endif

	__ccmd_reset_hw(pdma_dev ,ccmd_ctx);

	if (ccmd_power_control(CCMD_POWER_OFF))
		return 2;
	return ret;
}

static int ccmd_context_init(
	struct ccmd_context *ccmd_ctx, u32 kctx_id, u32 cid, u32 mode)
{
	struct pdma_device *pdma_dev;
	gfp_t gfp = (GFP_HIGHUSER | __GFP_ZERO);
	dma_addr_t dma_addr;

	if (!ccmd_ctx || cid == CCMD_UNSUPPORTED_CID || mode >= UNSUPPORTED_MODE) {
		pr_info("NULL pointer or invalid CID (%u)(%u)", cid, mode);
		return -EINVAL;
	}

	pdma_dev = ccmd_ctx->pdma_dev;
	if (!pdma_dev) {
		pr_info("Invalid pointer to pdma device\n");
		return -EINVAL;
	}

	/* ring buffer is allocated when context lock hw */
	ccmd_ctx->ringbuf_vaddr = __get_free_pages(gfp, pdma_dev->page_order);

	if (!ccmd_ctx->ringbuf_vaddr) {
		pr_info("%s __get_free_pages failed\n", __func__);
		return -ENOMEM;
	}

	/* Cache Sync for zero out ringbuf with __GFP_ZERO */
	dma_addr = dma_map_single(pdma_dev->dev, (void *)ccmd_ctx->ringbuf_vaddr,
		(PAGE_SIZE << pdma_dev->page_order), DMA_BIDIRECTIONAL);
	if (dma_mapping_error(pdma_dev->dev, dma_addr)) {
		pr_info("%s fail to performance cache sync via dma\n", __func__);
		free_pages(ccmd_ctx->ringbuf_vaddr, pdma_dev->page_order);
		ccmd_ctx->ringbuf_vaddr = 0;
		return -ENOMEM;
	}
	dma_sync_single_for_device(pdma_dev->dev, dma_addr,
		(PAGE_SIZE << pdma_dev->page_order), DMA_TO_DEVICE);
	dma_unmap_page(pdma_dev->dev, dma_addr,
		(PAGE_SIZE << pdma_dev->page_order), DMA_BIDIRECTIONAL);

	ccmd_ctx->ringbuf_paddr = virt_to_phys((void *)ccmd_ctx->ringbuf_vaddr);

	pr_info("[CCMD] %s cid %u is created. kctx %u, ringbuf_paddr: 0x%llx\n",
		__func__, cid, kctx_id, ccmd_ctx->ringbuf_paddr);

	ccmd_ctx->kctx_id = kctx_id;
	ccmd_ctx->cid = cid;
	ccmd_ctx->mode = mode;
#ifdef CCMD_DEBUG_MODE
	ccmd_ctx->cid_reg_base = pdma_dev->reg_base;
#else
	ccmd_ctx->cid_reg_base = pdma_dev->reg_base + 0x10000 + (0x4000ULL * cid);
#endif
	/* user gets available cid, so init ccmd_context */
	list_add(&ccmd_ctx->entry, &pdma_dev->ctx_list);

	return 0;
}

static void ccmd_context_destroy(struct ccmd_context *ccmd_ctx)
{
	struct pdma_device *pdma_dev;

	if (IsValidCcmdCtx(ccmd_ctx)) {
		pdma_dev = ccmd_ctx->pdma_dev;
		lockdep_assert_held(&pdma_dev->pdma_device_lock);

		free_pages(ccmd_ctx->ringbuf_vaddr, pdma_dev->page_order);

		ccmd_ctx->cid = CCMD_UNSUPPORTED_CID;
		ccmd_ctx->ringbuf_vaddr = 0;
		ccmd_ctx->ringbuf_paddr = 0;
		list_del(&ccmd_ctx->entry);
	}
}


static u32 getCCMDCid(struct pdma_device *pdma_dev, unsigned int mode)
{
	u32 index;
	u32 cid = CCMD_UNSUPPORTED_CID;

	if (!pdma_dev) {
		pr_info("Invalid pointer to pdma device\n");
		return cid;
	}

	lockdep_assert_held(&pdma_dev->pdma_device_lock);

	if (mode == COMPUTE_TLS &&
		pdma_dev->non_api_ctx_cnt == pdma_dev->max_non_api_ctx_cnt) {
		pr_info("Allow only %u non API contexts concurrently\n",
			pdma_dev->max_non_api_ctx_cnt);
		return cid;
	}

	for (index = 0; index < pdma_dev->max_ctx_cnt; index++) {
		if (((pdma_dev->ccmd_locked_ctx_id >> index) & 0x1) == 0) {
			pdma_dev->ccmd_locked_ctx_id |= (0x1 << index);
			cid = index;
			if (mode == COMPUTE_TLS)
				pdma_dev->non_api_ctx_cnt++;
			break;
		}
	}

	pr_info("[CCMD] %s, get CID %u. mode %u (0x%x)", __func__,
		cid, mode, pdma_dev->ccmd_locked_ctx_id);

	return cid;
}

static void releaseCCMDCid(struct pdma_device *pdma_dev, u32 cid, u32 mode)
{
	u32 cid_mask = 1 << cid;

	if (!pdma_dev) {
		pr_info("Invalid pointer to pdma device\n");
		return;
	}

	lockdep_assert_held(&pdma_dev->pdma_device_lock);

	if (!(pdma_dev->ccmd_locked_ctx_id & cid_mask)) {
		pr_info("[CCMD] Try to release unused cid\n");
		return;
	}

	pdma_dev->ccmd_locked_ctx_id &= ~cid_mask;
	if (mode == COMPUTE_TLS)
		pdma_dev->non_api_ctx_cnt--;
}

static const struct of_device_id pdma_of_match[] = {
	{ .compatible = "mediatek,gpupdma", },
	{/* sentinel */}
};

void gpu_pdma_vma_open(struct vm_area_struct *vma)
{
	pr_debug("gpu_pdma VMA open, virt %lx, phys %lx\n", vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

void gpu_pdma_vma_close(struct vm_area_struct *vma)
{
	pr_debug("gpu_pdma VMA close, virt %lx, phys %lx\n", vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

const struct vm_operations_struct gpu_pdma_vm_ops = {
	.open = gpu_pdma_vma_open,
	.close = gpu_pdma_vma_close,
};

static int gpu_pdma_open(struct inode *inode, struct file *filp)
{
	unsigned long flags;
	struct ccmd_context *ccmd_ctx =
		kzalloc(sizeof(struct ccmd_context), GFP_KERNEL);

	if (!g_pdma_dev)
		return -ENODEV;

	if (!ccmd_ctx) {
		pr_info("Allocate CCMD Context handle fail\n");
		return -ENOMEM;
	}

	ccmd_ctx->pdma_dev = g_pdma_dev;
	ccmd_ctx->cid = CCMD_UNSUPPORTED_CID;
	INIT_LIST_HEAD(&ccmd_ctx->entry);
	INIT_LIST_HEAD(&ccmd_ctx->pbha_list);

	filp->private_data = ccmd_ctx;
	spin_lock_irqsave(&g_pdma_file_lock, flags);
	g_pdma_open_cnt++;
	spin_unlock_irqrestore(&g_pdma_file_lock, flags);
	// TODO: debug remove
	pr_info("[CCMD] %s ccmd_ctx: %p %u\n", __func__, ccmd_ctx, g_pdma_open_cnt);
//	pr_debug("%s open count %u\n", __func__, g_pdma_open_cnt);
	return 0;
}

static int gpu_pdma_release(struct inode *inode, struct file *filp)
{
	unsigned long flags;
	struct ccmd_context *ccmd_ctx = filp->private_data;
	struct pdma_device *pdma_dev = ccmd_ctx->pdma_dev;
	struct list_head *pbha_entry, *pbha_tmp;
	struct extended_pbha *ext_pbha;

	mutex_lock(&pdma_dev->pdma_device_lock);
	if (IsValidCcmdCtx(ccmd_ctx)) {
		if (unlikely(!list_empty(&ccmd_ctx->pbha_list))) {
			pr_info("[CCMD] %s unreturned PBHA ID found in kctx/cid %u/%u",
				__func__, ccmd_ctx->kctx_id, ccmd_ctx->cid);
			list_for_each_safe(pbha_entry, pbha_tmp, &ccmd_ctx->pbha_list) {
				ext_pbha = list_entry(pbha_entry, struct extended_pbha, entry);
				__pdma_release_extended_pbha(ccmd_ctx->kctx_id, ext_pbha->id);
			}
		}
		reset_ccmd_hw(pdma_dev, ccmd_ctx);
		releaseCCMDCid(pdma_dev, ccmd_ctx->cid, ccmd_ctx->mode);
		ccmd_context_destroy(ccmd_ctx);
	}
	mutex_unlock(&pdma_dev->pdma_device_lock);

	kfree(ccmd_ctx);
	filp->private_data = NULL;
	spin_lock_irqsave(&g_pdma_file_lock, flags);
	g_pdma_open_cnt--;
	spin_unlock_irqrestore(&g_pdma_file_lock, flags);

	pr_debug("%s open count %u\n", __func__, g_pdma_open_cnt);
	return 0;
}

static int gpu_pdma_mmap(struct file *const filp,
	struct vm_area_struct *const vma)
{
	unsigned long length = vma->vm_end - vma->vm_start;
	unsigned long phy_addr = vma->vm_pgoff << PAGE_SHIFT;
	struct ccmd_context *ccmd_ctx = filp->private_data;
	struct pdma_device *pdma_dev;
	unsigned long ringbuf_pa;

	pr_debug("[CCMD] %s phy: %lx, length: %ld (%p)",
		__func__, phy_addr, length, ccmd_ctx);

	if (!ccmd_ctx)
		return -EBADF;

	if (!ccmd_ctx->pdma_dev)
		return -ENODEV;

	if (ccmd_ctx->cid == CCMD_UNSUPPORTED_CID)
		return -EINVAL;

	pdma_dev = ccmd_ctx->pdma_dev;
	ringbuf_pa = ccmd_ctx->ringbuf_paddr;

	mutex_lock(&pdma_dev->pdma_device_lock);

	if (!IsValidCcmdCtx(ccmd_ctx)) {
		mutex_unlock(&pdma_dev->pdma_device_lock);
		return -EINVAL;
	}

	if (phy_addr == ringbuf_pa && phy_addr != 0 &&
		length == RING_BUFFER_PAGE_SIZE(pdma_dev->page_order)) {
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	} else if (phy_addr == ccmd_ctx->cid_reg_base && phy_addr != 0 &&
		length == pdma_dev->reg_region) {
		vma->vm_page_prot =  pgprot_noncached(vma->vm_page_prot);
	} else if (phy_addr == pdma_dev->hw_sem_base && phy_addr != 0 &&
		((length >> PAGE_SHIFT) == 1)) {
		/* Expect only one page needed from hw semaphore base */
		vma->vm_page_prot =  pgprot_noncached(vma->vm_page_prot);
	} else {
		pr_info("Invalid argument! addr: 0x%lx, length: %ld\n", phy_addr, length);
		mutex_unlock(&pdma_dev->pdma_device_lock);
		return -EINVAL;
	}

	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
		vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		pr_info("%s remap_pfn_range fail\n", __func__);
		mutex_unlock(&pdma_dev->pdma_device_lock);
		return -EAGAIN;
	}

	mutex_unlock(&pdma_dev->pdma_device_lock);
	vma->vm_ops = &gpu_pdma_vm_ops;
	return 0;
}

static long gpu_pdma_unlocked_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	long ret = 0;
	u32 cid;
	struct pdma_hw_lock hw_lock;
	struct pdma_rw_ptr rw_ptr;
	struct ccmd_context *ccmd_ctx = filp->private_data;
	struct pdma_device *pdma_dev;

	if (!ccmd_ctx)
		return -EBADF;

	pdma_dev = ccmd_ctx->pdma_dev;
	if (!pdma_dev)
		return -ENODEV;

	switch (cmd) {
	case GPU_PDMA_LOCKHW:
		ret = copy_from_user(&hw_lock, (void __user *)arg, sizeof(struct pdma_hw_lock));
		if (ret) {
			pr_info("[ERROR] GPU_PDMA_LOCKHW copy_from_user Fail: %lu\n", ret);
			return -EFAULT;
		}
		mutex_lock(&pdma_dev->pdma_device_lock);
		cid = getCCMDCid(pdma_dev, hw_lock.in.mode);
		if (cid != CCMD_UNSUPPORTED_CID && ccmd_ctx->cid == CCMD_UNSUPPORTED_CID) {
			/* Init context */
			if (ccmd_context_init(
				ccmd_ctx, hw_lock.in.kctx_id, cid, hw_lock.in.mode)) {
				pr_info("Init ccmd context failed\n");
				releaseCCMDCid(pdma_dev, cid, hw_lock.in.mode);
				mutex_unlock(&pdma_dev->pdma_device_lock);
				return -EFAULT;
			}

			/* Config HW */
			if (init_ccmd_hw(pdma_dev, ccmd_ctx)) {
				pr_info("Config ring buffer PA fail. pid/tid: %d/%d (%d)\n",
					current->tgid, current->pid, hw_lock.in.kctx_id);

				releaseCCMDCid(pdma_dev, cid, hw_lock.in.mode);
				ccmd_context_destroy(ccmd_ctx);
				mutex_unlock(&pdma_dev->pdma_device_lock);
				return -EFAULT;
			}

			hw_lock.out.status = 1;
			hw_lock.out.base = ccmd_ctx->cid_reg_base;		/* PA of cidx ptr base */
			hw_lock.out.region_size = pdma_dev->reg_region;
			hw_lock.out.ringbuf = ccmd_ctx->ringbuf_paddr; /* pa of ring */
			hw_lock.out.size = RING_BUFFER_PAGE_SIZE(pdma_dev->page_order);
			hw_lock.out.hw_sem_base = pdma_dev->hw_sem_base;
			hw_lock.out.hw_sem_offset = pdma_dev->hw_sem_offset;
			hw_lock.out.sw_ver = pdma_dev->sw_version;
			hw_lock.out.cid = ccmd_ctx->cid;
#ifdef CCMD_DEBUG_MODE
			hw_lock.out.debug_mode = 1;
#else
			hw_lock.out.debug_mode = 0;
#endif
			pr_info("[CCMD] GPU_PDMA_LOCKHW success pid/tid: %d/%d, (%u)(%u)\n",
				current->tgid, current->pid, hw_lock.in.kctx_id, ccmd_ctx->cid);
/*
			if (hw_lock.in.mode == 1){
				pdma_dev->dynamic_mode = ged_gpu_slc_get_dynamic_mode();
				ged_gpu_slc_dynamic_mode(POLICY_TEX_CACHE_LSC_ALLOC);
			}
*/
		} else
			memset(&hw_lock, 0, sizeof(struct pdma_hw_lock));


		if (!hw_lock.out.status) {
			pr_info("[CCMD] GPU_PDMA_LOCKHW Fail by pid/tid: %d/%d (%d)\n",
				current->tgid, current->pid, hw_lock.in.kctx_id);
			mutex_unlock(&pdma_dev->pdma_device_lock);
			return -EBUSY;
		}
		mutex_unlock(&pdma_dev->pdma_device_lock);

		ret = copy_to_user((void __user *)arg, &hw_lock, sizeof(struct pdma_hw_lock));
		if (ret) {
			pr_info("[ERROR] GPU_PDMA_LOCKHW copy_to_user Fail: %lu\n", ret);
			return -EFAULT;
		}
		break;
	case GPU_PDMA_UNLOCKHW:

		ret = copy_from_user(&hw_lock, (void __user *)arg, sizeof(struct pdma_hw_lock));
		if (ret) {
			pr_info("[ERROR] GPU_PDMA_UNLOCKHW copy_from_user Fail: %lu\n", ret);
			return -EFAULT;
		}

		mutex_lock(&pdma_dev->pdma_device_lock);
		if (IsValidCcmdCtx(ccmd_ctx) &&
				(hw_lock.in.kctx_id == ccmd_ctx->kctx_id)) {
			pr_info("GPU_PDMA_UNLOCKHW done and ctx %u, cid %u release lock\n",
				ccmd_ctx->kctx_id, ccmd_ctx->cid);

			reset_ccmd_hw(pdma_dev, ccmd_ctx);
			releaseCCMDCid(pdma_dev, ccmd_ctx->cid, ccmd_ctx->mode);
			ccmd_context_destroy(ccmd_ctx);
			hw_lock.out.status = 1;
			hw_lock.out.base = 0;
			hw_lock.out.region_size = 0;
			hw_lock.out.ringbuf = 0;
			hw_lock.out.size = 0;
			hw_lock.out.hw_sem_base = 0;
			hw_lock.out.hw_sem_offset = 0;
			hw_lock.out.cid = 0;
			//ged_gpu_slc_dynamic_mode(pdma_dev->dynamic_mode);
		} else {
			pr_info("GPU_PDMA_UNLOCKHW failed. matched kctx_id: %u != %u (%d)\n",
					hw_lock.in.kctx_id, ccmd_ctx->kctx_id, current->pid);
				mutex_unlock(&pdma_dev->pdma_device_lock);
				return -EINVAL;
		}
		mutex_unlock(&pdma_dev->pdma_device_lock);
		ret = copy_to_user((void __user *)arg, &hw_lock, sizeof(struct pdma_hw_lock));
		if (ret) {
			pr_info("[ERROR] GPU_PDMA_UNLOCKHW copy_to_user Fail: %lu\n", ret);
			return -EFAULT;
		}
		break;
	case GPU_PDMA_WRITE_HWPTR:
		ret = copy_from_user(&rw_ptr, (void __user *)arg, sizeof(struct pdma_rw_ptr));
		if (ret) {
			pr_info("[ERROR] GPU_PDMA_WRITE_HWPTR copy_from_user Fail: %lu\n", ret);
			return -EFAULT;
		}
		mutex_lock(&pdma_dev->pdma_device_lock);
		if (IsValidCcmdCtx(ccmd_ctx) ||
			(ccmd_ctx->kctx_id != rw_ptr.in.kctx_id)) {
			pr_info("Must lock hw before update ptr: %u,%u,%u\n",
				rw_ptr.in.kctx_id, ccmd_ctx->kctx_id, ccmd_ctx->cid);
			mutex_unlock(&pdma_dev->pdma_device_lock);
			return -EINVAL;
		}

		/* write hwptr reg */
		writel(rw_ptr.in.hwptr,
			(pdma_dev->pdma_reg_base_kva +
			(CIDX_RING_BUFFER_HWPTR(ccmd_ctx->cid))));
		pr_debug("update hwptr: 0x%08X\n",
			readl(pdma_dev->pdma_reg_base_kva +
			(CIDX_RING_BUFFER_HWPTR(ccmd_ctx->cid))));
		mutex_unlock(&pdma_dev->pdma_device_lock);
		break;
	case GPU_PDMA_READ_HRPTR:
		/* read hrptr reg */
		ret = copy_from_user(&rw_ptr, (void __user *)arg, sizeof(struct pdma_rw_ptr));
		if (ret) {
			pr_info("[ERROR] GPU_PDMA_WRITE_HWPTR copy_from_user Fail: %lu\n", ret);
			return -EFAULT;
		}
		mutex_lock(&pdma_dev->pdma_device_lock);
		if (IsValidCcmdCtx(ccmd_ctx) ||
			(ccmd_ctx->kctx_id != rw_ptr.in.kctx_id)) {
			pr_info("Must lock hw before update ptr: %u,%u,%u\n",
				rw_ptr.in.kctx_id, ccmd_ctx->kctx_id, ccmd_ctx->cid);
			mutex_unlock(&pdma_dev->pdma_device_lock);
			return -EINVAL;
		}
		rw_ptr.out.hrptr = readl(pdma_dev->pdma_reg_base_kva +
			CIDX_RING_BUFFER_HRPTR(ccmd_ctx->cid));
		pr_debug("read hrptr: 0x%08X\n", rw_ptr.out.hrptr);

		ret = copy_to_user((void __user *)arg, &rw_ptr, sizeof(unsigned int));
		if (ret) {
			pr_info("[ERROR] GPU_CCMD_READ_HRPTR copy_to_user Fail: %lu\n", ret);
			mutex_unlock(&pdma_dev->pdma_device_lock);
			return -EFAULT;
		}
		mutex_unlock(&pdma_dev->pdma_device_lock);
		break;
	default:
		ret = -1;
		break;
	}
	return ret;
}

static long gpu_pdma_compat_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	long ret;
	void __user *data;

	data = compat_ptr((uint32_t)arg);
	ret = gpu_pdma_unlocked_ioctl(filp, cmd, (unsigned long)data);

	return ret;
}

static int gpu_pdma_get_hw_sem(struct pdma_device *pdma_dev)
{
	int ret = -1;

	if (!pdma_dev)
		return ret;

	if (pdma_dev->hw_sem_base && pdma_dev->hw_sem_offset)
		ret = (
			(readl(pdma_dev->pdma_hw_sem_base_kva) >> pdma_dev->hw_sem_bit)
			& 0x1);
	else
		pr_info("@%s: get hw sem status failed\n", __func__);
	return ret;
}

static void gpu_pdma_set_irq(struct pdma_device *pdma_dev, int idx)
{
	struct pdma_sram *pdma_sram_base;

	if (!pdma_dev)
		return;

	pdma_sram_base = pdma_dev->pdma_sram_base_kva;
	if (pdma_sram_base) {
		/* Clear interrupt status if irq is to be disabled*/
		if (idx != 0)
			pdma_sram_base->interrupt_status =
				((pdma_sram_base->interrupt_status >> 1) << 1) | (idx & 0x1);
		else
			pdma_sram_base->interrupt_status = 0;
	} else
		pr_info("@%s: set irq failed\n", __func__);
}

const struct file_operations gpu_pdma_fops = {
	.owner = THIS_MODULE,
	.open = gpu_pdma_open,
	.release = gpu_pdma_release,
	.mmap = gpu_pdma_mmap,
	.unlocked_ioctl = gpu_pdma_unlocked_ioctl,
	.compat_ioctl = gpu_pdma_compat_ioctl,
};

static struct miscdevice gpu_pdma_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gpu_pdma",
	.fops = &gpu_pdma_fops,
};

static ssize_t gpu_pdma_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	u32 used_pbha_cnt = 0;
	struct pdma_device *pdma_dev = get_PDMA_Device();
	struct list_head *entry, *tmp, *pbha_entry, *pbha_tmp;
	u32 available_pbha_cnt;

	if (!pdma_dev)
		return -ENODEV;

	available_pbha_cnt = (1 << pdma_dev->extended_pbha_bits)-CCMD_RESERVED_PBHA_NUM;

	mutex_lock(&pdma_dev->pdma_device_lock);

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"hwlock status:		0x%x\n", pdma_dev->ccmd_locked_ctx_id);
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"hw_sem status:		0x%x\n", gpu_pdma_get_hw_sem(pdma_dev));
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"interrupt status:	0x%x\n",
		(pdma_dev->pdma_sram_base_kva->interrupt_status & 0x2)>>1);
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"irq enable status:	0x%x\n",
		pdma_dev->pdma_sram_base_kva->interrupt_status & 0x1);

	if (pdma_dev->ccmd_locked_ctx_id != 0) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"ccmd context info:\n");
		list_for_each_safe(entry, tmp, &pdma_dev->ctx_list) {
			struct ccmd_context *ctx;

			ctx = list_entry(entry, struct ccmd_context, entry);
			pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"cid:			0x%x\n", ctx->cid);
			pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"kctx:			0x%x\n", ctx->kctx_id);
			pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"mode:			[%s]\n", (ctx->mode == COMPUTE_TLS)
				? "COMPUTE_TLS" : "SMART API");
			pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"pbha owned		: ");
			list_for_each_safe(pbha_entry, pbha_tmp, &ctx->pbha_list) {
				struct extended_pbha *ext_pbha;

				ext_pbha = list_entry(pbha_entry, struct extended_pbha, entry);
				pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"%u ,", ext_pbha->id);
				used_pbha_cnt++;
			}
			pos += scnprintf(buf + pos, PAGE_SIZE - pos, "\n");
			pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"ringbuf base:		0x%llx\n\n", ctx->ringbuf_paddr);
		}
	}
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"remaining PBHA: %u/%u\n",
		(available_pbha_cnt - used_pbha_cnt), available_pbha_cnt);
	mutex_unlock(&pdma_dev->pdma_device_lock);

	return pos;
}

static ssize_t gpu_pdma_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t n)
{
	struct pdma_device *pdma_dev = get_PDMA_Device();
	int val;

	if (!pdma_dev)
		return -ENODEV;

	if (kstrtouint(buf, 0, &val) == 0){
		if (val == 0 || val == 1)
			gpu_pdma_set_irq(pdma_dev, val);
		else
			pr_info("@%s: Invalid value for gpu_pdma\n", __func__);
	}

	return n;
}
DEVICE_ATTR_RW(gpu_pdma);
/* /sys/class/misc/gpu_pdma/gpu_pdma */
static int __create_file(void)
{
	int ret = 0;

	ret = misc_register(&gpu_pdma_device);
	if (unlikely(ret != 0)) {
		pr_info("@%s: misc register failed\n", __func__);
		return ret;
	}

	ret = device_create_file(gpu_pdma_device.this_device,
		&dev_attr_gpu_pdma);
	if (unlikely(ret != 0))
		return ret;

	return 0;
}
static void __delete_file(void)
{
	device_remove_file(gpu_pdma_device.this_device,
		&dev_attr_gpu_pdma);
	misc_deregister(&gpu_pdma_device);
}

static int gpu_pdma_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;
	struct device_node *node = pdev->dev.of_node;

	g_pdma_dev = kzalloc(sizeof(struct pdma_device), GFP_KERNEL);

	if (!g_pdma_dev)
		return -ENOMEM;

	/* Init device */
	g_pdma_dev->dev = &pdev->dev;
	INIT_LIST_HEAD(&g_pdma_dev->ctx_list);
	mutex_init(&g_pdma_dev->pdma_device_lock);

#ifdef CCMD_DEBUG_MODE
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
#else
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
#endif
	if (res == NULL) {
		pr_info("PDMA platform_get_resource fail\n");
		return -ENODEV;
	}
	g_pdma_dev->reg_base = res->start;
	g_pdma_dev->reg_region = res->end - res->start + 1;
	g_pdma_dev->pdma_reg_base_kva = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(g_pdma_dev->pdma_reg_base_kva))
		return PTR_ERR(g_pdma_dev->pdma_reg_base_kva);

	pr_info("@%s: PDMA reg base: 0x%llx, size: 0x%llx, kva: 0x%p\n", __func__,
		g_pdma_dev->reg_base, g_pdma_dev->reg_region,
		g_pdma_dev->pdma_reg_base_kva);

	/* Remove  get HW  Semaphore */
	g_pdma_dev->hw_sem_base = 0;
	g_pdma_dev->hw_sem_offset = 0;

	if (of_property_read_u8(node, "concurrent-contexts",
			&g_pdma_dev->max_ctx_cnt) < 0) {
		/* Support single context only if not specified */
		g_pdma_dev->max_ctx_cnt = 1;
	}

	if (of_property_read_u8(node, "concurrent-non-api-contexts",
		&g_pdma_dev->max_non_api_ctx_cnt) < 0) {
		/* Support single context only if not specified */
		g_pdma_dev->max_non_api_ctx_cnt = 1;
	} else if (g_pdma_dev->max_non_api_ctx_cnt > g_pdma_dev->max_ctx_cnt) {
		/* non api context should be less than
		 * or equal to max concurrent context
		 */
		g_pdma_dev->max_non_api_ctx_cnt = (g_pdma_dev->max_ctx_cnt == 1) ?
			1 : (g_pdma_dev->max_ctx_cnt - 1) ;
	}
	g_pdma_dev->non_api_ctx_cnt = 0;
	pr_info("CCMD supports 0x%x(0x%x) context(s) concurrently\n",
		g_pdma_dev->max_ctx_cnt, g_pdma_dev->max_non_api_ctx_cnt);

	if (!of_property_read_u32(node, "ringbuf-page-order", &g_pdma_dev->page_order)) {
		pr_info("PDMA get page size order of ring buffer: %u. PAGE_SIZE: 0x%lx\n",
			g_pdma_dev->page_order, PAGE_SIZE);
	} else {
		pr_info("PDMA get ringbuf-page-order fail\n");
		return -ENODEV;
	}

	if (!of_property_read_u32(node, "config-mode", &g_pdma_dev->config_mode)) {
		pr_info("PDMA get config-mode: %u\n", g_pdma_dev->config_mode);
	} else {
		pr_info("PDMA get g_config_mode fail\n");
		g_pdma_dev->config_mode = CCMD_CONFIG_MODE_GPUEB;
	}

	/* sram */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (res == NULL) {
		pr_info("PDMA platform_get_resource 2 fail\n");
		return -ENODEV;
	}
	g_pdma_dev->pdma_sram_base = res->start;
	g_pdma_dev->pdma_sram_base_kva =
		devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR(g_pdma_dev->pdma_sram_base_kva))
		return PTR_ERR(g_pdma_dev->pdma_sram_base_kva);

	/* init sram values */
	g_pdma_dev->pdma_sram_base_kva->ccmd_hw_reset = 0;
	g_pdma_dev->pdma_sram_base_kva->interrupt_status = 0;

	pr_info("@%s: SRAM base: 0x%llx, size: 0x%llx, kva: 0x%p\n", __func__,
		g_pdma_dev->pdma_sram_base, resource_size(res),
		g_pdma_dev->pdma_sram_base_kva);

	init_pbha_pool(node, g_pdma_dev);

	ret = __create_file();
	if (ret)
		pr_info("@%s: __create_files failed: %d\n", __func__, ret);

	ret = pdma_get_chipid(g_pdma_dev);
	if (ret)
		pr_info("@%s: __get sw version fail: %d\n", __func__, ret);

	dma_set_mask(g_pdma_dev->dev, DMA_BIT_MASK(64));

	return ret;
}

static void gpu_pdma_remove(struct platform_device *pdev)
{
	__delete_file();
}

static struct platform_driver gpu_pdma_driver = {
	.probe = gpu_pdma_probe,
	.remove = gpu_pdma_remove,
	.driver = {
		.name  = PDMA_DEVNAME,
		.owner = THIS_MODULE,
		.of_match_table = pdma_of_match,
	},
};

static int __init gpu_pdma_init(void)
{
	int ret;

	ret = platform_driver_register(&gpu_pdma_driver);
	if (ret)
		pr_info("Fail to register PDMA platform driver\n");

	return ret;
}

static void __exit gpu_pdma_exit(void)
{
	platform_driver_unregister(&gpu_pdma_driver);
}

void pdma_lock_reclaim(u32 kctx_id)
{
	struct list_head *entry, *tmp, *pbha_entry, *pbha_tmp;
	struct ccmd_context *ccmd_ctx = NULL;
	struct extended_pbha *ext_pbha = NULL;

	mutex_lock(&g_pdma_dev->pdma_device_lock);
	if (g_pdma_dev->ccmd_locked_ctx_id == 0) {
		pr_debug("[PDMA] HW is not locked.\n");
		mutex_unlock(&g_pdma_dev->pdma_device_lock);
		return;
	}

	/* vaidate ccmd_ctx */
	list_for_each_safe(entry, tmp, &g_pdma_dev->ctx_list) {
		ccmd_ctx = list_entry(entry, struct ccmd_context, entry);
		if (ccmd_ctx->kctx_id == kctx_id) {
			// Need to check if pbha is all returned?
			if (unlikely(!list_empty(&ccmd_ctx->pbha_list))) {
				pr_info("[CCMD] %s unreturned PBHA ID found in kctx/cid %u/%u",
					__func__, ccmd_ctx->kctx_id, ccmd_ctx->cid);
				list_for_each_safe(pbha_entry, pbha_tmp, &ccmd_ctx->pbha_list) {
					ext_pbha = list_entry(pbha_entry, struct extended_pbha, entry);
					__pdma_release_extended_pbha(ccmd_ctx->kctx_id, ext_pbha->id);
				}
			}
			reset_ccmd_hw(g_pdma_dev, ccmd_ctx);
			releaseCCMDCid(g_pdma_dev, ccmd_ctx->cid, ccmd_ctx->mode);
			ccmd_context_destroy(ccmd_ctx);
			pr_info("%s reclaim done and kctx %u release lock\n", __func__, kctx_id);
		}
	}
	mutex_unlock(&g_pdma_dev->pdma_device_lock);
}
EXPORT_SYMBOL_GPL(pdma_lock_reclaim);

u32 pdma_request_extended_pbha(u32 kctx_id)
{
	u32 pbha_id = CCMD_UNSUPPORTED_PBHA_ID;
	struct extended_pbha *ext_pbha = NULL;
	struct list_head *entry, *tmp;
	struct ccmd_context *ccmd_ctx = NULL;

	mutex_lock(&g_pdma_dev->pdma_device_lock);

	/* validate kctx_id */
	list_for_each_safe(entry, tmp, &g_pdma_dev->ctx_list) {
		ccmd_ctx = list_entry(entry, struct ccmd_context, entry);
		if (ccmd_ctx->kctx_id == kctx_id)
			break;
	}

	if (ccmd_ctx) {
		if (!list_empty(&g_pdma_dev->extened_pbha_pool)) {
			ext_pbha = list_first_entry(&g_pdma_dev->extened_pbha_pool,
				struct extended_pbha, entry);
			pbha_id = ext_pbha->id;

			pr_info("[CCMD] %s: kctx/cid: %u/%u request PBHA %u\n",
				__func__, kctx_id, ccmd_ctx->cid, pbha_id);
			list_del_init(&ext_pbha->entry);
			list_add_tail(&ext_pbha->entry, &ccmd_ctx->pbha_list);
		} else
			// TODO: DUMP CTXs PBHA ID LIST
			pr_info("[CCMD] %s: No available PBHA ID for kctx %u!!\n",
				__func__, kctx_id);
	} else
		pr_info("[CCMD] %s: kctx %u doesn't lock CCMD HW\n", __func__,
			kctx_id);
	mutex_unlock(&g_pdma_dev->pdma_device_lock);

	return pbha_id;
}
EXPORT_SYMBOL_GPL(pdma_request_extended_pbha);

static void __pdma_release_extended_pbha(u32 kctx_id, u32 pbha_id)
{
	struct extended_pbha *ext_pbha;
	struct list_head *entry, *tmp;
	struct ccmd_context *ccmd_ctx = NULL;

	lockdep_assert_held(&g_pdma_dev->pdma_device_lock);
	/* validate kctx_id */
	list_for_each_safe(entry, tmp, &g_pdma_dev->ctx_list) {
		ccmd_ctx = list_entry(entry, struct ccmd_context, entry);
		if (ccmd_ctx->kctx_id == kctx_id)
			break;
	}

	if (ccmd_ctx) {
		if (!list_empty(&ccmd_ctx->pbha_list)) {
			list_for_each_safe(entry, tmp, &ccmd_ctx->pbha_list) {
				ext_pbha = list_entry(entry, struct extended_pbha, entry);
				if (ext_pbha->id == pbha_id) {
					list_del_init(&ext_pbha->entry);
					list_add_tail(&ext_pbha->entry, &g_pdma_dev->extened_pbha_pool);
					pr_info("[CCMD] %s: release pbha %u from kctx/cid: %u/%u\n",
						__func__, pbha_id, kctx_id, ccmd_ctx->cid);
					break;
				}
				pr_info("[CCMD] %s: PBHA %u(%u) does not belongs to kctx %u\n",
					__func__, pbha_id, ext_pbha->id, kctx_id);
			}
		} else
			pr_info("[CCMD] %s: kctx %u doesn't request any PBHA ID\n",
						__func__, kctx_id);
	} else
		pr_info("[CCMD] %s: kctx %u doesn't lock CCMD HW\n", __func__,
			kctx_id);
}
void pdma_release_extended_pbha(u32 kctx_id, u32 pbha_id)
{

	mutex_lock(&g_pdma_dev->pdma_device_lock);
	__pdma_release_extended_pbha(kctx_id, pbha_id);
	mutex_unlock(&g_pdma_dev->pdma_device_lock);
}
EXPORT_SYMBOL_GPL(pdma_release_extended_pbha);

void pdma_zombie_entry_clean_up(void)
{

}
EXPORT_SYMBOL_GPL(pdma_zombie_entry_clean_up);

module_init(gpu_pdma_init);
module_exit(gpu_pdma_exit);
MODULE_LICENSE("GPL");
