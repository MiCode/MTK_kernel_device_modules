#
# Copyright (C) 2020 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

ccflags-y += -DMET_USER_EVENT_SUPPORT
ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/inc \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/ppm_v3/inc/	\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cpufreq_v2/inc/	\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/	\
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/common/include

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6768)),)
ccflags-y += -I$(CPUFREQ_ROOT_DIR)/src/plat_k68
include $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cpufreq_v2/src/plat_k68/Makefile
ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/inc \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/ppm_v3/inc/	\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cpufreq_v2/inc/	\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/mt6768/include	\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcupm/mt6768/
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_6893)),)
ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/src/plat_k6893/	\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcupm/include
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6765)),)
ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/inc \
	-I$(CPUFREQ_ROOT_DIR)/src/plat_k6765 \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
endif

ifneq ($(strip $(CONFIG_MEDIATEK_CPU_DVFS_MT6833)),)
ccflags-y += -I$(CPUFREQ_ROOT_DIR)/src/common_v3
include $(srctree)/drivers/misc/mediatek/cpufreq_v2/src/common_v3/Makefile

ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/inc \
	-I$(CPUFREQ_ROOT_DIR)/src/common_v3 \
	-I$(srctree)/drivers/misc/mediatek/include \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat	\
	-I$(srctree)/drivers/misc/mediatek/mt6833 \
	-I$(srctree)/drivers/misc/mediatek/include	\
	-I$(srctree)/drivers/misc/mediatek/mcupm/include	\
	-I$(srctree)/drivers/misc/mediatek/mcupm/mt6833/
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6739)),)
ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/inc \
	-I$(srctree)/drivers/misc/mediatek/include \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/common/include	\
	-I$(srctree)/drivers/misc/mediatek/cpufreq_v2/src/common/platform   \
	-I$(srctree)/drivers/misc/mediatek/mcupm/common/

endif

ifneq ($(strip $(CONFIG_MEDIATEK_CPU_DVFS_MT6761)),)
ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/inc \
	-I$(srctree)/drivers/misc/mediatek/include \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/common/include	\
	-I$(srctree)/drivers/misc/mediatek/cpufreq_v2/src/platform   \
	-I$(srctree)/drivers/misc/mediatek/mcupm/common/

endif
#sspm
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V1)),)
	ccflags-y += -DUSE_SSPM_VER_V1
	ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v1/
endif
#sspm for k6765
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V0)),)
ccflags-y += -DUSE_SSPM_VER_V1
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v0/
endif
#sspm for common_v3-6833
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V2)),)
#ccflags-y += -DUSE_SSPM_VER_V1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2/
endif

obj-$(CONFIG_MEDIATEK_CPU_DVFS) += CPU_DVFS.o
ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6739)),)
CPU_DVFS-y := common/mtk_cpufreq_main.o
CPU_DVFS-y += common/mtk_cpufreq_interface.o
CPU_DVFS-y += common/mtk_cpufreq_api.o
CPU_DVFS-y += common/mtk_cpufreq_debug.o
CPU_DVFS-y += common/platform/mtk_cpufreq_platform.o
else
CPU_DVFS-y := mtk_cpufreq_main.o
CPU_DVFS-y += mtk_cpufreq_interface.o
CPU_DVFS-y += mtk_cpufreq_api.o
CPU_DVFS-y += mtk_cpufreq_debug.o
CPU_DVFS-y += mtk_cpufreq_hybrid.o
endif
ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6768)),)
CPU_DVFS-y += plat_k68/mtk_cpufreq_platform.o
endif
ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6765)),)
CPU_DVFS-y += plat_k6765/mtk_cpufreq_platform.o
endif
ifneq ($(strip $(CONFIG_MEDIATEK_CPU_DVFS_MT6833)),)
CPU_DVFS-y += common_v3/mtk_cpufreq_platform.o
endif
ifneq ($(strip $(CONFIG_MEDIATEK_CPU_DVFS_MT6761)),)
CPU_DVFS-y += platform/mtk_cpufreq_platform.o
endif
ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_6893)),)
CPU_DVFS-y += plat_k6893/mtk_cpufreq_platform.o
endif
