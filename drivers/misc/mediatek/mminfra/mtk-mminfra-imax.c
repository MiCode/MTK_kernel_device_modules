// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/random.h>
#include <linux/slab.h>
#if IS_ENABLED(CONFIG_MTK_SLBC)
#include <slbc_ops.h>
#endif
#include <iommu_debug.h>

#include "cmdq-util.h"
#include "mtk-interconnect.h"
#include "mtk-smi-dbg.h"

#define mminfra_read(addr)			readl_relaxed(addr)
#define mminfra_write(addr, val)		writel(val, addr)
#define mminfra_crit(fmt, ...)        pr_info("[MMInfra] " fmt, ##__VA_ARGS__)

enum {
	DISPSYS_BASE,
	DISPSYS1_BASE,
	MDPSYS_BASE,
	MDPSYS1_BASE,
	DISP_LARB_0,
	DISP_LARB_1,
	DISP_LARB_2,
	DISP_LARB_3,
	MDP_LARB_0,
	MDP_LARB_1,
};

enum {
	FAKE_ENG_0 = 0,
	FAKE_ENG_1,
	FAKE_ENG_2,
	FAKE_ENG_3,
	FAKE_ENG_NR,
};

enum {
	BW_LEVEL_0 = 0,
	BW_LEVEL_1,
	BW_LEVEL_2,
	BW_LEVEL_NR,
};

struct fake_eng_data {
	u32 larb_id;
	u32 port_id;
	struct icc_path *icc_path;
	// reg offset
	u32 rd_addr_ofs;
	u32 wr_addr_ofs;
	u32 con0_ofs;
	u32 con1_ofs;
	u32 reset_ofs;
	u32 enable_ofs;
	u32 state_ofs;
	// setting
	u32 wr_pat;
	u32 length;
	u32 burst;
	u32 dis_rd;
	u32 dis_wr;
	u32 latency;
	u32 loop;
};

struct bw_level_data {
	u32 latency;
	u32 bw;
};

static void __iomem *dispsys_base, *dispsys1_base;
static void __iomem *mdpsys_base, *mdpsys1_base;
static void __iomem *disp_larb_0_base, *disp_larb_1_base, *disp_larb_2_base, *disp_larb_3_base;
static void __iomem *mdp_larb_0_base, *mdp_larb_1_base, *mdp_larb_2_base;
static unsigned int mm_sram_base;
static unsigned int disp_larb_0_fake, disp_larb_1_fake, disp_larb_2_fake, disp_larb_3_fake;
static unsigned int mdp_larb_0_fake, mdp_larb_1_fake, mdp_larb_2_fake;

static struct device *dev;
static struct platform_device *g_pdev;
static bool is_init;

static u32 bw_fuzzer;
static u32 mminfra_bw_fuzzer_enable;
static u32 mm_fake_eng_enable;
static struct fake_eng_data fake_eng_info[FAKE_ENG_NR];
static struct bw_level_data bw_level_info[BW_LEVEL_NR];
static struct task_struct *fuzzer_task;
static struct task_struct *monitor_task;

static void init_mmsys(void)
{
	if (dispsys_base) {
		mminfra_write(dispsys_base + 0x108, 0xffffffff);
		mminfra_write(dispsys_base + 0x118, 0xffffffff);
		mminfra_write(dispsys_base + 0x1a8, 0xffffffff);
	}

	if (dispsys1_base) {
		mminfra_write(dispsys1_base + 0x108, 0xffffffff);
		mminfra_write(dispsys1_base + 0x118, 0xffffffff);
		mminfra_write(dispsys1_base + 0x1a8, 0xffffffff);
	}

	if (mdpsys_base) {
		mminfra_write(mdpsys_base + 0x108, 0xffffffff);
		mminfra_write(mdpsys_base + 0x118, 0xffffffff);
		mminfra_write(mdpsys_base + 0x128, 0xffffffff);
		mminfra_write(mdpsys_base + 0x138, 0xffffffff);
		mminfra_write(mdpsys_base + 0x148, 0xffffffff);
	}

	if (mdpsys1_base) {
		mminfra_write(mdpsys1_base + 0x108, 0xffffffff);
		mminfra_write(mdpsys1_base + 0x118, 0xffffffff);
		mminfra_write(mdpsys1_base + 0x128, 0xffffffff);
		mminfra_write(mdpsys1_base + 0x138, 0xffffffff);
		mminfra_write(mdpsys1_base + 0x148, 0xffffffff);
	}
}

static void init_mdp_smi(int id, void __iomem *mdp_larb_base, unsigned int fake_port)
{
	/* NON_SEC_CON */
	mminfra_write(mdp_larb_base + 0x380 + 0x4 * fake_port,
		mminfra_read(mdp_larb_base + 0x380 + 0x4 * fake_port) | 0x000f0000);

	mminfra_crit("%s %d NON_SEC_CON=0x%x\n", __func__, id,
		mminfra_read(mdp_larb_base + 0x380 + 0x4 * fake_port));
}

