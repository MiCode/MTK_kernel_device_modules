// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/rpmsg.h>
#include <hds.h>
#include <hds_ipi_msg.h>
#include <hds_plat.h>
#include <apu_sysmem.h>

#define APU_HDS_BUFFER_MAX_SIZE (1*1024*1024)

struct apu_hds_async_ipi_msg {
	struct apu_hds_ipi_msg msg;
	struct list_head node;
};

static int apu_hds_sync_ipi_async(struct apu_hds_device *hdev, struct apu_hds_ipi_msg *msg)
{
	struct apu_hds_async_ipi_msg *a_msg = NULL;

	a_msg = kzalloc(sizeof(*a_msg), GFP_KERNEL);
	if (!a_msg)
		return -ENOMEM;

	apu_hds_debug("msg op: %u\n", msg->op);

	mutex_lock(&hdev->msg_mtx);
	memcpy(&a_msg->msg, msg, sizeof(*msg));
	list_add_tail(&a_msg->node, &hdev->msgs);
	schedule_work(&hdev->ipi_wk);
	mutex_unlock(&hdev->msg_mtx);

	return 0;
}

static int apu_hds_dev_power_on(struct rpmsg_endpoint *ept)
{
	int ret = 0;

	/* power on */
	ret = rpmsg_sendto(ept, NULL, 1, 0);
	if (ret && ret != -EOPNOTSUPP) {
		apu_hds_err("rpmsg_sendto(power on) fail(%d)\n", ret);
	} else {
		/* refcnt++ */
		mutex_lock(&g_hdev->power_mtx);
		g_hdev->power_cnt++;
		mutex_unlock(&g_hdev->power_mtx);
		ret = 0;
	}

	apu_hds_debug("ret(%d)\n", ret);

	return ret;
}

static int apu_hds_dev_power_off(struct rpmsg_endpoint *ept)
{
	int ret = 0;

	/* power off to restore ref cnt */
	ret = rpmsg_sendto(ept, NULL, 0, 1);
	if (ret && ret != -EOPNOTSUPP)
		apu_hds_err("rpmsg_sendto(power off) fail(%d)\n", ret);

	apu_hds_debug("ret(%d)\n", ret);

	return ret;
}

static int apu_hds_dev_handshake_query_info(struct apu_hds_device *hdev)
{
	struct apu_hds_ipi_msg ipi_msg = {};

	/* handshake with up */
	ipi_msg.op = APU_HDS_IPI_MSG_QUERY_INFO;
	apu_hds_sync_ipi_async(hdev, &ipi_msg);

	return 0;
}

static int apu_hds_dev_handshake_set_buffer(struct apu_hds_device *hdev)
{
	struct apu_hds_ipi_msg ipi_msg = {};
	int ret = 0;

	/* alloc work buffer */
	hdev->workbuf = hdev->allocator->alloc(hdev->allocator, APU_SYSMEM_TYPE_DRAM,
		hdev->init_workbuf_size, 0, "hds_workbuf");
	if (!hdev->workbuf) {
		apu_hds_err("alloc workbuf failed, size(%u)\n", hdev->init_workbuf_size);
		ret = -ENOMEM;
		goto out;
	}

	/* map work buffer */
	hdev->workmap = hdev->allocator->map(hdev->allocator, APU_SYSMEM_GET_DMABUF(hdev->workbuf),
		APU_SYSMEM_TYPE_DRAM, F_APU_SYSMEM_MAP_TYPE_DEVICE_SHAREVA);
	if (!hdev->workmap) {
		apu_hds_err("map workmap failed, size(%u)\n", hdev->init_workbuf_size);
		ret = -ENOMEM;
		goto free_workbuf;
	}

	/* send ipi msg to up */
	ipi_msg.op = APU_HDS_IPI_MSG_INIT;
	ipi_msg.init.init_workbuf_va = hdev->workmap->device_va;
	ipi_msg.init.init_workbuf_size = hdev->init_workbuf_size;
	apu_hds_sync_ipi_async(hdev, &ipi_msg);

	goto out;

free_workbuf:
	hdev->allocator->free(hdev->allocator, hdev->workbuf);
	hdev->workbuf = NULL;
out:
	return ret;
}

