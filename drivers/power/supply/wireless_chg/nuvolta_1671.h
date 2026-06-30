
/**
 * @file nuvolta_1671.h
 * @author <xiezhichang@xiaomi.com>
 * @data October 10 2025
 * @brief
 *  nuvolta 1671 wireless charge!
*/
#ifndef __NUVOLTA_1671_HEADER__
#define __NUVOLTA_1671_HEADER__
#include "wireless_charger_class.h"
#include "pmic_voter.h"
#include <linux/power_supply.h>
#include "charger_class.h"
//registers define
#define REG_RX_REV_CMD 0x0020
#define REG_RX_REV_DATA1 0x0021
#define RX_POWER_OFF_ERR 0x0028
#define TRX_MODE_EN 0x0063
#define RX_RTX_MODE 0x002f
#define RX_DATA_INFO 0x1200
#define RX_POWER_MODE 0x1203
#define RX_FASTCHG_RESULT 0x1209
#define RX_SS_VOLTAGE 0x126A

#define TRX_MODE_STATUS					0x03

#define REG_GPIO_RAIL_CTRL				0x1153
#define RX_CMD_SET_1P8V					0x3f

#define REG_MTP_CTRL1					0x0019
#define RX_CMD_EXECUTE_HIGH				0x01
#define RX_CMD_EXECUTE_LOW				0x00

#define REG_MTP_CTRL2					0x001a
#define RX_CMD_ENTER_WRITE_MODE			0x5a
#define RX_CMD_ENTER_READ_MODE			0xa5
#define RX_CMD_EN_READ					0x04
#define RX_CMD_ENTER_TRIM				0x3c

#define REG_DTM_UNLOCK_REG0				0x2017
#define REG_DTM_UNLOCK_REG1				0x2020

#define RX_FW_DATA_DEFAULT_LENGTH		32768

#define RX_FW_HANDLE_STEP				4
#define RX_FW_FORMAT_LENGTH				4096
#define RX_FW_WRITE_CHECK_COUNT			250
#define RX_FW_HANDLE_DELAY_MS			20
#define RX_FW_CHECK_DELAY_US			100
#define RX_FW_EXIT_DELAY_MS				100

#define REG_MTP_CTRL_DATA0				0x001c
#define REG_MTP_CTRL_DATA1				0x001d
#define REG_MTP_CTRL_DATA2				0x001e
#define REG_MTP_CTRL_DATA3				0x001f
#define RX_CMD_ERASE_MASK				0xFF

#define REG_MTP_STATUS					0x001b
#define MTP_STATUS_MASK					0x80
#define MTP_WRITE_RESULT_MASK			0x40
#define MTP_STATUS_SHIFT				7
#define MTP_WRITE_RESULT_SHIFT			6

#define REG_RX_INT_0					0x0060
#define REG_RX_INT_1					0x0061
#define REG_RX_INT_2					0x0062
#define REG_RX_INT_3					0x0063
#define RX_CMD_START_READ				0x88
#define RX_CMD_ENABLE_CRC				0x02
#define RX_CMD_ENABLE_TX				0x01
#define RX_CMD_DISABLE_TX				0x00

#define FW_VERSION_BUF_LENGTH			7
#define CRC_WORK_DELAY_US				100000
#define CRC_CHECK_SUCCESS				0x66
#define CRC_CHECK_ERR_VER				0xFE

