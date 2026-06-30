// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2016 MediaTek Inc.

#include <asm-generic/errno-base.h>
#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/sysfs.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by vmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#include <linux/io.h>

#include <audio_ipi_queue.h>

#if IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
#include <scp_helper.h>
#include <scp_audio_ipi.h>
#include <scp_feature_define.h>
#endif

#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
#include <adsp_helper.h>
#endif


#include <audio_log.h>
#include <audio_assert.h>

#include <audio_controller_msg_id.h>
#include <audio_messenger_ipi.h>

#include <audio_task_manager.h>

#include <audio_ipi_dma.h>
#include <audio_ipi_platform.h>
#include <audio_ipi_platform_auto.h>

#include <adsp_ipi_queue.h>



static int parsing_ipi_msg_from_kernel_space(
	void *user_data_ptr,
	uint8_t data_type)
{
	struct ipi_msg_t ipi_msg;
	struct ipi_msg_dma_info_t *dma_info = NULL;
	struct aud_data_t *wb_dram = NULL;

	uint32_t msg_len = 0;

	uint32_t hal_data_size = 0;
	void *copy_hal_data = NULL;

	uint32_t hal_wb_buf_size = 0;
	void *hal_wb_buf_addr = NULL;

	phys_addr_t dram_buf = 0;
	void *dram_buf_virt = NULL;

	int retval = 0;


	/* get message size to read */
	msg_len = msg_len_of_type(data_type);
	if (msg_len > sizeof(struct ipi_msg_t))  {
		pr_notice("msg_len %u > %zu!!",
			  msg_len, sizeof(struct ipi_msg_t));
		retval = -1;
		goto parsing_exit;
	}

	memset(&ipi_msg, 0, sizeof(struct ipi_msg_t));
	if (user_data_ptr == NULL) {
		pr_notice("user_data_ptr is null");
		retval = -EINVAL;
		goto parsing_exit;
	}
	memcpy(&ipi_msg, user_data_ptr, msg_len);

	if (ipi_msg.data_type != data_type) { /* double check */
		pr_notice("data_type %d != %d", ipi_msg.data_type, data_type);
		retval = -1;
		goto parsing_exit;
	}
	if (ipi_msg.source_layer != AUDIO_IPI_LAYER_FROM_HAL) {
		pr_notice("source_layer %d != %d", ipi_msg.source_layer, AUDIO_IPI_LAYER_FROM_HAL);
		retval = -1;
		goto parsing_exit;
	}
	msg_len = get_message_buf_size(&ipi_msg);
	retval = check_msg_format(&ipi_msg, msg_len);
	if (retval != 0)
		goto parsing_exit;

	ipi_dbg("task %d msg 0x%x", ipi_msg.task_scene, ipi_msg.msg_id);

	/* get dma buf if need */
	dma_info = &ipi_msg.dma_info;
	wb_dram = &dma_info->wb_dram;

	if (data_type == AUDIO_IPI_DMA) {
		/* check DMA & Write-Back size */
		hal_data_size = dma_info->hal_buf.data_size;
		if (hal_data_size > MAX_DSP_DMA_WRITE_SIZE) {
			DUMP_IPI_MSG("hal_data_size error!!", &ipi_msg);
			retval = -1;
			goto parsing_exit;
		}
		hal_wb_buf_size = dma_info->hal_buf.memory_size;
		if (hal_wb_buf_size > MAX_DSP_DMA_WRITE_SIZE) {
			DUMP_IPI_MSG("hal_wb_buf_size error!!", &ipi_msg);
			retval = -1;
			goto parsing_exit;
		}

		/* get hal data & write hal data to DRAM */
		copy_hal_data = vmalloc(hal_data_size);
		if (copy_hal_data == NULL) {
			retval = -ENOMEM;
			goto parsing_exit;
		}
		if (dma_info->hal_buf.addr == NULL) {
			pr_notice("dma_info->hal_buf.addr is NULL");
			retval = -EINVAL;
			goto parsing_exit;
		}
		memcpy(copy_hal_data,
				 (void *)dma_info->hal_buf.addr,
				 hal_data_size);
		dma_info->data_size = hal_data_size;
		retval = audio_ipi_dma_write_region(
				 ipi_msg.task_scene,
				 copy_hal_data,
				 hal_data_size,
				 &dma_info->rw_idx);
		if (retval != 0) {
			pr_notice("dma write region error!!");
			goto parsing_exit;
		}

		/* write back result to hal later, like get parameter */
		hal_wb_buf_addr = (void *)dma_info->hal_buf.addr;
		ipi_dbg(
			"write region copy_hal_data(%p), hal_data_size %d, hal_wb_buf_size %d"
			, copy_hal_data, hal_data_size, hal_wb_buf_size);
		if (hal_wb_buf_size != 0 && hal_wb_buf_addr != NULL) {
			/* alloc a dma for wb */
			audio_ipi_dma_alloc(ipi_msg.task_scene,
					    &dram_buf,
					    &dram_buf_virt,
					    hal_wb_buf_size);

			wb_dram->memory_size = hal_wb_buf_size;
			wb_dram->data_size = 0;
			wb_dram->addr_val = dram_buf;
			ipi_dbg(
				"hal_wb_buf_addr(%p), wb dram_buf(0x%x), hal_wb_buf_size(%d)"
				, hal_wb_buf_addr, (uint32_t)dram_buf, hal_wb_buf_size);

			/* force need ack to get dsp info */
			if (ipi_msg.ack_type != AUDIO_IPI_MSG_NEED_ACK) {
				pr_notice("task %d msg 0x%x need ack!!",
					  ipi_msg.task_scene, ipi_msg.msg_id);
				ipi_msg.ack_type = AUDIO_IPI_MSG_NEED_ACK;
			}
		}
#ifdef DEBUG_IPI
		DUMP_IPI_MSG("dma", &ipi_msg);
#endif
	}

	/* sent message */
	retval = audio_send_ipi_filled_msg(&ipi_msg);
	if (retval != 0) {
		pr_notice("audio_send_ipi_filled_msg error!!");
		goto parsing_exit;
	}


	/* write back data to hal */
	if (data_type == AUDIO_IPI_DMA &&
	    hal_wb_buf_size != 0 &&
	    hal_wb_buf_addr != NULL &&
	    wb_dram != NULL &&
	    wb_dram->addr_val != 0 &&
	    ipi_msg.dsp_ret == 1) {
		if (wb_dram->data_size > hal_wb_buf_size) {
			pr_notice("wb_dram->data_size %u > hal_wb_buf_size %u!!",
				  wb_dram->data_size,
				  hal_wb_buf_size);
			ipi_msg.dsp_ret = 0;
		} else if (wb_dram->data_size == 0) {
			pr_notice("ipi wb data sz = 0!! check adsp write");
			ipi_msg.dsp_ret = 0;
		} else {
			if (dram_buf_virt == NULL) {
				pr_info("memcpy dma err, id = 0x%x",
					ipi_msg.msg_id);
				ipi_msg.dsp_ret = 0;
			} else
				memcpy(hal_wb_buf_addr,
					 dram_buf_virt,
					 wb_dram->data_size);
		}
	}


	/* write back ipi msg to hal */
	if (data_type == AUDIO_IPI_DMA) /* clear sensitive addr info */
		memset(&ipi_msg.dma_info, 0, IPI_MSG_DMA_INFO_SIZE);

	memcpy(user_data_ptr,
			      &ipi_msg,
			      sizeof(struct ipi_msg_t));

parsing_exit:
	if (copy_hal_data != NULL) {
		vfree(copy_hal_data);
		copy_hal_data = NULL;
	}
	if (dram_buf != 0) {
		ipi_dbg(
			"task %d msg 0x%x, free wb buffer 0x%x, hal_wb_buf_size 0x%x"
			, ipi_msg.task_scene, ipi_msg.msg_id,
			(uint32_t)dram_buf, hal_wb_buf_size);
		audio_ipi_dma_free(ipi_msg.task_scene,
				   dram_buf, hal_wb_buf_size);
	}
	ipi_dbg("task %d msg 0x%x, retval %d",
		ipi_msg.task_scene, ipi_msg.msg_id, retval);

	return retval;
}

