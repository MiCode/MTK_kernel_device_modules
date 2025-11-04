/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 *
 */

#ifndef __NEBULA_MAILBOX_H__
#define __NEBULA_MAILBOX_H__

#include <linux/mailbox_controller.h>

enum {
	POWER_CHANNEL,
	ORTHER_CHANNEL,
	REGULATOR_CHANNEL,
	PMIC_CHANNEL,
	QOS_CHANNEL,
	NEBULA_CHANNEL_NUM,
};

struct nebula_mbox_hdr {
	uint16_t msg_size;
	uint8_t valid;
	uint8_t polling_mode;
	uint32_t seq_num;
};

struct nebula_mbox_slots {
	uint32_t slot_size;
	uint32_t slot_cnt;
	uint64_t bitmap_alloc[1];
	uint64_t bitmap_submit[1];
	unsigned char slots[];
};

void nebula_mbox_rx_poll(struct mbox_chan *link, void *req, void *resp);
void nebula_mbox_set_slot_size(int slot_size);


static inline int get_chan_id(uint32_t tx_id)
{
	return (tx_id >> 8) & 0xFF;
}

static inline int get_slot_id(uint32_t tx_id)
{
	return tx_id & 0xFF;
}

static inline uint32_t gen_tx_id(int chan_id, uint32_t slot_id)
{
	return ((chan_id & 0xFF) << 8) | (slot_id & 0xFF);
}

static inline int nebula_mbox_index_of_slot(struct nebula_mbox_slots *slots,
		void *slot)
{
	uint64_t base = (uint64_t) slots->slots;
	uint64_t end =
			(uint64_t) (slots->slots + slots->slot_size * slots->slot_cnt);
	uint64_t s = (uint64_t) slot;

	return s >= base && s < end ? (s - base) / slots->slot_size : -1;
}

static inline void *nebula_mbox_get_slot(struct nebula_mbox_slots *slots,
		int index)
{
	return slots->slots + index * slots->slot_size;
}

static inline int nebula_mbox_find_slot_by_hdr(struct nebula_mbox_slots *slots,
		struct nebula_mbox_hdr *hdr)
{
	int slot_id = -1;
	int i;
	struct nebula_mbox_hdr *t;

	for (i = 0; i < slots->slot_cnt; i++) {
		t = (struct nebula_mbox_hdr *) nebula_mbox_get_slot(slots, i);
		if (t->seq_num == hdr->seq_num) {
			slot_id = i;
			break;
		}
	}

	return slot_id;
}

void *nebula_mbox_alloc_slot(struct nebula_mbox_slots *slots, int *slot_id);
void nebula_mbox_submit_slot(struct nebula_mbox_slots *slots, void *slot);
void nebula_mbox_free_slot(struct nebula_mbox_slots *slots, int slot_id);

#endif
