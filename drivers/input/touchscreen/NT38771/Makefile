#
# Makefile for the Novatek NT387xx touchscreen driver.
#

# Each configuration option enables a list of files.
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
obj-$(CONFIG_TOUCHSCREEN_NT38771) += nt38771.o
nt38771-y := \
			nt387xx.o \
			nt387xx_fw_update.o\
			nt387xx_ext_proc.o\
			nt387xx_selftest.o
