# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.


subdir-ccflags-y += \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/ \
	-I$(DEVICE_MODULES_PATH)/drivers/i2c/busses/ \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/ccu/inc \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/imgsensor/inc \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cam_cal/inc \

obj-$(CONFIG_MTK_IMGSENSOR_ISP4) += isp4/
obj-$(CONFIG_MTK_IMGSENSOR_ISP4_C) += isp4_c/
obj-$(CONFIG_MTK_IMGSENSOR) += isp6s/
obj-$(CONFIG_MTK_IMGSENSOR_ISP6S_MON) += isp6s_mon/

ifeq ($(CONFIG_MTK_IMGSENSOR_ISP4), $(filter $(CONFIG_MTK_IMGSENSOR_ISP4), m y))
$(info imgsensor CONFIG_MTK_IMGSENSOR_ISP4 = $(CONFIG_MTK_IMGSENSOR_ISP4))
else ifeq ($(CONFIG_MTK_IMGSENSOR_ISP4_C), $(filter $(CONFIG_MTK_IMGSENSOR_ISP4_C), m y))
$(info imgsensor CONFIG_MTK_IMGSENSOR_ISP4_C = $(CONFIG_MTK_IMGSENSOR_ISP4_C))
else ifeq ($(CONFIG_MTK_IMGSENSOR), $(filter $(CONFIG_MTK_IMGSENSOR), m y))
$(info imgsensor CONFIG_MTK_IMGSENSOR = $(CONFIG_MTK_IMGSENSOR))
else ifeq ($(CONFIG_MTK_IMGSENSOR_ISP6S_MON), $(filter $(CONFIG_MTK_IMGSENSOR_ISP6S_MON), m y))
$(info imgsensor CONFIG_MTK_IMGSENSOR_ISP6S_MON = $(CONFIG_MTK_IMGSENSOR_ISP6S_MON))
endif