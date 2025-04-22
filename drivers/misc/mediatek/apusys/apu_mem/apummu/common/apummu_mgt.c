// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/list.h>
#include <linux/mutex.h>

#include <linux/dma-buf.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/bitmap.h>
#include <linux/find.h>
#include <linux/bitops.h>
#include <linux/vmalloc.h>

#include <apusys_device.h>
#include "apummu_drv.h"
#include "apummu_mgt.h"
#include "apummu_mem.h"
#include "apummu_remote_cmd.h"
#include "apummu_cmn.h"
#include "apummu_trace.h"

#if IS_ENABLED(CONFIG_MTK_SLBC)
#include "slbc_ops.h"
static int gid;
static struct slbc_gid_data *ammu_slbc_gid_data;
#endif

extern struct apummu_dev_info *g_adv;

struct apummu_tbl g_ammu_table_set;

#define AMMU_FREE_DRAM_DELAY_MS	(5 * 1000)
static struct workqueue_struct *ammu_workq;
static struct delayed_work DRAM_free_work;

#define SHIFT_BITS			(12)

#define IOVA2EVA(input_addr, offset)	(input_addr - offset)
#define EVA2IOVA(input_addr, offset)	(input_addr + offset)

#define AMMU_SSID_MAX		(256)
#define SLC_DC_BUF_EVA      0x0F000000

static DEFINE_MUTEX(session_lock);
static DEFINE_SPINLOCK(ssid_lock);
static DECLARE_BITMAP(ssid_bitmap, AMMU_SSID_MAX);

/**
 * @input:
 *  type -> buffer type
 *  input_addr -> addr to encode (IOVA)
 *  output_addr -> encoded address (EVA)
 * @output:
 *  if encode succeeded
 * @description:
 *  encode input addr according to type
 */
static int addr_encode(uint64_t input_addr, enum AMMU_BUF_TYPE type, uint64_t *output_addr, uint SLC_DC_EN)
{
	int ret = 0;
	uint64_t ret_addr;

	switch (type) {
	case AMMU_DATA_BUF:
		ret_addr = IOVA2EVA(input_addr, g_adv->plat.encode_offset);
		if (SLC_DC_EN)
			ret_addr = SLC_DC_BUF_EVA;
		break;
	case AMMU_CMD_BUF:
	case AMMU_VLM_BUF:
		ret_addr = input_addr;
		break;
	default:
		AMMU_LOG_ERR("APUMMU encode invalid buffer type(%u)\n", type);
		ret = -EINVAL;
		goto out;
	}

	*output_addr = ret_addr;
out:
	return ret;
}

static int addr_decode(uint64_t input_addr, enum AMMU_BUF_TYPE type, uint64_t *output_addr)
{
	int ret = 0;
	uint64_t ret_addr;

	switch (type) {
	case AMMU_DATA_BUF:
		ret_addr = EVA2IOVA(input_addr, g_adv->plat.encode_offset);
		break;
	case AMMU_CMD_BUF:
	case AMMU_VLM_BUF:
		ret_addr = input_addr;
		break;
	default:
		AMMU_LOG_ERR("APUMMU decode invalid buffer type(%u)\n", type);
		ret = -EINVAL;
		goto out;
	}

	*output_addr = ret_addr;
out:
	return ret;
}

int apummu_eva_decode(uint64_t eva, uint64_t *iova, enum AMMU_BUF_TYPE type)
{
	return addr_decode(eva, type, iova);
}

/**
 * @input:
 *  session -> for session check
 * @output:
 *  if the stable of input session is exist
 * @description:
 *  Check if session table of input session exist
 */
static struct apummu_session_tbl *session_table_find(uint64_t session)
{
	struct list_head *list_ptr;
	struct apummu_session_tbl *sTable_ptr;

	mutex_lock(&g_ammu_table_set.gtable_lock);
	list_for_each(list_ptr, &g_ammu_table_set.g_stable_head) {
		sTable_ptr = list_entry(list_ptr, struct apummu_session_tbl, list);
		if (sTable_ptr->session == session) {
			mutex_unlock(&g_ammu_table_set.gtable_lock);
			return sTable_ptr;
		}
	}
	mutex_unlock(&g_ammu_table_set.gtable_lock);

	return NULL;
}

static void count_page_array_en_num(struct apummu_session_tbl *sTable_ptr)
{
	int i;
	uint32_t idx;

	for (idx = 0; idx < 2; idx++) {
		if (sTable_ptr->stable_info.DRAM_page_array_mask[idx] != 0) {
			for (i = 31; i >= 0; i--)
				if (sTable_ptr->stable_info.DRAM_page_array_mask[idx] & (1 << i))
					break;

			sTable_ptr->stable_info.DRAM_page_array_en_num[idx] = i + 1;
		} else
			sTable_ptr->stable_info.DRAM_page_array_en_num[idx] = 0;
	}
}

static void ammu_DRAM_free_work(struct work_struct *work)
{
	mutex_lock(&g_ammu_table_set.DRAM_FB_lock);
	if (g_ammu_table_set.is_work_canceled) {
		ammu_trace_begin("APUMMU: free DRAM");
		apummu_dram_remap_runtime_free(g_adv);
		ammu_trace_end();
		AMMU_LOG_INFO("Delay DRAM Free done\n");
	} else
		g_ammu_table_set.is_work_canceled = true;

	g_ammu_table_set.is_free_job_set = false;
	mutex_unlock(&g_ammu_table_set.DRAM_FB_lock);
}

