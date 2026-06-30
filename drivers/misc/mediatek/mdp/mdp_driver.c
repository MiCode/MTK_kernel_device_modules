// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include "mdp_ioctl_ex.h"
#include "mdp_driver.h"
#include "cmdq_struct.h"
#include "cmdq_virtual.h"
#include "cmdq_reg.h"
#include "mdp_common.h"
#include "cmdq_device.h"

#include "cmdq_helper_ext.h"
#include "cmdq_record.h"
#include "cmdq_device.h"

#ifdef CMDQ_SECURE_PATH_SUPPORT
#include <cmdq-sec.h>
#endif

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/sched/clock.h>
#include <linux/vmalloc.h>
#ifdef CMDQ_USE_LEGACY
#include <mach/mt_boot.h>
#endif

/*
 * @device tree porting note
 * alps/kernel-3.10/arch/arm64/boot/dts/{platform}.dts
 *  - use of_device_id to match driver and device
 *  - use io_map to map and get VA of HW's rgister
 */
static const struct of_device_id cmdq_of_ids[] = {
	{.compatible = "mediatek,mdp",},
	{}
};

static dev_t gMdpDevNo;
static struct cdev *gMdpCDev;
static struct class *gMDPClass;

static int cmdq_open(struct inode *pInode, struct file *pFile)
{
	struct cmdqFileNodeStruct *pNode;

	CMDQ_VERBOSE("CMDQ driver open fd=%p begin\n", pFile);

	pFile->private_data = kzalloc(sizeof(struct cmdqFileNodeStruct),
		GFP_KERNEL);
	if (!pFile->private_data) {
		CMDQ_ERR("Can't allocate memory for CMDQ file node\n");
		return -ENOMEM;
	}

	pNode = (struct cmdqFileNodeStruct *)pFile->private_data;
	pNode->userPID = current->pid;
	pNode->userTGID = current->tgid;

	INIT_LIST_HEAD(&(pNode->taskList));
	spin_lock_init(&pNode->nodeLock);

	CMDQ_VERBOSE("CMDQ driver open end\n");

	return 0;
}


static int cmdq_release(struct inode *pInode, struct file *pFile)
{
	struct cmdqFileNodeStruct *pNode;
	unsigned long flags;

	CMDQ_LOG("CMDQ driver release fd=%p begin\n", pFile);

	pNode = (struct cmdqFileNodeStruct *)pFile->private_data;

	if (!pNode) {
		CMDQ_ERR("CMDQ file node NULL\n");
		return -EFAULT;
	}

	spin_lock_irqsave(&pNode->nodeLock, flags);

	/* note that we did not release CMDQ tasks
	 * issued by this file node,
	 * since their HW operation may be pending.
	 */

	spin_unlock_irqrestore(&pNode->nodeLock, flags);

	/* release by mapping job */
	mdp_ioctl_free_job_by_node(pNode);

	/* scan through tasks that created by
	 * this file node and release them
	 */
	cmdq_mdp_release_task_by_file_node((void *)pNode);

	kfree(pFile->private_data);
	pFile->private_data = NULL;

	mdp_ioctl_free_readback_slots_by_node(pFile);

	if (!cmdq_mdp_vcp_pq_readback_support())
		cmdqCoreFreeWriteAddressByNode(pFile, CMDQ_CLT_MDP);
	else
		cmdqCoreWriteAddressVcpFreeByNode(pFile, CMDQ_CLT_MDP);

	CMDQ_LOG("CMDQ driver release end\n");

	return 0;
}

void cmdq_driver_dump_readback(dma_addr_t *addrs, u32 count, u32 *values)
{
	u32 i, n, len, cur;
	char buf[72];
	int ret;

	if (likely(!cmdq_core_profile_pqreadback_enabled() &&
		!cmdq_core_profile_pqreadback_once_enabled()))
		return;

	CMDQ_LOG("read back dump begin ...\n");

	i = 0;
	while (i < count) {
		ret = snprintf(buf, sizeof(buf), "%pa:", &addrs[i]);
		if (ret < 0)
			CMDQ_ERR("%s snprintf failed!!!\n", __func__);
		else
			len = ret;

		cur = addrs[i] & 0xFFFFFFF0;

		/* limit max num 4 in line */
		for (n = 0; n < 4 && i < count &&
			cur == (addrs[i] & 0xFFFFFFF0); n++) {
			ret = snprintf(buf + len, sizeof(buf) - len,
				" %#010x", values[i]);
			if (ret < 0)
				CMDQ_ERR("%s snprintf failed!!!\n", __func__);
			else
				len += ret;

			i++;
		}

		CMDQ_LOG("%s\n", buf);
	}

	CMDQ_LOG("read back dump end\n");
}


