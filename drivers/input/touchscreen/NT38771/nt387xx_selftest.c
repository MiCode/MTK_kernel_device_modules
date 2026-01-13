/*
 * Copyright (C) 2024 Novatek, Inc.
 *
 * $Revision: 67976 $
 * $Date: 2020-08-27 16:49:50 +0800 (週四, 27 八月 2020) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#include "nt387xx.h"
#include "nt387xx_selftest.h"

#if NVT_TOUCH_MP

#define NORMAL_MODE 0x00
#define TEST_MODE_2 0x22
#define MP_MODE_CC 0x41
#define FREQ_HOP_DISABLE 0x66

#define SHORT_TEST_CSV_FILE "/data/local/tmp/ShortTest.csv"
#define OPEN_TEST_CSV_FILE "/data/local/tmp/OpenTest.csv"
#define FW_RAWDATA_CSV_FILE "/data/local/tmp/FWRawdataTest.csv"
#define FW_CC_CSV_FILE "/data/local/tmp/FWCCTest.csv"
#define NOISE_TEST_CSV_FILE "/data/local/tmp/NoiseTest.csv"
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
#define DIGITAL_NOISE_TEST_CSV_FILE "/data/local/tmp/DigitalNoiseTest.csv"
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/
#define PEN_FW_RAW_TEST_CSV_FILE "/data/local/tmp/PenFWRawTest.csv"
#define PEN_NOISE_TEST_CSV_FILE "/data/local/tmp/PenNoiseTest.csv"

#define nvt_mp_seq_printf(m, fmt, args...) do {	\
	seq_printf(m, fmt, ##args);	\
	if (!nvt_mp_test_result_printed)	\
		printk(fmt, ##args);	\
} while (0)
static int32_t TestResult_SPI_Comm = 0;
static int32_t TestResult_Cap_Rawdata = 0;
static uint8_t *RecordResult_Short = NULL;
static uint8_t *RecordResult_Open = NULL;
static int32_t *RawData_Short = NULL;
static int32_t *RawData_Open = NULL;
static uint8_t *RecordResult_Short_TXRX = NULL;
static uint8_t *RecordResult_Short_TXTX = NULL;
static uint8_t *RecordResult_Short_RXRX = NULL;
static uint8_t *RecordResult_Open_Mutual = NULL;
static uint8_t *RecordResult_Open_SelfTX = NULL;
static uint8_t *RecordResult_Open_SelfRX = NULL;
static uint8_t *RecordResult_FW_Rawdata = NULL;
static uint8_t *RecordResult_FW_CC = NULL;
static uint8_t *RecordResult_FW_DiffMax = NULL;
static uint8_t *RecordResult_FW_DiffMin = NULL;
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
static uint8_t *RecordResult_FW_Digital_DiffMax[3] = {NULL, NULL, NULL};
static uint8_t *RecordResult_FW_Digital_DiffMin[3] = {NULL, NULL, NULL};
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/
static uint8_t RecordResult_Rawdata_Flatness[4] = {0};
static uint8_t *RecordResult_PenTipX_Raw = NULL;
static uint8_t *RecordResult_PenTipY_Raw = NULL;
static uint8_t *RecordResult_PenRingX_Raw = NULL;
static uint8_t *RecordResult_PenRingY_Raw = NULL;
static uint8_t *RecordResult_PenTipX_DiffMax = NULL;
static uint8_t *RecordResult_PenTipX_DiffMin = NULL;
static uint8_t *RecordResult_PenTipY_DiffMax = NULL;
static uint8_t *RecordResult_PenTipY_DiffMin = NULL;
static uint8_t *RecordResult_PenRingX_DiffMax = NULL;
static uint8_t *RecordResult_PenRingX_DiffMin = NULL;
static uint8_t *RecordResult_PenRingY_DiffMax = NULL;
static uint8_t *RecordResult_PenRingY_DiffMin = NULL;

static int32_t TestResult_Short = 0;
static int32_t TestResult_Short_TXRX = 0;
static int32_t TestResult_Short_TXTX = 0;
static int32_t TestResult_Short_RXRX = 0;
static int32_t TestResult_Open = 0;
static int32_t TestResult_Open_Mutual = 0;
static int32_t TestResult_Open_SelfTX = 0;
static int32_t TestResult_Open_SelfRX = 0;
static int32_t TestResult_FW_Rawdata = 0;
static int32_t TestResult_FW_CC = 0;
static int32_t TestResult_Noise = 0;
static int32_t TestResult_FW_DiffMax = 0;
static int32_t TestResult_FW_DiffMin = 0;
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
static int32_t TestResult_Digital_Noise = 0;
static int32_t TestResult_FW_Digital_DiffMax[3] = {0};
static int32_t TestResult_FW_Digital_DiffMin[3] = {0};
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/
static int32_t TestResult_Rawdata_Flatness = 0;
static int32_t TestResult_Pen_FW_Raw = 0;
static int32_t TestResult_PenTipX_Raw = 0;
static int32_t TestResult_PenTipY_Raw = 0;
static int32_t TestResult_PenRingX_Raw = 0;
static int32_t TestResult_PenRingY_Raw = 0;
static int32_t TestResult_Pen_Noise = 0;
static int32_t TestResult_PenTipX_DiffMax = 0;
static int32_t TestResult_PenTipX_DiffMin = 0;
static int32_t TestResult_PenTipY_DiffMax = 0;
static int32_t TestResult_PenTipY_DiffMin = 0;
static int32_t TestResult_PenRingX_DiffMax = 0;
static int32_t TestResult_PenRingX_DiffMin = 0;
static int32_t TestResult_PenRingY_DiffMax = 0;
static int32_t TestResult_PenRingY_DiffMin = 0;

static int32_t *RawData_Short_TXRX = NULL;
static int32_t *RawData_Short_TXTX = NULL;
static int32_t *RawData_Short_RXRX = NULL;
static int32_t *RawData_Open_Mutual = NULL;
static int32_t *RawData_Open_SelfTX = NULL;
static int32_t *RawData_Open_SelfRX = NULL;
static int32_t *RawData_Diff = NULL;
static int32_t *RawData_Diff_Min = NULL;
static int32_t *RawData_Diff_Max = NULL;
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
static int32_t *RawData_Digital_Diff_Min[3] = {NULL, NULL, NULL};
static int32_t *RawData_Digital_Diff_Max[3] = {NULL, NULL, NULL};
static int32_t *RawData_FW_Rawdata[4] = {NULL, NULL, NULL, NULL};
static int32_t *RawData_FW_CC = NULL;
static int32_t *RawData_TX_Trim_CC = NULL;
static int32_t *RawData_RX_Trim_CC = NULL;
static int32_t *RawData_Golden_CC = NULL;
static int32_t *RawData_TX_Golden_CC = NULL;
static int32_t *RawData_RX_Golden_CC = NULL;
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/
static int32_t *RawData_PenTipX_Raw = NULL;
static int32_t *RawData_PenTipY_Raw = NULL;
static int32_t *RawData_PenRingX_Raw = NULL;
static int32_t *RawData_PenRingY_Raw = NULL;
static int32_t *RawData_PenTipX_DiffMin = NULL;
static int32_t *RawData_PenTipX_DiffMax = NULL;
static int32_t *RawData_PenTipY_DiffMin = NULL;
static int32_t *RawData_PenTipY_DiffMax = NULL;
static int32_t *RawData_PenRingX_DiffMin = NULL;
static int32_t *RawData_PenRingX_DiffMax = NULL;
static int32_t *RawData_PenRingY_DiffMin = NULL;
static int32_t *RawData_PenRingY_DiffMax = NULL;
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
static int32_t Rawdata_Flatness_Info[4][16] = {{0}, {0}, {0}, {0}};
static bool rawdata_flatness_info_support = 0;
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/
static int32_t Rawdata_FlatnessValueOper1[4] = {0};
static struct proc_dir_entry *NVT_proc_selftest_entry = NULL;
static struct proc_dir_entry *NVT_proc_android_touch_entry = NULL;
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
static struct proc_dir_entry *hq_proc_selftest_entry = NULL;
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/
static int8_t nvt_mp_test_result_printed = 0;
static uint8_t fw_ver = 0;
static uint16_t nvt_pid = 0;

extern void nvt_change_mode(uint8_t mode);
extern uint8_t nvt_get_fw_pipe(void);
extern void nvt_read_mdata(uint32_t xdata_addr);
extern void nvt_get_mdata(int32_t *buf, uint8_t *m_x_num, uint8_t *m_y_num);
extern void nvt_read_get_num_mdata(uint32_t xdata_addr, int32_t *buffer, uint32_t num);
int32_t nvt_mp_parse_dt(struct device_node *root, const char *node_compatible);

/*******************************************************
Description:
	Novatek touchscreen allocate buffer for mp selftest.

return:
	Executive outcomes. 0---succeed. -12---Out of memory
*******************************************************/
static int nvt_mp_buffer_init(void)
{
	int32_t i;
	size_t RecordResult_BufSize = X_Y_DIMENSION_MAX;
	size_t RawData_BufSize = X_Y_DIMENSION_MAX * sizeof(int32_t);
	size_t Pen_RecordResult_BufSize = PEN_X_Y_DIMENSION_MAX;
	size_t Pen_RawData_BufSize = PEN_X_Y_DIMENSION_MAX * sizeof(int32_t);

	RecordResult_Short = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_Short) {
		NVT_ERR("kzalloc for RecordResult_Short failed!\n");
		return -ENOMEM;
	}

        RecordResult_Open = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_Open) {
		NVT_ERR("kzalloc for RecordResult_Open failed!\n");
		return -ENOMEM;
	}

	RecordResult_Short_TXRX = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_Short_TXRX) {
		NVT_ERR("kzalloc for RecordResult_Short_TXRX failed!\n");
		return -ENOMEM;
	}

	RecordResult_Short_TXTX = (uint8_t *)kzalloc(X_Y_MAX, GFP_KERNEL);
	if (!RecordResult_Short_TXTX) {
		NVT_ERR("kzalloc for RecordResult_Short_TXTX failed!\n");
		return -ENOMEM;
	}

	RecordResult_Short_RXRX = (uint8_t *)kzalloc(X_Y_MAX, GFP_KERNEL);
	if (!RecordResult_Short_RXRX) {
		NVT_ERR("kzalloc for RecordResult_Short_RXRX failed!\n");
		return -ENOMEM;
	}

	RecordResult_Open_Mutual = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_Open_Mutual) {
		NVT_ERR("kzalloc for RecordResult_Open_Mutual failed!\n");
		return -ENOMEM;
	}

	RecordResult_Open_SelfTX = (uint8_t *)kzalloc(X_Y_MAX, GFP_KERNEL);
	if (!RecordResult_Open_SelfTX) {
		NVT_ERR("kzalloc for RecordResult_Open_SelfTX failed!\n");
		return -ENOMEM;
	}

	RecordResult_Open_SelfRX = (uint8_t *)kzalloc(X_Y_MAX, GFP_KERNEL);
	if (!RecordResult_Open_SelfRX) {
		NVT_ERR("kzalloc for RecordResult_Open_SelfRX failed!\n");
		return -ENOMEM;
	}

	RecordResult_FW_Rawdata = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_FW_Rawdata) {
		NVT_ERR("kzalloc for RecordResult_FW_Rawdata failed!\n");
		return -ENOMEM;
	}

	RecordResult_FW_CC = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_FW_CC) {
		NVT_ERR("kzalloc for RecordResult_FW_CC failed!\n");
		return -ENOMEM;
	}

	RecordResult_FW_DiffMax = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_FW_DiffMax) {
		NVT_ERR("kzalloc for RecordResult_FW_DiffMax failed!\n");
		return -ENOMEM;
	}

	RecordResult_FW_DiffMin = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_FW_DiffMin) {
		NVT_ERR("kzalloc for RecordResult_FW_DiffMin failed!\n");
		return -ENOMEM;
	}
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
	for (i = 0; i < 3; i++) {
		RecordResult_FW_Digital_DiffMax[i] = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
		if (!RecordResult_FW_Digital_DiffMax[i]) {
			NVT_ERR("kzalloc for RecordResult_FW_Digital_DiffMax[%d] failed!\n", i);
			return -ENOMEM;
		}

		RecordResult_FW_Digital_DiffMin[i] = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
		if (!RecordResult_FW_Digital_DiffMin[i]) {
			NVT_ERR("kzalloc for RecordResult_FW_Digital_DiffMin[%d] failed!\n", i);
			return -ENOMEM;
		}
	}
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/
	if (ts->pen_support) {
		RecordResult_PenTipX_Raw = (uint8_t *)kzalloc(Pen_RecordResult_BufSize, GFP_KERNEL);
		if (!RecordResult_PenTipX_Raw) {
			NVT_ERR("kzalloc for RecordResult_PenTipX_Raw failed!\n");
			return -ENOMEM;
		}

		RecordResult_PenTipY_Raw = (uint8_t *)kzalloc(Pen_RecordResult_BufSize, GFP_KERNEL);
		if (!RecordResult_PenTipY_Raw) {
			NVT_ERR("kzalloc for RecordResult_PenTipY_Raw failed!\n");
			return -ENOMEM;
		}

		RecordResult_PenRingX_Raw = (uint8_t *)kzalloc(Pen_RecordResult_BufSize, GFP_KERNEL);
		if (!RecordResult_PenRingX_Raw) {
			NVT_ERR("kzalloc for RecordResult_PenRingX_Raw failed!\n");
			return -ENOMEM;
		}

		RecordResult_PenRingY_Raw = (uint8_t *)kzalloc(Pen_RecordResult_BufSize, GFP_KERNEL);
		if (!RecordResult_PenRingY_Raw) {
			NVT_ERR("kzalloc for RecordResult_PenRingY_Raw failed!\n");
			return -ENOMEM;
		}

		RecordResult_PenTipX_DiffMax = (uint8_t *)kzalloc(Pen_RecordResult_BufSize, GFP_KERNEL);
		if (!RecordResult_PenTipX_DiffMax) {
			NVT_ERR("kzalloc for RecordResult_PenTipX_DiffMax failed!\n");
			return -ENOMEM;
		}

		RecordResult_PenTipX_DiffMin = (uint8_t *)kzalloc(Pen_RecordResult_BufSize, GFP_KERNEL);
		if (!RecordResult_PenTipX_DiffMin) {
			NVT_ERR("kzalloc for RecordResult_PenTipX_DiffMin failed!\n");
			return -ENOMEM;
		}

		RecordResult_PenTipY_DiffMax = (uint8_t *)kzalloc(Pen_RecordResult_BufSize, GFP_KERNEL);
		if (!RecordResult_PenTipY_DiffMax) {
			NVT_ERR("kzalloc for RecordResult_PenTipY_DiffMax failed!\n");
			return -ENOMEM;
		}

		RecordResult_PenTipY_DiffMin = (uint8_t *)kzalloc(Pen_RecordResult_BufSize, GFP_KERNEL);
		if (!RecordResult_PenTipY_DiffMin) {
			NVT_ERR("kzalloc for RecordResult_PenTipY_DiffMin failed!\n");
			return -ENOMEM;
		}

		RecordResult_PenRingX_DiffMax = (uint8_t *)kzalloc(Pen_RecordResult_BufSize, GFP_KERNEL);
		if (!RecordResult_PenRingX_DiffMax) {
			NVT_ERR("kzalloc for RecordResult_PenRingX_DiffMax failed!\n");
			return -ENOMEM;
		}

		RecordResult_PenRingX_DiffMin = (uint8_t *)kzalloc(Pen_RecordResult_BufSize, GFP_KERNEL);
		if (!RecordResult_PenRingX_DiffMin) {
			NVT_ERR("kzalloc for RecordResult_PenRingX_DiffMin failed!\n");
			return -ENOMEM;
		}

		RecordResult_PenRingY_DiffMax = (uint8_t *)kzalloc(Pen_RecordResult_BufSize, GFP_KERNEL);
		if (!RecordResult_PenRingY_DiffMax) {
			NVT_ERR("kzalloc for RecordResult_PenRingY_DiffMax failed!\n");
			return -ENOMEM;
		}

		RecordResult_PenRingY_DiffMin = (uint8_t *)kzalloc(Pen_RecordResult_BufSize, GFP_KERNEL);
		if (!RecordResult_PenRingY_DiffMin) {
			NVT_ERR("kzalloc for RecordResult_PenRingY_DiffMin failed!\n");
			return -ENOMEM;
		}
	} /* if (ts->pen_support) */

        RawData_Short = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Short) {
		NVT_ERR("kzalloc for RawData_Short failed!\n");
		return -ENOMEM;
	}
        RawData_Open = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Open) {
		NVT_ERR("kzalloc for RawData_Open failed!\n");
		return -ENOMEM;
	}

	RawData_Short_TXRX = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Short_TXRX) {
		NVT_ERR("kzalloc for RawData_Short_TXRX failed!\n");
		return -ENOMEM;
	}

	RawData_Short_TXTX = (int32_t *)kzalloc(X_Y_MAX * sizeof(int32_t), GFP_KERNEL);
	if (!RawData_Short_TXTX) {
		NVT_ERR("kzalloc for RawData_Short_TXTX failed!\n");
		return -ENOMEM;
	}

	RawData_Short_RXRX = (int32_t *)kzalloc(X_Y_MAX * sizeof(int32_t), GFP_KERNEL);
	if (!RawData_Short_RXRX) {
		NVT_ERR("kzalloc for RawData_Short_RXRX failed!\n");
		return -ENOMEM;
	}

	RawData_Open_Mutual = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Open_Mutual) {
		NVT_ERR("kzalloc for RawData_Open_Mutual failed!\n");
		return -ENOMEM;
	}

	RawData_Open_SelfTX = (int32_t *)kzalloc(X_Y_MAX * sizeof(int32_t), GFP_KERNEL);
	if (!RawData_Open_SelfTX) {
		NVT_ERR("kzalloc for RawData_Open_SelfTX failed!\n");
		return -ENOMEM;
	}

	RawData_Open_SelfRX = (int32_t *)kzalloc(X_Y_MAX * sizeof(int32_t), GFP_KERNEL);
	if (!RawData_Open_SelfRX) {
		NVT_ERR("kzalloc for RawData_Open_SelfRX failed!\n");
		return -ENOMEM;
	}

	RawData_Diff = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Diff) {
		NVT_ERR("kzalloc for RawData_Diff failed!\n");
		return -ENOMEM;
	}

	RawData_Diff_Min = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Diff_Min) {
		NVT_ERR("kzalloc for RawData_Diff_Min failed!\n");
		return -ENOMEM;
	}

	RawData_Diff_Max = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Diff_Max) {
		NVT_ERR("kzalloc for RawData_Diff_Max failed!\n");
		return -ENOMEM;
	}
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
	for (i = 0; i < 3; i++) {
		RawData_Digital_Diff_Min[i] = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
		if (!RawData_Digital_Diff_Min[i]) {
			NVT_ERR("kzalloc for RawData_Digital_Diff_Min[%d] failed!\n", i);
			return -ENOMEM;
		}

		RawData_Digital_Diff_Max[i] = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
		if (!RawData_Digital_Diff_Max[i]) {
			NVT_ERR("kzalloc for RawData_Digital_Diff_Max[%d] failed!\n", i);
			return -ENOMEM;
		}
	}

	for (i = 0; i < 4; i++) {
		RawData_FW_Rawdata[i] = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
		if (!RawData_FW_Rawdata[i]) {
			NVT_ERR("kzalloc for RawData_FW_Rawdata[%d] failed!\n", i);
			return -ENOMEM;
		}
	}
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/
	RawData_FW_CC = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_FW_CC) {
		NVT_ERR("kzalloc for RawData_FW_CC failed!\n");
		return -ENOMEM;
	}
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
	RawData_TX_Trim_CC = (int32_t *)kzalloc(X_Y_MAX * sizeof(int32_t), GFP_KERNEL);
	if (!RawData_TX_Trim_CC) {
		NVT_ERR("kzalloc for RawData_TX_Trim_CC failed!\n");
		return -ENOMEM;
	}

	RawData_RX_Trim_CC = (int32_t *)kzalloc(X_Y_MAX * sizeof(int32_t), GFP_KERNEL);
	if (!RawData_RX_Trim_CC) {
		NVT_ERR("kzalloc for RawData_RX_Trim_CC failed!\n");
		return -ENOMEM;
	}

	RawData_Golden_CC = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Golden_CC) {
		NVT_ERR("kzalloc for RawData_Golden_CC failed!\n");
		return -ENOMEM;
	}

	RawData_TX_Golden_CC = (int32_t *)kzalloc(X_Y_MAX * sizeof(int32_t), GFP_KERNEL);
	if (!RawData_TX_Golden_CC) {
		NVT_ERR("kzalloc for RawData_TX_Golden_CC failed!\n");
		return -ENOMEM;
	}

	RawData_RX_Golden_CC = (int32_t *)kzalloc(X_Y_MAX * sizeof(int32_t), GFP_KERNEL);
	if (!RawData_RX_Golden_CC) {
		NVT_ERR("kzalloc for RawData_RX_Golden_CC failed!\n");
		return -ENOMEM;
	}
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/
	if (ts->pen_support) {
		RawData_PenTipX_Raw = (int32_t *)kzalloc(Pen_RawData_BufSize, GFP_KERNEL);
		if (!RawData_PenTipX_Raw) {
			NVT_ERR("kzalloc for RawData_PenTipX_Raw failed!\n");
			return -ENOMEM;
		}

		RawData_PenTipY_Raw = (int32_t *)kzalloc(Pen_RawData_BufSize, GFP_KERNEL);
		if (!RawData_PenTipY_Raw) {
			NVT_ERR("kzalloc for RawData_PenTipY_Raw failed!\n");
			return -ENOMEM;
		}

		RawData_PenRingX_Raw = (int32_t *)kzalloc(Pen_RawData_BufSize, GFP_KERNEL);
		if (!RawData_PenRingX_Raw) {
			NVT_ERR("kzalloc for RawData_PenRingX_Raw failed!\n");
			return -ENOMEM;
		}

		RawData_PenRingY_Raw = (int32_t *)kzalloc(Pen_RawData_BufSize, GFP_KERNEL);
		if (!RawData_PenRingY_Raw) {
			NVT_ERR("kzalloc for RawData_PenRingY_Raw failed!\n");
			return -ENOMEM;
		}

		RawData_PenTipX_DiffMax = (int32_t *)kzalloc(Pen_RawData_BufSize, GFP_KERNEL);
		if (!RawData_PenTipX_DiffMax) {
			NVT_ERR("kzalloc for RawData_PenTipX_DiffMax failed!\n");
			return -ENOMEM;
		}

		RawData_PenTipX_DiffMin = (int32_t *)kzalloc(Pen_RawData_BufSize, GFP_KERNEL);
		if (!RawData_PenTipX_DiffMin) {
			NVT_ERR("kzalloc for RawData_PenTipX_DiffMin failed!\n");
			return -ENOMEM;
		}

		RawData_PenTipY_DiffMax = (int32_t *)kzalloc(Pen_RawData_BufSize, GFP_KERNEL);
		if (!RawData_PenTipY_DiffMax) {
			NVT_ERR("kzalloc for RawData_PenTipY_DiffMax failed!\n");
			return -ENOMEM;
		}

		RawData_PenTipY_DiffMin = (int32_t *)kzalloc(Pen_RawData_BufSize, GFP_KERNEL);
		if (!RawData_PenTipY_DiffMin) {
			NVT_ERR("kzalloc for RawData_PenTipY_DiffMin failed!\n");
			return -ENOMEM;
		}

		RawData_PenRingX_DiffMax = (int32_t *)kzalloc(Pen_RawData_BufSize, GFP_KERNEL);
		if (!RawData_PenRingX_DiffMax) {
			NVT_ERR("kzalloc for RawData_PenRingX_DiffMax failed!\n");
			return -ENOMEM;
		}

		RawData_PenRingX_DiffMin = (int32_t *)kzalloc(Pen_RawData_BufSize, GFP_KERNEL);
		if (!RawData_PenRingX_DiffMin) {
			NVT_ERR("kzalloc for RawData_PenRingX_DiffMin failed!\n");
			return -ENOMEM;
		}

		RawData_PenRingY_DiffMax = (int32_t *)kzalloc(Pen_RawData_BufSize, GFP_KERNEL);
		if (!RawData_PenRingY_DiffMax) {
			NVT_ERR("kzalloc for RawData_PenRingY_DiffMax failed!\n");
			return -ENOMEM;
		}

		RawData_PenRingY_DiffMin = (int32_t *)kzalloc(Pen_RawData_BufSize, GFP_KERNEL);
		if (!RawData_PenRingY_DiffMin) {
			NVT_ERR("kzalloc for RawData_PenRingY_DiffMin failed!\n");
			return -ENOMEM;
		}
	} /* if (ts->pen_support) */

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen free buffer for mp selftest.

