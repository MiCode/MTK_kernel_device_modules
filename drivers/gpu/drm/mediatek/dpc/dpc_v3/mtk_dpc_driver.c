// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/sched/clock.h>
#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif

#include <dt-bindings/clock/mmdvfs-clk.h>
#include <dt-bindings/memory/mtk-smi-user.h>
#include <soc/mediatek/mmdvfs_v3.h>
#include "mtk-mmdvfs-v3-memory.h"
#include <clk-fmeter.h>
#include "mtk-smi-dbg.h"
#include "mtk-mminfra-util.h"

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
#include "vcp_status.h"
#endif

#if IS_ENABLED(CONFIG_MTK_HWCCF)
#include "hwccf_provider.h"
#include "clkchk.h"
#endif

#include "mtk_dpc_v3.h"
#include "mtk_dpc_mmp.h"
#include "mtk_dpc_internal.h"

#include "mtk_log.h"
#include "mtk_disp_vidle.h"
#include "mtk-mml-dpc.h"
#include "mdp_dpc.h"
#include "mtk_vdisp.h"

int debug_mmp = 1;
module_param(debug_mmp, int, 0644);
int debug_trace = 1;
module_param(debug_trace, int, 0644);
int debug_dvfs;
module_param(debug_dvfs, int, 0644);
int debug_irq = BIT(0) | BIT(7);
module_param(debug_irq, int, 0644);
int irq_aee;
module_param(irq_aee, int, 0644);
int mminfra_floor;
module_param(mminfra_floor, int, 0644);
int post_vlp_delay = 60;
module_param(post_vlp_delay, int, 0644);
u32 dump_begin;
module_param(dump_begin, uint, 0644);
u32 dump_lines = 40;
module_param(dump_lines, uint, 0644);
u32 dump_to_kmsg = 1;
module_param(dump_to_kmsg, uint, 0644);
u32 debug_presz;
module_param(debug_presz, uint, 0644);

/* 0: normal, 1: force wait, 2: force skip */
int wfe_prete = 2;
module_param(wfe_prete, int, 0644);

/* 0: normal, 1: no dt on, 2: eof off, 3: 1 and 2 */
int dt_strategy;
module_param(dt_strategy, int, 0644);

int toggle_cg_fsm = 1;
module_param(toggle_cg_fsm, int, 0644);

int mask_busy_irq = 1;
module_param(mask_busy_irq, int, 0644);

/* BIT0: ap, BIT1: gce*/
int excep_by_xpu = 0b11;
module_param(excep_by_xpu, int, 0644);

static void __iomem *dpc_base;
static struct mtk_dpc *g_priv;
static int step_size = 1;
static unsigned long *g_urate_freq_steps;

/* debug emi violation */
static void __iomem *mmpc_emi_req;
static void __iomem *mmpc_ddrsrc_req;
static void __iomem *spm_ddr_emi_req;
static void __iomem *vdisp_dvfsrc_sw_req4;
static void __iomem *clk_disp_sel;
static void __iomem *debug_reg;
static void __iomem *rgu_reg;

static void __iomem *busy_mask[10];

static void __iomem *hwccf_cg47_set;
static void __iomem *hwccf_cg47_clr;
static void __iomem *hwccf_cg47_en;
static void __iomem *hwccf_cg47_sta;
static void __iomem *hwccf_dummy_set;		/* for cg link/unlink */
static void __iomem *hwccf_dummy_clr;		/* for cg link/unlink */
static void __iomem *hwccf_dummy_en;		/* for cg link/unlink */
static void __iomem *hwccf_xpu0_mtcmos_set;
static void __iomem *hwccf_xpu0_mtcmos_clr;
static void __iomem *hwccf_xpu0_local_en;
static void __iomem *hwccf_xpu6_local_en;
static void __iomem *hwccf_global_en;
static void __iomem *hwccf_global_sta;
static void __iomem *hwccf_total_sta;		/* check all fsm idle, 1 = all idle, 0 = backup idle */
static void __iomem *hwccf_hw_mtcmos_req;	/* for dpc to hwccf for mtcmos */
static void __iomem *hwccf_hw_irq_req;		/* for dpc to hwccf for buck (irq voter) */
static void __iomem *hwccf_mtcmos_pm_ack;	/* for pm check */
static void __iomem *hwccf_mtcmos_en;		/* SW + HW */
static void __iomem *hwccf_mtcmos_sta;

static void __iomem *mmpc_dummy_voter;	/* 0x160(RU) 0x164(SET) 0x168(CLR) */

static atomic_t g_mminfra_cnt = ATOMIC_INIT(0);
static atomic_t buck_ref = ATOMIC_INIT(0);
static atomic_t hwccf_ref = ATOMIC_INIT(0);
static atomic_t pre_cg_ref = ATOMIC_INIT(0);

static atomic_t excep_ret[32] = { ATOMIC_INIT(0) };
static atomic_t g_user_9 = ATOMIC_INIT(0);
static atomic_t g_user_14 = ATOMIC_INIT(0);
static atomic_t g_user_15 = ATOMIC_INIT(0);
static atomic_t g_user_16 = ATOMIC_INIT(0);
static atomic_t g_user_17 = ATOMIC_INIT(0);

static const char trace_buf_mml_on[] = "C|-65536|power_MML|1\n";
static const char trace_buf_mml_off[] = "C|-65536|power_MML|0\n";
static const char trace_buf_disp_on[] = "C|-65536|power_DISP|1\n";
static const char trace_buf_disp_off[] = "C|-65536|power_DISP|0\n";
static const char trace_buf_mminfra_on[] = "C|-65536|power_MMINFRA|1\n";
static const char trace_buf_mminfra_off[] = "C|-65536|power_MMINFRA|0\n";
static const char * const trace_buf_keep[4][2] = {
	{"B|-65536|vidle_keep_0\n", "E|-65536|vidle_keep_0\n"},
	{"B|-65536|vidle_keep_1\n", "E|-65536|vidle_keep_1\n"},
	{"B|-65536|vidle_keep_2\n", "E|-65536|vidle_keep_2\n"},
	{"B|-65536|vidle_keep_3\n", "E|-65536|vidle_keep_3\n"}};
static const char * const trace_buf_release[4][2] = {
	{"B|-65536|vidle_release_0\n", "E|-65536|vidle_release_0\n"},
	{"B|-65536|vidle_release_1\n", "E|-65536|vidle_release_1\n"},
	{"B|-65536|vidle_release_2\n", "E|-65536|vidle_release_2\n"},
	{"B|-65536|vidle_release_3\n", "E|-65536|vidle_release_3\n"}};

static noinline int tracing_mark_write(const char buf[])
{
#ifdef CONFIG_TRACING
	if (debug_trace)
		trace_puts(buf);
#endif
	return 0;
}

static struct mtk_dpc_mtcmos_cfg mt6991_mtcmos_cfg[DPC_SUBSYS_CNT] = {
	{0x500, 0x520, 0x540, 0, 0, 0, 0},
	{0x580, 0x5A0, 0x5C0, 0, 0, 0, 0},
	{0x600, 0x620, 0x640, 0, 0, 0, 0},
	{0x680, 0x6A0, 0x6C0, 0, 0, 0, 0},
	{0x700, 0x720, 0x740, 0, 0, 0, 0},
	{0xB00, 0xB20, 0xB40, 0, 0, 0, 0},
	{0xC00, 0xC20, 0xC40, 0, 0, 0, 0},
	{0xD00, 0xD20, 0xD40, 0, 0, 0, 0},
};

static struct mtk_dpc_mtcmos_cfg mt6993_mtcmos_cfg[DPC3_SUBSYS_CNT] = {
	{0x1000, 0x1020, 0x1024, 0, 0, DPC_MTCMOS_MANUAL, 19},  /* DIS0A */
	{0x1100, 0x1120, 0x1124, 0, 0, DPC_MTCMOS_MANUAL, 20},  /* DIS0B */
	{0x1200, 0x1220, 0x1224, 0, 0, DPC_MTCMOS_MANUAL, 21},  /* DIS1A */
	{0x1300, 0x1320, 0x1324, 0, 0, DPC_MTCMOS_MANUAL, 22},  /* DIS1B */
	{0x1400, 0x1420, 0x1424, 0, 0, DPC_MTCMOS_MANUAL, 23},  /* OVL0 */
	{0x1500, 0x1520, 0x1524, 0, 0, DPC_MTCMOS_MANUAL, 24},  /* OVL1 */
	{0x1600, 0x1620, 0x1624, 0, 0, DPC_MTCMOS_MANUAL, 25},  /* OVL2 */
	{0x1700, 0x1720, 0x1724, 0, 0, DPC_MTCMOS_MANUAL, 26},  /* MML0 */
	{0x1800, 0x1820, 0x1824, 0, 0, DPC_MTCMOS_MANUAL, 27},  /* MML1 */
	{0x1900, 0x1920, 0x1924, 0, 0, DPC_MTCMOS_MANUAL, 28},  /* MML2 */
	{0x1A00, 0x1A20, 0x1A24, 0, 0, DPC_MTCMOS_MANUAL, 30},  /* DPTX !! notice !! */
	{0x1B00, 0x1B20, 0x1B24, 0, 0, DPC_MTCMOS_MANUAL, 29},  /* PERI !! notice !! */
};

static struct mtk_dpc2_dt_usage mt6991_dt_usage[DPC2_VIDLE_CNT] = {
/* 0*/	{0, 500},						/* OVL/DISP0	EOF	OFF	*/
/* 1*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ - DPC2_DT_MTCMOS},	/*		TE	ON	*/
/* 2*/	{0, 0x13B13B},						/*		TE	PRETE	*/
/* 3*/	{1, DPC2_DT_POSTSZ},					/*		TE	OFF	*/
/* 4*/	{0, 500},						/* DISP1	EOF	OFF	*/
/* 5*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ - DPC2_DT_MTCMOS},	/*		TE	ON	*/
/* 6*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ},			/*		TE	PRETE	*/
/* 7*/	{1, DPC2_DT_POSTSZ},					/*		TE	OFF	*/
/* 8*/	{0, 0x13B13B},	/* VDISP */
/* 9*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ - DPC2_DT_MMINFRA},
/*10*/	{1, DPC2_DT_POSTSZ},
/*11*/	{0, 500},	/* HRT */
/*12*/	{1, DPC2_DT_TE_120 - DPC2_DT_INFRA},
/*13*/	{1, DPC2_DT_POSTSZ},
/*14*/	{0, 0x13B13B},	/* SRT */
/*15*/	{0, 0x13B13B},
/*16*/	{0, 0x13B13B},
/*17*/	{0, 0x13B13B},	/* MMINFRA */
/*18*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ - DPC2_DT_MMINFRA},
/*19*/	{1, DPC2_DT_POSTSZ},
/*20*/	{0, 0x13B13B},	/* INFRA */
/*21*/	{0, 0x13B13B},
/*22*/	{0, 0x13B13B},
/*23*/	{0, 0x13B13B},	/* MAINPLL */
/*24*/	{0, 0x13B13B},
/*25*/	{0, 0x13B13B},
/*26*/	{0, 0x13B13B},	/* MSYNC 2.0 */
/*27*/	{0, 0x13B13B},
/*28*/	{0, 0x13B13B},
/*29*/	{1, 100},	/* SAFE ZONE */
/*30*/	{0, 1000},	/* RESERVED */
/*31*/	{0, 0x13B13B},
/*32*/	{0, 500},						/* MML1		EOF	OFF	*/
/*33*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ - DPC2_DT_MTCMOS},	/*		TE	ON	*/
/*34*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ + 20},		/*		TE	PRETE	*/
/*35*/	{1, DPC2_DT_POSTSZ},					/*		TE	OFF	*/
/*36*/	{0, 0x13B13B},	/* VDISP */
/*37*/	{0, 0x13B13B},
/*38*/	{0, 0x13B13B},
/*39*/	{0, 500},	/* HRT */
/*40*/	{1, DPC2_DT_TE_120 - DPC2_DT_INFRA},
/*41*/	{1, DPC2_DT_POSTSZ},
/*42*/	{0, 0x13B13B},	/* SRT */
/*43*/	{0, 0x13B13B},
/*44*/	{0, 0x13B13B},
/*45*/	{0, 0x13B13B},	/* MMINFRA */
/*46*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ - DPC2_DT_MMINFRA},
/*47*/	{1, DPC2_DT_POSTSZ},
/*48*/	{0, 0x13B13B},	/* INFRA */
/*49*/	{0, 0x13B13B},
/*50*/	{0, 0x13B13B},
/*51*/	{0, 0x13B13B},	/* MAINPLL */
/*52*/	{0, 0x13B13B},
/*53*/	{0, 0x13B13B},
/*54*/	{0, 0x13B13B},	/* RESERVED */
/*55*/	{0, 0x13B13B},
/*56*/	{0, 0x13B13B},
/*57*/	{0, 0x13B13B},	/* DISP 26M */
/*58*/	{0, 0x13B13B},
/*59*/	{0, 0x13B13B},
/*60*/	{0, 0x13B13B},	/* DISP PMIC */
/*61*/	{0, 0x13B13B},
/*62*/	{0, 0x13B13B},
/*63*/	{0, 0x13B13B},	/* DISP VCORE */
/*64*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ - DPC2_DT_MTCMOS - DPC2_DT_DSION - DPC2_DT_VCORE},
/*65*/	{1, DPC2_DT_POSTSZ + DPC2_DT_DSIOFF},
/*66*/	{0, 0x13B13B},	/* MML 26M */
/*67*/	{0, 0x13B13B},
/*68*/	{0, 0x13B13B},
/*69*/	{0, 0x13B13B},	/* MML PMIC */
/*70*/	{0, 0x13B13B},
/*71*/	{0, 0x13B13B},
/*72*/	{0, 0x13B13B},	/* MML VCORE */
/*73*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ - DPC2_DT_MTCMOS - DPC2_DT_DSION - DPC2_DT_VCORE},
/*74*/	{1, DPC2_DT_POSTSZ + DPC2_DT_DSIOFF},
/*75*/	{0, 0x13B13B},	/* DSIPHY */
/*76*/	{1, DPC2_DT_TE_120 - DPC2_DT_PRESZ - DPC2_DT_MTCMOS - DPC2_DT_DSION},
/*77*/	{1, DPC2_DT_POSTSZ},
};

static struct mtk_dpc2_dt_usage mt6993_dt_usage[DPC3_VIDLE_CNT] = {
/* 0*/	{0, DPC2_DT_POSTSZ},					/* OVL/DISP0	EOF	OFF	*/
/* 1*/	{1, DT_TE_360 - DPC2_DT_PRESZ - DPC2_DT_MTCMOS},	/*		TE	ON	*/
/* 2*/	{0, 0x13B13B},						/*		TE	PRETE	*/
/* 3*/	{1, DPC2_DT_POSTSZ},					/*		TE	OFF	*/
/* 4*/	{0, DPC2_DT_POSTSZ},					/* DISP1	EOF	OFF	*/
/* 5*/	{1, DT_TE_360 - DPC2_DT_PRESZ - DPC2_DT_MTCMOS},	/*		TE	ON	*/
/* 6*/	{1, DT_TE_360 - DPC2_DT_PRESZ},				/*		TE	PRETE	*/
/* 7*/	{1, DPC2_DT_POSTSZ},					/*		TE	OFF	*/
/* 8*/	{0, DPC2_DT_POSTSZ},	/* VDISP */
/* 9*/	{1, DT_TE_360 - DPC2_DT_PRESZ - DPC2_DT_MMINFRA},
/*10*/	{1, DPC2_DT_POSTSZ},
/*11*/	{0, DPC2_DT_POSTSZ},	/* HRT */
/*12*/	{1, DT_TE_360 - DPC2_DT_INFRA},
/*13*/	{1, DPC2_DT_POSTSZ},
/*14*/	{0, 0x13B13B},	/* SRT */
/*15*/	{0, 0x13B13B},
/*16*/	{0, 0x13B13B},
/*17*/	{0, DPC2_DT_MMINFRA},	/* MMINFRA */
/*18*/	{1, DT_TE_360 - DPC2_DT_PRESZ - DPC2_DT_MMINFRA},
/*19*/	{0, DPC2_DT_MMINFRA},
/*20*/	{0, 0x13B13B},	/* INFRA */
/*21*/	{0, 0x13B13B},
/*22*/	{0, 0x13B13B},
/*23*/	{0, 0x13B13B},	/* MAINPLL */
/*24*/	{0, 0x13B13B},
/*25*/	{0, 0x13B13B},
/*26*/	{0, 0x13B13B},	/* MSYNC 2.0 */
/*27*/	{0, 0x13B13B},
/*28*/	{0, 0x13B13B},
/*29*/	{1, 100},	/* SAFE ZONE */
/*30*/	{0, 1000},	/* RESERVED */
/*31*/	{0, 0x13B13B},
/*32*/	{0, DPC2_DT_POSTSZ},					/* MML1		EOF	OFF	*/
/*33*/	{1, DT_TE_360 - DPC2_DT_PRESZ - DPC2_DT_MTCMOS},	/*		TE	ON	*/
/*34*/	{1, DT_TE_360 - DPC2_DT_PRESZ + 20},			/*		TE	PRETE	*/
/*35*/	{1, DPC2_DT_POSTSZ},					/*		TE	OFF	*/
/*36*/	{0, 0x13B13B},	/* VDISP */
/*37*/	{0, 0x13B13B},
/*38*/	{0, 0x13B13B},
/*39*/	{0, DPC2_DT_POSTSZ},	/* HRT */
/*40*/	{1, DT_TE_360 - DPC2_DT_INFRA},
/*41*/	{1, DPC2_DT_POSTSZ},
/*42*/	{0, 0x13B13B},	/* SRT */
/*43*/	{0, 0x13B13B},
/*44*/	{0, 0x13B13B},
/*45*/	{0, DPC2_DT_POSTSZ},	/* MMINFRA */
/*46*/	{0, DT_TE_360 - DPC2_DT_PRESZ - DPC2_DT_MMINFRA},
/*47*/	{0, DPC2_DT_POSTSZ},
/*48*/	{0, 0x13B13B},	/* INFRA */
/*49*/	{0, 0x13B13B},
/*50*/	{0, 0x13B13B},
/*51*/	{0, 0x13B13B},	/* MAINPLL */
/*52*/	{0, 0x13B13B},
/*53*/	{0, 0x13B13B},
/*54*/	{0, 0x13B13B},	/* RESERVED */
/*55*/	{0, 0x13B13B},
/*56*/	{0, 0x13B13B},
/*57*/	{0, 0x13B13B},	/* DISP 26M */
/*58*/	{0, 0x13B13B},
/*59*/	{0, 0x13B13B},
/*60*/	{0, 0x13B13B},	/* DISP PMIC */
/*61*/	{0, 0x13B13B},
/*62*/	{0, 0x13B13B},
/*63*/	{0, 0x13B13B},	/* DISP VCORE */
/*64*/	{1, DT_TE_360 - DPC2_DT_PRESZ - DPC2_DT_MTCMOS - DPC2_DT_DSION - DPC2_DT_VCORE},
/*65*/	{1, DPC2_DT_POSTSZ + DPC2_DT_DSIOFF},
/*66*/	{0, 0x13B13B},	/* MML 26M */
/*67*/	{0, 0x13B13B},
/*68*/	{0, 0x13B13B},
/*69*/	{0, 0x13B13B},	/* MML PMIC */
/*70*/	{0, 0x13B13B},
/*71*/	{0, 0x13B13B},
/*72*/	{0, 0x13B13B},	/* MML VCORE */
/*73*/	{1, DT_TE_360 - DPC2_DT_PRESZ - DPC2_DT_MTCMOS - DPC2_DT_DSION - DPC2_DT_VCORE},
/*74*/	{1, DPC2_DT_POSTSZ + DPC2_DT_DSIOFF},
/*75*/	{0, 0x13B13B},	/* DSIPHY */
/*76*/	{1, DT_TE_360 - DPC2_DT_PRESZ - DPC2_DT_MTCMOS - DPC2_DT_DSION},
/*77*/	{1, DPC2_DT_POSTSZ},
/*78*/	{0, 0x13B13B},	/* PERI */
/*79*/	{0, 0x13B13B},
/*80*/	{0, 0x13B13B},
};

static struct mtk_dpc_channel_bw_cfg mt6991_ch_bw_cfg[24] = {
/*	offset	shift	bw		bits	AXI	S/H	R/W	mmlsys	*/
/* 0*/	{0xA10,	0, 0, 0},	/*	[9:0]	00	S	R	0xA70	*/
/* 1*/	{0xA1C,	0, 0, 0},	/*	[9:0]	00	S	W	0xA7C	*/
/* 2*/	{0xA28,	0, 0, 0},	/*	[9:0]	00	H	R	0xA88	*/
/* 3*/	{0xA34,	0, 0, 0},	/*	[9:0]	00	H	W	0xA94	*/
/* 4*/	{0xA10,	12, 0, 0},	/*	[21:12]	01	S	R	0xA70	*/
/* 5*/	{0xA1C,	12, 0, 0},	/*	[21:12]	01	S	W	0xA7C	*/
/* 6*/	{0xA28,	12, 0, 0},	/*	[21:12]	01	H	R	0xA88	*/
/* 7*/	{0xA34,	12, 0, 0},	/*	[21:12]	01	H	W	0xA94	*/
/* 8*/	{0xA14,	0, 0, 0},	/*	[9:0]	10	S	R	0xA74	*/
/* 9*/	{0xA20,	0, 0, 0},	/*	[9:0]	10	S	W	0xA80	*/
/*10*/	{0xA2C,	0, 0, 0},	/*	[9:0]	10	H	R	0xA8C	*/
/*11*/	{0xA38,	0, 0, 0},	/*	[9:0]	10	H	W	0xA98	*/
/*12*/	{0xA14,	12, 0, 0},	/*	[21:12]	11	S	R	0xA74	*/
/*13*/	{0xA20,	12, 0, 0},	/*	[21:12]	11	S	W	0xA80	*/
/*14*/	{0xA2C,	12, 0, 0},	/*	[21:12]	11	H	R	0xA8C	*/
/*15*/	{0xA38,	12, 0, 0},	/*	[21:12]	11	H	W	0xA98	*/
/*16*/	{0xA18,	0, 0, 0},	/*	[9:0]	SLB	S	R	0xA78	*/
/*17*/	{0xA24,	0, 0, 0},	/*	[9:0]	SLB	S	W	0xA84	*/
/*18*/	{0xA30,	0, 0, 0},	/*	[9:0]	SLB	H	R	0xA90	*/
/*19*/	{0xA3C,	0, 0, 0},	/*	[9:0]	SLB	H	W	0xA9C	*/
/*20*/	{0xA18,	12, 0, 0},	/*	[21:12]	SLB	S	R	0xA78	*/
/*21*/	{0xA24,	12, 0, 0},	/*	[21:12]	SLB	S	W	0xA84	*/
/*22*/	{0xA30,	12, 0, 0},	/*	[21:12]	SLB	H	R	0xA90	*/
/*23*/	{0xA3C,	12, 0, 0},	/*	[21:12]	SLB	H	W	0xA9C	*/
/*	AXI00	AXI01	AXI10	AXI11	*/
/*	0	1	21	20	*/
/*	37	36	34	35	*/
/*	2	32	3	33	*/
};

static struct mtk_dpc_channel_bw_cfg mt6993_ch_bw_cfg[28] = {
/*	offset	shift	bw		bits	AXI	S/H	R/W	mmlsys	*/
/* 0*/	{0xA10,	0, 0, 0},	/*	[12:0]	00	S	R	0xA70	*/
/* 1*/	{0xA1C,	0, 0, 0},	/*	[12:0]	00	S	W	0xA7C	*/
/* 2*/	{0xA28,	0, 0, 0},	/*	[12:0]	00	H	R	0xA88	*/
/* 3*/	{0xA34,	0, 0, 0},	/*	[12:0]	00	H	W	0xA94	*/
/* 4*/	{0xA10,	16, 0, 0},	/*	[28:16]	01	S	R	0xA70	*/
/* 5*/	{0xA1C,	16, 0, 0},	/*	[28:16]	01	S	W	0xA7C	*/
/* 6*/	{0xA28,	16, 0, 0},	/*	[28:16]	01	H	R	0xA88	*/
/* 7*/	{0xA34,	16, 0, 0},	/*	[28:16]	01	H	W	0xA94	*/
/* 8*/	{0xA14,	0, 0, 0},	/*	[12:0]	10	S	R	0xA74	*/
/* 9*/	{0xA20,	0, 0, 0},	/*	[12:0]	10	S	W	0xA80	*/
/*10*/	{0xA2C,	0, 0, 0},	/*	[12:0]	10	H	R	0xA8C	*/
/*11*/	{0xA38,	0, 0, 0},	/*	[12:0]	10	H	W	0xA98	*/
/*12*/	{0xA14,	16, 0, 0},	/*	[28:16]	11	S	R	0xA74	*/
/*13*/	{0xA20,	16, 0, 0},	/*	[28:16]	11	S	W	0xA80	*/
/*14*/	{0xA2C,	16, 0, 0},	/*	[28:16]	11	H	R	0xA8C	*/
/*15*/	{0xA38,	16, 0, 0},	/*	[28:16]	11	H	W	0xA98	*/
/*16*/	{0xA18,	0, 0, 0},	/*	[12:0]	SLB	S	R	0xA78	*/
/*17*/	{0xA24,	0, 0, 0},	/*	[12:0]	SLB	S	W	0xA84	*/
/*18*/	{0xA30,	0, 0, 0},	/*	[12:0]	SLB	H	R	0xA90	*/
/*19*/	{0xA3C,	0, 0, 0},	/*	[12:0]	SLB	H	W	0xA9C	*/
/*20*/	{0xA18,	16, 0, 0},	/*	[28:16]	SLB	S	R	0xA78	*/
/*21*/	{0xA24,	16, 0, 0},	/*	[28:16]	SLB	S	W	0xA84	*/
/*22*/	{0xA30,	16, 0, 0},	/*	[28:16]	SLB	H	R	0xA90	*/
/*23*/	{0xA3C,	16, 0, 0},	/*	[28:16]	SLB	H	W	0xA9C	*/
/*    COMM 1       1       0       0	*/
/* bus_sel  1       0       0       1	*/
/*      AXI11   AXI10   AXI00   AXI01	*/
/*	0	21	1	20	*/
/*	36	34	37	35	*/
/*	51	52	50	53	*/
/*	55	32	34	33	*/
/*	3	57	56	2	*/

/*24*/	{0xAA0,	0, 0, 0},	/*	[12:0]	DRAM	H		0xAB0	*/
/*25*/	{0xAA0,	16, 0, 0},	/*	[28:16]	DRAM	S		0xAB0	*/
/*26*/	{0xAA4,	0, 0, 0},	/*	[12:0]	EMI	H		0xAB4	*/
/*27*/	{0xAA4,	16, 0, 0},	/*	[28:16]	EMI	S		0xAB4	*/
};

