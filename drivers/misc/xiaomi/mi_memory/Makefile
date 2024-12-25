# SPDX-License-Identifier: GPL-2.0
#
# Makefile for mi memory info
#
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/ufs/xiaomi/core

ifeq ($(CONFIG_MI_MEMORY_SYSFS),)
obj-m	+= mi_memory.o
mi_memory-y += mi_memory_dummy.o
else
obj-$(CONFIG_MI_MEMORY_SYSFS) 	+= mi_memory.o
mi_memory-y += mi_memory_sysfs.o dram_info.o ufs_info.o
endif
