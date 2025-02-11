# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/usb/typec
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/typec/tcpc/inc
CFLAGS_pi3dpx1205a.o += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/typec/mux

obj-$(CONFIG_TYPEC_MUX_PI3DPX1205A) += pi3dpx1205a.o