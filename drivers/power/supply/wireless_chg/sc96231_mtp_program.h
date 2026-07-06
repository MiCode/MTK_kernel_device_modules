// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
*/
#ifndef __SC96231_MTP_PROGRAM_H__
#define __SC96231_MTP_PROGRAM_H__

#define PGM_HEADER_SIZE		 8
#define PGM_MSG_SIZE         0x100
#define PGM_XOR_LEN 	     6
#define PGM_XOR_INDEX		 2

#define PGM_FRIMWARE_SIZE    256

//  The PGM will update the Status in the SRAM as follows:
#define PGM_STATE_NONE       0x00   // reset value

#define PGM_STATE_TIMEOUT    0x7f

#define PGM_STATE_DONE       0xa5   // operating done
#define PGM_STATE_BUSY       0xbb   // operating busy

#define PGM_STATE_ERRLEN     0xe0   // wrong message length
#define PGM_STATE_ERRXOR     0xe1   // wrong check sum
#define PGM_STATE_ERRPROG    0xe2   // read/write fail
#define PGM_STATE_ERRTIMEOUT 0xe3   // read/write timeout
#define PGM_STATE_ERRAUTH    0xe4   // authentication failed
#define PGM_STATE_ND         0xff   // command is not defined
// AP command list
#define PGM_CMD_NONE         0x00   // none command

#define PGM_CMD_INFO         0xa0   // get PGM information
#define PGM_CMD_MARGIN       0xb0   // set margin

#define PGM_CMD_VERIFY       0xc0   // get verify result
#define PGM_CMD_READ_CODE    0xc1   // read code memory
#define PGM_CMD_WRITE_CODE   0xc2   // write code memory

#define PGM_CMD_READ_TRIM    0xc3   // read data from TRIM area
#define PGM_CMD_WRITE_TRIM   0xc4   // write data to TRIM area
#define PGM_CMD_READ_CUST    0xc5   // read data from CUST area
#define PGM_CMD_WRITE_CUST   0xc6   // write data to CUST area
//#define PGM_CMD_CRC_CODE     0xc7   // read opt/mtp/
#define PGM_CMD_READ_ADC     0xca   // read adc 

#define PGM_CMD_RANDOM       0xcc  // gets the random number used for authentication calculations
#define PGM_CMD_AUTH         0xcd  // authentication unlock command

#define PGM_CMD_CHIP_ERASE   0xce   // full chip erase
#define PGM_CMD_PAGE_ERASE   0xe9   // page erase */
#define PGM_CMD_TRIM_ERASE   0xea   // trim area erase
#define PGM_CMD_CUST_ERASE   0xeb   // customer area erase
#define PGM_CMD_SECTOR_ERASE 0xee   // sector erase

#define PGM_STATE_ADDR       0x0000
#define PGM_CMD_ADDR         0x0001
#define PGM_LENGTH_ADDR      0x0004
#define PGM_CHECKSUM_ADDR    0x0006
#define PGM_MSG_ADDR         0x0008

#define SRAM_BASE            0x20000000

#define READ_MAIN            0
#define READ_TRIM            1
#define READ_CUST            2

#define WRITE_MAIN           0
#define WRITE_TRIM           1
#define WRITE_CUST           2

#define CRC_CHECK_MARGIN0    0
#define CRC_CHECK_MARGIN3    3


#pragma pack (1)
typedef struct {
    uint8_t state;
    uint8_t cmd;
    uint16_t addr;
    uint16_t len;
    uint16_t xor;
    uint8_t msg[PGM_MSG_SIZE];
} _type;

typedef union {
    uint8_t value[PGM_MSG_SIZE+PGM_HEADER_SIZE];
    _type type;
} pgmPktType;
#pragma pack ()

int sc96231_mtp_program(struct sc96231 *sc, uint8_t *image, uint32_t len);
#endif
