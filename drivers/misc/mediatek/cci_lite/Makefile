# SPDX-License-Identifier: GPL-2.0

ccflags-y += \
	-I$(srctree)/include/ \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sched/

obj-$(CONFIG_MEDIATEK_CCI_DEBUG_LITE) += ccidvfs.o
ccidvfs-y += cci-dbg-lite.o
