// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GPUEB_IPI_H__
#define __GPUEB_IPI_H__

// Common implementation
#define IPI_TIMEOUT_MS     10000U
/* GPUEB side set CFG_AP_EB_IPI_TEST=yes to do IPI test */
#define IPI_TEST           0

#define GPUEB_MBOXDEV_NAME "gpueb_mboxdev"

extern struct mtk_mbox_device   gpueb_mboxdev;
extern struct mtk_ipi_device    gpueb_ipidev;
extern struct mtk_mbox_info     *gpueb_mbox_info;
extern struct mtk_mbox_pin_send *gpueb_mbox_pin_send;
extern struct mtk_mbox_pin_recv *gpueb_mbox_pin_recv;
extern const char *gpueb_mbox_pin_send_name[20];
extern const char *gpueb_mbox_pin_recv_name[20];

int gpueb_ipi_init(struct platform_device *pdev);
unsigned int gpueb_get_ts_mbox(void);
int gpueb_get_send_PIN_offset_by_name(char *send_PIN_name);
int gpueb_get_send_PIN_ID_by_name(char *send_PIN_name);
int gpueb_get_recv_PIN_ID_by_name(char *recv_PIN_name);
void *get_gpueb_ipidev(void);
unsigned int gpueb_get_mbox1_irq(void);
void gpueb_clr_mbox1_irq(unsigned int val);

#if IPI_TEST
int gpueb_ipi_test_init(void);
int gpueb_ipi_send_compl_test(int ipi, int msg);
int gpueb_get_send_pin_count(void);
#endif

#endif /* __GPUEB_IPI_H__ */