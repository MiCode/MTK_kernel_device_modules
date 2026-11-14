
#define pr_fmt(fmt)    "[ds28e30] %s: " fmt, __func__
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/string.h>

#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/regmap.h>
#include <linux/random.h>
#include "ds28e30.h"
#include "battery_auth_class.h"
#define INIT_ERR_VALUE              0xFFFFFF
#define ONE_WIRE_OUT_LOW            gpiod_direction_output(g_info->auth_dev->gpiod, 0)
#define ONE_WIRE_OUT_HIGH           gpiod_direction_output(g_info->auth_dev->gpiod, 1)
#define ONE_WIRE_CONFIG_IN          gpiod_direction_input(g_info->auth_dev->gpiod)
#define ONE_WIRE_GPIO_READ          gpiod_get_value(g_info->auth_dev->gpiod)
static struct mutex ds_cmd_lock;
static uint16_t CRC16;
static int page2_data_valid = 0;
static uint8_t page2_data[32];
static const short oddparity[16] = 
	{ 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };
static u8 cmd_len = 0, cmd_buffer[256];
struct ds_data {
	uint32_t last_counter;
	uint32_t last_cycle;
	uint32_t last_raw_soh;
	uint32_t last_ui_soh;
	struct device *dev;
	struct platform_device *pdev;
	const char *auth_name;
	struct auth_device *auth_dev;
};
static struct ds_data *g_info;
static void hex2asc(uint8_t *output, uint8_t *input, uint32_t len)
{
	uint32_t i;
	for (i = 0; i < len; i++) {
		output[i * 2] = ((input[i] & 0xF0) >> 4) + 0x30;
		output[i * 2 + 1] = (input[i] & 0x0F) + 0x30;
		if (output[i * 2] >= 0x3A)
			output[i * 2] += 7;
		if (output[i * 2 + 1] >= 0x3A)
			output[i * 2 + 1] += 7;
	}
}
static void hex_dump(const char *prefix, uint8_t *data_buf, int length)
{
	char buffer[128];
	if (!prefix) {
		pr_err("hex dump falild, prefix null\n");
		return;
	}
	if ((strlen(prefix) + length * 2) > sizeof(buffer))
		pr_err("hex dump falild, length too large\n");
	else {
		if (length == 0 || data_buf == NULL)
			pr_err("%s\n", prefix);
		else {
			memset(buffer, 0x00, sizeof(buffer));
			hex2asc(buffer, data_buf, length);
			pr_err("%s%s\n", prefix, buffer);
		}
	}
}

#ifdef MODULE
static char __chg_cmdline[1024];
static char *chg_cmdline = __chg_cmdline;

const char *chg_get_cmd(void)
{
	struct device_node * of_chosen = NULL;
	char *bootargs = NULL;

	if (__chg_cmdline[0] != 0)
		return chg_cmdline;

	of_chosen = of_find_node_by_path("/chosen");
	if (of_chosen) {
		bootargs = (char *)of_get_property(
					of_chosen, "bootargs", NULL);
		if (!bootargs)
			pr_err("%s: failed to get bootargs\n", __func__);
		else {
			strncpy(__chg_cmdline, bootargs, 512);
			pr_err("%s: bootargs: %s\n", __func__, bootargs);
		}
	} else
		pr_err("%s: failed to get /chosen \n", __func__);

	return chg_cmdline;
}

#else
const char *chg_get_cmd(void)
{
	return saved_command_line;
}
#endif

