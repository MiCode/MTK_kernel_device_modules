#ifndef __HQSYS_PCBA_
#define __HQSYS_PCBA_

typedef enum
{
	PCBA_INFO_UNKNOW = 0,

	PCBA_P16_P0_1_CN,
	PCBA_P16_P0_1_GL,
	PCBA_P16_P0_1_JP,
	PCBA_P16_P0_1_SA,
	PCBA_P16_P0_1_IN,
	PCBA_P16P_P0_1_GL,
	PCBA_P16P_P0_1_IN,

	PCBA_P16_P1_CN,
	PCBA_P16_P1_GL,
	PCBA_P16_P1_JP,
	PCBA_P16_P1_SA,
	PCBA_P16_P1_IN,
	PCBA_P16P_P1_GL,
	PCBA_P16P_P1_IN,

	PCBA_P16_P1_1_CN,
	PCBA_P16_P1_1_GL,
	PCBA_P16_P1_1_JP,
	PCBA_P16_P1_1_SA,
	PCBA_P16_P1_1_IN,
	PCBA_P16P_P1_1_GL,
	PCBA_P16P_P1_1_IN,

	PCBA_P16_P2_CN,
	PCBA_P16_P2_GL,
	PCBA_P16_P2_JP,
	PCBA_P16_P2_SA,
	PCBA_P16_P2_IN,
	PCBA_P16P_P2_GL,
	PCBA_P16P_P2_IN,

	PCBA_P16_MP_CN,
	PCBA_P16_MP_GL,
	PCBA_P16_MP_JP,
	PCBA_P16_MP_SA,
	PCBA_P16_MP_IN,
	PCBA_P16P_MP_GL,
	PCBA_P16P_MP_IN,

	PCBA_INFO_END,
} PCBA_INFO;

typedef enum
{
	STAGE_UNKNOW = 0,
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
	{ 130,  225,   P0_1,   "P0.1",},
	{ 226,  315,   P1,     "P1",  },
	{ 316,  405,   P1_1,   "P1.1",},
	{ 406,  485,   P2,     "P2",  },
	{ 486,  565,   MP,     "MP",  },
};

struct pcba {
	PCBA_INFO pcba_info;
	char pcba_info_name[32];
} pcba_map[] = {

	{PCBA_P16_P0_1_CN,          "PCBA_P16_P0-1_CN"},
	{PCBA_P16_P0_1_GL,          "PCBA_P16_P0-1_GL"},
	{PCBA_P16_P0_1_JP,          "PCBA_P16_P0-1_JP"},
	{PCBA_P16_P0_1_SA,          "PCBA_P16_P0-1_SA"},
	{PCBA_P16_P0_1_IN,          "PCBA_P16_P0-1_IN"},
	{PCBA_P16P_P0_1_GL,         "PCBA_P16P_P0-1_GL"},
	{PCBA_P16P_P0_1_IN,         "PCBA_P16P_P0-1_IN"},

	{PCBA_P16_P1_CN,            "PCBA_P16_P1_CN"},
	{PCBA_P16_P1_GL,            "PCBA_P16_P1_GL"},
	{PCBA_P16_P1_JP,            "PCBA_P16_P1_JP"},
	{PCBA_P16_P1_SA,            "PCBA_P16_P1_SA"},
	{PCBA_P16_P1_IN,            "PCBA_P16_P1_IN"},
	{PCBA_P16P_P1_GL,           "PCBA_P16P_P1_GL"},
	{PCBA_P16P_P1_IN,           "PCBA_P16P_P1_IN"},

	{PCBA_P16_P1_1_CN,          "PCBA_P16_P1-1_CN"},
	{PCBA_P16_P1_1_GL,          "PCBA_P16_P1-1_GL"},
	{PCBA_P16_P1_1_JP,          "PCBA_P16_P1-1_JP"},
	{PCBA_P16_P1_1_SA,          "PCBA_P16_P1-1_SA"},
	{PCBA_P16_P1_1_IN,          "PCBA_P16_P1-1_IN"},
	{PCBA_P16P_P1_1_GL,         "PCBA_P16P_P1-1_GL"},
	{PCBA_P16P_P1_1_IN,         "PCBA_P16P_P1-1_IN"},

	{PCBA_P16_P2_CN,            "PCBA_P16_P2_CN"},
	{PCBA_P16_P2_GL,            "PCBA_P16_P2_GL"},
	{PCBA_P16_P2_JP,            "PCBA_P16_P2_JP"},
	{PCBA_P16_P2_SA,            "PCBA_P16_P2_SA"},
	{PCBA_P16_P2_IN,            "PCBA_P16_P2_IN"},
	{PCBA_P16P_P2_GL,           "PCBA_P16P_P2_GL"},
	{PCBA_P16P_P2_IN,           "PCBA_P16P_P2_IN"},

	{PCBA_P16_MP_CN,            "PCBA_P16_MP_CN"},
	{PCBA_P16_MP_GL,            "PCBA_P16_MP_GL"},
	{PCBA_P16_MP_JP,            "PCBA_P16_MP_JP"},
	{PCBA_P16_MP_SA,            "PCBA_P16_MP_SA"},
	{PCBA_P16_MP_IN,            "PCBA_P16_MP_IN"},
	{PCBA_P16P_MP_GL,           "PCBA_P16P_MP_GL"},
	{PCBA_P16P_MP_IN,           "PCBA_P16P_MP_IN"},
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
