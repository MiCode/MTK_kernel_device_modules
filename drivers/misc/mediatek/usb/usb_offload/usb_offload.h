/* SPDX-License-Identifier: GPL-2.0 */
/*
 * MTK USB Offload Driver
 * *
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Jeremy Chou <jeremy.chou@mediatek.com>
 */

#ifndef __USB_OFFLOAD_H__
#define __USB_OFFLOAD_H__

#include <linux/types.h>
#include <sound/asound.h>

#include <linux/module.h>
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
#include <linux/usb.h>
#include <linux/iommu.h>
#include <linux/dma-mapping.h>

#define MIN_USB_OFFLOAD_SHIFT (8)
#define MIN_USB_OFFLOAD_POOL_SIZE (1 << MIN_USB_OFFLOAD_SHIFT)

#define BUF_DCBAA_SIZE					1
#define BUF_CTX_SIZE					31
#define BUF_ERST_SIZE					1
#define BUF_EV_RING_SIZE				1
#define BUF_TR_RING_SIZE				((15 * 2 + 1) * 2)
#define BUF_URB_SIZE					5 /* hid*1, stream*2, trace*2 */

#define ERST_SIZE						16
#define USB_OFFLOAD_TRBS_PER_SEGMENT	256
#define USB_OFFLOAD_TRB_SEGMENT_SIZE	(USB_OFFLOAD_TRBS_PER_SEGMENT*16)
#define XHCI1_INTR_TARGET	1

struct uo_provider;

/* dram provider, main sram provider, secondary sram provider */
enum uo_provider_type {
	UO_PROV_DRAM = 0,
	UO_PROV_SRAM = 1,
	UO_PROV_SRAM_2 = 2,
	UO_PROV_NUM,
};

/* dram source, afe sram source, usb sram source */
enum uo_source_type {
	UO_SOURCE_DRAM,
	UO_SOURCE_USB_SRAM,
	UO_SOURCE_AFE_SRAM,
	UO_SOURCE_NUM,
};

enum uo_struct {
    UO_STRUCT_DCBAA,
    UO_STRUCT_CTX,
    UO_STRUCT_ERST,
    UO_STRUCT_EVRING,
    UO_STRUCT_TRRING,
    UO_STRUCT_URB,
    UO_STRUCT_NUM,
};

struct uo_rsv_region {
	dma_addr_t physical;
	void *virtual;
	unsigned long long size;
	bool is_valid;
	struct gen_pool *pool;
};

struct uo_provider_ops {
	/* init provider */
	int (*init)(struct uo_provider *itself);

	/* request region from allocated part */
	void *(*alloc_dyn)(struct uo_provider *itself, dma_addr_t *phy, unsigned int size, int align);

	/* release region fron allocated part */
	int (*free_dyn)(struct uo_provider *itself, dma_addr_t addr);

	/* init reserved part */
	int (*init_rsv)(struct uo_provider *itself,	unsigned int size, int min_order);

	/* deinit reserved part */
	int (*deinit_rsv)(struct uo_provider *itself);

	/* request region from reserved part */
	void *(*alloc_rsv)(struct uo_provider *itself, dma_addr_t *phy, unsigned int size, int align);

	/* release region from reserved part */
	int (*free_rsv)(struct uo_provider *itself, void *vir, unsigned int size);

	/* get region which should be set to uncacheable */
	unsigned int (*mpu_region)(struct uo_provider *itself, dma_addr_t *phys);

	/* control power of provider */
	int (*power_ctrl)(struct uo_provider *itself, bool is_on);

	/* get provider name*/
	char *(*get_name)(void);
};

struct uo_provider {
	struct device *dev;
	enum uo_provider_type id;
	enum uo_source_type type;
	bool is_init;
	u32 struct_cnt;
	bool power;
	struct uo_rsv_region rsv_region;
	struct uo_provider_ops ops;
};

struct uo_buffer {
	struct uo_provider *provider;
	void *virt;
	dma_addr_t phys;
	size_t size;
	bool allocated;
	bool is_rsv;
	enum uo_struct type;
};

struct uo_buffer_array {
	struct uo_buffer *first_buf;
	int length;
};

