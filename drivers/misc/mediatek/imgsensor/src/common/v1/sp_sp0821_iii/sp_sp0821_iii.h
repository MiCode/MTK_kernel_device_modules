#ifndef __SP0821_H__
#define __SP0821_H__

#define SP0821_I2C_RETRIES		(1)
#define SP0821_I2C_RETRY_DELAY		(2)

/********************************************
 *
 * sp0821 struct
 *
 *******************************************/

struct sp0821 {
	uint8_t i2c_seq;
	uint8_t i2c_addr;
	uint8_t hwen_flag;
	char bus_num[2];
	int reset_gpio;
        int reset_gpio1;
	struct i2c_client *i2c_client;
	struct device *dev;
	struct pinctrl *sp0821_pinctrl;
	struct regulator *vcama;
	struct regulator *vcamio;
};

/********************************************
 *
 * print information control
 *
 *******************************************/
#define qvga_dev_err(dev, format, ...) \
			pr_err("[%s]" format, dev_name(dev), ##__VA_ARGS__)

#define qvga_dev_info(dev, format, ...) \
			pr_info("[%s]" format, dev_name(dev), ##__VA_ARGS__)

#define qvga_dev_dbg(dev, format, ...) \
			pr_debug("[%s]" format, dev_name(dev), ##__VA_ARGS__)

#endif
