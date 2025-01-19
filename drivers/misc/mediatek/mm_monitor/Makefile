# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2024 MediaTek Inc.
#

ccflags-y += \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/iommu/ \
	-I$(DEVICE_MODULES_PATH)/drivers/iommu/arm/arm-smmu-v3

obj-$(CONFIG_MTK_MM_MONITOR) += mtk-mm-monitor-controller.o

#ccflags-y += -DMMMC_SUPPORT_FPGA

ifneq ($(wildcard $(src)/mm-monitor-platform-mt6991.c),)
obj-$(CONFIG_MTK_MM_MONITOR) += mm-monitor-platform-mt6991.o
endif
ifneq ($(wildcard $(src)/mm-monitor-platform-mt6993.c),)
obj-$(CONFIG_MTK_MM_MONITOR) += mm-monitor-platform-mt6993.o
endif
