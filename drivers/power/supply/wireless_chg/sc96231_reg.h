// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
*/
#ifndef __SC96231_REG__
#define __SC96231_REG__
#include <linux/alarmtimer.h>
#include <linux/notifier.h>

/// -----------------------------------------------------------------
///                      TX AP Command
/// -----------------------------------------------------------------
typedef enum {
	AP_CMD_RSV0           = BIT(0), 
    AP_CMD_CHIP_RESET     = BIT(1),
    AP_CMD_SEND_PPP       = BIT(2),
    AP_CMD_LVP_CHANGE     = BIT(3),
    AP_CMD_OVP_CHANGE     = BIT(4),
    AP_CMD_OCP_CHANGE     = BIT(5),
    AP_CMD_MAX_I_CHANGE   = BIT(6),
    AP_CMD_OTP_CHANGE     = BIT(7),
    AP_CMD_HW_WDT_DISABLE = BIT(10),
    AP_CMD_HW_WDGT_FEED   = BIT(11),
    AP_CMD_TX_FRQ_SHIFT       = BIT(8),
    AP_CMD_TX_DISABLE         = BIT(18),
    AP_CMD_TX_ENABLE          = BIT(19),
    AP_CMD_TX_FPWM_AUTO       = BIT(20),
    AP_CMD_TX_FPWM_MANUAL     = BIT(21),
    AP_CMD_TX_FPWM_UPDATE     = BIT(22),
    AP_CMD_TX_FPWM_CONFIG     = BIT(23),
    AP_CMD_TX_FOD_ENABLE      = BIT(24),
    AP_CMD_TX_FOD_DISABLE     = BIT(25),
    AP_CMD_TX_PING_OCP_CHANGE = BIT(26),
    AP_CMD_TX_PING_OVP_CHANGE = BIT(27),
    AP_CMD_TX_OPEN_LOOP       = BIT(28),
    AP_CMD_MI_Q_CALI          = BIT(29),

    AP_CMD_WAIT_FOR_UPDATE    = BIT(31),
} TX_CMD;

typedef enum {
    STANDBY_MODE              = BIT(0),
    RX_MODE                   = BIT(1),
    TX_MODE                   = BIT(2),
    BPP_MODE                  = BIT(3),
    EPP_MODE                  = BIT(4),
    PRIVATE_MODE              = BIT(5),
    RPP24bit_MODE             = BIT(6),
    VSYS_MODE                 = BIT(7),
    AC_MODE                   = BIT(8),
    SLEEP_MODE                = BIT(9),
    HVWDGT_MODE               = BIT(10),
    FOD_MODE                  = BIT(11),
    BRG_HALF                  = BIT(12),
    PPP_BUSY                  = BIT(13),
    TX_PING_MODE              = BIT(14),
    TX_PT_MODE                = BIT(15),
    TX_POWER_VOUT             = BIT(16)
} WORK_MODE_E;

