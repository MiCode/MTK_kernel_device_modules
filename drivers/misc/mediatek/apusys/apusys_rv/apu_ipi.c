// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/lockdep.h>
#include <linux/pm_runtime.h>
#include <linux/random.h>
#include <linux/ratelimit.h>
#include <linux/rpmsg.h>
#include <linux/sched/clock.h>
#include <linux/time64.h>
#include <linux/wait.h>

#include "apusys_rv_trace.h"

#include <mt-plat/aee.h>

#include "apu.h"
#include "apu_hw.h"
#include "apu_config.h"
#include "apu_mbox.h"
#include "apu_ipi_config.h"
#include "apu_excep.h"
#define CREATE_TRACE_POINTS
#include "apusys_rv_events.h"
#include "apu_regdump.h"
#include "apu_ipi_ut.h"

#if APU_PRG_SUPPORT
#include "pci_dev.h"
#endif

#define APUSYS_RV_IPI_HANDLE_PRINT \
	"%s: ipi_id=%d, len=%d, csum=0x%x, serial_no=%d, user_id=0x%llx, " \
	"usg_cnt = %d, latency=%lld, elapse=%lld, t_hndlr=%llu," \
	"t_mtx_lock=%llu, t_usage_cnt_update=%lld, t_wakup=%lld\n"
#define APUSYS_RV_IPI_HANDLE_PRINT_HANDLER_EXEC_LONG \
	"%s long: ipi_id=%d, len=%d, csum=0x%x, serial_no=%d, user_id=0x%llx, " \
	"usg_cnt = %d, latency=%lld, elapse=%lld, t_hndlr=%llu," \
	"t_mtx_lock=%llu, t_usage_cnt_update=%lld, t_wakup=%lld\n"

static struct lock_class_key ipi_lock_key[APU_IPI_MAX];

/* currently only used for IPI with send_ipi_in_rv_handler_max_ost != 0 */
static struct semaphore apu_ipi_ost_sem[APU_IPI_MAX];

static unsigned int tx_serial_no;
static unsigned int rx_serial_no;
unsigned int temp_buf[APU_SHARE_BUFFER_SIZE / 4];
static uint32_t wait_inbox_timeout_cnt;
bool wait_inbox_timeout_aee_triggered;

/* for IRQ affinity tuning */
static struct mutex affin_lock;
static unsigned int affin_depth;
static struct mtk_apu *g_apu;

static int current_ipi_handler_id;

static inline void dump_mbox0_reg(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	unsigned int i, val;

	/* mbox0_in mbox0_out mbox0_dummy */
	for (i = 0; i < 8; i++) {
		val = ioread32(apu->apu_mbox + i * APU_MBOX_SLOT_SIZE);
		dev_info(dev, "m0_in 0x%x (0x%x)\n", i * APU_MBOX_SLOT_SIZE, val);
	}

	for (i = 0; i < 8; i++) {
		val = ioread32(apu->apu_mbox + 0x20 + i * APU_MBOX_SLOT_SIZE);
		dev_info(dev, "m0_out 0x%x (0x%x)\n", 0x20 + i * APU_MBOX_SLOT_SIZE, val);
	}

	for (i = 0; i < 4; i++) {
		val = ioread32(apu->apu_mbox + 0x40 + i * APU_MBOX_SLOT_SIZE);
		dev_info(dev, "m0_dummy 0x%x (0x%x)\n", 0x40 + i * APU_MBOX_SLOT_SIZE,  val);
	}

	dev_info(dev, "m0_wkup_cfg 0x80 (0x%x)\n", ioread32(apu->apu_mbox + 0x80));
	dev_info(dev, "m0_func_cfg 0xB0 (0x%x)\n", ioread32(apu->apu_mbox + 0xB0));
	dev_info(dev, "m0_ibox_irq 0xC0 (0x%x)\n", ioread32(apu->apu_mbox + 0xC0));
	dev_info(dev, "m0_obox_irq 0xC4 (0x%x)\n", ioread32(apu->apu_mbox + 0xC4));
	dev_info(dev, "m0_err_irq 0xC8 (0x%x)\n", ioread32(apu->apu_mbox + 0xC8));
	dev_info(dev, "m0_ibox_mask 0xD0 (0x%x)\n", ioread32(apu->apu_mbox + 0xD0));
	dev_info(dev, "m0_ibox_pri_mask 0xD4 (0x%x)\n", ioread32(apu->apu_mbox + 0xD4));
	dev_info(dev, "m0_obox_mask 0xD8 (0x%x)\n", ioread32(apu->apu_mbox + 0xD8));
	dev_info(dev, "m0_err_mask 0xDC (0x%x)\n", ioread32(apu->apu_mbox + 0xDC));
	dev_info(dev, "m0_domain_cfg 0xE0 (0x%x)\n", ioread32(apu->apu_mbox + 0xE0));
	dev_info(dev, "m0_err_record 0xF0 (0x%x)\n", ioread32(apu->apu_mbox + 0xF0));
}

static inline void dump_msg_buf(struct mtk_apu *apu, void *data, uint32_t len)
{
	struct device *dev = apu->dev;
	uint32_t i;
	int size = 64, num;
	uint8_t buf[64], *ptr = buf;
	int ret;

	dev_info(dev, "===== dump message =====\n");
	for (i = 0; i < len; i++) {
		num = snprintf(ptr, size, "%02x ", ((uint8_t *)data)[i]);
		if (num <= 0) {
			dev_info(dev, "%s: snprintf return error(num = %d)\n",
				__func__, num);
			return;
		}
		size -= num;
		ptr += num;

		if ((i + 1) % 4 == 0) {
			ret = snprintf(ptr++, size--, " ");
			if (ret <= 0) {
				dev_info(dev, "%s: snprintf return error(ret = %d)\n",
					__func__, ret);
				return;
			}
		}

		if ((i + 1) % 16 == 0 || (i + 1) >= len) {
			dev_info(dev, "%s\n", buf);
			size = 64;
			ptr = buf;
		}
	}
	dev_info(dev, "========================\n");
}

