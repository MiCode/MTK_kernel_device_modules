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

#define SLC_BUF_SIZE 1024

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
	unsigned int error_flags_enable;
	struct error_flags_data *error_flags;
};

struct error_flags_data{
	unsigned int *int_sts;
	int int_sts_len;
	unsigned int *int_sts_msk;
	int int_sts_msk_len;
	unsigned int *int_sts_fatal;
	int int_sts_fatal_len;
	unsigned int *int_sts_clr;
	int int_sts_clr_len;
	unsigned int mux_sel;
	unsigned int *mux_vals;
	int mux_vals_len;
	unsigned int *mux_data;
	int mux_data_len;
	unsigned int dfd_enable;
	struct dfd_data *dfd;
	unsigned int tag_ecc_enable;
	struct tag_ecc_data *tag_ecc;
};

struct dfd_data{
	unsigned int *data_err_status;
	unsigned int *tag_err_status;
	unsigned int *latch_data;
	unsigned int *sram_sel;
	unsigned int data_num;
	unsigned int tag_num;
};

struct tag_ecc_data{
	unsigned int *ce_int_sts_msk;
	unsigned int *ue_int_sts_msk;
	unsigned int mux_sel;
	unsigned int mux_num;
	unsigned int *mux_data;
};

static int err_flag_dump(struct slc_drvdata *drvdata, char slc_err_mesg[], int *slc_err_mesg_idx,
	enum error_type *total_error_type);
static void dfd_dump(struct slc_drvdata *drvdata, char slc_err_mesg[], int *slc_err_mesg_idx);
static void tag_ecc_dump(struct edac_device_ctl_info *dci, char slc_err_mesg[], int *slc_err_mesg_idx);

static enum error_type read_parity_status(struct edac_device_ctl_info *dci, unsigned int emi_idx)
{
	struct arm_smccc_res smc_res;
	struct slc_drvdata *drvdata = dci->pvt_info;
	unsigned int content;
	int port_idx, chn_idx, cs_idx;
	int ecc_position_idx;
	int dump_success;
	enum error_type partial_error_type = NO_ERROR;
	enum error_type total_error_type = NO_ERROR;
	char ecc_position[40] = {0};