static inline void Delay_ns(uint32_t T)
{
	u64 pre, last;
	pre = ktime_get_ns();
	while (1) {
		last = ktime_get_ns();
		if (last - pre >= T)
			break;
	}
}
static void Delay_us(uint32_t T)
{
	Delay_ns(T * 1000);
}
static uint8_t ow_reset(void)
{
	uint8_t presence = 0xFF;
	ulong flags;
	raw_spin_lock_irqsave(&g_info->auth_dev->io_lock, flags);
	ONE_WIRE_OUT_LOW;
	Delay_us(48);
	ONE_WIRE_CONFIG_IN;
	Delay_us(7);
	presence = ONE_WIRE_GPIO_READ;
	Delay_us(50);
	raw_spin_unlock_irqrestore(&g_info->auth_dev->io_lock, flags);
	return presence;
}
static uint8_t read_bit(void)
{
	uint32_t vamm;
	ulong flags = 0;
	raw_spin_lock_irqsave(&g_info->auth_dev->io_lock, flags);
	ONE_WIRE_OUT_LOW;
	ONE_WIRE_CONFIG_IN;
	vamm = ONE_WIRE_GPIO_READ;
	raw_spin_unlock_irqrestore(&g_info->auth_dev->io_lock, flags);
	ONE_WIRE_OUT_HIGH;
	Delay_us(12);
	return vamm;
}
void write_bit(char bitval)
{
	ulong flags = 0;
	raw_spin_lock_irqsave(&g_info->auth_dev->io_lock, flags);
	ONE_WIRE_OUT_LOW;
	if (bitval) {
		ONE_WIRE_OUT_HIGH;
		raw_spin_unlock_irqrestore(&g_info->auth_dev->io_lock, flags);
		Delay_us(12);
	} else {
		Delay_us(6);
		ONE_WIRE_OUT_HIGH;
		raw_spin_unlock_irqrestore(&g_info->auth_dev->io_lock, flags);
		Delay_us(6);
	}
}
static uint8_t read_byte(void)
{
	uint8_t i;
	uint8_t value = 0;
	for (i = 0; i < 8; i++) {
	if (read_bit())
		value |= 0x01 << i;
	}
	return value;
}
static void write_byte(char val)
{
	uint8_t i;
	uint8_t temp;
	for (i = 0; i < 8; i++) {
		temp = val >> i;
		temp &= 0x01;
		write_bit(temp);
	}
}
uint16_t docrc16(uint16_t data)
{
	data = (data ^ (CRC16 & 0xff)) & 0xff;
	CRC16 >>= 8;
	if (oddparity[data & 0xf] ^ oddparity[data >> 4])
		CRC16 ^= 0xc001;
	data <<= 6;
	CRC16 ^= data;
	data <<= 1;
	CRC16 ^= data;
	return CRC16;
}
int standard_cmd_flow(uint8_t *write_buf, int write_len,
	      int delay_ms, int expect_read_len,
	      uint8_t *read_buf, int *read_len)
{
	int i, pkt_len = 0;
	uint8_t pkt[256] = { 0 };
	uint8_t command, param;
	mutex_lock(&ds_cmd_lock);
	if ((ow_reset()) != 0) {
		pr_err("<ds28e30> Failed to reset ds28e30!\n");
		ow_reset();
		mutex_unlock(&ds_cmd_lock);
		return ERROR_NO_DEVICE;
	}
	write_byte(CMD_SKIP_ROM);
	hex_dump("<ds28e30> <- CC", NULL, 0);
	pkt[pkt_len++] = CMD_START;
	pkt[pkt_len++] = write_len;
	memcpy(&pkt[pkt_len], write_buf, write_len);
	command = pkt[2];
	param = pkt[3];
	pkt_len += write_len;
	//send packet to DS28E30
	for (i = 0; i < pkt_len; i++)
		write_byte(pkt[i]);
	hex_dump("<ds28e30> <- ", pkt, pkt_len);
	// read two CRC bytes
	pkt[pkt_len++] = read_byte();
	pkt[pkt_len++] = read_byte();
	hex_dump("<ds28e30> -> ", &pkt[pkt_len - 2], 2);
	// check CRC16
	CRC16 = 0;
	for (i = 0; i < pkt_len; i++)
		docrc16(pkt[i]);
	if (CRC16 != 0xB001)
		pr_err("<ds28e30>: 1 crc error!\n");
	if (delay_ms > 0) {
		write_byte(0xAA);
		hex_dump("<ds28e30> <- AA", NULL, 0);
		udelay(1000 * delay_ms);
	}
	// read FF and the length byte
	pkt[0] = read_byte();
	pkt[1] = read_byte();
	*read_len = pkt[1];
	hex_dump("<ds28e30> -> ", &pkt[0], 2);
	for (i = 0; i < expect_read_len + 2; i++)
		read_buf[i] = read_byte();
	hex_dump("<ds28e30> -> ", read_buf, expect_read_len + 2);
	if (command == CMD_READ_MEM) {
		if (param == DC_PAGE) {
			memset(&read_buf[4], 0x00, 29);
			pr_err("force page%u 00\n", param);
			hex_dump("<ds28e30> -> ", read_buf, expect_read_len + 2);
		}
	}
	CRC16 = 0;
	docrc16(expect_read_len);
	for (i = 0; i < (expect_read_len + 2); i++)
		docrc16(read_buf[i]);
	if (expect_read_len != *read_len)
		pr_err("<ds28e30>: read length error:%02x\n", *read_len);
	if (CRC16 != 0xB001) {
		pr_err("<ds28e30>: 2 crc error!\n");
		mutex_unlock(&ds_cmd_lock);
		return DS_FALSE;
	} else {
		mutex_unlock(&ds_cmd_lock);
		return DS_TRUE;
	}
}
static int ds28e30_cmd_writeMemory(int pg, uint8_t *data)
{
	uint8_t write_buf[50];
	int write_len;
	uint8_t read_buf[64];
	int read_len;
	pr_err("enter\n");
	write_len = 0;
	write_buf[write_len++] = CMD_WRITE_MEM;
	write_buf[write_len++] = pg;
	memcpy(&write_buf[write_len], data, 32);
	write_len += 32;
	read_len = 1;
	if (standard_cmd_flow(write_buf, write_len, DELAY_DS28E30_EE_WRITE_TWM, read_len, read_buf, &read_len) == DS_TRUE && read_buf[0] == RESULT_SUCCESS) {
		pr_err("finish\n");
		return DS_TRUE;
	} else {
		pr_err("failed exit\n");
		return DS_FALSE;
	}
}
static int ds28e30_cmd_readMemory(int pg, uint8_t *data)
{
	uint8_t write_buf[10];
	int write_len;
	uint8_t read_buf[64];
	int read_len;
	pr_err("enter\n");
	write_len = 0;
	write_buf[write_len++] = CMD_READ_MEM;
	write_buf[write_len++] = pg;
	read_len = 33;
	if (standard_cmd_flow(write_buf, write_len, DELAY_DS28E30_EE_READ_TRM, read_len, read_buf, &read_len) == DS_TRUE && read_buf[0] == RESULT_SUCCESS) {
		memcpy(data, &read_buf[1], 32);
		pr_err("finish\n");
		return DS_TRUE;
	} else {
		pr_err("failed exit\n");
		return DS_FALSE;
	}
}
static int ds28e30_cmd_decrementCounter(void)
{
	uint8_t write_buf[255];
	uint8_t read_buf[255];
	int write_len = 0;
	int read_len = 1;
	write_buf[write_len++] = CMD_DECREMENT_CNT;
	if (standard_cmd_flow(write_buf, write_len, DELAY_DS28E30_EE_WRITE_TWM + 50, read_len, read_buf, &read_len) == DS_TRUE && read_buf[0] == RESULT_SUCCESS) {
		pr_err("finish\n");
		return DS_TRUE;
	} else {
		pr_err("failed exit\n");
		return DS_FALSE;
	}
}
static int ds28e30_get_page_data_retry(int page, uint8_t *data)
{
	int i;
	if (page >= MAX_PAGENUM)
		return DS_FALSE;
	for (i = 0; i < GET_USER_MEMORY_RETRY; i++) {
		if (ds28e30_cmd_readMemory(page, data) == DS_TRUE)
			return DS_TRUE;
		else {
			ow_reset();
		}
	}
	return DS_FALSE;
}
static int ds28e30_set_page_data_retry(int page, uint8_t *data)
{
	int i;
	if (page >= MAX_PAGENUM)
		return DS_FALSE;
	for (i = 0; i < GET_USER_MEMORY_RETRY; i++) {
		if (ds28e30_cmd_writeMemory(page, data) == DS_TRUE)
			return DS_TRUE;
		else {
			ow_reset();
		}
	}
	return DS_FALSE;
}
static int ds28e30_read_cycle_count(u8 *new_cycle)
{
	uint8_t page_buf[32];
	if (g_info->last_cycle != INIT_ERR_VALUE) {
		*new_cycle = g_info->last_cycle;
		pr_err("use last cycle count(%u)\n", g_info->last_cycle);
		return 0;
	} else {
		if (ds28e30_get_page_data_retry(PG_USER_EEPROM_2, page_buf) == DS_TRUE) {
			page2_data_valid = 1;
			memcpy(page2_data, page_buf, 32);
			*new_cycle = page_buf[30];
			g_info->last_cycle = *new_cycle;
			pr_err("use new cycle count(%u)\n", g_info->last_cycle);
			return 0;
		} else {
			g_info->last_cycle = INIT_ERR_VALUE;
			pr_err("read new cycle count failed\n");
			return -1;
		}
	}
}
static int ds28e30_write_cycle_count(u32 new_cycle)
{
	uint8_t page_buf[32];
	u8 cycle_count = new_cycle;
	if (page2_data_valid) {
		memcpy(page_buf, page2_data, 32);
	} else {
		if (ds28e30_get_page_data_retry(PG_USER_EEPROM_2, page_buf) == DS_FALSE) {
			pr_err("read page2 data failed\n");
			return -1;
		}
	}
	if (page_buf[30] == new_cycle) {
		pr_err("update cycle count:%u skip\n", new_cycle);
		return 0;
	}
	page_buf[30] = cycle_count;
	if (ds28e30_set_page_data_retry(PG_USER_EEPROM_2, page_buf) == DS_TRUE) {
		page2_data_valid = 1;
		page2_data[30] = cycle_count;
		g_info->last_cycle = cycle_count;
		pr_err("write cycle count(%u) succeeded\n", cycle_count);
		return 0;
	} else {
		g_info->last_cycle = INIT_ERR_VALUE;
		pr_err("write cycle count(%u) failed\n", cycle_count);
		return -1;
	}
}
static int ds28e30_read_dc_counter(uint32_t *counter_val)
{
	uint8_t page_buf[32];
	if (g_info->last_counter != INIT_ERR_VALUE) {
		*counter_val = g_info->last_counter;
		pr_err("use last dc counter(%u)\n", g_info->last_counter);
		return 0;
	} else {
		if (ds28e30_get_page_data_retry(DC_PAGE, page_buf) == DS_TRUE) {
			*counter_val = (page_buf[2] << 16) | (page_buf[1] << 8) | page_buf[0];
			g_info->last_counter = *counter_val;
			pr_err("use new dc counter(%u)\n", g_info->last_counter);
			return 0;
		} else {
			g_info->last_counter = INIT_ERR_VALUE;
			pr_err("read new dc counter failed\n");
			return -1;
		}
	}
}
static int ds28e30_decrease_counter(void)
{
	int i, ret = DS_FALSE;
	uint32_t  current_counter, last_new_counter;
	// update initial dc counter value
	last_new_counter = g_info->last_counter;
	if (last_new_counter == INIT_ERR_VALUE) {
		ret = ds28e30_read_dc_counter(&last_new_counter);
		if (ret < 0) {
			//ret = ST1WIRE_ACK_ERROR;
			pr_err("update new dc counter failed\n");
			return -1;
		}
	}
	for (i = 0; i < GET_USER_MEMORY_RETRY; i++) {
		// skip first recheck
		if (i > 0) {
			g_info->last_counter = INIT_ERR_VALUE;
			ret = ds28e30_read_dc_counter(&current_counter);
			if (ret < 0) {
				//ret = ST1WIRE_ACK_ERROR;
				pr_err("read new dc counter failed\n");
				return -1;
			}
			if (current_counter + 1 == last_new_counter) {
				pr_err("decrease new counter retry(%d)(%u)\n", i - 1, last_new_counter - 1);
				break;
			}
		}
		if (ds28e30_cmd_decrementCounter() == DS_FALSE)
			continue;
		g_info->last_counter = last_new_counter - 1;
		ret = DS_TRUE;
		pr_err("decrease dc counter retry(%d)(%u)\n", i, g_info->last_counter);
		break;
	}
	return (ret == DS_TRUE) ? 0 : -1;
}
static int ds28e30_set_raw_soh(struct auth_device *auth_dev, int raw_soh)
{
	u8 new_raw_soh = raw_soh;
	uint8_t page_buf[32];
	pr_info("enter\n");
	if (page2_data_valid) {
		memcpy(page_buf, page2_data, 32);
	} else {
		if (ds28e30_get_page_data_retry(PG_USER_EEPROM_2, page_buf) == DS_FALSE) {
			pr_err("read page2 data failed\n");
			return -1;
		}
	}
	if (page_buf[16] == new_raw_soh) {
		pr_err("update raw soh:%u skip\n", new_raw_soh);
		return 0;
	}
	page_buf[16] = new_raw_soh;
	if (ds28e30_set_page_data_retry(PG_USER_EEPROM_2, page_buf) == DS_TRUE) {
		page2_data_valid = 1;
		page2_data[16] = new_raw_soh;
		g_info->last_raw_soh = new_raw_soh;
		pr_err("write raw soh(%u) succeeded\n", new_raw_soh);
		return 0;
	} else {
		g_info->last_raw_soh = INIT_ERR_VALUE;
		pr_err("write raw soh(%u) failed\n", new_raw_soh);
		return -1;
	}
}
static int ds28e30_get_raw_soh(struct auth_device *auth_dev)
{
	u8 raw_soh;
	uint8_t page_buf[32];
	u32 new_raw_soh = INIT_ERR_VALUE;
	pr_info("enter\n");
	if (g_info->last_raw_soh != INIT_ERR_VALUE) {
		new_raw_soh = g_info->last_raw_soh;
		pr_err("use last raw soh(%u)\n", g_info->last_raw_soh);
	} else {
		if (ds28e30_get_page_data_retry(PG_USER_EEPROM_2, page_buf) == DS_TRUE) {
			page2_data_valid = 1;
			memcpy(page2_data, page_buf, 32);
			raw_soh = page_buf[16];
			g_info->last_raw_soh = new_raw_soh = raw_soh;
			pr_err("read new raw soh(%u) succeeded\n", raw_soh);
		} else {
			g_info->last_raw_soh = new_raw_soh = INIT_ERR_VALUE;
			pr_err("read new raw soh failed\n");
		}
	}
	if (new_raw_soh > 100) {
		if (!ds28e30_set_raw_soh(auth_dev, 100)) {
			g_info->last_raw_soh = 100;
			pr_err("use new raw soh(100)\n");
			return 100;
		} else {
			g_info->last_raw_soh = INIT_ERR_VALUE;
			pr_err("update new raw soh to 100 failed\n");
			return -1;
		}
	} else if (new_raw_soh == INIT_ERR_VALUE) {
		g_info->last_raw_soh = INIT_ERR_VALUE;
		pr_err("invalid raw soh\n");
		return -1;
	} else {
		g_info->last_raw_soh = new_raw_soh;
		pr_err("raw soh value(%u)\n", g_info->last_raw_soh);
		return new_raw_soh;
	}
}

