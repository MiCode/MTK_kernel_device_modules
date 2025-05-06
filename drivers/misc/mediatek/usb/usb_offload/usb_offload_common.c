// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Offload Driver
 * *
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Jeremy Chou <jeremy.chou@mediatek.com>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/usb.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/usb/audio.h>
#include <linux/usb/audio-v2.h>
#include <linux/usb/audio-v3.h>
#include <linux/uaccess.h>
#include <sound/pcm.h>
#include <sound/core.h>
#include <sound/asound.h>
#include <linux/iommu.h>
#include <linux/dma-mapping.h>
/* #include <trace/hooks/audio_usboffload.h> */
#include "clk-mtk.h"

#if IS_ENABLED(CONFIG_SND_USB_AUDIO)
#include "usbaudio.h"
#include "card.h"
#include "helper.h"
#include "pcm.h"
#include "power.h"
#endif
#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
#include <adsp_helper.h>
#include "audio_messenger_ipi.h"
#include "audio_task.h"
#include "audio_controller_msg_id.h"
#endif
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
#include <scp_rv.h>
#endif
#include "../usb_xhci/xhci.h"
#include "../usb_xhci/xhci-mtk.h"
#include "../usb_xhci/xhci-trace.h"
#include "mtu3.h"
#include "usb_boost.h"
#include "usb_offload.h"
#include "audio_task_usb_msg_id.h"

static void (*mbrain_cb)(struct uo_mbrain data);

unsigned int usb_offload_log;
module_param(usb_offload_log, uint, 0644);
MODULE_PARM_DESC(usb_offload_log, "Enable/Disable USB Offload log");

unsigned int debug_memory_log;
module_param(debug_memory_log, uint, 0644);
MODULE_PARM_DESC(debug_memory_log, "Enable/Disable Debug Memory log");

static struct usb_audio_dev uadev[SNDRV_CARDS];
struct usb_offload_dev *uodev;

static void check_valid_device(struct usb_device *rhdev,
	struct usb_device *udev, bool *support, bool *bypass);

/* driver event sync */
#define DRV_STAGE_IDLE       (1U << 0)
#define DRV_STAGE_FILE_OPS   (1U << 1)
#define DRV_STAGE_ALSA_OPS   (1U << 2)
#define DRV_STAGE_XHCI_TRACE (1U << 3)

#define WAIT_IDLE_TIMEOUT_NS 1000000000 /* 1 sec */
static int stage_occupy(unsigned long stage);
static inline void stage_vacate(unsigned long stage);

/* audio interface */
static void uaudio_dev_intf_cleanup(struct intf_info *info);
static void uaudio_dev_intf_release(struct usb_audio_dev *dev, int info_idx);
static int uaudio_dev_intf_prepare(struct usb_audio_dev *dev, struct intf_info *info);
static void uaudio_dev_intf_lift(struct usb_audio_dev *dev, struct intf_info *info);

/* audio device */
static void uaudio_dev_cleanup(struct usb_audio_dev *dev);
static void uaudio_dev_release(struct kref *kref);
static void uaudio_dev_shutdown(struct usb_audio_dev *dev);
static int uaudio_dev_create_sideband(struct usb_audio_dev *dev);
static void uaudio_dev_remove_sideband(struct usb_audio_dev *dev);
static struct usb_audio_dev *uaudio_dev_match(struct xhci_virt_device *vdev);

/* disable state */
#define NOT_DISABLE   (0)
#define ON_DISABLE    (1)
#define DONE_DISABLE  (2)
#define DISABLE_WAIT_TIME 10000 /* 10 secs */

/* ipi message helper */
static int send_init_adsp(void);
static int send_deinit_adsp(void);
static int send_enable_stream(struct usb_audio_stream_msg *msg, int info_idx);
static int send_disable_stream(u8 card_num, u8 direction, int info_idx, bool must_success);

/* xhci helper */
static int xhci_prepare_offloading(struct usb_device *udev,
	struct xhci_sideband_ *sb, unsigned int pipe);
static void xhci_lift_offloading(struct usb_device *udev,
	struct xhci_sideband_ *sb, unsigned int pipe);
static struct xhci_ring *xhci_mtk_alloc_ring(struct xhci_hcd *xhci,
	int num_segs, int cycle_state, enum xhci_ring_type ring_type,
	unsigned int max_packet, gfp_t mem_flags,
	enum uo_provider_type type, bool is_rsv);
static int xhci_mtk_alloc_erst(struct usb_offload_dev *udev,
	struct xhci_ring *evt_ring, struct xhci_erst *erst);
static struct xhci_ring *xhci_mtk_alloc_event_ring(struct usb_offload_dev *udev);
static int xhci_mtk_ring_expansion(struct xhci_hcd *xhci,
	struct xhci_ring *ring, dma_addr_t phys, void *vir);

/* usb offload device helper */
static void usb_offload_start_offloading(struct usb_audio_dev *dev);
static void usb_offload_end_offloading(struct usb_audio_dev *dev);

static void usb_offload_status(void)
{
	USB_OFFLOAD_INFO("adsp(init:%d ready:%d) stream:%d(tx:%d rx:%d) card:%d(speed:%d)\n",
		uodev->adsp_inited, uodev->adsp_ready, uodev->is_streaming, uodev->tx_streaming,
		uodev->rx_streaming, uodev->last_card_num, uodev->speed);
}

static void print_all_memory(void)
{
	struct uo_buffer *array;
	int i, type, length;

	for (type = 0; type < UO_STRUCT_NUM; type++) {
		if (!uodev->buf_array[type].first_buf)
			continue;
		array = uodev->buf_array[type].first_buf;
		length = uodev->buf_array[type].length;
		for (i = 0; i < length; i++) {
			if (array[i].allocated)
				USB_OFFLOAD_MEM_DBG("%s\n", mtk_offload_parse_buffer(&array[i]));
		}
	}

	for (i = 0; i < UO_PROV_NUM; i++) {
		if (mtk_offload_provider_is_valid(i))
			USB_OFFLOAD_INFO("[id:%d] %s\n", i, mtk_offload_provider_parse_count(i));
	}
}

static void adsp_ee_recovery(void)
{
	struct xhci_intr_reg *ir_set ;
	u32 temp, irq_pending;
	u64 temp_64;

	if (!uodev->xhci)
		return;

	ir_set = &uodev->xhci->run_regs->ir_set[XHCI1_INTR_TARGET];

	USB_OFFLOAD_INFO("ADSP EE ++ op:0x%08x, iman:0x%08X, erdp:0x%llX\n",
			readl(&uodev->xhci->op_regs->status),
			readl(&ir_set->irq_pending),
			xhci_read_64(uodev->xhci, &ir_set->erst_dequeue));

	USB_OFFLOAD_INFO("// Disabling event ring interrupts\n");
	temp = readl(&uodev->xhci->op_regs->status);
	writel((temp & ~0x1fff) | STS_EINT, &uodev->xhci->op_regs->status);
	temp = readl(&ir_set->irq_pending);
	writel(ER_IRQ_DISABLE(temp), &ir_set->irq_pending);

	irq_pending = readl(&ir_set->irq_pending);
	irq_pending |= IMAN_IP;
	writel(irq_pending, &ir_set->irq_pending);

	temp_64 = xhci_read_64(uodev->xhci, &ir_set->erst_dequeue);
	/* Clear the event handler busy flag (RW1C) */
	temp_64 |= ERST_EHB;
	xhci_write_64(uodev->xhci, temp_64, &ir_set->erst_dequeue);

	USB_OFFLOAD_INFO("ADSP EE -- op:0x%08x, iman:0x%08X, erdp:0x%llX\n",
			readl(&uodev->xhci->op_regs->status),
			readl(&ir_set->irq_pending),
			xhci_read_64(uodev->xhci, &ir_set->erst_dequeue));
}

#ifdef CFG_RECOVERY_SUPPORT
static int usb_offload_event_receive(struct notifier_block *this, unsigned long event,
			    void *ptr)
{
	switch (event) {
	case ADSP_EVENT_STOP:
		USB_OFFLOAD_INFO("<ADSP STOP(%ld)>\n", event);
		uodev->adsp_ready = false;
		break;
	case ADSP_EVENT_READY:
		USB_OFFLOAD_INFO("<ADSP READY(%ld)>\n", event);
		uodev->adsp_ready = true;
		break;
	default:
		USB_OFFLOAD_INFO("unknown event:%ld\n", event);
		goto error;
	}

	usb_offload_status();
	if (!uodev->adsp_ready && uodev->total_connected > 0)
		adsp_ee_recovery();
error:
	return 0;
}

static struct notifier_block adsp_usb_offload_notifier = {
	.notifier_call = usb_offload_event_receive,
	.priority = PRIMARY_FEATURE_PRI,
};
#endif
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
#ifdef SKIP
static int usb_offload_event_receive_scp(struct notifier_block *this,
					 unsigned long event,
					 void *ptr)
{

	switch (event) {
	case SCP_EVENT_STOP:
		pr_info("%s event[%lu]\n", __func__, event);
		if (uodev->connected)
			adsp_ee_recovery();
		break;
	case SCP_EVENT_READY:
		pr_info("%s event[%lu]\n", __func__, event);
		break;
	default:
		pr_info("%s event[%lu]\n", __func__, event);
	}
	return 0;
}

static struct notifier_block scp_usb_offload_notifier = {
	.notifier_call = usb_offload_event_receive_scp,
};
#endif
#endif
static struct snd_usb_substream *find_snd_usb_substream(unsigned int card_num,
	unsigned int pcm_idx, unsigned int direction)
{
	struct snd_usb_stream *as;
	struct snd_usb_substream *subs = NULL;
	struct snd_usb_audio *chip = NULL;

	chip = uadev[card_num].chip;
	if (!chip || atomic_read(&chip->shutdown)) {
		USB_OFFLOAD_ERR("instance of usb crad # %d does not exist\n", card_num);
		goto done;
	}

	if (pcm_idx >= chip->pcm_devs) {
		USB_OFFLOAD_ERR("invalid pcm dev number %u > %d\n", pcm_idx, chip->pcm_devs);
		goto done;
	}

	if (direction > SNDRV_PCM_STREAM_CAPTURE) {
		USB_OFFLOAD_ERR("invalid direction %u\n", direction);
		goto done;
	}

	list_for_each_entry(as, &chip->pcm_list, list) {
		if (as->pcm_index == pcm_idx) {
			subs = &as->substream[direction];
			if (!subs->data_endpoint && !subs->sync_endpoint) {
				USB_OFFLOAD_ERR("stream disconnected, bail out\n");
				subs = NULL;
				goto done;
			}
			goto done;
		}
	}
done:
	return subs;
}

static void sound_usb_connect(struct snd_usb_audio *chip)
{
	struct usb_interface *intf = NULL;
	struct usb_device *rhdev;
	bool support, on_hub;
	int if_idx;

	USB_OFFLOAD_INFO("++ chip:%p udev:%p card_num:%d\n",
		chip, chip->dev, chip->card->number);

	if (stage_occupy(DRV_STAGE_ALSA_OPS) < 0)
		goto busy;

	if (usb_offload_link_xhci(uodev->dev) < 0) {
		USB_OFFLOAD_ERR("fail to link xhci\n");
		uo_mbrain_update(UO_PHASE_CONNECT, UO_ERROR_xHCI_NOT_RDY);
		goto err;
	}

	if (chip->num_interfaces > 0 && chip->intf[chip->num_interfaces - 1]) {
		if_idx = (chip->num_interfaces - 1);
		intf = chip->intf[if_idx];
	}

	if (!intf) {
		USB_OFFLOAD_ERR("sideband interface can't be NULL\n");
		goto err;
	}

	rhdev = (xhci_to_hcd(uodev->xhci))->self.root_hub;
	check_valid_device(rhdev, chip->dev, &support, &on_hub);

	uadev[chip->card->number].card_num = chip->card->number;
	uadev[chip->card->number].is_valid = support;
	uadev[chip->card->number].on_hub = on_hub;
	uadev[chip->card->number].chip = chip;
	uadev[chip->card->number].slot_id = chip->dev->slot_id;
	uadev[chip->card->number].sb_intf = intf;
	atomic_set(&uadev[chip->card->number].connected, 1);

	uodev->total_connected++;

	USB_OFFLOAD_INFO("create [card:%d slot:%d] support:%d on_hub:%d total_connected:%d\n",
		chip->card->number, chip->dev->slot_id, support, on_hub, uodev->total_connected);

	uo_mbrain_update(UO_PHASE_CONNECT, UO_ERROR_SUCCESS);
err:
	stage_vacate(DRV_STAGE_ALSA_OPS);
busy:
	USB_OFFLOAD_INFO("--\n");
}

static void sound_usb_disconnect(struct snd_usb_audio *chip)
{
	struct usb_audio_dev *dev;
	unsigned int card_num;

	if (!chip)
		return;

	card_num = chip->card->number;
	USB_OFFLOAD_INFO("++ card_num:%d\n", card_num);
	if (stage_occupy(DRV_STAGE_ALSA_OPS) < 0)
		goto busy;

	if (card_num >= SNDRV_CARDS) {
		USB_OFFLOAD_ERR("invalid card:%d\n", card_num);
		goto err;
	}

	if (!uodev->total_connected) {
		USB_OFFLOAD_ERR("no chip connected (total_connected:%d)\n", uodev->total_connected);
		goto err;
	}

	dev = &uadev[card_num];

	USB_OFFLOAD_INFO("(in_use:%d kref:%d) total_connected:%d\n",
		atomic_read(&dev->in_use), kref_read(&dev->kref), uodev->total_connected);

	/* chip has already been cleaned up or never populated */
	if (!dev->chip) {
		USB_OFFLOAD_ERR("audio device might be cleaned up before ???\n");
		goto err;
	}

	uaudio_dev_shutdown(dev);
	atomic_set(&uadev[card_num].connected, 0);
	uodev->total_connected--;
err:
	stage_vacate(DRV_STAGE_ALSA_OPS);
busy:
	USB_OFFLOAD_INFO("-- total_connected:%d\n", uodev->total_connected);
}

static void monitor_alloc_virt_device(void *unused, struct xhci_virt_device *vdev)
{
	struct xhci_sideband_ *sb = NULL;
	struct usb_audio_dev *dev;

	USB_OFFLOAD_INFO("slot_id:%d vdev:%p\n", vdev->slot_id, vdev);

	dev = uaudio_dev_match(vdev);
	if (dev && dev->chip)
		USB_OFFLOAD_INFO("allocate virtual device after uac connect??\n");
	else
		goto not_match;

	if (stage_occupy(DRV_STAGE_XHCI_TRACE) < 0)
		goto busy;

	sb = dev->sb;
	if (sb) {
		if (sb->vdev && sb->vdev != vdev) {
			/* seems virtual device was released before, assign it manually */
			USB_OFFLOAD_INFO("virtual device unmatch %p <-> %p\n", sb->vdev, vdev);
			sb->vdev = vdev;
			vdev->sideband = sb;
		}
	}

	stage_vacate(DRV_STAGE_XHCI_TRACE);
busy:
not_match:
	return;
}