static void free_memory(void)
{
	ammu_trace_begin("APUMMU: free memory");

	mutex_lock(&g_ammu_table_set.DRAM_FB_lock);
	mutex_lock(&g_ammu_table_set.gtable_lock);
	if (g_adv->plat.alloc_DRAM_FB_in_session_create) {
		if (g_adv->rsc.vlm_dram.iova != 0) {
			queue_delayed_work(ammu_workq, &DRAM_free_work,
				msecs_to_jiffies(AMMU_FREE_DRAM_DELAY_MS));
			g_ammu_table_set.is_free_job_set = true;
		}
	} else {
		apummu_dram_remap_runtime_free_whole_list(g_adv);
		g_ammu_table_set.subcmd_refcnt = 0;
		g_ammu_table_set.alloc_subcmd_refcnt = 0;
		AMMU_LOG_INFO("DRAM FB Free done\n");
	}

	if (g_ammu_table_set.is_SLB_alloc) {
		ammu_trace_begin("APUMMU: Free SLB without IPI");
		/* MDW will close session in IPI handler in some case */
		// apummu_remote_mem_free_pool(g_adv);
		if (apummu_free_general_SLB(g_adv))
			AMMU_LOG_WRN("General APU SLB free fail\n");

		g_ammu_table_set.is_SLB_alloc = false;
		ammu_trace_end();
	}

	g_ammu_table_set.is_VLM_info_IPI_sent = false;
	g_ammu_table_set.is_SLB_set = false;
	mutex_unlock(&g_ammu_table_set.gtable_lock);
	mutex_unlock(&g_ammu_table_set.DRAM_FB_lock);

	ammu_trace_end();
}

static int apummu_whole_DRAM_FB_alloc(void)
{
	int ret = 0;

	/*
	 * Alloc DRAM for VLM fall back if DRAM is not allocated
	 * Cancel DRAM free delay workqueue if it is created
	 */
	if (g_adv->remote.vlm_size != 0) {
		if (g_adv->rsc.vlm_dram.iova == 0) {
			ammu_trace_begin("APUMMU: Alloc DRAM");
			ret = apummu_dram_remap_runtime_alloc(g_adv);
			if (ret) {
				ammu_trace_end();
				apusys_ammu_exception("alloc DRAM FB fail\n");
				goto out;
			}
			ammu_trace_end();
		} else { // DRAM not free, cancel delay job
			if (!cancel_delayed_work(&DRAM_free_work) && g_ammu_table_set.is_free_job_set)
				g_ammu_table_set.is_work_canceled = false;
			else
				g_ammu_table_set.is_free_job_set = false;
		}
	}

out:
	return ret;
}

static void apummu_general_SLB_alloc(void)
{
	if (!g_adv->plat.is_general_SLB_support)
		return;

	if (!g_ammu_table_set.is_SLB_alloc) { // SLB retry
		ammu_trace_begin("APUMMU: SLB alloc");
		/* Do not assign return value, since alloc SLB may fail */
		if (apummu_alloc_general_SLB(g_adv))
			AMMU_LOG_VERBO("general SLB alloc fail...\n");
		else
			g_ammu_table_set.is_SLB_alloc = true;

		ammu_trace_end();
	}
}

static int apummu_first_session_IPI(bool vlm_dram_condition)
{
	int ret = 0;

	ammu_trace_begin("APUMMU: SLB + DRAM IPI");
	// if (g_adv->rsc.vlm_dram.iova != 0 || g_ammu_table_set.is_SLB_alloc) {
	if (vlm_dram_condition || g_ammu_table_set.is_SLB_alloc) {
		ret = apummu_remote_set_hw_default_iova_one_shot(g_adv);
		if (ret) {
			ammu_trace_end();
			AMMU_LOG_ERR("Remote set hw IOVA one shot fail!!\n");
			apusys_ammu_exception("Set DRAM FB + SLB fail\n");
			goto out;
		}
	}
	ammu_trace_end();

	g_ammu_table_set.is_SLB_set = (g_ammu_table_set.is_SLB_alloc);
	g_ammu_table_set.is_VLM_info_IPI_sent = true;

out:
	return ret;
}

static int apummu_set_general_SLB_IPI(void)
{
	int ret = 0;

	if (!g_ammu_table_set.is_SLB_set && g_ammu_table_set.is_SLB_alloc) {
		ammu_trace_begin("APUMMU: SLB ONLY IPI");
		ret = apummu_remote_mem_add_pool(
			g_adv, APUMMU_MEM_TYPE_GENERAL_S,
			g_adv->rsc.genernal_SLB.iova,
			g_adv->rsc.genernal_SLB.size, 0
		);
		if (ret) {
			ammu_trace_end();
			apusys_ammu_exception("Set SLB fail\n");
			goto out;
		}
		ammu_trace_end();

		g_ammu_table_set.is_SLB_set = true;
	}

out:
	return ret;
}

static int DRAM_and_SLB_alloc(void)
{
	int ret = 0;

	mutex_lock(&g_ammu_table_set.DRAM_FB_lock);
	ret = apummu_whole_DRAM_FB_alloc();
	if (ret)
		goto out;

	apummu_general_SLB_alloc();

	if (!g_ammu_table_set.is_VLM_info_IPI_sent) {
		ret = apummu_first_session_IPI((g_adv->rsc.vlm_dram.iova != 0));
		if (ret)
			goto free_DRAM;
	} else {
		/* SLB retry IPI */
		ret = apummu_set_general_SLB_IPI();
		if (ret)
			goto free_general_SLB;
	}

out:
	mutex_unlock(&g_ammu_table_set.DRAM_FB_lock);
	return ret;

free_DRAM:
	apummu_dram_remap_runtime_free(g_adv);
free_general_SLB:
	apummu_free_general_SLB(g_adv);
	mutex_unlock(&g_ammu_table_set.DRAM_FB_lock);
	return ret;
}

static int DRAM_FB_alloc_with_size(uint32_t ctx_num_going_alloc, uint64_t *iova)
{
	int ret = 0;
	uint64_t ret_IOVA = 0;

	if (g_adv->remote.vlm_size != 0) {
		ammu_trace_begin("APUMMU: Alloc DRAM");
		ret = apummu_dram_remap_runtime_alloc_with_size(g_adv, ctx_num_going_alloc, &ret_IOVA);
		if (ret) {
			ammu_trace_end();
			apusys_ammu_exception("alloc DRAM FB fail\n");
			goto out;
		}
		ammu_trace_end();
	} else {
		AMMU_LOG_WRN("Should not call VLM alloc in VLM not supported platform\n");
	}

	*iova = ret_IOVA;

out:
	return ret;
}