static int ds28e30_get_first_usage_date(struct auth_device *auth_dev, u8 *first_usage_date, int len)
{
	int ret = 0, i = 0, invalid_count = 0;
	uint8_t page_data[32];

	ret = ds28e30_get_page_data_retry(PG_USER_EEPROM_1, page_data);
	if (ret == DS_FALSE) {
		pr_err("%s %d failed: %d\n", __func__, __LINE__, ret);
		return -1;
	}

	memcpy(first_usage_date, page_data, len);
	for (i = 0; i < len; i++) {
		if (first_usage_date[i] == 0xff)
			invalid_count++;
	}

	if (invalid_count >= 5) {
		pr_info("%s invalid value, set 0\n", __func__);
		memset(first_usage_date, '0', len);
	}

	for (i = 0; i < len; i++) {
		pr_info("%s data[%d]:0x%x\n", __func__, i, first_usage_date[i]);
	}

	return 0;
}
static int ds28e30_set_first_usage_date(struct auth_device *auth_dev, u8 *first_usage_date, int len)
{
	int ret = 0, i = 0;
	unsigned char buf[32];

	for (i = 0; i < len; i++) {
		buf[i] = first_usage_date[i];
	}
	pr_info("%s:buf:%s\n", __func__, buf);

	ret = ds28e30_set_page_data_retry(PG_USER_EEPROM_1, buf);
	if (ret == DS_FALSE) {
		pr_err("%s ds28e30_memory_write fail!\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < len; i++) {
		pr_info("%s buf[%d]:0x%x\n", __func__, i, buf[i]);
	}

	return 0;
}


static int ds28e30_get_cycle_count(struct auth_device *auth_dev, u32 * cycle_count)
{
	int ret;
	u8  get_count;
	u32 dc_count;
	pr_info("enter\n");
	*cycle_count = 0;
	ret = ds28e30_read_cycle_count(&get_count);
	if (ret) {
		pr_err("cycle count read failed\n");
		return -1;
	}
	if (get_count == 0xff)
		get_count = 0;
	pr_err("%s get_count:0x%2x\n", __func__, get_count);
	if (get_count >= 29) {
		if (ds28e30_read_dc_counter(&dc_count)) {
			pr_err("dc counter read failed\n");
			return -1;
		}
		*cycle_count = DC_INIT_VALUE - dc_count;
		if (*cycle_count < get_count)
			*cycle_count = get_count;
		pr_info("dc_count=%d\n", dc_count);
		pr_info("cycle_count=%d\n", *cycle_count);
	} else {
		*cycle_count = get_count;
	}
	pr_err("%s cycle_count:0x%x\n", __func__, *cycle_count);
	return 0;
}
static int ds28e30_set_cycle_count(struct auth_device *auth_dev, u32 set_cycle_count, u32 get_cycle)
{
	int ret = 0;
	uint32_t dc_counter;
	int i = set_cycle_count - get_cycle;
	pr_err("add_num:%d\n", i);
	if (i <= 0)
		return 0;
	pr_err("set_cycle_count: %d\n", set_cycle_count);
	if (set_cycle_count < 30) {
		ret = ds28e30_write_cycle_count(set_cycle_count);
		if (ret) {
			pr_err("update new cycle count:%d failed\n", set_cycle_count);
			return -1;
		}
	} else if (set_cycle_count >= 30 && get_cycle <= 29) {
		ret = ds28e30_write_cycle_count(29);
		if (ret) {
			pr_err("update cycle count:29 failed\n");
			return -1;
		}
		ret = ds28e30_read_dc_counter(&dc_counter);
		if (ret) {
			pr_err("read dc counter failed\n");
			return -1;
		}
		get_cycle = DC_INIT_VALUE - dc_counter;
		i = set_cycle_count - get_cycle;
		if (i <= 0)
			return 0;
		while (i--) {
			ret = ds28e30_decrease_counter();
			if (ret){
				pr_err("decrease dc counter failed at line %d\n", __LINE__);
				return -1;
			}
		}
	} else {
		while (i--) {
			ret = ds28e30_decrease_counter();
			if (ret) {
				pr_err("decrease dc counter failed at line %d\n", __LINE__);
				return -1;
			}
		}
	}
	return 0;
}
static int ds28e30_get_ui_soh(struct auth_device *auth_dev, u8 *ui_soh_data, int len)
{
	int ret = 0, i = 0, invalid_count = 0;
	uint8_t page_data[32];

	ret = ds28e30_get_page_data_retry(PG_USER_EEPROM_2, page_data);
	if (ret == DS_FALSE) {
		pr_err("%s %d failed: %d\n", __func__, __LINE__, ret);
		return -EINVAL;
	}

	memcpy(ui_soh_data, page_data, len);
	for (i = 0; i < len; i++) {
		if (ui_soh_data[i] == 0xff)
			invalid_count++;
	}

	if (invalid_count >= 5) {
		pr_err("%s invalid value, set 0\n", __func__);
		memset(ui_soh_data, 0, len);
	}

	pr_info("%s ui_soh:\n", __func__);

	for (i = 0; i < len; i++) {
		pr_info("%s data[%d]:0x%x\n", __func__, i, ui_soh_data[i]);
	}

	return 0;
}

static int ds28e30_set_ui_soh(struct auth_device *auth_dev, u8 *ui_soh_data, int len)
{
	int ret = 0, i = 0;
	unsigned char buf[32];

	ret = ds28e30_get_page_data_retry(PG_USER_EEPROM_2, buf);
	if (ret == DS_FALSE) {
		pr_err("%s read fail\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < len; i++) {
		buf[i] = ui_soh_data[i];
	}

	ret =  ds28e30_set_page_data_retry(PG_USER_EEPROM_2, buf);
	if (ret == DS_FALSE) {
		pr_err("%s slg_memory_write fail!\n", __func__);
		return -EINVAL;
	}

	pr_info("%s ui_soh:\n", __func__);

	for (i = 0; i < len; i++) {
		pr_info("%s buf[%d]:0x%x\n", __func__, i, buf[i]);
	}

	return 0;
}
struct auth_ops ds28e30_auth_ops = {
	.get_first_usage_date = ds28e30_get_first_usage_date,
	.set_first_usage_date = ds28e30_set_first_usage_date,
	.get_cycle_count = ds28e30_get_cycle_count,
	.set_cycle_count = ds28e30_set_cycle_count,
	.get_ui_soh = ds28e30_get_ui_soh,
	.set_ui_soh = ds28e30_set_ui_soh,
	.get_raw_soh = ds28e30_get_raw_soh,
	.set_raw_soh = ds28e30_set_raw_soh,
};
// cat /sys/bus/platform/devices/maxim_ds28e30/cmdbuf
static ssize_t ds_cmdbuf_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, len = 0;
	for (i = 0; i < cmd_len; i++)
		len += snprintf(buf + len, PAGE_SIZE, "%02X ", cmd_buffer[i]);
	len += snprintf(buf + len, PAGE_SIZE, "\n");
	return len;
}
// echo "32 50 44 00" > /sys/bus/platform/devices/maxim_ds28e30/cmdbuf
// 32(DEX): read data length
// 50(DEX): delay(ms) before read from secret ic
// 44(HEX): command byte
// 00(HEX): command payload
static ssize_t ds_cmdbuf_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int     i, read_len;
	ulong   tx_hex[64];
	char    *tx_ptr, *hexstr, txbuf[128];
	ssize_t len = 0, num = 0;
	u8      wait_time, tx_hex_u8[64];
	// parse data package
	cmd_len = 0;
	tx_ptr = txbuf;
	sscanf(buf, "%s", &cmd_len);
	sscanf(&buf[3], "%s", &wait_time);
	memset(txbuf, 0x00, sizeof(txbuf));
	strcpy(txbuf, &buf[6]);
	memset(cmd_buffer, 0x00, sizeof(cmd_buffer));
	while ((hexstr = strsep(&tx_ptr, " ")) != NULL) {
		if (kstrtoul(hexstr, 16, &tx_hex[num]) < 0) {
			cmd_len = 0;
			pr_err("parse byte(%zd) failed\n", num);
			return -EINVAL;
		}
		num++;
	}
	// dump tx buffer hex data
	memset(txbuf, 0x00, sizeof(txbuf));
	len += snprintf(txbuf, sizeof(txbuf), "tx data:");
	for (i = 0; i < num; i++)
		len += snprintf(txbuf + len, PAGE_SIZE, " %02lX", tx_hex[i]);
	pr_info("%s\n", txbuf);
	for (i = 0; i < num; i++)
		tx_hex_u8[i] = tx_hex[i];
	if (standard_cmd_flow(tx_hex_u8, num, wait_time, cmd_len, cmd_buffer, &read_len) != DS_TRUE) {
		cmd_len = 0;
		pr_err("ds28e30 write cmdbuf failed\n");
		return -EINVAL;
	}
	return size;
}
static struct device_attribute dev_attr_ds_cmdbuf =
	__ATTR(cmdbuf, S_IRUSR | S_IWUSR, ds_cmdbuf_show, ds_cmdbuf_store);
