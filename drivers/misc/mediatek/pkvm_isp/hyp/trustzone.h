/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __REE_TRUSTZONE_H__
#define __REE_TRUSTZONE_H__

#include <linux/types.h>

#define TZ_RESULT_SUCCESS 0x00000000 // The operation was successful.
#define TZ_RESULT_ERROR_GENERIC 0xFFFF0000 // Non-specific cause.
#define TZ_RESULT_ERROR_ACCESS_DENIED 0xFFFF0001 // Access privileges are not sufficient.
#define TZ_RESULT_ERROR_CANCEL 0xFFFF0002 // The operation was cancelled.
#define TZ_RESULT_ERROR_ACCESS_CONFLICT 0xFFFF0003 // Concurrent accesses caused conflict.
#define TZ_RESULT_ERROR_EXCESS_DATA 0xFFFF0004 // Too much data for the requested operation was passed.
#define TZ_RESULT_ERROR_BAD_FORMAT 0xFFFF0005 // Input data was of invalid format.
#define TZ_RESULT_ERROR_BAD_PARAMETERS 0xFFFF0006 // Input parameters were invalid.
#define TZ_RESULT_ERROR_BAD_STATE 0xFFFF0007 // Operation is not valid in the current state.
#define TZ_RESULT_ERROR_ITEM_NOT_FOUND 0xFFFF0008 // The requested data item is not found.
#define TZ_RESULT_ERROR_NOT_IMPLEMENTED 0xFFFF0009 // The requested operation should exist but is not yet implemented.
#define TZ_RESULT_ERROR_NOT_SUPPORTED 0xFFFF000A // The requested operation is not supported in this Implementation.
#define TZ_RESULT_ERROR_NO_DATA 0xFFFF000B // Expected data was missing.
#define TZ_RESULT_ERROR_OUT_OF_MEMORY 0xFFFF000C // System ran out of resources.
#define TZ_RESULT_ERROR_BUSY 0xFFFF000D // The system is busy working on something else.
#define TZ_RESULT_ERROR_COMMUNICATION 0xFFFF000E // Communication with a remote party failed.
#define TZ_RESULT_ERROR_SECURITY 0xFFFF000F // A security fault was detected.
#define TZ_RESULT_ERROR_SHORT_BUFFER 0xFFFF0010 // The supplied buffer is too short for the generated output.
#define TZ_RESULT_ERROR_INVALID_HANDLE 0xFFFF0011 // The handle is invalid.

typedef int TZ_RESULT;

#endif /* __REE_TRUSTZONE_H__ */