static void monitor_free_virt_device(void *unused, struct xhci_virt_device *vdev)
{
	struct usb_audio_dev *dev;

	USB_OFFLOAD_INFO("slot_id:%d vdev:%p\n", vdev->slot_id, vdev);

	dev = uaudio_dev_match(vdev);
	if (dev && dev->chip)
		USB_OFFLOAD_INFO("free virtual device before uac disconnect??\n");
	else
		goto not_match;

	if (stage_occupy(DRV_STAGE_XHCI_TRACE) < 0)
		goto busy;

	stage_vacate(DRV_STAGE_XHCI_TRACE);
busy:
not_match:
	return;
}

static bool is_support_format(snd_pcm_format_t fmt)
{
	switch (fmt) {
	case SNDRV_PCM_FORMAT_S8:
	case SNDRV_PCM_FORMAT_U8:
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S16_BE:
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_U16_BE:
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_BE:
	case SNDRV_PCM_FORMAT_U24_LE:
	case SNDRV_PCM_FORMAT_U24_BE:
	case SNDRV_PCM_FORMAT_S24_3LE:
	case SNDRV_PCM_FORMAT_S24_3BE:
	case SNDRV_PCM_FORMAT_U24_3LE:
	case SNDRV_PCM_FORMAT_U24_3BE:
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S32_BE:
	case SNDRV_PCM_FORMAT_U32_LE:
	case SNDRV_PCM_FORMAT_U32_BE:
		return true;
	default:
		return false;
	}
}

static enum usb_audio_device_speed
get_speed_info(enum usb_device_speed udev_speed)
{
	switch (udev_speed) {
	case USB_SPEED_LOW:
		return USB_AUDIO_DEVICE_SPEED_LOW;
	case USB_SPEED_FULL:
		return USB_AUDIO_DEVICE_SPEED_FULL;
	case USB_SPEED_HIGH:
		return USB_AUDIO_DEVICE_SPEED_HIGH;
	case USB_SPEED_SUPER:
		return USB_AUDIO_DEVICE_SPEED_SUPER;
	case USB_SPEED_SUPER_PLUS:
		return USB_AUDIO_DEVICE_SPEED_SUPER_PLUS;
	default:
		USB_OFFLOAD_INFO("udev speed %d\n", udev_speed);
		return USB_AUDIO_DEVICE_SPEED_INVALID;
	}
}

static bool is_pcm_size_valid(struct usb_audio_stream_info *uainfo, struct snd_usb_substream *subs)
{
	struct snd_usb_endpoint *data_ep = NULL;
	int pkt_sz, ringbuf_sz, packs_per_ms;

	if (!uainfo || !subs || !subs->data_endpoint)
		return false;

	data_ep = subs->data_endpoint;
	if (get_speed_info(subs->dev->speed) != USB_AUDIO_DEVICE_SPEED_FULL)
		packs_per_ms = 8 >> data_ep->datainterval;
	else
		packs_per_ms = 1;

	/* multiple by 1000 to avoid 3 digit after decimal dot */
	pkt_sz = 1000 * (((uainfo->bit_depth * uainfo->number_of_ch) >> 3) * uainfo->bit_rate) / data_ep->pps;
	ringbuf_sz = pkt_sz * packs_per_ms * uainfo->dram_size * uainfo->dram_cnt;

	USB_OFFLOAD_DBG("10*pkt_sz:%d 10*ringbuf_sz:%d 10*pcm_sz:%d\n",
		pkt_sz, ringbuf_sz, 10* uainfo->pcm_size);

	/* cause we multiple pkt_sz by 10 before, pcm_size should be also multipled */
	if (ringbuf_sz < uainfo->pcm_size * 10) {
		USB_OFFLOAD_ERR("Invalid uainfo(1000*pcm_sz:%d > 1000*ringbuf_sz:%d) 1000*pkt_sz:%d\n",
			1000 * uainfo->pcm_size, ringbuf_sz, pkt_sz);
		USB_OFFLOAD_ERR("pkt_per_ms:%d interval:%d frame_bit:%d rate:%d pps:%d\n",
			packs_per_ms, data_ep->datainterval, uainfo->bit_depth * uainfo->number_of_ch,
			uainfo->bit_rate, data_ep->pps);
		return false;
	}

	return true;
}

static bool is_uainfo_valid(struct usb_audio_stream_info *uainfo)
{
	if (uainfo == NULL) {
		USB_OFFLOAD_ERR("uainfo is NULL\n");
		return false;
	}

	if (uainfo->enable > 1) {
		USB_OFFLOAD_ERR("uainfo->enable invalid (%d)\n", uainfo->enable);
		return false;
	}

	if (uainfo->bit_rate > 768000) {
		USB_OFFLOAD_ERR("uainfo->bit_rate invalid (%d)\n", uainfo->bit_rate);
		return false;
	}

	if (uainfo->bit_depth > 32) {
		USB_OFFLOAD_ERR("uainfo->bit_depth invalid (%d)\n", uainfo->bit_depth);
		return false;
	}

	if (uainfo->number_of_ch > 2) {
		USB_OFFLOAD_ERR("uainfo->number_of_ch invalid (%d)\n", uainfo->number_of_ch);
		return false;
	}

	if (uainfo->direction > 1) {
		USB_OFFLOAD_ERR("uainfo->direction invalid (%d)\n", uainfo->direction);
		return false;
	}

	if (!is_support_format(uainfo->audio_format)) {
		USB_OFFLOAD_ERR("unsupported pcm format %d\n", uainfo->audio_format);
		return false;
	}


	return true;
}

static void dump_uainfo(struct usb_audio_stream_info *uainfo)
{
	USB_OFFLOAD_INFO(
		"enable:%d rate:%d ch:%d depth:%d dir:%d period:%d card:%d pcm:%d drm_sz:%d drm_cnt:%d\n",
		uainfo->enable,
		uainfo->bit_rate,
		uainfo->number_of_ch,
		uainfo->bit_depth,
		uainfo->direction,
		uainfo->xhc_irq_period_ms,
		uainfo->pcm_card_num,
		uainfo->pcm_dev_num,
		uainfo->dram_size,
		uainfo->dram_cnt);
}

static void uaudio_dev_intf_cleanup(struct intf_info *info)
{
	info->in_use = false;

	if (mtk_offload_free_mem(info->dsp_urb))
		USB_OFFLOAD_ERR("fail to free urb\n");
}

static void uaudio_dev_intf_release(struct usb_audio_dev *dev, int info_idx)
{
	struct intf_info *info;

	if (!dev || info_idx >= dev->num_intf)
		return;
	info = &dev->info[info_idx];

	/* cleanup audio interface */
	USB_OFFLOAD_INFO("cleanup info_idx:%d, card:%d kref:%d\n",
		info_idx, dev->card_num, kref_read(&dev->kref));
	uaudio_dev_intf_cleanup(info);

	info->direction ? (uodev->rx_streaming = false) : (uodev->tx_streaming = false);

	/* release audio device if it's unused anymore */
	if (atomic_read(&dev->in_use))
		kref_put(&dev->kref, uaudio_dev_release);
}

static int uaudio_dev_intf_prepare(struct usb_audio_dev *dev, struct intf_info *info)
{
	int retval = 0;

	if (info->data_ep_pipe) {
		retval = xhci_prepare_offloading(dev->udev, dev->sb, info->data_ep_pipe);
		if (retval < 0) {
			USB_OFFLOAD_ERR("fail to prepare data endpoint\n");
			return retval;
		}
	}

	if (info->sync_ep_pipe) {
		retval = xhci_prepare_offloading(dev->udev, dev->sb, info->sync_ep_pipe);
		if (retval < 0) {
			USB_OFFLOAD_ERR("fail to prepare sync endpoint\n");
			return retval;
		}
	}

	return retval;
}

static void uaudio_dev_intf_lift(struct usb_audio_dev *dev, struct intf_info *info)
{
	if (info->data_ep_pipe) {
		xhci_lift_offloading(dev->udev, dev->sb, info->data_ep_pipe);
		info->data_ep_pipe = 0;
	}

	if (info->sync_ep_pipe) {
		xhci_lift_offloading(dev->udev, dev->sb, info->sync_ep_pipe);
		info->sync_ep_pipe = 0;
	}
}

static int uaudio_dev_create_sideband(struct usb_audio_dev *dev)
{
	struct usb_interface *intf = NULL;
	struct xhci_sideband_ *sb = NULL;

	if (!dev || !dev->sb_intf)
		return -EINVAL;

	intf = dev->sb_intf;
	sb = xhci_sideband_register_(intf, XHCI_SIDEBAND_VENDOR, NULL);
	if (!sb) {
		USB_OFFLOAD_ERR("fail to register sideband, sb_intf:%p\n", intf);
		return -ENOMEM;
	}

	USB_OFFLOAD_INFO("register sideband:%p vdev:%p\n", sb, sb->vdev);
	dev->sb = sb;

	return 0;
}

static void uaudio_dev_remove_sideband(struct usb_audio_dev *dev)
{
	struct xhci_hcd *xhci = uodev->xhci;
	struct xhci_virt_device *vdev;
	struct xhci_sideband_ *sb;
	u32 max_devies;

	if (!dev || !dev->sb)
		return;

	sb = dev->sb;
	max_devies = HCS_MAX_SLOTS(xhci->cap_regs->hcs_params1);
	if (dev->slot_id < max_devies && dev->slot_id >= 0) {
		vdev = xhci->devs[dev->slot_id];
		if (vdev == sb->vdev) {
			USB_OFFLOAD_INFO("unregister sideband:%p\n", sb);
			xhci_sideband_unregister_(sb);
		} else {
			/* seems virtual device was released before, just releasing sideband */
			USB_OFFLOAD_ERR("virtual device unmatch %p <-> %p\n", sb->vdev, vdev);
			kfree(sb);
		}
	} else {
		USB_OFFLOAD_ERR("invalid slot:%d\n", dev->slot_id);
		kfree(sb);
	}

	dev->sb = NULL;
}

static void uaudio_dev_cleanup(struct usb_audio_dev *dev)
{
	int if_idx;

	if (!dev || !dev->udev)
		return;

	USB_OFFLOAD_INFO("cleanup device, card:%d\n", dev->card_num);

	for (if_idx = 0; if_idx < dev->num_intf; if_idx++) {
		if (!dev->info[if_idx].in_use)
			continue;
		uaudio_dev_intf_release(dev, if_idx);
	}

	dev->num_intf = 0;
	kfree(dev->info);
	dev->info = NULL;
	dev->udev = NULL;
}

static void uaudio_dev_release(struct kref *kref)
{
	struct usb_audio_dev *dev = container_of(kref, struct usb_audio_dev, kref);
	struct xhci_sideband_ *sb;

	if (!dev) {
		USB_OFFLOAD_ERR("dev has been freed!!\n");
		return;
	}
	sb = dev->sb;
	USB_OFFLOAD_INFO("all interfaces freed, release card:%d\n", dev->card_num);

	if (!sb || !sb->ir) {
		USB_OFFLOAD_ERR("sideband/interrupter was already freed\n");
		goto skip_remove;
	}
	USB_OFFLOAD_ERR("remove interrupter%d:%p (sb:%p)\n", sb->ir->intr_num, sb->ir, sb);

	/* interrupter */
	xhci_sideband_remove_interrupter_(sb);

	/* sideband */
	uaudio_dev_remove_sideband(dev);

skip_remove:
	atomic_set(&dev->in_use, 0);

	if (uodev->last_card_num == dev->card_num)
		/* all streaming interface on device stopped */
		usb_offload_end_offloading(dev);
	else
		/* unlikely to here ... */
		USB_OFFLOAD_ERR("card:%d doesn't match (last:%d)\n",
			dev->card_num, uodev->last_card_num);
}

static void uaudio_dev_shutdown(struct usb_audio_dev *dev)
{
	u8 card_num = dev->card_num;
	struct intf_info *info;
	int if_idx;

	if (!atomic_read(&dev->in_use))
		goto skip_disable;

	USB_OFFLOAD_INFO("start shutting down...\n");

	for (if_idx = 0; if_idx < dev->num_intf; if_idx++) {
		info = &dev->info[if_idx];
		if (!info->in_use)
			continue;

		/* this might trigger @uaudio_dev_release */
		if (send_disable_stream(card_num, info->direction, if_idx, false) < 0)
			USB_OFFLOAD_ERR("fail to disable audio stream, card:%d info_idx:%d\n",
				card_num, if_idx);

		/* wait for ipi service being ready */
		mdelay(1);
	}

skip_disable:
	uaudio_dev_cleanup(dev);
	dev->chip = NULL;
}

static struct usb_audio_dev *uaudio_dev_match(struct xhci_virt_device *vdev)
{
	int index;

	if (!uodev->total_connected || !vdev)
		goto done;

	for (index = 0; index < SNDRV_CARDS; index++) {
		if (uadev[index].slot_id == vdev->slot_id)
			return &uadev[index];
	}

done:
	return NULL;
}

static void usb_offload_start_offloading(struct usb_audio_dev *dev)
{
	uodev->last_card_num = dev->card_num;
	uodev->speed = dev->udev->speed;
	uodev->is_streaming = true;

	/* hold wakelock if device's'on hub */
	usb_offload_hub_working(dev->on_hub, true);

	/* improve power consumption under deep-idle */
	usb_offload_improve_idle_power(true);

	/* bypass clock cehcker */
	mtk_clk_notify(NULL, NULL, NULL, 1, 1, 0, CLK_EVT_BYPASS_PLL);
}

static void usb_offload_end_offloading(struct usb_audio_dev *dev)
{
	uodev->last_card_num = -EINVAL;
	uodev->speed = USB_SPEED_UNKNOWN;
	uodev->is_streaming = false;

	usb_offload_hid_stop();

	usb_offload_hub_working(dev->on_hub, false);

	usb_offload_improve_idle_power(false);

	mtk_clk_notify(NULL, NULL, NULL, 0, 1, 0, CLK_EVT_BYPASS_PLL);
}

