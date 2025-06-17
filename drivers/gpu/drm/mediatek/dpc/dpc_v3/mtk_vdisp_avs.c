// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/ktime.h>
#include <linux/pm_opp.h>
#include <linux/nvmem-consumer.h>
#include <soc/mediatek/mmdvfs_public.h>

#include "mtk-mmdvfs-debug.h"
#include "mtk-mmdvfs-v3-memory.h"
#include "vcp_status.h"
#include "mtk_vdisp.h"
#include "mtk_vdisp_avs.h"
#include "mtk_log.h"

#define VDISP_IPI_ACK_TIMEOUT_US 1000

static void __iomem *g_aging_base;
static struct clk *g_mmdvfs_clk;
const struct mtk_vdisp_up_data *g_vdisp_up_data;
struct mtk_vdisp_avs_ipi_data {
	u32 func_id;
	u32 val;
};
static bool fast_en;
static bool vcp_is_alive = true;
static bool aging_force_disable;
static uint32_t vdisp_opp_num = 5;
struct vdisp_mmup_sram {
	void __iomem *mmup_base_va;
	void __iomem *vdisp_base_va; // mmup_base + ofst
	uint32_t ofst;
	bool ofst_is_init;
	bool is_valid;
};
static struct vdisp_mmup_sram g_vdisp_mmup_sram;
static struct notifier_block g_vdisp_vcp_nb;
static uint32_t *g_vdisp_efuse_val;
static unsigned long g_vdisp_max_freq = 728000000;
static uint32_t *g_vdisp_cal;

static void mtk_vdisp_set_mmup_sram_ofst(uint32_t ofst)
{
	if (!ofst) {
		VDISPERR("vdisp mmup sram ofst not ready!\n");
		return;
	}

	g_vdisp_mmup_sram.ofst = ofst;
	g_vdisp_mmup_sram.ofst_is_init = true;
}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
static void mtk_vdisp_mmup_sram_init(void)
{
	void __iomem *mmup_base;

	// do this only once
	if (g_vdisp_mmup_sram.is_valid || !g_vdisp_mmup_sram.ofst_is_init)
		return;

	mmup_base = vcp_get_sram_virt_ex();
	if (!mmup_base)
		return;

	g_vdisp_mmup_sram.mmup_base_va = mmup_base;
	g_vdisp_mmup_sram.vdisp_base_va =
		g_vdisp_mmup_sram.mmup_base_va + g_vdisp_mmup_sram.ofst;
	g_vdisp_mmup_sram.is_valid = true;
}
#endif

#define vdisp_avs_ipi_send_slot(id, value) \
	mtk_vdisp_avs_ipi_send((struct mtk_vdisp_avs_ipi_data) \
	{ .func_id = id, .val = value})
#define IPI_TIMEOUT_MS	(200U)
static int mtk_vdisp_avs_ipi_send(struct mtk_vdisp_avs_ipi_data data)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_ARM64)
	int ack = 0, i = 0;
	struct mtk_ipi_device *ipidev;

	ipidev = vcp_get_ipidev(VDISP_FEATURE_ID);
	if (!ipidev) {
		VDISPDBG("vcp_get_ipidev fail");
		return IPI_DEV_ILLEGAL;
	}
	ack = VDISP_SHRMEM_BITWISE_VAL & BIT(VDISP_AVS_IPI_ACK_BIT);
	ret = mtk_ipi_send(ipidev, IPI_OUT_VDISP, IPI_SEND_WAIT,
		&data, PIN_OUT_SIZE_VDISP, IPI_TIMEOUT_MS);
	if (ret != IPI_ACTION_DONE) {
		VDISPDBG("ipi fail: %d", ret);
		return ret;
	}
	while ((VDISP_SHRMEM_BITWISE_VAL & BIT(VDISP_AVS_IPI_ACK_BIT)) == ack) {
		udelay(1);
		i++;
		if (i >= VDISP_IPI_ACK_TIMEOUT_US) {
			VDISPDBG("ack timeout, vdisp_sram 0x%pa (mmup_sram 0x%pa, vdisp_ofst 0x%08x)",
				g_vdisp_mmup_sram.vdisp_base_va,
				g_vdisp_mmup_sram.mmup_base_va,
				g_vdisp_mmup_sram.ofst);
			return IPI_COMPL_TIMEOUT;
		}
	}