enum {
	ENABLE_STREAM,
	DISABLE_STREAM,
};
#define USB_OFFLOAD_IOC_MAGIC 'U'
/* Enable/Disable USB Offload */
#define USB_OFFLOAD_INIT_ADSP		_IOW(USB_OFFLOAD_IOC_MAGIC, 0, int)
/* Enable USB Stream */
#define USB_OFFLOAD_ENABLE_STREAM	_IOW(USB_OFFLOAD_IOC_MAGIC, 1, unsigned int)
/* Disable USB Stream */
#define USB_OFFLOAD_DISABLE_STREAM	_IOW(USB_OFFLOAD_IOC_MAGIC, 2, unsigned int)

#define BUS_INTERVAL_FULL_SPEED 1000 /* in us */
#define BUS_INTERVAL_HIGHSPEED_AND_ABOVE 125 /* in us */
#define MAX_BINTERVAL_ISOC_EP 16
#define DEV_RELEASE_WAIT_TIMEOUT 10000 /* in ms */

enum usb_audio_stream_status {
	USB_AUDIO_STREAM_STATUS_ENUM_MIN_VAL = INT_MIN,
	USB_AUDIO_STREAM_REQ_START = 0,
	USB_AUDIO_STREAM_REQ_STOP = 1,
	USB_AUDIO_STREAM_STATUS_ENUM_MAX_VAL = INT_MAX,
};

enum usb_audio_device_speed {
	USB_AUDIO_DEVICE_SPEED_ENUM_MIN_VAL = INT_MIN,
	USB_AUDIO_DEVICE_SPEED_INVALID = 0,
	USB_AUDIO_DEVICE_SPEED_LOW = 1,
	USB_AUDIO_DEVICE_SPEED_FULL = 2,
	USB_AUDIO_DEVICE_SPEED_HIGH = 3,
	USB_AUDIO_DEVICE_SPEED_SUPER = 4,
	USB_AUDIO_DEVICE_SPEED_SUPER_PLUS = 5,
	USB_AUDIO_DEVICE_SPEED_ENUM_MAX_VAL = INT_MAX,
};

struct mem_info_xhci {
	bool adv_lowpwr;
	unsigned long long rsv_dram_addr;
	unsigned int rsv_dram_size;
	unsigned long long rsv_sram_addr;
	unsigned int rsv_sram_size;
};

struct usb_audio_stream_info {
	unsigned char enable;
	unsigned char pcm_card_num;
	unsigned char pcm_dev_num;
	unsigned char direction;

	unsigned int bit_depth;
	unsigned int number_of_ch;
	unsigned int bit_rate;

	unsigned char service_interval_valid;
	unsigned int service_interval;
	unsigned char xhc_irq_period_ms;
	unsigned char xhc_urb_num;
	unsigned char dram_size;
	unsigned char dram_cnt;
	unsigned char start_thld;
	unsigned char stop_thld;
	unsigned int pcm_size;

	snd_pcm_format_t audio_format;
};

struct usb_endpoint_info {
	struct usb_endpoint_descriptor desc;
	unsigned long long urb_start_addr;
	unsigned int urb_size;
	unsigned int urb_num;
	unsigned int urb_packs;
};

#define STREAM_FLAG_DATA_EP	  (0x1U << 0)
#define STREAM_FLAG_SYNC_EP	  (0x1U << 1)
#define STREAM_FLAG_XHCI_HALT (0x1U << 2)

struct usb_audio_stream_msg {
	unsigned char flag;
	enum usb_audio_stream_status status;
	unsigned int slot_id;
	unsigned char direction;
	struct usb_interface_descriptor std_as_opr_intf_desc;
	struct usb_endpoint_info data_ep_info;
	struct usb_endpoint_info sync_ep_info;
	u16 usb_audio_spec_revision;
	unsigned char data_path_delay;
	unsigned char usb_audio_subslot_size;
	enum usb_audio_device_speed speed_info;
	unsigned char controller_num;
	struct usb_audio_stream_info uainfo;
};

#define HID_FLAG_XHCI_HALT  (0x1U <<0)

struct usb_offload_urb_msg {
	unsigned char flag;
	unsigned char enable;
	unsigned char direction;
	unsigned int slot_id;
	struct usb_interface_descriptor intf_desc;
	struct usb_endpoint_descriptor ep_desc;
	unsigned long long urb_start_addr;
	unsigned int urb_size;
	unsigned long long first_trb;
	unsigned char cycle_state;
};