/// -----------------------------------------------------------------
///                      sys mode
/// -----------------------------------------------------------------
typedef union {
    uint32_t value;
    struct {
        uint32_t STANDBY        : 1;
        uint32_t RECEIVER       : 1;
        uint32_t TRANSMITTER    : 1;
        uint32_t BPP_MODE       : 1;
        uint32_t EPP_MODE       : 1;
        uint32_t PREIVATE_MODE  : 1;
        uint32_t RPP_24BIT      : 1;
        uint32_t VDD_STATUS     : 1;
        uint32_t AC_MODE        : 1;
        uint32_t SLEEP_MODE     : 1;
        uint32_t HVWDGT_STATUS  : 1;
        uint32_t FOD_STATUS     : 1;
        uint32_t BRG_HALF       : 1;
        uint32_t PPP_BUSY       : 1;
        uint32_t TX_PING_MODE   : 1;
        uint32_t TX_PT_MODE     : 1;
        uint32_t TX_POWER_VOUT  : 1;
    };
} SYSMODE;
/// -----------------------------------------------------------------
///                      EPT RESON
/// -----------------------------------------------------------------
typedef enum {
    UNKOWN              = 0X00,
    CHARGE_COMPLETE     = 0X01,
    INTERNAL_FAULT      = 0X02,
    OVER_TEMPERATURE    = 0X03,
    OVER_VOLTAGE        = 0X04,
    OVER_CURRENT        = 0X05,
    BATTERY_FAILURE     = 0X06,
    RESERVED1           = 0X07,
    NO_RESPONSE         = 0X08,
    RESERVED2           = 0X09,
    NEGO_FAILURE        = 0X0A,
    RESTART_PT          = 0X0B,
} EPT_RESON;
/// -----------------------------------------------------------------
///                      EPT TYPE
/// -----------------------------------------------------------------
enum{
    EPT_CMD             = BIT(0),
    EPT_SEL             = BIT(1),
    EPT_SS              = BIT(2),
    EPT_ID              = BIT(3),
    EPT_XID             = BIT(4),
    EPT_CFG             = BIT(5),
    EPT_CFG_COUNT_ERR   = BIT(6),
    EPT_PCH             = BIT(7),
    EPT_NEG             = BIT(8),
    EPT_CALI            = BIT(9),
    EPT_FIRSTCEP        = BIT(10),
    EPT_TIMEOUT         = BIT(11),
    EPT_CEP_TIMEOUT     = BIT(12),
    EPT_RPP_TIMEOUT     = BIT(13),
    EPT_LCP             = BIT(14),
    EPT_OCP             = BIT(15),
    EPT_OVP             = BIT(16),
    EPT_LVP             = BIT(17),
    EPT_FOD             = BIT(18),
    EPT_OTP             = BIT(19),
    EPT_PING_OCP        = BIT(20),
    EPT_PING_OVP        = BIT(21),
    EPT_CLAMP_OVP       = BIT(22),
    EPT_NGATE_OVP       = BIT(23),
    EPT_AC_DET          = BIT(24),
    EPT_PKTERR          = BIT(25),
};

/// -----------------------------------------------------------------
///                      INT FLAG
/// -----------------------------------------------------------------
// public
#define   WP_IRQ_NONE                        0
#define   WP_IRQ_OCP                         BIT(0)
#define   WP_IRQ_OVP                         BIT(1)
#define   WP_IRQ_CLAMP_OVP                   BIT(2)
#define   WP_IRQ_NGATE_OVP                   BIT(3)
#define   WP_IRQ_LVP                         BIT(4)
#define   WP_IRQ_OTP                         BIT(5)
#define   WP_IRQ_OTP_160                     BIT(6)
#define   WP_IRQ_SLEEP                       BIT(7)
#define   WP_IRQ_MODE_CHANGE                 BIT(8)
#define   WP_IRQ_PKT_RECV                    BIT(9)
#define   WP_IRQ_PPP_TIMEOUT                 BIT(10)
#define   WP_IRQ_PPP_SUCCESS                 BIT(11)
#define   WP_IRQ_AFC                         BIT(12)
#define   WP_IRQ_POWER_PROFILE               BIT(13)
// RX
#define   WP_IRQ_RX_POWER_ON                 BIT(14)
#define   WP_IRQ_RX_SS_PKT                   BIT(15)
#define   WP_IRQ_RX_ID_PKT                   BIT(16)
#define   WP_IRQ_RX_CFG_PKT                	 BIT(17)
#define   WP_IRQ_RX_READY                    BIT(18)
#define   WP_IRQ_RX_LDO_ON                   BIT(19)
#define   WP_IRQ_RX_LDO_OFF                  BIT(20)
#define   WP_IRQ_RX_LDO_OPP                  BIT(21)
#define   WP_IRQ_RX_SCP                      BIT(22)
#define   WP_IRQ_RX_RENEG_SUCCESS            BIT(23)
#define   WP_IRQ_RX_RENEG_TIMEOUT            BIT(24)
#define   WP_IRQ_RX_RENEG_FAIL               BIT(25)

