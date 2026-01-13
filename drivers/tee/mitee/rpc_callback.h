/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2019-2021, Linaro Limited
 * Copyright (C) 2024 XiaoMi, Inc.
 */
#ifndef __RPC_CALLBACK_H
#define __RPC_CALLBACK_H

#define REE_CALLBACK_MODULE_TEE_FRAMEWORK 0 //for TEE OS
#define REE_CALLBACK_MODULE_TEE_TUI       1 //for TUI

#define OPTEE_REE_CALLBACK_ALLOCATE_SECMEM 0
#define OPTEE_REE_CALLBACK_FREE_SECMEM     1
#define OPTEE_REE_CALLBACK_NOTIFY          2
#define OPTEE_REE_CALLBACK_CALL            3
#define OPTEE_REE_CALLBACK_ALLOCATE_NONSECMEM 4
#define OPTEE_REE_CALLBACK_FREE_NONSECMEM     5

void mitee_rpc_register_callback(uint32_t module_id, uint32_t cmd, uint32_t (*callback)(struct optee_msg_param_value *value,
										   void *buf, uint32_t size_in, uint32_t *size_out));

#endif