static int partial_DRAM_FB_alloc_and_add_pool(uint64_t session, uint32_t subcmd_num)
{
	int subcmd_refcnt_diff, ret = 0;
	struct apummu_session_tbl *sTable_ptr;
	uint64_t allocated_iova;

	AMMU_LOG_VERBO("subcmd_num = %u\n", subcmd_num);

	sTable_ptr = session_table_find(session);
	if (!sTable_ptr) {
		AMMU_LOG_ERR("Session table NOT exist!!!(0x%llx)\n", session);
		ret = -ENOMEM;
		goto out;
	}

	// Count ctx number gonna alloc
	mutex_lock(&sTable_ptr->stable_lock);
	sTable_ptr->subcmd_num_for_DRAM_FB += subcmd_num;
	AMMU_LOG_VERBO("sTable_ptr->subcmd_num_for_DRAM_FB = %u\n", sTable_ptr->subcmd_num_for_DRAM_FB);
	mutex_unlock(&sTable_ptr->stable_lock);
	mutex_lock(&g_ammu_table_set.gtable_lock);
	g_ammu_table_set.subcmd_refcnt += subcmd_num;
	mutex_unlock(&g_ammu_table_set.gtable_lock);
	if (g_ammu_table_set.alloc_subcmd_refcnt >= g_adv->remote.dram_max)
		subcmd_refcnt_diff = 0;
	else {
		if (g_ammu_table_set.subcmd_refcnt >= g_adv->remote.dram_max)
			subcmd_refcnt_diff =
				g_adv->remote.dram_max -
				g_ammu_table_set.alloc_subcmd_refcnt;
		else
			subcmd_refcnt_diff =
				g_ammu_table_set.subcmd_refcnt -
				g_ammu_table_set.alloc_subcmd_refcnt;
	}

	// if there is ctx number gonna alloc, alloc and send IPI
	if (subcmd_refcnt_diff > 0) {
		ret = DRAM_FB_alloc_with_size(subcmd_refcnt_diff, &allocated_iova);
		if (ret)
			goto out;

		ret = apummu_remote_mem_add_pool(
			g_adv, APUMMU_MEM_TYPE_VLM,
			allocated_iova, subcmd_refcnt_diff,
			g_ammu_table_set.alloc_subcmd_refcnt
		);
		if (ret) {
			AMMU_LOG_ERR("APUMMU add DRAM into pool fail\n");
			goto free_DRAM;
		}
		g_ammu_table_set.alloc_subcmd_refcnt += subcmd_refcnt_diff;
	}
	AMMU_LOG_VERBO("subcmd_num = %u\n", subcmd_num);
	AMMU_LOG_VERBO("g_ammu_table_set.subcmd_refcnt = %u\n", g_ammu_table_set.subcmd_refcnt);
	AMMU_LOG_VERBO("g_ammu_table_set.alloc_subcmd_refcnt = %u\n", g_ammu_table_set.alloc_subcmd_refcnt);
	AMMU_LOG_VERBO("subcmd_refcnt_diff = %d\n", subcmd_refcnt_diff);

out:
	return ret;

free_DRAM:
	apummu_dram_remap_runtime_free_single_node(g_adv, allocated_iova);
	return ret;
}

static int general_SLB_alloc_and_add_pool(void)
{
	int ret = 0;

	apummu_general_SLB_alloc();
	ret = apummu_set_general_SLB_IPI();
	if (ret)
		goto free_general_SLB;

	return ret;

free_general_SLB:
	apummu_free_general_SLB(g_adv);
	return ret;
}

int ammu_DRAM_FB_alloc(uint64_t session, uint32_t vlm_size, uint32_t subcmd_num)
{
	int ret = 0;

	if (!g_adv->plat.alloc_DRAM_FB_in_session_create && vlm_size) {
		mutex_lock(&g_ammu_table_set.DRAM_FB_lock);
		ret = partial_DRAM_FB_alloc_and_add_pool(session, subcmd_num);
		general_SLB_alloc_and_add_pool();
		mutex_unlock(&g_ammu_table_set.DRAM_FB_lock);
	}

	return ret;
}

static u64 ssid_alloc_cnt;

static void ammu_SSID_alloc(struct apummu_session_tbl *sTable_ptr)
{
	static unsigned long ssid_idx = 0;
	unsigned long ssid_max = g_adv->plat.ssid_max + 1;
	unsigned long sess_num = g_adv->plat.reserved_session_num;
	unsigned long flags;
	unsigned long ssid;

	spin_lock_irqsave(&ssid_lock, flags);

	if (ssid_alloc_cnt < sess_num) {
		AMMU_LOG_INFO("Use minimum ssid\n");
		sTable_ptr->stable_info.SMMU_SSID = 0;
		sTable_ptr->ssid_need_free = false;
		goto exit;
	}

	ssid = find_first_bit(ssid_bitmap, ssid_max);
	if (ssid >= ssid_max) {
		AMMU_LOG_INFO("No more free ssid\n");
		sTable_ptr->stable_info.SMMU_SSID = ssid_idx;
		sTable_ptr->ssid_need_free = false;
		// next
		ssid_idx = ssid_idx == (ssid_max - 1) ? 0 : ssid_idx + 1;
		goto exit;
	}

	__clear_bit(ssid, ssid_bitmap);
	sTable_ptr->stable_info.SMMU_SSID = ssid;
	sTable_ptr->ssid_need_free = true;
exit:
	ssid_alloc_cnt++;
	AMMU_LOG_INFO("ssid: %d\n", sTable_ptr->stable_info.SMMU_SSID);

	spin_unlock_irqrestore(&ssid_lock, flags);
}

