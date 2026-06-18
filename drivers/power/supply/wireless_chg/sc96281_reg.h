// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
*/
#ifndef __SC96281_REG__
#define __SC96281_REG__

typedef enum private_cmd_t {
    PRIVATE_CMD_NONE                = 0x00,
    PRIVATE_CMD_READ_HW_VER         = 0x01,
    PRIVATE_CMD_READ_TEMP           = 0x02,
    PRIVATE_CMD_SET_FREQ            = 0x03,
    PRIVATE_CMD_GET_FREQ            = 0x04,
    PRIVATE_CMD_READ_FW_VER         = 0x05,
    PRIVATE_CMD_READ_IIN            = 0x06,
    PRIVATE_CMD_READ_VIN            = 0x07,
    PRIVATE_CMD_DEV_AUTH_CONFIRM    = 0x08,
    PRIVATE_CMD_SET_VIN             = 0x0A,
    PRIVATE_CMD_ADAPTER_TYPE        = 0x0B,
    PRIVATE_CMD_AUTH_REQ            = 0x3B,
    PRIVATE_CMD_GET_BPP_TX_UUID     = 0x3F,
    PRIVATE_CMD_GET_EPP_TX_UUID     = 0x4C,
    PRIVATE_CMD_CTRL_PRI_PKT        = 0x63,
    PRIVATE_CMD_GET_TX_MAC_ADDR_L   = 0xB6,
    PRIVATE_CMD_GET_TX_MAC_ADDR_H   = 0xB7,
    PRIVATE_CMD_SET_MIN_FREQ        = 0xD3,
    PRIVATE_CMD_SET_AUTO_VLIMIT     = 0xD5,
    PRIVATE_CMD_SET_TX_VLIMIT       = 0xD6,
} private_cmd_t;
typedef enum private_packet_t {
    PRIVATE_PACKET_NONE = 0,
    PRIVATE_PACKET_VOLTAGE_LIMIT,
    PRIVATE_PACKET_MIN_FREQ_LIMIT,
    PRIVATE_PACKET_CHARGE_STATUS,
    PRIVATE_PACKET_FAN_SPEED,
} private_packet_t;
typedef enum ap_cmd_t {
    // shared
    AP_CMD_RSV0                      = BIT(0),
    AP_CMD_CHIP_RESET                = BIT(1),
    AP_CMD_SEND_PPP                  = BIT(2),
    AP_CMD_LVP_CHANGE                = BIT(3),
    AP_CMD_OVP_CHANGE                = BIT(4),
    AP_CMD_OCP_CHANGE                = BIT(5),
    AP_CMD_MAX_I_CHANGE              = BIT(6),
    AP_CMD_OTP_CHANGE                = BIT(7),
    AP_CMD_HV_WDT_DISABLE            = BIT(10),
    AP_CMD_HV_WDGT_FEED              = BIT(11),
    // RX
    AP_CMD_RX_SEND_EPT               = BIT(12),
    AP_CMD_RX_RENEG                  = BIT(13),
    AP_CMD_RX_VOUT_CHANGE            = BIT(14),
    AP_CMD_RX_VOUT_ENABLE            = BIT(15),
    AP_CMD_RX_VOUT_DISABLE           = BIT(16),
    AP_CMD_RX_RPP_8BIT               = BIT(17),
    AP_CMD_RX_RPP_16BIT              = BIT(18),
    // TX
    AP_CMD_TX_FRQ_SHIFT              = BIT(8),
    AP_CMD_TX_DISABLE                = BIT(18),
    AP_CMD_TX_ENABLE                 = BIT(19),
    AP_CMD_TX_FPWM_AUTO              = BIT(20),
    AP_CMD_TX_FPWM_MANUAL            = BIT(21),
    AP_CMD_TX_FPWM_UPDATE            = BIT(22),
    AP_CMD_TX_FPWM_CONFIG            = BIT(23),
    AP_CMD_TX_FOD_ENABLE             = BIT(24),
    AP_CMD_TX_FOD_DISABLE            = BIT(25),
    AP_CMD_TX_PING_OCP_CHANGE        = BIT(26),
    AP_CMD_TX_PING_OVP_CHANGE        = BIT(27),
    AP_CMD_TX_OPEN_LOOP              = BIT(28),
    AP_CMD_WAIT_FOR_UPDATE           = BIT(31),
} ap_cmd_t;
typedef enum cust_cmd_t {
    CUST_CMD_NONE                    = 0,
    CUST_CMD_FOD_16_SEGMENT          = BIT(0),
    CUST_CMD_ULPM                    = BIT(1),
    CUST_CMD_RX_FAKE_FAST_CHARGE     = BIT(18),
    CUST_CMD_RX_FAST_CHARGE          = BIT(19),
    CUST_CMD_RX_VOLT_LIMIT           = BIT(29),
    CUST_CMD_RX_TP_SEND              = BIT(28),
    CUST_CMD_RX_RENEG                = BIT(30)
} cust_cmd_t;
//Wireless Power Interrupt Request
typedef enum wp_irq_t {
    // shared
    WP_IRQ_NONE                  = 0,
    WP_IRQ_OCP                   = BIT(0),
    WP_IRQ_OVP                   = BIT(1),
    WP_IRQ_CLAMP_OVP             = BIT(2),
    WP_IRQ_NGATE_OVP             = BIT(3),
    WP_IRQ_LVP                   = BIT(4),
    WP_IRQ_OTP                   = BIT(5),
    WP_IRQ_OTP_110               = BIT(6),
    WP_IRQ_SLEEP                 = BIT(7),
    WP_IRQ_MODE_CHANGE           = BIT(8),
    WP_IRQ_PKT_RECV              = BIT(9),
    WP_IRQ_PPP_TIMEOUT           = BIT(10),
    WP_IRQ_PPP_SUCCESS           = BIT(11),
    WP_IRQ_AFC                   = BIT(12),
    WP_IRQ_PROFILE               = BIT(13),
    WP_IRQ_RX_POWER_ON           = BIT(14),
    WP_IRQ_RX_SS_PKT             = BIT(15),
    WP_IRQ_RX_ID_PKT             = BIT(16),
    WP_IRQ_RX_CFG_PKT            = BIT(17),
    WP_IRQ_RX_READY              = BIT(18),
    WP_IRQ_RX_LDO_ON             = BIT(19),
    WP_IRQ_RX_LDO_OFF            = BIT(20),
    WP_IRQ_RX_LDO_OPP            = BIT(21),
    WP_IRQ_RX_SCP                = BIT(22),
    WP_IRQ_RX_RENEG_SUCCESS      = BIT(23),
    WP_IRQ_RX_RENEG_FAIL         = BIT(24),
    WP_IRQ_ERROR_CODE            = BIT(25),
    WP_IRQ_RX_FACTORY_TEST       = BIT(26),
    WP_IRQ_RX_AUTH               = BIT(27),
    WP_IRQ_RX_FAST_CHARGE_SUCCESS = BIT(28),
    WP_IRQ_RX_FAST_CHARGE_TIMEOUT = BIT(29),
    WP_IRQ_RX_FAST_CHARGE_FAIL   = BIT(30),
    WP_IRQ_TX_DET_RX             = BIT(14),
    WP_IRQ_TX_EPT                = BIT(15),// Power Removed
    WP_IRQ_TX_PT                 = BIT(16),// Power Transfer
    WP_IRQ_TX_FOD                = BIT(17),
    WP_IRQ_TX_DET_TX             = BIT(18),
    WP_IRQ_TX_CEP_TIMEOUT        = BIT(19),
    WP_IRQ_TX_RPP_TIMEOUT        = BIT(20),
    WP_IRQ_TX_PING               = BIT(21),
    WP_IRQ_TX_SS_PKT             = BIT(22),
    WP_IRQ_TX_ID_PKT             = BIT(23),
    WP_IRQ_TX_CFG_PKT            = BIT(24),
    WP_IRQ_TX_POWER_ON           = BIT(26),
    WP_IRQ_TX_BRG_OCP            = BIT(27),
} wp_irq_t;
typedef enum error_code_tx_t {
    ERROR_CODE_TX_PING_OVP = 1,
    ERROR_CODE_TX_PING_OCP,
    ERROR_CODE_TX_OVP,
    ERROR_CODE_TX_OCP,
    ERROR_CODE_TX_BRIDGE_OCP,
    ERROR_CODE_TX_CLAMP_OVP,
    ERROR_CODE_TX_LVP,
    ERROR_CODE_TX_OTP,
    ERROR_CODE_TX_OTP_HARD,
    ERROR_CODE_TX_PRE_FOD,
    ERROR_CODE_TX_FOD,
    ERROR_CODE_TX_CE_TIMEOUT,
    ERROR_CODE_TX_RP_TIMEOUT,
    ERROR_CODE_TX_NOT_SS,
    ERROR_CODE_TX_NOT_ID,
    ERROR_CODE_TX_NOT_XID,
    ERROR_CODE_TX_NOT_CFG,
    ERROR_CODE_TX_SS_TIMEOUT,
    ERROR_CODE_TX_ID_TIMEOUT,
    ERROR_CODE_TX_XID_TIMEOUT,
    ERROR_CODE_TX_CFG_TIMEOUT,
    ERROR_CODE_TX_NEG_TIMEOUT,
    ERROR_CODE_TX_CAL_TIMEOUT,
    ERROR_CODE_TX_CFG_COUNT,
    ERROR_CODE_TX_PCH_VALUE,
    ERROR_CODE_TX_EPT_PKT,
    ERROR_CODE_TX_ILLEGAL_PKT,
    ERROR_CODE_TX_AC_DET,
    ERROR_CODE_TX_CHG_FULL,
    ERROR_CODE_TX_SS_ID,
    ERROR_CODE_TX_AP_CMD,
} error_code_tx_t;
typedef enum error_code_rx_t {
    ERROR_CODE_RX_AP_CMD = 1,
    ERROR_CODE_RX_AC_LOSS,
    ERROR_CODE_RX_SS_OVP,
    ERROR_CODE_RX_VOUT_OVP,
    ERROR_CODE_RX_OVP_SUSTAIN,
    ERROR_CODE_RX_OCP_ADC,
    ERROR_CODE_RX_OCP_HARD,
    ERROR_CODE_RX_SCP,
    ERROR_CODE_RX_OTP_HARD,
    ERROR_CODE_RX_OTP_110,
    ERROR_CODE_RX_NGATE_OVP,
    ERROR_CODE_RX_LDO_OPP,
    ERROR_CODE_RX_SLEEP,
    ERROR_CODE_RX_HOP1,
    ERROR_CODE_RX_HOP2,
    ERROR_CODE_RX_HOP3,
    ERROR_CODE_RX_VRECT_OVP,
} error_code_rx_t;

