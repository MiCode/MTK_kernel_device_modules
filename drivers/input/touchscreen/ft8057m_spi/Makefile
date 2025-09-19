# Makefile for the focaltech touchscreen drivers.


ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

obj-m += ft8057m_spi.o
ft8057m_spi-y += focaltech_core.o \
                 focaltech_ex_fun.o \
                 focaltech_ex_mode.o \
                 focaltech_gesture.o \
                 focaltech_esdcheck.o \
                 focaltech_point_report_check.o \
                 focaltech_flash_spi.o \
                 focaltech_spi_v2.o \
                 focaltech_proximity.o \
                 focaltech_test/focaltech_test.o \
                 focaltech_test/focaltech_test_ini.o \
                 focaltech_test/supported_ic/focaltech_test_ft8057m.o \
                 focaltech_debug.o \
                 focaltech_hp.o