#endif
	return ret;
}

static bool wait_for_aging_ack_timeout(int flag)
{
	u32 i = 0;

	while (((VDISP_SHRMEM_BITWISE_VAL >> VDISP_AVS_AGING_ACK_BIT) & 1UL) != flag) {
		udelay(1);
		i++;
		if (i >= VDISP_IPI_ACK_TIMEOUT_US) {
			VDISPDBG("ack timeout, vdisp_sram 0x%pa (mmup_sram 0x%pa, vdisp_ofst 0x%08x)",
				g_vdisp_mmup_sram.vdisp_base_va,
				g_vdisp_mmup_sram.mmup_base_va,
				g_vdisp_mmup_sram.ofst);
			vdisp_avs_ipi_send_slot(FUNC_IPI_AGING_ACK, 0);
			return true;
		}
	}
	return false;
}

static int vdisp_avs_ipi_send_slot_enable_vcp
	(enum mtk_vdisp_avs_ipi_func_id func_id, uint32_t val)
{
	int ret = 0;

	ret = mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_RST);
	if (ret) {
		VDISPDBG("request mmdvfs rst fail");
		return ret;
	}
	if (!vcp_is_alive) {
		VDISPDBG("vcp is not alive, do nothing");
		goto release_vcp;
	}
	ret = vdisp_avs_ipi_send_slot(func_id, val);

release_vcp:
	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_RST);
	return ret;
}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
static int mtk_vdisp_avs_vcp_notifier
	(struct notifier_block *nb, unsigned long vcp_event, void *unused)
{
	switch (vcp_event) {
	case VCP_EVENT_READY:
		mtk_vdisp_mmup_sram_init();
		break;
	case VCP_EVENT_STOP:
		break;
	case VCP_EVENT_SUSPEND:
		vcp_is_alive = false;
		break;
	case VCP_EVENT_RESUME:
		vcp_is_alive = true;
		break;
	}

	return NOTIFY_DONE;
}
#endif

