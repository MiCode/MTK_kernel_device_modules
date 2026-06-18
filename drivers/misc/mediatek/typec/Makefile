# SPDX-License-Identifier: GPL-2.0


ifeq ($(CONFIG_AW_USBPD), y)
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/typec/tcpc_aw
obj-$(CONFIG_TCPC_CLASS) += tcpc_aw/
else
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/typec/tcpc

obj-$(CONFIG_TCPC_CLASS) += tcpc/
endif

obj-$(CONFIG_MTK_USB_TYPEC_MUX) += mux/