typedef enum ept_reason_t{
    EPT_UNKNOWN               	= 0x00,   // The Receiver may use this value if it does not have a specific reason for terminating the power transfer, or if none of the other values listed in Table 6-6 is appropriate.
    EPT_CHG_COMPLETE            = 0x01,   // The Receiver should use this value if it determines that the battery of the Mobile Device is fully charged. On receipt of an End Power Transfer Packet containing this value, the Transmitter should set any charged indication on its user interface that is associated with the Receiver.
    EPT_INTERNAL_FAULT          = 0x02,   // The Receiver may use this value if it has encountered some internal problem, e.g. a software or logic error.
    EPT_OVER_TEMPERATURE        = 0x03,   // The Receiver should use this value if it has measured a temperature within the Mobile Device that exceeds a limit.
    EPT_OVER_VOLTAGE            = 0x04,   // The Receiver should use this value if it has measured a voltage within the Mobile Device that exceeds a limit.
    EPT_OVER_CURRENT            = 0x05,   // The Receiver should use this value if it has measured a current within the Mobile Device that exceeds a limit.
    EPT_BATTERY_FAILURE         = 0x06,   // The Receiver should use this value if it has determined a problem with the battery of the Mobile Device.
    EPT_RECONFIGURE             = 0x07,
    EPT_NO_RESPONSE             = 0x08,   // The Receiver should use this value if it determines that the Transmitter does not respond to Control Error Packets as expected (i.e. does not increase/decrease its Primary Cell current appropriately).
    EPT_RESERVED09              = 0x09,
    EPT_NEGOTIATION_FAILURE     = 0x0a,   // A Power Receiver should use this value if it cannot negotiate a suitable Guaranteed Power level.
    EPT_RESTART_POWER_TRANSFER  = 0x0b,   // A Power Receiver should use this value if sees a need for Foreign Object Detection with no power transfer in progress (see Section 11.3, FOD based on quality factor change). To enable such detection, the power transfer has to be terminated. Typically, the Power Transmitter then performs Foreign Object Detection before restarting the power transfer.
} ept_reason_t;
typedef enum work_mode_t{
    WORK_MODE_STANDBY              = BIT(0),
    WORK_MODE_RX                   = BIT(1),
    WORK_MODE_TX                   = BIT(2),
    WORK_MODE_BPP                  = BIT(3),
    WORK_MODE_EPP                  = BIT(4),
    WORK_MODE_PRIVATE              = BIT(5),
    WORK_MODE_RPP_16BIT            = BIT(6),
    WORK_MODE_VSYS                 = BIT(7),
    WORK_MODE_AC                   = BIT(8),
    WORK_MODE_SLEEP                = BIT(9),
    WORK_MODE_HV_WDT               = BIT(10),
    WORK_MODE_FOD                  = BIT(11),
    WORK_MODE_BRG_HALF             = BIT(12),
    WORK_MODE_PPP_BUSY             = BIT(13),
    WORK_MODE_TX_PING              = BIT(14),
    WORK_MODE_TX_PT                = BIT(15),
    WORK_MODE_TX_POWER_VOUT        = BIT(16)
} work_mode_t;

