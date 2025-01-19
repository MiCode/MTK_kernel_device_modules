// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/ctype.h>
#include <linux/edac.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/types.h>
#include <mt-plat/aee.h>
#include <soc/mediatek/emi.h>
#include "edac_module.h"

enum error_type {
	NO_ERROR = 0,
	CORRECTABLE_ERROR,
	UNCORRECTABLE_ERROR
};

struct slc_drvdata {
	void __iomem **base;
	int sb_irq;
	int db_irq;
	unsigned int slc_parity_cnt;
	unsigned int parity_err_offset;
	unsigned int parity_err_ext_offset;
	unsigned int parity_err_addr_offset;
	unsigned int parity_err_status_offset;
	unsigned int port_num;
	unsigned int cs_num;
	unsigned int chn_num;
	unsigned int assert;
};
static enum error_type read_parity_status(struct edac_device_ctl_info *dci, unsigned int emi_id)
{
	struct arm_smccc_res smc_res;
	struct slc_drvdata *drvdata = dci->pvt_info;
	unsigned int content;
	int port_idx, chn_idx, cs_idx;
	int ecc_idx, ecc_count;
	enum error_type partial_error_type = NO_ERROR;
	enum error_type total_error_type = NO_ERROR;
	char ecc_position[40] = {0};

	for (port_idx = 0; port_idx < drvdata->port_num; port_idx++) {
		arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_SLC_PARITY_SELECT,
				emi_id, port_idx, 0, 0, 0, 0, &smc_res);
		if (smc_res.a0) {
			pr_info("%s:%d failed to clear slc parity, ret=0x%lx\n",
				__func__, __LINE__, smc_res.a0);
		}
		for (chn_idx = 0; chn_idx < drvdata->chn_num; chn_idx++) {
			for (cs_idx = 0; cs_idx < drvdata->cs_num; cs_idx++) {
				content = readl(drvdata->base[emi_id] + drvdata->parity_err_status_offset + ((chn_idx * drvdata->cs_num + cs_idx) << 2));
				ecc_count = 0;
				for (ecc_idx = 0; ecc_idx < 4; ecc_idx++) {
					if (content >> ecc_idx)
						++ecc_count;
				}

				snprintf(ecc_position, sizeof(ecc_position), "emi: %d, port: %d, chn: %d, cs: %d\n", emi_id, port_idx, chn_idx, cs_idx);

				if (ecc_count == 1) {
					partial_error_type = CORRECTABLE_ERROR;
					edac_device_handle_ce_count(dci, ecc_count, 0, 0, ecc_position);
				} else if (ecc_count > 1) {
					partial_error_type = UNCORRECTABLE_ERROR;
					edac_device_handle_ue_count(dci, ecc_count, 0, 0, ecc_position);
				}

				if (total_error_type == NO_ERROR) {
					if (partial_error_type != NO_ERROR)
						total_error_type = partial_error_type;
				} else if (total_error_type == CORRECTABLE_ERROR) {
					if (partial_error_type == UNCORRECTABLE_ERROR)
						total_error_type = partial_error_type;
				}
			}
		}
	}

	return total_error_type;
}

static void slc_clear_violation(unsigned int emi_id)
{
	struct arm_smccc_res smc_res;

	arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_SLC_PARITY_CLEAR,
			emi_id, 0, 0, 0, 0, 0, &smc_res);
	if (smc_res.a0) {
		pr_info("%s:%d failed to clear slc parity, ret=0x%lx\n",
			__func__, __LINE__, smc_res.a0);
	}
}

static irqreturn_t slc_err_handler(int irq, void *dev_id)
{
	struct edac_device_ctl_info *dci = dev_id;
	struct slc_drvdata *drvdata = dci->pvt_info;
	enum error_type partial_error_type = NO_ERROR;
	enum error_type total_error_type = NO_ERROR;
	char ecc_mesg[40] = {0};
	int ecc_mesg_idx = 0;
	unsigned long long parity_err_tol;
	unsigned int parity_err_value;
	unsigned int parity_err_ext_value;
	unsigned int emi_id;

	for (emi_id = 0; emi_id < drvdata->slc_parity_cnt; emi_id++) {
		parity_err_value = readl(drvdata->base[emi_id] + drvdata->parity_err_offset);
		parity_err_ext_value = readl(drvdata->base[emi_id] + drvdata->parity_err_ext_offset);
        	parity_err_tol = parity_err_value + ((unsigned long long)parity_err_ext_value << 32);
		if (parity_err_tol > 0)
			ecc_mesg_idx = snprintf(ecc_mesg + ecc_mesg_idx, sizeof(ecc_mesg) - ecc_mesg_idx, "emi: %d, overall: %llx\n", emi_id, parity_err_tol);
	}

	for (emi_id = 0; emi_id < drvdata->slc_parity_cnt; emi_id++) {
		partial_error_type = read_parity_status(dci, emi_id);
		if (total_error_type == NO_ERROR) {
			if (partial_error_type != NO_ERROR)
				total_error_type = partial_error_type;
		} else if (total_error_type == CORRECTABLE_ERROR) {
			if (partial_error_type == UNCORRECTABLE_ERROR)
				total_error_type = partial_error_type;
		}
	}

	for (emi_id = 0; emi_id < drvdata->slc_parity_cnt; emi_id++) {
		slc_clear_violation(emi_id);
	}

	if (total_error_type == UNCORRECTABLE_ERROR) {
		pr_info("error type: %d bit error\n", total_error_type);
		aee_kernel_exception("SLC_PARITY", ecc_mesg);
	}

	if (drvdata->assert)
                BUG_ON(1);

	return IRQ_HANDLED;
}

