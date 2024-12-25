/*
 *
 * FocalTech fts TouchScreen driver.
 *
 * Copyright (c) 2012-2022, Focaltech Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*****************************************************************************
*
* File Name: focaltech_upgrade_ft5008.c
*
* Author: Focaltech Driver Team
*
* Created: 2022-10-09
*
* Abstract:
*
* Reference:
*
*****************************************************************************/
/*****************************************************************************
* 1.Included header files
*****************************************************************************/
#include "../focaltech_flash.h"

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_DELAY_ERASE_PAGE            6
#define FTS_SIZE_PAGE                   256
#define FTS_FLASH_PACKET_SIZE           1024//max 2048


/*****************************************************************************
* Static function prototypes
*****************************************************************************/
/************************************************************************
* Name: fts_ft5008_crc16_calc_host
* Brief:
* Input:
* Output:
* Return: return ecc
***********************************************************************/
static u16 fts_ft5008_crc16_calc_host(u8 *pbuf, u32 length)
{
    u16 ecc = 0;
    u32 i = 0;
    u32 j = 0;

    for ( i = 0; i < length; i += 2 ) {
        ecc ^= ((pbuf[i] << 8) | (pbuf[i + 1]));
        for (j = 0; j < 16; j ++) {
            if (ecc & 0x01)
                ecc = (u16)((ecc >> 1) ^ AL2_FCS_COEF);
            else
                ecc >>= 1;
        }
    }

    return ecc;
}

/************************************************************************
 * Name: fts_ft5008_flash_write_buf
 * Brief: write buf data to flash address
 * Input: saddr - start address data write to flash
 *        buf - data buffer
 *        len - data length
 *        delay - delay after write
 * Output:
 * Return: return data ecc of host if success, otherwise return error code
 ***********************************************************************/
static int fts_ft5008_flash_write_buf(u32 saddr, u8 *buf, u32 len, u32 delay)
{
    int ret = 0;
    u32 i = 0;
    u32 j = 0;
    u32 packet_number = 0;
    u32 packet_len = 0;
    u32 addr = 0;
    u32 offset = 0;
    u32 remainder = 0;
    u32 cmdlen = 0;
    u8 packet_buf[FTS_FLASH_PACKET_SIZE + FTS_CMD_WRITE_LEN] = { 0 };
    int ecc_in_host = 0;
    u8 cmd = 0;
    u8 val[FTS_CMD_FLASH_STATUS_LEN] = { 0 };
    u16 read_status = 0;
    u16 wr_ok = 0;
    u32 flash_packet_size = FTS_FLASH_PACKET_SIZE;

    FTS_INFO( "**********write data to flash**********");
    if (!buf || !len || (len > FTS_MAX_LEN_FILE)) {
        FTS_ERROR("buf/len(%d) is invalid", len);
        return -EINVAL;
    }

    FTS_INFO("data buf start addr=0x%x, len=0x%x", saddr, len);
    packet_number = len / flash_packet_size;
    remainder = len % flash_packet_size;
    if (remainder > 0)
        packet_number++;
    packet_len = flash_packet_size;
    FTS_INFO("write data, num:%d remainder:%d", packet_number, remainder);

    for (i = 0; i < packet_number; i++) {
        offset = i * flash_packet_size;
        addr = saddr + offset;

        /* last packet */
        if ((i == (packet_number - 1)) && remainder)
            packet_len = remainder;

        if (fts_data->bus_type == BUS_TYPE_SPI_V2) {
            packet_buf[0] = FTS_CMD_SET_WFLASH_ADDR;
            packet_buf[1] = BYTE_OFF_16(addr);
            packet_buf[2] = BYTE_OFF_8(addr);
            packet_buf[3] = BYTE_OFF_0(addr);
            ret = fts_write(packet_buf, FTS_LEN_SET_ADDR);
            if (ret < 0) {
                FTS_ERROR("set flash address fail");
                return ret;
            }

            packet_buf[0] = FTS_CMD_WRITE;
            cmdlen = 1;
        } else {
            packet_buf[0] = FTS_CMD_WRITE;
            packet_buf[1] = BYTE_OFF_16(addr);
            packet_buf[2] = BYTE_OFF_8(addr);
            packet_buf[3] = BYTE_OFF_0(addr);
            packet_buf[4] = BYTE_OFF_8(packet_len);
            packet_buf[5] = BYTE_OFF_0(packet_len);
            cmdlen = 6;
        }

        for (j = 0; j < packet_len; j++) {
            packet_buf[cmdlen + j] = buf[offset + j];
        }

        ret = fts_write(packet_buf, packet_len + cmdlen);
        if (ret < 0) {
            FTS_ERROR("app write fail");
            return ret;
        }
        mdelay(delay);

        /* read status */
        wr_ok = FTS_CMD_FLASH_STATUS_WRITE_OK + addr / packet_len;
        for (j = 0; j < FTS_RETRIES_WRITE; j++) {
            cmd = FTS_CMD_FLASH_STATUS;
            ret = fts_read(&cmd , 1, val, FTS_CMD_FLASH_STATUS_LEN);
            read_status = (((u16)val[0]) << 8) + val[1];
            /*  FTS_INFO("%x %x", wr_ok, read_status); */
            if (wr_ok == read_status) {
                break;
            }
            mdelay(FTS_RETRIES_DELAY_WRITE);
        }
    }

    ecc_in_host = (int)fts_ft5008_crc16_calc_host(buf, len);
    return ecc_in_host;
}

