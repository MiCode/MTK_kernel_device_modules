# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.


subdir-ccflags-y += \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/ \
	-I$(DEVICE_MODULES_PATH)/drivers/i2c/busses/ \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/ccu/inc \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/imgsensor/inc \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cam_cal/inc \

obj-$(CONFIG_MTK_IMGSENSOR_ISP4) += isp4/
obj-$(CONFIG_MTK_IMGSENSOR) += isp6s/