static int ds28e30_probe(struct platform_device *pdev)
{
	//int ret = 0;
	struct ds_data *info;
	char *secret_ic_str = NULL;
	int secret_ic = -1;
	pr_err("%s enter\n", __func__);
	/* Only register the secret ic probed in lk successfully */
	secret_ic_str = strstr(chg_get_cmd(), SECRET_IC);
	if (secret_ic_str) {
		sscanf(secret_ic_str, SECRET_IC"%d", &secret_ic);
		if (secret_ic != 3) {
			pr_err("%s: slg or slg2 secret ic didn't probed in lk\n", __func__);
			return -ENODEV;
		}
	} else {
		pr_err("%s: Not found kernel commandline option 'secret_ic='\n", __func__);
		return -ENODEV;
	}
	info = devm_kzalloc(&(pdev->dev), sizeof(struct ds_data), GFP_KERNEL);
	if (!info) {
		pr_err("%s alloc mem fail\n", __func__);
		return -ENOMEM;
	}
	if ((!pdev->dev.of_node
		|| !of_device_is_available(pdev->dev.of_node)))
		return -ENODEV;
	mutex_init(&ds_cmd_lock);
	info->dev = &pdev->dev;
	info->pdev = pdev;
	platform_set_drvdata(pdev, info);
	/* As only one secret ic will be register, use the same auth name */
	// ret = of_property_read_string(pdev->dev.of_node,
	// 	"auth_name", &info->auth_name);
	// if (ret < 0) {
	// 	pr_info("%s can not find auth name(%d)\n", __func__, ret);
	// 	info->auth_name = "third_supplier";
	// }
	info->auth_name = "secret_ic";
	info->auth_dev = auth_device_register(info->auth_name, NULL, info, &ds28e30_auth_ops);
	if (IS_ERR_OR_NULL(info->auth_dev)) {
		pr_err("%s failed to register auth device\n", __func__);
		return PTR_ERR(info->auth_dev);
	}
	if (device_create_file(&pdev->dev, &dev_attr_ds_cmdbuf))
		pr_err("create %s file failed\n", dev_attr_ds_cmdbuf.attr.name);
	g_info = info;
	return 0;
}
static void ds28e30_remove(struct platform_device *pdev)
{
	mutex_destroy(&ds_cmd_lock);
	return;
}
static const struct of_device_id ds28e30_of_ids[] = {
	{ .compatible = "maxim,ds28e30" },
	{ },
};
static struct platform_driver ds28e30_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "maxim,ds28e30",
		.of_match_table = ds28e30_of_ids,
	},
	.probe = ds28e30_probe,
	.remove = ds28e30_remove,
};
static int __init ds28e30_init(void)
{
	pr_err("%s enter\n", __func__);
	return platform_driver_register(&ds28e30_driver);
}
static void __exit ds28e30_exit(void)
{
	pr_err("%s enter\n", __func__);
	platform_driver_unregister(&ds28e30_driver);
}
module_init(ds28e30_init);
module_exit(ds28e30_exit);
MODULE_LICENSE("GPL");

