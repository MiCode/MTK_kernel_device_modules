#
# Makefile for nfc devices
#
$(info   CONFIG_NXP_NFC_I2C = $(CONFIG_NFC_NXP_I2C))
ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/pmic/include
obj-$(CONFIG_NXP_NFC_I2C)		+= nfc_i2c.o
nfc_i2c-objs				:= common.o common_ese.o i2c_drv.o
ccflags-y                      += -DDEBUG