static void ammu_SSID_free(struct apummu_session_tbl *sTable_ptr)
{
	unsigned long flags;
	int ssid;
	bool free;

	// init
	ssid = sTable_ptr->stable_info.SMMU_SSID;
	free = sTable_ptr->ssid_need_free;

	AMMU_LOG_INFO("ssid: %d\n", ssid);

	spin_lock_irqsave(&ssid_lock, flags);

	if (ssid >= AMMU_SSID_MAX || !free)
		goto exit;

	__set_bit(ssid, ssid_bitmap);
exit:
	ssid_alloc_cnt--;

	spin_unlock_irqrestore(&ssid_lock, flags);
}

static int ammu_DRAM_FB_refcnt_adjust(uint64_t session, uint32_t subcmd_num)
{
	int ret = 0;
	struct apummu_session_tbl *sTable_ptr;

	// ref cnt adjust

	AMMU_LOG_VERBO("subcmd_num = %u\n", subcmd_num);

	sTable_ptr = session_table_find(session);
	if (!sTable_ptr) {
		AMMU_LOG_ERR("Session table NOT exist!!!(0x%llx)\n", session);
		ret = -ENOMEM;
		goto out;
	}

	mutex_lock(&sTable_ptr->stable_lock);
	sTable_ptr->subcmd_num_for_DRAM_FB -= subcmd_num;
	AMMU_LOG_VERBO("sTable_ptr->subcmd_num_for_DRAM_FB = %u\n", sTable_ptr->subcmd_num_for_DRAM_FB);
	mutex_unlock(&sTable_ptr->stable_lock);

	mutex_lock(&g_ammu_table_set.gtable_lock);
	g_ammu_table_set.subcmd_refcnt -= subcmd_num;
	mutex_unlock(&g_ammu_table_set.gtable_lock);

out:
	return ret;
}

int ammu_DRAM_FB_free(uint64_t session, uint32_t vlm_size, uint32_t subcmd_num)
{
	int ret = 0;

	if (!g_adv->plat.alloc_DRAM_FB_in_session_create && vlm_size)
		ret = ammu_DRAM_FB_refcnt_adjust(session, subcmd_num);

	return ret;
}

/**
 * @input:
 *  None
 * @output:
 *  if stable alloc succeeded
 * @description:
 *  alloc and return pointer
 */
static struct apummu_session_tbl *session_table_alloc_and_return(uint64_t session)
{
	int ret = 0;
	struct apummu_session_tbl *sTable_ptr = NULL;

	ammu_trace_begin("APUMMU: session table allocate");

	sTable_ptr = kvzalloc(sizeof(struct apummu_session_tbl), GFP_KERNEL);
	if (!sTable_ptr) {
		AMMU_LOG_ERR("Session table alloc failed, kvzalloc failed\n");
		goto out;
	}

	mutex_lock(&g_ammu_table_set.gtable_lock);
	g_ammu_table_set.session_tbl_cnt += 1;
	list_add_tail(&sTable_ptr->list, &g_ammu_table_set.g_stable_head);
	mutex_unlock(&g_ammu_table_set.gtable_lock);

	mutex_init(&sTable_ptr->stable_lock);
	sTable_ptr->session = session;

	if (g_adv->plat.is_ASE_support)
		ammu_SSID_alloc(sTable_ptr);

	if (g_adv->plat.alloc_DRAM_FB_in_session_create) {
		ret = DRAM_and_SLB_alloc();
		if (ret) {
			kvfree(sTable_ptr);
			sTable_ptr = NULL;
			goto out;
		}
	}

out:
	ammu_trace_end();
	return sTable_ptr;
}

static struct apummu_session_tbl *session_table_alloc_and_return_mtx(uint64_t session)
{
	struct apummu_session_tbl *tbl;

	mutex_lock(&session_lock);
	tbl = session_table_alloc_and_return(session);
	mutex_unlock(&session_lock);

	return tbl;
}

static int enable_slc(uint64_t session)
{
#if IS_ENABLED(CONFIG_MTK_SLBC)
	int ret_slbc = 0;
#endif
	if (g_adv->plat.is_SLC_support) {
		if (g_ammu_table_set.session_tbl_cnt == (g_adv->plat.reserved_session_num + 1)) {
			gid = -1;
			ammu_slbc_gid_data = vzalloc(sizeof(struct slbc_gid_data));
			ammu_slbc_gid_data->sign = 0x51ca11ca;
			ret_slbc = slbc_gid_request(ID_NPU, &gid, ammu_slbc_gid_data);
			if (ret_slbc)
				AMMU_LOG_INFO("slc request fail\n");
			ret_slbc = slbc_validate(ID_NPU, gid);
			if (ret_slbc)
				AMMU_LOG_INFO("slc validate fail\n");
			AMMU_LOG_INFO("enable slc success\n");
		}
	}
	AMMU_LOG_INFO("enable slc done\n");
	return ret_slbc;
}

static int session_table_alloc_inner(uint64_t session)
{
	int ret = 0;
	struct apummu_session_tbl *sTable_ptr;
	//int ret_slc = 0;

	if (g_adv == NULL) {
		AMMU_LOG_ERR("Invalid apummu_device\n");
		ret = -EINVAL;
		goto exit;
	}

	sTable_ptr = session_table_alloc_and_return(session);
	if (!sTable_ptr) {
		ret = -ENOMEM;
		goto exit;
	}
	ret = enable_slc(session);

exit:
	return ret;
}

int session_table_alloc(uint64_t session)
{
	int ret;

	mutex_lock(&session_lock);
	ret = session_table_alloc_inner(session);
	mutex_unlock(&session_lock);

	return ret;
}

