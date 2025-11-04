// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <asm/io.h>
#include <asm/kvm_pkvm_module.h>
#include <nvhe/spinlock.h>
#include <include/export.h>
#include "include/hypmmu.h"

#define DEBUG_HAL 0
#define ACTIVE 1
#define UNIT_TEST 0

#define MPU_ALIGNMENT_4KB          (0x00001000ULL)
#define MPU_ALIGNMENT_16KB         (0x00004000ULL)
#define MPU_ALIGNMENT_64KB         (0x00010000ULL)
#define MPU_ALIGNMENT_256KB        (0x00040000ULL)
#define MPU_ALIGNMENT_512KB        (0x00080000ULL)
#define MPU_ALIGNMENT_1MB          (0x00100000ULL)
#define MPU_ALIGNMENT_2MB          (0x00200000ULL)
#define MPU_ALIGNMENT_4MB          (0x00400000ULL)
#define MPU_SHIFT_4KB              (12)
#define MPU_SHIFT_16KB             (14)
#define MPU_SHIFT_64KB             (16)
#define MPU_SHIFT_256KB            (18)
#define MPU_SHIFT_512KB            (19)
#define MPU_SHIFT_1MB              (20)
#define MPU_SHIFT_2MB              (21)
#define MPU_SHIFT_4MB              (22)

#define MPU_TABLE_SIZE             (MPU_ALIGNMENT_4MB)
#define MPU_TABLE_NUM_PER_BYTE     (2)
#define MPU_PAGE_SIZE              (MPU_ALIGNMENT_4KB)
#define MPU_PAGE_SHIFT             (MPU_SHIFT_4KB)

#define MPU_AREA_SIZE_4GB          (0x100000000ULL)
#define MPU_AREA_SIZE_8GB          (0x200000000ULL)
#define MPU_AREA_SIZE_16GB         (0x400000000ULL)
#define MPU_AREA_SIZE_32GB         (0x800000000ULL)

#define MPU_AREA_0_START           (0x0)
#define MPU_AREA_0_END             (MPU_AREA_SIZE_16GB)
#define MPU_AREA_1_START           (MPU_AREA_SIZE_16GB)
#define MPU_AREA_1_END             (MPU_AREA_SIZE_32GB)

#define MPU_BYP_TABLE_ENTRY_SIZE   (MPU_ALIGNMENT_2MB)
#define MPU_BYP_TABLE_ENTRY_SHIFT  (MPU_SHIFT_2MB)
#define MPU_BYP_TABLE_REG_NUM      (256)  /* 8192 entries * 1 bit / 32 bits = 256 unsigned int */
#define MPU_BYP_TABLE_ENTRY_NUM    (8192) /* 16GB / 2MB = 8192 entries */

/**************************************************
 * IMPORTANT:
 * This section must be aligned with LK gpueb_plat.h
 **************************************************/
#define GPR_BASE_ADDR(x)           (g_GPUEB_GPR_BASE + (x * g_SRAM_GPR_SIZE_4B))

#define GPUEB_GPR6                 GPR_BASE_ADDR(6)
#define GPUEB_GPR11                GPR_BASE_ADDR(11)
#define GPUEB_GPR12                GPR_BASE_ADDR(12)

#define GPUEB_MBOX_IPI_GPUEB_STA   (g_MBOX0_SEND)
#define GPUEB_MBOX_IPI_GPUEB_SET   (g_MBOX0_SET)
/**************************************************/

/**************************************************
 * Register Manipulations
 **************************************************/
#define READ_REGISTER_UINT32(reg) (readl(reg))
#define WRITE_REGISTER_UINT32(reg, val) (writel(val, reg))
#define INREG32(x)          READ_REGISTER_UINT32((unsigned int *)(x))
#define OUTREG32(x, y)      WRITE_REGISTER_UINT32((unsigned int *)(x), (unsigned int )(y))
#define SETREG32(x, y)      OUTREG32(x, INREG32(x)|(y))
#define DRV_Reg32(addr)             INREG32(addr)
#define DRV_WriteReg32(addr, data)  OUTREG32(addr, data)
#define DRV_SetReg32(addr, data)    SETREG32(addr, data)
/**************************************************/

/**************************************************
 * IMPORTANT:
 * This section must be aligned with GPUEB gpumpu_ipi.h
 **************************************************/
enum gpu_mpu_ipi_send_cmd {
	CMD_INIT_PAGE_TABLE = 2197,
	CMD_UPDATE_PAGE_TABLE,
	CMD_UPDATE_PAGE_TABLE_V2,
	CMD_UPDATE_PAGE_TABLE_V2_1,
};

/*
 * MPU page table update status
 */
enum mpu_page_table_update_status {
	MPU_UPDATE_SUCCESS,
	MPU_UPDATE_WAITING,
	MPU_UPDATE_FAIL,
};

struct gpu_mpu_shared_data {
	union {
		unsigned int mpu_info;
		unsigned int bypass;
	};
	union {
		unsigned int phys_addr;
		unsigned int entry_start;
	};
	union {
		unsigned int size;
		unsigned int num;
	};
};

struct gpu_mpu_ipi_send_data {
	unsigned int cmd;
	unsigned int mpu_info;
	union {
		struct {
			unsigned long long phys_base;
			unsigned long long size;
		} mpu_table;
		struct {
			unsigned long long phys_addr;
			unsigned long long size;
		} mpu_scatterlist_v1;
		struct {
			unsigned int mpu_info_num;
		} mpu_scatterlist_v2;
	} u;
};
/**************************************************/

/*
 * MPU info
 * NSR = non-secure range
 * KPB = kernel protect bypass
 * using the sam buffer in DRAM
 * MPU only support #12 virtual secure range
 */
