# SPDX-License-Identifier: GPL-2.0 */
#
# Copyright (c) 2019 MediaTek Inc.
#

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/helio-dvfsrc/

ifneq ($(CONFIG_MTK_VPU_SUPPORT),)
obj-m += vpusys.o
vpusys-objs += vpu_drv.o
vpusys-objs += vpu_algo.o
vpusys-objs += vpu_dbg.o
vpusys-objs += vpu_hw.o
vpusys-objs += vpu_reg.o
vpusys-objs += vpu_profile.o
vpusys-objs += vpubuf-dma.o
vpusys-objs += vpubuf-core.o
vpusys-objs += vpubuf-dma-contig.o
vpusys-objs += vpubuf-memops.o
endif