s32 cmdq_driver_ioctl_query_usage(struct file *pf, unsigned long param)
{
	int count[CMDQ_MAX_ENGINE_COUNT] = {0};

	if (cmdq_mdp_query_usage(count))
		return -EFAULT;

	if (copy_to_user((void *)param, count, sizeof(s32) *
		CMDQ_MAX_ENGINE_COUNT)) {
		CMDQ_ERR("CMDQ_IOCTL_QUERY_USAGE copy_to_user failed\n");
		return -EFAULT;
	}

	return 0;
}

s32 cmdq_driver_ioctl_query_cap_bits(unsigned long param)
{
	int capBits = 0;

	/* support wait and receive event in same tick */
	capBits |= (1L << CMDQ_CAP_WFE);

	if (copy_to_user((void *)param, &capBits, sizeof(int))) {
		CMDQ_ERR("Copy capacity bits to user space failed\n");
		return -EFAULT;
	}

	return 0;
}

s32 cmdq_driver_ioctl_query_dts(unsigned long param)
{
	struct cmdqDTSDataStruct *dts;

	dts = cmdq_core_get_dts_data();

	if (copy_to_user((void *)param, dts, sizeof(*dts))) {
		CMDQ_ERR("Copy dts to user space failed\n");
		return -EFAULT;
	}

	return 0;
}

s32 cmdq_driver_ioctl_notify_engine(unsigned long param)
{
	u64 engineFlag;

	if (copy_from_user(&engineFlag, (void *)param, sizeof(u64))) {
		CMDQ_ERR("%s copy_from_user failed\n", __func__);
		return -EFAULT;
	}
	cmdq_mdp_lock_resource(engineFlag, true);

	return 0;
}

static long cmdq_ioctl(struct file *pf, unsigned int code,
	unsigned long param)
{
	s32 status = 0;

	CMDQ_VERBOSE("%s code:0x%08x f:0x%p\n", __func__, code, pf);

	switch (code) {
	case CMDQ_IOCTL_QUERY_USAGE:
		status = cmdq_driver_ioctl_query_usage(pf, param);
		break;
	case CMDQ_IOCTL_QUERY_CAP_BITS:
		status = cmdq_driver_ioctl_query_cap_bits(param);
		break;
	case CMDQ_IOCTL_QUERY_DTS:
		status = cmdq_driver_ioctl_query_dts(param);
		break;
	case CMDQ_IOCTL_NOTIFY_ENGINE:
		status = cmdq_driver_ioctl_notify_engine(param);
		break;
	case CMDQ_IOCTL_ASYNC_EXEC:
		CMDQ_MSG("ioctl CMDQ_IOCTL_ASYNC_EXEC\n");
		status = mdp_ioctl_async_exec(pf, param);
		break;
	case CMDQ_IOCTL_ASYNC_WAIT:
		CMDQ_MSG("ioctl CMDQ_IOCTL_ASYNC_WAIT\n");
		status = mdp_ioctl_async_wait(param);
		break;
	case CMDQ_IOCTL_ALLOC_READBACK_SLOTS:
		CMDQ_MSG("ioctl CMDQ_IOCTL_ALLOC_READBACK_SLOTS\n");
		status = mdp_ioctl_alloc_readback_slots(pf, param);
		break;
	case CMDQ_IOCTL_FREE_READBACK_SLOTS:
		CMDQ_MSG("ioctl CMDQ_IOCTL_FREE_READBACK_SLOTS\n");
		status = mdp_ioctl_free_readback_slots(pf, param);
		break;
	case CMDQ_IOCTL_READ_READBACK_SLOTS:
		CMDQ_MSG("ioctl CMDQ_IOCTL_READ_READBACK_SLOTS\n");
		status = mdp_ioctl_read_readback_slots(param);
		break;
#ifdef MDP_COMMAND_SIMULATE
	case CMDQ_IOCTL_SIMULATE:
		CMDQ_LOG("ioctl CMDQ_IOCTL_SIMULATE\n");
		status = mdp_ioctl_simulate(param);
		break;
#endif
	default:
		CMDQ_ERR("unrecognized ioctl 0x%08x\n", code);
		return -ENOIOCTLCMD;
	}

	if (status < 0)
		CMDQ_ERR("ioctl return fail:%d\n", status);

	return status;
}

