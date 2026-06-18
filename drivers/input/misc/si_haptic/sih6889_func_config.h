/*
 *  Silicon Integrated Co., Ltd haptic sih6889 haptic config header file
 *
 *  Copyright (c) 2024 shanfa <shanfa.tang@si-in.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

#ifndef _SIH6889_FUNC_CONFIG_H_
#define _SIH6889_FUNC_CONFIG_H_
#include "sih6889_reg.h"
#include "haptic.h"
#include "haptic_mid.h"
#include "haptic_regmap.h"

typedef enum reg_operation {
	OPERATION_WRITE = 0,
	OPERATION_READ = 1,
	OPERATION_BIT = 2,
	OPERATION_END = 3,
} reg_op_e;

typedef struct haptic_reg_format {
	uint8_t reg_addr;
	uint8_t reg_value;
} haptic_bin_file_reg_format_t;

typedef struct haptic_reg_config {
	uint8_t reg_num;
	haptic_bin_file_reg_format_t reg_cont[HAPTIC_CONFIG_MAX_REG_NUM];
} haptic_reg_config_t;

typedef struct haptic_reg_configs {
	uint8_t reg_nums;
	haptic_bin_file_reg_format_t *reg_conts;
} haptic_reg_configs_t;

typedef struct haptic_reg_config_group {
	haptic_reg_configs_t common;
	haptic_reg_configs_t flexible;
	haptic_reg_configs_t lra;
} haptic_reg_config_group_t;

typedef struct reg_format {
	uint8_t addr;
	uint8_t val;
	uint8_t mask;	/* only useful for bit operation */
	reg_op_e operation;
} reg_format_t;

typedef struct lra_reg_config_s {
	char lra_name[SIH_LRA_NAME_LEN];
	haptic_reg_config_group_t *reg_config_list;
} lra_reg_config_s_t;

typedef struct lra_reg_func {
	char lra_name[SIH_LRA_NAME_LEN];
	reg_format_t *reg_cont_list;
	reg_format_t *reg_rl_list;
	reg_format_t *reg_vbat_list;
} lra_reg_func_t;

void sih6889_load_func_config(sih_haptic_t *sih_haptic, uint8_t func_type);
void sih6889_save_cont_config(sih_haptic_t *sih_haptic, uint32_t *reg_addr,
	uint32_t *reg_value, uint32_t len);

extern lra_reg_config_s_t sih6889_config_lists[];

#endif /* _SIH6889_FUNC_CONFIG_H_ */
