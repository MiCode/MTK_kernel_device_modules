// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/ktime.h>
#include <soc/mediatek/mmdvfs_v3.h>

#include "mtk-mmdvfs-v3-memory.h"
#include "vcp_status.h"
#include "mtk_vdisp.h"
#include "mtk_vdisp_avs.h"

#define VDISPDBG(fmt, args...) \
	pr_info("[vdisp] %s:%d " fmt "\n", __func__, __LINE__, ##args)

#define VDISPERR(fmt, args...) \
	pr_info("[vdisp][err] %s:%d " fmt "\n", __func__, __LINE__, ##args)

static void __iomem *g_aging_base;
static struct clk *g_mmdvfs_clk;
const struct mtk_vdisp_avs_data *g_vdisp_avs_data;

#define IPI_TIMEOUT_MS	(200U)
int mtk_vdisp_avs_ipi_send(u32 func_id)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_ARM64)
	ret = mtk_ipi_send(vcp_get_ipidev(VDISP_FEATURE_ID),
		IPI_OUT_VDISP, IPI_SEND_WAIT,
		&func_id, PIN_OUT_SIZE_VDISP, IPI_TIMEOUT_MS);
	if (ret != IPI_ACTION_DONE)
		VDISPDBG("ipi fail");
#endif
	return ret;
}