struct usb_offload_urb_complete {
	unsigned char direction;
	unsigned int slot_id;
	unsigned int ep_id;
	unsigned long long urb_start_addr;
	unsigned int actual_length;
	unsigned char more_complete;
	int status;
};

struct usb_offload_xhci_ep {
	unsigned char direction;
	unsigned int slot_id;
	unsigned int ep_id;
	unsigned long long cur_trb;
	unsigned char cycle_state;
};

struct usb_trace_msg {
	unsigned char enable;
	unsigned char disable_all;
	unsigned char direction;
	unsigned long long buffer;
	unsigned int size;
};

struct intf_info {
	unsigned int data_ep_pipe;
	unsigned int sync_ep_pipe;
	u8 intf_num;
	u8 pcm_card_num;
	u8 pcm_dev_num;
	u8 direction;
	bool in_use;

	/* urb in adsp/ap view */
	struct uo_buffer *dsp_urb;

	/* synchronization for disable stream*/
	atomic_t disable_sync;
	struct mutex lock;
};

struct usb_audio_dev {
	struct usb_device *udev;
	/* audio control interface */
	struct usb_host_interface *ctrl_intf;
	unsigned int card_num;
	atomic_t in_use;
	struct kref kref;
	int slot_id;

	/* interface specific */
	int num_intf;
	struct intf_info *info;

	struct snd_usb_audio *chip;
	atomic_t connected;

	wait_queue_head_t disabling_wq;

	/* xhci sideband */
	struct xhci_sideband_ *sb;
	struct usb_interface *sb_intf;

	bool is_valid;
	bool on_hub;
};

struct usb_offload_policy {
	bool adv_lowpwr;
	bool adv_lowpwr_dl_only;
	bool force_on_secondary;
	bool support_fb;
	bool support_hub;
	bool hid_disable_offload;
	bool hid_disable_sync;
	bool hid_tr_switch;
	bool support_idle_lowpwr;
	bool all_on_sram;
	bool ready_for_xhci;
	int smc_suspend;
	int smc_resume;
	enum uo_source_type main_sram;
	enum uo_source_type secondary_sram;
	u32 reserved_size;
};

struct usb_offload_dev {
	struct device *dev;
	int last_card_num;
	struct xhci_hcd *xhci;
	int total_connected;
	bool adv_lowpwr;
	bool is_streaming;
	bool tx_streaming;
	bool rx_streaming;
	bool adsp_inited;
	bool adsp_ready;
	enum usb_device_speed speed;
	bool hub_offloading;
	struct ssusb_offload *ssusb_offload_notify;
	void *tracer;
	struct uo_provider provider[UO_PROV_NUM];
	struct uo_buffer_array buf_array[UO_STRUCT_NUM];
	struct usb_offload_policy policy;

	/* interrupter */
	struct xhci_interrupter *ir;
	struct mutex ir_lock;

	/* driver stage sync */
	spinlock_t dev_lock;
	unsigned long stage;
};

extern int ssusb_offload_register(struct ssusb_offload *offload);
extern int ssusb_offload_unregister(struct ssusb_offload *offload);

extern bool usb_offload_ready(void);

extern struct usb_offload_dev *uodev;
extern unsigned int usb_offload_log;
#define USB_OFFLOAD_DBG(fmt, args...) do { \
	if (usb_offload_log > 0) \
		pr_info("UD, %s(%d) " fmt, __func__, __LINE__, ## args); \
	} while (0)
extern unsigned int debug_memory_log;
#define USB_OFFLOAD_MEM_DBG(fmt, args...) do { \
	if (debug_memory_log > 0) \
		pr_info("UD, %s(%d) " fmt, __func__, __LINE__, ## args); \
	} while (0)

#define USB_OFFLOAD_INFO(fmt, args...) do { \
	if (1) \
		pr_info("UO, %s(%d) " fmt, __func__, __LINE__, ## args); \
	} while (0)

#define USB_OFFLOAD_PROBE(fmt, args...) do { \
	if (1) \
		pr_info("UD, %s(%d) " fmt, __func__, __LINE__, ## args); \
	} while (0)

