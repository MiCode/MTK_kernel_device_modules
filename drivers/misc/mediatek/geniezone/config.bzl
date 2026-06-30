# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2024 MediaTek Inc.
#

gz_main_config = {
	"local_defines": [
		# disable tmem UTs due to including private headers
		"GZ_CHMEM_UT=0",
		# enable secure storage UTs.
		"GZ_SEC_STORAGE_UT=1",
	],
	"copts": [
	],
}

gz_tz_system_config = {
	"local_defines": [
		# disable TEEC-compliant APIs
		"CONFIG_TEE=0",
	],
	"copts": [
	],
}

