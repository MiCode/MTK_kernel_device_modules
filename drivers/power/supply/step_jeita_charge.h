#define JEITA_TUPLE_COUNT	7
#define STEP_JEITA_TUPLE_COUNT	3
#define THERMAL_LIMIT_COUNT	36
#define THERMAL_LIMIT_TUPLE	7
#define FCC_DESCENT_DELAY	1000
#define JEITA_FCC_DESCENT_STEP	1000
#define SW_CV_COUNT		3
#define TYPEC_BURN_TEMP		750
#define TYPEC_BURN_HYST		100
#define MAX_THERMAL_FCC		8000
#define MIN_THERMAL_FCC		200
#define ITERM_FCC_WARM		9000
#define ENABLE_FCC_ITERM_LEVEL	14
#define WARM_TEMP        	481
#define BATTERY_CYCLE_100_TO_300 100
#define BATTERY_CYCLE_300_TO_800 300
#define BATTERY_CYCLE_UP_800     800
#define ENABLE_JEITA_DESCENT	0

struct step_jeita_cfg0 {
	int low_threshold;
	int high_threshold;
	int value;
};

struct step_jeita_cfg1 {
	int low_threshold;
	int high_threshold;
        int extra_threshold;
	int low_value;
        int high_value;
};

void reset_step_jeita_charge(struct mtk_charger *info);
int step_jeita_init(struct mtk_charger *info, struct device *dev);