static void query_curr_ro(const struct mtk_vdisp_aging_data *aging_data)
{
	u32 ro_fresh_curr = 0, ro_aging_curr = 0;

	/* read vdisp avs RO fresh current */
	// 1. Enable Power on
	writel(0x0000001D, g_aging_base + aging_data->reg_pwr_ctrl);
	writel(0x0000001F, g_aging_base + aging_data->reg_pwr_ctrl);
	writel(0x0000005F, g_aging_base + aging_data->reg_pwr_ctrl);
	writel(0x0000004F, g_aging_base + aging_data->reg_pwr_ctrl);
	writel(0x0000006F, g_aging_base + aging_data->reg_pwr_ctrl);
	// 2. let rst_b =1’b0 (normal mode0 to  rst_b = 1’b1
	writel(0x00000000, g_aging_base + aging_data->reg_test);
	writel(0x00000100, g_aging_base + aging_data->reg_test);
	// 3. enable RO
	writel(aging_data->reg_ro_en0_fresh_val,
		g_aging_base + aging_data->reg_ro_en0);
	writel(0x00000000, g_aging_base + aging_data->reg_ro_en1);
	writel(aging_data->reg_ro_en2_fresh_val,
		g_aging_base + aging_data->reg_ro_en2);
	// 4. RO select
	writel(aging_data->ro_sel_0_fresh_val,
		g_aging_base + aging_data->ro_sel_0);
	writel(aging_data->ro_sel_1_fresh_val,
		g_aging_base + aging_data->ro_sel_1);
	// 5. Aptv_timer + cnt restart & clr
	writel(0x00022A00, g_aging_base + aging_data->win_cyc);
	udelay(20);
	writel(0x00002A00, g_aging_base + aging_data->win_cyc);
	// 6. Aptv_timer + cnt start & clr
	writel(0x00012A00, g_aging_base + aging_data->win_cyc);
	// 7. Count over
	udelay(460);
	// Get RO fresh current Value
	ro_fresh_curr = readl(g_aging_base + aging_data->ro_fresh) & 0xFFFF;
	// VDISPDBG("VDISP_AVS_RO_FRESH_CURR=%x", ro_fresh_curr);
	// 8. Power off
	writel(0x0000006F, g_aging_base + aging_data->reg_pwr_ctrl);
	writel(0x0000007F, g_aging_base + aging_data->reg_pwr_ctrl);
	writel(0x0000003F, g_aging_base + aging_data->reg_pwr_ctrl);
	writel(0x0000001F, g_aging_base + aging_data->reg_pwr_ctrl);
	writel(0x0000001E, g_aging_base + aging_data->reg_pwr_ctrl);
	writel(0x0000001C, g_aging_base + aging_data->reg_pwr_ctrl);

	/* read vdisp avs RO aging current */
	// // 1. Enable Power on
	// writel(0x0000001D, g_aging_base + aging_data->reg_pwr_ctrl);
	// writel(0x0000001F, g_aging_base + aging_data->reg_pwr_ctrl);
	// writel(0x0000005F, g_aging_base + aging_data->reg_pwr_ctrl);
	// writel(0x0000004F, g_aging_base + aging_data->reg_pwr_ctrl);
	// writel(0x0000006F, g_aging_base + aging_data->reg_pwr_ctrl);
	// 2. let rst_b =1’b0 (normal mode0 to  rst_b = 1’b1
	writel(0x00000000, g_aging_base + aging_data->reg_test);
	writel(0x00000100, g_aging_base + aging_data->reg_test);
	// 3. enable RO
	writel(aging_data->reg_ro_en0_aging_val,
		g_aging_base + aging_data->reg_ro_en0);
	writel(0x00000000, g_aging_base + aging_data->reg_ro_en1);
	writel(aging_data->reg_ro_en2_aging_val,
		g_aging_base + aging_data->reg_ro_en2);
	// 4. RO select
	writel(aging_data->ro_sel_0_aging_val,
		g_aging_base + aging_data->ro_sel_0);
	writel(aging_data->ro_sel_1_aging_val,
		g_aging_base + aging_data->ro_sel_1);
	// 5. Aptv_timer + cnt restart & clr
	writel(0x00022A00, g_aging_base + aging_data->win_cyc);
	udelay(20);
	writel(0x00002A00, g_aging_base + aging_data->win_cyc);
	// 6. Aptv_timer + cnt start & clr
	writel(0x00012A00, g_aging_base + aging_data->win_cyc);
	// 7. Count over
	udelay(460);
	// Get RO aging current Value
	ro_aging_curr = readl(g_aging_base + aging_data->ro_aging) & 0xFFFF;
	// VDISPDBG("VDISP_AVS_RO_AGING_CURR=%x", ro_aging_curr);

	/* update to share memory and send ipi to update */
	vdisp_avs_ipi_send_slot(FUNC_IPI_AGING_UPDATE, (ro_fresh_curr << 16) | ro_aging_curr);
}

