/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _MTK_EEM_
#define _MTK_EEM_

#include <linux/kernel.h>
#include <mt-plat/sync_write.h>
#include "mtk_eem_config.h"
#include "mtk_aee_rr.h"

#define EN_EEM (1) /* enable/disable EEM (SW) */

/* have 5 banks */
enum eem_ctrl_id {
	EEM_CTRL_2L = 0,
	EEM_CTRL_L = 1,
	EEM_CTRL_CCI = 2,
#if ENABLE_LOO
	EEM_CTRL_L_HI = 2,
#endif
	EEM_CTRL_GPU = 3,
#if ENABLE_LOO
	EEM_CTRL_2L_HI = 4,
#endif

	NR_EEM_CTRL,
};

enum eem_det_id {
	EEM_DET_2L	=	EEM_CTRL_2L,
	EEM_DET_L	=	EEM_CTRL_L,
	EEM_DET_CCI	=	EEM_CTRL_CCI,
#if ENABLE_LOO
	EEM_DET_L_HI = EEM_CTRL_L_HI,
#endif
	EEM_DET_GPU =	EEM_CTRL_GPU,
#if ENABLE_LOO
	EEM_DET_2L_HI	=	EEM_CTRL_2L_HI,
#endif

	NR_EEM_DET,
};

enum mt_eem_cpu_id {
	MT_EEM_CPU_LL,
	MT_EEM_CPU_L,
	MT_EEM_CPU_CCI,

	NR_MT_EEM_CPU,
};

/* internal use */
/* EEM detector is disabled by who */
enum {
	BY_PROCFS	= BIT(0),
	BY_INIT_ERROR	= BIT(1),
	BY_MON_ERROR	= BIT(2),
	BY_PROCFS_INIT2 = BIT(3),
};

enum eem_phase {
	EEM_PHASE_INIT01,
	EEM_PHASE_INIT02,
	EEM_PHASE_MON,

	NR_EEM_PHASE,
};

enum eem_features {
	FEA_INIT01	= BIT(EEM_PHASE_INIT01),
	FEA_INIT02	= BIT(EEM_PHASE_INIT02),
	FEA_MON		= BIT(EEM_PHASE_MON),
};

enum {
	EEM_VOLT_NONE	= 0,
	EEM_VOLT_UPDATE  = BIT(0),
	EEM_VOLT_RESTORE = BIT(1),
};

extern u32 get_devinfo_with_index(u32 index);
extern const unsigned int reg_dump_addr_off[105];

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
#define CONFIG_EEM_AEE_RR_REC 1
#undef CONFIG_EEM_AEE_RR_REC
#endif

#if IS_ENABLED(CONFIG_EEM_AEE_RR_REC)
enum eem_state {
	EEM_CPU_2_LITTLE_IS_SET_VOLT = 0,	/* 2L */
	EEM_CPU_LITTLE_IS_SET_VOLT = 1,		/* L */
	EEM_CPU_CCI_IS_SET_VOLT = 2,		/* CCI */
#if ENABLE_LOO
	EEM_CPU_LITTLE_HI_IS_SET_VOLT = 2,
#endif
	EEM_GPU_IS_SET_VOLT = 3,			/* G */
#if ENABLE_LOO
	EEM_CPU_2_LITTLE_HI_IS_SET_VOLT = 4,
#endif
};
#endif

/* EEM Extern Function */
extern int mt_eem_status(enum eem_det_id id);
extern unsigned int get_efuse_status(void);
extern unsigned int mt_eem_is_enabled(void);

extern void eem_set_pi_efuse(enum eem_det_id id, unsigned int pi_efuse);

/* DRCC */
extern unsigned int drcc_offset_done;
extern void mt_ptp_lock(unsigned long *flags);
extern void mt_ptp_unlock(unsigned long *flags);
#endif
