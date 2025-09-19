
/**
 * @file sc96281.h
 * @author <linjiashuo@xiaomi.com>
 * @data Feb 10 2025
 * @brief
 *  southchip sc96281 wireless charge!
*/
#ifndef __SC96281_HEADER__
#define __SC96281_HEADER__

#include <linux/power_supply.h>
#include "wireless_charger_class.h"
#include "sc96281_reg.h"
#include "../pmic_voter.h"
#include "../charger_class.h"

#define ADAPTER_VOL_MAX_MV			30000
#define ADAPTER_VOL_MIN_MV			4000
#define ADAPTER_VOL_DEFAULT_MV			6000
#define VOUT_SET_MAX_MV				19500
#define VOUT_SET_MIN_MV				4000
#define VOUT_SET_DEFAULT_MV			6000

//vout definition
#define BPP_DEFAULT_VOUT			6000
#define BPP_QC2_VOUT				6500
#define BPP_PLUS_VOUT				9000
#define EPP_DEFAULT_VOUT			11000
#define EPP_PLUS_VOUT				15000

// ichg vote value
#define RX_PMIC_ICL_DEFAULT_MA			1500
#define RX_PMIC_FCC_DEFAULT_MA			9200

//protect threshold definition
#define RX_MAX_IOUT_TRIG			2650
#define RX_MAX_IOUT_CLR				2000
#define RX_MAX_TEMP_TRIG			84
#define RX_MAX_TEMP_CLR				65

// interrupts foward definition
#define RX_INT_LDO_ON				0x0001	//BIT(0)
#define RX_INT_FAST_CHARGE			0x0002	//BIT(1)
#define RX_INT_AUTHEN_FINISH			0x0004	//BIT(2)
#define RX_INT_RENEGO_DONE			0x0008	//BIT(3)
#define RX_INT_ALARM_SUCCESS			0x0010	//BIT(4)
#define RX_INT_ALARM_FAIL			0x0020	//BIT(5)
#define RX_INT_OOB_GOOD				0x0040	//BIT(6)
#define RX_INT_RPP				0x0080	//BIT(7)
#define RX_INT_TRANSPARENT_SUCCESS		0x0100	//BIT(8)
#define RX_INT_TRANSPARENT_FAIL			0x0200	//BIT(9)
#define RX_INT_FACTORY_TEST			0x0400	//BIT(10)
#define RX_INT_ERR_CODE				0x0800	//BIT(11)
#define RX_INT_OCP_OTP_ALARM			0x1000	//BIT(12)
#define RX_INT_SLEEP				0x2000	//BIT(13)
#define RX_INT_POWER_OFF			0x4000	//BIT(14)
#define RX_INT_POWER_ON				0x8000	//BIT(15)

// interrupts reverse definition
#define RTX_INT_PING				0x0001
#define RTX_INT_GET_RX				0x0002
#define RTX_INT_CEP_TIMEOUT			0x0004
#define RTX_INT_EPT				0x0008
#define RTX_INT_PROTECTION			0x0010
#define RTX_INT_GET_TX				0x0020
#define RTX_INT_REVERSE_TEST_READY		0x0040
#define RTX_INT_REVERSE_TEST_DONE		0x0080
#define RTX_INT_FOD				0x0100
#define RTX_INT_EPT_PKT				0x0200
#define RTX_INT_ERR_CODE			0x0800

//factory test cmd
#define FACTORY_TEST_CMD			0x1F
#define FACTORY_TEST_CMD_ADAPTER_TYPE		0x0B
#define FACTORY_TEST_CMD_RX_IOUT		0x12
#define FACTORY_TEST_CMD_RX_VOUT		0x13
#define FACTORY_TEST_CMD_RX_CHIP_ID		0x23
#define FACTORY_TEST_CMD_RX_FW_ID		0x24
#define FACTORY_TEST_CMD_REVERSE_REQ		0x30

#define TRANS_DATA_LENGTH_1BYTE			0x18
#define TRANS_DATA_LENGTH_2BYTE			0x28
#define TRANS_DATA_LENGTH_3BYTE			0x38
#define TRANS_DATA_LENGTH_5BYTE			0x58

