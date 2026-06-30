// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/firmware.h>

#include "mdla_cfg_data.h"
#include "mdla_rv.h"

#define FW_ELF_NAME   "mdla_firmware.elf"
#define BOOT_BIN_NAME "mdla_boot.bin"
#define MAIN_BIN_NAME "mdla_main.bin"

#define BOOT_DATA_MIN_SIZE 128
#define BOOT_DATA_MAX_SIZE (4 * 1024)
#define MAIN_DATA_MIN_SIZE (16 * 1024)
#define MAIN_DATA_MAX_SIZE (256 * 1024)

#define BOOT_DATA_SIZE 0x400
#define MAIN_DATA_SIZE 0xFF00

enum LOAD_DATA_METHOD {
	LOAD_NONE,
	LOAD_ELF_FILE,
	LOAD_HDR_FILE,
	LOAD_BIN_FILE,
	LOAD_PRE_SECURE_BOOT,
};

static int load_data_method;


static int mdla_data_alloc_mem(struct device *dev, struct mdla_rv_mem *mem, size_t len)
{
	mem->buf = dma_alloc_coherent(dev, len, &mem->da, GFP_KERNEL);
	if (mem->buf == NULL || mem->da == 0) {
		dev_info(dev, "%s() dma_alloc_coherent cfg_init_data fail\n\n", __func__);
		return -1;
	}
	mem->size = len;
	memset(mem->buf, 0, len);

	/* AISIM: It's not necessary to get iova */

	return 0;
}

static void mdla_data_free_mem(struct device *dev, struct mdla_rv_mem  *mem)
{
	if (mem->buf && mem->da)
		dma_free_coherent(dev, mem->size, mem->buf, mem->da);
	mem->da = 0;
}

static int mdla_plat_load_elf(struct device *dev, struct mdla_rv_mem  *boot_men, struct mdla_rv_mem  *main_men)
{
	return 0;
}

static int mdla_plat_load_img(struct device *dev, struct mdla_rv_mem  *boot_men, struct mdla_rv_mem  *main_men)
{
	int ret = 0;
	const struct firmware *bootcode;
	const struct firmware *maincode;

	dev_info(dev, "%s start\n", __func__);

	if (request_firmware(&bootcode, BOOT_BIN_NAME, dev) != 0) {
		dev_info(dev, "mdla_boot.bin not available\n");
		return -1;
	}

	if ((bootcode->size < BOOT_DATA_MIN_SIZE) || (bootcode->size > BOOT_DATA_MAX_SIZE)) {
		dev_info(dev, "mdla_boot.bin abnormal size\n");
		ret = -1;
		goto release_boot;
	}

	if (mdla_data_alloc_mem(dev, boot_men, bootcode->size) < 0) {
		ret = -1;
		goto release_boot;
	}

	memcpy(boot_men->buf, bootcode->data,  bootcode->size);

	if (request_firmware(&maincode, MAIN_BIN_NAME, dev) != 0) {
		dev_info(dev, "mdla_main.bin not available\n");
		ret = -1;
		goto release_boot;
	}

	if ((maincode->size < MAIN_DATA_MIN_SIZE) || (maincode->size > MAIN_DATA_MAX_SIZE)) {
		dev_info(dev, "mdla_main.bin abnormal size\n");
		ret = -1;
		goto release_main;
	}

	if (mdla_data_alloc_mem(dev, main_men, maincode->size) < 0) {
		ret = -1;
		goto release_main;
	}

	memcpy(main_men->buf, maincode->data,  maincode->size);

release_main:
	release_firmware(maincode);
release_boot:
	release_firmware(bootcode);

	return ret;
}

static int mdla_plat_load_hdr(struct device *dev, struct mdla_rv_mem *boot_men, struct mdla_rv_mem *main_men)
{
	u32 i;
	int ret = 0;

	if (mdla_data_alloc_mem(dev, boot_men, BOOT_DATA_SIZE) < 0)
		return -1;

	if (mdla_data_alloc_mem(dev, main_men, MAIN_DATA_SIZE) < 0) {
		mdla_data_free_mem(dev, boot_men);
		return -1;
	}

	memcpy(boot_men->buf, cfg_init, sizeof(cfg_init));

	for (i = 0; cfg_main_section[i].data != NULL; i++)
		memcpy(boot_men->buf + cfg_main_section[i].ofs,
				cfg_main_section[i].data, cfg_main_section[i].size);

	return ret;
}

static int mdla_plat_load_pre_secure_boot(struct device *dev, struct mdla_rv_mem *boot_men, struct mdla_rv_mem *main_men)
{
	if (mdla_data_alloc_mem(dev, boot_men, BOOT_DATA_SIZE) < 0)
		return -1;

	if (mdla_data_alloc_mem(dev, main_men, MAIN_DATA_SIZE) < 0) {
		mdla_data_free_mem(dev, boot_men);
		return -1;
	}

	return 0;
}

int mdla_plat_load_data(struct device *dev, struct mdla_rv_mem *boot_men, struct mdla_rv_mem *main_men)
{
	int ret;
	const char *method = NULL;

	ret = of_property_read_string(dev->of_node, "boot-method", &method);

	if (ret < 0)
		return -1;

	if (!strcmp(method, "bin"))
		load_data_method = LOAD_BIN_FILE;
	else if (!strcmp(method, "elf"))
		load_data_method = LOAD_ELF_FILE;
	else if (!strcmp(method, "array"))
		load_data_method = LOAD_HDR_FILE;
	else if (!strcmp(method, "PreSecureBoot"))
		load_data_method = LOAD_PRE_SECURE_BOOT;

	switch (load_data_method) {
	case LOAD_BIN_FILE:
		ret = mdla_plat_load_img(dev, boot_men, main_men);
		break;
	case LOAD_ELF_FILE:
		ret = mdla_plat_load_elf(dev, boot_men, main_men);
		break;
	case LOAD_HDR_FILE:
		ret = mdla_plat_load_hdr(dev, boot_men, main_men);
		break;
	case LOAD_PRE_SECURE_BOOT:
		ret = mdla_plat_load_pre_secure_boot(dev, boot_men, main_men);
		break;
	default:
		break;
	}

	if (ret == 0)
		dev_info(dev, "load mdla firmware by %s\n", method);

	return ret;
}

void mdla_plat_unload_data(struct device *dev, struct mdla_rv_mem *boot_men, struct mdla_rv_mem *main_men)
{
	switch (load_data_method) {
	case LOAD_BIN_FILE:
	case LOAD_ELF_FILE:
	case LOAD_HDR_FILE:
	case LOAD_PRE_SECURE_BOOT:
		mdla_data_free_mem(dev, boot_men);
		mdla_data_free_mem(dev, main_men);
		break;
	default:
		break;
	}
}