static uint32_t calculate_csum(void *data, uint32_t len)
{
	uint32_t csum = 0, res = 0, i;
	uint8_t *ptr;

	for (i = 0; i < (len / sizeof(csum)); i++)
		csum += *(((uint32_t *)data) + i);

	ptr = (uint8_t *)data + len / sizeof(csum) * sizeof(csum);
	for (i = 0; i < (len % sizeof(csum)); i++)
		res |= *(ptr + i) << i * 8;

	csum += res;

	return csum;
}

static inline bool bypass_check(u32 id)
{
	/* whitelist IPI used in power off flow */
	return id == APU_IPI_DEEP_IDLE;
}

static void ipi_usage_cnt_update(struct mtk_apu *apu, u32 id, int diff)
{
	struct apu_ipi_desc *ipi = &apu->ipi_desc[id];

	if (ipi_attrs[id].ack != IPI_WITH_ACK)
		return;

	spin_lock(&apu->usage_cnt_lock);
	ipi->usage_cnt += diff;
	spin_unlock(&apu->usage_cnt_lock);
}

extern int apu_deepidle_power_on_aputop(struct mtk_apu *apu);

#if IS_ENABLED(CONFIG_VHOST_APU)
unsigned int virtio_csum;
uint32_t virtio_send_bufer_da;
uint32_t virtio_recv_bufer_da;

struct vhost_apu_platform_fp *vhost_apu_platform;

void vhost_apu_set_fp(struct vhost_apu_platform_fp *apu_platform)
{
	if (!apu_platform) {
		pr_info("%s apu_platform is NULL ", __func__);
		return;
	}
	vhost_apu_platform = apu_platform;
}
EXPORT_SYMBOL(vhost_apu_set_fp);

int vhost_apu_ipi_send(struct apu_mbox_hdr hdr)
{
	virtio_csum = hdr.csum;
	virtio_send_bufer_da = hdr.virtio_send_bufer_da;
	virtio_recv_bufer_da = hdr.virtio_recv_bufer_da;
	apu_ipi_send(g_apu, hdr.id, NULL, hdr.len, 0);
}
EXPORT_SYMBOL(vhost_apu_ipi_send);

#endif