static void init_disp_smi(int id, void __iomem *disp_larb_base, unsigned int fake_port)
{
	/* NON_SEC_CON */
	mminfra_write(disp_larb_base + 0x380 + 0x4 * fake_port,
		mminfra_read(disp_larb_base + 0x380 + 0x4 * fake_port) & 0xfff0fffe);

	mminfra_crit("%s %d NON_SEC_CON=0x%x\n", __func__, id,
		mminfra_read(disp_larb_base + 0x380 + 0x4 * fake_port));
}

static void init_smi(unsigned int is_sram)
{
	if (is_sram) {
		if (mdp_larb_0_base)
			init_mdp_smi(0, mdp_larb_0_base, mdp_larb_0_fake);
		if (mdp_larb_1_base)
			init_mdp_smi(1, mdp_larb_1_base, mdp_larb_1_fake);
		if (mdp_larb_2_base)
			init_mdp_smi(1, mdp_larb_2_base, mdp_larb_2_fake);
	}

	if (disp_larb_0_base)
		init_disp_smi(0, disp_larb_0_base, disp_larb_0_fake);
	if (disp_larb_1_base)
		init_disp_smi(1, disp_larb_1_base, disp_larb_1_fake);
	if (disp_larb_2_base)
		init_disp_smi(2, disp_larb_2_base, disp_larb_2_fake);
	if (disp_larb_3_base)
		init_disp_smi(3, disp_larb_3_base, disp_larb_3_fake);
}

static void fake_eng_set(unsigned int id, void __iomem *subsys_base, unsigned int eng_id,
	unsigned int rd_addr, unsigned int wr_addr, unsigned int wr_pat, unsigned int length,
			unsigned int burst, unsigned int dis_rd, unsigned int dis_wr,
			unsigned int latency, unsigned int loop)
{
	unsigned int shift = 0;

	if (subsys_base) {
		if (eng_id == 1)
			shift = 0x20;

		/* SUBSYS_FAKE_ENG_RD_ADDR */
		mminfra_write(subsys_base + 0x210 + shift, rd_addr);
		/* SUBSYS_FAKE_ENG_WR_ADDR */
		mminfra_write(subsys_base + 0x214 + shift, wr_addr);
		/* SUBSYS_FAKE_ENG_CON0 */
		mminfra_write(subsys_base + 0x208 + shift,
			(wr_pat << 24) | (loop << 22) | length);
		/* SUBSYS_FAKE_ENG_CON1 */
		mminfra_write(subsys_base + 0x20c + shift,
			(burst << 12) | (dis_wr << 11) | (dis_rd << 10) | latency);
		mminfra_write(subsys_base + 0x204 + shift, 0x1); /* SUBSYS_FAKE_ENG_RST */
		mminfra_write(subsys_base + 0x204 + shift, 0x0); /* SUBSYS_FAKE_ENG_RST */
		mminfra_write(subsys_base + 0x200 + shift, 0x3); /* SUBSYS_FAKE_ENG_EN */

		mminfra_crit("%s id:%u eng:%u SUBSYS_FAKE_ENG_RD_ADDR=0x%x\n", __func__,
			id, eng_id, mminfra_read(subsys_base + 0x210 + shift));
		mminfra_crit("%s id:%u eng:%u SUBSYS_FAKE_ENG_WR_ADDR=0x%x\n", __func__,
			id, eng_id, mminfra_read(subsys_base + 0x214 + shift));
		mminfra_crit("%s id:%u eng:%u SUBSYS_FAKE_ENG_CON0=0x%x\n", __func__,
			id, eng_id, mminfra_read(subsys_base + 0x208 + shift));
		mminfra_crit("%s id:%u eng:%u SUBSYS_FAKE_ENG_CON1=0x%x\n", __func__,
			id, eng_id, mminfra_read(subsys_base + 0x20c + shift));
		mminfra_crit("%s id:%u eng:%u SUBSYS_FAKE_ENG_RST=0x%x\n", __func__,
			id, eng_id, mminfra_read(subsys_base + 0x204 + shift));
		mminfra_crit("%s id:%u eng:%u SUBSYS_FAKE_ENG_EN=0x%x\n", __func__,
			id, eng_id, mminfra_read(subsys_base + 0x200 + shift));

		/* SUBSYS_FAKE_ENG_STATE */
		if ((mminfra_read(subsys_base + 0x218) & 0x1) != 0)
			mminfra_crit("%s id:%u eng:%u FAKE_ENG_STATE is busy\n",
				__func__, id, eng_id);
		else
			mminfra_crit("%s id:%u eng:%u FAKE_ENG_STATE is idle\n",
				__func__, id, eng_id);
	}
}