static int send_init_adsp(void)
{
	struct mem_info_xhci *xhci_mem;
	enum uo_mbrain_phase phase = UO_PHASE_INIT_ADSP;
	int ret = 0;

	if (uodev->adsp_inited) {
		USB_OFFLOAD_ERR("ADSP ALREADY INITED!!!\n");
		ret = -EBUSY;
		goto fail;
	}

	xhci_mem = kzalloc(sizeof(*xhci_mem), GFP_KERNEL);
	if (!xhci_mem) {
		USB_OFFLOAD_ERR("Fail to allocate xhci_mem\n");
		uo_mbrain_update(phase, UO_ERROR_INSUFFICIENT_SPACE);
		ret = -ENOMEM;
		goto fail;
	}

	/* init reserved region on main sram */
	if (mtk_offload_init_rsv(uodev, UO_PROV_SRAM)) {
		uodev->adv_lowpwr = false;
		USB_OFFLOAD_INFO("mode change, adv_lowpwr->%d\n", uodev->adv_lowpwr);
	}
	xhci_mem->adv_lowpwr = uodev->adv_lowpwr;

	/* fill in dram mpu region */
	xhci_mem->rsv_dram_size = mtk_offload_get_mpu_region(UO_PROV_DRAM, &xhci_mem->rsv_dram_addr);

	/* fill in main sram mpu region */
	xhci_mem->rsv_sram_size = mtk_offload_get_mpu_region(UO_PROV_SRAM, &xhci_mem->rsv_sram_addr);

	if (!xhci_mem->rsv_dram_addr || !xhci_mem->rsv_dram_size ||
		(xhci_mem->adv_lowpwr && (!xhci_mem->rsv_sram_size || !xhci_mem->rsv_sram_addr))) {
		USB_OFFLOAD_ERR("wrong mpu region\n");
		ret = -ENOMEM;
		goto fail;
	}

	USB_OFFLOAD_INFO("rsv-dram:0x%llx size:%d\n", xhci_mem->rsv_dram_addr, xhci_mem->rsv_dram_size);
	USB_OFFLOAD_INFO("rsv-sram:0x%llx size:%d\n", xhci_mem->rsv_sram_addr, xhci_mem->rsv_sram_size);

	if (uodev->adsp_ready)
		ret = usb_offload_send_ipi_msg(UOI_INIT_ADSP, xhci_mem, sizeof(struct mem_info_xhci));
	else {
		/* adsp was expected to be ready before sending ipi_msg(INIT_ADSP) */
		USB_OFFLOAD_ERR("adsp should be ready\n");
		ret = -EINVAL;
	}

	if (!ret) {
		uodev->adsp_inited = true;
		uo_mbrain_update(phase, UO_ERROR_SUCCESS);
	} else {
		mtk_offload_deinit_rsv(UO_PROV_SRAM);
		if (uodev->adv_lowpwr != uodev->policy.adv_lowpwr) {
			uodev->adv_lowpwr = uodev->policy.adv_lowpwr;
			USB_OFFLOAD_INFO("mode back, adv_lowpwr->%d\n", uodev->adv_lowpwr);
		}
	}

	kfree(xhci_mem);
fail:
	usb_offload_status();
	print_all_memory();
	return ret;
}

static int send_deinit_adsp(void)
{
	int ret;

	if (!uodev->adsp_inited) {
		USB_OFFLOAD_ERR("ADSP ALREADY DEINITED!!!!\n");
		ret = -EBUSY;
		goto fail;
	}

	if (!uodev->adsp_ready) {
		/* after ee triggered, we'll be requested to send ipi message (DEINIT_ADSP), although
		 * audio service has already died, we still need to deinit reserved sram region.
		 */
		ret = 0;
		USB_OFFLOAD_INFO("adsp wasn't ready, skip sending ipi message\n");
	} else
		ret = usb_offload_send_ipi_msg(UOI_DEINIT_ADSP, NULL, 0);

	if (ret < 0)
		USB_OFFLOAD_INFO("adsp deinit fail, rsv-region wasn't freed\n");
	else {
		uodev->adsp_inited = false;
		if (!mtk_offload_provider_get_cnt(UO_PROV_SRAM)) {
			mtk_offload_deinit_rsv(UO_PROV_SRAM);
			if (uodev->adv_lowpwr != uodev->policy.adv_lowpwr) {
				uodev->adv_lowpwr = uodev->policy.adv_lowpwr;
				USB_OFFLOAD_INFO("mode back, adv_lowpwr->%d\n", uodev->adv_lowpwr);
			}
		}
	}

fail:
	usb_offload_status();
	print_all_memory();
	return ret;
}

static int issue_audio_stream(enum usb_offload_ipi_msg ipi_type,
	struct usb_audio_stream_msg *msg, struct usb_audio_dev *dev,
	struct intf_info *info, int info_idx, bool must_success, bool skip_ipi)
{
	bool enable, cleanup;
	int ret;

	if (!dev || !info)
		return -EINVAL;

	if (!skip_ipi)
		ret = usb_offload_send_ipi_msg(ipi_type, msg, sizeof(struct usb_audio_stream_msg));
	else {
		USB_OFFLOAD_INFO("adsp wasn't ready, skip sending ipi message");
		ret = 0;
	}

	enable = msg->uainfo.enable;
	if (enable) {
		if (must_success && ret < 0)
			cleanup = true;
		else {
			cleanup = false;

			if (kref_read(&dev->kref) == 1)
				/* the first interface on device to start streaming */
				usb_offload_start_offloading(dev);

			msg->direction ? (uodev->rx_streaming = true) : (uodev->tx_streaming = true);
		}
	} else {
		if ((!must_success) || (must_success && (ret == 0 || ret != -ETIMEDOUT)))
			cleanup = true;
		else {
			USB_OFFLOAD_ERR("uac interface wasn't free\n");
			cleanup = false;
		}
	}

	USB_OFFLOAD_INFO("enable:%d ret:%d must_success:%d skip_ipi:%d cleanup:%d\n",
		enable, ret, must_success, skip_ipi, cleanup);

	if (cleanup)
		uaudio_dev_intf_release(dev, info_idx);

	usb_offload_status();
	print_all_memory();
	return ret;
}

static int send_enable_stream(struct usb_audio_stream_msg *msg, int info_idx)
{
	u8 card_num = msg->uainfo.pcm_card_num;
	struct usb_audio_dev *dev;
	struct intf_info *info;
	int retval;

	if (card_num >= SNDRV_CARDS || uadev[card_num].info == NULL
			|| info_idx < 0 || info_idx >= uadev[card_num].num_intf)
		return -EINVAL;

	dev = &uadev[card_num];
	info = &dev->info[info_idx];

	/* prepare for offloading */
	retval = uaudio_dev_intf_prepare(dev, info);
	if ( retval < 0)
		return retval;

	if (uodev->adsp_ready) {
		/* issue ENABLE_TRACE to dsp */
		usb_offload_trace_start(msg);

		/* issue ENABLE_STREAM to dsp */
		retval = issue_audio_stream(UOI_ENABLE_STREAM, msg, dev, info, info_idx, true, false);
	} else {
		/* adsp was expected to be ready before sending ipi_msg(ENABLE_STREAM) */
		USB_OFFLOAD_ERR("adsp should be ready\n");
		uaudio_dev_intf_lift(dev, info);
		retval = -EINVAL;
	}

	return retval;
}

static int send_disable_stream(u8 card_num,
	u8 direction, int info_idx, bool must_success)
{
	struct xhci_hcd *xhci = uodev->xhci;
	struct usb_audio_dev *dev;
	struct usb_audio_stream_msg msg = {
		.status = USB_AUDIO_STREAM_REQ_STOP,
		.direction = direction,
		.uainfo.pcm_card_num = card_num,
		.uainfo.enable = 0,
		.uainfo.direction = direction,
	};
	struct intf_info *info;
	bool run_disable = true, skip_ipi;
	int retval = 0, state;

	if (card_num >= SNDRV_CARDS || uadev[card_num].info == NULL
			|| info_idx < 0 || info_idx >= uadev[card_num].num_intf)
		return -EINVAL;

	dev = &uadev[card_num];
	info = &dev->info[info_idx];

	/* we might revice disable-request which trigger by several event
	 * ex plug-out event, HAL issuing ioctl etc. so synchornize first
	 */
	mutex_lock(&info->lock);
	USB_OFFLOAD_INFO("card_num:%d if_idx:%d (disable_sync:%d)\n",
		card_num, info_idx, atomic_read(&info->disable_sync));
	state = atomic_read(&info->disable_sync);
	switch (state) {

	/* we're the first one to run */
	case NOT_DISABLE:
		atomic_set(&info->disable_sync, ON_DISABLE);
		break;

	/* others was running, we should wait */
	case ON_DISABLE:

		retval = wait_event_timeout(dev->disabling_wq,
			(atomic_read(&info->disable_sync) == DONE_DISABLE),
			msecs_to_jiffies(DISABLE_WAIT_TIME));
		if (!retval)
			USB_OFFLOAD_ERR("timeout while waiting disabling");
		else
			USB_OFFLOAD_INFO("success to wait\n");
		fallthrough;

	/* others have already done before */
	case DONE_DISABLE:
		run_disable = false;
		break;

	/* weird state.... it shouldn't be here */
	default:
		USB_OFFLOAD_ERR("unknown state:%d\n", state);
		mutex_unlock(&info->lock);
		return -EFAULT;
	}
	mutex_unlock(&info->lock);

	USB_OFFLOAD_INFO("run_disabled:%d\n", run_disable);
	if (!run_disable)
		return 0;

	USB_OFFLOAD_INFO("xhc state:0x%x\n", xhci->xhc_state);
	if ((xhci->xhc_state & XHCI_STATE_DYING) || (xhci->xhc_state & XHCI_STATE_HALTED)) {
		USB_OFFLOAD_INFO("xhci halted or dying\n");
		msg.flag |= STREAM_FLAG_XHCI_HALT;
	} else
		/* lift from offloading */
		uaudio_dev_intf_lift(dev, info);

	/* after ee triggered, it's requested to send ipi_msg(DISABLE_STREAM)
	 * although audio service has already died, we still need to cleanup
	 * audio interface and tracer which we had enabled before.
	 */
	skip_ipi = !uodev->adsp_ready;

	/* issue DISABLE_TRACE to dsp */
	usb_offload_trace_stop(msg.direction, skip_ipi);

	/* issue DISABLE_STREAM to dsp */
	retval = issue_audio_stream(UOI_DISABLE_STREAM, &msg,
		dev, info, info_idx, must_success, skip_ipi);

	/* notify others that we've done */
	atomic_set(&info->disable_sync, DONE_DISABLE);
	wake_up(&dev->disabling_wq);

	return retval;
}

static int info_idx_from_ifnum(unsigned int card_num, int intf_num, bool enable)
{
	int i;

	USB_OFFLOAD_DBG("enable:%d, card_num:%d, intf_num:%d\n",
			enable, card_num, intf_num);

	/*
	 * default index 0 is used when info is allocated upon
	 * first enable audio stream req for a pcm device
	 */
	if (enable && !uadev[card_num].info) {
		USB_OFFLOAD_MEM_DBG("enable:%d, uadev[%d].info:%p\n",
				enable, card_num, uadev[card_num].info);
		return 0;
	}

	USB_OFFLOAD_DBG("num_intf:%d\n", uadev[card_num].num_intf);

	for (i = 0; i < uadev[card_num].num_intf; i++) {
		if (enable && !uadev[card_num].info[i].in_use)
			return i;
		else if (!enable && uadev[card_num].info[i].intf_num == intf_num)
			return i;
	}

	return -EINVAL;
}

static int get_data_interval_from_si(struct snd_usb_substream *subs,
		u32 service_interval)
{
	unsigned int bus_intval, bus_intval_mult, binterval;

	if (subs->dev->speed >= USB_SPEED_HIGH)
		bus_intval = BUS_INTERVAL_HIGHSPEED_AND_ABOVE;
	else
		bus_intval = BUS_INTERVAL_FULL_SPEED;

	if (service_interval % bus_intval)
		return -EINVAL;

	bus_intval_mult = service_interval / bus_intval;
	binterval = ffs(bus_intval_mult);
	if (!binterval || binterval > MAX_BINTERVAL_ISOC_EP)
		return -EINVAL;

	/* check if another bit is set then bail out */
	bus_intval_mult = bus_intval_mult >> binterval;
	if (bus_intval_mult)
		return -EINVAL;

	return (binterval - 1);
}

static void *find_csint_desc(unsigned char *descstart, int desclen, u8 dsubtype)
{
	u8 *p, *end, *next;

	p = descstart;
	end = p + desclen;
	while (p < end) {
		if (p[0] < 2)
			return NULL;
		next = p + p[0];
		if (next > end)
			return NULL;
		if (p[1] == USB_DT_CS_INTERFACE && p[2] == dsubtype)
			return p;
		p = next;
	}
	return NULL;
}