/* device_va == iova */
int addr_encode_and_write_stable(enum AMMU_BUF_TYPE type, uint64_t session, uint64_t device_va,
	uint64_t buf_size, uint64_t *eva, uint SLC_DC_EN)
{
	int ret = 0;
	uint64_t ret_eva = 0;
	struct apummu_session_tbl *sTable_ptr;
	uint32_t SLB_type, cross_page_array_num = 0;
	uint8_t mask_idx;

	ammu_trace_begin("APUMMU: add table");

	if (g_adv == NULL) {
		AMMU_LOG_ERR("Invalid apummu_device\n");
		ret = -EINVAL;
		goto out;
	}

	AMMU_LOG_VERBO("session = 0x%llx, device_va = 0x%llx, size = 0x%llx\n",
		session, device_va, buf_size);

	if (device_va < 0x20000000) {
		if (((device_va >= g_adv->remote.vlm_addr) &&
			(device_va <= (g_adv->remote.vlm_addr + g_adv->remote.vlm_size)))) {
			ret_eva = device_va;
			goto out;
		} else if ((device_va >= g_adv->remote.SLB_base_addr) &&
			((device_va < (g_adv->remote.SLB_base_addr + g_adv->remote.SLB_size)))) {
			SLB_type = (device_va == g_adv->rsc.internal_SLB.iova)
				? APUMMU_MEM_TYPE_RSV_S
				: APUMMU_MEM_TYPE_EXT;

			ret = ammu_session_table_add_SLB(session, SLB_type);
			if (ret) {
				AMMU_LOG_ERR("IOVA 2 EVA SLB fail!!\n");
				goto out;
			}

			ret_eva = (device_va == g_adv->rsc.internal_SLB.iova)
				? g_adv->remote.vlm_addr
				: device_va;

			goto out;
		} else {
			AMMU_LOG_ERR("Invalid input VA 0x%llx\n", device_va);
			ret = -EINVAL;
			goto out;
		}
	}

	/* addr encode and CHECK input type */
	ret = addr_encode(device_va, type, &ret_eva, SLC_DC_EN);
	if (ret)
		goto out;

	if (!buf_size || device_va < 0x40000000)
		goto out;

	/* check if session table exist by session */
	sTable_ptr = session_table_find(session);
	if (!sTable_ptr) {
		/* if session table not exist alloc a session table */
		sTable_ptr = session_table_alloc_and_return_mtx(session);
		if (!sTable_ptr) {
			ret = -ENOMEM;
			goto out;
		}

		if (g_adv->plat.alloc_DRAM_FB_in_session_create) {
			ret = DRAM_and_SLB_alloc();
			if (ret)
				goto out;
		}
	}

	mutex_lock(&sTable_ptr->stable_lock);
	/* Hint for RV APUMMU fill VSID table */
	/* NOTE: cross_page_array_num is use when the given buffer cross differnet page array */
	if ((device_va >> SHIFT_BITS) & 0xf00000) { // mask for 36-bit
		uint32_t page_sft = g_adv->plat.address_bits == 36 ? 31 : 29;

		cross_page_array_num = (((device_va + buf_size - 1) >> page_sft)
							- ((device_va) >> page_sft));
		do {
			/* >> 29 = 512M / 0x20000000 */
			/* >> 31 =   2G / 0x80000000 */
			mask_idx = (((device_va - 0x100000000ull) >> page_sft)
						+ cross_page_array_num) & (0x1f);

			sTable_ptr->stable_info.DRAM_page_array_mask[1] |= (1 << mask_idx);
			sTable_ptr->DRAM_4_16G_mask_cnter[mask_idx] += 1;

		} while (cross_page_array_num--);
		sTable_ptr->stable_info.mem_mask |= (1 << DRAM_4_16G);
	} else {
		cross_page_array_num =
			(((device_va + buf_size - 1) / (0x8000000)) - ((device_va) / (0x8000000)));
		do {
			/* - (0x40000000) because mapping start from 1G */
			/* >> 27 = 128M / 0x20000000 */
			mask_idx = (((device_va - (0x40000000)) >> 27)
						+ cross_page_array_num) & (0x1f);

			sTable_ptr->stable_info.DRAM_page_array_mask[0] |= (1 << mask_idx);
			sTable_ptr->DRAM_1_4G_mask_cnter[mask_idx] += 1;
		} while (cross_page_array_num--);
		sTable_ptr->stable_info.mem_mask |= (1 << DRAM_1_4G);
	}

	count_page_array_en_num(sTable_ptr);
	mutex_unlock(&sTable_ptr->stable_lock);

out:
	*eva = ret_eva;

	AMMU_LOG_VERBO("apummu add 0x%llx -> 0x%llx in 0x%llx stable done\n",
		ret_eva, device_va, session);
	ammu_trace_end();
	return ret;
}

/* get session table by session */
int get_session_table(uint64_t session, void **tbl_kva, uint32_t *size)
{
	int ret = 0;
	struct apummu_session_tbl *sTable_ptr;

	ammu_trace_begin("APUMMU: Get session table");

	sTable_ptr = session_table_find(session);
	if (!sTable_ptr) {
		AMMU_LOG_ERR("Session table NOT exist!!!(0x%llx)\n", session);
		ret = -ENOMEM;
		goto out;
	}

	mutex_lock(&sTable_ptr->stable_lock);
	AMMU_LOG_VERBO("stable session(%llx), mem_mask = 0x%08x\n",
		sTable_ptr->session,
		sTable_ptr->stable_info.mem_mask);
	AMMU_LOG_VERBO("stable DRAM_page_array_mask 1~4G = 0x%08x, enable num = 0x%08x\n",
		sTable_ptr->stable_info.DRAM_page_array_mask[0],
		sTable_ptr->stable_info.DRAM_page_array_en_num[0]);
	AMMU_LOG_VERBO("stable DRAM_page_array_mask 4~16G = 0x%08x, enable num = 0x%08x\n",
		sTable_ptr->stable_info.DRAM_page_array_mask[1],
		sTable_ptr->stable_info.DRAM_page_array_en_num[1]);
	AMMU_LOG_VERBO("stable EXT_SLB_addr = 0x%08x, RSV_S (start, page) = (%u, %u)\n",
		sTable_ptr->stable_info.EXT_SLB_addr,
		sTable_ptr->stable_info.RSV_S_SLB_page_array_start,
		sTable_ptr->stable_info.RSV_S_SLB_page);

	*tbl_kva = (void *) (&sTable_ptr->stable_info);
	*size = sizeof(struct ammu_stable_info);
	mutex_unlock(&sTable_ptr->stable_lock);

out:
	ammu_trace_end();
	return ret;
}

