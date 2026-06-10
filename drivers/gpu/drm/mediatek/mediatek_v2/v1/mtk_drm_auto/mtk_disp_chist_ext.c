// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/slab.h>
#include "mtk_drm_crtc.h"
#include "mtk_drm_drv.h"
#include "mtk_log.h"
#include "mtk_disp_chist.h"
#include "mtk_disp_chist_ext.h"
#include "cmdq_chist_backup.h"
#include "mtk_dump.h"
#include "mtk_disp_pq_helper.h"
#include <linux/delay.h>
#include <linux/clk.h>

static atomic_t g_chist_sof_irq_available = ATOMIC_INIT(0);
static atomic_t g_chist_hist_available = ATOMIC_INIT(0);

static DECLARE_WAIT_QUEUE_HEAD(g_chist_sof_irq_wq);
static DECLARE_WAIT_QUEUE_HEAD(g_chist_hist_wq);

struct mutex chist_config_lock;
struct mutex chist_readback_lock;

#define DISP_CHIST_COLOR_FORMAT 0x3ff
/* channel 0~3 has 256 bins, 4~6 has 128 bins */
#define DISP_CHIST_MAX_BIN 256
#define DISP_CHIST_MAX_BIN_LOW 128

/* chist custom info */
#define DISP_CHIST_HWC_CHANNEL_INDEX 4
/* chist custom info end*/

#define DISP_CHIST_YUV_PARAM_COUNT  12
#define DISP_CHIST_POST_PARAM_INDEX 9

#define DISP_CHIST_MAX_RGB 0x0321

#define DISP_CHIST_DUAL_PIPE_OVERLAP 0

#define DISP_CHIST_EN                0x0
#define DISP_CHIST_INTEN             0x08
#define DISP_CHIST_INSTA             0x0C
#define DISP_CHIST_CFG               0x20
#define DISP_CHIST_SIZE              0x30
#define DISP_CHIST_CONFIG            0x40
#define DISP_CHIST_Y2R_PAPA_R0       0x50
#define DISP_CHIST_Y2R_PAPA_POST_A0  0x80
#define DISP_CHIST_SHADOW_CTRL       0xF0
#define FLD_APB_MCYC_RD REG_FLD_MSB_LSB(3, 3)
#define FLD_READ_WRK_REG REG_FLD_MSB_LSB(2, 2)
#define FLD_FORCE_COMMIT REG_FLD_MSB_LSB(1, 1)
#define FLD_BYPASS_SHADOW REG_FLD_MSB_LSB(0, 0)
// channel_n_win_x_main = DISP_CHIST_CH0_WIN_X_MAIN + n * 0x10
#define DISP_CHIST_CH0_WIN_X_MAIN    0x0460
#define DISP_CHIST_CH0_WIN_Y_MAIN    0x0464
#define DISP_CHIST_CH0_BLOCK_INFO    0x0468
#define DISP_CHIST_CH0_BLOCK_CROP    0x046C
#define DISP_CHIST_WEIGHT            0x0500
#define DISP_CHIST_BLD_CONFIG        0x0504
#define DISP_CHIST_HIST_CH_CFG1      0x0510
#define DISP_CHIST_HIST_CH_CFG2      0x0514
#define DISP_CHIST_HIST_CH_CFG3      0x0518
#define DISP_CHIST_HIST_CH_CFG4      0x051C
#define DISP_CHIST_HIST_CH_CFG5      0x0520
#define DISP_CHIST_HIST_CH_CFG6      0x0524
#define DISP_CHIST_HIST_CH_CNF0      0x0528
#define DISP_CHIST_HIST_CH_CNF1      0x052C
#define DISP_CHIST_HIST_CH_CH0_CNF0  0x0530
#define DISP_CHIST_HIST_CH_CH0_CNF1  0x0534
#define DISP_CHIST_HIST_CH_MON       0x0568

#define DISP_CHIST_APB_READ          0x0600
#define DISP_CHIST_SRAM_R_IF         0x0680


#define DISP_CHIST_CONFIG_LIST_NODE_SIZE_MAX 4

enum CHIST_IOCTL_CMD {
	CHIST_CONFIG = 0,
	CHIST_UNKNOWN,
};

struct chist_config_node {
	struct drm_mtk_chist_channel_config_ext (*data)[CHIST_CHANNEL_CONFIG_MAX];
	int size;   //real channel numble in one node
	struct chist_config_node *prev;
	struct chist_config_node *next;
};

static struct chist_config_node *chist_config_list_head;
static int chist_config_node_count;
static struct drm_mtk_channel_available_hists_ext available_hists;
static struct mtk_ddp_comp *default_comp;
static struct mtk_ddp_comp *default_comp1;