return:
	n.a.
*******************************************************/
static void nvt_mp_buffer_deinit(void)
{
	int32_t i;
	if (RecordResult_Short) {
		kfree(RecordResult_Short);
		RecordResult_Short = NULL;
	}
        if (RecordResult_Open) {
		kfree(RecordResult_Open);
		RecordResult_Open = NULL;
	}

	if (RecordResult_Short_TXRX) {
		kfree(RecordResult_Short_TXRX);
		RecordResult_Short_TXRX = NULL;
	}

	if (RecordResult_Short_TXTX) {
		kfree(RecordResult_Short_TXTX);
		RecordResult_Short_TXTX = NULL;
	}

	if (RecordResult_Short_RXRX) {
		kfree(RecordResult_Short_RXRX);
		RecordResult_Short_RXRX = NULL;
	}

	if (RecordResult_Open_Mutual) {
		kfree(RecordResult_Open_Mutual);
		RecordResult_Open_Mutual = NULL;
	}

	if (RecordResult_Open_SelfTX) {
		kfree(RecordResult_Open_SelfTX);
		RecordResult_Open_SelfTX = NULL;
	}

	if (RecordResult_Open_SelfRX) {
		kfree(RecordResult_Open_SelfRX);
		RecordResult_Open_SelfRX = NULL;
	}

	if (RecordResult_FW_Rawdata) {
		kfree(RecordResult_FW_Rawdata);
		RecordResult_FW_Rawdata = NULL;
	}

	if (RecordResult_FW_CC) {
		kfree(RecordResult_FW_CC);
		RecordResult_FW_CC = NULL;
	}

	if (RecordResult_FW_DiffMax) {
		kfree(RecordResult_FW_DiffMax);
		RecordResult_FW_DiffMax = NULL;
	}

	if (RecordResult_FW_DiffMin) {
		kfree(RecordResult_FW_DiffMin);
		RecordResult_FW_DiffMin = NULL;
	}
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
	for (i = 0; i < 3; i++) {
		if (RecordResult_FW_Digital_DiffMax[i]) {
			kfree(RecordResult_FW_Digital_DiffMax[i]);
			RecordResult_FW_Digital_DiffMax[i] = NULL;
		}

		if (RecordResult_FW_Digital_DiffMin[i]) {
			kfree(RecordResult_FW_Digital_DiffMin[i]);
			RecordResult_FW_Digital_DiffMin[i] = NULL;
		}
	}
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/
	if (ts->pen_support) {
		if (RecordResult_PenTipX_Raw) {
			kfree(RecordResult_PenTipX_Raw);
			RecordResult_PenTipX_Raw = NULL;
		}

		if (RecordResult_PenTipY_Raw) {
			kfree(RecordResult_PenTipY_Raw);
			RecordResult_PenTipY_Raw = NULL;
		}

		if (RecordResult_PenRingX_Raw) {
			kfree(RecordResult_PenRingX_Raw);
			RecordResult_PenRingX_Raw = NULL;
		}

		if (RecordResult_PenRingY_Raw) {
			kfree(RecordResult_PenRingY_Raw);
			RecordResult_PenRingY_Raw = NULL;
		}

		if (RecordResult_PenTipX_DiffMax) {
			kfree(RecordResult_PenTipX_DiffMax);
			RecordResult_PenTipX_DiffMax = NULL;
		}

		if (RecordResult_PenTipX_DiffMin) {
			kfree(RecordResult_PenTipX_DiffMin);
			RecordResult_PenTipX_DiffMin = NULL;
		}

		if (RecordResult_PenTipY_DiffMax) {
			kfree(RecordResult_PenTipY_DiffMax);
			RecordResult_PenTipY_DiffMax = NULL;
		}

		if (RecordResult_PenTipY_DiffMin) {
			kfree(RecordResult_PenTipY_DiffMin);
			RecordResult_PenTipY_DiffMin = NULL;
		}

		if (RecordResult_PenRingX_DiffMax) {
			kfree(RecordResult_PenRingX_DiffMax);
			RecordResult_PenRingX_DiffMax = NULL;
		}

		if (RecordResult_PenRingX_DiffMin) {
			kfree(RecordResult_PenRingX_DiffMin);
			RecordResult_PenRingX_DiffMin = NULL;
		}

		if (RecordResult_PenRingY_DiffMax) {
			kfree(RecordResult_PenRingY_DiffMax);
			RecordResult_PenRingY_DiffMax = NULL;
		}

		if (RecordResult_PenRingY_DiffMin) {
			kfree(RecordResult_PenRingY_DiffMin);
			RecordResult_PenRingY_DiffMin = NULL;
		}
	} /* if (ts->pen_support) */

	if (RawData_Short) {
		kfree(RawData_Short);
		RawData_Short = NULL;
	}

        if (RawData_Open) {
		kfree(RawData_Open);
		RawData_Open = NULL;
	}

	if (RawData_Short_TXRX) {
		kfree(RawData_Short_TXRX);
		RawData_Short_TXRX = NULL;
	}

	if (RawData_Short_TXTX) {
		kfree(RawData_Short_TXTX);
		RawData_Short_TXTX = NULL;
	}

	if (RawData_Short_RXRX) {
		kfree(RawData_Short_RXRX);
		RawData_Short_RXRX = NULL;
	}

	if (RawData_Open_Mutual) {
		kfree(RawData_Open_Mutual);
		RawData_Open_Mutual = NULL;
	}

	if (RawData_Open_SelfTX) {
		kfree(RawData_Open_SelfTX);
		RawData_Open_SelfTX = NULL;
	}

	if (RawData_Open_SelfRX) {
		kfree(RawData_Open_SelfRX);
		RawData_Open_SelfRX = NULL;
	}

	if (RawData_Diff) {
		kfree(RawData_Diff);
		RawData_Diff = NULL;
	}

	if (RawData_Diff_Min) {
		kfree(RawData_Diff_Min);
		RawData_Diff_Min = NULL;
	}

	if (RawData_Diff_Max) {
		kfree(RawData_Diff_Max);
		RawData_Diff_Max = NULL;
	}
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
	for (i = 0; i < 3; i++) {
		if (RawData_Digital_Diff_Min[i]) {
			kfree(RawData_Digital_Diff_Min[i]);
			RawData_Digital_Diff_Min[i] = NULL;
		}

		if (RawData_Digital_Diff_Max[i]) {
			kfree(RawData_Digital_Diff_Max[i]);
			RawData_Digital_Diff_Max[i] = NULL;
		}
	}

	for (i = 0; i < 4; i++) {
		if (RawData_FW_Rawdata[i]) {
			kfree(RawData_FW_Rawdata[i]);
			RawData_FW_Rawdata[i] = NULL;
		}
	}

	if (RawData_FW_CC) {
		kfree(RawData_FW_CC);
		RawData_FW_CC = NULL;
	}

	if (RawData_TX_Trim_CC) {
		kfree(RawData_TX_Trim_CC);
		RawData_TX_Trim_CC = NULL;
	}

	if (RawData_RX_Trim_CC) {
		kfree(RawData_RX_Trim_CC);
		RawData_RX_Trim_CC = NULL;
	}

	if (RawData_Golden_CC) {
		kfree(RawData_Golden_CC);
		RawData_Golden_CC = NULL;
	}

	if (RawData_TX_Golden_CC) {
		kfree(RawData_TX_Golden_CC);
		RawData_TX_Golden_CC = NULL;
	}

	if (RawData_RX_Golden_CC) {
		kfree(RawData_RX_Golden_CC);
		RawData_RX_Golden_CC = NULL;
	}
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/
	if (ts->pen_support) {
		if (RawData_PenTipX_Raw) {
			kfree(RawData_PenTipX_Raw);
			RawData_PenTipX_Raw = NULL;
		}

		if (RawData_PenTipY_Raw) {
			kfree(RawData_PenTipY_Raw);
			RawData_PenTipY_Raw = NULL;
		}

		if (RawData_PenRingX_Raw) {
			kfree(RawData_PenRingX_Raw);
			RawData_PenRingX_Raw = NULL;
		}

		if (RawData_PenRingY_Raw) {
			kfree(RawData_PenRingY_Raw);
			RawData_PenRingY_Raw = NULL;
		}

		if (RawData_PenTipX_DiffMax) {
			kfree(RawData_PenTipX_DiffMax);
			RawData_PenTipX_DiffMax = NULL;
		}

		if (RawData_PenTipX_DiffMin) {
			kfree(RawData_PenTipX_DiffMin);
			RawData_PenTipX_DiffMin = NULL;
		}

		if (RawData_PenTipY_DiffMax) {
			kfree(RawData_PenTipY_DiffMax);
			RawData_PenTipY_DiffMax = NULL;
		}

		if (RawData_PenTipY_DiffMin) {
			kfree(RawData_PenTipY_DiffMin);
			RawData_PenTipY_DiffMin = NULL;
		}

		if (RawData_PenRingX_DiffMax) {
			kfree(RawData_PenRingX_DiffMax);
			RawData_PenRingX_DiffMax = NULL;
		}

		if (RawData_PenRingX_DiffMin) {
			kfree(RawData_PenRingX_DiffMin);
			RawData_PenRingX_DiffMin = NULL;
		}

		if (RawData_PenRingY_DiffMax) {
			kfree(RawData_PenRingY_DiffMax);
			RawData_PenRingY_DiffMax = NULL;
		}

		if (RawData_PenRingY_DiffMin) {
			kfree(RawData_PenRingY_DiffMin);
			RawData_PenRingY_DiffMin = NULL;
		}
	} /* if (ts->pen_support) */
}

static void nvt_print_data_log_in_one_line(int32_t *data, int32_t data_num)
{
	char *tmp_log = NULL;
	int32_t i = 0;

	tmp_log = (char *)kzalloc(data_num * 7 + 1, GFP_KERNEL);
	if (!tmp_log) {
		NVT_ERR("kzalloc for tmp_log failed!\n ");
		return;
	}

	for (i = 0; i < data_num; i++) {
		sprintf(tmp_log + i * 7, "%6d,", data[i]);
	}
	tmp_log[data_num * 7] = '\0';
	printk("%s", tmp_log);
	if (tmp_log) {
		kfree(tmp_log);
		tmp_log = NULL;
	}

	return;
}

static void nvt_print_result_log_in_one_line(uint8_t *result, int32_t result_num)
{
	char *tmp_log = NULL;
	int32_t i = 0;

	tmp_log = (char *)kzalloc(result_num * 6 + 1, GFP_KERNEL);
	if (!tmp_log) {
		NVT_ERR("kzalloc for tmp_log failed!\n ");
		return;
	}

	for (i = 0; i < result_num; i++) {
		sprintf(tmp_log + i * 6, "0x%02X, ", result[i]);
	}
	tmp_log[result_num * 6] = '\0';
	printk("%s", tmp_log);
	if (tmp_log) {
		kfree(tmp_log);
		tmp_log = NULL;
	}

	return;
}

/*******************************************************
Description:
	Novatek touchscreen self-test criteria print function.

return:
	n.a.
*******************************************************/
static void nvt_print_data_array(int32_t *array, int32_t x_ch, int32_t y_ch)
{
	int32_t j = 0;

	for (j = 0; j < y_ch; j++) {
		nvt_print_data_log_in_one_line(array + j * x_ch, x_ch);
		printk("\n");
	}
}

static void nvt_print_criteria(void)
{
	NVT_LOG("++\n");

	//---PS_Config_Lmt_Short_TXRX---
	printk("PS_Config_Lmt_Short_TXRX_P:\n");
	nvt_print_data_array(PS_Config_Lmt_Short_TXRX_P, ts->x_num, ts->y_num);
	printk("PS_Config_Lmt_Short_TXRX_N:\n");
	nvt_print_data_array(PS_Config_Lmt_Short_TXRX_N, ts->x_num, ts->y_num);

	//---PS_Config_Lmt_Short_TXTX---
	printk("PS_Config_Lmt_Short_TXTX_P:\n");
	nvt_print_data_array(PS_Config_Lmt_Short_TXTX_P, ts->x_num, 1);
	printk("PS_Config_Lmt_Short_TXTX_N:\n");
	nvt_print_data_array(PS_Config_Lmt_Short_TXTX_N, ts->x_num, 1);

	//---PS_Config_Lmt_Short_RXRX---
	printk("PS_Config_Lmt_Short_RXRX_P:\n");
	nvt_print_data_array(PS_Config_Lmt_Short_RXRX_P, 1, ts->y_num);
	printk("PS_Config_Lmt_Short_RXRX_N:\n");
	nvt_print_data_array(PS_Config_Lmt_Short_RXRX_N, 1, ts->y_num);

	//---PS_Config_Lmt_Open_Mutual---
	printk("PS_Config_Lmt_Open_Mutual_P:\n");
	nvt_print_data_array(PS_Config_Lmt_Open_Mutual_P, ts->x_num, ts->y_num);
	printk("PS_Config_Lmt_Open_Mutual_N:\n");
	nvt_print_data_array(PS_Config_Lmt_Open_Mutual_N, ts->x_num, ts->y_num);

	//---PS_Config_Lmt_Open_SelfTX---
	printk("PS_Config_Lmt_Open_SelfTX_P:\n");
	nvt_print_data_array(PS_Config_Lmt_Open_SelfTX_P, ts->x_num, 1);
	printk("PS_Config_Lmt_Open_SelfTX_N:\n");
	nvt_print_data_array(PS_Config_Lmt_Open_SelfTX_N, ts->x_num, 1);

	//---PS_Config_Lmt_Open_SelfRX---
	printk("PS_Config_Lmt_Open_SelfRX_P:\n");
	nvt_print_data_array(PS_Config_Lmt_Open_SelfRX_P, 1, ts->y_num);
	printk("PS_Config_Lmt_Open_SelfRX_N:\n");
	nvt_print_data_array(PS_Config_Lmt_Open_SelfRX_N, 1, ts->y_num);

	//---PS_Config_Lmt_FW_Rawdata---
	printk("PS_Config_Lmt_FW_Rawdata_P:\n");
	nvt_print_data_array(PS_Config_Lmt_FW_Rawdata_P, ts->x_num, ts->y_num);
	printk("PS_Config_Lmt_FW_Rawdata_N:\n");
	nvt_print_data_array(PS_Config_Lmt_FW_Rawdata_N, ts->x_num, ts->y_num);

	//---PS_Config_Lmt_FW_CC---
	printk("PS_Config_Lmt_FW_CC_P:\n");
	nvt_print_data_array(PS_Config_Lmt_FW_CC_P, ts->x_num, ts->y_num);
	printk("PS_Config_Lmt_FW_CC_N:\n");
	nvt_print_data_array(PS_Config_Lmt_FW_CC_N, ts->x_num, ts->y_num);

	//---PS_Config_Lmt_FW_Diff---
	printk("PS_Config_Lmt_FW_Diff_P:\n");
	nvt_print_data_array(PS_Config_Lmt_FW_Diff_P, ts->x_num, ts->y_num);
	printk("PS_Config_Lmt_FW_Diff_N:\n");
	nvt_print_data_array(PS_Config_Lmt_FW_Diff_N, ts->x_num, ts->y_num);

	//---PS_Config_Lmt_Digital_FW_Diff---
	printk("PS_Config_Lmt_FW_Digital_Diff_P:\n");
	nvt_print_data_array(PS_Config_Lmt_FW_Digital_Diff_P, ts->x_num, ts->y_num);
	printk("PS_Config_Lmt_FW_Digital_Diff_N:\n");
	nvt_print_data_array(PS_Config_Lmt_FW_Digital_Diff_N, ts->x_num, ts->y_num);
/*P16 code for HQFEAT-93999 by xiongdejun at 2025/6/27 start*/
	//---PS_Config_Lmt_Digital_FW_Diff---
	printk("PS_Config_Lmt_FlatnessValueOper1_P:\n");
	nvt_print_data_array(PS_Config_Lmt_FlatnessValueOper1_P, 4, 1);
	printk("PS_Config_Lmt_FlatnessValueOper1_N:\n");
	nvt_print_data_array(PS_Config_Lmt_FlatnessValueOper1_N, 4, 1);
/*P16 code for HQFEAT-93999 by xiongdejun at 2025/6/27 end*/
	if (ts->pen_support) {
		//---PS_Config_Lmt_PenTipX_FW_Raw---
		printk("PS_Config_Lmt_PenTipX_FW_Raw_P:\n");
		nvt_print_data_array(PS_Config_Lmt_PenTipX_FW_Raw_P, ts->pen_x_num_x, ts->pen_x_num_y);
		printk("PS_Config_Lmt_PenTipX_FW_Raw_N:\n");
		nvt_print_data_array(PS_Config_Lmt_PenTipX_FW_Raw_N, ts->pen_x_num_x, ts->pen_x_num_y);

		//---PS_Config_Lmt_PenTipY_FW_Raw---
		printk("PS_Config_Lmt_PenTipY_FW_Raw_P:\n");
		nvt_print_data_array(PS_Config_Lmt_PenTipY_FW_Raw_P, ts->pen_y_num_x, ts->pen_y_num_y);
		printk("PS_Config_Lmt_PenTipY_FW_Raw_N:\n");
		nvt_print_data_array(PS_Config_Lmt_PenTipY_FW_Raw_N, ts->pen_y_num_x, ts->pen_y_num_y);

		//---PS_Config_Lmt_PenRingX_FW_Raw---
		printk("PS_Config_Lmt_PenRingX_FW_Raw_P:\n");
		nvt_print_data_array(PS_Config_Lmt_PenRingX_FW_Raw_P, ts->pen_x_num_x, ts->pen_x_num_y);
		printk("PS_Config_Lmt_PenRingX_FW_Raw_N:\n");
		nvt_print_data_array(PS_Config_Lmt_PenRingX_FW_Raw_N, ts->pen_x_num_x, ts->pen_x_num_y);

		//---PS_Config_Lmt_PenRingY_FW_Raw---
		printk("PS_Config_Lmt_PenRingY_FW_Raw_P:\n");
		nvt_print_data_array(PS_Config_Lmt_PenRingY_FW_Raw_P, ts->pen_y_num_x, ts->pen_y_num_y);
		printk("PS_Config_Lmt_PenRingY_FW_Raw_N:\n");
		nvt_print_data_array(PS_Config_Lmt_PenRingY_FW_Raw_N, ts->pen_y_num_x, ts->pen_y_num_y);

		//---PS_Config_Lmt_PenTipX_FW_Diff---
		printk("PS_Config_Lmt_PenTipX_FW_Diff_P:\n");
		nvt_print_data_array(PS_Config_Lmt_PenTipX_FW_Diff_P, ts->pen_x_num_x, ts->pen_x_num_y);
		printk("PS_Config_Lmt_PenTipX_FW_Diff_N:\n");
		nvt_print_data_array(PS_Config_Lmt_PenTipX_FW_Diff_N, ts->pen_x_num_x, ts->pen_x_num_y);

		//---PS_Config_Lmt_PenTipY_FW_Diff---
		printk("PS_Config_Lmt_PenTipY_FW_Diff_P:\n");
		nvt_print_data_array(PS_Config_Lmt_PenTipY_FW_Diff_P, ts->pen_y_num_x, ts->pen_y_num_y);
		printk("PS_Config_Lmt_PenTipY_FW_Diff_N:\n");
		nvt_print_data_array(PS_Config_Lmt_PenTipY_FW_Diff_N, ts->pen_y_num_x, ts->pen_y_num_y);

		//---PS_Config_Lmt_PenRingX_FW_Diff---
		printk("PS_Config_Lmt_PenRingX_FW_Diff_P:\n");
		nvt_print_data_array(PS_Config_Lmt_PenRingX_FW_Diff_P, ts->pen_x_num_x, ts->pen_x_num_y);
		printk("PS_Config_Lmt_PenRingX_FW_Diff_N:\n");
		nvt_print_data_array(PS_Config_Lmt_PenRingX_FW_Diff_N, ts->pen_x_num_x, ts->pen_x_num_y);

		//---PS_Config_Lmt_PenRingY_FW_Diff---
		printk("PS_Config_Lmt_PenRingY_FW_Diff_P:\n");
		nvt_print_data_array(PS_Config_Lmt_PenRingY_FW_Diff_P, ts->pen_y_num_x, ts->pen_y_num_y);
		printk("PS_Config_Lmt_PenRingY_FW_Diff_N:\n");
		nvt_print_data_array(PS_Config_Lmt_PenRingY_FW_Diff_N, ts->pen_y_num_x, ts->pen_y_num_y);
	} /* if (ts->pen_support) */

	NVT_LOG("--\n");
}

