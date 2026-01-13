/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016-2019 The Linux Foundation. All rights reserved.
 */
#ifndef __PMIC_VOTER_H
#define __PMIC_VOTER_H
#include <linux/mutex.h>
#define CHARGER_PLUG_VOTER	"CHARGER_PLUG_VOTER"
#define CHARGER_TYPE_VOTER	"CHARGER_TYPE_VOTER"
#define HW_LIMIT_VOTER		"HW_LIMIT_VOTER"
#define PDM_VOTER		"PDM_VOTER"
#define JEITA_CHARGE_VOTER	"JEITA_CHARGE_VOTER"
#define THERMAL_VOTER		"THERMAL_VOTER"
#define CHARGERIC_VOTER		"CHARGERIC_VOTER"
#define STEP_CHARGE_VOTER	"STEP_CHARGE_VOTER"
#define FFC_VOTER		"FFC_VOTER"
#define TYPEC_BURN_VOTER	"TYPEC_BURN_VOTER"
#define SIC_VOTER		"SIC_VOTER"
#define FG_I2C_VOTER	        "FG_I2C_VOTER"
#define MAIN_CON_ERR_VOTER	     "MAIN_CON_ERR_VOTER"
#define ICL_VOTER		"ICL_VOTER"
#define FV_DIFF_VOTER		"FV_DIFF_VOTER"
#define FV_DEC_VOTER		"FV_DEC_VOTER"
#define LPD_TRIG_VOTER	     "LPD_TRIG_VOTER"
#define WLS_CHG_VOTER		"WLS_CHG_VOTER"
#define WLS_DEBUG_CHG_VOTER		"WLS_DEBUG_CHG_VOTER"
#define WLS_THERMAL_VOTER		"WLS_THERMAL_VOTER"
#define SUPPLEMENT_CHG_VOTER	"SUPPLEMENT_CHG_VOTER"
#define FG_ERR_VOTER		"FG_ERR_VOTER"
#define ISC_ALERT_VOTER         "ISC_ALERT_VOTER"

#define ENDURANCE_VOTER		"ENDURANCE_VOTER"
#define SMOOTH_NEW_VOTER		"SMOOTH_NEW_VOTER"
#define NAVIGATION_VOTER	"NAVIGATION_VOTER"
#if IS_ENABLED(CONFIG_XM_BATTERY_HEALTH)
/* ovter of battery health fatures */
#define XM_BATT_HEALTH_VOTER           "XM_BATT_HEALTH_VOTER"
#define NIGHT_CHG_VOTER "NIGHT_CHG_VOTER"
#endif
#if IS_ENABLED(CONFIG_RUST_DETECTION)
#define LPD_DECTEED_VOTER	"LPD_DECTEED_VOTER"
#endif
#define CP_CHG_DONE		"CP_CHG_DONE"
#define SINK_VBUS_VOTER		"SINK_VBUS_VOTER"

struct votable;

enum votable_type {
	VOTE_MIN,
	VOTE_MAX,
	VOTE_SET_ANY,
	NUM_VOTABLE_TYPES,
};

extern bool is_client_vote_enabled(struct votable *votable, const char *client_str);
extern bool is_client_vote_enabled_locked(struct votable *votable,
							const char *client_str);
extern bool is_override_vote_enabled(struct votable *votable);
extern bool is_override_vote_enabled_locked(struct votable *votable);
extern int get_client_vote(struct votable *votable, const char *client_str);
extern int get_client_vote_locked(struct votable *votable, const char *client_str);
extern int get_effective_result(struct votable *votable);
extern int get_effective_result_locked(struct votable *votable);
extern int get_effective_result_exclude_client(struct votable *votable, const char *exclude_client_str);
extern const char *get_effective_client(struct votable *votable);
extern const char *get_effective_client_locked(struct votable *votable);
extern int vote(struct votable *votable, const char *client_str, bool state, int val);
extern int vote_override(struct votable *votable, const char *override_client,
		  bool state, int val);
extern int rerun_election(struct votable *votable);
extern struct votable *find_votable(const char *name);
extern struct votable *create_votable(const char *name,
				int votable_type,
				int (*callback)(struct votable *votable,
						void *data,
						int effective_result,
						const char *effective_client),
				void *data);
extern void destroy_votable(struct votable *votable);
extern void lock_votable(struct votable *votable);
extern void unlock_votable(struct votable *votable);
#endif /* __PMIC_VOTER_H */