static void fake_eng_set_v2(u32 id, void __iomem *subsys_base, u32 rd_addr,
		u32 wr_addr, u32 wr_pat, u32 length,
		u32 burst, u32 dis_rd, u32 dis_wr,
		u32 latency, u32 loop)
{
	if (subsys_base) {
		/* SUBSYS_FAKE_ENG_RD_ADDR */
		mminfra_write(subsys_base + fake_eng_info[id].rd_addr_ofs, rd_addr);
		/* SUBSYS_FAKE_ENG_WR_ADDR */
		mminfra_write(subsys_base + fake_eng_info[id].wr_addr_ofs, wr_addr);
		/* SUBSYS_FAKE_ENG_CON0 */
		mminfra_write(subsys_base + fake_eng_info[id].con0_ofs,
			(wr_pat << 24) | (loop << 22) | length);
		/* SUBSYS_FAKE_ENG_CON1 */
		mminfra_write(subsys_base + fake_eng_info[id].con1_ofs,
			(burst << 12) | (dis_wr << 11) | (dis_rd << 10) | latency);
	}
}

static int init_ctrl_base(struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dispsys");
	if (!res) {
		dev_notice(dev, "could not get resource for dispsys\n");
		return -EINVAL;
	}

	dispsys_base = ioremap(res->start, resource_size(res));
	if (IS_ERR(dispsys_base)) {
		dev_notice(dev, "could not ioremap resource for dispsys:%ld\n",
			PTR_ERR(dispsys_base));
		dispsys_base = NULL;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dispsys1");
	if (!res)
		dev_notice(dev, "could not get resource for dispsys1\n");

	if (res) {
		dispsys1_base = ioremap(res->start, resource_size(res));
		if (IS_ERR(dispsys1_base)) {
			dev_notice(dev, "could not ioremap resource for dispsys1\n");
			dispsys1_base = NULL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mdpsys");
	if (!res)
		dev_notice(dev, "could not get resource for mdpsys\n");

	if (res) {
		mdpsys_base = ioremap(res->start, resource_size(res));
		if (IS_ERR(mdpsys_base)) {
			dev_notice(dev, "could not ioremap resource for mdpsys\n");
			mdpsys_base = NULL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mdpsys1");
	if (!res)
		dev_notice(dev, "could not get resource for mdpsys1\n");

	if (res) {
		mdpsys1_base = ioremap(res->start, resource_size(res));
		if (IS_ERR(mdpsys1_base)) {
			dev_notice(dev, "could not ioremap resource for mdpsys1\n");
			mdpsys1_base = NULL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "disp_larb_0");
	if (!res)
		dev_notice(dev, "could not get resource for disp_larb_0\n");

	if (res) {
		disp_larb_0_base = ioremap(res->start, resource_size(res));
		if (IS_ERR(disp_larb_0_base)) {
			dev_notice(dev, "could not ioremap resource for disp_larb_0\n");
			disp_larb_0_base = NULL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "disp_larb_1");
	if (!res)
		dev_notice(dev, "could not get resource for disp_larb_1\n");

	if (res) {
		disp_larb_1_base = ioremap(res->start, resource_size(res));
		if (IS_ERR(disp_larb_1_base)) {
			dev_notice(dev, "could not ioremap resource for disp_larb_1\n");
			disp_larb_1_base = NULL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "disp_larb_2");
	if (!res)
		dev_notice(dev, "could not get resource for disp_larb_2\n");

	if (res) {
		disp_larb_2_base = ioremap(res->start, resource_size(res));
		if (IS_ERR(disp_larb_2_base)) {
			dev_notice(dev, "could not ioremap resource for disp_larb_2\n");
			disp_larb_2_base = NULL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "disp_larb_3");
	if (!res)
		dev_notice(dev, "could not get resource for disp_larb_3\n");

	if (res) {
		disp_larb_3_base = ioremap(res->start, resource_size(res));
		if (IS_ERR(disp_larb_3_base)) {
			dev_notice(dev, "could not ioremap resource for disp_larb_3\n");
			disp_larb_3_base = NULL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mdp_larb_0");
	if (!res)
		dev_notice(dev, "could not get resource for mdp_larb_0\n");

	if (res) {
		mdp_larb_0_base = ioremap(res->start, resource_size(res));
		if (IS_ERR(mdp_larb_0_base)) {
			dev_notice(dev, "could not ioremap resource for mdp_larb_0\n");
			mdp_larb_0_base = NULL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mdp_larb_1");
	if (!res)
		dev_notice(dev, "could not get resource for mdp_larb_1\n");

	if (res) {
		mdp_larb_1_base = ioremap(res->start, resource_size(res));
		if (IS_ERR(mdp_larb_1_base)) {
			dev_notice(dev, "could not ioremap resource for mdp_larb_1\n");
			mdp_larb_1_base = NULL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mdp_larb_2");
	if (!res)
		dev_notice(dev, "could not get resource for mdp_larb_2\n");

	if (res) {
		mdp_larb_2_base = ioremap(res->start, resource_size(res));
		if (IS_ERR(mdp_larb_2_base)) {
			dev_notice(dev, "could not ioremap resource for mdp_larb_2\n");
			mdp_larb_2_base = NULL;
		}
	}

	of_property_read_u32(dev->of_node, "disp-larb0-fake-port", &disp_larb_0_fake);
	of_property_read_u32(dev->of_node, "disp-larb1-fake-port", &disp_larb_1_fake);
	of_property_read_u32(dev->of_node, "disp-larb2-fake-port", &disp_larb_2_fake);
	of_property_read_u32(dev->of_node, "disp-larb3-fake-port", &disp_larb_3_fake);

	of_property_read_u32(dev->of_node, "mdp-larb0-fake-port", &mdp_larb_0_fake);
	of_property_read_u32(dev->of_node, "mdp-larb1-fake-port", &mdp_larb_1_fake);
	of_property_read_u32(dev->of_node, "mdp-larb2-fake-port", &mdp_larb_2_fake);

	of_property_read_u32(dev->of_node, "mm-sram-base", &mm_sram_base);
	return 0;
}

static int do_mminfra_imax(const char *val, const struct kernel_param *kp)
{
	int ret = 0;
	unsigned int latency, is_sram;
	void *dram_base;
	dma_addr_t dram_phy_base;
#if IS_ENABLED(CONFIG_MTK_SLBC)
	struct slbc_data sram_data;
#endif
	u64 dma_mask;

	ret = sscanf(val, "%u %u", &latency, &is_sram);
	if (ret != 2) {
		pr_notice("%s: invalid input: %s, result(%d)\n", __func__, val, ret);
		return -EINVAL;
	}

	if (!is_init) {
		init_ctrl_base(g_pdev);
		is_init = true;
	}
	init_mmsys();
	init_smi(is_sram);
	cmdq_util_mminfra_cmd(2);

	if (is_sram) {
#if IS_ENABLED(CONFIG_MTK_SLBC)
		sram_data.uid = UID_MML;
		sram_data.type = TP_BUFFER;
		//sram_data.flag = FG_POWER;
		ret = slbc_request(&sram_data);

		if (ret >= 0)
			mm_sram_base = (unsigned long)sram_data.paddr;
#endif

		if (mm_sram_base) {
			fake_eng_set(MDPSYS_BASE, mdpsys_base, 0, mm_sram_base, mm_sram_base,
				4, 255, 7, 0, 0, latency, 1);
			fake_eng_set(MDPSYS1_BASE, mdpsys1_base, 0, mm_sram_base, mm_sram_base,
				4, 255, 7, 0, 0, latency, 1);
		}

	}

	dma_mask = dma_get_mask(mtk_smmu_get_shared_device(dev));
	ret = dma_set_mask_and_coherent(mtk_smmu_get_shared_device(dev), DMA_BIT_MASK(32));
	if (ret)
		mminfra_crit("%s: fail to do dma mask %d", __func__, ret);
	dram_base = dma_alloc_coherent(mtk_smmu_get_shared_device(dev),
		1024*1024, &dram_phy_base, GFP_KERNEL);
	ret = dma_set_mask_and_coherent(mtk_smmu_get_shared_device(dev), dma_mask);
	if (ret)
		mminfra_crit("%s: fail to do dma mask %d", __func__, ret);
	if (!dram_base) {
		mminfra_crit("%s: allocate dram memory failed\n", __func__);
		return -ENOMEM;
	}
	mminfra_crit("%s dram addr = %pa\n", __func__, &dram_phy_base);

	fake_eng_set(DISPSYS_BASE, dispsys_base, 0, dram_phy_base, dram_phy_base,
		4, 255, 7, 0, 0, latency, 1);
	fake_eng_set(DISPSYS_BASE, dispsys_base, 1, dram_phy_base, dram_phy_base,
		4, 255, 7, 0, 0, latency, 1);
	fake_eng_set(DISPSYS1_BASE, dispsys1_base, 0, dram_phy_base, dram_phy_base,
		4, 255, 7, 0, 0, latency, 1);
	fake_eng_set(DISPSYS1_BASE, dispsys1_base, 1, dram_phy_base, dram_phy_base,
		4, 255, 7, 0, 0, latency, 1);

	return ret;
}

static struct kernel_param_ops mminfra_imax_ops = {
	.set = do_mminfra_imax,
};
module_param_cb(mminfra_imax, &mminfra_imax_ops, NULL, 0644);
MODULE_PARM_DESC(mminfra_imax, "mminfra imax");

static void fake_eng_enable(bool enable, unsigned int id, void __iomem *subsys_base)
{
	if (subsys_base) {
		/* SUBSYS_FAKE_ENG_RST */
		mminfra_write(subsys_base + fake_eng_info[id].reset_ofs, 0x1);
		/* SUBSYS_FAKE_ENG_RST */
		mminfra_write(subsys_base + fake_eng_info[id].reset_ofs, 0x0);
		/* SUBSYS_FAKE_ENG_EN */
		if (enable)
			mminfra_write(subsys_base + fake_eng_info[id].enable_ofs, 0x3);
		else
			mminfra_write(subsys_base + fake_eng_info[id].enable_ofs, 0x0);

		/* SUBSYS_FAKE_ENG_STATE */
		if ((mminfra_read(subsys_base + fake_eng_info[id].state_ofs) & 0x1) != 0)
			mminfra_crit("%s id:%u FAKE_ENG_STATE is busy\n", __func__, id);
		else
			mminfra_crit("%s id:%u FAKE_ENG_STATE is idle\n", __func__, id);
	}
}

int fake_eng_fuzzer(bool enable, u32 larb)
{
	int ret = 0;
	void *dram_base;
	static dma_addr_t dram_phy_base;
	u64 dma_mask;

	if (!is_init)
		init_ctrl_base(g_pdev);

	init_smi(0);
	cmdq_util_mminfra_cmd(2);

	if (!is_init) {
		dma_mask = dma_get_mask(mtk_smmu_get_shared_device(dev));
		ret = dma_set_mask_and_coherent(mtk_smmu_get_shared_device(dev), DMA_BIT_MASK(32));
		if (ret)
			mminfra_crit("%s: fail to do dma mask %d", __func__, ret);
		dram_base = dma_alloc_coherent(mtk_smmu_get_shared_device(dev),
			1024*1024, &dram_phy_base, GFP_KERNEL);
		ret = dma_set_mask_and_coherent(mtk_smmu_get_shared_device(dev), dma_mask);
		if (ret)
			mminfra_crit("%s: fail to do dma mask %d", __func__, ret);
		if (!dram_base) {
			mminfra_crit("%s: allocate dram memory failed\n", __func__);
			return -ENOMEM;
		}
		mminfra_crit("%s dram addr = %pa\n", __func__, &dram_phy_base);
		is_init = true;
	}

	if (enable) {
		if (larb == fake_eng_info[FAKE_ENG_0].larb_id)
			fake_eng_set_v2(FAKE_ENG_0, dispsys_base, dram_phy_base, dram_phy_base,
				fake_eng_info[FAKE_ENG_0].wr_pat, fake_eng_info[FAKE_ENG_0].length,
				fake_eng_info[FAKE_ENG_0].burst, fake_eng_info[FAKE_ENG_0].dis_rd,
				fake_eng_info[FAKE_ENG_0].dis_wr, fake_eng_info[FAKE_ENG_0].latency,
				fake_eng_info[FAKE_ENG_0].loop);
		else if (larb == fake_eng_info[FAKE_ENG_1].larb_id)
			fake_eng_set_v2(FAKE_ENG_1, dispsys_base, dram_phy_base, dram_phy_base,
				fake_eng_info[FAKE_ENG_1].wr_pat, fake_eng_info[FAKE_ENG_1].length,
				fake_eng_info[FAKE_ENG_1].burst, fake_eng_info[FAKE_ENG_1].dis_rd,
				fake_eng_info[FAKE_ENG_1].dis_wr, fake_eng_info[FAKE_ENG_1].latency,
				fake_eng_info[FAKE_ENG_1].loop);
		else if (larb == fake_eng_info[FAKE_ENG_2].larb_id)
			fake_eng_set_v2(FAKE_ENG_2, dispsys1_base, dram_phy_base, dram_phy_base,
				fake_eng_info[FAKE_ENG_2].wr_pat, fake_eng_info[FAKE_ENG_2].length,
				fake_eng_info[FAKE_ENG_2].burst, fake_eng_info[FAKE_ENG_2].dis_rd,
				fake_eng_info[FAKE_ENG_2].dis_wr, fake_eng_info[FAKE_ENG_2].latency,
				fake_eng_info[FAKE_ENG_2].loop);
		else if (larb == fake_eng_info[FAKE_ENG_3].larb_id)
			fake_eng_set_v2(FAKE_ENG_3, dispsys1_base, dram_phy_base, dram_phy_base,
				fake_eng_info[FAKE_ENG_3].wr_pat, fake_eng_info[FAKE_ENG_3].length,
				fake_eng_info[FAKE_ENG_3].burst, fake_eng_info[FAKE_ENG_3].dis_rd,
				fake_eng_info[FAKE_ENG_3].dis_wr, fake_eng_info[FAKE_ENG_3].latency,
				fake_eng_info[FAKE_ENG_3].loop);
	}

	if (larb == fake_eng_info[FAKE_ENG_0].larb_id)
		fake_eng_enable(enable, FAKE_ENG_0, dispsys_base);
	else if (larb == fake_eng_info[FAKE_ENG_1].larb_id)
		fake_eng_enable(enable, FAKE_ENG_1, dispsys_base);
	else if (larb == fake_eng_info[FAKE_ENG_2].larb_id)
		fake_eng_enable(enable, FAKE_ENG_2, dispsys1_base);
	else if (larb == fake_eng_info[FAKE_ENG_3].larb_id)
		fake_eng_enable(enable, FAKE_ENG_3, dispsys1_base);

	return ret;
}

static u32 latency_to_report_bw(u32 latency)
{
	int i;
	u32 report_bw = 0;

	for (i = 0; i < BW_LEVEL_NR; i++) {
		if (latency == bw_level_info[i].latency) {
			report_bw = bw_level_info[i].bw;
			break;
		}
		if (i == (BW_LEVEL_NR-1))
			pr_notice("%s: latency %u is not in bw_level_info\n", __func__, latency);
	}

	return report_bw;
}

int mminfra_fake_eng_enable(const char *val, const struct kernel_param *kp)
{
	int ret = 0, i;

	ret = kstrtou32(val, 0, &mm_fake_eng_enable);
	if (ret) {
		pr_notice("%s: invalid input: %s, result(%d)\n", __func__, val, ret);
		return -EINVAL;
	}

	if (mm_fake_eng_enable) {
		for (i = 0; i < FAKE_ENG_NR; i++)
			mtk_icc_set_bw(fake_eng_info[i].icc_path,
				MBps_to_icc(latency_to_report_bw(fake_eng_info[i].latency)), 0);
		for (i = 0; i < FAKE_ENG_NR; i++)
			fake_eng_fuzzer(true, fake_eng_info[i].larb_id);
	} else {
		for (i = 0; i < FAKE_ENG_NR; i++)
			fake_eng_fuzzer(false, fake_eng_info[i].larb_id);
		for (i = 0; i < FAKE_ENG_NR; i++)
			mtk_icc_set_bw(fake_eng_info[i].icc_path, 0, 0);
	}

	return 0;
}

int mminfra_fake_eng_enable_status(char *buf, const struct kernel_param *kp)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%s:%d", __func__, mm_fake_eng_enable ? 1 : 0);

	return len;

}

static const struct kernel_param_ops mminfra_fake_eng_enable_ops = {
	.set = mminfra_fake_eng_enable,
	.get = mminfra_fake_eng_enable_status,
};
module_param_cb(mminfra_fake_eng_enable, &mminfra_fake_eng_enable_ops, NULL, 0644);
MODULE_PARM_DESC(mminfra_fake_eng_enable, "mminfra fake eng enable");

static int mminfra_bw_fuzzer_test(void *data)
{
	int i;
	u32 max_time_inteval = 5;  //ms

	while (!kthread_should_stop()) {
		for (i = 0; i < FAKE_ENG_NR; i++)
			fake_eng_fuzzer(true, fake_eng_info[i].larb_id);

		mdelay(max_time_inteval);

		for (i = 0; i < FAKE_ENG_NR; i++)
			fake_eng_fuzzer(false, fake_eng_info[i].larb_id);

		mdelay(max_time_inteval);
	}

	return 0;
}

static int mminfra_bw_monitor_test(void *data)
{
	int i;
	u32 report_bw[FAKE_ENG_NR] = {0};
	u32 temp_bw[4] = {0}, temp_port[4] = {0};
	ktime_t start_time = 0, end_time = 0;

	while (!kthread_should_stop()) {
		end_time = ktime_get();
		for (i = 0; i < FAKE_ENG_NR; i++) {
			smi_larb_monitor_stop(fake_eng_info[i].larb_id, &temp_bw[0]);
			report_bw[i] = (temp_bw[0] * 1000) / (1024*1024) / (ktime_ms_delta(end_time, start_time));
		}

		for (i = 0; i < FAKE_ENG_NR; i++)
			mtk_icc_set_bw(fake_eng_info[i].icc_path, MBps_to_icc(report_bw[i]), 0);

		for (i = 0; i < FAKE_ENG_NR; i++) {
			temp_port[0] = fake_eng_info[i].port_id;
			temp_port[1] = fake_eng_info[i].port_id;
			temp_port[2] = fake_eng_info[i].port_id;
			temp_port[3] = fake_eng_info[i].port_id;
			smi_larb_monitor_start(fake_eng_info[i].larb_id, &temp_port[0], SMI_MON_ALL);
		}
		start_time = ktime_get();
		mdelay(1);
	}

	for (i = 0; i < FAKE_ENG_NR; i++)
		mtk_icc_set_bw(fake_eng_info[i].icc_path, 0, 0);

	return 0;
}

int mminfra_bw_fuzzer(const char *val, const struct kernel_param *kp)
{
	int ret = 0;

	ret = kstrtou32(val, 0, &mminfra_bw_fuzzer_enable);
	if (ret) {
		pr_notice("%s: invalid input: %s, result(%d)\n", __func__, val, ret);
		return -EINVAL;
	}

	if (!fuzzer_task) {
		fuzzer_task = kthread_run(mminfra_bw_fuzzer_test, NULL, "mminfra_bw_fuzzer_test");
	} else {
		kthread_stop(fuzzer_task);
		fuzzer_task = NULL;
	}

	if (!monitor_task) {
		monitor_task = kthread_run(mminfra_bw_monitor_test, NULL, "mminfra_bw_fuzzer_test");
	} else {
		kthread_stop(monitor_task);
		monitor_task = NULL;
	}

	return 0;
}

int mminfra_bw_fuzzer_status(char *buf, const struct kernel_param *kp)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%s:%d", __func__, fuzzer_task ? 1 : 0);

	return len;
}

static const struct kernel_param_ops mminfra_bw_fuzzer_ops = {
	.set = mminfra_bw_fuzzer,
	.get = mminfra_bw_fuzzer_status,
};
module_param_cb(mminfra_bw_fuzzer, &mminfra_bw_fuzzer_ops, NULL, 0644);
MODULE_PARM_DESC(mminfra_bw_fuzzer, "mminfra bw_fuzzer");

int mminfra_bw_fuzzer_para(const char *val, const struct kernel_param *kp)
{
	int ret = 0, i;
	u32 latency = 0, report_bw;

	ret = kstrtou32(val, 0, &latency);
	if (ret) {
		pr_notice("%s: invalid input: %s, result(%d)\n", __func__, val, ret);
		return -EINVAL;
	}
	mminfra_crit("%s: input latency: %d\n", __func__, latency);

	fake_eng_info[FAKE_ENG_0].latency = latency;
	fake_eng_info[FAKE_ENG_1].latency = latency;
	fake_eng_info[FAKE_ENG_2].latency = latency;
	fake_eng_info[FAKE_ENG_3].latency = latency;

	for (i = 0; i < FAKE_ENG_NR; i++) {
		report_bw = latency_to_report_bw(fake_eng_info[i].latency);
		mminfra_crit("%s: report_bw for fake_eng_info[%d]: %d\n", __func__, i, report_bw);
		mtk_icc_set_bw(fake_eng_info[i].icc_path, MBps_to_icc(report_bw), 0);
	}

	return 0;
}

int mminfra_bw_fuzzer_para_status(char *buf, const struct kernel_param *kp)
{
	int len = 0, i;

	for (i = 0; i < FAKE_ENG_NR; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len,
			"%d: larb(%d) wr_pat(%d) length(%d) burst(%d) dis_rd(%d) dis_wr(%d) latency(%d) loop(%d)\n",
			i, fake_eng_info[i].larb_id, fake_eng_info[i].wr_pat,
			fake_eng_info[i].length, fake_eng_info[i].burst, fake_eng_info[i].dis_rd,
			fake_eng_info[i].dis_wr, fake_eng_info[i].latency, fake_eng_info[i].loop);
	}

	return len;
}

static const struct kernel_param_ops mminfra_bw_fuzzer_para_ops = {
	.set = mminfra_bw_fuzzer_para,
	.get = mminfra_bw_fuzzer_para_status,
};
module_param_cb(mminfra_bw_fuzzer_para, &mminfra_bw_fuzzer_para_ops, NULL, 0644);
MODULE_PARM_DESC(mminfra_bw_fuzzer_para, "mminfra bw_fuzzer para");

static int mminfra_imax_probe(struct platform_device *pdev)
{
	int i;
	u32 tmp = 0;

	g_pdev = pdev;
	dev = &pdev->dev;

	of_property_read_u32(dev->of_node, "disp_larb0_fake_port", &disp_larb_0_fake);
	of_property_read_u32(dev->of_node, "disp_larb1_fake_port", &disp_larb_1_fake);
	of_property_read_u32(dev->of_node, "disp_larb2_fake_port", &disp_larb_2_fake);
	of_property_read_u32(dev->of_node, "disp_larb3_fake_port", &disp_larb_3_fake);

	of_property_read_u32(dev->of_node, "mdp_larb0_fake_port", &mdp_larb_0_fake);
	of_property_read_u32(dev->of_node, "mdp_larb1_fake_port", &mdp_larb_1_fake);
	of_property_read_u32(dev->of_node, "mdp_larb2_fake_port", &mdp_larb_2_fake);

	of_property_read_u32(dev->of_node, "bw-fuzzer", &bw_fuzzer);
	pr_notice("[mminfra] bw_fuzzer=%d\n", bw_fuzzer);
	if (bw_fuzzer == 1) {
		for (i = 0; i < FAKE_ENG_NR; i++) {
			if (!of_property_read_u32_index(dev->of_node, "larb-id", i, &tmp))
				fake_eng_info[i].larb_id = tmp;
			if (!of_property_read_u32_index(dev->of_node, "port-id", i, &tmp))
				fake_eng_info[i].port_id = tmp;
			// reg offset
			if (!of_property_read_u32_index(dev->of_node, "rd-addr-ofs", i, &tmp))
				fake_eng_info[i].rd_addr_ofs = tmp;
			if (!of_property_read_u32_index(dev->of_node, "wr-addr-ofs", i, &tmp))
				fake_eng_info[i].wr_addr_ofs = tmp;
			if (!of_property_read_u32_index(dev->of_node, "con0-ofs", i, &tmp))
				fake_eng_info[i].con0_ofs = tmp;
			if (!of_property_read_u32_index(dev->of_node, "con1-ofs", i, &tmp))
				fake_eng_info[i].con1_ofs = tmp;
			if (!of_property_read_u32_index(dev->of_node, "reset-ofs", i, &tmp))
				fake_eng_info[i].reset_ofs = tmp;
			if (!of_property_read_u32_index(dev->of_node, "enable-ofs", i, &tmp))
				fake_eng_info[i].enable_ofs = tmp;
			if (!of_property_read_u32_index(dev->of_node, "state-ofs", i, &tmp))
				fake_eng_info[i].state_ofs = tmp;
			// setting
			if (!of_property_read_u32_index(dev->of_node, "wr-pat", i, &tmp))
				fake_eng_info[i].wr_pat = tmp;
			if (!of_property_read_u32_index(dev->of_node, "length", i, &tmp))
				fake_eng_info[i].length = tmp;
			if (!of_property_read_u32_index(dev->of_node, "burst", i, &tmp))
				fake_eng_info[i].burst = tmp;
			if (!of_property_read_u32_index(dev->of_node, "dis-rd", i, &tmp))
				fake_eng_info[i].dis_rd = tmp;
			if (!of_property_read_u32_index(dev->of_node, "dis-wr", i, &tmp))
				fake_eng_info[i].dis_wr = tmp;
			if (!of_property_read_u32_index(dev->of_node, "latency", i, &tmp))
				fake_eng_info[i].latency = tmp;
			if (!of_property_read_u32_index(dev->of_node, "loop", i, &tmp))
				fake_eng_info[i].loop = tmp;
		}
		fake_eng_info[FAKE_ENG_0].icc_path = of_mtk_icc_get(dev, "fake_eng_0");
		if (IS_ERR_OR_NULL(fake_eng_info[FAKE_ENG_0].icc_path))
			pr_info("get icc path failed:%s\n", "fake_eng_0");
		fake_eng_info[FAKE_ENG_1].icc_path = of_mtk_icc_get(dev, "fake_eng_1");
		if (IS_ERR_OR_NULL(fake_eng_info[FAKE_ENG_1].icc_path))
			pr_info("get icc path failed:%s\n", "fake_eng_1");
		fake_eng_info[FAKE_ENG_2].icc_path = of_mtk_icc_get(dev, "fake_eng_2");
		if (IS_ERR_OR_NULL(fake_eng_info[FAKE_ENG_2].icc_path))
			pr_info("get icc path failed:%s\n", "fake_eng_2");
		fake_eng_info[FAKE_ENG_3].icc_path = of_mtk_icc_get(dev, "fake_eng_3");
		if (IS_ERR_OR_NULL(fake_eng_info[FAKE_ENG_3].icc_path))
			pr_info("get icc path failed:%s\n", "fake_eng_3");

		for (i = 0; i < BW_LEVEL_NR; i++) {
			if (!of_property_read_u32_index(dev->of_node, "bw-table-latency", i, &tmp))
				bw_level_info[i].latency = tmp;
			if (!of_property_read_u32_index(dev->of_node, "bw-table-bw", i, &tmp))
				bw_level_info[i].bw = tmp;
		}
	}

	return 0;
}

static const struct of_device_id of_mminfra_imax_match_tbl[] = {
	{
		.compatible = "mediatek,mminfra-imax",
	},
	{}
};

static struct platform_driver mminfra_imax_drv = {
	.probe = mminfra_imax_probe,
	.driver = {
		.name = "mtk-mminfra-imax",
		.of_match_table = of_mminfra_imax_match_tbl,
	},
};

static int __init mtk_mminfra_imax_init(void)
{
	s32 status;

	status = platform_driver_register(&mminfra_imax_drv);
	if (status) {
		pr_notice("Failed to register MMInfra imax driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_mminfra_imax_exit(void)
{
	platform_driver_unregister(&mminfra_imax_drv);
}

module_init(mtk_mminfra_imax_init);
module_exit(mtk_mminfra_imax_exit);
MODULE_DESCRIPTION("MTK MMInfra IMAX driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL v2");