int apu_ipi_send(struct mtk_apu *apu, u32 id, void *data, u32 len,
		 u32 wait_ms)
{
	struct mtk_apu_hw_ops *hw_ops;
	struct apu_ipi_desc *ipi;
	struct timespec64 ts, te;
	struct device *dev;
	struct apu_mbox_hdr hdr;
	unsigned long timeout;
	int ret = 0;
	uint64_t user_id = 0;

	ktime_get_ts64(&ts);

#if IS_ENABLED(CONFIG_VHOST_APU)
	mutex_lock(&apu->send_lock);
	uint32_t curr_vm_id = 0;

	curr_vm_id = (id >> 16);
	id = (id & 0xFFFF);

	if (curr_vm_id == 0) {
		if ((!apu) || (id <= APU_IPI_INIT) ||
			(id >= APU_IPI_MAX) || (id == APU_IPI_NS_SERVICE) ||
			(len > APU_SHARE_BUFFER_SIZE) || (!data))
			return -EINVAL;
	}
#else
	if ((!apu) || (id <= APU_IPI_INIT) ||
	    (id >= APU_IPI_MAX) || (id == APU_IPI_NS_SERVICE) ||
	    (len > APU_SHARE_BUFFER_SIZE) || (!data))
		return -EINVAL;
#endif

	dev = apu->dev;
	ipi = &apu->ipi_desc[id];
	hw_ops = &apu->platdata->ops;

	if (ipi_attrs[id].send_ipi_in_rv_handler_max_ost != 0) {
		ret = down_trylock(&apu_ipi_ost_sem[id]);
		if (ret) {
			pr_info("%s(%d): down_trylock fail(%d), usage_cnt(%d/%d)\n",
				__func__, id, ret, ipi->usage_cnt, ipi_attrs[id].send_ipi_in_rv_handler_max_ost);
			return -EBUSY;
		}
	}

	if ((apu->platdata->flags & F_FAST_ON_OFF) == 0)
		if (!pm_runtime_enabled(dev)) {
			dev_info(dev, "%s: rpm disabled, ipi=%d\n", __func__, id);
#if IS_ENABLED(CONFIG_VHOST_APU)
			mutex_unlock(&apu->send_lock);
#endif
			return -EBUSY;
		}

#if IS_ENABLED(CONFIG_VHOST_APU)
	if ((apu->platdata->flags & F_FAST_ON_OFF) &&
		ipi_attrs[id].direction == IPI_HOST_INITIATE &&
		apu->ipi_pwr_ref_cnt[id] == 0 && curr_vm_id == 0) {
		dev_info(dev, "%s: host initiated ipi(%d) not power on\n",
			__func__, id);
		mutex_unlock(&apu->send_lock);
		return -EINVAL;
	}

	if (ipi_attrs[id].direction == IPI_HOST_INITIATE &&
	    apu->ipi_inbound_locked == IPI_LOCKED && !bypass_check(id) &&
		curr_vm_id == 0) {
		/* remove to reduce log */
		/*
		 * apu_ipi_info_ratelimited(dev, "%s: ipi locked, ipi=%d\n",
		 *	__func__, id);
		 */
		mutex_unlock(&apu->send_lock);
		return -EAGAIN;
	}
#else
	if ((apu->platdata->flags & F_FAST_ON_OFF) &&
		ipi_attrs[id].direction == IPI_HOST_INITIATE &&
		apu->ipi_pwr_ref_cnt[id] == 0) {
		dev_info(dev, "%s: host initiated ipi(%d) not power on\n",
			__func__, id);
		return -EINVAL;
	}

	mutex_lock(&apu->send_lock);

	if (ipi_attrs[id].direction == IPI_HOST_INITIATE &&
	    apu->ipi_inbound_locked == IPI_LOCKED && !bypass_check(id)) {
		/* remove to reduce log */
		/*
		 * apu_ipi_info_ratelimited(dev, "%s: ipi locked, ipi=%d\n",
		 *	__func__, id);
		 */
		mutex_unlock(&apu->send_lock);
		return -EAGAIN;
	}
#endif

	/* re-init inbox mask everytime for aoc */
	apu_mbox_inbox_init(apu);

	if ((apu->platdata->flags & F_FAST_ON_OFF) == 0) {
		ret = apu_deepidle_power_on_aputop(apu);
		if (ret) {
			dev_info(dev, "apu_deepidle_power_on_aputop failed\n");
			mutex_unlock(&apu->send_lock);
			return -ESHUTDOWN;
		}
	}

	ret = apu_mbox_wait_inbox(apu);
	if (ret) {
		dev_info(dev, "wait inbox fail, ret=%d\n", ret);
		wait_inbox_timeout_cnt++;
		if (!wait_inbox_timeout_aee_triggered &&
			wait_inbox_timeout_cnt > 3 && !apu->bypass_pwr_off_chk) {
			apusys_rv_aee_warn("APUSYS_RV", "Wait_inbox_timeout");
			wait_inbox_timeout_aee_triggered = true;
		}
		goto unlock_mutex;
	}
	wait_inbox_timeout_cnt = 0;

	/* copy message payload to share buffer, need to do cache flush if
	 * the buffer is cacheable. currently not
	 */
#if IS_ENABLED(CONFIG_VHOST_APU)
	if (curr_vm_id == 0)
		memcpy_toio(apu->send_buf, data, len);
#else
	memcpy_toio(apu->send_buf, data, len);
#endif

	hdr.id = id;
	hdr.len = len;
#if IS_ENABLED(CONFIG_VHOST_APU)
	hdr.vm_id = curr_vm_id;
	if (curr_vm_id > 0) {
		hdr.csum = virtio_csum;
		hdr.virtio_send_bufer_da = virtio_send_bufer_da;
		hdr.virtio_recv_bufer_da = virtio_recv_bufer_da;
		virtio_csum = 0;
	} else {
		hdr.csum = calculate_csum(data, len);
	}

	hdr.serial_no = tx_serial_no++;

	if (curr_vm_id > 0)
		user_id = 0;
	else if (len >= sizeof(uint64_t))
		user_id = (((uint64_t)(((uint32_t *) (data))[len/sizeof(uint32_t) - 1])) << 32) +
			(((uint32_t *) (data))[len/sizeof(uint32_t) - 2]);

	/* only vm 0 need to do ipi_send_pre */
	if ((hw_ops->ipi_send_pre) && (!hdr.vm_id)) {
		hw_ops->ipi_send_pre(apu, id,
			ipi_attrs[id].direction == IPI_HOST_INITIATE);
	}
#else
	hdr.csum = calculate_csum(data, len);
	hdr.serial_no = tx_serial_no++;

	if (len >= sizeof(uint64_t))
		user_id = (((uint64_t)(((uint32_t *) (data))[len/sizeof(uint32_t) - 1])) << 32) +
			(((uint32_t *) (data))[len/sizeof(uint32_t) - 2]);

	if (hw_ops->ipi_send_pre)
		hw_ops->ipi_send_pre(apu, id,
			ipi_attrs[id].direction == IPI_HOST_INITIATE);
#endif

	if (APU_PRG_SUPPORT) {
		if (hw_ops->timesync_update)
			hw_ops->timesync_update(apu);
	}

	/* set ack to false first before write inbox */
	apu->ipi_id = id;
	apu->ipi_id_ack[id] = false;

	apu_mbox_write_inbox(apu, &hdr);

	/* ipi send so update counter right away*/
	ipi_usage_cnt_update(apu, id, 1);

	/* poll ack from remote processor if wait_ms specified */
	if (wait_ms) {
		timeout = msecs_to_jiffies(wait_ms);
		ret = wait_event_timeout(apu->ack_wq,
					 apu->ipi_id_ack[id],
					 timeout);

		if (!apu->ipi_id_ack[id] &&
			WARN(!ret, "apu ipi %d ack timeout!", id)) {
			ret = -ETIME;
		} else {
			ret = 0;
		}
		apu->ipi_id_ack[id] = false;
	}

unlock_mutex:
	if (hw_ops->ipi_send_post)
		hw_ops->ipi_send_post(apu);

	mutex_unlock(&apu->send_lock);

	ktime_get_ts64(&te);
	ts = timespec64_sub(te, ts);

	if (apu->platdata->flags & F_APUSYS_RV_TAG_SUPPORT)
		trace_apusys_rv_ipi_send(id, len, hdr.serial_no, hdr.csum, user_id,
			ipi->usage_cnt, timespec64_to_ns(&ts));

	apu_info_ratelimited(dev,
		"%s: ipi_id=%d, len=%d, csum=%x, serial_no=%d, user_id=0x%llx, usg_cnt = %d, elapse=%lld\n",
		__func__, id, len, hdr.csum, hdr.serial_no, user_id, ipi->usage_cnt,
		timespec64_to_ns(&ts));
#if IS_ENABLED(CONFIG_VHOST_APU)
	dev_info(dev, "curr_vm_id = %d\n", curr_vm_id);
#endif

	return ret;
}