/* used registers define */
#define REG_RX_SENT_CMD					0x0000
#define REG_RX_SENT_DATA1				0x0001
#define REG_RX_SENT_DATA2				0x0002
#define REG_RX_SENT_DATA3				0x0003
#define REG_RX_SENT_DATA4				0x0004
#define REG_RX_SENT_DATA5				0x0005
#define REG_RX_SENT_DATA6				0x0006
#define REG_RX_SENT_DATA8				0x0008
#define REG_RX_SENT_DATA9				0x0009
#define REG_RX_SENT_DATAA				0x000A
#define REG_RX_SENT_DATAB				0x000B
#define REG_RX_SENT_DATAC				0x000C
#define REG_RX_SENT_DATAD				0x000D
#define REG_RX_REV_CMD					0x0020
#define REG_RX_REV_DATA1				0x0021
#define REG_RX_REV_DATA2				0x0022
#define REG_RX_REV_DATA3				0x0023
#define REG_RX_REV_DATA4				0x0024
#define REG_RX_REV_DATA5				0x0025
#define REG_RX_REV_DATA8				0x0028
#define REG_RX_REV_DATAF				0x002f

#define I2C_CMD_CHECK_RETRY_COUNT		100
#define I2C_CMD_CHECK_ADDR				0x232c
#define I2C_CMD_CHECK_BUSY				0x55
#define RX_CMD_I2C_CHECK_MASK			0x88

#define RX_AUTH_DATA_LENGTH				40
#define ADAPTER_TYPE_SHIFT				7
#define AUTH_RESULT_SHIFT				8
#define POWER_MODE_SHIFT				3
#define MAX_POWER_SHIFT					5
#define TX_ID_LOW_SHIFT					26
#define TX_ID_HIGH_SHIFT				27
#define UUID0_SHIFT						28
#define UUID1_SHIFT						29
#define UUID2_SHIFT						30
#define UUID3_SHIFT						31
#define TX_MAC_ADDR0_SHIFT				34
#define TX_MAC_ADDR1_SHIFT				33
#define TX_MAC_ADDR2_SHIFT				32
#define TX_MAC_ADDR3_SHIFT				38
#define TX_MAC_ADDR4_SHIFT				37
#define TX_MAC_ADDR5_SHIFT				36

#define RX_CLEAR_INT_LENGTH				0x02
#define RX_CLEAR_INT_DATA				0xFF
#define RX_CLEAR_INT_TRIGGER_RX			0x04

#define RX_CMD_CLEAR_CEP				0x21
#define RX_CMD_ENABLE_REVERSE_FOD		0x23
#define RX_CMD_SET_RX_VOUT				0x31
#define RX_CMD_CLEAR_INT				0x68
#define RX_CMD_TRANSMIT_PACKET			0x69
#define RX_CMD_FOD_SET					0x98
#define RX_CMD_RENEGO_SET				0xA8
#define RX_CMD_ALARM_VOL				0xA9
#define RX_CMD_ENABLE_BLE				0xAA
#define TX_CMD_SET_TX_AUTHEN			0x6A
#define REVERSE_FOD_EN					0x01
#define REVERSE_FOD_DIS					0x00
#define REVERSE_FOD_DEFAULT_GAIN		94
#define REVERSE_FOD_TRIGGER_RX			0x04
#define DISABLE_REVERSE_FOD_TRIGGER_RX	0x02

#define BPP_PLUS_FOD_SET_CMD_LENGTH		0x0D
#define FOD_PARAMS_MAX_LENGTH			5
#define FOD_SET_DELAY_MS				20
#define FOD_SET_TRIGGER_RX				15

#define RECEIVE_DATA_MAX_COUNT			50
#define RECEIVE_DATA_LENGTH_SHIFT		40
#define RECEIVE_DATA_SHIFT				41

#define REG_MTP_STATE_PIN				0x1001
#define RX_CMD_MTP_STATE_EN				0x80

#define REG_SECTOR_SELECT_REG			0x0012
#define SECTOR_SELECT_MASK				0xff

#define I2C_READ_RX_BUF_LENGTH			30
#define GET_RX_TEMP_SHIFT0				14
#define GET_RX_TEMP_SHIFT1				15
#define GET_IOUT_SHIFT0					16
#define GET_IOUT_SHIFT1					17
#define VRECT_SHIFT0					18
#define VRECT_SHIFT1					19
#define GET_VOUT_SHIFT0					20
#define GET_VOUT_SHIFT1					21
#define GET_CEP_SHIFT					10