void mtk_vdisp_avs_query_aging_val(struct device *dev)
{
	int ret;
	static ktime_t last_time;
	ktime_t cur_time = ktime_get();
	ktime_t elapse_time = cur_time - last_time;
	ktime_t t_ag;

	t_ag = fast_en ? 10*1e9 : 24*60*60*1e9; // 1day: 24*60*60*1e9

	if (aging_force_disable)
		return;

	if ((elapse_time < t_ag) && last_time)
		return;
	last_time = cur_time;

	if (!g_aging_base) {
		VDISPDBG("g_aging_base uninitialized, skip");
		return;
	}

	if (!g_mmdvfs_clk) {
		VDISPDBG("g_mmdvfs_clk uninitialized, skip");
		return;
	}

	if (!g_vdisp_up_data || !g_vdisp_up_data->avs ||
		!g_vdisp_up_data->avs->aging) {
		VDISPDBG("vdisp_data uninitialized, skip");
		return;
	}

	/* trigger VDISP OPP4 750mV */
	if (mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_DISP)) {
		VDISPDBG("request mmdvfs disp fail");
		return;
	}
	if (!vcp_is_alive) {
		VDISPDBG("vcp is not alive, skip");
		goto release_vcp;
	}

	if ((VDISP_SHRMEM_BITWISE_VAL & BIT(VDISP_AVS_AGING_ENABLE_BIT)) !=
		BIT(VDISP_AVS_AGING_ENABLE_BIT)) {
		VDISPDBG("aging disabled, skip");
		goto release_vcp;
	}

	if ((vdisp_avs_ipi_send_slot(FUNC_IPI_AGING_ACK, 1) != IPI_ACTION_DONE) ||
		wait_for_aging_ack_timeout(1))
		goto release_vcp;

	ret = clk_set_rate(g_mmdvfs_clk, g_vdisp_max_freq);
	if (ret) {
		VDISPDBG("request vdisp opp4 fail: %d", ret);
		if (vdisp_avs_ipi_send_slot(FUNC_IPI_AGING_ACK, 0) != IPI_ACTION_DONE)
			VDISPDBG("restore aging ack fail");
		goto release_vcp;
	}

	/* wait for VDISP_AVS_AGE_ACK to be set */
	if (wait_for_aging_ack_timeout(0))
		goto release_mmdvfs_clk;

	/* query current aging sensor value */
	query_curr_ro(g_vdisp_up_data->avs->aging);

	/* release VDISP opp4 */
release_mmdvfs_clk:
	ret = clk_set_rate(g_mmdvfs_clk, 0);
	if (ret)
		VDISPDBG("release vdisp opp request fail: %d", ret);

release_vcp:
	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_DISP);
}

void mtk_vdisp_set_clk(unsigned long rate)
{
	int ret = 0;
	ktime_t start_time, end_time;
	static unsigned long last_rate;

	if (rate == last_rate)
		return;

	if (!g_mmdvfs_clk) {
		VDISPDBG("g_mmdvfs_clk uninitialized, skip");
		return;
	}

	start_time = ktime_get();
	if (mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_DISP)) {
		VDISPDBG("request mmdvfs disp fail");
		return;
	}

	if (!vcp_is_alive) {
		VDISPDBG("vcp is not alive, skip");
		goto release_vcp;
	}

	ret = clk_set_rate(g_mmdvfs_clk, rate);
	if (ret)
		VDISPDBG("set rate(%lu) failed ret(%d)", rate, ret);

release_vcp:
	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_DISP);

	end_time = ktime_get();
	VDISPDBG("execution %lld us", ktime_us_delta(end_time, start_time));
}