int apu_ipi_lock(struct mtk_apu *apu)
{
	struct apu_ipi_desc *ipi;
	int i;
	bool ready_to_lock = true;

	if (mutex_trylock(&apu->send_lock) == 0)
		return -EBUSY;

	if (apu->ipi_inbound_locked == IPI_LOCKED) {
		dev_info(apu->dev, "%s: ipi already locked\n", __func__);
		mutex_unlock(&apu->send_lock);
		return 0;
	}

	spin_lock(&apu->usage_cnt_lock);
	for (i = 0; i < APU_IPI_MAX; i++) {
		ipi = &apu->ipi_desc[i];

		if (ipi_attrs[i].ack == IPI_WITH_ACK &&
		    ipi->usage_cnt != 0 &&
		    !bypass_check(i)) {
			spin_unlock(&apu->usage_cnt_lock);
			dev_info(apu->dev, "%s: ipi %d is still in use %d\n",
				 __func__, i, ipi->usage_cnt);
			spin_lock(&apu->usage_cnt_lock);
			ready_to_lock = false;
		}

	}

	if (!ready_to_lock) {
		spin_unlock(&apu->usage_cnt_lock);
		mutex_unlock(&apu->send_lock);
		return -EBUSY;
	}

	apu->ipi_inbound_locked = IPI_LOCKED;
	spin_unlock(&apu->usage_cnt_lock);

	mutex_unlock(&apu->send_lock);

	return 0;
}

void apu_ipi_unlock(struct mtk_apu *apu)
{
	mutex_lock(&apu->send_lock);

	if (apu->ipi_inbound_locked == IPI_UNLOCKED)
		dev_info(apu->dev, "%s: ipi already unlocked\n", __func__);

	spin_lock(&apu->usage_cnt_lock);
	apu->ipi_inbound_locked = IPI_UNLOCKED;
	spin_unlock(&apu->usage_cnt_lock);

	mutex_unlock(&apu->send_lock);
}

int apu_ipi_register(struct mtk_apu *apu, u32 id,
		     ipi_top_handler_t top_handler, ipi_handler_t handler, void *priv)
{
	if (!apu || id >= APU_IPI_MAX ||
		WARN_ON(!top_handler && !handler)) {
		if (apu)
			dev_info(apu->dev,
				"%s failed. apu=%p, id=%d, handler=%p, priv=%p\n",
				__func__, apu, id, handler, priv);
		return -EINVAL;
	}

	dev_info(apu->dev, "%s: apu=%p, ipi=%d, handler=%p, priv=%p",
		 __func__, apu, id, handler, priv);

	mutex_lock(&apu->ipi_desc[id].lock);
	apu->ipi_desc[id].top_handler = top_handler;
	apu->ipi_desc[id].handler = handler;
	apu->ipi_desc[id].priv = priv;

	/* to inform rv side that apu initiated ipi is ready */
	apu->conf_buf->ipi_krn_cb_rdy[id/32] |= BIT(id % 32);

	mutex_unlock(&apu->ipi_desc[id].lock);

	return 0;
}

void apu_ipi_unregister(struct mtk_apu *apu, u32 id)
{
	if (!apu || id >= APU_IPI_MAX) {
		if (apu != NULL)
			dev_info(apu->dev, "%s: invalid id=%d\n", __func__, id);
		return;
	}

	mutex_lock(&apu->ipi_desc[id].lock);
	apu->ipi_desc[id].top_handler = NULL;
	apu->ipi_desc[id].handler = NULL;
	apu->ipi_desc[id].priv = NULL;
	mutex_unlock(&apu->ipi_desc[id].lock);
}

static int apu_init_ipi_top_handler(void *data, unsigned int len, void *priv)
{
	struct mtk_apu *apu = priv;

	strscpy(apu->fw_ver, data, APU_FW_VER_LEN);

	apu->run.signaled = 1;
	wake_up_interruptible(&apu->run.wq);

	return IRQ_HANDLED;
}

static void apu_init_ipi_bottom_handler(void *data, unsigned int len, void *priv)
{
	struct mtk_apu *apu = priv;
	int ret;

	strscpy(apu->fw_ver, data, APU_FW_VER_LEN);

	/* remain for driver to know whether cold boot success or not */
	apu->run.signaled = 1;
	wake_up_interruptible(&apu->run.wq);

	ret = apu_power_on_off(apu->pdev, APU_IPI_INIT, 0, 1);
	if (ret)
		dev_info(apu->dev, "%s: call apu_power_on_off fail(%d)\n", __func__, ret);
}