#define ADAPTER_VOL_MAX_MV				30000
#define ADAPTER_VOL_MIN_MV				4000
#define ADAPTER_VOL_DEFAULT_MV			6000
#define ADAPTER_VOL_PACKET_LENGTH		0x05
#define ADAPTER_VOL_SET_TYPE			0x02
#define ADAPTER_VOL_TRIGGER_RX			0x07
#define RX_CMD_FASTCHG_SET_TX_VOLTAGE	0x0a

#define TRANS_DATA_LENGTH_1BYTE			0x18
#define TRANS_DATA_LENGTH_2BYTE			0x28
#define TRANS_DATA_LENGTH_3BYTE			0x38
#define TRANS_DATA_LENGTH_5BYTE			0x58

#define VOUT_SET_MAX_MV					19500
#define VOUT_SET_MIN_MV					4000
#define VOUT_SET_DEFAULT_MV				6000
#define VOUT_SET_PACKET_LENGTH			0x02
#define VOUT_SET_TRIGGER_RX				0x04

#define REG_MCU_CTRL_REG				0x1000
#define RX_CMD_DIS_MCU_1665				0xc0
#define RX_CMD_DIS_MCU_1651				0x90
#define RX_CMD_DIS_MCU_EN_TRIM			0xe0

#define REG_RX_SLEEP_CTRL_REG			0x0090
#define RX_CMD_DISABLE_SLEEP			0x41


#define FUDA1665_DTM_REG0_DATA0			0x2d
#define FUDA1665_DTM_REG0_DATA1			0xd2
#define FUDA1665_DTM_REG0_DATA2			0x22
#define FUDA1665_DTM_REG0_DATA3			0xdd
#define FUDA1651_DTM_REG0_DATA0			0x69
#define FUDA1651_DTM_REG0_DATA1			0x96
#define FUDA1651_DTM_REG0_DATA2			0x66
#define FUDA1651_DTM_REG0_DATA3			0x99
#define FUDA1665_DTM_REG1_DATA0			0x4b
#define FUDA1665_DTM_REG1_DATA1			0xb4
#define FUDA1665_DTM_REG1_DATA2			0x44
#define FUDA1665_DTM_REG1_DATA3			0xbb
#define FUDA1651_DTM_REG1_DATA0			0x78
#define FUDA1651_DTM_REG1_DATA1			0x87
#define FUDA1651_DTM_REG1_DATA2			0x1e
#define FUDA1651_DTM_REG1_DATA3			0xe1

#define REG_CONFIRM_DATA				0x008b

#define REG_MTP_CTRL0					0x0017
#define RX_CMD_WRITE_ENABLE				0x01

#define RENEGO_LENGTH					0x01
#define RENEGO_TRIGGER_RX				0x03

#define TRX_ISENSE_HIGH	0x11
#define TRX_ISENSE_LOW	0x10
#define TRX_VRECT_HIGH	0x13
#define TRX_VRECT_LOW	0x12

#define TRANS_DATA_LENGTH_1BYTE	0x18
#define TRANS_DATA_LENGTH_2BYTE	0x28
#define TRANS_DATA_LENGTH_3BYTE	0x38
#define TRANS_DATA_LENGTH_5BYTE	0x58