#if NVT_SAVE_TEST_DATA_IN_FILE
static int32_t nvt_save_rawdata_to_csv(int32_t *rawdata, uint8_t x_ch, uint8_t y_ch, const char *file_path, uint32_t offset)
{
	int32_t x = 0;
	int32_t y = 0;
	int32_t iArrayIndex = 0;
	struct file *fp = NULL;
	char *fbufp = NULL;
#ifdef HAVE_VFS_WRITE
	mm_segment_t org_fs;
#endif
	int32_t write_ret = 0;
	uint32_t output_len = 0;
	loff_t pos = 0;

	printk("%s:++\n", __func__);
	fbufp = (char *)kzalloc(x_ch * y_ch * 7 + y_ch * 2 + 1, GFP_KERNEL);
	if (!fbufp) {
		NVT_ERR("kzalloc for fbufp failed!\n");
		return -ENOMEM;
	}

	for (y = 0; y < y_ch; y++) {
		for (x = 0; x < x_ch; x++) {
			iArrayIndex = y * x_ch + x;
			sprintf(fbufp + iArrayIndex * 7 + y * 2, "%6d,", rawdata[iArrayIndex]);
		}
		nvt_print_data_log_in_one_line(rawdata + y * x_ch, x_ch);
		printk("\n");
		sprintf(fbufp + (iArrayIndex + 1) * 7 + y * 2,"\r\n");
	}

	if (offset == 0)
		fp = filp_open(file_path, O_RDWR | O_CREAT | O_TRUNC, 0666);
	else
		fp = filp_open(file_path, O_RDWR | O_CREAT, 0666);
	if (fp == NULL || IS_ERR(fp)) {
		NVT_ERR("open %s failed, errno=%ld\n", file_path, PTR_ERR(fp));
		if (fbufp) {
			kfree(fbufp);
			fbufp = NULL;
		}
		return -1;
	}

	output_len = y_ch * x_ch * 7 + y_ch * 2;
	pos = offset;
#ifdef HAVE_VFS_WRITE
	org_fs = get_fs();
	set_fs(KERNEL_DS);
	write_ret = vfs_write(fp, (char __user *)fbufp, output_len, &pos);
	set_fs(org_fs);
#else
	write_ret = kernel_write(fp, fbufp, output_len, &pos);
#endif /* #ifdef HAVE_VFS_WRITE */
	if (write_ret <= 0) {
		NVT_ERR("write %s failed, output_len=%u, write_ret=%d\n", file_path, output_len, write_ret);
		if (fp) {
			filp_close(fp, NULL);
			fp = NULL;
		}
		if (fbufp) {
			kfree(fbufp);
			fbufp = NULL;
		}
		return -1;
	}

	if (fp) {
		filp_close(fp, NULL);
		fp = NULL;
	}
	if (fbufp) {
		kfree(fbufp);
		fbufp = NULL;
	}

	printk("%s:--\n", __func__);

	return 0;
}
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

static int32_t nvt_polling_hand_shake_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 250;

	usleep_range(20000, 20000);

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_SPI_READ(ts->client, buf, 2);

		if ((buf[1] == 0xA0) || (buf[1] == 0xA1))
			break;

		usleep_range(20000, 20000);
	}

	if (i >= retry) {
		NVT_ERR("polling hand shake status failed, buf[1]=0x%02X\n", buf[1]);

		// Read back 5 bytes from offset EVENT_MAP_HOST_CMD for debug check
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		CTP_SPI_READ(ts->client, buf, 6);
		NVT_ERR("Read back 5 bytes from offset EVENT_MAP_HOST_CMD: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", buf[1], buf[2], buf[3], buf[4], buf[5]);

		return -1;
	} else {
		return 0;
	}
}

static int8_t nvt_switch_FreqHopEnDis(uint8_t FreqHopEnDis)
{
	uint8_t buf[8] = {0};
	uint8_t retry = 0;
	int8_t ret = 0;

	NVT_LOG("++\n");

	for (retry = 0; retry < 20; retry++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

		//---switch FreqHopEnDis---
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = FreqHopEnDis;
		CTP_SPI_WRITE(ts->client, buf, 2);

		msleep(35);

		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;
	}

	if (unlikely(retry == 20)) {
		NVT_ERR("switch FreqHopEnDis 0x%02X failed, buf[1]=0x%02X\n", FreqHopEnDis, buf[1]);
		ret = -1;
	}

	NVT_LOG("--\n");

	return ret;
}

static int32_t nvt_read_fw_rawdata(int32_t *xdata)
{
	uint8_t x_num = 0;
	uint8_t y_num = 0;
	uint32_t x = 0;
	uint32_t y = 0;
	int32_t iArrayIndex = 0;

	NVT_LOG("++\n");

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->RAW_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->RAW_PIPE1_ADDR);

	nvt_get_mdata(xdata, &x_num, &y_num);

	for (y = 0; y < y_num; y++) {
		for (x = 0; x < x_num; x++) {
			iArrayIndex = y * x_num + x;
			xdata[iArrayIndex] = (int16_t)xdata[iArrayIndex];
		}
	}

	printk("%s:\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(xdata, ts->x_num, ts->y_num, FW_RAWDATA_CSV_FILE, 0) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	nvt_print_data_array(xdata, ts->x_num, ts->y_num);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	NVT_LOG("--\n");

	return 0;
}