#ifdef CONFIG_COMPAT
static long cmdq_ioctl_compat(struct file *pFile, unsigned int code,
	unsigned long param)
{
	switch (code) {
	case CMDQ_IOCTL_QUERY_USAGE:
	case CMDQ_IOCTL_EXEC_COMMAND:
	case CMDQ_IOCTL_ASYNC_JOB_EXEC:
	case CMDQ_IOCTL_ASYNC_JOB_WAIT_AND_CLOSE:
	case CMDQ_IOCTL_ALLOC_WRITE_ADDRESS:
	case CMDQ_IOCTL_FREE_WRITE_ADDRESS:
	case CMDQ_IOCTL_READ_ADDRESS_VALUE:
	case CMDQ_IOCTL_QUERY_CAP_BITS:
	case CMDQ_IOCTL_QUERY_DTS:
	case CMDQ_IOCTL_NOTIFY_ENGINE:
	case CMDQ_IOCTL_ASYNC_EXEC:
	case CMDQ_IOCTL_ASYNC_WAIT:
	case CMDQ_IOCTL_ALLOC_READBACK_SLOTS:
	case CMDQ_IOCTL_FREE_READBACK_SLOTS:
	case CMDQ_IOCTL_READ_READBACK_SLOTS:
	case CMDQ_IOCTL_SIMULATE:
		/* All ioctl structures should be the same size in
		 * 32-bit and 64-bit linux.
		 */
		return cmdq_ioctl(pFile, code, param);
	case CMDQ_IOCTL_LOCK_MUTEX:
	case CMDQ_IOCTL_UNLOCK_MUTEX:
		CMDQ_ERR("[COMPAT]deprecated ioctl 0x%08x\n", code);
		return -ENOIOCTLCMD;
	default:
		CMDQ_ERR("[COMPAT]unrecognized ioctl 0x%08x\n", code);
		return -ENOIOCTLCMD;
	}

	CMDQ_ERR("[COMPAT]unrecognized ioctl 0x%08x\n", code);
	return -ENOIOCTLCMD;
}
#endif


static const struct file_operations mdpOP = {
	.owner = THIS_MODULE,
	.open = cmdq_open,
	.release = cmdq_release,
	.unlocked_ioctl = cmdq_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = cmdq_ioctl_compat,
#endif
};

static int cmdq_pm_notifier_cb(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:	/* Going to suspend the system */
		/* The next stage is freeze process. */
		/* We will queue all request in suspend callback, */
		/* so don't care this stage */
		return NOTIFY_DONE;	/* don't care this event */
	case PM_POST_SUSPEND:
		/* processes had resumed in previous stage
		 * (system resume callback)
		 * resume CMDQ driver to execute.
		 */
		cmdq_core_resume_notifier();
		return NOTIFY_OK;	/* process done */
	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_DONE;
}

/* Hibernation and suspend events */
static struct notifier_block cmdq_pm_notifier_block = {
	.notifier_call = cmdq_pm_notifier_cb,
	.priority = 5,
};

void mdp_mme_init(void)
{
#if IS_ENABLED(CONFIG_MTK_CMDQ_DEBUG) && IS_ENABLED(CONFIG_MTK_MME_SUPPORT)
	MME_REGISTER_BUFFER(MME_MODULE_MMSYS, "MDP", MME_BUFFER_INDEX_0, MDP_LOG_SIZE);
#endif
}

static int cmdq_probe(struct platform_device *pDevice)
{
	int status;
	struct device *object;

	/* mdp mme log init */
	mdp_mme_init();

	CMDQ_LOG("[MDP] MDP driver probe begin\n");

	/* Function link */
	cmdq_virtual_function_setting();

	/* init cmdq device related data */
	cmdq_dev_init(pDevice);

	/* init cmdq context */
	cmdq_core_initialize();

	/* init cmdq context */
	CMDQ_LOG("call cmdq_mdp_init\n");
	cmdq_mdp_init(pDevice);

	status = alloc_chrdev_region(&gMdpDevNo, 0, 1,
		MDP_DRIVER_DEVICE_NAME);
	if (status != 0) {
		/* Cannot get MDP device major number */
		CMDQ_ERR("Get MDP device major number(%d) failed(%d)\n",
			gMdpDevNo, status);
	} else {
		/* Get MDP device major number successfully */
		CMDQ_MSG("Get MDP device major number(%d) success(%d)\n",
			gMdpDevNo, status);
	}

	/* ioctl access point (/dev/mtk_mdp) */
	gMdpCDev = cdev_alloc();
	gMdpCDev->owner = THIS_MODULE;
	gMdpCDev->ops = &mdpOP;

	status = cdev_add(gMdpCDev, gMdpDevNo, 1);

	gMDPClass = class_create(MDP_DRIVER_DEVICE_NAME);
	object = device_create(gMDPClass, NULL, gMdpDevNo, NULL,
		MDP_DRIVER_DEVICE_NAME);
	if (IS_ERR(object)) {
		CMDQ_ERR("Failed to create device %s(%pe)\n",
			MDP_DRIVER_DEVICE_NAME, object);
		return PTR_ERR(object);
	}

	/* mtk-cmdq-mailbox will register the irq */

	mdp_limit_dev_create(pDevice);

	/* Register PMQoS */
	cmdq_core_register_task_cycle_cb(cmdq_mdp_get_func()->getGroupMdp(),
		cmdq_mdp_get_func()->beginTask,
		cmdq_mdp_get_func()->endTask);

	if (cmdq_mdp_get_func()->mdpIsCaminSupport()) {
		cmdq_core_register_task_cycle_cb(cmdq_mdp_get_func()->getGroupIsp(),
			cmdq_mdp_get_func()->beginISPTask,
			cmdq_mdp_get_func()->endISPTask);
	}

	/* register pm notifier */
	status = register_pm_notifier(&cmdq_pm_notifier_block);
	if (status != 0) {
		CMDQ_ERR("Failed to register_pm_notifier(%d)\n", status);
		return -ENODEV;
	}

	CMDQ_LOG("MDP driver probe end\n");

	return 0;
}