enum {
	/*
	 * MPU_info invalid(legacy mode and ignore fault),
	 * permission check based on SR_index and KPB will be ignored.
	 */
	MPU_INFO_PA_INVALID,
	MPU_INFO_NSR_KPB_0,     /* NSR with KPB = 0 */
	MPU_INFO_NSR_KPB_1,     /* NSR with KPB = 1 */
	MPU_INFO_ACCESS_DENIED, /* access denied */
	MPU_INFO_VSR4,          /* virtual secure range 4 */
	MPU_INFO_VSR5,
	MPU_INFO_VSR6,
	MPU_INFO_VSR7,
	MPU_INFO_VSR8,
	MPU_INFO_VSR9,
	MPU_INFO_VSR10,
	MPU_INFO_VSR11,
	MPU_INFO_VSR12,
	MPU_INFO_VSR13,
	MPU_INFO_VSR14,
	MPU_INFO_VSR15,
	MPU_INFO_MAX,
};

enum gpu_mpu_gpueb_reg_name {
	NAME_SRAM_BASE,
	NAME_GPR_TBL_ADDR,
	NAME_GPR_MSG_ADDR,
	NAME_GPR_ACK,
	NAME_MBOX_IPI_SEND_DATA,
	NAME_MBOX_IPI_GPUEB_STA,
	NAME_MBOX_IPI_GPUEB_SET,
	NAME_GPU_MPU_TABLE_BASE,
	NAME_REG_NAME_MAX,
};

struct gpu_mpu_addr {
	u64 pa;
	u64 va;
	const char *name;
};

static struct gpu_mpu_addr g_gpueb_reg[] = {
	{0, 0, "GPUEB_SRAM_BASE"},
	{0, 0, "GPUEB_GPR_TBL_ADDR"},       /* PA of GPU_MPU_TABLE_BASE */
	{0, 0, "GPUEB_GPR_MSG_ADDR"},       /* PA of GPUEB_MBOX_IPI_SEND_DATA */
	{0, 0, "GPUEB_GPR_ACK"},            /* GPUEB ACK reg */
	{0, 0, "GPUEB_MBOX_IPI_SEND_DATA"}, /* APMCU to GPUEB send data addr */
	{0, 0, "GPUEB_MBOX_IPI_GPUEB_STA"}, /* APMCU to GPUEB interrupt status */
	{0, 0, "GPUEB_MBOX_IPI_GPUEB_SET"}, /* APMCU to GPUEB interrupt set */
	{0, 0, "GPU_MPU_TABLE_BASE"},
};

static unsigned int g_gpu_mpu_support;
static unsigned int g_GPUEB_MBOX_IPI_ID; /* must be aligned with dts */
static unsigned int g_GPUEB_MBOX_IPI_SEND_DATA_SIZE; /* must be aligned with dts */
static unsigned int g_GPUEB_SRAM_BASE;
static unsigned int g_GPUEB_GPR_BASE;
static unsigned int g_GPUEB_SRAM_SIZE;
static unsigned int g_SRAM_GPR_SIZE_4B = 0x4U;
static unsigned int g_MBOX_SLOT_SIZE_4B;
static unsigned int g_MBOX0_SET;
static unsigned int g_MBOX0_SEND;
static unsigned int g_GPU_MPU_DATA_START_OFFSET;
static unsigned int g_GPU_MPU_DATA_OFFSET;
static unsigned int g_GPU_MPU_DATA_SIZE;
static struct gpu_mpu_ipi_send_data g_gpumpu_send_data;
static DEFINE_HYP_SPINLOCK(g_shared_sram_lock);
static unsigned short g_mpu_byp_table_cnt[2 * MPU_BYP_TABLE_ENTRY_NUM]; /* two MPU areas */

static u64 gpu_mpu_map_io(u64 reg_base, u32 size, const char *reg_name);
static u64 gpu_mpu_map_nc(u64 reg_base, u32 size, const char *reg_name);
static int ipi_communication(void);
static bool check_bypass(unsigned short entry);
static void pmm_pre_init(void);
static int pmm_prepare(void);
static int pmm_secure(u64 paddr, u32 size, u8 pmm_attr);
static int pmm_unsecure(u64 paddr, u32 size, u8 pmm_attr);
static int pmm_protect_v2_1(u64 paddr, u8 order, u8 pmm_attr, bool secure);
static int pmm_secure_v2_1(u64 paddr, u8 order, u8 pmm_attr);
static int pmm_unsecure_v2_1(u64 paddr, u8 order, u8 pmm_attr);
static int pmm_sync(void);
static int pmm_defragment(void);

static u64 gpu_mpu_map_io(u64 reg_base, u32 size, const char *reg_name)
{
	int err;
	unsigned long vptr = 0;

	err = mod_ops->create_private_mapping((phys_addr_t)reg_base, (size_t)size,
			PAGE_HYP_DEVICE, &vptr);
	if (err) {
		if (reg_name)
			MOD_PUTS(reg_name);
		MOD_PUTS3("[ERROR] map failed", reg_base, size, err);
		return 0;
	}
#if (DEBUG_HAL)
	if (reg_name)
		MOD_PUTS(reg_name);
	MOD_PUTS3("map successfully", reg_base, size, vptr);
#endif

	return vptr;
}

static u64 gpu_mpu_map_nc(u64 reg_base, u32 size, const char *reg_name)
{
	int err;
	unsigned long vptr = 0;

	err = mod_ops->create_private_mapping((phys_addr_t)reg_base, (size_t)size,
			PAGE_HYP | KVM_PGTABLE_PROT_NORMAL_NC, &vptr);
	if (err) {
		if (reg_name)
			MOD_PUTS(reg_name);
		MOD_PUTS3("[ERROR] map failed", reg_base, size, err);
		return 0;
	}
#if (DEBUG_HAL)
	if (reg_name)
		MOD_PUTS(reg_name);
	MOD_PUTS3("map successfully", reg_base, size, vptr);
#endif

	return vptr;
}