int printf_all_chist_config_nodes(struct chist_config_node *head)
{
	struct chist_config_node *p;

	p = head;
	DDPDBG("----------%s in----------\n", __func__);

	if (head == NULL)
		return 0;

	do {
		for (int i = 0; i < p->size; i++) {
			DDPDBG("%s i[%d] id:%d %d, xywh<%d %d %d %d> size<%d>\n", __func__,
				i, (*p->data)[i].chist_id, (*p->data)[i].channel_id,
				(*p->data)[i].roi_start_x, (*p->data)[i].roi_start_y,
				(*p->data)[i].blk_width, (*p->data)[i].blk_height, p->size);
			}
		p = p->next;

	} while (p != head);
	DDPDBG("----------%s out----------\n", __func__);

	return 0;
}


static inline bool modify_channel_flag_if_needed(unsigned int chist_id, unsigned int channel_id,
	unsigned int roi_start_x, unsigned int roi_start_y,
	unsigned int blk_width, unsigned int blk_height, unsigned int flag)
{
	struct chist_config_node *temp = chist_config_list_head;

	while (temp != NULL) {
		for (int i = 0; i < temp->size; i++) {
			if (temp->data[i]->chist_id ==
				chist_id && temp->data[i]->channel_id == channel_id
				&& temp->data[i]->roi_start_x ==
				roi_start_x && temp->data[i]->roi_start_y == roi_start_y
				&& temp->data[i]->blk_width ==
				blk_width && temp->data[i]->blk_height == blk_height
				&& temp->data[i]->flag != flag) {
				temp->data[i]->flag = flag;
				return true;
			}
		}
		temp = temp->next;
		if (temp == chist_config_list_head)
			break;
	}

	return false;
}

static inline bool is_list_full(void)
{
	return chist_config_node_count >= DISP_CHIST_CONFIG_LIST_NODE_SIZE_MAX;
}

static inline void inc_chist_config_node_count(void)
{
	chist_config_node_count++;
	DDPDBG("++chist_config_node_count(%d)\n", chist_config_node_count);
}

static inline void dec_chist_config_node_count(void)
{
	if (chist_config_node_count <= 0) {
		DDPDBG("chist_config_node_count(%d) <= 0\n", chist_config_node_count);
		chist_config_node_count = 0;
	} else
		chist_config_node_count--;

	DDPDBG("--chist_config_node_count(%d)\n", chist_config_node_count);
}

static inline bool is_data_valid(unsigned int chist_id, unsigned int channel_id,
	unsigned int roi_start_x, unsigned int roi_start_y,
	unsigned int blk_width, unsigned int blk_height)
{
	struct mtk_disp_chist *chist_data;

	chist_data = comp_to_chist(default_comp);

	if (chist_id >= DISP_CHIST_COUNT)
		return false;
	if (channel_id >= DISP_CHIST_CHANNEL_COUNT)
		return false;
	if (roi_start_x + blk_width > chist_data->primary_data->frame_width ||
		roi_start_y + blk_height > chist_data->primary_data->frame_height)
		return false;

	return true;
}

static inline int insert_chist_config_data_to_list(
	struct drm_mtk_chist_channel_config_ext (*dataArray)[CHIST_CHANNEL_CONFIG_MAX], int size)
{
	struct chist_config_node *newNode = NULL;

	mutex_lock(&chist_config_lock);

	if (is_list_full()) {
		DDPMSG("[E] %s Circular Linked List is full\n", __func__);
		mutex_unlock(&chist_config_lock);
		return -1;
	}

	if (size > CHIST_CHANNEL_CONFIG_MAX) {
		mutex_unlock(&chist_config_lock);
		return -1;
	}