static irqreturn_t apu_ipi_handler(int irq, void *priv)
{
	struct timespec64 t_elapse, tl;
	struct timespec64 t1, t2, t_diff;
	uint64_t t_elapse_ns = 0, t_mtx_lock_ns = 0, t_handler_ns = 0;
	uint64_t t_usage_cnt_update_ns = 0, t_wakup_ns = 0;
	struct mtk_apu *apu = priv;
	struct device *dev = apu->dev;
	ipi_handler_t handler;
	u32 id, len;
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;
	struct apu_ipi_desc *ipi;
	uint64_t user_id = 0;
#if IS_ENABLED(CONFIG_VHOST_APU)
	uint32_t vm_id = 0;
#endif

	if (hw_ops->ipi_clamp)
		hw_ops->ipi_clamp(apu);

	id = apu->hdr.id;
	len = apu->hdr.len;
	ipi = &apu->ipi_desc[id];

	if (len >= sizeof(uint64_t))
		user_id = (((uint64_t)(((uint32_t *) (temp_buf))[len/sizeof(uint32_t) - 1])) << 32) +
			(((uint32_t *) (temp_buf))[len/sizeof(uint32_t) - 2]);

	/* get the latency of threaded irq */
	ktime_get_ts64(&apu->ipi_bottom_ts_begin);
	tl = timespec64_sub(apu->ipi_bottom_ts_begin, apu->ipi_top_ts_end);

	ktime_get_ts64(&t1);
	mutex_lock(&apu->ipi_desc[id].lock);
	ktime_get_ts64(&t2);
	t_diff = timespec64_sub(t2, t1);
	t_mtx_lock_ns = timespec64_to_ns(&t_diff);
#if IS_ENABLED(CONFIG_VHOST_APU)
	vm_id = apu->hdr.vm_id;

	if (vm_id != 0 && vhost_apu_platform != NULL) {
		vhost_apu_platform->vhost_apu_handler(apu->hdr, vm_id);
		apu_mbox_ack_outbox(apu);
		ipi_usage_cnt_update(apu, id, -1);
		mutex_unlock(&apu->ipi_desc[id].lock);
		apu->ipi_id_ack[id] = true;
		wake_up(&apu->ack_wq);
		goto out;
	}
#endif

	handler = apu->ipi_desc[id].handler;
	if (!handler) {
		dev_info(dev, "IPI id=%d is not registered", id);
		mutex_unlock(&apu->ipi_desc[id].lock);
		goto out;
	}

	current_ipi_handler_id = id;

	if (apu->apusys_rv_trace_on)
		apusys_rv_trace_begin("apu_ipi_handle(%d)", id);
	ktime_get_ts64(&apu->ipi_handler_ts_begin);
	handler(temp_buf, len, apu->ipi_desc[id].priv);
	ktime_get_ts64(&apu->ipi_handler_ts_end);
	if (apu->apusys_rv_trace_on)
		apusys_rv_trace_end();
	t_diff = timespec64_sub(apu->ipi_handler_ts_end, apu->ipi_handler_ts_begin);
	t_handler_ns = timespec64_to_ns(&t_diff);

	ktime_get_ts64(&t1);
	ipi_usage_cnt_update(apu, id, -1);
	ktime_get_ts64(&t2);
	t_diff = timespec64_sub(t2, t1);
	t_usage_cnt_update_ns = timespec64_to_ns(&t_diff);

	if (ipi_attrs[id].send_ipi_in_rv_handler_max_ost != 0)
		up(&apu_ipi_ost_sem[id]);

	current_ipi_handler_id = -1;

	mutex_unlock(&apu->ipi_desc[id].lock);

	apu->ipi_id_ack[id] = true;

	ktime_get_ts64(&t1);
	wake_up(&apu->ack_wq);
	ktime_get_ts64(&t2);
	t_diff = timespec64_sub(t2, t1);
	t_wakup_ns = timespec64_to_ns(&t_diff);

out:
	ktime_get_ts64(&apu->ipi_bottom_ts_end);
	t_elapse = timespec64_sub(apu->ipi_bottom_ts_end, apu->ipi_bottom_ts_begin);
	t_elapse_ns = timespec64_to_ns(&t_elapse);

	if (apu->platdata->flags & F_APUSYS_RV_TAG_SUPPORT)
		trace_apusys_rv_ipi_handle(id, len, apu->hdr.serial_no,
			apu->hdr.csum, user_id, ipi->usage_cnt,
			timespec64_to_ns(&apu->ipi_top_ts_begin), timespec64_to_ns(&apu->ipi_bottom_ts_begin),
			timespec64_to_ns(&tl), t_elapse_ns, t_handler_ns);

	/* t_elapse_ns > 1s */
	if (t_elapse_ns > 1000000000)
		dev_info(dev, APUSYS_RV_IPI_HANDLE_PRINT_HANDLER_EXEC_LONG,
			__func__, id, len, apu->hdr.csum, apu->hdr.serial_no, user_id, ipi->usage_cnt,
			timespec64_to_ns(&tl), t_elapse_ns, t_handler_ns,
			t_mtx_lock_ns, t_usage_cnt_update_ns, t_wakup_ns);
	else if ((apu->platdata->flags & F_BRINGUP) != 0)
		dev_info(dev, APUSYS_RV_IPI_HANDLE_PRINT,
			__func__, id, len, apu->hdr.csum, apu->hdr.serial_no, user_id, ipi->usage_cnt,
			timespec64_to_ns(&tl), t_elapse_ns, t_handler_ns,
			t_mtx_lock_ns, t_usage_cnt_update_ns, t_wakup_ns);
	else
		apu_info_ratelimited(dev, APUSYS_RV_IPI_HANDLE_PRINT,
			__func__, id, len, apu->hdr.csum, apu->hdr.serial_no, user_id, ipi->usage_cnt,
			timespec64_to_ns(&tl), t_elapse_ns, t_handler_ns,
			t_mtx_lock_ns, t_usage_cnt_update_ns, t_wakup_ns);

	if (hw_ops->wake_unlock && ipi_attrs[id].direction == IPI_APU_INITIATE
		&& ipi_attrs[id].ack == IPI_WITH_ACK)
		hw_ops->wake_unlock(apu, id);

#if APU_PRG_SUPPORT
	/* Unmask MSI IRQ */
	apu_pcie_host_unmask_irq(APU_OUTBOX0_INTR);
#endif

	return IRQ_HANDLED;
}

