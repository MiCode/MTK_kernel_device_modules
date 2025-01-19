# SPDX-License-Identifier: GPL-2.0
#
# USB peripheral controller drivers
#


ifneq ($(CONFIG_MTK_USB_OFFLOAD), )
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/usb/usb_offload
endif

obj-$(CONFIG_MTK_USB_SRAM)	+= usb_sram.o
ifneq ($(CONFIG_MTK_USB_SRAM), )
usb_sram-y := usb_sram_manager.o
endif