static inline int dpc_pm_ctrl(bool en)
{
	int ret = 0;

	if (!g_priv->pd_dev)
		goto skip_pm;

	if (en) {
		ret = pm_runtime_resume_and_get(g_priv->pd_dev);
		if (ret) {
			DPCERR("get failed ret(%d) vcp_is_alive(%u)", ret, g_priv->vcp_is_alive);
			return -1;
		}

		/* read dummy register to make sure it's ready to use */
		if (g_priv->mminfra_dummy && (readl(g_priv->mminfra_dummy) == 0)) {
			dump_stack();
			DPCAEE("%s read mminfra dummy failed", __func__);
			pm_runtime_put_sync(g_priv->pd_dev);
			return -2;
		}

		/* disable devapc power check false alarm, */
		/* DPC address is bound by power of disp1 on 6989 */
		if (g_priv->mminfra_hangfree)
			writel(readl(g_priv->mminfra_hangfree) & ~0x1, g_priv->mminfra_hangfree);
	} else
		pm_runtime_put_sync(g_priv->pd_dev);

	return ret;

skip_pm:
	mtk_mminfra_on_off(en, g_priv->mminfra_pwr_idx, g_priv->mminfra_pwr_type);

	return ret;
}

static int dpc_mminfra_on_off(bool en, const enum mtk_vidle_voter_user user)
{
	s32 cnt;

	cnt = en ? atomic_inc_return(&g_mminfra_cnt) : atomic_dec_return(&g_mminfra_cnt);
	if (cnt < 0) {
		DPCERR("skipped, user(%u) mminfra cnt < 0", user);
		atomic_set_release(&g_mminfra_cnt, 0);
		return -1;
	}

	if (en && cnt == 1)
		dpc_mmp(mminfra, MMPROFILE_FLAG_START, user, 0x11111111);

	dpc_pm_ctrl(en);

	if (!en && cnt == 0)
		dpc_mmp(mminfra, MMPROFILE_FLAG_END, user, 0x22222222);

	return 0;
}

static inline bool dpc_pm_check_and_get(void)
{
	if (!g_priv->pd_dev)
		return false;

	return pm_runtime_get_if_in_use(g_priv->pd_dev) > 0 ? true : false;
}

void dpc_pre_cg_ctrl(bool en, bool lock)
{
	s32 cnt;

	if (lock)
		mutex_lock(&g_priv->excp_lock);

	if (en) {
		cnt = atomic_inc_return(&pre_cg_ref);
		if (cnt == 1) {
			clk_prepare_enable(g_priv->pwr_clk[0]);
			clk_prepare_enable(g_priv->pwr_clk[1]);
			clk_prepare_enable(g_priv->pwr_clk[2]);
		}
	} else {
		cnt = atomic_dec_return(&pre_cg_ref);
		if (cnt == 0) {
			clk_disable_unprepare(g_priv->pwr_clk[2]);
			clk_disable_unprepare(g_priv->pwr_clk[1]);
			clk_disable_unprepare(g_priv->pwr_clk[0]);
		}
	}

	if (lock)
		mutex_unlock(&g_priv->excp_lock);

	if (cnt < 0)
		DPCERR("pre_cg_ref cnt underflow");
}

int dpc_buck_status(int op)
{
	if (op == 0) {
		u32 v1 = readl(hwccf_xpu0_local_en) & 0x1ff80000;
		u32 v2 = readl(hwccf_xpu6_local_en) & 0x1ff80000;
		u32 v3 = readl(hwccf_global_en) & 0x1ff80000;

		atomic_set_release(&buck_ref, 0);

		if (v1 || v2 || v3) {
			dump_stack();
			DPCERR("voter 0:%#x 6:%#x G:%#x", v1, v2, v3);
		}
	} else if (op == 1) {
		writel(0, dpc_base + DISP_DPC_INTSTA_INTF_PWR_RDY_STATE);
		atomic_set_release(&buck_ref, 1);
	} else
		return atomic_read(&buck_ref);

	return op;
}

static void dpc_mtcmos_vote_v2(const u32 subsys, const u8 thread, const bool en)
{
	u32 addr = 0;

	if (subsys >= g_priv->subsys_cnt)
		return;

	/* CLR : execute SW threads, disable auto MTCMOS */
	addr = en ? (g_priv->mtcmos_cfg[subsys].thread_clr + thread * 0x4)
		  : (g_priv->mtcmos_cfg[subsys].thread_set + thread * 0x4);
	writel(1, dpc_base + addr);
}

static void dpc_mtcmos_vote_v3(const u32 subsys, const u8 thread, const bool en)
{
	u32 addr = 0;

	if (subsys >= g_priv->subsys_cnt)
		return;

	/* CLR : execute SW threads, disable auto MTCMOS */
	addr = en ? (g_priv->mtcmos_cfg[subsys].thread_clr)
		  : (g_priv->mtcmos_cfg[subsys].thread_set);
	writel(BIT(thread), dpc_base + addr);
}

static int dpc_wait_pwr_ack_v2(const u32 subsys)
{
	int ret = 0;
	u32 value = 0;

	if (subsys >= g_priv->subsys_cnt)
		return -1;

	if (!g_priv->mtcmos_cfg[subsys].chk_va) {
		DPCERR("0x%pa not exist\n", &g_priv->mtcmos_cfg[subsys].chk_pa);
		return -1;
	}

	if (!dpc_is_power_on_v2()) {
		DPCERR("disp vcore is not power on");
		ret = -3;
		goto no_pwr_err;
	}

	/* by dpc */
	ret = readl_poll_timeout_atomic(g_priv->mtcmos_cfg[subsys].chk_va,
					value, value & BIT(20), 1, 200);
	if (ret < 0) {
		DPCERR("wait subsys(%d) power on timeout", subsys);
		goto no_pwr_err;
	}

	return ret;

no_pwr_err:
	dump_stack();
	udelay(post_vlp_delay);
	return ret;
}

static int dpc_wait_pwr_ack_v3(const u32 subsys)
{
	int ret = 0;
	u32 value = 0;
	u32 mask = (subsys < g_priv->subsys_cnt) ? BIT(subsys) : subsys;

	if (!dpc_is_power_on_v2()) {
		DPCERR("disp vcore is not power on");
		ret = -3;
		goto no_pwr_err;
	}

	/* The order of mtcmos status bits must be exactly the same as enum mtk_dpc_subsys_v3
	 * [0]disp0a [1]disp0b [2]disp1a [3]disp1b
	 * [4]ovl0  [5]ovl1  [6]ovl2
	 * [7]mml0  [8]mml1  [9]mml2
	 * [10] dptx [11] peri
	 */
	ret = readl_poll_timeout_atomic(dpc_base + DISP_DPC_MTCMOS_STATUS,
					value, value & mask, 1, 3000);
	if (ret < 0) {
		DPCERR("subsys(%u) status(%#x) voter(%#x) intf(%#x,%#x,%#x,%#x,%#x,%#x,%#x,%#x,%#x,%#x)",
			subsys, readl(dpc_base + DISP_DPC_MTCMOS_STATUS), readl(g_priv->voter_set_va),
			readl(ioremap(0x3eff2080, 0x4)),
			readl(ioremap(0x3eff3080, 0x4)),
			readl(ioremap(0x3eff4080, 0x4)),
			readl(ioremap(0x3eff5080, 0x4)),
			readl(ioremap(0x3eff6080, 0x4)),
			readl(ioremap(0x3eff7080, 0x4)),
			readl(ioremap(0x3eff8080, 0x4)),
			readl(ioremap(0x3eff9080, 0x4)),
			readl(ioremap(0x3effa080, 0x4)),
			readl(ioremap(0x3effb080, 0x4)));
		goto no_pwr_err;
	}

	return ret;

no_pwr_err:
	dump_stack();
	udelay(post_vlp_delay);
	return ret;
}

static void dpc_dt_set_v2(u16 dt, u32 us)
{
	u32 value = us * 26;	/* 26M base, 20 bits range, 38.46 ns ~ 38.46 ms*/

	if ((dt > 56) && (g_priv->mmsys_id != MMSYS_MT6991))
		return;

	writel(value, dpc_base + DISP_REG_DPC_DTx_COUNTER(dt));
}

static void dpc2_dt_en(u16 idx, bool en, bool set_sw_trig)
{
	u32 val;
	u32 addr;
	u16 bit;

	if (g_priv->mmsys_id == MMSYS_MT6989 || g_priv->mmsys_id == MMSYS_MT6878) {
		if (idx < 32) {
			addr = DISP_REG_DPC_DISP_DT_EN;
			bit = idx;
		} else {
			addr = DISP_REG_DPC_MML_DT_EN;
			bit = idx - 32;
		}
	} else if (g_priv->mmsys_id == MMSYS_MT6991) {
		if (idx < 32) {
			addr = DISP_REG_DPC_DISP_DT_EN;
			bit = idx;
		} else if (idx < 57) {
			addr = DISP_REG_DPC_MML_DT_EN;
			bit = idx - 32;
		} else if (idx < 66) {
			addr = DISP_REG_DPC2_DISP_DT_EN;
			bit = idx - 57;
		} else if (idx < 75) {
			addr = DISP_REG_DPC2_MML_DT_EN;
			bit = idx - 66;
		} else if (idx < 78){
			addr = DISP_REG_DPC2_DISP_DT_EN;
			bit = idx - 66;
		} else {
			DPCERR("idx(%u) over then dt count", idx);
			return;
		}
	} else if (g_priv->mmsys_id == MMSYS_MT6993) {
		if (idx < 32) {
			addr = DISP_REG_DPC_DISP_DT_EN;
			bit = idx;
		} else if (idx < 57) {
			addr = DISP_REG_DPC_MML_DT_EN;
			bit = idx - 32;
		} else if (idx < 66) {
			addr = DISP_REG_DPC2_DISP_DT_EN;
			bit = idx - 57;
		} else if (idx < 75) {
			addr = DISP_REG_DPC2_MML_DT_EN;
			bit = idx - 66;
		} else if (idx < 81){
			addr = DISP_REG_DPC2_DISP_DT_EN;
			bit = idx - 66;
		} else {
			DPCERR("idx(%u) over then dt count", idx);
			return;
		}
	} else {
		DPCERR("not support platform");
		return;
	}

	val = readl(dpc_base + addr);
	if (en)
		val |= BIT(bit);
	else
		val &= ~BIT(bit);
	writel(val, dpc_base + addr);

	if (idx < 57)
		addr += 0x4;
	else
		addr += 0x8;

	val = readl(dpc_base + addr);
	if (set_sw_trig)
		val |= BIT(bit);
	else
		val &= ~BIT(bit);
	writel(val, dpc_base + addr);
}

static void dpc_dt_en_all(const u32 subsys, u32 dt_en)
{
	u32 cnt, idx;
	struct mtk_dpc_dt_usage *usage = NULL;

	if (subsys == DPC_SUBSYS_DISP) {
		writel(dt_en, dpc_base + DISP_REG_DPC_DISP_DT_EN);
		usage = &g_priv->disp_dt_usage[0];
	} else if (subsys == DPC_SUBSYS_MML) {
		writel(dt_en, dpc_base + DISP_REG_DPC_MML_DT_EN);
		usage = &g_priv->mml_dt_usage[0];
	}

	if (!usage) {
		DPCERR("%s:%d NULL Pointer\n", __func__, __LINE__);
		return;
	}

	cnt = __builtin_popcount(dt_en);
	while (cnt--) {
		idx = __builtin_ffs(dt_en) - 1;
		dt_en &= ~(1 << idx);
		dpc_dt_set_v2(usage[idx].index, usage[idx].ep);
	}
}

static void dpc_dt_set_update(u16 dt, u32 us)
{
	if (g_priv->dpc2_dt_usage) {
		g_priv->dpc2_dt_usage[dt].val = us;
	}// else {
		// if (dt < DPC_DISP_DT_CNT)
		// 	mt6989_disp_dt_usage[dt].ep = us;
		// else if (dt < DPC_DISP_DT_CNT + DPC_MML_DT_CNT)
		// 	mt6989_mml_dt_usage[dt - DPC_DISP_DT_CNT].ep = us;
	//}

	if (!dpc_is_power_on_v2())
		return;

	dpc_dt_set_v2(dt, us);
}

static void dpc_duration_update_v2(const u32 us)
{
	u32 presz = DPC2_DT_PRESZ;

	if (debug_presz)
		presz = debug_presz;

	dpc_dt_set_update( 1, us - presz - DPC2_DT_MTCMOS);
	dpc_dt_set_update( 5, us - presz - DPC2_DT_MTCMOS);
	dpc_dt_set_update( 6, us - presz);
	dpc_dt_set_update( 9, us - presz - DPC2_DT_MMINFRA);
	dpc_dt_set_update(12, us - DPC2_DT_INFRA);
	dpc_dt_set_update(18, us - presz - DPC2_DT_MMINFRA);
	dpc_dt_set_update(33, us - presz - DPC2_DT_MTCMOS);
	dpc_dt_set_update(34, us - DPC2_DT_PRESZ + 20);
	dpc_dt_set_update(40, us - DPC2_DT_INFRA);
	dpc_dt_set_update(46, us - presz - DPC2_DT_MMINFRA);
	dpc_dt_set_update(64, us - presz - DPC2_DT_MTCMOS - DPC2_DT_DSION - DPC2_DT_VCORE);
	dpc_dt_set_update(61, us - DPC2_DT_INFRA);
	dpc_dt_set_update(70, us - DPC2_DT_INFRA);
	dpc_dt_set_update(73, us - presz - DPC2_DT_MTCMOS - DPC2_DT_DSION - DPC2_DT_VCORE);
	dpc_dt_set_update(76, us - presz - DPC2_DT_MTCMOS - DPC2_DT_DSION);

	/* wa for 90 hz extra dsi te */
	dpc_dt_set_update(7, us == 11111 ? 3000 : DPC2_DT_POSTSZ);
}

static void dpc_duration_update_v3(const u32 us)
{
	dpc_duration_update_v2(us);
}

static void dpc_enable_v2(const u8 en)
{
	u16 i = 0;

	if (en == 2)
		g_priv->vidle_mask = 0;

	debug_irq = 0; /* disable irq for both vdo and cmd mode, remove this if needed */

	if (en) {
		if (g_priv->mmsys_id == MMSYS_MT6991) {
			for (i = 0; i < DPC2_VIDLE_CNT; i ++) {
				if (g_priv->dpc2_dt_usage[i].en) {
					dpc2_dt_en(i, true, false);
					dpc_dt_set_update(i, g_priv->dpc2_dt_usage[i].val);
				}
			}

			if (debug_irq) {
				writel(BIT(31) | BIT(18) | BIT(9),
				       dpc_base + DISP_REG_DPC_DISP_INTEN);

				writel(BIT(13) | BIT(14) | BIT(17) | BIT(18) | BIT(31),
				       dpc_base + DISP_REG_DPC_MML_INTEN);
			}

			writel(0x40, dpc_base + DISP_DPC_ON2SOF_DT_EN);
			writel(0xf, dpc_base + DISP_DPC_ON2SOF_DSI0_SOF_COUNTER); /* should > 2T, cannot be zero */
			writel(0x2, dpc_base + DISP_REG_DPC_DEBUG_SEL);	/* mtcmos_debug */
			writel(0x7, dpc_base + DISP_REG_DPC_DUMMY0); /* criteria switch to DSC */
		} else {
			/* DT enable only 1, 3, 5, 6, 7, 12, 13, 29, 30, 31 */
			dpc_dt_en_all(DPC_SUBSYS_DISP, 0xe00030ea);

			/* DT enable only 1, 3, 8, 9 */
			dpc_dt_en_all(DPC_SUBSYS_MML, 0x30a);
		}

		/* wla ddren ack */
		writel(1, dpc_base + DISP_REG_DPC_DDREN_ACK_SEL);

		writel(0, dpc_base + DISP_REG_DPC_MERGE_DISP_INT_CFG);
		writel(0, dpc_base + DISP_REG_DPC_MERGE_MML_INT_CFG);

		writel(0x1f, dpc_base + DISP_REG_DPC_DISP_EXT_INPUT_EN);
		writel(0x3, dpc_base + DISP_REG_DPC_MML_EXT_INPUT_EN);

		writel(g_priv->dt_follow_cfg, dpc_base + DISP_REG_DPC_DISP_DT_FOLLOW_CFG);
		writel(g_priv->dt_follow_cfg, dpc_base + DISP_REG_DPC_MML_DT_FOLLOW_CFG);

		/* DISP_DPC_EN | DISP_DPC_DT_EN | DISP_DPC_MMQOS_ALWAYS_SCAN_EN */
		writel(0x13 | (en == 2 ? BIT(16) : 0), dpc_base + DISP_REG_DPC_EN);
		dpc_mmp(config, MMPROFILE_FLAG_PULSE, U32_MAX, 1);
	} else {
		/* disable inten to avoid burst irq */
		writel(0, dpc_base + DISP_REG_DPC_DISP_INTEN);
		writel(0, dpc_base + DISP_REG_DPC_MML_INTEN);
		writel(0, dpc_base + DISP_REG_DPC_EN);

		/* reset dpc to clean counter start and value */
		writel(1, dpc_base + DISP_REG_DPC_RESET);
		writel(0, dpc_base + DISP_REG_DPC_RESET);

		dpc_mmp(config, MMPROFILE_FLAG_PULSE, U32_MAX, 0);
	}

	/* enable gce event */
	writel(en, dpc_base + DISP_REG_DPC_EVENT_EN);

	g_priv->enabled = en;
}

static void dt_strategy_decision(void)
{
	if (dt_strategy & 0b01) {	/* skip power on dt */
		g_priv->dpc2_dt_usage[1].en = 0;
		g_priv->dpc2_dt_usage[5].en = 0;
		g_priv->dpc2_dt_usage[9].en = 0;
		g_priv->dpc2_dt_usage[12].en = 0;
		g_priv->dpc2_dt_usage[18].en = 0;
		g_priv->dpc2_dt_usage[33].en = 0;
		g_priv->dpc2_dt_usage[40].en = 0;
		g_priv->dpc2_dt_usage[46].en = 0;
	} else {
		g_priv->dpc2_dt_usage[1].en = 1;
		g_priv->dpc2_dt_usage[5].en = 1;
		g_priv->dpc2_dt_usage[9].en = 1;
		g_priv->dpc2_dt_usage[12].en = 1;
		g_priv->dpc2_dt_usage[18].en = 1;
		g_priv->dpc2_dt_usage[33].en = 1;
		g_priv->dpc2_dt_usage[40].en = 1;
		g_priv->dpc2_dt_usage[46].en = 1;
	}

	if (dt_strategy & 0b10) {	/* enable eof power off dt */
		g_priv->dpc2_dt_usage[0].en = 1;
		g_priv->dpc2_dt_usage[4].en = 1;
		g_priv->dpc2_dt_usage[8].en = 1;
		g_priv->dpc2_dt_usage[11].en = 1;
		g_priv->dpc2_dt_usage[17].en = 1;
		g_priv->dpc2_dt_usage[32].en = 1;
		g_priv->dpc2_dt_usage[39].en = 1;
		g_priv->dpc2_dt_usage[45].en = 1;
	} else {
		g_priv->dpc2_dt_usage[0].en = 0;
		g_priv->dpc2_dt_usage[4].en = 0;
		g_priv->dpc2_dt_usage[8].en = 0;
		g_priv->dpc2_dt_usage[11].en = 0;
		g_priv->dpc2_dt_usage[17].en = 0;
		g_priv->dpc2_dt_usage[32].en = 0;
		g_priv->dpc2_dt_usage[39].en = 0;
		g_priv->dpc2_dt_usage[45].en = 0;
	}
}

static void dpc_enable_v3(const u8 en)
{
	//DPCFUNC("en(%u)", en);

	u16 i = 0;

	if (en == 2) {
		g_priv->vidle_mask = 0;
		debug_irq = 0;
	}

	if (en) {
		dt_strategy_decision();
		for (i = 0; i < DPC2_VIDLE_CNT; i ++) {	/* TODO: priv->dt_cnt */
			if (g_priv->dpc2_dt_usage[i].en) {
				dpc2_dt_en(i, true, false);
				dpc_dt_set_update(i, g_priv->dpc2_dt_usage[i].val);
			}
		}

		/* sw trig en reserved DT for gce exception voter debug */
		dpc2_dt_en(30, true, true);
		dpc2_dt_en(55, true, true);
		dpc2_dt_en(56, true, true);

		/* select GCE EVENT */
		writel(230, dpc_base + DISP_DPC_EVENT_SEL_G7);

		writel(99 |		// disp_dt_te_sel[0]: trigger_te0
		       0 << 8 |		// disp_dt_te_sel[1]: unused
		       0 << 16 |	// disp_dt_te_sel[2]: unused
		       0 << 24,		// disp_dt_te_sel[3]: unused
		       dpc_base + DISP_DPC_DISP_DT_TE_MON_SEL_G0);
		writel(55 |		// disp_dt_te_sel[4]: dt_done55
		       56 << 8 |	// disp_dt_te_sel[5]: dt_done56
		       34 << 16 |	// disp_dt_te_sel[6]: dt_done34_0
		       30 << 24,	// disp_dt_te_sel[7]: dt_done30
		       dpc_base + DISP_DPC_DISP_DT_TE_MON_SEL_G1);

		if (debug_irq & BIT(0))
			writel(0xffffffff, dpc_base + DISP_DPC_INTEN_DISP_PM_CFG_ERROR);

		if (debug_irq & BIT(1))
			writel(INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DT_TE_SEL_0,	/* trigger_te0 */
			       dpc_base + DISP_DPC_INTEN_DT_TE_THREAD);
		else if (debug_irq & BIT(2))
			writel(INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DT_TE_SEL_0 |	/* trigger_te0 */
			       INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DT_TE_SEL_6,	/* dt_done34_0 */
			       dpc_base + DISP_DPC_INTEN_DT_TE_THREAD);
		else if (debug_irq & BIT(7))
			writel(INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DT_TE_SEL_0 |	/* trigger_te0 */
			       INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DT_TE_SEL_4 |	/* dt_done55 */
			       INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DT_TE_SEL_5 |	/* dt_done56 */
			       INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DT_TE_SEL_7,	/* dt_done30 */
			       dpc_base + DISP_DPC_INTEN_DT_TE_THREAD);

		if (debug_irq & BIT(3))
			writel(INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP1A_MTCMOS_ON |
			       INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP1A_MTCMOS_OFF,
			       dpc_base + DISP_DPC_INTEN_MTCMOS_ON_OFF);

		if (debug_irq & BIT(8))
			writel(INTSTA_MML2_HWVOTE_SIGN_HI |
			       INTSTA_MML2_HWVOTE_SIGN_LO,
			       dpc_base + DISP_DPC_INTEN_HWVOTE_STATE);

		if (debug_irq & BIT(4))	/* mminfra DT*/
			writel(BIT(24) | BIT(25) | BIT(26),
			       dpc_base + DISP_DPC_INTEN_MTCMOS_BUSY);

		if (debug_irq & BIT(5))	/* mminfra on off rdy rising */
			writel(BIT(20) | BIT(21),
			       dpc_base + DISP_DPC_INTEN_INTF_PWR_RDY_STATE);

		writel(g_priv->dt_follow_cfg, dpc_base + DISP_REG_DPC_DISP_DT_FOLLOW_CFG);
		writel(g_priv->dt_follow_cfg, dpc_base + DISP_REG_DPC_MML_DT_FOLLOW_CFG);
		writel(0x40, dpc_base + DISP_DPC_ON2SOF_DT_EN);
		writel(0xf, dpc_base + DISP_DPC_ON2SOF_DSI0_SOF_COUNTER); /* should > 2T, cannot be zero */
		// writel(0x2, dpc_base + DISP_REG_DPC_DEBUG_SEL);	/* mtcmos_debug */

		// writel(0x18240, dpc_base + DISP_REG_DPC_MML_DT_CFG); /* MML dsi frame busy switch to unused DSI3 */
		writel(0x3, dpc_base + DISP_REG_DPC_DISP_POWER_STATE_CFG); /* MML busy select to unused RDMA1 */

		/* wla ddren ack */
		// writel(1, dpc_base + DISP_REG_DPC_DDREN_ACK_SEL);
		// writel(0x3F3F0000, dpc_base + 0xBC); /* DISP_DPC_DDREN_URGENT_SW_CTRL */

		/* [0] urgent from 1:MML_DDREN_URG & DISP1_HRT_URG, 0:all subsys */
		/* [1] disp_vcore controlled by 1:DPC_EN, 0:DUMMY1 */
		writel(0x3, dpc_base + DISP_REG_DPC_DUMMY0);

		writel(0, dpc_base + DISP_REG_DPC_MERGE_DISP_INT_CFG);
		writel(0, dpc_base + DISP_REG_DPC_MERGE_DISP_INTSTA);
		writel(0, dpc_base + DISP_REG_DPC_MERGE_MML_INT_CFG);
		writel(0, dpc_base + DISP_REG_DPC_MERGE_MML_INTSTA);

		/* enable external signal from DSI and TE */
		writel(0x3F, dpc_base + DISP_REG_DPC_DISP_EXT_INPUT_EN);
		writel(0x11, dpc_base + DISP_REG_DPC_MML_EXT_INPUT_EN);

		writel(g_priv->dt_follow_cfg, dpc_base + DISP_REG_DPC_DISP_DT_FOLLOW_CFG);
		writel(g_priv->dt_follow_cfg, dpc_base + DISP_REG_DPC_MML_DT_FOLLOW_CFG);

		/* DISP_DPC_EN | DISP_DPC_DT_EN | DISP_DPC_MMQOS_ALWAYS_SCAN_EN */
		writel(0x13 | (en == 2 ? BIT(16) : 0), dpc_base + DISP_REG_DPC_EN);
		dpc_mmp(config, MMPROFILE_FLAG_PULSE, U32_MAX, 1);

		if (mask_busy_irq) {
			writel(~0xffffff00, busy_mask[0]);		/* disp0a: dlo, dli */
			writel(~(0x1fff | 0x182000), busy_mask[1]);	/* disp0a: mdp_rsz, oddmr, dlo, relay */
			writel(~0x7ffffff, busy_mask[2]);		/* disp1a: dlo, dli */
			writel(~0x700, busy_mask[3]);			/* disp1a: chist */
			writel(~0xffff0, busy_mask[4]);			/* ovl0: dlo */
			writel(0, busy_mask[5]);	/* disp0b irq */
			writel(0, busy_mask[6]);	/* disp1b irq */
			writel(0, busy_mask[7]);	/* ovl0 irq */
			writel(0, busy_mask[8]);	/* ovl1 irq */
			writel(0, busy_mask[9]);	/* ovl2 irq */
		}
	} else {
		/* [0] disp_vcore SW_CTRL */
		/* [1] disp_vcore SW_CTRL_VALUE */
		writel(0x1, dpc_base + DISP_REG_DPC_DUMMY1);

		/* disable inten to avoid burst irq */
		// writel(0, dpc_base + DISP_REG_DPC_DISP_INTEN);
		// writel(0, dpc_base + DISP_REG_DPC_MML_INTEN);
		writel(0, dpc_base + DISP_REG_DPC_EN);

		dpc_mmp(config, MMPROFILE_FLAG_PULSE, U32_MAX, 0);
	}

	/* enable gce event */
	writel(en, dpc_base + DISP_DPC_EVENT_CFG);

	g_priv->enabled = en;
}