#define USB_OFFLOAD_ERR(fmt, args...) do { \
	if (1) \
		pr_info("UD, %s(%d) " fmt, __func__, __LINE__, ## args); \
	} while (0)

int xhci_mtk_realloc_transfer_ring(unsigned int slot_id, unsigned int ep_id,
	enum uo_provider_type id, bool is_rsv);

#define wait_condition(condition, timeout) ({ struct timespec64 ref, cur;\
	ktime_get_ts64(&ref); \
	cur = ref; \
	while (!condition && (cur.tv_nsec - ref.tv_nsec) < timeout) { \
		mdelay(1); \
		ktime_get_ts64(&cur); \
	} \
	(condition) ? 0 : -ETIMEDOUT; \
})

/****
 * mmemory manager exported api
 ****/
u32 mtk_offload_get_cnt(enum uo_provider_type id);
bool mtk_offload_provider_is_valid(enum uo_provider_type id);
int mtk_offload_provider_register(struct usb_offload_dev *udev, enum uo_provider_type id);
u32 mtk_offload_provider_get_cnt(enum uo_provider_type id);
int mtk_offload_init_rsv(struct usb_offload_dev *udev, enum uo_provider_type id);
void mtk_offload_deinit_rsv(enum uo_provider_type id);
unsigned int mtk_offload_get_mpu_region(enum uo_provider_type id, dma_addr_t *phys);
void mtk_offload_provider_power(enum uo_provider_type id, bool is_on);
int mtk_offload_alloc_mem(struct uo_buffer *buf, unsigned int size,
	int align, enum uo_provider_type id, enum uo_struct type, bool is_rsv);
int mtk_offload_free_mem(struct uo_buffer *buf);
char *mtk_offload_parse_buffer(struct uo_buffer *buf);
char *mtk_offload_provider_parse_count(enum uo_provider_type id);
bool mtk_offload_hold_apsrc(void);
bool mtk_offload_hold_vcore(void);

/****
 * buffer array management api
 ****/
struct uo_buffer *uob_get_empty(enum uo_struct type);
struct uo_buffer *uob_get_first(enum uo_struct type);
struct uo_buffer *uob_search(enum uo_struct type, dma_addr_t phy);
void uob_assign_array(enum uo_struct type, void *first_buffer, int length);
int uob_init(enum uo_struct type);
void uob_deinit(enum uo_struct type);

/****
 * provider access & control api
 ****/
int uop_register(struct device *dev, struct uo_provider *provider,
	enum uo_provider_type type, struct uo_provider_ops *ops);
int uop_init(struct uo_provider *provider);
void *uop_alloc_dyn(struct uo_provider *provider, dma_addr_t *phy, unsigned int size, int align);
int uop_free_dyn(struct uo_provider *provider, dma_addr_t phy_addr);
int uop_pwr_ctrl(struct uo_provider *provider, bool is_on);
int uop_init_rsv(struct uo_provider *provider, unsigned int size, int min_order);
int uop_deinit_rsv(struct uo_provider *provider);
void *uop_alloc_rsv(struct uo_provider *provider, dma_addr_t *phy, unsigned int size, int align);
int uop_free_rsv(struct uo_provider *provider, void *vir, unsigned int size);
unsigned int uop_mpu_region(struct uo_provider *provider, dma_addr_t *phy);
char *uop_get_name(struct uo_provider *provider);
char *uo_struct_name(enum uo_struct type);
u32 uo_get_cnt_power_sensitive(struct uo_provider *provider);
int uop_increase_cnt(struct uo_provider *provider, enum uo_struct type);
void uop_decrease_cnt(struct uo_provider *provider,	enum uo_struct type);
char *uo_provider_parse_count(struct uo_provider *provider);
int mtk_register_usb_sram_ops(
	void *(*allocate)(dma_addr_t *phys_addr, unsigned int size, int align),
	int (*free)(dma_addr_t phys_addr));
/****
 * provider instance
 ****/
extern struct uo_provider_ops uo_dram_ops;
extern struct uo_provider_ops uo_afe_sram_ops;
extern struct uo_provider_ops uo_usb_sram_ops;

/****
 * generic api of reserved region
 ****/
