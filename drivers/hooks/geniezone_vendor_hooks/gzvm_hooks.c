// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/anon_inodes.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/soc/mediatek/gzvm_hook.h>
#include <trace/hooks/gzvm.h>

static gzvm_iommu_sync_t iommu_sync_callback;

void gzvm_register_iommu_sync_cb(gzvm_iommu_sync_t func_ptr)
{
	iommu_sync_callback = func_ptr;
}
EXPORT_SYMBOL(gzvm_register_iommu_sync_cb);

/* vendor hook callback */
static void
gzvm_handle_demand_page_pre_handler(void *data, struct gzvm *vm, int memslot_id, u64 pfn, u64 gfn, u32 nr_entries)
{
}

static void
gzvm_handle_demand_page_post_handler(void *data, struct gzvm *vm, int memslot_id, u64 pfn, u64 gfn, u32 nr_entries)
{
	if (iommu_sync_callback)
		iommu_sync_callback();
	else
		pr_info("[%s] callback not registered!\n", __func__);
}

static void gzvm_destroy_vm_post_process_handler(void *data, struct gzvm *vm)
{
	if (iommu_sync_callback)
		iommu_sync_callback();
	else
		pr_info("[%s] callback not registered!\n", __func__);
}

static struct platform_driver gzvm_vendor_hooks = {
	.driver = {
		.name = "gzvm_vendor_hooks",
	},
};

static int __init gzvm_hooks_init(void)
{
	int ret;

	pr_info("[%s]: initializing\n", module_name(THIS_MODULE));

	ret = platform_driver_register(&gzvm_vendor_hooks);

	if (ret) {
		pr_info("[%s]: unable to register driver\n", __func__);
		return ret;
	}

	pr_info("[%s]: registering gzvm hooks\n", __func__);

	register_trace_android_vh_gzvm_handle_demand_page_pre(gzvm_handle_demand_page_pre_handler, NULL);
	register_trace_android_vh_gzvm_handle_demand_page_post(gzvm_handle_demand_page_post_handler, NULL);
	register_trace_android_vh_gzvm_destroy_vm_post_process(gzvm_destroy_vm_post_process_handler, NULL);
	return 0;
}

static void __exit gzvm_hooks_exit(void)
{
	pr_info("[%s]: going to exit\n", module_name(THIS_MODULE));

	platform_driver_unregister(&gzvm_vendor_hooks);
}

module_init(gzvm_hooks_init);
module_exit(gzvm_hooks_exit);

MODULE_AUTHOR("MediaTek");
MODULE_DESCRIPTION("GZVM Vendor Hooks");
MODULE_LICENSE("GPL");