int mtk_vdisp_up_analysis(void)
{
	char msg[512] = {0};
	int i = 0, ret = 0, idx_temp = 0, retry_time = 0, written = 0;
	uint32_t hist[VDISP_BUCK_HIST_REC_CNT*VDISP_BUCK_HIST_OBJ_CNT+1] = {0};

	// do nothing if not allow sleeping
	if(in_interrupt() || irqs_disabled() || !preemptible())
		return 0;

	if (!g_vdisp_up_data || !g_vdisp_up_data->buck_hist_support) {
		VDISPDBG("buck hist not support");
		return -1;
	}

	if (!g_vdisp_cal) {
		VDISPDBG("g_vdisp_cal not init");
		return -1;
	}

	ret = mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_RST);
	if (ret) {
		VDISPDBG("request mmdvfs rst fail");
		return ret;
	}
	if (!vcp_is_alive) {
		VDISPDBG("vcp is not alive, do nothing");
		mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_RST);
		return ret;
	}

	/* copy mmup info to kernel local variable */
	for (i = 0; i < vdisp_opp_num; i++)
		g_vdisp_cal[i] = VDISP_CAL(i);
	do {
		if (retry_time >= 3) {
			VDISPERR("buck hist read fail, retry_time(%d)", retry_time);
			break;
		}
		idx_temp = VDISP_SHRMEM_READ_CHK(VDISP_BUCK_HIST_IDX);
		for (i = 0; i <= VDISP_BUCK_HIST_REC_CNT*VDISP_BUCK_HIST_OBJ_CNT; i++)
			hist[i] = VDISP_SHRMEM_READ_CHK(VDISP_BUCK_HIST_BASE + 0x4*i);
		retry_time++;
	} while (idx_temp != VDISP_SHRMEM_READ_CHK(VDISP_BUCK_HIST_IDX));

	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_RST);

	if (idx_temp >= VDISP_BUCK_HIST_REC_CNT) {
		VDISPERR("errornous idx(%d)", idx_temp);
		ret = -1;
		return ret;
	}

	/* print mmup info */
	mtk_dprec_logger_pr(DPREC_LOGGER_DUMP, "== VDISP MMUP ANALYSIS ==\n");
	if (g_vdisp_efuse_val && g_vdisp_up_data->avs
		&& g_vdisp_up_data->avs->efuse && g_vdisp_up_data->avs->efuse->tbl)
		for (i = 0; i < g_vdisp_up_data->avs->efuse->num; i++) {
			if (!g_vdisp_up_data->avs->efuse->tbl[i].name)
				continue;
			mtk_dprec_logger_pr(DPREC_LOGGER_DUMP, "%s(0x%08x)\n",
				g_vdisp_up_data->avs->efuse->tbl[i].name, g_vdisp_efuse_val[i]);
		}
	written = scnprintf(msg, 512, "vdisp_cal:");
	for (i = 0; i < vdisp_opp_num; i++)
		written += scnprintf(msg + written, 512 - written,
			"[%d](%d) ", i, g_vdisp_cal[i]);
	mtk_dprec_logger_pr(DPREC_LOGGER_DUMP, "%s\n", msg);

	mtk_dprec_logger_pr(DPREC_LOGGER_DUMP,
		"vdisp_hist[i]: lvl,   volt,  temp,       step, ts\n");
	i = idx_temp;
	do {
		mtk_dprec_logger_pr(DPREC_LOGGER_DUMP,
			"vdisp_hist[%d]:   %d, %06d, %05d, 0x%08x, %d.%06d\n", i,
			hist[VDISP_BUCK_HIST_OBJ_CNT*i + VDISP_BUCK_HIST_LVL_OFST],
			hist[VDISP_BUCK_HIST_OBJ_CNT*i + VDISP_BUCK_HIST_VOLT_OFST],
			hist[VDISP_BUCK_HIST_OBJ_CNT*i + VDISP_BUCK_HIST_TEMP_OFST],
			hist[VDISP_BUCK_HIST_OBJ_CNT*i + VDISP_BUCK_HIST_STEP_OFST],
			hist[VDISP_BUCK_HIST_OBJ_CNT*i + VDISP_BUCK_HIST_SEC_OFST],
			hist[VDISP_BUCK_HIST_OBJ_CNT*i + VDISP_BUCK_HIST_USEC_OFST]);
		i = (i + 1) % VDISP_BUCK_HIST_REC_CNT;
	} while (i != idx_temp);

	return ret;
}