static u8 bw_to_level_v3(const u32 total_bw)
{
	int i;

	for (i = 0 ; i < step_size ; i++)
		if (total_bw <= g_urate_freq_steps[i])
			return i;

	mtk_dprec_logger_pr(DPREC_LOGGER_ERROR, "total_bw:%u > max_bw:%lu\n",
		total_bw, g_urate_freq_steps[step_size - 1]);

	return step_size - 1;
}

static u8 dpc_max_dvfs_level(void)
{
	/* find max(disp level, mml level, bw level) */
	u8 max_level = g_priv->dvfs_bw.disp_level > g_priv->dvfs_bw.bw_level?
		       g_priv->dvfs_bw.disp_level : g_priv->dvfs_bw.bw_level;

	return max_level > g_priv->dvfs_bw.mml_level ? max_level : g_priv->dvfs_bw.mml_level;
}

static void dpc_hrt_bw_set_v2(const u32 subsys, const u32 bw_in_mb, bool force)
{
	u32 total_bw = bw_in_mb;

	if (unlikely(g_priv->total_hrt_unit == 0))
		return;

	/* U32_MAX means no need to update, just read */
	mutex_lock(&g_priv->dvfs_bw.lock);
	if (bw_in_mb != U32_MAX) {
		if (subsys == DPC_SUBSYS_DISP)
			g_priv->dvfs_bw.disp_bw[DPC_TOTAL_HRT] = bw_in_mb;
		else if (subsys == DPC_SUBSYS_MML0)
			g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_HRT] = bw_in_mb;
		else if (subsys == DPC_SUBSYS_MML1)
			g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_HRT] = bw_in_mb;
	}
	total_bw = g_priv->dvfs_bw.disp_bw[DPC_TOTAL_HRT] +
		   g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_HRT] +
		   g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_HRT];
	mutex_unlock(&g_priv->dvfs_bw.lock);

	if (unlikely(debug_dvfs)) {
		if (bw_in_mb == U32_MAX)
			DPCFUNC("subsys(%u) updated(%u,%u,%u)", subsys,
				g_priv->dvfs_bw.disp_bw[DPC_TOTAL_HRT],
				g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_HRT],
				g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_HRT]);
		else
			DPCFUNC("subsys(%u) bw_in_mb(%u) trigger(%u) updated(%u,%u,%u)", subsys, bw_in_mb, force,
				g_priv->dvfs_bw.disp_bw[DPC_TOTAL_HRT],
				g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_HRT],
				g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_HRT]);
	}
	dpc_mmp(hrt_bw, MMPROFILE_FLAG_PULSE, subsys << 16 | force, bw_in_mb);

	if (!force)
		return;

	/* trigger dram dvfs first */
	writel(total_bw * 10000 / g_priv->hrt_emi_efficiency / g_priv->total_hrt_unit,
		dpc_base + DISP_REG_DPC_DISP_HIGH_HRT_BW);

	/* trigger vdisp dvfs */
	dpc_dvfs_set_v2(DPC_SUBSYS_DISP, 0, false);
}

static void dpc_hrt_bw_set_v3(const u32 subsys, const u32 bw_in_mb, bool force)
{
	u32 total_bw = bw_in_mb;

	if (unlikely(g_priv->total_hrt_unit == 0))
		return;

	/* U32_MAX means no need to update, just read */
	mutex_lock(&g_priv->dvfs_bw.lock);
	if (bw_in_mb != U32_MAX) {
		if (subsys == 0)
			g_priv->dvfs_bw.disp_bw[DPC_TOTAL_HRT] = bw_in_mb;
		else if (subsys == DPC3_SUBSYS_MML0)
			g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_HRT] = bw_in_mb;
		else if (subsys == DPC3_SUBSYS_MML1)
			g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_HRT] = bw_in_mb;
		else if (subsys == DPC3_SUBSYS_MML2)
			g_priv->dvfs_bw.mml2_bw[DPC_TOTAL_HRT] = bw_in_mb;
	}
	total_bw = g_priv->dvfs_bw.disp_bw[DPC_TOTAL_HRT] +
		   g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_HRT] +
		   g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_HRT] +
		   g_priv->dvfs_bw.mml2_bw[DPC_TOTAL_HRT];
	mutex_unlock(&g_priv->dvfs_bw.lock);

	if (unlikely(debug_dvfs)) {
		if (bw_in_mb == U32_MAX)
			DPCFUNC("subsys(%u) updated(%u,%u,%u,%u)", subsys,
				g_priv->dvfs_bw.disp_bw[DPC_TOTAL_HRT],
				g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_HRT],
				g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_HRT],
				g_priv->dvfs_bw.mml2_bw[DPC_TOTAL_HRT]);
		else
			DPCFUNC("subsys(%u) bw_in_mb(%u) trigger(%u) updated(%u,%u,%u,%u)", subsys, bw_in_mb, force,
				g_priv->dvfs_bw.disp_bw[DPC_TOTAL_HRT],
				g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_HRT],
				g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_HRT],
				g_priv->dvfs_bw.mml2_bw[DPC_TOTAL_HRT]);
	}
	dpc_mmp(hrt_bw, MMPROFILE_FLAG_PULSE, subsys << 16 | force, bw_in_mb);

	if (!force)
		return;

	/* trigger dram dvfs first */
	/* read and clear hrt bw */
	u32 value = readl(dpc_base + g_priv->ch_bw_cfg[24].offset) & ~(0x1fff << g_priv->ch_bw_cfg[24].shift);

	/* update hrt bw */
	value |= ((total_bw * 10000 / g_priv->hrt_emi_efficiency / g_priv->total_hrt_unit)
			<< g_priv->ch_bw_cfg[24].shift);

	/* report both dram and emi */
	writel(value, dpc_base + g_priv->ch_bw_cfg[24].offset);
	writel(value, dpc_base + g_priv->ch_bw_cfg[26].offset);

	/* trigger vdisp dvfs */
	dpc_dvfs_set_v2(DPC_SUBSYS_DISP, 0, false);
}

static void dpc_srt_bw_set_v2(const u32 subsys, const u32 bw_in_mb, bool force)
{
	u32 total_bw = bw_in_mb;

	if (g_priv->total_srt_unit == 0)
		return;

	/* U32_MAX means no need to update, just read */
	mutex_lock(&g_priv->dvfs_bw.lock);
	if (bw_in_mb != U32_MAX) {
		if (subsys == DPC_SUBSYS_DISP)
			g_priv->dvfs_bw.disp_bw[DPC_TOTAL_SRT] = bw_in_mb;
		else if (subsys == DPC_SUBSYS_MML0)
			g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_SRT] = bw_in_mb;
		else if (subsys == DPC_SUBSYS_MML1)
			g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_SRT] = bw_in_mb;
	}
	total_bw = g_priv->dvfs_bw.disp_bw[DPC_TOTAL_SRT] +
		   g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_SRT] +
		   g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_SRT];
	mutex_unlock(&g_priv->dvfs_bw.lock);

	if (unlikely(debug_dvfs)) {
		if (bw_in_mb == U32_MAX)
			DPCFUNC("subsys(%u) updated(%u,%u,%u)", subsys,
				g_priv->dvfs_bw.disp_bw[DPC_TOTAL_SRT],
				g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_SRT],
				g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_SRT]);
		else
			DPCFUNC("subsys(%u) bw_in_mb(%u) trigger(%u) updated(%u,%u,%u)", subsys, bw_in_mb, force,
				g_priv->dvfs_bw.disp_bw[DPC_TOTAL_SRT],
				g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_SRT],
				g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_SRT]);
	}
	dpc_mmp(srt_bw, MMPROFILE_FLAG_PULSE, subsys << 16 | force, bw_in_mb);

	if (!force)
		return;

	writel(total_bw * g_priv->srt_emi_efficiency / 10000 / g_priv->total_srt_unit,
		dpc_base + DISP_REG_DPC_DISP_SW_SRT_BW);
}

static void dpc_srt_bw_set_v3(const u32 subsys, const u32 bw_in_mb, bool force)
{
	u32 total_bw = bw_in_mb;

	if (g_priv->total_srt_unit == 0)
		return;

	/* U32_MAX means no need to update, just read */
	mutex_lock(&g_priv->dvfs_bw.lock);
	if (bw_in_mb != U32_MAX) {
		if (subsys == 0)
			g_priv->dvfs_bw.disp_bw[DPC_TOTAL_SRT] = bw_in_mb;
		else if (subsys == DPC3_SUBSYS_MML0)
			g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_SRT] = bw_in_mb;
		else if (subsys == DPC3_SUBSYS_MML1)
			g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_SRT] = bw_in_mb;
		else if (subsys == DPC3_SUBSYS_MML2)
			g_priv->dvfs_bw.mml2_bw[DPC_TOTAL_SRT] = bw_in_mb;
	}
	total_bw = g_priv->dvfs_bw.disp_bw[DPC_TOTAL_SRT] +
		   g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_SRT] +
		   g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_SRT] +
		   g_priv->dvfs_bw.mml2_bw[DPC_TOTAL_SRT];
	mutex_unlock(&g_priv->dvfs_bw.lock);

	if (unlikely(debug_dvfs)) {
		if (bw_in_mb == U32_MAX)
			DPCFUNC("subsys(%u) updated(%u,%u,%u,%u)", subsys,
				g_priv->dvfs_bw.disp_bw[DPC_TOTAL_SRT],
				g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_SRT],
				g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_SRT],
				g_priv->dvfs_bw.mml2_bw[DPC_TOTAL_SRT]);
		else
			DPCFUNC("subsys(%u) bw_in_mb(%u) trigger(%u) updated(%u,%u,%u,%u)", subsys, bw_in_mb, force,
				g_priv->dvfs_bw.disp_bw[DPC_TOTAL_SRT],
				g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_SRT],
				g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_SRT],
				g_priv->dvfs_bw.mml2_bw[DPC_TOTAL_SRT]);
	}
	dpc_mmp(srt_bw, MMPROFILE_FLAG_PULSE, subsys << 16 | force, bw_in_mb);

	if (!force)
		return;

	/* read and clear srt bw */
	u32 value = readl(dpc_base + g_priv->ch_bw_cfg[25].offset) & ~(0x1fff << g_priv->ch_bw_cfg[25].shift);

	/* update srt bw */
	value |= ((total_bw * g_priv->srt_emi_efficiency / 10000 / g_priv->total_srt_unit)
			<< g_priv->ch_bw_cfg[25].shift);

	/* report both dram and emi */
	writel(value, dpc_base + g_priv->ch_bw_cfg[25].offset);
	writel(value, dpc_base + g_priv->ch_bw_cfg[27].offset);
}

static int vdisp_level_set_vcp(const u32 subsys, const u8 level)
{
	int ret = 0;
	u32 value = 0, swreq = 0;

	if (vdisp_dvfsrc_sw_req4) {
		swreq = readl(vdisp_dvfsrc_sw_req4);
		if (swreq & 0x70) {
			DPCFUNC("clear SW_REQ4(%#x) before vote level(%u)", swreq, level);
			writel(swreq & ~0x70, vdisp_dvfsrc_sw_req4);
		}
	}

	/* polling vdisp dvfsrc idle */
	if (g_priv->vdisp_dvfsrc) {
		ret = readl_poll_timeout(g_priv->vdisp_dvfsrc, value,
					 (value & g_priv->vdisp_dvfsrc_idle_mask) == 0, 1, 500);
		if (ret < 0)
			DPCERR("subsys(%d) wait vdisp dvfsrc idle timeout", subsys);
	}
	writel(level, dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_VAL);

	if (debug_dvfs)
		DPCFUNC("level(%u) disp_sel(%#x) RC_STA(%#010x) SW_REQ4(%#x)\n", level,
			clk_disp_sel ? readl(clk_disp_sel) : 0,
			g_priv->vdisp_dvfsrc ? readl(g_priv->vdisp_dvfsrc) : 0,
			vdisp_dvfsrc_sw_req4 ? ((readl(vdisp_dvfsrc_sw_req4) & 0x70) >> 4) : 0);

	return ret;
}

static void dpc_dvfs_set_v2(const u32 subsys, const u8 level, bool update_level)
{
	u32 mmdvfs_user = U32_MAX;
	u8 max_level;

	if (update_level && (level > g_priv->vdisp_level_cnt)) {
		DPCERR("vdisp support only %u levels", g_priv->vdisp_level_cnt);
		return;
	}

	mutex_lock(&g_priv->dvfs_bw.lock);

	if (update_level) {
		if (subsys == DPC_SUBSYS_DISP)
			g_priv->dvfs_bw.disp_level = level;
		else
			g_priv->dvfs_bw.mml_level = level;
	}

	max_level = dpc_max_dvfs_level();
	if (max_level < level)
		max_level = level;
	vdisp_level_set_vcp(subsys, max_level);

	mutex_unlock(&g_priv->dvfs_bw.lock);

	/* add vdisp info to met */
	if (MEM_BASE) {
		mmdvfs_user = (subsys == DPC_SUBSYS_DISP) ? MMDVFS_USER_DISP : MMDVFS_USER_MML;
		writel(g_priv->vdisp_level_cnt - max_level, MEM_USR_OPP(mmdvfs_user, false));
	}

	dpc_mmp(vdisp_level, MMPROFILE_FLAG_PULSE,
		g_priv->dvfs_bw.disp_bw[DPC_TOTAL_HRT] << 16 |
		g_priv->dvfs_bw.mml0_bw[DPC_TOTAL_HRT] +
		g_priv->dvfs_bw.mml1_bw[DPC_TOTAL_HRT] +
		g_priv->dvfs_bw.mml2_bw[DPC_TOTAL_HRT],
		((unsigned long)g_priv->dvfs_bw.disp_level) << 24 |
		((unsigned long)g_priv->dvfs_bw.mml_level) << 16 |
		((unsigned long)g_priv->dvfs_bw.bw_level) << 8 |
		(unsigned long)max_level);

	if (unlikely(debug_dvfs))
		DPCFUNC("subsys(%u) level(%u,%u,%u)", subsys,
			g_priv->dvfs_bw.disp_level, g_priv->dvfs_bw.mml_level, g_priv->dvfs_bw.bw_level);
}

static void dpc_ch_bw_set_v2(const u32 subsys, const u8 idx, const u32 bw_in_mb)
{
	u32 value = 0;
	u32 ch_bw = bw_in_mb;

	if (unlikely(mminfra_floor && bw_in_mb && (bw_in_mb < mminfra_floor * 16)))
		ch_bw = mminfra_floor * 16;

	if (idx < 24) {
	/* use display voter for both display and mml, since mml voter is reserved for others */
		value = readl(dpc_base + g_priv->ch_bw_cfg[idx].offset) & ~(0x3ff << g_priv->ch_bw_cfg[idx].shift);
		value |= (ch_bw * 100 / g_priv->ch_bw_urate / 16) << g_priv->ch_bw_cfg[idx].shift;

		if (unlikely(debug_dvfs))
			DPCFUNC("subsys(%u) idx(%u) bw(%u)MB", subsys, idx, ch_bw);

		writel(value, dpc_base + g_priv->ch_bw_cfg[idx].offset);
		dpc_mmp(ch_bw, MMPROFILE_FLAG_PULSE, idx, ch_bw);
	} else {
		DPCERR("idx %u > 24", idx);
		return;
	}
}

static void dpc_channel_bw_set_by_idx_v2(const u32 subsys, const u8 idx, const u32 bw_in_mb)
{
	u32 ch_bw = bw_in_mb;
	u32 cur_ch_bw = 0;
	u32 max_ch_bw = 0;
	int i = 0;

	mutex_lock(&g_priv->dvfs_bw.lock);
	cur_ch_bw = g_priv->ch_bw_cfg[idx].disp_bw + g_priv->ch_bw_cfg[idx].mml_bw;

	if (subsys == DPC_SUBSYS_DISP)
		g_priv->ch_bw_cfg[idx].disp_bw = bw_in_mb;
	else
		g_priv->ch_bw_cfg[idx].mml_bw = bw_in_mb;

	ch_bw = g_priv->ch_bw_cfg[idx].disp_bw + g_priv->ch_bw_cfg[idx].mml_bw;
	mutex_unlock(&g_priv->dvfs_bw.lock);

	if (ch_bw == cur_ch_bw)
		return;

	dpc_mmp(ch_bw, MMPROFILE_FLAG_PULSE, BIT(subsys) << 16 | idx, bw_in_mb << 16 | ch_bw);
	dpc_ch_bw_set_v2(subsys, idx, ch_bw);

	mutex_lock(&g_priv->dvfs_bw.lock);
	for (i = 0; i < 24; i++) {
		ch_bw = g_priv->ch_bw_cfg[i].disp_bw + g_priv->ch_bw_cfg[i].mml_bw;
		if (ch_bw > max_ch_bw)
			max_ch_bw = ch_bw;
	}
	g_priv->dvfs_bw.bw_level = bw_to_level_v3(max_ch_bw);
	mutex_unlock(&g_priv->dvfs_bw.lock);
}

static void dpc_dvfs_trigger(const char *caller)
{
	g_priv->hrt_bw_set(DPC_SUBSYS_MML, U32_MAX, true);
	g_priv->srt_bw_set(DPC_SUBSYS_MML, U32_MAX, true);

	if (unlikely(debug_dvfs))
		DPCFUNC("by %s", caller);
}

static void mt6991_set_mtcmos(const u32 subsys, const enum mtk_dpc_mtcmos_mode mode)
{
	bool en = (mode == DPC_MTCMOS_AUTO ? true : false);
	int ret = 0;
	u32 value = 0;
	u32 rtff_mask = 0;
	u8 power_on = dpc_is_power_on_v2() | mminfra_is_power_on_v2() << 1;

	if (g_priv == NULL) {
		DPCERR("g_priv null\n");
		return;
	}
	if (subsys >= g_priv->subsys_cnt) {
		DPCERR("not support subsys(%u)", subsys);
		return;
	}
	if (power_on != 0b11) {
		static bool called;

		mtk_dprec_logger_pr(DPREC_LOGGER_ERROR, "subsys(%u) en(%u) skipped due to no power(%#x)\n",
				    subsys, en, power_on);
		if (!called) {
			called = true;
			DPCERR("subsys(%u) en(%u) skipped due to no power(%#x)",
				    subsys, en, power_on);
			dump_stack();
		}
		return;
	}
	dpc_mmp(mtcmos_auto, MMPROFILE_FLAG_PULSE, subsys, en);

	if (subsys == DPC_SUBSYS_DISP)
		rtff_mask = 0xf00;
	else if (subsys == DPC_SUBSYS_MML1)
		rtff_mask = BIT(15);
	else if (subsys == DPC_SUBSYS_MML0)
		rtff_mask = BIT(14);

	/* [SWITCH TO DPC AUTO MODE]
	 *   1. unset bootup (enable rtff)
	 *   2. enable AUTO_ONOFF_MASTER_EN
	 * [SWITCH TO MANUAL MODE]
	 *   1. vote thread
	 *   2. wait until mtcmos is ON_ACT state
	 *   3. disable AUTO_ONOFF_MASTER_EN
	 *   4. set bootup (disable rtff)
	 *   5. unvote thread
	 */
	if (mode == DPC_MTCMOS_AUTO) {
		if (g_priv->rtff_pwr_con && has_cap(DPC_CAP_MTCMOS))
			writel(readl(g_priv->rtff_pwr_con) & ~rtff_mask, g_priv->rtff_pwr_con);
	} else {
		g_priv->mtcmos_vote(subsys, 5, true);

		if (g_priv->mtcmos_cfg[subsys].mode == DPC_MTCMOS_AUTO) {
			/* MTCMOS_STA [20]ON_ACT [21]OFF_IDLE [22]RUNNING */
			ret = readl_poll_timeout_atomic(g_priv->mtcmos_cfg[subsys].chk_va,
							value, value & BIT(20), 1, 200);
			if (ret < 0)
				DPCERR("wait subsys(%d) ON_ACT state timeout", subsys);
		}
	}

	value = (en && has_cap(DPC_CAP_MTCMOS)) ? 0x31 : 0x70;
	if (subsys == DPC_SUBSYS_DISP) {
		writel(value, dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_DIS1].cfg);
		writel(value, dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_DIS0].cfg);
		writel(value, dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_OVL0].cfg);
		writel(value, dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_OVL1].cfg);
	} else if (subsys == DPC_SUBSYS_MML1) {
		writel(value, dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_MML1].cfg);
	} else if (subsys == DPC_SUBSYS_MML0) {
		writel(value, dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_MML0].cfg);
	}
	g_priv->mtcmos_cfg[subsys].mode = mode;

	if (!en) {
		if (g_priv->rtff_pwr_con && has_cap(DPC_CAP_MTCMOS))
			writel(readl(g_priv->rtff_pwr_con) | rtff_mask, g_priv->rtff_pwr_con);
		g_priv->mtcmos_vote(subsys, 5, false);
	}

	dpc_mmp(mtcmos_auto, MMPROFILE_FLAG_PULSE, subsys, readl(g_priv->rtff_pwr_con));
}

static void mt6993_set_mtcmos(const u32 subsys, const enum mtk_dpc_mtcmos_mode mode)
{
#if IS_ENABLED(CONFIG_MTK_HWCCF)
	int ret;
	bool en = has_cap(DPC_CAP_MTCMOS) ? (mode == DPC_MTCMOS_AUTO ? true : false) : false;
	u32 value = 0, temp;
	u32 mask = 0;
	u16 i = 0;
	unsigned long flags;

	if (g_priv == NULL) {
		DPCERR("g_priv null\n");
		return;
	}
	dpc_mmp(mtcmos_auto, MMPROFILE_FLAG_PULSE, subsys, en);

	/* [SWITCH TO DPC AUTO MODE]
	 *   1. enable AUTO_ONOFF_MASTER_EN
	 *   2. break the cg-link
	 * [SWITCH TO MANUAL MODE]
	 *   1. rebuild the cg-link
	 *   2. force DT fsm to OFF state by MANUAL_MODE_EN and MANUAL_MODE(2)
	 *   3. disable AUTO_ONOFF_MASTER_EN
	 *   4. enable RTFF_EN and disable SRAM_SLEEP_PD
	 */

	value = en ? 0x33 : 0x552;
	mask = (subsys == DPC3_SUBSYS_DISP) ? 0x3f80000 : BIT(g_priv->mtcmos_cfg[subsys].link_bit);

	spin_lock_irqsave(&g_priv->mtcmos_cfg_lock, flags);

	if (en) {
		if (subsys == DPC3_SUBSYS_DISP) {
			for (i = 0; i < 7; i++)
				writel(value, dpc_base + g_priv->mtcmos_cfg[i].cfg);
		} else if (subsys < g_priv->subsys_cnt)
			writel(value, dpc_base + g_priv->mtcmos_cfg[subsys].cfg);

		writel(mask, hwccf_dummy_set);
		ret = readl_poll_timeout_atomic(hwccf_dummy_en, temp, temp & mask, 1, 2000);
		if (ret < 0)
			DPCERR("polling unlink timeout %d", __LINE__);
	} else {
		writel(mask, hwccf_dummy_clr);
		ret = readl_poll_timeout_atomic(hwccf_dummy_en, temp, !(temp & mask), 1, 2000);
		if (ret < 0)
			DPCERR("polling relink timeout %d", __LINE__);

		if (subsys == DPC3_SUBSYS_DISP) {
			for (i = 0; i < 7; i++)
				writel(value, dpc_base + g_priv->mtcmos_cfg[i].cfg);
		} else if (subsys < g_priv->subsys_cnt)
			writel(value, dpc_base + g_priv->mtcmos_cfg[subsys].cfg);
	}

	/* toggle hwccf cg fsm */
	ret = hwccf_voter_ctrl(MM_HWCCF, HW_CCF_CG_GRP_10, HWCCF_VOTE, 30);
	if (ret < 0)
		DPCERR("vote vdisp dummy cg failed(%d)", ret);
	ret = hwccf_voter_ctrl(MM_HWCCF, HW_CCF_CG_GRP_10, HWCCF_UNVOTE, 30);
	if (ret < 0)
		DPCERR("unvote vdisp dummy cg failed(%d)", ret);

	if (subsys == DPC3_SUBSYS_DISP)
		g_priv->mtcmos_cfg[DPC3_SUBSYS_DIS1A].mode = (enum mtk_dpc_mtcmos_mode)en;
	else if (subsys < g_priv->subsys_cnt)
		g_priv->mtcmos_cfg[subsys].mode = (enum mtk_dpc_mtcmos_mode)en;

	spin_unlock_irqrestore(&g_priv->mtcmos_cfg_lock, flags);

	dpc_mmp(mtcmos_auto, MMPROFILE_FLAG_PULSE, subsys, en);

	if (ret < 0)
		clkchk_external_dump();
#endif
}