static int usb_offload_prepare_msg(struct snd_usb_substream *subs,
		struct usb_audio_stream_info *uainfo,
		struct usb_audio_stream_msg *msg,
		struct usb_audio_dev *audio_dev,
		int info_idx)
{
	struct usb_interface *iface;
	struct usb_host_interface *alts;
	struct usb_interface_descriptor *altsd;
	struct usb_interface_assoc_descriptor *assoc;
	struct usb_host_endpoint *ep;
	struct uac_format_type_i_continuous_descriptor *fmt;
	struct uac_format_type_i_discrete_descriptor *fmt_v1;
	struct uac_format_type_i_ext_descriptor *fmt_v2;
	struct uac1_as_header_descriptor *as;
	int ret;
	unsigned int protocol, card_num, pcm_dev_num;
	int interface, altset_idx;
	void *hdr_ptr;
	unsigned int data_ep_pipe = 0, sync_ep_pipe = 0;

	if (!subs || !audio_dev) {
		ret = -ENODEV;
		goto err;
	}

	if (subs->cur_audiofmt == NULL) {
		USB_OFFLOAD_ERR("substream->cur_audio_fmt is NULL!\n");
		ret = -ENODEV;
		goto err;
	}
	interface = subs->cur_audiofmt->iface;
	altset_idx = subs->cur_audiofmt->altset_idx;

	iface = usb_ifnum_to_if(subs->dev, interface);
	if (!iface) {
		USB_OFFLOAD_ERR("interface # %d does not exist\n", interface);
		ret = -ENODEV;
		goto err;
	}

	msg->uainfo = *uainfo;

	assoc = iface->intf_assoc;
	pcm_dev_num = uainfo->pcm_dev_num;
	card_num = uainfo->pcm_card_num;

	msg->direction = uainfo->direction;

	alts = &iface->altsetting[altset_idx];
	altsd = get_iface_desc(alts);
	protocol = altsd->bInterfaceProtocol;

	/* get format type */
	if (protocol != UAC_VERSION_3) {
		fmt = find_csint_desc(alts->extra, alts->extralen,
				UAC_FORMAT_TYPE);
		if (!fmt) {
			USB_OFFLOAD_ERR("%u:%d : no UAC_FORMAT_TYPE desc\n",
					interface, altset_idx);
			ret = -ENODEV;
			goto err;
		}
	}

	if (!audio_dev->ctrl_intf) {
		USB_OFFLOAD_ERR("audio ctrl intf info not cached\n");
		ret = -ENODEV;
		goto err;
	}

	if (protocol != UAC_VERSION_3) {
		hdr_ptr = find_csint_desc(audio_dev->ctrl_intf->extra,
				audio_dev->ctrl_intf->extralen,
				UAC_HEADER);
		if (!hdr_ptr) {
			USB_OFFLOAD_ERR("no UAC_HEADER desc\n");
			ret = -ENODEV;
			goto err;
		}
	}

	if (protocol == UAC_VERSION_1) {
		struct uac1_ac_header_descriptor *uac1_hdr = hdr_ptr;

		as = find_csint_desc(alts->extra, alts->extralen,
			UAC_AS_GENERAL);
		if (!as) {
			USB_OFFLOAD_ERR("%u:%d : no UAC_AS_GENERAL desc\n",
					interface, altset_idx);
			ret = -ENODEV;
			goto err;
		}
		msg->data_path_delay = as->bDelay;
		fmt_v1 = (struct uac_format_type_i_discrete_descriptor *)fmt;
		msg->usb_audio_subslot_size = fmt_v1->bSubframeSize;

		msg->usb_audio_spec_revision = le16_to_cpu(uac1_hdr->bcdADC);
	} else if (protocol == UAC_VERSION_2) {
		struct uac2_ac_header_descriptor *uac2_hdr = hdr_ptr;

		fmt_v2 = (struct uac_format_type_i_ext_descriptor *)fmt;
		msg->usb_audio_subslot_size = fmt_v2->bSubslotSize;

		msg->usb_audio_spec_revision = le16_to_cpu(uac2_hdr->bcdADC);
	} else if (protocol == UAC_VERSION_3) {
		if (assoc->bFunctionSubClass ==
					UAC3_FUNCTION_SUBCLASS_FULL_ADC_3_0) {
			USB_OFFLOAD_ERR("full adc is not supported\n");
			ret = -EINVAL;
		}

		switch (le16_to_cpu(get_endpoint(alts, 0)->wMaxPacketSize)) {
		case UAC3_BADD_EP_MAXPSIZE_SYNC_MONO_16:
		case UAC3_BADD_EP_MAXPSIZE_SYNC_STEREO_16:
		case UAC3_BADD_EP_MAXPSIZE_ASYNC_MONO_16:
		case UAC3_BADD_EP_MAXPSIZE_ASYNC_STEREO_16: {
			msg->usb_audio_subslot_size = 0x2;
			break;
		}

		case UAC3_BADD_EP_MAXPSIZE_SYNC_MONO_24:
		case UAC3_BADD_EP_MAXPSIZE_SYNC_STEREO_24:
		case UAC3_BADD_EP_MAXPSIZE_ASYNC_MONO_24:
		case UAC3_BADD_EP_MAXPSIZE_ASYNC_STEREO_24: {
			msg->usb_audio_subslot_size = 0x3;
			break;
		}

		default:
			USB_OFFLOAD_ERR("%d: %u: Invalid wMaxPacketSize\n",
					interface, altset_idx);
			ret = -EINVAL;
			goto err;
		}
	} else {
		USB_OFFLOAD_ERR("unknown protocol version %x\n", protocol);
		ret = -ENODEV;
		goto err;
	}

	msg->slot_id = subs->dev->slot_id;

	memcpy(&msg->std_as_opr_intf_desc, &alts->desc, sizeof(alts->desc));

	/* data endpoint */
	ep = usb_pipe_endpoint(subs->dev, subs->data_endpoint->pipe);
	if (!ep) {
		USB_OFFLOAD_ERR("data ep # %d context is null\n",
				subs->data_endpoint->ep_num);
		ret = -ENODEV;
		goto err;
	}
	data_ep_pipe = subs->data_endpoint->pipe;
	memcpy(&msg->data_ep_info.desc, &ep->desc, sizeof(ep->desc));
	msg->flag |= STREAM_FLAG_DATA_EP;

	/* sync endpoint*/
	if (subs->sync_endpoint) {
		ep = usb_pipe_endpoint(subs->dev, subs->sync_endpoint->pipe);
		if (!ep) {
			USB_OFFLOAD_ERR("implicit fb on data ep\n");
			goto skip_sync_ep;
		}
		sync_ep_pipe = subs->sync_endpoint->pipe;
		memcpy(&msg->sync_ep_info.desc, &ep->desc, sizeof(ep->desc));
		msg->flag |= STREAM_FLAG_SYNC_EP;
	}

skip_sync_ep:
	msg->speed_info = get_speed_info(subs->dev->speed);
	if (msg->speed_info == USB_AUDIO_DEVICE_SPEED_INVALID) {
		ret = -ENODEV;
		goto err;
	}

	if (!atomic_read(&audio_dev->in_use) && uodev->last_card_num >= 0) {
		USB_OFFLOAD_ERR("not support multi device offloading, last:%d card_num:%d\n",
			uodev->last_card_num, card_num);
		return -ENODEV;
	}

	if (!atomic_read(&audio_dev->in_use)) {
		/* sideband */
		if (uaudio_dev_create_sideband(audio_dev) < 0)
			return -ENODEV;

		/* interrupter */
		if (xhci_sideband_create_interrupter_(audio_dev->sb, 1,
			true, uodev->xhci->imod_interval, XHCI1_INTR_TARGET) < 0) {
			USB_OFFLOAD_ERR("fail to create interrupter%d\n", XHCI1_INTR_TARGET);
			return -ENODEV;
		}
		uodev->ir = audio_dev->sb->ir;

		/* setup audio device */
		kref_init(&audio_dev->kref);
		init_waitqueue_head(&audio_dev->disabling_wq);
		audio_dev->num_intf = subs->dev->config->desc.bNumInterfaces;
		audio_dev->info = kcalloc(audio_dev->num_intf, sizeof(struct intf_info), GFP_KERNEL);
		if (!audio_dev->info) {
			xhci_sideband_remove_interrupter_(audio_dev->sb);
			uaudio_dev_remove_sideband(audio_dev);
			audio_dev->sb = NULL;
			return -ENOMEM;
		}
		audio_dev->udev = subs->dev;
		audio_dev->card_num = card_num;
		atomic_set(&audio_dev->in_use, 1);
	} else
		kref_get(&audio_dev->kref);

	USB_OFFLOAD_INFO("card_num:%d info_idx:%d num_intf:%d kref:%d\n",
		audio_dev->card_num, info_idx, audio_dev->num_intf, kref_read(&audio_dev->kref));

	/* setup audio interface */
	audio_dev->info[info_idx].data_ep_pipe = data_ep_pipe;
	audio_dev->info[info_idx].sync_ep_pipe = sync_ep_pipe;
	audio_dev->info[info_idx].pcm_card_num = card_num;
	audio_dev->info[info_idx].pcm_dev_num = pcm_dev_num;
	audio_dev->info[info_idx].direction = subs->direction;
	audio_dev->info[info_idx].intf_num = interface;
	audio_dev->info[info_idx].in_use = true;
	mutex_init(&audio_dev->info[info_idx].lock);
	atomic_set(&audio_dev->info[info_idx].disable_sync, NOT_DISABLE);

	return 0;
err:
	return ret;
}

struct urb_information {
	unsigned int align_size;
	unsigned int urb_size;
	unsigned int urb_num;
	unsigned int urb_packs;
};

static unsigned int get_usb_speed_rate(bool is_full_speed, unsigned int rate)
{
	if (is_full_speed)
		return ((rate << 13) + 62) / 125;
	else
		return ((rate << 10) + 62) / 125;
}

static struct urb_information mtk_usb_offload_calculate_urb(
	struct usb_audio_stream_info *uainfo, struct snd_usb_substream *subs)
{
	struct urb_information urb_info;
	struct snd_usb_endpoint *ep = subs->data_endpoint;
	struct snd_usb_audio *chip = ep->chip;
	unsigned int freqn, freqmax;
	unsigned int maxsize, packs_per_ms, max_packs_per_urb, urb_packs, nurbs;
	unsigned int buffer_size;
	unsigned long long align = 64 - 1;
	int frame_bits, packets;

	frame_bits = uainfo->bit_depth * uainfo->number_of_ch;
	freqn = get_usb_speed_rate(
		get_speed_info(subs->dev->speed) == USB_AUDIO_DEVICE_SPEED_FULL,
		uainfo->bit_rate);
	freqmax = freqn + (freqn >> 1);
	maxsize = (((freqmax << ep->datainterval) + 0xffff) >> 16) * (frame_bits >> 3);

	if (ep->maxpacksize && ep->maxpacksize < maxsize) {
		unsigned int pre_maxsize = maxsize, pre_freqmax = freqmax;
		unsigned int data_maxsize = maxsize = ep->maxpacksize;

		freqmax = (data_maxsize / (frame_bits >> 3)) << (16 - ep->datainterval);
		USB_OFFLOAD_INFO("maxsize:%d->%d freqmax:%d->%d\n",
			pre_maxsize, maxsize, pre_freqmax, freqmax);
	}

	if (chip->dev->speed != USB_SPEED_FULL) {
		packs_per_ms = 8 >> ep->datainterval;
		max_packs_per_urb = MAX_PACKS_HS;
	} else {
		packs_per_ms = 1;
		max_packs_per_urb = MAX_PACKS;
	}
	max_packs_per_urb = max(1u, max_packs_per_urb >> ep->datainterval);

	if (usb_pipein(ep->pipe)) {
		urb_packs = packs_per_ms;
		urb_packs = min(max_packs_per_urb, urb_packs);

		while (urb_packs > 1 && urb_packs * maxsize >= uainfo->pcm_size)
			urb_packs >>= 1;

		if (uainfo->xhc_irq_period_ms * uainfo->xhc_urb_num * packs_per_ms
			> USB_OFFLOAD_TRBS_PER_SEGMENT) {
			urb_packs = uainfo->xhc_irq_period_ms * packs_per_ms;
			nurbs = USB_OFFLOAD_TRBS_PER_SEGMENT / urb_packs;
		} else {
			urb_packs = uainfo->xhc_irq_period_ms * packs_per_ms;
			nurbs = uainfo->xhc_urb_num;
		}
	} else {
		nurbs = uainfo->xhc_urb_num;
		urb_packs = uainfo->xhc_irq_period_ms * packs_per_ms;
	}

	packets = urb_packs;
	buffer_size = maxsize * packets;

	USB_OFFLOAD_INFO(
		"maxsz:%d frame_bits:%d freqmax:%d freqn:%d intval:%d urb_pkt:%d bufsz:%d maxpktsz:%d",
		maxsize, frame_bits, freqmax, freqn, ep->datainterval,
		packets, buffer_size, ep->maxpacksize);

	urb_info.urb_size = buffer_size;
	urb_info.urb_num = nurbs;
	urb_info.urb_packs = packets;
	urb_info.align_size = buffer_size + align;

	return urb_info;
}

/* allocate urb and 2nd segment in one time */
static int usb_offload_prepare_msg_ext(
	struct usb_audio_stream_msg *msg, struct usb_audio_stream_info *uainfo,
	struct snd_usb_substream *subs, struct intf_info *info)
{
	struct urb_information urb_info;
	struct usb_host_endpoint *ep;
	struct xhci_ring *ring;
	unsigned int total_size;
	unsigned long long align = 64 - 1;
	unsigned int slot_id, ep_id;
	dma_addr_t phy_addr;
	void *vir_addr;
	int ret, i;
	bool expend_tr, has_sync_ep = false;
	enum uo_provider_type type;

	info->dsp_urb = uob_get_empty(UO_STRUCT_URB);
	if (!info->dsp_urb) {
		USB_OFFLOAD_ERR("insufficent space on %s array (dir:%s)\n",
			uo_struct_name(UO_STRUCT_URB), uainfo->direction == 1 ? "in" : "out");
		return -ENOMEM;
	}

	/* calculate urb size of data endpoint */
	urb_info = mtk_usb_offload_calculate_urb(uainfo, subs);
	USB_OFFLOAD_MEM_DBG("[urb_info] align_size:%d size:%d nurbs:%d packs:%d\n",
		urb_info.align_size, urb_info.urb_size, urb_info.urb_num, urb_info.urb_packs);
	total_size = urb_info.align_size * urb_info.urb_num;

	/* check if it needs 2nd segment */
	expend_tr = get_speed_info(subs->dev->speed) > USB_AUDIO_DEVICE_SPEED_FULL &&
		uainfo->xhc_irq_period_ms == 20 ? true : false;
	if (expend_tr)
		total_size += USB_OFFLOAD_TRB_SEGMENT_SIZE + align;

	/* check if there's sync ep */
	if (msg->flag & STREAM_FLAG_SYNC_EP) {
		has_sync_ep = true;
		total_size += (SYNC_URBS * (4 + align));
	}

	USB_OFFLOAD_MEM_DBG("total_size:%d direction:%d (has_sync_ep:%d expand_tr:%d)\n",
		total_size, uainfo->direction, has_sync_ep, expend_tr);

	type = usb_offload_mem_type_lp_ex(uainfo->direction);

	/* requeset for memory (urbs + 2nd segment)*/
	ret = mtk_offload_alloc_mem(info->dsp_urb, total_size, USB_OFFLOAD_TRB_SEGMENT_SIZE,
				type, UO_STRUCT_URB, false);
	if (ret != 0)
		return ret;

	/* assign memory for 2nd segment */
	phy_addr = info->dsp_urb->phys;
	if (expend_tr) {
		slot_id = subs->dev->slot_id;
		ep = usb_pipe_endpoint(subs->dev, subs->data_endpoint->pipe);
		if (ep) {
			ep_id = xhci_get_endpoint_index_(&ep->desc);
			ring = uodev->xhci->devs[slot_id]->eps[ep_id].ring;
			phy_addr = (phy_addr + align) & (~align);
			vir_addr = (void *)((u64)info->dsp_urb->virt + (u64)(phy_addr - info->dsp_urb->phys));
			xhci_mtk_ring_expansion(uodev->xhci, ring, phy_addr, vir_addr);

			phy_addr = info->dsp_urb->phys + USB_OFFLOAD_TRB_SEGMENT_SIZE;
		}
	}

	/* assign memory for sync ep's urbs */
	/* always 4 urbs for sync and length=4 for each */
	if (has_sync_ep) {
		phy_addr = (phy_addr + align) & (~align);
		msg->sync_ep_info.urb_start_addr = (unsigned long long)phy_addr;
		msg->sync_ep_info.urb_size = 4;
		msg->sync_ep_info.urb_num = SYNC_URBS;
		msg->sync_ep_info.urb_packs = 1;

		for (i = 0; i < msg->sync_ep_info.urb_num; i++) {
			USB_OFFLOAD_INFO("[sync urb%d] phys:0x%llx size:%d\n",
				i, phy_addr, msg->sync_ep_info.urb_size);
			phy_addr += msg->sync_ep_info.urb_size;
			phy_addr = (phy_addr + align) & (~align);
		}
	}