static int apu_hds_dev_ipi_cb(struct rpmsg_device *rpdev, void *data,
	int len, void *priv, u32 src)
{
	int ret = 0;
	struct apu_hds_device *hdev = (struct apu_hds_device *)priv;
	struct apu_hds_ipi_msg *ipi_msg = (struct apu_hds_ipi_msg *)data;

	if (len != sizeof(*ipi_msg)) {
		apu_hds_err("ipi msg size is not matched(%d/%lu)\n", len, sizeof(*ipi_msg));
		ret = -EINVAL;
		goto out;
	}

	apu_hds_debug("op(%u)\n", ipi_msg->op);

	switch (ipi_msg->op) {
	case APU_HDS_IPI_MSG_QUERY_INFO:
		/* check size boundary */
		if (ipi_msg->info.init_workbuf_size > APU_HDS_BUFFER_MAX_SIZE ||
			ipi_msg->info.per_cmd_appendix_size > APU_HDS_BUFFER_MAX_SIZE ||
			ipi_msg->info.per_subcmd_appendix_size > APU_HDS_BUFFER_MAX_SIZE) {
			apu_hds_err("invalid info size init(%u) cmd(%u) sc(%u)\n",
				ipi_msg->info.init_workbuf_size,
				ipi_msg->info.per_cmd_appendix_size,
				ipi_msg->info.per_subcmd_appendix_size);
			ret = -EINVAL;
		} else {
			hdev->version_hw = ipi_msg->info.version_hw;
			hdev->version_date = ipi_msg->info.version_date;
			hdev->version_revision = ipi_msg->info.version_revision;
			hdev->init_workbuf_size = ipi_msg->info.init_workbuf_size;
			hdev->per_cmd_appendix_size = ipi_msg->info.per_cmd_appendix_size;
			hdev->per_subcmd_appendix_size = ipi_msg->info.per_subcmd_appendix_size;
			apu_hds_info("hds info: ver(0x%x.0x%x.0x%x) size init(%u) cmd(%u) sc(%u)\n",
				hdev->version_hw,
				hdev->version_date,
				hdev->version_revision,
				hdev->init_workbuf_size,
				hdev->per_cmd_appendix_size,
				hdev->per_subcmd_appendix_size);

			/* get platform function by version */
			hdev->plat_func = hds_plat_get_funcs(hdev->version_hw,
				hdev->version_date, hdev->version_revision);
			if (hdev->plat_func == NULL) {
				apu_hds_err("no plat funcs for version(0x%x.0x%x.0x%x)\n",
					hdev->version_hw, hdev->version_date, hdev->version_revision);
				break;
			}

			if (hdev->plat_func->plat_init(hdev))
				apu_hds_err("plat version(0x%x.0x%x.0x%x) init failed\n",
					hdev->version_hw,
					hdev->version_date,
					hdev->version_revision);

			ret = apu_hds_dev_handshake_set_buffer(hdev);
			if (ret)
				apu_hds_err("handshake set buffer failed(%d)\n", ret);
		}
		break;
	case APU_HDS_IPI_MSG_INIT:
		/* init done */
		apu_hds_info("init done\n");
		hdev->inited = true;
		break;

	default:
		apu_hds_err("unknown msg op(%d)\n", ipi_msg->op);
		ret = -EINVAL;
		break;
	}

out:
	mutex_lock(&hdev->power_mtx);
	hdev->power_cnt--;
	mutex_unlock(&hdev->power_mtx);
	if (hdev->power_cnt == 0) {
		if (apu_hds_dev_power_off(hdev->ept)) {
			apu_hds_err("power off failed\n");
			ret = -EIO;
		}
	}
	apu_hds_debug("\n");

	return ret;
}

static void apu_hds_ipi_async_func(struct work_struct *wk)
{
	struct apu_hds_device *hdev = container_of(wk, struct apu_hds_device, ipi_wk);
	struct apu_hds_async_ipi_msg *a_msg = NULL;
	int ret = 0;

	mutex_lock(&hdev->msg_mtx);

	if (list_empty(&hdev->msgs)) {
		apu_hds_err("msg list is not empty\n");
		mutex_unlock(&hdev->msg_mtx);
		return;
	}

	a_msg = list_first_entry(&hdev->msgs, struct apu_hds_async_ipi_msg, node);
	list_del(&a_msg->node);

	mutex_unlock(&hdev->msg_mtx);

	apu_hds_debug("msg op: %u\n", a_msg->msg.op);

	/* power on */
	ret = apu_hds_dev_power_on(hdev->ept);
	if (ret) {
		apu_hds_err("power on failed(%d)\n", ret);
		goto free_msg;
	}

	/* send ipi msg to up */
	ret = rpmsg_send(hdev->ept, &a_msg->msg, sizeof(a_msg->msg));
	if (ret)
		apu_hds_err("send handshake failed(%d)\n", ret);

	apu_hds_debug("send done msg op: %u\n", a_msg->msg.op);

free_msg:
	kfree(a_msg);
}

int apu_hds_dev_init(struct apu_hds_device *hdev)
{
	struct rpmsg_channel_info chinfo = {};
	int ret = 0;

	apu_hds_debug("ret(%d)\n", ret);
	mutex_init(&hdev->power_mtx);
	mutex_init(&hdev->msg_mtx);

	hdev->pmu_lv = 1;
	hdev->pmu_tag_en = 0;

	/* create allocator */
	hdev->allocator = apu_sysmem_create_allocator(0xFFFFFFFD);
	if (!hdev->allocator) {
		apu_hds_err("create allocator failed\n");
		ret = -ENOMEM;
		goto out;
	}

	/* init wq */
	INIT_LIST_HEAD(&hdev->msgs);
	INIT_WORK(&hdev->ipi_wk, &apu_hds_ipi_async_func);

	/* create endpoint */
	strscpy(chinfo.name, hdev->rpdev->id.name, RPMSG_NAME_SIZE);
	chinfo.src = hdev->rpdev->src;
	chinfo.dst = RPMSG_ADDR_ANY;
	hdev->ept = rpmsg_create_ept(hdev->rpdev,
		apu_hds_dev_ipi_cb, hdev, chinfo);
	if (!hdev->ept)
		return -ENODEV;

	/* init default platform function */
	hdev->plat_func = hds_plat_get_funcs(0, 0, 0);
	if (hdev->plat_func == NULL) {
		apu_hds_err("no plat funcs for version(0x%x.0x%x.0x%x)\n",
			hdev->version_hw, hdev->version_date, hdev->version_revision);
		ret = -EINVAL;
		goto destroy_ept;
	}

	/* handshake for init */
	ret = apu_hds_dev_handshake_query_info(hdev);
	if (ret) {
		apu_hds_err("handshake failed(%d)\n", ret);
		goto destroy_ept;
	}

	goto out;

destroy_ept:
	rpmsg_destroy_ept(hdev->ept);
out:
	return ret;
}

void apu_hds_dev_deinit(struct apu_hds_device *hdev)
{
	/* destroy endpoint */
	rpmsg_destroy_ept(hdev->ept);

	/* free workbuf */
	if (!hdev->workbuf && !hdev->workmap) {
		hdev->allocator->unmap(hdev->allocator, hdev->workmap);
		hdev->allocator->free(hdev->allocator, hdev->workbuf);
	}
}
