# SPDX-License-Identifier: GPL-2.0

ccflags-y := -fno-function-sections -fno-data-sections

ccflags-y := -I$(srctree)/init/

obj-$(CONFIG_ROOTFS_CHECK) += rootfs-check.o
rootfs-check-y = rsa.o rootfs_check.o