// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/rpmsg.h>
#include "mdw_cmn.h"
#include "mdw_trace.h"
#include "mdw_rv.h"
#include "mdw_rv_msg.h"
#include "mdw_rv_tag.h"
#define CREATE_TRACE_POINTS
#include "mdw_rv_events.h"
#include <linux/sched/clock.h>

#define MDW_CMD_IPI_TIMEOUT (10*1000) //ms
#define MDW_RV_DEV_MSG_IDR_MIN (1)
#define MDW_RV_DEV_MSG_IDR_MAX (65536)

static int mdw_rv_dev_msg_insert(struct mdw_rv_dev *mrdev,
	struct mdw_ipi_msg_sync *s_msg)
{
	if (s_msg->msg.sync_id >= MDW_RV_DEV_MSG_IDR_MIN &&
		s_msg->msg.sync_id <= MDW_RV_DEV_MSG_IDR_MAX) {
		mdw_drv_err("mdw rv insert idr error, exist already(0x%llu)\n", s_msg->msg.sync_id);
		return -EINVAL;
	}

	s_msg->msg.sync_id = idr_alloc_cyclic(&mrdev->msg_idr, s_msg,
		MDW_RV_DEV_MSG_IDR_MIN, MDW_RV_DEV_MSG_IDR_MAX, GFP_KERNEL);
	if (s_msg->msg.sync_id < MDW_RV_DEV_MSG_IDR_MIN) {
		mdw_drv_err("mdw_rv alloc idr fail\n");
		return -EUSERS;
	}

	mdw_drv_debug("insert msg(%pK/%pK) sync id(%llu)\n", s_msg, &s_msg->msg.sync_id, s_msg->msg.sync_id);

	return 0;
}

static struct mdw_ipi_msg_sync *mdw_rv_dev_msg_find_erase(struct mdw_rv_dev *mrdev,
	uint64_t sync_id)
{
	struct mdw_ipi_msg_sync *s_msg = NULL;

	mdw_drv_debug("remove msg sync_id(%llu)\n", sync_id);
	s_msg = idr_remove(&mrdev->msg_idr, sync_id);
	if (!s_msg) {
		mdw_drv_err("can't find msg sync_id(0x%llx)\n", sync_id);
		mdw_exception("can't find msg sync_id\n");
	} else if (s_msg->msg.sync_id != sync_id) {
		mdw_drv_err("msg sync_id(%llu/%llu) confuse\n", s_msg->msg.sync_id, sync_id);
		mdw_exception("msg sync_id confuse\n");
	}

	return s_msg;
}

static void mdw_rv_dev_power_off(struct work_struct *wk)
{
	struct mdw_rv_dev *mrdev =
		container_of(wk, struct mdw_rv_dev, power_off_wk);
	int ret = 0;

	mdw_drv_debug("worker call power off\n");
	ret = mdw_rv_dev_power_onoff(mrdev, MDW_APU_POWER_OFF);  // power off
	if (ret && ret != -EOPNOTSUPP)
		mdw_drv_err("rpmsg_sendto(power off) fail(%d)\n", ret);
}

static void mdw_rv_dev_timer_callback(struct timer_list *timer)
{
	struct mdw_rv_dev *mrdev =
		container_of(timer, struct mdw_rv_dev, power_off_timer);

	mdw_flw_debug("dtime timer up\n");
	schedule_work(&mrdev->power_off_wk);
}