static void cmdq_remove(struct platform_device *pDevice)
{
	cmdq_core_remove();
	disable_irq(cmdq_dev_get_irq_id());
}


static int cmdq_suspend(struct device *pDevice)
{
	CMDQ_LOG("%s ignore\n", __func__);
	return cmdq_core_suspend();
}

static int cmdq_resume(struct device *pDevice)
{
	CMDQ_LOG("%s ignore\n", __func__);
	return cmdq_core_resume();
}

static int cmdq_pm_restore_noirq(struct device *pDevice)
{
	return 0;
}

static const struct dev_pm_ops cmdq_pm_ops = {
	.suspend = cmdq_suspend,
	.resume = cmdq_resume,
	.freeze = NULL,
	.thaw = NULL,
	.poweroff = NULL,
	.restore = NULL,
	.restore_noirq = cmdq_pm_restore_noirq,
};


static struct platform_driver gCmdqDriver = {
	.probe = cmdq_probe,
	.remove = cmdq_remove,
	.driver = {
		.name = MDP_DRIVER_DEVICE_NAME,
		.owner = THIS_MODULE,
		.pm = &cmdq_pm_ops,
		.of_match_table = cmdq_of_ids,
	}
};

static int __init cmdq_init(void)
{
	int status;

	CMDQ_LOG("%s MDP driver init begin\n", __func__);

	/* MDP function link */
	cmdq_mdp_virtual_function_setting();
	cmdq_mdp_platform_function_setting();

	status = platform_driver_register(&gCmdqDriver);
	if (status != 0) {
		CMDQ_ERR("Failed to register the CMDQ driver(%d)\n", status);
		return -ENODEV;
	}

	mdpsyscon_init();
	cmdq_core_late_init();
	status = mdp_sync_device_init();
	if (status != 0)
		CMDQ_ERR("fence init fail:%d\n", status);

	CMDQ_LOG("MDP driver init end\n");

	return 0;
}

static void __exit cmdq_exit(void)
{
	s32 status;

	CMDQ_LOG("MDP driver exit begin\n");

	device_destroy(gMDPClass, gMdpDevNo);

	class_destroy(gMDPClass);

	cdev_del(gMdpCDev);

	gMdpCDev = NULL;

	unregister_chrdev_region(gMdpDevNo, 1);

	platform_driver_unregister(&gCmdqDriver);

	/* register pm notifier */
	status = unregister_pm_notifier(&cmdq_pm_notifier_block);
	if (status != 0) {
		/* Failed to unregister_pm_notifier */
		CMDQ_ERR("Failed to unregister_pm_notifier(%d)\n", status);
	}

	/* Unregister MDP callback */
	cmdqCoreRegisterCB(cmdq_mdp_get_func()->getGroupMdp(),
		NULL, NULL, NULL, NULL);

	/* De-Initialize group callback */
	cmdq_core_deinit_group_cb();

	/* De-Initialize cmdq core */
	cmdq_core_deinitialize();

	/* De-Initialize cmdq dev related data */
	cmdq_dev_deinit();
	mdpsyscon_deinit();

	CMDQ_LOG("MDP driver exit end\n");
}

module_init(cmdq_init);
module_exit(cmdq_exit);

MODULE_DESCRIPTION("MTK CMDQ driver");
MODULE_AUTHOR("Pablo<pablo.sun@mediatek.com>");
MODULE_LICENSE("GPL");