static void dpc_dsi_pll_set_v2(const u32 value)
{
	if (g_priv == NULL) {
		DPCERR("g_priv null\n");
		return;
	}

	/* if DSI_PLL_SEL is set, power ON disp1 and set DSI_CK_KEEP_EN */
	if (g_priv->dsi_ck_keep_mask && (value & BIT(0))) {
		mtk_disp_vlp_vote_v2(VOTE_SET, DISP_VIDLE_USER_DISP_DPC_CFG);
		/* will be cleared when ff enable */

		dpc_wait_pwr_ack_v2(DPC_SUBSYS_DIS1);
		writel(g_priv->dsi_ck_keep_mask, dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_DIS1].cfg);
	}
}

static void dpc_disp_group_enable(bool en)
{
	u32 value = 0;

	if (g_priv == NULL) {
		DPCERR("g_priv null\n");
		return;
	}

	/* DDR_SRC and EMI_REQ DT is follow DISP1 */
	value = (en && has_cap(DPC_CAP_APSRC)) ? 0x01010101 : 0x0D0D0D0D;
	writel(value, dpc_base + DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG);

	/* channel bw DT is follow SRT*/
	value = (en && has_cap(DPC_CAP_QOS)) ? 0 : 0x00010001;
	writel(value, dpc_base + DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG);

	/* lower vdisp level */
	value = (en && has_cap(DPC_CAP_VDISP)) ? 0 : 1;
	writel(value, dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_CFG);

	/* mminfra request */
	writel(0x00080008, dpc_base + DISP_DPC_MMINFRA_HWVOTE_CFG);
	value = (en && has_cap(DPC_CAP_MMINFRA_PLL)) ? 0x000200 : 0x1a0a1a;
	writel(value, dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG);

	/* dsi pll auto */
	value = (en && has_cap(DPC_CAP_DSI)) ? 0x11 : 0x1;
	writel(value, dpc_base + DISP_DPC_MIPI_SODI5_EN);

	/* vcore off */
	value = (en && has_cap(DPC_CAP_PMIC_VCORE)) ? 0x180000 : 0x181e1e;
	writel(value, dpc_base + DISP_DPC2_DISP_26M_PMIC_VCORE_OFF_CFG);
}

static void dpc_mml_group_enable(bool en)
{
	u32 value = 0;

	if (g_priv == NULL) {
		DPCERR("g_priv null\n");
		return;
	}

	/* DDR_SRC and EMI_REQ DT is follow MML1 */
	value = (en && has_cap(DPC_CAP_APSRC)) ? 0x01010101 : 0x0D0D0D0D;
	writel(value, dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG);

	/* lower vdisp level */
	value = (en && has_cap(DPC_CAP_VDISP)) ? 0 : 1;
	writel(value, dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_CFG);

	/* channel bw DT is follow SRT*/
	value = (en && has_cap(DPC_CAP_QOS)) ? 0 : 0x00010001;
	writel(value, dpc_base + DISP_REG_DPC_MML_HRTBW_SRTBW_CFG);

	/* mminfra request */
	writel(0x00080008, dpc_base + DISP_DPC_MMINFRA_HWVOTE_CFG);
	value = (en && has_cap(DPC_CAP_MMINFRA_PLL)) ? 0x000a00 : 0x1a0a1a; /* mminfrq req always off */
	writel(value, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);

	/* vcore off */
	value = (en && has_cap(DPC_CAP_PMIC_VCORE)) ? 0x180000 : 0x181e1e;
	writel(value, dpc_base + DISP_DPC2_MML_26M_PMIC_VCORE_OFF_CFG);
}

void dpc_group_enable_v2(const u16 group, bool en)
{
	if (group == DPC_SUBSYS_DISP)
		dpc_disp_group_enable(en);
	else
		dpc_mml_group_enable(en);
}

void dpc_group_enable_v3(const u16 group, bool en)
{
	int ret;
	u32 value = 0;

	if (group == 7777) {
		/* force vote mminfra req */
		writel(0x00080008, dpc_base + DISP_DPC_MMINFRA_HWVOTE_CFG);
		writel(0x0a0a0a, dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG);
		writel(0x0a0a0a, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);

		/* [0] urgent from 1:MML_DDREN_URG & DISP1_HRT_URG, 0:all subsys */
		/* [1] disp_vcore controlled by 1:DPC_EN, 0:DUMMY1 */
		writel(0b01, dpc_base + DISP_REG_DPC_DUMMY0);

		/* [0] disp_vcore SW_CTRL */
		/* [1] disp_vcore SW_CTRL_VALUE */
		writel(0b01, dpc_base + DISP_REG_DPC_DUMMY1);

		/* polling disp vcore req idle */
		ret = readl_poll_timeout_atomic(hwccf_hw_mtcmos_req, value, !(value & BIT(5)), 1, 2000);
		if (ret < 0)
			DPCERR("polling dpc req idle timeout %d", __LINE__);

		/* polling dpc mminfra req idle */
		ret = readl_poll_timeout_atomic(hwccf_hw_irq_req, value, !(value & 0xc), 1, 2000);
		if (ret < 0)
			DPCERR("polling dpc req idle timeout %d", __LINE__);
	} else if (group == DPC_SUBSYS_DISP) {
		if (en) {
			dpc_disp_group_enable(en);

			/* polling dpc mminfra req idle */
			ret = readl_poll_timeout_atomic(hwccf_hw_irq_req, value, !(value & 0xc), 1, 2000);
			if (ret < 0)
				DPCERR("polling dpc req idle timeout %d", __LINE__);

			/* enable off DT */
			dpc_dt_set_update(19, g_priv->dpc2_dt_usage[19].val);
			dpc2_dt_en(19, true, false);
		} else {
			/* polling dpc mminfra req idle */
			ret = readl_poll_timeout_atomic(hwccf_hw_irq_req, value, !(value & 0xc), 1, 2000);
			if (ret < 0)
				DPCERR("polling dpc req idle timeout %d", __LINE__);

			dpc_disp_group_enable(en);

			/* disable off DT */
			dpc2_dt_en(19, false, false);

			/* unmask dpc sideband */
			writel(0, dpc_base + DISP_REG_DPC_DISP_MASK_CFG);
		}
	} else {
		dpc_mml_group_enable(en);
		writel(0, dpc_base + DISP_REG_DPC_DISP_MASK_CFG);
	}
}

static void dpc_config_v2(const u32 subsys, bool en)
{
	static bool is_mminfra_ctrl_by_dpc;

	/* vote power and wait mtcmos on before switch to hw mode */
	mtk_disp_vlp_vote_v2(VOTE_SET, DISP_VIDLE_USER_DISP_DPC_CFG);
	dpc_wait_pwr_ack_v2(DPC_SUBSYS_DIS1);
	dpc_wait_pwr_ack_v2(DPC_SUBSYS_DIS0);
	dpc_wait_pwr_ack_v2(DPC_SUBSYS_OVL0);

	if (!en && is_mminfra_ctrl_by_dpc) {
		if (unlikely(dump_to_kmsg))
			DPCDUMP("dpc get mminfra");
		else
			mtk_dprec_logger_pr(DPREC_LOGGER_FENCE, "dpc get mminfra\n");

		if (dpc_pm_ctrl(true))
			return;
		is_mminfra_ctrl_by_dpc = false;
	}

	writel(en ? 0 : U32_MAX, dpc_base + DISP_REG_DPC_DISP_MASK_CFG);
	writel(en ? 0 : U32_MAX, dpc_base + DISP_REG_DPC_MML_MASK_CFG);

	/* set resource auto or manual mode */
	g_priv->group_enable(DPC_SUBSYS_DISP, en);

	/* set mtcmos auto or manual mode */
	g_priv->set_mtcmos(DPC_SUBSYS_DISP, (enum mtk_dpc_mtcmos_mode)en);

	if (en && has_cap(DPC_CAP_MMINFRA_PLL) && !is_mminfra_ctrl_by_dpc) {
		dpc_pm_ctrl(false);
		is_mminfra_ctrl_by_dpc = true;

		if (unlikely(dump_to_kmsg))
			DPCDUMP("dpc put mminfra");
		else
			mtk_dprec_logger_pr(DPREC_LOGGER_FENCE, "dpc put mminfra\n");
	}

	/* will be unvoted by atomic commit gce pkt, for suspend resuming protection */
	/* mtk_disp_vlp_vote(VOTE_CLR, DISP_VIDLE_USER_DISP_DPC_CFG); */

	dpc_mmp(config, MMPROFILE_FLAG_PULSE, subsys, en);
}

static void dpc_config_v3(const u32 subsys, bool en)
{
	int ret;
	u32 value = 0;
	u32 hwvote_bk = 0;
	static bool is_mminfra_ctrl_by_dpc;

	dpc_vidle_power_keep_v3(DISP_VIDLE_USER_DISP_DPC_CFG);
	if (!en && is_mminfra_ctrl_by_dpc) {
		if (unlikely(dump_to_kmsg))
			DPCDUMP("dpc get mminfra");
		else
			mtk_dprec_logger_pr(DPREC_LOGGER_FENCE, "dpc get mminfra\n");

		if (dpc_mminfra_on_off(true, DISP_VIDLE_USER_NST_LOCK))
			return;
		is_mminfra_ctrl_by_dpc = false;
	}

	dpc_wait_pwr_ack_v3(DPC3_SUBSYS_DISP);

	if (en) {
		/* release forced vote req, SW_CTRL = 0, must clr before master_en */
		writel(0, dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_DIS0A].cfg + 0x30);
		writel(0, dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_DIS0B].cfg + 0x30);
		writel(0, dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_DIS1A].cfg + 0x30);
		writel(0, dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_DIS1B].cfg + 0x30);
		writel(0, dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_OVL0].cfg + 0x30);
		writel(0, dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_OVL1].cfg + 0x30);
		writel(0, dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_OVL2].cfg + 0x30);
		writel(0, dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_MML0].cfg + 0x30);
		writel(0, dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_MML1].cfg + 0x30);
		writel(0, dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_MML2].cfg + 0x30);

		/* master_en = 1, un-link */
		g_priv->set_mtcmos(DPC3_SUBSYS_DISP, (enum mtk_dpc_mtcmos_mode)en);
		g_priv->set_mtcmos(DPC3_SUBSYS_MML0, (enum mtk_dpc_mtcmos_mode)en);
		g_priv->set_mtcmos(DPC3_SUBSYS_MML1, (enum mtk_dpc_mtcmos_mode)en);
		g_priv->set_mtcmos(DPC3_SUBSYS_MML2, (enum mtk_dpc_mtcmos_mode)en);

		/* forced vote req on, for apsrc and emireq */
		writel(1, dpc_base + DISP_REG_DPC3_DTx_SW_TRIG(5));
		writel(1, dpc_base + DISP_REG_DPC3_DTx_SW_TRIG(33));
		ret = readl_poll_timeout_atomic(hwccf_hw_mtcmos_req, value, !(value & 0xffc0), 1, 2000);
		if (ret < 0)
			DPCERR("polling dpc req idle timeout %d", __LINE__);

		/* trig_en = 0 */
		dpc2_dt_en(1, true, false);
		dpc2_dt_en(3, true, false);
		dpc2_dt_en(5, true, false);
		dpc2_dt_en(7, true, false);
		dpc2_dt_en(33, true, false);
		dpc2_dt_en(35, true, false);

		/* set resource auto mode */
		g_priv->group_enable(0, true);
		dpc_mml_group_enable(true);
	} else {
		/* set resource manual mode */
		g_priv->group_enable(0, false);
		dpc_mml_group_enable(false);

		/* trig_en = 1 */
		dpc2_dt_en(1, true, true);
		dpc2_dt_en(3, true, true);
		dpc2_dt_en(5, true, true);
		dpc2_dt_en(7, true, true);
		dpc2_dt_en(33, true, true);
		dpc2_dt_en(35, true, true);
		ret = readl_poll_timeout_atomic(hwccf_hw_mtcmos_req, value, !(value & 0xffc0), 1, 2000);
		if (ret < 0)
			DPCERR("polling dpc req idle timeout %d", __LINE__);

		/* forced vote req off, by SW_CTRL = 1, val = 1 */
		writel(0, dpc_base + DISP_DPC_INTSTA_HWVOTE_STATE);
		writel(BIT(4) | BIT(8), dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_DIS0A].cfg + 0x30);
		writel(BIT(4) | BIT(8), dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_DIS0B].cfg + 0x30);
		writel(BIT(4) | BIT(8), dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_DIS1A].cfg + 0x30);
		writel(BIT(4) | BIT(8), dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_DIS1B].cfg + 0x30);
		writel(BIT(4) | BIT(8), dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_OVL0].cfg + 0x30);
		writel(BIT(4) | BIT(8), dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_OVL1].cfg + 0x30);
		writel(BIT(4) | BIT(8), dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_OVL2].cfg + 0x30);
		writel(BIT(4) | BIT(8), dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_MML0].cfg + 0x30);
		writel(BIT(4) | BIT(8), dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_MML1].cfg + 0x30);
		writel(BIT(4) | BIT(8), dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_MML2].cfg + 0x30);
		ret = readl_poll_timeout_atomic(hwccf_hw_mtcmos_req, value, !(value & 0xffc0), 1, 2000);
		if (ret < 0)
			DPCERR("polling dpc req idle timeout %d", __LINE__);

		/* re-link, master_en = 0 */
		g_priv->set_mtcmos(DPC3_SUBSYS_DISP, (enum mtk_dpc_mtcmos_mode)en);
		g_priv->set_mtcmos(DPC3_SUBSYS_MML0, (enum mtk_dpc_mtcmos_mode)en);
		g_priv->set_mtcmos(DPC3_SUBSYS_MML1, (enum mtk_dpc_mtcmos_mode)en);
		g_priv->set_mtcmos(DPC3_SUBSYS_MML2, (enum mtk_dpc_mtcmos_mode)en);

		/* check hw vote high */
		hwvote_bk = readl(dpc_base + DISP_DPC_INTSTA_HWVOTE_STATE);

		/* forced vote req off, by SW_CTRL = 1, val = 0 */
		writel(0, dpc_base + DISP_DPC_INTSTA_HWVOTE_STATE);
		writel(BIT(4), dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_DIS0A].cfg + 0x30);
		writel(BIT(4), dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_DIS0B].cfg + 0x30);
		writel(BIT(4), dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_DIS1A].cfg + 0x30);
		writel(BIT(4), dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_DIS1B].cfg + 0x30);
		writel(BIT(4), dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_OVL0].cfg + 0x30);
		writel(BIT(4), dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_OVL1].cfg + 0x30);
		writel(BIT(4), dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_OVL2].cfg + 0x30);
		writel(BIT(4), dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_MML0].cfg + 0x30);
		writel(BIT(4), dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_MML1].cfg + 0x30);
		writel(BIT(4), dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_MML2].cfg + 0x30);
		ret = readl_poll_timeout_atomic(hwccf_hw_mtcmos_req, value, !(value & 0xffc0), 1, 2000);
		if (ret < 0)
			DPCERR("polling dpc req idle timeout %d", __LINE__);

		mtk_dprec_logger_pr(DPREC_LOGGER_FENCE, "dpc_cfg HWVOTE(%#x->%#x) SWHW(%#x) 0(%#x) 6(%#x)\n",
			hwvote_bk,
			readl(dpc_base + DISP_DPC_INTSTA_HWVOTE_STATE),
			readl(hwccf_mtcmos_en),
			readl(hwccf_xpu0_local_en),
			readl(hwccf_xpu6_local_en));
	}

	if (en && has_cap(DPC_CAP_MMINFRA_PLL) && !is_mminfra_ctrl_by_dpc) {
		dpc_mminfra_on_off(false, DISP_VIDLE_USER_NST_LOCK);
		is_mminfra_ctrl_by_dpc = true;

		if (unlikely(dump_to_kmsg))
			DPCDUMP("dpc put mminfra");
		else
			mtk_dprec_logger_pr(DPREC_LOGGER_FENCE, "dpc put mminfra\n");
	}
	dpc_vidle_power_release_v3(DISP_VIDLE_USER_DISP_DPC_CFG);
	dpc_mmp(config, MMPROFILE_FLAG_PULSE, subsys, en);
}

irqreturn_t mt6991_irq_handler(int irq, void *dev_id)
{
	struct mtk_dpc *priv = dev_id;
	u32 disp_sta, mml_sta;
	irqreturn_t ret = IRQ_NONE;
	static DEFINE_RATELIMIT_STATE(err_rate, HZ, 1);
	static bool mml_sof_has_begin;
	static bool mml1_has_begin;

	if (IS_ERR_OR_NULL(priv))
		return ret;

	if (!debug_irq) /* avoid irq from being triggered unexpectedly */
		return IRQ_HANDLED;

	dpc_mmp(mminfra, MMPROFILE_FLAG_START, 0x77777777, 1);
	if (dpc_pm_ctrl(true)) {
		dpc_mmp(mminfra, MMPROFILE_FLAG_END, U32_MAX, U32_MAX);
		return IRQ_NONE;
	}
	dpc_mmp(mminfra, MMPROFILE_FLAG_PULSE, 0x77777777, 2);

	disp_sta = readl(dpc_base + DISP_REG_DPC_DISP_INTSTA);
	mml_sta =  readl(dpc_base + DISP_REG_DPC_MML_INTSTA);
	if ((!disp_sta) && (!mml_sta)) {
		dpc_mmp(folder, MMPROFILE_FLAG_PULSE,
			readl(priv->vdisp_ao_cg_con), readl(priv->mminfra_chk));

		if (__ratelimit(&err_rate)) {
			if (unlikely(irq_aee)) {
				DPCAEE("irq err clksq(%u) ulposc(%u) vdisp_ao_cg(%#x) dpc_merge(%#x, %#x)",
				mt_get_fmeter_freq(47, VLPCK), /* clksq */
				mt_get_fmeter_freq(59, VLPCK), /* ulposc */
				readl(priv->vdisp_ao_cg_con),
				readl(dpc_base + DISP_REG_DPC_MERGE_DISP_INT_CFG),
				readl(dpc_base + DISP_REG_DPC_MERGE_MML_INT_CFG));
			} else {
				DPCERR("irq err clksq(%u) ulposc(%u) vdisp_ao_cg(%#x) dpc_merge(%#x, %#x)",
				mt_get_fmeter_freq(47, VLPCK), /* clksq */
				mt_get_fmeter_freq(59, VLPCK), /* ulposc */
				readl(priv->vdisp_ao_cg_con),
				readl(dpc_base + DISP_REG_DPC_MERGE_DISP_INT_CFG),
				readl(dpc_base + DISP_REG_DPC_MERGE_MML_INT_CFG));
			}
		}

		goto out;
	}

	if (disp_sta)
		writel(~disp_sta, dpc_base + DISP_REG_DPC_DISP_INTSTA);
	if (mml_sta)
		writel(~mml_sta, dpc_base + DISP_REG_DPC_MML_INTSTA);

	if (disp_sta & BIT(31) || mml_sta & BIT(31)) {
		u32 disp_err_sta = readl(dpc_base + 0x87c);
		u32 mml_err_sta = readl(dpc_base + 0x880);
		u32 debug_sta = readl(dpc_base + DISP_REG_DPC_DEBUG_STA);

		dpc_mmp(folder, MMPROFILE_FLAG_PULSE, readl(priv->vdisp_ao_cg_con), debug_sta);
		dpc_mmp(folder, MMPROFILE_FLAG_PULSE, disp_err_sta, mml_err_sta);

		if (__ratelimit(&err_rate)) {
			if (unlikely(irq_aee))
				DPCAEE("irq err disp(%#x) mml(%#x) debug(%#x)",
					disp_err_sta, mml_err_sta, debug_sta);
			else
				DPCERR("irq err disp(%#x) mml(%#x) debug(%#x)",
					disp_err_sta, mml_err_sta, debug_sta);
		}
	}

	/* MML1 */
	if (mml_sta & BIT(17) && !mml_sof_has_begin) {
		dpc_mmp(mml_sof, MMPROFILE_FLAG_START, 0, 0);
		mml_sof_has_begin = true;
	}
	if (mml_sta & BIT(18) && mml_sof_has_begin) {
		dpc_mmp(mml_sof, MMPROFILE_FLAG_END, 0, 0);
		mml_sof_has_begin = false;
	}
	if (mml_sta & BIT(14) && !mml1_has_begin) {
		dpc_mmp(mtcmos_mml1, MMPROFILE_FLAG_START, 0, 0);
		tracing_mark_write(trace_buf_mml_on);
		mml1_has_begin = true;
	}
	if (mml_sta & BIT(13) && mml1_has_begin) {
		dpc_mmp(mtcmos_mml1, MMPROFILE_FLAG_END, 0, 0);
		tracing_mark_write(trace_buf_mml_off);
		mml1_has_begin = false;
	}

	/* Panel TE */
	if (disp_sta & BIT(18)) {
		if (mmpc_emi_req && mmpc_ddrsrc_req && spm_ddr_emi_req)
			dpc_mmp(prete, MMPROFILE_FLAG_PULSE,
				(readl(mmpc_emi_req) & 0xffff) << 16 | (readl(mmpc_ddrsrc_req) & 0xffff),
				readl(spm_ddr_emi_req));
		else
			dpc_mmp(prete, MMPROFILE_FLAG_PULSE, 0, 0);
	}
	if (disp_sta & BIT(9)) {
		u32 presz = DPC2_DT_PRESZ;

		if (debug_presz)
			presz = debug_presz;
		dpc_mmp(prete, MMPROFILE_FLAG_PULSE, presz, priv->dpc2_dt_usage[6].val);
	}

	ret = IRQ_HANDLED;
out:
	dpc_pm_ctrl(false);
	dpc_mmp(mminfra, MMPROFILE_FLAG_END, 0x77777777, 3);
	return ret;
}