static int ipi_communication(void)
{
#if ACTIVE
	unsigned int ack;

	while (DRV_Reg32(g_gpueb_reg[NAME_MBOX_IPI_GPUEB_STA].va) & (1 << g_GPUEB_MBOX_IPI_ID)) {
#if (DEBUG_HAL)
		MOD_PUTS("wait for previous GPUEB MBOX IPI done...");
#endif
	}
	/* IPI msg */
	for (unsigned int i = 0; i < sizeof(struct gpu_mpu_ipi_send_data) / sizeof(unsigned int); i++)
		DRV_WriteReg32(g_gpueb_reg[NAME_MBOX_IPI_SEND_DATA].va + i * sizeof(unsigned int),
				((unsigned int *)(&g_gpumpu_send_data))[i]);
#if (DEBUG_HAL)
	MOD_PUTS("prepare IPI msg done");
#endif
	/* set RSV to wait */
	DRV_WriteReg32(g_gpueb_reg[NAME_GPR_ACK].va, MPU_UPDATE_WAITING);
#if (DEBUG_HAL)
	MOD_PUTS("set rsv done");
#endif
	/* inform GPUEB to read data */
	DRV_SetReg32(g_gpueb_reg[NAME_MBOX_IPI_GPUEB_SET].va, 1 << g_GPUEB_MBOX_IPI_ID);
#if (DEBUG_HAL)
	MOD_PUTS2("GPUEB_MBOX_IPI_GPUEB_STA", g_gpueb_reg[NAME_MBOX_IPI_GPUEB_STA].va,
			DRV_Reg32(g_gpueb_reg[NAME_MBOX_IPI_GPUEB_STA].va));
#endif
	while ((ack = DRV_Reg32(g_gpueb_reg[NAME_GPR_ACK].va)) == MPU_UPDATE_WAITING) {
#if (DEBUG_HAL)
		MOD_PUTS("wait for GPUEB update table done...");
#endif
	}
#if (DEBUG_HAL)
	MOD_PUTS1("ack", ack);
#endif
	if (ack == MPU_UPDATE_FAIL)
		MOD_PUTS("[ERROR] GPUEB update table fail!");
#endif

	/* reset offset, mpu_info_num */
	g_GPU_MPU_DATA_OFFSET = g_GPU_MPU_DATA_START_OFFSET;
	g_gpumpu_send_data.u.mpu_scatterlist_v2.mpu_info_num = 0;

	return 0;
}

static bool check_bypass(unsigned short entry)
{
	static unsigned short max_page_num_per_entry = MPU_BYP_TABLE_ENTRY_SIZE >> MPU_PAGE_SHIFT;

	if (g_mpu_byp_table_cnt[entry] > max_page_num_per_entry) {
		MOD_PUTS2("[WARN] {entry g_mpu_byp_table_cnt[entry]}", entry, g_mpu_byp_table_cnt[entry]);
		return false;
	} else if (g_mpu_byp_table_cnt[entry]) {
		return false;
	} else {
		return true;
	}
}