	/* assign memory for data ep's urbs */
	phy_addr = (phy_addr + align) & (~align);
	msg->data_ep_info.urb_start_addr = (unsigned long long)phy_addr;
	msg->data_ep_info.urb_size = urb_info.urb_size;
	msg->data_ep_info.urb_num = urb_info.urb_num;
	msg->data_ep_info.urb_packs = urb_info.urb_packs;

	for (i = 0; i < msg->data_ep_info.urb_num; i++) {
		USB_OFFLOAD_INFO("[urb(%s)%d] phys:0x%llx size:%d\n",
			uainfo->direction == SNDRV_PCM_STREAM_CAPTURE ? "in" : "out",
			i, phy_addr, msg->data_ep_info.urb_size);

		phy_addr += msg->data_ep_info.urb_size;
		phy_addr = (phy_addr + align) & (~align);
	}

	return 0;
}

static int handle_enable_stream(struct usb_audio_stream_info *uainfo,
	struct snd_usb_substream *subs, struct usb_audio_dev *audio_dev,
	int info_idx)
{
	struct usb_audio_stream_msg msg = {0};
	struct snd_usb_audio *chip = NULL;
	struct intf_info *audio_intf;
	u8 pcm_dev_num, direction;
	int ret = 0;

	direction = uainfo->direction;
	pcm_dev_num = uainfo->pcm_dev_num;

	chip = audio_dev->chip;
	if (!subs || !chip || atomic_read(&chip->shutdown)) {
		USB_OFFLOAD_ERR("can't find substream for card# %u, dev# %u, dir: %u\n",
				audio_dev->card_num, pcm_dev_num, direction);
		return -ENODEV;
	}

	if (atomic_read(&chip->shutdown) || !subs->stream || !subs->stream->pcm
			|| !subs->stream->chip) {
		ret = -ENODEV;
		goto error;
	}

	USB_OFFLOAD_INFO("subs:%p udev:%p card_num:%d info_idx:%d dir:%d\n",
		subs, subs->dev, audio_dev->card_num, info_idx, direction);

	if (uainfo->service_interval_valid) {
		ret = get_data_interval_from_si(subs, uainfo->service_interval);
		if (ret == -EINVAL) {
			USB_OFFLOAD_ERR("invalid service interval %u\n",
					uainfo->service_interval);
			goto error;
		}
	}

	audio_dev->ctrl_intf = chip->ctrl_intf;
	if (uainfo->enable) {
		ret = usb_offload_prepare_msg(subs, uainfo, &msg, audio_dev, info_idx);
		if (ret < 0) {
			USB_OFFLOAD_ERR("fail to prepare message, ret:%d\n", ret);
			goto error;
		}

		audio_intf = &audio_dev->info[info_idx];
		ret = usb_offload_prepare_msg_ext(&msg, uainfo, subs, audio_intf);
		if (ret < 0) {
			USB_OFFLOAD_ERR("fail to prepare message ext, ret:%d\n", ret);
			uo_mbrain_update(UO_PHASE_ENABLE_STREAM, UO_ERROR_ALLOC_URB_FAIL);
			goto error;
		}

		msg.status = USB_AUDIO_STREAM_REQ_START;
		ret = send_enable_stream(&msg, info_idx);

	} else {
		audio_intf = &audio_dev->info[info_idx];
		ret = send_disable_stream(audio_dev->card_num, direction, info_idx, true);
	}

error:
	return ret;
}

static bool xhci_mtk_is_usb_offload_enabled(struct xhci_hcd *xhci,
					   struct xhci_virt_device *vdev,
					   unsigned int ep_index)
{
	return uodev->policy.ready_for_xhci;
}

static struct xhci_device_context_array *xhci_mtk_alloc_dcbaa(struct xhci_hcd *xhci,
						 gfp_t flags)
{
	struct xhci_device_context_array *xhci_ctx;
	struct uo_buffer *buf;

	if (uob_init(UO_STRUCT_DCBAA)) {
		USB_OFFLOAD_ERR("fail to init %s array\n", uo_struct_name(UO_STRUCT_DCBAA));
		return NULL;
	}

	buf = uob_get_empty(UO_STRUCT_DCBAA);
	if (!buf) {
		USB_OFFLOAD_ERR("insufficent on %s array\n", uo_struct_name(UO_STRUCT_DCBAA));
		return NULL;
	}

	if (mtk_offload_alloc_mem(buf, sizeof(*xhci_ctx), 64,
			usb_offload_mem_type(), UO_STRUCT_DCBAA, true)) {
		USB_OFFLOAD_ERR("fail to allocate dcbaa\n");
		return NULL;
	}

	xhci_ctx = (struct xhci_device_context_array *) buf->virt;
	xhci_ctx->dma = buf->phys;

	if (uob_init(UO_STRUCT_CTX))
		USB_OFFLOAD_ERR("fail to init %s array\n", uo_struct_name(UO_STRUCT_CTX));

	return xhci_ctx;
}

static void xhci_mtk_free_dcbaa(struct xhci_hcd *xhci)
{
	struct uo_buffer *buf;

	buf = uob_get_first(UO_STRUCT_DCBAA);
	if (!buf) {
		USB_OFFLOAD_ERR("DCBAA has not been initialized.\n");
		return;
	}

	if (mtk_offload_free_mem(buf))
		USB_OFFLOAD_ERR("fail to free dcbaa\n");

	uob_deinit(UO_STRUCT_CTX);
	uob_deinit(UO_STRUCT_DCBAA);
}

static void xhci_mtk_alloc_container_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx,
				int type, gfp_t flags)
{
	struct uo_buffer *buf = uob_get_empty(UO_STRUCT_CTX);

	USB_OFFLOAD_INFO("%s context (%d)\n", type == XHCI_CTX_TYPE_INPUT ? "input" : "output", type);

	if (!buf) {
		USB_OFFLOAD_ERR("insufficent on %s array\n", uo_struct_name(UO_STRUCT_CTX));
		goto error;
	}

	if (mtk_offload_alloc_mem(buf, ctx->size, 64,
			usb_offload_mem_type(), UO_STRUCT_CTX, true)) {
		USB_OFFLOAD_ERR("fail to allocate context\n");
		goto error;
	}

	ctx->bytes = buf->virt;
	ctx->dma = buf->phys;
	return;
error:
	ctx->bytes = NULL;
	ctx->dma = 0;
}

static void xhci_mtk_free_container_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx)
{
	struct uo_buffer *buf = uob_search(UO_STRUCT_CTX, ctx->dma);

	if (buf) {
		if (mtk_offload_free_mem(buf))
			USB_OFFLOAD_ERR("fail to free context\n");
	} else
		USB_OFFLOAD_ERR("context(vir:%p phys:0x%llx) isn't under managed\n",
			ctx->bytes,ctx->dma);
}

static int xhci_mtk_free_ring(struct xhci_ring *ring, enum uo_struct type)
{
	struct xhci_segment *seg;
	struct uo_buffer *buf;

	if (!ring || !ring->first_seg)
		return 0;

	seg = ring->first_seg;
	buf = uob_search(type, seg->dma);
	if (buf) {
		if (seg->trbs) {
			if (mtk_offload_free_mem(buf))
				USB_OFFLOAD_ERR("fail to %s ring\n",
					type == UO_STRUCT_EVRING ? "ev" : "tr");
			seg->trbs = NULL;
		}
		kfree(seg->bounce_buf);
		kfree(seg);
		kfree(ring);
		return 0;
	} else
		return -EINVAL;
}

static void xhci_mtk_usb_offload_segment_free(struct xhci_hcd *xhci,
			struct xhci_segment *seg, enum xhci_ring_type ring_type)
{
	enum uo_struct type;
	struct uo_buffer *buf;

	if (seg->trbs) {
		type = ring_type == TYPE_EVENT ? UO_STRUCT_EVRING : UO_STRUCT_TRRING;
		buf = uob_search(type, seg->dma);
		if (buf && mtk_offload_free_mem(buf))
			USB_OFFLOAD_ERR("fail to ring segment\n");
		seg->trbs = NULL;
	}

	kfree(seg->bounce_buf);
	kfree(seg);
}

static struct xhci_segment *xhci_mtk_usb_offload_segment_alloc(struct xhci_hcd *xhci,
						   unsigned int cycle_state,
						   unsigned int max_packet,
						   gfp_t flags,
						   enum uo_provider_type p_type,
						   bool is_rsv,
						   enum xhci_ring_type type)
{
	struct xhci_segment *seg;
	dma_addr_t	dma;
	int	i;
	struct uo_buffer *buf;
	enum uo_struct struct_type;

	struct_type = type == TYPE_EVENT ? UO_STRUCT_EVRING : UO_STRUCT_TRRING;
	buf = uob_get_empty(struct_type);
	if (!buf) {
		USB_OFFLOAD_ERR("insufficent buffer on %s array\n", uo_struct_name(struct_type));
		return NULL;
	}

	seg = kzalloc(sizeof(*seg), flags);
	if (!seg)
		return NULL;

	if (mtk_offload_alloc_mem(buf, USB_OFFLOAD_TRB_SEGMENT_SIZE,
			USB_OFFLOAD_TRB_SEGMENT_SIZE, p_type, struct_type, is_rsv)) {
		USB_OFFLOAD_ERR("fail to allocate %s ring\n", type == TYPE_EVENT ? "ev" : "tr");
		kfree(seg);
		return NULL;
	}

	seg->trbs = buf->virt;
	seg->dma = 0;
	dma = buf->phys;

	if (!seg->trbs) {
		USB_OFFLOAD_ERR("No seg->trbs\n");
		kfree(seg);
		return NULL;
	}

	if (max_packet) {
		seg->bounce_buf = kzalloc(max_packet, flags);
		if (!seg->bounce_buf) {
			xhci_mtk_usb_offload_segment_free(xhci, seg, type);
			return NULL;
		}
	}
	/* If the cycle state is 0, set the cycle bit to 1 for all the TRBs */
	if (cycle_state == 0) {
		for (i = 0; i < USB_OFFLOAD_TRBS_PER_SEGMENT; i++)
			seg->trbs[i].link.control |= cpu_to_le32(TRB_CYCLE);
	}
	seg->dma = dma;
	seg->next = NULL;

	return seg;
}

/* Allocate segments and link them for a ring */
static int xhci_mtk_usb_offload_alloc_segments_for_ring(struct xhci_hcd *xhci,
		struct xhci_segment **first, struct xhci_segment **last,
		unsigned int num_segs, unsigned int num, unsigned int cycle_state,
		enum xhci_ring_type type, unsigned int max_packet, gfp_t flags,
		enum uo_provider_type p_type, bool is_rsv)
{
	struct xhci_segment *prev;
	bool chain_links;

	/* Set chain bit for 0.95 hosts, and for isoc rings on AMD 0.96 host */
	chain_links = !!(xhci_link_trb_quirk(xhci) ||
			 (type == TYPE_ISOC &&
			  (xhci->quirks & XHCI_AMD_0x96_HOST)));

	prev = xhci_mtk_usb_offload_segment_alloc(xhci, cycle_state, max_packet, flags,
			p_type, is_rsv, type);
	if (!prev)
		return -ENOMEM;
	num++;

	*first = prev;
	while (num < num_segs) {
		struct xhci_segment	*next;

		next = xhci_mtk_usb_offload_segment_alloc(
					xhci, cycle_state, max_packet, flags, p_type, is_rsv, type);
		if (!next) {
			prev = *first;
			while (prev) {
				next = prev->next;
				xhci_mtk_usb_offload_segment_free(xhci, prev, type);
				prev = next;
			}
			return -ENOMEM;
		}
		xhci_link_segments_(prev, next, type, chain_links);
		prev = next;
		num++;
	}
	xhci_link_segments_(prev, *first, type, chain_links);
	*last = prev;
	return 0;
}

static void xhci_mtk_link_rings(struct xhci_hcd *xhci, struct xhci_ring *ring,
	struct xhci_segment *first, struct xhci_segment *last, unsigned int num_segs)
{
	struct xhci_segment *next;
	bool chain_links;

	chain_links = !!(xhci_link_trb_quirk(xhci) ||
			 (ring->type == TYPE_ISOC &&
			  (xhci->quirks & XHCI_AMD_0x96_HOST)));

	next = ring->enq_seg->next;
	xhci_link_segments_(ring->enq_seg, first, ring->type, chain_links);
	xhci_link_segments_(last, next, ring->type, chain_links);
	ring->num_segs += num_segs;
	ring->num_trbs_free += (TRBS_PER_SEGMENT - 1) * num_segs;

	if (ring->type != TYPE_EVENT && ring->enq_seg == ring->last_seg) {
		ring->last_seg->trbs[TRBS_PER_SEGMENT-1].link.control
			&= ~cpu_to_le32(LINK_TOGGLE);
		last->trbs[TRBS_PER_SEGMENT-1].link.control
			|= cpu_to_le32(LINK_TOGGLE);
		ring->last_seg = last;
	}
}

static int xhci_mtk_ring_expansion(struct xhci_hcd *xhci,
	struct xhci_ring *ring, dma_addr_t phys, void *vir)
{
	struct xhci_segment *new_seg, *seg;
	unsigned int num_segs = 1, max_packet, i;
	bool chain_links;

	if (!ring)
		return -EINVAL;

	chain_links = !!(xhci_link_trb_quirk(xhci) ||
			 (ring->type == TYPE_ISOC &&
			  (xhci->quirks & XHCI_AMD_0x96_HOST)));

	USB_OFFLOAD_INFO("phys:0x%llx vir:%p\n", (u64)phys, vir);

	new_seg = kzalloc(sizeof(*new_seg), GFP_ATOMIC);
	if (!new_seg)
		return -ENOMEM;

	new_seg->trbs = vir;
	new_seg->dma = phys;

	USB_OFFLOAD_INFO("vir:%p phy:0x%llx\n",
		new_seg->trbs, (u64)new_seg->dma);

	if (!new_seg->trbs) {
		USB_OFFLOAD_ERR("No seg->trbs\n");
		kfree(new_seg);
		return -EINVAL;
	}

	max_packet = ring->bounce_buf_len;
	if (max_packet) {
		new_seg->bounce_buf = kzalloc(max_packet, GFP_ATOMIC);
		if (!new_seg->bounce_buf) {
			kfree(new_seg);
			return -ENOMEM;
		}
	}

	if (ring->cycle_state == 0) {
		for (i = 0; i < USB_OFFLOAD_TRBS_PER_SEGMENT; i++)
			new_seg->trbs[i].link.control |= cpu_to_le32(TRB_CYCLE);
	}
	new_seg->next = NULL;

	xhci_link_segments_(new_seg, new_seg, ring->type, chain_links);

	xhci_mtk_link_rings(xhci, ring, new_seg, new_seg, num_segs);