int mdw_rv_dev_dtime_handle(struct mdw_rv_dev *mrdev, struct mdw_cmd *c)
{
	struct mdw_device *mdev = c->mpriv->mdev;
	uint64_t curr_dtime_ts = 0;
	unsigned long power_dtime = 0;
	uint32_t activate_dtime = c->power_dtime;

	mdw_flw_debug("\n");

	/* dtime handle */
	activate_dtime = (activate_dtime > dbg_max_dtime)? dbg_max_dtime: activate_dtime;
	activate_dtime = (activate_dtime < dbg_min_dtime)? dbg_min_dtime: activate_dtime;

	mutex_lock(&mdev->dtime_mtx);
	curr_dtime_ts = c->end_ts + activate_dtime;
	if (mdev->max_dtime_ts < curr_dtime_ts) {
		mdev->max_dtime_ts = curr_dtime_ts;
	} else {
		mdw_drv_warn("skip create power off dtime timer\n");
		goto out;
	}

	power_dtime = msecs_to_jiffies(mdev->max_dtime_ts - c->end_ts);

	/* if old timer exist then delete */
	if (timer_pending(&mrdev->power_off_timer))
		del_timer(&mrdev->power_off_timer);

	/* timer power off according to power_dtime */
	timer_setup(&mrdev->power_off_timer, mdw_rv_dev_timer_callback, 0);
	mod_timer(&mrdev->power_off_timer, jiffies + power_dtime);

out:
	mutex_unlock(&mdev->dtime_mtx);
	return 0;
}

/**
 * mdw_rv_dev_power_onoff() - Use rpmsg_sendto to power on or off
 * @mrdev: the struct mdw_rv_dev
 * @power_onoff: true: power on , false: power off
 *
 * Return: Both of 0 and -EOPNOTSUPP on success.
 */