typedef struct fod_type_t {
    uint8_t gain;
    int8_t  offset;
} fod_type_t;

typedef struct ask_pkt_t {
    union {
        uint8_t buff[32];
        struct {
            uint8_t header;
            uint8_t msg[31];
        };
    };
} ask_pkt_t;

typedef struct fsk_pkt_t {
    union {
        uint8_t buff[32];
        struct {
            uint8_t header;
            int8_t  msg[31];
        };
    };
} fsk_pkt_t;

typedef union fsk_param_t {
    uint8_t value;
    struct {
        uint8_t depth             : 2;
        uint8_t polarity          : 1;
        uint8_t cycle             : 2;    //Qi2.0 and newer
        uint8_t reserved          : 3;
    };
} fsk_param_t;
typedef union contract_t {    // power transfer contract
    uint32_t value;
    struct {
        uint8_t               rp_pkt_hdr;
        uint8_t               guaranteed_power;// The value in this field is in units of 0.5 W.
        uint8_t               ref_power;       // reference power,received power calculation reference
        fsk_param_t         fsk_param;
    };
} contract_t;
typedef struct id_pkt_t {
    uint8_t header;
    uint8_t  minor_version        : 4;
    uint8_t  major_version        : 4;
    uint16_t manufacturer_code;
    uint32_t basic_device_identifier3 : 7;
    uint32_t ext                      : 1;
    uint32_t basic_device_identifier2 : 8;
    uint32_t basic_device_identifier1 : 8;
    uint32_t basic_device_identifier0 : 8;
} id_pkt_t;
typedef struct cfg_pkt_t {
    uint8_t ref_power             : 6;
    uint8_t power_class           : 2;
    uint8_t reserved0;
    uint8_t count                 : 3;
    uint8_t zero_0                : 1;
    uint8_t ob                    : 1;//Qi1.3 and newer
    uint8_t reserved1             : 1;
    uint8_t ai                    : 1;//Qi1.3 and newer
    uint8_t zero_1                : 1;
    uint8_t window_offset         : 3;
    uint8_t window_size           : 5;
    uint8_t dup                   : 1;   //Qi1.3 and newer
    uint8_t buffer_size           : 3;   //Qi1.3 and newer
    uint8_t depth                 : 2;
    uint8_t polarity              : 1;
    uint8_t neg                   : 1;
} cfg_pkt_t;
typedef struct pt_cap_pkt_t{
    uint8_t guaranteed_power      : 6;
    uint8_t power_class           : 2;
    uint8_t potential_power       : 6;
    uint8_t reserved0             : 2;
    uint8_t not_ressens           : 1;
    uint8_t wpid                  : 1;
    uint8_t buffer_size           : 3;//Qi1.3 and newer
    uint8_t ob                    : 1;//Qi1.3 and newer
    uint8_t ar                    : 1;//Qi1.3 and newer
    uint8_t dup                   : 1;//Qi1.3 and newer
} pt_cap_pkt_t;
typedef enum response_t {
    RESPONSE_NACK = 0,
    RESPONSE_ATN  = 0x33,
    RESPONSE_ND   = 0x55,
    RESPONSE_ACK  = 0xFF
} response_t;
typedef enum project_flag_t {
    PROJECT_FLAG_ID                     = BIT(0),
    PROJECT_FLAG_AUTH                   = BIT(1),
    PROJECT_FLAG_ADAPTER_TYPE           = BIT(2),
    PROJECT_FLAG_UUID                   = BIT(3),
    PROJECT_FLAG_FAST_CHARGE            = BIT(4),
    PROJECT_FLAG_RENEG                  = BIT(5),
    // PROJECT_FLAG_VOLT_LIMIT          = BIT(6),
    PROJECT_FLAG_TP_SEND                = BIT(8),
} project_flag_t;

