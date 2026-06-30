/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __VIRTIO_EINT_H__
#define __VIRTIO_EINT_H__
#include <linux/types.h>
#define MAX_GIRQ_IOV_SIZE (4)

enum {
	EINT_OPS_SUBMIT_NORMAL,
	EINT_OPS_SUBMIT_MASK,
	EINT_OPS_SUBMIT_UNMASK,
	EINT_OPS_SUBMIT_RELEASE,
	EINT_OPS_SHUTDOWN,
};

enum {
	GIRQ_CB_MSG,
	GIRQ_CB_STP_IF_RX,
};

struct eint_event_packet {
	uint32_t event_id;
	uint32_t num;//gpio id
	uint8_t event_type; //gpio direction
	int32_t gFlag; //irq flag
	int32_t ret;
	uint32_t value;
	bool wait_result;
};

struct eint_request_packet {
	uint32_t request_id;
	uint32_t num;
	uint32_t request_type;
	uint32_t debonce;
	uint8_t op;
	uint32_t ret;
	uint64_t value;
	size_t out_buf_size[MAX_GIRQ_IOV_SIZE];
	size_t in_buf_size[MAX_GIRQ_IOV_SIZE];
};

struct eint_response_packet {
	uint32_t request_id;
	uint32_t num;
	uint8_t op;
	bool done;
	int32_t ret;
	size_t in_buf_size[MAX_GIRQ_IOV_SIZE];

};

enum {
	GIRQ_VQ_REQ = 0,
	GIRQ_VQ_EVT = 1,
	GIRQ_VQ_MAX = 2,
};

int get_virtio_eint_ready(void);
void set_gpio_count(int num);
void register_handler_cb(void(*cb)(int,int));
void register_findirq_cb(int(*cb)(int));
int submit_cmd(struct irq_data *d, int cmdId, int type, unsigned int dbc);
int get_debug_level(void);
int get_debug_gpio(void);
bool get_debug_drop(void);

int getSendnum(void);
void setSend_num(int gpio_n);
int setGpioReqFlag(struct eint_request_packet *req, int32_t rf);
int setGpioReqDebounce(struct eint_request_packet *req, int32_t rf); //{ return req->request_type = rf; }
int getResretValue(void);

//void setSendsta(gpio_n);

void virt_eint_set_test1(int a);

#endif