void uo_rst_rsv_region(struct uo_rsv_region *rsv_region);
int uo_init_rsv_pool(struct uo_provider *itself, int min_alloc_order);
void uo_deinit_rsv_pool(struct uo_provider *itself);
void *uo_generic_alloc_rsv(struct uo_provider *itself, dma_addr_t *phy, unsigned int size, int align);
int uo_generic_free_rsv(struct uo_provider *itself, void *vir, unsigned int size);
unsigned int uo_generic_mpu_region(struct uo_provider *itself, dma_addr_t *phys);

/****
 * hid offload control api
 ****/
void usb_offload_hid_probe(void);
int usb_offload_hid_start(void);
void usb_offload_hid_finish(void);
void usb_offload_hid_stop(void);
bool usb_offload_trace_hid_enqueue(struct xhci_hcd *xhci, struct urb *urb);

/****
 * ipi senter & receiver related
 ****/
enum usb_offload_ipi_msg {
	UOI_INIT_ADSP = 0,
	UOI_DEINIT_ADSP,
	UOI_ENABLE_STREAM,
	UOI_DISABLE_STREAM,
	UOI_ENABLE_HID,
	UOI_DISABLE_HID,
	UOI_ENABLE_TRACE,
	UOI_DISABLE_TRACE,
	UOI_NUM,
};
void usb_offload_register_ipi_recv(void);
int usb_offload_send_ipi_msg(enum usb_offload_ipi_msg msg_type, void *data, size_t size);
void usb_offload_ipi_hid_handler(void *param);
void usb_offload_ipi_trace_handler(void *param);

/****
 * platform policy
 ****/
void usb_offload_platform_policy_init(struct device *dev, struct usb_offload_policy *policy);
int usb_offload_link_xhci(struct device *dev);
void usb_offload_improve_idle_power(bool start);
enum uo_provider_type usb_offload_mem_type(void);
enum uo_provider_type usb_offload_mem_type_lp(void);
enum uo_provider_type usb_offload_mem_type_lp_ex(int direction);
void usb_offload_hub_working(bool dev_on_hub, bool hold);

enum usb_plat_action {
	UO_PLAT_ACTION_SUSPEND,
	UO_PLAT_ACTION_RESUME,
};

void usb_offload_platform_action(struct device *dev, enum usb_plat_action action);

/****
 * raw data dump related
 ****/
int usb_offload_debug_init(struct usb_offload_dev *udev);
int usb_offload_debug_deinit(struct usb_offload_dev *udev);
void usb_offload_trace_start(struct usb_audio_stream_msg *msg);
void usb_offload_trace_stop(int dir, bool skip_ipi);
void usb_offload_trace_stop_all(void);

/****
 * mbbrain related
 ****/

enum uo_mbrain_phase {
	UO_PHASE_CONNECT,
	UO_PHASE_OPEN,
	UO_PHASE_INIT_ADSP,
	UO_PHASE_ENABLE_STREAM,
	UO_PHASE_DISABLE_STREAM,
	UO_PHASE_HID_START,
	UO_PHASE_HID_ONGOING,
};

enum uo_mbrain_error {
	UO_ERROR_SUCCESS = 0,
	UO_ERROR_IPI_FAIL,
	UO_ERROR_ALLOC_SB_FAIL,
	UO_ERROR_ALLOC_URB_FAIL,
	UO_ERROR_ALLOC_TR_FAIL,
	UO_ERROR_xHCI_NOT_RDY,
	UO_ERROR_NO_DEV_CONNECTED,
	UO_ERROR_INSUFFICIENT_SPACE,
	UO_ERROR_RSV_REGION_ISSUE,
	UO_ERROR_TIMEOUT,
	UO_ERROR_ABNORMAL_BEHAVIOR,
};

struct uo_mbrain {
	u16 vid;
	u16 pid;
	enum uo_mbrain_phase phase;
	enum uo_mbrain_error error;
};

int register_uo_mbrain_cb(void (*cb)(struct uo_mbrain data));
int unregister_uo_mbrain_cb(void);
void uo_mbrain_update(enum uo_mbrain_phase phase, enum uo_mbrain_error error);
#endif /* __USB_OFFLOAD_H__ */