typedef struct cust_rx_t {               // <offset>
    uint32_t          chip_id;            // 0X0000
    uint32_t          firmware_ver;       // 0X0004
    uint32_t          hardware_ver;       // 0X0008
    uint32_t          git_ver;            // 0X000C
    uint16_t          mfr_code;           // 0X0010
    uint16_t          rsv_0012;           // 0X0012
    uint32_t          rsv_0014;           // 0X0014
    uint32_t          rsv_0018;           // 0X0018
    uint32_t          firmware_check;     // 0X001C
    uint16_t          start_ocp;          // 0X0020
    uint16_t          start_ovp;          // 0X0022
    uint16_t          ilimit;             // 0X0024
    uint16_t          ocp;                // 0X0026
    uint16_t          ovp;                // 0X0028
    uint16_t          ldo_opp_soft;       // 0X002A
    uint16_t          clamp_ovp;          // 0X002C
    uint16_t          ngate_ovp;          // 0X002E
    uint16_t          start_vrect;        // 0X0030
    uint16_t          max_vrect;          // 0X0032
    uint16_t          target_vout;        // 0X0034
    uint16_t          rsv_0036;           // 0x0036
    uint16_t          epp_vout_level[4];  // 0X0038
    uint16_t          full_brg_vrect;     // 0X0040
    uint16_t          half_brg_vrect;     // 0X0042
    uint16_t          full_sr_iout;       // 0X0044
    uint16_t          half_sr_iout;       // 0X0046
    uint16_t          sr_iout_hys;        // 0X0048
    uint16_t          rsv_004A;           // 0X004A
    uint16_t          vrect_curve_x1;     // 0X004C
    uint16_t          vrect_curve_x2;     // 0X004E
    uint16_t          vrect_curve_y1;     // 0X0050
    uint16_t          vrect_curve_y2;     // 0X0052
    uint16_t          vrect_offset;       // 0X0054
    uint16_t          rsv_0056;           // 0x0056
    uint16_t          cep_interval;       // 0X0058
    uint16_t          rpp_interval;       // 0X005A
    uint32_t          rsv_005C;           // 0X005C
    fod_type_t        bpp_fod[8];         // 0X0060
    fod_type_t        epp_fod[8];         // 0X0070
    fsk_param_t       fsk_param;          // 0X0080
    uint8_t           rsv_0081;           // 0X0081
    uint8_t           fsk_dm_th;          // 0X0082
    uint8_t           rsv_0083;           // 0X0083
    uint8_t           otp;                // 0X0084
    uint8_t           rsv_0085[3];        // 0X0085
    uint16_t          wake_time;          // 0X0088
    uint16_t          rsv_008A;           // 0X008A
    uint8_t           ask_cfg;            // 0X008C
    uint8_t           rsv_008D[3];        // 0X008D
    uint8_t           vsys_cfg;           // 0X0090
    uint8_t           rsv_0091[3];        // 0X0091
    uint16_t          dummyload_idle;     // 0X0094
    uint16_t          dummyload_ovp;      // 0X0096
    uint32_t          rsv_0098[2];        // 0X0098
    uint8_t           tx_setting[0x60];   // 0X00A0
    uint32_t          cmd;                // 0X0100
    uint32_t          irq_en;             // 0X0104
    uint32_t          irq_flag;           // 0X0108
    uint32_t          irq_clr;            // 0X010C
    uint32_t          cep_cnt;            // 0X0110
    uint32_t          mode;               // 0X0114
    uint32_t          random;             // 0X0118
    uint32_t          wdg_cnt;            // 0X011C
    ask_pkt_t         ask_pkt;            // 0X0120
    fsk_pkt_t         fsk_pkt;            // 0X0140
    uint16_t          vout;               // 0X0160
    uint16_t          rsv_0162;           // 0X0162
    uint16_t          iout;               // 0X0164
    uint16_t          rsv_0166;           // 0X0166
    uint16_t          vrect;              // 0X0168
    uint16_t          target_vrect;       // 0X016A
    uint16_t          rx_ping_v;          // 0X016C
    uint16_t          rsv_016E;           // 0X016E
    uint16_t          receive_power;      // 0X0170
    uint16_t          transmit_power;     // 0X0172
    uint16_t          power_freq;         // 0X0174
    uint16_t          power_period;       // 0X0176
    int16_t           t_die;              // 0X0178
    uint16_t          rsv_017A;           // 0X017A
    uint8_t           power_duty;         // 0X017C
    uint8_t           rsv_017D[3];        // 0X017D
    uint32_t          rsv_0180[3];        // 0X0180
    uint8_t           brg_sr_mode;        // 0X018C
    uint8_t           brg_rect_mode;      // 0X018D
    uint16_t          rsv_018E;           // 0X018E
    uint8_t           ept_reason;         // 0X0190
    uint8_t           err_code;           // 0X0191
    uint8_t           rsv_0192[2];        // 0X0192
    uint32_t          ept_type;           // 0X0194
    id_pkt_t          rxid_pkt;           // 0X0198
    uint8_t           sig_strength;       // 0X01A0
    uint8_t           pch_value;          // 0X01A1
    uint16_t          rsv_01A2;           // 0X01A2
    cfg_pkt_t         rxcfg_pkt;          // 0X01A4
    uint8_t           rsv_01A9[3];        // 0X01A9
    contract_t        neg_req_contract;   // 0X01AC
    contract_t        neg_cur_contract;   // 0X01B0
    int8_t            cep_value;          // 0X01B4
    uint8_t           chs_value;          // 0X01B5
    uint16_t          rpp_value;          // 0X01B6
    uint32_t          rsv_01B8[2];        // 0X01B8
    uint32_t          rsv_01C0;           // 0X01C0
    uint16_t          tx_mfr_code;        // 0X01C4
    uint8_t           tx_ver;             // 0X01C6
    uint8_t           afc_ver;            // 0X01C7
    pt_cap_pkt_t      capability_pkt;     // 0X01C8
    uint8_t           rsv_01CB;           // 0X01CB
    uint8_t           rsv_01CC[3];        // 0X01CC
    uint8_t           iic_check;          // 0X01CF
    struct {
        uint32_t      cmd;                // 0X01D0
        uint32_t      fun_flag;           // 0X01D4
        uint8_t       adapter_type;       // 0X01D8
        uint8_t       rsv_01d9;           // 0X01D9
        uint16_t      rev_01da;           // 0X01DA
        uint32_t      tx_uuid;            // 0X01DC
        uint16_t      fc_volt;            // 0X01E0
        uint16_t      volt_limit;         // 0X01E2
        uint8_t       reneg_param;        // 0X01E4
        uint8_t       rsv_01e5[3];        // 0X01E5
        uint32_t      cfg;                // 0X01E8
        uint8_t       rsv_01ec[20];       // 0X01EC
    } mi_ctx;
} cust_rx_t;
typedef struct cust_tx_t{               // <offset>
    uint32_t          chip_id;            // 0X0000
    uint32_t          firmware_ver;       // 0X0004
    uint32_t          hardware_ver;       // 0X0008
    uint32_t          git_ver;            // 0X000C
    uint16_t          mfr_code;           // 0X0010
    uint16_t          rsv_0012;           // 0X0012
    uint32_t          rsv_0014;           // 0X0014
    uint32_t          rsv_0018;           // 0X0018
    uint32_t          firmware_check;     // 0X001C
    uint8_t           rx_setting[0x80];   // 0X0020
    uint16_t          min_freq;           // 0X00A0
    uint16_t          max_freq;           // 0X00A2
    uint16_t          lvp;                // 0X00A4
    uint16_t          rsv_00A6;           // 0X00A6
    uint16_t          ping_ocp;           // 0X00A8
    uint16_t          ocp;                // 0X00AA
    uint16_t          ping_ovp;           // 0X00AC
    uint16_t          ovp;                // 0X00AE
    uint16_t          clamp_ovp;          // 0X00B0
    uint16_t          ngete_ovp;          // 0X00B2
    uint8_t           otp;                // 0X00B4
    uint8_t           rsv_00B5[3];        // 0X00B5
    uint32_t          rsv_00B8;           // 0X00B8
    uint16_t          ping_interval;      // 0X00BC
    uint16_t          ping_timeout;       // 0X00BE
    uint16_t          ping_freq;          // 0X00C0
    uint8_t           ping_duty;          // 0X00C2
    uint8_t           rsv_00C3;           // 0X00C3
    uint32_t          rsv_00C4[3];        // 0X00C4
    uint8_t           brg_dead_time;      // 0X00D0
    uint8_t           min_duty;           // 0X00D1
    uint16_t          rsv_00D2;           // 0X00D2
    uint8_t           brg_reg_cfg;        // 0X00D4
    uint8_t           rsv_00D5;           // 0X00D5
    uint16_t          brg_half_vin;       // 0X00D6
    uint16_t          brg_full_iin;       // 0X00D8
    uint16_t          brg_hys_iin;        // 0X00DA
    uint16_t          brg_switch_freq;    // 0X00DC
    uint8_t           brg_switch_duty;    // 0X00DE
    uint8_t           rsv_00DF;           // 0X00DF
    uint8_t           fod_idx;            // 0X00E0
    uint8_t           fod_cnt;            // 0X00E1
    uint16_t          rsv_00E2;           // 0X00E2
    uint16_t          fod_power_loss[6];  // 0X00E4
    uint32_t          rsv_00F0[4];        // 0X00F0
    uint32_t          cmd;                // 0X0100
    uint32_t          irq_en;             // 0X0104
    uint32_t          irq_flag;           // 0X0108
    uint32_t          irq_clr;            // 0X010C
    uint32_t          cep_cnt;            // 0X0110
    uint32_t          mode;               // 0X0114
    uint32_t          random;             // 0X0118
    uint32_t          wdg_cnt;            // 0X011C
    ask_pkt_t         ask_pkt;            // 0X0120
    fsk_pkt_t         fsk_pkt;            // 0X0140
    uint16_t          vout;               // 0X0160
    uint16_t          rsv_0162;           // 0X0162
    uint16_t          iout;               // 0X0164
    uint16_t          rsv_0166;           // 0X0166
    uint16_t          vrect;              // 0X0168
    uint16_t          target_vrect;       // 0X016A
    uint16_t          rx_ping_v;          // 0X016C
    uint16_t          rsv_016E;           // 0X016E
    uint16_t          receive_power;      // 0X0170
    uint16_t          transmit_power;     // 0X0172
    uint16_t          power_freq;         // 0X0174
    uint16_t          power_period;       // 0X0176
    int16_t           t_die;              // 0X0178
    uint16_t          rsv_017A;           // 0X017A
    uint8_t           power_duty;         // 0X017C
    uint8_t           rsv_017D[3];        // 0X017D
    uint32_t          rsv_0180[3];        // 0X0180
    uint8_t           brg_sr_mode;        // 0X018C
    uint8_t           brg_rect_mode;      // 0X018D
    uint16_t          rsv_018E;           // 0X018E
    uint8_t           ept_reason;         // 0X0190
    uint8_t           err_code;           // 0X0191
    uint8_t           rsv_0192[2];        // 0X0192
    uint32_t          ept_type;           // 0X0194
    id_pkt_t          rxid_pkt;           // 0X0198
    uint8_t           sig_strength;       // 0X01A0
    uint8_t           pch_value;          // 0X01A1
    uint16_t          rsv_01A2;           // 0X01A2
    cfg_pkt_t         rxcfg_pkt;          // 0X01A4
    uint8_t           rsv_01A9[3];        // 0X01A9
    contract_t        neg_req_contract;   // 0X01AC
    contract_t        neg_cur_contract;   // 0X01B0
    int8_t            cep_value;          // 0X01B4
    uint8_t           chs_value;          // 0X01B5
    uint16_t          rpp_value;          // 0X01B6
    uint32_t          rsv_01B8[2];        // 0X01B8
    uint32_t          rsv_01C0;           // 0X01C0
    uint16_t          tx_mfr_code;        // 0X01C4
    uint8_t           tx_ver;             // 0X01C6
    uint8_t           afc_ver;            // 0X01C7
    pt_cap_pkt_t      capability_pkt;     // 0X01C8
    uint8_t           rsv_01CB;           // 0X01CB
    uint8_t           rsv_01CC[3];        // 0X01CC
    uint8_t           iic_check;          // 0X01CF
    struct {
        uint32_t      cmd;                // 0X01D0
        uint32_t      flag;               // 0X01D4
        uint8_t       RSV01D0[40];        // 0X01D8
    } public;
} cust_tx_t;
#define cust_rx (*((cust_rx_t *)(0)))
#define cust_tx (*((cust_tx_t *)(0)))
#endif