#define WLS_CHG_TX_QLIMIT_FCC_5W		1000
#define WLS_CHG_TX_QLIMIT_ICL_5W		400

#define SUPER_TX_FREQUENCY_MIN_KHZ		116
#define SUPER_TX_FREQUENCY_DEFAULT_KHZ		137
#define SUPER_TX_FREQUENCY_MAX_KHZ		141

#define SUPER_TX_VOUT_MIN_MV			20000
#define SUPER_TX_VOUT_PLAN_A_MV			32000
#define SUPER_TX_VOUT_PLAN_B_MV			34000
#define SUPER_TX_VOUT_MAX_MV			36000

#define WLS_TX_FAN_SPEED_MIN			0
#define WLS_TX_FAN_SPEED_QUIET			5
#define WLS_TX_FAN_SPEED_NORMAL			8
#define WLS_TX_FAN_SPEED_MAX			10

#define WLS_DEFAULT_TX_Q1			0x15
#define WLS_DEFAULT_TX_Q2			0x1B

//reverse charge timer
#define REVERSE_TRANSFER_TIMEOUT_TIMER		(100 * 1000)
#define REVERSE_PING_TIMEOUT_TIMER		(20 * 1000)
//reverse charge fod setting
#define REVERSE_FOD_GAIN			94
#define REVERSE_FOD_OFFSET			0
//driver name definition
#define SC96281_DRIVER_NAME			"sc96281"
//firmware check result
#define RX_CHECK_SUCCESS			(1 << 0)
#define TX_CHECK_SUCCESS			(1 << 1)
#define BOOT_CHECK_SUCCESS			(1 << 2)

#ifndef ABS
#define ABS(x) ((x) > 0 ? (x) : (-x))
#endif
#define ABS_CEP_VALUE 1

/* fod_para */
#define DEFAULT_FOD_PARAM_LEN			16
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

enum FW_UPDATE_CMD {
	FW_UPDATE_NONE,
	FW_UPDATE_ERASE = 97,
	FW_UPDATE_USER,
	FW_UPDATE_CHECK,
	FW_UPDATE_FORCE,
	FW_UPDATE_FROM_BIN,
	FW_UPDATE_MAX,
};

enum rev_boost_purpose {
	BOOST_FOR_FWUPDATE,
	BOOST_FOR_REVCHG,
	BOOST_PURPOSE_MAX,
};

enum reverse_chg_boost_src {
	PMIC_REV_BOOST,
	PMIC_HBOOST,
	EXTERNAL_BOOST,
	BOOST_SRC_MAX,
};

enum reverse_charge_mode {
	REVERSE_CHARGE_CLOSE = 0,
	REVERSE_CHARGE_OPEN,
};

enum reverse_chg_state {
	REVERSE_STATE_OPEN,
	REVERSE_STATE_TIMEOUT,
	REVERSE_STATE_ENDTRANS,
	REVERSE_STATE_FORWARD,
	REVERSE_STATE_TRANSFER,
	REVERSE_STATE_WAITPING,
};

enum reverse_chg_test_state {
	REVERSE_TEST_NONE,
	REVERSE_TEST_SCHEDULE = 1,
	REVERSE_TEST_PROCESSING,
	REVERSE_TEST_READY,
	REVERSE_TEST_DONE,
};