// TX
#define	  WP_IRQ_TX_DET_RX      		 	 BIT(14)
#define   WP_IRQ_TX_REMOVE_POWER          	 BIT(15) // Power Removed
#define	  WP_IRQ_TX_POWER_TRANSFER           BIT(16) // Power Transfer
#define	  WP_IRQ_TX_FOD         		 	 BIT(17)
#define   WP_IRQ_TX_DET_TX      	 		 BIT(18)
#define   WP_IRQ_TX_CEP_TIMEOUT 	 		 BIT(19)
#define   WP_IRQ_TX_RPP_TIMEOUT 	 		 BIT(20)
#define   WP_IRQ_TX_PING        	 		 BIT(21)
#define   WP_IRQ_TX_SS_PKT      	 		 BIT(22)
#define   WP_IRQ_TX_ID_PKT      	 		 BIT(23)
#define   WP_IRQ_TX_CFG_PKT     	 		 BIT(24)
#define   WP_IRQ_TX_BLE_CONNECT              BIT(25)
#define   WP_IRQ_TX_POWER_ON    	 		 BIT(26)
#define   WP_IRQ_TX_CHS_UPDATE               BIT(27)
//#define     WP_IRQ_TX_POWER_ON     		 BIT(27)

#define REVERSE_PING_TIMEOUT_TIMER		(20 * 1000)
#define REVERSE_TRANSFER_TIMEOUT_TIMER	(100 * 1000)
#define REVERSE_PEN_DELAY_TIMER         (10 * 1000)
#define REVERSE_PPE_TIMEOUT_TIMER       (5 * 1000)
#define PEN_SOC_FULL_COUNT              18

#ifdef SC_PROPRIETARY
typedef enum SC_PropPhaseE {
    SC_PROP_PHASE_INIT,
    SC_PROP_PHASE_CHECK_REQ, // 发送确认PT支持私有的请求
    SC_PROP_PHASE_CHECK_PT,  // 确认PT支持私有
    SC_PROP_PHASE_PT_AUTH,   // PR认证PT
    SC_PROP_PHASE_PR_AUTH,   // PT认证PR
    SC_PROP_PHASE_CONFIRM,   // 等待PT确认
    SC_PROP_PHASE_SUCCESS,   // 认证成功
    SC_PROP_PHASE_FAILED     // 认证失败
} SC_PropPhaseE;
#endif

