#
# Makefile for the fingerprint device.
#
xiaomi_spi_tee-objs := fp_driver.o fp_netlink.o fp_platform.o
obj-m := xiaomi_spi_tee.o

ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
subdir-ccflags-y += -I$(srctree)/drivers/tee/teei/510/tz_driver/include/
subdir-ccflags-y += -I$(srctree)/drivers/tee/teei/510/common/include/

CDEFINES += -DCONFIG_FP_MTK_PLATFORM
KBUILD_CPPFLAGS += $(CDEFINES)
