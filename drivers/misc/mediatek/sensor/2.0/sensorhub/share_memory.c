// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "share_mem " fmt

#include <linux/err.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/io.h>
#include <linux/slab.h>

#include "sensor_comm.h"
#include "share_memory.h"
#include "hf_sensor_type.h"
#include "tiny_crc32.h"

enum {
	SHARE_MEM_USAGE_V1 = 0,
	SHARE_MEM_USAGE_V2 = 1,
};

struct share_mem_config_handle {
	int (*handler)(struct share_mem_config *cfg, void *private_data);
	void *private_data;
};

struct share_mem_usage {
	uint8_t payload_type;
	uint8_t version;
	bool init_status;
	int id;
};

static struct share_mem_config_handle shm_handle[MAX_SHARE_MEM_PAYLOAD_TYPE];

static struct share_mem_usage shm_usage_table[] = {
	{
		.payload_type = SHARE_MEM_DATA_PAYLOAD_TYPE,
		.id = SENS_MEM_ID,
	},
	{
		.payload_type = SHARE_MEM_SUPER_DATA_PAYLOAD_TYPE,
		.id = SENS_SUPER_MEM_ID,
	},
	{
		.payload_type = SHARE_MEM_LIST_PAYLOAD_TYPE,
		.id = SENS_LIST_MEM_ID,
		.version = SHARE_MEM_USAGE_V2,
	},
	{
		.payload_type = SHARE_MEM_DEBUG_PAYLOAD_TYPE,
		.id = SENS_DEBUG_MEM_ID,
		.version = SHARE_MEM_USAGE_V2,
	},
	{
		.payload_type = SHARE_MEM_CUSTOM_W_PAYLOAD_TYPE,
		.id = SENS_CUSTOM_W_MEM_ID,
		.version = SHARE_MEM_USAGE_V2,
	},
	{
		.payload_type = SHARE_MEM_CUSTOM_R_PAYLOAD_TYPE,
		.id = SENS_CUSTOM_R_MEM_ID,
		.version = SHARE_MEM_USAGE_V2,
	},
};

static int share_mem_notify(struct share_mem *shm,
		struct share_mem_notify *notify)
{
	int ret = 0;
	struct sensor_comm_notify n;

	if (shm->write_position == shm->last_write_position)
		return -ENOBUFS;

	n.sensor_type = notify->sensor_type;
	n.command = notify->notify_cmd;
	n.value[0] = shm->write_position;
	n.length = sizeof(n.value[0]);
	ret = sensor_comm_notify(&n);
	if (ret < 0)
		return ret;
	notify->sequence = n.sequence;
	shm->last_write_position = shm->write_position;
	return ret;
}

static void share_mem_buffer_full_detect(struct share_mem *shm,
		uint32_t curr_written)
{
	int ret = 0;
	uint32_t rp = 0, wp = 0, buffer_size = 0;
	struct share_mem_notify notify;

	shm->buffer_full_written += curr_written;
	if (shm->buffer_full_written < shm->buffer_full_threshold)
		return;

	rp = shm->base->rp;
	wp = shm->base->wp;
	buffer_size = shm->base->buffer_size;

	shm->buffer_full_written = (wp > rp) ?
		(wp - rp) : (buffer_size - rp + wp);
	if (shm->buffer_full_written >= shm->buffer_full_threshold) {
		notify.sensor_type = SENSOR_TYPE_INVALID;
		notify.notify_cmd = shm->buffer_full_cmd;
		ret = share_mem_notify(shm, &notify);
		if (ret < 0)
			pr_err("%s buffer full notify fail %d\n",
				shm->name, ret);
		else
			shm->buffer_full_written = 0;
	}
}

int share_mem_seek(struct share_mem *shm, uint32_t write_position)
{
	if (!shm->base)
		return -EINVAL;

	mutex_lock(&shm->lock);
	shm->write_position = write_position;
	mutex_unlock(&shm->lock);
	return 0;
}

int share_mem_read_reset(struct share_mem *shm)
{
	if (!shm->base)
		return -EINVAL;

	mutex_lock(&shm->lock);
	shm->base->rp = 0;
	shm->write_position = 0;
	mutex_unlock(&shm->lock);
	return 0;
}

