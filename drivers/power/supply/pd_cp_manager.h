
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>

#define PDM_SM_DELAY_1000MS 1000
#define PDM_SM_DELAY_300MS	300
#define PDM_SM_DELAY_200MS	200
#define PDM_SM_DELAY_500MS	500
#define PDM_SM_DELAY_100MS	100

#define LARGE_IBAT_DIFF		650
#define MEDIUM_IBAT_DIFF	150
#define LARGE_VBAT_DIFF		300
#define MEDIUM_VBAT_DIFF	200
#define LARGE_IBUS_DIFF		500
#define MEDIUM_IBUS_DIFF	100
#define SMALL_IBUS_DIFF		0
#define LARGE_VBUS_DIFF		800
#define MEDIUM_VBUS_DIFF	400
#ifdef CONFIG_FACTORY_BUILD
#define MAX_CABLE_RESISTANCE	550
#else
#define MAX_CABLE_RESISTANCE	1000
#endif

#define LARGE_STEP		5
#define MEDIUM_STEP		2
#define SMALL_STEP		1

#define LOW_POWER_PD2_VINMIN 	4500
#define LOW_POWER_PD2_ICL 	1000

#define DEFAULT_PDO_VBUS_1S	5000
#define DEFAULT_PDO_IBUS_1S	3000

#define MIN_JEITA_CHG_INDEX	2
#define MAX_JEITA_CHG_INDEX	4
#if  defined (CONFIG_TARGET_PRODUCT_YUECHU)
#define MIN_THERMAL_LIMIT_FCC	1500
#elif defined (CONFIG_TARGET_PRODUCT_ARISTOTLE)
#define MIN_THERMAL_LIMIT_FCC	2000
#else
#define MIN_THERMAL_LIMIT_FCC	2500
#endif
#define MIN_THERMAL_LIMIT_FCC_BYPASS	1600
#define BQ_TAPER_HYS_MV             10
#define BQ_TAPER_GT_CV_TIMES                3

/* define daumier macro */
#define MIN_THERMAL_LIMIT_FCC_DAUMIER 500
#define MIN_1_1_CHARGE_CURRENT  600
#define MIN_2_1_CHARGE_CURRENT  1000
#define MIN_4_1_CHARGE_CURRENT  2000

#define MAX_WATT_33W		34000000
#define MAX_WATT_67W		67000000
#define MAX_VBUS_67W		12000
#define MAX_IBUS_67W		6200
#define MIN_IBUS_67W		2200
#define MIN_IBUS_67W_DAUMIER		2000
#define SECOND_IBUS_67W		6100
#define PD2_VBUS		    9000
#define MAX_VBUS_90W		17000
#define MIN_IBUS_90W		1600


/* defined for non_verified pps charger maxium fcc */
#define NON_VERIFIED_PPS_FCC_MAX		4800// J7A: 4100
/* defined min fcc threshold for start bq direct charging */
#define START_DRIECT_CHARGE_FCC_MIN_THR			2000
#define MAX_THERMAL_LEVEL			13
#define MAX_THERMAL_LEVEL_FOR_DUAL_BQ			9

#define FCC_MAX_MA_FOR_MASTER_BQ			6000
#define IBUS_THRESHOLD_MA_FOR_DUAL_BQ			2100
#define IBUS_THR_MA_HYS_FOR_DUAL_BQ			200
#define IBUS_THR_TO_CLOSE_SLAVE_COUNT_MAX			40
#define BYPASS_IN		3000
#define BYPASS_OUT		3500
#define MAX_BYPASS_CURRENT_MA		3000
/* jeita related */
#define JEITA_WARM_THR			480
#define JEITA_COOL_NOT_ALLOW_CP_THR			50

#define PDO_MAX_NUM			7

/*
 * add hysteresis for warm threshold to avoid flash
 * charge and normal charge switch frequently at
 * the warm threshold
 */
#define JEITA_HYSTERESIS			20

/* product related */
#define LOW_POWER_PPS_CURR_THR			2000
#define XIAOMI_LOW_POWER_PPS_CURR_MAX			1500
#define PPS_VOL_MAX			20000
#define PPS_CURR_MIN			2000
#define PPS_VOL_HYS			1000

#define STEP_MV			20
#define TAPER_VOL_HYS			80
#define TAPER_WITH_IBUS_HYS			60
#define TAPER_IBUS_THR			450

/* BQ taper related */
#define BQ_TAPER_DECREASE_STEP_MA	100
#define HIGH_VBAT_MV			8700
#define CRITICAL_HIGH_VBAT_MV		8900

#define VBAT_HIGH_FOR_FC_HYS_MV		30
#define CAPACITY_TOO_HIGH_THR			95
#define CAPACITY_HIGH_THR			80

#define PM_STATE_LOG_MAX    32

#define MAX_VBUS_TUNE_COUNT		40
#define MAX_ADAPTER_ADJUST_COUNT	10
#define MAX_ENABLE_CP_COUNT		5
#define MAX_TAPER_COUNT			3

#define BYPASS_ENTRY_FCC	4000
#define BYPASS_EXIT_FCC		6000

#define SC8561_FORWARD_4_1_CHARGER_MODE     0
#define SC8561_FORWARD_2_1_CHARGER_MODE     1
#define SC8561_FORWARD_1_1_CHARGER_MODE     2
#define SC8561_FORWARD_1_1_CHARGER_MODE1    3
#define SC8561_REVERSE_1_4_CONVERTER_MODE   4
#define SC8561_REVERSE_1_2_CONVERTER_MODE   5
#define SC8561_REVERSE_1_1_CONVERTER_MODE   6
#define SC8561_REVERSE_1_1_CONVERTER_MODE1  7
#define	CP_FORWARD_4_TO_1             0
#define	CP_FORWARD_2_TO_1             1
#define	CP_FORWARD_1_TO_1             2