long audio_ipi_kernel_ioctl(
	unsigned int cmd, unsigned long arg)
{
#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
	uint32_t dsp_id = 0;
#endif

	struct audio_ipi_reg_dma_t dma_reg;
	int retval = 0;
	uint32_t check_sum = 0;

	AUD_LOG_V("cmd = %u, arg = %lu", cmd, arg);

	switch (cmd) {
	case AUDIO_IPI_IOCTL_SEND_MSG_ONLY: {
		retval = parsing_ipi_msg_from_kernel_space(
				 (void *)arg, AUDIO_IPI_MSG_ONLY);
		break;
	}
	case AUDIO_IPI_IOCTL_SEND_PAYLOAD: {
		retval = parsing_ipi_msg_from_kernel_space(
				 (void *)arg, AUDIO_IPI_PAYLOAD);
		break;
	}
	case AUDIO_IPI_IOCTL_SEND_DRAM: {
		retval = parsing_ipi_msg_from_kernel_space(
				 (void *)arg, AUDIO_IPI_DMA);
		break;
	}
	case AUDIO_IPI_IOCTL_INIT_DSP: {
		pr_debug("AUDIO_IPI_IOCTL_INIT_DSP");
		mutex_lock(&init_dsp_lock);
#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
		for (dsp_id = 0; dsp_id < NUM_OPENDSP_TYPE; dsp_id++) {
			if (is_audio_use_adsp(dsp_id))
				audio_ipi_init_dsp_hifi3(dsp_id);

		}
#endif
#if IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
		if (is_audio_scp_support())
			audio_ipi_init_dsp_rv();
#endif
		/* copy g_audio_task_info to HAL */
		if (((void *)arg) == NULL)
			retval = -EINVAL;
		else
			memcpy((void *)arg,
					      g_audio_task_info,
					      sizeof(g_audio_task_info));

		mutex_unlock(&init_dsp_lock);
		break;
	}
	case AUDIO_IPI_IOCTL_REG_DMA: {
		if (((void *)arg) == NULL) {
			retval = -1;
			break;
		}
		memcpy(&dma_reg,
				 (void *)arg,
				 sizeof(struct audio_ipi_reg_dma_t));

		check_sum = dma_reg.magic_footer + dma_reg.magic_header;
		if (check_sum != 0xFFFFFFFF) {
			pr_notice("dma reg check fail! header(0x%x) footer(0x%x)",
				  dma_reg.magic_header,
				  dma_reg.magic_footer);
			retval = -1;
			break;
		}

		mutex_lock(&reg_dma_lock);
		if (dma_reg.reg_flag)
			retval = audio_ipi_dma_alloc_region(dma_reg.task,
							    dma_reg.a2d_size,
							    dma_reg.d2a_size);
		else
			retval = audio_ipi_dma_free_region(dma_reg.task);
		mutex_unlock(&reg_dma_lock);

		break;
	}
	default:
		retval = -ENOIOCTLCMD;
		break;
	}
	return retval;
}
EXPORT_SYMBOL(audio_ipi_kernel_ioctl);