irqreturn_t mt6993_irq_handler(int irq, void *dev_id)
{
	struct mtk_dpc *priv = dev_id;
	u32 mtcmos_sta = 0, err_sta = 0, dt_sta = 0, mtcmos_busy = 0, pwr_rdy = 0, hwvote_sta = 0;
	irqreturn_t ret = IRQ_NONE;
	static DEFINE_RATELIMIT_STATE(err_rate, HZ, 1);
	static bool disp1_has_begin = true;
	static bool mml2_has_begin = true;

	if (IS_ERR_OR_NULL(priv))
		return ret;

	if (!debug_irq) /* avoid irq from being triggered unexpectedly */
		return IRQ_HANDLED;

	if (!dpc_buck_status(-1)) { /* buck off */
		DPCERR("return, buck off");
		return IRQ_HANDLED;
	}

	if (dpc_mminfra_on_off(true, DISP_VIDLE_USER_DISP_DPC_CFG)) {
		dpc_mmp(mminfra, MMPROFILE_FLAG_END, U32_MAX, U32_MAX);
		return IRQ_NONE;
	}

	hwvote_sta =  readl(dpc_base + DISP_DPC_INTSTA_HWVOTE_STATE);
	mtcmos_busy = readl(dpc_base + DISP_DPC_INTSTA_MTCMOS_BUSY);
	mtcmos_sta = readl(dpc_base + DISP_DPC_INTSTA_MTCMOS_ON_OFF);
	err_sta = readl(dpc_base + DISP_DPC_INTSTA_DISP_PM_CFG_ERROR);
	dt_sta = readl(dpc_base + DISP_DPC_INTSTA_DT_TE_THREAD);
	pwr_rdy =  readl(dpc_base + DISP_DPC_INTSTA_INTF_PWR_RDY_STATE);
	if (!(mtcmos_sta || err_sta || dt_sta || mtcmos_busy || pwr_rdy || hwvote_sta)) {
		if (__ratelimit(&err_rate)) {
			DPCERR("irq err clksq");
			debug_irq = 0;
		}
		goto out;
	}

	if (mtcmos_busy)
		writel(0, dpc_base + DISP_DPC_INTSTA_MTCMOS_BUSY);
	if (mtcmos_sta)
		writel(0, dpc_base + DISP_DPC_INTSTA_MTCMOS_ON_OFF);
	if (err_sta)
		writel(0, dpc_base + DISP_DPC_INTSTA_DISP_PM_CFG_ERROR);
	if (dt_sta)
		writel(0, dpc_base + DISP_DPC_INTSTA_DT_TE_THREAD);
	if (pwr_rdy)
		writel(0, dpc_base + DISP_DPC_INTSTA_INTF_PWR_RDY_STATE);
	if (hwvote_sta)
		writel(0, dpc_base + DISP_DPC_INTSTA_HWVOTE_STATE);

	if (err_sta) {
		dpc_mmp(folder, MMPROFILE_FLAG_PULSE, err_sta, readl(hwccf_global_en));
		if (__ratelimit(&err_rate)) {
			if (irq_aee == 1) {
				DPCAEE("irq err(%#x) psel(%#x=%#x) V(%#x) G(%#x)",
					err_sta,
					readl(dpc_base + DISP_SW_OFF_CONFIG_PADDR_W_PWRITE),
					readl(dpc_base + DISP_SW_OFF_CONFIG_PWRITE),
					readl(mmpc_dummy_voter),
					readl(hwccf_global_en));
			} else if (irq_aee == 2) {
				DPCERR("irq err(%#x) psel(%#x=%#x) V(%#x) G(%#x)",
					err_sta,
					readl(dpc_base + DISP_SW_OFF_CONFIG_PADDR_W_PWRITE),
					readl(dpc_base + DISP_SW_OFF_CONFIG_PWRITE),
					readl(mmpc_dummy_voter),
					readl(hwccf_global_en));
				#if IS_ENABLED(CONFIG_MTK_HWCCF)
				clkchk_external_dump();
				#endif
				BUG_ON(1);
			} else if (irq_aee == 3) {
				if (rgu_reg)
					writel(0x1209, rgu_reg);
			} else
				DPCERR("irq err(%#x) psel(%#x=%#x) V(%#x) G(%#x)",
					err_sta,
					readl(dpc_base + DISP_SW_OFF_CONFIG_PADDR_W_PWRITE),
					readl(dpc_base + DISP_SW_OFF_CONFIG_PWRITE),
					readl(mmpc_dummy_voter),
					readl(hwccf_global_en));
		}
	}

	/* Panel TE */
	if (debug_irq & (BIT(1) | BIT(2) | BIT(7)))
		if (dt_sta & INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DT_TE_SEL_0)
			dpc_mmp(prete, MMPROFILE_FLAG_PULSE, 0, 0);

	if (debug_irq & BIT(2))
		if (dt_sta & INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DT_TE_SEL_6)
			dpc_mmp(mtcmos_mml1, MMPROFILE_FLAG_PULSE, 0, 0);

	if (debug_irq & BIT(3)) {
		if ((mtcmos_sta & INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP1A_MTCMOS_OFF) && disp1_has_begin) {
			dpc_mmp(mtcmos_disp1, MMPROFILE_FLAG_END, 0, 0);
			tracing_mark_write(trace_buf_disp_off);
			disp1_has_begin = false;
		}
		if ((mtcmos_sta & INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP1A_MTCMOS_ON) && !disp1_has_begin) {
			dpc_mmp(mtcmos_disp1, MMPROFILE_FLAG_START, 0, 1);
			tracing_mark_write(trace_buf_disp_on);
			disp1_has_begin = true;
		}
	}

	if (debug_irq & BIT(8)) {
		if ((hwvote_sta & INTSTA_MML2_HWVOTE_SIGN_LO) && mml2_has_begin) {
			dpc_mmp(mtcmos_mml1, MMPROFILE_FLAG_END, 0, 0);
			tracing_mark_write(trace_buf_mml_off);
			mml2_has_begin = false;
		}
		if ((hwvote_sta & INTSTA_MML2_HWVOTE_SIGN_HI) && !mml2_has_begin) {
			dpc_mmp(mtcmos_mml1, MMPROFILE_FLAG_START, 0, 1);
			tracing_mark_write(trace_buf_mml_on);
			mml2_has_begin = true;
		}
	}

	if (debug_irq & BIT(4)) {
		if (mtcmos_busy & BIT(24))	// DT17 mminfra off
			dpc_mmp(debug1, MMPROFILE_FLAG_PULSE, 0x24, 0);
		if (mtcmos_busy & BIT(26))	// DT19 mminfra off
			dpc_mmp(debug1, MMPROFILE_FLAG_PULSE, 0x26, 0);
		if (mtcmos_busy & BIT(25))	// DT18 mminfra on
			dpc_mmp(debug1, MMPROFILE_FLAG_PULSE, 0x25, 1);
	}

	if (debug_irq & BIT(5)) {
		if (pwr_rdy & BIT(20)) {	// mminfra off rdy rising
			dpc_mmp(debug2, MMPROFILE_FLAG_END, 0x20, 0);
			tracing_mark_write(trace_buf_mminfra_off);
		}
		if (pwr_rdy & BIT(21)) {	// mminfra on rdy rising
			dpc_mmp(debug2, MMPROFILE_FLAG_START, 0x21, 1);
			tracing_mark_write(trace_buf_mminfra_on);
		}
	}

	/* Reserved DT for gce exception voter debug */
	if (debug_irq & BIT(7)) {
		if (dt_sta & INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DT_TE_SEL_5)
			dpc_mmp(hwccf_gce_vote, MMPROFILE_FLAG_END,
				readl(hwccf_xpu6_local_en), readl(hwccf_global_en));
		if (dt_sta & INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DT_TE_SEL_4)
			dpc_mmp(hwccf_gce_vote, MMPROFILE_FLAG_START,
				readl(hwccf_xpu6_local_en), readl(hwccf_global_en));
		if (dt_sta & INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DT_TE_SEL_7) {
			dpc_mmp(hwccf_gce_vote, MMPROFILE_FLAG_PULSE,
				readl(mmpc_dummy_voter), readl(hwccf_global_en));
			if (irq_aee == 1)
				DPCAEE("gce exception voter unbalanced");
			else
				DPCERR("gce exception voter unbalanced");
		}
	}

	ret = IRQ_HANDLED;
out:
	dpc_mminfra_on_off(false, DISP_VIDLE_USER_DISP_DPC_CFG);
	return ret;
}

static void get_addr_byname(const char *name, void __iomem **va, resource_size_t *pa)
{
	void __iomem *_va;
	struct resource *res;

	res = platform_get_resource_byname(g_priv->pdev, IORESOURCE_MEM, name);
	if (res) {
		_va = ioremap(res->start, resource_size(res));
		if (!IS_ERR_OR_NULL(_va)) {
			*va = _va;
			if (pa)
				*pa = res->start;

			DPCDUMP("mapping %s %pa done", name, &res->start);
		} else
			DPCERR("failed to map %s %pR", name, res);
	} else
		DPCERR("failed to map %s", name);
}

static int dpc_res_init_v2(struct mtk_dpc *priv)
{
	int ret = 0;
	int num_irqs;

	get_addr_byname("DPC_BASE", &dpc_base, &priv->dpc_pa);
	ret = IS_ERR_OR_NULL(dpc_base);
	if (ret)
		return ret;

	get_addr_byname("rtff_pwr_con", &priv->rtff_pwr_con, NULL);
	get_addr_byname("disp_sw_vote_set", &priv->voter_set_va, &priv->voter_set_pa);
	get_addr_byname("disp_sw_vote_clr", &priv->voter_clr_va, &priv->voter_clr_pa);
	get_addr_byname("vcore_mode_set", &priv->vcore_mode_set_va, NULL);
	get_addr_byname("vcore_mode_clr", &priv->vcore_mode_clr_va, NULL);
	get_addr_byname("vdisp_dvfsrc", &priv->vdisp_dvfsrc, NULL);
	get_addr_byname("disp_vcore_pwr_chk", &priv->dispvcore_chk, NULL);
	get_addr_byname("mminfra_pwr_chk", &priv->mminfra_chk, NULL);
	get_addr_byname("mminfra_voter", &priv->mminfra_voter, NULL);
	get_addr_byname("mminfra_dummy", &priv->mminfra_dummy, NULL);
	get_addr_byname("mminfra_hangfree", &priv->mminfra_hangfree, NULL);
	get_addr_byname("vdisp_ao_cg_con", &priv->vdisp_ao_cg_con, NULL);

	if (priv->mmsys_id == MMSYS_MT6991) {
		enum mtk_dpc_subsys subsys = 0;

		/* use for gced, modify for access mmup inside mminfra */
		priv->voter_set_pa -= 0x800000;
		priv->voter_clr_pa -= 0x800000;

		/* power check by dpc, instead of subsys_pm */
		for (subsys = 0; subsys < g_priv->subsys_cnt; subsys++) {
			priv->mtcmos_cfg[subsys].chk_pa = priv->dpc_pa + priv->mtcmos_cfg[subsys].cfg + 0x8;
			priv->mtcmos_cfg[subsys].chk_va = dpc_base + priv->mtcmos_cfg[subsys].cfg + 0x8;
		}

		mmpc_emi_req = ioremap(0x31b5103c, 0x4);
		mmpc_ddrsrc_req = ioremap(0x31b5101c, 0x4);
		spm_ddr_emi_req = ioremap(0x1c00488c, 0x4);
		vdisp_dvfsrc_sw_req4 = ioremap(0x31a9101c, 0x4);
		clk_disp_sel = ioremap(0x1000c050, 0x4);
	}

	num_irqs = platform_irq_count(priv->pdev);
	if (num_irqs <= 0) {
		DPCERR("unable to count IRQs");
		ret = -EPROBE_DEFER;
		goto skip_irq_request;
	} else if (num_irqs == 1) {
		priv->disp_irq = platform_get_irq(priv->pdev, 0);
	} else if (num_irqs == 2) {
		priv->disp_irq = platform_get_irq(priv->pdev, 0);
		priv->mml_irq = platform_get_irq(priv->pdev, 1);
	}

	if (priv->disp_irq > 0) {
		ret = devm_request_irq(priv->dev, priv->disp_irq, priv->disp_irq_handler,
				       IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(priv->dev), priv);
		if (ret)
			DPCERR("devm_request_irq %d fail: %d", priv->disp_irq, ret);
	}
	if (priv->mml_irq > 0) {
		ret = devm_request_irq(priv->dev, priv->mml_irq, priv->mml_irq_handler,
				       IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(priv->dev), priv);
		if (ret)
			DPCERR("devm_request_irq %d fail: %d", priv->mml_irq, ret);
	}
	DPCFUNC("disp irq %d, mml irq %d, ret %d", priv->disp_irq, priv->mml_irq, ret);

skip_irq_request:

	/* disable merge irq */
	writel(0, dpc_base + DISP_REG_DPC_MERGE_DISP_INT_CFG);
	writel(0, dpc_base + DISP_REG_DPC_MERGE_DISP_INTSTA);
	writel(0, dpc_base + DISP_REG_DPC_MERGE_MML_INT_CFG);
	writel(0, dpc_base + DISP_REG_DPC_MERGE_MML_INTSTA);

	/* enable external signal from DSI and TE */
	writel(0x1F, dpc_base + DISP_REG_DPC_DISP_EXT_INPUT_EN);
	writel(0x3, dpc_base + DISP_REG_DPC_MML_EXT_INPUT_EN);

	/* request emireq, ddrsrc, mainpll, infra, mminfra */
	writel(0x000D000D, dpc_base + DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG);
	writel(0x000D000D, dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG);
	writel(0x181818, dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG);
	writel(0x181818, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);

	/* keep vdisp opp */
	writel(0x1, dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_CFG);
	writel(0x1, dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_CFG);

	/* keep HRT and SRT BW */
	writel(0x00010001, dpc_base + DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG);
	writel(0x00010001, dpc_base + DISP_REG_DPC_MML_HRTBW_SRTBW_CFG);

	writel(priv->dt_follow_cfg, dpc_base + DISP_REG_DPC_DISP_DT_FOLLOW_CFG);
	writel(priv->dt_follow_cfg, dpc_base + DISP_REG_DPC_MML_DT_FOLLOW_CFG);
	writel(0x40, dpc_base + DISP_DPC_ON2SOF_DT_EN);
	writel(0xf, dpc_base + DISP_DPC_ON2SOF_DSI0_SOF_COUNTER); /* should > 2T, cannot be zero */
	writel(0x2, dpc_base + DISP_REG_DPC_DEBUG_SEL);	/* mtcmos_debug */

	if (priv->mmsys_id == MMSYS_MT6991) {
		/* set vdisp level */
		// dpc_dvfs_set(DPC_SUBSYS_DISP, 0x4, true);

		/* set channel bw for the first HRT_READ layer, which is exdma3 currently */
		dpc_ch_bw_set_v2(DPC_SUBSYS_DISP, 6, 363 * 16);

		/* set total HRT bw */
		priv->hrt_bw_set(DPC_SUBSYS_DISP, 363 * priv->total_hrt_unit, true);
	}

	return ret;
}

static int dpc_res_init_v3(struct mtk_dpc *priv)
{
	int ret = 0;
	enum mtk_dpc_subsys subsys = 0;

	DPCFUNC();

	get_addr_byname("DPC_BASE", &dpc_base, &priv->dpc_pa);
	ret = IS_ERR_OR_NULL(dpc_base);
	if (ret)
		return ret;

	get_addr_byname("disp_sw_vote_set", &priv->voter_set_va, &priv->voter_set_pa);
	get_addr_byname("disp_sw_vote_clr", &priv->voter_clr_va, &priv->voter_clr_pa);
	get_addr_byname("vcore_mode_set", &priv->vcore_mode_set_va, NULL);
	get_addr_byname("vcore_mode_clr", &priv->vcore_mode_clr_va, NULL);
	// get_addr_byname("vdisp_dvfsrc", &priv->vdisp_dvfsrc, NULL);	/* TODO: need porting devapc */
	get_addr_byname("disp_vcore_pwr_chk", &priv->dispvcore_chk, NULL);
	get_addr_byname("vdisp_ao_cg_con", &priv->vdisp_ao_cg_con, NULL);

	/* use for gced, modify for access mmup inside mminfra */
	priv->voter_set_pa -= 0x800000;
	priv->voter_clr_pa -= 0x800000;

	/* power check by dpc, instead of subsys_pm */
	for (subsys = 0; subsys < g_priv->subsys_cnt; subsys++) {
		priv->mtcmos_cfg[subsys].chk_pa = priv->dpc_pa + priv->mtcmos_cfg[subsys].cfg + 0xc;
		priv->mtcmos_cfg[subsys].chk_va = dpc_base + priv->mtcmos_cfg[subsys].cfg + 0xc;
	}

	priv->disp_irq = platform_get_irq(priv->pdev, 0);
	if (priv->disp_irq > 0) {
		ret = devm_request_irq(priv->dev, priv->disp_irq, priv->disp_irq_handler,
				       IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(priv->dev), priv);
		if (ret)
			DPCERR("devm_request_irq %d fail: %d", priv->disp_irq, ret);
	} else
		DPCERR("unable to get IRQ");

	/* enable external signal from DSI and TE */
	writel(0x3F, dpc_base + DISP_REG_DPC_DISP_EXT_INPUT_EN);
	writel(0x11, dpc_base + DISP_REG_DPC_MML_EXT_INPUT_EN);

	/* force request emireq and ddrsrc */
	writel(0x0D0D0D0D, dpc_base + DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG);	/* TCUCOH, DATACOH */
	writel(0x0D0D0D0D, dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG);

	/* DISP_DPC_MMINFRA_HWVOTE_CFG */
	writel(0x00080008, dpc_base + DISP_DPC_MMINFRA_HWVOTE_CFG);
	writel(0x1a1a1a, dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG);
	writel(0x1a1a1a, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);

	/* keep vdisp opp */
	writel(0x1, dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_CFG);			/* TODO: CHECK VALUE */
	writel(0x1, dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_CFG);

	/* keep HRT and SRT BW */
	writel(0x00010001, dpc_base + DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG);		/* TODO: CHECK VALUE */
	writel(0x00010001, dpc_base + DISP_REG_DPC_MML_HRTBW_SRTBW_CFG);

	writel(priv->dt_follow_cfg, dpc_base + DISP_REG_DPC_DISP_DT_FOLLOW_CFG);	/* TODO: CHECK VALUE */
	writel(priv->dt_follow_cfg, dpc_base + DISP_REG_DPC_MML_DT_FOLLOW_CFG);
	writel(0x40, dpc_base + DISP_DPC_ON2SOF_DT_EN);
	writel(0xf, dpc_base + DISP_DPC_ON2SOF_DSI0_SOF_COUNTER); /* should > 2T, cannot be zero */

	/* set vdisp level */
	// dpc_dvfs_set(DPC_SUBSYS_DISP, 0x4, true);

	/* set channel bw for the first HRT_READ layer, which is exdma3 currently */
	// dpc_ch_bw_set_v2(DPC_SUBSYS_DISP, 6, 363 * 16);

	/* set total HRT bw */
	// dpc_hrt_bw_set_v3(DPC_SUBSYS_DISP, 363 * priv->total_hrt_unit, true);

	hwccf_mtcmos_pm_ack = ioremap(0x31c12900, 0x4);
	hwccf_dummy_set = ioremap(0x31c03fb0, 0x4);
	hwccf_dummy_clr = ioremap(0x31c03fb4, 0x4);
	hwccf_dummy_en = ioremap(0x31c03fb8, 0x4);
	hwccf_xpu0_mtcmos_set = ioremap(0x31c20700, 0x4);
	hwccf_xpu0_mtcmos_clr = ioremap(0x31c20704, 0x4);
	hwccf_xpu0_local_en = ioremap(0x31c20708, 0x4);
	hwccf_xpu6_local_en = ioremap(0x31c71708, 0x4);
	hwccf_global_en = ioremap(0x31c13700, 0x4);
	hwccf_global_sta = ioremap(0x31c13704, 0x4);
	hwccf_total_sta = ioremap(0x31c10000, 0x4);
	hwccf_mtcmos_en = ioremap(0x31c11318, 0x4);
	hwccf_mtcmos_sta = ioremap(0x31c1131c, 0x4);
	hwccf_hw_mtcmos_req = ioremap(0x31c14300, 0x4);
	hwccf_hw_irq_req = ioremap(0x31c14400, 0x4);
	hwccf_cg47_set = ioremap(0x31c20234, 0x4);
	hwccf_cg47_clr = ioremap(0x31c20238, 0x4);
	hwccf_cg47_en = ioremap(0x31c122bc, 0x4);
	hwccf_cg47_sta = ioremap(0x31c118bc, 0x4);

	mmpc_dummy_voter = ioremap(0x31b50160, 0x4);


	// Mask unprocessed mutex
	busy_mask[0] = ioremap(0x3E320328, 0x4); //(0x1000100)
	busy_mask[1] = ioremap(0x3E32032C, 0x4); //(0x182300)
	//             ioremap(0x3E520328, 0x4); //(0x0)
	//             ioremap(0x3E52032C, 0x4); //(0x0)
	busy_mask[2] = ioremap(0x3E720328, 0x4); //(0x601)
	busy_mask[3] = ioremap(0x3E72032C, 0x4); //(0x300)
	//             ioremap(0x3E920328, 0x4); //(0x0)
	//             ioremap(0x3E92032C, 0x4); //(0x0)
	//             ioremap(0x32920328, 0x4); //(0x0)
	busy_mask[4] = ioremap(0x3292032C, 0x4); //(0x10000)

	// Mask unprocessed irq
	//             ioremap(0x3EFF1034, 0x4);
	busy_mask[5] = ioremap(0x3EFF1134, 0x4);
	//             ioremap(0x3EFF1234, 0x4);
	busy_mask[6] = ioremap(0x3EFF1334, 0x4);
	busy_mask[7] = ioremap(0x3EFF1434, 0x4);
	busy_mask[8] = ioremap(0x3EFF1534, 0x4);
	busy_mask[9] = ioremap(0x3EFF1634, 0x4);
	//             ioremap(0x3EFF1734, 0x4);
	//             ioremap(0x3EFF1834, 0x4);
	//             ioremap(0x3EFF1934, 0x4);

	return 0;
}

static void mtk_disp_vlp_vote_v2(unsigned int vote_set, unsigned int thread)
{
	void __iomem *voter_va = vote_set ? g_priv->voter_set_va : g_priv->voter_clr_va;
	u32 ack = vote_set ? BIT(thread) : 0;
	u32 val = 0;
	u16 i = 0;
	static atomic_t has_begin = ATOMIC_INIT(0);

	if (!voter_va)
		return;

	writel_relaxed(BIT(thread), voter_va);
	do {
		writel_relaxed(BIT(thread), voter_va);
		val = readl(voter_va);
		if ((val & BIT(thread)) == ack)
			break;

		if (i > 2500) {
			DPCERR("%s by thread(%u) timeout, vcp(%u) mminfra(%u)",
				vote_set ? "set" : "clr", thread,
				g_priv->vcp_is_alive, mminfra_is_power_on_v2());
			return;
		}

		udelay(2);
		i++;
	} while (1);

	/* check voter only, later will use another API to power on mminfra */

	dpc_mmp(vlp_vote, MMPROFILE_FLAG_PULSE, BIT(thread) | vote_set, val);

	if (val != 0 && atomic_read(&has_begin) == 0) {
		atomic_set(&has_begin, 1);
		dpc_mmp(vlp_vote, MMPROFILE_FLAG_START, 0, val);
	} else if (val == 0 && atomic_read(&has_begin) == 1) {
		atomic_set(&has_begin, 0);
		dpc_mmp(vlp_vote, MMPROFILE_FLAG_END, 0, 0);
	}
}

static int dpc_vidle_power_keep_v2(const enum mtk_vidle_voter_user _user)
{
	int ret = VOTER_PM_DONE;
	enum mtk_vidle_voter_user user = _user & DISP_VIDLE_USER_MASK;

	if (!g_priv->vcp_is_alive) {
		DPCFUNC("by user(%#x) skipped", _user);
		return VOTER_PM_FAILED;
	}

	if (_user & VOTER_ONLY) {
		mtk_disp_vlp_vote_v2(VOTE_SET, user);
		return VOTER_ONLY;
	} else if (user == DISP_VIDLE_USER_TOP_CLK_ISR) {
		/* skip pm_get to fix unstable DSI TE, mminfra power is held by DPC usually */
		/* but if no power at this time, the user should call pm_get to ensure power */
		mtk_disp_vlp_vote_v2(VOTE_SET, user);
		return mminfra_is_power_on_v2() ? VOTER_ONLY : VOTER_PM_LATER;
	}

	if (dpc_pm_ctrl(true))
		return VOTER_PM_FAILED;

	mtk_disp_vlp_vote_v2(VOTE_SET, user);

	if (!g_priv->enabled || user < DISP_VIDLE_USER_CRTC)
		return ret;

	switch (user) {
	case DISP_VIDLE_USER_MML1:
		ret = dpc_wait_pwr_ack_v2(DPC_SUBSYS_MML1);
		break;
	case DISP_VIDLE_USER_MML0:
		ret = dpc_wait_pwr_ack_v2(DPC_SUBSYS_MML0);
		break;
	case DISP_VIDLE_USER_PQ:
		if (g_priv->root_dev)
			pm_runtime_get_sync(g_priv->root_dev); /* make sure power is on, must not in ISR */
		dpc_wait_pwr_ack_v2(DPC_SUBSYS_DIS1);
		ret = dpc_wait_pwr_ack_v2(DPC_SUBSYS_DIS0);
		if (g_priv->root_dev)
			pm_runtime_put_sync(g_priv->root_dev);
		break;
	case DISP_VIDLE_USER_CRTC:
		if (g_priv->root_dev)
			pm_runtime_get_sync(g_priv->root_dev);
		dpc_wait_pwr_ack_v2(DPC_SUBSYS_DIS1);
		dpc_wait_pwr_ack_v2(DPC_SUBSYS_DIS0);
		dpc_wait_pwr_ack_v2(DPC_SUBSYS_OVL0);
		ret = dpc_wait_pwr_ack_v2(DPC_SUBSYS_OVL1);
		if (g_priv->root_dev)
			pm_runtime_put_sync(g_priv->root_dev);
		break;
	case DISP_VIDLE_USER_DISP_DPC_CFG:
	case DISP_VIDLE_USER_DPC_DUMP:
	case DISP_VIDLE_USER_SMI_DUMP:
	case DISP_VIDLE_FORCE_KEEP:
		dpc_wait_pwr_ack_v2(DPC_SUBSYS_DIS1);
		dpc_wait_pwr_ack_v2(DPC_SUBSYS_DIS0);
		dpc_wait_pwr_ack_v2(DPC_SUBSYS_OVL0);
		ret = dpc_wait_pwr_ack_v2(DPC_SUBSYS_OVL1);
		break;
	default:
		udelay(post_vlp_delay);
	}

	return ret;
}

static void dpc_vidle_power_release_v2(const enum mtk_vidle_voter_user user)
{
	if (!g_priv->vcp_is_alive) {
		DPCFUNC("by user(%u) skipped", user & DISP_VIDLE_USER_MASK);
		return;
	}

	mtk_disp_vlp_vote_v2(VOTE_CLR, user & DISP_VIDLE_USER_MASK);

	if ((user & VOTER_ONLY) || ((user & DISP_VIDLE_USER_MASK) == DISP_VIDLE_USER_TOP_CLK_ISR))
		return;

	dpc_pm_ctrl(false);
}

static void dpc_clear_wfe_event_v2(struct cmdq_pkt *pkt, enum mtk_vidle_voter_user user, int event)
{
	switch (wfe_prete) {
	case 1:
		break;
	case 2:
		return;
	default:
		if (!has_cap(DPC_CAP_MTCMOS))
			return;
	}

	cmdq_pkt_clear_event(pkt, event);
	cmdq_pkt_wfe(pkt, event);
}

static void dpc_vidle_power_keep_by_gce_v2(struct cmdq_pkt *pkt, const enum mtk_vidle_voter_user user,
					const u16 gpr, void *reuse)
{
	cmdq_pkt_write(pkt, NULL, g_priv->voter_set_pa, BIT(user), U32_MAX);

	switch (user) {
	case DISP_VIDLE_USER_DISP_CMDQ:
		cmdq_pkt_poll_sleep(pkt, BIT(20), g_priv->mtcmos_cfg[DPC_SUBSYS_DIS1].chk_pa, BIT(20));
		cmdq_pkt_poll_sleep(pkt, BIT(20), g_priv->mtcmos_cfg[DPC_SUBSYS_DIS0].chk_pa, BIT(20));
		cmdq_pkt_poll_sleep(pkt, BIT(20), g_priv->mtcmos_cfg[DPC_SUBSYS_OVL0].chk_pa, BIT(20));
		cmdq_pkt_poll_sleep(pkt, BIT(20), g_priv->mtcmos_cfg[DPC_SUBSYS_OVL1].chk_pa, BIT(20));
		break;
	case DISP_VIDLE_USER_DDIC_CMDQ:
		cmdq_pkt_poll_sleep(pkt, BIT(20), g_priv->mtcmos_cfg[DPC_SUBSYS_DIS1].chk_pa, BIT(20));
		break;
	case DISP_VIDLE_USER_MML1_CMDQ:
		cmdq_pkt_poll_sleep(pkt, BIT(20), g_priv->mtcmos_cfg[DPC_SUBSYS_MML1].chk_pa, BIT(20));
		break;
	case DISP_VIDLE_USER_MML0_CMDQ:
		cmdq_pkt_poll_sleep(pkt, BIT(20), g_priv->mtcmos_cfg[DPC_SUBSYS_MML0].chk_pa, BIT(20));
		break;
	default:
		DPCERR("not support user %u", user);
		return;
	}
}

static void dpc_vidle_power_release_by_gce_v2(struct cmdq_pkt *pkt, const enum mtk_vidle_voter_user user, void *reuse)
{
	cmdq_pkt_write(pkt, NULL, g_priv->voter_clr_pa, BIT(user), U32_MAX);
}

