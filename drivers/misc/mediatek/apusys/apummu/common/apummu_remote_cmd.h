/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef __APUSYS_APUMMU_REMOTE_CMD_H__
#define __APUSYS_APUMMU_REMOTE_CMD_H__

#define AMMU_RPMSG_RAED(var, data, size, idx) do {\
			memcpy(var, data + idx, size); \
			idx = idx + size/sizeof(uint32_t); \
	} while (0)

#define AMMU_RPMSG_write(var, data, size, idx) do {\
			memcpy(data + idx, var, size); \
			idx = idx + size/sizeof(uint32_t); \
	} while (0)


extern struct apummu_msg *g_reply;
extern struct apummu_msg_mgr *g_ammu_msg;

int apummu_remote_check_reply(void *reply);


int apummu_remote_set_op(void *drvinfo, uint32_t *argv, uint32_t argc);

int apummu_remote_handshake(void *drvinfo, void *remote);
int apummu_remote_set_hw_default_iova(void *drvinfo, uint32_t ctx, uint64_t iova);
int apummu_remote_set_hw_default_iova_one_shot(void *drvinfo);

int apummu_remote_alloc_mem(void *drvinfo,
		uint32_t type, uint64_t input_addr, uint32_t size,
		uint64_t *addr, uint32_t *sid);
int apummu_remote_free_mem(void *drvinfo, uint32_t sid, uint32_t *type,
		uint64_t *addr, uint32_t *size);
int apummu_remote_map_mem(void *drvinfo,
		uint64_t session, uint32_t sid, uint64_t *addr);
int apummu_remote_unmap_mem(void *drvinfo,
		uint64_t session, uint32_t sid);
int apummu_remote_import_mem(void *drvinfo, uint64_t session, uint32_t sid);
int apummu_remote_unimport_mem(void *drvinfo, uint64_t session, uint32_t sid);

#endif