/// -----------------------------------------------------------------
///                              WPC TYPE
///
/// -----------------------------------------------------------------
#define MAX_ASK_SIZE            32
#define MAX_FSK_SIZE            32
typedef struct {
    uint8_t max_power             : 6;
    uint8_t power_class           : 2;
    uint8_t reserved0;
    uint8_t count                 : 3;
    uint8_t ZERO                  : 1;
    uint8_t reserved1             : 3;
    uint8_t prop                  : 1;
    uint8_t window_offset         : 3;
    uint8_t window_size           : 5;
    uint8_t reserved2             : 4;
    uint8_t depth                 : 2;
    uint8_t polarity              : 1;
    uint8_t neg                   : 1;
} CfgPktType;
typedef struct {
    uint8_t  header;
    uint8_t  minor_version        : 4;
    uint8_t  major_version        : 4;
    uint16_t manufacturer_code;
    uint32_t basic_device_identifier3 : 7;
    uint32_t ext                      : 1;
    uint32_t basic_device_identifier2 : 8;
    uint32_t basic_device_identifier1 : 8;
    uint32_t basic_device_identifier0 : 8;
} IdPktType;
typedef struct {
    uint8_t SS;
    uint8_t PCH;
    uint16_t RSV;
} SSPktType;
typedef struct {
    uint8_t cep;
    uint8_t rsv;
    uint8_t rpp;
} PTpktType;
typedef struct {
    uint16_t mcode;
    uint8_t  minor                : 4;
    uint8_t  major                : 4;
    uint8_t  afc;
} TxInfoType;
typedef union {
    uint8_t value;
    struct {
        uint8_t depth             : 2;
        uint8_t polarity          : 1;
        uint8_t reserved          : 5;
    };
} FSKParametersType;
typedef union {              // power transfer contract
    uint32_t value;
    struct {
        uint8_t RPPTHeader;
        uint8_t guaranteed_power; // The value in this field is in units of 0.5 W.
        uint8_t max_power;
        FSKParametersType fsk;
    };
} ContractType;
typedef struct {
    uint8_t guaranteed_power      : 6;
    uint8_t power_class           : 2;
    uint8_t potential_power       : 6;
    uint8_t reserved0             : 2;
    uint8_t not_ressens           : 1;
    uint8_t WPID                  : 1;
    uint8_t reserved1             : 6;
} CapabilityType;
typedef struct {
    uint8_t ping                  : 1;
    uint8_t ptr                   : 1;
    uint8_t rsv0                  : 6;
    uint8_t rsv1;
} BRGManualType;
typedef struct {
    union {
        uint8_t buf[MAX_ASK_SIZE];
        struct {
            uint8_t header;
            uint8_t msg[MAX_ASK_SIZE-1];
        };
    };
} AskType;
typedef struct {
    union {
        uint8_t buf[MAX_FSK_SIZE];
        struct {
            uint8_t header;
            uint8_t msg[MAX_FSK_SIZE-1];
        };
    };
} FskType;
typedef struct {
    uint8_t G;
    int8_t Offs;
} FodType;
/// -----------------------------------------------------------------
///                      Customer Registers
///            SRAM address: 0X20000000 ~ 0X20000200
/// -----------------------------------------------------------------
#define CUSTOMER_REGISTERS_BASE_ADDR      0x20000000
typedef struct {                    // <offset>
    uint16_t 		 ChipID;       // 0X0000
    uint16_t 		 CustID;       // 0X0002
    uint32_t 		 FirmwareVer;  // 0X0004
    uint32_t 		 HardwareVer;  // 0X0008
    uint32_t 		 GitVer;       // 0X000C
    uint16_t 		 MfrCode;      // 0X0010
    uint16_t 		 Reserved0012; // 0X0012
    uint32_t 		 Reserved0014; // 0X0014
    uint32_t 		 Reserved0018; // 0X0018
    uint32_t 		 FirmwareCheck;// 0X001C
	uint8_t          RXSetting[0x80]; // 0X0020
	uint16_t 		 MinFreq;          // 0X00A0
    uint16_t 		 MaxFreq;          // 0X00A2
    uint16_t 		 TxLVP;            // 0X00A4
    uint16_t 		 TxDCM;            // 0X00A6
    uint16_t 		 PingOCP;          // 0X00A8
    uint16_t 		 TxOCP;            // 0X00AA
    uint16_t 		 PingOVP;          // 0X00AC
    uint16_t 		 TxOVP;            // 0X00AE
    uint16_t 		 TxClampOVP;       // 0X00B0
    uint16_t 		 TxNGateOVP;       // 0X00B2
    uint8_t  		 TxOTP;            // 0X00B4
    uint8_t  		 Reserved00B5[3];  // 0X00B5
    uint32_t 		 Reserved00B8;     // 0X00B8
    uint16_t 		 PingInterval;     // 0X00BC
    uint16_t 		 PingTimeout;      // 0X00BE
    uint16_t 		 PingFreq;         // 0X00C0
    uint8_t  		 PingDuty;         // 0X00C2
    uint8_t  		 Reserved00C3;     // 0X00C3
    uint32_t 		 Reserved00C4[3];  // 0X00C4
    uint8_t  		 BridgeDeadTime;   // 0X00D0
    uint8_t  		 MinDuty;          // 0X00D1
    uint16_t 		 Reserved00D2;     // 0X00D2
    uint8_t  		 BridgeCfg;        // 0X00D4
    uint8_t  		 Reserved00D5;     // 0X00D5
    uint16_t 		 BridgeHalfV;      // 0X00D6
    uint16_t 		 BridgeFullI;      // 0X00D8
    uint16_t 		 BridgeHysI;       // 0X00DA
    uint16_t 		 BridgeAdjFreq;    // 0X00DC
    uint8_t  		 BridgeAdjDuty;    // 0X00DE
    uint8_t  		 Reserved00DF;     // 0X00DF
    uint8_t  		 TxFOD_Index;      // 0X00E0
    uint8_t  		 TxFOD_Cnt;        // 0X00E1
    uint16_t 		 Reserved00E2;     // 0X00E2
    uint16_t 		 FOD_PowerLoss[6]; // 0X00E4
    uint16_t 		 pre_fod_thr;      // 0X00F0: Q值检测阈值，小于此值会报FOD
    uint8_t  		 ptr_fod_en;       // 0X00F2: 功率传输阶段FOD使能开关:<0:关><1:开>
    uint8_t  		 pre_fod_en;       // 0X00F3: Q值检测开关:<0:关><1:开>
    uint32_t 		 Reserved00F0[3];  // 0X00F4
	uint32_t         Cmd;             // 0X0100
    uint32_t         IRQ_En;          // 0X0104
    uint32_t         IRQ_Flag;        // 0X0108
    uint32_t         IRQ_Clr;         // 0X010C
    uint32_t         CEP_Cnt;         // 0X0110
    uint32_t         Mode;            // 0X0114
    uint32_t         Reserved0118;    // 0X0118
    uint32_t         WDG_Cnt;         // 0X011C
    AskType          ASK_Packet;      // 0X0120
    FskType          FSK_Packet;      // 0X0140
    uint16_t         VOut;            // 0X0160
    uint16_t         Reserved0162;    // 0X0162
    uint16_t         IOut;            // 0X0164
    uint16_t         Reserved0166;    // 0X0166
    uint16_t         VRect;           // 0X0168
    uint16_t         TargetVRect;     // 0X016A
    uint16_t         RxPingV;         // 0X016C
    uint16_t         Reserved016E;    // 0X016E
    uint16_t         ReceivePower;    // 0X0170
    uint16_t         TransmitPower;   // 0X0172
    uint16_t         PowerFreq;       // 0X0174
    uint16_t         PowerPeriod;     // 0X0176
    int16_t          T_Die;           // 0X0178
    uint16_t         Reserved017A;    // 0X017A
    uint8_t          PowerDuty;       // 0X017C
    uint8_t          Reserved017D[3]; // 0X017D
    uint32_t         Reserved0180[3]; // 0X0180
    uint8_t          SR_State;        // 0X018C
    uint8_t          RecState;        // 0X018D
    uint16_t         Reserved018E;    // 0X018E
    uint8_t          EPT_Reason;      // 0X0190
    uint8_t          Reserved0191[3]; // 0X0191
    uint32_t         EPT_Type;        // 0X0194
    IdPktType    RxID_Packet;     // 0X0198
    uint8_t          SigStrength;     // 0X01A0
    uint8_t          PCH_Value;       // 0X01A1
    uint16_t         Reserved01A2;    // 0X01A2
    uint8_t          Reserved01A4;    // 0X01A4//原来是CFG包的Header，CFG包定义删除Header
    CfgPktType   	 RxCfgPacket;     // 0X01A5
    uint16_t         Reserved01AA;    // 0X01AA
    ContractType     NegReqContract;  // 0X01AC
    ContractType     NegCurContract;  // 0X01B0
    int8_t           CEP_Value;       // 0X01B4
    uint8_t          CHS_Value;       // 0X01B5
    uint16_t         RPP_Value;       // 0X01B6
    uint32_t         Reserved01B8[2]; // 0X01B8
    // wpc paramete  rs
    uint8_t          Reserved01C0[4];  // 0X01C0
    uint16_t         TxMfrCode;        // 0X01C4
    uint8_t          TxVer;            // 0X01C6
    uint8_t          AFC_Ver;          // 0X01C7
    CapabilityType   CapabilityPacket; // 0X01C8
    uint8_t          Reserved01CB;     // 0X01CB
    uint8_t          Reserved01CC[3];  // 0X01CC
    uint8_t          IIC_Check;        // 0X01CF
	uint8_t  		 battery_level;    // 0X01D0
    uint8_t  		 pen_type;         // 0X01D1
    uint8_t  		 RSC01D2[6];       // 0X01D2
    uint8_t  		 CHARGE_TYPE;      // 0X01D8
    uint8_t  		 RSV01D9[3];       // 0X01D9
    uint8_t  		 RSV01DC[2];       // 0X01DC
    uint8_t  		 mac_byte0_1[2];   // 0X01DE
    uint8_t  		 mac_byte2_5[4];   // 0X01E0
    uint8_t  		 RSV01E4[28];      // 0X01E4
} TXCustType;