	for (port_idx = 0; port_idx < drvdata->port_num; port_idx++) {
		arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_SLC_PARITY_SELECT,
				emi_idx, port_idx, 0, 0, 0, 0, &smc_res);
		if (smc_res.a0) {
			pr_info("%s:%d failed to select slc parity, ret=0x%lx\n",
				__func__, __LINE__, smc_res.a0);
		}
		for (chn_idx = 0; chn_idx < drvdata->chn_num; chn_idx++) {
			for (cs_idx = 0; cs_idx < drvdata->cs_num; cs_idx++) {
				content = readl(drvdata->base[emi_idx] + drvdata->parity_err_status_offset + ((chn_idx * drvdata->cs_num + cs_idx) << 2));
				dump_success = 0;
				ecc_position_idx = snprintf(ecc_position, sizeof(ecc_position),
					"emi: %d, port: %d, chn: %d, cs: %d", emi_idx, port_idx, chn_idx, cs_idx);
				if (ecc_position_idx > 0 && ecc_position_idx < sizeof(ecc_position))
					dump_success = 1;

				if (((content & 0x3) == 0x3) || (((content >> 2) & 0x3) == 0x3)) {
					partial_error_type = UNCORRECTABLE_ERROR;
					if (dump_success)
						edac_device_handle_ue_count(dci, 2, 0, 0, ecc_position);
				} else if (((content & 0x3) == 0x1) || (((content >> 2) & 0x3) == 0x1)) {
					partial_error_type = CORRECTABLE_ERROR;
					if (dump_success)
						edac_device_handle_ce_count(dci, 1, 0, 0, ecc_position);
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

static void slc_clear_violation(unsigned int emi_idx)
{
	struct arm_smccc_res smc_res;

	arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_SLC_PARITY_CLEAR,
			emi_idx, 0, 0, 0, 0, 0, &smc_res);
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
	int dump_log;
	char slc_err_mesg[SLC_BUF_SIZE] = {0};
	int slc_err_mesg_idx = 0;
	unsigned long long parity_err_tol;
	unsigned int parity_err_value;
	unsigned int parity_err_ext_value;
	unsigned int emi_idx;
	struct arm_smccc_res smc_res;

	for (emi_idx = 0; emi_idx < drvdata->slc_parity_cnt; emi_idx++) {
		parity_err_value = readl(drvdata->base[emi_idx] + drvdata->parity_err_offset);
		parity_err_ext_value = readl(drvdata->base[emi_idx] + drvdata->parity_err_ext_offset);
        	parity_err_tol = parity_err_value + ((unsigned long long)parity_err_ext_value << 32);
		if (parity_err_tol > 0)
			if (slc_err_mesg_idx < SLC_BUF_SIZE)
				slc_err_mesg_idx += snprintf(slc_err_mesg + slc_err_mesg_idx,
					sizeof(slc_err_mesg) - slc_err_mesg_idx, "emi: %d, overall: %llx\n",
					emi_idx, parity_err_tol);
	}
	for (emi_idx = 0; emi_idx < drvdata->slc_parity_cnt; emi_idx++) {
		partial_error_type = read_parity_status(dci, emi_idx);
		if (total_error_type == NO_ERROR) {
			if (partial_error_type != NO_ERROR)
				total_error_type = partial_error_type;
		} else if (total_error_type == CORRECTABLE_ERROR) {
			if (partial_error_type == UNCORRECTABLE_ERROR)
				total_error_type = partial_error_type;
		}
	}
	if (total_error_type == UNCORRECTABLE_ERROR)
		BUG_ON(1);
	if ((total_error_type == CORRECTABLE_ERROR) && drvdata->assert)
		BUG_ON(1);
	for (emi_idx = 0; emi_idx < drvdata->slc_parity_cnt; emi_idx++) {
		slc_clear_violation(emi_idx);
	}

	//error flag
	slc_err_mesg_idx = 0;
	total_error_type = NO_ERROR;
	if (drvdata->error_flags_enable == 1) {
		if (slc_err_mesg_idx < SLC_BUF_SIZE)
			slc_err_mesg_idx += snprintf(slc_err_mesg + slc_err_mesg_idx,
				SLC_BUF_SIZE - slc_err_mesg_idx, "debug flags\n");
		dump_log = err_flag_dump(drvdata, slc_err_mesg, &slc_err_mesg_idx, &total_error_type);
		if (dump_log == 1) {
			pr_info("%s", slc_err_mesg);
			memset(slc_err_mesg, '\0', SLC_BUF_SIZE);
			slc_err_mesg_idx = 0;
		}
		//dfd
		if (drvdata->error_flags->dfd_enable == 1) {
			dfd_dump(drvdata, slc_err_mesg, &slc_err_mesg_idx);
			if (dump_log == 1) {
				pr_info("%s", slc_err_mesg);
				memset(slc_err_mesg, '\0', SLC_BUF_SIZE);
				slc_err_mesg_idx = 0;
			}
		}
		//tag_ecc
		if (drvdata->error_flags->tag_ecc_enable == 1) {
			tag_ecc_dump(dci, slc_err_mesg, &slc_err_mesg_idx);
		}
		if (total_error_type == UNCORRECTABLE_ERROR)
			BUG_ON(1);
		if ((total_error_type == CORRECTABLE_ERROR) && drvdata->assert)
			BUG_ON(1);
		//clear error flag
		for (emi_idx = 0; emi_idx < drvdata->slc_parity_cnt; emi_idx++) {
			arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_SLC_ERROR_FLAGS_CLEAR,
					emi_idx, 0, 0, 0, 0, 0, &smc_res);
			if (smc_res.a0) {
				pr_info("%s:%d MTK_SLC_ERROR_FLAGS_CLEAR failed, ret=0x%lx\n",
					__func__, __LINE__, smc_res.a0);
			}
		}
	}

	return IRQ_HANDLED;
}

static int err_flag_dump(struct slc_drvdata *drvdata, char slc_err_mesg[], int *slc_err_mesg_idx,
	enum error_type *total_error_type)
{
	unsigned int emi_idx, i, j, reg_val;
	int dump_log = 0;
	struct arm_smccc_res smc_res;

	//dump error flag
	for (emi_idx = 0; emi_idx < drvdata->slc_parity_cnt; emi_idx++) {
		if (*slc_err_mesg_idx < SLC_BUF_SIZE)
			*slc_err_mesg_idx += snprintf(slc_err_mesg + *slc_err_mesg_idx,
				SLC_BUF_SIZE - *slc_err_mesg_idx, "emi: %d", emi_idx);
		for (i=0; i<drvdata->error_flags->int_sts_len; ++i) {
			reg_val = readl(drvdata->base[emi_idx] + drvdata->error_flags->int_sts[i]);
			if ((reg_val & drvdata->error_flags->int_sts_msk[i]) != 0 )
				dump_log = 1;
			if ((reg_val & drvdata->error_flags->int_sts_fatal[i]) != 0 )
				*total_error_type = UNCORRECTABLE_ERROR;
			if (*slc_err_mesg_idx < SLC_BUF_SIZE)
				*slc_err_mesg_idx += snprintf(slc_err_mesg + *slc_err_mesg_idx,
					SLC_BUF_SIZE - *slc_err_mesg_idx, ", %x: %x",
					drvdata->error_flags->int_sts[i], reg_val);
		}
		if (*slc_err_mesg_idx < SLC_BUF_SIZE)
			*slc_err_mesg_idx += snprintf(slc_err_mesg + *slc_err_mesg_idx,
				SLC_BUF_SIZE - *slc_err_mesg_idx, ", mux:\n");
		for (i=0; i<drvdata->error_flags->mux_vals_len; ++i) {
			if (*slc_err_mesg_idx < SLC_BUF_SIZE)
				*slc_err_mesg_idx += snprintf(slc_err_mesg + *slc_err_mesg_idx,
					SLC_BUF_SIZE - *slc_err_mesg_idx, "%d",
					drvdata->error_flags->mux_vals[i]);
			arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_SLC_ERROR_FLAGS_SELECT,
					emi_idx, drvdata->error_flags->mux_vals[i], 0, 0, 0, 0, &smc_res);
			if (smc_res.a0) {
				pr_info("%s:%d MTK_SLC_ERROR_FLAGS_SELECT failed, ret=0x%lx\n",
					__func__, __LINE__, smc_res.a0);
			}
			for (j=0; j<drvdata->error_flags->mux_data_len; ++j)
				if (*slc_err_mesg_idx < SLC_BUF_SIZE)
					*slc_err_mesg_idx += snprintf(slc_err_mesg + *slc_err_mesg_idx,
						SLC_BUF_SIZE - *slc_err_mesg_idx, ", %x: %x",
						drvdata->error_flags->mux_data[j],
						readl(drvdata->base[emi_idx] + drvdata->error_flags->mux_data[j]));
			if (*slc_err_mesg_idx < SLC_BUF_SIZE)
				*slc_err_mesg_idx += snprintf(slc_err_mesg + *slc_err_mesg_idx,
					SLC_BUF_SIZE - *slc_err_mesg_idx, "\n");
		}
	}

	return dump_log;
}

static void dfd_dump(struct slc_drvdata *drvdata, char slc_err_mesg[], int *slc_err_mesg_idx)
{
	unsigned int emi_idx, chn_idx, reg_val, tag_idx, data_idx;
	struct arm_smccc_res smc_res;
	struct dfd_data *dfd = drvdata->error_flags->dfd;

	if (*slc_err_mesg_idx < SLC_BUF_SIZE)
		*slc_err_mesg_idx += snprintf(slc_err_mesg + *slc_err_mesg_idx,
			SLC_BUF_SIZE - *slc_err_mesg_idx, "slc dfd\n");
	for (emi_idx = 0; emi_idx < drvdata->slc_parity_cnt; emi_idx++) {
		if (*slc_err_mesg_idx < SLC_BUF_SIZE)
			*slc_err_mesg_idx += snprintf(slc_err_mesg + *slc_err_mesg_idx,
				SLC_BUF_SIZE - *slc_err_mesg_idx, "emi: %d\n", emi_idx);
		for (chn_idx=0; chn_idx < drvdata->chn_num; ++chn_idx) {
			if (*slc_err_mesg_idx < SLC_BUF_SIZE)
				*slc_err_mesg_idx += snprintf(slc_err_mesg + *slc_err_mesg_idx,
					SLC_BUF_SIZE - *slc_err_mesg_idx, "chn: %d", chn_idx);
			//data
			if (*slc_err_mesg_idx < SLC_BUF_SIZE)
				*slc_err_mesg_idx += snprintf(slc_err_mesg + *slc_err_mesg_idx,
					SLC_BUF_SIZE - *slc_err_mesg_idx, ", data");
			reg_val = readl(drvdata->base[emi_idx] + dfd->data_err_status[chn_idx]);
			for (data_idx = 0; data_idx < dfd->data_num; ++data_idx) {
				if (((reg_val>>data_idx) & 0x1) == 1) {
					arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_SLC_DFD_DATA_SELECT,
							emi_idx, chn_idx, data_idx, 0, 0, 0, &smc_res);
					if (smc_res.a0) {
						pr_info("%s:%d MTK_SLC_DFD_SELECT failed, ret=0x%lx\n",
							__func__, __LINE__, smc_res.a0);
					}
					if (*slc_err_mesg_idx < SLC_BUF_SIZE)
						*slc_err_mesg_idx += snprintf(slc_err_mesg + *slc_err_mesg_idx,
							SLC_BUF_SIZE - *slc_err_mesg_idx, ", %x: %x", data_idx,
							readl(drvdata->base[emi_idx] + dfd->latch_data[chn_idx]));
				}
			}
			//tag
			if (*slc_err_mesg_idx < SLC_BUF_SIZE)
				*slc_err_mesg_idx += snprintf(slc_err_mesg + *slc_err_mesg_idx,
					SLC_BUF_SIZE - *slc_err_mesg_idx, ", tag");
			reg_val = readl(drvdata->base[emi_idx] + dfd->tag_err_status[chn_idx]);
			for (tag_idx = 0; tag_idx < dfd->tag_num; ++tag_idx) {
				if (((reg_val>>tag_idx) & 0x1) == 1) {
					arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_SLC_DFD_TAG_SELECT,
							emi_idx, chn_idx, tag_idx, 0, 0, 0, &smc_res);
					if (smc_res.a0) {
						pr_info("%s:%d MTK_SLC_DFD_SELECT failed, ret=0x%lx\n",
							__func__, __LINE__, smc_res.a0);
					}
					if (*slc_err_mesg_idx < SLC_BUF_SIZE)
						*slc_err_mesg_idx += snprintf(slc_err_mesg + *slc_err_mesg_idx,
							SLC_BUF_SIZE - *slc_err_mesg_idx, ", %x: %x", tag_idx,
							readl(drvdata->base[emi_idx] + dfd->latch_data[chn_idx]));
				}
			}
			if (*slc_err_mesg_idx < SLC_BUF_SIZE)
				*slc_err_mesg_idx += snprintf(slc_err_mesg + *slc_err_mesg_idx,
					SLC_BUF_SIZE - *slc_err_mesg_idx, "\n");
		}
	}
}