static int32_t nvt_read_CC(int32_t *xdata)
{
	uint8_t x_num = 0;
	uint8_t y_num = 0;
	uint32_t x = 0;
	uint32_t y = 0;
	int32_t iArrayIndex = 0;

	NVT_LOG("++\n");

	// Get Trim CC
	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR);
	else
		nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR);
	nvt_get_mdata(xdata, &x_num, &y_num);
	for (y = 0; y < y_num; y++) {
		for (x = 0; x < x_num; x++) {
			iArrayIndex = y * x_num + x;
			xdata[iArrayIndex] = (int16_t)xdata[iArrayIndex];
		}
	}
	printk("%s:\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
	if (nvt_save_rawdata_to_csv(xdata, ts->x_num, ts->y_num, FW_CC_CSV_FILE, 0) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	nvt_print_data_array(xdata, ts->x_num, ts->y_num);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
	printk("%s:RawData_TX_Trim_CC\n", __func__);
	nvt_read_get_num_mdata(ts->mmap->TX_SELF_DIFF_ADDR, RawData_TX_Trim_CC, ts->x_num * 1);
#if NVT_SAVE_TEST_DATA_IN_FILE
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	nvt_print_data_array(RawData_TX_Trim_CC, ts->x_num, 1);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	printk("%s:RawData_RX_Trim_CC\n", __func__);
	nvt_read_get_num_mdata(ts->mmap->RX_SELF_DIFF_ADDR, RawData_RX_Trim_CC, 1 * ts->y_num);
#if NVT_SAVE_TEST_DATA_IN_FILE
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	nvt_print_data_array(RawData_RX_Trim_CC, 1, ts->y_num);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	// Get Golden CC
	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR);
	nvt_get_mdata(RawData_Golden_CC, &x_num, &y_num);
	for (y = 0; y < y_num; y++) {
		for (x = 0; x < x_num; x++) {
			iArrayIndex = y * x_num + x;
			RawData_Golden_CC[iArrayIndex] = (int16_t)RawData_Golden_CC[iArrayIndex];
		}
	}
	printk("%s:RawData_Golden_CC\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	nvt_print_data_array(RawData_Golden_CC, ts->x_num, ts->y_num);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	printk("%s:RawData_TX_Golden_CC\n", __func__);
	nvt_read_get_num_mdata(ts->mmap->TX_SELF_RAWIIR_ADDR, RawData_TX_Golden_CC, ts->x_num * 1);
#if NVT_SAVE_TEST_DATA_IN_FILE
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	nvt_print_data_array(RawData_TX_Golden_CC, ts->x_num, 1);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	printk("%s:RawData_RX_Golden_CC\n", __func__);
	nvt_read_get_num_mdata(ts->mmap->RX_SELF_RAWIIR_ADDR, RawData_RX_Golden_CC, 1 * ts->y_num);
#if NVT_SAVE_TEST_DATA_IN_FILE
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	nvt_print_data_array(RawData_RX_Golden_CC, 1, ts->y_num);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	NVT_LOG("--\n");
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/
	return 0;
}

static int32_t nvt_read_pen_baseline(void)
{
#if NVT_SAVE_TEST_DATA_IN_FILE
	uint32_t csv_output_offset = 0;
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	NVT_LOG("++\n");

	nvt_read_get_num_mdata(ts->mmap->PEN_2D_BL_TIP_X_ADDR, RawData_PenTipX_Raw, ts->pen_x_num_x * ts->pen_x_num_y);
	nvt_read_get_num_mdata(ts->mmap->PEN_2D_BL_TIP_Y_ADDR, RawData_PenTipY_Raw, ts->pen_y_num_x * ts->pen_y_num_y);
	nvt_read_get_num_mdata(ts->mmap->PEN_2D_BL_RING_X_ADDR, RawData_PenRingX_Raw, ts->pen_x_num_x * ts->pen_x_num_y);
	nvt_read_get_num_mdata(ts->mmap->PEN_2D_BL_RING_Y_ADDR, RawData_PenRingY_Raw, ts->pen_y_num_x * ts->pen_y_num_y);

	// Save Rawdata to CSV file
	printk("%s:RawData_PenTipX_Raw\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
	if (nvt_save_rawdata_to_csv(RawData_PenTipX_Raw, ts->pen_x_num_x, ts->pen_x_num_y, PEN_FW_RAW_TEST_CSV_FILE, csv_output_offset) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}
	csv_output_offset += ts->pen_x_num_y * ts->pen_x_num_x * 7 + ts->pen_x_num_y * 2;
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	nvt_print_data_array(RawData_PenTipX_Raw, ts->pen_x_num_x, ts->pen_x_num_y);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	printk("%s:RawData_PenTipY_Raw\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
	if (nvt_save_rawdata_to_csv(RawData_PenTipY_Raw, ts->pen_y_num_x, ts->pen_y_num_y, PEN_FW_RAW_TEST_CSV_FILE, csv_output_offset) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}
	csv_output_offset += ts->pen_y_num_y * ts->pen_y_num_x * 7 + ts->pen_y_num_y * 2;
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	nvt_print_data_array(RawData_PenTipY_Raw, ts->pen_y_num_x, ts->pen_y_num_y);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	printk("%s:RawData_PenRingX_Raw\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
	if (nvt_save_rawdata_to_csv(RawData_PenRingX_Raw, ts->pen_x_num_x, ts->pen_x_num_y, PEN_FW_RAW_TEST_CSV_FILE, csv_output_offset) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}
	csv_output_offset += ts->pen_x_num_y * ts->pen_x_num_x * 7 + ts->pen_x_num_y * 2;
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	nvt_print_data_array(RawData_PenRingX_Raw, ts->pen_x_num_x, ts->pen_x_num_y);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	printk("%s:RawData_PenRingY_Raw\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
	if (nvt_save_rawdata_to_csv(RawData_PenRingY_Raw, ts->pen_y_num_x, ts->pen_y_num_y, PEN_FW_RAW_TEST_CSV_FILE, csv_output_offset) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	nvt_print_data_array(RawData_PenRingY_Raw, ts->pen_y_num_x, ts->pen_y_num_y);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	NVT_LOG("--\n");

	return 0;
}

static void nvt_enable_noise_collect(int32_t frame_num)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---enable noise collect---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x47;
	buf[2] = 0xAA;
	buf[3] = frame_num;
	buf[4] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 5);
}

static int32_t nvt_read_fw_noise(int32_t *xdata)
{
	uint8_t x_num = 0;
	uint8_t y_num = 0;
	uint32_t x = 0;
	uint32_t y = 0;
	int32_t iArrayIndex = 0;
	int32_t frame_num = 0;
#if NVT_SAVE_TEST_DATA_IN_FILE
	uint32_t rawdata_diff_min_offset = 0;
	uint32_t csv_pen_noise_offset = 0;
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	NVT_LOG("++\n");

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		return -EAGAIN;
	}

	frame_num = PS_Config_Diff_Test_Frame / 10;
	if (frame_num <= 0)
		frame_num = 1;
	printk("%s: frame_num=%d\n", __func__, frame_num);
	nvt_enable_noise_collect(frame_num);
	// need wait PS_Config_Diff_Test_Frame * 8.3ms
	msleep(frame_num * 83);

	if (nvt_polling_hand_shake_status()) {
		return -EAGAIN;
	}

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR);

	nvt_get_mdata(xdata, &x_num, &y_num);

	for (y = 0; y < y_num; y++) {
		for (x = 0; x < x_num; x++) {
			iArrayIndex = y * x_num + x;
			RawData_Diff_Max[iArrayIndex] = (int8_t)((xdata[iArrayIndex] >> 8) & 0xFF);
			RawData_Diff_Min[iArrayIndex] = (int8_t)(xdata[iArrayIndex] & 0xFF);
		}
	}

	if (ts->pen_support) {
		// get pen noise data
		nvt_read_get_num_mdata(ts->mmap->PEN_2D_DIFF_TIP_X_ADDR, RawData_PenTipX_DiffMax, ts->pen_x_num_x * ts->pen_x_num_y);
		nvt_read_get_num_mdata(ts->mmap->PEN_2D_RAW_TIP_X_ADDR, RawData_PenTipX_DiffMin, ts->pen_x_num_x * ts->pen_x_num_y);
		nvt_read_get_num_mdata(ts->mmap->PEN_2D_DIFF_TIP_Y_ADDR, RawData_PenTipY_DiffMax, ts->pen_y_num_x * ts->pen_y_num_y);
		nvt_read_get_num_mdata(ts->mmap->PEN_2D_RAW_TIP_Y_ADDR, RawData_PenTipY_DiffMin, ts->pen_y_num_x * ts->pen_y_num_y);
		nvt_read_get_num_mdata(ts->mmap->PEN_2D_DIFF_RING_X_ADDR, RawData_PenRingX_DiffMax, ts->pen_x_num_x * ts->pen_x_num_y);
		nvt_read_get_num_mdata(ts->mmap->PEN_2D_RAW_RING_X_ADDR, RawData_PenRingX_DiffMin, ts->pen_x_num_x * ts->pen_x_num_y);
		nvt_read_get_num_mdata(ts->mmap->PEN_2D_DIFF_RING_Y_ADDR, RawData_PenRingY_DiffMax, ts->pen_y_num_x * ts->pen_y_num_y);
		nvt_read_get_num_mdata(ts->mmap->PEN_2D_RAW_RING_Y_ADDR, RawData_PenRingY_DiffMin, ts->pen_y_num_x * ts->pen_y_num_y);
	} /* if (ts->pen_support) */

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	printk("%s:RawData_Diff_Max:\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(RawData_Diff_Max, ts->x_num, ts->y_num, NOISE_TEST_CSV_FILE, 0) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}

	rawdata_diff_min_offset = ts->y_num * ts->x_num * 7 + ts->y_num * 2;
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	nvt_print_data_array(RawData_Diff_Max, ts->x_num, ts->y_num);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	printk("%s:RawData_Diff_Min:\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(RawData_Diff_Min, ts->x_num, ts->y_num, NOISE_TEST_CSV_FILE, rawdata_diff_min_offset) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	nvt_print_data_array(RawData_Diff_Min, ts->x_num, ts->y_num);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	if (ts->pen_support) {
		printk("%s:RawData_PenTipX_DiffMax:\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
		if (nvt_save_rawdata_to_csv(RawData_PenTipX_DiffMax, ts->pen_x_num_x, ts->pen_x_num_y, PEN_NOISE_TEST_CSV_FILE, csv_pen_noise_offset) < 0) {
			NVT_ERR("save rawdata to CSV file failed\n");
			return -EAGAIN;
		}
		csv_pen_noise_offset += ts->pen_x_num_y * ts->pen_x_num_x * 7 + ts->pen_x_num_y * 2;
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
		nvt_print_data_array(RawData_PenTipX_DiffMax, ts->pen_x_num_x, ts->pen_x_num_y);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

		printk("%s:RawData_PenTipX_DiffMin:\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
		if (nvt_save_rawdata_to_csv(RawData_PenTipX_DiffMin, ts->pen_x_num_x, ts->pen_x_num_y, PEN_NOISE_TEST_CSV_FILE, csv_pen_noise_offset) < 0) {
			NVT_ERR("save rawdata to CSV file failed\n");
			return -EAGAIN;
		}
		csv_pen_noise_offset += ts->pen_x_num_y * ts->pen_x_num_x * 7 + ts->pen_x_num_y * 2;
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
		nvt_print_data_array(RawData_PenTipX_DiffMin, ts->pen_x_num_x, ts->pen_x_num_y);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

		printk("%s:RawData_PenTipY_DiffMax:\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
		if (nvt_save_rawdata_to_csv(RawData_PenTipY_DiffMax, ts->pen_y_num_x, ts->pen_y_num_y, PEN_NOISE_TEST_CSV_FILE, csv_pen_noise_offset) < 0) {
			NVT_ERR("save rawdata to CSV file failed\n");
			return -EAGAIN;
		}
		csv_pen_noise_offset += ts->pen_y_num_y * ts->pen_y_num_x * 7 + ts->pen_y_num_y * 2;
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
		nvt_print_data_array(RawData_PenTipY_DiffMax, ts->pen_y_num_x, ts->pen_y_num_y);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

		printk("%s:RawData_PenTipY_DiffMin:\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
		if (nvt_save_rawdata_to_csv(RawData_PenTipY_DiffMin, ts->pen_y_num_x, ts->pen_y_num_y, PEN_NOISE_TEST_CSV_FILE, csv_pen_noise_offset) < 0) {
			NVT_ERR("save rawdata to CSV file failed\n");
			return -EAGAIN;
		}
		csv_pen_noise_offset += ts->pen_y_num_y * ts->pen_y_num_x * 7 + ts->pen_y_num_y * 2;
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
		nvt_print_data_array(RawData_PenTipY_DiffMin, ts->pen_y_num_x, ts->pen_y_num_y);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

		printk("%s:RawData_PenRingX_DiffMax:\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
		if (nvt_save_rawdata_to_csv(RawData_PenRingX_DiffMax, ts->pen_x_num_x, ts->pen_x_num_y, PEN_NOISE_TEST_CSV_FILE, csv_pen_noise_offset) < 0) {
			NVT_ERR("save rawdata to CSV file failed\n");
			return -EAGAIN;
		}
		csv_pen_noise_offset += ts->pen_x_num_y * ts->pen_x_num_x * 7 + ts->pen_x_num_y * 2;
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
		nvt_print_data_array(RawData_PenRingX_DiffMax, ts->pen_x_num_x, ts->pen_x_num_y);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

		printk("%s:RawData_PenRingX_DiffMin:\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
		if (nvt_save_rawdata_to_csv(RawData_PenRingX_DiffMin, ts->pen_x_num_x, ts->pen_x_num_y, PEN_NOISE_TEST_CSV_FILE, csv_pen_noise_offset) < 0) {
			NVT_ERR("save rawdata to CSV file failed\n");
			return -EAGAIN;
		}
		csv_pen_noise_offset += ts->pen_x_num_y * ts->pen_x_num_x * 7 + ts->pen_x_num_y * 2;
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
		nvt_print_data_array(RawData_PenRingX_DiffMin, ts->pen_x_num_x, ts->pen_x_num_y);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

		printk("%s:RawData_PenRingY_DiffMax:\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
		if (nvt_save_rawdata_to_csv(RawData_PenRingY_DiffMax, ts->pen_y_num_x, ts->pen_y_num_y, PEN_NOISE_TEST_CSV_FILE, csv_pen_noise_offset) < 0) {
			NVT_ERR("save rawdata to CSV file failed\n");
			return -EAGAIN;
		}
		csv_pen_noise_offset += ts->pen_y_num_y * ts->pen_y_num_x * 7 + ts->pen_y_num_y * 2;
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
		nvt_print_data_array(RawData_PenRingY_DiffMax, ts->pen_y_num_x, ts->pen_y_num_y);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

		printk("%s:RawData_PenRingY_DiffMin:\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
		if (nvt_save_rawdata_to_csv(RawData_PenRingY_DiffMin, ts->pen_y_num_x, ts->pen_y_num_y, PEN_NOISE_TEST_CSV_FILE, csv_pen_noise_offset) < 0) {
			NVT_ERR("save rawdata to CSV file failed\n");
			return -EAGAIN;
		}
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
		nvt_print_data_array(RawData_PenRingY_DiffMin, ts->pen_y_num_x, ts->pen_y_num_y);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	} /* if (ts->pen_support) */

	NVT_LOG("--\n");

	return 0;
}
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
static int32_t nvt_enter_digital_test(uint8_t enter_digital_test)
{
	int32_t ret;
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 70;

	NVT_LOG("++, enter_digital_test=%d\n", enter_digital_test);

	/*---set xdata index to EVENT BUF ADDR---*/
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	/*---set mode---*/
	if (enter_digital_test) {
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x32;
		buf[2] = 0x01;
		buf[3] = 0x08;
		buf[4] = enter_digital_test;
		CTP_SPI_WRITE(ts->client, buf, 5);
	} else { // leave digital test
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x32;
		buf[2] = 0x00;
		buf[3] = 0x07;
		CTP_SPI_WRITE(ts->client, buf, 4);
	}

	/*---polling fw handshake---*/
	for (i = 0; i < retry; i++) {
		/*---set xdata index to EVENT BUF ADDR---*/
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);
		/*---read fw status---*/
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_SPI_READ(ts->client, buf, 2);
		if (buf[1] == 0xAA) {
			break;
		}
		msleep(20);
	}

	if (i >= retry) {
		NVT_ERR("polling hand shake status failed, buf[1]=0x%02X\n", buf[1]);
		/* Read back 5 bytes from offset EVENT_MAP_HOST_CMD for debug check*/
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		CTP_SPI_READ(ts->client, buf, 6);
		NVT_ERR("Read back 5 bytes from offset EVENT_MAP_HOST_CMD: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
			 buf[1], buf[2], buf[3], buf[4], buf[5]);
		ret = -1;
		goto out;
	} else {
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0xCC;
		CTP_SPI_WRITE(ts->client, buf, 2);
		ret = 0;
	}

	msleep(20);
out:

	NVT_LOG("--\n");

	return ret;
}

#define RAWDATA_FLATNESS_INFO_ITEM_NUM 16
static int32_t nvt_get_rawdata_flatness_info(int32_t *rawdata_flatness_info)
{
	uint8_t buf[64] = {0};
	int32_t i;
	int16_t *pinfo;

	NVT_LOG("++\n");

	pinfo = (int16_t *)(buf + 1);
	nvt_set_page(ts->mmap->RAWDATA_FLATNESS_INFO_ADDR);
	buf[0] = ts->mmap->RAWDATA_FLATNESS_INFO_ADDR & 0xFF;
	CTP_SPI_READ(ts->client, buf, RAWDATA_FLATNESS_INFO_ITEM_NUM * 2 + 1);
	if ((buf[1] + buf[2]) == 0xFF) {
		rawdata_flatness_info_support = true;
		printk("rawdata flatness info: ");
		for (i = 0; i < RAWDATA_FLATNESS_INFO_ITEM_NUM; i++) {
			if (i == 0) {
				// only keep 1st byte version info
				rawdata_flatness_info[0] = (pinfo[0] & 0xFF);
			} else {
				rawdata_flatness_info[i] = pinfo[i];
			}
			printk("%d, ", rawdata_flatness_info[i]);
		}
		printk("\n");
	} else {
		rawdata_flatness_info_support = false;
		NVT_ERR("rawdata flatness info not support. Version=0x%02X, VerBar=0x%02X\n", buf[1], buf[2]);
	}

	NVT_LOG("--\n");
	return 0;
}

static int32_t nvt_read_fw_digital_noise(int32_t *xdata)
{
	uint8_t x_num = 0;
	uint8_t y_num = 0;
	uint32_t x = 0;
	uint32_t y = 0;
	int32_t iArrayIndex = 0;
	int32_t frame_num = 0;
#if NVT_SAVE_TEST_DATA_IN_FILE
	uint32_t rawdata_diff_offset = 0;
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	int32_t i;

	NVT_LOG("++\n");

	for (i = 0; i < 3 ; i++) {
		// start from F2 to F4
/*P16 code for BUGP16-3030 by xiongdejun at 2025/5/29 start*/
		if(nvt_enter_digital_test(i+2)){
                	return -EAGAIN;
                }
/*P16 code for BUGP16-3030 by xiongdejun at 2025/5/29 end*/
		//---Enter Test Mode---
		if (nvt_clear_fw_status()) {
			return -EAGAIN;
		}

		frame_num = PS_Config_Digital_Diff_Test_Frame / 10;
		if (frame_num <= 0)
			frame_num = 1;
		printk("%s: frame_num=%d\n", __func__, frame_num);
		nvt_enable_noise_collect(frame_num);
		// need wait PS_Config_Digital_Diff_Test_Frame * 8.3ms
		msleep(frame_num * 83);

		if (nvt_polling_hand_shake_status()) {
			return -EAGAIN;
		}

		if (nvt_get_fw_pipe() == 0)
			nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR);
		else
			nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR);

		nvt_get_mdata(xdata, &x_num, &y_num);

		for (y = 0; y < y_num; y++) {
			for (x = 0; x < x_num; x++) {
				iArrayIndex = y * x_num + x;
				RawData_Digital_Diff_Max[i][iArrayIndex] = (int8_t)((xdata[iArrayIndex] >> 8) & 0xFF);
				RawData_Digital_Diff_Min[i][iArrayIndex] = (int8_t)(xdata[iArrayIndex] & 0xFF);
			}
		}

		// Collect Fn Rawdata and Flatness Info
		printk("%s:RawData_FW_Rawdata[%d](F%d):\n", __func__, i+1, i+2);
		nvt_read_fw_rawdata(RawData_FW_Rawdata[i+1]);
		nvt_get_rawdata_flatness_info(Rawdata_Flatness_Info[i+1]);
		Rawdata_FlatnessValueOper1[i+1] = Rawdata_Flatness_Info[i+1][5];

		//---Leave Test Mode---
		nvt_change_mode(NORMAL_MODE);
/*P16 code for BUGP16-3030 by xiongdejun at 2025/5/29 start*/
		if(nvt_enter_digital_test(0)){
                	return -EAGAIN;
                }
/*P16 code for BUGP16-3030 by xiongdejun at 2025/5/29 end*/
		printk("%s:RawData_Digital_Diff_Max[%d](F%d):\n", __func__, i, i+2);
#if NVT_SAVE_TEST_DATA_IN_FILE
		// Save Rawdata to CSV file
		if (nvt_save_rawdata_to_csv(RawData_Digital_Diff_Max[i], ts->x_num, ts->y_num, DIGITAL_NOISE_TEST_CSV_FILE, i ? rawdata_diff_offset : 0) < 0) {
			NVT_ERR("save rawdata to CSV file failed\n");
			return -EAGAIN;
		}
		rawdata_diff_offset += ts->y_num * ts->x_num * 7 + ts->y_num * 2;
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
		nvt_print_data_array(RawData_Digital_Diff_Max[i], ts->x_num, ts->y_num);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

		printk("%s:RawData_Digital_Diff_Min[%d](F%d):\n", __func__, i, i+2);
#if NVT_SAVE_TEST_DATA_IN_FILE
		// Save Rawdata to CSV file
		if (nvt_save_rawdata_to_csv(RawData_Digital_Diff_Min[i], ts->x_num, ts->y_num, DIGITAL_NOISE_TEST_CSV_FILE, rawdata_diff_offset) < 0) {
			NVT_ERR("save rawdata to CSV file failed\n");
			return -EAGAIN;
		}
		rawdata_diff_offset += ts->y_num * ts->x_num * 7 + ts->y_num * 2;
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
		nvt_print_data_array(RawData_Digital_Diff_Min[i], ts->x_num, ts->y_num);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	}

	NVT_LOG("--\n");

	return 0;
}
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/
static void nvt_enable_open_test(void)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---enable open test---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x45;
	buf[2] = 0xAA;
	buf[3] = 0x02;
	buf[4] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 5);
}

static void nvt_enable_short_test(void)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---enable short test---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x43;
	buf[2] = 0xAA;
	buf[3] = 0x02;
	buf[4] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 5);
}

static int32_t nvt_read_fw_open(int32_t *xdata)
{
	uint8_t x_num = 0;
	uint8_t y_num = 0;
	uint32_t x = 0;
	uint32_t y = 0;
	int32_t iArrayIndex = 0;
#if NVT_SAVE_TEST_DATA_IN_FILE
	uint32_t csv_output_offset = 0;
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	NVT_LOG("++\n");

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		return -EAGAIN;
	}

	nvt_enable_open_test();

	if (nvt_polling_hand_shake_status()) {
		return -EAGAIN;
	}

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->RAW_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->RAW_PIPE1_ADDR);

	nvt_get_mdata(xdata, &x_num, &y_num);

	for (y = 0; y < y_num; y++) {
		for (x = 0; x < x_num; x++) {
			iArrayIndex = y * x_num + x;
			xdata[iArrayIndex] = (int16_t)xdata[iArrayIndex];
		}
	}

	nvt_read_get_num_mdata(ts->mmap->TX_SELF_RAWIIR_ADDR, RawData_Open_SelfTX, ts->x_num * 1);
	nvt_read_get_num_mdata(ts->mmap->RX_SELF_RAWIIR_ADDR, RawData_Open_SelfRX, 1 * ts->y_num);

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	printk("%s:RawData_Open_Mutual\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
	// Save RawData to CSV file
	if (nvt_save_rawdata_to_csv(xdata, ts->x_num, ts->y_num, OPEN_TEST_CSV_FILE, 0) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	nvt_print_data_array(xdata, ts->x_num, ts->y_num);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	printk("%s:RawData_Open_SelfTX\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
	csv_output_offset = ts->y_num * ts->x_num * 7 + ts->y_num * 2;
	// Save RawData to CSV file
	if (nvt_save_rawdata_to_csv(RawData_Open_SelfTX, ts->x_num, 1, OPEN_TEST_CSV_FILE, csv_output_offset) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	nvt_print_data_array(RawData_Open_SelfTX, ts->x_num, 1);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	printk("%s:RawData_Open_SelfRX\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
	csv_output_offset += 1 * ts->x_num * 7 + 1 * 2;
	// Save RawData to CSV file
	if (nvt_save_rawdata_to_csv(RawData_Open_SelfRX, 1, ts->y_num, OPEN_TEST_CSV_FILE, csv_output_offset) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	nvt_print_data_array(RawData_Open_SelfRX, 1, ts->y_num);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	NVT_LOG("--\n");

	return 0;
}

static int32_t nvt_read_fw_short(int32_t *xdata)
{
	uint8_t x_num = 0;
	uint8_t y_num = 0;
	uint32_t x = 0;
	uint32_t y = 0;
	int32_t iArrayIndex = 0;
#if NVT_SAVE_TEST_DATA_IN_FILE
	uint32_t csv_output_offset = 0;
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	NVT_LOG("++\n");

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		return -EAGAIN;
	}

	nvt_enable_short_test();

	if (nvt_polling_hand_shake_status()) {
		return -EAGAIN;
	}

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->RAW_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->RAW_PIPE1_ADDR);

	nvt_get_mdata(xdata, &x_num, &y_num);

	for (y = 0; y < y_num; y++) {
		for (x = 0; x < x_num; x++) {
			iArrayIndex = y * x_num + x;
			xdata[iArrayIndex] = (int16_t)xdata[iArrayIndex];
		}
	}

	nvt_read_get_num_mdata(ts->mmap->TX_SELF_RAWIIR_ADDR, RawData_Short_TXTX, ts->x_num * 1);
	nvt_read_get_num_mdata(ts->mmap->RX_SELF_RAWIIR_ADDR, RawData_Short_RXRX, 1 * ts->y_num);

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	printk("%s:RawData_Short_TXRX\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(xdata, ts->x_num, ts->y_num, SHORT_TEST_CSV_FILE, 0) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	nvt_print_data_array(xdata, ts->x_num, ts->y_num);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	printk("%s:RawData_Short_TXTX\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
	csv_output_offset = ts->y_num * ts->x_num * 7 + ts->y_num * 2;
	// Save RawData to CSV file
	if (nvt_save_rawdata_to_csv(RawData_Short_TXTX, ts->x_num, 1, SHORT_TEST_CSV_FILE, csv_output_offset) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	nvt_print_data_array(RawData_Short_TXTX, ts->x_num, 1);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	printk("%s:RawData_Short_RXRX\n", __func__);
#if NVT_SAVE_TEST_DATA_IN_FILE
	csv_output_offset += 1 * ts->x_num * 7 + 1 * 2;
	// Save RawData to CSV file
	if (nvt_save_rawdata_to_csv(RawData_Short_RXRX, 1, ts->y_num, SHORT_TEST_CSV_FILE, csv_output_offset) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	nvt_print_data_array(RawData_Short_RXRX, 1, ts->y_num);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	NVT_LOG("--\n");

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen raw data test for each single point function.

return:
	Executive outcomes. 0---passed. negative---failed.
*******************************************************/
static int32_t RawDataTest_SinglePoint_Sub(int32_t rawdata[], uint8_t RecordResult[], uint8_t x_ch, uint8_t y_ch, int32_t Rawdata_Limit_Postive[], int32_t Rawdata_Limit_Negative[])
{
	int32_t i = 0;
	int32_t j = 0;
	int32_t iArrayIndex = 0;
	bool isPass = true;

	for (j = 0; j < y_ch; j++) {
		for (i = 0; i < x_ch; i++) {
			iArrayIndex = j * x_ch + i;

			RecordResult[iArrayIndex] = 0x00; // default value for PASS

			if(rawdata[iArrayIndex] > Rawdata_Limit_Postive[iArrayIndex])
				RecordResult[iArrayIndex] |= 0x01;

			if(rawdata[iArrayIndex] < Rawdata_Limit_Negative[iArrayIndex])
				RecordResult[iArrayIndex] |= 0x02;
		}
	}

	//---Check RecordResult---
	for (j = 0; j < y_ch; j++) {
		for (i = 0; i < x_ch; i++) {
			if (RecordResult[j * x_ch + i] != 0) {
				isPass = false;
				break;
			}
		}
	}

	if (isPass == false) {
		return -1; // FAIL
	} else {
		return 0; // PASS
	}
}

/*******************************************************
Description:
	Novatek touchscreen print self-test result function.

return:
	n.a.
*******************************************************/
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
void print_selftest_result(struct seq_file *m, int32_t TestResult, uint8_t RecordResult[], int32_t rawdata[], uint8_t x_len, uint8_t y_len)
{
	int32_t i = 0;
	int32_t j = 0;
	int32_t iArrayIndex = 0;

	switch (TestResult) {
		case 0xDCDCDCDC:
			nvt_mp_seq_printf(m, " DATA COLLECT.\n");
#if !NVT_SAVE_TEST_DATA_IN_FILE
			goto print_test_data;
#endif /* #if !NVT_SAVE_TEST_DATA_IN_FILE */
			break;
		case 0:
			nvt_mp_seq_printf(m, " PASS.\n");
#if !NVT_SAVE_TEST_DATA_IN_FILE
			goto print_test_data;
#endif /* #if !NVT_SAVE_TEST_DATA_IN_FILE */
			break;

		case 1:
			nvt_mp_seq_printf(m, " ERROR! Read Data FAIL!\n");
			break;

		case -1:
			nvt_mp_seq_printf(m, " FAIL!\n");
			nvt_mp_seq_printf(m, "RecordResult:\n");
			for (i = 0; i < y_len; i++) {
				for (j = 0; j < x_len; j++) {
					iArrayIndex = i * x_len + j;
					seq_printf(m, "0x%02X, ", RecordResult[iArrayIndex]);
				}
				if (!nvt_mp_test_result_printed)
					nvt_print_result_log_in_one_line(RecordResult + i * x_len, x_len);
				nvt_mp_seq_printf(m, "\n");
			}
#if !NVT_SAVE_TEST_DATA_IN_FILE
print_test_data:
#endif /* #if !NVT_SAVE_TEST_DATA_IN_FILE */
			nvt_mp_seq_printf(m, "ReadData:\n");
			for (i = 0; i < y_len; i++) {
				for (j = 0; j < x_len; j++) {
					iArrayIndex = i * x_len + j;
					seq_printf(m, "%6d,", rawdata[iArrayIndex]);
				}
				if (!nvt_mp_test_result_printed)
					nvt_print_data_log_in_one_line(rawdata + i * x_len, x_len);
				nvt_mp_seq_printf(m, "\n");
			}
			break;
	}
	nvt_mp_seq_printf(m, "\n");
}

/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/

/*******************************************************
Description:
	Novatek touchscreen self-test sequence print show
	function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
static int32_t c_show_selftest(struct seq_file *m, void *v)
{
	int32_t i;

	NVT_LOG("++\n");

	if (ts->pen_support) {
		if ((TestResult_Short == 0) && (TestResult_Open == 0) &&
			(TestResult_FW_Rawdata == 0) && (TestResult_FW_CC == 0) &&
			(TestResult_Noise == 0) && (TestResult_Digital_Noise == 0) &&
			(TestResult_Rawdata_Flatness == 0) &&
			(TestResult_Pen_FW_Raw == 0) && (TestResult_Pen_Noise == 0)) {
			nvt_mp_seq_printf(m, "Selftest PASS.\n\n");
		} else {
			nvt_mp_seq_printf(m, "Selftest FAIL!\n\n");
		}
	} else {
		if ((TestResult_Short == 0) && (TestResult_Open == 0) &&
			(TestResult_FW_Rawdata == 0) && (TestResult_FW_CC == 0) &&
			(TestResult_Noise == 0) && (TestResult_Digital_Noise == 0) &&
			(TestResult_Rawdata_Flatness == 0)) {
			nvt_mp_seq_printf(m, "Selftest PASS.\n\n");
		} else {
			nvt_mp_seq_printf(m, "Selftest FAIL!\n\n");
		}
	}

	nvt_mp_seq_printf(m, "FW Version: %d, NVT PID: 0x%04X\n\n", fw_ver, nvt_pid);

	nvt_mp_seq_printf(m, "Short Test");
#if NVT_SAVE_TEST_DATA_IN_FILE
	if ((TestResult_Short == 0) || (TestResult_Short == 1)) {
		print_selftest_result(m, TestResult_Short, RecordResult_Short_TXRX, RawData_Short_TXRX, ts->x_num, ts->y_num);
	} else { // TestResult_Short is -1
		nvt_mp_seq_printf(m, " FAIL!\n");
		if (TestResult_Short_TXRX == -1) {
			nvt_mp_seq_printf(m, "Short TXRX");
			print_selftest_result(m, TestResult_Short_TXRX, RecordResult_Short_TXRX, RawData_Short_TXRX, ts->x_num, ts->y_num);
		}
		if (TestResult_Short_TXTX == -1) {
			nvt_mp_seq_printf(m, "Short TXTX");
			print_selftest_result(m, TestResult_Short_TXTX, RecordResult_Short_TXTX, RawData_Short_TXTX, ts->x_num, 1);
		}
		if (TestResult_Short_RXRX == -1) {
			nvt_mp_seq_printf(m, "Short RXRX");
			print_selftest_result(m, TestResult_Short_RXRX, RecordResult_Short_RXRX, RawData_Short_RXRX, 1, ts->y_num);
		}
	}
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	if (TestResult_Short == 0)
		nvt_mp_seq_printf(m, " PASS.\n");
	else if (TestResult_Short == 1)
		nvt_mp_seq_printf(m, " ERROR! Read Data FAIL!\n");
	else
		nvt_mp_seq_printf(m, " FAIL!\n");
	nvt_mp_seq_printf(m, "Short TXRX");
	print_selftest_result(m, TestResult_Short_TXRX, RecordResult_Short_TXRX, RawData_Short_TXRX, ts->x_num, ts->y_num);
	nvt_mp_seq_printf(m, "Short TXTX");
	print_selftest_result(m, TestResult_Short_TXTX, RecordResult_Short_TXTX, RawData_Short_TXTX, ts->x_num, 1);
	nvt_mp_seq_printf(m, "Short RXRX");
	print_selftest_result(m, TestResult_Short_RXRX, RecordResult_Short_RXRX, RawData_Short_RXRX, 1, ts->y_num);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	nvt_mp_seq_printf(m, "Open Test");
#if NVT_SAVE_TEST_DATA_IN_FILE
	if ((TestResult_Open == 0) || (TestResult_Open == 1)) {
		print_selftest_result(m, TestResult_Open, RecordResult_Open_Mutual, RawData_Open_Mutual, ts->x_num, ts->y_num);
	} else { // TestResult_Open is -1
		nvt_mp_seq_printf(m, " FAIL!\n");
		if (TestResult_Open_Mutual == -1) {
			nvt_mp_seq_printf(m, "Open Mutual");
			print_selftest_result(m, TestResult_Open_Mutual, RecordResult_Open_Mutual, RawData_Open_Mutual, ts->x_num, ts->y_num);
		}
		if (TestResult_Open_SelfTX == -1) {
			nvt_mp_seq_printf(m, "Open SelfTX");
			print_selftest_result(m, TestResult_Open_SelfTX, RecordResult_Open_SelfTX, RawData_Open_SelfTX, ts->x_num, 1);
		}
		if (TestResult_Open_SelfRX == -1) {
			nvt_mp_seq_printf(m, "Open SelfRX");
			print_selftest_result(m, TestResult_Open_SelfRX, RecordResult_Open_SelfRX, RawData_Open_SelfRX, 1, ts->y_num);
		}
	}
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	if (TestResult_Open == 0)
		nvt_mp_seq_printf(m, " PASS.\n");
	else if (TestResult_Open == 1)
		nvt_mp_seq_printf(m, " ERROR! Read Data FAIL!\n");
	else
		nvt_mp_seq_printf(m, " FAIL!\n");
	nvt_mp_seq_printf(m, "Open Mutual");
	print_selftest_result(m, TestResult_Open_Mutual, RecordResult_Open_Mutual, RawData_Open_Mutual, ts->x_num, ts->y_num);
	nvt_mp_seq_printf(m, "Open SelfTX");
	print_selftest_result(m, TestResult_Open_SelfTX, RecordResult_Open_SelfTX, RawData_Open_SelfTX, ts->x_num, 1);
	nvt_mp_seq_printf(m, "Open SelfRX");
	print_selftest_result(m, TestResult_Open_SelfRX, RecordResult_Open_SelfRX, RawData_Open_SelfRX, 1, ts->y_num);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	for (i = 0; i < 4; i++) {
		if (i == 0) {
			nvt_mp_seq_printf(m, "F%d FW Rawdata Test", i+1);
			print_selftest_result(m, TestResult_FW_Rawdata, RecordResult_FW_Rawdata, RawData_FW_Rawdata[0], ts->x_num, ts->y_num);
		} else {
			nvt_mp_seq_printf(m, "F%d FW Rawdata", i+1);
			print_selftest_result(m, 0xDCDCDCDC, NULL, RawData_FW_Rawdata[i], ts->x_num, ts->y_num);
		}
		nvt_mp_seq_printf(m, "F%d Rawdata Flatness Info", i+1);
		if (rawdata_flatness_info_support)
			print_selftest_result(m, 0xDCDCDCDC, NULL, Rawdata_Flatness_Info[i], RAWDATA_FLATNESS_INFO_ITEM_NUM, 1);
		else
			nvt_mp_seq_printf(m, " NOT SUPPORT.\n");
	}
/*P16 code for HQFEAT-93999 by xiongdejun at 2025/6/27 start*/
	if (rawdata_flatness_info_support) {
		nvt_mp_seq_printf(m, "Rawdata Flatness Test");
		print_selftest_result(m, TestResult_Rawdata_Flatness, RecordResult_Rawdata_Flatness, Rawdata_FlatnessValueOper1, 4, 1);
	}
/*P16 code for HQFEAT-93999 by xiongdejun at 2025/6/27 end*/
	nvt_mp_seq_printf(m, "FW CC Test");
	print_selftest_result(m, TestResult_FW_CC, RecordResult_FW_CC, RawData_FW_CC, ts->x_num, ts->y_num);
	nvt_mp_seq_printf(m, "TX Trim CC");
	print_selftest_result(m, 0xDCDCDCDC, NULL, RawData_TX_Trim_CC, ts->x_num, 1);
	nvt_mp_seq_printf(m, "RX Trim CC");
	print_selftest_result(m, 0xDCDCDCDC, NULL, RawData_RX_Trim_CC, 1, ts->y_num);
	nvt_mp_seq_printf(m, "Golden CC");
	print_selftest_result(m, 0xDCDCDCDC, NULL, RawData_Golden_CC, ts->x_num, ts->y_num);
	nvt_mp_seq_printf(m, "TX Golden CC");
	print_selftest_result(m, 0xDCDCDCDC, NULL, RawData_TX_Golden_CC, ts->x_num, 1);
	nvt_mp_seq_printf(m, "RX Golden CC");
	print_selftest_result(m, 0xDCDCDCDC, NULL, RawData_RX_Golden_CC, 1, ts->y_num);

	nvt_mp_seq_printf(m, "Noise Test");
#if NVT_SAVE_TEST_DATA_IN_FILE
	if ((TestResult_Noise == 0) || (TestResult_Noise == 1)) {
		print_selftest_result(m, TestResult_FW_DiffMax, RecordResult_FW_DiffMax, RawData_Diff_Max, ts->x_num, ts->y_num);
	} else { // TestResult_Noise is -1
		nvt_mp_seq_printf(m, " FAIL!\n");
		if (TestResult_FW_DiffMax == -1) {
			nvt_mp_seq_printf(m, "FW Diff Max");
			print_selftest_result(m, TestResult_FW_DiffMax, RecordResult_FW_DiffMax, RawData_Diff_Max, ts->x_num, ts->y_num);
		}
		if (TestResult_FW_DiffMin == -1) {
			nvt_mp_seq_printf(m, "FW Diff Min");
			print_selftest_result(m, TestResult_FW_DiffMin, RecordResult_FW_DiffMin, RawData_Diff_Min, ts->x_num, ts->y_num);
		}
	}
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	if (TestResult_Noise == 0)
		nvt_mp_seq_printf(m, " PASS.\n");
	else if (TestResult_Noise == 1)
		nvt_mp_seq_printf(m, " ERROR! Read Data FAIL!\n");
	else
		nvt_mp_seq_printf(m, " FAIL!\n");
	nvt_mp_seq_printf(m, "FW Diff Max");
	print_selftest_result(m, TestResult_FW_DiffMax, RecordResult_FW_DiffMax, RawData_Diff_Max, ts->x_num, ts->y_num);
	nvt_mp_seq_printf(m, "FW Diff Min");
	print_selftest_result(m, TestResult_FW_DiffMin, RecordResult_FW_DiffMin, RawData_Diff_Min, ts->x_num, ts->y_num);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	nvt_mp_seq_printf(m, "Digital Noise Test");
#if NVT_SAVE_TEST_DATA_IN_FILE
	if ((TestResult_Digital_Noise == 0) || (TestResult_Digital_Noise == 1)) {
		print_selftest_result(m, TestResult_FW_Digital_DiffMax[0], RecordResult_FW_Digital_DiffMax[0], RawData_Digital_Diff_Max[0], ts->x_num, ts->y_num);
	} else { // TestResult_Digital_Noise is -1
		nvt_mp_seq_printf(m, " FAIL!\n");
		for (i = 0; i < 3; i++) {
			if (TestResult_FW_Digital_DiffMax[i] == -1) {
				nvt_mp_seq_printf(m, "FW Digital Diff Max F%d", i+2);
				print_selftest_result(m, TestResult_FW_Digital_DiffMax[i], RecordResult_FW_Digital_DiffMax[i], RawData_Digital_Diff_Max[i], ts->x_num, ts->y_num);
			}
			if (TestResult_FW_Digital_DiffMin[i] == -1) {
				nvt_mp_seq_printf(m, "FW Digital Diff Min F%d", i+2);
				print_selftest_result(m, TestResult_FW_Digital_DiffMin[i], RecordResult_FW_Digital_DiffMin[i], RawData_Digital_Diff_Min[i], ts->x_num, ts->y_num);
			}
		}
	}
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	if (TestResult_Digital_Noise == 0)
		nvt_mp_seq_printf(m, " PASS.\n");
	else if (TestResult_Digital_Noise == 1)
		nvt_mp_seq_printf(m, " ERROR! Read Data FAIL!\n");
	else
		nvt_mp_seq_printf(m, " FAIL!\n");
	for (i = 0; i < 3; i++) {
		nvt_mp_seq_printf(m, "FW Digital Diff Max F%d", i+2);
		print_selftest_result(m, TestResult_FW_Digital_DiffMax[i], RecordResult_FW_Digital_DiffMax[i], RawData_Digital_Diff_Max[i], ts->x_num, ts->y_num);
		nvt_mp_seq_printf(m, "FW Digital Diff Min F%d", i+2);
		print_selftest_result(m, TestResult_FW_Digital_DiffMin[i], RecordResult_FW_Digital_DiffMin[i], RawData_Digital_Diff_Min[i], ts->x_num, ts->y_num);
	}
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

	if (ts->pen_support) {
		nvt_mp_seq_printf(m, "Pen FW Rawdata Test");
#if NVT_SAVE_TEST_DATA_IN_FILE
		if ((TestResult_Pen_FW_Raw == 0) || (TestResult_Pen_FW_Raw == 1)) {
			print_selftest_result(m, TestResult_Pen_FW_Raw, RecordResult_PenTipX_Raw, RawData_PenTipX_Raw, ts->pen_x_num_x, ts->pen_x_num_y);
		} else { // TestResult_Pen_FW_Raw is -1
			nvt_mp_seq_printf(m, " FAIL!\n");
			if (TestResult_PenTipX_Raw == -1) {
				nvt_mp_seq_printf(m, "Pen Tip X Raw");
				print_selftest_result(m, TestResult_PenTipX_Raw, RecordResult_PenTipX_Raw, RawData_PenTipX_Raw, ts->pen_x_num_x, ts->pen_x_num_y);
			}
			if (TestResult_PenTipY_Raw == -1) {
				nvt_mp_seq_printf(m, "Pen Tip Y Raw");
				print_selftest_result(m, TestResult_PenTipY_Raw, RecordResult_PenTipY_Raw, RawData_PenTipY_Raw, ts->pen_y_num_x, ts->pen_y_num_y);
			}
			if (TestResult_PenRingX_Raw == -1) {
				nvt_mp_seq_printf(m, "Pen Ring X Raw");
				print_selftest_result(m, TestResult_PenRingX_Raw, RecordResult_PenRingX_Raw, RawData_PenRingX_Raw, ts->pen_x_num_x, ts->pen_x_num_y);
			}
			if (TestResult_PenRingY_Raw == -1) {
				nvt_mp_seq_printf(m, "Pen Ring Y Raw");
				print_selftest_result(m, TestResult_PenRingY_Raw, RecordResult_PenRingY_Raw, RawData_PenRingY_Raw, ts->pen_y_num_x, ts->pen_y_num_y);
			}
		}
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
		if (TestResult_Pen_FW_Raw == 0)
			nvt_mp_seq_printf(m, " PASS.\n");
		else if (TestResult_Pen_FW_Raw == 1)
			nvt_mp_seq_printf(m, " ERROR! Read Data FAIL!\n");
		else
			nvt_mp_seq_printf(m, " FAIL!\n");
		nvt_mp_seq_printf(m, "Pen Tip X Raw");
		print_selftest_result(m, TestResult_PenTipX_Raw, RecordResult_PenTipX_Raw, RawData_PenTipX_Raw, ts->pen_x_num_x, ts->pen_x_num_y);
		nvt_mp_seq_printf(m, "Pen Tip Y Raw");
		print_selftest_result(m, TestResult_PenTipY_Raw, RecordResult_PenTipY_Raw, RawData_PenTipY_Raw, ts->pen_y_num_x, ts->pen_y_num_y);
		nvt_mp_seq_printf(m, "Pen Ring X Raw");
		print_selftest_result(m, TestResult_PenRingX_Raw, RecordResult_PenRingX_Raw, RawData_PenRingX_Raw, ts->pen_x_num_x, ts->pen_x_num_y);
		nvt_mp_seq_printf(m, "Pen Ring Y Raw");
		print_selftest_result(m, TestResult_PenRingY_Raw, RecordResult_PenRingY_Raw, RawData_PenRingY_Raw, ts->pen_y_num_x, ts->pen_y_num_y);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */

		nvt_mp_seq_printf(m, "Pen Noise Test");
#if NVT_SAVE_TEST_DATA_IN_FILE
		if ((TestResult_Pen_Noise == 0) || (TestResult_Pen_Noise == 1)) {
			print_selftest_result(m, TestResult_Pen_Noise, RecordResult_PenTipX_DiffMax, RawData_PenTipX_DiffMax, ts->pen_x_num_x, ts->pen_x_num_y);
		} else { // TestResult_Pen_Noise is -1
			nvt_mp_seq_printf(m, " FAIL!\n");
			if (TestResult_PenTipX_DiffMax == -1) {
				nvt_mp_seq_printf(m, "Pen Tip X Diff Max");
				print_selftest_result(m, TestResult_PenTipX_DiffMax, RecordResult_PenTipX_DiffMax, RawData_PenTipX_DiffMax, ts->pen_x_num_x, ts->pen_x_num_y);
			}
			if (TestResult_PenTipX_DiffMin == -1) {
				nvt_mp_seq_printf(m, "Pen Tip X Diff Min");
				print_selftest_result(m, TestResult_PenTipX_DiffMin, RecordResult_PenTipX_DiffMin, RawData_PenTipX_DiffMin, ts->pen_x_num_x, ts->pen_x_num_y);
			}
			if (TestResult_PenTipY_DiffMax == -1) {
				nvt_mp_seq_printf(m, "Pen Tip Y Diff Max");
				print_selftest_result(m, TestResult_PenTipY_DiffMax, RecordResult_PenTipY_DiffMax, RawData_PenTipY_DiffMax, ts->pen_y_num_x, ts->pen_y_num_y);
			}
			if (TestResult_PenTipY_DiffMin == -1) {
				nvt_mp_seq_printf(m, "Pen Tip Y Diff Min");
				print_selftest_result(m, TestResult_PenTipY_DiffMin, RecordResult_PenTipY_DiffMin, RawData_PenTipY_DiffMin, ts->pen_y_num_x, ts->pen_y_num_y);
			}
			if (TestResult_PenRingX_DiffMax == -1) {
				nvt_mp_seq_printf(m, "Pen Ring X Diff Max");
				print_selftest_result(m, TestResult_PenRingX_DiffMax, RecordResult_PenRingX_DiffMax, RawData_PenRingX_DiffMax, ts->pen_x_num_x, ts->pen_x_num_y);
			}
			if (TestResult_PenRingX_DiffMin == -1) {
				nvt_mp_seq_printf(m, "Pen Ring X Diff Min");
				print_selftest_result(m, TestResult_PenRingX_DiffMin, RecordResult_PenRingX_DiffMin, RawData_PenRingX_DiffMin, ts->pen_x_num_x, ts->pen_x_num_y);
			}
			if (TestResult_PenRingY_DiffMax == -1) {
				nvt_mp_seq_printf(m, "Pen Ring Y Diff Max");
				print_selftest_result(m, TestResult_PenRingY_DiffMax, RecordResult_PenRingY_DiffMax, RawData_PenRingY_DiffMax, ts->pen_y_num_x, ts->pen_y_num_y);
			}
			if (TestResult_PenRingY_DiffMin == -1) {
				nvt_mp_seq_printf(m, "Pen Ring Y Diff Min");
				print_selftest_result(m, TestResult_PenRingY_DiffMin, RecordResult_PenRingY_DiffMin, RawData_PenRingY_DiffMin, ts->pen_y_num_x, ts->pen_y_num_y);
			}
		}
#else /* #if NVT_SAVE_TEST_DATA_IN_FILE */
		if (TestResult_Pen_Noise == 0)
			nvt_mp_seq_printf(m, " PASS.\n");
		else if (TestResult_Pen_Noise == 1)
			nvt_mp_seq_printf(m, " ERROR! Read Data FAIL!\n");
		else
			nvt_mp_seq_printf(m, " FAIL!\n");
		nvt_mp_seq_printf(m, "Pen Tip X Diff Max");
		print_selftest_result(m, TestResult_PenTipX_DiffMax, RecordResult_PenTipX_DiffMax, RawData_PenTipX_DiffMax, ts->pen_x_num_x, ts->pen_x_num_y);
		nvt_mp_seq_printf(m, "Pen Tip X Diff Min");
		print_selftest_result(m, TestResult_PenTipX_DiffMin, RecordResult_PenTipX_DiffMin, RawData_PenTipX_DiffMin, ts->pen_x_num_x, ts->pen_x_num_y);
		nvt_mp_seq_printf(m, "Pen Tip Y Diff Max");
		print_selftest_result(m, TestResult_PenTipY_DiffMax, RecordResult_PenTipY_DiffMax, RawData_PenTipY_DiffMax, ts->pen_y_num_x, ts->pen_y_num_y);
		nvt_mp_seq_printf(m, "Pen Tip Y Diff Min");
		print_selftest_result(m, TestResult_PenTipY_DiffMin, RecordResult_PenTipY_DiffMin, RawData_PenTipY_DiffMin, ts->pen_y_num_x, ts->pen_y_num_y);
		nvt_mp_seq_printf(m, "Pen Ring X Diff Max");
		print_selftest_result(m, TestResult_PenRingX_DiffMax, RecordResult_PenRingX_DiffMax, RawData_PenRingX_DiffMax, ts->pen_x_num_x, ts->pen_x_num_y);
		nvt_mp_seq_printf(m, "Pen Ring X Diff Min");
		print_selftest_result(m, TestResult_PenRingX_DiffMin, RecordResult_PenRingX_DiffMin, RawData_PenRingX_DiffMin, ts->pen_x_num_x, ts->pen_x_num_y);
		nvt_mp_seq_printf(m, "Pen Ring Y Diff Max");
		print_selftest_result(m, TestResult_PenRingY_DiffMax, RecordResult_PenRingY_DiffMax, RawData_PenRingY_DiffMax, ts->pen_y_num_x, ts->pen_y_num_y);
		nvt_mp_seq_printf(m, "Pen Ring Y Diff Min");
		print_selftest_result(m, TestResult_PenRingY_DiffMin, RecordResult_PenRingY_DiffMin, RawData_PenRingY_DiffMin, ts->pen_y_num_x, ts->pen_y_num_y);
#endif /* #if NVT_SAVE_TEST_DATA_IN_FILE */
	} /* if (ts->pen_support) */

	nvt_mp_test_result_printed = 1;

	NVT_LOG("--\n");

	return 0;
}
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
static int32_t hq_show_selftest(struct seq_file *m, void *v)
{
	char test_result_bmp[7] = { 0 };
        int i;
	NVT_LOG("++\n");
	nvt_mp_printf("FW Version: %d\n\n", fw_ver);
	nvt_mp_printf("SPI communication test");
	if (TestResult_SPI_Comm == 0) {
		test_result_bmp[0] = 'P';
		nvt_mp_printf("Pass!\n");
	} else {
		test_result_bmp[0] = 'F';
		nvt_mp_printf("Fail!\n");
	}
	nvt_mp_printf("Short Test");
	if (TestResult_Short == 0) {
		test_result_bmp[3] = 'P';
		nvt_mp_printf("Pass!\n");
	} else {
		test_result_bmp[3] = 'F';
		nvt_mp_printf("Fail!\n");
	}
	print_selftest_result(m, TestResult_Short, RecordResult_Short,
			      RawData_Short, X_Channel, Y_Channel);
	nvt_mp_printf("Open Test");
	if (TestResult_Open == 0) {
		test_result_bmp[2] = 'P';
		nvt_mp_printf("Pass!\n");
	} else {
		test_result_bmp[2] = 'F';
		nvt_mp_printf("Fail!\n");
	}
	print_selftest_result(m, TestResult_Open, RecordResult_Open,
			      RawData_Open, X_Channel, Y_Channel);
	nvt_mp_printf("FW Rawdata Test");
	if (TestResult_Cap_Rawdata == 0) {
		test_result_bmp[1] = 'P';
                for (i = 0; i < 4; i++) {
		if (i == 0) {
			nvt_mp_seq_printf(m, "F%d FW Rawdata Test", i+1);
			print_selftest_result(m, TestResult_FW_Rawdata, RecordResult_FW_Rawdata, RawData_FW_Rawdata[0], ts->x_num, ts->y_num);
		} else {
			nvt_mp_seq_printf(m, "F%d FW Rawdata", i+1);
			print_selftest_result(m, 0xDCDCDCDC, NULL, RawData_FW_Rawdata[i], ts->x_num, ts->y_num);
		}
		nvt_mp_seq_printf(m, "F%d Rawdata Flatness Info", i+1);
		if (rawdata_flatness_info_support)
			print_selftest_result(m, 0xDCDCDCDC, NULL, Rawdata_Flatness_Info[i], RAWDATA_FLATNESS_INFO_ITEM_NUM, 1);
		else
			nvt_mp_seq_printf(m, " NOT SUPPORT.\n");
	        }
		nvt_mp_printf("Pass!\n");
	} else { // TestResult_Cap_Rawdata is -1
		test_result_bmp[1] = 'F';
		nvt_mp_printf("FAIL!\n");
		if (TestResult_FW_Rawdata == -1) {
			nvt_mp_printf("FW Rawdata");
                        for (i = 0; i < 4; i++) {
			if (i == 0) {
				nvt_mp_seq_printf(m, "F%d FW Rawdata Test", i+1);
				print_selftest_result(m, TestResult_FW_Rawdata, RecordResult_FW_Rawdata, RawData_FW_Rawdata[0], ts->x_num, ts->y_num);
			} else {
				nvt_mp_seq_printf(m, "F%d FW Rawdata", i+1);
				print_selftest_result(m, 0xDCDCDCDC, NULL, RawData_FW_Rawdata[i], ts->x_num, ts->y_num);
			}
			nvt_mp_seq_printf(m, "F%d Rawdata Flatness Info", i+1);
			if (rawdata_flatness_info_support)
				print_selftest_result(m, 0xDCDCDCDC, NULL, Rawdata_Flatness_Info[i], RAWDATA_FLATNESS_INFO_ITEM_NUM, 1);
			else
				nvt_mp_seq_printf(m, " NOT SUPPORT.\n");
	        	}
		}
		if (TestResult_FW_CC == -1) {
			nvt_mp_printf("FW CC");
			print_selftest_result(m, TestResult_FW_CC, RecordResult_FW_CC,
					      RawData_FW_CC, X_Channel, Y_Channel);
		}
	}
	nvt_mp_printf("Noise Test");
	if (TestResult_Noise == 0) {
		test_result_bmp[4] = 'P';
		nvt_mp_printf("Pass!");
		print_selftest_result(m, TestResult_FW_DiffMax, RecordResult_FW_DiffMax,
				      RawData_Diff_Max, X_Channel, Y_Channel);
	} else { // TestResult_Noise is -1
		test_result_bmp[4] = 'F';
		nvt_mp_printf("FAIL!\n");
		if (TestResult_FW_DiffMax == -1) {
			nvt_mp_printf("FW Diff Max");
			print_selftest_result(m, TestResult_FW_DiffMax, RecordResult_FW_DiffMax,
					      RawData_Diff_Max, X_Channel, Y_Channel);
		}
		if (TestResult_FW_DiffMin == -1) {
			nvt_mp_printf("FW Diff Min");
			print_selftest_result(m, TestResult_FW_DiffMin, RecordResult_FW_DiffMin,
					      RawData_Diff_Min, X_Channel, Y_Channel);
		}
	}
	nvt_mp_printf("Digital Noise Test");
	if (TestResult_Digital_Noise == 0) {
		test_result_bmp[5] = 'P';
          	for (i = 0; i < 3; i++) {
		      nvt_mp_seq_printf(m, "FW Digital Diff Max F%d", i+2);
		      print_selftest_result(m, TestResult_FW_Digital_DiffMax[i], RecordResult_FW_Digital_DiffMax[i], RawData_Digital_Diff_Max[i], ts->x_num, ts->y_num);
		      nvt_mp_seq_printf(m, "FW Digital Diff Min F%d", i+2);
		      print_selftest_result(m, TestResult_FW_Digital_DiffMin[i], RecordResult_FW_Digital_DiffMin[i], RawData_Digital_Diff_Min[i], ts->x_num, ts->y_num);
		}
		nvt_mp_printf("Pass!\n");
	} else {
		test_result_bmp[5] = 'F';
          	for (i = 0; i < 3; i++) {
		      nvt_mp_seq_printf(m, "FW Digital Diff Max F%d", i+2);
		      print_selftest_result(m, TestResult_FW_Digital_DiffMax[i], RecordResult_FW_Digital_DiffMax[i], RawData_Digital_Diff_Max[i], ts->x_num, ts->y_num);
		      nvt_mp_seq_printf(m, "FW Digital Diff Min F%d", i+2);
		      print_selftest_result(m, TestResult_FW_Digital_DiffMin[i], RecordResult_FW_Digital_DiffMin[i], RawData_Digital_Diff_Min[i], ts->x_num, ts->y_num);
		}
		nvt_mp_printf("Fail!\n");
	}
/*P16 code for HQFEAT-93999 by xiongdejun at 2025/6/27 start*/
	if(TestResult_Rawdata_Flatness == 0){
		test_result_bmp[6] = 'P';
		nvt_mp_printf("Pass!\n");
		print_selftest_result(m, TestResult_Rawdata_Flatness, RecordResult_Rawdata_Flatness, Rawdata_FlatnessValueOper1, 4, 1);
	}else{
		test_result_bmp[6] = 'F';
		nvt_mp_printf("FAIL!\n");
		print_selftest_result(m, TestResult_Rawdata_Flatness, RecordResult_Rawdata_Flatness, Rawdata_FlatnessValueOper1, 4, 1);
	}

	seq_printf(m, "0%c-1%c-2%c-3%c-4%c-5%c-6%c\n",
		test_result_bmp[0],
		test_result_bmp[1],
		test_result_bmp[2],
		test_result_bmp[3],
		test_result_bmp[4],
        test_result_bmp[5],
		test_result_bmp[6]);
	nvt_mp_test_result_printed = 1;
	NVT_LOG("--\n");
/*P16 code for HQFEAT-93999 by xiongdejun at 2025/6/27 end*/
    return 0;
}

/*******************************************************
Description:
	Novatek touchscreen self-test sequence print start
	function.

return:
	Executive outcomes. 1---call next function.
	NULL---not call next function and sequence loop
	stop.
*******************************************************/
static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

/*******************************************************
Description:
	Novatek touchscreen self-test sequence print next
	function.

return:
	Executive outcomes. NULL---no next and call sequence
	stop function.
*******************************************************/
static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

/*******************************************************
Description:
	Novatek touchscreen self-test sequence print stop
	function.

return:
	n.a.
*******************************************************/
static void c_stop(struct seq_file *m, void *v)
{
	return;
}

const struct seq_operations nvt_selftest_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_show_selftest
};
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
const struct seq_operations hq_selftest_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = hq_show_selftest
};
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/
static int32_t c_show_selftest_fail(struct seq_file *m, void *v)
{
	NVT_LOG("++\n");

	nvt_mp_seq_printf(m, "Selftest FAIL!\n\n");
	nvt_mp_seq_printf(m, "FW Version: %d, NVT PID: 0x%04X\n\n", fw_ver, nvt_pid);
	nvt_mp_test_result_printed = 1;

	NVT_LOG("--\n");
	return 0;
}

const struct seq_operations nvt_selftest_fail_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_show_selftest_fail
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_selftest open function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
static int32_t nvt_selftest_open(struct inode *inode, struct file *file)
{
	struct device_node *np = ts->client->dev.of_node;
	unsigned char mpcriteria[32] = {0};	//novatek-mp-criteria-default
	int32_t i;

	TestResult_Short = 0;
	TestResult_Short_TXRX = 0;
	TestResult_Short_TXTX = 0;
	TestResult_Short_RXRX = 0;
	TestResult_Open = 0;
	TestResult_Open_Mutual = 0;
	TestResult_Open_SelfTX = 0;
	TestResult_Open_SelfRX = 0;
	TestResult_FW_Rawdata = 0;
	TestResult_FW_CC = 0;
	TestResult_Noise = 0;
	TestResult_FW_DiffMax = 0;
	TestResult_FW_DiffMin = 0;
	TestResult_Digital_Noise = 0;
	for (i = 0; i < 3; i++) {
		TestResult_FW_Digital_DiffMax[i] = 0;
		TestResult_FW_Digital_DiffMin[i] = 0;
	}
	TestResult_Rawdata_Flatness = 0;
	if (ts->pen_support) {
		TestResult_Pen_FW_Raw = 0;
		TestResult_PenTipX_Raw = 0;
		TestResult_PenTipY_Raw = 0;
		TestResult_PenRingX_Raw = 0;
		TestResult_PenRingY_Raw = 0;
		TestResult_Pen_Noise = 0;
		TestResult_PenTipX_DiffMax = 0;
		TestResult_PenTipX_DiffMin = 0;
		TestResult_PenTipY_DiffMax = 0;
		TestResult_PenTipY_DiffMin = 0;
		TestResult_PenRingX_DiffMax = 0;
		TestResult_PenRingX_DiffMin = 0;
		TestResult_PenRingY_DiffMax = 0;
		TestResult_PenRingY_DiffMin = 0;
	} /* if (ts->pen_support) */
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 start*/
	if(ts->nvt_tool_in_use){
		NVT_ERR("NVT tool in use.\n");
		return -EBUSY;
	}
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 end*/
	NVT_LOG("++\n");

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	//---Download MP FW---
	if (nvt_update_firmware(MP_UPDATE_FIRMWARE_NAME, false) < 0) {
		NVT_ERR("update mp firmware failed!\n");
		goto failed_out;
	}

	if (nvt_get_fw_info()) {
		NVT_ERR("get fw info failed!\n");
		goto failed_out;
	}

	fw_ver = ts->fw_ver;
	nvt_pid = ts->nvt_pid;

	/* Parsing criteria from dts */
	if(of_property_read_bool(np, "novatek,mp-support-dt")) {
		/*
		 * Parsing Criteria by Novatek PID
		 * The string rule is "novatek-mp-criteria-<nvt_pid>"
		 * nvt_pid is 2 bytes (show hex).
		 *
		 * Ex. nvt_pid = 500A
		 *     mpcriteria = "novatek-mp-criteria-500A"
		 */
		snprintf(mpcriteria, sizeof(mpcriteria), "novatek-mp-criteria-%04X", ts->nvt_pid);

		if (nvt_mp_parse_dt(np, mpcriteria)) {
			//---Download Normal FW---
			nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME, false);
			mutex_unlock(&ts->lock);
			NVT_ERR("mp parse device tree failed!\n");
			return -EINVAL;
		}
	} else {
		NVT_LOG("Not found novatek,mp-support-dt, use default setting\n");
		//---Print Test Criteria---
		nvt_print_criteria();
	}

	if (nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN)) {
		NVT_ERR("check fw reset state failed!\n");
		goto failed_out;
	}

	if (nvt_switch_FreqHopEnDis(FREQ_HOP_DISABLE)) {
		NVT_ERR("switch frequency hopping disable failed!\n");
		goto failed_out;
	}

	if (nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN)) {
		NVT_ERR("check fw reset state failed!\n");
		goto failed_out;
	}

	msleep(100);

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		NVT_ERR("clear fw status failed!\n");
		goto failed_out;
	}

	nvt_change_mode(MP_MODE_CC);

	if (nvt_check_fw_status()) {
		NVT_ERR("check fw status failed!\n");
		goto failed_out;
	}

	//---FW Rawdata Test---
	if (nvt_read_fw_rawdata(RawData_FW_Rawdata[0]) != 0) {
		TestResult_FW_Rawdata = 1;
		goto failed_out;
	} else {
		TestResult_FW_Rawdata = RawDataTest_SinglePoint_Sub(RawData_FW_Rawdata[0], RecordResult_FW_Rawdata, ts->x_num, ts->y_num,
												PS_Config_Lmt_FW_Rawdata_P, PS_Config_Lmt_FW_Rawdata_N);
	}
	nvt_get_rawdata_flatness_info(Rawdata_Flatness_Info[0]);
	Rawdata_FlatnessValueOper1[0] = Rawdata_Flatness_Info[0][5];

	if (nvt_read_CC(RawData_FW_CC) != 0) {
		TestResult_FW_CC = 1;
		goto failed_out;
	} else {
		TestResult_FW_CC = RawDataTest_SinglePoint_Sub(RawData_FW_CC, RecordResult_FW_CC, ts->x_num, ts->y_num,
											PS_Config_Lmt_FW_CC_P, PS_Config_Lmt_FW_CC_N);
	}

	if (ts->pen_support) {
		//---Pen FW Rawdata Test---
		if (nvt_read_pen_baseline() != 0) {
			TestResult_Pen_FW_Raw = 1;
			goto failed_out;
		} else {
			TestResult_PenTipX_Raw = RawDataTest_SinglePoint_Sub(RawData_PenTipX_Raw, RecordResult_PenTipX_Raw, ts->pen_x_num_x, ts->pen_x_num_y,
											PS_Config_Lmt_PenTipX_FW_Raw_P, PS_Config_Lmt_PenTipX_FW_Raw_N);
			TestResult_PenTipY_Raw = RawDataTest_SinglePoint_Sub(RawData_PenTipY_Raw, RecordResult_PenTipY_Raw, ts->pen_y_num_x, ts->pen_y_num_y,
											PS_Config_Lmt_PenTipY_FW_Raw_P, PS_Config_Lmt_PenTipY_FW_Raw_N);
			TestResult_PenRingX_Raw = RawDataTest_SinglePoint_Sub(RawData_PenRingX_Raw, RecordResult_PenRingX_Raw, ts->pen_x_num_x, ts->pen_x_num_y,
											PS_Config_Lmt_PenRingX_FW_Raw_P, PS_Config_Lmt_PenRingX_FW_Raw_N);
			TestResult_PenRingY_Raw = RawDataTest_SinglePoint_Sub(RawData_PenRingY_Raw, RecordResult_PenRingY_Raw, ts->pen_y_num_x, ts->pen_y_num_y,
											PS_Config_Lmt_PenRingY_FW_Raw_P, PS_Config_Lmt_PenRingY_FW_Raw_N);

			if ((TestResult_PenTipX_Raw == -1) || (TestResult_PenTipY_Raw == -1) || (TestResult_PenRingX_Raw == -1) || (TestResult_PenRingY_Raw == -1))
				TestResult_Pen_FW_Raw = -1;
			else
				TestResult_Pen_FW_Raw = 0;
		}
	} /* if (ts->pen_support) */

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	//---Noise Test---
	if (nvt_read_fw_noise(RawData_Diff) != 0) {
		TestResult_Noise = 1;	// 1: ERROR
		TestResult_FW_DiffMax = 1;
		TestResult_FW_DiffMin = 1;
		if (ts->pen_support) {
			TestResult_Pen_Noise = 1;
			TestResult_PenTipX_DiffMax = 1;
			TestResult_PenTipX_DiffMin = 1;
			TestResult_PenTipY_DiffMax = 1;
			TestResult_PenTipY_DiffMin = 1;
			TestResult_PenRingX_DiffMax = 1;
			TestResult_PenRingX_DiffMin = 1;
			TestResult_PenRingY_DiffMax = 1;
			TestResult_PenRingY_DiffMin = 1;
		} /* if (ts->pen_support) */
		goto failed_out;
	} else {
		TestResult_FW_DiffMax = RawDataTest_SinglePoint_Sub(RawData_Diff_Max, RecordResult_FW_DiffMax, ts->x_num, ts->y_num,
											PS_Config_Lmt_FW_Diff_P, PS_Config_Lmt_FW_Diff_N);

		TestResult_FW_DiffMin = RawDataTest_SinglePoint_Sub(RawData_Diff_Min, RecordResult_FW_DiffMin, ts->x_num, ts->y_num,
											PS_Config_Lmt_FW_Diff_P, PS_Config_Lmt_FW_Diff_N);

		if ((TestResult_FW_DiffMax == -1) || (TestResult_FW_DiffMin == -1))
			TestResult_Noise = -1;
		else
			TestResult_Noise = 0;

		if (ts->pen_support) {
			TestResult_PenTipX_DiffMax = RawDataTest_SinglePoint_Sub(RawData_PenTipX_DiffMax, RecordResult_PenTipX_DiffMax, ts->pen_x_num_x, ts->pen_x_num_y,
											PS_Config_Lmt_PenTipX_FW_Diff_P, PS_Config_Lmt_PenTipX_FW_Diff_N);

			TestResult_PenTipX_DiffMin = RawDataTest_SinglePoint_Sub(RawData_PenTipX_DiffMin, RecordResult_PenTipX_DiffMin, ts->pen_x_num_x, ts->pen_x_num_y,
											PS_Config_Lmt_PenTipX_FW_Diff_P, PS_Config_Lmt_PenTipX_FW_Diff_N);

			TestResult_PenTipY_DiffMax = RawDataTest_SinglePoint_Sub(RawData_PenTipY_DiffMax, RecordResult_PenTipY_DiffMax, ts->pen_y_num_x, ts->pen_y_num_y,
											PS_Config_Lmt_PenTipY_FW_Diff_P, PS_Config_Lmt_PenTipY_FW_Diff_N);

			TestResult_PenTipY_DiffMin = RawDataTest_SinglePoint_Sub(RawData_PenTipY_DiffMin, RecordResult_PenTipY_DiffMin, ts->pen_y_num_x, ts->pen_y_num_y,
											PS_Config_Lmt_PenTipY_FW_Diff_P, PS_Config_Lmt_PenTipY_FW_Diff_N);

			TestResult_PenRingX_DiffMax = RawDataTest_SinglePoint_Sub(RawData_PenRingX_DiffMax, RecordResult_PenRingX_DiffMax, ts->pen_x_num_x, ts->pen_x_num_y,
											PS_Config_Lmt_PenRingX_FW_Diff_P, PS_Config_Lmt_PenRingX_FW_Diff_N);

			TestResult_PenRingX_DiffMin = RawDataTest_SinglePoint_Sub(RawData_PenRingX_DiffMin, RecordResult_PenRingX_DiffMin, ts->pen_x_num_x, ts->pen_x_num_y,
											PS_Config_Lmt_PenRingX_FW_Diff_P, PS_Config_Lmt_PenRingX_FW_Diff_N);

			TestResult_PenRingY_DiffMax = RawDataTest_SinglePoint_Sub(RawData_PenRingY_DiffMax, RecordResult_PenRingY_DiffMax, ts->pen_y_num_x, ts->pen_y_num_y,
											PS_Config_Lmt_PenRingY_FW_Diff_P, PS_Config_Lmt_PenRingY_FW_Diff_N);

			TestResult_PenRingY_DiffMin = RawDataTest_SinglePoint_Sub(RawData_PenRingY_DiffMin, RecordResult_PenRingY_DiffMin, ts->pen_y_num_x, ts->pen_y_num_y,
											PS_Config_Lmt_PenRingY_FW_Diff_P, PS_Config_Lmt_PenRingY_FW_Diff_N);

			if ((TestResult_PenTipX_DiffMax == -1) || (TestResult_PenTipX_DiffMin == -1) || (TestResult_PenTipY_DiffMax == -1) || (TestResult_PenTipY_DiffMin == -1) ||
				(TestResult_PenRingX_DiffMax == -1) || (TestResult_PenRingX_DiffMin == -1) || (TestResult_PenRingY_DiffMax == -1) || (TestResult_PenRingY_DiffMin == -1))
				TestResult_Pen_Noise = -1;
			else
				TestResult_Pen_Noise = 0;
		} /* if (ts->pen_support) */
	}
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
	//---Digital Noise Test---
	if (nvt_read_fw_digital_noise(RawData_Diff) != 0) {
		TestResult_Digital_Noise = 1;	// 1: ERROR
		for (i = 0; i < 3; i++) {
			TestResult_FW_Digital_DiffMax[i] = 1;
			TestResult_FW_Digital_DiffMin[i] = 1;
		}
		goto failed_out;
	} else {
		for (i = 0; i < 3; i++) {
			TestResult_FW_Digital_DiffMax[i] = RawDataTest_SinglePoint_Sub(RawData_Digital_Diff_Max[i], RecordResult_FW_Digital_DiffMax[i], ts->x_num, ts->y_num,
												PS_Config_Lmt_FW_Digital_Diff_P, PS_Config_Lmt_FW_Digital_Diff_N);

			TestResult_FW_Digital_DiffMin[i] = RawDataTest_SinglePoint_Sub(RawData_Digital_Diff_Min[i], RecordResult_FW_Digital_DiffMin[i], ts->x_num, ts->y_num,
												PS_Config_Lmt_FW_Digital_Diff_P, PS_Config_Lmt_FW_Digital_Diff_N);
		}

		if ((TestResult_FW_Digital_DiffMax[0] == -1) || (TestResult_FW_Digital_DiffMin[0] == -1) ||
				(TestResult_FW_Digital_DiffMax[1] == -1) || (TestResult_FW_Digital_DiffMin[1] == -1) ||
				(TestResult_FW_Digital_DiffMax[2] == -1) || (TestResult_FW_Digital_DiffMin[2] == -1))
			TestResult_Digital_Noise = -1;
		else
			TestResult_Digital_Noise = 0;
	}
/*P16 code for HQFEAT-93999 by xiongdejun at 2025/6/27 start*/
	//---Rawdata Flatness Test---
	/* need be placed after nvt_read_fw_digital_noise() due to nvt_get_rawdata_flatness_info() is called to get rawdata flatness info in that function */
	if (rawdata_flatness_info_support) {
		TestResult_Rawdata_Flatness = RawDataTest_SinglePoint_Sub(Rawdata_FlatnessValueOper1, RecordResult_Rawdata_Flatness, 4, 1,
											PS_Config_Lmt_FlatnessValueOper1_P, PS_Config_Lmt_FlatnessValueOper1_N);
	}
/*P16 code for HQFEAT-93999 by xiongdejun at 2025/6/27 end*/
	//--Short Test---
	if (nvt_read_fw_short(RawData_Short_TXRX) != 0) {
		TestResult_Short = 1; // 1:ERROR
		goto failed_out;
	} else {
		//---Self Test Check --- // 0:PASS, -1:FAIL
		TestResult_Short_TXRX = RawDataTest_SinglePoint_Sub(RawData_Short_TXRX, RecordResult_Short_TXRX, ts->x_num, ts->y_num,
										PS_Config_Lmt_Short_TXRX_P, PS_Config_Lmt_Short_TXRX_N);

		TestResult_Short_TXTX = RawDataTest_SinglePoint_Sub(RawData_Short_TXTX, RecordResult_Short_TXTX, ts->x_num, 1,
										PS_Config_Lmt_Short_TXTX_P, PS_Config_Lmt_Short_TXTX_N);

		TestResult_Short_RXRX = RawDataTest_SinglePoint_Sub(RawData_Short_RXRX, RecordResult_Short_RXRX, 1, ts->y_num,
										PS_Config_Lmt_Short_RXRX_P, PS_Config_Lmt_Short_RXRX_N);

		if ((TestResult_Short_TXRX == -1) || (TestResult_Short_TXTX == -1) || (TestResult_Short_RXRX == -1))
			TestResult_Short = -1;
		else
			TestResult_Short = 0;
	}

	//---Open Test---
	if (nvt_read_fw_open(RawData_Open_Mutual) != 0) {
		TestResult_Open = 1;    // 1:ERROR
		goto failed_out;
	} else {
		//---Self Test Check --- // 0:PASS, -1:FAIL
		TestResult_Open_Mutual = RawDataTest_SinglePoint_Sub(RawData_Open_Mutual, RecordResult_Open_Mutual, ts->x_num, ts->y_num,
											PS_Config_Lmt_Open_Mutual_P, PS_Config_Lmt_Open_Mutual_N);

		TestResult_Open_SelfTX = RawDataTest_SinglePoint_Sub(RawData_Open_SelfTX, RecordResult_Open_SelfTX, ts->x_num, 1,
											PS_Config_Lmt_Open_SelfTX_P, PS_Config_Lmt_Open_SelfTX_N);

		TestResult_Open_SelfRX = RawDataTest_SinglePoint_Sub(RawData_Open_SelfRX, RecordResult_Open_SelfRX, 1, ts->y_num,
											PS_Config_Lmt_Open_SelfRX_P, PS_Config_Lmt_Open_SelfRX_N);

		if ((TestResult_Open_Mutual == -1) || (TestResult_Open_SelfTX == -1) || (TestResult_Open_SelfRX == -1))
			TestResult_Open = -1;
		else
			TestResult_Open = 0;
	}

	//---Download Normal FW---
	nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME, false);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	nvt_mp_test_result_printed = 0;
	return seq_open(file, &nvt_selftest_seq_ops);

failed_out:
	nvt_read_fw_history_all();
/*P16 code for BUGP16-3030 by xiongdejun at 2025/5/29 start*/
	//---Download normal FW---
	nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME, false);
/*P16 code for BUGP16-3030 by xiongdejun at 2025/5/29 end*/
	mutex_unlock(&ts->lock);

	nvt_mp_test_result_printed = 0;
	return seq_open(file, &nvt_selftest_fail_seq_ops);
}
static int32_t hq_selftest_open(struct inode *inode, struct file *file)
{
	struct device_node *np = ts->client->dev.of_node;
	unsigned char mpcriteria[32] = {0};	//novatek-mp-criteria-default
	int32_t i;
	uint8_t buf[8] = {0};
	TestResult_SPI_Comm = 0;
	TestResult_Cap_Rawdata = 0;
	TestResult_Short = 0;
	TestResult_Short_TXRX = 0;
	TestResult_Short_TXTX = 0;
	TestResult_Short_RXRX = 0;
	TestResult_Open = 0;
	TestResult_Open_Mutual = 0;
	TestResult_Open_SelfTX = 0;
	TestResult_Open_SelfRX = 0;
	TestResult_FW_Rawdata = 0;
	TestResult_FW_CC = 0;
	TestResult_Noise = 0;
	TestResult_FW_DiffMax = 0;
	TestResult_FW_DiffMin = 0;
	TestResult_Digital_Noise = 0;
	for (i = 0; i < 3; i++) {
		TestResult_FW_Digital_DiffMax[i] = 0;
		TestResult_FW_Digital_DiffMin[i] = 0;
	}
	TestResult_Rawdata_Flatness = 0;
	if (ts->pen_support) {
		TestResult_Pen_FW_Raw = 0;
		TestResult_PenTipX_Raw = 0;
		TestResult_PenTipY_Raw = 0;
		TestResult_PenRingX_Raw = 0;
		TestResult_PenRingY_Raw = 0;
		TestResult_Pen_Noise = 0;
		TestResult_PenTipX_DiffMax = 0;
		TestResult_PenTipX_DiffMin = 0;
		TestResult_PenTipY_DiffMax = 0;
		TestResult_PenTipY_DiffMin = 0;
		TestResult_PenRingX_DiffMax = 0;
		TestResult_PenRingX_DiffMin = 0;
		TestResult_PenRingY_DiffMax = 0;
		TestResult_PenRingY_DiffMin = 0;
	} /* if (ts->pen_support) */
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 start*/
	if(ts->nvt_tool_in_use){
		NVT_ERR("NVT tool in use.\n");
		return -EBUSY;
	}
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 end*/
	NVT_LOG("++\n");

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */
	buf[0] = 0x00;
	if (CTP_SPI_READ(ts->client, buf, 2) < 0) {
		TestResult_SPI_Comm = -1;
		TestResult_Short = -1;
		TestResult_Open = -1;
		TestResult_FW_Rawdata = -1;
		TestResult_Noise = -1;
		goto err_nvt_spi_read;
	} else {
		TestResult_SPI_Comm = 0;
	}
	//---Download MP FW---
	if (nvt_update_firmware(MP_UPDATE_FIRMWARE_NAME, false) < 0) {
		NVT_ERR("update mp firmware failed!\n");
		goto failed_out;
	}

	if (nvt_get_fw_info()) {
		NVT_ERR("get fw info failed!\n");
		goto failed_out;
	}

	fw_ver = ts->fw_ver;
	nvt_pid = ts->nvt_pid;

	/* Parsing criteria from dts */
	if(of_property_read_bool(np, "novatek,mp-support-dt")) {
		/*
		 * Parsing Criteria by Novatek PID
		 * The string rule is "novatek-mp-criteria-<nvt_pid>"
		 * nvt_pid is 2 bytes (show hex).
		 *
		 * Ex. nvt_pid = 500A
		 *     mpcriteria = "novatek-mp-criteria-500A"
		 */
		snprintf(mpcriteria, sizeof(mpcriteria), "novatek-mp-criteria-%04X", ts->nvt_pid);

		if (nvt_mp_parse_dt(np, mpcriteria)) {
			//---Download Normal FW---
			nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME, false);
			mutex_unlock(&ts->lock);
			NVT_ERR("mp parse device tree failed!\n");
			return -EINVAL;
		}
	} else {
		NVT_LOG("Not found novatek,mp-support-dt, use default setting\n");
		//---Print Test Criteria---
		nvt_print_criteria();
	}

	if (nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN)) {
		NVT_ERR("check fw reset state failed!\n");
		goto failed_out;
	}

	if (nvt_switch_FreqHopEnDis(FREQ_HOP_DISABLE)) {
		NVT_ERR("switch frequency hopping disable failed!\n");
		goto failed_out;
	}

	if (nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN)) {
		NVT_ERR("check fw reset state failed!\n");
		goto failed_out;
	}

	msleep(100);

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		NVT_ERR("clear fw status failed!\n");
		goto failed_out;
	}

	nvt_change_mode(MP_MODE_CC);

	if (nvt_check_fw_status()) {
		NVT_ERR("check fw status failed!\n");
		goto failed_out;
	}

	//---FW Rawdata Test---
	if (nvt_read_fw_rawdata(RawData_FW_Rawdata[0]) != 0) {
		TestResult_FW_Rawdata = 1;
		goto failed_out;
	} else {
		TestResult_FW_Rawdata = RawDataTest_SinglePoint_Sub(RawData_FW_Rawdata[0], RecordResult_FW_Rawdata, ts->x_num, ts->y_num,
												PS_Config_Lmt_FW_Rawdata_P, PS_Config_Lmt_FW_Rawdata_N);
	}
	nvt_get_rawdata_flatness_info(Rawdata_Flatness_Info[0]);
	Rawdata_FlatnessValueOper1[0] = Rawdata_Flatness_Info[0][5];

	if (nvt_read_CC(RawData_FW_CC) != 0) {
		TestResult_FW_CC = 1;
		goto failed_out;
	} else {
		TestResult_FW_CC = RawDataTest_SinglePoint_Sub(RawData_FW_CC, RecordResult_FW_CC, ts->x_num, ts->y_num,
											PS_Config_Lmt_FW_CC_P, PS_Config_Lmt_FW_CC_N);
	}
		
	if ((TestResult_FW_Rawdata == 1) || (TestResult_FW_CC == 1)) {
		TestResult_Cap_Rawdata = 1;
		goto failed_out;
	} else {
		if ((TestResult_FW_Rawdata == -1) || (TestResult_FW_CC == -1))
			TestResult_Cap_Rawdata = -1;
		else
			TestResult_Cap_Rawdata = 0;
	}

	if (ts->pen_support) {
		//---Pen FW Rawdata Test---
		if (nvt_read_pen_baseline() != 0) {
			TestResult_Pen_FW_Raw = 1;
			goto failed_out;
		} else {
			TestResult_PenTipX_Raw = RawDataTest_SinglePoint_Sub(RawData_PenTipX_Raw, RecordResult_PenTipX_Raw, ts->pen_x_num_x, ts->pen_x_num_y,
											PS_Config_Lmt_PenTipX_FW_Raw_P, PS_Config_Lmt_PenTipX_FW_Raw_N);
			TestResult_PenTipY_Raw = RawDataTest_SinglePoint_Sub(RawData_PenTipY_Raw, RecordResult_PenTipY_Raw, ts->pen_y_num_x, ts->pen_y_num_y,
											PS_Config_Lmt_PenTipY_FW_Raw_P, PS_Config_Lmt_PenTipY_FW_Raw_N);
			TestResult_PenRingX_Raw = RawDataTest_SinglePoint_Sub(RawData_PenRingX_Raw, RecordResult_PenRingX_Raw, ts->pen_x_num_x, ts->pen_x_num_y,
											PS_Config_Lmt_PenRingX_FW_Raw_P, PS_Config_Lmt_PenRingX_FW_Raw_N);
			TestResult_PenRingY_Raw = RawDataTest_SinglePoint_Sub(RawData_PenRingY_Raw, RecordResult_PenRingY_Raw, ts->pen_y_num_x, ts->pen_y_num_y,
											PS_Config_Lmt_PenRingY_FW_Raw_P, PS_Config_Lmt_PenRingY_FW_Raw_N);

			if ((TestResult_PenTipX_Raw == -1) || (TestResult_PenTipY_Raw == -1) || (TestResult_PenRingX_Raw == -1) || (TestResult_PenRingY_Raw == -1))
				TestResult_Pen_FW_Raw = -1;
			else
				TestResult_Pen_FW_Raw = 0;
		}
	} /* if (ts->pen_support) */

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	//---Noise Test---
	if (nvt_read_fw_noise(RawData_Diff) != 0) {
		TestResult_Noise = 1;	// 1: ERROR
		TestResult_FW_DiffMax = 1;
		TestResult_FW_DiffMin = 1;
		if (ts->pen_support) {
			TestResult_Pen_Noise = 1;
			TestResult_PenTipX_DiffMax = 1;
			TestResult_PenTipX_DiffMin = 1;
			TestResult_PenTipY_DiffMax = 1;
			TestResult_PenTipY_DiffMin = 1;
			TestResult_PenRingX_DiffMax = 1;
			TestResult_PenRingX_DiffMin = 1;
			TestResult_PenRingY_DiffMax = 1;
			TestResult_PenRingY_DiffMin = 1;
		} /* if (ts->pen_support) */
		goto failed_out;
	} else {
		TestResult_FW_DiffMax = RawDataTest_SinglePoint_Sub(RawData_Diff_Max, RecordResult_FW_DiffMax, ts->x_num, ts->y_num,
											PS_Config_Lmt_FW_Diff_P, PS_Config_Lmt_FW_Diff_N);

		TestResult_FW_DiffMin = RawDataTest_SinglePoint_Sub(RawData_Diff_Min, RecordResult_FW_DiffMin, ts->x_num, ts->y_num,
											PS_Config_Lmt_FW_Diff_P, PS_Config_Lmt_FW_Diff_N);

		if ((TestResult_FW_DiffMax == -1) || (TestResult_FW_DiffMin == -1))
			TestResult_Noise = -1;
		else
			TestResult_Noise = 0;

		if (ts->pen_support) {
			TestResult_PenTipX_DiffMax = RawDataTest_SinglePoint_Sub(RawData_PenTipX_DiffMax, RecordResult_PenTipX_DiffMax, ts->pen_x_num_x, ts->pen_x_num_y,
											PS_Config_Lmt_PenTipX_FW_Diff_P, PS_Config_Lmt_PenTipX_FW_Diff_N);

			TestResult_PenTipX_DiffMin = RawDataTest_SinglePoint_Sub(RawData_PenTipX_DiffMin, RecordResult_PenTipX_DiffMin, ts->pen_x_num_x, ts->pen_x_num_y,
											PS_Config_Lmt_PenTipX_FW_Diff_P, PS_Config_Lmt_PenTipX_FW_Diff_N);

			TestResult_PenTipY_DiffMax = RawDataTest_SinglePoint_Sub(RawData_PenTipY_DiffMax, RecordResult_PenTipY_DiffMax, ts->pen_y_num_x, ts->pen_y_num_y,
											PS_Config_Lmt_PenTipY_FW_Diff_P, PS_Config_Lmt_PenTipY_FW_Diff_N);

			TestResult_PenTipY_DiffMin = RawDataTest_SinglePoint_Sub(RawData_PenTipY_DiffMin, RecordResult_PenTipY_DiffMin, ts->pen_y_num_x, ts->pen_y_num_y,
											PS_Config_Lmt_PenTipY_FW_Diff_P, PS_Config_Lmt_PenTipY_FW_Diff_N);

			TestResult_PenRingX_DiffMax = RawDataTest_SinglePoint_Sub(RawData_PenRingX_DiffMax, RecordResult_PenRingX_DiffMax, ts->pen_x_num_x, ts->pen_x_num_y,
											PS_Config_Lmt_PenRingX_FW_Diff_P, PS_Config_Lmt_PenRingX_FW_Diff_N);

			TestResult_PenRingX_DiffMin = RawDataTest_SinglePoint_Sub(RawData_PenRingX_DiffMin, RecordResult_PenRingX_DiffMin, ts->pen_x_num_x, ts->pen_x_num_y,
											PS_Config_Lmt_PenRingX_FW_Diff_P, PS_Config_Lmt_PenRingX_FW_Diff_N);

			TestResult_PenRingY_DiffMax = RawDataTest_SinglePoint_Sub(RawData_PenRingY_DiffMax, RecordResult_PenRingY_DiffMax, ts->pen_y_num_x, ts->pen_y_num_y,
											PS_Config_Lmt_PenRingY_FW_Diff_P, PS_Config_Lmt_PenRingY_FW_Diff_N);

			TestResult_PenRingY_DiffMin = RawDataTest_SinglePoint_Sub(RawData_PenRingY_DiffMin, RecordResult_PenRingY_DiffMin, ts->pen_y_num_x, ts->pen_y_num_y,
											PS_Config_Lmt_PenRingY_FW_Diff_P, PS_Config_Lmt_PenRingY_FW_Diff_N);

			if ((TestResult_PenTipX_DiffMax == -1) || (TestResult_PenTipX_DiffMin == -1) || (TestResult_PenTipY_DiffMax == -1) || (TestResult_PenTipY_DiffMin == -1) ||
				(TestResult_PenRingX_DiffMax == -1) || (TestResult_PenRingX_DiffMin == -1) || (TestResult_PenRingY_DiffMax == -1) || (TestResult_PenRingY_DiffMin == -1))
				TestResult_Pen_Noise = -1;
			else
				TestResult_Pen_Noise = 0;
		} /* if (ts->pen_support) */
	}

	//---Digital Noise Test---
	if (nvt_read_fw_digital_noise(RawData_Diff) != 0) {
		TestResult_Digital_Noise = 1;	// 1: ERROR
		for (i = 0; i < 3; i++) {
			TestResult_FW_Digital_DiffMax[i] = 1;
			TestResult_FW_Digital_DiffMin[i] = 1;
		}
		goto failed_out;
	} else {
		for (i = 0; i < 3; i++) {
			TestResult_FW_Digital_DiffMax[i] = RawDataTest_SinglePoint_Sub(RawData_Digital_Diff_Max[i], RecordResult_FW_Digital_DiffMax[i], ts->x_num, ts->y_num,
												PS_Config_Lmt_FW_Digital_Diff_P, PS_Config_Lmt_FW_Digital_Diff_N);

			TestResult_FW_Digital_DiffMin[i] = RawDataTest_SinglePoint_Sub(RawData_Digital_Diff_Min[i], RecordResult_FW_Digital_DiffMin[i], ts->x_num, ts->y_num,
												PS_Config_Lmt_FW_Digital_Diff_P, PS_Config_Lmt_FW_Digital_Diff_N);
		}

		if ((TestResult_FW_Digital_DiffMax[0] == -1) || (TestResult_FW_Digital_DiffMin[0] == -1) ||
				(TestResult_FW_Digital_DiffMax[1] == -1) || (TestResult_FW_Digital_DiffMin[1] == -1) ||
				(TestResult_FW_Digital_DiffMax[2] == -1) || (TestResult_FW_Digital_DiffMin[2] == -1))
			TestResult_Digital_Noise = -1;
		else
			TestResult_Digital_Noise = 0;
	}
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/

	//---Rawdata Flatness Test---
	/* need be placed after nvt_read_fw_digital_noise() due to nvt_get_rawdata_flatness_info() is called to get rawdata flatness info in that function */
	if (rawdata_flatness_info_support) {
		TestResult_Rawdata_Flatness  = RawDataTest_SinglePoint_Sub(Rawdata_FlatnessValueOper1, RecordResult_Rawdata_Flatness, 4, 1,
											PS_Config_Lmt_FlatnessValueOper1_P, PS_Config_Lmt_FlatnessValueOper1_N);
	}

	//--Short Test---
	if (nvt_read_fw_short(RawData_Short_TXRX) != 0) {
		TestResult_Short = 1; // 1:ERROR
		goto failed_out;
	} else {
		//---Self Test Check --- // 0:PASS, -1:FAIL
		TestResult_Short_TXRX = RawDataTest_SinglePoint_Sub(RawData_Short_TXRX, RecordResult_Short_TXRX, ts->x_num, ts->y_num,
										PS_Config_Lmt_Short_TXRX_P, PS_Config_Lmt_Short_TXRX_N);

		TestResult_Short_TXTX = RawDataTest_SinglePoint_Sub(RawData_Short_TXTX, RecordResult_Short_TXTX, ts->x_num, 1,
										PS_Config_Lmt_Short_TXTX_P, PS_Config_Lmt_Short_TXTX_N);

		TestResult_Short_RXRX = RawDataTest_SinglePoint_Sub(RawData_Short_RXRX, RecordResult_Short_RXRX, 1, ts->y_num,
										PS_Config_Lmt_Short_RXRX_P, PS_Config_Lmt_Short_RXRX_N);

		if ((TestResult_Short_TXRX == -1) || (TestResult_Short_TXTX == -1) || (TestResult_Short_RXRX == -1))
			TestResult_Short = -1;
		else
			TestResult_Short = 0;
	}

	//---Open Test---
	if (nvt_read_fw_open(RawData_Open_Mutual) != 0) {
		TestResult_Open = 1;    // 1:ERROR
		goto failed_out;
	} else {
		//---Self Test Check --- // 0:PASS, -1:FAIL
		TestResult_Open_Mutual = RawDataTest_SinglePoint_Sub(RawData_Open_Mutual, RecordResult_Open_Mutual, ts->x_num, ts->y_num,
											PS_Config_Lmt_Open_Mutual_P, PS_Config_Lmt_Open_Mutual_N);

		TestResult_Open_SelfTX = RawDataTest_SinglePoint_Sub(RawData_Open_SelfTX, RecordResult_Open_SelfTX, ts->x_num, 1,
											PS_Config_Lmt_Open_SelfTX_P, PS_Config_Lmt_Open_SelfTX_N);

		TestResult_Open_SelfRX = RawDataTest_SinglePoint_Sub(RawData_Open_SelfRX, RecordResult_Open_SelfRX, 1, ts->y_num,
											PS_Config_Lmt_Open_SelfRX_P, PS_Config_Lmt_Open_SelfRX_N);

		if ((TestResult_Open_Mutual == -1) || (TestResult_Open_SelfTX == -1) || (TestResult_Open_SelfRX == -1))
			TestResult_Open = -1;
		else
			TestResult_Open = 0;
	}

	//---Download Normal FW---
	nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME, false);