irqreturn_t apu_ipi_int_handler(int irq, void *priv)
{
	struct mtk_apu *apu = priv;
	struct device *dev = apu->dev;
	struct mtk_share_obj *recv_obj = apu->recv_buf;
	u32 id, len, calc_csum;
	bool finish = false;
	uint32_t status;
	ipi_top_handler_t top_handler;
	int ret;
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;
#if IS_ENABLED(CONFIG_VHOST_APU)
	uint32_t vm_id = 0;
#endif

	status = ioread32(apu->apu_mbox + 0xc4);
	if (status != ((1 << APU_MBOX_HDR_SLOTS) - 1))
		dev_info(dev, "WARN abnormal isr call(0x%x)\n", status);

	ktime_get_ts64(&apu->ipi_top_ts_begin);

	apu_mbox_read_outbox(apu, &apu->hdr);
	id = apu->hdr.id;
	len = apu->hdr.len;

	if (id >= APU_IPI_MAX) {
		dev_info(dev, "no such IPI id = %d\n", id);
		finish = true;
	}

	if (len > APU_SHARE_BUFFER_SIZE) {
		dev_info(dev, "IPI message too long(len %d, max %d)\n",
			len, APU_SHARE_BUFFER_SIZE);
		finish = true;
	}

	if (apu->hdr.serial_no != rx_serial_no) {
		dev_info(dev, "unmatched serial_no: curr=%u, recv=%u\n",
			rx_serial_no, apu->hdr.serial_no);
		/* correct the serial no. */
		rx_serial_no = apu->hdr.serial_no;
		/* apu_regdump(); */
		dump_mbox0_reg(apu);
		apusys_rv_aee_warn("APUSYS_RV", "IPI rx_serial_no unmatch");
	}
	rx_serial_no++;
#if IS_ENABLED(CONFIG_VHOST_APU)
	vm_id = apu->hdr.vm_id;
	if (vm_id != 0)
		return IRQ_WAKE_THREAD;
#endif

	if (finish)
		goto done;

	if (hw_ops->wake_lock && ipi_attrs[id].direction == IPI_APU_INITIATE
		&& ipi_attrs[id].ack == IPI_WITH_ACK)
		hw_ops->wake_lock(apu, id);

	memcpy_fromio(temp_buf, &recv_obj->share_buf, len);

	/* ack after data copied */
	apu_mbox_ack_outbox(apu);
#if APU_PRG_SUPPORT
	/* mask & clear irq */
	apu_pcie_host_mask_clr_irq(APU_OUTBOX0_INTR);
#endif

	calc_csum = calculate_csum(temp_buf, len);
	if (calc_csum != apu->hdr.csum) {
		dev_info(dev, "csum error: recv=0x%08x, calc=0x%08x, skip\n",
			apu->hdr.csum, calc_csum);
		dump_msg_buf(apu, temp_buf, apu->hdr.len);
		/* apu_regdump(); */
		dump_mbox0_reg(apu);
		apusys_rv_aee_warn("APUSYS_RV", "IPI rx csum error");
		/* csum error data not valid */
		return IRQ_HANDLED;
	}

	/* excute top handler if exist */
	top_handler = apu->ipi_desc[id].top_handler;
	if (top_handler) {
		ret = top_handler(temp_buf, len, apu->ipi_desc[id].priv);
		if (ret == IRQ_HANDLED)
			return IRQ_HANDLED;
	}

	ktime_get_ts64(&apu->ipi_top_ts_end);

	/*
	 * if (apu->platdata->flags & F_BRINGUP)
	 *	dev_info(dev, "%s: ipi_id=%d, len=%d, csum=0x%x, serial_no=%d\n",
	 *		__func__, apu->hdr.id, apu->hdr.len, apu->hdr.csum, apu->hdr.serial_no);
	 */

	return IRQ_WAKE_THREAD;

done:
	apu_mbox_ack_outbox(apu);
#if APU_PRG_SUPPORT
	/* mask & clear irq */
	apu_pcie_host_mask_clr_irq(APU_OUTBOX0_INTR);
	/* Unmask MSI IRQ */
	apu_pcie_host_unmask_irq(APU_OUTBOX0_INTR);
#endif

	return IRQ_HANDLED;
}

static int apu_send_ipi(struct platform_device *pdev, u32 id, void *buf,
			unsigned int len, unsigned int wait)
{
	struct mtk_apu *apu = platform_get_drvdata(pdev);

	return apu_ipi_send(apu, id, buf, len, wait);
}

static int apu_register_ipi(struct platform_device *pdev, u32 id,
			    ipi_handler_t handler, void *priv)
{
	struct mtk_apu *apu = platform_get_drvdata(pdev);

	return apu_ipi_register(apu, id, NULL, handler, priv);
}

static void apu_unregister_ipi(struct platform_device *pdev, u32 id)
{
	struct mtk_apu *apu = platform_get_drvdata(pdev);

	apu_ipi_unregister(apu, id);
}