static const struct of_device_id slc_parity_of_ids[] = {
	{.compatible = "mediatek,edac-slc-parity",},
	{}
};
MODULE_DEVICE_TABLE(of, slc_parity_of_ids);

static int slc_err_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;
	struct edac_device_ctl_info *dci;
	struct slc_drvdata *drvdata;
	struct resource *r;
	int res = 0;
	int size, ret, i;
	unsigned int *dump_list;

	dci = edac_device_alloc_ctl_info(sizeof(*drvdata), "slc", 1, "slc", 1, 0, 0);
	if (!dci) {
		dev_info(&pdev->dev, "edac alloc ctl fail\n");
		res = -ENOMEM;
		goto err1;
	}

	drvdata = dci->pvt_info;
	dci->dev = &pdev->dev;
	platform_set_drvdata(pdev, dci);

	if (!devres_open_group(&pdev->dev, NULL, GFP_KERNEL)) {
		dev_info(&pdev->dev, "Open group fail\n");
		res = -ENOMEM;
		goto err2;
	}

	size = of_property_count_elems_of_size(pdev->dev.of_node, "reg", sizeof(unsigned int) * 4);
	if (size <= 0) {
		dev_info(&pdev->dev, "Unable to get regs\n");
		res = -ENXIO;
		goto err2;
	}
	drvdata->slc_parity_cnt = (unsigned int)size;

	drvdata->base = devm_kmalloc_array(&pdev->dev, drvdata->slc_parity_cnt, sizeof(unsigned int), GFP_KERNEL);
	if (!drvdata->base) {
		dev_info(&pdev->dev, "Unable to alloc mem\n");
		res = -ENOMEM;
		goto err2;
	}

	for (i = 0; i < drvdata->slc_parity_cnt; i++) {
		r = platform_get_resource(pdev, IORESOURCE_MEM, i);
		drvdata->base[i] = devm_ioremap(&pdev->dev, r->start, resource_size(r));
		if (!drvdata->base[i]) {
			dev_info(&pdev->dev, "Unable to map regs\n");
			res = -EIO;
			goto err2;
		}
	}

	size = of_property_count_elems_of_size(pdev->dev.of_node, "dump", sizeof(unsigned int));
	if (size <= 0) {
		dev_info(&pdev->dev, "Unable to get dump size\n");
		res = -ENXIO;
		goto err2;
	}

	dump_list = devm_kmalloc(&pdev->dev, size * sizeof(unsigned int), GFP_KERNEL);
	if (!dump_list) {
		dev_info(&pdev->dev, "Unable to alloc mem\n");
		res = -ENOMEM;
		goto err2;
	}

	ret = of_property_read_u32_array(pdev->dev.of_node, "dump", dump_list, size);
	if (ret) {
		dev_info(&pdev->dev, "Unable to get dump value\n");
		res = -ENXIO;
		goto err2;
	}
	drvdata->parity_err_offset = dump_list[0];
	drvdata->parity_err_ext_offset = dump_list[1];
	drvdata->parity_err_addr_offset = dump_list[2];
	drvdata->parity_err_status_offset = dump_list[3];

	ret = of_property_read_u32(pdev->dev.of_node, "port-num", &(drvdata->port_num));
	if (ret) {
		dev_info(&pdev->dev, "Unable to get port-num\n");
		res = -ENXIO;
		goto err2;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "cs-num", &(drvdata->cs_num));
	if (ret) {
		dev_info(&pdev->dev, "Unable to get cs-num\n");
		res = -ENXIO;
		goto err2;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "chn-num", &(drvdata->chn_num));
	if (ret) {
		dev_info(&pdev->dev, "Unable to get chn-num\n");
		res = -ENXIO;
		goto err2;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "assert", &(drvdata->assert));
	if (ret) {
		dev_info(&pdev->dev, "Unable to get assert\n");
		res = -ENXIO;
		goto err2;
	}

	id = of_match_device(slc_parity_of_ids, &pdev->dev);
	dci->mod_name = pdev->dev.driver->name;
	dci->ctl_name = id ? id->compatible : "unknown";
	dci->dev_name = dev_name(&pdev->dev);

	if (edac_device_add_device(dci)) {
		dev_info(&pdev->dev, "Add edac device fail\n");
		res = -ENOMEM;
		goto err2;
	}

	drvdata->db_irq = platform_get_irq(pdev, 0);
	res = devm_request_threaded_irq(&pdev->dev, drvdata->db_irq,
			       NULL, (irq_handler_t)slc_err_handler,
			       IRQF_TRIGGER_NONE | IRQF_ONESHOT, dev_name(&pdev->dev), dci);
	if (res) {
		dev_info(&pdev->dev, "Request threaded irq fail\n");
		goto err3;
	}

	devres_close_group(&pdev->dev, NULL);

	return 0;
err3:
	dev_info(&pdev->dev, "in err3\n");
	edac_device_del_device(&pdev->dev);
err2:
	dev_info(&pdev->dev, "in err2\n");
	devres_release_group(&pdev->dev, NULL);
	edac_device_free_ctl_info(dci);
err1:
	dev_info(&pdev->dev, "in err1\n");
	return res;
}

static void slc_err_remove(struct platform_device *pdev)
{
	struct edac_device_ctl_info *dci = platform_get_drvdata(pdev);

	edac_device_del_device(&pdev->dev);
	edac_device_free_ctl_info(dci);
};

static struct platform_driver slc_edac_driver = {
	.probe = slc_err_probe,
	.remove = slc_err_remove,
	.driver = {
		.name = "slc_parity_driver",
		.of_match_table = slc_parity_of_ids,
	},
};

module_platform_driver(slc_edac_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("EDAC Driver for MediaTek SLC");
