# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2024 MediaTek Inc.
#

tmem_config = {
	"local_defines": [
#		"GCOV_PROFILE",
		"MTEE_DEVICES_SUPPORT",
#		"TCORE_MEMORY_LEAK_DETECTION_SUPPORT",
#		"TCORE_PROFILING_SUPPORT",
#		"TCORE_PROFILING_AUTO_DUMP",
		"TEE_DEVICES_SUPPORT",
	],
	"copts": [
		"-Werror",
	],
}

tmem_ffa_config = {
	"local_defines": [
#		"GCOV_PROFILE",
		"MTEE_DEVICES_SUPPORT",
#		"TCORE_MEMORY_LEAK_DETECTION_SUPPORT",
#		"TCORE_PROFILING_SUPPORT",
#		"TCORE_PROFILING_AUTO_DUMP",
		"TEE_DEVICES_SUPPORT",
	],
	"copts": [
		"-Werror",
	],
}

