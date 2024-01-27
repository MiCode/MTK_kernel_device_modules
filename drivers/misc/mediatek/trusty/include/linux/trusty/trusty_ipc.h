/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef __LINUX_TRUSTY_TRUSTY_IPC_H
#define __LINUX_TRUSTY_TRUSTY_IPC_H

struct tipc_chan;

struct tipc_msg_buf {
	void *buf_va;
	phys_addr_t buf_pa;
	size_t buf_sz;
	size_t wpos;
	size_t rpos;
	struct list_head node;
};

enum tipc_chan_event {
	TIPC_CHANNEL_CONNECTED = 1,
	TIPC_CHANNEL_DISCONNECTED,
	TIPC_CHANNEL_SHUTDOWN,
};

struct tipc_chan_ops {
	void (*handle_event)(void *cb_arg, int event);
	struct tipc_msg_buf *(*handle_msg)(void *cb_arg,
					   struct tipc_msg_buf *mb);
	void (*handle_release)(void *cb_arg);
};

struct tipc_chan *ise_create_channel(struct device *dev,
				      const struct tipc_chan_ops *ops,
				      void *cb_arg);

int ise_chan_connect(struct tipc_chan *chan, const char *port);

int ise_chan_queue_msg(struct tipc_chan *chan, struct tipc_msg_buf *mb);

int ise_chan_shutdown(struct tipc_chan *chan);

void ise_chan_destroy(struct tipc_chan *chan);

struct tipc_msg_buf *ise_chan_get_rxbuf(struct tipc_chan *chan);

void ise_chan_put_rxbuf(struct tipc_chan *chan, struct tipc_msg_buf *mb);

struct tipc_msg_buf *
ise_chan_get_txbuf_timeout(struct tipc_chan *chan, long timeout);

void ise_chan_put_txbuf(struct tipc_chan *chan, struct tipc_msg_buf *mb);

static inline size_t mb_avail_space(struct tipc_msg_buf *mb)
{
	return mb->buf_sz - mb->wpos;
}

static inline size_t mb_avail_data(struct tipc_msg_buf *mb)
{
	return mb->wpos - mb->rpos;
}

static inline void *mb_put_data(struct tipc_msg_buf *mb, size_t len)
{
	void *pos = (u8 *)mb->buf_va + mb->wpos;
	BUG_ON(mb->wpos + len > mb->buf_sz);
	mb->wpos += len;
	return pos;
}

static inline void *mb_get_data(struct tipc_msg_buf *mb, size_t len)
{
	void *pos = (u8 *)mb->buf_va + mb->rpos;
	BUG_ON(mb->rpos + len > mb->wpos);
	mb->rpos += len;
	return pos;
}

#endif /* __LINUX_TRUSTY_TRUSTY_IPC_H */