/************************************************************************
* Name: fts_ft5008_upgrade
* Brief:
* Input:
* Output:
* Return: return 0 if success, otherwise return error code
***********************************************************************/
static int fts_ft5008_upgrade(u8 *buf, u32 len)
{
    int ret = 0;
    u32 start_addr = 0;
    u8 cmd[4] = { 0 };
    u32 delay = 0;
    int ecc_in_host = 0;
    int ecc_in_tp = 0;

    if ((NULL == buf) || (len < FTS_MIN_LEN)) {
        FTS_ERROR("buffer/len(%x) is invalid", len);
        return -EINVAL;
    }

    /* enter into upgrade environment */
    ret = fts_fwupg_enter_into_boot();
    if (ret < 0) {
        FTS_ERROR("enter into pramboot/bootloader fail,ret=%d", ret);
        goto fw_reset;
    }

    cmd[0] = FTS_CMD_APP_DATA_LEN_INCELL;
    cmd[1] = BYTE_OFF_16(len);
    cmd[2] = BYTE_OFF_8(len);
    cmd[3] = BYTE_OFF_0(len);
    ret = fts_write(cmd, FTS_CMD_DATA_LEN_LEN);
    if (ret < 0) {
        FTS_ERROR("data len cmd write fail");
        goto fw_reset;
    }

    cmd[0] = FTS_CMD_FLASH_MODE;
    cmd[1] = FLASH_MODE_UPGRADE_VALUE;
    ret = fts_write(cmd, 2);
    if (ret < 0) {
        FTS_ERROR("upgrade mode(09) cmd write fail");
        goto fw_reset;
    }

    delay = FTS_DELAY_ERASE_PAGE * (len / FTS_SIZE_PAGE);
    ret = fts_fwupg_erase(delay);
    if (ret < 0) {
        FTS_ERROR("erase cmd write fail");
        goto fw_reset;
    }

    /* write app */
    start_addr = upgrade_func_ft5008.appoff;
    delay = (FTS_FLASH_PACKET_SIZE / FTS_SIZE_PAGE) * 2;
    ecc_in_host = fts_ft5008_flash_write_buf(start_addr, buf, len, delay);
    if (ecc_in_host < 0 ) {
        FTS_ERROR("flash write fail");
        goto fw_reset;
    }

    /* ecc */
    ecc_in_tp = fts_fwupg_ecc_cal(start_addr, len);
    if (ecc_in_tp < 0 ) {
        FTS_ERROR("ecc read fail");
        goto fw_reset;
    }

    FTS_INFO("ecc in tp:%x, host:%x", ecc_in_tp, ecc_in_host);
    if (ecc_in_tp != ecc_in_host) {
        FTS_ERROR("ecc check fail");
        goto fw_reset;
    }

    FTS_INFO("upgrade success, reset to normal boot");
    ret = fts_fwupg_reset_in_boot();
    if (ret < 0) {
        FTS_ERROR("reset to normal boot fail");
    }

    msleep(200);
    return 0;

fw_reset:
    FTS_INFO("upgrade fail, reset to normal boot");
    ret = fts_fwupg_reset_in_boot();
    if (ret < 0) {
        FTS_ERROR("reset to normal boot fail");
    }
    return -EIO;
}


int fts_read_lockdown_info(u8 *buf)
{
	u32 lockdown_addr = 0x3F000;
	int ret = 0;
	u8 lockdown_info[0x20] = {0};
	int i = 0;
	int count = 0;
	
	fts_flash_read(lockdown_addr, lockdown_info, sizeof(lockdown_info));
	if (ret < 0) {
    	FTS_ERROR("fail to get vendor id from tp");
    	return ret;
	}

	for(i = 0; i < 8; i++) {
		count += sprintf(buf + count, " %02x ", lockdown_info[i]);
	}

	FTS_INFO("read lockdown info: %s\n", lockdown_info);
	FTS_INFO("read lockdown info: %s\n", buf);
	//memcpy(buf, lockdown_info, sizeof(lockdown_info));

	return 0;
}

int fts_read_lockdown_info_proc(u8 *buf)
{
	u32 lockdown_addr = 0x3F000;
	int ret = 0;
	u8 lockdown_info[0x20] = {0};
	int i = 0;
	int count = 0;

	ret = fts_flash_read(lockdown_addr, lockdown_info, sizeof(lockdown_info));
	if (ret < 0) {
		FTS_ERROR("fail to get vendor id from tp");
		return ret;
	}

	for(i = 0; i < 8; i++) {
		count += sprintf(buf + count, "%c", lockdown_info[i]);
	}
	/*memcpy(buf, lockdown_info, sizeof(lockdown_info));*/
	FTS_INFO("read lockdown info: %s\n", buf);
	return 0;
}

struct upgrade_func upgrade_func_ft5008 = {
    .ctype = {0x90},
    .fwveroff = 0x010E,
    .fwcfgoff = 0x1F80,
    .appoff = 0x0000,
    .upgspec_version = UPGRADE_SPEC_V_1_1,
    .pramboot_supported = false,
    .hid_supported = true,
    .upgrade = fts_ft5008_upgrade,
};