err_nvt_spi_read:
	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	nvt_mp_test_result_printed = 0;
	return seq_open(file, &hq_selftest_seq_ops);

failed_out:
	nvt_read_fw_history_all();
/*P16 code for BUGP16-3030 by xiongdejun at 2025/5/29 start*/
	nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME, false);
/*P16 code for BUGP16-3030 by xiongdejun at 2025/5/29 end*/
	mutex_unlock(&ts->lock);

	nvt_mp_test_result_printed = 0;
	return seq_open(file, &nvt_selftest_fail_seq_ops);
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_selftest_fops = {
	.proc_open = nvt_selftest_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_selftest_fops = {
	.owner = THIS_MODULE,
	.open = nvt_selftest_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
#ifdef HAVE_PROC_OPS
static const struct proc_ops hq_selftest_fops = {
	.proc_open = hq_selftest_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations hq_selftest_fops = {
	.owner = THIS_MODULE,
	.open = hq_selftest_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/
#ifdef CONFIG_OF
/*******************************************************
Description:
	Novatek touchscreen parse AIN setting for array type.

return:
	n.a.
*******************************************************/
int32_t nvt_mp_parse_ain(struct device_node *np, const char *name, uint8_t *array, int32_t size)
{
	struct property *data;
	int32_t len, ret;
	int32_t tmp[64];
	int32_t i;

	data = of_find_property(np, name, &len);
	len /= sizeof(u32);
	if ((!data) || (!len) || (len != size)) {
		NVT_ERR("error find %s. len=%d\n", name, len);
		return -1;
	} else {
		NVT_LOG("%s. len=%d\n", name, len);
		ret = of_property_read_u32_array(np, name, tmp, len);
		if (ret) {
			NVT_ERR("error reading %s. ret=%d\n", name, ret);
			return -1;
		}

		for (i = 0; i < len; i++)
			array[i] = tmp[i];

#if NVT_DEBUG
		printk("[NVT-ts] %s = ", name);
		nvt_print_result_log_in_one_line(array, len);
		printk("\n");
#endif
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen parse criterion for u32 type.

return:
	n.a.
*******************************************************/
int32_t nvt_mp_parse_u32(struct device_node *np, const char *name, int32_t *para)
{
	int32_t ret;

	ret = of_property_read_u32(np, name, para);
	if (ret) {
		NVT_ERR("error reading %s. ret=%d\n", name, ret);
		return -1;
	} else {
#if NVT_DEBUG
		NVT_LOG("%s=%d\n", name, *para);
#endif
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen parse criterion for array type.

return:
	n.a.
*******************************************************/
int32_t nvt_mp_parse_array(struct device_node *np, const char *name, int32_t *array,
		int32_t x_ch, int32_t y_ch)
{
	struct property *data;
	int32_t len, ret, size;

	size = x_ch * y_ch;
	data = of_find_property(np, name, &len);
	len /= sizeof(u32);
	if ((!data) || (!len) || (len < size)) {
		NVT_ERR("error find %s. len=%d\n", name, len);
		return -1;
	} else {
		NVT_LOG("%s. len=%d\n", name, len);
		ret = of_property_read_u32_array(np, name, array, len);
		if (ret) {
			NVT_ERR("error reading %s. ret=%d\n", name, ret);
			return -1;
		}

#if NVT_DEBUG
		NVT_LOG("%s =\n", name);
		nvt_print_data_array(array, x_ch, y_ch);
#endif
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen parse criterion for pen array type.

return:
	n.a.
*******************************************************/
int32_t nvt_mp_parse_pen_array(struct device_node *np, const char *name, int32_t *array,
		uint32_t x_num, uint32_t y_num)
{
	struct property *data;
	int32_t len, ret;
#if NVT_DEBUG
	int32_t j = 0;
#endif
	uint32_t size;

	size = x_num * y_num;
	data = of_find_property(np, name, &len);
	len /= sizeof(u32);
	if ((!data) || (!len) || (len < size)) {
		NVT_ERR("error find %s. len=%d\n", name, len);
		return -1;
	} else {
		NVT_LOG("%s. len=%d\n", name, len);
		ret = of_property_read_u32_array(np, name, array, len);
		if (ret) {
			NVT_ERR("error reading %s. ret=%d\n", name, ret);
			return -1;
		}

#if NVT_DEBUG
		NVT_LOG("%s =\n", name);
		for (j = 0; j < y_num; j++) {
			nvt_print_data_log_in_one_line(array + j * x_num, x_num);
			printk("\n");
		}
#endif
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen parse device tree mp function.

return:
	n.a.
*******************************************************/
int32_t nvt_mp_parse_dt(struct device_node *root, const char *node_compatible)
{
	struct device_node *np = root;
	struct device_node *child = NULL;

	NVT_LOG("Parse mp criteria for node %s\n", node_compatible);

	/* find each MP sub-nodes */
	for_each_child_of_node(root, child) {
		/* find the specified node */
		if (of_device_is_compatible(child, node_compatible)) {
			NVT_LOG("found child node %s\n", node_compatible);
			np = child;
			break;
		}
	}
	if (child == NULL) {
		NVT_ERR("Not found compatible node %s!\n", node_compatible);
		return -1;
	}

	/* MP Criteria */
	if (nvt_mp_parse_array(np, "PS_Config_Lmt_Short_TXRX_P", PS_Config_Lmt_Short_TXRX_P,
			ts->x_num, ts->y_num))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_Short_TXRX_N", PS_Config_Lmt_Short_TXRX_N,
			ts->x_num, ts->y_num))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_Short_TXTX_P", PS_Config_Lmt_Short_TXTX_P,
			ts->x_num, 1))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_Short_TXTX_N", PS_Config_Lmt_Short_TXTX_N,
			ts->x_num, 1))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_Short_RXRX_P", PS_Config_Lmt_Short_RXRX_P,
			1, ts->y_num))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_Short_RXRX_N", PS_Config_Lmt_Short_RXRX_N,
			1, ts->y_num))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_Open_Mutual_P", PS_Config_Lmt_Open_Mutual_P,
			ts->x_num, ts->y_num))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_Open_Mutual_N", PS_Config_Lmt_Open_Mutual_N,
			ts->x_num, ts->y_num))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_Open_SelfTX_P", PS_Config_Lmt_Open_SelfTX_P,
			ts->x_num, 1))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_Open_SelfTX_N", PS_Config_Lmt_Open_SelfTX_N,
			ts->x_num, 1))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_Open_SelfRX_P", PS_Config_Lmt_Open_SelfRX_P,
			1, ts->y_num))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_Open_SelfRX_N", PS_Config_Lmt_Open_SelfRX_N,
			1, ts->y_num))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FW_Rawdata_P", PS_Config_Lmt_FW_Rawdata_P,
			ts->x_num, ts->y_num))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FW_Rawdata_N", PS_Config_Lmt_FW_Rawdata_N,
			ts->x_num, ts->y_num))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FW_CC_P", PS_Config_Lmt_FW_CC_P,
			ts->x_num, ts->y_num))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FW_CC_N", PS_Config_Lmt_FW_CC_N,
			ts->x_num, ts->y_num))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FW_Diff_P", PS_Config_Lmt_FW_Diff_P,
			ts->x_num, ts->y_num))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FW_Diff_N", PS_Config_Lmt_FW_Diff_N,
			ts->x_num, ts->y_num))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FW_Digital_Diff_P", PS_Config_Lmt_FW_Digital_Diff_P,
			ts->x_num, ts->y_num))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FW_Digital_Diff_N", PS_Config_Lmt_FW_Digital_Diff_N,
			ts->x_num, ts->y_num))
		return -1;
