// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <asm/kvm_pkvm_module.h>

#include "pkvm_hypmmu_host.h"

#undef pr_fmt
#define pr_fmt(fmt) "[PKVM_HYPMMU_GPUMPU]: " fmt

static int init_hvc;

static void setup_hvc_call(void)
{
	init_hvc = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(gpu_mpu_hyp_init), mod_token);
}

static int init_hvc_call(void)
{
	int ret;
	u32 gpu_mpu_data_size = 0;
	u32 slot_size = 0;
	u32 ipi_id = 0;
	u32 send_size = 0;
	u32 gpueb_base = 0;
	u32 gpueb_size = 0;
	u32 gpueb_gpr_base = 0;
	u32 mbox0_set = 0;
	u32 mbox0_send = 0;
	u32 gpu_mpu_support = 0;
	u64 arg_1, arg_2, arg_3, arg_4, arg_5;

	do {
		struct device_node *node = NULL;
		int idx;

		node = of_find_compatible_node(NULL, NULL, "mediatek,gpueb");
		if (!node) {
			pr_info("gpueb node not found\n");
			break;
		}

		ret = of_property_read_u32(node, "gpu-mpu-data-size", &gpu_mpu_data_size);
		if (ret) {
			pr_info("gpu-mpu-data-size not found, ret=%d\n", ret);
			break;
		}

		ret = of_property_read_u32(node, "slot-size", &slot_size);
		if (ret) {
			pr_info("slot-size not found, ret=%d\n", ret);
			break;
		}

		idx = of_property_match_string(node, "send-name-table", "IPI_ID_GPUMPU");
		if (idx < 0) {
			pr_info("send-name-table not found, idx=%d\n", idx);
			break;
		}
		/* <id mbox send_size> */
		ret = of_property_read_u32_index(node, "send-table", (u32)(idx * 3), &ipi_id);
		if (ret) {
			pr_info("send-table not found, ret=%d\n", ret);
			break;
		}
		ret = of_property_read_u32_index(node, "send-table", (u32)(idx * 3 + 2), &send_size);
		if (ret) {
			pr_info("send-table not found, ret=%d\n", ret);
			break;
		}

		idx = of_property_match_string(node, "reg-names", "gpueb_base");
		if (idx < 0) {
			pr_info("reg-names not found, idx=%d\n", idx);
			break;
		}
		/* <0 base 0 size> */
		ret = of_property_read_u32_index(node, "reg", (u32)(idx * 4 + 1), &gpueb_base);
		if (ret) {
			pr_info("reg not found, ret=%d\n", ret);
			break;
		}
		ret = of_property_read_u32_index(node, "reg", (u32)(idx * 4 + 3), &gpueb_size);
		if (ret) {
			pr_info("reg not found, ret=%d\n", ret);
			break;
		}

		idx = of_property_match_string(node, "reg-names", "gpueb_gpr_base");
		if (idx < 0) {
			pr_info("reg-names not found, idx=%d\n", idx);
			break;
		}
		/* <0 base 0 size> */
		ret = of_property_read_u32_index(node, "reg", (u32)(idx * 4 + 1), &gpueb_gpr_base);
		if (ret) {
			pr_info("reg not found, ret=%d\n", ret);
			break;
		}

		idx = of_property_match_string(node, "reg-names", "mbox0_set");
		if (idx < 0) {
			pr_info("reg-names not found, idx=%d\n", idx);
			break;
		}
		/* <0 base 0 size> */
		ret = of_property_read_u32_index(node, "reg", (u32)(idx * 4 + 1), &mbox0_set);
		if (ret) {
			pr_info("reg not found, ret=%d\n", ret);
			break;
		}

		idx = of_property_match_string(node, "reg-names", "mbox0_send");
		if (idx < 0) {
			pr_info("reg-names not found, idx=%d\n", idx);
			break;
		}
		/* <0 base 0 size> */
		ret = of_property_read_u32_index(node, "reg", (u32)(idx * 4 + 1), &mbox0_send);
		if (ret) {
			pr_info("reg not found, ret=%d\n", ret);
			break;
		}

		ret = of_property_read_u32(node, "gpu-mpu-support", &gpu_mpu_support);
		if (ret) {
			pr_info("gpu-mpu-support not found, ret=%d\n", ret);
			break;
		}
	} while(0);

	pr_info("gpu_mpu_support=%u gpu_mpu_data_size=0x%x slot_size=%u ipi_id=%u send_size=%u gpueb_base=0x%x gpueb_size=0x%x gpueb_gpr_base=0x%x mbox0_set=0x%x mbox0_send=0x%x\n",
			gpu_mpu_support, gpu_mpu_data_size,
			slot_size,
			ipi_id, send_size,
			gpueb_base, gpueb_size,
			gpueb_gpr_base,
			mbox0_set, mbox0_send);

	arg_1 = ((u64)gpu_mpu_data_size << 32) | (u64)gpu_mpu_support;
	arg_2 = ((u64)send_size         << 32) | (u64)ipi_id;
	arg_3 = ((u64)gpueb_size        << 32) | (u64)gpueb_base;
	arg_4 = ((u64)gpueb_gpr_base    << 32) | (u64)slot_size;
	arg_5 = ((u64)mbox0_send        << 32) | (u64)mbox0_set;

	ret = pkvm_el2_mod_call(init_hvc, arg_1, arg_2, arg_3, arg_4, arg_5);

	return ret;
}

int init_gpumpu(void)
{
	int ret;

	setup_hvc_call();

	ret = init_hvc_call();
	if (ret)
		pr_info("init hvc call failed ret=%d\n", ret);

	return ret;
}