#define cust_tx     (*((TXCustType*)(0)))

#define readCust(sc, member, p)     \
	sc96231_read_block(sc, (uint64_t)(&member) + CUSTOMER_REGISTERS_BASE_ADDR, (uint8_t *)(p), (uint32_t)sizeof(member)) 

#define writeCust(sc, member, p_val) \
	sc96231_write_block(sc, (uint64_t)(&member) + CUSTOMER_REGISTERS_BASE_ADDR, (uint8_t *)(p_val), (uint32_t)sizeof(member))

/// -----------------------------------------------------------------
///                      customer command
/// -----------------------------------------------------------------
typedef enum {
    PRIVATE_CMD_NONE               = 0x00,
    PRIVATE_CMD_READ_HW_VER        = 0x01,
    PRIVATE_CMD_READ_TEMP          = 0x02,
    PRIVATE_CMD_SET_FREQ           = 0x03,
    PRIVATE_CMD_GET_FREQ           = 0x04,
    PRIVATE_CMD_READ_FW_VER        = 0x05,
    PRIVATE_CMD_READ_Iin           = 0x06,
    PRIVATE_CMD_READ_Vin           = 0x07,
    PRIVATE_CMD_DEV_AUTH_CONFIRM   = 0x08,
    PRIVATE_CMD_SET_Vin            = 0x0A,
    PRIVATE_CMD_ADAPTER_TYPE       = 0x0B,
    PRIVATE_CMD_AUTH_REQ           = 0x3B,
    PRIVATE_CMD_GET_BPP_TX_UUID    = 0x3F,
    PRIVATE_CMD_GET_EPP_TX_UUID    = 0x4C,
    PRIVATE_CMD_CTRL_PRI_PKT       = 0x63,
    PRIVATE_CMD_GET_TX_MAC_ADDR_L  = 0xB6,
    PRIVATE_CMD_GET_TX_MAC_ADDR_H  = 0xB7,
    PRIVATE_CMD_SET_MIN_FREQ       = 0xD3,
    PRIVATE_CMD_SET_AUTO_VLIMIT    = 0xD5,
    PRIVATE_CMD_SET_TX_VLIMIT      = 0xD6,
} PRIVATE_CMD_E;
typedef enum PRIVATE_PACKET_E{
    PRIVATE_PACKET_NONE=0,
    PRIVATE_PACKET_VOLTAGE_LIMIT,
    PRIVATE_PACKET_MIN_FREQ_LIMIT,
    PRIVATE_PACKET_CHARGE_STATUS,
    PRIVATE_PACKET_FAN_SPEED,
}PRIVATE_PACKET_E;