#define RX_OFFSET_THRESHOLD 5000
#define I2C_RETRY_CNT 3
//vout definition
#define BPP_DEFAULT_VOUT 6000
#define BPP_QC2_VOUT 6500
#define BPP_PLUS_VOUT 9000
#define EPP_DEFAULT_VOUT 11000
#define EPP_PLUS_VOUT 15000
//protect threshold definition
#define RX_MAX_IOUT 2650
#define RX_MAX_TEMP 84
//reverse charge timer
#define REVERSE_TRANSFER_TIMEOUT_TIMER (100 * 1000)
#define REVERSE_PING_TIMEOUT_TIMER (20 * 1000)
// interrupts foward definition
#define RX_INT_LDO_ON                   0x0001
#define RX_INT_FAST_CHARGE              0x0002
#define RX_INT_AUTHEN_FINISH            0x0004
#define RX_INT_RENEGO_DONE              0x0008
#define RX_INT_ALARM_SUCCESS            0x0010
#define RX_INT_ALARM_FAIL               0x0020
#define RX_INT_OOB_GOOD                 0x0040
#define RX_INT_RPP                      0x0080
#define RX_INT_TRANSPARENT_SUCCESS      0x0100
#define RX_INT_TRANSPARENT_FAIL         0x0200
#define RX_INT_FACTORY_TEST             0x0400
#define RX_INT_OCP_OTP_ALARM            0x1000
#define RX_INT_POWER_OFF                0x4000
#define RX_INT_POWER_ON                 0x8000
//power pack
#define RX_INT_POWER_REDUCE_F0 0xF0
#define RX_INT_POWER_REDUCE_F1 0xF1
#define RX_INT_POWER_REDUCE_F2 0xF2
#define RX_INT_POWER_REDUCE_F3 0xF3
#define RX_INT_POWER_REDUCE_F4 0xF4

// interrupts reverse definition
#define RTX_INT_PING                    0x0001
#define RTX_INT_GET_RX                  0x0002
#define RTX_INT_CEP_TIMEOUT             0x0004
#define RTX_INT_EPT                     0x0008
#define RTX_INT_PROTECTION              0x0010
#define RTX_INT_GET_TX                  0x0020
#define RTX_INT_REVERSE_TEST_READY      0x0040
#define RTX_INT_REVERSE_TEST_DONE       0x0080
#define RTX_INT_FOD                     0x0100
#define RTX_INT_EPT_PKT                 0x0200
#define RTX_INT_ERR_CODE                0x0800
//factory test cmd
#define FACTORY_TEST_CMD 0x1F
#define FACTORY_TEST_CMD_ADAPTER_TYPE 0x0B
#define FACTORY_TEST_CMD_RX_IOUT 0x12
#define FACTORY_TEST_CMD_RX_VOUT 0x13
#define FACTORY_TEST_CMD_RX_CHIP_ID 0x23
#define FACTORY_TEST_CMD_RX_FW_ID 0x24
#define FACTORY_TEST_CMD_REVERSE_REQ 0x30
//reverse charge timer
#define REVERSE_CHG_CHECK_DELAY_MS 100000
#define REVERSE_DPING_CHECK_DELAY_MS 10000
//reverse charge fod setting
#define REVERSE_FOD_GAIN 94
#define REVERSE_FOD_OFFSET 0
//transparent data
#define SUPER_TX_VOUT_MIN_MV			20000
#define SUPER_TX_VOUT_PLAN_A_MV			32000
#define SUPER_TX_VOUT_PLAN_B_MV			34000
#define SUPER_TX_VOUT_MAX_MV			36000

#define SUPER_TX_FREQUENCY_DEFAULT_KHZ		137
#define SUPER_TX_FREQUENCY_MIN_KHZ		116
#define SUPER_TX_FREQUENCY_MAX_KHZ		141

#define WLS_TX_FAN_SPEED_MIN			0
#define WLS_TX_FAN_SPEED_QUIET			5
#define WLS_TX_FAN_SPEED_NORMAL			8
#define WLS_TX_FAN_SPEED_MAX			10

#define SUPER_TX_FAN_SPEED_MIN_PERCENT 0
#define SUPER_TX_FAN_SPEED_MAX_PERCENT 10