	seg = ring->first_seg;
	for (i = 0; i < ring->num_segs; i++) {
		USB_OFFLOAD_INFO("[seg%d] vir:%p phy:0x%llx point_to:0x%llx\n",
			i, seg->trbs, (u64)seg->dma,
			seg->trbs[USB_OFFLOAD_TRBS_PER_SEGMENT - 1].link.segment_ptr);
		seg = seg->next;
	}

	return 0;
}

static struct xhci_ring *xhci_mtk_alloc_ring(struct xhci_hcd *xhci,
		int num_segs, int cycle_state, enum xhci_ring_type ring_type,
		unsigned int max_packet, gfp_t mem_flags,
		enum uo_provider_type p_type, bool is_rsv)
{
	struct xhci_ring	*ring;
	int ret;

	ring = kzalloc(sizeof(*ring), mem_flags);
	if (!ring)
		return NULL;

	ring->num_segs = num_segs;
	ring->bounce_buf_len = max_packet;
	INIT_LIST_HEAD(&ring->td_list);
	ring->type = ring_type;

	ret = xhci_mtk_usb_offload_alloc_segments_for_ring(xhci, &ring->first_seg,
			&ring->last_seg, num_segs, 0, cycle_state, ring_type,
			max_packet, mem_flags, p_type, is_rsv);
	if (ret) {
		USB_OFFLOAD_ERR("Fail to alloc segment for rings (mem_id:%d)\n", p_type);
		goto fail;
	}

	if (ring_type != TYPE_EVENT) {
		/* See section 4.9.2.1 and 6.4.4.1 */
		ring->last_seg->trbs[USB_OFFLOAD_TRBS_PER_SEGMENT - 1].link.control |=
			cpu_to_le32(LINK_TOGGLE);
	}
	xhci_initialize_ring_info_(ring);
	return ring;

fail:
	kfree(ring);
	return NULL;
}

static void xhci_mtk_free_event_ring(struct usb_offload_dev *udev, struct xhci_ring *evt_ring)
{
	int ret = 0;

	if (!evt_ring) {
		USB_OFFLOAD_INFO("Event ring has already freed\n");
		return;
	}

	ret = xhci_mtk_free_ring(evt_ring, UO_STRUCT_EVRING);
	if (ret == -EINVAL)
		USB_OFFLOAD_MEM_DBG("phy:0x%llx isn't under managed\n",
			evt_ring->first_seg->dma);
}

static void xhci_mtk_free_erst(struct usb_offload_dev *udev, struct xhci_erst *erst)
{
	struct uo_buffer *buf;

	if (erst == NULL || erst->erst_dma_addr == 0) {
		USB_OFFLOAD_INFO("ERST has already freed\n");
		return;
	}

	buf = uob_search(UO_STRUCT_ERST, erst->erst_dma_addr);
	if (buf && mtk_offload_free_mem(buf))
		USB_OFFLOAD_ERR("Fail to free erst\n");
	else
		erst->entries = NULL;
}

static struct xhci_ring *xhci_mtk_alloc_transfer_ring(struct xhci_hcd *xhci,
		u32 endpoint_type, enum xhci_ring_type ring_type,
		unsigned int max_packet, gfp_t mem_flags)
{
	return xhci_ring_alloc_(xhci, 2, ring_type, max_packet, mem_flags);
}

static int xhci_mtk_ep_ctx_control(struct xhci_hcd *xhci, struct xhci_virt_device *virt_dev,
	u32 ep_id, struct xhci_ep_ctx *new_ctx, bool reconfigure)
{
	struct xhci_input_control_ctx *ctrl_ctx;
	struct xhci_ep_ctx *in_ep_ctx;
	struct usb_hcd *hcd;
	int ret;

	if (!xhci || !new_ctx || !virt_dev)
		return -EINVAL;

	hcd = xhci->main_hcd;
	in_ep_ctx = xhci_get_ep_ctx__(xhci, virt_dev->in_ctx, ep_id);
	if (!in_ep_ctx)
		return -EINVAL;

	in_ep_ctx->ep_info = new_ctx->ep_info;
	in_ep_ctx->ep_info2 = new_ctx->ep_info2;
	in_ep_ctx->deq = new_ctx->deq;
	in_ep_ctx->tx_info = new_ctx->tx_info;
	if (xhci->quirks & XHCI_MTK_HOST) {
		in_ep_ctx->reserved[0] = new_ctx->reserved[0];
		in_ep_ctx->reserved[1] = new_ctx->reserved[1];
	}

	ctrl_ctx = (struct xhci_input_control_ctx *)virt_dev->in_ctx->bytes;
	ctrl_ctx->add_flags = cpu_to_le32(BIT(ep_id + 1));
	if (reconfigure) {
		/* command: RECONFIGURE_ENDPOINT */
		ctrl_ctx->drop_flags = cpu_to_le32(BIT(ep_id + 1));
		ret = xhci_check_bandwidth_(hcd, virt_dev->udev);
	} else {
		/* command: EVALUATE_CONTEXT */
		ctrl_ctx->drop_flags = 0;
		ret = -EOPNOTSUPP;
	}

	return ret;
}

static int xhci_mtk_soft_reset_ep(struct xhci_hcd *xhci, u32 slot, u32 ep)
{
	struct xhci_command *command;
	u32 trb_slot_id = SLOT_ID_FOR_TRB(slot);
	u32 trb_ep_index = EP_INDEX_FOR_TRB(ep);
	u32 type = TRB_TYPE(TRB_RESET_EP);
	int ret = 0;

	command = xhci_alloc_command_(xhci, true, GFP_ATOMIC);
	if (!command) {
		USB_OFFLOAD_ERR("fail to allocate xhci_command\n");
		return -ENOMEM;
	}

	type |= TRB_TSP; /* for soft reset */

	ret = xhci_vendor_queue_command_(xhci, command, 0, 0, 0,
		trb_slot_id | trb_ep_index | type , false);
	if (ret) {
		USB_OFFLOAD_ERR("fail to queue reset endpoint for slot %d ep %d, ret:%d\n",
			slot, ep, ret);
		return ret;
	}

	wait_for_completion(command->completion);
	xhci_free_command_(uodev->xhci, command);
	USB_OFFLOAD_ERR("complete reset endpoint for slot %d ep %d\n", slot, ep);

	return 0;
}

static void xhci_mtk_ring_check(struct xhci_ring *ring)
{
	dma_addr_t first_dma, link_dma, point_dma;
	struct xhci_segment *cur, *next;
	union xhci_trb *link_trb;
	int i;

	cur = ring->first_seg;
	for (i = 0; i < ring->num_segs; i++) {
		next = cur->next;
		first_dma = cur->dma;
		link_dma = cur->dma + ((USB_OFFLOAD_TRBS_PER_SEGMENT - 1) * 16);

		/* set segment pointer of link trb again */
		link_trb = &cur->trbs[USB_OFFLOAD_TRBS_PER_SEGMENT - 1];
		link_trb->link.segment_ptr = cpu_to_le64(next->dma);

		point_dma = link_trb->link.segment_ptr;
		USB_OFFLOAD_MEM_DBG("cur:[start_trb:0x%llx end_trb:0x%llx point_to:0x%llx(high:0x%x low:0x%x)],"
			" next:(start_trb:0x%llx)\n", first_dma, link_dma,
			point_dma, link_trb->generic.field[1], link_trb->generic.field[0], next->dma);
		cur = cur->next;
	}
}

int xhci_mtk_realloc_transfer_ring(unsigned int slot_id, unsigned int ep_id,
	enum uo_provider_type type, bool is_rsv)
{
	struct xhci_hcd *xhci = uodev->xhci;
	struct xhci_virt_device *virt_dev;
	struct xhci_ep_ctx *out_ep_ctx;
	struct xhci_ring *new_ring, *old_ring;
	int num_segs = 1, max_packet;
	int cycle_state = 1, ret;
	enum xhci_ring_type ring_type;

	virt_dev = xhci->devs[slot_id];
	if (!virt_dev) {
		USB_OFFLOAD_ERR("(slot:%d) virtual device is NULL\n", slot_id);
		ret = -EINVAL;
		goto error;
	}

	out_ep_ctx = xhci_get_ep_ctx__(xhci, virt_dev->out_ctx, ep_id);
	if (!out_ep_ctx) {
		USB_OFFLOAD_ERR("(slot:%d ep:%d) endpoint context is NULL\n", slot_id, ep_id);
		ret = -EINVAL;
		goto error;
	}

	/* 1. create new ring
	 * we only need to provide new ring on virt_dev->eps[ep_id].new_ring,
	 * old ring would be freed automatically if RECONFIURE_ENDPOINT was successful.
	 */
	old_ring = virt_dev->eps[ep_id].ring;
	max_packet = old_ring->bounce_buf_len;
	ring_type = old_ring->type;

	if (type == UO_PROV_NUM)
		new_ring = xhci_ring_alloc_(xhci, 2, ring_type, max_packet, GFP_ATOMIC);
	else
		new_ring = xhci_mtk_alloc_ring(xhci, num_segs, cycle_state, ring_type,
			max_packet, GFP_ATOMIC, type, is_rsv);

	if (!new_ring) {
		USB_OFFLOAD_ERR("fail to allocate ring\n");
		ret = -ENOMEM;
		goto error;
	}
	USB_OFFLOAD_INFO("[slot:%d ep:%d] new_ring:(phy:0x%llx) old_ring:(phy:0x%llx) ep_state:%d\n",
		slot_id, ep_id, new_ring->first_seg->dma, old_ring->first_seg->dma,
		out_ep_ctx->ep_info & EP_STATE_MASK);
	mdelay(5);
	xhci_mtk_ring_check(new_ring);
	virt_dev->eps[ep_id].new_ring = new_ring;

	/* 2. update dequeue pointer of output ep context */
	out_ep_ctx->deq = cpu_to_le64(virt_dev->eps[ep_id].new_ring->first_seg->dma |
					virt_dev->eps[ep_id].new_ring->cycle_state);

	/* 3. send command: RECONFIGURE_ENDPOINT*/
	ret = xhci_mtk_ep_ctx_control(xhci, virt_dev, ep_id, out_ep_ctx, true);
	if (ret) {
		USB_OFFLOAD_ERR("fail to send RECONFIGURE_ENDPOINT\n");
		goto error;
	}
	USB_OFFLOAD_INFO("[slot:%d ep:%d] after REGONFIGURE_ENDPOINT, ep_state:0x%x\n",
		slot_id, ep_id,	out_ep_ctx->ep_info & EP_STATE_MASK);

	/* fix me: we only handle STOPPED state ?? */
	if ((out_ep_ctx->ep_info & EP_STATE_MASK) == EP_STATE_STOPPED) {
		if (xhci_mtk_soft_reset_ep(xhci, slot_id, ep_id))
			USB_OFFLOAD_ERR("fail to send RESET_ENDPOINT, ep_state:0x%x\n",
				out_ep_ctx->ep_info & EP_STATE_MASK);
	}

	return ret;
error:
	USB_OFFLOAD_ERR("fail to reallocate, ring was still on AP viewed only\n");
	return ret;
}

static int xhci_prepare_offloading(struct usb_device *udev,
	struct xhci_sideband_ *sb, unsigned int pipe)
{
	struct usb_host_endpoint *endpoint;
	enum uo_provider_type type;
	unsigned int slot, ep;
	int direction, retval;

	if (!udev || !sb)
		return -EINVAL;

	endpoint = usb_pipe_endpoint(udev, pipe);
	if (!endpoint) {
		USB_OFFLOAD_INFO("pipe:%d context is null\n", pipe);
		return -EINVAL;
	}

	slot = udev->slot_id;
	ep = xhci_get_endpoint_index_(&endpoint->desc);
	direction = usb_endpoint_dir_in(&endpoint->desc);
	type = usb_offload_mem_type_lp_ex(direction);

	USB_OFFLOAD_INFO("prepare offloading for slot:%d ep:%d dir:%d\n", slot, ep, direction);

	retval = xhci_sideband_add_endpoint_(sb, endpoint);
	if (retval < 0) {
		USB_OFFLOAD_ERR("fail to add endpoint to sideband\n");
		return retval;
	}

	retval = xhci_mtk_realloc_transfer_ring(slot, ep, type, true);
	if (retval < 0) {
		USB_OFFLOAD_ERR("fail to reallocate transfer ring\n");
		if (xhci_sideband_remove_endpoint_(sb, endpoint) < 0)
			USB_OFFLOAD_ERR("fail to remove endpoint from sideband\n");
		return retval;
	}

	return 0;
}

static void xhci_lift_offloading(struct usb_device *udev,
	struct xhci_sideband_ *sb, unsigned int pipe)
{
	struct xhci_hcd *xhci = uodev->xhci;
	struct usb_host_endpoint *endpoint;
	struct xhci_virt_device *virt_dev;
	struct xhci_ep_ctx *ep_ctx;
	unsigned int slot, ep;
	int direction;

	if (!udev || !sb)
		return;

	endpoint = usb_pipe_endpoint(udev, pipe);
	if (!endpoint) {
		USB_OFFLOAD_INFO("pipe:%d context is null\n", pipe);
		return;
	}

	slot = udev->slot_id;
	ep = xhci_get_endpoint_index_(&endpoint->desc);
	direction = usb_endpoint_dir_in(&endpoint->desc);

	USB_OFFLOAD_INFO("lift offloading for slot:%d ep:%d dir:%d\n", slot, ep, direction);

	if (xhci_sideband_stop_endpoint_(sb, endpoint)< 0)
		USB_OFFLOAD_ERR("fail to stop endpoint");

	virt_dev = xhci->devs[slot];
	if (virt_dev) {
		ep_ctx = xhci_get_ep_ctx__(xhci, virt_dev->out_ctx, ep);
		if (ep_ctx)
			USB_OFFLOAD_INFO("ep_state:%d\n", ep_ctx->ep_info & EP_STATE_MASK);
	}

	if (xhci_sideband_remove_endpoint_(sb, endpoint) < 0)
		USB_OFFLOAD_ERR("fail to remove emdpoint from sideband\n");
}

static void xhci_mtk_free_transfer_ring(struct xhci_hcd *xhci,
	struct xhci_ring *ring, unsigned int ep_index)
{
	int ret = 0;

	ret = xhci_mtk_free_ring(ring, UO_STRUCT_TRRING);
	if (ret == -EINVAL) {
		USB_OFFLOAD_MEM_DBG("ep_id:%d phy:0x%llx isn't under managed\n",
			ep_index, ring->first_seg->dma);
		xhci_ring_free_(xhci, ring);
	}
}

