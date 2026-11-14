#ifndef APUSYS_CE_DECL_H
#define APUSYS_CE_DECL_H

#define APU_CE_MAGIC 0x00555041
#define APU_CE_OFFSET 0x00003890
#define APU_CE_IMAGE_SIZE 0x000027E0
#define APU_CE_STACK_SIZE 0x00000034
#define APU_CE_CONFIG_COUNT 5

struct apu_ce_config_t {
    unsigned int config_id;     // hw config id
    unsigned int value;         // hw entry value (word)
};

extern struct apu_ce_config_t apu_ce_configs[APU_CE_CONFIG_COUNT];
enum {
    FAST_DVFS = 0x00004D78,
    OUTERLOOP = 0x000051D8,
    PSC = 0x00005650,
    APU_SLEEP = 0x00005688,
    APU_WAKEUP = 0x00005994,
    DATA_NOC_MAIN = 0x00005F74,
    SMMU_RESTORE_MAIN = 0x00005FD8
};
enum {
    FAST_DVFS_LATENCY = 0x00003890,
    APU_SLEEP_VER = 0x00003898,
    APU_WAKEUP_VER = 0x0000389C,
    APU_BRISKET_INIT_DONE = 0x000038A0,
    DNOC_EMI_BW_DATA = 0x000038A4,
    SMMU_BKRS_WA_VAL = 0x000038A8,
    SMMU_BKRS_REG_VAL_LIST = 0x000038AC
};
enum {
    ID_PSC = 2,
    ID_APU_WAKEUP = 16,
    ID_APU_SLEEP = 17,
    ID_DATA_NOC_MAIN = 18,
    ID_OUTERLOOP = 20,
    ID_SMMU_RESTORE_MAIN = 26,
    ID_FAST_DVFS = 31
};
#endif