enum fod_param_id {
	FOD_PARAM_20V,
	FOD_PARAM_27V,
	FOD_PARAM_BPP_PLUS,
	FOD_PARAM_MAX,
};
struct params_t {
	uint8_t gain;
	int8_t  offset;
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

enum wls_power_mode {
	BPP_MODE,
	EPP_MODE,
};

enum tx_action {
	TX_ACTION_REPLY_ACK		= 0x00,
	TX_ACTION_REPLY_PACKAGE		= 0x01,
	TX_ACTION_NO_REPLY		= 0x02,
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

enum wls_low_inductance_tx_type {
	ADAPTER_LOW_INDUCTANCE_TX_50W,
	ADAPTER_LOW_INDUCTANCE_TX_80W,
	ADAPTER_LOW_INDUCTANCE_TX_TYPE_MAX,
};

enum ADAPTER_CMD_TYPE {
	ADAPTER_CMD_TYPE_NONE = 0,
	ADAPTER_CMD_TYPE_F0 = 0XF0,
	ADAPTER_CMD_TYPE_F1 = 0XF1,
	ADAPTER_CMD_TYPE_F2 = 0XF2,
	ADAPTER_CMD_TYPE_F3 = 0XF3,
	ADAPTER_CMD_TYPE_F4 = 0XF4,
};

typedef enum {
	UNKNOWN_PACKET,
	WLS_SOC_PACKET,
	WLS_Q_STARTEGY_PACKET,
	WLS_FAN_SPEED_PACKET,
	WLS_VOUT_RANGE_PACKET,
	WLS_FREQUENCE_PACKET,
}WLS_TRANS_PACKET_TYPE;

enum WLS_DEBUG_CMD {
	WLS_DEBUG_FCC = 1,
	WLS_DEBUG_ICL,
	WLS_DEBUG_EPP_FOD_SINGLE,
	WLS_DEBUG_EPP_FOD_ALL,
	WLS_DEBUG_EPP_FOD_ALL_DIRECTLY, // without uuid check
	WLS_DEBUG_MAX,
};

enum WLS_CP_FORWARD_CHARGER_MODE {
	FORWARD_4_1_CHARGER_MODE = 0,
	FORWARD_2_1_CHARGER_MODE = 1,
};

#define DECL_INTERRUPT_MAP(regval, redir_irq) {\
	.irq_regval = regval,\
	.irq_flag = redir_irq, \
}
struct int_map_t {
	uint32_t irq_regval;
	uint16_t irq_flag;
};

struct wls_fw_parameters {
	u8 fw_rx_id;
	u8 fw_tx_id;
	u8 fw_boot_id;
	u8 hw_id_h;
	u8 hw_id_l;
};

struct wls_bin_parameters{
	u16 total_length;
	u8 fw_area;
	u16 block_num;
	unsigned char wls_bin[32768];
};

typedef enum trans_data_flag {
	TRANS_DATA_FLAG_NONE = 0,
	TRANS_DATA_FLAG_SOC,
	TRANS_DATA_FLAG_QVALUE,
	TRANS_DATA_FLAG_FAN_SPEED,
	TRANS_DATA_FLAG_VOUT_RANGE,
	TRANS_DATA_FLAG_FREQUENCE,
} TRANS_DATA_FLAG;

struct trans_data_lis_node {
	struct list_head lnode;
	TRANS_DATA_FLAG data_flag;
	int value;
};

struct sc96281_chg {
	struct i2c_client	*client;
	struct device		*dev;
	struct regmap		*regmap;
	// irq and gpio
	unsigned int		tx_on_gpio;
	unsigned int		reverse_boost_gpio;
	unsigned int		irq_gpio;
	unsigned int		power_good_gpio;
	unsigned int		power_good_irq;
	unsigned int		rx_sleep_gpio;
	// lock
	struct mutex		wireless_chg_int_lock;
	struct mutex		data_transfer_lock;
	struct mutex		i2c_rw_lock;
	spinlock_t		list_lock;
	bool			mutex_lock_sts;
	//list
	struct list_head	header;
	wait_queue_head_t	wait_que;
	int			head_cnt;
	// delay works
	struct delayed_work	wireless_int_work;
	struct delayed_work	wireless_pg_det_work;
	struct delayed_work	chg_monitor_work;
	struct delayed_work	init_detect_work;
	struct delayed_work	init_fw_check_work;
	struct delayed_work	rx_alarm_work;
	struct delayed_work	i2c_check_work;
	struct delayed_work	renegociation_work;
	struct delayed_work	reverse_chg_config_work;
	struct delayed_work	reverse_chg_monitor_work;
	struct delayed_work	reverse_transfer_timeout_work;
	struct delayed_work	reverse_ping_timeout_work;
	struct delayed_work	factory_reverse_start_work;
	struct delayed_work	factory_reverse_stop_work;
	struct delayed_work	trans_data_work;
	struct delayed_work	mutex_unlock_work;
	// alarm
	struct alarm		reverse_dping_alarm;
	struct alarm		reverse_chg_alarm;
	//vote
	struct votable		*fcc_votable;
	struct votable		*icl_votable;
	// wireless charge device
	struct wireless_charger_device *wlschgdev;
	struct charger_device	*master_cp_dev;
	struct charger_device	*chg_dev;
	const char		*wlsdev_name;
	// charger device
	struct charger_device	*cp_master_dev;
	// power supply
	struct power_supply	*batt_psy;
	struct power_supply	*wireless_psy;
	struct regulator	*pmic_boost;
	struct wakeup_source	*wls_wakelock;
	// fw params
	bool			fw_update;
	int			fw_version_index;
	int			fw_version_index_default;
	int			fw_version_index_jp;
	bool			fw_version_reflash;
	unsigned char		*fw_data_ptr;
	int			fw_data_size;
	struct wls_fw_parameters *wls_fw_data;
	const char		*fw_upgrade_fail_info;
	//fw_bin
	bool			fw_program;
	int			fw_bin_length;
	unsigned char		fw_bin[32768];
	// fod & debug_fod
	int			fod_params_size;
	struct fod_params_t	fod_params[FOD_PARA_MAX_GROUP];
	struct fod_params_t	fod_params_default;
	uint8_t			wls_debug_one_fod_index;
	int			wls_debug_set_fod_type;
	struct params_t		wls_debug_one_fod_param;
	struct fod_params_t	*wls_debug_all_fod_params;
	// revchg
	bool			reverse_chg_en;
	bool			bc12_reverse_chg;
	bool			user_reverse_chg;
	bool			is_reverse_boosting;
	bool			is_reverse_closing;
	int			is_reverse_chg;
	int			reverse_boost_src;
	int			revchg_boost_vol;
	int			fwupdate_boost_vol;
	int			revchg_test_status;
	// wireless compatible info
	bool			is_car_tx;
	bool			is_music_tx;
	bool			is_train_tx;
	bool			is_plate_tx;
	bool			is_sailboat_tx;
	bool			is_standard_tx;
	bool			is_support_fan_tx;
	bool			low_inductance_50w_tx;
	bool			low_inductance_80w_tx;
	// transparent params
	int			quiet_sts;
	int			tx_speed;
	bool			q_value_supprot;
	u8			tx_q1[ADAPTER_LOW_INDUCTANCE_TX_TYPE_MAX];
	u8			tx_q2[ADAPTER_LOW_INDUCTANCE_TX_TYPE_MAX];
	bool			is_vout_range_set_done;
	// basic chg params
	u8			epp;
	u8			uuid[4];
	u8			set_fastcharge_vout_cnt;
	u8			set_tx_voltage_cnt;
	u8			ss;
	u8			power_good_flag;
	u8			enable_flag;
	u8			fc_flag;
	u16			adapter_type;
	int			batt_soc;
	int			raw_soc;
	int			batt_temp;
	int			target_vol;
	int			target_curr;
	int			pre_curr;
	int			pre_vol;
	int			vout_setted;
	int			chg_status;
	int			chg_phase;
	int			rx_temp;
	int 			rx_fcc;
	int			rx_icl;
	int			cp_chg_mode;
	int			qc_type;
	bool			force_cp_2_1_mode;
	bool			parallel_charge;
	bool			qc_enable;
	bool			i2c_ok_flag;
	bool			oxp_scheduled_flag;
	bool			tx_timeout_flag;

	enum ADAPTER_CMD_TYPE	current_for_adapter_cmd;
	ask_pkt_t		sent_pri_packet;
	WLS_TRANS_PACKET_TYPE	current_trans_packet_type;
};
#endif
