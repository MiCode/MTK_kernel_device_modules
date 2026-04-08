# SPDX-License-Identifier: GPL-2.0 */
#
# Copyright (c) 2022 MediaTek Inc.
#
ccflags-y += \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cm_mgr_legacy_v1/ \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cm_mgr_legacy_v1/cm_mgr_plat_k8788/ \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/spm_v4/ \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/spm_v4/ \
    -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/vcorefs_v3/ \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/ \
    -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/helio-dvfsrc/ \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM) \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/ \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/tinysys_scmi/ \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/qos \
	-I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/ \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cpufreq_v2/inc/ \
	-I$(srctree)/include \

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_V2),y)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v2
endif

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_V1),y)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v1
endif

ifneq ($(strip $(CONFIG_MEDIATEK_CPU_DVFS_MT6771)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cpufreq_v2/src/plat_k6771
endif

# main
obj-${CONFIG_MTK_CM_MGR_LEGACY_MT8788} += mtk_cm_mgr.o
mtk_cm_mgr-y = mtk_cm_mgr_mt8788.o
mtk_cm_mgr-y += mtk_cm_mgr_platform_mt8788.o