/* free session table by session */
static int session_table_free_inner(uint64_t session)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_MTK_SLBC)
	int ret_slbc = 0;
#endif
	struct apummu_session_tbl *sTable_ptr;

	ammu_trace_begin("APUMMU: free session table");

	if (g_adv == NULL) {
		AMMU_LOG_ERR("Invalid apummu_device\n");
		ret = -EINVAL;
		goto out;
	}

	sTable_ptr = session_table_find(session);
	if (!sTable_ptr) {
		ret = -EINVAL;
		AMMU_LOG_ERR("free session table FAILED!!!, session table 0x%llx not found\n",
				session);
		goto out;
	}

	mutex_lock(&g_ammu_table_set.gtable_lock);
	mutex_lock(&sTable_ptr->stable_lock);
	if (!g_adv->plat.alloc_DRAM_FB_in_session_create) {
		g_ammu_table_set.subcmd_refcnt -= sTable_ptr->subcmd_num_for_DRAM_FB;
		AMMU_LOG_VERBO("--- g_ammu_table_set.subcmd_refcnt = %u\n", g_ammu_table_set.subcmd_refcnt);
	}
	list_del(&sTable_ptr->list);
	mutex_unlock(&sTable_ptr->stable_lock);
	if (g_adv->plat.is_ASE_support)
		ammu_SSID_free(sTable_ptr);
	kvfree(sTable_ptr);

	g_ammu_table_set.session_tbl_cnt -= 1;
	if (g_ammu_table_set.session_tbl_cnt == g_adv->plat.reserved_session_num) {
		mutex_unlock(&g_ammu_table_set.gtable_lock);
		free_memory();
		mutex_lock(&g_ammu_table_set.gtable_lock);
#if IS_ENABLED(CONFIG_MTK_SLBC)
		/*slc release flow*/
		ret_slbc = slbc_invalidate(ID_NPU, gid);
		if (ret_slbc)
			AMMU_LOG_INFO("slc invalidate fail");
		ret_slbc = slbc_gid_release(ID_NPU, gid);
		if (ret_slbc)
			AMMU_LOG_INFO("slc release fail");
		vfree(ammu_slbc_gid_data);
#endif
	}
	mutex_unlock(&g_ammu_table_set.gtable_lock);
out:
	ammu_trace_end();
	return ret;
}

int session_table_free(uint64_t session)
{
	int ret;

	mutex_lock(&session_lock);
	ret = session_table_free_inner(session);
	mutex_unlock(&session_lock);

	return ret;
}

void dump_session_table_set(void)
{
	struct list_head *list_ptr;
	struct apummu_session_tbl *sTable_ptr;
	uint32_t i = 0;

	AMMU_LOG_DBG("== APUMMU dump session table Start ==\n");
	AMMU_LOG_DBG("Total stable cnt = %u\n", g_ammu_table_set.session_tbl_cnt);
	AMMU_LOG_DBG("----------------------------------\n");

	mutex_lock(&g_ammu_table_set.gtable_lock);
	list_for_each(list_ptr, &g_ammu_table_set.g_stable_head) {
		sTable_ptr = list_entry(list_ptr, struct apummu_session_tbl, list);

		mutex_lock(&sTable_ptr->stable_lock);
		AMMU_LOG_DBG("== dump session table %u info ==\n", i++);
		AMMU_LOG_DBG("session              = 0x%llx\n",
			sTable_ptr->session);
		AMMU_LOG_DBG("mem_mask             = 0x%x\n",
			sTable_ptr->stable_info.mem_mask);
		AMMU_LOG_DBG("DRAM_page_array_mask = 0x%x 0x%x\n",
			sTable_ptr->stable_info.DRAM_page_array_mask[0],
			sTable_ptr->stable_info.DRAM_page_array_mask[1]);
		AMMU_LOG_DBG("DRAM_page_array_en_num = %u, %u\n",
			sTable_ptr->stable_info.DRAM_page_array_en_num[0],
			sTable_ptr->stable_info.DRAM_page_array_en_num[1]);
		AMMU_LOG_DBG("EXT_SLB_addr = 0x%x, RSV_S_SLB PA start = %u, page = %u\n",
			sTable_ptr->stable_info.EXT_SLB_addr,
			sTable_ptr->stable_info.RSV_S_SLB_page_array_start,
			sTable_ptr->stable_info.RSV_S_SLB_page);
		mutex_unlock(&sTable_ptr->stable_lock);
	}
	mutex_unlock(&g_ammu_table_set.gtable_lock);

	AMMU_LOG_DBG("== APUMMU dump session table End ==\n");
}

int ammu_session_table_add_SLB(uint64_t session, uint32_t type)
{
	int ret = 0;
	struct apummu_session_tbl *sTable_ptr;

	if (g_adv == NULL) {
		AMMU_LOG_ERR("Invalid apummu_device\n");
		ret = -EINVAL;
		goto out;
	}

	sTable_ptr = session_table_find(session);
	if (!sTable_ptr) {
		ret = -EINVAL;
		AMMU_LOG_ERR("Add SLB to stable FAILED!!!, session table 0x%llx not found\n",
				session);
		goto out;
	}

	if (type == APUMMU_MEM_TYPE_EXT) {
		if (!g_adv->plat.external_SLB_cnt) {
			ret = -ENOMEM;
			AMMU_LOG_ERR("External SLB is not alloced\n");
			goto out;
		}

		mutex_lock(&sTable_ptr->stable_lock);
		sTable_ptr->stable_info.EXT_SLB_addr = g_adv->rsc.external_SLB.iova;
		sTable_ptr->stable_info.mem_mask |= (1 << SLB_EXT);
		mutex_unlock(&sTable_ptr->stable_lock);
	} else if (type == APUMMU_MEM_TYPE_RSV_S) {
		if (!g_adv->plat.internal_SLB_cnt) {
			ret = -ENOMEM;
			AMMU_LOG_ERR("Internal SLB is not alloced\n");
			goto out;
		}

		mutex_lock(&sTable_ptr->stable_lock);
		sTable_ptr->stable_info.RSV_S_SLB_page_array_start =
			(g_adv->rsc.internal_SLB.iova - g_adv->remote.SLB_base_addr) >> 19;
		sTable_ptr->stable_info.RSV_S_SLB_page =
			g_adv->rsc.internal_SLB.size >> 19; // / 512K
		sTable_ptr->stable_info.mem_mask |= (1 << SLB_RSV_S);
		mutex_unlock(&sTable_ptr->stable_lock);
	} else {
		AMMU_LOG_ERR("Invalid apu memory type\n");
		ret = -EINVAL;
		goto out;
	}

out:
	return ret;
}

