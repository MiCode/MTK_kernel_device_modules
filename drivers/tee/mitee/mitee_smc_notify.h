#ifndef _MITEE_SMC_NOTIFY_H_
#define _MITEE_SMC_NOTIFY_H_

#include <linux/arm_ffa.h>

#define MITEE_NOTIFICATION_TYPE_CONNECT     0x01
#define MITEE_NOTIFICATION_TYPE_SIGNAL      0x02
#define MITEE_NOTIFICATION_TYPE_DATAFRAG    0x03
#define MITEE_NOTIFICATION_TYPE_DATA        0x04

typedef struct mitee_smc_notify_ctx {
	struct ffa_device *ffa_dev;
} mitee_smc_notify_ctx_t;

void mitee_smc_notify_init(struct ffa_device *ffa_dev);

extern unsigned int mitee_smc_notify_connect(uint8_t module_id,  unsigned int magic_num);
extern int mitee_smc_notify_signal(uint8_t module_id, unsigned int token, const uint8_t challenge);
extern int mitee_smc_notify_data(uint8_t module_id, unsigned int token, const void *data, uint16_t data_size);
#endif
