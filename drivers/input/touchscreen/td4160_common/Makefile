#
# omnivision_td4160.ko
#
# Kbuild: for kernel building external module
#
# Note:
# - Please use these predefined name for keeping code:
#   KO_MODULE_NAME KO_MODULE_PATH KO_MODULE_SRC
#

KO_MODULE_NAME := omnivision_td4160
$(warning =======================TRAN TD4160 VENDOR START===============================)
#
# source
#
$(KO_MODULE_NAME)-y := omnivision_tcm_zeroflash.o \
		omnivision_tcm_reflash.o \
		omnivision_tcm_device.o \
		omnivision_tcm_testing.o \
		omnivision_tcm_csv_parser.o \
		omnivision_tcm_recovery.o \
		omnivision_tcm_diagnostics.o \
		omnivision_tcm_core.o \
		omnivision_tcm_touch.o \
		omnivision_tcm_spi.o
#
# Build Options
#
subdir-ccflags-y += -Wno-error

ccflags-y += -I$(DEVICE_MODULES_PATH)/include/linux
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/td4160_common/
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/

#
# Final Objects
#
obj-m := $(KO_MODULE_NAME).o