/*P16 code for HQFEAT-93999 by xiongdejun at 2025/6/27 start*/
	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FlatnessValueOper1_P", PS_Config_Lmt_FlatnessValueOper1_P,
			4, 1))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FlatnessValueOper1_N", PS_Config_Lmt_FlatnessValueOper1_N,
			4, 1))
		return -1;
/*P16 code for HQFEAT-93999 by xiongdejun at 2025/6/27 end*/
	if (ts->pen_support) {
		if (nvt_mp_parse_pen_array(np, "PS_Config_Lmt_PenTipX_FW_Raw_P", PS_Config_Lmt_PenTipX_FW_Raw_P,
				ts->pen_x_num_x, ts->pen_x_num_y))
			return -1;

		if (nvt_mp_parse_pen_array(np, "PS_Config_Lmt_PenTipX_FW_Raw_N", PS_Config_Lmt_PenTipX_FW_Raw_N,
				ts->pen_x_num_x, ts->pen_x_num_y))
			return -1;

		if (nvt_mp_parse_pen_array(np, "PS_Config_Lmt_PenTipY_FW_Raw_P", PS_Config_Lmt_PenTipY_FW_Raw_P,
				ts->pen_y_num_x, ts->pen_y_num_y))
			return -1;

		if (nvt_mp_parse_pen_array(np, "PS_Config_Lmt_PenTipY_FW_Raw_N", PS_Config_Lmt_PenTipY_FW_Raw_N,
				ts->pen_y_num_x, ts->pen_y_num_y))
			return -1;

		if (nvt_mp_parse_pen_array(np, "PS_Config_Lmt_PenRingX_FW_Raw_P", PS_Config_Lmt_PenRingX_FW_Raw_P,
				ts->pen_x_num_x, ts->pen_x_num_y))
			return -1;

		if (nvt_mp_parse_pen_array(np, "PS_Config_Lmt_PenRingX_FW_Raw_N", PS_Config_Lmt_PenRingX_FW_Raw_N,
				ts->pen_x_num_x, ts->pen_x_num_y))
			return -1;

		if (nvt_mp_parse_pen_array(np, "PS_Config_Lmt_PenRingY_FW_Raw_P", PS_Config_Lmt_PenRingY_FW_Raw_P,
				ts->pen_y_num_x, ts->pen_y_num_y))
			return -1;

		if (nvt_mp_parse_pen_array(np, "PS_Config_Lmt_PenRingY_FW_Raw_N", PS_Config_Lmt_PenRingY_FW_Raw_N,
				ts->pen_y_num_x, ts->pen_y_num_y))
			return -1;

		if (nvt_mp_parse_pen_array(np, "PS_Config_Lmt_PenTipX_FW_Diff_P", PS_Config_Lmt_PenTipX_FW_Diff_P,
				ts->pen_x_num_x, ts->pen_x_num_y))
			return -1;

		if (nvt_mp_parse_pen_array(np, "PS_Config_Lmt_PenTipX_FW_Diff_N", PS_Config_Lmt_PenTipX_FW_Diff_N,
				ts->pen_x_num_x, ts->pen_x_num_y))
			return -1;

		if (nvt_mp_parse_pen_array(np, "PS_Config_Lmt_PenTipY_FW_Diff_P", PS_Config_Lmt_PenTipY_FW_Diff_P,
				ts->pen_y_num_x, ts->pen_y_num_y))
			return -1;

		if (nvt_mp_parse_pen_array(np, "PS_Config_Lmt_PenTipY_FW_Diff_N", PS_Config_Lmt_PenTipY_FW_Diff_N,
				ts->pen_y_num_x, ts->pen_y_num_y))
			return -1;

		if (nvt_mp_parse_pen_array(np, "PS_Config_Lmt_PenRingX_FW_Diff_P", PS_Config_Lmt_PenRingX_FW_Diff_P,
				ts->pen_x_num_x, ts->pen_x_num_y))
			return -1;

		if (nvt_mp_parse_pen_array(np, "PS_Config_Lmt_PenRingX_FW_Diff_N", PS_Config_Lmt_PenRingX_FW_Diff_N,
				ts->pen_x_num_x, ts->pen_x_num_y))
			return -1;

		if (nvt_mp_parse_pen_array(np, "PS_Config_Lmt_PenRingY_FW_Diff_P", PS_Config_Lmt_PenRingY_FW_Diff_P,
				ts->pen_y_num_x, ts->pen_y_num_y))
			return -1;

		if (nvt_mp_parse_pen_array(np, "PS_Config_Lmt_PenRingY_FW_Diff_N", PS_Config_Lmt_PenRingY_FW_Diff_N,
				ts->pen_y_num_x, ts->pen_y_num_y))
			return -1;
	} /* if (ts->pen_support) */

	if (nvt_mp_parse_u32(np, "PS_Config_Diff_Test_Frame", &PS_Config_Diff_Test_Frame))
		return -1;

	if (nvt_mp_parse_u32(np, "PS_Config_Digital_Diff_Test_Frame", &PS_Config_Digital_Diff_Test_Frame))
		return -1;

	NVT_LOG("Parse mp criteria done!\n");

	return 0;
}
#endif /* #ifdef CONFIG_OF */