typedef enum tx_irq_flag_t {
    RTX_INT_UNKNOWN,
    RTX_INT_PING,
    RTX_INT_GET_RX,
    RTX_INT_CEP_TIMEOUT,
    RTX_INT_EPT,
    RTX_INT_PROTECTION,
    RTX_INT_GET_TX,
    RTX_INT_REVERSE_TEST_READY,
    RTX_INT_REVERSE_TEST_DONE,
    RTX_INT_FOD,
    RTX_INT_EPT_PKT,
    RTX_INT_ERR_CODE,
    RTX_INT_TX_DET_RX,
    RTX_INT_TX_CONFIG,
    RTX_INT_TX_CHS_UPDATE,
    RTX_INT_TX_BLE_CONNECT,
} tx_irq_flag_t;

#define DECL_INTERRUPT_MAP(regval, redir_irq) {\
    .irq_regval = regval,\
    .irq_flag = redir_irq, \
}

typedef struct int_map_t {
    uint32_t irq_regval;
    int irq_flag;
} int_map_t;

struct wls_fw_parameters {
	u8 fw_rx_id;
	u8 fw_tx_id;
	u8 fw_boot_id;
	u8 hw_id_h;
	u8 hw_id_l;
};

#define RX_CHECK_SUCCESS (1 << 0)
#define TX_CHECK_SUCCESS (1 << 1)
#define BOOT_CHECK_SUCCESS (1 << 2)