int apu_power_on_off(struct platform_device *pdev, u32 id, u32 on, u32 off)
{
	int ret, i;
	struct mtk_apu *apu = platform_get_drvdata(pdev);
	struct device *dev;
	struct mtk_apu_hw_ops *hw_ops;
	struct apu_ipi_desc *ipi;
	struct timespec64 t1, t2;
	struct timespec64 ts_polling, te_polling;
	uint64_t time_diff;

	ktime_get_ts64(&t1);

	if (!apu)
		return -EINVAL;

	dev = apu->dev;

	if (id >= APU_IPI_MAX) {
		dev_info(dev, "%s: invalid ipi id = %d", __func__, id);
		return -EINVAL;
	}

	hw_ops = &apu->platdata->ops;

	if ((apu->platdata->flags & F_FAST_ON_OFF) == 0 ||
		!hw_ops->power_on_off) {
		/* dev_info(dev, "%s: not support\n", __func__); */
		return -EOPNOTSUPP;
	}

	if (apu->apusys_rv_trace_on)
		apusys_rv_trace_begin("apu_pwr(%d/%d/%d)", id, on, off);

	if (apu->pwr_profile_polling_mode && hw_ops->polling_rpc_status) {
		mutex_lock(&apu->power_profile_lock);
		ktime_get_ts64(&ts_polling);
	}

	ipi = &apu->ipi_desc[id];
	spin_lock(&apu->usage_cnt_lock);
	if (off == 1 && ipi_attrs[id].direction == IPI_HOST_INITIATE &&
		(ipi->usage_cnt != 0 && apu->ipi_pwr_ref_cnt[id] <= 1) &&
		!(ipi->usage_cnt == 1 && current_ipi_handler_id == id &&
			apu->ipi_pwr_ref_cnt[id] == 1)) {
		dev_info(dev, "%s: power off fail, ipi(%d) usage cnt(%d) not zero!\n",
			__func__, id, ipi->usage_cnt);
		spin_unlock(&apu->usage_cnt_lock);
		apusys_rv_aee_warn("APUSYS_RV", "APUSYS_RV_OFF_FAIL");
		if (apu->pwr_profile_polling_mode && hw_ops->polling_rpc_status)
			mutex_unlock(&apu->power_profile_lock);
		ret = -EINVAL;
		goto out;
	}
	spin_unlock(&apu->usage_cnt_lock);

	ret = hw_ops->power_on_off(apu, id, on, off);

	if (apu->pwr_profile_polling_mode && hw_ops->polling_rpc_status &&
		((on && apu->local_pwr_ref_cnt == 1) || (off && apu->local_pwr_ref_cnt == 0))) {
		if (on)
			hw_ops->polling_rpc_status(apu, 1, 1000000);
		else if (off)
			hw_ops->polling_rpc_status(apu, 0, 1000000);
		ktime_get_ts64(&te_polling);
		ts_polling = timespec64_sub(te_polling, ts_polling);
		time_diff = timespec64_to_ns(&ts_polling);
		apu_power_on_off_profile(on, off, time_diff, apu->smc_time_diff_ns);
	}

	ktime_get_ts64(&t2);
	t1 = timespec64_sub(t2, t1);
	time_diff = timespec64_to_ns(&t1);

	if (apu->platdata->flags & F_APUSYS_RV_TAG_SUPPORT)
		trace_apusys_rv_pwr_ctrl(id, on, off, time_diff,
			apu->sub_latency[0], apu->sub_latency[1], apu->sub_latency[2],
			apu->sub_latency[3], apu->sub_latency[4], apu->sub_latency[5],
			apu->sub_latency[6], apu->sub_latency[7]);

	for (i = 0; i < MAX_PWR_SUB_LATENCY; i++)
		apu->sub_latency[i] = 0;

	if (apu->pwr_profile_polling_mode && hw_ops->polling_rpc_status)
		mutex_unlock(&apu->power_profile_lock);

	apu_info_ratelimited(dev, "%s: id(%u), on(%u), off(%u), latency = %llu ns\n",
		__func__, id, on, off, time_diff);

out:
	if (apu->apusys_rv_trace_on)
		apusys_rv_trace_end();

	return ret;
}

static struct mtk_apu_rpmsg_info mtk_apu_rpmsg_info = {
	.send_ipi = apu_send_ipi,
	.register_ipi = apu_register_ipi,
	.unregister_ipi = apu_unregister_ipi,
	.ns_ipi_id = APU_IPI_NS_SERVICE,
	.power_on_off = apu_power_on_off,
};

static void apu_add_rpmsg_subdev(struct mtk_apu *apu)
{
	apu->rpmsg_subdev =
		mtk_apu_rpmsg_create_rproc_subdev(to_platform_device(apu->dev),
						  &mtk_apu_rpmsg_info);

	if (apu->rpmsg_subdev)
		rproc_add_subdev(apu->rproc, apu->rpmsg_subdev);
}

static void apu_remove_rpmsg_subdev(struct mtk_apu *apu)
{
	if (apu->rpmsg_subdev) {
		rproc_remove_subdev(apu->rproc, apu->rpmsg_subdev);
		mtk_apu_rpmsg_destroy_rproc_subdev(apu->rpmsg_subdev);
		apu->rpmsg_subdev = NULL;
	}
}

void apu_ipi_config_remove(struct mtk_apu *apu)
{
	dma_free_coherent(apu->dev, APU_SHARE_BUF_SIZE,
			  apu->recv_buf, apu->recv_buf_da);
}

int apu_ipi_config_init(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	struct apu_ipi_config *ipi_config;
	void *ipi_buf = NULL;
	dma_addr_t ipi_buf_da = 0;

	ipi_config = (struct apu_ipi_config *)
		get_apu_config_user_ptr(apu->conf_buf, eAPU_IPI_CONFIG);

	/* initialize shared buffer */
	ipi_buf = dma_alloc_coherent(dev, APU_SHARE_BUF_SIZE,
				     &ipi_buf_da, GFP_KERNEL);
	if (!ipi_buf || !ipi_buf_da) {
		dev_info(dev, "failed to allocate ipi share memory\n");
		return -ENOMEM;
	}

	if (BOOT_FROM_APU_TCM) {
		ipi_buf = apu->apu_tcm + IPI_BUF_OFS;
		ipi_buf_da = APU_TCM_BASE + IPI_BUF_OFS;
	}

	dev_info(dev, "%s: ipi_buf=%p, ipi_buf_da=%llu\n",
		 __func__, ipi_buf, ipi_buf_da);

	memset_io(ipi_buf, 0, sizeof(struct mtk_share_obj)*2);

	apu->recv_buf = ipi_buf;
	apu->recv_buf_da = ipi_buf_da;
	apu->send_buf = ipi_buf + sizeof(struct mtk_share_obj);
	apu->send_buf_da = ipi_buf_da + sizeof(struct mtk_share_obj);

	ipi_config->in_buf_da = apu->send_buf_da;
	ipi_config->out_buf_da = apu->recv_buf_da;

	return 0;
}