static int ammu_remove_stable_SLB_status(struct apummu_session_tbl *sTable_ptr, uint32_t type)
{
	int ret = 0;

	mutex_lock(&sTable_ptr->stable_lock);
	if (type == APUMMU_MEM_TYPE_EXT) {
		sTable_ptr->stable_info.EXT_SLB_addr = 0;
		sTable_ptr->stable_info.mem_mask &= ~(1 << SLB_EXT);
	} else if (type == APUMMU_MEM_TYPE_RSV_S) {
		sTable_ptr->stable_info.RSV_S_SLB_page_array_start = 0;
		sTable_ptr->stable_info.RSV_S_SLB_page = 0;
		sTable_ptr->stable_info.mem_mask &= ~(1 << SLB_RSV_S);
	} else {
		AMMU_LOG_ERR("Invalid apu memory type %u\n", type);
		ret = -EINVAL;
		goto out;
	}

out:
	mutex_unlock(&sTable_ptr->stable_lock);
	return ret;
}


int apummu_stable_buffer_remove(uint64_t session, uint64_t device_va, uint64_t buf_size)
{
	int ret = 0;
	struct apummu_session_tbl *sTable_ptr;
	uint32_t SLB_type, cross_page_array_num = 0;
	uint8_t mask_idx;
	bool is_36bit;

	if (device_va < 0x20000000) {
		if (((device_va >= g_adv->remote.vlm_addr) &&
			(device_va <= (g_adv->remote.vlm_addr + g_adv->remote.vlm_size))))
			goto out;
		else if ((device_va >= g_adv->remote.SLB_base_addr) &&
			((device_va < (g_adv->remote.SLB_base_addr + g_adv->remote.SLB_size)))) {
			SLB_type = (device_va == g_adv->rsc.internal_SLB.iova)
				? APUMMU_MEM_TYPE_RSV_S
				: APUMMU_MEM_TYPE_EXT;

			ret = ammu_session_table_remove_SLB(session, SLB_type);
			if (ret) {
				AMMU_LOG_ERR("Remove SLB buffer fail!!\n");
				goto out;
			}

			goto out;
		} else {
			AMMU_LOG_ERR("Invalid input VA 0x%llx\n", device_va);
			ret = -EINVAL;
			goto out;
		}
	}

	sTable_ptr = session_table_find(session);
	if (!sTable_ptr) {
		AMMU_LOG_ERR("Session table NOT exist!!!(0x%llx)\n", session);
		ret = -ENOMEM;
		goto out;
	}

	if (!buf_size || device_va < 0x40000000)
		goto out;

	is_36bit = device_va & (0xf00000000);

	mutex_lock(&sTable_ptr->stable_lock);
	if (is_36bit) {
		uint32_t page_sft = g_adv->plat.address_bits == 36 ? 31 : 29;

		cross_page_array_num = (((device_va + buf_size - 1) >> page_sft)
							- ((device_va) >> page_sft));
		do {
			/* >> 29 = 512M / 0x20000000 */
			/* >> 31 =   2G / 0x80000000 */
			mask_idx = (((device_va - 0x100000000ull) >> page_sft)
						+ cross_page_array_num) & (0x1f);

			sTable_ptr->DRAM_4_16G_mask_cnter[mask_idx] -= 1;
			if (sTable_ptr->DRAM_4_16G_mask_cnter[mask_idx] == 0) {
				sTable_ptr->stable_info.DRAM_page_array_mask[1] &=
					~(1 << mask_idx);
			}
		} while (cross_page_array_num--);

		if (sTable_ptr->stable_info.DRAM_page_array_mask[1] == 0)
			sTable_ptr->stable_info.mem_mask &= ~(1 << DRAM_4_16G);
	} else {
		cross_page_array_num =
			(((device_va + buf_size - 1) / (0x8000000)) - ((device_va) / (0x8000000)));
		do {
			/* - (0x40000000) because mapping start from 1G */
			/* >> 27 = 128M / 0x20000000 */
			mask_idx = (((device_va - (0x40000000)) >> 27)
						+ cross_page_array_num) & (0x1f);

			sTable_ptr->DRAM_1_4G_mask_cnter[mask_idx] -= 1;
			if (sTable_ptr->DRAM_1_4G_mask_cnter[mask_idx] == 0) {
				sTable_ptr->stable_info.DRAM_page_array_mask[0] &=
					~(1 << mask_idx);
			}
		} while (cross_page_array_num--);

		if (sTable_ptr->stable_info.DRAM_page_array_mask[0] == 0)
			sTable_ptr->stable_info.mem_mask &= ~(1 << DRAM_1_4G);
	}
	count_page_array_en_num(sTable_ptr);
	mutex_unlock(&sTable_ptr->stable_lock);

out:
	return ret;
}

