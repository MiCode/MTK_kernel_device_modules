// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include "mtk_disp_oddmr.h"
#include "mtk_disp_oddmr_tuning.h"

#ifdef APP_DEBUG
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#define DDPINFO printf
#define PC_ERR printf
#define kfree free
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
#else
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/ratelimit.h>
#include "../mtk_drm_crtc.h"
#include "../mtk_drm_ddp_comp.h"
#include "../mtk_dump.h"
#include "../mtk_drm_mmp.h"
#include "../mtk_drm_gem.h"
#include "../mtk_drm_fb.h"
#undef DDPINFO
#define DDPINFO DDPMSG
#endif
/* return loaded size */
static uint32_t mtk_oddmr_parse_table_pq(uint32_t table_idx,
		uint8_t *data, uint32_t len, struct mtk_oddmr_pq_param *pq_param)
{
	void *buffer_alloc;
	uint32_t counts;

	if (len > 0) {
		counts = *(uint32_t *)data;
		data += 4;
		if (len != (4 + counts * 8)) {
			DDPINFO("%s:%d, size is invalid size:%d,cnts:%d\n",
					__func__, __LINE__, len, counts);
			return 0;
		}
		if (pq_param == NULL) {
			DDPINFO("%s:%d, pq_param is NULL\n",
					__func__, __LINE__);
			return 0;
		}
		if (pq_param->param != NULL) {
			kfree(pq_param->param);
			pq_param->param = NULL;
			pq_param->counts = 0;
		}
#ifndef APP_DEBUG
		buffer_alloc = kzalloc(len - 4, GFP_KERNEL);
#else
		buffer_alloc = malloc(len - 4);
#endif
		if (!buffer_alloc) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
					__func__, __LINE__);
			return 0;
		}
		pq_param->param = buffer_alloc;
		pq_param->counts = counts;
		memcpy(pq_param->param, data, len - 4);
	} else
		DDPINFO("%s:%d, len = 0 skip\n", __func__, __LINE__);
	return len;
}
static uint32_t mtk_oddmr_parse_raw_table(uint32_t table_idx,
		uint8_t *data, uint32_t len, struct mtk_oddmr_table_raw *raw_table)
{
	void *buffer_alloc;

	if (raw_table == NULL) {
		DDPINFO("%s:%d, raw_table is NULL\n",
				__func__, __LINE__);
		return 0;
	}
	if (raw_table->value != NULL) {
		kfree(raw_table->value);
		raw_table->value = NULL;
		raw_table->size = 0;
	}
#ifndef APP_DEBUG
	buffer_alloc = kzalloc(len, GFP_KERNEL);
#else
	buffer_alloc = malloc(len);
#endif
	if (!buffer_alloc) {
		DDPINFO("%s:%d, param buffer alloc fail\n",
				__func__, __LINE__);
		return 0;
	}
	raw_table->value = buffer_alloc;
	memcpy(raw_table->value, data, len);
	raw_table->size = len;
	return len;
}

static uint32_t mtk_oddmr_parse_od_table_basic_info(struct mtk_oddmr_od_param *od_param,
	uint32_t table_idx, uint8_t *data)
{
	uint32_t size;

	if (od_param->od_basic_info.basic_param.bin_version >= 2) {
		size = sizeof(struct mtk_oddmr_od_table_basic_info);
		memcpy(&od_param->od_tables[table_idx]->table_basic_info, data, size);
	} else {
		size = sizeof(struct mtk_oddmr_od_table_basic_info) - 4 * 2; // no remap_gian, table_offset
		memcpy(&od_param->od_tables[table_idx]->table_basic_info, data, 4 * 8);
		od_param->od_tables[table_idx]->table_basic_info.reserved = *(uint32_t *)(data +  4 * 8);
		od_param->od_tables[table_idx]->table_basic_info.remap_gian = 255; //default 255
		od_param->od_tables[table_idx]->table_basic_info.table_offset = 0; //default 0
	}
	return size;
}

/*
 * common way to describe od weight, not used
 * return loaded size
 */
static uint32_t mtk_oddmr_parse_od_table_gain(struct mtk_oddmr_od_param *od_param,
	uint32_t table_idx, uint8_t *data, uint32_t len)
{
	void *buffer_alloc;

	if (od_param->od_tables[table_idx] != NULL &&
		od_param->od_tables[table_idx]->gain_table_raw == NULL) {
#ifndef APP_DEBUG
		buffer_alloc = kzalloc(len, GFP_KERNEL);
#else
		buffer_alloc = malloc(len);
#endif
		if (!buffer_alloc) {
			DDPINFO("%s:%d, param buffer alloc fail\n", __func__, __LINE__);
			return 0;
		}
		od_param->od_tables[table_idx]->gain_table_raw = buffer_alloc;
		memcpy(od_param->od_tables[table_idx]->gain_table_raw, data, len);
	} else {
		len = 0;
	}
	return len;
}

/*
 * another way to describe od fps/dbv weight
 * return loaded size
 */
static uint32_t mtk_oddmr_parse_od_table_bl_gain(struct mtk_oddmr_od_param *od_param,
	uint32_t table_idx, uint8_t *data, uint32_t len)
{
	uint32_t counts;

	counts = *(uint32_t *)data;
	od_param->od_tables[table_idx]->bl_cnt = counts;

	data += 4;
	if (len > 4) {
		if (od_param->od_basic_info.basic_param.bin_version >= 2)
			memcpy(&od_param->od_tables[table_idx]->bl_table, data, len - 4);
		else
			for (int i = 0; i < counts; i++) {
				od_param->od_tables[table_idx]->bl_table[i].item = *(uint32_t *)data;
				data += 4;
				od_param->od_tables[table_idx]->bl_table[i].value_r = *(uint32_t *)data;
				od_param->od_tables[table_idx]->bl_table[i].value = *(uint32_t *)data;
				od_param->od_tables[table_idx]->bl_table[i].value_b = *(uint32_t *)data;
				data += 4;
			}
	}
	return len;
}