void query_curr_ro(const struct mtk_vdisp_avs_data *vdisp_avs_data)
{
	u32 ro_fresh_curr = 0, ro_aging_curr = 0, ro_curr = 0, i = 0;

	/* read vdisp avs RO fresh current */
	// 1. Enable Power on
	writel(0x0000001D, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	writel(0x0000001F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	writel(0x0000005F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	writel(0x0000004F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	writel(0x0000006F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	// 2. let rst_b =1’b0 (normal mode0 to  rst_b = 1’b1
	writel(0x00000000, g_aging_base + vdisp_avs_data->aging_reg_test);
	writel(0x00000100, g_aging_base + vdisp_avs_data->aging_reg_test);
	// 3. enable RO
	writel(vdisp_avs_data->aging_reg_ro_en0_fresh_val,
		g_aging_base + vdisp_avs_data->aging_reg_ro_en0);
	writel(0x00000000, g_aging_base + vdisp_avs_data->aging_reg_ro_en1);
	writel(vdisp_avs_data->aging_reg_ro_en2_fresh_val,
		g_aging_base + vdisp_avs_data->aging_reg_ro_en2);
	// 4. RO select
	writel(vdisp_avs_data->aging_ro_sel_0_fresh_val,
		g_aging_base + vdisp_avs_data->aging_ro_sel_0);
	writel(vdisp_avs_data->aging_ro_sel_1_fresh_val,
		g_aging_base + vdisp_avs_data->aging_ro_sel_1);
	// 5. Aptv_timer + cnt restart & clr
	writel(0x00022A00, g_aging_base + vdisp_avs_data->aging_win_cyc);
	udelay(20);
	writel(0x00002A00, g_aging_base + vdisp_avs_data->aging_win_cyc);
	// 6. Aptv_timer + cnt start & clr
	writel(0x00012A00, g_aging_base + vdisp_avs_data->aging_win_cyc);
	// 7. Count over
	udelay(460);
	// Get RO fresh current Value
	ro_fresh_curr = readl(g_aging_base + vdisp_avs_data->aging_ro_fresh) & 0xFFFF;
	// VDISPDBG("VDISP_AVS_RO_FRESH_CURR=%x", ro_fresh_curr);
	// 8. Power off
	writel(0x0000006F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	writel(0x0000007F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	writel(0x0000003F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	writel(0x0000001F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	writel(0x0000001E, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	writel(0x0000001C, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);

	/* read vdisp avs RO aging current */
	// // 1. Enable Power on
	// writel(0x0000001D, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	// writel(0x0000001F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	// writel(0x0000005F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	// writel(0x0000004F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	// writel(0x0000006F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	// 2. let rst_b =1’b0 (normal mode0 to  rst_b = 1’b1
	writel(0x00000000, g_aging_base + vdisp_avs_data->aging_reg_test);
	writel(0x00000100, g_aging_base + vdisp_avs_data->aging_reg_test);
	// 3. enable RO
	writel(vdisp_avs_data->aging_reg_ro_en0_aging_val,
		g_aging_base + vdisp_avs_data->aging_reg_ro_en0);
	writel(0x00000000, g_aging_base + vdisp_avs_data->aging_reg_ro_en1);
	writel(vdisp_avs_data->aging_reg_ro_en2_aging_val,
		g_aging_base + vdisp_avs_data->aging_reg_ro_en2);
	// 4. RO select
	writel(vdisp_avs_data->aging_ro_sel_0_aging_val,
		g_aging_base + vdisp_avs_data->aging_ro_sel_0);
	writel(vdisp_avs_data->aging_ro_sel_1_aging_val,
		g_aging_base + vdisp_avs_data->aging_ro_sel_1);
	// 5. Aptv_timer + cnt restart & clr
	writel(0x00022A00, g_aging_base + vdisp_avs_data->aging_win_cyc);
	udelay(20);
	writel(0x00002A00, g_aging_base + vdisp_avs_data->aging_win_cyc);
	// 6. Aptv_timer + cnt start & clr
	writel(0x00012A00, g_aging_base + vdisp_avs_data->aging_win_cyc);
	// 7. Count over
	udelay(460);
	// Get RO aging current Value
	ro_aging_curr = readl(g_aging_base + vdisp_avs_data->aging_ro_aging) & 0xFFFF;
	// VDISPDBG("VDISP_AVS_RO_AGING_CURR=%x", ro_aging_curr);

	/* update to share memory and send ipi to update */
	ro_curr = (ro_fresh_curr << 16) | ro_aging_curr;
	writel(ro_curr, MEM_VDISP_AVS_RO_CURR);
	if (mtk_vdisp_avs_ipi_send(FUNC_IPI_AGING_UPDATE) != IPI_ACTION_DONE)
		return;

	/* wait for mmup updating ro_curr */
	while (readl(MEM_VDISP_AVS_RO_CURR) != 0) {
		// busy waiting
		udelay(1);
		i++;
		if (i >= 1000) {
			VDISPDBG("ack timeout");
			break;
		}
	}
}

void mtk_vdisp_avs_query_aging_val(struct device *dev)
{
	u32 i = 0;
	static ktime_t last_time;
	ktime_t cur_time = ktime_get();
	ktime_t elapse_time = cur_time - last_time;
	ktime_t t_ag;

	if (!MEM_BASE)
		return;

	t_ag = (VDISP_SHRMEM_BITWISE_VAL & BIT(VDISP_AVS_AGING_FAST_EN_BIT))
		? 10*1e9 : 24*60*60*1e9; // 1day: 24*60*60*1e9

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

	if (!g_vdisp_avs_data) {
		VDISPDBG("vdisp_avs_data uninitialized, skip");
		return;
	}

	if (VDISP_SHRMEM_BITWISE_VAL &
		(BIT(VDISP_AVS_AGING_DISABLE_BIT) | BIT(VDISP_AVS_DISABLE_BIT))) {
		VDISPDBG("aging disabled, skip");
		return;
	}

	/* trigger VDISP OPP4 750mV */
	if (mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_DISP)) {
		VDISPDBG("request mmdvfs disp fail");
		return;
	}
	writel(VDISP_SHRMEM_BITWISE_VAL | BIT(VDISP_AVS_AGING_ACK_BIT),
		VDISP_SHRMEM_BITWISE);
	clk_set_rate(g_mmdvfs_clk, 728000000);

	/* wait for VDISP_AVS_AGE_ACK to be set */
	while ((VDISP_SHRMEM_BITWISE_VAL & BIT(VDISP_AVS_AGING_ACK_BIT)) != 0) {
		// busy waiting
		udelay(1);
		i++;
		if (i >= 1000) {
			VDISPDBG("ack timeout");
			writel(VDISP_SHRMEM_BITWISE_VAL & ~BIT(VDISP_AVS_AGING_ACK_BIT),
				VDISP_SHRMEM_BITWISE);
			goto release_mmdvfs_clk;
		}
	}

	/* query current aging sensor value */
	query_curr_ro(g_vdisp_avs_data);

	/* release VDISP opp4 */
release_mmdvfs_clk:
	clk_set_rate(g_mmdvfs_clk, 0);
	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_DISP);
}

int mtk_vdisp_avs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct clk *clk;
	const struct mtk_vdisp_data *vdisp_data = of_device_get_match_data(dev);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "aging_base");
	if (res) {
		g_aging_base = devm_ioremap(dev, res->start, resource_size(res));
		if (!g_aging_base) {
			VDISPERR("fail to ioremap aging_base: 0x%pa", &res->start);
			return -EINVAL;
		}
	}

	clk = devm_clk_get(dev, "mmdvfs_clk");
	if (!IS_ERR(clk))
		g_mmdvfs_clk = clk;

	g_vdisp_avs_data = vdisp_data->avs;

	return 0;
}
