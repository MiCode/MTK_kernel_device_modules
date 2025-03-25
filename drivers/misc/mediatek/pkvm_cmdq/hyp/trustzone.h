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
// The requested operation is valid but is not supported in this Implementation.
#define TZ_RESULT_ERROR_NOT_SUPPORTED 0xFFFF000A
#define TZ_RESULT_ERROR_NO_DATA 0xFFFF000B // Expected data was missing.
#define TZ_RESULT_ERROR_OUT_OF_MEMORY 0xFFFF000C // System ran out of resources.
#define TZ_RESULT_ERROR_BUSY 0xFFFF000D // The system is busy working on something else.
#define TZ_RESULT_ERROR_COMMUNICATION 0xFFFF000E // Communication with a remote party failed.
#define TZ_RESULT_ERROR_SECURITY 0xFFFF000F // A security fault was detected.
#define TZ_RESULT_ERROR_SHORT_BUFFER 0xFFFF0010 // The supplied buffer is too short for the generated output.
#define TZ_RESULT_ERROR_INVALID_HANDLE 0xFFFF0011 // The handle is invalid.

typedef int TZ_RESULT;

/*  */
/* ERROR code number (ERRNO) */
/* note the error result returns negative value, i.e, -(ERRNO) */
/*  */
#define	CMDQ_ERR_NOMEM		(12)	/* out of memory */
#define	CMDQ_ERR_FAULT		(14)	/* bad address */

#define CMDQ_ERR_ADDR_CONVERT_HANDLE_2_PA (1000)
#define CMDQ_ERR_ADDR_CONVERT_ALLOC_MVA   (1100)
#define CMDQ_ERR_ADDR_CONVERT_ALLOC_MVA_N2S	(1101)
#define CMDQ_ERR_ADDR_CONVERT_FREE_MVA	  (1200)
#define CMDQ_ERR_PORT_CONFIG			  (1300)

/* param check */
#define CMDQ_ERR_UNKNOWN_ADDR_METADATA_TYPE (1400)
#define CMDQ_ERR_TOO_MANY_SEC_HANDLE (1401)
/* security check */
#define CMDQ_ERR_SECURITY_INVALID_INSTR	  (1500)
#define CMDQ_ERR_SECURITY_INVALID_SEC_HANDLE (1501)
#define CMDQ_ERR_SECURITY_INVALID_DAPC_FALG (1502)
#define CMDQ_ERR_INSERT_DAPC_INSTR_FAILED (1503)
#define CMDQ_ERR_INSERT_PORT_SECURITY_INSTR_FAILED (1504)
#define CMDQ_ERR_INVALID_SECURITY_THREAD (1505)
#define CMDQ_ERR_PATH_RESOURCE_NOT_READY (1506)
#define CMDQ_ERR_NULL_TASK (1507)
#define CMDQ_ERR_SECURITY_INVALID_SOP (1508)
#define CMDQ_ERR_SECURITY_INVALID_SEC_PORT_FALG (1511)

/* msee error */
#define CMDQ_ERR_OPEN_IOCTL_FAILED (1600)
/* secure access error */
#define CMDQ_ERR_MAP_ADDRESS_FAILED (2001)
#define CMDQ_ERR_UNMAP_ADDRESS_FAILED (2002)
#define CMDQ_ERR_RESUME_WORKER_FAILED (2003)
#define CMDQ_ERR_SUSPEND_WORKER_FAILED (2004)
/* HW error */
#define CMDQ_ERR_SUSPEND_HW_FAILED (4001)
#define CMDQ_ERR_RESET_HW_FAILED (4002)

#define CMDQ_TL_ERR_UNKNOWN_IWC_CMD	   (5000)

#define CMDQ_ERR_DR_IPC_EXECUTE_SESSION   (5001)
#define CMDQ_ERR_DR_IPC_CLOSE_SESSION	 (5002)
#define CMDQ_ERR_DR_EXEC_FAILED		   (5003)

#endif /* __REE_TRUSTZONE_H__ */