static struct xhci_interrupter *
xhci_mtk_alloc_interrupter(struct xhci_hcd *xhci,
		int num_seg, gfp_t mem_flags)
{
	struct device *dev = xhci_to_hcd(xhci)->self.sysdev;
	struct xhci_interrupter *ir;
	int ret;

	USB_OFFLOAD_MEM_DBG("num_seg:%d\n", num_seg);

	ir = kzalloc_node(sizeof(*ir), mem_flags, dev_to_node(dev));
	if (!ir) {
		USB_OFFLOAD_ERR("error allocating interrupter\n");
		return NULL;
	}

	ir->event_ring = xhci_mtk_alloc_event_ring(uodev);
	if (!ir->event_ring) {
		USB_OFFLOAD_ERR("error allocating event ring\n");
		kfree(ir);
		return NULL;
	}

	ret = xhci_mtk_alloc_erst(uodev, ir->event_ring, &ir->erst);
	if (ret) {
		USB_OFFLOAD_ERR("Failed to allocate interrupter erst\n");
		xhci_mtk_free_ring(ir->event_ring, UO_STRUCT_EVRING);
		kfree(ir);
		return NULL;
	}

	return ir;
}

static void xhci_mtk_free_interrupter(struct xhci_hcd *xhci, struct xhci_interrupter *ir)
{
	if (!ir || !xhci)
		return;

	/* interrupter 0 was primary one */
	if (ir->intr_num == 0) {
		xhci_free_interrupter_(xhci, ir);
		return;
	}

	mutex_lock(&uodev->ir_lock);
	if (!uodev->ir) {
		USB_OFFLOAD_INFO("interrupter%d was already freed\n", XHCI1_INTR_TARGET);
		return;
	}

	if (unlikely(uodev->ir != ir)) {
		USB_OFFLOAD_ERR("interrupter%d wasn't under managed\n", XHCI1_INTR_TARGET);
		return;
	}

	if (uodev->last_card_num >= 0 && uodev->last_card_num < SNDRV_CARDS) {
		if (uadev[uodev->last_card_num].sb)
			uadev[uodev->last_card_num].sb->ir = NULL;
		else
			USB_OFFLOAD_ERR("sideband was already freed\n");
	}

	xhci->interrupters[ir->intr_num] = NULL;
	uodev->ir = NULL;
	mutex_unlock(&uodev->ir_lock);

	xhci_mtk_free_erst(uodev, &ir->erst);
	xhci_mtk_free_event_ring(uodev, ir->event_ring);
	ir->event_ring = NULL;
	kfree(ir);

	return;
}

static struct xhci_ring *xhci_mtk_alloc_event_ring(struct usb_offload_dev *udev)
{
	struct xhci_ring *event_ring;
	int num_segs = 1;
	int cycle_state = 1;

	event_ring = xhci_mtk_alloc_ring(udev->xhci, num_segs, cycle_state,
			TYPE_EVENT, 0, GFP_ATOMIC, usb_offload_mem_type_lp(), true);
	if (!event_ring) {
		USB_OFFLOAD_ERR("error allocating event ring\n");
		return NULL;
	}
	return event_ring;
}

static int xhci_mtk_alloc_erst(struct usb_offload_dev *udev,
		struct xhci_ring *evt_ring, struct xhci_erst *erst)
{
	int ret;
	size_t size;
	unsigned int val;
	struct xhci_segment *seg;
	struct xhci_erst_entry *entry;
	struct uo_buffer *buf;

	size = size_mul(sizeof(struct xhci_erst_entry), evt_ring->num_segs);
	buf = uob_get_empty(UO_STRUCT_ERST);
	if (!buf) {
		USB_OFFLOAD_ERR("insufficent on %s array\n", uo_struct_name(UO_STRUCT_ERST));
		ret = -EINVAL;
		goto FAIL_TO_ALLOC_ERST;
	}
	ret = mtk_offload_alloc_mem(buf, size, 64,
		usb_offload_mem_type_lp(), UO_STRUCT_ERST, true);
	if (ret != 0) {
		USB_OFFLOAD_ERR("Allocate ERST Fail!!!\n");
		goto FAIL_TO_ALLOC_ERST;
	}
	erst->entries = (struct xhci_erst_entry *)buf->virt;
	erst->erst_dma_addr = buf->phys;
	erst->num_entries = evt_ring->num_segs;

	seg = evt_ring->first_seg;
	for (val = 0; val < evt_ring->num_segs; val++) {
		entry = &erst->entries[val];
		entry->seg_addr = cpu_to_le64(seg->dma);
		entry->seg_size = cpu_to_le32(TRBS_PER_SEGMENT);
		entry->rsvd = 0;
		seg = seg->next;
	}
	return 0;

FAIL_TO_ALLOC_ERST:
	return ret;
}

static bool xhci_mtk_is_streaming(struct xhci_hcd *xhci)
{
	return uodev->is_streaming;
}

static int check_usb_offload_quirk(int vid, int pid)
{
	if ((vid == 0x046D && pid == 0x0A38) ||
		(vid == 0x3302 && pid == 0x00c0) ||
		(vid == 0x0BDA && pid == 0x4BD1) ||
		(vid == 0x8087 && pid == 0x1024) ||
		(vid == 0x0ECB && pid == 0x20F6)) {
		USB_OFFLOAD_INFO("vid:0x%x pid:0x%x NOT SUPPORT!!\n", vid, pid);
		return -1;
	}

	return 0;
}

static void check_valid_device(struct usb_device *rhdev,
	struct usb_device *udev, bool *support, bool *on_hub)
{
	struct usb_host_config *config;
	struct usb_interface *intf;
	struct usb_host_interface *hostif;
	struct usb_interface_descriptor *intfd;
	struct usb_endpoint_descriptor *epd;
	int vid, pid, device_class;
	int intf_idx, alt_idx, ep_idx;
	bool has_valid_ep = false, has_invalid_ep = false, valid = false;
	u8 ep_usage, class, subclass;

	*on_hub = false;
	*support = false;

	vid = udev->descriptor.idVendor;
	pid = udev->descriptor.idProduct;
	if (check_usb_offload_quirk(vid, pid)) {
		USB_OFFLOAD_INFO("(vid:0x%x pid:0x%x) in unsupport quirk table\n", vid, pid);
		goto error;
	}

	if (udev->parent != rhdev) {
		*on_hub = true;
		if (!uodev->policy.support_hub) {
			USB_OFFLOAD_INFO("unsupport hub offloading\n");
			goto error;
		}
	}

	if (unlikely(!udev->actconfig))
		return;

	config = udev->actconfig;
	device_class = udev->descriptor.bDeviceClass;

	/* step1. check device class */
	if (device_class != USB_CLASS_PER_INTERFACE && device_class != USB_CLASS_MISC) {
		USB_OFFLOAD_DBG("(vid:0x%x pid:0x%x bDeviceClass:0x%x) wasn't audio device\n",
			vid, pid, device_class);
		goto error;
	}

	/* step2. check every interfaces*/
	for (intf_idx = 0; intf_idx < config->desc.bNumInterfaces; intf_idx++) {
		intf = config->interface[intf_idx];
		if (unlikely(!intf))
			continue;

		/* step3. check every alternative setting
		 * [valid alt_setting]:   audio streaming interface class
		 * [invalid alt_setting]: others (hid class, audio control class etc)
		 */
		for (alt_idx = 0; alt_idx < intf->num_altsetting; alt_idx++) {
			hostif = &intf->altsetting[alt_idx];
			intfd = get_iface_desc(hostif);
			if (unlikely(!intfd))
				continue;

			class = intfd->bInterfaceClass;
			subclass = intfd->bInterfaceSubClass;

			if (!(class == USB_CLASS_AUDIO && subclass == USB_SUBCLASS_AUDIOSTREAMING)) {
				USB_OFFLOAD_DBG("(intf:%d alt:%d bInterfaceClass:0x%x "
					"bInterfaceSubClass:0x%x)wasn't stream intface\n",
					intf_idx, alt_idx, class, subclass);
				continue;
			}

			/* step4. check every endpoints
			 * [valid ep]:   endpoint with valid usage
			 * [invalid ep]: endpoint with invalid usage
			 */
			for (ep_idx = 0; ep_idx < intfd->bNumEndpoints; ep_idx++) {
				epd = get_endpoint(hostif, ep_idx);
				if (unlikely(!epd))
					continue;
				ep_usage = epd->bmAttributes & USB_ENDPOINT_USAGE_MASK;

				switch (ep_usage) {
				/* 0x00: data usage */
				case USB_ENDPOINT_USAGE_DATA:
					valid = true;
					has_valid_ep = true;
					break;
				/* 0x10: explicit feedback*/
				case USB_ENDPOINT_USAGE_FEEDBACK:
					if (uodev->policy.support_fb) {
						valid = true;
						has_valid_ep = true;
						break;
					}
					fallthrough;
				/* 0x20: implicit feedback */
				default:
					USB_OFFLOAD_INFO("(intf:%d alt:%dep:%d usage:0x%02x) invalid\n",
						intf_idx, alt_idx, ep_idx, ep_usage);
					valid = false;
					has_invalid_ep = true;
					break;
				}
			}
		}
	}

	*support = (has_valid_ep && !has_invalid_ep);

error:
	USB_OFFLOAD_INFO("vid:0x%x pid:0x%x support:%d(valid_ep:%d invlaid_ep:%d) on_hub:%d\n",
		vid, pid, *support, has_valid_ep, has_invalid_ep, *on_hub);
}

static int usb_offload_open(struct inode *ip, struct file *fp)
{
	int i, ret = 0;

	USB_OFFLOAD_INFO("++\n");
	if (stage_occupy(DRV_STAGE_FILE_OPS) < 0) {
		ret = -EBUSY;
		goto busy;
	}

	if (!uodev->total_connected || uodev->is_streaming) {
		USB_OFFLOAD_ERR("unexpected open (total_connected:%d is_streaming:%d)\n",
			uodev->total_connected, uodev->is_streaming);
		ret = -EOPNOTSUPP;
		goto not_support;
	}

	for (i = 0; i < SNDRV_CARDS; i++) {
		if (!atomic_read(&uadev[i].connected) || uadev[i].chip == NULL)
			continue;
		USB_OFFLOAD_INFO("%s device (card:%d)\n", uadev[i].is_valid ? "valid" : "invalid", i);
		if (!uadev[i].is_valid) {
			USB_OFFLOAD_ERR("detected invalid device\n");
			ret = -EOPNOTSUPP;
			goto not_support;
		}
	}

	USB_OFFLOAD_INFO("-- support offloading!!\n");

	stage_vacate(DRV_STAGE_FILE_OPS);
	uo_mbrain_update(UO_PHASE_OPEN, UO_ERROR_SUCCESS);
	return ret;

not_support:
	stage_vacate(DRV_STAGE_FILE_OPS);
busy:
	USB_OFFLOAD_INFO("-- unsupport offloading!!\n");
	return ret;
}

static int usb_offload_release(struct inode *ip, struct file *fp)
{
	int ret = 0, idx;

	USB_OFFLOAD_INFO("++\n");
	if (stage_occupy(DRV_STAGE_FILE_OPS) < 0) {
		ret = -EBUSY;
		goto busy;
	}

	for (idx = 0; idx < SNDRV_CARDS; idx++) {
		if (!uadev[idx].chip)
			continue;

		/* just cleanup audio device, do not shut it down */
		uaudio_dev_cleanup(&uadev[idx]);
	}

	usb_offload_status();

	stage_vacate(DRV_STAGE_FILE_OPS);
busy:
	USB_OFFLOAD_INFO("--\n");
	return ret;
}

static inline const char *ioctl_event_name(unsigned int cmd, unsigned long value)
{
	const char *name;

	switch (cmd) {
	case USB_OFFLOAD_INIT_ADSP:
		if (value)
			name = "<IOCTL:INIT_ADSP>";
		else
			name = "<IOCTL:DEINIT_ADSP>";
		break;
	case USB_OFFLOAD_ENABLE_STREAM:
		name = "<IOCTL:ENABLE_STREAM>";
		break;
	case USB_OFFLOAD_DISABLE_STREAM:
		name = "<IOCTL:DISABLE_STREAM>";
		break;
	default:
		name = "<IOCTL:UNKNOWN>";
		break;
	}

	return name;
}

static long usb_offload_ioctl(struct file *fp,
	unsigned int cmd, unsigned long value)
{
	long ret = 0;
	const char *name = ioctl_event_name(cmd, value);
	struct usb_audio_stream_info uainfo;
	struct snd_usb_substream *subs;
	struct usb_audio_dev *audio_dev;
	int info_idx = -EINVAL;
	bool streaming;

	USB_OFFLOAD_INFO("%s ++\n", name);

	if (stage_occupy(DRV_STAGE_FILE_OPS) < 0) {
		ret = -EBUSY;
		goto busy;
	}

	switch (cmd) {
	case USB_OFFLOAD_INIT_ADSP:
		usb_offload_register_ipi_recv();
		if (value)
			ret = send_init_adsp();
		else
			ret = send_deinit_adsp();
		break;
	case USB_OFFLOAD_ENABLE_STREAM:
	case USB_OFFLOAD_DISABLE_STREAM:
		if (!uodev->adsp_inited) {
			USB_OFFLOAD_ERR("ADSP NOT INITED YET!!!\n");
			ret = -EFAULT;
			goto fail;
		}

		if (copy_from_user(&uainfo, (void __user *)value, sizeof(uainfo))) {
			USB_OFFLOAD_ERR("copy_from_user ERR!!!\n");
			ret = -EFAULT;
			goto fail;
		}

		dump_uainfo(&uainfo);

		if (uainfo.pcm_card_num >= SNDRV_CARDS) {
			USB_OFFLOAD_ERR("invalid card_num:%d\n", uainfo.pcm_card_num);
			ret = -EFAULT;
			goto fail;
		}

		subs = find_snd_usb_substream(
			uainfo.pcm_card_num, uainfo.pcm_dev_num, uainfo.direction);
		if (!subs) {
			USB_OFFLOAD_ERR("substream was null, maybe it's disconnected\n");
			ret = -EFAULT;
			goto fail;
		}

		if (!is_uainfo_valid(&uainfo) || !is_pcm_size_valid(&uainfo, subs)) {
			USB_OFFLOAD_ERR("uainfo invalid!!!\n");
			ret = -EFAULT;
			goto fail;
		}

		streaming = uainfo.direction ? uodev->rx_streaming : uodev->tx_streaming;
		if (!((uainfo.enable != 0) ^ streaming)) {
			USB_OFFLOAD_ERR("%s was already %s\n", uainfo.direction ? "rx" : "tx",
				uainfo.enable ? "enabled" : "disabled");
			ret = -EBUSY;
			goto fail;
		}

		audio_dev = &uadev[uainfo.pcm_card_num];
		if (!audio_dev->is_valid) {
			USB_OFFLOAD_ERR("routing on invalid device\n");
			ret = -EINVAL;
			goto fail;
		}

		info_idx = info_idx_from_ifnum(uainfo.pcm_card_num, subs->cur_audiofmt ?
			subs->cur_audiofmt->iface : -1, uainfo.enable);

		if ((uainfo.enable && info_idx < 0) ||
			(!uainfo.enable && (info_idx < 0 || info_idx >= audio_dev->num_intf))) {
			USB_OFFLOAD_ERR("invalid info_idx:%d (enable:%d)\n",
				info_idx, uainfo.enable);
			ret = -EINVAL;
			goto fail;
		}

		ret = handle_enable_stream(&uainfo, subs, audio_dev, info_idx);
		break;
	}

fail:
	stage_vacate(DRV_STAGE_FILE_OPS);
busy:
	USB_OFFLOAD_INFO("%s ret:%ld --\n", name, ret);
	return ret;
}

