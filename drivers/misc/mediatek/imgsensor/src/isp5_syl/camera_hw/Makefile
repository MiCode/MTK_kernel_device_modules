# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

ccflags-y += -I$(MTK_CUSTOM_PATH)/imgsensor/inc \

IMGSENSOR_SUBDRV_OBJ := \
	../common/v1_1/camera_hw/regulator/regulator.o \
	../common/v1_1/camera_hw/gpio/gpio.o \
	../common/v1_1/camera_hw/mclk/mclk.o

IMGSENSOR_SUBDRV_OBJ += $(subdrv-rpath)/imgsensor_cfg_table.o \

ifeq ($(CONFIG_MTK_IMGSENSOR_ISP5_SYL), m)
$(MTK_PLATFORM_KO)-objs += $(IMGSENSOR_SUBDRV_OBJ)
else ifeq ($(CONFIG_MTK_IMGSENSOR_ISP5_SYL), y)
obj-$(CONFIG_MTK_IMGSENSOR_ISP5_SYL) += $(IMGSENSOR_SUBDRV_OBJ)
endif
