#ifndef __OCA72XXX_DEVICE_H__
#define __OCA72XXX_DEVICE_H__
#include <linux/version.h>
#include <linux/kernel.h>
#include <sound/control.h>
#include <sound/soc.h>
#include "oca72xxx_acf_bin.h"

#define OCA72XXX_PID_9B_PRODUCT_MAX	(1)
#define OCA72XXX_PID_18_PRODUCT_MAX	(1)
#define OCA72XXX_PID_39_PRODUCT_MAX	(3)
#define OCA72XXX_PID_59_3X9_PRODUCT_MAX	(2)
#define OCA72XXX_PID_59_5X9_PRODUCT_MAX	(4)
#define OCA72XXX_PID_5A_PRODUCT_MAX	(6)
#define OCA72XXX_PID_76_PROFUCT_MAX	(5)
#define OCA72XXX_PID_60_PROFUCT_MAX	(5)
#define OCA_PRODUCT_NAME_LEN		(8)

#define OCA_GPIO_HIGHT_LEVEL		(1)
#define OCA_GPIO_LOW_LEVEL		(0)

#define OCA_I2C_RETRIES			(5)
#define OCA_I2C_RETRY_DELAY		(2)
#define OCA_I2C_READ_MSG_NUM		(2)

#define OCA_READ_CHIPID_RETRIES		(5)
#define OCA_READ_CHIPID_RETRY_DELAY	(2)
#define OCA_DEV_REG_CHIPID		(0x00)

#define OCA_DEV_REG_INVALID_MASK		(0xff)

#define OCA_NO_RESET_GPIO		(-1)

#define OCA_PID_9B_BIN_REG_CFG_COUNT	(10)

#define OCA72XXX_DELAY_REG_ADDR		(0xFE)
#define OCA72XXX_REG_DELAY_TIME		(1000)

#define OCA_BOOST_VOLTAGE_MIN		(0x00)

#define OCA_REG_NONE		(0xFF)
/********************************************
 *
 * oca72xxx devices attributes
 *
 *******************************************/
struct oca_device;

struct oca_device_ops {
	int (*pwr_on_func)(struct oca_device *oca_dev, struct oca_data_container *data);
	int (*pwr_off_func)(struct oca_device *oca_dev, struct oca_data_container *data);
};

enum oca_dev_chipid {
	OCA_DEV_CHIPID_18 = 0x18,
	OCA_DEV_CHIPID_39 = 0x39,
	OCA_DEV_CHIPID_59 = 0x59,
	OCA_DEV_CHIPID_69 = 0x69,
	OCA_DEV_CHIPID_5A = 0x09,
	OCA_DEV_CHIPID_9A = 0x9A,
	OCA_DEV_CHIPID_9B = 0x9B,
	OCA_DEV_CHIPID_76 = 0x11,
	OCA_DEV_CHIPID_60 = 0x60,
};

enum oca_dev_hw_status {
	OCA_DEV_HWEN_OFF = 0,
	OCA_DEV_HWEN_ON,
	OCA_DEV_HWEN_INVALID,
	OCA_DEV_HWEN_STATUS_MAX,
};

enum oca_dev_soft_off_enable {
	OCA_DEV_SOFT_OFF_DISENABLE = 0,
	OCA_DEV_SOFT_OFF_ENABLE = 1,
};

enum oca_dev_soft_rst_enable {
	OCA_DEV_SOFT_RST_DISENABLE = 0,
	OCA_DEV_SOFT_RST_ENABLE = 1,
};

enum oca_reg_receiver_mode {
	OCA_NOT_REC_MODE = 0,
	OCA_IS_REC_MODE = 1,
};

enum oca_reg_voltage_status {
	OCA_VOLTAGE_LOW = 0,
	OCA_VOLTAGE_HIGH,
};

struct oca_mute_desc {
	uint8_t addr;
	uint8_t enable;
	uint8_t disable;
	uint16_t mask;
};

struct oca_soft_rst_desc {
	int len;
	unsigned char *access;
};

struct oca_esd_check_desc {
	uint8_t first_update_reg_addr;
	uint8_t first_update_reg_val;
};

struct oca_rec_mode_desc {
	uint8_t addr;
	uint8_t enable;
	uint8_t disable;
	uint8_t mask;
};

struct oca_voltage_desc {
	uint8_t addr;
	uint8_t vol_max;
	uint8_t vol_min;
};

struct oca_device {
	uint8_t i2c_addr;
	uint8_t chipid;
	uint8_t soft_rst_enable;
	uint8_t soft_off_enable;
	uint8_t is_rec_mode;
	int hwen_status;
	int i2c_bus;
	int rst_gpio;
	int reg_max_addr;
	int product_cnt;
	const char **product_tab;
	const unsigned char *reg_access;

	struct device *dev;
	struct i2c_client *i2c;
	struct oca_mute_desc mute_desc;
	struct oca_soft_rst_desc soft_rst_desc;
	struct oca_esd_check_desc esd_desc;
	struct oca_rec_mode_desc rec_desc;
	struct oca_voltage_desc vol_desc;

	struct oca_device_ops ops;
};


int oca72xxx_dev_i2c_write_byte(struct oca_device *oca_dev,
			uint8_t reg_addr, uint8_t reg_data);
int oca72xxx_dev_i2c_read_byte(struct oca_device *oca_dev,
			uint8_t reg_addr, uint8_t *reg_data);
int oca72xxx_dev_i2c_read_msg(struct oca_device *oca_dev,
	uint8_t reg_addr, uint8_t *data_buf, uint32_t data_len);
int oca72xxx_dev_i2c_write_bits(struct oca_device *oca_dev,
	uint8_t reg_addr, uint8_t mask, uint8_t reg_data);
void oca72xxx_dev_soft_reset(struct oca_device *oca_dev);
void oca72xxx_dev_hw_pwr_ctrl(struct oca_device *oca_dev, bool enable);
int oca72xxx_dev_default_pwr_on(struct oca_device *oca_dev,
			struct oca_data_container *profile_data);
int oca72xxx_dev_default_pwr_off(struct oca_device *oca_dev,
			struct oca_data_container *profile_data);
int oca72xxx_dev_esd_reg_status_check(struct oca_device *oca_dev);
int oca72xxx_dev_check_reg_is_rec_mode(struct oca_device *oca_dev);
int oca72xxx_dev_init(struct oca_device *oca_dev);

#endif