	for (int i = 0; i < size; i++) {
		unsigned int chist_id = (*dataArray)[i].chist_id;
		unsigned int channel_id = (*dataArray)[i].channel_id;
		unsigned int roi_start_x = (*dataArray)[i].roi_start_x;
		unsigned int roi_start_y = (*dataArray)[i].roi_start_y;
		unsigned int blk_width = (*dataArray)[i].blk_width;
		unsigned int blk_height = (*dataArray)[i].blk_height;
		unsigned int flag = (*dataArray)[i].flag;

		DDPDBG("%s id:[%d:%d] xy(%d,%d) wh[%dx%d]\n", __func__,
			chist_id, channel_id, roi_start_x, roi_start_y, blk_width, blk_height);

		if (!is_data_valid(chist_id, channel_id,
			roi_start_x, roi_start_y, blk_width, blk_height)) {
			DDPMSG("[E] %s Invalid data\n", __func__);
			mutex_unlock(&chist_config_lock);
			return -1;
		}

		if (flag == DISABLEHW && modify_channel_flag_if_needed(chist_id,
			channel_id, roi_start_x, roi_start_y, blk_width, blk_height, flag))
			continue;

		if (newNode == NULL) {
			DDPDBG("%s create a new node.\n", __func__);
			newNode = kmalloc(sizeof(struct chist_config_node), GFP_KERNEL);
			if (newNode == NULL) {
				DDPPR_ERR("%s line = %d BUG kmalloc newNode fail\n",
					__func__, __LINE__);
				mutex_unlock(&chist_config_lock);
				return -1;
			}
			newNode->size = 0;
			newNode->data =
			(struct drm_mtk_chist_channel_config_ext (*)[CHIST_CHANNEL_CONFIG_MAX])
			kmalloc_array(CHIST_CHANNEL_CONFIG_MAX,
			sizeof(struct drm_mtk_chist_channel_config_ext), GFP_KERNEL);
			if (newNode->data == NULL) {
				DDPPR_ERR("%s line = %d BUG kmalloc newNode->data fail\n",
					__func__, __LINE__);
				kfree(newNode);
				mutex_unlock(&chist_config_lock);
				return -1;
			}

			if (chist_config_list_head == NULL) {
				chist_config_list_head = newNode;
				newNode->next = newNode;
				newNode->prev = newNode;
			} else {
				chist_config_list_head->prev->next = newNode;
				newNode->prev = chist_config_list_head->prev;
				newNode->next = chist_config_list_head;
				chist_config_list_head->prev = newNode;
			}

			inc_chist_config_node_count();
			DDPDBG("%s line = %d chist_config_node_count = %d.\n",
				__func__, __LINE__, chist_config_node_count);
		}
			memcpy(&(*newNode->data)[newNode->size],
				&(*dataArray)[i], sizeof((*dataArray)[i]));

			DDPDBG("Insert node: size:%d id:[%d:%d] xy(%d,%d) wh[%dx%d]\n",
				newNode->size, (*newNode->data)[newNode->size].chist_id,
				(*newNode->data)[newNode->size].channel_id,
				(*newNode->data)[newNode->size].roi_start_x,
				(*newNode->data)[newNode->size].roi_start_y,
				(*newNode->data)[newNode->size].blk_width,
				(*newNode->data)[newNode->size].blk_height);

			newNode->size++;
	}

	printf_all_chist_config_nodes(chist_config_list_head);

	mutex_unlock(&chist_config_lock);
	return 0;
}

static inline int delete_chist_config_node(struct chist_config_node *del)
{
	if (del == NULL)
		return -1;
	del->size = 0;
	if (del->prev != del) {
		del->prev->next = del->next;
		del->next->prev = del->prev;
	}
	if (del->data != NULL) {
		kfree(del->data);
		del->data = NULL;
	}
	kfree(del);
	del = NULL;
	dec_chist_config_node_count();

	return 0;
}

static inline int delete_all_chist_config_nodes(struct chist_config_node *head)
{
	mutex_lock(&chist_config_lock);

	if (head == NULL) {
		mutex_unlock(&chist_config_lock);
		return 0;
	}

	while (head->next != head) {
		head = head->next;
		delete_chist_config_node(head->prev);
	}

	delete_chist_config_node(head);

	if (chist_config_list_head != NULL) {
		DDPDBG("%s line = %d ========Should never in=========\n", __func__, __LINE__);
		chist_config_list_head = NULL;
		chist_config_node_count = 0;
	}

	mutex_unlock(&chist_config_lock);

	return 0;
}


static inline int delete_disabled_channel_after_used(struct chist_config_node *curr)
{
	int newSize = 0;

	if (curr == NULL)
		return -1;

	for (int i = 0; i < curr->size; i++) {
		if (curr->data[i]->flag != DISABLEHW) {
			memcpy(&curr->data[newSize], &curr->data[i], sizeof(curr->data[i]));
			newSize++;
		}
	}
	curr->size = newSize;

	if (curr->size == 0)
		delete_chist_config_node(curr);

	return 0;
}

static inline void move_chist_config_list_head_to_next(struct chist_config_node *head)
{
	mutex_lock(&chist_config_lock);

	if (head == NULL)
		DDPDBG("chist_config_nodeList is NULL!\n");
	else
		chist_config_list_head = chist_config_list_head->next;

	mutex_unlock(&chist_config_lock);
}

static inline int disp_chist_shift_num(struct mtk_ddp_comp *comp)
{
	return comp_to_chist(comp)->data->chist_shift_num;
}

static const struct component_ops mtk_disp_chist_component_ops_ext = {
	.bind = disp_chist_bind, .unbind = disp_chist_unbind,
};

void disp_chist_wait_sof_irq_ext(void)
{
	int ret = -1;

	if (atomic_read(&g_chist_sof_irq_available) == 0) {
		ret = wait_event_interruptible(g_chist_sof_irq_wq,
				atomic_read(&g_chist_sof_irq_available) == 1);
	} else {
		return;
	}
}

