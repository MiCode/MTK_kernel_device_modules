/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __ADSP_PLATFORM_INTERFACE_H__
#define __ADSP_PLATFORM_INTERFACE_H__

struct adspsys_priv;
struct adsp_priv;

enum adsp_sharedmem_id {
	ADSP_SHAREDMEM_BOOTUP_MARK = 0,
	ADSP_SHAREDMEM_SYS_STATUS,
	ADSP_SHAREDMEM_MPUINFO,
	ADSP_SHAREDMEM_WAKELOCK,
	ADSP_SHAREDMEM_IPCBUF,
	ADSP_SHAREDMEM_C2C_0_BUF,
	ADSP_SHAREDMEM_C2C_1_BUF,
	ADSP_SHAREDMEM_C2C_BUFINFO,
	ADSP_SHAREDMEM_TIMESYNC,
	ADSP_SHAREDMEM_DVFSSYNC,
	ADSP_SHAREDMEM_SLEEPSYNC,
	ADSP_SHAREDMEM_BUS_MON_DUMP,
	ADSP_SHAREDMEM_INFRA_BUS_DUMP,
	ADSP_SHAREDMEM_LATMON_DUMP,
	ADSP_SHAREDMEM_NUM,
};

struct adsp_hardware_operations {
	void (*set_swirq)(u32 cid);
	u32 (*check_swirq)(u32 cid);
	void (*clr_spm)(u32 cid);
	void (*toggle_semaphore)(u32 bit);
	u32 (*get_semaphore)(u32 bit);
	bool (*check_hifi_status)(u32 mask);
	u32 (*read_adsp_sys_status)(u32 cid);
	bool (*is_adsp_axibus_idle)(u32 *backup);
	bool (*check_core_active)(u32 cid);
	int (*pre_lock)(u32 cid, bool is_lock);
	ssize_t (*get_dpsw_status)(char *buf, u32 buf_size);
};

/* platform wrap methods */
void adsp_set_swirq(u32 cid);
u32 adsp_check_swirq(u32 cid);
void adsp_clear_spm(u32 cid);
void adsp_toggle_semaphore(u32 bit);
u32 adsp_get_semaphore(u32 bit);
bool check_hifi_status(u32 mask);
u32 read_adsp_sys_status(u32 cid);
u32 get_adsp_sys_status(struct adsp_priv *pdata);
bool is_adsp_axibus_idle(u32 *backup);
bool adsp_is_pre_lock_support(void);
bool check_core_active(u32 cid);
int _adsp_pre_lock(u32 cid, bool is_lock);

#define adsp_pre_lock(cid) _adsp_pre_lock(cid, true)
#define adsp_pre_unlock(cid) _adsp_pre_lock(cid, false)

#define ADSP_A_IS_WFI               (1 << 0)
#define ADSP_B_IS_WFI               (1 << 1)
#define ADSP_AXI_BUS_IS_IDLE        (1 << 2)

#endif