static void dpc_ap_vote_mmpc(bool add, const enum mtk_vidle_voter_user user)
{
	int ret = 0;
	u32 value = 0;
	// u32 mask = 0x1ff80000;

	if (add) {
		if (g_priv->mtcmos_cfg[DPC3_SUBSYS_DIS1A].mode == DPC_MTCMOS_AUTO) {
			dpc2_dt_en(3, true, true);
			dpc2_dt_en(7, true, true);
			dpc2_dt_en(35, true, true);
		}

		/* polling dpc req idle */
		ret = readl_poll_timeout_atomic(hwccf_hw_mtcmos_req, value, !(value & 0xffc0), 1, 2000);
		if (ret < 0)
			DPCERR("polling dpc req idle timeout %d", __LINE__);

		/* polling mminfra req idle */
		ret = readl_poll_timeout_atomic(hwccf_hw_irq_req, value, !(value & 0xc), 1, 2000);
		if (ret < 0)
			DPCERR("polling mminfra req idle timeout %d", __LINE__);

		mtk_disp_vlp_vote_v2(add, user);

		/* polling dpc req idle */
		ret = readl_poll_timeout_atomic(hwccf_hw_mtcmos_req, value, !(value & 0xffc0), 1, 2000);
		if (ret < 0)
			DPCERR("polling dpc req idle timeout %d", __LINE__);

		/* polling mminfra req idle */
		ret = readl_poll_timeout_atomic(hwccf_hw_irq_req, value, !(value & 0xc), 1, 2000);
		if (ret < 0)
			DPCERR("polling mminfra req idle timeout %d", __LINE__);

		/* polling mtcmos ack */
		ret = readl_poll_timeout_atomic(hwccf_mtcmos_pm_ack, value, value & 0x1ff80000, 1, 2000);
		if (ret < 0) {
			DPCERR("polling mtcmos ack timeout %d value(%#x)", __LINE__, value);

			/* polling all fsm idle */
			ret = readl_poll_timeout_atomic(hwccf_total_sta, value, !(value & 0xfffe), 1, 10000);
			if (ret < 0)
				DPCERR("polling all fsm idle timeout %d", __LINE__);

			writel(BIT(16), hwccf_cg47_set);
			ret = readl_poll_timeout_atomic(hwccf_cg47_en, value, value & BIT(16), 1, 200);
			ret = readl_poll_timeout_atomic(hwccf_cg47_sta, value, !(value & BIT(16)), 1, 10000);

			writel(BIT(16), hwccf_cg47_clr);
			ret = readl_poll_timeout_atomic(hwccf_cg47_en, value, !(value & BIT(16)), 1, 200);
			ret = readl_poll_timeout_atomic(hwccf_cg47_sta, value, !(value & BIT(16)), 1, 10000);
		}
	} else {
		mtk_disp_vlp_vote_v2(add, user);
#ifdef IF_ZERO
		/* polling dpc req idle */
		ret = readl_poll_timeout_atomic(hwccf_hw_mtcmos_req, value, !(value & 0xffc0), 1, 2000);
		if (ret < 0)
			DPCERR("polling dpc req idle timeout %d", __LINE__);

		/* polling mminfra req idle */
		ret = readl_poll_timeout_atomic(hwccf_hw_irq_req, value, !(value & 0xc), 1, 2000);
		if (ret < 0)
			DPCERR("polling mminfra req idle timeout %d", __LINE__);
#endif
		if (g_priv->mtcmos_cfg[DPC3_SUBSYS_DIS1A].mode == DPC_MTCMOS_AUTO) {
			dpc2_dt_en(3, true, false);
			dpc2_dt_en(7, true, false);
			dpc2_dt_en(35, true, false);
		}
	}

	return;
}

static void dpc_ap_ref_cnt(bool add, const enum mtk_vidle_voter_user user)
{
	unsigned long flags;
	s32 cnt;

	spin_lock_irqsave(&g_priv->hwccf_ref_lock, flags);
	cnt = add ? atomic_inc_return(&hwccf_ref) : atomic_dec_return(&hwccf_ref);

	if (add && cnt == 1) {
		dpc_mmp(hwccf_vote, MMPROFILE_FLAG_START, BIT(28) | user, cnt);
		if (excep_by_xpu & BIT(0))
			dpc_hwccf_vote(VOTE_SET, NULL, user, false, 0);
		else
			dpc_ap_vote_mmpc(VOTE_SET, user);
	} else if (!add && cnt == 0) {
		if (excep_by_xpu & BIT(0))
			dpc_hwccf_vote(VOTE_CLR, NULL, user, false, 0);
		else
			dpc_ap_vote_mmpc(VOTE_CLR, user);
		dpc_mmp(hwccf_vote, MMPROFILE_FLAG_END, BIT(29) | user, cnt);
	} else if (cnt < 0) {
		atomic_set_release(&hwccf_ref, 0);
		dpc_mmp(folder, MMPROFILE_FLAG_PULSE, (add ? BIT(28) : BIT(29)) | user, 0xdead0022);
	} else {
		dpc_mmp(hwccf_vote, MMPROFILE_FLAG_PULSE, (add ? BIT(28) : BIT(29)) | user, cnt);
	}

	spin_unlock_irqrestore(&g_priv->hwccf_ref_lock, flags);

	if (cnt < 0) {
		if (unlikely(irq_aee))
			DPCAEE("underflow user(%u) hwccf_ref(%d)", user, cnt);
		else
			DPCERR("underflow user(%u) hwccf_ref(%d)", user, cnt);
	}
}

static int dpc_vidle_power_keep_v3(const enum mtk_vidle_voter_user _user)
{
	s32 user_cnt = 0;
	int ret = VOTER_PM_DONE;
	enum mtk_vidle_voter_user user = _user & DISP_VIDLE_USER_MASK;
	unsigned long flags;

	if (!dpc_buck_status(-1)) { /* buck off */
		dpc_mmp(folder, MMPROFILE_FLAG_PULSE, BIT(28) | user, 0xdead0011);
		return VOTER_PM_SKIP_PWR_OFF;
	}

	if (user == DISP_VIDLE_USER_TOP_CLK_ISR || user == DISP_VIDLE_USER_MML_CLK_ISR
			|| user == DISP_VIDLE_USER_DISP_DPC_CFG) {
		spin_lock_irqsave(&g_priv->excp_spin_lock, flags);

		user_cnt = atomic_inc_return(&g_user_15);
		dpc_mmp(user_15, user_cnt == 1 ? MMPROFILE_FLAG_START : MMPROFILE_FLAG_PULSE, 1, user_cnt);
		if (user_cnt == 1) {
			dpc_mminfra_on_off(true, user);
			dpc_ap_ref_cnt(VOTE_SET, user);
		}

		atomic_inc(&excep_ret[user]);
		spin_unlock_irqrestore(&g_priv->excp_spin_lock, flags);
		return ret;
	}

	/* No need this user for xpu exception */
	if ((excep_by_xpu & BIT(0)) && (user == DISP_VIDLE_USER_FOR_FRAME))
		return ret;

	tracing_mark_write(trace_buf_keep[0][0]);
	mutex_lock(&g_priv->excp_lock);

	switch (user) {
	case DISP_VIDLE_USER_NST_LOCK:
		user_cnt = atomic_inc_return(&g_user_9);
		dpc_mmp(user_9, user_cnt == 1 ? MMPROFILE_FLAG_START : MMPROFILE_FLAG_PULSE, 1, user_cnt);
		break;
	case DISP_VIDLE_USER_FOR_FRAME:
		user_cnt = atomic_inc_return(&g_user_14);
		dpc_mmp(user_14, user_cnt == 1 ? MMPROFILE_FLAG_START : MMPROFILE_FLAG_PULSE, 1, user_cnt);
		break;
	case DISP_VIDLE_USER_CRTC:
		user_cnt = atomic_inc_return(&g_user_16);
		dpc_mmp(user_16, user_cnt == 1 ? MMPROFILE_FLAG_START : MMPROFILE_FLAG_PULSE, 1, user_cnt);
		break;
	case DISP_VIDLE_USER_PQ:
		user_cnt = atomic_inc_return(&g_user_17);
		dpc_mmp(user_17, user_cnt == 1 ? MMPROFILE_FLAG_START : MMPROFILE_FLAG_PULSE, 1, user_cnt);
		break;
	default:
		break;
	}

	if (excep_by_xpu & BIT(0)) {
		tracing_mark_write(trace_buf_keep[1][0]);
		dpc_mminfra_on_off(VOTE_SET, user);

		tracing_mark_write(trace_buf_keep[2][0]);
		dpc_pre_cg_ctrl(VOTE_SET, false);

		tracing_mark_write(trace_buf_keep[3][0]);
		dpc_ap_ref_cnt(VOTE_SET, user);

		writel(0x1, dpc_base + DISP_REG_DPC_DUMMY1);
	} else {
		if (user == DISP_VIDLE_USER_FOR_FRAME)
			dpc_ap_vote_mmpc(VOTE_SET, user);
		else {
			dpc_mminfra_on_off(VOTE_SET, user);
			dpc_pre_cg_ctrl(VOTE_SET, false);
			dpc_ap_ref_cnt(VOTE_SET, user);
		}
	}

	atomic_inc(&excep_ret[user]);
	mutex_unlock(&g_priv->excp_lock);

	if (excep_by_xpu & BIT(0)) {
		tracing_mark_write(trace_buf_keep[3][1]);
		tracing_mark_write(trace_buf_keep[2][1]);
		tracing_mark_write(trace_buf_keep[1][1]);
	}
	tracing_mark_write(trace_buf_keep[0][1]);

	return ret;
}

static void dpc_vidle_power_release_v3(const enum mtk_vidle_voter_user _user)
{
	s32 user_cnt = 0;
	enum mtk_vidle_voter_user user = _user & DISP_VIDLE_USER_MASK;
	unsigned long flags;

	if (atomic_read(&excep_ret[user]) <= 0) {
		dpc_mmp(folder, MMPROFILE_FLAG_PULSE, BIT(29) | user, 0xdead0011);
		return;
	}

	if (user == DISP_VIDLE_USER_TOP_CLK_ISR || user == DISP_VIDLE_USER_MML_CLK_ISR
			|| user == DISP_VIDLE_USER_DISP_DPC_CFG) {
		spin_lock_irqsave(&g_priv->excp_spin_lock, flags);

		user_cnt = atomic_dec_return(&g_user_15);
		if (user_cnt == 0) {
			dpc_ap_ref_cnt(VOTE_CLR, user);
			dpc_mminfra_on_off(false, user);
		} else if (user_cnt < 0) {
			atomic_set_release(&g_user_15, 0);
			spin_unlock_irqrestore(&g_priv->excp_spin_lock, flags);
			if (unlikely(irq_aee))
				DPCAEE("user(%u) underflow", user);
			else
				DPCERR("user(%u) underflow", user);
			return;
		}

		atomic_dec(&excep_ret[user]);
		spin_unlock_irqrestore(&g_priv->excp_spin_lock, flags);
		dpc_mmp(user_15, user_cnt == 0 ? MMPROFILE_FLAG_END : MMPROFILE_FLAG_PULSE, 1, user_cnt);
		return;
	}

	/* No need this user for xpu exception */
	if ((excep_by_xpu & BIT(0)) && (user == DISP_VIDLE_USER_FOR_FRAME))
		return;

	tracing_mark_write(trace_buf_release[0][0]);
	mutex_lock(&g_priv->excp_lock);

	switch (user) {
	case DISP_VIDLE_USER_NST_LOCK:
		user_cnt = atomic_dec_if_positive(&g_user_9);
		dpc_mmp(user_9, user_cnt == 0 ? MMPROFILE_FLAG_END : MMPROFILE_FLAG_PULSE, 0, user_cnt);
		break;
	case DISP_VIDLE_USER_FOR_FRAME:
		user_cnt = atomic_dec_if_positive(&g_user_14);
		dpc_mmp(user_14, user_cnt == 0 ? MMPROFILE_FLAG_END : MMPROFILE_FLAG_PULSE, 0, user_cnt);
		break;
	case DISP_VIDLE_USER_CRTC:
		user_cnt = atomic_dec_if_positive(&g_user_16);
		dpc_mmp(user_16, user_cnt == 0 ? MMPROFILE_FLAG_END : MMPROFILE_FLAG_PULSE, 0, user_cnt);
		break;
	case DISP_VIDLE_USER_PQ:
		user_cnt = atomic_dec_if_positive(&g_user_17);
		dpc_mmp(user_17, user_cnt == 0 ? MMPROFILE_FLAG_END : MMPROFILE_FLAG_PULSE, 0, user_cnt);
		break;
	default:
		break;
	}

	if (excep_by_xpu & BIT(0)) {
		tracing_mark_write(trace_buf_release[1][0]);
		dpc_ap_ref_cnt(VOTE_CLR, user);

		tracing_mark_write(trace_buf_release[2][0]);
		dpc_pre_cg_ctrl(VOTE_CLR, false);

		tracing_mark_write(trace_buf_release[3][0]);
		dpc_mminfra_on_off(VOTE_CLR, user);
	} else {
		if (user == DISP_VIDLE_USER_FOR_FRAME)
			dpc_ap_vote_mmpc(VOTE_CLR, user);
		else {
			dpc_ap_ref_cnt(VOTE_CLR, user);
			dpc_pre_cg_ctrl(VOTE_CLR, false);
			dpc_mminfra_on_off(VOTE_CLR, user);

		}
	}

	atomic_dec(&excep_ret[user]);
	mutex_unlock(&g_priv->excp_lock);

	if (excep_by_xpu & BIT(0)) {
		tracing_mark_write(trace_buf_release[3][1]);
		tracing_mark_write(trace_buf_release[2][1]);
		tracing_mark_write(trace_buf_release[1][1]);
	}
	tracing_mark_write(trace_buf_release[0][1]);

	return;
}

static void dpc_gce_ref_cnt(struct cmdq_pkt *pkt, bool add, const enum mtk_vidle_voter_user user,
			    const u16 gpr, struct cmdq_reuse *reuse)
{
	u32 xpu_base = 0x31471700;	/* XPU6 */
	u32 mask = 0x1ff80000;
	GCE_COND_DECLARE;
	struct cmdq_operand lop, rop;
	const u16 dummy_voter = CMDQ_THR_SPR_IDX2;
	const u16 mtcmos_sta = CMDQ_THR_SPR_IDX1;
	const u16 global_sta = CMDQ_THR_SPR_IDX2;
	struct cmdq_poll_reuse poll_reuse = {0};

	GCE_COND_ASSIGN(pkt, CMDQ_THR_SPR_IDX3, 0);
	lop.reg = true;
	lop.idx = dummy_voter;
	rop.reg = false;
	rop.value = 0;

/*SPR2*/cmdq_pkt_read(pkt, NULL, 0x31350160, dummy_voter);

	if (add) {
		// if (dummy_voter == 0) vote hwccf
/*SPR3*/	GCE_IF(lop, R_CMDQ_EQUAL, rop);
		cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG, 0x1a1a1a, U32_MAX);

		if (!toggle_cg_fsm) {
			dpc_hwccf_vote(VOTE_SET, pkt, user, false, gpr);
			goto vote_out;
		}

		cmdq_pkt_poll_sleep(pkt, 0, 0x31410000, 0xfffe);	/* polling all fsm idle */
		cmdq_pkt_write(pkt, NULL, xpu_base, mask, U32_MAX);	/* vote xpu6 mtcmos voter */
		cmdq_pkt_poll_sleep(pkt, mask, xpu_base + 0x8, mask);	/* check xpu6 local enable */
		cmdq_pkt_poll_sleep(pkt, mask, 0x31413700, mask);	/* check global enable */

		/* polling status idle, timeout = 12us * 833 = ~10ms */
		if (reuse) {
			cmdq_pkt_poll_timeout_reuse(pkt, 0, SUBSYS_NO_SUPPORT, 0x3141131c, mask, 833, gpr, &poll_reuse);
			memcpy(reuse, &poll_reuse, sizeof(poll_reuse));
		} else
/*SPR1,2*/		cmdq_pkt_poll_timeout(pkt, 0, SUBSYS_NO_SUPPORT, 0x3141131c, mask, 833, gpr);

		/* 1. mtcmos_sta = ((read(0x3141131c) & mask) != 0) ? 1 : 0 */
		lop.reg = true;
		lop.idx = mtcmos_sta;
		rop.reg = false;
		/* 1.1 mtcmos_sta = read(0x3141131c) */
/*SPR1*/	cmdq_pkt_read_addr(pkt, 0x3141131c, mtcmos_sta);
		/* 1.2 mtcmos_sta = (mtcmos_sta >> 16) & (mask >> 16), rop.value type is u16 */
		rop.value = 16;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_RIGHT_SHIFT, mtcmos_sta, &lop, &rop);
		rop.value = (u16)(mask >> 16);
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_AND, mtcmos_sta, &lop, &rop);
		/* 1.3 mtcmos_sta = (mtcmos_sta != 0) ? 1 : 0 */
		rop.value = 0;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_NOT, mtcmos_sta, &lop, NULL);
		rop.value = 1;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_AND, mtcmos_sta, &lop, &rop);

		/* 2. global_sta = ((read(0x31413704) & mask) == 0) ? 1 : 0 */
		lop.reg = true;
		lop.idx = global_sta;
		rop.reg = false;
		/* 2.1 global_sta = read(0x31413704) & mask */
/*SPR2*/	cmdq_pkt_read_addr(pkt, 0x31413704, global_sta);
		/* 2.2 global_sta = (global_sta >> 16) & (mask >> 16), rop.value type is u16 */
		rop.value = 16;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_RIGHT_SHIFT, mtcmos_sta, &lop, &rop);
		rop.value = (u16)(mask >> 16);
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_AND, mtcmos_sta, &lop, &rop);
		/* 2.2 global_sta = (global_sta == 0) ? 1 : 0 */
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_NOT, global_sta, &lop, NULL);
		rop.value = 1;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_AND, global_sta, &lop, &rop);

		/* 3. mtcmos_sta = (mtcmos_sta && global_sta) */
		lop.reg = true;
		lop.idx = mtcmos_sta;
		rop.reg = true;
		rop.idx = global_sta;
/*SPR1*/	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_AND, mtcmos_sta, &lop, &rop);

		if (reuse)
			GCE_FI_REUSE(&reuse[3]);
		else
			GCE_FI;

		/* 4. if (mtcmos_sta == 1) then toggle cg47*/
		lop.reg = true;
		lop.idx = mtcmos_sta;
		rop.reg = false;
		rop.value = 1;
/*SPR3*/	GCE_IF(lop, R_CMDQ_EQUAL, rop);
			cmdq_pkt_write(pkt, NULL, 0x31471234, BIT(22), U32_MAX);/* set cg47 */
			cmdq_pkt_poll_sleep(pkt, BIT(22), 0x314122bc, BIT(22));	/* check en is set */
			cmdq_pkt_poll_sleep(pkt, 0, 0x314118bc, BIT(22));	/* check sta idle */
			cmdq_pkt_write(pkt, NULL, 0x31471238, BIT(22), U32_MAX);/* clr cg47 */
			cmdq_pkt_poll_sleep(pkt, 0, 0x314122bc, BIT(22));	/* check en is clr */
			cmdq_pkt_poll_sleep(pkt, 0, 0x314118bc, BIT(22));	/* check sta idle */

vote_out:
		if (reuse)
			GCE_FI_REUSE(&reuse[4]);
		else
			GCE_FI;

		cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC3_DTx_SW_TRIG(55), 1, U32_MAX);
		cmdq_pkt_write(pkt, NULL, 0x31350164, BIT(user), ~0);
	} else {
		// if (dummy_voter == 0) then hint underflow happened
		GCE_IF(lop, R_CMDQ_EQUAL, rop);
		cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC3_DTx_SW_TRIG(30), 1, U32_MAX);
		if (reuse)
			GCE_FI_REUSE(&reuse[5]);
		else
			GCE_FI;

		cmdq_pkt_write(pkt, NULL, 0x31350168, BIT(user), ~0);

		// if (dummy_voter == 0) unvote hwccf
		cmdq_pkt_read(pkt, NULL, 0x31350160, dummy_voter);
		GCE_IF(lop, R_CMDQ_EQUAL, rop);
		dpc_hwccf_vote(VOTE_CLR, pkt, user, false, gpr);
		cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC3_DTx_SW_TRIG(56), 1, U32_MAX);
		cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG, 0x000a00, U32_MAX);
		if (reuse)
			GCE_FI_REUSE(&reuse[6]);
		else
			GCE_FI;
	}
#ifdef IF_ZERO
	// ref cnt version
	GCE_COND_DECLARE;
	struct cmdq_operand lop, rop;
	const u16 refcnt = CMDQ_THR_SPR_IDX2;

	GCE_COND_ASSIGN(pkt, CMDQ_THR_SPR_IDX1, 0);
	lop.reg = true;
	lop.idx = refcnt;
	rop.reg = false;

	cmdq_pkt_read(pkt, NULL, 0x3135013c, refcnt);

	if (add) {
		// if (refcnt == 0)
		rop.value = 0;
		GCE_IF(lop, R_CMDQ_EQUAL, rop);
		dpc_hwccf_vote(VOTE_SET, pkt, user, false, gpr);
		if (reuse)
			GCE_FI_REUSE(&reuse[0]);
		else
			GCE_FI;

		// refcnt++
		rop.value = 1;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, refcnt, &lop, &rop);
		cmdq_pkt_write_reg_addr(pkt, 0x3135013c, refcnt, ~0);
		cmdq_pkt_write(pkt, NULL, 0x31350164, BIT(user), ~0);
	} else {
		// if (refcnt == 0) then hint underflow happened
		rop.value = 0;
		GCE_IF(lop, R_CMDQ_EQUAL, rop);
		cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC3_DTx_SW_TRIG(30), 1, U32_MAX);
		if (reuse)
			GCE_FI_REUSE(&reuse[1]);
		else
			GCE_FI;

		// if (refcnt == 1)
		rop.value = 1;
		cmdq_pkt_read(pkt, NULL, 0x3135013c, refcnt);
		GCE_IF(lop, R_CMDQ_EQUAL, rop);
		dpc_hwccf_vote(VOTE_CLR, pkt, user, false, gpr);
		if (reuse)
			GCE_FI_REUSE(&reuse[2]);
		else
			GCE_FI;

		// if (refcnt >= 1) then refcnt--
		cmdq_pkt_read(pkt, NULL, 0x3135013c, refcnt);
		GCE_IF(lop, R_CMDQ_GREATER_EQUAL, rop);
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_SUBTRACT, refcnt, &lop, &rop);
		cmdq_pkt_write_reg_addr(pkt, 0x3135013c, refcnt, ~0);
		cmdq_pkt_write(pkt, NULL, 0x31350168, BIT(user), ~0);
		if (reuse)
			GCE_FI_REUSE(&reuse[3]);
		else
			GCE_FI;
	}
#endif
}

static void dpc_vidle_power_keep_by_gce_v3(struct cmdq_pkt *pkt, const enum mtk_vidle_voter_user user,
					   const u16 gpr, void *reuse)
{
	// dpc_mmp(vlp_vote, MMPROFILE_FLAG_PULSE, BIT(user), 0x11111111);
	cmdq_pkt_wfe(pkt, g_priv->event_hwccf_vote);
	if (excep_by_xpu & BIT(1))
		dpc_gce_ref_cnt(pkt, true, user, gpr, (struct cmdq_reuse *)reuse);
	else {
		cmdq_pkt_poll_sleep(pkt, 0, 0x31414300, 0xffc0);		/* polling dpc to hwccf req idle */
		cmdq_pkt_poll_sleep(pkt, 0, 0x31414400, 0xc);			/* polling mminfra req idle */
		cmdq_pkt_write(pkt, NULL, g_priv->voter_set_pa, BIT(user), U32_MAX);
		cmdq_pkt_poll_sleep(pkt, 0, 0x31414300, 0xffc0);		/* polling dpc to hwccf req idle */
		cmdq_pkt_poll_sleep(pkt, 0, 0x31414400, 0xc);			/* polling mminfra req idle */
		cmdq_pkt_poll_sleep(pkt, 0x1ff80000, 0x31412900, 0x1ff80000);	/* polling mtcmos ack */
		cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG, 0x1a1a1a, U32_MAX);
	}
	cmdq_pkt_set_event(pkt, g_priv->event_hwccf_vote);
}

static void dpc_vidle_power_release_by_gce_v3(struct cmdq_pkt *pkt, const enum mtk_vidle_voter_user user, void *reuse)
{
	cmdq_pkt_wfe(pkt, g_priv->event_hwccf_vote);
	if (excep_by_xpu & BIT(1))
		dpc_gce_ref_cnt(pkt, false, user, 0, (struct cmdq_reuse *)reuse);
	else {
		cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG, 0x000a00, U32_MAX);
		cmdq_pkt_write(pkt, NULL, g_priv->voter_clr_pa, BIT(user), U32_MAX);
#ifdef IF_ZERO
		cmdq_pkt_poll_sleep(pkt, 0, 0x31414300, 0xffc0);		/* polling dpc to hwccf req idle */
		cmdq_pkt_poll_sleep(pkt, 0, 0x31414400, 0xc);			/* polling mminfra req idle */
#endif
	}
	cmdq_pkt_set_event(pkt, g_priv->event_hwccf_vote);
	// dpc_mmp(vlp_vote, MMPROFILE_FLAG_PULSE, BIT(user), 0x22222222);
}

static void dpc_power_clean_up_by_gce(struct cmdq_pkt *pkt)
{
	u32 val = 0;
	int user;

	val = readl(mmpc_dummy_voter);
	if (val == 0)
		return;
	for (user = 0; user < 32; user++) {
		if (val & (1 << user)) {
			DPCERR("user:%d did not release", user);
			cmdq_pkt_set_event(pkt, g_priv->event_hwccf_vote);
			dpc_vidle_power_release_by_gce_v3(pkt, user, NULL);
		}
	}
}