void disp_chist_on_start_of_frame_ext(struct mtk_ddp_comp *comp)
{
	if (!atomic_read(&g_chist_sof_irq_available) && chist_config_node_count > 0) {
		atomic_set(&g_chist_sof_irq_available, 1);
		wake_up_interruptible(&g_chist_sof_irq_wq);
	}
}

int disp_chist_get_channel_region_info_ext(struct cmdq_pkt *handle, struct slot_info *slot)
{
	cmdq_pkt_backup_chist_region_info(handle, slot);

	return 0;
}

int disp_chist_set_channel_config_ext(struct cmdq_pkt *handle, struct chist_config_node *curr)
{
	struct mtk_ddp_comp *comp;
	unsigned int channel;
	bool enable;
	int enable_val[2] = {0, 0};
	struct mtk_disp_chist *chist_data;

	chist_data = comp_to_chist(default_comp);
	static int first_config = 1;

	mutex_lock(&chist_config_lock);

	if (curr == NULL) {
		mutex_unlock(&chist_config_lock);
		return -1;
	}
	if (first_config == 1) {
		cmdq_pkt_write(handle, default_comp->cmdq_base,
				default_comp->regs_pa + DISP_CHIST_CFG, 0x106, ~0);
		cmdq_pkt_write(handle, default_comp1->cmdq_base,
				default_comp1->regs_pa + DISP_CHIST_CFG, 0x106, ~0);
	}

	for (int i = 0; i < curr->size; i++) {
		if (curr == NULL) {
			mutex_unlock(&chist_config_lock);
			return -1;
		}

		channel = (*curr->data)[i].channel_id;
		if (channel >= DISP_CHIST_CHANNEL_COUNT) {
			DDPMSG("[E] %s(%d) Invalid channel_id:%d\n", __func__, __LINE__, channel);
			mutex_unlock(&chist_config_lock);
			return -1;
		}

		DDPDBG("%s size[%d] id:%d %d, xywh<%d %d %d %d>",
			__func__, i, (*curr->data)[i].chist_id, channel,
			(*curr->data)[i].roi_start_x, (*curr->data)[i].roi_start_y,
			(*curr->data)[i].blk_width, (*curr->data)[i].blk_height);

		if ((*curr->data)[i].chist_id == 0)
			comp = default_comp;
		else
			comp = default_comp1;

		//roi
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_CHIST_CH0_WIN_X_MAIN + channel * 0x10,
			(((*curr->data)[i].roi_start_x + (*curr->data)[i].blk_width - 1) << 16) |
			(*curr->data)[i].roi_start_x, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_CHIST_CH0_WIN_Y_MAIN + channel * 0x10,
			(((*curr->data)[i].roi_start_y + (*curr->data)[i].blk_height - 1) << 16) |
			(*curr->data)[i].roi_start_y, ~0);

		if (first_config == 1) {
		// block
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_CHIST_CH0_BLOCK_INFO + channel * 0x10,
				((chist_data->primary_data->frame_height << 16) |
				(chist_data->primary_data->frame_width & 0xFFF)), ~0);

			// bin count, 0:256,1:128,2:64,3:32,4:16,5:8
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_CHIST_HIST_CH_CFG3,
				disp_chist_bin_count_regs(
				(*curr->data)[i].bin_count) << channel * 4,
				0x07 << channel * 4);
			// channel sel color format, channel bin count
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_CHIST_HIST_CH_CFG2,
				(*curr->data)[i].color_format << channel * 4, 0x0F << channel * 4);

			// (HS)V = max(R G B)
			if ((*curr->data)[i].color_format == MTK_DRM_COLOR_FORMAT_M)
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_CHIST_HIST_CH_CFG4,
					DISP_CHIST_MAX_RGB, ~0);
			//if (curr->size >= 12)
				//first_config = 0;
		}

		enable_val[(*curr->data)[i].chist_id] |= (1 << channel);

	}
	cmdq_pkt_write(handle, default_comp->cmdq_base,
			default_comp->regs_pa + DISP_CHIST_HIST_CH_CFG1,
			enable_val[0], 0x7F);
	cmdq_pkt_write(handle, default_comp1->cmdq_base,
			default_comp1->regs_pa + DISP_CHIST_HIST_CH_CFG1,
			enable_val[1], 0x7F);

	mutex_unlock(&chist_config_lock);
	return 0;
}