int share_mem_write_reset(struct share_mem *shm)
{
	if (!shm->base)
		return -EINVAL;

	mutex_lock(&shm->lock);
	shm->base->wp = 0;
	shm->write_position = 0;
	shm->last_write_position = 0;
	shm->buffer_full_written = 0;
	mutex_unlock(&shm->lock);
	return 0;
}

static int share_mem_read_dram(struct share_mem *shm,
		void *buf, uint32_t count)
{
	uint32_t rp = 0, wp = 0, buffer_size = 0, item_size = 0;
	uint8_t *src = NULL, *dst = buf;
	uint32_t first = 0, second = 0, read = 0;

	if (!shm->item_size || count % shm->item_size)
		return -EINVAL;

	wp = shm->write_position;
	rp = shm->base->rp;
	buffer_size = shm->base->buffer_size;
	item_size = shm->base->item_size;
	src = (uint8_t *)shm->base + offsetof(struct share_mem_base, data);

	if (item_size != shm->item_size)
		return -EIO;

	if (wp == rp)
		return 0;

	if (wp > rp) {
		first = wp - rp;
		second = 0;
	} else {
		first = buffer_size - rp;
		second = wp;
	}

	first = min(first, count);
	second = min(count - first, second);

	memcpy_fromio(dst, src + rp, first);
	memcpy_fromio(dst + first, src, second);
	read = first + second;
	rp += read;
	rp %= buffer_size;

	/*
	 * make sure that the data is copied before
	 * incrementing the rp index counter
	 */
	smp_wmb();
	shm->base->rp = rp;

	return read;
}

int share_mem_read(struct share_mem *shm, void *buf, uint32_t count)
{
	int ret = 0;

	if (!shm->base || !buf || !count)
		return -EINVAL;

	mutex_lock(&shm->lock);
	ret = share_mem_read_dram(shm, buf, count);
	mutex_unlock(&shm->lock);
	return ret;
}

static int share_mem_write_dram(struct share_mem *shm,
		void *buf, uint32_t count)
{
	uint32_t rp = 0, wp = 0, buffer_size = 0, item_size = 0, write = 0;
	uint8_t *src = buf, *dst = NULL;

	if (!shm->item_size || count % shm->item_size)
		return -EINVAL;

	rp = shm->base->rp;
	wp = shm->base->wp;
	buffer_size = shm->base->buffer_size;
	item_size = shm->base->item_size;
	dst = (uint8_t *)shm->base + offsetof(struct share_mem_base, data);

	if (item_size != shm->item_size)
		return -EIO;

	/* remain 1 count */
	while ((write < count) && ((wp + item_size) % buffer_size != rp)) {
		memcpy_toio(dst + wp, src + write, item_size);
		write += item_size;
		wp += item_size;
		wp %= buffer_size;
	}

	if (!write)
		return 0;

	/*
	 * make sure that the data is copied before
	 * incrementing the wp index counter
	 */
	smp_wmb();
	shm->base->wp = wp;
	shm->write_position = wp;

	if (shm->buffer_full_detect)
		share_mem_buffer_full_detect(shm, write);

	return write;
}

int share_mem_write(struct share_mem *shm, void *buf, uint32_t count)
{
	int ret = 0;

	if (!shm->base || !buf || !count)
		return -EINVAL;

	mutex_lock(&shm->lock);
	ret = share_mem_write_dram(shm, buf, count);
	mutex_unlock(&shm->lock);
	return ret;
}

int share_mem_flush(struct share_mem *shm, struct share_mem_notify *notify)
{
	int ret = 0;

	if (!shm->base)
		return -EINVAL;

	mutex_lock(&shm->lock);
	ret = share_mem_notify(shm, notify);
	mutex_unlock(&shm->lock);
	return ret;
}

