// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include "mbraink_auto.h"
#include "mbraink_auto_hv.h"

#if IS_ENABLED(CONFIG_GRT_HYPERVISOR)
#include "mbraink_hypervisor_virtio.h"
#endif

static long handle_cpu_loading_info(const void *auto_ioctl_data, void *mbraink_auto_data)
{
	struct nbl_trace_buf_trans *cpu_loading_buf =
					(struct nbl_trace_buf_trans *)(mbraink_auto_data);
	long ret = 0;

	if (copy_from_user(cpu_loading_buf, (struct nbl_trace_buf_trans *)auto_ioctl_data,
			sizeof(struct nbl_trace_buf_trans))) {
		pr_notice("copy nbl_trace_buf_trans data from user Err!\n");
		return -EPERM;
	}

	if (cpu_loading_buf != NULL) {
		switch (cpu_loading_buf->trans_type) {
		case 0:
		{
			ret = mbraink_auto_set_vcpu_record(0);
			break;
		}
		case 1:
		{
			ret = mbraink_auto_set_vcpu_record(1);
			break;
		}
		case 2:
		{
			if (cpu_loading_buf->length == 0) {
				pr_notice("length is 0. no need do anything\n");
			} else {
				void *vcpu_buffer = vmalloc(cpu_loading_buf->length *
								sizeof(struct trace_vcpu_rec));

				if (vcpu_buffer == NULL)
					return -ENOMEM;
				ret = mbraink_auto_get_vcpu_record(cpu_loading_buf, vcpu_buffer);

				if (copy_to_user((struct mbraink_auto_ioctl_info *)auto_ioctl_data,
						cpu_loading_buf,
						sizeof(struct mbraink_auto_ioctl_info))) {
					pr_notice("Copy mbraink_auto_ioctl_buf to user error!\n");
					vfree(vcpu_buffer);
					return -EPERM;
				}
				if (copy_to_user(cpu_loading_buf->vcpu_data,
						vcpu_buffer,
						cpu_loading_buf->length *
						sizeof(struct trace_vcpu_rec))) {
					pr_notice("Copy vcpu_data to user error!\n");
					vfree(vcpu_buffer);
					return -EPERM;
				}

				vfree(vcpu_buffer);
			}
			break;
		}
		default:
		{
			pr_info("unknown vcpu type %d\n", cpu_loading_buf->trans_type);
			ret = -1;
		}
		}
	}
	return ret;
}

long mbraink_auto_ioctl(unsigned long arg, void *mbraink_data)
{
	long ret = 0;
	void *mbraink_auto_data = NULL;
	struct mbraink_auto_ioctl_info *auto_ioctl_buf =
		(struct mbraink_auto_ioctl_info *)(mbraink_data);

	if (copy_from_user(auto_ioctl_buf, (struct mbraink_auto_ioctl_info *) arg,
			sizeof(struct mbraink_auto_ioctl_info))) {
		pr_notice("copy mbraink_auto_ioctl_info data from user Err!\n");
		return -EPERM;
	}

	switch (auto_ioctl_buf->auto_ioctl_type) {
	case AUTO_CPULOAD_INFO:
	{
		mbraink_auto_data = kmalloc(sizeof(struct nbl_trace_buf_trans), GFP_KERNEL);
		if (!mbraink_auto_data)
			goto End;
		ret = handle_cpu_loading_info(auto_ioctl_buf->auto_ioctl_data, mbraink_auto_data);
		kfree(mbraink_auto_data);
		break;
	}
	default:
		pr_notice("%s:illegal ioctl number %u.\n", __func__,
			auto_ioctl_buf->auto_ioctl_type);
		return -EINVAL;
	}

	return ret;
End:
	pr_info("%s: kmalloc failed\n", __func__);
	return -ENOMEM;
}

int mbraink_auto_init(void)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_GRT_HYPERVISOR)
	ret = vhost_mbraink_init();
	if (ret)
		pr_notice("mbraink virtio init failed.\n");
#endif

	ret = mbraink_auto_cpuload_init();

	return ret;
}

void mbraink_auto_deinit(void)
{
#if IS_ENABLED(CONFIG_GRT_HYPERVISOR)
	//vhost_mbraink_deinit();
#endif
	mbraink_auto_cpuload_deinit();
}