/*******************************************************
Description:
	Novatek touchscreen MP function proc. file node
	initial function.

return:
	Executive outcomes. 0---succeed. -1---failed.
*******************************************************/
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
int32_t nvt_mp_proc_init(void)
{
	NVT_proc_selftest_entry = proc_create("nvt_selftest", 0444, NULL, &nvt_selftest_fops);
	NVT_proc_android_touch_entry = proc_mkdir("android_touch", NULL);
	if (NVT_proc_android_touch_entry == NULL) {
		NVT_ERR("create /proc/android_touch Failed!\n");
		return -1;
	}
	hq_proc_selftest_entry = proc_create(NVT_SELFTEST, 0444, NVT_proc_android_touch_entry, &hq_selftest_fops);
	if (NVT_proc_selftest_entry == NULL || hq_proc_selftest_entry == NULL) {
		NVT_ERR("create /proc/nvt_selftest Failed!\n");
		return -1;
	} else {
		if(nvt_mp_buffer_init()) {
			NVT_ERR("Allocate mp memory failed\n");
			return -1;
		}
		else {
			NVT_LOG("create /proc/nvt_selftest Succeeded!\n");
		}
		return 0;
	}
}
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/
/*******************************************************
Description:
	Novatek touchscreen MP function proc. file node
	deinitial function.

return:
	n.a.
*******************************************************/
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
void nvt_mp_proc_deinit(void)
{
	nvt_mp_buffer_deinit();

	if (NVT_proc_selftest_entry != NULL) {
		remove_proc_entry("nvt_selftest", NULL);
		NVT_proc_selftest_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", "nvt_selftest");
	}
	if (hq_proc_selftest_entry != NULL) {
		remove_proc_entry(NVT_SELFTEST, NULL);
		hq_proc_selftest_entry = NULL;
		NVT_LOG("Removed /proc/android_touch/%s\n", "selftest");
	}
  	if(NVT_proc_android_touch_entry != NULL){
            remove_proc_entry("android_touch", NULL);
          	NVT_proc_android_touch_entry = NULL;
          	NVT_LOG("Removed /proc/%s\n", "android_touch");
        }
}
#endif /* #if NVT_TOUCH_MP */
/*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/
/*P16 code for HQFEAT-89698 by xiongdejun at 2025/4/23 start*/
int nvt_factory_open_test(void){
	struct device_node *np = ts->client->dev.of_node;
	unsigned char mpcriteria[32] = {0};	//novatek-mp-criteria-default
	TestResult_Open = 0;
	TestResult_Open_Mutual = 0;
	TestResult_Open_SelfTX = 0;
	TestResult_Open_SelfRX = 0;
	TestResult_FW_CC = 0;
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 start*/
	if(ts->nvt_tool_in_use){
		NVT_ERR("NVT tool in use.\n");
		return -EBUSY;
	}
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 end*/
	NVT_LOG("++\n");

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	//---Download MP FW---
	if (nvt_update_firmware(MP_UPDATE_FIRMWARE_NAME, false) < 0) {
		NVT_ERR("update mp firmware failed!\n");
		goto failed_out;
	}

	if (nvt_get_fw_info()) {
		NVT_ERR("get fw info failed!\n");
		goto failed_out;
	}

	fw_ver = ts->fw_ver;
	nvt_pid = ts->nvt_pid;

	/* Parsing criteria from dts */
	if(of_property_read_bool(np, "novatek,mp-support-dt")) {
		/*
		 * Parsing Criteria by Novatek PID
		 * The string rule is "novatek-mp-criteria-<nvt_pid>"
		 * nvt_pid is 2 bytes (show hex).
		 *
		 * Ex. nvt_pid = 500A
		 *     mpcriteria = "novatek-mp-criteria-500A"
		 */
		snprintf(mpcriteria, sizeof(mpcriteria), "novatek-mp-criteria-%04X", ts->nvt_pid);

		if (nvt_mp_parse_dt(np, mpcriteria)) {
			//---Download Normal FW---
			nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME, false);
			NVT_ERR("mp parse device tree failed!\n");
			goto failed_out;
		}
	} else {
		NVT_LOG("Not found novatek,mp-support-dt, use default setting\n");
		//---Print Test Criteria---
		nvt_print_criteria();
	}

	if (nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN)) {
		NVT_ERR("check fw reset state failed!\n");
		goto failed_out;
	}

	if (nvt_switch_FreqHopEnDis(FREQ_HOP_DISABLE)) {
		NVT_ERR("switch frequency hopping disable failed!\n");
		goto failed_out;
	}

	if (nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN)) {
		NVT_ERR("check fw reset state failed!\n");
		goto failed_out;
	}

	msleep(100);

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		NVT_ERR("clear fw status failed!\n");
		goto failed_out;
	}

	nvt_change_mode(MP_MODE_CC);

	if (nvt_check_fw_status()) {
		NVT_ERR("check fw status failed!\n");
		goto failed_out;
	}

	if (nvt_read_CC(RawData_FW_CC) != 0) {
		TestResult_FW_CC = 1;
		goto failed_out;
	} else {
		TestResult_FW_CC = RawDataTest_SinglePoint_Sub(RawData_FW_CC, RecordResult_FW_CC, ts->x_num, ts->y_num,
											PS_Config_Lmt_FW_CC_P, PS_Config_Lmt_FW_CC_N);
	}

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	//---Open Test---
	if (nvt_read_fw_open(RawData_Open_Mutual) != 0) {
		TestResult_Open = 1;    // 1:ERROR
/*P16 code for HQFEAT-89149 by xiongdejun at 2024/4/25 start*/
#if IS_ENABLED(CONFIG_MIEV)
		xiaomi_touch_mievent_report_int(TOUCH_EVENT_OPENTEST_FAIL, 0, "TpOpenTestFail", "novatek", -1);
#endif
/*P16 code for HQFEAT-89149 by xiongdejun at 2024/4/25 end*/
	} else {
		//---Self Test Check --- // 0:PASS, -1:FAIL
		TestResult_Open_Mutual = RawDataTest_SinglePoint_Sub(RawData_Open_Mutual, RecordResult_Open_Mutual, ts->x_num, ts->y_num,
											PS_Config_Lmt_Open_Mutual_P, PS_Config_Lmt_Open_Mutual_N);

		TestResult_Open_SelfTX = RawDataTest_SinglePoint_Sub(RawData_Open_SelfTX, RecordResult_Open_SelfTX, ts->x_num, 1,
											PS_Config_Lmt_Open_SelfTX_P, PS_Config_Lmt_Open_SelfTX_N);

		TestResult_Open_SelfRX = RawDataTest_SinglePoint_Sub(RawData_Open_SelfRX, RecordResult_Open_SelfRX, 1, ts->y_num,
											PS_Config_Lmt_Open_SelfRX_P, PS_Config_Lmt_Open_SelfRX_N);

		if ((TestResult_Open_Mutual == -1) || (TestResult_Open_SelfTX == -1) || (TestResult_Open_SelfRX == -1))
			TestResult_Open = -1;
		else
			TestResult_Open = 0;
	}

	//---Download Normal FW---
	nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME, false);
	mutex_unlock(&ts->lock);
	if (TestResult_Open == 0){
			NVT_LOG("************All test pass!!!************");
			return 0;
	} else {
			NVT_ERR("************Test FAIL!!!************");
			return -1;
	}