static void pmm_pre_init(void)
{
	MOD_PUTS("gpu-mpu pre_init");

	if (!g_gpu_mpu_support) {
		MOD_PUTS("not support gpu mpu");
		return;
	}

	if (sizeof(struct gpu_mpu_ipi_send_data) > g_GPUEB_MBOX_IPI_SEND_DATA_SIZE * g_MBOX_SLOT_SIZE_4B) {
		MOD_PUTS2("[WARN] send data size greater than dts send_size, force to disable gpu mpu",
				sizeof(struct gpu_mpu_ipi_send_data),
				g_GPUEB_MBOX_IPI_SEND_DATA_SIZE * g_MBOX_SLOT_SIZE_4B);
		g_gpu_mpu_support = 0;
		return;
	}

	g_gpueb_reg[NAME_SRAM_BASE].pa = g_GPUEB_SRAM_BASE;
	g_gpueb_reg[NAME_SRAM_BASE].va = gpu_mpu_map_io(g_gpueb_reg[NAME_SRAM_BASE].pa,
			g_GPUEB_SRAM_SIZE, g_gpueb_reg[NAME_SRAM_BASE].name);

	g_gpueb_reg[NAME_GPR_TBL_ADDR].pa = GPUEB_GPR6;
	g_gpueb_reg[NAME_GPR_TBL_ADDR].va =
			g_gpueb_reg[NAME_GPR_TBL_ADDR].pa - g_gpueb_reg[NAME_SRAM_BASE].pa +
			g_gpueb_reg[NAME_SRAM_BASE].va;

	g_gpueb_reg[NAME_GPR_MSG_ADDR].pa = GPUEB_GPR11;
	g_gpueb_reg[NAME_GPR_MSG_ADDR].va =
			g_gpueb_reg[NAME_GPR_MSG_ADDR].pa - g_gpueb_reg[NAME_SRAM_BASE].pa +
			g_gpueb_reg[NAME_SRAM_BASE].va;

	g_gpueb_reg[NAME_GPR_ACK].pa = GPUEB_GPR12;
	g_gpueb_reg[NAME_GPR_ACK].va =
			g_gpueb_reg[NAME_GPR_ACK].pa - g_gpueb_reg[NAME_SRAM_BASE].pa +
			g_gpueb_reg[NAME_SRAM_BASE].va;

	g_gpueb_reg[NAME_MBOX_IPI_GPUEB_STA].pa = GPUEB_MBOX_IPI_GPUEB_STA;
	g_gpueb_reg[NAME_MBOX_IPI_GPUEB_STA].va = gpu_mpu_map_io(g_gpueb_reg[NAME_MBOX_IPI_GPUEB_STA].pa,
			PAGE_SIZE, g_gpueb_reg[NAME_MBOX_IPI_GPUEB_STA].name);

	g_gpueb_reg[NAME_MBOX_IPI_GPUEB_SET].pa = GPUEB_MBOX_IPI_GPUEB_SET;
	g_gpueb_reg[NAME_MBOX_IPI_GPUEB_SET].va =
			g_gpueb_reg[NAME_MBOX_IPI_GPUEB_SET].pa - g_gpueb_reg[NAME_MBOX_IPI_GPUEB_STA].pa +
			g_gpueb_reg[NAME_MBOX_IPI_GPUEB_STA].va;

#if ACTIVE
	g_gpueb_reg[NAME_MBOX_IPI_SEND_DATA].pa = DRV_Reg32(g_gpueb_reg[NAME_GPR_MSG_ADDR].va);
	g_gpueb_reg[NAME_MBOX_IPI_SEND_DATA].va =
			g_gpueb_reg[NAME_MBOX_IPI_SEND_DATA].pa - g_gpueb_reg[NAME_SRAM_BASE].pa +
			g_gpueb_reg[NAME_SRAM_BASE].va;

	g_GPU_MPU_DATA_START_OFFSET = DRV_Reg32(g_gpueb_reg[NAME_GPR_ACK].va);

	g_gpueb_reg[NAME_GPU_MPU_TABLE_BASE].pa = DRV_Reg32(g_gpueb_reg[NAME_GPR_TBL_ADDR].va);
	g_gpueb_reg[NAME_GPU_MPU_TABLE_BASE].pa = g_gpueb_reg[NAME_GPU_MPU_TABLE_BASE].pa << MPU_PAGE_SHIFT;
	g_gpueb_reg[NAME_GPU_MPU_TABLE_BASE].va = gpu_mpu_map_nc(g_gpueb_reg[NAME_GPU_MPU_TABLE_BASE].pa,
			MPU_TABLE_SIZE, g_gpueb_reg[NAME_GPU_MPU_TABLE_BASE].name);
#endif

#if (DEBUG_HAL)
	for (int i = 0; i < NAME_REG_NAME_MAX; i++) {
		MOD_PUTS(g_gpueb_reg[i].name);
		MOD_PUTS2("{pa va}", g_gpueb_reg[i].pa, g_gpueb_reg[i].va);
	}
	MOD_PUTS1("g_GPU_MPU_DATA_START_OFFSET", g_GPU_MPU_DATA_START_OFFSET);
#endif

#if UNIT_TEST
	do {
		u8 ORDER_512KB = 7;
		u8 ORDER_1MB = 8;
		u8 ORDER_2MB = 9;
		u8 ORDER_4MB = 10;
		unsigned int IDX_512KB = 64;
		unsigned int IDX_1MB = 128;
		unsigned int IDX_2MB = 256;
		unsigned int IDX_3MB = 384;
		unsigned int IDX_4MB = 512;
		unsigned int IDX_5MB = 640;
		unsigned int IDX_6MB = 768;
		unsigned int IDX_32GB = 4194304;
		unsigned int IDX_START;
		unsigned int IDX_END;
		bool is_fail = false;
		unsigned char *table = (unsigned char *)g_gpueb_reg[NAME_GPU_MPU_TABLE_BASE].va;
		int ut_ret = 0;

		MOD_PUTS("Case 0");
		for (unsigned int i = 0; i < IDX_32GB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = 0; i < 2 * MPU_BYP_TABLE_ENTRY_NUM; i++) {
			if (g_mpu_byp_table_cnt[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (is_fail) {
			MOD_PUTS("Case 0 FAIL");
			break;
		}
		MOD_PUTS("Case 0 PASS");

		MOD_PUTS("Case 1");
		ut_ret = pmm_prepare();
		ut_ret = pmm_secure_v2_1(MPU_ALIGNMENT_1MB, ORDER_4MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		IDX_START = IDX_1MB;
		IDX_END = IDX_START + IDX_4MB;
		for (unsigned int i = 0; i < IDX_START; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_START; i < IDX_END; i++) {
			if (table[i] != 0x88) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_END; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (g_mpu_byp_table_cnt[0] != 256 ||
			g_mpu_byp_table_cnt[1] != 512 ||
			g_mpu_byp_table_cnt[2] != 256) {
			is_fail = true;
		}
		if (is_fail) {
			MOD_PUTS1("Case 1 FAIL", ut_ret);
			break;
		}
		ut_ret = pmm_prepare();
		ut_ret = pmm_unsecure_v2_1(MPU_ALIGNMENT_1MB, ORDER_4MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		for (unsigned int i = 0; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = 0; i < 3; i++) {
			if (g_mpu_byp_table_cnt[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (is_fail) {
			MOD_PUTS1("Case 1 FAIL", ut_ret);
			break;
		}
		MOD_PUTS("Case 1 PASS");

		MOD_PUTS("Case 2");
		ut_ret = pmm_prepare();
		ut_ret = pmm_secure_v2_1(MPU_ALIGNMENT_1MB, ORDER_2MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_secure_v2_1(MPU_ALIGNMENT_1MB + MPU_ALIGNMENT_2MB, ORDER_1MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		IDX_START = IDX_1MB;
		IDX_END = IDX_START + IDX_3MB;
		for (unsigned int i = 0; i < IDX_START; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_START; i < IDX_END; i++) {
			if (table[i] != 0x88) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_END; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (g_mpu_byp_table_cnt[0] != 256 ||
			g_mpu_byp_table_cnt[1] != 512 ||
			g_mpu_byp_table_cnt[2] != 0) {
			is_fail = true;
		}
		if (is_fail) {
			MOD_PUTS1("Case 2 FAIL", ut_ret);
			break;
		}
		ut_ret = pmm_prepare();
		ut_ret = pmm_unsecure_v2_1(MPU_ALIGNMENT_1MB, ORDER_2MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_unsecure_v2_1(MPU_ALIGNMENT_1MB + MPU_ALIGNMENT_2MB, ORDER_1MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		for (unsigned int i = 0; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = 0; i < 3; i++) {
			if (g_mpu_byp_table_cnt[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (is_fail) {
			MOD_PUTS1("Case 2 FAIL", ut_ret);
			break;
		}
		MOD_PUTS("Case 2 PASS");

		MOD_PUTS("Case 3");
		ut_ret = pmm_prepare();
		ut_ret = pmm_secure_v2_1(0x0, ORDER_1MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_secure_v2_1(0x0 + MPU_ALIGNMENT_1MB, ORDER_4MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		IDX_START = 0;
		IDX_END = IDX_START + IDX_5MB;
		for (unsigned int i = 0; i < IDX_START; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_START; i < IDX_END; i++) {
			if (table[i] != 0x88) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_END; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (g_mpu_byp_table_cnt[0] != 512 ||
			g_mpu_byp_table_cnt[1] != 512 ||
			g_mpu_byp_table_cnt[2] != 256) {
			is_fail = true;
		}
		if (is_fail) {
			MOD_PUTS1("Case 3 FAIL", ut_ret);
			break;
		}
		ut_ret = pmm_prepare();
		ut_ret = pmm_unsecure_v2_1(0x0, ORDER_1MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_unsecure_v2_1(0x0 + MPU_ALIGNMENT_1MB, ORDER_4MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		for (unsigned int i = 0; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = 0; i < 3; i++) {
			if (g_mpu_byp_table_cnt[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (is_fail) {
			MOD_PUTS1("Case 3 FAIL", ut_ret);
			break;
		}
		MOD_PUTS("Case 3 PASS");

		MOD_PUTS("Case 4");
		ut_ret = pmm_prepare();
		ut_ret = pmm_secure_v2_1(0x0, ORDER_4MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		IDX_START = 0;
		IDX_END = IDX_START + IDX_4MB;
		for (unsigned int i = 0; i < IDX_START; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_START; i < IDX_END; i++) {
			if (table[i] != 0x88) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_END; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (g_mpu_byp_table_cnt[0] != 512 ||
			g_mpu_byp_table_cnt[1] != 512 ||
			g_mpu_byp_table_cnt[2] != 0) {
			is_fail = true;
		}
		if (is_fail) {
			MOD_PUTS1("Case 4 FAIL", ut_ret);
			break;
		}
		ut_ret = pmm_prepare();
		ut_ret = pmm_unsecure_v2_1(0x0, ORDER_4MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		for (unsigned int i = 0; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = 0; i < 3; i++) {
			if (g_mpu_byp_table_cnt[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (is_fail) {
			MOD_PUTS1("Case 4 FAIL", ut_ret);
			break;
		}
		MOD_PUTS("Case 4 PASS");

		MOD_PUTS("Case 5");
		ut_ret = pmm_prepare();
		ut_ret = pmm_secure_v2_1(MPU_ALIGNMENT_1MB, ORDER_2MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		IDX_START = IDX_1MB;
		IDX_END = IDX_START + IDX_2MB;
		for (unsigned int i = 0; i < IDX_START; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_START; i < IDX_END; i++) {
			if (table[i] != 0x88) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_END; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (g_mpu_byp_table_cnt[0] != 256 ||
			g_mpu_byp_table_cnt[1] != 256 ||
			g_mpu_byp_table_cnt[2] != 0) {
			is_fail = true;
		}
		if (is_fail) {
			MOD_PUTS1("Case 5 FAIL", ut_ret);
			break;
		}
		ut_ret = pmm_prepare();
		ut_ret = pmm_unsecure_v2_1(MPU_ALIGNMENT_1MB, ORDER_2MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		for (unsigned int i = 0; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = 0; i < 3; i++) {
			if (g_mpu_byp_table_cnt[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (is_fail) {
			MOD_PUTS1("Case 5 FAIL", ut_ret);
			break;
		}
		MOD_PUTS("Case 5 PASS");

		MOD_PUTS("Case 6");
		ut_ret = pmm_prepare();
		ut_ret = pmm_secure_v2_1(MPU_ALIGNMENT_1MB, ORDER_1MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		IDX_START = IDX_1MB;
		IDX_END = IDX_START + IDX_1MB;
		for (unsigned int i = 0; i < IDX_START; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_START; i < IDX_END; i++) {
			if (table[i] != 0x88) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_END; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (g_mpu_byp_table_cnt[0] != 256 ||
			g_mpu_byp_table_cnt[1] != 0 ||
			g_mpu_byp_table_cnt[2] != 0) {
			is_fail = true;
		}
		if (is_fail) {
			MOD_PUTS1("Case 6 FAIL", ut_ret);
			break;
		}
		ut_ret = pmm_prepare();
		ut_ret = pmm_unsecure_v2_1(MPU_ALIGNMENT_1MB, ORDER_1MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		for (unsigned int i = 0; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = 0; i < 3; i++) {
			if (g_mpu_byp_table_cnt[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (is_fail) {
			MOD_PUTS1("Case 6 FAIL", ut_ret);
			break;
		}
		MOD_PUTS("Case 6 PASS");

		MOD_PUTS("Case 7");
		ut_ret = pmm_prepare();
		ut_ret = pmm_secure_v2_1(0x0, ORDER_1MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_secure_v2_1(0x0 + MPU_ALIGNMENT_1MB, ORDER_2MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		IDX_START = 0;
		IDX_END = IDX_START + IDX_3MB;
		for (unsigned int i = 0; i < IDX_START; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_START; i < IDX_END; i++) {
			if (table[i] != 0x88) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_END; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (g_mpu_byp_table_cnt[0] != 512 ||
			g_mpu_byp_table_cnt[1] != 256 ||
			g_mpu_byp_table_cnt[2] != 0) {
			is_fail = true;
		}
		if (is_fail) {
			MOD_PUTS1("Case 7 FAIL", ut_ret);
			break;
		}
		ut_ret = pmm_prepare();
		ut_ret = pmm_unsecure_v2_1(0x0, ORDER_1MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_unsecure_v2_1(0x0 + MPU_ALIGNMENT_1MB, ORDER_2MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		for (unsigned int i = 0; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = 0; i < 3; i++) {
			if (g_mpu_byp_table_cnt[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (is_fail) {
			MOD_PUTS1("Case 7 FAIL", ut_ret);
			break;
		}
		MOD_PUTS("Case 7 PASS");

		MOD_PUTS("Case 8");
		ut_ret = pmm_prepare();
		ut_ret = pmm_secure_v2_1(0x0, ORDER_2MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		IDX_START = 0;
		IDX_END = IDX_START + IDX_2MB;
		for (unsigned int i = 0; i < IDX_START; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_START; i < IDX_END; i++) {
			if (table[i] != 0x88) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_END; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (g_mpu_byp_table_cnt[0] != 512 ||
			g_mpu_byp_table_cnt[1] != 0 ||
			g_mpu_byp_table_cnt[2] != 0) {
			is_fail = true;
		}
		if (is_fail) {
			MOD_PUTS1("Case 8 FAIL", ut_ret);
			break;
		}
		ut_ret = pmm_prepare();
		ut_ret = pmm_unsecure_v2_1(0x0, ORDER_2MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		for (unsigned int i = 0; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = 0; i < 3; i++) {
			if (g_mpu_byp_table_cnt[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (is_fail) {
			MOD_PUTS1("Case 8 FAIL", ut_ret);
			break;
		}
		MOD_PUTS("Case 8 PASS");

		MOD_PUTS("Case 9");
		ut_ret = pmm_prepare();
		ut_ret = pmm_secure_v2_1(0x0, ORDER_1MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		IDX_START = 0;
		IDX_END = IDX_START + IDX_1MB;
		for (unsigned int i = 0; i < IDX_START; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_START; i < IDX_END; i++) {
			if (table[i] != 0x88) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_END; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (g_mpu_byp_table_cnt[0] != 256 ||
			g_mpu_byp_table_cnt[1] != 0 ||
			g_mpu_byp_table_cnt[2] != 0) {
			is_fail = true;
		}
		if (is_fail) {
			MOD_PUTS1("Case 9 FAIL", ut_ret);
			break;
		}
		ut_ret = pmm_prepare();
		ut_ret = pmm_unsecure_v2_1(0x0, ORDER_1MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		for (unsigned int i = 0; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = 0; i < 3; i++) {
			if (g_mpu_byp_table_cnt[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (is_fail) {
			MOD_PUTS1("Case 9 FAIL", ut_ret);
			break;
		}
		MOD_PUTS("Case 9 PASS");

		MOD_PUTS("Case 10");
		ut_ret = pmm_prepare();
		ut_ret = pmm_secure_v2_1(MPU_ALIGNMENT_512KB, ORDER_1MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		IDX_START = IDX_512KB;
		IDX_END = IDX_START + IDX_1MB;
		for (unsigned int i = 0; i < IDX_START; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_START; i < IDX_END; i++) {
			if (table[i] != 0x88) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_END; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (g_mpu_byp_table_cnt[0] != 256 ||
			g_mpu_byp_table_cnt[1] != 0 ||
			g_mpu_byp_table_cnt[2] != 0) {
			is_fail = true;
		}
		if (is_fail) {
			MOD_PUTS1("Case 10 FAIL", ut_ret);
			break;
		}
		ut_ret = pmm_prepare();
		ut_ret = pmm_unsecure_v2_1(MPU_ALIGNMENT_512KB, ORDER_1MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		for (unsigned int i = 0; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = 0; i < 3; i++) {
			if (g_mpu_byp_table_cnt[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (is_fail) {
			MOD_PUTS1("Case 10 FAIL", ut_ret);
			break;
		}
		MOD_PUTS("Case 10 PASS");

		MOD_PUTS("Case 11");
		ut_ret = pmm_prepare();
		ut_ret = pmm_secure_v2_1(MPU_ALIGNMENT_512KB, ORDER_1MB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		IDX_START = IDX_512KB;
		IDX_END = IDX_START + IDX_1MB;
		for (unsigned int i = 0; i < IDX_START; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_START; i < IDX_END; i++) {
			if (table[i] != 0x88) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_END; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (g_mpu_byp_table_cnt[0] != 256 ||
			g_mpu_byp_table_cnt[1] != 0 ||
			g_mpu_byp_table_cnt[2] != 0) {
			is_fail = true;
		}
		if (is_fail) {
			MOD_PUTS1("Case 11 FAIL", ut_ret);
			break;
		}
		ut_ret = pmm_prepare();
		ut_ret = pmm_unsecure_v2_1(MPU_ALIGNMENT_512KB, ORDER_512KB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		IDX_START = IDX_1MB;
		IDX_END = IDX_START + IDX_512KB;
		for (unsigned int i = 0; i < IDX_START; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_START; i < IDX_END; i++) {
			if (table[i] != 0x88) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = IDX_END; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (g_mpu_byp_table_cnt[0] != 128 ||
			g_mpu_byp_table_cnt[1] != 0 ||
			g_mpu_byp_table_cnt[2] != 0) {
			is_fail = true;
		}
		if (is_fail) {
			MOD_PUTS1("Case 11 FAIL", ut_ret);
			break;
		}
		ut_ret = pmm_prepare();
		ut_ret = pmm_unsecure_v2_1(MPU_ALIGNMENT_512KB + MPU_ALIGNMENT_512KB, ORDER_512KB, HYP_PMM_ATTR_SVP);
		ut_ret = pmm_sync();
		for (unsigned int i = 0; i < IDX_6MB; i++) {
			if (table[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		for (unsigned int i = 0; i < 3; i++) {
			if (g_mpu_byp_table_cnt[i] != 0x0) {
				is_fail = true;
				break;
			}
		}
		if (is_fail) {
			MOD_PUTS1("Case 11 FAIL", ut_ret);
			break;
		}
		MOD_PUTS("Case 11 PASS");
	} while (0);
#endif /* UNIT_TEST */

	return;
}

static int pmm_prepare(void)
{
#if (DEBUG_HAL)
	MOD_PUTS("gpu-mpu prepare");
#endif

	if (!g_gpu_mpu_support)
		return 0;

	hyp_spin_lock(&g_shared_sram_lock);
#if (DEBUG_HAL)
	MOD_PUTS("[LOCK] g_shared_sram_lock");
#endif

	/* Set initial values */
	g_GPU_MPU_DATA_OFFSET = g_GPU_MPU_DATA_START_OFFSET; /* record the n'th data */
	g_gpumpu_send_data.u.mpu_scatterlist_v2.mpu_info_num = 0; /* record the written number of data */

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

static int pmm_protect_v2_1(u64 paddr, u8 order, u8 pmm_attr, bool secure)
{
	int ret = 0;
	struct gpu_mpu_shared_data gpumpu_shared_data;
	u32 size = PAGE_SIZE << order;
	u8 mpu_info = secure ? hypmmu_get_srinfo(pmm_attr) : MPU_INFO_PA_INVALID;
	bool bypass;
	unsigned short byp_table_entry_start = 0;
	unsigned short byp_table_entry_end = 0;

#if (DEBUG_HAL)
	if (secure)
		MOD_PUTS6("gpu-mpu secure_v2_1", paddr, paddr + size, order, size, pmm_attr, mpu_info);
	else
		MOD_PUTS6("gpu-mpu unsecure_v2_1", paddr, paddr + size, order, size, pmm_attr, mpu_info);
#endif

	if (!g_gpu_mpu_support)
		return 0;

	if ((paddr % MPU_ALIGNMENT_4KB) || (size % MPU_ALIGNMENT_4KB)) {
		MOD_PUTS2("[ERROR] paddr or size is not 4KB aligned", paddr, size);
		return -1;
	}

	if (((s64)paddr     <  MPU_AREA_0_START) ||
		(paddr          >= MPU_AREA_1_END)   ||
		((paddr + size) <= MPU_AREA_0_START) ||
		((paddr + size) >  MPU_AREA_1_END)) {
		MOD_PUTS2("[ERROR] out of range", paddr, size);
		return -1;
	}

	if (mpu_info >= MPU_INFO_MAX) {
		MOD_PUTS1("[WARN] invalid mpu_info, force to MPU_INFO_ACCESS_DENIED", mpu_info);
		mpu_info = MPU_INFO_ACCESS_DENIED;
	}
	bypass = (mpu_info == MPU_INFO_PA_INVALID || mpu_info == MPU_INFO_NSR_KPB_0) ? true : false;

#if ACTIVE
	unsigned char *byte;
	bool begin_upper;
	unsigned int page_num;
	unsigned short start_padding, end_padding;

	/* Update MPU table and count secure pages per 2MB entry */
	byte = (unsigned char *)g_gpueb_reg[NAME_GPU_MPU_TABLE_BASE].va +
			((paddr >> MPU_PAGE_SHIFT) / MPU_TABLE_NUM_PER_BYTE);
	begin_upper = ((paddr >> MPU_PAGE_SHIFT) % MPU_TABLE_NUM_PER_BYTE) ? true : false;
	page_num = size >> MPU_PAGE_SHIFT;
#if (DEBUG_HAL)
	MOD_PUTS4("{byte begin_upper page_num bypass}", byte, begin_upper, page_num, bypass);
#endif

	for (unsigned int i = 0; i < page_num; i++) {
		bool set_upper = begin_upper ? (i % 2 ? false : true) : (i % 2 ? true : false);
		u8 mpu_info_old = set_upper ? ((*byte & 0xF0) >> 4) : (*byte & 0x0F);
		bool bypass_old = (mpu_info_old == MPU_INFO_PA_INVALID || mpu_info_old == MPU_INFO_NSR_KPB_0) ?
				true : false;
		unsigned short byp_table_entry =
				(unsigned short)((paddr + (i * MPU_PAGE_SIZE)) >> MPU_BYP_TABLE_ENTRY_SHIFT);

		if (bypass && !bypass_old)
			g_mpu_byp_table_cnt[byp_table_entry]--;
		else if (!bypass && bypass_old)
			g_mpu_byp_table_cnt[byp_table_entry]++;

		if (set_upper) {
			*byte = (*byte & 0x0F) | ((mpu_info << 4) & 0xF0);
			byte++;
		} else {
			*byte = (*byte & 0xF0) | (mpu_info & 0x0F);
		}
	}

	byp_table_entry_start = (unsigned short)(paddr >> MPU_BYP_TABLE_ENTRY_SHIFT);
	byp_table_entry_end   = (unsigned short)((paddr + size) >> MPU_BYP_TABLE_ENTRY_SHIFT);
	start_padding = (paddr % MPU_BYP_TABLE_ENTRY_SIZE) ? 1 : 0;
	end_padding = ((paddr + size) % MPU_BYP_TABLE_ENTRY_SIZE) ? 1 : 0;
	if (bypass) {
		if (start_padding) {
			if (!check_bypass(byp_table_entry_start))
				byp_table_entry_start += start_padding;
		}
		if (end_padding) {
			if (check_bypass(byp_table_entry_end))
				byp_table_entry_end += end_padding;
		}
	} else {
		byp_table_entry_end += end_padding;
	}
#endif

	gpumpu_shared_data.bypass = bypass;
	gpumpu_shared_data.entry_start = byp_table_entry_start;
	gpumpu_shared_data.num = (byp_table_entry_end > byp_table_entry_start) ?
			(byp_table_entry_end - byp_table_entry_start) : 0;
	g_gpumpu_send_data.cmd = CMD_UPDATE_PAGE_TABLE_V2_1;
	g_gpumpu_send_data.u.mpu_scatterlist_v2.mpu_info_num += 1; /* Numbers of shared data */
#if (DEBUG_HAL)
	MOD_PUTS5("{cmd mpu_info_num bypass entry_start num}",
			g_gpumpu_send_data.cmd,
			g_gpumpu_send_data.u.mpu_scatterlist_v2.mpu_info_num,
			gpumpu_shared_data.bypass,
			gpumpu_shared_data.entry_start,
			gpumpu_shared_data.num);
#endif

#if ACTIVE
	/* Write msg to SRAM */
	for (unsigned int i = 0; i < sizeof(struct gpu_mpu_shared_data) / sizeof(unsigned int); i++) {
		DRV_WriteReg32(g_gpueb_reg[NAME_SRAM_BASE].va + g_GPU_MPU_DATA_OFFSET + i * sizeof(unsigned int),
				((unsigned int *)(&gpumpu_shared_data))[i]);
	}
#endif
	/* update OFFSET for next data */
	g_GPU_MPU_DATA_OFFSET += sizeof(struct gpu_mpu_shared_data);

	/* if SRAM is full, call EB to update MPU table */
	if ((g_GPU_MPU_DATA_OFFSET + sizeof(struct gpu_mpu_shared_data)) >
		(g_GPU_MPU_DATA_START_OFFSET + g_GPU_MPU_DATA_SIZE)) {
#if (DEBUG_HAL)
		MOD_PUTS1("SRAM is full, sync first", g_GPU_MPU_DATA_SIZE);
#endif
		ret = ipi_communication();
	}

	return ret;
}

static int pmm_secure_v2_1(u64 paddr, u8 order, u8 pmm_attr)
{
	return pmm_protect_v2_1(paddr, order, pmm_attr, true);
}

static int pmm_unsecure_v2_1(u64 paddr, u8 order, u8 pmm_attr)
{
	return pmm_protect_v2_1(paddr, order, pmm_attr, false);
}

static int pmm_sync(void)
{
	int ret = 0;

#if (DEBUG_HAL)
	MOD_PUTS("gpu-mpu sync");
#endif

	if (!g_gpu_mpu_support)
		return 0;

	if (g_GPU_MPU_DATA_OFFSET == g_GPU_MPU_DATA_START_OFFSET) { /* No gpumpu data on SRAM */
#if (DEBUG_HAL)
		MOD_PUTS("No data to sync!");
#endif
	} else {
		ret = ipi_communication();
	}

	hyp_spin_unlock(&g_shared_sram_lock);
#if (DEBUG_HAL)
	MOD_PUTS("[UNLOCK] g_shared_sram_lock");
#endif

	return 0;
}

static int pmm_defragment(void)
{
	return 0;
}

static struct pmm_hal pmm_ops = {
	.prepare		= pmm_prepare,
	.secure			= pmm_secure,
	.unsecure		= pmm_unsecure,
	.secure_v2		= pmm_secure_v2_1,
	.unsecure_v2		= pmm_unsecure_v2_1,
	.sync			= pmm_sync,
	.defragment		= pmm_defragment,
};

static const char *pmm_hal_name = "gpu-mpu";

void register_gpumpu_pmm_hal(const struct user_pt_regs *regs)
{
	MOD_PUTS("register_gpumpu_pmm_hal");

	g_gpu_mpu_support               = regs->regs[1] & 0xffffffff;
	g_GPU_MPU_DATA_SIZE             = regs->regs[1] >> 32;
	g_GPUEB_MBOX_IPI_ID             = regs->regs[2] & 0xffffffff;
	g_GPUEB_MBOX_IPI_SEND_DATA_SIZE = regs->regs[2] >> 32;
	g_GPUEB_SRAM_BASE               = regs->regs[3] & 0xffffffff;
	g_GPUEB_SRAM_SIZE               = regs->regs[3] >> 32;
	g_MBOX_SLOT_SIZE_4B             = regs->regs[4] & 0xffffffff;
	g_GPUEB_GPR_BASE                = regs->regs[4] >> 32;
	g_MBOX0_SET                     = regs->regs[5] & 0xffffffff;
	g_MBOX0_SEND                    = regs->regs[5] >> 32;

#if (DEBUG_HAL)
	MOD_PUTS1("g_gpu_mpu_support", g_gpu_mpu_support);
	MOD_PUTS1("g_GPU_MPU_DATA_SIZE", g_GPU_MPU_DATA_SIZE);
	MOD_PUTS1("g_GPUEB_MBOX_IPI_ID", g_GPUEB_MBOX_IPI_ID);
	MOD_PUTS1("g_GPUEB_MBOX_IPI_SEND_DATA_SIZE", g_GPUEB_MBOX_IPI_SEND_DATA_SIZE);
	MOD_PUTS1("g_GPUEB_SRAM_BASE", g_GPUEB_SRAM_BASE);
	MOD_PUTS1("g_GPUEB_SRAM_SIZE", g_GPUEB_SRAM_SIZE);
	MOD_PUTS1("g_MBOX_SLOT_SIZE_4B", g_MBOX_SLOT_SIZE_4B);
	MOD_PUTS1("g_GPUEB_GPR_BASE", g_GPUEB_GPR_BASE);
	MOD_PUTS1("g_MBOX0_SET", g_MBOX0_SET);
	MOD_PUTS1("g_MBOX0_SEND", g_MBOX0_SEND);
#endif

	pmm_ops.name = pmm_hal_name;
	pmm_ops.is_enabled = true;

	pmm_pre_init();

	hyp_pmm_hal_register(&pmm_ops);
}
