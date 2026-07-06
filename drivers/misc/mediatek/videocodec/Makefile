#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

ccflags-y += \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/videocodec/common   \
    -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/videocodec/common/drv/inc   \
    -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/videocodec/include   \
    -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/videocodec/include/kernel   \
    -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk-interconnect   \
    -I$(DEVICE_MODULES_PATH)/drivers/dma-buf/heaps	\
	-I$(DEVICE_MODULES_PATH)/drivers/dma-buf		\
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/iommu

obj-$(CONFIG_MTK_VIDEOCODEC_DRIVER) += vcodec_kernel_common_driver.o
vcodec_kernel_common_driver-$(CONFIG_MTK_VIDEOCODEC_DRIVER) +=	\
								common/videocodec_kernel.o   \
								common/val/src/val.o   \
								common/drv/src/drv_base.o

ifeq ($(CONFIG_MTK_VIDEOCODEC_IPM_V1),y)
	ccflags-y += \
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/videocodec/v1
	obj-$(CONFIG_MTK_VIDEOCODEC_DRIVER) += vcodec_kernel_driver-v1.o
	vcodec_kernel_driver-v1-$(CONFIG_MTK_VIDEOCODEC_DRIVER) +=  v1/mtk_vcodec_pm_plat.o   \
									common/dvfs_v2.o	\
									common/mtk_vcodec_mem.o   \
									v1/videocodec_kernel_drv_plat.o \
									v1/mtk_vcodec_intr.o   \
									v1/mtk_vcodec_pm.o   \
									v1/videocodec_kernel_drv.o
endif

ifeq ($(CONFIG_MTK_VIDEOCODEC_IPM_V2),y)
	subdir-ccflags-y := -Werror $(call cc-disable-warning, unused-but-set-variable)
	ccflags-y += \
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat   \
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi \
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmdvfs \
		-I$(DEVICE_MODULES_PATH)/drivers/soc/mediatek \
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/helio-dvfsrc
	obj-$(CONFIG_MTK_VIDEOCODEC_DRIVER) += vcodec_kernel_driver-v1.o
		vcodec_kernel_driver-v1-$(CONFIG_MTK_VIDEOCODEC_DRIVER) +=  v2/videocodec_kernel_drv.o   \
										common/dvfs_v2.o			\
										common/mtk_vcodec_mem.o   \
										v2/mtk_vcodec_pm.o
endif

