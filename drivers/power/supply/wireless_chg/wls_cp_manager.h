#ifndef __WLS_CP_HEADER__
#define __WLS_CP_HEADER__
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>

#define WLS_SM_DELAY_300MS                      300
#define WLS_SM_DELAY_500MS                      500
#define WLS_SM_DELAY_1000MS                     1000
#define WLS_SM_DELAY_1500MS                     1500
#define LARGE_IBAT_DIFF                         3000
#define MEDIUM_IBAT_DIFF                        1800
#define SMALL_IBAT_DIFF                         800
#define LARGE_VBAT_DIFF                         400
#define MEDIUM_VBAT_DIFF                        80
#define LARGE_IBUS_DIFF                         1200
#define MEDIUM_IBUS_DIFF                        400
#define SMALL_IBUS_DIFF                         100
#define LARGE_VBUS_DIFF                         800
#define MEDIUM_VBUS_DIFF                        400
#define MAX_CABLE_RESISTANCE                    350
#define LARGE_STEP                              4
#define MEDIUM_STEP                             2
#define SMALL_STEP                              1
#define MIN_JEITA_CHG_INDEX                     2
#define MIN_JEITA_CHG_INDEX_WLS                 1
#define MAX_JEITA_CHG_INDEX                     4
#define MIN_THERMAL_LIMIT_FCC                   2000
#define MIN_2_1_CHARGE_CURRENT                  1200
#define MIN_4_1_CHARGE_CURRENT                  2000
#define STEP_MV                                 25
#define MAX_VBUS_TUNE_COUNT                     40
#define MAX_ENABLE_CP_COUNT                     10
#define MAX_TAPER_COUNT                         5
#define EXIT_CP_SM_VOLTAGE                      11000
#define PMIC_PARALLEL_ICL                       100
#define MAX_CURRENT                             9000
#define MAX_CURRENT_REDUCE                      8100

#define SC8561_FORWARD_4_1_CHARGER_MODE         0
#define SC8561_FORWARD_2_1_CHARGER_MODE         1
#define SC8561_FORWARD_1_1_CHARGER_MODE         2
#define SC8561_FORWARD_1_1_CHARGER_MODE1        3
#define SC8561_REVERSE_1_4_CONVERTER_MODE       4
#define SC8561_REVERSE_1_2_CONVERTER_MODE       5
#define SC8561_REVERSE_1_1_CONVERTER_MODE       6
#define SC8561_REVERSE_1_1_CONVERTER_MODE1      7
#define	CP_FORWARD_4_TO_1                       0
#define	CP_FORWARD_2_TO_1                       1
#define	CP_FORWARD_1_TO_1                       2

#endif