int ammu_session_table_remove_SLB(uint64_t session, uint32_t type)
{
	int ret = 0;
	struct apummu_session_tbl *sTable_ptr;

	if (g_adv == NULL) {
		AMMU_LOG_ERR("Invalid apummu_device\n");
		ret = -EINVAL;
		goto out;
	}

	sTable_ptr = session_table_find(session);
	if (!sTable_ptr) {
		ret = -EINVAL;
		AMMU_LOG_ERR("Remove SLB to stable FAILED!!!, session table 0x%llx not found\n",
				session);
		goto out;
	}

	ret = ammu_remove_stable_SLB_status(sTable_ptr, type);

out:
	return ret;
}

void ammu_session_table_check_SLB(uint32_t type)
{
	int ret = 0;
	struct list_head *list_ptr;
	struct apummu_session_tbl *sTable_ptr;

	mutex_lock(&g_ammu_table_set.gtable_lock);
	list_for_each(list_ptr, &g_ammu_table_set.g_stable_head) {
		sTable_ptr = list_entry(list_ptr, struct apummu_session_tbl, list);

		if (type == APUMMU_MEM_TYPE_EXT) {
			if (sTable_ptr->stable_info.EXT_SLB_addr != 0) {
				ret = ammu_remove_stable_SLB_status(sTable_ptr, APUMMU_MEM_TYPE_EXT);
				if (ret)
					AMMU_LOG_ERR("ammu_remove_stable_SLB_status fail\n");
			}
		} else if (type == APUMMU_MEM_TYPE_RSV_S) {
			if (sTable_ptr->stable_info.RSV_S_SLB_page_array_start != 0) {
				ret = ammu_remove_stable_SLB_status(sTable_ptr, APUMMU_MEM_TYPE_RSV_S);
				if (ret)
					AMMU_LOG_ERR("ammu_remove_stable_SLB_status fail\n");
			}
		}
	}
	mutex_unlock(&g_ammu_table_set.gtable_lock);
}

int get_session_ssid(uint64_t session, uint32_t *ssid)
{
	struct apummu_session_tbl *sTable_ptr;
	int ret = 0;

	/* check if session table exist by session */
	sTable_ptr = session_table_find(session);
	if (!sTable_ptr) {
		ret = -ENOMEM;
		goto exit;
	}

	*ssid = sTable_ptr->stable_info.SMMU_SSID;
exit:
	return ret;
}

static uint32_t ammu_appendix_cb_size(uint32_t num_subcmds)
{
	return sizeof(struct ammu_stable_info);
}

static int ammu_appendix_cb_process(enum apu_appendix_cb_type type,
	struct apusys_cmd_info *cmd_info, void *va, uint32_t size)
{
	uint32_t szof;
	int ret = 0;

	if (cmd_info == NULL) {
		AMMU_LOG_ERR("Invalid cmd_info\n");
		return -EINVAL;
	}

	/* check argument */
	if (!size || va == NULL || !cmd_info->num_subcmds) {
		AMMU_LOG_ERR("Invalid size: %u, va: %p, num_subcmds: %u\n",
			size, va, cmd_info->num_subcmds);
		return -EINVAL;
	}

	/* check size */
	szof = sizeof(struct ammu_stable_info);
	if (size != szof) {
		AMMU_LOG_ERR("Invalid size: %u, sizeof stable: %u\n",
			size, szof);
		return -EINVAL;
	}

	switch (type) {
	case APU_APPENDIX_CB_CREATE:
		break;
	case APU_APPENDIX_CB_PREPROCESS: {
		void *tbl_kva;
		uint32_t tbl_size;

		ret = get_session_table(cmd_info->session_id, &tbl_kva, &tbl_size);
		if (ret) {
			AMMU_LOG_ERR("get_session_table fail: %d\n", ret);
			break;
		}
		memcpy(va, tbl_kva, size);
		break;
	}
	case APU_APPENDIX_CB_POSTPROCESS:
		break;
	case APU_APPENDIX_CB_POSTPROCESS_LATE:
		break;
	case APU_APPENDIX_CB_DELETE:
		break;
	default:
		break;
	}

	return ret;
}

/* Init lust head, lock */
int apummu_mgt_init(void)
{
	int ret;

	ret = apusys_request_cmdbuf_appendix(APU_APPENDIX_CB_OWNER_AMMU, ammu_appendix_cb_size, ammu_appendix_cb_process);
	if (ret) {
		AMMU_LOG_ERR("apusys_request_cmdbuf_appendix fail: %d\n", ret);
		goto exit;
	}

	g_ammu_table_set.is_VLM_info_IPI_sent = false;
	g_ammu_table_set.is_SLB_set = false;
	g_ammu_table_set.is_work_canceled = true;
	g_ammu_table_set.is_SLB_alloc = false;
	g_ammu_table_set.session_tbl_cnt = 0;
	INIT_LIST_HEAD(&g_ammu_table_set.g_stable_head);
	mutex_init(&g_ammu_table_set.gtable_lock);
	mutex_init(&g_ammu_table_set.DRAM_FB_lock);

	INIT_DELAYED_WORK(&DRAM_free_work, ammu_DRAM_free_work);
	ammu_workq = alloc_ordered_workqueue("ammu_dram_free", WQ_MEM_RECLAIM);
	bitmap_fill(ssid_bitmap, AMMU_SSID_MAX);
exit:
	return ret;
}

/* apummu_mgt_destroy session table set */
void apummu_mgt_destroy(void)
{
	struct list_head *list_ptr1, *list_ptr2;
	struct apummu_session_tbl *sTable_ptr; // stable stand for session table

	mutex_lock(&g_ammu_table_set.gtable_lock);
	list_for_each_safe(list_ptr1, list_ptr2, &g_ammu_table_set.g_stable_head) {
		sTable_ptr = list_entry(list_ptr1, struct apummu_session_tbl, list);
		list_del(&sTable_ptr->list);
		kvfree(sTable_ptr);
		sTable_ptr = NULL;
		AMMU_LOG_VERBO("kref put\n");
		g_ammu_table_set.session_tbl_cnt -= 1;
	}
	mutex_unlock(&g_ammu_table_set.gtable_lock);

	free_memory();

	/* Flush free DRAM job */
	flush_delayed_work(&DRAM_free_work);
}
