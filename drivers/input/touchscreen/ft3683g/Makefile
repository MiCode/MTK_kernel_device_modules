# SPDX-License-Identifier: GPL-2.0
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-$(CONFIG_TOUCHSCREEN_MTK_TOUCH) += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/
obj-m := ft3683g.o

ft3683g-y := \
	focaltech_core.o \
	focaltech_ex_fun.o \
	focaltech_ex_mode.o \
	focaltech_gesture.o \
	focaltech_esdcheck.o \
	focaltech_point_report_check.o \
	focaltech_test/focaltech_test.o \
	focaltech_test/focaltech_test_ini.o \
	focaltech_test/supported_ic/focaltech_test_ft3683g.o \
	focaltech_flash/focaltech_upgrade_ft3683g.o \
	focaltech_flash.o \
	focaltech_spi.o