int share_mem_init(struct share_mem *shm, struct share_mem_config *cfg)
{
	int ret = 0;

	if (!shm->name || !shm->item_size)
		return -EINVAL;

	mutex_init(&shm->lock);

	cfg->base->wp = 0;
	cfg->base->rp = 0;
	cfg->base->item_size = shm->item_size;
	cfg->base->buffer_size =
		(((long)cfg->buffer_size -
		offsetof(struct share_mem_base, data)) /
		shm->item_size) * shm->item_size;

	shm->write_position = 0;
	shm->last_write_position = 0;

	if (shm->buffer_full_detect) {
		shm->buffer_full_written = 0;
		shm->buffer_full_threshold =
			((uint32_t)(((cfg->base->buffer_size - shm->item_size) /
			shm->item_size) * 8 / 10)) *
			shm->item_size;
		if (shm->buffer_full_threshold <= shm->item_size) {
			ret = -EINVAL;
			goto exit;
		}
	}

	shm->base = cfg->base;
exit:
	return ret;
}

static bool share_mem_need_do_config(struct share_mem_usage *usage)
{
	if (usage->version == SHARE_MEM_USAGE_V2 && share_buffer_enabled())
		return false;

	return true;
}

static int share_mem_send_config(void)
{
	int ret = 0;
	uint32_t i = 0, index = 0;
	struct share_mem_usage *usage = NULL;
	struct sensor_comm_ctrl *ctrl = NULL;
	struct sensor_comm_share_mem *comm_shm = NULL;
	uint32_t ctrl_size = 0;

	ctrl_size = ipi_comm_size(sizeof(*ctrl) + sizeof(*comm_shm));
	ctrl = kzalloc(ctrl_size, GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

	ctrl->sensor_type = 0;
	ctrl->command = SENS_COMM_CTRL_SHARE_MEMORY_CMD;
	ctrl->length = sizeof(*comm_shm);
	comm_shm = (struct sensor_comm_share_mem *)ctrl->data;

	memset(comm_shm, 0, sizeof(*comm_shm));
	for (i = 0; i < ARRAY_SIZE(shm_usage_table); i++) {
		usage = &shm_usage_table[i];
		if (usage->payload_type >= MAX_SHARE_MEM_PAYLOAD_TYPE)
			continue;
		/* host init share mem ready we can send share mem to scp */
		if (usage->init_status) {
			comm_shm->base_info[index].payload_type =
				usage->payload_type;
			comm_shm->base_info[index].payload_base =
				(uint32_t)scp_get_reserve_mem_phys(usage->id);
			WARN_ON(!comm_shm->base_info[index].payload_base);
			comm_shm->available_num = ++index;
		}
		if (index == ARRAY_SIZE(comm_shm->base_info) ||
		    (i == (ARRAY_SIZE(shm_usage_table) - 1) && index)) {
			ret = sensor_comm_ctrl_send(ctrl, ctrl_size);
			if (ret < 0)
				break;
			index = 0;
			memset(comm_shm, 0, sizeof(*comm_shm));
		}
	}
	kfree(ctrl);
	return ret;
}

int share_mem_config(void)
{
	int ret = 0;
	uint32_t i = 0;
	struct share_mem_config cfg;
	struct share_mem_config_handle *handle = NULL;
	struct share_mem_usage *usage = NULL;

	for (i = 0; i < ARRAY_SIZE(shm_usage_table); i++) {
		usage = &shm_usage_table[i];
		if (!share_mem_need_do_config(usage))
			continue;
		/* must reset init_status to false scp reset each times */
		usage->init_status = false;
		if (usage->payload_type >= MAX_SHARE_MEM_PAYLOAD_TYPE) {
			pr_err("payload type %u invalid index %u\n",
				usage->payload_type, i);
			BUG_ON(1);
		}
		handle = &shm_handle[usage->payload_type];
		if (!handle->handler) {
			pr_err("payload type %u handler NULL index %u\n",
				usage->payload_type, i);
			BUG_ON(1);
		}
		memset(&cfg, 0, sizeof(cfg));
		cfg.payload_type = usage->payload_type;
		cfg.base =
			(void *)(long)scp_get_reserve_mem_virt(usage->id);
		cfg.buffer_size =
			(uint32_t)scp_get_reserve_mem_size(usage->id);
		BUG_ON(!cfg.base);
		ret = handle->handler(&cfg, handle->private_data);
		if (ret < 0)
			continue;
		usage->init_status = true;
	}

	return share_mem_send_config();
}

void share_mem_config_handler_register(uint8_t payload_type,
		int (*f)(struct share_mem_config *cfg, void *private_data),
		void *private_data)
{
	if (payload_type >= MAX_SHARE_MEM_PAYLOAD_TYPE)
		return;

	shm_handle[payload_type].private_data = private_data;
	shm_handle[payload_type].handler = f;
}

void share_mem_config_handler_unregister(uint8_t payload_type)
{
	if (payload_type >= MAX_SHARE_MEM_PAYLOAD_TYPE)
		return;

	shm_handle[payload_type].handler = NULL;
	shm_handle[payload_type].private_data = NULL;
}

#undef pr_fmt
#define pr_fmt(fmt) "share_buf " fmt
static DEFINE_SPINLOCK(share_buffer_rx_lock);
static struct sensor_comm_notify share_buffer_rx_notify;
static struct share_buffer_comm *share_buffer_comm_list[MAX_SHARE_BUFFER_CHN];

void share_buffer_init(struct share_buffer *sb, phys_addr_t addr, uint32_t size)
{
	void *base = (void *)(long)addr;
	uint32_t head_magic_size = sizeof(*sb->head_magic);
	uint32_t tail_magic_size = sizeof(*sb->tail_magic);
	uint32_t magic_size = head_magic_size + tail_magic_size;

	if (!share_buffer_enabled())
		return;

	if (!sb)
		return;

	BUG_ON(!base || !size || (size < magic_size));
	sb->total_size = size;
	sb->buffer_size = size - magic_size;
	sb->head_magic = (uint32_t *)base;
	sb->buffer = (void *)(base + head_magic_size);
	sb->tail_magic = (uint32_t *)(base + size - tail_magic_size);

	*sb->head_magic = 0xDEADBEEF;
	*sb->tail_magic = 0xDEADBEEF;

	// Must mb here to ensure getting the correct magic number
	smp_wmb();
	sb->inited = true;
}

int share_buffer_write(struct share_buffer *sb,
		uint32_t offset, void *data, uint32_t length)
{
	void *write_addr = NULL;

	if (!sb || !sb->inited)
		return -EINVAL;
	if ((offset + length) > sb->buffer_size)
		return -ENOMEM;

	if (*sb->head_magic != 0xDEADBEEF) {
		pr_err("before write head magic wrong\n");
		BUG();
	}
	if (*sb->tail_magic != 0xDEADBEEF) {
		pr_err("before write tail magic wrong\n");
		BUG();
	}

	write_addr = (void *)((unsigned long)sb->buffer + offset);
	memcpy_toio(write_addr, data, length);

	if (*sb->tail_magic != 0xDEADBEEF) {
		pr_err("after write tail magic wrong\n");
		BUG();
	}

	return length;
}

int share_buffer_read(struct share_buffer *sb,
		uint32_t offset, void *data, uint32_t length)
{
	void *read_addr = NULL;

	if (!share_buffer_enabled())
		return -EOPNOTSUPP;

	if (!sb || !sb->inited)
		return -EINVAL;
	if ((offset + length) > sb->buffer_size)
		return -ENOMEM;

	if (*sb->head_magic != 0xDEADBEEF) {
		pr_err("before read head magic wrong\n");
		BUG();
	}
	if (*sb->tail_magic != 0xDEADBEEF) {
		pr_err("before read tail magic wrong\n");
		BUG();
	}

	read_addr = (void *)((unsigned long)sb->buffer + offset);
	memcpy_fromio(data, read_addr, length);

	return length;
}

static void share_buffer_notify_handler(struct sensor_comm_notify *n,
		void *private_data)
{
	struct share_buffer_header *rx_sbh = NULL;
	uint8_t channel;

	spin_lock(&share_buffer_rx_lock);
	memcpy(&share_buffer_rx_notify, n, sizeof(share_buffer_rx_notify));
	rx_sbh = (struct share_buffer_header *)share_buffer_rx_notify.value;
	channel = rx_sbh->channel;
	spin_unlock(&share_buffer_rx_lock);
	if (channel >= MAX_SHARE_BUFFER_CHN) {
		WARN(1, "invalid channel %u\n", channel);
		return;
	}
	complete(&share_buffer_comm_list[channel]->done);
}

static int share_buffer_seq_comm(struct share_buffer_comm *sbc,
		struct share_buffer_mem *sbm)
{
	int ret = 0;
	int timeout = 0;
	unsigned long flags = 0;
	struct sensor_comm_notify tx_notify;
	struct share_buffer_header tx_sbh, rx_sbh;
	uint32_t crc = 0;

	if (!share_buffer_enabled())
		return -EOPNOTSUPP;

	if (sizeof(tx_sbh) > sizeof(tx_notify.value))
		return -EINVAL;

	/*
	 * NOTE: must reinit_completion before sensor_comm_notify
	 * wrong sequence:
	 * sensor_comm_notify ---> reinit_completion -> wait_for_completion
	 *		       |
	 *		    complete
	 * if complete before reinit_completion, will lose this complete
	 * right sequence:
	 * reinit_completion -> sensor_comm_notify -> wait_for_completion
	 */
	reinit_completion(&sbc->done);

	sbm->crc = tiny_crc32(&sbm->header, offsetof(typeof(*sbm), data) -
		offsetof(typeof(*sbm), header) + sbm->header.tx_len);

	ret = share_buffer_write(&sbc->sb_tx,
			0, sbm, sizeof(*sbm) + sbm->header.tx_len);
	if (ret < 0)
		return ret;

	tx_notify.sensor_type = sbm->header.sensor_type;
	tx_notify.command = SENS_COMM_NOTIFY_SHARE_BUFFER;
	tx_notify.length = sizeof(tx_sbh);
	tx_sbh.sensor_type = sbm->header.sensor_type;
	tx_sbh.channel = sbm->header.channel;
	tx_sbh.command = sbm->header.command;
	tx_sbh.sub_command = sbm->header.sub_command;
	tx_sbh.tx_len = sbm->header.tx_len;
	tx_sbh.rx_len = sbm->header.rx_len;
	memcpy(tx_notify.value, &tx_sbh, sizeof(tx_sbh));
	ret = sensor_comm_notify(&tx_notify);
	if (ret < 0)
		return ret;

	timeout = wait_for_completion_timeout(&sbc->done,
		msecs_to_jiffies(100));
	if (!timeout)
		return -ETIMEDOUT;

	spin_lock_irqsave(&share_buffer_rx_lock, flags);
	if (share_buffer_rx_notify.sequence != tx_notify.sequence &&
		share_buffer_rx_notify.sensor_type != tx_notify.sensor_type &&
		share_buffer_rx_notify.command != tx_notify.command) {
		spin_unlock_irqrestore(&share_buffer_rx_lock, flags);
		return -EREMOTEIO;
	}
	memcpy(&rx_sbh, share_buffer_rx_notify.value, sizeof(rx_sbh));
	spin_unlock_irqrestore(&share_buffer_rx_lock, flags);

	if (tx_sbh.sensor_type != rx_sbh.sensor_type ||
			tx_sbh.channel != rx_sbh.channel ||
			tx_sbh.command != rx_sbh.command ||
			tx_sbh.sub_command != rx_sbh.sub_command) {
		pr_err("tx|rx sbh %u|%u %u|%u %u|%u %u|%u fail\n",
			tx_sbh.sensor_type, rx_sbh.sensor_type,
			tx_sbh.channel, rx_sbh.channel,
			tx_sbh.command, rx_sbh.command,
			tx_sbh.sub_command, rx_sbh.sub_command);
		return -EILSEQ;
	}

	if (rx_sbh.rx_len > sbm->length ||
			rx_sbh.rx_len > sbm->header.rx_len) {
		pr_err("rx|tx len %u|%u %u fail\n",
			rx_sbh.rx_len, sbm->length, sbm->header.rx_len);
		return -EMSGSIZE;
	}

	ret = share_buffer_read(&sbc->sb_rx,
			0, sbm, sizeof(*sbm) + rx_sbh.rx_len);
	if (ret < 0)
		return ret;

	if (rx_sbh.sensor_type != sbm->header.sensor_type ||
			rx_sbh.command != sbm->header.command ||
			rx_sbh.sub_command != sbm->header.sub_command ||
			rx_sbh.rx_len != sbm->header.rx_len) {
		pr_err("sbh|sbm msg %u|%u %u|%u %u|%u %u|%u fail\n",
			rx_sbh.sensor_type, sbm->header.sensor_type,
			rx_sbh.command, sbm->header.command,
			rx_sbh.sub_command, sbm->header.sub_command,
			rx_sbh.rx_len, sbm->header.rx_len);
		return -EBADMSG;
	}

	crc = tiny_crc32(&sbm->header, offsetof(typeof(*sbm), data) -
		offsetof(typeof(*sbm), header) + sbm->header.rx_len);
	if (crc != sbm->crc) {
		pr_err("crc check error\n");
		return -EBADMSG;
	}

	return 0;
}

int share_buffer_comm_with(struct share_buffer_comm *sbc,
		int sensor_type, uint8_t command, uint8_t sub_command,
		void *tx_buf, uint32_t tx_len,
		void *rx_buf, uint32_t rx_len)
{
	int retry = 0, ret = 0;
	const int max_retry = 3;
	struct share_buffer_mem *sbm = NULL;
	uint32_t data_len = tx_len > rx_len ? tx_len : rx_len;
	uint32_t sbm_len = sizeof(*sbm) + data_len;

	if (!share_buffer_enabled())
		return -EOPNOTSUPP;

	if (!sbc)
		return -EFAULT;

	if (sbc->channel >= MAX_SHARE_BUFFER_CHN)
		return -EINVAL;

	if (command >= sbc->max_cmd)
		return -EINVAL;

	sbm = kzalloc(sbm_len, GFP_KERNEL);
	if (!sbm)
		return -ENOMEM;

	mutex_lock(&sbc->lock);
	do {
		/* retry need refill shm */
		sbm->header.sensor_type = sensor_type;
		sbm->header.channel = sbc->channel;
		sbm->header.command = command;
		sbm->header.sub_command = sub_command;
		sbm->header.tx_len = tx_len;
		sbm->header.rx_len = rx_len;
		sbm->length = data_len;
		if (tx_buf && tx_len)
			memcpy(sbm->data, tx_buf, sbm->header.tx_len);

		ret = share_buffer_seq_comm(sbc, sbm);
	} while (retry++ < max_retry && ret < 0);
	mutex_unlock(&sbc->lock);

	if (ret < 0) {
		pr_err("comm with %u %u %u %u fail %d\n",
			sbc->channel, sensor_type, command, sub_command, ret);
		goto out;
	}

	ret = sbm->header.rx_len;
	if (rx_buf && rx_len)
		memcpy(rx_buf, sbm->data, sbm->header.rx_len);

out:
	kfree(sbm);
	return ret;
}

struct share_buffer_comm *share_buffer_comm_get(uint8_t channel)
{
	return READ_ONCE(share_buffer_comm_list[channel]);
}

int share_buffer_comm_init(struct share_buffer_comm *sbc)
{
	if (!share_buffer_enabled())
		return -EOPNOTSUPP;

	BUG_ON(sbc->channel >= MAX_SHARE_BUFFER_CHN);
	BUG_ON(cmpxchg(&share_buffer_comm_list[sbc->channel], NULL, (void *)sbc));

	mutex_init(&sbc->lock);
	init_completion(&sbc->done);
	share_buffer_init(&sbc->sb_tx, sbc->tx_addr, sbc->tx_size);
	share_buffer_init(&sbc->sb_rx, sbc->rx_addr, sbc->rx_size);
	return 0;
}

int share_buffer_comm_plat_init(void)
{
	unsigned long flags = 0;

	if (!share_buffer_enabled())
		return -EOPNOTSUPP;

	spin_lock_irqsave(&share_buffer_rx_lock, flags);
	memset(&share_buffer_rx_notify, 0, sizeof(share_buffer_rx_notify));
	spin_unlock_irqrestore(&share_buffer_rx_lock, flags);

	sensor_comm_notify_handler_register(SENS_COMM_NOTIFY_SHARE_BUFFER,
		share_buffer_notify_handler, NULL);
	return 0;
}

void share_buffer_comm_plat_exit(void)
{
	if (!share_buffer_enabled())
		return;

	sensor_comm_notify_handler_unregister(SENS_COMM_NOTIFY_SHARE_BUFFER);
}