/*
 * another way to describe od fps/dbv weight
 * return loaded size
 */
static uint32_t mtk_oddmr_parse_od_table_fps_gain(struct mtk_oddmr_od_param *od_param,
	uint32_t table_idx, uint8_t *data, uint32_t len)
{
	uint32_t counts;

	counts = *(uint32_t *)data;
	od_param->od_tables[table_idx]->fps_cnt = counts;

	data += 4;
	if (len > 4) {
		if (od_param->od_basic_info.basic_param.bin_version >= 2)
			memcpy(&od_param->od_tables[table_idx]->fps_table, data, len - 4);
		else
			for (int i = 0; i < counts; i++) {
				od_param->od_tables[table_idx]->fps_table[i].item = *(uint32_t *)data;
				data += 4;
				od_param->od_tables[table_idx]->fps_table[i].value_r = *(uint32_t *)data;
				od_param->od_tables[table_idx]->fps_table[i].value = *(uint32_t *)data;
				od_param->od_tables[table_idx]->fps_table[i].value_b = *(uint32_t *)data;
				data += 4;
			}
	}
	return len;
}

static int _mtk_oddmr_load_param(struct mtk_oddmr_od_param *od_param, struct mtk_drm_oddmr_param *param)
{
	int ret = -EFAULT;
	uint32_t table_idx = 0;
	uint8_t *data, *p;
	uint32_t counting_size, sub_head_id, data_type_id, tmp_head_id, target_size;

	if (param == NULL || param->data == NULL) {
		DDPINFO("%s:%d, param is NULL\n", __func__, __LINE__);
		return -EFAULT;
	}
#ifndef APP_DEBUG
	data = kzalloc(param->size, GFP_KERNEL);
#else
	data = malloc(param->size);
	memset(data, 0, param->size);
#endif
	if (!data) {
		DDPINFO("%s:%d, param buffer alloc fail\n", __func__, __LINE__);
		return -EFAULT;
	}
#ifndef APP_DEBUG
	if (copy_from_user(data, param->data, param->size)) {
		DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
		ret = -EFAULT;
		goto fail;
	}
#else
	memcpy(data, param->data, param->size);
#endif
	table_idx = param->head_id >> 16 & 0xFF;
	data_type_id = param->head_id >> 24;
	DDPINFO("%s:%d, table_idx 0x%x data_type_id 0x%x size %d\n", __func__, __LINE__,
			table_idx, data_type_id, param->size);
	if ((param->head_id & 0xFFFF) == ODDMR_SECTION_WHOLE) {
		p = data;
		counting_size = 0;
		sub_head_id = *(uint32_t *)p;

		/* sub section head + size + data_body */
		DDPINFO("%s:%d, ++++++++ table%d 0x%x parsing begin,%d/%d\n", __func__, __LINE__,
				table_idx, param->head_id, counting_size, param->size);
		while (counting_size < param->size && sub_head_id != ODDMR_SECTION_END) {
			uint32_t tmp_size = 0;

			if (counting_size + 8 > param->size) {// OOB check for sub head+size
				PC_ERR("%s:%d, 0x%x size error, counting %d + 8 > param %d\n",
					__func__, __LINE__, sub_head_id, counting_size, param->size);
				ret = -EFAULT;
				goto fail;
			}
			/* p is now pointing to sub head */
			tmp_head_id = *(uint32_t *)p;
			sub_head_id = tmp_head_id & 0xFFFF;
			p += 4;
			counting_size += 4;
			/* p is now pointing to sub size */
			tmp_size = *(uint32_t *)p;
			p += 4;
			counting_size += 4;
			DDPINFO("%s:%d, parsing 0x%x size %d\n", __func__, __LINE__,
					tmp_head_id, tmp_size);
			if (counting_size + tmp_size > param->size) {// OOB check for sub data_body
				PC_ERR("%s:%d, 0x%x size error, counting %d + tmp %d > param %d\n",
					__func__, __LINE__, sub_head_id, counting_size, tmp_size, param->size);
				ret = -EFAULT;
				goto fail;
			}
			/* p is now pointing to sub data_body */
			if (tmp_size == 0) {
				DDPINFO("%s:%d, tmp_head_id 0x%x skip due to size = 0\n",
						__func__, __LINE__, tmp_head_id);
				goto skip_loop;
			}
			if (sub_head_id == OD_BASIC_PARAM &&
					data_type_id == ODDMR_OD_BASIC_INFO) {
				/* p is now pointing to sub data_body */
				if (tmp_size == sizeof(struct mtk_oddmr_od_basic_param) - 8) {
					DDPINFO("%s:%d, bin version: mt6991 and before\n", __func__, __LINE__);
					memcpy((uint8_t *)&od_param->od_basic_info.basic_param + 4,
							p, 4 * 13);  //skip bin_version
					memcpy((uint8_t *)&od_param->od_basic_info.basic_param + 4 * 15,
							p + 4 * 13, tmp_size - 4 * 13);  //skip nonlinear_node_cnt
					od_param->od_basic_info.basic_param.nonlinear_node_cnt = 33; //default 33
				} else if (tmp_size == sizeof(struct mtk_oddmr_od_basic_param)) {
					DDPINFO("%s:%d, bin version: %d\n", __func__, __LINE__, *(uint32_t *)p);
					memcpy(&od_param->od_basic_info.basic_param, p, tmp_size);
				} else {
					DDPINFO("%s:%d, 0x%x size error\n",
							__func__, __LINE__, sub_head_id);
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == OD_BASIC_PQ &&
					data_type_id == ODDMR_OD_BASIC_INFO) {
				/* p is now pointing to sub data_body */
				tmp_size = mtk_oddmr_parse_table_pq(table_idx, p, tmp_size,
						&od_param->od_basic_info.basic_pq);
				if (tmp_size == 0) {
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == OD_TABLE_BASIC_INFO &&
					data_type_id == ODDMR_OD_TABLE) {
				/* p is now pointing to sub data_body */
				if (od_param->od_basic_info.basic_param.bin_version >= 2)
					target_size = sizeof(struct mtk_oddmr_od_table_basic_info);
				else // no remap_gian, table_offset
					target_size = sizeof(struct mtk_oddmr_od_table_basic_info) - 4 * 2;
				if (tmp_size != target_size) {
					DDPINFO("%s:%d, 0x%x size error\n",
							__func__, __LINE__, sub_head_id);
					ret = -EFAULT;
					goto fail;
				}
				tmp_size = mtk_oddmr_parse_od_table_basic_info(od_param, table_idx, p);
			} else if (sub_head_id == OD_TABLE_GAIN_TABLE &&
					data_type_id == ODDMR_OD_TABLE) {
				/* p is now pointing to sub data_body */
				uint32_t counts_fps = *(uint32_t *)p;
				uint32_t counts_bl = *(uint32_t *)(p + 4);

				if (tmp_size != 8 + counts_fps * 4 + counts_bl * 4 +
						counts_fps * counts_bl) {
					DDPINFO("%s:%d, table%d 0x%x size error,",
						__func__, __LINE__, table_idx, sub_head_id);
					DDPINFO("size %d,fps_cnt %d dbv_cnt %d\n",
						tmp_size, counts_fps, counts_bl);
					ret = -EFAULT;
					goto fail;
				}
				if (mtk_oddmr_parse_od_table_gain(od_param, table_idx, p, tmp_size) !=
					tmp_size) {
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == OD_TABLE_PQ_OD &&
					data_type_id == ODDMR_OD_TABLE) {
				/* p is now pointing to sub data_body */
				tmp_size = mtk_oddmr_parse_table_pq(table_idx, p, tmp_size,
						&od_param->od_tables[table_idx]->pq_od);
				if (tmp_size == 0) {
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == OD_TABLE_FPS_GAIN_TABLE &&
					data_type_id == ODDMR_OD_TABLE) {
				/* p is now pointing to sub data_body */
				uint32_t counts = *(uint32_t *)p;

				if (od_param->od_basic_info.basic_param.bin_version >= 2)
					target_size = counts * 4 * 4 + 4; // r,g,b separated
				else
					target_size = counts * 4 * 2 + 4;
				if (tmp_size < 4
					|| counts > OD_GAIN_MAX
					|| (tmp_size != target_size)) {
					DDPINFO("%s:%d, table%d 0x%x size error,size %d,count %d\n",
							__func__, __LINE__,
							table_idx, tmp_head_id, tmp_size, counts);
					ret = -EFAULT;
					goto fail;
				}
				if (mtk_oddmr_parse_od_table_fps_gain(od_param, table_idx, p, tmp_size) !=
					tmp_size) {
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == OD_TABLE_DBV_GAIN_TABLE &&
					data_type_id == ODDMR_OD_TABLE) {
				/* p is now pointing to sub data_body */
				uint32_t counts = *(uint32_t *)p;

				if (od_param->od_basic_info.basic_param.bin_version >= 2)
					target_size = counts * 4 * 4 + 4; // r,g,b separated
				else
					target_size = counts * 4 * 2 + 4;
				if (tmp_size < 4 ||
					counts > OD_GAIN_MAX
					|| (tmp_size != target_size)) {
					DDPINFO("%s:%d, table%d 0x%x size error,size %d,count %d\n",
							__func__, __LINE__,
							table_idx, tmp_head_id, tmp_size, counts);
					ret = -EFAULT;
					goto fail;
				}
				if (mtk_oddmr_parse_od_table_bl_gain(od_param, table_idx, p, tmp_size) !=
					tmp_size) {
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == OD_TABLE_DATA && data_type_id == ODDMR_OD_TABLE) {
				/* p is now pointing to sub data_body */
				tmp_size = mtk_oddmr_parse_raw_table(table_idx, p, tmp_size,
						&od_param->od_tables[table_idx]->raw_table);
				if (tmp_size == 0) {
					DDPINFO("%s:%d, table%d 0x%x size error,size %d\n",
							__func__, __LINE__,
							table_idx, tmp_head_id, tmp_size);
					ret = -EFAULT;
					goto fail;
				}
			} else {
				ret = -EFAULT;
				DDPINFO("%s:%d, 0x%x, not support counting %d/%d\n",
						__func__, __LINE__,
						tmp_head_id, counting_size, param->size);
				break;
			}
			p += tmp_size;
			counting_size += tmp_size;
			/* p is now pointing to next sub head */
skip_loop:
			DDPINFO("%s:%d, table%d 0x%x size %d, counting %d/%d\n",
					__func__, __LINE__, table_idx, tmp_head_id,
					tmp_size, counting_size, param->size);
			sub_head_id = *(uint32_t *)p & 0xFFFF;
			if (sub_head_id == ODDMR_SECTION_END || counting_size == param->size) {
				ret = 0;
				DDPINFO("%s:%d, -------- table%d 0x%x parsing end,(%d + 4)/%d\n",
						__func__, __LINE__, table_idx, param->head_id,
						counting_size, param->size);
				break;
			}
		}
	} else {
		DDPINFO("%s:%d, table%d 0x%x not support\n",
				__func__, __LINE__, table_idx, param->head_id);
	}
fail:
	kfree(data);
	data = NULL;
	return ret;
}
int mtk_oddmr_load_param(struct mtk_disp_oddmr *oddmr_data, struct mtk_drm_oddmr_param *param)
{
	int ret = -1, i = 0;
	uint32_t table_idx, size_alloc;
	void *data;
	struct mtk_oddmr_od_param *od_param = &oddmr_data->primary_data->od_param;
	char valid_table_str[OD_TABLE_MAX + 1];

	if (param == NULL) {
		DDPINFO("%s:%d, param is NULL\n",
				__func__, __LINE__);
		return -EFAULT;
	}
	switch (param->head_id >> 24) {
	case ODDMR_OD_BASIC_INFO:
		if (oddmr_data->primary_data->od_basic_info_loaded) {
			DDPINFO("%s:%d, basic info is already loaded\n", __func__, __LINE__);
			return -EFAULT;
		}
		ret = _mtk_oddmr_load_param(od_param, param);
		if (ret >= 0) {
			oddmr_data->primary_data->od_basic_info_loaded = 1;
			DDPINFO("%s:%d, od basic info load success\n", __func__, __LINE__);
		}
		break;
	case ODDMR_OD_TABLE:
		table_idx = param->head_id >> 16 & 0xFF;
		if (table_idx >= OD_TABLE_MAX ||
				table_idx >= od_param->od_basic_info.basic_param.table_cnt) {
			DDPINFO("%s:%d, table_idx %d is invalid\n",
					__func__, __LINE__, table_idx);
			return -EFAULT;
		}
		oddmr_data->primary_data->od_state = ODDMR_INVALID;
		size_alloc = sizeof(struct mtk_oddmr_od_table);
		if (od_param->od_tables[table_idx] == NULL) {
			DDPINFO("%s:%d, od_table%d is NULL\n",
					__func__, __LINE__, table_idx);
#ifndef APP_DEBUG
			data = kzalloc(size_alloc, GFP_KERNEL);
#else
			data = malloc(size_alloc);
			memset(data, 0, size_alloc);
#endif
			if (!data) {
				DDPINFO("%s:%d, param buffer alloc fail\n",
						__func__, __LINE__);
				return -EFAULT;
			}
			od_param->od_tables[table_idx] = data;
		} else {
			memset(od_param->od_tables[table_idx], 0, size_alloc);
		}
		ret = _mtk_oddmr_load_param(od_param, param);
		if (ret == 0) {
			if (!od_param->valid_table[table_idx])
				od_param->valid_table_cnt += 1;
			od_param->valid_table[table_idx] = true;
			oddmr_data->primary_data->od_state = ODDMR_LOAD_PARTS;
		}
		for (i = 0; i < OD_TABLE_MAX; i++)
			valid_table_str[i] = od_param->valid_table[i] ? '1' : '0';
		valid_table_str[OD_TABLE_MAX] = '\0';
		DDPINFO("%s:%d, od valid_table_cnt %d: %s\n",
				__func__, __LINE__,
				od_param->valid_table_cnt, valid_table_str);
		break;
	default:
		DDPINFO("%s:%d, param is invalid 0x%x\n",
				__func__, __LINE__, param->head_id);
		ret = -EINVAL;
		break;
	}
	return ret;
}

int mtk_oddmr_od_tuning_read(struct mtk_ddp_comp *comp, uint32_t table_idx,
		struct mtk_oddmr_sw_reg *sw_reg, struct mtk_oddmr_od_param *pparam)
{
	uint32_t map_id, *val;
	int ret = 0;

	map_id = (sw_reg->reg & 0xFFFF) / 4;
	val = &sw_reg->val;
	switch (map_id) {
	case OD_BASIC_BIN_VERSION:
		*val = pparam->od_basic_info.basic_param.bin_version;
		break;
	case OD_BASIC_RESOLUTION_SWITCH_MODE:
		*val = pparam->od_basic_info.basic_param.resolution_switch_mode;
		break;
	case OD_BASIC_PANEL_WIDTH:
		*val = pparam->od_basic_info.basic_param.panel_width;
		break;
	case OD_BASIC_PANEL_HEIGHT:
		*val = pparam->od_basic_info.basic_param.panel_height;
		break;
	case OD_BASIC_TABLE_CNT:
		*val = pparam->od_basic_info.basic_param.table_cnt;
		break;
	case OD_BASIC_OD_MODE:
		*val = pparam->od_basic_info.basic_param.od_mode;
		break;
	case OD_BASIC_DITHER_SEL:
		*val = pparam->od_basic_info.basic_param.dither_sel;
		break;
	case OD_BASIC_DITHER_CTL:
		*val = pparam->od_basic_info.basic_param.dither_ctl;
		break;
	case OD_BASIC_SCALING_MODE:
		*val = pparam->od_basic_info.basic_param.scaling_mode;
		break;
	case OD_BASIC_OD_HSK2:
		*val = pparam->od_basic_info.basic_param.od_hsk_2;
		break;
	case OD_BASIC_OD_HSK3:
		*val = pparam->od_basic_info.basic_param.od_hsk_3;
		break;
	case OD_BASIC_OD_HSK4:
		*val = pparam->od_basic_info.basic_param.od_hsk_4;
		break;
	case OD_BASIC_RESERVED:
		break;
	case OD_BASIC_NONLINEAR_NODE_CNT:
		*val = pparam->od_basic_info.basic_param.nonlinear_node_cnt;
		break;
	case OD_TABLE_WIDTH:
		*val = pparam->od_tables[table_idx]->table_basic_info.width;
		break;
	case OD_TABLE_HEIGHT:
		*val = pparam->od_tables[table_idx]->table_basic_info.height;
		break;
	case OD_TABLE_FPS:
		*val = pparam->od_tables[table_idx]->table_basic_info.fps;
		break;
	case OD_TABLE_BL:
		*val = pparam->od_tables[table_idx]->table_basic_info.dbv;
		break;
	case OD_TABLE_MIN_FPS:
		*val = pparam->od_tables[table_idx]->table_basic_info.min_fps;
		break;
	case OD_TABLE_MAX_FPS:
		*val = pparam->od_tables[table_idx]->table_basic_info.max_fps;
		break;
	case OD_TABLE_MIN_BL:
		*val = pparam->od_tables[table_idx]->table_basic_info.min_dbv;
		break;
	case OD_TABLE_MAX_BL:
		*val = pparam->od_tables[table_idx]->table_basic_info.max_dbv;
		break;
	case OD_TABLE_RESERVED:
		break;
	case OD_TABLE_REMAP_GIAN:
		*val = pparam->od_tables[table_idx]->table_basic_info.remap_gian;
		break;
	case OD_TABLE_TABLE_OFFSET:
		*val = pparam->od_tables[table_idx]->table_basic_info.table_offset;
		break;
	case OD_TABLE_FPS_CNT:
		*val = pparam->od_tables[table_idx]->fps_cnt;
		break;
	case OD_TABLE_BL_CNT:
		*val = pparam->od_tables[table_idx]->bl_cnt;
		break;
	case OD_TABLE_FPS0:
	case OD_TABLE_FPS1:
	case OD_TABLE_FPS2:
	case OD_TABLE_FPS3:
	case OD_TABLE_FPS4:
	case OD_TABLE_FPS5:
	case OD_TABLE_FPS6:
	case OD_TABLE_FPS7:
	case OD_TABLE_FPS8:
	case OD_TABLE_FPS9:
	case OD_TABLE_FPS10:
	case OD_TABLE_FPS11:
	case OD_TABLE_FPS12:
	case OD_TABLE_FPS13:
	case OD_TABLE_FPS14:
	{
		int idx;

		idx = map_id - OD_TABLE_FPS0;
		*val = pparam->od_tables[table_idx]->fps_table[idx].item;
		break;
	}
	case OD_TABLE_FPS_WEIGHT0:
	case OD_TABLE_FPS_WEIGHT1:
	case OD_TABLE_FPS_WEIGHT2:
	case OD_TABLE_FPS_WEIGHT3:
	case OD_TABLE_FPS_WEIGHT4:
	case OD_TABLE_FPS_WEIGHT5:
	case OD_TABLE_FPS_WEIGHT6:
	case OD_TABLE_FPS_WEIGHT7:
	case OD_TABLE_FPS_WEIGHT8:
	case OD_TABLE_FPS_WEIGHT9:
	case OD_TABLE_FPS_WEIGHT10:
	case OD_TABLE_FPS_WEIGHT11:
	case OD_TABLE_FPS_WEIGHT12:
	case OD_TABLE_FPS_WEIGHT13:
	case OD_TABLE_FPS_WEIGHT14:
	{
		int idx;

		idx = map_id - OD_TABLE_FPS_WEIGHT0;
		*val = pparam->od_tables[table_idx]->fps_table[idx].value;
		break;
	}
	case OD_TABLE_FPS_WEIGHT0_R:
	case OD_TABLE_FPS_WEIGHT1_R:
	case OD_TABLE_FPS_WEIGHT2_R:
	case OD_TABLE_FPS_WEIGHT3_R:
	case OD_TABLE_FPS_WEIGHT4_R:
	case OD_TABLE_FPS_WEIGHT5_R:
	case OD_TABLE_FPS_WEIGHT6_R:
	case OD_TABLE_FPS_WEIGHT7_R:
	case OD_TABLE_FPS_WEIGHT8_R:
	case OD_TABLE_FPS_WEIGHT9_R:
	case OD_TABLE_FPS_WEIGHT10_R:
	case OD_TABLE_FPS_WEIGHT11_R:
	case OD_TABLE_FPS_WEIGHT12_R:
	case OD_TABLE_FPS_WEIGHT13_R:
	case OD_TABLE_FPS_WEIGHT14_R:
	{
		int idx;

		idx = map_id - OD_TABLE_FPS_WEIGHT0_R;
		*val = pparam->od_tables[table_idx]->fps_table[idx].value_r;
		break;
	}
	case OD_TABLE_FPS_WEIGHT0_B:
	case OD_TABLE_FPS_WEIGHT1_B:
	case OD_TABLE_FPS_WEIGHT2_B:
	case OD_TABLE_FPS_WEIGHT3_B:
	case OD_TABLE_FPS_WEIGHT4_B:
	case OD_TABLE_FPS_WEIGHT5_B:
	case OD_TABLE_FPS_WEIGHT6_B:
	case OD_TABLE_FPS_WEIGHT7_B:
	case OD_TABLE_FPS_WEIGHT8_B:
	case OD_TABLE_FPS_WEIGHT9_B:
	case OD_TABLE_FPS_WEIGHT10_B:
	case OD_TABLE_FPS_WEIGHT11_B:
	case OD_TABLE_FPS_WEIGHT12_B:
	case OD_TABLE_FPS_WEIGHT13_B:
	case OD_TABLE_FPS_WEIGHT14_B:
	{
		int idx;

		idx = map_id - OD_TABLE_FPS_WEIGHT0_B;
		*val = pparam->od_tables[table_idx]->fps_table[idx].value_b;
		break;
	}
	case OD_TABLE_BL0:
	case OD_TABLE_BL1:
	case OD_TABLE_BL2:
	case OD_TABLE_BL3:
	case OD_TABLE_BL4:
	case OD_TABLE_BL5:
	case OD_TABLE_BL6:
	case OD_TABLE_BL7:
	case OD_TABLE_BL8:
	case OD_TABLE_BL9:
	case OD_TABLE_BL10:
	case OD_TABLE_BL11:
	case OD_TABLE_BL12:
	case OD_TABLE_BL13:
	case OD_TABLE_BL14:
	{
		int idx;

		idx = map_id - OD_TABLE_BL0;
		*val = pparam->od_tables[table_idx]->bl_table[idx].item;
		break;
	}
	case OD_TABLE_BL_WEIGHT0:
	case OD_TABLE_BL_WEIGHT1:
	case OD_TABLE_BL_WEIGHT2:
	case OD_TABLE_BL_WEIGHT3:
	case OD_TABLE_BL_WEIGHT4:
	case OD_TABLE_BL_WEIGHT5:
	case OD_TABLE_BL_WEIGHT6:
	case OD_TABLE_BL_WEIGHT7:
	case OD_TABLE_BL_WEIGHT8:
	case OD_TABLE_BL_WEIGHT9:
	case OD_TABLE_BL_WEIGHT10:
	case OD_TABLE_BL_WEIGHT11:
	case OD_TABLE_BL_WEIGHT12:
	case OD_TABLE_BL_WEIGHT13:
	case OD_TABLE_BL_WEIGHT14:
	{
		int idx;

		idx = map_id - OD_TABLE_BL_WEIGHT0;
		*val = pparam->od_tables[table_idx]->bl_table[idx].value;
		break;
	}
	case OD_TABLE_BL_WEIGHT0_R:
	case OD_TABLE_BL_WEIGHT1_R:
	case OD_TABLE_BL_WEIGHT2_R:
	case OD_TABLE_BL_WEIGHT3_R:
	case OD_TABLE_BL_WEIGHT4_R:
	case OD_TABLE_BL_WEIGHT5_R:
	case OD_TABLE_BL_WEIGHT6_R:
	case OD_TABLE_BL_WEIGHT7_R:
	case OD_TABLE_BL_WEIGHT8_R:
	case OD_TABLE_BL_WEIGHT9_R:
	case OD_TABLE_BL_WEIGHT10_R:
	case OD_TABLE_BL_WEIGHT11_R:
	case OD_TABLE_BL_WEIGHT12_R:
	case OD_TABLE_BL_WEIGHT13_R:
	case OD_TABLE_BL_WEIGHT14_R:
	{
		int idx;

		idx = map_id - OD_TABLE_BL_WEIGHT0_R;
		*val = pparam->od_tables[table_idx]->bl_table[idx].value_r;
		break;
	}
	case OD_TABLE_BL_WEIGHT0_B:
	case OD_TABLE_BL_WEIGHT1_B:
	case OD_TABLE_BL_WEIGHT2_B:
	case OD_TABLE_BL_WEIGHT3_B:
	case OD_TABLE_BL_WEIGHT4_B:
	case OD_TABLE_BL_WEIGHT5_B:
	case OD_TABLE_BL_WEIGHT6_B:
	case OD_TABLE_BL_WEIGHT7_B:
	case OD_TABLE_BL_WEIGHT8_B:
	case OD_TABLE_BL_WEIGHT9_B:
	case OD_TABLE_BL_WEIGHT10_B:
	case OD_TABLE_BL_WEIGHT11_B:
	case OD_TABLE_BL_WEIGHT12_B:
	case OD_TABLE_BL_WEIGHT13_B:
	case OD_TABLE_BL_WEIGHT14_B:
	{
		int idx;

		idx = map_id - OD_TABLE_BL_WEIGHT0_B;
		*val = pparam->od_tables[table_idx]->bl_table[idx].value_b;
		break;
	}
	case ODDMR_BASE_ADDRESS:
	{
		*val = (uint32_t)comp->regs_pa;
		break;
	}
	default:
		ret = -EFAULT;
		break;
	}
	DDPINFO("%s:%d, ret %d 0x%x,0x%x\n",
			__func__, __LINE__, ret, map_id, *val);
	return ret;
}

int mtk_oddmr_od_tuning_write(struct mtk_ddp_comp *comp, uint32_t table_idx,
		struct mtk_oddmr_sw_reg *sw_reg, struct mtk_oddmr_od_param *pparam)
{
	uint32_t map_id, val;
	int ret = 0;

	map_id = (sw_reg->reg & 0xFFFF) / 4;
	val = sw_reg->val;
	switch (map_id) {
	case OD_BASIC_BIN_VERSION:
		pparam->od_basic_info.basic_param.bin_version = val;
		break;
	case OD_BASIC_RESOLUTION_SWITCH_MODE:
		pparam->od_basic_info.basic_param.resolution_switch_mode = val;
		break;
	case OD_BASIC_PANEL_WIDTH:
		pparam->od_basic_info.basic_param.panel_width = val;
		break;
	case OD_BASIC_PANEL_HEIGHT:
		pparam->od_basic_info.basic_param.panel_height = val;
		break;
	case OD_BASIC_TABLE_CNT:
		pparam->od_basic_info.basic_param.table_cnt = val;
		break;
	case OD_BASIC_OD_MODE:
		pparam->od_basic_info.basic_param.od_mode = val;
		break;
	case OD_BASIC_DITHER_SEL:
		pparam->od_basic_info.basic_param.dither_sel = val;
		break;
	case OD_BASIC_DITHER_CTL:
		pparam->od_basic_info.basic_param.dither_ctl = val;
		break;
	case OD_BASIC_SCALING_MODE:
		pparam->od_basic_info.basic_param.scaling_mode = val;
		break;
	case OD_BASIC_OD_HSK2:
		pparam->od_basic_info.basic_param.od_hsk_2 = val;
		break;
	case OD_BASIC_OD_HSK3:
		pparam->od_basic_info.basic_param.od_hsk_3 = val;
		break;
	case OD_BASIC_OD_HSK4:
		pparam->od_basic_info.basic_param.od_hsk_4 = val;
		break;
	case OD_BASIC_RESERVED:
		break;
	case OD_BASIC_NONLINEAR_NODE_CNT:
		pparam->od_basic_info.basic_param.nonlinear_node_cnt = val;
		break;
	case OD_TABLE_WIDTH:
		pparam->od_tables[table_idx]->table_basic_info.width = val;
		break;
	case OD_TABLE_HEIGHT:
		pparam->od_tables[table_idx]->table_basic_info.height = val;
		break;
	case OD_TABLE_FPS:
		pparam->od_tables[table_idx]->table_basic_info.fps = val;
		break;
	case OD_TABLE_BL:
		pparam->od_tables[table_idx]->table_basic_info.dbv = val;
		break;
	case OD_TABLE_MIN_FPS:
		pparam->od_tables[table_idx]->table_basic_info.min_fps = val;
		break;
	case OD_TABLE_MAX_FPS:
		pparam->od_tables[table_idx]->table_basic_info.max_fps = val;
		break;
	case OD_TABLE_MIN_BL:
		pparam->od_tables[table_idx]->table_basic_info.min_dbv = val;
		break;
	case OD_TABLE_MAX_BL:
		pparam->od_tables[table_idx]->table_basic_info.max_dbv = val;
		break;
	case OD_TABLE_RESERVED:
		break;
	case OD_TABLE_REMAP_GIAN:
		pparam->od_tables[table_idx]->table_basic_info.remap_gian = val;
		break;
	case OD_TABLE_TABLE_OFFSET:
		pparam->od_tables[table_idx]->table_basic_info.table_offset = val;
		break;
	case OD_TABLE_FPS_CNT:
		pparam->od_tables[table_idx]->fps_cnt = val;
		break;
	case OD_TABLE_BL_CNT:
		pparam->od_tables[table_idx]->bl_cnt = val;
		break;
	case OD_TABLE_FPS0:
	case OD_TABLE_FPS1:
	case OD_TABLE_FPS2:
	case OD_TABLE_FPS3:
	case OD_TABLE_FPS4:
	case OD_TABLE_FPS5:
	case OD_TABLE_FPS6:
	case OD_TABLE_FPS7:
	case OD_TABLE_FPS8:
	case OD_TABLE_FPS9:
	case OD_TABLE_FPS10:
	case OD_TABLE_FPS11:
	case OD_TABLE_FPS12:
	case OD_TABLE_FPS13:
	case OD_TABLE_FPS14:
	{
		int idx;

		idx = map_id - OD_TABLE_FPS0;
		pparam->od_tables[table_idx]->fps_table[idx].item = val;
		break;
	}
	case OD_TABLE_FPS_WEIGHT0:
	case OD_TABLE_FPS_WEIGHT1:
	case OD_TABLE_FPS_WEIGHT2:
	case OD_TABLE_FPS_WEIGHT3:
	case OD_TABLE_FPS_WEIGHT4:
	case OD_TABLE_FPS_WEIGHT5:
	case OD_TABLE_FPS_WEIGHT6:
	case OD_TABLE_FPS_WEIGHT7:
	case OD_TABLE_FPS_WEIGHT8:
	case OD_TABLE_FPS_WEIGHT9:
	case OD_TABLE_FPS_WEIGHT10:
	case OD_TABLE_FPS_WEIGHT11:
	case OD_TABLE_FPS_WEIGHT12:
	case OD_TABLE_FPS_WEIGHT13:
	case OD_TABLE_FPS_WEIGHT14:
	{
		int idx;

		idx = map_id - OD_TABLE_FPS_WEIGHT0;
		pparam->od_tables[table_idx]->fps_table[idx].value = val;
		break;
	}
	case OD_TABLE_FPS_WEIGHT0_R:
	case OD_TABLE_FPS_WEIGHT1_R:
	case OD_TABLE_FPS_WEIGHT2_R:
	case OD_TABLE_FPS_WEIGHT3_R:
	case OD_TABLE_FPS_WEIGHT4_R:
	case OD_TABLE_FPS_WEIGHT5_R:
	case OD_TABLE_FPS_WEIGHT6_R:
	case OD_TABLE_FPS_WEIGHT7_R:
	case OD_TABLE_FPS_WEIGHT8_R:
	case OD_TABLE_FPS_WEIGHT9_R:
	case OD_TABLE_FPS_WEIGHT10_R:
	case OD_TABLE_FPS_WEIGHT11_R:
	case OD_TABLE_FPS_WEIGHT12_R:
	case OD_TABLE_FPS_WEIGHT13_R:
	case OD_TABLE_FPS_WEIGHT14_R:
	{
		int idx;

		idx = map_id - OD_TABLE_FPS_WEIGHT0_R;
		pparam->od_tables[table_idx]->fps_table[idx].value_r = val;
		break;
	}
	case OD_TABLE_FPS_WEIGHT0_B:
	case OD_TABLE_FPS_WEIGHT1_B:
	case OD_TABLE_FPS_WEIGHT2_B:
	case OD_TABLE_FPS_WEIGHT3_B:
	case OD_TABLE_FPS_WEIGHT4_B:
	case OD_TABLE_FPS_WEIGHT5_B:
	case OD_TABLE_FPS_WEIGHT6_B:
	case OD_TABLE_FPS_WEIGHT7_B:
	case OD_TABLE_FPS_WEIGHT8_B:
	case OD_TABLE_FPS_WEIGHT9_B:
	case OD_TABLE_FPS_WEIGHT10_B:
	case OD_TABLE_FPS_WEIGHT11_B:
	case OD_TABLE_FPS_WEIGHT12_B:
	case OD_TABLE_FPS_WEIGHT13_B:
	case OD_TABLE_FPS_WEIGHT14_B:
	{
		int idx;

		idx = map_id - OD_TABLE_FPS_WEIGHT0_B;
		pparam->od_tables[table_idx]->fps_table[idx].value_b = val;
		break;
	}
	case OD_TABLE_BL0:
	case OD_TABLE_BL1:
	case OD_TABLE_BL2:
	case OD_TABLE_BL3:
	case OD_TABLE_BL4:
	case OD_TABLE_BL5:
	case OD_TABLE_BL6:
	case OD_TABLE_BL7:
	case OD_TABLE_BL8:
	case OD_TABLE_BL9:
	case OD_TABLE_BL10:
	case OD_TABLE_BL11:
	case OD_TABLE_BL12:
	case OD_TABLE_BL13:
	case OD_TABLE_BL14:
	{
		int idx;

		idx = map_id - OD_TABLE_BL0;
		pparam->od_tables[table_idx]->bl_table[idx].item = val;
		break;
	}
	case OD_TABLE_BL_WEIGHT0:
	case OD_TABLE_BL_WEIGHT1:
	case OD_TABLE_BL_WEIGHT2:
	case OD_TABLE_BL_WEIGHT3:
	case OD_TABLE_BL_WEIGHT4:
	case OD_TABLE_BL_WEIGHT5:
	case OD_TABLE_BL_WEIGHT6:
	case OD_TABLE_BL_WEIGHT7:
	case OD_TABLE_BL_WEIGHT8:
	case OD_TABLE_BL_WEIGHT9:
	case OD_TABLE_BL_WEIGHT10:
	case OD_TABLE_BL_WEIGHT11:
	case OD_TABLE_BL_WEIGHT12:
	case OD_TABLE_BL_WEIGHT13:
	case OD_TABLE_BL_WEIGHT14:
	{
		int idx;

		idx = map_id - OD_TABLE_BL_WEIGHT0;
		pparam->od_tables[table_idx]->bl_table[idx].value = val;
		break;
	}
	case OD_TABLE_BL_WEIGHT0_R:
	case OD_TABLE_BL_WEIGHT1_R:
	case OD_TABLE_BL_WEIGHT2_R:
	case OD_TABLE_BL_WEIGHT3_R:
	case OD_TABLE_BL_WEIGHT4_R:
	case OD_TABLE_BL_WEIGHT5_R:
	case OD_TABLE_BL_WEIGHT6_R:
	case OD_TABLE_BL_WEIGHT7_R:
	case OD_TABLE_BL_WEIGHT8_R:
	case OD_TABLE_BL_WEIGHT9_R:
	case OD_TABLE_BL_WEIGHT10_R:
	case OD_TABLE_BL_WEIGHT11_R:
	case OD_TABLE_BL_WEIGHT12_R:
	case OD_TABLE_BL_WEIGHT13_R:
	case OD_TABLE_BL_WEIGHT14_R:
	{
		int idx;

		idx = map_id - OD_TABLE_BL_WEIGHT0_R;
		pparam->od_tables[table_idx]->bl_table[idx].value_r = val;
		break;
	}
	case OD_TABLE_BL_WEIGHT0_B:
	case OD_TABLE_BL_WEIGHT1_B:
	case OD_TABLE_BL_WEIGHT2_B:
	case OD_TABLE_BL_WEIGHT3_B:
	case OD_TABLE_BL_WEIGHT4_B:
	case OD_TABLE_BL_WEIGHT5_B:
	case OD_TABLE_BL_WEIGHT6_B:
	case OD_TABLE_BL_WEIGHT7_B:
	case OD_TABLE_BL_WEIGHT8_B:
	case OD_TABLE_BL_WEIGHT9_B:
	case OD_TABLE_BL_WEIGHT10_B:
	case OD_TABLE_BL_WEIGHT11_B:
	case OD_TABLE_BL_WEIGHT12_B:
	case OD_TABLE_BL_WEIGHT13_B:
	case OD_TABLE_BL_WEIGHT14_B:
	{
		int idx;

		idx = map_id - OD_TABLE_BL_WEIGHT0_B;
		pparam->od_tables[table_idx]->bl_table[idx].value_b = val;
		break;
	}
	default:
		ret = -EFAULT;
		break;
	}
	DDPINFO("%s:%d, ret %d 0x%x,0x%x\n",
			__func__, __LINE__, ret, map_id, val);
	return ret;
}