#define WLS_DEFAULT_TX_Q1			0x15
#define WLS_DEFAULT_TX_Q2			0x1B
#define WLS_CHG_TX_QLIMIT_FCC_5W		1000
#define WLS_CHG_TX_QLIMIT_ICL_5W		400
//driver name definition
#define NUVOLTA_1671_DRIVER_NAME "nuvolta_1671"
//firmware check result
#define RX_CHECK_SUCCESS (1 << 0)
#define TX_CHECK_SUCCESS (1 << 1)
#define BOOT_CHECK_SUCCESS (1 << 2)
#ifndef ABS
#define ABS(x) ((x) > 0 ? (x) : (-x))
#endif
#define ABS_CEP_VALUE 1
/* fod_para */
#define DEFAULT_FOD_PARAM_LEN			20
enum PARAMS_T {
	PARAMS_T_GAIN,
	PARAMS_T_OFFSET,
	PARAMS_T_MAX,
};
#define FOD_PARA_MAX_GROUP			20
enum fod_para_ele {
	FOD_PARA_TYPE = 0,
	FOD_PARA_LENGTH,
	FOD_PARA_UUID,
	FOD_PARA_PARAMS,
	FOD_PARA_MAX,
};
#define nuvolta_err(fmt, ...)							\
do {										\
	if (log_level >= 0)							\
		printk(KERN_ERR "[wls_nu1671] " fmt, ##__VA_ARGS__);	\
} while (0)
#define nuvolta_info(fmt, ...)							\
do {										\
	if (log_level >= 1)							\
		printk(KERN_ERR "[wls_nu1671] " fmt, ##__VA_ARGS__);	\
} while (0)
#define nuvolta_dbg(fmt, ...)							\
do {										\
	if (log_level >= 2)							\
		printk(KERN_ERR "[wls_nu1671] " fmt, ##__VA_ARGS__);	\
} while (0)
enum FW_UPDATE_CMD {
	FW_UPDATE_NONE,
	FW_UPDATE_ERASE = 97,
	FW_UPDATE_USER,
	FW_UPDATE_CHECK,
	FW_UPDATE_FORCE,
	FW_UPDATE_FROM_BIN,
	FW_UPDATE_MAX,
};
enum reverse_chg_state {
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

enum fod_param_id {
	FOD_PARAM_20V,
	FOD_PARAM_27V,
	FOD_PARAM_BPP_PLUS,
	FOD_PARAM_MAX,
};
struct params_t {
	u8 gain;
	u8 offset;
};
struct fod_params_t {
	u8 params_nums;
	u8 type;
	u8 length;
	int uuid;
	struct params_t params[DEFAULT_FOD_PARAM_LEN];
};
enum auth_status {
	AUTH_STATUS_FAILED,
	AUTH_STATUS_SHAR1_OK,
	AUTH_STATUS_USB_TYPE_OK,
	AUTH_STATUS_UUID_OK = 4,
	AUTH_STATUS_TX_MAC_OK = 6,
};
enum wls_chg_stage {
	NORMAL_MODE = 1,
	TAPER_MODE,
	FULL_MODE,
	RECHG_MODE,
};
enum wls_work_mode {
	RX_MODE,
	RTX_MODE,
};
enum wls_adapter_type {
	ADAPTER_NONE,
	ADAPTER_SDP,
	ADAPTER_CDP,
	ADAPTER_DCP,
	ADAPTER_QC2 = 5,
	ADAPTER_QC3,
	ADAPTER_PD,
	ADAPTER_AUTH_FAILED,
	ADAPTER_XIAOMI_QC3,
	ADAPTER_XIAOMI_PD,
	ADAPTER_ZIMI_CAR_POWER,
	ADAPTER_XIAOMI_PD_40W,
	ADAPTER_VOICE_BOX,
	ADAPTER_XIAOMI_PD_50W,
	ADAPTER_XIAOMI_PD_60W,
	ADAPTER_XIAOMI_PD_100W,
};
typedef enum {
	UNKNOWN,
	ADAPTER_CMD_TYPE_F0,
	ADAPTER_CMD_TYPE_F1,
	ADAPTER_CMD_TYPE_F2,
	ADAPTER_CMD_TYPE_F3,
	ADAPTER_CMD_TYPE_F4,
}ADAPTER_CMD_TYPE;

typedef enum {
	UNKNOWN_PACKET,
	WLS_SOC_PACKET,
	WLS_Q_STARTEGY_PACKET,
	WLS_FAN_SPEED_PACKET,
	WLS_VOUT_RANGE_PACKET,
	WLS_FREQUENCE_PACKET,
}WLS_TRANS_PACKET_TYPE;

enum reverse_chg_boost_src {
	PMIC_REV_BOOST,
	PMIC_HBOOST,
	EXTERNAL_BOOST,
	BOOST_SRC_MAX,
};

enum rev_boost_purpose {
	BOOST_FOR_FWUPDATE,
	BOOST_FOR_REVCHG,
	BOOST_PURPOSE_MAX,
};

enum reverse_chg_test_state {
	REVERSE_TEST_NONE,
	REVERSE_TEST_SCHEDULE = 1,
	REVERSE_TEST_PROCESSING,
	REVERSE_TEST_READY,
	REVERSE_TEST_DONE,
};

enum wls_low_inductance_tx_type {
	ADAPTER_LOW_INDUCTANCE_TX_50W,
	ADAPTER_LOW_INDUCTANCE_TX_80W,
	ADAPTER_LOW_INDUCTANCE_TX_TYPE_MAX,
};

typedef enum trans_data_flag {
	TRANS_DATA_FLAG_NONE = 0,
	TRANS_DATA_FLAG_SOC,
	TRANS_DATA_FLAG_QVALUE,
	TRANS_DATA_FLAG_FAN_SPEED,
	TRANS_DATA_FLAG_VOUT_RANGE,
	TRANS_DATA_FLAG_FREQUENCE,
} TRANS_DATA_FLAG;

enum tx_action {
	TX_ACTION_REPLY_ACK = 0x00,
	TX_ACTION_REPLY_PACKAGE = 0x01,
	TX_ACTION_NO_REPLY = 0x02,
};

struct trans_data_lis_node {
	struct list_head lnode;
	TRANS_DATA_FLAG data_flag;
	int value;
};

struct nuvolta_1671_chg {
	struct i2c_client		*client;
	struct device			*dev;
	struct regmap			*regmap;
	// irq and gpio
	unsigned int tx_on_gpio;
	unsigned int reverse_boost_gpio;
	unsigned int irq_gpio;
	unsigned int power_good_gpio;
	unsigned int power_good_irq;
	unsigned int enable_gpio;
	// delay works
	struct delayed_work    wireless_int_work;
	struct delayed_work    wireless_pg_det_work;
	struct delayed_work    chg_monitor_work;
	struct delayed_work    reverse_chg_state_work;
	struct delayed_work    reverse_dping_state_work;
	struct delayed_work    init_detect_work;
	struct delayed_work    factory_reverse_start_work;
	struct delayed_work    factory_reverse_stop_work;
	struct delayed_work    rx_alarm_work;
	struct delayed_work    i2c_check_work;
	struct delayed_work    renegociation_work;
	struct delayed_work    trans_data_work;
	struct delayed_work    mutex_unlock_work;
	struct delayed_work    init_fw_check_work;
	// lock
	struct mutex    wireless_chg_int_lock;
	struct mutex data_transfer_lock;
	struct mutex i2c_lock;
	spinlock_t list_lock;
	bool mutex_lock_sts;
	//list
	struct list_head	header;
	wait_queue_head_t	wait_que;
	int head_cnt;
	// alarm
	struct alarm	reverse_dping_alarm;
	struct alarm	reverse_chg_alarm;
	//vote
	struct votable *fcc_votable;
	struct votable *icl_votable;
	// wireless charge device
	struct wireless_charger_device *wlschgdev;
	struct charger_device *master_cp_dev;
	struct charger_device *chg_dev;
	const char *wlsdev_name;
	// charger device
	struct charger_device *cp_master_dev;
	// power supply
	struct power_supply *batt_psy;
	struct power_supply *wireless_psy;
	struct regulator *pmic_boost;
	struct wakeup_source *wls_wakelock;
	//fw
	struct wls_fw_parameters *wls_fw_data;
	int fw_bin_length;
	int	fw_version_index;
	int	fw_version_index_default;
	int	fw_version_index_jp;
	int	fw_data_size;
	unsigned char *fw_data_ptr;
	//fod
	int	fod_params_size;
	int fod_params_size_2_1;
	struct fod_params_t	fod_params[FOD_PARA_MAX_GROUP];
	struct fod_params_t	fod_params_2_1[FOD_PARA_MAX_GROUP];
	struct fod_params_t	fod_params_default;
	struct fod_params_t fod_params_bpp_plus;
	struct fod_params_t wls_debug_all_fod_params;
	//rev chg
	struct delayed_work	reverse_chg_config_work;
	struct delayed_work	reverse_chg_monitor_work;
	struct delayed_work	reverse_transfer_timeout_work;
	struct delayed_work	reverse_ping_timeout_work;
	bool reverse_chg_en;
	bool bc12_reverse_chg;
	bool user_reverse_chg;
	bool is_reverse_boosting;
	bool is_reverse_closing;
	bool fw_version_reflash;
	bool tx_timeout_flag;
	int is_reverse_chg;
	int reverse_boost_src;
	int revchg_boost_vol;
	int fwupdate_boost_vol;
	int revchg_test_status;
	//HALL
	struct delayed_work hall_interrupt_work;
	int support_hall;
	unsigned int hall_n_int_gpio;
	unsigned int hall_s_int_gpio;
	unsigned int hall_n_int_irq;
	unsigned int hall_s_int_irq;
	bool hall_n_gpio_status;
	bool hall_s_gpio_status;
	bool magnetic_case_flag;
	// transparent params
	int quiet_sts;
	bool is_support_fan_tx;
	ADAPTER_CMD_TYPE current_for_adapter_cmd;
	WLS_TRANS_PACKET_TYPE current_trans_packet_type;
	int tx_speed;
	u8 tx_q1[ADAPTER_LOW_INDUCTANCE_TX_TYPE_MAX];
	u8 tx_q2[ADAPTER_LOW_INDUCTANCE_TX_TYPE_MAX];
	bool is_vout_range_set_done;
	bool q_value_supprot;
	bool low_inductance_50w_tx;
	bool low_inductance_80w_tx;
	// driver parameters
	u8 epp;
	u8 epp_tx_id_h;
	u8 epp_tx_id_l;
	u8 fc_flag;
	u8 uuid[4];
	u8 power_good_flag;
	u8 set_fastcharge_vout_cnt;
	u8 ss;
	u16 adapter_type;
	u16	adapter_type_first;
	int	qc_type;
	int batt_soc;
	int raw_soc;
	int batt_temp;
	int target_vol;
	int target_curr;
	int pre_curr;
	int pre_vol;
	int vout_setted;
	int chg_status;
	int chg_phase;
	int	cp_chg_mode;
	bool fw_update;
	bool is_car_tx;
	bool is_music_tx;
	bool is_train_tx;
	bool is_plate_tx;
	bool is_standard_tx;
	bool parallel_charge;
	bool qc_enable;
	bool alarm_flag;
	bool i2c_ok_flag;
	bool shutdown_flag;
	bool is_sailboat_tx;
	bool force_cp_2_1_mode;
	bool pg_low_debounce;
	u8 set_tx_voltage_cnt;
	u8 enable_flag;
};
struct wls_fw_parameters {
	u8 fw_version;
	u8 hw_id_h;
	u8 hw_id_l;
};
struct wls_bin_parameters{
	u16 total_length;
	u8 fw_area;
	u16 block_num;
	unsigned char wls_bin[32768];
};
#endif