static void tag_ecc_dump(struct edac_device_ctl_info *dci, char slc_err_mesg[], int *slc_err_mesg_idx)
{
	struct slc_drvdata *drvdata = dci->pvt_info;
	unsigned int emi_idx, chn_idx, mux_idx, i, reg_val;
	int tag_ecc_ce = 0;
	int tag_ecc_ue = 0;
	struct arm_smccc_res smc_res;
	struct tag_ecc_data *tag_ecc = drvdata->error_flags->tag_ecc;

	for (emi_idx = 0; emi_idx < drvdata->slc_parity_cnt; emi_idx++) {
		for (i=0; i<drvdata->error_flags->int_sts_len; ++i) {
			reg_val = readl(drvdata->base[emi_idx] + drvdata->error_flags->int_sts[i]);
			if ((reg_val & tag_ecc->ce_int_sts_msk[i]) != 0 )
				tag_ecc_ce = 1;
			if ((reg_val & tag_ecc->ue_int_sts_msk[i]) != 0 )
				tag_ecc_ue = 1;
		}
	}
	if (!tag_ecc_ce && !tag_ecc_ue)
		return;

	if (*slc_err_mesg_idx < SLC_BUF_SIZE)
		*slc_err_mesg_idx += snprintf(slc_err_mesg + *slc_err_mesg_idx,
			SLC_BUF_SIZE - *slc_err_mesg_idx, "tag ecc\n");
	for (emi_idx = 0; emi_idx < drvdata->slc_parity_cnt; emi_idx++) {
		if (*slc_err_mesg_idx < SLC_BUF_SIZE)
			*slc_err_mesg_idx += snprintf(slc_err_mesg + *slc_err_mesg_idx,
				SLC_BUF_SIZE - *slc_err_mesg_idx, "emi: %d, mux:\n", emi_idx);
		for (mux_idx=0; mux_idx<tag_ecc->mux_num; ++mux_idx) {
			if (*slc_err_mesg_idx < SLC_BUF_SIZE)
				*slc_err_mesg_idx += snprintf(slc_err_mesg + *slc_err_mesg_idx,
					SLC_BUF_SIZE - *slc_err_mesg_idx, "%d", mux_idx);
			arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_SLC_TAG_ECC_SELECT,
					emi_idx, mux_idx, 0, 0, 0, 0, &smc_res);
			if (smc_res.a0) {
				pr_info("%s:%d MTK_SLC_TAG_ECC_SELECT failed, ret=0x%lx\n",
					__func__, __LINE__, smc_res.a0);
			}
			for (chn_idx=0; chn_idx < drvdata->chn_num; ++chn_idx) {
				if (*slc_err_mesg_idx < SLC_BUF_SIZE)
					*slc_err_mesg_idx += snprintf(slc_err_mesg + *slc_err_mesg_idx,
						SLC_BUF_SIZE - *slc_err_mesg_idx, ", %x: %x",
						tag_ecc->mux_data[chn_idx],
						readl(drvdata->base[emi_idx] + tag_ecc->mux_data[chn_idx]));
			}
			if (*slc_err_mesg_idx < SLC_BUF_SIZE)
				*slc_err_mesg_idx += snprintf(slc_err_mesg + *slc_err_mesg_idx,
					SLC_BUF_SIZE - *slc_err_mesg_idx, "\n");
		}
		arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_SLC_TAG_ECC_CLEAR,
				emi_idx, 0, 0, 0, 0, 0, &smc_res);
		if (smc_res.a0) {
			pr_info("%s:%d MTK_SLC_TAG_ECC_CLEAR failed, ret=0x%lx\n",
				__func__, __LINE__, smc_res.a0);
		}
	}
	if (tag_ecc_ce) {
		edac_device_handle_ce_count(dci, 1, 0, 0, "tag");
	}
	if (tag_ecc_ue) {
		edac_device_handle_ue_count(dci, 2, 0, 0, "tag");
		pr_info("%s", slc_err_mesg);
		memset(slc_err_mesg, '\0', SLC_BUF_SIZE);
		slc_err_mesg_idx = 0;
	}
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
	struct device_node *error_flags_node = NULL;
	struct device_node *dfd_node = NULL;
	struct device_node *tag_ecc_node = NULL;
	struct dfd_data *dfd = NULL;
	struct tag_ecc_data *tag_ecc = NULL;

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

	drvdata->base = devm_kzalloc(&pdev->dev, drvdata->slc_parity_cnt * sizeof(unsigned int), GFP_KERNEL);
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

	size = of_property_count_u32_elems(pdev->dev.of_node, "dump");
	if (size <= 0) {
		dev_info(&pdev->dev, "Unable to get dump size\n");
		res = -ENXIO;
		goto err2;
	}

	dump_list = devm_kzalloc(&pdev->dev, size * sizeof(unsigned int), GFP_KERNEL);
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

	error_flags_node = of_get_child_by_name(pdev->dev.of_node, "error-flags");
	ret = 0;
	if (error_flags_node) {
		drvdata->error_flags_enable = 1;
		dev_info(&pdev->dev, "error flags support\n");
		drvdata->error_flags = devm_kzalloc(&pdev->dev, sizeof(*(drvdata->error_flags)), GFP_KERNEL);
		if (!drvdata->error_flags) {
			dev_info(&pdev->dev, "Failed to allocate memory for error_flags\n");
			ret = -ENOMEM;
			goto err2;
		}

		//int-sts
		drvdata->error_flags->int_sts_len = of_property_count_u32_elems(error_flags_node, "int-sts");
		if (drvdata->error_flags->int_sts_len < 0) {
			dev_info(&pdev->dev, "Failed to count int-sts elements\n");
			ret = drvdata->error_flags->int_sts_len;
			goto err2;
		}
		drvdata->error_flags->int_sts = devm_kzalloc(&pdev->dev, drvdata->error_flags->int_sts_len * sizeof(u32), GFP_KERNEL);
		if (!drvdata->error_flags->int_sts) {
			dev_info(&pdev->dev, "Failed to allocate memory for int-sts\n");
			ret = -ENOMEM;
			goto err2;
		}
		ret = of_property_read_u32_array(error_flags_node, "int-sts", drvdata->error_flags->int_sts, drvdata->error_flags->int_sts_len);
		if (ret) {
			dev_info(&pdev->dev, "Failed to read int-sts property\n");
			goto err2;
		}


		//int-sts-msk
		drvdata->error_flags->int_sts_msk_len = of_property_count_u32_elems(error_flags_node, "int-sts-msk");
		if (drvdata->error_flags->int_sts_msk_len < 0) {
			dev_info(&pdev->dev, "Failed to count int-sts-msk elements\n");
			ret = drvdata->error_flags->int_sts_msk_len;
			goto err2;
		}
		drvdata->error_flags->int_sts_msk = devm_kzalloc(&pdev->dev, drvdata->error_flags->int_sts_msk_len * sizeof(u32), GFP_KERNEL);
		if (!drvdata->error_flags->int_sts_msk) {
			dev_info(&pdev->dev, "Failed to allocate memory for int-sts-msk\n");
			ret = -ENOMEM;
			goto err2;
		}
		ret = of_property_read_u32_array(error_flags_node, "int-sts-msk", drvdata->error_flags->int_sts_msk, drvdata->error_flags->int_sts_msk_len);
		if (ret) {
			dev_info(&pdev->dev, "Failed to read int-sts-msk property\n");
			goto err2;
		}

		//int-sts-fatal
		drvdata->error_flags->int_sts_fatal_len = of_property_count_u32_elems(error_flags_node,
			"int-sts-fatal");
		if (drvdata->error_flags->int_sts_fatal_len < 0) {
			dev_info(&pdev->dev, "Failed to count int-sts-fatal elements\n");
			ret = drvdata->error_flags->int_sts_fatal_len;
			goto err2;
		}
		drvdata->error_flags->int_sts_fatal = devm_kzalloc(&pdev->dev,
			drvdata->error_flags->int_sts_fatal_len * sizeof(u32), GFP_KERNEL);
		if (!drvdata->error_flags->int_sts_fatal) {
			ret = -ENOMEM;
			goto err2;
		}
		ret = of_property_read_u32_array(error_flags_node, "int-sts-fatal",
			drvdata->error_flags->int_sts_fatal, drvdata->error_flags->int_sts_fatal_len);
		if (ret) {
			dev_info(&pdev->dev, "Failed to read int-sts-fatal property\n");
			goto err2;
		}

		//int-sts-clr
		drvdata->error_flags->int_sts_clr_len = of_property_count_u32_elems(error_flags_node,
			"int-sts-clr");
		if (drvdata->error_flags->int_sts_clr_len < 0) {
			dev_info(&pdev->dev, "Failed to count int-sts-clr elements\n");
			ret = drvdata->error_flags->int_sts_clr_len;
			goto err2;
		}
		drvdata->error_flags->int_sts_clr = devm_kzalloc(&pdev->dev,
			drvdata->error_flags->int_sts_clr_len * sizeof(u32), GFP_KERNEL);
		if (!drvdata->error_flags->int_sts_clr) {
			dev_info(&pdev->dev, "Failed to allocate memory for int-sts-clr\n");
			ret = -ENOMEM;
			goto err2;
		}
		ret = of_property_read_u32_array(error_flags_node, "int-sts-clr",
			drvdata->error_flags->int_sts_clr, drvdata->error_flags->int_sts_clr_len);
		if (ret) {
			dev_info(&pdev->dev, "Failed to read int-sts-clr property\n");
			goto err2;
		}

		//mux-sel
		ret = of_property_read_u32(error_flags_node, "mux-sel", &(drvdata->error_flags->mux_sel));
		if (ret) {
			dev_info(&pdev->dev, "Failed to get mux_sel property\n");
			goto err2;
		}

		//mux-vals
		drvdata->error_flags->mux_vals_len = of_property_count_u32_elems(error_flags_node,
			"mux-vals");
		if (drvdata->error_flags->mux_vals_len < 0) {
			dev_info(&pdev->dev, "Failed to count mux-vals elements\n");
			ret = drvdata->error_flags->mux_vals_len;
			goto err2;
		}
		drvdata->error_flags->mux_vals = devm_kzalloc(&pdev->dev,
			drvdata->error_flags->mux_vals_len * sizeof(u32), GFP_KERNEL);
		if (!drvdata->error_flags->mux_vals) {
			ret = -ENOMEM;
			goto err2;
		}
		ret = of_property_read_u32_array(error_flags_node, "mux-vals",
			drvdata->error_flags->mux_vals, drvdata->error_flags->mux_vals_len);
		if (ret) {
			dev_info(&pdev->dev, "Failed to read mux-vals property\n");
			goto err2;
		}

		//mux-data
		drvdata->error_flags->mux_data_len = of_property_count_u32_elems(error_flags_node,
			"mux-data");
		if (drvdata->error_flags->mux_data_len < 0) {
			dev_info(&pdev->dev, "Failed to count mux-data elements\n");
			ret = drvdata->error_flags->mux_data_len;
			goto err2;
		}
		drvdata->error_flags->mux_data = devm_kzalloc(&pdev->dev,
			drvdata->error_flags->mux_data_len * sizeof(u32), GFP_KERNEL);
		if (!drvdata->error_flags->mux_data) {
			ret = -ENOMEM;
			goto err2;
		}
		ret = of_property_read_u32_array(error_flags_node, "mux-data",
			drvdata->error_flags->mux_data, drvdata->error_flags->mux_data_len);
		if (ret) {
			dev_info(&pdev->dev, "Failed to read mux-data property\n");
			goto err2;
                }
		dfd_node = of_get_child_by_name(error_flags_node, "dfd");
		if (dfd_node) {
			drvdata->error_flags->dfd_enable = 1;
			dev_info(&pdev->dev, "dfd support\n");
			drvdata->error_flags->dfd = devm_kzalloc(&pdev->dev,
				sizeof(*(drvdata->error_flags->dfd)), GFP_KERNEL);
			if (!drvdata->error_flags->dfd) {
				dev_info(&pdev->dev, "Failed to allocate memory for dfd\n");
				ret = -ENOMEM;
				goto err2;
			}
			dfd = drvdata->error_flags->dfd;

			//data-err-status
			dfd->data_err_status = devm_kzalloc(&pdev->dev, drvdata->chn_num * sizeof(u32), GFP_KERNEL);
			if (!dfd->data_err_status) {
				dev_info(&pdev->dev, "Failed to allocate memory for data-err-status\n");
				ret = -ENOMEM;
				goto err2;
			}
			ret = of_property_read_u32_array(dfd_node, "data-err-status", dfd->data_err_status,
				drvdata->chn_num);
			if (ret) {
				dev_info(&pdev->dev, "Failed to read data-err-status property\n");
				goto err2;
			}

			//tag-err-status
			dfd->tag_err_status = devm_kzalloc(&pdev->dev, drvdata->chn_num * sizeof(u32), GFP_KERNEL);
			if (!dfd->tag_err_status) {
				dev_info(&pdev->dev, "Failed to allocate memory for tag-err-status\n");
				ret = -ENOMEM;
				goto err2;
			}
			ret = of_property_read_u32_array(dfd_node, "tag-err-status", dfd->tag_err_status,
				drvdata->chn_num);
			if (ret) {
				dev_info(&pdev->dev, "Failed to read tag-err-status property\n");
				goto err2;
			}

			//data-num
			ret = of_property_read_u32(dfd_node, "data-num", &(dfd->data_num));
			if (ret) {
				dev_info(&pdev->dev, "Unable to get data-num\n");
				res = -ENXIO;
				goto err2;
			}

			//tag-num
			ret = of_property_read_u32(dfd_node, "tag-num", &(dfd->tag_num));
			if (ret) {
				dev_info(&pdev->dev, "Unable to get tag-num\n");
				res = -ENXIO;
				goto err2;
			}

			//latch-data
			dfd->latch_data = devm_kzalloc(&pdev->dev, drvdata->chn_num * sizeof(u32), GFP_KERNEL);
			if (!dfd->latch_data) {
				dev_info(&pdev->dev, "Failed to allocate memory for latch-data\n");
				ret = -ENOMEM;
				goto err2;
			}
			ret = of_property_read_u32_array(dfd_node, "latch-data", dfd->latch_data, drvdata->chn_num);
			if (ret) {
				dev_info(&pdev->dev, "Failed to read latch-data property\n");
				goto err2;
			}

			//sram-sel
			dfd->sram_sel = devm_kzalloc(&pdev->dev, drvdata->chn_num * sizeof(u32), GFP_KERNEL);
			if (!dfd->sram_sel) {
				dev_info(&pdev->dev, "Failed to allocate memory for sram-sel\n");
				ret = -ENOMEM;
				goto err2;
			}
			ret = of_property_read_u32_array(dfd_node, "sram-sel", dfd->sram_sel, drvdata->chn_num);
			if (ret) {
				dev_info(&pdev->dev, "Failed to read sram-sel property\n");
				goto err2;
			}
		}
		tag_ecc_node = of_get_child_by_name(error_flags_node, "tag-ecc");
		if (tag_ecc_node) {
			drvdata->error_flags->tag_ecc_enable = 1;
			dev_info(&pdev->dev, "tag_ecc support\n");
			drvdata->error_flags->tag_ecc = devm_kzalloc(&pdev->dev,
				sizeof(*(drvdata->error_flags->tag_ecc)), GFP_KERNEL);
			if (!drvdata->error_flags->tag_ecc) {
				ret = -ENOMEM;
				goto err2;
			}
			tag_ecc = drvdata->error_flags->tag_ecc;

			//ce-int-sts-msk
			tag_ecc->ce_int_sts_msk = devm_kzalloc(&pdev->dev, drvdata->error_flags->int_sts_msk_len * sizeof(u32), GFP_KERNEL);
			if (!tag_ecc->ce_int_sts_msk) {
				dev_info(&pdev->dev, "Failed to allocate memory for ce-int-sts-msk\n");
				ret = -ENOMEM;
				goto err2;
			}
			ret = of_property_read_u32_array(tag_ecc_node, "ce-int-sts-msk", tag_ecc->ce_int_sts_msk, drvdata->error_flags->int_sts_msk_len);
			if (ret) {
				dev_info(&pdev->dev, "Failed to read ce-int-sts-msk property\n");
				goto err2;
			}

			//ue-int-sts-msk
			tag_ecc->ue_int_sts_msk = devm_kzalloc(&pdev->dev, drvdata->error_flags->int_sts_msk_len * sizeof(u32), GFP_KERNEL);
			if (!tag_ecc->ue_int_sts_msk) {
				dev_info(&pdev->dev, "Failed to allocate memory for ue-int-sts-msk\n");
				ret = -ENOMEM;
				goto err2;
			}
			ret = of_property_read_u32_array(tag_ecc_node, "ue-int-sts-msk", tag_ecc->ue_int_sts_msk, drvdata->error_flags->int_sts_msk_len);
			if (ret) {
				dev_info(&pdev->dev, "Failed to read ue-int-sts-msk property\n");
				goto err2;
			}

			//mux-sel
			ret = of_property_read_u32(tag_ecc_node, "mux-sel", &(tag_ecc->mux_sel));
			if (ret) {
				dev_info(&pdev->dev, "Unable to get mux-sel\n");
				res = -ENXIO;
				goto err2;
			}

			//mux-num
			ret = of_property_read_u32(tag_ecc_node, "mux-num", &(tag_ecc->mux_num));
			if (ret) {
				dev_info(&pdev->dev, "Unable to get mux-num\n");
				res = -ENXIO;
				goto err2;
			}

			//mux-data
			tag_ecc->mux_data = devm_kzalloc(&pdev->dev, drvdata->chn_num * sizeof(u32), GFP_KERNEL);
			if (!tag_ecc->mux_data) {
				ret = -ENOMEM;
				goto err2;
			}
			ret = of_property_read_u32_array(tag_ecc_node, "mux-data", tag_ecc->mux_data,
				drvdata->chn_num);
			if (ret) {
				dev_info(&pdev->dev, "Failed to read mux-data property\n");
				goto err2;
			}
		}
	} else {
		drvdata->error_flags_enable = 0;
		dev_info(&pdev->dev, "error flags not support\n");
	}

	id = of_match_device(slc_parity_of_ids, &pdev->dev);
	dci->mod_name = pdev->dev.driver->name;
	dci->ctl_name = id ? id->compatible : "unknown";
	dci->dev_name = dev_name(&pdev->dev);
	dci->log_ce = 0;

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

void write2buf(char slc_err_mesg[], int *slc_err_mesg_idx, char *slc_log)
{
	if (*slc_err_mesg_idx < SLC_BUF_SIZE)
		*slc_err_mesg_idx += snprintf(slc_err_mesg + *slc_err_mesg_idx,
			SLC_BUF_SIZE - *slc_err_mesg_idx, slc_log);
}

module_platform_driver(slc_edac_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("EDAC Driver for MediaTek SLC");