#if (IS_ENABLED(CONFIG_DEBUG_FS) | IS_ENABLED(CONFIG_PROC_FS))
static int parse_u32(const char *input, u32 *p_v1, u32 *p_v2, u32 fmt)
{
	int ret = 0;
	char *token, *end, *str;
	unsigned long v = 0;

	if (!p_v1)
		return -EINVAL;

	str = kstrdup(input, GFP_KERNEL);
	if (!str) {
		VDISPDBG("fail to allocate memory");
		return -ENOMEM;
	}

	end = str;
	token = strsep(&end, ",");
	if (!token) {
		ret = -EINVAL;
		if (end)
			goto free_str;
	}
	ret = kstrtoul(token, fmt, &v);
	if (ret)
		goto free_str;
	memcpy((void *)p_v1, (void *)&v, sizeof(u32));

	if ((*end == '\0') || !p_v2)
		goto free_str;
	ret = kstrtoul(end, fmt, &v);
	if (ret)
		goto free_str;
	memcpy((void *)p_v2, (void *)&v, sizeof(u32));

free_str:
	kfree(str);

	return ret;
}

static int mtk_vdisp_avs_dbg_opt(const char *opt)
{
	int ret = 0;
	u32 v1 = 0, v2 = 0;

	if (strncmp(opt + 4, "off:", 4) == 0) {
		if (parse_u32(opt + 8, &v1, &v2, 10)) {
			VDISPDBG("[Warning] avs:off parsing failed");
			return -EINVAL;
		}
		/* opp(v1); step(v2) max 31 steps */
		if ((v1 >= vdisp_opp_num || v2 >= 31)) {
			VDISPDBG("[Warning] avs:off invalid input");
			return -EINVAL;
		}
		/*Set opp and step*/
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_AVS_STEP, (v1 << 16) | v2);
		if (ret)
			return ret;
		/*Off avs*/
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_AVS_EN, 0);
		if (ret)
			return ret;
		ret = mmdvfs_debug_force_step(2, vdisp_opp_num - v1 - 1);
	} else if (strncmp(opt + 4, "off", 3) == 0) {
		/*Off avs*/
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_AVS_EN, 0);
	} else if (strncmp(opt + 4, "t_ag:", 5) == 0) {
		if (parse_u32(opt + 9, &v1, NULL, 10)) {
			VDISPDBG("[Warning] avs:t_ag parsing failed");
			return -EINVAL;
		}
		fast_en = (v1 != 0);
		ret = 0;
	} else if (strncmp(opt + 4, "on", 2) == 0) {
		/*On avs*/
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_AVS_EN, 1);
	} else if (strncmp(opt + 4, "dbg:on", 6) == 0) {
		/*On avs debug mode */
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_AVS_DBG_MODE, 1);
	} else if (strncmp(opt + 4, "dbg:off", 7) == 0) {
		/*Off avs debug mode*/
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_AVS_DBG_MODE, 0);
	} else if (strncmp(opt + 4, "rst_efuse", 9) == 0) {
		/* reset efuse */
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_RESET_EFUSE_VAR, 0);
	} else if (strncmp(opt + 4, "vir_efuse:", 10) == 0) {
		/* virtual efuse (ofs, val) */
		if (parse_u32(opt + 14, &v1, &v2, 16)) {
			VDISPDBG("[Warning] avs:vir_efuse parsing failed");
			return -EINVAL;
		}
		ret = vdisp_avs_ipi_send_slot_enable_vcp(v1, v2);
	} else if (strncmp(opt + 4, "set_temp:", 9) == 0) {
		/* set temperature */
		if (parse_u32(opt + 13, &v1, NULL, 10)) {
			VDISPDBG("[Warning] avs:set_temp parsing failed");
			return -EINVAL;
		}
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_SET_TEMP, v1);
	}

	return ret;
}