int disp_chist_parsing_data_from_cmdq_buffer(struct slot_info *slot)
{
	static char *buffer;
	int i;

	buffer = (char *)cmdq_slot_get_va(slot, 0);

	if (buffer) {
		unsigned int chist0_cfg = *((unsigned int *)buffer);

		DDPDBG("%s, chist0_cfg = %d.\n", __func__, chist0_cfg);
		for (i = 0; i < 7; i++) {
			int chist0_channel_enable =
				(chist0_cfg >> i) & 0x1;  //get bit0 ~ bit6 channel enable status

			if (chist0_channel_enable == 1) {
				available_hists.hist_available[i] = 1;
				available_hists.hist_num++;
				(*available_hists.channel_hist)[i].chist_id = 0;
				(*available_hists.channel_hist)[i].channel_id = i;
				(*available_hists.channel_hist)[i].roi_start_x =
					(*((unsigned int *)(buffer + 4 + (12 * i)))) & 0x1FFF;
				(*available_hists.channel_hist)[i].roi_start_y =
					(*((unsigned int *)(buffer + 8 + (12 * i)))) & 0x1FFF;
				(*available_hists.channel_hist)[i].blk_height =
				((*((unsigned int *)(buffer + 8 + (12 * i))) >> 16) & 0x1FFF) -
					(*available_hists.channel_hist)[i].roi_start_y + 1;
				(*available_hists.channel_hist)[i].blk_width =
				((*((unsigned int *)(buffer + 4 + (12 * i))) >> 16) & 0x1FFF) -
					(*available_hists.channel_hist)[i].roi_start_x + 1;
				memcpy(&((*available_hists.channel_hist)[i]).hist,
					((unsigned int *)(buffer + 176 + (128 * i))), 128);
			}
		}

		unsigned int chist1_cfg = *((unsigned int *)(buffer + 88));

		DDPDBG("%s, chist1_cfg = %d.\n", __func__, chist1_cfg);
		for (i = 0; i < 7; i++) {
			int chist1_channel_enable =
				(chist1_cfg >> i) & 0x1;  //get bit0 ~ bit6 channel enable status

			if (chist1_channel_enable == 1) {
				available_hists.hist_available[i + 7] = 1;
				available_hists.hist_num++;
				(*available_hists.channel_hist)[i + 7].chist_id = 1;
				(*available_hists.channel_hist)[i + 7].channel_id = i;
				(*available_hists.channel_hist)[i + 7].roi_start_x =
					(*((unsigned int *)(buffer + 92 + (12 * i)))) & 0x1FFF;
				(*available_hists.channel_hist)[i + 7].roi_start_y =
					(*((unsigned int *)(buffer + 96 + (12 * i)))) & 0x1FFF;
				(*available_hists.channel_hist)[i + 7].blk_height =
					((*((unsigned int *)(buffer + 96 + (12 * i))) >> 16) &
					0x1FFF) -
					(*available_hists.channel_hist)[i + 7].roi_start_y + 1;
				(*available_hists.channel_hist)[i + 7].blk_width =
					((*((unsigned int *)(buffer + 92 + (12 * i))) >> 16) &
					0x1FFF) -
					(*available_hists.channel_hist)[i + 7].roi_start_x + 1;
				memcpy(&((*available_hists.channel_hist)[i + 7]).hist,
					((unsigned int *)(buffer + 1072 + (128 * i))), 128);
			}
		}
	}

	if (buffer != NULL)
		memset(buffer, 0, 2000);

	for (i = 0; i < 14; i++) {
		DDPDBG("<%2d>en<%d><%4d, %4d, %4d, %4d><%8d %8d %8d %8d %8d %8d %8d %8d>\n",
		i, available_hists.hist_available[i],
		(*available_hists.channel_hist)[i].roi_start_x,
		(*available_hists.channel_hist)[i].roi_start_y,
		(*available_hists.channel_hist)[i].blk_width,
		(*available_hists.channel_hist)[i].blk_height,
		(*available_hists.channel_hist)[i].hist[0],
		(*available_hists.channel_hist)[i].hist[4],
		(*available_hists.channel_hist)[i].hist[8],
		(*available_hists.channel_hist)[i].hist[12],
		(*available_hists.channel_hist)[i].hist[16],
		(*available_hists.channel_hist)[i].hist[20],
		(*available_hists.channel_hist)[i].hist[24],
		(*available_hists.channel_hist)[i].hist[28]);
	}

	return 0;
}

int disp_chist_read_hist_ext(struct cmdq_pkt *handle, struct slot_info *slot)
{
	int max_bins = 32;
	struct mtk_ddp_comp *comp;
	unsigned int chist_id;
	unsigned int channel_id;

	cmdq_pkt_backup_chist_hist(handle, slot);

	return 0;
}


static int cmdq_cb_done;