static void dpc_hwccf_vote(bool on, struct cmdq_pkt *pkt, const enum mtk_vidle_voter_user user, bool lock,
			   const u16 gpr)
{
	int ret = 0;
	u32 value = 0;
	u32 xpu_base = 0x31471700;	/* XPU6 */
	u32 mask = 0;
	unsigned long flags;

	switch (user) {
	case DISP_VIDLE_USER_DISP_VCORE:
		mask = BIT(18);		/* disp_vcore */
		break;
	default:
		mask = 0x1ff80000;	/* disp0ab disp1ab ovl012 mml012 */
		break;
	}

	if (pkt) {
		if (lock)
			cmdq_pkt_wfe(pkt, g_priv->event_hwccf_vote);
		if (on) {
			if (!toggle_cg_fsm) {
				cmdq_pkt_write(pkt, NULL, xpu_base, mask, U32_MAX);		/* vote mtcmos voter */
				cmdq_pkt_poll_sleep(pkt, mask, xpu_base + 0x8, mask);		/* check local en */
				cmdq_pkt_poll_sleep(pkt, mask, 0x31413700, mask);		/* check global en */
				cmdq_pkt_poll_sleep(pkt, 0, 0x3141131c, mask);			/* check status idle */
				cmdq_pkt_poll_sleep(pkt, mask, 0x31412900, mask);		/* check pm ack */
			}
		} else {
			if (toggle_cg_fsm)
				cmdq_pkt_poll_sleep(pkt, 0, 0x31410000, 0xfffe);	/* polling all fsm idle */

			cmdq_pkt_write(pkt, NULL, xpu_base + 0x4, mask, U32_MAX);	/* unvote xpu6 mtcmos voter */
			cmdq_pkt_poll_sleep(pkt, 0, xpu_base + 0x8, mask);		/* check xpu6 local enable */
		}
		if (lock)
			cmdq_pkt_set_event(pkt, g_priv->event_hwccf_vote);
	} else {
		if (lock)
			spin_lock_irqsave(&g_priv->hwccf_ref_lock, flags);

		if (on) {
			if (toggle_cg_fsm) {
				/* polling all fsm idle */
				ret = readl_poll_timeout_atomic(hwccf_total_sta, value, !(value & 0xfffe), 1, 10000);
				if (ret < 0)
					goto err1;
			}

			writel(mask, hwccf_xpu0_mtcmos_set);			/* vote xpu0 mtcmos voter */

			ret = readl_poll_timeout_atomic(hwccf_xpu0_local_en, value, value & mask, 1, 2000);
			if (ret < 0)
				goto err2;
			ret = readl_poll_timeout_atomic(hwccf_global_en, value, value & mask, 1, 10000);
			if (ret < 0)
				goto err3;
			ret = readl_poll_timeout_atomic(hwccf_mtcmos_sta, value, !(value & mask), 1, 10000);
			if (ret < 0) {
				if (!toggle_cg_fsm)
					goto err4;

				if ((readl(hwccf_global_sta) & mask) == 0) {
					writel(BIT(16), hwccf_cg47_set);
					ret = readl_poll_timeout_atomic(hwccf_cg47_en, value, value & BIT(16), 1, 200);
					if (ret < 0)
						goto err5;
					ret = readl_poll_timeout_atomic(hwccf_cg47_sta, value, !(value & BIT(16)),
									1, 10000);
					if (ret < 0)
						goto err6;

					writel(BIT(16), hwccf_cg47_clr);
					ret = readl_poll_timeout_atomic(hwccf_cg47_en, value, !(value & BIT(16)),
									1, 200);
					if (ret < 0)
						goto err7;
					ret = readl_poll_timeout_atomic(hwccf_cg47_sta, value, !(value & BIT(16)),
									1, 10000);
					if (ret < 0)
						goto err8;
				}
			}
			ret = readl_poll_timeout_atomic(hwccf_mtcmos_pm_ack, value, value & mask, 1, 10000);
			if (ret < 0)
				goto err9;
		} else {
			if (toggle_cg_fsm) {
				/* polling all fsm idle */
				ret = readl_poll_timeout_atomic(hwccf_total_sta, value, !(value & 0xfffe), 1, 10000);
				if (ret < 0)
					goto err1;
			}

			writel(mask, hwccf_xpu0_mtcmos_clr);			/* vote xpu0 mtcmos voter */

			ret = readl_poll_timeout_atomic(hwccf_xpu0_local_en, value, !(value & mask), 1, 2000);
			if (ret < 0)
				goto err2;
		}

		if (lock)
			spin_unlock_irqrestore(&g_priv->hwccf_ref_lock, flags);
	}

	return;

err1:
	DPCERR("pwr(%u) mask(%#x) polling status(%#x) idle timeout 1, ret(%d)", on, mask, value, ret);
	goto err_dump;
err2:
	DPCERR("pwr(%u) mask(%#x) polling local_en(%#x) timeout, ret(%d)", on, mask, value, ret);
	goto err_dump;
err3:
	DPCERR("pwr(%u) mask(%#x) polling global_en(%#x) timeout, ret(%d)", on, mask, value, ret);
	goto err_dump;
err4:
	DPCERR("pwr(%u) mask(%#x) polling status(%#x) idle timeout 2, ret(%d)", on, mask, value, ret);
	goto err_dump;
err5:
	DPCERR("pwr(%u) mask(%#x) polling cg47_en(%#x) timeout, ret(%d)", on, mask, value, ret);
	goto err_dump;
err6:
	DPCERR("pwr(%u) mask(%#x) polling cg47_sta(%#x) idle timeout, ret(%d)", on, mask, value, ret);
	goto err_dump;
err7:
	DPCERR("pwr(%u) mask(%#x) polling cg47_clr(%#x) timeout, ret(%d)", on, mask, value, ret);
	goto err_dump;
err8:
	DPCERR("pwr(%u) mask(%#x) polling cg47_sta(%#x) idle timeout 2, ret(%d)", on, mask, value, ret);
	goto err_dump;
err9:
	DPCERR("pwr(%u) mask(%#x) polling mtcmos pm ack (%#x) timeout, ret(%d)", on, mask, value, ret);
	goto err_dump;
err_dump:
	if (lock)
		spin_unlock_irqrestore(&g_priv->hwccf_ref_lock, flags);
	dump_stack();
#if IS_ENABLED(CONFIG_MTK_HWCCF)
	clkchk_external_dump();
#endif
	BUG_ON(1);
}

static void dpc_get_avail_urate_freq(struct device *dev)
{
	int i = 0, sw_ver, opp_table_num;
	struct dev_pm_opp *opp;
	unsigned long freq;

	sw_ver = vdisp_get_chipid();
	if (sw_ver < 0) {
		DPCERR("Invalid sw_ver");
		return;
	}
	DPCDUMP("sw_ver:%d", sw_ver);

	// B0 chip use opp_table[1] if exist in dts
	opp_table_num = of_count_phandle_with_args(dev->of_node, "operating-points-v2", NULL);
	dev_pm_opp_of_add_table_indexed(dev,
		((sw_ver == 1) && (opp_table_num > 1)) ? 1 : 0);

	step_size = dev_pm_opp_get_opp_count(dev);
	g_urate_freq_steps = kcalloc(step_size, sizeof(unsigned long), GFP_KERNEL);
	freq = 0;
	while (!IS_ERR(opp = dev_pm_opp_find_freq_ceil(dev, &freq))) {
		if (i >= step_size)
			break;
		g_urate_freq_steps[i] = (freq / 1000000) * 16 * g_priv->ch_bw_urate / 100;
		DPCFUNC("g_urate_freq_steps[%d] = %lu", i, g_urate_freq_steps[i]);
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}
}

static bool dpc_is_power_on_v2(void)
{
	if (!g_priv->dispvcore_chk)
		return true;

	return readl(g_priv->dispvcore_chk) & g_priv->dispvcore_chk_mask;
}

static bool mminfra_is_power_on_v2(void)
{
	if (!g_priv->mminfra_chk)
		return true;

	/* subsys pm 0x31ac03dc bit1 */
	/* VLP_AO RSVD6(0x918) MM1_DONE bit0 */
	return readl(g_priv->mminfra_chk) & g_priv->mminfra_chk_mask;
}

static void dpc_monitor_config(struct cmdq_pkt *cmdq_handle, const u32 value)
{
	DDPDBG("%s:%d value:%d\n", __func__, __LINE__, value);
	if (cmdq_handle == NULL)
		writel(value, dpc_base + DISP_DPC_DDREN_MONITOR_CFG);
	else
		cmdq_pkt_write(cmdq_handle, NULL, g_priv->dpc_pa + DISP_DPC_DDREN_MONITOR_CFG, value, ~0);
}

static void dpc_analysis_v2(void)
{
	char msg[512] = {0};
	int written = 0;
	struct timespec64 ts = {0};
	int i;

	if (!dpc_is_power_on_v2()) {
		DPCFUNC("disp vcore is not power on");
		return;
	}

	if (dpc_pm_ctrl(true))
		return;

	ktime_get_ts64(&ts);
	written = scnprintf(msg, 512, "[%lu.%06lu]:", (unsigned long)ts.tv_sec,
		(unsigned long)DO_COMMMON_MOD(DO_COMMON_DIV(ts.tv_nsec, NSEC_PER_USEC), 1000000));

	written += scnprintf(msg + written, 512 - written,
		"vidle(%#x) dpc_en(%#x) voter(%#x) ",
		g_priv->vidle_mask, readl(dpc_base), readl(g_priv->voter_set_va));

	written += scnprintf(msg + written, 512 - written,
		"vdisp[cfg val](%#04x %#04x)(%#04x %#04x) swreq4(%#x) disp_sel(%#x) ",
		readl(dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_CFG),
		readl(dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_VAL),
		readl(dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_CFG),
		readl(dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_VAL),
		vdisp_dvfsrc_sw_req4 ? (readl(vdisp_dvfsrc_sw_req4) & 0x70) >> 4 : 0,
		clk_disp_sel ? (readl(clk_disp_sel) & 0x0f000000) >> 24 : 0);

	if (g_priv->mmsys_id == MMSYS_MT6993) {
		written += scnprintf(msg + written, 512 - written,
			"dram[cfg hrt srt](%#010x %#06x %#06x) ",
			readl(dpc_base + DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG),
			(readl(dpc_base + g_priv->ch_bw_cfg[24].offset)) & 0xfff,
			(readl(dpc_base + g_priv->ch_bw_cfg[25].offset) >> g_priv->ch_bw_cfg[25].shift) & 0xfff);
	} else {
		written += scnprintf(msg + written, 512 - written,
			"total[cfg hrt srt](%#010x %#06x %#06x)(%#010x %#06x %#06x) ",
			readl(dpc_base + DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG),
			readl(dpc_base + DISP_REG_DPC_DISP_HIGH_HRT_BW),
			readl(dpc_base + DISP_REG_DPC_DISP_SW_SRT_BW),
			readl(dpc_base + DISP_REG_DPC_MML_HRTBW_SRTBW_CFG),
			readl(dpc_base + DISP_REG_DPC_MML_SW_HRT_BW),
			readl(dpc_base + DISP_REG_DPC_MML_SW_SRT_BW));
	}

	written += scnprintf(msg + written, 512 - written,
		"[ddremi mminfra](%#010x %#08x)(%#010x %#08x) ",
		readl(dpc_base + DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG),
		readl(dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG),
		readl(dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG),
		readl(dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG));

	if (unlikely(dump_to_kmsg))
		DPCDUMP("%s", msg);
	else
		mtk_dprec_logger_pr(DPREC_LOGGER_DUMP, "%s\n", msg);

	if (g_priv->mmsys_id == MMSYS_MT6991) {
		written = scnprintf(msg, 512, "mtcmos(%#x %#x %#x %#x %#x %#x) ",
			readl(dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_DIS0].cfg),
			readl(dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_DIS1].cfg),
			readl(dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_OVL0].cfg),
			readl(dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_OVL1].cfg),
			readl(dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_MML1].cfg),
			readl(dpc_base + g_priv->mtcmos_cfg[DPC_SUBSYS_MML0].cfg));

		written += scnprintf(msg + written, 512 - written,
			"ch[hrt srt](%#04x %#04x %#04x %#04x)(%#04x %#04x %#04x %#04x) dt",
			(readl(dpc_base + g_priv->ch_bw_cfg[2].offset)) & 0xfff,
			(readl(dpc_base + g_priv->ch_bw_cfg[6].offset) >> g_priv->ch_bw_cfg[6].shift) & 0xfff,
			(readl(dpc_base + g_priv->ch_bw_cfg[10].offset)) & 0xfff,
			(readl(dpc_base + g_priv->ch_bw_cfg[14].offset) >> g_priv->ch_bw_cfg[14].shift) & 0xfff,
			(readl(dpc_base + g_priv->ch_bw_cfg[0].offset)) & 0xfff,
			(readl(dpc_base + g_priv->ch_bw_cfg[4].offset) >> g_priv->ch_bw_cfg[4].shift) & 0xfff,
			(readl(dpc_base + g_priv->ch_bw_cfg[8].offset)) & 0xfff,
			(readl(dpc_base + g_priv->ch_bw_cfg[12].offset) >> g_priv->ch_bw_cfg[12].shift) & 0xfff);
	} else if (g_priv->mmsys_id == MMSYS_MT6993) {
		written = scnprintf(msg, 512, "mtcmos(%#x %#x %#x %#x %#x %#x) ",
			readl(dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_OVL0].cfg),
			readl(dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_DIS0A].cfg),
			readl(dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_DIS1A].cfg),
			readl(dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_MML0].cfg),
			readl(dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_MML1].cfg),
			readl(dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_MML2].cfg));

		written += scnprintf(msg + written, 512 - written,
			"ch[hrt srt w](%#04x %#04x %#04x %#04x)(%#04x %#04x %#04x %#04x)(%#04x %#04x) dt",
			(readl(dpc_base + g_priv->ch_bw_cfg[2].offset) >> g_priv->ch_bw_cfg[2].shift) & 0xfff,
			(readl(dpc_base + g_priv->ch_bw_cfg[6].offset) >> g_priv->ch_bw_cfg[6].shift) & 0xfff,
			(readl(dpc_base + g_priv->ch_bw_cfg[10].offset) >> g_priv->ch_bw_cfg[10].shift) & 0xfff,
			(readl(dpc_base + g_priv->ch_bw_cfg[14].offset) >> g_priv->ch_bw_cfg[14].shift) & 0xfff,
			(readl(dpc_base + g_priv->ch_bw_cfg[0].offset) >> g_priv->ch_bw_cfg[0].shift) & 0xfff,
			(readl(dpc_base + g_priv->ch_bw_cfg[4].offset) >> g_priv->ch_bw_cfg[4].shift) & 0xfff,
			(readl(dpc_base + g_priv->ch_bw_cfg[8].offset) >> g_priv->ch_bw_cfg[8].shift) & 0xfff,
			(readl(dpc_base + g_priv->ch_bw_cfg[12].offset) >> g_priv->ch_bw_cfg[12].shift) & 0xfff,
			(readl(dpc_base + g_priv->ch_bw_cfg[7].offset) >> g_priv->ch_bw_cfg[7].shift) & 0xfff,
			(readl(dpc_base + g_priv->ch_bw_cfg[11].offset) >> g_priv->ch_bw_cfg[11].shift) & 0xfff);
	}
	for (i = 0; i < DPC2_VIDLE_CNT; i ++) /* TODO: priv->dt_cnt */
		if (g_priv->dpc2_dt_usage[i].en)
			written += scnprintf(msg + written, 512 - written, "[%d]%u ",
				i, g_priv->dpc2_dt_usage[i].val);

	if (unlikely(dump_to_kmsg))
		DPCDUMP("%s", msg);
	else
		mtk_dprec_logger_pr(DPREC_LOGGER_DUMP, "%s\n", msg);

	dpc_pm_ctrl(false);
}

static void dpc_analysis_v3(void)
{
	char msg[512] = {0};
	int written = 0;
//	uint64_t time;
//	unsigned long rem_nsec;
	int i;

	if (!dpc_is_power_on_v2()) {
		DPCFUNC("disp vcore is not power on");
		return;
	}

	if (dpc_pm_ctrl(true))
		return;

//	time = sched_clock();
//	rem_nsec = do_div(time, NSEC_PER_SEC);
//	written = scnprintf(msg, 512, "[%5lu.%06lu]",
//			(unsigned long)time, DO_COMMON_DIV(rem_nsec, NSEC_PER_USEC));

	written = scnprintf(msg + written, 512 - written,
		"caps(%#x) dpc_en(%#x) voter(%#x) SWHW(%#x) 0(%#x) 6(%#x) ",
		g_priv->vidle_mask, readl(dpc_base), readl(mmpc_dummy_voter),
		readl(hwccf_mtcmos_en),
		readl(hwccf_xpu0_local_en),
		readl(hwccf_xpu6_local_en));

	written += scnprintf(msg + written, 512 - written,
		"vdisp[cfg val](%#04x %#04x)(%#04x %#04x) swreq4(%#x) disp_sel(%#x) ",
		readl(dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_CFG),
		readl(dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_VAL),
		readl(dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_CFG),
		readl(dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_VAL),
		vdisp_dvfsrc_sw_req4 ? (readl(vdisp_dvfsrc_sw_req4) & 0x70) >> 4 : 0,
		clk_disp_sel ? (readl(clk_disp_sel) & 0x0f000000) >> 24 : 0);

	written += scnprintf(msg + written, 512 - written,
		"dram[cfg hrt srt](%#010x %#06x %#06x) ",
		readl(dpc_base + DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG),
		(readl(dpc_base + g_priv->ch_bw_cfg[24].offset)) & 0xfff,
		(readl(dpc_base + g_priv->ch_bw_cfg[25].offset) >> g_priv->ch_bw_cfg[25].shift) & 0xfff);

	written += scnprintf(msg + written, 512 - written,
		"[ddremi mminfra](%#010x %#08x)(%#010x %#08x) ",
		readl(dpc_base + DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG),
		readl(dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG),
		readl(dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG),
		readl(dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG));

	if (unlikely(dump_to_kmsg))
		DPCDUMP("%s", msg);
	mtk_dprec_logger_pr(DPREC_LOGGER_DUMP, "%s\n", msg);

	written = scnprintf(msg, 512, "mtcmos(%#x %#x %#x %#x %#x) ",
		readl(dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_OVL0].cfg),
		readl(dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_DIS0A].cfg),
		readl(dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_DIS1A].cfg),
		readl(dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_MML1].cfg),
		readl(dpc_base + g_priv->mtcmos_cfg[DPC3_SUBSYS_MML2].cfg));

	written += scnprintf(msg + written, 512 - written,
		"ch[hrt srt w](%#04x %#04x %#04x %#04x)(%#04x %#04x %#04x %#04x)(%#04x %#04x) dt",
		(readl(dpc_base + g_priv->ch_bw_cfg[2].offset) >> g_priv->ch_bw_cfg[2].shift) & 0xfff,
		(readl(dpc_base + g_priv->ch_bw_cfg[6].offset) >> g_priv->ch_bw_cfg[6].shift) & 0xfff,
		(readl(dpc_base + g_priv->ch_bw_cfg[10].offset) >> g_priv->ch_bw_cfg[10].shift) & 0xfff,
		(readl(dpc_base + g_priv->ch_bw_cfg[14].offset) >> g_priv->ch_bw_cfg[14].shift) & 0xfff,
		(readl(dpc_base + g_priv->ch_bw_cfg[0].offset) >> g_priv->ch_bw_cfg[0].shift) & 0xfff,
		(readl(dpc_base + g_priv->ch_bw_cfg[4].offset) >> g_priv->ch_bw_cfg[4].shift) & 0xfff,
		(readl(dpc_base + g_priv->ch_bw_cfg[8].offset) >> g_priv->ch_bw_cfg[8].shift) & 0xfff,
		(readl(dpc_base + g_priv->ch_bw_cfg[12].offset) >> g_priv->ch_bw_cfg[12].shift) & 0xfff,
		(readl(dpc_base + g_priv->ch_bw_cfg[7].offset) >> g_priv->ch_bw_cfg[7].shift) & 0xfff,
		(readl(dpc_base + g_priv->ch_bw_cfg[11].offset) >> g_priv->ch_bw_cfg[11].shift) & 0xfff);

	for (i = 0; i < DPC2_VIDLE_CNT; i ++) /* TODO: priv->dt_cnt */
		if (g_priv->dpc2_dt_usage[i].en)
			written += scnprintf(msg + written, 512 - written, "[%d]%u ",
				i, g_priv->dpc2_dt_usage[i].val);

	if (unlikely(dump_to_kmsg))
		DPCDUMP("%s", msg);
	mtk_dprec_logger_pr(DPREC_LOGGER_DUMP, "%s\n", msg);

	dpc_pm_ctrl(false);
	// clkchk_external_dump();
}

static void dpc_dump(void)
{
	u32 i = 0;

	if (!dpc_is_power_on_v2()) {
		DPCFUNC("disp vcore is not power on");
		return;
	}

	if (dpc_pm_ctrl(true))
		return;

	for (i = dump_begin; i < (dump_begin + dump_lines); i++) {
		DPCDUMP("[0x%03X] %08X %08X %08X %08X", i * 0x10,
			readl(dpc_base + i * 0x10 + 0x0),
			readl(dpc_base + i * 0x10 + 0x4),
			readl(dpc_base + i * 0x10 + 0x8),
			readl(dpc_base + i * 0x10 + 0xc));
	}
	dpc_pm_ctrl(false);
}

static int dpc_smi_force_on_callback(struct notifier_block *nb, unsigned long action, void *data)
{
	int i;
	int ret = 0;

	DPCFUNC("action(%lu)", action);

	if (g_priv->root_dev) {
		/* only for dpc v2 */
		if (action == true) {
			dpc_pm_ctrl(true);
			dpc_vidle_power_keep_v3(DISP_VIDLE_USER_SMI_DUMP);
			ret = pm_runtime_get_sync(g_priv->root_dev);
			if (ret < 0)
				DPCERR("get root_dev failed(%d)", ret);
		} else {
			ret = pm_runtime_put_sync(g_priv->root_dev);
			if (ret < 0)
				DPCERR("put root_dev failed(%d)", ret);
			dpc_vidle_power_release_v3(DISP_VIDLE_USER_SMI_DUMP);
			dpc_pm_ctrl(false);
		}
		return NOTIFY_DONE;
	}

	if (action == true) {
		dpc_mminfra_on_off(VOTE_SET, DISP_VIDLE_USER_SMI_DUMP);
		for (i = 0; i < g_priv->pwr_clk_num; i++) {
			if (IS_ERR(g_priv->pwr_clk[i])) {
				DPCDUMP("%s invalid %d clk\n", __func__, i);
				return NOTIFY_DONE;
			}
			clk_prepare_enable(g_priv->pwr_clk[i]);
		}
		dpc_vidle_power_keep_v3(DISP_VIDLE_USER_SMI_DUMP);
	} else {
		dpc_vidle_power_release_v3(DISP_VIDLE_USER_SMI_DUMP);
		for (i = g_priv->pwr_clk_num - 1; i >= 0; i--) {
			if (IS_ERR(g_priv->pwr_clk[i])) {
				DPCDUMP("%s invalid %d clk\n", __func__, i);
				return NOTIFY_DONE;
			}
			clk_disable_unprepare(g_priv->pwr_clk[i]);
		}
		dpc_mminfra_on_off(VOTE_CLR, DISP_VIDLE_USER_SMI_DUMP);
	}

	return NOTIFY_DONE;
}

static int dpc_smi_pwr_get_if_in_use(void *data)
{
	if (hwccf_is_enabled(MM_HWCCF,HW_CCF_MTCMOS_GRP_0, HWCCF_VOTE, 18) != 1) {
		DPCFUNC("disp vcore mtcmos not on");
		return -1;
	}

	mtk_vidle_hint_update(VIDLE_HINT_SMI_DUMP);
	if (g_priv && g_priv->mtcmos_cfg && (g_priv->mtcmos_cfg[DPC3_SUBSYS_DIS1A].mode == DPC_MTCMOS_AUTO)) {
		mtk_vidle_config_ff(false);
		DPCFUNC("disable ff and add debounce");
	}

	return 0;
}

static int dpc_smi_pwr_get(void *data)
{
	DPCFUNC("+");
	return 0;
}

static int dpc_smi_pwr_put(void *data)
{
	DPCFUNC("-");
	return 0;
}

static struct smi_user_pwr_ctrl dpc_smi_pwr_funcs_v3 = {
	 .name = "disp_dpc",
	 .data = NULL,
	 .smi_user_id =  MTK_SMI_DISP,
	 .smi_user_get = dpc_smi_pwr_get,
	 .smi_user_put = dpc_smi_pwr_put,
	 .smi_user_get_if_in_use = dpc_smi_pwr_get_if_in_use,
};

static struct dpc_funcs funcs_v3 = {
	// .dpc_enable = dpc_enable_v3,
	// .dpc_config = dpc_config_v3,
	// .dpc_group_enable = dpc_group_enable_v3,
	// .dpc_mtcmos_auto = dpc_mtcmos_auto_v3,
	// .dpc_duration_update = dpc_duration_update_v3,
	// .dpc_mtcmos_vote = dpc_mtcmos_vote_v3,
	.dpc_dsi_pll_set = dpc_dsi_pll_set_v2,
	.dpc_clear_wfe_event = dpc_clear_wfe_event_v2,
	// .dpc_vidle_power_keep = dpc_vidle_power_keep_v3,
	// .dpc_vidle_power_release = dpc_vidle_power_release_v3,
	// .dpc_vidle_power_keep_by_gce = dpc_vidle_power_keep_by_gce_v3,
	// .dpc_vidle_power_release_by_gce = dpc_vidle_power_release_by_gce_v3,
	// .dpc_hrt_bw_set = dpc_hrt_bw_set_v3,
	// .dpc_srt_bw_set = dpc_srt_bw_set_v3,
	.dpc_dvfs_set = dpc_dvfs_set_v2,
	.dpc_dvfs_trigger = dpc_dvfs_trigger,
	.dpc_channel_bw_set_by_idx = dpc_channel_bw_set_by_idx_v2,
	// .dpc_analysis = dpc_analysis_v2,
	.dpc_debug_cmd = process_dbg_opt,
	.dpc_mminfra_on_off = dpc_mminfra_on_off,
	// .dpc_mtcmos_on_off = dpc_hwccf_vote,
	.dpc_monitor_config = dpc_monitor_config,
	.dpc_buck_status = dpc_buck_status,
};

