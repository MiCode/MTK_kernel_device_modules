#
# Makefile for the Novatek NT36xxx touchscreen driver.
#

# Each configuration option enables a list of files.
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/

obj-m += nt36523d_spi.o
nt36523d_spi-y := \
				nt36xxx.o \
				nt36xxx_fw_update.o \
				nt36xxx_ext_proc.o \
				nt36xxx_mp_ctrlram.o