static const char usb_offload_shortname[] = "mtk_usb_offload";

/* file operations for /dev/mtk_usb_offload */
static const struct file_operations usb_offload_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = usb_offload_ioctl,
	.open = usb_offload_open,
	.release = usb_offload_release,
};

static struct miscdevice usb_offload_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = usb_offload_shortname,
	.fops = &usb_offload_fops,
};

static struct xhci_vendor_ops xhci_mtk_vendor_ops = {
	.is_usb_offload_enabled = xhci_mtk_is_usb_offload_enabled,
	.alloc_dcbaa = xhci_mtk_alloc_dcbaa,
	.free_dcbaa = xhci_mtk_free_dcbaa,
	.alloc_container_ctx = xhci_mtk_alloc_container_ctx,
	.free_container_ctx = xhci_mtk_free_container_ctx,
	.alloc_transfer_ring = xhci_mtk_alloc_transfer_ring,
	.free_transfer_ring = xhci_mtk_free_transfer_ring,
	.alloc_interrupter = xhci_mtk_alloc_interrupter,
	.free_interrupter = xhci_mtk_free_interrupter,
	.is_streaming = xhci_mtk_is_streaming,
	.usb_offload_skip_urb = usb_offload_trace_hid_enqueue,
	.usb_offload_connect = sound_usb_connect,
	.usb_offload_disconnect = sound_usb_disconnect,
};

int xhci_mtk_ssusb_offload_get_mode(struct device *dev)
{
	bool hold_apsrc, hold_vcore;
	int mode;

	if (!uodev->is_streaming)
		return SSUSB_OFFLOAD_MODE_NONE;

	/* power sensitive: erst, ev_ring, tr_ring, urb */

	/* if any power sensitive structure on dram ? */
	hold_apsrc = mtk_offload_hold_apsrc();

	/* if any power sensitive structure on afe sram ? */
	hold_vcore = mtk_offload_hold_vcore();

	if (hold_apsrc) {
		mode = uodev->speed < USB_SPEED_SUPER ?
				SSUSB_OFFLOAD_MODE_D : SSUSB_OFFLOAD_MODE_D_SS;
		goto dram_mode;
	}

	if (hold_vcore)
		mode = uodev->speed < USB_SPEED_SUPER ?
			SSUSB_OFFLOAD_MODE_S : SSUSB_OFFLOAD_MODE_S_SS;
	else
		mode = uodev->speed < USB_SPEED_SUPER ?
			SSUSB_OFFLOAD_MODE_S_EX : SSUSB_OFFLOAD_MODE_S_SS_EX;

dram_mode:
	USB_OFFLOAD_INFO("mode:%d hold_apsrc:%d hold_vcore:%d\n", mode, hold_apsrc, hold_vcore);
	return mode;
}

static int stage_occupy(unsigned long target)
{
	unsigned long idle = DRV_STAGE_IDLE;
	int retval;

	spin_lock(&uodev->dev_lock);

	retval = wait_condition(!test_bit(~idle, &uodev->stage), WAIT_IDLE_TIMEOUT_NS);
	if (retval == 0) {
		set_bit(target, &uodev->stage);
		USB_OFFLOAD_DBG("driver's idle, set stage:0x%lx\n", uodev->stage);
	}

	if (retval < 0)
		USB_OFFLOAD_ERR("driver's busy, stage(cur:0x%lx target:0x%lx)\n",
			uodev->stage, target);

	spin_unlock(&uodev->dev_lock);
	return retval;
}

static inline void stage_vacate(unsigned long stage)
{
	clear_bit(stage, &uodev->stage);
}

static int usb_offload_probe(struct platform_device *pdev)
{
	struct device_node *node_xhci_host;
	int ret = 0;
	int dsp_type;

	uodev = devm_kzalloc(&pdev->dev, sizeof(struct usb_offload_dev),
		GFP_KERNEL);
	if (!uodev) {
		USB_OFFLOAD_ERR("Fail to allocate usb_offload_dev\n");
		return -ENOMEM;
	}

	uodev->dev = &pdev->dev;

	device_init_wakeup(uodev->dev, true);

	usb_offload_platform_policy_init(uodev->dev, &uodev->policy);
	uodev->adv_lowpwr = uodev->policy.adv_lowpwr;

	uodev->is_streaming = false;
	uodev->adsp_inited = false;
	uodev->adsp_ready = true; /* default ready ?? */
	uodev->speed = USB_SPEED_UNKNOWN;
	uodev->xhci = NULL;
	uodev->total_connected = 0;
	uodev->last_card_num = -EINVAL;

	node_xhci_host = of_parse_phandle(uodev->dev->of_node, "xhci-host", 0);
	if (node_xhci_host) {

		/* reggister dram provider */
		ret = mtk_offload_provider_register(uodev, UO_PROV_DRAM);
		if (!ret) {
			/* initialize dram reserved region */
			ret = mtk_offload_init_rsv(uodev, UO_PROV_DRAM);
			if (ret) {
				USB_OFFLOAD_ERR("fail to init reserved region of dram provider\n");
				goto INIT_SHAREMEM_FAIL;
			}
		} else {
			USB_OFFLOAD_ERR("fail to register dram provider\n");
			goto INIT_SHAREMEM_FAIL;
		}

		if (uodev->adv_lowpwr) {
			/* reggister sram provider */
			if (mtk_offload_provider_register(uodev, UO_PROV_SRAM)) {
				USB_OFFLOAD_ERR("fail to register sram provider\n");
				uodev->policy.adv_lowpwr = false;
				uodev->adv_lowpwr = uodev->policy.adv_lowpwr;
			}
		}

		ret = misc_register(&usb_offload_device);
		if (ret) {
			USB_OFFLOAD_ERR("Fail to allocate usb_offload_device\n");
			ret = -ENOMEM;
			goto INIT_MISC_DEV_FAIL;
		}

		uodev->ssusb_offload_notify = kzalloc(
					sizeof(*uodev->ssusb_offload_notify), GFP_KERNEL);
		if (!uodev->ssusb_offload_notify) {
			USB_OFFLOAD_ERR("Fail to alloc ssusb_offload_notify\n");
			ret = -ENOMEM;
			goto INIT_OFFLOAD_NOTIFY_FAIL;
		}
		uodev->ssusb_offload_notify->dev = uodev->dev;
		uodev->ssusb_offload_notify->get_mode = xhci_mtk_ssusb_offload_get_mode;
		ret = ssusb_offload_register(uodev->ssusb_offload_notify);
		if (ret) {
			USB_OFFLOAD_ERR("Fail to register ssusb_offload\n");
			ret = -ENOMEM;
			goto REG_SSUSB_OFFLOAD_FAIL;
		}

		/* driver stage sync */
		uodev->stage = 0;
		spin_lock_init(&uodev->dev_lock);

		/* interrupter sync */
		mutex_init(&uodev->ir_lock);

		USB_OFFLOAD_INFO("Set XHCI vendor hook ops\n");
		platform_set_drvdata(pdev, &xhci_mtk_vendor_ops);
		dsp_type = get_adsp_type();
		if (dsp_type == ADSP_TYPE_HIFI3) {
#ifdef CFG_RECOVERY_SUPPORT
			adsp_register_notify(&adsp_usb_offload_notifier);
#else
			USB_OFFLOAD_ERR("Do not register notifier. Recovery not enabled\n");
#endif
		} else if (dsp_type == ADSP_TYPE_RV55) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
#ifdef SKIP
			scp_A_register_notify(&scp_usb_offload_notifier);
#endif
#else
			USB_OFFLOAD_ERR("Do not register notifier. SCP not enabled\n");
#endif
		} else {
			USB_OFFLOAD_ERR("Unknown dsp type: %d\n", dsp_type);
			ret = -EINVAL;
			goto GET_DSP_TYPE_FAIL;
		}

	} else {
		USB_OFFLOAD_ERR("No 'xhci_host' node, NOT support USB_OFFLOAD\n");
		ret = -ENODEV;
		goto INIT_SHAREMEM_FAIL;
	}

	WARN_ON(register_trace_xhci_alloc_virt_device(monitor_alloc_virt_device, NULL));
	WARN_ON(register_trace_xhci_free_virt_device(monitor_free_virt_device, NULL));

	uob_assign_array(UO_STRUCT_DCBAA, NULL, BUF_DCBAA_SIZE);
	uob_assign_array(UO_STRUCT_CTX, NULL, BUF_CTX_SIZE);
	uob_assign_array(UO_STRUCT_ERST, NULL, BUF_ERST_SIZE);
	uob_assign_array(UO_STRUCT_EVRING, NULL, BUF_EV_RING_SIZE);
	uob_assign_array(UO_STRUCT_TRRING, NULL, BUF_TR_RING_SIZE);
	uob_assign_array(UO_STRUCT_URB, NULL, BUF_URB_SIZE);

	uob_init(UO_STRUCT_ERST);
	uob_init(UO_STRUCT_EVRING);
	uob_init(UO_STRUCT_TRRING);
	uob_init(UO_STRUCT_URB);

	usb_offload_hid_probe();
	usb_offload_debug_init(uodev);

	USB_OFFLOAD_INFO("Probe Success!!!");
	return ret;
GET_DSP_TYPE_FAIL:
REG_SSUSB_OFFLOAD_FAIL:
	kfree(uodev->ssusb_offload_notify);
INIT_OFFLOAD_NOTIFY_FAIL:
	misc_deregister(&usb_offload_device);
INIT_MISC_DEV_FAIL:
INIT_SHAREMEM_FAIL:
	of_node_put(node_xhci_host);
	USB_OFFLOAD_ERR("Probe Fail!!!");
	return ret;
}

static void usb_offload_remove(struct platform_device *pdev)
{
	int ret;

	USB_OFFLOAD_INFO("\n");
	ret = ssusb_offload_unregister(uodev->ssusb_offload_notify);
	if (ret)
		USB_OFFLOAD_ERR("ssusb_offload_unregister failed!\n");

	WARN_ON(unregister_trace_xhci_alloc_virt_device(monitor_alloc_virt_device, NULL));
	WARN_ON(unregister_trace_xhci_free_virt_device(monitor_free_virt_device, NULL));

	uob_deinit(UO_STRUCT_ERST);
	uob_deinit(UO_STRUCT_EVRING);
	uob_deinit(UO_STRUCT_TRRING);
	uob_deinit(UO_STRUCT_URB);
}

int register_uo_mbrain_cb(void (*cb)(struct uo_mbrain data))
{
	if (!cb)
		return -EINVAL;

	mbrain_cb = cb;
	USB_OFFLOAD_INFO("register mbrain callback\n");
	return 0;
}
EXPORT_SYMBOL_GPL(register_uo_mbrain_cb);

int unregister_uo_mbrain_cb(void)
{
	mbrain_cb = NULL;
	USB_OFFLOAD_INFO("unregister mbrain callback\n");
	return 0;
}
EXPORT_SYMBOL_GPL(unregister_uo_mbrain_cb);

void uo_mbrain_update(enum uo_mbrain_phase phase, enum uo_mbrain_error error)
{
	struct uo_mbrain data = {0};

	data.phase = phase;
	data.error = error;

	USB_OFFLOAD_DBG("vid:0x%x pid:0x%x phase:%d error:%d\n",
		data.vid, data.pid, data.phase, data.error);

	if (mbrain_cb)
		mbrain_cb(data);
}

static int usb_offload_smc_ctrl(int smc_req)
{
	struct arm_smccc_res res;

	USB_OFFLOAD_INFO("smc_req:%d\n", smc_req);
	if (smc_req != -1)
		arm_smccc_smc(MTK_SIP_KERNEL_USB_CONTROL,
			smc_req, 0, 0, 0, 0, 0, 0, &res);

	return 0;
}

static int __maybe_unused usb_offload_suspend(struct device *dev)
{
	if (!uodev->is_streaming)
		return 0;

	usb_offload_hid_start();
	print_all_memory();
	usb_offload_platform_action(dev, UO_PLAT_ACTION_SUSPEND);

	/* if it's streaming, call to TFA if it's required */
	return usb_offload_smc_ctrl(uodev->policy.smc_suspend);
}

static int __maybe_unused usb_offload_resume(struct device *dev)
{
	if (!uodev->is_streaming)
		return 0;

	usb_offload_hid_finish();
	print_all_memory();
	usb_offload_platform_action(dev, UO_PLAT_ACTION_RESUME);

	return usb_offload_smc_ctrl(uodev->policy.smc_resume);
}

static int __maybe_unused usb_offload_runtime_suspend(struct device *dev)
{
	if (!device_may_wakeup(dev))
		return 0;

	if (!uodev->is_streaming)
		return 0;

	usb_offload_hid_start();
	print_all_memory();
	usb_offload_platform_action(dev, UO_PLAT_ACTION_SUSPEND);

	return usb_offload_smc_ctrl(uodev->policy.smc_suspend);
}

static int __maybe_unused usb_offload_runtime_resume(struct device *dev)
{
	if (!device_may_wakeup(dev))
		return 0;

	if (!uodev->is_streaming)
		return 0;

	usb_offload_hid_finish();
	print_all_memory();
	usb_offload_platform_action(dev, UO_PLAT_ACTION_RESUME);

	return usb_offload_smc_ctrl(uodev->policy.smc_resume);
}

static const struct dev_pm_ops usb_offload_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(usb_offload_suspend, usb_offload_resume)
	SET_RUNTIME_PM_OPS(usb_offload_runtime_suspend,
					usb_offload_runtime_resume, NULL)
};

#define DEV_PM_OPS (IS_ENABLED(CONFIG_PM) ? &usb_offload_pm_ops : NULL)

static const struct of_device_id usb_offload_of_match[] = {
	{.compatible = "mediatek,usb-offload",},
	{},
};

MODULE_DEVICE_TABLE(of, usb_offload_of_match);

static struct platform_driver usb_offload_driver = {
	.probe = usb_offload_probe,
	.remove = usb_offload_remove,
	.driver = {
		.name = "mtk-usb-offload",
		.pm = DEV_PM_OPS,
		.of_match_table = of_match_ptr(usb_offload_of_match),
	},
};
module_platform_driver(usb_offload_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek USB Offload Driver");
