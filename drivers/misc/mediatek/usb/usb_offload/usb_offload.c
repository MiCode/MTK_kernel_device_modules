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
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/genalloc.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/usb.h>

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

#if IS_ENABLED(CONFIG_SND_USB_AUDIO)
#include "usbaudio.h"
#include "card.h"
#include "helper.h"
#include "pcm.h"
#include "power.h"
#endif
#if IS_ENABLED(CONFIG_USB_XHCI_MTK)
#include "xhci.h"
#include "xhci-mtk.h"
#endif
#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
#include <adsp_helper.h>
#include "audio_messenger_ipi.h"
#include "audio_task.h"
#include "audio_controller_msg_id.h"
#endif
#include "usb_offload.h"

static unsigned int usb_offload_log;
module_param(usb_offload_log, uint, 0644);
MODULE_PARM_DESC(usb_offload_log, "Enable/Disable USB Offload log");

#define USB_OFFLOAD_INFO(fmt, args...) do { \
	if (usb_offload_log > 1) \
		pr_info("USB_OFFLOAD, %s, " fmt, __func__, ## args); \
	} while (0)

#define USB_OFFLOAD_ERR(fmt, args...) do { \
	if (usb_offload_log) \
		pr_info("USB_OFFLOAD, %s, " fmt, __func__, ## args); \
	} while (0)


static struct usb_audio_dev uadev[SNDRV_CARDS];
static struct usb_offload_dev *uodev;
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

static void dump_uainfo(struct usb_audio_stream_info *uainfo)
{
	USB_OFFLOAD_INFO("uainfo->enable: %d\n"
						"uainfo->bit_rate: %d\n"
						"uainfo->number_of_ch: %d\n"
						"uainfo->bit_depth: %d\n"
						"uainfo->direction: %d\n"
						"uainfo->pcm_card_num: %d\n"
						"uainfo->pcm_dev_num: %d\n"
						"uainfo->xhc_irq_period_ms: %d\n"
						"uainfo->xhc_urb_num: %d\n"
						"uainfo->dram_size: %d\n"
						"uainfo->dram_cnt: %d\n"
						"uainfo->start_thld: %d\n"
						"uainfo->stop_thld: %d\n"
						"uainfo->pcm_size: %d\n"
						"uainfo->service_interval: %d\n"
						"uainfo->service_interval_valid\n",
						uainfo->enable,
						uainfo->bit_rate,
						uainfo->number_of_ch,
						uainfo->bit_depth,
						uainfo->direction,
						uainfo->pcm_card_num,
						uainfo->pcm_dev_num,
						uainfo->xhc_irq_period_ms,
						uainfo->xhc_urb_num,
						uainfo->dram_size,
						uainfo->dram_cnt,
						uainfo->start_thld,
						uainfo->stop_thld,
						uainfo->pcm_size,
						uainfo->service_interval,
						uainfo->service_interval_valid);
}

static void usb_audio_dev_intf_cleanup(struct usb_device *udev,
		struct intf_info *info)
{
	info->in_use = false;
}

static void uaudio_dev_cleanup(struct usb_audio_dev *dev)
{
	int if_idx;

	if (!dev->udev) {
		USB_OFFLOAD_ERR("USB audio device is already freed.\n");
		return;
	}

	/* free xfer buffer and unmap xfer ring and buf per interface */
	for (if_idx = 0; if_idx < dev->num_intf; if_idx++) {
		if (!dev->info[if_idx].in_use)
			continue;
		usb_audio_dev_intf_cleanup(dev->udev, &dev->info[if_idx]);
		USB_OFFLOAD_INFO("release resources: intf# %d card# %d\n",
				dev->info[if_idx].intf_num, dev->card_num);
	}

	dev->num_intf = 0;

	/* free interface info */
	kfree(dev->info);
	dev->info = NULL;
	dev->udev = NULL;
}

static void uaudio_disconnect_cb(struct snd_usb_audio *chip)
{
	int ret;
	struct usb_audio_dev *dev;
	int card_num = chip->card_num;
	struct usb_audio_stream_msg msg = {0};

	USB_OFFLOAD_INFO("for card# %d\n", card_num);

	if (card_num >=  SNDRV_CARDS) {
		USB_OFFLOAD_ERR("invalid card number\n");
		return;
	}

	mutex_lock(&chip->dev_lock);
	dev = &uadev[card_num];

	/* clean up */
	if (!dev->udev) {
		USB_OFFLOAD_INFO("no clean up required\n");
		goto done;
	}

	if (atomic_read(&dev->in_use)) {
		mutex_unlock(&chip->dev_lock);

		msg.status = USB_AUDIO_STREAM_REQ_STOP;
		msg.status_valid = 1;

		/* write to audio ipi*/
		ret = 0;
		/* wait response */

		atomic_set(&dev->in_use, 0);

		mutex_lock(&chip->dev_lock);
	}

	uaudio_dev_cleanup(dev);
done:
	mutex_unlock(&chip->dev_lock);
}

static void uaudio_dev_release(struct kref *kref)
{
	struct usb_audio_dev *dev = container_of(kref, struct usb_audio_dev, kref);

	USB_OFFLOAD_INFO("for dev %pK\n", dev);

	atomic_set(&dev->in_use, 0);
	wake_up(&dev->disconnect_wq);
}

