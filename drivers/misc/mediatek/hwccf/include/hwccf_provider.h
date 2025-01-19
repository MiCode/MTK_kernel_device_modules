// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _HWCCF_PROVIDER_H_
#define _HWCCF_PROVIDER_H_

#include <clk-mtk.h>
#include <hwccf_port.h> /* Kernel only */
#include <hwccf_provider_data.h> /* Kernel only */

/* HWCCF Clock OPs */
enum HWCCF_VOTER_CTRL_ERRNO {
	HWV_CTRL_OK         = 0,
	/* Avoid conflicting with existing errno. */
	HWV_PREPARE_TIMEOUT = 1000,
	HWV_VOTE_TIMEOUT    = 1001,
	HWV_SET_TIMEOUT     = 1002,
	HWV_SET_CHK_TIMEOUT = 1003,
	HWV_REGMAP_NULL     = 1004,
	HWV_READ_FAIL       = 1005,
	HWV_TEST_EARLY_RET  = 1006,
	HWV_WRONG_ID        = 1007,
	HWV_EINVAL          = 2000,
};

struct hwccf_ops {
	int (*hwccf_voter_ctrl)(enum HWCCF_TYPE hwccf_type, uint32_t resource_id,
						enum HWCCF_OP hwccf_op, uint32_t vote_bit);
	int (*hwccf_multi_voter_ctrl)(enum HWCCF_TYPE hwccf_type, uint32_t resource_id,
						enum HWCCF_OP hwccf_op, uint32_t vote_val);
	int (*raw_hwccf_voter_ctrl)(struct cb_params *);
	int (*hwccf_is_enabled)(enum HWCCF_TYPE hwccf_type, uint32_t resource_id,
						enum HWCCF_OP hwccf_op, uint32_t vote_val);
	int (*raw_hwccf_is_enabled)(struct cb_params *);
	int (*hwccf_irq_voter_ctrl)(enum HWCCF_TYPE hwccf_type, uint32_t resource_id,
						enum HWCCF_OP hwccf_op, uint32_t vote_val);
	void (*hwccf_freeze)(int is_MASK_XPC, struct regmap *regmap);
	void (*hwccf_unfreeze)(int is_MASK_XPC, struct regmap *regmap);
};

struct hwccf_match_data {
	int required_regmaps;
	struct hwccf_ops *ops;
};

// General HWCCF Voter control functions
int hwccf_voter_ctrl(enum HWCCF_TYPE hwccf_type, uint32_t resource_id,
						enum HWCCF_OP hwccf_op, uint32_t vote_bit);

int hwccf_multi_voter_ctrl(enum HWCCF_TYPE hwccf_type, uint32_t resource_id,
						enum HWCCF_OP hwccf_op, uint32_t vote_val);

// General IRQ HWCCF Voter control functions
int hwccf_irq_voter_ctrl(enum HWCCF_TYPE hwccf_type, uint32_t resource_id,
						enum HWCCF_OP hwccf_op, uint32_t vote_bit);

// Check resource or auto link resource is enabled
int hwccf_is_enabled(enum HWCCF_TYPE hwccf_type, uint32_t resource_id,
						enum HWCCF_OP hwccf_op, uint32_t vote_bit);

#endif /* _HWCCF_PROVIDER_H_ */