static void process_dbg_opt(const char *opt)
{
	int ret = 0;
	u32 v1 = 0, v2 = 0, v3 = 0;
	u32 mminfra_hangfree_val = 0;

	if (strncmp(opt, "cap:", 4) == 0) {
		ret = sscanf(opt, "cap:0x%x\n", &v1);
		if (ret != 1) {
			DPCDUMP("[Waring] cap sscanf not match");
			goto err;
		}
		DPCDUMP("cap(0x%x->0x%x)", g_priv->vidle_mask, v1);
		g_priv->vidle_mask = v1;

		if (g_priv->vidle_mask & BIT(0)) {
			funcs_v3.dpc_vidle_power_keep = g_priv->power_keep;
			funcs_v3.dpc_vidle_power_release = g_priv->power_release;
			funcs_v3.dpc_vidle_power_keep_by_gce = g_priv->power_keep_by_gce;
			funcs_v3.dpc_vidle_power_release_by_gce = g_priv->power_release_by_gce;
			funcs_v3.dpc_mtcmos_auto = g_priv->set_mtcmos;
			funcs_v3.dpc_config = g_priv->config;

			mtk_vidle_register(&funcs_v3, DPC_VER3);
			mml_dpc_register(&funcs_v3, DPC_VER3);
			mdp_dpc_register(&funcs_v3, DPC_VER2);
			mtk_vdisp_dpc_register(&funcs_v3);
		}
	} else if (strncmp(opt, "vote:", 5) == 0) {
		ret = sscanf(opt, "vote:%u\n", &v1);
		if (ret != 1) {
			DPCDUMP("[Waring]vote sscanf not match");
			goto err;
		}
		if (v1 == 1)
			writel(0xffffffff, g_priv->voter_clr_va);
		else
			writel(0xffffffff, g_priv->voter_set_va);
	} else if (strncmp(opt, "swreq:", 6) == 0) {
		ret = sscanf(opt, "swreq:%u\n", &v1);
		if (ret != 1)
			goto err;
		if (vdisp_dvfsrc_sw_req4)
			writel(v1 << 4, vdisp_dvfsrc_sw_req4);
	} else if (strncmp(opt, "dt:", 3) == 0) {
		ret = sscanf(opt, "dt:%u,%u\n", &v1, &v2);
		if (ret != 2) {
			DPCDUMP("dt:2,1000 => set dt(4) counter as 1000us");
			goto err;
		}

		if (v2 <= 0)
			g_priv->dpc2_dt_usage[v1].en = 0;
		else
			g_priv->dpc2_dt_usage[v1].en = 1;

		dpc_dt_set_update((u16)v1, v2);
	}

	/* The commands after this line needs to check power-on */
	if (!dpc_is_power_on_v2()) {
		DPCFUNC("disp vcore is not power on");
		return;
	}

	if (dpc_pm_ctrl(true))
		return;

	if (g_priv->mminfra_hangfree) {
		/* disable devapc power check temporarily, the value usually not changed after boot */
		mminfra_hangfree_val = readl(g_priv->mminfra_hangfree);
		writel(mminfra_hangfree_val & ~0x1, g_priv->mminfra_hangfree);
	}

	if (strncmp(opt, "wr:", 3) == 0) {
		ret = sscanf(opt, "wr:0x%x=0x%x\n", &v1, &v2);
		if (ret != 2)
			goto err;
		DPCFUNC("(%pK)=(%x)", dpc_base + v1, v2);
		writel(v2, dpc_base + v1);
	} else if (strncmp(opt, "ww:", 3) == 0) {
		ret = sscanf(opt, "ww:0x%x=0x%x\n", &v1, &v2);
		if (ret != 2)
			goto err;
		debug_reg = ioremap(v1, 0x4);
		writel(v2, debug_reg);
		DPCDUMP("(0x%X)=(0x%X)", v1, readl(debug_reg));
		iounmap(debug_reg);
	} else if (strncmp(opt, "rd:", 3) == 0) {
		ret = sscanf(opt, "rd:0x%x\n", &v1);
		if (ret != 1)
			goto err;
		debug_reg = ioremap(v1, 0x4);
		DPCDUMP("(0x%X)=(0x%X)", v1, readl(debug_reg));
		iounmap(debug_reg);
	} else if (strncmp(opt, "channel:", 8) == 0) {
		ret = sscanf(opt, "channel:%u,%u,%u\n", &v1, &v2, &v3);
		if (ret != 3) {
			DPCDUMP("channel:0,2,1000 => ch(2) bw(1000)MB by subsys(0)");
			goto err;
		}
		dpc_ch_bw_set_v2(v1, v2, v3);
		DPCDUMP("dpc_ch_bw_set subsys(%u) idx(%u) bw(%u)", v1, v2, v3);
	} else if (strncmp(opt, "vdisp:", 6) == 0) {
		ret = sscanf(opt, "vdisp:%u,%u\n", &v1, &v2);
		if (ret != 2) {
			DPCDUMP("vdisp:0,4 => level(4) by subsys(0)");
			goto err;
		}
		dpc_dvfs_set_v2(v1, v2, true);
	} else if (strncmp(opt, "hrt_bw:", 7) == 0) {
		ret = sscanf(opt, "hrt_bw:%u\n", &v1);
		if (ret != 1)
			goto err;

		g_priv->hrt_bw_set(0, v1, true);
	} else if (strncmp(opt, "srt_bw:", 7) == 0) {
		ret = sscanf(opt, "srt_bw:%u\n", &v1);
		if (ret != 1)
			goto err;

		g_priv->srt_bw_set(0, v1, true);
	}  else if (strncmp(opt, "presz:", 6) == 0) {
		ret = sscanf(opt, "presz:%u,%u\n", &v1, &v2);
		if (ret != 2) {
			DPCDUMP("presz:500,8333 => update dt(8333) by presz(500), unset by presz(0)");
			goto err;
		}
		debug_presz = v1;
		g_priv->duration_update(v2);
	} else if (strncmp(opt, "force_rsc:", 10) == 0) {
		ret = sscanf(opt, "force_rsc:%u\n", &v1);
		if (ret != 1) {
			DPCDUMP("[Waring]force_rsc sscanf not match");
			goto err;
		}
		if (v1) {
			writel(0x000D000D, dpc_base + DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG);
			writel(0x000D000D, dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG);
			writel(0x181818, dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG);
			writel(0x181818, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);
			writel(0x181818, dpc_base + DISP_DPC2_DISP_26M_PMIC_VCORE_OFF_CFG);
			writel(0x181818, dpc_base + DISP_DPC2_MML_26M_PMIC_VCORE_OFF_CFG);
		} else {
			writel(0x00050005, dpc_base + DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG);
			writel(0x00050005, dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG);
			writel(0x080808, dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG);
			writel(0x080808, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);
			writel(0x080808, dpc_base + DISP_DPC2_DISP_26M_PMIC_VCORE_OFF_CFG);
			writel(0x080808, dpc_base + DISP_DPC2_MML_26M_PMIC_VCORE_OFF_CFG);
		}
	} else if (strncmp(opt, "dump", 4) == 0) {
		dpc_dump();
	} else if (strncmp(opt, "analysis", 8) == 0) {
		g_priv->analysis();
	} else if (strncmp(opt, "thread:", 7) == 0) {
		ret = sscanf(opt, "thread:%u\n", &v1);
		if (ret != 1) {
			DPCDUMP("[Waring]thread sscanf not match");
			goto err;
		}
		if (v1 == 1) {
			g_priv->mtcmos_vote(DPC_SUBSYS_DIS0, 6, true);
			g_priv->mtcmos_vote(DPC_SUBSYS_DIS1, 6, true);
			g_priv->mtcmos_vote(DPC_SUBSYS_OVL0, 6, true);
			g_priv->mtcmos_vote(DPC_SUBSYS_OVL1, 6, true);
			g_priv->mtcmos_vote(DPC_SUBSYS_MML1, 6, true);
		} else {
			g_priv->mtcmos_vote(DPC_SUBSYS_DIS0, 6, false);
			g_priv->mtcmos_vote(DPC_SUBSYS_DIS1, 6, false);
			g_priv->mtcmos_vote(DPC_SUBSYS_OVL0, 6, false);
			g_priv->mtcmos_vote(DPC_SUBSYS_OVL1, 6, false);
			g_priv->mtcmos_vote(DPC_SUBSYS_MML1, 6, false);
		}
	} else if (strncmp(opt, "rtff:", 5) == 0) {
		ret = sscanf(opt, "rtff:%u\n", &v1);
		if (ret != 1) {
			DPCDUMP("[Waring]rtff sscanf not match");
			goto err;
		}
		writel(v1, g_priv->rtff_pwr_con);
	} else if (strncmp(opt, "vcore:", 6) == 0) {
		ret = sscanf(opt, "vcore:%u\n", &v1);
		if (ret != 1) {
			DPCDUMP("[Waring]vcore sscanf not match");
			goto err;
		}
		if (v1)
			writel(v1, g_priv->vcore_mode_set_va);
		else
			writel(v1, g_priv->vcore_mode_clr_va);
	} else if (strncmp(opt, "unlink:", 7) == 0) {
		ret = sscanf(opt, "unlink:%u,%u\n", &v1, &v2);
		if (ret != 2) {
			DPCDUMP("unlink:4,1 => unlink ovl0; unlink:4,0 => link ovl0");
			goto err;
		}
		/*
		 *  0 DPC3_SUBSYS_DIS0A,  19 ocip_dis0a_mtcmos_req
		 *  1 DPC3_SUBSYS_DIS0B,  20 ocip_dis0b_mtcmos_req
		 *  2 DPC3_SUBSYS_DIS1A,  21 ocip_dis1a_mtcmos_req
		 *  3 DPC3_SUBSYS_DIS1B,  22 ocip_dis1b_mtcmos_req
		 *  4 DPC3_SUBSYS_OVL0,   23 ocip_ovl0_mtcmos_req
		 *  5 DPC3_SUBSYS_OVL1,   24 ocip_ovl1_mtcmos_req
		 *  6 DPC3_SUBSYS_OVL2,   25 ocip_ovl2_mtcmos_req
		 *  7 DPC3_SUBSYS_MML0,   26 ocip_mml0_mtcmos_req
		 *  8 DPC3_SUBSYS_MML1,   27 ocip_mml1_mtcmos_req
		 *  9 DPC3_SUBSYS_MML2,   28 ocip_mml2_mtcmos_req
		 * 10 DPC3_SUBSYS_DPTX,   30 ocip_disp_dptx_mtcmos_req (notice idx!!!)
		 * 11 DPC3_SUBSYS_PERI,   29 ocip_disp_peri_mtcmos_req (notice idx!!!)
		 */
#if IS_ENABLED(CONFIG_MTK_HWCCF)
		if (v1 == 100) {
			hwccf_voter_ctrl(MM_HWCCF, HW_CCF_CG_GRP_51, v2 ? HWCCF_VOTE : HWCCF_UNVOTE, 19);
			hwccf_voter_ctrl(MM_HWCCF, HW_CCF_CG_GRP_51, v2 ? HWCCF_VOTE : HWCCF_UNVOTE, 20);
			hwccf_voter_ctrl(MM_HWCCF, HW_CCF_CG_GRP_51, v2 ? HWCCF_VOTE : HWCCF_UNVOTE, 21);
			hwccf_voter_ctrl(MM_HWCCF, HW_CCF_CG_GRP_51, v2 ? HWCCF_VOTE : HWCCF_UNVOTE, 22);
			hwccf_voter_ctrl(MM_HWCCF, HW_CCF_CG_GRP_51, v2 ? HWCCF_VOTE : HWCCF_UNVOTE, 23);
			hwccf_voter_ctrl(MM_HWCCF, HW_CCF_CG_GRP_51, v2 ? HWCCF_VOTE : HWCCF_UNVOTE, 24);
			hwccf_voter_ctrl(MM_HWCCF, HW_CCF_CG_GRP_51, v2 ? HWCCF_VOTE : HWCCF_UNVOTE, 25);
			hwccf_voter_ctrl(MM_HWCCF, HW_CCF_CG_GRP_51, v2 ? HWCCF_VOTE : HWCCF_UNVOTE, 26);
			hwccf_voter_ctrl(MM_HWCCF, HW_CCF_CG_GRP_51, v2 ? HWCCF_VOTE : HWCCF_UNVOTE, 27);
			hwccf_voter_ctrl(MM_HWCCF, HW_CCF_CG_GRP_51, v2 ? HWCCF_VOTE : HWCCF_UNVOTE, 28);
		} else
			hwccf_voter_ctrl(MM_HWCCF, HW_CCF_CG_GRP_51, v2 ? HWCCF_VOTE : HWCCF_UNVOTE, v1 + 19);
#endif
	}

	if (g_priv->mminfra_hangfree) {
		/* enable devapc power check */
		writel(mminfra_hangfree_val, g_priv->mminfra_hangfree);
	}

	dpc_pm_ctrl(false);

	return;
err:
	DPCERR();
	(void)dpc_ap_ref_cnt(0, 0);
	(void)dpc_gce_ref_cnt(0, 0, 0, 0, 0);
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
static ssize_t fs_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
	const u32 debug_bufmax = 512 - 1;
	size_t ret;
	char cmd_buffer[512] = {0};
	char *tok, *buf;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&cmd_buffer, ubuf, count))
		return -EFAULT;

	cmd_buffer[count] = 0;
	buf = cmd_buffer;
	DPCFUNC("%s", cmd_buffer);

	while ((tok = strsep(&buf, " ")) != NULL)
		process_dbg_opt(tok);

	return ret;
}

static const struct file_operations debug_fops = {
	.write = fs_write,
};
#endif

static struct mtk_dpc mt6991_dpc_driver_data = {
	.mmsys_id = MMSYS_MT6991,
	.mtcmos_cfg = mt6991_mtcmos_cfg,
	.subsys_cnt = DPC_SUBSYS_CNT,
	.ch_bw_cfg = mt6991_ch_bw_cfg,
	.vdisp_dvfsrc_idle_mask = 0xc00000,
	.dispvcore_chk_mask = BIT(29),
	.mminfra_chk_mask = BIT(0),
	.set_mtcmos = mt6991_set_mtcmos,
	.disp_irq_handler = mt6991_irq_handler,
	.duration_update = dpc_duration_update_v2,
	.enable = dpc_enable_v2,
	.res_init = dpc_res_init_v2,
	.dt_follow_cfg = 0x3f3c,
	.dpc2_dt_usage = mt6991_dt_usage,
	.total_srt_unit = 64,
	.total_hrt_unit = 64,
	.srt_emi_efficiency = 13715,			// multiply (1.33 * 33/32(TCU)) = 1.3715
	.hrt_emi_efficiency = 8242,			// divide 0.85 * 33/32(TCU) = *100/82.4242
	.ch_bw_urate = 70,				// divide 0.7
	.vcp_is_alive = true,
	.dsi_ck_keep_mask = BIT(2),
	.vdisp_level_cnt = 5,
	.hrt_bw_set = dpc_hrt_bw_set_v2,
	.srt_bw_set = dpc_srt_bw_set_v2,
	.mtcmos_vote = dpc_mtcmos_vote_v2,
	.group_enable = dpc_group_enable_v2,
	.power_keep = dpc_vidle_power_keep_v2,
	.power_release = dpc_vidle_power_release_v2,
	.power_keep_by_gce = dpc_vidle_power_keep_by_gce_v2,
	.power_release_by_gce = dpc_vidle_power_release_by_gce_v2,
	.config = dpc_config_v2,
	.analysis = dpc_analysis_v2,
};

static struct mtk_dpc mt6993_dpc_driver_data = {
	.mmsys_id = MMSYS_MT6993,
	.mtcmos_cfg = mt6993_mtcmos_cfg,
	.subsys_cnt = DPC3_SUBSYS_CNT,
	.ch_bw_cfg = mt6993_ch_bw_cfg,
	.vdisp_dvfsrc_idle_mask = 0xc00,
	.dispvcore_chk_mask = BIT(30)|BIT(31),
	.mminfra_chk_mask = BIT(0),
	.set_mtcmos = mt6993_set_mtcmos,
	.disp_irq_handler = mt6993_irq_handler,
	.duration_update = dpc_duration_update_v3,
	.enable = dpc_enable_v3,
	.res_init = dpc_res_init_v3,
	.dt_follow_cfg = 0x3f3c,
	.dpc2_dt_usage = mt6993_dt_usage,
	.total_srt_unit = 64,
	.total_hrt_unit = 64,
	.srt_emi_efficiency = 13715,			// multiply (1.33 * 33/32(TCU)) = 1.3715
	.hrt_emi_efficiency = 8242,			// divide 0.85 * 33/32(TCU) = *100/82.4242
	.ch_bw_urate = 70,				// divide 0.7
	.vcp_is_alive = true,
	.dsi_ck_keep_mask = BIT(2),
	.vdisp_level_cnt = 6,
	.hrt_bw_set = dpc_hrt_bw_set_v3,
	.srt_bw_set = dpc_srt_bw_set_v3,
	.mtcmos_vote = dpc_mtcmos_vote_v3,
	.group_enable = dpc_group_enable_v3,
	.power_keep = dpc_vidle_power_keep_v3,
	.power_release = dpc_vidle_power_release_v3,
	.power_keep_by_gce = dpc_vidle_power_keep_by_gce_v3,
	.power_release_by_gce = dpc_vidle_power_release_by_gce_v3,
	.config = dpc_config_v3,
	.analysis = dpc_analysis_v3,
};

static const struct of_device_id mtk_dpc_driver_v3_dt_match[] = {
	{.compatible = "mediatek,mt6991-disp-dpc-v3", .data = &mt6991_dpc_driver_data},
	{.compatible = "mediatek,mt6993-disp-dpc-v3", .data = &mt6993_dpc_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_dpc_driver_v3_dt_match);

static int mtk_dpc_probe_v3(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dpc *priv;
	struct clk *clk;
	const struct of_device_id *of_id;
	int ret = 0, genpd_num = 0, clk_num = 0, i;
#if defined(DISP_VIDLE_ENABLE)
	int sw_ver = 0;
#endif

	DPCFUNC("+");

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	of_id = of_match_device(mtk_dpc_driver_v3_dt_match, dev);
	if (!of_id) {
		DPCERR("DPC device match failed\n");
		return -EPROBE_DEFER;
	}

	priv = (struct mtk_dpc *)of_id->data;
	if (priv == NULL) {
		DPCERR("invalid priv data\n");
		return -EPROBE_DEFER;
	}
	priv->pdev = pdev;
	priv->dev = dev;
	g_priv = priv;
	platform_set_drvdata(pdev, priv);

	mutex_init(&priv->dvfs_bw.lock);
	mutex_init(&priv->excp_lock);
	spin_lock_init(&priv->mtcmos_cfg_lock);
	spin_lock_init(&priv->skip_force_power_lock);
	spin_lock_init(&priv->hwccf_ref_lock);
	spin_lock_init(&priv->excp_spin_lock);

	genpd_num = of_count_phandle_with_args(dev->of_node, "power-domains", NULL);
	if (genpd_num > 0) {
		priv->pd_dev = dev;
		if (!pm_runtime_enabled(priv->pd_dev))
			pm_runtime_enable(priv->pd_dev);
		pm_runtime_irq_safe(priv->pd_dev);
	}

	if (of_find_property(dev->of_node, "root-dev", NULL)) {
		struct device_node *node = of_parse_phandle(dev->of_node, "root-dev", 0);
		struct platform_device *pdev = NULL;

		if (node)
			pdev = of_find_device_by_node(node);
		if (pdev) {
			priv->root_dev = &pdev->dev;
			if (!pm_runtime_enabled(priv->root_dev))
				pm_runtime_enable(priv->root_dev);
		}
	}

#if defined(DISP_VIDLE_ENABLE)
	if (of_property_read_u32(dev->of_node, "vidle-mask", &priv->vidle_mask)) {
		DPCERR("failed to get vidle mask:%#x", priv->vidle_mask);
		priv->vidle_mask = 0;
	}

	/* Vidle separate operation */
	// A0/B0 chip discrimination
	sw_ver = vdisp_get_chipid();
	if(sw_ver == 0) {
		priv->vidle_mask = (priv->vidle_mask & ~BIT(DPC_CAP_MMINFRA_PLL));
		DPCERR("E1: No MMINFRA_QOF: %#x", priv->vidle_mask);
	}

	if (of_property_read_s32(dev->of_node, "debug-irq", &debug_irq)) {
		DPCERR("failed to get vidle debug irq:%#x", debug_irq);
	}
#endif

	if (of_property_read_u32(dev->of_node, "mminfra-pwr-idx", &priv->mminfra_pwr_idx)) {
		DPCERR("failed to get mminfra-pwr-idx");
		priv->mminfra_pwr_idx = 0;
	}

	if (of_property_read_u32(dev->of_node, "mminfra-pwr-type", &priv->mminfra_pwr_type)) {
		DPCERR("failed to get mminfra-pwr-type");
		priv->mminfra_pwr_type = 0;
	}

	if (of_property_read_u16(dev->of_node, "event-vote-hwccf", &priv->event_hwccf_vote))
		DPCERR("failed to get event-vote-hwccf");

	/* platform setting */
	priv->res_init(priv);
	if (ret) {
		DPCERR("res init failed:%d", ret);
		return ret;
	}

	clk_num = of_count_phandle_with_args(dev->of_node, "clocks", "#clock-cells");
	if (clk_num > 0) {
		priv->pwr_clk_num = clk_num;
		priv->pwr_clk = devm_kmalloc_array(dev, priv->pwr_clk_num,
						sizeof(*priv->pwr_clk), GFP_KERNEL);
		for (i = 0; i < priv->pwr_clk_num; i++) {
			clk = of_clk_get(dev->of_node, i);
			if (IS_ERR(clk)) {
				DPCERR("%s get %d clk failed\n", __func__, i);
				priv->pwr_clk_num = 0;
				return -EINVAL;
			}
			priv->pwr_clk[i] = clk;
		}
	} else {
		priv->pwr_clk_num = 0;
		priv->pwr_clk = NULL;
	}

	priv->smi_nb.notifier_call = dpc_smi_force_on_callback;
	mtk_smi_dbg_register_force_on_notifier(&priv->smi_nb);

#if IS_ENABLED(CONFIG_DEBUG_FS)
	priv->fs = debugfs_create_file("dpc_ctrl", S_IFREG | 0440, NULL, NULL, &debug_fops);
	if (IS_ERR(priv->fs))
		DPCERR("debugfs_create_file failed:%ld", PTR_ERR(priv->fs));
#endif
	dpc_get_avail_urate_freq(dev);

	dpc_mmp_init();

	funcs_v3.dpc_duration_update = priv->duration_update;
	funcs_v3.dpc_enable = priv->enable;
	funcs_v3.dpc_hrt_bw_set = priv->hrt_bw_set;
	funcs_v3.dpc_srt_bw_set = priv->srt_bw_set;
	funcs_v3.dpc_mtcmos_vote = priv->mtcmos_vote;
	funcs_v3.dpc_group_enable = priv->group_enable;
	funcs_v3.dpc_vidle_power_keep = priv->power_keep;
	funcs_v3.dpc_vidle_power_release = priv->power_release;
	funcs_v3.dpc_vidle_power_keep_by_gce = priv->power_keep_by_gce;
	funcs_v3.dpc_vidle_power_release_by_gce = priv->power_release_by_gce;
	funcs_v3.dpc_mtcmos_auto = priv->set_mtcmos;
	funcs_v3.dpc_config = priv->config;
	funcs_v3.dpc_analysis = priv->analysis;

	if (priv->mmsys_id == MMSYS_MT6993) {
		if (!has_cap(DPC_CAP_MTCMOS)) {
			funcs_v3.dpc_vidle_power_keep = NULL;
			funcs_v3.dpc_vidle_power_release = NULL;
			funcs_v3.dpc_vidle_power_keep_by_gce = NULL;
			funcs_v3.dpc_vidle_power_release_by_gce = NULL;
		}
		funcs_v3.dpc_mtcmos_vote = NULL;
		funcs_v3.dpc_dsi_pll_set = NULL;
		funcs_v3.dpc_check_pll = NULL;
		funcs_v3.dpc_mtcmos_on_off = dpc_hwccf_vote;
		funcs_v3.dpc_pre_cg_ctrl = dpc_pre_cg_ctrl;
		funcs_v3.dpc_power_clean_up_by_gce = dpc_power_clean_up_by_gce;
	}

	mtk_vidle_register(&funcs_v3, DPC_VER3);
	mml_dpc_register(&funcs_v3, DPC_VER3);
	mdp_dpc_register(&funcs_v3, DPC_VER2);
	mtk_vdisp_dpc_register(&funcs_v3);
	mtk_smi_dbg_register_pwr_ctrl_cb(&dpc_smi_pwr_funcs_v3);

	DPCFUNC("-");
	return ret;
}

static void mtk_dpc_remove_v3(struct platform_device *pdev)
{
	DPCFUNC();
}

static void mtk_dpc_shutdown_v3(struct platform_device *pdev)
{
	struct mtk_dpc *priv = platform_get_drvdata(pdev);

	priv->skip_force_power = true;
}

struct platform_driver mtk_dpc_driver_v3 = {
	.probe = mtk_dpc_probe_v3,
	.remove = mtk_dpc_remove_v3,
	.shutdown = mtk_dpc_shutdown_v3,
	.driver = {
		.name = "mediatek-disp-dpc-v3",
		.owner = THIS_MODULE,
		.of_match_table = mtk_dpc_driver_v3_dt_match,
	},
};

static int __init mtk_dpc_init_v3(void)
{
	DPCFUNC("+");
	platform_driver_register(&mtk_dpc_driver_v3);
	DPCFUNC("-");
	return 0;
}

static void __exit mtk_dpc_exit_v3(void)
{
	DPCFUNC();
}

module_init(mtk_dpc_init_v3);
module_exit(mtk_dpc_exit_v3);

MODULE_AUTHOR("William Yang <William-tw.Yang@mediatek.com>");
MODULE_DESCRIPTION("MTK Display Power Controller V3.0");
MODULE_LICENSE("GPL");
