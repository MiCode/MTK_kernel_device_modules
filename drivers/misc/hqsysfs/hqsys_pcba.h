#ifndef __HQSYS_PCBA_
#define __HQSYS_PCBA_

typedef enum
{
	PCBA_INFO_UNKNOW = 0,

	PCBA_O6_P0_GLOBAL,
	PCBA_O6P_P0_GLOBAL,
	PCBA_O6_P0_EEA,
	PCBA_O7_P0_GL,
	PCBA_O7N_P0_GL,
	PCBA_O7L_P0_LA,
	PCBA_O7E_P0_EEA,

	PCBA_O6_P0_1_GLOBAL,
	PCBA_O6P_P0_1_GLOBAL,
	PCBA_O6_P0_1_EEA,
	PCBA_O7_P0_1_GL,
	PCBA_O7N_P0_1_GL,
	PCBA_O7L_P0_1_LA,
	PCBA_O7E_P0_1_EEA,

	PCBA_O6_P1_GLOBAL,
	PCBA_O6P_P1_GLOBAL,
	PCBA_O6_P1_EEA,
	PCBA_O7_P1_GL,
	PCBA_O7N_P1_GL,
	PCBA_O7L_P1_LA,
	PCBA_O7E_P1_EEA,

	PCBA_O6_P1_1_GLOBAL,
	PCBA_O6P_P1_1_GLOBAL,
	PCBA_O6_P1_1_EEA,
	PCBA_O7_P1_1_GL,
	PCBA_O7N_P1_1_GL,
	PCBA_O7L_P1_1_LA,
	PCBA_O7E_P1_1_EEA,

	PCBA_O6_P2_GLOBAL,
	PCBA_O6P_P2_GLOBAL,
	PCBA_O6_P2_EEA,
	PCBA_O7_P2_GL,
	PCBA_O7N_P2_GL,
	PCBA_O7L_P2_LA,
	PCBA_O7E_P2_EEA,

	PCBA_O6_MP_GLOBAL,
	PCBA_O6P_MP_GLOBAL,
	PCBA_O6_MP_EEA,
	PCBA_O7_MP_GL,
	PCBA_O7N_MP_GL,
	PCBA_O7L_MP_LA,
	PCBA_O7E_MP_EEA,

        PCBA_INFO_END,
} PCBA_INFO;

char pcba_str[] = "PCBA_UNKNOW";

struct pcba {
	PCBA_INFO pcba_info;
	char pcba_info_name[32];
} pcba_map[] = {

	{PCBA_O6_P0_GLOBAL,         "PCBA_O6_P0_GLOBAL"},
	{PCBA_O6P_P0_GLOBAL,        "PCBA_O6P_P0_GLOBAL"},
	{PCBA_O6_P0_EEA,            "PCBA_O6_P0_EEA"},
	{PCBA_O7_P0_GL,             "PCBA_O7_P0_GL"},
	{PCBA_O7N_P0_GL,            "PCBA_O7N_P0_GL"},
	{PCBA_O7L_P0_LA,            "PCBA_O7L_P0_LA"},
	{PCBA_O7E_P0_EEA,           "PCBA_O7E_P0_EEA"},

	{PCBA_O6_P0_1_GLOBAL,       "PCBA_O6_P0-1_GLOBAL"},
	{PCBA_O6P_P0_1_GLOBAL,      "PCBA_O6P_P0-1_GLOBAL"},
	{PCBA_O6_P0_1_EEA,          "PCBA_O6_P0-1_EEA"},
	{PCBA_O7_P0_1_GL,           "PCBA_O7_P0-1_GL"},
	{PCBA_O7N_P0_1_GL,          "PCBA_O7N_P0-1_GL"},
	{PCBA_O7L_P0_1_LA,          "PCBA_O7L_P0-1_LA"},
	{PCBA_O7E_P0_1_EEA,         "PCBA_O7E_P0-1_EEA"},

	{PCBA_O6_P1_GLOBAL,         "PCBA_O6_P1_GLOBAL"},
	{PCBA_O6P_P1_GLOBAL,        "PCBA_O6P_P1_GLOBAL"},
	{PCBA_O6_P1_EEA,            "PCBA_O6_P1_EEA"},
	{PCBA_O7_P1_GL,             "PCBA_O7_P1_GL"},
	{PCBA_O7N_P1_GL,            "PCBA_O7N_P1_GL"},
	{PCBA_O7L_P1_LA,            "PCBA_O7L_P1_LA"},
	{PCBA_O7E_P1_EEA,           "PCBA_O7E_P1_EEA"},

	{PCBA_O6_P1_1_GLOBAL,       "PCBA_O6_P1-1_GLOBAL"},
	{PCBA_O6P_P1_1_GLOBAL,      "PCBA_O6P_P1-1_GLOBAL"},
	{PCBA_O6_P1_1_EEA,          "PCBA_O6_P1-1_EEA"},
	{PCBA_O7_P1_1_GL,           "PCBA_O7_P1-1_GL"},
	{PCBA_O7N_P1_1_GL,          "PCBA_O7N_P1-1_GL"},
	{PCBA_O7L_P1_1_LA,          "PCBA_O7L_P1-1_LA"},
	{PCBA_O7E_P1_1_EEA,         "PCBA_O7E_P1-1_EEA"},

	{PCBA_O6_P2_GLOBAL,         "PCBA_O6_P2_GLOBAL"},
	{PCBA_O6P_P2_GLOBAL,        "PCBA_O6P_P2_GLOBAL"},
	{PCBA_O6_P2_EEA,            "PCBA_O6_P2_EEA"},
	{PCBA_O7_P2_GL,             "PCBA_O7_P2_GL"},
	{PCBA_O7N_P2_GL,            "PCBA_O7N_P2_GL"},
	{PCBA_O7L_P2_LA,            "PCBA_O7L_P2_LA"},
	{PCBA_O7E_P2_EEA,           "PCBA_O7E_P2_EEA"},

	{PCBA_O6_MP_GLOBAL,         "PCBA_O6_MP_GLOBAL"},
	{PCBA_O6P_MP_GLOBAL,        "PCBA_O6P_MP_GLOBAL"},
	{PCBA_O6_MP_EEA,            "PCBA_O6_MP_EEA"},
	{PCBA_O7_MP_GL,             "PCBA_O7_MP_GL"},
	{PCBA_O7N_MP_GL,            "PCBA_O7N_MP_GL"},
	{PCBA_O7L_MP_LA,            "PCBA_O7L_MP_LA"},
	{PCBA_O7E_MP_EEA,           "PCBA_O7E_MP_EEA"},
};

typedef enum
{
	STAGE_UNKNOW = 0,
	P0,
	P0_1,
	P1,
	P1_1,
	P2,
	MP,
} PROJECT_STAGE;

struct project_stage {
	int voltage_min;
	int voltage_max;
	PROJECT_STAGE project_stage;
	char hwc_level[20];
} stage_map[] = {
        {   0,  129,   P0,     "P0",  },
	{ 130,  225,   P0_1,   "P0.1",},
	{ 226,  315,   P1,     "P1",  },
	{ 316,  405,   P1_1,   "P1.1",},
	{ 406,  485,   P2,     "P2",  },
	{ 486,  565,   MP,     "MP",  },
};

struct PCBA_MSG {
	PCBA_INFO huaqin_pcba_config;
	PROJECT_STAGE pcba_stage;
	unsigned int pcba_config;
	unsigned int pcba_config_count;
	const char *rsc;
	const char *sku;
};

struct PCBA_MSG* get_pcba_msg(void);

#endif
