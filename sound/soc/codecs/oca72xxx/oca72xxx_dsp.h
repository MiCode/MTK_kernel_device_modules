#ifndef __OCA72XXX_DSP_H__
#define __OCA72XXX_DSP_H__

#include "oca72xxx_device.h"

/*#define OCA_MTK_OPEN_DSP_PLATFORM*/
/*#define OCA_QCOM_OPEN_DSP_PLATFORM*/

/*Note: The pord_ID is configured according to different platforms*/
#define OCA_DSP_SLEEP_TIME	(10)

#define OCA_DSP_MSG_HDR_VER (1)

#define OCA_RX_DEFAULT_TOPO_ID		(0x1000FF01)
#define OCA_RX_DEFAULT_PORT_ID		(0x4000)

#define OCSDSP_RX_SET_ENABLE		(0x10013D11)
#define OCSDSP_RX_PARAMS			(0x10013D12)
#define OCSDSP_RX_VMAX_0			(0X10013D17)
#define OCSDSP_RX_VMAX_1			(0X10013D18)
#define OCA_MSG_ID_SPIN 			(0x10013D2E)

enum {
	OCA_SPIN_0 = 0,
	OCA_SPIN_90,
	OCA_SPIN_180,
	OCA_SPIN_270,
	OCA_SPIN_MAX,
};

typedef struct mtk_dsp_msg_header {
	int32_t type;
	int32_t opcode_id;
	int32_t version;
	int32_t reserver[3];
} mtk_dsp_hdr_t;

enum oca_rx_module_enable {
	OCA_RX_MODULE_DISENABLE = 0,
	OCA_RX_MODULE_ENABLE,
};

enum oca_dsp_msg_type {
	DSP_MSG_TYPE_DATA = 0,
	DSP_MSG_TYPE_CMD = 1,
};

enum oca_dsp_channel {
	OCA_DSP_CHANNEL_0 = 0,
	OCA_DSP_CHANNEL_1,
	OCA_DSP_CHANNEL_MAX,
};

uint8_t oca72xxx_dsp_isEnable(void);
int oca72xxx_dsp_get_rx_module_enable(int *enable);
int oca72xxx_dsp_set_rx_module_enable(int enable);
int oca72xxx_dsp_get_vmax(uint32_t *vmax, int channel);
int oca72xxx_dsp_set_vmax(uint32_t vmax, int channel);
int oca72xxx_dsp_set_spin(uint32_t ctrl_value);
int oca72xxx_dsp_get_spin(void);
int oca72xxx_spin_set_record_val(void);
void oca72xxx_device_parse_port_id_dt(struct oca_device *oca_dev);
void oca72xxx_device_parse_topo_id_dt(struct oca_device *oca_dev);

#endif