void disp_chist_cmdq_cb(struct cmdq_cb_data data)
{
	unsigned long flags;
	struct slot_info *slot = (struct slot_info *)data.data;

	mutex_lock(&chist_readback_lock);

	spin_lock_irqsave(&available_hists.data_lock, flags);

	disp_chist_parsing_data_from_cmdq_buffer(slot);

	//read_hist_with_cpu();

	spin_unlock_irqrestore(&available_hists.data_lock, flags);

	mutex_unlock(&chist_readback_lock);

	cmdq_pkt_destroy(slot->pkt);
	cmdq_cb_done = 0;
	cmdq_slot_free(slot);

	atomic_set(&g_chist_hist_available, 1);
	wake_up_interruptible(&g_chist_hist_wq);
}

void disp_chist_wait_hist(struct mtk_ddp_comp *comp)
{
	int ret = 0;

	if (atomic_read(&g_chist_hist_available) == 0) {
		DDPDBG("%s wait_event_interruptible\n", __func__);
		ret = wait_event_interruptible(g_chist_hist_wq,
				atomic_read(&g_chist_hist_available) == 1);
		DDPDBG("%s g_chist_hist_available = 1, waken up, ret = %d", __func__, ret);
	} else {
		DDPDBG("%s g_chist_hist_available = 0\n", __func__);
	}
}

int disp_chist_kthread_ext(void *data)
{
	struct chist_config_node *curr;
	struct slot_info *slot = NULL;
	struct cmdq_client *client = NULL;
	int cnt = 0;
	struct cmdq_pkt *chist_pkt = NULL;
	int cnt_debug = 0;

	cmdq_cb_done = 0;

	while (1) {
		disp_chist_wait_sof_irq_ext();

		if (kthread_should_stop()) {
			DDPMSG("[E] %s stopped\n", __func__);
			break;
		}

		cnt_debug = 0;
		while (cmdq_cb_done != 0) {
			cnt_debug++;
			if (cnt_debug > 6)
				break;
			usleep_range(2900, 3000);
		}
		DDPDBG("cmdq_cb_done = %d cnt_debug = %d!", cmdq_cb_done, cnt_debug);

		mutex_lock(&chist_readback_lock);

		cmdq_cb_done = 1;

		curr = chist_config_list_head;
		if (curr != NULL) {
			// search available client.
			if (default_comp->mtk_crtc->gce_obj.client[CLIENT_PQ_EOF])
				client = default_comp->mtk_crtc->gce_obj.client[CLIENT_PQ_EOF];
			else
				client = default_comp->mtk_crtc->gce_obj.client[CLIENT_CFG];

			// create pkt
			mtk_crtc_pkt_create(&chist_pkt,
				&default_comp->mtk_crtc->base, client);

			/* wait frame done */
			cmdq_pkt_clear_event(chist_pkt,
			default_comp->mtk_crtc->gce_obj.event[EVENT_VDO_EOF]);
			cmdq_pkt_wfe(chist_pkt,
				default_comp->mtk_crtc->gce_obj.event[EVENT_VDO_EOF]);

			slot = cmdq_slot_alloc(client);
			slot->pkt = chist_pkt;
			DDPDBG("%s+ line = %d pageSize:%d.\n",
				__func__, __LINE__, PAGE_SIZE);
			disp_chist_get_channel_region_info_ext(chist_pkt, slot);
			curr = chist_config_list_head;
			disp_chist_set_channel_config_ext(chist_pkt, curr);
			disp_chist_read_hist_ext(chist_pkt, slot);
			cmdq_pkt_flush_threaded(chist_pkt, disp_chist_cmdq_cb, slot);

			move_chist_config_list_head_to_next(chist_config_list_head);
			cnt++;
			atomic_set(&g_chist_sof_irq_available, 0);
		}

		mutex_unlock(&chist_readback_lock);
	}

	return 0;
}

int disp_chist_copy_hist_to_user_ext(struct drm_mtk_channel_hists_ext *hists)
{
	unsigned long flags;
	unsigned int count = 0;

	spin_lock_irqsave(&available_hists.data_lock, flags);

	for (int i = 0; i < CHIST_CHANNEL_CONFIG_MAX; i++) {
		if (available_hists.hist_available[i] == 1) {
			if (&hists->channel_hist[count] == NULL)
				DDPPR_ERR("%s line = %d hists->channel_hist[count] = NULL.\n",
					__func__, __LINE__);

			if (&((*available_hists.channel_hist)[i]) == NULL)
				DDPMSG("[E] %s available_hists.channel_hist[i] = 0x%llx.\n",
					__func__, &((*available_hists.channel_hist)[i]));

			memcpy(&hists->channel_hist[count],
				&((*available_hists.channel_hist)[i]),
				sizeof(struct drm_mtk_channel_hist_ext));

			available_hists.hist_available[i] = 0;
			count++;

		}
	}
	available_hists.hist_num = count;

	spin_unlock_irqrestore(&available_hists.data_lock, flags);
	atomic_set(&g_chist_hist_available, 0);

	return 0;
}