failed_out:
	nvt_read_fw_history_all();
/*P16 code for BUGP16-3030 by xiongdejun at 2025/5/29 start*/
	nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME, false);
/*P16 code for BUGP16-3030 by xiongdejun at 2025/5/29 end*/
	mutex_unlock(&ts->lock);
  	return -EAGAIN;
}

int nvt_factory_short_test(void){
	struct device_node *np = ts->client->dev.of_node;
	unsigned char mpcriteria[32] = {0};	//novatek-mp-criteria-default

	TestResult_Short = 0;
	TestResult_Short_TXRX = 0;
	TestResult_Short_TXTX = 0;
	TestResult_Short_RXRX = 0;
	TestResult_FW_CC = 0;
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 start*/
	if(ts->nvt_tool_in_use){
		NVT_ERR("NVT tool in use.\n");
		return -EBUSY;
	}
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 end*/
	NVT_LOG("++\n");

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	//---Download MP FW---
	if (nvt_update_firmware(MP_UPDATE_FIRMWARE_NAME, false) < 0) {
		NVT_ERR("update mp firmware failed!\n");
		goto failed_out;
	}

	if (nvt_get_fw_info()) {
		NVT_ERR("get fw info failed!\n");
		goto failed_out;
	}

	fw_ver = ts->fw_ver;
	nvt_pid = ts->nvt_pid;

	/* Parsing criteria from dts */
	if(of_property_read_bool(np, "novatek,mp-support-dt")) {
		/*
		 * Parsing Criteria by Novatek PID
		 * The string rule is "novatek-mp-criteria-<nvt_pid>"
		 * nvt_pid is 2 bytes (show hex).
		 *
		 * Ex. nvt_pid = 500A
		 *     mpcriteria = "novatek-mp-criteria-500A"
		 */
		snprintf(mpcriteria, sizeof(mpcriteria), "novatek-mp-criteria-%04X", ts->nvt_pid);

		if (nvt_mp_parse_dt(np, mpcriteria)) {
			//---Download Normal FW---
			nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME, false);
			mutex_unlock(&ts->lock);
			NVT_ERR("mp parse device tree failed!\n");
			goto failed_out;
		}
	} else {
		NVT_LOG("Not found novatek,mp-support-dt, use default setting\n");
		//---Print Test Criteria---
		nvt_print_criteria();
	}

	if (nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN)) {
		NVT_ERR("check fw reset state failed!\n");
		goto failed_out;
	}

	if (nvt_switch_FreqHopEnDis(FREQ_HOP_DISABLE)) {
		NVT_ERR("switch frequency hopping disable failed!\n");
		goto failed_out;
	}

	if (nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN)) {
		NVT_ERR("check fw reset state failed!\n");
		goto failed_out;
	}

	msleep(100);

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		NVT_ERR("clear fw status failed!\n");
		goto failed_out;
	}

	nvt_change_mode(MP_MODE_CC);

	if (nvt_check_fw_status()) {
		NVT_ERR("check fw status failed!\n");
		goto failed_out;
	}

	if (nvt_read_CC(RawData_FW_CC) != 0) {
		TestResult_FW_CC = 1;
		goto failed_out;
	} else {
		TestResult_FW_CC = RawDataTest_SinglePoint_Sub(RawData_FW_CC, RecordResult_FW_CC, ts->x_num, ts->y_num,
											PS_Config_Lmt_FW_CC_P, PS_Config_Lmt_FW_CC_N);
	}

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	//--Short Test---
	if (nvt_read_fw_short(RawData_Short_TXRX) != 0) {
		TestResult_Short = 1; // 1:ERROR
/*P16 code for HQFEAT-89149 by xiongdejun at 2024/4/25 start*/
#if IS_ENABLED(CONFIG_MIEV)
		xiaomi_touch_mievent_report_int(TOUCH_EVENT_SHORTTEST_FAIL, 0, "TpShortTestFail", "novatek", -1);
#endif
/*P16 code for HQFEAT-89149 by xiongdejun at 2024/4/25 end*/
	} else {
		//---Self Test Check --- // 0:PASS, -1:FAIL
		TestResult_Short_TXRX = RawDataTest_SinglePoint_Sub(RawData_Short_TXRX, RecordResult_Short_TXRX, ts->x_num, ts->y_num,
										PS_Config_Lmt_Short_TXRX_P, PS_Config_Lmt_Short_TXRX_N);

		TestResult_Short_TXTX = RawDataTest_SinglePoint_Sub(RawData_Short_TXTX, RecordResult_Short_TXTX, ts->x_num, 1,
										PS_Config_Lmt_Short_TXTX_P, PS_Config_Lmt_Short_TXTX_N);

		TestResult_Short_RXRX = RawDataTest_SinglePoint_Sub(RawData_Short_RXRX, RecordResult_Short_RXRX, 1, ts->y_num,
										PS_Config_Lmt_Short_RXRX_P, PS_Config_Lmt_Short_RXRX_N);

		if ((TestResult_Short_TXRX == -1) || (TestResult_Short_TXTX == -1) || (TestResult_Short_RXRX == -1))
			TestResult_Short = -1;
		else
			TestResult_Short = 0;
	}

	//---Download Normal FW---
	nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME, false);
	mutex_unlock(&ts->lock);
	if (TestResult_Short == 0){
			NVT_LOG("************All test pass!!!************");
			return 0;
	} else {
			NVT_ERR("************Test FAIL!!!************");
			return -1;
	}
failed_out:
	nvt_read_fw_history_all();
/*P16 code for BUGP16-3030 by xiongdejun at 2025/5/29 start*/
	nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME, false);
/*P16 code for BUGP16-3030 by xiongdejun at 2025/5/29 end*/
	mutex_unlock(&ts->lock);
  	return -EAGAIN;
}
/*P16 code for HQFEAT-89698 by xiongdejun at 2025/4/23 end*/