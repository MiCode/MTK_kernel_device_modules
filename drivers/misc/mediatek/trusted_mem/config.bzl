# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2024 MediaTek Inc.
#

ffa_config = {
	"local_defines": [
		# Override Kconfig defines to force enabling FFA support
		"CONFIG_ARM_FFA_SMCCC=1",
		"CONFIG_ARM_FFA_TRANSPORT=1",
		# MTK adpated implementations
		"MTK_ADAPTED=1",
		# MTK adapted workarounds, must use with MTK_ADAPTED
		"MTK_ADAPTED_WA=1",
	],
	"copts": [
		"-Werror",
	],
}

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