int disp_chist_act_get_hist_ext(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;

	if (data == NULL) {
		DDPPR_ERR("%s line = %d data == NULL.\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (atomic_read(&g_chist_hist_available) == 1) {
		ret = disp_chist_copy_hist_to_user_ext((struct drm_mtk_channel_hists_ext *) data);
		if (ret < 0)
			return -EFAULT;
	} else {
		disp_chist_wait_hist(comp);
		ret = disp_chist_copy_hist_to_user_ext((struct drm_mtk_channel_hists_ext *) data);
		if (ret < 0)
			return -EFAULT;
	}

	return ret;
}

int disp_chist_act_set_config_ext(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	struct drm_mtk_chist_channels_configs_ext *chist_configs_ext =
		(struct drm_mtk_chist_channels_configs_ext *) data;

	DDPINFO("%s ++\n", __func__);

	mutex_lock(&chist_readback_lock);

	if (chist_configs_ext->set_channel_num == 0) {
		ret = delete_all_chist_config_nodes(chist_config_list_head);
	} else {
		struct drm_mtk_chist_channel_config_ext channel_configs[CHIST_CHANNEL_CONFIG_MAX]
		= {0};

		copy_from_user(channel_configs, chist_configs_ext->channel_config,
		sizeof(struct drm_mtk_chist_channel_config_ext) * CHIST_CHANNEL_CONFIG_MAX);
		ret = insert_chist_config_data_to_list(channel_configs,
			chist_configs_ext->set_channel_num);
	}

	mutex_unlock(&chist_readback_lock);
	DDPINFO("%s --\n", __func__);
	return ret;
}

int disp_chist_act_get_caps_ext(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	struct drm_mtk_chist_caps *caps_info = data;
	struct mtk_disp_chist *chist_data;
	unsigned int index = caps_info->device_id & 0xffff;

	DDPINFO("%s ++\n", __func__);

	if (comp == NULL) {
		DDPPR_ERR("%s, null pointer! index:%d\n", __func__, index);
		return -1;
	}
	chist_data = comp_to_chist(comp);

	caps_info->lcm_width = chist_data->primary_data->frame_width;
	caps_info->lcm_height = chist_data->primary_data->frame_height;
	caps_info->support_color = DISP_CHIST_COLOR_FORMAT;

	DDPDBG("%s chist id:%d, w:%d,h:%d\n", __func__, caps_info->device_id,
		caps_info->lcm_width, caps_info->lcm_height);
	DDPINFO("%s --\n", __func__);
	return ret;
}

static int disp_chist_ioctl_transact_ext(struct mtk_ddp_comp *comp,
		      unsigned int cmd, void *params, unsigned int size)
{
	int ret = -1;

	if (comp->id == DDP_COMPONENT_CHIST0) {
		switch (cmd) {
		case PQ_CHIST_GET_HIST_EXT:
			ret = disp_chist_act_get_hist_ext(comp, params);
			break;
		case PQ_CHIST_SET_CONFIG_EXT:
			ret = disp_chist_act_set_config_ext(comp, params);
			break;
		case PQ_CHIST_GET_CAPS_EXT:
			ret = disp_chist_act_get_caps_ext(comp, params);
			break;
		default:
			break;
		}
	} else {
		ret = 0;
	}

	return ret;
}

void disp_chist_init_primary_data_ext(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_chist *chist_data = comp_to_chist(comp);
	struct mtk_disp_chist *companion_data = comp_to_chist(chist_data->companion);
	struct cmdq_client *client = NULL;

	if (chist_data->is_right_pipe) {
		kfree(chist_data->primary_data);
		chist_data->primary_data = NULL;
		chist_data->primary_data = companion_data->primary_data;
		return;
	}

	// init primary data
	init_waitqueue_head(&(chist_data->primary_data->event_wq));
	mutex_init(&(chist_data->primary_data->clk_lock));
	mutex_init(&(chist_data->primary_data->data_lock));

	memset(&(chist_data->primary_data->block_config), 0,
			sizeof(chist_data->primary_data->block_config));
	memset(&(chist_data->primary_data->chist_config), 0,
			sizeof(chist_data->primary_data->chist_config));
	memset(&(chist_data->primary_data->disp_hist), 0,
			sizeof(chist_data->primary_data->disp_hist));

	atomic_set(&(chist_data->primary_data->irq_event), 0);
	atomic_set(&(chist_data->primary_data->clock_on), 0);
	chist_data->primary_data->need_restore = 0;
	chist_data->primary_data->pre_frame_width = 0;
	chist_data->primary_data->present_fence = 0;
	chist_data->primary_data->pipe_width = 0;
	chist_data->primary_data->frame_width = 0;
	chist_data->primary_data->frame_height = 0;

	if (comp->id == DDP_COMPONENT_CHIST0) {
		mutex_init(&chist_config_lock);
		mutex_init(&chist_readback_lock);
		if (default_comp->mtk_crtc->gce_obj.client[CLIENT_PQ_EOF])
			client = default_comp->mtk_crtc->gce_obj.client[CLIENT_PQ_EOF];
		else
			client = default_comp->mtk_crtc->gce_obj.client[CLIENT_CFG];

	}
}

int disp_chist_io_cmd_ext(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	      enum mtk_ddp_io_cmd cmd, void *params)
{
	switch (cmd) {
	case PQ_FILL_COMP_PIPE_INFO:
	{
		struct mtk_disp_chist *data = comp_to_chist(comp);
		bool *is_right_pipe = &data->is_right_pipe;
		int ret, *path_order = &data->path_order;
		struct mtk_ddp_comp **companion = &data->companion;
		struct mtk_disp_chist *companion_data;

		if (atomic_read(&comp->mtk_crtc->pq_data->pipe_info_filled) == 1)
			break;
		ret = disp_pq_helper_fill_comp_pipe_info(comp,
			path_order, is_right_pipe, companion);
		if (!ret && comp->mtk_crtc->is_dual_pipe && data->companion) {
			companion_data = comp_to_chist(data->companion);
			companion_data->path_order = data->path_order;
			companion_data->is_right_pipe = !data->is_right_pipe;
			companion_data->companion = comp;
		}
		disp_chist_init_primary_data_ext(comp);
		if (comp->mtk_crtc->is_dual_pipe && data->companion)
			disp_chist_init_primary_data_ext(data->companion);
	}
		break;
	default:
		break;
	}

	return 0;
}


/* don't need start funcs, chist will start by ioctl_set_config*/

static const struct mtk_ddp_comp_funcs mtk_disp_chist_funcs = {
	.config = disp_chist_config,
	.first_cfg = disp_chist_first_cfg,
	.bypass = disp_chist_bypass,
	.user_cmd = disp_chist_user_cmd,
	.prepare = disp_chist_prepare,
	.unprepare = disp_chist_unprepare,
	.pq_frame_config = disp_chist_frame_config,
	.io_cmd = disp_chist_io_cmd_ext,
	.pq_ioctl_transact = disp_chist_ioctl_transact_ext,
	.mutex_sof_irq = disp_chist_on_start_of_frame_ext,
};

int disp_chist_probe_ext(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_chist *priv;
	struct task_struct *task;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	priv->primary_data = kzalloc(sizeof(*priv->primary_data), GFP_KERNEL);
	if (priv->primary_data == NULL) {
		ret = -ENOMEM;
		DDPPR_ERR("Failed to alloc primary_data %d\n", ret);
		goto error_dev_init;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_CHIST);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		ret = comp_id;
		goto error_primary;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_chist_funcs);
	if (ret != 0) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		ret = -ENOMEM;
		goto error_primary;
	}

	priv->data = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_chist_component_ops_ext);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	if (!default_comp && comp_id == DDP_COMPONENT_CHIST0) {
		default_comp = &priv->ddp_comp;

	if (available_hists.channel_hist == NULL) {
		available_hists.channel_hist =
			(struct drm_mtk_channel_hist_ext (*)[CHIST_CHANNEL_CONFIG_MAX])
			kmalloc_array(CHIST_CHANNEL_CONFIG_MAX,
			sizeof(struct drm_mtk_channel_hist_ext), GFP_KERNEL);
	}

		for (int i = 0; i < CHIST_CHANNEL_CONFIG_MAX; i++)
			available_hists.hist_available[i] = 0;

		spin_lock_init(&(available_hists.data_lock));
		task = kthread_create(disp_chist_kthread_ext, NULL, "disp_chist_kthread_ext");
		if (IS_ERR(task)) {
			DDPPR_ERR("%s fail to create mtk_chist_kthread.\n", __func__);
			return PTR_ERR(task);
		}
		wake_up_process(task);
	}
	if (!default_comp1 && comp_id == DDP_COMPONENT_CHIST1)
		default_comp1 = &priv->ddp_comp;

error_primary:
	if (ret < 0)
		kfree(priv->primary_data);
error_dev_init:
	if (ret < 0)
		devm_kfree(dev, priv);

	DDPINFO("%s-\n", __func__);
	return ret;
}

int disp_chist_remove_ext(struct platform_device *pdev)
{
	struct mtk_disp_chist *priv = dev_get_drvdata(&pdev->dev);

	if (available_hists.channel_hist != NULL)
		kfree(available_hists.channel_hist);

	component_del(&pdev->dev, &mtk_disp_chist_component_ops_ext);

	mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	return 0;
}