typedef enum {
    PPE_NONE,
    PPE_SS,
    PPE_HALL,
}PEN_PLACE_ERR;

enum FW_UPDATE_CMD {
    FW_UPDATE_NONE,
    FW_UPDATE_CHECK,
    FW_UPDATE_FORCE,
    FW_UPDATE_FROM_BIN,
    FW_UPDATE_ERASE,
    FW_UPDATE_AUTO,
    FW_UPDATE_MAX,
};

enum reverse_charge_state {
    REVERSE_STATE_OPEN,
    REVERSE_STATE_TIMEOUT,
    REVERSE_STATE_ENDTRANS,
    REVERSE_STATE_FORWARD,
    REVERSE_STATE_TRANSFER,
    REVERSE_STATE_WAITPING,
};

enum reverse_charge_mode {
    REVERSE_CHARGE_CLOSE = 0,
    REVERSE_CHARGE_OPEN,
};

struct wireless_rev_proc_data {
        bool wireless_reverse_closing;
        bool reverse_chg_en;
        bool user_reverse_chg;
        // bool bc12_reverse_chg;
        // bool batt_missing;
        // bool wired_chg_ok;
        int reverse_chg_sts;
        int int_flag;
        // //firmware update
        bool fw_updating;
        // bool only_check;
        // bool from_bin;
        // bool force_download;
        // bool user_update;
        // bool fw_erase;
        // bool power_on_update;
        // bool wls_sleep_fw_update;
        // bool wls_sleep_usb_insert;
        // int firmware_update_state;
        int ss_voltage;
};

struct sc96231 {
    struct device *dev;
    struct i2c_client *client;
    struct regmap *regmap;
    struct wireless_charger_device *chg_dev;
    // struct wireless_charger_properties chg_props;
    // irq and gpio
    int irq;
    int irq_gpio;
    int hall3_irq;
    int hall3_gpio;
    int hall4_irq;
    int hall4_gpio;
    int hall3_s_irq;
    int hall3_s_gpio;
    int hall4_s_irq;
    int hall4_s_gpio;
    int hall_ppe_n_irq;
    int hall_ppe_n_gpio;
    int hall_ppe_s_irq;
    int hall_ppe_s_gpio;
    int reverse_txon_gpio;
    int reverse_boost_gpio;
    // fw
    bool fw_program;
    int fw_data_size;
    unsigned char *fw_data_ptr;
    struct wls_fw_parameters *wls_fw_data;
    int fw_bin_length;
    unsigned char fw_bin[32768];
    // dts prop
    int project_vendor;
    int rx_role;
    int fw_version_index;
    // lock
    struct mutex i2c_rw_lock;
    struct mutex wireless_chg_int_lock;
    struct mutex hall_int_lock;
    // delay work
    struct delayed_work interrupt_work;
    struct delayed_work hall_irq_work;
    struct delayed_work ppe_hall_irq_work;
    //struct delayed_work monitor_work;
    struct delayed_work reverse_charge_config_work;
    struct delayed_work tx_ping_timeout_work;
    struct delayed_work tx_transfer_timeout_work;
    struct delayed_work tx_firmware_update;
    struct delayed_work pen_data_handle_work;
    struct delayed_work pen_place_err_check_work;
	struct delayed_work init_fw_check_work;
    //proc data
    struct wireless_rev_proc_data proc_data;
	struct power_supply	*batt_psy;
    // pen info
    int reverse_pen_soc;
    int reverse_pen_full;
    int reverse_vout;
    int reverse_iout;
    int reverse_ss;
    u8 pen_mac_data[6];
    int hall3_online;
    int hall4_online;
    int hall3_s_val;
    int hall4_s_val;
    int hall_ppe_n_val;
    int hall_ppe_s_val;
    //xm add
    int pen_place_err;
    bool support_tx_only;
    bool tx_timeout_flag;
    bool fw_update;
    bool using_bin;
};

extern int pen_charge_state_notifier_register_client(struct notifier_block *nb);
extern int pen_charge_state_notifier_unregister_client(struct notifier_block *nb);
#endif

