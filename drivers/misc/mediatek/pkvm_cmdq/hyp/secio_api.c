// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <pkvm_mgmt/pkvm_mgmt.h>

#include "sys.h"
#include "trustzone.h"

#define SIO_READ (0)
#define SIO_WRITE (1)

#define IO_OFFSET_LIMIT (0x1000)
#define IO_ACCESS_SIZE (4)

static inline bool is_valid_offset(uint32_t offset)
{
	/* Currently, the maximum IO_SIZE is 124K in TF-A */
	if (offset >= (IO_OFFSET_LIMIT * 0x1F))
		return false;

	if ((offset % IO_ACCESS_SIZE) != 0)
		return false;

	return true;
}

static inline bool is_valid_secio_type(uint32_t type)
{
	if ((type > SECIO_INVALID) && (type < SECIO_MAX))
		return true;
	return false;
}

#define IS_NULL(p) (p == NULL)
static TZ_RESULT SECIO_ACCESS(uint32_t io_type, uint32_t dir, uint32_t offset,
	uint32_t write_val, uint32_t *read_val)
{
	struct arm_smccc_res res;

	if (!is_valid_offset(offset))
		return TZ_RESULT_ERROR_BAD_FORMAT;

	if (!is_valid_secio_type(io_type))
		return TZ_RESULT_ERROR_ACCESS_DENIED;

	if ((dir == SIO_READ) && IS_NULL(read_val))
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	if (dir == SIO_READ)
		arm_smccc_1_1_smc(MTK_SIP_HYP_SECIO_READ,
			io_type, offset, 0, 0, 0, 0, 0, &res);
	else
		arm_smccc_1_1_smc(MTK_SIP_HYP_SECIO_WRITE,
			io_type, offset, write_val, 0, 0, 0, 0, &res);

	if (dir == SIO_READ && !IS_NULL(read_val))
		*read_val = res.a1;

	return (TZ_RESULT)res.a0;
}

TZ_RESULT SECIO_WRITE(uint32_t io_type, uint32_t reg_offset, uint32_t write_val)
{
	return SECIO_ACCESS(io_type, SIO_WRITE, reg_offset, write_val, NULL);
}

TZ_RESULT SECIO_READ(uint32_t io_type, uint32_t reg_offset, uint32_t *read_val)
{
	return SECIO_ACCESS(io_type, SIO_READ, reg_offset, 0, read_val);
}