static int info_idx_from_ifnum(int card_num, int intf_num, bool enable)
{
	int i;

	/*
	 * default index 0 is used when info is allocated upon
	 * first enable audio stream req for a pcm device
	 */
	if (enable && !uadev[card_num].info)
		return 0;

	for (i = 0; i < uadev[card_num].num_intf; i++) {
		if (enable && !uadev[card_num].info[i].in_use)
			return i;
		else if (!enable &&
				uadev[card_num].info[i].intf_num == intf_num)
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

/* looks up alias, if any, for controller DT node and returns the index */
static int usb_get_controller_id(struct usb_device *udev)
{
	if (udev->bus->sysdev && udev->bus->sysdev->of_node)
		return of_alias_get_id(udev->bus->sysdev->of_node, "usb");

	return -ENODEV;
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
	int protocol, card_num, pcm_dev_num;
	int interface, altset_idx;
	void *hdr_ptr;
	unsigned int data_ep_pipe = 0, sync_ep_pipe = 0;

	interface = subs->cur_audiofmt->iface;
	altset_idx = subs->cur_audiofmt->altset_idx;

	iface = usb_ifnum_to_if(subs->dev, interface);
	if (!iface) {
		USB_OFFLOAD_ERR("interface # %d does not exist\n", interface);
		ret = -ENODEV;
		goto err;
	}
	msg->uainfo = *uainfo;

	dump_uainfo(&msg->uainfo);

	assoc = iface->intf_assoc;
	pcm_dev_num = uainfo->pcm_dev_num;
	card_num = uainfo->pcm_card_num;

	msg->direction = uainfo->direction;
	msg->pcm_dev_num = uainfo->pcm_dev_num;
	msg->pcm_card_num = uainfo->pcm_card_num;

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

	if (!uadev[card_num].ctrl_intf) {
		USB_OFFLOAD_ERR("audio ctrl intf info not cached\n");
		ret = -ENODEV;
		goto err;
	}

	if (protocol != UAC_VERSION_3) {
		hdr_ptr = find_csint_desc(uadev[card_num].ctrl_intf->extra,
				uadev[card_num].ctrl_intf->extralen,
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
		msg->data_path_delay_valid = 1;
		fmt_v1 = (struct uac_format_type_i_discrete_descriptor *)fmt;
		msg->usb_audio_subslot_size = fmt_v1->bSubframeSize;
		msg->usb_audio_subslot_size_valid = 1;

		msg->usb_audio_spec_revision = le16_to_cpu(uac1_hdr->bcdADC);
		msg->usb_audio_spec_revision_valid = 1;
	} else if (protocol == UAC_VERSION_2) {
		struct uac2_ac_header_descriptor *uac2_hdr = hdr_ptr;

		fmt_v2 = (struct uac_format_type_i_ext_descriptor *)fmt;
		msg->usb_audio_subslot_size = fmt_v2->bSubslotSize;
		msg->usb_audio_subslot_size_valid = 1;

		msg->usb_audio_spec_revision = le16_to_cpu(uac2_hdr->bcdADC);
		msg->usb_audio_spec_revision_valid = 1;
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
		msg->usb_audio_subslot_size_valid = 1;
	} else {
		USB_OFFLOAD_ERR("unknown protocol version %x\n", protocol);
		ret = -ENODEV;
		goto err;
	}

	msg->slot_id = subs->dev->slot_id;
	msg->slot_id_valid = 1;

	memcpy(&msg->std_as_opr_intf_desc, &alts->desc, sizeof(alts->desc));
	msg->std_as_opr_intf_desc_valid = 1;

	ep = usb_pipe_endpoint(subs->dev, subs->data_endpoint->pipe);
	if (!ep) {
		USB_OFFLOAD_ERR("data ep # %d context is null\n",
				subs->data_endpoint->ep_num);
		ret = -ENODEV;
		goto err;
	}
	data_ep_pipe = subs->data_endpoint->pipe;
	memcpy(&msg->std_as_data_ep_desc, &ep->desc, sizeof(ep->desc));
	msg->std_as_data_ep_desc_valid = 1;

	if (subs->sync_endpoint) {
		ep = usb_pipe_endpoint(subs->dev, subs->sync_endpoint->pipe);
		if (!ep) {
			USB_OFFLOAD_ERR("implicit fb on data ep\n");
			goto skip_sync_ep;
		}
		sync_ep_pipe = subs->sync_endpoint->pipe;
		memcpy(&msg->std_as_sync_ep_desc, &ep->desc, sizeof(ep->desc));
		msg->std_as_sync_ep_desc_valid = 1;
	}

skip_sync_ep:
	msg->interrupter_num = uodev->intr_num;
	msg->interrupter_num_valid = 1;
	msg->controller_num_valid = 0;
	ret = usb_get_controller_id(subs->dev);
	if (ret >= 0) {
		msg->controller_num = ret;
		msg->controller_num_valid = 1;
	}

	msg->speed_info = get_speed_info(subs->dev->speed);
	if (msg->speed_info == USB_AUDIO_DEVICE_SPEED_INVALID) {
		ret = -ENODEV;
		goto err;
	}

	msg->speed_info_valid = 1;

	if (!atomic_read(&uadev[card_num].in_use)) {
		kref_init(&uadev[card_num].kref);
		init_waitqueue_head(&uadev[card_num].disconnect_wq);
		uadev[card_num].num_intf =
		subs->dev->config->desc.bNumInterfaces;
		uadev[card_num].info = kcalloc(uadev[card_num].num_intf,
		sizeof(struct intf_info), GFP_KERNEL);
		if (!uadev[card_num].info) {
			ret = -ENOMEM;
			goto err;
		}
		uadev[card_num].udev = subs->dev;
		atomic_set(&uadev[card_num].in_use, 1);
	} else {
		kref_get(&uadev[card_num].kref);
	}

	uadev[card_num].card_num = card_num;
	uadev[card_num].usb_core_id = msg->controller_num;

	uadev[card_num].info[info_idx].data_ep_pipe = data_ep_pipe;
	uadev[card_num].info[info_idx].sync_ep_pipe = sync_ep_pipe;
	uadev[card_num].info[info_idx].pcm_card_num = card_num;
	uadev[card_num].info[info_idx].pcm_dev_num = pcm_dev_num;
	uadev[card_num].info[info_idx].direction = subs->direction;
	uadev[card_num].info[info_idx].intf_num = interface;
	uadev[card_num].info[info_idx].in_use = true;

	set_bit(card_num, &uodev->card_slot);

	return 0;
err:
	return ret;
}

int send_init_ipi_msg_to_adsp(struct mpu_info_xhci *mpu_info)
{
	int send_result = 0;
	struct ipi_msg_t ipi_msg;

	USB_OFFLOAD_INFO("xhci_rsv_addr: 0x%x, xhci_rsv_size: %d, size: %d\n",
			mpu_info->xhci_data_addr,
			mpu_info->xhci_data_size,
			sizeof(*mpu_info));

	// Send struct usb_audio_stream_info Address to Hifi3 Via IPI
	adsp_register_feature(OFFLOAD_FEATURE_ID);
	send_result = audio_send_ipi_msg(
					 &ipi_msg, TASK_SCENE_AUDIO_CONTROLLER_HIFI3_A,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_PAYLOAD,
					 AUDIO_IPI_MSG_NEED_ACK,
					 AUD_CTL_MSG_A2D_IPI_TEST,
					 sizeof(struct mpu_info_xhci),
					 0,
					 (char *)mpu_info);
	adsp_deregister_feature(OFFLOAD_FEATURE_ID);

	if (send_result != 0)
		USB_OFFLOAD_ERR("USB Offload init IPI msg send fail\n");
	else
		USB_OFFLOAD_INFO("USB Offload init IPI msg send succeed\n");

	return send_result;
}

int send_uas_ipi_msg_to_adsp(struct usb_audio_stream_msg *uas_msg)
{
	int send_result = 0;
	struct ipi_msg_t ipi_msg;

	USB_OFFLOAD_INFO("msg: %p, size: %d\n",
			uas_msg, sizeof(*uas_msg));

	// Send struct usb_audio_stream_info Address to Hifi3 Via IPI
	adsp_register_feature(OFFLOAD_FEATURE_ID);
	send_result = audio_send_ipi_msg(
					 &ipi_msg, TASK_SCENE_AUDIO_CONTROLLER_HIFI3_A,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_PAYLOAD,
					 AUDIO_IPI_MSG_NEED_ACK,
					 AUD_CTL_MSG_A2D_IPI_TEST,
					 sizeof(struct usb_audio_stream_msg),
					 0,
					 (char *)uas_msg);
	adsp_deregister_feature(OFFLOAD_FEATURE_ID);

	if (send_result != 0)
		USB_OFFLOAD_ERR("USB Offload uas IPI msg send fail\n");
	else
		USB_OFFLOAD_INFO("USB Offload uas ipi msg send succeed\n");

	return send_result;
}

static int usb_offload_enable_stream(struct usb_audio_stream_info *uainfo)
{
	struct usb_audio_stream_msg msg = {0};
	struct snd_usb_substream *subs;
	struct snd_pcm_substream *substream;
	struct snd_usb_audio *chip = NULL;
	struct intf_info *info;
	struct usb_host_endpoint *ep;

	u8 pcm_card_num, pcm_dev_num, direction;
	int info_idx = -EINVAL, datainterval = -EINVAL, ret = 0;
	int interface;

	direction = uainfo->direction;
	pcm_dev_num = uainfo->pcm_dev_num;
	pcm_card_num = uainfo->pcm_card_num;

	USB_OFFLOAD_INFO("direction: %d, pcm_dev_num: %d, pcm_card_num: %d\n",
			direction, pcm_dev_num, pcm_card_num);

	if (pcm_card_num >= SNDRV_CARDS) {
		USB_OFFLOAD_ERR("invalid card # %u", pcm_card_num);
		ret = -EINVAL;
		goto done;
	}

	if (!is_support_format(uainfo->audio_format)) {
		USB_OFFLOAD_ERR("unsupported pcm format received %d\n",
				uainfo->audio_format);
		ret = -EINVAL;
		goto done;
	}

	subs = find_snd_usb_substream(pcm_card_num, pcm_dev_num, direction,
					&chip, uaudio_disconnect_cb);
	if (!subs || !chip || atomic_read(&chip->shutdown)) {
		USB_OFFLOAD_ERR("can't find substream for card# %u, dev# %u, dir: %u\n",
				pcm_card_num, pcm_dev_num, direction);
		ret = -ENODEV;
		goto done;
	}

	mutex_lock(&chip->dev_lock);
	USB_OFFLOAD_INFO("inside mutex\n");

	if (subs->cur_audiofmt)
		interface = subs->cur_audiofmt->iface;
	else
		interface = -1;

	info_idx = info_idx_from_ifnum(pcm_card_num, interface,
		uainfo->enable);
	if (atomic_read(&chip->shutdown) || !subs->stream || !subs->stream->pcm
			|| !subs->stream->chip || !subs->pcm_substream) {
		ret = -ENODEV;
		mutex_unlock(&chip->dev_lock);
		goto done;
	}
	USB_OFFLOAD_INFO("info_idx: %d, interface: %d\n",
			info_idx, interface);

	if (uainfo->enable) {
		if (info_idx < 0) {
			USB_OFFLOAD_ERR("interface# %d already in use card# %d\n",
					interface, pcm_card_num);
			ret = -EBUSY;
			mutex_unlock(&chip->dev_lock);
			goto done;
		}
	}

	if (uainfo->service_interval_valid) {
		ret = get_data_interval_from_si(subs,
						uainfo->service_interval);
		if (ret == -EINVAL) {
			USB_OFFLOAD_ERR("invalid service interval %u\n",
					uainfo->service_interval);
			mutex_unlock(&chip->dev_lock);
			goto done;
		}

		datainterval = ret;
		USB_OFFLOAD_INFO("data interval %u\n", ret);
	}

	uadev[pcm_card_num].ctrl_intf = chip->ctrl_intf;

	if (!uainfo->enable) {
		info = &uadev[pcm_card_num].info[info_idx];
		if (info->data_ep_pipe) {
			ep = usb_pipe_endpoint(uadev[pcm_card_num].udev,
					info->data_ep_pipe);
			if (!ep)
				USB_OFFLOAD_ERR("no data ep\n");
			else
				USB_OFFLOAD_INFO("stop data ep\n");
			info->data_ep_pipe = 0;
		}

		if (info->sync_ep_pipe) {
			ep = usb_pipe_endpoint(uadev[pcm_card_num].udev,
					info->sync_ep_pipe);
			if (!ep)
				USB_OFFLOAD_ERR("no sync ep\n");
			else
				USB_OFFLOAD_INFO("stop sync ep\n");
			info->sync_ep_pipe = 0;
		}
	}

	substream = subs->pcm_substream;

	if (!substream->ops->hw_params || !substream->ops->hw_free
		|| !substream->ops->prepare) {

		USB_OFFLOAD_ERR("no hw_params/hw_free/prepare ops\n");
		ret = -ENODEV;
		mutex_unlock(&chip->dev_lock);
		goto done;
	}
	if (uainfo->enable) {
		ret = usb_offload_prepare_msg(subs, uainfo, &msg, info_idx);
		USB_OFFLOAD_INFO("prepare msg, ret: %d\n", ret);
		if (ret < 0) {
			mutex_unlock(&chip->dev_lock);
			return ret;
		}
	} else {
		ret = substream->ops->hw_free(substream);
		USB_OFFLOAD_INFO("hw_free, ret: %d\n", ret);

		msg.uainfo.direction = uainfo->direction;
	}
	msg.status = uainfo->enable ?
		USB_AUDIO_STREAM_REQ_START : USB_AUDIO_STREAM_REQ_STOP;

	/* write to audio ipi*/
	ret = send_uas_ipi_msg_to_adsp(&msg);
	USB_OFFLOAD_INFO("send_ipi_msg_to_adsp msg, ret: %d\n", ret);
	/* wait response */
	dump_uainfo(&msg.uainfo);

done:
	if (!uainfo->enable && ret != -EINVAL && ret != -ENODEV) {
		mutex_lock(&chip->dev_lock);
		if (info_idx >= 0) {
			info = &uadev[pcm_card_num].info[info_idx];
			usb_audio_dev_intf_cleanup(
									uadev[pcm_card_num].udev,
									info);
			USB_OFFLOAD_INFO("release resources: intf# %d card# %d\n",
					interface, pcm_card_num);
			}
			if (atomic_read(&uadev[pcm_card_num].in_use))
				kref_put(&uadev[pcm_card_num].kref, uaudio_dev_release);
			mutex_unlock(&chip->dev_lock);
	}

	return ret;
}

static struct usb_offload_dram usb_offload_dram_buffer[USB_OFFLOAD_SHARE_MEM_NUM];
static struct gen_pool *usb_offload_dram_pool[USB_OFFLOAD_SHARE_MEM_NUM];

static int dump_mtk_usb_offload_gen_pool(void)
{
	int i = 0;

	for (i = 0; i < USB_OFFLOAD_SHARE_MEM_NUM; i++) {
		USB_OFFLOAD_INFO("gen_pool_avail: %zu, gen_pool_size: %zu\n",
				gen_pool_avail(usb_offload_dram_pool[i]),
				gen_pool_size(usb_offload_dram_pool[i]));
	}
	return 0;
}

struct gen_pool *mtk_get_usb_offload_dram_gen_pool(int id)
{
	if (id >= USB_OFFLOAD_SHARE_MEM_NUM) {
		USB_OFFLOAD_INFO("id: %d\n", id);
		return NULL;
	}
	return usb_offload_dram_pool[id];
}
EXPORT_SYMBOL_GPL(mtk_get_usb_offload_dram_gen_pool);

static int mtk_init_usb_offload_sharemem(uint32_t mem_id, uint32_t buf_id)
{
	int ret = 0;

	if (!adsp_get_reserve_mem_phys(mem_id))
		return -EPROBE_DEFER;
	usb_offload_dram_buffer[buf_id].phy_addr = adsp_get_reserve_mem_phys(mem_id);
	usb_offload_dram_buffer[buf_id].va_addr =
			(unsigned long long) adsp_get_reserve_mem_virt(mem_id);
	usb_offload_dram_buffer[buf_id].vir_addr = adsp_get_reserve_mem_virt(mem_id);
	usb_offload_dram_buffer[buf_id].size = adsp_get_reserve_mem_size(mem_id);

	USB_OFFLOAD_INFO("mem_id(%u), buf_id(%u), phy_addr(0x%llx), vir_addr(%p)\n",
			mem_id, buf_id,
			usb_offload_dram_buffer[buf_id].phy_addr,
			usb_offload_dram_buffer[buf_id].vir_addr);
	USB_OFFLOAD_INFO("va_addr:(0x%llx), size(%llu)\n",
			usb_offload_dram_buffer[buf_id].va_addr,
			usb_offload_dram_buffer[buf_id].size);

	return ret;
}

static int mtk_usb_offload_gen_pool_create(int min_alloc_order, int nid)
{
	int i, ret = 0;
	unsigned long va_start;
	size_t va_chunk;

	if (min_alloc_order <= 0)
		return -1;

	USB_OFFLOAD_INFO("%d\n", __LINE__);

	for (i = 0; i < USB_OFFLOAD_SHARE_MEM_NUM; i++) {
		usb_offload_dram_pool[i] = gen_pool_create(min_alloc_order, -1);
		if (!usb_offload_dram_pool[i])
			return -ENOMEM;

		va_start = usb_offload_dram_buffer[i].va_addr;
		va_chunk = usb_offload_dram_buffer[i].size;
		if ((!va_start) || (!va_chunk)) {
			ret = -1;
			break;
		}
		if (gen_pool_add_virt(usb_offload_dram_pool[i], (unsigned long)va_start,
				usb_offload_dram_buffer[i].phy_addr, va_chunk, -1)) {
			USB_OFFLOAD_ERR("failed, va_start: 0x%lx, va_chunk: %zu\n",
					va_start, va_chunk);
		}

		USB_OFFLOAD_INFO("success, va_start: 0x%lx, va_chunk: %zu, dram_pool[%d]: %p\n",
					va_start, va_chunk, i, usb_offload_dram_pool[i]);
	}
	dump_mtk_usb_offload_gen_pool();
	return ret;
}

static int mtk_usb_offload_genpool_allocate_memory(unsigned char **vaddr,
		dma_addr_t *paddr,
		unsigned int size,
		int id,
		int align)
{
	/* gen pool related */
	struct gen_pool *gen_pool_usb_offload = mtk_get_usb_offload_dram_gen_pool(id);

	if (gen_pool_usb_offload == NULL) {
		USB_OFFLOAD_ERR("gen_pool_usb_offload == NULL\n");
		return -1;
	}

	/* allocate VA with gen pool */
	if (*vaddr == NULL) {
		*vaddr = (unsigned char *)gen_pool_dma_zalloc_align(gen_pool_usb_offload,
									size, paddr, align);
		*paddr = gen_pool_virt_to_phys(gen_pool_usb_offload,
					(unsigned long)*vaddr);
	}
	USB_OFFLOAD_INFO("size: %u, id: %d, vaddr: %p, DMA paddr: 0x%llx\n",
			size, id, vaddr, (unsigned long long)*paddr);

	return 0;
}

static int mtk_usb_offload_genpool_free_memory(unsigned char **vaddr,
		size_t *size, int id)
{
	/* gen pool related */
	struct gen_pool *gen_pool_usb_offload = mtk_get_usb_offload_dram_gen_pool(id);

	if (gen_pool_usb_offload == NULL) {
		USB_OFFLOAD_ERR("gen_pool_usb_offload == NULL\n");
		return -1;
	}

	if (!gen_pool_has_addr(gen_pool_usb_offload, (unsigned long)*vaddr, *size)) {
		USB_OFFLOAD_ERR("vaddr is not in genpool\n");
		return -1;
	}

	/* allocate VA with gen pool */
	if (*vaddr) {
		gen_pool_free(gen_pool_usb_offload, (unsigned long)*vaddr, *size);
		*vaddr = NULL;
		*size = 0;
	}

	return 0;
}

int mtk_usb_offload_allocate_mem(struct usb_offload_buffer *buf,
		unsigned int size, int align)
{
	int ret = 0;

	if (buf->dma_area) {
		ret = mtk_usb_offload_genpool_free_memory(
					&buf->dma_area,
					&buf->dma_bytes,
					USB_OFFLOAD_SHARE_MEM_ID);
	}
	ret =  mtk_usb_offload_genpool_allocate_memory
				(&buf->dma_area,
				 &buf->dma_addr,
				 size,
				 USB_OFFLOAD_SHARE_MEM_ID,
				 align);

	if (!ret) {
		buf->dma_bytes = size;
		buf->allocated = true;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_usb_offload_allocate_mem);

int mtk_usb_offload_free_mem(struct usb_offload_buffer *buf)
{
	int ret = 0;

	ret = mtk_usb_offload_genpool_free_memory(
				&buf->dma_area,
				&buf->dma_bytes,
				USB_OFFLOAD_SHARE_MEM_ID);

	if (!ret) {
		buf->dma_addr = 0;
		buf->allocated = 0;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_usb_offload_free_mem);

static bool xhci_mtk_is_usb_offload_enabled(struct xhci_hcd *xhci,
					   struct xhci_virt_device *vdev,
					   unsigned int ep_index)
{
	return true;
}

static struct xhci_device_context_array *xhci_mtk_alloc_dcbaa(struct xhci_hcd *xhci,
						 gfp_t flags)
{
	struct xhci_device_context_array *xhci_ctx = xhci->dcbaa;

	buf_dcbaa = kzalloc(sizeof(struct usb_offload_buffer), GFP_KERNEL);
	if (mtk_usb_offload_allocate_mem(buf_dcbaa, sizeof(*xhci_ctx), 64)) {
		USB_OFFLOAD_ERR("FAIL to allocate mem for USB Offload DCBAA\n");
		return NULL;
	}

	USB_OFFLOAD_INFO("size of dcbaa: %d\n", sizeof(*xhci_ctx));
	xhci_ctx = (struct xhci_device_context_array *) buf_dcbaa->dma_area;
	xhci_ctx->dma = buf_dcbaa->dma_addr;
	USB_OFFLOAD_INFO("xhci_ctx.dev_context_ptrs:%p xhci_ctx.dma:%llx\n",
			xhci_ctx->dev_context_ptrs, xhci_ctx->dma);

	buf_ctx = kzalloc(sizeof(struct usb_offload_buffer) * BUF_CTX_SIZE, GFP_KERNEL);
	buf_seg = kzalloc(sizeof(struct usb_offload_buffer) * BUF_SEG_SIZE, GFP_KERNEL);
	return xhci_ctx;
}

static void xhci_mtk_free_dcbaa(struct xhci_hcd *xhci)
{
	if (!buf_dcbaa) {
		USB_OFFLOAD_ERR("DCBAA has not been initialized.\n");
		return;
	}

	if (mtk_usb_offload_free_mem(buf_dcbaa))
		USB_OFFLOAD_ERR("FAIL to free mem for USB Offload DCBAA\n");
	else
		USB_OFFLOAD_INFO("Free mem DCBAA DONE\n");
}

static int get_first_avail_buf_ctx_idx(struct xhci_hcd *xhci)
{
	unsigned int idx;

	for (idx = 0; idx <= BUF_CTX_SIZE; idx++) {
		USB_OFFLOAD_INFO("idx: %d, alloc: %d, DMA area: %p, addr: %llx, bytes: %d\n",
					idx,
					buf_ctx[idx].allocated,
					buf_ctx[idx].dma_area,
					buf_ctx[idx].dma_addr,
					buf_ctx[idx].dma_bytes);

		if (!buf_ctx[idx].allocated)
			return idx;
	}
	USB_OFFLOAD_ERR("NO Available BUF Context.\n");
	return 0;
}

static void xhci_mtk_alloc_container_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx,
				int type, gfp_t flags)
{

	int buf_ctx_slot = get_first_avail_buf_ctx_idx(xhci);

	if (mtk_usb_offload_allocate_mem(&buf_ctx[buf_ctx_slot], ctx->size, 64)) {
		USB_OFFLOAD_ERR("FAIL to allocate mem for USB Offload Context %d size: %d\n",
				buf_ctx_slot, ctx->size);
		return;
	}
	USB_OFFLOAD_INFO("Success allocated mem for USB Offload Context %d\n", buf_ctx_slot);

	ctx->bytes = buf_ctx[buf_ctx_slot].dma_area;
	ctx->dma = buf_ctx[buf_ctx_slot].dma_addr;
	USB_OFFLOAD_INFO("ctx.bytes: %p, ctx.dma: %llx, ctx.size: %d\n",
			ctx->bytes, ctx->dma, ctx->size);
}

static void xhci_mtk_free_container_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx)
{
	unsigned int idx;

	for (idx = 0; idx < BUF_CTX_SIZE; idx++) {
		USB_OFFLOAD_INFO("ctx[%d], alloc: %d, dma_addr: %llx, dma: %llx\n",
				idx,
				buf_ctx[idx].allocated,
				buf_ctx[idx].dma_addr,
				ctx->dma);

		if (buf_ctx[idx].allocated && buf_ctx[idx].dma_addr == ctx->dma) {
			if (mtk_usb_offload_free_mem(&buf_ctx[idx]))
				USB_OFFLOAD_ERR("FAIL: free mem ctx: %d\n", idx);
			else
				USB_OFFLOAD_INFO("Free mem ctx: %d DONE\n", idx);
			return;
		}
	}
	USB_OFFLOAD_INFO("NO Context MATCH to be freed. ctx.bytes:%p ctx.dma:%llx ctx.size: %d\n",
			ctx->bytes,
			ctx->dma,
			ctx->size);
}

static int get_first_avail_buf_seg_idx(struct xhci_hcd *xhci)
{
	unsigned int idx;

	for (idx = 0; idx < BUF_SEG_SIZE; idx++) {
		USB_OFFLOAD_INFO("seg[%d], alloc: %d, DMA area: %p, addr: %llx, bytes: %d\n",
				idx,
				buf_seg[idx].allocated,
				buf_seg[idx].dma_area,
				buf_seg[idx].dma_addr,
				buf_seg[idx].dma_bytes);

		if (!buf_seg[idx].allocated)
			return idx;
	}
	USB_OFFLOAD_ERR("NO Available BUF Segment.\n");
	return 0;
}

static void xhci_mtk_usb_offload_segment_free(struct xhci_hcd *xhci, struct xhci_segment *seg)
{
	unsigned int idx;

	if (seg->trbs) {
		for (idx = 0; idx < BUF_SEG_SIZE; idx++) {
			USB_OFFLOAD_INFO("seg[%d], alloc: %d, dma_addr: %llx, dma: %llx\n",
					idx,
					buf_seg[idx].allocated,
					buf_seg[idx].dma_addr,
					seg->dma);

			if (buf_seg[idx].allocated && buf_seg[idx].dma_addr == seg->dma) {
				if (mtk_usb_offload_free_mem(&buf_seg[idx]))
					USB_OFFLOAD_ERR("FAIL: free mem seg: %d\n", idx);
				else
					USB_OFFLOAD_INFO("Free mem seg: %d DONE\n", idx);
				return;
			}
		}
		USB_OFFLOAD_INFO("NO Segment MATCH to be freed. seg->trbs: %p, seg->dma: %llx\n",
				seg->trbs, seg->dma);
		seg->trbs = NULL;
	}
	kfree(seg->bounce_buf);
	kfree(seg);
}

static void xhci_mtk_usb_offload_free_segments_for_ring(struct xhci_hcd *xhci,
				struct xhci_segment *first)
{
	struct xhci_segment *seg;

	seg = first->next;
	while (seg != first) {
		struct xhci_segment *next = seg->next;

		xhci_mtk_usb_offload_segment_free(xhci, seg);
		seg = next;
	}
	xhci_mtk_usb_offload_segment_free(xhci, first);
}

/*
 * Allocates a generic ring segment from the ring pool, sets the dma address,
 * initializes the segment to zero, and sets the private next pointer to NULL.
 *
 * Section 4.11.1.1:
 * "All components of all Command and Transfer TRBs shall be initialized to '0'"
 */
static struct xhci_segment *xhci_mtk_usb_offload_segment_alloc(struct xhci_hcd *xhci,
						   unsigned int cycle_state,
						   unsigned int max_packet,
						   gfp_t flags)
{
	struct xhci_segment *seg;
	dma_addr_t	dma;
	int		i;
	int buf_seg_slot = get_first_avail_buf_seg_idx(xhci);

	seg = kzalloc(sizeof(*seg), flags);
	if (!seg)
		return NULL;

	if (mtk_usb_offload_allocate_mem(&buf_seg[buf_seg_slot],
		USB_OFFLOAD_TRB_SEGMENT_SIZE, USB_OFFLOAD_TRB_SEGMENT_SIZE)) {
		USB_OFFLOAD_ERR("FAIL to allocate mem for USB Offload Segment %d, size: %d\n",
				buf_seg_slot, USB_OFFLOAD_TRB_SEGMENT_SIZE);
		return NULL;
	}
	USB_OFFLOAD_INFO("Success allocated mem for USB Offload Segment %d\n", buf_seg_slot);

	seg->trbs = (void *) buf_seg[buf_seg_slot].dma_area;
	dma = buf_seg[buf_seg_slot].dma_addr;
	USB_OFFLOAD_INFO("seg->trbs: %p, dma: %llx, size: %d\n",
			seg->trbs,
			dma,
			sizeof(buf_seg[buf_seg_slot]));

	if (!seg->trbs) {
		USB_OFFLOAD_ERR("No seg->trbs\n");
		kfree(seg);
		return NULL;
	}

	if (max_packet) {
		seg->bounce_buf = kzalloc(max_packet, flags);
		if (!seg->bounce_buf) {
			xhci_mtk_usb_offload_segment_free(xhci, seg);
			kfree(seg);
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
		unsigned int num_segs, unsigned int cycle_state,
		enum xhci_ring_type type, unsigned int max_packet, gfp_t flags)
{
	struct xhci_segment *prev;
	bool chain_links;

	USB_OFFLOAD_INFO("ring->first_seg: %p, ring->last_seg: %p\n",
			first, last);
	USB_OFFLOAD_INFO("num_segs: %d, cycle_state: %d, ring_type: %d, max_packet: %d\n",
			num_segs, cycle_state, type, max_packet);

	/* Set chain bit for 0.95 hosts, and for isoc rings on AMD 0.96 host */
	chain_links = !!(xhci_link_trb_quirk(xhci) ||
			 (type == TYPE_ISOC &&
			  (xhci->quirks & XHCI_AMD_0x96_HOST)));

	prev = xhci_mtk_usb_offload_segment_alloc(xhci, cycle_state, max_packet, flags);
	if (!prev)
		return -ENOMEM;
	num_segs--;

	*first = prev;
	while (num_segs > 0) {
		struct xhci_segment	*next;

		next = xhci_mtk_usb_offload_segment_alloc(xhci, cycle_state, max_packet, flags);
		if (!next) {
			prev = *first;
			while (prev) {
				next = prev->next;
				xhci_mtk_usb_offload_segment_free(xhci, prev);
				prev = next;
			}
			return -ENOMEM;
		}
		xhci_link_segments(prev, next, type, chain_links);

		prev = next;
		num_segs--;
	}
	xhci_link_segments(prev, *first, type, chain_links);
	*last = prev;

	return 0;
}

static struct xhci_ring *xhci_mtk_alloc_transfer_ring(struct xhci_hcd *xhci,
		u32 endpoint_type, enum xhci_ring_type ring_type,
		unsigned int max_packet, gfp_t mem_flags)
{


	struct xhci_ring	*ring;
	int ret;
	int num_segs = 2;
	int cycle_state = 1;

	ring = kzalloc(sizeof(*ring), mem_flags);
	if (!ring)
		return NULL;

	ring->num_segs = num_segs;
	ring->bounce_buf_len = max_packet;
	INIT_LIST_HEAD(&ring->td_list);
	ring->type = ring_type;
	if (num_segs == 0)
		return ring;

	ret = xhci_mtk_usb_offload_alloc_segments_for_ring(xhci, &ring->first_seg,
			&ring->last_seg, num_segs, cycle_state, ring_type,
			max_packet, mem_flags);
	if (ret) {
		USB_OFFLOAD_ERR("Fail to alloc segment for rings\n");
		goto fail;
	}

	if (ring_type != TYPE_EVENT) {
		/* See section 4.9.2.1 and 6.4.4.1 */
		ring->last_seg->trbs[TRBS_PER_SEGMENT - 1].link.control |=
			cpu_to_le32(LINK_TOGGLE);
	}
	xhci_initialize_ring_info(ring, cycle_state);
	//trace_xhci_ring_alloc(ring);
	return ring;

fail:
	kfree(ring);
	return NULL;
}

static void xhci_mtk_free_transfer_ring(struct xhci_hcd *xhci,
		struct xhci_ring *ring, unsigned int ep_index)
{
	if (!ring)
		return;

	if (ring->first_seg)
		xhci_mtk_usb_offload_free_segments_for_ring(xhci, ring->first_seg);

	kfree(ring);
}

static int usb_offload_open(struct inode *ip, struct file *fp)
{
	struct device_node *node_xhci_host;
	struct platform_device *pdev_xhci_host = NULL;
	struct xhci_hcd_mtk *mtk;
	struct xhci_hcd *xhci;
	struct usb_device *udev;
	int err = 0;
	int i;
	int desc_class = -1;

	USB_OFFLOAD_INFO("%d\n", __LINE__);

	node_xhci_host = of_parse_phandle(uodev->dev->of_node, "xhci_host", 0);
	if (node_xhci_host) {
		pdev_xhci_host = of_find_device_by_node(node_xhci_host);
		if (!pdev_xhci_host)
			return -ENODEV;
		of_node_put(node_xhci_host);

		mtk = platform_get_drvdata(pdev_xhci_host);
		if (!mtk)
			return -ENODEV;
		xhci = hcd_to_xhci(mtk->hcd);
	} else {
		USB_OFFLOAD_ERR("No 'xhci_host' node, NOT SUPPORT USB Offload!\n");
		err = -EINVAL;
		goto GET_OF_NODE_FAIL;
	}

	for (i = 0; i <= 2; i++) {
		if (xhci->devs[i] != NULL)
			USB_OFFLOAD_INFO("xhci->devs[%d]->udev->descriptor.bDeviceClass: 0x%x\n",
					i, xhci->devs[i]->udev->descriptor.bDeviceClass);
	}

	if (xhci->devs[2] != NULL) {
		USB_OFFLOAD_INFO("Multiple Devices - NOT SUPPORT USB OFFLOAD!!\n");
		return -1;
	}

	if (xhci->devs[1] != NULL) {
		udev = xhci->devs[1]->udev;
		desc_class = udev->descriptor.bDeviceClass;
		USB_OFFLOAD_INFO("Single Device - bDeviceClass: 0x%x\n", desc_class);

		if (desc_class == 0x00 || desc_class == 0xef) {
			desc_class =
				udev->config->interface[0]->cur_altsetting->desc.bInterfaceClass;
			USB_OFFLOAD_INFO("Single Device - bInterfaceClass: 0x%x\n", desc_class);
		}

		if (desc_class == 0x01) {
			USB_OFFLOAD_INFO("Single UAC Device - SUPPORT USB OFFLOAD!!\n");
			return 0;
		}
		USB_OFFLOAD_INFO("Single Device - Not UAC Device. NOT SUPPORT USB OFFLOAD!!\n");
	}
GET_OF_NODE_FAIL:
	return -1;
}

static int usb_offload_release(struct inode *ip, struct file *fp)
{
	USB_OFFLOAD_INFO("%d\n", __LINE__);
	return 0;
}

static long usb_offload_ioctl(struct file *fp,
	unsigned int cmd, unsigned long value)
{
	long ret = 0;
	struct usb_audio_stream_info uainfo;
	struct mpu_info_xhci *xhci_rsv_mem;

	USB_OFFLOAD_INFO("cmd: %d\n", cmd);

	switch (cmd) {
	case USB_OFFLOAD_INIT_ADSP:
		USB_OFFLOAD_INFO("USB_OFFLOAD_INIT_ADSP: %ld\n", value);
		xhci_rsv_mem = kzalloc(sizeof(*xhci_rsv_mem), GFP_KERNEL);
		if (value == 1) {
			xhci_rsv_mem->xhci_data_addr =
				usb_offload_dram_buffer[USB_OFFLOAD_SHARE_MEM_ID].phy_addr;
			xhci_rsv_mem->xhci_data_size =
				usb_offload_dram_buffer[USB_OFFLOAD_SHARE_MEM_ID].size;
		} else {
			xhci_rsv_mem->xhci_data_addr = 0;
			xhci_rsv_mem->xhci_data_size = 0;
		}
		ret = send_init_ipi_msg_to_adsp(xhci_rsv_mem);
		if (ret)
			ret = -1;
		break;
	case USB_OFFLOAD_ENABLE_STREAM:
	case USB_OFFLOAD_DISABLE_STREAM:
		USB_OFFLOAD_INFO("USB_OFFLOAD_ENABLE_STREAM / USB_OFFLOAD_DISABLE_STREAM\n");
		if (copy_from_user(&uainfo, (void __user *)value, sizeof(uainfo))) {
			ret = -EFAULT;
			goto fail;
		}
		dump_uainfo(&uainfo);

		ret = usb_offload_enable_stream(&uainfo);
		if (ret)
			ret = -1;
		break;
	}
fail:
	USB_OFFLOAD_INFO("ioctl returning, ret: %d\n", ret);
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
};

static int usb_offload_probe(struct platform_device *pdev)
{
	struct device_node *node_xhci_host;
	int ret = 0;

	uodev = devm_kzalloc(&pdev->dev, sizeof(struct usb_offload_dev),
		GFP_KERNEL);
	if (!uodev)
		return -ENOMEM;

	uodev->dev = &pdev->dev;

	node_xhci_host = of_parse_phandle(uodev->dev->of_node, "xhci_host", 0);
	if (node_xhci_host) {
		USB_OFFLOAD_INFO("Set XHCI vendor hook ops\n");
		platform_set_drvdata(pdev, &xhci_mtk_vendor_ops);

		ret = mtk_init_usb_offload_sharemem(ADSP_XHCI_MEM_ID, USB_OFFLOAD_SHARE_MEM_ID);
		if (ret == -EPROBE_DEFER)
			goto INIT_SHAREMEM_FAIL;
		ret = mtk_usb_offload_gen_pool_create(MIN_USB_OFFLOAD_SHIFT, -1);

		ret = misc_register(&usb_offload_device);
	} else
		USB_OFFLOAD_ERR("No 'xhci_host' node, NOT support USB_OFFLOAD\n");

INIT_SHAREMEM_FAIL:
	of_node_put(node_xhci_host);
	return ret;
}

static int usb_offload_remove(struct platform_device *pdev)
{
	return 0;
}

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
		.of_match_table = of_match_ptr(usb_offload_of_match),
	},
};
module_platform_driver(usb_offload_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek USB Offload Driver");