int mdw_rv_dev_power_onoff(struct mdw_rv_dev *mrdev, enum mdw_power_type power_onoff)
{
	struct mdw_device *mdev = mrdev->mdev;
	uint32_t cnt = 100, i = 0, dst = 0;
	int ret = 0, len = 0, cmd_running = 0;
	enum mdw_power_type power_flag = MDW_APU_POWER_OFF;

	atomic_inc(&mdev->pwr_usage);
	mutex_lock(&mdev->power_mtx);
	if (mdev->support_power_fast_on_off == false) {
		mdw_drv_debug("platform does NOT support fast off/on\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	/* set param */
	if (power_onoff == MDW_APU_POWER_ON) {
		mdw_drv_debug("fast power on\n");
		len = 1;
	} else {
		dst = 1;
		power_flag = MDW_APU_POWER_ON;
		mdw_drv_debug("fast power off\n");
	}

	/* check cmd_running */
	if (power_onoff == MDW_APU_POWER_OFF) {
		cmd_running  = atomic_read(&mdev->cmd_running);
		if (cmd_running) {
			mdw_drv_debug("cmd_running, skip power off\n");
			goto out;
		}
	}

	/* send & retry */
	for (i = 0; i < cnt; i++) {
		/* power api */
		if (mdev->power_state == power_flag) {
			ret = rpmsg_sendto(mrdev->ept, NULL, len, dst);
			if (ret && ret != -EOPNOTSUPP)
				mdw_drv_info("rpmsg_sendto(power) fail(%d)\n", ret);
			else
				mdev->power_state = !power_flag;
		} else {
			mdw_flw_debug("skip power action power_state(%d)\n", power_flag);
		}

		/* send busy, retry */
		if (ret == -EBUSY) {
			if (!(i % 10))
				mdw_drv_info("re-send power ipi (%u/%u)\n", i, cnt);

			usleep_range(10000, 11000);
			continue;
		}
		break;
	}

	if (ret == -EOPNOTSUPP) {
		mdw_drv_debug("No support fast power on/off\n");
		mdev->support_power_fast_on_off = false;
		mdev->power_state = MDW_APU_POWER_OFF;
	}

out:
	mutex_unlock(&mdev->power_mtx);
	atomic_dec(&mdev->pwr_usage);
	return ret;
}

static int mdw_rv_dev_send_msg(struct mdw_rv_dev *mrdev, struct mdw_ipi_msg_sync *s_msg)
{
	int ret = 0, power_ret = 0;
	uint32_t cnt = 100, i = 0;

	mdw_drv_debug("sync id(0x%llu) (0x%llx/%u) msg_size(%lu)\n",
		s_msg->msg.sync_id, s_msg->msg.c.iova, s_msg->msg.c.size, sizeof(s_msg->msg));

	ret = mdw_rv_dev_power_onoff(mrdev, MDW_APU_POWER_ON);  // power on
	if (ret && ret != -EOPNOTSUPP) {
		mdw_drv_err("rpmsg_sendto(power on) fail(%d)\n", ret);
		goto out;
	}

	/* insert to msg list */
	mutex_lock(&mrdev->msg_mtx);
	mdw_rv_dev_msg_insert(mrdev, s_msg);
	mutex_unlock(&mrdev->msg_mtx);

	atomic_inc(&mrdev->mdev->ipi_usage);
	/* send & retry */
	for (i = 0; i < cnt; i++) {
		mdw_trace_begin("apumdw:send_ipi|msg:0x%llu", s_msg->msg.sync_id);
		ret = rpmsg_send(mrdev->ept, &s_msg->msg, sizeof(s_msg->msg));
		mdw_trace_end();

		/* send busy, retry */
		if (ret == -EBUSY || ret == -EAGAIN) {
			if (!(i % 10))
				mdw_drv_info("re-send ipi(%u/%u) ret(%d)\n", i, cnt, ret);
			if (ret == -EAGAIN && i < 10)
				usleep_range(200, 500);
			else if (ret == -EAGAIN && i < 50)
				usleep_range(1000, 2000);
			else
				usleep_range(10000, 11000);
			continue;
		}
		break;
	}

	/* send ipi fail, remove msg from list */
	if (ret) {
		mdw_drv_err("send ipi msg(0x%llu) fail(%d)\n",
			s_msg->msg.sync_id, ret);

		power_ret = mdw_rv_dev_power_onoff(mrdev, MDW_APU_POWER_OFF);  // power off
		if (power_ret && power_ret != -EOPNOTSUPP) {
			mdw_drv_err("rpmsg_sendto(power off) fail(%d)\n", power_ret);
			ret = power_ret;
			goto out;
		}

		mutex_lock(&mrdev->msg_mtx);
		if (mdw_rv_dev_msg_find_erase(mrdev, s_msg->msg.sync_id) == s_msg) {
			mdw_drv_warn("remove ipi msg(0x%llu)\n", s_msg->msg.sync_id);
			s_msg->msg.sync_id = 0;
		} else
			mdw_drv_err("can't find ipi msg(0x%llu)\n", s_msg->msg.sync_id);

		mutex_unlock(&mrdev->msg_mtx);
	}
out:
	atomic_dec(&mrdev->mdev->ipi_usage);
	return ret;
}

static void mdw_rv_ipi_cmplt_sync(struct mdw_ipi_msg_sync *s_msg)
{
	mdw_flw_debug("\n");
	complete(&s_msg->cmplt);
}

static int mdw_rv_dev_send_sync(struct mdw_rv_dev *mrdev, struct mdw_ipi_msg *msg)
{
	int ret = 0, power_ret = 0;
	struct mdw_ipi_msg_sync *s_msg = NULL;
	unsigned long timeout = msecs_to_jiffies(MDW_CMD_IPI_TIMEOUT);

	s_msg = kzalloc(sizeof(*s_msg), GFP_KERNEL);
	if (!s_msg)
		return -ENOMEM;

	memcpy(&s_msg->msg, msg, sizeof(*msg));
	init_completion(&s_msg->cmplt);
	s_msg->complete = mdw_rv_ipi_cmplt_sync;

	mutex_lock(&mrdev->mtx);
	/* send */
	ret = mdw_rv_dev_send_msg(mrdev, s_msg);
	if (ret) {
		mdw_drv_err("send msg fail\n");
		goto fail_send_sync;
	}
	mutex_unlock(&mrdev->mtx);

	/* wait for response */
	if (!wait_for_completion_timeout(&s_msg->cmplt, timeout)) {
		mdw_drv_err("ipi no response\n");

		mutex_lock(&mrdev->msg_mtx);
		if (mdw_rv_dev_msg_find_erase(mrdev, s_msg->msg.sync_id) == s_msg) {
			mdw_drv_warn("remove ipi msg(0x%llu)\n", s_msg->msg.sync_id);
			s_msg->msg.sync_id = 0;
		} else
			mdw_drv_err("can't find ipi msg(0x%llu)\n",s_msg->msg.sync_id);
		mutex_unlock(&mrdev->msg_mtx);

		ret = -ETIME;
	} else {
		memcpy(msg, &s_msg->msg, sizeof(*msg));
		ret = msg->ret;
		if (ret)
			mdw_drv_warn("up return fail(%d)\n", ret);
	}

	power_ret = mdw_rv_dev_power_onoff(mrdev, MDW_APU_POWER_OFF);  // power off
	if (power_ret && power_ret != -EOPNOTSUPP)
		mdw_drv_err("rpmsg_sendto(power off) fail(%d)\n", power_ret);

	goto out;

fail_send_sync:
	mutex_unlock(&mrdev->mtx);
out:
	kfree(s_msg);

	return ret;
}

static void mdw_rv_ipi_cmplt_cmd(struct mdw_ipi_msg_sync *s_msg)
{
	int ret = 0;
	struct mdw_rv_cmd *rc =
		container_of(s_msg, struct mdw_rv_cmd, s_msg);
	struct mdw_cmd *c = rc->c;
	struct mdw_rv_dev *mrdev = (struct mdw_rv_dev *)c->mpriv->mdev->dev_specific;

	switch (s_msg->msg.ret) {
	case MDW_IPI_MSG_STATUS_BUSY:
		ret = -EBUSY;
		mdw_drv_warn("uP mdw busy, sync_id(0x%llx) inf_id(0x%llx)\n",
			s_msg->msg.sync_id, c->inference_id);
		break;

	case MDW_IPI_MSG_STATUS_ERR:
		ret = -EREMOTEIO;
		mdw_drv_err("uP mdw error, sync_id(0x%llx) inf_id(0x%llx)\n",
			s_msg->msg.sync_id, c->inference_id);
		mdw_rv_exception("uP mdw error\n");
		break;

	case MDW_IPI_MSG_STATUS_TIMEOUT:
		ret = -ETIME;
		break;

	default:
		break;
	}
	if (ret)
		mdw_drv_err("s(0x%llx) c(0x%llx) ret(%d/0x%llx) time(%llu) pid(%d/%d)\n",
			(uint64_t)c->mpriv, c->kid, ret, c->einfos->c.sc_rets,
			c->einfos->c.total_us, c->pid, c->tgid);
	c->enter_rv_cb_time = mrdev->enter_rv_cb_time;
	c->rv_cb_time = mrdev->rv_cb_time;
	s_msg->msg.sync_id = 0;
	mdw_cmd_trace(rc->c, MDW_CMD_DONE);

	c->complete(c, ret);
}

int mdw_rv_dev_run_cmd(struct mdw_fpriv *mpriv, struct mdw_rv_cmd *rc)
{
	int ret = 0;
	struct mdw_rv_dev *mrdev = (struct mdw_rv_dev *)mpriv->mdev->dev_specific;

	mdw_trace_begin("apumdw:dev_run|cmd:uid(0x%llx) inf_id(0x%08x,0x%08x)",
			rc->c->uid,
			(uint32_t)(rc->c->inference_id >> 32),
			(uint32_t)(rc->c->inference_id & 0xFFFFFFFF));

	mdw_drv_debug("pid(%d) run cmd(0x%llx/0x%llx) dva(0x%llx) size(%llu)\n",
		task_pid_nr(current), rc->c->kid, rc->c->inference_id,
		rc->cb->device_va, rc->cb->size);

	rc->s_msg.msg.id = MDW_IPI_APU_CMD;
	rc->s_msg.msg.c.iova = rc->cb->device_va;
	rc->s_msg.msg.c.size = rc->cb->size;
	rc->s_msg.msg.c.start_ts_ns = rc->start_ts_ns;
	rc->s_msg.complete = mdw_rv_ipi_cmplt_cmd;
	rc->s_msg.msg.inf_id = rc->c->inference_id;

	/* send */
	ret = mdw_rv_dev_send_msg(mrdev, &rc->s_msg);
	mdw_cmd_trace(rc->c, MDW_CMD_START);
	if (ret) {
		mdw_cmd_trace(rc->c, MDW_CMD_DONE);
		mdw_drv_err("pid(%d) send msg fail\n", task_pid_nr(current));
	}
	mdw_trace_end();
	return ret;
}

static int mdw_rv_callback(struct rpmsg_device *rpdev, void *data,
	int len, void *priv, u32 src)
{
	struct mdw_ipi_msg *msg = (struct mdw_ipi_msg *)data;
	struct mdw_ipi_msg_sync *s_msg = NULL;
	struct mdw_rv_dev *mrdev = (struct mdw_rv_dev *)priv;
	uint64_t ts1 = 0, ts2 = 0;

	mdw_drv_debug("callback msg(%d/0x%llu)\n", msg->id, msg->sync_id);

	ts1 = sched_clock();

	mutex_lock(&mrdev->msg_mtx);
	ts2 = sched_clock();
	mrdev->enter_rv_cb_time = ts2 - ts1;

	s_msg = mdw_rv_dev_msg_find_erase(mrdev, msg->sync_id);
	if (!s_msg) {
		mdw_drv_err("get ipi msg fail(0x%llu)", msg->sync_id);
	} else {
		memcpy(&s_msg->msg, msg, sizeof(*msg));
	}
	mutex_unlock(&mrdev->msg_mtx);

	ts1 = sched_clock();
	mrdev->rv_cb_time = ts1 - ts2;
	/* complete callback */
	if (s_msg && s_msg->complete)
		s_msg->complete(s_msg);

	return 0;
}

int mdw_rv_dev_set_param(struct mdw_rv_dev *mrdev, enum mdw_info_type type, uint32_t val)
{
	struct mdw_ipi_msg msg;
	int ret = 0;

	if (type == MDW_INFO_KLOG) {
		g_mdw_klog = val;
		goto out;
	} else if (type >= MDW_INFO_MAX) {
		ret = -EINVAL;
		goto out;
	}
	memset(&msg, 0, sizeof(msg));
	msg.id = MDW_IPI_PARAM;
	msg.p.type = type;
	msg.p.dir = MDW_INFO_SET;
	msg.p.value = val;

	mdw_drv_debug("set param(%u/%u/%u)\n", msg.p.type, msg.p.dir, msg.p.value);
	ret = mdw_rv_dev_send_sync(mrdev, &msg);
	if (!ret)
		memcpy(&mrdev->param, &msg.p, sizeof(msg.p));

out:
	return ret;
}

int mdw_rv_dev_get_param(struct mdw_rv_dev *mrdev, enum mdw_info_type type, uint32_t *val)
{
	int ret = 0;
	struct mdw_ipi_msg msg;

	switch (type) {
	case MDW_INFO_KLOG:
		*val = g_mdw_klog;
		break;
	case MDW_INFO_NORMAL_TASK_DLA:
		*val = mrdev->stat->task_num[APUSYS_DEVICE_MDLA][MDW_QUEUE_NORMAL];
		break;
	case MDW_INFO_NORMAL_TASK_DSP:
		*val = mrdev->stat->task_num[APUSYS_DEVICE_VPU][MDW_QUEUE_NORMAL] +
		mrdev->stat->task_num[APUSYS_DEVICE_MVPU][MDW_QUEUE_NORMAL];
		break;
	case MDW_INFO_NORMAL_TASK_DMA:
		*val = mrdev->stat->task_num[APUSYS_DEVICE_EDMA][MDW_QUEUE_NORMAL];
		break;
	case MDW_INFO_MIN_DTIME:
	case MDW_INFO_MIN_ETIME:
	case MDW_INFO_MAX_DTIME:
	case MDW_INFO_RESERV_TIME_REMAIN:
		memset(&msg, 0, sizeof(msg));
		msg.id = MDW_IPI_PARAM;
		msg.p.type = type;
		msg.p.dir = MDW_INFO_GET;

		mdw_drv_debug("get param(%u/%u/%u)\n", msg.p.type, msg.p.dir, msg.p.value);
		ret = mdw_rv_dev_send_sync(mrdev, &msg);
		if (!ret)
			*val = msg.p.value;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int mdw_rv_dev_handshake(struct mdw_rv_dev *mrdev)
{
	struct mdw_ipi_msg msg;
	int ret = 0;
	uint32_t type = 0;

	/* query basic infos */
	memset(&msg, 0, sizeof(msg));
	msg.id = MDW_IPI_HANDSHAKE;
	msg.h.h_id = MDW_IPI_HANDSHAKE_BASIC_INFO;
	msg.h.basic.stat_iova = mrdev->stat_iova;
	msg.h.basic.stat_size = sizeof(struct mdw_stat);

	ret = mdw_rv_dev_send_sync(mrdev, &msg);
	if (ret)
		goto out;

	if (msg.id != MDW_IPI_HANDSHAKE ||
		msg.h.h_id != MDW_IPI_HANDSHAKE_BASIC_INFO) {
		ret = -EINVAL;
		goto out;
	}

	memcpy(mrdev->dev_mask, &msg.h.basic.dev_bmp, sizeof(mrdev->dev_mask));
	memcpy(mrdev->mem_mask, &msg.h.basic.mem_bmp, sizeof(mrdev->mem_mask));
	mrdev->rv_version = msg.h.basic.version;
	mrdev->mdev->power_gain_time_us = msg.h.basic.power_gain_time;
	mdw_drv_warn("apusys: rv infos(%u)(0x%lx/0x%llx)(0x%lx/0x%llx) pwr(%u)\n",
		mrdev->rv_version, mrdev->dev_mask[0],
		msg.h.basic.dev_bmp, mrdev->mem_mask[0], msg.h.basic.mem_bmp,
		mrdev->mdev->power_gain_time_us);

	/* query device num */
	type = 0;
	do {
		type = find_next_bit(mrdev->dev_mask, APUSYS_DEVICE_MAX, type);
		if (type >= APUSYS_DEVICE_MAX)
			break;

		memset(&msg, 0, sizeof(msg));
		msg.id = MDW_IPI_HANDSHAKE;
		msg.h.h_id = MDW_IPI_HANDSHAKE_DEV_NUM;
		msg.h.dev.type = type;
		ret = mdw_rv_dev_send_sync(mrdev, &msg);
		if (ret)
			break;

		if (msg.id != MDW_IPI_HANDSHAKE ||
			msg.h.h_id != MDW_IPI_HANDSHAKE_DEV_NUM) {
			ret = -EINVAL;
			break;
		}

		mrdev->dev_num[type] = msg.h.dev.num;
		memcpy(&mrdev->meta_data[msg.h.dev.type][0],
			msg.h.dev.meta, sizeof(msg.h.dev.meta));
		mdw_drv_debug("dev(%d) num(%u)\n", type, msg.h.dev.num);
		type++;
	} while (type < APUSYS_DEVICE_MAX);

	/* query mem info */
	type = 0;
	do {
		type = find_next_bit(mrdev->mem_mask, MDW_MEM_TYPE_MAX, type);
		if (type >= MDW_MEM_TYPE_MAX)
			break;

		memset(&msg, 0, sizeof(msg));
		msg.id = MDW_IPI_HANDSHAKE;
		msg.h.h_id = MDW_IPI_HANDSHAKE_MEM_INFO;
		msg.h.mem.type = type;
		ret = mdw_rv_dev_send_sync(mrdev, &msg);
		if (ret)
			break;

		if (msg.id != MDW_IPI_HANDSHAKE ||
			msg.h.h_id != MDW_IPI_HANDSHAKE_MEM_INFO) {
			ret = -EINVAL;
			break;
		}

		/* only vlm need addr */
		if (type == MDW_MEM_TYPE_VLM)
			mrdev->minfos[type].device_va = msg.h.mem.start;
		mrdev->minfos[type].size = msg.h.mem.size;

		mdw_drv_debug("mem(%d)(0x%llx/0x%x)\n",
			type, msg.h.mem.start, msg.h.mem.size);
		type++;
	} while (type < MDW_MEM_TYPE_MAX);

out:
	return ret;
}

static void mdw_rv_dev_init_func(struct work_struct *wk)
{
	struct mdw_rv_dev *mrdev = container_of(wk, struct mdw_rv_dev, init_wk);
	struct mdw_device *mdev = mrdev->mdev;
	int ret = 0;

	mdw_drv_info("%s/%d\n", __func__, __LINE__);
	ret = mdw_rv_dev_handshake(mrdev);
	if (ret) {
		mdw_drv_err("handshake fail(%d)\n", ret);
		return;
	}

	memcpy(mdev->dev_mask, mrdev->dev_mask, sizeof(mrdev->dev_mask));
	mdev->inited = true;
	mdw_drv_info("late init done\n");
}

int mdw_rv_dev_init(struct mdw_device *mdev)
{
	struct rpmsg_channel_info chinfo = {};
	struct mdw_rv_dev *mrdev = NULL;
	int ret = 0;

	if (!mdev->rpdev || mdev->driver_type != MDW_DRIVER_TYPE_RPMSG) {
		mdw_drv_err("invalid driver type(%d)\n", mdev->driver_type);
		ret = -EINVAL;
		goto out;
	}

	mrdev = kzalloc(sizeof(*mrdev), GFP_KERNEL);
	if (!mrdev)
		return -ENOMEM;

	mdev->dev_specific = mrdev;
	mrdev->mdev = mdev;
	mrdev->rpdev = mdev->rpdev;

	strscpy(chinfo.name, mrdev->rpdev->id.name, RPMSG_NAME_SIZE);
	chinfo.src = mrdev->rpdev->src;
	chinfo.dst = RPMSG_ADDR_ANY;
	mrdev->ept = rpmsg_create_ept(mrdev->rpdev,
		mdw_rv_callback, mrdev, chinfo);
	if (!mrdev->ept) {
		mdw_drv_err("create ept fail\n");
		ret = -ENODEV;
		goto free_mrdev;
	}

	/* allocate stat buffer */
	mrdev->stat_buf = mdev->allocator->alloc(mdev->allocator, APU_SYSMEM_TYPE_DRAM,
		sizeof(*mrdev->stat), 0, "apumdw_stat_buf");
	if (!mrdev->stat_buf) {
		mdw_drv_err("allocate stat memory failed\n");
		goto free_ept;
	}
	/* map stat buffer */
	mrdev->stat_map = mdev->allocator->map(mdev->allocator, APU_SYSMEM_GET_DMABUF(mrdev->stat_buf),
		APU_SYSMEM_TYPE_DRAM, F_APU_SYSMEM_MAP_TYPE_DEVICE_SHAREVA);
	if (!mrdev->stat_map) {
		mdw_drv_err("map stat memory failed\n");
		goto free_stat_buf;
	}
	/* assign iova */
	mrdev->stat = mrdev->stat_map->vaddr;
	mrdev->stat_iova = mrdev->stat_map->device_va;
	mdw_drv_debug("stat buffer(0x%llx/%llu/%lu)\n", mrdev->stat_map->device_va,
		mrdev->stat_map->size, sizeof(*mrdev->stat));

	/* init up dev */
	mutex_init(&mrdev->msg_mtx);
	mutex_init(&mrdev->mtx);
	idr_init(&mrdev->msg_idr);
	INIT_WORK(&mrdev->init_wk, &mdw_rv_dev_init_func);
	INIT_WORK(&mrdev->power_off_wk, &mdw_rv_dev_power_off);

	schedule_work(&mrdev->init_wk);

	goto out;

free_stat_buf:
	mdev->allocator->free(mdev->allocator, mrdev->stat_buf);
free_ept:
	rpmsg_destroy_ept(mrdev->ept);
free_mrdev:
	kfree(mrdev);
	mdev->dev_specific = NULL;
out:
	return ret;
}

void mdw_rv_dev_deinit(struct mdw_device *mdev)
{
	struct mdw_rv_dev *mrdev = (struct mdw_rv_dev *)mdev->dev_specific;

	if (mrdev == NULL)
		return;

	mdev->allocator->unmap(mdev->allocator, mrdev->stat_map);
	mdev->allocator->free(mdev->allocator, mrdev->stat_buf);
	rpmsg_destroy_ept(mrdev->ept);
	if (timer_pending(&mrdev->power_off_timer))
		del_timer(&mrdev->power_off_timer);
	kfree(mrdev);
	mdev->dev_specific = NULL;
}
