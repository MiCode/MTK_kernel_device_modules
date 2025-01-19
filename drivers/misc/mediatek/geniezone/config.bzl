# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2024 MediaTek Inc.
#

gz_main_config = {
	"local_defines": [
		"GZ_SEC_STORAGE_UT=1",
	],
	"copts": [
	],
}

gz_tz_system_config = {
	"local_defines": [
		"CONFIG_TEE=0", # disable TEEC-compliant APIs
	],
	"copts": [
	],
}