int mtk_vdisp_up_dbg_opt(const char *opt)
{
	int ret = 0;
	u32 v1 = 0, v2 = 0;

	if (strncmp(opt, "avs:", 4) == 0)
		return mtk_vdisp_avs_dbg_opt(opt);

	/* uP other opt */
	if (strncmp(opt + 3, "vdisp_read_level", 16) == 0) {
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_UNIT_TEST, UT_RD_LVL);
	} else if (strncmp(opt + 3, "vdisp_read_voltage", 18) == 0) {
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_UNIT_TEST, UT_RD_VOL);
	} else if (strncmp(opt + 3, "vdisp_update_level:-1", 21) == 0) {
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_RESTORE_FREERUN, 0);
	} else if (strncmp(opt + 3, "vdisp_update_level", 18) == 0) {
		if (parse_u32(opt + 22, &v1, NULL, 10)) {
			VDISPDBG("[Warning] up:vdisp_update_level parsing failed");
			return -EINVAL;
		}
		if (v1 >= vdisp_opp_num) {
			VDISPDBG("[Warning] up:vdisp_update_level invalid input");
			return -EINVAL;
		}
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_UNIT_TEST, (v1 << 16) | UT_WR_LVL);
	} else if (strncmp(opt + 3, "vdisp_on", 8) == 0) {
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_UNIT_TEST, UT_PWR_ON);
	} else if (strncmp(opt + 3, "vdisp_off", 9) == 0) {
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_UNIT_TEST, UT_PWR_OFF);
	} else if (strncmp(opt + 3, "arb:", 4) == 0) {
		/* arbitrary input */
		if (parse_u32(opt + 7, &v1, &v2, 10)) {
			VDISPDBG("[Warning] up:arb parsing failed");
			return -EINVAL;
		}
		ret = vdisp_avs_ipi_send_slot_enable_vcp(v1, v2);
	} else if (strncmp(opt + 3, "chg_stg:", 8) == 0) {
		/* change MMuP VDISP stage*/
		if (parse_u32(opt + 11, &v1, NULL, 10)) {
			VDISPDBG("[Warning] up:chg_stg parsing failed");
			return -EINVAL;
		}
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_CHANGE_STAGE, v1);
	} else if (strncmp(opt + 3, "analysis", 8) == 0) {
		ret = mtk_vdisp_up_analysis();
	}

	return ret;
}
#endif /* #if (IS_ENABLED(CONFIG_DEBUG_FS) | IS_ENABLED(CONFIG_PROC_FS)) */

int mtk_vdisp_efuse_probe(struct platform_device *pdev)
{
	uint32_t i, used_efuse_num;
	size_t len = 0;
	struct device *dev = &pdev->dev;
	struct nvmem_cell *cell = NULL;
	uint32_t *buf = NULL;
	const struct mtk_vdisp_efuse_lut *efuse_tbl;

	/* early return check */
	// already probed, return directly
	if (g_vdisp_efuse_val)
		return 0;

	// platform data not ready, return directly
	if (!g_vdisp_up_data || !g_vdisp_up_data->avs || !g_vdisp_up_data->avs->efuse)
		return 0;

	/* get efuse driver data */
	used_efuse_num = g_vdisp_up_data->avs->efuse->num;
	efuse_tbl = g_vdisp_up_data->avs->efuse->tbl;
	// platform data not valid, return directly
	if ((used_efuse_num == 0) || !efuse_tbl)
		return 0;

	/* store efuse values */
	g_vdisp_efuse_val = kcalloc(used_efuse_num, sizeof(uint32_t), GFP_KERNEL);
	if (!g_vdisp_efuse_val)
		return -1;

	for (i = 0; i < used_efuse_num; i++) {
		if (!efuse_tbl[i].cell_name)
			continue;

		/* get efuse buf */
		cell = nvmem_cell_get(dev, efuse_tbl[i].cell_name);
		if (IS_ERR(cell)) {
			if (PTR_ERR(cell) == -EPROBE_DEFER)
				return -EPROBE_DEFER;
			return -1;
		}

		buf = (uint32_t *)nvmem_cell_read(cell, &len);
		nvmem_cell_put(cell);

		if (IS_ERR(buf))
			return PTR_ERR(buf);

		g_vdisp_efuse_val[i] = *buf;
		VDISPDBG("efuse_val[%d] %s(0x%08x)", i,
			efuse_tbl[i].name ? efuse_tbl[i].name : "unnamed",
			g_vdisp_efuse_val[i]);

		kfree(buf);
	}

	return 0;
}