int apu_ipi_affin_enable(void)
{
	struct mtk_apu *apu = g_apu;
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;
	int ret = 0;

	mutex_lock(&affin_lock);

	if (affin_depth == 0)
		ret = hw_ops->irq_affin_set(apu);

	affin_depth++;

	mutex_unlock(&affin_lock);

	return ret;
}

int apu_ipi_affin_disable(void)
{
	struct mtk_apu *apu = g_apu;
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;
	int ret = 0;

	mutex_lock(&affin_lock);

	affin_depth--;

	if (affin_depth == 0)
		ret = hw_ops->irq_affin_unset(apu);

	mutex_unlock(&affin_lock);

	return ret;
}

int apu_ipi_init(struct platform_device *pdev, struct mtk_apu *apu)
{
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;
	struct device *dev = apu->dev;
	int i, ret;

	tx_serial_no = 0;
	rx_serial_no = 0;

	mutex_init(&apu->send_lock);
	mutex_init(&apu->power_lock);
	mutex_init(&apu->power_profile_lock);
	spin_lock_init(&apu->usage_cnt_lock);

	mutex_init(&affin_lock);
	affin_depth = 0;
	g_apu = apu;

	for (i = 0; i < APU_IPI_MAX; i++)
		sema_init(&apu_ipi_ost_sem[i], ipi_attrs[i].send_ipi_in_rv_handler_max_ost);

	if (apu->platdata->flags & F_APU_IPI_UT_SUPPORT)
		apu_ipi_ut_init(apu);

	if (apu->platdata->flags & F_RV_BSP_RX_SUPPORT)
		rv_bsp_rx_init(apu);

	for (i = 0; i < APU_IPI_MAX; i++) {
		mutex_init(&apu->ipi_desc[i].lock);
		lockdep_set_class_and_name(&apu->ipi_desc[i].lock,
					   &ipi_lock_key[i],
					   ipi_attrs[i].name);
		apu->ipi_pwr_ref_cnt[i] = 0;
	}

	current_ipi_handler_id = -1;

	init_waitqueue_head(&apu->run.wq);
	init_waitqueue_head(&apu->ack_wq);

	/* APU initialization IPI register */
	if ((apu->platdata->flags & F_FAST_ON_OFF) == 0) {
		ret = apu_ipi_register(apu, APU_IPI_INIT, apu_init_ipi_top_handler, NULL, apu);
		if (ret) {
			dev_info(dev, "failed to register apu_init_ipi_top_handler for init, ret=%d\n",
				ret);
			return ret;
		}
	} else {
		ret = apu_ipi_register(apu, APU_IPI_INIT, NULL, apu_init_ipi_bottom_handler, apu);
		if (ret) {
			dev_info(dev, "failed to register apu_init_ipi_bottom_handler for init, ret=%d\n",
				ret);
			return ret;
		}
	}

	/* add rpmsg subdev */
	apu_add_rpmsg_subdev(apu);

	/* register mailbox IRQ */
#if APU_PRG_SUPPORT
		apu->mbox0_irq_number = apu_get_irq(APU_OUTBOX_IRQ_BIT_0);
#else
		apu->mbox0_irq_number = platform_get_irq_byname(pdev, "mbox0_irq");
#endif
	dev_info(&pdev->dev, "%s: mbox0_irq = %d\n", __func__,
		 apu->mbox0_irq_number);

	ret = devm_request_threaded_irq(&pdev->dev, apu->mbox0_irq_number,
				apu_ipi_int_handler, apu_ipi_handler, IRQF_ONESHOT,
				"apu_ipi", apu);
	if (ret < 0) {
		dev_info(&pdev->dev, "%s: failed to request irq %d, ret=%d\n",
			__func__, apu->mbox0_irq_number, ret);
		goto remove_rpmsg_subdev;
	}

	hw_ops->irq_affin_init(apu);
	if (hw_ops->irq_affin_online && hw_ops->irq_affin_offline) {
		apu->cpuhp_state = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
				"apu_ipi:online",
				hw_ops->irq_affin_online,
				hw_ops->irq_affin_offline);
	}

	apu_mbox_hw_init(apu);

	return 0;

remove_rpmsg_subdev:
	apu_remove_rpmsg_subdev(apu);
	apu_ipi_unregister(apu, APU_IPI_INIT);

	return ret;
}

void apu_ipi_remove(struct mtk_apu *apu)
{
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;

	apu_mbox_hw_exit(apu);
	apu_remove_rpmsg_subdev(apu);
	apu_ipi_unregister(apu, APU_IPI_INIT);
	if (apu->cpuhp_state)
		cpuhp_remove_state(apu->cpuhp_state);
	if (hw_ops->irq_affin_clear)
		hw_ops->irq_affin_clear(apu);
	if (apu->platdata->flags & F_APU_IPI_UT_SUPPORT)
		apu_ipi_ut_exit();
	if (apu->platdata->flags & F_RV_BSP_RX_SUPPORT)
		rv_bsp_rx_exit();
}

