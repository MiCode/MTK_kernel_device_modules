#include "mitee_smc_notify.h"
#include "optee_ffa.h"
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/string.h>

static mitee_smc_notify_ctx_t ctx;

// smc_notify_init
void mitee_smc_notify_init(struct ffa_device *dev)
{
	ctx.ffa_dev = dev;
}

static int mitee_smc_notify_send(uint8_t module_id, uint8_t type, unsigned int token,\
			uint16_t data_size,uint64_t data0, uint64_t data1, uint64_t data2)
{
	int ret;
	/* ffa ops */
	const struct ffa_msg_ops *msg_ops = ctx.ffa_dev->ops->msg_ops;
	struct ffa_send_direct_data data = {
		OPTEE_FFA_SMC_NOTIFY,
		(uint64_t)token | ((uint64_t)module_id << 32) \
		| ((uint64_t)type << 40) | ((uint64_t)data_size << 48),
		data0,
		data1,
		data2,
	};
	ret = msg_ops->sync_send_receive(ctx.ffa_dev, &data);
	if (type == MITEE_NOTIFICATION_TYPE_CONNECT && !ret) {
		// return connect token
		return (int)data.data0;
	}
	return ret;
}

/* connect once to get token */
unsigned int mitee_smc_notify_connect(uint8_t module_id,  unsigned int magic_num)
{
	int ret;
	ret = mitee_smc_notify_send(module_id, MITEE_NOTIFICATION_TYPE_CONNECT,\
				magic_num, 0, 0, 0, 0);
	if (ret == 0xffffffff){
		pr_err("mitee smc notify connect failed \n");
	}
	// return connect token
	return (unsigned int)ret;
}

/* linux kernel send signal to mitee */
int mitee_smc_notify_signal(uint8_t module_id, unsigned int token, const uint8_t challenge)
{
	int ret;
	ret = mitee_smc_notify_send(module_id, MITEE_NOTIFICATION_TYPE_SIGNAL,\
				token, 1u, challenge, 0, 0);
	if (ret){
		pr_err("mitee smc signal failed \n");
	}
	return ret;
}

/* linux kernel send data to mitee */
int mitee_smc_notify_data(uint8_t module_id, unsigned int token,\
				const void *data, uint16_t data_size)
{
	int ret;
	uint16_t left_size = data_size;
	uint16_t curr_size = 0u;
	uint64_t data_buf[3] = {0};

	if (left_size > 24u) {
		curr_size = 24u;
	} else {
		curr_size = left_size;
	}
	left_size -= curr_size;

	memcpy(data_buf, data, curr_size);
	/* inform the data size in the first smc call */
	ret = mitee_smc_notify_send(module_id, MITEE_NOTIFICATION_TYPE_DATA,\
				token, data_size, data_buf[0], data_buf[1], data_buf[2]);
	/* data fragment */
	while (left_size > 0) {
		if (left_size > 24u) {
			curr_size = 24u;
		} else {
			curr_size = left_size;
		}
		left_size -= curr_size;
		memcpy(data_buf, data + (data_size - left_size - curr_size), curr_size);
		ret = mitee_smc_notify_send(module_id, MITEE_NOTIFICATION_TYPE_DATAFRAG,\
				token, curr_size, data_buf[0], data_buf[1], data_buf[2]);
	}
	return 0;
}

EXPORT_SYMBOL_GPL(mitee_smc_notify_connect);
EXPORT_SYMBOL_GPL(mitee_smc_notify_signal);
EXPORT_SYMBOL_GPL(mitee_smc_notify_data);