int mtk_vdisp_avs_probe(struct platform_device *pdev)
{
	int ret, sw_ver, opp_tbl_num;
	unsigned long freq = 0;
	struct dev_pm_opp *opp;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct clk *clk;
	const struct mtk_vdisp_data *vdisp_data = of_device_get_match_data(dev);
	void __iomem *mmup_sram_ofst_addr = NULL;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "aging_base");
	if (res) {
		g_aging_base = devm_ioremap(dev, res->start, resource_size(res));
		if (!g_aging_base) {
			VDISPERR("fail to ioremap aging_base: 0x%pa", &res->start);
			return -EINVAL;
		}
	}

	/* this register is used for passing vdisp mmup sram offset */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vdisp_ao_dummy");
	if (res) {
		mmup_sram_ofst_addr = devm_ioremap(dev, res->start, resource_size(res));
		if (!mmup_sram_ofst_addr) {
			VDISPERR("fail to ioremap vdisp_ao_dummy: 0x%pa", &res->start);
			return -EINVAL;
		}
		mtk_vdisp_set_mmup_sram_ofst(readl(mmup_sram_ofst_addr));
	}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
	if (!g_vdisp_vcp_nb.notifier_call) {
		g_vdisp_vcp_nb.notifier_call = mtk_vdisp_avs_vcp_notifier;
		vcp_A_register_notify_ex(VDISP_FEATURE_ID, &g_vdisp_vcp_nb);
	}
#endif

	clk = devm_clk_get(dev, "mmdvfs_clk");
	if (!IS_ERR(clk))
		g_mmdvfs_clk = clk;

	if (vdisp_data && vdisp_data->up)
		g_vdisp_up_data = vdisp_data->up;

	ret = mtk_vdisp_efuse_probe(pdev);
	if (ret)
		VDISPDBG("fail to get efuse");

	/* signal to uP start supporting AVS */
#if defined(DISP_AVS_SUPPORT)
	if(vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_MGK_SUPPORT_AVS, 1))
		VDISPDBG("fail to enable MGK support AVS");
#endif

	/* OPP related operation */
	// A0/B0 chip discrimination
	sw_ver = vdisp_get_chipid();
	if (sw_ver < 0)
		return 0;

	// B0 chip use opp_tbl[1] if exist in dts
	opp_tbl_num = of_count_phandle_with_args(dev->of_node, "operating-points-v2", NULL);
	ret = dev_pm_opp_of_add_table_indexed(dev,
		((sw_ver == 0x0001) && (opp_tbl_num > 1)) ? 1 : 0);
	if (ret)
		return 0;

	ret = dev_pm_opp_get_opp_count(dev);
	if (ret > 0) {
		vdisp_opp_num = ret;
		if (!g_vdisp_cal)
			g_vdisp_cal = kcalloc(vdisp_opp_num, sizeof(uint32_t), GFP_KERNEL);
		VDISPDBG("get vdisp_opp_num(%d)", vdisp_opp_num);
	}

	while (!IS_ERR(opp = dev_pm_opp_find_freq_ceil(dev, &freq))) {
		g_vdisp_max_freq = freq;
		freq++;
		dev_pm_opp_put(opp);
	}
	VDISPDBG("get g_vdisp_max_freq(%lu)", g_vdisp_max_freq);

	return 0;
}
