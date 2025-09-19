// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
*/

#include <asm-generic/errno-base.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/semaphore.h>
#include <linux/sched/clock.h>
#include <uapi/linux/sched/types.h>

#include "sc96281.h"
#include "sc96281_mtp_program.h"
#include "sc96281_pgm.h"
//#include "sc96281_firmware.h"

static int log_level = 1;
#define sc96281_mtp_err(fmt, ...)							\
do {										\
	if (log_level >= 0)							\
		printk(KERN_ERR "[SC96281_MTP] " fmt, ##__VA_ARGS__);	\
} while (0)
#define sc96281_mtp_info(fmt, ...)							\
do {										\
	if (log_level >= 1)							\
		printk(KERN_ERR "[SC96281_MTP] " fmt, ##__VA_ARGS__);	\
} while (0)
#define sc96281_mtp_dbg(fmt, ...)							\
do {										\
	if (log_level >= 2)							\
		printk(KERN_ERR "[SC96281_MTP] " fmt, ##__VA_ARGS__);	\
} while (0)

#ifdef USING_BIN_FILE
#define FIRMWARE_FILE_PATH     "/data/misc/sc/sc96281.bin"
#endif

#define FIRMWARE_SIZE          (32 * 1024) /**< 32K Bytes */
#define SRAM_BASE_ADDRESS      0x20000000

#define CRC_POLY               0x04c11db7

#define CHIP_SIZE              (32 * 1024)
//#define SECTOR_SIZE            256
#define CRC_SECTOR             64
#define PGM_WORD               4
#define PGM_INFO_SIZE          64

#define INFO_LENGTH            128

#define REG_TM                 0x0011
#define TM_ST_BIT              0
#define TM_DATA                0xED

#define REG_ATE                0x60000000

#define REG_RST                0xFFE4
#define RST_PW0                0x96
#define RST_PW1                0x00

#define REG_IDLE               0xFFE7
#define IDLE_PW0               0x5AA5
#define IDLE_PW1               0x00
#define IDLE_ADDR_HIGH         0x4000
#define IDLE_ADDR_LOW1         0x7008
#define IDLE_ADDR_LOW2         0x7004
#define IDLE_VAL               0x5F4E585F

#define REG_DIG                0x007F

#define ATE_BOOT_BIT           3
//REG_DIG
#define DIG_TM_BIT             0

#define DIG_PW0                0x51
#define DIG_PW1                0x49
#define DIG_PW2                0x57
#define DIG_PW3                0x45
#define DIG_PW4                0x4E

#define ADDR_HIGH_BASE         0xFFE0

#define PGM_PKG_SIZE           64
#define FOUR_BYTE_ALIGN        4
#define HIGH_ADDR_NUM          2

static int __sc96281_read_block(struct sc96281_chg *sc, uint16_t reg, uint16_t length, uint8_t *data)
{
    int ret;

    ret = regmap_bulk_read(sc->regmap, reg, data, length);
    if (ret < 0) {
        sc96281_mtp_err("i2c read fail: can't read from reg 0x%04X\n", reg);
    }

    return ret;
}

static int __sc96281_write_block(struct sc96281_chg *sc, uint16_t reg, uint16_t length, uint8_t *data)
{
    int ret;

    ret = regmap_bulk_write(sc->regmap, reg, data, length);
    if (ret < 0) {
        sc96281_mtp_err("i2c write fail: can't write 0x%04X: %d\n", reg, ret);
    }

    return ret;
}

static int sc96281_read_block(struct sc96281_chg *sc, uint16_t reg, uint8_t *data, uint16_t len)
{
    int ret;

    mutex_lock(&sc->i2c_rw_lock);
    ret = __sc96281_read_block(sc, reg, len, data);
    mutex_unlock(&sc->i2c_rw_lock);

    return ret;
}

static int sc96281_write_block(struct sc96281_chg *sc, uint16_t reg, uint8_t *data, uint16_t len)
{
    int ret;

    mutex_lock(&sc->i2c_rw_lock);
    ret = __sc96281_write_block(sc, reg, len, data);
    mutex_unlock(&sc->i2c_rw_lock);

    return ret;
}

static int SC96281_write_byte(struct sc96281_chg *sc, uint16_t reg, uint8_t data)
{
    int ret;

    mutex_lock(&sc->i2c_rw_lock);
    ret = __sc96281_write_block(sc, reg, 1, &data);
    mutex_unlock(&sc->i2c_rw_lock);

    return ret;
}

static int SC96281_read_register(struct sc96281_chg *sc, uint16_t reg, uint8_t *data, uint8_t len)
{
    int ret;
    uint8_t r_data[4] = {0};
    uint8_t i;

    if ((reg % 4) + len > 4)
    {
        sc96281_mtp_err("input data error\n");
        return -1;
    }

    ret = sc96281_read_block(sc, reg - (reg % 4), r_data, 4);
    if (ret) {
        return ret;
    }

    for (i = 0; i < len; i++) {
        *(data + i) = r_data[(reg % 4) + i];
    }
    return ret;
}

static int sc96281_write_register(struct sc96281_chg *sc, uint16_t reg, uint8_t *data, uint8_t len)
{
    int ret;
    uint8_t r_data[4] = {0};
    uint8_t i;

    if ((reg % 4) + len > 4)
    {
        sc96281_mtp_err("input data error\n");
        return -1;
    }

    ret = sc96281_write_block(sc, reg - (reg % 4), r_data, 4);

    for (i = 0; i < len; i++) {
        r_data[(reg % 4) + i] = *(data + i);
    }

    ret = sc96281_write_block(sc, reg - (reg % 4), r_data, 4);
    return ret;
}

static uint32_t sc96281_func_crc32(uint32_t data, uint32_t crc_init)
{
    uint32_t crc_poly = CRC_POLY;
    uint8_t i = 0;


    for(i = 0; i < 32; i++) {
        crc_init = (crc_init << 1) ^ ((((crc_init >> 31) & 0x01) ^ ((data >> i) & 0x01))
                        == 0x01 ? 0xFFFFFFFF & crc_poly : 0x00000000 & crc_poly);
    }

    return (uint32_t)crc_init;
}

/*********************pgm api**********************/
__maybe_unused static uint16_t pgm_xor(uint8_t *buffer, uint16_t len) {
    uint16_t val = 0x00;
    uint16_t i;

    for (i=0; i<len; i=i+2) {
        val ^= (uint16_t)buffer[i] | (((uint16_t)buffer[i+1])<<8);
    }

    return (val & 0xffff);
}

__maybe_unused static uint8_t pgm_state(struct sc96281_chg *sc) {
    uint16_t i;
    uint8_t state[4] = {0};
    uint16_t sram_addr_h = SRAM_BASE >> 16;
    uint16_t sram_addr_l = SRAM_BASE & 0xFFFF;
    uint8_t addr_high[2] = {0};

    addr_high[0] = sram_addr_h >> 8;
    addr_high[1] = sram_addr_h & 0XFF;

    sc96281_write_register(sc, ADDR_HIGH_BASE, addr_high, 2);
    for (i = 0; i < 1000; i++) {
        msleep(1);
        SC96281_read_register(sc, sram_addr_l + PGM_STATE_ADDR, state, 4);
        if ((state[0] != PGM_STATE_BUSY) && (state[0] != PGM_STATE_NONE)) {
            return state[0];
        }
    }

    return PGM_STATE_TIMEOUT;
}

__maybe_unused static int pgm_sector_erase(struct sc96281_chg *sc, uint32_t addr, uint16_t length) {
    uint8_t state = 0;
    pgmPktType pgm = {0};
    uint16_t sram_addr_h = SRAM_BASE >> 16;
    uint16_t sram_addr_l = SRAM_BASE & 0xFFFF;
    uint8_t addr_high[2] = {0};
    uint8_t write_single_data[1] = {PGM_CMD_SECTOR_ERASE};

    addr_high[0] = sram_addr_h >> 8;
    addr_high[1] = sram_addr_h & 0XFF;

    sc96281_write_register(sc, ADDR_HIGH_BASE, addr_high, 2);

    pgm.type.addr = (uint16_t)(addr /PGM_WORD);
    pgm.type.len = length /PGM_WORD;
    pgm.type.xor = pgm_xor(&pgm.value[PGM_XOR_INDEX], PGM_XOR_LEN);
    pgm.type.xor ^= PGM_CMD_SECTOR_ERASE<<8;
    sc96281_write_register(sc, sram_addr_l + PGM_STATE_ADDR, &pgm.value[0], PGM_HEADER_SIZE);
    sc96281_write_register(sc, sram_addr_l + PGM_CMD_ADDR, write_single_data, 1);

    state = pgm_state(sc);
    if (state != PGM_STATE_DONE) {
        sc96281_mtp_err("Error: pgm sector erase state %02x\n",state);
        return -EINVAL;
    }
    return 0;
}

__maybe_unused static int pgm_chip_erase(struct sc96281_chg *sc) {
    uint8_t state = 0;
    pgmPktType pgm = {0};
    uint16_t sram_addr_h = SRAM_BASE >> 16;
    uint16_t sram_addr_l = SRAM_BASE & 0xFFFF;
    uint8_t addr_high[2] = {0};
    uint8_t pgm_erase[4] = {0};

    addr_high[0] = sram_addr_h >> 8;
    addr_high[1] = sram_addr_h & 0XFF;

    sc96281_write_register(sc, ADDR_HIGH_BASE, addr_high, 2);

    pgm.type.xor = pgm_xor(&pgm.value[PGM_XOR_INDEX], PGM_XOR_LEN);
    pgm.type.xor ^= PGM_CMD_CHIP_ERASE<<8;
    sc96281_write_block(sc, sram_addr_l + PGM_STATE_ADDR, &pgm.value[0], PGM_HEADER_SIZE);
    SC96281_read_register(sc, sram_addr_l + PGM_STATE_ADDR, pgm_erase, 4);
    SC96281_read_register(sc, sram_addr_l + PGM_STATE_ADDR, pgm_erase, 4);

    pgm_erase[1] = 0xce;
    sc96281_write_register(sc, sram_addr_l + PGM_STATE_ADDR, pgm_erase, 4);

    state = pgm_state(sc);
    if (state != PGM_STATE_DONE) {
        sc96281_mtp_err("Error: pgm chip erase state %02x\n",state);
        return -EINVAL;
    }
    return 0;
}

__maybe_unused static int pgm_trim_erase(struct sc96281_chg *sc) {
    uint8_t state = 0;
    pgmPktType pgm = {0};
    uint16_t sram_addr_h = SRAM_BASE >> 16;
    uint16_t sram_addr_l = SRAM_BASE & 0xFFFF;
    uint8_t addr_high[2] = {0};
    uint8_t write_single_data[1] = {PGM_CMD_TRIM_ERASE};

    addr_high[0] = sram_addr_h >> 8;
    addr_high[1] = sram_addr_h & 0XFF;

    sc96281_write_register(sc, ADDR_HIGH_BASE, addr_high, 2);

    pgm.type.xor = pgm_xor(&pgm.value[PGM_XOR_INDEX], PGM_XOR_LEN);
    pgm.type.xor ^= PGM_CMD_TRIM_ERASE<<8;
    sc96281_write_register(sc, sram_addr_l + PGM_STATE_ADDR, &pgm.value[0], PGM_HEADER_SIZE);
    sc96281_write_register(sc, sram_addr_l + PGM_CMD_ADDR, write_single_data, 1);

    state = pgm_state(sc);
    if (state != PGM_STATE_DONE) {
        sc96281_mtp_err("Error: pgm trim erase state %02x\n",state);
        return -EINVAL;
    }
    return 0;
}

__maybe_unused static int pgm_cust_erase(struct sc96281_chg *sc) {
    uint8_t state = 0;
    pgmPktType pgm = {0};
    uint16_t sram_addr_h = SRAM_BASE >> 16;
    uint16_t sram_addr_l = SRAM_BASE & 0xFFFF;
    uint8_t addr_high[2] = {0};
    uint8_t write_single_data[1] = {PGM_CMD_CUST_ERASE};

    addr_high[0] = sram_addr_h >> 8;
    addr_high[1] = sram_addr_h & 0XFF;

    sc96281_write_register(sc, ADDR_HIGH_BASE, addr_high, 2);

    pgm.type.xor = pgm_xor(&pgm.value[PGM_XOR_INDEX], PGM_XOR_LEN);
    pgm.type.xor ^= PGM_CMD_CUST_ERASE<<8;
    sc96281_write_register(sc, sram_addr_l + PGM_STATE_ADDR, &pgm.value[0], PGM_HEADER_SIZE);
    sc96281_write_register(sc, sram_addr_l + PGM_CMD_ADDR, write_single_data, 1);

    state = pgm_state(sc);
    if (state != PGM_STATE_DONE) {
        sc96281_mtp_err("Error: pgm cut erase state %02x\n",state);
        return -EINVAL;
    }
    return 0;
}

__maybe_unused static int pgm_set_margin(struct sc96281_chg *sc, uint8_t buffer) {
    uint8_t state = 0;
    pgmPktType pgm = {0};
    uint16_t sram_addr_h = SRAM_BASE >> 16;
    uint16_t sram_addr_l = SRAM_BASE & 0xFFFF;
    uint8_t addr_high[2] = {0};
    uint8_t margin_cmd[4] = {0};

    addr_high[0] = sram_addr_h >> 8;
    addr_high[1] = sram_addr_h & 0XFF;

    sc96281_write_register(sc, ADDR_HIGH_BASE, addr_high, 2);

    pgm.type.addr = 0x00;
    pgm.type.len = 1;
    memcpy(pgm.type.msg, &buffer, 1);

    pgm.type.xor = pgm_xor(&pgm.value[PGM_XOR_INDEX], PGM_XOR_LEN + 4);
    pgm.type.xor ^= PGM_CMD_MARGIN << 8;

    sc96281_write_block(sc, sram_addr_l + PGM_STATE_ADDR, &pgm.value[0], 4 + PGM_HEADER_SIZE);
    SC96281_read_register(sc, sram_addr_l + PGM_STATE_ADDR, margin_cmd, 4);
    SC96281_read_register(sc, sram_addr_l + PGM_STATE_ADDR, margin_cmd, 4);

    margin_cmd[1] = PGM_CMD_MARGIN;
    sc96281_write_register(sc, sram_addr_l + PGM_STATE_ADDR, margin_cmd, 4);

    state = pgm_state(sc);
    if (state != PGM_STATE_DONE) {
        sc96281_mtp_err("Error: pgm access state %02x\n", state);
        return -EINVAL;
    }
    return 0;
}

__maybe_unused static uint16_t count_num(uint8_t * buffer, uint16_t len, uint8_t num) {
    uint16_t i;
    uint16_t count = 0;

    for (i=0; i<len; i++) {
        if (buffer[i] == num) {
            count++;
        }
    }

    return count;
}

__maybe_unused static int SC96281_get_pgm_info(struct sc96281_chg *sc, char * info) {
    uint8_t state = 0;
    uint8_t i;
    uint8_t index;
    pgmPktType pgm = {0};
    uint16_t msg_len;
    uint16_t sram_addr_h = SRAM_BASE >> 16;
    uint16_t sram_addr_l = SRAM_BASE & 0xFFFF;
    uint8_t addr_high[2] = {0};
    uint8_t pgm_info[4] = {0};


    addr_high[0] = sram_addr_h >> 8;
    addr_high[1] = sram_addr_h & 0XFF;

    sc96281_write_register(sc, ADDR_HIGH_BASE, addr_high, 2);

    pgm.type.xor = pgm_xor(&pgm.value[PGM_XOR_INDEX], PGM_XOR_LEN);
    pgm.type.xor ^= PGM_CMD_INFO<<8;
    sc96281_write_register(sc, sram_addr_l + PGM_STATE_ADDR, &pgm.value[0], 4);
    sc96281_write_register(sc, sram_addr_l + PGM_STATE_ADDR + 4, &pgm.value[4], 4);
    SC96281_read_register(sc, sram_addr_l + PGM_STATE_ADDR, pgm_info, 4);
    SC96281_read_register(sc, sram_addr_l + PGM_STATE_ADDR, pgm_info, 4);

    pgm_info[1] = 0xA0;
    sc96281_write_register(sc, sram_addr_l + PGM_STATE_ADDR, pgm_info, 4);

    state = pgm_state(sc);
    if (state == PGM_STATE_DONE) {
        SC96281_read_register(sc, sram_addr_l + PGM_STATE_ADDR, (uint8_t *)(&pgm.value[0]), 4);
        SC96281_read_register(sc, sram_addr_l + PGM_STATE_ADDR + 4, (uint8_t *)(&pgm.value[4]), 4);
        sc96281_mtp_err("pgm.type.len = %d\n", pgm.type.len);
        msg_len = pgm.type.len;
        sc96281_read_block(sc, sram_addr_l + PGM_MSG_ADDR, pgm.type.msg, msg_len);
        if (pgm_xor((uint8_t *)pgm.value, msg_len + PGM_HEADER_SIZE) == 0x00) {
            index = 0;
            for (i=0; i<msg_len; i++) {
                if (pgm.type.msg[i] != 0) {
                    info[index] = pgm.type.msg[i];
                    index++;
                }
            }
        }
    }
    else {
        sc96281_mtp_err("Error: pgm info state %02x\n",state);
        return -EINVAL;
    }
    return 0;
}

__maybe_unused static int pgm_access(struct sc96281_chg *sc, uint8_t * key_buffer, uint8_t key_len) {
    uint8_t state = 0;
    pgmPktType pgm = {0};
    uint16_t sram_addr_h = SRAM_BASE >> 16;
    uint16_t sram_addr_l = SRAM_BASE & 0xFFFF;
    uint8_t addr_high[2] = {0};
    uint8_t write_single_data[1] = {PGM_CMD_AUTH};

    addr_high[0] = sram_addr_h >> 8;
    addr_high[1] = sram_addr_h & 0XFF;

    sc96281_write_register(sc, ADDR_HIGH_BASE, addr_high, 2);

    pgm.type.addr = 0x00;
    pgm.type.len = key_len/PGM_WORD;
    memcpy(pgm.type.msg, key_buffer, key_len);

    pgm.type.xor = pgm_xor(&pgm.value[PGM_XOR_INDEX], PGM_XOR_LEN);
    pgm.type.xor ^= PGM_CMD_AUTH<<8;
    sc96281_write_register(sc, sram_addr_l + PGM_STATE_ADDR, &pgm.value[0], key_len+PGM_HEADER_SIZE);
    sc96281_write_register(sc, sram_addr_l + PGM_CMD_ADDR, write_single_data, 1);

    state = pgm_state(sc);
    if (state != PGM_STATE_DONE) {
        sc96281_mtp_err("Error: pgm access state %02x\n",state);
        return -EINVAL;
    }
    return 0;
}

__maybe_unused static int pgm_read_crc(struct sc96281_chg *sc, uint8_t * crc, uint16_t crc_size, uint16_t addr, uint16_t len) {
    volatile uint8_t state = 0;
    uint16_t i;
    uint16_t index;
    pgmPktType pgm = {0};
    uint16_t msg_len;
    uint16_t sram_addr_h = SRAM_BASE >> 16;
    uint16_t sram_addr_l = SRAM_BASE & 0xFFFF;
    uint8_t addr_high[2] = {0};
    uint8_t crc_verify_cmd[4] = {0};

    addr_high[0] = sram_addr_h >> 8;
    addr_high[1] = sram_addr_h & 0XFF;

    sc96281_write_register(sc, ADDR_HIGH_BASE, addr_high, 2);

    pgm.type.addr = addr;
    pgm.type.len = len;
    pgm.type.xor = pgm_xor(&pgm.value[PGM_XOR_INDEX], PGM_XOR_LEN);

    pgm.type.xor ^= PGM_CMD_VERIFY<<8;
    sc96281_write_block(sc, sram_addr_l + PGM_STATE_ADDR, &pgm.value[0], PGM_HEADER_SIZE);
    SC96281_read_register(sc, sram_addr_l + PGM_STATE_ADDR, crc_verify_cmd, 4);
    SC96281_read_register(sc, sram_addr_l + PGM_STATE_ADDR, crc_verify_cmd, 4);

    crc_verify_cmd[1] = PGM_CMD_VERIFY;
    sc96281_write_register(sc, sram_addr_l + PGM_STATE_ADDR, crc_verify_cmd, 4);
    msleep(1);
    state = pgm_state(sc);
    if (state == PGM_STATE_DONE) {
        sc96281_read_block(sc, sram_addr_l + PGM_STATE_ADDR, (uint8_t *)pgm.value, PGM_HEADER_SIZE);
        msg_len = crc_size;
        SC96281_read_register(sc, sram_addr_l + PGM_MSG_ADDR, pgm.type.msg, msg_len);
        if (pgm_xor((uint8_t *)pgm.value, msg_len + PGM_HEADER_SIZE) == 0x00) {
            index = 0;
            for (i=0; (i < msg_len) && (i < crc_size); i++) {
                crc[index] = pgm.type.msg[i];
                index++;
            }
        }
   }
   else {
        sc96281_mtp_err("Error: pgm read crc state %02x\n",state);
        return -EINVAL;
    }
    return 0;
}

__maybe_unused static int pgm_read(struct sc96281_chg *sc, uint32_t addr, uint8_t *buffer, uint16_t len,uint8_t sel) {
    uint16_t i;
    uint8_t state = 0;
    pgmPktType pgm = {0};
    uint16_t offset;
    uint8_t * p_buffer;
    uint16_t sram_addr_h = SRAM_BASE >> 16;
    uint16_t sram_addr_l = SRAM_BASE & 0xFFFF;
    uint8_t addr_high[2] = {0};

    p_buffer = buffer;

    addr_high[0] = sram_addr_h >> 8;
    addr_high[1] = sram_addr_h & 0XFF;

    sc96281_write_register(sc, ADDR_HIGH_BASE, addr_high, 2);

    for (i = 0; i < len; i += PGM_MSG_SIZE) {
        memset(&pgm.value, 0, sizeof(pgm.value));
        pgm.type.addr = (addr + i) /PGM_WORD;
        offset = PGM_MSG_SIZE;
        if (len < (i + PGM_MSG_SIZE)) {
            offset = len - i;
        }
        pgm.type.len = offset /PGM_WORD;

        pgm.type.xor = pgm_xor(&pgm.value[PGM_XOR_INDEX], PGM_XOR_LEN);
        pgm.type.xor ^= sel<<8;

        sc96281_write_register(sc, sram_addr_l + PGM_STATE_ADDR, &pgm.value[0], PGM_HEADER_SIZE);
        sc96281_write_register(sc, sram_addr_l + PGM_CMD_ADDR, &sel, 1);

        state = pgm_state(sc);
        if(state == PGM_STATE_DONE) {
            SC96281_read_register(sc, sram_addr_l + PGM_MSG_ADDR, p_buffer ,offset);
        } else {
            sc96281_mtp_err("Error: pgm read state %02x\n",state);
            return -EINVAL;
        }
        p_buffer += offset;
    }
    return 0;
}

__maybe_unused static int pgm_write(struct sc96281_chg *sc, uint32_t addr, uint8_t *buffer, uint16_t len, uint8_t sel) {
    uint16_t i;
    uint8_t state = 0;
    pgmPktType pgm = {0};
    uint16_t offset;
    uint16_t sram_addr_h = SRAM_BASE >> 16;
    uint16_t sram_addr_l = SRAM_BASE & 0xFFFF;
    uint8_t addr_high[2] = {0};
    uint8_t pgm_write_cmd[4] = {0};

    addr_high[0] = sram_addr_h >> 8;
    addr_high[1] = sram_addr_h & 0xFF;

    sc96281_write_register(sc, ADDR_HIGH_BASE, addr_high, 2);
    for (i = 0; i < len; i += PGM_FRIMWARE_SIZE) {
        memset(pgm.value, 0, sizeof(pgm.value));
        pgm.type.addr = (addr + i) / PGM_WORD;
        offset = PGM_FRIMWARE_SIZE;
        if (len < (i + PGM_FRIMWARE_SIZE)) {
            offset = len - i;
        }

        pgm.type.len = offset / PGM_WORD;
        memcpy(pgm.type.msg, &buffer[i], offset);
        pgm.type.xor = pgm_xor(&pgm.value[PGM_XOR_INDEX], offset + PGM_XOR_LEN);
        pgm.type.xor ^= (sel << 8);
        sc96281_write_block(sc, sram_addr_l + PGM_STATE_ADDR, pgm.value, offset + PGM_HEADER_SIZE);

        SC96281_read_register(sc, sram_addr_l + PGM_STATE_ADDR, pgm_write_cmd, 4);
        SC96281_read_register(sc, sram_addr_l + PGM_STATE_ADDR, pgm_write_cmd, 4);

        pgm_write_cmd[1] = sel;
        sc96281_write_register(sc, sram_addr_l + PGM_STATE_ADDR, pgm_write_cmd, 4);

        state = pgm_state(sc);
        if (state != PGM_STATE_DONE) {
            sc96281_mtp_err("Error: pgm write state %02x, cmd %02x\n",state, sel);
            return -EINVAL;
        }
    }
    sc96281_mtp_err("firmware write successful\n");
    return 0;
}

static int bool_sel(struct sc96281_chg *sc, bool en)
{
    int ret;
    uint8_t data[4] = {0};
    uint16_t addr_sel_h = REG_ATE >> 16;
    uint16_t addr_sel_l = REG_ATE & 0xFFFF;
    uint8_t addr_high[2] = {0};

    addr_high[0] = addr_sel_h >> 8;
    addr_high[1] = addr_sel_h & 0xFF;

    sc96281_write_block(sc, ADDR_HIGH_BASE, addr_high, HIGH_ADDR_NUM);

    ret = sc96281_read_block(sc, addr_sel_l, data, FOUR_BYTE_ALIGN);
    if (ret < 0) {
        return ret;
    }

    if (en) {
        data[0] |= ATE_BOOT_BIT;
    } else {
        data[0] &= 0xFF;
    }

    return sc96281_write_block(sc, addr_sel_l, data, FOUR_BYTE_ALIGN);
}


static int sys_reset_ctrl(struct sc96281_chg *sc, bool en)
{
    if (en) {
        return SC96281_write_byte(sc, REG_RST, RST_PW0);
    } else {
        return SC96281_write_byte(sc, REG_RST, RST_PW1);
    }
}

static int mcu_idle_ctrl(struct sc96281_chg *sc, bool en)
{
    uint8_t idle_data[4] = {0};
    uint16_t idle_addr_high = IDLE_ADDR_HIGH;
    uint16_t idle_addr_low1 = IDLE_ADDR_LOW1;
    uint16_t idle_addr_low2 = IDLE_ADDR_LOW2;
    uint32_t idle_value = IDLE_VAL;
    uint8_t idle_result[4] = {0};
    uint8_t idle_clear[4] = {0};
    int ret = 0;

    idle_data[0] = IDLE_PW0 >> 8;
    idle_data[1] = IDLE_PW0 & 0xFF;
    idle_data[2] = idle_addr_high >> 8;
    idle_data[3] = idle_addr_high & 0xFF;

    idle_result[0] = (idle_value & 0xFF);
    idle_result[1] = ((idle_value >> 8) & 0xFF);
    idle_result[2] = ((idle_value >> 16) & 0xFF);
    idle_result[3] = ((idle_value >> 24) & 0xFF);
    if (en) {
        ret = sc96281_write_block(sc, REG_IDLE, idle_data, 1);
        ret = sc96281_write_block(sc, REG_IDLE + 1, idle_data + 1, 1);
        ret = sc96281_write_block(sc, ADDR_HIGH_BASE, idle_data + 2, HIGH_ADDR_NUM);
        ret = sc96281_write_block(sc, idle_addr_low1, idle_result, FOUR_BYTE_ALIGN);
        ret = sc96281_write_block(sc, idle_addr_low2, idle_clear, FOUR_BYTE_ALIGN);
        msleep(5);
        ret = sc96281_write_block(sc, idle_addr_low1, idle_result, FOUR_BYTE_ALIGN);
        ret = sc96281_write_block(sc, idle_addr_low2, idle_clear, FOUR_BYTE_ALIGN);
    }
    return ret;
}

//***************File api******************
/*read bin file, do not use in this case*/
#ifdef USING_BIN_FILE
static int fp_size(struct file *f)
{
    int error = -EBADF;
    struct kstat stat;

    error = vfs_getattr(&f->f_path, &stat, STATX_SIZE, AT_STATX_FORCE_SYNC);

    if (error == 0) {
        return stat.size;
    }
    else {
        sc96281_mtp_err("get file file stat error\n");
        return error;
    }
}

static int file_read(char *filename, char **buf)
{
    struct file *fp;
    //mm_segment_t fs;
    int size = 0;
    loff_t pos = 0;

    fp = filp_open(filename, O_RDONLY, 0);
    if (IS_ERR(fp)) {
        sc96281_mtp_err("open %s file error\n", filename);
        goto end;
    }

    //fs = get_fs();
    //set_fs(KERNEL_DS);
    size = fp_size(fp);
    if (size <= 0) {
        sc96281_mtp_err("load file:%s error\n", filename);
        goto error;
    }

    *buf = kzalloc(size + 1, GFP_KERNEL);
    kernel_read(fp, *buf, size, &pos);

error:
    filp_close(fp, NULL);
    //set_fs(fs);
end:
    return size;
}

static int sc96281_read_bin(struct sc96281_chg *sc, char *firmware_buf, uint32_t *firmware_len)
{
    char *buf = NULL;
    int size = 0;

    size = file_read(FIRMWARE_FILE_PATH, &buf);
    if (size > 0){
        memcpy(firmware_buf, buf, size);
        *firmware_len = size;

        kfree(buf);
        return 0;
    }

    return -1;
}

#endif /* USING_BIN_FILE */
static int sram_write(struct sc96281_chg *sc)
{
    int i;
    int ret;
    uint16_t addr_base_h = SRAM_BASE_ADDRESS >> 16;
    uint16_t addr_base_l = SRAM_BASE_ADDRESS & 0xFFFF;
    uint32_t size = sizeof(SC96281_PGM_DATA);
    uint8_t addr_high[2] = {0};
    int pgm_pkg_count = 0;
    int pgm_pkg_num = 0;
    int pgm_pkg_remaind = 0;
    char info[PGM_INFO_SIZE];

    /*dig_tm_entry(sc);*/
    pgm_pkg_num = size / PGM_PKG_SIZE;
    pgm_pkg_remaind = size % PGM_PKG_SIZE;
    sc96281_mtp_info("%s: SC96281 pgm num = %d remaind = %d\n", __func__, pgm_pkg_num, pgm_pkg_remaind);

    ret = mcu_idle_ctrl(sc, true);
    if (ret < 0) {
        sc96281_mtp_err("%s: mcu_idle_ctrl failed\n", __func__);
        return -EINVAL;
    }

    addr_high[0] = addr_base_h >> 8;
    addr_high[1] = addr_base_h & 0xFF;
    sc96281_write_register(sc, ADDR_HIGH_BASE, addr_high, HIGH_ADDR_NUM);
    for (i = 0; i < size; i += PGM_PKG_SIZE) {
        if (pgm_pkg_count == pgm_pkg_num && pgm_pkg_remaind != 0) {
            ret = sc96281_write_block(sc, addr_base_l + i, (uint8_t *)&SC96281_PGM_DATA[i], (uint8_t)pgm_pkg_remaind);
            break;
        }
        ret = sc96281_write_block(sc, addr_base_l + i, (uint8_t *)&SC96281_PGM_DATA[i], PGM_PKG_SIZE);
        if (ret < 0) {
            sc96281_mtp_err("%s: sc96281_write_block failed\n", __func__);
            return -EINVAL;
        }
        pgm_pkg_count++;
    }

    ret = bool_sel(sc, true);
    ret |= sys_reset_ctrl(sc, true);
    if (ret < 0) {
        sc96281_mtp_err("%s: sys_reset_ctrl failed\n", __func__);
        return ret;
    }

    ret = SC96281_get_pgm_info(sc, info);
    sc96281_mtp_err("%s: info = %s\n", __func__, info);
    sc96281_mtp_err("%s: pgm run sucessfully\n", __func__);

    return ret;
}

static int fw_program(struct sc96281_chg *sc, uint8_t *buf, uint32_t len)
{
    pgm_chip_erase(sc);

    return pgm_write(sc, 0x0000, buf, (uint16_t)len, PGM_CMD_WRITE_CODE);
}

static int crc_check(struct sc96281_chg *sc, uint8_t margin, uint8_t *buf, uint32_t len)
{
    int i, ret;
    uint32_t data32 = 0;
    uint32_t read_crc = 0;

    //set margin
    ret = pgm_set_margin(sc, margin);
    if (ret < 0) {
        sc96281_mtp_err("set margin fail\n");
        return ret;
    }

    ret = pgm_read_crc(sc, (uint8_t *)(&read_crc), 4, 0x0000, CHIP_SIZE / PGM_WORD);
    if (ret < 0) {
        sc96281_mtp_err("pgm read crc failed");
        return -EINVAL;
    }

    data32 = 0xFFFFFFFF;
    for (i = 0; i < (CHIP_SIZE / PGM_WORD); i++) {
        data32 = sc96281_func_crc32(*(uint32_t *)(buf + i * PGM_WORD), data32);
    }

    sc96281_mtp_info("margin data crc = 0x%08x read crc = 0x%08x\n", data32, read_crc);
    if (data32 != read_crc) {
        sc96281_mtp_err("check crc fail\n");
        return -EINVAL;
    }

    return 0;
}

int mtp_program(struct sc96281_chg *sc, bool using_bin)
{
    int ret;
    uint32_t firmware_length = 0;
    uint8_t *firmware_buf = NULL;

    sc96281_mtp_info("program start\n");
    sc->fw_program = true;

#ifdef USING_BIN_FILE
    firmware_buf = kzalloc(FIRMWARE_SIZE, GFP_KERNEL);
    memset(firmware_buf, 0x00, FIRMWARE_SIZE);
    ret = sc96281_read_bin(sc, firmware_buf, &firmware_length);
	if (ret != 0 || firmware_buf == NULL) {
		sc96281_mtp_err("firmware get error %d\n", ret);
		goto program_fail;
	}
#else
    if (using_bin) {
        firmware_buf = (uint8_t *)sc->fw_bin;
        firmware_length = (uint32_t)sc->fw_bin_length;
    } else {
        firmware_buf = (uint8_t *)sc->fw_data_ptr;
        firmware_length = (uint32_t)sc->fw_data_size;
    }
#endif
    sc96281_mtp_err("firmware len ---> %d", firmware_length);

    //run pgm
    ret = sram_write(sc);
    if (ret < 0) {
        sc96281_mtp_err("sram write fail\n");
        goto program_fail;
    }

    ret = fw_program(sc, firmware_buf, firmware_length);
    if (ret < 0) {
        sc96281_mtp_err("fw program fail\n");
        goto program_fail;
    }

    ret = crc_check(sc, CRC_CHECK_MARGIN0, firmware_buf, firmware_length);
    if (ret < 0) {
        sc96281_mtp_err("margin 0 check crc fail\n");
        goto program_fail;
    }

    ret = crc_check(sc, CRC_CHECK_MARGIN7, firmware_buf, firmware_length);
    if (ret < 0) {
        sc96281_mtp_err("margin 7 check crc fail\n");
        goto program_fail;
    }

    sc96281_mtp_info("program successful\n");
    sc->fw_program = false;
    return 0;

program_fail:
    sc96281_mtp_err("program fail\n");
    sc->fw_program = false;
    return -1;
}
