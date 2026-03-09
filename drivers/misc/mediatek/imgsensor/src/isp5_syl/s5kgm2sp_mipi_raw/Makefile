# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

IMGSENSOR_SUBDRV_OBJ := \
	$(subdrv-rpath)/s5kgm2spmipiraw_Sensor.o \
	$(subdrv-rpath)/s5kgm2sp_pdaf_cal.o \

ifeq ($(CONFIG_MTK_IMGSENSOR_ISP5_SYL), m)
$(MTK_PLATFORM_KO)-objs += $(IMGSENSOR_SUBDRV_OBJ)
else ifeq ($(CONFIG_MTK_IMGSENSOR_ISP5_SYL), y)
obj-$(CONFIG_MTK_IMGSENSOR_ISP5_SYL) += $(IMGSENSOR_SUBDRV_OBJ)
endif
